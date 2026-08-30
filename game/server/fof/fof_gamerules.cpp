#include "cbase.h"
#include "fof/fof_breakbad_mode.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_elimination_mode.h"
#include "fof/fof_modes.h"
#include "fof/fof_gamerules.h"
#include "fof/fof_player.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_bot.h"
#include "fof/fof_server_commands.h"
#include "team.h"
#include "fof/fof_player_equipment.h"
#include "game.h"
#include "fof/fof_rounds.h"
#include "recipientfilter.h"
#include "util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFMapProfile_t
{
	const char *m_pszMapName;
	int m_nShootoutSlots;
	int m_nTwoTeamSlots;
	int m_nThreeTeamSlots;
	int m_nFourTeamSlots;
	bool m_bDisableHorseRam;
};

static const FoFMapProfile_t s_FoFMapProfiles[] =
{
	{ "fof_cripplecreek",      16, 24, 20, 20, false },
	{ "fof_fistful",           20, 24, 24, 24, false },
	{ "fof_desperados",        16, 24, 20, 20, false },
	{ "fofhr_monumentvalley",  24, 24, 24, 24, false },
	{ "fofhr_tramonto",        24, 24, 24, 24, false },
	{ "fofhr_coldblood",       24, 24, 24, 24, true  },
	{ "fof_nest",              16, 24, 20, 20, false },
	{ "fof_robertlee",         16, 24, 20, 20, false },
	{ "fof_revenge",           16, 24, 20, 20, true  },
	{ "fof_depot",             16, 24, 20, 20, false },
	{ "fof_winterlong",        16, 24, 20, 20, true  },
	{ "fof_sweetwater",        16, 24, 20, 20, false },
	{ "fof_overtop",           16, 24, 20, 20, false },
	{ "fof_tortuga",           16, 24, 20, 20, false },
	{ "fof_trampled",          16, 24, 24, 24, false },
	{ "fof_impact",            16, 24, 20, 20, false },
	{ "fof_nest_12",           12, 24, 20, 12, false },
	{ "fof_sweetwater_12",     12, 20, 16,  1, false },
	{ "fof_tramonto_12",       12, 16, 12, 12, false },
	{ "fof_robertlee_12",      12, 20, 16, 12, false },
	{ "fof_sawmill_12",        12, 24, 20, 12, false },
	{ "fof_tortuga_12",        12, 16, 16, 12, false },
};

static float s_flFoFHighPingDuration[MAX_PLAYERS + 1];
static float s_flFoFPacketLossScore[MAX_PLAYERS + 1];
static float s_flFoFNextConnectionQualityCheck = 0.0f;

static void FoFSendRulesNotice(
	CFoF_Player *pPlayer, const char *pszToken )
{
	if ( !pPlayer || !pszToken )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
	MessageEnd();
}

static void FoFUpdateConnectionQuality()
{
	if ( !engine->IsDedicatedServer() ||
		gpGlobals->curtime < s_flFoFNextConnectionQualityCheck )
	{
		return;
	}
	s_flFoFNextConnectionQualityCheck = gpGlobals->curtime + 0.25f;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef maxPing( "fof_sv_max_ping", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 6 )
	{
		return;
	}
	const bool bCheckHighPing = maxPing.IsValid() && maxPing.GetInt() > 0;
	static ConVarRef warmup( "fof_warmup", true );
	const bool bCanWarnPacketLoss =
		!warmup.IsValid() || !warmup.GetBool();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			pPlayer->IsHLTV() || pPlayer->IsReplay() )
		{
			s_flFoFHighPingDuration[i] = 0.0f;
			s_flFoFPacketLossScore[i] = 0.0f;
			continue;
		}

		int nPing = 0;
		int nPacketLoss = 0;
		UTIL_GetPlayerConnectionInfo( i, nPing, nPacketLoss );
		if ( bCanWarnPacketLoss && nPacketLoss > 1 )
		{
			s_flFoFPacketLossScore[i] +=
				static_cast< float >( nPacketLoss ) * 0.25f;
			if ( s_flFoFPacketLossScore[i] > 100.0f )
			{
				FoFSendRulesNotice(
					pPlayer, "#Unreliable_Connection_Warning" );
				s_flFoFPacketLossScore[i] = 0.0f;
			}
		}
		if ( !bCheckHighPing )
			continue;
		if ( nPing < maxPing.GetInt() )
		{
			s_flFoFHighPingDuration[i] = MAX(
				0.0f, s_flFoFHighPingDuration[i] - 0.25f );
			continue;
		}

		if ( s_flFoFHighPingDuration[i] == 3.0f )
			FoFSendRulesNotice( pPlayer, "#Warning_High_Ping" );
		s_flFoFHighPingDuration[i] += 0.25f;
		if ( s_flFoFHighPingDuration[i] < 10.0f )
			continue;

		Msg( "=== %s was kicked due high ping or unreliable connection (ping %i loss %i) === \n",
			pPlayer->GetPlayerName(), nPing, nPacketLoss );
		engine->ServerCommand( UTIL_VarArgs(
			"kickid %i kicked from server due high ping or unreliable connection\n",
			pPlayer->GetUserID() ) );
		s_flFoFHighPingDuration[i] = 0.0f;
	}
}

