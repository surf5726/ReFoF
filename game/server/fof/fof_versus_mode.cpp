//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF Versus arena scheduler and round authority.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_versus_mode.h"

#include "fof/fof_player.h"
#include "fof/fof_bot.h"
#include "fof/fof_player_equipment.h"
#include "hl2mp_gamerules.h"
#include "nav_mesh.h"
#include "recipientfilter.h"
#include "util.h"

#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_versus, CVersusMode );

static const int FOF_VERSUS_PHASE_WAITING = 0;
static const int FOF_VERSUS_PHASE_PREPARING = 1;
static const int FOF_VERSUS_PHASE_ACTIVE = 2;
static const int FOF_VERSUS_PHASE_RESULT = 3;

static const float FOF_VERSUS_FIRST_PREPARATION = 4.0f;
static const float FOF_VERSUS_NEXT_PREPARATION = 6.0f;
static const float FOF_VERSUS_RESULT_TIME = 4.0f;

BEGIN_DATADESC( CVersusMode )
	DEFINE_FIELD( m_nPhase, FIELD_INTEGER ),
	DEFINE_FIELD( m_nRoundNumber, FIELD_INTEGER ),
	DEFINE_FIELD( m_nArenaRotation, FIELD_INTEGER ),
	DEFINE_FIELD( m_flPhaseDeadline, FIELD_TIME ),
	DEFINE_FIELD( m_flMatchCycleDeadline, FIELD_TIME ),
	DEFINE_FIELD( m_flNextHudUpdate, FIELD_TIME ),
	DEFINE_FIELD( m_bWasWarmup, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC( ModeThink ),
END_DATADESC()

static int FoFVersusCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static bool FoFVersusWarmupActive()
{
	static ConVarRef warmup( "fof_warmup", true );
	return warmup.IsValid() && warmup.GetBool();
}

static int FoFVersusArenaCount()
{
	static ConVarRef arenas( "fof_sv_versus_arenas", true );
	return clamp( arenas.IsValid() ? arenas.GetInt() : 1, 1, 7 );
}

static int FoFVersusArenaCloneCount()
{
	static ConVarRef clones( "fof_sv_versus_arena_clones", true );
	return clamp( clones.IsValid() ? clones.GetInt() : 1, 1, 32 );
}

static float FoFVersusRoundTime()
{
	static ConVarRef roundTime( "fof_sv_versus_round_time", true );
	return MAX( roundTime.IsValid() ? roundTime.GetFloat() : 30.0f, 1.0f );
}

static bool FoFVersusSwitchSides()
{
	static ConVarRef switchSides( "fof_sv_versus_round_switch", true );
	return switchSides.IsValid() && switchSides.GetBool();
}

static bool FoFVersusTournamentEnabled()
{
	static ConVarRef tournament( "fof_sv_versus_tournament", true );
	return tournament.IsValid() && tournament.GetBool();
}

static int FoFVersusTournamentArena()
{
	static ConVarRef playArena(
		"fof_sv_versus_tournament_playarena", true );
	return clamp( playArena.IsValid() ? playArena.GetInt() : 3,
		1, FoFVersusArenaCount() );
}

static int FoFVersusRankPoints( CFoF_Player *pPlayer, CFoF_Player *pOpponent )
{
	if ( !pPlayer || !pOpponent || pPlayer->IsBot() || pOpponent->IsBot() )
		return 0;

	const float flRank = clamp(
		static_cast< float >( static_cast< int >( pPlayer->GetFoFGlobalRank() ) ),
		1.0f, 1000000.0f );
	return static_cast< int >(
		static_cast< int >( pOpponent->GetFoFGlobalRank() ) / flRank * 20.0f );
}

static bool FoFGetVersusArenaSpawns( int nArena, int nClone,
	CBaseEntity *&pSpawnA, CBaseEntity *&pSpawnB )
{
	char szSpawnA[64];
	char szSpawnB[64];
	Q_snprintf( szSpawnA, sizeof( szSpawnA ),
		"arena%d_a-%d", nArena, nClone );
	Q_snprintf( szSpawnB, sizeof( szSpawnB ),
		"arena%d_b-%d", nArena, nClone );
	pSpawnA = gEntList.FindEntityByName( NULL, szSpawnA );
	pSpawnB = gEntList.FindEntityByName( NULL, szSpawnB );
	return pSpawnA && pSpawnB;
}

static bool FoFVersusNextEquipmentToken(
	const char *&pCursor, char *pszToken, int nTokenSize )
{
	if ( !pCursor || !*pCursor || !pszToken || nTokenSize <= 0 )
		return false;

	int nWritten = 0;
	while ( *pCursor && *pCursor != ',' )
	{
		if ( nWritten + 1 < nTokenSize )
			pszToken[nWritten++] = *pCursor;
		++pCursor;
	}
	pszToken[nWritten] = '\0';
	if ( *pCursor == ',' )
		++pCursor;

	char *pStart = pszToken;
	while ( *pStart == ' ' || *pStart == '\t' ||
		*pStart == '\r' || *pStart == '\n' )
	{
		++pStart;
	}
	if ( pStart != pszToken )
		Q_memmove( pszToken, pStart, Q_strlen( pStart ) + 1 );

	int nLength = Q_strlen( pszToken );
	while ( nLength > 0 )
	{
		const char last = pszToken[nLength - 1];
		if ( last != ' ' && last != '\t' && last != '\r' && last != '\n' )
			break;
		pszToken[--nLength] = '\0';
	}
	return true;
}

CVersusMode::CVersusMode()
	: m_nPhase( FOF_VERSUS_PHASE_WAITING )
	, m_nRoundNumber( 1 )
	, m_nArenaRotation( 0 )
	, m_flPhaseDeadline( 0.0f )
	, m_flMatchCycleDeadline( 0.0f )
	, m_flNextHudUpdate( 0.0f )
	, m_bWasWarmup( false )
{
}

void CVersusMode::Precache()
{
	BaseClass::Precache();
	PrecacheScriptSound( "FoF.BountyObjective" );
}

void CVersusMode::Spawn()
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
	{
		Warning( "============ MAP HAS NO NAV MESH, CAN'T START VERSUS MODE ==============\n" );
		return;
	}

	Precache();
	BaseClass::Spawn();
	ListenForGameEvent( "player_connect_fof" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_disconnect" );
	ParseEquipmentConVars();
	ResetMode( true );
	m_bWasWarmup = FoFVersusWarmupActive();
	SetThink( &CVersusMode::ModeThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CVersusMode::UpdateOnRemove()
{
	ResetMode( true );
	StopListeningForAllEvents();
	BaseClass::UpdateOnRemove();
}

void CVersusMode::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent )
		return;

	const char *pszName = pEvent->GetName();
	if ( !Q_stricmp( pszName, "player_connect_fof" ) )
	{
		CFoF_Player *pPlayer = ToFoFPlayer(
			UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
		ClearPlayerVersusState( pPlayer );
		if ( m_nPhase == FOF_VERSUS_PHASE_WAITING )
			m_flPhaseDeadline = gpGlobals->curtime;
	}
	else if ( !Q_stricmp( pszName, "player_death" ) ||
		!Q_stricmp( pszName, "player_disconnect" ) )
	{
		SetNextThink( gpGlobals->curtime );
	}
}

void CVersusMode::ResetMode( bool bClearPlayers )
{
	if ( bClearPlayers )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			ClearPlayerVersusState(
				ToFoFPlayer( UTIL_PlayerByIndex( i ) ) );
		}
	}

	m_Matches.Purge();
	m_nPhase = FOF_VERSUS_PHASE_WAITING;
	m_nRoundNumber = 1;
	m_flPhaseDeadline = gpGlobals->curtime + 0.25f;
	m_flMatchCycleDeadline = 0.0f;
	m_flNextHudUpdate = gpGlobals->curtime;
}

