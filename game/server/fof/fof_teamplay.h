//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF map-driven teamplay round controller.
//
//=============================================================================//
#ifndef FOF_TEAMPLAY_H
#define FOF_TEAMPLAY_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"

class CTrigger_EndRound : public CLogicalEntity,
	public CGameEventListener
{
public:
	DECLARE_CLASS( CTrigger_EndRound, CLogicalEntity );
	DECLARE_DATADESC();

	CTrigger_EndRound();
	virtual void Spawn();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void InputTie( inputdata_t &inputData );
	void InputVigVictory( inputdata_t &inputData );
	void InputDespVictory( inputdata_t &inputData );
	void InputVictory_DeclareWinner( inputdata_t &inputData );
	void InputRespawnPlayers( inputdata_t &inputData );
	void InputTeleportToRespawn( inputdata_t &inputData );
	void ExtraTime( inputdata_t &inputData );
	void RoundTime( inputdata_t &inputData );
	void InputSetVigObjective( inputdata_t &inputData );
	void InputSetDespObjective( inputdata_t &inputData );
	void InputSetVigReward( inputdata_t &inputData );
	void InputSetDespReward( inputdata_t &inputData );
	void InputSetVigScore( inputdata_t &inputData );
	void InputSetDespScore( inputdata_t &inputData );
	void InputAddVigScore( inputdata_t &inputData );
	void InputAddDespScore( inputdata_t &inputData );
	void InputAddCash_All( inputdata_t &inputData );
	void InputAddCash_Vigilantes( inputdata_t &inputData );
	void InputAddCash_Desperados( inputdata_t &inputData );
	void InputAddCash_Auto( inputdata_t &inputData );
	void InputResetScores( inputdata_t &inputData );
	void InputDisplayMsg( inputdata_t &inputData );
	void InputHealAndReload( inputdata_t &inputData );

	int GetTeamReward( int nTeam ) const;
	int GetTeamObjective( int nTeam ) const;
	int GetRespawnSystem() const { return m_nRespawnSystem; }
	int GetRoundNumber() const { return m_nRoundNumber; }
	bool IsRoundBased() const { return m_bRoundBased; }
	bool HasPendingSideSwitch() const { return m_bSwitchTeamsPending; }
	float GetExtraTimeRemaining() const;
	float GetRoundTimeEnd() const;
	float GetDelayedRespawnTime() const { return m_flDelayedRespawnTime; }

	void ResetRoundState();
	void ApplyRespawnSystem();
	void ShowTeamplayDialog();
	void FireNewRoundOutput();
	void FireNewBuyRoundOutput();
	void FireRoundTimeEndOutput();
	void FireTimerEndOutput();
	void FireNoDespAliveOutput();
	void FireNoVigAliveOutput();
	void ClearTeamStateForNewRound();
	void PrepareTeamsForNewRound();
	void ProcessDelayedRespawn();
	void DelayExtraTimeEnd( float flSeconds );
	void ClearDelayedRespawnTime() { m_flDelayedRespawnTime = 0.0f; }
	void ClearPendingSideSwitch() { m_bSwitchTeamsPending = false; }
	float GetRoundLimitTime() const { return m_flRoundLimitTime; }

private:
	void SendCurrentState();
	void FinishRound( int nWinningTeam );
	void SetTeamObjective( int nTeam, int nObjective );
	void SetTeamReward( int nTeam, int nReward );
	void SetTeamRoundScore( int nTeam, int nScore );
	void AddCashToTeam( int nTeam, int nAmount );
	void UpdateEliminationObjectiveScore(
		int nScoringTeam, int nDeadTeam );
	void BalanceTeams();

	// Member order is ABI-significant. The shipped object is 0x490 bytes and
	// places the CGameEventListener secondary base at the original ABI slot.
	COutputEvent m_OnNoDespAlive;
	COutputEvent m_OnNoVigAlive;
	COutputEvent m_OnTimerEnd;
	COutputEvent m_OnNewRound;
	COutputEvent m_OnNewBuyRound;
	COutputEvent m_OnRoundEnd;
	COutputEvent m_OnRoundTimeEnd;
	COutputEvent m_OnRoundEven;
	COutputEvent m_OnRoundOdd;
	COutputEvent m_OnNewRoundRemove;

	bool m_bRoundBased;
	float m_flRoundLimitTime;
	int m_nTeamObjective[6];
	int m_nTeamReward[6];
	int m_nRespawnSystem;
	float m_flExtraTimeEnd;
	bool m_bSwitchTeamsPending;
	bool m_bSwitchTeams;
	float m_flDelayedRespawnTime;
	float m_flRoundStartTime;
	int m_nRoundTime;
	int m_nRoundNumber;
	int m_nPendingWinner;
	bool m_bAutoCash;
};

#endif // FOF_TEAMPLAY_H
