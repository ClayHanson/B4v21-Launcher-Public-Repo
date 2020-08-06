#include "gui/shiny/guiBlurCtrl.h"
#include "gui/core/guiCanvas.h"

IMPLEMENT_CONOBJECT(GuiBlurCtrl);

#define BLUR_OFFSET 2

GuiBlurCtrl::GuiBlurCtrl()
{
	mBlurBitmap     = NULL;
	mBlurAmount     = 0;
	mLastUpdateTime = 0;
	mBlurBufferSize = 0;
	mBlurBuffer     = NULL;
	mTextureObject  = NULL;
	mPassedOnce     = false;
}

void GuiBlurCtrl::initPersistFields()
{
	Parent::initPersistFields();

	addGroup("Blur");
	addField("amount", TypeS32, Offset(mBlurAmount, GuiBlurCtrl));
	endGroup("Blur");
}

void GuiBlurCtrl::onPreRender()
{
	if (!mPassedOnce)
		return;

	if (mTextureObject.isValid() && Sim::getCurrentTime() - mLastUpdateTime < 2000)
		return;
}

void GuiBlurCtrl::onRender(Point2I offset, const RectI& updateRect)
{
	if (!mPassedOnce)
	{
		mPassedOnce = true;
		return;
	}

	if (mBlurAmount != 0 && mPassedOnce && (!mTextureObject.isValid() || Sim::getCurrentTime() - mLastUpdateTime >= 64))
	{
		mLastUpdateTime = Sim::getCurrentTime();
		Point2I extent(mBounds.extent);
		RectI BlurRect(0, 0, extent.x, extent.y);
		U32 iExpectedSize = (extent.x * extent.y) * 3;
		if (mBlurBufferSize < iExpectedSize)
		{
			// Resize the buffer
			if (mBlurBuffer == NULL)
			{
				mBlurBuffer = (U8*)dMalloc(iExpectedSize);
				mBlurBufferSize = iExpectedSize;
			}
			else
			{
				mBlurBuffer = (U8*)dRealloc((void*)mBlurBuffer, iExpectedSize);
				mBlurBufferSize = iExpectedSize;
			}
		}

		// Read the pixels at the positions
		glReadPixels(offset.x, (Canvas->getHeight() - updateRect.extent.y) - offset.y, extent.x, extent.y, GL_RGB, GL_UNSIGNED_BYTE, mBlurBuffer);

		// Create a new bitmap if the old one isn't valid anymore
		if (mBlurBitmap == NULL || mBlurBitmap->getWidth() != extent.x || mBlurBitmap->getHeight() != extent.y)
		{
			if (mBlurBitmap != NULL)
				delete mBlurBitmap;

			mBlurBitmap = new GBitmap();
			mBlurBitmap->allocateBitmap(U32(extent.x), U32(extent.y));
		}

		// flip the rows
		for (U32 y = 0; y < extent.y; y++)
			dMemcpy(mBlurBitmap->getAddress(0, extent.y - y - 1), mBlurBuffer + y * extent.x * 3, U32(extent.x * 3));

		if (!mTextureObject.isValid())
		{
			FileStream fStream;
			if (fStream.open("launcher/test.png", FileStream::Write))
			{
				mBlurBitmap->writePNG(fStream);
				fStream.close();
			}
		}

		mTextureObject = TextureHandle("__GUI_BLUR_CTRL__", mBlurBitmap, TextureHandleType::BitmapKeepTexture, true);
	}

	if (!mTextureObject.isValid())
		return;

	for (int i = -mBlurAmount; i < mBlurAmount; i++)
	{
		dglSetBitmapModulation(ColorF(1.f, 1.f, 1.f, 1.f - F32((F32)mAbs(i) / (F32)mBlurAmount)));
		dglDrawBitmap(mTextureObject, Point2I(updateRect.point.x + (i * 2), updateRect.point.y));
	}

	for (int i = -mBlurAmount; i < mBlurAmount; i++)
	{
		dglSetBitmapModulation(ColorF(1.f, 1.f, 1.f, 1.f - F32((F32)mAbs(i) / (F32)mBlurAmount)));
		dglDrawBitmap(mTextureObject, Point2I(updateRect.point.x, updateRect.point.y + (i * 2)));
	}

	dglClearBitmapModulation();
	renderChildControls(offset, updateRect);
}