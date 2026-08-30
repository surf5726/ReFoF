//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF riding horse.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_horse.h"
#include "fof/fof_player.h"
#include "npcevent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pszFoFHorseModel =
	"models/horse/riding_horse.mdl";
static const char *s_pszFoFHorseGallopSounds[] =
{
	"Horse.Gallop",
	"Horse.Gallop2",
	"Horse.Gallop3",
	"Horse.Gallop4"
};
static const int FOF_HORSE_IDLE_EFFECT = 0x800;
static CUtlVector< CHandle< CFoF_Horse > > s_FoFHorses;

class CTriggerDangerTraceEnum : public IEntityEnumerator
{
public:
	explicit CTriggerDangerTraceEnum( CFoF_Horse *pHorse )
		: m_pHorse( pHorse )
	{
	}

	bool EnumEntity( IHandleEntity *pHandleEntity ) OVERRIDE
	{
		CBaseEntity *pEntity = gEntList.GetBaseEntity(
			pHandleEntity->GetRefEHandle() );
		if ( !pEntity || pEntity->IsSolid() )
			return true;

		if ( FClassnameIs( pEntity, "trigger_hurt_fof" ) ||
			FClassnameIs( pEntity, "trigger_hurt" ) )
		{
			m_pHorse->NotifyDangerTrigger();
		}

		return true;
	}

private:
	CFoF_Horse *m_pHorse;
};

static bool FoFHorseRaceMap()
{
	const char *pszMapName = STRING( gpGlobals->mapname );
	return pszMapName &&
		( !Q_strnicmp( pszMapName, "fofhr_", 6 ) ||
		  !Q_strnicmp( pszMapName, "tphr_", 5 ) );
}

LINK_ENTITY_TO_CLASS( fof_horse, CFoF_Horse );

IMPLEMENT_SERVERCLASS_ST( CFoF_Horse, DT_FoF_Horse )
END_SEND_TABLE()

BEGIN_DATADESC( CFoF_Horse )
	DEFINE_THINKFUNC( HorseIdle ),
END_DATADESC()

CFoF_Horse::CFoF_Horse()
{
	m_hRider = NULL;
	m_bOccupied = false;
	m_bRemovalPending = false;
	m_bHorseRaceMap = false;
	m_flMountAvailableTime = 0.0f;
	m_flPreviousSpeed = 0.0f;
	m_flCrashUntil = 0.0f;
	m_flNextIdleAnimation = 0.0f;
	m_flNextHoofSound = 0.0f;
	m_flPreviousRiderYaw = 0.0f;
	m_flDangerResetTime = 0.0f;
	m_flRemovalTime = 0.0f;
	m_flNextDangerScan = 0.0f;
	m_flIdleSince = 0.0f;
	m_nMoveXPoseParameter = -1;
	m_nMoveYPoseParameter = -1;
	m_vecSpawnOrigin.Init();
	m_angSpawnAngles.Init();
	m_pszGallopSound = s_pszFoFHorseGallopSounds[0];
}

void CFoF_Horse::Precache()
{
	PrecacheModel( s_pszFoFHorseModel );
	for ( int i = 0; i < ARRAYSIZE( s_pszFoFHorseGallopSounds ); ++i )
		PrecacheScriptSound( s_pszFoFHorseGallopSounds[i] );
	PrecacheScriptSound( "Horse.Clop" );
	PrecacheScriptSound( "NPC_Horse.Pain" );
	PrecacheScriptSound( "Player.ButtWalk" );
	BaseClass::Precache();
}

void CFoF_Horse::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel( s_pszFoFHorseModel );

	RemoveEffects( EF_NODRAW );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	SetCollisionBounds(
		Vector( -24.0f, -24.0f, -70.0f ),
		Vector( 24.0f, 24.0f, 0.0f ) );
	AddEffects( EF_ITEM_BLINK );
	AddEffects( FOF_HORSE_IDLE_EFFECT );

	m_vecSpawnOrigin = GetAbsOrigin();
	m_angSpawnAngles = GetAbsAngles();
	m_bRemovalPending = false;
	m_bHorseRaceMap = FoFHorseRaceMap();
	m_flDangerResetTime = 0.0f;
	m_flRemovalTime = m_bHorseRaceMap ? gpGlobals->curtime + 60.0f : 0.0f;
	m_flNextDangerScan = 0.0f;
	m_flIdleSince = 0.0f;
	m_pszGallopSound = s_pszFoFHorseGallopSounds[
		random->RandomInt( 0, ARRAYSIZE( s_pszFoFHorseGallopSounds ) - 1 )];
	m_nMoveXPoseParameter = LookupPoseParameter( "move_x" );
	m_nMoveYPoseParameter = LookupPoseParameter( "move_y" );
	m_flNextIdleAnimation = gpGlobals->curtime +
		random->RandomFloat( 1.5f, 4.0f );

	SetHorseSequence( "idle", 1.0f );
	if ( GetMoveParent() == NULL )
	{
		SetThink( &CFoF_Horse::HorseIdle );
		SetNextThink( gpGlobals->curtime +
			random->RandomFloat( 0.3f, 1.5f ) );
	}

	bool bRegistered = false;
	for ( int i = 0; i < s_FoFHorses.Count(); ++i )
	{
		if ( s_FoFHorses[i].Get() == this )
		{
			bRegistered = true;
			break;
		}
	}
	if ( !bRegistered )
		s_FoFHorses.AddToTail( this );
}