static const FoFMapProfile_t *FoFFindMapProfile( const char *pszMapName )
{
	if ( !pszMapName )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( s_FoFMapProfiles ); ++i )
	{
		if ( !Q_stricmp( pszMapName, s_FoFMapProfiles[i].m_pszMapName ) )
			return &s_FoFMapProfiles[i];
	}
	return NULL;
}

bool FoFShouldUseDynamicRespawns()
{
	if ( !gpGlobals || gpGlobals->mapname == NULL_STRING )
		return false;

	const char *pszMapName = STRING( gpGlobals->mapname );
	if ( !FoFFindMapProfile( pszMapName ) ||
		!Q_strnicmp( pszMapName, "fofhr_", 6 ) ||
		gEntList.FindEntityByClassname( NULL, "legacy_respawns" ) )
	{
		return false;
	}

	return true;
}

bool FoFMapDisablesHorseRam()
{
	if ( !gpGlobals || gpGlobals->mapname == NULL_STRING )
		return false;
	const FoFMapProfile_t *pProfile = FoFFindMapProfile(
		STRING( gpGlobals->mapname ) );
	return pProfile && pProfile->m_bDisableHorseRam;
}

static int FoFMapSizeForCurrentRules( const char *pszMapName )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 1 )
		return 1;

	const FoFMapProfile_t *pProfile = FoFFindMapProfile( pszMapName );
	if ( !pProfile )
		return 1;

	int nSupportedSlots = pProfile->m_nShootoutSlots;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
	{
		static ConVarRef maxTeams( "fof_sv_maxteams", true );
		switch ( maxTeams.IsValid() ? maxTeams.GetInt() : 2 )
		{
		case 2: nSupportedSlots = pProfile->m_nTwoTeamSlots; break;
		case 3: nSupportedSlots = pProfile->m_nThreeTeamSlots; break;
		case 4: nSupportedSlots = pProfile->m_nFourTeamSlots; break;
		default: return 1;
		}
	}

	if ( nSupportedSlots < 16 )
		return 0;
	if ( nSupportedSlots < 21 )
		return 1;
	return 2;
}

static void FoFSelectMapCycle( int nMode, int nMapSize )
{
	const char *pszMapCycle = NULL;
	static ConVarRef ghostTown( "fof_sv_ghost_town", true );
	static ConVarRef mapCycleTP( "fof_sv_mapcycle_tp", true );
	static ConVarRef mapCycleDM( "fof_sv_mapcycle_dm", true );
	static ConVarRef mapCycleDM12( "fof_sv_mapcycle_dm_12", true );

	if ( nMode == 2 )
		pszMapCycle = mapCycleTP.IsValid() ? mapCycleTP.GetString() : "mapcycle_tp.txt";
	else if ( nMode == 6 )
		pszMapCycle = "mapcycle_cm.txt";
	else if ( ghostTown.IsValid() && ghostTown.GetBool() )
		pszMapCycle = "mapcycle_gt.txt";
	else if ( nMode == 5 )
		pszMapCycle = "mapcycle_vs.txt";
	else if ( nMode == 4 && HL2MPRules() && !HL2MPRules()->IsTeamplay() )
		pszMapCycle = "mapcycle_cw.txt";
	else if ( nMapSize == 0 && gpGlobals->maxClients < 13 )
		pszMapCycle = mapCycleDM12.IsValid() ? mapCycleDM12.GetString() : "mapcycle_12.txt";
	else
		pszMapCycle = mapCycleDM.IsValid() ? mapCycleDM.GetString() : "mapcycle.txt";

	if ( pszMapCycle && pszMapCycle[0] )
		mapcyclefile.SetValue( pszMapCycle );
}

static bool FoFMapSupportsMode( const char *pszMapName, int nMode )
{
	if ( !pszMapName )
		return false;
	if ( nMode == 4 )
		return Q_stristr( pszMapName, "fof_" ) != NULL ||
			Q_stristr( pszMapName, "fofhr_" ) != NULL;
	if ( nMode == 5 )
		return Q_stristr( pszMapName, "vs_" ) != NULL;
	return true;
}

bool FoFShouldPreserveRoundEntity( const char *pszClassname )
{
	static const char *s_pszPreservedEntities[] =
	{
		"fof_teamplay",
		"fof_breakbad",
		"fof_elimination",
		"fof_versus",
		"fof_coursemode",
	};

	for ( int i = 0; i < ARRAYSIZE( s_pszPreservedEntities ); ++i )
	{
		if ( !Q_stricmp( pszClassname, s_pszPreservedEntities[i] ) )
			return true;
	}

	return false;
}

float FoFGetItemRespawnTime( void )
{
	static ConVarRef itemRespawnTime(
		"fof_sv_item_respawn_time", true );
	return itemRespawnTime.IsValid() ? itemRespawnTime.GetFloat() : 30.0f;
}

bool CHL2MPRules::FoFAllowEntitySpawn( const char *pszClassname )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 ||
		FoFGetCourseMaxPlayers() >= 2 )
	{
		return true;
	}

	if ( !pszClassname )
		return false;

	return !Q_stricmp( pszClassname, "crate" ) ||
		!Q_stricmp( pszClassname, "barrel" );
}

void CHL2MPRules::GetFoFNeutralColor(
	int nContext, float &flRed, float &flGreen, float &flBlue )
{
	(void)nContext;
	flRed = 0.76f;
	flGreen = 0.76f;
	flBlue = 0.76f;
}

