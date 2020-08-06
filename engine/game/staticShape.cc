//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "dgl/dgl.h"
#include "core/dnet.h"
#include "core/bitStream.h"
#include "game/game.h"
#include "math/mMath.h"
#include "console/simBase.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "game/moveManager.h"
#include "ts/tsShapeInstance.h"
#include "core/resManager.h"
#include "game/staticShape.h"
#include "math/mathIO.h"
#include "game/shadow.h"
#include "sim/netConnection.h"
#include "lightingSystem/sgLighting.h"

extern bool gEditingMission;

static const U32 sgAllowedDynamicTypes = 0xffffff;

//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(StaticShapeData);

StaticShapeData::StaticShapeData()
{
   shadowEnable = true;
   shadowCanMove = false;
   shadowCanAnimate = true;

   dynamicTypeField     = 0;

   genericShadowLevel = StaticShape_GenericShadowLevel;
   noShadowLevel = StaticShape_NoShadowLevel;
   noIndividualDamage = false;
}

void StaticShapeData::initPersistFields()
{
   Parent::initPersistFields();

   addField("noIndividualDamage",   TypeBool, Offset(noIndividualDamage,   StaticShapeData));
   addField("dynamicType",          TypeS32,  Offset(dynamicTypeField,     StaticShapeData));
}

void StaticShapeData::packData(BitStream* stream)
{
   Parent::packData(stream);
   stream->writeFlag(noIndividualDamage);
   stream->write(dynamicTypeField);
}

void StaticShapeData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);
   noIndividualDamage = stream->readFlag();
   stream->read(&dynamicTypeField);
}


//----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(StaticShape);

StaticShape::StaticShape()
{
   overrideOptions = false;

   mTypeMask |= StaticShapeObjectType | StaticObjectType;
   mDataBlock = 0;
}

StaticShape::~StaticShape()
{
}


void StaticShape::initPersistFields()
{
   Parent::initPersistFields();

   addGroup("Lighting");
   addField("receiveSunLight", TypeBool, Offset(receiveSunLight, SceneObject));
   addField("receiveLMLighting", TypeBool, Offset(receiveLMLighting, SceneObject));
   addField("useAdaptiveSelfIllumination", TypeBool, Offset(useAdaptiveSelfIllumination, SceneObject));
   addField("useCustomAmbientLighting", TypeBool, Offset(useCustomAmbientLighting, SceneObject));
   addField("customAmbientSelfIllumination", TypeBool, Offset(customAmbientForSelfIllumination, SceneObject));
   addField("customAmbientLighting", TypeColorF, Offset(customAmbientLighting, SceneObject));
   addField("lightGroupName", TypeString, Offset(lightGroupName, SceneObject));
   addField("useLightingOcclusion", TypeBool, Offset(useLightingOcclusion, SceneObject));
   endGroup("Lighting");
}

void StaticShape::inspectPostApply()
{
   if(isServerObject()) {
      setMaskBits(0xffffffff);
   }
}


//----------------------------------------------------------------------------

bool StaticShape::onAdd()
{
   if(!Parent::onAdd() || !mDataBlock)
      return false;

   // We need to modify our type mask based on what our datablock says...
   mTypeMask |= (mDataBlock->dynamicTypeField & sgAllowedDynamicTypes);

   addToScene();

   if (isServerObject())
      scriptOnAdd();
   return true;
}

bool StaticShape::onNewDataBlock(GameBaseData* dptr)
{
   mDataBlock = dynamic_cast<StaticShapeData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr))
      return false;

   scriptOnNewDataBlock();
   return true;
}

void StaticShape::onRemove()
{
   scriptOnRemove();
   removeFromScene();
   Parent::onRemove();
}

void StaticShape::renderShadow(F32 dist, F32 fogAmount)
{
	if(!mDataBlock->shadowEnable)
		return;

	bool allowanimate = mDataBlock->shadowCanAnimate &&
		(!(!mDataBlock->shape.isNull() && mDataBlock->shape->sequences.empty()));
   bool allowmove = mDataBlock->shadowCanMove && gEditingMission;

	shadows.sgRender(this, mShapeInstance, dist, fogAmount,
		mDataBlock->genericShadowLevel, mDataBlock->noShadowLevel,
		mDataBlock->shadowNode, allowmove, allowanimate);
}

//----------------------------------------------------------------------------

