//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server bot player entity.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_bot.h"
#include "hl2mp/hl2mp_playeranimstate.h"
#include "activitylist.h"
#include "gameinterface.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_crate.h"
#include "fof/fof_horse.h"
#include "fof/fof_player_equipment.h"
#include "fof/fof_player_shared.h"
#include "ai_basenpc.h"
#include "BasePropDoor.h"
#include "doors.h"
#include "filesystem.h"
#include "in_buttons.h"
#include "movehelper_server.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "props.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_bot, CFoFBot );

static const char *s_FoFBotNames[] =
{
	"BOT With No Name",
	"BOT Tuco",
	"BOT Django",
	"BOT Monco",
	"BOT Sentenza",
	"BOT Nobody",
	"BOT Sabata",
	"BOT Harmonica",
	"BOT Silence",
	"BOT Trinity",
	"BOT Colonel Mortimer",
	"BOT Sugar Colt",
	"BOT Cheyenne",
	"BOT Indio",
	"BOT Bill Carson",
	"BOT Frank",
	"BOT Angel Eyes",
	"BOT Keoma",
	"BOT Nino",
	"BOT Cuchillo",
	"BOT Bambino",
	"BOT Mezcal",
	"BOT El Diablo",
	"BOT Kowalski",
	"BOT Brad Fletcher",
	"BOT Colorado",
	"BOT Brokstone",
	"BOT Cassidy",
	"BOT\tNavajo Joe",
	"BOT Rattigan",
	"BOT Hank Fellows",
	"BOT Machete",
};

static const char *s_FoFGhostBotNames[] =
{
	"GHOST Narmer",
	"GHOST Hor-Aha",
	"GHOST Djer",
	"GHOST Djet",
	"GHOST Merneith",
	"GHOST Den",
	"GHOST Anedjib",
	"GHOST Semerkhet",
	"GHOST Qa'a",
	"GHOST Sneferka",
};

static int s_nNextFoFBotName;
static int s_nNextFoFGhostBotName;
static float s_flNextFoFGhostTownUpdate;
static float s_flLastFoFGhostTownUpdate;

static bool FoFIsValidBotTarget(
	CFoFBot *pBot, CFoF_Player *pTarget );
static bool FoFBotCanSeeTarget(
	CFoFBot *pBot, CFoF_Player *pTarget );
static void FoFUpdateBotWeaponHeuristics(
	CFoFBot *pBot, CBaseCombatWeapon *pWeapon );
static void FoFNotifyBotKilledTarget( CFoFBot *pBot );

static int FoFResolveBotTeam( int nRequestedTeam )
{
	// Course wave files use team 6 for the zombie player model and faction.
	// It is a real special team, but it is deliberately outside the ordinary
	// fof_sv_maxteams range used for automatic team balancing.
	if ( nRequestedTeam == FOF_TEAM_ZOMBIES )
		return nRequestedTeam;

	if ( !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
		return TEAM_UNASSIGNED;

	static ConVarRef maxTeams( "fof_sv_maxteams", true );
	const int nTeamCount = maxTeams.IsValid() ? maxTeams.GetInt() : 4;
	const int nLastTeam = clamp( nTeamCount, 1, 4 ) + 1;
	if ( nRequestedTeam >= 2 && nRequestedTeam <= nLastTeam )
		return nRequestedTeam;

	int nBestTeam = 2;
	int nBestCount = INT_MAX;
	for ( int nTeam = 2; nTeam <= nLastTeam; ++nTeam )
	{
		int nCount = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsConnected() &&
				pPlayer->GetTeamNumber() == nTeam )
			{
				++nCount;
			}
		}
		if ( nCount < nBestCount )
		{
			nBestCount = nCount;
			nBestTeam = nTeam;
		}
	}
	return nBestTeam;
}

static CBasePlayer *FoFClientPutInServerOverrideBot(
	edict_t *pEdict, const char *pszPlayerName )
{
	CFoFBot *pPlayer = dynamic_cast< CFoFBot * >(
		CHL2MP_Player::CreatePlayer( "fof_bot", pEdict ) );
	if ( pPlayer )
		pPlayer->SetPlayerName( pszPlayerName );
	return pPlayer;
}

static CFoFBot *FoFCreateBotEntity(
	const char *pszName, bool bFrozen )
{
	ClientPutInServerOverride( &FoFClientPutInServerOverrideBot );
	edict_t *pEdict = engine->CreateFakeClient( pszName );
	ClientPutInServerOverride( NULL );
	if ( !pEdict )
	{
		Msg( "Failed to create Bot.\n" );
		return NULL;
	}

	CFoFBot *pPlayer = dynamic_cast< CFoFBot * >(
		CBaseEntity::Instance( pEdict ) );
	if ( !pPlayer )
		return NULL;

	pPlayer->ClearFlags();
	pPlayer->AddFlag( FL_CLIENT | FL_FAKECLIENT );
	if ( bFrozen )
		pPlayer->AddEFlags( EFL_BOT_FROZEN );
	return pPlayer;
}

static bool FoFSpawnBotForTeam( CFoFBot *pPlayer, int nTeam )
{
	if ( !pPlayer )
		return false;

	if ( nTeam > TEAM_SPECTATOR )
		return pPlayer->HandleFoFTeamJoin( nTeam, true );

	if ( pPlayer->IsObserver() )
	{
		pPlayer->StopObserverMode();
		pPlayer->State_Transition( STATE_ACTIVE );
	}
	pPlayer->ChangeTeam( nTeam );
	return pPlayer->FinalizeFoFSpawn( true );
}

CFoFBot *FoFPutBotInServer( bool bFrozen, int nInitialHealth,
	const char *pszRequestedName, bool bPopulationManaged )
{
	FoFBotProfile_t profile;
	const bool bUseProfile =
		( !pszRequestedName || !pszRequestedName[0] ) &&
		FoFLoadBotProfile( s_nNextFoFBotName, profile );
	static ConVarRef editorActive( "fof_sv_bot_edit_active", true );
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bUseProfileName =
		( editorActive.IsValid() && editorActive.GetBool() ) ||
		( currentMode.IsValid() && currentMode.GetInt() == 6 );
	char szBotName[64];
	if ( pszRequestedName && pszRequestedName[0] )
	{
		Q_strncpy( szBotName, pszRequestedName, sizeof( szBotName ) );
	}
	else if ( bUseProfileName && bUseProfile && profile.m_szName[0] )
	{
		Q_strncpy( szBotName, profile.m_szName, sizeof( szBotName ) );
	}
	else
	{
		const char *pszName = s_FoFBotNames[
			( s_nNextFoFBotName + 1 ) % ARRAYSIZE( s_FoFBotNames )];
		Q_strncpy( szBotName, pszName, sizeof( szBotName ) );
	}

	CFoFBot *pPlayer = FoFCreateBotEntity( szBotName, bFrozen );
	if ( !pPlayer )
		return NULL;

	FoFConfigureBotRuntime(
		pPlayer, bUseProfile ? &profile : NULL, bPopulationManaged );
	pPlayer->RemoveAllItems( true );
	// Ordinary bots still read preset skill/equipment, but their roster names
	// are independent outside the editor/course. The server auto-balances the team.
	// The preset's bot_force_team field belongs to explicitly configured course
	// and editor bots; applying it here stacks the first BOT Tuco on team 2
	// with the host and disables both damage and target acquisition.
	FoFSpawnBotForTeam(
		pPlayer, FoFResolveBotTeam( TEAM_UNASSIGNED ) );
	if ( nInitialHealth > 0 )
		pPlayer->SetHealth( nInitialHealth );
	++s_nNextFoFBotName;
	return pPlayer;
}

CFoFBot *FoFPutConfiguredBotInServer(
	const FoFBotProfile_t &profile,
	const Vector &origin, const Vector &direction )
{
	char szBotName[64];
	if ( profile.m_szName[0] )
	{
		Q_strncpy( szBotName, profile.m_szName, sizeof( szBotName ) );
	}
	else
	{
		const char *pszName = s_FoFBotNames[
			s_nNextFoFBotName % ARRAYSIZE( s_FoFBotNames )];
		Q_strncpy( szBotName, pszName, sizeof( szBotName ) );
	}

	CFoFBot *pPlayer = FoFCreateBotEntity( szBotName, false );
	if ( !pPlayer )
		return NULL;

	FoFConfigureBotRuntime( pPlayer, &profile, false );
	pPlayer->RemoveAllItems( true );
	QAngle angles = vec3_angle;
	if ( direction.LengthSqr() > 0.0f )
	{
		VectorAngles( direction, angles );
		angles.x = clamp( AngleNormalize( angles.x ), -89.0f, 89.0f );
		angles.y = AngleNormalize( angles.y );
		angles.z = 0.0f;
	}

	if ( pPlayer->IsObserver() )
	{
		pPlayer->StopObserverMode();
		pPlayer->State_Transition( STATE_ACTIVE );
	}
	pPlayer->ChangeTeam(
		FoFResolveBotTeam( profile.m_nForceTeam ), true, false, true );
	if ( !pPlayer->FinalizeFoFSpawnAt( origin, angles ) )
		return NULL;

	++s_nNextFoFBotName;
	return pPlayer;
}

CFoFBot *FoFPutGhostBotInServer()
{
	const char *pszName = s_FoFGhostBotNames[
		s_nNextFoFGhostBotName % ARRAYSIZE( s_FoFGhostBotNames )];
	++s_nNextFoFGhostBotName;

	ClientPutInServerOverride( &FoFClientPutInServerOverrideBot );
	edict_t *pEdict = engine->CreateFakeClient( pszName );
	ClientPutInServerOverride( NULL );
	if ( !pEdict )
	{
		Msg( "Failed to create Ghost.\n" );
		return NULL;
	}

	CFoFBot *pPlayer = dynamic_cast< CFoFBot * >(
		CBaseEntity::Instance( pEdict ) );
	if ( !pPlayer )
		return NULL;

	pPlayer->ClearFlags();
	pPlayer->AddFlag( FL_CLIENT | FL_FAKECLIENT );
	pPlayer->SetFoFGhost( true );
	pPlayer->ChangeTeam( TEAM_UNASSIGNED );
	pPlayer->RemoveAllItems( true );
	pPlayer->FinalizeFoFSpawn( true );
	return pPlayer;
}

static CFoFBot *FoFFindGhostBot()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoFBot *pBot = dynamic_cast< CFoFBot * >( UTIL_PlayerByIndex( i ) );
		if ( pBot && pBot->IsConnected() && pBot->IsBot() &&
			pBot->IsFoFBotGhost() )
		{
			return pBot;
		}
	}

	return NULL;
}

void FoFUpdateGhostTownBots()
{
	static ConVarRef ghostTown( "fof_sv_ghost_town", true );
	if ( !ghostTown.IsValid() || !ghostTown.GetBool() ||
		!HL2MPRules() || !HL2MPRules()->IsTeamplay() )
	{
		return;
	}

	const float flNow = gpGlobals->curtime;
	if ( flNow >= s_flLastFoFGhostTownUpdate &&
		flNow < s_flNextFoFGhostTownUpdate )
	{
		return;
	}
	s_flLastFoFGhostTownUpdate = flNow;
	s_flNextFoFGhostTownUpdate = flNow + 0.25f;

	int nPlayerCount = 0;
	int nGhostCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;

		++nPlayerCount;
		if ( pPlayer->IsBot() && pPlayer->IsFoFBotGhost() )
			++nGhostCount;
	}
	if ( nPlayerCount == 0 )
		return;

	const float flLastPopulatedSlot =
		MAX( 1.0f, static_cast< float >( gpGlobals->maxClients - 1 ) );
	const float flDesiredGhosts = RemapValClamped(
		static_cast< float >( nPlayerCount ),
		0.0f, flLastPopulatedSlot,
		static_cast< float >( gpGlobals->maxClients ) * 0.2f, 0.0f );
	const int nDesiredGhosts = static_cast< int >( flDesiredGhosts );

	if ( nGhostCount < nDesiredGhosts )
	{
		FoFPutGhostBotInServer();
	}
	else if ( nGhostCount > nDesiredGhosts )
	{
		CFoFBot *pBot = FoFFindGhostBot();
		if ( pBot )
		{
			engine->ServerCommand( UTIL_VarArgs(
				"kickid %d \"Ghost Town population\"\n",
				pBot->GetUserID() ) );
		}
	}
}

CON_COMMAND_F( bot, "Add a bot.", FCVAR_CHEAT )
{
	int nCount = clamp( args.FindArgInt( "-count", 1 ), 1, 16 );
	const bool bFrozen = args.FindArg( "-frozen" ) != NULL;
	const int nInitialHealth = clamp(
		args.FindArgInt( "-health", 0 ), 0, 10000 );
	while ( nCount-- > 0 )
		FoFPutBotInServer( bFrozen, nInitialHealth, NULL );
}

CFoFBot::CFoFBot()
	: m_bFoFGhost( false )
{
}

void CFoFBot::Spawn()
{
	FoFResetBotSpawnState( this );
	SetFoFBotGhost( m_bFoFGhost );
	BaseClass::Spawn();
	SetFoFBotGhost( m_bFoFGhost );
}

int CFoFBot::OnTakeDamage( const CTakeDamageInfo &info )
{
	CFoF_Player *pAttacker = ToFoFPlayer( info.GetAttacker() );
	const int nGhostDamage = DMG_BURN | DMG_FALL | DMG_BLAST;
	if ( pAttacker && m_bFoFGhost &&
		( info.GetDamageType() & nGhostDamage ) == 0 )
	{
		return 0;
	}

	return BaseClass::OnTakeDamage( info );
}

int CFoFBot::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	FoFNotifyBotDamaged( this, ToFoFPlayer( info.GetAttacker() ) );
	return BaseClass::OnTakeDamage_Alive( info );
}

void CFoFBot::Event_Killed( const CTakeDamageInfo &info )
{
	CFoFBot *pAttacker = dynamic_cast< CFoFBot * >( info.GetAttacker() );
	if ( pAttacker && pAttacker->IsBot() && pAttacker->IsAlive() &&
		FoFPlayersAreEnemies( pAttacker, this ) )
	{
		FoFNotifyBotKilledTarget( pAttacker );
	}
	BaseClass::Event_Killed( info );
}

bool CFoFBot::Weapon_Switch(
	CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	if ( GetTeamNumber() == FOF_TEAM_ZOMBIES && pWeapon &&
		!FClassnameIs( pWeapon, "weapon_fists" ) )
	{
		return false;
	}

	// FoF updates the bot's attack styles/range and clears its current
	// weapon action before delegating to the player implementation.
	FoFUpdateBotWeaponHeuristics( this, pWeapon );
	return BaseClass::Weapon_Switch( pWeapon, viewmodelindex );
}

bool CFoFBot::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	if ( FoFIsCourseCompanionBot( this ) )
		return false;
	return BaseClass::BumpWeapon( pWeapon );
}

void CFoFBot::DelayFoFNextAction( void )
{
	// The original helper writes only the next-action clock.
	FoFDelayBotNextAction( this, 0.35f );
}

void CFoFBot::RefreshFoFEquipment( void )
{
	if ( !IsAlive() )
		return;

	int scriptedItems[4];
	if ( FoFGetBotProfileEquipment(
		this, scriptedItems, ARRAYSIZE( scriptedItems ) ) )
	{
		SetFoFEquipmentSelection(
			scriptedItems, ARRAYSIZE( scriptedItems ) );
		ApplyFoFEquipmentSelection();
		return;
	}

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	if ( pActiveWeapon && ( nMode == 3 ?
		pActiveWeapon->FoFWeaponID() != 0 : pActiveWeapon->CanDeploy() ) )
	{
		return;
	}
	const float flCash = GetFoFCash();
	if ( nMode == 3 && flCash < 50.0f &&
		random->RandomInt( 0, 100 ) > 30 )
	{
		return;
	}

	int items[2] = { -1, -1 };
	int nItemCount = 0;
	const int nRoll = random->RandomInt( 0, 1000 );
	if ( nMode == 2 && flCash >= 100.0f )
	{
		if ( nRoll <= 400 )
		{
			items[nItemCount++] = 21;
			items[nItemCount++] = 2;
		}
		else if ( nRoll <= 800 )
		{
			items[nItemCount++] = 4;
		}
		else
		{
			items[nItemCount++] = 29;
		}
	}
	else if ( nMode == 2 && flCash >= 75.0f )
	{
		if ( nRoll <= 400 )
		{
			items[nItemCount++] = 4;
		}
		else
		{
			items[nItemCount++] = 21;
			items[nItemCount++] = 1;
		}
	}
	else if ( flCash >= 50.0f )
	{
		if ( nRoll <= 400 )
		{
			items[nItemCount++] = 28;
			items[nItemCount++] = 28;
		}
		else
		{
			items[nItemCount++] = 31;
		}
	}
	else if ( flCash >= 35.0f )
	{
		if ( nRoll <= 400 )
			items[nItemCount++] = 28;
		else if ( nRoll <= 600 )
			items[nItemCount++] = 11;
		else
		{
			items[nItemCount++] = 1;
			items[nItemCount++] = 2;
		}
	}
	else if ( flCash >= 20.0f )
	{
		if ( nRoll <= 400 )
			items[nItemCount++] = 28;
		else if ( nRoll <= 700 )
			items[nItemCount++] = 2;
		else
			items[nItemCount++] = 19;
	}

	SetFoFEquipmentSelection( items, nItemCount );
	ApplyFoFEquipmentSelection();
}

void CFoFBot::ActivateFoFInvulnerability( void )
{
	BaseClass::ActivateFoFInvulnerability();
	FoFClearBotTarget( this );
}

void CFoFBot::SelectFoFEquipment( void )
{
	if ( IsFoFBotGhost() )
		return;

	int scriptedItems[4];
	const bool bScripted = FoFGetBotProfileEquipment(
		this, scriptedItems, ARRAYSIZE( scriptedItems ) );
	static ConVarRef weaponMenu( "fof_sv_weaponmenu", true );
	if ( !bScripted && weaponMenu.IsValid() && !weaponMenu.GetBool() )
		return;

	if ( bScripted )
	{
		SetFoFEquipmentSelection(
			scriptedItems, ARRAYSIZE( scriptedItems ) );
		return;
	}

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	const bool bTeamplay = HL2MPRules() && HL2MPRules()->IsTeamplay();
	int items[3] = { -1, -1, -1 };
	int nItemCount = 0;
	const int nRoll = random->RandomInt( 0, 1000 );

	if ( !bTeamplay || nMode == 4 )
	{
		if ( nMode != 1 && nMode != 4 )
			return;

		if ( nRoll > 950 )
		{
			items[nItemCount++] = 11;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 850 )
		{
			items[nItemCount++] = 19;
			items[nItemCount++] = 20;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 500 )
		{
			items[nItemCount++] = 2;
			items[nItemCount++] = 20;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 100 )
		{
			items[nItemCount++] = 28;
			items[nItemCount++] = 34;
			items[nItemCount++] = 1;
		}
		else
		{
			items[nItemCount++] = 13;
			items[nItemCount++] = 34;
			items[nItemCount++] = 1;
		}
	}
	else
	{
		if ( nMode != 1 )
			return;

		if ( nRoll > 900 )
		{
			items[nItemCount++] = 33;
			items[nItemCount++] = 20;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 800 )
		{
			items[nItemCount++] = 11;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 750 )
		{
			items[nItemCount++] = 30;
			items[nItemCount++] = 20;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 500 )
		{
			items[nItemCount++] = 2;
			items[nItemCount++] = 20;
			items[nItemCount++] = 1;
		}
		else if ( nRoll > 100 )
		{
			items[nItemCount++] = 28;
			items[nItemCount++] = 34;
			items[nItemCount++] = 1;
		}
		else
		{
			items[nItemCount++] = 1;
		}
	}

	SetFoFEquipmentSelection( items, nItemCount );
}

