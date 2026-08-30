#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#include "prediction.h"
#include "util_shared.h"
#else
#include "ilagcompensationmanager.h"
#endif
#include "fof/weapon_fists_ghost.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
static bool FoFFistImpactWater(
	CBaseEntity *pIgnore,
	const Vector &vecStart,
	const Vector &vecEnd )
{
	if ( UTIL_PointContents( vecStart ) &
		( CONTENTS_WATER | CONTENTS_SLIME ) )
	{
		return false;
	}
	if ( !( UTIL_PointContents( vecEnd ) &
		( CONTENTS_WATER | CONTENTS_SLIME ) ) )
	{
		return false;
	}

	trace_t waterTrace;
	UTIL_TraceLine(
		vecStart,
		vecEnd,
		CONTENTS_WATER | CONTENTS_SLIME,
		pIgnore,
		COLLISION_GROUP_NONE,
		&waterTrace );
	return waterTrace.fraction < 1.0f;
}
#else
static CHL2MP_Player *FoFStartFistLagCompensation(
	CBasePlayer *pOwner )
{
	CHL2MP_Player *pLagPlayer = ToHL2MPPlayer( pOwner );
	if ( pLagPlayer )
	{
		lagcompensation->StartLagCompensation(
			pLagPlayer, pLagPlayer->GetCurrentCommand() );
	}
	return pLagPlayer;
}

static void FoFFinishFistLagCompensation(
	CHL2MP_Player *pLagPlayer )
{
	if ( pLagPlayer )
		lagcompensation->FinishLagCompensation( pLagPlayer );
}

static void FoFApplyFistDamage(
	CBaseCombatWeapon *pWeapon,
	CBasePlayer *pOwner,
	trace_t &traceHit,
	float flDamage )
{
	CBaseEntity *pHitEntity = traceHit.m_pEnt;
	if ( !pWeapon || !pOwner || !pHitEntity )
		return;

	Vector vecDirection;
	pOwner->EyeVectors( &vecDirection, NULL, NULL );
	VectorNormalize( vecDirection );

	CTakeDamageInfo info( pOwner, pOwner, flDamage, DMG_CLUB );
	info.SetWeapon( pWeapon );
	if ( pHitEntity->IsNPC() )
		info.AdjustPlayerDamageInflictedForSkillLevel();
	CalculateMeleeDamageForce( &info, vecDirection, traceHit.endpos );
	pHitEntity->DispatchTraceAttack( info, vecDirection, &traceHit );
	ApplyMultiDamage();
	pWeapon->TraceAttackToTriggers(
		info, traceHit.startpos, traceHit.endpos, vecDirection );
}
#endif

CWeaponFistsGhost::CWeaponFistsGhost()
{
	m_nDelayedActivity = ACT_INVALID;
	m_flLastAttackTime = 0.0f;
	m_flLastAttackDuration = 0.0f;
	nLastActivity = 0;
	m_flHitDelay = 0.0f;
	AddEffects( EF_ITEM_BLINK );
}

void CWeaponFistsGhost::PrimaryAttack()
{
	BeginFistSwing( false );
}

void CWeaponFistsGhost::SecondaryAttack()
{
	BeginFistSwing( true );
}

void CWeaponFistsGhost::BeginFistSwing(
	bool bSecondary )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;
	if ( !IsGhostFists() && bSecondary &&
		( pOwner->m_nButtons & IN_ATTACK ) )
	{
		return;
	}

	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( !pFoFOwner )
		return;
	if ( !IsGhostFists() )
		pFoFOwner->SetFoFMeleeActionState( true );

	m_nDelayedActivity =
		static_cast< Activity >( bSecondary ? 0xC0 : 0xBF );
	m_flHitDelay =
		gpGlobals->curtime + ( bSecondary ? 0.5f : 0.25f );
	const Activity modelActivity = bSecondary ?
		FoFModelActivity(
			this, "ACT_VM_HITCENTER2", ACT_VM_HITCENTER2 ) :
		FoFModelActivity(
			this, "ACT_VM_HITCENTER", ACT_VM_HITCENTER );
	SendWeaponAnim( modelActivity );
	pOwner->SetAnimation(
		static_cast< PLAYER_ANIM >( bSecondary ? 5 : 6 ) );
#ifdef CLIENT_DLL
	pFoFOwner->DoAnimationEvent(
		bSecondary ?
			PLAYERANIMEVENT_ATTACK_PRIMARY :
			PLAYERANIMEVENT_ATTACK_SECONDARY,
		0 );
