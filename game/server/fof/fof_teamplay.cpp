//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF map-driven teamplay round controller.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_teamplay.h"
#include "fof/fof_player.h"
#include "hl2mp_gamerules.h"
#include "recipientfilter.h"
#include "team.h"
#include "fof/fof_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CBaseEntity *g_pLastCombineSpawn;
extern CBaseEntity *g_pLastRebelSpawn;

LINK_ENTITY_TO_CLASS( fof_teamplay, CTrigger_EndRound );

ConVar round_start_time(
	"round_start_time", "0", FCVAR_GAMEDLL | FCVAR_REPLICATED );
ConVar round_end_time(
	"round_end_time", "0", FCVAR_GAMEDLL | FCVAR_REPLICATED );

static bool FoFTeamplayIsActive()
{
	CHL2MPRules *pRules = HL2MPRules();
	return pRules && pRules->IsFoFTeamplayRoundActive();
}

static void FoFFireSimpleEvent( const char *pszName )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( pszName ) : NULL;
	if ( pEvent )
		gameeventmanager->FireEvent( pEvent );
}

static void FoFFireTeamValueEvent(
	const char *pszName, int nTeam,
	const char *pszValueName, int nValue )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( pszName ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetInt( "team", nTeam );
	pEvent->SetInt( pszValueName, nValue );
	gameeventmanager->FireEvent( pEvent );
}

static int FoFTeamRoundsWon( int nTeam )
{
	CTeam *pTeam = GetGlobalTeam( nTeam );
	return pTeam ? pTeam->GetRoundsWon() : 0;
}

static int FoFTeamPlayerCount( int nTeam, bool bAliveOnly )
{
	int nCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() != nTeam ||
			( bAliveOnly && !pPlayer->IsAlive() ) )
		{
			continue;
		}
		++nCount;
	}
	return nCount;
}

static int FoFTeamBalanceScore( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return -1000;

	int nTeamExperience = 0;
	int nTeamPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pTeamPlayer =
			ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pTeamPlayer || !pTeamPlayer->IsConnected() ||
			pTeamPlayer->GetTeamNumber() != pPlayer->GetTeamNumber() )
		{
			continue;
		}

		nTeamExperience += pTeamPlayer->GetFoFExperience();
		++nTeamPlayers;
	}

	nTeamExperience = clamp( nTeamExperience, 1, 10000 );
	nTeamPlayers = clamp( nTeamPlayers, 1, 10000 );
	const float flTeamExperienceCap =
		static_cast< float >( nTeamExperience ) /
		static_cast< float >( nTeamPlayers ) * 1.5f;
	const float flExperience = MIN(
		MAX( static_cast< float >(
			pPlayer->GetFoFExperience() -
			pPlayer->GetFoFTotalNotoriety() ), 0.0f ),
		flTeamExperienceCap );
	const int nDeaths = clamp( pPlayer->DeathCount(), 5, 1000 );
	const float flAccuracy = clamp(
		pPlayer->GetFoFReportedAccuracy(), 0.25f, 0.85f );
	const float flDenominator = 10.0f -
		static_cast< float >( pPlayer->FragCount() ) /
		static_cast< float >( nDeaths );
	return static_cast< int >(
		flExperience / flDenominator * flAccuracy );
}

static CFoF_Player *FoFSelectTeamBalancePlayer( int nTeam )
{
	CFoF_Player *pBestWithoutFriend = NULL;
	CFoF_Player *pBestPlayer = NULL;
	int nBestWithoutFriendScore = -1000;
	int nBestScore = -1000;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() != nTeam )
		{
			continue;
		}

		const int nScore = FoFTeamBalanceScore( pPlayer );
		if ( nScore > nBestScore )
		{
			nBestScore = nScore;
			pBestPlayer = pPlayer;
		}
		if ( !pPlayer->HasFoFFriendOnTeam() &&
			nScore > nBestWithoutFriendScore )
		{
			nBestWithoutFriendScore = nScore;
			pBestWithoutFriend = pPlayer;
		}
	}

	return pBestWithoutFriend ? pBestWithoutFriend : pBestPlayer;
}

static void FoFSendTeamplayNotice(
	CFoF_Player *pPlayer, const char *pszToken )
{
	if ( !pPlayer || !pszToken || !pszToken[0] )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
	MessageEnd();
}

struct FoFTeamScrambleEntry_t
{
	CFoF_Player *m_pPlayer;
	int m_nScore;
};

static int FoFCompareTeamScrambleEntries(
	const FoFTeamScrambleEntry_t *pFirst,
	const FoFTeamScrambleEntry_t *pSecond )
{
	if ( pFirst->m_nScore != pSecond->m_nScore )
		return pSecond->m_nScore - pFirst->m_nScore;
	return pFirst->m_pPlayer->entindex() - pSecond->m_pPlayer->entindex();
}

static int FoFGetDominantTeam( float flThreshold )
{
	CTeam *pVigilantes = GetGlobalTeam( TEAM_COMBINE );
	CTeam *pDesperados = GetGlobalTeam( TEAM_REBELS );
	if ( !pVigilantes || !pDesperados )
		return 0;

	const int nPlayers = pVigilantes->GetNumPlayers() +
		pDesperados->GetNumPlayers();
	const int nVigScore = MAX( pVigilantes->GetScore(), 0 );
	const int nDespScore = MAX( pDesperados->GetScore(), 0 );
	const int nTotalScore = nVigScore + nDespScore;
	if ( nPlayers < 8 || nTotalScore <= 0 )
		return 0;

	const float flVigShare = static_cast< float >( nVigScore ) /
		static_cast< float >( nTotalScore );
	const float flDespShare = static_cast< float >( nDespScore ) /
		static_cast< float >( nTotalScore );
	if ( flVigShare > flThreshold )
		return TEAM_COMBINE;
	if ( flDespShare > flThreshold )
		return TEAM_REBELS;
	return 0;
}

