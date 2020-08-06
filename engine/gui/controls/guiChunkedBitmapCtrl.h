//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUICHUNKEDBITMAPCTRL_H_
#define _GUICHUNKEDBITMAPCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif
#ifndef _GTEXMANAGER_H_
#include "dgl/gTexManager.h"
#endif

#include "dgl/gChunkedTexManager.h"

/// Renders a bitmap.
class GuiChunkedBitmapCtrl : public GuiControl
{
private:
	typedef GuiControl Parent;

protected:
	static bool setBitmapName(void* obj, const char* data);
	static const char* getBitmapName(void* obj, const char* data);

	StringTableEntry mBitmapName;
	ChunkedTextureHandle mTextureHandle;
	Point2I startPoint;
	bool mWrap;

public:
	//creation methods
	DECLARE_CONOBJECT(GuiChunkedBitmapCtrl);
	GuiChunkedBitmapCtrl();
	static void initPersistFields();

	//Parental methods
	bool onWake();
	void onSleep();
	void inspectPostApply();

	void setDownloadProgress(S32 curr, S32 max);
	void setBitmap(const char* name, bool resize = false);
	void setBitmap(const ChunkedTextureHandle& handle, bool resize = false);

	ChunkedTextureHandle* getBitmap() { return &mTextureHandle; }

	S32 getWidth() const { return(mTextureHandle.getWidth()); }
	S32 getHeight() const { return(mTextureHandle.getHeight()); }

	void onRender(Point2I offset, const RectI& updateRect);
	void setValue(S32 x, S32 y);
};

#endif
