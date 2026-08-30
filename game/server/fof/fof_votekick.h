#ifndef FOF_VOTEKICK_H
#define FOF_VOTEKICK_H
#ifdef _WIN32
#pragma once
#endif

class CFoF_Player;

bool FoFShowVotekickMenu( CFoF_Player *pPlayer, int nPage = 0 );
bool FoFHandleVotekickSelection(
	CFoF_Player *pPlayer, int nCommandId );

#endif // FOF_VOTEKICK_H
