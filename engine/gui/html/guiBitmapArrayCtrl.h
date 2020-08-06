#ifndef _GUI_BITMAP_ARRAY_CTRL_
#define _GUI_BITMAP_ARRAY_CTRL_

#include "gui/utility/BitmapArray.h"
#include "gui/core/guiControl.h"
#include "console/console.h"

class GuiBitmapArrayCtrl : public GuiControl
{
	typedef GuiControl Parent;
protected: // Protected methods
	static bool setBitmapName(void* obj, const char* data);
	static bool setBitmapIndex(void* obj, const char* data);

protected: // Variables
	StringTableEntry mBitmapName;
	BitmapArray mArray;
	S32 mArrayIndex;
	F32 mSpin;

	bool mFlip;
	bool mStretch;
	bool mDownloading;

	ColorI mBitmapColor;

protected: // Cache
	Point2I mBitmapSize;
	U32 mBitmapArraySize;

public: // Base methods
	GuiBitmapArrayCtrl();
	~GuiBitmapArrayCtrl();

public: // Inherited methods
	static void initPersistFields();

	//Parental methods
	bool onWake();
	void onSleep();
	void onRender(Point2I offset, const RectI& updateRect);
	void preRender();

public: // Custom methods
	void SetArrayIndex(S32 index);
	bool SetBitmap(TextureObject* texture, bool resize = false);
	bool SetBitmap(const char* fileName, bool resize = false);

	S32 GetWidth();
	S32 GetHeight();

public:
	DECLARE_CONOBJECT(GuiBitmapArrayCtrl);
};


#endif