void CVersusMode::ParseEquipmentConVars()
{
	m_Equipment.Purge();
	for ( int nArena = 1; nArena <= 7; ++nArena )
	{
		for ( int nSide = 1; nSide <= 2; ++nSide )
		{
			char szConVar[64];
			Q_snprintf( szConVar, sizeof( szConVar ),
				"fof_sv_versus_arena%d_items_%c",
				nArena, nSide == 1 ? 'a' : 'b' );
			ConVarRef items( szConVar, true );
			if ( items.IsValid() )
				ParseEquipmentList( nArena, nSide, items.GetString() );
		}
	}
}

void CVersusMode::ParseEquipmentList(
	int nArena, int nSide, const char *pszItems )
{
	if ( !pszItems || !pszItems[0] )
		return;

	const char *pCursor = pszItems;
	char szToken[64];
	while ( FoFVersusNextEquipmentToken(
		pCursor, szToken, sizeof( szToken ) ) )
	{
		if ( !szToken[0] )
			continue;

		const int nItem = FoFEquipmentItemId( szToken );
		if ( nItem <= 0 )
			continue;

		Equipment_t equipment;
		equipment.m_nArena = nArena;
		equipment.m_nItem = nItem;
		equipment.m_nSide = nSide;
		m_Equipment.AddToTail( equipment );
	}
}

