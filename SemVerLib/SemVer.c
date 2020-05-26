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

#include "SemVer.h"

#include <ctype.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

// TODO:
// We need assert() or something like it, to always be functional.
// Not sure this is 100% portable.
#ifdef NDEBUG
 #undef NDEBUG
#endif
#define DEBUG
 #include <assert.h>
#undef DEBUG

static const char _alphanumT = 'a'; // Becuase 'a' > 'N' in the ASCII table.
static const char _numericT = 'N';	// Because 'N' < 'a' in the ASCII table.

static const char _null = '\0';
static const char _dot = '.';
static const char _hyphen = '-';
static const char _plus = '+';
static const char _zero = '0';

// When a tag is detected, we allocate space for this many ParsedTagRecord's.
// When we run out, this is how many we attempt to expand it by. Considered 
// using a linked list, but I think over-all, this will be faster, consume less
// memory and provide better locality of data for cache hits.
static const size_t _prereleaseDataAllocationCount = 5;
static const size_t _metaDataAllocationCount = 5;

// Private functions in alphabetical order...

// Ensures that pParsed is properly initialized, or allocates an initialized
// record if pParsed is NULL.
static inline VersionParseRecord* InitializeParseDataRecord(VersionParseRecord *pParsed)
{
	if (NULL == pParsed)
	{
		pParsed = calloc(1, sizeof(VersionParseRecord));
	}
	else
	{
		memset(pParsed, 0, sizeof(VersionParseRecord));
	}
	
	assert(NULL != pParsed);
	return pParsed;
}

// Reallocate a zeroed block on the heap and copy oldBlock into it.
static inline void* recalloc(void *oldBlock, size_t currentCount, size_t additionalCount, size_t size)
{
	assert(additionalCount > 0);
	void *newBlock = calloc(currentCount + additionalCount, size);
	assert(NULL != newBlock);
	memcpy(newBlock, oldBlock, size * currentCount);
	free(oldBlock);
}

// Ensure that pParsed data is set correctly, after running off the end
// of the version string. This is the final node in the classification
// state machine.
static inline VersionParseRecord* SetFinalVersion(VersionParseRecord *pParsed)
{
	// Easier to maintain as a switch statement, than an if/else tree.
	// The compiler/optimizer will decide whether to use a jump table 
	// or test and branch.
	switch (pParsed->state)
	{
		case eInPatch:
			// fall-thru...
		// If eInPrereleaseFirstChar, we failed to successfully advance.
		// If eInPrereleaseFirstFieldChar, we failed to successfully advance.
		case eInPreAlphaNumericField:
			// fall-thru...
		case eInPreNumericField:
			// fall-thru...
		// If eInMetaFirstChar, we failed to successfully advance.
		case eInMetaField:
			pParsed->versionType = eSemVer_2_0_0;
			break;
		default:
			pParsed->versionType = eUnknownVersion;
	}

	return pParsed;
}

// Called for all early exits from the state machine.
static inline VersionParseRecord* SetVersionType(VersionParseRecord *p, VersionType vt)
{
	p->versionType = vt;
	return p;
}

// Called from each of the two points in the state machine that jump into build
// meta processing.
static void TransitionToMeta(VersionParseRecord *pParsed, const char *pIter, const char *pCandidate)
{
	pParsed->pMetaData = calloc(_metaDataAllocationCount, sizeof(ParsedTagRecord));
	assert(NULL != pParsed->pMetaData);
	pParsed->pMetaData[0].fieldIdx = (pIter - pCandidate) + 1;
	pParsed->state = eInMetaFirstChar;
}