static void FoFScrambleTeams()
{
	CUtlVector< FoFTeamScrambleEntry_t > entries;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;
		if ( pPlayer->IsFoFBotGhost() )
		{
			engine->ServerCommand( UTIL_VarArgs(
				"kickid %d\n", pPlayer->GetUserID() ) );
			continue;
		}
		if ( pPlayer->GetTeamNumber() != TEAM_COMBINE &&
			pPlayer->GetTeamNumber() != TEAM_REBELS )
		{
			continue;
		}

		FoFTeamScrambleEntry_t entry;
		entry.m_pPlayer = pPlayer;
		entry.m_nScore = MAX( FoFTeamBalanceScore( pPlayer ), 0 );
		entries.AddToTail( entry );
	}
	if ( entries.Count() < 2 )
		return;

	entries.Sort( FoFCompareTeamScrambleEntries );
	CTeam *pVigilantes = GetGlobalTeam( TEAM_COMBINE );
	CTeam *pDesperados = GetGlobalTeam( TEAM_REBELS );
	const int nVigScore = pVigilantes ? pVigilantes->GetScore() : 0;
	const int nDespScore = pDesperados ? pDesperados->GetScore() : 0;
	int nCurrentTeam = nVigScore <= nDespScore ? TEAM_REBELS : TEAM_COMBINE;
	float flAssignedScore[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int nAssignedPlayers[4] = { 0, 0, 0, 0 };

	for ( int i = 0; i < entries.Count(); ++i )
	{
		FoFTeamScrambleEntry_t &entry = entries[i];
		if ( entry.m_pPlayer->GetTeamNumber() != nCurrentTeam )
		{
			entry.m_pPlayer->ChangeTeam(
				nCurrentTeam, true, false, true );
		}
		FoFSendTeamplayNotice( entry.m_pPlayer, "#TeamsScrambled" );
		flAssignedScore[nCurrentTeam] += entry.m_nScore;
		++nAssignedPlayers[nCurrentTeam];

		const int nOtherTeam =
			nCurrentTeam == TEAM_COMBINE ? TEAM_REBELS : TEAM_COMBINE;
		if ( i < entries.Count() / 2 )
		{
			if ( flAssignedScore[nOtherTeam] <=
				flAssignedScore[nCurrentTeam] )
			{
				nCurrentTeam = nOtherTeam;
			}
		}
		else if ( nAssignedPlayers[nOtherTeam] <=
			nAssignedPlayers[nCurrentTeam] &&
			nAssignedPlayers[nOtherTeam] != nAssignedPlayers[nCurrentTeam] )
		{
			nCurrentTeam = nOtherTeam;
		}
	}

	if ( pVigilantes )
	{
		pVigilantes->SetRoundsWon( 0 );
		pVigilantes->SetScore( 0 );
	}
	if ( pDesperados )
	{
		pDesperados->SetRoundsWon( 0 );
		pDesperados->SetScore( 0 );
	}
}

BEGIN_DATADESC( CTrigger_EndRound )
	DEFINE_KEYFIELD( m_nRespawnSystem, FIELD_INTEGER, "RespawnSystem" ),
	DEFINE_KEYFIELD( m_bRoundBased, FIELD_BOOLEAN, "RoundBased" ),
	DEFINE_KEYFIELD( m_bSwitchTeams, FIELD_BOOLEAN, "SwitchTeams" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "InputTie", InputTie ),
	DEFINE_INPUTFUNC( FIELD_VOID, "InputVigVictory", InputVigVictory ),
	DEFINE_INPUTFUNC( FIELD_VOID, "InputDespVictory", InputDespVictory ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DeclareWinner", InputVictory_DeclareWinner ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "InputRespawnPlayers", InputRespawnPlayers ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "InputTeleportToRespawn", InputTeleportToRespawn ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "ExtraTime", ExtraTime ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "RoundTime", RoundTime ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetVigObjective", InputSetVigObjective ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetDespObjective", InputSetDespObjective ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetVigReward", InputSetVigReward ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetDespReward", InputSetDespReward ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetVigScore", InputSetVigScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetDespScore", InputSetDespScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddVigScore", InputAddVigScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddDespScore", InputAddDespScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddCash_AllPlayers", InputAddCash_All ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddCash_Vigilantes", InputAddCash_Vigilantes ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddCash_Desperados", InputAddCash_Desperados ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AddCash_Auto", InputAddCash_Auto ),
	DEFINE_INPUTFUNC( FIELD_VOID, "InputResetScores", InputResetScores ),
	DEFINE_INPUTFUNC( FIELD_STRING, "InputDisplayMsg", InputDisplayMsg ),
	DEFINE_INPUTFUNC( FIELD_VOID, "InputHealAndReload", InputHealAndReload ),

	DEFINE_OUTPUT( m_OnNoDespAlive, "OnNoDespAlive" ),
	DEFINE_OUTPUT( m_OnNoVigAlive, "OnNoVigAlive" ),
	DEFINE_OUTPUT( m_OnTimerEnd, "OnTimerEnd" ),
	DEFINE_OUTPUT( m_OnNewRound, "OnNewRound" ),
	DEFINE_OUTPUT( m_OnNewBuyRound, "OnNewBuyRound" ),
	DEFINE_OUTPUT( m_OnRoundEnd, "OnRoundEnd" ),
	DEFINE_OUTPUT( m_OnRoundTimeEnd, "OnRoundTimeEnd" ),
	DEFINE_OUTPUT( m_OnRoundOdd, "OnRoundOdd" ),
	DEFINE_OUTPUT( m_OnRoundEven, "OnRoundEven" ),
	DEFINE_OUTPUT( m_OnNewRoundRemove, "OnNewRoundRemove" ),
END_DATADESC()

#if !defined( _DEBUG ) && defined( _M_IX86 )
COMPILE_TIME_ASSERT( sizeof( CTrigger_EndRound ) == 0x490 );
#endif

CTrigger_EndRound::CTrigger_EndRound()
{
	Q_memset( m_nTeamObjective, 0, sizeof( m_nTeamObjective ) );
	Q_memset( m_nTeamReward, 0, sizeof( m_nTeamReward ) );
	m_bRoundBased = false;
	m_flRoundLimitTime = 0.0f;
	m_nRespawnSystem = 0;
	m_flExtraTimeEnd = 0.0f;
	m_bSwitchTeamsPending = false;
	m_bSwitchTeams = false;
	m_flDelayedRespawnTime = 0.0f;
	m_flRoundStartTime = 0.0f;
	m_nRoundTime = 0;
	m_nRoundNumber = 0;
	m_nPendingWinner = 0;
	m_bAutoCash = false;
}

void CTrigger_EndRound::Spawn()
{
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_connect_fof" );
	m_bSwitchTeamsPending = false;
	m_nPendingWinner = 0;
}

void CTrigger_EndRound::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent )
		return;

	const char *pszName = pEvent->GetName();
	if ( !Q_stricmp( pszName, "player_connect_fof" ) )
	{
		if ( FoFTeamplayIsActive() )
			SendCurrentState();
		return;
	}

	if ( !Q_stricmp( pszName, "player_death" ) )
	{
		CFoF_Player *pPlayer = ToFoFPlayer(
			UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
		if ( !pPlayer )
			return;

		const int nDeadTeam = pPlayer->GetTeamNumber();
		if ( m_nTeamObjective[2] == 1 && nDeadTeam == 3 )
			UpdateEliminationObjectiveScore( 2, nDeadTeam );
		if ( m_nTeamObjective[3] == 1 && nDeadTeam == 2 )
			UpdateEliminationObjectiveScore( 3, nDeadTeam );
		return;
	}

	if ( Q_stricmp( pszName, "round_start" ) )
		return;

	++m_nRoundNumber;
	m_flExtraTimeEnd = 0.0f;
	m_nRoundTime = 0;
	m_flRoundStartTime = gpGlobals->curtime;
	round_start_time.SetValue( m_flRoundStartTime );
	round_end_time.SetValue( m_flRoundStartTime );
	if ( m_nRoundNumber & 1 )
		m_OnRoundOdd.FireOutput( this, this );
	else
		m_OnRoundEven.FireOutput( this, this );

	static ConVarRef optionalOutput( "fof_sv_optional_output", true );
	if ( optionalOutput.IsValid() && optionalOutput.GetBool() )
		m_OnNewRoundRemove.FireOutput( this, this );
}

