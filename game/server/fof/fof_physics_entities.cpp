//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF path-corner train used by older objective maps.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_physics_entities.h"
#include "trains.h"
#include "soundenvelope.h"
#include "fof/fof_player_shared.h"
#include "props.h"
#include "props_shared.h"
#include "utlmap.h"
#include "fof/fof_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CBasePlatTrainFoF )
	DEFINE_KEYFIELD( m_NoiseMoving, FIELD_SOUNDNAME, "noise1" ),
	DEFINE_KEYFIELD( m_NoiseArrived, FIELD_SOUNDNAME, "noise2" ),
	DEFINE_SOUNDPATCH( m_pMovementSound ),
	DEFINE_KEYFIELD( m_volume, FIELD_FLOAT, "volume" ),
	DEFINE_FIELD( m_flTWidth, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTLength, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flLip, FIELD_FLOAT, "lip" ),
	DEFINE_KEYFIELD( m_flWait, FIELD_FLOAT, "wait" ),
	DEFINE_KEYFIELD( m_flHeight, FIELD_FLOAT, "height" ),
END_DATADESC()

CBasePlatTrainFoF::CBasePlatTrainFoF()
	: m_NoiseMoving( NULL_STRING )
	, m_NoiseArrived( NULL_STRING )
	, m_pMovementSound( NULL )
	, m_volume( 0.0f )
	, m_flTWidth( 0.0f )
	, m_flTLength( 0.0f )
{
}

CBasePlatTrainFoF::~CBasePlatTrainFoF()
{
	StopMovingSound();
}

bool CBasePlatTrainFoF::KeyValue(
	const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "rotation" ) )
	{
		m_vecFinalAngle.x = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

void CBasePlatTrainFoF::Precache()
{
	UTIL_ValidateSoundName( m_NoiseMoving, "Plat.DefaultMoving" );
	UTIL_ValidateSoundName( m_NoiseArrived, "Plat.DefaultArrive" );
	BaseClass::Precache();
}

void CBasePlatTrainFoF::PlayMovingSound()
{
	StopMovingSound();
	if ( m_NoiseMoving == NULL_STRING )
		return;

	CSoundEnvelopeController &controller =
		CSoundEnvelopeController::GetController();
	CPASAttenuationFilter filter( this );
	m_pMovementSound = controller.SoundCreate(
		filter, entindex(), CHAN_STATIC,
		STRING( m_NoiseMoving ), ATTN_NORM );
	controller.Play( m_pMovementSound, m_volume, PITCH_NORM );
}

void CBasePlatTrainFoF::StopMovingSound()
{
	if ( !m_pMovementSound )
		return;

	CSoundEnvelopeController::GetController().SoundDestroy(
		m_pMovementSound );
	m_pMovementSound = NULL;
}

static void FoFEmitTrainArrivalSound( CBaseEntity *pTrain,
	string_t iszSound, float flVolume )
{
	if ( !pTrain || iszSound == NULL_STRING )
		return;

	CPASAttenuationFilter filter( pTrain );
	EmitSound_t sound;
	sound.m_nChannel = CHAN_VOICE;
	sound.m_pSoundName = STRING( iszSound );
	sound.m_flVolume = flVolume;
	sound.m_SoundLevel = SNDLVL_NORM;
	pTrain->EmitSound( filter, pTrain->entindex(), sound );
}

LINK_ENTITY_TO_CLASS( func_train_fof, CFuncTrainFoF );

BEGIN_DATADESC( CFuncTrainFoF )
	DEFINE_FIELD( m_hCurrentTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bActivated, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hEnemy, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flNextBlockTime, FIELD_TIME ),
	DEFINE_FIELD( m_iszLastTarget, FIELD_STRING ),
	DEFINE_KEYFIELD( m_flBlockDamage, FIELD_FLOAT, "dmg" ),
	DEFINE_FUNCTION( Wait ),
	DEFINE_FUNCTION( Next ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Start", InputStart ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Stop", InputStop ),
END_DATADESC()

CFuncTrainFoF::CFuncTrainFoF()
	: m_bActivated( false )
	, m_flBlockDamage( 2.0f )
	, m_flNextBlockTime( 0.0f )
	, m_iszLastTarget( NULL_STRING )
{
}

void CFuncTrainFoF::Spawn()
{
	Precache();
	if ( m_flSpeed == 0.0f )
		m_flSpeed = 100.0f;
	if ( m_flBlockDamage == 0.0f )
		m_flBlockDamage = 2.0f;
	if ( m_target == NULL_STRING )
		Warning( "func_train_fof '%s' has no target.\n", GetDebugName() );

	SetMoveType( MOVETYPE_PUSH );
	SetSolid( SOLID_BSP );
	SetModel( STRING( GetModelName() ) );
	if ( HasSpawnFlags( SF_TRACKTRAIN_PASSABLE ) )
		AddSolidFlags( FSOLID_NOT_SOLID );
	m_bActivated = false;
	if ( m_volume == 0.0f )
		m_volume = 0.85f;
}

void CFuncTrainFoF::Precache()
{
	BaseClass::Precache();
}

void CFuncTrainFoF::Activate()
{
	BaseClass::Activate();
	if ( m_bActivated )
		return;

	SetupTarget();
	m_bActivated = true;
	if ( !m_hCurrentTarget )
		return;

	UTIL_SetOrigin( this,
		m_hCurrentTarget->GetLocalOrigin() - CollisionProp()->OBBCenter() );
	if ( GetSolid() == SOLID_BSP )
		VPhysicsInitShadow( false, false );

	if ( GetEntityName() == NULL_STRING )
	{
		SetMoveDoneTime( 0.1f );
		SetMoveDone( &CFuncTrainFoF::Next );
	}
	else
	{
		AddSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
	}
}

void CFuncTrainFoF::OnRestore()
{
	BaseClass::OnRestore();
	if ( IsMoving() )
		m_target = m_iszLastTarget;
	SetupTarget();
}

void CFuncTrainFoF::SetupTarget()
{
	if ( m_hCurrentTarget || m_target == NULL_STRING )
		return;

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target );
	if ( !pTarget )
	{
		Warning( "Can't find target of func_train_fof %s\n", GetDebugName() );
		return;
	}
	m_target = pTarget->m_target;
	m_hCurrentTarget = pTarget;
}

