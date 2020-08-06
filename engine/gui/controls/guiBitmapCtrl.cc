//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "dgl/dgl.h"
#include "game/net/TCPBinaryDownload.h"

#include "gui/controls/guiBitmapCtrl.h"

IMPLEMENT_CONOBJECT(GuiBitmapCtrl);

GuiBitmapCtrl::GuiBitmapCtrl(void) {
	mBitmapName = StringTable->insert("");
	startPoint.set(0, 0);
	mWrap = false;
}

bool GuiBitmapCtrl::setBitmapName(void *obj, const char *data) {
	// Prior to this, you couldn't do bitmap.bitmap = "foo.jpg" and have it work.
	// With protected console types you can now call the setBitmap function and
	// make it load the image.
	static_cast<GuiBitmapCtrl *>(obj)->setBitmap(data);

	// Return false because the setBitmap method will assign 'mBitmapName' to the
	// argument we are specifying in the call.
	return false;
}

void GuiBitmapCtrl::initPersistFields() {
	Parent::initPersistFields();
	addGroup("Misc");
	addProtectedField("bitmap", TypeFilename, Offset(mBitmapName, GuiBitmapCtrl), &setBitmapName, &defaultProtectedGetFn, "");
	//addField("bitmap", TypeFilename, Offset(mBitmapName, GuiBitmapCtrl));
	addField("wrap", TypeBool, Offset(mWrap, GuiBitmapCtrl));
	endGroup("Misc");
}

ConsoleMethod( GuiBitmapCtrl, setValue, void, 4, 4, "(int xAxis, int yAxis)"
              "Set the offset of the bitmap.")
{
   object->setValue(dAtoi(argv[2]), dAtoi(argv[3]));
}

ConsoleMethod( GuiBitmapCtrl, setBitmap, void, 3, 3, "(string filename)"
              "Set the bitmap displayed in the control. Note that it is limited in size, to 256x256.")
{
   object->setBitmap(argv[2]);
}

bool GuiBitmapCtrl::onWake() {
	if (!Parent::onWake())
		return false;

	if (!mTextureHandle) {
		setActive(true);
		setBitmap(mBitmapName);
	}
	return true;
}

void GuiBitmapCtrl::onSleep() {
	if(!mAwake) mTextureHandle = NULL;
	Parent::onSleep();
}

//-------------------------------------
void GuiBitmapCtrl::inspectPostApply() {
	// if the extent is set to (0,0) in the gui editor and appy hit, this control will
	// set it's extent to be exactly the size of the bitmap (if present)
	Parent::inspectPostApply();

	if (!mWrap && (mBounds.extent.x == 0) && (mBounds.extent.y == 0) && mTextureHandle) {
		TextureObject *texture = (TextureObject *)mTextureHandle;
		mBounds.extent.x = texture->bitmapWidth;
		mBounds.extent.y = texture->bitmapHeight;
	}
}

void GuiBitmapCtrl::setBitmap(const char *name, bool resize) {
	mBitmapName = StringTable->insert(name);
	if (*mBitmapName) {
		mTextureHandle = TextureHandle(mBitmapName, BitmapTexture, true);

		// Resize the control to fit the bitmap
		if (resize) {
			TextureObject* texture = (TextureObject *)mTextureHandle;
			mBounds.extent.x = texture->bitmapWidth;
			mBounds.extent.y = texture->bitmapHeight;
			Point2I extent = getParent()->getExtent();
			parentResized(extent, extent);
		}
	}
	else
		mTextureHandle = NULL;
	setUpdate();
}

void GuiBitmapCtrl::setBitmap(const TextureHandle &handle, bool resize) {
	mTextureHandle = handle;

	// Resize the control to fit the bitmap
	if (resize) {
		TextureObject* texture = (TextureObject *)mTextureHandle;
		mBounds.extent.x = texture->bitmapWidth;
		mBounds.extent.y = texture->bitmapHeight;
		Point2I extent = getParent()->getExtent();
		parentResized(extent, extent);
	}
}

void GuiBitmapCtrl::onRender(Point2I offset, const RectI &updateRect) {
	if (mTextureHandle) {
		dglClearBitmapModulation();
		if (mWrap) {
			TextureObject* texture = (TextureObject *)mTextureHandle;
			RectI srcRegion;
			RectI dstRegion;
			float xdone = ((float)mBounds.extent.x / (float)texture->bitmapWidth) + 1;
			float ydone = ((float)mBounds.extent.y / (float)texture->bitmapHeight) + 1;

			int xshift = startPoint.x%texture->bitmapWidth;
			int yshift = startPoint.y%texture->bitmapHeight;
			for (int y = 0; y < ydone; ++y)
				for (int x = 0; x < xdone; ++x) {
					srcRegion.set(0, 0, texture->bitmapWidth, texture->bitmapHeight);
					dstRegion.set(((texture->bitmapWidth*x) + offset.x) - xshift,
						((texture->bitmapHeight*y) + offset.y) - yshift,
						texture->bitmapWidth,
						texture->bitmapHeight);
					dglDrawBitmapStretchSR(texture, dstRegion, srcRegion, false);
				}
		} else {
			RectI rect(offset, mBounds.extent);
			dglDrawBitmapStretch(mTextureHandle, rect);
		}
	}

	if (mProfile->mBorder || !mTextureHandle) {
		RectI rect(offset.x, offset.y, mBounds.extent.x, mBounds.extent.y);
		dglDrawRect(rect, mProfile->mBorderColor);
	}

	renderChildControls(offset, updateRect);
}

void GuiBitmapCtrl::setValue(S32 x, S32 y) {
	if (mTextureHandle) {
		TextureObject* texture = (TextureObject *)mTextureHandle;
		x += texture->bitmapWidth / 2;
		y += texture->bitmapHeight / 2;
	}
	while (x < 0)
		x += 256;
	startPoint.x = x % 256;

	while (y < 0)
		y += 256;
	startPoint.y = y % 256;
}