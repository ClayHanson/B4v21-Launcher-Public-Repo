//-----------------------------------------------------------------------------
// Torque Engine
//
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "dgl/dgl.h"
#include "console/consoleTypes.h"
#include "platform/platformAudio.h"
#include "gui/core/guiCanvas.h"
#include "gui/controls/guiButtonBaseCtrl.h"
#include "i18n/lang.h"

IMPLEMENT_CONOBJECT(GuiButtonBaseCtrl);

GuiButtonBaseCtrl::GuiButtonBaseCtrl()
{
	mPitch = 1.f;
	mDepressed = false;
	mMouseOver = false;
	mActive = true;
	mButtonText = StringTable->insert("Button");
	mButtonTextID = StringTable->insert("");
	mStateOn = false;
	mRadioGroup = -1;
	mButtonType = ButtonTypePush;
}

bool GuiButtonBaseCtrl::onWake()
{
   if(!Parent::onWake())
      return false;

   // is we have a script variable, make sure we're in sync
   if ( mConsoleVariable[0] )
   	mStateOn = Con::getBoolVariable( mConsoleVariable );
   if(mButtonTextID && *mButtonTextID != 0)
	   setTextID(mButtonTextID);

   return true;
}

ConsoleMethod( GuiButtonBaseCtrl, performClick, void, 2, 2, "() - simulates a button click from script." )
{
   argc; argv;
   object->onAction();
}

ConsoleMethod( GuiButtonBaseCtrl, setText, void, 3, 3, "(string text) - sets the text of the button to the string." )
{
   argc;
   object->setText( argv[2] );
}

ConsoleMethod( GuiButtonBaseCtrl, setTextID, void, 3, 3, "(string id) - sets the text of the button to the localized string." )
{
	argc;
	object->setTextID(argv[2]);
}
ConsoleMethod( GuiButtonBaseCtrl, getText, const char *, 2, 2, "() - returns the text of the button." )
{
   argc; argv;
   return object->getText( );
}
ConsoleMethod( GuiButtonBaseCtrl, setStateOn, void, 3, 3, "(bool isStateOn) - sets the state on member and updates siblings of the same group." )
{
   argc;
   object->setStateOn(dAtob(argv[2]));
}


static EnumTable::Enums buttonTypeEnums[] = 
{
   { GuiButtonBaseCtrl::ButtonTypePush, "PushButton" },
   { GuiButtonBaseCtrl::ButtonTypeCheck, "ToggleButton" },
   { GuiButtonBaseCtrl::ButtonTypeRadio, "RadioButton" },
};
      
static EnumTable gButtonTypeTable(3, &buttonTypeEnums[0]); 
      
      

void GuiButtonBaseCtrl::initPersistFields()
{
   Parent::initPersistFields();
   addGroup("Misc");		
   addField("pitch", TypeF32, Offset(mPitch, GuiButtonBaseCtrl));
   addField("text", TypeCaseString, Offset(mButtonText, GuiButtonBaseCtrl));
   addField("textID", TypeString, Offset(mButtonTextID, GuiButtonBaseCtrl));
   addField("groupNum", TypeS32, Offset(mRadioGroup, GuiButtonBaseCtrl));
   addField("buttonType", TypeEnum, Offset(mButtonType, GuiButtonBaseCtrl), 1, &gButtonTypeTable);
   endGroup("Misc");		
}

void GuiButtonBaseCtrl::setText(const char *text)
{
   mButtonText = StringTable->insert(text);
}

void GuiButtonBaseCtrl::setStateOn( bool bStateOn )
{
   if(!mActive)
      return;

   if(mButtonType == ButtonTypeCheck)
   {
      mStateOn = bStateOn;
   }
   else if(mButtonType == ButtonTypeRadio)
   {
      messageSiblings(mRadioGroup);
      mStateOn = bStateOn;
   }		
   setUpdate();
}

