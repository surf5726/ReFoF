//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "steam/steam_api.h"
#include "fof/fof_scoreboard.h"
#include "c_team.h"
#include "c_playerresource.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_player_shared.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/IImage.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/SectionedListPanel.h>
#include "voice_status.h"
#include "fof/fof_motd.h"
#include <vgui_controls/ScrollBar.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static CFoFScoreboardDialog *g_pFoFScoreboard = NULL;

//-----------------------------------------------------------------------------
// Purpose: Konstructor
//-----------------------------------------------------------------------------
CFoFScoreboardDialog::CFoFScoreboardDialog(
	IViewPort *pViewPort )
	: CClientScoreBoardDialog( pViewPort ),
	  m_pPlayerListDM( NULL ),
	  m_pPlayerListR( NULL ),
	  m_pPlayerListC( NULL ),
	  m_pBackground( NULL ),
	  m_pMuteButtonLeft( NULL ),
	  m_pMuteButtonRight( NULL ),
	  m_pTimeLeft( NULL ),
	  m_iFoFPanelWide( 0 ),
	  m_iFoFPanelTall( 0 )
{
	for ( int i = 0; i < FOF_SCOREBOARD_BADGE_COUNT; ++i )
		m_iFoFBadgeImages[i] = -1;

	// ScoreBoard.res formats the title through %server%.  The scoreboard can
	// be constructed after server_spawn on a late video-mode/resource reload,
	// so keep the stock FoF hostname as a stable fallback until an event
	// supplies the real (possibly custom) server name.
	Q_strncpy( m_szFoFServerName, "Fistful of Frags Server",
		sizeof( m_szFoFServerName ) );

	// The shipped scoreboard resource contains three independent
	// SectionedListPanels.  The SDK base constructor creates a stock child
	// named PlayerList before loading that resource, so explicitly bind the
	// base pointer to the FoF deathmatch list and retain the two team lists.
	SectionedListPanel *legacyList = m_pPlayerList;
	m_pPlayerListDM = dynamic_cast<SectionedListPanel *>(
		FindChildByName( "PlayerListDM" ) );
	if ( !m_pPlayerListDM )
	{
		legacyList->SetName( "PlayerListDM" );
		m_pPlayerListDM = legacyList;
	}
	else
	{
		legacyList->SetVisible( false );
	}

	m_pPlayerListR = dynamic_cast<SectionedListPanel *>(
		FindChildByName( "PlayerListR" ) );
	if ( !m_pPlayerListR )
		m_pPlayerListR = new SectionedListPanel( this, "PlayerListR" );

	m_pPlayerListC = dynamic_cast<SectionedListPanel *>(
		FindChildByName( "PlayerListC" ) );
	if ( !m_pPlayerListC )
		m_pPlayerListC = new SectionedListPanel( this, "PlayerListC" );

	m_pPlayerList = m_pPlayerListDM;

	m_pBackground = new ImagePanel( this, NULL );
	m_pBackground->SetImage( "scoreboardbg" );
	m_pBackground->SetZPos( -1 );
	m_pBackground->SetShouldScaleImage( true );
	m_pBackground->SetProportional( true );

	// Original FoF brackets the voice-chat hint with two state images.  They
	// are created in code (and therefore do not appear in ScoreBoard.res).
	m_pMuteButtonLeft =
		new ImagePanel( this, "FoFMuteButtonLeft" );
	m_pMuteButtonRight =
		new ImagePanel( this, "FoFMuteButtonRight" );
	ImagePanel *muteButtons[] =
		{ m_pMuteButtonLeft, m_pMuteButtonRight };
	for ( int i = 0; i < ARRAYSIZE( muteButtons ); ++i )
	{
		muteButtons[i]->SetImage( "mute_button_off" );
		muteButtons[i]->SetShouldScaleImage( true );
		muteButtons[i]->SetMouseInputEnabled( false );
		muteButtons[i]->SetKeyBoardInputEnabled( false );
		muteButtons[i]->SetZPos( 10 );
	}

	// FoF draws the match timer above (and outside) the scoreboard card. Keep
	// it as a viewport sibling so the scoreboard's clipping rectangle cannot
	// cut it off at the upper edge.
	m_pTimeLeft = new Label(
		GetParent(), "FoFScoreboardTimeLeft", L"" );
	m_pTimeLeft->SetProportional( true );
	m_pTimeLeft->SetContentAlignment( Label::a_center );
	m_pTimeLeft->SetPaintBackgroundEnabled( false );
	m_pTimeLeft->SetMouseInputEnabled( false );
	m_pTimeLeft->SetKeyBoardInputEnabled( false );
	m_pTimeLeft->SetVisible( false );
	m_pTimeLeft->SetZPos( 30 );

	// Reload after all three children exist so ScoreBoard.res can apply the
	// original 620x420 root and the DM/R/C list bounds to real controls.
	LoadControlSettings( "Resource/UI/ScoreBoard.res" );
	m_iFoFPanelWide = GetWide();
	m_iFoFPanelTall = GetTall();

	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		lists[i]->SetVerticalScrollbar( false );
		lists[i]->SetBgColor( Color( 0, 0, 0, 0 ) );
		lists[i]->SetBorder( NULL );
	}

	g_pFoFScoreboard = this;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFoFScoreboardDialog::~CFoFScoreboardDialog()
{
	if ( m_pTimeLeft )
	{
		m_pTimeLeft->MarkForDeletion();
		m_pTimeLeft = NULL;
	}

	if ( g_pFoFScoreboard == this )
		g_pFoFScoreboard = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings
//-----------------------------------------------------------------------------
void CFoFScoreboardDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// The original reloads after the derived DM/R/C controls exist.  It also
	// repeats this on scheme changes so windowed/fullscreen transitions keep
	// the resource-authored bounds.
	LoadControlSettings( "Resource/UI/ScoreBoard.res" );
	if ( m_szFoFServerName[0] )
		SetDialogVariable( "server", m_szFoFServerName );
	if ( GetWide() >= 400 && GetTall() >= 300 )
	{
		m_iFoFPanelWide = GetWide();
		m_iFoFPanelTall = GetTall();
	}

	HFont scoreboardFont = pScheme->GetFont( "FoFScoreboard", false );

	// FoF replaces the stock scoreboard image list with its complete badge
	// set.  The first image is the 15x15 FoF icon; all rank/status badges are
	// authored as 20x10 images and are proportionally scaled afterwards.
	if ( m_pImageList )
		delete m_pImageList;
	m_pImageList = new ImageList( false );
	m_mapAvatarsToImageList.RemoveAll();

	static const char *badgeImageNames[] =
	{
		"icon_fof",
		"badge_bot",
		"badge_dev",
		"badge_contrib",
		"badge_special",
		"badge_l1",
		"badge_l2",
		"badge_l3",
		"badge_l4",
		"badge_elm_top10",
		"badge_elm_top50",
		"badge_elm_top100",
		"badge_1st",
		"badge_2nd",
		"badge_3rd",
		"badge_elm_top1"
	};
	COMPILE_TIME_ASSERT( ARRAYSIZE( badgeImageNames ) ==
		FOF_SCOREBOARD_BADGE_COUNT );

	for ( int i = 0; i < FOF_SCOREBOARD_BADGE_COUNT; ++i )
	{
		m_iFoFBadgeImages[i] = -1;
		IImage *badgeImage = scheme()->GetImage( badgeImageNames[i], true );
		if ( !badgeImage )
			continue;

		if ( i == FOF_SCOREBOARD_ICON )
			badgeImage->SetSize( 15, 15 );
		else
			badgeImage->SetSize( 20, 10 );
		m_iFoFBadgeImages[i] = m_pImageList->AddImage( badgeImage );
	}
	PostApplySchemeSettings( pScheme );

	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		if ( !lists[i] )
			continue;

		lists[i]->SetImageList( m_pImageList, false );
		// The original client installs FoFScoreboard as the
		// SectionedListPanel header font before the ranked-team sections are
		// created, so Break Bad's $ totals inherit it automatically.
		lists[i]->SetHeaderFont( scoreboardFont );
		lists[i]->SetRowFont( scoreboardFont );
		lists[i]->SetBgColor( Color( 0, 0, 0, 0 ) );
		lists[i]->SetBorder( NULL );
	}

	// The resource intentionally leaves these labels without an FgColor.
	// The original client writes opaque white to every one; without that pass
	// this SDK renders the column
	// headings transparent.
	static const char *whiteHeaderControls[] =
	{
		"DM_PlayerCount", "DM_NotorietyHeader", "DM_ScoreHeader",
		"DM_PingHeader", "R_TeamScoreLabel", "R_PlayerCount",
		"R_NotorietyHeader", "R_ScoreHeader", "R_CashHeader",
		"R_PingHeader", "C_TeamScoreLabel", "C_PlayerCount",
		"C_NotorietyHeader", "C_ScoreHeader", "C_CashHeader",
		"C_PingHeader"
	};
	for ( int i = 0; i < ARRAYSIZE( whiteHeaderControls ); ++i )
	{
		Panel *control = FindChildByName( whiteHeaderControls[i] );
		if ( control )
			control->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	// The voice-chat hint uses the bright RGBA value 178, 240, 40, 255.
	Label *mutePlayerHint = dynamic_cast<Label *>(
		FindChildByName( "MutePlayerHint" ) );
	if ( mutePlayerHint )
	{
		mutePlayerHint->SetFgColor( Color( 178, 240, 40, 255 ) );
		mutePlayerHint->SetFont(
			pScheme->GetFont( "FoFSlideBig", false ) );
		mutePlayerHint->SetContentAlignment( Label::a_center );
		mutePlayerHint->SetZPos( 9 );
	}

	if ( m_pTimeLeft )
	{
		m_pTimeLeft->SetFont(
			pScheme->GetFont( "MenuFontSmall", true ) );
		m_pTimeLeft->SetFgColor( Color( 255, 255, 255, 255 ) );
		m_pTimeLeft->SetBorder( NULL );
	}

	SetBgColor( Color(0, 0, 0, 0) );
	// FoF's shipped ClientScheme makes this root border invisible and lets
	// scoreboardbg provide the rounded black edge.  The SDK BaseBorder is
	// bright white, so applying it here draws a second rectangular frame
	// that is absent from the original at every resolution.
	SetBorder( NULL );
	Reset();
}

