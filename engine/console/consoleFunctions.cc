//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/ast.h"
#include "core/resManager.h"
#include "core/fileStream.h"
#include "console/compiler.h"
#include "platform/event.h"
#include "platform/gameInterface.h"
#include "platform/platformInput.h"
#include "dgl/gBitmap.h"

// This is a temporary hack to get tools using the library to
// link in this module which contains no other references.
bool LinkConsoleFunctions = false;

// Buffer for expanding script filenames.
static char scriptFilenameBuffer[1024];
GBitmap* currBitmap = NULL;

//----------------------------------------------------------------
ConsoleFunction(freeBitmap, void, 1, 1, "()") {
	if (currBitmap == NULL) return;
	delete currBitmap;
	currBitmap = NULL;
}

ConsoleFunction(loadBitmap, bool, 2, 2, "(name)") {
	if (currBitmap != NULL) return false;
	currBitmap = GBitmap::load(argv[1]);
	return !(currBitmap == NULL);
}

ConsoleFunction(getBitmapColor, const char*, 3, 3, "(x, y)") {
	ColorI col;
	if (currBitmap == NULL) {
		Con::errorf("%s() - no bitmap set", argv[0]);
		return StringTable->insert("0 0 0 0");
	}

	currBitmap->getColor(dAtoi(argv[1]), dAtoi(argv[2]), col);

	char* ret = Con::getReturnBuffer(32);
	dSprintf(ret, 32, "%d %d %d %d", col.red, col.green, col.blue, col.alpha);

	return ret;
}

ConsoleFunction(getBitmapExtent, const char*, 1, 1, "()") {
	if (currBitmap == NULL) {
		Con::errorf("%s() - no bitmap set", argv[0]);
		return StringTable->insert("0 0");
	}

	S32 width  = currBitmap->getWidth();
	S32 height = currBitmap->getHeight();

	char* ret = Con::getReturnBuffer(128);
	dSprintf(ret, 16, "%d %d", width, height);
	return ret;
}

//----------------------------------------------------------------

ConsoleFunction(expandFilename, const char*, 2, 2, "(string filename)")
{
   argc;
   char* ret = Con::getReturnBuffer( 1024 );
   Con::expandScriptFilename(ret, 1024, argv[1]);
   return ret;
}

ConsoleFunctionGroupBegin(StringFunctions, "General string manipulation functions.");

ConsoleFunction(chr, const char*, 2, 2, "(int charCode)")
{
	U8 index = (U8)dAtoi(argv[2]);
	char* ret = Con::getReturnBuffer(2);
	ret[0] = index;
	ret[1] = 0;

	return ret;
}

ConsoleFunction(ord, S32, 2, 2, "(string chr)")
{
	return *argv[1];
}

ConsoleFunction(strcmp, S32, 3, 3, "(string one, string two)"
                "Case sensitive string compare.")
{
   argc;
   return dStrcmp(argv[1], argv[2]);
}

ConsoleFunction(stricmp, S32, 3, 3, "(string one, string two)"
                "Case insensitive string compare.")
{
   argc;
   return dStricmp(argv[1], argv[2]);
}

ConsoleFunction(strlen, S32, 2, 2, "(string str)"
               "Calculate the length of a string in characters.")
{
   argc;
   return dStrlen(argv[1]);
}

ConsoleFunction(strstr, S32 , 3, 3, "(string one, string two) "
                "Returns the start of the sub string two in one or"
                " -1 if not found.")
{
   argc;
   // returns the start of the sub string argv[2] in argv[1]
   // or -1 if not found.

   const char *retpos = dStrstr(argv[1], argv[2]);
   if(!retpos)
      return -1;
   return retpos - argv[1];
}

ConsoleFunction(strpos, S32, 3, 4, "(string hay, string needle, int offset=0) "
                "Find needle in hay, starting offset bytes in.")
{
   S32 ret = -1;
   S32 start = 0;
   if(argc == 4)
      start = dAtoi(argv[3]);
   U32 sublen = dStrlen(argv[2]);
   U32 strlen = dStrlen(argv[1]);
   if(start < 0)
      return -1;
   if(sublen + start > strlen)
      return -1;
   for(; start + sublen <= strlen; start++)
      if(!dStrncmp(argv[1] + start, argv[2], sublen))
         return start;
   return -1;
}

ConsoleFunction(ltrim, const char *,2,2,"(string value)")
{
   argc;
   const char *ret = argv[1];
   while(*ret == ' ' || *ret == '\n' || *ret == '\t')
      ret++;
   return ret;
}

ConsoleFunction(rtrim, const char *,2,2,"(string value)")
{
   argc;
   S32 firstWhitespace = 0;
   S32 pos = 0;
   const char *str = argv[1];
   while(str[pos])
   {
      if(str[pos] != ' ' && str[pos] != '\n' && str[pos] != '\t')
         firstWhitespace = pos + 1;
      pos++;
   }
   char *ret = Con::getReturnBuffer(firstWhitespace + 1);
   dStrncpy(ret, argv[1], firstWhitespace);
   ret[firstWhitespace] = 0;
   return ret;
}

ConsoleFunction(trim, const char *,2,2,"(string)")
{
   argc;
   const char *ptr = argv[1];
   while(*ptr == ' ' || *ptr == '\n' || *ptr == '\t')
      ptr++;
   S32 firstWhitespace = 0;
   S32 pos = 0;
   const char *str = ptr;
   while(str[pos])
   {
      if(str[pos] != ' ' && str[pos] != '\n' && str[pos] != '\t')
         firstWhitespace = pos + 1;
      pos++;
   }
   char *ret = Con::getReturnBuffer(firstWhitespace + 1);
   dStrncpy(ret, ptr, firstWhitespace);
   ret[firstWhitespace] = 0;
   return ret;
}

ConsoleFunction(stripChars, const char*, 3, 3, "(string value, string chars) "
                "Remove all the characters in chars from value." )
{
   argc;
   char* ret = Con::getReturnBuffer( dStrlen( argv[1] ) + 1 );
   dStrcpy( ret, argv[1] );
   U32 pos = dStrcspn( ret, argv[2] );
   while ( pos < dStrlen( ret ) )
   {
      dStrcpy( ret + pos, ret + pos + 1 );
      pos = dStrcspn( ret, argv[2] );
   }
   return( ret );
}

