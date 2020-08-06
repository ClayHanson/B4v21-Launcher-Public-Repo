//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _GUIBACKGROUNDCTRL_H_
#define _GUIBACKGROUNDCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif
#include "gui/utility/TimerUtil.h"
#include "gui/utility/BitmapArray.h"
#include "gui/utility/GuiString.h"

/// Renders a background, so you can have a backdrop for your GUI.
class GuiContextMenuCtrl;
class GuiBackgroundCtrl : public GuiControl
{
public:
	typedef GuiControl Parent;
public:
	static void _onselect(void* uData, U32 argc, char* argv);

	struct NavButton
	{
		typedef ColorI(*BMP_COLOR_FUNC)(GuiBackgroundCtrl*);
		typedef bool(*VISIBILITY_FUNC)(GuiBackgroundCtrl*);
		typedef void(*EXECUTE_FUNC)(GuiBackgroundCtrl*);

		U32 iconIdx;
		BMP_COLOR_FUNC getBitmapModColor;
		VISIBILITY_FUNC isVisible;
		EXECUTE_FUNC invoke;

		// Calculation
		Point2I imageOffset;
		RectI rect;

		NavButton(U32 idx, BMP_COLOR_FUNC bmpColorPtr, VISIBILITY_FUNC visiblePtr, EXECUTE_FUNC execPtr)
		{
			iconIdx           = idx;
			getBitmapModColor = bmpColorPtr;
			isVisible         = visiblePtr;
			invoke            = execPtr;
			rect              = RectI(0, 0, 0, 0);
			imageOffset       = Point2I(0, 0);
		}
	};

	BitmapArray mIconArray;
	NavButton* mNavButtons;
	NavButton* mNavHoverBtn;
	Point2I mNavTextPos;
	bool mNavWasTransitioning;
	char* mNavText;
	RectI mNavRc;
	RectI mNavTextRc;

	Point2F mNavImageScalar;
	Point2I mNavImageOffset;
	Point2I mNavImageDragStart;
	Point2I mNavImageOffsetDragStart;
	bool mNavDragImage;

	GuiCursor* mDefaultCursor;
	GuiCursor* mMoveCursor;
	
	GuiContextMenuCtrl* mContextMenu;

protected:
	static bool setBitmapName(void *obj, const char *data);
	static const char *getBitmapName(void *obj, const char *data);

	struct BGSlide
	{
		TextureHandle bitmapIcon;
		U16 index;
		char* mBitmapName;
		TextureHandle mTextureHandle;
		Point2I bmpRes;
		BGSlide* last;
		BGSlide* next;
		bool defaultImage;
	};

	struct DefaultState
	{
		StringTableEntry bmpName;
		Point2I imgOffset;
		Point2F imgScalar;
		bool autoPlay;
		bool shuffle;
		bool loaded;

		DefaultState()
		{
			bmpName   = NULL;
			imgOffset = Point2I(0, 0);
			imgScalar = Point2F(1.f, 1.f);
			autoPlay  = false;
			shuffle   = false;
			loaded    = false;
		}
	};

	struct DirectoryMenuState
	{
		friend class GuiBackgroundCtrl;
		GuiBackgroundCtrl* parent;
		TimerUtil animTimer;
		U32 scroll;
		bool is_open;
		bool visible;
		bool drag_scroll;

		struct SlideItem
		{
			BGSlide* slide;
			GuiString text[2];
			RectI itemRect;
			RectI imageRect;
		};

		Vector<SlideItem> slides;

		// Cache stuff
		ColorF closeIconColor;
		ColorF backgroundHLColor;
		ColorF backgroundColor;
		ColorF borderColor;
		ColorF blackColor;

		// Cache rects
		Point2I closeIconPoint;
		RectI scrollHandleRect;
		RectI scrollContentRect;
		RectI closeButtonRect;
		RectI scrollBarRect;
		RectI menuRect;