void CFoFScoreboardDialog::ShowPanel( bool bShow )
{
	BaseClass::ShowPanel( bShow );

	if ( m_pTimeLeft )
	{
		// The scoreboard is constructed with no VGUI parent and is attached
		// to the viewport only after its constructor returns.  Reparent this
		// sibling lazily on first show; otherwise it is visible in state but
		// has no drawable surface tree.
		if ( GetParent() && m_pTimeLeft->GetParent() != GetParent() )
			m_pTimeLeft->SetParent( GetParent() );
		m_pTimeLeft->SetVisible( bShow );
		if ( bShow )
		{
			UpdateFoFTimeLeft();
			m_pTimeLeft->MoveToFront();
		}
	}
}

void CFoFScoreboardDialog::FireGameEvent( IGameEvent *event )
{
	if ( event && !Q_strcmp( event->GetName(), "server_spawn" ) )
	{
		const char *hostname = event->GetString( "hostname", "" );
		if ( hostname && hostname[0] )
		{
		Q_strncpy(
			m_szFoFServerName,
				hostname,
			sizeof( m_szFoFServerName ) );
		}
	}

	BaseClass::FireGameEvent( event );

	// BaseClass posts text directly to ServerName.  Reassert the dialog
	// variable afterwards as well, otherwise the next unrelated variable
	// update expands %server% back to "[unknown]".
	if ( event && !Q_strcmp( event->GetName(), "server_spawn" ) )
		SetDialogVariable( "server", m_szFoFServerName );
}

void CFoFScoreboardDialog::Reset()
{
	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		if ( !lists[i] )
			continue;
		lists[i]->DeleteAllItems();
		lists[i]->RemoveAllSections();
	}

	m_iSectionId = 0;
	m_fNextUpdateTime = 0.0f;
	ConfigureFoFListSections();
	UpdateFoFModeVisibility();
}

