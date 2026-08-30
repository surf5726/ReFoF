#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_spencer.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#else
#include "npcevent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponSpencer::CWeaponSpencer()
{
	m_bReloadsSingly = true;
	m_bNeedPump = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
	m_bDelayedReload = false;
}

bool CWeaponSpencer::Deploy()
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

bool CWeaponSpencer::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponSpencer::ItemBusyFrame()
{
	BaseClass::ItemBusyFrame();
}

void CWeaponSpencer::ItemHolsterFrame()
{
	BaseClass::ItemHolsterFrame();
#ifndef CLIENT_DLL
	if ( m_bInReload && m_iClip1 < GetMaxClip1() )
		m_bInReload = false;
#endif
}

void CWeaponSpencer::DryFire()
{
	FoFPlayDryFire( 0.5f );
}

void CWeaponSpencer::PrimaryAttack()
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
	const float flPunchY = SharedRandomFloat( "spencer", -1.0f, 1.0f );
	const float flPunchX = SharedRandomFloat( "spencer", -2.0f, -3.0f );
	pOwner->ViewPunch( QAngle( flPunchX, flPunchY, 0.0f ) );
#ifdef CLIENT_DLL
	FoFCreateMuzzleSmoke( pOwner, false );
#endif
}

bool CWeaponSpencer::StartReload()
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

bool CWeaponSpencer::Reload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

#ifdef CLIENT_DLL
	if ( !DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD ) )
		return false;
#else
	DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
#endif
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );
	pOwner->SetAnimation( PLAYER_RELOAD );
	FoFEmitReloadAnimationEvent( pOwner );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	return true;
}

void CWeaponSpencer::FinishReload()
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

void CWeaponSpencer::Pump()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	WeaponSound( SPECIAL1 );
	SendWeaponAnim( FoFShotgunPumpActivity( this ) );
	const float flEndTime = gpGlobals->curtime + SequenceDuration();
	pOwner->SetNextAttack( flEndTime );
	m_flNextPrimaryAttack = flEndTime;
	if ( m_bDelayedReload )
		m_bDelayedReload = false;
}

void CWeaponSpencer::ItemPostFrame()
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

#ifndef CLIENT_DLL
void CWeaponSpencer::Operator_HandleAnimEvent(
	animevent_t *pEvent,
	CBaseCombatCharacter *pOperator )
{
	if ( !pEvent )
		return;

	if ( pEvent->event == 3266 )
	{
		m_bNeedPump = false;
		return;
	}

	if ( pEvent->event == EVENT_WEAPON_RELOAD_FILL_CLIP )
	{
		CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
		if ( pOwner &&
			pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 &&
			m_iClip1 < GetMaxClip1() )
		{
			++m_iClip1;
		}
		return;
	}

	BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
}
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSpencer, DT_WeaponSpencer )

BEGIN_NETWORK_TABLE( CWeaponSpencer, DT_WeaponSpencer )
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
BEGIN_PREDICTION_DATA( CWeaponSpencer )
	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_spencer, CWeaponSpencer )
#else
LINK_ENTITY_TO_CLASS( weapon_spencer, CWeaponSpencer );
PRECACHE_WEAPON_REGISTER( weapon_spencer );
#endif
