#ifndef FOF_WEAPON_PROPERTIES_H
#define FOF_WEAPON_PROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

class CBaseCombatWeapon;
class CBasePlayer;
class CHL2MPSWeaponInfo;

int FoFWeaponTier( CBaseCombatWeapon *pWeapon );
bool FoFWeaponCanChargeThrow( CBaseCombatWeapon *pWeapon );
bool FoFWeaponHasActualReloadPresentation( CBaseCombatWeapon *pWeapon );
bool FoFIsRevolverWeapon( const CBaseCombatWeapon *pWeapon );
const char *FoFGetOppositeHandWeaponClassname( const char *pszClassname );
const CHL2MPSWeaponInfo *FoFWeaponInfo( CBaseCombatWeapon *pWeapon );
float FoFWeaponSpreadSpeed( CBaseCombatWeapon *pWeapon );

float FoFWeaponAccuracySpread(
	CBasePlayer *pPlayer,
	CBaseCombatWeapon *pWeapon,
	int nAccuracy );
float FoFSmoothCrosshairAperture(
	float flCurrent,
	float flTarget,
	float flSpreadSpeed );

#endif // FOF_WEAPON_PROPERTIES_H
