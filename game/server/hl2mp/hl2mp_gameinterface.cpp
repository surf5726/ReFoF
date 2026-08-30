//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "hl2mp_gameinterface.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_modes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	// FoF supports up to 25 clients; the stock HL2DM limit would clamp larger
	// listen-server configurations before the first map starts.
	minplayers = 2;
	maxplayers = 25;
	defaultMaxPlayers = 20;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	FoFExecuteMapConfig();
	if ( HL2MPRules() )
		HL2MPRules()->ApplyFoFMapOwnedRuleSelection();
}
