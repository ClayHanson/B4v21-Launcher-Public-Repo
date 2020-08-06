#define DONT_DEFINE_NEW
#define _ALLOW_KEYWORD_MACROS

#include "console/console.h"
#include "core/stringTable.h"
#include "../lib/pcre/pcre.h"

ConsoleFunction(strMatch, bool, 3, 5, "(string pattern, string haystack[, string outputVariableName[, int offset]])\nExample: strMatch(\"/([A-Z])\\w+/mi\", \"Regex is awesome!\", matches);")
{
	// Get the arguments
	const char* pPattern = argv[1];
	const char* pHaystack = argv[2];
	const char* pOutputName = (argc >= 4 ? (*argv[3] == 0 ? NULL : StringTable->insert(argv[3])) : NULL);
	int iOffset = (argc >= 5 ? dAtoi(argv[4]) : 0);
	int iFlags = 0;
	int iMatchData[30];
	pcre* rExpression;

	// Sanitize the pattern string
	if (dStrchr(pPattern, '/') != pPattern || dStrrchr(pPattern, '/') == NULL || dStrrchr(pPattern, '/') == pPattern)
	{
		// This isn't formatted correctly...
		Con::errorf("ERROR: strMatch() - Pattern is formatted incorrectly. Should be \"/regex_here/flags_here\" (ex. \"/([A-Z])\\w+/mi\")");
		return false;
	}

	// Scan the flags
	for (const char* ptr = dStrrchr(pPattern, '/') + 1; *ptr != 0; ptr++)
	{
		if (*ptr == 'x')
			iFlags |= PCRE_EXTENDED;
		else if (*ptr == 'i')
			iFlags |= PCRE_CASELESS;
		else if (*ptr == 'm')
			iFlags |= PCRE_MULTILINE;
		else if (*ptr == 's')
			iFlags |= PCRE_DOTALL;
		else if (*ptr == 'U')
			iFlags |= PCRE_UNGREEDY;
		else if (*ptr == 'g')
			Con::warnf("WARNING: strMatch() - Flag 'g' is implied; it does not need to be included.");
		else
		{
			Con::errorf("ERROR: strMatch() - Unknown flag '%.1s'!", ptr);
			return false;
		}
	}

	// Populate the pattern buffer
	char pzPatternBuffer[1024];
	dStrcpy(pzPatternBuffer, pPattern + 1);
	*dStrrchr(pzPatternBuffer, '/') = 0;

	// Get the regex ready
	const char* pzErrorMessage = NULL;
	int iErrOffset = 0;
	if ((rExpression = pcre_compile(pzPatternBuffer, iFlags, &pzErrorMessage, &iErrOffset, NULL)) == NULL)
	{
		Con::errorf("ERROR: strMatch() - Syntax error in regex!\r\n%s\r\n", pzErrorMessage);
		return false;
	}

	// Get the result of the match
	int iMatchCount = pcre_exec(rExpression, NULL, pHaystack, dStrlen(pHaystack), iOffset, 0, iMatchData, sizeof(iMatchData) / sizeof(int));

	// We're done with the expression
	pcre_free(rExpression);

	// If there was no variable output name, then we're done!
	if (pOutputName == NULL)
		return iMatchCount > 0;

	// Setup the buffers
	char pzLocalName[1024];
	char pzValue[1024];
	int iMatchNo = 0;

	// Sanitize the output name
	if (dStrlen(pOutputName) <= 0 || dStrlen(pOutputName) >= 256)
	{
		Con::warnf("WARNING: strMatch() - Variable output name \"%s\" is %s! Using default name 'matches'.", pOutputName, (dStrlen(pOutputName) <= 0 ? "too short" : "too long"));
		pOutputName = "matches";
	}

	// Start populating local variables w/ matches
	for (int i = 0; i < iMatchCount; i++)
	{
		int iStart = iMatchData[2 * i];
		int iEnd = iMatchData[2 * i + 1];

		// Ignore invalid matches
		if (iStart == iEnd)
			continue;

		// Build the variable name
		dSprintf(pzLocalName, sizeof(pzLocalName), "%s%d", pOutputName, iMatchNo++);

		// Build the value
		*pzValue = 0;
		dStrncat(pzValue, pHaystack + iStart, (((iEnd - iStart) >= (sizeof(pzValue) - 1)) ? sizeof(pzValue) - 1 : iEnd - iStart));

		// Set the local variable
		Con::setLocalVariable(pzLocalName, avar("%d\t%d\t%s", iStart, iEnd, StringTable->insert(pzValue)));
	}

	// Set the count variable
	dSprintf(pzLocalName, sizeof(pzLocalName), "%sCount", pOutputName);
	Con::setLocalVariable(pzLocalName, avar("%d", iMatchNo));

	// All done!
	return iMatchCount > 0;
}