bool CHL2MPRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 2 &&
		!IsFoFTeamplayRespawnAllowed() )
	{
		return false;
	}

	return CTeamplayRules::FPlayerCanRespawn( pPlayer );
}

void CHL2MPRules::UpdateFoFPeriodicCash()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode != 1 && nMode != 3 )
		return;

	static ConVarRef warmup( "fof_warmup", true );
	if ( ( warmup.IsValid() && warmup.GetBool() ) || g_fGameOver )
	{
		m_flNextFoFCashTick = 0.0f;
		return;
	}

	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	if ( classicShootout.IsValid() && classicShootout.GetBool() )
		return;

	if ( m_flNextFoFCashTick > gpGlobals->curtime )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			continue;
		}

		const float flIncrement =
			pPlayer->FragCount() == 0 && pPlayer->DeathCount() == 0 &&
			pPlayer->GetFoFCash() == 0.0f ? 30.0f : 1.0f;
		pPlayer->m_flFoFCash += flIncrement;
	}

	m_flNextFoFCashTick = gpGlobals->curtime + 2.0f;
}

void CHL2MPRules::InitializeFoFRuleTeams()
{
	static const char *s_pszTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"Vigilantes",
		"Desperados",
		"Bandidos",
		"Rangers",
		"Zombies",
	};

	for ( int i = 0; i < ARRAYSIZE( s_pszTeamNames ); ++i )
	{
		CTeam *pTeam = static_cast< CTeam * >(
			CreateEntityByName( "team_manager" ) );
		pTeam->Init( s_pszTeamNames[i], i );
		g_Teams.AddToTail( pTeam );
	}
}

void CHL2MPRules::UpdateFoFServerState()
{
	static ConVarRef teamplay( "mp_teamplay", true );
	if ( teamplay.IsValid() )
		m_bTeamPlayEnabled = teamplay.GetBool();

	InitializeFoFMapState();
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bBreakBad = currentMode.IsValid() && currentMode.GetInt() == 3;
	// BreakBad processes the cash clock before the warmup transition. A
	// zeroed balance must reach the deferred spawn award before idle income
	// can turn it into the ordinary zero-cash starter grant.
	if ( bBreakBad )
		UpdateFoFPeriodicCash();
	UpdateFoFWarmupState();
	if ( !bBreakBad )
		UpdateFoFPeriodicCash();
	UpdateFoFTeamplayState();
	FoFUpdateBreakBadMode();
	FoFUpdateCourseMode();
	CDarkMode *pElimination = dynamic_cast< CDarkMode * >(
		gEntList.FindEntityByClassname( NULL, "fof_elimination" ) );
	if ( pElimination )
		pElimination->ModeThink();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
			pPlayer->RetryPendingFoFSpawn();
	}
	FoFUpdateBotPopulation();
	FoFUpdateGhostTownBots();
	UpdateFoFTeamClassState();
	FoFUpdateConnectionQuality();
}

void CHL2MPRules::HandleFoFPlayerKilled(
	CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	FoFReportCourseKill(
		GetDeathScorer( info.GetAttacker(), info.GetInflictor() ),
		pVictim,
		info );
}

void CHL2MPRules::BeginFoFIntermission()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			pPlayer->AddFlag( FL_FROZEN );
	}

	FoFPresentEndMapStatistics();
}

void CHL2MPRules::HandleFoFClientSettingsChanged(
	CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( pFoFPlayer )
	{
		if ( !pFoFPlayer->IsFakeClient() )
		{
			const char *pszFOV = engine->GetClientConVarValue(
				pFoFPlayer->entindex(), "fov_desired" );
			if ( pszFOV && pszFOV[0] )
			{
				const int nFOV = clamp( Q_atoi( pszFOV ), 75, 90 );
				if ( nFOV != pFoFPlayer->GetFoFPlayerFOV() )
				{
					pFoFPlayer->QueueFoFPlayerFOV( nFOV );
					ClientPrint(
						pFoFPlayer, HUD_PRINTTALK,
						"Your FoV will be adjusted after respawn" );
				}
			}
		}
		pFoFPlayer->SetPlayerModel();
	}
	BaseClass::ClientSettingsChanged( pPlayer );
}

static int FoFCountNoBotsHumanPlayers()
{
	int nHumans = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsFakeClient() ||
			pPlayer->IsHLTV() || pPlayer->IsReplay() )
		{
			continue;
		}
		++nHumans;
	}
	return nHumans;
}

void CHL2MPRules::HandleFoFNoBotsVote( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsFakeClient() )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef botEdit( "fof_sv_bot_edit", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode == 6 || nMode == 5 ||
		( botEdit.IsValid() && botEdit.GetBool() ) ||
		( nMode == 4 && !IsTeamplay() ) )
	{
		return;
	}

	const CSteamID *pSteamID = engine->GetClientSteamID( pPlayer->edict() );
	if ( pSteamID && pSteamID->IsValid() )
	{
		const uint64 nSteamID = pSteamID->ConvertToUint64();
		if ( m_FoFNoBotsVoters.Find( nSteamID ) !=
			m_FoFNoBotsVoters.InvalidIndex() )
		{
			return;
		}
		m_FoFNoBotsVoters.AddToTail( nSteamID );
	}

	int nVotesNeeded = FoFCountNoBotsHumanPlayers();

	if ( m_FoFNoBotsVoters.Count() >= nVotesNeeded )
	{
		UTIL_ClientPrintAll( HUD_PRINTTALK, "#NoBots_Vote_Done" );
		static ConVarRef slotPercentage( "fof_sv_bot_slotpct", true );
		if ( slotPercentage.IsValid() )
			slotPercentage.SetValue( 0.0f );
		FoFUpdateBotPopulation();
		return;
	}

	char szVotesRemaining[16];
	Q_snprintf( szVotesRemaining, sizeof( szVotesRemaining ), "%d",
		nVotesNeeded - m_FoFNoBotsVoters.Count() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#NoBots_Vote_Left",
		szVotesRemaining );
}

