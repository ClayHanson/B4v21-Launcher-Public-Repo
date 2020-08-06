#include "gui/utility/TimerUtil.h"
#include "math/mMath.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

TimerUtil::TimerUtil()
{
	reset();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void TimerUtil::update()
{
	if (!mStarted)
		return;

	mTimePassed += Sim::getCurrentTime() - mLastTime;
	mLastTime    = Sim::getCurrentTime();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ColorI TimerUtil::getColorValue(ColorI desired, U32 time, U32 offset)
{
	return ColorI(desired.red, desired.green, desired.blue, U8((F32)desired.alpha * getAmount(time, offset)));
}

ColorF TimerUtil::getColorValue(ColorF desired, U32 time, U32 offset)
{
	return ColorF(desired.red, desired.green, desired.blue, F32(desired.alpha * getAmount(time, offset)));
}

F32 TimerUtil::getAmount(U32 time, U32 offset)
{
	update();
	if (mInverse)
		return 1.f - mClampF((F32)(mTimePassed - offset) / (F32)time, 0.f, 1.f);

	return mClampF((F32)(mTimePassed - offset) / (F32)time, 0.f, 1.f);
}

U32 TimerUtil::getTimePassed()
{
	update();
	return mTimePassed;
}

void TimerUtil::start(bool inverse)
{
	if (mStarted)
		return;

	mInverse  = inverse;
	mLastTime = Sim::getCurrentTime();
	mStarted  = true;
}

void TimerUtil::stop()
{
	if (!mStarted)
		return;

	update();
	mStarted = false;
}

void TimerUtil::reset()
{
	mTimePassed = 0;
	mLastTime   = 0;
	mStarted    = false;
	mInverse    = false;
}