//
// I make no apologies for the complexity of this function.  It's a state 
// machine, implemented as a giant switch statement.  It could be simplified,
// but only at the expense of raising the total complexity of this module (IMO),
// and possibly reducing opportunities for the compiler to optimize it.
//
// Normally I wouldn't write a function with a complexity level > 7, but this is
// exactly the kind of problem that can only be solved efficiently in this way.  
// Not only that, but you'll have to grok the comments as well as the code, or 
// you may be lost.  The only alternative is to use a regex module, and I am
// trying to avoid non standard library dependencies.  I also don't trust regex
// compilers to do the right thing 100% of the time.
//
VersionParseRecord* ClassifyVersionCandidate(const char *pCandidate, VersionParseRecord *pParsed)
{
	pParsed = InitializeParseDataRecord(pParsed);

	if ((NULL == pCandidate) || (_null == pCandidate[0])) return SetVersionType(pParsed, eNotVersion);

	const char *pIter = pCandidate;

	// Note that there are no look-aheads.  
	// We parse the string one character at a time, for exactly O(n).

	while(_null != *pIter)
	{
		switch( pParsed->state )
		{
			case eStart: // Expect first major version field digit.

				// *pIter has the first character in the string.
			
				// There's two ways out of this state.  Either this string doesn't
				// look like any kind of version number, or it starts with a digit.

				if (!isdigit(*pIter)) return SetVersionType(pParsed, eNotVersion);
				
				pParsed->state = eInMajor;
				pParsed->majorDigits++;

				// Only place we can legally encounter leading zero in major, is here.
				if (_zero == *pIter) pParsed->majorHasLeadingZero = true; 

				break;
			
			case eInMajor: // Expect digits or dot.

				// There's four ways out of major. Either we hit a dot, violate
				// the no leading zero rule, find some other trash, or the loop
				// terminates on a _null character.  
				// Otherwise, we count digits.
				
				// We've already consumed one digit to get into this state.

				if (_dot == *pIter)	
				{
					if (pParsed->majorHasLeadingZero)
					{
						pParsed->isPrereleaseVersion = true;
					}

					pParsed->minorIdx = (pIter - pCandidate) + 1;
					pParsed->state = eInMinor;

					break;
				}
				
				// If we're looking at another digit AND majorHasLeadingZero, or there is
				// any kind of trash, we have no clue what kind of version string this is.
				if (!isdigit(*pIter) || pParsed->majorHasLeadingZero) return SetVersionType(pParsed, eUnknownVersion);

				pParsed->majorDigits++;

				break;
			
			case eInMinor:  // Expect digits or dot.

				// We entered this state with it pointing to the first
				// candidate digit.

				// There's four ways out of minor. Either we hit a dot, violate
				// the no leading zero rule, find some other trash, or the loop
				// terminates on a _null character.
				// Otherwise, we count digits.

				if (_dot == *pIter) 
				{
					if (0 == pParsed->minorDigits) return SetVersionType(pParsed, eUnknownVersion);

					pParsed->patchIdx = (pIter - pCandidate) + 1;
					pParsed->state = eInPatch;
					break;
				}

				if (!isdigit(*pIter) || pParsed->minorHasLeadingZero) return SetVersionType(pParsed, eUnknownVersion);

				if (_zero == *pIter)
				{
					pParsed->minorHasLeadingZero = true;
				}

				pParsed->minorDigits++;

				break;

			case eInPatch:  // Expect digits, hyphen or plus.

				// We entered this state with it pointing to the first
				// candidate digit, so on our first pass, patchDigits is zero.

				// There's six ways out of patch.  Either we hit a dot, a hyphen or plus,
				// violate the no leading zero rule, some other trash, or the loop 
				// terminates on a _null character.
				// Otherwise, we count digits.

				if (0 != pParsed->patchDigits)
				{
					if (_hyphen == *pIter) 
					{
						pParsed->pPrereleaseData = calloc(_prereleaseDataAllocationCount, sizeof(ParsedTagRecord));
						assert(NULL != pParsed->pPrereleaseData);
						pParsed->pPrereleaseData[0].fieldIdx = (pIter - pCandidate) + 1;
						pParsed->state = eInPrereleaseFirstChar;
						break;
					}

					if (_plus == *pIter)
					{
						pParsed->pMetaData = calloc(_metaDataAllocationCount, sizeof(ParsedTagRecord));
						assert(NULL != pParsed->pMetaData);
						pParsed->pMetaData[0].fieldIdx = (pIter - pCandidate) + 1;
						pParsed->state = eInMetaFirstChar;
						break;
					}

					// Four or more dotted fields, not SemVer.
					if (_dot == *pIter) return SetVersionType(pParsed, eUnknownVersion); 
				}
				else // 0 == patchDigits
				{
					if ((_hyphen == *pIter) || (_plus == *pIter) || (_dot == *pIter))
					{
						return SetVersionType(pParsed, eUnknownVersion);
					}
				}

				if (!isdigit(*pIter)) return SetVersionType(pParsed, eUnknownVersion);

				if (pParsed->patchHasLeadingZero) return SetVersionType(pParsed, eUnknownVersion);

				if (_zero == *pIter)
				{
					// A second zero, counts as "other trash".
					if (pParsed->patchHasLeadingZero) return SetVersionType(pParsed, eUnknownVersion);;
					pParsed->patchHasLeadingZero = true;
				}

				pParsed->patchDigits++;
				break;	

			case eInPrereleaseFirstChar: // Expect digits or alpha characters.

				// There can be multiple dot seperated fields.
				// There cannot be any zero length fields.
				// Either the first character in the field is a digit or a character.
				//   If a digit and it's zero, and there's more than on digit but no alpha's, it's invalid.
				//   If a digit and it's 1..9, the field may be either numeric or alphanumeric.
				//   If a character, the field is alphanumeric.

				// We entered this state with pIter pointing to the first
				// candidate digit, so prereleaseDigits is currently zero.
				if ((_plus == *pIter) || (_dot == *pIter)) return SetVersionType(pParsed, eUnknownVersion);

				// Fall-thru...
			case eInPrereleaseFirstFieldChar:

				// We should be looking at a valid field char at this point.
				if (IsValidPrereleaseFieldChar(*pIter)) 
				{
					if (isdigit(*pIter)) 
					{
						pParsed->state = eInPreNumericField;
						if (_zero == *pIter) 
						{
							pParsed->pPrereleaseData[pParsed->prereleaseFieldCount].fieldHasLeadingZero = true;
						}
					}
					else
					{
						pParsed->state = eInPreAlphaNumericField;
					}

					// We visit this code once per valid field.
					pParsed->prereleaseFieldCount++;
					pParsed->prereleaseChars++;
				}
				else
				{
					return SetVersionType(pParsed, eUnknownVersion);
				}

				break;

			case eInPreAlphaNumericField:

				// We get here only if the first character, and any subsequent characters were legal.  
				// We're now looking for field delimiters and invalid characters.

				if (_dot == *pIter)
				{
					assert(NULL != pParsed->pPrereleaseData);
					ParsedTagRecord *ppdr = &(pParsed->pPrereleaseData[pParsed->prereleaseFieldCount - 1]);
					ppdr->fieldType = _alphanumT;

					pParsed->state = eInPrereleaseFirstFieldChar;
					break;
				}
				else if (_plus == *pIter)
				{
					TransitionToMeta(pParsed, pIter, pCandidate);
					break;
				}
				else if (!IsValidPrereleaseFieldChar(*pIter)) 
				{
					return SetVersionType(pParsed, eUnknownVersion);
				}

				pParsed->prereleaseChars++;

				break;

			case eInPreNumericField:

				// We get here only if the first character, and any subsequent characters were legal.  
				// We're now looking for field delimiters and invalid characters.
				// We may have to fall-back to alphanum field status on valid non-digit.
				
				if (!isdigit(*pIter))
				{
					if (pParsed->fieldNeedsAlphaToPass)
					{
						if ((_dot == *pIter) || (_plus == *pIter))
						{
							pParsed->prereleaseFieldCount--;
							return SetVersionType(pParsed, eUnknownVersion);
						}
					}

					if (_dot == *pIter)
					{
						assert(NULL != pParsed->pPrereleaseData);
						ParsedTagRecord *ppdr = &(pParsed->pPrereleaseData[pParsed->prereleaseFieldCount - 1]);
						ppdr->fieldType = _alphanumT;

						pParsed->state = eInPrereleaseFirstFieldChar;
					}
					else if (_plus == *pIter)
					{
						TransitionToMeta(pParsed, pIter, pCandidate);
						break;
					}
					else if (isalpha(*pIter) || (_hyphen == *pIter))
					{
						assert(NULL != pParsed->pPrereleaseData);
						ParsedTagRecord *ppdr = &(pParsed->pPrereleaseData[pParsed->prereleaseFieldCount - 1]);
						ppdr->fieldHasLeadingZero = false;
						pParsed->state = eInPreAlphaNumericField;
						pParsed->fieldNeedsAlphaToPass = false;
						pParsed->pPrereleaseData[pParsed->prereleaseFieldCount].fieldHasLeadingZero = false;
					}
				}
				else if ((pParsed->pPrereleaseData[pParsed->prereleaseFieldCount - 1].fieldHasLeadingZero))
				{
					// At this point, we may not have a SemVer string at all, but if there's
					// an alpha character in the field, it could pass.  So we mark this as
					// needing an alpha character to succeed.
					pParsed->fieldNeedsAlphaToPass = true;
				}

				pParsed->prereleaseChars++;

				break;

			case eInMetaFirstChar:

				// Meta is a little bit simpler than prerelease. No worries 
				// about leading zeros, but empty fields are still forbidden.
				if (!IsValidMetaFieldChar(*pIter)) return SetVersionType(pParsed, eUnknownVersion);

				pParsed->state = eInMetaField;
				pParsed->metaChars++;
				pParsed->metaFieldCount++;

				break;

			case eInMetaField :
				
				// We get here only if the first character, and any subsequent characters were legal.  
				// Watch for field delimiters and invalid characters.

				if (_dot == *pIter)
				{
					assert(NULL != pParsed->pMetaData);
					ParsedTagRecord *ppdr = &(pParsed->pMetaData[pParsed->metaFieldCount - 1]);
					ppdr->fieldLength = (pIter - pCandidate) + 1;
					pParsed->state = eInMetaFirstChar;
				}
				else if (!IsValidMetaFieldChar(*pIter))
				{
					return SetVersionType(pParsed, eUnknownVersion);
				}

				pParsed->metaChars++;
				break;
		}

		pIter++;
		pParsed->parsedIdx++;
	} // end while(...)

	// When we fall out of the loop, we've run out of characters to parse,
	// and there have been no obvious problems.  But whether we have a valid
	// SemVer string depends on how far we got.

	if (pParsed->fieldNeedsAlphaToPass)
	{
		pParsed->prereleaseFieldCount--;
		return SetVersionType(pParsed, eUnknownVersion);
	}

	pParsed->hasPrereleaseTag = (NULL != pParsed->pPrereleaseData);
	pParsed->isPrereleaseVersion |= pParsed->hasPrereleaseTag;
	pParsed->hasMetaTag = (NULL != pParsed->pMetaData);

	return SetFinalVersion(pParsed);
}

