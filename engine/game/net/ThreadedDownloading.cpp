#include "platform\gameInterface.h"
#include "ThreadedDownloading.h"
#include "gui/core/guiControl.h"
#include "core/fileStream.h"
#include "core/resManager.h"
#include "console/String.h"
#include "math/mMath.h"
#include "dgl/dgl.h"
#include <Windows.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdarg.h>
using namespace ThreadedDownloading;

enum ThreadVar {
	THREAD_FALSE,
	THREAD_REQUEST_FALSE,
	THREAD_REQUEST_TRUE,
	THREAD_TRUE
};

enum TransferEncoding {
	NORMAL,
	CHUNKED
};

#define READ_HARD_LIMIT 1000000000
//#define THREADED_DOWNLOADING_DEBUG

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static char __THREADED_EVAL_BUFFER__[1024];
static inline void PERFORM_THREADED_EVAL(const char* Format, ...)
{
	// Culminate the args into one string
	va_list vArgs;
	va_start(vArgs, Format);
	dVsprintf(__THREADED_EVAL_BUFFER__, sizeof(__THREADED_EVAL_BUFFER__), Format, vArgs);
	va_end(vArgs);

	const char* argv[]   = { "eval", __THREADED_EVAL_BUFFER__ };
	SimConsoleEvent* evt = new SimConsoleEvent(2, argv, false);

	// Post the event
	Sim::postEvent(Sim::getRootGroup(), evt, Sim::getCurrentTime() + 32);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

QueuedDownload** QueuedDownload::queue       = NULL;
QueuedDownload* QueuedDownload::first        = NULL;
DownloadID QueuedDownload::highestDownloadID = 0;
U32 QueuedDownload::queueLength              = 0;
U32 QueuedDownload::queueSize                = 0;

// Downloading variables
static bool dQueue_Paused  = false;
HANDLE dQueue_Thread       = NULL;
Mutex dQueue_Mutex;

// Thread
ThreadVar dQueue_ThreadRunning = THREAD_TRUE;

// Prototypes
void Queue_DownloadNext();
void Queue_CloseDownloadThread();
bool Queue_CancelDownload();
bool Queue_PauseDownload();
bool Queue_ResumeDownload();
void Queue_Remove(QueuedDownload* download);
void Queue_PopFront();
bool Queue_PushBack(QueuedDownload* download);

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public stuff

DownloadID ThreadedDownloading::Download(const char* url, const char* savePath)
{
	QueuedDownload* NewQEntry = new QueuedDownload();

	// Set its save path
	if (!NewQEntry->SetSavePath(savePath))
	{
		delete NewQEntry;
		return INVALID_DOWNLOAD_ID;
	}

	// Set its URL
	if (!NewQEntry->SetURL(url))
	{
		delete NewQEntry;
		return INVALID_DOWNLOAD_ID;
	}

	// Attempt to find this new entry in the queue
	for (U32 i = 0; i < QueuedDownload::queueLength; i++)
	{
		QueuedDownload* entry = QueuedDownload::queue[i];

		// Mismatch check
		if (NewQEntry->GetHashCode() != entry->GetHashCode())
			continue;

		// Found it!
		delete NewQEntry;
		return entry->GetDownloadID();
	}

	// Queue it up
	if (!NewQEntry->Queue())
	{
		delete NewQEntry;
		return INVALID_DOWNLOAD_ID;
	}

	return NewQEntry->GetDownloadID();
}

bool ThreadedDownloading::Cancel(DownloadID download)
{
	for (U32 i = 0; i < QueuedDownload::queueLength; i++)
	{
		QueuedDownload* entry = QueuedDownload::queue[i];

		// Find it by download ID
		if (entry->GetDownloadID() != download)
			continue;

		// Cancel this entry
		entry->Cancel();

		// Delete it
		delete entry;

		// Done!
		return true;
	}

	// Couldn't find it.
	Con::errorf("ERROR: ThreadedDownloading::Cancel() - Could not find any downloads with the ID %d.", download);
	return false;
}

QueuedDownload* ThreadedDownloading::GetQueueEntry(DownloadID dId)
{
	for (U32 i = 0; i < QueuedDownload::queueLength; i++)
	{
		QueuedDownload* entry = QueuedDownload::queue[i];

		// Find it by download ID
		if (entry->GetDownloadID() != dId)
			continue;

		return entry;
	}

	return NULL;
}

void ThreadedDownloading::Shutdown()
{
	if (QueuedDownload::queueSize == 0)
		return;

	// Cancel the current download
	Queue_CancelDownload();

	// Clear entries
	while (QueuedDownload::queueLength > 0)
		Queue_PopFront();
}

void FORCE_FRAME()
{
	extern GameInterface* Game;
	Game->journalProcess();
	Net::process();
	Platform::process();
	TimeManager::process();
	Game->processEvents();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Downloader Thread Variables

static TransferEncoding DThread_TEnc = TransferEncoding::NORMAL;
static SSL_CTX* DThread_CTX          = NULL;
static SSL* DThread_SSL              = NULL;
static S32 DThread_RedirectCounter   = 0;
static S32 DThread_Tag               = 0;
static S32 DThread_ResponseCode      = 0;
static S32 DThread_ChunkedLeftOver   = 0;
static S32 DThread_ContentLengthMax  = 0;
static S32 DThread_ContentLength     = 0;
static S32 DThread_TransferRate      = 0;
static S32 DThread_BytesPerRead      = 12288;
static U32 DThread_BufferSize        = 0;
static U8* DThread_Buffer            = NULL;
static S8* DThread_RedirectUrl       = NULL;
static bool DThread_UseSSL           = false;
static bool DThread_Connected        = false;
static bool DThread_GotHeaders       = false;
static bool DThread_GotWholePage     = false;

#ifdef TORQUE_DEBUG
static bool DThread_DrawDebug = true;
#else
static bool DThread_DrawDebug = false;
#endif

//---------------------------------------------------------------

static struct
{
	GuiControlProfile* pDefaultProfile;
	TextureHandle pGraphBG;
	S32 iGraphPoints[10];
	S32 iCurrentGraphPoint;
	bool bInit = false;
} DThread_DebugDrawInfo;

//---------------------------------------------------------------

static FileStream DThread_FileStream;

//---------------------------------------------------------------

#ifdef THREADED_DOWNLOADING_DEBUG
#define DThread_Log(...) Con::printf(__VA_ARGS__);
#else
#define DThread_Log(...)
#endif

//---------------------------------------------------------------

bool DTHREAD_processLine(U8* line);
void DTHREAD_send(const char* buffer);

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Downloader Callbacks

void DTHREAD_onDisconnected()
{
	if (DThread_RedirectUrl != NULL)
	{
		// Limit the number of redirections
		if (DThread_RedirectCounter >= 4)
		{
			// We've hit the limit for this queue'd entry. Stop this madness.
			// Free the redirect pointer
			dFree(DThread_RedirectUrl);
			DThread_RedirectUrl = NULL;

			// Recall this function
			DTHREAD_onDisconnected();

			return;
		}

		DThread_Log("Redirecting...");

		// We need to reset this queue object's URL to the new redirect url, but cannot do so whilst it is first in queue. Let's do a dirty hack.
		// Redirect to this location
		dQueue_Paused = true;
		bool bResult  = QueuedDownload::queue[0]->SetURL((const char*)DThread_RedirectUrl);
		dQueue_Paused = false;

		// Free the redirect pointer
		dFree(DThread_RedirectUrl);
		DThread_RedirectUrl = NULL;

		// Make sure it got set!
		if (!bResult)
		{
			// It didn't get set. Next queue object.
			DThread_Log("Failed to redirect!");
			Queue_PopFront();
		}
		
		// Restart the download
		Queue_DownloadNext();

		return;
	}

	// Complete the download (if we can)
	if (DThread_FileStream.getStatus() != Stream::Status::Closed)
	{
		// Close the stream
		DThread_FileStream.close();

		// Sleep a bit
		Sleep(32);

		if (QueuedDownload::queueSize > 0)
		{
			// Call the appropriate callback
			if (DThread_GotWholePage)
				QueuedDownload::queue[0]->onDownloadComplete.Invoke(1, QueuedDownload::queue[0]->GetSavePath());
			else
				QueuedDownload::queue[0]->onDownloadFailed.Invoke(1, "ERR_FILE_INCOMPLETE");
		}
	}

	DThread_Log("on Disconnect");

	// Reset counters
	DThread_RedirectCounter = 0;

	// We're done with this entry. Remove it from the queue
	Queue_PopFront();

	// On to the next entry
	Queue_DownloadNext();
}

void DTHREAD_onConnectFailed()
{
	DThread_Log("on Connect Failure");

	// We're done with this entry. Remove it from the queue
	Queue_PopFront();

	// On to the next entry
	Queue_DownloadNext();
}

void DTHREAD_onConnect()
{
	// Build the request string
	char RequestBuffer[1024];
	dSprintf(RequestBuffer, 1024, "GET %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: B4v21Launcher\r\nConnection: close\r\n\r\n", (*QueuedDownload::queue[0]->GetURL()->getPath() == '/' ? "" : "/"), QueuedDownload::queue[0]->GetURL()->getPath(), QueuedDownload::queue[0]->GetURL()->getHostName());

	DThread_Log("on Connect -- Sent \"%s\"", RequestBuffer);

	// Send the request
	DTHREAD_send(RequestBuffer);
}

void DTHREAD_onLine(const char* data)
{
	if (DThread_FileStream.getStatus() == Stream::Status::Closed || QueuedDownload::queueSize == 0)
		return;

	// Write the data to the file
	DThread_FileStream.write(dStrlen(data), (const void*)data);

	// Report progress
	QueuedDownload::queue[0]->onDownloadProgress.Invoke(3, (int)DThread_ContentLength, (int)DThread_ContentLengthMax, (int)DThread_TransferRate);

	DThread_Log("on Line: \"%s\"", data, DThread_TransferRate);
}

void DTHREAD_onRawData(const char* data, U32 size)
{
	if (DThread_FileStream.getStatus() == Stream::Status::Closed || QueuedDownload::queueSize == 0)
		return;

	// Write the data to the file
	DThread_FileStream.write(size, (const void*)data);

	// Report progress
	QueuedDownload::queue[0]->onDownloadProgress.Invoke(3, (int)DThread_ContentLength, (int)DThread_ContentLengthMax, (int)DThread_TransferRate);

	DThread_Log("on RawData: %d byte(s)", size);
}

void DTHREAD_onRedirect(const char* newURL)
{
	// Do not write to the redirect url pointer multiple times
	if (DThread_RedirectUrl != NULL)
		return;

	// Set the redirect link
	DThread_RedirectUrl = (S8*)dStrdup(newURL);
	DThread_RedirectCounter++;

	DThread_Log("on Redirect: %s", newURL);
}

void DTHREAD_onHeader(const char* name, const char* value)
{
	DThread_Log("on Header: \"%s\" => \"%s\"", name, value);
}

void DTHREAD_GotHeaders()
{
	if (DThread_RedirectUrl != NULL || QueuedDownload::queueSize == 0)
		return;

	DThread_Log("Opening the output file...");

	// Get the file stream ready!
	if (!DThread_FileStream.open(QueuedDownload::queue[0]->GetSavePath(), FileStream::AccessMode::Write))
	{
		// Failed to open it... Uh oh!
		Con::errorf("Failed to open \"%s\" for writing!", QueuedDownload::queue[0]->GetSavePath());
		QueuedDownload::queue[0]->onDownloadFailed.Invoke(1, "OUT_FILE_FAILED_TO_OPEN");
		Queue_PopFront();
	}
	else
	{
		// The file opened; Start the download!
		QueuedDownload::queue[0]->onDownloadStart.Invoke(2, QueuedDownload::queue[0]->GetDownloadID(), DThread_ContentLengthMax);
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Downloader Thread

void DTHREAD_send(const char* buffer)
{
	S32 len = (DThread_UseSSL ? SSL_write(DThread_SSL, buffer, dStrlen(buffer)) : ::send(DThread_Tag, buffer, dStrlen(buffer), 0));

	if (len < 0)
	{
		S32 err = SSL_get_error(DThread_SSL, len);
		switch (err)
		{
			case SSL_ERROR_WANT_WRITE:
				return;
			case SSL_ERROR_WANT_READ:
				return;
			case SSL_ERROR_ZERO_RETURN:
			case SSL_ERROR_SYSCALL:
			case SSL_ERROR_SSL:
			default:
				return;
		}
	}
}

void DTHREAD_onDisconnect()
{
	if (DThread_BufferSize)
	{
		DThread_Buffer[DThread_BufferSize] = 0;
		DTHREAD_processLine(DThread_Buffer);
		dFree(DThread_Buffer);

		DThread_Buffer     = NULL;
		DThread_BufferSize = 0;
	}
	else DThread_Buffer = 0;

	DThread_Connected = false;
	if (DThread_SSL)
	{
		SSL_free(DThread_SSL);
		DThread_SSL = NULL;
	}

	DTHREAD_onDisconnected();
}

void DTHREAD_disconnect()
{
	if (!DThread_Connected)
		return;

	if (!DThread_SSL)
	{
		if (DThread_Tag != InvalidSocket)
			closesocket(DThread_Tag);
	}
	else
	{
		closesocket(DThread_Tag);
		DThread_SSL = NULL;
	}

	DThread_Connected = false;
	DTHREAD_onDisconnect();
}

void DTHREAD_connect()
{
	UrlStructInfo* URL = QueuedDownload::queue[0]->GetURL();
	char address[1024];

	// Build the address
	sprintf(address, "%s:%d", URL->getHostName(), URL->getPortNum());

	Con::printf("Connecting to host \"%s\" on port # %d to get path \"%s\"", URL->getHostName(), URL->getPortNum(), URL->getPath());

	DThread_GotWholePage     = false;
	DThread_GotHeaders       = false;
	DThread_ContentLengthMax = 0;
	DThread_ContentLength    = 0;
	DThread_ChunkedLeftOver  = 0;
	DThread_TEnc             = TransferEncoding::NORMAL;

	DThread_UseSSL = true;
	int s          = socket(AF_INET, SOCK_STREAM, 0);
	if (!s)
	{
		Con::printf("DTHREAD_connect() - Error creating socket.");
		return;
	}

	// Resolve the URL's IP
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = inet_addr(URL->getHostName());
	sa.sin_port        = htons(URL->getPortNum());
	S32 socklen        = sizeof(sa);

	// Connect to it
	if (sa.sin_addr.s_addr != INADDR_NONE)
	{
		if (::connect(s, (struct sockaddr *)&sa, socklen))
		{
			Con::printf("TCPQuery::connect() - Error connecting to server.");
			return;
		}
	}
	else
	{
		struct hostent* phost = gethostbyname(URL->getHostName());

		if ((phost) && (phost->h_addrtype == AF_INET))
		{
			sa.sin_addr = *(in_addr*)(phost->h_addr);
			
			if (::connect(s, (struct sockaddr *)&sa, socklen))
			{
				Con::printf("TCPQuery::connect() - Error connecting to server.");
				return;
			}
		}
		else
		{
			Con::errorf("TCPQuery::connect() - Error.");
			return;
		}
	}

	if (URL->getPortNum() == 80)
	{
		// We're not doing a SSL connection, so just stop here.
		DThread_Tag       = s;
		DThread_Connected = true;
		DThread_UseSSL    = false;

		DTHREAD_onConnect();
		return;
	}

	// Get the SSL context ready
	SSL_CTX* cl;
	if (!s)
	{
		Con::printf("TCPQuery::connect() - Error creating socket.");
		DTHREAD_onConnectFailed();
		return;
	}

	if (!DThread_CTX)
		DThread_CTX = SSL_CTX_new(SSLv23_client_method());

	DThread_SSL = SSL_new(DThread_CTX);
	SSL_CTX_set_options(DThread_CTX, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);

	if (!DThread_SSL)
	{
		Con::errorf("TCPQuery::connect() - Error creating SSL.");
		DTHREAD_onConnectFailed();
		return;
	}

	DThread_Tag = SSL_get_fd(DThread_SSL);
	SSL_set_fd(DThread_SSL, s);
	int err     = SSL_connect(DThread_SSL);

	if (err <= 0)
	{
		Con::errorf("TCPQuery::connect() - Error creating SSL connection.  err=%x", err);
		DThread_SSL = NULL;
		SSL_free(DThread_SSL);

		// Try normally adding socket
		DThread_UseSSL = false;
		DThread_Tag    = Net::openPrivateConnectionTo(address);
		if (DThread_Tag != InvalidSocket)
			DThread_Connected = true;

		return;
	}

	DThread_Connected = true;
	DTHREAD_onConnect();
}

bool DTHREAD_processLine(U8* line)
{
	if (DThread_GotHeaders && DThread_TEnc == TransferEncoding::CHUNKED && *line != 0)
	{
		DThread_ChunkedLeftOver = dStrtol((const char*)line, NULL, 16);

		if (DThread_ChunkedLeftOver == 0)
		{
			DThread_GotWholePage = true;
			DTHREAD_disconnect();
		}

		return true;
	}
	else if (!DThread_GotHeaders && *line == 0)
	{
		DThread_GotHeaders = true;

		DTHREAD_GotHeaders();
		return true;
	}
	else if (!DThread_GotHeaders)
	{
		char* headerEnd = dStrstr((const char*)line, ": ");
		if (headerEnd == NULL)
		{
			if (dStrstr((const char*)line, "HTTP/1.1 ") == (char*)line)
			{
				// Status code response
				DThread_ResponseCode = dAtoi((const char*)line + dStrlen("HTTP/1.1 "));
				return true;
			}

			return false;
		}

		char* headerName  = new char[(headerEnd - (char*)line) + 1];
		char* headerValue = new char[(((char*)line + dStrlen((char*)line)) - headerEnd)]; // Not adding +1 because we aren't trying to get the space in the header value

		// Get header name
		*headerEnd = 0;
		dStrcpy(headerName, (char*)line);
		*headerEnd = ':';

		// Get header value
		dStrcpy(headerValue, headerEnd + 2);

		// Do some processing of our own
		if (!dStricmp(headerName, "Content-Length"))
			DThread_ContentLengthMax = dAtoi(headerValue);
		else if (!dStricmp(headerName, "Location"))
			DTHREAD_onRedirect(headerValue);
		else if (!dStricmp(headerName, "Transfer-Encoding") && !dStricmp(headerValue, "chunked"))
			DThread_TEnc = TransferEncoding::CHUNKED;

		// Send to onHeader callback
		DTHREAD_onHeader(headerName, headerValue);

		delete[] headerName;
		delete[] headerValue;

		return true;
	}

	DTHREAD_onLine((const char*)line);
	return true;
}

void DTHREAD_parseLine(U8* buffer, U32* start, U32 bufferLen)
{
	if (!DThread_Connected)
	{
		// ignore
		*start += bufferLen;
		return;
	}

	U32 i;
	U8* line = buffer + *start;

	if (DThread_GotHeaders)
	{
		U32 usedBuffer = bufferLen;
		if (DThread_TEnc == TransferEncoding::CHUNKED)
		{
			if (DThread_ChunkedLeftOver != 0)
			{
				usedBuffer               = mClamp(mClamp(usedBuffer, 1, DThread_ChunkedLeftOver), 1, 6144);
				DThread_ChunkedLeftOver -= usedBuffer;
			}
			else goto PARSE_LINE_CONTINUE;
		}

		DThread_ContentLength += usedBuffer;
		DTHREAD_onRawData((const char*)(buffer + *start), usedBuffer);
		*start += usedBuffer;
		return;
	}

PARSE_LINE_CONTINUE:
	for (i = *start; i < bufferLen; i++)
		if (buffer[i] == '\n' || buffer[i] == 0)
			break;

	U32 len = i - *start;
	if (i == bufferLen || DThread_Buffer)
	{
		// we've hit the end with no newline
		DThread_Buffer      = (U8*)dRealloc(DThread_Buffer, DThread_BufferSize + len + 2);
		dMemcpy(DThread_Buffer + DThread_BufferSize, line, len);
		DThread_BufferSize += len;
		*start              = i;

		// process the line
		if (i != bufferLen)
		{
			DThread_Buffer[DThread_BufferSize] = 0;
			if (DThread_BufferSize && DThread_Buffer[DThread_BufferSize - 1] == '\r')
				DThread_Buffer[DThread_BufferSize - 1] = 0;

			U8* temp           = DThread_Buffer;
			DThread_Buffer     = NULL;
			DThread_BufferSize = 0;

			DTHREAD_processLine(temp);
			dFree(temp);
		}
	}
	else if (i != bufferLen)
	{
		line[len] = 0;
		if (len && line[len - 1] == '\r')
			line[len - 1] = 0;

		DTHREAD_processLine(line);
	}

	if (i != bufferLen)
		* start = i + 1;
}

U32 DTHREAD_onReceive(U8* buffer, U32 bufferLen)
{
	U32 start = 0;
	DTHREAD_parseLine(buffer, &start, bufferLen);
	return start;
}

static int CurrentTransferRate = 0;
bool ThreadRunning             = false;
DWORD WINAPI TRANSFER_RATE_CALCULATION(LPVOID uData)
{
	S32 transfer_rate_cache[3];
	U32 transfer_rate_cache_no = 0;

	while (ThreadRunning)
	{
		// Add this to the transfer rate cache
		transfer_rate_cache[transfer_rate_cache_no] = CurrentTransferRate;
		if (++transfer_rate_cache_no >= (sizeof(transfer_rate_cache) / sizeof(S32)))
			transfer_rate_cache_no = 0;

		// Calculate the transfer rate
		DThread_TransferRate = 0;
		for (int i = 0; i < (sizeof(transfer_rate_cache) / sizeof(S32)); i++)
			DThread_TransferRate += transfer_rate_cache[i];
		DThread_TransferRate /= (sizeof(transfer_rate_cache) / sizeof(S32));

		// Add this to the debug graph
		if (DThread_DrawDebug)
		{
			if (DThread_DebugDrawInfo.iCurrentGraphPoint >= (sizeof(DThread_DebugDrawInfo.iGraphPoints) / sizeof(S32)))
			{
				DThread_DebugDrawInfo.iCurrentGraphPoint = (sizeof(DThread_DebugDrawInfo.iGraphPoints) / sizeof(S32)) - 1;
				for (int i = 1; i < sizeof(DThread_DebugDrawInfo.iGraphPoints) / sizeof(S32); i++)
					DThread_DebugDrawInfo.iGraphPoints[i - 1] = DThread_DebugDrawInfo.iGraphPoints[i];
			}

			DThread_DebugDrawInfo.iGraphPoints[DThread_DebugDrawInfo.iCurrentGraphPoint++] = DThread_TransferRate;
		}

		// Reset this
		CurrentTransferRate = 0;

		Sleep(1000);
	}

	// Reset the transfer rate when we're not doing anything
	DThread_TransferRate = 0;
	return 0;
}

DWORD WINAPI DOWNLOAD_THREAD(LPVOID uData)
{
	HANDLE TRC   = NULL;
	char* buf    = NULL;
	int   buflen = 0;
	int   len    = 100;
	S32   loops  = 0;

	if (ThreadRunning)
		return 0;

	// Reset
	TRC                      = CreateThread(NULL, 0, TRANSFER_RATE_CALCULATION, NULL, 0, NULL);
	DThread_TEnc             = TransferEncoding::NORMAL;
	DThread_SSL              = NULL;
	DThread_ResponseCode     = 0;
	DThread_ChunkedLeftOver  = 0;
	DThread_ContentLengthMax = 0;
	DThread_ContentLength    = 0;
	DThread_BufferSize       = 0;
	DThread_Buffer           = NULL;
	DThread_Connected        = false;
	DThread_GotHeaders       = false;

	// Process
	ThreadRunning = true;
	while (dQueue_ThreadRunning == THREAD_TRUE)
	{
		if (dQueue_ThreadRunning == THREAD_REQUEST_FALSE)
		{
			dQueue_ThreadRunning = THREAD_FALSE;
			break;
		}

		if (QueuedDownload::queueLength == 0)
			break;

		// If the download is paused, then wait here.
		if (dQueue_Paused)
		{
			QueuedDownload::queue[0]->onDownloadProgress.Invoke(3, (int)DThread_ContentLength, (int)DThread_ContentLengthMax, (int)DThread_TransferRate);

			Sleep(1000);
			continue;
		}

		// Get the current queue obj
		QueuedDownload* CurrentDownload = QueuedDownload::queue[0];

		if (!DThread_Connected)
		{
			// Connect!
			DTHREAD_connect();
			continue;
		}

		// Clamp the bytes per read value to something sensible
		if (DThread_BytesPerRead <= 0)
			DThread_BytesPerRead = 1;
		else if (DThread_BytesPerRead >= READ_HARD_LIMIT) // 2 mb/s limit
			DThread_BytesPerRead = READ_HARD_LIMIT - 1;

		// Resize the buffer if necessary
		if (buflen <= DThread_BytesPerRead)
		{
			if (buf != NULL)
				free(buf);

			buf    = (char*)malloc(DThread_BytesPerRead + 1);
			buflen = DThread_BytesPerRead + 1;
		}

		// Read the socket
		*buf = 0;
		if (DThread_UseSSL)
			len = SSL_read(DThread_SSL, buf, DThread_BytesPerRead);
		else
			len = recv(DThread_Tag, buf, DThread_BytesPerRead, 0);

		if (len == SOCKET_ERROR)
		{
			Con::errorf("Socket error!");
			DTHREAD_disconnect();

			continue;
		}

		CurrentTransferRate = len;

		buf[len]   = 0;
		S32 size   = len;
		U8* buffer = (U8*)buf;
		while (size)
		{
			U32 ret = DTHREAD_onReceive((U8*)buffer, size);
			size   -= ret;
			buffer += ret;
		}

		if (len <= 0 && DThread_TEnc == TransferEncoding::NORMAL)
		{
			DThread_GotWholePage = true;
			DTHREAD_disconnect();
		}

		if (DThread_UseSSL)
		{
			if (!DThread_SSL)
			{
				DThread_GotWholePage = true;
				continue;
			}

			S16 err = 0;
			SSL_get_error(DThread_SSL, err);
			if (err)
				Con::errorf("SSL ERROR: # %04d", err);
		}

		Sleep(4);
	}

	// Reset everything
	if (buf != NULL)
		free(buf);

	DTHREAD_disconnect();

	dQueue_ThreadRunning = ThreadVar::THREAD_FALSE;
	dQueue_Thread        = NULL;
	ThreadRunning        = false;

	WaitForSingleObject(TRC, 3000);

	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Private stuff

void Queue_DownloadNext()
{
	// Close the current file
	DThread_FileStream.close();

	// If the thread already exists, then set it to download
	if (dQueue_Thread != NULL)
	{
		dQueue_ThreadRunning = THREAD_TRUE;
		dQueue_Thread        = CreateThread(NULL, 0, DOWNLOAD_THREAD, NULL, 0, NULL);

		return;
	}

	// Stop the current queue
	dQueue_ThreadRunning = THREAD_REQUEST_FALSE;
	WaitForSingleObject(dQueue_Thread, 9000);
	dQueue_Thread = NULL;

	// Create a new thread
	dQueue_ThreadRunning = THREAD_TRUE;
	dQueue_Thread        = CreateThread(NULL, 0, DOWNLOAD_THREAD, NULL, 0, NULL);
}

void Queue_CloseDownloadThread()
{
	if (dQueue_Thread == NULL)
		return;

	dQueue_ThreadRunning = THREAD_REQUEST_FALSE;
	WaitForSingleObject(dQueue_Thread, 9000);
	dQueue_Thread = NULL;
}

bool Queue_CancelDownload()
{
	Queue_CloseDownloadThread();
	return false;
}

bool Queue_PauseDownload()
{
	if (dQueue_Paused)
		return true;

	dQueue_Paused = true;
	return true;
}

bool Queue_ResumeDownload()
{
	if (!dQueue_Paused)
		return true;

	dQueue_Paused = false;
	return true;
}

/// This does NOT delete the entries it removes.
void Queue_Remove(QueuedDownload* download)
{
	// Sanity check
	if (QueuedDownload::queueLength == 0)
		return;

	// If there's no use for a queue, then stop here.
	if (QueuedDownload::queueLength == 1)
	{
		// Just a quick validity check...
		if (download != QueuedDownload::queue[0])
			return;

		// Stop the download & wait for it to finish cleaning up
		Queue_CloseDownloadThread();

		// Free the queue
		dFree(QueuedDownload::queue);

		// Reset everything
		QueuedDownload::queueLength = 0;
		QueuedDownload::queueSize   = 0;
		QueuedDownload::queue       = NULL;

		return;
	}

	// Find it & remove it
	for (U32 i = 0; i < QueuedDownload::queueLength; i++)
	{
		if (QueuedDownload::queue[i] != download)
			continue;

		// If it's the first entry, then wait for the download thread to stop
		if (i == 0)
			Queue_CloseDownloadThread();

		// Offset the queue by one
		dMemmove(QueuedDownload::queue + i, QueuedDownload::queue + i + 1, sizeof(QueuedDownload*) * ((QueuedDownload::queueLength - i) - 1));

		// Decrement the length
		QueuedDownload::queueLength--;

		// Set the rest to 'NULL'
		dMemset(QueuedDownload::queue + QueuedDownload::queueLength, NULL, sizeof(QueuedDownload*) * (QueuedDownload::queueLength - i));

		// Enter the next object in queue if we were being downloaded
		if (i == 0)
			Queue_DownloadNext();

		// Done!
		return;
	}

	AssertWarn(false, avar("WARNING: Queue_Remove() - Unable to find download # %d in queue!", download->GetDownloadID()));
}

void Queue_PopFront()
{
	// Sanity check
	if (QueuedDownload::queueLength == 0)
		return;

	// Stop downloading!
	if (Con::isMainThread())
		Queue_CloseDownloadThread();

	// If there's no use for a queue, then stop here.
	if (QueuedDownload::queueLength == 1)
	{
		dFree(QueuedDownload::queue);

		QueuedDownload::queueLength = 0;
		QueuedDownload::queueSize   = 0;
		QueuedDownload::queue       = NULL;
		return;
	}

	// Offset the queue by one
	dMemmove(QueuedDownload::queue, QueuedDownload::queue + 1, sizeof(QueuedDownload*) * (QueuedDownload::queueLength - 1));

	// Decrement the length of the queue
	QueuedDownload::queueLength--;
}

bool Queue_PushBack(QueuedDownload* download)
{
	if (QueuedDownload::queueLength == 0)
	{
		// Allocate a new list
		QueuedDownload::queue = (QueuedDownload**)dMalloc(sizeof(QueuedDownload*));

		AssertFatal(QueuedDownload::queue != NULL, "FATAL ERROR: Queue_PushBack() - Failed to allocate a new queue!");

		// Set the first entry
		QueuedDownload::queue[0]    = download;
		QueuedDownload::queueLength = 1;
		QueuedDownload::queueSize   = 1;

		// Download next automatically
		Queue_DownloadNext();

		// Done!
		return true;
	}

	// If there is an available spot open, then take it.
	if (QueuedDownload::queueLength < QueuedDownload::queueSize)
	{
		QueuedDownload::queue[QueuedDownload::queueLength++] = download;

		// Done!
		return true;
	}

	// Re-size the list to fit more in it
	QueuedDownload::queue = (QueuedDownload**)dRealloc((void*)QueuedDownload::queue, sizeof(QueuedDownload*) * ++QueuedDownload::queueSize);

	AssertFatal(QueuedDownload::queue != NULL, "FATAL ERROR: Queue_PushBack() - Failed to resize the queue!");

	// Add it in
	QueuedDownload::queue[QueuedDownload::queueLength++] = download;

	// Done!
	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// QueuedDownload stuff

QueuedDownload::QueuedDownload()
{
	mCallbackUserData = NULL;
	mId               = INVALID_DOWNLOAD_ID;
	mInQueue          = false;
	mValid            = false;
	mSavePath         = NULL;
	mHashCode         = 0;

	previous = NULL;
	next     = first;
	first    = this;
}

QueuedDownload::~QueuedDownload()
{
	if (mInQueue)
		Queue_Remove(this);

	if (next)
		next->previous = previous;
	
	if (previous)
		previous->next = next;

	if (first == this)
		first = next;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void QueuedDownload::GenerateHash()
{
	String OurHashableString = "";

	// Reset the hash code
	mHashCode = 0;

	// Include the URL string
	if (mURL.GetURL() != NULL)
		OurHashableString += mURL.GetURL();

	// Include the savepath string
	if (mSavePath != NULL)
		OurHashableString += mSavePath;

	// Generate the hash
	mHashCode = OurHashableString.getHash();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool QueuedDownload::Queue()
{
	if (mInQueue || !mValid)
		return false;

	// Add ourselves to the queue
	if (!Queue_PushBack(this))
		return false;

	// Assign ourselves an ID if necessary
	if (mId == INVALID_DOWNLOAD_ID)
		mId = QueuedDownload::highestDownloadID++;

	// Set state variables
	mInQueue = true;
	return true;
}

bool QueuedDownload::Cancel()
{
	if (!mInQueue)
		return false;

	// Remove ourselves from the queue
	Queue_Remove(this);
	mInQueue = false;

	// Done!
	return true;
}

bool QueuedDownload::Pause()
{
	if (!mInQueue || QueuedDownload::queue[0] != this || dQueue_Paused)
		return false;

	Queue_PauseDownload();
	return true;
}

bool QueuedDownload::Resume()
{
	if (!mInQueue || QueuedDownload::queue[0] != this || !dQueue_Paused)
		return false;

	Queue_ResumeDownload();
	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const char* QueuedDownload::GetSavePath()
{
	return mSavePath;
}

UrlStructInfo* QueuedDownload::GetURL()
{
	return &mURL;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool QueuedDownload::SetSavePath(const char* path)
{
	if (mInQueue && QueuedDownload::queue[0] == this)
	{
		Con::errorf("ERROR: QueuedDownload::SetSavePath() - Cannot set the save path of a queue entry that is currently being downloaded!");
		return false;
	}

	// Delete the old save path
	if (mSavePath != NULL)
		delete[] mSavePath;

	// Expand the save path
	char filePath[1024];
	{
		const char* tPath = dStrrchr(path, '/');
		if (tPath == NULL)
			tPath = dStrrchr(path, '\\');

		if (!tPath)
			* filePath = 0;
		else
		{
			U32 len = tPath - path;
			dStrncpy(filePath, path, len);
			filePath[len] = 0;
		}
	}

	if (!Platform::isDirectory(filePath))
	{
		Con::errorf("ERROR: QueuedDownload::SetSavePath() - Directory \"%s\" does not exist!", filePath);
		return false;
	}

	// Allocate a new string
	mSavePath = new char[dStrlen(path) + 1];
	dStrcpy(mSavePath, path);

	// Set valid
	if (mURL.getHostName() != NULL)
		mValid = true;

	// Re-generate the hash
	GenerateHash();

	// Done!
	return true;
}

bool QueuedDownload::SetURL(const char* URL)
{
	if (!dQueue_Paused && mInQueue && QueuedDownload::queue[0] == this)
	{
		Con::errorf("ERROR: QueuedDownload::SetURL() - Cannot set the URL of a queue entry that is currently being downloaded!");
		return false;
	}

	// Set the URL
	if (!mURL.setURL(URL))
	{
		Con::errorf("ERROR: QueuedDownload::SetURL() - Url \"%s\" is invalid!", URL);
		return false;
	}

	// Set valid!
	if (mSavePath != NULL)
		mValid = true;

	// Re-generate the hash
	GenerateHash();

	// Done!
	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool QueuedDownload::IsDownloading()
{
	if (!mInQueue)
		return false;

	return mInQueue ? false : (QueuedDownload::queue[0] == this ? !dQueue_Paused : false);
}

bool QueuedDownload::IsInQueue()
{
	return mInQueue;
}

bool QueuedDownload::IsPaused()
{
	return !mInQueue ? true : (QueuedDownload::queue[0] == this ? dQueue_Paused : true);
}

bool QueuedDownload::IsValid()
{
	return mValid;
}

U32 QueuedDownload::GetDownloadID()
{
	return mId;
}

U32 QueuedDownload::GetHashCode()
{
	return mHashCode;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Initialization

void ThreadedDownloading::Init()
{
	Con::addVariable("Pref::Launcher::Downloader::BytesPerRead", TypeS32, &DThread_BytesPerRead);
	Con::addVariable("DebugDownloader", TypeBool, &DThread_DrawDebug);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Debugging

#define DRAW_DEBUG_TEXT(_Format, ...) \
	current   = avar(_Format, __VA_ARGS__);\
	current_x = xOffset - pProfile->mFont->getStrWidth(current);\
	dglSetBitmapModulation(ColorF(0, 0, 0, 1));\
	dglDrawText(pProfile->mFont, Point2I(current_x - 1, yOffset), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x + 1, yOffset), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x, yOffset - 1), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x, yOffset + 1), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x - 1, yOffset - 1), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x + 1, yOffset - 1), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x - 1, yOffset + 1), current);\
	dglDrawText(pProfile->mFont, Point2I(current_x + 1, yOffset + 1), current);\
	dglSetBitmapModulation(ColorF(1, 1, 1, 1));\
	dglDrawText(pProfile->mFont, Point2I(current_x, yOffset), current);\
	yOffset += pProfile->mFont->getHeight();

void ThreadedDownloading::DrawDebug()
{
	if (!DThread_DrawDebug)
		return;

	if (!DThread_DebugDrawInfo.bInit)
	{
		DThread_DebugDrawInfo.pGraphBG           = TextureHandle("DTDDIPGBG", GBitmap::load("launcher/ui/DBG_G_BG.png"));
		DThread_DebugDrawInfo.bInit              = true;
		DThread_DebugDrawInfo.pDefaultProfile    = ((GuiControlProfile*)Sim::findObject("GuiDefaultProfile"));
		DThread_DebugDrawInfo.iCurrentGraphPoint = 0;

		dMemset(DThread_DebugDrawInfo.iGraphPoints, -1, sizeof(DThread_DebugDrawInfo.iGraphPoints));
	}

	GuiControlProfile* pProfile = DThread_DebugDrawInfo.pDefaultProfile;
	const char* current         = NULL;
	int current_x               = 0;
	int xOffset                 = Platform::getWindowSize().x - 10;
	int yOffset                 = 10;

	// Draw debug information
	DRAW_DEBUG_TEXT("0x%08p (%d bytes), %d in queue, avg %06d B/s", QueuedDownload::queue, QueuedDownload::queueSize * sizeof(QueuedDownload*), QueuedDownload::queueLength, DThread_TransferRate);
	DRAW_DEBUG_TEXT("DTHREAD @ 0x%08p (%s)%s", dQueue_Thread, (dQueue_ThreadRunning == ThreadVar::THREAD_FALSE ? "THREAD_FALSE" : (dQueue_ThreadRunning == ThreadVar::THREAD_REQUEST_FALSE ? "THREAD_REQUEST_FALSE" : (dQueue_ThreadRunning == ThreadVar::THREAD_REQUEST_TRUE ? "THREAD_REQUEST_TRUE" : (dQueue_ThreadRunning == ThreadVar::THREAD_TRUE ? "THREAD_TRUE" : "UNKNOWN")))),
			(dQueue_Paused ? " [PAUSED]" : ""));

	float MEM         = (float)Memory::getMemoryUsed();
	const char* MTYPE = "B";

	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "KB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "MB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "GB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "TB"; }

	DRAW_DEBUG_TEXT("MEMORY %.2f %s", MEM, MTYPE);

	// Draw all entries
	for (int i = 0; i < QueuedDownload::queueLength; i++)
	{
		QueuedDownload* pEntry = QueuedDownload::queue[i];

		if (yOffset >= (Platform::getWindowSize().y / 2))
		{
			DRAW_DEBUG_TEXT("...%04d hidden entr%s...", QueuedDownload::queueLength - (i + 1), ((QueuedDownload::queueLength - (i + 1)) == 1 ? "y" : "ies"));
			break;
		}

		if (i == 0)
		{
			DRAW_DEBUG_TEXT("\"%s\" => \"%s\" - %.3d%% - [%04d]", pEntry->GetURL()->GetURL(), pEntry->GetSavePath(), (DThread_ContentLengthMax == 0 ? 0 : 100 * DThread_ContentLength / DThread_ContentLengthMax), i);
		}
		else
		{
			DRAW_DEBUG_TEXT("\"%s\" => \"%s\" - .... - [%04d]", pEntry->GetURL()->GetURL(), pEntry->GetSavePath(), i);
		}
	}

	// Display the download graph
	MEM   = DThread_TransferRate;
	MTYPE = "B";

	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "KB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "MB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "GB"; }
	if (MEM >= 1024.f) { MEM /= 1024.f; MTYPE = "TB"; }

	float MEM2         = (F32)DThread_BytesPerRead;
	const char* MTYPE2 = "B";

	if (MEM2 >= 1024.f) { MEM2 /= 1024.f; MTYPE2 = "KB"; }
	if (MEM2 >= 1024.f) { MEM2 /= 1024.f; MTYPE2 = "MB"; }
	if (MEM2 >= 1024.f) { MEM2 /= 1024.f; MTYPE2 = "GB"; }
	if (MEM2 >= 1024.f) { MEM2 /= 1024.f; MTYPE2 = "TB"; }

	dglClearBitmapModulation();

	// Draw a graph
	RectI rGraphBox(0, 0, 200, 100);
	rGraphBox.point = Point2I(Platform::getWindowSize().x - (rGraphBox.extent.x + 10), Platform::getWindowSize().y - (rGraphBox.extent.y + 20 + pProfile->mFont->getHeight()));

	// Draw the graph background
	dglDrawBitmap(DThread_DebugDrawInfo.pGraphBG, rGraphBox.point);

	// Draw transfer rate
	current = avar("%.2f %s/s (Throttled at %.2f %s/s)", MEM, MTYPE, MEM2, MTYPE2);
	dglSetBitmapModulation(ColorF(0.f, 0.f, 0.f, 1.f));
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x - 1, (rGraphBox.point.y + rGraphBox.extent.y + 10) - 1), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x, (rGraphBox.point.y + rGraphBox.extent.y + 10) - 1), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x + 1, (rGraphBox.point.y + rGraphBox.extent.y + 10) - 1), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x - 1, (rGraphBox.point.y + rGraphBox.extent.y + 10)), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x, (rGraphBox.point.y + rGraphBox.extent.y + 10)), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x + 1, (rGraphBox.point.y + rGraphBox.extent.y + 10)), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x - 1, (rGraphBox.point.y + rGraphBox.extent.y + 10) + 1), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x, (rGraphBox.point.y + rGraphBox.extent.y + 10) + 1), current);
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x + 1, (rGraphBox.point.y + rGraphBox.extent.y + 10) + 1), current);
	dglSetBitmapModulation(ColorF(1.f, 1.f, 1.f, 1.f));
	dglDrawText(pProfile->mFont, Point2I(rGraphBox.point.x, rGraphBox.point.y + rGraphBox.extent.y + 10), current);
	dglSetBitmapModulation(ColorF(0.68627f, 0.83921f, 0.65098f, 1.f));

	// Resize the graph box (to account for the edges)
	rGraphBox.point.y  += 1;
	rGraphBox.extent.x -= 1;
	rGraphBox.extent.y -= 1;

	// Add new point to the graph (if we can)
	int iNumPoints = sizeof(DThread_DebugDrawInfo.iGraphPoints) / sizeof(S32);

	// Do the graph displaying bit
	S32* pPointBuffer   = DThread_DebugDrawInfo.iGraphPoints;
	int iNumValidPoints = 0;
	int iHighestPoint   = 0;
	for (int i = 0; i < iNumPoints; i++)
	{
		if (pPointBuffer[i] < 0)
			break;

		// Set valid point count
		iNumValidPoints++;

		if (pPointBuffer[i] > iHighestPoint)
			iHighestPoint = pPointBuffer[i];
	}

	// Draw transfer rate