void CFuncTrainFoF::Blocked( CBaseEntity *pOther )
{
	if ( !pOther || gpGlobals->curtime < m_flNextBlockTime )
		return;
	m_flNextBlockTime = gpGlobals->curtime + 0.5f;
	pOther->TakeDamage( CTakeDamageInfo(
		this, this, m_flBlockDamage, DMG_CRUSH ) );
}

void CFuncTrainFoF::Use( CBaseEntity *pActivator, CBaseEntity *pCaller,
	USE_TYPE useType, float value )
{
	if ( HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) )
	{
		RemoveSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
		Next();
	}
	else
	{
		AddSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
		if ( m_hEnemy )
			m_target = m_hEnemy->GetEntityName();
		SetNextThink( TICK_NEVER_THINK );
		SetLocalVelocity( vec3_origin );
		FoFEmitTrainArrivalSound( this, m_NoiseArrived, m_volume );
	}
}

void CFuncTrainFoF::Wait()
{
	if ( !m_hCurrentTarget )
		return;

	variant_t emptyValue;
	m_hCurrentTarget->AcceptInput(
		"InPass", this, this, emptyValue, 0 );
	if ( m_hCurrentTarget->HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) ||
		HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) )
	{
		AddSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
		StopMovingSound();
		FoFEmitTrainArrivalSound( this, m_NoiseArrived, m_volume );
		SetMoveDoneTime( -1.0f );
		return;
	}

	if ( m_flWait != 0.0f )
	{
		SetMoveDoneTime( m_flWait );
		StopMovingSound();
		FoFEmitTrainArrivalSound( this, m_NoiseArrived, m_volume );
		SetMoveDone( &CFuncTrainFoF::Next );
	}
	else
	{
		Next();
	}
}

