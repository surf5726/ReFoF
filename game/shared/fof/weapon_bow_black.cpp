#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_bow_black.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#include "prediction.h"
#else
#include "fof/fof_projectiles.h"
#include "npcevent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
static float FoFBlackBowSignedSpread( float flMagnitude )
{
	return random->RandomInt( 0, 100 ) > 50 ?
		-flMagnitude : flMagnitude;
}

static void FoFFireBlackBowShootEvent( CFoF_Player *pOwner )
{
	IGameEvent *pEvent = gameeventmanager->CreateEvent(
		"player_shoot", true );
	if ( !pEvent )
		return;

	pEvent->SetInt( "userid", pOwner->GetUserID() );
	pEvent->SetString( "weapon", "arrow" );
	pEvent->SetInt( "mode", 0 );
	gameeventmanager->FireEvent( pEvent );
}

static void FoFFireBlackBowArrow( CFoF_Player *pOwner )
{
	Vector vecForward, vecRight, vecUp;
	pOwner->EyeVectors( &vecForward, &vecRight, &vecUp );
	const Vector vecOrigin =
		pOwner->Weapon_ShootPosition() + vecForward * 7.0f;

	const int nAccuracy = pOwner->m_nPlayerAccuracy;
	if ( nAccuracy == 3 )
	{
		vecForward.x += FoFBlackBowSignedSpread( 0.06f );
		vecForward.y += FoFBlackBowSignedSpread( 0.06f );
	}
	else if ( nAccuracy < 1 || nAccuracy > 2 )
	{
		vecForward.x += FoFBlackBowSignedSpread( 0.10f );
		vecForward.y += FoFBlackBowSignedSpread( 0.10f );
	}

	QAngle angArrow;
	VectorAngles( vecForward, angArrow );
	const float flSight = pOwner->GetFoFSightExpFactor();
	const int iDamage = static_cast< int >( RemapValClamped(
		flSight, 0.5f, 1.0f, 25.0f, 55.0f ) );
	CBowarrowBolt *pArrow = CBowarrowBolt::BoltCreate(
		vecOrigin, angArrow, iDamage, true, pOwner );
	if ( pArrow )
	{
		const float flSpeed = RemapValClamped(
			flSight, 0.5f, 1.0f, 1500.0f, 3000.0f );
		pArrow->SetAbsVelocity( vecForward * flSpeed );
	}
}
#endif

CWeaponBowarrowBlack::CWeaponBowarrowBlack()
{
	AddEffects( EF_ITEM_BLINK );
	m_bReloadsSingly = true;
	m_bFiresUnderwater = true;
#ifdef CLIENT_DLL
	m_FoFClientBowByte = 0;
	m_nFoFBowTrailingReserved = 0;
#else
	Q_memset( m_FoFBowAlignmentPadding, 0,
		sizeof( m_FoFBowAlignmentPadding ) );
	Q_memset( m_FoFBowTrailingPadding, 0,
		sizeof( m_FoFBowTrailingPadding ) );
#endif
	m_bPressed = false;
	m_bMustReload = false;
	m_flThrowPower = 0.0f;
	m_flArrowChange = 0.0f;
	m_bNeedArrowChange = false;
	m_nCurrentAmmo = 0;
}

bool CWeaponBowarrowBlack::Deploy()
{
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pOwner->RecalculateWeaponSpeed();
#ifdef CLIENT_DLL
		pOwner->SetFoFSightExpFactor( 0.0f );
#else
		pOwner->m_flSightExpFactor = 0.0f;
		pOwner->m_nPlayerInfo |= 0x10;
		m_nCurrentAmmo = 0;
#endif
		m_iClip1 = 0;
		m_bMustReload = true;

		CBaseViewModel *pViewModel = pOwner->GetViewModel( 0, true );
		if ( pViewModel )
		{
			pViewModel->m_nSkin = 1;
#ifdef CLIENT_DLL
			pViewModel->SetPoseParameter(
				pViewModel->GetModelPtr(), "aim_pitch", 0.0f );
#else
			pViewModel->SetPoseParameter( "aim_pitch", 0.0f );
#endif
			pViewModel->m_nSkin = 1;
		}
	}
	return BaseClass::Deploy();
}

