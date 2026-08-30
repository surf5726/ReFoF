//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF ghost entity.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_ghost.h"

#include "basecombatweapon.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_player.h"
#include "particle_parse.h"

#include <math.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_ghost, CBaseGhost );

IMPLEMENT_SERVERCLASS_ST( CBaseGhost, DT_BaseGhost )
END_SEND_TABLE()

BEGIN_DATADESC( CBaseGhost )
	DEFINE_THINKFUNC( GhostThink ),
	DEFINE_THINKFUNC( SpawnThink ),
	DEFINE_THINKFUNC( DieThink ),
	DEFINE_ENTITYFUNC( GhostTouch ),
END_DATADESC()

CBaseGhost::CBaseGhost()
{
	m_hGhostTarget = NULL;
	m_vecSteeringVelocity.Init();
	m_vecWanderDestination.Init();
	m_bHasWanderDestination = false;
	m_flNextTurnTime = 0.0f;
	m_flFadeTimer = 0.0f;
	m_flNextAttackTime = 0.0f;
	m_bAttackAnimation = false;
	m_flAttackStartTime = 0.0f;
	m_bWeaponEffectSpawned = false;
	m_vecWeaponSpawnPosition.Init();
	m_bDying = false;
	m_flGhostGunForceUntil = 0.0f;
}

void CBaseGhost::Precache()
{
	BaseClass::Precache();
	PrecacheModel( "models/npc/ghost.mdl" );
	PrecacheScriptSound( "Ghost.Spawn" );
	PrecacheScriptSound( "Ghost.Attack" );
	PrecacheScriptSound( "Ghost.Alert" );
	PrecacheScriptSound( "Ghost.Death" );
	PrecacheParticleSystem( "ghost_spawn" );
	PrecacheParticleSystem( "ghost_weapon_spawn" );
}

void CBaseGhost::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel( "models/npc/ghost.mdl" );
	SetMoveType( MOVETYPE_NOCLIP, MOVECOLLIDE_DEFAULT );
	UTIL_SetSize( this, Vector( -16.0f, -16.0f, -32.0f ),
		Vector( 16.0f, 16.0f, 72.0f ) );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST );

	m_hGhostTarget = NULL;
	m_vecSteeringVelocity.Init();
	m_vecWanderDestination.Init();
	m_bHasWanderDestination = false;
	m_flNextTurnTime = 0.0f;
	m_flNextAttackTime = 0.0f;
	m_bAttackAnimation = false;
	m_flAttackStartTime = 0.0f;
	m_bWeaponEffectSpawned = false;
	m_vecWeaponSpawnPosition.Init();
	m_bDying = false;
	m_flGhostGunForceUntil = 0.0f;

	SetRenderMode( kRenderTransColor );
	SetRenderColor( 0, 0, 0, 0 );
	m_takedamage = DAMAGE_EVENTS_ONLY;
	SetGhostSequence( "idle01" );
	SetTouch( &CBaseGhost::GhostTouch );

	EmitSound( "Ghost.Spawn" );
	QAngle particleAngles(
		RandomFloat( 0.0f, 360.0f ),
		RandomFloat( 0.0f, 360.0f ),
		RandomFloat( 0.0f, 360.0f ) );
	DispatchParticleEffect( "ghost_spawn",
		GetAbsOrigin() + Vector( 0.0f, 0.0f, 50.0f ),
		particleAngles );

	m_flFadeTimer = 1.5f;
	SetThink( &CBaseGhost::SpawnThink );
	SetNextThink( gpGlobals->curtime + 0.01f );
}

bool CBaseGhost::ShouldCollide(
	int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ||
		collisionGroup == COLLISION_GROUP_PROJECTILE )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

