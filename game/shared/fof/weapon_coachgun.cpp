#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_coachgun.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponCoachgun::CWeaponCoachgun()
{
	m_bDouble = false;
	m_bReloaded = false;
}

bool CWeaponCoachgun::Deploy()
{
	m_bInReload = false;
	m_bReloaded = false;
#ifndef CLIENT_DLL
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pOwner->m_nPlayerInfo |= 0x10;
	}
#endif
	const bool bDeployed = BaseClass::Deploy();
	return bDeployed;
}

bool CWeaponCoachgun::Holster(
	CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		CBaseViewModel *pViewModel = pOwner->GetViewModel( 0, true );
		if ( pViewModel )
			pViewModel->SetBodygroup( 1, 0 );
	}
	m_bInReload = false;
#endif
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponCoachgun::ItemBusyFrame()
{
#ifdef CLIENT_DLL
	BaseClass::ItemBusyFrame();
#endif
}

void CWeaponCoachgun::ItemHolsterFrame()
{
#ifdef CLIENT_DLL
	BaseClass::ItemHolsterFrame();
#endif
}

bool CWeaponCoachgun::StartReload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || m_iClip1 >= GetMaxClip1() ||
		m_flNextPrimaryAttack > gpGlobals->curtime )
	{
		return false;
	}

	m_bInReload = true;
	return true;
}

bool CWeaponCoachgun::Reload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		m_iClip1 >= GetMaxClip1() || m_bReloaded )
	{
		return false;
	}

	m_bReloaded = true;
	m_bInReload = true;
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );
	FoFEmitReloadAnimationEvent( pOwner );
	DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
#ifdef CLIENT_DLL
	const float flEndTime = gpGlobals->curtime +
		FoFActionSequenceDuration( 0.5f, false );
#else
	const float flEndTime = gpGlobals->curtime + SequenceDuration();
#endif
	m_flNextPrimaryAttack = flEndTime;
	pOwner->SetNextAttack( flEndTime );
	return true;
}

void CWeaponCoachgun::FinishReload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	m_bInReload = false;
	m_bReloaded = false;
	m_iClip1 = 2;
}

void CWeaponCoachgun::DryFire()
{
#ifdef CLIENT_DLL
	FoFPlayDryFire( 0.5f, false );
#else
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
#endif
}

void CWeaponCoachgun::PrimaryAttack()
{
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || m_bInReload ||
		m_flNextPrimaryAttack > gpGlobals->curtime || m_iClip1 <= 0 )
	{
		return;
	}

#ifdef CLIENT_DLL
	FoFPresentMuzzleFlash( pOwner, this, false );
#else
	pOwner->DoMuzzleFlash();
#endif
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	m_bDouble = false;
	WeaponSound( SINGLE );
#ifdef CLIENT_DLL
	m_flNextPrimaryAttack = gpGlobals->curtime +
		FoFActionSequenceDuration( 0.5f, false );
#else
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
#endif
	--m_iClip1;

	const int nPellets =
		pOwner->GetFoFSightExpFactor() == 1.0f ? 13 : 11;
	FoFEmitPrimaryAttack(
		pOwner,
		nPellets,
		AUTOAIM_10DEGREES,
		5.0f,
		5,
		FIRE_BULLETS_DONT_HIT_UNDERWATER );

	const float flPunchZ = SharedRandomFloat( "coachz", -1.0f, -2.0f );
	const float flPunchY = SharedRandomFloat( "coachy", -1.0f, 2.0f );
	const float flPunchX = SharedRandomFloat(
		"coachx",
		m_bDouble ? -7.0f : -4.0f,
		m_bDouble ? -8.0f : -5.0f );
	pOwner->ViewPunch( QAngle( flPunchX, flPunchY, flPunchZ ) );
#ifdef CLIENT_DLL
	FoFCreateMuzzleSmoke( pOwner, false );
	if ( m_bDouble )
		FoFCreateMuzzleSmoke( pOwner, false );
#endif
	m_bDouble = false;
}

void CWeaponCoachgun::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	UpdateAutoFire();
	const bool bAutoReload = FoFAutoReloadEnabled( pOwner );

	if ( m_bInReload )
	{
		if ( !m_bReloaded )
		{
			Reload();
			return;
		}
		if ( m_flNextPrimaryAttack < gpGlobals->curtime )
			FinishReload();
		return;
	}

	if ( ( pOwner->m_nButtons & IN_ATTACK ) &&
		m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		if ( m_iClip1 <= 0 && CanReload() )
		{
			if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
				StartReload();
			else
				DryFire();
			return;
		}
		PrimaryAttack();
		return;
	}

	if ( ( pOwner->m_nButtons & IN_RELOAD ) && CanReload() )
	{
		StartReload();
		return;
	}

	m_bFireOnEmpty = false;
	if ( m_iClip1 <= 0 &&
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		if ( bAutoReload )
			StartReload();
		else
			WeaponIdle();
		return;
	}
	if ( !ReloadOrSwitchWeapons() && !m_bInReload )
		WeaponIdle();
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCoachgun, DT_WeaponCoachgun )

BEGIN_NETWORK_TABLE( CWeaponCoachgun, DT_WeaponCoachgun )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bDouble ) ),
	RecvPropBool( RECVINFO( m_bReloaded ) ),
#else
	SendPropBool( SENDINFO( m_bDouble ) ),
	SendPropBool( SENDINFO( m_bReloaded ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponCoachgun )
	DEFINE_PRED_FIELD( m_bDouble, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bReloaded, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_coachgun, CWeaponCoachgun )
#else
LINK_ENTITY_TO_CLASS( weapon_coachgun, CWeaponCoachgun );
PRECACHE_WEAPON_REGISTER( weapon_coachgun );
#endif
