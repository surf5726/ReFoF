#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_base_revolver.h"
#include "fof/fof_weapon_activities.h"
#include "hl2mp_weapon_parse.h"
#include "fof/fof_weapon_properties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFWeaponHandPair_t
{
	const char *m_pszFirstHand;
	const char *m_pszSecondHand;
};

static const FoFWeaponHandPair_t s_FoFWeaponHandPairs[] =
{
	{ "weapon_ghostgun",         "weapon_ghostgun2" },
	{ "weapon_deringer",         "weapon_deringer2" },
	{ "weapon_volcanic",         "weapon_volcanic2" },
	{ "weapon_coltnavy",         "weapon_coltnavy2" },
	{ "weapon_sawedoff_shotgun", "weapon_sawedoff_shotgun2" },
	{ "weapon_hammerless",       "weapon_hammerless2" },
	{ "weapon_remington_army",   "weapon_remington_army2" },
	{ "weapon_maresleg",         "weapon_maresleg2" },
	{ "weapon_schofield",        "weapon_schofield2" },
	{ "weapon_peacemaker",       "weapon_peacemaker2" },
	{ "weapon_walker",           "weapon_walker2" },
	{ "weapon_whiskey",          "weapon_whiskey2" }
};

bool FoFIsRevolverWeapon( const CBaseCombatWeapon *pWeapon )
{
	return dynamic_cast< const CFoFBaseRevolver * >( pWeapon ) != NULL;
}

const char *FoFGetOppositeHandWeaponClassname( const char *pszClassname )
{
	if ( !pszClassname )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( s_FoFWeaponHandPairs ); ++i )
	{
		const FoFWeaponHandPair_t &pair = s_FoFWeaponHandPairs[i];
		if ( !Q_stricmp( pszClassname, pair.m_pszFirstHand ) )
			return pair.m_pszSecondHand;
		if ( !Q_stricmp( pszClassname, pair.m_pszSecondHand ) )
			return pair.m_pszFirstHand;
	}

	return NULL;
}

int FoFWeaponTier( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return -2;

	struct FoFWeaponTierName_t
	{
		const char *pszClassname;
		int nTier;
	};

	// Original item-definition table queried by CFoF_Player::BumpWeapon.
	// Grand Elimination promotes a player to outlaw only when this value is
	// greater than one.  Keep the immutable classification shared so client
	// and server code never grow independent weapon tables.
	static const FoFWeaponTierName_t s_WeaponTiers[] =
	{
		{ "weapon_fists", -2 },
		{ "weapon_fists_ghost", -2 },
		{ "weapon_ghostgun", -2 },
		{ "weapon_ghostgun2", -2 },
		{ "weapon_xbow", -2 },
		{ "weapon_knife", 0 },
		{ "weapon_deringer", 0 },
		{ "weapon_deringer2", 0 },
		{ "weapon_dynamite", 0 },
		{ "weapon_volcanic", 0 },
		{ "weapon_volcanic2", 0 },
		{ "weapon_coltnavy", 0 },
		{ "weapon_coltnavy2", 0 },
		{ "weapon_hammerless", 0 },
		{ "weapon_hammerless2", 0 },
		{ "weapon_remington_army", 0 },
		{ "weapon_remington_army2", 0 },
		{ "weapon_maresleg", 0 },
		{ "weapon_maresleg2", 0 },
		{ "weapon_carbine", 0 },
		{ "weapon_axe", 1 },
		{ "weapon_bow", 1 },
		{ "weapon_sawedoff_shotgun", 1 },
		{ "weapon_sawedoff_shotgun2", 1 },
		{ "weapon_schofield", 1 },
		{ "weapon_schofield2", 1 },
		{ "weapon_henryrifle", 1 },
		{ "weapon_whiskey", 1 },
		{ "weapon_whiskey2", 1 },
		{ "weapon_peacemaker", 2 },
		{ "weapon_peacemaker2", 2 },
		{ "weapon_bow_black", 2 },
		{ "weapon_coachgun", 2 },
		{ "weapon_spencer", 2 },
		{ "weapon_dynamite_black", 2 },
		{ "weapon_machete", 3 },
		{ "weapon_shotgun", 3 },
		{ "weapon_sharps", 3 },
		{ "weapon_walker", 3 },
		{ "weapon_walker2", 3 },
		{ "weapon_dynamite_belt", 3 },
	};

	const char *pszClassname = pWeapon->GetClassname();
	for ( int i = 0; pszClassname && i < ARRAYSIZE( s_WeaponTiers ); ++i )
	{
		if ( !Q_stricmp( pszClassname, s_WeaponTiers[i].pszClassname ) )
			return s_WeaponTiers[i].nTier;
	}
	return -2;
}

