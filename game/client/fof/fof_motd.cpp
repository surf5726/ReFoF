//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "fof/fof_motd.h"
#include <cdll_client_int.h>
#include <vgui/IScheme.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <filesystem.h>
#include <KeyValues.h>
#include <convar.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/BuildGroup.h>
#include "vgui_bitmapbutton.h"
#include "IGameUIFuncs.h" // for key bindings
#include <igameresources.h>
#include <game/client/iviewport.h>
#include "networkstringtable_clientdll.h"
#include "inetchannelinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameUIFuncs *gameuifuncs; // for key binding details

using namespace vgui;

static ConVar fof_sv_motd_countdown(
	"fof_sv_motd_countdown",
	"1",
	FCVAR_REPLICATED,
	"MOTD panel countdown in seconds" );

static bool s_bFoFFirstMotdShown = false;
static bool s_bFoFFirstMotdDismissed = false;
static float s_flFoFFirstMotdWaitStarted = -1.0f;

static void FoFMarkFirstMotdSkipped()
{
	s_bFoFFirstMotdShown = true;
	s_bFoFFirstMotdDismissed = true;
}

static void FoFCloseSkippedFirstMotd()
{
	if ( !gViewPortInterface )
		return;

	IViewPortPanel *pPanel =
		gViewPortInterface->FindPanelByName( PANEL_INFO );
	if ( pPanel &&
		( pPanel->IsVisible() ||
		  gViewPortInterface->GetActivePanel() == pPanel ) )
	{
		// Closing through the viewport is required here.  SetVisible( false )
		// alone leaves PANEL_INFO registered as the active input panel and also
		// leaves CTextWindow's dim background enabled.
		gViewPortInterface->ShowPanel( pPanel, false );
	}
}

bool FoFIsListenServerSession()
{
	if ( !engine || !engine->IsInGame() )
		return false;

	// FoF sets this client flag when its launcher starts a listen server.  It
	// can remain set when the same client later connects to a dedicated
	// server, so also require the engine's in-process loopback channel.  A
	// localhost SRCDS still uses a network channel and therefore keeps MOTD.
	ConVar *pListenServer = cvar ? cvar->FindVar( "fof_listenserver" ) : NULL;
	INetChannelInfo *pNetChannel = engine->GetNetChannelInfo();
	return pListenServer && pListenServer->GetBool() &&
		pNetChannel && pNetChannel->IsLoopback();
}

bool FoFIsLocalCourseSession()
{
	if ( !cvar || !FoFIsListenServerSession() )
		return false;

	ConVar *pCurrentMode = cvar->FindVar( "fof_sv_currentmode" );
	if ( !pCurrentMode || pCurrentMode->GetInt() != 6 )
		return false;

	// The course parser publishes its forced team after successfully loading
	// the selected script.  Keep the script-name fallback for the brief frame
	// between the post-map launcher command and that parser completing.
	ConVar *pForcedTeam = cvar->FindVar( "fof_course_forced_team" );
	if ( pForcedTeam && pForcedTeam->GetInt() > 0 )
		return true;

	ConVar *pCourseScript = cvar->FindVar( "fof_course_script" );
	const char *pszCourseScript =
		pCourseScript ? pCourseScript->GetString() : NULL;
	if ( !pszCourseScript || !pszCourseScript[0] ||
		!Q_stricmp( pszCourseScript, "none" ) )
	{
		return false;
	}

	char mapName[MAX_PATH];
	Q_FileBase( engine->GetLevelName(), mapName, sizeof( mapName ) );
	return mapName[0] && Q_stristr( pszCourseScript, mapName ) != NULL;
}

static bool FoFShouldSkipFirstMotd()
{
	return FoFIsListenServerSession();
}

void FoFResetFirstMotdForLevel()
{
	s_bFoFFirstMotdShown = false;
	s_bFoFFirstMotdDismissed = false;
	s_flFoFFirstMotdWaitStarted = -1.0f;
}

