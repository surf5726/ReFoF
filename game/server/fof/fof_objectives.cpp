//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF capture, mobile-objective, and cannon-ball map entities.
//
//=============================================================================//
#include "cbase.h"
#include "baseanimating.h"
#include "basecombatcharacter.h"
#include "basetoggle.h"
#include "fof/fof_objectives.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_teamplay.h"
#include "fof/fof_player.h"
#include "GameEventListener.h"
#include "explode.h"
#include "in_buttons.h"
#include "particle_parse.h"
#include "recipientfilter.h"
#include "team.h"
#include "world.h"
#include "trains.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar showtriggers;

LINK_ENTITY_TO_CLASS( fof_mobile_point, CMobilePoint );
LINK_ENTITY_TO_CLASS( fof_capture_zone, CCapturePoint );
LINK_ENTITY_TO_CLASS( fof_cannon_ball, CCannonBall );
LINK_ENTITY_TO_CLASS( fof_cap_entity, CFoFCapEnt );

IMPLEMENT_SERVERCLASS_ST( CFoFCapEnt, DT_FoFCapEnt )
	SendPropBool( SENDINFO( m_bCapActive ) ),
	SendPropBool( SENDINFO( m_bBeingCaptured ) ),
	SendPropInt( SENDINFO( m_nClassFilter ) ),
	SendPropFloat( SENDINFO( m_flCapProgress ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

CFoFCapEnt::CFoFCapEnt()
{
	m_nClassFilter = 0;
	m_nAnnounceFilter = 0;
	m_flCaptureTime = 0.0f;
	m_bCapActive = false;
	m_bBeingCaptured = false;
	m_nRadius = 256;
	m_nMaxCapturers = 5;
	m_flCaptureScale = 1.5f;
	m_flCurrentCaptureTime = 0.0f;
	m_bTraceWall = false;
	m_flCapProgress = 0.0f;
	Q_memset( &m_WallTrace, 0, sizeof( m_WallTrace ) );
	m_flDecrementCaptureRate = 0.0f;
}

static const char *FoFCaptureModelForRadius( int nRadius )
{
	if ( nRadius <= 128 )
		return "models/props/cap_circle_256.mdl";
	if ( nRadius <= 256 )
		return "models/props/cap_circle_512.mdl";
	return "models/props/cap_circle_768.mdl";
}

static int FoFObjectiveCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static bool FoFObjectiveWarmupActive()
{
	static ConVarRef warmup( "fof_warmup", true );
	return warmup.IsValid() && warmup.GetBool();
}

static bool FoFObjectiveTeamReward( int nTeam, int &nReward )
{
	CBaseEntity *pEntity =
		gEntList.FindEntityByClassname( NULL, "fof_teamplay" );
	CTrigger_EndRound *pController =
		dynamic_cast< CTrigger_EndRound * >( pEntity );
	if ( !pController )
		return false;

	nReward = pController->GetTeamReward( nTeam );
	return true;
}

static void FoFFireShortEvent( const char *pszName,
	const char *pszKey, int nValue )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( pszName ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetInt( pszKey, nValue );
	gameeventmanager->FireEvent( pEvent );
}

static void FoFCreateTimedParticle( const char *pszEffectName,
	const Vector &vecOrigin, CBaseEntity *pParent, float flLifetime )
{
	CBaseEntity *pParticle = CreateEntityByName( "info_particle_system" );
	if ( !pParticle )
		return;

	pParticle->KeyValue( "start_active", "1" );
	pParticle->KeyValue( "effect_name", pszEffectName );
	pParticle->SetAbsOrigin( vecOrigin );
	if ( pParent )
		pParticle->SetParent( pParent );
	DispatchSpawn( pParticle );
	if ( gpGlobals->curtime > 0.5f )
		pParticle->Activate();
	pParticle->SetThink( &CBaseEntity::SUB_Remove );
	pParticle->SetNextThink( gpGlobals->curtime + flLifetime );
}

void CCapturePoint::SendCaptureMessage( CBasePlayer *pPlayer,
	float flCurrentCaptureTime, bool bCapturing )
{
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "CapMessage" );
		WRITE_BYTE( bCapturing );
		WRITE_SHORT( static_cast< int >( flCurrentCaptureTime ) );
		WRITE_SHORT( static_cast< int >( m_flCapture_time ) );
		WRITE_SHORT( entindex() );
		WRITE_STRING( STRING( GetEntityName() ) );
		WRITE_VEC3COORD( GetLocalOrigin() );
	MessageEnd();
}

BEGIN_DATADESC( CMobilePoint )
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_nTeam, FIELD_INTEGER, "iTeam" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_OUTPUT( m_OutputStartMoving, "OnStartMoving" ),
	DEFINE_OUTPUT( m_OutputStopMoving, "OnStopMoving" ),
	DEFINE_THINKFUNC( MoveThink ),
END_DATADESC()

CMobilePoint::CMobilePoint()
{
	m_pOperator = NULL;
	m_bDisabled = false;
	m_bMoving = false;
	m_flLastPushTime = 0.0f;
	m_flWheelAngle = 0.0f;
	m_bReverse = false;
	m_vecMoveDirection.Init();
	m_nTeam = 0;
}

void CMobilePoint::Precache()
{
	const char *pszModel = GetModelName() != NULL_STRING ?
		STRING( GetModelName() ) : "";
	PrecacheModel( pszModel, true );
}

void CMobilePoint::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel( GetModelName() != NULL_STRING ?
		STRING( GetModelName() ) : "" );
	SetMoveType( MOVETYPE_PUSH, MOVECOLLIDE_DEFAULT );
	SetSolid( SOLID_VPHYSICS );

	// FoF clears both map keyfields here.  Retain that shipped quirk:
	// StartDisabled and iTeam exist in the datamap but do not survive Spawn.
	m_nTeam = 0;
	m_bDisabled = false;
	m_bMoving = false;
	m_flLastPushTime = 0.0f;
	m_flWheelAngle = 0.0f;
	m_pOperator = NULL;
	UseClientSideAnimation();
	ChangeTeam( m_nTeam );
	m_bReverse = false;
	SetThink( &CMobilePoint::MoveThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CMobilePoint::InputEnable( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bDisabled = false;
}

void CMobilePoint::InputDisable( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bDisabled = true;
}

static Vector FoFFindMobilePointPassablePosition(
	CFoF_Player *pPlayer, const Vector &vecOrigin, float flRadius )
{
	if ( !pPlayer )
		return vec3_origin;

	QAngle searchAngles( 0.0f, 0.0f, 0.0f );
	for ( float flYaw = 0.0f; flYaw <= 400.0f; flYaw += 15.0f )
	{
		searchAngles.y = flYaw;
		Vector vecDirection;
		AngleVectors( searchAngles, &vecDirection );

		const Vector vecCandidate = vecOrigin + vecDirection * flRadius;
		trace_t groundTrace;
		UTIL_TraceLine(
			vecCandidate + Vector( 0.0f, 0.0f, 50.0f ),
			vecCandidate - Vector( 0.0f, 0.0f, 100.0f ),
			MASK_SOLID_BRUSHONLY, pPlayer,
			COLLISION_GROUP_PLAYER_MOVEMENT, &groundTrace );

		Vector vecGround = groundTrace.endpos;
		vecGround.z += 10.0f;

		trace_t playerTrace;
		UTIL_TraceEntity(
			pPlayer, vecGround, vecGround, MASK_PLAYERSOLID,
			pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &playerTrace );
		if ( playerTrace.startsolid || playerTrace.allsolid )
			continue;

		trace_t pathTrace;
		UTIL_TraceEntity(
			pPlayer,
			vecOrigin + Vector( 0.0f, 0.0f, 10.0f ),
			vecGround, MASK_SOLID_BRUSHONLY, pPlayer,
			COLLISION_GROUP_NONE, &pathTrace );
		if ( !pathTrace.startsolid && !pathTrace.allsolid )
			return pathTrace.endpos;
	}

	return vec3_origin;
}

static void FoFSendMobileCannonOperatorWarning( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "#Mobile_Cannon_Operator_Warning" );
	MessageEnd();
}

void CMobilePoint::MoveThink()
{
	Vector vecOrigin = GetAbsOrigin();
	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );
	vecForward.z = 0.0f;
	VectorNormalize( vecForward );

	if ( m_pOperator &&
		m_pOperator->GetAbsOrigin().DistTo( vecOrigin ) > 110.0f )
	{
		m_pOperator = NULL;
	}

	int nPushers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->pl.deadflag ||
			pPlayer->IsOnFoFHorse() || ( pPlayer->GetFlags() & FL_INWATER ) ||
			pPlayer->GetLocalOrigin().DistTo( GetLocalOrigin() ) > 110.0f )
		{
			continue;
		}

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() &&
			!pPlayer->HasFoFMobileCannonOperatorAccess() )
		{
			const float flNextWarning =
				pPlayer->GetFoFNextMobileCannonWarningTime();
			if ( flNextWarning <= gpGlobals->curtime &&
				gpGlobals->curtime != flNextWarning )
			{
				FoFSendMobileCannonOperatorWarning( pPlayer );
				pPlayer->SetFoFNextMobileCannonWarningTime(
					gpGlobals->curtime + 60.0f );
			}
			continue;
		}

		if ( m_nTeam != 0 && pPlayer->GetTeamNumber() != m_nTeam )
			continue;

		trace_t playerTrace;
		UTIL_TraceEntity(
			pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(),
			pPlayer->PlayerSolidMask( false ), pPlayer,
			COLLISION_GROUP_PLAYER_MOVEMENT, &playerTrace );
		if ( !playerTrace.startsolid && !playerTrace.allsolid )
		{
			if ( !m_pOperator )
				m_pOperator = pPlayer;
			++nPushers;
			continue;
		}

		Vector vecToCannon =
			( vecOrigin + Vector( 0.0f, 0.0f, 30.0f ) ) -
			pPlayer->GetAbsOrigin();
		if ( VectorNormalize( vecToCannon ) <= 0.0f )
			continue;

		const Vector vecClearanceTest =
			pPlayer->GetAbsOrigin() - vecToCannon * 50.0f;
		UTIL_TraceEntity(
			pPlayer, vecClearanceTest, vecClearanceTest,
			pPlayer->PlayerSolidMask( false ), pPlayer,
			COLLISION_GROUP_PLAYER_MOVEMENT, &playerTrace );
		if ( !playerTrace.startsolid && !playerTrace.allsolid )
		{
			pPlayer->SetAbsOrigin(
				pPlayer->GetAbsOrigin() -
				vecToCannon * ( gpGlobals->frametime * 55.0f ) );
			continue;
		}

		Vector vecPassable = FoFFindMobilePointPassablePosition(
			pPlayer, pPlayer->GetAbsOrigin(), 60.0f );
		if ( vecPassable == vec3_origin )
		{
			vecPassable = FoFFindMobilePointPassablePosition(
				pPlayer, pPlayer->GetAbsOrigin(), 100.0f );
		}
		if ( vecPassable != vec3_origin )
			pPlayer->SetAbsOrigin( vecPassable );
	}

	trace_t rightGroundTrace;
	const Vector vecRightGround = vecOrigin + vecRight * 25.0f;
	UTIL_TraceLine(
		vecRightGround + Vector( 0.0f, 0.0f, 50.0f ),
		vecRightGround - Vector( 0.0f, 0.0f, 150.0f ),
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE,
		&rightGroundTrace );

	trace_t leftGroundTrace;
	const Vector vecLeftGround = vecOrigin - vecRight * 25.0f;
	UTIL_TraceLine(
		vecLeftGround + Vector( 0.0f, 0.0f, 50.0f ),
		vecLeftGround - Vector( 0.0f, 0.0f, 150.0f ),
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE,
		&leftGroundTrace );

	QAngle localAngles = GetLocalAngles();
	const float flTargetRoll = RemapValClamped(
		rightGroundTrace.endpos.z - leftGroundTrace.endpos.z,
		-30.0f, 30.0f, 30.0f, -30.0f );
	if ( localAngles.z > flTargetRoll + 1.0f ||
		localAngles.z < flTargetRoll - 1.0f )
	{
		const float flRollStep = gpGlobals->frametime * 20.0f;
		localAngles.z += localAngles.z <= flTargetRoll ?
			flRollStep : -flRollStep;
		SetLocalAngles( localAngles );
	}

	if ( m_pOperator )
	{
		Vector vecOperatorVelocity = m_pOperator->GetAbsVelocity();
		VectorNormalize( vecOperatorVelocity );

		Vector vecOperatorRelation =
			vecOrigin - m_pOperator->GetAbsOrigin();
		vecOperatorRelation.z = 0.0f;
		VectorNormalize( vecOperatorRelation );

		const float flForwardRelation =
			DotProduct( vecOperatorRelation, vecForward );
		const float flRightRelation =
			DotProduct( vecOperatorRelation, vecRight );
		if ( m_pOperator->m_nButtons & IN_WALK )
		{
			QAngle angles = GetAbsAngles();
			if ( flRightRelation > 0.1f )
				angles.y += gpGlobals->frametime * 30.0f;
			if ( flRightRelation < -0.1f )
				angles.y -= gpGlobals->frametime * 30.0f;
			SetAbsAngles( angles );
		}
		else if ( flForwardRelation > 0.55f &&
			flForwardRelation < 0.96f &&
			DotProduct( vecOperatorVelocity, vecForward ) > 0.9f )
		{
			m_vecMoveDirection = vecForward;
			m_flLastPushTime = gpGlobals->curtime;
			m_bReverse = false;
		}
		else if ( flForwardRelation < -0.1f )
		{
			m_vecMoveDirection = -vecForward;
			m_flLastPushTime = gpGlobals->curtime;
			m_bReverse = true;
		}
	}

	bool bBlocked = false;
	if ( m_flLastPushTime > gpGlobals->curtime - 0.25f )
	{
		const Vector vecTraceStart = vecOrigin +
			m_vecMoveDirection * 10.0f + Vector( 0.0f, 0.0f, 40.0f );
		const Vector vecTraceEnd = vecOrigin + m_vecMoveDirection *
			( m_bReverse ? 50.0f : 20.0f ) +
			Vector( 0.0f, 0.0f, 40.0f );

		trace_t moveTrace;
		UTIL_TraceHull(
			vecTraceStart, vecTraceEnd,
			Vector( -20.0f, -20.0f, -10.0f ),
			Vector( 20.0f, 20.0f, 10.0f ),
			MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &moveTrace );
		if ( moveTrace.m_pEnt )
		{
			bBlocked = true;
			CBasePlayer *pHitPlayer = ToBasePlayer( moveTrace.m_pEnt );
			if ( pHitPlayer )
			{
				pHitPlayer->SetGroundEntity( NULL );
				pHitPlayer->SetAbsVelocity( vec3_origin );
				pHitPlayer->ApplyAbsVelocityImpulse(
					m_vecMoveDirection * 100.0f +
					Vector( 0.0f, 0.0f, 100.0f ) );
			}
			else
			{
				CTakeDamageInfo info(
					this, this, 100.0f, DMG_AIRBOAT );
				CalculateMeleeDamageForce(
					&info, m_vecMoveDirection,
					moveTrace.endpos, 1.0f );
				moveTrace.m_pEnt->DispatchTraceAttack(
					info, m_vecMoveDirection, &moveTrace, NULL );
				ApplyMultiDamage();
				TraceAttackToTriggers(
					info, vecTraceStart, vecTraceEnd,
					m_vecMoveDirection );
			}
		}

		vecOrigin +=
			m_vecMoveDirection * ( gpGlobals->frametime * 50.0f );
		const float flWheelSpeed =
			clamp( ( nPushers - 1 ) * 0.5f, 0.0f, 1.0f ) *
			50.0f + 100.0f;
		if ( m_bReverse )
		{
			m_flWheelAngle -= gpGlobals->frametime * flWheelSpeed;
			if ( m_flWheelAngle < 0.0f )
				m_flWheelAngle = 360.0f;
		}
		else
		{
			m_flWheelAngle += gpGlobals->frametime * flWheelSpeed;
			if ( m_flWheelAngle > 360.0f )
				m_flWheelAngle = 0.0f;
		}

		if ( !m_bMoving )
		{
			m_bMoving = true;
			m_OutputStartMoving.FireOutput( NULL, this );
		}
	}
	else if ( m_bMoving )
	{
		m_bMoving = false;
		m_OutputStopMoving.FireOutput( NULL, this );
	}

	const Vector vecSupportOffset = vecRight * 25.0f - vecForward * 10.0f;
	trace_t firstSupportTrace;
	UTIL_TraceLine(
		vecOrigin + vecSupportOffset + Vector( 0.0f, 0.0f, 20.0f ),
		vecOrigin + vecSupportOffset - Vector( 0.0f, 0.0f, 30.0f ),
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE,
		&firstSupportTrace );

	const Vector vecOtherSupportOffset =
		-vecRight * 25.0f - vecForward * 10.0f;
	trace_t secondSupportTrace;
	UTIL_TraceLine(
		vecOrigin + vecOtherSupportOffset + Vector( 0.0f, 0.0f, 20.0f ),
		vecOrigin + vecOtherSupportOffset - Vector( 0.0f, 0.0f, 30.0f ),
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE,
		&secondSupportTrace );

	if ( firstSupportTrace.fraction == 1.0f &&
		secondSupportTrace.fraction == 1.0f )
	{
		SetAbsOrigin(
			GetAbsOrigin() - vecUp * ( gpGlobals->frametime * 50.0f ) +
			vecForward * ( gpGlobals->frametime * 15.0f ) );
	}
	else if ( !bBlocked )
	{
		Vector vecGroundedOrigin;
		if ( firstSupportTrace.fraction != 1.0f )
		{
			vecGroundedOrigin = firstSupportTrace.endpos -
				vecRight * 25.0f + vecForward * 10.0f;
		}
		else
		{
			vecGroundedOrigin = secondSupportTrace.endpos +
				vecRight * 25.0f + vecForward * 10.0f;
		}

		Teleport( &vecGroundedOrigin, NULL, NULL );
		SetBoneController( 0, m_flWheelAngle );
	}

	SetNextThink( gpGlobals->curtime );
}

