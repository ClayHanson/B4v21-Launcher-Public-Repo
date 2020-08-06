//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/controls/guiBackgroundCtrl.h"
#include "gui/shiny/guiContextMenuCtrl.h"
#include "platform/platformVideo.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "console/console.h"
#include "dgl/dgl.h"

#define BACKGROUND_BUTTON_WIDTH 32
#define BACKGROUND_BUTTON_HEIGHT 64
#define FADE_TIME 400
#define EXPIRE_TIME 5000
#define CONFIG_HEADER "BGDT"
#define CONFIG_VERSION 1

IMPLEMENT_CONOBJECT(GuiBackgroundCtrl);

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define DIRM_WIDTH 630
#define DIRM_HEIGHT 460
#define DIRM_PAD 7
#define DIRM_FADE_TIME 400
#define DIRM_CLOSE_ICON_IDX 12

// Init
GuiBackgroundCtrl::DirectoryMenuState::DirectoryMenuState(GuiBackgroundCtrl* us) : animTimer()
{
	parent        = us;
	hoverItem     = NULL;
	contentHeight = 0;
	scroll        = 0;
	is_open       = false;
	closeHover    = false;
	visible       = false;
	drag_scroll   = false;
	firstShown    = 0;
	lastShown     = 0;

	scrollHandleMaxYPos = 0;
	scrollHandleMinYPos = 0;
}

// Methods
void GuiBackgroundCtrl::DirectoryMenuState::open()
{
	if (is_open || animTimer.isRunning())
		return;

	// Reset some variables
	closeHover = false;
	visible    = true;
	scroll     = 0;

	// Calculate rects
	calculate();

	// Start our timer
	animTimer.reset();
	animTimer.start();
}

void GuiBackgroundCtrl::DirectoryMenuState::close(bool immediate)
{
	if (immediate)
	{
		// Force it to close
		animTimer.stop();
		is_open = false;
		visible = false;

		return;
	}

	if (!is_open || animTimer.isRunning())
		return;

	// Start our timer
	animTimer.reset();
	animTimer.start(true);
}

void GuiBackgroundCtrl::DirectoryMenuState::render(Point2I& offset, const RectI& updateRect)
{
	F32 amount = 1.f;

	// Do the animation
	if (animTimer.isRunning() && (amount = animTimer.getAmount(DIRM_FADE_TIME)) == animTimer.getEndValue())
	{
		// Timer is done
		animTimer.stop();

		if (animTimer.isInverse())
		{
			visible = false;
			is_open = false;

			// Reset all texture handles
			for (Vector<SlideItem>::iterator it = slides.begin(); it != slides.end(); it++)
				((SlideItem)(*it)).slide->bitmapIcon = NULL;

			return;
		}
		else
			is_open = true;
	}

	// Draw the background & border
	dglDrawRectFill(menuRect, animTimer.getColorValue(backgroundColor, DIRM_FADE_TIME));
	dglDrawRect(menuRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));

	// Draw the close button
	if (closeHover)
		dglDrawRectFill(closeButtonRect, animTimer.getColorValue(backgroundHLColor, DIRM_FADE_TIME));

	// Draw the icon
	dglSetBitmapModulation(animTimer.getColorValue(closeIconColor, DIRM_FADE_TIME));
	dglDrawBitmapSR(parent->mIconArray.get(), offset + closeIconPoint, parent->mIconArray[DIRM_CLOSE_ICON_IDX]);

	// Draw the close button's border
	dglDrawRect(closeButtonRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));

	RectI clipRc(dglGetClipRect());
	dglSetClipRect(scrollContentRect);

	// Draw content
	for (Vector<SlideItem>::iterator it = slides.begin() + firstShown; it != slides.begin() + lastShown; it++)
	{
		SlideItem* item = &(*it);
		if (item->itemRect.point.y > (scrollContentRect.point.y + scrollContentRect.extent.y))
			break;

		if (item->itemRect.point.y + item->itemRect.extent.y < scrollContentRect.point.y)
			continue;

		// Draw selection
		if (hoverItem == item)
			dglDrawRectFill(item->itemRect, ColorI(255, 255, 255, 100));

		// Draw the item's border
		dglDrawRect(item->itemRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));

		// Draw the item image background
		dglDrawRectFill(item->imageRect, animTimer.getColorValue(blackColor, DIRM_FADE_TIME));

		// Draw the item's image
		TextureObject* imgTO = parent->getBitmapIcon(item->slide, item->imageRect.extent, !drag_scroll);
		if (imgTO)
			dglDrawBitmap(imgTO, item->imageRect.point);

		// Draw the item image frame
		dglDrawRect(item->imageRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));

		// Draw the item's text
		for (S32 i = 0; i < sizeof(SlideItem::text) / sizeof(GuiString); i++)
			item->text[i].drawText();
	}

	// Reset clip rect
	dglSetClipRect(clipRc);

	// Draw the scroll bar handle
	dglDrawRectFill(scrollHandleRect, animTimer.getColorValue(ColorI(255, 0, 0, 255), DIRM_FADE_TIME));

	// Draw the scroll bar's border
	dglDrawRect(scrollBarRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));

	// Draw the scroll content's border
	dglDrawRect(scrollContentRect, animTimer.getColorValue(borderColor, DIRM_FADE_TIME));
}

void GuiBackgroundCtrl::DirectoryMenuState::setScroll(U32 amt)
{
	U32 oldScroll            = scroll;
	scroll                   = mClamp(amt, 0, maxScroll);
	scrollHandleRect.point.y = (scrollBarRect.point.y + S32((F32)scrollHandleMaxYOff * F32((F32)scroll / (F32)maxScroll))) + (scrollHandleRect.point.x - scrollBarRect.point.x);

	// Update firstShown & lastShown
	firstShown = 0xFFFFFFFF;
	lastShown  = 0xFFFFFFFF;
	for (Vector<SlideItem>::iterator it = slides.begin(); it != slides.end(); it++)
	{
		SlideItem* item = &(*it);

		// Offset by scroll
		item->itemRect.point.y  -= scroll - oldScroll;
		item->imageRect.point.y -= scroll - oldScroll;

		// Offset text
		for (S32 i = 0; i < sizeof(SlideItem::text) / sizeof(GuiString); i++)
		{
			RectI oldRect(item->text[i].getDrawRect());
			oldRect.point.y -= scroll - oldScroll;

			item->text[i].setVisible(oldRect.point.y + oldRect.extent.y >= scrollContentRect.point.y && oldRect.point.y < scrollContentRect.point.y + scrollContentRect.extent.y);
			item->text[i].setRect(oldRect);
		}

		if (lastShown != 0xFFFFFFFF)
			continue;

		if (item->itemRect.point.y > (scrollContentRect.point.y + scrollContentRect.extent.y))
		{
			lastShown = it - slides.begin();
			continue;
		}

		if (item->itemRect.point.y + item->itemRect.extent.y < scrollContentRect.point.y)
			continue;

		if (firstShown == 0xFFFFFFFF)
			firstShown = it - slides.begin();
	}

	if (lastShown == 0xFFFFFFFF)
		lastShown = slides.size();

	// Don't advance if we're scrolling
	if (drag_scroll)
		return;

	// Calculate the slides' bitmaps
	for (Vector<SlideItem>::iterator it = slides.begin() + firstShown; it != slides.begin() + lastShown; it++)
	{
		SlideItem* slide = &*it;
		if (slide->slide->bmpRes != Point2I(-1, -1))
			continue;

		// Calculate the icon while we're at it
		parent->getBitmapIcon(slide->slide, slide->imageRect.extent, true);

		// Set the new text
		slide->text[1].setText(avar("%d%s%d", slide->slide->bmpRes.x, "x", slide->slide->bmpRes.y));
	}
}

void GuiBackgroundCtrl::DirectoryMenuState::onMouseDown(Point2I point)
{
	if (!RectI(parent->localToGlobalCoord(scrollBarRect.point), scrollBarRect.extent).pointInRect(point))
	{
		if (!hoverItem)
			return;

		parent->setNext(hoverItem->slide->index, true);
		close();
		return;
	}

	if (RectI(parent->localToGlobalCoord(scrollHandleRect.point), scrollHandleRect.extent).pointInRect(point))
	{
		// Drag by the handle
		scrollHandleDragOffset = point.y - parent->localToGlobalCoord(scrollHandleRect.point).y;
		drag_scroll            = true;
		return;
	}

	// Scroll click
	if (point.y < parent->localToGlobalCoord(scrollHandleRect.point).y)
	{
		// Scroll up a bit
		setScroll(scroll - 252);
		return;
	}

	// Scroll down a bit
	setScroll(scroll + 252);
}

void GuiBackgroundCtrl::DirectoryMenuState::onMouseUp(Point2I point)
{
	if (drag_scroll)
	{
		// Stop dragging the scroll handle
		drag_scroll = false;
		setScroll(scroll);
		return;
	}

	if (RectI(parent->localToGlobalCoord(closeButtonRect.point), closeButtonRect.extent).pointInRect(point))
	{
		close();
		return;
	}
}