void CFoFBot::SetFoFGhost( bool bGhost )
{
	m_bFoFGhost = bGhost;
	SetFoFBotGhost( bGhost );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server bot profiles, per-frame commands and population management.
//
//=============================================================================//

static const int FOF_BOT_MAX_PATH_POINTS = 512;
static const int FOF_BOT_MAX_ACTIONS = 8;

enum FoFBotObjective_t
{
	FOF_BOT_OBJECTIVE_NONE = 0,
	FOF_BOT_OBJECTIVE_CRATE,
	FOF_BOT_OBJECTIVE_HORSE,
	FOF_BOT_OBJECTIVE_MODE
};

enum FoFBotActionType_t
{
	FOF_BOT_ACTION_NONE = 0,
	FOF_BOT_ACTION_ATTACK = 1,
	FOF_BOT_ACTION_HIDE = 3,
	FOF_BOT_ACTION_OPEN_CRATE = 4,
	FOF_BOT_ACTION_CROUCH_WAIT = 5,
	FOF_BOT_ACTION_UNSTUCK = 6,
	FOF_BOT_ACTION_FOLLOW_PATH = 7,
	FOF_BOT_ACTION_SUPPORT_TEAMMATE = 8,
	FOF_BOT_ACTION_ROAM_PATH = 9,
	FOF_BOT_ACTION_ROAM_LOOK = 10,
	FOF_BOT_ACTION_ADVANCE = 11,
	FOF_BOT_ACTION_COURSE_HOLD = 12,
	FOF_BOT_ACTION_HIDE_PATH = 13,
	FOF_BOT_ACTION_CHASE_PATH = 14,
	FOF_BOT_ACTION_ROAM_LONG = 15
};

struct FoFBotAction_t
{
	FoFBotAction_t() :
		m_nType( FOF_BOT_ACTION_NONE ),
		m_bStarted( false ),
		m_flEndTime( 0.0f )
	{
		m_vecGoal.Init();
	}

	int m_nType;
	bool m_bStarted;
	float m_flEndTime;
	Vector m_vecGoal;
	CHandle< CBaseEntity > m_hEntity;
};

struct FoFBotRuntimeState_t
{
	FoFBotRuntimeState_t()
		: m_bConfigured( false ),
		  m_bPopulationManaged( false ),
		  m_bEquipmentSelected( false ),
		  m_bEquipmentRefreshComplete( false ),
		  m_bHasCachedCommand( false ),
		  m_nObjective( FOF_BOT_OBJECTIVE_NONE ),
		  m_flNextTargetSearch( 0.0f ),
		  m_flNextObjectiveSearch( 0.0f ),
		  m_flObjectiveExpireTime( 0.0f ),
		  m_flNextInteractionCheck( 0.0f ),
		  m_flNextSupportSearch( 0.0f ),
		  m_flNextStrafeChange( 0.0f ),
		  m_flStrafeEnd( 0.0f ),
		  m_flNextWanderTurn( 0.0f ),
		  m_flNextShot( 0.0f ),
		  m_flChargeRelease( 0.0f ),
		  m_flNextWeaponCheck( 0.0f ),
		  m_flNextWeaponSelection( 0.0f ),
		  m_flNextStuckCheck( 0.0f ),
		  m_flNextPathBuild( 0.0f ),
		  m_flNextWanderGoal( 0.0f ),
		  m_flNextKick( 0.0f ),
		  m_flNextObstacleInteraction( 0.0f ),
		  m_flCourseRemovalAt( 0.0f ),
		  m_flStuckRecoveryEnd( 0.0f ),
		  m_flStuckRecoverySide( 0.0f ),
		  m_flStrafeMove( 0.0f ),
		  m_flStrafeBias( 0.0f ),
		  m_flWanderYaw( 0.0f ),
		  m_flAimPitchNoise( 0.0f ),
		  m_flAimYawNoise( 0.0f ),
		  m_flViewPitchVelocity( 0.0f ),
		  m_flViewYawVelocity( 0.0f ),
		  m_flAimOnTargetTime( 0.0f ),
		  m_flProfileRotationSpeed( 7.5f ),
		  m_flProfileShootDelay( 2.9f ),
		  m_flProfileAimTrailing( 0.85f ),
		  m_flProfileStrafe( 5.0f ),
		  m_flProfileAggression( 7.0f ),
		  m_flProfileAverageSkill( 5.0f ),
		  m_flWeaponMinRange( 0.0f ),
		  m_flWeaponRange( 500.0f ),
		  m_nPrimaryAttackStyle( 3 ),
		  m_nSecondaryAttackStyle( 3 ),
		  m_bHasDynamiteThrowVelocity( false ),
		  m_bCourseCompanion( false ),
		  m_bCourseRemovalQueued( false ),
		  m_bChargingAttack( false ),
		  m_bRoaming( false ),
		  m_bPathToTarget( false ),
		  m_nCommandNumber( 0 ),
		  m_nCourseOrder( -1 ),
		  m_nCourseGlowBand( 0 ),
		  m_nPendingCourseGlowBand( -1 ),
		  m_flCourseGlowReenableTime( 0.0f ),
		  m_flCourseOrderRadius( 0.0f ),
		  m_nActionCount( 0 ),
		  m_nPathCount( 0 ),
		  m_nPathIndex( 0 ),
		  m_nStuckChecks( 0 )
	{
		m_CachedCommand.Reset();
		m_vecLastPosition.Init();
		m_vecPathGoal.Init();
		m_vecWanderGoal.Init();
		m_vecCourseCompanionHome.Init();
		m_vecCourseFollowAnchor.Init();
		m_vecDynamiteThrowVelocity.Init();
		m_flNextCombatVoice = 0.0f;
		m_flNextAimedShot = 0.0f;
		for ( int i = 0; i < ARRAYSIZE( m_nEquipment ); ++i )
			m_nEquipment[i] = -1;
		Q_memset( m_nPathTraverse, NUM_TRAVERSE_TYPES,
			sizeof( m_nPathTraverse ) );
	}

	CHandle< CFoFBot > m_hBot;
	CHandle< CFoF_Player > m_hTarget;
	CHandle< CFoF_Player > m_hSupportTarget;
	FoFBotProfile_t m_Profile;
	bool m_bConfigured;
	bool m_bPopulationManaged;
	bool m_bEquipmentSelected;
	bool m_bEquipmentRefreshComplete;
	bool m_bHasCachedCommand;
	CUserCmd m_CachedCommand;
	int m_nObjective;
	CHandle< FoF_Crate > m_hObjectiveCrate;
	CHandle< CFoF_Horse > m_hObjectiveHorse;
	CHandle< CBaseEntity > m_hObjectiveEntity;
	CHandle< CBaseEntity > m_hBlockingObstacle;
	float m_flNextTargetSearch;
	float m_flNextObjectiveSearch;
	float m_flObjectiveExpireTime;
	float m_flNextInteractionCheck;
	float m_flNextSupportSearch;
	float m_flNextStrafeChange;
	float m_flStrafeEnd;
	float m_flNextWanderTurn;
	float m_flNextShot;
	float m_flChargeRelease;
	float m_flNextWeaponCheck;
	float m_flNextWeaponSelection;
	float m_flNextStuckCheck;
	float m_flNextPathBuild;
	float m_flNextWanderGoal;
	float m_flNextKick;
	float m_flNextObstacleInteraction;
	float m_flCourseRemovalAt;
	float m_flStuckRecoveryEnd;
	float m_flStuckRecoverySide;
	float m_flStrafeMove;
	float m_flStrafeBias;
	float m_flWanderYaw;
	float m_flAimPitchNoise;
	float m_flAimYawNoise;
	float m_flViewPitchVelocity;
	float m_flViewYawVelocity;
	float m_flAimOnTargetTime;
	float m_flProfileRotationSpeed;
	float m_flProfileShootDelay;
	float m_flProfileAimTrailing;
	float m_flProfileStrafe;
	float m_flProfileAggression;
	float m_flProfileAverageSkill;
	float m_flWeaponMinRange;
	float m_flWeaponRange;
	int m_nPrimaryAttackStyle;
	int m_nSecondaryAttackStyle;
	bool m_bHasDynamiteThrowVelocity;
	bool m_bCourseCompanion;
	bool m_bCourseRemovalQueued;
	bool m_bChargingAttack;
	bool m_bRoaming;
	bool m_bPathToTarget;
	int m_nCommandNumber;
	int m_nCourseOrder;
	int m_nCourseGlowBand;
	int m_nPendingCourseGlowBand;
	float m_flCourseGlowReenableTime;
	float m_flCourseOrderRadius;
	int m_nActionCount;
	int m_nPathCount;
	int m_nPathIndex;
	int m_nStuckChecks;
	Vector m_vecLastPosition;
	Vector m_vecPathGoal;
	Vector m_vecWanderGoal;
	Vector m_vecCourseCompanionHome;
	Vector m_vecCourseFollowAnchor;
	Vector m_vecDynamiteThrowVelocity;
	Vector m_vecPath[FOF_BOT_MAX_PATH_POINTS];
	unsigned char m_nPathTraverse[FOF_BOT_MAX_PATH_POINTS];
	FoFBotAction_t m_Actions[FOF_BOT_MAX_ACTIONS];
	int m_nEquipment[4];
	float m_flNextCombatVoice;
	float m_flNextAimedShot;
};

static FoFBotRuntimeState_t s_FoFBotRuntime[MAX_PLAYERS + 1];
static float s_flNextFoFBotPopulationUpdate;

static float FoFBotMoveSpeed(
	CFoFBot *pBot, const FoFBotRuntimeState_t &state )
{
	NOTE_UNUSED( state );
	return MAX( 1.0f, pBot->GetPlayerMaxSpeed() );
}

static bool FoFIsConfiguredCourseBot(
	const FoFBotRuntimeState_t &state )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return state.m_bConfigured && currentMode.IsValid() &&
		currentMode.GetInt() == 6;
}

static float FoFUnitProfileValue( int nValue )
{
	return clamp( static_cast< float >( nValue ) * 0.1f, 0.0f, 1.0f );
}

static void FoFUpdateBotProfileValues( FoFBotRuntimeState_t &state )
{
	if ( FoFIsConfiguredCourseBot( state ) )
	{
		// The course files store five integer editor sliders.  The shipped bot
		// constructor converts them before any movement or attack code reads
		// them; using the raw integers here made low-skill course bots turn and
		// fire much faster than the original.
		state.m_flProfileRotationSpeed =
			( FoFUnitProfileValue( state.m_Profile.m_nRotationSpeed ) + 1.0f ) * 5.0f;
		state.m_flProfileShootDelay = 5.0f -
			FoFUnitProfileValue( state.m_Profile.m_nShootDelay ) * 4.2f;
		state.m_flProfileAimTrailing = 1.2f -
			FoFUnitProfileValue( state.m_Profile.m_nAimTrailing ) * 0.7f;
		state.m_flProfileStrafe = clamp(
			static_cast< float >( state.m_Profile.m_nStrafe ), 1.0f, 10.0f );
		state.m_flProfileAggression = clamp(
			static_cast< float >( state.m_Profile.m_nAggression ), 1.0f, 10.0f );
		state.m_flProfileAverageSkill =
			static_cast< float >(
				state.m_Profile.m_nRotationSpeed +
				state.m_Profile.m_nShootDelay +
				state.m_Profile.m_nAimTrailing +
				state.m_Profile.m_nStrafe ) * 0.25f;
		return;
	}

	// Ordinary FoF bots receive these runtime ranges independently of the
	// course editor sliders.
	state.m_flProfileRotationSpeed = random->RandomFloat( 6.5f, 8.5f );
	state.m_flProfileShootDelay = random->RandomFloat( 1.85f, 3.5f );
	state.m_flProfileAimTrailing = random->RandomFloat( 0.65f, 0.8f );
	state.m_flProfileStrafe = random->RandomFloat( 4.0f, 7.0f );
	state.m_flProfileAggression = random->RandomFloat( 6.0f, 8.0f );
	state.m_flProfileAverageSkill =
		( state.m_flProfileRotationSpeed + state.m_flProfileShootDelay +
		  state.m_flProfileAimTrailing + state.m_flProfileStrafe ) * 0.25f;
}

static void FoFResetBotMapState()
{
	for ( int i = 0; i < ARRAYSIZE( s_FoFBotRuntime ); ++i )
		s_FoFBotRuntime[i] = FoFBotRuntimeState_t();

	s_flNextFoFBotPopulationUpdate = 0.0f;
	s_flNextFoFGhostTownUpdate = 0.0f;
	s_flLastFoFGhostTownUpdate = 0.0f;
}

class CFoFBotMapStateSystem : public CAutoGameSystem
{
public:
	CFoFBotMapStateSystem() :
		CAutoGameSystem( "CFoFBotMapStateSystem" )
	{
	}

	virtual void LevelInitPreEntity()
	{
		FoFResetBotMapState();
	}

	virtual void LevelShutdownPostEntity()
	{
		FoFResetBotMapState();
	}
};

static CFoFBotMapStateSystem s_FoFBotMapStateSystem;

FoFBotProfile_t::FoFBotProfile_t()
	: m_nRotationSpeed( 5 ),
	  m_nShootDelay( 5 ),
	  m_nAimTrailing( 5 ),
	  m_nStrafe( 5 ),
	  m_nForceTeam( 0 ),
	  m_nAggression( 5 )
{
	m_szName[0] = '\0';
	m_szEquipment[0] = '\0';
}

static FoFBotRuntimeState_t *FoFGetBotRuntimeState( CFoFBot *pBot )
{
	if ( !pBot )
		return NULL;

	const int nIndex = pBot->entindex();
	if ( nIndex < 1 || nIndex > MAX_PLAYERS )
		return NULL;

	FoFBotRuntimeState_t &state = s_FoFBotRuntime[nIndex];
	if ( state.m_hBot.Get() != pBot )
		return NULL;
	return &state;
}

void FoFSetBotDynamiteThrowVelocity(
	CFoF_Player *pPlayer, const Vector &vecVelocity )
{
	CFoFBot *pBot = dynamic_cast< CFoFBot * >( pPlayer );
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;
	pState->m_vecDynamiteThrowVelocity = vecVelocity;
	pState->m_bHasDynamiteThrowVelocity = vecVelocity.LengthSqr() > 1.0f;
}

bool FoFConsumeBotDynamiteThrowVelocity(
	CFoF_Player *pPlayer, Vector &vecVelocity )
{
	CFoFBot *pBot = dynamic_cast< CFoFBot * >( pPlayer );
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState || !pState->m_bHasDynamiteThrowVelocity )
		return false;

	vecVelocity = pState->m_vecDynamiteThrowVelocity;
	pState->m_vecDynamiteThrowVelocity.Init();
	pState->m_bHasDynamiteThrowVelocity = false;
	return true;
}

CFoFBot *FoFPutCourseCompanionInServer(
	int nTeam, const Vector &origin, const QAngle &angles,
	int nRemaining )
{
	FoFBotProfile_t profile;
	profile.m_nRotationSpeed = 8;
	profile.m_nShootDelay = 8;
	profile.m_nAimTrailing = 8;
	profile.m_nStrafe = 8;
	profile.m_nForceTeam = nTeam;
	profile.m_nAggression = 8;
	Q_strncpy( profile.m_szName, "helper", sizeof( profile.m_szName ) );
	const char *pszEquipment = "2,2,-1,-1,";
	if ( nRemaining == 1 )
		pszEquipment = "2,21,-1,-1,";
	else if ( nRemaining == 2 )
		pszEquipment = "11,2,-1,-1,";
	else if ( nRemaining == 3 )
		pszEquipment = "16,2,-1,-1,";
	Q_strncpy( profile.m_szEquipment, pszEquipment,
		sizeof( profile.m_szEquipment ) );

	// The course helper uses the literal fake-client name "bot"; the engine
	// supplies duplicate-name suffixes while the internal profile remains
	// "helper".
	CFoFBot *pPlayer = FoFCreateBotEntity( "bot", false );
	if ( !pPlayer )
		return NULL;

	FoFConfigureBotRuntime( pPlayer, &profile, false );
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pPlayer );
	if ( pState )
	{
		pState->m_bCourseCompanion = true;
		pState->m_vecCourseCompanionHome = origin;
	}
	pPlayer->RemoveAllItems( true );
	if ( pPlayer->IsObserver() )
	{
		pPlayer->StopObserverMode();
		pPlayer->State_Transition( STATE_ACTIVE );
	}
	pPlayer->ChangeTeam( nTeam, true, false, true );
	if ( !pPlayer->FinalizeFoFSpawnAt( origin, angles ) )
	{
		engine->ServerCommand( UTIL_VarArgs(
			"kickid %d \"Course companion spawn failed\"\n",
			pPlayer->GetUserID() ) );
		return NULL;
	}

	return pPlayer;
}

bool FoFIsCourseCompanionBot( const CFoF_Player *pPlayer )
{
	CFoFBot *pBot = dynamic_cast< CFoFBot * >(
		const_cast< CFoF_Player * >( pPlayer ) );
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	return pState && pState->m_bCourseCompanion;
}

static void FoFUpdateCourseCompanionGlow(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
#ifdef GLOWS_ENABLE
	if ( !pBot || !state.m_bCourseCompanion )
		return;

	const int nDesiredBand = !pBot->IsAlive() || pBot->GetHealth() >= 60 ?
		0 : pBot->GetHealth() > 30 ? 1 : 2;
	if ( state.m_nPendingCourseGlowBand >= 0 )
	{
		if ( state.m_nPendingCourseGlowBand == nDesiredBand )
		{
			if ( gpGlobals->curtime < state.m_flCourseGlowReenableTime )
				return;
			if ( nDesiredBand > 0 && !pBot->IsGlowEffectActive() )
				pBot->AddGlowEffect();
			state.m_nCourseGlowBand = nDesiredBand;
			state.m_nPendingCourseGlowBand = -1;
			state.m_flCourseGlowReenableTime = 0.0f;
			return;
		}
		state.m_nPendingCourseGlowBand = -1;
		state.m_flCourseGlowReenableTime = 0.0f;
	}

	if ( state.m_nCourseGlowBand != nDesiredBand )
	{
		if ( pBot->IsGlowEffectActive() )
		{
			pBot->RemoveGlowEffect();
			state.m_nCourseGlowBand = 0;
			if ( nDesiredBand > 0 )
			{
				state.m_nPendingCourseGlowBand = nDesiredBand;
				state.m_flCourseGlowReenableTime =
					gpGlobals->curtime + 0.1f;
			}
			return;
		}

		if ( nDesiredBand > 0 )
			pBot->AddGlowEffect();
		state.m_nCourseGlowBand = nDesiredBand;
	}
	else if ( nDesiredBand == 0 && pBot->IsGlowEffectActive() )
	{
		pBot->RemoveGlowEffect();
	}
	else if ( nDesiredBand > 0 && !pBot->IsGlowEffectActive() )
	{
		pBot->AddGlowEffect();
	}
#else
	NOTE_UNUSED( pBot );
	NOTE_UNUSED( state );
#endif
}

static void FoFNotifyBotKilledTarget( CFoFBot *pBot )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( pState )
		pState->m_flNextTargetSearch = 0.0f;
}

struct FoFBotWeaponHeuristic_t
{
	const char *m_pszClassname;
	float m_flPrimaryMinRange;
	float m_flPrimaryMaxRange;
	float m_flSecondaryMinRange;
	float m_flSecondaryMaxRange;
	int m_nPriority;
	int m_nPrimaryAttackStyle;
	int m_nSecondaryAttackStyle;
	bool m_bSecondSelection;
	bool m_bCanDualWield;
};

// Gameplay fields from the shipped 41-record bot weapon table: primary and
// secondary ranges, selection priority, attack styles and hand capabilities.
static const FoFBotWeaponHeuristic_t s_FoFBotWeaponHeuristics[] =
{
	{ "weapon_fists",               0,   50, 0,   55,  1, 1, 1, false, false },
	{ "weapon_fists_ghost",         0,   50, 0,   55,  1, 1, 1, false, false },
	{ "weapon_knife",               0,   53, 0,  150,  2, 1, 2, false, false },
	{ "weapon_axe",                 0,   56, 0,  150,  4, 1, 2, false, false },
	{ "weapon_machete",             0,   60, 0,  150,  5, 1, 2, false, false },
	{ "weapon_deringer",            0,  450, 0,    0,  3, 3, 0, false, false },
	{ "weapon_deringer2",           0,  450, 0,    0,  3, 3, 0, true,  false },
	{ "weapon_hammerless",          0,  450, 0,    0,  4, 3, 0, false, false },
	{ "weapon_hammerless2",         0,  450, 0,    0,  4, 3, 0, true,  false },
	{ "weapon_coltnavy",            0,  700, 0,  150,  5, 3, 0, false, true  },
	{ "weapon_coltnavy2",           0,  700, 0,  150,  5, 3, 0, true,  true  },
	{ "weapon_remington_army",      0,  850, 0,  150,  6, 3, 0, false, true  },
	{ "weapon_remington_army2",     0,  850, 0,  150,  6, 3, 0, false, true  },
	{ "weapon_schofield",           0,  850, 0,  150,  6, 3, 0, false, true  },
	{ "weapon_schofield2",          0,  850, 0,  150,  6, 3, 0, true,  true  },
	{ "weapon_peacemaker",          0,  850, 0,  150,  7, 3, 0, false, true  },
	{ "weapon_peacemaker2",         0,  850, 0,  150,  7, 3, 0, true,  true  },
	{ "weapon_walker",              0,  900, 0, 1000,  8, 3, 0, false, false },
	{ "weapon_walker2",             0,  900, 0, 1000,  8, 3, 0, true,  false },
	{ "weapon_mauser",              0,  900, 0, 1000,  8, 3, 0, false, false },
	{ "weapon_mauser2",             0,  900, 0, 1000,  8, 3, 0, true,  false },
	{ "weapon_maresleg",            0,  750, 0,  750,  6, 3, 0, false, false },
	{ "weapon_maresleg2",           0,  750, 0,  750,  6, 3, 0, true,  false },
	{ "weapon_volcanic",            0,  500, 0,  450,  4, 3, 0, false, false },
	{ "weapon_volcanic2",           0,  500, 0,  450,  4, 3, 0, true,  false },
	{ "weapon_sawedoff_shotgun",    0,  500, 0,  450,  4, 3, 0, false, false },
	{ "weapon_sawedoff_shotgun2",   0,  500, 0,  450,  4, 3, 0, true,  false },
	{ "weapon_carbine",             0, 2000, 0,  750,  7, 5, 0, false, false },
	{ "weapon_henryrifle",          0, 2000, 0,  750,  8, 5, 0, false, false },
	{ "weapon_sharps",              0, 3500, 0,  750,  9, 5, 0, false, false },
	{ "weapon_spencer",             0, 1800, 0,  750,  7, 5, 0, false, false },
	{ "weapon_bow",                 0, 2000, 0, 1500,  6, 5, 0, false, false },
	{ "weapon_bow_black",           0, 2000, 0, 1500,  6, 5, 0, false, false },
	{ "weapon_xbow",                0, 1000, 0,  800, 10, 5, 0, false, false },
	{ "weapon_coachgun",            0,  850, 0,  850,  6, 4, 0, false, false },
	{ "weapon_shotgun",             0,  550, 0,  750,  7, 4, 0, false, false },
	{ "weapon_dynamite",          200, 1200, 0,    0,  4, 7, 0, false, false },
	{ "weapon_dynamite_black",    300, 1200, 0,    0,  5, 7, 0, false, false },
	{ "weapon_dynamite_belt",     100,  600, 0,    0,  6, 7, 0, false, false },
	{ "weapon_whiskey",             0,   80, 0,    0,  4, 3, 0, false, false },
	{ "weapon_whiskey2",            0,   80, 0,    0,  4, 3, 0, true,  false },
};

static const FoFBotWeaponHeuristic_t *FoFFindBotWeaponHeuristic(
	const char *pszClassname )
{
	if ( !pszClassname )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( s_FoFBotWeaponHeuristics ); ++i )
	{
		if ( !Q_stricmp( pszClassname,
			s_FoFBotWeaponHeuristics[i].m_pszClassname ) )
		{
			return &s_FoFBotWeaponHeuristics[i];
		}
	}
	return NULL;
}

