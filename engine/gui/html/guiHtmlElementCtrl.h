#ifndef _GUI_HTML_ELEMENT_CTRL_
#define _GUI_HTML_ELEMENT_CTRL_

#include "platform/platform.h"
#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"

class CssProperties : SimObject
{
	typedef SimObject Parent;

public: // Enums


public: // Variables
	StringTableEntry mElementID; // #<ID name here> {
	StringTableEntry mElementClassName; // .<class name here> {

public: // Properties
	struct CSS_Border
	{
		struct
		{
			S32 left;
			ColorI leftColor;

			S32 top;
			ColorI topColor;

			S32 right;
			ColorI rightColor;

			S32 bottom;
			ColorI bottomColor;
		};

		enum CSSE_BORDER_STYLE
		{
			BORDER_NONE,
			BORDER_HIDDEN,
			BORDER_SOLID,
			BORDER_INHERIT,
			BORDER_INITIAL
		} style;

		CSS_Border()
		{
			color = ColorI(0, 0, 0, 255);
			width = 2;
			style = BORDER_NONE;
		}
	} mBorder;

	struct CSS_Background
	{
		enum CSSE_BACKGROUND_REPEAT
		{
			BGR_REPEAT,
			BGR_REPEAT_X,
			BGR_REPEAT_Y,
			BGR_NO_REPEAT,
			BGR_SPACE,
			BGR_ROUND,
			BGR_INITIAL,
			BGR_INHERIT
		} repeat;

		TextureHandle image;
		Point2I imagePos;
		Point2I imageSize;
		ColorI color;
	} mBackground;

public: // Other stuff
	DECLARE_CONOBJECT(CssProperties);

	CssProperties();
	~CssProperties();
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class GuiHtmlElementCtrl : GuiControl
{
	typedef GuiControl Parent;

private:
	DECLARE_CONOBJECT(GuiHtmlElementCtrl);

public:
	GuiHtmlElementCtrl();
	~GuiHtmlElementCtrl();

public:
};

#endif