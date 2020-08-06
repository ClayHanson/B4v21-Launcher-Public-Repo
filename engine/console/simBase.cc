//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/simBase.h"
#include "core/stringTable.h"
#include "console/console.h"
#include "core/fileStream.h"
#include "sim/actionMap.h"
#include "core/resManager.h"
#include "core/fileObject.h"
#include "console/consoleInternal.h"
#include "platform/profiler.h"
#include "console/typeValidators.h"
#include "core/frameAllocator.h"

namespace Sim
{
   // Don't forget to InstantiateNamed* in simManager.cc - DMM
   ImplementNamedSet(ActiveActionMapSet)
   ImplementNamedSet(GhostAlwaysSet)
   ImplementNamedSet(LightSet)
   ImplementNamedSet(WayPointSet)
   ImplementNamedSet(fxReplicatorSet)
   ImplementNamedSet(fxFoliageSet)
   ImplementNamedGroup(ActionMapGroup)
   ImplementNamedGroup(ClientGroup)
   ImplementNamedGroup(GuiGroup)
   ImplementNamedGroup(ScriptClassGroup)
   ImplementNamedGroup(GuiDataGroup)
   ImplementNamedGroup(TCPGroup)

   //groups created on the client
   ImplementNamedGroup(ClientConnectionGroup)
   ImplementNamedGroup(ChunkFileGroup)
   ImplementNamedSet(sgMissionLightingFilterSet)
}   

//---------------------------------------------------------------------------

void SimObjectList::pushBack(SimObject* obj)
{
   if (find(begin(),end(),obj) == end())
      push_back(obj);
}	

void SimObjectList::pushBackForce(SimObject* obj)
{
   iterator itr = find(begin(),end(),obj);
   if (itr == end()) 
   {
      push_back(obj);
   }
   else 
   {
      // Move to the back...
      //
      SimObject* pBack = *itr;
      removeStable(pBack);
      push_back(pBack);
   }
}	

void SimObjectList::pushFront(SimObject* obj)
{
   if (find(begin(),end(),obj) == end())
      push_front(obj);
}	

void SimObjectList::remove(SimObject* obj)
{
   iterator ptr = find(begin(),end(),obj);
   if (ptr != end()) 
      erase(ptr);
}

void SimObjectList::removeStable(SimObject* obj)
{
   iterator ptr = find(begin(),end(),obj);
   if (ptr != end()) 
      erase(ptr);
}

S32 QSORT_CALLBACK SimObjectList::compareId(const void* a,const void* b)
{
   return (*reinterpret_cast<const SimObject* const*>(a))->getId() -
      (*reinterpret_cast<const SimObject* const*>(b))->getId();
}

void SimObjectList::sortId()
{
   dQsort(address(),size(),sizeof(value_type),compareId);
}	


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

SimFieldDictionary::Entry *SimFieldDictionary::mFreeList = NULL;

static Chunker<SimFieldDictionary::Entry> fieldChunker;

SimFieldDictionary::Entry *SimFieldDictionary::allocEntry()
{
   if(mFreeList)
   {
      Entry *ret = mFreeList;
      mFreeList = ret->next;
      return ret;
   }
   else
      return fieldChunker.alloc();
}

void SimFieldDictionary::freeEntry(SimFieldDictionary::Entry *ent)
{
   ent->next = mFreeList;
   mFreeList = ent;
}

SimFieldDictionary::SimFieldDictionary()
{
   for(U32 i = 0; i < HashTableSize; i++)
      mHashTable[i] = 0;

   mVersion = 0;
}

SimFieldDictionary::~SimFieldDictionary()
{
   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i]; walk;)
      {
         Entry *temp = walk;
         walk = temp->next;

         dFree(temp->value);
         freeEntry(temp);
      }
   }
}

void SimFieldDictionary::setFieldValue(StringTableEntry slotName, const char *value)
{
   U32 bucket = HashPointer(slotName) % HashTableSize;
   Entry **walk = &mHashTable[bucket];
   while(*walk && (*walk)->slotName != slotName)
      walk = &((*walk)->next);

   Entry *field = *walk;
   if(!*value)
   {
      if(field)
      {
         mVersion++;

         dFree(field->value);
         *walk = field->next;
         freeEntry(field);
      }
   }
   else
   {
      if(field)
      {
         dFree(field->value);
         field->value = dStrdup(value);
      }
      else
      {
         mVersion++;

         field = allocEntry();
         field->value = dStrdup(value);
         field->slotName = slotName;
         field->next = NULL;
         *walk = field;
      }
   }
}

const char *SimFieldDictionary::getFieldValue(StringTableEntry slotName)
{
   U32 bucket = HashPointer(slotName) % HashTableSize;

   for(Entry *walk = mHashTable[bucket];walk;walk = walk->next)
      if(walk->slotName == slotName)
         return walk->value;

   return NULL;
}

//---------------------------------------------------------------------------

SimObject::SimObject()
{
   objectName            = NULL;
   mInternalName          = NULL;
   nextNameObject        = (SimObject*)-1;
   nextManagerNameObject = (SimObject*)-1;
   nextIdObject          = NULL;

   mId           = 0;
   mGroup        = 0;
   mNameSpace    = NULL;
   mNotifyList   = NULL;
   mFlags.set( ModStaticFields | ModDynamicFields );
   mTypeMask             = 0;

   mFieldDictionary = NULL;
   mCanSaveFieldDictionary	=	true;
}

static void writeTabs(Stream &stream, U32 count)
{
   char tab[] = "\t";
   while(count--)
      stream.write(1, (void*)tab);
}

void SimFieldDictionary::assignFrom(SimFieldDictionary *dict)
{
   mVersion++;

   for(U32 i = 0; i < HashTableSize; i++)
      for(Entry *walk = dict->mHashTable[i];walk; walk = walk->next)
         setFieldValue(walk->slotName, walk->value);
}

void SimFieldDictionary::writeFields(SimObject *obj, Stream &stream, U32 tabStop)
{
   const AbstractClassRep::FieldList &list = obj->getFieldList();
   
   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i];walk; walk = walk->next)
      {
         // make sure we haven't written this out yet:
         U32 i;
         for(i = 0; i < list.size(); i++)
            if(list[i].pFieldname == walk->slotName)
               break;

         if(i != list.size())
            continue;

         if (!obj->writeField(walk->slotName, walk->value))
            continue;

         U32 nBufferSize = (dStrlen( walk->value ) * 2) + dStrlen( walk->slotName ) + 16;
         FrameTemp<char> expandedBuffer( nBufferSize );

         writeTabs(stream, tabStop+1);

         dSprintf(expandedBuffer, nBufferSize, "%s = \"", walk->slotName);
         expandEscape((char*)expandedBuffer + dStrlen(expandedBuffer), walk->value);
         dStrcat(expandedBuffer, "\";\r\n");
         stream.write(dStrlen(expandedBuffer),expandedBuffer);
      }
   }
}

static S32 QSORT_CALLBACK compareEntries(const void* a,const void* b)
{
   SimFieldDictionary::Entry *fa = *((SimFieldDictionary::Entry **)a);
   SimFieldDictionary::Entry *fb = *((SimFieldDictionary::Entry **)b);
   return dStricmp(fa->slotName, fb->slotName);
}