void CFoF_Horse::UpdateOnRemove()
{
	CFoF_Player *pRider = m_hRider.Get();
	m_hRider = NULL;
	m_bOccupied = false;
	if ( pRider && pRider->GetFoFHorse() == this )
		pRider->DismountFoFHorse( true );

	StopHorseSounds();
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
	for ( int i = s_FoFHorses.Count() - 1; i >= 0; --i )
	{
		if ( !s_FoFHorses[i].Get() || s_FoFHorses[i].Get() == this )
			s_FoFHorses.FastRemove( i );
	}

	BaseClass::UpdateOnRemove();
}

bool CFoF_Horse::IsAvailableForMount()
{
	return !m_bOccupied && m_hRider.Get() == NULL &&
		GetMoveParent() == NULL &&
		gpGlobals->curtime >= m_flMountAvailableTime;
}

CFoF_Player *CFoF_Horse::GetRider() const
{
	return m_hRider.Get();
}

void CFoF_Horse::EnterMountedState( CFoF_Player *pRider )
{
	m_hRider = pRider;
	m_bOccupied = pRider != NULL;
	m_flPreviousSpeed = pRider ? pRider->GetAbsVelocity().Length() : 0.0f;
	m_flPreviousRiderYaw = pRider ? pRider->EyeAngles()[YAW] : 0.0f;
	m_flCrashUntil = 0.0f;
	m_flNextHoofSound = 0.0f;
	m_flDangerResetTime = 0.0f;
	m_flIdleSince = 0.0f;

	RemoveEffects( EF_ITEM_BLINK );
	RemoveEffects( FOF_HORSE_IDLE_EFFECT );
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
	SetSolid( SOLID_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
}

void CFoF_Horse::LeaveMountedState()
{
	StopHorseSounds();
	m_hRider = NULL;
	m_bOccupied = false;
	m_flMountAvailableTime = gpGlobals->curtime + 0.3f;
	m_flPreviousSpeed = 0.0f;
	m_flCrashUntil = 0.0f;
	m_flNextIdleAnimation = gpGlobals->curtime + 1.5f;
	m_flDangerResetTime = 0.0f;
	m_flNextDangerScan = 0.0f;
	m_flIdleSince = gpGlobals->curtime;
	if ( m_bHorseRaceMap )
		UpdateHorseRaceRemovalTime();

	AddEffects( FOF_HORSE_IDLE_EFFECT );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	SetCollisionBounds(
		Vector( -24.0f, -24.0f, -70.0f ),
		Vector( 24.0f, 24.0f, 0.0f ) );
	SetHorseSequence( "idle", 1.0f );
	SetThink( &CFoF_Horse::HorseIdle );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CFoF_Horse::SetHorseSequence(
	const char *pszSequence, float flPlaybackRate )
{
	const int nSequence = LookupSequence( pszSequence );
	if ( nSequence >= 0 && GetSequence() != nSequence )
	{
		ResetSequence( nSequence );
		SetCycle( 0.0f );
	}
	SetPlaybackRate( flPlaybackRate );
}

void CFoF_Horse::StopHorseSounds()
{
	StopSound( "Horse.Clop" );
	for ( int i = 0; i < ARRAYSIZE( s_pszFoFHorseGallopSounds ); ++i )
		StopSound( s_pszFoFHorseGallopSounds[i] );
}

void CFoF_Horse::NotifyDangerTrigger()
{
	m_flNextDangerScan = gpGlobals->curtime + 2.0f;
	if ( m_bHorseRaceMap )
		m_flRemovalTime = gpGlobals->curtime + 0.1f;
	else
		m_flDangerResetTime = gpGlobals->curtime + 0.1f;
}

void CFoF_Horse::ResetToSpawnPosition()
{
	StopHorseSounds();
	SetAbsOrigin( m_vecSpawnOrigin );
	SetAbsAngles( m_angSpawnAngles );
	SetAbsVelocity( vec3_origin );
	SetLocalAngularVelocity( vec3_angle );
	SetHorseSequence( "idle", 1.0f );
	m_flPreviousSpeed = 0.0f;
	m_flCrashUntil = 0.0f;
	m_flNextHoofSound = 0.0f;
	m_flIdleSince = gpGlobals->curtime;
}

void CFoF_Horse::UpdateHorseRaceRemovalTime()
{
	int nNearbyHorses = 0;
	int nHorseCount = 0;
	for ( int i = s_FoFHorses.Count() - 1; i >= 0; --i )
	{
		CFoF_Horse *pHorse = s_FoFHorses[i].Get();
		if ( !pHorse )
		{
			s_FoFHorses.FastRemove( i );
			continue;
		}

		++nHorseCount;
		if ( pHorse != this && !pHorse->m_bOccupied &&
			( pHorse->GetAbsOrigin() - GetAbsOrigin() ).Length() < 2000.0f )
		{
			++nNearbyHorses;
		}
	}

	const float flNearbyScale = 1.0f -
		clamp( nNearbyHorses * 0.25f, 0.0f, 1.0f ) * 0.5f;
	const float flPopulationLifetime = 60.0f -
		clamp( nHorseCount * 0.04f, 0.0f, 1.0f ) * 30.0f;
	m_flRemovalTime = gpGlobals->curtime +
		flPopulationLifetime * flNearbyScale;
}

bool CFoF_Horse::UpdateHorseIdleLifecycle()
{
	if ( m_flRemovalTime > 0.0f &&
		gpGlobals->curtime > m_flRemovalTime )
	{
		if ( !m_bRemovalPending )
		{
			m_bRemovalPending = true;
			SetThink( NULL );
			SetNextThink( TICK_NEVER_THINK );
			UTIL_Remove( this );
		}
		return false;
	}

	if ( gpGlobals->curtime > m_flNextDangerScan )
	{
		m_flNextDangerScan = gpGlobals->curtime + 0.25f;
		const Vector vecStart = GetAbsOrigin();
		const Vector vecEnd = vecStart + Vector( 0.0f, 0.0f, -50.0f );
		Ray_t ray;
		ray.Init( vecStart, vecEnd );
		CTriggerDangerTraceEnum enumerator( this );
		enginetrace->EnumerateEntities( ray, true, &enumerator );
	}

	if ( m_flIdleSince > 0.0f &&
		gpGlobals->curtime - m_flIdleSince > 60.0f )
	{
		m_flIdleSince = gpGlobals->curtime;
		m_flDangerResetTime = gpGlobals->curtime + 2.0f;
	}

	if ( m_flDangerResetTime > 0.0f &&
		gpGlobals->curtime > m_flDangerResetTime )
	{
		m_flDangerResetTime = 0.0f;
		ResetToSpawnPosition();
	}

	return true;
}

void CFoF_Horse::UpdateMountedState(
	float flRiderYaw, CFoF_Player *pRider )
{
	if ( !pRider || pRider != m_hRider.Get() )
		return;

	const float flSpeed = pRider->GetAbsVelocity().Length();
	const bool bSliding = pRider->GetFoFSlideForce() > 0.2f;
	const float flYawDelta = AngleDiff( flRiderYaw, m_flPreviousRiderYaw );

	if ( m_flPreviousSpeed - flSpeed > 270.0f &&
		gpGlobals->curtime >= m_flCrashUntil )
	{
		m_flCrashUntil = gpGlobals->curtime + 0.75f;
		EmitSound( "Player.ButtWalk" );
		EmitSound( "NPC_Horse.Pain" );
	}

	if ( gpGlobals->curtime < m_flCrashUntil )
	{
		SetHorseSequence( "crash", 1.0f );
	}
	else if ( bSliding && flSpeed >= 50.0f )
	{
		SetHorseSequence( "slide", 1.0f );
	}
	else if ( flSpeed > 250.0f )
	{
		SetHorseSequence( "run", clamp( flSpeed / 350.0f, 0.75f, 1.5f ) );
	}
	else if ( flSpeed >= 10.0f )
	{
		SetHorseSequence( "walk", clamp( flSpeed / 150.0f, 0.35f, 1.5f ) );
	}
	else if ( flYawDelta > 1.0f )
	{
		SetHorseSequence( "turn_l", 1.0f );
	}
	else if ( flYawDelta < -1.0f )
	{
		SetHorseSequence( "turn_r", 1.0f );
	}
	else
	{
		SetHorseSequence( "idle", 1.0f );
	}

	const int nRiderMoveX = pRider->LookupPoseParameter( "move_x" );
	const int nRiderMoveY = pRider->LookupPoseParameter( "move_y" );
	if ( m_nMoveXPoseParameter >= 0 && nRiderMoveX >= 0 )
	{
		SetPoseParameter( m_nMoveXPoseParameter,
			pRider->GetPoseParameter( nRiderMoveX ) );
	}
	if ( m_nMoveYPoseParameter >= 0 && nRiderMoveY >= 0 )
	{
		SetPoseParameter( m_nMoveYPoseParameter,
			pRider->GetPoseParameter( nRiderMoveY ) );
	}

	StudioFrameAdvance();
	DispatchAnimEvents( this );

	if ( flSpeed > 50.0f && !bSliding )
	{
		if ( gpGlobals->curtime >= m_flNextHoofSound )
		{
			if ( flSpeed > 250.0f )
			{
				EmitSound( m_pszGallopSound );
				m_flNextHoofSound = gpGlobals->curtime + 1.08f;
			}
			else
			{
				EmitSound( "Horse.Clop" );
				m_flNextHoofSound = gpGlobals->curtime + 1.23f;
			}
		}
	}
	else
	{
		m_flNextHoofSound = gpGlobals->curtime;
	}

	m_flPreviousSpeed = flSpeed;
	m_flPreviousRiderYaw = flRiderYaw;
}

void CFoF_Horse::HorseIdle()
{
	if ( m_bOccupied || m_hRider.Get() )
		return;
	if ( !UpdateHorseIdleLifecycle() )
		return;

	if ( gpGlobals->curtime >= m_flNextIdleAnimation )
	{
		if ( random->RandomInt( 0, 4 ) == 0 )
			SetHorseSequence( "rear", 1.0f );
		else
			SetHorseSequence( "idle", 1.0f );

		m_flNextIdleAnimation = gpGlobals->curtime +
			random->RandomFloat( 2.0f, 5.0f );
	}
	else if ( IsSequenceFinished() )
	{
		SetHorseSequence( "idle", 1.0f );
	}

	StudioFrameAdvance();
	DispatchAnimEvents( this );
	SetNextThink( gpGlobals->curtime );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF ambient horse NPC.
//
//=============================================================================//

enum
{
	SCHED_HORSE_ANGRY = LAST_SHARED_SCHEDULE,
	SCHED_HORSE_KICK,
	SCHED_HORSE_IDLE,
};

static int ACT_HORSE_ANGRY;
static int ACT_HORSE_KICK;
static int AE_HORSE_KICK;

LINK_ENTITY_TO_CLASS( npc_horse, CNPC_Horse );

static const char *FoFHorseModelName()
{
	return "models/horse/horse1.mdl";
}

BEGIN_DATADESC( CNPC_Horse )
	DEFINE_KEYFIELD( m_bSaddle, FIELD_BOOLEAN, "saddle" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Kick", InputKick ),
END_DATADESC()

CNPC_Horse::CNPC_Horse()
	: m_hKickAttacker( NULL )
	, m_bSaddle( false )
	, m_flNextAngryTime( 0.0f )
{
}

void CNPC_Horse::Precache()
{
	BaseClass::Precache();
	PrecacheModel( FoFHorseModelName() );
	PrecacheScriptSound( "NPC_Horse.Idle" );
	PrecacheScriptSound( "NPC_Horse.Kicked" );
}

void CNPC_Horse::Spawn()
{
	BaseClass::Spawn();
	if ( GetModelName() == NULL_STRING )
		SetModelName( AllocPooledString( FoFHorseModelName() ) );

	Precache();
	SetModel( STRING( GetModelName() ) );
	SetHealth( 100 );
	m_bloodColor = DONT_BLEED;
	SetFadeDistance( 3000.0f, 3700.0f );
	SetHullType( HULL_HUMAN );
	SetHullSizeNormal();
	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_NONE );
	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND );
	m_NPCState = NPC_STATE_IDLE;
	NPCInit();
	m_hKickAttacker = NULL;
	m_flNextAngryTime = 0.0f;
}

bool CNPC_Horse::CreateVPhysics()
{
	return true;
}

Class_T CNPC_Horse::Classify()
{
	return CLASS_EARTH_FAUNA;
}

int CNPC_Horse::SelectSchedule()
{
	return SCHED_HORSE_IDLE;
}

CBasePlayer *CNPC_Horse::FindKickTarget() const
{
	Vector forward;
	GetVectors( &forward, NULL, NULL );
	const Vector vecCenter = GetAbsOrigin() + forward * 90.0f;
	const Vector vecMins = vecCenter + Vector( -25.0f, -25.0f, 10.0f );
	const Vector vecMaxs = vecCenter + Vector( 25.0f, 25.0f, 70.0f );

	CBaseEntity *pEntities[5];
	const int nEntities = UTIL_EntitiesInBox( pEntities, ARRAYSIZE( pEntities ),
		vecMins, vecMaxs, 0 );
	for ( int i = 0; i < nEntities; ++i )
	{
		if ( pEntities[i] && pEntities[i]->IsPlayer() )
			return ToBasePlayer( pEntities[i] );
	}
	return NULL;
}

void CNPC_Horse::ApplyKick( CBasePlayer *pTarget )
{
	if ( !pTarget )
		return;
	static ConVarRef kickDamage( "fof_npc_horsekickdamage", true );
	static ConVarRef kickForce( "fof_npc_horsekickforce", true );
	const float flDamage = kickDamage.IsValid() ? kickDamage.GetFloat() : 100.0f;
	const float flForce = kickForce.IsValid() ? kickForce.GetFloat() : 500.0f;

	Vector forward;
	GetVectors( &forward, NULL, NULL );
	pTarget->SetGroundEntity( NULL );
	pTarget->ApplyAbsVelocityImpulse( forward * flForce );

	CBaseEntity *pDamageOwner = m_hKickAttacker.Get();
	if ( !pDamageOwner )
		pDamageOwner = this;
	CTakeDamageInfo info( pDamageOwner, pDamageOwner, flDamage, DMG_FALL );
	pTarget->TakeDamage( info );
	m_hKickAttacker = NULL;
	EmitSound( "NPC_Horse.Kicked" );
}

void CNPC_Horse::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event != AE_HORSE_KICK )
	{
		BaseClass::HandleAnimEvent( pEvent );
		return;
	}

	ApplyKick( FindKickTarget() );
}

