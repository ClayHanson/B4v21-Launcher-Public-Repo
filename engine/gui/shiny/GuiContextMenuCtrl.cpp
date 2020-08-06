#include "gui/shiny/guiContextMenuCtrl.h"
#include "platform/platformVideo.h"
#include "gui/core/guiCanvas.h"
#include "dgl/dgl.h"

IMPLEMENT_CONOBJECT(GuiContextMenuCtrl);

#define ITEM_PADDING 4
#define CONTENT_OFFSET 24
#define SEPERATOR_HEIGHT 5
#define EXPAND_TIME 400
#define EXPAND_EXPIRE_TIME 300
#define APPEAR_TIME 200

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiContextMenuCtrl::GuiContextMenuCtrl() : mAppearTimer()
{
	mValidProfileBitmap = false;
	mParent             = NULL;
	mOldMousedOver      = NULL;
	mMousedOver         = NULL;
	mMouseOverTime      = 0;
	mMOverExpireTime    = 0;
}

GuiContextMenuCtrl::~GuiContextMenuCtrl()
{
	if (mParent)
		mParent->subMenu = NULL;

	clearItems();
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiContextMenuCtrl::Entry* GuiContextMenuCtrl::getHitscanEntry(Point2I point)
{
	// Check if we're inside our moused over's submenu
	if (mOldMousedOver && mOldMousedOver->subMenu && mOldMousedOver->subMenu->isVisible())
	{
		RectI rc(mOldMousedOver->subMenu->getBounds());
		rc.point = mOldMousedOver->subMenu->localToGlobalCoord(rc.point);

		if (rc.pointInRect(point) || mOldMousedOver->subMenu->getHitscanEntry(point))
			return mOldMousedOver;
	}

	// Hit scan
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		Entry* item = *it;

		// Skip invalid entries
		if (item->type == EntryType::SEPERATOR)
			continue;

		// Calculate the global rect
		RectI itemRC(item->renderRect);
		itemRC.point = localToGlobalCoord(itemRC.point);

		// Determine if 'point' is in this entry's rect
		if (!itemRC.pointInRect(point))
			continue;

		return item;
	}

	// Couldn't find any entries that the mouse was over
	GuiContextMenuCtrl* subMenu = getOpenSubMenu();
	return !mOldMousedOver ? (subMenu ? subMenu->mParent : NULL) : NULL;
}

void GuiContextMenuCtrl::processKeyEvent(const GuiEvent& event)
{
	// 15 : Left arrow
	// 16 : Up arrow
	// 17 : Right arrow
	// 18 : Down arrow

	enum {
		LEFT   = 0x0F,
		UP     = 0x10,
		RIGHT  = 0x11,
		DOWN   = 0x12,
		ENTER  = 0x03,
		ESCAPE = 0x09
	};

	// Send this event to that submenu
	GuiContextMenuCtrl* subMenu = getOpenSubMenu();
	if (subMenu && mOldMousedOver != subMenu->mParent)
	{
		subMenu->processKeyEvent(event);
		return;
	}

	switch (event.keyCode)
	{
		case LEFT:
		{
			if (mParent)
			{
				closeContextMenu();
				return;
			}

			break;
		}
		case RIGHT:
		{
			if (mMousedOver == NULL)
			{
				mMousedOver    = mList[0];
				mMouseOverTime = 0;
				return;
			}

			if (!mMousedOver->subMenu || mMousedOver->subMenu->isVisible())
				return;

			if (mMousedOver->subMenu->mList.size() != 0)
				mMousedOver->subMenu->mMousedOver = mMousedOver->subMenu->mList[0];

			mMouseOverTime = Sim::getCurrentTime() - EXPAND_TIME;
			break;
		}
		case UP:
		{
			if (mMousedOver == NULL)
			{
				mMousedOver    = mList[0];
				mMouseOverTime = 0;
				return;
			}

			if (mMousedOver->index == 0)
				return;

			// Close the last sub menu
			if (mMousedOver->subMenu && mMousedOver->subMenu->isVisible())
				mMousedOver->subMenu->closeContextMenu();

			mMouseOverTime = 0;
			mMousedOver    = mList[mMousedOver->index - 1];

			// Skip seperators
			while (mMousedOver->index != 0 && mMousedOver->type == EntryType::SEPERATOR)
				mMousedOver = mList[mMousedOver->index - 1];

			break;
		}
		case DOWN:
		{
			if (mMousedOver == NULL)
			{
				mMousedOver    = mList[0];
				mMouseOverTime = 0;
				return;
			}

			if (mMousedOver->index >= mList.size() - 1)
				return;

			// Close the last sub menu
			if (mMousedOver->subMenu && mMousedOver->subMenu->isVisible())
				mMousedOver->subMenu->closeContextMenu();

			mMouseOverTime = 0;
			mMousedOver    = mList[mMousedOver->index + 1];

			// Skip seperators
			while (mMousedOver->index < mList.size() - 1 && mMousedOver->type == EntryType::SEPERATOR)
				mMousedOver = mList[mMousedOver->index + 1];

			break;
		}
		case ESCAPE:
		{
			closeContextMenu();
			break;
		}
		case ENTER:
		{
			if (!mMousedOver)
				return;

			RectI rc(mMousedOver->renderRect);
			rc.point = localToGlobalCoord(rc.point);

			GuiEvent fakeEvent;
			fakeEvent.mouseClickCount = 1;
			fakeEvent.mousePoint      = Point2I(rc.point.x + (rc.extent.x / 2), rc.point.y + (rc.extent.y / 2));

			onMouseUp(fakeEvent);
			break;
		}
		default:
		{
#ifndef TORQUE_DEBUG
			Con::printf("ASC: %03d, KEY: %03d, MOD: %04d", event.ascii, event.keyCode, event.modifier);
#endif
		}
	}
}

