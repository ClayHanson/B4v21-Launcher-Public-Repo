#ifndef _STRING_H_
#define _STRING_H_

#include "gui/utility/TimerUtil.h"
#include "console/consoleTypes.h"
#include "core/tVector.h"
#include "math/mRect.h"

class GFont;
class String
{
protected: // Variables
	Vector<S32> mWordIndexes;
	Vector<S32> mRecordIndexes;
	Vector<S32> mFieldIndexes;
	char* mBuffer;
	U32 mBufferLength;
	U32 mBufferSize;
	U32 mHash;

protected: // GUI Variables
	U32 mScrollTimer;
	U32 mStringWidth;
	GFont* mFont;
	U32 mScroll;
	U8 mScrollAct;

public: // Base stuff
	String();
	String(const String& str);
	String(const char* str, U32 size = 0);
	~String();

protected: // Protected methods
	inline S32 GetIndexedLength(Vector<S32>* vec, U32 index);
	inline void SetIndexValue(Vector<S32>* vec, U32 index, const char* text);
	inline void CalculateIndex(Vector<S32>* vec, U32 offset = 0);
	inline void CalculateIndexes(U32 offset = 0);
	inline bool FitSize(U32 size);
	inline bool AddString(const char* str, U32 size = 0);
	inline void SetString(const char* str, U32 size = 0);
	inline void HashString();
	inline S32 ValidateIndex(S32 index);

public: // Methods
	/// Add a formatted string
	void append(const char* _format, ...);

	/// Set the buffer to a formatted string
	void set(const char* _format, ...);

	/// Converts the string to all uppercase.
	void toUpperCase();

	/// Converts the string to all lowercase.
	void toLowerCase();

	/// Returns the amount of occurances that were replaced in the string.
	S32 replace(const char* from, const char* to);

	/// Find a needle in the haystack at the given offset.
	S32 indexOf(const char* needle, U32 offset = 0, U32 size = 0);

	/// Trim the left side of the string.
	void lTrim();

	/// Trim the right side of the string.
	void rTrim();

	/// Completely trim the string.
	void trim();

	/// Strip characters from the string. Returns the amount of characters that were stripped.
	U32 stripCharacters(const char* badTable);

	/// Get a sub of the string
	String subStr(U32 offset, U32 length);

	/// @Fields
	/// @ {
	inline S32 getFieldCount() { return mFieldIndexes.size(); }
	String getField(U32 index);
	void setField(U32 index, const char* text);
	/// @ }

	/// @Records
	/// @ {
	inline S32 getRecordCount() { return mRecordIndexes.size(); }
	String getRecord(U32 index);
	void setRecord(U32 index, const char* text);
	/// @ }

	/// @Words
	/// @ {
	inline S32 getWordCount() { return mWordIndexes.size(); }
	String getWord(U32 index);
	void setWord(U32 index, const char* text);
	/// @ }

#ifdef TORQUE_DEBUG
	/// @Debug
	/// @{
	void dumpIndexes();
	/// @}
#endif

	/// Get the length of the string
	S32 length();

	/// Get the string
	inline const char* get() { return mBuffer; }

	/// Get the hash of the string
	inline U32 getHash() { return mHash; }

public: // GUI Methods
	inline U32 getStringWidth() { return mStringWidth; }
	inline GFont* getDrawFont() { return mFont; }
	void drawText(const RectI& rc, ColorI color = ColorI(255, 255, 255, 255), bool allowScroll = true);
	void setDrawFont(GFont* font);

public: // Overloads
	operator const char* () const { return (const char*)mBuffer; }
	operator char* () const { return (char*)mBuffer; }
	String operator+(const char* str);
	String& operator+=(const char* str);
	String& operator=(const char* str);
	String operator+(String& str);
	String& operator+=(String& str);
	String& operator=(const String& str);
	char operator[](U32 idx);
	bool operator==(const char* str);
	bool operator==(const String str);
	bool operator!=(const char* str);
	bool operator!=(const String str);
};

#endif