int CTrigger_EndRound::GetTeamReward( int nTeam ) const
{
	return nTeam >= 0 && nTeam < ARRAYSIZE( m_nTeamReward ) ?
		m_nTeamReward[nTeam] : 0;
}

int CTrigger_EndRound::GetTeamObjective( int nTeam ) const
{
	return nTeam >= 0 && nTeam < ARRAYSIZE( m_nTeamObjective ) ?
		m_nTeamObjective[nTeam] : 0;
}

float CTrigger_EndRound::GetExtraTimeRemaining() const
{
	if ( !FoFTeamplayIsActive() || m_flExtraTimeEnd == 0.0f )
		return 0.0f;
	return m_flExtraTimeEnd - gpGlobals->curtime;
}

float CTrigger_EndRound::GetRoundTimeEnd() const
{
	return m_nRoundTime > 0 ?
		m_flRoundStartTime + static_cast< float >( m_nRoundTime ) : 0.0f;
}

void CTrigger_EndRound::ResetRoundState()
{
	m_flExtraTimeEnd = 0.0f;
	m_nRoundTime = 0;
	CHL2MPRules *pRules = HL2MPRules();
	const float flMinutes = pRules ?
		pRules->GetFoFTimeLimitMinutes() : 0.0f;
	m_flRoundLimitTime = gpGlobals->curtime + flMinutes * 60.0f;
}

void CTrigger_EndRound::ApplyRespawnSystem()
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( pRules )
		pRules->ApplyFoFTeamplayRespawnSystem( m_nRespawnSystem );
}

void CTrigger_EndRound::ShowTeamplayDialog()
{
	if ( m_bRoundBased && m_nRespawnSystem == 1 )
		FoFFireSimpleEvent( "tp_dlg_show" );
}

void CTrigger_EndRound::FireNewRoundOutput()
{
	m_OnNewRound.FireOutput( this, this );
}

void CTrigger_EndRound::FireNewBuyRoundOutput()
{
	m_OnNewBuyRound.FireOutput( this, this );
}

void CTrigger_EndRound::FireRoundTimeEndOutput()
{
	m_OnRoundTimeEnd.FireOutput( this, this );
}

void CTrigger_EndRound::FireTimerEndOutput()
{
	m_OnTimerEnd.FireOutput( this, this );
}

void CTrigger_EndRound::FireNoDespAliveOutput()
{
	m_OnNoDespAlive.FireOutput( this, this );
}

void CTrigger_EndRound::FireNoVigAliveOutput()
{
	m_OnNoVigAlive.FireOutput( this, this );
}

void CTrigger_EndRound::ClearTeamStateForNewRound()
{
	Q_memset( m_nTeamObjective, 0, sizeof( m_nTeamObjective ) );
	Q_memset( m_nTeamReward, 0, sizeof( m_nTeamReward ) );
}

void CTrigger_EndRound::PrepareTeamsForNewRound()
{
	if ( m_bSwitchTeamsPending )
	{
		// The shipped implementation uses team 0 as a temporary bucket so a
		// player can never be selected twice while Vigilantes and Desperados
		// are exchanged.
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() &&
				pPlayer->GetTeamNumber() == TEAM_REBELS )
			{
				pPlayer->ChangeTeam( TEAM_UNASSIGNED, true, false, true );
			}
		}
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() &&
				pPlayer->GetTeamNumber() == TEAM_COMBINE )
			{
				pPlayer->ChangeTeam( TEAM_REBELS, true, false, true );
			}
		}
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() &&
				pPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
			{
				pPlayer->ChangeTeam( TEAM_COMBINE, true, false, true );
			}
		}

		const int nVigScore = FoFTeamRoundsWon( TEAM_COMBINE );
		const int nDespScore = FoFTeamRoundsWon( TEAM_REBELS );
		SetTeamRoundScore( TEAM_COMBINE, nDespScore );
		SetTeamRoundScore( TEAM_REBELS, nVigScore );
		m_bSwitchTeamsPending = false;
	}

	BalanceTeams();
}