#define DRAW_LINE(start, end, color) \
	rightVector = (Point2F((F32)end.x, (F32)end.y) - Point2F((F32)start.x, (F32)start.y)); rightVector.normalize(); \
	crossResult = mCross(Point3F(rightVector.x, 0.f, rightVector.y), Point3F(0.f, 1.f, 0.f)); \
	rightVector = Point2F(crossResult.x, crossResult.z); rightVector.normalize();\
	dglDrawLine(Point2I(S32((F32)start.x + rightVector.x), S32((F32)start.y + rightVector.y)), Point2I(S32((F32)end.x + rightVector.x), S32((F32)end.y + rightVector.y)), color);\
	dglDrawLine(Point2I(S32((F32)start.x + rightVector.x), S32((F32)start.y + rightVector.y)), end, color);\
	dglDrawLine(Point2I(S32((F32)start.x + rightVector.x), S32((F32)start.y + rightVector.y)), Point2I(S32((F32)end.x - rightVector.x), S32((F32)end.y - rightVector.y)), color);\
	dglDrawLine(start, Point2I(S32((F32)end.x - rightVector.x), S32((F32)end.y - rightVector.y)), color);\
	dglDrawLine(start, end, color);\
	dglDrawLine(Point2I(S32((F32)start.x - rightVector.x), S32((F32)start.y - rightVector.y)), Point2I(S32((F32)end.x - rightVector.x), S32((F32)end.y - rightVector.y)), color);\

	// Trying to get from right to left here
	// Start drawing the lines between points
	Point2I pLastPoint  = Point2I(0, 0);
	Point2F rightVector = Point2F(0.f, 0.f);
	Point3F crossResult = Point3F(0.f, 0.f, 0.f);
	for (int i = 0; i < iNumValidPoints; i++)
	{
		int iPoint = pPointBuffer[i];
		Point2I begin(0, 0);
		Point2I end(0, 0);

		begin.x = 2 + (rGraphBox.point.x + S32((F32)i * F32((F32)rGraphBox.extent.x / (F32)iNumValidPoints)));
		begin.y = rGraphBox.point.y + (rGraphBox.extent.y - S32(F32(rGraphBox.extent.y) * F32((F32)iPoint / (F32)iHighestPoint)));
		end.x   = (rGraphBox.point.x + S32((F32)(i + 1) * F32((F32)rGraphBox.extent.x / (F32)iNumValidPoints))) - 2;
		end.y   = begin.y;

		// And finally, draw it
		if (i != 0)
		{
			//dglDrawLine(pLastPoint, begin, ColorI(175, 214, 166, 255));
			DRAW_LINE(pLastPoint, begin, ColorI(175, 214, 166, 255));
		}

		//dglDrawLine(begin, end, ColorI(175, 214, 166, 255));
		DRAW_LINE(begin, end, ColorI(175, 214, 166, 255));

		// Set our point as the last point & keep on going
		pLastPoint = end;
	}

	dglClearBitmapModulation();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Torque functions

