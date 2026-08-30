//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF warmup, round rebuild and dynamic respawn state.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_gamerules.h"
#include "fof/fof_ai_editor.h"
#include "fof/fof_player.h"
#include "fof/fof_player_statistics.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_rounds.h"
#include "fof/fof_course_mode.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int s_nLastFoFWarmupSecond = -1;

static bool FoFRespawnDebugEnabled()
{
	static ConVarRef debugRespawns( "fof_debug_respawns", true );
	return debugRespawns.IsValid() && debugRespawns.GetBool();
}

static bool FoFCanRestartAfterWarmup( CFoF_Player *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() &&
		!pPlayer->IsHLTV() && !pPlayer->IsReplay() &&
		pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

static void FoFRestartPlayersAfterWarmup( CHL2MPRules &rules )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bBreakBad = currentMode.IsValid() && currentMode.GetInt() == 3;
	// FoF clears every carried item before rebuilding the map.  The
	// selected Shootout loadout is stored separately on CFoF_Player and is
	// deliberately retained for the post-cleanup spawn transaction.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFCanRestartAfterWarmup( pPlayer ) )
			continue;

		pPlayer->RemoveFlag( FL_ATCONTROLS );
		if ( pPlayer->GetActiveWeapon() )
			pPlayer->GetActiveWeapon()->Holster();
		pPlayer->RemoveAllItems( true );
		pPlayer->ResetScores();
		if ( bBreakBad )
			pPlayer->m_flFoFCash = 0.0f;
	}

	rules.CleanUpMap();
	FoFLoadShootoutCustomPreset(
		gpGlobals->mapname != NULL_STRING ?
		STRING( gpGlobals->mapname ) : "" );

	// CFoF_Player::Spawn requires a FoF spawn chosen in advance.  Calling the
	// SDK respawn/ForceRespawn path directly skips that contract and sends the
	// player to spectator.  FinalizeFoFSpawn mirrors the shipped
	// CFoF_Player::TryRespawn(bool) path: select first, then invoke Spawn.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( FoFCanRestartAfterWarmup( pPlayer ) )
			pPlayer->FinalizeFoFSpawn( true );
	}

	// Map-owned listeners finish their round reset after the scene and players
	// have been rebuilt. BreakBad replaces its disabled crates at this event.
	if ( bBreakBad )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "round_start" );
		if ( pEvent )
			gameeventmanager->FireEvent( pEvent );
	}
}

static void FoFShowWarmupNotice( int nSecondsRemaining )
{
	char szMessage[32];
	Q_snprintf( szMessage, sizeof( szMessage ),
		"WARM-UP: %i", nSecondsRemaining );

	hudtextparms_t text;
	text.x = 0.15f;
	text.y = 0.085f;
	text.effect = 0;
	text.r1 = 255;
	text.g1 = 100;
	text.b1 = 0;
	text.a1 = 200;
	text.r2 = 0;
	text.g2 = 0;
	text.b2 = 0;
	text.a2 = 255;
	text.fadeinTime = 0.0f;
	text.fadeoutTime = 0.0f;
	text.holdTime = 1.1f;
	text.fxTime = 0.0f;
	text.channel = 3;
	UTIL_HudMessageAll( text, szMessage );
}

void CHL2MPRules::InitializeFoFWarmupState()
{
	static ConVarRef warmupTime( "fof_sv_obj_warmuptime", true );
	static ConVarRef warmup( "fof_warmup", true );
	static ConVarRef mapStartTime( "map_start_time", true );

	float flDuration = warmupTime.IsValid() ? warmupTime.GetFloat() : 25.0f;
	// FoF clamps dedicated servers to a 25-second minimum while still
	// allowing shorter listen-server warmups for local course/testing maps.
	if ( engine->IsDedicatedServer() && flDuration < 25.0f )
	{
		flDuration = 25.0f;
		if ( warmupTime.IsValid() )
			warmupTime.SetValue( 25 );
	}

	if ( warmup.IsValid() )
		warmup.SetValue( 1 );
	if ( mapStartTime.IsValid() )
		mapStartTime.SetValue( -1.0f );

	m_flFoFWarmupEndTime =
		gpGlobals->curtime + MAX( 0.0f, flDuration );
	m_bFoFWarmupComplete = false;
	s_nLastFoFWarmupSecond = -1;
}

void CHL2MPRules::UpdateFoFWarmupState()
{
	if ( m_bFoFWarmupComplete )
		return;

	static ConVarRef warmup( "fof_warmup", true );
	if ( !warmup.IsValid() || !warmup.GetBool() )
	{
		// Preserve an explicit administrator override without forcing an
		// additional map cleanup/restart behind the command's back.
		m_bFoFWarmupComplete = true;
		static ConVarRef mapStartTime( "map_start_time", true );
		if ( mapStartTime.IsValid() && mapStartTime.GetFloat() < 0.0f )
			mapStartTime.SetValue( gpGlobals->curtime );
		return;
	}

	// Explicit course launches can start once the player joins. An automatic
	// course lobby keeps the normal warmup before opening its task vote.
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bCourseMode = currentMode.IsValid() && currentMode.GetInt() == 6;
	bool bCoursePlayerJoined = false;
	if ( bCourseMode )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() &&
				pPlayer->GetTeamNumber() > TEAM_SPECTATOR )
			{
				bCoursePlayerJoined = true;
				break;
			}
		}
		if ( bCoursePlayerJoined && FoFHasConfiguredCourseForMap() )
		{
			FinishFoFWarmup();
			return;
		}
	}

	const int nSecondsRemaining = static_cast< int >(
		m_flFoFWarmupEndTime - gpGlobals->curtime );
	if ( nSecondsRemaining >= 0 &&
		nSecondsRemaining != s_nLastFoFWarmupSecond )
	{
		s_nLastFoFWarmupSecond = nSecondsRemaining;
		FoFShowWarmupNotice( nSecondsRemaining );
	}

	if ( gpGlobals->curtime >= m_flFoFWarmupEndTime &&
		( !bCourseMode || bCoursePlayerJoined ) )
		FinishFoFWarmup();
}

