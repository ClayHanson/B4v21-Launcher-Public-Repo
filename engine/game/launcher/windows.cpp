#include "core/resManager.h"
#include "console/console.h"
#include <Windows.h>

#define MSGBOX_FLAG(f) { #f, f }

WCHAR* CharToWide(const char* text)
{
	int nChars = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	WCHAR* ret = new WCHAR[nChars];

	MultiByteToWideChar(CP_ACP, 0, text, -1, (LPWSTR)ret, nChars);

	return ret;
}

ConsoleFunction(realMessageBox, const char*, 3, 4, "(string title, string text, string flags = OK)")
{
	const char* tags = (argc == 3 ? "OK" : argv[3]);
	struct
	{
		const char* FlagName;
		int Flag;
	} flags[] =
	{
		MSGBOX_FLAG(MB_ABORTRETRYIGNORE),
		MSGBOX_FLAG(MB_CANCELTRYCONTINUE),
		MSGBOX_FLAG(MB_HELP),
		MSGBOX_FLAG(MB_OK),
		MSGBOX_FLAG(MB_OKCANCEL),
		MSGBOX_FLAG(MB_RETRYCANCEL),
		MSGBOX_FLAG(MB_YESNO),
		MSGBOX_FLAG(MB_YESNOCANCEL),

		MSGBOX_FLAG(MB_ICONEXCLAMATION),
		MSGBOX_FLAG(MB_ICONWARNING),
		MSGBOX_FLAG(MB_ICONINFORMATION),
		MSGBOX_FLAG(MB_ICONASTERISK),
		MSGBOX_FLAG(MB_ICONQUESTION),
		MSGBOX_FLAG(MB_ICONSTOP),
		MSGBOX_FLAG(MB_ICONERROR),
		MSGBOX_FLAG(MB_ICONHAND),

		MSGBOX_FLAG(MB_DEFBUTTON1),
		MSGBOX_FLAG(MB_DEFBUTTON2),
		MSGBOX_FLAG(MB_DEFBUTTON3),
		MSGBOX_FLAG(MB_DEFBUTTON4)
	};

	// Parse tags
	const char* start = tags;
	long Flags = 0;
	for (const char* ptr = tags; ; ptr++)
	{
		if (*ptr == 0 || *ptr == ' ')
		{
			char str[128];
			*str = 0;
			dStrncat(str, start, ptr - start);

			for (int i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
			{
				if (dStricmp(str, flags[i].FlagName + 3))
					continue;

				Flags |= flags[i].Flag;
				break;
			}

			start = ptr + 1;
			if (*ptr == 0)
				break;
		}
	}

	int result = MessageBoxA(NULL, argv[2], argv[1], Flags);
	switch (result)
	{
		case IDABORT:
			return "ABORT";
		case IDCANCEL:
			return "CANCEL";
		case IDCONTINUE:
			return "CONTINUE";
		case IDIGNORE:
			return "IGNORE";
		case IDNO:
			return "NO";
		case IDOK:
			return "OK";
		case IDRETRY:
			return "RETRY";
		case IDTRYAGAIN:
			return "TRYAGAIN";
		case IDYES:
			return "YES";
		default:
			return "";
	}
}

ConsoleFunction(openInExplorer, void, 2, 2, "(Path)")
{
	ShellExecuteA(NULL, "open", argv[1], NULL, NULL, SW_SHOWDEFAULT);
}

ConsoleFunction(launchExe, void, 3, 3, "(executable, args)")
{
	// additional information
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	// set the size of the structures
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Create argument list
	char args[1024];
	dSprintf(args, 1024, "\"%s\" %s", argv[1], argv[2]);

	// Convert to wchar
	unsigned short* a = new unsigned short[dStrlen(args) + 1];
	unsigned short* c = new unsigned short[dStrlen(argv[1]) + 1];
	unsigned short* b = new unsigned short[dStrlen(argv[1]) + 1];
	unsigned short* ptr = c;
	for (const char* ptr_ = argv[1]; *ptr_ != 0; ptr_++)
		* ptr++ = *ptr_;

	*ptr++ = 0;
	ptr    = b;

	for (const char* ptr_ = argv[1]; *ptr_ != 0; ptr_++)
	{
		if (dStrchr(ptr_, '/') == NULL && dStrchr(ptr_, '\\') == NULL) break;
		*ptr++ = *ptr_;
	}

	*ptr++ = 0;
	ptr    = a;
	for (const char* ptr_ = args; *ptr_ != 0; ptr_++)
		* ptr++ = *ptr_;

	*ptr++ = 0;

	// Start the program
	CreateProcess((wchar_t*)c, (wchar_t*)a, NULL, NULL, FALSE, 0, NULL, (wchar_t*)b, &si, &pi);

	// Free the buffers
	delete[] a;
	delete[] b;
	delete[] c;
}

ConsoleFunction(getMyDocumentsPath, const char*, 1, 1, "Get the path to 'My Documents'.")
{
	HKEY regKey;
	char* returnString = NULL;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders", 0, KEY_READ, &regKey) == ERROR_SUCCESS)
	{
		char buf[MAX_PATH];
		DWORD size = sizeof(buf);
		RegQueryValueEx(regKey, dT("Personal"), NULL, NULL, (U8*)buf, &size);

		// Fix wide char
		char* ptr = buf;
		for (S32 i = 0; i < MAX_PATH; i += 2)
			* ptr++ = *(buf + i);

		returnString = Con::getReturnBuffer(size + 1);
		dStrcpy(returnString, (const char*)buf);

		RegCloseKey(regKey);
	}

	// Fix retarded windows userprofile token
	if (dStrstr((const char*)returnString, "%USERPROFILE%") == returnString)
	{
		if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Volatile Environment", 0, KEY_READ, &regKey) == ERROR_SUCCESS)
		{
			char buf[1024];
			DWORD size = sizeof(buf);
			RegQueryValueEx(regKey, dT("USERPROFILE"), NULL, NULL, (U8*)buf, &size);

			char* ptr = buf;
			for (S32 i = 0; i < MAX_PATH; i += 2)
				* ptr++ = *(buf + i);

			char newRet[1024];
			dStrcpy(newRet, buf);
			dStrcat(newRet, returnString + dStrlen("%USERPROFILE%"));
			returnString = Con::getReturnBuffer(dStrlen(newRet) + 1);
			dStrcpy(returnString, newRet);

			RegCloseKey(regKey);
		}
	}

	return returnString;
}