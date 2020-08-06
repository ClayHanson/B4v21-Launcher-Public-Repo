#include "console/String.h"
#include "dgl/gFont.h"
#include "dgl/dgl.h"
#include <stdarg.h>

#define SCROLL_CYCLE_TIME 2000

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool strInitTable = true;
U8   strHashTable[256];

void initHashTable()
{
	for (U32 i = 0; i < 256; i++)
	{
		U8 c           = (U8)i;
		strHashTable[i] = c * c;
	}

	strInitTable = false;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

String::String()
{
	mBuffer       = NULL;
	mBufferLength = 0;
	mBufferSize   = 0;
	mHash         = 0;
	mScroll       = 0;
	mScrollTimer  = 0;
	mScrollAct    = 0;
	mFont         = NULL;
}

String::String(const String& str)
{
	mBuffer       = NULL;
	mBufferLength = 0;
	mBufferSize   = 0;
	mHash         = 0;
	mScroll       = 0;
	mScrollTimer  = 0;
	mScrollAct    = 0;
	mFont         = NULL;

	AddString(str.mBuffer, str.mBufferLength);
}

String::String(const char* str, U32 size)
{
	mBuffer       = NULL;
	mBufferLength = 0;
	mBufferSize   = 0;
	mHash         = 0;
	mScroll       = 0;
	mScrollTimer  = 0;
	mScrollAct    = 0;
	mFont         = NULL;

	AddString(str, size ? size : dStrlen(str));
}

String::~String()
{
	if (mBuffer)
		dFree(mBuffer);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

S32 String::GetIndexedLength(Vector<S32>* vec, U32 index)
{
	return ((index + 1) >= vec->size() ? mBufferLength - (*vec)[index] : ((*vec)[index + 1] - 1) - (*vec)[index]);
}

void String::SetIndexValue(Vector<S32>* vec, U32 index, const char* text)
{
	// Sanity check
	if (index >= vec->size() && (vec->size() - 1 == (U32)-1 || ((index = vec->size() - 1) == (U32)-1)))
	{
		Con::errorf("String::SetIndexValue() - Index out of range; index = %d, size = %d", index, vec->size());
		return;
	}

	// Offset all subsequent indexes
	U32 length    = dStrlen(text);
	U32 oldLength = GetIndexedLength(vec, index);
	U32 offset    = length - oldLength;
	U32 start     = (*vec)[index];

	// Offset them
	for (Vector<S32>::iterator it = vec->begin() + index + 1; it != vec->end(); it++)
		((S32)(*it)) += offset;

	// Set the new buffer length
	mBufferLength += offset;

	// Make sure the buffer can accomadate our new length
	FitSize(mBufferLength + 1);

	// Move the buffer around accordingly
	if (length > oldLength) // The new length is longer than the old one; move the buffer to the right
		dMemmove(mBuffer + start + offset, mBuffer + start, mBufferLength - start);
	else if (length < oldLength) // The new length is shorter than the old one; move the buffer to the length
		dMemmove(mBuffer + (start + oldLength) + offset, mBuffer + (start + oldLength), mBufferLength - ((start + oldLength) + offset));

	// Copy the text to the index
	dMemcpy(mBuffer + start, text, length);

	// Null-terminate the buffer
	mBuffer[mBufferLength] = 0;

	// Re-calculate words
	if (vec != &mWordIndexes)
		CalculateIndex(&mWordIndexes);
	if (vec != &mRecordIndexes)
		CalculateIndex(&mRecordIndexes);
	if (vec != &mFieldIndexes)
		CalculateIndex(&mFieldIndexes);
}

void String::CalculateIndex(Vector<S32>* vec, U32 offset)
{
	if (offset == 0)
	{
		vec->clear();
		vec->push_back(0);
	}

	// Convert all characters to uppercase
	int identifier = (vec == &mFieldIndexes ? 0 : (vec == &mWordIndexes ? 1 : 2));
	char* end      = mBuffer + mBufferLength;
	for (char* ptr = mBuffer + offset; ptr != end; ptr++)
	{
		if (identifier == 0 && *ptr == '\t') // Fields
			vec->push_back((ptr - mBuffer) + 1);
		if (identifier == 1 && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) // Words
			vec->push_back((ptr - mBuffer) + 1);
		if (identifier == 2 && *ptr == '\n') // Records
			vec->push_back((ptr - mBuffer) + 1);
	}
}

void String::CalculateIndexes(U32 offset)
{
	CalculateIndex(&mFieldIndexes, offset);
	CalculateIndex(&mWordIndexes, offset);
	CalculateIndex(&mRecordIndexes, offset);
}

bool String::FitSize(U32 size)
{
	// Sanity check
	if (!size)
		return false;

	// If we're already big enough for this, then do nothing here
	if (size <= mBufferSize)
		return true;

	// Determine what to do to the buffer
	if (!mBuffer)
	{
		// Create the buffer
		mBufferSize = size + 64; // Alloc a little more than we need
		mBuffer     = (char*)dMalloc(mBufferSize);
	}
	else if (size >= mBufferSize)
	{
		// Resize the buffer
		mBufferSize = size + 64; // Alloc a little more than we need
		mBuffer     = (char*)dRealloc((void*)mBuffer, mBufferSize);
	}

	return true;
}

bool String::AddString(const char* str, U32 size)
{
	if (!str)
		return false;

	if (!size)
		size = dStrlen(str);

	// Add the string to the length
	mBufferLength += size;

	// Determine what to do to the buffer
	FitSize(mBufferLength + 1);

	// Determine the add position
	char* ptr = mBuffer + (mBufferLength - size);

	// Add this string to the buffer
	dMemcpy(ptr, str, size);
	ptr[size] = 0;

	// Calculate indexes
	CalculateIndexes(mBufferLength - size);

	// Calculate the new hash
	HashString();

	// Done!
	return true;
}

void String::SetString(const char* str, U32 size)
{
	// Reset the buffer elements
	mBufferLength = 0;

	// Set the string
	AddString(str, size);
}

void String::HashString()
{
	if (!mBuffer)
	{
		mHash = 0;
		return;
	}

	// Init the hash table if necessary
	if (strInitTable)
		initHashTable();

	char* ptr = mBuffer;
	U32 len   = mBufferLength;
	mHash     = 0;

	char c;
	while ((c = *ptr++) != 0 && len--)
	{
		mHash <<= 1;
		mHash  ^= strHashTable[c];
	}

	// Calculate the string width
	if (mFont)
		mStringWidth = mFont->getStrNWidthPrecise((const UTF8*)mBuffer, mBufferLength);
}

S32 String::ValidateIndex(S32 index)
{
	return (index < 0 ? 0 : (index >= mBufferLength ? mBufferLength : index));
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void String::append(const char* _format, ...)
{
	char buffer[2048];
	va_list list;
	va_start(list, _format);
	dVsprintf(buffer, sizeof(buffer), _format, list);
	va_end(list);

	AddString(buffer);
}

void String::set(const char* _format, ...)
{
	char buffer[2048];
	va_list list;
	va_start(list, _format);
	dVsprintf(buffer, sizeof(buffer), _format, list);
	va_end(list);

	SetString(buffer);
}

void String::toUpperCase()
{
	// Convert all characters to uppercase
	char* end = mBuffer + mBufferLength;
	for (char* ptr = mBuffer; ptr != end; ptr++)
	{
		if (*ptr < 97 || *ptr > 122)
			continue;

		*ptr = (*ptr - 97) + 65;
	}

	// Rehash the string
	HashString();
}

void String::toLowerCase()
{
	// Convert all characters to uppercase
	char* end = mBuffer + mBufferLength;
	for (char* ptr = mBuffer; ptr != end; ptr++)
	{
		if (*ptr < 65 || *ptr > 90)
			continue;

		*ptr = (*ptr - 65) + 97;
	}

	// Rehash the string
	HashString();
}

S32 String::replace(const char* from, const char* to)
{
	// Replace characters in the string
	S32 fromLen = dStrlen(from);
	S32 toLen   = dStrlen(to);
	S32 count   = 0;
	
	// Get the number of occurances for the 'from' string
	const char* scan = mBuffer;
	while (scan)
	{
		scan = dStrstr(scan, from);
		if (scan)
		{
			scan += fromLen;
			count++;
		}
	}

	// Perform the replacing
	mBufferLength   = mBufferLength + ((toLen - fromLen) * count);
	mBufferSize     = mBufferLength + 32 + ((toLen - fromLen) * count);
	char* newBuffer = (char*)dMalloc(mBufferSize);
	char* dstPtr    = newBuffer;
	char* scanPtr   = mBuffer;
	U32 scanp       = 0;
	U32 dstp        = 0;

	for (;;)
	{
		const char* scan = dStrstr(scanPtr, from);
		if (!scan)
		{
			dStrcpy(dstPtr, scanPtr);
			break;
		}

		U32 len  = scan - scanPtr;
		dStrncpy(dstPtr, scanPtr, len);
		dstPtr  += len;
		dStrcpy(dstPtr, to);
		dstPtr  += toLen;
		scanPtr += len + fromLen;
	}

	// Replace the old buffer
	if (mBuffer)
		dFree(mBuffer);

	mBuffer = newBuffer;

	// Calculate new hash
	HashString();

	// Done!
	return count;
}

S32 String::indexOf(const char* needle, U32 offset, U32 size)
{
	U32 mem_size = (!size ? dStrlen(needle) : size);

	// Sanity check
	if (mem_size >= mBufferLength)
		return -1;

	// Determine the type of comparison
	if (size)
	{
		// Memory comparison
		char* end = mBuffer + (mBufferLength - mem_size);
		for (char* ptr = mBuffer + ValidateIndex(offset); ptr != end; ptr++)
			if (!dMemcmp(ptr, needle, mem_size))
				return ptr - mBuffer;

		// Couldn't find it!
		return -1;
	}

	// Normal string comparison
	char* end = mBuffer + (mBufferLength - mem_size);
	for (char* ptr = mBuffer + ValidateIndex(offset); ptr != end; ptr++)
		if (!dStrnicmp(ptr, needle, mem_size))
			return ptr - mBuffer;
	
	// Couldn't find it!
	return -1;
}

void String::lTrim()
{
	// Find the new start
	char* start = mBuffer;
	char* end   = mBuffer + mBufferLength + 1;
	for (char* ptr = start; ptr != end; ptr++)
	{
		// Skip whitespace
		if (*ptr <= 32)
			continue;

		start = ptr;
		break;
	}

	// Don't do anything if we found no whitespace
	if (start == mBuffer)
		return;

	// Trim the string!
	dMemmove(mBuffer, start, end - start);
	mBufferLength = end - start;

	// Re-hash the string
	HashString();
}

void String::rTrim()
{
	// Find the new end
	char* start = mBuffer;
	char* end   = mBuffer + mBufferLength + 1;
	for (char* ptr = end; ptr != start; ptr--)
	{
		// Skip whitespace
		if (*ptr <= 32)
			continue;

		end = ptr;
		break;
	}

	// Don't do anything if we found no whitespace
	if (end == (mBuffer + mBufferLength + 1))
		return;

	end++;

	// Trim the string!
	*end          = 0;
	mBufferLength = end - start;

	// Re-hash the string
	HashString();
}

void String::trim()
{
	lTrim();
	rTrim();
}

U32 String::stripCharacters(const char* badTable)
{
	U32 ret = 0;

	// Loop through the bad chars
	for (const char* badPtr = badTable; *badPtr != 0; badPtr++)
	{
		char chr  = *badPtr;
		char* ptr = mBuffer;

		// Clear the string out
		while ((ptr = dStrchr(ptr, chr)) != NULL)
		{
			dMemmove(ptr, ptr + 1, (mBuffer + mBufferLength) - ptr);
			mBufferLength--;
			ret++;
		}
	}

	// Re-hash the string
	HashString();

	// Done!
	return ret;
}

String String::subStr(U32 offset, U32 length)
{
	String ret(mBuffer + ValidateIndex(offset), ((offset + length) >= mBufferLength ? length - ((offset + length) - mBufferLength) : length));
	return ret;
}

/// Get a field
String String::getField(U32 index)
{
	String ret;

	// Sanity check
	if (index >= mFieldIndexes.size() && (index = mFieldIndexes.size() - 1) == (U32)-1)
		return ret;

	// Get the string!
	ret.SetString(mBuffer + mFieldIndexes[index], GetIndexedLength(&mFieldIndexes, index));
	return ret;
}

/// Set a field
void String::setField(U32 index, const char* text)
{
	SetIndexValue(&mFieldIndexes, index, text);
}

String String::getRecord(U32 index)
{
	String ret;

	// Sanity check
	if (index >= mRecordIndexes.size() && (index = mRecordIndexes.size() - 1) == (U32)-1)
		return ret;
	
	// Get the string!
	ret.SetString(mBuffer + mRecordIndexes[index], GetIndexedLength(&mRecordIndexes, index));
	return ret;
}

void String::setRecord(U32 index, const char* text)
{
	SetIndexValue(&mRecordIndexes, index, text);
}

String String::getWord(U32 index)
{
	String ret;

	// Sanity check
	if (index >= mWordIndexes.size() && (index = mWordIndexes.size() - 1) == (U32)-1)
		return ret;

	// Get the string!
	ret.SetString(mBuffer + mWordIndexes[index], GetIndexedLength(&mWordIndexes, index));
	return ret;
}

void String::setWord(U32 index, const char* text)
{
	SetIndexValue(&mWordIndexes, index, text);
}

#ifdef TORQUE_DEBUG
void String::dumpIndexes()
{
	Vector<S32>* vecs[]    = { &mFieldIndexes, &mWordIndexes, &mRecordIndexes };
	const char* vecNames[] = { "Fields",       "Words",       "Records" };
	char buf[1024];

	for (S32 i = 0; i < sizeof(vecNames) / sizeof(const char*); i++)
	{
		Vector<S32>* vec = vecs[i];
		Con::printf("[%04d] \"%s\":", i, vecNames[i]);
		for (Vector<S32>::iterator it = vec->begin(); it != vec->end(); it++)
		{
			S32 start = *it;
			S32 length = GetIndexedLength(vec, it - vec->begin());

			*buf = 0;
			dStrncat(buf, mBuffer + start, length);

			Con::printf("   > [%04d]: (START=%d, LENGTH=%d, STRING=\"%s\")", it - vec->begin(), start, length, buf);
		}
	}
}
#endif

S32 String::length()
{
	return mBufferLength;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void String::drawText(const RectI& rc, ColorI color, bool allowScroll)
{
	if (!mFont)
		return;

	dglSetBitmapModulation(color);
	if (mStringWidth > rc.extent.x && allowScroll)
	{
		U32 scrollAmt = mStringWidth - rc.extent.x;

		// Apply the new clip rect
		RectI old = dglGetClipRect();
		dglSetClipRect(rc);

		// Draw the scrolling text
		S32 time = Sim::getCurrentTime() - mScrollTimer;
		if (time >= SCROLL_CYCLE_TIME)
		{
			mScrollTimer = Sim::getCurrentTime();
			mScrollAct   = (mScrollAct + 1) % 4;
			time         = Sim::getCurrentTime() - mScrollTimer;
		}

		switch (mScrollAct)
		{
			case 0: // Wait
			{
				dglDrawText(mFont, rc.point, mBuffer);
				break;
			}
			case 1: // Scroll to the left
			{
				S32 xOff = S32((F32)mScroll * F32((F32)time / (F32)SCROLL_CYCLE_TIME));
				dglDrawText(mFont, Point2I(rc.point.x - xOff, rc.point.y), mBuffer);
				break;
			}
			case 2: // Wait
			{
				dglDrawText(mFont, Point2I(rc.point.x - scrollAmt, rc.point.y), mBuffer);
				break;
			}
			case 3: // Scroll to the right
			{
				S32 xOff = S32((F32)mScroll * F32((F32)time / (F32)SCROLL_CYCLE_TIME));
				dglDrawText(mFont, Point2I(rc.point.x - (scrollAmt - xOff), rc.point.y), mBuffer);
				break;
			}
		}

		// Apply the old clip rect
		dglSetClipRect(old);
	}

	dglDrawText(mFont, rc.point, mBuffer);
}

void String::setDrawFont(GFont* font)
{
	mFont = font;

	// Calculate the string width
	if (mFont && mBuffer)
		mStringWidth = mFont->getStrNWidthPrecise((const UTF8*)mBuffer, mBufferLength);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

String String::operator+(const char* str)
{
	String ret(mBuffer, mBufferLength);
	
	// Add the string to it
	ret.AddString(str);

	// Done!
	return ret;
}

String& String::operator+=(const char* str)
{
	AddString(str);
	return *this;
}

String& String::operator=(const char* str)
{
	SetString(str);
	return *this;
}


String String::operator+(String& str)
{
	String ret(mBuffer, mBufferLength);

	// Add the string to it
	ret.AddString(str.mBuffer, str.mBufferLength);

	// Done!
	return ret;
}

String& String::operator+=(String& str)
{
	AddString(str.mBuffer, str.mBufferLength);
	return *this;
}

String& String::operator=(const String& str)
{
	SetString(str.mBuffer, str.mBufferLength);
	return *this;
}

char String::operator[](U32 idx)
{
	return mBuffer ? mBuffer[ValidateIndex(idx)] : 0;
}

bool String::operator==(const char* str)
{
	return !dStricmp(str, mBuffer);
}

bool String::operator==(const String str)
{
	return str.mHash == mHash;
}

bool String::operator!=(const char* str)
{
	return dStricmp(str, mBuffer);
}

bool String::operator!=(const String str)
{
	return str.mHash != mHash;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------