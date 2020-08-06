#ifndef _BITMAP_ARRAY_H_
#define _BITMAP_ARRAY_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _MPOINT_H_
#include "math/mPoint.h"
#endif
#ifndef _MRECT_H_
#include "math/mRect.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif
#ifndef _GTEXMANAGER_H_
#include "dgl/gTexManager.h"
#endif


class BitmapArray
{
public:
	typedef RectI* iterator;

private:
	TextureHandle mHandle;
	Vector<RectI> mRects;
	bool mDownloading;

public:
	BitmapArray();
	~BitmapArray();

protected:
	void scan();

public:
	bool set(const char* textureName);
	bool set(TextureObject* obj);
	TextureObject* get();

public:
	void update();

public:
	RectI operator[](int x);
	RectI at(S32 index);
	S32 size();

public:
	iterator begin();
	iterator end();
};

#endif