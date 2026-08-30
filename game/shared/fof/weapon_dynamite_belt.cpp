#include "cbase.h"
#include "ammodef.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/weapon_dynamite_belt.h"
#include "in_buttons.h"
#include "npcevent.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#include "fof/fof_weapon_classmap.h"
#else
#include "basegrenade_shared.h"
#include "fof/fof_bot.h"
#include "recipientfilter.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoF_Player *FoFDynamiteOwner( CBaseEntity *pEntity )
{
	return ToFoFPlayer( pEntity );
}

#ifndef CLIENT_DLL
static void FoFCheckDynamiteThrowPosition( CBasePlayer *pPlayer,
	const Vector &vecEye, Vector &vecSource )
{
	const float flRadius = 6.0f;
	trace_t trace;
	UTIL_TraceHull( vecEye, vecSource,
		Vector( -flRadius, -flRadius, -flRadius ),
		Vector( flRadius, flRadius, flRadius ),
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer,
		pPlayer->GetCollisionGroup(), &trace );
	if ( trace.DidHit() )
		vecSource = trace.endpos;
}

static void FoFCreateDynamiteProjectile( const char *pszClassname,
	const Vector &vecSource, const QAngle &angDynamite,
	const Vector &vecVelocity, const AngularImpulse &angVelocity,
	CBaseEntity *pThrower, float flTimer )
{
	CBaseEntity *pEntity = CBaseEntity::Create(
		pszClassname, vecSource, angDynamite, pThrower );
	CBaseGrenade *pGrenade = dynamic_cast< CBaseGrenade * >( pEntity );
	if ( !pGrenade )
		return;

	pGrenade->SetThrower( ToBaseCombatCharacter( pThrower ) );
	pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;
	if ( pThrower )
		pGrenade->ChangeTeam( pThrower->GetTeamNumber() );
	IPhysicsObject *pPhysics = pGrenade->VPhysicsGetObject();
	if ( pPhysics )
	{
		// FoF's common projectile factory scales every initial linear
		// velocity, including the two death-release paths.
		Vector vecScaledVelocity = vecVelocity * 0.85f;
		pPhysics->SetVelocity( &vecScaledVelocity, &angVelocity );
	}

	variant_t timer;
	timer.SetFloat( MAX( 0.01f, flTimer ) );
	pGrenade->AcceptInput(
		"SetTimer", pThrower, pThrower, timer, 0 );
}
#endif
CWeaponDynamiteBelt::CWeaponDynamiteBelt()
{
	AddEffects( EF_ITEM_BLINK );
#ifndef CLIENT_DLL
	Q_memset( m_FoFDynamitePadding0, 0,
		sizeof( m_FoFDynamitePadding0 ) );
	Q_memset( m_FoFDynamitePadding1, 0,
		sizeof( m_FoFDynamitePadding1 ) );
#endif
	m_bRedraw = false;
	m_AttackPaused = 0;
	m_fDrawbackFinished = false;
	m_flCountdown = 0.0f;
	m_flReleaseTime = 0.0f;
	m_flDrawbackFinishTime = 0.0f;
	m_flRedrawTime = 0.0f;
	m_flOwnerAccuracy = 0.0f;
}

Activity CWeaponDynamiteBelt::PrimaryPullbackActivity() const
{
	return m_flOwnerAccuracy < 0.5f ?
		ACT_VM_PULLBACK : ACT_VM_PULLBACK_HIGH;
}

float CWeaponDynamiteBelt::PrimaryReleaseDelay() const
{
	return 1.25f;
}

float CWeaponDynamiteBelt::PrimaryCountdownDuration() const
{
	return 3.5f;
}

bool CWeaponDynamiteBelt::ShouldThrowOnHolster() const
{
	return false;
}

bool CWeaponDynamiteBelt::ShouldReleasePrimaryThrow(
	CFoF_Player *pOwner ) const
{
	return pOwner != NULL;
}

bool CWeaponDynamiteBelt::ShouldDecrementAmmoOnThrow() const
{
	return false;
}