void CHL2MPRules::FinishFoFWarmup()
{
	if ( m_bFoFWarmupComplete )
		return;
	m_bFoFWarmupComplete = true;

	static ConVarRef warmup( "fof_warmup", true );
	static ConVarRef mapStartTime( "map_start_time", true );
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( warmup.IsValid() )
		warmup.SetValue( 0 );
	if ( mapStartTime.IsValid() )
		mapStartTime.SetValue( gpGlobals->curtime );
	s_nLastFoFWarmupSecond = -1;

	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
			FoFResetAccuracyStats( pPlayer );
	}
	// Shootout and BreakBad share the shipped state-1 round rebuild.  This is
	// not a second equipment-selection window: the existing selection survives
	// the cleanup and is applied once after the new FoF spawn is established.
	if ( nMode == 1 || nMode == 3 )
		FoFRestartPlayersAfterWarmup( *this );
}

static const unsigned int FOF_RESPAWN_TRACE_MASK = 0x400B;
static const float FOF_RESPAWN_PROBE_GRID = 450.0f;
static const float FOF_RESPAWN_RECENT_DISTANCE = 100.0f;
static const float FOF_RESPAWN_HOT_DISTANCE = 1000.0f;
static const float FOF_RESPAWN_CRATE_DISTANCE = 1200.0f;
static const float FOF_RESPAWN_PLAYER_DISTANCE = 2500.0f;
static const float FOF_RESPAWN_INFLUENCE_SCAN_DISTANCE = 2000.0f;
static const float FOF_RESPAWN_INFLUENCE_UPDATE_DISTANCE = 50.0f;
static const float FOF_RESPAWN_INFLUENCE_RADIUS = 500.0f;
static const float FOF_RESPAWN_SELECTED_INFLUENCE_RADIUS = 750.0f;
static const float FOF_RESPAWN_PLAYER_BLOCK_RADIUS = 150.0f;
static const int FOF_RESPAWN_TEAM_COUNT = 6;
static const int FOF_RESPAWN_MAX_BLOCKED_PROBES = 150;

struct FoFManualRespawnProbe_t
{
	char m_szMapName[64];
	unsigned int m_nNavAreaID;
	Vector m_vecOrigin;
};

struct FoFRespawnProbe_t
{
	FoFRespawnProbe_t() :
		m_pNavArea( NULL ), m_nNavAreaID( 0 ), m_flHotness( 0.1f )
	{
		m_vecOrigin.Init();
	}

	Vector m_vecOrigin;
	CNavArea *m_pNavArea;
	unsigned int m_nNavAreaID;
	float m_flHotness;
};

struct FoFSpawnProbeAssociation_t
{
	FoFSpawnProbeAssociation_t() :
		m_nProbeIndex( -1 ), m_flPathDistance( 0.0f ),
		m_bVisibleFromSpawn( false )
	{
		m_vecOrigin.Init();
	}

	int m_nProbeIndex;
	Vector m_vecOrigin;
	float m_flPathDistance;
	bool m_bVisibleFromSpawn;
};

struct FoFSpawnProbeRecord_t
{
	FoFSpawnProbeRecord_t() :
		m_hSpawn( NULL ), m_flHotness( 0.1f ), m_flNearCrates( 0.0f ),
		m_flTotalInfluence( 0.0f )
	{
		m_vecOrigin.Init();
		for ( int i = 0; i < FOF_RESPAWN_TEAM_COUNT; ++i )
			m_flTeamInfluence[i] = 0.0f;
	}

	EHANDLE m_hSpawn;
	Vector m_vecOrigin;
	CUtlVector< FoFSpawnProbeAssociation_t > m_Associations;
	float m_flHotness;
	float m_flNearCrates;
	float m_flTeamInfluence[FOF_RESPAWN_TEAM_COUNT];
	float m_flTotalInfluence;
};

struct FoFPlayerRespawnInfluence_t
{
	FoFPlayerRespawnInfluence_t() :
		m_nSpawnEntityIndex( -1 ), m_flInfluence( 0.0f )
	{
	}

	int m_nSpawnEntityIndex;
	float m_flInfluence;
};

struct FoFPlayerRespawnState_t
{
	FoFPlayerRespawnState_t() :
		m_flDangerSum( 0.0f ), m_flNearCratesSum( 0.0f ),
		m_flHotnessSum( 0.0f ), m_nSamples( 0 )
	{
		m_hPlayer = NULL;
		m_hThreat = NULL;
		m_vecLastInfluenceOrigin.Init();
	}

	void Reset( CFoF_Player *pPlayer )
	{
		m_hPlayer = pPlayer;
		m_hThreat = NULL;
		m_vecLastInfluenceOrigin.Init();
		m_Influences.RemoveAll();
		m_RecentSpawnEntityIndices.RemoveAll();
		m_flDangerSum = 0.0f;
		m_flNearCratesSum = 0.0f;
		m_flHotnessSum = 0.0f;
		m_nSamples = 0;
	}

	EHANDLE m_hPlayer;
	EHANDLE m_hThreat;
	Vector m_vecLastInfluenceOrigin;
	CUtlVector< FoFPlayerRespawnInfluence_t > m_Influences;
	CUtlVector< int > m_RecentSpawnEntityIndices;
	float m_flDangerSum;
	float m_flNearCratesSum;
	float m_flHotnessSum;
	int m_nSamples;
};

struct FoFGlobalRespawnHistory_t
{
	FoFGlobalRespawnHistory_t() :
		m_flDangerSum( 0.0f ), m_flNearCratesSum( 0.0f ),
		m_flHotnessSum( 0.0f ), m_nSamples( 0 )
	{
	}

	float m_flDangerSum;
	float m_flNearCratesSum;
	float m_flHotnessSum;
	int m_nSamples;
};

static CUtlVector< FoFManualRespawnProbe_t > g_FoFManualRespawnProbes;
static CUtlVector< FoFRespawnProbe_t > g_FoFRespawnProbes;
static CUtlVector< FoFSpawnProbeRecord_t > g_FoFSpawnProbeRecords;
static FoFPlayerRespawnState_t g_FoFPlayerRespawnStates[MAX_PLAYERS + 1];
static FoFGlobalRespawnHistory_t g_FoFGlobalRespawnHistory;
static Vector g_vecFoFRespawnDebugCoordinate;
static bool g_bFoFRespawnDebugCoordinateValid = false;
static bool g_bFoFRespawnProbeMapReady = false;
static int g_nFoFNextInfluenceRecord = 0;
static float g_flFoFNextRespawnSelectionTime = 0.0f;