bool CVersusMode::IsEligiblePlayer( CFoF_Player *pPlayer ) const
{
	return pPlayer && pPlayer->IsConnected() &&
		!pPlayer->IsHLTV() && !pPlayer->IsReplay() &&
		!pPlayer->IsFoFBotGhost() &&
		pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

bool CVersusMode::IsPlayerInMatch( CFoF_Player *pPlayer ) const
{
	if ( !pPlayer )
		return false;
	for ( int i = 0; i < m_Matches.Count(); ++i )
	{
		if ( m_Matches[i].m_hPlayerA.Get() == pPlayer ||
			m_Matches[i].m_hPlayerB.Get() == pPlayer )
		{
			return true;
		}
	}
	return false;
}

int CVersusMode::FindBestOpponent(
	const CUtlVector< CFoF_Player * > &players,
	CFoF_Player *pPlayer ) const
{
	if ( !pPlayer || players.Count() <= 0 )
		return -1;

	int nBest = 0;
	int nBestDistance = INT_MAX;
	const int nRank = static_cast< int >( pPlayer->GetFoFGlobalRank() );
	for ( int i = 0; i < players.Count(); ++i )
	{
		const int nDistance = abs(
			static_cast< int >( players[i]->GetFoFGlobalRank() ) - nRank );
		if ( nDistance < nBestDistance )
		{
			nBest = i;
			nBestDistance = nDistance;
		}
	}
	return nBest;
}

bool CVersusMode::FindArenaSpawns(
	int nSlot, int &nArena, int &nClone,
	CBaseEntity *&pSpawnA, CBaseEntity *&pSpawnB ) const
{
	pSpawnA = NULL;
	pSpawnB = NULL;
	const int nArenaCount = FoFVersusArenaCount();
	const int nCloneCount = FoFVersusArenaCloneCount();
	const bool bTournament = FoFVersusTournamentEnabled();
	const int nSlotCount = bTournament ?
		nCloneCount : nArenaCount * nCloneCount;

	for ( int nAttempt = 0; nAttempt < nSlotCount; ++nAttempt )
	{
		const int nLogicalSlot =
			( nSlot + m_nArenaRotation + nAttempt ) % nSlotCount;
		if ( bTournament )
		{
			nArena = FoFVersusTournamentArena();
			nClone = nLogicalSlot + 1;
		}
		else
		{
			nArena = nLogicalSlot % nArenaCount + 1;
			nClone = nLogicalSlot / nArenaCount + 1;
		}

		bool bAlreadyAssigned = false;
		for ( int i = 0; i < m_Matches.Count(); ++i )
		{
			if ( m_Matches[i].m_nArena == nArena &&
				m_Matches[i].m_nClone == nClone )
			{
				bAlreadyAssigned = true;
				break;
			}
		}
		if ( bAlreadyAssigned )
			continue;

		if ( FoFGetVersusArenaSpawns(
			nArena, nClone, pSpawnA, pSpawnB ) )
			return true;
	}

	return false;
}

bool CVersusMode::BuildMatches()
{
	m_Matches.Purge();
	CUtlVector< CFoF_Player * > players;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( IsEligiblePlayer( pPlayer ) )
			players.AddToTail( pPlayer );
	}

	if ( players.Count() < 1 )
		return false;

	const int nCapacity = FoFVersusTournamentEnabled() ?
		FoFVersusArenaCloneCount() :
		FoFVersusArenaCount() * FoFVersusArenaCloneCount();
	int nSlot = 0;
	while ( players.Count() >= 1 && m_Matches.Count() < nCapacity )
	{
		const int nFirst =
			( m_nArenaRotation + m_Matches.Count() ) % players.Count();
		CFoF_Player *pPlayerA = players[nFirst];
		players.FastRemove( nFirst );
		const int nOpponent = FindBestOpponent( players, pPlayerA );
		CFoF_Player *pPlayerB = NULL;
		if ( nOpponent >= 0 )
		{
			pPlayerB = players[nOpponent];
			players.FastRemove( nOpponent );
		}
		else if ( !FoFVersusTournamentEnabled() )
		{
			// Normal Versus supplies an opponent for an unmatched participant.
			pPlayerB = FoFPutBotInServer( false, 100 );
		}
		if ( !pPlayerB )
		{
			if ( FoFVersusTournamentEnabled() )
			{
				SendNotice( pPlayerA, "#VS_T_Finished" );
				pPlayerA->ChangeTeam(
					TEAM_SPECTATOR, false, false, true );
			}
			break;
		}

		int nArena = 0;
		int nClone = 0;
		CBaseEntity *pSpawnA = NULL;
		CBaseEntity *pSpawnB = NULL;
		if ( !FindArenaSpawns(
			nSlot++, nArena, nClone, pSpawnA, pSpawnB ) )
		{
			Warning( "Versus could not find an available arena spawn pair.\n" );
			break;
		}

		Match_t match;
		match.m_hPlayerA = pPlayerA;
		match.m_hPlayerB = pPlayerB;
		match.m_hSpawnA = pSpawnA;
		match.m_hSpawnB = pSpawnB;
		match.m_nArena = nArena;
		match.m_nClone = nClone;
		match.m_nWinsA = 0;
		match.m_nWinsB = 0;
		match.m_bRoundResolved = false;
		match.m_bMatchComplete = false;
		match.m_nPointsForA = FoFVersusRankPoints( pPlayerA, pPlayerB );
		match.m_nPointsForB = FoFVersusRankPoints( pPlayerB, pPlayerA );
		m_Matches.AddToTail( match );
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( IsEligiblePlayer( pPlayer ) && !IsPlayerInMatch( pPlayer ) )
		{
			pPlayer->SetFoFVersusSpawn( NULL );
			pPlayer->AddFlag( FL_ATCONTROLS );
		}
	}

	return m_Matches.Count() > 0;
}