void CWeaponDynamiteBelt::PreparePrimaryPullback( CFoF_Player *pOwner )
{
	if ( pOwner )
		m_flOwnerAccuracy = pOwner->BeginFoFDynamiteBeltThrow();
}

float CWeaponDynamiteBelt::PrimaryDrawbackPredictionLeadTime() const
{
	return 0.2f;
}

float CWeaponDynamiteBelt::ThrowEventPredictionLeadTime() const
{
	return 0.0f;
}

float CWeaponDynamiteBelt::RedrawFallbackTime()
{
	if ( !m_bRedraw || m_flNextPrimaryAttack >= FLT_MAX )
		return 0.0f;
	return static_cast< float >( m_flNextPrimaryAttack ) +
		gpGlobals->interval_per_tick;
}

void CWeaponDynamiteBelt::Precache()
{
	BaseClass::Precache();
#ifndef CLIENT_DLL
	UTIL_PrecacheOther( ProjectileClassname() );
#endif
	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );
	PrecacheScriptSound( "Weapon_Dynamite.Burn" );
	m_bRedraw = false;
}

#ifdef CLIENT_DLL
int CWeaponDynamiteBelt::DrawModel( int flags )
{
	if ( !BaseClass::DrawModel( flags ) )
		return 0;
	if ( GetOwnerEntity() && !IsDormant() && m_AttackPaused != 0 )
		FoFDrawDynamiteFuseFX( this, IsBlackDynamite() );
	return 1;
}
#endif

bool CWeaponDynamiteBelt::Deploy()
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	m_flCountdown = 0.0f;
	m_flReleaseTime = 0.0f;
	m_flDrawbackFinishTime = 0.0f;
	m_flRedrawTime = 0.0f;
	m_AttackPaused = 0;

	CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->m_Local.m_iHideHUD |= HIDEHUD_CROSSHAIR;
		pOwner->RecalculateWeaponSpeed();
		CBaseViewModel *pViewModel = pOwner->GetViewModel( 0, true );
		if ( pViewModel )
			pViewModel->m_nSkin = IsBlackDynamite() ? 1 : 0;
	}

	const bool bResult = BaseClass::Deploy();
	return bResult;
}

bool CWeaponDynamiteBelt::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
	if ( ShouldThrowOnHolster() && m_flCountdown > 0.0f &&
		pOwner && pOwner->IsAlive() )
	{
		CompleteThrow( pOwner, EVENT_WEAPON_THROW );
	}
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	m_flReleaseTime = 0.0f;
	m_flDrawbackFinishTime = 0.0f;
	m_flRedrawTime = 0.0f;
	return BaseClass::Holster( pSwitchingTo );
}

#ifdef CLIENT_DLL
void CWeaponDynamiteBelt::SetDormant( bool bDormant )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	m_flReleaseTime = 0.0f;
	m_flDrawbackFinishTime = 0.0f;
	m_flRedrawTime = 0.0f;
	BaseClass::SetDormant( bDormant );
}
#endif

void CWeaponDynamiteBelt::PrimaryAttack()
{
	CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
	if ( m_bRedraw || !pOwner || !pOwner->IsAlive() )
		return;
	PreparePrimaryPullback( pOwner );
	m_flReleaseTime = gpGlobals->curtime + PrimaryReleaseDelay();
	m_flCountdown = gpGlobals->curtime + PrimaryCountdownDuration();
	m_AttackPaused = 1;
	SendWeaponAnim( PrimaryPullbackActivity() );
#ifdef CLIENT_DLL
	m_flDrawbackFinishTime = gpGlobals->curtime +
		GetViewModelSequenceDuration() -
		PrimaryDrawbackPredictionLeadTime();
#endif
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;
	if ( !HasPrimaryAmmo() )
		pOwner->SwitchToNextBestWeapon( this );
}

void CWeaponDynamiteBelt::SecondaryAttack()
{
}

