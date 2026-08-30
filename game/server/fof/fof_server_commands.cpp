//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-side gameplay commands. Developer probes and retired private
// account commands are intentionally excluded.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_course_mode.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_server_commands.h"
#include "fof/fof_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void FoFRoundRestartCommand( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CHL2MPRules *pRules = HL2MPRules();
	if ( pRules )
		pRules->RestartGame();
}

static void FoFStartCourseCommand( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	FoFStartCourseMode( args.ArgC() > 1 ? args[1] : "" );
}

static void FoFDynamiteThrowCommand( const CCommand &args )
{
	// The original command is itself only this diagnostic.
	// The actual dynamite throw remains weapon input, not a console command.
	Msg( "You must be a server admin to use this command\n" );
}

static ConCommand round_restart(
	"round_restart", FoFRoundRestartCommand, "", FCVAR_GAMEDLL );
static ConCommand start_course(
	"start_course", FoFStartCourseCommand, "", FCVAR_GAMEDLL );
static ConCommand dyn_throw(
	"dyn_throw", FoFDynamiteThrowCommand, "", FCVAR_GAMEDLL );

struct FoFEndMapAwards_t
{
	FoFEndMapAwards_t()
		: m_flBestRatio( 0.0f ),
		  m_nBestRatioPlayer( 0 ),
		  m_nBestKillstreak( 0 ),
		  m_nBestKillstreakPlayer( 0 ),
		  m_flBestAccuracy( 0.0f ),
		  m_nBestAccuracyPlayer( 0 ),
		  m_nBestDrunkard( 0 ),
		  m_nBestDrunkardPlayer( 0 )
	{
	}

	float m_flBestRatio;
	int m_nBestRatioPlayer;
	int m_nBestKillstreak;
	int m_nBestKillstreakPlayer;
	float m_flBestAccuracy;
	int m_nBestAccuracyPlayer;
	int m_nBestDrunkard;
	int m_nBestDrunkardPlayer;
};

static CFoF_Player *FoFEndMapPlayerByIndex( int nPlayerIndex )
{
	return ToFoFPlayer( UTIL_PlayerByIndex( nPlayerIndex ) );
}

static bool FoFEndMapAwardEligible( CFoF_Player *pPlayer )
{
	return pPlayer && !pPlayer->IsFakeClient() &&
		pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

static float FoFEndMapRatio( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return 0.0f;

	const int nFrags = clamp( pPlayer->FragCount(), 1, 10000 );
	const float flMinutes = clamp(
		pPlayer->GetFoFMapPlayTime() / 60.0f, 5.0f, 1000.0f );
	return static_cast< float >( nFrags ) / flMinutes;
}

static void FoFFindEndMapAwards( FoFEndMapAwards_t &awards )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = FoFEndMapPlayerByIndex( i );
		if ( !FoFEndMapAwardEligible( pPlayer ) )
			continue;

		const float flRatio = FoFEndMapRatio( pPlayer );
		if ( flRatio > awards.m_flBestRatio )
		{
			awards.m_flBestRatio = flRatio;
			awards.m_nBestRatioPlayer = i;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = FoFEndMapPlayerByIndex( i );
		if ( !FoFEndMapAwardEligible( pPlayer ) )
			continue;

		const float flAccuracy = pPlayer->GetFoFReportedAccuracy();
		if ( flAccuracy > awards.m_flBestAccuracy )
		{
			awards.m_flBestAccuracy = flAccuracy;
			awards.m_nBestAccuracyPlayer = i;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = FoFEndMapPlayerByIndex( i );
		if ( !FoFEndMapAwardEligible( pPlayer ) )
			continue;

		const int nKillstreak = pPlayer->GetFoFBestMultiKill();
		const bool bReplaceTie =
			nKillstreak == awards.m_nBestKillstreak &&
			( awards.m_nBestKillstreakPlayer ==
				awards.m_nBestRatioPlayer ||
			  awards.m_nBestKillstreakPlayer ==
				awards.m_nBestAccuracyPlayer );
		if ( nKillstreak > awards.m_nBestKillstreak || bReplaceTie )
		{
			awards.m_nBestKillstreak = nKillstreak;
			awards.m_nBestKillstreakPlayer = i;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = FoFEndMapPlayerByIndex( i );
		if ( !FoFEndMapAwardEligible( pPlayer ) )
			continue;

		const int nDrunkard = pPlayer->GetFoFBestDrunkard();
		const bool bReplaceTie =
			nDrunkard == awards.m_nBestDrunkard &&
			( awards.m_nBestDrunkardPlayer ==
				awards.m_nBestRatioPlayer ||
			  awards.m_nBestDrunkardPlayer ==
				awards.m_nBestKillstreakPlayer ||
			  awards.m_nBestDrunkardPlayer ==
				awards.m_nBestAccuracyPlayer );
		if ( nDrunkard > awards.m_nBestDrunkard || bReplaceTie )
		{
			awards.m_nBestDrunkard = nDrunkard;
			awards.m_nBestDrunkardPlayer = i;
		}
	}
}

static void FoFSendEndMapAwards(
	CFoF_Player *pPlayer, const FoFEndMapAwards_t &awards )
{
	if ( !pPlayer || pPlayer->IsFakeClient() )
		return;

	const int nPlayerIndex = pPlayer->entindex();
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "GoodBadYou" );
		WRITE_FLOAT( awards.m_flBestRatio );
		WRITE_LONG( awards.m_nBestRatioPlayer );
		WRITE_LONG( awards.m_nBestKillstreak );
		WRITE_LONG( awards.m_nBestKillstreakPlayer );
		WRITE_FLOAT( awards.m_flBestAccuracy );
		WRITE_LONG( awards.m_nBestAccuracyPlayer );
		WRITE_LONG( awards.m_nBestDrunkard );
		WRITE_LONG( awards.m_nBestDrunkardPlayer );
		WRITE_FLOAT( nPlayerIndex != awards.m_nBestRatioPlayer ?
			FoFEndMapRatio( pPlayer ) : 0.0f );
		WRITE_FLOAT( nPlayerIndex != awards.m_nBestKillstreakPlayer ?
			static_cast< float >( pPlayer->GetFoFBestMultiKill() ) : 0.0f );
		WRITE_FLOAT( nPlayerIndex != awards.m_nBestAccuracyPlayer ?
			pPlayer->GetFoFReportedAccuracy() : 0.0f );
		WRITE_FLOAT( nPlayerIndex != awards.m_nBestDrunkardPlayer ?
			static_cast< float >( pPlayer->GetFoFBestDrunkard() ) : 0.0f );
	MessageEnd();
}

void FoFPresentEndMapStatistics( void )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "game_end" ) : NULL;
	if ( pEvent )
		gameeventmanager->FireEvent( pEvent );

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = FoFEndMapPlayerByIndex( i );
		if ( pPlayer )
			pPlayer->CommitFoFMapStatistics( true );
	}

	FoFEndMapAwards_t awards;
	FoFFindEndMapAwards( awards );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		FoFSendEndMapAwards( FoFEndMapPlayerByIndex( i ), awards );
}
