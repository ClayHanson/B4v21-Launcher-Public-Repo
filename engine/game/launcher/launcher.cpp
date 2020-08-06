#include "game/net/TCPBinaryDownload.h"
#include "core/resManager.h"
#include "core/resizeStream.h"
#include "gui/core/guiCanvas.h"
#include "platform/gameInterface.h"
#include "core/findMatch.h"
#include "sim/netConnection.h"
#include "sim/netInterface.h"

ConsoleFunction(tcpDownload, S32, 4, 9, "(savePath, host, path[, port[, onCompleteCallback[, onProgressCallback[, onDownloadStartCallback[, postString]]]]]) - Returns a download ID")
{
	S32 port = argc >= 5 ? dAtoi(argv[4]) : 80;

	char port_string[128];
	dSprintf(port_string, 128, "%d", port);

	return downloadTCPFile(argv[2],
		port_string, // Port
		argv[3], // URL path
		argv[1], // savePath
		(argc >= 9 ? argv[8] : "\r\n"), // postString
		StringTable->insert(argc >= 8 ? argv[7] : ""), // onDownloadStart()
		StringTable->insert(argc >= 6 ? argv[5] : ""), // onDownloadComplete(savedFileName)
		StringTable->insert(argc >= 7 ? argv[6] : "")); // onDownloadProgress(curr, max)
}

ConsoleFunction(tcpCancelDownload, bool, 2, 2, "(downloadId) - Cancel a queue'd download")
{
	return cancelTCPFileDownload(dAtoi(argv[1]));
}

ConsoleFunction(getLauncherVersion, const char*, 1, 1, "Get the current B4v21 Launcher version.")
{
	return "1.1.7"
#ifdef TORQUE_DEBUG
		" (DEBUG BUILD)"
#endif
		;
}

ConsoleFunction(isDebugMode, bool, 1, 1, "")
{
#ifdef TORQUE_DEBUG
	return true;
#else
	return false;
#endif
}