static void FoFClearGeneratedRespawnProbes( void )
{
	g_FoFRespawnProbes.Purge();
	g_FoFSpawnProbeRecords.Purge();
	g_bFoFRespawnProbeMapReady = false;
	g_bFoFRespawnDebugCoordinateValid = false;
	g_vecFoFRespawnDebugCoordinate.Init();
	g_nFoFNextInfluenceRecord = 0;
	g_flFoFNextRespawnSelectionTime = 0.0f;
}

static void FoFClearAllRespawnProbeState( void )
{
	FoFClearGeneratedRespawnProbes();
	g_FoFManualRespawnProbes.Purge();
	for ( int i = 0; i < ARRAYSIZE( g_FoFPlayerRespawnStates ); ++i )
		g_FoFPlayerRespawnStates[i].Reset( NULL );
	g_FoFGlobalRespawnHistory = FoFGlobalRespawnHistory_t();
}

static bool FoFRespawnProbeHasClearance( const Vector &vecOrigin )
{
	trace_t trace;
	UTIL_TraceHull(
		vecOrigin, vecOrigin + Vector( 0.0f, 0.0f, 64.0f ),
		Vector( -2.0f, -2.0f, 0.0f ),
		Vector( 2.0f, 2.0f, 4.0f ),
		FOF_RESPAWN_TRACE_MASK, NULL,
		COLLISION_GROUP_NONE, &trace );
	if ( !trace.startsolid && trace.fraction >= 0.5f )
		return true;

	UTIL_TraceHull(
		vecOrigin, vecOrigin + Vector( 0.0f, 0.0f, 32.0f ),
		Vector( -2.0f, -2.0f, 0.0f ),
		Vector( 2.0f, 2.0f, 4.0f ),
		FOF_RESPAWN_TRACE_MASK, NULL,
		COLLISION_GROUP_NONE, &trace );
	return !trace.startsolid && trace.fraction >= 0.8f;
}

static bool FoFRespawnProbeTooCloseToRecent( const Vector &vecOrigin )
{
	const float flMinDistanceSqr =
		FOF_RESPAWN_RECENT_DISTANCE * FOF_RESPAWN_RECENT_DISTANCE;
	const int nFirst = MAX( 0, g_FoFRespawnProbes.Count() - 50 );
	for ( int i = nFirst; i < g_FoFRespawnProbes.Count(); ++i )
	{
		if ( g_FoFRespawnProbes[i].m_vecOrigin.DistToSqr( vecOrigin ) <
			flMinDistanceSqr )
		{
			return true;
		}
	}
	return false;
}

static void FoFAppendGeneratedRespawnProbe(
	const Vector &vecOrigin, CNavArea *pNavArea,
	bool bForce )
{
	if ( !bForce &&
		( FoFRespawnProbeTooCloseToRecent( vecOrigin ) ||
		  !FoFRespawnProbeHasClearance( vecOrigin ) ) )
	{
		return;
	}

	const int nProbe = g_FoFRespawnProbes.AddToTail();
	g_FoFRespawnProbes[nProbe].m_vecOrigin = vecOrigin;
	g_FoFRespawnProbes[nProbe].m_pNavArea = pNavArea;
	g_FoFRespawnProbes[nProbe].m_nNavAreaID =
		pNavArea ? pNavArea->GetID() : 0;
	g_vecFoFRespawnDebugCoordinate = vecOrigin;
	g_bFoFRespawnDebugCoordinateValid = true;
}

static void FoFTraceRespawnWorldLine(
	const Vector &vecStart, const Vector &vecEnd, trace_t *pTrace )
{
	CTraceFilterWorldOnly traceFilter;
	UTIL_TraceLine(
		vecStart, vecEnd, FOF_RESPAWN_TRACE_MASK,
		&traceFilter, pTrace );
}

static void FoFTraceRespawnUnfilteredLine(
	const Vector &vecStart, const Vector &vecEnd, trace_t *pTrace )
{
	Ray_t ray;
	ray.Init( vecStart, vecEnd );
	enginetrace->TraceRay(
		ray, FOF_RESPAWN_TRACE_MASK, NULL, pTrace );
}

static bool FoFRespawnPointsHaveLineOfSight(
	const Vector &vecStart, const Vector &vecEnd )
{
	trace_t trace;
	FoFTraceRespawnWorldLine(
		vecStart + Vector( 0.0f, 0.0f, 64.0f ),
		vecEnd + Vector( 0.0f, 0.0f, 64.0f ), &trace );
	return trace.fraction >= 1.0f;
}

static bool FoFRespawnProbesHaveHotnessLineOfSight(
	const Vector &vecStart, const Vector &vecEnd )
{
	trace_t trace;
	FoFTraceRespawnUnfilteredLine(
		vecStart + Vector( 0.0f, 0.0f, 64.0f ),
		vecEnd + Vector( 0.0f, 0.0f, 64.0f ), &trace );
	return trace.fraction >= 1.0f;
}

static void FoFComputeRespawnProbeHotness( void )
{
	const float flHotDistanceSqr =
		FOF_RESPAWN_HOT_DISTANCE * FOF_RESPAWN_HOT_DISTANCE;
	for ( int i = 0; i < g_FoFRespawnProbes.Count(); ++i )
	{
		float flRawHotness = 0.0f;
		const Vector &vecOrigin = g_FoFRespawnProbes[i].m_vecOrigin;
		for ( int j = 0; j < g_FoFRespawnProbes.Count(); ++j )
		{
			if ( i == j )
				continue;

			const float flDistanceSqr = vecOrigin.DistToSqr(
				g_FoFRespawnProbes[j].m_vecOrigin );
			if ( flDistanceSqr < 100.0f )
			{
				flRawHotness += 1.0f;
				continue;
			}
			if ( flDistanceSqr >= flHotDistanceSqr )
				continue;
			if ( !FoFRespawnProbesHaveHotnessLineOfSight(
				vecOrigin, g_FoFRespawnProbes[j].m_vecOrigin ) )
			{
				continue;
			}

			const float flDistance = FastSqrt( flDistanceSqr );
			flRawHotness += 1.0f -
				0.9f * clamp( flDistance / FOF_RESPAWN_HOT_DISTANCE,
					0.0f, 1.0f );
		}

		g_FoFRespawnProbes[i].m_flHotness = 0.1f +
			clamp( flRawHotness / 35.0f, 0.0f, 1.0f ) * 0.9f;
	}
}

