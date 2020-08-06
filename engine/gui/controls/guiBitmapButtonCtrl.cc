//-----------------------------------------------------------------------------
// Torque Game Engine
//
// Copyright (c) 2001 GarageGames.Com
//-----------------------------------------------------------------------------


//-------------------------------------
//
// Bitmap Button Contrl
// Set 'bitmap' comsole field to base name of bitmaps to use.  This control will
// append '_n' for normal
// append '_h' for hilighted
// append '_d' for depressed
//
// if bitmap cannot be found it will use the default bitmap to render.
//
// if the extent is set to (0,0) in the gui editor and appy hit, this control will
// set it's extent to be exactly the size of the normal bitmap (if present)
//


#include "console/console.h"
#include "dgl/dgl.h"
#include "console/consoleTypes.h"
#include "platform/platformAudio.h"
#include "gui/core/guiCanvas.h"
#include "gui/core/guiDefaultControlRender.h"
#include "gui/controls/guiBitmapButtonCtrl.h"

IMPLEMENT_CONOBJECT(GuiBitmapButtonCtrl);

//-------------------------------------
GuiBitmapButtonCtrl::GuiBitmapButtonCtrl() : mColor(255, 255, 255, 255)
{
	mIconBitmapName = StringTable->insert("");
	mIconIndex      = 0;
	mBitmapName     = StringTable->insert("");
	mBounds.extent.set(140, 30);
}


//-------------------------------------
void GuiBitmapButtonCtrl::initPersistFields()
{
   Parent::initPersistFields();
   addField("bitmap", TypeFilename, Offset(mBitmapName, GuiBitmapButtonCtrl));
   addField("icons", TypeFilename, Offset(mIconBitmapName, GuiBitmapButtonCtrl));
   addField("iconIndex", TypeS32, Offset(mIconIndex, GuiBitmapButtonCtrl));
   addField("color", TypeColorI, Offset(mColor, GuiBitmapButtonCtrl));
}


//-------------------------------------
bool GuiBitmapButtonCtrl::onWake()
{
   if (! Parent::onWake())
      return false;

   setActive(true);
   setBitmap(mBitmapName);
   mIcons.set(mIconBitmapName);
   return true;
}


//-------------------------------------
void GuiBitmapButtonCtrl::onSleep()
{
   mTextureNormal = NULL;
   mTextureHilight = NULL;
   mTextureDepressed = NULL;
   mIcons.set((const char*)NULL);
   Parent::onSleep();
}


//-------------------------------------

ConsoleMethod( GuiBitmapButtonCtrl, setBitmap, void, 3, 3, "(filepath name)")
{
   object->setBitmap(argv[2]);
}

//-------------------------------------
void GuiBitmapButtonCtrl::inspectPostApply()
{
   // if the extent is set to (0,0) in the gui editor and appy hit, this control will
   // set it's extent to be exactly the size of the normal bitmap (if present)
   Parent::inspectPostApply();

   if ((mBounds.extent.x == 0) && (mBounds.extent.y == 0) && mTextureNormal)
   {
      TextureObject *texture = (TextureObject *) mTextureNormal;
      mBounds.extent.x = texture->bitmapWidth;
      mBounds.extent.y = texture->bitmapHeight;
   }
}


//-------------------------------------
void GuiBitmapButtonCtrl::setBitmap(const char *name)
{
   mBitmapName = StringTable->insert(name);
   if(!isAwake())
      return;

   if (*mBitmapName)
   {
      char buffer[1024];
      char *p;
      dStrcpy(buffer, name);
      p = buffer + dStrlen(buffer);

      mTextureNormal = TextureHandle(buffer, BitmapTexture, true);
      if (!mTextureNormal)
      {
         dStrcpy(p, "_n");
         mTextureNormal = TextureHandle(buffer, BitmapTexture, true);
      }
      dStrcpy(p, "_h");
      mTextureHilight = TextureHandle(buffer, BitmapTexture, true);
      if (!mTextureHilight)
         mTextureHilight = mTextureNormal;
      dStrcpy(p, "_d");
      mTextureDepressed = TextureHandle(buffer, BitmapTexture, true);
      if (!mTextureDepressed)
         mTextureDepressed = mTextureHilight;
      dStrcpy(p, "_i");
      mTextureInactive = TextureHandle(buffer, BitmapTexture, true);
      if (!mTextureInactive)
         mTextureInactive = mTextureNormal;
   }
   else
   {
      mTextureNormal = NULL;
      mTextureHilight = NULL;
      mTextureDepressed = NULL;
      mTextureInactive = NULL;
   }
   setUpdate();
}