static void FoFUpdateBotWeaponHeuristics(
	CFoFBot *pBot, CBaseCombatWeapon *pWeapon )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;

	pState->m_bChargingAttack = false;
	pState->m_bHasDynamiteThrowVelocity = false;
	pState->m_vecDynamiteThrowVelocity.Init();
	pState->m_flChargeRelease = 0.0f;
	if ( !pWeapon )
	{
		pState->m_flWeaponMinRange = 0.0f;
		pState->m_flWeaponRange = 0.0f;
		return;
	}

	const FoFBotWeaponHeuristic_t *pHeuristic =
		FoFFindBotWeaponHeuristic( pWeapon->GetClassname() );
	if ( !pHeuristic )
	{
		pState->m_nPrimaryAttackStyle = 3;
		pState->m_nSecondaryAttackStyle = 0;
		pState->m_flWeaponMinRange = 0.0f;
		if ( pWeapon->FoFWeaponID() != 6 )
			pState->m_flWeaponRange = 500.0f;
		return;
	}

	pState->m_nPrimaryAttackStyle = pHeuristic->m_nPrimaryAttackStyle;
	pState->m_nSecondaryAttackStyle = pHeuristic->m_nSecondaryAttackStyle;
	// Whiskey (weapon ID 6) deliberately preserves the previous range in the
	// shipped override while still updating the attack styles.
	if ( pWeapon->FoFWeaponID() == 6 )
		return;

	pState->m_flWeaponMinRange = pHeuristic->m_flPrimaryMinRange;
	pState->m_flWeaponRange = pHeuristic->m_flPrimaryMaxRange;
	if ( pState->m_nPrimaryAttackStyle == 1 )
	{
		const float flMeleeScale = RemapValClamped(
			pState->m_flProfileAggression,
			1.0f, 10.0f, 0.5f, 1.0f );
		pState->m_flWeaponRange = clamp(
			pState->m_flWeaponRange * flMeleeScale, 33.0f, 100.0f );
	}
}

static void FoFClearBotPath( FoFBotRuntimeState_t &state )
{
	state.m_nPathCount = 0;
	state.m_nPathIndex = 0;
}

static void FoFClearBotObjective(
	FoFBotRuntimeState_t &state, bool bClearPath )
{
	state.m_nObjective = FOF_BOT_OBJECTIVE_NONE;
	state.m_hObjectiveCrate = NULL;
	state.m_hObjectiveHorse = NULL;
	state.m_hObjectiveEntity = NULL;
	state.m_flObjectiveExpireTime = 0.0f;
	if ( bClearPath )
	{
		FoFClearBotPath( state );
		state.m_flNextPathBuild = 0.0f;
	}
}

static void FoFClearBotActions( FoFBotRuntimeState_t &state )
{
	state.m_nActionCount = 0;
	for ( int i = 0; i < FOF_BOT_MAX_ACTIONS; ++i )
		state.m_Actions[i] = FoFBotAction_t();
}

static FoFBotAction_t *FoFCurrentBotAction(
	FoFBotRuntimeState_t &state )
{
	return state.m_nActionCount > 0 ? &state.m_Actions[0] : NULL;
}

static bool FoFAppendBotAction(
	FoFBotRuntimeState_t &state, int nType, const Vector &goal,
	CBaseEntity *pEntity = NULL, float flEndTime = 0.0f )
{
	if ( state.m_nActionCount >= FOF_BOT_MAX_ACTIONS )
		return false;

	FoFBotAction_t &action = state.m_Actions[state.m_nActionCount++];
	action = FoFBotAction_t();
	action.m_nType = nType;
	action.m_vecGoal = goal;
	action.m_hEntity = pEntity;
	action.m_flEndTime = flEndTime;
	return true;
}

static bool FoFInsertBotAction(
	FoFBotRuntimeState_t &state, int nType, const Vector &goal,
	CBaseEntity *pEntity = NULL, float flEndTime = 0.0f )
{
	if ( state.m_nActionCount >= FOF_BOT_MAX_ACTIONS )
		return false;
	for ( int i = state.m_nActionCount; i > 0; --i )
		state.m_Actions[i] = state.m_Actions[i - 1];
	++state.m_nActionCount;
	FoFBotAction_t &action = state.m_Actions[0];
	action = FoFBotAction_t();
	action.m_nType = nType;
	action.m_vecGoal = goal;
	action.m_hEntity = pEntity;
	action.m_flEndTime = flEndTime;
	return true;
}

static void FoFPopBotAction( FoFBotRuntimeState_t &state )
{
	if ( state.m_nActionCount <= 0 )
		return;
	for ( int i = 1; i < state.m_nActionCount; ++i )
		state.m_Actions[i - 1] = state.m_Actions[i];
	--state.m_nActionCount;
	state.m_Actions[state.m_nActionCount] = FoFBotAction_t();
}

static bool FoFAddBotPathPoint( FoFBotRuntimeState_t &state,
	const Vector &point, NavTraverseType traverse )
{
	if ( state.m_nPathCount >= FOF_BOT_MAX_PATH_POINTS )
		return false;

	if ( state.m_nPathCount > 0 &&
		( state.m_vecPath[state.m_nPathCount - 1] - point ).LengthSqr() < 16.0f )
	{
		return true;
	}

	state.m_vecPath[state.m_nPathCount] = point;
	state.m_nPathTraverse[state.m_nPathCount] =
		static_cast< unsigned char >( traverse );
	++state.m_nPathCount;
	return true;
}

class FoFBotPathCost
{
public:
	float operator()( CNavArea *pArea, CNavArea *pFromArea,
		const CNavLadder *pLadder, const CFuncElevator *pElevator,
		float flLength )
	{
		ShortestPathCost shortestPath;
		float flCost = shortestPath(
			pArea, pFromArea, pLadder, pElevator, flLength );
		if ( !pArea || !pFromArea || flCost < 0.0f )
			return flCost;

		if ( pArea->GetAttributes() & NAV_MESH_AVOID )
		{
			const float flDistance = pLadder ? pLadder->m_length :
				flLength > 0.0f ? flLength :
				( pArea->GetCenter() - pFromArea->GetCenter() ).Length();
			flCost += flDistance * 20.0f;
		}
		return flCost;
	}
};

static bool FoFBuildBotPath( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, const Vector &goal, bool bCloseGoal,
	bool bPathToTarget )
{
	FoFClearBotPath( state );
	state.m_bPathToTarget = bPathToTarget;
	state.m_vecPathGoal = goal;

	if ( !pBot || !TheNavMesh || !TheNavMesh->IsLoaded() )
		return false;

	Vector startPosition = pBot->GetAbsOrigin();
	Vector goalPosition = goal;
	startPosition.z += 5.0f;
	goalPosition.z += 5.0f;

	// FoF's CreatePath uses an unrestricted nearest-area query for
	// the bot and a 40-unit query for goals that must be touched precisely.
	CNavArea *pStartArea = TheNavMesh->GetNearestNavArea(
		startPosition, true, 10000.0f, false, true, TEAM_ANY );
	CNavArea *pGoalArea = TheNavMesh->GetNearestNavArea(
		goalPosition, !bCloseGoal, bCloseGoal ? 40.0f : 10000.0f,
		false, false, TEAM_ANY );
	if ( !pStartArea || !pGoalArea )
		return false;

	if ( pStartArea == pGoalArea )
	{
		FoFAddBotPathPoint( state, goal, NUM_TRAVERSE_TYPES );
		return true;
	}

	FoFBotPathCost pathCost;
	if ( !NavAreaBuildPath( pStartArea, pGoalArea, &goalPosition,
		pathCost, NULL, 0.0f, TEAM_ANY, false ) )
	{
		return false;
	}

	CNavArea *route[FOF_BOT_MAX_PATH_POINTS];
	int nRouteCount = 0;
	for ( CNavArea *pArea = pGoalArea;
		pArea && nRouteCount < ARRAYSIZE( route );
		pArea = pArea->GetParent() )
	{
		route[nRouteCount++] = pArea;
		if ( pArea == pStartArea )
			break;
	}
	if ( nRouteCount == 0 || route[nRouteCount - 1] != pStartArea )
		return false;

	Vector previousPoint = startPosition;
	for ( int i = nRouteCount - 2; i >= 0; --i )
	{
		CNavArea *pFromArea = route[i + 1];
		CNavArea *pToArea = route[i];
		const NavTraverseType traverse = pToArea->GetParentHow();
		Vector waypoint = pToArea->GetCenter();
		if ( traverse >= GO_NORTH && traverse <= GO_WEST )
		{
			pFromArea->ComputeClosestPointInPortal(
				pToArea, static_cast< NavDirType >( traverse ),
				previousPoint, &waypoint );
		}
		if ( !FoFAddBotPathPoint( state, waypoint, traverse ) )
			break;
		previousPoint = waypoint;
	}

	FoFAddBotPathPoint( state, goal, NUM_TRAVERSE_TYPES );
	state.m_nPathIndex = 0;
	return state.m_nPathCount > 0;
}

static bool FoFGetCurrentBotPathPoint( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, Vector &point, NavTraverseType &traverse )
{
	while ( state.m_nPathIndex < state.m_nPathCount )
	{
		point = state.m_vecPath[state.m_nPathIndex];
		const Vector delta = point - pBot->GetAbsOrigin();
		const bool bLastPoint =
			state.m_nPathIndex == state.m_nPathCount - 1;
		const float flReachDistance = bLastPoint ? 64.0f : 42.0f;
		if ( delta.Length2DSqr() > flReachDistance * flReachDistance ||
			fabsf( delta.z ) > 72.0f )
		{
			traverse = static_cast< NavTraverseType >(
				state.m_nPathTraverse[state.m_nPathIndex] );
			return true;
		}
		++state.m_nPathIndex;
	}
	return false;
}

static void FoFSetBotMoveToward( const QAngle &viewAngles,
	const Vector &origin, const Vector &destination, float flSpeed,
	CUserCmd &cmd, float *pMoveYaw )
{
	const Vector delta = destination - origin;
	const float flMoveYaw = UTIL_VecToYaw( delta );
	const float flRelativeYaw = DEG2RAD(
		AngleDiff( flMoveYaw, viewAngles.y ) );
	const float flForwardFraction = cosf( flRelativeYaw );
	const float flSideFraction = sinf( flRelativeYaw );

	cmd.forwardmove = 0.0f;
	if ( flForwardFraction > 0.25f )
		cmd.forwardmove = flSpeed;
	else if ( flForwardFraction < -0.25f )
		cmd.forwardmove = -flSpeed;

	cmd.sidemove = 0.0f;
	if ( flSideFraction >= 0.25f )
		cmd.sidemove = -flSpeed;
	else if ( flSideFraction <= -0.25f )
		cmd.sidemove = flSpeed;

	if ( pMoveYaw )
		*pMoveYaw = flMoveYaw;
}

static void FoFSynchronizeBotMovementButtons( CUserCmd &cmd )
{
	cmd.buttons &= ~( IN_FORWARD | IN_BACK );

	const float flMoveLength = sqrtf(
		cmd.forwardmove * cmd.forwardmove +
		cmd.sidemove * cmd.sidemove );
	if ( flMoveLength <= 0.0f )
		return;

	const float flForwardFraction = cmd.forwardmove / flMoveLength;
	if ( flForwardFraction > 0.25f )
		cmd.buttons |= IN_FORWARD;
	else if ( flForwardFraction < -0.25f )
		cmd.buttons |= IN_BACK;
}

static bool FoFBuildBotNavigationCommand( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, const Vector &goal, bool bPathToTarget,
	float flSpeed, CUserCmd &cmd, float *pMoveYaw,
	Vector *pWaypoint = NULL )
{
	const float flGoalMoveTolerance = bPathToTarget ? 96.0f : 32.0f;
	const bool bGoalMoved =
		( state.m_vecPathGoal - goal ).Length2DSqr() >
		flGoalMoveTolerance * flGoalMoveTolerance;
	const bool bPathTypeChanged = state.m_bPathToTarget != bPathToTarget;
	const bool bNeedsPath = bPathTypeChanged ||
		state.m_nPathIndex >= state.m_nPathCount || bGoalMoved;
	if ( bNeedsPath && ( bPathTypeChanged ||
		gpGlobals->curtime >= state.m_flNextPathBuild ) )
	{
		state.m_flNextPathBuild = gpGlobals->curtime +
			( bPathToTarget ? 0.45f : 1.0f );
		FoFBuildBotPath( pBot, state, goal, false, bPathToTarget );
	}

	Vector waypoint;
	NavTraverseType traverse = NUM_TRAVERSE_TYPES;
	if ( !FoFGetCurrentBotPathPoint( pBot, state, waypoint, traverse ) )
		return false;

	FoFSetBotMoveToward( cmd.viewangles, pBot->GetAbsOrigin(),
		waypoint, flSpeed, cmd, pMoveYaw );
	if ( pWaypoint )
		*pWaypoint = waypoint;
	if ( traverse == GO_JUMP )
		cmd.buttons |= IN_JUMP;
	else if ( traverse == GO_LADDER_UP )
	{
		cmd.buttons |= IN_FORWARD | IN_JUMP;
		cmd.upmove = flSpeed;
	}
	else if ( traverse == GO_LADDER_DOWN )
	{
		cmd.buttons |= IN_FORWARD | IN_DUCK;
		cmd.upmove = -flSpeed;
	}
	return true;
}

static bool FoFIsHorseRaceMap()
{
	const char *pszMapName = STRING( gpGlobals->mapname );
	return pszMapName &&
		( !Q_strnicmp( pszMapName, "fofhr_", 6 ) ||
		  !Q_strnicmp( pszMapName, "tphr_", 5 ) );
}

static bool FoFBotMaySeekCrates( CFoFBot *pBot )
{
	if ( !pBot || pBot->IsOnFoFHorse() )
		return false;

	static ConVarRef forceOpen( "fof_bot_forceopenchest", true );
	if ( forceOpen.IsValid() && forceOpen.GetBool() )
		return true;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return !currentMode.IsValid() || currentMode.GetInt() != 6;
}

static CBaseEntity *FoFFindNearestBotObjectiveByClassname(
	CFoFBot *pBot, const char *pszClassname )
{
	CBaseEntity *pBest = NULL;
	float flBestDistanceSqr = FLT_MAX;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, pszClassname ) ) != NULL )
	{
		if ( pEntity->IsMarkedForDeletion() ||
			pEntity->IsEffectActive( EF_NODRAW ) )
		{
			continue;
		}
		if ( HL2MPRules() && HL2MPRules()->IsTeamplay() &&
			pEntity->GetTeamNumber() >= FIRST_GAME_TEAM &&
			pBot->GetTeamNumber() >= FIRST_GAME_TEAM &&
			pEntity->GetTeamNumber() != pBot->GetTeamNumber() )
		{
			continue;
		}

		const float flDistanceSqr = pBot->GetAbsOrigin().DistToSqr(
			pEntity->GetAbsOrigin() );
		if ( flDistanceSqr < flBestDistanceSqr )
		{
			flBestDistanceSqr = flDistanceSqr;
			pBest = pEntity;
		}
	}
	return pBest;
}

static CBaseEntity *FoFFindCourseBotFollowTarget( CFoFBot *pBot )
{
	CFoF_Player *pBest = NULL;
	int nBestScore = INT_MIN;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer == pBot || pPlayer->IsBot() ||
			!pPlayer->IsAlive() || pPlayer->IsObserver() )
		{
			continue;
		}
		if ( pBot->GetTeamNumber() >= FIRST_GAME_TEAM &&
			pPlayer->GetTeamNumber() != pBot->GetTeamNumber() )
		{
			continue;
		}

		if ( pPlayer->FragCount() >= nBestScore )
		{
			nBestScore = pPlayer->FragCount();
			pBest = pPlayer;
		}
	}
	return pBest;
}

static bool FoFBuildCourseCompanionFollowPath(
	CFoFBot *pBot, FoFBotRuntimeState_t &state,
	CBaseEntity *pFollowTarget, Vector &goal )
{
	if ( !pBot || !pFollowTarget || !TheNavMesh ||
		!TheNavMesh->IsLoaded() )
	{
		return false;
	}

	const Vector targetOrigin = pFollowTarget->GetAbsOrigin();
	CNavArea *pTargetArea = TheNavMesh->GetNearestNavArea(
		targetOrigin, false,
		static_cast< float >( random->RandomInt( 100, 150 ) ),
		false, true, TEAM_ANY );
	if ( !pTargetArea )
		return false;

	goal = pTargetArea->GetCenter();
	if ( !FoFBuildBotPath( pBot, state, goal, false, false ) )
		return false;
	state.m_vecCourseFollowAnchor = targetOrigin;
	return true;
}

static CCourseMode *FoFFindCourseMode()
{
	return dynamic_cast< CCourseMode * >(
		gEntList.FindEntityByClassname( NULL, "fof_coursemode" ) );
}

static bool FoFGetBotCourseOrder( CFoFBot *pBot,
	int &nOrder, Vector &orderOrigin, float &flOrderRadius )
{
	nOrder = -1;
	orderOrigin.Init();
	flOrderRadius = 0.0f;
	if ( !pBot )
		return false;

	CCourseMode *pCourse = FoFFindCourseMode();
	if ( !pCourse )
		return false;

	const int nPlayerTeam = pCourse->GetCoursePlayerTeam();
	if ( nPlayerTeam >= FIRST_GAME_TEAM &&
		pBot->GetTeamNumber() == nPlayerTeam )
	{
		return false;
	}

	nOrder = pCourse->GetBotOrder(
		pBot->GetAbsOrigin(), orderOrigin, flOrderRadius );
	return nOrder >= 0;
}

static bool FoFTrySelectBotModeObjective(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode != 2 && nMode != 4 && nMode != 6 )
		return false;

	CCourseMode *pCourse = nMode == 6 ? FoFFindCourseMode() : NULL;
	CBaseEntity *pObjective = NULL;
	if ( nMode != 6 )
	{
		pObjective = FoFFindNearestBotObjectiveByClassname(
			pBot, "fof_cap_entity" );
		if ( !pObjective )
		{
			pObjective = FoFFindNearestBotObjectiveByClassname(
				pBot, "fof_cart_push" );
		}
	}
	if ( !pObjective && nMode == 6 )
	{
		if ( !pCourse ||
			( FoFIsCourseCompanionBot( pBot ) &&
			  pCourse->ShouldCourseCompanionsFollowPlayer() ) )
			pObjective = FoFFindCourseBotFollowTarget( pBot );
	}
	if ( !pObjective )
		return false;


	Vector goal = pObjective->GetAbsOrigin();
	const bool bCourseCompanionFollow = nMode == 6 &&
		FoFIsCourseCompanionBot( pBot ) && pObjective->IsPlayer();
	if ( bCourseCompanionFollow )
	{
		if ( !FoFBuildCourseCompanionFollowPath(
			pBot, state, pObjective, goal ) )
		{
			return false;
		}
	}
	else if ( !FoFBuildBotPath( pBot, state, goal, false, true ) )
	{
		return false;
	}

	state.m_nObjective = FOF_BOT_OBJECTIVE_MODE;
	state.m_hObjectiveEntity = pObjective;
	state.m_hObjectiveCrate = NULL;
	state.m_hObjectiveHorse = NULL;
	state.m_flObjectiveExpireTime = gpGlobals->curtime + 12.0f;
	return true;
}

static bool FoFTrySelectBotHorseObjective(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( !FoFIsHorseRaceMap() || pBot->IsOnFoFHorse() )
		return false;

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, "fof_horse" ) ) != NULL )
	{
		CFoF_Horse *pHorse = dynamic_cast< CFoF_Horse * >( pEntity );
		if ( !pHorse || !pHorse->IsAvailableForMount() ||
			pBot->GetAbsOrigin().DistTo( pHorse->GetAbsOrigin() ) >= 500.0f )
		{
			continue;
		}

		Vector goal = pHorse->GetAbsOrigin();
		goal.z += 35.0f;
		if ( !FoFBuildBotPath( pBot, state, goal, false, true ) )
		{
			FoFClearBotPath( state );
			state.m_bPathToTarget = true;
			state.m_vecPathGoal = goal;
			FoFAddBotPathPoint( state, goal, NUM_TRAVERSE_TYPES );
		}

		state.m_nObjective = FOF_BOT_OBJECTIVE_HORSE;
		state.m_hObjectiveHorse = pHorse;
		state.m_hObjectiveCrate = NULL;
		state.m_hObjectiveEntity = NULL;
		state.m_flObjectiveExpireTime = gpGlobals->curtime + 12.0f;
		return true;
	}

	return false;
}

static bool FoFTrySelectBotCrateObjective(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( !FoFBotMaySeekCrates( pBot ) )
		return false;

	static const char *s_pszCrateClasses[] =
	{
		"fof_crate",
		"fof_crate_low",
		"fof_crate_med"
	};
	bool bSkippedClass = false;
	for ( int nClass = 0; nClass < ARRAYSIZE( s_pszCrateClasses ); ++nClass )
	{
		if ( !bSkippedClass && random->RandomInt( 0, 10 ) > 5 )
		{
			bSkippedClass = true;
			continue;
		}

		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByClassname(
			pEntity, s_pszCrateClasses[nClass] ) ) != NULL )
		{
			FoF_Crate *pCrate = dynamic_cast< FoF_Crate * >( pEntity );
			if ( !pCrate || !pCrate->IsReadyForBotUse() )
				continue;

			if ( pBot->GetAbsOrigin().DistTo( pCrate->GetAbsOrigin() ) >=
				1000.0f )
			{
				continue;
			}

			Vector goal = pCrate->GetAbsOrigin();
			if ( FoFBuildBotPath( pBot, state, goal, true, true ) )
			{
				state.m_nObjective = FOF_BOT_OBJECTIVE_CRATE;
				state.m_hObjectiveCrate = pCrate;
				state.m_hObjectiveHorse = NULL;
				state.m_hObjectiveEntity = NULL;
				state.m_flObjectiveExpireTime = gpGlobals->curtime + 15.0f;
				return true;
			}

			// The original scheduler tries at most the first nearby candidate
			// from each crate class before moving on to the next class.
			break;
		}
	}

	return false;
}