void CFoFScoreboardDialog::Update()
{
	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		if ( lists[i] )
			lists[i]->DeleteAllItems();
	}

	UpdateFoFModeVisibility();
	UpdateTeamInfo();
	UpdatePlayerInfo();

	Panel *spectatorLabel = FindChildByName( "Spectators" );
	const int panelWide = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 620 );
	const int teamPanelTall = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 350 );
	const int dmPanelTall = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 420 );
	const int dmListTall = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 400 - 30 );
	const int teamListTall =
		scheme()->GetProportionalScaledValueEx( GetScheme(), 300 ) -
		( dmPanelTall - teamPanelTall );
	if ( GetFoFMode() == 2 )
	{
		// Restore the resource-authored logical geometry on every update.
		// All three list controls use autoresize=3, so relying on their
		// previous physical bounds accumulates width/height errors when a
		// video mode or FoF game mode changes.
		SetSize( panelWide, teamPanelTall );
		const int teamListSplit =
			scheme()->GetProportionalScaledValueEx( GetScheme(), 310 );
		if ( m_pPlayerListR )
		{
			m_pPlayerListR->SetBounds(
				0,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 50 ),
				teamListSplit,
				teamListTall );
		}
		if ( m_pPlayerListC )
		{
			// Scale the split once, then give the right-hand list the exact
			// remainder of the parent.  Independently scaling 310 + 310 can
			// round a few pixels wider than scaled 620; the parent then clips
			// the Vigilantes ping values at its right edge.
			m_pPlayerListC->SetBounds(
				teamListSplit,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 50 ),
				MAX( GetWide() - teamListSplit, 1 ),
				teamListTall );
		}
		if ( m_pBackground )
		{
			const int backgroundSize = GetWide();
			m_pBackground->SetSize( backgroundSize, backgroundSize );
			if ( m_pBackground->GetImage() )
			{
				m_pBackground->GetImage()->SetSize(
					backgroundSize, backgroundSize );
			}
		}
		if ( spectatorLabel )
		{
			const int spectatorX =
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 20 );
			spectatorLabel->SetBounds(
				spectatorX,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 275 ),
				GetWide() - spectatorX * 2,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 20 ) );
		}
	}
	else
	{
		// Original UpdateFoFVisibility asks the
		// resource-authored list for its 360-unit width before shrinking the
		// 620-unit teamplay root.  The original then adds four *physical*
		// pixels (not four proportional units) to the parent width.
		const int contentWide = GetFoFDeathmatchListWide();
		SetSize( contentWide + 4, dmPanelTall );

		// Keep the original ordering: resize the
		// parent first, then restore the child after autoresize runs.  Using
		// explicit logical bounds also makes switching modes and resolutions
		// idempotent rather than feeding the last physical size back in.
		if ( m_pPlayerListDM )
		{
			m_pPlayerListDM->SetBounds(
				0,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 15 ),
				contentWide,
				dmListTall );
		}
		if ( m_pBackground )
		{
			const int backgroundSize =
				GetWide() +
				scheme()->GetProportionalScaledValueEx( GetScheme(), 38 );
			m_pBackground->SetSize( backgroundSize, backgroundSize );
			if ( m_pBackground->GetImage() )
			{
				m_pBackground->GetImage()->SetSize(
					backgroundSize, backgroundSize );
			}
		}
		if ( spectatorLabel )
		{
			const int spectatorX =
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 10 );
			spectatorLabel->SetBounds(
				spectatorX,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 380 ),
				GetWide() - spectatorX * 2,
				scheme()->GetProportionalScaledValueEx(
					GetScheme(), 20 ) );
		}
	}

	// SectionedListPanel reapplies both TransparentLightBlack and
	// ButtonDepressedBorder from its own ApplySchemeSettings after the
	// scoreboard resource is reloaded.  Original FoF draws the unified tint
	// solely through scoreboardbg; leaving the list background enabled puts
	// a second black layer over only the middle of the board and makes the
	// uncovered top and bottom look like two differently coloured bars.
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		if ( lists[i] )
		{
			lists[i]->SetBgColor( Color( 0, 0, 0, 0 ) );
			lists[i]->SetBorder( NULL );
		}
	}

	FitFoFListLineSpacing();
	LayoutFoFMuteHint();
	MoveToCenterOfScreen();
	UpdateFoFTimeLeft();
	m_fNextUpdateTime = gpGlobals->curtime + 1.0f;
}

using namespace vgui;

static void FoFSetScoreboardControlVisible( Panel *parent,
	const char *name, bool visible )
{
	Panel *control = parent ? parent->FindChildByName( name ) : NULL;
	if ( control )
		control->SetVisible( visible );
}

int CFoFScoreboardDialog::GetFoFMode() const
{
	ConVar *mode = cvar ? cvar->FindVar( "fof_sv_currentmode" ) : NULL;
	return mode ? mode->GetInt() : 0;
}

bool CFoFScoreboardDialog::IsFoFRankedTeamMode() const
{
	const int mode = GetFoFMode();
	// These are FoF four-team modes even when the generic mp_teamplay cvar
	// remains zero (which is valid for a Break Bad listen server).
	return mode == 3 || mode == 4;
}

int CFoFScoreboardDialog::GetBreakBadSectionFromTeamNumber(
	int teamNumber ) const
{
	if ( teamNumber < FOF_FIRST_PLAYING_TEAM ||
		teamNumber > FOF_LAST_PLAYING_TEAM )
	{
		return 0;
	}

	const int targetScore = g_PR
		? g_PR->GetTeamScore( teamNumber )
		: ( GetGlobalTeam( teamNumber )
			? GetGlobalTeam( teamNumber )->Get_Score() : 0 );
	int section = 1;
	for ( int team = FOF_FIRST_PLAYING_TEAM;
		team <= FOF_LAST_PLAYING_TEAM; ++team )
	{
		if ( team == teamNumber )
			continue;

		const int otherScore = g_PR
			? g_PR->GetTeamScore( team )
			: ( GetGlobalTeam( team )
				? GetGlobalTeam( team )->Get_Score() : 0 );
		if ( otherScore > targetScore ||
			( otherScore == targetScore && team < teamNumber ) )
		{
			++section;
		}
	}
	return clamp( section, 1, 4 );
}

