#ifndef FOF_SPAWN_H
#define FOF_SPAWN_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "modelentities.h"

class CTeamSpawn : public CPointEntity
{
public:
	DECLARE_CLASS( CTeamSpawn, CPointEntity );
	DECLARE_DATADESC();

	CTeamSpawn();
	bool IsEnabled() const { return !m_bDisabled; }
	void InputEnable( inputdata_t &inputData );
	void InputDisable( inputdata_t &inputData );

private:
	bool m_bDisabled;
};

class CFuncRespawnRoomVisualizer : public CFuncBrush
{
public:
	DECLARE_CLASS( CFuncRespawnRoomVisualizer, CFuncBrush );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFuncRespawnRoomVisualizer();
	virtual void Spawn();
	virtual bool ShouldCollide(
		int collisionGroup, int contentsMask ) const;
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	void InputEnable( inputdata_t &inputData );
	void InputDisable( inputdata_t &inputData );

private:
	void RespawnBlockedPlayersInside();
	int m_nTeamBlock;
};

#endif // FOF_SPAWN_H
