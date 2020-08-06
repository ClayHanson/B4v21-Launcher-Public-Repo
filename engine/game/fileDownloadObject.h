#ifndef _FILE_DOWNLOAD_OBJECT_H_
#define _FILE_DOWNLOAD_OBJECT_H_

#ifndef _GAMEBASE_H_
#include "game/gameBase.h"
#endif
#ifndef _MATERIALLIST_H_
#include "dgl/materialList.h"
#endif
#ifndef _PLATFORMAUDIO_H_
#include "platform/platformAudio.h"
#endif
#ifndef _MOVEMANAGER_H_
#include "game/moveManager.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _CONVEX_H_
#include "collision/convex.h"
#endif
#ifndef _SCENESTATE_H_
#include "sceneGraph/sceneState.h"
#endif
#ifndef _NETSTRINGTABLE_H_
#include "sim/netStringTable.h"
#endif

struct fileGhoster : public NetObject {
	typedef NetObject Parent;
public:
	enum fileGhosterOperations {
		FILE_BEGIN_INFO   = 0b0001,
		FILE_BEGIN_BUFFER = 0b0010,
		FILE_OP_BITS      = 2
	};

	struct GhostedFile {
		Vector<char*> fileBuffers;
		char* fileName;
	};

protected:
	Vector<GhostedFile*> files;
	unsigned long long offset;

public:
	DECLARE_CONOBJECT(fileGhoster);

	fileGhoster();
	~fileGhoster();

	static void initPersistFields();
	U32 packUpdate(NetConnection *conn, U32 mask, BitStream *stream);
	void unpackUpdate(NetConnection *conn, BitStream *stream);
};

#endif