void CFoFScoreboardDialog::AddFoFListSection(
	SectionedListPanel *list, int sectionID, bool includeCash )
{
	if ( !list )
		return;

	char sectionName[32];
	if ( sectionID == 0 )
		Q_strncpy( sectionName, "Players", sizeof( sectionName ) );
	else
		Q_snprintf( sectionName, sizeof( sectionName ),
			"Team%i", sectionID );
	list->AddSection( sectionID, sectionName, FoFPlayerSortFunc );
	list->SetSectionAlwaysVisible( sectionID );
	const bool modeTwo = GetFoFMode() == 2;
	list->SetSectionFgColor( sectionID, sectionID == 0
		? Color( 0, 0, 0, 0 )
		: Color( 205, 205, 205, 255 ) );

	const int badgeWidth = ( m_iAvatarWidth * 3 ) / 4;
	const int avatarWidth = ShowAvatars() ? m_iAvatarWidth : 0;
	int nameWidth = scheme()->GetProportionalScaledValueEx(
		GetScheme(), modeTwo ? 85 : 107 );
	int screenWide = 0;
	int screenTall = 0;
	surface()->GetScreenSize( screenWide, screenTall );
	const float widescreenAvatarFraction = clamp(
		( screenWide - 1280.0f ) / 640.0f, 0.0f, 1.0f );
	const int fixedAvatarHalfWide = m_iAvatarWidth / 2;
	nameWidth -= RoundFloatToInt(
		fixedAvatarHalfWide * ( 1.0f - widescreenAvatarFraction ) );
	const int expWidth = scheme()->GetProportionalScaledValueEx(
		GetScheme(), modeTwo ? 50 : 58 );
	const int fragWidth = scheme()->GetProportionalScaledValueEx(
		GetScheme(), modeTwo ? 50 : 70 );
	const int cashWidth = includeCash
		? scheme()->GetProportionalScaledValueEx( GetScheme(), 40 ) : 0;
	const int pingWidth = scheme()->GetProportionalScaledValueEx(
		GetScheme(), modeTwo ? 38 : 54 );
	const int voiceWidth = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 27 );
	HFont fallbackFont = scheme()->GetIScheme( GetScheme() )->GetFont(
		"DefaultVerySmallFallBack", false );

	list->AddColumnToSection( sectionID, "badge", "",
		SectionedListPanel::COLUMN_IMAGE |
		SectionedListPanel::COLUMN_RIGHT, badgeWidth );
	if ( ShowAvatars() )
	{
		list->AddColumnToSection( sectionID, "avatar", "",
			SectionedListPanel::COLUMN_IMAGE |
			SectionedListPanel::COLUMN_CENTER, avatarWidth );
	}
	list->AddColumnToSection( sectionID, "name", "", 0, nameWidth,
		fallbackFont );
	list->AddColumnToSection( sectionID, "exp", "",
		SectionedListPanel::COLUMN_RIGHT, expWidth );
	list->AddColumnToSection( sectionID, "frags", "",
		SectionedListPanel::COLUMN_RIGHT, fragWidth );
	if ( includeCash )
	{
		list->AddColumnToSection( sectionID, "cash", "",
			SectionedListPanel::COLUMN_RIGHT, cashWidth );
	}
	list->AddColumnToSection( sectionID, "ping", "",
		SectionedListPanel::COLUMN_RIGHT, pingWidth );
	list->AddColumnToSection( sectionID, "voice", "",
		SectionedListPanel::COLUMN_RIGHT, voiceWidth );
}

int CFoFScoreboardDialog::GetFoFDeathmatchListWide()
{
	// ScoreBoard.res authors PlayerListDM at 360 logical units.  The original
	// reads that value before the first autoresize pass.  Reading our live
	// control here feeds the previous mode's autoresized width back into the
	// next update and makes the panel grow every time it is reopened.
	return scheme()->GetProportionalScaledValueEx( GetScheme(), 360 );
}

void CFoFScoreboardDialog::ConfigureFoFListSections()
{
	// All three controls retain section zero. Break Bad and Elimination use
	// DM sections 1..4, one independent section per playing team.
	const int mode = GetFoFMode();
	const bool includeCash = mode == 2;
	const int lineSpacing = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 17 );
	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		if ( lists[i] )
			lists[i]->SetLineSpacing( lineSpacing );
	}

	AddFoFListSection( m_pPlayerListDM, 0, includeCash );
	AddFoFListSection( m_pPlayerListR, 0, includeCash );
	AddFoFListSection( m_pPlayerListC, 0, includeCash );
	if ( IsFoFRankedTeamMode() )
	{
		const bool keepEmptySections = mode == 3;
		for ( int list = 0; list < ARRAYSIZE( lists ); ++list )
		{
			for ( int section = 1; section <= 4; ++section )
			{
				AddFoFListSection( lists[list], section, false );
				lists[list]->SetSectionAlwaysVisible(
					section, keepEmptySections );
			}
		}
	}
}

void CFoFScoreboardDialog::FitFoFListLineSpacing()
{
	SectionedListPanel *lists[] =
		{ m_pPlayerListDM, m_pPlayerListR, m_pPlayerListC };
	const int maximumSpacing = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 17 );
	const int minimumSpacing = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 8 );
	int fittedSpacing = maximumSpacing;
	bool hasVisibleList = false;

	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		SectionedListPanel *list = lists[i];
		if ( !list || !list->IsVisible() )
			continue;

		hasVisibleList = true;
		list->SetVerticalScrollbar( false );
		if ( list->GetScrollBar() )
		{
			list->GetScrollBar()->SetValue( 0 );
			list->GetScrollBar()->SetVisible( false );
		}
	}

	if ( !hasVisibleList )
		return;

	// SectionedListPanel's content measurement includes every visible section
	// header, section gap and player row. Try the largest readable spacing
	// first, then tighten only as much as the current panel actually needs.
	for ( int spacing = maximumSpacing;
		spacing >= minimumSpacing; --spacing )
	{
		bool fits = true;
		for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
		{
			SectionedListPanel *list = lists[i];
			if ( !list || !list->IsVisible() )
				continue;

			list->SetLineSpacing( spacing );
			int contentWide = 0;
			int contentTall = 0;
			list->GetContentSize( contentWide, contentTall );
			if ( contentTall > list->GetTall() )
			{
				fits = false;
				break;
			}
		}

		if ( fits )
		{
			fittedSpacing = spacing;
			break;
		}
	}

	for ( int i = 0; i < ARRAYSIZE( lists ); ++i )
	{
		SectionedListPanel *list = lists[i];
		if ( !list || !list->IsVisible() )
			continue;

		list->SetLineSpacing( fittedSpacing );
		if ( list->GetScrollBar() )
		{
			list->GetScrollBar()->SetValue( 0 );
			list->GetScrollBar()->SetVisible( false );
		}
	}
}

