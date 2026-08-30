//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF listen-server AI editor command protocol.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_ai_editor.h"

#include "filesystem.h"
#include "fof/fof_bot.h"
#include "fof/fof_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFAIEditorServerState_t
{
	FoFAIEditorServerState_t()
		: m_nNormalOffset( 0 ),
		  m_nPropType( 0 )
	{
		m_szPropClassname[0] = '\0';
		m_szPropModel[0] = '\0';
	}

	CHandle< CFoF_Player > m_hOwner;
	FoFBotProfile_t m_BotProfile;
	char m_szPropClassname[64];
	char m_szPropModel[128];
	int m_nNormalOffset;
	int m_nPropType;
	CUtlVector< EHANDLE > m_PlacedEntities;
};

static FoFAIEditorServerState_t s_FoFAIEditorStates[MAX_PLAYERS + 1];

static bool FoFIsAIEditorCommand( const char *pszCommand )
{
	return FStrEq( pszCommand, "ai_editor_start" ) ||
		FStrEq( pszCommand, "ai_editor_end" ) ||
		FStrEq( pszCommand, "select_bot" ) ||
		FStrEq( pszCommand, "create_bot" ) ||
		FStrEq( pszCommand, "create_prop" ) ||
		FStrEq( pszCommand, "remove_bot" ) ||
		FStrEq( pszCommand, "remove_all_bots" );
}

static FoFAIEditorServerState_t *FoFGetAIEditorState(
	CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return NULL;

	const int nIndex = pPlayer->entindex();
	if ( nIndex < 1 || nIndex > MAX_PLAYERS )
		return NULL;

	FoFAIEditorServerState_t &state = s_FoFAIEditorStates[nIndex];
	if ( state.m_hOwner.Get() != pPlayer )
	{
		state.m_hOwner = pPlayer;
		state.m_BotProfile = FoFBotProfile_t();
		state.m_szPropClassname[0] = '\0';
		state.m_szPropModel[0] = '\0';
		state.m_nNormalOffset = 0;
		state.m_nPropType = 0;
		state.m_PlacedEntities.Purge();
	}
	return &state;
}

static void FoFSetAIEditorConVar( const char *pszName, int nValue )
{
	ConVarRef variable( pszName, true );
	if ( variable.IsValid() )
		variable.SetValue( nValue );
}

static bool FoFReadEditorPlacement(
	const CCommand &args, Vector &origin, Vector &direction )
{
	if ( args.ArgC() < 7 )
		return false;

	origin.Init(
		static_cast< float >( Q_atoi( args[1] ) ),
		static_cast< float >( Q_atoi( args[2] ) ),
		static_cast< float >( Q_atoi( args[3] ) ) );
	direction.Init(
		Q_atof( args[4] ), Q_atof( args[5] ), Q_atof( args[6] ) );
	return IsEntityPositionReasonable( origin );
}

static void FoFStartAIEditor( CFoF_Player *pPlayer )
{
	CBaseEntity *pSpawn = gEntList.FindEntityByClassname(
		NULL, "info_player_fof" );
	if ( pSpawn )
		pSpawn->SetAbsOrigin( pPlayer->GetAbsOrigin() );

	FoFSetAIEditorConVar( "fof_sv_bot_slotpct", 0 );
	FoFSetAIEditorConVar( "fof_sv_bot_edit", 1 );
	FoFSetAIEditorConVar( "fof_sv_bot_edit_active", 1 );
	FoFSetAIEditorConVar( "fof_bot_skill", 6 );
	pPlayer->RemoveAllItems( true );
	Msg( "AI editor mode ON\n" );
}

static void FoFEndAIEditor( CFoF_Player *pPlayer )
{
	FoFSetAIEditorConVar( "fof_sv_bot_edit_active", 0 );
	FoFSetAIEditorConVar( "fof_bot_skill", 5 );
	Msg( "AI editor mode OFF\n" );
	if ( !pPlayer->IsObserver() )
		pPlayer->ForceRespawn();
}