int CBaseGhost::OnTakeDamage( const CTakeDamageInfo &info )
{
	CBaseEntity *pAttackerEntity = info.GetAttacker();
	CFoF_Player *pAttacker = ToFoFPlayer( pAttackerEntity );
	if ( pAttacker && IsValidTarget( pAttacker ) &&
		( pAttacker->GetFoFPlayerInfo() & 0x800 ) == 0 )
	{
		AlertTarget( pAttacker );
	}

	CBaseEntity *pWeapon = info.GetInflictor();
	if ( !pWeapon || pWeapon->IsPlayer() )
		pWeapon = info.GetWeapon();

	const bool bExplosiveWeapon = pWeapon &&
		( pWeapon->ClassMatches( "dynamite" ) ||
		  pWeapon->ClassMatches( "dynamite_black" ) ||
		  pWeapon->ClassMatches( "weapon_dynamite" ) ||
		  pWeapon->ClassMatches( "weapon_dynamite_black" ) );
	if ( !m_bDying &&
		( bExplosiveWeapon || ( info.GetDamageType() & DMG_BLAST ) ) )
	{
		if ( pAttacker )
		{
			pAttacker->AddFoFNotoriety( 20 );
			IGameEvent *pEvent =
				gameeventmanager->CreateEvent( "ghost_buster" );
			if ( pEvent )
			{
				pEvent->SetInt( "entindex_ghost_buster",
					pAttacker->entindex() );
				gameeventmanager->FireEvent( pEvent );
			}
		}
		BeginDeath();
	}

	return BaseClass::OnTakeDamage( info );
}