ConsoleFunctionGroupBegin(ThreadedDownloading, "Threaded downloading.");

ConsoleFunction(downloadFile, S32, 3, 7, "(URL, savePath[, onCompleteCallback[, onProgressCallback[, onDownloadStartCallback[, onDownloadFailCallback]]]])")
{
	DownloadID OurID = ThreadedDownloading::Download(argv[1], argv[2]);

	// Check if it successfully queue'd up
	if (OurID == INVALID_DOWNLOAD_ID)
		return -1;

	// Get the download object
	ThreadedDownloading::QueuedDownload* pEntry = ThreadedDownloading::GetQueueEntry(OurID);

	// This is never supposed to happen!
	if (pEntry == NULL)
	{
		Con::errorf("Something has gone terribly wrong!");
		throw;
	}

	// Link the specified callbacks
	if (argc >= 4)
	{
		// Setup the 'complete' callback
		pEntry->onDownloadComplete.AddListener([](void* userData, U32 argc, char* argList)->void
		{
			const char* pFileName = CALLBACK_EVENT_ARG(const char*);

			PERFORM_THREADED_EVAL("%s(\"%s\");", (const char*)userData, pFileName);
		}, (void*)StringTable->insert(argv[3], true));
	}

	if (argc >= 5)
	{
		// Setup the 'progress' callback
		pEntry->onDownloadProgress.AddListener([](void* userData, U32 argc, char* argList)->void
		{
			int iContentLength    = CALLBACK_EVENT_ARG(int);
			int iContentLengthMax = CALLBACK_EVENT_ARG(int);
			int iTransferRate     = CALLBACK_EVENT_ARG(int);

			PERFORM_THREADED_EVAL("%s(%d, %d, %d);", (const char*)userData, iContentLength, DThread_ContentLengthMax, iTransferRate);
		}, (void*)StringTable->insert(argv[4], true));
	}

	if (argc >= 6)
	{
		// Setup the 'start' callback
		pEntry->onDownloadStart.AddListener([](void* userData, U32 argc, char* argList)->void
			{
				int iContentLength = CALLBACK_EVENT_ARG(int);

				PERFORM_THREADED_EVAL("%s(%d);", (const char*)userData, iContentLength);
			}, (void*)StringTable->insert(argv[5], true));
	}

	if (argc >= 7)
	{
		// Setup the 'fail' callback
		pEntry->onDownloadFailed.AddListener([](void* userData, U32 argc, char* argList)->void
			{
				const char* pFailReason = CALLBACK_EVENT_ARG(const char*);

				PERFORM_THREADED_EVAL("%s(\"%s\");", (const char*)userData, pFailReason);
			}, (void*)StringTable->insert(argv[6], true));
	}

	// Done!
	return (S32)OurID;
}

