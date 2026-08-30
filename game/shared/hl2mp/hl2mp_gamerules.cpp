//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl2mp_gamerules.h"
#include "viewport_panel_names.h"
#include "gameeventdefs.h"
#include "fof/fof_weapon_ballistics.h"
#include <KeyValues.h>
#include "ammodef.h"

#ifndef CLIENT_DLL

	#include "eventqueue.h"
	#include "player.h"
	#include "gamerules.h"
	#include "game.h"
	#include "items.h"
	#include "entitylist.h"
	#include "mapentities.h"
	#include "in_buttons.h"
	#include <ctype.h>
	#include "voice_gamemgr.h"
	#include "iscorer.h"
#include "hl2mp_player.h"
#if defined( GAME_DLL )
#include "fof/fof_course_mode.h"
#include "fof/fof_gamerules.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_modes.h"
#include "fof/fof_player.h"
#endif
	#include "weapon_hl2mpbasehlmpcombatweapon.h"
	#include "team.h"
	#include "hl2mp_gameinterface.h"
	#include "hl2mp_cvars.h"

#if defined( DEBUG )
	#include "hl2mp_bot_temp.h"
#endif

extern void respawn(CBaseEntity *pEdict, bool fCopyCorpse);

extern bool FindInList( const char **pStrings, const char *pToFind );