ConsoleFunction(stripColorCodes, const char*, 2,2,  "(stringtoStrip) - "
                "remove TorqueML color codes from the string.")
{
   char* ret = Con::getReturnBuffer( dStrlen( argv[1] ) + 1 );
   dStrcpy(ret, argv[1]);
   Con::stripColorChars(ret);
   return ret;
}

ConsoleFunction(strlwr,const char *,2,2,"(string) "
                "Convert string to lower case.")
{
   argc;
   char *ret = Con::getReturnBuffer(dStrlen(argv[1]) + 1);
   dStrcpy(ret, argv[1]);
   return dStrlwr(ret);
}

ConsoleFunction(strupr,const char *,2,2,"(string) "
                "Convert string to upper case.")
{
   argc;
   char *ret = Con::getReturnBuffer(dStrlen(argv[1]) + 1);
   dStrcpy(ret, argv[1]);
   return dStrupr(ret);
}

ConsoleFunction(strchr,const char *,3,3,"(string,char)")
{
   argc;
   const char *ret = dStrchr(argv[1], argv[2][0]);
   return ret ? ret : "";
}

ConsoleFunction(strreplace, const char *, 4, 4, "(string source, string from, string to)")
{
   argc;
   S32 fromLen = dStrlen(argv[2]);
   if(!fromLen)
      return argv[1];

   S32 toLen = dStrlen(argv[3]);
   S32 count = 0;
   const char *scan = argv[1];
   while(scan)
   {
      scan = dStrstr(scan, argv[2]);
      if(scan)
      {
         scan += fromLen;
         count++;
      }
   }
   char *ret = Con::getReturnBuffer(dStrlen(argv[1]) + 1 + (toLen - fromLen) * count);
   U32 scanp = 0;
   U32 dstp = 0;
   for(;;)
   {
      const char *scan = dStrstr(argv[1] + scanp, argv[2]);
      if(!scan)
      {
         dStrcpy(ret + dstp, argv[1] + scanp);
         return ret;
      }
      U32 len = scan - (argv[1] + scanp);
      dStrncpy(ret + dstp, argv[1] + scanp, len);
      dstp += len;
      dStrcpy(ret + dstp, argv[3]);
      dstp += toLen;
      scanp += len + fromLen;
   }
   return ret;
}

ConsoleFunction(getSubStr, const char *, 4, 4, "getSubStr(string str, int start, int numChars) "
                "Returns the substring of str, starting at start, and continuing "
                "to either the end of the string, or numChars characters, whichever "
                "comes first.")
{
   argc;
   // Returns the substring of argv[1], starting at argv[2], and continuing
   //  to either the end of the string, or argv[3] characters, whichever
   //  comes first.
   //
   S32 startPos   = dAtoi(argv[2]);
   S32 desiredLen = dAtoi(argv[3]);
   if (startPos < 0 || desiredLen < 0) {
      Con::errorf(ConsoleLogEntry::Script, "getSubStr(...): error, starting position and desired length must be >= 0: (%d, %d)", startPos, desiredLen);

      return "";
   }

   S32 baseLen = dStrlen(argv[1]);
   if (baseLen < startPos)
      return "";

   U32 actualLen = desiredLen;
   if (startPos + desiredLen > baseLen)
      actualLen = baseLen - startPos;

   char *ret = Con::getReturnBuffer(actualLen + 1);
   dStrncpy(ret, argv[1] + startPos, actualLen);
   ret[actualLen] = '\0';

   return ret;
}

// Used?
ConsoleFunction( stripTrailingSpaces, const char*, 2, 2, "stripTrailingSpaces( string )" )
{
   argc;
   S32 temp = S32(dStrlen( argv[1] ));
   if ( temp )
   {
      while ( ( argv[1][temp - 1] == ' ' || argv[1][temp - 1] == '_' ) && temp >= 1 )
         temp--;

      if ( temp )
      {
         char* returnString = Con::getReturnBuffer( temp + 1 );
         dStrncpy( returnString, argv[1], U32(temp) );
         returnString[temp] = '\0';
         return( returnString );			
      }
   }

   return( "" );	
}

ConsoleFunctionGroupEnd(StringFunctions);

//--------------------------------------

static const char *getUnit(const char *string, U32 index, const char *set, char* buffer = NULL)
{
   U32 sz;
   while(index--)
   {
      if(!*string)
         return "";
      sz = dStrcspn(string, set);
      if (string[sz] == 0)
         return "";
      string += (sz + 1);
   }
   sz = dStrcspn(string, set);
   if (sz == 0)
      return "";

   if (buffer != NULL)
   {
	   dStrncpy(buffer, string, sz);
	   buffer[sz] = 0;
	   return buffer;
   }

   char *ret = Con::getReturnBuffer(sz+1);
   dStrncpy(ret, string, sz);
   ret[sz] = '\0';
   return ret;
}

static const char *getUnits(const char *string, S32 startIndex, S32 endIndex, const char *set)
{
   S32 sz;
   S32 index = startIndex;
   while(index--)
   {
      if(!*string)
         return "";
      sz = dStrcspn(string, set);
      if (string[sz] == 0)
         return "";
      string += (sz + 1);
   }
   const char *startString = string;
   while(startIndex <= endIndex--)
   {
      sz = dStrcspn(string, set);
      string += sz;
      if (*string == 0)
         break;
      string++;
   }
   if(!*string)
      string++;
   U32 totalSize = (U32(string - startString));
   char *ret = Con::getReturnBuffer(totalSize);
   dStrncpy(ret, startString, totalSize - 1);
   ret[totalSize-1] = '\0';
   return ret;
}

static U32 getUnitCount(const char *string, const char *set)
{
   U32 count = 0;
   U8 last = 0;
   while(*string)
   {
      last = *string++;

      for(U32 i =0; set[i]; i++)
      {
         if(last == set[i])
         {
            count++;
            last = 0;
            break;
         }
      }
   }
   if(last)
      count++;
   return count;
}


