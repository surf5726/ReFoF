#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_yellowboy.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponYellowboy::CWeaponYellowboy()
{
	m_bReloadsSingly = true;
	m_bNeedPump = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
	m_bDelayedReload = false;
}

bool CWeaponYellowboy::Deploy()
{
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

bool CWeaponYellowboy::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponYellowboy::ItemBusyFrame()
{
	BaseClass::ItemBusyFrame();
}

void CWeaponYellowboy::ItemHolsterFrame()
{
	BaseClass::ItemHolsterFrame();
#ifndef CLIENT_DLL
	if ( m_bInReload && m_iClip1 < GetMaxClip1() )
		m_bInReload = false;
#endif
}

void CWeaponYellowboy::DryFire()
{
	FoFPlayDryFire( 0.5f );
}

void CWeaponYellowboy::PrimaryAttack()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( m_bNeedPump || !pOwner || !pOwner->IsAlive() || m_iClip1 <= 0 )
		return;

	WeaponSound( SINGLE );
#ifdef CLIENT_DLL
	FoFPresentMuzzleFlash( pOwner, this, false );
#else
	pOwner->DoMuzzleFlash();
#endif
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	--m_iClip1;
	FoFEmitPrimaryAttack( pOwner, 1, AUTOAIM_2DEGREES );
	m_bInReload = false;
	m_bNeedPump = true;
	const float flPunchY = SharedRandomFloat( "hennry", -0.5f, 0.5f );
	const float flPunchX = SharedRandomFloat( "hennry", -3.0f, -4.0f );
	pOwner->ViewPunch( QAngle( flPunchX, flPunchY, 0.0f ) );
#ifdef CLIENT_DLL
	FoFCreateMuzzleSmoke( pOwner, false );
#endif
}

bool CWeaponYellowboy::StartReload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( m_bNeedPump || !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	SendWeaponAnim( FoFShotgunReloadStartActivity( this ) );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_bInReload = true;
	return true;
}

bool CWeaponYellowboy::Reload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	++m_iClip1;
	DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );
	pOwner->SetAnimation( PLAYER_RELOAD );
	FoFEmitReloadAnimationEvent( pOwner );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	return true;
}

void CWeaponYellowboy::FinishReload()
{
	m_bInReload = false;
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
#ifdef CLIENT_DLL
	if ( !pOwner || !pOwner->IsAlive() )
#else
	if ( !pOwner )
#endif
		return;

	SendWeaponAnim( FoFShotgunReloadFinishActivity( this ) );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

void CWeaponYellowboy::Pump()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;
	m_bNeedPump = false;
	WeaponSound( SPECIAL1 );
	SendWeaponAnim( FoFShotgunPumpActivity( this ) );
	const float flEndTime = gpGlobals->curtime + SequenceDuration();
	pOwner->SetNextAttack( flEndTime );
	m_flNextPrimaryAttack = flEndTime;
	if ( m_bDelayedReload )
		m_bDelayedReload = false;
}

void CWeaponYellowboy::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	if ( m_bNeedPump && ( pOwner->m_nButtons & IN_RELOAD ) )
		m_bDelayedReload = true;

	if ( m_bInReload )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->curtime )
			return;
		if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		{
			FinishReload();
			return;
		}
		if ( m_iClip1 > 0 &&
			( pOwner->m_nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) )
		{
			FinishReload();
			return;
		}
		if ( m_iClip1 < GetMaxClip1() )
		{
			if ( !Reload() )
				FinishReload();
		}
		else
		{
			FinishReload();
		}
		return;
	}

	if ( m_bNeedPump && m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		Pump();
		return;
	}

	if ( !m_bNeedPump &&
		( m_bDelayedFire1 || ( pOwner->m_nButtons & IN_ATTACK ) ) &&
		m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		m_bDelayedFire1 = false;
		if ( ( UsesClipsForAmmo1() && m_iClip1 <= 0 ) ||
			( !UsesClipsForAmmo1() &&
			  pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ) )
		{
			if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
				DryFire();
			else
				StartReload();
			return;
		}
		if ( FoFRejectUnderwaterPrimaryAttack( pOwner ) )
			return;
		if ( pOwner->m_afButtonPressed & IN_ATTACK )
			m_flNextPrimaryAttack = gpGlobals->curtime;
		PrimaryAttack();
		return;
	}

	if ( ( pOwner->m_nButtons & IN_RELOAD ) &&
		UsesClipsForAmmo1() && !m_bInReload && !m_bDelayedReload )
	{
		StartReload();
		return;
	}

	m_bFireOnEmpty = false;
	if ( !HasAnyAmmo() && m_flNextPrimaryAttack < gpGlobals->curtime )
	{
		if ( !( GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY ) &&
			pOwner->SwitchToNextBestWeapon( this ) )
		{
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.3f;
			return;
		}
	}
	else if ( m_iClip1 <= 0 && FoFAutoReloadEnabled( pOwner ) &&
		m_flNextPrimaryAttack < gpGlobals->curtime && StartReload() )
	{
		return;
	}
	WeaponIdle();
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponYellowboy, DT_WeaponYellowboy )

BEGIN_NETWORK_TABLE( CWeaponYellowboy, DT_WeaponYellowboy )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bNeedPump ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire1 ) ),
	RecvPropBool( RECVINFO( m_bDelayedFire2 ) ),
	RecvPropBool( RECVINFO( m_bDelayedReload ) ),
#else
	SendPropBool( SENDINFO( m_bNeedPump ) ),
	SendPropBool( SENDINFO( m_bDelayedFire1 ) ),
	SendPropBool( SENDINFO( m_bDelayedFire2 ) ),
	SendPropBool( SENDINFO( m_bDelayedReload ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponYellowboy )
	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_henryrifle, CWeaponYellowboy )
#else
LINK_ENTITY_TO_CLASS( weapon_henryrifle, CWeaponYellowboy );
PRECACHE_WEAPON_REGISTER( weapon_henryrifle );
#endif
