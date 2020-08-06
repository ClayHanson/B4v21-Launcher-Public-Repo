//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "dgl/dgl.h"
#include "game/net/TCPBinaryDownload.h"

#include "gui/controls/guiChunkedBitmapCtrl.h"

IMPLEMENT_CONOBJECT(GuiChunkedBitmapCtrl);

GuiChunkedBitmapCtrl::GuiChunkedBitmapCtrl(void) {
	mBitmapName = StringTable->insert("");
	startPoint.set(0, 0);
	mWrap = false;
}

bool GuiChunkedBitmapCtrl::setBitmapName(void* obj, const char* data) {
	// Prior to this, you couldn't do bitmap.bitmap = "foo.jpg" and have it work.
	// With protected console types you can now call the setBitmap function and
	// make it load the image.
	static_cast<GuiChunkedBitmapCtrl*>(obj)->setBitmap(data);

	// Return false because the setBitmap method will assign 'mBitmapName' to the
	// argument we are specifying in the call.
	return false;
}

void GuiChunkedBitmapCtrl::initPersistFields() {
	Parent::initPersistFields();
	addGroup("Misc");
	addProtectedField("bitmap", TypeFilename, Offset(mBitmapName, GuiChunkedBitmapCtrl), &setBitmapName, &defaultProtectedGetFn, "");
	//addField("bitmap", TypeFilename, Offset(mBitmapName, GuiChunkedBitmapCtrl));
	addField("wrap", TypeBool, Offset(mWrap, GuiChunkedBitmapCtrl));
	endGroup("Misc");
}

ConsoleMethod(GuiChunkedBitmapCtrl, setValue, void, 4, 4, "(int xAxis, int yAxis)"
	"Set the offset of the bitmap.")
{
	object->setValue(dAtoi(argv[2]), dAtoi(argv[3]));
}

ConsoleMethod(GuiChunkedBitmapCtrl, setBitmap, void, 3, 3, "(string filename)"
	"Set the bitmap displayed in the control. Note that it is limited in size, to 256x256.")
{
	object->setBitmap(argv[2]);
}

bool GuiChunkedBitmapCtrl::onWake() {
	if (!Parent::onWake())
		return false;

	if (!mTextureHandle) {
		setActive(true);
		setBitmap(mBitmapName);
	}
	return true;
}

void GuiChunkedBitmapCtrl::onSleep() {
	if (!mAwake) mTextureHandle = NULL;
	Parent::onSleep();
}

//-------------------------------------
void GuiChunkedBitmapCtrl::inspectPostApply() {
	// if the extent is set to (0,0) in the gui editor and appy hit, this control will
	// set it's extent to be exactly the size of the bitmap (if present)
	Parent::inspectPostApply();

	if (!mWrap && (mBounds.extent.x == 0) && (mBounds.extent.y == 0) && mTextureHandle) {
		ChunkedTextureHandle* texture = (ChunkedTextureHandle*)&mTextureHandle;
		mBounds.extent.x = texture->getWidth();
		mBounds.extent.y = texture->getHeight();
	}
}

void GuiChunkedBitmapCtrl::setBitmap(const char* name, bool resize) {
	mBitmapName = StringTable->insert(name);
	if (*mBitmapName) {
		mTextureHandle = ChunkedTextureHandle(mBitmapName);

		// Resize the control to fit the bitmap
		if (resize) {
			ChunkedTextureHandle* texture = (ChunkedTextureHandle*)&mTextureHandle;
			mBounds.extent.x = texture->getWidth();
			mBounds.extent.y = texture->getHeight();
			Point2I extent = getParent()->getExtent();
			parentResized(extent, extent);
		}
	}
	else
		mTextureHandle = NULL;
	setUpdate();
}

void GuiChunkedBitmapCtrl::setBitmap(const ChunkedTextureHandle& handle, bool resize) {
	mTextureHandle = handle;

	// Resize the control to fit the bitmap
	if (resize) {
		ChunkedTextureHandle* texture = (ChunkedTextureHandle*)&mTextureHandle;
		mBounds.extent.x = texture->getWidth();
		mBounds.extent.y = texture->getHeight();
		Point2I extent = getParent()->getExtent();
		parentResized(extent, extent);
	}
}

void GuiChunkedBitmapCtrl::onRender(Point2I offset, const RectI& updateRect) {
	if (mTextureHandle) {
		dglClearBitmapModulation();
		if (mWrap) {
			ChunkedTextureHandle* texture = (ChunkedTextureHandle*)&mTextureHandle;
			RectI srcRegion;
			RectI dstRegion;
			float xdone = ((float)mBounds.extent.x / (float)texture->getWidth()) + 1;
			float ydone = ((float)mBounds.extent.y / (float)texture->getHeight()) + 1;

			int xshift = startPoint.x % texture->getWidth();
			int yshift = startPoint.y % texture->getHeight();
			for (int y = 0; y < ydone; ++y)
				for (int x = 0; x < xdone; ++x) {
					srcRegion.set(0, 0, texture->getWidth(), texture->getHeight());
					dstRegion.set(((texture->getWidth() * x) + offset.x) - xshift,
						((texture->getHeight() * y) + offset.y) - yshift,
						texture->getWidth(),
						texture->getHeight());
					dglDrawBitmapStretchSR(texture->getSubTexture(0, 0), dstRegion, srcRegion, false);
				}
		}
		else {
			RectI rect(offset, mBounds.extent);
			dglDrawBitmapStretch(mTextureHandle.getSubTexture(0, 0), rect);
		}
	}

	if (mProfile->mBorder || !mTextureHandle) {
		RectI rect(offset.x, offset.y, mBounds.extent.x, mBounds.extent.y);
		dglDrawRect(rect, mProfile->mBorderColor);
	}

	renderChildControls(offset, updateRect);
}

void GuiChunkedBitmapCtrl::setValue(S32 x, S32 y) {
	if (mTextureHandle) {
		ChunkedTextureHandle* texture = (ChunkedTextureHandle*)&mTextureHandle;
		x += texture->getWidth() / 2;
		y += texture->getHeight() / 2;
	}
	while (x < 0)
		x += 256;
	startPoint.x = x % 256;

	while (y < 0)
		y += 256;
	startPoint.y = y % 256;
}