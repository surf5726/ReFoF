//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF course script controller.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_course_mode.h"

#include "eventqueue.h"
#include "filesystem.h"
#include "fof/fof_bot.h"
#include "fof/fof_player.h"
#include "fof/fof_player_equipment.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_properties.h"
#include "hl2mp_gamerules.h"
#include "KeyValues.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "props.h"
#include "recipientfilter.h"
#include "tier1/utlstring.h"
#include "util.h"
#include "vphysics/constraints.h"

#include "tier0/memdbgon.h"

// The shipped server owns course execution here: course files describe a
// timed instruction stream plus named branches, while AI-editor files hold
// the authoritative bot and prop presets used by wave/object instructions.

LINK_ENTITY_TO_CLASS( fof_coursemode, CCourseMode );

BEGIN_DATADESC( CCourseMode )
END_DATADESC()

static CHandle< CCourseMode > s_hFoFCourseMode;

static int FoFCompareCourseFilenames(
	const CUtlString *pLeft, const CUtlString *pRight )
{
	return Q_stricmp( pLeft->String(), pRight->String() );
}

static bool FoFParseCourseFilename(
	const char *pszFilename, char *pszCategory, int nCategorySize,
	char *pszMap, int nMapSize )
{
	char parts[MAX_PATH];
	Q_StripExtension( pszFilename, parts, sizeof( parts ) );
	char *pAuthor = Q_strrchr( parts, '-' );
	if ( !pAuthor )
		return false;
	*pAuthor = '\0';
	char *pMap = Q_strrchr( parts, '-' );
	if ( !pMap )
		return false;
	*pMap++ = '\0';
	char *pCategory = Q_strrchr( parts, '-' );
	if ( !pCategory )
		return false;
	++pCategory;
	Q_strncpy( pszCategory, pCategory, nCategorySize );
	Q_strncpy( pszMap, pMap, nMapSize );
	return pszCategory[0] && pszMap[0];
}

static bool FoFFindNextCourse(
	const char *pszCurrent, char *pszNextScript, int nScriptSize,
	char *pszNextMap, int nMapSize )
{
	char szCurrentCategory[32], szCurrentMap[MAX_PATH];
	if ( !FoFParseCourseFilename(
		pszCurrent, szCurrentCategory, sizeof( szCurrentCategory ),
		szCurrentMap, sizeof( szCurrentMap ) ) )
	{
		return false;
	}

	CUtlVector< CUtlString > names;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pFilename = filesystem->FindFirstEx(
		"fof_scripts/courses/*.txt", "GAME", &findHandle );
	while ( pFilename )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
			names.AddToTail( CUtlString( pFilename ) );
		pFilename = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );
	names.Sort( FoFCompareCourseFilenames );

	int nCurrent = -1;
	for ( int i = 0; i < names.Count(); ++i )
	{
		if ( !Q_stricmp( names[i].String(), pszCurrent ) )
		{
			nCurrent = i;
			break;
		}
	}
	if ( nCurrent < 0 )
		return false;

	for ( int offset = 1; offset < names.Count(); ++offset )
	{
		const int index = ( nCurrent + offset ) % names.Count();
		char szCategory[32], szMap[MAX_PATH];
		if ( !FoFParseCourseFilename(
			names[index].String(), szCategory, sizeof( szCategory ),
			szMap, sizeof( szMap ) ) ||
			Q_stricmp( szCategory, szCurrentCategory ) )
		{
			continue;
		}
		Q_strncpy( pszNextScript, names[index].String(), nScriptSize );
		Q_strncpy( pszNextMap, szMap, nMapSize );
		return true;
	}
	return false;
}

static const char *const s_FoFCourseInstructionNames[] =
{
	"player_spawn",
	"wave_spawn",
	"wave_spawn_dyn",
	"object_spawn",
	"message",
	"goto",
	"stop",
	"check_enemies",
	"check_location",
	"check_capture",
	"check_timer",
	"check_train",
	"check_wait",
	"game_end",
	"entity_io",
	"check_sp",
	"disable_check",
	"give_equip",
	"check_bot_orders",
	"play_audio",
	"challenge_end",
};

struct FoFCourseStatDefinition_t
{
	int m_nId;
	const char *m_pszToken;
};

// Script check_sp values refer to the original server table's
// these stable IDs; reported gameplay events use the matching string token.
static const FoFCourseStatDefinition_t s_FoFCourseStats[] =
{
	{  1, "drink_whiskey" },
	{  2, "open_chest" },
	{  3, "grab_phys" },
	{ 30, "drop_phys" },
	{  4, "reach_loc" },
	{  5, "pick_wep" },
	{ 24, "draw_wep" },
	{ 26, "reload_wep" },
	{ 25, "draw_fists" },
	{ 22, "drop_wep" },
	{ 15, "break_phys" },
	{ 27, "break_bottle" },
	{ 28, "break_sil" },
	{ 10, "say_yeah" },
	{ 11, "say_no" },
	{ 29, "switch_side" },
	{ 23, "shot_fired" },
	{  9, "kill_any" },
	{  6, "kill_fan" },
	{  7, "kill_kick" },
	{ 12, "kill_fists" },
	{  8, "kill_fall" },
	{ 13, "kill_head" },
	{ 14, "kill_throwngun" },
	{ 16, "kill_rifle" },
	{ 17, "kill_bow" },
	{ 18, "kill_revolver" },
	{ 19, "kill_shotgun" },
	{ 20, "kill_dynamite" },
	{ 21, "kill_blunt" },
	{ 31, "kill_throwprop" },
	{ 32, "kill_blast" },
	{ 33, "break_phys_kick" },
	{ 34, "break_phys_kick+j" },
	{ 35, "break_phys_kick+wj" },
};

static int FoFCourseStatIdFromName( const char *pszStat )
{
	if ( !pszStat || !pszStat[0] )
		return -1;

	for ( int i = 0; i < ARRAYSIZE( s_FoFCourseStats ); ++i )
	{
		if ( !Q_stricmp( pszStat, s_FoFCourseStats[i].m_pszToken ) )
			return s_FoFCourseStats[i].m_nId;
	}

	return -1;
}

static int FoFCourseInstructionType( const char *pszType )
{
	if ( !pszType || !pszType[0] )
		return -1;

	for ( int i = 0; i < ARRAYSIZE( s_FoFCourseInstructionNames ); ++i )
	{
		if ( !Q_stricmp( pszType, s_FoFCourseInstructionNames[i] ) )
			return i;
	}
	return -1;
}

static bool FoFCourseIsNumericEntry( const char *pszEntry, float &flValue )
{
	if ( !pszEntry || !pszEntry[0] )
		return false;

	char *pEnd = NULL;
	const double value = strtod( pszEntry, &pEnd );
	if ( pEnd == pszEntry || !pEnd || *pEnd != '\0' )
		return false;

	flValue = static_cast< float >( value );
	return true;
}

static Vector FoFCourseReadVector( KeyValues *pValues,
	const char *pszX, const char *pszY, const char *pszZ )
{
	return Vector(
		pValues->GetFloat( pszX, 0.0f ),
		pValues->GetFloat( pszY, 0.0f ),
		pValues->GetFloat( pszZ, 0.0f ) );
}

static bool FoFCourseLoadKeyValues( KeyValues *pRoot,
	const char *pszDirectory, const char *pszFilename )
{
	if ( !pRoot || !pszDirectory || !pszFilename || !pszFilename[0] )
		return false;

	char szPath[MAX_PATH];
	Q_snprintf( szPath, sizeof( szPath ), "%s/%s", pszDirectory, pszFilename );
	return pRoot->LoadFromFile( filesystem, szPath, "MOD" ) ||
		pRoot->LoadFromFile( filesystem, szPath, "GAME" );
}

static int FoFCountCourseWaveEnemies( const char *pszFilename )
{
	if ( !pszFilename || !pszFilename[0] )
		return 0;

	KeyValues *pWave = new KeyValues( "BotList" );
	if ( !FoFCourseLoadKeyValues(
		pWave, "fof_scripts/ai_editor", pszFilename ) )
	{
		pWave->deleteThis();
		return 0;
	}

	int nEnemies = 0;
	for ( KeyValues *pPreset = pWave->GetFirstTrueSubKey();
		pPreset; pPreset = pPreset->GetNextTrueSubKey() )
	{
		if ( !Q_stricmp( pPreset->GetName(), "preset" ) )
			++nEnemies;
	}
	pWave->deleteThis();
	return nEnemies;
}

bool FoFLoadEliminationSafeZoneCenters( CUtlVector< Vector > &centers )
{
	centers.Purge();
	if ( !gpGlobals || gpGlobals->mapname == NULL_STRING )
		return false;

	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "%s.txt",
		STRING( gpGlobals->mapname ) );

	KeyValues *pCourse = new KeyValues( "Course" );
	if ( !FoFCourseLoadKeyValues(
		pCourse, "fof_scripts/elimination", szFilename ) )
	{
		pCourse->deleteThis();
		return false;
	}

	for ( KeyValues *pNode = pCourse->GetFirstTrueSubKey();
		pNode; pNode = pNode->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( pNode->GetString( "type", "" ), "check_capture" ) )
			continue;

		centers.AddToTail( FoFCourseReadVector(
			pNode, "origin_x", "origin_y", "origin_z" ) );
	}

	pCourse->deleteThis();
	return centers.Count() > 0;
}

static void FoFCourseTrimToken( char *pszToken )
{
	if ( !pszToken )
		return;

	char *pStart = pszToken;
	while ( *pStart == ' ' || *pStart == '\t' ||
		*pStart == '\r' || *pStart == '\n' )
	{
		++pStart;
	}
	if ( pStart != pszToken )
		memmove( pszToken, pStart, Q_strlen( pStart ) + 1 );

	int nLength = Q_strlen( pszToken );
	while ( nLength > 0 )
	{
		const char last = pszToken[nLength - 1];
		if ( last != ' ' && last != '\t' && last != '\r' && last != '\n' )
			break;
		pszToken[--nLength] = '\0';
	}
}

static bool FoFCourseNextToken( const char *&pCursor, char separator,
	char *pszToken, int nTokenSize )
{
	if ( !pCursor || !*pCursor || !pszToken || nTokenSize <= 0 )
		return false;

	int nWritten = 0;
	while ( *pCursor && *pCursor != separator )
	{
		if ( nWritten + 1 < nTokenSize )
			pszToken[nWritten++] = *pCursor;
		++pCursor;
	}
	pszToken[nWritten] = '\0';
	if ( *pCursor == separator )
		++pCursor;
	FoFCourseTrimToken( pszToken );
	return true;
}

CCourseMode::CCourseMode()
	: m_flNextLoadAttempt( 0.0f )
	, m_flNextUpdate( 0.0f )
	, m_flStartTime( 0.0f )
	, m_flNextScheduledAt( 0.0f )
	, m_flEndMenuAt( 0.0f )
	, m_nNextScheduledInstruction( 0 )
	, m_nPlayerTeam( 2 )
	, m_nMaxPlayers( 1 )
	, m_nBotAlliance( 1 )
	, m_nTotalEnemies( 0 )
	, m_nEnemiesKilled( 0 )
	, m_nAward( 0 )
	, m_nDispatchBudget( 0 )
	, m_bLoaded( false )
	, m_bStarted( false )
	, m_bEnded( false )
	, m_bScheduleStopped( false )
	, m_bEndMenuShown( false )
	, m_bPlayerSpawnExecuted( false )
	, m_flCourseVoteEnd( 0.0f )
	, m_bCourseSelectionPending( false )
	, m_bCourseVoteOpen( false )
	, m_nSelectedCourse( -1 )
	, m_nPendingEndSelection( 0 )
	, m_flEndVoteDeadline( 0.0f )
	, m_bPrivateListenSession( false )
{
	m_szLoadedScript[0] = '\0';
	m_szConfiguredScript[0] = '\0';
	m_hPlayerSpawn = NULL;
	Q_memset( m_nEndVotes, 0, sizeof( m_nEndVotes ) );
	m_szPrivatePassword[0] = '\0';
}

void CCourseMode::Spawn()
{
	BaseClass::Spawn();
	ConVarRef hostname( "hostname", true );
	ConVarRef password( "sv_password", true );
	m_bPrivateListenSession = !engine->IsDedicatedServer() &&
		hostname.IsValid() && !Q_stricmp( hostname.GetString(), "local sp course" );
	if ( m_bPrivateListenSession && password.IsValid() )
	{
		Q_strncpy( m_szPrivatePassword, password.GetString(),
			sizeof( m_szPrivatePassword ) );
	}
	s_hFoFCourseMode = this;
	m_flNextLoadAttempt = gpGlobals->curtime;
}

