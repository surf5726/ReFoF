#include "cbase.h"
#include "fof/fof_launcher_ranked.h"

#include "fof/fof_launcher_widgets.h"
#include "filesystem.h"

#include <time.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/RichText.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *g_pszFoFRankedLeaderboardURL =
	"http://steamcommunity.com/stats/265630/leaderboards/6421235/";

static int FoFRankedDaysRemaining(
	int year,
	int month,
	int day )
{
	const time_t now = time( NULL );
	struct tm *pNow = localtime( &now );
	if ( !pNow )
		return 0;

	// The original client uses the historical
	// launcher approximation: every month is 30 days, and any later year
	// contributes one 365-day block.  Preserve that behavior, including its
	// handling of stale season files.
	int daysRemaining =
		( month - pNow->tm_mon - 1 ) * 30 + day - pNow->tm_mday;
	if ( year > pNow->tm_year + 1900 )
		daysRemaining += 365;
	return daysRemaining;
}

static void FoFRankedConstructLocalizedText(
	const char *pszToken,
	const wchar_t *pValue,
	wchar_t *pOutput,
	int outputBytes )
{
	const wchar_t *pFormat = g_pVGuiLocalize->Find( pszToken );
	if ( pFormat )
	{
		g_pVGuiLocalize->ConstructString(
			pOutput,
			outputBytes,
			pFormat,
			1,
			pValue );
		return;
	}

	V_wcsncpy(
		pOutput,
		pValue ? pValue : L"",
		outputBytes / sizeof( wchar_t ) );
}

CFoFRankedPanel::CFoFRankedPanel( vgui::Panel *pParent )
	: BaseClass( pParent, "FoFRankedPanel" )
	, m_pLeaderboard( NULL )
	, m_pRules( NULL )
	, m_pModeButton( NULL )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hInfoFont( vgui::INVALID_FONT )
	, m_bShowingRules( false )
{
	m_wszTitle[0] = L'\0';
	m_wszEnd[0] = L'\0';

	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );

	m_pLeaderboard = new vgui::HTML(
		this, "html_ranked", true, false );
	m_pLeaderboard->SetScrollbarsEnabled( true );
	m_pLeaderboard->SetContextMenuEnabled( false );
	m_pLeaderboard->SetViewSourceEnabled( false );

	m_pRules = new vgui::RichText( this, "ranked_rules" );
	m_pRules->SetVerticalScrollbar( true );
	m_pRules->SetUnusedScrollbarInvisible( true );
	m_pRules->SetPanelInteractive( true );

	m_pModeButton = new vgui::Button(
		this,
		"rankhelp",
		"?",
		this,
		"ranked_toggle_info" );
	m_pModeButton->SetButtonBorderEnabled( false );
	m_pModeButton->DrawFocusBox( false );

	ReloadSeasonText();
	ShowRules( false );
	SetVisible( false );
}

CFoFRankedPanel::~CFoFRankedPanel()
{
}

void CFoFRankedPanel::Open()
{
	ShowRules( false );
	SetVisible( true );
	m_pLeaderboard->OpenURL(
		g_pszFoFRankedLeaderboardURL, NULL, false );
	Repaint();
}

void CFoFRankedPanel::Close()
{
	SetVisible( false );
}

bool CFoFRankedPanel::IsOpen()
{
	return IsVisible();
}

void CFoFRankedPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hTitleFont = pFoFScheme ?
		pFoFScheme->GetFont( "NotorietyFont", false ) :
		vgui::INVALID_FONT;
	m_hInfoFont = pFoFScheme ?
		pFoFScheme->GetFont( "DefaultFoF", false ) :
		vgui::INVALID_FONT;
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = pScheme->GetFont( "DefaultSmall", false );
	if ( m_hInfoFont == vgui::INVALID_FONT )
		m_hInfoFont = pScheme->GetFont( "Default", false );

	m_pRules->SetFont( m_hInfoFont );
	m_pRules->SetFgColor( Color( 225, 222, 210, 255 ) );
	m_pRules->SetBgColor( Color( 8, 8, 7, 238 ) );
	m_pRules->SetPaintBackgroundEnabled( true );
	m_pRules->SetPaintBorderEnabled( false );

	m_pModeButton->SetFont( m_hInfoFont );
	m_pModeButton->SetDefaultColor(
		Color( 225, 222, 210, 255 ), Color( 20, 18, 14, 235 ) );
	m_pModeButton->SetArmedColor(
		Color( 255, 255, 255, 255 ), Color( 154, 28, 18, 245 ) );
	m_pModeButton->SetDepressedColor(
		Color( 255, 255, 255, 255 ), Color( 190, 22, 13, 255 ) );
}

void CFoFRankedPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );

	const int margin = MAX(
		FoFLauncherScalePixel( 2.0f, screenTall ), 1 );
	const int headerTall = MAX(
		FoFLauncherScalePixel( 18.0f, screenTall ), 1 );
	const int buttonWide = MAX(
		FoFLauncherScalePixel(
			m_bShowingRules ? 55.0f : 18.0f,
			screenTall ),
		1 );
	m_pModeButton->SetBounds(
		MAX( wide - buttonWide - margin, 0 ),
		margin,
		buttonWide,
		MAX( headerTall - margin * 2, 1 ) );

	const int contentY = headerTall;
	const int contentTall = MAX( tall - contentY, 1 );
	m_pLeaderboard->SetBounds(
		0, contentY, MAX( wide, 1 ), contentTall );
	m_pRules->SetBounds(
		0, contentY, MAX( wide, 1 ), contentTall );
}

void CFoFRankedPanel::PaintBackground()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	vgui::surface()->DrawSetColor( 4, 3, 2, 220 );
	vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
}

void CFoFRankedPanel::Paint()
{
	BaseClass::Paint();
	if ( m_hTitleFont == vgui::INVALID_FONT )
		return;

	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int margin = MAX(
		FoFLauncherScalePixel( 2.5f, screenTall ), 1 );
	const int textY = MAX(
		( FoFLauncherScalePixel( 18.0f, screenTall ) -
			vgui::surface()->GetFontTall( m_hTitleFont ) ) / 2,
		0 );
	FoFLauncherDrawText(
		m_wszTitle,
		m_hTitleFont,
		margin,
		textY,
		Color( 238, 228, 208, 255 ) );

	if ( m_hInfoFont != vgui::INVALID_FONT && m_wszEnd[0] )
	{
		const int endWide = FoFLauncherTextWide(
			m_wszEnd, m_hInfoFont );
		const int buttonReserve = FoFLauncherScalePixel(
			m_bShowingRules ? 60.0f : 23.0f, screenTall );
		FoFLauncherDrawText(
			m_wszEnd,
			m_hInfoFont,
			MAX( wide - endWide - buttonReserve, margin ),
			MAX(
				( FoFLauncherScalePixel( 18.0f, screenTall ) -
					vgui::surface()->GetFontTall( m_hInfoFont ) ) / 2,
				0 ),
			Color( 226, 218, 195, 255 ) );
	}
}

void CFoFRankedPanel::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "ranked_toggle_info" ) )
	{
		ShowRules( !m_bShowingRules );
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFRankedPanel::ReloadSeasonText()
{
	int season = 0;
	int endDay = 0;
	int endMonth = 0;
	int endYear = 0;
	KeyValues *pConfig = new KeyValues( "server_bans" );
	if ( pConfig->LoadFromFile(
		filesystem, "fof_scripts/serverban.txt", "GAME" ) )
	{
		KeyValues *pRanked = pConfig->FindKey( "ranked", false );
		if ( pRanked )
		{
			season = pRanked->GetInt( "season", 0 );
			endDay = pRanked->GetInt( "end_day", 0 );
			endMonth = pRanked->GetInt( "end_month", 0 );
			endYear = pRanked->GetInt( "end_year", 0 );
		}
	}
	pConfig->deleteThis();
	FormatSeasonText( season, endDay, endMonth, endYear );

	const wchar_t *pRules =
		g_pVGuiLocalize->Find( "#Ranked_Mode_Info" );
	if ( pRules && pRules[0] )
	{
		m_pRules->SetText( pRules );
	}
	else
	{
		m_pRules->SetText(
			"Earn dollars from kills and assists. Richer targets are worth "
			"more, each match charges an entry fee, and the top 100 players "
			"receive a scoreboard badge for the following season." );
	}
	m_pRules->GotoTextStart();
}

void CFoFRankedPanel::SetSeasonData(
	int season,
	int endDay,
	int endMonth,
	int endYear )
{
	FormatSeasonText( season, endDay, endMonth, endYear );
	Repaint();
}

void CFoFRankedPanel::FormatSeasonText(
	int season,
	int endDay,
	int endMonth,
	int endYear )
{
	wchar_t wszSeason[32];
	V_snwprintf(
		wszSeason,
		ARRAYSIZE( wszSeason ),
		L"%d",
		season );
	FoFRankedConstructLocalizedText(
		"#Ranked_Mode_Title",
		wszSeason,
		m_wszTitle,
		sizeof( m_wszTitle ) );
	if ( !m_wszTitle[0] )
	{
		V_wcsncpy(
			m_wszTitle,
			L"RANKED SHOOTOUT",
			ARRAYSIZE( m_wszTitle ) );
	}

	wchar_t wszDays[32];
	V_snwprintf(
		wszDays,
		ARRAYSIZE( wszDays ),
		L"%d",
		FoFRankedDaysRemaining(
			endYear, endMonth, endDay ) );
	FoFRankedConstructLocalizedText(
		"#Ranked_Mode_End",
		wszDays,
		m_wszEnd,
		sizeof( m_wszEnd ) );
}

void CFoFRankedPanel::ShowRules( bool showRules )
{
	m_bShowingRules = showRules;
	m_pLeaderboard->SetVisible( !showRules );
	m_pRules->SetVisible( showRules );
	m_pModeButton->SetText(
		showRules ? "#GameUI_Back" : "?" );
	InvalidateLayout( true );
	Repaint();
}