static int FoFCollectRespawnProbes(
	const Vector &vecOrigin, float flUnusedDistance,
	float flPathDistance,
	CUtlVector< FoFSpawnProbeAssociation_t > *pAssociations )
{
	(void)flUnusedDistance;
	pAssociations->RemoveAll();

	CNavArea *pSpawnArea = TheNavMesh->GetNearestNavArea(
		vecOrigin, false, 150.0f, false, true, TEAM_ANY );
	if ( !pSpawnArea )
		return -1;

	CUtlVector< Vector > selectedOrigins;
	const Vector vecSpawnEye =
		vecOrigin + Vector( 0.0f, 0.0f, 64.0f );
	selectedOrigins.AddToTail( vecSpawnEye );
	const int nAnchor = pAssociations->AddToTail();
	FoFSpawnProbeAssociation_t &anchor = ( *pAssociations )[nAnchor];
	anchor.m_vecOrigin = vecSpawnEye;
	anchor.m_bVisibleFromSpawn = true;
	ShortestPathCost pathCost;

	for ( int i = 0; i < g_FoFRespawnProbes.Count(); ++i )
	{
		const FoFRespawnProbe_t &probe = g_FoFRespawnProbes[i];
		const Vector vecProbeEye =
			probe.m_vecOrigin + Vector( 0.0f, 0.0f, 64.0f );
		const bool bVisible = FoFRespawnPointsHaveLineOfSight(
			probe.m_vecOrigin, vecOrigin );
		float flAssociationPathDistance = 0.0f;

		if ( selectedOrigins.Count() >= FOF_RESPAWN_MAX_BLOCKED_PROBES )
		{
			if ( !bVisible )
				continue;
		}
		else if ( !bVisible )
		{
			const float flDirectDistance =
				probe.m_vecOrigin.DistTo( vecOrigin );
			if ( flDirectDistance > flPathDistance * 1.5f ||
				!probe.m_pNavArea )
			{
				continue;
			}

			const float flForwardPath = NavAreaTravelDistance(
				pSpawnArea, probe.m_pNavArea, pathCost );
			const float flReversePath = NavAreaTravelDistance(
				probe.m_pNavArea, pSpawnArea, pathCost );
			float flShortestPath = -1.0f;
			if ( flForwardPath >= 0.0f )
				flShortestPath = flForwardPath;
			if ( flReversePath >= 0.0f &&
				( flShortestPath < 0.0f ||
				  flReversePath < flShortestPath ) )
			{
				flShortestPath = flReversePath;
			}
			if ( flShortestPath < 0.0f ||
				flShortestPath > flPathDistance )
			{
				continue;
			}
			flAssociationPathDistance = flShortestPath;

			const float flMinimumSeparation = RemapValClamped(
				flDirectDistance, 0.0f, flPathDistance,
				0.0f, 250.0f );
			bool bTooClose = false;
			for ( int j = 0; j < selectedOrigins.Count(); ++j )
			{
				if ( selectedOrigins[j].DistTo( vecProbeEye ) >=
					flMinimumSeparation )
				{
					continue;
				}

				trace_t trace;
				FoFTraceRespawnWorldLine(
					selectedOrigins[j], vecProbeEye, &trace );
				if ( trace.fraction >= 1.0f )
				{
					bTooClose = true;
					break;
				}
			}
			if ( bTooClose )
				continue;
		}

		const int nAssociation = pAssociations->AddToTail();
		FoFSpawnProbeAssociation_t &association =
			( *pAssociations )[nAssociation];
		association.m_nProbeIndex = i;
		association.m_vecOrigin = vecProbeEye;
		association.m_flPathDistance = flAssociationPathDistance;
		association.m_bVisibleFromSpawn = bVisible;
		selectedOrigins.AddToTail( vecProbeEye );
	}

	// The original candidate vector contains the spawn itself as element zero.
	return selectedOrigins.Count();
}

static float FoFComputeSpawnHotness(
	const FoFSpawnProbeRecord_t &record )
{
	if ( record.m_Associations.Count() <= 1 )
		return 0.1f;

	float flHotness = 0.0f;
	for ( int i = 0; i < record.m_Associations.Count(); ++i )
	{
		const int nProbe = record.m_Associations[i].m_nProbeIndex;
		if ( nProbe >= 0 && nProbe < g_FoFRespawnProbes.Count() )
			flHotness += g_FoFRespawnProbes[nProbe].m_flHotness;
	}
	// The original averages the zero-hotness spawn anchor together with probes.
	return flHotness / record.m_Associations.Count();
}

static float FoFComputeSpawnNearCrates( const Vector &vecOrigin )
{
	static const char *s_pszCrateClasses[] =
	{
		"fof_crate",
		"fof_crate_med",
		"fof_crate_low"
	};

	float flNearCrates = 0.0f;
	for ( int nClass = 0; nClass < ARRAYSIZE( s_pszCrateClasses ); ++nClass )
	{
		CBaseEntity *pCrate = NULL;
		while ( ( pCrate = gEntList.FindEntityByClassname(
			pCrate, s_pszCrateClasses[nClass] ) ) != NULL )
		{
			const Vector &vecCrateOrigin = pCrate->GetAbsOrigin();
			const float flDistance = vecOrigin.DistTo( vecCrateOrigin );
			if ( flDistance >= FOF_RESPAWN_CRATE_DISTANCE * 0.7f )
				continue;

			CNavArea *pSpawnArea = TheNavMesh->GetNearestNavArea(
				vecOrigin, false, 10000.0f, false, true, TEAM_ANY );
			CNavArea *pCrateArea = TheNavMesh->GetNearestNavArea(
				vecCrateOrigin, false, 10000.0f, false, true, TEAM_ANY );
			if ( !pSpawnArea || !pCrateArea )
				continue;

			ShortestPathCost pathCost;
			const float flPathDistance = NavAreaTravelDistance(
				pSpawnArea, pCrateArea, pathCost );
			if ( flPathDistance >= FOF_RESPAWN_CRATE_DISTANCE )
				continue;

			const float flCrateClassScale = 1.0f -
				clamp( nClass * 0.5f, 0.0f, 1.0f ) * 0.67f;
			float flPathScale = 1.0f;
			if ( FOF_RESPAWN_CRATE_DISTANCE != 150.0f )
			{
				flPathScale -= clamp(
					( flPathDistance - 150.0f ) /
						( FOF_RESPAWN_CRATE_DISTANCE - 150.0f ),
					0.0f, 1.0f ) * 0.5f;
			}
			flNearCrates += flPathScale * flCrateClassScale;
		}
	}
	return flNearCrates;
}