void CTrigger_EndRound::ProcessDelayedRespawn()
{
	if ( m_flDelayedRespawnTime == 0.0f ||
		gpGlobals->curtime <= m_flDelayedRespawnTime )
	{
		return;
	}

	m_flDelayedRespawnTime = 0.0f;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() &&
			pPlayer->GetTeamNumber() != TEAM_SPECTATOR &&
			!pPlayer->IsAlive() )
		{
			pPlayer->FinalizeFoFSpawn( true );
		}
	}
}

void CTrigger_EndRound::DelayExtraTimeEnd( float flSeconds )
{
	if ( m_flExtraTimeEnd != 0.0f )
		m_flExtraTimeEnd += flSeconds;
}

void CTrigger_EndRound::SendCurrentState()
{
	FoFFireSimpleEvent( "tp_dlg_show" );
	for ( int nTeam = 2; nTeam <= 3; ++nTeam )
	{
		FoFFireTeamValueEvent(
			"tp_teamobj", nTeam, "obj", m_nTeamObjective[nTeam] );
		FoFFireTeamValueEvent(
			"tp_teamreward", nTeam, "reward", m_nTeamReward[nTeam] );
	}
}

void CTrigger_EndRound::SetTeamObjective(
	int nTeam, int nObjective )
{
	if ( nTeam < 0 || nTeam >= ARRAYSIZE( m_nTeamObjective ) )
		return;
	m_nTeamObjective[nTeam] = nObjective;
	FoFFireTeamValueEvent( "tp_teamobj", nTeam, "obj", nObjective );
}

void CTrigger_EndRound::SetTeamReward( int nTeam, int nReward )
{
	if ( nTeam < 0 || nTeam >= ARRAYSIZE( m_nTeamReward ) )
		return;
	m_nTeamReward[nTeam] = nReward;
	FoFFireTeamValueEvent( "tp_teamreward", nTeam, "reward", nReward );
	SetTeamRoundScore( nTeam, 0 );
}

void CTrigger_EndRound::SetTeamRoundScore( int nTeam, int nScore )
{
	CTeam *pTeam = GetGlobalTeam( nTeam );
	if ( pTeam )
		pTeam->SetRoundsWon( MAX( nScore, 0 ) );
}

void CTrigger_EndRound::AddCashToTeam( int nTeam, int nAmount )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() < 2 ||
			( nTeam >= 2 && pPlayer->GetTeamNumber() != nTeam ) )
		{
			continue;
		}
		pPlayer->AwardFoFCash(
			static_cast< float >( nAmount ), "#Cash_Added" );
	}
}

void CTrigger_EndRound::UpdateEliminationObjectiveScore(
	int nScoringTeam, int nDeadTeam )
{
	const int nPlayers = FoFTeamPlayerCount( nDeadTeam, false );
	const int nAlive = FoFTeamPlayerCount( nDeadTeam, true );
	if ( nPlayers <= 0 )
		return;

	float flProgress;
	if ( nScoringTeam == nDeadTeam )
	{
		flProgress = static_cast< float >( nAlive ) /
			static_cast< float >( nPlayers );
	}
	else
	{
		flProgress = 1.0f - static_cast< float >( nAlive - 1 ) /
			static_cast< float >( nPlayers );
	}
	flProgress = clamp( flProgress, 0.0f, 1.0f );
	SetTeamRoundScore( nScoringTeam,
		static_cast< int >( flProgress * m_nTeamReward[nScoringTeam] ) );
}

void CTrigger_EndRound::BalanceTeams()
{
	const int nVigilantes = FoFTeamPlayerCount( 2, false );
	const int nDesperados = FoFTeamPlayerCount( 3, false );
	const int nDominantTeam = FoFGetDominantTeam( 0.65f );
	if ( abs( nVigilantes - nDesperados ) <= 1 && nDominantTeam == 0 )
		return;

	const int nFrom = nDominantTeam != 0 ? nDominantTeam :
		( nVigilantes > nDesperados ? 2 : 3 );
	const int nTo = nFrom == 2 ? 3 : 2;
	CFoF_Player *pPlayer = FoFSelectTeamBalancePlayer( nFrom );
	if ( pPlayer )
	{
		pPlayer->ChangeTeam( nTo, true, false, true );
		FoFSendTeamplayNotice( pPlayer, "#TeamBalancing" );
	}

	static float s_flNextScrambleTime = 0.0f;
	static float s_flLastScrambleCheckTime = 0.0f;
	if ( gpGlobals->curtime < s_flLastScrambleCheckTime )
		s_flNextScrambleTime = 0.0f;
	s_flLastScrambleCheckTime = gpGlobals->curtime;

	static ConVarRef scrambleTeams( "fof_sv_scrambleteams", true );
	if ( scrambleTeams.IsValid() && scrambleTeams.GetBool() &&
		gpGlobals->curtime >= s_flNextScrambleTime &&
		FoFGetDominantTeam( 0.60f ) != 0 )
	{
		s_flNextScrambleTime = gpGlobals->curtime + 180.0f;
		FoFScrambleTeams();
	}
}

