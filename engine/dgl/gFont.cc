//-----------------------------------------------------------------------------
// Torque Game Engine 
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformFont.h"
#include "platform/profiler.h"
#include "platform/platformMutex.h"
#include "console/console.h"
#include "core/stream.h"
#include "dgl/gBitmap.h"
#include "core/fileStream.h"
#include "core/findMatch.h"
#include "dgl/gTexManager.h"
#include "dgl/gFont.h"
#include "util/safeDelete.h"
#include "core/frameAllocator.h"
#include "core/unicode.h"
#include "zlib.h"
#include "ctype.h"  // Needed for isupper and tolower

S32 GFont::smSheetIdCount = 0;
const U32 GFont::csm_fileVersion = 3;

ConsoleFunction(populateFontCacheString, void, 4, 4, "(faceName, size, string) - "
                "Populate the font cache for the specified font with characters from the specified string.")
{
   Resource<GFont> f = GFont::create(argv[1], dAtoi(argv[2]), Con::getVariable("$GUI::fontCacheDirectory"));

   if(f.isNull())
   {
      Con::errorf("populateFontCacheString - could not load font '%s %d'!", argv[1], dAtoi(argv[2]));
      return;
   }

   if(!f->hasPlatformFont())
   {
      Con::errorf("populateFontCacheString - font '%s %d' has no platform font! Cannot generate more characters.", argv[1], dAtoi(argv[2]));
      return;
   }

   // This has the side effect of generating character info, including the bitmaps.
   f->getStrWidthPrecise(argv[3]);
}

ConsoleFunction(populateFontCacheRange, void, 5, 5, "(faceName, size, rangeStart, rangeEnd) - "
                "Populate the font cache for the specified font with Unicode code points in the specified range. "
                "Note we only support BMP-0, so code points range from 0 to 65535.")
{
   Resource<GFont> f = GFont::create(argv[1], dAtoi(argv[2]), Con::getVariable("$GUI::fontCacheDirectory"));

   if(f.isNull())
   {
      Con::errorf("populateFontCacheRange - could not load font '%s %d'!", argv[1], dAtoi(argv[2]));
      return;
   }

   U32 rangeStart = dAtoi(argv[3]);
   U32 rangeEnd   = dAtoi(argv[4]);

   if(rangeStart > rangeEnd)
   {
      Con::errorf("populateFontCacheRange - range start is after end!");
      return;
   }

   if(!f->hasPlatformFont())
   {
      Con::errorf("populateFontCacheRange - font '%s %d' has no platform font! Cannot generate more characters.", argv[1], dAtoi(argv[2]));
      return;
   }

   // This has the side effect of generating character info, including the bitmaps.
   for(U32 i=rangeStart; i<rangeEnd; i++)
   {
      if(f->isValidChar(i))
         f->getCharWidth(i);
      else
         Con::warnf("populateFontCacheRange - skipping invalid char 0x%x",  i);
   }

   // All done!
}

ConsoleFunction(dumpFontCacheStatus, void, 1, 1, "() - Return a full description "
                "of all cached fonts, along with info on the codepoints each contains.")
{
   FindMatch match("*.uft", 4096);
   ResourceManager->findMatches(&match);

   Con::printf("--------------------------------------------------------------------------");
   Con::printf("   Font Cache Usage Report (%d fonts found)", match.numMatches());

   for (U32 i = 0; i < match.numMatches(); i++)
   {
      char *curMatch = match.matchList[i];
      Resource<GFont> font = ResourceManager->load(curMatch);

      // Deal with inexplicably missing or failed to load fonts.
      if (font.isNull())
      {
         Con::errorf(" o Couldn't load font : %s", curMatch);
         continue;
      }

      // Ok, dump info!
      font->dumpInfo();
   }
}

ConsoleFunction(writeFontCache, void, 1, 1, "() - force all cached fonts to"
                "serialize themselves to the cache.")
{
   FindMatch match("*.uft", 4096);
   ResourceManager->findMatches(&match);

   Con::printf("--------------------------------------------------------------------------");
   Con::printf("   Writing font cache to disk (%d fonts found)", match.numMatches());

   for (U32 i = 0; i < match.numMatches(); i++)
   {
      char *curMatch = match.matchList[i];
      Resource<GFont> font = ResourceManager->load(curMatch);

      // Deal with inexplicably missing or failed to load fonts.
      if (font.isNull())
      {
         Con::errorf(" o Couldn't find font : %s", curMatch);
         continue;
      }

      // Ok, dump info!
      FileStream stream;
      if(ResourceManager->openFileForWrite(stream, curMatch)) 
      {
         Con::printf("      o Writing '%s' to disk...", curMatch);
         font->write(stream);
         stream.close();
      }
      else
      {
         Con::errorf("      o Could not open '%s' for write!", curMatch);
      }
   }
}