BEGIN_DATADESC( CCapturePoint )
	DEFINE_THINKFUNC( PlayerUpdateThink ),
	DEFINE_KEYFIELD( m_flCapture_time, FIELD_FLOAT, "Timer" ),
	DEFINE_KEYFIELD( m_nShowProgressBar, FIELD_INTEGER, "ShowBar" ),
	DEFINE_KEYFIELD( m_bActive, FIELD_BOOLEAN, "Enabled" ),
	DEFINE_KEYFIELD( m_nClassFilter, FIELD_INTEGER, "ClassFilter" ),
	DEFINE_OUTPUT( m_OnPlayerCapture, "OnCaptured" ),
	DEFINE_OUTPUT( m_OnPlayerCaptureStart, "OnCaptureStart" ),
	DEFINE_OUTPUT( m_OnPlayerHalfCapture, "OnHalfCapture" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

CCapturePoint::CCapturePoint()
{
	m_nClassFilter = 0;
	m_flCapture_time = 0.0f;
	m_nShowProgressBar = 0;
	m_flCurrentCaptureTime = 0.0f;
	m_bActive = false;
	m_bCaptureStarted = false;
	m_bHalfCaptureFired = false;
}

void CCapturePoint::Spawn()
{
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_PUSH );

	if ( showtriggers.GetInt() == 0 )
		AddEffects( EF_NODRAW );

	SetModel( STRING( GetModelName() ) );
	CreateVPhysics();
	m_bCaptureStarted = false;
	if ( m_flCapture_time > 0.0f )
		m_flCurrentCaptureTime = m_flCapture_time;
}