int CVersusMode::GetTotalRounds() const
{
	static ConVarRef roundNumber( "fof_sv_versus_round_number", true );
	const int nRounds = MAX(
		roundNumber.IsValid() ? roundNumber.GetInt() : 1, 1 );
	return nRounds * ( FoFVersusSwitchSides() ? 2 : 1 );
}

void CVersusMode::EquipPlayer(
	CFoF_Player *pPlayer, int nArena, int nSide )
{
	if ( !IsEligiblePlayer( pPlayer ) )
		return;

	pPlayer->RemoveAllItems( true );
	FoFResetEquipmentState( pPlayer );
	FoFSendEquipmentItem( pPlayer, -1 );
	pPlayer->CBasePlayer::GiveAmmo( 100, "Buckshot", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "357", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "XBowBolt", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "Rifle", true );
	pPlayer->CBasePlayer::GiveAmmo( 100, "Rifle2", true );

	for ( int i = 0; i < m_Equipment.Count(); ++i )
	{
		const Equipment_t &equipment = m_Equipment[i];
		if ( equipment.m_nArena != nArena || equipment.m_nSide != nSide )
			continue;
		FoFApplyEquipmentItem( pPlayer, equipment.m_nItem );
		FoFSendEquipmentItem( pPlayer, equipment.m_nItem );
	}

	CBaseCombatWeapon *pFists =
		pPlayer->Weapon_OwnsThisType( "weapon_fists" );
	if ( !pFists )
	{
		pFists = dynamic_cast< CBaseCombatWeapon * >(
			pPlayer->GiveFoFNamedItem( "weapon_fists" ) );
	}
	if ( pFists )
		pPlayer->Weapon_Switch( pFists );
}