static const char* setUnit(const char *string, U32 index, const char *replace, const char *set)
{
   U32 sz;
   const char *start = string;
   char *ret = Con::getReturnBuffer(dStrlen(string) + dStrlen(replace) + 1);
   ret[0] = '\0';
   U32 padCount = 0;

   while(index--)
   {
      sz = dStrcspn(string, set);
      if(string[sz] == 0)
      {
         string += sz;
         padCount = index + 1;
         break;
      }
      else
         string += (sz + 1);
   }
   // copy first chunk
   sz = string-start;
   dStrncpy(ret, start, sz);
   for(U32 i = 0; i < padCount; i++)
      ret[sz++] = set[0];

   // replace this unit
   ret[sz] = '\0';
   dStrcat(ret, replace);

   // copy remaining chunks
   sz = dStrcspn(string, set);         // skip chunk we're replacing
   if(!sz && !string[sz])
      return ret;

   string += sz;
   dStrcat(ret, string);
   return ret;
}


static const char* removeUnit(const char *string, U32 index, const char *set)
{
   U32 sz;
   const char *start = string;
   char *ret = Con::getReturnBuffer(dStrlen(string) + 1);
   ret[0] = '\0';
   U32 padCount = 0;

   while(index--)
   {
      sz = dStrcspn(string, set);
      // if there was no unit out there... return the original string
      if(string[sz] == 0)
         return start;
      else
         string += (sz + 1);
   }
   // copy first chunk
   sz = string-start;
   dStrncpy(ret, start, sz);
   ret[sz] = 0;

   // copy remaining chunks
   sz = dStrcspn(string, set);         // skip chunk we're removing

   if(string[sz] == 0) {               // if that was the last...
      if(string != start) {
         ret[string - start - 1] = 0;  // then kill any trailing delimiter
      }
      return ret;                      // and bail
   }

   string += sz + 1; // skip the extra field delimiter
   dStrcat(ret, string);
   return ret;
}


//--------------------------------------
ConsoleFunctionGroupBegin( FieldManipulators, "Functions to manipulate data returned in the form of \"x y z\".");

ConsoleFunction(getWordPos, S32, 3, 5, "(string text, string word, start=0, caseSensitive=false)")
{
	if (*argv[1] == 0)
		return -1;

	char buffer[1024];
	const char* text   = argv[1];
	const char* toFind = argv[2];
	bool caseSensitive = (argc >= 5 ? dAtob(argv[4]) : false);
	S32 start          = (argc >= 4 ? dAtoi(argv[3]) : 0);
	U32 count          = getUnitCount(text, " \t\n");

	if (start < 0 || start >= count)
	{
		Con::errorf("getWordPos(...): error, starting index was invalid; index = %d, size = %d", start, count);
		return -1;
	}
	
	// Attempt to find the unit
	for (int i = start; i < count; i++)
	{
		const char* word = getUnit(text, i, " \t\n", buffer);
		if ((caseSensitive && dStrcmp(toFind, word)) || (!caseSensitive && dStricmp(toFind, word)))
			continue;

		return i;
	}

	return -1;
}

ConsoleFunction(getWord, const char *, 3, 3, "(string text, int index)")
{
   argc;
   return getUnit(argv[1], dAtoi(argv[2]), " \t\n");
}

ConsoleFunction(getWords, const char *, 3, 4, "(string text, int index, int endIndex=INF)")
{
   U32 endIndex;
   if(argc==3)
      endIndex = 1000000;
   else
      endIndex = dAtoi(argv[3]);
   return getUnits(argv[1], dAtoi(argv[2]), endIndex, " \t\n");
}

ConsoleFunction(setWord, const char *, 4, 4, "newText = setWord(text, index, replace)")
{
   argc;
   return setUnit(argv[1], dAtoi(argv[2]), argv[3], " \t\n");
}

ConsoleFunction(removeWord, const char *, 3, 3, "newText = removeWord(text, index)")
{
   argc;
   return removeUnit(argv[1], dAtoi(argv[2]), " \t\n");
}

ConsoleFunction(getWordCount, S32, 2, 2, "getWordCount(text)")
{
   argc;
   return getUnitCount(argv[1], " \t\n");
}

//--------------------------------------
ConsoleFunction(getFieldPos, S32, 3, 5, "(string text, string word, start=0, caseSensitive=false)")
{
	if (*argv[1] == 0)
		return -1;

	char buffer[1024];
	const char* text = argv[1];
	const char* toFind = argv[2];
	bool caseSensitive = (argc >= 5 ? dAtob(argv[4]) : false);
	S32 start = (argc >= 4 ? dAtoi(argv[3]) : 0);
	U32 count = getUnitCount(text, "\t\n");

	if (start < 0 || start >= count)
	{
		Con::errorf("getFieldPos(...): error, starting index was invalid; index = %d, size = %d", start, count);
		return -1;
	}

	// Attempt to find the unit
	for (int i = start; i < count; i++)
	{
		const char* word = getUnit(text, i, "\t\n", buffer);
		if ((caseSensitive && dStrcmp(toFind, word)) || (!caseSensitive && dStricmp(toFind, word)))
			continue;

		return i;
	}

	return -1;
}

ConsoleFunction(getField, const char *, 3, 3, "getField(text, index)")
{
   argc;
   return getUnit(argv[1], dAtoi(argv[2]), "\t\n");
}

ConsoleFunction(getFields, const char *, 3, 4, "getFields(text, index [,endIndex])")
{
   U32 endIndex;
   if(argc==3)
      endIndex = 1000000;
   else
      endIndex = dAtoi(argv[3]);
   return getUnits(argv[1], dAtoi(argv[2]), endIndex, "\t\n");
}

ConsoleFunction(setField, const char *, 4, 4, "newText = setField(text, index, replace)")
{
   argc;
   return setUnit(argv[1], dAtoi(argv[2]), argv[3], "\t\n");
}

ConsoleFunction(removeField, const char *, 3, 3, "newText = removeField(text, index)" )
{
   argc;
   return removeUnit(argv[1], dAtoi(argv[2]), "\t\n");
}

ConsoleFunction(getFieldCount, S32, 2, 2, "getFieldCount(text)")
{
   argc;
   return getUnitCount(argv[1], "\t\n");
}