float CHL2MPRules::GetFoFTimeLimitMinutes() const
{
	return flTimeLimit;
}

int CHL2MPRules::GetFoFMapSize() const
{
	return m_nMapSize;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF server-side initialization for the HL2MP game-rules table.
//
//=============================================================================//

void CHL2MPRules::InitializeFoFServerState()
{
	m_nMapSize = 1;
	m_nTPClassesTotal = 0;
	flTimeLimit = 0.0f;
	m_flNextFoFTeamClassStateUpdate = 0.0f;
	m_flNextFoFCashTick = 0.0f;
	m_flFoFWarmupEndTime = 0.0f;
	m_bFoFMapStateInitialized = false;
	m_bFoFWarmupComplete = true;
	m_hFoFTeamplayController = NULL;
	m_nFoFTeamplayRoundState = 0;
	m_flFoFTeamplayStateDeadline = 0.0f;
	m_flFoFTeamplayBuyEndTime = 0.0f;
	m_nFoFTeamplayRespawnState = 2;
	m_nFoFTeamplayLastBuyTick = -1;
	m_FoFNoBotsVoters.RemoveAll();
	Q_memset( s_flFoFHighPingDuration, 0,
		sizeof( s_flFoFHighPingDuration ) );
	Q_memset( s_flFoFPacketLossScore, 0,
		sizeof( s_flFoFPacketLossScore ) );
	s_flFoFNextConnectionQualityCheck = 0.0f;

	for ( int i = 0; i < CHL2MPRules::FOF_TEAMPLAY_CLASS_COUNT; ++i )
	{
		m_nTPClassesUsage.Set( i, 0 );
		m_nTPClassesSlots.Set( i, 0 );
		m_nTPClassesUsageDesp.Set( i, 0 );
		m_nTPClassesSlotsDesp.Set( i, 0 );
	}

	for ( int i = 0; i < CHL2MPRules::FOF_SAFE_ZONE_POINT_COUNT; ++i )
	{
		vSafeZone1.Set( i, vec3_origin );
		vSafeZone2.Set( i, vec3_origin );
		vSafeZone3.Set( i, vec3_origin );
		vSafeZone4.Set( i, vec3_origin );
		m_bSafeZoneState.Set( i, false );
	}
}

void CHL2MPRules::ApplyFoFMapOwnedRuleSelection()
{
	FoFApplyMapOwnedModeSelection();
	m_bTeamPlayEnabled = teamplay.GetBool();
}

void CHL2MPRules::InitializeFoFMapState()
{
	if ( m_bFoFMapStateInitialized )
		return;

	// FoF defers this work until the first game-rules Think.  At that
	// point server.cfg/listenserver.cfg and the command line have supplied the
	// selected mode.  Running it from the constructor or ServerActivate reads
	// defaults and creates the wrong controller for the entire map.
	m_bFoFMapStateInitialized = true;
	static ConVarRef forceSpectator( "fof_sv_force_spect", true );
	if ( forceSpectator.IsValid() )
		forceSpectator.SetValue( 0 );
	FoFExecuteMapConfig();
	ApplyFoFMapOwnedRuleSelection();
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef pastMode( "fof_sv_pastmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( pastMode.IsValid() )
		pastMode.SetValue( nMode );
	m_nMapSize = FoFMapSizeForCurrentRules(
		gpGlobals->mapname != NULL_STRING ? STRING( gpGlobals->mapname ) : "" );
	FoFSelectMapCycle( nMode, m_nMapSize );
	const char *pszMapName = gpGlobals->mapname != NULL_STRING ?
		STRING( gpGlobals->mapname ) : "";
	if ( !FoFMapSupportsMode( pszMapName, nMode ) )
	{
		if ( nMode == 4 )
		{
			Warning( "\n\n WARNING! %s isn't compatible with Elimination mode, moving to next map\n\n",
				pszMapName );
		}
		else
		{
			Warning( "\n\n WARNING! %s isn't compatible with Versus mode, moving to next map\n\n",
				pszMapName );
		}
		ChangeLevel();
		return;
	}
	static ConVarRef timeLimit( "mp_timelimit", true );
	flTimeLimit = timeLimit.IsValid() ? timeLimit.GetFloat() : 0.0f;

	InitializeFoFWarmupState();
	FoFCreateModeControllerForCurrentGame();
	InitializeFoFTeamplayState();
}

int CHL2MPRules::GetFoFTeamClassCount() const
{
	return clamp( static_cast< int >( m_nTPClassesTotal ),
		0, CHL2MPRules::FOF_TEAMPLAY_CLASS_COUNT );
}

bool CHL2MPRules::IsFoFTeamClassAvailable(
	int classIndex, int teamNumber ) const
{
	if ( classIndex < 0 ||
		classIndex >= GetFoFTeamClassCount() )
		return false;

	if ( teamNumber == TEAM_COMBINE )
	{
		return m_nTPClassesUsage[classIndex] <
			m_nTPClassesSlots[classIndex];
	}

	return m_nTPClassesUsageDesp[classIndex] <
		m_nTPClassesSlotsDesp[classIndex];
}

int CHL2MPRules::SelectFoFAvailableTeamClass( int teamNumber )
{
	const int nClassCount = GetFoFTeamClassCount();
	if ( nClassCount <= 0 )
		return -1;

	for ( int nAttempt = 0; nAttempt < 50; ++nAttempt )
	{
		const int nClass = random->RandomInt( 0, nClassCount - 1 );
		if ( !IsFoFTeamClassAvailable( nClass, teamNumber ) )
			continue;

		if ( teamNumber == TEAM_COMBINE )
		{
			m_nTPClassesUsage.Set( nClass,
				m_nTPClassesUsage[nClass] + 1 );
		}
		else
		{
			m_nTPClassesUsageDesp.Set( nClass,
				m_nTPClassesUsageDesp[nClass] + 1 );
		}
		return nClass;
	}

	// FoF falls back to the first class when the bounded allocation search
	// exhausts the temporary slots; a bot must not spawn without a class.
	return 0;
}

void CHL2MPRules::UpdateFoFTeamClassState()
{
	if ( gpGlobals->curtime < m_flNextFoFTeamClassStateUpdate )
		return;
	m_flNextFoFTeamClassStateUpdate = gpGlobals->curtime + 0.25f;

	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	const bool bEnabled = teamClasses.IsValid() && teamClasses.GetBool();
	int nClassCount = 0;
	for ( int nClass = 0;
		nClass < CHL2MPRules::FOF_TEAMPLAY_CLASS_COUNT;
		++nClass )
	{
		int nSlots = 0;
		if ( bEnabled )
		{
			char szClassName[64];
			float flShare = 0.0f;
			if ( FoFGetTeamClassDefinition(
				nClass, szClassName, sizeof( szClassName ), flShare ) )
			{
				++nClassCount;
				nSlots = clamp( static_cast< int >(
					gpGlobals->maxClients * 0.5f * flShare ), 1, 99 );
			}
		}

		m_nTPClassesSlots.Set( nClass, nSlots );
		m_nTPClassesSlotsDesp.Set( nClass, nSlots );
		// These are temporary allocation counters, not persistent occupancy.
		// The original rules clear them on the quarter-second update. Keeping
		// every bot's class here permanently disables the human class cards.
		m_nTPClassesUsage.Set( nClass, 0 );
		m_nTPClassesUsageDesp.Set( nClass, 0 );
	}
	m_nTPClassesTotal = nClassCount;
}

void CHL2MPRules::ResetFoFSafeZoneNetwork()
{
	for ( int i = 0; i < CHL2MPRules::FOF_SAFE_ZONE_POINT_COUNT; ++i )
	{
		vSafeZone1.Set( i, vec3_origin );
		vSafeZone2.Set( i, vec3_origin );
		vSafeZone3.Set( i, vec3_origin );
		vSafeZone4.Set( i, vec3_origin );
		m_bSafeZoneState.Set( i, false );
	}
}

bool CHL2MPRules::SetFoFSafeZoneQuad(
	int index,
	const Vector &northWest,
	const Vector &northEast,
	const Vector &southWest,
	const Vector &southEast,
	bool active )
{
	if ( index < 0 ||
		index >= CHL2MPRules::FOF_SAFE_ZONE_POINT_COUNT )
		return false;

	// The shipped client consumes these in the non-numeric order
	// v2, v1, v3, v4.  Keep the server assignment named explicitly.
	vSafeZone1.Set( index, northWest );
	vSafeZone2.Set( index, northEast );
	vSafeZone3.Set( index, southWest );
	vSafeZone4.Set( index, southEast );
	m_bSafeZoneState.Set( index, active );
	return true;
}

void FoFPrecacheAssets()
{
	static const char *s_pszHatModels[] =
	{
		"models/hats/hat1.mdl",
		"models/hats/hat2.mdl",
		"models/hats/hat3.mdl",
		"models/hats/hat4.mdl",
		"models/hats/hat5.mdl",
		"models/hats/hat6.mdl",
		"models/hats/hat7.mdl",
		"models/hats/hat8.mdl",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszHatModels ); ++i )
		CBaseEntity::PrecacheModel( s_pszHatModels[i], true );

	static const char *s_pszPlayerModels[] =
	{
		"models/playermodels/player1.mdl",
		"models/playermodels/player2.mdl",
		"models/playermodels/bandito.mdl",
		"models/playermodels/frank.mdl",
		"models/zombies/fof_zombie.mdl",
		"models/npc/ghost.mdl",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszPlayerModels ); ++i )
		CBaseEntity::PrecacheModel( s_pszPlayerModels[i], true );

	static const char *s_pszModels[] =
	{
		"models/props/cap_circle_64.mdl",
		"models/props/gun_cabinet/gun_cabinet.mdl",
		"models/props/gun_cabinet/gun_cabinet_gold.mdl",
		"models/props/gun_cabinet/gun_cabinet_glass.mdl",
		"models/props/gun_cabinet/gun_cabinet_gold_glass.mdl",
		"sprites/glow1.vmt",
		"models/player.mdl",
		"models/gibs/agibs.mdl",
		"models/gibs/hgibs.mdl",
		"models/elpaso/fire_fragment1.mdl",
		"models/elpaso/barrel1_explosive.mdl",
		"models/elpaso/barrel2_explosive.mdl",
		"models/props_junk/watermelon01_spiky.mdl",
		"models/weapons/v_fists.mdl",
		"models/horse/riding_horse.mdl",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszModels ); ++i )
		CBaseEntity::PrecacheModel( s_pszModels[i], true );

	static const char *s_pszSounds[] =
	{
		"Course.End_Music",
		"Chicken.Attack",
		"Standoff_Music",
		"Player.Slide",
		"BB.MostWantedWarning",
		"FoF.MultiKill1",
		"FoF.MultiKill2",
		"FoF.MultiKill3",
		"FoF.MultiKill_Special",
		"Whiskey.Glug",
		"FoF.SackRelocated",
		"FoFPlayer.SideJump",
		"FoFPlayer.Equipment",
		"FoFPlayer.WeaponPickUp",
		"FoFPlayer.KnifePickUp",
		"FoFPlayer.AxePickUp",
		"FoFPlayer.KickSwoosh",
		"FoFPlayer.KickHit",
		"FoFPlayer.KickHitBoots",
		"Weapon_Handgun.Throw",
		"FoF.BuyTick",
		"FoF.BuyTickEnd",
		"FX_RicochetSound.BoilerPlate",
		"FoF.LootIn",
		"Player.Spurs",
		"Player.WhiskeyMovement",
		"anthem.victory",
		"anthem.defeat",
		"Bounty.Victory",
		"Bounty.Defeat",
		"FoF.BountyObjective",
		"Course.Stinger2",
		"Course.Stinger3",
		"Course.Stinger4",
		"Mexican.Death1",
		"Mexican.DropSombrero",
		"FoF.Banked",
		"BaseGrenade.ExplodeBig",
		"FoF.HeadshotLoud",
		"FoF.Headshot",
		"FoF.Pain1",
		"FoF.SmallPain",
		"FoF.Death",
		"Player.FallGib",
		"FoF.Death_p2",
		"FoF.Death_p3",
		"FoF.Death_p4",
		"Ghost.Alert",
		"Ghost.Death",
		"HL2Player.BurnPain",
		"FoF.RespawnAlert",
		"Ragged_Powerup.Full",
		"Zombie.AttackHit",
		"Zombie.AttackMiss",
		"Zombie.Pain",
		"Zombie.Die",
		"Zombie.Alert",
		"Zombie.Idle",
		"ZombieFoF.Attack",
		"ZombieFoF.Chase",
		"NPC_Horse.Pain",
		"Horse.ClopBack",
		"Horse.Clop",
		"Horse.Gallop",
		"Horse.Gallop2",
		"Horse.Gallop3",
		"Horse.Gallop4",
		"Horse.Gallop_Hard",
		"Horse.Slide",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszSounds ); ++i )
		CBaseEntity::PrecacheScriptSound( s_pszSounds[i] );

	UTIL_PrecacheOther( "fof_ghost" );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server configuration surface. Gameplay systems use ConVarRef to keep