void CVersusMode::StartRound( float flPreparationTime )
{
	// Every arena advances together. Rebuild breakables and remove the previous
	// round's projectiles/dropped items before placing the next participants.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !IsEligiblePlayer( pPlayer ) )
			continue;
		pPlayer->PrepareFoFRoundRespawn();
	}
	if ( HL2MPRules() )
		HL2MPRules()->CleanUpMap();

	const bool bSwapSides = FoFVersusSwitchSides() &&
		( m_nRoundNumber & 1 ) == 0;
	for ( int i = 0; i < m_Matches.Count(); ++i )
	{
		Match_t &match = m_Matches[i];
		match.m_bRoundResolved = false;
		CBaseEntity *pSpawnA = NULL;
		CBaseEntity *pSpawnB = NULL;
		if ( !FoFGetVersusArenaSpawns(
			match.m_nArena, match.m_nClone, pSpawnA, pSpawnB ) )
		{
			Warning( "Versus arena spawn pair missing after round rebuild.\n" );
			match.m_bRoundResolved = true;
			continue;
		}
		match.m_hSpawnA = pSpawnA;
		match.m_hSpawnB = pSpawnB;
		CFoF_Player *pPlayerA = match.m_hPlayerA.Get();
		CFoF_Player *pPlayerB = match.m_hPlayerB.Get();
		if ( IsEligiblePlayer( pPlayerA ) )
		{
			pPlayerA->SetFoFVersusSpawn(
				bSwapSides ? match.m_hSpawnB.Get() : match.m_hSpawnA.Get() );
			if ( pPlayerA->FinalizeFoFSpawn( true ) )
			{
				EquipPlayer( pPlayerA, match.m_nArena, bSwapSides ? 2 : 1 );
				pPlayerA->SetAbsVelocity( vec3_origin );
				pPlayerA->AddFlag( FL_ATCONTROLS );
			}
		}
		if ( IsEligiblePlayer( pPlayerB ) )
		{
			pPlayerB->SetFoFVersusSpawn(
				bSwapSides ? match.m_hSpawnA.Get() : match.m_hSpawnB.Get() );
			if ( pPlayerB->FinalizeFoFSpawn( true ) )
			{
				EquipPlayer( pPlayerB, match.m_nArena, bSwapSides ? 1 : 2 );
				pPlayerB->SetAbsVelocity( vec3_origin );
				pPlayerB->AddFlag( FL_ATCONTROLS );
			}
		}
		SendMatchPresentation( match );
	}

	m_nPhase = FOF_VERSUS_PHASE_PREPARING;
	m_flPhaseDeadline = gpGlobals->curtime + flPreparationTime;
	if ( m_nRoundNumber == 1 )
	{
		m_flMatchCycleDeadline = gpGlobals->curtime +
			( FoFVersusRoundTime() + FOF_VERSUS_FIRST_PREPARATION ) *
			GetTotalRounds();
	}
	m_flNextHudUpdate = gpGlobals->curtime;
}

void CVersusMode::StartActiveRound()
{
	for ( int i = 0; i < m_Matches.Count(); ++i )
	{
		CFoF_Player *pPlayers[2] =
		{
			m_Matches[i].m_hPlayerA.Get(),
			m_Matches[i].m_hPlayerB.Get(),
		};
		for ( int nPlayer = 0; nPlayer < 2; ++nPlayer )
		{
			if ( !IsEligiblePlayer( pPlayers[nPlayer] ) )
				continue;
			pPlayers[nPlayer]->RemoveFlag( FL_ATCONTROLS );
			pPlayers[nPlayer]->EmitSound( "FoF.BountyObjective" );
		}
	}

	m_nPhase = FOF_VERSUS_PHASE_ACTIVE;
	m_flPhaseDeadline = gpGlobals->curtime + FoFVersusRoundTime();
	m_flNextHudUpdate = gpGlobals->curtime;
}

