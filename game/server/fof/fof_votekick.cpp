#include "cbase.h"
#include "fof/fof_player.h"
#include "fof/fof_votekick.h"

#include "recipientfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFVotekickCandidate_t
{
	int m_nPlayerIndex;
	int m_nNameChanges;
	int m_nVotes;
};

static int __cdecl FoFVotekickCandidateCompare(
	const FoFVotekickCandidate_t *pLeft,
	const FoFVotekickCandidate_t *pRight )
{
	if ( pLeft->m_nNameChanges != pRight->m_nNameChanges )
		return pRight->m_nNameChanges - pLeft->m_nNameChanges;
	if ( pLeft->m_nVotes != pRight->m_nVotes )
		return pRight->m_nVotes - pLeft->m_nVotes;
	return pRight->m_nPlayerIndex - pLeft->m_nPlayerIndex;
}

static void FoFSendVotekickMenuLine(
	CFoF_Player *pPlayer, const char *pszLine,
	bool bMore, int nCommandId )
{
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "ShowMenuFoF" );
		WRITE_STRING( pszLine );
		WRITE_BYTE( bMore ? 1 : 0 );
		WRITE_SHORT( nCommandId );
	MessageEnd();
}

static bool FoFIsVotekickEligibleTarget( CFoF_Player *pCandidate )
{
	if ( !pCandidate || !pCandidate->IsConnected() ||
		pCandidate->IsFakeClient() ||
		pCandidate->IsHLTV() || pCandidate->IsReplay() )
	{
		return false;
	}

	// The original target predicate also rejects the three low Steam-resource
	// state bits. They mark clients that must not enter the public vote list.
	return ( pCandidate->GetFoFResourceState() & 0x7 ) == 0;
}

static bool FoFHasVotekickPerformance( CFoF_Player *pCandidate )
{
	if ( !pCandidate )
		return false;

	const int nFrags = clamp( pCandidate->FragCount(), 1, 10000 );
	const float flPlayedSeconds = MAX( 0.0f,
		gpGlobals->curtime -
		pCandidate->GetFoFVotekickConnectionStartTime() );
	const float flPlayedMinutes = clamp(
		flPlayedSeconds / 60.0f, 5.0f, 1000.0f );
	return static_cast< float >( nFrags ) / flPlayedMinutes >= 2.5f;
}

static void FoFBanVotekickTarget(
	CFoF_Player *pTarget, int nMinutes )
{
	if ( !pTarget || !pTarget->edict() )
		return;

	engine->ServerCommand( UTIL_VarArgs(
		"banid %i %i kick\n", nMinutes,
		engine->GetPlayerUserId( pTarget->edict() ) ) );
}

static bool FoFGetVoterAccountID(
	CFoF_Player *pPlayer, uint32 &nAccountID )
{
	nAccountID = 0;
	if ( !pPlayer || pPlayer->IsFakeClient() )
		return false;

	const CSteamID *pSteamID = engine->GetClientSteamID( pPlayer->edict() );
	if ( !pSteamID || !pSteamID->IsValid() )
		return false;

	nAccountID = pSteamID->GetAccountID();
	return nAccountID != 0;
}

static int FoFCountVotekickPlayers( bool bActiveOnly )
{
	int nPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsFakeClient() ||
			pPlayer->IsHLTV() || pPlayer->IsReplay() )
			continue;
		if ( bActiveOnly && pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			continue;
		++nPlayers;
	}
	return nPlayers;
}

static float FoFVotekickVotesNeeded( CFoF_Player *pTarget )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 6 )
	{
		const int nPlayers = FoFCountVotekickPlayers( true );
		return static_cast< float >( clamp( nPlayers - 1, 2, 99 ) );
	}

	// FoF retains the half-player threshold as a float. For an odd human
	// population the integer vote count therefore has to cross the half vote.
	const int nPlayers = FoFCountVotekickPlayers( false );
	float flNeeded = clamp( nPlayers * 0.5f, 5.0f, 10.0f );
	if ( pTarget )
	{
		// Personal stat 2 is the persistent rounds-played value supplied by the
		// stock client. Veteran targets gain up to four additional required votes.
		flNeeded += RemapValClamped(
			static_cast< float >( pTarget->GetFoFPersonalStat( 2 ) ),
			0.0f, 2000.0f, 0.0f, 4.0f );
	}
	return flNeeded;
}

