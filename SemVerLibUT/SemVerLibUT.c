// Copyright (c) Joseph W Donahue dba SharperHacks.com
// http://www.sharperhacks.com
//
// Licensed under the terms of the MIT license (https://opensource.org/licenses/MIT). See LICENSE.TXT.
//
//  Contact: coders@sharperhacks.com
// ---------------------------------------------------------------------------

#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\SemVerLib\SemVer.h"

#define BUFSIZE 2048

int ProcessFile(char *fileName)
{
	FILE* fp = NULL;
	errno_t result = fopen_s(&fp, fileName, "r");

	bool expectValid = true;
	size_t failCount = 0;

	// TODO: Better error handling
	if (NULL == fp)
	{
		printf("Failed to open '%s'. Error code: %d\n", fileName, result);
		return result;
	}

	do 
	{
		char buf[BUFSIZE];
		
		memset(buf, 0, BUFSIZE);

		// TODO: fgets() return value handling.
		if (NULL == fgets(buf, BUFSIZE, fp)) break;

		buf[strlen(buf) - 1] = '\0';

		if (expectValid && 0 == strcmp(buf, "Begin Invalid"))
		{
			printf("*\n* Expecting invalid strings to end-of-file.\n*\n");
			expectValid = false;
		}

		VersionParseRecord *pvpr = ClassifyVersionCandidate(buf, NULL);

		if (expectValid)
		{
			if (eSemVer_2_0_0 != pvpr->versionType)
			{
				failCount++;
				printf("ClassifyVersionCandidate() failed for valid version string: %s\n", buf);
			}
			else
			{
				printf("Is valid SemVer: %s\n", buf);
			}
		}
		else
		{
			if (eSemVer_2_0_0 == pvpr->versionType)
			{
				failCount++;
				printf("ClassifyVersionCandidate() failed to reject invalid version string: %s\n", buf);
			}
			else
			{
				printf("Is invalid SemVer: %s\n", buf);
			}
		}

	} while (!feof(fp));

	return 0;
}

int main(int argc, char** argv)
{
	for (int idx = 1; idx < argc; idx++)
	{
		ProcessFile(argv[idx]);
	}

	return 0;
}