static bool FoFTrySelectBotObjective(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( state.m_nObjective != FOF_BOT_OBJECTIVE_NONE )
		return true;
	if ( gpGlobals->curtime < state.m_flNextObjectiveSearch )
		return false;

	state.m_flNextObjectiveSearch = gpGlobals->curtime + 0.75f;
	if ( FoFTrySelectBotModeObjective( pBot, state ) )
		return true;
	if ( FoFIsCourseCompanionBot( pBot ) )
		return false;
	if ( FoFTrySelectBotHorseObjective( pBot, state ) )
		return true;
	return FoFTrySelectBotCrateObjective( pBot, state );
}

static void FoFAimBotToward(
	CFoFBot *pBot, const Vector &point, CUserCmd &cmd )
{
	QAngle desiredAngles;
	VectorAngles( point - pBot->EyePosition(), desiredAngles );
	desiredAngles.x = clamp( AngleNormalize( desiredAngles.x ), -89.0f, 89.0f );
	desiredAngles.y = AngleNormalize( desiredAngles.y );
	desiredAngles.z = 0.0f;

	cmd.viewangles = pBot->EyeAngles();
	const float flTurn = 240.0f * gpGlobals->frametime;
	cmd.viewangles.x = UTIL_ApproachAngle(
		desiredAngles.x, cmd.viewangles.x, flTurn );
	cmd.viewangles.y = UTIL_ApproachAngle(
		desiredAngles.y, cmd.viewangles.y, flTurn );
	cmd.viewangles.z = 0.0f;
}

static bool FoFBuildBotObjectiveCommand(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	if ( state.m_nObjective == FOF_BOT_OBJECTIVE_NONE )
		return false;
	if ( gpGlobals->curtime >= state.m_flObjectiveExpireTime )
	{
		FoFClearBotObjective( state, true );
		return false;
	}

	CBaseEntity *pObjective = NULL;
	CFoF_Player *pFollowPlayer = NULL;
	Vector goal;
	float flUseDistance = 0.0f;
	bool bCloseGoal = false;
	bool bPressUse = false;
	bool bCourseCompanionFollow = false;
	if ( state.m_nObjective == FOF_BOT_OBJECTIVE_CRATE )
	{
		FoF_Crate *pCrate = state.m_hObjectiveCrate.Get();
		if ( !pCrate ||
			( !pCrate->IsReadyForBotUse() &&
			  !pCrate->IsBeingOpenedBy( pBot ) ) )
		{
			FoFClearBotObjective( state, true );
			return false;
		}

		pObjective = pCrate;
		goal = pCrate->GetAbsOrigin();
		flUseDistance = 100.0f;
		bCloseGoal = true;
		bPressUse = true;
	}
	else if ( state.m_nObjective == FOF_BOT_OBJECTIVE_HORSE )
	{
		if ( pBot->IsOnFoFHorse() )
		{
			FoFClearBotObjective( state, true );
			return false;
		}

		CFoF_Horse *pHorse = state.m_hObjectiveHorse.Get();
		if ( !pHorse || !pHorse->IsAvailableForMount() )
		{
			FoFClearBotObjective( state, true );
			return false;
		}

		pObjective = pHorse;
		goal = pHorse->GetAbsOrigin();
		goal.z += 35.0f;
		flUseDistance = 96.0f;
		bPressUse = true;
	}
	else if ( state.m_nObjective == FOF_BOT_OBJECTIVE_MODE )
	{
		pObjective = state.m_hObjectiveEntity.Get();
		pFollowPlayer = ToFoFPlayer( pObjective );
		if ( !pObjective || pObjective->IsMarkedForDeletion() ||
			pObjective->IsEffectActive( EF_NODRAW ) ||
			( pFollowPlayer &&
			  ( !pFollowPlayer->IsAlive() || pFollowPlayer->IsObserver() ) ) )
		{
			FoFClearBotObjective( state, true );
			return false;
		}

		bCourseCompanionFollow = pFollowPlayer &&
			FoFIsCourseCompanionBot( pBot );
		if ( bCourseCompanionFollow )
		{
			CCourseMode *pCourse = FoFFindCourseMode();
			if ( pCourse &&
				!pCourse->ShouldCourseCompanionsFollowPlayer() )
			{
				FoFClearBotObjective( state, true );
				return false;
			}
		}
		if ( bCourseCompanionFollow &&
			pFollowPlayer->GetAbsOrigin().DistToSqr(
				state.m_vecCourseFollowAnchor ) > Square( 128.0f ) )
		{
			Vector updatedGoal;
			if ( FoFBuildCourseCompanionFollowPath(
				pBot, state, pFollowPlayer, updatedGoal ) )
			{
				state.m_vecPathGoal = updatedGoal;
				state.m_flObjectiveExpireTime =
					gpGlobals->curtime + 12.0f;
			}
			else
			{
				FoFClearBotObjective( state, true );
				return false;
			}
		}
		goal = bCourseCompanionFollow ?
			state.m_vecPathGoal : pObjective->GetAbsOrigin();
		flUseDistance = bCourseCompanionFollow ? 64.0f :
			pFollowPlayer ? 180.0f : 110.0f;
	}
	else
	{
		FoFClearBotObjective( state, true );
		return false;
	}

	const float flDistance = bCourseCompanionFollow ?
		pBot->GetAbsOrigin().DistTo( goal ) :
		pBot->WorldSpaceCenter().DistTo( pObjective->WorldSpaceCenter() );
	if ( !pFollowPlayer )
		FoFAimBotToward( pBot, pObjective->WorldSpaceCenter(), cmd );
	else
		cmd.viewangles = pBot->EyeAngles();
	if ( flDistance <= flUseDistance )
	{
		if ( pFollowPlayer )
		{
			if ( bCourseCompanionFollow )
			{
				state.m_vecCourseCompanionHome = goal;
			}
			FoFClearBotObjective( state, true );
			state.m_flNextObjectiveSearch = gpGlobals->curtime + 1.5f;
			return false;
		}
		cmd.forwardmove = 0.0f;
		cmd.sidemove = 0.0f;
		if ( bPressUse )
			cmd.buttons |= IN_USE;
		state.m_flObjectiveExpireTime = gpGlobals->curtime + 15.0f;
		if ( state.m_nObjective == FOF_BOT_OBJECTIVE_CRATE )
		{
			FoF_Crate *pCrate = state.m_hObjectiveCrate.Get();
			if ( pCrate && pCrate->IsReadyForBotUse() )
				pCrate->Use( pBot, pBot, USE_ON, 1.0f );
		}
		return true;
	}

	Vector waypoint;
	NavTraverseType traverse = NUM_TRAVERSE_TYPES;
	if ( !FoFGetCurrentBotPathPoint( pBot, state, waypoint, traverse ) )
	{
		if ( !FoFBuildBotPath( pBot, state, goal, bCloseGoal,
			!bCourseCompanionFollow ) ||
			!FoFGetCurrentBotPathPoint( pBot, state, waypoint, traverse ) )
		{
			FoFClearBotObjective( state, true );
			return false;
		}
	}

	const float flSpeed = MAX( 1.0f, pBot->GetPlayerMaxSpeed() );
	float flMoveYaw = cmd.viewangles.y;
	FoFSetBotMoveToward( cmd.viewangles, pBot->GetAbsOrigin(),
		waypoint, flSpeed, cmd, &flMoveYaw );
	cmd.viewangles.y = UTIL_ApproachAngle(
		flMoveYaw, cmd.viewangles.y, 120.0f * gpGlobals->frametime );
	if ( traverse == GO_JUMP )
		cmd.buttons |= IN_JUMP;
	else if ( traverse == GO_LADDER_UP )
	{
		cmd.buttons |= IN_FORWARD | IN_JUMP;
		cmd.upmove = flSpeed;
	}
	else if ( traverse == GO_LADDER_DOWN )
	{
		cmd.buttons |= IN_FORWARD | IN_DUCK;
		cmd.upmove = -flSpeed;
	}
	return true;
}

static bool FoFBotCanUseNearbyEntity(
	CFoFBot *pBot, CBaseEntity *pEntity )
{
	if ( pBot->IsFoFBotGhost() || !pEntity || pEntity == pBot ||
		pEntity->IsMarkedForDeletion() ||
		pEntity->IsEffectActive( EF_NODRAW ) )
	{
		return false;
	}

	if ( FClassnameIs( pEntity, "item_whiskey" ) ||
		FClassnameIs( pEntity, "item_golden_skull" ) )
	{
		return true;
	}

	CFoF_Horse *pHorse = dynamic_cast< CFoF_Horse * >( pEntity );
	if ( pHorse )
	{
		return FoFIsHorseRaceMap() && !pBot->IsOnFoFHorse() &&
			pHorse->IsAvailableForMount();
	}

	CBaseCombatWeapon *pWeapon =
		dynamic_cast< CBaseCombatWeapon * >( pEntity );
	if ( !pWeapon || pWeapon->GetOwnerEntity() ||
		pWeapon->FoFWeaponID() == 6 || pBot->WeaponCount() >= 5 )
	{
		return false;
	}

	Vector vecVelocity;
	pWeapon->GetVelocity( &vecVelocity, NULL );
	return vecVelocity.Length() <= 10.0f;
}

static bool FoFHandleBotNearbyUse(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	if ( FoFIsCourseCompanionBot( pBot ) )
		return false;
	if ( gpGlobals->curtime < state.m_flNextInteractionCheck )
		return false;
	state.m_flNextInteractionCheck = gpGlobals->curtime + 0.25f;

	const float flRadius = FoFIsHorseRaceMap() ? 100.0f : 75.0f;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityInSphere(
		pEntity, pBot->GetAbsOrigin(), flRadius ) ) != NULL )
	{
		if ( !FoFBotCanUseNearbyEntity( pBot, pEntity ) )
			continue;

		FoFAimBotToward( pBot, pEntity->WorldSpaceCenter(), cmd );
		cmd.forwardmove = 0.0f;
		cmd.sidemove = 0.0f;
		cmd.buttons |= IN_USE;
		return true;
	}
	return false;
}

static void FoFHandleBotPathObstacle(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	if ( cmd.forwardmove < 20.0f )
	{
		state.m_hBlockingObstacle = NULL;
		return;
	}

	Vector forward;
	AngleVectors( QAngle( 0.0f, cmd.viewangles.y, 0.0f ), &forward );
	const Vector start = pBot->GetAbsOrigin() + Vector( 0.0f, 0.0f, 35.0f );
	const Vector end = start + forward * 48.0f;
	trace_t trace;
	UTIL_TraceHull( start, end,
		Vector( -17.0f, -17.0f, -34.0f ),
		Vector( 17.0f, 17.0f, 34.0f ),
		MASK_PLAYERSOLID, pBot, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
	CBaseEntity *pObstacle = trace.m_pEnt;
	if ( trace.fraction >= 0.95f || !pObstacle || pObstacle->IsWorld() )
	{
		state.m_hBlockingObstacle = NULL;
		return;
	}

	state.m_hBlockingObstacle = pObstacle;
	if ( gpGlobals->curtime < state.m_flNextObstacleInteraction )
		return;

	if ( FClassnameIs( pObstacle, "func_breakable_surf" ) )
	{
		cmd.buttons |= IN_ATTACK;
		state.m_flNextObstacleInteraction = gpGlobals->curtime + 0.35f;
		return;
	}

	if ( FClassnameIs( pObstacle, "prop_door_rotating" ) )
	{
		CBasePropDoor *pDoor = dynamic_cast< CBasePropDoor * >( pObstacle );
		if ( pDoor && pDoor->IsDoorClosed() && !pDoor->IsDoorLocked() )
		{
			pObstacle->Use( pBot, pBot, USE_ON, 0.0f );
			state.m_flNextObstacleInteraction = gpGlobals->curtime + 0.25f;
			state.m_flNextPathBuild = 0.0f;
		}
		return;
	}

	if ( FClassnameIs( pObstacle, "func_door_rotating" ) )
	{
		CBaseDoor *pDoor = dynamic_cast< CBaseDoor * >( pObstacle );
		if ( pDoor && pDoor->m_toggle_state == TS_AT_BOTTOM &&
			!pDoor->m_bLocked )
		{
			pDoor->Use( pBot, pBot, USE_ON, 0.0f );
			state.m_flNextObstacleInteraction = gpGlobals->curtime + 0.25f;
			state.m_flNextPathBuild = 0.0f;
		}
		return;
	}

	if ( pBot->GetTeamNumber() != FOF_TEAM_ZOMBIES &&
		!pBot->IsFoFBotGhost() && !pBot->IsOnFoFHorse() &&
		( FClassnameIs( pObstacle, "prop_physics" ) ||
		  FClassnameIs( pObstacle, "prop_physics_multiplayer" ) ||
		  FClassnameIs( pObstacle, "prop_physics_respawnable" ) ) )
	{
		if ( pObstacle->HasSpawnFlags(
			SF_PHYSPROP_DEBRIS | SF_PHYSPROP_MOTIONDISABLED |
			SF_PHYSPROP_NO_ROTORWASH_PUSH ) )
		{
			return;
		}

		CBreakableProp *pBreakable =
			dynamic_cast< CBreakableProp * >( pObstacle );
		if ( pBreakable &&
			( pBreakable->GetExplosiveDamage() > 0.0f ||
			  pBreakable->GetExplosiveRadius() > 0.0f ) )
		{
			return;
		}

		IPhysicsObject *pPhysics = pObstacle->VPhysicsGetObject();
		if ( !pPhysics || !pPhysics->IsMoveable() )
			return;
		FoFAimBotToward( pBot, pObstacle->WorldSpaceCenter(), cmd );
		cmd.buttons |= IN_SPEED;
		state.m_flNextObstacleInteraction = gpGlobals->curtime + 1.5f;
	}
}

static void FoFBuildBotScriptPath( char *pszPath, int nPathSize )
{
	static ConVarRef scriptName( "fof_bot_scriptname", true );
	const char *pszConfigured = scriptName.IsValid() ?
		scriptName.GetString() : "1";
	if ( !pszConfigured || !pszConfigured[0] ||
		!Q_stricmp( pszConfigured, "0" ) ||
		!Q_stricmp( pszConfigured, "1" ) )
	{
		pszConfigured = "1_default_bots.txt";
	}

	char szFile[128];
	Q_strncpy( szFile, pszConfigured, sizeof( szFile ) );
	const int nLength = Q_strlen( szFile );
	if ( nLength < 4 || Q_stricmp( szFile + nLength - 4, ".txt" ) )
		Q_strncat( szFile, ".txt", sizeof( szFile ) );

	if ( !Q_strnicmp( szFile, "fof_scripts/bots/", 17 ) )
		Q_strncpy( pszPath, szFile, nPathSize );
	else
		Q_snprintf( pszPath, nPathSize, "fof_scripts/bots/%s", szFile );
}

static void FoFApplyBotSkill( FoFBotProfile_t &profile )
{
	static ConVarRef botSkill( "fof_bot_skill", true );
	const int nSkill = clamp(
		botSkill.IsValid() ? botSkill.GetInt() : 5, 0, 6 );
	if ( nSkill == 6 )
		return;

	if ( nSkill == 5 )
	{
		profile.m_nRotationSpeed = random->RandomInt( 0, 10 );
		profile.m_nShootDelay = random->RandomInt( 0, 10 );
		profile.m_nAimTrailing = random->RandomInt( 0, 10 );
		profile.m_nStrafe = random->RandomInt( 0, 10 );
		profile.m_nAggression = random->RandomInt( 0, 10 );
		return;
	}

	const int nFixedSkill = clamp(
		RoundFloatToInt( static_cast< float >( nSkill ) * 2.5f ), 0, 10 );
	profile.m_nRotationSpeed = nFixedSkill;
	profile.m_nShootDelay = nFixedSkill;
	profile.m_nAimTrailing = nFixedSkill;
	profile.m_nStrafe = nFixedSkill;
	profile.m_nAggression = nFixedSkill;
}

static void FoFParseBotEquipment(
	const char *pszEquipment, int *pItems, int nItemCount )
{
	for ( int i = 0; i < nItemCount; ++i )
		pItems[i] = -1;

	if ( !pszEquipment )
		return;

	const char *pCursor = pszEquipment;
	for ( int i = 0; i < nItemCount && *pCursor; ++i )
	{
		while ( *pCursor == ' ' || *pCursor == '\t' || *pCursor == ',' )
			++pCursor;
		if ( !*pCursor )
			break;

		char szToken[64];
		int nLength = 0;
		while ( *pCursor && *pCursor != ',' )
		{
			if ( nLength < sizeof( szToken ) - 1 )
				szToken[nLength++] = *pCursor;
			++pCursor;
		}
		while ( nLength > 0 &&
			( szToken[nLength - 1] == ' ' ||
			  szToken[nLength - 1] == '\t' ) )
		{
			--nLength;
		}
		szToken[nLength] = '\0';
		pItems[i] = FoFEquipmentItemId( szToken );
	}
}

bool FoFLoadBotProfile( int nProfile, FoFBotProfile_t &profile )
{
	char szPath[MAX_PATH];
	FoFBuildBotScriptPath( szPath, sizeof( szPath ) );

	KeyValues *pRoot = new KeyValues( "BotList" );
	if ( !pRoot->LoadFromFile( filesystem, szPath, "GAME" ) )
	{
		Warning( "FoF bot profile '%s' could not be loaded.\n", szPath );
		pRoot->deleteThis();
		return false;
	}

	int nPresetCount = 0;
	for ( KeyValues *pPreset = pRoot->GetFirstTrueSubKey();
		pPreset; pPreset = pPreset->GetNextTrueSubKey() )
	{
		if ( !Q_stricmp( pPreset->GetName(), "preset" ) )
			++nPresetCount;
	}
	if ( nPresetCount <= 0 )
	{
		pRoot->deleteThis();
		return false;
	}

	int nWanted = nProfile % nPresetCount;
	if ( nWanted < 0 )
		nWanted += nPresetCount;
	KeyValues *pSelected = NULL;
	for ( KeyValues *pPreset = pRoot->GetFirstTrueSubKey();
		pPreset; pPreset = pPreset->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( pPreset->GetName(), "preset" ) )
			continue;
		if ( nWanted-- == 0 )
		{
			pSelected = pPreset;
			break;
		}
	}

	if ( !pSelected )
	{
		pRoot->deleteThis();
		return false;
	}

	profile.m_nRotationSpeed = clamp(
		pSelected->GetInt( "bot_rotation_speed", 5 ), 0, 10 );
	profile.m_nShootDelay = clamp(
		pSelected->GetInt( "bot_shoot_delay", 5 ), 0, 10 );
	profile.m_nAimTrailing = clamp(
		pSelected->GetInt( "bot_aim_trailing", 5 ), 0, 10 );
	profile.m_nStrafe = clamp(
		pSelected->GetInt( "bot_strafe", 5 ), 0, 10 );
	profile.m_nForceTeam = pSelected->GetInt( "bot_force_team", 0 );
	profile.m_nAggression = clamp(
		pSelected->GetInt( "bot_aggression", 5 ), 0, 10 );
	Q_strncpy( profile.m_szName,
		pSelected->GetString( "bot_name", "BOT With No Name" ),
		sizeof( profile.m_szName ) );
	Q_strncpy( profile.m_szEquipment,
		pSelected->GetString( "bot_equipment", "" ),
		sizeof( profile.m_szEquipment ) );
	FoFApplyBotSkill( profile );
	pRoot->deleteThis();
	return true;
}

void FoFConfigureBotRuntime( CFoFBot *pBot,
	const FoFBotProfile_t *pProfile, bool bPopulationManaged )
{
	if ( !pBot || pBot->entindex() < 1 || pBot->entindex() > MAX_PLAYERS )
		return;

	FoFBotRuntimeState_t &state = s_FoFBotRuntime[pBot->entindex()];
	state = FoFBotRuntimeState_t();
	state.m_hBot = pBot;
	state.m_bPopulationManaged = bPopulationManaged;
	state.m_vecLastPosition = pBot->GetAbsOrigin();
	state.m_flWanderYaw = pBot->EyeAngles().y;
	if ( pProfile )
	{
		state.m_Profile = *pProfile;
		state.m_bConfigured = true;
		FoFParseBotEquipment(
			pProfile->m_szEquipment, state.m_nEquipment,
			ARRAYSIZE( state.m_nEquipment ) );
	}
	FoFUpdateBotProfileValues( state );
	if ( FoFIsConfiguredCourseBot( state ) )
	{
		// Course actors keep their connected identity for names and team
		// colours.  The client excludes this player-info flag from rankings.
		pBot->AddSpawnFlags( 0x400000 );
		pBot->m_nPlayerInfo |= 0x400000;
	}

	pBot->SelectFoFEquipment();
	state.m_bEquipmentSelected = true;
}