void CFoFScoreboardDialog::UpdateFoFModeVisibility()
{
	const bool splitTeamLists = GetFoFMode() == 2;
	if ( m_pBackground )
		m_pBackground->SetImage(
			splitTeamLists ? "scoreboardbg_tp" : "scoreboardbg" );
	if ( m_pPlayerListDM )
		m_pPlayerListDM->SetVisible( !splitTeamLists );
	if ( m_pPlayerListR )
		m_pPlayerListR->SetVisible( splitTeamLists );
	if ( m_pPlayerListC )
		m_pPlayerListC->SetVisible( splitTeamLists );

	static const char *dmControls[] =
	{
		"DM_PlayerCount", "DM_ScoreHeader", "DM_NotorietyHeader",
		"DM_PingHeader"
	};
	static const char *teamControls[] =
	{
		"R_TeamScoreLabel", "C_TeamScoreLabel", "VerticalLine",
		"R_PlayerCount", "R_NotorietyHeader", "R_ScoreHeader",
		"R_CashHeader", "R_PingHeader", "C_PlayerCount",
		"C_NotorietyHeader", "C_ScoreHeader", "C_CashHeader",
		"C_PingHeader"
	};

	for ( int i = 0; i < ARRAYSIZE( dmControls ); ++i )
		FoFSetScoreboardControlVisible( this, dmControls[i],
			!splitTeamLists );
	for ( int i = 0; i < ARRAYSIZE( teamControls ); ++i )
		FoFSetScoreboardControlVisible( this, teamControls[i],
			splitTeamLists );
}

void CFoFScoreboardDialog::UpdateFoFTimeLeft()
{
	if ( !m_pTimeLeft )
		return;
	if ( FoFIsLocalCourseSession() )
	{
		m_pTimeLeft->SetVisible( false );
		return;
	}

	CHL2MPRules *rules = HL2MPRules();
	const float remaining =
		rules ? rules->GetFoFScoreboardTimeRemaining() : -1.0f;
	if ( remaining < 0.0f )
	{
		m_pTimeLeft->SetVisible( false );
		return;
	}

	const int totalSeconds = MAX( 0, (int)ceil( remaining ) );
	wchar_t minutes[16];
	wchar_t seconds[16];
	V_snwprintf( minutes, ARRAYSIZE( minutes ), L"%d",
		totalSeconds / 60 );
	V_snwprintf( seconds, ARRAYSIZE( seconds ), L"%02d",
		totalSeconds % 60 );

	wchar_t timeText[128];
	const wchar_t *format =
		g_pVGuiLocalize->Find( "#ScoreBoard_Timer" );
	if ( format )
	{
		g_pVGuiLocalize->ConstructString(
			timeText, sizeof( timeText ), format, 2,
			minutes, seconds );
	}
	else
	{
		V_snwprintf( timeText, ARRAYSIZE( timeText ),
			L"TIME LEFT %s:%s", minutes, seconds );
	}
	m_pTimeLeft->SetText( timeText );

	Panel *timerParent = m_pTimeLeft->GetParent();
	const int parentWide = timerParent
		? timerParent->GetWide() : ScreenWidth();
	int scoreboardX = 0;
	int scoreboardY = 0;
	GetPos( scoreboardX, scoreboardY );
	const int timerTall = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 20 );
	const int timerGap = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 20 );
	m_pTimeLeft->SetBounds(
		0,
		MAX( scoreboardY - timerGap, 0 ),
		parentWide,
		timerTall );
	m_pTimeLeft->SetVisible( IsVisible() );
}

void CFoFScoreboardDialog::LayoutFoFMuteHint()
{
	Label *hint = dynamic_cast<Label *>(
		FindChildByName( "MutePlayerHint" ) );
	if ( !hint || !m_pMuteButtonLeft || !m_pMuteButtonRight )
		return;

	IScheme *activeScheme = scheme()->GetIScheme( GetScheme() );
	if ( activeScheme )
		hint->SetFont( activeScheme->GetFont( "FoFSlideBig", false ) );
	hint->SetContentAlignment( Label::a_center );

	// In the original client the text itself is centred, 30
	// logical units high and sits 25 units above the DM root bottom (55 in
	// the split-team board).  Two 20-unit square state images touch its
	// sides and are vertically inset by five units.
	int textWide = 0;
	int textTall = 0;
	hint->GetContentSize( textWide, textTall );
	textWide = MAX( textWide, 1 );

	const int hintTall = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 30 );
	const int iconSize = scheme()->GetProportionalScaledValueEx(
		GetScheme(), 20 );
	const int bottomInset = scheme()->GetProportionalScaledValueEx(
		GetScheme(), GetFoFMode() == 2 ? 55 : 25 );
	const int textX = ( GetWide() - textWide ) / 2;
	const int textY = GetTall() - bottomInset;
	const int iconY = textY + ( hintTall - iconSize ) / 2;

	hint->SetBounds( textX, textY, textWide, hintTall );
	hint->SetFgColor( Color( 178, 240, 40, 255 ) );
	m_pMuteButtonLeft->SetBounds(
		textX - iconSize, iconY, iconSize, iconSize );
	m_pMuteButtonRight->SetBounds(
		textX + textWide, iconY, iconSize, iconSize );
}

void CFoFScoreboardDialog::InitScoreboardSections()
{
	ConfigureFoFListSections();
}