bool CCapturePoint::CreateVPhysics()
{
	VPhysicsInitShadow( false, false, NULL );
	return true;
}

void CCapturePoint::StartTouch( CBaseEntity *pOther )
{
	if ( !m_bActive || !pOther || !pOther->IsPlayer() || !pOther->IsAlive() )
		return;

	EHANDLE hPlayer = pOther;
	if ( m_hTouchingPlayers.Find( hPlayer ) != -1 )
		return;

	if ( m_hTouchingPlayers.Count() == 0 )
	{
		SetThink( &CCapturePoint::PlayerUpdateThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	m_hTouchingPlayers.AddToTail( hPlayer );
}

void CCapturePoint::EndTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() || !pOther->IsAlive() )
		return;

	EHANDLE hPlayer = pOther;
	m_hTouchingPlayers.FindAndRemove( hPlayer );
	if ( m_hTouchingPlayers.Count() == 0 &&
		m_flCurrentCaptureTime <= 0.0f )
	{
		SetNextThink( 0.0f );
	}
}

void CCapturePoint::PlayerUpdateThink()
{
	bool bCapturing = false;
	for ( int i = 0; i < m_hTouchingPlayers.Count(); ++i )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hTouchingPlayers[i].Get() );
		if ( !pPlayer || !pPlayer->IsAlive() || !m_bActive )
			continue;

		m_flCurrentCaptureTime -= 0.1f;
		if ( !m_bCaptureStarted )
		{
			m_OnPlayerCaptureStart.FireOutput( NULL, this );
			m_bCaptureStarted = true;
		}

		if ( !m_bHalfCaptureFired &&
			m_flCapture_time * 0.5f > m_flCurrentCaptureTime )
		{
			m_OnPlayerHalfCapture.FireOutput( NULL, this );
			m_bHalfCaptureFired = true;
		}

		bCapturing = true;
		break;
	}

	if ( m_flCurrentCaptureTime <= 0.0f )
	{
		m_OnPlayerCapture.FireOutput( NULL, this );
		m_bActive = false;
		SetNextThink( 0.0f );
	}

	if ( m_nShowProgressBar > 0 )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
				SendCaptureMessage(
					pPlayer, m_flCurrentCaptureTime, bCapturing );
		}
	}

	SetNextThink( m_flCurrentCaptureTime > 0.0f ?
		gpGlobals->curtime + 0.1f : 0.0f );
}

