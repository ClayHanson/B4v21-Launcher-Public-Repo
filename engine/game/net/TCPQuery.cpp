//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "game/net/TCPQuery.h"
#include "game/net/tcpObject.h"

#include "platform/platform.h"
#include "platform/event.h"
#include "platform/gameInterface.h"
#include "console/simBase.h"
#include "console/consoleInternal.h"
#include "game/demoGame.h"
#include "math/mMath.h"
#include "platform/profiler.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_SIZE_PER_READ 8192
#define MAX_CHUNKED_DOWNLOAD_SPEED 2048 * 3

TCPQuery* TCPQuery::first = NULL;
static Thread* netThread  = NULL;
static S32 stepsPerRead   = 128;
static S32 sizePerRead    = 8192;

#ifdef TCPQUERY_MULTI_THREAD
void THREAD_netprocessing(S32 arg) {
	bool valid = true;

	while (valid) {
		PROFILE_START(NetProcessSSL);
		Net::processSSL();
		PROFILE_END();

		if (TCPQuery::first != NULL) {
			valid = false;
			for (TCPQuery* walk = TCPQuery::first; walk; walk = walk->next) {
				if (walk->getState() != TCPQuery::Disconnected) {
					valid = true;
					break;
				}
			}
		} else valid = false;
	}

	netThread = NULL;
}
#endif

TCPQuery::TCPQuery(void(*cba_onConnected)(TCPQuery*),
	void(*cba_onDNSResolved)(TCPQuery*),
	void(*cba_onDNSFailed)(TCPQuery*),
	void(*cba_onConnectFailed)(TCPQuery*),
	void(*cba_onDisconnected)(TCPQuery*),
	void(*cba_onLine)(TCPQuery*, const char* data),
	void(*cba_onRawData)(TCPQuery*, const char* data, U32 size),
	void(*cba_onHeader)(TCPQuery*, const char* headerName, const char* value),
	void(*cba_onRedirect)(TCPQuery*, const char* newUrl),
	bool deleteOnceDone)
{
	cb_onConnected = cba_onConnected;
	cb_onDNSResolved = cba_onDNSResolved;
	cb_onDNSFailed = cba_onDNSFailed;
	cb_onConnectFailed = cba_onConnectFailed;
	cb_onDisconnected = cba_onDisconnected;
	cb_onLine = cba_onLine;
	cb_onRawData = cba_onRawData;
	cb_onHeader = cba_onHeader;
	cb_onRedirect = cba_onRedirect;

	contentLengthMax = 0;
	chunkedLeftOver = 0;
	contentLength = 0;
	deleteOnDisconnect = deleteOnceDone;
	gotHeaders = false;
	mBuffer = NULL;
	mBufferSize = 0;
	mPort = 0;
	mTag = InvalidSocket;
	mNext = NULL;
	mState = Disconnected;
	encType = NORMAL;
	statusResonse = 0;
#ifdef TCPQUERY_MULTI_THREAD
	mutex = Mutex::createMutex();
	locked = false;
#endif

	ctx = NULL;
	ssl = NULL;

	next  = first;
	first = this;

	doDisconnect = false;
}

TCPQuery::~TCPQuery() {
#ifdef TCPQUERY_MULTI_THREAD
	Mutex::destroyMutex(mutex);
#endif
	disconnect();
	dFree(mBuffer);

	// Relink
	if (first == this) {
		first = next;
	} else {
		for (TCPQuery* walk = TCPQuery::first; walk; walk = walk->next) {
			if (walk->next != this) continue;
			walk->next = next;
			break;
		}
	}

	if (ctx) SSL_CTX_free(ctx);
}

void TCPQuery::init() {
	Con::addVariable("TCPQuery::StepsPerRead", TypeS32, &stepsPerRead);
	Con::addVariable("TCPQuery::SizePerRead", TypeS32, &sizePerRead);
	Con::setIntVariable("TCPQuery::MaxSizePerRead", MAX_SIZE_PER_READ);
}

TCPQuery *TCPQuery::table[TCPQuery::TableSize] = { 0, };

TCPQuery *TCPQuery::find(NetSocket tag) {
	for (TCPQuery *walk = table[U32(tag) & TableMask]; walk; walk = walk->mNext)
		if (walk->mTag == tag)
			return walk;
	return NULL;
}

void TCPQuery::addToTable(NetSocket newTag) {
	removeFromTable();
	mTag = newTag;
	mNext = table[U32(mTag) & TableMask];
	table[U32(mTag) & TableMask] = this;
}

void TCPQuery::removeFromTable() {
	for (TCPQuery **walk = &table[U32(mTag) & TableMask]; *walk; walk = &((*walk)->mNext)) {
		if (*walk == this) {
			*walk = mNext;
			return;
		}
	}
}

