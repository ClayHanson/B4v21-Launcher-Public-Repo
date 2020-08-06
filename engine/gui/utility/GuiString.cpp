#include "gui/utility/GuiString.h"
#include "dgl/gFont.h"
#include "dgl/dgl.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiString::GuiString()
{
	mStringWidth = 0;
	mFont        = NULL;
	mString      = NULL;
	mDrawRect    = RectI(0, 0, 0, 0);
	mHidden      = false;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiString::update()
{
	// Update the width
	if (mString && mFont)
		mStringWidth = mFont->getStrWidthPrecise((const UTF8*)mString);

	// Update the scroll amount
	if (mString && mFont)
		mScroll.amount = mStringWidth - mDrawRect.extent.x;
	
	// Update cache
	mCache.will_scroll = (mStringWidth > mDrawRect.extent.x);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiString::setVisible(bool visible)
{
	mHidden = !visible;
}

void GuiString::setFont(const GFont* newFont)
{
	// Set our font pointer to the new font
	mFont = (GFont*)newFont;

	// Update stuff
	update();
}

void GuiString::setText(const char* str)
{
	if (!str)
	{
		mString      = NULL;
		mStringWidth = 0;
		return;
	}

	// Set our string pointer to the new string
	mString = StringTable->insert(str);

	// Update stuff
	update();
}

void GuiString::setRect(RectI newRect)
{
	mDrawRect = newRect;

	// Update stuff
	update();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiString::drawText(bool allowScroll)
{
	if (mHidden)
		return;

	AssertFatal(mFont && mString, "GuiString::drawText() - mFont or mString is NULL!");
	S32 time = Sim::getCurrentTime() - mScroll.timer;

	// Advance an act if we pass our delay
	if (time >= SCROLL_DELAY)
	{
		mScroll.timer = Sim::getCurrentTime();
		mScroll.act   = (mScroll.act + 1) % 4;
		time          = Sim::getCurrentTime() - mScroll.timer;
	}

	// If we're NOT wider than the draw rect, then just draw as normal.
	if (!mCache.will_scroll || !allowScroll)
	{
		dglDrawText(mFont, mDrawRect.point, mString);
		return;
	}
	
	// Set our new clip rect
	RectI oldCR = dglGetClipRect();
	dglSetClipRect(mDrawRect);

	// Calculate a few other things
	U32 scrollAmt = mStringWidth - mDrawRect.extent.x;

	// Determine what to do at each act
	switch (mScroll.act)
	{
		case 0: // Wait
		{
			dglDrawText(mFont, mDrawRect.point, mString);
			break;
		}
		case 1: // Scroll to the left
		{
			S32 xOff = S32((F32)scrollAmt * F32((F32)time / (F32)SCROLL_DELAY));
			dglDrawText(mFont, Point2I(mDrawRect.point.x - xOff, mDrawRect.point.y), mString);
			break;
		}
		case 2: // Wait
		{
			dglDrawText(mFont, Point2I(mDrawRect.point.x - scrollAmt, mDrawRect.point.y), mString);
			break;
		}
		case 3: // Scroll to the right
		{
			S32 xOff = S32((F32)scrollAmt * F32((F32)time / (F32)SCROLL_DELAY));
			dglDrawText(mFont, Point2I(mDrawRect.point.x - (scrollAmt - xOff), mDrawRect.point.y), mString);
			break;
		}
	}

	// Reset our clip rect
	dglSetClipRect(oldCR);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------