ConsoleFunction(populateAllFontCacheString, void, 2, 2, "(string) - "
                "Populate the font cache for all fonts with characters from the specified string.")
{
   FindMatch match("*.uft", 4096);
   ResourceManager->findMatches(&match);

   Con::printf("Populating font cache with string '%s' (%d fonts found)", argv[1], match.numMatches());

   for (U32 i = 0; i < match.numMatches(); i++)
   {
      char *curMatch = match.matchList[i];
      Resource<GFont> font = ResourceManager->load(curMatch);

      // Deal with inexplicably missing or failed to load fonts.
      if (font.isNull())
      {
         Con::errorf(" o Couldn't load font : %s", curMatch);
         continue;
      }

      if(!font->hasPlatformFont())
      {
         Con::errorf("populateAllFontCacheString - font '%s' has no platform font! Cannot generate more characters.", curMatch);
         continue;
      }

      // This has the side effect of generating character info, including the bitmaps.
      font->getStrWidthPrecise(argv[1]);
   }
}

ConsoleFunction(populateAllFontCacheRange, void, 3, 3, "(rangeStart, rangeEnd) - "
                "Populate the font cache for all fonts with Unicode code points in the specified range. "
                "Note we only support BMP-0, so code points range from 0 to 65535.")
{
   U32 rangeStart = dAtoi(argv[1]);
   U32 rangeEnd   = dAtoi(argv[2]);

   if(rangeStart > rangeEnd)
   {
      Con::errorf("populateAllFontCacheRange - range start is after end!");
      return;
   }

   FindMatch match("*.uft", 4096);
   ResourceManager->findMatches(&match);

   Con::printf("Populating font cache with range 0x%x to 0x%x (%d fonts found)", rangeStart, rangeEnd, match.numMatches());

   for (U32 i = 0; i < match.numMatches(); i++)
   {
      char *curMatch = match.matchList[i];
      Resource<GFont> font = ResourceManager->load(curMatch);

      // Deal with inexplicably missing or failed to load fonts.
      if (font.isNull())
      {
         Con::errorf(" o Couldn't load font : %s", curMatch);
         continue;
      }

      if(!font->hasPlatformFont())
      {
         Con::errorf("populateAllFontCacheRange - font '%s' has no platform font! Cannot generate more characters.", curMatch);
         continue;
      }

      // This has the side effect of generating character info, including the bitmaps.
      Con::printf("   o Populating font '%s'", curMatch);
      for(U32 i=rangeStart; i<rangeEnd; i++)
      {
         if(font->isValidChar(i))
            font->getCharWidth(i);
         else
            Con::warnf("populateAllFontCacheRange - skipping invalid char 0x%x",  i);
      }
   }
   // All done!
}

ConsoleFunction(exportCachedFont, void, 6, 6, "(fontName, size, fileName, padding, kerning) - "
                "Export specified font to the specified filename as a PNG. The "
                "image can then be processed in Photoshop or another tool and "
                "reimported using importCachedFont. Characters in the font are"
                "exported as one long strip.")
{
   // Read in some params.
   const char *fontName = argv[1];
   S32 fontSize   = dAtoi(argv[2]);
   const char *fileName = argv[3];
   S32 padding    = dAtoi(argv[4]);
   S32 kerning    = dAtoi(argv[5]);

   // Tell the font to export itself.
   Resource<GFont> f = GFont::create(argv[1], dAtoi(argv[2]), Con::getVariable("$GUI::fontCacheDirectory"));

   if(f.isNull())
   {
      Con::errorf("populateFontCacheString - could not load font '%s %d'!", argv[1], dAtoi(argv[2]));
      return;
   }

   f->exportStrip(fileName, padding, kerning);
}

ConsoleFunction(importCachedFont, void, 6, 6, "(fontName, size, fileName, padding, kerning) - "
                "Import an image strip from exportCachedFont. Call with the "
                "same parameters you called exportCachedFont.")
{
   // Read in some params.
   const char *fontName = argv[1];
   S32 fontSize   = dAtoi(argv[2]);
   const char *fileName = argv[3];
   S32 padding    = dAtoi(argv[4]);
   S32 kerning    = dAtoi(argv[5]);

   // Tell the font to import itself.
   Resource<GFont> f = GFont::create(argv[1], dAtoi(argv[2]), Con::getVariable("$GUI::fontCacheDirectory"));

   if(f.isNull())
   {
      Con::errorf("populateFontCacheString - could not load font '%s %d'!", argv[1], dAtoi(argv[2]));
      return;
   }

   f->importStrip(fileName, padding, kerning);
}

ConsoleFunction(duplicateCachedFont, void, 4, 4, "(oldFontName, oldFontSize, newFontName) -"
                "Copy the specified old font to a new name. The new copy will not have a "
                "platform font backing it, and so will never have characters added to it. "
                "But this is useful for making copies of fonts to add postprocessing effects "
                "to via exportCachedFont.")
{
   char newFontFile[256];
   GFont::getFontCacheFilename(argv[3], dAtoi(argv[2]), 256, newFontFile);

   // Load the original font.
   Resource<GFont> font = GFont::create(argv[1], dAtoi(argv[2]), Con::getVariable("$GUI::fontCacheDirectory"));

   // Deal with inexplicably missing or failed to load fonts.
   if (font.isNull())
   {
      Con::errorf(" o Couldn't find font : %s", newFontFile);
      return;
   }

   // Ok, dump info!
   FileStream stream;
   if(ResourceManager->openFileForWrite(stream, newFontFile)) 
   {
      Con::printf("      o Writing duplicate font '%s' to disk...", newFontFile);
      font->write(stream);
      stream.close();
   }
   else
   {
      Con::errorf("      o Could not open '%s' for write!", newFontFile);
   }
}