// These should work for both unsigned and signed char.
#define MakeUnsignedWord(b1, b2) ((uint16_t)((uint16_t)(((uint8_t)(b1) & 0xF) << 8) & ((uint16_t)(((uint8_t)(b2) & 0xF)))))
#define MakeUnsignedDWord(b1, b2, b3, b4) ( ( (((uint32_t)MakeUnsignedWord((b1), (b2)) << 16) ) & (((uint32_t)MakeUnsignedWord((b3), (b4)))) ) )
#define MakeUnsignedQWord(b1, b2, b3, b4, b5, b6, b7, b8) ( ( ((uint64_t)MakeUnsignedDWord((b1), (b2), (b3), (b4)) << 32) & ((uint64_t)MakeUnsignedDWord((b5), (b6), (b7), (b8))) ) )

static inline int CompareBytes(char b1, char b2)
{
	if (b1 > b2) return 1;
	if (b1 < b2) return -1;
	return 0;
}

static inline int CompareWords(uint16_t b1, uint16_t b2)
{
	if (b1 > b2) return 1;
	if (b1 < b2) return -1;
	return 0;
}

static inline int CompareDWords(uint32_t b1, uint32_t b2)
{
	if (b1 > b2) return 1;
	if (b1 < b2) return -1;
	return 0;
}

