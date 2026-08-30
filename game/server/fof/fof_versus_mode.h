#ifndef FOF_VERSUS_MODE_H
#define FOF_VERSUS_MODE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "GameEventListener.h"
#include "utlvector.h"

class CFoF_Player;

class CVersusMode : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CVersusMode, CBaseEntity );
	DECLARE_DATADESC();

	CVersusMode();
	virtual void Precache();
	virtual void Spawn();
	virtual void UpdateOnRemove();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void ModeThink();

private:
	struct Equipment_t
	{
		int m_nArena;
		int m_nItem;
		int m_nSide;
	};

	struct Match_t
	{
		CHandle< CFoF_Player > m_hPlayerA;
		CHandle< CFoF_Player > m_hPlayerB;
		CHandle< CBaseEntity > m_hSpawnA;
		CHandle< CBaseEntity > m_hSpawnB;
		int m_nArena;
		int m_nClone;
		int m_nWinsA;
		int m_nWinsB;
		bool m_bRoundResolved;
		bool m_bMatchComplete;
		int m_nPointsForA;
		int m_nPointsForB;
	};

	void ResetMode( bool bClearPlayers );
	void ParseEquipmentConVars();
	void ParseEquipmentList( int nArena, int nSide, const char *pszItems );
	bool BuildMatches();
	bool FindArenaSpawns( int nSlot, int &nArena, int &nClone,
		CBaseEntity *&pSpawnA, CBaseEntity *&pSpawnB ) const;
	bool IsEligiblePlayer( CFoF_Player *pPlayer ) const;
	bool IsPlayerInMatch( CFoF_Player *pPlayer ) const;
	int FindBestOpponent(
		const CUtlVector< CFoF_Player * > &players,
		CFoF_Player *pPlayer ) const;
	int GetTotalRounds() const;
	void StartRound( float flPreparationTime );
	void EquipPlayer( CFoF_Player *pPlayer, int nArena, int nSide );
	void StartActiveRound();
	void UpdateActiveRound();
	void ResolveMatchRound( Match_t &match, bool bTimedOut );
	void FinishCurrentRound();
	void CompleteMatches();
	void ClearPlayerVersusState( CFoF_Player *pPlayer ) const;
	void UpdateHudTimers();
	void SendNotice( CFoF_Player *pPlayer, const char *pszToken,
		const char *pszArgument1 = "", const char *pszArgument2 = "",
		const char *pszArgument3 = "" ) const;
	void SendMatchPresentation( const Match_t &match ) const;
	void SendRoundResult( const Match_t &match ) const;
	void SendMatchResult( const Match_t &match ) const;
	void SendForfeitResult( const Match_t &match,
		CFoF_Player *pWinner, CFoF_Player *pOther ) const;

	CUtlVector< Equipment_t > m_Equipment;
	CUtlVector< Match_t > m_Matches;
	int m_nPhase;
	int m_nRoundNumber;
	int m_nArenaRotation;
	float m_flPhaseDeadline;
	float m_flMatchCycleDeadline;
	float m_flNextHudUpdate;
	bool m_bWasWarmup;
};

#endif // FOF_VERSUS_MODE_H