static PlatformFont* createSafePlatformFont(const char *name, U32 size, U32 charset = TGE_ANSI_CHARSET)
{
   PlatformFont *platFont = createPlatformFont(name, size, charset);
   
   if (platFont == NULL)
   {
      Con::errorf("Loading platform font failed, trying font fallbacks...");
      // Couldn't load the requested font.  This probably will be common
      // since many unix boxes don't have arial or lucida console installed.
      // Attempt to map the font name into a font we're pretty sure exist
      // Lucida Console is a common code & console font on windows, and
      // Monaco is the recommended code & console font on mac.

      // this is the name of the final fallback font.
      char* fallback = "Helvetica";

      if(dStricmp(name, fallback) == 0)
      {
         Con::errorf("Font fallback utterly failed.");
         return NULL;
      }
      else if (dStricmp(name, "arial") == 0)
         fallback = "Helvetica";
      else if (dStricmp(name, "lucida console") == 0)
         fallback = "Monaco";
      else if (dStricmp(name, "monaco") == 0)
         fallback = "Courier";

      platFont = createSafePlatformFont(fallback, size, charset);
   }
   
   return platFont;
}

ResourceInstance* constructFont(Stream& stream)
{
   GFont *ret = new GFont;

   if(!ret->read(stream))
   {
      SAFE_DELETE(ret);
   }

   if(ret)
   {
      ret->mPlatformFont = createSafePlatformFont(ret->mFaceName, ret->mSize, ret->mCharSet);
   }
   
   return ret;
}

void GFont::getFontCacheFilename(const char *faceName, U32 size, U32 buffLen, char *outBuff)
{
   dSprintf(outBuff, buffLen, "%s/%s %d (%s).uft", Con::getVariable("$GUI::fontCacheDirectory"), faceName, size, getCharSetName(0));
}

Resource<GFont> GFont::create(const char *faceName, U32 size, const char *cacheDirectory, U32 charset /* = TGE_ANSI_CHARSET */)
{
   char buf[256];
   dSprintf(buf, sizeof(buf), "%s/%s %d (%s).uft", cacheDirectory, faceName, size, getCharSetName(charset));
   
   Resource<GFont> ret = ResourceManager->load(buf);
   if(bool(ret))
   {
      ret->mGFTFile = StringTable->insert(buf);
      return ret;
   }

   PlatformFont *platFont = createSafePlatformFont(faceName, size, charset);
   
   
   GFont *resFont = new GFont;
   resFont->mPlatformFont = platFont;
   resFont->addSheet();
   resFont->mGFTFile = StringTable->insert(buf);
   resFont->mFaceName = StringTable->insert(faceName);
   resFont->mSize = size;
   resFont->mCharSet = charset;

   resFont->mHeight   = platFont->getFontHeight();
   resFont->mBaseline = platFont->getFontBaseLine();
   resFont->mAscent   = platFont->getFontBaseLine();
   resFont->mDescent  = platFont->getFontHeight() - platFont->getFontBaseLine();

   ResourceManager->add(buf, resFont, false);
   return ResourceManager->load(buf);
}

//-------------------------------------------------------------------------

GFont::GFont()
{
   VECTOR_SET_ASSOCIATION(mCharInfoList);
   VECTOR_SET_ASSOCIATION(mTextureSheets);

   for (U32 i = 0; i < (sizeof(mRemapTable) / sizeof(S32)); i++)
      mRemapTable[i] = -1;

   mCurX = mCurY = mCurSheet = -1;

   mPlatformFont = NULL;
   mGFTFile = NULL;
   mFaceName = NULL;
   mSize = 0;
   mCharSet = 0;
   mNeedSave = false;
   
   mMutex = Mutex::createMutex();
}

GFont::~GFont()
{
   if(mNeedSave)
   {
      FileStream stream;
      if(ResourceManager->openFileForWrite(stream, mGFTFile)) 
      {
         write(stream);
         stream.close();
      }
   }
   
   S32 i;

   for(i = 0;i < mCharInfoList.size();i++)
   {
       SAFE_DELETE_ARRAY(mCharInfoList[i].bitmapData);
   }

   SAFE_DELETE(mPlatformFont);
   
   Mutex::destroyMutex(mMutex);
}