void FoFResetBotSpawnState( CFoFBot *pBot )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;

	pState->m_hTarget = NULL;
	pState->m_hSupportTarget = NULL;
	pState->m_flNextTargetSearch = 0.0f;
	pState->m_flNextObjectiveSearch = 0.0f;
	pState->m_flNextInteractionCheck = 0.0f;
	pState->m_flNextSupportSearch = 0.0f;
	FoFClearBotObjective( *pState, false );
	FoFClearBotActions( *pState );
	pState->m_flNextStrafeChange = 0.0f;
	pState->m_flStrafeEnd = 0.0f;
	pState->m_flNextWanderTurn = 0.0f;
	pState->m_flNextShot = gpGlobals->curtime + 1.0f;
	pState->m_flNextCombatVoice = 0.0f;
	pState->m_flNextAimedShot = 0.0f;
	pState->m_flChargeRelease = 0.0f;
	pState->m_flNextWeaponCheck = 0.0f;
	pState->m_flNextWeaponSelection = 0.0f;
	pState->m_flNextStuckCheck = gpGlobals->curtime + 0.75f;
	pState->m_flNextPathBuild = 0.0f;
	pState->m_flNextWanderGoal = 0.0f;
	pState->m_flNextKick = 0.0f;
	pState->m_hBlockingObstacle = NULL;
	pState->m_flNextObstacleInteraction = 0.0f;
	pState->m_flStuckRecoveryEnd = 0.0f;
	pState->m_flStuckRecoverySide = 0.0f;
	pState->m_flStrafeMove = 0.0f;
	pState->m_flStrafeBias = 0.0f;
	pState->m_flWanderYaw = pBot->EyeAngles().y;
	pState->m_flAimPitchNoise = 0.0f;
	pState->m_flAimYawNoise = 0.0f;
	pState->m_flViewPitchVelocity = 0.0f;
	pState->m_flViewYawVelocity = 0.0f;
	pState->m_flAimOnTargetTime = 0.0f;
	pState->m_flWeaponMinRange = 0.0f;
	pState->m_flWeaponRange = 500.0f;
	pState->m_nPrimaryAttackStyle = 3;
	pState->m_nSecondaryAttackStyle = 3;
	pState->m_bChargingAttack = false;
	pState->m_bHasDynamiteThrowVelocity = false;
	pState->m_vecDynamiteThrowVelocity.Init();
	pState->m_bRoaming = false;
	pState->m_bPathToTarget = false;
	pState->m_nCourseOrder = -1;
	pState->m_nCourseGlowBand = 0;
	pState->m_nPendingCourseGlowBand = -1;
	pState->m_flCourseGlowReenableTime = 0.0f;
	pState->m_flCourseOrderRadius = 0.0f;
	pState->m_nStuckChecks = 0;
	pState->m_bHasCachedCommand = false;
	pState->m_CachedCommand.Reset();
	pState->m_vecLastPosition = pBot->GetAbsOrigin();
	pState->m_vecPathGoal.Init();
	pState->m_vecWanderGoal.Init();
	FoFClearBotPath( *pState );
	pState->m_bEquipmentRefreshComplete = false;
}

void FoFUpdateBotEquipment( CFoFBot *pBot )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;
	if ( !pBot->IsFoFSpawnEquipmentFinalized() )
		return;

	if ( !pState->m_bEquipmentSelected )
	{
		pBot->SelectFoFEquipment();
		pState->m_bEquipmentSelected = true;
	}

	if ( pBot->IsAlive() && !pState->m_bEquipmentRefreshComplete )
	{
		static ConVarRef currentMode( "fof_sv_currentmode", true );
		if ( pState->m_bConfigured && currentMode.IsValid() &&
			currentMode.GetInt() == 6 )
		{
			pBot->RemoveAllItems( true );
			FoFResetEquipmentState( pBot );
			FoFGiveEquipmentList(
				pBot, pState->m_Profile.m_szEquipment, false, true );
			// The original GiveDefaultItems path replenishes these six pools
			// after applying the bot profile.  In particular, bows deploy empty
			// and require XBowBolt reserve ammo before they can load an arrow.
			pBot->CBasePlayer::GiveAmmo( 1000, "Buckshot", true );
			pBot->CBasePlayer::GiveAmmo( 1000, "357", true );
			pBot->CBasePlayer::GiveAmmo( 1000, "XBowBolt", true );
			pBot->CBasePlayer::GiveAmmo( 1000, "XBowBolt2", true );
			pBot->CBasePlayer::GiveAmmo( 1000, "Rifle", true );
			pBot->CBasePlayer::GiveAmmo( 1000, "Rifle2", true );
			CBaseCombatWeapon *pFists = pBot->Weapon_OwnsThisType(
				"weapon_fists" );
			if ( !pFists )
			{
				pFists = dynamic_cast< CBaseCombatWeapon * >(
					pBot->GiveFoFNamedItem( "weapon_fists" ) );
			}
			if ( !pBot->GetActiveWeapon() )
			{
				if ( pFists )
					pBot->Weapon_Switch( pFists );
			}
			pBot->RecalculateWeaponSpeed();
			// The bot may have run one empty-inventory frame while its normal
			// spawn transaction was still pending.  Do not retain that frame's
			// randomized 3-5 second weapon-selection delay after the scripted
			// course inventory has now been installed.
			pState->m_flNextWeaponSelection = 0.0f;
		}
		else if ( !currentMode.IsValid() || currentMode.GetInt() != 3 )
		{
			// BreakBad already performs its cash purchase in OnPlayerSpawn.
			pBot->RefreshFoFEquipment();
		}
		pState->m_bEquipmentRefreshComplete = true;
	}
}

bool FoFGetBotProfileEquipment(
	CFoFBot *pBot, int *pItems, int nItemCount )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 3 )
	{
		// BreakBad buys from the bot's current cash budget. Reading ordinary
		// profile equipment here bypasses that selection with fixed loadouts.
		return false;
	}
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState || !pState->m_bConfigured || !pItems || nItemCount <= 0 )
		return false;

	for ( int i = 0; i < nItemCount; ++i )
	{
		pItems[i] = i < ARRAYSIZE( pState->m_nEquipment ) ?
			pState->m_nEquipment[i] : -1;
	}
	return true;
}

static const char *FoFForcedBotWeapon()
{
	static ConVarRef forceWeapon( "fof_bot_forceweapon", true );
	if ( !forceWeapon.IsValid() )
		return NULL;
	const char *pszWeapon = forceWeapon.GetString();
	return pszWeapon && pszWeapon[0] && Q_stricmp( pszWeapon, "0" ) ?
		pszWeapon : NULL;
}

static CBaseCombatWeapon *FoFGiveOrFindBotWeapon(
	CFoFBot *pBot, const char *pszClassname )
{
	if ( !pBot || !pszClassname || !pszClassname[0] )
		return NULL;

	CBaseCombatWeapon *pWeapon = pBot->Weapon_OwnsThisType( pszClassname );
	if ( !pWeapon )
	{
		pWeapon = dynamic_cast< CBaseCombatWeapon * >(
			pBot->GiveFoFNamedItem( pszClassname ) );
	}
	return pWeapon;
}

static bool FoFIsValidBotTarget( CFoFBot *pBot, CFoF_Player *pTarget )
{
	if ( !pBot || !pTarget || pTarget == pBot ||
		!pTarget->IsConnected() || !pTarget->IsAlive() ||
		pTarget->IsObserver() || pTarget->IsHLTV() || pTarget->IsReplay() ||
		pTarget->IsFoFBotGhost() || pTarget->IsFoFInvulnerable() ||
		( pTarget->GetFlags() & FL_NOTARGET ) )
	{
		return false;
	}

	return FoFPlayersAreEnemies( pBot, pTarget );
}

void FoFResetBotCombatState(
	CFoFBot *pBot, float flAttackDelay, bool bClearTarget )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;

	if ( bClearTarget )
	{
		pState->m_hTarget = NULL;
		pState->m_hSupportTarget = NULL;
		FoFClearBotActions( *pState );
		FoFClearBotPath( *pState );
		pState->m_flNextPathBuild = 0.0f;
	}
	pState->m_bChargingAttack = false;
	pState->m_flChargeRelease = 0.0f;
	pState->m_flNextTargetSearch = 0.0f;
	pState->m_flNextShot = gpGlobals->curtime + MAX( 0.0f, flAttackDelay );
}

void FoFClearBotTarget( CFoFBot *pBot )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;

	pState->m_hTarget = NULL;
	pState->m_hSupportTarget = NULL;
	FoFClearBotActions( *pState );
	FoFClearBotPath( *pState );
	pState->m_flNextPathBuild = 0.0f;
}

void FoFDelayBotNextAction( CFoFBot *pBot, float flDelay )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState )
		return;

	pState->m_flNextShot = gpGlobals->curtime + MAX( 0.0f, flDelay );
}

void FoFNotifyBotDamaged(
	CFoFBot *pBot, CFoF_Player *pAttacker )
{
	FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	if ( !pState || !FoFIsValidBotTarget( pBot, pAttacker ) )
		return;

	if ( pState->m_hTarget.Get() == pAttacker )
		return;

	CFoF_Player *pCurrentTarget = pState->m_hTarget.Get();
	if ( !pCurrentTarget )
		return;
	const float flAggroDistance = pBot->GetAbsOrigin().DistTo(
		pCurrentTarget->GetAbsOrigin() ) * 0.5f;
	if ( ( pAttacker->GetAbsOrigin() - pBot->GetAbsOrigin() ).LengthSqr() >
		flAggroDistance * flAggroDistance )
	{
		return;
	}

	pState->m_flNextTargetSearch = gpGlobals->curtime + 0.25f;
}

static CFoF_Player *FoFFindBotTarget( CFoFBot *pBot )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	const bool bUnlimitedDetection = nMode != 1 || FoFIsHorseRaceMap() ||
		( HL2MPRules() && HL2MPRules()->IsTeamplay() );
	float flDetectionRange = 0.0f;
	if ( !bUnlimitedDetection )
	{
		int nConnectedPlayers = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsConnected() &&
				!pPlayer->IsHLTV() && !pPlayer->IsReplay() )
			{
				++nConnectedPlayers;
			}
		}
		const float flPopulation = gpGlobals->maxClients > 0 ?
			static_cast< float >( nConnectedPlayers ) /
			static_cast< float >( gpGlobals->maxClients ) : 0.0f;
		flDetectionRange = RemapValClamped(
			flPopulation, 0.5f, 0.9f, 2500.0f, 850.0f );
	}
	const Vector origin = pBot->GetAbsOrigin();
	const FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
	const int nAttackStyle = pState ? pState->m_nPrimaryAttackStyle : 3;
	CBaseCombatWeapon *pWeapon = pBot->GetActiveWeapon();
	const bool bDynamite = pWeapon && pWeapon->FoFWeaponID() == 5;
	CFoF_Player *pBest = NULL;
	float flBestScore = -1000000.0f;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pTarget = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsValidBotTarget( pBot, pTarget ) )
			continue;

		const float flDistance =
			( pTarget->GetAbsOrigin() - origin ).Length();
		if ( !bUnlimitedDetection && flDistance > flDetectionRange )
			continue;

		// Visibility only affects the two close-range scoring branches.
		const bool bVisible = flDistance <= 1000.0f &&
			FoFBotCanSeeTarget( pBot, pTarget );
		float flScore = 0.0f;
		if ( flDistance <= 350.0f )
		{
			flScore = RemapValClamped(
				flDistance, 0.0f, 350.0f, 20.0f, 10.0f );
			flScore *= bVisible ? 3.0f : 0.5f;
		}
		else if ( flDistance <= 1000.0f )
		{
			flScore = RemapValClamped(
				flDistance, 350.0f, 1000.0f,
				nAttackStyle == 1 ? 12.0f : 8.0f, 0.0f );
			if ( bVisible )
				flScore += nAttackStyle == 5 ? 6.0f : 2.0f;

			if ( bDynamite )
				flScore += 2.0f;
			if ( bVisible && nAttackStyle != 1 &&
				pTarget->FInViewCone( pBot ) )
			{
				flScore += 3.0f;
			}
		}
		else
		{
			flScore = RemapValClamped(
				flDistance, 1000.0f, 3000.0f, 3.0f, 1.0f );
		}
		if ( flScore > flBestScore )
		{
			pBest = pTarget;
			flBestScore = flScore;
		}
	}
	return pBest;
}

static bool FoFBotCanSeeTarget( CFoFBot *pBot, CFoF_Player *pTarget )
{
	trace_t trace;
	UTIL_TraceLine( pBot->EyePosition(), pTarget->EyePosition(), MASK_SHOT,
		pBot, COLLISION_GROUP_NONE, &trace );
	return trace.fraction >= 0.99f || trace.m_pEnt == pTarget;
}

static void FoFUpdateBotForcedWeapon(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( pBot->GetTeamNumber() == FOF_TEAM_ZOMBIES )
		return;

	if ( gpGlobals->curtime < state.m_flNextWeaponCheck )
		return;
	state.m_flNextWeaponCheck = gpGlobals->curtime + 0.5f;

	const char *pszForcedWeapon = FoFForcedBotWeapon();
	if ( !pszForcedWeapon )
		return;
	CBaseCombatWeapon *pWeapon =
		FoFGiveOrFindBotWeapon( pBot, pszForcedWeapon );
	if ( pWeapon && pBot->GetActiveWeapon() != pWeapon )
		pBot->Weapon_Switch( pWeapon );
}

static bool FoFSelectBestBotWeapon(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, bool bMeleeOnly )
{
	if ( pBot->GetTeamNumber() == FOF_TEAM_ZOMBIES )
		return false;

	CBaseCombatWeapon *pBestFirst = NULL;
	CBaseCombatWeapon *pBestSecond = NULL;
	int nBestFirstPriority = INT_MIN;
	int nBestSecondPriority = INT_MIN;
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CBaseCombatWeapon *pWeapon = pBot->GetWeapon( i );
		if ( !pWeapon )
			continue;

		const FoFBotWeaponHeuristic_t *pHeuristic =
			FoFFindBotWeaponHeuristic( pWeapon->GetClassname() );
		if ( bMeleeOnly &&
			( !pHeuristic || pHeuristic->m_nPrimaryAttackStyle != 1 ) )
		{
			continue;
		}
		const int nPriority = pHeuristic ?
			pHeuristic->m_nPriority : pWeapon->GetWeight();
		const bool bSecondSelection = pHeuristic ?
			pHeuristic->m_bSecondSelection : pWeapon->IsSecondGun();
		if ( bSecondSelection )
		{
			if ( nPriority > nBestSecondPriority )
			{
				nBestSecondPriority = nPriority;
				pBestSecond = pWeapon;
			}
		}
		else if ( nPriority > nBestFirstPriority )
		{
			nBestFirstPriority = nPriority;
			pBestFirst = pWeapon;
		}
	}

	bool bSwitched = false;
	if ( pBestSecond && pBestSecond != pBot->GetActiveWeapon2() )
		bSwitched = pBot->Weapon_Switch( pBestSecond ) || bSwitched;
	if ( pBestFirst && pBestFirst != pBot->GetActiveWeapon1() )
		bSwitched = pBot->Weapon_Switch( pBestFirst ) || bSwitched;
	return bSwitched;
}

static void FoFUpdateBotWeaponSelection(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( pBot->GetTeamNumber() == FOF_TEAM_ZOMBIES )
		return;

	if ( ( pBot->GetFlags() & ( FL_FROZEN | FL_ATCONTROLS ) ) ||
		FoFForcedBotWeapon() ||
		gpGlobals->curtime < state.m_flNextWeaponSelection )
	{
		return;
	}
	state.m_flNextWeaponSelection = gpGlobals->curtime +
		random->RandomFloat( 3.0f, 5.0f );
	FoFSelectBestBotWeapon( pBot, state, false );
}

static bool FoFChooseBotWanderGoal(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( !pBot || !TheNavMesh || !TheNavMesh->IsLoaded() ||
		TheNavAreas.Count() <= 0 )
	{
		return false;
	}

	const Vector origin = pBot->GetAbsOrigin();
	if ( state.m_bCourseCompanion )
	{
		CCourseMode *pCourse = FoFFindCourseMode();
		Vector roamCenter = state.m_vecCourseCompanionHome;
		Vector captureOrigin;
		float flCaptureRadius = 0.0f;
		const bool bCaptureActive = pCourse &&
			pCourse->GetActiveCaptureZone(
				captureOrigin, flCaptureRadius );
		if ( bCaptureActive )
			roamCenter = captureOrigin;
		const float flRoamRadius = bCaptureActive ?
			MAX( 80.0f, flCaptureRadius - 56.0f ) :
			pCourse && pCourse->ShouldCourseCompanionsFollowPlayer() ?
				240.0f : 400.0f;
		CNavArea *pHomeArea = TheNavMesh->GetNearestNavArea(
			roamCenter,
			false, 500.0f, false, true, TEAM_ANY );
		if ( pHomeArea )
		{
			CUtlVector< CNavArea * > nearbyAreas;
			CollectSurroundingAreas(
				&nearbyAreas, pHomeArea, flRoamRadius + 100.0f );
			for ( int attempt = 0;
				attempt < 24 && nearbyAreas.Count() > 0; ++attempt )
			{
				CNavArea *pArea = nearbyAreas[
					random->RandomInt( 0, nearbyAreas.Count() - 1 )];
				if ( !pArea || pArea->IsBlocked( TEAM_ANY ) ||
					pArea->IsDamaging() || pArea->HasAvoidanceObstacle() )
				{
					continue;
				}

				const Vector goal = pArea->GetRandomPoint();
				if ( goal.DistToSqr( roamCenter ) >
					Square( flRoamRadius ) ||
					goal.DistToSqr( origin ) < Square( 70.0f ) )
				{
					continue;
				}
				trace_t goalTrace;
				UTIL_TraceHull( goal, goal,
					VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID,
					pBot, COLLISION_GROUP_PLAYER_MOVEMENT, &goalTrace );
				if ( goalTrace.startsolid || goalTrace.allsolid )
					continue;

				if ( FoFBuildBotPath( pBot, state, goal, false, false ) )
				{
					state.m_vecWanderGoal = goal;
					state.m_flNextWanderGoal = gpGlobals->curtime +
						random->RandomFloat( 2.0f, 4.0f );
					return true;
				}
			}
		}

		FoFClearBotPath( state );
		state.m_flNextWanderGoal = gpGlobals->curtime + 0.75f;
		return false;
	}

	for ( int attempt = 0; attempt < 16; ++attempt )
	{
		CNavArea *pArea = TheNavAreas[
			random->RandomInt( 0, TheNavAreas.Count() - 1 )];
		if ( !pArea || pArea->IsBlocked( TEAM_ANY ) ||
			pArea->IsDamaging() || pArea->HasAvoidanceObstacle() )
		{
			continue;
		}

		const Vector goal = pArea->GetRandomPoint();
		const float flDistanceSqr = ( goal - origin ).Length2DSqr();
		if ( flDistanceSqr < 320.0f * 320.0f ||
			flDistanceSqr > 6000.0f * 6000.0f )
		{
			continue;
		}

		if ( FoFBuildBotPath( pBot, state, goal, false, false ) )
		{
			state.m_vecWanderGoal = goal;
			state.m_flNextWanderGoal = gpGlobals->curtime +
				random->RandomFloat( 5.0f, 10.0f );
			return true;
		}
	}

	FoFClearBotPath( state );
	state.m_flNextWanderGoal = gpGlobals->curtime + 0.75f;
	return false;
}

static void FoFBuildBotWanderCommand(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	// Configured Course bots use 110 only for their no-target roaming
	// schedule. Once an enemy is selected, combat navigation uses the bot's
	// current m_flMaxspeed like the original server.
	const float flRoamSpeed = FoFIsConfiguredCourseBot( state ) ?
		110.0f : FoFBotMoveSpeed( pBot, state );
	cmd.viewangles = pBot->EyeAngles();
	cmd.viewangles.x = 0.0f;

	if ( state.m_bPathToTarget ||
		state.m_nPathIndex >= state.m_nPathCount ||
		gpGlobals->curtime >= state.m_flNextWanderGoal )
	{
		FoFChooseBotWanderGoal( pBot, state );
	}

	float flMoveYaw = state.m_flWanderYaw;
	Vector waypoint;
	if ( state.m_nPathIndex < state.m_nPathCount &&
		FoFBuildBotNavigationCommand( pBot, state,
			state.m_vecWanderGoal, false, flRoamSpeed, cmd,
			&flMoveYaw, &waypoint ) )
	{
		state.m_flWanderYaw = flMoveYaw;
		cmd.viewangles.y = UTIL_ApproachAngle(
			flMoveYaw, cmd.viewangles.y,
			120.0f * gpGlobals->frametime );
		FoFSetBotMoveToward( cmd.viewangles, pBot->GetAbsOrigin(),
			waypoint, flRoamSpeed, cmd, NULL );

		return;
	}

	if ( gpGlobals->curtime >= state.m_flNextWanderTurn )
	{
		state.m_flNextWanderTurn =
			gpGlobals->curtime + random->RandomFloat( 0.8f, 2.2f );
		state.m_flWanderYaw = AngleNormalize(
			state.m_flWanderYaw + random->RandomFloat( -80.0f, 80.0f ) );
	}
	cmd.viewangles.y = UTIL_ApproachAngle(
		state.m_flWanderYaw, cmd.viewangles.y,
		120.0f * gpGlobals->frametime );
	cmd.forwardmove = flRoamSpeed;
}

