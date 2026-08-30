#include "cbase.h"
#include "fof/fof_launcher_panel.h"
#include "fof/fof_launcher_personal_stats.h"
#include "fof/fof_launcher_workshop.h"
#include "fof/fof_client_settings.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "KeyValues.h"
#include "steam/steam_api.h"
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/ComboBox.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoFServersPanel *g_pFoFServersPanel = NULL;

static ConVar fof_ping_limit(
	"fof_ping_limit",
	"200",
	FCVAR_ARCHIVE,
	"Maximum ping shown by the FoF launcher server list." );

static void FoFAddPingFilterItem(
	vgui::ComboBox *pCombo,
	const char *pszLabel,
	int nIndex )
{
	KeyValues *pData = new KeyValues( "data" );
	pData->SetInt( "ping_index", nIndex );
	pData->SetInt( "ping", FoFLauncherPingLimitFromIndex( nIndex ) );
	pCombo->AddItem( pszLabel, pData );
	pData->deleteThis();
}

CFoFServersPanel::CFoFServersPanel()
	: BaseClass( NULL, "FoFServersPanel" )
	, m_pSinglePlayer( NULL )
	, m_pTeams( NULL )
	, m_pCoop( NULL )
	, m_pRanked( NULL )
	, m_pFriends( NULL )
	, m_pFindServers( NULL )
	, m_pBrowserWarning( NULL )
	, m_pSearch( NULL )
	, m_pCourseRefresh( NULL )
	, m_pRefresh( NULL )
	, m_pPingDrop( NULL )
	, m_pRankedPanel( NULL )
	, m_pWorkshopPanel( NULL )
	, m_pStatsPanel( NULL )
	, m_hInternetRequest( NULL )
	, m_hServerRulesRequest( INVALID_HTTPREQUEST_HANDLE )
	, m_iOnlinePlayers( -1 )
	, m_iFullServers( 0 )
	, m_iFriendsPlaying( 0 )
	, m_iPingLimit( FoFLauncherPingLimitFromIndex(
		FoFLauncherPingLimitIndexFromValue(
			fof_ping_limit.GetInt() ) ) )
	, m_iPage( FOF_LAUNCHER_PAGE_SINGLEPLAYER )
	, m_bLauncherVisible( false )
	, m_bServerRuleWhitelisting( false )
	, m_bServerRulesRequested( false )
	, m_iBackgroundWide( -1 )
	, m_iBackgroundFourThree( -1 )
	, m_hSectionFont( vgui::INVALID_FONT )
	, m_hSmallFont( vgui::INVALID_FONT )
	, m_iTopTall( 32 )
	, m_iBodyY( 19 )
	, m_iLeftWide( 128 )
	, m_iHeaderTall( 20 )
	, m_iFooterTall( 15 )
	, m_iCourseCardTall( 65 )
	, m_iCourseGroupGap( 2 )
	, m_flNextWesternPassUpdate( 0.0f )
	, m_nWesternPassProgress( 0 )
{
	m_szLastSearch[0] = '\0';
	for ( int group = 0; group < FOF_LAUNCHER_MAX_GROUPS; ++group )
	{
		m_iGroupScroll[group] = 0;
		m_pGroupPrevious[group] = NULL;
		m_pGroupNext[group] = NULL;
	}

	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetPostChildPaintEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	MakePopup( false );

	m_iBackgroundWide = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iBackgroundWide, "vgui/main_menu_bg", true, false );
	m_iBackgroundFourThree = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iBackgroundFourThree, "vgui/main_menu_bg_43", true, false );

	m_pSinglePlayer = new CFoFLauncherButton(
		this, "SPButton", "#LaunchMenu_Singleplayer", this, "sp" );
	m_pTeams = new CFoFLauncherButton(
		this, "TeamsButton", "#LaunchMenu_Teams", this, "teams" );
	m_pCoop = new CFoFLauncherButton(
		this, "CoopButton", "#LaunchMenu_Coop", this, "coop" );
	m_pRanked = new CFoFLauncherButton(
		this, "RankedButton", "#LaunchMenu_Ranked", this, "ranked" );
	m_pFriends = new CFoFLauncherButton(
		this, "FriendsButton", "#FoF_FriendName", this, "friends" );
	m_pFindServers = new CFoFLauncherButton(
		this,
		"stock_browser",
		"#GameUI_GameMenu_FindServers",
		this,
		"stock_browser" );
	m_pBrowserWarning = new CFoFBrowserWarningButton(
		this, "warning", this, "warning_accept" );
	m_pSinglePlayer->SetSelected( true );
	// FoF retains the complete Ranked Shootout page and command route,
	// but deliberately hides its retired navigation entry.
	m_pRanked->SetVisible( false );

	m_pSearch = new CFoFSearchEntry(
		this, "SearchBar", "#FoF.SearchBar" );
	m_pSearch->SetMultiline( false );
	m_pSearch->SetEditable( true );
	m_pSearch->SetMouseInputEnabled( true );
	m_pSearch->SetKeyBoardInputEnabled( true );
	m_pSearch->SetMaximumCharCount( 80 );
	m_pCourseRefresh = new CFoFRefreshButton(
		this, "CourseRefreshButton", this, "course_refresh", true );
	m_pRefresh = new CFoFRefreshButton(
		this, "RefreshButton", this, "refresh", false );
	m_pPingDrop = new CFoFPingComboBox(
		this, "Ping Filter", 7 );
	FoFAddPingFilterItem( m_pPingDrop, "< 75ms", 0 );
	FoFAddPingFilterItem( m_pPingDrop, "< 100ms", 1 );
	FoFAddPingFilterItem( m_pPingDrop, "< 125ms", 2 );
	FoFAddPingFilterItem( m_pPingDrop, "< 150ms", 3 );
	FoFAddPingFilterItem( m_pPingDrop, "< 175ms", 4 );
	FoFAddPingFilterItem( m_pPingDrop, "< 200ms", 5 );
	m_pPingDrop->AddActionSignalTarget( this );
	m_pPingDrop->SetOpenDirection( vgui::Menu::UP );
	int pingRow = FoFLauncherPingLimitIndexFromValue( m_iPingLimit );
	pingRow = clamp( pingRow, 0, m_pPingDrop->GetItemCount() - 1 );
	m_pPingDrop->SilentActivateItemByRow( pingRow );

	for ( int group = 0; group < FOF_LAUNCHER_MAX_GROUPS; ++group )
	{
		char name[32];
		char command[32];
		Q_snprintf( name, sizeof( name ), "CoursePrevious%d", group );
		Q_snprintf( command, sizeof( command ), "previous:%d", group );
		m_pGroupPrevious[group] = new CFoFLauncherPageButton(
			this, name, "<", this, command );
		m_pGroupPrevious[group]->SetZPos( 100 );
		Q_snprintf( name, sizeof( name ), "CourseNext%d", group );
		Q_snprintf( command, sizeof( command ), "next:%d", group );
		m_pGroupNext[group] = new CFoFLauncherPageButton(
			this, name, ">", this, command );
		m_pGroupNext[group]->SetZPos( 100 );
	}

	m_pRankedPanel = new CFoFRankedPanel( this );
	m_pWorkshopPanel = new CFoFWorkshopPanel( this );
	m_pStatsPanel = new CFoFPersonalStatsPanel();
	RequestServerRules();

	LoadTopBar();
	LoadCourses();
	vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );
	SetVisible( false );
}

