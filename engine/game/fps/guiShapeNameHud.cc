//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "dgl/dgl.h"
#include "dgl/gFont.h"
#include "gui/core/guiControl.h"
#include "gui/core/guiTSControl.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneGraph.h"
#include "game/shapeBase.h"
#include "game/gameConnection.h"

//----------------------------------------------------------------------------
/// Displays name & damage above shape objects.
///
/// This control displays the name and damage value of all named
/// ShapeBase objects on the client.  The name and damage of objects
/// within the control's display area are overlayed above the object.
///
/// This GUI control must be a child of a TSControl, and a server connection
/// and control object must be present.
///
/// This is a stand-alone control and relies only on the standard base GuiControl.
class GuiShapeNameHud : public GuiControl 
{
   typedef GuiControl Parent;

   // field data
   F32      mVerticalOffset;
   F32      mDistanceFade;

public:
   // Debugging
   Vector<StringTableEntry> objectIDs;

protected:
   void drawName( Point2I offset, const char *buf, F32 opacity);

public:
   GuiShapeNameHud();

   // GuiControl
   virtual void onRender(Point2I offset, const RectI &updateRect);

   static void initPersistFields();
   DECLARE_CONOBJECT( GuiShapeNameHud );
};


//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiShapeNameHud);

/// Default distance for object's information to be displayed.
static const F32 cDefaultVisibleDistance = 500.0f;

GuiShapeNameHud::GuiShapeNameHud()
{
   mVerticalOffset = 0.5;
   mDistanceFade = 0.1;
}

void GuiShapeNameHud::initPersistFields()
{
   Parent::initPersistFields();

   addGroup("Misc");
   addField( "verticalOffset", TypeF32, Offset( mVerticalOffset, GuiShapeNameHud ) );
   addField( "distanceFade", TypeF32, Offset( mDistanceFade, GuiShapeNameHud ) );
   endGroup("Misc");
}