void SimFieldDictionary::printFields(SimObject *obj)
{
   const AbstractClassRep::FieldList &list = obj->getFieldList();
   char expandedBuffer[4096];
   Vector<Entry *> flist(__FILE__, __LINE__);

   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i];walk; walk = walk->next)
      {
         // make sure we haven't written this out yet:
         U32 i;
         for(i = 0; i < list.size(); i++)
            if(list[i].pFieldname == walk->slotName)
               break;

         if(i != list.size())
            continue;

         flist.push_back(walk);
      }
   }
   dQsort(flist.address(),flist.size(),sizeof(Entry *),compareEntries);

   for(Vector<Entry *>::iterator itr = flist.begin(); itr != flist.end(); itr++)
   {
      dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s = \"", (*itr)->slotName);
      expandEscape(expandedBuffer + dStrlen(expandedBuffer), (*itr)->value);
      Con::printf("%s\"", expandedBuffer);
   }
}

//------------------------------------------------------------------------------
SimFieldDictionaryIterator::SimFieldDictionaryIterator(SimFieldDictionary * dictionary)
{
   mDictionary = dictionary;
   mHashIndex = -1;
   mEntry = 0;
   operator++();
}

SimFieldDictionary::Entry* SimFieldDictionaryIterator::operator++()
{
   if(!mDictionary)
      return(mEntry);

   if(mEntry)
      mEntry = mEntry->next;

   while(!mEntry && (mHashIndex < (SimFieldDictionary::HashTableSize-1)))
      mEntry = mDictionary->mHashTable[++mHashIndex];

   return(mEntry);
}

SimFieldDictionary::Entry* SimFieldDictionaryIterator::operator*()
{
   return(mEntry);
}

void SimObject::assignFieldsFrom(SimObject *parent)
{
   // only allow field assigns from objects of the same class:
   if(getClassRep() == parent->getClassRep())
   {
      const AbstractClassRep::FieldList &list = getFieldList();

      // copy out all the fields:
      for(U32 i = 0; i < list.size(); i++)
      {
         const AbstractClassRep::Field* f = &list[i];
         if(f->elementCount == 1)
         {
            const char *fieldVal = Con::getData(f->type, (void *) (((const char *)parent) + f->offset), 0, f->table, f->flag);
            if(fieldVal)
               Con::setData(f->type, (void *) (((const char *)this) + f->offset), 0, 1, &fieldVal, f->table);
         }
         else
         {
            for(U32 j = 0; j < f->elementCount; j++)
            {
               const char *fieldVal = Con::getData(f->type, (void *) (((const char *)parent) + f->offset), j, f->table, f->flag);
               if(fieldVal)
                  Con::setData(f->type, (void *) (((const char *)this) + f->offset), j, 1, &fieldVal, f->table);
            }
         }
      }
   }

   if(parent->mFieldDictionary)
   {
      if( mFieldDictionary == NULL )
      mFieldDictionary = new SimFieldDictionary;
      mFieldDictionary->assignFrom(parent->mFieldDictionary);
   }
}

bool SimObject::writeField(StringTableEntry fieldname, const char* value)
{
   // Don't write empty fields.
   if (!value || !*value)
      return false;

   return true;
}

void SimObject::writeFields(Stream& stream, U32 tabStop, U32 flags)
{
	const AbstractClassRep::FieldList& list = getFieldList();
	const char* docRoot = Con::getVariable("$DocRoot");
	const char* modRoot = Con::getVariable("$ModRoot");
	char pWhitespace[128];
	char* whitePtr = pWhitespace;
	S32 docRootLen = dStrlen(docRoot);
	S32 modRootLen = dStrlen(modRoot);
	S32 longestLen = 0;

	*whitePtr = 0;

	if (flags & WhitespaceFields)
	{
		for (U32 i = 0; i < list.size(); i++)
		{
			const AbstractClassRep::Field* f = &list[i];

			if (f->type == AbstractClassRep::DepricatedFieldType ||
				f->type == AbstractClassRep::StartGroupFieldType ||
				f->type == AbstractClassRep::EndGroupFieldType) continue;

			for (U32 j = 0; S32(j) < f->elementCount; j++)
			{
				int Length = 0;

				// Get the length of theis element
				if (f->elementCount == 1)
					Length = dStrlen(f->pFieldname);
				else
				{
					char temp[64];
					dSprintf(temp, sizeof(temp), "%s[%d]", f->pFieldname, j);
					Length = dStrlen(temp);
				}

				// Compare it to the longest length we currently have
				if (Length > longestLen)
				{
					longestLen = Length;

					while (dStrlen(pWhitespace) < longestLen)
					{
						*whitePtr++ = ' ';
						*whitePtr = 0;
					}
				}
			}
		}
	}

	for (U32 i = 0; i < list.size(); i++)
	{
		const AbstractClassRep::Field* f = &list[i];

		if (f->type == AbstractClassRep::DepricatedFieldType ||
			f->type == AbstractClassRep::StartGroupFieldType ||
			f->type == AbstractClassRep::EndGroupFieldType) continue;

		for (U32 j = 0; S32(j) < f->elementCount; j++)
		{
			char array[8];
			dSprintf(array, 8, "%d", j);
			const char* val = getDataField(StringTable->insert(f->pFieldname), array);

			// Make a copy for the field check.
			if (!val)
				continue;

			U32 nBufferSize = dStrlen(val) + 1;
			FrameTemp<char> valCopy(nBufferSize);
			dStrcpy((char*)valCopy, val);

			if (!writeField(f->pFieldname, valCopy))
				continue;

			val = valCopy;

			U32 expandedBufferSize = (nBufferSize * 2) + ((flags & WhitespaceFields) == 0 ? 32 : 128);
			FrameTemp<char> expandedBuffer(expandedBufferSize);
			if (flags & WhitespaceFields)
			{
				// Add whitespace
				if (f->elementCount == 1)
					dSprintf(expandedBuffer, expandedBufferSize, "%s%s = \"", f->pFieldname, pWhitespace + dStrlen(f->pFieldname));
				else
				{
					dSprintf(expandedBuffer, expandedBufferSize, "%s[%d] = \"", f->pFieldname, j);
					int wAmt = dStrlen(expandedBuffer) - 4;
					dSprintf(expandedBuffer, expandedBufferSize, "%s[%d]%s = \"", f->pFieldname, j, pWhitespace + wAmt);
				}
			}
			else
			{
				// No whitespace
				if (f->elementCount == 1)
					dSprintf(expandedBuffer, expandedBufferSize, "%s = \"", f->pFieldname);
				else
					dSprintf(expandedBuffer, expandedBufferSize, "%s[%d] = \"", f->pFieldname, j);
			}

			// detect and collapse relative path information
			if (f->type == TypeFilename)
			{
				if (docRootLen && (dStrnicmp(docRoot, val, docRootLen) == 0))
				{
					val += (docRootLen - 1);
					dStrcat(expandedBuffer, ".");
				}
				else
					if (modRootLen && (dStrnicmp(modRoot, val, modRootLen) == 0))
					{
						val += (modRootLen - 1);
						dStrcat(expandedBuffer, "~");
					}
			}

			expandEscape((char*)expandedBuffer + dStrlen(expandedBuffer), val);
			dStrcat(expandedBuffer, "\";\r\n");

			writeTabs(stream, tabStop);
			stream.write(dStrlen(expandedBuffer), expandedBuffer);
		}
	}

	if (mFieldDictionary && mCanSaveFieldDictionary)
		mFieldDictionary->writeFields(this, stream, tabStop);
}