//--------------------------------------
ConsoleFunction(getRecord, const char *, 3, 3, "getRecord(text, index)")
{
   argc;
   return getUnit(argv[1], dAtoi(argv[2]), "\n");
}

ConsoleFunction(getRecords, const char *, 3, 4, "getRecords(text, index [,endIndex])")
{
   U32 endIndex;
   if(argc==3)
      endIndex = 1000000;
   else
      endIndex = dAtoi(argv[3]);
   return getUnits(argv[1], dAtoi(argv[2]), endIndex, "\n");
}

ConsoleFunction(setRecord, const char *, 4, 4, "newText = setRecord(text, index, replace)")
{
   argc;
   return setUnit(argv[1], dAtoi(argv[2]), argv[3], "\n");
}

ConsoleFunction(removeRecord, const char *, 3, 3, "newText = removeRecord(text, index)" )
{
   argc;
   return removeUnit(argv[1], dAtoi(argv[2]), "\n");
}

ConsoleFunction(getRecordCount, S32, 2, 2, "getRecordCount(text)")
{
   argc;
   return getUnitCount(argv[1], "\n");
}
//--------------------------------------
ConsoleFunction(firstWord, const char *, 2, 2, "firstWord(text)")
{
   argc;
   const char *word = dStrchr(argv[1], ' ');
   U32 len;
   if(word == NULL)
      len = dStrlen(argv[1]);
   else
      len = word - argv[1];
   char *ret = Con::getReturnBuffer(len + 1);
   dStrncpy(ret, argv[1], len);
   ret[len] = 0;
   return ret;
}

ConsoleFunction(restWords, const char *, 2, 2, "restWords(text)")
{
   argc;
   const char *word = dStrchr(argv[1], ' ');
   if(word == NULL)
      return "";
   char *ret = Con::getReturnBuffer(dStrlen(word + 1) + 1);
   dStrcpy(ret, word + 1);
   return ret;
}

static bool isInSet(char c, const char *set)
{
   if (set)
      while (*set)
         if (c == *set++)
            return true;

   return false;
}

ConsoleFunction(NextToken,const char *,4,4,"nextToken(str,token,delim)")
{
   argc;

   char *str = (char *) argv[1];
   const char *token = argv[2];
   const char *delim = argv[3];

   if (str)
   {
      // skip over any characters that are a member of delim
      // no need for special '\0' check since it can never be in delim
      while (isInSet(*str, delim))
         str++;

      // skip over any characters that are NOT a member of delim
      const char *tmp = str;

      while (*str && !isInSet(*str, delim))
         str++;

      // terminate the token
      if (*str)
         *str++ = 0;

      // set local variable if inside a function
      if (gEvalState.stack.size() && 
         gEvalState.stack.last()->scopeName)
         Con::setLocalVariable(token,tmp);
      else
         Con::setVariable(token,tmp);

      // advance str past the 'delim space'
      while (isInSet(*str, delim))
         str++;
   }

   return str;
}

ConsoleFunctionGroupEnd( FieldManipulators )
//----------------------------------------------------------------

ConsoleFunctionGroupBegin( TaggedStrings, "Functions dealing with tagging/detagging strings.");

ConsoleFunction(detag, const char *, 2, 2, "detag(textTagString)")
{
   argc;
   if(argv[1][0] == StringTagPrefixByte)
   {
      const char *word = dStrchr(argv[1], ' ');
      if(word == NULL)
         return "";
      char *ret = Con::getReturnBuffer(dStrlen(word + 1) + 1);
      dStrcpy(ret, word + 1);
      return ret;
   }
   else
      return argv[1];
}

ConsoleFunction(getTag, const char *, 2, 2, "getTag(textTagString)")
{
   argc;
   if(argv[1][0] == StringTagPrefixByte)
   {
      const char * space = dStrchr(argv[1], ' ');

      U32 len;
      if(space)
         len = space - argv[1];
      else
         len = dStrlen(argv[1]) + 1;

      char * ret = Con::getReturnBuffer(len);
      dStrncpy(ret, argv[1] + 1, len - 1);
      ret[len - 1] = 0;

      return(ret);
   }
   else
      return(argv[1]);
}

ConsoleFunctionGroupEnd( TaggedStrings );

//----------------------------------------------------------------

ConsoleFunctionGroupBegin( Output, "Functions to output to the console." );

ConsoleFunction(echo, void, 2, 0, "echo(text [, ... ])")
{
   U32 len = 0;
   S32 i;
   for(i = 1; i < argc; i++)
      len += dStrlen(argv[i]);

   char *ret = Con::getReturnBuffer(len + 1);
   ret[0] = 0;
   for(i = 1; i < argc; i++)
      dStrcat(ret, argv[i]);

   Con::printf("%s", ret);
   ret[0] = 0;
}

ConsoleFunction(warn, void, 2, 0, "warn(text [, ... ])")
{
   U32 len = 0;
   S32 i;
   for(i = 1; i < argc; i++)
      len += dStrlen(argv[i]);

   char *ret = Con::getReturnBuffer(len + 1);
   ret[0] = 0;
   for(i = 1; i < argc; i++)
      dStrcat(ret, argv[i]);

   Con::warnf(ConsoleLogEntry::General, "%s", ret);
   ret[0] = 0;
}

ConsoleFunction(error, void, 2, 0, "error(text [, ... ])")
{
   U32 len = 0;
   S32 i;
   for(i = 1; i < argc; i++)
      len += dStrlen(argv[i]);

   char *ret = Con::getReturnBuffer(len + 1);
   ret[0] = 0;
   for(i = 1; i < argc; i++)
      dStrcat(ret, argv[i]);

   Con::errorf(ConsoleLogEntry::General, "%s", ret);
   ret[0] = 0;
}

ConsoleFunction(expandEscape, const char *, 2, 2, "expandEscape(text)")
{
   argc;
   char *ret = Con::getReturnBuffer(dStrlen(argv[1])*2 + 1);  // worst case situation
   expandEscape(ret, argv[1]);
   return ret;
}

ConsoleFunction(collapseEscape, const char *, 2, 2, "collapseEscape(text)")
{
   argc;
   char *ret = Con::getReturnBuffer(dStrlen(argv[1]) + 1);  // worst case situation
   dStrcpy( ret, argv[1] );
   collapseEscape( ret );
   return ret;
}