void CTrigger_EndRound::FinishRound( int nWinningTeam )
{
	static ConVarRef roundsPlayed( "fof_sv_roundsplayed", true );
	static ConVarRef maxRounds( "fof_sv_maxrounds", true );
	static ConVarRef winLimit( "fof_sv_winlimit", true );
	static ConVarRef switchSides( "fof_sv_tp_switchsides", true );
	if ( roundsPlayed.IsValid() )
		roundsPlayed.SetValue( roundsPlayed.GetInt() + 1 );

	int nVigScore = FoFTeamRoundsWon( 2 );
	int nDespScore = FoFTeamRoundsWon( 3 );
	if ( nVigScore == 0 && nWinningTeam == 2 )
		nVigScore = 1;
	if ( nDespScore == 0 && nWinningTeam == 3 )
		nDespScore = 1;
	SetTeamRoundScore( 2, nVigScore );
	SetTeamRoundScore( 3, nDespScore );

	bool bLastRound = false;
	if ( maxRounds.IsValid() && maxRounds.GetInt() > 0 &&
		roundsPlayed.IsValid() &&
		roundsPlayed.GetInt() >= maxRounds.GetInt() )
	{
		bLastRound = true;
	}
	if ( winLimit.IsValid() && winLimit.GetInt() > 0 &&
		( nVigScore >= winLimit.GetInt() ||
		  nDespScore >= winLimit.GetInt() ) )
	{
		if ( !switchSides.IsValid() || !switchSides.GetBool() ||
			m_nPendingWinner != 0 )
		{
			bLastRound = true;
		}
		else
		{
			m_bSwitchTeamsPending = true;
			m_nPendingWinner = nWinningTeam == 2 ? 3 : 2;
		}
	}
	if ( m_bSwitchTeams )
		m_bSwitchTeamsPending = true;

	if ( m_bSwitchTeamsPending && !bLastRound )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			FoFSendTeamplayNotice(
				ToFoFPlayer( UTIL_PlayerByIndex( i ) ),
				"#TP_SwitchSides_Notice" );
	}

	int nMVPIndex = 0;
	int nMVPScore = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			continue;
		}

		pPlayer->AwardFoFCaptureNotoriety();
		if ( pPlayer->GetTeamNumber() == nWinningTeam &&
			pPlayer->FragCount() > nMVPScore )
		{
			nMVPIndex = pPlayer->entindex();
			nMVPScore = pPlayer->FragCount();
		}
	}

	if ( m_bAutoCash )
		FoFFireSimpleEvent( "round_end_autocash" );
	FoFFireSimpleEvent( "tp_dlg_hide" );
	m_OnRoundEnd.FireOutput( this, this );

	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "round_end" ) : NULL;
	if ( pEvent )
	{
		pEvent->SetInt( "LScr", FoFTeamRoundsWon( 2 ) );
		pEvent->SetInt( "OScr", FoFTeamRoundsWon( 3 ) );
		pEvent->SetBool( "LRnd", bLastRound );
		pEvent->SetInt( "TWinner", nWinningTeam );
		pEvent->SetInt( "MVP_Index", nMVPIndex );
		pEvent->SetInt( "MVP_Score", nMVPScore );
		gameeventmanager->FireEvent( pEvent );
	}

	CHL2MPRules *pRules = HL2MPRules();
	if ( pRules )
		pRules->FinishFoFTeamplayRound( bLastRound );
}

void CTrigger_EndRound::InputTie( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	if ( FoFTeamplayIsActive() )
		FinishRound( 0 );
}

void CTrigger_EndRound::InputVigVictory( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	if ( !FoFTeamplayIsActive() )
		return;
	if ( m_nTeamObjective[2] == 2 )
		SetTeamRoundScore( 2, m_nTeamReward[2] );
	FinishRound( 2 );
}

void CTrigger_EndRound::InputDespVictory( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	if ( !FoFTeamplayIsActive() )
		return;
	if ( m_nTeamObjective[3] == 2 )
		SetTeamRoundScore( 3, m_nTeamReward[3] );
	FinishRound( 3 );
}

void CTrigger_EndRound::InputVictory_DeclareWinner(
	inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	if ( !FoFTeamplayIsActive() )
		return;

	const int nVigScore = FoFTeamRoundsWon( 2 );
	const int nDespScore = FoFTeamRoundsWon( 3 );
	int nWinner = nVigScore > nDespScore ? 2 :
		nDespScore > nVigScore ? 3 : 0;

	static ConVarRef aliveCheck( "fof_sv_teamplay_alivecheck", true );
	if ( aliveCheck.IsValid() && aliveCheck.GetBool() )
	{
		for ( int nTeam = 2; nTeam <= 3; ++nTeam )
		{
			const int nOtherTeam = nTeam == 2 ? 3 : 2;
			if ( m_nTeamObjective[nTeam] == 2 &&
				FoFTeamPlayerCount( nOtherTeam, true ) == 0 )
			{
				nWinner = nTeam;
				break;
			}
		}
		if ( nWinner >= 2 && m_nTeamObjective[nWinner] == 2 )
			SetTeamRoundScore( nWinner, m_nTeamReward[nWinner] );
	}
	FinishRound( nWinner );
}

void CTrigger_EndRound::InputRespawnPlayers( inputdata_t &inputData )
{
	if ( !FoFTeamplayIsActive() )
		return;

	const int nSelection = inputData.value.Int();
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
		BalanceTeams();

	if ( nSelection == -2 )
	{
		static ConVarRef respawnDelay( "fof_sv_respawntime", true );
		const float flDelay = respawnDelay.IsValid() ?
			respawnDelay.GetFloat() : 0.0f;
		if ( flDelay != 0.0f )
			m_flDelayedRespawnTime = gpGlobals->curtime + flDelay;
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			continue;
		}

		const bool bRespawn = nSelection == -1 ||
			( nSelection == -2 && m_flDelayedRespawnTime == 0.0f &&
			  !pPlayer->IsAlive() ) ||
			( nSelection == 0 && pPlayer->IsAlive() ) ||
			( nSelection > 0 && pPlayer->GetTeamNumber() == nSelection );
		if ( bRespawn )
		{
			pPlayer->ResetFoFHeavyLoadRespawnCycle();
			// The FoF respawn transaction selects the team spawn before Spawn.
			// The inherited ForceRespawn skips that step and falls back to spectator.
			pPlayer->FinalizeFoFSpawn( true );
		}
	}
}