void CVersusMode::ResolveMatchRound( Match_t &match, bool bTimedOut )
{
	if ( match.m_bRoundResolved )
		return;

	CFoF_Player *pPlayerA = match.m_hPlayerA.Get();
	CFoF_Player *pPlayerB = match.m_hPlayerB.Get();
	const bool bValidA = IsEligiblePlayer( pPlayerA );
	const bool bValidB = IsEligiblePlayer( pPlayerB );
	const bool bAliveA = bValidA && pPlayerA->IsAlive();
	const bool bAliveB = bValidB && pPlayerB->IsAlive();
	if ( !bTimedOut && bAliveA && bAliveB )
		return;

	int nWinner = 0;
	if ( bAliveA != bAliveB )
		nWinner = bAliveA ? 1 : 2;
	else if ( bTimedOut && bAliveA && bAliveB )
	{
		char szHealthA[16];
		char szHealthB[16];
		Q_snprintf( szHealthA, sizeof( szHealthA ), "%d", pPlayerA->GetHealth() );
		Q_snprintf( szHealthB, sizeof( szHealthB ), "%d", pPlayerB->GetHealth() );
		SendNotice( pPlayerA, "#VS_Alive_Health", szHealthB );
		SendNotice( pPlayerB, "#VS_Alive_Health", szHealthA );
		if ( pPlayerA->GetHealth() != pPlayerB->GetHealth() )
			nWinner = pPlayerA->GetHealth() > pPlayerB->GetHealth() ? 1 : 2;
	}
	else if ( bValidA != bValidB )
	{
		nWinner = bValidA ? 1 : 2;
	}

	if ( nWinner == 1 )
		++match.m_nWinsA;
	else if ( nWinner == 2 )
		++match.m_nWinsB;
	match.m_bRoundResolved = true;

	if ( bAliveA )
		pPlayerA->AddFlag( FL_ATCONTROLS );
	if ( bAliveB )
		pPlayerB->AddFlag( FL_ATCONTROLS );
	if ( bValidA != bValidB )
	{
		SendForfeitResult( match,
			nWinner == 1 ? pPlayerA : pPlayerB,
			nWinner == 1 ? pPlayerB : pPlayerA );
	}
	else
	{
		SendRoundResult( match );
	}
}

void CVersusMode::UpdateActiveRound()
{
	const bool bTimedOut = gpGlobals->curtime >= m_flPhaseDeadline;
	bool bAllResolved = true;
	for ( int i = 0; i < m_Matches.Count(); ++i )
	{
		ResolveMatchRound( m_Matches[i], bTimedOut );
		if ( !m_Matches[i].m_bRoundResolved )
			bAllResolved = false;
	}

	if ( bAllResolved || bTimedOut )
		FinishCurrentRound();
}

void CVersusMode::FinishCurrentRound()
{
	const bool bLastRound = m_nRoundNumber >= GetTotalRounds();
	for ( int i = 0; i < m_Matches.Count(); ++i )
	{
		Match_t &match = m_Matches[i];
		if ( !match.m_bRoundResolved )
			ResolveMatchRound( match, true );
		if ( bLastRound )
		{
			match.m_bMatchComplete = true;
			SendMatchResult( match );
		}
	}

	m_nPhase = FOF_VERSUS_PHASE_RESULT;
	m_flPhaseDeadline = gpGlobals->curtime + FOF_VERSUS_RESULT_TIME;
	m_flNextHudUpdate = gpGlobals->curtime;
}

void CVersusMode::CompleteMatches()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		ClearPlayerVersusState(
			ToFoFPlayer( UTIL_PlayerByIndex( i ) ) );
	}
	m_Matches.Purge();
	++m_nArenaRotation;
	m_nRoundNumber = 1;
	m_nPhase = FOF_VERSUS_PHASE_WAITING;
	m_flPhaseDeadline = gpGlobals->curtime + 0.25f;
	m_flMatchCycleDeadline = 0.0f;
	m_flNextHudUpdate = gpGlobals->curtime;
}

void CVersusMode::ClearPlayerVersusState( CFoF_Player *pPlayer ) const
{
	if ( !pPlayer )
		return;
	pPlayer->SetFoFVersusSpawn( NULL );
	pPlayer->RemoveFlag( FL_ATCONTROLS );
}

void CVersusMode::SendNotice(
	CFoF_Player *pPlayer, const char *pszToken,
	const char *pszArgument1, const char *pszArgument2,
	const char *pszArgument3 ) const
{
	if ( !pPlayer || pPlayer->IsBot() || !pszToken || !pszToken[0] )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 0 );
		WRITE_STRING( pszToken );
		WRITE_STRING( pszArgument1 ? pszArgument1 : "" );
		WRITE_STRING( pszArgument2 ? pszArgument2 : "" );
		WRITE_STRING( pszArgument3 ? pszArgument3 : "" );
	MessageEnd();
}