void CFuncTrainFoF::Next()
{
	CBaseEntity *pTarget = GetNextTarget();
	if ( !pTarget )
	{
		StopMovingSound();
		FoFEmitTrainArrivalSound( this, m_NoiseArrived, m_volume );
		return;
	}

	m_iszLastTarget = m_target;
	m_target = pTarget->m_target;
	m_flWait = pTarget->GetDelay();
	if ( m_hCurrentTarget && m_hCurrentTarget->m_flSpeed != 0.0f )
		m_flSpeed = m_hCurrentTarget->m_flSpeed;

	m_hCurrentTarget = pTarget;
	m_hEnemy = pTarget;
	if ( pTarget->HasSpawnFlags( SF_CORNER_TELEPORT ) )
	{
		IncrementInterpolationFrame();
		UTIL_SetOrigin( this,
			pTarget->GetLocalOrigin() - CollisionProp()->OBBCenter() );
		Wait();
		return;
	}

	PlayMovingSound();
	SetMoveDone( &CFuncTrainFoF::Wait );
	LinearMove( pTarget->GetLocalOrigin() - CollisionProp()->OBBCenter(),
		m_flSpeed );
}

void CFuncTrainFoF::Start()
{
	if ( !HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) )
		return;
	RemoveSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
	Next();
}

void CFuncTrainFoF::Stop()
{
	if ( HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) )
		return;
	AddSpawnFlags( SF_TRAIN_WAIT_RETRIGGER );
	if ( m_hEnemy )
		m_target = m_hEnemy->GetEntityName();
	SetNextThink( TICK_NEVER_THINK );
	SetAbsVelocity( vec3_origin );
	FoFEmitTrainArrivalSound( this, m_NoiseArrived, m_volume );
	SetMoveDone( NULL );
	SetMoveDoneTime( -1.0f );
}

void CFuncTrainFoF::InputToggle( inputdata_t &inputData )
{
	if ( HasSpawnFlags( SF_TRAIN_WAIT_RETRIGGER ) )
		Start();
	else
		Stop();
}

void CFuncTrainFoF::InputStart( inputdata_t &inputData )
{
	Start();
}

void CFuncTrainFoF::InputStop( inputdata_t &inputData )
{
	Stop();
}

ConVar sv_turbophysics(
	"sv_turbophysics", "1", FCVAR_REPLICATED,
	"Turns on turbo physics" );

static bool FoFPhysicsPropModelContains(
	const CPhysicsProp *pProp, const char *pszNeedle )
{
	if ( !pProp || pProp->GetModelName() == NULL_STRING )
		return false;

	const char *pszModelName = STRING( pProp->GetModelName() );
	return pszModelName && Q_stristr( pszModelName, pszNeedle ) != NULL;
}

static bool FoFPhysicsPropForcesSolidMode( const CPhysicsProp *pProp )
{
	return FoFPhysicsPropModelContains( pProp, "bottle" ) ||
		FoFPhysicsPropModelContains( pProp, "rail_" ) ||
		FoFPhysicsPropModelContains( pProp, "railing" ) ||
		FoFPhysicsPropModelContains( pProp, "fence_" );
}

static bool FoFPhysicsPropUsesPushawayCollision(
	const CPhysicsProp *pProp )
{
	return FoFPhysicsPropModelContains( pProp, "crate" ) ||
		FoFPhysicsPropModelContains( pProp, "barrel" ) ||
		FoFPhysicsPropModelContains( pProp, "saloon_door" ) ||
		FoFPhysicsPropModelContains( pProp, "watermelon" ) ||
		FoFPhysicsPropModelContains( pProp, "chair" ) ||
		FoFPhysicsPropModelContains( pProp, "bench" ) ||
		FoFPhysicsPropModelContains( pProp, "silhouette" );
}

void FoFPreparePhysicsPropMultiplayerSpawn( CPhysicsProp *pProp )
{
	IBreakableWithPropData *pPropData =
		dynamic_cast< IBreakableWithPropData * >( pProp );
	if ( pPropData &&
		pPropData->GetPhysicsMode() == PHYSICS_MULTIPLAYER_AUTODETECT &&
		FoFPhysicsPropModelContains( pProp, "silhouette" ) )
	{
		pPropData->SetPhysicsMode( PHYSICS_MULTIPLAYER_NON_SOLID );
		return;
	}

	if ( !pPropData || !FoFPhysicsPropForcesSolidMode( pProp ) ||
		pPropData->GetPhysicsMode() == PHYSICS_MULTIPLAYER_SOLID )
	{
		return;
	}

	pPropData->SetPhysicsMode( PHYSICS_MULTIPLAYER_SOLID );
}