void CTrigger_EndRound::InputTeleportToRespawn(
	inputdata_t &inputData )
{
	if ( !FoFTeamplayIsActive() )
		return;

	const int nTeam = inputData.value.Int();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() ||
			pPlayer->GetTeamNumber() != nTeam )
		{
			continue;
		}

		CBaseEntity *pSpawn = pPlayer->EntSelectSpawnPoint();
		if ( !pSpawn )
			continue;
		const Vector vecOrigin = pSpawn->GetAbsOrigin();
		const QAngle angAngles = pSpawn->GetAbsAngles();
		const Vector vecVelocity = vec3_origin;
		pPlayer->Teleport( &vecOrigin, &angAngles, &vecVelocity );
	}
}

void CTrigger_EndRound::ExtraTime( inputdata_t &inputData )
{
	if ( !FoFTeamplayIsActive() )
		return;

	const int nSeconds = clamp( inputData.value.Int(), 0, 120 );
	if ( m_flExtraTimeEnd == 0.0f )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() )
				pPlayer->EmitSound( "FoF.BountyObjective" );
		}
		m_flExtraTimeEnd = gpGlobals->curtime;
	}
	m_flExtraTimeEnd += static_cast< float >( nSeconds ) + 0.5f;

	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "tp_timer" ) : NULL;
	if ( pEvent )
	{
		pEvent->SetInt( "extra_time",
			static_cast< int >( m_flExtraTimeEnd ) );
		gameeventmanager->FireEvent( pEvent );
	}
	for ( int nTeam = 2; nTeam <= 3; ++nTeam )
	{
		FoFFireTeamValueEvent(
			"tp_teamobj", nTeam, "obj", m_nTeamObjective[nTeam] );
		FoFFireTeamValueEvent(
			"tp_teamreward", nTeam, "reward", m_nTeamReward[nTeam] );
	}
}

void CTrigger_EndRound::RoundTime( inputdata_t &inputData )
{
	if ( !FoFTeamplayIsActive() )
		return;
	m_nRoundTime += inputData.value.Int();
	round_end_time.SetValue(
		m_flRoundStartTime + static_cast< float >( m_nRoundTime ) );
}

void CTrigger_EndRound::InputSetVigObjective( inputdata_t &inputData )
{
	SetTeamObjective( 2, inputData.value.Int() );
}

void CTrigger_EndRound::InputSetDespObjective( inputdata_t &inputData )
{
	SetTeamObjective( 3, inputData.value.Int() );
}

void CTrigger_EndRound::InputSetVigReward( inputdata_t &inputData )
{
	SetTeamReward( 2, inputData.value.Int() );
}

void CTrigger_EndRound::InputSetDespReward( inputdata_t &inputData )
{
	SetTeamReward( 3, inputData.value.Int() );
}

void CTrigger_EndRound::InputSetVigScore( inputdata_t &inputData )
{
	SetTeamRoundScore( 2, inputData.value.Int() );
}

void CTrigger_EndRound::InputSetDespScore( inputdata_t &inputData )
{
	SetTeamRoundScore( 3, inputData.value.Int() );
}

void CTrigger_EndRound::InputAddVigScore( inputdata_t &inputData )
{
	if ( FoFTeamplayIsActive() )
		SetTeamRoundScore(
			2, FoFTeamRoundsWon( 2 ) + inputData.value.Int() );
}

void CTrigger_EndRound::InputAddDespScore( inputdata_t &inputData )
{
	if ( FoFTeamplayIsActive() )
		SetTeamRoundScore(
			3, FoFTeamRoundsWon( 3 ) + inputData.value.Int() );
}

void CTrigger_EndRound::InputAddCash_All( inputdata_t &inputData )
{
	AddCashToTeam( 0, inputData.value.Int() );
}

void CTrigger_EndRound::InputAddCash_Vigilantes(
	inputdata_t &inputData )
{
	AddCashToTeam( 2, inputData.value.Int() );
}

void CTrigger_EndRound::InputAddCash_Desperados(
	inputdata_t &inputData )
{
	AddCashToTeam( 3, inputData.value.Int() );
}

void CTrigger_EndRound::InputAddCash_Auto( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	m_bAutoCash = true;
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules )
		return;

	int nTeamPlayers[6] = { 0 };
	int nTeamNotoriety[6] = { 0 };
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;
		const int nTeam = pPlayer->GetTeamNumber();
		if ( nTeam >= 2 && nTeam < ARRAYSIZE( nTeamPlayers ) )
		{
			++nTeamPlayers[nTeam];
			nTeamNotoriety[nTeam] += pPlayer->GetFoFLastRoundNotoriety();
		}
	}

	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() < 2 ||
			pPlayer->GetTeamNumber() >= ARRAYSIZE( nTeamPlayers ) )
		{
			continue;
		}

		if ( pPlayer->GetFoFExperience() < 25 &&
			( !teamClasses.IsValid() || !teamClasses.GetBool() ) )
			engine->ClientCommand( pPlayer->edict(), "equipmenu\n" );
		const int nTeam = pPlayer->GetTeamNumber();
		const int nCash = pRules->GetFoFNotorietyPayout(
			nTeamNotoriety[nTeam], pPlayer->GetFoFLastRoundNotoriety(),
			nTeamPlayers[nTeam] );
		pPlayer->AwardFoFCash(
			static_cast< float >( nCash ), "#Cash_Added" );
	}
}

void CTrigger_EndRound::InputResetScores( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	for ( int nTeam = 2; nTeam <= 3; ++nTeam )
	{
		CTeam *pTeam = GetGlobalTeam( nTeam );
		if ( !pTeam )
			continue;
		pTeam->AddScore( pTeam->GetRoundsWon() );
		pTeam->SetRoundsWon( 0 );
	}

	CHL2MPRules *pRules = HL2MPRules();
	if ( pRules )
		pRules->RestartFoFTeamplayRound();
}

