//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "dgl/dgl.h"

#include "gui/game/guiProgressCtrl.h"

IMPLEMENT_CONOBJECT(GuiProgressCtrl);

GuiProgressCtrl::GuiProgressCtrl()
{
   mProgress = 0.0f;
   mLoadAnim = 0;
}

const char* GuiProgressCtrl::getScriptValue()
{
   char * ret = Con::getReturnBuffer(64);
   dSprintf(ret, 64, "%g", mProgress);
   return ret;
}

void GuiProgressCtrl::setScriptValue(const char *value)
{
   //set the value
   if (! value)
      mProgress = 0.0f;
   else
      mProgress = dAtof(value);

   //validate the value
   mLoadAnim = (mProgress == -1.0f);
   mProgress = mClampF(mProgress, 0.f, 1.f);
   setUpdate();
}

void GuiProgressCtrl::onPreRender()
{
   const char * var = getVariable();
   if(var)
   {
      F32 value = mClampF(dAtof(var), 0.f, 1.f);
      if(value != mProgress)
      {
         mProgress = value;
         setUpdate();
      }
   }
}

#define LOAD_ANIM_FULL_LOOP_TIME 4000
#define LOAD_ANIM_BAR_WIDTH (mBounds.extent.x / 4)

void GuiProgressCtrl::onRender(Point2I offset, const RectI &updateRect) {
	RectI ctrlRect(offset, mBounds.extent);

	//draw the progress
	if (!mLoadAnim) {
		S32 width = (S32)((F32)mBounds.extent.x * mProgress);

		if (width > 0) {
			RectI progressRect = ctrlRect;
			progressRect.extent.x = width;
			dglDrawRectFill(progressRect, mProfile->mFillColor);
		}
	} else {
		S32 time  = Sim::getCurrentTime() % LOAD_ANIM_FULL_LOOP_TIME;
		S32 start = -LOAD_ANIM_BAR_WIDTH + S32(F32((F32)time / (F32)LOAD_ANIM_FULL_LOOP_TIME) * (F32)(mBounds.extent.x + LOAD_ANIM_BAR_WIDTH));
		U32 width = LOAD_ANIM_BAR_WIDTH;

		if (width > 0) {
			RectI progressRect = ctrlRect;

			progressRect.point.x += start;

			progressRect.extent.x = width;
			dglDrawRectFill(progressRect, mProfile->mFillColor);
		}
	}

	//now draw the border
	if (mProfile->mBorder)
		dglDrawRect(ctrlRect, mProfile->mBorderColor);

	Parent::onRender(offset, updateRect);

	//render the children
	renderChildControls(offset, updateRect);
}