void CVersusMode::SendMatchPresentation( const Match_t &match ) const
{
	CFoF_Player *pPlayerA = match.m_hPlayerA.Get();
	CFoF_Player *pPlayerB = match.m_hPlayerB.Get();
	if ( !pPlayerA || !pPlayerB )
		return;

	char szRound[16];
	Q_snprintf( szRound, sizeof( szRound ), "%d/%d",
		m_nRoundNumber, GetTotalRounds() );
	char szPlayerA[MAX_PLAYER_NAME_LENGTH + 24];
	char szPlayerB[MAX_PLAYER_NAME_LENGTH + 24];
	Q_snprintf( szPlayerA, sizeof( szPlayerA ), "%s (%d)",
		pPlayerA->GetPlayerName(), static_cast< int >( pPlayerA->GetFoFGlobalRank() ) );
	Q_snprintf( szPlayerB, sizeof( szPlayerB ), "%s (%d)",
		pPlayerB->GetPlayerName(), static_cast< int >( pPlayerB->GetFoFGlobalRank() ) );
	SendNotice( pPlayerA, "#VS_Match_Presentation",
		szPlayerA, szPlayerB );
	SendNotice( pPlayerB, "#VS_Match_Presentation",
		szPlayerB, szPlayerA );
	SendNotice( pPlayerA, "#VS_Round_Counter", szRound );
	SendNotice( pPlayerB, "#VS_Round_Counter", szRound );
	if ( !FoFVersusTournamentEnabled() )
	{
		char szPointsA[16];
		char szPointsB[16];
		Q_snprintf( szPointsA, sizeof( szPointsA ), "%d", match.m_nPointsForA );
		Q_snprintf( szPointsB, sizeof( szPointsB ), "%d", match.m_nPointsForB );
		SendNotice( pPlayerA, "#VS_Rank_Forecast", szPointsA, szPointsB );
		SendNotice( pPlayerB, "#VS_Rank_Forecast", szPointsB, szPointsA );
	}
}

void CVersusMode::SendRoundResult( const Match_t &match ) const
{
	CFoF_Player *pPlayerA = match.m_hPlayerA.Get();
	CFoF_Player *pPlayerB = match.m_hPlayerB.Get();
	if ( !pPlayerA || !pPlayerB )
		return;

	char szWinsA[MAX_PLAYER_NAME_LENGTH + 24];
	char szWinsB[MAX_PLAYER_NAME_LENGTH + 24];
	Q_snprintf( szWinsA, sizeof( szWinsA ), "%s %d",
		pPlayerA->GetPlayerName(), match.m_nWinsA );
	Q_snprintf( szWinsB, sizeof( szWinsB ), "%d %s",
		match.m_nWinsB, pPlayerB->GetPlayerName() );
	SendNotice( pPlayerA,
		"#VS_Round_Result", szWinsA, szWinsB );
	SendNotice( pPlayerB,
		"#VS_Round_Result", szWinsA, szWinsB );

	char szRound[16];
	Q_snprintf( szRound, sizeof( szRound ), "%d/%d",
		m_nRoundNumber, GetTotalRounds() );
	SendNotice( pPlayerA, "#VS_Round_Counter", szRound );
	SendNotice( pPlayerB, "#VS_Round_Counter", szRound );
}

void CVersusMode::SendMatchResult( const Match_t &match ) const
{
	CFoF_Player *pPlayerA = match.m_hPlayerA.Get();
	CFoF_Player *pPlayerB = match.m_hPlayerB.Get();
	const bool bRankedMatch = !FoFVersusTournamentEnabled();
	if ( match.m_nWinsA == match.m_nWinsB )
	{
		if ( match.m_nWinsA == 0 && bRankedMatch )
		{
			char szPointsA[16];
			char szPointsB[16];
			Q_snprintf( szPointsA, sizeof( szPointsA ), "%d", match.m_nPointsForB );
			Q_snprintf( szPointsB, sizeof( szPointsB ), "%d", match.m_nPointsForA );
			SendNotice( pPlayerA, "#VS_Match_DrawZero", szPointsA );
			SendNotice( pPlayerB, "#VS_Match_DrawZero", szPointsB );
			if ( pPlayerA )
				pPlayerA->SetFoFGlobalRank( pPlayerA->GetFoFGlobalRank() - match.m_nPointsForB );
			if ( pPlayerB )
				pPlayerB->SetFoFGlobalRank( pPlayerB->GetFoFGlobalRank() - match.m_nPointsForA );
		}
		else
		{
			SendNotice( pPlayerA, "#VS_Match_Draw" );
			SendNotice( pPlayerB, "#VS_Match_Draw" );
		}
		return;
	}

	CFoF_Player *pWinner =
		match.m_nWinsA > match.m_nWinsB ? pPlayerA : pPlayerB;
	CFoF_Player *pLoser = pWinner == pPlayerA ? pPlayerB : pPlayerA;
	const int nPoints = bRankedMatch ?
		( pWinner == pPlayerA ? match.m_nPointsForA : match.m_nPointsForB ) : 0;
	char szPoints[16];
	Q_snprintf( szPoints, sizeof( szPoints ), "%d", nPoints );
	SendNotice( pWinner, "#VS_Match_Winner_Points", szPoints );
	SendNotice( pLoser, "#VS_Match_Loser_Points", szPoints );
	if ( pWinner )
		pWinner->SetFoFGlobalRank( pWinner->GetFoFGlobalRank() + nPoints );
	if ( pLoser )
		pLoser->SetFoFGlobalRank( pLoser->GetFoFGlobalRank() - nPoints );
}

