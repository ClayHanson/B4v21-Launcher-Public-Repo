//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/consoleTypes.h"
#include "console/console.h"
#include "dgl/dgl.h"
#include "gui/controls/guiCategoryListCtrl.h"
#include "gui/containers/guiScrollCtrl.h"
#include "gui/core/guiDefaultControlRender.h"

extern U32 timeDelta;

IMPLEMENT_CONOBJECT(GuiCategoryListCtrl);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Helpers

static F32 MoveTowards(F32 CURRENT, F32 TARGET, F32 DELTA)
{
	F32 REALDELTA = (F32)DELTA / 1000.f;
	if (mFabs(TARGET - CURRENT) <= REALDELTA)
		return TARGET;

	return CURRENT + ((TARGET - CURRENT < 0.f ? -1.f : 1.f) * REALDELTA);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

S32 GuiCategoryListCtrl::Entry::GetTotalHeight()
{
	S32 ret = GetRect().extent.y;
	if (!expanded)
		return ret;

	// Get the lowest point of our children
	S32 lowest = -1;
	for (Vector<Entry*>::iterator it = children.begin(); it != children.end(); it++)
	{
		Entry* child  = *it;
		RectI childRc = child->GetRect();

		// Skip invalid entries
		if (!child->visible || (!expanded && child->yOffset == yOffset))
			continue;

		if (childRc.point.y + childRc.extent.y >= lowest)
			lowest = childRc.point.y + childRc.extent.y;
	}

	// Skip invalid lowest point
	if (lowest == -1)
		return ret;

	// Add the lowest Y offset to our score
	return ret + (lowest - GetRect().point.y);
}

RectI GuiCategoryListCtrl::Entry::GetRect()
{
	return RectI(list->localToGlobalCoord(list->mBounds.point).x,
		list->localToGlobalCoord(list->mBounds.point).y + (S32)yOffset,
		list->mBounds.extent.x,
		list->mProfile->mFont->getHeight() + (list->mEntryPadding * 2));
}

void GuiCategoryListCtrl::Entry::SetExpanded(bool val)
{
	expanded         = val;
	expandTimer      = Sim::getCurrentTime();
	UpdateExpanded();
}

void GuiCategoryListCtrl::Entry::UpdateExpanded()
{
	expandedInHierarchy = (parent ? parent->expanded : true);
	for (Vector<Entry*>::iterator it = children.begin(); it != children.end(); it++)
	{
		((Entry*)(*it))->UpdateExpanded();
		((Entry*)(*it))->yOffset = yOffset;
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiCategoryListCtrl::GuiCategoryListCtrl()
{
	VECTOR_SET_ASSOCIATION(mList);
	mCurrentlySelected  = -1;
	mStartDragIndex     = 0;
	entryIndentStepSize = 10;
	mEntryPadding       = 1;
	mDragIndex          = 0;
	mActive             = true;
	mCanDragEntries     = true;
	mRefitContent       = false;
	mDragging           = false;
	mMouseDownPos       = Point2I(0, 0);
}

GuiCategoryListCtrl::~GuiCategoryListCtrl()
{
	clearEntries();
}

void GuiCategoryListCtrl::initPersistFields()
{
	Parent::initPersistFields();
	addField("canDragEntries", TypeS32, Offset(mCanDragEntries, GuiCategoryListCtrl));
	addField("entryIndentStepSize", TypeS32, Offset(entryIndentStepSize, GuiCategoryListCtrl));
	addField("entryPadding", TypeS32, Offset(mEntryPadding, GuiCategoryListCtrl));
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiCategoryListCtrl::Entry* GuiCategoryListCtrl::findEntryById(const char* id)
{
	if (id == NULL)
		return NULL;

	StringTableEntry ID = StringTable->insert(id);
	for (Vector<Entry*>::iterator it = mAllEntries.begin(); it != mAllEntries.end(); it++)
	{
		Entry* e = *it;
		if (dStricmp(e->id, ID))
			continue;

		return e;
	}

	return NULL;
}

void GuiCategoryListCtrl::removeEntry(Entry* OurEntry)
{
	// Remove ourself from the mAllEntries list
	mAllEntries.erase(OurEntry->AllListIndex);

	// Re-index entries that come after us
	for (int i = OurEntry->AllListIndex; i < mAllEntries.size(); i++)
		mAllEntries[i]->AllListIndex--;

	// Remove ourself from the orphan list (if we're on it)
	if (OurEntry->parent == NULL)
	{
		for (int i = 0; i < mList.size(); i++)
		{
			Entry* e = mList[i];
			if (e != OurEntry)
				continue;

			// Found it! Remove it.
			mList.erase(i);
			break;
		}
	}

	// Remove ourself from our parent's children list
	if (OurEntry->parent != NULL)
	{
		for (int i = 0; i < OurEntry->parent->children.size(); i++)
		{
			Entry* e = OurEntry->parent->children[i];
			if (e != OurEntry)
				continue;

			// Found it! Remove it.
			OurEntry->parent->children.erase(i);
			break;
		}

		OurEntry->parent = NULL;
	}

	// Remove our children from ourself
	if (OurEntry->children.size() != 0)
	{
		for (Vector<Entry*>::iterator it = OurEntry->children.begin(); it != OurEntry->children.end(); it++)
		{
			Entry* child = *it;

			// Orphan this child
			mList.push_back(child);
			child->parent = NULL;
		}

		OurEntry->children.clear();
	}

	if (mMousedOver == OurEntry)
		mMousedOver = NULL;

	// Free up memory
	dFree(OurEntry->text);
	delete OurEntry;

	// Resize ourself
	fitContent();
}

void GuiCategoryListCtrl::setEntryChildIndex(Entry* OurEntry, int newIndex)
{
	// Get the parent vector
	Vector<Entry*> newList;
	Vector<Entry*>* vec = (OurEntry->parent ? &OurEntry->parent->children : &mList);
	newIndex            = mClamp(newIndex, 0, vec->size() - 1);

	// Get its index, remove it & re-add it.
	for (Vector<Entry*>::iterator it = vec->begin(); it != vec->end(); it++)
	{
		Entry* child = *it;
		if (child == OurEntry)
		{
			if (newIndex == (it - vec->begin()))
				return;

			continue;
		}

		// Push ourselves if it's our new index
		if (newIndex == (it - vec->begin()))
			newList.push_back(OurEntry);

		// Push the child back
		newList.push_back(child);
	}

	// Repopulate the vec
	vec->clear();
	for (Vector<Entry*>::iterator it = newList.begin(); it != newList.end(); it++)
		vec->push_back(*it);
}

S32 GuiCategoryListCtrl::getEntryChildIndex(Entry* OurEntry)
{
	Vector<Entry*>* vec = (OurEntry->parent ? &OurEntry->parent->children : &mList);

	// Get the index
	for (Vector<Entry*>::iterator it = vec->begin(); it != vec->end(); it++)
	{
		if (((Entry*)(*it)) != OurEntry)
			continue;

		return it - vec->begin();
	}

	// Not in this index..?
	return -1;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiCategoryListCtrl::renderEntry(Entry* entry, Point2I& offset, const RectI& updateRect)
{
	// 0 = Collapsed (closed), 1 = Expanded (opened)
	if (!entry->visible)
		return;

	// Get rects & positions
	RectI EntryRect(entry->GetRect());
	EntryRect.point.x = offset.x;
	Point2I textOffset(EntryRect.point.x + mEntryPadding, EntryRect.point.y + mEntryPadding);

	Point2I localOffset = globalToLocalCoord(offset);
	if (entry->yOffset != localOffset.y)
		entry->yOffset = MoveTowards(entry->yOffset, localOffset.y, timeDelta * 400);

	if (entry->yOffset != entry->oldYOffset)
	{
		entry->oldYOffset = true;
		mRefitContent     = true;
	}

	if (entry->expanded)
	{
		// Set the offset
		offset.x += entryIndentStepSize;
		offset.y += EntryRect.extent.y;

		// Drag stuff
		bool renderDragger = (mDragging && mMousedOver->parent == entry);

		// Render our children
		for (Vector<Entry*>::iterator it = entry->children.begin(); it != entry->children.end(); it++)
		{
			Entry* child = *it;

			// Render the drag reticle
			if (renderDragger && mDragIndex == (it - entry->children.begin()))
			{
				RectI childRect = child->GetRect();

				// Draw it
				dglDrawLine(Point2I(childRect.point.x + 2, childRect.point.y), Point2I((childRect.point.x + childRect.extent.x) - 4, childRect.point.y), ColorI(0, 0, 0, 255));
			}

			// Render the child
			renderEntry(child, offset, updateRect);
		}

		// Reset the offset
		offset.x -= entryIndentStepSize;
		offset.y -= EntryRect.extent.y;
	}

	// Render the background, first
	if (entry->bgColor.alpha != 0)
		dglDrawRectFill(EntryRect, entry->bgColor);

	// Draw the "highlight" rect
	if (mMousedOver == entry)
	{
		F32 amt = (0.5f + (0.5f * mCos(M_PI_F * (F32(Sim::getCurrentTime() % 500) / 250.f))));
		dglDrawRectFill(EntryRect, ColorI(entry->selectColor.red, entry->selectColor.green, entry->selectColor.blue, 50 + U8(100.f * amt)));
	}
	else if (mCurrentlySelected == entry->AllListIndex)
	{
		dglDrawRectFill(EntryRect, ColorI(entry->selectColor.red, entry->selectColor.green, entry->selectColor.blue, 100));
	}

	// Set foreground modulation
	dglSetBitmapModulation(entry->fgColor);

	// Next, draw the arrow (if we have any children)
	if (mProfile->mBitmapArrayRects.size() >= 2 && entry->children.size() != 0)
	{
		RectI* arrRect = (entry->expanded ? &mProfile->mBitmapArrayRects[1] : &mProfile->mBitmapArrayRects[0]);

		dglDrawBitmapSR(mProfile->mTextureHandle, Point2I(textOffset.x, EntryRect.point.y + ((EntryRect.extent.y / 2) - (arrRect->extent.y / 2))), *arrRect);
		textOffset.x += arrRect->extent.x + 2;
	}

	// Draw the text
	dglDrawText(mProfile->mFont, textOffset, entry->text, mProfile->mFontColors);
	
	// Clear modulation
	dglClearBitmapModulation();

	// Offset it
	offset.y += EntryRect.extent.y;
}

void GuiCategoryListCtrl::onRender(Point2I offset, const RectI& updateRect)
{
	mRefitContent = false;

	// Render orphan'd entries
	Point2I entryOffset(offset);
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
		renderEntry((Entry*)(*it), entryOffset, updateRect);

	if (mRefitContent)
		fitContent();

	// Render children
	renderChildControls(offset, updateRect);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool GuiCategoryListCtrl::onWake()
{
	if (!Parent::onWake())
		return false;

	mProfile->constructBitmapArray();

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiCategoryListCtrl::fitContent()
{
	S32 h = 0;
	for (Vector<Entry*>::iterator it = mList.begin(); it != mList.end(); it++)
	{
		Entry* e   = *it;
		S32 height = e->yOffset + e->GetTotalHeight();

		if (height > h)
			h = height;
	}

	setHeight(h);
}

void GuiCategoryListCtrl::stopTransition(S32& offset, Entry* entry)
{
	Vector<Entry*>* vec = &mList;

	if (entry != NULL)
		vec = &entry->children;

	// Get offset from children
	for (Vector<Entry*>::iterator it = vec->begin(); it != vec->end(); it++)
	{
		Entry* e   = *it;
		e->yOffset = offset;
		offset    += (e->expandedInHierarchy && e->visible ? e->GetRect().extent.y : 0);

		stopTransition(offset, e);
	}

	if (entry == NULL)
		fitContent();
}

bool GuiCategoryListCtrl::addEntry(const char* id, const char* text, const char* parentId)
{
	// Sanity check
	if (id == NULL || *id == 0)
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::addEntry() - Invalid ID name \"%s\"; Not accepting entry", id);
		return false;
	}

	Entry* ParentEntry = findEntryById(parentId);
	Entry* OurEntry    = findEntryById(id);

	// Safety check
	if (OurEntry != NULL)
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::addEntry() - An entry already exists with the ID \"%s\"!", id);
		return false;
	}
	
	// Safety check
	if (parentId != NULL && ParentEntry == NULL)
		Con::warnf("WARNING: GuiCategoryListCtrl::addEntry() - Failed to find the parent entry with an ID of \"%s\"", parentId);

	// Create the entry
	OurEntry               = new Entry(this);
	OurEntry->yOffset      = mAllEntries.size() > 0 ? mAllEntries.front()->yOffset + mAllEntries.front()->GetTotalHeight() : 0;
	OurEntry->AllListIndex = mAllEntries.size();
	OurEntry->parent       = ParentEntry;
	OurEntry->id           = StringTable->insert(id);
	OurEntry->text         = dStrdup(text);

	// Add it to our parent's children vector
	if (ParentEntry)
		ParentEntry->children.push_back(OurEntry);

	// Done. Add it to the lists
	if (OurEntry->parent == NULL)
		mList.push_back(OurEntry);

	mAllEntries.push_back(OurEntry);

	// Resize ourself
	fitContent();
	return true;
}

void GuiCategoryListCtrl::removeEntry(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::removeEntry() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	removeEntry(OurEntry);
}

S32 GuiCategoryListCtrl::getEntryChildrenCount(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::getEntryChildrenCount() - No entry exists with the ID \"%s\"!", id);
		return 0;
	}

	return OurEntry->children.size();
}

S32 GuiCategoryListCtrl::getEntryCount()
{
	return mAllEntries.size();
}

const GuiCategoryListCtrl::Entry* GuiCategoryListCtrl::getEntry(S32 idx)
{
	if (idx < 0 || idx >= mAllEntries.size())
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::getEntry() - Index out of range; index = %d, size = %d", idx, mAllEntries.size());
		return NULL;
	}

	return mAllEntries[idx];
}

