//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"

#include "hl2mp_player_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_ballistics.h"
#include "fof/fof_weapon_activities.h"
#if defined( CLIENT_DLL )
#include "fof/fof_viewmodel.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( basehl2mpcombatweapon, CBaseHL2MPCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHL2MPCombatWeapon , DT_BaseHL2MPCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHL2MPCombatWeapon , DT_BaseHL2MPCombatWeapon )
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseHL2MPCombatWeapon )

	DEFINE_FIELD( m_bLowered,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRaiseTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flHolsterTime,		FIELD_TIME ),

END_DATADESC()

#endif

BEGIN_PREDICTION_DATA( CBaseHL2MPCombatWeapon )
END_PREDICTION_DATA()

extern ConVar sk_auto_reload_time;

CBaseHL2MPCombatWeapon::CBaseHL2MPCombatWeapon( void )
{
#ifdef CLIENT_DLL
	Q_memset( m_FoFClientLayoutPad, 0, sizeof( m_FoFClientLayoutPad ) );
#endif
}

void CBaseHL2MPCombatWeapon::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();

	CBaseCombatCharacter *pOwner = GetOwner();
	if ( pOwner && !pOwner->IsPlayer() )
		return;

	if ( !pOwner || pOwner->GetActiveWeapon() == this )
		return;

	// FoF deliberately does not run HL2's timed holstered auto-reload
	// here.  It finishes this path by resolving the primary-hand handle.
	// Preserve that hand-aware access without reintroducing HL2MP reload logic.
	pOwner->GetActiveWeapon1();
}

void CBaseHL2MPCombatWeapon::WeaponIdle( void )
{
	Activity idleActivity = ACT_INVALID;
	if ( WeaponShouldBeLowered() )
	{
		if ( GetActivity() != ACT_VM_IDLE_LOWERED &&
			GetActivity() != ACT_VM_IDLE_TO_LOWERED &&
			GetActivity() != ACT_TRANSITION )
		{
			idleActivity = ACT_VM_IDLE_LOWERED;
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			idleActivity = ACT_VM_IDLE_LOWERED;
		}
	}
	else if ( ( m_flRaiseTime < gpGlobals->curtime &&
		GetActivity() == ACT_VM_IDLE_LOWERED ) ||
		HasWeaponIdleTimeElapsed() )
	{
		idleActivity = ACT_VM_IDLE;
	}

	if ( idleActivity != ACT_INVALID )
		SendWeaponAnim( idleActivity );
}

float CBaseHL2MPCombatWeapon::FoFActionSequenceDuration(
	float flFallbackDuration,
	bool bSetWeaponIdleTime )
{
#ifdef CLIENT_DLL
	float flDuration = SequenceDuration();
	if ( !IsFinite( flDuration ) || flDuration < 0.05f )
		flDuration = GetViewModelSequenceDuration();
#else
	float flDuration = GetViewModelSequenceDuration();
	if ( !IsFinite( flDuration ) || flDuration < 0.05f )
		flDuration = SequenceDuration();
#endif
	if ( !IsFinite( flDuration ) || flDuration < 0.05f )
		flDuration = flFallbackDuration;
	if ( bSetWeaponIdleTime )
		SetWeaponIdleTime( gpGlobals->curtime + flDuration );
	return flDuration;
}

bool CBaseHL2MPCombatWeapon::FoFAutoReloadEnabled(
	const CBasePlayer *pOwner ) const
{
	const CFoF_Player *pFoFOwner =
		dynamic_cast< const CFoF_Player * >( pOwner );
	return pFoFOwner &&
		( pFoFOwner->GetFoFPlayerInfo() & 0x20 ) != 0 &&
		( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) == 0;
}

