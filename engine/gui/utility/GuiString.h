#ifndef _GUI_STRING_H_
#define _gUI_STRING_H_

#define SCROLL_DELAY 2000

#include "console/consoleTypes.h"
#include "math/mMath.h"

class GFont;
class GuiString
{
protected: // Base variables
	StringTableEntry mString;
	U32 mStringWidth;
	RectI mDrawRect;
	GFont* mFont;
	bool mHidden;

protected: // Cache variables
	struct CacheState
	{
		bool will_scroll;

		CacheState()
		{
			will_scroll = false;
		}
	} mCache;
	
protected: // Scrolling variables
	struct ScrollState
	{
		U32 timer;
		U32 amount;
		U8 act;

		ScrollState()
		{
			timer  = 0;
			amount = 0;
			act    = 0;
		}
	} mScroll;

public: // Base methods
	GuiString();

protected: // Protected methods
	void update();

public: // Set methods
	void setVisible(bool visible);
	void setFont(const GFont* newFont);
	void setText(const char* str);
	void setRect(RectI newRect);

public: // Get methods
	inline bool isVisible() { return !mHidden; }
	inline StringTableEntry getText() { return mString; }
	inline GFont* getFont() { return mFont; }
	inline RectI getDrawRect() { return mDrawRect; }
	inline U32 getStringWidth() { return mStringWidth; }

public: // Rendering
	void drawText(bool allowScroll = true);
};

#endif