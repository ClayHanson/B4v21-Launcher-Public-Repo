#include "game/net/TCPBinaryDownload.h"
#include "platform/gameInterface.h"
#include "game/net/TCPQuery.h"
#include "core/resManager.h"
#include "core/fileStream.h"

#define MAX_DOWNLOADS 2048
#define MAX_RETRIES 3

// THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! 
// 
// This garbage download system is being replaced by those found in ThreadedDownloading.h!
// Consider these deprecated. They will be removed at some point in the future.
// 
// THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! THIS IS ALL OUTDATED! 

struct DownloadQueueObject
{
	S32 downloadId;
	void(*cbf_doneCallback)(void* userData, const char* fName, S32 response);
	void(*cbf_onProgress)(void* userData, S32 curr, S32 max);
	void(*cbf_onDownloadStart)(void* userData, DownloadTCPID);
	FileStream* stream;
	char* cb_doneCallback;
	char* cb_onProgress;
	char* cb_onDownloadStart;
	char* hostName;
	char* port;
	char* path;
	char* savePath;
	char* postString;
	U32 expectedSize;
	S32 retries;
	void* userData;
};

static TCPQuery* DOWNLOAD_TCP_QUERY_OBJECT = NULL;
static DownloadQueueObject* DLqueue[MAX_DOWNLOADS];
static bool DLqueueFilled = false;
static S32 DLqueueSize = 0;
static S32 DLlastID = 0;

void TCPF_DL_onFailure(TCPQuery* o);
void TCPF_DL_onConnected(TCPQuery* o);
void TCPF_DL_onDisconnected(TCPQuery* o);
void TCPF_DL_onRawData(TCPQuery* o, const char* data, U32 size);
void TCPF_DL_onRedirect(TCPQuery* o, const char* newURL);
void TCPF_DL_deleteQueuedObject(DownloadQueueObject* queueObject);
void TCPF_DL_NextDownload();

DownloadTCPID downloadTCPFile(const char* hostName, const char* port, const char* path, const char* savePath, const char* postString,
	StringTableEntry onDownloadStartFuncName,
	StringTableEntry onCompleteFuncName,
	StringTableEntry onProgressFuncName,
	void(*onDownloadStartFunc)(void* userData, DownloadTCPID),
	void(*onCompleteFunc)(void* userData, const char* fName, S32 response),
	void(*onProgressFunc)(void* userData, S32 curr, S32 max),
	void* userData)
{
	if (!DLqueueFilled)
	{
		DLqueueFilled = true;
		for (S32 i = 0; i < MAX_DOWNLOADS; i++) DLqueue[i] = NULL;
	}

	if (DLqueueSize >= MAX_DOWNLOADS)
	{
		Con::errorf("downloadTCPFile() - No room for a new queue entry; Already have %05d / %05d downloads queued.", DLqueueSize, MAX_DOWNLOADS);
		return -1;
	}

	DownloadQueueObject* queueObject = new DownloadQueueObject;
	queueObject->userData            = userData;
	queueObject->downloadId          = DLlastID++;
	queueObject->cbf_doneCallback    = onCompleteFunc;
	queueObject->cbf_onProgress      = onProgressFunc;
	queueObject->cbf_onDownloadStart = onDownloadStartFunc;
	queueObject->cb_doneCallback     = (onCompleteFuncName ? new char[dStrlen(onCompleteFuncName) + 1] : NULL);
	queueObject->cb_onProgress       = (onProgressFuncName ? new char[dStrlen(onProgressFuncName) + 1] : NULL);
	queueObject->cb_onDownloadStart  = (onDownloadStartFuncName ? new char[dStrlen(onDownloadStartFuncName) + 1] : NULL);
	queueObject->hostName            = new char[dStrlen(hostName) + 1];
	queueObject->port                = new char[dStrlen(port) + 1];
	queueObject->path                = new char[dStrlen(path) + 1];
	queueObject->savePath            = new char[dStrlen(savePath) + 1];
	queueObject->postString          = new char[dStrlen(postString) + 1];
	queueObject->stream              = new FileStream();
	queueObject->expectedSize        = 0;

	if (queueObject->cb_doneCallback) dStrcpy(queueObject->cb_doneCallback, onCompleteFuncName);
	if (queueObject->cb_onProgress) dStrcpy(queueObject->cb_onProgress, onProgressFuncName);
	if (queueObject->cb_onDownloadStart) dStrcpy(queueObject->cb_onDownloadStart, onDownloadStartFuncName);
	dStrcpy(queueObject->hostName, hostName);
	dStrcpy(queueObject->port, port);
	dStrcpy(queueObject->path, path);
	dStrcpy(queueObject->savePath, savePath);
	dStrcpy(queueObject->postString, postString);

	DLqueue[DLqueueSize] = queueObject;
	DLqueueSize++;
	if (DLqueueSize == 1) TCPF_DL_NextDownload();

	return queueObject->downloadId;
}

