#include "dgl/dgl.h"
#include "platform/platform.h"
#include "core/dnet.h"
#include "audio/audioDataBlock.h"
#include "game/gameConnection.h"
#include "game/moveManager.h"
#include "console/consoleTypes.h"
#include "core/bitStream.h"
#include "ts/tsPartInstance.h"
#include "ts/tsShapeInstance.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "game/shadow.h"
#include "game/fx/explosion.h"
#include "game/shapeBase.h"
#include "terrain/waterBlock.h"
#include "game/debris.h"
#include "terrain/sky.h"
#include "game/physicalZone.h"
#include "sceneGraph/detailManager.h"
#include "math/mathUtils.h"
#include "math/mMatrix.h"
#include "math/mRandom.h"
#include "platform/profiler.h"
#include "game/fileDownloadObject.h"

#define MAX_FILEBUFFER_SIZE 32768

IMPLEMENT_CO_NETOBJECT_V1(fileGhoster);

fileGhoster::fileGhoster() {
	offset   = 0;
}

fileGhoster::~fileGhoster() {
	for (Vector<GhostedFile*>::iterator it = files.begin(); it != files.end(); it++) {
		GhostedFile* file = *it;

		for (Vector<char*>::iterator bit = fileBuffers.begin(); bit != fileBuffers.end(); bit++) {
			char* buffer = *it;
			delete[] buffer;
		}
	}

	if (fileName != NULL) delete[] fileName;
}

void fileGhoster::initPersistFields() {
	Parent::initPersistFields();
}

U32 fileGhoster::packUpdate(NetConnection *conn, U32 mask, BitStream *stream) {
	Con::errorf("packUpdate();");
	return 0;
}

void fileGhoster::unpackUpdate(NetConnection *conn, BitStream *stream) {
	Con::errorf("unpackUpdate();");
}