//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (c) 2002 GarageGames.Com
//-----------------------------------------------------------------------------

#include "gui/html/guiBitmapArrayCtrl.h"
#include "gui/utility/BitmapArray.h"
#include "console/consoleTypes.h"
#include "gui/core/guiControl.h"
#include "dgl/dgl.h"

/// Renders a skinned border.
class GuiBitmapBorderCtrl : public GuiBitmapArrayCtrl
{
	typedef GuiBitmapArrayCtrl Parent;

	enum {
		BorderTopLeft,
		BorderTopRight,
		BorderTop,
		BorderLeft,
		BorderRight,
		BorderBottomLeft,
		BorderBottom,
		BorderBottomRight,
		NumBitmaps
	};
public:
	void onRender(Point2I offset, const RectI& updateRect);
	DECLARE_CONOBJECT(GuiBitmapBorderCtrl);
};

IMPLEMENT_CONOBJECT(GuiBitmapBorderCtrl);

void GuiBitmapBorderCtrl::onRender(Point2I offset, const RectI& updateRect)
{
	if (!mArray.get())
	{
		renderChildControls(offset, updateRect);
		return;
	}

	// Update the array
	mArray.update();

	// Stop here if there is an insufficient amount of rects!
	if (mArray.size() < NumBitmaps)
	{
		renderChildControls(offset, updateRect);
		return;
	}

	dglSetClipRect(updateRect);
	TextureHandle handle = mArray.get();

	//draw the outline
	RectI winRect;
	winRect.point = offset;
	winRect.extent = mBounds.extent;

	winRect.point.x += mArray[BorderLeft].extent.x;
	winRect.point.y += mArray[BorderTop].extent.y;

	winRect.extent.x -= mArray[BorderLeft].extent.x + mArray[BorderRight].extent.x;
	winRect.extent.y -= mArray[BorderTop].extent.y + mArray[BorderBottom].extent.y;

	//if(mProfile->mOpaque)
	dglDrawRectFill(winRect, mBitmapColor);

	dglClearBitmapModulation();
	dglSetBitmapModulation(mBitmapColor);
	dglDrawBitmapSR(handle, offset, mArray[BorderTopLeft]);
	dglDrawBitmapSR(handle, Point2I(offset.x + mBounds.extent.x - mArray[BorderTopRight].extent.x, offset.y),
		mArray[BorderTopRight]);

	RectI destRect;
	destRect.point.x = offset.x + mArray[BorderTopLeft].extent.x;
	destRect.point.y = offset.y;
	destRect.extent.x = mBounds.extent.x - mArray[BorderTopLeft].extent.x - mArray[BorderTopRight].extent.x;
	destRect.extent.y = mArray[BorderTop].extent.y;
	RectI stretchRect = mArray[BorderTop];
	stretchRect.inset(1, 0);
	dglDrawBitmapStretchSR(handle, destRect, stretchRect);

	destRect.point.x = offset.x;
	destRect.point.y = offset.y + mArray[BorderTopLeft].extent.y;
	destRect.extent.x = mArray[BorderLeft].extent.x;
	destRect.extent.y = mBounds.extent.y - mArray[BorderTopLeft].extent.y - mArray[BorderBottomLeft].extent.y;
	stretchRect = mArray[BorderLeft];
	stretchRect.inset(0, 1);
	dglDrawBitmapStretchSR(handle, destRect, stretchRect);

	destRect.point.x = offset.x + mBounds.extent.x - mArray[BorderRight].extent.x;
	destRect.extent.x = mArray[BorderRight].extent.x;
	destRect.point.y = offset.y + mArray[BorderTopRight].extent.y;
	destRect.extent.y = mBounds.extent.y - mArray[BorderTopRight].extent.y - mArray[BorderBottomRight].extent.y;

	stretchRect = mArray[BorderRight];
	stretchRect.inset(0, 1);
	dglDrawBitmapStretchSR(handle, destRect, stretchRect);

	dglDrawBitmapSR(handle, offset + Point2I(0, mBounds.extent.y - mArray[BorderBottomLeft].extent.y), mArray[BorderBottomLeft]);
	dglDrawBitmapSR(handle, offset + mBounds.extent - mArray[BorderBottomRight].extent, mArray[BorderBottomRight]);

	destRect.point.x = offset.x + mArray[BorderBottomLeft].extent.x;
	destRect.extent.x = mBounds.extent.x - mArray[BorderBottomLeft].extent.x - mArray[BorderBottomRight].extent.x;

	destRect.point.y = offset.y + mBounds.extent.y - mArray[BorderBottom].extent.y;
	destRect.extent.y = mArray[BorderBottom].extent.y;
	stretchRect = mArray[BorderBottom];
	stretchRect.inset(1, 0);

	dglDrawBitmapStretchSR(handle, destRect, stretchRect);
	dglClearBitmapModulation();

	renderChildControls(offset, updateRect);
}