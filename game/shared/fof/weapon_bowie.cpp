#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/weapon_bowie.h"

#ifdef CLIENT_DLL
#include "fof/fof_player_activities.h"
#include "fof/fof_weapon_classmap.h"
#else
#include "fof/fof_projectiles.h"
#include "fof/fof_player_statistics.h"
#include "ilagcompensationmanager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
static const Vector s_FoFBowieHullMins( -16.0f, -16.0f, -16.0f );
static const Vector s_FoFBowieHullMaxs( 16.0f, 16.0f, 16.0f );

#ifndef CLIENT_DLL
static CHL2MP_Player *FoFStartBowieLagCompensation( CBasePlayer *pOwner )
{
	CHL2MP_Player *pLagPlayer = ToHL2MPPlayer( pOwner );
	if ( pLagPlayer )
	{
		lagcompensation->StartLagCompensation(
			pLagPlayer, pLagPlayer->GetCurrentCommand() );
	}
	return pLagPlayer;
}

static void FoFFinishBowieLagCompensation( CHL2MP_Player *pLagPlayer )
{
	if ( pLagPlayer )
		lagcompensation->FinishLagCompensation( pLagPlayer );
}

static void FoFApplyBowieDamage(
	CWeaponBowie *pWeapon,
	CBasePlayer *pOwner,
	trace_t &traceHit )
{
	CBaseEntity *pHitEntity = traceHit.m_pEnt;
	if ( !pWeapon || !pOwner || !pHitEntity )
		return;
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	CFoF_Player *pTarget = ToFoFPlayer( pHitEntity );
	if ( pFoFOwner && pTarget &&
		FoFPlayersAreEnemies( pFoFOwner, pTarget ) )
	{
		FoFRecordAccuracyHit( pFoFOwner, 30.0f );
	}

	Vector vecDirection;
	pOwner->EyeVectors( &vecDirection, NULL, NULL );
	VectorNormalize( vecDirection );

	CTakeDamageInfo info( pOwner, pOwner, 30.0f, DMG_CLUB );
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

static void FoFRefineBowieHullTrace(
	trace_t &traceHit,
	const Vector &vecStart,
	CBasePlayer *pOwner )
{
	const float *pMinMaxs[2] =
	{
		s_FoFBowieHullMins.Base(), s_FoFBowieHullMaxs.Base()
	};
	const Vector vecHullEnd =
		vecStart + ( traceHit.endpos - vecStart ) * 2.0f;

	trace_t bestTrace;
	UTIL_TraceLine(
		vecStart, vecHullEnd, MASK_SHOT_HULL, pOwner,
		COLLISION_GROUP_NONE, &bestTrace );
	if ( bestTrace.fraction < 1.0f )
	{
		traceHit = bestTrace;
		return;
	}

	float flBestDistance = 1.0e6f;
	for ( int x = 0; x < 2; ++x )
	{
		for ( int y = 0; y < 2; ++y )
		{
			for ( int z = 0; z < 2; ++z )
			{
				const Vector vecEnd(
					vecHullEnd.x + pMinMaxs[x][0],
					vecHullEnd.y + pMinMaxs[y][1],
					vecHullEnd.z + pMinMaxs[z][2] );
				trace_t candidate;
				UTIL_TraceLine(
					vecStart, vecEnd, MASK_SHOT_HULL, pOwner,
					COLLISION_GROUP_NONE, &candidate );
				if ( candidate.fraction < 1.0f )
				{
					const float flDistance =
						( candidate.endpos - vecStart ).Length();
					if ( flDistance < flBestDistance )
					{
						traceHit = candidate;
						flBestDistance = flDistance;
					}
				}
			}
		}
	}
}

static void FoFTraceBowie(
	CBasePlayer *pOwner,
	float flRange,
	trace_t &traceHit )
{
	Vector vecForward;
	pOwner->EyeVectors( &vecForward, NULL, NULL );
	const Vector vecStart = pOwner->Weapon_ShootPosition();
	Vector vecEnd = vecStart + vecForward * flRange;

	UTIL_TraceLine(
		vecStart, vecEnd, MASK_SHOT_HULL, pOwner,
		COLLISION_GROUP_NONE, &traceHit );
	if ( traceHit.fraction != 1.0f )
		return;

	vecEnd -= vecForward * ( 1.732f * 16.0f );
	UTIL_TraceHull(
		vecStart, vecEnd, s_FoFBowieHullMins, s_FoFBowieHullMaxs,
		MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	if ( traceHit.fraction >= 1.0f || !traceHit.m_pEnt )
		return;

	Vector vecToTarget = traceHit.m_pEnt->GetAbsOrigin() - vecStart;
	VectorNormalize( vecToTarget );
	if ( vecToTarget.Dot( vecForward ) < 0.70721f )
		traceHit.fraction = 1.0f;
	else
		FoFRefineBowieHullTrace( traceHit, vecStart, pOwner );
}

static Activity FoFBowieAttackActivity(
	CWeaponBowie *pWeapon,
	CBasePlayer *pOwner,
	trace_t &traceHit )
{
	pWeapon->WeaponSound( SINGLE );
	if ( traceHit.fraction == 1.0f )
	{
		return FoFModelActivity(
			pWeapon, "ACT_VM_MISSCENTER", ACT_VM_MISSCENTER );
	}

#ifdef CLIENT_DLL
	if ( traceHit.m_pEnt && !traceHit.m_pEnt->IsPlayer() )
		pWeapon->WeaponSound( MELEE_HIT_WORLD );
#else
	FoFApplyBowieDamage( pWeapon, pOwner, traceHit );
	pWeapon->WeaponSound(
		traceHit.m_pEnt && traceHit.m_pEnt->IsPlayer() ?
			MELEE_HIT : MELEE_HIT_WORLD );
#endif

	return FoFModelActivity(
		pWeapon, "ACT_VM_HITCENTER", ACT_VM_HITCENTER );
}

static void FoFFinishBowieSwing(
	CWeaponBowie *pWeapon,
	CBasePlayer *pOwner,
	Activity attackActivity )
{
	pWeapon->SendWeaponAnim( attackActivity );
	pOwner->SetAnimation( PLAYER_ATTACK1 );
#ifdef CLIENT_DLL
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( pFoFOwner )
	{
		pFoFOwner->DoAnimationEvent(
			PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
	}
#else
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( pFoFOwner && pOwner->IsBot() )
	{
		pFoFOwner->DoAnimationEvent(
			PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
	}
#endif

	const float flNextAttack =
		gpGlobals->curtime + pWeapon->SequenceDuration();
	pWeapon->m_flNextPrimaryAttack = flNextAttack;
	pWeapon->m_flNextSecondaryAttack = flNextAttack;
}

CWeaponBowie::CWeaponBowie()
{
	m_flDelayedFire = 0.0f;
	m_bShotDelayed = false;
	AddEffects( EF_ITEM_BLINK );
}

void CWeaponBowie::PrimaryAttack()
{
	if ( m_flNextSecondaryAttack <= gpGlobals->curtime && !m_bShotDelayed )
		FoFPrimarySwing();
}

void CWeaponBowie::FoFPrimarySwing()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;
#ifndef CLIENT_DLL
	if ( !pOwner->IsAlive() )
		return;
	FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	CHL2MP_Player *pLagPlayer = FoFStartBowieLagCompensation( pOwner );
#endif

	trace_t traceHit;
	FoFTraceBowie( pOwner, GetRange(), traceHit );
	const Activity attackActivity =
		FoFBowieAttackActivity( this, pOwner, traceHit );
	if ( traceHit.fraction != 1.0f )
		ImpactEffect( traceHit );
#ifndef CLIENT_DLL
	FoFFinishBowieLagCompensation( pLagPlayer );
#endif
	FoFFinishBowieSwing( this, pOwner, attackActivity );
}

void CWeaponBowie::SecondaryAttack()
{
	const float flNow = gpGlobals->curtime;
	if ( m_flNextSecondaryAttack > flNow || m_bShotDelayed ||
		m_flNextPrimaryAttack > flNow )
	{
		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	m_bShotDelayed = true;
	m_flDelayedFire = flNow + 0.5f;
	m_flNextPrimaryAttack = m_flDelayedFire;
	m_flNextSecondaryAttack = flNow + 2.0f;
	pOwner->SetNextAttack( m_flDelayedFire );
	SendWeaponAnim( FoFModelActivity(
		this, "ACT_VM_SECONDARYATTACK", static_cast< Activity >( 181 ) ) );

	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	if ( pFoFOwner )
		pFoFOwner->SetFoFMeleeActionState( true );
	pOwner->ViewPunch( QAngle( -2.0f, 2.0f, -2.0f ) );
	WeaponSound( SPECIAL1 );

#ifdef CLIENT_DLL
	if ( pFoFOwner )
	{
		pFoFOwner->DoAnimationEvent(
			PLAYERANIMEVENT_ATTACK_SECONDARY, 0 );
	}
#else
	if ( pFoFOwner && pOwner->IsBot() )
	{
		pFoFOwner->DoAnimationEvent(
			PLAYERANIMEVENT_ATTACK_SECONDARY, 0 );
	}
#endif
}

void CWeaponBowie::ItemPostFrame()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	const float flNow = gpGlobals->curtime;
	if ( m_flDelayedFire != 0.0f &&
		flNow > m_flDelayedFire && m_bShotDelayed )
	{
#ifndef CLIENT_DLL
		Vector vecForward, vecRight, vecUp;
		pOwner->EyeVectors( &vecForward, &vecRight, &vecUp );
		const Vector vecOrigin = pOwner->Weapon_ShootPosition() +
			vecForward * 25.0f + vecRight * 5.7f;
		QAngle angProjectile;
		VectorAngles( vecForward, angProjectile );
		angProjectile.x += 90.0f;
		CKnifeBolt *pProjectile = CKnifeBolt::BoltCreate(
			vecOrigin, angProjectile, 35, pOwner );
		if ( pProjectile )
			pProjectile->SetAbsVelocity( vecForward * 850.0f );
#endif

		m_bShotDelayed = false;
		CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
		if ( pFoFOwner )
			pFoFOwner->SetFoFMeleeActionState( false );
		m_flDelayedFire = flNow + 0.4f;

#ifndef CLIENT_DLL
		const int iAmmoType = GetSecondaryAmmoType();
		if ( iAmmoType >= 0 && pOwner->GetAmmoCount( iAmmoType ) > 0 )
		{
			pOwner->RemoveAmmo( 1, iAmmoType );
		}
		else
		{
			pOwner->Weapon_Switch(
				pOwner->Weapon_OwnsThisType( "weapon_fists" ) );
			UTIL_Remove( this );
			return;
		}
#endif
	}

	if ( m_flDelayedFire != 0.0f &&
		flNow > m_flDelayedFire && !m_bShotDelayed )
	{
		Deploy();
		m_flDelayedFire = 0.0f;
	}

	BaseClass::ItemPostFrame();
}

bool CWeaponBowie::Deploy()
{
	m_flDelayedFire = 0.0f;
	m_bShotDelayed = false;
	CFoF_Player *pOwner = ToFoFPlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pOwner->RecalculateWeaponSpeed();
#ifndef CLIENT_DLL
		pOwner->SetFoFMeleeActionState( false );
#endif
	}
	return BaseClass::Deploy();
}

bool CWeaponBowie::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	return BaseClass::Holster( pSwitchingTo );
}

float CWeaponBowie::GetRange()
{
	return 55.0f;
}

float CWeaponBowie::GetFireRate()
{
	return 0.8f;
}

float CWeaponBowie::GetDamageForActivity( Activity )
{
	return 30.0f;
}

void CWeaponBowie::AddViewKick()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->IsAlive() )
	{
		pOwner->ViewPunch( QAngle(
			random->RandomFloat( 4.0f, 6.0f ),
			random->RandomFloat( -2.0f, -1.0f ), 0.0f ) );
	}
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBowie, DT_WeaponBowie )

BEGIN_NETWORK_TABLE( CWeaponBowie, DT_WeaponBowie )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bShotDelayed ) ),
	RecvPropFloat( RECVINFO( m_flDelayedFire ) ),
#else
	SendPropBool( SENDINFO( m_bShotDelayed ) ),
	SendPropFloat( SENDINFO( m_flDelayedFire ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponBowie )
	DEFINE_PRED_FIELD( m_flDelayedFire, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bShotDelayed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_knife, CWeaponBowie )
#else
LINK_ENTITY_TO_CLASS( weapon_knife, CWeaponBowie );
PRECACHE_WEAPON_REGISTER( weapon_knife );
#endif