void CCapturePoint::InputEnable( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bActive = true;
}

void CCapturePoint::InputDisable( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bActive = false;
}

BEGIN_DATADESC( CFoFCapEnt )
	DEFINE_ENTITYFUNC( PickUpTouch ),
	DEFINE_THINKFUNC( CapThink ),
	DEFINE_KEYFIELD( m_nRadius, FIELD_INTEGER, "Radius" ),
	DEFINE_KEYFIELD( m_flCaptureTime, FIELD_FLOAT, "Timer" ),
	DEFINE_KEYFIELD( m_bCapActive, FIELD_BOOLEAN, "Enabled" ),
	DEFINE_KEYFIELD( m_bTraceWall, FIELD_BOOLEAN, "WallTrace" ),
	DEFINE_KEYFIELD( m_nClassFilter, FIELD_INTEGER, "ClassFilter" ),
	DEFINE_KEYFIELD( m_nAnnounceFilter, FIELD_INTEGER, "AnnounceFilter" ),
	DEFINE_KEYFIELD( m_flDecrementCaptureRate, FIELD_FLOAT, "DecrementRate" ),
	DEFINE_OUTPUT( m_OnPlayerCapture, "OnCaptured" ),
	DEFINE_OUTPUT( m_OnZoneEnabled, "OnZoneEnabled" ),
	DEFINE_OUTPUT( m_OnCapStarted, "OnCapStarted" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableZone", InputEnableZone ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableZone", InputDisableZone ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetCapProgressTime", InputSetCapProgressTime ),
END_DATADESC()

void CFoFCapEnt::Precache()
{
	PrecacheModel( "models/props/cap_circle_256.mdl", true );
	PrecacheModel( "models/props/cap_circle_512.mdl", true );
	PrecacheModel( "models/props/cap_circle_768.mdl", true );
}

void CFoFCapEnt::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel( FoFCaptureModelForRadius( m_nRadius ) );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	DispatchUpdateTransmitState();
	AddEffects( EF_NODRAW );

	m_nMaxCapturers = 5;
	m_flCaptureScale = 1.5f;
	m_flCurrentCaptureTime = 0.0f;
	m_flCapProgress = 0.0f;
	m_bBeingCaptured = false;
	m_CapturingPlayers.RemoveAll();
	ChangeTeam( m_nClassFilter );
	if ( m_nRadius > 128 )
		m_nRadius -= 70;
	m_nSkin = m_nClassFilter == 2 ? 1 : 2;

	ListenForGameEvent( "player_connect_fof" );
	if ( FoFObjectiveCurrentMode() == 4 )
	{
		AddEffects( EF_NOINTERP );
		m_nSkin = 0;
	}

	if ( m_bCapActive )
		EnableZone();
}