#else
	if ( pOwner->IsBot() )
	{
		pFoFOwner->DoAnimationEvent(
			bSecondary ?
				PLAYERANIMEVENT_ATTACK_PRIMARY :
				PLAYERANIMEVENT_ATTACK_SECONDARY,
			0 );
	}
#endif

	const float flNextAttack =
		gpGlobals->curtime + SequenceDuration() * 0.75f;
	m_flNextPrimaryAttack = flNextAttack;
	m_flNextSecondaryAttack = flNextAttack;
}

void CWeaponFistsGhost::ResolveFistHit()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

#ifndef CLIENT_DLL
	CHL2MP_Player *pLagPlayer =
		FoFStartFistLagCompensation( pOwner );
#endif

	Vector vecForward;
	pOwner->EyeVectors( &vecForward, NULL, NULL );
	const Vector vecStart = pOwner->Weapon_ShootPosition();
	const Vector vecFullEnd = vecStart + vecForward * 55.0f;
	Vector vecEnd = vecFullEnd;
	trace_t traceHit;
	UTIL_TraceLine(
		vecStart, vecEnd, MASK_SHOT, pOwner,
		COLLISION_GROUP_NONE, &traceHit );
	if ( traceHit.fraction == 1.0f )
	{
		vecEnd -= vecForward * 27.712f;
		UTIL_TraceHull(
			vecStart, vecEnd,
			Vector( -16.0f, -16.0f, -16.0f ),
			Vector( 16.0f, 16.0f, 16.0f ),
			MASK_SHOT_HULL,
			pOwner,
			COLLISION_GROUP_NONE,
			&traceHit );
	}

#ifdef CLIENT_DLL
	const bool bFirstPrediction =
		!prediction || prediction->IsFirstTimePredicted();
	if ( bFirstPrediction )
		WeaponSound( SINGLE );

	if ( traceHit.fraction == 1.0f )
	{
		if ( bFirstPrediction )
			FoFFistImpactWater( pOwner, vecStart, vecFullEnd );
		return;
	}

	if ( bFirstPrediction )
	{
		if ( IsGhostFists() )
		{
			WeaponSound(
				traceHit.m_pEnt && traceHit.m_pEnt->IsPlayer() ?
					MELEE_HIT : MELEE_HIT_WORLD );
		}
		else
		{
			CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
			const bool bBrassKnuckles =
				pFoFOwner &&
				( pFoFOwner->GetFoFPlayerInfo() & 0x4 );
			if ( traceHit.m_pEnt && traceHit.m_pEnt->IsPlayer() )
			{
				if ( bBrassKnuckles )
					WeaponSound( SPECIAL2 );
				else if (
					static_cast< int >( m_nDelayedActivity ) == 0xBF )
				{
					WeaponSound( SPECIAL1 );
				}
				else
					WeaponSound( MELEE_HIT );
			}
			else
			{
				WeaponSound(
					bBrassKnuckles ?
						SPECIAL3 : MELEE_HIT_WORLD );
			}
		}

		if ( !FoFFistImpactWater(
			pOwner, traceHit.startpos, traceHit.endpos ) )
		{
			UTIL_ImpactTrace( &traceHit, DMG_CLUB );
		}
	}
#else
	WeaponSound( SINGLE );
	if ( traceHit.fraction < 1.0f )
	{
		const bool bSecondary =
			static_cast< int >( m_nDelayedActivity ) == 0xC0;
		const Activity damageActivity = bSecondary ?
			FoFModelActivity(
				this, "ACT_VM_HITCENTER2", ACT_VM_HITCENTER2 ) :
			FoFModelActivity(
				this, "ACT_VM_HITCENTER", ACT_VM_HITCENTER );
		FoFApplyFistDamage(
			this, pOwner, traceHit,
			GetDamageForActivity( damageActivity ) );

		if ( IsGhostFists() )
		{
			WeaponSound(
				traceHit.m_pEnt && traceHit.m_pEnt->IsPlayer() ?
					MELEE_HIT : MELEE_HIT_WORLD );
		}
		else
		{
			CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
			const bool bBrassKnuckles =
				pFoFOwner &&
				( pFoFOwner->GetFoFPlayerInfo() & 0x4 );
			if ( traceHit.m_pEnt && traceHit.m_pEnt->IsPlayer() )
			{
				if ( bBrassKnuckles )
					WeaponSound( SPECIAL2 );
				else if ( !bSecondary )
					WeaponSound( SPECIAL1 );
				else
					WeaponSound( MELEE_HIT );
			}
			else
			{
				WeaponSound(
					bBrassKnuckles ?
						SPECIAL3 : MELEE_HIT_WORLD );
			}
		}
		ImpactEffect( traceHit );
	}

	FoFFinishFistLagCompensation( pLagPlayer );