static void FoFBuildSpawnProbeRecords( void )
{
	static const float s_flProbeSpacingDistances[] =
	{
		1200.0f,
		1500.0f,
		1800.0f
	};
	static const float s_flProbePathDistances[] =
	{
		1600.0f,
		1900.0f,
		2200.0f
	};
	static const int s_nMinimumProbeCounts[] =
	{
		30,
		30,
		10
	};

	CBaseEntity *pSpawn = NULL;
	int nSpawnIndex = 0;
	while ( ( pSpawn = gEntList.FindEntityByClassname(
		pSpawn, "info_player_fof" ) ) != NULL )
	{
		const int nRecord = g_FoFSpawnProbeRecords.AddToTail();
		FoFSpawnProbeRecord_t &record =
			g_FoFSpawnProbeRecords[nRecord];
		record.m_hSpawn = pSpawn;
		record.m_vecOrigin = pSpawn->GetAbsOrigin();

		if ( FoFRespawnDebugEnabled() )
		{
			Msg( "\n ======= Generating Probes for Respawn %d ======= \n\n",
				nSpawnIndex );
		}
		int nProbeCount = -1;
		for ( int nAttempt = 0;
			nAttempt < ARRAYSIZE( s_flProbeSpacingDistances );
			++nAttempt )
		{
			nProbeCount = FoFCollectRespawnProbes(
				record.m_vecOrigin,
				s_flProbeSpacingDistances[nAttempt],
				s_flProbePathDistances[nAttempt],
				&record.m_Associations );
			if ( nProbeCount >=
				s_nMinimumProbeCounts[nAttempt] )
			{
				break;
			}
		}

		if ( FoFRespawnDebugEnabled() )
		{
			Msg( " %d probes found for spot %d \n\n",
				nProbeCount, pSpawn->entindex() );
		}
		record.m_flHotness = FoFComputeSpawnHotness( record );
		record.m_flNearCrates =
			FoFComputeSpawnNearCrates( record.m_vecOrigin );
		++nSpawnIndex;
	}

	if ( FoFRespawnDebugEnabled() )
	{
		for ( int i = 0; i < g_FoFSpawnProbeRecords.Count(); ++i )
		{
			CBaseEntity *pRecordSpawn =
				g_FoFSpawnProbeRecords[i].m_hSpawn.Get();
			Msg( "hot pct for spot %d (%d): %f\n",
				i, pRecordSpawn ? pRecordSpawn->entindex() : -1,
				g_FoFSpawnProbeRecords[i].m_flHotness );
		}
	}
}

static FoFPlayerRespawnState_t *FoFGetPlayerRespawnState(
	CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return NULL;

	const int nPlayerIndex = pPlayer->entindex();
	if ( nPlayerIndex <= 0 ||
		nPlayerIndex >= ARRAYSIZE( g_FoFPlayerRespawnStates ) )
	{
		return NULL;
	}

	FoFPlayerRespawnState_t &state =
		g_FoFPlayerRespawnStates[nPlayerIndex];
	if ( state.m_hPlayer.Get() != pPlayer )
		state.Reset( pPlayer );
	return &state;
}

static bool FoFRespawnCentersHaveLineOfSight(
	const Vector &vecStart, const Vector &vecEnd )
{
	trace_t trace;
	FoFTraceRespawnWorldLine( vecStart, vecEnd, &trace );
	return !trace.startsolid && trace.fraction >= 1.0f;
}

static void FoFRebuildPlayerRespawnInfluences(
	CFoF_Player *pPlayer, const Vector &vecCenter, float flRadius )
{
	FoFPlayerRespawnState_t *pState =
		FoFGetPlayerRespawnState( pPlayer );
	if ( !pState )
		return;

	pState->m_Influences.RemoveAll();
	for ( int i = 0; i < g_FoFSpawnProbeRecords.Count(); ++i )
	{
		FoFSpawnProbeRecord_t &record = g_FoFSpawnProbeRecords[i];
		CBaseEntity *pSpawn = record.m_hSpawn.Get();
		if ( !pSpawn || record.m_Associations.Count() == 0 ||
			record.m_vecOrigin.DistTo( vecCenter ) >
				FOF_RESPAWN_INFLUENCE_SCAN_DISTANCE )
		{
			continue;
		}

		int nBestAssociation = -1;
		if ( FoFRespawnCentersHaveLineOfSight(
			record.m_vecOrigin, vecCenter ) )
		{
			nBestAssociation = 0;
		}
		else
		{
			int nBestOrder = INT_MAX;
			for ( int j = 0; j < record.m_Associations.Count(); ++j )
			{
				const FoFSpawnProbeAssociation_t &association =
					record.m_Associations[j];
				const float flDistance =
					vecCenter.DistTo( association.m_vecOrigin );
				if ( flDistance > flRadius ||
					!FoFRespawnCentersHaveLineOfSight(
						vecCenter, association.m_vecOrigin ) )
				{
					continue;
				}

				const int nOrder = association.m_bVisibleFromSpawn ?
					0 : static_cast< int >( flDistance );
				if ( nBestAssociation < 0 || nOrder < nBestOrder )
				{
					nBestAssociation = j;
					nBestOrder = nOrder;
				}
			}
		}

		if ( nBestAssociation < 0 )
			continue;

		const FoFSpawnProbeAssociation_t &association =
			record.m_Associations[nBestAssociation];
		const Vector &vecSpawnOrigin = pSpawn->GetAbsOrigin();
		float flTravelDistance = association.m_bVisibleFromSpawn ?
			0.0f : association.m_flPathDistance;
		const float flPlayerToSpawn =
			vecCenter.DistTo( vecSpawnOrigin );
		const float flProbeToSpawn =
			association.m_vecOrigin.DistTo( vecSpawnOrigin );
		const float flPlayerToProbe =
			vecCenter.DistTo( association.m_vecOrigin );
		if ( flProbeToSpawn > flPlayerToSpawn )
			flTravelDistance -= flPlayerToProbe;
		else
			flTravelDistance += flPlayerToProbe;

		const float flInfluence = 1.0f -
			clamp( ( flTravelDistance - 100.0f ) / 2420.0f,
				0.0f, 1.0f ) * 0.99f;
		const int nInfluence = pState->m_Influences.AddToTail();
		pState->m_Influences[nInfluence].m_nSpawnEntityIndex =
			pSpawn->entindex();
		pState->m_Influences[nInfluence].m_flInfluence = flInfluence;
	}
}