void CWeaponDynamiteBelt::ItemPostFrame()
{
	CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
#ifdef CLIENT_DLL
	if ( m_AttackPaused != 0 && m_flDrawbackFinishTime <= 0.0f &&
		m_flReleaseTime > 0.0f && gpGlobals->curtime >= m_flReleaseTime &&
		pOwner && pOwner->IsAlive() )
	{
		CompleteThrow( pOwner, EVENT_WEAPON_THROW );
	}
	if ( !m_fDrawbackFinished && m_AttackPaused != 0 &&
		m_flDrawbackFinishTime > 0.0f &&
		gpGlobals->curtime >= m_flDrawbackFinishTime )
	{
		m_fDrawbackFinished = true;
	}
#else
	if ( m_flCountdown > 0.0f &&
		gpGlobals->curtime > m_flCountdown )
	{
		m_flCountdown = 0.0f;
		StopSound( "Weapon_Dynamite.Burn" );
		if ( pOwner && pOwner->IsAlive() )
		{
			SpawnFoFDeathDynamite( pOwner, pOwner, true );
			EntityMessageBegin( pOwner, true );
				WRITE_BYTE( 5 );
			MessageEnd();
		}
	}
	if ( m_flReleaseTime > 0.0f &&
		gpGlobals->curtime > m_flReleaseTime &&
		m_flCountdown > 0.0f )
	{
		CReliableBroadcastRecipientFilter filter;
		CBaseEntity::EmitSound(
			filter, entindex(), "Weapon_Dynamite.Burn" );
		m_flReleaseTime = 0.0f;
	}
#endif
	if ( m_fDrawbackFinished && pOwner && pOwner->IsAlive() )
	{
		if ( m_AttackPaused == 1 && ShouldReleasePrimaryThrow( pOwner ) )
		{
			BeginThrowAnimation( ACT_VM_THROW, 0.4f );
		}
		else if ( m_AttackPaused == 2 &&
			!( pOwner->m_nButtons & IN_ATTACK2 ) )
		{
			BeginThrowAnimation(
				( pOwner->m_nButtons & IN_DUCK ) ?
					ACT_VM_SECONDARYATTACK : ACT_VM_HAULBACK,
				0.23333333f );
		}
	}
	BaseClass::ItemPostFrame();
	float flRedrawTime = m_flRedrawTime;
	const float flFallbackRedrawTime = RedrawFallbackTime();
	if ( flFallbackRedrawTime > 0.0f &&
		( flRedrawTime <= 0.0f || flFallbackRedrawTime < flRedrawTime ) )
	{
		flRedrawTime = flFallbackRedrawTime;
	}
	if ( m_bRedraw && ( IsViewModelSequenceFinished() ||
		( flRedrawTime > 0.0f && gpGlobals->curtime >= flRedrawTime ) ) )
	{
		Reload();
	}
}

bool CWeaponDynamiteBelt::Reload()
{
	if ( !HasPrimaryAmmo() )
		return false;
	if ( m_bRedraw && gpGlobals->curtime >= m_flNextPrimaryAttack &&
		gpGlobals->curtime >= m_flNextSecondaryAttack )
	{
		SendWeaponAnim( ACT_VM_DRAW );
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
		m_bRedraw = false;
		m_flRedrawTime = 0.0f;
	}
	return true;
}

void CWeaponDynamiteBelt::BeginThrowAnimation(
	Activity activity, float flEventCycle )
{
	SendWeaponAnim( activity );
	m_fDrawbackFinished = false;
	m_flDrawbackFinishTime = 0.0f;
#ifdef CLIENT_DLL
	const float flSequenceDuration = GetViewModelSequenceDuration();
	m_flReleaseTime = gpGlobals->curtime +
		flSequenceDuration * flEventCycle -
		ThrowEventPredictionLeadTime();
	m_flRedrawTime = gpGlobals->curtime + flSequenceDuration;
#else
	NOTE_UNUSED( flEventCycle );
#endif
}

void CWeaponDynamiteBelt::CompleteThrow( CFoF_Player *pOwner, int nEvent )
{
#ifndef CLIENT_DLL
	SpawnDynamite( pOwner, nEvent );
#else
	NOTE_UNUSED( nEvent );
#endif
	ThrowPresentation( pOwner );
	if ( ShouldDecrementAmmoOnThrow() )
		DecrementAmmo( pOwner );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
	m_flTimeWeaponIdle = FLT_MAX;
	if ( pOwner->GetActiveWeapon() != this )
		m_iState = WEAPON_IS_CARRIED_BY_PLAYER;
}