static inline int CompareQWords(uint64_t b1, uint64_t b2)
{
	if (b1 > b2) return 1;
	if (b1 < b2) return -1;
	return 0;
}

// This should only ever be called when both fields are the same length!
// TODO: Investiage whether a simple linear comparison of characters is faster.
//       I suspect the additional compares will in fact slow it down. The bit
//		 twiddling we're doing here can be achieved in registers, so both take
//		 the same memory load hit, but what we're doing here will have fewer
//		 code pipeline stalls.
//
// In our world, we have numeric fields (version triple and prerelease fields),
// and alphanumeric fields.  This function can compare any two fields of the 
// same length. Fields that are not the same length are easy to sort based on
// their length alone.
//
static int CompareFields(const char *pV1, size_t idx1, const char *pV2, size_t idx2, size_t count)
{
	int result;

	pV1 += idx1;
	pV2 += idx2;

	switch (count)
	{
		case 0:

			result = 0;
			break;

		case 1:

			// The vast majority of version strings in use today have only one or two
			// digits in most of their fields.
			result = CompareBytes(*pV1, *pV2);
			break;

		case 2:

			result = CompareWords(MakeUnsignedWord(*pV1, *(pV1 + 1)), MakeUnsignedWord(*pV2, *(pV2 + 1)));
			break;

		case 3:
		{
			result = CompareWords(MakeUnsignedWord(*pV1, *(pV1 + 1)), MakeUnsignedWord(*pV2, *(pV2 + 1)));
			if (0 == result)
			{
				result = CompareBytes(*(pV1 + 2), *(pV2 + 2));
			} // else
			break;
		}

		case 4:

			// Now we're getting into the "-beta" territory and some active patch nums.
			result = CompareDWords( MakeUnsignedDWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3)),
									MakeUnsignedDWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3)));
			break;

		case 5:

			result = CompareDWords( MakeUnsignedDWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3)),
									MakeUnsignedDWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3)));
			if (0 == result)
			{
				result = CompareBytes(*(pV1 + 4), *(pV2 + 4));
			}
			break;

		case 6:

			result = CompareDWords( MakeUnsignedDWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3)),
									MakeUnsignedDWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3)));
			if (0 == result)
			{
				result = CompareWords( MakeUnsignedWord(*(pV1 + 4), *(pV1 + 5)),
									   MakeUnsignedWord(*(pV2 + 4), *(pV2 + 5)));
			}
			break;

		case 7:

			result = CompareDWords( MakeUnsignedDWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3)),
									MakeUnsignedDWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3)));
			if (0 == result)
			{
				result = CompareWords( MakeUnsignedWord(*(pV1 + 4), *(pV1 + 5)),
									   MakeUnsignedWord(*(pV2 + 4), *(pV2 + 5)));
			}

			if (0 == result)
			{
				result = CompareBytes(*(pV1 + 6), *(pV2 + 6));
			}
			break;

		case 8:

			result = CompareQWords( MakeUnsignedQWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3), *(pV1 + 4), *(pV1 + 5), *(pV1 + 6), *(pV1 + 7)),
								    MakeUnsignedQWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3), *(pV2 + 4), *(pV2 + 5), *(pV2 + 6), *(pV2 + 7)));
			break;

		default:

			result = CompareQWords( MakeUnsignedQWord(*pV1, *(pV1 + 1), *(pV1 + 2), *(pV1 + 3), *(pV1 + 4), *(pV1 + 5), *(pV1 + 6), *(pV1 + 7)),
								 MakeUnsignedQWord(*pV2, *(pV2 + 1), *(pV2 + 2), *(pV2 + 3), *(pV2 + 4), *(pV2 + 5), *(pV2 + 6), *(pV2 + 7)));
			if (0 == result)
			{
				// Recursion should be safe, until we start seeing prerelease
				// fields that are several hundred bytes long (unlikely).
				result = CompareFields(pV1, idx1 + 8, pV2, idx2 + 8, count - 8);
			}
			break;
	}

	return result;
}


