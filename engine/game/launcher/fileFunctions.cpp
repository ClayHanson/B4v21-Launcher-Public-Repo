#include "core/fileStream.h"
#include "console/console.h"
#include "core/findMatch.h"
#include "core/resManager.h"
#include "core/tVector.h"

#define GET_ADDR(type, off) *((type*)&buffer[off])

static Vector<Platform::FileInfo>* fileList = NULL;
static Vector<Platform::FileInfo>* findList = NULL;
static S32 fileListIdx                      = 0;

ConsoleFunction(findFirstAllFiles, const char*, 3, 3, "(path, pattern)")
{
	char path[512];
	const char* pattern = argv[2];

	dStrcpy(path, argv[1]);
	if (path[dStrlen(path) - 1] == '\\' || path[dStrlen(path) - 1] == '/')
		path[dStrlen(path) - 1] = 0;

	if (findList == NULL)
		findList = new Vector<Platform::FileInfo>;

	Vector<Platform::FileInfo> tmpList;

	// Dump the path
	if (!Platform::dumpPath(path, tmpList, -1, pattern) || tmpList.size() == 0)
		return "";

	findList->clear();

	// Get valid files
	for (Vector<Platform::FileInfo>::iterator it = tmpList.begin(); it != tmpList.end(); it++)
	{
		Platform::FileInfo fInfo = *it;
		const char* ext = dStrrchr(fInfo.pFileName, '.');

		if (ext == NULL || dStricmp(ext, ".exe") || !FindMatch::isMatch(pattern, fInfo.pFileName, false))
			continue;

		findList->push_back(fInfo);
	}

	// Create the return buffer
	char* buff = Con::getReturnBuffer(dStrlen(findList->front().pFullPath) + dStrlen(findList->front().pFileName) + 2);
	dStrcpy(buff, findList->front().pFullPath);
	dStrcat(buff, "/");
	dStrcat(buff, findList->front().pFileName);

	// Remove the first entry from the list
	findList->pop_front();

	// Return it
	return buff;
}

ConsoleFunction(findNextAllFiles, const char*, 1, 1, "()")
{
	if (!findList || findList->size() == 0)
		return "";

	// Create the return buffer
	char* buff = Con::getReturnBuffer(dStrlen(findList->front().pFullPath) + dStrlen(findList->front().pFileName) + 2);
	dStrcpy(buff, findList->front().pFullPath);
	dStrcat(buff, "/");
	dStrcat(buff, findList->front().pFileName);

	// Remove the first entry from the list
	findList->pop_front();

	// Return it
	return buff;
}

ConsoleFunction(getFirstDirectoryFile, const char*, 2, 2, "(path)")
{
	if (fileList == NULL)
		fileList = new Vector<Platform::FileInfo>;

	char path[1024];
	dStrcpy(path, argv[1]);
	if (*(path + (dStrlen(path) - 1)) == '\\' || *(path + (dStrlen(path) - 1)) == '/')
		* (path + (dStrlen(path) - 1)) = 0;

	if (!Platform::isDirectory(path))
	{
		Con::errorf("getFirstDirectoryFile() - Path \"%s\" does not exist.", path);
		return "";
	}

	if (!Platform::dumpPath(path, *fileList))
		return "";

	if (fileList->empty())
		return "";

	char fName[1024];
	dSprintf(fName, 1024, "%s/%s", fileList->first().pFullPath, fileList->first().pFileName);

	fileList->pop_front();

	char* ret = Con::getReturnBuffer(dStrlen(fName) + 1);
	dStrcpy(ret, fName);

	return ret;
}

ConsoleFunction(getNextDirectoryFile, const char*, 1, 1, "()")
{
	if (fileList == NULL)
		return "";

	if (fileListIdx >= fileList->size())
	{
		if (fileList != NULL)
		{
			delete fileList;
			fileList = NULL;
		}

		return "";
	}

	char fName[1024];
	dSprintf(fName, 1024, "%s/%s", fileList->first().pFullPath, fileList->first().pFileName);

	fileList->pop_front();

	char* ret = Con::getReturnBuffer(dStrlen(fName) + 1);
	dStrcpy(ret, fName);

	return ret;
}

