//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF horse mounting and dismounting.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_player.h"
#include "fof/fof_gamerules.h"
#include "fof/fof_horse.h"
#include "in_buttons.h"
#include "recipientfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoF_Horse *FoFHorseFromEntity( CBaseEntity *pEntity )
{
	while ( pEntity )
	{
		CFoF_Horse *pHorse = dynamic_cast< CFoF_Horse * >( pEntity );
		if ( pHorse )
			return pHorse;
		pEntity = pEntity->GetMoveParent();
	}
	return NULL;
}

static bool FoFCanSeeHorse(
	CFoF_Player *pPlayer, CFoF_Horse *pHorse )
{
	CTraceFilterSkipTwoEntities filter(
		pPlayer, pHorse, COLLISION_GROUP_NONE );
	trace_t trace;
	UTIL_TraceLine(
		pPlayer->EyePosition(), pHorse->WorldSpaceCenter(),
		MASK_SOLID, &filter, &trace );
	return trace.fraction == 1.0f;
}

bool CFoF_Player::TryMountFoFHorse()
{
	if ( !IsAlive() || IsObserver() || IsOnFoFHorse() ||
		( GetFlags() & ( FL_DUCKING | FL_ANIMDUCKING ) ) )
	{
		return false;
	}

	Vector forward;
	EyeVectors( &forward );
	trace_t trace;
	UTIL_TraceLine(
		EyePosition(), EyePosition() + forward * 75.0f,
		MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	CFoF_Horse *pHorse = FoFHorseFromEntity( trace.m_pEnt );

	if ( !pHorse )
	{
		CBaseEntity *pEntity = NULL;
		float flBestDistanceSqr = Square( 96.0f );
		while ( ( pEntity = gEntList.FindEntityByClassnameWithin(
			pEntity, "fof_horse", GetAbsOrigin(), 96.0f ) ) != NULL )
		{
			CFoF_Horse *pCandidate = dynamic_cast< CFoF_Horse * >( pEntity );
			if ( !pCandidate || !pCandidate->IsAvailableForMount() ||
				!FoFCanSeeHorse( this, pCandidate ) )
			{
				continue;
			}

			const float flDistanceSqr = GetAbsOrigin().DistToSqr(
				pCandidate->GetAbsOrigin() );
			if ( flDistanceSqr < flBestDistanceSqr )
			{
				flBestDistanceSqr = flDistanceSqr;
				pHorse = pCandidate;
			}
		}
	}

	if ( !pHorse || !pHorse->IsAvailableForMount() ||
		!FoFCanSeeHorse( this, pHorse ) )
	{
		return false;
	}

	const float flScale = GetModelScale();
	CTraceFilterSkipTwoEntities mountFilter(
		this, pHorse, COLLISION_GROUP_PLAYER_MOVEMENT );
	Ray_t mountRay;
	mountRay.Init(
		GetAbsOrigin(), GetAbsOrigin(),
		Vector( -16.0f, -16.0f, 0.0f ) * flScale,
		Vector( 16.0f, 16.0f, 100.0f ) * flScale );
	trace_t mountTrace;
	enginetrace->TraceRay(
		mountRay, MASK_PLAYERSOLID, &mountFilter, &mountTrace );
	if ( mountTrace.startsolid || mountTrace.allsolid )
	{
		EmitSound( "HL2Player.UseDeny" );
		ClientPrint( this, HUD_PRINTTALK,
			"FoFPlayer_CantRideWarning" );
		m_flNextPickupInteraction = gpGlobals->curtime + 0.5f;
		return false;
	}

	RemoveFlag( FL_DUCKING | FL_ANIMDUCKING );
	m_Local.m_bDucked = false;
	m_Local.m_bDucking = false;
	m_Local.m_flDucktime = 0.0f;
	m_Local.m_flDuckJumpTime = 0.0f;
	m_Local.m_flJumpTime = 0.0f;

	SetFoFOnHorse( true );
	SetFoFHorseAcceleration( 0.75f );
	m_flFoFHorseDismountHold = 0.0f;
	m_hFoFHorse = pHorse;

	QAngle horseAngles( 0.0f, EyeAngles()[YAW], 0.0f );
	SetFoFHorseAngles( horseAngles );
	pHorse->SetAbsAngles( QAngle( 0.0f, horseAngles[YAW] + 180.0f, 0.0f ) );
	const int nSaddleAttachment = LookupAttachment( "saddle" );
	pHorse->SetParent( this, nSaddleAttachment > 0 ? nSaddleAttachment : -1 );
	pHorse->SetLocalOrigin( vec3_origin );
	pHorse->AddEffects( EF_BONEMERGE_FASTCULL );
	pHorse->EnterMountedState( this );
	if ( gpGlobals->curtime >= m_flNextFoFHorseHintTime )
	{
		CSingleUserRecipientFilter filter( this );
		filter.MakeReliable();
		UserMessageBegin( filter, "BBNotices" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "#horse_riding_sprint" );
		MessageEnd();
		m_flNextFoFHorseHintTime = gpGlobals->curtime + 30.0f;
	}
	RecalculateWeaponSpeed();
	return true;
}

bool CFoF_Player::FindFoFHorseDismountPosition(
	CFoF_Horse *pHorse, Vector &vecDismountPosition )
{
	if ( !pHorse )
		return false;

	Vector forward, right;
	AngleVectors( QAngle( 0.0f, EyeAngles()[YAW], 0.0f ),
		&forward, &right, NULL );
	const Vector vecOrigin = GetAbsOrigin();
	const Vector candidates[] =
	{
		vecOrigin + right * 56.0f,
		vecOrigin - right * 56.0f,
		vecOrigin - forward * 56.0f,
		vecOrigin + forward * 56.0f,
		vecOrigin
	};

	CTraceFilterSkipTwoEntities filter(
		this, pHorse, COLLISION_GROUP_PLAYER_MOVEMENT );
	for ( int i = 0; i < ARRAYSIZE( candidates ); ++i )
	{
		Ray_t ray;
		ray.Init(
			candidates[i], candidates[i],
			VEC_HULL_MIN_SCALED( this ),
			VEC_HULL_MAX_SCALED( this ) );
		trace_t trace;
		enginetrace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );
		if ( !trace.startsolid && !trace.allsolid )
		{
			vecDismountPosition = candidates[i];
			return true;
		}
	}

	return false;
}