CFoFServersPanel::~CFoFServersPanel()
{
	ReleaseServerRulesRequest();
	ReleaseInternetRequest();
	ClearServerRules();
	ClearServerEntries();
	ClearCourseEntries();
	vgui::ivgui()->RemoveTickSignal( GetVPanel() );
	if ( m_iBackgroundWide >= 0 )
		vgui::surface()->DestroyTextureID( m_iBackgroundWide );
	if ( m_iBackgroundFourThree >= 0 )
		vgui::surface()->DestroyTextureID( m_iBackgroundFourThree );
}

void CFoFServersPanel::OpenLauncher()
{
	SetVisible( true );
	MoveToFront();
	InvalidateLayout( true );
	Repaint();
}

void CFoFServersPanel::OpenWorkshop()
{
	OpenLauncher();
	if ( m_pStatsPanel )
		m_pStatsPanel->Close();
	if ( m_pWorkshopPanel )
		m_pWorkshopPanel->Open();
}

void CFoFServersPanel::OpenStats()
{
	OpenLauncher();
	if ( m_pWorkshopPanel )
		m_pWorkshopPanel->Close();
	if ( m_pStatsPanel )
		m_pStatsPanel->Open();
}

void CFoFServersPanel::OnTick()
{
	// The shipped ServersPanel.res keeps this child visible and relies on its
	// PANEL_GAMEUIDLL parent for the actual GameUI lifetime.  Mirror that
	// behavior here: filtering normal in-game clients hides the launcher's
	// right-hand content after opening the menu from a listen or remote server.
	const bool visible = enginevgui && enginevgui->IsGameUIVisible();
	if ( IsVisible() != visible )
		SetVisible( visible );
	if ( visible && !m_bServerRulesRequested )
		RequestServerRules();
	if ( visible && !m_bLauncherVisible )
	{
		RefreshSocialData();
		ReloadCourseStats();
	}
	m_bLauncherVisible = visible;
	if ( !visible )
	{
		if ( m_pStatsPanel )
			m_pStatsPanel->Close();
		return;
	}

	// config.cfg may be executed after this panel is constructed.  Keep the
	// cached filter and its row synchronized with the archived FoF ConVar.
	const int nConfiguredPingLimit = FoFLauncherPingLimitFromIndex(
		FoFLauncherPingLimitIndexFromValue( fof_ping_limit.GetInt() ) );
	if ( m_iPingLimit != nConfiguredPingLimit )
	{
		m_iPingLimit = nConfiguredPingLimit;
		const int nPingRow = clamp(
			FoFLauncherPingLimitIndexFromValue( m_iPingLimit ),
			0,
			m_pPingDrop->GetItemCount() - 1 );
		m_pPingDrop->SilentActivateItemByRow( nPingRow );
		for ( int group = 0; group < FOF_LAUNCHER_MAX_GROUPS; ++group )
			m_iGroupScroll[group] = 0;
		RecountServerTotals();
		InvalidateLayout();
		Repaint();
	}

	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int x = MAX( RoundFloatToInt( screenWide * 0.05f ), 24 );
	const int y = MAX( RoundFloatToInt( screenTall * 0.0104f ), 4 );
	const int rightMargin = MAX(
		RoundFloatToInt( screenWide * 0.0303f ), 16 );
	const int bottomMargin = MAX(
		RoundFloatToInt( screenTall * 0.03125f ), 12 );
	const int wide = MAX( screenWide - x - rightMargin, 320 );
	const int tall = MAX( screenTall - y - bottomMargin, 240 );
	int oldX = 0;
	int oldY = 0;
	int oldWide = 0;
	int oldTall = 0;
	GetBounds( oldX, oldY, oldWide, oldTall );
	if ( oldX != x || oldY != y || oldWide != wide || oldTall != tall )
	{
		SetBounds( x, y, wide, tall );
		InvalidateLayout( true );
	}

	char search[128];
	m_pSearch->GetText( search, sizeof( search ) );
	if ( Q_stricmp( search, m_szLastSearch ) )
	{
		Q_strncpy( m_szLastSearch, search, sizeof( m_szLastSearch ) );
		for ( int group = 0;
			group < FOF_LAUNCHER_MAX_GROUPS;
			++group )
		{
			m_iGroupScroll[group] = 0;
		}
		RecountServerTotals();
		InvalidateLayout();
	}

	// Both the launcher and the statistics window are GameUI popups.  The
	// launcher repaints on its polling tick, so restore the later popup to the
	// front after that work; otherwise only the part outside the launcher is
	// visible after the first statistics-button click.
	if ( m_pStatsPanel && m_pStatsPanel->IsOpen() )
		m_pStatsPanel->MoveToFront();
}