static void FoFSelectAIEditorEntry(
	FoFAIEditorServerState_t &state, const CCommand &args )
{
	if ( args.ArgC() < 3 )
		return;

	Q_strncpy( state.m_BotProfile.m_szName,
		args[1], sizeof( state.m_BotProfile.m_szName ) );
	Q_strncpy( state.m_BotProfile.m_szEquipment,
		args[2], sizeof( state.m_BotProfile.m_szEquipment ) );
	Q_strncpy( state.m_szPropClassname,
		args[1], sizeof( state.m_szPropClassname ) );
	Q_strncpy( state.m_szPropModel,
		args[2], sizeof( state.m_szPropModel ) );

	state.m_BotProfile.m_nRotationSpeed = args.ArgC() > 3 ?
		clamp( Q_atoi( args[3] ), 0, 10 ) : 5;
	state.m_BotProfile.m_nShootDelay = args.ArgC() > 4 ?
		clamp( Q_atoi( args[4] ), 0, 10 ) : 5;
	state.m_BotProfile.m_nAimTrailing = args.ArgC() > 5 ?
		clamp( Q_atoi( args[5] ), 0, 10 ) : 5;
	state.m_BotProfile.m_nStrafe = args.ArgC() > 6 ?
		clamp( Q_atoi( args[6] ), 0, 10 ) : 5;
	state.m_BotProfile.m_nForceTeam = args.ArgC() > 7 ?
		Q_atoi( args[7] ) : 0;
	state.m_BotProfile.m_nAggression = args.ArgC() > 8 ?
		clamp( Q_atoi( args[8] ), 0, 10 ) : 5;
	state.m_nNormalOffset = args.ArgC() > 3 ? Q_atoi( args[3] ) : 0;
	state.m_nPropType = args.ArgC() > 4 ? Q_atoi( args[4] ) : 0;
}

static CBaseEntity *FoFCreateAIEditorProp(
	const char *pszClassname, const char *pszModel,
	const Vector &origin, const Vector &direction )
{
	if ( !pszClassname || !pszClassname[0] )
		return NULL;

	const bool bPhysics =
		Q_stristr( pszClassname, "physics" ) != NULL;
	const bool bDynamic =
		Q_stristr( pszClassname, "dynamic" ) != NULL;
	const bool bHadPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	CBaseEntity *pEntity = CreateEntityByName( pszClassname );
	if ( !pEntity )
	{
		CBaseEntity::SetAllowPrecache( bHadPrecache );
		return NULL;
	}

	pEntity->SetAbsOrigin( origin );
	QAngle angles = vec3_angle;
	if ( bPhysics || bDynamic )
	{
		angles.Init( direction.x, direction.y, direction.z );
	}
	else if ( direction.LengthSqr() > 0.0f )
	{
		VectorAngles( direction, angles );
	}
	pEntity->SetAbsAngles( angles );

	if ( pszModel && pszModel[0] && Q_stricmp( pszModel, "null" ) )
	{
		CBaseEntity::PrecacheModel( pszModel );
		pEntity->KeyValue( "model", pszModel );
	}

	if ( bPhysics || bDynamic )
	{
		pEntity->KeyValue( "fademindist", "-1" );
		pEntity->KeyValue( "fademaxdist", "0" );
		pEntity->KeyValue( "fadescale", "1" );
		pEntity->KeyValue( "inertiaScale", "1.0" );
		pEntity->KeyValue( "physdamagescale", "0.1" );
	}
	else if ( FClassnameIs( pEntity, "item_whiskey" ) )
	{
		pEntity->AddSpawnFlags( 0x200 );
	}

	DispatchSpawn( pEntity );
	pEntity->Activate();
	if ( bDynamic )
		pEntity->SetCollisionGroup( 6 );
	if ( !bPhysics && !bDynamic )
		pEntity->AddSpawnFlags( 0x100 | 0x800 );
	pEntity->AddSpawnFlags( 0x1000 );
	CBaseEntity::SetAllowPrecache( bHadPrecache );
	return pEntity;
}

static CBaseEntity *FoFCreateAIEditorProp(
	const FoFAIEditorServerState_t &state,
	const Vector &origin, const Vector &direction )
{
	return FoFCreateAIEditorProp(
		state.m_szPropClassname, state.m_szPropModel,
		origin, direction );
}

bool FoFLoadShootoutCustomPreset( const char *pszMapName )
{
	static ConVarRef customPreset( "fof_sv_shootout_custom", true );
	if ( !customPreset.IsValid() || !customPreset.GetBool() ||
		!pszMapName || !pszMapName[0] )
	{
		return false;
	}

	char szFilename[MAX_PATH];
	char szPath[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ),
		"%s-shootout.ai", pszMapName );
	Q_snprintf( szPath, sizeof( szPath ),
		"fof_scripts/ai_editor/%s", szFilename );
	if ( !filesystem->FileExists( szPath, "GAME" ) )
	{
		DevMsg( "\n fof_sv_shootout_custom: no custom preset "
			"exist for %s!\n\n", pszMapName );
		return false;
	}

	static const char *s_pszCrateClasses[] =
	{
		"fof_crate",
		"fof_crate_med",
		"fof_crate_low",
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszCrateClasses ); ++i )
	{
		CBaseEntity *pCrate = NULL;
		while ( ( pCrate = gEntList.FindEntityByClassname(
			pCrate, s_pszCrateClasses[i] ) ) != NULL )
		{
			UTIL_Remove( pCrate );
		}
	}

	KeyValues *pRoot = new KeyValues( "PropList" );
	if ( !pRoot->LoadFromFile( filesystem, szPath, "GAME" ) )
	{
		pRoot->deleteThis();
		return false;
	}

	for ( KeyValues *pEntry = pRoot->GetFirstTrueSubKey(); pEntry;
		pEntry = pEntry->GetNextTrueSubKey() )
	{
		const Vector origin(
			pEntry->GetFloat( "origin_x", 0.0f ),
			pEntry->GetFloat( "origin_y", 0.0f ),
			pEntry->GetFloat( "origin_z", 0.0f ) );
		const Vector direction(
			pEntry->GetFloat( "dir_x", 0.0f ),
			pEntry->GetFloat( "dir_y", 0.0f ),
			pEntry->GetFloat( "dir_z", 0.0f ) );
		FoFCreateAIEditorProp(
			pEntry->GetString( "entity", "" ),
			pEntry->GetString( "data", "" ),
			origin, direction );
	}

	pRoot->deleteThis();
	DevMsg( "\n fof_sv_shootout_custom: custom preset created\n\n" );
	return true;
}