static bool FoFCourseDynamicSpawnIsClear(
	const Vector &origin, float searchRadius, bool bCheckViewCone,
	int nPlayerTeam, bool bBotAlliance )
{
	trace_t trace;
	UTIL_TraceHull( origin, origin, VEC_HULL_MIN, VEC_HULL_MAX,
		MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
	if ( trace.startsolid || trace.allsolid )
		return false;

	const float minPlayerDistance = searchRadius * ( 1.0f / 3.0f );
	const float minPlayerDistanceSqr =
		minPlayerDistance * minPlayerDistance;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsAlive() )
			continue;
		if ( bBotAlliance && pPlayer->GetTeamNumber() != nPlayerTeam )
			continue;
		if ( bCheckViewCone && !pPlayer->FInViewCone( origin ) )
			continue;
		if ( pPlayer->GetAbsOrigin().DistToSqr( origin ) <
			minPlayerDistanceSqr )
		{
			return false;
		}

		if ( pPlayer->FVisible( origin ) )
			return false;
	}

	CBaseEntity *pEntities[16];
	const Vector mins = origin + Vector( -32.0f, -32.0f, -4.0f );
	const Vector maxs = origin + Vector( 32.0f, 32.0f, 76.0f );
	return UTIL_EntitiesInBox(
		pEntities, ARRAYSIZE( pEntities ), mins, maxs, FL_CLIENT ) == 0;
}

static bool FoFFindCourseDynamicSpawn(
	const Vector &center, float radius, Vector &origin,
	int nPlayerTeam, bool bBotAlliance )
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() ||
		TheNavAreas.Count() <= 0 || radius <= 0.0f )
	{
		return false;
	}

	CNavArea *pStartArea = TheNavMesh->GetNearestNavArea(
		center, false, 10000.0f, false, true, TEAM_ANY );
	if ( !pStartArea )
		return false;

	const float radiusSqr = radius * radius;
	CUtlVector< CNavArea * > candidates;
	CollectSurroundingAreas( &candidates, pStartArea, radius );
	for ( int i = candidates.Count() - 1; i >= 0; --i )
	{
		CNavArea *pArea = candidates[i];
		if ( !pArea || pArea->IsBlocked( TEAM_ANY ) ||
			pArea->IsDamaging() || pArea->HasAvoidanceObstacle() )
		{
			candidates.FastRemove( i );
			continue;
		}
		if ( pArea->GetCenter().DistToSqr( center ) > radiusSqr )
			candidates.FastRemove( i );
	}
	if ( candidates.Count() <= 0 )
		return false;

	for ( int attempt = 0; attempt < 100; ++attempt )
	{
		CNavArea *pArea = candidates[
			random->RandomInt( 0, candidates.Count() - 1 )];
		Vector candidate = pArea->GetRandomPoint();
		if ( candidate.DistToSqr( center ) > radiusSqr )
			continue;
		candidate.z += 10.0f;
		if ( !FoFCourseDynamicSpawnIsClear(
			candidate, radius, true, nPlayerTeam, bBotAlliance ) )
			continue;

		origin = candidate;
		return true;
	}

	const float flStartYaw = static_cast< float >(
		random->RandomInt( -180, 180 ) );
	for ( float flYaw = flStartYaw;
		flYaw < flStartYaw + 360.0f; flYaw += 25.0f )
	{
		Vector vecDirection;
		AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &vecDirection );
		Vector candidate = center + vecDirection * radius;
		trace_t groundTrace;
		UTIL_TraceLine( candidate + Vector( 0.0f, 0.0f, 256.0f ),
			candidate - Vector( 0.0f, 0.0f, 512.0f ),
			MASK_PLAYERSOLID_BRUSHONLY, NULL,
			COLLISION_GROUP_NONE, &groundTrace );
		if ( !groundTrace.DidHit() )
			continue;
		candidate = groundTrace.endpos + Vector( 0.0f, 0.0f, 10.0f );
		CNavArea *pArea = TheNavMesh->GetNearestNavArea(
			candidate, true, 100.0f, false, false, TEAM_ANY );
		if ( !pArea || pArea->IsBlocked( TEAM_ANY ) ||
			!FoFCourseDynamicSpawnIsClear(
				candidate, radius, true,
				nPlayerTeam, bBotAlliance ) )
		{
			continue;
		}
		origin = candidate;
		return true;
	}

	DevMsg( "No dynamic spawn point found \n" );
	return false;
}

void CCourseMode::UpdateOnRemove()
{
	// The client has already reset fof_listenserver when disconnect tears down
	// entities. Remember the private launch instead, and leave a changed password alone.
	if ( m_bPrivateListenSession && !engine->IsDedicatedServer() )
	{
		ConVarRef password( "sv_password", true );
		if ( password.IsValid() &&
			!Q_strcmp( password.GetString(), m_szPrivatePassword ) )
			password.SetValue( "" );
	}

	if ( s_hFoFCourseMode.Get() == this )
		s_hFoFCourseMode = NULL;
	BaseClass::UpdateOnRemove();
}

void CCourseMode::ResetRuntimeState( bool bRemoveSpawnedEntities )
{
	SendCourseHint( "", "", 0 );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && !pPlayer->IsBot() )
			pPlayer->RemoveFlag( FL_FROZEN );
	}

	for ( int i = m_ActiveChecks.Count() - 1; i >= 0; --i )
		RemoveCheck( i, true );
	m_ActiveChecks.Purge();

	bool bQueuedBotKick = false;
	if ( bRemoveSpawnedEntities )
	{
		for ( int i = 0; i < m_SpawnedEntities.Count(); ++i )
		{
			CBaseEntity *pEntity = m_SpawnedEntities[i].Get();
			if ( !pEntity )
				continue;
			CBasePlayer *pPlayer = ToBasePlayer( pEntity );
			if ( pPlayer && pPlayer->IsBot() )
			{
				engine->ServerCommand( UTIL_VarArgs(
					"kickid %d \"Course reset\"\n", pPlayer->GetUserID() ) );
				bQueuedBotKick = true;
			}
			else
			{
				UTIL_Remove( pEntity );
			}
		}
	}
	if ( bQueuedBotKick )
		engine->ServerExecute();
	m_PendingBotSpawns.Purge();
	m_SpawnedEntities.Purge();
	for ( int i = 0; i < m_Instructions.Count(); ++i )
	{
		m_Instructions[i].m_nGotoRemaining = MAX(
			0, Q_atoi( m_Instructions[i].m_szValue ) );
	}

	m_hCoursePlayer = NULL;
	m_flStartTime = 0.0f;
	m_flNextScheduledAt = 0.0f;
	m_flEndMenuAt = 0.0f;
	m_nNextScheduledInstruction = 0;
	m_nEnemiesKilled = 0;
	m_nAward = 0;
	m_nDispatchBudget = 0;
	m_bStarted = false;
	m_bEnded = false;
	m_bScheduleStopped = false;
	m_bEndMenuShown = false;
	m_bPlayerSpawnExecuted = false;
	Q_memset( m_nEndVotes, 0, sizeof( m_nEndVotes ) );
	m_nPendingEndSelection = 0;
	m_flEndVoteDeadline = 0.0f;
}

bool FoFHasConfiguredCourseForMap()
{
	static ConVarRef courseScript( "fof_course_script", true );
	return courseScript.IsValid() && gpGlobals &&
		gpGlobals->mapname != NULL_STRING &&
		Q_stristr( courseScript.GetString(), STRING( gpGlobals->mapname ) ) != NULL;
}

bool CCourseMode::LoadConfiguredCourse()
{
	static ConVarRef courseScript( "fof_course_script", true );
	if ( !courseScript.IsValid() )
		return false;

	const char *pszScript = courseScript.GetString();
	Q_strncpy( m_szConfiguredScript, pszScript ? pszScript : "",
		sizeof( m_szConfiguredScript ) );
	CloseCourseSelection();
	m_bCourseSelectionPending = false;
	m_nSelectedCourse = -1;
	if ( !FoFHasConfiguredCourseForMap() )
	{
		FindCourseChoices();
		if ( m_CourseChoices.Count() == 0 )
		{
			Warning( "No course scripts exist for this map!\n" );
			m_flNextLoadAttempt = gpGlobals->curtime + 2.0f;
			return false;
		}
		// Prepare the initial spawn from the first candidate, but do not run
		// its instructions until the spawned players have selected a course.
		m_bCourseSelectionPending = true;
		return LoadCourseScript( m_CourseChoices[0].m_szScript );
	}
	return LoadCourseScript( pszScript );
}

bool CCourseMode::LoadCourseScript( const char *pszScript )
{
	if ( Q_strstr( pszScript, ".." ) || strchr( pszScript, '/' ) ||
		strchr( pszScript, '\\' ) || strchr( pszScript, ':' ) )
	{
		Warning( "FoF course rejected unsafe script name '%s'.\n", pszScript );
		m_flNextLoadAttempt = gpGlobals->curtime + 2.0f;
		return false;
	}

	if ( m_bLoaded && !Q_stricmp( pszScript, m_szLoadedScript ) )
		return true;

	KeyValues *pCourse = new KeyValues( "Course" );
	if ( !FoFCourseLoadKeyValues(
		pCourse, "fof_scripts/courses", pszScript ) )
	{
		pCourse->deleteThis();
		m_flNextLoadAttempt = gpGlobals->curtime + 0.5f;
		return false;
	}

	ResetRuntimeState( true );
	m_Instructions.Purge();
	m_nPlayerTeam = pCourse->GetInt( "player_team", 2 );
	m_nMaxPlayers = pCourse->GetInt( "max_players", 1 ) == 1 ? 1 : 6;
	m_nBotAlliance = pCourse->GetInt( "bot_alliance", 1 );
	m_nTotalEnemies = MAX( 0, pCourse->GetInt( "total_enemies", 0 ) );
	int nConfiguredEnemies = 0;
	bool bConfiguredPlayerSpawn = false;

	for ( KeyValues *pNode = pCourse->GetFirstTrueSubKey();
		pNode; pNode = pNode->GetNextTrueSubKey() )
	{
		const int nType = FoFCourseInstructionType(
			pNode->GetString( "type", "" ) );
		if ( nType < 0 )
		{
			Warning( "FoF course '%s': unknown instruction '%s' at '%s'.\n",
				pszScript, pNode->GetString( "type", "" ), pNode->GetName() );
			continue;
		}

		FoFCourseInstruction_t instruction;
		Q_memset( &instruction, 0, sizeof( instruction ) );
		instruction.m_nType = nType;
		Q_strncpy( instruction.m_szEntry, pNode->GetName(),
			sizeof( instruction.m_szEntry ) );
		Q_strncpy( instruction.m_szData, pNode->GetString( "data", "" ),
			sizeof( instruction.m_szData ) );
		Q_strncpy( instruction.m_szValue, pNode->GetString( "value", "" ),
			sizeof( instruction.m_szValue ) );
		Q_strncpy( instruction.m_szOutput, pNode->GetString( "output", "" ),
			sizeof( instruction.m_szOutput ) );
		instruction.m_vecOrigin = FoFCourseReadVector(
			pNode, "origin_x", "origin_y", "origin_z" );
		instruction.m_vecDirection = FoFCourseReadVector(
			pNode, "dir_x", "dir_y", "dir_z" );
		instruction.m_bNumericEntry = FoFCourseIsNumericEntry(
			instruction.m_szEntry, instruction.m_flEntryTime );
		if ( !instruction.m_bNumericEntry )
			instruction.m_flEntryTime = -1.0f;
		instruction.m_nGotoRemaining = MAX( 0,
			Q_atoi( instruction.m_szValue ) );

		if ( nType == FOF_COURSE_PLAYER_SPAWN &&
			!bConfiguredPlayerSpawn )
		{
			CBaseEntity *pSpawn = m_hPlayerSpawn.Get();
			if ( !pSpawn )
			{
				pSpawn = CreateEntityByName( "info_player_fof" );
				m_hPlayerSpawn = pSpawn;
			}
			if ( pSpawn )
			{
				QAngle spawnAngles = vec3_angle;
				if ( instruction.m_vecDirection.LengthSqr() > 0.0f )
				{
					VectorAngles(
						instruction.m_vecDirection, spawnAngles );
					spawnAngles.x = 0.0f;
					spawnAngles.z = 0.0f;
				}
				pSpawn->SetAbsOrigin( instruction.m_vecOrigin );
				pSpawn->SetAbsAngles( spawnAngles );
			}
			bConfiguredPlayerSpawn = true;
		}
		if ( nType == FOF_COURSE_WAVE_SPAWN ||
			nType == FOF_COURSE_WAVE_SPAWN_DYNAMIC )
		{
			nConfiguredEnemies += FoFCountCourseWaveEnemies(
				instruction.m_szData );
		}
		m_Instructions.AddToTail( instruction );
	}
	if ( m_nTotalEnemies <= 0 )
		m_nTotalEnemies = nConfiguredEnemies;

	pCourse->deleteThis();
	Q_strncpy( m_szLoadedScript, pszScript, sizeof( m_szLoadedScript ) );
	m_bLoaded = m_Instructions.Count() > 0;
	m_flNextLoadAttempt = gpGlobals->curtime + 0.5f;

	static ConVarRef forcedTeam( "fof_course_forced_team", true );
	if ( forcedTeam.IsValid() )
		forcedTeam.SetValue( m_nPlayerTeam );
	CleanupMapCourseEntities( true );

	return m_bLoaded;
}