bool FoFShowFirstMotd()
{
	if ( FoFShouldSkipFirstMotd() )
	{
		FoFMarkFirstMotdSkipped();
		FoFCloseSkippedFirstMotd();
		return false;
	}

	// The shipped FoF flow presents this panel once per map, before that
	// map's tutorial/team flow.  LevelInit explicitly resets this state.
	if ( s_flFoFFirstMotdWaitStarted < 0.0f && gpGlobals )
		s_flFoFFirstMotdWaitStarted = gpGlobals->curtime;

	if ( !gViewPortInterface || !g_pStringTableInfoPanel ||
		s_bFoFFirstMotdShown )
	{
		return false;
	}

	const int motdIndex =
		g_pStringTableInfoPanel->FindStringIndex( "motd" );
	if ( motdIndex == ::INVALID_STRING_INDEX )
		return false;

	int dataLength = 0;
	const char *motdData =
		static_cast< const char * >(
			g_pStringTableInfoPanel->GetStringUserData(
				motdIndex, &dataLength ) );
	if ( !motdData || dataLength <= 0 || !motdData[0] )
		return false;

	IViewPortPanel *panel =
		gViewPortInterface->FindPanelByName( PANEL_INFO );
	if ( !panel )
		return false;

	KeyValues *data = new KeyValues( "data" );
	data->SetInt( "type", TYPE_INDEX );
	data->SetString( "title", "" );
	data->SetString( "msg", "motd" );
	data->SetInt( "cmd", TEXTWINDOW_CMD_NONE );
	data->SetBool( "unload", true );
	panel->SetData( data );
	data->deleteThis();

	s_bFoFFirstMotdShown = true;
	s_bFoFFirstMotdDismissed = false;
	gViewPortInterface->ShowPanel( panel, true );
	return true;
}

