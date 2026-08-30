#include "cbase.h"
#include "ammodef.h"
#include "fof/fof_weapon_ballistics.h"
#include "shot_manipulator.h"

#include "tier0/memdbgon.h"

static const float g_FoFFixedBulletPattern[17][2] =
{
	{  0.00f,  0.00f },
	{  0.00f,  0.50f }, {  0.00f, -0.50f },
	{  0.50f,  0.00f }, { -0.50f,  0.00f },
	{  0.35f,  0.35f }, { -0.35f, -0.35f },
	{ -0.35f,  0.35f }, {  0.35f, -0.35f },
	{  0.00f,  0.75f }, {  0.00f, -0.75f },
	{  0.75f,  0.00f }, { -0.75f,  0.00f },
	{  0.55f,  0.55f }, { -0.55f, -0.55f },
	{ -0.55f,  0.55f }, {  0.55f, -0.55f }
};

bool FoFAmmoUsesFixedBulletPattern(
	CAmmoDef *pAmmoDef,
	int iAmmoType,
	int nShots )
{
	if ( !pAmmoDef || iAmmoType < 0 || nShots < 2 )
		return false;

	if ( ( pAmmoDef->Flags( iAmmoType ) &
		FOF_AMMO_FIXED_BULLET_PATTERN ) != 0 )
		return true;

	Ammo_t *pAmmo = pAmmoDef->GetAmmoOfIndex( iAmmoType );
	return pAmmo && pAmmo->pName &&
		( !Q_stricmp( pAmmo->pName, "Buckshot" ) ||
		  !Q_stricmp( pAmmo->pName, "RockSalt" ) );
}

Vector FoFComputeBulletDirection(
	const FireBulletsInfo_t &info,
	int iShot,
	int iSeed,
	bool bFixedPattern,
	bool bPerimeterSpread )
{
	CShotManipulator manipulator( info.m_vecDirShooting );
	const int iSpreadSeed = iSeed + iShot;
	RandomSeed( iSpreadSeed );
	float flSpreadX = 0.0f;
	float flSpreadY = 0.0f;

	if ( bFixedPattern &&
		iShot >= 0 && iShot < ARRAYSIZE( g_FoFFixedBulletPattern ) )
	{
		flSpreadX = g_FoFFixedBulletPattern[iShot][0];
		flSpreadY = g_FoFFixedBulletPattern[iShot][1];
	}
	else if ( iShot == 0 && info.m_iShots > 1 &&
		( info.m_nFlags & FIRE_BULLETS_FIRST_SHOT_ACCURATE ) )
	{
		return manipulator.GetShotDirection();
	}
	else if ( bPerimeterSpread )
	{
		flSpreadX =
			SharedRandomFloat( "//2", -0.5f, 0.5f, iSpreadSeed ) +
			SharedRandomFloat( "//1", -0.5f, 0.5f, iSpreadSeed );
		flSpreadY = 1.0f - fabsf( flSpreadX );
		if ( SharedRandomFloat( "ay2", -0.5f, 0.5f, iSpreadSeed ) +
			SharedRandomFloat( "ay1", -0.5f, 0.5f, iSpreadSeed ) < 0.0f )
		{
			flSpreadY = -flSpreadY;
		}
	}
	else
	{
		flSpreadX =
			SharedRandomFloat( "bx2", -0.5f, 0.5f, iSpreadSeed ) +
			SharedRandomFloat( "bx1", -0.5f, 0.5f, iSpreadSeed );
		flSpreadY =
			SharedRandomFloat( "by2", -0.5f, 0.5f, iSpreadSeed ) +
			SharedRandomFloat( "by1", -0.5f, 0.5f, iSpreadSeed );
	}

	Vector vecDirection = manipulator.GetShotDirection() +
		flSpreadX * info.m_vecSpread.x * manipulator.GetRightVector() +
		flSpreadY * info.m_vecSpread.y * manipulator.GetUpVector();
	VectorNormalize( vecDirection );
	return vecDirection;
}

Vector FoFComputeBulletTraceSource(
	const FireBulletsInfo_t &info,
	int iShot,
	bool bFixedPattern )
{
	if ( !bFixedPattern || iShot < 0 ||
		iShot >= ARRAYSIZE( g_FoFFixedBulletPattern ) ||
		( info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER ) )
	{
		return info.m_vecSrc;
	}

	CShotManipulator manipulator( info.m_vecDirShooting );
	return info.m_vecSrc +
		g_FoFFixedBulletPattern[iShot][0] * 10.0f *
			manipulator.GetRightVector() +
		g_FoFFixedBulletPattern[iShot][1] * 10.0f *
			manipulator.GetUpVector();
}

void FoFFirePrimaryBullets(
	CBasePlayer *pOwner,
	int iAmmoType,
	int nShots,
	float flAutoAimScale,
	float flAperture,
	float flDamage,
	int iPlayerDamage,
	int nFlags )
{
	if ( !pOwner )
		return;

	FireBulletsInfo_t bulletInfo(
		nShots,
		pOwner->Weapon_ShootPosition(),
		pOwner->GetAutoaimVector( flAutoAimScale ),
		Vector( flAperture, flAperture, flAperture ),
		MAX_TRACE_LENGTH,
		iAmmoType );
	bulletInfo.m_pAttacker = pOwner;
	bulletInfo.m_bPrimaryAttack = true;
	bulletInfo.m_flDamage = flDamage;
	bulletInfo.m_iPlayerDamage = iPlayerDamage;
	bulletInfo.m_nFlags = nFlags;
	pOwner->FireBullets( bulletInfo );
}
