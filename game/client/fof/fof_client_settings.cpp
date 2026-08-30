#include "cbase.h"
#include "fof/fof_client_settings.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_properties.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Source normally exposes the archived m_pitch magnitude only while cheats are
// enabled.  A stale or externally supplied value therefore changes vertical
// mouse speed merely by toggling sv_cheats.  Keep FoF's normal pitch magnitude
// stable while retaining the user's inverted-mouse sign.
float FoFStableMousePitch( float flConfiguredPitch )
{
	return flConfiguredPitch > 0.0f ? 0.022f : -0.022f;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF client preferences and replicated server ConVar mirrors.
//
//=============================================================================//

ConVar fof_blood_allowed(
	"fof_blood_allowed", "1", FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Allows blood splatter effects" );

static ConVar fof_glow_allowed(
	"fof_glow_allowed", "1", FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Allows glow outlining effects" );

static ConVar fof_outline_dist(
	"fof_outline_dist", "512", FCVAR_ARCHIVE,
	"Max distance weapons do not get outlined anymore" );

static ConVar fof_browserfilters(
	"fof_browserfilters", "", FCVAR_ARCHIVE );
static ConVar fof_browserwarning(
	"fof_browserwarning", "0", FCVAR_ARCHIVE );
static ConVar fof_highlight_servers(
	"fof_highlight_servers", "1", FCVAR_ARCHIVE );

ConVar fof_weapon_lean(
	"fof_weapon_lean", "1", FCVAR_ARCHIVE );

ConVar cl_sidespeed(
	"cl_sidespeed", "400", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar cl_upspeed(
	"cl_upspeed", "320", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar cl_forwardspeed(
	"cl_forwardspeed", "400", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar cl_backspeed(
	"cl_backspeed", "400", FCVAR_REPLICATED | FCVAR_CHEAT );

// The original client registers this client-side command-avoidance limit
// with default "256" and flags 0x4010 (CHEAT | HIDDEN).
ConVar hl2mp_max_separation_force(
	"hl2mp_max_separation_force", "256", FCVAR_CHEAT | FCVAR_HIDDEN );

ConVar v_viewmodel_fov(
	"viewmodel_fov", "45", FCVAR_ARCHIVE, "",
	true, 40.0f, true, 50.0f );

// FoF registers a separate archived/userinfo sensitivity scale for the
// Sharps scope.  The server and external configuration surface expect this
// child ConVar to exist even though the stock zoom path owns the base value.
static ConVar zoom_sensitivity_ratio_sharps(
	"zoom_sensitivity_ratio_sharps", "0.5",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Additional mouse sensitivity scale factor applied when FOV is zoomed in." );

bool FoFResolveZoomSensitivity(
	C_BasePlayer *pPlayer,
	int nLocalFOV,
	int nDefaultFOV,
	float &flZoomSensitivityRatio )
{
	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	const bool bLongGun = pWeapon && pWeapon->FoFWeaponID() == 3;

	// The original client starts applying the zoom ratio as
	// soon as a long gun's sight expansion reaches 0.1, before integer FOV
	// interpolation necessarily leaves the default value.  The Sharps is the
	// original FoF-weight-8 special case.
	if ( nLocalFOV == nDefaultFOV &&
		( !bLongGun || FoFSightExpFactor( pPlayer ) < 0.1f ) )
	{
		return false;
	}

	if ( pWeapon && pWeapon->FoFWeaponWeight() == 8 )
		flZoomSensitivityRatio = zoom_sensitivity_ratio_sharps.GetFloat();

	return true;
}

static ConVar fof_announcement_version(
	"fof_announcement_version", "", FCVAR_ARCHIVE,
	"Latest announcement shown to client" );
static ConVar fof_update_version(
	"fof_update_version", "", FCVAR_ARCHIVE,
	"Last patch notes seen by client" );
static ConVar fof_player_prog(
	"fof_player_prog", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "",
	true, 0.0f, true, 100.0f );

// These mirrors are registered by both the original client and server.  They
// let dedicated-server SetConVar updates reach every client-side HUD and
// prediction reader instead of relying on a coincidental ConVar owner.
static ConVar fof_sv_team_glow(
	"fof_sv_team_glow", "", FCVAR_REPLICATED, "" );
static ConVar fof_sv_classic_shootout(
	"fof_sv_classic_shootout", "", FCVAR_REPLICATED, "" );
static ConVar fof_sv_headshots_only(
	"fof_sv_headshots_only", "", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes(
	"fof_sv_tp_classes", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_force_spect(
	"fof_sv_force_spect", "0", FCVAR_REPLICATED, "" );
static ConVar fof_warmup(
	"fof_warmup", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_course_forced_team(
	"fof_course_forced_team", "0", FCVAR_REPLICATED );

static ConVar fof_sv_tp_classes_c0(
	"fof_sv_tp_classes_c0", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c1(
	"fof_sv_tp_classes_c1", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c2(
	"fof_sv_tp_classes_c2", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c3(
	"fof_sv_tp_classes_c3", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c4(
	"fof_sv_tp_classes_c4", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c5(
	"fof_sv_tp_classes_c5", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c6(
	"fof_sv_tp_classes_c6", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_tp_classes_c7(
	"fof_sv_tp_classes_c7", "0", FCVAR_REPLICATED, "" );

static ConVar fof_sv_supercharged_respawntime(
	"fof_sv_supercharged_respawntime", "75", FCVAR_REPLICATED, "",
	true, 45.0f, true, 240.0f );
static ConVar fof_sv_supercharged(
	"fof_sv_supercharged", "1", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_ghost_town_xbow(
	"fof_sv_ghost_town_xbow", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_ghost_town(
	"fof_sv_ghost_town", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_dm_comp_points(
	"fof_sv_dm_comp_points", "500", FCVAR_REPLICATED, "",
	true, 500.0f, true, 1000.0f );
static ConVar fof_sv_dm_comp(
	"fof_sv_dm_comp", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_bot_edit_active(
	"fof_sv_bot_edit_active", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_bot_edit(
	"fof_sv_bot_edit", "0", FCVAR_REPLICATED, "" );

static ConVar fof_sv_nemesis(
	"fof_sv_nemesis", "1", FCVAR_REPLICATED,
	"Enables nemesis system for Shootout modes",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_max_ping(
	"fof_sv_max_ping", "250", FCVAR_REPLICATED,
	"Max latency players are allowed to play in a server. Set to 0 to disable" );
static ConVar fof_sv_winlimit(
	"fof_sv_winlimit", "0", FCVAR_REPLICATED,
	"How many points a team needs to get to win the map" );
static ConVar fof_sv_weaponmenu(
	"fof_sv_weaponmenu", "1", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_disable_killstreak(
	"fof_sv_disable_killstreak", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_teambalance_allowed(
	"fof_sv_teambalance_allowed", "1", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_elm_freevision(
	"fof_sv_elm_freevision", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_currentmode(
	"fof_sv_currentmode", "1", FCVAR_REPLICATED, "",
	true, 1.0f, true, 6.0f );
static ConVar map_start_time(
	"map_start_time", "0", FCVAR_REPLICATED, "" );
static ConVar fof_sv_elm_norespawn(
	"fof_sv_elm_norespawn", "0", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_elm_roundtime(
	"fof_sv_elm_roundtime", "240", FCVAR_REPLICATED, "",
	true, 0.0f, true, 400.0f );
static ConVar fof_sv_playerattack_allowed(
	"fof_sv_playerattack_allowed", "1", FCVAR_REPLICATED, "",
	true, 0.0f, true, 1.0f );
static ConVar fof_sv_speedpenalty(
	"fof_sv_speedpenalty", "0.7", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.7f, true, 0.7f );

static ConVar fof_sv_pricemult_henry(
	"fof_sv_pricemult_henry", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_walker(
	"fof_sv_pricemult_walker", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_sharps(
	"fof_sv_pricemult_sharps", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_dynamite(
	"fof_sv_pricemult_dynamite", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_righthanded(
	"fof_sv_pricemult_righthanded", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_lefthanded(
	"fof_sv_pricemult_lefthanded", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_bow(
	"fof_sv_pricemult_bow", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );
static ConVar fof_sv_pricemult_carbine(
	"fof_sv_pricemult_carbine", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 0.5f, true, 10.0f );

static ConVar fof_sv_weapon_drop(
	"fof_sv_weapon_drop", "0", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Teamplay cvar, set 0 for normal drops (as in DM), 1 all weapons drop, 2 all weapons drop with weapon aging, 3 no weapons drop",
	true, 0.0f, true, 99.0f );
static ConVar fof_sv_health_max(
	"fof_sv_health_max", "100", FCVAR_REPLICATED | FCVAR_NOTIFY, "",
	true, 100.0f, true, 166.0f );

static const char *kFoFClientSettingsPath = "cfg/config2.cfg";
static const char *kFoFTargetIdEnemiesSetting =
	"fof_hud_targetid_show_enemies";
static const int kFoFLauncherPingLimits[] =
{
	75,
	100,
	125,
	150,
	175,
	200
};

int FoFLauncherPingLimitFromIndex( int nIndex )
{
	if ( nIndex < 0 )
		nIndex = 0;
	else if ( nIndex >= ARRAYSIZE( kFoFLauncherPingLimits ) )
		nIndex = ARRAYSIZE( kFoFLauncherPingLimits ) - 1;
	return kFoFLauncherPingLimits[nIndex];
}

int FoFLauncherPingLimitIndexFromValue( int nValue )
{
	for ( int i = 0; i < ARRAYSIZE( kFoFLauncherPingLimits ); ++i )
	{
		if ( nValue <= kFoFLauncherPingLimits[i] )
			return i;
	}
	return ARRAYSIZE( kFoFLauncherPingLimits ) - 1;
}

static bool FoFReadClientSettingsToken(
	const char *&pCursor,
	char *pszToken,
	int nTokenSize )
{
	pszToken[0] = '\0';
	if ( !engine || !pCursor )
		return false;
	pCursor = engine->ParseFile( pCursor, pszToken, nTokenSize );
	return pszToken[0] != '\0';
}

int FoFReadPersistentClientInt( const char *pszName, int nFallback )
{
	if ( !filesystem || !pszName || !pszName[0] )
		return nFallback;

	CUtlBuffer settings( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !filesystem->ReadFile(
		kFoFClientSettingsPath, "MOD", settings ) )
	{
		return nFallback;
	}

	const char *pCursor = settings.String();
	char szName[128];
	char szValue[128];
	while ( FoFReadClientSettingsToken(
		pCursor, szName, sizeof( szName ) ) )
	{
		if ( !FoFReadClientSettingsToken(
			pCursor, szValue, sizeof( szValue ) ) )
		{
			break;
		}
		if ( !Q_stricmp( szName, pszName ) )
			return atoi( szValue );
	}
	return nFallback;
}

void FoFWritePersistentClientInt( const char *pszName, int nValue )
{
	if ( !filesystem || !pszName || !pszName[0] )
		return;

	ConVarRef targetIdEnemies( kFoFTargetIdEnemiesSetting, true );
	int nTargetIdEnemies = FoFReadPersistentClientInt(
		kFoFTargetIdEnemiesSetting,
		targetIdEnemies.IsValid() ? targetIdEnemies.GetInt() : 0 );
	if ( !Q_stricmp( pszName, kFoFTargetIdEnemiesSetting ) )
		nTargetIdEnemies = nValue;
	else
		return;

	CUtlBuffer settings( 0, 0, CUtlBuffer::TEXT_BUFFER );
	settings.PutString( "// Client-only FoF settings.\n" );
	settings.Printf(
		"%s \"%i\"\n",
		kFoFTargetIdEnemiesSetting,
		clamp( nTargetIdEnemies, 0, 1 ) );
	filesystem->CreateDirHierarchy( "cfg", "MOD" );
	filesystem->WriteFile(
		kFoFClientSettingsPath, "MOD", settings );
}