bool CWeaponBowarrowBlack::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponBowarrowBlack::Reload()
{
	return DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
}

void CWeaponBowarrowBlack::PrimaryAttack()
{
	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
			Reload();
		else
			m_flNextPrimaryAttack = 0.15f;
		return;
	}

	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

#ifndef CLIENT_DLL
	if ( m_nCurrentAmmo == 0 )
	{
		FoFFireBlackBowShootEvent( pOwner );
		FoFFireBlackBowArrow( pOwner );
	}
#endif
	pOwner->SetAnimation( PLAYER_ATTACK1 );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
	--m_iClip1;
	if ( m_nCurrentAmmo != 2 )
		WeaponSound( SINGLE );
	pOwner->ViewPunch( QAngle( -4.0f, 0.0f, 0.0f ) );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75f;
	m_bMustReload = true;
	SetWeaponIdleTime(
		gpGlobals->curtime + SequenceDuration( ACT_VM_PRIMARYATTACK ) );
}

void CWeaponBowarrowBlack::SecondaryAttack()
{
}

void CWeaponBowarrowBlack::ItemPostFrame()
{
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	if ( m_bMustReload && HasWeaponIdleTimeElapsed() &&
		!( pOwner->m_afButtonPressed & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		Reload();
		return;
	}

	if ( ( pOwner->m_nButtons & IN_ATTACK ) &&
		gpGlobals->curtime >= m_flNextPrimaryAttack &&
		pOwner->GetFoFSightExpFactor() > 0.5f )
	{
		if ( m_iClip1 <= 0 && CanReload() && HasWeaponIdleTimeElapsed() )
		{
			Reload();
			return;
		}
		if ( pOwner->m_afButtonPressed & IN_ATTACK )
			m_flNextPrimaryAttack = gpGlobals->curtime;
		PrimaryAttack();
	}

	if ( m_iClip1 == 1 && HasWeaponIdleTimeElapsed() )
		WeaponIdle();
}

bool CWeaponBowarrowBlack::SendWeaponAnim( int iActivity )
{
	if ( iActivity == ACT_VM_IDLE && m_iClip1 <= 0 )
		iActivity = ACT_VM_FIDGET;
	return BaseClass::SendWeaponAnim( iActivity );
}

#ifndef CLIENT_DLL
void CWeaponBowarrowBlack::Operator_HandleAnimEvent(
	animevent_t *pEvent,
	CBaseCombatCharacter *pOperator )
{
	if ( !pEvent || pEvent->event != EVENT_WEAPON_RELOAD_FILL_CLIP )
	{
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		return;
	}

	m_iClip1 = 1;
	m_bMustReload = false;
}
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBowarrowBlack, DT_WeaponBowarrowBlack )

BEGIN_NETWORK_TABLE( CWeaponBowarrowBlack, DT_WeaponBowarrowBlack )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bMustReload ) ),
	RecvPropBool( RECVINFO( m_bPressed ) ),
	RecvPropFloat( RECVINFO( m_flThrowPower ) ),
	RecvPropFloat( RECVINFO( m_flArrowChange ) ),
	RecvPropBool( RECVINFO( m_bNeedArrowChange ) ),
	RecvPropInt( RECVINFO( m_nCurrentAmmo ) ),
#else
	SendPropBool( SENDINFO( m_bMustReload ) ),
	SendPropBool( SENDINFO( m_bPressed ) ),
	SendPropFloat( SENDINFO( m_flThrowPower ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flArrowChange ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bNeedArrowChange ) ),
	SendPropInt( SENDINFO( m_nCurrentAmmo ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponBowarrowBlack )
	DEFINE_PRED_FIELD( m_bMustReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bPressed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flThrowPower, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flArrowChange, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_bow_black, CWeaponBowarrowBlack )
#else
LINK_ENTITY_TO_CLASS( weapon_bow_black, CWeaponBowarrowBlack );
PRECACHE_WEAPON_REGISTER( weapon_bow_black );
#endif
