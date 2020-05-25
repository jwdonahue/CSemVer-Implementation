// Copyright (c) Joseph W Donahue dba SharperHacks.com
// http://www.sharperhacks.com
//
// Licensed under the terms of the MIT license (https://opensource.org/licenses/MIT). See LICENSE.TXT.
//
//  Contact: coders@sharperhacks.com
// ---------------------------------------------------------------------------

#pragma once

#ifndef _SharperHacks_SemVer_h_Defined
#define _SharperHacks_SemVer_h_Defined

#include <stdlib.h>  // for size_t.
#include <stdbool.h> // for bool.

// The lack of an unambiguous distinction between v1 and v2 of SemVer
// is it's most glaring defect.  But a v1 string also qualifies as a v2 string,
// so we ignore v1 strings all-together.  The one possible exception is v1-beta,
// anyone who took a dependency on v1-beta should have upgraded to v1 long ago.
// Hence no v1 here!
typedef enum 
{ 
	eNotVersion = 0,	// Certainly not a recognizable version.
	eUnknownVersion,	// May be a version of unknown type.
	eSemVer_2_0_0		// Definitely SemVer 2 compliant.
} VersionType;

// The ClassifyVersionCandidate() implementation uses these states, and
// publishes the maximum state achieved during parsing, in the VersionParseRecord
// that it returns.
typedef enum 
{ 
	eStart = 0,
	eInMajor,
	eInMinor,			
	eInPatch, 
	eInPrereleaseFirstChar,
	eInPrereleaseFirstFieldChar,
	eInPreAlphaNumericField,
	eInPreNumericField,
	eInMetaFirstChar,
	eInMetaField
} ParseState;

typedef struct _ParsedTagRecord
{
	// Points to first valid field character, not the delims.
	size_t fieldIdx;

	// Count of field characters, not delims.
	size_t fieldLength;

	// Only applies to prerelease fields. Never set for meta.
	bool fieldHasLeadingZero;

	// 'N' == numeric. Because 'N' < 'a' in the ASCII table.
	// 'a' == alphanumeric. Becuase 'a' > 'N' in the ASCII table.
	char fieldType;

} ParsedTagRecord;

// We surface all of this for cases where a tool must gracefully fall-back
// to some non-SemVer version string, in which case they can use this to
// decide how to proceed.  It may be that the string becomes SemVer compliant,
// if truncated at the string can be truncated at the point we stopped parsing.
//
// This data can also be used to efficiently split the version string into 
// database fields.  A table of string fields can easily be sorted correctly.
//
// Note that a v1-beta string will fail after accumulating the entire version
// triple, and VersionParseRecord->it will be pointing to the dot that starts
// the prerelease tag.  But this result will be ambigous in the case of a four
// digit version string, so further parsing will be required and may not yield
// accurate results anyway.  You can at least flag the string for human 
// intervention or conversion.
typedef struct _VersionParseRecord
{
	VersionType versionType;
	size_t majorDigits;
	size_t minorDigits;
	size_t patchDigits;
	size_t prereleaseChars;
	size_t prereleaseFieldCount;
	size_t metaChars;
	size_t metaFieldCount;

	// Assume that if versionType == eSemVer_2_0_0, then majorIdx must be zero.
	// These indexes point to first valid field character, not the delims.
	size_t minorIdx;
	size_t patchIdx;

	bool isPrereleaseVersion; 
	bool hasPrereleaseTag;
	bool hasMetaTag;

	// When these are true, the field has either a single zero in it,
	// or it's got other digits or trash that disqualified the string.
	bool majorHasLeadingZero;
	bool minorHasLeadingZero;
	bool patchHasLeadingZero;

	// Dynamic array of prerelease data.
	ParsedTagRecord *pPrereleaseData;

	// Dynamic array of build meta data.
	ParsedTagRecord *pMetaData;

	ParseState state;

	// For each character that is successfully parsed, parsedIdx is incremented.
	size_t parsedIdx;

} VersionParseRecord;

/// <summary>
/// Determine the best version type.
/// </summary>
/// <remarks>
/// Classification requires parsing, so this function does both while
/// accumulating information regarding the exact layout of the string.
/// </remarks>
extern VersionParseRecord* ClassifyVersionCandidate(const char *pCandidate, VersionParseRecord *pParsed);

/// <summary>
/// Applies SemVer rules to compare pV1 to pV2.
/// </summary>
/// <param name="pV1>Ponter to version string.</param>
/// <param name="pdr1>
/// Pointer to parse results from ClassifyVersionCandidate(pV1), or null.
/// </param>
/// <param name="pV2>Ponter to version string.</param>
/// <param name="pdr2>
/// Pointer to parse results from ClassifyVersionCandidate(pV2), or null.
/// </param>
/// <returns>
/// -1 if *pV1 < *pV2
///  0 if *pV1 == *pV2
///  1 if *pV1 > *pV2
/// -2 if either string is not eSemVer_2_0_0.
/// </returns>
extern int CompareVersions(const char *pV1, const VersionParseRecord *pdr1, const char *pV2, const VersionParseRecord *pdr2);

// Helpers.

#define IsValidTagFieldChar(c) (isalpha(c) || isdigit(c) || ((char)(c) == '-'))
#define IsValidPrereleaseFieldChar(c) IsValidTagFieldChar(c)
#define IsValidMetaFieldChar(c) IsValidTagFieldChar(c)

#endif