void CFoFServersPanel::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "sp" ) )
	{
		SelectPage( FOF_LAUNCHER_PAGE_SINGLEPLAYER );
		return;
	}
	if ( !Q_stricmp( pszCommand, "course_refresh" ) )
	{
		ReloadCourseStats();
		for ( int group = 0;
			group < FOF_LAUNCHER_MAX_GROUPS;
			++group )
		{
			m_iGroupScroll[group] = 0;
		}
		if ( m_iPage != FOF_LAUNCHER_PAGE_SINGLEPLAYER )
			RefreshSocialData();
		InvalidateLayout();
		Repaint();
		return;
	}
	if ( !Q_stricmp( pszCommand, "workshop_content_installed" ) )
	{
		ClearCourseEntries();
		LoadCourses();
		for ( int group = 0;
			group < FOF_LAUNCHER_MAX_GROUPS;
			++group )
		{
			m_iGroupScroll[group] = 0;
		}
		RecountServerTotals();
		InvalidateLayout();
		Repaint();
		return;
	}
	if ( !Q_stricmp( pszCommand, "refresh" ) )
	{
		RefreshSocialData();
		InvalidateLayout();
		Repaint();
		return;
	}
	if ( !Q_stricmp( pszCommand, "teams" ) )
	{
		SelectPage( FOF_LAUNCHER_PAGE_TEAMS );
		return;
	}
	if ( !Q_stricmp( pszCommand, "coop" ) )
	{
		SelectPage( FOF_LAUNCHER_PAGE_COOP );
		return;
	}
	if ( !Q_stricmp( pszCommand, "ranked" ) )
	{
		SelectPage( FOF_LAUNCHER_PAGE_RANKED );
		return;
	}
	if ( !Q_stricmp( pszCommand, "friends" ) )
	{
		SelectPage( FOF_LAUNCHER_PAGE_FRIENDS );
		return;
	}
	if ( !Q_stricmp( pszCommand, "warning_accept" ) )
	{
		ConVarRef browserWarning( "fof_browserwarning", true );
		if ( browserWarning.IsValid() )
			browserWarning.SetValue( 1 );
		engine->ClientCmd_Unrestricted(
			"gamemenucommand openserverbrowser\n" );
		m_pBrowserWarning->DismissWarning();
		return;
	}
	if ( !Q_stricmp( pszCommand, "stock_browser" ) ||
		!Q_stricmp( pszCommand, "findservers" ) )
	{
		ConVarRef browserWarning( "fof_browserwarning", true );
		if ( !browserWarning.IsValid() || browserWarning.GetBool() )
		{
			engine->ClientCmd_Unrestricted(
				"gamemenucommand openserverbrowser\n" );
		}
		else
		{
			m_pBrowserWarning->BeginWarning();
		}
		return;
	}
	if ( !Q_strnicmp( pszCommand, "course:", 7 ) )
	{
		LaunchCourse( atoi( pszCommand + 7 ) );
		return;
	}
	if ( !Q_strnicmp( pszCommand, "server:", 7 ) )
	{
		JoinServer( atoi( pszCommand + 7 ) );
		return;
	}
	if ( !Q_strnicmp( pszCommand, "previous:", 9 ) )
	{
		const int group = atoi( pszCommand + 9 );
		if ( group >= 0 && group < FOF_LAUNCHER_MAX_GROUPS &&
			m_iGroupScroll[group] > 0 )
		{
			--m_iGroupScroll[group];
			InvalidateLayout( true );
			Repaint();
		}
		return;
	}
	if ( !Q_strnicmp( pszCommand, "next:", 5 ) )
	{
		const int group = atoi( pszCommand + 5 );
		if ( group >= 0 && group < FOF_LAUNCHER_MAX_GROUPS )
		{
			++m_iGroupScroll[group];
			InvalidateLayout( true );
			Repaint();
		}
		return;
	}
	if ( !Q_strnicmp( pszCommand, "top:", 4 ) )
	{
		ActivateTopItem( atoi( pszCommand + 4 ) );
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFServersPanel::OnTextChanged( KeyValues *pData )
{
	if ( !pData || pData->GetPtr( "panel" ) != m_pPingDrop )
		return;

	KeyValues *pItem = m_pPingDrop->GetActiveItemUserData();
	if ( !pItem )
		return;

	const int nPingLimitIndex = clamp(
		pItem->GetInt( "ping_index", 5 ), 0, 5 );
	const int pingLimit = FoFLauncherPingLimitFromIndex( nPingLimitIndex );
	if ( pingLimit == m_iPingLimit )
		return;

	m_iPingLimit = pingLimit;
	fof_ping_limit.SetValue( pingLimit );
	for ( int group = 0; group < FOF_LAUNCHER_MAX_GROUPS; ++group )
		m_iGroupScroll[group] = 0;
	RecountServerTotals();
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::SelectPage( int page )
{
	m_iPage = clamp(
		page,
		(int)FOF_LAUNCHER_PAGE_SINGLEPLAYER,
		(int)FOF_LAUNCHER_PAGE_FRIENDS );
	m_pSinglePlayer->SetSelected(
		m_iPage == FOF_LAUNCHER_PAGE_SINGLEPLAYER );
	m_pTeams->SetSelected(
		m_iPage == FOF_LAUNCHER_PAGE_TEAMS );
	m_pCoop->SetSelected(
		m_iPage == FOF_LAUNCHER_PAGE_COOP );
	m_pRanked->SetSelected(
		m_iPage == FOF_LAUNCHER_PAGE_RANKED );
	m_pFriends->SetSelected(
		m_iPage == FOF_LAUNCHER_PAGE_FRIENDS );
	if ( m_iPage == FOF_LAUNCHER_PAGE_RANKED )
		m_pRankedPanel->Open();
	else
		m_pRankedPanel->Close();
	for ( int group = 0;
		group < FOF_LAUNCHER_MAX_GROUPS;
		++group )
	{
		m_iGroupScroll[group] = 0;
	}
	RecountServerTotals();
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::LoadTopBar()
{
	KeyValues *pTopBar = new KeyValues( "TopBar" );
	if ( pTopBar->LoadFromFile(
		filesystem, "fof_scripts/top_bar_items.txt", "GAME" ) )
	{
		int index = 0;
		for ( KeyValues *pItem = pTopBar->GetFirstTrueSubKey();
			pItem;
			pItem = pItem->GetNextTrueSubKey() )
		{
			const char *pTitle = pItem->GetString( "title", "" );
			const char *pCommand = pItem->GetString( "cmd", "" );
			if ( !pTitle[0] || !pCommand[0] )
				continue;
			char name[32];
			char command[32];
			Q_snprintf( name, sizeof( name ), "TopBarItem%d", index );
			Q_snprintf( command, sizeof( command ), "top:%d", index );
			CFoFTopBarButton *pButton = new CFoFTopBarButton(
				this,
				name,
				pTitle,
				pItem->GetString( "icon", "" ),
				this,
				command );
			pButton->SetBlink( index == 0 );
			m_TopButtons.AddToTail( pButton );
			m_TopCommands.AddToTail( CUtlString( pCommand ) );
			++index;
		}
	}
	pTopBar->deleteThis();
}

void CFoFServersPanel::ActivateTopItem( int index )
{
	if ( index < 0 || index >= m_TopCommands.Count() )
		return;
	const char *pCommand = m_TopCommands[index].String();
	if ( Q_stricmp( pCommand, "workshop" ) )
		m_pWorkshopPanel->Close();
	if ( Q_stricmp( pCommand, "stats" ) )
		m_pStatsPanel->Close();
	if ( !Q_strnicmp( pCommand, "http://", 7 ) ||
		!Q_strnicmp( pCommand, "https://", 8 ) )
	{
		ISteamFriends *pFriends = steamapicontext ?
			steamapicontext->SteamFriends() : NULL;
		if ( pFriends )
			pFriends->ActivateGameOverlayToWebPage( pCommand );
	}
	else if ( !Q_stricmp( pCommand, "workshop" ) )
	{
		m_pWorkshopPanel->Open();
	}
	else if ( !Q_stricmp( pCommand, "stats" ) )
	{
		m_pStatsPanel->Open();
	}
}

void FoFCreateLauncherPanel()
{
	if ( !g_pFoFServersPanel )
		g_pFoFServersPanel = new CFoFServersPanel();
}

CON_COMMAND( OpenFoFServerPanel,
	"Open the Fistful of Frags launcher server panel." )
{
	FoFCreateLauncherPanel();
	if ( g_pFoFServersPanel )
		g_pFoFServersPanel->OpenLauncher();
}

CON_COMMAND( OpenWSDialog,
	"Open the Fistful of Frags Workshop panel." )
{
	FoFCreateLauncherPanel();
	if ( g_pFoFServersPanel )
		g_pFoFServersPanel->OpenWorkshop();
}

CON_COMMAND( OpenStatsDialog,
	"Open the Fistful of Frags personal statistics panel." )
{
	FoFCreateLauncherPanel();
	if ( g_pFoFServersPanel )
		g_pFoFServersPanel->OpenStats();
}

static bool FoFShouldShowWesternPass()
{
	ISteamUser *pSteamUser = steamapicontext ?
		steamapicontext->SteamUser() : NULL;
	return !pSteamUser || pSteamUser->GetPlayerSteamLevel() <= 0;
}

static int FoFReadWesternPassProgress()
{
	ISteamUser *pSteamUser = steamapicontext ?
		steamapicontext->SteamUser() : NULL;
	if ( !pSteamUser || !filesystem )
		return 0;

	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "fof_scripts/saves/%u.txt",
		pSteamUser->GetSteamID().GetAccountID() );

	KeyValues *pRecords = new KeyValues( "Records" );
	if ( !pRecords->LoadFromFile( filesystem, path, "MOD" ) )
	{
		pRecords->deleteThis();
		return 0;
	}

	char encoded[32];
	Q_strncpy( encoded, pRecords->GetString( "value", "*" ),
		sizeof( encoded ) );
	pRecords->deleteThis();

	for ( int i = 0; i < ARRAYSIZE( encoded ) && encoded[i]; ++i )
	{
		switch ( encoded[i] )
		{
		case 'h': encoded[i] = '7'; break;
		case 'l': encoded[i] = '2'; break;
		case '$': encoded[i] = '4'; break;
		case 'b': encoded[i] = '9'; break;
		case 'w': encoded[i] = '1'; break;
		case 'u': encoded[i] = '3'; break;
		case '-': encoded[i] = '5'; break;
		case 'k': encoded[i] = '0'; break;
		case '#': encoded[i] = '8'; break;
		case 't': encoded[i] = '6'; break;
		default: break;
		}
	}

	return clamp( Q_atoi( encoded ), 0, 10000 );
}

static float FoFLauncherHorizontalAspectScale(
	int screenWide,
	int screenTall )
{
	// FoF buckets the integer display aspect ratio, then uses the
	// result to convert the launcher's 640-wide coordinates.  Reproducing
	// that slightly unusual calculation matters on 16:9 displays: scaling
	// these controls from the 480-line height makes the search field much
	// too short.
	const int aspectBucket = screenTall > 0 ?
		screenWide / screenTall : 1;
	const float aspectBlend = clamp(
		( (float)aspectBucket - 1.33f ) * 0.9523809f,
		0.0f,
		1.0f );
	return 1.35f + aspectBlend * 0.5f;
}

static int FoFLauncherScaleHorizontalPixel(
	float logicalPixels,
	int screenWide,
	int screenTall )
{
	const float aspectScale = FoFLauncherHorizontalAspectScale(
		screenWide, screenTall );
	return RoundFloatToInt(
		logicalPixels * (float)screenWide /
		( aspectScale * 640.0f ) );
}

void CFoFServersPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hSectionFont = pFoFScheme ?
		pFoFScheme->GetFont( "NotorietyFont", false ) :
		vgui::INVALID_FONT;
	m_hSmallFont = pFoFScheme ?
		pFoFScheme->GetFont( "Default", false ) :
		vgui::INVALID_FONT;
	if ( m_hSectionFont == vgui::INVALID_FONT )
		m_hSectionFont = pScheme->GetFont( "DefaultSmall", false );
	if ( m_hSmallFont == vgui::INVALID_FONT )
		m_hSmallFont = pScheme->GetFont( "Default", false );
	m_pSearch->SetFont( m_hSmallFont );
	m_pSearch->SetFgColor( Color( 220, 220, 215, 255 ) );
	// The shipped client explicitly overrides SourceScheme's translucent
	// TextEntry background with packed colour 0xFF1E1E1E.
	m_pSearch->SetBgColor( Color( 30, 30, 30, 255 ) );
	m_pSearch->SetPaintBackgroundEnabled( true );
	m_pSearch->SetPaintBorderEnabled( true );
	m_pSearch->SetHintStyle(
		m_hSmallFont, Color( 220, 220, 215, 255 ) );
	m_pPingDrop->SetFont( m_hSmallFont );
	m_pPingDrop->SetFgColor( Color( 196, 191, 180, 255 ) );
	m_pPingDrop->SetBgColor( Color( 30, 30, 26, 255 ) );
}

void CFoFServersPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	int panelX = 0;
	int panelY = 0;
	GetPos( panelX, panelY );

	// The shipped launcher places its 92%-sized body at five percent of the
	// screen height.  Its top strip is a separate, fixed-height row near the
	// top edge.  At lower resolutions the body intentionally overlaps it.
	m_iTopTall = 32;
	m_iBodyY = clamp(
		RoundFloatToInt( screenTall * 0.05f ) - panelY,
		0,
		MAX( tall - 1, 0 ) );
	m_iLeftWide = clamp(
		(int)( screenWide * 0.2f ), 1, MAX( wide - 1, 1 ) );
	m_iHeaderTall = MAX( FoFLauncherScalePixel( 20.0f, screenTall ), 1 );
	m_iFooterTall = MAX( FoFLauncherScalePixel( 15.0f, screenTall ), 1 );

	const int navTall = MAX(
		FoFLauncherScalePixel( 40.0f, screenTall ), 1 );
	const int singlePlayerY = m_iBodyY +
		FoFLauncherScalePixel( 20.0f, screenTall );
	const int teamsY = m_iBodyY +
		FoFLauncherScalePixel( 60.0f, screenTall );
	const int coopY = m_iBodyY +
		FoFLauncherScalePixel( 100.0f, screenTall );
	const int rankedY = m_iBodyY +
		FoFLauncherScalePixel( 140.0f, screenTall );
	m_pSinglePlayer->SetBounds(
		0, singlePlayerY, m_iLeftWide, navTall );
	m_pTeams->SetBounds( 0, teamsY, m_iLeftWide, navTall );
	m_pCoop->SetBounds( 0, coopY, m_iLeftWide, navTall );
	m_pRanked->SetBounds( 0, rankedY, m_iLeftWide, navTall );
	const int friendsY = m_iBodyY +
		FoFLauncherScalePixel( 180.0f, screenTall );
	m_pFriends->SetBounds( 0, friendsY, m_iLeftWide, navTall );
	m_pFindServers->SetBounds(
		0,
		m_iBodyY + FoFLauncherScalePixel( 220.0f, screenTall ),
		m_iLeftWide,
		navTall );
	m_pBrowserWarning->SetBounds(
		MAX( m_iLeftWide - FoFLauncherScaleHorizontalPixel(
			30.0f, screenWide, screenTall ), 0 ),
		m_iBodyY + FoFLauncherScalePixel( 200.0f, screenTall ),
		MAX( FoFLauncherScaleHorizontalPixel(
			150.0f, screenWide, screenTall ), 1 ),
		MAX( FoFLauncherScalePixel( 80.0f, screenTall ), 1 ) );
	m_pBrowserWarning->SetTextInset(
		MAX( FoFLauncherScaleHorizontalPixel(
			10.0f, screenWide, screenTall ), 0 ),
		MAX( FoFLauncherScalePixel( 5.0f, screenTall ), 0 ) );
	const int courseRefreshWide = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	int selectedY = singlePlayerY;
	if ( m_iPage == FOF_LAUNCHER_PAGE_TEAMS )
		selectedY = teamsY;
	else if ( m_iPage == FOF_LAUNCHER_PAGE_COOP )
		selectedY = coopY;
	else if ( m_iPage == FOF_LAUNCHER_PAGE_RANKED )
		selectedY = rankedY;
	else if ( m_iPage == FOF_LAUNCHER_PAGE_FRIENDS )
		selectedY = friendsY;
	const int courseRefreshMargin =
		FoFLauncherScalePixel( 5.0f, screenTall );
	m_pCourseRefresh->SetBounds(
		MAX( m_iLeftWide - courseRefreshWide - courseRefreshMargin, 0 ),
		selectedY + MAX( ( navTall - courseRefreshWide ) / 2, 0 ),
		courseRefreshWide,
		courseRefreshWide );

	const int refreshWide = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int searchTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int pingTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int searchY = m_iBodyY +
		FoFLauncherScalePixel( 275.0f, screenTall );
	const int searchWide = clamp(
		m_iLeftWide - FoFLauncherScaleHorizontalPixel(
			35.0f, screenWide, screenTall ),
		1,
		MAX( m_iLeftWide - refreshWide, 1 ) );
	m_pSearch->SetBounds(
		0,
		MIN( searchY,
			tall - m_iFooterTall - pingTall - searchTall - 8 ),
		searchWide,
		searchTall );
	int searchX = 0;
	int actualSearchY = 0;
	int actualSearchWide = 0;
	int actualSearchTall = 0;
	m_pSearch->GetBounds(
		searchX, actualSearchY, actualSearchWide, actualSearchTall );
	m_pRefresh->SetBounds(
		MAX( m_iLeftWide - FoFLauncherScaleHorizontalPixel(
			30.0f, screenWide, screenTall ),
			searchX + actualSearchWide ),
		actualSearchY,
		refreshWide,
		refreshWide );
	const int footerTop = tall - m_iFooterTall;
	const int pingTop = footerTop - pingTall;
	m_pPingDrop->SetBounds(
		0,
		pingTop,
		m_iLeftWide,
		pingTall );

	wchar_t wszFriends[128];
	wchar_t wszFriendsPlaying[16];
	V_snwprintf(
		wszFriendsPlaying,
		ARRAYSIZE( wszFriendsPlaying ),
		L"%d",
		m_iFriendsPlaying );
	const wchar_t *pFriendsFormat =
		g_pVGuiLocalize->Find( "#FoF_Friends_Games" );
	if ( pFriendsFormat )
	{
		g_pVGuiLocalize->ConstructString(
			wszFriends,
			sizeof( wszFriends ),
			pFriendsFormat,
			1,
			wszFriendsPlaying );
		m_pFriends->SetCustomText( wszFriends );
	}

	int topX = 0;
	const int topButtonTall = MAX(
		FoFLauncherScalePixel( 12.5f, screenTall ), 1 );
	const int topMargin = MAX( topButtonTall / 10, 2 );
	for ( int i = 0; i < m_TopButtons.Count(); ++i )
	{
		const int buttonWide = clamp(
			m_TopButtons[i]->DesiredWide( topButtonTall, topMargin ),
			60,
			MAX( wide - topX, 60 ) );
		m_TopButtons[i]->SetBounds(
			topX, 0, buttonWide, topButtonTall );
		topX += buttonWide;
	}

	const int workshopMargin = MAX(
		FoFLauncherScalePixel( 5.0f, screenTall ), 1 );
	const int workshopWide = MIN(
		MAX( FoFLauncherScalePixel( 300.0f, screenTall ), 1 ),
		MAX( wide - m_iLeftWide - workshopMargin * 2, 1 ) );
	const int workshopTall = MIN(
		MAX( FoFLauncherScalePixel( 200.0f, screenTall ), 1 ),
		MAX( tall - m_iBodyY - m_iFooterTall - workshopMargin * 2, 1 ) );
	m_pWorkshopPanel->SetBounds(
		m_iLeftWide + workshopMargin,
		m_iBodyY + workshopMargin,
		workshopWide,
		workshopTall );

	if ( m_iPage == FOF_LAUNCHER_PAGE_SINGLEPLAYER )
		LayoutCourses();
	else
		LayoutServers();

	const int rankedContentX = m_iLeftWide +
		FoFLauncherScalePixel( 2.5f, screenTall );
	const int rankedContentY = m_iBodyY + m_iHeaderTall +
		FoFLauncherScalePixel( 15.0f, screenTall ) +
		m_iCourseCardTall + m_iCourseGroupGap;
	m_pRankedPanel->SetBounds(
		rankedContentX,
		rankedContentY,
		MAX( wide - rankedContentX -
			FoFLauncherScalePixel( 10.0f, screenTall ), 1 ),
		MAX( tall - m_iFooterTall - rankedContentY, 1 ) );
}