void CCourseMode::FindCourseChoices()
{
	m_CourseChoices.RemoveAll();
	if ( !filesystem || !gpGlobals || gpGlobals->mapname == NULL_STRING )
		return;

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszFile = filesystem->FindFirstEx(
		"fof_scripts/courses/*.txt", "GAME", &handle );
	while ( pszFile )
	{
		if ( !filesystem->FindIsDirectory( handle ) &&
			Q_stristr( pszFile, STRING( gpGlobals->mapname ) ) &&
			( !engine->IsDedicatedServer() || Q_stristr( pszFile, "-coop-" ) ) )
		{
			FoFCourseChoice_t choice;
			Q_memset( &choice, 0, sizeof( choice ) );
			Q_strncpy( choice.m_szScript, pszFile, sizeof( choice.m_szScript ) );
			char szTitle[MAX_PATH];
			Q_strncpy( szTitle, pszFile, sizeof( szTitle ) );
			char *pszSeparator = strchr( szTitle, '-' );
			if ( pszSeparator )
				*pszSeparator = '\0';
			Q_snprintf( choice.m_szLabel, sizeof( choice.m_szLabel ),
				"%s%s", szTitle, engine->IsDedicatedServer() ? "" :
				( Q_stristr( pszFile, "-sp-" ) ? " (SP)" : " (COOP)" ) );
			// The original inserts each filesystem result at the head.
			m_CourseChoices.AddToHead( choice );
		}
		pszFile = filesystem->FindNext( handle );
	}
	if ( handle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( handle );
	// The original course vote has nine numbered counters.
	if ( m_CourseChoices.Count() > 9 )
		m_CourseChoices.RemoveMultiple( 9, m_CourseChoices.Count() - 9 );
}

void CCourseMode::BeginCourseSelection()
{
	if ( m_CourseChoices.Count() == 0 )
		FindCourseChoices();
	if ( m_CourseChoices.Count() == 0 )
		return;

	CloseCourseSelection();
	for ( int i = 0; i < m_CourseChoices.Count(); ++i )
		m_CourseChoices[i].m_nVotes = 0;
	m_bCourseSelectionPending = true;
	m_bCourseVoteOpen = true;
	m_nSelectedCourse = -1;
	m_flCourseVoteEnd = gpGlobals->curtime + 10.0f;
}

void CCourseMode::CloseCourseSelection()
{
	for ( int i = 0; i < m_CourseMenuPlayers.Count(); ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( m_CourseMenuPlayers[i].Get() );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;
		if ( pPlayer->GetFoFVotekickMenuState() == 97 )
			pPlayer->SetFoFVotekickMenuState( -1 );
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "ShowMenuFoF" );
			WRITE_STRING( "" );
			WRITE_BYTE( 0 );
			WRITE_SHORT( -999 );
		MessageEnd();
	}
	m_CourseMenuPlayers.RemoveAll();
	m_bCourseVoteOpen = false;
}

bool CCourseMode::HandleCourseSelection(
	CFoF_Player *pPlayer, int nSelection )
{
	if ( !pPlayer || pPlayer->GetFoFVotekickMenuState() != 97 )
		return false;
	if ( !m_bCourseVoteOpen || pPlayer->IsBot() ||
		pPlayer->GetTeamNumber() != m_nPlayerTeam ||
		nSelection < 1 || nSelection > m_CourseChoices.Count() )
	{
		return true;
	}

	FoFCourseChoice_t &choice = m_CourseChoices[nSelection - 1];
	++choice.m_nVotes;
	pPlayer->SetFoFVotekickMenuState( -1 );
	UTIL_ClientPrintAll( HUD_PRINTTALK,
		UTIL_VarArgs( "%s voted for %s", pPlayer->GetPlayerName(), choice.m_szLabel ) );
	return true;
}

void CCourseMode::UpdateCourseSelection()
{
	if ( !m_bCourseVoteOpen && m_nSelectedCourse < 0 )
	{
		CFoF_Player *pPlayer = FindCoursePlayer();
		if ( !pPlayer || m_CourseChoices.Count() == 0 )
			return;
		// Warmup ends with a player reset before the task vote. Do not run
		// the candidate's instructions or spawn its bots during this reset.
		RespawnCoursePlayers( true );
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "round_start", true );
		if ( pEvent )
			gameeventmanager->FireEvent( pEvent );
		CleanupMapCourseEntities( true );
		if ( m_CourseChoices.Count() == 1 )
		{
			m_bCourseSelectionPending = false;
			StartCourse( pPlayer );
			return;
		}
		BeginCourseSelection();
	}
	if ( m_CourseChoices.Count() == 0 )
		return;

	if ( m_bCourseVoteOpen )
	{
		int nPlayers = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
				pPlayer->IsHLTV() || pPlayer->IsReplay() ||
				pPlayer->GetTeamNumber() != m_nPlayerTeam )
			{
				continue;
			}
			++nPlayers;
			if ( m_CourseMenuPlayers.Find( pPlayer ) != m_CourseMenuPlayers.InvalidIndex() )
				continue;
			m_CourseMenuPlayers.AddToTail( pPlayer );
			CSingleUserRecipientFilter filter( pPlayer );
			filter.MakeReliable();
			for ( int nChoice = 0; nChoice < m_CourseChoices.Count(); ++nChoice )
			{
				UserMessageBegin( filter, "ShowMenuFoF" );
					WRITE_STRING( m_CourseChoices[nChoice].m_szLabel );
					WRITE_BYTE( nChoice + 1 < m_CourseChoices.Count() ? 1 : 0 );
					WRITE_SHORT( nChoice + 1 );
				MessageEnd();
			}
			pPlayer->SetFoFVotekickMenuState( 97 );
		}

		int nVotes = 0;
		int nWinner = 0;
		for ( int i = 0; i < m_CourseChoices.Count(); ++i )
		{
			nVotes += m_CourseChoices[i].m_nVotes;
			if ( m_CourseChoices[i].m_nVotes > m_CourseChoices[nWinner].m_nVotes )
				nWinner = i;
		}
		if ( gpGlobals->curtime < m_flCourseVoteEnd && nVotes < nPlayers )
			return;

		m_nSelectedCourse = nWinner;
		CloseCourseSelection();
	}
	// All votes can decide the winner early, but the original still waits
	// for the existing selection deadline before resetting the course.
	if ( gpGlobals->curtime <= m_flCourseVoteEnd || !FindCoursePlayer() )
		return;

	char szScript[MAX_PATH];
	Q_strncpy( szScript, m_CourseChoices[m_nSelectedCourse].m_szScript,
		sizeof( szScript ) );
	m_nSelectedCourse = -1;
	m_bCourseSelectionPending = false;
	m_bLoaded = false;
	if ( !LoadCourseScript( szScript ) )
		return;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() &&
			pPlayer->GetTeamNumber() > TEAM_SPECTATOR &&
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			pPlayer->ChangeFoFTeam( m_nPlayerTeam, true, false, true );
		}
	}
	RestartCourse( false );
}

CFoF_Player *CCourseMode::FindCoursePlayer() const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() )
			continue;
		if ( pPlayer->GetTeamNumber() == m_nPlayerTeam )
			return pPlayer;
	}
	return NULL;
}

int CCourseMode::GetCoursePlayerTeam() const
{
	CFoF_Player *pPlayer = m_hCoursePlayer.Get();
	return pPlayer ? pPlayer->GetTeamNumber() : TEAM_UNASSIGNED;
}

bool CCourseMode::IsImpactMarkerActive() const
{
	if ( !m_bStarted || m_bEnded )
		return false;

	const int nBreakSilhouette =
		FoFCourseStatIdFromName( "break_sil" );
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		if ( m_ActiveChecks[i].m_nStatId == nBreakSilhouette )
			return true;
	}
	return false;
}

bool CCourseMode::ShouldBlockRespawn( const CFoF_Player *pPlayer ) const
{
	return pPlayer && m_bStarted && m_bEnded &&
		pPlayer->GetTeamNumber() == m_nPlayerTeam;
}

bool CCourseMode::AreBotTeamsAllied(
	int nFirstTeam, int nSecondTeam ) const
{
	return m_bLoaded && m_nBotAlliance &&
		nFirstTeam > TEAM_SPECTATOR &&
		nSecondTeam > TEAM_SPECTATOR &&
		nFirstTeam != m_nPlayerTeam &&
		nSecondTeam != m_nPlayerTeam;
}

int CCourseMode::GetBotOrder( const Vector &origin,
	Vector &orderOrigin, float &orderRadius ) const
{
	orderOrigin.Init();
	orderRadius = 0.0f;

	int nBestInstruction = -1;
	int nBestCheck = -1;
	float flBestDistance = 0.0f;
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( nInstruction < 0 || nInstruction >= m_Instructions.Count() )
			continue;

		const FoFCourseInstruction_t &instruction =
			m_Instructions[nInstruction];
		if ( instruction.m_nType != FOF_COURSE_CHECK_BOT_ORDERS )
			continue;

		const float flRadius = static_cast< float >(
			m_ActiveChecks[i].m_nCheckValue );
		if ( flRadius <= 0.0f )
			continue;

		const Vector delta = origin - instruction.m_vecOrigin;
		const float flDistance = delta.Length();
		if ( flDistance > flRadius || fabsf( delta.z ) > flRadius * 0.5f ||
			flDistance <= flBestDistance )
		{
			continue;
		}

		nBestInstruction = nInstruction;
		nBestCheck = i;
		flBestDistance = flDistance;
	}

	if ( nBestInstruction < 0 )
		return -1;

	const FoFCourseInstruction_t &instruction =
		m_Instructions[nBestInstruction];
	orderOrigin = instruction.m_vecOrigin;
	orderRadius = static_cast< float >(
		m_ActiveChecks[nBestCheck].m_nCheckValue );
	return Q_atoi( instruction.m_szData );
}

bool CCourseMode::ShouldCourseCompanionsFollowPlayer() const
{
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( nInstruction < 0 || nInstruction >= m_Instructions.Count() )
			continue;
		const int nType = m_Instructions[nInstruction].m_nType;
		if ( nType == FOF_COURSE_CHECK_LOCATION ||
			nType == FOF_COURSE_CHECK_TRAIN ||
			nType == FOF_COURSE_CHECK_BOT_ORDERS )
		{
			return true;
		}
	}
	return false;
}

bool CCourseMode::HasActiveTrainCheck() const
{
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( nInstruction >= 0 && nInstruction < m_Instructions.Count() &&
			m_Instructions[nInstruction].m_nType == FOF_COURSE_CHECK_TRAIN )
		{
			return true;
		}
	}
	return false;
}

bool CCourseMode::GetActiveCaptureZone(
	Vector &origin, float &radius ) const
{
	origin.Init();
	radius = 0.0f;
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( nInstruction < 0 || nInstruction >= m_Instructions.Count() ||
			m_Instructions[nInstruction].m_nType != FOF_COURSE_CHECK_CAPTURE )
		{
			continue;
		}
		origin = m_Instructions[nInstruction].m_vecOrigin;
		radius = 256.0f;
		return true;
	}
	return false;
}