static float FoFFindPlayerRespawnInfluence(
	const FoFPlayerRespawnState_t &state, int nSpawnEntityIndex )
{
	for ( int i = 0; i < state.m_Influences.Count(); ++i )
	{
		if ( state.m_Influences[i].m_nSpawnEntityIndex ==
			nSpawnEntityIndex )
		{
			return state.m_Influences[i].m_flInfluence;
		}
	}
	return 0.0f;
}

static void FoFRefreshSpawnInfluenceRecord( int nRecord )
{
	if ( g_FoFSpawnProbeRecords.Count() == 0 )
		return;

	const bool bRotate = nRecord < 0;
	if ( bRotate )
		nRecord = g_nFoFNextInfluenceRecord;
	if ( nRecord < 0 || nRecord >= g_FoFSpawnProbeRecords.Count() )
		nRecord = 0;

	FoFSpawnProbeRecord_t &record = g_FoFSpawnProbeRecords[nRecord];
	for ( int i = 0; i < FOF_RESPAWN_TEAM_COUNT; ++i )
		record.m_flTeamInfluence[i] = 0.0f;
	record.m_flTotalInfluence = 0.0f;

	CBaseEntity *pSpawn = record.m_hSpawn.Get();
	if ( pSpawn )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pOther = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pOther || !pOther->IsAlive() )
				continue;

			FoFPlayerRespawnState_t *pState =
				FoFGetPlayerRespawnState( pOther );
			if ( !pState )
				continue;
			const float flInfluence = FoFFindPlayerRespawnInfluence(
				*pState, pSpawn->entindex() );
			if ( flInfluence <= 0.0f )
				continue;

			const int nTeam = clamp(
				pOther->GetTeamNumber(), 0, FOF_RESPAWN_TEAM_COUNT - 1 );
			record.m_flTeamInfluence[nTeam] += flInfluence;
			record.m_flTotalInfluence += flInfluence;
		}
	}

	if ( bRotate )
	{
		g_nFoFNextInfluenceRecord =
			( nRecord + 1 ) % g_FoFSpawnProbeRecords.Count();
	}
}

static float FoFRecentSpawnWeight(
	const FoFPlayerRespawnState_t &state,
	int nSpawnEntityIndex, int nWindow )
{
	int nPosition = nWindow + 1;
	for ( int i = 0; i < state.m_RecentSpawnEntityIndices.Count(); ++i )
	{
		if ( state.m_RecentSpawnEntityIndices[i] == nSpawnEntityIndex ||
			i > nWindow + 1 )
		{
			nPosition = i;
			break;
		}
	}

	if ( nWindow <= 1 )
		return nPosition < nWindow ? 1.0f : 0.0f;
	return 1.0f - clamp(
		static_cast< float >( nPosition - 1 ) /
			static_cast< float >( nWindow - 1 ),
		0.0f, 1.0f );
}

struct FoFRespawnCandidate_t
{
	float m_flScore;
	int m_nSpawnEntityIndex;
	int m_nRecord;
	float m_flDanger;
	float m_flHotness;
	float m_flNearCrates;
};

static int FoFSortRespawnCandidates(
	const FoFRespawnCandidate_t *pLeft,
	const FoFRespawnCandidate_t *pRight )
{
	if ( pLeft->m_flScore > pRight->m_flScore )
		return -1;
	if ( pRight->m_flScore > pLeft->m_flScore )
		return 1;
	return 0;
}