void GuiBackgroundCtrl::DirectoryMenuState::onMouseDragged(Point2I point)
{
	if (!drag_scroll)
		return;

	// Drag it!
	point.y -= scrollHandleDragOffset;

	U32 minY   = parent->localToGlobalCoord(Point2I(0, scrollHandleMinYPos)).y;
	U32 maxY   = parent->localToGlobalCoord(Point2I(0, scrollHandleMaxYPos)).y;
	F32 amount = (point.y < minY ? 0.f : (point.y >= maxY ? 1.f : F32(point.y - minY) / F32(maxY - minY)));

	setScroll(amount * (F32)maxScroll);
}

void GuiBackgroundCtrl::DirectoryMenuState::onMouseMove(Point2I point)
{
	closeHover = false;
	if (RectI(parent->localToGlobalCoord(closeButtonRect.point), closeButtonRect.extent).pointInRect(point))
	{
		closeHover = true;
		return;
	}

	if (scrollContentRect.pointInRect(point))
		hoverItem = getHitscan(point);
}

void GuiBackgroundCtrl::DirectoryMenuState::onWheelScroll(S32 amt, Point2I point)
{
	if (drag_scroll)
		return;

	bool up = (amt < 0);
	if (RectI(parent->localToGlobalCoord(scrollContentRect.point), scrollContentRect.extent).pointInRect(point))
		setScroll(scroll + (up ? -126 : 126));
}

GuiBackgroundCtrl::DirectoryMenuState::SlideItem* GuiBackgroundCtrl::DirectoryMenuState::getHitscan(Point2I pnt)
{
	for (Vector<SlideItem>::iterator it = slides.begin() + firstShown; it != slides.begin() + lastShown; it++)
	{
		SlideItem* slide = &(*it);

		// Ignore invalid rects
		if (!slide->itemRect.pointInRect(pnt))
			continue;

		// Hit it!
		return slide;
	}

	// Didn't hit anything
	return NULL;
}

void GuiBackgroundCtrl::DirectoryMenuState::calculate()
{
	// Populate the slides list
	slides.clear();
	slides.setSize(parent->slides.size());
	for (Vector<BGSlide*>::iterator it = parent->slides.begin(); it != parent->slides.end(); it++)
	{
		BGSlide* slide   = *it;
		SlideItem* sItem = &slides[it - parent->slides.begin()];

		// Init this SlideItem
		dMemset(&slides[it - parent->slides.begin()], 0, sizeof(SlideItem));

		sItem->slide     = slide;
		sItem->itemRect  = RectI(0, 0, 0, 0);
		sItem->imageRect = RectI(0, 0, 0, 0);

		// Init text stuff
		for (S32 i = 0; i < sizeof(SlideItem::text) / sizeof(GuiString); i++)
		{
			sItem->text[i].setRect(RectI(0, 0, 0, 0));
			sItem->text[i].setFont(parent->mProfile->mFont);
		}

		// Init actual text
		sItem->text[0].setText(slide->mBitmapName);
		sItem->text[1].setText(avar("%d%s%d", slide->bmpRes.x, "x", slide->bmpRes.y));
	}

	// Get important vars
	RectI pRect(parent->mBounds);

	// Get the close button icon (#12)
	RectI close_icon = parent->mIconArray[DIRM_CLOSE_ICON_IDX];

	// Calculate sizes
	menuRect          = RectI(0, 0, DIRM_WIDTH, DIRM_HEIGHT);
	closeButtonRect   = RectI(0, 0, close_icon.extent.x + (DIRM_PAD * 2), close_icon.extent.y + (DIRM_PAD * 2));
	scrollContentRect = RectI(0, 0, menuRect.extent.x - (DIRM_PAD * 2), menuRect.extent.y - (closeButtonRect.extent.y + (DIRM_PAD * 2)) - DIRM_PAD);
	scrollBarRect     = RectI(0, 0, 16, scrollContentRect.extent.y);

	// For local rects
	Point2I itemSize    = Point2I(scrollContentRect.extent.x - scrollBarRect.extent.x, 126);
	RectI itemImageRect = RectI(0, 0, itemSize.y - (DIRM_PAD * 2), itemSize.y - (DIRM_PAD * 2));

	// Calculate positions
	menuRect.point          = Point2I(pRect.point.x + ((pRect.extent.x / 2) - (menuRect.extent.x / 2)), pRect.point.y + ((pRect.extent.y / 2) - (menuRect.extent.y / 2)));
	closeButtonRect.point   = Point2I((menuRect.point.x + menuRect.extent.x) - (closeButtonRect.extent.x + DIRM_PAD), menuRect.point.y + DIRM_PAD);
	scrollContentRect.point = Point2I(menuRect.point.x + DIRM_PAD, closeButtonRect.point.y + closeButtonRect.extent.y + DIRM_PAD);
	scrollBarRect.point     = Point2I((scrollContentRect.point.x + scrollContentRect.extent.x) - scrollBarRect.extent.x, scrollContentRect.point.y);
	closeIconPoint          = Point2I(closeButtonRect.point.x + ((closeButtonRect.extent.x / 2) - (close_icon.extent.x / 2)), closeButtonRect.point.y + ((closeButtonRect.extent.y / 2) - (close_icon.extent.y / 2)));

	// Calculate offset positions
	itemImageRect.point = Point2I(itemSize.x - (itemImageRect.extent.x + DIRM_PAD), DIRM_PAD);

	// Calculate rects for items
	contentHeight = 0;
	for (Vector<SlideItem>::iterator it = slides.begin(); it != slides.end(); it++)
	{
		SlideItem* item = &(*it);
		item->itemRect  = RectI(scrollContentRect.point + Point2I(0, itemSize.y * (it - slides.begin())), itemSize);
		item->imageRect = RectI(item->itemRect.point + itemImageRect.point, itemImageRect.extent);
		contentHeight  += item->itemRect.extent.y;
	}

	// Calculate the scroll bar stuff
	scrollHandleRect = RectI(Point2I(0, 0), Point2I(scrollBarRect.extent.x - 4, scrollBarRect.extent.y));

	// Set the appropriate scrollbar height
	if (contentHeight > scrollContentRect.extent.y)
		scrollHandleRect.extent.y = scrollBarRect.extent.y - S32(F32(F32(contentHeight - scrollContentRect.extent.y) / (F32)scrollContentRect.extent.y) * (F32)scrollBarRect.extent.y);

	U32 scrollOffset = ((scrollBarRect.extent.x / 2) - (scrollHandleRect.extent.x / 2));
	if (contentHeight < scrollContentRect.extent.y)
		scrollHandleRect.extent.y = scrollBarRect.extent.y - (scrollOffset * 2);
	
	// Make sure it doesn't get too small
	if (scrollHandleRect.extent.y < 16)
		scrollHandleRect.extent.y = 16;

	// Calculate max scroll
	maxScroll = contentHeight - scrollContentRect.extent.y;

	scrollHandleRect.point.x = scrollBarRect.point.x + scrollOffset;
	scrollHandleRect.point.y = scrollBarRect.point.y + scrollOffset;
	scrollHandleMaxYOff      = (scrollBarRect.extent.y - scrollHandleRect.extent.y) - (scrollOffset * 2);
	scrollHandleMaxYPos      = scrollBarRect.point.y + scrollHandleMaxYOff;
	scrollHandleMinYPos      = scrollBarRect.point.y + scrollOffset;

	// Setup colors
	closeIconColor    = ColorF(255, 67, 67, 255);
	backgroundHLColor = ColorI(255, 255, 255, 100);
	backgroundColor   = ColorI(0, 0, 0, 200);
	borderColor       = ColorI(120, 120, 120, 255);
	blackColor        = ColorI(0, 0, 0, 255);

	/*
		Point2I closeIconPoint;
		RectI scrollHandleRect;
		RectI scrollContentRect;
		RectI closeButtonRect;
		RectI scrollBarRect;
		RectI menuRect;
	*/

	// Offset everything
	for (Vector<SlideItem>::iterator it = slides.begin(); it != slides.end(); it++)
	{
		SlideItem* slide = &(*it);

		// local to global conversion
		slide->imageRect.point = parent->localToGlobalCoord(((SlideItem*)(&*it))->imageRect.point);
		slide->itemRect.point  = parent->localToGlobalCoord(((SlideItem*)(&*it))->itemRect.point);

		// Calc freespace
		U32 freeSpace = (slide->imageRect.point.x - slide->itemRect.point.x) - (DIRM_PAD * 2);

		// convert text rects, too
		Point2I start(slide->itemRect.point + Point2I(DIRM_PAD, DIRM_PAD));
		for (S32 i = 0; i < sizeof(SlideItem::text) / sizeof(GuiString); i++)
		{
			GuiString* str = &slide->text[i];
			Point2I ext(freeSpace, str->getFont()->getHeight());

			// Set new rect
			str->setRect(RectI(start, ext));

			// Offset start
			start.y += str->getFont()->getHeight();
		}
	}

	scrollHandleMinYPos     = parent->localToGlobalCoord(Point2I(0, scrollHandleMinYPos)).y;
	scrollHandleMaxYPos     = parent->localToGlobalCoord(Point2I(0, scrollHandleMaxYPos)).y;
	scrollHandleRect.point  = parent->localToGlobalCoord(scrollHandleRect.point);
	scrollContentRect.point = parent->localToGlobalCoord(scrollContentRect.point);
	closeButtonRect.point   = parent->localToGlobalCoord(closeButtonRect.point);
	scrollBarRect.point     = parent->localToGlobalCoord(scrollBarRect.point);
	menuRect.point          = parent->localToGlobalCoord(menuRect.point);
	closeIconPoint          = parent->localToGlobalCoord(closeIconPoint);

	// Reset scroll
	setScroll(scroll);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiBackgroundCtrl::_onselect(void* uData, U32 argc, char* argList)
{
	S32 rowID           = CALLBACK_EVENT_ARG(S32);
	const char* rowText = CALLBACK_EVENT_ARG(const char*);

	static_cast<GuiBackgroundCtrl*>(uData)->onContextMenuSelected(rowID, rowText);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiBackgroundCtrl::GuiBackgroundCtrl() : rect(0, 0, 0, 0), mIconArray(), mDirectoryMenu(this)
{
	mContextMenu = NULL;
	transitionTime = 1400;
	transitionDelay = 3000;
	imageDisplayTimer = 0;
	doubleClickTimer = 0;
	timer = 0;
	mDisallowDrag = false;
	mNavDragImage = false;
	mWindowRelative = false;
	mPlay = true;
	mShuffle = false;
	currentSlide = NULL;
	mLeftHandle = NULL;
	mRightHandle = NULL;
	nextSlide = NULL;
	mNavHoverBtn = NULL;
	mNavText = NULL;
	mShowMem = false;
	mNavImageScalar = Point2F(1.f, 1.f);
	mNavImageOffset = Point2I(0, 0);
	mNavImageDragStart = Point2I(0, 0);
	mNavTextPos = Point2I(0, 0);
	mNavRc = RectI(0, 0, 0, 0);
	mNavTextRc = RectI(0, 0, 0, 0);
	mMoveCursor = NULL;
	mDefaultCursor = NULL;

	// Create the navigation buttons
	mNavButtons = new NavButton[8]
	{
		/* PREVIOUS BUTTON */
		{
			10,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 255, 255, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return true;
			},
			[](GuiBackgroundCtrl* us)
			{
				if (us->isTransitioning())
					return;

				if (us->getCurrent() == 0)
				{
					us->setNext(us->getSlideCount() - 1, true);
					return;
				}

				us->setNext(us->getCurrent() - 1, true);
			}
		},
		/* PLAY BUTTON */
		{
			7,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 255, 255, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return !us->isPlaying();
			},
			[](GuiBackgroundCtrl* us)
			{
				us->setPlay(true);
			}
		},
		/* PAUSE BUTTON */
		{
			6,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 255, 255, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return us->isPlaying();
			},
			[](GuiBackgroundCtrl* us)
			{
				us->setPlay(false);
			}
		},
		/* SHUFFLE BUTTON */
		{
			8,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return us->isShuffled() ? ColorI(255, 255, 255, 255) : ColorI(255, 255, 255, 100);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return true;
			},
			[](GuiBackgroundCtrl* us)
			{
				us->setShuffle(!us->isShuffled());
			}
		},
		/* NEXT BUTTON */
		{
			11,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 255, 255, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return true;
			},
			[](GuiBackgroundCtrl* us)
			{
				if (us->isTransitioning())
					return;

				if (us->getCurrent() == (us->getSlideCount() - 1))
				{
					us->setNext(0, true);
					return;
				}

				us->setNext(us->getCurrent() + 1, true);
			}
		},
		/* DIRECTORY BUTTON */
		{
			13,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 255, 255, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return true;
			},
			[](GuiBackgroundCtrl* us)
			{
				us->openDirectory();
			}
		},
		/* CLOSE BUTTON */
		{
			12,
			[](GuiBackgroundCtrl* us)->ColorI
			{
				return ColorI(255, 67, 67, 255);
			},
			[](GuiBackgroundCtrl* us)->bool
			{
				return true;
			},
			[](GuiBackgroundCtrl* us)
			{
				us->closeNavMenu();
			}
		},
		/* ENDER */
		{
			0x7FFFFFFF,
			NULL,
			NULL,
			NULL
		}
	};

	VECTOR_SET_ASSOCIATION(slides);
}