ConsoleFunction(setLogMode, void, 2, 2, "setLogMode(mode);")
{
   argc;
   Con::setLogMode(dAtoi(argv[1]));
}

ConsoleFunction(setEchoFileLoads, void, 2, 2, "setEchoFileLoads(bool);")
{
   argc;
   ResourceManager->setFileNameEcho(dAtob(argv[1]));
}

ConsoleFunctionGroupEnd( Output );

//----------------------------------------------------------------

ConsoleFunction(quit, void, 1, 1, "quit() End execution of Torque.")
{
   argc; argv;
   Platform::postQuitMessage(0);
}

ConsoleFunction(quitWithErrorMessage, void, 2, 2, "quitWithErrorMessage(msg)"
                " - Quit, showing the provided error message. This is equivalent"
                " to an AssertISV.")
{
   AssertISV(false, argv[1]);
}

//----------------------------------------------------------------

ConsoleFunction( gotoWebPage, void, 2, 2, "( address ) - Open a URL in the user's favorite web browser." )
{
   argc;
   char* protocolSep = dStrstr(argv[1],"://");

   if( protocolSep != NULL )
   {
      Platform::openWebBrowser(argv[1]);
      return;
   }

   // if we don't see a protocol seperator, then we know that some bullethead
   // sent us a bad url. We'll first check to see if a file inside the sandbox
   // with that name exists, then we'll just glom "http://" onto the front of 
   // the bogus url, and hope for the best.
   
   char fileName[2048]; 
   char urlBuf[2048];
   Con::expandScriptFilename(fileName, sizeof(fileName), argv[1]);
   
   if(Platform::isFile(fileName) || Platform::isDirectory(fileName))
   {
      dSprintf(urlBuf, sizeof(urlBuf), "file://%s/%s",Platform::getWorkingDirectory(),fileName);
      Platform::openWebBrowser(urlBuf);
      return;
   }
   
   dSprintf(urlBuf, sizeof(urlBuf), "http://%s",fileName);
   Platform::openWebBrowser(urlBuf);
   return;
}

//----------------------------------------------------------------

ConsoleFunctionGroupBegin(MetaScripting, "Functions that let you manipulate the scripting engine programmatically.");

ConsoleFunction(call, const char *, 2, 0, "call(funcName [,args ...])")
{
   return Con::execute(argc - 1, argv + 1);
}

static U32 execDepth = 0;
static U32 journalDepth = 1;

ConsoleFunction(compile, bool, 2, 2, "compile(fileName)")
{
#if 0
   argc;
   char nameBuffer[512];
   char* script = NULL;
   U32 scriptSize = 0;

   Stream *compiledStream = NULL;
   FileTime comModifyTime, scrModifyTime;

   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);

   // If the script file extention is '.ed.cs' then compile it to a different compiled extention
   const char *ext = dStrchr( scriptFilenameBuffer, '.' );

   if( ext != NULL && ( dStricmp( ext, ".ed.cs" ) == 0 ) || ( dStricmp( ext, ".ed.gui" ) == 0 ) )  
      dSprintf( nameBuffer, sizeof(nameBuffer), "%s.edso", scriptFilenameBuffer );
   else
      dSprintf( nameBuffer, sizeof(nameBuffer), "%s.dso", scriptFilenameBuffer );
   
   ResourceObject *rScr = ResourceManager->find(scriptFilenameBuffer);
   ResourceObject *rCom = ResourceManager->find(nameBuffer);

   if(rCom)
      rCom->getFileTimes(NULL, &comModifyTime);
   if(rScr)
      rScr->getFileTimes(NULL, &scrModifyTime);

   Stream *s = ResourceManager->openStream(scriptFilenameBuffer);
   if(s)
   {
      scriptSize = ResourceManager->getSize(scriptFilenameBuffer);
      script = new char [scriptSize+1];
      s->read(scriptSize, script);
      ResourceManager->closeStream(s);
      script[scriptSize] = 0;
   }

   if (!scriptSize || !script)
   {
      delete [] script;
      Con::errorf(ConsoleLogEntry::Script, "compile: invalid script file %s.", scriptFilenameBuffer);
      return false;
   }
   // compile this baddie.
   Con::printf("Compiling %s...", scriptFilenameBuffer);
   CodeBlock *code = new CodeBlock();
   code->compile(nameBuffer, scriptFilenameBuffer, script);
   delete code;
   code = NULL;

   delete[] script;
   return true;
#endif

   return false;
}