// Applies sorting logic to prerelease tags.
static int ComparePrereleaseTags(const char *pV1, const VersionParseRecord *pdr1, const char *pV2, const VersionParseRecord *pdr2)
{
	if (pdr1->prereleaseFieldCount > pdr2->prereleaseFieldCount) return 1;
	if (pdr1->prereleaseFieldCount < pdr2->prereleaseFieldCount) return -1;
	// else they have the same number of fields, try comparing the field types and lengths.

	for (size_t idx = 0; idx < pdr1->prereleaseFieldCount; idx++)
	{
		if (pdr1->pPrereleaseData[idx].fieldType > pdr2->pPrereleaseData[idx].fieldType) return 1;
		if (pdr1->pPrereleaseData[idx].fieldType < pdr2->pPrereleaseData[idx].fieldType) return -1;
		// else they have the same type, so check their lengths.

		if (pdr1->pPrereleaseData[idx].fieldLength > pdr2->pPrereleaseData[idx].fieldLength) return 1;
		if (pdr1->pPrereleaseData[idx].fieldLength < pdr2->pPrereleaseData[idx].fieldLength) return -1;
		// else they have matching field lengths, now we must compare contents...

		int result = CompareFields(pV1, pdr1->pPrereleaseData[idx].fieldIdx, pV2, pdr2->pPrereleaseData[idx].fieldIdx, pdr1->pPrereleaseData[idx].fieldLength);

		// When they compare the same, we have to compare the next field, if any.
		if (0 == result) continue;
		
		// We found a difference!
		return result;
	}

	// If we ran out of fields without returning a result, then they must be equal.
	return 0;
}