void CFoFScoreboardDialog::UpdateTeamInfo()
{
	if ( !g_PR )
		return;

	// Keep every resource placeholder backed by one persistent dialog
	// variable.  Mixing SetControlString with %variable% labels makes the
	// asynchronous direct text alternate with "[unknown]" whenever another
	// dialog variable is updated.
	SetDialogVariable( "server", m_szFoFServerName );

	int playablePlayers = 0;
	int spectators = 0;
	int teamPlayers[FOF_LAST_PLAYING_TEAM + 1] = { 0 };
	char spectatorNames[1024];
	spectatorNames[0] = '\0';

	for ( int player = 1; player <= gpGlobals->maxClients; ++player )
	{
		if ( !g_PR->IsConnected( player ) )
			continue;
		if ( !ShouldShowPlayer( player ) )
			continue;

		const int team = g_PR->GetTeam( player );
		if ( team != TEAM_SPECTATOR )
		{
			++playablePlayers;
			if ( team >= FOF_FIRST_PLAYING_TEAM &&
				team <= FOF_LAST_PLAYING_TEAM )
			{
				++teamPlayers[team];
			}
		}
		else if ( !g_PR->IsFakePlayer( player ) )
		{
			if ( spectators > 0 )
				V_strncat( spectatorNames, ", ",
					sizeof( spectatorNames ) );
			V_strncat( spectatorNames, g_PR->GetPlayerName( player ),
				sizeof( spectatorNames ) );
			++spectators;
		}
	}

	wchar_t playerCountValue[16];
	wchar_t playerCountText[96];
	V_snwprintf(
		playerCountValue,
		ARRAYSIZE( playerCountValue ),
		L"%d",
		playablePlayers );
	const wchar_t *playerCountFormat =
		g_pVGuiLocalize->Find( "#ScoreBoard_Players_FoF" );
	if ( playerCountFormat )
	{
		g_pVGuiLocalize->ConstructString(
			playerCountText,
			sizeof( playerCountText ),
			playerCountFormat,
			1,
			playerCountValue );
		SetDialogVariable( "dm_playercount", playerCountText );
	}
	else
	{
		SetDialogVariable( "dm_playercount", playablePlayers );
	}
	wchar_t desperadoCountValue[16];
	wchar_t desperadoCountText[96];
	wchar_t vigilanteCountValue[16];
	wchar_t vigilanteCountText[96];
	V_snwprintf( desperadoCountValue, ARRAYSIZE( desperadoCountValue ),
		L"%d", teamPlayers[3] );
	V_snwprintf( vigilanteCountValue, ARRAYSIZE( vigilanteCountValue ),
		L"%d", teamPlayers[2] );
	if ( playerCountFormat )
	{
		g_pVGuiLocalize->ConstructString(
			desperadoCountText,
			sizeof( desperadoCountText ),
			playerCountFormat,
			1,
			desperadoCountValue );
		g_pVGuiLocalize->ConstructString(
			vigilanteCountText,
			sizeof( vigilanteCountText ),
			playerCountFormat,
			1,
			vigilanteCountValue );
		SetDialogVariable( "r_teamplayercount", desperadoCountText );
		SetDialogVariable( "c_teamplayercount", vigilanteCountText );
	}
	else
	{
		SetDialogVariable( "r_teamplayercount", teamPlayers[3] );
		SetDialogVariable( "c_teamplayercount", teamPlayers[2] );
	}

	if ( GetFoFMode() == 2 )
	{
		wchar_t desperadoName[64];
		wchar_t vigilanteName[64];
		const char *desperadoNameAnsi = g_PR->GetTeamName( 3 );
		const char *vigilanteNameAnsi = g_PR->GetTeamName( 2 );
		g_pVGuiLocalize->ConvertANSIToUnicode(
			( desperadoNameAnsi && desperadoNameAnsi[0] )
				? desperadoNameAnsi : "Desperados",
			desperadoName,
			sizeof( desperadoName ) );
		g_pVGuiLocalize->ConvertANSIToUnicode(
			( vigilanteNameAnsi && vigilanteNameAnsi[0] )
				? vigilanteNameAnsi : "Vigilantes",
			vigilanteName,
			sizeof( vigilanteName ) );
		V_wcsupr( desperadoName );
		V_wcsupr( vigilanteName );

		wchar_t desperadoScore[96];
		wchar_t vigilanteScore[96];
		V_snwprintf( desperadoScore, ARRAYSIZE( desperadoScore ),
			L"%s %d", desperadoName, g_PR->GetTeamScore( 3 ) );
		V_snwprintf( vigilanteScore, ARRAYSIZE( vigilanteScore ),
			L"%d %s", g_PR->GetTeamScore( 2 ), vigilanteName );
		SetDialogVariable( "teamscore_desperados", desperadoScore );
		SetDialogVariable( "teamscore_vigilantes", vigilanteScore );
	}
	else
	{
		SetDialogVariable( "teamscore_desperados",
			g_PR->GetTeamScore( 3 ) );
		SetDialogVariable( "teamscore_vigilantes",
			g_PR->GetTeamScore( 2 ) );
	}
	if ( spectators > 0 )
	{
		wchar_t spectatorCount[16];
		wchar_t spectatorNamesWide[1024];
		wchar_t spectatorText[1200];
		V_snwprintf( spectatorCount, ARRAYSIZE( spectatorCount ),
			L"%d", spectators );
		g_pVGuiLocalize->ConvertANSIToUnicode(
			spectatorNames, spectatorNamesWide,
			sizeof( spectatorNamesWide ) );

		const wchar_t *spectatorFormat =
			g_pVGuiLocalize->Find( "#ScoreBoard_SpectatorFoF" );
		if ( spectatorFormat )
		{
			g_pVGuiLocalize->ConstructString(
				spectatorText,
				sizeof( spectatorText ),
				spectatorFormat,
				2,
				spectatorCount,
				spectatorNamesWide );
			SetDialogVariable( "spectators", spectatorText );
		}
		else
		{
			SetDialogVariable( "spectators", spectatorNamesWide );
		}
	}
	else
	{
		SetDialogVariable( "spectators", L" " );
	}
	Panel *spectatorLabel = FindChildByName( "Spectators" );
	if ( spectatorLabel )
		spectatorLabel->SetVisible( spectators > 0 );

	// ScoreBoard.res contains "%MutePlayerHint%". Supply the FoF-only dialog
	// variable so VGUI does not expand it to "[unknown]". The value uses the
	// uppercased fof_mutelist binding and the voice_enable ON/OFF token.
	char muteListKey[64];
	const char *boundKey = engine ?
		engine->Key_LookupBinding( "fof_mutelist" ) : NULL;
	Q_strncpy( muteListKey,
		( boundKey && boundKey[0] ) ? boundKey : "not bound",
		sizeof( muteListKey ) );
	Q_strupr( muteListKey );

	wchar_t muteListKeyWide[64];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		muteListKey, muteListKeyWide, sizeof( muteListKeyWide ) );

	ConVar *voiceEnable = cvar ? cvar->FindVar( "voice_enable" ) : NULL;
	const bool voiceEnabled = voiceEnable && voiceEnable->GetBool();
	const char *muteHintToken =
		voiceEnabled ?
			"#MutePlayerHintON" : "#MutePlayerHintOFF";
	const wchar_t *muteHintFormat =
		g_pVGuiLocalize->Find( muteHintToken );
	if ( muteHintFormat )
	{
		wchar_t muteHintText[256];
		g_pVGuiLocalize->ConstructString(
			muteHintText,
			sizeof( muteHintText ),
			muteHintFormat,
			1,
			muteListKeyWide );
		SetDialogVariable( "MutePlayerHint", muteHintText );
	}
	else
	{
		SetDialogVariable( "MutePlayerHint", L" " );
	}

	const char *muteButtonImage =
		voiceEnabled ? "mute_button_on" : "mute_button_off";
	if ( m_pMuteButtonLeft )
		m_pMuteButtonLeft->SetImage( muteButtonImage );
	if ( m_pMuteButtonRight )
		m_pMuteButtonRight->SetImage( muteButtonImage );

	if ( IsFoFRankedTeamMode() )
	{
		const int mode = GetFoFMode();
		for ( int section = 1; section <= 4; ++section )
		{
			// The shipped Break Bad board merges tied team totals. Preserve the
			// descending cash order, but use team number as a stable tiebreaker
			// so each of the four teams retains its own section.
			int score = 0;
			for ( int team = FOF_FIRST_PLAYING_TEAM;
				team <= FOF_LAST_PLAYING_TEAM; ++team )
			{
				score = g_PR->GetTeamScore( team );
				const int teamSection =
					GetBreakBadSectionFromTeamNumber( team );
				if ( teamSection == section )
					break;
			}

			wchar_t title[64];
			if ( mode == 3 )
				V_snwprintf( title, ARRAYSIZE( title ), L"$%i", score );
			else
				V_snwprintf( title, ARRAYSIZE( title ),
					L"Rounds Won: %i", score );
			m_pPlayerListDM->ModifyColumn( section, "name", title );
		}
	}
}