void GFont::dumpInfo()
{
   // Number and extent of mapped characters?
   U32 mapCount = 0, mapBegin=0xFFFF, mapEnd=0;
   for(U32 i=0; i<0x10000; i++)
   {
      if(mRemapTable[i] != -1)
      {
         mapCount++;
         if(i<mapBegin) mapBegin = i;
         if(i>mapEnd)   mapEnd   = i;
      }
   }


   // Let's write out all the info we can on this font.
   Con::printf("   '%s' %dpt", mFaceName, mSize);
   Con::printf("      - %d texture sheets, %d mapped characters.", mTextureSheets.size(), mapCount);

   if(mapCount)
      Con::printf("      - Codepoints range from 0x%x to 0x%x.", mapBegin, mapEnd);
   else
      Con::printf("      - No mapped codepoints.", mapBegin, mapEnd);
   Con::printf("      - Platform font is %s.", (mPlatformFont ? "present" : "not present") );
}

//////////////////////////////////////////////////////////////////////////

bool GFont::loadCharInfo(const UTF16 ch)
{
    if(mRemapTable[ch] != -1)
        return true;    // Not really an error

    if(mPlatformFont && mPlatformFont->isValidChar(ch))
    {
        Mutex::lockMutex(mMutex); // the CharInfo returned by mPlatformFont is static data, must protect from changes.
        PlatformFont::CharInfo &ci = mPlatformFont->getCharInfo(ch);
        if(ci.bitmapData)
            addBitmap(ci);

        mCharInfoList.push_back(ci);
        mRemapTable[ch] = mCharInfoList.size() - 1;
        
        mNeedSave = true;

        Mutex::unlockMutex(mMutex);
        return true;
    }

    return false;
}

void GFont::addBitmap(PlatformFont::CharInfo &charInfo)
{
   U32 nextCurX = U32(mCurX + charInfo.width ); /*7) & ~0x3;*/
   U32 nextCurY = U32(mCurY + mPlatformFont->getFontHeight()); // + 7) & ~0x3;

   // These are here for postmortem debugging.
   bool routeA = false, routeB = false;

   if(mCurSheet == -1 || nextCurY >= TextureSheetSize)
   {
      routeA = true;
      addSheet();

      // Recalc our nexts.
      nextCurX = U32(mCurX + charInfo.width); // + 7) & ~0x3;
      nextCurY = U32(mCurY + mPlatformFont->getFontHeight()); // + 7) & ~0x3;
   }

   if( nextCurX >= TextureSheetSize)
   {
      routeB = true;
      mCurX = 0;
      mCurY = nextCurY;

      // Recalc our nexts.
      nextCurX = U32(mCurX + charInfo.width); // + 7) & ~0x3;
      nextCurY = U32(mCurY + mPlatformFont->getFontHeight()); // + 7) & ~0x3;
   }

   // Check the Y once more - sometimes we advance to a new row and run off
   // the end.
   if(nextCurY >= TextureSheetSize)
   {
      routeA = true;
      addSheet();

      // Recalc our nexts.
      nextCurX = U32(mCurX + charInfo.width); // + 7) & ~0x3;
      nextCurY = U32(mCurY + mPlatformFont->getFontHeight()); // + 7) & ~0x3;
   }

    charInfo.bitmapIndex = mCurSheet;
    charInfo.xOffset = mCurX;
    charInfo.yOffset = mCurY;

   mCurX = nextCurX;

   S32 x, y;
   GBitmap *bmp = mTextureSheets[mCurSheet].getBitmap();

   AssertFatal(bmp->getFormat() == GBitmap::Alpha, "GFont::addBitmap - cannot added characters to non-greyscale textures!");

   for(y = 0;y < charInfo.height;y++)
      for(x = 0;x < charInfo.width;x++)
         *bmp->getAddress(x + charInfo.xOffset, y + charInfo.yOffset) = charInfo.bitmapData[y * charInfo.width + x];

   mTextureSheets[mCurSheet].refresh();
}

void GFont::addSheet()
{
    char buf[30];
    dSprintf(buf, sizeof(buf), "newfont_%d", smSheetIdCount++);

    GBitmap *bitmap = new GBitmap(TextureSheetSize, TextureSheetSize, false, GBitmap::Alpha);

    // Set everything to transparent.
    U8 *bits = bitmap->getWritableBits();
    dMemset(bits, 0, sizeof(U8) *TextureSheetSize*TextureSheetSize);

    TextureHandle handle = TextureHandle(buf, bitmap);
    handle.setFilterNearest();

    mTextureSheets.increment();
    constructInPlace(&mTextureSheets.last());
    mTextureSheets.last() = handle;

    mCurX = 0;
    mCurY = 0;
    mCurSheet = mTextureSheets.size() - 1;
}

//////////////////////////////////////////////////////////////////////////

const PlatformFont::CharInfo &GFont::getCharInfo(const UTF16 in_charIndex) const
{
    PROFILE_START(NewFontGetCharInfo);

    AssertFatal(in_charIndex, "GFont::getCharInfo - can't get info for char 0!");

    if(mRemapTable[in_charIndex] == -1)
    {
		// getCharInfo() is const to the outside world, so we cast away the const here
        const_cast<GFont *>(this)->loadCharInfo(in_charIndex);
    }

    AssertFatal(mRemapTable[in_charIndex] != -1, "No remap info for this character");

    PROFILE_END();
    
   if(mRemapTable[in_charIndex] == -1)
      return getDefaultCharInfo();
   else
      return mCharInfoList[mRemapTable[in_charIndex]];
}

