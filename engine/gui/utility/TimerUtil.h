#ifndef _TIMER_UTIL_H_
#define _TIMER_UTIL_H_

#include "console/consoleTypes.h"
#include "core/color.h"

class TimerUtil
{
private: // Variables
	U32 mTimePassed;
	U32 mLastTime;
	bool mStarted;
	bool mInverse;

public: // Init
	TimerUtil();

protected: // Protected Methods
	void update();

public: // Methods
	inline bool isRunning() { return mStarted; }
	inline bool isInverse() { return mInverse; }
	inline F32 getEndValue() { return mInverse ? 0.f : 1.f; }
	ColorI getColorValue(ColorI desired, U32 time, U32 offset = 0);
	ColorF getColorValue(ColorF desired, U32 time, U32 offset = 0);
	F32 getAmount(U32 time, U32 offset = 0);
	U32 getTimePassed();
	void start(bool inverse = false);
	void stop();
	void reset();
};

#endif