void CFoFScoreboardDialog::AddHeader()
{
	AddFoFListSection( m_pPlayerListDM, 0, false );
}

void CFoFScoreboardDialog::AddSection(
	int teamType, int teamNumber )
{
	if ( teamType != TYPE_TEAM )
		return;

	int sectionID = 0;
	const char *listName = NULL;
	SectionedListPanel *list = GetFoFListForTeam( teamNumber,
		sectionID, listName );
	if ( list )
		AddFoFListSection( list, sectionID, GetFoFMode() == 2 );
}

using namespace vgui;

SectionedListPanel *
CFoFScoreboardDialog::GetFoFListForTeam( int teamNumber,
	int &sectionID, const char *&listName ) const
{
	sectionID = 0;
	listName = "none";

	if ( teamNumber == TEAM_SPECTATOR )
		return NULL;

	if ( GetFoFMode() == 2 )
	{
		// Original Teamplay uses two independent tables: desperados on the
		// left and vigilantes on the right.  Teams 4/5 are intentionally not
		// routed in this two-team mode.
		if ( teamNumber == 3 )
		{
			listName = "R";
			return m_pPlayerListR;
		}
		if ( teamNumber == 2 )
		{
			listName = "C";
			return m_pPlayerListC;
		}
		return NULL;
	}

	listName = "DM";
	if ( IsFoFRankedTeamMode() )
	{
		// Keep all four teams in separate sections. The section helper sorts
		// by team score/cash and resolves ties by the stable team number.
		sectionID = GetBreakBadSectionFromTeamNumber( teamNumber );
	}
	return m_pPlayerListDM;
}

int CFoFScoreboardDialog::GetSectionFromTeamNumber(
	int teamNumber )
{
	int sectionID = 0;
	const char *listName = NULL;
	GetFoFListForTeam( teamNumber, sectionID, listName );
	return sectionID;
}

bool CFoFScoreboardDialog::ShouldShowPlayer( int playerIndex ) const
{
	const bool bCourse = GetFoFMode() == 6 || FoFIsLocalCourseSession();
	return !bCourse || !g_PR->IsFakePlayer( playerIndex );
}

Color CFoFScoreboardDialog::GetFoFPlayerColor(
	int playerIndex ) const
{
	if ( !g_PR )
		return Color( 205, 205, 205, 255 );

	ConVar *modeVar = cvar ? cvar->FindVar( "fof_sv_currentmode" ) : NULL;
	ConVar *forceSpectatorVar =
		cvar ? cvar->FindVar( "fof_sv_force_spect" ) : NULL;
	const bool preserveTeamColorWhenDead =
		modeVar && modeVar->GetInt() == 1 &&
		( !forceSpectatorVar || !forceSpectatorVar->GetBool() );

	if ( !preserveTeamColorWhenDead && !g_PR->IsAlive( playerIndex ) )
		return Color( 128, 128, 128, 255 );

	const int team = g_PR->GetTeam( playerIndex );
	int mappedTeam = 0;
	if ( team >= FOF_FIRST_PLAYING_TEAM &&
		team <= FOF_LAST_PLAYING_TEAM )
	{
		char variableName[64];
		Q_snprintf( variableName, sizeof( variableName ),
			"fof_sv_team_remap_%i", team - 1 );
		ConVar *remap = cvar ? cvar->FindVar( variableName ) : NULL;
		mappedTeam = remap ? remap->GetInt() : team;
	}
	return g_PR->GetTeamColor( mappedTeam );
}