const PlatformFont::CharInfo &GFont::getDefaultCharInfo()
{
   static PlatformFont::CharInfo c;
   // c is initialized by the CharInfo default constructor.
   return c;
}

//////////////////////////////////////////////////////////////////////////

U32 GFont::getStrWidth(const UTF8* in_pString) const
{
   AssertFatal(in_pString != NULL, "GFont::getStrWidth: String is NULL, height is undefined");
   // If we ain't running debug...
   if (in_pString == NULL || *in_pString == NULL)
      return 0;

   return getStrNWidth(in_pString, dStrlen(in_pString));
}

U32 GFont::getStrWidthPrecise(const UTF8* in_pString) const
{
   AssertFatal(in_pString != NULL, "GFont::getStrWidth: String is NULL, height is undefined");
   // If we ain't running debug...
   if (in_pString == NULL)
      return 0;

   return getStrNWidthPrecise(in_pString, dStrlen(in_pString));
}

//////////////////////////////////////////////////////////////////////////
U32 GFont::getStrNWidth(const UTF8 *str, U32 n) const
{
   // UTF8 conversion is expensive. Avoid converting in a tight loop.
   FrameTemp<UTF16> str16(n + 1);
   convertUTF8toUTF16(str, str16, n+1);
   return getStrNWidth(str16, dStrlen(str16));
}

U32 GFont::getStrNWidth(const UTF16 *str, U32 n) const
{
   AssertFatal(str != NULL, "GFont::getStrNWidth: String is NULL");

   if (str == NULL || str[0] == NULL || n == 0)   
      return 0;
      
   U32 totWidth = 0;
   UTF16 curChar;
   U32 charCount;
   
   for(charCount = 0; charCount < n; charCount++)
   {
      curChar = str[charCount];
      if(curChar == NULL)
         break;

      if(isValidChar(curChar))
      {
         const PlatformFont::CharInfo& rChar = getCharInfo(curChar);
         totWidth += rChar.xIncrement;
      }
      else if (curChar == dT('\t'))
      {
         const PlatformFont::CharInfo& rChar = getCharInfo(dT(' '));
         totWidth += rChar.xIncrement * TabWidthInSpaces;
      }
   }

   return(totWidth);
}

U32 GFont::getStrNWidthPrecise(const UTF8 *str, U32 n) const
{
   FrameTemp<UTF16> str16(n + 1);
   convertUTF8toUTF16(str, str16, n);
   return getStrNWidthPrecise(str16, n);
}

U32 GFont::getStrNWidthPrecise(const UTF16 *str, U32 n) const
{
   AssertFatal(str != NULL, "GFont::getStrNWidth: String is NULL");

   if (str == NULL || str[0] == NULL || n == 0)   
      return(0);
      
   U32 totWidth = 0;
   UTF16 curChar;
   U32 charCount = 0;
   
   for(charCount = 0; charCount < n; charCount++)
   {
      curChar = str[charCount];
      if(curChar == NULL)
         break;
         
      if(isValidChar(curChar))
      {
         const PlatformFont::CharInfo& rChar = getCharInfo(curChar);
         totWidth += rChar.xIncrement;
      }
      else if (curChar == dT('\t'))
      {
         const PlatformFont::CharInfo& rChar = getCharInfo(dT(' '));
         totWidth += rChar.xIncrement * TabWidthInSpaces;
      }
   }

   UTF16 endChar = str[getMin(charCount,n-1)];

   if (isValidChar(endChar))
   {
      const PlatformFont::CharInfo& rChar = getCharInfo(endChar);
      if (rChar.width > rChar.xIncrement)
         totWidth += (rChar.width - rChar.xIncrement);
   }

   return(totWidth);
}

U32 GFont::getBreakPos(const UTF16 *str16, U32 slen, U32 width, bool breakOnWhitespace) const
{
   // Some early out cases.
   if(slen==0)
      return 0;

   U32 ret = 0;
   U32 lastws = 0;
   UTF16 c;
   U32 charCount = 0;

   for( charCount=0; charCount < slen; charCount++)
   {
      c = str16[charCount];
      if(c == NULL)
         break;
         
      if(c == dT('\t'))
         c = dT(' ');
      if(!isValidChar(c))
      {
         ret++;
         continue;
      }
      if(c == dT(' '))
         lastws = ret+1;
      const PlatformFont::CharInfo& rChar = getCharInfo(c);
      if(rChar.width > width || rChar.xIncrement > width)
      {
         if(lastws && breakOnWhitespace)
            return lastws;
         return ret;
      }
      width -= rChar.xIncrement;
      
      ret++;
   }
   return ret;
}

