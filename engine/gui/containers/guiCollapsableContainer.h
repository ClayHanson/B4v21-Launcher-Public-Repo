#ifndef _GUICOLLAPSABLECONTAINER_H_
#define _GUICOLLAPSABLECONTAINER_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

class GuiCollapsableContainer : public GuiControl
{
private:
	typedef GuiControl Parent;
public:
	DECLARE_CONOBJECT(GuiCollapsableContainer);
	static void initPersistFields();

	GuiCollapsableContainer();
	virtual ~GuiFrameSetCtrl();

	void addObject(SimObject *obj);
	void removeObject(SimObject *obj);

	virtual void resize(const Point2I &newPosition, const Point2I &newExtent);

	virtual void getCursor(GuiCursor *&cursor, bool &showCursor, const GuiEvent &lastGuiEvent);

	virtual void onMouseDown(const GuiEvent &event);
	virtual void onMouseUp(const GuiEvent &event);
	virtual void onMouseDragged(const GuiEvent &event);
	virtual void onMouseEnter(const GuiEvent &event);
	virtual void onMouseLeave(const GuiEvent &event);

	bool onAdd();
	void onRender(Point2I offset, const RectI &updateRect);
protected:
	/* member variables */
	Vector<S32> mColumnOffsets;
	Vector<S32> mRowOffsets;
	GuiCursor *mMoveCursor;
	GuiCursor *mUpDownCursor;
	GuiCursor *mLeftRightCursor;
	GuiCursor *mDefaultCursor;
	FrameDetail mFramesetDetails;
	VectorPtr<FrameDetail *> mFrameDetails;
	bool mAutoBalance;
	S32   mFudgeFactor;

	/* divider activation member variables */
	Region mCurHitRegion;
	Point2I mLocOnDivider;
	S32 mCurVerticalHit;
	S32 mCurHorizontalHit;

	bool init(U32 columns, U32 rows, const U32 columnOffsets[], const U32 rowOffsets[]);
	bool initCursors();

	Region findHitRegion(const Point2I &point);
	Region pointInAnyRegion(const Point2I &point);
	S32 findResizableFrames(S32 indexes[]);
	bool hitVerticalDivider(S32 x, const Point2I &point);
	bool hitHorizontalDivider(S32 y, const Point2I &point);

	void rebalance(const Point2I &newExtent);

	void computeSizes(bool balanceFrames = false);
	void computeMovableRange(Region hitRegion, S32 vertHit, S32 horzHit, S32 numIndexes, const S32 indexes[], S32 ranges[]);

	void drawDividers(const Point2I &offset);
public:
	U32 columns() const { return(mColumnOffsets.size()); }
	U32 rows() const { return(mRowOffsets.size()); }
	U32 borderWidth() const { return(mFramesetDetails.mBorderWidth); }
	Vector<S32>* columnOffsets() { return(&mColumnOffsets); }
	Vector<S32>* rowOffsets() { return(&mRowOffsets); }
	FrameDetail* framesetDetails() { return(&mFramesetDetails); }

	bool findFrameContents(S32 index, GuiControl **gc, FrameDetail **fd);

	void frameBorderEnable(S32 index, const char *state = NULL);
	void frameBorderMovable(S32 index, const char *state = NULL);
	void frameMinExtent(S32 index, const Point2I &extent);

	void balanceFrames() { computeSizes(true); }
	void updateSizes() { computeSizes(); }

	bool onWake();

private:
	DISABLE_COPY_CTOR(GuiFrameSetCtrl);
	DISABLE_ASSIGNMENT(GuiFrameSetCtrl);
};

//-----------------------------------------------------------------------------
// x is the first value inside the next column, so the divider x-coords
// precede x.
inline bool GuiFrameSetCtrl::hitVerticalDivider(S32 x, const Point2I &point)
{
	return((point.x >= S32(x - mFramesetDetails.mBorderWidth)) && (point.x < x) && (point.y >= 0) && (point.y < S32(mBounds.extent.y)));
}

//-----------------------------------------------------------------------------
// y is the first value inside the next row, so the divider y-coords precede y.
inline bool GuiFrameSetCtrl::hitHorizontalDivider(S32 y, const Point2I &point)
{
	return((point.x >= 0) && (point.x < S32(mBounds.extent.x)) && (point.y >= S32(y - mFramesetDetails.mBorderWidth)) && (point.y < y));
}

#endif // _GUI_FRAME_CTRL_H