void SimObject::write(Stream &stream, U32 tabStop, U32 flags)
{
   // Only output selected objects if they want that.
   if((flags & SelectedOnly) && !isSelected())
      return;

   writeTabs(stream, tabStop);
   char buffer[1024];
   dSprintf(buffer, sizeof(buffer), "new %s(%s)\r\n", getClassName(), getName() ? getName() : "");
   stream.write(dStrlen(buffer), buffer);
   writeTabs(stream, tabStop);
   stream.write(3, "{\r\n");
   writeTabs(stream, tabStop);
   writeFields(stream, tabStop + 1, flags);
   writeTabs(stream, tabStop);
   stream.write(4, "};\r\n");
}
bool SimObject::save(const char* pcFileName, bool bOnlySelected, S32 iInitialTab, bool bPretty)
{
   static const char *beginMessage = "//--- OBJECT WRITE BEGIN ---";
   static const char *endMessage = "//--- OBJECT WRITE END ---";
   FileStream stream;
   FileObject f;
   f.readMemory(pcFileName);

   // check for flags <selected, ...>
   U32 writeFlags = 0;
   if (bOnlySelected)
	   writeFlags |= SimObject::SelectedOnly;
   if (bPretty)
	   writeFlags |= SimObject::WhitespaceFields;

   if(!ResourceManager->openFileForWrite(stream, pcFileName)) 
      return false;

   char docRoot[256];
   char modRoot[256];

   dStrcpy(docRoot, pcFileName);
   char *p = dStrrchr(docRoot, '/');
   if (p) *++p = '\0';
   else  docRoot[0] = '\0';

   dStrcpy(modRoot, pcFileName);
   p = dStrchr(modRoot, '/');
   if (p) *++p = '\0';
   else  modRoot[0] = '\0';

   Con::setVariable("$DocRoot", docRoot);
   Con::setVariable("$ModRoot", modRoot);

   const char *buffer;
   while(!f.isEOF())
   {
      buffer = (const char *) f.readLine();
      if(!dStrcmp(buffer, beginMessage))
         break;
      stream.write(dStrlen(buffer), buffer);
      stream.write(2, "\r\n");
   }
   stream.write(dStrlen(beginMessage), beginMessage);
   stream.write(2, "\r\n");
   write(stream, iInitialTab, writeFlags);
   stream.write(dStrlen(endMessage), endMessage);
   stream.write(2, "\r\n");
   while(!f.isEOF())
   {
      buffer = (const char *) f.readLine();
      if(!dStrcmp(buffer, endMessage))
         break;
   }
   while(!f.isEOF())
   {
      buffer = (const char *) f.readLine();
      stream.write(dStrlen(buffer), buffer);
      stream.write(2, "\r\n");
   }

   Con::setVariable("$DocRoot", NULL);
   Con::setVariable("$ModRoot", NULL);

   return true;

}

ConsoleFunctionGroupBegin ( SimFunctions, "Functions relating to Sim.");

ConsoleFunction(nameToID, S32, 2, 2, "nameToID(object)")
{
   argc;
   SimObject *obj = Sim::findObject(argv[1]);
   if(obj)
      return obj->getId();
   else
      return -1;
}

ConsoleFunction(isObject, bool, 2, 2, "isObject(object)")
{
   argc;
   if (!dStrcmp(argv[1], "0") || !dStrcmp(argv[1], ""))
      return false;
   else
      return (Sim::findObject(argv[1]) != NULL);
}

ConsoleFunction(cancel,void,2,2,"cancel(eventId)")
{
   argc;
   Sim::cancelEvent(dAtoi(argv[1]));
}

ConsoleFunction(isEventPending, bool, 2, 2, "isEventPending(%scheduleId);")
{
   argc;
   return Sim::isEventPending(dAtoi(argv[1]));
}

ConsoleFunction(getEventTimeLeft, S32, 2, 2, "getEventTimeLeft(scheduleId) Get the time left in ms until this event will trigger.")
{
   return Sim::getEventTimeLeft(dAtoi(argv[1]));
}

ConsoleFunction(getScheduleDuration, S32, 2, 2, "getScheduleDuration(%scheduleId);")
{
   argc;   S32 ret = Sim::getScheduleDuration(dAtoi(argv[1]));
   return ret;
}

ConsoleFunction(getTimeSinceStart, S32, 2, 2, "getTimeSinceStart(%scheduleId);")
{
   argc;   S32 ret = Sim::getTimeSinceStart(dAtoi(argv[1]));
   return ret;
}

ConsoleFunction(schedule, S32, 4, 0, "schedule(time, refobject|0, command, <arg1...argN>)")
{
   U32 timeDelta = U32(dAtof(argv[1]));
   SimObject *refObject = Sim::findObject(argv[2]);
   if(!refObject)
   {
      if(argv[2][0] != '0')
         return 0;

      refObject = Sim::getRootGroup();
   }
   SimConsoleEvent *evt = new SimConsoleEvent(argc - 3, argv + 3, false);

   S32 ret = Sim::postEvent(refObject, evt, Sim::getCurrentTime() + timeDelta);
// #ifdef DEBUG
//    Con::printf("ref %s schedule(%s) = %d", argv[2], argv[3], ret);
//    Con::executef(1, "backtrace");
// #endif
   return ret;
}

ConsoleFunctionGroupEnd( SimFunctions );

ConsoleMethod(SimObject, save, bool, 3, 6, "obj.save(fileName, <selectedOnly = false>, <tabStart = 0>, <pretty = false>)")
{
   bool bSelectedOnly = false;
   bool bPretty       = false;
   S32 iTabStart      = 0;

   if (argc >= 4)
      bSelectedOnly	= dAtob(argv[3]);

   if (argc >= 5)
	   iTabStart = dAtoi(argv[4]);
   
   if (argc >= 6)
	   bPretty = dAtob(argv[5]);

   return object->save(argv[2], bSelectedOnly, iTabStart, bPretty);

}

ConsoleMethod(SimObject, setName, void, 3, 3, "obj.setName(newName)")
{
   argc;
   object->assignName(argv[2]);
}

ConsoleMethod(SimObject, getName, const char *, 2, 2, "obj.getName()")
{
   argc; argv;
   const char *ret = object->getName();
   return ret ? ret : "";
}

ConsoleMethod(SimObject, getClassName, const char *, 2, 2, "obj.getClassName()")
{
   argc; argv;
   const char *ret = object->getClassName();
   return ret ? ret : "";
}

ConsoleMethod(SimObject, getFieldValue, const char *, 3, 3, "obj.getFieldValue(fieldName);")
{
   argc; argv;

   const char *fieldName = StringTable->insert( argv[2] );
   return object->getDataField( fieldName, NULL );
}


ConsoleMethod(SimObject, setFieldValue, bool, 4, 4, "obj.setFieldValue(fieldName,value);")
{
   argc; argv;
   const char *fieldName = StringTable->insert(argv[2]);
   const char *value = argv[3];

   object->setDataField( fieldName, NULL, value );

   return true;

}


//-----------------------------------------------------------------------------
//	Set the internal name, can be used to find child objects
//	in a meaningful way, usually from script, while keeping
//	common script functionality together using the controls "Name" field.
//-----------------------------------------------------------------------------
ConsoleMethod( SimObject, setInternalName, void, 3, 3, "string InternalName")
{
   object->setInternalName(argv[2]);
}