void TCPF_DL_NextDownload()
{
	if (DLqueueSize == 0)
		return;

	DownloadQueueObject* queueObject = DLqueue[0];

	// If we have no more queue'd downloads, then just stop.
	if (!queueObject)
		return;
	
	// If the TCPQuery object doesn't exist, create it with our callback functions.
	if (!DOWNLOAD_TCP_QUERY_OBJECT)
		DOWNLOAD_TCP_QUERY_OBJECT = new TCPQuery(TCPF_DL_onConnected, NULL, TCPF_DL_onFailure, TCPF_DL_onFailure, TCPF_DL_onDisconnected, NULL, TCPF_DL_onRawData, NULL, TCPF_DL_onRedirect, false);

	// Announce the next file.
	Con::printf("Downloading next file in queue (\"%s%s%s\")...", queueObject->hostName, (*queueObject->path == '/' ? "" : "/"), queueObject->path);

	// Callback
	if (queueObject->cb_onDownloadStart)
	{
		char dId[128];
		dSprintf(dId, 128, "%d", queueObject->downloadId);
		Con::executef(2, queueObject->cb_onDownloadStart, dId);
	}

	if (queueObject->cbf_onDownloadStart)
		queueObject->cbf_onDownloadStart(queueObject->userData, queueObject->downloadId);

	char address[1024];
	dSprintf(address, 1024, "%s:%s", queueObject->hostName, queueObject->port);
	DOWNLOAD_TCP_QUERY_OBJECT->connect(address);

}

void TCPF_DL_deleteQueuedObject(DownloadQueueObject* queueObject)
{
	if (!queueObject)
		return;

	delete[] queueObject->hostName;
	delete[] queueObject->port;
	delete[] queueObject->path;
	delete[] queueObject->savePath;
	delete[] queueObject->postString;
	if (queueObject->cb_doneCallback) delete[] queueObject->cb_doneCallback;
	if (queueObject->cb_onDownloadStart) delete[] queueObject->cb_onDownloadStart;
	if (queueObject->cb_onProgress) delete[] queueObject->cb_onProgress;
	if (queueObject->stream) delete queueObject->stream;
	delete queueObject;
}

void TCPF_DL_PushNextDownload()
{
	if (DLqueueSize != 0)
	{
		DownloadQueueObject* queueObject = DLqueue[0];

		// Delete the downloaded queue object
		TCPF_DL_deleteQueuedObject(queueObject);

		// Reorder
		for (S32 i = 1; i <= DLqueueSize; i++)
			DLqueue[i - 1] = (i >= MAX_DOWNLOADS ? NULL : DLqueue[i]);
		
		// Resize
		DLqueueSize--;
	}

	TCPF_DL_NextDownload();
}

void TCPF_DL_onFailure(TCPQuery* o)
{
	if (DLqueue[0]->retries++ >= MAX_RETRIES)
	{
		Con::printf("Failed to download %s/%s.", DLqueue[0]->hostName, DLqueue[0]->path);
		TCPF_DL_PushNextDownload();
	}
	else
	{
		Con::printf("Failed to download %s/%s, retrying... [%d]", DLqueue[0]->hostName, DLqueue[0]->path);
		TCPF_DL_NextDownload();
	}
}

void TCPF_DL_onConnected(TCPQuery* o)
{
	if (!ResourceManager->openFileForWrite(*DLqueue[0]->stream, DLqueue[0]->savePath, 1U, true))
	{
		Con::errorf("Failed to open \"%s\" for writing", DLqueue[0]->savePath);
		delete DLqueue[0]->stream;
		DLqueue[0]->stream = NULL;
		return;
	} else
		{
		char send[1024];
		dSprintf(send, 1024, "GET %s%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: BlocklandLauncher\r\nConnection: close\r\n%s", (*DLqueue[0]->path == '/' ? "" : "/"), DLqueue[0]->path, DLqueue[0]->hostName, DLqueue[0]->postString);
		o->send(send);
	}
}

