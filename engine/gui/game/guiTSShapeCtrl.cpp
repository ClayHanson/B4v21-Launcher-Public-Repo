#include "gui/core/guiTSControl.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneLighting.h"
#include "sceneGraph/sceneGraph.h"
#include "game/gameBase.h"

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define MaxScriptThreads 4
#define MaxSequenceIndex 4

class GuiShapeCtrl : public GuiTSCtrl
{
	typedef GuiTSCtrl Parent;

public:
	struct Thread {
		/// State of the animation thread.
		enum State {
			Play, Stop, Pause
		};
		TSThread* thread; ///< Pointer to 3space data.
		U32 state;        ///< State of the thread
						  ///
						  ///  @see Thread::State
		S32 sequence;     ///< The animation sequence which is running in this thread.
		U32 sound;        ///< Handle to sound.
		bool atEnd;       ///< Are we at the end of this thread?
		bool forward;     ///< Are we playing the thread forward? (else backwards)
	};

	Thread mScriptThread[MaxScriptThreads];

public:
	StringTableEntry mShapeName;
	TSShapeInstance* mShape;

public:
	CameraQuery    mLastCameraQuery;
	GuiShapeCtrl();

	virtual void renderWorld(const RectI& updateRect);
	static void initPersistFields();
	void setShape(const char* file);
	
public:
	bool setThreadSequence(U32 slot, S32 seq, bool reset = true)
	{
		Thread& st = mScriptThread[slot];
		if (st.thread && st.sequence == seq && st.state == Thread::Play && !reset)
			return true;

		if (seq < MaxSequenceIndex)
		{
			st.sequence = seq;
			if (reset)
			{
				st.state = Thread::Play;
				st.atEnd = false;
				st.forward = true;
			}

			if (mShape)
			{
				if (!st.thread)
					st.thread = mShape->addThread();

				mShape->setSequence(st.thread, seq, 0);
				updateThread(st);
			}

			return true;
		}

		return false;
	}

	void updateThread(Thread& st)
	{
		switch (st.state)
		{
			case Thread::Stop:
				mShape->setTimeScale(st.thread, 1);
				mShape->setPos(st.thread, 0);
				// Drop through to pause state
			case Thread::Pause:
				mShape->setTimeScale(st.thread, 0);
				break;
			case Thread::Play:
				if (st.atEnd)
				{
					mShape->setTimeScale(st.thread, 1);
					mShape->setPos(st.thread, st.forward ? 1 : 0);
					mShape->setTimeScale(st.thread, 0);
				}
				else
				{
					mShape->setTimeScale(st.thread, st.forward ? 1 : -1);
				}

				break;
		}
	}

	bool stopThread(U32 slot)
	{
		Thread& st = mScriptThread[slot];
		if (st.sequence != -1 && st.state != Thread::Stop)
		{
			st.state = Thread::Stop;
			updateThread(st);
			return true;
		}

		return false;
	}

	bool pauseThread(U32 slot)
	{
		Thread& st = mScriptThread[slot];
		if (st.sequence != -1 && st.state != Thread::Pause)
		{
			st.state = Thread::Pause;
			updateThread(st);
			return true;
		}

		return false;
	}

	bool playThread(U32 slot)
	{
		Thread& st = mScriptThread[slot];
		if (st.sequence != -1 && st.state != Thread::Play)
		{
			st.state = Thread::Play;
			updateThread(st);
			return true;
		}

		return false;
	}

	void advanceThreads(F32 dt)
	{
		for (U32 i = 0; i < MaxScriptThreads; i++)
		{
			Thread& st = mScriptThread[i];
			if (st.thread)
			{
				if (!mShape->getShape()->sequences[st.sequence].isCyclic() && !st.atEnd && (st.forward ? mShape->getPos(st.thread) >= 1.0 : mShape->getPos(st.thread) <= 0))
				{
					st.atEnd = true;
					updateThread(st);
				}

				mShape->advanceTime(dt, st.thread);
			}
		}
	}