const char* GuiCategoryListCtrl::getEntryText(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("ERROR: GuiCategoryListCtrl::getEntryText() - No entry exists with the ID \"%s\"!", id);
		return "";
	}

	return OurEntry->text;
}

void GuiCategoryListCtrl::setEntryText(const char* id, const char* newText)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryText() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	dFree(OurEntry->text);
	OurEntry->text = dStrdup(newText);
}

const GuiCategoryListCtrl::Entry* GuiCategoryListCtrl::getEntryParent(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::getEntryParent() - No entry exists with the ID \"%s\"!", id);
		return NULL;
	}

	return OurEntry->parent;
}

void GuiCategoryListCtrl::setEntryParent(const char* id, const char* newParent)
{
	Entry* ParentEntry = findEntryById(newParent);
	Entry* OurEntry    = findEntryById(id);

	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryParent() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	if (ParentEntry == OurEntry->parent)
		return;

	// Remove ourself from the orphan list (if we're on it)
	if (OurEntry->parent == NULL)
	{
		for (int i = 0; i < mList.size(); i++)
		{
			Entry* e = mList[i];
			if (e != OurEntry)
				continue;

			// Found it! Remove it.
			mList.erase(i);
			break;
		}
	}

	// Remove ourself from our previous parent's children list
	if (OurEntry->parent != NULL)
	{
		for (int i = 0; i < OurEntry->parent->children.size(); i++)
		{
			Entry* e = OurEntry->parent->children[i];
			if (e != OurEntry)
				continue;

			// Found it! Remove it.
			OurEntry->parent->children.erase(i);
			break;
		}

		OurEntry->parent = NULL;
	}

	// Update the expanded variable
	OurEntry->UpdateExpanded();

	// Set our new parent
	OurEntry->parent = ParentEntry;
	if (ParentEntry)
		ParentEntry->children.push_back(OurEntry);
	else
		mList.push_back(OurEntry);
}

