#ifndef _CALLBACK_OBJECT_H_
#define _CALLBACK_OBJECT_H_

#include "console/consoleInternal.h"
#include "game/helpers/CallbackEvent.h"
#include "core/tVector.h"

class CallbackObject : public SimObject
{
protected:
	typedef SimObject Parent;

public:
	Vector<StringTableEntry> mCallbacks;
	CallbackEvent mEvent;

public:
	CallbackObject();
	~CallbackObject();

	DECLARE_CONOBJECT(CallbackObject);

public:
	void Reset();
};

#endif