void GFont::wrapString(const UTF8 *txt, U32 lineWidth, Vector<U32> &startLineOffset, Vector<U32> &lineLen)
{
   Con::errorf("GFont::wrapString(): Not yet converted to be UTF-8 safe");

   startLineOffset.clear();
   lineLen.clear();

   if (!txt || !txt[0] || lineWidth < getCharWidth('W')) //make sure the line width is greater then a single character
      return;

   U32 len = dStrlen(txt);

   U32 startLine; 

   for (U32 i = 0; i < len;)
   {
      startLine = i;
      startLineOffset.push_back(startLine);

      // loop until the string is too large
      bool needsNewLine = false;
      U32 lineStrWidth = 0;
      for (; i < len; i++)
      {
         if(isValidChar(txt[i]))
         {
            lineStrWidth += getCharInfo(txt[i]).xIncrement;
            if ( txt[i] == '\n' || lineStrWidth > lineWidth )
            {
               needsNewLine = true;
               break;      
            }
         }
      }

      if (!needsNewLine)
      {
         // we are done!
         lineLen.push_back(i - startLine);
         return;
      }

      // now determine where to put the newline
      // else we need to backtrack until we find a either space character 
      // or \\ character to break up the line. 
      S32 j;
      for (j = i - 1; j >= startLine; j--)
      {
         if (dIsspace(txt[j]))
            break;
      }

      if (j < startLine)
      {
         // the line consists of a single word!              
         // So, just break up the word
         j = i - 1;
      }
      lineLen.push_back(j - startLine);
      i = j;

      // now we need to increment through any space characters at the
      // beginning of the next line
      for (i++; i < len; i++)
      {
         if (!dIsspace(txt[i]) || txt[i] == '\n')
            break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////

bool GFont::read(Stream& io_rStream)
{
    // Handle versioning
    U32 version;
    io_rStream.read(&version);
    if(version != csm_fileVersion)
        return false;

    char buf[256];
    io_rStream.readString(buf);
    mFaceName = StringTable->insert(buf);

    io_rStream.read(&mSize);
    io_rStream.read(&mCharSet);

    io_rStream.read(&mHeight);
    io_rStream.read(&mBaseline);
    io_rStream.read(&mAscent);
    io_rStream.read(&mDescent);

    U32 size = 0;
    io_rStream.read(&size);
    mCharInfoList.setSize(size);
    U32 i;
    for(i = 0; i < size; i++)
    {
        PlatformFont::CharInfo *ci = &mCharInfoList[i];
        io_rStream.read(&ci->bitmapIndex);
        io_rStream.read(&ci->xOffset);
        io_rStream.read(&ci->yOffset);
        io_rStream.read(&ci->width);
        io_rStream.read(&ci->height);
        io_rStream.read(&ci->xOrigin);
        io_rStream.read(&ci->yOrigin);
        io_rStream.read(&ci->xIncrement);
        ci->bitmapData = NULL;
   }

   U32 numSheets = 0;
   io_rStream.read(&numSheets);
   
   for(i = 0; i < numSheets; i++)
   {
       GBitmap *bmp = new GBitmap;
       if(!bmp->readPNG(io_rStream))
       {
           delete bmp;
           return false;
       }

       char buf[30];
       dSprintf(buf, sizeof(buf), "font_%d", smSheetIdCount++);

       mTextureSheets.increment();
       constructInPlace(&mTextureSheets.last());
       mTextureSheets.last() = TextureHandle(buf, bmp);
       mTextureSheets.last().setFilterNearest();
   }
   
   // Read last position info
   io_rStream.read(&mCurX);
   io_rStream.read(&mCurY);
   io_rStream.read(&mCurSheet);

   // Read the remap table.
   U32 minGlyph, maxGlyph;
   io_rStream.read(&minGlyph);
   io_rStream.read(&maxGlyph);

   if(maxGlyph >= minGlyph)
   {
      // Length of buffer..
      U32 buffLen;
      io_rStream.read(&buffLen);

      // Read the buffer.
      FrameTemp<S32> inBuff(buffLen);
      io_rStream.read(buffLen, inBuff);

      // Decompress.
      uLongf destLen = (maxGlyph-minGlyph+1)*sizeof(S32);
      uncompress((Bytef*)&mRemapTable[minGlyph], &destLen, (Bytef*)(S32*)inBuff, buffLen);

      AssertISV(destLen == (maxGlyph-minGlyph+1)*sizeof(S32), "GFont::read - invalid remap table data!");

      // Make sure we've got the right endianness.
      for(i = minGlyph; i <= maxGlyph; i++)
         mRemapTable[i] = convertBEndianToHost(mRemapTable[i]);
   }
   
   return (io_rStream.getStatus() == Stream::Ok);
}

bool GFont::write(Stream& stream)
{
    // Handle versioning
    stream.write(csm_fileVersion);

    // Write font info
    stream.writeString(mFaceName);
    stream.write(mSize);
    stream.write(mCharSet);
   
    stream.write(mHeight);
    stream.write(mBaseline);
    stream.write(mAscent);
    stream.write(mDescent);

    // Write char info list
    stream.write(U32(mCharInfoList.size()));
    U32 i;
    for(i = 0; i < mCharInfoList.size(); i++)
    {
        const PlatformFont::CharInfo *ci = &mCharInfoList[i];
        stream.write(ci->bitmapIndex);
        stream.write(ci->xOffset);
        stream.write(ci->yOffset);
        stream.write(ci->width);
        stream.write(ci->height);
        stream.write(ci->xOrigin);
        stream.write(ci->yOrigin);
        stream.write(ci->xIncrement);
   }

   stream.write(mTextureSheets.size());
   for(i = 0; i < mTextureSheets.size(); i++)
       mTextureSheets[i].getBitmap()->writePNG(stream);

   stream.write(mCurX);
   stream.write(mCurY);
   stream.write(mCurSheet);

   // Get the min/max we have values for, and only write that range out.
   S32 minGlyph = S32_MAX, maxGlyph = 0;

   for(i = 0; i < 65536; i++)
   {
      if(mRemapTable[i] != -1)
      {
         if(i>maxGlyph) maxGlyph = i;
         if(i<minGlyph) minGlyph = i;
      }
   }

   stream.write(minGlyph);
   stream.write(maxGlyph);

   // Skip it if we don't have any glyphs to do...
   if(maxGlyph >= minGlyph)
   {
      // Put everything big endian, to be consistent. Do this inplace.
      for(i = minGlyph; i <= maxGlyph; i++)
         mRemapTable[i] = convertHostToBEndian(mRemapTable[i]);

      {
         // Compress.
         const U32 buffSize = 128 * 1024;
         FrameTemp<S32> outBuff(buffSize);
         uLongf destLen = buffSize * sizeof(S32);
         compress2((Bytef*)(S32*)outBuff, &destLen, (Bytef*)(S32*)&mRemapTable[minGlyph], (maxGlyph-minGlyph+1)*sizeof(S32), 9);

         // Write out.
         stream.write((U32)destLen);
         stream.write(destLen, outBuff);
      }

      // Put us back to normal.
      for(i = minGlyph; i <= maxGlyph; i++)
         mRemapTable[i] = convertBEndianToHost(mRemapTable[i]);
   }
   
   return (stream.getStatus() == Stream::Ok);
}

void GFont::exportStrip(const char *fileName, U32 padding, U32 kerning)
{
   // Figure dimensions of our strip by iterating over all the char infos.
   U32 totalHeight = 0;
   U32 totalWidth = 0;

   S32 heightMin=0, heightMax=0;

   for(S32 i=0; i<mCharInfoList.size(); i++)
   {
      totalWidth += mCharInfoList[i].width + kerning + 2*padding;
      heightMin = getMin((S32)heightMin, (S32)getBaseline() - (S32)mCharInfoList[i].yOrigin);
      heightMax = getMax((S32)heightMax, (S32)getBaseline() - (S32)mCharInfoList[i].yOrigin + (S32)mCharInfoList[i].height);
   }

   totalHeight = heightMax - heightMin + 2*padding;

   // Make the bitmap.
   GBitmap gb(totalWidth, totalHeight, false, mTextureSheets[0].getBitmap()->getFormat());

   dMemset(gb.getWritableBits(), 0, sizeof(U8) * totalHeight * totalWidth );

   // Ok, copy some rects, taking into account padding, kerning, offset.
   U32 curWidth = kerning + padding;

   for(S32 i=0; i<mCharInfoList.size(); i++)
   {
      // Skip invalid stuff.
      if(mCharInfoList[i].bitmapIndex == -1 || mCharInfoList[i].height == 0 || mCharInfoList[i].width == 0)
         continue;

      // Copy the rect.
      U32 bitmap = mCharInfoList[i].bitmapIndex;

      RectI ri(mCharInfoList[i].xOffset, mCharInfoList[i].yOffset, mCharInfoList[i].width, mCharInfoList[i].height );
      Point2I outRi(curWidth, padding + getBaseline() - mCharInfoList[i].yOrigin);
      gb.copyRect(mTextureSheets[bitmap].getBitmap(), ri, outRi); 

      // Advance.
      curWidth +=  mCharInfoList[i].width + kerning + 2*padding;
   }

   // Write the image!
   FileStream fs;
   
   if(!ResourceManager->openFileForWrite(fs, fileName))
   {
      Con::errorf("GFont::exportStrip - failed to open '%s' for writing.", fileName);
      return;
   }
 
   // Done!
   gb.writePNG(fs, false);
}

/// Used for repacking in GFont::importStrip.
struct GlyphMap
{
   U32 charId;
   GBitmap *bitmap;
};

static S32 QSORT_CALLBACK GlyphMapCompare(const void *a, const void *b)
{
   S32 ha = ((GlyphMap *) a)->bitmap->height;
   S32 hb = ((GlyphMap *) b)->bitmap->height;

   return hb - ha;
}


void GFont::importStrip(const char *fileName, U32 padding, U32 kerning)
{
   // Wipe our texture sheets, and reload bitmap data from the specified file.
   // Also deal with kerning.
   // Also, we may have to load RGBA instead of RGB.

   // Wipe our texture sheets.
   mCurSheet = mCurX = mCurY = 0;
   mTextureSheets.clear();

   //  Now, load the font strip.
   GBitmap *strip = GBitmap::load(fileName);

   if(!strip)
   {
      Con::errorf("GFont::importStrip - could not load file '%s'!", fileName);
      return;
   }

   // And get parsing and copying - load up all the characters as separate
   // GBitmaps, sort, then pack. Not terribly efficient but this is basically
   // on offline task anyway.

   // Ok, snag some glyphs.
   Vector<GlyphMap> glyphList;
   glyphList.reserve(mCharInfoList.size());

   U32 curWidth = 0;
   for(S32 i=0; i<mCharInfoList.size(); i++)
   {
      // Skip invalid stuff.
      if(mCharInfoList[i].bitmapIndex == -1 || mCharInfoList[i].height == 0 || mCharInfoList[i].width == 0)
         continue;

      // Allocate a new bitmap for this glyph, taking into account kerning and padding.
      glyphList.increment();
      glyphList.last().bitmap = new GBitmap(mCharInfoList[i].width + kerning + 2*padding, mCharInfoList[i].height + 2*padding, false, strip->getFormat());
      glyphList.last().charId = i;

      // Copy the rect.
      RectI ri(curWidth, getBaseline() - mCharInfoList[i].yOrigin, glyphList.last().bitmap->width, glyphList.last().bitmap->height);
      Point2I outRi(0,0);
      glyphList.last().bitmap->copyRect(strip, ri, outRi); 

      // Update glyph attributes.
      mCharInfoList[i].width = glyphList.last().bitmap->width;
      mCharInfoList[i].height = glyphList.last().bitmap->height;
      mCharInfoList[i].xOffset -= kerning + padding;
      mCharInfoList[i].xIncrement += kerning;
      mCharInfoList[i].yOffset -= padding;

      // Advance.
      curWidth += ri.extent.x;
   }

   // Ok, we have a big list of glyphmaps now. So let's sort them, then pack them.
   dQsort(glyphList.address(), glyphList.size(), sizeof(GlyphMap), GlyphMapCompare);

   // They're sorted by height, so now we can do some sort of awesome packing.
   Point2I curSheetSize(256, 256);
   Vector<U32> sheetSizes;

   S32 curY = 0;
   S32 curX = 0;
   S32 curLnHeight = 0;
   S32 maxHeight = 0;
   for(U32 i = 0; i < glyphList.size(); i++)
   {
      PlatformFont::CharInfo *ci = &mCharInfoList[glyphList[i].charId];
      
      if(ci->height > maxHeight)
         maxHeight = ci->height;

      if(curX + ci->width > curSheetSize.x)
      {
         curY += curLnHeight;
         curX = 0;
         curLnHeight = 0;
      }

      if(curY + ci->height > curSheetSize.y)
      {
         sheetSizes.push_back(curSheetSize.y);
         curX = 0;
         curY = 0;
         curLnHeight = 0;
      }

      if(ci->height > curLnHeight)
         curLnHeight = ci->height;
      
      ci->bitmapIndex = sheetSizes.size();
      ci->xOffset = curX;
      ci->yOffset = curY;
      curX += ci->width;
   }

   // Terminate the packing loop calculations.
   curY += curLnHeight;

   if(curY < 64)
      curSheetSize.y = 64;
   else if(curY < 128)
      curSheetSize.y = 128;

   sheetSizes.push_back(curSheetSize.y);

   if(getHeight() + padding * 2 > maxHeight)
      maxHeight = getHeight() + padding * 2;

   // Allocate texture pages.
   for(S32 i=0; i<sheetSizes.size(); i++)
   {
      char buf[30];
      dSprintf(buf, sizeof(buf), "newfont_%d", smSheetIdCount++);

      GBitmap *bitmap = new GBitmap(TextureSheetSize, TextureSheetSize, false, strip->getFormat());

      // Set everything to transparent.
      U8 *bits = bitmap->getWritableBits();
      dMemset(bits, 0, sizeof(U8) *TextureSheetSize*TextureSheetSize * strip->bytesPerPixel);

      TextureHandle handle = TextureHandle( buf, bitmap );
      mTextureSheets.increment();
      constructInPlace(&mTextureSheets.last());
      mTextureSheets.last() = handle;
   }


   // Alright, we're ready to copy bits!
   for(S32 i=0; i<glyphList.size(); i++)
   {
      // Copy each glyph into the appropriate place.
      PlatformFont::CharInfo *ci = &mCharInfoList[glyphList[i].charId];
      U32 bi = ci->bitmapIndex;
      mTextureSheets[bi].getBitmap()->copyRect(glyphList[i].bitmap, RectI(0,0, glyphList[i].bitmap->width,glyphList[i].bitmap->height), Point2I(ci->xOffset, ci->yOffset));
   }

   // Ok, all done! Just refresh some textures and we're set.
   for(S32 i=0; i<sheetSizes.size(); i++)
      mTextureSheets[i].refresh();
}