void CFoFServersPanel::PaintBackground()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	vgui::surface()->DrawSetColor( 7, 3, 0, 230 );
	vgui::surface()->DrawFilledRect( 0, 0, wide, m_iTopTall );
	vgui::surface()->DrawSetColor( 50, 50, 38, 255 );
	vgui::surface()->DrawFilledRect( 0, m_iBodyY, wide, tall );

	const int backgroundY = m_iBodyY + m_iHeaderTall;
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const bool useWideBackground = screenWide * 3 > screenTall * 4;
	const int texture = useWideBackground ?
		m_iBackgroundWide : m_iBackgroundFourThree;
	const int imageGutter = MAX(
		FoFLauncherScalePixel( 5.0f, screenTall ), 1 );
	const int backgroundWide = MAX(
		(int)( screenWide * 0.92f ) -
			m_iLeftWide - imageGutter,
		1 );
	const int backgroundTall = MAX(
		(int)( screenTall * 0.92f ) -
			FoFLauncherScalePixel( 35.0f, screenTall ),
		1 );
	const int backgroundBottom = backgroundY + backgroundTall;
	if ( texture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( texture );
		// The original image panel leaves a narrow right gutter and sizes the
		// backing image independently from its clipping panel.  Its wide image
		// deliberately extends beyond the panel and is clipped on the right.
		// main_menu_bg stores its 2048x1280 artwork in a 2048x2048 texture;
		// the remaining rows are only mustard-coloured padding.
		const float backgroundV = useWideBackground ? 0.625f : 1.0f;
		vgui::surface()->DrawTexturedSubRect(
			m_iLeftWide,
			backgroundY,
			m_iLeftWide + backgroundWide,
			backgroundBottom,
			0.0f, 0.0f, 1.0f, backgroundV );
	}

	const int footerTop = tall - m_iFooterTall;
	const int pingTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int pingTop = footerTop - pingTall;
	vgui::surface()->DrawSetColor( 36, 37, 31, 255 );
	vgui::surface()->DrawFilledRect(
		0, pingTop, m_iLeftWide, footerTop );
	vgui::surface()->DrawSetColor( 50, 50, 38, 255 );
	vgui::surface()->DrawFilledRect( 0, footerTop, wide, tall );
}