bool GuiCategoryListCtrl::isEntryExpanded(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::isEntryExpanded() - No entry exists with the ID \"%s\"!", id);
		return false;
	}

	return OurEntry->expanded;
}

void GuiCategoryListCtrl::setEntryExpanded(const char* id, bool value)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryExpanded() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	OurEntry->SetExpanded(value);
}

bool GuiCategoryListCtrl::isEntryVisible(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::isEntryVisible() - No entry exists with the ID \"%s\"!", id);
		return false;
	}

	return OurEntry->visible;
}

void GuiCategoryListCtrl::setEntryVisible(const char* id, bool value)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryVisible() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	OurEntry->visible = value;
}

void GuiCategoryListCtrl::setEntryColors(const char* id, ColorI fgColor, ColorI bgColor, ColorI selectColor)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryColors() - No entry exists with the ID \"%s\"!", id);
		return;
	}

	OurEntry->fgColor     = fgColor;
	OurEntry->bgColor     = bgColor;
	OurEntry->selectColor = selectColor;
}

StringTableEntry GuiCategoryListCtrl::getSelectedEntryId()
{
	if (mCurrentlySelected < 0 || mCurrentlySelected >= mAllEntries.size())
		return "";

	return mAllEntries[mCurrentlySelected]->id;
}