void GuiContextMenuCtrl::refitSelf()
{
	Point2I newExtent(mMinExtent.x, (!mList.size() ? mMinExtent.y : ITEM_PADDING * 2));

	// Find the longest width
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		Entry* item = *it;

		// Add to height
		newExtent.y += (item->type == EntryType::SEPERATOR ? SEPERATOR_HEIGHT : ITEM_PADDING + mProfile->mFont->getHeight());

		// Skip items w/o text
		if (!item->text)
			continue;

		// Get the real width of this item
		U32 width = mProfile->mFont->getStrWidth((UTF8*)item->text) + (ITEM_PADDING * 2) + (CONTENT_OFFSET * 2);
		if (width > newExtent.x)
			newExtent.x = width;
	}

	// Resize ourself
	setExtent(newExtent);

	// Re-calculate our entries' rects
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
		calcItemRect((Entry*)(*it));
}

void GuiContextMenuCtrl::calcItemRect(Entry* entry)
{
	entry->renderRect = RectI(
		ITEM_PADDING,
		(entry->index == 0 ? ITEM_PADDING : mList[entry->index - 1]->renderRect.point.y + mList[entry->index - 1]->renderRect.extent.y),
		mBounds.extent.x - (ITEM_PADDING * 2),
		0
	);

	// Determine other stuff
	switch (entry->type)
	{
		case EntryType::TEXT_IMAGE:
		case EntryType::TEXT:
		{
			entry->renderRect.extent.y = mProfile->mFont->getHeight() + ITEM_PADDING;
			entry->textPnt.x           = ITEM_PADDING + CONTENT_OFFSET;
			entry->textPnt.y           = entry->renderRect.point.y + ((entry->renderRect.extent.y / 2) - (mProfile->mFont->getHeight() / 2));
			break;
		}
		case EntryType::SEPERATOR:
		{
			entry->renderRect.extent.y = SEPERATOR_HEIGHT;
			entry->seperatorStart      = Point2I(entry->renderRect.point.x + CONTENT_OFFSET + ITEM_PADDING, entry->renderRect.point.y + (entry->renderRect.extent.y / 2));
			entry->seperatorEnd        = Point2I((entry->renderRect.point.x + entry->renderRect.extent.x) - ITEM_PADDING, entry->renderRect.point.y + (entry->renderRect.extent.y / 2));
			break;
		}
	}

	// Determine the image rects
	if (mValidProfileBitmap)
	{
		entry->expandImgRect = RectI(
			(entry->renderRect.point.x + entry->renderRect.extent.x) - (ITEM_PADDING + mProfile->mBitmapArrayRects[4].extent.x),
			entry->renderRect.point.y + ((entry->renderRect.extent.y / 2) - (mProfile->mBitmapArrayRects[4].extent.y / 2)),
			mProfile->mBitmapArrayRects[4].extent.x,
			mProfile->mBitmapArrayRects[4].extent.y
		);
	}
}