void SimObject::setInternalName(const char* newname)
{
   if(newname)
      mInternalName = StringTable->insert(newname);
}

ConsoleMethod( SimObject, getInternalName, const char*, 2, 2, "getInternalName returns the objects internal name")
{
   return object->getInternalName();
}

StringTableEntry	SimObject::getInternalName()
{
   return mInternalName;
}


ConsoleMethod(SimObject, dumpClassHierarchy, void, 2, 2, "obj.dumpClassHierarchy()")
{
   object->dumpClassHierarchy();
}
ConsoleMethod(SimObject, isMemberOfClass, bool, 3, 3, " isMemberOfClass(string classname) -- returns true if this object is a member of the specified class")
{

   AbstractClassRep* pRep = object->getClassRep();
   while(pRep)
   {
      if(!dStricmp(pRep->getClassName(), argv[2]))
      {
         //matches
         return true;
      }

      pRep	=	pRep->getParentClass();
   }

   return false;
}
ConsoleMethod(SimObject, getId, S32, 2, 2, "obj.getId()")
{
   argc; argv;
   return object->getId();
}

ConsoleMethod(SimObject, getGroup, S32, 2, 2, "obj.getGroup()")
{
   argc; argv;
   SimGroup *grp = object->getGroup();
   if(!grp)
      return -1;
   return grp->getId();
}

ConsoleMethod(SimObject, delete, void, 2, 2,"obj.delete()")
{
   argc;argv;
   object->deleteObject();
}

ConsoleMethod(SimObject,schedule, S32, 4, 0, "object.schedule(time, command, <arg1...argN>);")
{
   U32 timeDelta = U32(dAtof(argv[2]));
   argv[2] = argv[3];
   argv[3] = argv[1];
   SimConsoleEvent *evt = new SimConsoleEvent(argc - 2, argv + 2, true);
   S32 ret = Sim::postEvent(object, evt, Sim::getCurrentTime() + timeDelta);
// #ifdef DEBUG
//    Con::printf("obj %s schedule(%s) = %d", argv[3], argv[2], ret);
//    Con::executef(1, "backtrace");
// #endif
   return ret;
}

static S32 QSORT_CALLBACK compareFields(const void* a,const void* b)
{
   const AbstractClassRep::Field* fa = *((const AbstractClassRep::Field**)a);
   const AbstractClassRep::Field* fb = *((const AbstractClassRep::Field**)b);

   return dStricmp(fa->pFieldname, fb->pFieldname);
}

ConsoleMethod(SimObject, getDynamicFieldCount, S32, 2, 2, "obj.getDynamicFieldCount()")
{
   S32 count = 0;
   SimFieldDictionary* fieldDictionary = object->getFieldDictionary();
   for (SimFieldDictionaryIterator itr(fieldDictionary); *itr; ++itr)
      count++;

   return count;
}

ConsoleMethod(SimObject, getDynamicField, const char*, 3, 3, "obj.getDynamicField(index)")
{
   SimFieldDictionary* fieldDictionary = object->getFieldDictionary();
   SimFieldDictionaryIterator itr(fieldDictionary);
   S32 index = dAtoi(argv[2]);
   for (S32 i = 0; i < index; i++)
   {
      if (!(*itr))
      {
         Con::warnf("Invalid dynamic field index passed to SimObject::getDynamicField!");
         return NULL;
      }
      ++itr;
   }

   char* buffer = Con::getReturnBuffer(256);
   if (*itr)
   {
      SimFieldDictionary::Entry* entry = *itr;
      dSprintf(buffer, 256, "%s\t%s", entry->slotName, entry->value);
      return buffer;
   }

   Con::warnf("Invalid dynamic field index passed to SimObject::getDynamicField!");
   return NULL;
}

ConsoleMethod(SimObject,dump, void, 2, 2, "obj.dump()")
{
   argc; argv;
   const AbstractClassRep::FieldList &list = object->getFieldList();
   char expandedBuffer[4096];

   Con::printf("Member Fields:");
   Vector<const AbstractClassRep::Field *> flist(__FILE__, __LINE__);

   for(U32 i = 0; i < list.size(); i++)
      flist.push_back(&list[i]);

   dQsort(flist.address(),flist.size(),sizeof(AbstractClassRep::Field *),compareFields);

   for(Vector<const AbstractClassRep::Field *>::iterator itr = flist.begin(); itr != flist.end(); itr++)
   {
      const AbstractClassRep::Field* f = *itr;
      if( f->type == AbstractClassRep::DepricatedFieldType ||
		  f->type == AbstractClassRep::StartGroupFieldType ||
		  f->type == AbstractClassRep::EndGroupFieldType) continue;

      for(U32 j = 0; S32(j) < f->elementCount; j++)
      {
         const char *val = Con::getData(f->type, (void *) (((const char *)object) + f->offset), j, f->table, f->flag);
         if(!val /*|| !*val*/)
            continue;
         if(f->elementCount == 1)
            dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s = \"", f->pFieldname);
         else
            dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s[%d] = \"", f->pFieldname, j);
         expandEscape(expandedBuffer + dStrlen(expandedBuffer), val);
         Con::printf("%s\"", expandedBuffer);
      }
   }

   Con::printf("Tagged Fields:");
   if(object->getFieldDictionary())
      object->getFieldDictionary()->printFields(object);

   Con::printf("Methods:");
   Namespace *ns = object->getNamespace();
   Vector<Namespace::Entry *> vec(__FILE__, __LINE__);

   if(ns)
      ns->getEntryList(&vec);

   for(Vector<Namespace::Entry *>::iterator j = vec.begin(); j != vec.end(); j++)
      Con::printf("  %s() - %s", (*j)->mFunctionName, (*j)->mUsage ? (*j)->mUsage : "");

}

ConsoleMethod(SimObject, getType, S32, 2, 2, "obj.getType()")
{
   argc; argv;
   return((S32)object->getType());
}

bool SimObject::isMethod( const char* methodName )
{
   if( !methodName || !methodName[0] )
      return false;

   StringTableEntry stname = StringTable->insert( methodName );

   if( getNamespace() )
      return ( getNamespace()->lookup( stname ) != NULL );

   return false;
}

ConsoleMethod(SimObject, isMethod, bool, 3, 3, "obj.isMethod(string method name)")
{
   return object->isMethod( argv[2] );
}


const char *SimObject::tabComplete(const char *prevText, S32 baseLen, bool fForward)
{
   return mNameSpace->tabComplete(prevText, baseLen, fForward);
}

