#ifndef _CALLBACK_EVENT_H_
#define _CALLBACK_EVENT_H_

#include "console/consoleInternal.h"

class CallbackEvent {
public: // Types
	typedef void(*EventFunction)(void* userData, U32 argc, char* argList);

public: // Data
	struct EventAction
	{
		EventFunction funcPtr;
		void* userData;
	};

private: // Variables
	EventAction** mActionList;
	U32 mActionCount;

public: // Initializers
	CallbackEvent();
	~CallbackEvent();

private: // Memory management
	bool ResizeList(U32 newSize);

public: // For ease-of-access
	template<typename T> static T GetArg(char** argList)
	{
		return (*(T*)((*argList += _INTSIZEOF(T)) - _INTSIZEOF(T)));
	}

public:
	EventAction* GetListener(U32 index);
	U32 GetListenerCount();

	bool AddListener(EventFunction funcPtr, void* userData = NULL);
	void RemoveListener(EventFunction funcPtr);
	void ClearListeners();
	void Invoke(U32 argc, ...);
	void Invoke(U32 argc, const char** argv);
};

#define CALLBACK_EVENT_ARG(type) CallbackEvent::GetArg<type>(&argList)
#define CALLBACK_EVENT_LFUNC() [](void* userData, U32 argc, char* argList)->void 

#endif