void CFoFCapEnt::PickUpTouch( CBaseEntity *pOther )
{
	NOTE_UNUSED( pOther );
}

void CFoFCapEnt::FireCapZoneEvent() const
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "cap_zone" ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetInt( "team", m_nClassFilter );
	pEvent->SetInt( "show_mode", m_nAnnounceFilter );
	pEvent->SetFloat( "pos_x", GetAbsOrigin().x );
	pEvent->SetFloat( "pos_y", GetAbsOrigin().y );
	pEvent->SetFloat( "pos_z", GetAbsOrigin().z );
	gameeventmanager->FireEvent( pEvent );
}

void CFoFCapEnt::FireCapZoneOffEvent() const
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "cap_zone_off" ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetFloat( "pos_x", GetAbsOrigin().x );
	pEvent->SetFloat( "pos_y", GetAbsOrigin().y );
	pEvent->SetFloat( "pos_z", GetAbsOrigin().z );
	gameeventmanager->FireEvent( pEvent );
}

void CFoFCapEnt::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent || Q_stricmp( pEvent->GetName(), "player_connect_fof" ) )
		return;

	const int nMode = FoFObjectiveCurrentMode();
	if ( nMode == 6 || nMode == 4 || FoFObjectiveWarmupActive() ||
		!m_bCapActive )
	{
		return;
	}

	FireCapZoneEvent();
}

void CFoFCapEnt::SendCaptureMessage(
	CBasePlayer *pPlayer, bool bContested ) const
{
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	UserMessageBegin( filter, "CapMessage" );
		WRITE_BYTE( m_CapturingPlayers.Count() );
		WRITE_BYTE( bContested ? 2 : 1 );
		WRITE_BYTE( clamp( static_cast< int >(
			m_flCapProgress * 100.0f ), 0, 100 ) );
	MessageEnd();
}

void CFoFCapEnt::UpdateCapture()
{
	if ( FoFObjectiveWarmupActive() )
		return;

	const bool bHadCapturers = m_CapturingPlayers.Count() > 0;
	m_CapturingPlayers.RemoveAll();
	bool bContested = false;
	const Vector vecCenter = GetAbsOrigin();
	const float flRadiusSqr = static_cast< float >( m_nRadius * m_nRadius );

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->IsObserver() )
			continue;
		if ( pPlayer->GetAbsOrigin().DistToSqr( vecCenter ) > flRadiusSqr )
			continue;
		if ( fabsf( pPlayer->GetAbsOrigin().z - vecCenter.z ) > 50.0f )
			continue;

		if ( pPlayer->GetTeamNumber() != m_nClassFilter )
		{
			bContested = true;
			continue;
		}

		pPlayer->RecordFoFCaptureParticipation(
			2.0f, static_cast< int >( m_flCaptureTime ) );
		m_CapturingPlayers.AddToTail( pPlayer->GetUserID() );
	}

	const int nCapturers = m_CapturingPlayers.Count();
	if ( nCapturers > 0 )
	{
		if ( !bHadCapturers )
			m_OnCapStarted.FireOutput( NULL, this );

		if ( !bContested )
		{
			float flCaptureStep = m_flCaptureScale;
			if ( m_nMaxCapturers > 1 )
			{
				flCaptureStep = RemapValClamped(
					static_cast< float >( nCapturers ),
					1.0f, static_cast< float >( m_nMaxCapturers ),
					1.0f, m_flCaptureScale );
			}
			else if ( nCapturers < m_nMaxCapturers )
			{
				flCaptureStep = 1.0f;
			}

			m_flCurrentCaptureTime += flCaptureStep;
			m_flCapProgress = clamp( m_flCurrentCaptureTime /
				MAX( m_flCaptureTime, 0.01f ), 0.0f, 1.0f );
			CTeam *pTeam = GetGlobalTeam( m_nClassFilter );
			int nTeamReward = 0;
			if ( pTeam && FoFObjectiveTeamReward(
				m_nClassFilter, nTeamReward ) )
			{
				pTeam->SetRoundsWon( static_cast< int >(
					m_flCapProgress * nTeamReward ) );
			}
			FoFFireShortEvent( "tp_capturing", "team", m_nClassFilter );
		}

		m_bBeingCaptured = true;
		for ( int i = 0; i < nCapturers; ++i )
		{
			SendCaptureMessage(
				UTIL_PlayerByUserId( m_CapturingPlayers[i] ), bContested );
		}
		return;
	}

	if ( m_flDecrementCaptureRate > 0.0f && m_flCapProgress > 0.0f )
	{
		m_flCurrentCaptureTime = MAX( 0.0f,
			m_flCurrentCaptureTime - m_flDecrementCaptureRate );
		m_flCapProgress = clamp( m_flCurrentCaptureTime /
			MAX( m_flCaptureTime, 0.01f ), 0.0f, 1.0f );
	}
	m_bBeingCaptured = false;
}