bool TCPQuery::processArguments(S32 argc, const char **argv) {
	if (argc == 0)
		return true;
	else if (argc == 1) {
		addToTable(U32(dAtoi(argv[0])));
		return true;
	}
	return false;
}

U32 TCPQuery::onReceive(U8 *buffer, U32 bufferLen) {
	// we got a raw buffer event
	// default action is to split the buffer into lines of text
	// and call processLine on each
	// for any incomplete lines we have mBuffer

	U32 start = 0;
	parseLine(buffer, &start, bufferLen);
	return start;
}

void TCPQuery::parseLine(U8 *buffer, U32 *start, U32 bufferLen) {
	if (mState == Disconnected) {
		// ignore
		*start += bufferLen;
		return;
	}

	U32 i;
	U8 *line = buffer + *start;

	if (gotHeaders && cb_onRawData) {
		U32 usedBuffer = bufferLen;

		if (encType == TransferEncoding::CHUNKED) {
			if (chunkedLeftOver != 0) {
				usedBuffer       = mClamp(mClamp(usedBuffer, 1, chunkedLeftOver), 1, MAX_CHUNKED_DOWNLOAD_SPEED);
				chunkedLeftOver -= usedBuffer;
			} else goto PARSE_LINE_CONTINUE;
		}

		contentLength += usedBuffer;
		cb_onRawData(this, (const char*)(buffer + *start), usedBuffer);
		*start += usedBuffer;
		return;
	}

	PARSE_LINE_CONTINUE:
	 
	for (i = *start; i < bufferLen; i++)
		if (buffer[i] == '\n' || buffer[i] == 0)
			break;

	U32 len = i - *start;

	if (i == bufferLen || mBuffer) {
		// we've hit the end with no newline
		mBuffer = (U8 *)dRealloc(mBuffer, mBufferSize + len + 2);
		dMemcpy(mBuffer + mBufferSize, line, len);
		mBufferSize += len;
		*start = i;

		// process the line
		if (i != bufferLen) {
			mBuffer[mBufferSize] = 0;
			if (mBufferSize && mBuffer[mBufferSize - 1] == '\r')
				mBuffer[mBufferSize - 1] = 0;
			U8 *temp = mBuffer;
			mBuffer = 0;
			mBufferSize = 0;

			processLine(temp);
			dFree(temp);
		}
	}
	else if (i != bufferLen) {
		line[len] = 0;
		if (len && line[len - 1] == '\r')
			line[len - 1] = 0;

		processLine(line);
	}
	if (i != bufferLen)
		*start = i + 1;
}

void TCPQuery::onConnectionRequest(const NetAddress *addr, U32 connectId) {
	Con::errorf("TCPQuery::onConnectionRequest() - This should never be called -- the goon?");
}