static float FoFBotStrafeClearance(
	CFoFBot *pBot, float flDirection, float flMoveSpeed )
{
	QAngle moveAngles( 0.0f, pBot->EyeAngles().y, 0.0f );
	Vector vecRight;
	AngleVectors( moveAngles, NULL, &vecRight, NULL );

	const Vector vecStart =
		pBot->GetAbsOrigin() + Vector( 0.0f, 0.0f, 20.0f );
	const float flProbeDistance = random->RandomFloat( 0.4f, 0.6f ) *
		flMoveSpeed;
	const Vector vecEnd = vecStart +
		vecRight * ( flDirection * flProbeDistance );

	trace_t trace;
	UTIL_TraceHull( vecStart, vecEnd,
		VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID,
		pBot, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
	return trace.fraction;
}

static void FoFUpdateBotStrafe( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, float flDistance, float flMoveSpeed )
{
	if ( gpGlobals->curtime < state.m_flStrafeEnd )
		return;

	state.m_flStrafeMove = 0.0f;
	if ( gpGlobals->curtime < state.m_flNextStrafeChange )
		return;

	const int nStrafe = clamp(
		RoundFloatToInt( state.m_flProfileStrafe ), 0, 10 );
	if ( nStrafe <= 0 || random->RandomInt( 1, 10 ) > nStrafe )
	{
		state.m_flNextStrafeChange = gpGlobals->curtime +
			random->RandomFloat( 0.25f, 0.55f );
		return;
	}

	float flDirection;
	if ( state.m_flStrafeBias < -0.25f )
		flDirection = 1.0f;
	else if ( state.m_flStrafeBias > 0.25f )
		flDirection = -1.0f;
	else
		flDirection = random->RandomInt( 0, 100 ) < 50 ? -1.0f : 1.0f;

	float flClearance = FoFBotStrafeClearance(
		pBot, flDirection, flMoveSpeed );
	if ( flClearance < 0.3f )
	{
		flDirection = -flDirection;
		flClearance = FoFBotStrafeClearance(
			pBot, flDirection, flMoveSpeed );
	}
	if ( flClearance <= 0.3f )
		return;

	state.m_flStrafeMove = flDirection * flMoveSpeed;
	state.m_flStrafeEnd = gpGlobals->curtime + random->RandomFloat(
		0.35f, MAX( 0.35f, flClearance * 0.6f ) );

	const float flDistanceFactor = RemapValClamped(
		flDistance, 50.0f, 500.0f, 0.0f, 1.0f );
	const float flNextScale = 4.0f - flDistanceFactor * 2.0f;
	const float flNextBase = random->RandomFloat( 0.15f, 0.25f );
	state.m_flNextStrafeChange = gpGlobals->curtime +
		Lerp( 0.375f, flNextBase, flNextScale );
	state.m_flStrafeBias += flDirection > 0.0f ? 0.05f : -0.05f;
}

static bool FoFIsDynamiteWeapon( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	const char *pszClassname = pWeapon->GetClassname();
	return pszClassname &&
		( !Q_stricmp( pszClassname, "weapon_dynamite" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_black" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_belt" ) );
}

static bool FoFBotWeaponHasPrimaryAmmo( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;
	return !pWeapon->UsesClipsForAmmo1() || pWeapon->Clip1() > 0;
}

static float FoFBotTurnValue(
	CFoFBot *pBot, const FoFBotRuntimeState_t &state )
{
	const float flSpeedFraction = clamp(
		pBot->GetAbsVelocity().Length2D() * 0.004f, 0.0f, 1.0f );
	return ( 2.0f - flSpeedFraction ) * state.m_flProfileRotationSpeed;
}

static void FoFUpdateBotViewAngles( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, const QAngle &desiredAngles,
	CUserCmd &cmd )
{
	cmd.viewangles = pBot->EyeAngles();
	const float flTurnFactor =
		( clamp( ( FoFBotTurnValue( pBot, state ) - 5.0f ) * 0.1f,
			0.0f, 1.0f ) + 1.0f ) * 0.5f;
	const float flAcceleration = flTurnFactor * 200.0f;
	const float flDamping = flTurnFactor * 25.0f;
	const float flMaximumAcceleration = flTurnFactor * 3000.0f;
	const float flFrameTime = gpGlobals->frametime;

	const float flYawError = AngleDiff(
		desiredAngles.y, cmd.viewangles.y );
	if ( fabsf( flYawError ) >= 1.0f )
	{
		const float flYawAcceleration = clamp(
			flYawError * flAcceleration -
			state.m_flViewYawVelocity * flDamping,
			-flMaximumAcceleration, flMaximumAcceleration );
		state.m_flViewYawVelocity += flYawAcceleration * flFrameTime;
		cmd.viewangles.y = AngleNormalize(
			cmd.viewangles.y + state.m_flViewYawVelocity * flFrameTime );
	}
	else
	{
		state.m_flViewYawVelocity = 0.0f;
		cmd.viewangles.y = desiredAngles.y;
	}

	const float flPitchError = AngleDiff(
		desiredAngles.x, cmd.viewangles.x );
	const float flPitchAcceleration = clamp(
		flPitchError * flAcceleration * 2.0f -
		state.m_flViewPitchVelocity * flDamping,
		-flMaximumAcceleration, flMaximumAcceleration );
	state.m_flViewPitchVelocity += flPitchAcceleration * flFrameTime;
	cmd.viewangles.x = clamp( AngleNormalize(
		cmd.viewangles.x + state.m_flViewPitchVelocity * flFrameTime ),
		-89.0f, 89.0f );
	cmd.viewangles.z = 0.0f;
}

static void FoFUpdateBotAimReadiness( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, const Vector &vecTarget,
	float flDistance, bool bVisible, const CUserCmd &cmd )
{
	Vector vecForward;
	AngleVectors( cmd.viewangles, &vecForward );
	Vector vecDirection = vecTarget - pBot->EyePosition();
	VectorNormalize( vecDirection );
	const float flBaseDot =
		state.m_nPrimaryAttackStyle == 1 ? 0.8f : 0.9f;
	const float flRequiredDot = Lerp(
		clamp( flDistance / 150.0f, 0.0f, 1.0f ),
		flBaseDot, 0.97f );
	if ( bVisible && DotProduct( vecForward, vecDirection ) >= flRequiredDot )
		state.m_flAimOnTargetTime += gpGlobals->frametime;
	else
		state.m_flAimOnTargetTime = 0.0f;
}

static bool FoFFindBotDynamiteAim( CFoFBot *pBot,
	const FoFBotRuntimeState_t &state, CFoF_Player *pTarget,
	float flDistance, QAngle &aimAngles, Vector &vecThrowVelocity )
{
	vecThrowVelocity.Init();
	if ( !pTarget || flDistance < state.m_flWeaponMinRange ||
		flDistance > state.m_flWeaponRange )
	{
		return false;
	}

	const Vector vecSource = pBot->Weapon_ShootPosition();
	Vector vecTarget = pTarget->WorldSpaceCenter();
	vecTarget += pTarget->GetAbsVelocity() * clamp(
		flDistance / 600.0f, 0.0f, 1.0f );
	Vector vecMins( -4.0f, -4.0f, -4.0f );
	Vector vecMaxs( 4.0f, 4.0f, 4.0f );
	Vector vecThrow;
	if ( FoFBotCanSeeTarget( pBot, pTarget ) )
	{
		vecThrow = VecCheckThrow(
			pBot, vecSource, vecTarget, 600.0f, 1.0f,
			&vecMins, &vecMaxs );
	}
	else
	{
		vecThrow = VecCheckToss(
			pBot, vecSource, vecTarget, -1.0f, 1.0f, true,
			&vecMins, &vecMaxs );
	}
	if ( vecThrow.LengthSqr() <= 1.0f )
		return false;

	vecThrowVelocity = vecThrow;
	VectorAngles( vecThrow, aimAngles );
	aimAngles.x = clamp( AngleNormalize( aimAngles.x ), -89.0f, 89.0f );
	aimAngles.y = AngleNormalize( aimAngles.y );
	aimAngles.z = 0.0f;
	return true;
}

static int FoFSelectBotFireButton( CFoFBot *pBot )
{
	if ( !pBot || !pBot->HasDualActiveWeapons() )
		return IN_ATTACK;

	// FoF's dual-revolver input routing is intentionally reversed relative to
	// the viewmodel indices: the first/right leaf consumes IN_ATTACK2 and the
	// second/left leaf consumes IN_ATTACK. FoF chooses a loaded hand
	// rather than driving both leaves with the same generic HL2MP attack bit.
	CBaseCombatWeapon *pFirst = pBot->GetActiveWeapon1();
	CBaseCombatWeapon *pSecond = pBot->GetActiveWeapon2();
	const bool bFirstLoaded = FoFBotWeaponHasPrimaryAmmo( pFirst );
	const bool bSecondLoaded = FoFBotWeaponHasPrimaryAmmo( pSecond );
	if ( bFirstLoaded && !bSecondLoaded )
		return IN_ATTACK2;
	if ( bSecondLoaded && !bFirstLoaded )
		return IN_ATTACK;
	return random->RandomInt( 0, 1 ) ? IN_ATTACK2 : IN_ATTACK;
}

static CBaseCombatWeapon *FoFFindBotWhiskey( CFoFBot *pBot )
{
	CBaseCombatWeapon *pSecondChoice = NULL;
	for ( int i = 0; pBot && i < MAX_WEAPONS; ++i )
	{
		CBaseCombatWeapon *pWeapon = pBot->GetWeapon( i );
		if ( !pWeapon )
			continue;
		if ( FClassnameIs( pWeapon, "weapon_whiskey" ) )
			return pWeapon;
		if ( FClassnameIs( pWeapon, "weapon_whiskey2" ) )
			pSecondChoice = pWeapon;
	}
	return pSecondChoice;
}

static float FoFBotWhiskeyFraction( CFoFBot *pBot )
{
	CBaseCombatWeapon *pWhiskey = FoFFindBotWhiskey( pBot );
	if ( !pWhiskey )
		return 0.0f;
	const int nMaximum = pWhiskey->GetMaxClip1();
	return nMaximum > 0 ? clamp(
		static_cast< float >( pWhiskey->Clip1() ) /
		static_cast< float >( nMaximum ), 0.0f, 1.0f ) : 0.0f;
}

static CFoF_Player *FoFFindBotSupportTarget(
	CFoFBot *pBot, float flMaxDistance )
{
	if ( !pBot || !HL2MPRules() || !HL2MPRules()->IsTeamplay() ||
		pBot->GetTeamNumber() < FIRST_GAME_TEAM )
	{
		return NULL;
	}

	CFoF_Player *pBest = NULL;
	float flBestDistanceSqr = flMaxDistance * flMaxDistance;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer == pBot || !pPlayer->IsAlive() ||
			pPlayer->IsObserver() || pPlayer->IsFoFBotGhost() ||
			pPlayer->GetTeamNumber() != pBot->GetTeamNumber() ||
			pPlayer->GetHealth() >= 76 )
		{
			continue;
		}

		const float flDistanceSqr = pBot->GetAbsOrigin().DistToSqr(
			pPlayer->GetAbsOrigin() );
		if ( flDistanceSqr < flBestDistanceSqr )
		{
			flBestDistanceSqr = flDistanceSqr;
			pBest = pPlayer;
		}
	}
	return pBest;
}

static bool FoFBuildBotSupportCommand( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, CFoF_Player *pTeammate, CUserCmd &cmd )
{
	if ( !pTeammate || !pTeammate->IsAlive() || pTeammate->IsObserver() ||
		pTeammate->GetTeamNumber() != pBot->GetTeamNumber() ||
		pTeammate->GetHealth() >= 76 )
	{
		state.m_hSupportTarget = NULL;
		state.m_flNextWeaponSelection = 0.0f;
		return false;
	}

	CBaseCombatWeapon *pWhiskey = FoFFindBotWhiskey( pBot );
	if ( !pWhiskey )
	{
		state.m_hSupportTarget = NULL;
		return false;
	}
	if ( pBot->GetActiveWeapon1() != pWhiskey &&
		pBot->GetActiveWeapon2() != pWhiskey )
	{
		pBot->Weapon_Switch(
			pWhiskey, pWhiskey->IsSecondGun() ? 1 : 0 );
	}
	if ( pWhiskey->Clip1() <= 0 )
	{
		const int nAmmoType = pWhiskey->GetPrimaryAmmoType();
		if ( nAmmoType < 0 || pBot->GetAmmoCount( nAmmoType ) <= 0 )
		{
			state.m_hSupportTarget = NULL;
			return false;
		}
		const int nReloadAmount = MIN(
			pWhiskey->GetMaxClip1(), pBot->GetAmmoCount( nAmmoType ) );
		pWhiskey->m_iClip1 = nReloadAmount;
		pBot->RemoveAmmo( nReloadAmount, nAmmoType );
		pWhiskey->SendWeaponAnim( ACT_VM_RELOAD );
		state.m_flNextShot = gpGlobals->curtime + 0.75f;
		cmd.buttons |= IN_RELOAD;
		cmd.forwardmove = 0.0f;
		cmd.sidemove = 0.0f;
		return true;
	}

	const Vector targetPoint =
		pTeammate->WorldSpaceCenter() + Vector( 0.0f, 0.0f, 20.0f );
	FoFAimBotToward( pBot, targetPoint, cmd );
	const float flDistance = pBot->WorldSpaceCenter().DistTo(
		pTeammate->WorldSpaceCenter() );
	if ( flDistance > 135.0f )
	{
		float flMoveYaw = cmd.viewangles.y;
		if ( !FoFBuildBotNavigationCommand( pBot, state,
			pTeammate->GetAbsOrigin(), true,
			MAX( 1.0f, pBot->GetPlayerMaxSpeed() ), cmd, &flMoveYaw ) )
		{
			cmd.forwardmove = MAX( 1.0f, pBot->GetPlayerMaxSpeed() );
		}
		return true;
	}

	cmd.forwardmove = 0.0f;
	cmd.sidemove = 0.0f;
	trace_t trace;
	UTIL_TraceLine( pBot->Weapon_ShootPosition(), targetPoint,
		MASK_SHOT_HULL, pBot, COLLISION_GROUP_NONE, &trace );
	if ( ( trace.fraction >= 0.98f || trace.m_pEnt == pTeammate ) &&
		gpGlobals->curtime >= state.m_flNextShot )
	{
		int nFireButton = IN_ATTACK;
		if ( pBot->HasDualActiveWeapons() &&
			pBot->GetActiveWeapon1() == pWhiskey )
		{
			nFireButton = IN_ATTACK2;
		}
		cmd.buttons |= nFireButton;
		state.m_flNextShot = gpGlobals->curtime + 0.55f;
	}
	return true;
}

static float FoFBotShootDelay( const FoFBotRuntimeState_t &state )
{
	if ( FoFIsConfiguredCourseBot( state ) )
		return state.m_flProfileShootDelay;

	static ConVarRef skill( "fof_bot_skill", true );
	switch ( skill.IsValid() ? skill.GetInt() : 2 )
	{
	case 0: return 5.0f;
	case 1: return 3.75f;
	case 2: return 2.75f;
	case 3: return 1.75f;
	case 4: return 0.9f;
	case 5:
	case 6: return state.m_flProfileShootDelay;
	default: return 1.0f;
	}
}

static void FoFBuildBotAttackCommand( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, bool bVisible, float flDistance,
	float flAimError,
	CUserCmd &cmd )
{
	CBaseCombatWeapon *pWeapon = pBot->GetActiveWeapon();
	const bool bLowHealthMeleeThrow =
		state.m_nPrimaryAttackStyle == 1 &&
		state.m_nSecondaryAttackStyle == 2 &&
		pBot->GetHealth() <= 32 && flDistance > 100.0f &&
		flDistance < 200.0f;
	float flUsableRange = bLowHealthMeleeThrow ?
		200.0f : state.m_flWeaponRange;
	CFoF_Player *pTarget = state.m_hTarget.Get();
	if ( state.m_nPrimaryAttackStyle == 1 && pTarget &&
		!pTarget->GetGroundEntity() )
	{
		flUsableRange *= 2.0f;
	}
	if ( !pWeapon || flUsableRange <= 0.0f ||
		flDistance > flUsableRange )
	{
		state.m_bChargingAttack = false;
		return;
	}
	if ( !bVisible && state.m_nPrimaryAttackStyle != 1 &&
		state.m_nPrimaryAttackStyle != 7 )
	{
		state.m_bChargingAttack = false;
		return;
	}
	// The original bot main holds secondary attack for
	// attack style 5 while it has a visible target. This is the FoF sight/draw
	// input used by rifles and bows; omitting it leaves bow bots unable to fire.
	if ( state.m_nPrimaryAttackStyle == 5 )
		cmd.buttons |= IN_ATTACK2;

	if ( state.m_nPrimaryAttackStyle != 1 &&
		state.m_nPrimaryAttackStyle != 7 &&
		pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() <= 0 )
	{
		// FoF's ordinary revolver reload starts on release. The original bot
		// alternates this input instead of holding it until the clip refills.
		if ( !pBot->FoFIsReloading() && pWeapon->FoFWeaponID() != 1 &&
			random->RandomInt( 0, 3 ) > 1 )
		{
			cmd.buttons |= IN_RELOAD;
		}
		state.m_bChargingAttack = false;
		return;
	}
	const float flAimWait = state.m_nPrimaryAttackStyle == 7 ?
		state.m_flProfileAimTrailing : RemapValClamped(
			state.m_flProfileAimTrailing, 1.2f, 0.5f, 1.0f, 0.2f );
	if ( state.m_flAimOnTargetTime < flAimWait )
		return;

	if ( state.m_nPrimaryAttackStyle == 7 )
	{
		const float flTolerance = RemapValClamped(
			state.m_flProfileAimTrailing, 1.2f, 0.5f, 13.0f, 2.0f );
		if ( flAimError > flTolerance )
			return;
	}

	// Style 7 is the dynamite family. It must press, hold through the pullback,
	// then release IN_ATTACK; treating it as a one-frame firearm click leaves
	// the weapon permanently paused in its draw animation.
	if ( state.m_nPrimaryAttackStyle == 7 &&
		FoFIsDynamiteWeapon( pWeapon ) )
	{
		if ( state.m_bChargingAttack )
		{
			if ( gpGlobals->curtime < state.m_flChargeRelease )
				cmd.buttons |= IN_ATTACK;
			else
			{
				state.m_bChargingAttack = false;
				state.m_flNextShot = gpGlobals->curtime + 0.35f;
			}
			return;
		}
		if ( gpGlobals->curtime >= state.m_flNextShot )
		{
			state.m_bChargingAttack = true;
			state.m_flChargeRelease = gpGlobals->curtime +
				RemapValClamped(
					state.m_flProfileAverageSkill,
					0.0f, 10.0f, 1.35f, 0.75f );
			cmd.buttons |= IN_ATTACK;
		}
		return;
	}

	if ( state.m_nPrimaryAttackStyle == 5 )
	{
		if ( !pBot->GetGroundEntity() ||
			gpGlobals->curtime <= state.m_flNextAimedShot )
		{
			return;
		}

		const float flDistanceFactor = RemapValClamped(
			flDistance, 1500.0f, 10.0f, 1.15f, 0.85f );
		float flRequiredSight = clamp( flDistanceFactor * RemapValClamped(
			state.m_flProfileShootDelay, 5.0f, 0.8f, 0.9f, 0.97f ),
			0.8f, 0.99f );
		if ( pWeapon->FoFWeaponID() == 1 )
		{
			flRequiredSight = RemapValClamped(
				state.m_flProfileShootDelay, 5.0f, 0.8f, 0.85f, 1.0f );
			state.m_flNextAimedShot = gpGlobals->curtime +
				pWeapon->GetFireRate() * RemapValClamped(
					state.m_flProfileAverageSkill, 1.0f, 10.0f, 5.5f, 1.5f );
		}
		if ( flDistance < 150.0f ||
			pBot->GetFoFSightExpFactor() >= flRequiredSight )
		{
			cmd.buttons |= IN_ATTACK;
		}
		return;
	}

	if ( state.m_nPrimaryAttackStyle == 1 )
	{
		int nAttackButton = IN_ATTACK;
		if ( state.m_nSecondaryAttackStyle == 1 )
		{
			// Fists alternate their two punches in FoF.
			nAttackButton = random->RandomInt( 0, 10 ) <= 5 ?
				IN_ATTACK2 : IN_ATTACK;
		}
		else if ( bLowHealthMeleeThrow )
		{
			nAttackButton = IN_ATTACK2;
		}

		cmd.buttons |= nAttackButton;
		return;
	}

	if ( state.m_nPrimaryAttackStyle == 3 ||
		state.m_nPrimaryAttackStyle == 4 )
	{
		// The profile is mapped to a short shot timer, not used as seconds
		// directly between shots. FoF fires near the end of that timer.
		if ( gpGlobals->curtime >= state.m_flNextShot )
		{
			state.m_flNextShot = gpGlobals->curtime + RemapValClamped(
				FoFBotShootDelay( state ), 0.8f, 5.0f, 0.3f, 1.15f );
		}
		else if ( gpGlobals->curtime >
			state.m_flNextShot - random->RandomFloat( 0.25f, 0.35f ) )
		{
			cmd.buttons |= FoFSelectBotFireButton( pBot );
			state.m_flNextShot = 0.0f;
		}
	}
	else if ( gpGlobals->curtime >= state.m_flNextShot )
	{
		cmd.buttons |= FoFSelectBotFireButton( pBot );
		state.m_flNextShot = gpGlobals->curtime +
			state.m_flProfileShootDelay;
	}

	if ( flDistance < 65.0f &&
		gpGlobals->curtime > state.m_flNextKick &&
		random->RandomInt( 0, 1500 ) > 1000 )
	{
		state.m_flNextKick = gpGlobals->curtime + 4.0f;
		cmd.buttons |= IN_SPEED;
	}
}

static Vector FoFBotRangedAimTarget( CFoFBot *pBot,
	const FoFBotRuntimeState_t &state, CFoF_Player *pTarget,
	float flDistance )
{
	Vector vecTarget = pTarget->EyePosition();
	if ( state.m_nPrimaryAttackStyle != 3 &&
		state.m_nPrimaryAttackStyle != 4 &&
		state.m_nPrimaryAttackStyle != 5 )
	{
		return vecTarget;
	}

	CBaseCombatWeapon *pWeapon = pBot->GetActiveWeapon();
	const float flBowLift = pWeapon && pWeapon->FoFWeaponID() == 1 ?
		RemapValClamped( flDistance, 400.0f, 1500.0f, 0.0f, 180.0f ) : 0.0f;
	vecTarget.z += flBowLift - RemapValClamped(
		flDistance, 20.0f, 100.0f, 5.0f, 20.0f );

	// FoF first aims below eye height. If that point is obstructed, the
	// visible-target branch falls back to the eye position plus bow lift.
	if ( FoFBotCanSeeTarget( pBot, pTarget ) )
	{
		trace_t trace;
		UTIL_TraceLine( pBot->EyePosition(), vecTarget,
			MASK_SHOT & ~CONTENTS_GRATE, pBot, COLLISION_GROUP_NONE, &trace );
		if ( trace.m_pEnt != pTarget )
			vecTarget = pTarget->EyePosition() + Vector( 0, 0, flBowLift );
	}
	return vecTarget;
}