void SimObject::setDataField(StringTableEntry slotName, const char *array, const char *value)
{
   // first search the static fields if enabled
   if(mFlags.test(ModStaticFields))
   {
      const AbstractClassRep::Field *fld = findField(slotName);
      if(fld)
      {
      if( fld->type == AbstractClassRep::DepricatedFieldType ||
		  fld->type == AbstractClassRep::StartGroupFieldType ||
		  fld->type == AbstractClassRep::EndGroupFieldType   ||
		  fld->readOnly) return;

         S32 array1 = array ? dAtoi(array) : 0;

         if(array1 >= 0 && array1 < fld->elementCount && fld->elementCount >= 1)
         {
            // If the set data notify callback returns true, then go ahead and
            // set the data, otherwise, assume the set notify callback has either
            // already set the data, or has deemed that the data should not
            // be set at all.
            FrameTemp<char> buffer(2048);
            FrameTemp<char> bufferSecure(2048); // This buffer is used to make a copy of the data 
            // so that if the prep functions or any other functions use the string stack, the data
            // is not corrupted.

            ConsoleBaseType *cbt = ConsoleBaseType::getType( fld->type );
            AssertFatal( cbt != NULL, "Could not resolve Type Id." );

            const char* szBuffer = cbt->prepData( value, buffer, 2048 );
            dMemset( bufferSecure, 0, 2048 );
            dMemcpy( bufferSecure, szBuffer, dStrlen( szBuffer ) );

            if( (*fld->setDataFn)( this, bufferSecure ) )
            Con::setData(fld->type, (void *) (((const char *)this) + fld->offset), array1, 1, &value, fld->table);
         }

         if(fld->validator)
            fld->validator->validateType(this, (void *) (((const char *)this) + fld->offset));
         /*else if(array1 == -1 && fld->elementCount == argc)
           {
           S32 i;
           for(i = 0; i < argc;i++)
           Con::setData(fld->type, (void *) (U32(this) + fld->offset), i, 1, argv + i, fld->table);
           }*/

         onStaticModified(slotName);
         return;
      }
   }

   if(mFlags.test(ModDynamicFields))
   {
      if(!mFieldDictionary)
         mFieldDictionary = new SimFieldDictionary;

      if(!array)
         mFieldDictionary->setFieldValue(slotName, value);
      else
      {
         char buf[256];
         dStrcpy(buf, slotName);
         dStrcat(buf, array);
         mFieldDictionary->setFieldValue(StringTable->insert(buf), value);
      }
   }
}


void  SimObject::dumpClassHierarchy()
{
   AbstractClassRep* pRep = getClassRep();
   while(pRep)
   {
      Con::warnf("%s ->", pRep->getClassName());
      pRep	=	pRep->getParentClass();
   }
}

const char *SimObject::getDataField(StringTableEntry slotName, const char *array)
{
   if(mFlags.test(ModStaticFields))
   {
      S32 array1 = array ? dAtoi(array) : -1;
      const AbstractClassRep::Field *fld = findField(slotName);
   
      if(fld)
      {
         if(array1 == -1 && fld->elementCount == 1)
            return (*fld->getDataFn)( this, Con::getData(fld->type, (void *) (((const char *)this) + fld->offset), 0, fld->table, fld->flag) );
         if(array1 >= 0 && array1 < fld->elementCount)
            return (*fld->getDataFn)( this, Con::getData(fld->type, (void *) (((const char *)this) + fld->offset), array1, fld->table, fld->flag) );// + typeSizes[fld.type] * array1));
         return "";
      }
   }

   if(mFlags.test(ModDynamicFields))
   {
      if(!mFieldDictionary)
         return "";

      if(!array) 
      {
         if (const char* val = mFieldDictionary->getFieldValue(slotName))
            return val;
      }
      else
      {
         static char buf[256];
         dStrcpy(buf, slotName);
         dStrcat(buf, array);
         if (const char* val = mFieldDictionary->getFieldValue(StringTable->insert(buf)))
            return val;
      }
   }

   return "";
}

SimObject::~SimObject()
{
   delete mFieldDictionary;

   AssertFatal(nextNameObject == (SimObject*)-1,avar(
                  "SimObject::~SimObject:  Not removed from dictionary: name %s, id %i",
                  objectName, mId));
   AssertFatal(nextManagerNameObject == (SimObject*)-1,avar(
                  "SimObject::~SimObject:  Not removed from manager dictionary: name %s, id %i",
                  objectName,mId));
   AssertFatal(mFlags.test(Added) == 0, "SimObject::object "
               "missing call to SimObject::onRemove");
}

//---------------------------------------------------------------------------

bool SimObject::isLocked()
{
   if(!mFieldDictionary)
      return false;

   const char * val = mFieldDictionary->getFieldValue( StringTable->insert( "locked", false ) );

   return( val ? dAtob(val) : false );
}

void SimObject::setLocked( bool b = true )
{
   setDataField(StringTable->insert("locked", false), NULL, b ? "true" : "false" );
}

bool SimObject::isHidden()
{
   if(!mFieldDictionary)
      return false;

   const char * val = mFieldDictionary->getFieldValue( StringTable->insert( "hidden", false ) );
   return( val ? dAtob(val) : false );
}

void SimObject::setHidden(bool b = true)
{
   setDataField(StringTable->insert("hidden", false), NULL, b ? "true" : "false" );
}

const char* SimObject::getIdString()
{
   static char IDbuffer[12];
   dSprintf(IDbuffer, sizeof(IDbuffer), "%d", mId);
   return IDbuffer;
}

//---------------------------------------------------------------------------

bool SimObject::onAdd()
{
   mFlags.set(Added);

   if(getClassRep())
      mNameSpace = getClassRep()->getNameSpace();

   // onAdd() should return FALSE if there was an error
   return true;
}

void SimObject::onRemove()
{
   mFlags.clear(Added);
}

void SimObject::onGroupAdd()
{
}

void SimObject::onGroupRemove()
{
}

void SimObject::onDeleteNotify(SimObject*)
{
}

void SimObject::onNameChange(const char*)
{
}

void SimObject::onStaticModified(const char*)
{
}

bool SimObject::processArguments(S32 argc, const char**)
{
   return argc == 0;
}

bool SimObject::isChildOfGroup(SimGroup* pGroup)
{
   if(!pGroup)
      return false;

   //if we *are* the group in question,
   //return true:
   if(pGroup == dynamic_cast<SimGroup*>(this))
      return true;

   SimGroup* temp	=	mGroup;
   while(temp)
   {
      if(temp == pGroup)
         return true;
      temp = temp->mGroup;
   }

   return false;
}


ConsoleMethod(SimObject, isChildOfGroup, bool, 3,3," returns true, if we are in the specified simgroup - or a subgroup thereof")
{
   SimGroup* pGroup = dynamic_cast<SimGroup*>(Sim::findObject(dAtoi(argv[2])));
   if(pGroup)
   {
      return object->isChildOfGroup(pGroup);
   }

   return false;
}
//---------------------------------------------------------------------------

static Chunker<SimObject::Notify> notifyChunker(128000);
SimObject::Notify *SimObject::mNotifyFreeList = NULL;

SimObject::Notify *SimObject::allocNotify()
{
   if(mNotifyFreeList)
   {
      SimObject::Notify *ret = mNotifyFreeList;
      mNotifyFreeList = ret->next;
      return ret;
   }
   return notifyChunker.alloc();
}

void SimObject::freeNotify(SimObject::Notify* note)
{
   AssertFatal(note->type != SimObject::Notify::Invalid, "Invalid notify");
   note->type = SimObject::Notify::Invalid;
   note->next = mNotifyFreeList;
   mNotifyFreeList = note;
}

//------------------------------------------------------------------------------

SimObject::Notify* SimObject::removeNotify(void *ptr, SimObject::Notify::Type type)
{
   Notify **list = &mNotifyList;
   while(*list)
   {
      if((*list)->ptr == ptr && (*list)->type == type)
      {
         SimObject::Notify *ret = *list;
         *list = ret->next;
         return ret;
      }
      list = &((*list)->next);
   }
   return NULL;
}