bool FoFWeaponCanChargeThrow( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	// GetWeaponID must identify an actual FoF weapon and may not be dynamite
	// (5) or the melee/throwable family (8). Base/unsupported weapons return
	// -1 and must not enter the charge, throw-animation or sound path.
	const int nWeaponID = pWeapon->FoFWeaponID();
	return nWeaponID > 0 && nWeaponID != 5 && nWeaponID != 8;
}

bool FoFWeaponHasActualReloadPresentation(
	CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	// The original client helper checks only the active weapon activity
	// for each physical hand.  It deliberately ignores m_bInReload and every
	// presentation-only guard.  Those extra states do not change on
	// the same command as the original server's activity and make sight
	// prediction contract or expand one command early at reload boundaries.
	// The remote client and a listen server can initialize the shared activity
	// table in different orders.  Use the same live-model mapping that sends
	// reload-start/finish so this predicate cannot compare a dynamically
	// resolved activity against a stale compile-time integer.  This matters at
	// the held reload + attack2 boundary: failing to recognize reload-start lets
	// sight expansion advance for one predicted command on every DS snapshot.
	const Activity nActivity = pWeapon->GetActivity();
	return nActivity == FoFVMReloadActivity( pWeapon ) ||
		nActivity == FoFVMReloadDeployedActivity( pWeapon ) ||
		nActivity == FoFShotgunReloadStartActivity( pWeapon ) ||
		nActivity == FoFShotgunReloadFinishActivity( pWeapon );
}

const CHL2MPSWeaponInfo *FoFWeaponInfo(
	CBaseCombatWeapon *pWeapon )
{
	return pWeapon ? &static_cast< const CHL2MPSWeaponInfo & >(
		pWeapon->GetWpnData() ) : NULL;
}

float FoFWeaponSpreadSpeed( CBaseCombatWeapon *pWeapon )
{
	const CHL2MPSWeaponInfo *pInfo = FoFWeaponInfo( pWeapon );
	return pInfo ? pInfo->m_flFoFSpreadSpeed : 0.0f;
}

float FoFWeaponAccuracySpread(
	CBasePlayer *pPlayer,
	CBaseCombatWeapon *pWeapon,
	int nAccuracy )
{
	const CHL2MPSWeaponInfo *pInfo = FoFWeaponInfo( pWeapon );
	if ( !pInfo )
		return 0.0f;

	nAccuracy = clamp( nAccuracy, 1, 5 );
	const float flBaseSpread = pInfo->m_flFoFSpread[nAccuracy];
	if ( !pWeapon->CanDualWield() )
		return flBaseSpread;

	// The original client indexes this table by
	// [hand stance][single/dual, sight, second gun].
	static const float s_FoFSpreadMultipliers[4][6] =
	{
		{ 0.90f, 0.90f, 0.88f, 0.88f, 1.00f, 1.00f },
		{ 0.87f, 1.00f, 1.25f, 1.30f, 0.85f, 1.00f },
		{ 1.00f, 0.75f, 1.30f, 1.25f, 1.15f, 0.87f },
		{ 1.12f, 1.12f, 1.35f, 1.35f, 0.85f, 0.85f },
	};

	const int nStance = clamp( FoFHandStance( pPlayer ), 0, 3 );
	const bool bSecondGun = pWeapon->IsSecondGun();
	int nColumn;
	if ( pPlayer->HasDualActiveWeapons() )
		nColumn = bSecondGun ? 3 : 2;
	else if ( FoFSightExpFactor( pPlayer ) > 0.0f )
		nColumn = bSecondGun ? 5 : 4;
	else
		nColumn = bSecondGun ? 1 : 0;

	return flBaseSpread * s_FoFSpreadMultipliers[nStance][nColumn];
}

float FoFSmoothCrosshairAperture(
	float flCurrent,
	float flTarget,
	float flSpreadSpeed )
{
	// The original implementation closes at one spread-speed per
	// second, opens at twice that rate, and snaps inside a 0.01 dead band.
	if ( flCurrent == flTarget )
		return flCurrent;

	const float flStep = MAX( flSpreadSpeed, 0.0f ) * gpGlobals->frametime;
	if ( flCurrent > flTarget )
		flCurrent -= flStep;
	else
		flCurrent += flStep * 2.0f;

	if ( fabsf( flCurrent - flTarget ) < 0.01f )
		flCurrent = flTarget;

	return flCurrent;
}
