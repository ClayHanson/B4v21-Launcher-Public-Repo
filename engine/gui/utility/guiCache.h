#ifndef _GUI_CACHE_H_
#define _GUI_CACHE_H_

#include "console/consoleTypes.h"
#include "dgl/gTexManager.h"
#include "core/tVector.h"
#include "math/mPoint.h"
#include "math/mRect.h"

class GuiControl;
class GuiCache;

class GuiCache
{
	friend class GuiControl;
private: // Data structures
	struct RecordEntry
	{
		// Actual data
		void* ptr;
		U32 size;

		// Cache
		char value[sizeof(U64)];

		// Methods
		inline bool IsDifferent();
		inline void UpdateValue();
	};

private: // Variables
	TextureHandle mTexHandle;
	GuiControl* mControl;
	GBitmap* mBitmap;
	char* mTexName;
	bool mRendering;

	Vector<RecordEntry*> mList;

public: // Base stuff
	GuiCache(GuiControl* parent);
	~GuiCache();

public: // Manipulation methods
	inline bool isPreRendering() { return mRendering; }
	bool recordVariable(void* ptr, U32 size);
	void clearRecords();

protected: // Protected methods
	void updateBitmap();

public: // Methods
	/// Call this in 'virtual void onPreRender();'
	void preRender();

	/// Call this in 'virtual void onRender(Point2I offset, const RectI &updateRect);'
	void render(Point2I& offset, const RectI& updateRect);
};

#endif