#include "gui/utility/BitmapArray.h"
#include "core/resManager.h"
#include "dgl/gBitmap.h"

BitmapArray::BitmapArray()
{
	mDownloading = false;
	mHandle      = NULL;
}

BitmapArray::~BitmapArray()
{
	if (mHandle)
		mHandle = NULL;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void BitmapArray::scan()
{
	// Clear existing rects
	mRects.clear();

	// Scan the image for rects
	GBitmap* bmp = mHandle.getBitmap();

	// Make sure the texture exists.
	if (!bmp)
		return;

	// If we're currently downloading this thing, then stop here
	if (mDownloading && ((TextureObject*)mHandle)->downloading)
	{
		mRects.push_back(RectI(0, 0, mHandle.getWidth(), mHandle.getHeight()));
		return;
	}

	//get the separator color
	ColorI sepColor;
	if (!bmp || !bmp->getColor(0, 0, sepColor))
	{
		Con::errorf("Failed to create bitmap array from %s - couldn't ascertain seperator color!", (*mHandle).texFileName);
		mRects.push_back(RectI(0, 0, mHandle.getWidth(), mHandle.getHeight()));
		return;
	}

	//now loop through all the scroll pieces, and find the bounding rectangle for each piece in each state
	S32 curY = 0;

	// ascertain the height of this row...
	ColorI color;
	while (curY < bmp->getHeight())
	{
		// skip any sep colors
		bmp->getColor(0, curY, color);
		if (color == sepColor)
		{
			curY++;
			continue;
		}

		// ok, process left to right, grabbing bitmaps as we go...
		S32 curX = 0;
		while (curX < bmp->getWidth())
		{
			bmp->getColor(curX, curY, color);
			if (color == sepColor)
			{
				curX++;
				continue;
			}

			S32 startX = curX;
			while (curX < bmp->getWidth())
			{
				bmp->getColor(curX, curY, color);
				if (color == sepColor)
					break;
				curX++;
			}

			S32 stepY = curY;
			while (stepY < bmp->getHeight())
			{
				bmp->getColor(startX, stepY, color);
				if (color == sepColor)
					break;
				stepY++;
			}

			mRects.push_back(RectI(startX, curY, curX - startX, stepY - curY));
		}

		// ok, now skip to the next separation color on column 0
		while (curY < bmp->getHeight())
		{
			bmp->getColor(0, curY, color);
			if (color == sepColor)
				break;

			curY++;
		}
	}
	
	if (mRects.size() == 0)
		mRects.push_back(RectI(0, 0, mHandle.getWidth(), mHandle.getHeight()));
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool BitmapArray::set(const char* textureName)
{
	if (textureName && *textureName)
	{
		mHandle = TextureHandle(textureName, BitmapKeepTexture, true);
		scan();

		TextureObject* tO = (TextureObject*)mHandle;
		mDownloading      = tO ? tO->downloading : false;

		if (!mRects.size())
			return false;
	}
	else
		mHandle = NULL;

	return true;
}

bool BitmapArray::set(TextureObject* texture)
{
	if (texture)
	{
		mHandle = TextureHandle(texture);
		scan();

		TextureObject* tO = (TextureObject*)mHandle;
		mDownloading      = tO ? tO->downloading : false;

		if (!mRects.size())
			return false;
	}
	else
		mHandle = NULL;

	return true;
}

TextureObject* BitmapArray::get()
{
	return mHandle;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void BitmapArray::update()
{
	if (mDownloading && (TextureObject*)mHandle != NULL && !((TextureObject*)(mHandle))->downloading)
	{
		// It finished downloading; Update rects
		mDownloading = false;
		scan();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

RectI BitmapArray::operator[](int x)
{
	return mRects.size() == 0 ? RectI(0, 0, 0, 0) : mRects[x % mRects.size()];
}

RectI BitmapArray::at(S32 index)
{
	return mRects.size() == 0 ? RectI(0, 0, 0, 0) : mRects[index % mRects.size()];
}

S32 BitmapArray::size()
{
	return mRects.size();
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

BitmapArray::iterator BitmapArray::begin()
{
	return mRects.begin();
}

BitmapArray::iterator BitmapArray::end()
{
	return mRects.end();
}