int CompareVersions(const char *pV1, const VersionParseRecord *pdr1, const char *pV2, const VersionParseRecord *pdr2)
{
	assert(NULL != pV1);
	assert(NULL != pdr1);
	assert(NULL != pV2);
	assert(NULL != pdr2);

	if ( (pdr1->versionType != eSemVer_2_0_0) || (pdr2->versionType != eSemVer_2_0_0))
	{
		// We don't know how to compare non-SemVer strings.
		return -2;
	}

	// At this point, we have two SemVer 2.0.0 validated strings.
	// First compare the parse data, if those match, then compare
	// string segments.

	if (pdr1->majorDigits == pdr2->majorDigits)
	{
		int fieldCompareResult = CompareFields(pV1, 0, pV2, 0, pdr1->majorDigits);

		if (0 != fieldCompareResult)
		{
			return fieldCompareResult;
		}

		if (pdr1->minorDigits == pdr2->minorDigits)
		{
			fieldCompareResult = CompareFields(pV1, pdr1->minorIdx, pV2, pdr2->minorIdx, pdr1->minorDigits);

			if (0 != fieldCompareResult)
			{
				return fieldCompareResult;
			}

			if (pdr1->patchDigits == pdr2->patchDigits)
			{
				fieldCompareResult = CompareFields(pV1, pdr1->patchIdx, pV2, pdr2->patchIdx, pdr1->patchDigits);

				if (0 != fieldCompareResult)
				{
					return fieldCompareResult;
				}

				// At this point we have equal triples.
				// If one or the other is lacking a prerlease tag, it's "bigger".
				
				if (pdr1->hasPrereleaseTag && !pdr2->hasPrereleaseTag) return 1;
				if (!pdr1->hasPrereleaseTag && pdr2->hasPrereleaseTag) return -1;

				if (pdr1->prereleaseFieldCount == pdr2->prereleaseFieldCount)
				{
					return ComparePrereleaseTags(pV1, pdr1, pV2, pdr2);
				}
				else
				{
					return pdr1->prereleaseFieldCount > pdr2->prereleaseFieldCount ? 1 : -1;
				}
			}
			else
			{
				return pdr1->patchDigits > pdr2->patchDigits ? 1 : -1;
			}
		}
		else
		{
			return pdr1->minorDigits > pdr2->minorDigits ? 1 : -1;
		}
	}
	else
	{
		return pdr1->majorDigits > pdr2->majorDigits ? 1 : -1;
	}

	return -2;
}