void SimObject::deleteNotify(SimObject* obj)
{
   AssertFatal(!obj->isDeleted(),
               "SimManager::deleteNotify: Object is being deleted");
   Notify *note = allocNotify();
   note->ptr = (void *) this;
   note->next = obj->mNotifyList;
   note->type = Notify::DeleteNotify;
   obj->mNotifyList = note;

   note = allocNotify();
   note->ptr = (void *) obj;
   note->next = mNotifyList;
   note->type = Notify::ClearNotify;
   mNotifyList = note;

   //obj->deleteNotifyList.pushBack(this);
   //clearNotifyList.pushBack(obj);
}

void SimObject::registerReference(SimObject **ptr)
{
   Notify *note = allocNotify();
   note->ptr = (void *) ptr;
   note->next = mNotifyList;
   note->type = Notify::ObjectRef;
   mNotifyList = note;
}

void SimObject::unregisterReference(SimObject **ptr)
{
   Notify *note = removeNotify((void *) ptr, Notify::ObjectRef);
   if(note)
      freeNotify(note);
}

void SimObject::clearNotify(SimObject* obj)
{
   Notify *note = obj->removeNotify((void *) this, Notify::DeleteNotify);
   if(note)
      freeNotify(note);

   note = removeNotify((void *) obj, Notify::ClearNotify);
   if(note)
      freeNotify(note);
}

void SimObject::processDeleteNotifies()
{
   // clear out any delete notifies and
   // object refs.

   while(mNotifyList)
   {
      Notify *note = mNotifyList;
      mNotifyList = note->next;

      AssertFatal(note->type != Notify::ClearNotify, "Clear notes should be all gone.");

      if(note->type == Notify::DeleteNotify)
      {
         SimObject *obj = (SimObject *) note->ptr;
         Notify *cnote = obj->removeNotify((void *)this, Notify::ClearNotify);
         obj->onDeleteNotify(this);
         freeNotify(cnote);
      }
      else
      {
         // it must be an object ref - a pointer refs this object
         *((SimObject **) note->ptr) = NULL;
      }
      freeNotify(note);
   }
}

void SimObject::clearAllNotifications()
{
   for(Notify **cnote = &mNotifyList; *cnote; )
   {
      Notify *temp = *cnote;
      if(temp->type == Notify::ClearNotify)
      {
         *cnote = temp->next;
         Notify *note = ((SimObject *) temp->ptr)->removeNotify((void *) this, Notify::DeleteNotify);
         freeNotify(temp);
         freeNotify(note);
      }
      else
         cnote = &(temp->next);
   }
}

//---------------------------------------------------------------------------

void SimObject::initPersistFields()
{
   Parent::initPersistFields();
   //add the canSaveDynamicFields property:
   addGroup("SimBase");
   addField("canSaveDynamicFields",		TypeBool,		Offset(mCanSaveFieldDictionary,			SimObject));
   addField("internalName",      TypeString,       Offset(mInternalName, SimObject));
   endGroup("SimBase");
}

bool SimObject::addToSet(SimObjectId spid)
{
   if (mFlags.test(Added) == false)
      return false;

   SimObject* ptr = Sim::findObject(spid);
   if (ptr) 
   {
      SimSet* sp = dynamic_cast<SimSet*>(ptr);
      AssertFatal(sp != 0,
                  "SimObject::addToSet: "
                  "ObjectId does not refer to a set object");
      sp->addObject(this);
      return true;
   }
   return false;
}

bool SimObject::addToSet(const char *ObjectName)
{
   if (mFlags.test(Added) == false)
      return false;

   SimObject* ptr = Sim::findObject(ObjectName);
   if (ptr) 
   {
      SimSet* sp = dynamic_cast<SimSet*>(ptr);
      AssertFatal(sp != 0,
                  "SimObject::addToSet: "
                  "ObjectName does not refer to a set object");
      sp->addObject(this);
      return true;
   }
   return false;
}

bool SimObject::removeFromSet(SimObjectId sid)
{
   if (mFlags.test(Added) == false)
      return false;

   SimSet *set;
   if(Sim::findObject(sid, set))
   {
      set->removeObject(this);
      return true;
   }
   return false;
}

bool SimObject::removeFromSet(const char *objectName)
{
   if (mFlags.test(Added) == false)
      return false;

   SimSet *set;
   if(Sim::findObject(objectName, set))
   {
      set->removeObject(this);
      return true;
   }
   return false;
}

void SimObject::inspectPreApply()
{
}

void SimObject::inspectPostApply()
{
}

IMPLEMENT_CONOBJECT(SimObject);

//---------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(SimDataBlock);
SimObjectId SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
S32 SimDataBlock::sNextModifiedKey = 0;

//---------------------------------------------------------------------------

SimDataBlock::SimDataBlock()
{
   setModDynamicFields(true);
   setModStaticFields(true);
}

bool SimDataBlock::onAdd()
{
   Parent::onAdd();

   // This initialization is done here, and not in the constructor,
   // because some jokers like to construct and destruct objects
   // (without adding them to the manager) to check what class
   // they are.
   modifiedKey = ++sNextModifiedKey;
   AssertFatal(sNextObjectId <= DataBlockObjectIdLast,
               "Exceeded maximum number of data blocks");

   // add DataBlock to the DataBlockGroup unless it is client side ONLY DataBlock
   if (getId() >= DataBlockObjectIdFirst && getId() <= DataBlockObjectIdLast)
      if (SimGroup* grp = Sim::getDataBlockGroup())
         grp->addObject(this);
   return true;
}

void SimDataBlock::assignId()
{
   // We don't want the id assigned by the manager, but it may have
   // already been assigned a correct data block id.
   if (getId() < DataBlockObjectIdFirst || getId() > DataBlockObjectIdLast)
      setId(sNextObjectId++);
}

void SimDataBlock::onStaticModified(const char*)
{
   modifiedKey = sNextModifiedKey++;

}

/*void SimDataBlock::setLastError(const char*)
{
} */

void SimDataBlock::packData(BitStream*)
{
}

void SimDataBlock::unpackData(BitStream*)
{
}

bool SimDataBlock::preload(bool, char[256])
{
   return true;
}

ConsoleFunction(deleteDataBlocks, void, 1, 1, "Delete all the datablocks we've downloaded. "
                "This is usually done in preparation of downloading a new set of datablocks, "
                " such as occurs on a mission change, but it's also good post-mission cleanup.")
{
   argc; argv;
   // delete from last to first:
   SimGroup *grp = Sim::getDataBlockGroup();
   for(S32 i = grp->size() - 1; i >= 0; i--)
   {
      SimObject *obj = (*grp)[i];
      obj->deleteObject();
   }
   SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
   SimDataBlock::sNextModifiedKey = 0;
}

//---------------------------------------------------------------------------

void SimSet::addObject(SimObject* obj)
{
   lock();
   objectList.pushBack(obj);
   deleteNotify(obj);
   unlock();
}

void SimSet::removeObject(SimObject* obj)
{
   lock();
   objectList.remove(obj);
   clearNotify(obj);
   unlock();
}

void SimSet::pushObject(SimObject* pObj)
{
   lock();
   objectList.pushBackForce(pObj);
   deleteNotify(pObj);
   unlock();
}

