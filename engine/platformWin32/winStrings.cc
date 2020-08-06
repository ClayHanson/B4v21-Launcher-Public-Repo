//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformWin32/platformWin32.h"

#ifdef HAS_VSSCANF
#  undef HAS_VSSCANF
#endif

#if defined(TORQUE_COMPILER_CODEWARRIOR)
#  define HAS_VSSCANF
#  define __vsscanf vsscanf
#endif

#if defined(TORQUE_COMPILER_GCC)
#  include <stdlib.h>
#  include <ctype.h>
#endif


char *dStrdup_r(const char *src, const char *fileName, dsize_t lineNumber)
{
   char *buffer = (char *) dMalloc_r(dStrlen(src) + 1, fileName, lineNumber);
   dStrcpy(buffer, src);
   return buffer;
}

char* dStrcat(char *dst, const char *src)
{
   return strcat(dst,src);
}

/*UTF8* dStrcat(UTF8 *dst, const UTF8 *src)
{
   return (UTF8*)strcat((char*)dst,(char*)src);
} */  

char* dStrncat(char *dst, const char *src, dsize_t len)
{
   return strncat(dst,src,len);
}

// concatenates a list of src's onto the end of dst
// the list of src's MUST be terminated by a NULL parameter
// dStrcatl(dst, sizeof(dst), src1, src2, NULL);
char* dStrcatl(char *dst, dsize_t dstSize, ...)
{
   const char* src;
   char *p = dst;

   AssertFatal(dstSize > 0, "dStrcatl: destination size is set zero");
   dstSize--;  // leave room for string termination

   // find end of dst
   while (dstSize && *p++)
      dstSize--;

   va_list args;
   va_start(args, dstSize);

   // concatenate each src to end of dst
   while ( (src = va_arg(args, const char*)) != NULL )
      while( dstSize && *src )
      {
         *p++ = *src++;
         dstSize--;
      }

   va_end(args);

   // make sure the string is terminated
   *p = 0;

   return dst;
}


// copy a list of src's into dst
// the list of src's MUST be terminated by a NULL parameter
// dStrccpyl(dst, sizeof(dst), src1, src2, NULL);
char* dStrcpyl(char *dst, dsize_t dstSize, ...)
{
   const char* src;
   char *p = dst;

   AssertFatal(dstSize > 0, "dStrcpyl: destination size is set zero");
   dstSize--;  // leave room for string termination

   va_list args;
   va_start(args, dstSize);

   // concatenate each src to end of dst
   while ( (src = va_arg(args, const char*)) != NULL )
      while( dstSize && *src )
      {
         *p++ = *src++;
         dstSize--;
      }

   va_end(args);

   // make sure the string is terminated
   *p = 0;

   return dst;
}


int dStrcmp(const char *str1, const char *str2)
{
   return strcmp(str1, str2);
}

int dStrcmp(const UTF16 *str1, const UTF16 *str2)
{
    return wcscmp(str1, str2);
}
 
int dStricmp(const char *str1, const char *str2)
{
   return stricmp(str1, str2);
}

int dStrncmp(const char *str1, const char *str2, dsize_t len)
{
   return strncmp(str1, str2, len);
}

int dStrnicmp(const char *str1, const char *str2, dsize_t len)
{
   return strnicmp(str1, str2, len);
}

char* dStrcpy(UTF8 *dst, const UTF8 *src)
{
   return strcpy((char*)dst,(char*)src);
}

char* dStrncpy(char *dst, const char *src, dsize_t len)
{
   return strncpy(dst,src,len);
}

/*char* dStrncpy(UTF8 *dst, const UTF8 *src, dsize_t len)
{
   return strncpy((char*)dst,(char*)src,len);
} */  

dsize_t dStrlen(const char *str)
{
   return (dsize_t)strlen(str);
}   

/*dsize_t dStrlen(const UTF8 *str)
{
    // [tom, 7/12/2005] http://mail.nl.linux.org/linux-utf8/2000-06/msg00002.html
    int c = 0;
    for(; str; str = getNextUTF8Char(str))
        c++;
    
    return c;
}*/

