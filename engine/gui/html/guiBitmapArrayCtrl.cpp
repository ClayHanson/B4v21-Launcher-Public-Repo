#include "gui/html/guiBitmapArrayCtrl.h"
#include "console/consoleTypes.h"
#include "dgl/dgl.h"

IMPLEMENT_CONOBJECT(GuiBitmapArrayCtrl);

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool GuiBitmapArrayCtrl::setBitmapName(void* obj, const char* data)
{
	static_cast<GuiBitmapArrayCtrl*>(obj)->SetBitmap(data);
	return false;
}

bool GuiBitmapArrayCtrl::setBitmapIndex(void* obj, const char* data)
{
	static_cast<GuiBitmapArrayCtrl*>(obj)->SetArrayIndex(dAtoi(data));
	return false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiBitmapArrayCtrl::GuiBitmapArrayCtrl() : mArray()
{
	mBitmapName      = StringTable->insert("");
	mBitmapSize      = Point2I(0, 0);
	mBitmapArraySize = 0;
	mArrayIndex      = 0;
	mSpin            = 0;
	mFlip            = false;
	mStretch         = true;
	mDownloading     = false;
	mBitmapColor     = ColorI(255, 255, 255, 255);
}

GuiBitmapArrayCtrl::~GuiBitmapArrayCtrl()
{
	mArray.set((const char*)NULL);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiBitmapArrayCtrl::initPersistFields()
{
	Parent::initPersistFields();
	addGroup("Image Properties");
	addProtectedField("bitmap", TypeFilename, Offset(mBitmapName, GuiBitmapArrayCtrl), &setBitmapName, &defaultProtectedGetFn, "");
	addProtectedField("index", TypeS32, Offset(mArrayIndex, GuiBitmapArrayCtrl), &setBitmapIndex, &defaultProtectedGetFn, "");
	endGroup("Image Properties");

	addGroup("Image Manipulation");
	addField("stretch", TypeBool, Offset(mStretch, GuiBitmapArrayCtrl));
	addField("flip", TypeBool, Offset(mFlip, GuiBitmapArrayCtrl));
	addField("color", TypeColorI, Offset(mBitmapColor, GuiBitmapArrayCtrl));
	addField("spin", TypeF32, Offset(mSpin, GuiBitmapArrayCtrl));
	endGroup("Image Manipulation");
}

//Parental methods
bool GuiBitmapArrayCtrl::onWake()
{
	if (!Parent::onWake())
		return false;

	if (mBitmapName && *mBitmapName)
	{
		setActive(true);
		SetBitmap(mBitmapName);
	}

	return true;
}

void GuiBitmapArrayCtrl::onSleep()
{
	if (!mAwake)
		mArray.set((const char*)NULL);

	Parent::onSleep();
}

void GuiBitmapArrayCtrl::onRender(Point2I offset, const RectI& updateRect)
{
	RectI drawRect(offset, mBounds.extent);

	// Render image
	if (*mBitmapName)
	{
		// Update the array
		mArray.update();

		// Setup bitmap modulation
		dglSetBitmapModulation(mBitmapColor);

		// Draw it
		if (!mStretch || mBounds.extent == mBitmapSize)
			dglDrawBitmapSR(mArray.get(), offset, mArray[mArrayIndex], mFlip);
		else
			dglDrawBitmapStretchSR(mArray.get(), drawRect, mArray[mArrayIndex], mFlip, mSpin);

		// Clear modulation
		dglClearBitmapModulation();
	}

	// Draw children
	renderChildControls(offset, updateRect);
}

void GuiBitmapArrayCtrl::preRender()
{
	if (!*mBitmapName || !mDownloading || mArray.get()->downloading)
		return;
	
	// If we're done downloading, then force our bitmap array to recalculate
	mArray.get();
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiBitmapArrayCtrl::SetArrayIndex(S32 index)
{
	if (!*mBitmapName)
		return;
		
	// Set it!
	mArrayIndex = mClamp(index, 0, mBitmapArraySize);
}

bool GuiBitmapArrayCtrl::SetBitmap(TextureObject* texture, bool resize)
{
	mBitmapName = texture == NULL ? NULL : texture->texFileName;

	// Setup everything!
	if (*mBitmapName)
	{
		if (!mArray.set(mBitmapName) || !mArray.get())
		{
			mBitmapArraySize = 0;
			mBitmapSize      = Point2I(0, 0);
			mBitmapName      = StringTable->insert("");
			return false;
		}

		// Cache
		mBitmapArraySize = mArray.size();
		mBitmapSize      = Point2I(mArray.get()->texWidth, mArray.get()->texHeight);
		mDownloading     = mArray.get()->downloading;

		// Resize the control to fit the bitmap
		if (resize)
		{
			TextureObject* texture = mArray.get();
			mBounds.extent.x       = texture->bitmapWidth;
			mBounds.extent.y       = texture->bitmapHeight;
			Point2I extent         = getParent()->getExtent();

			parentResized(extent, extent);
		}

		if (!mAwake)
			mArray.set((const char*)NULL);
	}
	else
	{
		mBitmapArraySize = 0;
		mBitmapSize      = Point2I(0, 0);
		mBitmapName      = StringTable->insert("");
		mArray.set((const char*)NULL);
	}

	SetArrayIndex(mArrayIndex);
	setUpdate();
	return true;
}

bool GuiBitmapArrayCtrl::SetBitmap(const char* fileName, bool resize)
{
	mBitmapName = StringTable->insert(fileName);

	// Setup everything!
	if (*mBitmapName)
	{
		if (!mArray.set(mBitmapName) || !mArray.get())
		{
			mBitmapArraySize = 0;
			mBitmapSize      = Point2I(0, 0);
			mBitmapName      = StringTable->insert("");
			return false;
		}

		// Cache
		mBitmapArraySize = mArray.size();
		mBitmapSize      = Point2I(mArray.get()->texWidth, mArray.get()->texHeight);
		mDownloading     = mArray.get()->downloading;

		// Resize the control to fit the bitmap
		if (resize)
		{
			TextureObject* texture = mArray.get();
			mBounds.extent.x       = texture->bitmapWidth;
			mBounds.extent.y       = texture->bitmapHeight;
			Point2I extent         = getParent()->getExtent();

			parentResized(extent, extent);
		}

		if (!mAwake)
			mArray.set((const char*)NULL);
	}
	else
	{
		mBitmapArraySize = 0;
		mBitmapSize      = Point2I(0, 0);
		mBitmapName      = StringTable->insert("");
		mArray.set((const char*)NULL);
	}

	SetArrayIndex(mArrayIndex);
	setUpdate();
	return true;
}

S32 GuiBitmapArrayCtrl::GetWidth()
{
	return mBitmapSize.x;
}

S32 GuiBitmapArrayCtrl::GetHeight()
{
	return mBitmapSize.y;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ConsoleMethod(GuiBitmapArrayCtrl, setArrayIndex, void, 3, 3, "(int index)")
{
	object->SetArrayIndex(dAtoi(argv[2]));
}

ConsoleMethod(GuiBitmapArrayCtrl, setBitmap, bool, 3, 4, "(string bitmapName[, bool resize])")
{
	return object->SetBitmap(argv[2], argc == 4 ? dAtob(argv[3]) : false);
}

ConsoleMethod(GuiBitmapArrayCtrl, getBitmapWidth, S32, 2, 2, "")
{
	return object->GetWidth();
}

ConsoleMethod(GuiBitmapArrayCtrl, getBitmapHeight, S32, 2, 2, "")
{
	return object->GetHeight();
}