void CVersusMode::SendForfeitResult(
	const Match_t &match, CFoF_Player *pWinner, CFoF_Player *pOther ) const
{
	if ( !pWinner )
		return;

	const bool bWinnerIsA = pWinner == match.m_hPlayerA.Get();
	char szScore[24];
	Q_snprintf( szScore, sizeof( szScore ), "(%d-%d)",
		bWinnerIsA ? match.m_nWinsA : match.m_nWinsB,
		bWinnerIsA ? match.m_nWinsB : match.m_nWinsA );
	SendNotice( pWinner, "#VS_Match_Winner",
		pWinner->GetPlayerName(), szScore );
	SendNotice( pOther, "#VS_Round_Winner",
		pWinner->GetPlayerName(), szScore );
}

void CVersusMode::UpdateHudTimers()
{
	if ( gpGlobals->curtime < m_flNextHudUpdate )
		return;
	m_flNextHudUpdate = gpGlobals->curtime + 1.0f;

	hudtextparms_t text;
	text.x = -1.0f;
	text.y = 0.1f;
	text.effect = 0;
	text.r1 = 55;
	text.g1 = 200;
	text.b1 = 10;
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
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !IsEligiblePlayer( pPlayer ) || pPlayer->IsBot() )
			continue;

		char szText[40];
		if ( IsPlayerInMatch( pPlayer ) )
		{
			const int nSeconds = MAX( 0,
				Ceil2Int( m_flPhaseDeadline - gpGlobals->curtime ) );
			Q_snprintf( szText, sizeof( szText ),
				"TIME LEFT: %i", nSeconds );
		}
		else
		{
			const int nSeconds = clamp( Ceil2Int(
				m_flMatchCycleDeadline - gpGlobals->curtime ), 0, 999 );
			Q_snprintf( szText, sizeof( szText ),
				"YOUR MATCH STARTS IN: %i", nSeconds );
		}
		UTIL_HudMessage( pPlayer, text, szText );
	}
}

void CVersusMode::ModeThink()
{
	if ( FoFVersusCurrentMode() != 5 )
	{
		ResetMode( true );
		SetNextThink( gpGlobals->curtime + 0.5f );
		return;
	}

	const bool bWarmup = FoFVersusWarmupActive();
	if ( bWarmup )
	{
		if ( !m_bWasWarmup )
			ResetMode( true );
		m_bWasWarmup = true;
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;
	}
	if ( m_bWasWarmup )
	{
		ResetMode( true );
		m_bWasWarmup = false;
	}

	switch ( m_nPhase )
	{
	case FOF_VERSUS_PHASE_WAITING:
		if ( gpGlobals->curtime >= m_flPhaseDeadline )
		{
			if ( BuildMatches() )
				StartRound( FOF_VERSUS_FIRST_PREPARATION );
			else
				m_flPhaseDeadline = gpGlobals->curtime + 1.0f;
		}
		break;
	case FOF_VERSUS_PHASE_PREPARING:
		UpdateHudTimers();
		if ( gpGlobals->curtime >= m_flPhaseDeadline )
			StartActiveRound();
		break;
	case FOF_VERSUS_PHASE_ACTIVE:
		UpdateHudTimers();
		UpdateActiveRound();
		break;
	case FOF_VERSUS_PHASE_RESULT:
		UpdateHudTimers();
		if ( gpGlobals->curtime >= m_flPhaseDeadline )
		{
			if ( m_nRoundNumber < GetTotalRounds() )
			{
				++m_nRoundNumber;
				StartRound( FOF_VERSUS_NEXT_PREPARATION );
			}
			else
			{
				CompleteMatches();
			}
		}
		break;
	default:
		ResetMode( true );
		break;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}
