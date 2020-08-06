//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/controls/guiPopUpCtrl.h"
#include "gui/core/guiCanvas.h"
#include "gui/utility/guiInputCtrl.h"

class GuiControlListPopUp : public GuiPopUpMenuCtrl
{
   typedef GuiPopUpMenuCtrl Parent;
public:
   bool onAdd();

   DECLARE_CONOBJECT(GuiControlListPopUp);
};

IMPLEMENT_CONOBJECT(GuiControlListPopUp);

bool GuiControlListPopUp::onAdd()
{
   if(!Parent::onAdd())
      return false;
   clear();

   for(AbstractClassRep *rep = AbstractClassRep::getClassList(); rep; rep = rep->getNextClass())
   {
      ConsoleObject *obj = rep->create();
      if(obj && dynamic_cast<GuiControl *>(obj))
      {
         if( !dynamic_cast<GuiCanvas*>(obj) && !dynamic_cast<GuiInputCtrl*>(obj))
         addEntry(rep->getClassName(), 0);
      }
      delete obj;
   }

   // We want to be alphabetical!
   sort();

   return true;
}