void CCourseMode::CleanupMapCourseEntities( bool bRemoveWhiskey )
{
	static const char *s_pszCourseRemovedClasses[] =
	{
		"fof_crate",
		"fof_crate_low",
		"fof_crate_med",
		"item_whiskey",
		"fof_cap_entity",
		"fof_teamplay",
		"fof_breakbad",
		"weapon_whiskey",
		"hl2mp_ragdoll"
	};

	const bool bStationMap = gpGlobals && gpGlobals->mapname != NULL_STRING &&
		!Q_stricmp( STRING( gpGlobals->mapname ), "tp_station" );
	for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity; )
	{
		CBaseEntity *pNext = gEntList.NextEnt( pEntity );
		const char *pszClassname = pEntity->GetClassname();
		bool bRemove = false;
		if ( pszClassname &&
			( bRemoveWhiskey || Q_stricmp( pszClassname, "item_whiskey" ) ) )
		{
			for ( int i = 0; i < ARRAYSIZE( s_pszCourseRemovedClasses ); ++i )
			{
				if ( !Q_stricmp( pszClassname, s_pszCourseRemovedClasses[i] ) )
				{
					bRemove = true;
					break;
				}
			}
		}
		if ( !bRemove && bStationMap && pszClassname &&
			!Q_stricmp( pszClassname, "func_respawnroomvisualizer" ) )
		{
			bRemove = true;
		}
		if ( bRemove )
			UTIL_Remove( pEntity );
		pEntity = pNext;
	}
	gEntList.CleanupDeleteList();
}

void CCourseMode::StartCourse( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !m_bLoaded || m_bStarted )
		return;

	m_hCoursePlayer = pPlayer;
	pPlayer->RemoveFlag( FL_FROZEN );
	m_flStartTime = gpGlobals->curtime;
	// Let UpdateSchedule apply the first instruction's own delay.  The first
	// shipped instruction is commonly zero, but that is data rather than an
	// invariant of the course format.
	m_flNextScheduledAt = 0.0f;
	m_nNextScheduledInstruction = 0;
	m_nEnemiesKilled = 0;
	m_bStarted = true;
	m_bEnded = false;
	m_bScheduleStopped = false;
	m_flEndMenuAt = 0.0f;
	m_bEndMenuShown = false;
	m_bPlayerSpawnExecuted = false;
}

void CCourseMode::UpdateCourseCompanions()
{
	if ( m_nMaxPlayers <= 1 )
		return;

	int nHumans = 0;
	int nCompanions = 0;
	CFoF_Player *pExtraCompanion = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;
		if ( FoFIsCourseCompanionBot( pPlayer ) )
		{
			++nCompanions;
			pExtraCompanion = pPlayer;
		}
		else if ( !pPlayer->IsBot() &&
			pPlayer->GetTeamNumber() == m_nPlayerTeam )
		{
			++nHumans;
			if ( !pPlayer->IsAlive() && !pPlayer->IsObserver() &&
				gpGlobals->curtime > pPlayer->m_flNextFoFAutojoinTime )
			{
				CSingleUserRecipientFilter filter( pPlayer );
				filter.MakeReliable();
				UserMessageBegin( filter, "BBNotices" );
					WRITE_BYTE( 1 );
					WRITE_STRING( "#FoF_Course_AwaitForRespawn" );
				MessageEnd();
				pPlayer->m_flNextFoFAutojoinTime =
					gpGlobals->curtime + 60.0f;
			}
		}
	}

	const int nDesired = nHumans > 0 ?
		MAX( 0, m_nMaxPlayers - nHumans ) : 0;
	if ( nCompanions > nDesired && pExtraCompanion )
	{
		engine->ServerCommand( UTIL_VarArgs(
			"kickid %d \"Co-op slot claimed\"\n",
			pExtraCompanion->GetUserID() ) );
		engine->ServerExecute();
		return;
	}
	// Missing companions are replenished only by player_spawn instructions.
	// Filling these slots every update bypasses the checkpoint/wave schedule.
}

void CCourseMode::RestartCourse( bool bResetScores )
{
	ResetRuntimeState( true );
	// Recreate the map-owned entities without loading the BSP again. Course
	// props may have moved or changed state during the previous round.
	if ( HL2MPRules() )
		HL2MPRules()->CleanUpMap();
	CleanupMapCourseEntities( true );
	RespawnCoursePlayers( bResetScores );
	StartCourse( FindCoursePlayer() );
}

void CCourseMode::RespawnCoursePlayers( bool bResetScores )
{
	const FoFCourseInstruction_t *pSpawnInstruction = NULL;
	for ( int i = 0; i < m_Instructions.Count(); ++i )
	{
		if ( m_Instructions[i].m_nType == FOF_COURSE_PLAYER_SPAWN )
		{
			pSpawnInstruction = &m_Instructions[i];
			break;
		}
	}

	if ( pSpawnInstruction )
	{
		CBaseEntity *pSpawn = m_hPlayerSpawn.Get();
		if ( !pSpawn )
		{
			pSpawn = CreateEntityByName( "info_player_fof" );
			m_hPlayerSpawn = pSpawn;
		}
		if ( pSpawn )
		{
			QAngle spawnAngles = vec3_angle;
			if ( pSpawnInstruction->m_vecDirection.LengthSqr() > 0.0f )
			{
				VectorAngles(
					pSpawnInstruction->m_vecDirection, spawnAngles );
				spawnAngles.x = 0.0f;
				spawnAngles.z = 0.0f;
			}
			pSpawn->SetAbsOrigin( pSpawnInstruction->m_vecOrigin );
			pSpawn->SetAbsAngles( spawnAngles );
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			continue;
		}

		pPlayer->SetFoFVotekickMenuState( -1 );
		pPlayer->RemoveFlag( FL_FROZEN );
		pPlayer->StopSound( "Course.End_Music" );
		pPlayer->RemoveAllItems( true );
		// Clear warmup scores, and retain the custom clean-score replay.
		if ( bResetScores )
			pPlayer->ResetScores();
		if ( pSpawnInstruction )
		{
			QAngle angles = vec3_angle;
			if ( pSpawnInstruction->m_vecDirection.LengthSqr() > 0.0f )
			{
				VectorAngles( pSpawnInstruction->m_vecDirection, angles );
				angles.x = 0.0f;
				angles.z = 0.0f;
			}
			pPlayer->FinalizeFoFSpawnAt(
				pSpawnInstruction->m_vecOrigin, angles );
		}
		else
		{
			pPlayer->FinalizeFoFSpawn( true );
		}
	}
}

void CCourseMode::EndCourse( bool bSuccess )
{
	if ( m_bEnded )
		return;

	m_bEnded = true;
	m_bScheduleStopped = true;
	m_flEndMenuAt = gpGlobals->curtime + 3.0f;
	m_bEndMenuShown = false;
	SendCourseHint( "", "", 0 );

	if ( bSuccess && !engine->IsDedicatedServer() &&
		Q_stristr( m_szLoadedScript, "mobile_cannon" ) )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() )
				continue;

			IGameEvent *pEvent = gameeventmanager ?
				gameeventmanager->CreateEvent(
					"course_mobile_completed" ) : NULL;
			if ( pEvent )
			{
				pEvent->SetInt( "entindex_player",
					engine->GetPlayerUserId( pPlayer->edict() ) );
				gameeventmanager->FireEvent( pEvent );
			}
			break;
		}
	}

	// The shipped listen server owns the local course-history file.  Dedicated
	// servers cannot update a remote client's MOD filesystem.
	if ( !engine->IsDedicatedServer() && filesystem && m_szLoadedScript[0] )
	{
		KeyValues *pStats = new KeyValues( "CourseStats" );
		pStats->LoadFromFile(
			filesystem, "fof_scripts/course_stats.txt", "MOD" );
		KeyValues *pCourse = pStats->FindKey( m_szLoadedScript, true );
		pCourse->SetInt( "timesPlayed",
			MAX( pCourse->GetInt( "timesPlayed", 0 ), 0 ) + 1 );
		if ( bSuccess )
		{
			pCourse->SetInt( "timesCompleted",
				MAX( pCourse->GetInt( "timesCompleted", 0 ), 0 ) + 1 );
		}
		if ( m_nAward > 0 )
		{
			const int nPreviousAward = clamp(
				pCourse->GetInt( "topAward", 0 ), 0, 3 );
			if ( nPreviousAward == 0 || m_nAward < nPreviousAward )
				pCourse->SetInt( "topAward", m_nAward );
		}
		filesystem->CreateDirHierarchy( "fof_scripts", "MOD" );
		pStats->SaveToFile(
			filesystem, "fof_scripts/course_stats.txt", "MOD" );
		pStats->deleteThis();
	}

	PrecacheScriptSound( "Course.End_Music" );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->IsObserver() )
		{
			continue;
		}
		pPlayer->AddFlag( FL_FROZEN );
		pPlayer->SetAbsVelocity( vec3_origin );
		if ( !pPlayer->IsBot() )
			pPlayer->EmitSound( "Course.End_Music" );
	}
	SendNotice( bSuccess ? "#Course_End_Win" : "#Course_End_Fail" );
	if ( !bSuccess )
	{
		const float flDenominator = static_cast< float >(
			clamp( m_nTotalEnemies, 1, 1000 ) );
		const int nCompletion = clamp( static_cast< int >(
			static_cast< float >( m_nEnemiesKilled ) * 100.0f /
			flDenominator ), 0, 99 );
		char szCompletion[16];
		Q_snprintf( szCompletion, sizeof( szCompletion ),
			"%d", nCompletion );
		SendNotice( "#Course_End_Completion", szCompletion, 0 );
	}

	for ( int i = m_ActiveChecks.Count() - 1; i >= 0; --i )
		RemoveCheck( i, true );
	m_ActiveChecks.Purge();

}

void CCourseMode::UpdateEndState()
{
	if ( m_bEndMenuShown )
	{
		UpdateEndVote();
		return;
	}
	if ( gpGlobals->curtime < m_flEndMenuAt )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			continue;
		}

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "ShowMenuFoF" );
			WRITE_STRING( "#Course_Vote_Replay" );
			WRITE_BYTE( 1 );
			WRITE_SHORT( 1 );
		MessageEnd();

		const bool bSinglePlayerCourse =
			m_nMaxPlayers <= 1 || gpGlobals->maxClients <= 1;
		if ( !bSinglePlayerCourse && !engine->IsDedicatedServer() )
		{
			UserMessageBegin( filter, "ShowMenuFoF" );
				WRITE_STRING( "#Course_Vote_New" );
				WRITE_BYTE( 1 );
				WRITE_SHORT( 2 );
			MessageEnd();
		}

		UserMessageBegin( filter, "ShowMenuFoF" );
			WRITE_STRING( bSinglePlayerCourse || engine->IsDedicatedServer() ?
				"#Course_Vote_Quit" : "#Course_Vote_Next" );
			WRITE_BYTE( 0 );
			WRITE_SHORT( 3 );
		MessageEnd();
		pPlayer->SetFoFVotekickMenuState( 98 );
	}
	m_bEndMenuShown = true;
	Q_memset( m_nEndVotes, 0, sizeof( m_nEndVotes ) );
	m_nPendingEndSelection = 0;
	m_flEndVoteDeadline = gpGlobals->curtime + 10.0f;
}

bool CCourseMode::HandleEndMenuSelection(
	CFoF_Player *pPlayer, int nSelection )
{
	if ( !pPlayer || !m_bEnded || !m_bEndMenuShown ||
		pPlayer->GetFoFVotekickMenuState() != 98 )
	{
		return false;
	}
	if ( pPlayer->IsBot() || pPlayer->GetTeamNumber() != m_nPlayerTeam ||
		nSelection < 1 || nSelection > 3 || m_nPendingEndSelection != 0 )
	{
		return true;
	}
	if ( m_nMaxPlayers <= 1 || gpGlobals->maxClients <= 1 )
	{
		pPlayer->SetFoFVotekickMenuState( -1 );
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "ShowMenuFoF" );
			WRITE_STRING( "" );
			WRITE_BYTE( 0 );
			WRITE_SHORT( -999 );
		MessageEnd();
		if ( nSelection == 1 )
			RestartCourse();
		else
			engine->ClientCommand( pPlayer->edict(), "disconnect\n" );
		return true;
	}

	++m_nEndVotes[nSelection - 1];
	pPlayer->SetFoFVotekickMenuState( -1 );
	static const char *s_pszVoteNotices[] =
	{
		"#FoF_VoteReplayCourse", "#FoF_VoteChangeCourse", "#FoF_VoteNextCourse",
	};
	UTIL_ClientPrintAll( HUD_PRINTTALK, s_pszVoteNotices[nSelection - 1],
		pPlayer->GetPlayerName() );
	return true;
}

