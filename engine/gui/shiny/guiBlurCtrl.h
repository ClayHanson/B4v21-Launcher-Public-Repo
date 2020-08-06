#ifndef	_GUI_BLUR_H_
#define _GUI_BLUR_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#include "dgl/dgl.h"
#include "console/console.h"
#include "console/consoleTypes.h"

class GuiBlurCtrl : public GuiControl {
	typedef GuiControl Parent;

private:
	TextureHandle mTextureObject;
	GBitmap* mBlurBitmap;
	Point2I mLastPassPos;
	bool mPassedOnce;
	S32 mBlurAmount;
	U32 mLastUpdateTime;
	U32 mBlurBufferSize;
	U8* mBlurBuffer;

public:
	GuiBlurCtrl();

	virtual void onPreRender();
	virtual void onRender(Point2I offset, const RectI& updateRect);

	static void initPersistFields();
	DECLARE_CONOBJECT(GuiBlurCtrl);
};

#endif