void CBaseHL2MPCombatWeapon::FoFEmitPrimaryAttack(
	CBasePlayer *pOwner,
	int nShots,
	float flAutoAimScale,
	float flDamage,
	int iPlayerDamage,
	int nFlags )
{
	if ( !pOwner )
		return;

	pOwner->SetAnimation( PLAYER_ATTACK1 );
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( !pFoFOwner )
		return;

	pFoFOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
#ifndef CLIENT_DLL
	if ( flDamage == 0.0f && iPlayerDamage == 0 )
	{
		const CHL2MPSWeaponInfo &weaponInfo = GetHL2MPWpnData();
		flDamage = static_cast< float >( weaponInfo.m_iPlayerDamage );
		iPlayerDamage = weaponInfo.m_iPlayerDamage;
	}
#endif
	FoFFirePrimaryBullets(
		pOwner,
		m_iPrimaryAmmoType,
		nShots,
		flAutoAimScale,
		pFoFOwner->GetFoFCrosshairAperture( 0 ),
		flDamage,
		iPlayerDamage,
		nFlags );
}

void CBaseHL2MPCombatWeapon::FoFEmitReloadAnimationEvent(
	CBasePlayer *pOwner )
{
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( pFoFOwner )
		pFoFOwner->DoAnimationEvent( PLAYERANIMEVENT_RELOAD, 0 );
}

void CBaseHL2MPCombatWeapon::FoFPlayDryFire(
	float flFallbackDuration,
	bool bSetWeaponIdleTime )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	m_flNextPrimaryAttack = gpGlobals->curtime +
		FoFActionSequenceDuration(
			flFallbackDuration, bSetWeaponIdleTime );
}

bool CBaseHL2MPCombatWeapon::FoFRejectUnderwaterPrimaryAttack(
	CBasePlayer *pOwner )
{
	if ( !pOwner || pOwner->GetWaterLevel() != 3 || m_bFiresUnderwater )
		return false;

	WeaponSound( EMPTY );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Lower( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Ready( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_LOWERED_TO_IDLE ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = false;	
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Deploy( void )
{
	// If we should be lowered, deploy in the lowered position
	// We have to ask the player if the last time it checked, the weapon was lowered
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		CHL2MP_Player *pPlayer = assert_cast<CHL2MP_Player*>( GetOwner() );
		if ( pPlayer->IsWeaponLowered() )
		{
			if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) != ACTIVITY_NOT_AVAILABLE )
			{
				if ( DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_IDLE_LOWERED, (char*)GetAnimPrefix() ) )
				{
					m_bLowered = true;

					// Stomp the next attack time to fix the fact that the lower idles are long
					pPlayer->SetNextAttack( gpGlobals->curtime + 1.0 );
					m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
					m_flNextSecondaryAttack	= gpGlobals->curtime + 1.0;
					return true;
				}
			}
		}
	}

	m_bLowered = false;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) )
	{
		CFoF_Player *pOwner = ToFoFPlayer( ToBasePlayer( GetOwner() ) );
		if ( pOwner )
		{
#ifdef CLIENT_DLL
			pOwner->SetFoFSightExpFactor( 0.0f );
#else
			pOwner->m_flSightExpFactor = 0.0f;
			if ( ( pOwner->m_nPlayerInfo & 0x10 ) &&
				!pOwner->HasDualActiveWeapons() )
			{
				pOwner->m_nPlayerInfo &= ~0x10;
			}
#endif
		}

		SetWeaponVisible( false );
		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
  	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		 GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
  		return false;

	if ( m_bLowered )
		return true;
	
#if !defined( CLIENT_DLL )

	if ( GlobalEntity_GetState( "friendly_encounter" ) == GLOBAL_ON )
		return true;

#endif

	return false;
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
Vector CBaseHL2MPCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	return BaseClass::GetBulletSpread( proficiency );
}

//-----------------------------------------------------------------------------
float CBaseHL2MPCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	return BaseClass::GetSpreadBias( proficiency );
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBaseHL2MPCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHL2MPCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}


//-----------------------------------------------------------------------------
Vector CBaseHL2MPCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	Vector baseSpread = BaseClass::GetBulletSpread( proficiency );

	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flModifier = (pProficiencyValues)[ proficiency ].spreadscale;
	return ( baseSpread * flModifier );
}

//-----------------------------------------------------------------------------
float CBaseHL2MPCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[ proficiency ].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.50, 1.0	},
		{ 2.00, 1.0	},
		{ 1.50, 1.0	},
		{ 1.25, 1.0 },
		{ 1.00, 1.0	},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif
