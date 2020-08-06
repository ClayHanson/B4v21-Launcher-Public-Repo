#include "game/helpers/CallbackEvent.h"
#include "platform/platform.h"

#include <stdarg.h>

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

CallbackEvent::CallbackEvent()
{
	mActionList  = NULL;
	mActionCount = 0;
}

CallbackEvent::~CallbackEvent()
{
	if (mActionList != NULL)
	{
		for (U32 i = 0; i < mActionCount; i++)
			delete mActionList[i];

		dFree(mActionList);
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool CallbackEvent::ResizeList(U32 newSize)
{
	// If we're already at that size, then there is nothing to do here.
	if (mActionCount == newSize)
		return true;

	// Delete everything?
	if (newSize == 0)
	{
		// Free every entry
		for (U32 i = 0; i < mActionCount; i++)
			delete mActionList[i];

		// Free the list itself
		dFree(mActionList);

		// Reset allocation information
		mActionList  = NULL;
		mActionCount = 0;

		// Done!
		return true;
	}

	// If this is our first allocation, then use a different alloc function.
	if (mActionList == NULL)
	{
		// Create the list
		mActionList = (EventAction**)dMalloc(sizeof(EventAction*) * newSize);

		// Allocation error!
		if (mActionList == NULL)
		{
			AssertFatal(false, avar("FATAL ERROR: CallbackEvent::ResizeList() - Failed to allocate %d pointer%s!", newSize, newSize == 1 ? "" : "s"));
			return false;
		}

		// Create new entries
		for (U32 i = 0; i < newSize; i++)
			mActionList[i] = new EventAction;

		// Set the size
		mActionCount = newSize;

		// Done!
		return true;
	}

	// Delete removed entries
	if (newSize < mActionCount)
		for (U32 i = 0; i < mActionCount - newSize; i++)
			delete mActionList[(mActionCount - (mActionCount - newSize)) + i];

	// Resize the list
	mActionList = (EventAction**)dRealloc((void*)mActionList, sizeof(EventAction*) * newSize);

	// Allocation error!
	if (mActionList == NULL)
	{
		AssertFatal(false, avar("FATAL ERROR: CallbackEvent::ResizeList() - Failed to re-allocate %d pointer%s!", newSize, newSize == 1 ? "" : "s"));
		return false;
	}

	// Create new entries
	if (newSize > mActionCount)
		for (U32 i = 0; i < newSize - mActionCount; i++)
			mActionList[mActionCount + i] = new EventAction;

	// Set the size
	mActionCount = newSize;

	// Done!
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

CallbackEvent::EventAction* CallbackEvent::GetListener(U32 index)
{
	return mActionList == NULL ? NULL : mActionList[index % mActionCount];
}

U32 CallbackEvent::GetListenerCount()
{
	return mActionCount;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool CallbackEvent::AddListener(EventFunction funcPtr, void* userData)
{
	// Sanity check
	AssertFatal(funcPtr != NULL, "FATAL ERROR: CallbackEvent::AddListener() - Attempted to add a NULL listener!");

	// Check to see if this function pointer is already in the list
	for (int i = 0; i < mActionCount; i++)
	{
		if (mActionList[i]->funcPtr != funcPtr)
			continue;

		// Stop here, as we have found the function pointer in our list.
		AssertWarn(false, "WARNING: CallbackEvent::AddListener() - Attempted to add a duplicate listener!");
		return false;
	}

	// Resize the list to account for the new entry
	if (!ResizeList(mActionCount + 1))
		return false;

	// Insert it into the list
	mActionList[mActionCount - 1]->funcPtr  = funcPtr;
	mActionList[mActionCount - 1]->userData = userData;

	// Done!
	return true;
}

void CallbackEvent::RemoveListener(EventFunction funcPtr)
{
	// Sanity check
	AssertFatal(funcPtr != NULL, "FATAL ERROR: CallbackEvent::RemoveListener() - Attempted to remove a NULL listener!");

	// Check to see if this function pointer is already in the list
	for (int i = 0; i < mActionCount; i++)
	{
		if (mActionList[i]->funcPtr != funcPtr)
			continue;

		// Plaster over it
		dMemmove(mActionList + i, mActionList + i + 1, sizeof(EventFunction) * ((mActionCount - i) - 1));

		// Resize the action list
		ResizeList(mActionCount - 1);

		// Stop here, as we have found the function pointer in our list.
		return;
	}

	// Couldn't find it...?
	AssertWarn(false, "WARNING: CallbackEvent::RemoveListener() - Couldn't find function pointer 0x%08x in our action list!");
}

void CallbackEvent::ClearListeners()
{
	if (mActionList == NULL)
		return;

	// Free it
	dFree(mActionList);

	// Reset
	mActionList  = NULL;
	mActionCount = 0;
}

void CallbackEvent::Invoke(U32 argc, ...)
{
	if (mActionList == NULL)
		return;

	va_list vArgs;

	// Invoke the functions
	for (U32 i = 0; i < mActionCount; i++)
	{
		// Open a new listing
		va_start(vArgs, argc);

		// Invoke it
		mActionList[i]->funcPtr(mActionList[i]->userData, argc, (char*)vArgs);

		// Close this listing
		va_end(vArgs);
	}
}

void CallbackEvent::Invoke(U32 argc, const char** argv)
{
	if (mActionList == NULL)
		return;

	// Invoke the functions
	for (U32 i = 0; i < mActionCount; i++)
	{
		// Invoke it
		mActionList[i]->funcPtr(mActionList[i]->userData, argc, (char*)(*argv));
	}
}