void CCourseMode::CloseEndMenu()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			continue;
		}
		pPlayer->SetFoFVotekickMenuState( -1 );
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "ShowMenuFoF" );
			WRITE_STRING( "" );
			WRITE_BYTE( 0 );
			WRITE_SHORT( -999 );
		MessageEnd();
	}
}

void CCourseMode::UpdateEndVote()
{
	if ( m_nPendingEndSelection != 0 )
	{
		if ( gpGlobals->curtime > m_flEndVoteDeadline )
			ExecuteEndMenuSelection( m_nPendingEndSelection );
		return;
	}

	int nPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() &&
			!pPlayer->IsHLTV() && !pPlayer->IsReplay() &&
			pPlayer->GetTeamNumber() == m_nPlayerTeam )
		{
			++nPlayers;
		}
	}
	const int nVotes = m_nEndVotes[0] + m_nEndVotes[1] + m_nEndVotes[2];
	if ( nPlayers == 0 ||
		( gpGlobals->curtime <= m_flEndVoteDeadline && nVotes < nPlayers ) )
	{
		return;
	}

	CloseEndMenu();
	if ( m_nEndVotes[0] > 0 && m_nEndVotes[0] > m_nEndVotes[1] &&
		m_nEndVotes[0] > m_nEndVotes[2] )
	{
		m_nPendingEndSelection = 1;
		m_flEndVoteDeadline = gpGlobals->curtime + 0.5f;
	}
	else if ( m_nEndVotes[1] > 0 && m_nEndVotes[1] > m_nEndVotes[0] &&
		m_nEndVotes[1] > m_nEndVotes[2] )
	{
		ExecuteEndMenuSelection( 2 );
	}
	else
	{
		m_nPendingEndSelection = 3;
		m_flEndVoteDeadline = gpGlobals->curtime + 1.0f;
	}
}

void CCourseMode::ExecuteEndMenuSelection( int nSelection )
{
	CFoF_Player *pPlayer = FindCoursePlayer();
	if ( !pPlayer )
		return;
	m_nPendingEndSelection = 0;

	if ( nSelection == 1 )
	{
		RestartCourse();
		return;
	}
	if ( nSelection == 2 )
	{
		if ( m_nMaxPlayers > 1 && gpGlobals->maxClients > 1 )
		{
			FindCourseChoices();
			BeginCourseSelection();
		}
		else
		{
			engine->ClientCommand( pPlayer->edict(), "disconnect\n" );
		}
		return;
	}
	if ( nSelection == 3 )
	{
		char szNextScript[MAX_PATH], szNextMap[MAX_PATH];
		if ( m_nMaxPlayers > 1 && !engine->IsDedicatedServer() && FoFFindNextCourse(
			m_szLoadedScript, szNextScript, sizeof( szNextScript ),
			szNextMap, sizeof( szNextMap ) ) )
		{
			static ConVarRef courseScript( "fof_course_script", true );
			if ( courseScript.IsValid() )
				courseScript.SetValue( szNextScript );
			engine->ServerCommand( UTIL_VarArgs(
				"changelevel %s\n", szNextMap ) );
		}
		else
		{
			engine->ClientCommand( pPlayer->edict(), "disconnect\n" );
		}
		return;
	}
}

void CCourseMode::Update()
{
	if ( gpGlobals->curtime < m_flNextUpdate )
		return;
	m_flNextUpdate = gpGlobals->curtime + 0.03f;

	static ConVarRef courseScript( "fof_course_script", true );
	const char *pszConfigured = courseScript.IsValid() ?
		courseScript.GetString() : "";
	if ( m_bLoaded && Q_stricmp( pszConfigured, m_szConfiguredScript ) )
	{
		CloseCourseSelection();
		m_bCourseSelectionPending = false;
		m_nSelectedCourse = -1;
		m_bLoaded = false;
		m_szLoadedScript[0] = '\0';
		ResetRuntimeState( true );
		m_Instructions.Purge();
		m_flNextLoadAttempt = gpGlobals->curtime;
	}

	if ( !m_bLoaded )
	{
		if ( gpGlobals->curtime >= m_flNextLoadAttempt )
			LoadConfiguredCourse();
		return;
	}

	static ConVarRef warmup( "fof_warmup", true );
	if ( warmup.IsValid() && warmup.GetBool() )
		return;
	if ( m_bCourseSelectionPending )
	{
		UpdateCourseSelection();
		return;
	}

	UpdateCourseCompanions();

	if ( !m_bStarted )
	{
		CFoF_Player *pPlayer = FindCoursePlayer();
		if ( pPlayer )
			StartCourse( pPlayer );
		return;
	}

	if ( m_bEnded )
	{
		UpdateEndState();
		return;
	}

	if ( !m_hCoursePlayer.Get() )
		m_hCoursePlayer = FindCoursePlayer();

	m_nDispatchBudget = 256;
	UpdatePendingBotSpawns();
	UpdateSchedule();
	if ( m_bEnded )
		return;

	int nAliveCoursePlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() &&
			pPlayer->GetTeamNumber() == m_nPlayerTeam &&
			pPlayer->IsAlive() )
		{
			++nAliveCoursePlayers;
		}
	}
	if ( nAliveCoursePlayers <= 0 )
	{
		EndCourse( false );
		return;
	}
	UpdateChecks();
}

void CCourseMode::UpdatePendingBotSpawns()
{
	if ( m_PendingBotSpawns.Count() <= 0 )
		return;

	int nConnectedPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsConnected() )
			++nConnectedPlayers;
	}
	// The course wave queue leaves the final client slot available.
	if ( nConnectedPlayers + 1 >= gpGlobals->maxClients )
		return;

	const FoFCourseBotSpawn_t pending = m_PendingBotSpawns[0];
	FoFBotProfile_t profile;
	profile.m_nRotationSpeed = pending.m_nRotationSpeed;
	profile.m_nShootDelay = pending.m_nShootDelay;
	profile.m_nAimTrailing = pending.m_nAimTrailing;
	profile.m_nStrafe = pending.m_nStrafe;
	profile.m_nForceTeam = pending.m_nForceTeam;
	profile.m_nAggression = pending.m_nAggression;
	Q_strncpy( profile.m_szName, pending.m_szName,
		sizeof( profile.m_szName ) );
	Q_strncpy( profile.m_szEquipment, pending.m_szEquipment,
		sizeof( profile.m_szEquipment ) );

	Vector spawnOrigin = pending.m_vecOrigin;
	if ( pending.m_bDynamic && !FoFFindCourseDynamicSpawn(
		pending.m_vecDynamicCenter,
		pending.m_flDynamicRadius, spawnOrigin,
		m_nPlayerTeam, m_nBotAlliance != 0 ) )
	{
		// A failed dynamic search is transient.  The original keeps the wave
		// entry and retries it after trying the other queued presets; deleting
		// it here permanently shortened zombie waves.
		m_PendingBotSpawns.Remove( 0 );
		m_PendingBotSpawns.AddToTail( pending );
		return;
	}

	CFoFBot *pBot = FoFPutConfiguredBotInServer(
		profile, spawnOrigin, pending.m_vecDirection );
	m_PendingBotSpawns.Remove( 0 );
	if ( !pBot )
		return;

	m_SpawnedEntities.AddToTail( pBot );
}

void CCourseMode::UpdateSchedule()
{
	if ( m_bScheduleStopped || m_bEnded )
		return;

	while ( m_nDispatchBudget > 0 &&
		m_nNextScheduledInstruction < m_Instructions.Count() )
	{
		FoFCourseInstruction_t &instruction =
			m_Instructions[m_nNextScheduledInstruction];
		if ( !instruction.m_bNumericEntry )
		{
			++m_nNextScheduledInstruction;
			continue;
		}

		if ( m_flNextScheduledAt <= 0.0f )
		{
			m_flNextScheduledAt = gpGlobals->curtime +
				MAX( 0.0f, instruction.m_flEntryTime );
		}
		if ( gpGlobals->curtime < m_flNextScheduledAt )
			return;

		const int nInstruction = m_nNextScheduledInstruction++;
		if ( instruction.m_nType == FOF_COURSE_STOP )
		{
			m_bScheduleStopped = true;
			return;
		}

		ExecuteInstruction( nInstruction, -1 );
		m_flNextScheduledAt = 0.0f;
	}
}

void CCourseMode::DispatchOutput( const char *pszOutput, int nCaller )
{
	if ( !pszOutput || !pszOutput[0] || m_nDispatchBudget <= 0 )
		return;

	for ( int i = 0; i < m_Instructions.Count() && m_nDispatchBudget > 0; ++i )
	{
		const FoFCourseInstruction_t &instruction = m_Instructions[i];
		if ( i == nCaller || instruction.m_bNumericEntry ||
			Q_stricmp( instruction.m_szEntry, pszOutput ) )
		{
			continue;
		}
		ExecuteInstruction( i, nCaller );
	}
}

void CCourseMode::ExecuteInstruction( int nInstruction, int nCaller )
{
	if ( nInstruction < 0 || nInstruction >= m_Instructions.Count() ||
		m_nDispatchBudget-- <= 0 || m_bEnded )
	{
		return;
	}

	FoFCourseInstruction_t &instruction = m_Instructions[nInstruction];
	bool bDispatchOutput = true;
	switch ( instruction.m_nType )
	{
	case FOF_COURSE_PLAYER_SPAWN:
		ExecutePlayerSpawn( instruction );
		break;
	case FOF_COURSE_WAVE_SPAWN:
		SpawnWave( instruction, false );
		break;
	case FOF_COURSE_WAVE_SPAWN_DYNAMIC:
		SpawnWave( instruction, true );
		break;
	case FOF_COURSE_OBJECT_SPAWN:
		SpawnObjects( instruction );
		break;
	case FOF_COURSE_MESSAGE:
		SendNotice( instruction.m_szData );
		break;
	case FOF_COURSE_GOTO:
		ExecuteGoto( nInstruction );
		bDispatchOutput = false;
		break;
	case FOF_COURSE_STOP:
		m_bScheduleStopped = true;
		bDispatchOutput = false;
		break;
	case FOF_COURSE_CHECK_ENEMIES:
	case FOF_COURSE_CHECK_LOCATION:
	case FOF_COURSE_CHECK_CAPTURE:
	case FOF_COURSE_CHECK_TIMER:
	case FOF_COURSE_CHECK_TRAIN:
	case FOF_COURSE_CHECK_WAIT:
	case FOF_COURSE_CHECK_SINGLEPLAYER:
	case FOF_COURSE_CHECK_BOT_ORDERS:
		AddCheck( nInstruction );
		bDispatchOutput = false;
		break;
	case FOF_COURSE_GAME_END:
		EndCourse( Q_stricmp( instruction.m_szData, "failed" ) != 0 );
		bDispatchOutput = false;
		break;
	case FOF_COURSE_ENTITY_IO:
		ExecuteEntityIO( instruction );
		break;
	case FOF_COURSE_DISABLE_CHECK:
		for ( int i = m_ActiveChecks.Count() - 1; i >= 0; --i )
		{
			const int nActiveInstruction = m_ActiveChecks[i].m_nInstruction;
			if ( nActiveInstruction != nInstruction &&
				nActiveInstruction >= 0 &&
				nActiveInstruction < m_Instructions.Count() &&
				!Q_stricmp( m_Instructions[nActiveInstruction].m_szEntry,
					instruction.m_szData ) )
			{
				RemoveCheck( i, true );
			}
		}
		break;
	case FOF_COURSE_GIVE_EQUIPMENT:
		GiveEquipment( instruction );
		break;
	case FOF_COURSE_PLAY_AUDIO:
		if ( instruction.m_szData[0] )
		{
			PrecacheScriptSound( instruction.m_szData );
			CFoF_Player *pPlayer = FindCoursePlayer();
			if ( pPlayer )
				pPlayer->EmitSound( instruction.m_szData );
		}
		break;
	case FOF_COURSE_CHALLENGE_END:
		ExecuteChallengeEnd( instruction );
		bDispatchOutput = false;
		break;
	default:
		bDispatchOutput = false;
		break;
	}

	if ( bDispatchOutput )
		DispatchOutput( instruction.m_szOutput, nInstruction );
}