GuiBackgroundCtrl::~GuiBackgroundCtrl() {
	clearSlides();
	delete[] mNavButtons;

	if (mNavText)
		dFree(mNavText);

	if (mContextMenu)
		mContextMenu->deleteObject();
}

#define OVERLAP(a, b) ((a.point.x < b.point.x + b.extent.x && a.point.x >= b.point.x) || (a.point.y < b.point.y + b.extent.y && a.point.y >= b.point.y))

void GuiBackgroundCtrl::linkSlides() {
	for (Vector<BGSlide*>::iterator it = slides.begin(); it != slides.end(); it++) {
		BGSlide* slide = *it;

		// Setup slide pointers
		slide->index = it - slides.begin();
		slide->last  = (it == slides.begin() ? slides.back() : slides[(it - slides.begin()) - 1]);
		slide->next  = (it == slides.end() - 1 ? slides.front() : slides[(it - slides.begin()) + 1]);
	}
}

bool GuiBackgroundCtrl::readyHandle(BGSlide* slide) {
	// Setup our own handle
	slide->mTextureHandle = TextureHandle(slide->mBitmapName, BitmapTexture, true);

	// If it failed to load, remove it from the slides vector.
	if (!slide->mTextureHandle)
		return false;

	// Set the bmpRes if necessary
	if (slide->bmpRes == Point2I(-1, -1))
		slide->bmpRes = Point2I(slide->mTextureHandle.getWidth(), slide->mTextureHandle.getHeight());

	return true;
}

void GuiBackgroundCtrl::initPersistFields()
{
	Parent::initPersistFields();

	addField("transitionDelay", TypeS32, Offset(transitionDelay, GuiBackgroundCtrl));
	addField("transitionTime", TypeS32, Offset(transitionTime, GuiBackgroundCtrl));
	addField("shuffle", TypeBool, Offset(mShuffle, GuiBackgroundCtrl));
	addField("windowRelative", TypeBool, Offset(mWindowRelative, GuiBackgroundCtrl));
	addField("play", TypeBool, Offset(mPlay, GuiBackgroundCtrl));
	addField("showMem", TypeBool, Offset(mShowMem, GuiBackgroundCtrl));
}

bool GuiBackgroundCtrl::onWake()
{
	if (!Parent::onWake())
		return false;

	if (!initCursors())
	{
		Con::errorf("ERROR: Failed to load cursors!");
		return false;
	}

	// Load the configuration file
	rect = RectI(localToGlobalCoord(mBounds.point), mBounds.extent);
	loadConfig();

	mNavImageOffset = Point2I(0, 0);
	if (currentSlide)
		readyHandle(currentSlide);

	mIconArray.set("launcher/ui/icons.png");
	mLeftHandle = TextureHandle("launcher/ui/LeftBtn.png", BitmapTexture, true);
	mRightHandle = TextureHandle("launcher/ui/RightBtn.png", BitmapTexture, true);
	timer = Sim::getCurrentTime() + transitionTime;

	return true;
}

void GuiBackgroundCtrl::onSleep() {
	for (Vector<BGSlide*>::iterator it = slides.begin(); it != slides.end(); it++) {
		BGSlide* slide = *it;
		slide->mTextureHandle = NULL;
	}

	mIconArray.set((const char*)NULL);
	mLeftHandle = NULL;
	mRightHandle = NULL;
	mNavHoverBtn = NULL;

	mDirectoryMenu.close(true);

	Parent::onSleep();
}

