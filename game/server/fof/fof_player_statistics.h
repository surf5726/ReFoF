#ifndef FOF_PLAYER_STATISTICS_H
#define FOF_PLAYER_STATISTICS_H
#ifdef _WIN32
#pragma once
#endif

class CBaseCombatWeapon;
class CBasePlayer;
class CFoF_Player;

void FoFTrackLevelWeaponUse(
	CBaseCombatWeapon *pWeapon, CBasePlayer *pPlayer );

void FoFRecordAccuracyShots( CFoF_Player *pPlayer, int nShots );
void FoFRecordAccuracyHit(
	CFoF_Player *pPlayer, float flDamage );
void FoFResetAccuracyStats( CFoF_Player *pPlayer );
float FoFGetAuthoritativeAccuracy( const CFoF_Player *pPlayer );

#endif // FOF_PLAYER_STATISTICS_H

