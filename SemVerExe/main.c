// Copyright (c) Joseph W Donahue dba SharperHacks.com
// http://www.sharperhacks.com
//
// Licensed under the terms of the MIT license (https://opensource.org/licenses/MIT). See LICENSE.TXT.
//
//  Contact: coders@sharperhacks.com
// ---------------------------------------------------------------------------

// NOTE1: As I write this code, I intend to target C compilers from C-99
// onwards.  But I only have VisualStudio 2017 to work with here, so if I
// missed the mark, please let me know right away, and I will do everything
// possible to fix it.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\SemVerLib\SemVer.h"

static const char *_usage = 
	"SemVer -option [arg ...]\n" \
	"  Options (not case sensitive):\n" \
	"    -h | -? | -help\n" \
	"      Show this help text." \
	"    -v | -validate <candidateVersion>\n" \
	"      Outputs 'Valid SemVer' and returns 0 if valid.\n"
	"      Outputs 'Invalid string' and returns -2 if not valid.\n"
	"    -c | -compare <candidate1> <candidate2>\n" \
	"      Outputs 'candidate1 > candidate2' and returns 1, if 1 > 2.\n" \
	"      Outputs 'candidate1 < candidate2' and returns -1, if 1 < 2.\n" \
	"      Outputs 'candidate1 == candidate2' and returns 0, if 1 == 2.\n" \
	"      Outputs 'No semver: candidate' and returns -2, if either not SemVer.\n" \
	"\n";

static const char _hyphen = '-';

static int _argc;
static char **_argv;
static int _argIdx = 1;

static int _handlerIdx = 0;

typedef int (*ArgHandler)(void);

static int Compare(void);
static int Help(void);
static bool ParseArg(int idx);
static int Validate(void);


static struct {
	char *ptoken;
	ArgHandler handler;
} _argHandlers[] = 
{
	{{"v"}, Validate}, // Index zero must be Validate.
	{{"c"}, Compare},
	{{"validate"}, Validate},
	{{"compare"}, Compare},
	{{"?"}, Help},
	{{"h"}, Help},
	{{"help"}, Help},
};

static int Compare(void)
{
	return -2;
}

static bool HandleArgs(int argc, char **argv)
{
	if (argc < 1) return false;

	_argc = argc;
	_argv = argv;

	if (!ParseArg(_argIdx))
	{
		return false;
	}

	return true;
}

static int Help(void)
{
	printf("%s", _usage);

	return 0;
}

static int MatchArg(char *token)
{
	for (int idx = 0; idx < sizeof _argHandlers; idx++)
	{
		if (0 == strcmp(token, _argHandlers[idx].ptoken))
		{
			return _handlerIdx = idx;
		}
	}

	return -1;
}

static bool ParseArg(int idx)
{
	char *arg = _argv[idx];

	if ((_hyphen == arg[0]))
	{
		int matchedIdx = MatchArg(arg+1);
		if (-1 != matchedIdx) return true;
	}
	else
	{
		// It's just a version string, caller wants validation.
		_handlerIdx = 0;
		return true;
	}
	
	return false;
}

static int validate(char *candidate)
{
	VersionParseRecord *vpr = ClassifyVersionCandidate(candidate, NULL);

	if (eSemVer_2_0_0 == vpr->versionType)
	{
		printf("Valid semver: %s", candidate);
		return 0;
	}

	printf("Invalid string: %s", candidate);

	return -2;
}

static int Validate(void)
{
	int valueIdx = _argIdx + 1;
	return validate(_argv[valueIdx]);
}

//
//
//
int main(int argc, char **argv)
{
	if (!HandleArgs(argc, argv)) return -2;

	return _argHandlers[_handlerIdx].handler();
}