static void FoFRemoveAIEditorEntity(
	FoFAIEditorServerState_t &state, const CCommand &args )
{
	if ( args.ArgC() < 4 )
		return;

	const Vector target(
		static_cast< float >( Q_atoi( args[1] ) ),
		static_cast< float >( Q_atoi( args[2] ) ),
		static_cast< float >( Q_atoi( args[3] ) ) );
	for ( int i = 0; i < state.m_PlacedEntities.Count(); )
	{
		CBaseEntity *pEntity = state.m_PlacedEntities[i].Get();
		if ( !pEntity )
		{
			state.m_PlacedEntities.Remove( i );
			continue;
		}
		if ( ( pEntity->GetAbsOrigin() - target ).Length() >= 10.0f )
		{
			++i;
			continue;
		}

		if ( pEntity->IsPlayer() )
		{
			CBasePlayer *pBot = ToBasePlayer( pEntity );
			if ( pBot && pBot->IsBot() )
			{
				engine->ServerCommand( UTIL_VarArgs(
					"kickid %d\n", pBot->GetUserID() ) );
			}
		}
		else
		{
			UTIL_Remove( pEntity );
		}
		state.m_PlacedEntities.Remove( i );
		return;
	}
}

static void FoFRemoveAllAIEditorEntities(
	FoFAIEditorServerState_t &state )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pBot = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pBot && pBot->IsConnected() && pBot->IsBot() &&
			!pBot->IsFoFBotGhost() )
		{
			engine->ServerCommand( UTIL_VarArgs(
				"kickid %d\n", pBot->GetUserID() ) );
		}
	}

	for ( int i = 0; i < state.m_PlacedEntities.Count(); ++i )
	{
		CBaseEntity *pEntity = state.m_PlacedEntities[i].Get();
		if ( pEntity && !pEntity->IsPlayer() )
			UTIL_Remove( pEntity );
	}
	state.m_PlacedEntities.Purge();
}

bool FoFHandleAIEditorCommand(
	CFoF_Player *pPlayer, const CCommand &args )
{
	const char *pszCommand = args.ArgC() > 0 ? args[0] : "";
	if ( !FoFIsAIEditorCommand( pszCommand ) )
		return false;
	if ( engine->IsDedicatedServer() )
		return false;

	FoFAIEditorServerState_t *pState = FoFGetAIEditorState( pPlayer );
	if ( !pState )
		return true;

	if ( FStrEq( pszCommand, "ai_editor_start" ) )
	{
		FoFStartAIEditor( pPlayer );
	}
	else if ( FStrEq( pszCommand, "ai_editor_end" ) )
	{
		FoFEndAIEditor( pPlayer );
	}
	else if ( FStrEq( pszCommand, "select_bot" ) )
	{
		FoFSelectAIEditorEntry( *pState, args );
	}
	else if ( FStrEq( pszCommand, "create_bot" ) )
	{
		Vector origin, direction;
		if ( FoFReadEditorPlacement( args, origin, direction ) )
		{
			FoFSetAIEditorConVar( "fof_sv_bot_edit", 1 );
			CFoFBot *pBot = FoFPutConfiguredBotInServer(
				pState->m_BotProfile, origin, direction );
			if ( pBot )
			{
				pBot->AddSpawnFlags( 0x1000 );
				pState->m_PlacedEntities.AddToTail( pBot );
			}
		}
	}
	else if ( FStrEq( pszCommand, "create_prop" ) )
	{
		Vector origin, direction;
		if ( FoFReadEditorPlacement( args, origin, direction ) )
		{
			CBaseEntity *pProp = FoFCreateAIEditorProp(
				*pState, origin, direction );
			if ( pProp )
				pState->m_PlacedEntities.AddToTail( pProp );
		}
	}
	else if ( FStrEq( pszCommand, "remove_bot" ) )
	{
		FoFRemoveAIEditorEntity( *pState, args );
	}
	else if ( FStrEq( pszCommand, "remove_all_bots" ) )
	{
		FoFRemoveAllAIEditorEntities( *pState );
	}
	return true;
}