dsize_t dStrlen(const UTF16 *str)
{
    return (dsize_t)wcslen(str);
}

char* dStrupr(char *str)
{
#if defined(TORQUE_COMPILER_CODEWARRIOR)
   // metrowerks strupr is broken
   _strupr(str);
   return str;
#else
   return strupr(str);
#endif
}


char* dStrlwr(char *str)
{
   return strlwr(str);
}


char* dStrchr(char *str, S32 c)
{
   return strchr(str,c);
}


const char* dStrchr(const char *str, S32 c)
{
   return strchr(str,c);
}


const char* dStrrchr(const char *str, S32 c)
{
   return strrchr(str,c);
}

char* dStrrchr(char *str, S32 c)
{
   return strrchr(str,c);
}

dsize_t dStrspn(const char *str, const char *set)
{
   return (dsize_t)strspn(str, set);
}

dsize_t dStrcspn(const char *str, const char *set)
{
   return (dsize_t)strcspn(str, set);
}


char* dStrstr(char *str1, char *str2)
{
   return strstr(str1,str2);
}

char* dStrstr(const char *str1, const char *str2)
{
   return strstr((char *)str1,str2);
}


char* dStrtok(char *str, const char *sep)
{
   return strtok(str, sep);
}

long int dStrtol(const char *str1, char **endptr, int base)
{
	return strtol(str1, endptr, base);
}


S32 dAtoi(const char *str)
{
   return atoi(str);
}

F32 dAtof(const char *str)
{
   // Warning: metrowerks crashes when strange strings are passed in '0x [enter]' for example!
   return (F32)atof(str);
}

bool dAtob(const char *str)
{
   return !dStricmp(str, "true") || dAtof(str);
}


bool dIsalnum(const char c)
{
   return isalnum(c)!=0;
}

bool dIsalpha(const char c)
{
   return isalpha(c)!=0;
}

bool dIsspace(const char c)
{
   return isspace(c)!=0;
}

bool dIsdigit(const char c)
{
   return isdigit(c)!=0;
}

void dPrintf(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vprintf(format, args);
}

S32 dVprintf(const char *format, void *arglist)
{
   S32 len = vprintf(format, (char*)arglist);
   return (len);
}

S32 dSprintf(char *buffer, U32 bufferSize, const char *format, ...)
{
   va_list args;
   va_start(args, format);

   S32 len = vsnprintf(buffer, bufferSize, format, args);

   AssertFatal( len < bufferSize, "dSprintf wrote to more memory than the specified buffer size" );

   return (len);
}

S32 dVsprintf(char *buffer, U32 bufferSize, const char *format, void *arglist)
{
   S32 len = vsnprintf(buffer, bufferSize, format, (char*)arglist);

   AssertFatal( len < bufferSize, "dVsprintf wrote to more memory than the specified buffer size" );

   return (len);
}

S32 dSscanf(const char *buffer, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   S32 result = vsscanf(buffer, format, args);
   va_end(args);
   return result;
}

S32 dFflushStdout()
{
   return fflush(stdout);
}

S32 dFflushStderr()
{
   return fflush(stderr);
}

void dQsort(void *base, U32 nelem, U32 width, S32 (QSORT_CALLBACK *fcmp)(const void *, const void *))
{
   qsort(base, nelem, width, fcmp);
}   

UTF8 * convertUTF16toUTF8(const UTF16 *string, UTF8 *buffer, U32 bufsize)
{
    int nRet;
    if((nRet = WideCharToMultiByte(CP_UTF8, 0, string, dStrlen(string), (LPSTR)buffer, bufsize, NULL, NULL)) != 0)
    {
        buffer[nRet] = 0;
        return buffer;
    }
    else
        return NULL;
}

UTF16 * convertUTF8toUTF16(const UTF8 *string, UTF16 *buffer, U32 bufsize)
{
    int nRet;
    if((nRet = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)string, dStrlen((const char *)string), buffer, bufsize)) != 0)
    {
        buffer[nRet] = 0;
        return buffer;
    }
    else
        return NULL;
}