TextureHandle GuiBackgroundCtrl::getBitmapIcon(BGSlide* slide, Point2I size, bool load)
{
	if (slide->bitmapIcon)
		return slide->bitmapIcon;

	if (!load)
		return 0;

	// Get the bitmap res
	if (slide->bmpRes == Point2I(-1, -1))
	{
		readyHandle(slide);
		slide->mTextureHandle = NULL;
	}

	// Ready some vars
	TextureHandle tObject = NULL;
	GBitmap* image         = NULL;
	GBitmap* iconBmp       = NULL;

	// Sanity check
	if (size.x > slide->bmpRes.x || size.y > slide->bmpRes.y)
	{
		Con::errorf("GuiBackgroundCtrl::getBitmapIcon() - Cannot stretch icon size past the original's size;");
		return 0;
	}

	if (!(tObject = TextureHandle(slide->mBitmapName, BitmapKeepTexture, true)).isValid())
	{
		Con::errorf("GuiBackgroundCtrl::getBitmapIcon() - Failed to open \"%s\"", slide->mBitmapName);
		tObject = NULL;
		return 0;
	}

	// It's not set; Load it
	if ((image = tObject.getBitmap()) == NULL)
	{
		Con::errorf("GuiBackgroundCtrl::getBitmapIcon() - Failed to get bitmap from \"%s\"", slide->mBitmapName);
		tObject = NULL;
		return 0;
	}

	// Create another bitmap for the scaled down version
	if ((iconBmp = new GBitmap(size.x, size.y, false, GBitmap::BitmapFormat::RGBA)) == NULL)
	{
		Con::errorf("GuiBackgroundCtrl::getBitmapIcon() - Failed to create a %dx%d bmp.", image->width, image->height);
		tObject = NULL;
		delete image;
		return 0;
	}

	// Get the copy rect
	RectI copyRect(0, 0, size.x, size.y);

	// Determine the best-looking copy rect
	if (image->width > image->height)
	{
		// WIDTH > HEIGHT
		// Left & right of image should touch edges
		copyRect.extent.y = (F32)image->height * F32((F32)size.x / (F32)image->width);
		copyRect.point.y  = (size.y / 2) - (copyRect.extent.y / 2);
	}
	else if (image->height > image->width)
	{
		// HEIGHT > WIDTH
		// Top & bottom of image should touch edges
		copyRect.extent.x = (F32)image->width * F32((F32)size.y / (F32)image->height);
		copyRect.point.x  = (size.x / 2) - (copyRect.extent.x / 2);
	}

	// Reset the bitmap
	dMemset(iconBmp->getAddress(0, 0), 0, iconBmp->byteSize);

	// Populate the bitmap
	U32 xskip = image->width / copyRect.extent.x;
	U32 yskip = image->height / copyRect.extent.y;
	
	// Scale it
	for (U32 i = 0; i < (copyRect.extent.x * copyRect.extent.y); i++)
	{
		U32 ToX   = (i % copyRect.extent.x);
		U32 ToY   = (i / copyRect.extent.x);
		U32 FromX = ToX * xskip;
		U32 FromY = ToY * yskip;
		ColorI col;

		// On error, break.
		if (!image->getColor(FromX, FromY, col))
			break;

		// Set the color
		iconBmp->setColor(copyRect.point.x + ToX, copyRect.point.y + ToY, col);
	}

	// Create the texture handle
	slide->bitmapIcon = TextureHandle(NULL, iconBmp, true);

	// Free the handle
	tObject = NULL;

	// Done!
	return slide->bitmapIcon;
}

void GuiBackgroundCtrl::clampBackground()
{
	// Clamp offset
	RectI OriginalRect(getRenderRect(currentSlide, rect, false, false));
	RectI OffsetRect(getRenderRect(currentSlide, rect, true, true));

	// Limit X
	if (OffsetRect.point.x > 0)
		mNavImageOffset.x = -OriginalRect.point.x;

	if (OffsetRect.point.x + OffsetRect.extent.x < Video::getResolution().w)
		mNavImageOffset.x = OriginalRect.point.x - (OffsetRect.extent.x - OriginalRect.extent.x);

	// Limit Y
	if (OffsetRect.point.y > 0)
		mNavImageOffset.y = -OriginalRect.point.y;

	if (OffsetRect.point.y + OffsetRect.extent.y < Video::getResolution().h)
		mNavImageOffset.y = OriginalRect.point.y - (OffsetRect.extent.y - OriginalRect.extent.y);
}

RectI GuiBackgroundCtrl::getRenderRect(BGSlide* slide, RectI originalRect, bool applyOffset, bool applyScalar)
{
	if (!slide) // || !slide->mTextureHandle)
		return originalRect;

	RectI newRect(originalRect);
	newRect.extent = slide->bmpRes;

	if (slide->bmpRes.x < slide->bmpRes.y)
	{
		newRect.extent.x = rect.extent.x;
		newRect.extent.y -= slide->bmpRes.x - newRect.extent.x;

		newRect.point.x = rect.point.x + (rect.extent.x / 2) - (newRect.extent.x / 2);
		newRect.point.y = rect.point.y + (rect.extent.y / 2) - (newRect.extent.y / 2);
	}
	else
	{
		newRect.extent.y = rect.extent.y;
		newRect.extent.x -= slide->bmpRes.y - newRect.extent.y;

		newRect.point.x = rect.point.x + (rect.extent.x / 2) - (newRect.extent.x / 2);
		newRect.point.y = rect.point.y + (rect.extent.y / 2) - (newRect.extent.y / 2);
	}

	newRect.extent.x = mAbs(newRect.extent.x);
	newRect.extent.y = mAbs(newRect.extent.y);

	if (newRect.extent.x < rect.extent.x) {
		// Image is narrower than the rect
		U32 smallerBy = rect.extent.x - newRect.extent.x;

		newRect.extent.x += smallerBy;
		newRect.extent.y += smallerBy;

		// Reposition
		newRect.point.x = rect.point.x + (rect.extent.x / 2) - (newRect.extent.x / 2);
		newRect.point.y = rect.point.y + (rect.extent.y / 2) - (newRect.extent.y / 2);
	}

	if (newRect.extent.y < rect.extent.y) {
		// Image is narrower than the rect
		U32 smallerBy = rect.extent.y - newRect.extent.y;

		newRect.extent.x += smallerBy;
		newRect.extent.y += smallerBy;

		// Reposition
		newRect.point.x = rect.point.x + (rect.extent.x / 2) - (newRect.extent.x / 2);
		newRect.point.y = rect.point.y + (rect.extent.y / 2) - (newRect.extent.y / 2);
	}

	if (applyScalar && slide == currentSlide && mNavImageScalar != Point2F(1.f, 1.f))
		newRect.extent = Point2I((F32)newRect.extent.x * mNavImageScalar.x, (F32)newRect.extent.y * mNavImageScalar.y);

	if (applyOffset && slide == currentSlide && mNavImageOffset != Point2I(0, 0))
		newRect.point += mNavImageOffset;

	return newRect;

	/*
	RectI newRect(originalRect);
	newRect.point.x += (newRect.extent.x / 2) - (slide->bmpRes.x / 2);
	newRect.point.y += (newRect.extent.y / 2) - (slide->bmpRes.y / 2);
	newRect.extent   = slide->bmpRes;

	// Sanity check
	if (newRect.point.x > originalRect.point.x) newRect.point.x = originalRect.point.x;
	if (newRect.point.y > originalRect.point.y) newRect.point.y = originalRect.point.y;
	if (newRect.point.x + newRect.extent.x < originalRect.point.x + originalRect.extent.x) newRect.extent.x -= newRect.extent.x - originalRect.extent.x;
	if (newRect.point.y + newRect.extent.y < originalRect.point.y + originalRect.extent.y) newRect.extent.y -= newRect.extent.y - originalRect.extent.y;

	// Make SURE it's centered
	newRect.point.x += (originalRect.extent.x / 2) - (newRect.extent.x / 2);
	newRect.point.y += (originalRect.extent.y / 2) - (newRect.extent.y / 2);

	// Neat zoom-in
	if (mPlay && slides.size() != 1) {
		// Only do this if there are more than one slide
		U8 zoom = U8((F64(Sim::getCurrentTime() - timer) / F64(transitionTime + transitionDelay)) * 70);

		newRect.point.x -= zoom;
		newRect.extent.x += zoom * 2;
		newRect.point.y -= zoom;
		newRect.extent.y += zoom * 2;
	}

	return newRect;
	*/
}

S32 GuiBackgroundCtrl::getCurrent()
{
	return currentSlide ? currentSlide->index : -1;
}

void GuiBackgroundCtrl::clearSlides() {
	for (Vector<BGSlide*>::iterator it = slides.begin(); it != slides.end(); it++)
	{
		BGSlide* slide        = *it;
		slide->bitmapIcon     = NULL;
		slide->mTextureHandle = NULL;
		dFree(slide->mBitmapName);
		delete slide;
	}

	slides.clear();
	defaultSlides.clear();

	nextSlide    = NULL;
	currentSlide = NULL;

	// Reset directory listing
	if (mDirectoryMenu.is_open)
		mDirectoryMenu.calculate();
}