		// Cache states
		U32 scrollHandleDragOffset;
		U32 scrollHandleMaxYOff;
		U32 scrollHandleMaxYPos;
		U32 scrollHandleMinYPos;
		U32 maxScroll;
		SlideItem* hoverItem;
		U32 firstShown;
		U32 lastShown;
		bool closeHover;
		U32 contentHeight;

		// Init
		DirectoryMenuState(GuiBackgroundCtrl* us);

		// Methods
		void open();
		void close(bool immediate = false);
		void render(Point2I& offset, const RectI& updateRect);
		void setScroll(U32 amt);
		void onMouseDown(Point2I point);
		void onMouseUp(Point2I point);
		void onMouseDragged(Point2I point);
		void onMouseMove(Point2I point);
		void onWheelScroll(S32 amt, Point2I point);
		SlideItem* getHitscan(Point2I pnt);
		void calculate();
	};

	DirectoryMenuState mDirectoryMenu;
	DefaultState mDefaultState;
	Vector<BGSlide*> defaultSlides;
	Vector<BGSlide*> slides;
	SimTime timer;
	SimTime delta;
	BGSlide* currentSlide;
	BGSlide* nextSlide;
	RectI rect;
	U32 transitionTime;
	U32 transitionDelay;
	U32 imageDisplayTimer;
	U32 doubleClickTimer;
	bool mDisallowDrag;
	bool mWindowRelative;
	bool mShowMem;
	bool mShuffle;
	bool mPlay;

	TextureHandle mLeftHandle;
	TextureHandle mRightHandle;

	void linkSlides();
	bool readyHandle(BGSlide* slide);

public:
	//creation methods
	DECLARE_CONOBJECT(GuiBackgroundCtrl);
	GuiBackgroundCtrl();
	~GuiBackgroundCtrl();
	static void initPersistFields();

	inline U32 getSlideCount() { return slides.size(); }

	//Slide management
	TextureHandle getBitmapIcon(BGSlide* slide, Point2I size, bool load = true);
	inline void clampBackground();
	RectI getRenderRect(BGSlide* slide, RectI originalRect, bool applyOffset = true, bool applyScalar = true);
	S32 getCurrent();
	void clearSlides();
	void removeSlide(BGSlide* slide);
	void addSlide(const char* fileName);
	void addDirectory(const char* directory);
	void setNext(U32 index, bool forceNow = false);
	void dump();

	// Methods
	bool isTransitioning() { return nextSlide != NULL; }
	bool isPlaying() { return mPlay; }
	bool isShuffled() { return mShuffle; }
	void openDirectory();
	void setShuffle(bool val);
	void setPlay(bool val);

	void closeNavMenu();

	// Nav buttons
	void renderNavButtons(Point2I offset, const RectI& updateRect);
	NavButton* hitScanNavMenu(Point2I pnt);
	void calculateNavMenu();

	//Parental methods
	bool onWake();
	void onSleep();

	virtual void resize(const Point2I& newPosition, const Point2I& newExtent);
	virtual void onMouseUp(const GuiEvent& event);
	virtual void onMouseDown(const GuiEvent& event);
	virtual void onMouseMove(const GuiEvent& event);
	virtual void onMouseEnter(const GuiEvent& event);
	virtual void onMouseLeave(const GuiEvent& event);
	virtual void onMouseDragged(const GuiEvent& event);
	virtual bool onMouseWheelUp(const GuiEvent& event);
	virtual bool onMouseWheelDown(const GuiEvent& event);

	virtual GuiControl* findHitControl(const Point2I& pt, S32 initialLayer = -1);

	void saveConfig();
	void loadConfig();
	void applyDefaultState();
	void onContextMenuSelected(S32 rowID, const char* rowText);
	virtual void onRightMouseDown(const GuiEvent& event);

	bool initCursors();
	void getCursor(GuiCursor*& cursor, bool& showCursor, const GuiEvent& lastGuiEvent);

	void onRender(Point2I offset, const RectI &updateRect);
};

#endif