void GuiButtonBaseCtrl::setTextID(const char *id)
{
	S32 n = Con::getIntVariable(id, -1);
	if(n != -1)
	{
		mButtonTextID = StringTable->insert(id);
		setTextID(n);
	}
}
void GuiButtonBaseCtrl::setTextID(S32 id)
{
	const UTF8 *str = getGUIString(id);
	if(str)
		setText((const char*)str);
	//mButtonTextID = id;
}
const char *GuiButtonBaseCtrl::getText()
{
   return mButtonText;
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::acceleratorKeyPress(U32)
{
   if (! mActive)
      return;

   //set the bool
   mDepressed = true;

   if (mProfile->mTabable)
      setFirstResponder();
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::acceleratorKeyRelease(U32)
{
   if (! mActive)
      return;

   if (mDepressed)
   {
      //set the bool
      mDepressed = false;
      //perform the action
      onAction();
   }

   //update
   setUpdate();
}

void GuiButtonBaseCtrl::onMouseDown(const GuiEvent &event)
{
	if (!mActive)
		return;

	if (mProfile->mCanKeyFocus)
		setFirstResponder();

	if (mProfile->mSoundButtonDown) {
		F32 pan = (F32(event.mousePoint.x) / F32(Canvas->mBounds.extent.x)*2.0f - 1.0f)*0.8f;
		AUDIOHANDLE handle = alxCreateSource(mProfile->mSoundButtonDown);
		alxSourcef(handle, AL_PITCH, mPitch);
		alxPlay(handle);
	}

	//lock the mouse
	mouseLock();
	mDepressed = true;

	//update
	setUpdate();
}

void GuiButtonBaseCtrl::onMouseEnter(const GuiEvent &event)
{
   setUpdate();
   if(isMouseLocked())
   {
      mDepressed = true;
      mMouseOver = true;
   }
   else
   {
      if ( mActive && mProfile->mSoundButtonOver )
      {
         F32 pan = (F32(event.mousePoint.x)/F32(Canvas->mBounds.extent.x)*2.0f-1.0f)*0.8f;
         AUDIOHANDLE handle = alxCreateSource(mProfile->mSoundButtonOver);
		 alxSourcef(handle, AL_PITCH, mPitch);
         alxPlay(handle);
      }
      mMouseOver = true;
   }
}

void GuiButtonBaseCtrl::onMouseLeave(const GuiEvent &)
{
   setUpdate();
   if(isMouseLocked())
      mDepressed = false;
   mMouseOver = false;
}

void GuiButtonBaseCtrl::onMouseUp(const GuiEvent &event)
{
   if (! mActive)
      return;

   mouseUnlock();

   setUpdate();

   //if we released the mouse within this control, perform the action
   if (mDepressed)
      onAction();

   mDepressed = false;
}

void GuiButtonBaseCtrl::onRightMouseUp(const GuiEvent &event)
{
   Con::executef( this, 2, "onRightClick" );

   Parent::onRightMouseUp( event );
}

//--------------------------------------------------------------------------
bool GuiButtonBaseCtrl::onKeyDown(const GuiEvent &event)
{
   //if the control is a dead end, kill the event
   if (!mActive)
      return true;

   //see if the key down is a return or space or not
   if ((event.keyCode == KEY_RETURN || event.keyCode == KEY_SPACE)
       && event.modifier == 0)
   {
	   if ( mProfile->mSoundButtonDown )
	   {
	      F32 pan = ( F32( event.mousePoint.x ) / F32( Canvas->mBounds.extent.x ) * 2.0f - 1.0f ) * 0.8f;
	      AUDIOHANDLE handle = alxCreateSource( mProfile->mSoundButtonDown );
	      alxPlay( handle );
	   }
      return true;
   }
   //otherwise, pass the event to it's parent
   return Parent::onKeyDown(event);
}

//--------------------------------------------------------------------------
bool GuiButtonBaseCtrl::onKeyUp(const GuiEvent &event)
{
   //if the control is a dead end, kill the event
   if (!mActive)
      return true;

   //see if the key down is a return or space or not
   if (mDepressed &&
      (event.keyCode == KEY_RETURN || event.keyCode == KEY_SPACE) &&
      event.modifier == 0)
   {
      onAction();
      return true;
   }

   //otherwise, pass the event to it's parent
   return Parent::onKeyUp(event);
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::setScriptValue(const char *value)
{
	mStateOn = dAtob(value);

	// Update the console variable:
	if ( mConsoleVariable[0] )
		Con::setBoolVariable( mConsoleVariable, mStateOn );

   setUpdate();
}

//---------------------------------------------------------------------------
const char *GuiButtonBaseCtrl::getScriptValue()
{
	return mStateOn ? "1" : "0";
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::onAction()
{
    if(!mActive)
        return;

    if(mButtonType == ButtonTypeCheck)
    {
        mStateOn = mStateOn ? false : true;

        // Update the console variable:
        if ( mConsoleVariable[0] )
            Con::setBoolVariable( mConsoleVariable, mStateOn );
        // Execute the console command (if any)
        if( mConsoleCommand[0] )
        {
            char buf[16];
            dSprintf(buf, sizeof(buf), "%d", getId());
            Con::setVariable("$ThisControl", buf);
            Con::evaluate( mConsoleCommand, false );
        }

	}
	else if(mButtonType == ButtonTypeRadio)
    {
        mStateOn = true;
        messageSiblings(mRadioGroup);
    }		
    setUpdate();


    // Provide and onClick script callback.
    if( isMethod("onClick") )
       Con::executef( this, 2, "onClick" );

    Parent::onAction();
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::onMessage( GuiControl *sender, S32 msg )
{
	Parent::onMessage(sender, msg);
	if( mRadioGroup == msg && mButtonType == ButtonTypeRadio )
	{
		setUpdate();
		mStateOn = ( sender == this );
	}
}
//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::onRender(Point2I offset, const RectI &updateRect) {
	enum {
		NORMAL,
		HILIGHT,
		DEPRESSED,
		INACTIVE
	} state = NORMAL;

	if (mActive) {
		if (mMouseOver) state = HILIGHT;
		if (mDepressed || mStateOn) state = DEPRESSED;
	} else state = INACTIVE;

	switch (state) {
		case NORMAL: {
			dglSetBitmapModulation(mProfile->mFontColor);
			renderJustifiedText(offset, mBounds.extent, (char*)mButtonText, &mProfile->mOutlineColor);
			break;
		}
		case HILIGHT: {
			dglSetBitmapModulation(mProfile->mFontColorHL);
			renderJustifiedText(offset, mBounds.extent, (char*)mButtonText, &mProfile->mOutlineColorHL);
			break;
		}
		case DEPRESSED: {
			dglSetBitmapModulation(mProfile->mFontColorSEL);
			renderJustifiedText(offset, mBounds.extent, (char*)mButtonText, &mProfile->mOutlineColorSEL);
			break;
		}
		case INACTIVE: {
			dglSetBitmapModulation(mProfile->mFontColorNA);
			renderJustifiedText(offset, mBounds.extent, (char*)mButtonText, &mProfile->mOutlineColorNA);
			break;
		}
	}

	//render the child controls
	renderChildControls(offset, updateRect);
}