#include "gui/utility/guiCache.h"
#include "gui/core/guiControl.h"
#include "dgl/gBitmap.h"
#include "dgl/dgl.h"

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiCache::GuiCache(GuiControl* parent)
{
	mControl   = parent;
	mTexHandle = NULL;
	mBitmap    = NULL;
	mRendering = false;

	mTexName = new char[dStrlen(parent->getIdString()) + dStrlen("_TEX") + 1];
	dStrcpy(mTexName, parent->getIdString());
	dStrcat(mTexName, "_TEX");
}

GuiCache::~GuiCache()
{
	if (mTexHandle)
		mTexHandle = NULL;

	if (mBitmap)
		delete mBitmap;

	clearRecords();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool GuiCache::RecordEntry::IsDifferent()
{
	return dMemcmp(ptr, value, size);
}

void GuiCache::RecordEntry::UpdateValue()
{
	dMemcpy(value, ptr, size);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool GuiCache::recordVariable(void* ptr, U32 size)
{
	if (size == 0 || size >= sizeof(RecordEntry::value))
	{
		Con::errorf("GuiCache::recordVariable() - Invalid variable size %d", size);
		return false;
	}

	RecordEntry* entry = NULL;
	for (Vector<RecordEntry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		entry = *it;
		if (entry->ptr != ptr)
			continue;

		Con::warnf("GuiCache::recordVariable() - Pointer 0x%08p is already being recorded!", ptr);
		return true;
	}

	// Add this pointer to the list
	entry       = new RecordEntry();
	entry->ptr  = ptr;
	entry->size = size;

	// Init
	entry->UpdateValue();

	// Add to the list
	mList.push_back(entry);
	return true;
}

void GuiCache::clearRecords()
{
	// Free all entries
	for (Vector<RecordEntry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		RecordEntry* entry = *it;
		delete entry;
	}

	// Clear it
	mList.clear();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiCache::updateBitmap()
{
	if (mBitmap)
	{
		delete mBitmap;
		mBitmap = NULL;
	}

	// Render it
	mRendering = true;
	RectI updateRect = RectI(mControl->localToGlobalCoord(mControl->mBounds.point), mControl->mBounds.extent);
	dglDrawRectFill(updateRect, ColorF(0.f, 1.f, 0.f, 1.f));
	mControl->onRender(mControl->localToGlobalCoord(mControl->mBounds.point), updateRect);
	mRendering = false;

	// Read the buffer
	glReadBuffer(GL_FRONT);

	// Get everything
	Point2I position = mControl->localToGlobalCoord(mControl->mBounds.point);
	Point2I extent   = mControl->mBounds.extent;
	U8* pixels       = new U8[extent.x * extent.y * 3];
	glReadPixels(position.x, position.y, extent.x, extent.y, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	mBitmap = new GBitmap;
	mBitmap->allocateBitmap(U32(extent.x), U32(extent.y));

	// flip the rows
	for (U32 y = 0; y < extent.y; y++)
		dMemcpy(mBitmap->getAddress(0, extent.y - y - 1), pixels + y * extent.x * 3, U32(extent.x * 3));

	// Free the pixels' buffer
	delete[] pixels;

	// Create the texture handle!
	mTexHandle = TextureHandle(mTexName, mBitmap, true);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiCache::preRender()
{
	// Check to see if an update is necessary
	bool needsUpdate = mBitmap == NULL;
	if (!needsUpdate)
	{
		for (Vector<RecordEntry*>::iterator it = mList.begin(); it != mList.end(); it++)
		{
			RecordEntry* entry = *it;
			if (!entry->IsDifferent())
				continue;

			// Update this entry's value
			entry->UpdateValue();

			// Update every entry after this' value
			for (; it != mList.end(); it++)
				((RecordEntry*)(*it))->UpdateValue();

			// Found an update factor!
			needsUpdate = true;
			break;
		}
	}

	if (needsUpdate)
		updateBitmap();
}

void GuiCache::render(Point2I& offset, const RectI& updateRect)
{
	if (!mTexHandle)
		return;

	// Draw the bitmap
	dglDrawBitmap(mTexHandle, offset);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------