bool TCPQuery::processLine(U8 *line) {
	if (gotHeaders && encType == TransferEncoding::CHUNKED && *line != 0) {
		chunkedLeftOver = dStrtol((const char*)line, NULL, 16);

		if (chunkedLeftOver == 0) {
			onDisconnect();
			disconnect();
		}

		return true;
	} else if (!gotHeaders && *line == 0) {
		gotHeaders = true;
		return true;
	} else if (!gotHeaders) {
		char* headerEnd = dStrstr((const char*)line, ": ");
		if (headerEnd == NULL) {
			if (dStrstr((const char*)line, "HTTP/1.1 ") == (char*)line) {
				// Status code response
				statusResonse = dAtoi((const char*)line + dStrlen("HTTP/1.1 "));
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
		if (!dStricmp(headerName, "Content-Length")) contentLengthMax = dAtoi(headerValue);
		else if (!dStricmp(headerName, "Location")) {
			if (cb_onRedirect) cb_onRedirect(this, headerValue);
		} else if (!dStricmp(headerName, "Transfer-Encoding")) {
			if (!dStricmp(headerValue, "chunked")) {
				encType = TransferEncoding::CHUNKED;
			}
		}

		// Send to onHeader callback
		if (cb_onHeader) cb_onHeader(this, headerName, headerValue);

		delete[] headerName;
		delete[] headerValue;

		return true;
	}

	if (cb_onLine) cb_onLine(this, (const char*)line);
	return true;
}

void TCPQuery::onDNSResolved() {
	mState = DNSResolved;
	if (cb_onDNSResolved) cb_onDNSResolved(this);
}

void TCPQuery::onDNSFailed() {
	mState = Disconnected;
	if (cb_onDNSFailed) cb_onDNSFailed(this);
}

void TCPQuery::onConnected() {
	mState = Connected;
	if (cb_onConnected) cb_onConnected(this);

#ifdef TCPQUERY_MULTI_THREAD
	// Create a thread for downloading stuff
	if (!netThread) netThread = new Thread(THREAD_netprocessing, 0, true);
#endif
}

void TCPQuery::onConnectFailed() {
	mState = Disconnected;
	if (cb_onConnectFailed) cb_onConnectFailed(this);
}

void TCPQuery::finishLastLine() {
	if (mBufferSize) {
		mBuffer[mBufferSize] = 0;
		processLine(mBuffer);
		dFree(mBuffer);
		mBuffer = 0;
		mBufferSize = 0;
	}
	else mBuffer = 0;
}

void TCPQuery::onDisconnect() {
	finishLastLine();
	mState = Disconnected;

	if (ssl) {
		SSL_free(ssl);
		ssl = NULL;
	}

	if (cb_onDisconnected) cb_onDisconnected(this);
}

void TCPQuery::listen(U16 port) {
	mState = Listening;
	U32 newTag = Net::openListenPort(port);
	addToTable(newTag);
}

void TCPQuery::connect(const char *address) {
	gotHeaders       = false;
	contentLengthMax = 0;
	contentLength    = 0;
	chunkedLeftOver  = 0;
	encType          = TransferEncoding::NORMAL;

	char remoteAddr[256];
	dStrcpy(remoteAddr, address);
	char *portString = dStrchr(remoteAddr, ':');

	if (portString) {
		*portString++ = 0;
		mPort = dAtoi(portString);
	} else mPort = 80;

	if (mPort != 443) {
		NetSocket newTag = Net::openConnectTo(address);
		addToTable(newTag);
		return;
	}

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (!s) {
		Con::printf("TCPQuery::connect() - Error creating socket.");
		return;
	}

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(remoteAddr); // address of google.ru
	sa.sin_port = htons(mPort);
	S32 socklen = sizeof(sa);

	if (sa.sin_addr.s_addr != INADDR_NONE) {
		if (::connect(s, (struct sockaddr *)&sa, socklen) && WSAGetLastError() != WSAEWOULDBLOCK) {
			Con::printf("TCPQuery::connect() - Error connecting to server.");
			return;
		}
	} else {
		struct hostent* phost = gethostbyname(remoteAddr);

		if ((phost) && (phost->h_addrtype == AF_INET)) {
			sa.sin_addr = *(in_addr*)(phost->h_addr);
			if (::connect(s, (struct sockaddr *)&sa, socklen) && WSAGetLastError() != WSAEWOULDBLOCK) {
				Con::printf("TCPQuery::connect() - Error connecting to server.");
				return;
			}
		} else {
			Con::errorf("TCPQuery::connect() - Error.");
			return;
		}
	}
	SSL_CTX* cl;
	if (!s) {
		Con::printf("TCPQuery::connect() - Error creating socket.");
		cb_onConnectFailed(this);
		return;
	}

	if (!ctx) ctx = SSL_CTX_new(SSLv23_client_method());
	ssl = SSL_new(ctx);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);

	if (!ssl) {
		Con::errorf("TCPQuery::connect() - Error creating SSL.");
		cb_onConnectFailed(this);
		return;
	}

	mTag = SSL_get_fd(ssl);
	SSL_set_fd(ssl, s);
	int err = SSL_connect(ssl);

	if (err <= 0) {
		Con::errorf("TCPQuery::connect() - Error creating SSL connection.  err=%x", err);
		ssl = NULL;
		SSL_free(ssl);

		// Try normally adding socket
		NetSocket newTag = Net::openConnectTo(address);
		addToTable(newTag);
		return;
	}

	onConnected();
}

void TCPQuery::disconnect() {
	if (mState == Disconnected) return;

#ifdef TCPQUERY_MULTI_THREAD
	if (Con::isMainThread()) {
		unlock();

		SSLThreadPause = 1;
		doDisconnect   = true;
		while (SSLThreadPause != 2)
		{
			extern GameInterface* Game;
			Game->journalProcess();
			Net::process();
			Platform::process();
			TimeManager::process();
			Sleep(32);
		}

		doDisconnect = false;
		if (!ssl) {
			if (mTag != InvalidSocket) {
				Net::closeConnectTo(mTag);
			}

			removeFromTable();
		}
		else
		{
			closesocket(mTag);
			ssl = NULL;
		}

		mState = Disconnected;
		if (cb_onDisconnected)
			cb_onDisconnected(this);

		SSLThreadPause = 1;
		while (SSLThreadPause != 0)
		{
			extern GameInterface* Game;
			Game->journalProcess();
			Net::process();
			Platform::process();
			TimeManager::process();
			Sleep(32);
		}
	} else {
		doDisconnect = false;

		if (!ssl) {
			if (mTag != InvalidSocket) {
				Net::closeConnectTo(mTag);
			}

			removeFromTable();
		}
		else
		{
			closesocket(mTag);
			ssl = NULL;
		}

		mState = Disconnected;
		if (cb_onDisconnected)
			cb_onDisconnected(this);
	}
#else
	if (!ssl) {
		if (mTag != InvalidSocket) {
			Net::closeConnectTo(mTag);
		}

		removeFromTable();
	}
	else {
		closesocket(mTag);
		ssl = NULL;
	}

	mState = Disconnected;
	if (cb_onDisconnected) cb_onDisconnected(this);
#endif
}

#ifdef TCPQUERY_MULTI_THREAD
bool TCPQuery::isLocked() {
	return locked;
}

void TCPQuery::lock() {
	Mutex::lockMutex(mutex);
	locked = true;
}

void TCPQuery::unlock() {
	Mutex::unlockMutex(mutex);
	locked = false;
}
#endif

void TCPQuery::send(const char *buffer) {
	if (ssl) {
		S32 len = SSL_write(ssl, buffer, dStrlen(buffer));

		if (len < 0) {
			S32 err = SSL_get_error(ssl, len);
			switch (err) {
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
	} else {
		Net::sendtoSocket(mTag, (const U8*)buffer, dStrlen(buffer));
	}
}

int TCPQuery::RecvPacket()
{
	if (!ssl || mState != Connected)
		return 0;

#ifdef TCPQUERY_MULTI_THREAD
	lock();
#endif

	char buf[MAX_SIZE_PER_READ + 1];
	S32 loops = 0;
	S32 len   = 100;

	for (S32 i = 0; i < stepsPerRead; i++)
	{
		if (doDisconnect)
			break;

		len      = SSL_read(ssl, buf, sizePerRead);
		buf[len] = 0;

		S32 size   = len;
		U8* buffer = (U8*)buf;
		while (size)
		{
			U32 ret = onReceive((U8*)buffer, size);
			size   -= ret;
			buffer += ret;
		}

		if (len <= 0 && encType == TransferEncoding::NORMAL)
		{
			onDisconnect();
			disconnect();
		}

		if (!ssl)
			break;

		S16 err = 0;
		SSL_get_error(ssl, err);

		if (err)
			break;
	}

#ifdef TCPQUERY_MULTI_THREAD
	unlock();
#endif

	return 0;
}

void DemoGame::processConnectedReceiveEvent(ConnectedReceiveEvent* event) {
	U32 size        = U32(event->size - ConnectedReceiveEventHeaderSize);
	U8 *buffer      = event->data;

	TCPObject *tcpo = TCPObject::find(event->tag);
	if (!tcpo) {
		TCPQuery* tcpa = TCPQuery::find(event->tag);

		if (!tcpa) {
			Con::printf("Got bad connected receive event.");
			return;
		}

		while (size) {
			U32 ret = tcpa->onReceive(buffer, size);
			//if (ret > size) break;
			size -= ret;
			buffer += ret;
		}
		return;
	}

	while (size) {
		U32 ret = tcpo->onReceive(buffer, size);
		AssertFatal(ret <= size, "Invalid return size");
		size -= ret;
		buffer += ret;
	}
}

void DemoGame::processConnectedAcceptEvent(ConnectedAcceptEvent* event) {
	TCPObject *tcpo = TCPObject::find(event->portTag);
	if (!tcpo) {
		TCPQuery *tcpa = TCPQuery::find(event->portTag);
		if(!tcpa) return;
		tcpa->onConnectionRequest(&event->address, event->connectionTag);
		return;
	}
	tcpo->onConnectionRequest(&event->address, event->connectionTag);
}

void DemoGame::processConnectedNotifyEvent(ConnectedNotifyEvent* event) {
	TCPObject* tcpo = TCPObject::find(event->tag);

	if (!tcpo) {
		TCPQuery* tcpa = TCPQuery::find(event->tag);
		if (!tcpa) return;

		switch (event->state) {
			case ConnectedNotifyEvent::DNSResolved:
				tcpa->onDNSResolved();
				break;
			case ConnectedNotifyEvent::DNSFailed:
				tcpa->onDNSFailed();
				break;
			case ConnectedNotifyEvent::Connected:
				tcpa->onConnected();
				break;
			case ConnectedNotifyEvent::ConnectFailed:
				tcpa->onConnectFailed();
				break;
			case ConnectedNotifyEvent::Disconnected:
				tcpa->onDisconnect();
				break;
		}
		return;
	}

	switch (event->state) {
		case ConnectedNotifyEvent::DNSResolved:
			tcpo->onDNSResolved();
			break;
		case ConnectedNotifyEvent::DNSFailed:
			tcpo->onDNSFailed();
			break;
		case ConnectedNotifyEvent::Connected:
			tcpo->onConnected();
			break;
		case ConnectedNotifyEvent::ConnectFailed:
			tcpo->onConnectFailed();
			break;
		case ConnectedNotifyEvent::Disconnected:
			tcpo->onDisconnect();
			break;
	}
}