static bool FoFBuildRespawnCandidate(
	CFoF_Player *pPlayer, FoFPlayerRespawnState_t *pState,
	int nRecord, bool bTeamplay, FoFRespawnCandidate_t *pCandidate )
{
	FoFSpawnProbeRecord_t &record = g_FoFSpawnProbeRecords[nRecord];
	CBaseEntity *pSpawn = record.m_hSpawn.Get();
	if ( !pSpawn )
		return false;

	const int nTeam = clamp(
		pPlayer->GetTeamNumber(), 0, FOF_RESPAWN_TEAM_COUNT - 1 );
	float flScore = 0.0f;
	float flCandidateDanger = 0.0f;
	if ( bTeamplay )
	{
		float flOwnInfluence = 0.0f;
		float flOtherInfluence = 0.0f;
		float flOtherSaturation = 0.0f;
		for ( int i = 0; i < FOF_RESPAWN_TEAM_COUNT; ++i )
		{
			const float flInfluence = record.m_flTeamInfluence[i];
			if ( i == nTeam )
				flOwnInfluence += flInfluence;
			else
			{
				flOtherInfluence += flInfluence;
				flOtherSaturation +=
					clamp( flInfluence * 2.0f, 0.0f, 1.0f );
			}
		}

		const float flOwnScale = 0.5f -
			clamp( ( gpGlobals->curtime -
				pPlayer->GetFoFLifeStartTime() - 1.0f ) * 0.25f,
				0.0f, 1.0f ) * 0.4f;
		const float flOwnDanger = flOwnInfluence * flOwnScale;
		if ( flOwnDanger > 0.3f )
			return false;

		const float flOtherScale = 1.0f +
			clamp( ( flOtherSaturation - 1.5f ) * ( 2.0f / 3.0f ),
				0.0f, 1.0f ) * 0.25f;
		const float flCalculatedDanger =
			flOwnDanger + flOtherInfluence * flOtherScale;
		if ( flCalculatedDanger > 0.3f )
			return false;

		if ( pState->m_nSamples > 3 )
		{
			const float flPlayerAverage = pState->m_flDangerSum /
				static_cast< float >( pState->m_nSamples );
			flScore = RemapValClamped(
				flPlayerAverage - flCalculatedDanger,
				-0.25f, 0.3f, 0.0f, 0.15f );
		}
		else
		{
			flScore = 1.0f -
				clamp( record.m_flTotalInfluence, 0.0f, 1.0f );
		}
		flCandidateDanger = flOtherInfluence;
	}
	else
	{
		const float flDanger = record.m_flTeamInfluence[nTeam];
		if ( flDanger > 0.4f )
			return false;

		if ( pState->m_nSamples > 3 )
		{
			const float flGlobalAverage =
				g_FoFGlobalRespawnHistory.m_flDangerSum /
				static_cast< float >( MAX(
					g_FoFGlobalRespawnHistory.m_nSamples, 1 ) );
			const float flPlayerAverage = pState->m_flDangerSum /
				static_cast< float >( pState->m_nSamples );
			const float flScale =
				flGlobalAverage - flPlayerAverage > 0.0f ? 0.5f : -0.5f;
			flScore = RemapValClamped(
				flDanger, 0.0f, 0.3f, 0.0f, flScale );
		}
		else
		{
			flScore = 1.0f - clamp( flDanger, 0.0f, 1.0f );
		}
		flCandidateDanger = flDanger;
	}

	if ( pState->m_nSamples > 3 )
	{
		const float flGlobalSamples = static_cast< float >(
			MAX( g_FoFGlobalRespawnHistory.m_nSamples, 1 ) );
		const float flPlayerSamples =
			static_cast< float >( pState->m_nSamples );
		const float flNearCratesScale =
			g_FoFGlobalRespawnHistory.m_flNearCratesSum /
				flGlobalSamples -
			pState->m_flNearCratesSum / flPlayerSamples > 0.0f ?
				0.15f : -0.15f;
		flScore += clamp( record.m_flNearCrates, 0.0f, 1.0f ) *
			flNearCratesScale;

		const float flHotnessScale =
			g_FoFGlobalRespawnHistory.m_flHotnessSum /
				flGlobalSamples -
			pState->m_flHotnessSum / flPlayerSamples > 0.0f ?
				0.1f : -0.1f;
		flScore += clamp( record.m_flHotness, 0.0f, 1.0f ) *
			flHotnessScale;

		flScore += 0.2f - FoFRecentSpawnWeight(
			*pState, pSpawn->entindex(), 5 ) * 0.4f;
		CBaseEntity *pThreat = pState->m_hThreat.Get();
		if ( pThreat )
		{
			flScore -= RemapValClamped(
				record.m_vecOrigin.DistTo( pThreat->GetAbsOrigin() ),
				500.0f, FOF_RESPAWN_PLAYER_DISTANCE,
				0.5f, 0.0f );
		}
	}

	pCandidate->m_flScore = flScore;
	pCandidate->m_nSpawnEntityIndex = pSpawn->entindex();
	pCandidate->m_nRecord = nRecord;
	pCandidate->m_flDanger = flCandidateDanger;
	pCandidate->m_flHotness = record.m_flHotness;
	pCandidate->m_flNearCrates = record.m_flNearCrates;
	return true;
}

static bool FoFSpawnHasBlockingPlayer(
	CFoF_Player *pPlayer, const Vector &vecOrigin )
{
	CBaseEntity *pEntity = NULL;
	for ( CEntitySphereQuery query(
		vecOrigin, FOF_RESPAWN_PLAYER_BLOCK_RADIUS );
		( pEntity = query.GetCurrentEntity() ) != NULL;
		query.NextEntity() )
	{
		if ( pEntity == pPlayer || !pEntity->IsPlayer() ||
			!pEntity->IsAlive() )
			continue;
		return true;
	}
	return false;
}

void FoFAddManualRespawnProbe(
	const char *pszMapName, unsigned int nNavAreaID,
	const Vector &vecOrigin )
{
	if ( !pszMapName || !pszMapName[0] )
		return;

	const int nProbe = g_FoFManualRespawnProbes.AddToTail();
	FoFManualRespawnProbe_t &probe =
		g_FoFManualRespawnProbes[nProbe];
	Q_strncpy( probe.m_szMapName, pszMapName,
		sizeof( probe.m_szMapName ) );
	probe.m_nNavAreaID = nNavAreaID;
	probe.m_vecOrigin = vecOrigin;
}

void FoFGenerateRespawnProbeMap( void )
{
	FoFClearGeneratedRespawnProbes();
	if ( FoFRespawnDebugEnabled() )
		Msg( "Starting Global Probe Generation Sweep...\n" );

	for ( int nArea = 0; nArea < TheNavAreas.Count(); ++nArea )
	{
		CNavArea *pArea = TheNavAreas[nArea];
		if ( !pArea )
			continue;

		Extent extent;
		pArea->GetExtent( &extent );
		const int nColumns = MAX( 1,
			Ceil2Int( extent.SizeX() / FOF_RESPAWN_PROBE_GRID ) );
		const int nRows = MAX( 1,
			Ceil2Int( extent.SizeY() / FOF_RESPAWN_PROBE_GRID ) );
		const float flCellWidth = extent.SizeX() / nColumns;
		const float flCellHeight = extent.SizeY() / nRows;

		for ( int x = 0; x < nColumns; ++x )
		{
			for ( int y = 0; y < nRows; ++y )
			{
				Vector vecProbe(
					extent.lo.x + ( x + 0.5f ) * flCellWidth,
					extent.lo.y + ( y + 0.5f ) * flCellHeight,
					0.0f );
				vecProbe.z = pArea->GetZ( vecProbe.x, vecProbe.y ) + 2.0f;
				FoFAppendGeneratedRespawnProbe(
					vecProbe, pArea, false );
			}
		}
	}

	const char *pszCurrentMap = gpGlobals->mapname.ToCStr();
	for ( int i = 0; i < g_FoFManualRespawnProbes.Count(); ++i )
	{
		const FoFManualRespawnProbe_t &probe =
			g_FoFManualRespawnProbes[i];
		if ( !Q_stricmp( pszCurrentMap, probe.m_szMapName ) )
		{
			CNavArea *pArea = TheNavMesh->GetNavAreaByID(
				probe.m_nNavAreaID );
			FoFAppendGeneratedRespawnProbe(
				probe.m_vecOrigin, pArea, true );
		}
	}

	if ( FoFRespawnDebugEnabled() )
	{
		Msg( "Global Sweep Complete: %d probes.\n",
			g_FoFRespawnProbes.Count() );
	}
	FoFComputeRespawnProbeHotness();
	FoFBuildSpawnProbeRecords();
	g_bFoFRespawnProbeMapReady =
		g_FoFRespawnProbes.Count() > 0 &&
		g_FoFSpawnProbeRecords.Count() > 0;
}