void FoFSetPhysicsPropMultiplayerCallbackFlags(
	CPhysicsProp *pProp, unsigned short nCallbackFlags )
{
	IPhysicsObject *pPhysics = pProp ? pProp->VPhysicsGetObject() : NULL;
	if ( pPhysics )
		pPhysics->SetCallbackFlags( nCallbackFlags );
}

void FoFConfigurePhysicsPropMultiplayerCollision( CPhysicsProp *pProp )
{
	if ( !sv_turbophysics.GetBool() ||
		!FoFPhysicsPropUsesPushawayCollision( pProp ) )
	{
		return;
	}

	pProp->SetCollisionGroup( COLLISION_GROUP_PUSHAWAY );
	FoFSetPhysicsPropMultiplayerCallbackFlags(
		pProp, CALLBACK_GLOBAL_FRICTION );
}

struct FoFPhysicsPropBounceState_t
{
	EHANDLE hProp;
	int nWallBounces;
	int nFloorBounces;
	int nPlayerHits;
};

static CUtlMap< int, FoFPhysicsPropBounceState_t > s_FoFPhysicsPropBounceStates(
	0, 0, DefLessFunc( int ) );

class CFoFPhysicsPropStateSystem : public CAutoGameSystem
{
public:
	CFoFPhysicsPropStateSystem() :
		CAutoGameSystem( "CFoFPhysicsPropStateSystem" )
	{
	}

	virtual void LevelShutdownPostEntity()
	{
		s_FoFPhysicsPropBounceStates.Purge();
	}
};

static CFoFPhysicsPropStateSystem s_FoFPhysicsPropStateSystem;

static FoFPhysicsPropBounceState_t &FoFPhysicsPropBounceState(
	CPhysicsProp *pProp )
{
	const int nHandle = pProp->GetRefEHandle().ToInt();
	int nIndex = s_FoFPhysicsPropBounceStates.Find( nHandle );
	if ( nIndex == s_FoFPhysicsPropBounceStates.InvalidIndex() )
	{
		FoFPhysicsPropBounceState_t state;
		state.hProp = pProp;
		state.nWallBounces = 0;
		state.nFloorBounces = 0;
		state.nPlayerHits = 0;
		nIndex = s_FoFPhysicsPropBounceStates.Insert( nHandle, state );
	}

	FoFPhysicsPropBounceState_t &state =
		s_FoFPhysicsPropBounceStates[nIndex];
	if ( state.hProp.Get() != pProp )
	{
		state.hProp = pProp;
		state.nWallBounces = 0;
		state.nFloorBounces = 0;
		state.nPlayerHits = 0;
	}
	return state;
}

static void FoFRecordPhysicsPropBounceState(
	CPhysicsProp *pProp, int nWallBounces, int nFloorBounces )
{
	FoFPhysicsPropBounceState_t &state =
		FoFPhysicsPropBounceState( pProp );
	state.nWallBounces = nWallBounces;
	state.nFloorBounces = nFloorBounces;
}

class CFoFPhysicsPropPlayerEnumerator : public IEntityEnumerator
{
public:
	CFoFPhysicsPropPlayerEnumerator(
		CPhysicsProp *pProp,
		CFoF_Player *pAttacker,
		int nDamage,
		const Vector &vecDamageForce )
		: m_pProp( pProp )
		, m_pAttacker( pAttacker )
		, m_nDamage( nDamage )
		, m_vecDamageForce( vecDamageForce )
	{
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEntity = pHandleEntity ?
			gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() ) : NULL;
		if ( !pEntity || !pEntity->IsPlayer() )
			return true;

		CFoF_Player *pTarget = ToFoFPlayer( pEntity );
		if ( !pTarget || !m_pProp )
			return true;

		IPhysicsObject *pPhysics = m_pProp->VPhysicsGetObject();
		if ( !pPhysics )
			return false;

		Vector vecVelocity;
		pPhysics->GetVelocity( &vecVelocity, NULL );
		Vector vecDirection = vecVelocity;
		VectorNormalize( vecDirection );