void CFoFCapEnt::CapThink()
{
	if ( m_flCapProgress < 1.0f )
	{
		UpdateCapture();
		SetNextThink( gpGlobals->curtime + 1.0f );
		return;
	}

	m_OnPlayerCapture.FireOutput( NULL, this );
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

void CFoFCapEnt::EnableZone()
{
	if ( FoFObjectiveCurrentMode() == 6 )
		return;

	FireCapZoneEvent();
	m_flCurrentCaptureTime = 0.0f;
	RemoveEffects( EF_NODRAW );
	DispatchUpdateTransmitState();
	SetThink( &CFoFCapEnt::CapThink );
	SetNextThink( gpGlobals->curtime + 0.01f );
	m_OnZoneEnabled.FireOutput( NULL, this );
}

void CFoFCapEnt::DisableZone()
{
	FireCapZoneOffEvent();
	m_flCurrentCaptureTime = 0.0f;
	AddEffects( EF_NODRAW );
	m_bCapActive = false;
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

void CFoFCapEnt::InputEnableZone( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bCapActive = true;
	EnableZone();
}

void CFoFCapEnt::InputDisableZone( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	DisableZone();
}

void CFoFCapEnt::InputSetCapProgressTime( inputdata_t &inputData )
{
	m_flCurrentCaptureTime = inputData.value.Float();
}

BEGIN_DATADESC( CCannonBall )
	DEFINE_ENTITYFUNC( BallTouch ),
	DEFINE_KEYFIELD( m_iszCannonModel, FIELD_STRING, "cannon_model" ),
	DEFINE_FIELD( m_hCannonModel, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iAttachmentIndex, FIELD_INTEGER, "iAttachIndex" ),
	DEFINE_KEYFIELD( m_fGravity, FIELD_FLOAT, "fGravity" ),
	DEFINE_KEYFIELD( m_iDamage, FIELD_INTEGER, "Damage" ),
	DEFINE_KEYFIELD( m_iRadius, FIELD_INTEGER, "Radius" ),
	DEFINE_KEYFIELD( m_bDirection, FIELD_BOOLEAN, "direction" ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "Launch", InputLaunch ),
END_DATADESC()

CCannonBall::CCannonBall()
{
	m_bLaunched = false;
	m_iDamage = 200;
	m_iRadius = 500;
	m_iAttachmentIndex = 0;
	m_bDirection = false;
	m_fGravity = 0.6f;
	m_hCannonModel = NULL;
	m_iszCannonModel = NULL_STRING;
	AddEffects( EF_NODRAW );
}

void CCannonBall::Precache()
{
	PrecacheModel( "models/weapons/cannon_ball.mdl" );
	PrecacheParticleSystem( "cannon_ball" );
	PrecacheParticleSystem( "explosion_turret_break_b" );
	PrecacheParticleSystem( "cannonball_impact" );
	PrecacheScriptSound( "TNTBow.Explosion" );
}

void CCannonBall::Spawn()
{
	Precache();
	SetModel( "models/weapons/cannon_ball.mdl" );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ) );
	SetSolid( SOLID_BBOX );
	SetGravity( m_fGravity );
	UpdateWaterState();
	SetTouch( &CCannonBall::BallTouch );

	if ( !m_hCannonModel && m_iszCannonModel != NULL_STRING )
	{
		CBaseEntity *pCannon = gEntList.FindEntityByName(
			NULL, m_iszCannonModel );
		m_hCannonModel = dynamic_cast< CBaseAnimating * >( pCannon );
	}
}

bool CCannonBall::CreateVPhysics()
{
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false, NULL );
	return true;
}

int CCannonBall::ObjectCaps()
{
	return ( BaseClass::ObjectCaps() & ~FCAP_NOTIFY_ON_TRANSITION ) |
		FCAP_WCEDIT_POSITION;
}

void CCannonBall::InputLaunch( inputdata_t &inputData )
{
	const int nLaunchSpeed = inputData.value.FieldType() == FIELD_INTEGER ?
		inputData.value.Int() : 0;
	if ( !inputData.pActivator )
	{
		Warning( "Cannon ball: no activator or caller!!\n" );
		return;
	}

	CBaseAnimating *pCannon = m_hCannonModel ?
		m_hCannonModel->GetBaseAnimating() : NULL;
	if ( !pCannon )
	{
		Warning( "Cannon ball: couldn't retrieve the cannon's aim attachment\n" );
		return;
	}

	Vector vecAttachment;
	QAngle angAttachment;
	if ( !pCannon->GetAttachment(
		m_iAttachmentIndex, vecAttachment, angAttachment ) )
	{
		Warning( "Cannon ball: couldn't retrieve the cannon's aim attachment\n" );
	}

	const QAngle angLaunch = m_bDirection ? GetAbsAngles() : angAttachment;
	Vector vecForward;
	AngleVectors( angLaunch, &vecForward );
	const Vector vecLaunchOrigin = vecAttachment + vecForward * 20.0f;

	CCannonBall *pBall = dynamic_cast< CCannonBall * >(
		CreateEntityByName( "fof_cannon_ball" ) );
	if ( !pBall )
		return;

	UTIL_SetOrigin( pBall, vecLaunchOrigin, false );
	pBall->SetAbsAngles( angLaunch );
	pBall->Spawn();
	pBall->SetOwnerEntity( inputData.pActivator );
	pBall->SetParent( NULL );
	pBall->m_bLaunched = true;
	pBall->SetAbsVelocity( vecForward * (float)nLaunchSpeed );
	pBall->m_iDamage = m_iDamage;
	pBall->m_iRadius = m_iRadius;
	pBall->m_fGravity = m_fGravity;
	pBall->RemoveEffects( EF_NODRAW );

	FoFCreateTimedParticle(
		"cannon_ball", pBall->GetAbsOrigin(), pBall, 4.0f );
	FoFCreateTimedParticle(
		"explosion_turret_break_b", pBall->GetAbsOrigin(), NULL, 4.0f );
}