ConsoleFunction(getDirectoryFileCount, S32, 2, 3, "(string path, recursiveDepth = -1)returns the number of files in the given directory")
{
	char path[1024];
	dStrcpy(path, argv[1]);
	if (*(path + (dStrlen(path) - 1)) == '\\' || *(path + (dStrlen(path) - 1)) == '/')
		* (path + (dStrlen(path) - 1)) = 0;

	if (!Platform::isDirectory(path))
	{
		Con::errorf("getFirstDirectoryFile() - Path \"%s\" does not exist.", path);
		return 0;
	}

	Vector<Platform::FileInfo> arr;
	Platform::dumpPath(path, arr, (argc == 3 ? dAtoi(argv[2]) : 1));
	return arr.size();
}

ConsoleFunction(getBlocklandVersion, const char*, 2, 2, "(File)")
{
	static struct VINFO {
		const char* Version;
		S32 Value;
		U32 VOffset;
	} VersionInfo[] = {
		{ "1.03", 0x3220206E, 0x0029692E }, // getCompileTimeString = n  2
		{ "8",    0x000004B0, 0x0019FC41 }, // getVersionNumber     = 1200
		{ "9",    0x00000384, 0x0006E6A1 }, // getVersionNumber     = 900
		{ "13",   0x00000514, 0x00072CC1 }, // getVersionNumber     = 1300
		{ "20",   0x000005E7, 0x001CFB11 }, // getBuildNumber       = 1511
		{ "20",   0x322D3442, 0x0032FB78 }, // getCompileTimeString = B4-2
	};

	// Open it
	Stream* s = ResourceManager->openStream(argv[1]);
	if (!s)
	{
		Con::errorf("ERROR: getBlocklandRevision() - Failed to open \"%s\"!", argv[1]);
		return "";
	}

	// Read the entire file into a buffer
	S32 size = ResourceManager->getSize(argv[1]);
	U8* buffer = (U8*)dMalloc(size + 1);
	buffer[size] = 0;
	s->read(size, buffer);
	ResourceManager->closeStream(s);

	// Make sure it's an EXE
	if (size < 2 || buffer[0] != 'M' || buffer[1] != 'Z')
	{
		Con::errorf("ERROR: getBlocklandRevision() - \"%s\" is not a valid Win32 EXE file!", argv[1]);
		dFree(buffer);
		return "";
	}

	// Find the version
	for (S32 i = 0; i < sizeof(VersionInfo) / sizeof(VersionInfo[0]); i++)
	{
		VINFO* ver = &VersionInfo[i];
		if (ver->VOffset >= size || GET_ADDR(S32, ver->VOffset) != ver->Value)
			continue;

		dFree(buffer);
		return ver->Version;
	}

	// We didn't find it. Search for the signature
	// Sig we're looking for: "CC CC CC CC B8 ?? ?? ?? ?? C3 CC CC CC CC"
	for (U32 COffset = 0; COffset < size - 13; COffset++)
	{
		if (buffer[COffset] != 0xCC ||
			buffer[COffset + 1] != 0xCC ||
			buffer[COffset + 2] != 0xCC ||
			buffer[COffset + 3] != 0xCC ||
			buffer[COffset + 4] != 0xB8 ||
			buffer[COffset + 9] != 0xC3 ||
			buffer[COffset + 10] != 0xCC ||
			buffer[COffset + 11] != 0xCC ||
			buffer[COffset + 12] != 0xCC ||
			buffer[COffset + 13] != 0xCC ||
			buffer[COffset + 14] != 0xCC ||
			buffer[COffset + 15] != 0xCC ||
			buffer[COffset + 16] != 0xCC ||
			buffer[COffset + 17] != 0xCC ||
			buffer[COffset + 18] != 0xCC ||
			buffer[COffset + 19] != 0xCC ||
			buffer[COffset + 20] != 0xDD ||
			buffer[COffset + 21] != 0x05 ||
			GET_ADDR(U32, COffset + 5) > 3000)
			continue;

		if (GET_ADDR(U32, COffset + 5) == 1800)
		{
			dFree(buffer);
			return "21";
		}

		break;
	}

	// Free the buffer
	dFree(buffer);

	// Done!
	return "";
}