ConsoleFunction(cancelDownload, bool, 2, 2, "(id)")
{
	DownloadID id = (DownloadID)dAtoi(argv[1]);
	return ThreadedDownloading::Cancel(id);
}

ConsoleFunction(isDownloadPaused, bool, 1, 1, "")
{
	return dQueue_Paused;
}

ConsoleFunction(pauseDownload, void, 1, 1, "")
{
	Queue_PauseDownload();
}

ConsoleFunction(resumeDownload, void, 1, 1, "")
{
	Queue_ResumeDownload();
}

ConsoleFunction(dumpDownloadQueue, void, 1, 1, "")
{
	Con::printf("%d active download%s @ (%08p)", QueuedDownload::queueLength, QueuedDownload::queueLength == 1 ? "" : "s", QueuedDownload::queue);

	for (S32 i = 0; i < QueuedDownload::queueLength; i++)
	{
		QueuedDownload* dEntry = QueuedDownload::queue[i];

		Con::printf("[%04d] : \"%s:%d%s%s\" -> \"%s\"", i, dEntry->GetURL()->getHostName(), dEntry->GetURL()->getPortNum(), (*dEntry->GetURL()->getPath() == '/' ? "" : "/"), dEntry->GetURL()->getPath(), dEntry->GetSavePath());
	}
}

ConsoleFunction(dumpDownloadLinks, void, 1, 1, "")
{
	// Count how many there are
	U32 count = 0;
	for (QueuedDownload* walk = QueuedDownload::first; walk; walk = walk->next)
		count++;

	// Print amount
	Con::printf("%d total download entr%s", count, count == 1 ? "y" : "ies");

	// Print downloads
	count = 0;
	for (QueuedDownload* walk = QueuedDownload::first; walk; walk = walk->next)
	{
		Con::printf("[%04d] : \"%s:%d%s%s\" -> \"%s\"", count, walk->GetURL()->getHostName(), walk->GetURL()->getPortNum(), (*walk->GetURL()->getPath() == '/' ? "" : "/"), walk->GetURL()->getPath(), walk->GetSavePath());
		count++;
	}
}

ConsoleFunctionGroupEnd(ThreadedDownloading);