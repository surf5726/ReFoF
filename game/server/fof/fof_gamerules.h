#ifndef FOF_GAMERULES_H
#define FOF_GAMERULES_H
#pragma once

void FoFPrecacheAssets();
bool FoFShouldPreserveRoundEntity( const char *pszClassname );
bool FoFShouldUseDynamicRespawns( void );
bool FoFMapDisablesHorseRam( void );
float FoFGetItemRespawnTime( void );

#endif // FOF_GAMERULES_H