void CCourseMode::ExecuteGoto( int nInstruction )
{
	FoFCourseInstruction_t &instruction = m_Instructions[nInstruction];
	if ( instruction.m_nGotoRemaining > 0 )
	{
		--instruction.m_nGotoRemaining;
		if ( instruction.m_nGotoRemaining > 0 && instruction.m_szOutput[0] )
		{
			DispatchOutput( instruction.m_szOutput, nInstruction );
			return;
		}
	}

	char labels[16][32];
	int nLabels = 0;
	const char *pCursor = instruction.m_szData;
	while ( nLabels < ARRAYSIZE( labels ) &&
		FoFCourseNextToken( pCursor, ',', labels[nLabels], sizeof( labels[0] ) ) )
	{
		if ( labels[nLabels][0] )
			++nLabels;
	}

	if ( nLabels > 0 )
	{
		const char *pszTarget = labels[RandomInt( 0, nLabels - 1 )];
		for ( int i = 0; i < m_Instructions.Count(); ++i )
		{
			if ( i == nInstruction || m_Instructions[i].m_bNumericEntry ||
				Q_stricmp( m_Instructions[i].m_szEntry, pszTarget ) )
			{
				continue;
			}

			m_nNextScheduledInstruction = i + 1;
			m_flNextScheduledAt = 0.0f;
			m_bScheduleStopped = false;
			ExecuteInstruction( i, nInstruction );
			break;
		}
	}
	instruction.m_nGotoRemaining = MAX( 0, Q_atoi( instruction.m_szValue ) );
}

void CCourseMode::ExecutePlayerSpawn(
	const FoFCourseInstruction_t &instruction )
{
	QAngle angles = vec3_angle;
	if ( instruction.m_vecDirection.LengthSqr() > 0.0f )
	{
		VectorAngles( instruction.m_vecDirection, angles );
		angles.x = 0.0f;
		angles.z = 0.0f;
	}

	CBaseEntity *pSpawn = m_hPlayerSpawn.Get();
	if ( !pSpawn )
	{
		pSpawn = CreateEntityByName( "info_player_fof" );
		m_hPlayerSpawn = pSpawn;
	}
	if ( pSpawn )
	{
		pSpawn->SetAbsOrigin( instruction.m_vecOrigin );
		pSpawn->SetAbsAngles( angles );
	}

	int nSpawned = 0;
	for ( int i = 1; i <= gpGlobals->maxClients && nSpawned < m_nMaxPlayers; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			continue;
		}

		const bool bInitializePlayer = !m_bPlayerSpawnExecuted || !pPlayer->IsAlive();
		if ( pPlayer->IsAlive() && !m_bPlayerSpawnExecuted )
		{
			const Vector velocity = vec3_origin;
			pPlayer->Teleport(
				&instruction.m_vecOrigin, &angles, &velocity );
		}
		else if ( !pPlayer->IsAlive() )
		{
			pPlayer->FinalizeFoFSpawnAt(
				instruction.m_vecOrigin, angles );
		}
		if ( bInitializePlayer )
		{
			pPlayer->SetLocalAngles( angles );
			pPlayer->SnapEyeAngles( angles );
			pPlayer->pl.v_angle = angles;
			pPlayer->ViewPunchReset();
		}
		if ( !m_hCoursePlayer.Get() )
			m_hCoursePlayer = pPlayer;
		++nSpawned;
	}

	const int nDesiredCompanions = MAX( 0, m_nMaxPlayers - nSpawned );
	int nCompanions = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pCompanion = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pCompanion || !FoFIsCourseCompanionBot( pCompanion ) ||
			!pCompanion->IsAlive() )
			continue;
		// Later player_spawn instructions update the checkpoint and replenish
		// casualties. They do not teleport or turn the surviving companions.
		if ( m_bPlayerSpawnExecuted )
		{
			++nCompanions;
			continue;
		}
		const Vector velocity = vec3_origin;
		pCompanion->Teleport(
			&instruction.m_vecOrigin, &angles, &velocity );
		pCompanion->SetLocalAngles( angles );
		pCompanion->SnapEyeAngles( angles );
		pCompanion->pl.v_angle = angles;
		++nCompanions;
	}

	while ( nCompanions < nDesiredCompanions )
	{
		CFoFBot *pCompanion = FoFPutCourseCompanionInServer(
			m_nPlayerTeam, instruction.m_vecOrigin, angles,
			nDesiredCompanions - nCompanions );
		if ( !pCompanion )
			break;
		pCompanion->FinalizeFoFSpawnEquipment();
		FoFUpdateBotEquipment( pCompanion );
		m_SpawnedEntities.AddToTail( pCompanion );
		++nCompanions;
	}
	m_bPlayerSpawnExecuted = true;
}

void CCourseMode::SpawnWave( const FoFCourseInstruction_t &instruction,
	bool bDynamic )
{
	KeyValues *pWave = new KeyValues( "BotList" );
	if ( !FoFCourseLoadKeyValues(
		pWave, "fof_scripts/ai_editor", instruction.m_szData ) )
	{
		Warning( "FoF course could not load wave '%s'.\n", instruction.m_szData );
		pWave->deleteThis();
		return;
	}

	const float flDynamicRadius = MAX( 0.0f, Q_atof( instruction.m_szValue ) );
	for ( KeyValues *pPreset = pWave->GetFirstTrueSubKey();
		pPreset; pPreset = pPreset->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( pPreset->GetName(), "preset" ) )
			continue;

		const Vector origin = FoFCourseReadVector(
			pPreset, "origin_x", "origin_y", "origin_z" );

		FoFCourseBotSpawn_t pending;
		Q_memset( &pending, 0, sizeof( pending ) );
		pending.m_nRotationSpeed = clamp(
			pPreset->GetInt( "bot_rotation_speed", 5 ), 0, 10 );
		pending.m_nShootDelay = clamp(
			pPreset->GetInt( "bot_shoot_delay", 5 ), 0, 10 );
		pending.m_nAimTrailing = clamp(
			pPreset->GetInt( "bot_aim_trailing", 5 ), 0, 10 );
		pending.m_nStrafe = clamp(
			pPreset->GetInt( "bot_strafe", 5 ), 0, 10 );
		pending.m_nForceTeam = pPreset->GetInt( "bot_force_team", 0 );
		if ( pending.m_nForceTeam < FOF_TEAM_VIGILANTES ||
			pending.m_nForceTeam > FOF_TEAM_ZOMBIES )
			pending.m_nForceTeam = m_nPlayerTeam == 2 ? 3 : 2;
		pending.m_nAggression = clamp(
			pPreset->GetInt( "bot_aggression", 5 ), 0, 10 );
		Q_strncpy( pending.m_szName,
			pPreset->GetString( "bot_name", "BOT Course" ),
			sizeof( pending.m_szName ) );
		Q_strncpy( pending.m_szEquipment,
			pPreset->GetString( "bot_equipment", "" ),
			sizeof( pending.m_szEquipment ) );
		pending.m_bDynamic = bDynamic;
		pending.m_flDynamicRadius = flDynamicRadius;
		pending.m_vecDynamicCenter = instruction.m_vecOrigin;
		pending.m_vecOrigin = origin;
		pending.m_vecDirection = FoFCourseReadVector(
			pPreset, "dir_x", "dir_y", "dir_z" );
		m_PendingBotSpawns.AddToTail( pending );
	}
	pWave->deleteThis();
}

void CCourseMode::SpawnObjects( const FoFCourseInstruction_t &instruction )
{
	KeyValues *pObjects = new KeyValues( "PropList" );
	if ( !FoFCourseLoadKeyValues(
		pObjects, "fof_scripts/ai_editor", instruction.m_szData ) )
	{
		Warning( "FoF course could not load objects '%s'.\n",
			instruction.m_szData );
		pObjects->deleteThis();
		return;
	}

	for ( KeyValues *pPreset = pObjects->GetFirstTrueSubKey();
		pPreset; pPreset = pPreset->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( pPreset->GetName(), "preset" ) )
			continue;
		const char *pszClassname = pPreset->GetString( "entity", "" );
		if ( !pszClassname[0] )
			continue;

		CBaseEntity *pEntity = CreateEntityByName( pszClassname );
		if ( !pEntity )
		{
			Warning( "FoF course could not create entity '%s'.\n", pszClassname );
			continue;
		}

		const Vector origin = FoFCourseReadVector(
			pPreset, "origin_x", "origin_y", "origin_z" );
		const Vector direction = FoFCourseReadVector(
			pPreset, "dir_x", "dir_y", "dir_z" );
		QAngle angles = vec3_angle;
		if ( direction.LengthSqr() > 0.0f )
			VectorAngles( direction, angles );
		pEntity->SetAbsOrigin( origin );
		pEntity->SetAbsAngles( angles );

		const char *pszData = pPreset->GetString( "data", "" );
		if ( pszData[0] && Q_stricmp( pszData, "null" ) )
			pEntity->KeyValue( "model", pszData );

		CBaseCombatWeapon *pWeapon =
			dynamic_cast< CBaseCombatWeapon * >( pEntity );
		if ( pWeapon )
			pWeapon->AddSpawnFlags( SF_WEAPON_START_CONSTRAINED );
		else
			pEntity->AddSpawnFlags( SF_NORESPAWN | 1 );

		DispatchSpawn( pEntity );
		pEntity->Activate();
		PrecacheScriptSound( "FoF.SackRelocated" );
		pEntity->EmitSound( "FoF.SackRelocated" );
		pEntity->AddEffects(
			EF_ITEM_BLINK | FOF_COURSE_USE_PROMPT_EFFECT );
		pEntity->AddSolidFlags( FOF_COURSE_USE_PICKUP_SOLID_FLAG );
		if ( pWeapon )
		{
			IPhysicsObject *pReferenceObject = g_PhysWorldObject;
			IPhysicsObject *pAttachedObject = pWeapon->VPhysicsGetObject();
			if ( pReferenceObject && pAttachedObject )
			{
				constraint_fixedparams_t fixed;
				fixed.Defaults();
				fixed.InitWithCurrentObjectState(
					pReferenceObject, pAttachedObject );
				fixed.constraint.forceLimit = lbs2kg( 10000 );
				fixed.constraint.torqueLimit = lbs2kg( 10000 );
				physenv->CreateFixedConstraint(
					pReferenceObject, pAttachedObject, NULL, fixed );
			}
			pWeapon->SendFoFWorldGlow();
		}
		m_SpawnedEntities.AddToTail( pEntity );
	}
	pObjects->deleteThis();
}

void CCourseMode::ExecuteEntityIO(
	const FoFCourseInstruction_t &instruction )
{
	const char *pCursor = instruction.m_szData;
	char szTarget[128], szInput[128], szParameter[256], szDelay[32];
	szTarget[0] = szInput[0] = szParameter[0] = szDelay[0] = '\0';
	FoFCourseNextToken( pCursor, ',', szTarget, sizeof( szTarget ) );
	FoFCourseNextToken( pCursor, ',', szInput, sizeof( szInput ) );
	FoFCourseNextToken( pCursor, ',', szParameter, sizeof( szParameter ) );
	FoFCourseNextToken( pCursor, ',', szDelay, sizeof( szDelay ) );
	if ( !szTarget[0] || !szInput[0] )
		return;
	const float flDelay = MAX( 0.0f, Q_atof( szDelay ) );
	DevMsg( "entity IO %s: %s %f \n", szTarget, szInput, flDelay );

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, szTarget );
	if ( !pTarget )
	{
		DevMsg( "entity IO not found!\n" );
		return;
	}

	variant_t value;
	value.SetFloat( Q_atof( szParameter ) );
	g_EventQueue.AddEvent( pTarget, szInput, value,
		flDelay, NULL, NULL );
}

void CCourseMode::GiveEquipment(
	const FoFCourseInstruction_t &instruction )
{
	CFoF_Player *pPlayer = FindCoursePlayer();
	if ( !pPlayer )
		return;
	if ( Q_atoi( instruction.m_szValue ) == 2 )
		m_hCoursePlayer = pPlayer;

	SendEquipmentItem( pPlayer, -1 );
	if ( Q_atoi( instruction.m_szValue ) > 0 )
	{
		pPlayer->ApplyFoFClientPreferences();
		pPlayer->RemoveAllItems( true );
	}

	pPlayer->CBasePlayer::GiveAmmo( 100, "Buckshot", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "357", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "XBowBolt", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "Rifle", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "Rifle2", true );

	// The course publishes items in source order and applies them from tail to
	// head.  Hand placement remains the ordinary inventory pickup policy.
	FoFGiveEquipmentList( pPlayer, instruction.m_szData, true, true );
	pPlayer->EmitSound( "FoFPlayer.Equipment" );
	if ( Q_atoi( instruction.m_szValue ) > 0 )
	{
		CBaseCombatWeapon *pFists = pPlayer->Weapon_OwnsThisType( "weapon_fists" );
		if ( !pFists )
		{
			pFists = dynamic_cast< CBaseCombatWeapon * >(
				pPlayer->GiveFoFNamedItem( "weapon_fists" ) );
		}
		pPlayer->SwitchToNextBestWeapon( NULL );
	}
}