//-------------------------------------
void GuiBitmapButtonCtrl::onRender(Point2I offset, const RectI& updateRect) {
	enum {
		NORMAL,
		HILIGHT,
		DEPRESSED,
		INACTIVE
	} state = NORMAL;

	if (mActive)
	{
		if (mMouseOver) state = HILIGHT;
		if (mDepressed || mStateOn) state = DEPRESSED;
	}
	else
		state = INACTIVE;

	ColorI outlineColor;
	switch (state) {
		case NORMAL: {
			renderButton(mTextureNormal, offset, updateRect);
			dglSetBitmapModulation(mProfile->mFontColor);
			outlineColor = mProfile->mOutlineColor;
			break;
		}
		case HILIGHT: {
			renderButton(mTextureHilight ? mTextureHilight : mTextureNormal, offset, updateRect);
			dglSetBitmapModulation(mProfile->mFontColorHL);
			outlineColor = mProfile->mOutlineColorHL;
			break;
		}
		case DEPRESSED: {
			renderButton(mTextureDepressed, offset, updateRect);
			dglSetBitmapModulation(mProfile->mFontColorSEL);
			outlineColor = mProfile->mOutlineColorSEL;
			break;
		}
		case INACTIVE: {
			renderButton(mTextureInactive ? mTextureInactive : mTextureNormal, offset, updateRect);
			dglSetBitmapModulation(mProfile->mFontColorNA);
			outlineColor = mProfile->mOutlineColorNA;
			break;
		}
	}

	if (*mButtonText)
		renderJustifiedText(offset, mBounds.extent, (char*)mButtonText, &outlineColor);

	// Stop here if there are no icons
	if (mIcons.get())
	{
		RectI rc = mIcons[mIconIndex];

		Point2I off(offset);
		off.x += (mBounds.extent.x / 2) - (rc.extent.x / 2);
		off.y += (mBounds.extent.y / 2) - (rc.extent.y / 2);

		dglDrawBitmapSR(mIcons.get(), off, rc);
	}
}

//------------------------------------------------------------------------------

void GuiBitmapButtonCtrl::renderButton(TextureHandle &texture, Point2I &offset, const RectI& updateRect)
{
   if (texture)
   {
      RectI rect(offset, mBounds.extent);
	  dglSetBitmapModulation(mColor);
      dglDrawBitmapStretch(texture, rect);
	  dglClearBitmapModulation();
      renderChildControls(offset, updateRect);
   }
   else
   {
	   dglSetBitmapModulation(mColor);
	   Parent::onRender(offset, updateRect);
   }
}

//------------------------------------------------------------------------------
IMPLEMENT_CONOBJECT(GuiBitmapButtonTextCtrl);

void GuiBitmapButtonTextCtrl::onRender(Point2I offset, const RectI& updateRect)
{
   enum {
      NORMAL,
      HILIGHT,
      DEPRESSED,
      INACTIVE
   } state = NORMAL;

   if (mActive)
   {
      if (mMouseOver) state = HILIGHT;
      if (mDepressed || mStateOn) state = DEPRESSED;
   }
   else
      state = INACTIVE;

   ColorI fontColor = mProfile->mFontColor;

   TextureHandle texture;

   switch (state)
   {
      case NORMAL:
         texture = mTextureNormal;
         fontColor = mProfile->mFontColor;
         break;
      case HILIGHT:
         texture = mTextureHilight;
         fontColor = mProfile->mFontColorHL;
         break;
      case DEPRESSED:
         texture = mTextureDepressed;
         fontColor = mProfile->mFontColorSEL;
         break;
      case INACTIVE:
         texture = mTextureInactive;
         fontColor = mProfile->mFontColorNA;
         if(!texture)
            texture = mTextureNormal;
         break;
   }
   if (texture)
   {
      RectI rect(offset, mBounds.extent);
      dglClearBitmapModulation();
      dglDrawBitmapStretch(texture, rect);

      Point2I textPos = offset;
      if(mDepressed)
         textPos += Point2I(1,1);

      // Make sure we take the profile's textOffset into account.
      textPos += mProfile->mTextOffset;

      dglSetBitmapModulation( fontColor );
      renderJustifiedText(textPos, mBounds.extent, mButtonText);

      renderChildControls( offset, updateRect);
   }
   else
      Parent::onRender(offset, updateRect);
}
