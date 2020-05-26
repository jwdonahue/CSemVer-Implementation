# CSemVer-Implementation
An implementation of SemVer written in C.

This is work-in-progress.  There are bugs. Mostly missing code where I got distracted by my day job.

Written to test a theory that it would be faster to process the raw SemVer strings, than to parse them out into scalar and string fields.
My [research](https://github.com/semver/semver/issues/567#issuecomment-633266706) suggests that both space and time can be conserved.
Read the code for the current list of TODO's.  I've got some ideas for new features, once I complete and test what I've already got underway.
