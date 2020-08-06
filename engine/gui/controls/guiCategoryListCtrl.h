#ifndef _GUI_CATEGORY_LIST_CTRL_H_
#define _GUI_CATEGORY_LIST_CTRL_H_

#include "gui/core/guiControl.h"

#define ANIM_MOUSEOVER_BRIGHTEN_TIME 1200

class GuiCategoryListCtrl : public GuiControl
{
private:
	typedef GuiControl Parent;

public:
	struct Entry
	{
		// Linking & heritage
		Vector<Entry*> children;
		U32 AllListIndex;
		Entry* parent;

		// Data
		StringTableEntry id;
		char* text;
		bool expandedInHierarchy;
		bool expanded;
		bool visible;

		// Cache
		GuiCategoryListCtrl* list;

		// Appearance Data
		ColorI selectColor;
		ColorI bgColor;
		ColorI fgColor;
		F32 yOffset;
		F32 oldYOffset;

		// Timers
		S32 expandTimer;

		Entry(GuiCategoryListCtrl* p)
		{
			expandedInHierarchy = true;
			AllListIndex = 0;
			parent       = NULL;
			id           = NULL;
			text         = NULL;
			visible      = true;
			expanded     = true;
			bgColor      = ColorI(0, 0, 0, 0);
			fgColor      = ColorI(0, 0, 0, 255);
			selectColor  = ColorI(126, 214, 255, 255);
			expandTimer  = Sim::getCurrentTime() - ANIM_MOUSEOVER_BRIGHTEN_TIME;
			oldYOffset   = 0;
			yOffset      = 0;
			list         = p;
		}

		S32 GetTotalHeight();
		RectI GetRect();
		void SetExpanded(bool val);
		void UpdateExpanded();
	};

	Vector<Entry*> mAllEntries;
	Vector<Entry*> mList;
	S32 mCurrentlySelected;
	S32 mEntryPadding;

	Entry* mMousedOver;
	S32 mMousedOverTime;

	bool mCanDragEntries;

	bool mDragging;
	S32 mStartDragIndex;
	S32 mDragIndex;

	bool mRefitContent;

	Point2I mMouseDownPos;

public: // Configuration
	S32 entryIndentStepSize;

public:
	GuiCategoryListCtrl();
	~GuiCategoryListCtrl();

	DECLARE_CONOBJECT(GuiCategoryListCtrl);
	static void initPersistFields();

protected: // Protected methods
	Entry* findEntryById(const char* id);
	void removeEntry(Entry* OurEntry);
	void setEntryChildIndex(Entry* OurEntry, int newIndex);
	S32 getEntryChildIndex(Entry* OurEntry);

public: // Rendering methods
	void renderEntry(Entry* entry, Point2I& offset, const RectI& updateRect);
	void onRender(Point2I offset, const RectI& updateRect);

public: // Simobject stuff
	bool onWake();

public: // Custom methods
	void fitContent();
	void stopTransition(S32& offset, Entry* entry = NULL);
	bool addEntry(const char* id, const char* text, const char* parentId);
	void removeEntry(const char* id);
	S32 getEntryChildrenCount(const char* id);
	S32 getEntryCount();
	const Entry* getEntry(S32 idx);
	const char* getEntryText(const char* id);
	void setEntryText(const char* id, const char* newText);
	const Entry* getEntryParent(const char* id);
	void setEntryParent(const char* id, const char* newParent);
	bool isEntryExpanded(const char* id);
	void setEntryExpanded(const char* id, bool value);
	bool isEntryVisible(const char* id);
	void setEntryVisible(const char* id, bool value);
	void setEntryColors(const char* id, ColorI fgColor, ColorI bgColor = ColorI(0, 0, 0, 0), ColorI selectColor = ColorI(126, 214, 255, 255));
	StringTableEntry getSelectedEntryId();
	void clearEntries();

	S32 determineDropIndex(Point2I mousePos);
	RectI getEntryRect(const char* id);
	void dumpEntries();

public: // Input methods
	Entry* getHitscanEntry(Point2I point);
	void setMousedOver(Entry* entry);
	void onMouseUp(const GuiEvent& event);
	void onMouseDown(const GuiEvent& event);
	void onMouseMove(const GuiEvent& event);
	void onMouseDragged(const GuiEvent& event);
	void onMouseEnter(const GuiEvent& event);
	void onMouseLeave(const GuiEvent& event);
	bool onKeyDown(const GuiEvent& event);
	void onRightMouseDown(const GuiEvent& event);
};

#endif