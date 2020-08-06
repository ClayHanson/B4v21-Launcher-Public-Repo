#include "URLStructInfo.h"

UrlStructInfo::UrlStructInfo()
{
	port      = 80;
	portStr   = NULL;
	hostName  = NULL;
	entireURL = NULL;
	path      = NULL;
}

UrlStructInfo::~UrlStructInfo()
{
	if (portStr != NULL)   delete[] portStr;
	if (hostName != NULL)  delete[] hostName;
	if (entireURL != NULL) delete[] entireURL;
	if (path != NULL)      delete[] path;
}

bool UrlStructInfo::setURL(const char* url)
{
	if (dStrstr(url, "http://") != url && dStrstr(url, "https://") != url) return false;
	const char* nPtr = url;
	const char* t_port = "80";
	char host[1024];

	if (dStrstr(url, "http://") == url) nPtr += 7;
	else if (dStrstr(url, "https://") == url)
	{
		t_port = "443";
		nPtr  += 8;
	}

	const char* pathStart = dStrchr(nPtr, '/');
	char t_path[1024];
	if (pathStart)
	{
		dStrcpy(host, nPtr);
		*(host + (pathStart - nPtr)) = 0;
	}
	else dStrcpy(host, nPtr);

	dStrcpy(t_path, (pathStart ? pathStart : ""));

	// Get the filename
	const char* t_name = dStrrchr((const char*)pathStart + 2, '/'); if (!t_name) t_name = t_path; else t_name++;

	// Save values
	if (portStr != NULL)   delete[] portStr;
	if (hostName != NULL)  delete[] hostName;
	if (entireURL != NULL) delete[] entireURL;
	if (path != NULL)      delete[] path;

	portStr   = new char[dStrlen(t_port) + 1];
	hostName  = new char[dStrlen(host) + 1];
	entireURL = new char[dStrlen(url) + 1];
	path      = new char[dStrlen(t_path) + 1];
	port      = dAtoi(t_port);

	dStrcpy(portStr, t_port);
	dStrcpy(hostName, host);
	dStrcpy(entireURL, url);
	dStrcpy(path, t_path);

	return true;
}

const char* UrlStructInfo::getHostName()
{
	return hostName;
}

const char* UrlStructInfo::getPath()
{
	return path;
}

const char* UrlStructInfo::getPort()
{
	return portStr;
}

S16 UrlStructInfo::getPortNum()
{
	return port;
}

bool UrlStructInfo::getPathFileName(char* buffer)
{
	if (path == NULL)
		return false;

	const char* name = dStrrchr(path, '/');
	if (!name) name = path;
	else name++;

	// Get the full valid name
	for (const char* end = name; *end != 0; end++)
	{
		if (!dStrchr("\\/:*?\"<>|", *end))
			continue;

		*buffer = 0;
		dStrncat(buffer, name, end - name);
		return true;
	}

	dStrcpy(buffer, name);
	return true;
}

bool UrlStructInfo::getPathFileExt(char* buffer)
{
	if (path == NULL)
		return false;

	const char* ret = dStrrchr(path, '.');
	if (!ret)
	{
		*buffer = 0;
		return false;
	}

	dStrcpy(buffer, ret);
	return true;
}

const char* UrlStructInfo::GetURL()
{
	return entireURL;
}