#endif

	if ( traceHit.fraction < 1.0f )
		nLastActivity = static_cast< int >( m_nDelayedActivity );
}

void CWeaponFistsGhost::ItemPostFrame()
{
	BaseClass::ItemPostFrame();
	if ( m_flHitDelay == 0.0f ||
		m_flHitDelay > gpGlobals->curtime )
	{
		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	m_flHitDelay = 0.0f;
	ResolveFistHit();
	if ( pOwner->m_nButtons & IN_ATTACK )
		m_flNextPrimaryAttack += 0.35f;
	else if ( pOwner->m_nButtons & IN_ATTACK2 )
		m_flNextSecondaryAttack += 0.35f;

	if ( !IsGhostFists() )
	{
		CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
		if ( pFoFOwner )
			pFoFOwner->SetFoFMeleeActionState( false );
	}
}

bool CWeaponFistsGhost::Deploy()
{
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->m_Local.m_iHideHUD |= HIDEHUD_CROSSHAIR;
		pOwner->RecalculateWeaponSpeed();
		if ( !IsGhostFists() )
		{
			m_flHitDelay = 0.0f;
#ifndef CLIENT_DLL
			pOwner->SetFoFMeleeActionState( false );
#endif
		}
	}
	return BaseClass::Deploy();
}

bool CWeaponFistsGhost::Holster(
	CBaseCombatWeapon *pSwitchingTo )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	CBaseViewModel *pViewModel =
		pOwner && pOwner->IsAlive() ?
			pOwner->GetViewModel( 0, true ) : NULL;
	if ( pViewModel &&
#ifdef CLIENT_DLL
		!pViewModel->IsDormant() &&
#endif
		pViewModel->GetModelPtr() )
	{
		pViewModel->SetBodygroup( 1, 0 );
	}
	return BaseClass::Holster( pSwitchingTo );
}

float CWeaponFistsGhost::GetRange()
{
	return 55.0f;
}

float CWeaponFistsGhost::GetFireRate()
{
	return 0.5f;
}

float CWeaponFistsGhost::GetDamageForActivity(
	Activity activity )
{
	if ( IsGhostFists() )
	{
		if ( activity == ACT_VM_HITCENTER )
			return 15.0f;
		if ( activity == ACT_VM_HITCENTER2 )
			return 25.0f;
		return 10.0f;
	}

	float flDamage =
		( activity == ACT_VM_HITRIGHT ||
			activity == ACT_VM_HITCENTER2 ) ?
			20.0f : 13.0f;
	CFoF_Player *pFoFOwner = ToFoFPlayer( GetOwner() );
	if ( pFoFOwner &&
		( pFoFOwner->GetFoFPlayerInfo() & 0x4 ) )
	{
		flDamage *= 1.5f;
	}
	return flDamage;
}

void CWeaponFistsGhost::AddViewKick()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->ViewPunch( QAngle(
			random->RandomFloat( 4.0f, 6.0f ),
			random->RandomFloat( -2.0f, -1.0f ),
			0.0f ) );
	}
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFistsGhost, DT_WeaponFistsGhost )

BEGIN_NETWORK_TABLE( CWeaponFistsGhost, DT_WeaponFistsGhost )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flLastAttackTime ) ),
	RecvPropFloat( RECVINFO( m_flLastAttackDuration ) ),
	RecvPropInt( RECVINFO( nLastActivity ) ),
	RecvPropFloat( RECVINFO( m_flHitDelay ) ),
#else
	SendPropFloat( SENDINFO( m_flLastAttackTime ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flLastAttackDuration ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( nLastActivity ) ),
	SendPropFloat( SENDINFO( m_flHitDelay ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponFistsGhost )
	DEFINE_PRED_FIELD( m_flLastAttackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastAttackDuration, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( nLastActivity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flHitDelay, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_fists_ghost, CWeaponFistsGhost )
#else
LINK_ENTITY_TO_CLASS( weapon_fists_ghost, CWeaponFistsGhost );
PRECACHE_WEAPON_REGISTER( weapon_fists_ghost );
#endif