int CNPC_Horse::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( pAttacker && pAttacker->IsPlayer() )
		m_hKickAttacker = pAttacker;

	if ( FindKickTarget() )
	{
		if ( !IsCurSchedule( SCHED_HORSE_KICK, true ) )
			SetSchedule( SCHED_HORSE_KICK );
		return 0;
	}

	if ( gpGlobals->curtime > m_flNextAngryTime )
	{
		SetSchedule( SCHED_HORSE_ANGRY );
		m_flNextAngryTime = gpGlobals->curtime +
			random->RandomFloat( 10.0f, 15.0f );
	}
	return 0;
}

int CNPC_Horse::GetSoundInterests()
{
	return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER;
}

void CNPC_Horse::IdleSound()
{
	EmitSound( "NPC_Horse.Idle" );
}

void CNPC_Horse::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Horse.Pain" );
}

float CNPC_Horse::MaxYawSpeed()
{
	return 0.0f;
}

void CNPC_Horse::InputKick( inputdata_t &inputdata )
{
}

AI_BEGIN_CUSTOM_NPC( npc_horse, CNPC_Horse )

	DECLARE_ACTIVITY( ACT_HORSE_ANGRY )
	DECLARE_ACTIVITY( ACT_HORSE_KICK )
	DECLARE_ANIMEVENT( AE_HORSE_KICK )

	DEFINE_SCHEDULE
	(
		SCHED_HORSE_ANGRY,

		" Tasks"
		"  TASK_SET_ACTIVITY   ACTIVITY:ACT_HORSE_ANGRY"
		"  TASK_SET_FAIL_SCHEDULE  SCHEDULE:SCHED_HORSE_IDLE"
		"  TASK_WAIT     2"
		" Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_HORSE_KICK,

		" Tasks"
		"  TASK_SET_ACTIVITY   ACTIVITY:ACT_HORSE_KICK"
		"  TASK_SET_FAIL_SCHEDULE  SCHEDULE:SCHED_HORSE_IDLE"
		"  TASK_WAIT     2"
		" Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_HORSE_IDLE,

		" Tasks"
		"  TASK_STOP_MOVING    1"
		"  TASK_SET_ACTIVITY   ACTIVITY:ACT_IDLE"
		"  TASK_WAIT     1"
		"  TASK_WAIT_PVS    0"
		" Interrupts"
		"  COND_LIGHT_DAMAGE"
		"  COND_HEAR_COMBAT"
		"  COND_HEAR_BULLET_IMPACT"
		"  COND_IDLE_INTERRUPT"
	)

AI_END_CUSTOM_NPC()