void GuiCategoryListCtrl::clearEntries()
{
	for (Vector<Entry*>::iterator it = mAllEntries.begin(); it != mAllEntries.end(); it++)
	{
		Entry* e = *it;

		// Free this entry
		dFree(e->text);
		delete (Entry*)(*it);
	}

	mCurrentlySelected = -1;
	mMousedOver = NULL;
	mAllEntries.clear();
	mList.clear();
}

S32 GuiCategoryListCtrl::determineDropIndex(Point2I mousePos)
{
	if (!mDragging || !mMousedOver)
		return 0;

	// Get the parent vector
	Vector<Entry*> vec = (mMousedOver->parent ? mMousedOver->parent->children : mList);

	// Sanity check
	if (vec.size() == 0)
		return 0;

	// Determine the applicable rect
	RectI applicableRect(
		vec.front()->GetRect().point.x,
		vec.front()->GetRect().point.y,
		vec.front()->GetRect().extent.x,
		(vec[vec.size() - 1]->GetRect().point.y + vec[vec.size() - 1]->GetRect().extent.y) - vec.front()->GetRect().point.y
	);

	// If we're not in the applicable rect, then just jump to the front / back
	if (!applicableRect.pointInRect(mousePos))
		return (mousePos.y < applicableRect.point.y + (applicableRect.extent.y / 2) ? 0 : vec.size() - 1);

	// See if it falls between any entries
	for (Vector<Entry*>::iterator it = vec.begin(); it != vec.end(); it++)
	{
		Entry* e  = *it;
		RectI erc = e->GetRect();

		// Skip invalid entries
		if (!e->visible || !e->expandedInHierarchy || !erc.pointInRect(mousePos))
			continue;

		return (mousePos.y < (erc.point.y + (erc.extent.y / 2)) ? (it - vec.begin()) : (it - vec.begin()) + 1);
	}

	// Failed everything...?
	return 0;
}