void CFoFServersPanel::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int contentX = m_iLeftWide +
		FoFLauncherScalePixel( 2.5f, screenTall );
	const int groupTitleTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int cardTall = m_iCourseCardTall;
	const int groupGap = m_iCourseGroupGap;
	int groupY = m_iBodyY + m_iHeaderTall;
	static const char *courseGroupTokens[] =
	{
		"#LaunchMenu_Tutorials",
		"#LaunchMenu_Challenges",
		"#LaunchMenu_Missions"
	};
	static const char *serverGroupTokens[] =
	{
		"#LaunchMenu2_Unranked",
		"#LaunchMenu2_Modded",
		"#LaunchMenu2_Custom",
		"#LaunchMenu2_Unranked2"
	};
	const int groupCount =
		m_iPage == FOF_LAUNCHER_PAGE_SINGLEPLAYER ? 3 :
		( m_iPage == FOF_LAUNCHER_PAGE_TEAMS ? 4 : 1 );
	for ( int group = 0; group < groupCount; ++group )
	{
		const char *pszToken = "";
		if ( m_iPage == FOF_LAUNCHER_PAGE_SINGLEPLAYER )
			pszToken = courseGroupTokens[group];
		else if ( m_iPage == FOF_LAUNCHER_PAGE_TEAMS )
			pszToken = serverGroupTokens[group];
		else if ( m_iPage == FOF_LAUNCHER_PAGE_COOP )
			pszToken = "#LaunchMenu_Coop";
		else if ( m_iPage == FOF_LAUNCHER_PAGE_RANKED )
			pszToken = "#LaunchMenu_Ranked";
		else if ( m_iPage == FOF_LAUNCHER_PAGE_FRIENDS )
			pszToken = "#FoF_FriendName";
		FoFLauncherDrawLocalizedText(
			pszToken,
			m_hSectionFont,
			contentX,
			groupY,
			Color( 235, 225, 210, 255 ) );
		groupY += groupTitleTall + cardTall + groupGap;
	}

	if ( m_iOnlinePlayers > 0 )
	{
		const wchar_t *pOnline =
			g_pVGuiLocalize->Find( "#Highlighted_Online" );
		if ( pOnline )
		{
			wchar_t wszOnline[128];
			V_snwprintf(
				wszOnline,
				ARRAYSIZE( wszOnline ),
				L"%d %ls",
				m_iOnlinePlayers,
				pOnline );
			const int onlineWide = FoFLauncherTextWide(
				wszOnline, m_hSmallFont );
			FoFLauncherDrawText(
				wszOnline,
				m_hSmallFont,
				MAX( wide - onlineWide - 12, m_iLeftWide ),
				m_iBodyY + MAX(
					( m_iHeaderTall -
						vgui::surface()->GetFontTall( m_hSmallFont ) ) / 2,
					0 ),
				Color( 240, 235, 20, 255 ) );
		}
	}

	if ( FoFShouldShowWesternPass() )
	{
		const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
		if ( m_flNextWesternPassUpdate <= now )
		{
			m_nWesternPassProgress = FoFReadWesternPassProgress();
			m_flNextWesternPassUpdate = now + 10.0f;
		}

		wchar_t wszRank[128];
		const wchar_t *pRankFormat =
			g_pVGuiLocalize->Find( "#FoF.Rank_display" );
		if ( pRankFormat )
		{
			wchar_t wszValue[32];
			V_snwprintf( wszValue, ARRAYSIZE( wszValue ), L"%.1f/100",
				(float)m_nWesternPassProgress * 0.01f );
			g_pVGuiLocalize->ConstructString(
				wszRank,
				sizeof( wszRank ),
				pRankFormat,
				1,
				wszValue );
			FoFLauncherDrawText(
				wszRank,
				m_hSmallFont,
				MAX( m_iLeftWide - FoFLauncherScalePixel(
					27.5f, screenTall ), 2 ),
				m_iBodyY + MAX(
					( m_iHeaderTall -
						vgui::surface()->GetFontTall( m_hSmallFont ) ) / 2,
					0 ),
				Color( 25, 230, 20, 255 ) );
		}
	}

	wchar_t wszFullServers[128];
	const wchar_t *pFullFormat =
		g_pVGuiLocalize->Find( "#FoF_Full_Games" );
	if ( pFullFormat )
	{
		wchar_t wszZero[16];
		V_snwprintf(
			wszZero,
			ARRAYSIZE( wszZero ),
			L"%d",
			m_iFullServers );
		g_pVGuiLocalize->ConstructString(
			wszFullServers,
			sizeof( wszFullServers ),
			pFullFormat,
			1,
			wszZero );
		const int textWide = FoFLauncherTextWide(
			wszFullServers, m_hSmallFont );
		FoFLauncherDrawText(
			wszFullServers,
			m_hSmallFont,
			MAX( wide - textWide - 4, m_iLeftWide ),
			tall - m_iFooterTall + 1,
			Color( 235, 235, 225, 255 ) );
	}

}

void CFoFServersPanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// Server and course cards overlap the navigation controls by design.  The
	// original panel paints the arrows after its card children, so perform the
	// button's single deferred paint pass here.  Input and armed state still
	// come from the live Button control.
	for ( int group = 0; group < FOF_LAUNCHER_MAX_GROUPS; ++group )
	{
		CFoFLauncherPageButton *pButtons[] =
		{
			m_pGroupPrevious[group],
			m_pGroupNext[group]
		};
		for ( int i = 0; i < ARRAYSIZE( pButtons ); ++i )
		{
			CFoFLauncherPageButton *pButton = pButtons[i];
			if ( !pButton || !pButton->IsVisible() )
				continue;

			pButton->PaintOnTop();
		}
	}
}