bool CFoFScoreboardDialog::FoFPlayerSortFunc(
	SectionedListPanel *list, int itemID1, int itemID2 )
{
	KeyValues *first = list->GetItemData( itemID1 );
	KeyValues *second = list->GetItemData( itemID2 );
	if ( !first || !second )
		return itemID1 < itemID2;

	ConVar *modeVar = cvar ? cvar->FindVar( "fof_sv_currentmode" ) : NULL;
	const int mode = modeVar ? modeVar->GetInt() : 0;
	if ( mode == 3 )
	{
		const int firstTeamScore = first->GetInt( "teamscore" );
		const int secondTeamScore = second->GetInt( "teamscore" );
		if ( firstTeamScore != secondTeamScore )
			return firstTeamScore > secondTeamScore;
	}

	const int firstExp = first->GetInt( "exp" );
	const int secondExp = second->GetInt( "exp" );
	if ( firstExp != secondExp )
		return firstExp > secondExp;

	if ( mode == 3 )
	{
		const int firstCash = first->GetInt( "cash" );
		const int secondCash = second->GetInt( "cash" );
		if ( firstCash != secondCash )
			return firstCash > secondCash;
	}

	const int firstFrags = first->GetInt( "frags" );
	const int secondFrags = second->GetInt( "frags" );
	if ( firstFrags != secondFrags )
		return firstFrags > secondFrags;

	const int firstPlayer = first->GetInt( "playerIndex" );
	const int secondPlayer = second->GetInt( "playerIndex" );
	if ( firstPlayer != secondPlayer )
		return firstPlayer > secondPlayer;
	return itemID1 < itemID2;
}

bool CFoFScoreboardDialog::GetPlayerScoreInfo(
	int playerIndex, KeyValues *data )
{
	if ( !g_PR || !data )
		return false;

	const int team = g_PR->GetTeam( playerIndex );
	C_BasePlayer *player = UTIL_PlayerByIndex( playerIndex );
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	int cash = 0;
	if ( GetFoFMode() == 2 && player && localPlayer &&
		( localPlayer->GetTeamNumber() == team ||
		  localPlayer->GetTeamNumber() == TEAM_SPECTATOR ) )
	{
		cash = (int)FoFCash( player );
	}

	data->SetInt( "playerIndex", playerIndex );
	data->SetInt( "team", team );
	data->SetString( "name", g_PR->GetPlayerName( playerIndex ) );
	data->SetInt( "exp", g_PR->GetFoFExp( playerIndex ) );
	data->SetInt( "frags", g_PR->GetPlayerScore( playerIndex ) );
	data->SetInt( "deaths", g_PR->GetDeaths( playerIndex ) );
	data->SetInt( "cash", cash );
	data->SetInt( "teamscore", g_PR->GetTeamScore( team ) );
	data->SetInt( "badge", GetFoFBadgeImageIndex( playerIndex ) );
	if ( GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		data->SetString( "voice", "Muted" );
	else if ( GetClientVoiceMgr()->IsPlayerSpeaking( playerIndex ) )
		data->SetString( "voice", ")(" );
	else
		data->SetString( "voice", "" );

	const int ping = g_PR->GetPing( playerIndex );
	if ( ping < 1 )
		data->SetString( "ping",
			g_PR->IsFakePlayer( playerIndex ) ? "BOT" : "" );
	else
		data->SetInt( "ping", ping );

	if ( m_pImageList )
		UpdatePlayerAvatar( playerIndex, data );
	return true;
}

int CFoFScoreboardDialog::GetFoFBadgeImageIndex( int playerIndex ) const
{
	if ( !g_PR )
		return 0;

	if ( g_PR->IsFakePlayer( playerIndex ) )
	{
		const int botBadge =
			m_iFoFBadgeImages[FOF_SCOREBOARD_BADGE_BOT];
		return botBadge >= 0 ? botBadge : 0;
	}

	struct FoFScoreboardBadgeState_t
	{
		int stateMask;
		FoFScoreboardBadgeImage_t image;
	};
	static const FoFScoreboardBadgeState_t badgeStates[] =
	{
		{ 0x00001, FOF_SCOREBOARD_BADGE_DEV },
		{ 0x00002, FOF_SCOREBOARD_BADGE_CONTRIB },
		{ 0x00004, FOF_SCOREBOARD_BADGE_SPECIAL },
		{ 0x00008, FOF_SCOREBOARD_BADGE_L1 },
		{ 0x00010, FOF_SCOREBOARD_BADGE_L2 },
		{ 0x00020, FOF_SCOREBOARD_BADGE_L3 },
		{ 0x00040, FOF_SCOREBOARD_BADGE_L4 },
		{ 0x00200, FOF_SCOREBOARD_BADGE_ELM_TOP10 },
		{ 0x00400, FOF_SCOREBOARD_BADGE_ELM_TOP50 },
		{ 0x00800, FOF_SCOREBOARD_BADGE_ELM_TOP100 },
		{ 0x01000, FOF_SCOREBOARD_BADGE_FIRST },
		{ 0x02000, FOF_SCOREBOARD_BADGE_SECOND },
		{ 0x04000, FOF_SCOREBOARD_BADGE_THIRD },
		{ 0x10000, FOF_SCOREBOARD_BADGE_ELM_TOP1 }
	};

	const int state = g_PR->GetFoFState( playerIndex );
	for ( int i = 0; i < ARRAYSIZE( badgeStates ); ++i )
	{
		if ( !( state & badgeStates[i].stateMask ) )
			continue;

		const int badge = m_iFoFBadgeImages[badgeStates[i].image];
		return badge >= 0 ? badge : 0;
	}

	return 0;
}

void CFoFScoreboardDialog::UpdatePlayerInfo()
{
	if ( !g_PR )
		return;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	SectionedListPanel *selectedList = NULL;
	int selectedItem = -1;

	for ( int playerIndex = 1; playerIndex <= gpGlobals->maxClients;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) )
			continue;
		if ( !ShouldShowPlayer( playerIndex ) )
			continue;

		int sectionID = 0;
		const char *listName = NULL;
		SectionedListPanel *list = GetFoFListForTeam(
			g_PR->GetTeam( playerIndex ), sectionID, listName );
		if ( !list )
			continue;

		KeyValues *data = new KeyValues( "data" );
		if ( !GetPlayerScoreInfo( playerIndex, data ) )
		{
			data->deleteThis();
			continue;
		}

		char safeName[MAX_PLAYER_NAME_LENGTH];
		UTIL_MakeSafeName( data->GetString( "name", "" ), safeName,
			sizeof( safeName ) );
		data->SetString( "name", safeName );

		const int itemID = list->AddItem( sectionID, data );
		list->SetItemFgColor( itemID, GetFoFPlayerColor( playerIndex ) );
		if ( localPlayer && localPlayer->entindex() == playerIndex )
		{
			selectedList = list;
			selectedItem = itemID;
		}
		data->deleteThis();
	}

	if ( selectedList && selectedItem >= 0 )
		selectedList->SetSelectedItem( selectedItem );
}