ConVar sv_hl2mp_weapon_respawn_time( "sv_hl2mp_weapon_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY );
ConVar sv_hl2mp_item_respawn_time( "sv_hl2mp_item_respawn_time", "30", FCVAR_GAMEDLL | FCVAR_NOTIFY );
ConVar sv_report_client_settings("sv_report_client_settings", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY );

extern ConVar mp_chattime;

extern CBaseEntity	 *g_pLastCombineSpawn;
extern CBaseEntity	 *g_pLastRebelSpawn;

#define WEAPON_MAX_DISTANCE_FROM_SPAWN 64

#endif


REGISTER_GAMERULES_CLASS( CHL2MPRules );

BEGIN_NETWORK_TABLE_NOBASE( CHL2MPRules, DT_HL2MPRules )

	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bTeamPlayEnabled ) ),
		RecvPropInt( RECVINFO( m_nMapSize ) ),
		RecvPropInt( RECVINFO( m_nTPClassesTotal ) ),
		RecvPropFloat( RECVINFO( flTimeLimit ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nTPClassesUsage ), RecvPropInt( RECVINFO( m_nTPClassesUsage[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nTPClassesSlots ), RecvPropInt( RECVINFO( m_nTPClassesSlots[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nTPClassesUsageDesp ), RecvPropInt( RECVINFO( m_nTPClassesUsageDesp[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nTPClassesSlotsDesp ), RecvPropInt( RECVINFO( m_nTPClassesSlotsDesp[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( vSafeZone1 ), RecvPropVector( RECVINFO( vSafeZone1[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( vSafeZone2 ), RecvPropVector( RECVINFO( vSafeZone2[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( vSafeZone3 ), RecvPropVector( RECVINFO( vSafeZone3[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( vSafeZone4 ), RecvPropVector( RECVINFO( vSafeZone4[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_bSafeZoneState ), RecvPropBool( RECVINFO( m_bSafeZoneState[0] ) ) ),
	#else
		SendPropBool( SENDINFO( m_bTeamPlayEnabled ) ),
		#if defined( GAME_DLL )
			SendPropInt( SENDINFO( m_nMapSize ) ),
			SendPropInt( SENDINFO( m_nTPClassesTotal ) ),
			SendPropFloat( SENDINFO( flTimeLimit ), 0, SPROP_NOSCALE ),
			SendPropArray3( SENDINFO_ARRAY3( m_nTPClassesUsage ), SendPropInt( SENDINFO_ARRAY( m_nTPClassesUsage ), 3, SPROP_UNSIGNED ) ),
			SendPropArray3( SENDINFO_ARRAY3( m_nTPClassesSlots ), SendPropInt( SENDINFO_ARRAY( m_nTPClassesSlots ), 3, SPROP_UNSIGNED ) ),
			SendPropArray3( SENDINFO_ARRAY3( m_nTPClassesUsageDesp ), SendPropInt( SENDINFO_ARRAY( m_nTPClassesUsageDesp ), 3, SPROP_UNSIGNED ) ),
			SendPropArray3( SENDINFO_ARRAY3( m_nTPClassesSlotsDesp ), SendPropInt( SENDINFO_ARRAY( m_nTPClassesSlotsDesp ), 3, SPROP_UNSIGNED ) ),
			SendPropArray3( SENDINFO_ARRAY3( vSafeZone1 ), SendPropVector( SENDINFO_ARRAY( vSafeZone1 ), -1, SPROP_COORD ) ),
			SendPropArray3( SENDINFO_ARRAY3( vSafeZone2 ), SendPropVector( SENDINFO_ARRAY( vSafeZone2 ), -1, SPROP_COORD ) ),
			SendPropArray3( SENDINFO_ARRAY3( vSafeZone3 ), SendPropVector( SENDINFO_ARRAY( vSafeZone3 ), -1, SPROP_COORD ) ),
			SendPropArray3( SENDINFO_ARRAY3( vSafeZone4 ), SendPropVector( SENDINFO_ARRAY( vSafeZone4 ), -1, SPROP_COORD ) ),
			SendPropArray3( SENDINFO_ARRAY3( m_bSafeZoneState ), SendPropBool( SENDINFO_ARRAY( m_bSafeZoneState ) ) ),
		#endif
	#endif

END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( hl2mp_gamerules, CHL2MPGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( HL2MPGameRulesProxy, DT_HL2MPGameRulesProxy )

static HL2MPViewVectors g_HL2MPViewVectors(
	Vector( 0, 0, 64 ),
	Vector( -16, -16, 0 ),
	Vector( 16, 16, 72 ),
	Vector( -16, -16, 0 ),
	Vector( 16, 16, 44 ),
	Vector( 0, 0, 40 ),
	Vector( -10, -10, -10 ),
	Vector( 10, 10, 10 ),
	Vector( 0, 0, 14 ),
	Vector( -16, -16, 0 ),
	Vector( 16, 16, 44 ) );

const CViewVectors *CHL2MPRules::GetViewVectors() const
{
	return &g_HL2MPViewVectors;
}

const HL2MPViewVectors *CHL2MPRules::GetHL2MPViewVectors() const
{
	return &g_HL2MPViewVectors;
}

CAmmoDef *GetAmmoDef()
{
	static CAmmoDef ammoDef;
	static bool initialized = false;

	if ( !initialized )
	{
		initialized = true;
		// Ammo counts are networked by numeric index; do not reorder these entries.
		ammoDef.AddAmmoType( "357",           DMG_BULLET,                  TRACER_LINE_AND_WHIZ, 40, 40,  120,  327.170441f,  AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER, 4, 8 );
		ammoDef.AddAmmoType( "Rifle",         DMG_BULLET,                  TRACER_LINE_AND_WHIZ, 60, 60,  120,  667.973f,     4, 6, 8 );
		ammoDef.AddAmmoType( "Rifle2",        DMG_BULLET,                  TRACER_LINE_AND_WHIZ, 100, 100, 120, 1226.88916f,   5, 9, 8 );
		ammoDef.AddAmmoType( "XBowBolt",      DMG_BULLET,                  TRACER_NONE,          80, 80,  100,  654.3409f,    0, 4, 8 );
		ammoDef.AddAmmoType( "XBowBolt2",     DMG_BURN,                    TRACER_NONE,          80, 80,  100,  654.3409f,    0, 4, 8 );
		ammoDef.AddAmmoType( "Knife",         DMG_SLASH,                   TRACER_NONE,          25, 25,   50,   81.79261f,   5, 8, 8 );
		ammoDef.AddAmmoType( "Axe",           DMG_SLASH,                   TRACER_NONE,          40, 40,   50,  136.321014f,  8, 12, 8 );
		ammoDef.AddAmmoType( "Machete",       DMG_SLASH,                   TRACER_NONE,          40, 40,   50,  136.321014f,  8, 12, 8 );
		ammoDef.AddAmmoType( "Buckshot",      DMG_BUCKSHOT,                TRACER_LINE,           4,  5,   80,  218.113632f, AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER | FOF_AMMO_FIXED_BULLET_PATTERN, 4, 8 );
		ammoDef.AddAmmoType( "RockSalt",      DMG_BUCKSHOT | DMG_ACID,     TRACER_LINE_AND_WHIZ,  4,  4,  100,  327.170441f, AMMO_FORCE_DROP_IF_CARRIED | AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER | FOF_AMMO_FIXED_BULLET_PATTERN, 6, 8 );
		ammoDef.AddAmmoType( "Grenade",       DMG_BURN,                    TRACER_NONE,           0,  0,   50,    0.0f,       0, 4, 8 );
		ammoDef.AddAmmoType( "Dynamite_B",    DMG_BURN,                    TRACER_NONE,           0,  0,   50,    0.0f,       0, 4, 8 );
		ammoDef.AddAmmoType( "dynamite_weak", DMG_BURN,                    TRACER_NONE,           0,  0,   50,    0.0f,       0, 4, 8 );
		ammoDef.AddAmmoType( "Gatling",       DMG_BULLET,                  TRACER_LINE,          45, 45, 1000, 2910.45386f,  0, 4, 8 );
	}

	return &ammoDef;
}

#ifdef CLIENT_DLL
static ConVar fof_sv_maxrounds(
	"fof_sv_maxrounds", "0", FCVAR_REPLICATED,
	"How many rounds are played until map ends" );
static ConVar fof_sv_roundsplayed(
	"fof_sv_roundsplayed", "0", FCVAR_REPLICATED );
static ConVar fof_timer_show(
	"fof_timer_show", "0", FCVAR_REPLICATED );

void CHL2MPRules::InitializeFoFClientState()
{
	m_nMapSize = 0;
	m_nTPClassesTotal = 0;
	flTimeLimit = 0.0f;
	Q_memset( m_nTPClassesUsage, 0, sizeof( m_nTPClassesUsage ) );
	Q_memset( m_nTPClassesSlots, 0, sizeof( m_nTPClassesSlots ) );
	Q_memset( m_nTPClassesUsageDesp, 0, sizeof( m_nTPClassesUsageDesp ) );
	Q_memset( m_nTPClassesSlotsDesp, 0, sizeof( m_nTPClassesSlotsDesp ) );
	Q_memset( vSafeZone1, 0, sizeof( vSafeZone1 ) );
	Q_memset( vSafeZone2, 0, sizeof( vSafeZone2 ) );
	Q_memset( vSafeZone3, 0, sizeof( vSafeZone3 ) );
	Q_memset( vSafeZone4, 0, sizeof( vSafeZone4 ) );
	Q_memset( m_bSafeZoneState, 0, sizeof( m_bSafeZoneState ) );
	m_flGameStartTime = 0.0f;
}

bool CHL2MPRules::GetFoFSafeZoneQuad(
	int index,
	Vector &corner0,
	Vector &corner1,
	Vector &corner2,
	Vector &corner3 ) const
{
	if ( index < 0 || index >= FOF_SAFE_ZONE_POINT_COUNT ||
		!m_bSafeZoneState[index] )
		return false;

	corner0 = vSafeZone2[index];
	corner1 = vSafeZone1[index];
	corner2 = vSafeZone3[index];
	corner3 = vSafeZone4[index];
	return true;
}

float CHL2MPRules::GetFoFScoreboardTimeRemaining() const
{
	static ConVarRef mapStartTime( "map_start_time", true );
	const int startTime = mapStartTime.IsValid()
		? mapStartTime.GetInt() : -1;
	if ( startTime == -1 || !gpGlobals )
		return -1.0f;

	return MAX(
		0.0f,
		(float)startTime + flTimeLimit * 60.0f - gpGlobals->curtime );
}
int CHL2MPRules::GetFoFTeamClassCount() const
{
	return clamp( m_nTPClassesTotal, 0, FOF_TEAMPLAY_CLASS_COUNT );
}

bool CHL2MPRules::IsFoFTeamClassAvailable(
	int classIndex, int teamNumber ) const
{
	if ( classIndex < 0 || classIndex >= GetFoFTeamClassCount() )
		return false;

	if ( teamNumber == TEAM_COMBINE )
	{
		return m_nTPClassesUsage[classIndex] <
			m_nTPClassesSlots[classIndex];
	}

	return m_nTPClassesUsageDesp[classIndex] <
		m_nTPClassesSlotsDesp[classIndex];
}

void CHL2MPRules::GetFoFNeutralColor(
	int nContext, float &flRed, float &flGreen, float &flBlue )
{
	(void)nContext;
	flRed = 0.76f;
	flGreen = 0.76f;
	flBlue = 0.76f;
}
#elif defined( GAME_DLL )
#endif

float CHL2MPRules::GetFoFNotorietyPayoutProgress() const
{
	if ( flTimeLimit > 0.0f && gpGlobals )
	{
		const float duration = flTimeLimit * 60.0f;
		const float remaining =
			m_flGameStartTime + duration - gpGlobals->curtime;
		return RemapValClamped(
			remaining, 0.0f, duration, 1.0f, 0.01f );
	}

	static ConVarRef maxRounds( "fof_sv_maxrounds", true );
	static ConVarRef roundsPlayed( "fof_sv_roundsplayed", true );
	if ( maxRounds.IsValid() && maxRounds.GetInt() > 0 )
	{
		return RemapValClamped(
			roundsPlayed.IsValid() ? (float)roundsPlayed.GetInt() : 0.0f,
			0.0f, (float)maxRounds.GetInt(), 1.0f, 0.01f );
	}
	return 0.5f;
}

int CHL2MPRules::GetFoFNotorietyPayout(
	int totalNotoriety,
	int playerNotoriety,
	int playerCount ) const
{
	const float progress = GetFoFNotorietyPayoutProgress();
	float baseScale = ( progress - 0.05f ) * 1.05263162f;
	baseScale = MAX( 0.0f, MIN( baseScale, 1.0f ) );
	const int basePayout = (int)( ( 1.0f + baseScale ) * 30.0f );

	playerNotoriety = MAX( 0, MIN( playerNotoriety, 1000 ) );
	float playerShare;
	if ( totalNotoriety == 0 )
	{
		playerShare = playerCount > 0
			? (float)( 1 / playerCount ) : 0.0f;
	}
	else
	{
		const float denominator = MAX(
			1.0f, MIN( (float)totalNotoriety, 1000.0f ) );
		playerShare = (float)playerNotoriety / denominator;
	}

	volatile float orderedBonus = (float)basePayout * 0.1f;
	orderedBonus = orderedBonus * (float)playerCount;
	orderedBonus = orderedBonus * playerShare;
	return basePayout + (int)orderedBonus;
}

static const char *s_PreserveEnts[] =
{
	"ai_network",
	"ai_hint",
	"hl2mp_gamerules",
	"team_manager",
	"player_manager",
	"env_soundscape",
	"env_soundscape_proxy",
	"env_soundscape_triggerable",
	"env_sun",
	"env_wind",
	"env_fog_controller",
	"func_brush",
	"func_wall",
	"func_buyzone",
	"func_illusionary",
	"infodecal",
	"info_projecteddecal",
	"info_node",
	"info_target",
	"info_node_hint",
	"info_player_deathmatch",
	"info_player_combine",
	"info_player_rebel",
	"info_map_parameters",
	"keyframe_rope",
	"move_rope",
	"info_ladder",
	"player",
	"point_viewcontrol",
	"scene_manager",
	"shadow_control",
	"sky_camera",
	"soundent",
	"trigger_soundscape",
	"viewmodel",
	"predicted_viewmodel",
	"worldspawn",
	"point_devshot_camera",
	"", // END Marker
};



#ifdef CLIENT_DLL
	void RecvProxy_HL2MPRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CHL2MPRules *pRules = HL2MPRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy )
		RecvPropDataTable( "hl2mp_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_HL2MPRules ), RecvProxy_HL2MPRules )
	END_RECV_TABLE()
#else
	void* SendProxy_HL2MPRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CHL2MPRules *pRules = HL2MPRules();
		Assert( pRules );
		return pRules;
	}

	BEGIN_SEND_TABLE( CHL2MPGameRulesProxy, DT_HL2MPGameRulesProxy )
		SendPropDataTable( "hl2mp_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_HL2MPRules ), SendProxy_HL2MPRules )
	END_SEND_TABLE()
#endif

#ifndef CLIENT_DLL

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
		{
			return ( pListener->GetTeamNumber() == pTalker->GetTeamNumber() );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

#endif

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
#if !defined( GAME_DLL )
char *sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"Combine",
	"Rebels",
};
#endif

CHL2MPRules::CHL2MPRules()
{
#ifdef CLIENT_DLL
	InitializeFoFClientState();
#endif

#ifndef CLIENT_DLL
	// Create the team managers
#if defined( GAME_DLL )
	InitializeFoFRuleTeams();
#else
	for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
	{
		CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "team_manager" ));
		pTeam->Init( sTeamNames[i], i );

		g_Teams.AddToTail( pTeam );
	}
#endif

	m_bTeamPlayEnabled = teamplay.GetBool();
	m_flIntermissionEndTime = 0.0f;
	m_flGameStartTime = 0;

	m_hRespawnableItemsAndWeapons.RemoveAll();
	m_tmNextPeriodicThink = 0;
	m_flRestartGameTime = 0;
	m_bCompleteReset = false;
	m_bHeardAllPlayersReady = false;
	m_bAwaitingReadyRestart = false;
	m_bChangelevelDone = false;

#if defined( GAME_DLL )
	InitializeFoFServerState();
#endif

#endif
}

CHL2MPRules::~CHL2MPRules( void )
{
#ifndef CLIENT_DLL
	// Note, don't delete each team since they are in the gEntList and will 
	// automatically be deleted from there, instead.
	g_Teams.Purge();
#endif
}

void CHL2MPRules::CreateStandardEntities( void )
{

#ifndef CLIENT_DLL
	// Create the entity that will send our data to the client.

	BaseClass::CreateStandardEntities();

	g_pLastCombineSpawn = NULL;
	g_pLastRebelSpawn = NULL;

#ifdef DBGFLAG_ASSERT
	CBaseEntity *pEnt = 
#endif
	CBaseEntity::Create( "hl2mp_gamerules", vec3_origin, vec3_angle );
	Assert( pEnt );
#endif
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHL2MPRules::FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( weaponstay.GetInt() > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return 0;		// weapon respawns almost instantly
		}
	}

	return sv_hl2mp_weapon_respawn_time.GetFloat();
#endif

	return 0;		// weapon respawns almost instantly
}


bool CHL2MPRules::IsIntermission( void )
{
#ifndef CLIENT_DLL
	return m_flIntermissionEndTime > gpGlobals->curtime;
#endif

	return false;
}

void CHL2MPRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
#ifndef CLIENT_DLL
	if ( IsIntermission() )
		return;
#if defined( GAME_DLL )
	HandleFoFPlayerKilled( pVictim, info );
#endif
	BaseClass::PlayerKilled( pVictim, info );
#endif
}


void CHL2MPRules::Think( void )
{

#ifndef CLIENT_DLL
	
	CGameRules::Think();

#if defined( GAME_DLL )
	UpdateFoFServerState();
#endif

	if ( g_fGameOver )   // someone else quit the game already
	{
		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->curtime )
		{
			if ( !m_bChangelevelDone )
			{
				ChangeLevel(); // intermission is over
				m_bChangelevelDone = true;
			}
		}

		return;
	}

//	float flTimeLimit = mp_timelimit.GetFloat() * 60;
	float flFragLimit = fraglimit.GetFloat();
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bCourseMode = currentMode.IsValid() &&
		currentMode.GetInt() == 6;
	// Course scripts own their timers and completion.  Match limits must not
	// open the ordinary deathmatch results over the course end menu.

	if ( !bCourseMode && GetMapRemainingTime() < 0 )
	{
		GoToIntermission();
		return;
	}

	if ( !bCourseMode && flFragLimit )
	{
		if( IsTeamplay() == true )
		{
			CTeam *pCombine = g_Teams[TEAM_COMBINE];
			CTeam *pRebels = g_Teams[TEAM_REBELS];

			if ( pCombine->GetScore() >= flFragLimit || pRebels->GetScore() >= flFragLimit )
			{
				GoToIntermission();
				return;
			}
		}
		else
		{
			// check if any player is over the frag limit
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

				if ( pPlayer && pPlayer->FragCount() >= flFragLimit )
				{
					GoToIntermission();
					return;
				}
			}
		}
	}

	if ( gpGlobals->curtime > m_tmNextPeriodicThink )
	{		
		CheckAllPlayersReady();
		CheckRestartGame();
		m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
	}

	if ( m_flRestartGameTime > 0.0f && m_flRestartGameTime <= gpGlobals->curtime )
	{
		RestartGame();
	}

	if( m_bAwaitingReadyRestart && m_bHeardAllPlayersReady )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "All players ready. Game will restart in 5 seconds" );
		UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "All players ready. Game will restart in 5 seconds" );

		m_flRestartGameTime = gpGlobals->curtime + 5;
		m_bAwaitingReadyRestart = false;
	}

	ManageObjectRelocation();

#endif
}

void CHL2MPRules::GoToIntermission( void )
{
#ifndef CLIENT_DLL
	if ( g_fGameOver )
		return;

	g_fGameOver = true;

	m_flIntermissionEndTime = gpGlobals->curtime + mp_chattime.GetInt();

	#if defined( GAME_DLL )
	BeginFoFIntermission();
	#else
	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		pPlayer->ShowViewPortPanel( PANEL_SCOREBOARD );
		pPlayer->AddFlag( FL_FROZEN );
	}
	#endif
#endif
	
}

bool CHL2MPRules::CheckGameOver()
{
#ifndef CLIENT_DLL
	if ( g_fGameOver )   // someone else quit the game already
	{
		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->curtime )
		{
			ChangeLevel(); // intermission is over			
		}

		return true;
	}
#endif

	return false;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHL2MPRules::FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}
#endif
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	CWeaponHL2MPBase *pHL2Weapon = dynamic_cast< CWeaponHL2MPBase*>( pWeapon );

	if ( pHL2Weapon )
	{
		return pHL2Weapon->GetOriginalSpawnOrigin();
	}
#endif
	
	return pWeapon->GetAbsOrigin();
}

#ifndef CLIENT_DLL

CItem* IsManagedObjectAnItem( CBaseEntity *pObject )
{
	return dynamic_cast< CItem*>( pObject );
}

CWeaponHL2MPBase* IsManagedObjectAWeapon( CBaseEntity *pObject )
{
	return dynamic_cast< CWeaponHL2MPBase*>( pObject );
}

bool GetObjectsOriginalParameters( CBaseEntity *pObject, Vector &vOriginalOrigin, QAngle &vOriginalAngles )
{
	if ( CItem *pItem = IsManagedObjectAnItem( pObject ) )
	{
		if ( pItem->m_flNextResetCheckTime > gpGlobals->curtime )
			 return false;
		
		vOriginalOrigin = pItem->GetOriginalSpawnOrigin();
		vOriginalAngles = pItem->GetOriginalSpawnAngles();

		pItem->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_item_respawn_time.GetFloat();
		return true;
	}
	else if ( CWeaponHL2MPBase *pWeapon = IsManagedObjectAWeapon( pObject )) 
	{
		if ( pWeapon->m_flNextResetCheckTime > gpGlobals->curtime )
			 return false;

		vOriginalOrigin = pWeapon->GetOriginalSpawnOrigin();
		vOriginalAngles = pWeapon->GetOriginalSpawnAngles();

		pWeapon->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_weapon_respawn_time.GetFloat();
		return true;
	}

	return false;
}

void CHL2MPRules::ManageObjectRelocation( void )
{
	int iTotal = m_hRespawnableItemsAndWeapons.Count();

	if ( iTotal > 0 )
	{
		for ( int i = 0; i < iTotal; i++ )
		{
			CBaseEntity *pObject = m_hRespawnableItemsAndWeapons[i].Get();
			
			if ( pObject )
			{
				Vector vSpawOrigin;
				QAngle vSpawnAngles;

				if ( GetObjectsOriginalParameters( pObject, vSpawOrigin, vSpawnAngles ) == true )
				{
					float flDistanceFromSpawn = (pObject->GetAbsOrigin() - vSpawOrigin ).Length();

					if ( flDistanceFromSpawn > WEAPON_MAX_DISTANCE_FROM_SPAWN )
					{
						bool shouldReset = false;
						IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();

						if ( pPhysics )
						{
							shouldReset = pPhysics->IsAsleep();
						}
						else
						{
							shouldReset = (pObject->GetFlags() & FL_ONGROUND) ? true : false;
						}

						if ( shouldReset )
						{
							pObject->Teleport( &vSpawOrigin, &vSpawnAngles, NULL );
							pObject->EmitSound( "AlyxEmp.Charge" );

							IPhysicsObject *pPhys = pObject->VPhysicsGetObject();

							if ( pPhys )
							{
								pPhys->Wake();
							}
						}
					}
				}
			}
		}
	}
}

//=========================================================
//AddLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::AddLevelDesignerPlacedObject( CBaseEntity *pEntity )
{
	if ( m_hRespawnableItemsAndWeapons.Find( pEntity ) == -1 )
	{
		m_hRespawnableItemsAndWeapons.AddToTail( pEntity );
	}
}

//=========================================================
//RemoveLevelDesignerPlacedWeapon
//=========================================================
void CHL2MPRules::RemoveLevelDesignerPlacedObject( CBaseEntity *pEntity )
{
	if ( m_hRespawnableItemsAndWeapons.Find( pEntity ) != -1 )
	{
		m_hRespawnableItemsAndWeapons.FindAndRemove( pEntity );
	}
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHL2MPRules::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->GetOriginalSpawnOrigin();
}

//=========================================================
// What angles should this item use to respawn?
//=========================================================
QAngle CHL2MPRules::VecItemRespawnAngles( CItem *pItem )
{
	return pItem->GetOriginalSpawnAngles();
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHL2MPRules::FlItemRespawnTime( CItem *pItem )
{
	#if defined( GAME_DLL )
		return FoFGetItemRespawnTime();
	#endif

	return sv_hl2mp_item_respawn_time.GetFloat();
}

//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
bool CHL2MPRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	if ( weaponstay.GetInt() > 0 )
	{
		if ( pPlayer->Weapon_OwnsThisType( pItem->GetClassname(), pItem->GetSubType() ) )
			 return false;
	}

	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

#endif

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHL2MPRules::WeaponShouldRespawn( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( pWeapon->HasSpawnFlags( SF_NORESPAWN ) )
	{
		return GR_WEAPON_RESPAWN_NO;
	}
#endif

	return GR_WEAPON_RESPAWN_YES;
}

//-----------------------------------------------------------------------------
// Purpose: Player has just left the game
//-----------------------------------------------------------------------------
void CHL2MPRules::ClientDisconnected( edict_t *pClient )
{
#ifndef CLIENT_DLL
	// Msg( "CLIENT DISCONNECTED, REMOVING FROM TEAM.\n" );

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
	if ( pPlayer )
	{
		// Remove the player from his team
		if ( pPlayer->GetTeam() )
		{
			pPlayer->GetTeam()->RemovePlayer( pPlayer );
		}
	}

	BaseClass::ClientDisconnected( pClient );

#endif
}


//=========================================================
// Deathnotice.
//=========================================================
#if defined( GAME_DLL ) && !defined( CLIENT_DLL )
static const char *FoFDeathNoticeEntityName( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;

	CBaseCombatWeapon *pWeapon = pEntity->MyCombatWeaponPointer();
	if ( pWeapon )
	{
		const char *pszNoticeName = pWeapon->GetDeathNoticeName();
		if ( pszNoticeName && pszNoticeName[0] )
			return pszNoticeName;
	}
	return pEntity->GetClassname();
}

static void FoFNormalizeDeathNoticeName(
	const char *pszSource, int nDamageType,
	char *pszOutput, int nOutputSize )
{
	if ( !pszOutput || nOutputSize <= 0 )
		return;

	const char *pszName = pszSource && pszSource[0] ? pszSource : "world";
	if ( Q_stristr( pszName, "physics" ) )
	{
		pszName = ( nDamageType & DMG_BLAST ) ? "blast" : "physics";
	}
	else if ( !Q_strnicmp( pszName, "weapon_", 7 ) )
	{
		pszName += 7;
	}
	else if ( !Q_strnicmp( pszName, "npc_", 4 ) )
	{
		pszName += 4;
	}
	else if ( !Q_strnicmp( pszName, "func_", 5 ) )
	{
		pszName += 5;
	}

	Q_strncpy( pszOutput, pszName, nOutputSize );
}

static int FoFDeathNoticeWeaponIndex(
	const char *pszRawName, const char *pszNoticeName )
{
	int nIndex = FoFItemStatisticsIndex( pszRawName );
	if ( nIndex >= 0 )
		return nIndex;

	nIndex = FoFItemStatisticsIndex( pszNoticeName );
	if ( nIndex >= 0 || !pszNoticeName || !pszNoticeName[0] )
		return nIndex;

	char szWeaponName[128];
	Q_snprintf( szWeaponName, sizeof( szWeaponName ),
		"weapon_%s", pszNoticeName );
	return FoFItemStatisticsIndex( szWeaponName );
}
#endif

void CHL2MPRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
#ifndef CLIENT_DLL
#if defined( GAME_DLL )
	{
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );
	CFoF_Player *pFoFVictim = ToFoFPlayer( pVictim );
	CFoF_Player *pFoFScorer = ToFoFPlayer( pScorer );
	const int nDamageType = info.GetDamageType();

	const char *pszRawName = NULL;
	if ( pFoFVictim && pFoFScorer &&
		pFoFVictim->GetFoFKicker() == pFoFScorer &&
		( nDamageType & ( DMG_BURN | DMG_FALL | DMG_DROWN ) ) )
	{
		pszRawName = "kick-fall";
	}
	else if ( pFoFScorer && pFoFScorer->IsOnFoFHorse() &&
		( nDamageType & DMG_AIRBOAT ) )
	{
		pszRawName = "horse-ram";
	}
	else if ( nDamageType & DMG_BURN )
	{
		pszRawName = "flame";
	}
	else if ( nDamageType & DMG_PREVENT_PHYSICS_FORCE )
	{
		pszRawName = "thrown_gun";
	}
	else if ( info.GetDamageCustom() == 11 )
	{
		pszRawName = "kick";
	}
	else
	{
		CBaseEntity *pWeapon = info.GetWeapon();
		if ( pWeapon )
			pszRawName = FoFDeathNoticeEntityName( pWeapon );
		else if ( pScorer && pInflictor == pScorer )
			pszRawName = FoFDeathNoticeEntityName(
				pScorer->GetActiveWeapon() );
		else
			pszRawName = FoFDeathNoticeEntityName( pInflictor );
	}

	char szWeaponName[128];
	FoFNormalizeDeathNoticeName(
		pszRawName, nDamageType, szWeaponName, sizeof( szWeaponName ) );
	if ( pFoFScorer &&
		( !Q_stricmp( szWeaponName, "fists" ) ||
		  !Q_stricmp( szWeaponName, "weapon_fists" ) ) &&
		( pFoFScorer->GetFoFPlayerInfo() & 0x4 ) )
	{
		Q_strncpy( szWeaponName, "fists_brass", sizeof( szWeaponName ) );
		pszRawName = "fists_brass";
	}

	int nAssistUserID = 0;
	if ( pFoFVictim )
	{
		CFoF_Player *pAssister = ToFoFPlayer(
			pFoFVictim->hPlayerAssisted.Get() );
		if ( pAssister && pAssister != pFoFScorer )
			nAssistUserID = pAssister->GetUserID();
	}

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "player_death" );
	if ( pEvent )
	{
		pEvent->SetInt( "userid", pVictim->GetUserID() );
		pEvent->SetInt( "attacker", pScorer ? pScorer->GetUserID() : 0 );
		pEvent->SetString( "weapon", szWeaponName );
		pEvent->SetInt( "priority", 7 );
		pEvent->SetBool( "headshot", ( nDamageType & DMG_SHOCK ) != 0 );
		pEvent->SetInt( "assist", nAssistUserID );
		pEvent->SetInt( "damagebits", nDamageType );
		pEvent->SetBool( "penetration",
			info.GetPlayerPenetrationCount() > 0 );
		pEvent->SetInt( "weapon_index",
			FoFDeathNoticeWeaponIndex( pszRawName, szWeaponName ) );
		gameeventmanager->FireEvent( pEvent );
	}
	return;
	}
#endif
	// Work out what killed the player, and send a message to all clients about it
	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_ID = 0;

	// Find the killer & the scorer
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

	// Custom kill type?
	if ( info.GetDamageCustom() )
	{
		killer_weapon_name = GetDamageCustomString( info );
		if ( pScorer )
		{
			killer_ID = pScorer->GetUserID();
		}
	}
	else
	{
#if defined( GAME_DLL )
		CFoF_Player *pFoFScorer = ToFoFPlayer( pScorer );
		if ( pFoFScorer && pFoFScorer->IsOnFoFHorse() &&
			( info.GetDamageType() & DMG_AIRBOAT ) )
		{
			killer_ID = pFoFScorer->GetUserID();
			killer_weapon_name = "horse-ram";
		}
		else
#endif
		{
		// Is the killer a client?
		if ( pScorer )
		{
			killer_ID = pScorer->GetUserID();
			
			if ( pInflictor )
			{
				if ( pInflictor == pScorer )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( pScorer->GetActiveWeapon() )
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname();
					}
				}
				else
				{
					killer_weapon_name = pInflictor->GetClassname();  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = pInflictor->GetClassname();
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		{
			killer_weapon_name += 7;
		}
		else if ( strncmp( killer_weapon_name, "npc_", 4 ) == 0 )
		{
			killer_weapon_name += 4;
		}
		else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		{
			killer_weapon_name += 5;
		}
		else if ( strstr( killer_weapon_name, "physics" ) )
		{
			killer_weapon_name = "physics";
		}

		if ( strcmp( killer_weapon_name, "prop_combine_ball" ) == 0 )
		{
			killer_weapon_name = "combine_ball";
		}
		else if ( strcmp( killer_weapon_name, "grenade_ar2" ) == 0 )
		{
			killer_weapon_name = "smg1_grenade";
		}
		else if ( strcmp( killer_weapon_name, "satchel" ) == 0 || strcmp( killer_weapon_name, "tripmine" ) == 0)
		{
			killer_weapon_name = "slam";
		}
		}


	}

	IGameEvent *event = gameeventmanager->CreateEvent( "player_death" );
	if( event )
	{
		event->SetInt("userid", pVictim->GetUserID() );
		event->SetInt("attacker", killer_ID );
		event->SetString("weapon", killer_weapon_name );
		event->SetInt( "priority", 7 );
		gameeventmanager->FireEvent( event );
	}
#endif

}

void CHL2MPRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
#ifndef CLIENT_DLL

#if defined( GAME_DLL )
	HandleFoFClientSettingsChanged( pPlayer );
	return;
#endif

	CHL2MP_Player *pHL2Player = ToHL2MPPlayer( pPlayer );

	if ( pHL2Player == NULL )
		return;

	const char *pCurrentModel = modelinfo->GetModelName( pPlayer->GetModel() );
	const char *szModelName = engine->GetClientConVarValue( engine->IndexOfEdict( pPlayer->edict() ), "cl_playermodel" );

	//If we're different.
	if ( stricmp( szModelName, pCurrentModel ) )
	{
		//Too soon, set the cvar back to what it was.
		//Note: this will make this function be called again
		//but since our models will match it'll just skip this whole dealio.
		if ( pHL2Player->GetNextModelChangeTime() >= gpGlobals->curtime )
		{
			char szReturnString[512];

			Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", pCurrentModel );
			engine->ClientCommand ( pHL2Player->edict(), szReturnString );

			Q_snprintf( szReturnString, sizeof( szReturnString ), "Please wait %d more seconds before trying to switch.\n", (int)(pHL2Player->GetNextModelChangeTime() - gpGlobals->curtime) );
			ClientPrint( pHL2Player, HUD_PRINTTALK, szReturnString );
			return;
		}

		if ( HL2MPRules()->IsTeamplay() == false )
		{
			pHL2Player->SetPlayerModel();

			const char *pszCurrentModelName = modelinfo->GetModelName( pHL2Player->GetModel() );

			char szReturnString[128];
			Q_snprintf( szReturnString, sizeof( szReturnString ), "Your player model is: %s\n", pszCurrentModelName );

			ClientPrint( pHL2Player, HUD_PRINTTALK, szReturnString );
		}
		else
		{
			if ( Q_stristr( szModelName, "models/human") )
			{
				pHL2Player->ChangeTeam( TEAM_REBELS );
			}
			else
			{
				pHL2Player->ChangeTeam( TEAM_COMBINE );
			}
		}
	}
	if ( sv_report_client_settings.GetInt() == 1 )
	{
		UTIL_LogPrintf( "\"%s\" cl_cmdrate = \"%s\"\n", pHL2Player->GetPlayerName(), engine->GetClientConVarValue( pHL2Player->entindex(), "cl_cmdrate" ));
	}

	BaseClass::ClientSettingsChanged( pPlayer );
#endif
	
}

int CHL2MPRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
#ifndef CLIENT_DLL
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() || IsTeamplay() == false )
		return GR_NOTTEAMMATE;

#if defined( GAME_DLL )
	if ( FoFCourseBotsAreAllied(
		pPlayer->GetTeamNumber(), pTarget->GetTeamNumber() ) )
	{
		return GR_TEAMMATE;
	}
#endif

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}
#endif

	return GR_NOTTEAMMATE;
}

const char *CHL2MPRules::GetGameDescription( void )
{
	ConVar *pCurrentMode = cvar ?
		cvar->FindVar( "fof_sv_currentmode" ) : NULL;
	ConVar *pMaxTeams = cvar ? cvar->FindVar( "fof_sv_maxteams" ) : NULL;
	ConVar *pRankedServer = cvar ?
		cvar->FindVar( "fof_sv_rankedserver" ) : NULL;
	ConVar *pClassicShootout = cvar ?
		cvar->FindVar( "fof_sv_classic_shootout" ) : NULL;
	ConVar *pGhostTown = cvar ?
		cvar->FindVar( "fof_sv_ghost_town" ) : NULL;
	ConVar *pBattleRoyale = cvar ?
		cvar->FindVar( "fof_sv_battle_royale" ) : NULL;

	const int nMode = pCurrentMode ? pCurrentMode->GetInt() : 1;
	const int nTeams = pMaxTeams ? pMaxTeams->GetInt() : 4;
	if ( pRankedServer && pRankedServer->GetBool() && nMode == 1 )
	{
		if ( !IsTeamplay() )
			return "Ranked Shootout";
		if ( nTeams == 2 )
			return "Ranked 2 Teams";
		if ( nTeams == 3 )
			return "Ranked 3 Teams";
		if ( nTeams == 4 )
			return "Ranked 4 Teams";
	}

	if ( nMode == 2 )
		return "Teamplay";
	if ( nMode == 3 )
		return "Break Bad";
	if ( nMode == 4 )
	{
		return pBattleRoyale && pBattleRoyale->GetBool() ?
			"Grand Elimination" : "Team Elimination";
	}
	if ( nMode == 5 )
		return "Versus";
	if ( nMode == 6 )
		return "Course Mode";
	if ( pGhostTown && pGhostTown->GetBool() )
		return "Ghost Town";

	const bool bClassic =
		pClassicShootout && pClassicShootout->GetBool();
	if ( !IsTeamplay() )
		return bClassic ? "Classic Shootout" : "Shootout";
	if ( bClassic )
		return "Classic Team Shootout";
	if ( nTeams == 2 )
		return "2 Teams Shootout";
	if ( nTeams == 3 )
		return "3 Teams Shootout";
	if ( nTeams == 4 )
		return "4 Teams Shootout";
	return "Fistful of Frags";
}

bool CHL2MPRules::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
{
	return true;
}
 
float CHL2MPRules::GetMapRemainingTime()
{
	// if timelimit is disabled, return 0
	if ( mp_timelimit.GetInt() <= 0 )
		return 0;

	// timelimit is in minutes

	float timeleft = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f ) - gpGlobals->curtime;

	return timeleft;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPRules::Precache( void )
{
	CBaseEntity::PrecacheScriptSound( "AlyxEmp.Charge" );
}
bool CHL2MPRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}

	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER ||
		collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		return false;
	}

	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 

}

bool CHL2MPRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
#ifndef CLIENT_DLL
	if( BaseClass::ClientCommand( pEdict, args ) )
		return true;


	CHL2MP_Player *pPlayer = (CHL2MP_Player *) pEdict;

	if ( pPlayer->ClientCommand( args ) )
		return true;
#endif

	return false;
}

