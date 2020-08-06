#ifndef URL_STRUCT_INFO_H_
#define URL_STRUCT_INFO_H_

#include "console/consoleInternal.h"

class UrlStructInfo {
private:
	S16   port;
	char* entireURL;
	char* portStr;
	char* hostName;
	char* path;

public:
	UrlStructInfo();
	~UrlStructInfo();

	const char* getHostName();
	const char* getPath();
	const char* getPort();
	S16 getPortNum();

	bool getPathFileName(char* buffer);
	bool getPathFileExt(char* buffer);
	bool setURL(const char* url);

	const char* GetURL();
};

#endif