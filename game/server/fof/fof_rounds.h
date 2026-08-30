#ifndef FOF_ROUNDS_H
#define FOF_ROUNDS_H
#ifdef _WIN32
#pragma once
#endif

class CBaseEntity;
class CFoF_Player;
class Vector;

void FoFAddManualRespawnProbe(
	const char *pszMapName, unsigned int nNavAreaID,
	const Vector &vecOrigin );
void FoFGenerateRespawnProbeMap( void );
bool FoFGetRespawnProbeDebugCoordinate( Vector *pCoordinate );
bool FoFHasRespawnProbeMap( void );
void FoFUpdatePlayerRespawnInfluence( CFoF_Player *pPlayer );
void FoFSetPlayerRespawnThreat(
	CFoF_Player *pPlayer, CBaseEntity *pThreat );
CBaseEntity *FoFSelectRespawnPoint( CFoF_Player *pPlayer );

#endif // FOF_ROUNDS_H
