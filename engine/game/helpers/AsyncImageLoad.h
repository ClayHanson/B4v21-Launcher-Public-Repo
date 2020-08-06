#ifndef _ASYNC_IMAGE_LOAD_
#define _ASYNC_IMAGE_LOAD_

#include "game/helpers/CallbackEvent.h"
#include "platform/platformThread.h"
#include "dgl/gBitmap.h"

void ASYNC_LOAD_IMAGE(S32 uData);
struct AsyncImageLoad
{
public:
	// (const AsyncImageLoad* info, bool success)
	CallbackEvent onDoneLoading;
	StringTableEntry mFileName;
	GBitmap* mBitmap;
	void* mUserData;

private:
	Thread* mThread;
	U32 mEventId;

public:
	~AsyncImageLoad();
	static AsyncImageLoad* load(const char* fileName, void* userData = NULL);

public:
	void start();
};

#endif