void GuiBackgroundCtrl::removeSlide(BGSlide* slide)
{
	// Erase the slide
	slides.erase(slides.begin() + slide->index);

	// Re-link slides
	if (slide->next)
		slide->next->last = slide->last;
	if (slide->last)
		slide->last->next = slide->next;

	for (int i = slide->index; i < slides.size(); i++)
		slides[i]->index--;

	// If this is the current slide, remove it
	if (currentSlide == slide)
		currentSlide = (slide->next == slide ? NULL : slide->next);

	if (nextSlide == slide)
		nextSlide = (slide->next == slide ? NULL : slide->next);

	if (slide->defaultImage)
	{
		// Remove it from the default slide list
		for (Vector<BGSlide*>::iterator it = defaultSlides.begin(); it != defaultSlides.end(); it++)
		{
			BGSlide* pSlide = *it;
			if (pSlide != slide)
				continue;

			// Found it.
			defaultSlides.erase(it);
			break;
		}
	}

	// Reset directory listing
	if (mDirectoryMenu.is_open)
		mDirectoryMenu.calculate();
}

void GuiBackgroundCtrl::addSlide(const char* fileName)
{
	// Check for an existing slide w/ this filename
	const char* ourFName = dStrrchr(fileName, '/');
	bool defaultImage    = false;

	if (ourFName == NULL)
	{
		Con::errorf("Invalid image path \"%s\"", fileName);
		return;
	}

	ourFName++;
	if (dStrstr(ourFName, "screenshot_") != ourFName && dStrstr(ourFName, "blockland_") != ourFName)
	{
		defaultImage = true;

		// Default file
		for (Vector<BGSlide*>::iterator it = defaultSlides.begin(); it != defaultSlides.end(); it++)
		{
			BGSlide* slide         = *it;
			const char* theirFName = dStrrchr(slide->mBitmapName, '/') + 1;

			// Found our image among the default images. Return.
			if (!dStricmp(theirFName, ourFName))
				return;
		}
	}

	BGSlide* newSlide        = new BGSlide;
	newSlide->bitmapIcon     = NULL;
	newSlide->mTextureHandle = NULL;
	newSlide->defaultImage   = defaultImage;
	newSlide->index          = slides.size();
	newSlide->mBitmapName    = dStrdup(fileName);
	newSlide->bmpRes         = Point2I(-1, -1);

	// Setup next link
	if (slides.size() != 0)
		slides[slides.size() - 1]->next = newSlide;

	// Add to slides
	if (defaultImage)
		defaultSlides.push_back(newSlide);

	slides.push_back(newSlide);

	// Further linking setup
	newSlide->last  = slides[slides.size() - 2];
	newSlide->next  = slides.front();
	slides[0]->last = newSlide;

	// Set the current slide to this if this is the first slide.
	if (currentSlide == NULL)
		currentSlide = newSlide;

	timer = Sim::getCurrentTime();

	// Reset directory listing
	if (mDirectoryMenu.is_open)
		mDirectoryMenu.calculate();
}

void GuiBackgroundCtrl::addDirectory(const char* directory)
{
	Vector<Platform::FileInfo> fileList;

	char path[1024];
	dStrcpy(path, directory);
	if (*(path + (dStrlen(path) - 1)) == '\\' || *(path + (dStrlen(path) - 1)) == '/')
		*(path + (dStrlen(path) - 1)) = 0;

	// Ensure the directory exists
	if (!Platform::isDirectory(path))
	{
		Con::errorf("GuiBackgroundCtrl::addDirectory() - Path \"%s\" does not exist.", path);
		return;
	}

	// Dump the directory in the file list
	Platform::dumpPath(path, fileList);

	// Process the file list
	S32 added = 0;
	for (Vector<Platform::FileInfo>::iterator it = fileList.begin(); it != fileList.end(); it++)
	{
		Platform::FileInfo* info = &(*it);
		char fName[1024];

		// Build it
		dStrcpy(fName, info->pFullPath);
		dStrcat(fName, "/");
		dStrcat(fName, info->pFileName);
		
		// Get the extension
		const char* ext = dStrrchr(fName, '.');

		// Continue if it isn't a valid image file
		if (!ext || (dStricmp(ext, ".jpg") && dStricmp(ext, ".jpeg") && dStricmp(ext, ".png") && dStricmp(ext, ".bmp")))
			continue;

		// Add the slide
		addSlide(fName);
		added++;
	}

	Con::printf("Added %d image%s from \"%s\"", added, (added == 1 ? "" : "s"), directory);
}

void GuiBackgroundCtrl::setNext(U32 index, bool forceNow) {
	if (nextSlide != NULL)
	{
		Con::errorf("ERROR: GuiBackgroundCtrl::setNext() - Cannot set next slide while in transition!");
		return;
	}

	if (index >= slides.size())
	{
		Con::errorf("ERROR: GuiBackgroundCtrl::setNext() - Index out of range; index = %d, size = %d", index, slides.size());
		return;
	}

	if (slides[index] == currentSlide)
		return;

	if (forceNow)
	{
		currentSlide->mTextureHandle = NULL;
		currentSlide = slides[index];
		timer        = Sim::getCurrentTime() - transitionTime;
		nextSlide    = NULL;
	}
	else
		nextSlide = slides[index];

	mNavImageOffset = Point2I(0, 0);
	mNavImageScalar = Point2F(1.f, 1.f);
}

void GuiBackgroundCtrl::dump() {
	Con::printf("%d screenshot(s):", slides.size());
	for (Vector<BGSlide*>::iterator it = slides.begin(); it != slides.end(); it++)
	{
		BGSlide* slide = *it;

		Con::printf("  > [%04d] \"%s\"", slide->index, slide->mBitmapName);
	}
}

void GuiBackgroundCtrl::openDirectory()
{
	mDirectoryMenu.open();
}

void GuiBackgroundCtrl::setShuffle(bool val)
{
	mShuffle = val;
}

void GuiBackgroundCtrl::setPlay(bool val)
{
	mPlay = val;
}