RectI GuiCategoryListCtrl::getEntryRect(const char* id)
{
	Entry* OurEntry = findEntryById(id);

	// Safety check
	if (OurEntry == NULL)
	{
		Con::errorf("Error: GuiCategoryListCtrl::setEntryExpanded() - No entry exists with the ID \"%s\"!", id);
		return RectI(0, 0, 0, 0);
	}

	return OurEntry->GetRect();
}

void GuiCategoryListCtrl::dumpEntries()
{
	for (Vector<Entry*>::iterator it = mAllEntries.begin(); it != mAllEntries.end(); it++)
	{
		Entry* e = *it;

		Con::printf("Entry # %04d / %04d | [IDX %04d]:", it - mAllEntries.begin(), mAllEntries.size(), e->AllListIndex);
		Con::printf("  > ID       = \"%s\"", e->id);
		Con::printf("  > FG COLOR = (%03d, %03d, %03d, %03d)", e->fgColor.red, e->fgColor.green, e->fgColor.blue, e->fgColor.alpha);
		Con::printf("  > BG COLOR = (%03d, %03d, %03d, %03d)", e->bgColor.red, e->bgColor.green, e->bgColor.blue, e->bgColor.alpha);
		Con::printf("  > PARENT   = %s", e->parent == NULL ? "<none>" : e->parent->id);
		Con::printf("  > EXPANDED = %s <%d>", e->expanded ? "TRUE" : "FALSE", e->expandTimer);
		Con::printf("  > Y-OFFSET = %f", e->yOffset);
		Con::printf("");
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiCategoryListCtrl::Entry* GuiCategoryListCtrl::getHitscanEntry(Point2I point)
{
	Entry* ret = NULL;
	for (Vector<Entry*>::iterator it = mAllEntries.begin(); it != mAllEntries.end(); it++)
	{
		Entry* e  = *it;
		RectI erc = e->GetRect();

		// Skip invalid entries
		if (!e->visible || !e->expandedInHierarchy || !erc.pointInRect(point))
			continue;

		ret = e;
	}
	
	return ret;
}

void GuiCategoryListCtrl::setMousedOver(Entry* entry)
{
	mMousedOver     = entry;
	mMousedOverTime = Sim::getCurrentTime();
}

void GuiCategoryListCtrl::onMouseMove(const GuiEvent& event)
{
	if (!mDragging)
		setMousedOver(getHitscanEntry(event.mousePoint));
}

void GuiCategoryListCtrl::onMouseUp(const GuiEvent& event)
{
	if (mMousedOver)
	{
		if (mDragging)
		{
			mDragging = false;

			// Set the new index
			setEntryChildIndex(mMousedOver, mDragIndex);

			// Ready some buffers
			char oldIndex[64];
			char newIndex[64];

			// Set it up
			dSprintf(oldIndex, sizeof(oldIndex), "%d", mStartDragIndex);
			dSprintf(newIndex, sizeof(newIndex), "%d", mDragIndex);

			// Call the ending callback
			Con::executef(this, 5, "onDragEnd", mMousedOver->id, mMousedOver->text, oldIndex, newIndex);
		}

		if (mMousedOver->children.size() != 0)
		{
			mMousedOver->SetExpanded(!mMousedOver->expanded);
		}
		else
		{
			mCurrentlySelected = mMousedOver->AllListIndex;
			Con::executef(this, 3, "onSelect", mMousedOver->id, mMousedOver->text);
			if (mMousedOver == NULL)
				mCurrentlySelected = -1;
		}
	}
	else
	{
		mCurrentlySelected = -1;
		mDragging          = false;
	}
}

void GuiCategoryListCtrl::onMouseDown(const GuiEvent& event)
{
	mMouseDownPos = event.mousePoint;
}

void GuiCategoryListCtrl::onMouseDragged(const GuiEvent& event)
{
	if (mMousedOver && mCanDragEntries && !mMousedOver->children.size())
	{
		if (!mDragging)
		{
			float DIST = mSqrt(mPow(mMouseDownPos.x - event.mousePoint.x, 2.f) + mPow(mMouseDownPos.y - event.mousePoint.y, 2.f));
			if (DIST < 4.f)
				return;

			mStartDragIndex = getEntryChildIndex(mMousedOver);
			mDragging       = true;
			Con::executef(this, 4, "onDragStart", mMousedOver->id, mMousedOver->text, avar("%d", mStartDragIndex));
		}

		mDragIndex = determineDropIndex(event.mousePoint);
	}
}

void GuiCategoryListCtrl::onMouseEnter(const GuiEvent& event)
{
	if (!mDragging)
		setMousedOver(getHitscanEntry(event.mousePoint));
}

void GuiCategoryListCtrl::onMouseLeave(const GuiEvent& event)
{
	if (!mDragging)
		setMousedOver(NULL);
}

bool GuiCategoryListCtrl::onKeyDown(const GuiEvent& event)
{
	return false;
}

void GuiCategoryListCtrl::onRightMouseDown(const GuiEvent& event)
{
	if (mMousedOver)
	{
		if (mMousedOver->children.size() != 0)
		{
			mMousedOver->SetExpanded(!mMousedOver->expanded);
		}
		else
		{
			mCurrentlySelected = mMousedOver->AllListIndex;
			Con::executef(this, 3, "onRightClicked", mMousedOver->id, mMousedOver->text);
			if (mMousedOver == NULL)
				mCurrentlySelected = -1;
		}
	}
	else
		mCurrentlySelected = -1;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ConsoleMethod(GuiCategoryListCtrl, addEntry, bool, 4, 5, "(string newId, string text[, string parentId])")
{
	return object->addEntry(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
}

ConsoleMethod(GuiCategoryListCtrl, removeEntry, void, 3, 3, "(string id)")
{
	object->removeEntry(argv[2]);
}

ConsoleMethod(GuiCategoryListCtrl, clearEntries, void, 2, 2, "")
{
	object->clearEntries();
}

ConsoleMethod(GuiCategoryListCtrl, setEntryText, void, 4, 4, "(string id, string text)")
{
	object->setEntryText(argv[2], argv[3]);
}

ConsoleMethod(GuiCategoryListCtrl, getEntryText, const char*, 3, 3, "(string id)")
{
	return object->getEntryText(argv[2]);
}

ConsoleMethod(GuiCategoryListCtrl, getEntryCount, S32, 2, 2, "()")
{
	return object->getEntryCount();
}

ConsoleMethod(GuiCategoryListCtrl, getEntryID, const char*, 3, 3, "(int index)")
{
	const GuiCategoryListCtrl::Entry* e = object->getEntry(dAtoi(argv[2]));
	return e == NULL ? "" : e->id;
}

ConsoleMethod(GuiCategoryListCtrl, getEntryChildrenCount, S32, 3, 3, "(string id)")
{
	return object->getEntryChildrenCount(argv[2]);
}

ConsoleMethod(GuiCategoryListCtrl, setEntryExpanded, void, 4, 4, "(string id, bool value)")
{
	object->setEntryExpanded(argv[2], dAtob(argv[3]));
}

ConsoleMethod(GuiCategoryListCtrl, isEntryExpanded, bool, 3, 3, "(string id)")
{
	return object->isEntryExpanded(argv[2]);
}

ConsoleMethod(GuiCategoryListCtrl, setEntryVisible, void, 4, 4, "(string id, bool value)")
{
	object->setEntryVisible(argv[2], dAtob(argv[3]));
}

ConsoleMethod(GuiCategoryListCtrl, isEntryVisible, bool, 3, 3, "(string id)")
{
	return object->isEntryVisible(argv[2]);
}

ConsoleMethod(GuiCategoryListCtrl, getEntryParent, const char*, 3, 3, "(string id)")
{
	const GuiCategoryListCtrl::Entry* e = object->getEntryParent(argv[2]);
	return e == NULL ? "" : e->id;
}

ConsoleMethod(GuiCategoryListCtrl, setEntryParent, void, 4, 4, "(string id, string newParentId)")
{
	object->setEntryParent(argv[2], *argv[3] == 0 ? NULL : argv[3]);
}

ConsoleMethod(GuiCategoryListCtrl, setEntryColors, void, 5, 6, "(string id, color foreground, color background[, color select])")
{
	ColorI fgColor(255, 255, 255, 255);
	ColorI bgColor(0, 0, 0, 0);
	ColorI selectColor(126, 214, 255, 255);

	int r, g, b, a;

	dSscanf(argv[3], "%d %d %d %d", &r, &g, &b, &a);
	fgColor.red   = r;
	fgColor.green = g;
	fgColor.blue  = b;
	fgColor.alpha = a;

	dSscanf(argv[4], "%d %d %d %d", &r, &g, &b, &a);
	bgColor.red   = r;
	bgColor.green = g;
	bgColor.blue  = b;
	bgColor.alpha = a;

	if (argc == 6)
	{
		dSscanf(argv[5], "%d %d %d %d", &r, &g, &b, &a);
		selectColor.red   = r;
		selectColor.green = g;
		selectColor.blue  = b;
		selectColor.alpha = a;
	}

	object->setEntryColors(argv[2], fgColor, bgColor, selectColor);
}

ConsoleMethod(GuiCategoryListCtrl, dumpEntries, void, 2, 2, "")
{
	object->dumpEntries();
}

ConsoleMethod(GuiCategoryListCtrl, getSelectedRow, const char*, 2, 2, "")
{
	return object->getSelectedEntryId();
}

ConsoleMethod(GuiCategoryListCtrl, fitContent, void, 2, 2, "")
{
	object->fitContent();
}

ConsoleMethod(GuiCategoryListCtrl, stopTransition, void, 2, 2, "()")
{
	S32 offset = 0;
	object->stopTransition(offset);
}