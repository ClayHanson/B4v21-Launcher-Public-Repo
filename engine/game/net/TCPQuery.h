//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _NETC_H_
#define _NETC_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif

struct ssl_st;
typedef struct ssl_st SSL;
struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

#define TCPQUERY_USE_SSL
#define TCPQUERY_MULTI_THREAD

#ifdef TCPQUERY_MULTI_THREAD
extern int SSLThreadPause;
#endif

class TCPQuery {
public:
	enum State { Disconnected, DNSResolved, Connected, Listening };

	enum TransferEncoding {
		NORMAL,
		CHUNKED
	};

public:
	static TCPQuery* first;
	TCPQuery* next;

private:
	enum { TableSize = 256, TableMask = 0xFF };
	static TCPQuery* table[TableSize];
	TCPQuery *mNext;
	State mState;

#ifdef TCPQUERY_MULTI_THREAD
	void* mutex;
	bool locked;
#endif

	int mTag;

	SSL *ssl;
	SSL_CTX* ctx;

protected:
	TransferEncoding encType;
	U32 mBufferSize;
	U16 mPort;
	U8 *mBuffer;

protected:
	void(*cb_onRedirect)(TCPQuery*, const char* newUrl);
	void(*cb_onConnected)(TCPQuery*);
	void(*cb_onDNSResolved)(TCPQuery*);
	void(*cb_onDNSFailed)(TCPQuery*);
	void(*cb_onConnectFailed)(TCPQuery*);
	void(*cb_onDisconnected)(TCPQuery*);
	void(*cb_onHeader)(TCPQuery*, const char* headerName, const char* value);
	void(*cb_onLine)(TCPQuery*, const char* data);
	void(*cb_onRawData)(TCPQuery*, const char* data, U32 size);
	bool deleteOnDisconnect;
	bool doDisconnect;

public:
	bool gotHeaders;
	S32 contentLengthMax;
	S32 contentLength;
	S32 chunkedLeftOver;
	S32 statusResonse;
	
public:
	TCPQuery(
		void(*cba_onConnected)(TCPQuery*),
		void(*cba_onDNSResolved)(TCPQuery*),
		void(*cba_onDNSFailed)(TCPQuery*),
		void(*cba_onConnectFailed)(TCPQuery*),
		void(*cba_onDisconnected)(TCPQuery*),
		void(*cba_onLine)(TCPQuery*, const char* data),
		void(*cba_onRawData)(TCPQuery*, const char* data, U32 size),
		void(*cba_onHeader)(TCPQuery*, const char* headerName, const char* value),
		void(*cb_onRedirect)(TCPQuery*, const char* newUrl),
		bool deleteOnceDone = true);
	~TCPQuery();

	static void init();

	void parseLine(U8 *buffer, U32 *start, U32 bufferLen);
	void finishLastLine();

	static TCPQuery *find(NetSocket tag);

	// onReceive gets called continuously until all bytes are processed
	// return # of bytes processed each time.
	virtual U32 onReceive(U8 *buffer, U32 bufferLen); // process a buffer of raw packet data
	virtual bool processLine(U8 *line); // process a complete line of text... default action is to call into script
	virtual void onDNSResolved();
	virtual void onDNSFailed();
	virtual void onConnected();
	virtual void onConnectFailed();
	virtual void onConnectionRequest(const NetAddress *addr, U32 connectId);
	virtual void onDisconnect();
	void connect(const char *address);
	void listen(U16 port);
	void disconnect();
	State getState() { return mState; }

	void setOnDisconnect(void(*cba_onDisconnected)(TCPQuery*)) { cb_onDisconnected = cba_onDisconnected; }

#ifdef TCPQUERY_MULTI_THREAD
	bool isLocked();
	void lock();
	void unlock();

	bool wasDisconnectedByMain() { return doDisconnect; }
#endif

	bool processArguments(S32 argc, const char **argv);
	void send(const char *buffer);
	void addToTable(NetSocket newTag);
	void removeFromTable();

#ifdef TCPQUERY_USE_SSL
	int RecvPacket();
#endif

	void setPort(U16 port) { mPort = port; }
};

#endif  // _H_TCPOBJECT_