static void FoFBuildCachedBotAttackCommand( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, CFoF_Player *pTarget, CUserCmd &cmd )
{
	const float flDistance =
		( pTarget->GetAbsOrigin() - pBot->GetAbsOrigin() ).Length();
	Vector vecTarget = FoFBotRangedAimTarget(
		pBot, state, pTarget, flDistance );
	const float flLeadScale = RemapValClamped(
		state.m_flProfileAimTrailing,
		1.2f, 0.5f, 0.0f, 0.16f );
	vecTarget += pTarget->GetAbsVelocity() * flLeadScale;

	QAngle desiredAngles;
	VectorAngles( vecTarget - pBot->EyePosition(), desiredAngles );
	desiredAngles.x += state.m_flAimPitchNoise;
	desiredAngles.y += state.m_flAimYawNoise;
	desiredAngles.z = 0.0f;

	const bool bVisible = FoFBotCanSeeTarget( pBot, pTarget );
	const float flAimError = MAX(
		fabsf( AngleDiff( desiredAngles.x, cmd.viewangles.x ) ),
		fabsf( AngleDiff( desiredAngles.y, cmd.viewangles.y ) ) );
	FoFBuildBotAttackCommand(
		pBot, state, bVisible, flDistance, flAimError, cmd );
}

static void FoFBuildBotCombatCommand( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, CFoF_Player *pTarget, CUserCmd &cmd )
{
	const float flDistance =
		( pTarget->GetAbsOrigin() - pBot->GetAbsOrigin() ).Length();
	CBaseCombatWeapon *pWeapon = pBot->GetActiveWeapon();
	const bool bDynamite = state.m_nPrimaryAttackStyle == 7 &&
		FoFIsDynamiteWeapon( pWeapon );
	if ( bDynamite && flDistance < state.m_flWeaponMinRange )
	{
		if ( FoFSelectBestBotWeapon( pBot, state, true ) )
		{
			state.m_flNextShot = gpGlobals->curtime +
				random->RandomFloat( 2.0f, 4.0f );
		}
		return;
	}

	Vector vecTarget = FoFBotRangedAimTarget(
		pBot, state, pTarget, flDistance );
	const float flLeadScale = RemapValClamped(
		state.m_flProfileAimTrailing,
		1.2f, 0.5f, 0.0f, 0.16f );
	vecTarget += pTarget->GetAbsVelocity() * flLeadScale;

	QAngle desiredAngles;
	Vector vecDynamiteThrowVelocity;
	const bool bCanThrowDynamite = bDynamite && FoFFindBotDynamiteAim(
		pBot, state, pTarget, flDistance, desiredAngles,
		vecDynamiteThrowVelocity );
	if ( bDynamite )
	{
		FoFSetBotDynamiteThrowVelocity(
			pBot, bCanThrowDynamite ?
				vecDynamiteThrowVelocity : vec3_origin );
	}
	if ( !bCanThrowDynamite )
		VectorAngles( vecTarget - pBot->EyePosition(), desiredAngles );
	if ( gpGlobals->curtime >= state.m_flNextTargetSearch )
	{
		const float flNoise = RemapValClamped(
			state.m_flProfileAimTrailing,
			1.2f, 0.5f, 7.0f, 0.2f );
		state.m_flAimPitchNoise = random->RandomFloat( -flNoise, flNoise );
		state.m_flAimYawNoise = random->RandomFloat( -flNoise, flNoise );
	}
	if ( !bCanThrowDynamite )
	{
		desiredAngles.x += state.m_flAimPitchNoise;
		desiredAngles.y += state.m_flAimYawNoise;
	}
	desiredAngles.z = 0.0f;

	FoFUpdateBotViewAngles( pBot, state, desiredAngles, cmd );

	const bool bVisible = bDynamite ? bCanThrowDynamite :
		FoFBotCanSeeTarget( pBot, pTarget );
	float flEngagementRange = MAX( 33.0f, state.m_flWeaponRange );
	if ( state.m_nPrimaryAttackStyle == 1 &&
		state.m_nSecondaryAttackStyle == 2 && pBot->GetHealth() <= 32 )
	{
		flEngagementRange = 200.0f;
	}
	else if ( state.m_nPrimaryAttackStyle != 1 &&
		state.m_nPrimaryAttackStyle != 5 )
	{
		flEngagementRange = MIN( flEngagementRange, 600.0f );
	}
	const float flMoveSpeed = FoFBotMoveSpeed( pBot, state );
	float flMoveYaw = desiredAngles.y;
	if ( !bVisible || ( bDynamite && !bCanThrowDynamite ) )
	{
		if ( !FoFBuildBotNavigationCommand( pBot, state,
			pTarget->GetAbsOrigin(), true, flMoveSpeed, cmd,
			&flMoveYaw ) )
		{
			cmd.forwardmove = flMoveSpeed;
		}
	}
	else
	{
		if ( bDynamite )
		{
			state.m_flStrafeMove = 0.0f;
			cmd.forwardmove = flDistance > flEngagementRange ?
				flMoveSpeed : 0.0f;
			cmd.sidemove = 0.0f;
		}
		else
		{
			FoFUpdateBotStrafe(
				pBot, state, flDistance, flMoveSpeed );
			if ( flDistance > flEngagementRange )
				cmd.forwardmove = flMoveSpeed;
			cmd.sidemove = clamp(
				state.m_flStrafeMove, -flMoveSpeed, flMoveSpeed );
		}
	}

	Vector vecReadinessTarget = vecTarget;
	if ( bCanThrowDynamite )
	{
		Vector vecAimForward;
		AngleVectors( desiredAngles, &vecAimForward );
		vecReadinessTarget = pBot->EyePosition() + vecAimForward * 1000.0f;
	}
	if ( bDynamite )
	{
		FoFUpdateBotAimReadiness( pBot, state, vecReadinessTarget,
			flDistance, bVisible, cmd );
	}

	const float flAimError = MAX(
		fabsf( AngleDiff( desiredAngles.x, cmd.viewangles.x ) ),
		fabsf( AngleDiff( desiredAngles.y, cmd.viewangles.y ) ) );
	FoFBuildBotAttackCommand(
		pBot, state, bVisible, flDistance, flAimError, cmd );
}

static bool FoFBuildBotEscapePath(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, Vector &goal )
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() || TheNavAreas.Count() <= 0 )
		return false;

	const Vector origin = pBot->GetAbsOrigin();
	for ( int attempt = 0; attempt < 24; ++attempt )
	{
		CNavArea *pArea = TheNavAreas[
			random->RandomInt( 0, TheNavAreas.Count() - 1 )];
		if ( !pArea || pArea->IsBlocked( TEAM_ANY ) ||
			pArea->IsDamaging() || pArea->HasAvoidanceObstacle() ||
			( pArea->GetAttributes() & NAV_MESH_AVOID ) )
			continue;

		const Vector candidate = pArea->GetRandomPoint();
		const float flDistanceSqr = origin.DistToSqr( candidate );
		if ( flDistanceSqr < Square( 120.0f ) ||
			flDistanceSqr > Square( 700.0f ) )
		{
			continue;
		}
		if ( FoFBuildBotPath( pBot, state, candidate, false, true ) )
		{
			goal = candidate;
			return true;
		}
	}
	return false;
}

static bool FoFPlanBotHazardEscape(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	if ( !pBot || !TheNavMesh || !TheNavMesh->IsLoaded() )
		return false;

	CNavArea *pArea = TheNavMesh->GetNearestNavArea(
		pBot->GetAbsOrigin(), true, 100.0f, false, false, TEAM_ANY );
	const bool bHazardArea = pArea &&
		( pArea->IsDamaging() || pArea->HasAvoidanceObstacle() ||
		  ( pArea->GetAttributes() & NAV_MESH_AVOID ) );
	if ( !bHazardArea && !pBot->IsOnFire() )
		return false;

	FoFBotAction_t *pCurrent = FoFCurrentBotAction( state );
	if ( pCurrent && pCurrent->m_nType == FOF_BOT_ACTION_UNSTUCK )
		return true;

	Vector escapeGoal;
	FoFClearBotActions( state );
	FoFClearBotObjective( state, false );
	FoFClearBotPath( state );
	if ( !FoFBuildBotEscapePath( pBot, state, escapeGoal ) )
		return false;

	FoFAppendBotAction( state, FOF_BOT_ACTION_UNSTUCK,
		escapeGoal, NULL, gpGlobals->curtime + 5.0f );
	return true;
}

static bool FoFBotMoveEntersAvoidArea(
	CFoFBot *pBot, const CUserCmd &cmd )
{
	if ( !pBot || !TheNavMesh || !TheNavMesh->IsLoaded() )
		return false;

	QAngle moveAngles( 0.0f, cmd.viewangles[YAW], 0.0f );
	Vector forward, right;
	AngleVectors( moveAngles, &forward, &right, NULL );
	Vector moveDirection =
		forward * cmd.forwardmove + right * cmd.sidemove;
	moveDirection.z = 0.0f;
	if ( VectorNormalize( moveDirection ) <= 0.001f )
		return false;

	const Vector start = pBot->GetAbsOrigin();
	const Vector projected = start + moveDirection * 40.0f;
	trace_t forwardTrace;
	UTIL_TraceHull( start, projected, VEC_HULL_MIN, VEC_HULL_MAX,
		MASK_PLAYERSOLID_BRUSHONLY, pBot,
		COLLISION_GROUP_PLAYER_MOVEMENT, &forwardTrace );
	if ( forwardTrace.fraction <= 0.9f )
		return false;

	trace_t groundTrace;
	UTIL_TraceLine( forwardTrace.endpos,
		forwardTrace.endpos - Vector( 0.0f, 0.0f, 300.0f ),
		MASK_PLAYERSOLID_BRUSHONLY, pBot,
		COLLISION_GROUP_PLAYER_MOVEMENT, &groundTrace );
	if ( groundTrace.fraction > 0.9f )
		return true;

	CNavArea *pArea = TheNavMesh->GetNearestNavArea(
		groundTrace.endpos, true, 50.0f, false, false, TEAM_ANY );
	return !pArea ||
		( pArea->GetAttributes() & NAV_MESH_AVOID ) != 0;
}

static void FoFConstrainBotHazardMovement(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	if ( !FoFBotMoveEntersAvoidArea( pBot, cmd ) )
		return;

	cmd.forwardmove = 0.0f;
	cmd.sidemove = 0.0f;
	cmd.buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );
	state.m_bHasCachedCommand = false;
	state.m_flNextPathBuild = 0.0f;
	FoFClearBotPath( state );

	Vector escapeGoal;
	if ( FoFBuildBotEscapePath( pBot, state, escapeGoal ) )
	{
		FoFClearBotActions( state );
		FoFAppendBotAction( state, FOF_BOT_ACTION_UNSTUCK,
			escapeGoal, NULL, gpGlobals->curtime + 5.0f );
	}
}

static void FoFApplyBotStuckRecovery( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, CUserCmd &cmd )
{
	const bool bTryingToMove =
		fabsf( cmd.forwardmove ) > 20.0f || fabsf( cmd.sidemove ) > 20.0f;
	if ( gpGlobals->curtime >= state.m_flNextStuckCheck )
	{
		const float flDistanceMoved =
			( pBot->GetAbsOrigin() - state.m_vecLastPosition ).Length2D();
		if ( bTryingToMove && flDistanceMoved < 8.0f )
		{
			++state.m_nStuckChecks;
			state.m_flStuckRecoverySide =
				( state.m_nStuckChecks & 1 ) ? 320.0f : -320.0f;
			state.m_flStuckRecoveryEnd = gpGlobals->curtime + 0.45f;
			state.m_flNextPathBuild = 0.0f;
			state.m_flNextWanderGoal = 0.0f;
			FoFClearBotPath( state );
			if ( state.m_nStuckChecks >= 2 )
			{
				Vector escapeGoal;
				FoFClearBotObjective( state, false );
				FoFClearBotActions( state );
				if ( FoFBuildBotEscapePath( pBot, state, escapeGoal ) )
				{
					FoFAppendBotAction( state, FOF_BOT_ACTION_UNSTUCK,
						escapeGoal, NULL, gpGlobals->curtime + 3.0f );
				}
				state.m_nStuckChecks = 0;
			}
		}
		else
		{
			state.m_nStuckChecks = 0;
		}

		state.m_vecLastPosition = pBot->GetAbsOrigin();
		state.m_flNextStuckCheck = gpGlobals->curtime + 0.6f;
	}

	if ( bTryingToMove &&
		gpGlobals->curtime < state.m_flStuckRecoveryEnd )
	{
		cmd.forwardmove = -180.0f;
		cmd.sidemove = state.m_flStuckRecoverySide;
	}
}

static bool FoFPlanBotCourseOrder(
	CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 6 )
		return false;

	int nOrder = -1;
	Vector orderOrigin;
	float flOrderRadius = 0.0f;
	if ( !FoFGetBotCourseOrder(
		pBot, nOrder, orderOrigin, flOrderRadius ) )
	{
		state.m_nCourseOrder = -1;
		state.m_flCourseOrderRadius = 0.0f;
		return false;
	}

	state.m_nCourseOrder = nOrder;
	state.m_flCourseOrderRadius = flOrderRadius;
	if ( nOrder == 0 )
	{
		FoFAppendBotAction( state, FOF_BOT_ACTION_COURSE_HOLD,
			orderOrigin, NULL, 0.0f );
		return true;
	}

	if ( nOrder != 1 &&
		( nOrder != 2 || state.m_nPrimaryAttackStyle != 5 ) )
	{
		return false;
	}

	const float flDistance = pBot->GetAbsOrigin().DistTo( orderOrigin );
	if ( flDistance <= 100.0f )
	{
		FoFAppendBotAction( state, FOF_BOT_ACTION_COURSE_HOLD,
			orderOrigin, NULL, gpGlobals->curtime + 1.0f );
		return true;
	}

	if ( FoFBuildBotPath( pBot, state, orderOrigin, false, true ) )
	{
		FoFAppendBotAction( state, FOF_BOT_ACTION_CHASE_PATH,
			orderOrigin, NULL, gpGlobals->curtime + 8.0f );
	}
	else
	{
		FoFAppendBotAction( state, FOF_BOT_ACTION_UNSTUCK,
			orderOrigin, NULL, gpGlobals->curtime + 3.0f );
	}
	return true;
}

static bool FoFPlanBotSupportAction( CFoFBot *pBot,
	FoFBotRuntimeState_t &state, CFoF_Player *pEnemy,
	bool bInsertAtFront = false )
{
	if ( pBot->IsFoFBotGhost() || !FoFFindBotWhiskey( pBot ) ||
		gpGlobals->curtime < state.m_flNextSupportSearch )
	{
		return false;
	}
	NOTE_UNUSED( pEnemy );

	state.m_flNextSupportSearch = gpGlobals->curtime + 0.75f;
	CFoF_Player *pTeammate = FoFFindBotSupportTarget( pBot, 250.0f );
	if ( !pTeammate )
		return false;

	state.m_hSupportTarget = pTeammate;
	if ( bInsertAtFront )
	{
		FoFInsertBotAction( state, FOF_BOT_ACTION_SUPPORT_TEAMMATE,
			pTeammate->GetAbsOrigin(), pTeammate,
			gpGlobals->curtime + 4.0f );
	}
	else
	{
		FoFAppendBotAction( state, FOF_BOT_ACTION_SUPPORT_TEAMMATE,
			pTeammate->GetAbsOrigin(), pTeammate,
			gpGlobals->curtime + 4.0f );
	}
	return true;
}

static void FoFPlanBotActions(
	CFoFBot *pBot, FoFBotRuntimeState_t &state, CFoF_Player *pTarget )
{
	if ( FoFPlanBotHazardEscape( pBot, state ) )
		return;

	if ( state.m_bCourseCompanion )
	{
		CCourseMode *pCourse = FoFFindCourseMode();
		Vector captureOrigin;
		float flCaptureRadius = 0.0f;
		if ( pCourse && pCourse->GetActiveCaptureZone(
			captureOrigin, flCaptureRadius ) )
		{
			state.m_vecCourseCompanionHome = captureOrigin;
			// The capture objective limits where the companion moves, not
			// which enemies it can engage from inside the zone.
			const float flSafeRadius = MAX( 64.0f, flCaptureRadius - 48.0f );
			if ( pBot->GetAbsOrigin().DistToSqr(
				captureOrigin ) > Square( flSafeRadius ) )
			{
				FoFClearBotActions( state );
				FoFClearBotObjective( state, false );
				FoFClearBotPath( state );
				if ( FoFBuildBotPath(
					pBot, state, captureOrigin, false, false ) )
				{
					FoFAppendBotAction( state, FOF_BOT_ACTION_CHASE_PATH,
						captureOrigin, NULL, gpGlobals->curtime + 10.0f );
					return;
				}
			}
		}
	}

	if ( !pTarget && state.m_bCourseCompanion )
	{
		CCourseMode *pCourse = FoFFindCourseMode();
		CFoF_Player *pFollowPlayer = ToFoFPlayer(
			FoFFindCourseBotFollowTarget( pBot ) );
		FoFBotAction_t *pCurrent = FoFCurrentBotAction( state );
		if ( pCourse && pFollowPlayer &&
			pCourse->ShouldCourseCompanionsFollowPlayer() &&
			pFollowPlayer->GetAbsOrigin().DistToSqr(
				state.m_vecCourseCompanionHome ) > Square( 128.0f ) &&
			pCurrent &&
			( pCurrent->m_nType == FOF_BOT_ACTION_ROAM_PATH ||
			  pCurrent->m_nType == FOF_BOT_ACTION_ROAM_LOOK ||
			  pCurrent->m_nType == FOF_BOT_ACTION_ROAM_LONG ) )
		{
			FoFClearBotActions( state );
			FoFClearBotObjective( state, false );
			FoFClearBotPath( state );
			state.m_flNextObjectiveSearch = 0.0f;
		}
	}

	if ( state.m_nActionCount > 0 )
	{
		FoFBotAction_t *pCurrent = FoFCurrentBotAction( state );
		if ( pCurrent &&
			pCurrent->m_nType != FOF_BOT_ACTION_SUPPORT_TEAMMATE )
		{
			FoFPlanBotSupportAction( pBot, state, pTarget, true );
		}
		return;
	}
	if ( FoFPlanBotCourseOrder( pBot, state ) )
		return;

	if ( FoFPlanBotSupportAction( pBot, state, pTarget ) )
		return;

	if ( pTarget )
	{
		int nAttackAction = FOF_BOT_ACTION_ATTACK;
		if ( !FoFBotCanSeeTarget( pBot, pTarget ) )
			nAttackAction = FOF_BOT_ACTION_CHASE_PATH;
		FoFAppendBotAction( state, nAttackAction,
			pTarget->GetAbsOrigin(), pTarget,
			gpGlobals->curtime + random->RandomFloat( 3.0f, 6.0f ) );
		return;
	}

	if ( !pBot->IsFoFBotGhost() &&
		FoFTrySelectBotObjective( pBot, state ) )
	{
		int nObjectiveAction = FOF_BOT_ACTION_FOLLOW_PATH;
		if ( state.m_nObjective == FOF_BOT_OBJECTIVE_CRATE )
			nObjectiveAction = FOF_BOT_ACTION_OPEN_CRATE;
		else
		{
			static ConVarRef currentMode( "fof_sv_currentmode", true );
			if ( currentMode.IsValid() && currentMode.GetInt() == 6 )
				nObjectiveAction = FOF_BOT_ACTION_FOLLOW_PATH;
		}
		FoFAppendBotAction( state, nObjectiveAction,
			state.m_vecPathGoal,
			state.m_nObjective == FOF_BOT_OBJECTIVE_CRATE ?
				static_cast< CBaseEntity * >( state.m_hObjectiveCrate.Get() ) :
			state.m_nObjective == FOF_BOT_OBJECTIVE_HORSE ?
				static_cast< CBaseEntity * >( state.m_hObjectiveHorse.Get() ) :
				state.m_hObjectiveEntity.Get(),
			gpGlobals->curtime + 15.0f );
		return;
	}

	if ( FoFChooseBotWanderGoal( pBot, state ) )
	{
		const float flRoamEnd = gpGlobals->curtime +
			random->RandomFloat( 10.0f, 20.0f );
		state.m_flNextWanderGoal = flRoamEnd;
		FoFAppendBotAction( state, FOF_BOT_ACTION_ROAM_PATH,
			state.m_vecWanderGoal, NULL, flRoamEnd );
		return;
	}

	FoFAppendBotAction( state, FOF_BOT_ACTION_ROAM_LOOK,
		vec3_origin, NULL,
		gpGlobals->curtime + random->RandomFloat( 5.0f, 8.0f ) );
}

