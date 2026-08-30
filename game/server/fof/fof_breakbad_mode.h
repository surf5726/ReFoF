#ifndef FOF_BREAKBAD_MODE_H
#define FOF_BREAKBAD_MODE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "GameEventListener.h"
#include "utlvector.h"

class CBBMulti;
class CFoF_Player;
class CTakeDamageInfo;

void FoFUpdateBreakBadMode();
void FoFBreakBadPlayerSpawn( CFoF_Player *pPlayer );
void FoFBreakBadPlayerDamaged(
	CFoF_Player *pVictim, const CTakeDamageInfo &info );
void FoFBreakBadPlayerKilled(
	CFoF_Player *pVictim, const CTakeDamageInfo &info );

struct FoFBreakBadLocation_t
{
	Vector m_vecOrigin;
	int m_nTier;
};

struct FoFBreakBadPlayerState_t
{
	float m_flLastSpawnTime;
	float m_flDamageFinePool;
	float m_flReservedState0;
	float m_flLastDamageFineTime;
	float m_flDisarmZoneTime;
	float m_flReservedState1;
	float m_flNextLootPickTime;
	float m_flNextLootDropTime;
	float m_flNextWhiskeyTime;
};

enum
{
	FOF_BREAKBAD_PLAYER_STATE_COUNT = 26
};

class CBreakBad : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CBreakBad, CBaseEntity );
	DECLARE_DATADESC();

	CBreakBad();
	virtual void Spawn();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void ModeThink();
	void InitializeRoundEntities();
	void OnPlayerSpawn( CFoF_Player *pPlayer );
	void OnPlayerDamaged(
		CFoF_Player *pVictim, const CTakeDamageInfo &info );
	void OnPlayerKilled(
		CFoF_Player *pVictim, const CTakeDamageInfo &info );

private:
	void ResetAllPlayerStates();
	void ResetPlayerState( CFoF_Player *pPlayer );
	void UpdateTeamCash();
	void UpdatePlayerZones(
		CFoF_Player *pPlayer, FoFBreakBadPlayerState_t &state );
	CBBMulti *CreatePlayerZone(
		const char *pszClassname, CFoF_Player *pPlayer,
		float flDistanceScale, bool bTrackRecentLootDrop );
	bool FindDynamicZonePosition(
		CFoF_Player *pPlayer, float flDistanceScale,
		bool bTrackRecentLootDrop, Vector &vecPosition ) const;
	bool IsZonePositionClear( const Vector &vecPosition ) const;
	float GetMapDistanceScale() const;
	float GetLootDropDistanceScale( CFoF_Player *pPlayer ) const;
	float CalculateNextLootPickTime( int nTeam ) const;
	void JailPlayer( CFoF_Player *pPlayer );
	FoFBreakBadPlayerState_t *PlayerState( CFoF_Player *pPlayer );

	FoFBreakBadPlayerState_t
		m_PlayerStates[FOF_BREAKBAD_PLAYER_STATE_COUNT];
	int m_nNextZonePlayer;
	CUtlVector< FoFBreakBadLocation_t > m_RoundLocations;
};

#endif // FOF_BREAKBAD_MODE_H