void CCannonBall::BallTouch( CBaseEntity *pOther )
{
	if ( !pOther || pOther->GetSolid() == SOLID_NONE )
		return;
	if ( pOther->IsSolidFlagSet( FSOLID_NOT_SOLID | FSOLID_TRIGGER ) ||
		IsEffectActive( EF_NODRAW ) )
		return;

	ExplodeBall();
}

void CCannonBall::ExplodeBall()
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( !pOwner )
		pOwner = GetWorldEntity();

	const Vector vecOrigin = GetAbsOrigin();
	CPASFilter filter( vecOrigin );
	te->Explosion( filter, -1.0f, &vecOrigin, g_sModelIndexFireball,
		1.0f, 25, TE_EXPLFLAG_NONE, m_iRadius, m_iDamage,
		NULL, 'C' );

	CTakeDamageInfo info(
		this, pOwner, (float)m_iDamage, DMG_BLAST );
	RadiusDamage( info, vecOrigin, (float)m_iRadius, CLASS_NONE, NULL );
	EmitSound( "TNTBow.Explosion" );

	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin - Vector( 0, 0, 128 ),
		MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	UTIL_DecalTrace( &tr, "Scorch" );

	FoFCreateTimedParticle(
		"cannonball_impact", vecOrigin, NULL, 2.0f );
	SetAbsVelocity( vec3_origin );
	SetTouch( NULL );
	SetThink( NULL );
	UTIL_Remove( this );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF payload-cart controller used by the official teamplay maps.
//
//=============================================================================//

static const int FOF_CART_DISABLED_EFFECT = ( 1 << 12 );
static const int FOF_CART_MAX_NEARBY_ENTITIES = 256;
static const float FOF_CART_SCAN_INTERVAL = 0.5f;
static const float FOF_CART_THINK_INTERVAL = 0.1f;
static const float FOF_CART_TRACE_HEIGHT = 70.0f;

LINK_ENTITY_TO_CLASS( fof_cart_push, CFoFPushCart );

IMPLEMENT_SERVERCLASS_ST( CFoFPushCart, DT_FoFPushCart )
	SendPropBool( SENDINFO( m_bEnabled ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CFoFPushCart )
	DEFINE_THINKFUNC( CapThink ),
	DEFINE_KEYFIELD( m_szTrainName, FIELD_STRING, "TrainName" ),
	DEFINE_KEYFIELD( m_szMoveAnim, FIELD_STRING, "MoveAnim" ),
	DEFINE_KEYFIELD( m_szIdleAnim, FIELD_STRING, "IdleAnim" ),
	DEFINE_KEYFIELD( m_nRadius, FIELD_INTEGER, "Radius" ),
	DEFINE_KEYFIELD( m_nVigPushDir, FIELD_INTEGER, "VigPushDir" ),
	DEFINE_KEYFIELD( m_nDespPushDir, FIELD_INTEGER, "DespPushDir" ),
	DEFINE_KEYFIELD( m_nTotalPushTime, FIELD_INTEGER, "TotalPushTime" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleCart", InputToggleCart ),
END_DATADESC()

static bool FoFIsTeamplayCartMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() )
		return false;
	if ( currentMode.GetInt() == 2 )
		return true;
	if ( currentMode.GetInt() != 6 )
		return false;

	CCourseMode *pCourse = dynamic_cast< CCourseMode * >(
		gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
	return pCourse && pCourse->HasActiveTrainCheck();
}

static void FoFFireCartCaptureEvent( int nTeam )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "tp_capturing" ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetInt( "team", nTeam );
	gameeventmanager->FireEvent( pEvent );
}

CFoFPushCart::CFoFPushCart()
{
	m_szTrainName = NULL_STRING;
	m_szMoveAnim = NULL_STRING;
	m_szIdleAnim = NULL_STRING;
	m_nRadius = 256;
	m_nVigPushDir = 0;
	m_nDespPushDir = 0;
	m_nTotalPushTime = 0;
	m_bIdleAnimation = true;
	m_flNextScan = 0.0f;
	m_nAnimationDirection = -1;
	m_nTrainDirection = 0;
	m_bEnabled = true;
}