ConsoleFunction(exec, bool, 2, 4, "exec(fileName [, nocalls [,journalScript]])")
{
   bool journal = false;

   execDepth++;
   if(journalDepth >= execDepth)
      journalDepth = execDepth + 1;
   else
      journal = true;

   bool noCalls = false;
   bool ret = false;

   if(argc >= 3 && dAtoi(argv[2]))
      noCalls = true;

   if(argc >= 4 && dAtoi(argv[3]) && !journal)
   {
      journal = true;
      journalDepth = execDepth;
   }

   // Determine the filename we actually want...
   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);

   const char *ext = dStrrchr(scriptFilenameBuffer, '.');

   if(!ext)
   {
      // We need an extension!
      Con::errorf(ConsoleLogEntry::Script, "exec: invalid script file name %s.", scriptFilenameBuffer);
      execDepth--;
      return false;
   }

   // Check Editor Extensions
   bool isEditorScript = false;

   // If the script file extention is '.ed.cs' then compile it to a different compiled extention
   const char *edExt = dStrchr( scriptFilenameBuffer, '.' );
   if( edExt != NULL && ( dStricmp( edExt, ".ed.cs" ) == 0 ) || ( dStricmp( edExt, ".ed.gui" ) == 0 ) )  
      isEditorScript = true;


   StringTableEntry scriptFileName = StringTable->insert(scriptFilenameBuffer);

   // Is this a file we should compile?
   bool compiled = dStricmp(ext, ".mis") && !journal && false; // !Con::getBoolVariable("Scripts::ignoreDSOs");

   // If we're in a journalling mode, then we will read the script
   // from the journal file.
   if(journal && Game->isJournalReading())
   {
      char fileNameBuf[256];
      bool fileRead;
      U32 fileSize;

      Game->getJournalStream()->readString(fileNameBuf);
      Game->getJournalStream()->read(&fileRead);
      if(!fileRead)
      {
         Con::errorf(ConsoleLogEntry::Script, "Journal script read (failed) for %s", fileNameBuf);
         execDepth--;
         return false;
      }
      Game->journalRead(&fileSize);
      char *script = new char[fileSize + 1];
      Game->journalRead(fileSize, script);
      script[fileSize] = 0;
      Con::printf("Executing (journal-read) %s.", scriptFileName);
      CodeBlock *newCodeBlock = new CodeBlock();
      newCodeBlock->compileExec(scriptFileName, script, noCalls, 0);
      delete [] script;

      execDepth--;
      return true;
   }

   // Ok, we let's try to load and compile the script.
   ResourceObject *rScr = ResourceManager->find(scriptFileName);
   ResourceObject *rCom = NULL;

   char nameBuffer[512];
   char* script = NULL;
   U32 scriptSize = 0;
   U32 version;

   Stream *compiledStream = NULL;
   FileTime comModifyTime, scrModifyTime;

   // Check here for .edso
   bool edso = false;
   if( dStricmp( ext, ".edso" ) == 0 )
   {
      edso = true;
      rCom = rScr;
      rScr = NULL;

      rCom->getFileTimes( NULL, &comModifyTime );
      dStrcpy( nameBuffer, scriptFileName );
   }

   // If we're supposed to be compiling this file, check to see if there's a DSO
   if(compiled && !edso)
   {
      if( isEditorScript )
      {
         dStrcpyl(nameBuffer, sizeof(nameBuffer), scriptFileName, ".edso", NULL);
         rCom = ResourceManager->find(nameBuffer);
      }
      else
   {
      dStrcpyl(nameBuffer, sizeof(nameBuffer), scriptFileName, ".dso", NULL);
      rCom = ResourceManager->find(nameBuffer);
      }

      if(rCom)
         rCom->getFileTimes(NULL, &comModifyTime);
      if(rScr)
         rScr->getFileTimes(NULL, &scrModifyTime);
   }

   // Let's do a sanity check to complain about DSOs in the future.
   //
   // MM:	This doesn't seem to be working correctly for now so let's just not issue
   //		the warning until someone knows how to resolve it.
   //
   //if(compiled && rCom && rScr && Platform::compareFileTimes(comModifyTime, scrModifyTime) < 0)
   //{
      //Con::warnf("exec: Warning! Found a DSO from the future! (%s)", nameBuffer);
   //}

   // If we had a DSO, let's check to see if we should be reading from it.
   if(compiled && rCom && (!rScr || Platform::compareFileTimes(comModifyTime, scrModifyTime) >= 0))
   {
      compiledStream = ResourceManager->openStream(nameBuffer);
      if (compiledStream)
      {
      // Check the version!
      compiledStream->read(&version);
      if(version != Con::DSOVersion)
      {
         Con::warnf("exec: Found an old DSO (%s, ver %d < %d), ignoring.", nameBuffer, version, Con::DSOVersion);
         ResourceManager->closeStream(compiledStream);
         compiledStream = NULL;
      }
   }
   }

   // If we're journalling, let's write some info out.
   if(journal && Game->isJournalWriting())
      Game->getJournalStream()->writeString(scriptFileName);

   if(rScr && !compiledStream)
   {
      // If we have source but no compiled version, then we need to compile
      // (and journal as we do so, if that's required).

      Stream *s = ResourceManager->openStream(scriptFileName);
      if(journal && Game->isJournalWriting())
         Game->getJournalStream()->write(bool(s != NULL));

      if(s)
      {
         scriptSize = ResourceManager->getSize(scriptFileName);
         script = new char [scriptSize+1];
         s->read(scriptSize, script);

         if(journal && Game->isJournalWriting())
         {
            Game->journalWrite(scriptSize);
            Game->journalWrite(scriptSize, script);
         }
         ResourceManager->closeStream(s);
         script[scriptSize] = 0;
      }

      if (!scriptSize || !script)
      {
         delete [] script;
         Con::errorf(ConsoleLogEntry::Script, "exec: invalid script file %s.", scriptFileName);
         execDepth--;
         return false;
      }

      if(compiled)
      {
         // compile this baddie.
         Con::printf("Compiling %s...", scriptFileName);
         CodeBlock *code = new CodeBlock();
         code->compile(nameBuffer, scriptFileName, script);
         delete code;
         code = NULL;

         compiledStream = ResourceManager->openStream(nameBuffer);
         if(compiledStream)
         {
            compiledStream->read(&version);
         }
         else
         {
            // We have to exit out here, as otherwise we get double error reports.
            delete [] script;
            execDepth--;
            return false;
         }
      }
   }
   else
   {
      if(journal && Game->isJournalWriting())
         Game->getJournalStream()->write(bool(false));
   }

   if(compiledStream)
   {
      // Delete the script object first to limit memory used
      // during recursive execs.
      delete [] script;
      script = 0;

      // We're all compiled, so let's run it.
      Con::printf("Loading compiled script %s.", scriptFileName);
      CodeBlock *code = new CodeBlock;
      code->read(scriptFileName, *compiledStream);
      ResourceManager->closeStream(compiledStream);
      code->exec(0, scriptFileName, NULL, 0, NULL, noCalls, NULL, 0);
      ret = true;
   }
   else
      if(rScr)
      {
         // No compiled script,  let's just try executing it
         // directly... this is either a mission file, or maybe
         // we're on a readonly volume.
         Con::printf("Executing %s.", scriptFileName);
         CodeBlock *newCodeBlock = new CodeBlock();
         StringTableEntry name = StringTable->insert(scriptFileName);

         newCodeBlock->compileExec(name, script, noCalls, 0);
         ret = true;
      }
      else
      {
         // Don't have anything.
         Con::warnf(ConsoleLogEntry::Script, "Missing file: %s!", scriptFileName);
         ret = false;
      }

   delete [] script;
   execDepth--;
   return ret;
}