void CTrigger_EndRound::InputDisplayMsg( inputdata_t &inputData )
{
	const char *pszMessage = inputData.value.String();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		FoFSendTeamplayNotice(
			ToFoFPlayer( UTIL_PlayerByIndex( i ) ), pszMessage );
}

void CTrigger_EndRound::InputHealAndReload( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;
		pPlayer->SetHealth( 100 );
		for ( int nWeapon = 0; nWeapon < MAX_WEAPONS; ++nWeapon )
		{
			CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon( nWeapon );
			if ( pWeapon && pWeapon->UsesClipsForAmmo1() )
				pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		}
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF teamplay round-state integration.
//
//=============================================================================//

enum FoFTeamplayRoundState_t
{
	FOF_TP_STATE_IDLE = 0,
	FOF_TP_STATE_WARMUP = 1,
	FOF_TP_STATE_BUY = 2,
	FOF_TP_STATE_ACTIVE = 3,
	FOF_TP_STATE_REBUILD = 7,
	FOF_TP_STATE_INTERMISSION = 10,
};

static CTrigger_EndRound *FoFFindTeamplayController()
{
	CBaseEntity *pEntity = gEntList.FindEntityByClassname(
		NULL, "fof_teamplay" );
	return dynamic_cast< CTrigger_EndRound * >( pEntity );
}

static int FoFTeamplayPlayerCount( int nTeam, bool bAliveOnly )
{
	int nCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() != nTeam ||
			( bAliveOnly && !pPlayer->IsAlive() ) )
		{
			continue;
		}

		++nCount;
	}
	return nCount;
}

static void FoFSetTeamplayPlayerControlLock( bool bLocked )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			continue;
		}

		if ( bLocked )
			pPlayer->AddFlag( FL_ATCONTROLS );
		else
			pPlayer->RemoveFlag( FL_ATCONTROLS );
	}
}

static void FoFFreezeTeamplayPlayersAtRoundEnd()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() &&
			pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		{
			pPlayer->AddFlag( FL_FROZEN );
		}
	}
}

static void FoFFireTeamplayEvent( const char *pszName )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( pszName ) : NULL;
	if ( pEvent )
		gameeventmanager->FireEvent( pEvent );
}

static void FoFResetTeamplayRoundWorld( CHL2MPRules *pRules )
{
	g_pLastCombineSpawn = NULL;
	g_pLastRebelSpawn = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;

		engine->ClientCommand( pPlayer->edict(), "r_cleardecals\n" );
		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
		if ( pWeapon )
			pWeapon->Holster();
		pPlayer->RemoveAllItems( true );
	}

	// fof_teamplay is on s_PreserveEnts. The map-owned controller and its
	// accumulated round data survive while doors, breakables and objectives
	// are restored from the BSP entity lump.
	pRules->CleanUpMap();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() < TEAM_COMBINE )
		{
			continue;
		}

		pPlayer->FinalizeFoFSpawn( true );
	}
}

static void FoFPlayTeamplayBuyTick(
	float flBuyEndTime, float flNextStateUpdate )
{
	const char *pszSound = flNextStateUpdate + 1.1f <= flBuyEndTime ?
		"FoF.BuyTick" : "FoF.BuyTickEnd";
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() &&
			pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		{
			pPlayer->EmitSound( pszSound );
		}
	}
}

void CHL2MPRules::InitializeFoFTeamplayState()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 2 )
	{
		m_hFoFTeamplayController = NULL;
		m_nFoFTeamplayRoundState = FOF_TP_STATE_IDLE;
		return;
	}

	CTrigger_EndRound *pController = FoFFindTeamplayController();
	m_hFoFTeamplayController = pController;
	if ( !pController )
	{
		m_nFoFTeamplayRoundState = FOF_TP_STATE_IDLE;
		return;
	}

	m_nFoFTeamplayRespawnState = 2;
	m_flFoFTeamplayBuyEndTime = 0.0f;
	m_nFoFTeamplayLastBuyTick = -1;
	SetFoFTeamplayRoundState( FOF_TP_STATE_WARMUP, 0.1f );
}

void CHL2MPRules::SetFoFTeamplayRoundState(
	int nState, float flDelay )
{
	m_nFoFTeamplayRoundState = nState;
	m_flFoFTeamplayStateDeadline = gpGlobals->curtime +
		MAX( flDelay, 0.0f );
}

bool CHL2MPRules::IsFoFTeamplayRoundActive() const
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() && currentMode.GetInt() == 2 &&
		m_nFoFTeamplayRoundState == FOF_TP_STATE_ACTIVE;
}

bool CHL2MPRules::IsFoFTeamplayWarmup() const
{
	return m_nFoFTeamplayRoundState == FOF_TP_STATE_WARMUP;
}

void CHL2MPRules::ApplyFoFTeamplayRespawnSystem(
	int nRespawnSystem )
{
	// This is the original rules assignment.
	m_nFoFTeamplayRespawnState = 2 - ( nRespawnSystem != 0 );
}

bool CHL2MPRules::IsFoFTeamplayRespawnAllowed() const
{
	return m_nFoFTeamplayRoundState != FOF_TP_STATE_ACTIVE ||
		m_nFoFTeamplayRespawnState != 1;
}

void CHL2MPRules::FinishFoFTeamplayRound( bool bLastRound )
{
	SetFoFTeamplayRoundState(
		bLastRound ? FOF_TP_STATE_INTERMISSION : FOF_TP_STATE_REBUILD,
		9.0f );
	FoFFreezeTeamplayPlayersAtRoundEnd();
}

void CHL2MPRules::RestartFoFTeamplayRound()
{
	SetFoFTeamplayRoundState( FOF_TP_STATE_REBUILD, 0.1f );
}