void CFoFPushCart::Spawn()
{
	BaseClass::Spawn();

	if ( GetModelName() == NULL_STRING ||
		!STRING( GetModelName() )[0] )
	{
		Warning( "prop at %.0f %.0f %.0f missing modelname\n",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	PrecacheModel( STRING( GetModelName() ) );
	SetModel( STRING( GetModelName() ) );

	CBaseEntity *pEntity = gEntList.FindEntityByName(
		NULL, m_szTrainName );
	CFuncTrackTrain *pTrain = dynamic_cast< CFuncTrackTrain * >( pEntity );
	if ( !pTrain )
	{
		Warning( "No parent train found at %.0f %.0f %.0f missing modelname\n",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	m_hTrain = pTrain;
	SetParent( pTrain );
	RemoveEFlags( EFL_TOUCHING_FLUID );
	SetTransmitState( FL_EDICT_ALWAYS );

	ListenForGameEvent( "round_start" );
	SetThink( &CFoFPushCart::CapThink );
	SetNextThink( gpGlobals->curtime + 0.01f );
}

void CFoFPushCart::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent || Q_stricmp( pEvent->GetName(), "round_start" ) )
		return;

	if ( m_nVigPushDir != 0 && m_nDespPushDir == 0 )
		ChangeTeam( 2 );
	else if ( m_nVigPushDir == 0 && m_nDespPushDir != 0 )
		ChangeTeam( 3 );
}

void CFoFPushCart::CapThink()
{
	if ( gpGlobals->curtime > m_flNextScan )
	{
		m_flNextScan = gpGlobals->curtime + FOF_CART_SCAN_INTERVAL;
		ScanForPushers();
	}

	if ( m_nAnimationDirection != -1 )
	{
		StudioFrameAdvance();
		DispatchAnimEvents( this );

		if ( ( m_nAnimationDirection > 0 && GetCycle() >= 0.999f ) ||
			( m_nAnimationDirection <= 0 && GetCycle() <= 0.0f ) )
		{
			m_nAnimationDirection = -1;
			SetPlaybackRate( 0.0f );
		}
	}

	SetNextThink( gpGlobals->curtime + FOF_CART_THINK_INTERVAL );
}

void CFoFPushCart::ScanForPushers()
{
	CFuncTrackTrain *pTrain = m_hTrain.Get();
	if ( !pTrain )
		return;

	if ( !FoFIsTeamplayCartMode() )
	{
		pTrain->Stop();
		return;
	}

	if ( !m_bEnabled )
		return;

	CBaseEntity *pNearby[FOF_CART_MAX_NEARBY_ENTITIES];
	const int nNearby = UTIL_EntitiesInSphere( pNearby,
		ARRAYSIZE( pNearby ), GetAbsOrigin(), (float)m_nRadius, 0 );
	int nVigilantes = 0;
	int nDesperados = 0;

	for ( int i = 0; i < nNearby; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( pNearby[i] );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		const Vector vecPlayerOrigin = pPlayer->GetAbsOrigin();
		if ( fabsf( vecPlayerOrigin.z - GetAbsOrigin().z ) >
			FOF_CART_TRACE_HEIGHT )
		{
			continue;
		}

		trace_t trace;
		UTIL_TraceLine(
			vecPlayerOrigin + Vector( 0, 0, FOF_CART_TRACE_HEIGHT ),
			GetAbsOrigin() + Vector( 0, 0, FOF_CART_TRACE_HEIGHT ),
			MASK_PLAYERSOLID_BRUSHONLY, this,
			COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0f )
			continue;

		if ( pPlayer->GetTeamNumber() == 2 && m_nVigPushDir != 0 )
		{
			++nVigilantes;
			pPlayer->RecordFoFCaptureParticipation(
				1.0f, m_nTotalPushTime );
		}
		else if ( pPlayer->GetTeamNumber() == 3 &&
			m_nDespPushDir != 0 )
		{
			++nDesperados;
			pPlayer->RecordFoFCaptureParticipation(
				1.0f, m_nTotalPushTime );
		}
	}

	if ( nVigilantes > 0 && nDesperados == 0 )
	{
		StartTrain( m_nVigPushDir );
		FoFFireCartCaptureEvent( 2 );
	}
	else if ( nDesperados > 0 && nVigilantes == 0 )
	{
		StartTrain( m_nDespPushDir );
		FoFFireCartCaptureEvent( 3 );
	}
	else if ( pTrain->GetCurrentSpeed() != 0.0f )
	{
		StopTrain();
	}
}

void CFoFPushCart::StartTrain( int nDirection )
{
	CFuncTrackTrain *pTrain = m_hTrain.Get();
	if ( !pTrain )
		return;

	m_nTrainDirection = nDirection;
	if ( m_bIdleAnimation && m_szMoveAnim != NULL_STRING )
		StartDirectionalAnimation( STRING( m_szMoveAnim ), nDirection );
	m_bIdleAnimation = false;

	inputdata_t inputData;
	Q_memset( &inputData, 0, sizeof( inputData ) );
	inputData.pActivator = this;
	inputData.pCaller = this;
	if ( nDirection == 1 )
		pTrain->InputStartForward( inputData );
	else
		pTrain->InputStartBackward( inputData );
}

void CFoFPushCart::StopTrain()
{
	CFuncTrackTrain *pTrain = m_hTrain.Get();
	if ( !pTrain )
		return;

	if ( m_szIdleAnim != NULL_STRING )
	{
		const int nSequence = LookupSequence( STRING( m_szIdleAnim ) );
		if ( nSequence >= 0 )
		{
			ResetSequence( nSequence );
			ResetSequenceInfo();
			SetCycle( 0.0f );
			SetPlaybackRate( 1.0f );
		}
	}

	pTrain->Stop();
	m_nTrainDirection = 0;
	m_nAnimationDirection = -1;
	m_bIdleAnimation = true;
}

void CFoFPushCart::StartDirectionalAnimation(
	const char *pszSequence, int nDirection )
{
	const int nSequence = LookupSequence( pszSequence );
	if ( nSequence < 0 )
		return;

	ResetSequence( nSequence );
	ResetSequenceInfo();
	SetAnimTime( gpGlobals->curtime );
	m_nAnimationDirection = nDirection;
	if ( nDirection > 0 )
	{
		SetPlaybackRate( 1.0f );
		SetCycle( 0.0f );
	}
	else
	{
		SetPlaybackRate( -1.0f );
		SetCycle( 0.999f );
	}
}

void CFoFPushCart::InputToggleCart( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	if ( m_bEnabled )
	{
		AddEffects( FOF_CART_DISABLED_EFFECT );
		m_bEnabled = false;
	}
	else
	{
		RemoveEffects( FOF_CART_DISABLED_EFFECT );
		m_bEnabled = true;
	}
}
