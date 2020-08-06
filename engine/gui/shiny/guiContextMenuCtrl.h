#ifndef _GUI_CONTEXT_MENU_CTRL_
#define _GUI_CONTEXT_MENU_CTRL_

#include "game/helpers/CallbackEvent.h"
#include "gui/utility/TimerUtil.h"
#include "gui/core/guiControl.h"
#include "core/tVector.h"

class GuiContextMenuCtrl : public GuiControl
{
	typedef GuiControl Parent;

public: // Data structures
	enum EntryType : U8
	{
		TEXT = 0,
		TEXT_IMAGE,
		SEPERATOR
	};

	struct Entry
	{
		S32 id;
		S32 index;
		EntryType type;
		RectI renderRect;
		RectI expandImgRect;

		// For text
		GuiContextMenuCtrl* contextMenu;
		GuiContextMenuCtrl* subMenu;
		Point2I textPnt;
		char* text;

		// For seperators
		Point2I seperatorStart;
		Point2I seperatorEnd;

		Entry()
		{
			id            = 0;
			index         = 0;
			type          = EntryType::TEXT;
			renderRect    = RectI(0, 0, 0, 0);
			expandImgRect = RectI(0, 0, 0, 0);

			// Text
			contextMenu = NULL;
			subMenu     = NULL;
			textPnt     = Point2I(0, 0);
			text        = NULL;

			// Seperators
			seperatorStart = Point2I(0, 0);
			seperatorEnd   = Point2I(0, 0);
		}
	};

public: // Public variables
	CallbackEvent onSelect;

protected: // Variables
	Vector<Entry*> mList;
	Entry* mMousedOver;
	U32 mMouseOverTime;

	Entry* mOldMousedOver;
	U32 mMOverExpireTime;

	bool mValidProfileBitmap;
	Entry* mParent;

	TimerUtil mAppearTimer;

public: // Base stuff
	GuiContextMenuCtrl();
	~GuiContextMenuCtrl();

	DECLARE_CONOBJECT(GuiContextMenuCtrl);

protected: // Internal methods
	Entry* getHitscanEntry(Point2I point);
	void processKeyEvent(const GuiEvent& event);
	void refitSelf();
	void calcItemRect(Entry* entry);
	void setMousedOver(Entry* entry);
	void renderEntry(Entry* entry, Point2I& offset, const RectI& updateRect);

public: // Inherited methods
	void onRender(Point2I offset, const RectI& updateRect);

	virtual bool onWake();
	virtual void onSleep();
	virtual bool OnVisible();
	virtual void OnInvisible();
	virtual void onRemove();
	bool onAdd();

	virtual void onLoseFirstResponder();
	void onMouseUp(const GuiEvent& event);
	void onMouseDown(const GuiEvent& event);
	void onMouseMove(const GuiEvent& event);
	void onMouseEnter(const GuiEvent& event);
	void onMouseLeave(const GuiEvent& event);

	virtual bool onKeyDown(const GuiEvent& event);
	virtual bool onKeyRepeat(const GuiEvent& event);

public: // Hierarchy
	GuiContextMenuCtrl* getOpenSubMenu();
	GuiContextMenuCtrl* getRootMenu();

public: // Custom methods
	void clearItems();
	GuiContextMenuCtrl* addMenu(const char* text);
	void addItem(const char* text, S32 id);
	void addSeperator();
	void forceRefit();
	void closeContextMenu();
};

#endif