ConsoleFunction(strMatchAll, bool, 4, 5, "(string pattern, string haystack, string outputVariableName[, int offset])\nExample: strMatchAll(\"/([A-Z])\\w+/mi\", \"Regex is awesome!\", matches);")
{
	// Get the arguments
	const char* pPattern    = argv[1];
	const char* pHaystack   = argv[2];
	const char* pOutputName = StringTable->insert(argv[3]);
	int iOffset             = (argc >= 5 ? dAtoi(argv[4]) : 0);
	int iFlags              = 0;
	int iMatchData[30];
	pcre* rExpression;

	// If there was no variable output name, then we're done!
	if (*pOutputName == 0 || pOutputName == NULL)
	{
		Con::errorf("ERROR: strMatchAll() - outputVariableName MUST be provided!");
		return false;
	}

	// Sanitize the pattern string
	if (dStrchr(pPattern, '/') != pPattern || dStrrchr(pPattern, '/') == NULL || dStrrchr(pPattern, '/') == pPattern)
	{
		// This isn't formatted correctly...
		Con::errorf("ERROR: strMatchAll() - Pattern is formatted incorrectly. Should be \"/regex_here/flags_here\" (ex. \"/([A-Z])\\w+/mi\")");
		return false;
	}

	// Scan the flags
	for (const char* ptr = dStrrchr(pPattern, '/') + 1; *ptr != 0; ptr++)
	{
		if (*ptr == 'x')
			iFlags |= PCRE_EXTENDED;
		else if (*ptr == 'i')
			iFlags |= PCRE_CASELESS;
		else if (*ptr == 'm')
			iFlags |= PCRE_MULTILINE;
		else if (*ptr == 's')
			iFlags |= PCRE_DOTALL;
		else if (*ptr == 'U')
			iFlags |= PCRE_UNGREEDY;
		else if (*ptr == 'g')
			Con::warnf("WARNING: strMatchAll() - Flag 'g' is implied; it does not need to be included.");
		else
		{
			Con::errorf("ERROR: strMatchAll() - Unknown flag '%.1s'!", ptr);
			return false;
		}
	}

	// Populate the pattern buffer
	char pzPatternBuffer[1024];
	dStrcpy(pzPatternBuffer, pPattern + 1);
	*dStrrchr(pzPatternBuffer, '/') = 0;

	// Get the regex ready
	const char* pzErrorMessage = NULL;
	int iErrOffset             = 0;
	if ((rExpression = pcre_compile(pzPatternBuffer, iFlags, &pzErrorMessage, &iErrOffset, NULL)) == NULL)
	{
		Con::errorf("ERROR: strMatchAll() - Syntax error in regex!\r\n%s\r\n", pzErrorMessage);
		return false;
	}

	// Sanitize the output name
	if (dStrlen(pOutputName) <= 0 || dStrlen(pOutputName) >= 256)
	{
		Con::warnf("WARNING: strMatchAll() - Variable output name \"%s\" is %s! Using default name 'matches'.", pOutputName, (dStrlen(pOutputName) <= 0 ? "too short" : "too long"));
		pOutputName = "matches";
	}

	// Store the current match #
	int iCurrentMatch = -1;
	char pzLocalName[1024];
	char pzValue[1024];

	// Loop until no matches are found
	while (true)
	{
		// Get the result of the match
		int iMatchCount = pcre_exec(rExpression, NULL, pHaystack, dStrlen(pHaystack), iOffset, 0, iMatchData, sizeof(iMatchData) / sizeof(int));

		// We didn't get any matches; Stop here.
		if (iMatchCount <= 0)
			break;

		// Setup the variables
		int iMatchNo = 0;

		// Increment the current match #
		iCurrentMatch++;

		// Start populating local variables w/ matches
		for (int i = 0; i < iMatchCount; i++)
		{
			int iStart = iMatchData[2 * i];
			int iEnd = iMatchData[2 * i + 1];

			// Ignore invalid matches
			if (iStart == iEnd)
				continue;

			// Build the variable name
			dSprintf(pzLocalName, sizeof(pzLocalName), "%s%d_%d", pOutputName, iCurrentMatch, iMatchNo++);

			// Build the value
			*pzValue = 0;
			dStrncat(pzValue, pHaystack + iStart, (((iEnd - iStart) >= (sizeof(pzValue) - 1)) ? sizeof(pzValue) - 1 : iEnd - iStart));

			// Set the local variable
			Con::setLocalVariable(pzLocalName, avar("%d\t%d\t%s", iStart, iEnd, StringTable->insert(pzValue)));
		}

		// Set the count variable
		dSprintf(pzLocalName, sizeof(pzLocalName), "%sCount%d", pOutputName, iCurrentMatch);
		Con::setLocalVariable(pzLocalName, avar("%d", iMatchNo));

		// Increment offset
		iOffset = iMatchData[1];
	}

	// Overall count
	dSprintf(pzLocalName, sizeof(pzLocalName), "%sCount", pOutputName);
	Con::setLocalVariable(pzLocalName, avar("%d", iCurrentMatch + 1));

	// We're done with the expression
	pcre_free(rExpression);

	// Done!
	return (iCurrentMatch > -1);
}