ConsoleFunction(eval, const char *, 2, 2, "eval(consoleString)")
{
   argc;
   return Con::evaluate(argv[1], false, NULL);
}

ConsoleFunction(getVariable, const char *, 2, 2, "(string varName)")
{
   return Con::getVariable(argv[1]);
}

ConsoleFunction(isFunction, bool, 2, 3, "([string className], string funcName)")
{
	if (argc == 2) {
		return Con::isFunction(argv[1]);
	} else return Con::isMethod(argv[1], argv[2]);
}

//----------------------------------------------------------------

ConsoleFunction(export, void, 2, 4, "export(searchString [, fileName [,append]])")
{
   const char *filename = NULL;
   bool append = (argc == 4) ? dAtob(argv[3]) : false;

   if (argc >= 3)
      if (Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[2]))
         filename = scriptFilenameBuffer;

   gEvalState.globalVars.exportVariables(argv[1], filename, append);
}

ConsoleFunction(deleteVariables, void, 2, 2, "deleteVariables(wildCard)")
{
   argc;
   gEvalState.globalVars.deleteVariables(argv[1]);
}

//----------------------------------------------------------------

ConsoleFunction(trace, void, 2, 2, "trace(bool)")
{
   argc;
   gEvalState.traceOn = dAtob(argv[1]);
   Con::printf("Console trace is %s", gEvalState.traceOn ? "on." : "off.");
}

//----------------------------------------------------------------

#if defined(TORQUE_DEBUG) || defined(INTERNAL_RELEASE)
ConsoleFunction(debug, void, 1, 1, "debug()")
{
   argv; argc;
   Platform::debugBreak();
}
#endif

ConsoleFunctionGroupEnd( MetaScripting );

//----------------------------------------------------------------

ConsoleFunctionGroupBegin( InputManagement, "Functions that let you deal with input from scripts" );

ConsoleFunction( deactivateDirectInput, void, 1, 1, "Deactivate input. (ie, ungrab the mouse so the user can do other things." )
{
   argc; argv;
   if ( Input::isActive() )
      Input::deactivate();
}

//--------------------------------------------------------------------------
ConsoleFunction( activateDirectInput, void, 1, 1, "Activate input. (ie, grab the mouse again so the user can play our game." )
{
   argc; argv;
   if ( !Input::isActive() )
      Input::activate();
}

ConsoleFunctionGroupEnd( InputManagement );

//----------------------------------------------------------------

ConsoleFunctionGroupBegin( FileSystem, "Functions allowing you to search for files, read them, write them, and access their properties.");

static ResourceObject *firstMatch = NULL;

ConsoleFunction(findFirstFile, const char *, 2, 2, "(string pattern) Returns the first file in the directory system matching the given pattern.")
{
   argc;
   const char *fn;
   firstMatch = NULL;
   if(Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]))
      firstMatch = ResourceManager->findMatch(scriptFilenameBuffer, &fn, NULL);
   if(firstMatch)
      return fn;
   else
      return "";
}

ConsoleFunction(findNextFile, const char *, 2, 2, "(string pattern) Returns the next file matching a search begun in findFirstFile.")
{
   argc;
   const char *fn;
   if(Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]))
      firstMatch = ResourceManager->findMatch(scriptFilenameBuffer, &fn, firstMatch);
   else
      firstMatch = NULL;
   if(firstMatch)
      return fn;
   else
      return "";
}

ConsoleFunction(getFileCount, S32, 2, 2, "(string pattern)returns the number of files in the directory tree that match the given pattern")
{
   argc;
   const char* fn;
   U32 count = 0;
   firstMatch = ResourceManager->findMatch(argv[1], &fn, NULL);
   if ( firstMatch )
   {
      count++;
      while ( 1 )
      {
         firstMatch = ResourceManager->findMatch(argv[1], &fn, firstMatch);
         if ( firstMatch )
            count++;
         else
            break;
      }
   }

   return( count );
}

ConsoleFunction(findFirstFileMultiExpr, const char *, 2, 2, "(string pattern) Returns the first file in the directory system matching the given pattern.")
{
   argc;
   const char *fn;
   firstMatch = NULL;
   if(Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]))
      firstMatch = ResourceManager->findMatchMultiExprs(scriptFilenameBuffer, &fn, NULL);
   if(firstMatch)
      return fn;
   else
      return "";
}

ConsoleFunction(findNextFileMultiExpr, const char *, 2, 2, "(string pattern) Returns the next file matching a search begun in findFirstFile.")
{
   argc;
   const char *fn;
   if(Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]))
      firstMatch = ResourceManager->findMatchMultiExprs(scriptFilenameBuffer, &fn, firstMatch);
   else
      firstMatch = NULL;
   if(firstMatch)
      return fn;
   else
      return "";
}

ConsoleFunction(getFileCountMultiExpr, S32, 2, 2, "(string pattern)returns the number of files in the directory tree that match the given pattern")
{
   argc;
   const char* fn;
   U32 count = 0;
   firstMatch = ResourceManager->findMatchMultiExprs(argv[1], &fn, NULL);
   if ( firstMatch )
   {
      count++;
      while ( 1 )
      {
         firstMatch = ResourceManager->findMatchMultiExprs(argv[1], &fn, firstMatch);
         if ( firstMatch )
            count++;
         else
            break;
      }
   }

   return( count );
}

ConsoleFunction(getFileCRC, S32, 2, 2, "getFileCRC(filename)")
{
   argc;
   U32 crcVal;
   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);

   if(!ResourceManager->getCrc(scriptFilenameBuffer, crcVal))
      return(-1);
   return(S32(crcVal));
}

ConsoleFunction(isFile, bool, 2, 2, "isFile(fileName)")
{
   argc;
   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);
   return bool(ResourceManager->find(scriptFilenameBuffer));
}

ConsoleFunction(isWriteableFileName, bool, 2, 2, "isWriteableFileName(fileName)")
{
   argc;
   // in a writeable directory?
   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);
   if(!ResourceManager->isValidWriteFileName(scriptFilenameBuffer))
      return(false);

   // exists?
   FileStream fs;
   if(!fs.open(scriptFilenameBuffer, FileStream::Read))
      return(true);

   // writeable? (ReadWrite will create file if it does not exist)
   fs.close();
   if(!fs.open(scriptFilenameBuffer, FileStream::ReadWrite))
      return(false);

   return(true);
}