void CBaseGhost::ImpactTrace(
	trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

CBaseEntity *CBaseGhost::GetEnemy()
{
	CBaseEntity *pTarget = m_hGhostTarget.Get();
	return pTarget && pTarget->IsPlayer() ? pTarget : NULL;
}

void CBaseGhost::UpdateOnRemove()
{
	SetNextThink( TICK_NEVER_THINK );
	SetThink( NULL );
	SetTouch( NULL );
	BaseClass::UpdateOnRemove();
}

bool CBaseGhost::CreateVPhysics()
{
	VPhysicsInitNormal( SOLID_VPHYSICS, 0, false, NULL );
	return true;
}

void CBaseGhost::ApplyFoFGhostGunShot(
	int nDamage, const Vector &vecDirection )
{
	// The original server stores this vector; GhostThink
	// consumes it, so applying velocity here would move the entity twice.
	m_vecSteeringVelocity = vecDirection * 150.0f;
	m_flGhostGunForceUntil = gpGlobals->curtime + 0.75f +
		clamp( ( (float)nDamage - 30.0f ) * 0.02f, 0.0f, 1.0f );
}

void CBaseGhost::SpawnThink()
{
	SetRenderMode( kRenderTransColor );
	const int nColor = clamp( (int)(
		( 1.0f - clamp( m_flFadeTimer * ( 2.0f / 3.0f ),
			0.0f, 1.0f ) ) * 255.0f ), 0, 255 );
	SetRenderColor( nColor, nColor, nColor, nColor );

	if ( m_flFadeTimer <= 0.0f )
	{
		m_flFadeTimer = 0.0f;
		SetGhostSequence( "idle01" );
		SetThink( &CBaseGhost::GhostThink );
		SetNextThink( gpGlobals->curtime + 0.01f );
		return;
	}

	m_flFadeTimer -= gpGlobals->frametime;
	SetNextThink( gpGlobals->curtime );
}

void CBaseGhost::GhostThink()
{
	Vector vecTarget;
	const bool bHasTarget = FindTargetPosition( vecTarget );
	const float flDistance = bHasTarget ?
		vecTarget.DistTo( GetAbsOrigin() ) : 0.0f;

	if ( bHasTarget )
	{
		if ( flDistance > 180.0f &&
			gpGlobals->curtime > m_flNextTurnTime )
		{
			m_flNextTurnTime = gpGlobals->curtime +
				RandomFloat( 0.5f, 1.0f );
		}
		else
		{
			FaceTarget( vecTarget );
		}
	}

	if ( m_bAttackAnimation &&
		gpGlobals->curtime - m_flAttackStartTime > 1.0f )
	{
		m_bAttackAnimation = false;
		SetGhostSequence( "chase01" );
	}
	StudioFrameAdvance();

	if ( m_flGhostGunForceUntil > gpGlobals->curtime )
	{
		if ( bHasTarget )
			SteerToward( vecTarget, 0.1f );
		const float flScale = RemapValClamped(
			m_flGhostGunForceUntil - gpGlobals->curtime,
			0.0f, 1.0f, 0.25f, 2.5f );
		SetAbsVelocity( -m_vecSteeringVelocity * flScale );
	}
	else if ( bHasTarget &&
		( m_bHasWanderDestination ||
		  ( flDistance > 50.0f && !m_bAttackAnimation ) ) )
	{
		SteerToward( vecTarget, 0.1f );
		if ( m_bHasWanderDestination )
		{
			SetAbsVelocity( m_vecSteeringVelocity * 0.3f );
		}
		else
		{
			const float flScale = RemapValClamped(
				gpGlobals->curtime - m_flAttackStartTime,
				0.0f, 10.0f, 0.85f, 1.25f );
			SetAbsVelocity( m_vecSteeringVelocity * flScale );
		}
	}
	else
	{
		SetAbsVelocity( vec3_origin );
	}

	if ( m_bHasWanderDestination && flDistance < 30.0f )
		m_bHasWanderDestination = false;

	TryAttack();
	CFoF_Player *pTarget = ToFoFPlayer( m_hGhostTarget.Get() );
	if ( pTarget &&
		gpGlobals->curtime - m_flAttackStartTime > 10.0f &&
		pTarget->GetAbsOrigin().DistTo( GetAbsOrigin() ) > 256.0f )
	{
		m_hGhostTarget = NULL;
	}

	SetNextThink( gpGlobals->curtime );
}

void CBaseGhost::DieThink()
{
	SetRenderMode( kRenderTransColor );
	const int nColor = clamp( (int)( clamp(
		m_flFadeTimer * ( 2.0f / 3.0f ), 0.0f, 1.0f ) * 255.0f ),
		0, 255 );
	SetRenderColor( nColor, nColor, nColor, nColor );

	if ( m_flFadeTimer <= 0.5f && !m_bWeaponEffectSpawned )
	{
		m_vecWeaponSpawnPosition =
			WorldSpaceCenter() + Vector( 0.0f, 0.0f, 40.0f );
		DispatchParticleEffect( "ghost_weapon_spawn",
			m_vecWeaponSpawnPosition, vec3_angle );
		m_bWeaponEffectSpawned = true;
	}

	if ( m_flFadeTimer <= 0.0f )
	{
		SpawnRandomWeapon();
		UTIL_Remove( this );
		return;
	}

	CFoF_Player *pTarget = ToFoFPlayer( m_hGhostTarget.Get() );
	if ( IsValidTarget( pTarget ) )
	{
		FaceTarget( pTarget->GetAbsOrigin() );
		SteerToward( pTarget->GetAbsOrigin(), 0.1f );
		SetAbsVelocity( m_vecSteeringVelocity );
	}

	m_flFadeTimer -= gpGlobals->frametime;
	SetNextThink( gpGlobals->curtime );
}

void CBaseGhost::GhostTouch( CBaseEntity *pOther )
{
	CFoF_Player *pPlayer = ToFoFPlayer( pOther );
	if ( IsValidTarget( pPlayer ) )
		AlertTarget( pPlayer );
}

bool CBaseGhost::IsValidTarget( CBaseEntity *pEntity ) const
{
	CFoF_Player *pPlayer = ToFoFPlayer( pEntity );
	return pPlayer && pPlayer->IsAlive() && !pPlayer->IsObserver();
}

void CBaseGhost::AlertTarget( CFoF_Player *pTarget )
{
	if ( !IsValidTarget( pTarget ) )
		return;
	if ( m_hGhostTarget.Get() == pTarget && m_bAttackAnimation )
		return;

	EmitSound( "Ghost.Alert" );
	m_bAttackAnimation = true;
	m_flAttackStartTime = gpGlobals->curtime;
	SetGhostSequence( "alert01" );
	m_bHasWanderDestination = false;
	m_hGhostTarget = pTarget;
}

bool CBaseGhost::FindTargetPosition( Vector &vecTarget )
{
	CFoF_Player *pTarget = ToFoFPlayer( m_hGhostTarget.Get() );
	if ( IsValidTarget( pTarget ) )
	{
		vecTarget = pTarget->GetAbsOrigin();
		return true;
	}
	m_hGhostTarget = NULL;

	if ( !m_bHasWanderDestination )
		ChooseWanderDestination();
	if ( !m_bHasWanderDestination )
		return false;

	vecTarget = m_vecWanderDestination;
	return true;
}

void CBaseGhost::ChooseWanderDestination()
{
	int nNearbyGhosts = 0;
	for ( CEntitySphereQuery query( GetAbsOrigin(), 256.0f );
		CBaseEntity *pEntity = query.GetCurrentEntity();
		query.NextEntity() )
	{
		if ( pEntity != this && pEntity->ClassMatches( "fof_ghost" ) )
			++nNearbyGhosts;
	}

	if ( nNearbyGhosts >= 2 )
	{
		CUtlVector< CBaseEntity * > spawnPoints;
		CBaseEntity *pSpawn = NULL;
		while ( ( pSpawn = gEntList.FindEntityByClassname(
			pSpawn, "info_player_fof" ) ) != NULL )
		{
			if ( pSpawn->GetAbsOrigin().DistTo( GetAbsOrigin() ) >= 2000.0f )
				spawnPoints.AddToTail( pSpawn );
		}

		for ( int nTry = 0; nTry < 3 && spawnPoints.Count() > 0; ++nTry )
		{
			CBaseEntity *pCandidate = spawnPoints[
				RandomInt( 0, spawnPoints.Count() - 1 ) ];
			if ( RandomInt( 0, 100 ) > 20 )
				continue;

			bool bGhostNearby = false;
			for ( CEntitySphereQuery query(
				pCandidate->GetAbsOrigin(), 256.0f );
				CBaseEntity *pEntity = query.GetCurrentEntity();
				query.NextEntity() )
			{
				if ( pEntity->ClassMatches( "fof_ghost" ) )
				{
					bGhostNearby = true;
					break;
				}
			}
			if ( !bGhostNearby )
			{
				m_vecWanderDestination = pCandidate->GetAbsOrigin();
				m_bHasWanderDestination = true;
				return;
			}
		}
	}

	Vector vecDirection;
	AngleVectors( QAngle(
		RandomFloat( -12.0f, 12.0f ),
		RandomFloat( 0.0f, 360.0f ), 0.0f ), &vecDirection );
	trace_t trace;
	UTIL_TraceLine( GetAbsOrigin(),
		GetAbsOrigin() + vecDirection * 500.0f,
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
	m_vecWanderDestination = trace.endpos;
	m_bHasWanderDestination =
		m_vecWanderDestination.DistTo( GetAbsOrigin() ) >= 30.0f;
}

void CBaseGhost::SteerToward(
	const Vector &vecTarget, float flWeight )
{
	Vector vecDesired = vecTarget - GetAbsOrigin();
	const float flDistance = VectorNormalize( vecDesired );
	if ( flDistance <= 0.001f )
		return;

	Vector vecCurrentDirection = m_vecSteeringVelocity;
	const float flCurrentSpeed = VectorNormalize( vecCurrentDirection );
	const float flDot = flCurrentSpeed > 0.0f ?
		DotProduct( vecCurrentDirection, vecDesired ) : 1.0f;
	const float flAcceleration = flDot > 0.25f ? 250.0f : 128.0f;
	const float flDecay = powf( 0.15f, flWeight );
	m_vecSteeringVelocity *= flDecay;
	m_vecSteeringVelocity += vecDesired * MIN(
		flAcceleration * flWeight, flDistance );

	const float flSpeedCap = MIN(
		200.0f, flDistance / MAX( flWeight, 0.001f ) );
	const float flSpeed = m_vecSteeringVelocity.Length();
	if ( flSpeed > flSpeedCap && flSpeed > 0.0f )
		m_vecSteeringVelocity *= flSpeedCap / flSpeed;
}

void CBaseGhost::FaceTarget( const Vector &vecTarget )
{
	Vector vecDirection = vecTarget - GetAbsOrigin();
	if ( vecDirection.LengthSqr() <= 0.001f )
		return;

	QAngle targetAngles;
	VectorAngles( vecDirection, targetAngles );
	QAngle currentAngles = GetAbsAngles();
	currentAngles.x = 0.0f;
	currentAngles.y = UTIL_ApproachAngle(
		targetAngles.y, currentAngles.y,
		150.0f * gpGlobals->frametime );
	currentAngles.z = 0.0f;
	SetAbsAngles( currentAngles );
}

void CBaseGhost::TryAttack()
{
	if ( m_bDying || gpGlobals->curtime < m_flNextAttackTime )
		return;

	const Vector vecStart =
		GetAbsOrigin() + Vector( 0.0f, 0.0f, 55.0f );
	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );
	trace_t trace;
	UTIL_TraceLine( vecStart, vecStart + vecForward * 45.0f,
		MASK_SHOT, this, COLLISION_GROUP_NONE, &trace );

	CFoF_Player *pPlayer = ToFoFPlayer( trace.m_pEnt );
	if ( !IsValidTarget( pPlayer ) )
	{
		m_flNextAttackTime = gpGlobals->curtime + 0.15f;
		return;
	}

	if ( !IsValidTarget( m_hGhostTarget.Get() ) )
	{
		m_flNextAttackTime = gpGlobals->curtime + 1.0f;
		AlertTarget( pPlayer );
		return;
	}

	m_flNextAttackTime = gpGlobals->curtime + 2.0f;
	m_bAttackAnimation = true;
	m_flAttackStartTime = gpGlobals->curtime;
	SetGhostSequence( "attack01" );
	EmitSound( "Ghost.Attack" );

	Vector vecImpulse(
		vecForward.x * 200.0f,
		vecForward.y * 200.0f,
		150.0f );
	CTakeDamageInfo damageInfo(
		this, this, vecImpulse, pPlayer->WorldSpaceCenter(),
		35.0f, DMG_SLASH );
	pPlayer->TakeDamage( damageInfo );
	pPlayer->ApplyAbsVelocityImpulse( vecImpulse );
}

void CBaseGhost::BeginDeath()
{
	if ( m_bDying )
		return;

	m_bDying = true;
	EmitSound( "Ghost.Death" );
	SetModelScale( 0.1f, 1.5f );
	m_flFadeTimer = 1.5f;
	m_bWeaponEffectSpawned = false;
	SetThink( &CBaseGhost::DieThink );
	SetNextThink( gpGlobals->curtime + 0.01f );
}

void CBaseGhost::SpawnRandomWeapon()
{
	CUtlVector< const FoFItemDefinition_t * > candidates;
	for ( int i = 0; i < FoFItemDefinitionCount(); ++i )
	{
		const FoFItemDefinition_t *pItem = FoFItemDefinitionAt( i );
		if ( !pItem || !pItem->m_pszClassname ||
			!pItem->m_pszClassname[0] ||
			!Q_stricmp( pItem->m_pszClassname,
				"weapon_sawedoff_shotgun" ) )
		{
			continue;
		}
		candidates.AddToTail( pItem );
	}
	if ( candidates.Count() <= 0 )
		return;

	const FoFItemDefinition_t *pSelected = NULL;
	for ( int nTry = 0; nTry < 16; ++nTry )
	{
		const FoFItemDefinition_t *pCandidate = candidates[
			RandomInt( 0, candidates.Count() - 1 ) ];
		if ( RandomInt( 0, 100 ) <= 40 )
		{
			pSelected = pCandidate;
			break;
		}
	}
	if ( !pSelected )
		pSelected = candidates[RandomInt( 0, candidates.Count() - 1 )];

	CBaseEntity *pEntity =
		CreateEntityByName( pSelected->m_pszClassname );
	if ( !pEntity )
		return;
	pEntity->SetAbsOrigin( m_vecWeaponSpawnPosition );
	DispatchSpawn( pEntity );
	CBaseCombatWeapon *pWeapon =
		dynamic_cast< CBaseCombatWeapon * >( pEntity );
	if ( pWeapon )
		pWeapon->FallInit();
}

void CBaseGhost::SetGhostSequence( const char *pszSequence )
{
	const int nSequence = LookupSequence( pszSequence );
	if ( nSequence < 0 )
		return;
	SetSequence( nSequence );
	SetCycle( 0.0f );
	SetPlaybackRate( 1.0f );
	ResetSequenceInfo();
}