void CCourseMode::ExecuteChallengeEnd(
	const FoFCourseInstruction_t &instruction )
{
	float thresholds[3] = { 0.0f, 0.0f, 0.0f };
	const char *pCursor = instruction.m_szData;
	char szValue[32];
	for ( int i = 0; i < ARRAYSIZE( thresholds ); ++i )
	{
		if ( FoFCourseNextToken( pCursor, ',', szValue, sizeof( szValue ) ) )
			thresholds[i] = Q_atof( szValue );
	}

	float flRemaining = 0.0f;
	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( nInstruction >= 0 && nInstruction < m_Instructions.Count() &&
			m_Instructions[nInstruction].m_nType == FOF_COURSE_CHECK_TIMER )
		{
			flRemaining = m_ActiveChecks[i].m_flTriggerAt - gpGlobals->curtime;
		}
	}

	m_nAward = 0;
	if ( flRemaining > 0.0f )
	{
		const char *pszAward = "#Challenge_Slow";
		if ( flRemaining > thresholds[0] )
		{
			pszAward = "#Challenge_Gold";
			m_nAward = 1;
		}
		else if ( flRemaining > thresholds[1] )
		{
			pszAward = "#Challenge_Silver";
			m_nAward = 2;
		}
		else if ( flRemaining > thresholds[2] )
		{
			pszAward = "#Challenge_Bronze";
			m_nAward = 3;
		}
		SendNotice( pszAward );
	}
	DispatchOutput( instruction.m_szOutput, -1 );
	EndCourse( true );
}

static void FoFFireCourseCaptureZoneEvent(
	const FoFCourseInstruction_t &instruction, int nTeam, bool bEnabled )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent(
			bEnabled ? "cap_zone" : "cap_zone_off" ) : NULL;
	if ( !pEvent )
		return;

	if ( bEnabled )
	{
		pEvent->SetInt( "team", nTeam );
		pEvent->SetInt( "show_mode", 0 );
	}
	pEvent->SetFloat( "pos_x", instruction.m_vecOrigin.x );
	pEvent->SetFloat( "pos_y", instruction.m_vecOrigin.y );
	pEvent->SetFloat( "pos_z", instruction.m_vecOrigin.z );
	gameeventmanager->FireEvent( pEvent );
}

void CCourseMode::AddCheck( int nInstruction )
{
	if ( nInstruction < 0 || nInstruction >= m_Instructions.Count() )
		return;

	FoFCourseActiveCheck_t check;
	Q_memset( &check, 0, sizeof( check ) );
	check.m_nInstruction = nInstruction;
	check.m_flStartedAt = gpGlobals->curtime;
	check.m_flLastUpdate = gpGlobals->curtime;
	const FoFCourseInstruction_t &instruction = m_Instructions[nInstruction];
	check.m_nCheckValue = clamp(
		Q_atoi( instruction.m_szValue ), 64, 16000 );

	if ( instruction.m_nType == FOF_COURSE_CHECK_ENEMIES )
	{
		check.m_flTriggerAt = gpGlobals->curtime;
	}
	else if ( instruction.m_nType == FOF_COURSE_CHECK_WAIT )
	{
		float flMinimum = Q_atof( instruction.m_szData );
		float flMaximum = flMinimum;
		const char *pComma = strchr( instruction.m_szData, ',' );
		if ( pComma )
			flMaximum = Q_atof( pComma + 1 );
		if ( flMaximum < flMinimum )
			V_swap( flMinimum, flMaximum );
		check.m_flTriggerAt = gpGlobals->curtime +
			RandomFloat( MAX( 0.0f, flMinimum ), MAX( 0.0f, flMaximum ) );
	}
	else if ( instruction.m_nType == FOF_COURSE_CHECK_TIMER )
	{
		check.m_flTriggerAt = gpGlobals->curtime +
			MAX( 0.0f, Q_atof( instruction.m_szValue ) );
	}
	else if ( instruction.m_nType == FOF_COURSE_CHECK_LOCATION )
	{
		SendLocationMarker( instruction, true );
		if ( instruction.m_szData[0] &&
			Q_atoi( instruction.m_szValue ) == 0 )
		{
			const char *pszModel = "models/props/cap_circle_64.mdl";
			PrecacheModel( pszModel );
			CBaseEntity *pVisual = CreateEntityByName( "dynamic_prop" );
			if ( pVisual )
			{
				pVisual->KeyValue( "model", pszModel );
				pVisual->SetAbsOrigin( instruction.m_vecOrigin );
				QAngle angles;
				VectorAngles( instruction.m_vecDirection, angles );
				angles.x = 0.0f;
				angles.z = 0.0f;
				pVisual->SetAbsAngles( angles );
				DispatchSpawn( pVisual );
				pVisual->Activate();
				check.m_hVisual = pVisual;
			}
		}
	}
	else if ( instruction.m_nType == FOF_COURSE_CHECK_CAPTURE &&
		Q_stricmp( instruction.m_szData, "invisible" ) )
	{
		const char *pszModel = "models/props/cap_circle_512.mdl";
		PrecacheModel( pszModel );
		CBaseEntity *pVisual = CreateEntityByName( "dynamic_prop" );
		if ( pVisual )
		{
			pVisual->KeyValue( "model", pszModel );
			pVisual->SetAbsOrigin( instruction.m_vecOrigin );
			QAngle angles;
			VectorAngles( instruction.m_vecDirection, angles );
			angles.x = 0.0f;
			angles.z = 0.0f;
			pVisual->SetAbsAngles( angles );
			DispatchSpawn( pVisual );
			pVisual->Activate();
			check.m_hVisual = pVisual;
		}
		FoFFireCourseCaptureZoneEvent(
			instruction, 2, true );
	}
	else if ( instruction.m_nType == FOF_COURSE_CHECK_SINGLEPLAYER )
	{
		char szData[256];
		Q_strncpy( szData, instruction.m_szData, sizeof( szData ) );
		char *pTitle = szData;
		char *pBody = strchr( pTitle, ';' );
		char *pMode = NULL;
		if ( pBody )
		{
			*pBody++ = '\0';
			pMode = strchr( pBody, ';' );
			if ( pMode )
				*pMode++ = '\0';
		}
		if ( Q_stricmp( pTitle, "null" ) && pBody &&
			Q_stricmp( pBody, "null" ) )
		{
			SendCourseHint( pTitle, pBody, pMode ? Q_atoi( pMode ) : 0 );
			check.m_bOwnsHint = true;
		}

		check.m_nStatId = Q_atoi( instruction.m_szValue );
		check.m_nStatRemaining = MAX( 0, pMode ? Q_atoi( pMode ) : 0 );
		check.m_nStatPending = 0;
	}
	m_ActiveChecks.AddToTail( check );
}

void CCourseMode::ReportStat( const char *pszStat )
{
	const int nStatId = FoFCourseStatIdFromName( pszStat );
	if ( nStatId < 0 )
		return;

	for ( int i = 0; i < m_ActiveChecks.Count(); ++i )
	{
		FoFCourseActiveCheck_t &check = m_ActiveChecks[i];
		if ( check.m_nInstruction < 0 ||
			check.m_nInstruction >= m_Instructions.Count() ||
			m_Instructions[check.m_nInstruction].m_nType !=
				FOF_COURSE_CHECK_SINGLEPLAYER ||
			check.m_nStatId != nStatId )
		{
			continue;
		}

		++check.m_nStatPending;
	}
}

void CCourseMode::RemoveCheck( int nCheck, bool bRemoveMarker )
{
	if ( nCheck < 0 || nCheck >= m_ActiveChecks.Count() )
		return;
	const int nInstruction = m_ActiveChecks[nCheck].m_nInstruction;
	if ( bRemoveMarker && nInstruction >= 0 &&
		nInstruction < m_Instructions.Count() &&
		m_Instructions[nInstruction].m_nType == FOF_COURSE_CHECK_LOCATION )
	{
		SendLocationMarker( m_Instructions[nInstruction], false );
	}
	if ( nInstruction >= 0 && nInstruction < m_Instructions.Count() &&
		m_Instructions[nInstruction].m_nType == FOF_COURSE_CHECK_CAPTURE &&
		Q_stricmp( m_Instructions[nInstruction].m_szData, "invisible" ) )
	{
		FoFFireCourseCaptureZoneEvent(
			m_Instructions[nInstruction], 2, false );
	}
	if ( m_ActiveChecks[nCheck].m_hVisual.Get() )
		UTIL_Remove( m_ActiveChecks[nCheck].m_hVisual.Get() );
	m_ActiveChecks.FastRemove( nCheck );
}

bool CCourseMode::EvaluateCheck( FoFCourseActiveCheck_t &check )
{
	if ( check.m_nInstruction < 0 ||
		check.m_nInstruction >= m_Instructions.Count() )
	{
		return false;
	}

	const FoFCourseInstruction_t &instruction =
		m_Instructions[check.m_nInstruction];
	const float flValue = MAX( 0.0f, Q_atof( instruction.m_szValue ) );

	switch ( instruction.m_nType )
	{
	case FOF_COURSE_CHECK_ENEMIES:
		if ( gpGlobals->curtime <= check.m_flTriggerAt )
			return false;
		check.m_flTriggerAt = gpGlobals->curtime + 1.0f;
		return CountAliveCourseBots() <= Q_atoi( instruction.m_szValue );

	case FOF_COURSE_CHECK_LOCATION:
	{
		const int nHalfExtent = check.m_nCheckValue / 2;
		const Vector vecMins = instruction.m_vecOrigin +
			Vector( -nHalfExtent, -nHalfExtent, 0.0f );
		const Vector vecMaxs = instruction.m_vecOrigin +
			Vector( nHalfExtent, nHalfExtent, nHalfExtent );

		CBaseEntity *pEntities[25];
		const int nEntities = UTIL_EntitiesInBox(
			pEntities, ARRAYSIZE( pEntities ),
			vecMins, vecMaxs, FL_CLIENT );
		for ( int i = 0; i < nEntities; ++i )
		{
			CFoF_Player *pLocationPlayer =
				ToFoFPlayer( pEntities[i] );
			if ( pLocationPlayer && !pLocationPlayer->IsBot() &&
				pLocationPlayer->IsAlive() )
			{
				return true;
			}
		}
		return false;
	}

	case FOF_COURSE_CHECK_CAPTURE:
	{
		if ( gpGlobals->curtime < check.m_flLastUpdate + 1.0f )
			return false;
		check.m_flLastUpdate = gpGlobals->curtime;
		int nCapturing = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pCapturePlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pCapturePlayer || !pCapturePlayer->IsAlive() ||
				pCapturePlayer->GetTeamNumber() != m_nPlayerTeam )
				continue;
			if ( ( pCapturePlayer->GetLocalOrigin() -
				instruction.m_vecOrigin ).Length() < 256.0f )
				++nCapturing;
		}
		if ( nCapturing > 0 )
			check.m_flProgress +=
				static_cast< float >( nCapturing ) / MAX( 1, m_nMaxPlayers );

		if ( Q_stricmp( instruction.m_szData, "invisible" ) )
		{
			const int nProgress = flValue > 0.0f ? clamp(
				static_cast< int >( check.m_flProgress * 100.0f / flValue ),
				0, 100 ) : 100;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CFoF_Player *pRecipient = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
				if ( !pRecipient || !pRecipient->IsConnected() || pRecipient->IsBot() )
					continue;
				CSingleUserRecipientFilter filter( pRecipient );
				UserMessageBegin( filter, "CapMessage" );
					WRITE_BYTE( nCapturing );
					WRITE_BYTE( 1 );
					WRITE_BYTE( nProgress );
				MessageEnd();
			}
		}
		return flValue <= 0.0f || check.m_flProgress >= flValue;
	}

	case FOF_COURSE_CHECK_TIMER:
	{
		if ( gpGlobals->curtime > check.m_flLastUpdate )
		{
			check.m_flLastUpdate = gpGlobals->curtime + 1.0f;
			const int nRemaining = static_cast< int >(
				check.m_flTriggerAt - gpGlobals->curtime );
			if ( instruction.m_szData[0] )
			{
				char szTimer[128];
				Q_snprintf( szTimer, sizeof( szTimer ), "%s %i",
					instruction.m_szData, nRemaining );

				hudtextparms_t text;
				text.x = -1.0f;
				text.y = 0.1f;
				text.effect = 0;
				text.r1 = 205;
				text.g1 = 240;
				text.b1 = 71;
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

				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
					if ( pPlayer && !pPlayer->IsObserver() )
						UTIL_HudMessage( pPlayer, text, szTimer );
				}
			}
			if ( nRemaining <= 0 )
				return true;
		}
		return false;
	}

	case FOF_COURSE_CHECK_WAIT:
		return gpGlobals->curtime >= check.m_flTriggerAt;

	case FOF_COURSE_CHECK_TRAIN:
	{
		const int nMode = Q_atoi( instruction.m_szData );
		const char *pszClassname = nMode == 2 ?
			"fof_mobile_point" : "func_tracktrain";
		CBaseEntity *pEntity = NULL;
		for ( CEntitySphereQuery sphere(
			instruction.m_vecOrigin,
			static_cast< float >( check.m_nCheckValue ), 0 );
			( pEntity = sphere.GetCurrentEntity() ) != NULL;
			sphere.NextEntity() )
		{
			if ( FClassnameIs( pEntity, pszClassname ) )
				return true;
		}
		return false;
	}

	case FOF_COURSE_CHECK_SINGLEPLAYER:
		if ( check.m_nStatPending <= 0 )
			return false;
		check.m_nStatRemaining -= check.m_nStatPending;
		check.m_nStatPending = 0;
		DevMsg( "Course SP Check detected: %s / retries %i\n",
			instruction.m_szData, MAX( 0, check.m_nStatRemaining ) );
		return check.m_nStatRemaining <= 0;

	case FOF_COURSE_CHECK_BOT_ORDERS:
		// This check is a persistent spatial order volume. m_szData is the
		// order kind and m_szValue is its radius; scripts remove it explicitly
		// with disable_check when the order should stop applying.
		return false;

	default:
		return false;
	}
}

