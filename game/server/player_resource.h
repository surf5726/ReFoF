//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLAYER_RESOURCE_H
#define PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"

class CPlayerResource : public CBaseEntity
{
	DECLARE_CLASS( CPlayerResource, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual	int	 ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	virtual void ResourceThink( void );
	virtual void UpdatePlayerData( void );
	virtual int  UpdateTransmitState(void);

protected:
	enum { PLAYER_RESOURCE_ARRAY_COUNT = 26 };

	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	CNetworkArray( int, m_iPing, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iScore, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iExp, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iDeaths, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_bConnected, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iTeam, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_bAlive, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iHealth, PLAYER_RESOURCE_ARRAY_COUNT );
	CNetworkArray( int, m_iFoFState, PLAYER_RESOURCE_ARRAY_COUNT );

	int	m_nUpdateCounter;
};

extern CPlayerResource *g_pPlayerResource;

#endif // PLAYER_RESOURCE_H
