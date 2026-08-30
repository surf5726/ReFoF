#ifndef FOF_HINTS_H
#define FOF_HINTS_H
#ifdef _WIN32
#pragma once
#endif

void FoFCourseHintReceive( const char *title, const char *body, int mode );

// Rebuild the local hint catalogue and persisted display statistics.
// Returns the same completion ratio exposed by the original client routine.
float FoFInitializePlayerHints();

// Select and present one entry from fof_scripts/hints.txt using the supplied
// comma-separated tag list.
bool FoFShowPlayerHintTags(
	const char *tags, int mode, bool forceAny = false );

// Present one of the personal-record statistic updates on the FoF HUD.

// FoF's shipped client renders keyboard bindings as <KEY>.  Keep that
// presentation rule scoped to FoF UI instead of changing the SDK-wide helper.
void FoFReplaceKeyBindings(
	const wchar_t *input,
	int inputBytes,
	wchar_t *output,
	int outputBytes );

#endif // FOF_HINTS_H
