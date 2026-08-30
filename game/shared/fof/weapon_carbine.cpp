#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_carbine.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponCarbine::CWeaponCarbine()
{
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
	m_bDelayedReload = false;
#ifdef CLIENT_DLL
	m_bReloaded = true;
#else
	m_bReloaded = false;
#endif
}

bool CWeaponCarbine::Deploy()
{
	m_bInReload = false;
	m_bReloaded = false;
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		if ( FoFShowsRifleCrosshair( pOwner ) )
			pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		else
			pOwner->m_Local.m_iHideHUD |= HIDEHUD_CROSSHAIR;
#ifndef CLIENT_DLL
		pOwner->m_nPlayerInfo |= 0x10;
#endif
	}
	return BaseClass::Deploy();
}

bool CWeaponCarbine::StartSingleShotReload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	m_bReloaded = false;
	m_bInReload = true;
	return true;
}

bool CWeaponCarbine::PlaySingleShotReload(
	CBasePlayer *pOwner,
	bool bLockOwnerUntilComplete )
{
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	m_bInReload = true;
	m_bReloaded = true;
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );
	FoFEmitReloadAnimationEvent( pOwner );
	if ( !DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD ) )
	{
		m_bInReload = false;
		m_bReloaded = false;
		return false;
	}

#ifdef CLIENT_DLL
	const float flEndTime = gpGlobals->curtime + SequenceDuration();
#else
	const float flEndTime = gpGlobals->curtime +
		FoFActionSequenceDuration( 1.0f, false );
#endif
	m_flNextPrimaryAttack = flEndTime;
	pOwner->SetNextAttack(
		bLockOwnerUntilComplete ? flEndTime : gpGlobals->curtime );
	return true;
}

void CWeaponCarbine::CompleteSingleShotReload( CBasePlayer *pOwner )
{
	if ( pOwner && pOwner->IsAlive() &&
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 &&
		m_iClip1 < GetMaxClip1() )
	{
		++m_iClip1;
	}
	m_bReloaded = false;
	m_bInReload = false;
}

bool CWeaponCarbine::HandleActiveReloadFrame( CBasePlayer *pOwner )
{
	if ( !m_bInReload )
		return false;
	if ( gpGlobals->curtime < m_flNextPrimaryAttack )
		return false;

	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		FinishReload();
		return true;
	}
	if ( !m_bReloaded )
	{
		if ( !Reload() )
			FinishReload();
	}
	else
	{
		FinishReload();
	}
	return true;
}

void CWeaponCarbine::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	UpdateAutoFire();
	if ( HandleActiveReloadFrame( pOwner ) )
		return;

	if ( m_bDelayedFire1 || ( pOwner->m_nButtons & IN_ATTACK ) )
	{
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			m_bDelayedFire1 = false;
			if ( UsesClipsForAmmo1() && m_iClip1 <= 0 )
			{
				if ( StartSingleShotReload() )
					return;
				WeaponSound( EMPTY );
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
				return;
			}
			if ( FoFRejectUnderwaterPrimaryAttack( pOwner ) )
				return;
			if ( pOwner->m_afButtonPressed & IN_ATTACK )
				m_flNextPrimaryAttack = gpGlobals->curtime;
			PrimaryAttack();
			return;
		}
	}

	if ( ( pOwner->m_nButtons & IN_RELOAD ) &&
		UsesClipsForAmmo1() && !m_bInReload )
	{
		if ( m_iClip1 < GetMaxClip1() &&
			pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
		{
			StartSingleShotReload();
		}
		return;
	}

	m_bFireOnEmpty = false;
	if ( m_iClip1 <= 0 &&
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		if ( FoFAutoReloadEnabled( pOwner ) &&
			m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			StartSingleShotReload();
			return;
		}
		WeaponIdle();
		return;
	}
	if ( !ReloadOrSwitchWeapons() && !m_bInReload )
		WeaponIdle();
}

bool CWeaponCarbine::Reload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || m_bReloaded ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	return PlaySingleShotReload( pOwner, true );
}

void CWeaponCarbine::FinishReload()
{
	CompleteSingleShotReload( ToBasePlayer( GetOwner() ) );
}

void CWeaponCarbine::PrimaryAttack()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || m_iClip1 <= 0 )
		return;

	WeaponSound( SINGLE );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_flNextPrimaryAttack = gpGlobals->curtime +
		FoFActionSequenceDuration( 0.5f );
	m_iClip1 = 0;
#ifdef CLIENT_DLL
	FoFPresentMuzzleFlash( pOwner, this, false );
#else
	pOwner->DoMuzzleFlash();
#endif
	FoFEmitPrimaryAttack( pOwner, 1, AUTOAIM_2DEGREES );

	const float flPunchY = SharedRandomFloat( "cartty", -1.0f, 1.0f );
	const float flPunchX = SharedRandomFloat( "carttx", -3.0f, -4.0f );
	pOwner->ViewPunch( QAngle( flPunchX, flPunchY, 0.0f ) );
#ifdef CLIENT_DLL
	FoFCreateMuzzleSmoke( pOwner, false );
#endif
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCarbine, DT_WeaponCarbine )

BEGIN_NETWORK_TABLE( CWeaponCarbine, DT_WeaponCarbine )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bDelayedFire1 ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire2 ) ),
	RecvPropBool( RECVINFO( m_bDelayedReload ) ),
	RecvPropBool( RECVINFO( m_bReloaded ) ),
#else
	SendPropBool( SENDINFO( m_bDelayedFire1 ) ),
	SendPropBool( SENDINFO( m_bDelayedFire2 ) ),
	SendPropBool( SENDINFO( m_bDelayedReload ) ),
	SendPropBool( SENDINFO( m_bReloaded ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponCarbine )
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bReloaded, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_carbine, CWeaponCarbine )
#else
LINK_ENTITY_TO_CLASS( weapon_carbine, CWeaponCarbine );
PRECACHE_WEAPON_REGISTER( weapon_carbine );
#endif