void GuiContextMenuCtrl::setMousedOver(Entry* entry)
{
	mMouseOverTime = Sim::getCurrentTime();
	if (mMousedOver == entry)
		return;

	if (mMousedOver && mMousedOver->subMenu && mMousedOver->subMenu->isVisible())
	{
		mOldMousedOver   = mMousedOver;
		mMOverExpireTime = Sim::getCurrentTime();
	}

	mMousedOver = entry;
}

void GuiContextMenuCtrl::renderEntry(Entry* entry, Point2I& offset, const RectI& updateRect)
{
	RectI drawRect(entry->renderRect);
	drawRect.point += offset;

	// Draw the background (if applicable)
	if (mMousedOver == entry)
		dglDrawRectFill(drawRect, mAppearTimer.getColorValue(mProfile->mFillColorHL, APPEAR_TIME));

	// Draw the expand image
	if (entry->subMenu && mValidProfileBitmap)
	{
		RectI expandImgRc(entry->expandImgRect);
		expandImgRc.point += offset;

		dglDrawBitmapSR(mProfile->mTextureHandle, expandImgRc.point, mProfile->mBitmapArrayRects[4]);
	}

	// Render type
	switch (entry->type)
	{
		case EntryType::TEXT_IMAGE:
		{
		}
		case EntryType::TEXT:
		{
			// Draw the text
			dglDrawText(mProfile->mFont, offset + entry->textPnt, entry->text, mProfile->mFontColors);
			break;
		}
		case EntryType::SEPERATOR:
		{
			// Draw the seperator
			dglDrawLine(offset + entry->seperatorStart, offset + entry->seperatorEnd, mAppearTimer.getColorValue(mProfile->mFontColorHL, APPEAR_TIME));
			break;
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiContextMenuCtrl::onRender(Point2I offset, const RectI& updateRect)
{
	// Determine if the LMB is down
	if (getRoot()->mouseButtonDown() && !getHitscanEntry(getRoot()->getCursorPos()))
	{
		getRootMenu()->closeContextMenu();
		return;
	}

	// Determine mouse over
	if (mMousedOver && mMousedOver->subMenu && !mMousedOver->subMenu->isVisible() && mMouseOverTime != 0 && (Sim::getCurrentTime() - mMouseOverTime) >= EXPAND_TIME)
	{
		getGroup()->addObject(mMousedOver->subMenu);
		mMousedOver->subMenu->refitSelf();

		// Determine the position
		RectI subRc(Point2I((mMousedOver->renderRect.point.x + mMousedOver->renderRect.extent.x + offset.x) - 1, (mMousedOver->renderRect.point.y + offset.y) - ITEM_PADDING), mMousedOver->subMenu->mBounds.extent);
		if (subRc.point.x + subRc.extent.x > Video::getResolution().w)
			subRc.point.x = offset.x + ((mMousedOver->renderRect.point.x - subRc.extent.x) + 1);

		mMousedOver->subMenu->setPosition(subRc.point);
		mMousedOver->subMenu->setVisible(true);
	}

	// Determine more mouse stuff
	if (mOldMousedOver && (Sim::getCurrentTime() - mMOverExpireTime) > EXPAND_EXPIRE_TIME)
	{
		if (mMousedOver != mOldMousedOver)
		{
			mOldMousedOver->subMenu->setMousedOver(NULL);
			mOldMousedOver->subMenu->setVisible(false);
			mOldMousedOver->subMenu->getParent()->removeObject(mOldMousedOver->subMenu);
		}

		mOldMousedOver = NULL;
	}

	// Determine appear timer
	if (mAppearTimer.isRunning() && mAppearTimer.getAmount(APPEAR_TIME) == 1.f)
		mAppearTimer.stop();

	// Render background
	dglDrawRectFill(updateRect, mAppearTimer.getColorValue(mProfile->mFillColor, APPEAR_TIME));

	// Set font color
	dglSetBitmapModulation(mAppearTimer.getColorValue(mProfile->mFontColor, APPEAR_TIME));

	// Render entries
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
		renderEntry((Entry*)(*it), offset, updateRect);

	// Reset font color
	dglClearBitmapModulation();

	// Render border
	if (mProfile->mBorder)
		dglDrawRect(updateRect, mAppearTimer.getColorValue(mProfile->mBorderColor, APPEAR_TIME));

	// Render children
	renderChildControls(offset, updateRect);
}

bool GuiContextMenuCtrl::onWake()
{
	if (!Parent::onWake())
		return false;

	mValidProfileBitmap = mProfile->constructBitmapArray();
	return true;
}

void GuiContextMenuCtrl::onSleep()
{
	Parent::onSleep();

	if (mMousedOver && mMousedOver->subMenu && mMousedOver->subMenu->isVisible())
	{
		mMousedOver->subMenu->setVisible(false);
		mMousedOver->subMenu->getParent()->removeObject(mMousedOver->subMenu);
	}

	if (mOldMousedOver && mOldMousedOver->subMenu && mOldMousedOver->subMenu->isVisible())
	{
		mOldMousedOver->subMenu->setVisible(false);
		mOldMousedOver->subMenu->getParent()->removeObject(mOldMousedOver->subMenu);
	}

	setMousedOver(NULL);
	mOldMousedOver = NULL;
}

bool GuiContextMenuCtrl::OnVisible()
{
	if (!Parent::OnVisible())
		return false;
	
	mAppearTimer.start();
	getRootMenu()->makeFirstResponder(true);
	return true;
}

void GuiContextMenuCtrl::OnInvisible()
{
	mAppearTimer.stop();
	mAppearTimer.reset();
}

void GuiContextMenuCtrl::onRemove()
{
	Parent::onRemove();
}

bool GuiContextMenuCtrl::onAdd()
{
	if (!Parent::onAdd())
		return false;

	return true;
}

void GuiContextMenuCtrl::onLoseFirstResponder()
{
	if (mParent)
		return;

	makeFirstResponder(true);
}

void GuiContextMenuCtrl::onMouseUp(const GuiEvent& event)
{
	if (!mMousedOver)
		return;
	else if (mMousedOver->subMenu)
	{
		mMouseOverTime = Sim::getCurrentTime() - EXPAND_TIME;
		return;
	}

	if (mParent)
	{
		// Get the top-most parent
		onSelect.Invoke(2, mMousedOver->id, mMousedOver->text);
		Con::executef(getRootMenu(), 3, "onSelect", avar("%d", mMousedOver->id), mMousedOver->text);
		setVisible(false);
		getGroup()->removeObject(this);

		// Hide all of the connected context menus
		GuiContextMenuCtrl* parent = this;
		while (parent)
		{
			parent->setVisible(false);
			if (parent->getParent())
				parent->getParent()->removeObject(parent);
			parent = (parent->mParent == NULL ? NULL : parent->mParent->contextMenu);
		}

		return;
	}

	onSelect.Invoke(2, mMousedOver->id, mMousedOver->text);
	Con::executef(this, 3, "onSelect", avar("%d", mMousedOver->id), mMousedOver->text);
	setVisible(false);
	getGroup()->removeObject(this);
}

void GuiContextMenuCtrl::onMouseDown(const GuiEvent& event)
{
}

void GuiContextMenuCtrl::onMouseMove(const GuiEvent& event)
{
	setMousedOver(getHitscanEntry(event.mousePoint));

	// Call this callback on all our parents
	GuiContextMenuCtrl* next = this;
	while (next && next->mParent && (next = next->mParent->contextMenu) != NULL)
		next->onMouseMove(event);
}

void GuiContextMenuCtrl::onMouseEnter(const GuiEvent& event)
{
	setMousedOver(getHitscanEntry(event.mousePoint));
}

void GuiContextMenuCtrl::onMouseLeave(const GuiEvent& event)
{
	if (mMousedOver && mMousedOver->subMenu && mMousedOver->subMenu->isVisible())
	{
		RectI rc(mMousedOver->subMenu->getBounds());
		rc.point = mMousedOver->subMenu->localToGlobalCoord(rc.point);

		if (!rc.pointInRect(event.mousePoint) && !mMousedOver->subMenu->getHitscanEntry(event.mousePoint))
			setMousedOver(NULL);

		return;
	}

	setMousedOver(NULL);
}

bool GuiContextMenuCtrl::onKeyDown(const GuiEvent& event)
{
	processKeyEvent(event);
	return true;
}

bool GuiContextMenuCtrl::onKeyRepeat(const GuiEvent& event)
{
	processKeyEvent(event);
	return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiContextMenuCtrl* GuiContextMenuCtrl::getOpenSubMenu()
{
	return ((mOldMousedOver && mOldMousedOver->subMenu && mOldMousedOver->subMenu->isVisible()) ? mOldMousedOver->subMenu : ((mMousedOver && mMousedOver->subMenu && mMousedOver->subMenu->isVisible()) ? mMousedOver->subMenu : NULL));
}

GuiContextMenuCtrl* GuiContextMenuCtrl::getRootMenu()
{
	GuiContextMenuCtrl* menu = this;
	while (menu->mParent != NULL)
		menu = menu->mParent->contextMenu;

	return menu;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiContextMenuCtrl::clearItems()
{
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		Entry* child = *it;

		// Free variables
		if (child->text)
			dFree(child->text);
		if (child->subMenu)
			child->subMenu->deleteObject();

		// Free thE CHILD
		delete child;
	}

	mList.clear();
	mOldMousedOver = NULL;
	mMousedOver    = NULL;
}

GuiContextMenuCtrl* GuiContextMenuCtrl::addMenu(const char* text)
{
	Entry* newEntry       = new Entry();
	newEntry->contextMenu = this;
	newEntry->index       = mList.size();
	newEntry->subMenu     = new GuiContextMenuCtrl();
	newEntry->id          = -1;
	newEntry->text        = dStrdup(text);
	newEntry->type        = EntryType::TEXT;

	// Register the menu
	if (!newEntry->subMenu->registerObject())
	{
		Con::errorf("ERROR: GuiContextMenuCtrl::addMenu() - Unable to register new menu!");
		dFree(newEntry->text);
		delete newEntry->subMenu;
		delete newEntry;
		return NULL;
	}

	// Setup the object
	newEntry->subMenu->assignName(avar("%d_sub-%d", getId(), newEntry->subMenu->getId()));
	newEntry->subMenu->setControlProfile(mProfile);
	newEntry->subMenu->setVisible(false);
	newEntry->subMenu->mParent = newEntry;

	// Calculate its rects
	calcItemRect(newEntry);

	// Add it to the list
	mList.push_back(newEntry);

	// Done!
	return newEntry->subMenu;
}

void GuiContextMenuCtrl::addItem(const char* text, S32 id)
{
	Entry* newEntry       = new Entry();
	newEntry->contextMenu = this;
	newEntry->index       = mList.size();
	newEntry->id          = id;
	newEntry->text        = dStrdup(text);
	newEntry->type        = EntryType::TEXT;

	// Calculate its rects
	calcItemRect(newEntry);

	// Add it to the list
	mList.push_back(newEntry);
}

void GuiContextMenuCtrl::addSeperator()
{
	Entry* newEntry       = new Entry();
	newEntry->contextMenu = this;
	newEntry->index       = mList.size();
	newEntry->id          = -1;
	newEntry->type        = EntryType::SEPERATOR;

	// Calculate its rects
	calcItemRect(newEntry);

	// Add it to the list
	mList.push_back(newEntry);
}

void GuiContextMenuCtrl::forceRefit()
{
	refitSelf();
}

void GuiContextMenuCtrl::closeContextMenu()
{
	if (getParent() == NULL)
		return;

	setVisible(false);

	if (mParent)
		mParent->contextMenu->mMouseOverTime = 0;

	// Hacky, but the only thing we can do in this scenerio
	char** argv = new char* [2];
	argv[0] = (char*)"remove";
	argv[1] = dStrdup(avar("%d", getParent()->getId()));
	argv[2] = (char*)avar("%d", getId());
	SimConsoleEvent* evt = new SimConsoleEvent(3, (const char**)argv, true);
	dFree(argv[1]);
	delete[] argv;

	S32 ret = Sim::postEvent(getParent(), evt, Sim::getCurrentTime() + 1);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ConsoleMethod(GuiContextMenuCtrl, forceRefit, void, 2, 2, "")
{
	object->forceRefit();
}

ConsoleMethod(GuiContextMenuCtrl, clearItems, void, 2, 2, "")
{
	object->clearItems();
}

ConsoleMethod(GuiContextMenuCtrl, addMenu, S32, 3, 3, "(string text)")
{
	GuiContextMenuCtrl* newObj = object->addMenu(argv[2]);

	return (newObj ? newObj->getId() : -1);
}

ConsoleMethod(GuiContextMenuCtrl, addItem, void, 4, 4, "(string text, int id)")
{
	object->addItem(argv[2], dAtoi(argv[3]));
}

ConsoleMethod(GuiContextMenuCtrl, addSeperator, void, 2, 2, "")
{
	object->addSeperator();
}