void TCPF_DL_onDisconnected(TCPQuery* o)
{
	if (DLqueue[0]->stream) DLqueue[0]->stream->close();
	if (DLqueue[0]->cb_doneCallback) Con::executef(2, DLqueue[0]->cb_doneCallback, DLqueue[0]->savePath);
	if (DLqueue[0]->cbf_doneCallback) DLqueue[0]->cbf_doneCallback(DLqueue[0]->userData, DLqueue[0]->savePath, DOWNLOAD_TCP_QUERY_OBJECT->statusResonse);
	TCPF_DL_PushNextDownload();
}

void TCPF_DL_onRawData(TCPQuery* o, const char* data, U32 size)
{
	if (!DLqueue[0]->stream) return;
	if (DLqueue[0]->cb_onProgress)
	{
		char curr[128];
		char max[128];

		dSprintf(curr, 128, "%d", DOWNLOAD_TCP_QUERY_OBJECT->contentLength);
		dSprintf(max, 128, "%d", DOWNLOAD_TCP_QUERY_OBJECT->contentLengthMax);

		Con::executef(3, DLqueue[0]->cb_onProgress, curr, max);
	}

	if (DLqueue[0]->cbf_onProgress) DLqueue[0]->cbf_onProgress(DLqueue[0]->userData, DOWNLOAD_TCP_QUERY_OBJECT->contentLength, DOWNLOAD_TCP_QUERY_OBJECT->contentLengthMax);
	DLqueue[0]->stream->write(size, (const void*)data);
}

void TCPF_DL_DontHandleDisconnect(TCPQuery* o)
{
	o->setOnDisconnect(TCPF_DL_onDisconnected);
}

void TCPF_DL_handleCancelDisconnect(TCPQuery* o)
{
	o->setOnDisconnect(TCPF_DL_onDisconnected);
	TCPF_DL_PushNextDownload();
}

void TCPF_DL_onRedirect(TCPQuery* o, const char* newURL)
{
	//DLqueue[0]
	char newPort[32]; dStrcpy(newPort, "80");
	const char* path = newURL;
	const char* url  = newURL;

	if (dStrstr(newURL, "://"))
	{
		if (dStrstr(newURL, "https://") == newURL) dStrcpy(newPort, "443");
		url = dStrstr(newURL, "://") + 3;
	}

	path = dStrchr(url, '/');
	if (path == NULL) path = newURL + dStrlen(newURL);

	delete[] DLqueue[0]->hostName;
	delete[] DLqueue[0]->path;
	delete[] DLqueue[0]->port;

	DLqueue[0]->hostName = new char[(path - url) + 1];
	DLqueue[0]->path = new char[dStrlen(path) + 1];
	DLqueue[0]->port = new char[dStrlen(newPort) + 1];

	char* ptr = DLqueue[0]->hostName;
	for (S32 i = 0; i < (path - url); i++) *ptr++ = *(url + i); *ptr++ = 0;
	dStrcpy(DLqueue[0]->path, path);
	dStrcpy(DLqueue[0]->port, newPort);

	DLqueue[0]->stream->close();

	DOWNLOAD_TCP_QUERY_OBJECT->setOnDisconnect(TCPF_DL_DontHandleDisconnect);
	DOWNLOAD_TCP_QUERY_OBJECT->disconnect();

	char addr[1024];
	dSprintf(addr, 1024, "%s:%s", DLqueue[0]->hostName, newPort);

	DOWNLOAD_TCP_QUERY_OBJECT->connect(addr);
}

bool cancelTCPFileDownload(DownloadTCPID id)
{
	for (S32 i = 0; i < DLqueueSize; i++)
	{
		DownloadQueueObject* qObj = DLqueue[i];

		// Try to find a queue entry with this download id
		if (qObj->downloadId != id)
			continue;

		// We found it -- delete it (and stop the download process if necessary)
		if (i == 0)
		{
			DOWNLOAD_TCP_QUERY_OBJECT->setOnDisconnect(TCPF_DL_handleCancelDisconnect);
			DOWNLOAD_TCP_QUERY_OBJECT->disconnect();

			// Increment download
			//TCPF_DL_PushNextDownload();
			return true;
		}

		// Re-organize the array
		for (i += 1; i <= DLqueueSize; i++)
			DLqueue[i - 1] = (i == MAX_DOWNLOADS ? NULL : DLqueue[i]);

		// Re-size the array
		DLqueueSize--;

		// Free memory
		TCPF_DL_deleteQueuedObject(qObj);

		return true;
	}

	// We didn't find it.
	return false;
}