		if ( FoFPlayersAreEnemies( m_pAttacker, pTarget ) &&
			m_nDamage > 0 )
		{
			pTarget->EmitSound( "Default.ImpactHard" );

			trace_t trace;
			const Vector vecCenter = m_pProp->WorldSpaceCenter();
			UTIL_TraceLine(
				vecCenter - vecDirection * 10.0f,
				vecCenter + vecDirection * 10.0f,
				MASK_SHOT,
				m_pProp,
				COLLISION_GROUP_NONE,
				&trace );

			CTakeDamageInfo damageInfo(
				m_pProp, m_pAttacker,
				static_cast< float >( m_nDamage ), DMG_CRUSH );
			damageInfo.SetDamageForce( m_vecDamageForce );
			damageInfo.SetDamagePosition( trace.endpos );
			pTarget->DispatchTraceAttack(
				damageInfo, vecDirection, &trace );
			ApplyMultiDamage();

			FoFPhysicsPropBounceState_t &state =
				FoFPhysicsPropBounceState( m_pProp );
			++state.nPlayerHits;
			if ( state.nPlayerHits == 2 )
				m_pProp->AddEffects( EF_ITEM_BLINK );
			if ( state.nPlayerHits >= 3 )
				m_pProp->Event_Killed( damageInfo );
		}

		if ( pTarget != m_pAttacker )
		{
			vecVelocity *= -0.2f;
			pPhysics->SetVelocity( &vecVelocity, NULL );
		}

		// The shipped enumerator stops after the first player intersected by
		// this sweep, including the physics attacker.
		return false;
	}

private:
	CPhysicsProp *m_pProp;
	CFoF_Player *m_pAttacker;
	int m_nDamage;
	Vector m_vecDamageForce;
};

bool FoFGetPhysicsPropBounceState(
	CBaseEntity *pEntity, int &nWallBounces, int &nFloorBounces )
{
	nWallBounces = 0;
	nFloorBounces = 0;

	CPhysicsProp *pProp = dynamic_cast< CPhysicsProp * >( pEntity );
	IMultiplayerPhysics *pMultiplayerPhysics =
		dynamic_cast< IMultiplayerPhysics * >( pEntity );
	if ( !pProp || !pMultiplayerPhysics )
		return false;

	const int nHandle = pProp->GetRefEHandle().ToInt();
	const int nIndex = s_FoFPhysicsPropBounceStates.Find( nHandle );
	if ( nIndex == s_FoFPhysicsPropBounceStates.InvalidIndex() )
		return true;

	const FoFPhysicsPropBounceState_t &state =
		s_FoFPhysicsPropBounceStates[nIndex];
	if ( state.hProp.Get() == pProp )
	{
		nWallBounces = state.nWallBounces;
		nFloorBounces = state.nFloorBounces;
	}
	return true;
}

static void FoFSteerPhysicsPropTowardEnemy(
	CPhysicsProp *pProp,
	IPhysicsObject *pPhysics,
	CFoF_Player *pAttacker,
	int nWallBounces,
	Vector &vecVelocity )
{
	if ( !pProp || !pPhysics || !pAttacker || nWallBounces <= 0 )
		return;

	const float flSpeed = VectorNormalize( vecVelocity );
	if ( flSpeed <= 0.0f )
		return;

	const Vector vecOrigin = pProp->GetAbsOrigin();
	for ( CEntitySphereQuery sphere( vecOrigin, 300.0f, 0 );
		CBaseEntity *pEntity = sphere.GetCurrentEntity();
		sphere.NextEntity() )
	{
		if ( !pEntity->IsPlayer() )
			continue;

		CFoF_Player *pTarget = ToFoFPlayer( pEntity );
		if ( !pTarget || !pTarget->IsAlive() ||
			pTarget == pAttacker ||
			!FoFPlayersAreEnemies( pAttacker, pTarget ) )
		{
			continue;
		}

		Vector vecTargetDirection =
			pTarget->WorldSpaceCenter() - vecOrigin;
		if ( VectorNormalize( vecTargetDirection ) <= 0.0f ||
			DotProduct( vecVelocity, vecTargetDirection ) <= 0.5f )
		{
			continue;
		}

		const float flTargetWeight =
			static_cast< float >( nWallBounces ) * 0.015f;
		Vector vecSteeredDirection =
			vecVelocity * ( 1.0f - flTargetWeight ) +
			vecTargetDirection * flTargetWeight;
		if ( VectorNormalize( vecSteeredDirection ) <= 0.0f )
		{
			vecVelocity *= flSpeed;
			return;
		}

		vecVelocity = vecSteeredDirection * flSpeed;
		pPhysics->SetVelocity( &vecVelocity, NULL );
		return;
	}

	vecVelocity *= flSpeed;
}