	DECLARE_CONOBJECT(GuiShapeCtrl);

protected:
	static bool _setShapeName(void* obj, const char* data)
	{
		static_cast<GuiShapeCtrl*>(obj)->setShape(data);
		return false;
	}
};

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiShapeCtrl);

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GuiShapeCtrl::GuiShapeCtrl()
{
	mShapeName = StringTable->insert("");
	mShape     = NULL;

	for (S32 i = 0; i < MaxScriptThreads; i++)
	{
		mScriptThread[i].sequence = -1;
		mScriptThread[i].thread   = 0;
		mScriptThread[i].sound    = 0;
		mScriptThread[i].state    = Thread::Stop;
		mScriptThread[i].atEnd    = false;
		mScriptThread[i].forward  = true;
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiShapeCtrl::initPersistFields()
{
	Parent::initPersistFields();
	addProtectedField("shape", TypeFilename, Offset(mShapeName, GuiShapeCtrl), &_setShapeName, &defaultProtectedGetFn, "");
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GuiShapeCtrl::setShape(const char* file)
{
	mShapeName = StringTable->insert(file);

	// Delete the existing instance, if any
	if (mShape)
	{
		delete mShape;
		mShape = NULL;
	}

	// Stop here if the file name is set to nothing
	if (!file || !*file)
		return;

	// Load the new instance
	mShape = new TSShapeInstance(ResourceManager->load(mShapeName), true);

	// Init threads
	for (S32 i = 0; i < MaxScriptThreads; i++)
	{
		mScriptThread[i].sequence = -1;
		mScriptThread[i].thread   = 0;
		mScriptThread[i].sound    = 0;
		mScriptThread[i].state    = Thread::Stop;
		mScriptThread[i].atEnd    = false;
		mScriptThread[i].forward  = true;
	}
}

void GuiShapeCtrl::renderWorld(const RectI& updateRect)
{
	if (!mShape)
		return;

	advanceThreads(TickSec);

	// RENDER CODE HERE
	mShape->setEnvironmentMapOn(false, 1);
	mShape->setAlphaAlways(0.f);

	TSShapeInstance::smNoRenderNonTranslucent = false;
	TSShapeInstance::smNoRenderTranslucent    = true;

	mShape->setupFog(0.f, ColorI(255, 255, 255, 255));
	mShape->animate();
	mShape->render();

	TSShapeInstance::smNoRenderNonTranslucent = false;
	TSShapeInstance::smNoRenderTranslucent    = false;
	TextureManager::setSmallTexturesActive(false);
}

ConsoleMethod(GuiShapeCtrl, setNodeColor, void, 4, 4, "(string nodeName, string color)")
{
	if (!object->mShape)
		return;

	S32 r = 0;
	S32 g = 0;
	S32 b = 0;
	S32 a = 255;
	dSscanf(argv[3], "%f %f %f %f", &r, &g, &b, &a);

	object->mShape->setNodeColor(dAtoi(argv[2]), ColorI(r, g, b, a));
	object->mShape->updateNodeColors();
}

ConsoleMethod(GuiShapeCtrl, playThread, bool, 3, 4, "(int slot, string sequenceName)")
{
	U32 slot = dAtoi(argv[2]);
	if (slot >= 0 && slot < MaxScriptThreads)
	{
		if (argc == 4)
		{
			if (object->mShape)
			{
				S32 seq = object->mShape->getShape()->findSequence(argv[3]);
				if (seq != -1 && object->setThreadSequence(slot, seq))
					return true;
			}
		}
		else
			if (object->playThread(slot))
				return true;
	}

	return false;
}

ConsoleMethod(GuiShapeCtrl, stopThread, bool, 3, 3, "(int slot)")
{
	int slot = dAtoi(argv[2]);
	if (slot >= 0 && slot < MaxScriptThreads)
	{
		if (object->stopThread(slot))
			return true;
	}

	return false;
}