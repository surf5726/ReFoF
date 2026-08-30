#ifndef FOF_WEAPON_BALLISTICS_H
#define FOF_WEAPON_BALLISTICS_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CAmmoDef;
class Vector;
struct FireBulletsInfo_t;

enum FoFAmmoFlags_t
{
	FOF_AMMO_FIXED_BULLET_PATTERN = 0x20000000,
};

void FoFFirePrimaryBullets(
	CBasePlayer *pOwner,
	int iAmmoType,
	int nShots,
	float flAutoAimScale,
	float flAperture,
	float flDamage = 0.0f,
	int iPlayerDamage = 0,
	int nFlags = 0 );

// FoF uses one deterministic 17-point pattern for its pellet ammunition.
// Keep this test and the seeded random-spread path shared by local prediction,
// authoritative server traces and remote temp-entity presentation.
bool FoFAmmoUsesFixedBulletPattern(
	CAmmoDef *pAmmoDef,
	int iAmmoType,
	int nShots );
Vector FoFComputeBulletDirection(
	const FireBulletsInfo_t &info,
	int iShot,
	int iSeed,
	bool bFixedPattern,
	bool bPerimeterSpread = false );
Vector FoFComputeBulletTraceSource(
	const FireBulletsInfo_t &info,
	int iShot,
	bool bFixedPattern );

#endif // FOF_WEAPON_BALLISTICS_H