void CFoF_Player::DismountFoFHorse( bool bForced )
{
	CFoF_Horse *pHorse = m_hFoFHorse.Get();
	if ( !IsOnFoFHorse() && !pHorse )
		return;

	Vector vecDismountPosition = GetAbsOrigin();
	const bool bFoundDismount = pHorse &&
		FindFoFHorseDismountPosition( pHorse, vecDismountPosition );
	if ( !bFoundDismount && !bForced )
		return;

	SetFoFOnHorse( false );
	SetFoFHorseAcceleration( 0.75f );
	m_flFoFHorseDismountHold = 0.0f;
	m_hFoFHorse = NULL;

	if ( pHorse )
	{
		pHorse->SetParent( NULL );
		pHorse->RemoveEffects( EF_BONEMERGE );
		pHorse->RemoveEffects( EF_BONEMERGE_FASTCULL );
		pHorse->LeaveMountedState();
	}

	SetCollisionBounds(
		VEC_HULL_MIN_SCALED( this ),
		VEC_HULL_MAX_SCALED( this ) );
	SetViewOffset( VEC_VIEW_SCALED( this ) );
	if ( bFoundDismount )
		SetAbsOrigin( vecDismountPosition );
	RecalculateWeaponSpeed();
}

void CFoF_Player::UpdateMountedFoFHorse()
{
	if ( !IsOnFoFHorse() )
	{
		m_flFoFHorseDismountHold = 0.0f;
		return;
	}

	CFoF_Horse *pHorse = m_hFoFHorse.Get();
	if ( !IsAlive() || !pHorse || pHorse->GetRider() != this )
	{
		DismountFoFHorse( true );
		return;
	}

	if ( m_nButtons & IN_DUCK )
	{
		m_flFoFHorseDismountHold += gpGlobals->frametime;
		if ( m_flFoFHorseDismountHold >= 0.75f )
		{
			DismountFoFHorse( false );
			return;
		}
	}
	else
	{
		m_flFoFHorseDismountHold = 0.0f;
	}

	pHorse->UpdateMountedState( EyeAngles()[YAW], this );
}

void CFoF_Player::UpdateFoFHorseRam()
{
	if ( !IsOnFoFHorse() )
		return;
	if ( FoFMapDisablesHorseRam() )
		return;

	const float flSpeed = GetAbsVelocity().Length();
	if ( flSpeed <= 0.0f || gpGlobals->curtime < m_flNextFoFHorseRamTime )
		return;

	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward );
	vecForward.z = 0.0f;
	VectorNormalize( vecForward );

	const Vector vecStart = GetAbsOrigin() + Vector( 0.0f, 0.0f, 40.0f );
	const Vector vecEnd = vecStart + vecForward * 30.0f;
	trace_t trace;
	UTIL_TraceHull(
		vecStart, vecEnd,
		Vector( -16.0f, -16.0f, -16.0f ),
		Vector( 16.0f, 16.0f, 16.0f ),
		MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &trace );

	CBaseEntity *pTarget = trace.m_pEnt;
	if ( !pTarget )
	{
		m_flNextFoFHorseRamTime = gpGlobals->curtime + 0.05f;
		return;
	}

	CFoF_Player *pTargetPlayer = ToFoFPlayer( pTarget );
	if ( !pTargetPlayer || !pTargetPlayer->IsOnFoFHorse() )
	{
		if ( !pTarget->IsPlayer() || flSpeed >= 300.0f )
		{
			CTakeDamageInfo info(
				this, this, 100.0f, DMG_AIRBOAT );
			CalculateMeleeDamageForce(
				&info, vecForward, trace.endpos, 1.0f );
			pTarget->DispatchTraceAttack( info, vecForward, &trace );
			ApplyMultiDamage();
			TraceAttackToTriggers(
				info, vecStart, vecEnd, vecForward );
		}
		else
		{
			pTarget->SetGroundEntity( NULL );
			pTarget->SetAbsVelocity( vec3_origin );
			pTarget->ApplyAbsVelocityImpulse(
				vecForward * 250.0f + Vector( 0.0f, 0.0f, 150.0f ) );
		}
	}

	m_flNextFoFHorseRamTime = gpGlobals->curtime + 0.1f;
}