void StaticShape::processTick(const Move* move)
{
   Parent::processTick(move);

   // Image Triggers
   if (move && mDamageState == Enabled) {
      setImageTriggerState(0,move->trigger[0]);
      setImageTriggerState(1,move->trigger[1]);
   }

   if (isMounted()) {
      MatrixF mat;
      mMount.object->getMountTransform(mMount.node,&mat);
      Parent::setTransform(mat);
      Parent::setRenderTransform(mat);
   }
}

void StaticShape::interpolateTick(F32)
{
   if (isMounted()) {
      MatrixF mat;
      mMount.object->getRenderMountTransform(mMount.node,&mat);
      Parent::setRenderTransform(mat);
   }
}

void StaticShape::setTransform(const MatrixF& mat)
{
   Parent::setTransform(mat);
   setMaskBits(PositionMask);
}

void StaticShape::onUnmount(ShapeBase*,S32)
{
   // Make sure the client get's the final server pos.
   setMaskBits(PositionMask);
}


//----------------------------------------------------------------------------

U32 StaticShape::packUpdate(NetConnection *connection, U32 mask, BitStream *bstream)
{
   U32 retMask = Parent::packUpdate(connection,mask,bstream);
   if (bstream->writeFlag(mask & PositionMask)) {
      bstream->writeAffineTransform(mObjToWorld);
      mathWrite(*bstream, mObjScale);
   }

   // powered?
   bstream->writeFlag(mPowered);

   if(bstream->writeFlag(mask & advancedStaticOptionsMask))
   {
	   bstream->writeFlag(receiveSunLight);
	   bstream->writeFlag(useAdaptiveSelfIllumination);
	   bstream->writeFlag(useCustomAmbientLighting);
	   bstream->writeFlag(customAmbientForSelfIllumination);
	   bstream->write(customAmbientLighting);
	   bstream->writeFlag(receiveLMLighting);
      bstream->writeFlag(useLightingOcclusion);

	   if(isServerObject())
	   {
		   lightIds.clear();
		   findLightGroup(connection);

		   U32 maxcount = getMin(lightIds.size(), SG_TSSTATIC_MAX_LIGHTS);
		   bstream->writeInt(maxcount, SG_TSSTATIC_MAX_LIGHT_SHIFT);
		   for(U32 i=0; i<maxcount; i++)
		   {
			   bstream->writeInt(lightIds[i], NetConnection::GhostIdBitSize);
		   }
	   }
	   else
	   {
		   // recording demo...
		   U32 maxcount = getMin(lightIds.size(), SG_TSSTATIC_MAX_LIGHTS);
		   bstream->writeInt(maxcount, SG_TSSTATIC_MAX_LIGHT_SHIFT);
		   for(U32 i=0; i<maxcount; i++)
		   {
			   bstream->writeInt(lightIds[i], NetConnection::GhostIdBitSize);
		   }
	   }
   }
   
   return retMask;
}

void StaticShape::unpackUpdate(NetConnection *connection, BitStream *bstream)
{
   Parent::unpackUpdate(connection,bstream);
   if (bstream->readFlag()) {
      MatrixF mat;
      bstream->readAffineTransform(&mat);
      Parent::setTransform(mat);
      Parent::setRenderTransform(mat);

      VectorF scale;
      mathRead(*bstream, &scale);
      setScale(scale);
   }

   // powered?
   mPowered = bstream->readFlag();

   if(bstream->readFlag())
   {
	   receiveSunLight = bstream->readFlag();
	   useAdaptiveSelfIllumination = bstream->readFlag();
	   useCustomAmbientLighting = bstream->readFlag();
	   customAmbientForSelfIllumination = bstream->readFlag();
	   bstream->read(&customAmbientLighting);
	   receiveLMLighting = bstream->readFlag();
      useLightingOcclusion = bstream->readFlag();

	   U32 count = bstream->readInt(SG_TSSTATIC_MAX_LIGHT_SHIFT);
	   lightIds.clear();
	   for(U32 i=0; i<count; i++)
	   {
		   S32 id = bstream->readInt(NetConnection::GhostIdBitSize);
		   lightIds.push_back(id);
	   }
   }
}


//----------------------------------------------------------------------------
ConsoleMethod( StaticShape, setPoweredState, void, 3, 3, "(bool isPowered)")
{
   if(!object->isServerObject())
      return;
   object->setPowered(dAtob(argv[2]));
}

ConsoleMethod( StaticShape, getPoweredState, bool, 2, 2, "")
{
   if(!object->isServerObject())
      return(false);
   return(object->isPowered());
}
