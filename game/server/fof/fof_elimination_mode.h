#ifndef FOF_ELIMINATION_MODE_H
#define FOF_ELIMINATION_MODE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "GameEventListener.h"
#include "utlvector.h"

class CFoFCapEnt;
class CFoF_Player;

bool FoFBattleRoyaleCanAcquireOutlawRole();

struct FoFBattleRoyaleArea_t
{
	Vector m_vecNorthWest;
	Vector m_vecNorthEast;
	Vector m_vecSouthWest;
	Vector m_vecSouthEast;
	Vector m_vecCenter;
	int m_nDistance;
	int m_nArea;
};

class CDarkMode : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CDarkMode, CBaseEntity );
	DECLARE_DATADESC();

	CDarkMode();
	virtual void Spawn();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void ModeThink();
	bool CanAcquireBattleRoyaleOutlawRole() const;
	CBaseEntity *GetRoundSpawnPoint( CFoF_Player *pPlayer ) const;

private:
	void BeginRound();
	bool SelectTeamSpawnPoints();
	bool ResetRoundScene();
	void StartBuyRound();
	void SetBuyRoundPlayerLock( bool bLocked );
	void ActivateRound();
	void HandlePlayerConnect( IGameEvent *pEvent );
	void HandlePlayerDeath( IGameEvent *pEvent );
	void ResetRoundPlayers();
	void UpdatePlayerGlowRegistration( bool bEnable );
	void UpdateFreeVision();
	void CheckNormalEliminationRound();
	void CheckBattleRoyaleRound();
	void ResetBattleRoyaleState( bool bRemoveCap );
	void InitializeBattleRoyaleRound();
	void UpdateBattleRoyaleState();
	void ActivateBattleRoyaleSafeZone();
	void UpdateBattleRoyaleSafeZone();
	void EnterBattleRoyaleStandoff();
	void SetBattleRoyaleNetworkAreas( bool bActive );
	void SendBattleRoyaleNotice(
		CFoF_Player *pPlayer, const char *pszToken ) const;
	void SendBattleRoyaleMarker( bool bAdd ) const;
	bool IsInsideBattleRoyaleZone( CFoF_Player *pPlayer ) const;
	void FinishRound( int nWinningTeam, int nWinningPlayer );
	void RestartRound();

	bool m_bRoundPending;
	bool m_bRoundActive;
	bool m_bRoundFinished;
	bool m_bLastRound;
	float m_flRoundStartTime;
	float m_flNextFreeVisionChange;
	float m_flRoundEndTime;
	float m_flRoundRestartTime;
	CUtlVector< FoFBattleRoyaleArea_t > m_BattleRoyaleAreas;
	CHandle< CFoFCapEnt > m_hBattleRoyaleCap;
	Vector m_vecBattleRoyaleCenter;
	int m_nBattleRoyalePhase;
	bool m_bBattleRoyaleSafeActive;
	float m_flBattleRoyalePreSafeEnd;
	float m_flBattleRoyaleShrinkEnd;
	float m_flBattleRoyalePostSafeEnd;
	float m_flBattleRoyaleNextTick;
	float m_flBattleRoyaleAreaPerTick;
	float m_flBattleRoyaleRemainingArea;
	CHandle< CBaseEntity > m_hTeamSpawnPoints[2];
	Vector m_vecTeamSpawnOrigins[2];
	bool m_bHaveTeamSpawnPoints;
	bool m_bSpawningRoundPlayers;
	bool m_bHadRound;
	int m_nLastBuyTick;
	float m_flNextModeUpdate;
};

#endif // FOF_ELIMINATION_MODE_H
