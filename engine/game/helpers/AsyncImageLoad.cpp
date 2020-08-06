#include "game/helpers/AsyncImageLoad.h"

AsyncImageLoad::~AsyncImageLoad()
{
	if (!mThread)
		Sim::cancelEvent(mEventId);
}

AsyncImageLoad* AsyncImageLoad::load(const char* fileName, void* userData)
{
	if (!Platform::isFile(fileName))
	{
		Con::errorf("Failed to find image \"%s\"!", fileName);
		return NULL;
	}

	// Create it
	AsyncImageLoad* ret = new AsyncImageLoad();

	ret->mFileName = StringTable->insert(fileName);
	ret->mBitmap   = NULL;
	ret->mUserData = userData;
	ret->mThread   = NULL;

	// Create the 'start' event
	SimEngineEvent* eve = new SimEngineEvent([](void* uData)
	{
		static_cast<AsyncImageLoad*>(uData)->start();
	},
	ret);

	ret->mEventId = Sim::postEvent(Sim::getRootGroup(), eve, Sim::getCurrentTime() + 1);

	// Done!
	return ret;
}

void AsyncImageLoad::start()
{
	if (mThread)
		return;

	if (GBitmap::findBmpResource(mFileName) != NULL)
	{
		// Already found it!
		onDoneLoading.Invoke(3, this, true, mUserData);
		delete this;
		return;
	}

	mThread = new Thread(ASYNC_LOAD_IMAGE, (S32)this, true);
}

void ASYNC_LOAD_IMAGE(S32 uData)
{
	// Get the data
	AsyncImageLoad* data = (AsyncImageLoad*)uData;

	// Load the image
	char ext[128];
	const char* ptr = dStrrchr((const char*)data->mFileName, '.');
	if (!ptr)
	{
		data->onDoneLoading.Invoke(3, data, 0, data->mUserData);
		delete data;
		return;
	}

	// Get the file extension
	FileStream s;
	if (!s.open(data->mFileName, FileStream::AccessMode::Read))
	{
		data->onDoneLoading.Invoke(3, data, 0, data->mUserData);
		delete data;
		return;
	}

	// Create the bitmap
	data->mBitmap = new GBitmap();

	// Load it by extension
	if (!dStricmp(ext, "jpg") || !dStricmp(ext, "jpeg"))
	{
		if (!data->mBitmap->readJPEG(s))
		{
			data->onDoneLoading.Invoke(3, data, 0, data->mUserData);
			delete data;
			return;
		}
	}
	else if (!dStricmp(ext, "png"))
	{
		if (!data->mBitmap->readPNG(s))
		{
			data->onDoneLoading.Invoke(3, data, 0, data->mUserData);
			delete data;
			return;
		}
	}
	else if (!dStricmp(ext, "bmp"))
	{
		if (!data->mBitmap->readMSBmp(s))
		{
			data->onDoneLoading.Invoke(3, data, 0, data->mUserData);
			delete data;
			return;
		}
	}

	// Close it
	s.close();

	// Done!
	data->onDoneLoading.Invoke(3, data, 1, data->mUserData);
	delete data;
}