bool FoFShowVotekickMenu( CFoF_Player *pPlayer, int nPage )
{
	static ConVarRef votekickAllowed( "fof_sv_votekickallowed", true );
	if ( !pPlayer ||
		( votekickAllowed.IsValid() && !votekickAllowed.GetBool() ) ||
		pPlayer->GetFoFVotekickMenuState() > 0 ||
		pPlayer->GetFoFCrateMenuTier() > 0 )
	{
		return false;
	}

	nPage = MAX( 0, nPage );
	CUtlVector< FoFVotekickCandidate_t > candidates;
	for ( int nPass = 2; nPass >= 1; --nPass )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pCandidate = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pCandidate || pCandidate == pPlayer ||
				!FoFHasVotekickPerformance( pCandidate ) )
			{
				continue;
			}

			const int nVotes = pCandidate->GetFoFVotekickVotes();
			if ( ( nPass == 2 && nVotes == 0 ) ||
				( nPass == 1 && nVotes > 0 ) )
			{
				continue;
			}

			pCandidate->UpdateFoFVotekickNameHistory();
			if ( nVotes > 1 && RemapValClamped(
				static_cast< float >(
					pCandidate->GetFoFVotekickNameChanges() + nVotes ),
				0.0f, 10.0f, 0.0f, 1.0f ) > 0.5f )
			{
				FoFBanVotekickTarget( pCandidate, 9999 );
				continue;
			}

			if ( !FoFIsVotekickEligibleTarget( pCandidate ) )
				continue;

			FoFVotekickCandidate_t candidate;
			candidate.m_nPlayerIndex = i;
			candidate.m_nNameChanges = clamp(
				pCandidate->GetFoFVotekickNameChanges(), 0, 100 );
			candidate.m_nVotes = nVotes;
			candidates.AddToTail( candidate );
		}
	}
	candidates.Sort( FoFVotekickCandidateCompare );

	const int nFirst = nPage * 8;
	const int nLast = MIN( nFirst + 8, candidates.Count() );
	for ( int i = nFirst; i < nLast; ++i )
	{
		const FoFVotekickCandidate_t &candidate = candidates[i];
		CFoF_Player *pCandidate = ToFoFPlayer(
			UTIL_PlayerByIndex( candidate.m_nPlayerIndex ) );
		if ( !pCandidate )
			continue;

		char szLine[128];
		Q_snprintf( szLine, sizeof( szLine ),
			"%s <%i names> <%i votes>",
			pCandidate->GetFoFVotekickDisplayName(),
			candidate.m_nNameChanges, candidate.m_nVotes );
		FoFSendVotekickMenuLine(
			pPlayer, szLine, true, candidate.m_nPlayerIndex );
	}

	if ( nLast < candidates.Count() )
	{
		FoFSendVotekickMenuLine(
			pPlayer, ">>> MORE PLAYERS", true, -nPage );
	}
	FoFSendVotekickMenuLine( pPlayer, ">>> CLOSE", false, 99 );
	pPlayer->SetFoFVotekickMenuState( 99 );
	return true;
}

bool FoFHandleVotekickSelection(
	CFoF_Player *pPlayer, int nCommandId )
{
	if ( !pPlayer || pPlayer->GetFoFVotekickMenuState() != 99 )
		return false;

	pPlayer->SetFoFVotekickMenuState( -1 );
	if ( nCommandId <= 0 )
		return FoFShowVotekickMenu( pPlayer, -nCommandId + 1 );
	if ( nCommandId == 99 )
		return true;

	CFoF_Player *pTarget = ToFoFPlayer(
		UTIL_PlayerByIndex( nCommandId ) );
	if ( !FoFIsVotekickEligibleTarget( pTarget ) )
		return true;

	uint32 nAccountID = 0;
	if ( !FoFGetVoterAccountID( pPlayer, nAccountID ) ||
		pTarget->HasFoFVotekickVoter( nAccountID ) )
	{
		return true;
	}

	pTarget->AddFoFVotekickVoter( nAccountID );
	const float flNeeded = FoFVotekickVotesNeeded( pTarget );
	char szVotesRemaining[8];
	Q_snprintf( szVotesRemaining, sizeof( szVotesRemaining ), "%i",
		static_cast< int >( clamp(
			flNeeded + 1.0f - pTarget->GetFoFVotekickVotes(),
			0.0f, 99.0f ) ) );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#Votekick_player_voted",
		pPlayer->GetPlayerName(), pTarget->GetPlayerName(),
		szVotesRemaining );

	if ( static_cast< float >( pTarget->GetFoFVotekickVotes() ) < flNeeded )
		return true;

	UTIL_ClientPrintAll( HUD_PRINTTALK, "#FoF_VotekickResult",
		pTarget->GetPlayerName(), "6" );
	ClientPrint( pTarget, HUD_PRINTCENTER, "#FoF_VotekickResult",
		pTarget->GetPlayerName(), "6" );
	FoFBanVotekickTarget( pTarget, 360 );
	return true;
}

static void FoFVotekickCommand( const CCommand &args )
{
	CFoF_Player *pPlayer = ToFoFPlayer( UTIL_GetCommandClient() );
	if ( pPlayer )
		FoFShowVotekickMenu( pPlayer, 0 );
}

static ConCommand votekick(
	"votekick", FoFVotekickCommand, "", FCVAR_CLIENTCMD_CAN_EXECUTE );
