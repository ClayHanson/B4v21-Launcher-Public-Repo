#include "game/helpers/CallbackObject.h"

IMPLEMENT_CONOBJECT(CallbackObject);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

CallbackObject::CallbackObject()
{
}

CallbackObject::~CallbackObject()
{
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void CallbackObject::Reset()
{
	mEvent.ClearListeners();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

ConsoleMethod(CallbackObject, AddListener, bool, 3, 3, "(string func)")
{
	return object->mEvent.AddListener(CALLBACK_EVENT_LFUNC() {
		const char** newArgv = new const char*[argc + 1];
		newArgv[0]           = (const char*)userData;

		// Read the arguments
		for (int i = 0; i < argc; i++)
			newArgv[i + 1] = CALLBACK_EVENT_ARG(const char*);

		Con::execute(argc + 1, newArgv);
		delete[] newArgv;
	}, (void*)StringTable->insert(argv[2]));
}

ConsoleMethod(CallbackObject, RemoveListener, bool, 3, 3, "(string func)")
{
	for (U32 i = 0; i < object->mEvent.GetListenerCount(); i++)
	{
		CallbackEvent::EventAction* aObj = object->mEvent.GetListener(i);
		if (aObj->userData == NULL || dStricmp((StringTableEntry)aObj->userData, argv[2]))
			continue;

		// Remove it!
		CallbackEvent::EventFunction func = aObj->funcPtr;
		object->mEvent.RemoveListener(func);
		return true;
	}

	// Fail
	return false;
}

ConsoleMethod(CallbackObject, Invoke, void, 3, 2, "([args])")
{
	object->mEvent.Invoke(argc - 2, argv + 2);
}