#ifdef CLIENT_DLL

	ConVar cl_autowepswitch(
		"cl_autowepswitch",
		"1",
		FCVAR_ARCHIVE | FCVAR_USERINFO,
		"Automatically switch to picked up weapons (if more powerful)" );

#else

#ifdef DEBUG

	// Handler for the "bot" command.
	void Bot_f()
	{		
		// Look at -count.
		int count = 1;
		count = clamp( count, 1, 16 );

		int iTeam = TEAM_COMBINE;
				
		// Look at -frozen.
		bool bFrozen = false;
			
		// Ok, spawn all the bots.
		while ( --count >= 0 )
		{
			BotPutInServer( bFrozen, iTeam );
		}
	}


	ConCommand cc_Bot( "bot", Bot_f, "Add a bot.", FCVAR_CHEAT );

#endif

	bool CHL2MPRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		// FoF adds acquired weapons to the inventory without Source's automatic
		// "more powerful weapon" switch; explicit FoF switch paths still call
		// Weapon_Switch themselves when a mode or hand replacement requires it.
		return false;
	}

#endif

#ifndef CLIENT_DLL

void CHL2MPRules::RestartGame()
{
#if defined( GAME_DLL )
	// The shipped FoF rules create the mode-owned entity during the round
	// restart path, before round_start is broadcast.  ServerActivate remains
	// a fallback for maps that never enter this path during startup.
	FoFCreateModeControllerForCurrentGame();
#endif

	// bounds check
	if ( mp_timelimit.GetInt() < 0 )
	{
		mp_timelimit.SetValue( 0 );
	}
	m_flGameStartTime = gpGlobals->curtime;
	if ( !IsFinite( m_flGameStartTime.Get() ) )
	{
		Warning( "Trying to set a NaN game start time\n" );
		m_flGameStartTime.GetForModify() = 0.0f;
	}

	CleanUpMap();
	
	// now respawn all players
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = (CHL2MP_Player*) UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( pPlayer->GetActiveWeapon() )
		{
			pPlayer->GetActiveWeapon()->Holster();
		}
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false );
		pPlayer->Reset();
	}

	// Respawn entities (glass, doors, etc..)

	CTeam *pRebels = GetGlobalTeam( TEAM_REBELS );
	CTeam *pCombine = GetGlobalTeam( TEAM_COMBINE );

	if ( pRebels )
	{
		pRebels->SetScore( 0 );
	}

	if ( pCombine )
	{
		pCombine->SetScore( 0 );
	}

	m_flIntermissionEndTime = 0;
	m_flRestartGameTime = 0.0;		
	m_bCompleteReset = false;

	IGameEvent * event = gameeventmanager->CreateEvent( "round_start" );
	if ( event )
	{
		event->SetInt("fraglimit", 0 );
		event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted

		event->SetString("objective","DEATHMATCH");

		gameeventmanager->FireEvent( event );
	}
}

