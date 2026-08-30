//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws the normal TF2 or HL2 HUD.
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl2mp/clientmode_hl2mpnormal.h"
#include "fof/fof_postprocess_effects.h"
#include "fof/fof_launcher_panel.h"
#include "fof/fof_hud.h"
#include "fof/c_fof_player.h"
#include "fof/fof_spectator_ui.h"
#include "fof/fof_scoreboard.h"
#include "vgui_int.h"
#include "hud.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "iinput.h"
#include "fof/fof_motd.h"
#include "glow_outline_effect.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
vgui::HScheme g_hVGuiCombineScheme = 0;

// Fistful of Frags client settings queried by the original server through the
// userinfo string.  These live in the FoF client-mode translation unit so we
// do not collide with the single-player fov_desired definition.
ConVar fov_desired(
	"fov_desired",
	"90",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Sets the base field-of-view.",
	true, 75.0f,
	true, 90.0f );

ConVar fof_secondary_toggle(
	"fof_secondary_toggle",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO );

ConVar fof_bodyawareness(
	"fof_bodyawareness",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Controls self body awareness -see your own body- (on/off). When disabled, player can't kick",
	true, 0.0f,
	true, 1.0f );

// cfg/clientupdate.cfg in the shipped game updates this to the current
// protocol-facing value (23 at the time of this compatibility snapshot).
ConVar fof_cl_version(
	"fof_cl_version",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO );

ConVar fof_badge_pref(
	"fof_badge_pref",
	"2",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Controls what badge player sees in scoreboard",
	true, 0.0f,
	true, 3.0f );

ConVar fof_freezecam(
	"fof_freezecam",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Use freeze deathcam if available (0 off, 1 on, 2 only far kills)",
	true, 0.0f,
	true, 2.0f );

ConVar fof_crosshair_rifle(
	"fof_crosshair_rifle",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Displays crosshair for rifles too",
	true, 0.0f,
	true, 1.0f );

ConVar fof_autoreload(
	"fof_autoreload",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Controls gun automatic reloading",
	true, 0.0f,
	true, 1.0f );

ConVar fof_play_taunts(
	"fof_play_taunts",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Play other player taunts/voice commands",
	true, 0.0f,
	true, 1.0f );

ConVar fof_horse_freeaim(
	"fof_horse_freeaim",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Freely aim while riding a horse",
	true, 0.0f,
	true, 1.0f );

ConVar fof_browserseefull(
	"fof_browserseefull",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO );

// The original client contains this protocol-facing name but only creates it
// conditionally.  Registering it unconditionally avoids GetUserSetting noise
// from the shipped server and supplies the expected neutral value.
ConVar fpl_score(
	"fpl_score",
	"0",
	FCVAR_ARCHIVE | FCVAR_USERINFO );


// Instance the singleton and expose the interface to it.
IClientMode *GetClientModeNormal()
{
	static ClientModeHL2MPNormal g_ClientModeNormal;
	return &g_ClientModeNormal;
}

ClientModeHL2MPNormal* GetClientModeHL2MPNormal()
{
	Assert( dynamic_cast< ClientModeHL2MPNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModeHL2MPNormal* >( GetClientModeNormal() );
}

int ClientModeHL2MPNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

//-----------------------------------------------------------------------------
// ClientModeHLNormal implementation
//-----------------------------------------------------------------------------
ClientModeHL2MPNormal::ClientModeHL2MPNormal()
{
	m_pViewport = new CHudViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ClientModeHL2MPNormal::~ClientModeHL2MPNormal()
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ClientModeHL2MPNormal::Init()
{
	BaseClass::Init();
	FoFCreateLauncherPanel();

	// Load up the combine control panel scheme
	g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/CombinePanelScheme.res", "CombineScheme" );
	if (!g_hVGuiCombineScheme)
	{
		Warning( "Couldn't load combine panel scheme!\n" );
	}
}

void ClientModeHL2MPNormal::LevelInit( const char *newmap )
{
	// The shipped FoF client reopens the server MOTD after a
	// changelevel, before the new map's intro/team flow.
	FoFResetFirstMotdForLevel();
	BaseClass::LevelInit( newmap );
}

bool ClientModeHL2MPNormal::DoPostScreenSpaceEffects( const CViewSetup *pSetup )
{
	// FoF renders all entity outlines after the normal scene and before the
	// view model.  Weapon and crate entity messages only register glow objects;
	// this client-mode hook is what actually composites those objects onscreen.
	g_GlowObjectManager.RenderGlowEffects( pSetup, 0 );
	FoFDrawIronsightEffect( pSetup );
	return true;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fistful of Frags viewport and custom viewport panels.
//
//=============================================================================//

void CHudViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	gHUD.InitColors( pScheme );
	SetPaintBackgroundEnabled( false );
}

IViewPortPanel *CHudViewport::CreatePanelByName( const char *szPanelName )
{
	if ( Q_strcmp( PANEL_SCOREBOARD, szPanelName ) == 0 )
		return new CFoFScoreboardDialog( this );

	if ( Q_strcmp( PANEL_INFO, szPanelName ) == 0 )
		return new CFoFMotd( this );

	if ( Q_strcmp( PANEL_SPECGUI, szPanelName ) == 0 )
		return new CFoFSpectatorGUI( this );

	if ( Q_strcmp( PANEL_SPECMENU, szPanelName ) == 0 )
		return new CFoFSpectatorMenu( this );

	return BaseClass::CreatePanelByName( szPanelName );
}

const char *FoFResolveViewportPanelName( const char *pszPanelName )
{
	if ( pszPanelName && Q_stricmp( pszPanelName, "motd" ) == 0 )
		return PANEL_INFO;

	return pszPanelName;
}

void FoFOnViewportPanelMessage( const char *pszPanelName, bool bShow )
{
	if ( !pszPanelName )
		return;

	// The server opens every FoF equipment variant through the stock
	// buypreset_main viewport name.  The shipped equipmenu command then routes
	// that request by game mode; CHudFoF owns the equivalent routing here.
	if ( !Q_stricmp( pszPanelName, PANEL_BUYPRESET_MAIN ) )
	{
		CHudFoF *pHud = FoFHud();
		if ( pHud )
			pHud->SetEquipmentMenuVisible( bShow );
		return;
	}

	// Keep the stock PANEL_CLASS route available for compatible servers and
	// plugins. ClassMenuFoF is coordinated by CHudFoF instead of registering
	// a second IViewPortPanel.
	if ( !Q_stricmp( pszPanelName, PANEL_CLASS ) )
	{
		CHudFoF *pHud = FoFHud();
		if ( pHud )
			pHud->SetTeamClassMenuVisible( bShow );
		return;
	}

	if ( Q_stricmp( pszPanelName, PANEL_SCOREBOARD ) )
		return;

	C_FoF_Player *pPlayer = dynamic_cast< C_FoF_Player * >(
		C_BasePlayer::GetLocalPlayer() );
	if ( pPlayer )
		pPlayer->SetFoFMapIntermission( bShow );
}
