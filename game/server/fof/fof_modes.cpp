//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF mode controller dispatch and map-owned mode selection.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_modes.h"

#include "fof/fof_breakbad_mode.h"
#include "game.h"
#include "hl2mp_gamerules.h"

#include "tier0/memdbgon.h"

extern ConVar fof_sv_currentmode;
extern ConVar fof_sv_maxteams;

static int FoFMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

void FoFCreateModeControllerForCurrentGame(
	bool bInitializeCurrentRound )
{
	const char *pszClassname = NULL;
	switch ( FoFMode() )
	{
	case 3:
		pszClassname = "fof_breakbad";
		break;
	case 4:
		pszClassname = "fof_elimination";
		break;
	case 5:
		pszClassname = "fof_versus";
		break;
	case 6:
		pszClassname = "fof_coursemode";
		break;
	default:
		break;
	}

	if ( !pszClassname ||
		gEntList.FindEntityByClassname( NULL, pszClassname ) )
	{
		return;
	}

	CBaseEntity *pController = CreateEntityByName( pszClassname );
	if ( pController )
	{
		DispatchSpawn( pController );
		if ( bInitializeCurrentRound && FoFMode() == 3 )
		{
			static_cast< CBreakBad * >( pController )->
				InitializeRoundEntities();
		}
	}
}

void FoFExecuteMapConfig()
{
	if ( !gpGlobals || gpGlobals->mapname == NULL_STRING )
		return;

	char szCommand[128];
	Q_snprintf( szCommand, sizeof( szCommand ),
		"exec mapcfg/%s.cfg\n", STRING( gpGlobals->mapname ) );
	engine->ServerCommand( szCommand );
	engine->ServerExecute();
}

void FoFApplyMapOwnedModeSelection()
{
	// Course sessions deliberately reuse objective-mode BSPs.  Their selected
	// mode remains authoritative over a map-owned teamplay controller.
	if ( fof_sv_currentmode.GetInt() == 6 )
		return;

	// FoF treats a map-owned fof_teamplay controller as authoritative.
	// This runs after BSP entities have spawned but before SV_ActivateServer so
	// the initial replicated-ConVar snapshot already contains Teamplay's mode.
	if ( !gEntList.FindEntityByClassname( NULL, "fof_teamplay" ) )
	{
		static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
		if ( fof_sv_currentmode.GetInt() == 4 &&
			( !battleRoyale.IsValid() || !battleRoyale.GetBool() ) )
		{
			teamplay.SetValue( 1 );
			fof_sv_maxteams.SetValue( 2 );
		}
		// Versus pairs individual players in arenas. It must not inherit the
		// previous mode's team selection, including a four-team Shootout setup.
		if ( fof_sv_currentmode.GetInt() == 5 )
			teamplay.SetValue( 0 );
		return;
	}

	teamplay.SetValue( 1 );
	fof_sv_currentmode.SetValue( 2 );
	fof_sv_maxteams.SetValue( 2 );
}