void GuiBackgroundCtrl::closeNavMenu()
{
	mNavHoverBtn      = NULL;
	imageDisplayTimer = Sim::getCurrentTime() - (FADE_TIME + EXPIRE_TIME + 1);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define NAV_BTN_PAD 4
#define NAV_BTN_SIZE 16
#define HALF_FADE_TIME (FADE_TIME / 2)

//BACKGROUND_NAV_BUTTON_PADDING
void GuiBackgroundCtrl::renderNavButtons(Point2I offset, const RectI& updateRect)
{
	SimTime curTime    = Sim::getCurrentTime();
	SimTime timePassed = curTime - imageDisplayTimer;
	F32 BGalphaScalar  = 1.f;
	F32 UIalphaScalar  = 1.f;

	// Calculate fade in & fade out
	if (timePassed < FADE_TIME)
	{
		// First BG, then UI
		if (timePassed < HALF_FADE_TIME)
		{
			BGalphaScalar = F32(timePassed) / (F32)HALF_FADE_TIME;
			UIalphaScalar = 0.f;
		}
		else
			UIalphaScalar = F32(timePassed - HALF_FADE_TIME) / (F32)HALF_FADE_TIME;
	}
	else if (timePassed > EXPIRE_TIME - 1)
	{
		// First UI, then BG
		if (timePassed < (EXPIRE_TIME + HALF_FADE_TIME))
			UIalphaScalar = 1.f - F32(F32(timePassed - EXPIRE_TIME) / (F32)HALF_FADE_TIME);
		else
		{
			BGalphaScalar = 1.f - F32(F32(timePassed - (EXPIRE_TIME + HALF_FADE_TIME)) / (F32)HALF_FADE_TIME);
			UIalphaScalar = 0.f;
		}
	}

	// Set new offset Y position
	offset.y = (20 - S32(((BGalphaScalar * 0.5f) + (UIalphaScalar * 0.5f)) * 20.f));

	// Calc rects
	RectI navMenuRc(mNavRc.point + offset, mNavRc.extent);
	RectI navTextMenuRc(mNavTextRc.point + offset, mNavTextRc.extent);

	// Get the texture
	TextureObject* texture = mIconArray.get();

	// Render the background & border of the interaction panel
	dglDrawRectFill(navMenuRc, ColorI(0, 0, 0, U8(200.f * BGalphaScalar)));
	dglDrawRect(navMenuRc, ColorI(120, 120, 120, U8(120.f * BGalphaScalar)));

	// Render the buttons
	S32 i = 1;
	for (NavButton* walk = &mNavButtons[(i = 1) - 1]; walk->invoke; walk = &mNavButtons[i++])
	{
		// Stop here if we're invisible
		if (!walk->isVisible(this))
			continue;

		// Render selected background if applicable
		if (mNavHoverBtn == walk)
			dglDrawRectFill(RectI(walk->rect.point + offset, walk->rect.extent), ColorI(255, 255, 255, U8(70.f * BGalphaScalar)));

		// Get the color
		ColorI mod_color(walk->getBitmapModColor(this));

		// Multiply by our scalar
		if (UIalphaScalar != 1.f)
			mod_color.alpha = U8((F32)mod_color.alpha * UIalphaScalar);

		// Draw this nav button
		dglSetBitmapModulation(mod_color);
		dglDrawBitmapSR(texture, offset + walk->imageOffset, mIconArray[walk->iconIdx]);
	}

	// Render the background & border of the text panel
	dglDrawRectFill(navTextMenuRc, ColorI(0, 0, 0, U8(200.f * BGalphaScalar)));
	dglDrawRect(navTextMenuRc, ColorI(120, 120, 120, U8(120.f * BGalphaScalar)));

	// Draw text
	dglSetBitmapModulation(ColorI(mProfile->mFontColor.red, mProfile->mFontColor.green, mProfile->mFontColor.blue, U8((F32)mProfile->mFontColor.alpha * UIalphaScalar)));
	dglDrawText(mProfile->mFont, offset + mNavTextPos, mNavText, mProfile->mFontColors);

	// Clear bitmap modulation
	dglClearBitmapModulation();
}

GuiBackgroundCtrl::NavButton* GuiBackgroundCtrl::hitScanNavMenu(Point2I pnt)
{
	S32 i = 1;
	for (NavButton* walk = &mNavButtons[(i = 1) - 1]; walk->invoke; walk = &mNavButtons[i++])
	{
		// Skip invisible nav buttons & rects that our pnt isnt in
		if (!walk->isVisible(this) || !walk->rect.pointInRect(pnt))
			continue;
		
		// Found it!
		return walk;
	}

	return NULL;
}

void GuiBackgroundCtrl::calculateNavMenu()
{
	// Initialize
	Point2I buttonSize(NAV_BTN_SIZE, NAV_BTN_SIZE);
	mNavRc = RectI(0, 0, NAV_BTN_PAD, NAV_BTN_SIZE + (NAV_BTN_PAD * 2));
	S32 i  = 1;

	// Load text
	char buffer[1024];
	dSprintf(buffer, sizeof(buffer), "(%d / %d) - %s%s%s", (currentSlide ? currentSlide->index + 1 : 0), slides.size(), (currentSlide ? "\"" : ""), (currentSlide ? currentSlide->mBitmapName : "N/A"), (currentSlide ? "\"" : ""));

	// Copy the buffer
	if (mNavText)
		dFree(mNavText);
	mNavText = dStrdup(buffer);

	// Calculate the total width of the menu first
	for (NavButton* walk = &mNavButtons[(i = 1) - 1]; walk->invoke; walk = &mNavButtons[i++])
	{
		// Skip invisible nav buttons
		if (!walk->isVisible(this))
			continue;

		// Add to the width
		mNavRc.extent.x += NAV_BTN_SIZE + NAV_BTN_PAD;
	}

	// Calculate the nav menu's position
	mNavRc.point.x = (mBounds.extent.x / 2) - (mNavRc.extent.x / 2);
	mNavRc.point.y = mBounds.extent.y - ((mBounds.extent.y / 4) - (mNavRc.extent.y / 2));

	// Now, calculate all nav buttons' rects
	S32 xOffset = mNavRc.point.x + NAV_BTN_PAD;
	for (NavButton* walk = &mNavButtons[(i = 1) - 1]; walk->invoke; walk = &mNavButtons[i++])
	{
		// Skip invisible nav buttons
		if (!walk->isVisible(this))
			continue;

		// First calculate the base rect
		walk->rect = RectI(xOffset, mNavRc.point.y + NAV_BTN_PAD, NAV_BTN_SIZE, NAV_BTN_SIZE);
		xOffset   += NAV_BTN_SIZE + NAV_BTN_PAD;

		// Then calculate the center position
		walk->imageOffset.x = walk->rect.point.x + ((walk->rect.extent.x / 2) - (mIconArray[walk->iconIdx].extent.x / 2));
		walk->imageOffset.y = walk->rect.point.y + ((walk->rect.extent.y / 2) - (mIconArray[walk->iconIdx].extent.y / 2));
	}

	// Calculate text bg rect
	mNavTextRc = RectI(0, 0, mProfile->mFont->getStrWidthPrecise((const UTF8*)mNavText) + (NAV_BTN_PAD * 2), mProfile->mFont->getHeight() + (NAV_BTN_PAD * 2));

	mNavTextRc.point = Point2I((mBounds.extent.x / 2) - (mNavTextRc.extent.x / 2), mNavRc.point.y + (mNavRc.extent.y + NAV_BTN_PAD));

	// Determine nav text position
	mNavTextPos.x = mNavTextRc.point.x + NAV_BTN_PAD;
	mNavTextPos.y = mNavTextRc.point.y + ((mNavTextRc.extent.y / 2) - (mProfile->mFont->getHeight() / 2));
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiBackgroundCtrl::onMouseUp(const GuiEvent& event)
{
	RectI leftRect(rect.point.x, rect.point.y, rect.extent.x / 6, rect.extent.y);
	RectI rightRect(rect.point.x + (rect.extent.x - (rect.extent.x / 6)), rect.point.y, rect.extent.x / 6, rect.extent.y);

	if (mNavDragImage)
	{
		mNavDragImage = false;
		return;
	}

	if (mDirectoryMenu.is_open)
	{
		mDirectoryMenu.onMouseUp(event.mousePoint);
		return;
	}

	if (imageDisplayTimer != 0) {
		imageDisplayTimer = Sim::getCurrentTime() - FADE_TIME;

		// Hit scan the nav menu
		NavButton* nBtn = hitScanNavMenu(event.mousePoint);
		if (nBtn)
		{
			timer = Sim::getCurrentTime();
			nBtn->invoke(this);
			calculateNavMenu();
			return;
		}

		if (currentSlide != NULL && (Sim::getCurrentTime() - timer) < transitionDelay)
		{
			if (leftRect.pointInRect(event.mousePoint))
			{
				timer = Sim::getCurrentTime();
				currentSlide->mTextureHandle = NULL;
				currentSlide = currentSlide->last;
				calculateNavMenu();
			}

			if (rightRect.pointInRect(event.mousePoint))
			{
				timer = Sim::getCurrentTime();
				currentSlide->mTextureHandle = NULL;
				currentSlide = currentSlide->next;
				calculateNavMenu();
			}
		}

		return;
	}

	if (doubleClickTimer == 0 || (Sim::getCurrentTime() - doubleClickTimer) >= 2000)
	{
		doubleClickTimer = Sim::getCurrentTime();
	}
	else if ((Sim::getCurrentTime() - doubleClickTimer) < 2000)
	{
		mNavWasTransitioning = isTransitioning();
		doubleClickTimer     = 0;
		delta                = Sim::getCurrentTime();
		imageDisplayTimer    = Sim::getCurrentTime();

		calculateNavMenu();
	}
}

void GuiBackgroundCtrl::onMouseMove(const GuiEvent& event)
{
	if (mDirectoryMenu.is_open)
	{
		mDirectoryMenu.onMouseMove(event.mousePoint);
		return;
	}

	mNavHoverBtn = hitScanNavMenu(event.mousePoint);
}

void GuiBackgroundCtrl::onMouseDown(const GuiEvent& event)
{
	mDisallowDrag = mDirectoryMenu.visible || mNavRc.pointInRect(event.mousePoint);

	if (mDirectoryMenu.is_open)
		mDirectoryMenu.onMouseDown(event.mousePoint);
}

void GuiBackgroundCtrl::onMouseEnter(const GuiEvent& event)
{
	mNavHoverBtn = hitScanNavMenu(event.mousePoint);
}

void GuiBackgroundCtrl::onMouseLeave(const GuiEvent& event)
{
	mNavHoverBtn  = NULL;
	mNavDragImage = false;
}

void GuiBackgroundCtrl::onMouseDragged(const GuiEvent& event)
{
	if (mDirectoryMenu.is_open)
	{
		mDirectoryMenu.onMouseDragged(event.mousePoint);
		return;
	}

	if (mDisallowDrag || (!mNavDragImage && !imageDisplayTimer))
		return;

	if (!mNavDragImage)
	{
		// Start dragging the image
		mNavDragImage            = true;
		mNavImageOffsetDragStart = mNavImageOffset;
		mNavImageDragStart       = event.mousePoint;
	}

	// Get the new image offset
	mNavImageOffset = mNavImageOffsetDragStart + (event.mousePoint - mNavImageDragStart);

	// Clamp the background
	clampBackground();
}


#define POINT2F(val) Point2F((val).x, (val).y)
#define POINT2I(val) Point2I((val).x, (val).y)
bool GuiBackgroundCtrl::onMouseWheelUp(const GuiEvent& event)
{
	if (mDirectoryMenu.is_open)
	{
		mDirectoryMenu.onWheelScroll(-1, event.mousePoint);
		return true;
	}

	if (imageDisplayTimer)
	{
		if (mNavImageScalar.x + 0.1f > 7.f || mNavImageScalar.y + 0.1f > 7.f)
		{
			mNavImageScalar.x = 7.f;
			mNavImageScalar.y = 7.f;
			return true;
		}

		// Scale the scalar
		RectI before(getRenderRect(currentSlide, rect, true, true));
		mNavImageScalar = Point2F(mNavImageScalar.x + 0.1f, mNavImageScalar.y + 0.1f);
		RectI after(getRenderRect(currentSlide, rect, true, true));

		mNavImageOffset += (before.extent - after.extent) / 2;

		// Reset the timer
		imageDisplayTimer = Sim::getCurrentTime() - FADE_TIME;

		// Re-clamp
		clampBackground();
	}

	return true;
}

bool GuiBackgroundCtrl::onMouseWheelDown(const GuiEvent& event)
{
	if (mDirectoryMenu.is_open)
	{
		mDirectoryMenu.onWheelScroll(1, event.mousePoint);
		return true;
	}

	if (imageDisplayTimer)
	{
		if (mNavImageScalar.x - 0.1f < 1.f || mNavImageScalar.y - 0.1f < 1.f)
		{
			mNavImageScalar.x = 1.f;
			mNavImageScalar.y = 1.f;
			return true;
		}

		RectI before(getRenderRect(currentSlide, rect, true, true));
		mNavImageScalar = Point2F(mNavImageScalar.x - 0.1f, mNavImageScalar.y - 0.1f);
		RectI after(getRenderRect(currentSlide, rect, true, true));

		mNavImageOffset += (before.extent - after.extent) / 2;

		// Reset the timer
		imageDisplayTimer = Sim::getCurrentTime() - FADE_TIME;

		// Re-clamp
		clampBackground();
	}

	return true;
}

GuiControl* GuiBackgroundCtrl::findHitControl(const Point2I& pt, S32 initialLayer)
{
	if (imageDisplayTimer)
	{
		if (mContextMenu && mContextMenu->isVisible())
		{
			GuiControl* ret = mContextMenu->findHitControl(pt, initialLayer);
			return (ret ? ret : this);
		}

		return this;
	}

	return Parent::findHitControl(pt, initialLayer);
}

void GuiBackgroundCtrl::resize(const Point2I& newPosition, const Point2I& newExtent)
{
	// Handle this
	Parent::resize(newPosition, newExtent);

	// Re-calculate the rect
	rect = RectI(localToGlobalCoord(mBounds.point), mBounds.extent);

	// Re-clamp the background
	clampBackground();

	// Re-calculate the directory menu
	if (mDirectoryMenu.is_open)
		mDirectoryMenu.calculate();
}

#define WRITE_LINE(fmt, ...) dSprintf(buf, sizeof(buf), fmt, __VA_ARGS__); f.write(dStrlen(buf), buf);
void GuiBackgroundCtrl::saveConfig()
{
	FileStream f;
	char fileName[1024];
	dSprintf(fileName, sizeof(fileName), "launcher/%s.dat", (getName() == 0 || !*getName()) ? "background" : getName());

	if (!ResourceManager->openFileForWrite(f, fileName, 1U, true))
	{
		Con::errorf("GuiBackgroundCtrl::saveConfig() - Failed to open \"%s\"!", fileName);
		return;
	}

	// Write config elements
	S32 strLen = (currentSlide ? dStrlen(currentSlide->mBitmapName) : 0);

	char header[5];
	dStrcpy(header, CONFIG_HEADER);

	f.write(4, header);
	f.write(U8(CONFIG_VERSION));
	f.write(strLen);
	f.write(mNavImageOffset.x);
	f.write(mNavImageOffset.y);
	f.write(mNavImageScalar.y);
	f.write(mNavImageScalar.y);
	f.write(mPlay);
	f.write(mShuffle);

	if (currentSlide)
		f.write(dStrlen(currentSlide->mBitmapName), currentSlide->mBitmapName);

	// Done!
	f.close();
}

void GuiBackgroundCtrl::loadConfig()
{
	FileStream f;
	char fileName[1024];
	dSprintf(fileName, sizeof(fileName), "launcher/%s.dat", (getName() == 0 || !*getName()) ? "background" : getName());

	if (!f.open(fileName, FileStream::AccessMode::Read))
		return;

	// Ready some variables
	StringTableEntry bmpName = NULL;
	S32 bmpNameStrLen        = 0;
	U8 version               = 0;
	char header[32];

	// Read it
	f.read(4, header);
	header[4] = 0;

	if (!dStrcmp(header, CONFIG_HEADER))
	{
		// After version 0
		f.read(&version);
		f.read(&bmpNameStrLen);
	}
	else
	{
		// Version 0 -- Before headers were implemented into it
		bmpNameStrLen = *((int*)header);
	}

	f.read(&mDefaultState.imgOffset.x);
	f.read(&mDefaultState.imgOffset.y);

	if (version == 1)
	{
		f.read(&mDefaultState.imgScalar.x);
		f.read(&mDefaultState.imgScalar.y);
	}
	else
	{
		mDefaultState.imgScalar.x = 1.f;
		mDefaultState.imgScalar.y = 1.f;
	}

	f.read(&mDefaultState.autoPlay);
	f.read(&mDefaultState.shuffle);

	// Read the bitmap name
	if (bmpNameStrLen)
	{
		char buf[1024];
		char* ptr = buf;
		if (bmpNameStrLen >= sizeof(buf))
			ptr = new char[bmpNameStrLen + 1];

		// Read name
		f.read(bmpNameStrLen, ptr);
		ptr[bmpNameStrLen] = 0;

		// Insert it into the string table
		bmpName = StringTable->insert(ptr);

		// Free the old string if applicable
		if (ptr != buf)
			delete[] ptr;
	}

	// Done!
	f.close();

	// Setup default state
	mDefaultState.loaded  = true;
	mDefaultState.bmpName = bmpName;
}

void GuiBackgroundCtrl::applyDefaultState()
{
	if (!mDefaultState.loaded)
		return;

	// Load the slide if necessary
	if (mDefaultState.bmpName && *mDefaultState.bmpName)
	{
		BGSlide* slide = NULL;
		bool secondTime = false;

	RETRY_ADD:
		for (Vector<BGSlide*>::iterator it = slides.begin(); it != slides.end(); it++)
		{
			BGSlide* cSlide = *it;
			if (dStricmp(cSlide->mBitmapName, mDefaultState.bmpName))
				continue;

			// Found
			slide = cSlide;
			break;
		}

		if (slide)
		{
			setNext(slide->index, true);
			goto SUCCESS;
		}

		if (secondTime)
		{
			Con::errorf("GuiBackgroundCtrl::applyDefaultState() - Unable to find image \"%s\"!", mDefaultState.bmpName);
			return;
		}

		// It doesn't exist; Add it
		addSlide(mDefaultState.bmpName);
		secondTime = true;
		goto RETRY_ADD;
	}

SUCCESS:
	// Apply default state
	mPlay           = mDefaultState.autoPlay;
	mShuffle        = mDefaultState.shuffle;
	mNavImageOffset = mDefaultState.imgOffset;
	mNavImageScalar = mDefaultState.imgScalar;
	rect            = RectI(localToGlobalCoord(mBounds.point), mBounds.extent);

	// Clamp the background
	onRender(rect.point, rect);
	clampBackground();
}

void GuiBackgroundCtrl::onContextMenuSelected(S32 rowID, const char* rowText)
{
	switch (rowID)
	{
		case 0: // Set default image
		{
			saveConfig();
			break;
		}
		case 1: // Open in Explorer
		{
			Con::executef(2, "openInExplorer", currentSlide->mBitmapName);
			break;
		}
		case 2: // Reload default background
		{
			loadConfig();
			applyDefaultState();
			break;
		}
	}
}

void GuiBackgroundCtrl::onRightMouseDown(const GuiEvent& event)
{
	if (!imageDisplayTimer || mNavDragImage)
		return;

	if (mDirectoryMenu.is_open)
		return;

	if (!mContextMenu)
	{
		mContextMenu = new GuiContextMenuCtrl();
		mContextMenu->setControlProfile(dynamic_cast<GuiControlProfile*>(Sim::findObject("LauncherContextMenuProfile")));
		mContextMenu->registerObject();
		mContextMenu->onSelect.AddListener(_onselect, this);
	}

	// Initial addition
	addObject(mContextMenu);
	mContextMenu->setPosition(event.mousePoint);
	mContextMenu->setVisible(true);
	mContextMenu->setVisible(false);

	// Populate it with items
	mContextMenu->clearItems();
	mContextMenu->addItem("Save default state", 0);
	mContextMenu->addItem("Open in Explorer", 1);
	mContextMenu->addSeperator();
	mContextMenu->addItem("Reload default state", 2);

	// Re-add the context menu
	mContextMenu->forceRefit();
	addObject(mContextMenu);
	mContextMenu->setPosition(event.mousePoint);
	mContextMenu->setVisible(true);
}

bool GuiBackgroundCtrl::initCursors()
{
	if (!mDefaultCursor || !mMoveCursor)
	{
		SimObject* obj;
		obj = Sim::findObject("DefaultCursor");
		mDefaultCursor = dynamic_cast<GuiCursor*>(obj);
		obj = Sim::findObject("MoveCursor");
		mMoveCursor = dynamic_cast<GuiCursor*>(obj);

		return (mDefaultCursor != NULL && mMoveCursor != NULL);
	}
	else return true;
}

void GuiBackgroundCtrl::getCursor(GuiCursor*& cursor, bool& showCursor, const GuiEvent& lastGuiEvent)
{
	cursor     = (mNavDragImage && mMoveCursor) ? mMoveCursor : NULL;
	showCursor = true;
}

void GuiBackgroundCtrl::onRender(Point2I offset, const RectI &updateRect)
{
	SimTime curTime = Sim::getCurrentTime();
	rect            = RectI(offset, mBounds.extent);

	// Fill in the rect we occupy
	dglDrawRectFill(rect, ColorI(0, 0, 0, 255));

	// Process the current slide
RETRY_SLIDE_RENDER:
	if (currentSlide != NULL) {
		if (!currentSlide->mTextureHandle)
		{
			// Attempt to ready the slide
			if (!readyHandle(currentSlide))
			{
				BGSlide* thisSlide = currentSlide;

				// If we failed to ready the slide's handle, then just kill it
				removeSlide(thisSlide);

				// Delete this slide
				dFree(thisSlide->mBitmapName);
				delete thisSlide;

				// Set the next slide
				mNavImageOffset = Point2I(0, 0);
				mNavImageScalar = Point2F(1.f, 1.f);
				
				// Re-render
				goto RETRY_SLIDE_RENDER;
			}
		}

		if (mPlay && (curTime - timer) >= transitionDelay && (curTime - (timer + transitionDelay)) < transitionTime)
		{
			if (nextSlide == NULL)
			{
				if (mShuffle)
				{
					// Pick a random slide
					while (true)
					{
						S32 rand = gRandGen.randI(0, slides.size() - 1);
						if (rand == currentSlide->index)
						{
							nextSlide = currentSlide->next;
							break;
						}

						nextSlide = slides[rand];
						break;
					}
				}
				else nextSlide = currentSlide->next;
			}

			if (!nextSlide->mTextureHandle && !readyHandle(nextSlide))
			{
				// If we failed to ready the slide's handle, then just kill it
				BGSlide* slideToNuke = nextSlide;

				// Remove it
				removeSlide(slideToNuke);

				// Delete this slide
				dFree(slideToNuke->mBitmapName);
				delete slideToNuke;
				nextSlide = NULL;

				// Re-render
				goto RETRY_SLIDE_RENDER;
			}

			if (nextSlide == currentSlide)
			{
				nextSlide = NULL;
				timer     = curTime;
				goto RETRY_SLIDE_RENDER;
			}

			// Fade in next slide
			U8 alpha = U8((F64((Sim::getCurrentTime() - timer) - transitionDelay) / F64(transitionTime)) * 255.0);

			// Render this slide
			dglDrawBitmapStretch(currentSlide->mTextureHandle, getRenderRect(currentSlide, rect));

			// Draw next slide
			timer += transitionDelay;
			dglSetBitmapModulation(ColorI(255, 255, 255, alpha));
			dglDrawBitmapStretch(nextSlide->mTextureHandle, getRenderRect(nextSlide, rect));
			dglClearBitmapModulation();
			timer -= transitionDelay;
		}
		else if (mPlay && (curTime - timer) >= transitionDelay + transitionTime)
		{
			// Set the next slide
			currentSlide->mTextureHandle = NULL;
			currentSlide    = nextSlide;
			timer           = Sim::getCurrentTime() - transitionTime;
			nextSlide       = NULL;
			mNavImageOffset = Point2I(0, 0);
			mNavImageScalar = Point2F(1.f, 1.f);

			// Re-do the rendering process
			onRender(offset, updateRect);
			return;
		}
		else
		{
			// Render the current slide
			dglDrawBitmapStretch(currentSlide->mTextureHandle, getRenderRect(currentSlide, rect));
		}
	}

	if (mProfile->mBorder || slides.size() == 0)
	{
		RectI rect(offset.x, offset.y, mBounds.extent.x, mBounds.extent.y);
		dglDrawRect(rect, mProfile->mBorderColor);
	}

	// Render children now
	renderChildControls(offset, updateRect);
	dglSetClipRect(updateRect);

	// Render nav menu
	if (imageDisplayTimer != 0)
	{
		RectI textArea(rect.point.x, rect.point.y + S32((F32)rect.extent.y / 1.5f), rect.extent.x, S32((F32)rect.extent.y / 1.5f));

		// Effectively stop the timer
		if (((mContextMenu && mContextMenu->isAwake()) || mDirectoryMenu.visible || mNavHoverBtn || mNavDragImage) && (Sim::getCurrentTime() - imageDisplayTimer) >= FADE_TIME)
			imageDisplayTimer = Sim::getCurrentTime() - FADE_TIME;

		if (mDirectoryMenu.visible)
			mDirectoryMenu.render(offset, updateRect);

		// Don't stop the timer if we're in a transition
		if (!nextSlide)
			timer += Sim::getCurrentTime() - delta;

		// Update delta
		delta = Sim::getCurrentTime();

		// Stop transitioning if we're here
		if (!nextSlide && mNavWasTransitioning)
		{
			mNavWasTransitioning = false;
			calculateNavMenu();
		}

		// Render the bit
		renderNavButtons(offset, updateRect);

		// Render the buttons
		RectI leftRect(rect.point.x, rect.point.y, rect.extent.x / 6, rect.extent.y);
		RectI rightRect(rect.point.x + (rect.extent.x - (rect.extent.x / 6)), rect.point.y, rect.extent.x / 6, rect.extent.y);
		Point2I cPos(Canvas->getCursorPos());

		if ((curTime - imageDisplayTimer) < EXPIRE_TIME)
		{
			if ((curTime - imageDisplayTimer) < FADE_TIME)
			{
				U8 alpha = U8((F32(curTime - imageDisplayTimer) / (F32)FADE_TIME) * 255);

				dglSetBitmapModulation(ColorI(255, 255, 255, alpha));
				dglDrawBitmapStretch(mLeftHandle, leftRect);
				dglDrawBitmapStretch(mRightHandle, rightRect);
				dglClearBitmapModulation();
			}
			else
			{
				dglDrawBitmapStretch(mLeftHandle, leftRect);
				dglDrawBitmapStretch(mRightHandle, rightRect);
			}
		}
		else if ((curTime - imageDisplayTimer) < (FADE_TIME + EXPIRE_TIME))
		{
			U8 alpha = (U8)255 - U8((F32((curTime - imageDisplayTimer) - EXPIRE_TIME) / (F32)FADE_TIME) * 255);

			dglSetBitmapModulation(ColorI(255, 255, 255, alpha));
			dglDrawBitmapStretch(mLeftHandle, leftRect);
			dglDrawBitmapStretch(mRightHandle, rightRect);
			dglClearBitmapModulation();
		}
		else
		{
			mNavHoverBtn      = NULL;
			imageDisplayTimer = 0;
		}
	}

	if (mShowMem && currentSlide && currentSlide->mTextureHandle)
	{
		// Buffer it
		char memText[1024];
		dSprintf(memText, 1024, "0x%08p [%d B] %dx%d", (*currentSlide->mTextureHandle).texGLName, (*currentSlide->mTextureHandle).bitmap ? (*currentSlide->mTextureHandle).bitmap->byteSize : 0, currentSlide->bmpRes.x, currentSlide->bmpRes.y);

		// Calculate position
		U32 strWidth = mProfile->mFont->getStrWidth((const UTF8*)memText);
		Point2I drawPos((offset.x + mBounds.extent.x) - strWidth, 0);

		// Draw the position
		dglDrawRectFill(RectI(drawPos.x, drawPos.y, strWidth, mProfile->mFont->getHeight()), ColorI(0, 0, 0, 255));

		// Draw the text
		dglDrawText(mProfile->mFont, drawPos, memText, mProfile->mFontColors);
	}
}

ConsoleMethod(GuiBackgroundCtrl, applyDefaultState, void, 2, 2, "")
{
	object->applyDefaultState();
}

ConsoleMethod(GuiBackgroundCtrl, clearSlides, void, 2, 2, "")
{
	object->clearSlides();
}

ConsoleMethod(GuiBackgroundCtrl, addDirectory, void, 3, 3, "(path)")
{
	object->addDirectory(argv[2]);
}

ConsoleMethod(GuiBackgroundCtrl, addSlide, void, 3, 3, "(path)")
{
	object->addSlide(argv[2]);
}

ConsoleMethod(GuiBackgroundCtrl, setNext, void, 3, 3, "(index)")
{
	object->setNext(dAtoi(argv[2]));
}

ConsoleMethod(GuiBackgroundCtrl, clamp, void, 2, 2, "")
{
	object->clampBackground();
}

ConsoleMethod(GuiBackgroundCtrl, dumpSlides, void, 2, 2, "()")
{
	object->dump();
}