void CHL2MPRules::CleanUpMap()
{
	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.

	// Get rid of all entities except players.
	CBaseEntity *pCur = gEntList.FirstEnt();
	while ( pCur )
	{
		CBaseHL2MPCombatWeapon *pWeapon = dynamic_cast< CBaseHL2MPCombatWeapon* >( pCur );
		// Weapons with owners don't want to be removed..
		if ( pWeapon )
		{
			if ( !pWeapon->GetPlayerOwner() )
			{
				UTIL_Remove( pCur );
			}
		}
		// remove entities that has to be restored on roundrestart (breakables etc)
		else if ( !FindInList( s_PreserveEnts, pCur->GetClassname() )
#if defined( GAME_DLL )
			&& !FoFShouldPreserveRoundEntity( pCur->GetClassname() )
#endif
			)
		{
			UTIL_Remove( pCur );
		}

		pCur = gEntList.NextEnt( pCur );
	}

	// Really remove the entities so we can have access to their slots below.
	gEntList.CleanupDeleteList();

	// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
	// could kill respawning CTs
	g_EventQueue.Clear();

	// Now reload the map entities.
	class CHL2MPMapEntityFilter : public IMapEntityFilter
	{
	public:
		virtual bool ShouldCreateEntity( const char *pClassname )
		{
			// Don't recreate the preserved entities.
			if ( !FindInList( s_PreserveEnts, pClassname )
#if defined( GAME_DLL )
				&& !FoFShouldPreserveRoundEntity( pClassname )
#endif
				)
			{
				return true;
			}
			else
			{
				// Increment our iterator since it's not going to call CreateNextEntity for this ent.
				if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
					m_iIterator = g_MapEntityRefs.Next( m_iIterator );

				return false;
			}
		}


		virtual CBaseEntity* CreateNextEntity( const char *pClassname )
		{
			if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
			{
				// This shouldn't be possible. When we loaded the map, it should have used 
				// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
				// with the same list of entities we're referring to here.
				Assert( false );
				return NULL;
			}
			else
			{
				CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
				m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

				if ( ref.m_iEdict == -1 || engine->PEntityOfEntIndex( ref.m_iEdict ) )
				{
					// Doh! The entity was delete and its slot was reused.
					// Just use any old edict slot. This case sucks because we lose the baseline.
					return CreateEntityByName( pClassname );
				}
				else
				{
					// Cool, the slot where this entity was is free again (most likely, the entity was 
					// freed above). Now create an entity with this specific index.
					return CreateEntityByName( pClassname, ref.m_iEdict );
				}
			}
		}

	public:
		int m_iIterator; // Iterator into g_MapEntityRefs.
	};
	CHL2MPMapEntityFilter filter;
	filter.m_iIterator = g_MapEntityRefs.Head();

	// DO NOT CALL SPAWN ON info_node ENTITIES!

	MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
}