bool FoFFirstMotdReadyForNextPanel()
{
	if ( FoFShouldSkipFirstMotd() )
	{
		FoFMarkFirstMotdSkipped();
		FoFCloseSkippedFirstMotd();
		return true;
	}

	if ( s_bFoFFirstMotdShown )
		return s_bFoFFirstMotdDismissed;

	// A few community servers intentionally publish no MOTD.  Keep their
	// tutorial/team flow usable while still giving the original server ample
	// time to populate the asynchronously rebuilt InfoPanel string table.
	return gpGlobals && s_flFoFFirstMotdWaitStarted >= 0.0f &&
		gpGlobals->curtime - s_flFoFFirstMotdWaitStarted >= 10.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFoFMotd::CFoFMotd(IViewPort *pViewPort) : CTextWindow( pViewPort )
{
	SetProportional( true );

	m_iScoreBoardKey = BUTTON_CODE_INVALID; // this is looked up in Activate()
	m_iFoFMotdBackgroundTexture = -1;
	m_iFoFLastContentType = -1;
	m_iFoFLayoutParentWide = -1;
	m_iFoFLayoutParentTall = -1;
	m_iFoFLayoutScreenWide = -1;
	m_iFoFLayoutScreenTall = -1;
	m_szFoFLastMessage[0] = '\0';
	m_flFoFMotdCountdown = 0.0f;
	m_pFoFOK = new CBitmapButton( this, "FoFMotdOK", "" );
	m_pFoFOK->AddActionSignalTarget( this );
	m_pFoFOK->SetCommand( "okay" );
	m_pFoFOK->SetMouseInputEnabled( true );
	m_pFoFOK->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	// The shipped FoF text-window constructor binds the OK button to the
	// small-cylinder rollover and light release samples.  Resource files do
	// not carry these bindings, so recreating the button alone is silent.
	m_pFoFOK->SetArmedSound( "ui/rollover_cyl2.wav" );
	m_pFoFOK->SetReleasedSound( "ui/release_fire1.wav" );
	m_pFoFOK->SetButtonBorderEnabled( false );
	m_pFoFOK->SetPaintBorderEnabled( false );
	m_pFoFOK->DrawFocusBox( false );
	m_pFoFOK->SetZPos( 20 );
	m_pFoFStayBackground = new CBitmapButton(
		this,
		"staybg",
		"#MOTD_background_button" );
	m_pFoFStayBackground->AddActionSignalTarget( this );
	m_pFoFStayBackground->SetCommand( "staybg" );
	m_pFoFStayBackground->SetMouseInputEnabled( true );
	m_pFoFStayBackground->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	m_pFoFStayBackground->SetArmedSound( "ui/rollover_cyl2.wav" );
	m_pFoFStayBackground->SetReleasedSound( "ui/release_fire1.wav" );
	m_pFoFStayBackground->SetButtonBorderEnabled( true );
	m_pFoFStayBackground->SetPaintBorderEnabled( false );
	m_pFoFStayBackground->SetPaintBackgroundEnabled( true );
	m_pFoFStayBackground->SetBgColor( Color( 10, 10, 10, 0 ) );
	m_pFoFStayBackground->SetAlpha( 255 );
	m_pFoFStayBackground->DrawFocusBox( false );
	m_pFoFStayBackground->SetContentAlignment( vgui::Label::a_west );
	m_pFoFStayBackground->SetVisible( false );
	m_pFoFStayBackground->SetZPos( 50 );
	m_pFoFMotdCountdown = new vgui::Label(
		this, "counter", "" );
	m_pFoFMotdCountdown->SetContentAlignment(
		vgui::Label::a_center );
	m_pFoFMotdCountdown->SetMouseInputEnabled( false );
	m_pFoFMotdCountdown->SetKeyBoardInputEnabled( false );
	m_pFoFMotdCountdown->SetPaintBackgroundEnabled( false );
	m_pFoFMotdCountdown->SetVisible( false );
	m_pFoFMotdCountdown->SetZPos( 30 );

	SetCloseButtonVisible( false );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFoFMotd::~CFoFMotd()
{
	if ( m_iFoFMotdBackgroundTexture >= 0 &&
		vgui::surface()->IsTextureIDValid( m_iFoFMotdBackgroundTexture ) )
	{
		vgui::surface()->DeleteTextureByID( m_iFoFMotdBackgroundTexture );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFoFMotd::Update()
{
	// The viewport may update every registered panel when spectator state
	// changes.  Do not restart the same web MOTD on each unrelated specgui
	// message; the repeated navigation prevents Steam's page from finishing.
	if ( m_bShownURL &&
		m_iFoFLastContentType == m_nContentType &&
		!Q_strcmp( m_szFoFLastMessage, m_szMessage ) )
	{
		m_pTextMessage->SetVisible( false );
		m_pHTMLMessage->SetVisible( true );
		m_pFoFOK->RequestFocus();
		return;
	}

	BaseClass::Update();
	if ( m_bShownURL )
	{
		m_iFoFLastContentType = m_nContentType;
		Q_strncpy(
			m_szFoFLastMessage,
			m_szMessage,
			sizeof( m_szFoFLastMessage ) );
	}
	else
	{
		m_iFoFLastContentType = -1;
		m_szFoFLastMessage[0] = '\0';
	}

	m_pFoFOK->RequestFocus();
}

void CFoFMotd::ShowURL(
	const char *URL,
	bool bAllowUserToDisable )
{
	( void )bAllowUserToDisable;
	// FoF deliberately presents its first-join web MOTD even though the
	// shipped user config sets cl_disablehtmlmotd.  Override the virtual URL
	// entry point so this also applies when TYPE_INDEX resolves the server's
	// "motd" string-table entry to a URL.
	BaseClass::ShowURL( URL, false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFoFMotd::SetVisible(bool state)
{
	const bool wasVisible = BaseClass::IsVisible();
	const bool isFirstMotd =
		m_nContentType == TYPE_INDEX &&
		!Q_stricmp( m_szMessage, "motd" );
	if ( state && isFirstMotd )
	{
		s_bFoFFirstMotdShown = true;
		s_bFoFFirstMotdDismissed = false;
	}
	BaseClass::SetVisible(state);
	m_pFoFOK->SetVisible( state );
	if ( m_pFoFStayBackground )
		m_pFoFStayBackground->SetVisible( false );
	if ( !state )
		EndFoFMotdCountdown();

	// Merely presenting the first MOTD is not permission to advance to the
	// tutorial/team flow.  The shipped client waits until the player closes
	// it.  Track the actual visible -> hidden transition so both the bitmap
	// OK button and controller/keyboard dismissal paths release the queue.
	if ( !state && wasVisible && isFirstMotd &&
		s_bFoFFirstMotdShown )
	{
		s_bFoFFirstMotdDismissed = true;
	}

	if ( state )
	{
		m_pFoFOK->MoveToFront();
		m_pFoFOK->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the text window
//-----------------------------------------------------------------------------
void CFoFMotd::ShowPanel(bool bShow)
{
	const bool isFirstMotd =
		m_nContentType == TYPE_INDEX &&
		!Q_stricmp( m_szMessage, "motd" );
	if ( bShow && isFirstMotd && FoFShouldSkipFirstMotd() )
	{
		FoFMarkFirstMotdSkipped();
		FoFCloseSkippedFirstMotd();
		return;
	}

	const bool wasVisible = BaseClass::IsVisible();
	if ( bShow )
	{
		// get key binding if shown
		if ( m_iScoreBoardKey == BUTTON_CODE_INVALID ) // you need to lookup the jump key AFTER the engine has loaded
		{
			m_iScoreBoardKey = gameuifuncs->GetButtonCodeForBind( "showscores" );
		}
	}

	BaseClass::ShowPanel( bShow );
	if ( bShow && !wasVisible && BaseClass::IsVisible() )
		BeginFoFMotdCountdown();
}

void CFoFMotd::BeginFoFMotdCountdown()
{
	if ( !m_pFoFMotdCountdown || !m_pFoFOK || !m_pHTMLMessage )
		return;
	if ( m_pFoFStayBackground )
		m_pFoFStayBackground->SetVisible( false );

	// The shipped client remaps the initial DS MOTD age from 0..600 seconds
	// onto 1..fof_sv_motd_countdown.  In particular, a replicated value of
	// zero still starts a one-second countdown instead of skipping it.
	m_flFoFMotdCountdown = m_bShownURL
		? RemapValClamped( 0.0f, 0.0f, 600.0f,
			1.0f, fof_sv_motd_countdown.GetFloat() )
		: 0.0f;
	if ( m_flFoFMotdCountdown <= 0.0f )
	{
		EndFoFMotdCountdown();
		return;
	}

	m_pFoFMotdCountdown->SetVisible( true );
	m_pFoFMotdCountdown->MoveToFront();
	m_pFoFOK->SetEnabled( false );
	m_pFoFOK->SetMouseInputEnabled( false );
	m_pHTMLMessage->SetMouseInputEnabled( false );
}

void CFoFMotd::EndFoFMotdCountdown()
{
	m_flFoFMotdCountdown = 0.0f;
	if ( m_pFoFMotdCountdown )
		m_pFoFMotdCountdown->SetVisible( false );
	if ( m_pFoFOK )
	{
		m_pFoFOK->SetEnabled( true );
		m_pFoFOK->SetMouseInputEnabled( true );
	}
	if ( m_pHTMLMessage )
		m_pHTMLMessage->SetMouseInputEnabled( true );
	if ( m_pFoFStayBackground )
	{
		const bool showStayBackground =
			IsVisible() && m_bShownURL;
		m_pFoFStayBackground->SetVisible( showStayBackground );
		m_pFoFStayBackground->SetMouseInputEnabled(
			showStayBackground );
		if ( showStayBackground )
			m_pFoFStayBackground->MoveToFront();
	}
}

void CFoFMotd::OnThink()
{
	BaseClass::OnThink();
	if ( IsVisible() &&
		m_nContentType == TYPE_INDEX &&
		!Q_stricmp( m_szMessage, "motd" ) &&
		FoFShouldSkipFirstMotd() )
	{
		FoFMarkFirstMotdSkipped();
		FoFCloseSkippedFirstMotd();
		return;
	}

	if ( !m_pHTMLMessage || !m_pFoFOK )
		return;

	if ( m_flFoFMotdCountdown > 0.0f )
	{
		char countdownText[32];
		Q_snprintf(
			countdownText,
			sizeof( countdownText ),
			"%.1f",
			m_flFoFMotdCountdown );
		m_pFoFMotdCountdown->SetText( countdownText );
		m_flFoFMotdCountdown -= gpGlobals->frametime;
		if ( m_flFoFMotdCountdown <= 0.0f )
			EndFoFMotdCountdown();
	}
	else if ( IsVisible() && m_pFoFOK->IsVisible() &&
		m_pFoFOK->IsEnabled() )
	{
		// The original client pulses the
		// enabled tm_ok image instead of using Button::SetBlink.
		const unsigned char brightness = static_cast< unsigned char >(
			fabs( sin( gpGlobals->curtime * 4.0f ) ) * 120.0f + 140.0f );
		const color32 okayColor = {
			brightness, brightness, brightness, 255 };
		m_pFoFOK->SetImage(
			CBitmapButton::BUTTON_ENABLED,
			"vgui/tm_ok",
			okayColor );
	}

	// A native HTML surface keeps absolute pixel bounds internally.  Refresh
	// the complete root/child layout together whenever the viewport changes,
	// rather than letting only the background and bitmap button pick up the
	// new proportional scale.
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	vgui::Panel *parent = GetParent();
	const int parentWide = parent ? parent->GetWide() : 0;
	const int parentTall = parent ? parent->GetTall() : 0;
	if ( screenWide != m_iFoFLayoutScreenWide ||
		screenTall != m_iFoFLayoutScreenTall ||
		parentWide != m_iFoFLayoutParentWide ||
		parentTall != m_iFoFLayoutParentTall )
	{
		LayoutFoFMotd();
	}

	// This SDK's HTML control owns a native browser surface which can keep
	// mouse focus over sibling VGUI controls even when the bitmap button is
	// painted in front of it.  The shipped FoF OK button overlaps the lower
	// edge of that browser.  Preserve the visual overlap, but temporarily
	// remove the browser from hit testing while the cursor is inside the OK
	// bounds so the whole visible button is clickable.
	bool cursorOverOkay = false;
	bool cursorOverStayBackground = false;
	if ( IsVisible() && m_pFoFOK->IsVisible() )
	{
		int cursorX = 0;
		int cursorY = 0;
		vgui::input()->GetCursorPos( cursorX, cursorY );
		m_pFoFOK->ScreenToLocal( cursorX, cursorY );
		cursorOverOkay =
			cursorX >= 0 && cursorY >= 0 &&
			cursorX < m_pFoFOK->GetWide() &&
			cursorY < m_pFoFOK->GetTall();
	}
	if ( IsVisible() && m_pFoFStayBackground &&
		m_pFoFStayBackground->IsVisible() )
	{
		int cursorX = 0;
		int cursorY = 0;
		vgui::input()->GetCursorPos( cursorX, cursorY );
		m_pFoFStayBackground->ScreenToLocal( cursorX, cursorY );
		cursorOverStayBackground =
			cursorX >= 0 && cursorY >= 0 &&
			cursorX < m_pFoFStayBackground->GetWide() &&
			cursorY < m_pFoFStayBackground->GetTall();
	}

	m_pHTMLMessage->SetMouseInputEnabled(
		m_flFoFMotdCountdown <= 0.0f &&
		!cursorOverOkay && !cursorOverStayBackground );
	if ( cursorOverOkay )
	{
		m_pFoFOK->SetMouseInputEnabled(
			m_flFoFMotdCountdown <= 0.0f );
		m_pFoFOK->MoveToFront();
	}
	if ( cursorOverStayBackground )
	{
		m_pFoFStayBackground->SetMouseInputEnabled(
			m_flFoFMotdCountdown <= 0.0f );
		m_pFoFStayBackground->MoveToFront();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Close the panel while deliberately retaining the HTML surface.
//-----------------------------------------------------------------------------
void CFoFMotd::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "staybg" ) )
	{
		// FoF routes staybg through the normal exit-command path after
		// clearing unload-on-dismissal.  Calling the stock "okay" branch with
		// that flag cleared preserves the page while still closing the viewport.
		m_bUnloadOnDismissal = false;
		BaseClass::OnCommand( "okay" );
		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFoFMotd::OnKeyCodePressed(KeyCode code)
{
	if ( m_iScoreBoardKey != BUTTON_CODE_INVALID && m_iScoreBoardKey == code )
	{
		gViewPortInterface->ShowPanel( PANEL_SCOREBOARD, true );
		gViewPortInterface->PostMessageToPanel( PANEL_SCOREBOARD, new KeyValues( "PollHideCode", "code", code ) );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------

static int FoFMotdScale( int logicalValue, int screenTall )
{
	// The shipped FoF client converts every TextWindow.res coordinate with
	// the current screen height divided by its 480-unit reference height.
	return RoundFloatToInt(
		static_cast< float >( logicalValue ) *
		static_cast< float >( screenTall ) / 480.0f );
}

void CFoFMotd::PaintBackground()
{
	if ( m_iFoFMotdBackgroundTexture < 0 )
		return;

	// The shipped client does not fit motdbg to the 400x330 TextWindow.
	// Its child bitmap is 400x410 proportional units and is clipped by the
	// root panel.  Keeping the extra 80 units is what carries the opaque
	// lower frame past the 295-unit HTML control.
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	if ( screenTall <= 0 )
		return;

	const int backgroundWide = FoFMotdScale( 400, screenTall );
	const int backgroundTall = FoFMotdScale( 410, screenTall );

	vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTexture( m_iFoFMotdBackgroundTexture );
	vgui::surface()->DrawTexturedRect(
		0, 0, backgroundWide, backgroundTall );
}

//-----------------------------------------------------------------------------
// Purpose: Keep every FoF MOTD control in the same proportional/local space.
//-----------------------------------------------------------------------------
void CFoFMotd::LayoutFoFMotd()
{
	vgui::Panel *parent = GetParent();
	int parentWide = 0;
	int parentTall = 0;
	if ( parent )
		parent->GetSize( parentWide, parentTall );

	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	if ( screenWide <= 0 || screenTall <= 0 )
	{
		screenWide = parentWide;
		screenTall = parentTall;
	}
	if ( screenWide <= 0 || screenTall <= 0 )
		return;

	const int rootWide = FoFMotdScale( 400, screenTall );
	const int rootTall = FoFMotdScale( 330, screenTall );
	int rootX = ( screenWide - rootWide ) / 2;
	int rootY = FoFMotdScale( 15, screenTall );

	// SetBounds is relative to the viewport panel, whereas GetScreenSize and
	// native HTML bounds use screen pixels.  Convert the desired screen-space
	// origin back to the parent's local space without using its (occasionally
	// stale after a mode switch) width for centering.
	if ( parent )
		parent->ScreenToLocal( rootX, rootY );
	SetBounds( rootX, rootY, rootWide, rootTall );

	const int contentX = FoFMotdScale( 10, screenTall );
	const int contentY = FoFMotdScale( 18, screenTall );
	const int contentWide = FoFMotdScale( 380, screenTall );
	const int contentTall = FoFMotdScale( 295, screenTall );
	m_pHTMLMessage->SetBounds(
		contentX, contentY, contentWide, contentTall );
	m_pTextMessage->SetBounds(
		contentX, contentY, contentWide, contentTall );

	m_pFoFOK->SetBounds(
		FoFMotdScale( 275, screenTall ),
		FoFMotdScale( 275, screenTall ),
		FoFMotdScale( 100, screenTall ),
		FoFMotdScale( 50, screenTall ) );
	m_pFoFOK->MoveToFront();
	m_pFoFStayBackground->SetBounds(
		FoFMotdScale( 15, screenTall ),
		FoFMotdScale( 305, screenTall ),
		FoFMotdScale( 130, screenTall ),
		FoFMotdScale( 20, screenTall ) );
	m_pFoFStayBackground->SetTextInset(
		FoFMotdScale( 5, screenTall ),
		MAX( FoFMotdScale( 1, screenTall ), 1 ) );
	if ( m_pFoFStayBackground->IsVisible() )
		m_pFoFStayBackground->MoveToFront();
	m_pFoFMotdCountdown->SetBounds(
		FoFMotdScale( 275, screenTall ),
		FoFMotdScale( 250, screenTall ),
		FoFMotdScale( 100, screenTall ),
		FoFMotdScale( 30, screenTall ) );
	if ( m_pFoFMotdCountdown->IsVisible() )
		m_pFoFMotdCountdown->MoveToFront();

	// Force the backing browser surface to adopt the new local rectangle.
	m_pHTMLMessage->InvalidateLayout( true );
	m_iFoFLayoutParentWide = parentWide;
	m_iFoFLayoutParentTall = parentTall;
	m_iFoFLayoutScreenWide = screenWide;
	m_iFoFLayoutScreenTall = screenTall;
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CFoFMotd::PerformLayout()
{
	BaseClass::PerformLayout();
	LayoutFoFMotd();

	// Unlike the stock multiplayer window, FoF keeps the proportional 400x330
	// root loaded from TextWindow.res.  Stretching this Frame to the viewport
	// would turn motdbg into a nearly full-screen sheet and vertically center
	// the HTML content instead of placing the panel near the top.
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFoFMotd::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pOK->SetVisible( false );
	m_pTitleLabel->SetVisible( false );
	SetCloseButtonVisible( false );
	// The shipped client applies this scheme color directly to the native
	// HTML panel.  ClientScheme does not define it, so the original fallback
	// is opaque red while the page is loading/counting down.
	m_pHTMLMessage->SetBgColor(
		pScheme->GetColor(
			"HTMLBackground",
			Color( 255, 0, 0, 255 ) ) );
	if ( m_iFoFMotdBackgroundTexture < 0 )
	{
		m_iFoFMotdBackgroundTexture =
			vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iFoFMotdBackgroundTexture,
			"vgui/motdbg",
			true,
			false );
	}

	const color32 white = { 255, 255, 255, 255 };
	const color32 armed = { 255, 238, 190, 255 };
	const color32 pressed = { 190, 165, 120, 255 };
	const color32 disabled = { 100, 100, 100, 180 };
	m_pFoFOK->SetImage(
		CBitmapButton::BUTTON_ENABLED, "vgui/tm_ok", white );
	m_pFoFOK->SetImage(
		CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/tm_ok", armed );
	m_pFoFOK->SetImage(
		CBitmapButton::BUTTON_PRESSED, "vgui/tm_ok", pressed );
	m_pFoFOK->SetImage(
		CBitmapButton::BUTTON_DISABLED, "vgui/tm_ok", disabled );
	const color32 stayEnabled = { 255, 255, 255, 255 };
	const color32 stayArmed = { 255, 230, 170, 255 };
	const color32 stayDisabled = { 55, 55, 85, 155 };
	m_pFoFStayBackground->SetImage(
		CBitmapButton::BUTTON_ENABLED,
		"vgui/slide_bg_small",
		stayEnabled );
	m_pFoFStayBackground->SetImage(
		CBitmapButton::BUTTON_ENABLED_MOUSE_OVER,
		"vgui/slide_bg_small",
		stayArmed );
	m_pFoFStayBackground->SetImage(
		CBitmapButton::BUTTON_PRESSED,
		"vgui/slide_bg_small",
		stayArmed );
	m_pFoFStayBackground->SetImage(
		CBitmapButton::BUTTON_DISABLED,
		"vgui/slide_bg_small",
		stayDisabled );
	// CTextWindow's m_hSmallFont animation variable assigns
	// HudSelectionNumbers4 to staybg; Default is too small at high resolutions.
	vgui::HFont stayFont =
		pScheme->GetFont( "HudSelectionNumbers4", true );
	if ( stayFont == vgui::INVALID_FONT )
		stayFont = pScheme->GetFont( "Default", true );
	m_pFoFStayBackground->SetFont( stayFont );
	m_pFoFStayBackground->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pFoFStayBackground->SetBgColor( Color( 10, 10, 10, 0 ) );
	m_pFoFStayBackground->SetZPos( 50 );
	m_pFoFStayBackground->SetPaintBorderEnabled( false );
	m_pFoFStayBackground->SetPaintBackgroundEnabled( true );
	m_pFoFStayBackground->SetAlpha( 255 );
	m_pFoFStayBackground->SetContentAlignment(
		vgui::Label::a_west );
	vgui::HFont countdownFont =
		pScheme->GetFont( "ClientTitleFont", true );
	if ( countdownFont == vgui::INVALID_FONT )
		countdownFont = pScheme->GetFont( "MenuFontMed", true );
	if ( countdownFont == vgui::INVALID_FONT )
		countdownFont = pScheme->GetFont( "Default", true );
	m_pFoFMotdCountdown->SetFont( countdownFont );
	m_pFoFMotdCountdown->SetFgColor( Color( 210, 210, 210, 255 ) );
	m_pFoFMotdCountdown->SetContentAlignment(
		vgui::Label::a_center );
	LayoutFoFMotd();
}