//----------------------------------------------------------------------------
/// Core rendering method for this control.
///
/// This method scans through all the current client ShapeBase objects.
/// If one is named, it displays the name and damage information for it.
///
/// Information is offset from the center of the object's bounding box,
/// unless the object is a PlayerObjectType, in which case the eye point
/// is used.
///
/// @param   updateRect   Extents of control.
void GuiShapeNameHud::onRender(Point2I, const RectI &updateRect)
{
	// Must be in a TS Control
	GuiTSCtrl *parent = dynamic_cast<GuiTSCtrl*>(getParent());
	if (!parent) return;

	// Must have a connection and control object
	GameConnection* conn = GameConnection::getConnectionToServer();
	if (!conn)
		return;

	ShapeBase* control = conn->getControlObject();
	if (!control)
		return;

	// Get control camera info
	MatrixF cam;
	Point3F camPos;
	VectorF camDir;
	conn->getControlCameraTransform(0, &cam);
	cam.getColumn(3, &camPos);
	cam.getColumn(1, &camDir);

	F32 camFov;
	conn->getControlCameraFov(&camFov);
	camFov = mDegToRad(camFov) / 2;

	// Visible distance info & name fading
	F32 visDistance = gClientSceneGraph->getVisibleDistance();
	F32 visDistanceSqr = visDistance * visDistance;
	F32 fadeDistance = visDistance * mDistanceFade;

	// Collision info. We're going to be running LOS tests and we
	// don't want to collide with the control object.
	static U32 losMask = TerrainObjectType | InteriorObjectType | ShapeBaseObjectType;
	control->disableCollision();

	// Render debug info
	Resource<GFont> debugFont = GFont::create("Consolas", 14, "", 0);
	for (Vector<StringTableEntry>::iterator it = objectIDs.begin(); it != objectIDs.end(); it++) {
		StringTableEntry objID = *it;
		SceneObject* object = NULL;

		if ((object = dynamic_cast<SceneObject*>(Sim::findObject(objID))) == NULL) {
			Con::errorf("Unable to find object by ID or name \"%s\"", objID);
			objectIDs.erase(it);
			it = objectIDs.begin();
			continue;
		}

		Point3F objPos(object->getPosition());
		objPos.z += object->getRenderWorldBox().len_z() / 2;

		Point3F drawPos(0.0f, 0.0f, 0.0f);
		if (!parent->project(objPos, &drawPos)) continue;

		Vector<char*> infoText;
		S32 lastLen = 0;

		// Create text
		infoText.push_back(new char[lastLen = (dStrlen(object->getIdString()) + 1)]); dSprintf(infoText[infoText.size() - 1], lastLen, "%s", object->getIdString());
		infoText.push_back(new char[lastLen = (dStrlen(object->getClassName()) + 1)]); dSprintf(infoText[infoText.size() - 1], lastLen, "%s", object->getClassName());

		// Calculate new screen position
		drawPos.y -= (F32)((infoText.size() * debugFont->getHeight()) / 2);

		for (Vector<char*>::iterator _it = infoText.begin(); _it != infoText.end(); _it++) {
			char* text = *_it;

			dglSetBitmapModulation(ColorI(0, 0, 0, 255));
			for (S32 i = 0; i < 4 * 4; i++) {
				S32 x = -2 + (i % 4);
				S32 y = -2 + (i / 4);

				dglDrawText(debugFont, Point2I(drawPos.x + x, drawPos.y + y), text);
			}

			dglSetBitmapModulation(ColorI(255, 255, 255, 255));
			dglDrawText(debugFont, Point2I(drawPos.x, drawPos.y), text);
			dglClearBitmapModulation();
			delete[] text;

			drawPos.y += debugFont->getHeight();
		}
	}

	// All ghosted objects are added to the server connection group,
	// so we can find all the shape base objects by iterating through
	// our current connection.
	for (SimSetIterator itr(conn); *itr; ++itr)
	{
		if ((*itr)->getType() & ShapeBaseObjectType)
		{
			ShapeBase* shape = static_cast<ShapeBase*>(*itr);
			if (shape != control && shape->getShapeName())
			{

				// Target pos to test, if it's a player run the LOS to his eye
				// point, otherwise we'll grab the generic box center.
				Point3F shapePos;
				if (shape->getType() & PlayerObjectType)
				{
					MatrixF eye;

					// Use the render eye transform, otherwise we'll see jittering
					shape->getRenderEyeTransform(&eye);
					eye.getColumn(3, &shapePos);
				}
				else
				{
					// Use the render transform instead of the box center
					// otherwise it'll jitter.
					MatrixF srtMat = shape->getRenderTransform();
					srtMat.getColumn(3, &shapePos);
				}

				VectorF shapeDir = shapePos - camPos;

				// Test to see if it's in range
				F32 shapeDist = shapeDir.lenSquared();
				if (shapeDist == 0 || shapeDist > visDistanceSqr)
					continue;
				shapeDist = mSqrt(shapeDist);

				// Test to see if it's within our viewcone, this test doesn't
				// actually match the viewport very well, should consider
				// projection and box test.
				shapeDir.normalize();
				F32 dot = mDot(shapeDir, camDir);
				if (dot < camFov)
					continue;

				// Test to see if it's behind something, and we want to
				// ignore anything it's mounted on when we run the LOS.
				RayInfo info;
				shape->disableCollision();
				ShapeBase *mount = shape->getObjectMount();

				if (mount)
					mount->disableCollision();
				bool los = !gClientContainer.castRay(camPos, shapePos, losMask, &info);
				shape->enableCollision();
				if (mount)
					mount->enableCollision();

				if (!los)
					continue;

				// Project the shape pos into screen space and calculate
				// the distance opacity used to fade the labels into the
				// distance.
				Point3F projPnt;
				shapePos.z += mVerticalOffset;
				if (!parent->project(shapePos, &projPnt))
					continue;
				F32 opacity = (shapeDist < fadeDistance) ? 1.0 :
					1.0 - (shapeDist - fadeDistance) / (visDistance - fadeDistance);

				// Render the shape's name
				drawName(Point2I((S32)projPnt.x, (S32)projPnt.y), shape->getShapeName(), opacity);
			}
		}
	}

	// Restore control object collision
	control->enableCollision();
}


//----------------------------------------------------------------------------
/// Render object names.
///
/// Helper function for GuiShapeNameHud::onRender
///
/// @param   offset  Screen coordinates to render name label. (Text is centered
///                  horizontally about this location, with bottom of text at
///                  specified y position.)
/// @param   name    String name to display.
/// @param   opacity Opacity of name (a fraction).
void GuiShapeNameHud::drawName(Point2I offset, const char *name, F32 opacity)
{
   // Center the name
   offset.x -= mProfile->mFont->getStrWidth((const UTF8 *)name) / 2;
   offset.y -= mProfile->mFont->getHeight();

   // Deal with opacity and draw.
   dglDrawText(mProfile->mFont, offset, name);
   dglClearBitmapModulation();
}

ConsoleMethod(GuiShapeNameHud, setDebuggingObjects, void, 3, 3, "(list)") {
	const char* ptr = argv[2];
	S32 length = dStrlen(argv[2]);

	object->objectIDs.clear();
	object->objectIDs.empty();

	// Scan the dbgList
	while (ptr < (argv[2] + length)) {
		const char* endPtr = ptr;

		// Find end character
		if ((endPtr = dStrstr(ptr, " ")) == NULL) endPtr = argv[2] + length;

		// Insert the objectID
		char tmp[32];
		char str[1024];

		dSprintf(tmp, 32, "%%.%ds", endPtr - ptr);
		dSprintf(str, 1024, tmp, ptr);

		Con::printf("Entry # %04d: %s", object->objectIDs.size(), str);
		object->objectIDs.push_back(StringTable->insert(str));

		// Onto the next word
		ptr = endPtr;
		if (ptr == NULL) break;
		ptr++;
	}
}