void SimSet::popObject()
{
   MutexHandle handle;
   handle.lock(mMutex);

   if (objectList.size() == 0) 
   {
      AssertWarn(false, "Stack underflow in SimSet::popObject");
      return;
   }

   SimObject* pObject = objectList[objectList.size() - 1];

   objectList.removeStable(pObject);
   clearNotify(pObject);
}

bool SimSet::reOrder( SimObject *obj, SimObject *target )
{
   MutexHandle handle;
   handle.lock(mMutex);

   iterator itrS, itrD;
   if ( (itrS = find(begin(),end(),obj)) == end() )
   {
      return false;  // object must be in list
   }

   if ( obj == target )
   {
      return true;   // don't reorder same object but don't indicate error
   }

   if ( !target )    // if no target, then put to back of list
   {
      if ( itrS != (end()-1) )      // don't move if already last object
      {
         objectList.erase(itrS);    // remove object from its current location
         objectList.push_back(obj); // push it to the back of the list
      }
   }
   else              // if target, insert object in front of target
   {
      if ( (itrD = find(begin(),end(),target)) == end() )
         return false;              // target must be in list

      objectList.erase(itrS);
		
      //Tinman - once itrS has been erased, itrD won't be pointing at the same place anymore - re-find...
      itrD = find(begin(),end(),target);
      objectList.insert(itrD,obj);
   }

   return true;
}   

void SimSet::onDeleteNotify(SimObject *object)
{
   removeObject(object);
   Parent::onDeleteNotify(object);
}

void SimSet::onRemove()
{
   MutexHandle handle;
   handle.lock(mMutex);

   objectList.sortId();
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
           ptr >= objectList.begin(); ptr--)
      {
         clearNotify(*ptr);
      }
   }

   handle.unlock();

   Parent::onRemove();
}

void SimSet::write(Stream &stream, U32 tabStop, U32 flags)
{
   MutexHandle handle;
   handle.lock(mMutex);

   // export selected only?
   if((flags & SelectedOnly) && !isSelected())
   {
      for(U32 i = 0; i < size(); i++)
         (*this)[i]->write(stream, tabStop, flags);

      return;

   }

   writeTabs(stream, tabStop);
   char buffer[1024];
   dSprintf(buffer, sizeof(buffer), "new %s(%s)\r\n", getClassName(), getName() ? getName() : "");
   writeTabs(stream, (tabStop == 0 ? 0 : (tabStop == 1 ? tabStop - 1 : (tabStop / 2) - 1)));
   stream.write(dStrlen(buffer), buffer);
   writeTabs(stream, tabStop);
   stream.write(3, "{\r\n");
   writeFields(stream, tabStop + 1, flags);

   if(size())
   {
      stream.write(2, "\r\n");
      for(U32 i = 0; i < size(); i++)
         (*this)[i]->write(stream, tabStop + 1, flags);
   }

   writeTabs(stream, tabStop);
   stream.write(4, "};\r\n");
}

void SimSet::deleteAll() {
	while(objectList.size() != 0) {
		SimObject* obj = objectList[0];
		SimSet* set    = dynamic_cast<SimSet*>(obj);

		if (set) set->deleteAll(); 
		obj->deleteObject();
	}
}

ConsoleMethod(SimSet, deleteAll, void, 2, 2, "set.deleteAll();") {
	object->deleteAll();
}

ConsoleMethod(SimSet, listObjects, void, 2, 2, "set.listObjects();")
{
   argc; argv;

   object->lock();
   SimSet::iterator itr;
   for(itr = object->begin(); itr != object->end(); itr++)
   {
      SimObject *obj = *itr;
      bool isSet = dynamic_cast<SimSet *>(obj) != 0;
      const char *name = obj->getName();
      if(name)
         Con::printf("   %d,\"%s\": %s %s", obj->getId(), name,
                     obj->getClassName(), isSet ? "(g)":"");
      else
         Con::printf("   %d: %s %s", obj->getId(), obj->getClassName(),
                     isSet ? "(g)" : "");
   }
   object->unlock();
}

ConsoleMethod(SimSet, add, void, 3, 0, "set.add(obj1,...)")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      if(obj)
         object->addObject(obj);
      else
         Con::printf("Set::add: Object \"%s\" doesn't exist", argv[i]);
   }
}

ConsoleMethod(SimSet, remove, void, 3, 0, "set.remove(obj1,...)")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      object->lock();
      if(obj && object->find(object->begin(),object->end(),obj) != object->end())
         object->removeObject(obj);
      else
         Con::printf("Set::remove: Object \"%s\" does not exist in set", argv[i]);
      object->unlock();
   }
}

ConsoleMethod(SimSet, clear, void, 2, 2, "set.clear()")
{
   argc; argv;
   object->lock();
   while (object->size() > 0)
      object->removeObject(*(object->begin()));
   object->unlock();
}

//-----------------------------------------------------------------------------
//	Make Sure Child 1 is Ordered Just Under Child 2.
//-----------------------------------------------------------------------------
ConsoleMethod(SimSet, reorderChild, void, 4,4," (child1, child2) uses simset reorder to push child 1 before child 2 - both must already be child controls of this control")
{
   SimObject* pObject = Sim::findObject(argv[2]);
   SimObject* pTarget	 = Sim::findObject(argv[3]);

   if(pObject && pTarget)
   {
      object->reOrder(pObject,pTarget);
   }
}

ConsoleMethod(SimSet, getCount, S32, 2, 2, "set.getCount()")
{
   argc; argv;
   return object->size();
}

ConsoleMethod(SimSet, getObject, S32, 3, 3, "set.getObject(objIndex)")
{
   argc;
   S32 objectIndex = dAtoi(argv[2]);
   if(objectIndex < 0 || objectIndex >= S32(object->size()))
   {
      Con::printf("Set::getObject index out of range.");
      return -1;
   }
   return ((*object)[objectIndex])->getId();
}

ConsoleMethod(SimSet, isMember, bool, 3, 3, "set.isMember(object)")
{
   argc;
   SimObject *testObject = Sim::findObject(argv[2]);
   if(!testObject)
   {
      Con::printf("SimSet::isMember: %s is not an object.", argv[2]);
      return false;
   }

   object->lock();
   for(SimSet::iterator i = object->begin(); i != object->end(); i++)
   {
      if(*i == testObject)
      {
         object->unlock();
         return true;
      }
   }
   object->unlock();

   return false;
}

ConsoleMethod(SimSet, bringToFront, void, 3, 3, "set.bringToFront(object)")
{
   argc;
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->bringObjectToFront(obj);
}

ConsoleMethod(SimSet, pushToBack, void, 3, 3, "set.pushToBack(object)")
{
   argc;
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->pushObjectToBack(obj);
}

ConsoleMethod(SimSet, reorder, void, 4, 4, "set.reorder(obj1, obj2)")
{
   SimObject *obj1 = Sim::findObject(argv[2]);
   SimObject *obj2 = Sim::findObject(argv[3]);
   if(obj1 == NULL || obj2 == NULL)
      return;
   
   object->reOrder(obj1, obj2);
}

//----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(SimSet);

//---------------------------------------------------------------------------

SimGroup::~SimGroup()
{
   lock();
   for (iterator itr = begin(); itr != end(); itr++)
      nameDictionary.remove(*itr);

   // XXX Move this later into Group Class
   // If we have any objects at this point, they should
   // already have been removed from the manager, so we
   // can just delete them directly.
   objectList.sortId();
   while (!objectList.empty()) 
   {
      delete objectList.last();
      objectList.decrement();
   }

   unlock();
}