// registration separate from implementation dependencies.
//
//=============================================================================//

#define FOF_SERVER_CVAR( symbol, value, flags, help ) \
	ConVar symbol( #symbol, value, flags, help )

// Public server state and compatibility controls. These are not FoF-prefixed,
// but they are owned by the
// game DLL rather than the engine.
FOF_SERVER_CVAR( map_start_time, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( mapname, "", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( nextfoflevel, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( player_usercommand_timeout, "0", FCVAR_GAMEDLL,
	"After this many seconds without a usercommand from a player, the client is kicked." );
FOF_SERVER_CVAR( sv_showimpacts, "0", FCVAR_GAMEDLL | FCVAR_CHEAT | FCVAR_REPLICATED,
	"Shows client (red) and server (blue) bullet impact point" );

FOF_SERVER_CVAR( fof_bot_debug, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_bot_forceopenchest, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_bot_forceweapon, "", FCVAR_GAMEDLL, "Accepts weapon names, example: weapon_coltnavy" );
FOF_SERVER_CVAR( fof_bot_mimic, "0", FCVAR_GAMEDLL, "Bot uses usercmd of player by index." );
FOF_SERVER_CVAR( fof_bot_mimic_yaw_offset, "0", FCVAR_GAMEDLL, "Offsets the bot yaw." );
FOF_SERVER_CVAR( fof_bot_scriptname, "1_default_bots.txt", FCVAR_GAMEDLL, "sets the name of the bot script file to load, must end in .txt" );
FOF_SERVER_CVAR( fof_bot_skill, "2", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_bot_stop, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_course_forced_team, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_course_script, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_debug_respawns, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_gunshot_wallocclusion, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_npc_horsekickdamage, "100", FCVAR_GAMEDLL, "Horse kick damage" );
FOF_SERVER_CVAR( fof_npc_horsekickforce, "500", FCVAR_GAMEDLL, "Horse kick force applied to player" );
FOF_SERVER_CVAR( fof_player_prog, "0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_battle_royale, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_battle_royale_ffa, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_bot_dynamicjoin, "0", FCVAR_GAMEDLL, "bots join and leave the server depending how many human players stay" );
FOF_SERVER_CVAR( fof_sv_bot_edit, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_bot_edit_active, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_bot_slotpct, "0", FCVAR_GAMEDLL, "percnetage of servers slots to be filled by bots" );
FOF_SERVER_CVAR( fof_sv_br_healrate, "2", FCVAR_GAMEDLL, "Heal rate when players are inside safe zone's inner circle" );
FOF_SERVER_CVAR( fof_sv_br_postsafe_time, "90", FCVAR_GAMEDLL, "Time after the safe zone has shrink" );
FOF_SERVER_CVAR( fof_sv_br_presafe_time, "25", FCVAR_GAMEDLL, "Time before safe zone gets activated" );
FOF_SERVER_CVAR( fof_sv_br_safe_time, "40", FCVAR_GAMEDLL, "Time it takes safe zone to shrink" );
FOF_SERVER_CVAR( fof_sv_capture_notoriety, "50", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_cart_highlight, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "Outline effect active for cart model" );
FOF_SERVER_CVAR( fof_sv_classic_shootout, "", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
// FoF registers this bounded server child with FCVAR_REPLICATED only;
// ConVar_Register adds FCVAR_GAMEDLL for the module. Matching that order is
// important because the client registers the parent first in a listen server.
ConVar fof_sv_currentmode(
	"fof_sv_currentmode", "1", FCVAR_REPLICATED, "",
	true, 1.0f, true, 6.0f );
FOF_SERVER_CVAR( fof_sv_disable_killstreak, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_dm_comp, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_dm_comp_points, "500", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_dm_timer_ends_map, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_dynamite_shot, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_elm_buytime, "7", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_elm_extratime, "45", FCVAR_GAMEDLL, "Time interval until enemy silouttes are visible again" );
FOF_SERVER_CVAR( fof_sv_elm_freevision, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_elm_norespawn, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_elm_roundtime, "240", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_force_spect, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_force_weapons, "0", FCVAR_GAMEDLL, "Forces one or several weapons to be used by every player. Set to 1 to enable" );
FOF_SERVER_CVAR( fof_sv_force_weapons_list, "empty", FCVAR_GAMEDLL, "Accepts weapon names separated by commas. Examples: 'weapon_coltnavy,weapon_knife'" );
FOF_SERVER_CVAR( fof_sv_ghost_town, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_ghost_town_xbow, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_headshots_only, "", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_health_max, "100", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_item_respawn_time, "60", FCVAR_GAMEDLL | FCVAR_NOTIFY, "" );
FOF_SERVER_CVAR( fof_sv_mapcycle_dm, "mapcycle.txt", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_mapcycle_dm_12, "mapcycle_12.txt", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_mapcycle_dm_32, "mapcycle_32.txt", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_mapcycle_tp, "mapcycle_tp.txt", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_mapdistance_mult, "1.0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_max_ping, "250", FCVAR_GAMEDLL | FCVAR_REPLICATED, "Max latency players are allowed to play in a server. Set to 0 to disable" );
FOF_SERVER_CVAR( fof_sv_maxidle_secs, "90", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_maxrounds, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How many rounds are played until map ends" );
ConVar fof_sv_maxteams(
	"fof_sv_maxteams", "4", FCVAR_REPLICATED, "",
	true, 2.0f, true, 4.0f );
FOF_SERVER_CVAR( fof_sv_motd_countdown, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "MOTD panel countdown in seconds" );
FOF_SERVER_CVAR( fof_sv_nemesis, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "Enables nemesis system for Shootout modes" );
FOF_SERVER_CVAR( fof_sv_obj_warmuptime, "25", FCVAR_GAMEDLL, "Warm up lenght" );
FOF_SERVER_CVAR( fof_sv_optional_output, "0", FCVAR_GAMEDLL, "Remove certain optional map entities" );
ConVar fof_sv_pastmode(
	"fof_sv_pastmode", "0", FCVAR_GAMEDLL | FCVAR_HIDDEN, "",
	true, 0.0f, true, 5.0f );
FOF_SERVER_CVAR( fof_sv_pickup_maxweight, "120", FCVAR_GAMEDLL, "Maximum wieght player can carry" );
FOF_SERVER_CVAR( fof_sv_playerattack_allowed, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_bow, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_carbine, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_dynamite, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_henry, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_lefthanded, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_righthanded, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_sharps, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_pricemult_walker, "1.0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_rankedserver, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_rankedserver_tp, "0", FCVAR_GAMEDLL, "" );
ConVar fof_sv_recoilamount(
	"fof_sv_recoilamount", "0.8",
	FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED,
	"Recoil amount for handguns", true, 0.8f, true, 0.8f );
FOF_SERVER_CVAR( fof_sv_roundsplayed, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_scrambleteams, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_shootout_custom, "0", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_smoke_alpha_max, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How translucid smoke is initially (forced on clients)" );
FOF_SERVER_CVAR( fof_sv_smoke_alpha_min, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How translucid smoke is when removed (forced on clients)" );
FOF_SERVER_CVAR( fof_sv_smoke_amount, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How many smoke pufs are emitted by weapons (forced on clients)" );
FOF_SERVER_CVAR( fof_sv_smoke_life, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How many seconds take the smoke to dissipate (forced on clients)" );
FOF_SERVER_CVAR( fof_sv_smoke_size, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "Smoke's initial size (forced on clients)" );
FOF_SERVER_CVAR( fof_sv_spawn_invul_time, "2", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_speedpenalty, "0.7", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_supercharged, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_supercharged_respawntime, "75", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_team_glow, "", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_team_remap_1, "2", FCVAR_GAMEDLL | FCVAR_REPLICATED, "reassign a team to team slot 1: 2-vigilantes, 3-desperados, 4-bandidos, 5-rangers" );
FOF_SERVER_CVAR( fof_sv_team_remap_2, "3", FCVAR_GAMEDLL | FCVAR_REPLICATED, "reassign a team to team slot 2: 2-vigilantes, 3-desperados, 4-bandidos, 5-rangers" );
FOF_SERVER_CVAR( fof_sv_team_remap_3, "4", FCVAR_GAMEDLL | FCVAR_REPLICATED, "reassign a team to team slot 3: 2-vigilantes, 3-desperados, 4-bandidos, 5-rangers" );
FOF_SERVER_CVAR( fof_sv_team_remap_4, "5", FCVAR_GAMEDLL | FCVAR_REPLICATED, "reassign a team to team slot 4: 2-vigilantes, 3-desperados, 4-bandidos, 5-rangers" );
FOF_SERVER_CVAR( fof_sv_teambalance_allowed, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
ConVar fof_sv_teamplay_autoroundend(
	"fof_sv_teamplay_autoroundend", "1", FCVAR_GAMEDLL | FCVAR_HIDDEN,
	"round ends automatically when a team reaches its score goal",
	true, 0.0f, true, 1.0f );
ConVar fof_sv_teamplay_alivecheck(
	"fof_sv_teamplay_alivecheck", "0", FCVAR_GAMEDLL | FCVAR_HIDDEN,
	"when declare winner input is used, game grants full points to only team alive",
	true, 0.0f, true, 1.0f );
FOF_SERVER_CVAR( fof_sv_tp_buytime, "5", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c0, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c1, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c2, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c3, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c4, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c5, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c6, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_classes_c7, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_tp_glowtime, "7", FCVAR_GAMEDLL, "amount of seconds alive team mates glow after respawn" );
FOF_SERVER_CVAR( fof_sv_tp_switchsides, "0", FCVAR_GAMEDLL, "Switch sides when a team reaches the score set by fof_sv_winlimit" );
FOF_SERVER_CVAR( fof_sv_versus_arena1_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena1_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena2_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena2_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena3_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena3_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena4_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena4_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena5_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena5_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena6_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena6_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena7_items_a, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena7_items_b, "", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arena_clones, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_arenas, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_round_number, "1", FCVAR_GAMEDLL, "Rounds to play per match" );
FOF_SERVER_CVAR( fof_sv_versus_round_switch, "1", FCVAR_GAMEDLL, "Players switch sides before playing next round" );
FOF_SERVER_CVAR( fof_sv_versus_round_time, "30", FCVAR_GAMEDLL, "Time a single match last" );
FOF_SERVER_CVAR( fof_sv_versus_tournament, "0", FCVAR_GAMEDLL, "Enables tournament mode (WIP)" );
FOF_SERVER_CVAR( fof_sv_versus_tournament_matches, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_versus_tournament_playarena, "3", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_votekickallowed, "1", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_wcrate_regentime, "100", FCVAR_GAMEDLL, "" );
FOF_SERVER_CVAR( fof_sv_weapon_drop, "0", FCVAR_GAMEDLL | FCVAR_NOTIFY | FCVAR_REPLICATED, "Teamplay cvar, set 0 for normal drops (as in DM), 1 all weapons drop, 2 all weapons drop with weapon aging, 3 no weapons drop" );
FOF_SERVER_CVAR( fof_sv_weaponmenu, "1", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );
FOF_SERVER_CVAR( fof_sv_winlimit, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "How many points a team needs to get to win the map" );
FOF_SERVER_CVAR( fof_warmup, "0", FCVAR_GAMEDLL | FCVAR_REPLICATED, "" );

#undef FOF_SERVER_CVAR

CON_COMMAND( fof_addprobe, "" )
{
	if ( args.ArgC() != 6 )
		return;

	FoFAddManualRespawnProbe(
		args[1], static_cast< unsigned int >( Q_atoi( args[2] ) ),
		Vector( Q_atof( args[3] ), Q_atof( args[4] ),
			Q_atof( args[5] ) ) );
}

CON_COMMAND( fof_debug_printcoords, "" )
{
	Vector coordinate;
	if ( !FoFGetRespawnProbeDebugCoordinate( &coordinate ) )
		return;

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	const char *pszCoordinates = UTIL_VarArgs( "%.2f %.2f %.2f\n",
		coordinate.x, coordinate.y, coordinate.z );
	if ( pPlayer )
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, pszCoordinates );
	else
		Msg( "%s", pszCoordinates );
}

CON_COMMAND( fof_debug_respawns_probemap, "" )
{
	FoFGenerateRespawnProbeMap();
}