bool FoFGetRespawnProbeDebugCoordinate( Vector *pCoordinate )
{
	if ( !pCoordinate || !g_bFoFRespawnDebugCoordinateValid )
		return false;
	*pCoordinate = g_vecFoFRespawnDebugCoordinate;
	return true;
}

bool FoFHasRespawnProbeMap( void )
{
	return g_bFoFRespawnProbeMapReady;
}

void FoFUpdatePlayerRespawnInfluence( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !FoFHasRespawnProbeMap() )
		return;

	FoFPlayerRespawnState_t *pState =
		FoFGetPlayerRespawnState( pPlayer );
	if ( !pState )
		return;

	const Vector &vecOrigin = pPlayer->GetAbsOrigin();
	if ( pState->m_vecLastInfluenceOrigin.DistTo( vecOrigin ) <=
		FOF_RESPAWN_INFLUENCE_UPDATE_DISTANCE )
	{
		return;
	}

	pState->m_vecLastInfluenceOrigin = vecOrigin;
	FoFRebuildPlayerRespawnInfluences(
		pPlayer, pPlayer->WorldSpaceCenter(),
		FOF_RESPAWN_INFLUENCE_RADIUS );
}

void FoFSetPlayerRespawnThreat(
	CFoF_Player *pPlayer, CBaseEntity *pThreat )
{
	FoFPlayerRespawnState_t *pState =
		FoFGetPlayerRespawnState( pPlayer );
	if ( pState )
		pState->m_hThreat = pThreat;
}

CBaseEntity *FoFSelectRespawnPoint( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !FoFHasRespawnProbeMap() ||
		g_flFoFNextRespawnSelectionTime > gpGlobals->curtime )
		return NULL;

	FoFPlayerRespawnState_t *pState =
		FoFGetPlayerRespawnState( pPlayer );
	if ( !pState )
		return NULL;
	pState->m_Influences.RemoveAll();

	const bool bTeamplay =
		HL2MPRules() && HL2MPRules()->IsTeamplay();
	CUtlVector< FoFRespawnCandidate_t > candidates;
	for ( int i = 0; i < g_FoFSpawnProbeRecords.Count(); ++i )
	{
		FoFRespawnCandidate_t candidate;
		if ( FoFBuildRespawnCandidate(
			pPlayer, pState, i, bTeamplay, &candidate ) )
			candidates.AddToTail( candidate );
	}

	if ( candidates.Count() == 0 )
		return NULL;
	candidates.Sort( FoFSortRespawnCandidates );

	FoFRespawnCandidate_t *pChosen = NULL;
	for ( int i = 0; i < candidates.Count(); ++i )
	{
		FoFRespawnCandidate_t &candidate = candidates[i];
		FoFSpawnProbeRecord_t &record =
			g_FoFSpawnProbeRecords[candidate.m_nRecord];
		if ( !record.m_hSpawn.Get() || FoFSpawnHasBlockingPlayer(
			pPlayer, record.m_vecOrigin ) )
		{
			continue;
		}
		pChosen = &candidate;
		break;
	}
	if ( !pChosen )
		return NULL;

	FoFSpawnProbeRecord_t &best =
		g_FoFSpawnProbeRecords[pChosen->m_nRecord];
	pState->m_flDangerSum += pChosen->m_flDanger;
	pState->m_flNearCratesSum += pChosen->m_flNearCrates;
	pState->m_flHotnessSum += pChosen->m_flHotness;
	pState->m_RecentSpawnEntityIndices.AddToHead(
		pChosen->m_nSpawnEntityIndex );
	++pState->m_nSamples;

	g_FoFGlobalRespawnHistory.m_flDangerSum += pChosen->m_flDanger;
	g_FoFGlobalRespawnHistory.m_flNearCratesSum +=
		pChosen->m_flNearCrates;
	g_FoFGlobalRespawnHistory.m_flHotnessSum += pChosen->m_flHotness;
	++g_FoFGlobalRespawnHistory.m_nSamples;

	FoFRebuildPlayerRespawnInfluences(
		pPlayer, best.m_vecOrigin + Vector( 0.0f, 0.0f, 64.0f ),
		FOF_RESPAWN_SELECTED_INFLUENCE_RADIUS );
	FoFRefreshSpawnInfluenceRecord( pChosen->m_nRecord );
	g_flFoFNextRespawnSelectionTime = gpGlobals->curtime + 0.1f;

	if ( FoFRespawnDebugEnabled() )
	{
		Msg( "--- Respawn chosen for %s: %f score %f danger, "
			"%f near crates, %f hot\n",
			pPlayer->GetPlayerName(), pChosen->m_flScore,
			pChosen->m_flDanger, pChosen->m_flNearCrates,
			pChosen->m_flHotness );
	}
	g_vecFoFRespawnDebugCoordinate = best.m_vecOrigin;
	g_bFoFRespawnDebugCoordinateValid = true;
	return best.m_hSpawn.Get();
}

class CFoFRespawnProbeSystem : public CAutoGameSystem
{
public:
	CFoFRespawnProbeSystem() :
		CAutoGameSystem( "CFoFRespawnProbeSystem" )
	{
	}

	virtual void LevelInitPostEntity( void )
	{
		FoFClearAllRespawnProbeState();
		engine->ServerCommand( "exec probes.cfg\n" );
		engine->ServerExecute();
	}

	virtual void LevelShutdownPostEntity( void )
	{
		FoFClearAllRespawnProbeState();
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		if ( FoFHasRespawnProbeMap() )
			FoFRefreshSpawnInfluenceRecord( -1 );
	}
};

static CFoFRespawnProbeSystem g_FoFRespawnProbeSystem;
