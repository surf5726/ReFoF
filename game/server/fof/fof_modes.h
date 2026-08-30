#ifndef FOF_MODES_H
#define FOF_MODES_H
#ifdef _WIN32
#pragma once
#endif

void FoFCreateModeControllerForCurrentGame(
	bool bInitializeCurrentRound = false );
void FoFExecuteMapConfig();
void FoFApplyMapOwnedModeSelection();

#endif // FOF_MODES_H