//---------------------------------------------------------------------------

void SimGroup::addObject(SimObject* obj)
{
   lock();

   // Make sure we aren't adding ourself.  This isn't the most robust check
   // but it should be good enough to prevent some self-foot-shooting.
   if(obj == this)
   {
      Con::errorf("SimGroup::addObject - (%d) can't add self!", getIdString());
      unlock();
      return;
   }

   if (obj->mGroup != this) 
   {
      if (obj->mGroup)
         obj->mGroup->removeObject(obj);
      nameDictionary.insert(obj);
      obj->mGroup = this;
      objectList.push_back(obj); // force it into the object list
                                 // doesn't get a delete notify
      obj->onGroupAdd();
   }
   unlock();
}

void SimGroup::removeObject(SimObject* obj)
{
   lock();
   if (obj->mGroup == this) 
   {
      obj->onGroupRemove();
      nameDictionary.remove(obj);
      objectList.remove(obj);
      obj->mGroup = 0;
   }
   unlock();
}

//---------------------------------------------------------------------------

void SimGroup::onRemove()
{
   lock();
   objectList.sortId();
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
           ptr >= objectList.begin(); ptr--)
      {
         (*ptr)->onGroupRemove();
         (*ptr)->mGroup = NULL;
         (*ptr)->unregisterObject();
         (*ptr)->mGroup = this;
      }
   }
   SimObject::onRemove();
   unlock();
}

//---------------------------------------------------------------------------

SimObject *SimGroup::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   SimObject *root = nameDictionary.find(stName);

   if(!root)
      return NULL;

   if(namePath[len] == 0)
      return root;

   return root->findObject(namePath + len + 1);
}

SimObject *SimSet::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   lock();
   for(SimSet::iterator i = begin(); i != end(); i++)
   {
      if((*i)->getName() == stName)
      {
         unlock();
         if(namePath[len] == 0)
            return *i;
         return (*i)->findObject(namePath + len + 1);
      }
   }
   unlock();
   return NULL;
}

SimObject* SimObject::findObject(const char* )
{
   return NULL;
}

//---------------------------------------------------------------------------

bool SimGroup::processArguments(S32, const char **)
{
   return true;
}

ConsoleMethod( SimGroup, findObjectByInternalName, S32, 3, 4, "string InternalName")
{

   StringTableEntry pcName = StringTable->insert(argv[2]);
   bool searchChildren = false;
   if (argc > 3)
      searchChildren = dAtob(argv[3]);

   SimObject* child = object->findObjectByInternalName(pcName, searchChildren);
   if(child)
      return child->getId();
   return 0;
}

SimObject* SimGroup::findObjectByInternalName(const char* internalName, bool searchChildren)
{
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      SimObject *childObj = static_cast<SimObject*>(*i);
      if(childObj->getInternalName() == internalName)
         return childObj;
      else if (searchChildren)
      {
         SimGroup* childGroup = dynamic_cast<SimGroup*>(*i);
         if (childGroup)
         {
            SimObject* found = childGroup->findObjectByInternalName(internalName, searchChildren);
            if (found) return found;
         }
      }
   }

   return NULL;
}

//----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(SimGroup);

//------------------------------------------------------------------------------

SimConsoleEvent::SimConsoleEvent(S32 argc, const char **argv, bool onObject)
{
   mOnObject = onObject;
   mArgc = argc;
   U32 totalSize = 0;
   S32 i;
   for(i = 0; i < argc; i++)
      totalSize += dStrlen(argv[i]) + 1;
   totalSize += sizeof(char *) * argc;

   mArgv = (char **) dMalloc(totalSize);
   char *argBase = (char *) &mArgv[argc];

   for(i = 0; i < argc; i++)
   {
      mArgv[i] = argBase;
      dStrcpy(mArgv[i], argv[i]);
      argBase += dStrlen(argv[i]) + 1;
   }
}

SimConsoleEvent::~SimConsoleEvent()
{
   dFree(mArgv);
}

void SimConsoleEvent::process(SimObject* object)
{
// #ifdef DEBUG
//    Con::printf("Executing schedule: %d", sequenceCount);
// #endif
   if(mOnObject)
      Con::execute(object, mArgc, const_cast<const char**>( mArgv ));
   else
      Con::execute(mArgc, const_cast<const char**>( mArgv ));
}

//-----------------------------------------------------------------------------

SimConsoleThreadExecCallback::SimConsoleThreadExecCallback() : retVal(NULL)
{
   sem = Semaphore::createSemaphore(0);
}

SimConsoleThreadExecCallback::~SimConsoleThreadExecCallback()
{
   Semaphore::destroySemaphore(sem);
}

void SimConsoleThreadExecCallback::handleCallback(const char *ret)
{
   retVal = ret;
   Semaphore::releaseSemaphore(sem);
}

const char *SimConsoleThreadExecCallback::waitForResult()
{
   if(Semaphore::acquireSemaphore(sem, true))
   {
      return retVal;
   }

   return NULL;
}

//-----------------------------------------------------------------------------

SimConsoleThreadExecEvent::SimConsoleThreadExecEvent(S32 argc, const char **argv, bool onObject, SimConsoleThreadExecCallback *callback) : 
   SimConsoleEvent(argc, argv, onObject),
   cb(callback)
{
}

void SimConsoleThreadExecEvent::process(SimObject* object)
{
   const char *retVal;
   if(mOnObject)
      retVal = Con::execute(object, mArgc, const_cast<const char**>( mArgv ));
   else
      retVal = Con::execute(mArgc, const_cast<const char**>( mArgv ));

   if(cb)
      cb->handleCallback(retVal);
}

//-----------------------------------------------------------------------------

SimEngineEvent::SimEngineEvent(SimEngineEventFunc funcPtr, void* userData)
{
	mFuncPtr  = funcPtr;
	mUserData = userData;
}

SimEngineEvent::~SimEngineEvent()
{
}

void SimEngineEvent::process(SimObject* object)
{
	mFuncPtr(mUserData);
}

//-----------------------------------------------------------------------------

inline void SimSetIterator::Stack::push_back(SimSet* set)
{
   increment();
   last().set = set;
   last().itr = set->begin();
}


//-----------------------------------------------------------------------------

SimSetIterator::SimSetIterator(SimSet* set)
{
   VECTOR_SET_ASSOCIATION(stack);

   if (!set->empty())
      stack.push_back(set);
}


//-----------------------------------------------------------------------------

SimObject* SimSetIterator::operator++()
{
   SimSet* set;
   if ((set = dynamic_cast<SimSet*>(*stack.last().itr)) != 0) 
   {
      if (!set->empty()) 
      {
         stack.push_back(set);
         return *stack.last().itr;
      }
   }

   while (++stack.last().itr == stack.last().set->end()) 
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.last().itr;
}	

SimObject* SimGroupIterator::operator++()
{
   SimGroup* set;
   if ((set = dynamic_cast<SimGroup*>(*stack.last().itr)) != 0) 
   {
      if (!set->empty()) 
      {
         stack.push_back(set);
         return *stack.last().itr;
      }
   }

   while (++stack.last().itr == stack.last().set->end()) 
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.last().itr;
}	