void CWeaponDynamiteBelt::ThrowPresentation( CFoF_Player *pOwner )
{
#ifndef CLIENT_DLL
	StopSound( "Weapon_Dynamite.Burn" );
#endif
	WeaponSound( WPN_DOUBLE );
	pOwner->SetAnimation( PLAYER_ATTACK1 );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
	m_bRedraw = true;
	m_flCountdown = 0.0f;
	m_AttackPaused = 0;
	m_flReleaseTime = 0.0f;
	m_flDrawbackFinishTime = 0.0f;
}

void CWeaponDynamiteBelt::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	if ( !pOwner )
		return;
	const int nAmmoType = m_iPrimaryAmmoType;
	if ( GetAmmoDef()->MaxCarry( nAmmoType ) == INFINITE_AMMO )
	{
		m_iClip1 = pOwner->GetAmmoCount( nAmmoType );
		return;
	}
	if ( pOwner->GetAmmoCount( nAmmoType ) > 1 )
	{
		pOwner->RemoveAmmo( 1, nAmmoType );
		m_iClip1 = pOwner->GetAmmoCount( nAmmoType );
		return;
	}
	CBaseCombatWeapon *pFists =
		pOwner->Weapon_OwnsThisType( "weapon_fists", 0 );
	if ( !pFists )
		return;
	pOwner->RemoveAmmo( 1, nAmmoType );
	pOwner->Weapon_Switch( pFists );
}

#ifdef CLIENT_DLL
bool CWeaponDynamiteBelt::OnFireEvent( C_BaseViewModel *pViewModel,
	const Vector &origin, const QAngle &angles,
	int event, const char *options )
{
	if ( event == EVENT_WEAPON_SEQUENCE_FINISHED &&
		m_AttackPaused != 0 && m_flDrawbackFinishTime > 0.0f )
	{
		m_fDrawbackFinished = true;
		return true;
	}
	if ( ( event == EVENT_WEAPON_THROW || event == EVENT_WEAPON_THROW2 ||
		event == EVENT_WEAPON_THROW3 ) && m_AttackPaused != 0 && !m_bRedraw )
	{
		CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
		if ( pOwner && pOwner->IsAlive() )
			CompleteThrow( pOwner, event );
		return true;
	}
	return BaseClass::OnFireEvent(
		pViewModel, origin, angles, event, options );
}
#else
void CWeaponDynamiteBelt::Operator_HandleAnimEvent(
	animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CFoF_Player *pOwner = FoFDynamiteOwner( GetOwner() );
	if ( !pEvent || !pOwner || !pOwner->IsAlive() )
	{
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		return;
	}
	switch ( pEvent->event )
	{
	case EVENT_WEAPON_SEQUENCE_FINISHED:
		m_fDrawbackFinished = true;
		return;
	case EVENT_WEAPON_THROW:
	case EVENT_WEAPON_THROW2:
	case EVENT_WEAPON_THROW3:
		CompleteThrow( pOwner, pEvent->event );
		return;
	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		return;
	}
}

bool CWeaponDynamiteBelt::IsFoFPrimedDeathDynamite() const
{
	return IsBlackDynamite() && m_flCountdown > gpGlobals->curtime;
}

