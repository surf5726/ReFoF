#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_shotgun_fof.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponShotgunFoF::CWeaponShotgunFoF()
{
	m_bReloadsSingly = true;
	m_bNeedPump = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
	m_bDelayedReload = false;
}

void CWeaponShotgunFoF::ContinueReload( CBasePlayer *pOwner )
{
	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		( m_iClip1 > 0 &&
		  ( pOwner->m_nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) ) )
	{
		FinishReload();
		return;
	}

	if ( m_iClip1 < GetMaxClip1() && Reload() )
		return;

	FinishReload();
}

bool CWeaponShotgunFoF::Deploy()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
#ifndef CLIENT_DLL
	CFoF_Player *pFoFOwner = ToFoFPlayer( GetOwner() );
	if ( pFoFOwner && pFoFOwner->IsAlive() )
		pFoFOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
#endif
	if ( pOwner && pOwner->IsAlive() &&
		m_iSecondaryAmmoType >= 0 &&
		pOwner->GetAmmoCount( m_iSecondaryAmmoType ) > 0 &&
		m_iPrimaryAmmoType != m_iSecondaryAmmoType )
	{
		pOwner->SetAmmoCount(
			pOwner->GetAmmoCount( m_iSecondaryAmmoType ),
			m_iPrimaryAmmoType );
		m_iPrimaryAmmoType = m_iSecondaryAmmoType;
		m_iClip1 = 6;
		m_iClip2 = 0;
	}

	const bool bDeployed = BaseClass::Deploy();
	return bDeployed;
}

bool CWeaponShotgunFoF::Holster(
	CBaseCombatWeapon *pSwitchingTo )
{
	if ( GetActivity() == FoFShotgunReloadFinishActivity( this ) )
		return false;
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponShotgunFoF::ItemHolsterFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner &&
#ifndef CLIENT_DLL
		pOwner->IsAlive() &&
#endif
		pOwner->GetActiveWeapon() != this &&
		m_bInReload && m_iClip1 < GetMaxClip1() )
	{
		m_bInReload = false;
	}
}

bool CWeaponShotgunFoF::StartReload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( m_bNeedPump || !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	SendWeaponAnim( FoFShotgunReloadStartActivity( this ) );
	SetBodygroup( 1, 0 );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_bInReload = true;
	return true;
}

bool CWeaponShotgunFoF::Reload()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	CBaseViewModel *pViewModel = pOwner->GetViewModel( 0, true );
	if ( pViewModel )
	{
		pViewModel->SetBodygroup(
			1, m_iPrimaryAmmoType == m_iSecondaryAmmoType ? 1 : 0 );
	}

	++m_iClip1;
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );
	FoFEmitReloadAnimationEvent( pOwner );
	DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	return true;
}

void CWeaponShotgunFoF::FinishReload()
{
	SetBodygroup( 1, 1 );
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	SendWeaponAnim( FoFShotgunReloadFinishActivity( this ) );
	m_bInReload = false;
	pOwner->SetNextAttack( gpGlobals->curtime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

void CWeaponShotgunFoF::Pump()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	m_bNeedPump = false;
	WeaponSound( SPECIAL1 );
	SendWeaponAnim( FoFShotgunPumpActivity( this ) );
	const float flEndTime = gpGlobals->curtime + SequenceDuration();
	pOwner->SetNextAttack( flEndTime );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	if ( m_bDelayedReload )
		m_bDelayedReload = false;
}

void CWeaponShotgunFoF::DryFire()
{
#ifdef CLIENT_DLL
	FoFPlayDryFire( 0.5f, false );
#else
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
#endif
}

void CWeaponShotgunFoF::PrimaryAttack()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || m_iClip1 <= 0 )
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
	FoFEmitPrimaryAttack(
		pOwner,
		11,
		AUTOAIM_10DEGREES,
		5.0f,
		5,
		FIRE_BULLETS_DONT_HIT_UNDERWATER );

	m_bInReload = false;
	m_bNeedPump = true;
	const float flPunchY = SharedRandomFloat( "shotgunpay", -2.0f, 2.0f );
	const float flPunchX = SharedRandomFloat( "shotgunpax", -2.0f, -1.0f );
	pOwner->ViewPunch( QAngle( flPunchX, flPunchY, 0.0f ) );
#ifdef CLIENT_DLL
	FoFCreateMuzzleSmoke( pOwner, false );
#endif
}

void CWeaponShotgunFoF::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	const bool bAutoReload = FoFAutoReloadEnabled( pOwner );
	if ( m_bNeedPump && ( pOwner->m_nButtons & IN_RELOAD ) )
		m_bDelayedReload = true;

	if ( m_bInReload && gpGlobals->curtime >= m_flNextPrimaryAttack )
	{
		ContinueReload( pOwner );
		return;
	}

	if ( m_bNeedPump && m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		Pump();
		return;
	}

	if ( m_bDelayedFire2 || ( pOwner->m_nButtons & IN_ATTACK2 ) )
	{
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			m_bDelayedFire2 = false;
			if ( m_iClip1 <= 1 && UsesClipsForAmmo1() )
			{
				if ( m_iClip1 == 1 )
					PrimaryAttack();
				else if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
					DryFire();
				else
					StartReload();
				return;
			}
			if ( FoFRejectUnderwaterPrimaryAttack( pOwner ) )
				return;
			if ( pOwner->m_afButtonPressed & IN_ATTACK2 )
				m_flNextPrimaryAttack = gpGlobals->curtime;
			SecondaryAttack();
			return;
		}
	}

	if ( m_bDelayedFire1 || ( pOwner->m_nButtons & IN_ATTACK ) )
	{
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime )
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
	else if ( m_iClip1 <= 0 && bAutoReload &&
		m_flNextPrimaryAttack < gpGlobals->curtime && StartReload() )
	{
		return;
	}

	WeaponIdle();
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShotgunFoF, DT_WeaponShotgunFoF )

BEGIN_NETWORK_TABLE( CWeaponShotgunFoF, DT_WeaponShotgunFoF )
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
BEGIN_PREDICTION_DATA( CWeaponShotgunFoF )
	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire1, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedFire2, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_shotgun, CWeaponShotgunFoF )
#else
LINK_ENTITY_TO_CLASS( weapon_shotgun, CWeaponShotgunFoF );
PRECACHE_WEAPON_REGISTER( weapon_shotgun );
#endif