static bool FoFExecuteBotAction(
	CFoFBot *pBot, FoFBotRuntimeState_t &state,
	CFoF_Player *pTarget, CUserCmd &cmd )
{
	FoFBotAction_t *pAction = FoFCurrentBotAction( state );
	if ( !pAction )
		return false;
	const bool bStartingAction = !pAction->m_bStarted;
	pAction->m_bStarted = true;

	switch ( pAction->m_nType )
	{
	case FOF_BOT_ACTION_ATTACK:
	case FOF_BOT_ACTION_ADVANCE:
		if ( !pTarget )
		{
			FoFPopBotAction( state );
			return false;
		}
		FoFBuildBotCombatCommand( pBot, state, pTarget, cmd );
		if ( pAction->m_nType == FOF_BOT_ACTION_ATTACK &&
			pBot->GetTeamNumber() == FOF_TEAM_ZOMBIES &&
			gpGlobals->curtime > state.m_flNextCombatVoice )
		{
			int nPlayers = 0;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				if ( ToFoFPlayer( UTIL_PlayerByIndex( i ) ) )
					++nPlayers;
			}
			state.m_flNextCombatVoice = gpGlobals->curtime +
				random->RandomFloat( 5.0f, 15.0f ) * nPlayers;
			CPASAttenuationFilter voiceRecipients( pBot->EyePosition(), 0.8f );
			CBaseEntity::EmitSound( voiceRecipients, pBot->entindex(), "ZombieFoF.Attack" );
		}
		if ( pAction->m_flEndTime > 0.0f &&
			gpGlobals->curtime >= pAction->m_flEndTime )
		{
			FoFPopBotAction( state );
		}
		return true;

	case FOF_BOT_ACTION_SUPPORT_TEAMMATE:
	{
		CFoF_Player *pTeammate = ToFoFPlayer(
			pAction->m_hEntity.Get() );
		if ( !FoFBuildBotSupportCommand(
			pBot, state, pTeammate, cmd ) ||
			( pAction->m_flEndTime > 0.0f &&
			  gpGlobals->curtime >= pAction->m_flEndTime ) )
		{
			state.m_hSupportTarget = NULL;
			state.m_flNextWeaponSelection = 0.0f;
			FoFPopBotAction( state );
		}
		return true;
	}

	case FOF_BOT_ACTION_CHASE_PATH:
	{
		CFoF_Player *pActionPlayer = ToFoFPlayer(
			pAction->m_hEntity.Get() );
		if ( pActionPlayer &&
			FoFIsValidBotTarget( pBot, pActionPlayer ) )
		{
			FoFBuildBotCombatCommand(
				pBot, state, pActionPlayer, cmd );
		}
		else if ( pActionPlayer &&
			pActionPlayer->GetTeamNumber() == pBot->GetTeamNumber() )
		{
			if ( !FoFBuildBotSupportCommand(
				pBot, state, pActionPlayer, cmd ) )
			{
				FoFPopBotAction( state );
				return false;
			}
		}
		else
		{
			const float flDistance = pBot->GetAbsOrigin().DistTo(
				pAction->m_vecGoal );
			if ( flDistance <= 45.0f )
			{
				FoFPopBotAction( state );
				return false;
			}

			cmd.viewangles = pBot->EyeAngles();
			float flMoveYaw = cmd.viewangles.y;
			if ( FoFBuildBotNavigationCommand( pBot, state,
				pAction->m_vecGoal, true,
				FoFBotMoveSpeed( pBot, state ),
				cmd, &flMoveYaw ) )
			{
				cmd.viewangles.y = UTIL_ApproachAngle(
					flMoveYaw, cmd.viewangles.y,
					180.0f * gpGlobals->frametime );
			}
			if ( pTarget && FoFBotCanSeeTarget( pBot, pTarget ) )
				FoFBuildCachedBotAttackCommand(
					pBot, state, pTarget, cmd );
		}

		if ( pAction->m_flEndTime > 0.0f &&
			gpGlobals->curtime >= pAction->m_flEndTime )
		{
			FoFPopBotAction( state );
		}
		return true;
	}

	case FOF_BOT_ACTION_HIDE:
	case FOF_BOT_ACTION_HIDE_PATH:
	{
		if ( ( pBot->GetAbsOrigin() - pAction->m_vecGoal ).Length2D() <= 45.0f ||
			( pAction->m_flEndTime > 0.0f &&
			  gpGlobals->curtime >= pAction->m_flEndTime ) )
		{
			FoFPopBotAction( state );
			return false;
		}

		cmd.viewangles = pBot->EyeAngles();
		if ( pTarget )
			FoFAimBotToward( pBot, pTarget->EyePosition(), cmd );
		float flMoveYaw = cmd.viewangles.y;
		if ( !FoFBuildBotNavigationCommand( pBot, state,
			pAction->m_vecGoal, true,
			FoFBotMoveSpeed( pBot, state ), cmd, &flMoveYaw ) )
		{
			FoFPopBotAction( state );
			return false;
		}
		if ( !pTarget )
		{
			cmd.viewangles.y = UTIL_ApproachAngle(
				flMoveYaw, cmd.viewangles.y,
				160.0f * gpGlobals->frametime );
		}
		if ( pTarget )
			FoFBuildCachedBotAttackCommand( pBot, state, pTarget, cmd );
		return true;
	}

	case FOF_BOT_ACTION_CROUCH_WAIT:
		cmd.viewangles = pBot->EyeAngles();
		cmd.forwardmove = 0.0f;
		cmd.sidemove = 0.0f;
		cmd.buttons |= IN_DUCK;
		if ( pTarget )
		{
			FoFAimBotToward( pBot, pTarget->EyePosition(), cmd );
			FoFBuildCachedBotAttackCommand( pBot, state, pTarget, cmd );
		}
		if ( !pTarget || ( pAction->m_flEndTime > 0.0f &&
			gpGlobals->curtime >= pAction->m_flEndTime ) )
		{
			FoFPopBotAction( state );
		}
		return true;

	case FOF_BOT_ACTION_OPEN_CRATE:
	case FOF_BOT_ACTION_FOLLOW_PATH:
		if ( !FoFBuildBotObjectiveCommand( pBot, state, cmd ) )
		{
			FoFPopBotAction( state );
			return false;
		}
		return true;

	case FOF_BOT_ACTION_COURSE_HOLD:
	{
		int nOrder = -1;
		Vector orderOrigin;
		float flOrderRadius = 0.0f;
		if ( !FoFGetBotCourseOrder(
			pBot, nOrder, orderOrigin, flOrderRadius ) ||
			nOrder != state.m_nCourseOrder )
		{
			state.m_nCourseOrder = -1;
			state.m_flCourseOrderRadius = 0.0f;
			FoFPopBotAction( state );
			return false;
		}

		cmd.viewangles = pBot->EyeAngles();
		FoFAimBotToward( pBot, orderOrigin, cmd );
		cmd.forwardmove = 0.0f;
		cmd.sidemove = 0.0f;
		if ( pAction->m_flEndTime > 0.0f &&
			gpGlobals->curtime >= pAction->m_flEndTime )
		{
			FoFPopBotAction( state );
		}
		return true;
	}

	case FOF_BOT_ACTION_ROAM_PATH:
	case FOF_BOT_ACTION_ROAM_LONG:
		FoFBuildBotWanderCommand( pBot, state, cmd );
		if ( ( pBot->GetAbsOrigin() - pAction->m_vecGoal ).Length2D() <= 35.0f ||
			( pAction->m_flEndTime > 0.0f &&
			  gpGlobals->curtime >= pAction->m_flEndTime ) )
		{
			FoFPopBotAction( state );
		}
		return true;

	case FOF_BOT_ACTION_ROAM_LOOK:
	{
		if ( bStartingAction && pBot->IsFoFBotGhost() )
			pBot->EmitSound( "Ghost.Alert" );
		cmd.viewangles = pBot->EyeAngles();
		const Vector vecLookDirection =
			pAction->m_vecGoal - pBot->EyePosition();
		const float flYawError = AngleDiff(
			UTIL_VecToYaw( vecLookDirection ), cmd.viewangles.y );
		const float flTurn = MIN( fabsf( flYawError ),
			MAX( FoFBotTurnValue( pBot, state ), 0.0f ) ) *
			gpGlobals->frametime * 35.0f;
		cmd.viewangles.y = AngleNormalize(
			cmd.viewangles.y + ( flYawError < 0.0f ? -flTurn : flTurn ) );
		if ( gpGlobals->curtime >= pAction->m_flEndTime )
			FoFPopBotAction( state );
		return true;
	}

	case FOF_BOT_ACTION_UNSTUCK:
		if ( ( pBot->GetAbsOrigin() - pAction->m_vecGoal ).Length2D() <= 45.0f ||
			gpGlobals->curtime >= pAction->m_flEndTime )
		{
			FoFPopBotAction( state );
			return false;
		}
		cmd.viewangles = pBot->EyeAngles();
		{
			float flMoveYaw = cmd.viewangles.y;
			if ( FoFBuildBotNavigationCommand( pBot, state,
				pAction->m_vecGoal, true,
				FoFBotMoveSpeed( pBot, state ), cmd, &flMoveYaw ) )
			{
				cmd.viewangles.y = UTIL_ApproachAngle(
					flMoveYaw, cmd.viewangles.y,
					220.0f * gpGlobals->frametime );
			}
			else
			{
				cmd.forwardmove = -180.0f;
				cmd.sidemove = state.m_flStuckRecoverySide;
			}
		}
		return true;
	}

	FoFPopBotAction( state );
	return false;
}

static bool FoFBuildMimicCommand( CUserCmd &cmd )
{
	static ConVarRef mimic( "fof_bot_mimic", true );
	static ConVarRef yawOffset( "fof_bot_mimic_yaw_offset", true );
	if ( !mimic.IsValid() )
		return false;
	const int nPlayer = mimic.GetInt();
	if ( nPlayer <= 0 || nPlayer > gpGlobals->maxClients )
		return false;

	CBasePlayer *pSource = UTIL_PlayerByIndex( nPlayer );
	if ( !pSource || !pSource->GetLastUserCommand() )
		return false;
	cmd = *pSource->GetLastUserCommand();
	if ( yawOffset.IsValid() )
		cmd.viewangles.y += yawOffset.GetFloat();
	return true;
}

static void FoFRunBotCommand(
	CFoFBot *pBot, CUserCmd &cmd, float flInputFrameTime )
{
	cmd.viewangles[PITCH] = clamp(
		AngleNormalize( cmd.viewangles[PITCH] ), -89.0f, 89.0f );
	cmd.viewangles[YAW] = AngleNormalize( cmd.viewangles[YAW] );
	cmd.viewangles[ROLL] = 0.0f;

	const float flOldFrameTime = gpGlobals->frametime;
	const float flOldCurTime = gpGlobals->curtime;
	pBot->SetTimeBase(
		gpGlobals->curtime + gpGlobals->frametime - flInputFrameTime );
	MoveHelperServer()->SetHost( pBot );
	pBot->PlayerRunCommand( &cmd, MoveHelperServer() );
	pBot->SetLastUserCommand( cmd );
	pBot->pl.fixangle = FIXANGLE_NONE;
	MoveHelperServer()->SetHost( NULL );
	gpGlobals->frametime = flOldFrameTime;
	gpGlobals->curtime = flOldCurTime;
}

static void FoFRunBot( CFoFBot *pBot, FoFBotRuntimeState_t &state )
{
	pBot->AddFlag( FL_FAKECLIENT );
	FoFUpdateCourseCompanionGlow( pBot, state );
	CUserCmd cmd;
	Q_memset( &cmd, 0, sizeof( cmd ) );
	cmd.viewangles = pBot->EyeAngles();
	cmd.command_number = ++state.m_nCommandNumber;
	cmd.tick_count = gpGlobals->tickcount;
	cmd.random_seed = random->RandomInt( 0, 0x7fffffff );

	static ConVarRef botStop( "fof_bot_stop", true );
	const bool bStopped = ( botStop.IsValid() && botStop.GetBool() ) ||
		pBot->IsEFlagSet( EFL_BOT_FROZEN ) ||
		( pBot->GetFlags() & FL_FROZEN ) != 0;
	if ( bStopped )
		state.m_bHasCachedCommand = false;
	if ( !bStopped && pBot->IsAlive() && !pBot->IsObserver() )
	{
		FoFUpdateBotForcedWeapon( pBot, state );
		FoFBotAction_t *pCurrentAction = FoFCurrentBotAction( state );
		if ( !pCurrentAction ||
			pCurrentAction->m_nType != FOF_BOT_ACTION_SUPPORT_TEAMMATE )
		{
			FoFUpdateBotWeaponSelection( pBot, state );
		}
		if ( gpGlobals->curtime >= state.m_flNextTargetSearch ||
			!FoFIsValidBotTarget( pBot, state.m_hTarget.Get() ) )
		{
			CFoF_Player *pOldTarget = state.m_hTarget.Get();
			state.m_hTarget = FoFFindBotTarget( pBot );
			if ( state.m_hTarget.Get() != pOldTarget )
			{
				state.m_flAimOnTargetTime = 0.0f;
				state.m_flViewPitchVelocity = 0.0f;
				state.m_flViewYawVelocity = 0.0f;
				if ( state.m_hTarget.Get() )
					FoFClearBotObjective( state, false );
				FoFClearBotActions( state );
				FoFClearBotPath( state );
				state.m_flNextPathBuild = 0.0f;
			}
			const float flTargetSearchInterval =
				state.m_nPrimaryAttackStyle == 1 ? 0.5f :
				state.m_nPrimaryAttackStyle == 5 ? 3.5f : 2.15f;
			state.m_flNextTargetSearch =
				gpGlobals->curtime + flTargetSearchInterval;
		}

		CFoF_Player *pTarget = state.m_hTarget.Get();
		if ( pTarget && state.m_nPrimaryAttackStyle != 7 )
		{
			// Target readiness is per-command, even when this listen-server
			// tick reuses the previous movement command.
			const float flDistance = pTarget->GetAbsOrigin().DistTo(
				pBot->GetAbsOrigin() );
			FoFUpdateBotAimReadiness( pBot, state, pTarget->EyePosition(),
				flDistance, FoFBotCanSeeTarget( pBot, pTarget ), cmd );
		}
		const bool bHasObjective =
			state.m_nObjective != FOF_BOT_OBJECTIVE_NONE;
		const bool bRefreshMovement =
			engine->IsDedicatedServer() ||
			FoFIsConfiguredCourseBot( state ) || bHasObjective ||
			!state.m_bHasCachedCommand ||
			( ( gpGlobals->tickcount + pBot->entindex() ) & 1 ) == 0;
		if ( bRefreshMovement )
		{
			FoFPlanBotActions( pBot, state, pTarget );
			bool bHandledAction =
				FoFExecuteBotAction( pBot, state, pTarget, cmd );
			if ( !bHandledAction )
			{
				FoFPlanBotActions( pBot, state, pTarget );
				bHandledAction =
					FoFExecuteBotAction( pBot, state, pTarget, cmd );
			}
			if ( !bHandledAction )
			{
				FoFBuildBotWanderCommand( pBot, state, cmd );
			}
			state.m_bRoaming = !pTarget &&
				state.m_nObjective == FOF_BOT_OBJECTIVE_NONE;

			if ( state.m_nCourseOrder == 0 )
			{
				cmd.forwardmove = 0.0f;
				cmd.sidemove = 0.0f;
				cmd.upmove = 0.0f;
				cmd.buttons = 0;
			}
			else
			{
				if ( !FoFHandleBotNearbyUse( pBot, state, cmd ) )
					FoFHandleBotPathObstacle( pBot, state, cmd );
				FoFApplyBotStuckRecovery( pBot, state, cmd );
			}
			FoFSynchronizeBotMovementButtons( cmd );
			state.m_CachedCommand = cmd;
			state.m_CachedCommand.buttons &=
				~( IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_SPEED );
			state.m_bHasCachedCommand = true;
		}
		else
		{
			const int nCommandNumber = cmd.command_number;
			const int nTickCount = cmd.tick_count;
			const int nRandomSeed = cmd.random_seed;
			const int nServerRandomSeed = cmd.server_random_seed;
			cmd = state.m_CachedCommand;
			cmd.command_number = nCommandNumber;
			cmd.tick_count = nTickCount;
			cmd.random_seed = nRandomSeed;
			cmd.server_random_seed = nServerRandomSeed;
			if ( pTarget && state.m_nCourseOrder != 0 )
				FoFBuildCachedBotAttackCommand( pBot, state, pTarget, cmd );
		}

		// Aiming is a bot-main input, independent of the movement action.
		// Retaining it only in the attack action releases secondary attack on
		// alternating movement frames and prevents rifles/bows from drawing.
		if ( pTarget && state.m_nCourseOrder != 0 &&
			state.m_nPrimaryAttackStyle == 5 &&
			state.m_flAimOnTargetTime > 0.0f &&
			FoFBotCanSeeTarget( pBot, pTarget ) )
		{
			cmd.buttons |= IN_ATTACK2;
		}
	}
	else if ( !pBot->IsAlive() &&
		FoFIsConfiguredCourseBot( state ) )
	{
		if ( state.m_flCourseRemovalAt <= 0.0f )
			state.m_flCourseRemovalAt = gpGlobals->curtime + 0.1f;
		else if ( !state.m_bCourseRemovalQueued &&
			gpGlobals->curtime >= state.m_flCourseRemovalAt )
		{
			// Keep the corpse after the fake-client slot is released, then use
			// the engine's normal delayed fade/removal path.
			CBaseEntity *pRagdoll = pBot->m_hRagdoll.Get();
			if ( pRagdoll )
				pRagdoll->SUB_StartFadeOut();
			pBot->m_hRagdoll = NULL;
			engine->ServerCommand( UTIL_VarArgs(
				"kickid %d\n", pBot->GetUserID() ) );
			state.m_bCourseRemovalQueued = true;
		}
	}
	else if ( !pBot->IsAlive() &&
		pBot->m_lifeState == LIFE_RESPAWNABLE &&
		!FoFIsConfiguredCourseBot( state ) )
	{
		cmd.buttons |= IN_ATTACK;
	}

	// The shipped client copies mimic input after the stop/natural-AI branch,
	// so mimic remains usable while fof_bot_stop is set.
	FoFBuildMimicCommand( cmd );
	if ( pBot->GetTeamNumber() == FOF_TEAM_ZOMBIES )
		cmd.buttons &= ~IN_SPEED;
	FoFConstrainBotHazardMovement( pBot, state, cmd );
	FoFRunBotCommand( pBot, cmd, gpGlobals->frametime );
}

void FoFRunBots()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoFBot *pBot = dynamic_cast< CFoFBot * >( UTIL_PlayerByIndex( i ) );
		if ( !pBot || !pBot->IsConnected() || !pBot->IsBot() )
		{
			continue;
		}

		FoFBotRuntimeState_t *pState = FoFGetBotRuntimeState( pBot );
		if ( !pState )
		{
			FoFConfigureBotRuntime( pBot, NULL, false );
			pState = FoFGetBotRuntimeState( pBot );
		}
		if ( !pState )
			continue;

		FoFUpdateBotEquipment( pBot );
		FoFRunBot( pBot, *pState );
	}
}

static int FoFCountPopulationBots( int nTeam = -1 )
{
	int nBots = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsBot() ||
			pPlayer->IsFoFBotGhost() )
		{
			continue;
		}

		if ( nTeam == -1 || pPlayer->GetTeamNumber() == nTeam )
			++nBots;
	}
	return nBots;
}

static int FoFCountConnectedFoFPlayers()
{
	int nPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() )
			++nPlayers;
	}
	return nPlayers;
}

static int FoFCountHumanPlayers( bool bIncludeSpectators )
{
	int nHumans = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsBot() ||
			( !bIncludeSpectators &&
			  pPlayer->GetTeamNumber() == TEAM_SPECTATOR ) )
		{
			continue;
		}
		++nHumans;
	}
	return nHumans;
}

static int FoFDesiredBotCount( int nActiveHumans )
{
	static ConVarRef slotPercentage( "fof_sv_bot_slotpct", true );
	static ConVarRef dynamicJoin( "fof_sv_bot_dynamicjoin", true );
	if ( !slotPercentage.IsValid() )
		return 0;

	const float flPercentage = slotPercentage.GetFloat();
	const int nBaseCount = static_cast< int >(
		static_cast< float >( gpGlobals->maxClients ) * flPercentage );
	int nDesired = nBaseCount;
	if ( ( dynamicJoin.IsValid() && dynamicJoin.GetBool() ) ||
		engine->IsDedicatedServer() )
	{
		nDesired = static_cast< int >( RemapValClamped(
			static_cast< float >( nActiveHumans ), 1.0f, 8.0f,
			static_cast< float >( nBaseCount ), 0.0f ) );

		if ( HL2MPRules() && HL2MPRules()->IsTeamplay() &&
			nActiveHumans == 9 )
		{
			nDesired = 1;
		}
	}

	while ( nDesired + nActiveHumans > gpGlobals->maxClients )
		--nDesired;
	return nDesired;
}

static int FoFPopulationBotRemovalTeam()
{
	if ( !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
		return TEAM_UNASSIGNED;

	static ConVarRef maxTeams( "fof_sv_maxteams", true );
	const int nTeamEnd =
		( maxTeams.IsValid() ? maxTeams.GetInt() : 4 ) + 2;
	int nRemovalTeam = FIRST_GAME_TEAM;
	int nMostBots = 0;
	for ( int nTeam = FIRST_GAME_TEAM;
		nTeam < nTeamEnd; ++nTeam )
	{
		const int nBots = FoFCountPopulationBots( nTeam );
		if ( nBots > nMostBots )
		{
			nMostBots = nBots;
			nRemovalTeam = nTeam;
		}
	}
	return nRemovalTeam;
}

static void FoFRemovePopulationBot( int nTeam )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->IsBot() ||
			pPlayer->IsFoFBotGhost() || pPlayer->GetTeamNumber() != nTeam )
		{
			continue;
		}

		engine->ServerCommand( UTIL_VarArgs(
			"kickid %d\n", pPlayer->GetUserID() ) );
		return;
	}
}

void FoFUpdateBotPopulation()
{
	if ( gpGlobals->curtime < s_flNextFoFBotPopulationUpdate )
		return;
	s_flNextFoFBotPopulationUpdate = gpGlobals->curtime + 0.25f;

	if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef botEdit( "fof_sv_bot_edit", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode == 6 ||
		( engine->IsDedicatedServer() && nMode == 5 ) ||
		( botEdit.IsValid() && botEdit.GetBool() ) )
	{
		return;
	}

	const int nBots = FoFCountPopulationBots();
	static ConVarRef slotPercentage( "fof_sv_bot_slotpct", true );
	if ( nBots == 0 &&
		( !slotPercentage.IsValid() || slotPercentage.GetFloat() == 0.0f ) )
	{
		return;
	}

	int nActiveHumans = FoFCountHumanPlayers( false );
	if ( FoFCountConnectedFoFPlayers() > gpGlobals->maxClients - 2 )
		nActiveHumans = FoFCountHumanPlayers( true );

	if ( nActiveHumans < 1 )
	{
		if ( engine->IsDedicatedServer() && nBots > 0 )
			FoFRemovePopulationBot( FoFPopulationBotRemovalTeam() );
		return;
	}

	const int nDesired = FoFDesiredBotCount( nActiveHumans );
	if ( nBots < nDesired )
	{
		FoFPutBotInServer( false, 0, NULL, true );
	}
	else if ( nBots > nDesired )
	{
		FoFRemovePopulationBot( FoFPopulationBotRemovalTeam() );
	}
}
