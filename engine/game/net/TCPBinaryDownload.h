#ifndef _TCP_BINARY_DOWNLOAD_H_
#define _TCP_BINARY_DOWNLOAD_H_

#include "console/consoleInternal.h"
#include "game/net/URLStructInfo.h"

typedef S32 DownloadTCPID;

/// Returns a download ID
DownloadTCPID downloadTCPFile(const char* hostName, const char* port, const char* path, const char* savePath, const char* postString,
	StringTableEntry onDownloadStartFuncName,
	StringTableEntry onCompleteFuncName,
	StringTableEntry onProgressFuncName,
	void(*onDownloadStartFunc)(void* userData, DownloadTCPID) = NULL,
	void(*onCompleteFunc)(void* userData, const char* fName, S32 response) = NULL,
	void(*onProgressFunc)(void* userData, S32 curr, S32 max) = NULL,
	void* userData = NULL);

bool cancelTCPFileDownload(DownloadTCPID id);

#endif