void CWeaponDynamiteBelt::SpawnFoFDeathDynamite(
	CFoF_Player *pOwner, CBaseEntity *pThrower, bool bBlastTriggered )
{
	if ( !pOwner )
		return;

	Vector vecForward;
	Vector vecRight;
	pOwner->EyeVectors( &vecForward, &vecRight, NULL );

	Vector vecSource = pOwner->EyePosition();
	Vector vecVelocity;
	pOwner->GetVelocity( &vecVelocity, NULL );
	float flTimer;
	if ( bBlastTriggered )
	{
		// An external explosion cooks off one carried stick almost
		// immediately. Black dynamite has priority in Event_Killed, and
		// clearing both ammo pools prevents a second normal stick spawning.
		vecSource.z -= 40.0f;
		vecVelocity += vecForward * 0.25f;
		flTimer = random->RandomFloat( 0.01f, 0.2f );
	}
	else
	{
		// Normal dynamite is released by its Holster path. Black dynamite
		// deliberately stays in hand, so the shipped death path gives its
		// lit stick this separate short toss.
		vecSource += vecForward * 18.0f + vecRight * 16.0f;
		vecSource.z -= 35.0f;
		vecVelocity += vecForward * 35.0f;
		flTimer = static_cast< float >( random->RandomInt( 3, 4 ) );
		pOwner->EmitSound( "Weapon_Dynamite.Burn" );
	}

	const int nGrenadeAmmo = GetAmmoDef()->Index( "Grenade" );
	if ( nGrenadeAmmo >= 0 )
		pOwner->SetAmmoCount( 0, nGrenadeAmmo );
	const int nBlackAmmo = GetAmmoDef()->Index( "Dynamite_B" );
	if ( nBlackAmmo >= 0 )
		pOwner->SetAmmoCount( 0, nBlackAmmo );

	const AngularImpulse angVelocity( 20.0f,
		static_cast< float >( random->RandomInt( -60, 60 ) ), 0.0f );
	FoFCreateDynamiteProjectile( ProjectileClassname(), vecSource,
		vec3_angle, vecVelocity, angVelocity, pThrower, flTimer );
}

void CWeaponDynamiteBelt::SpawnDynamite( CFoF_Player *pOwner, int nEvent )
{
	if ( !pOwner )
		return;
	NOTE_UNUSED( nEvent );

	Vector vecForward;
	Vector vecRight;
	pOwner->EyeVectors( &vecForward, &vecRight, NULL );
	Vector vecSource = pOwner->EyePosition() +
		vecForward * 18.0f + vecRight * 16.0f;
	FoFCheckDynamiteThrowPosition(
		pOwner, pOwner->EyePosition(), vecSource );

	Vector vecVelocity;
	if ( !FoFConsumeBotDynamiteThrowVelocity( pOwner, vecVelocity ) )
	{
		vecForward.z += 0.1f;
		pOwner->GetVelocity( &vecVelocity, NULL );
		vecVelocity += vecForward * 700.0f;
	}
	const AngularImpulse angVelocity( 600.0f,
		random->RandomInt( -1200, 1200 ), 0.0f );

	FoFCreateDynamiteProjectile( ProjectileClassname(), vecSource,
		vec3_angle, vecVelocity, angVelocity, pOwner,
		MAX( 0.05f, m_flCountdown - gpGlobals->curtime ) );
}
#endif
IMPLEMENT_NETWORKCLASS_ALIASED( WeaponDynamiteBelt, DT_WeaponDynamiteBelt )

BEGIN_NETWORK_TABLE( CWeaponDynamiteBelt, DT_WeaponDynamiteBelt )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bRedraw ) ),
	RecvPropBool( RECVINFO( m_fDrawbackFinished ) ),
	RecvPropInt( RECVINFO( m_AttackPaused ) ),
	RecvPropFloat( RECVINFO( m_flOwnerAccuracy ) ),
	RecvPropFloat( RECVINFO( m_flCountdown ) ),
#else
	SendPropBool( SENDINFO( m_bRedraw ) ),
	SendPropBool( SENDINFO( m_fDrawbackFinished ) ),
	SendPropInt( SENDINFO( m_AttackPaused ) ),
	SendPropFloat( SENDINFO( m_flOwnerAccuracy ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flCountdown ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponDynamiteBelt )
	DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fDrawbackFinished, FIELD_BOOLEAN,
		FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_AttackPaused, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCountdown, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flReleaseTime, FIELD_FLOAT, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flDrawbackFinishTime, FIELD_FLOAT,
		FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flRedrawTime, FIELD_FLOAT, FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flOwnerAccuracy, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_dynamite_belt, CWeaponDynamiteBelt )
#else
LINK_ENTITY_TO_CLASS( weapon_dynamite_belt, CWeaponDynamiteBelt );
PRECACHE_WEAPON_REGISTER( weapon_dynamite_belt );
#endif