void FoFUpdatePhysicsPropMultiplayer(
	CPhysicsProp *pProp, IPhysicsObject *pPhysics )
{
	if ( !pProp || !pPhysics )
		return;

	FoFPhysicsPropBounceState_t &state =
		FoFPhysicsPropBounceState( pProp );
	if ( pPhysics->IsAsleep() )
		return;

	CFoF_Player *pAttacker = ToFoFPlayer(
		pProp->HasPhysicsAttacker( 2.0f ) );
	if ( !pAttacker )
		return;

	Vector vecVelocity;
	AngularImpulse angularVelocity;
	pPhysics->GetVelocity( &vecVelocity, &angularVelocity );
	if ( vecVelocity.Length() <= 300.0f )
	{
		state.nWallBounces = 0;
		state.nFloorBounces = 0;
		return;
	}

	FoFSteerPhysicsPropTowardEnemy(
		pProp, pPhysics, pAttacker,
		state.nWallBounces, vecVelocity );

	Vector vecDirection = vecVelocity;
	if ( VectorNormalize( vecDirection ) <= 0.0f )
		return;

	if ( pAttacker->m_nPlayerInfo & 0x800000 )
	{
		const Vector vecStart = pProp->GetAbsOrigin();
		const Vector vecEnd = vecStart + vecDirection * 32.0f;
		trace_t trace;
		UTIL_TraceEntity( pProp, vecStart, vecEnd,
			MASK_SOLID_BRUSHONLY, pProp,
			COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0f &&
			!trace.startsolid && !trace.allsolid )
		{
			const float flNormalVelocity =
				DotProduct( vecVelocity, trace.plane.normal );
			if ( flNormalVelocity < 0.0f )
			{
				vecVelocity += trace.plane.normal *
					( flNormalVelocity * -1.45f );
				pPhysics->SetVelocity(
					&vecVelocity, &angularVelocity );

				if ( trace.plane.normal.z < 0.7f &&
					trace.plane.normal.z >= -0.7f )
				{
					++state.nWallBounces;
					state.nFloorBounces = 0;
				}
				if ( trace.plane.normal.z > 0.99f )
					++state.nFloorBounces;
			}
		}
	}

	FoFRecordPhysicsPropBounceState(
		pProp, state.nWallBounces, state.nFloorBounces );

	vecDirection = vecVelocity;
	if ( VectorNormalize( vecDirection ) <= 0.0f )
		return;

	IMultiplayerPhysics *pMultiplayerPhysics =
		dynamic_cast< IMultiplayerPhysics * >( pProp );
	if ( !pMultiplayerPhysics )
		return;

	float flDamage = RemapValClamped(
		pMultiplayerPhysics->GetMass(),
		1.0f, 60.0f, 1.0f, 50.0f );
	if ( state.nWallBounces > 0 )
	{
		flDamage *= RemapValClamped(
			static_cast< float >( state.nWallBounces ),
			1.0f, 2.0f, 1.75f, 2.5f );
	}
	if ( state.nFloorBounces > 0 )
		flDamage *= 1.33f;
	if ( state.nPlayerHits >= 2 )
		flDamage *= 1.5f;

	const Vector vecStart = pProp->GetAbsOrigin();
	Ray_t ray;
	ray.Init(
		vecStart,
		vecStart + vecDirection * 32.0f,
		pProp->CollisionProp()->OBBMins(),
		pProp->CollisionProp()->OBBMaxs() );

	CFoFPhysicsPropPlayerEnumerator enumerator(
		pProp, pAttacker,
		static_cast< int >( flDamage ), vecDirection );
	enginetrace->EnumerateEntities( ray, false, &enumerator );
}
