#include "platform/gameInterface.h"
#include "core/resManager.h"
#include "console/simBase.h"
#include "core/resizeStream.h"

static bool alreadyExtracting = false;

ConsoleFunction(isZipValid, bool, 2, 2, "(path)")
{
	ZipAggregate zipAggregate;
	if (zipAggregate.openAggregate(argv[1]) == false)
		return false;

	zipAggregate.closeAggregate();
	return true;
}

ConsoleFunction(exportZip, bool, 3, 7, "(path, savePath[, ignoreFirstZipPath[, deleteAfterDone[, progressCallbackFuncName[, doNotOverwriteList]]]]) - ignoreFirstZipPath is useful for github files that store the files a folder into the zip")
{
	if (alreadyExtracting)
	{
		Con::errorf("exportZip() - Cannot run exportZip while it's already running.");
		return false;
	}

	char arg_path[1024];
	char arg_savePath[1024];

	dStrcpy(arg_path, argv[1]);
	dStrcpy(arg_savePath, argv[2]);

	if (arg_savePath[dStrlen(arg_savePath) - 1] != '/' && arg_savePath[dStrlen(arg_savePath) - 1] != '\\')
		dStrcat(arg_savePath, "/");

	ZipAggregate zipAggregate;
	if (zipAggregate.openAggregate(arg_path) == false)
	{
		Con::errorf("Error opening zip (%s), need to handle this better...", arg_path);
		return false;
	}

	alreadyExtracting = true;

	Con::printf("Extracting %s to %s...", arg_path, arg_savePath);
	StringTableEntry progressCallBackFunc = (argc >= 6 ? StringTable->insert(argv[5]) : NULL);
	const char* OverwriteProtectionList = (argc >= 7 ? argv[6] : NULL);
	bool deleteAfterDone = (argc >= 5 ? argv[4] : false);
	bool ignoreFirstZipPath = (argc >= 4 ? dAtob(argv[3]) : false);
	bool isWindow = Sim::findObject("Canvas") != NULL;

	// Create the exclusion list
	Vector<char*> NoOverwriteList;

	if (OverwriteProtectionList != NULL)
	{
		const char* start = OverwriteProtectionList;
		for (const char* ptr = start; ; ptr++)
		{
			if (*ptr == 0 || *ptr == '\t')
			{
				char* NewString = new char[(ptr - start) + 1];
				*NewString = 0;
				dStrncat(NewString, start, ptr - start);
				NoOverwriteList.push_back(NewString);

				start = ptr + 1;
				if (*ptr == 0)
					break;
			}
		}
	}

	// Get file base
	char fileBase[1024];
	const char* path = dStrrchr(arg_path, '/');
	if (!path)
		path = arg_path;
	else path++;

	dStrcpy(fileBase, path);
	char* ext = dStrrchr(fileBase, '.');
	if (ext)
		* ext = 0;

	dStrcat(fileBase, "/");

	extern GameInterface* Game;

	FileStream* diskStream;
	ZipAggregate::iterator itr;
	for (itr = zipAggregate.begin(); itr != zipAggregate.end(); itr++)
	{
		const ZipAggregate::FileEntry& rEntry = *itr;
		const char* zip_path = dStrstr((const char*)rEntry.pPath, fileBase) + dStrlen(fileBase);

		if (dStrstr((const char*)rEntry.pPath, fileBase) == NULL)
			zip_path = "";

		if (ignoreFirstZipPath)
		{
			if (dStrchr(zip_path, '/')) zip_path = dStrchr(zip_path, '/') + 1;
			else zip_path = rEntry.pPath + dStrlen(rEntry.pPath);
		}

		// Determine the output path
		char outPath[1024];
		dSprintf(outPath, 1024, "%s%s/", arg_savePath, zip_path);

		// Determine the zipped file's path
		char zipFilePath[1024];
		dSprintf(zipFilePath, 1024, "%s%s", rEntry.pPath, rEntry.pFileName);

		bool overwriteProtected = false;
		for (Vector<char*>::iterator it = NoOverwriteList.begin(); it != NoOverwriteList.end(); it++)
		{
			char* fileName = *it;
			int len = dStrlen(fileName);

			if (!dStrstr(rEntry.pFileName, fileName))
				continue;

			overwriteProtected = true;
			break;
		}

		// Create the output path if it doesn't already exist
		if (!Platform::isDirectory(outPath))
			Platform::createPath(outPath);

		// Append the zipfile's name to the output path
		dStrcat(outPath, rEntry.pFileName);

		if (overwriteProtected && Platform::isFile(outPath))
		{
			Con::printf("Didn't overwrite \"%s\" because it already exists & is in the overwrite protection list!", outPath);
			continue;
		}

		if (progressCallBackFunc)
		{
			char curr[64];
			char max[64];

			dSprintf(curr, 64, "%d", itr - zipAggregate.begin());
			dSprintf(max, 64, "%d", zipAggregate.end() - zipAggregate.begin());

			Con::executef(3, progressCallBackFunc, curr, max);
		}

		if (isWindow && ((itr - zipAggregate.begin()) % 4) == 0)
		{
			// Do a tick whilst we wait for this
			Net::process();
			Platform::process();
			TimeManager::process();
		}

		// ---------------------------------------------------------------------------------------

		Stream* stream = NULL;
		diskStream = new FileStream;

		// Open zip file for read
		if (!diskStream->open(arg_path, FileStream::Read))
		{
			Con::printf("exportZip() - Failed to open zipfile \"%s\"", arg_path);
			delete stream;
			alreadyExtracting = false;
			break;
		}

		diskStream->setPosition(rEntry.fileOffset);

		ZipLocalFileHeader zlfHeader;
		if (zlfHeader.readFromStream(*diskStream) == false)
		{
			Con::errorf("exportZip() - '%s' is somehow not in the zip!", rEntry.pFileName);
			diskStream->close();
			continue;
		}

		if (zlfHeader.m_header.compressionMethod == ZipLocalFileHeader::Stored || rEntry.fileSize == 0)
		{
			// Just read straight from the stream...
			ResizeFilterStream* strm = new ResizeFilterStream;
			strm->attachStream(diskStream);
			strm->setStreamOffset(diskStream->getPosition(), rEntry.fileSize);
			stream = strm;
		}
		else
		{
			if (zlfHeader.m_header.compressionMethod == ZipLocalFileHeader::Deflated)
			{
				ZipSubRStream* zipStream = new ZipSubRStream;
				zipStream->attachStream(diskStream);
				zipStream->setUncompressedSize(rEntry.fileSize);
				stream = zipStream;
			}
			else
			{
				Con::errorf("exportZip() - '%s' Compressed inappropriately in the zip!", rEntry.pFileName);
				diskStream->close();
				continue;
			}
		}

		if (Platform::isFile(outPath))
			dFileDelete(outPath);

		FileStream out;
		if (!ResourceManager->openFileForWrite(out, outPath, FileStream::Write, true))
		{
			Con::errorf("exportZip() - Unable to open output file '%s' for writing.", outPath);
			ResourceManager->closeStream(stream);
			continue;
		}

#define CHUNK_SIZE 2048

		// Write contents
		U32 leftOver = rEntry.fileSize;
		while (leftOver)
		{
			char chunk[CHUNK_SIZE + 1];

			U32 len = (leftOver >= CHUNK_SIZE ? CHUNK_SIZE : leftOver);

			stream->read(len, &chunk);
			out.write(len, chunk);

			leftOver -= len;
		}

		// Cleanup
		out.close();
		ResourceManager->closeStream(stream);
	}

	// Free exclusion list
	for (Vector<char*>::iterator it = NoOverwriteList.begin(); it != NoOverwriteList.end(); it++)
	{
		char* str = *it;
		delete[] str;
	}

	if (isWindow)
		Platform::setWindowTitle("B4v21 Launcher");

	zipAggregate.closeAggregate();
	if (deleteAfterDone)
		dFileDelete(arg_path);

	alreadyExtracting = false;
	return true;
}