void CCourseMode::UpdateChecks()
{
	for ( int i = m_ActiveChecks.Count() - 1; i >= 0; --i )
	{
		const int nInstruction = m_ActiveChecks[i].m_nInstruction;
		if ( !EvaluateCheck( m_ActiveChecks[i] ) )
			continue;

		char szOutput[32];
		szOutput[0] = '\0';
		if ( nInstruction >= 0 && nInstruction < m_Instructions.Count() )
			Q_strncpy( szOutput, m_Instructions[nInstruction].m_szOutput,
				sizeof( szOutput ) );
		if ( nInstruction >= 0 && nInstruction < m_Instructions.Count() &&
			m_Instructions[nInstruction].m_nType == FOF_COURSE_CHECK_LOCATION )
		{
			CFoF_Player *pPlayer = FindCoursePlayer();
			FoFReportCourseStat( "reach_loc", pPlayer );
			const char *pszAction = m_Instructions[nInstruction].m_szData;
			if ( pPlayer && pPlayer->IsAlive() &&
				!Q_stricmp( pszAction, "!hurt" ) )
			{
				CTakeDamageInfo info(
					pPlayer, this, 25.0f, DMG_BURN );
				pPlayer->TakeDamage( info );
				PrecacheScriptSound( "FoF.Pain1" );
				pPlayer->EmitSound( "FoF.Pain1" );
			}
			else if ( pPlayer && pPlayer->IsAlive() &&
				!Q_stricmp( pszAction, "!kill" ) )
			{
				CTakeDamageInfo info(
					pPlayer, this, 100.0f, DMG_BLAST );
				pPlayer->TakeDamage( info );
			}
		}
		const bool bClearOwnedHint =
			m_ActiveChecks[i].m_bOwnsHint;
		RemoveCheck( i, true );
		if ( bClearOwnedHint )
			SendCourseHint( "", "", 0 );
		DispatchOutput( szOutput, nInstruction );
		return;
	}
}

int CCourseMode::CountAliveCourseBots() const
{
	int nAlive = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsAlive() &&
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
		{
			++nAlive;
		}
	}
	return nAlive;
}

void CCourseMode::SendCourseHint( const char *pszTitle,
	const char *pszBody, int nMode ) const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
			continue;
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "CourseHint" );
			WRITE_STRING( pszTitle ? pszTitle : "" );
			WRITE_STRING( pszBody ? pszBody : "" );
			WRITE_BYTE( nMode );
		MessageEnd();
	}
}

void CCourseMode::SendNotice( const char *pszToken,
	const char *pszArgument1, int nKind ) const
{
	if ( !pszToken || !pszToken[0] )
		return;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() )
			continue;
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "BBNotices" );
			WRITE_BYTE( nKind );
			WRITE_STRING( pszToken );
			WRITE_STRING( pszArgument1 ? pszArgument1 : "" );
			WRITE_STRING( "" );
			WRITE_STRING( "" );
		MessageEnd();
	}
}

void CCourseMode::SendEquipmentItem(
	CFoF_Player *pPlayer, int nItem ) const
{
	FoFSendEquipmentItem( pPlayer, nItem );
}

void CCourseMode::SendLocationMarker(
	const FoFCourseInstruction_t &instruction, bool bAdd ) const
{
	if ( !instruction.m_szData[0] ||
		!Q_stricmp( instruction.m_szData, "null" ) )
		return;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->GetTeamNumber() != m_nPlayerTeam )
			continue;
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		const bool bShow = bAdd && instruction.m_szData[0] != '!';
		UserMessageBegin( filter, "HUDBBMulti" );
			WRITE_BYTE( bShow ? 1 : 0 );
			WRITE_BYTE( 999 );
			WRITE_BYTE( 7 );
			WRITE_VEC3COORD( instruction.m_vecOrigin );
			WRITE_STRING( bShow ? instruction.m_szData : "" );
		MessageEnd();
	}
}

void FoFUpdateCourseMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 )
		return;

	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	if ( !pCourse )
	{
		pCourse = dynamic_cast< CCourseMode * >(
			gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
		if ( !pCourse )
		{
			pCourse = dynamic_cast< CCourseMode * >(
				CreateEntityByName( "fof_coursemode" ) );
			if ( pCourse )
				DispatchSpawn( pCourse );
		}
		s_hFoFCourseMode = pCourse;
	}
	if ( pCourse )
		pCourse->Update();
}

bool FoFStartCourseMode( const char *pszMapName )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 )
		return false;

	if ( pszMapName && pszMapName[0] &&
		( !gpGlobals || gpGlobals->mapname == NULL_STRING ||
		  Q_stricmp( pszMapName, STRING( gpGlobals->mapname ) ) ) )
	{
		return false;
	}

	// Creation, script loading and delayed player acquisition remain owned by
	// the course controller.  This command-facing entry point merely activates
	// that same path when the requested course map is already loaded.
	FoFUpdateCourseMode();
	return s_hFoFCourseMode.Get() != NULL;
}

int FoFGetCourseMaxPlayers()
{
	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	return pCourse ? pCourse->GetMaxPlayers() : 0;
}

bool FoFShouldDrawCourseImpactMarker( const CBasePlayer *pShooter )
{
	if ( !pShooter || pShooter->IsBot() )
		return false;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 )
		return false;

	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	if ( !pCourse )
	{
		pCourse = dynamic_cast< CCourseMode * >(
			gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
		s_hFoFCourseMode = pCourse;
	}

	return pCourse && pCourse->IsImpactMarkerActive();
}

bool FoFShouldBlockCourseRespawn( const CFoF_Player *pPlayer )
{
	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	if ( !pCourse )
	{
		pCourse = dynamic_cast< CCourseMode * >(
			gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
		s_hFoFCourseMode = pCourse;
	}
	return pCourse && pCourse->ShouldBlockRespawn( pPlayer );
}

bool FoFCourseBotsAreAllied( int nFirstTeam, int nSecondTeam )
{
	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	return pCourse &&
		pCourse->AreBotTeamsAllied( nFirstTeam, nSecondTeam );
}

void FoFReportCourseStat( const char *pszStat, CBaseEntity *pActor )
{
	if ( !pActor || !pszStat || !pszStat[0] )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 )
		return;

	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	if ( !pCourse )
	{
		pCourse = dynamic_cast< CCourseMode * >(
			gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
		s_hFoFCourseMode = pCourse;
	}
	if ( pCourse )
		pCourse->ReportStat( pszStat );
}

static bool FoFCourseClassnameContains(
	const char *pszClassname, const char *pszNeedle )
{
	return pszClassname && pszNeedle &&
		Q_stristr( pszClassname, pszNeedle ) != NULL;
}

void FoFReportCourseKill(
	CBasePlayer *pScorer,
	CBasePlayer *pVictim,
	const CTakeDamageInfo &info )
{
	if ( !pScorer || pScorer == pVictim )
		return;

	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	CFoF_Player *pCourseVictim = ToFoFPlayer( pVictim );
	if ( pCourse && pCourseVictim &&
		( pCourseVictim->m_nPlayerInfo & 0x400000 ) &&
		pCourseVictim->GetTeamNumber() != pCourse->GetCoursePlayerTeam() )
	{
		pCourse->ReportEnemyKilled();
	}

	FoFReportCourseStat( "kill_any", pScorer );

	const int nDamageType = info.GetDamageType();
	CBaseEntity *pInflictor = info.GetInflictor();
	const char *pszInflictor = pInflictor ? pInflictor->GetClassname() : NULL;
	CBaseCombatWeapon *pWeapon =
		dynamic_cast< CBaseCombatWeapon * >( info.GetWeapon() );
	const int nWeaponId = pWeapon ? pWeapon->FoFWeaponID() : -1;

	if ( nDamageType & DMG_DIRECT )
		FoFReportCourseStat( "kill_kick", pScorer );

	if ( nDamageType & DMG_BLAST )
		FoFReportCourseStat( "kill_blast", pScorer );

	// FoF marks a headshot with DMG_SHOCK before Event_Killed.  The shipped
	// course mode reports this independently of the weapon-family objective.
	if ( nDamageType & DMG_SHOCK )
		FoFReportCourseStat( "kill_head", pScorer );

	bool bThrownPhysicsPropKill = ( nDamageType & DMG_CRUSH ) != 0;
	if ( !bThrownPhysicsPropKill && ( nDamageType & DMG_BLAST ) )
	{
		CBreakableProp *pProp =
			dynamic_cast< CBreakableProp * >( pInflictor );
		bThrownPhysicsPropKill = pProp &&
			pProp->HasPhysicsAttacker( 2.0f ) == pScorer;
	}
	if ( bThrownPhysicsPropKill )
		FoFReportCourseStat( "kill_throwprop", pScorer );

	if ( FoFCourseClassnameContains( pszInflictor, "thrown_" ) )
		FoFReportCourseStat( "kill_throwngun", pScorer );

	if ( pWeapon && ( nDamageType & DMG_CLUB ) )
		FoFReportCourseStat( "kill_fists", pScorer );

	if ( nWeaponId == 1 ||
		FoFCourseClassnameContains( pszInflictor, "arrow" ) )
	{
		FoFReportCourseStat( "kill_bow", pScorer );
	}
	else if ( nWeaponId == 3 )
	{
		FoFReportCourseStat( "kill_rifle", pScorer );
	}
	else if ( pWeapon && ( nDamageType & DMG_BUCKSHOT ) )
	{
		FoFReportCourseStat( "kill_shotgun", pScorer );
	}
	else if ( nWeaponId == 5 ||
		FoFCourseClassnameContains( pszInflictor, "dynamite" ) )
	{
		FoFReportCourseStat( "kill_dynamite", pScorer );
	}
	else if ( nWeaponId == 8 )
	{
		FoFReportCourseStat( "kill_blunt", pScorer );
	}
	else if ( nWeaponId == 2 )
	{
		FoFReportCourseStat( "kill_revolver", pScorer );
		CFoF_Player *pFoFScorer = ToFoFPlayer( pScorer );
		if ( pFoFScorer && pFoFScorer->GetFoFHandStance() == 3 )
			FoFReportCourseStat( "kill_fan", pScorer );
	}
}

bool FoFHandleCourseEndMenuSelection(
	CFoF_Player *pPlayer, int nSelection )
{
	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	return pCourse &&
		pCourse->HandleEndMenuSelection( pPlayer, nSelection );
}

bool FoFHandleCourseSelection(
	CFoF_Player *pPlayer, int nSelection )
{
	CCourseMode *pCourse = s_hFoFCourseMode.Get();
	return pCourse && pCourse->HandleCourseSelection( pPlayer, nSelection );
}