void CHL2MPRules::CheckChatForReadySignal( CHL2MP_Player *pPlayer, const char *chatmsg )
{
	if( m_bAwaitingReadyRestart && FStrEq( chatmsg, mp_ready_signal.GetString() ) )
	{
		if( !pPlayer->IsReady() )
		{
			pPlayer->SetReady( true );
		}		
	}
}

void CHL2MPRules::CheckRestartGame( void )
{
	// Restart the game if specified by the server
	int iRestartDelay = mp_restartgame.GetInt();

	if ( iRestartDelay > 0 )
	{
		if ( iRestartDelay > 60 )
			iRestartDelay = 60;


		// let the players know
		char strRestartDelay[64];
		Q_snprintf( strRestartDelay, sizeof( strRestartDelay ), "%d", iRestartDelay );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );
		UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );

		m_flRestartGameTime = gpGlobals->curtime + iRestartDelay;
		m_bCompleteReset = true;
		mp_restartgame.SetValue( 0 );
	}

	if( mp_readyrestart.GetBool() )
	{
		m_bAwaitingReadyRestart = true;
		m_bHeardAllPlayersReady = false;
		

		const char *pszReadyString = mp_ready_signal.GetString();


		// Don't let them put anything malicious in there
		if( pszReadyString == NULL || Q_strlen(pszReadyString) > 16 )
		{
			pszReadyString = "ready";
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "hl2mp_ready_restart" );
		if ( event )
			gameeventmanager->FireEvent( event );

		mp_readyrestart.SetValue( 0 );

		// cancel any restart round in progress
		m_flRestartGameTime = -1;
	}
}

void CHL2MPRules::CheckAllPlayersReady( void )
{
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = (CHL2MP_Player*) UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;
		if ( !pPlayer->IsReady() )
			return;
	}
	m_bHeardAllPlayersReady = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CHL2MPRules::GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	const char *pszFormat = NULL;

	// team only
	if ( bTeamOnly == TRUE )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pszFormat = "HL2MP_Chat_Spec";
		}
		else
		{
			const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
			if ( chatLocation && *chatLocation )
			{
				pszFormat = "HL2MP_Chat_Team_Loc";
			}
			else
			{
				pszFormat = "HL2MP_Chat_Team";
			}
		}
	}
	// everyone
	else
	{
		if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		{
			pszFormat = "HL2MP_Chat_All";	
		}
		else
		{
			pszFormat = "HL2MP_Chat_AllSpec";
		}
	}

	return pszFormat;
}

#endif