//----------------------------------------------------------------

ConsoleFunction(fileExt, const char *, 2, 2, "fileExt(fileName)")
{
   argc;
   const char *ret = dStrrchr(argv[1], '.');
   if(ret)
      return ret;
   return "";
}

ConsoleFunction(fileBase, const char *, 2, 2, "fileBase(fileName)")
{
   argc;
   const char *path = dStrrchr(argv[1], '/');
   if(!path)
      path = argv[1];
   else
      path++;
   char *ret = Con::getReturnBuffer(dStrlen(path) + 1);
   dStrcpy(ret, path);
   char *ext = dStrrchr(ret, '.');
   if(ext)
      *ext = 0;
   return ret;
}

ConsoleFunction(fileName, const char *, 2, 2, "fileName(filePathName)")
{
   argc;
   const char *name = dStrrchr(argv[1], '/');
   if(!name)
      name = argv[1];
   else
      name++;
   char *ret = Con::getReturnBuffer(dStrlen(name));
   dStrcpy(ret, name);
   return ret;
}

ConsoleFunction(filePath, const char *, 2, 2, "filePath(fileName)")
{
   argc;
   const char *path = dStrrchr(argv[1], '/');
   if(!path)
      return "";
   U32 len = path - argv[1];
   char *ret = Con::getReturnBuffer(len + 1);
   dStrncpy(ret, argv[1], len);
   ret[len] = 0;
   return ret;
}

ConsoleFunction(pathCopy, bool, 3, 4, "pathCopy(fromFile, toFile [, nooverwrite = true])")
{
   bool nooverwrite = true;

   if( argc == 4 )
      nooverwrite = dAtob( argv[3] );

   static char fromFile[1024];
   static char toFile[1024];

   static char qualifiedFromFile[1024];
   static char qualifiedToFile[1024];

   Con::expandScriptFilename( fromFile, sizeof( fromFile ), argv[1] );
   Con::expandScriptFilename( toFile, sizeof( toFile ), argv[2] );

   dSprintf( qualifiedFromFile, 1024, "%s/%s", Platform::getWorkingDirectory(), fromFile );
   dSprintf( qualifiedToFile, 1024, "%s/%s", Platform::getWorkingDirectory(), toFile );

   return dPathCopy( qualifiedFromFile, qualifiedToFile, nooverwrite );
}

ConsoleFunction(getDirectoryList, const char*, 2, 3, "getDirectoryList(%path, %depth)")
{
   // Grab the full path.
   char path[512];
   dSprintf(path, 511, "%s/%s", Platform::getWorkingDirectory(), argv[1]);

   // Append a trailing backslash if it's not present already.
   if (path[dStrlen(path) - 1] != '/')
   {
      S32 pos = dStrlen(path);
      path[pos] = '/';
      path[pos + 1] = '\0';
   }

   // Grab the depth to search.
   S32 depth = 0;
   if (argc > 2)
      depth = dAtoi(argv[2]);

   // Dump the directories.
   Vector<StringTableEntry> directories;
   Platform::dumpDirectories(path, directories, depth, true);

   // Grab the required buffer length.
   S32 length = 0;
   S32 workingDirLength = dStrlen(Platform::getWorkingDirectory()) + 1;
   for (S32 i = 0; i < directories.size(); i++)
      length += dStrlen(directories[i]) + 1;

   // Get a return buffer.
   char* buffer = Con::getReturnBuffer(length);
   char* p = buffer;

   // Copy the directory names to the buffer.
   for (S32 i = 0; i < directories.size(); i++)
   {
      dStrcpy(p, directories[i]);
      p += dStrlen(directories[i]);
      // Space separated.
      p[0] = ' ';
      p++;
   }
   p--;
   p[0] = '\0';

   return buffer;
}

ConsoleFunction(fileSize, S32, 2, 2, "fileSize(fileName) returns filesize or -1 if no file")
{
   argc;
   Con::expandScriptFilename(scriptFilenameBuffer, sizeof(scriptFilenameBuffer), argv[1]);
   return Platform::getFileSize( scriptFilenameBuffer );
}

ConsoleFunction(getWorkingDirectory, const char *, 1, 1, "getWorkingDirectory()")
{
   return Platform::getWorkingDirectory();
}

ConsoleFunction(getExecutableName, const char *, 1, 1, "getExecutableName()")
{
   return Platform::getExecutableName();
}

ConsoleFunction(restartInstance, void, 1, 1, "restartInstance()")
{
   Game->setRestart( true );
   Platform::postQuitMessage( 0 );
}

ConsoleFunction( createPath, bool, 2,2, "createPath(\"file name or path name\");  creates the path or path to the file name")
{
   static char fileName[1024];
   static char sandboxFileName[1024];

   if (*argv[1] == '&') {
	   dStrcpy(sandboxFileName, argv[1] + 1);
   } else {
	   Con::expandScriptFilename(fileName, sizeof(fileName), argv[1]);
	   dSprintf(sandboxFileName, 1024, "%s/%s", Platform::getWorkingDirectory(), fileName);
   }
   
   return Platform::createPath(sandboxFileName);
}

ConsoleFunction(fileDelete, bool, 2, 2, "fileDelete('path')")
{
	static char fileName[1024];
	static char sandboxFileName[1024];

	Con::expandScriptFilename(fileName, sizeof(fileName), argv[1]);
	dSprintf(sandboxFileName, 1024, "%s/%s", Platform::getWorkingDirectory(), fileName);

	return dFileDelete(sandboxFileName);
}

ConsoleFunction(fileCopy, bool, 3, 3, "(oldPath, newPath)")
{
	return dPathCopy(argv[1], argv[2], false);
}

ConsoleFunction(fileRealDelete, bool, 2, 2, "fileRealDelete('path')")
{
	if (Platform::isDirectory(argv[1]))
	{
		Con::errorf("Not deleting \"%s\" because it's a path", argv[1]);
		return false;
	}

	return dFileDelete(argv[1]);
}

ConsoleFunctionGroupEnd( FileSystem );