void CHL2MPRules::UpdateFoFTeamplayState()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 2 )
		return;

	static ConVarRef timeLimit( "mp_timelimit", true );
	if ( timeLimit.IsValid() )
		flTimeLimit = timeLimit.GetFloat();

	CTrigger_EndRound *pController = dynamic_cast< CTrigger_EndRound * >(
		m_hFoFTeamplayController.Get() );
	if ( !pController )
	{
		pController = FoFFindTeamplayController();
		m_hFoFTeamplayController = pController;
	}
	if ( !pController )
		return;

	if ( gpGlobals->curtime < m_flFoFTeamplayStateDeadline )
		return;

	switch ( m_nFoFTeamplayRoundState )
	{
	case FOF_TP_STATE_WARMUP:
	{
		static ConVarRef warmup( "fof_warmup", true );
		if ( warmup.IsValid() && warmup.GetBool() )
		{
			SetFoFTeamplayRoundState(
				FOF_TP_STATE_WARMUP, 0.1f );
			return;
		}

		static ConVarRef mapStartTime( "map_start_time", true );
		if ( mapStartTime.IsValid() )
			mapStartTime.SetValue( gpGlobals->curtime );
		m_flGameStartTime = gpGlobals->curtime;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() )
			{
				pPlayer->ResetScores();
				pPlayer->m_flFoFCash = 0.0f;
			}
		}
		pController->ResetRoundState();
		SetFoFTeamplayRoundState( FOF_TP_STATE_REBUILD, 0.1f );
		return;
	}

	case FOF_TP_STATE_REBUILD:
	{
		if ( FoFTeamplayPlayerCount( TEAM_COMBINE, false ) == 0 &&
			FoFTeamplayPlayerCount( TEAM_REBELS, false ) == 0 )
		{
			SetFoFTeamplayRoundState(
				FOF_TP_STATE_REBUILD, 1.0f );
			return;
		}

		pController->PrepareTeamsForNewRound();
		FoFResetTeamplayRoundWorld( this );
		pController = dynamic_cast< CTrigger_EndRound * >(
			m_hFoFTeamplayController.Get() );
		if ( !pController )
		{
			pController = FoFFindTeamplayController();
			m_hFoFTeamplayController = pController;
		}
		if ( !pController )
			return;

		pController->ApplyRespawnSystem();
		FoFFireTeamplayEvent( "round_start" );
		pController->ClearTeamStateForNewRound();

		static ConVarRef buyTime( "fof_sv_tp_buytime", true );
		const float flBuyTime = buyTime.IsValid() ?
			MAX( buyTime.GetFloat(), 0.1f ) : 5.0f;
		m_flFoFTeamplayBuyEndTime = gpGlobals->curtime + flBuyTime;
		m_nFoFTeamplayLastBuyTick = -1;
		FoFSetTeamplayPlayerControlLock( true );
		pController->FireNewBuyRoundOutput();
		SetFoFTeamplayRoundState( FOF_TP_STATE_BUY, 0.1f );
		return;
	}

	case FOF_TP_STATE_BUY:
		if ( gpGlobals->curtime < m_flFoFTeamplayBuyEndTime )
		{
			FoFPlayTeamplayBuyTick(
				m_flFoFTeamplayBuyEndTime,
				m_flFoFTeamplayStateDeadline );
			SetFoFTeamplayRoundState(
				FOF_TP_STATE_BUY, 1.0f );
			return;
		}

		FoFSetTeamplayPlayerControlLock( false );
		m_flFoFTeamplayBuyEndTime = 0.0f;
		pController->FireNewRoundOutput();
		pController->ShowTeamplayDialog();
		m_nFoFTeamplayRoundState = FOF_TP_STATE_ACTIVE;
		if ( pController->IsRoundBased() )
		{
			m_flFoFTeamplayStateDeadline =
				gpGlobals->curtime + 0.2f;
		}
		else
		{
			m_flFoFTeamplayStateDeadline = MAX(
				pController->GetRoundLimitTime(),
				gpGlobals->curtime + 0.2f );
		}
		return;

	case FOF_TP_STATE_ACTIVE:
	{
		pController->ProcessDelayedRespawn();
		if ( !pController->IsRoundBased() )
		{
			SetFoFTeamplayRoundState(
				FOF_TP_STATE_INTERMISSION, 0.1f );
			return;
		}

		const int nVigPlayers =
			FoFTeamplayPlayerCount( TEAM_COMBINE, false );
		const int nDespPlayers =
			FoFTeamplayPlayerCount( TEAM_REBELS, false );
		if ( nVigPlayers == 0 && nDespPlayers == 0 )
		{
			SetFoFTeamplayRoundState(
				FOF_TP_STATE_REBUILD, 0.1f );
			return;
		}

		if ( ( nVigPlayers == 0 ) != ( nDespPlayers == 0 ) )
			pController->DelayExtraTimeEnd( 1.0f );
		else
		{
			const float flRoundEnd = pController->GetRoundTimeEnd();
			const float flExtraRemaining =
				pController->GetExtraTimeRemaining();
			if ( flRoundEnd > 0.0f && gpGlobals->curtime > flRoundEnd &&
				flExtraRemaining < 2.0f )
			{
				pController->FireRoundTimeEndOutput();
			}

			if ( pController->GetExtraTimeRemaining() < 0.0f )
				pController->FireTimerEndOutput();

			if ( pController->GetRespawnSystem() == 1 &&
				nVigPlayers > 0 && nDespPlayers > 0 )
			{
				if ( FoFTeamplayPlayerCount( TEAM_REBELS, true ) == 0 )
					pController->FireNoDespAliveOutput();
				if ( FoFTeamplayPlayerCount( TEAM_COMBINE, true ) == 0 )
					pController->FireNoVigAliveOutput();
			}
		}

		SetFoFTeamplayRoundState( FOF_TP_STATE_ACTIVE, 1.0f );
		return;
	}

	case FOF_TP_STATE_INTERMISSION:
		m_nFoFTeamplayRoundState = FOF_TP_STATE_IDLE;
		m_flFoFTeamplayStateDeadline =
			gpGlobals->curtime + 1000.0f;
		GoToIntermission();
		FoFFireTeamplayEvent( "tp_dlg_hide_full" );
		return;

	default:
		return;
	}
}
