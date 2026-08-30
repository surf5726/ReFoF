#include "cbase.h"
#include "fof/fof_launcher_personal_stats.h"

#include "fof/fof_steam_stats.h"
#include "fof/fof_launcher_widgets.h"
#include "hl2mp_weapon_parse.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "steam/steam_api.h"
#include "weapon_parse.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum FoFPersonalStatsMetric
{
	FOF_STATS_DAMAGE = 0,
	FOF_STATS_FRAGS,
	FOF_STATS_DAMAGE_PER_FRAG,
	FOF_STATS_FRAGS_PER_MINUTE,
	FOF_STATS_ACCURACY,
	FOF_STATS_WEAPON_DATA,
	FOF_STATS_METRIC_COUNT
};

static const int g_FoFStatsButtonMetrics[] =
{
	FOF_STATS_FRAGS,
	FOF_STATS_DAMAGE,
	FOF_STATS_DAMAGE_PER_FRAG,
	FOF_STATS_FRAGS_PER_MINUTE,
	FOF_STATS_ACCURACY,
	FOF_STATS_WEAPON_DATA
};

static const char *g_FoFStatsButtonLabels[] =
{
	"#PlayerFrags",
	"Damage",
	"Dmg/Frags",
	"Frags/Minute",
	"Accuracy",
	"Weapon Stats"
};

class CFoFPersonalStatsButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFPersonalStatsButton, vgui::Button );

public:
	CFoFPersonalStatsButton(
		vgui::Panel *pParent,
		const char *pszName,
		const char *pszLabel,
		vgui::Panel *pTarget,
		const char *pszCommand )
		: BaseClass( pParent, pszName, "", pTarget, pszCommand )
		, m_Label( pszLabel ? pszLabel : "" )
		, m_hFont( vgui::INVALID_FONT )
	{
		SetButtonBorderEnabled( false );
		SetPaintBackgroundEnabled( false );
		SetPaintBorderEnabled( false );
		DrawFocusBox( false );
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
			vgui::scheme()->GetScheme( "ClientScheme" ) );
		m_hFont = pFoFScheme ?
			pFoFScheme->GetFont( "DefaultFoF", false ) :
			vgui::INVALID_FONT;
		if ( m_hFont == vgui::INVALID_FONT )
			m_hFont = pScheme->GetFont( "DefaultSmall", false );
	}

	virtual void Paint()
	{
		int wide = 0;
		int tall = 0;
		GetSize( wide, tall );
		Color background( 0, 0, 0, 215 );
		if ( IsSelected() )
			background = Color( 205, 0, 0, 235 );
		else if ( IsArmed() )
			background = Color( 158, 27, 18, 235 );

		vgui::surface()->DrawSetColor( background );
		vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
		vgui::surface()->DrawSetColor( 205, 205, 198, 230 );
		vgui::surface()->DrawOutlinedRect( 0, 0, wide, tall );

		if ( !m_Label.IsEmpty() && m_hFont != vgui::INVALID_FONT )
		{
			wchar_t wszFallback[128];
			const wchar_t *pText = FoFLauncherLocalize(
				m_Label.String(), wszFallback, sizeof( wszFallback ) );
			int textWide = 0;
			int textTall = 0;
			vgui::surface()->GetTextSize(
				m_hFont, pText, textWide, textTall );
			FoFLauncherDrawText(
				pText,
				m_hFont,
				MAX( ( wide - textWide ) / 2, 0 ),
				MAX( ( tall - textTall ) / 2, 0 ),
				Color( 232, 230, 220, 255 ) );
		}
	}

private:
	CUtlString m_Label;
	vgui::HFont m_hFont;
};

static void FoFUppercaseStatsLabel(
	const char *pszSource,
	char *pszDestination,
	int destinationBytes )
{
	Q_strncpy(
		pszDestination,
		pszSource ? pszSource : "",
		destinationBytes );
	for ( char *pCharacter = pszDestination;
		*pCharacter;
		++pCharacter )
	{
		if ( *pCharacter >= 'a' && *pCharacter <= 'z' )
			*pCharacter = *pCharacter - 'a' + 'A';
	}
}

static void FoFSetStatsSummary(
	const char *pszToken,
	const char *pszValue,
	wchar_t *pOutput,
	int outputBytes )
{
	const wchar_t *pFormat = g_pVGuiLocalize->Find( pszToken );
	wchar_t wszValue[64];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		pszValue ? pszValue : "", wszValue, sizeof( wszValue ) );
	if ( pFormat )
	{
		g_pVGuiLocalize->ConstructString(
			pOutput, outputBytes, pFormat, 1, wszValue );
		return;
	}

	wchar_t wszToken[128];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		pszToken ? pszToken : "", wszToken, sizeof( wszToken ) );
	V_snwprintf(
		pOutput,
		outputBytes / sizeof( wchar_t ),
		L"%ls: %ls",
		wszToken,
		wszValue );
}

static void FoFSetStatsAssistSummary(
	int assists,
	float assistsPerFrag,
	wchar_t *pOutput,
	int outputBytes )
{
	char assistText[32];
	char ratioText[32];
	Q_snprintf( assistText, sizeof( assistText ), "%d", assists );
	Q_snprintf( ratioText, sizeof( ratioText ), "%.2f", assistsPerFrag );
	wchar_t wszAssists[32];
	wchar_t wszRatio[32];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		assistText, wszAssists, sizeof( wszAssists ) );
	g_pVGuiLocalize->ConvertANSIToUnicode(
		ratioText, wszRatio, sizeof( wszRatio ) );
	const wchar_t *pFormat = g_pVGuiLocalize->Find(
		"#FoF_Weapon_Stats_Assist" );
	if ( pFormat )
	{
		g_pVGuiLocalize->ConstructString(
			pOutput,
			outputBytes,
			pFormat,
			2,
			wszAssists,
			wszRatio );
	}
	else
	{
		V_snwprintf(
			pOutput,
			outputBytes / sizeof( wchar_t ),
			L"Assists: %ls (%ls per frag)",
			wszAssists,
			wszRatio );
	}
}

static const CHL2MPSWeaponInfo *FoFStatsWeaponInfo(
	const char *pszStatsName )
{
	char classname[64];
	Q_snprintf(
		classname,
		sizeof( classname ),
		"weapon_%s",
		pszStatsName ? pszStatsName : "" );
	WEAPON_FILE_INFO_HANDLE handle = LookupWeaponInfoSlot( classname );
	if ( handle == GetInvalidWeaponInfoHandle() )
	{
		ReadWeaponDataFromFileForSlot(
			filesystem, classname, &handle );
	}
	if ( handle == GetInvalidWeaponInfoHandle() )
		return NULL;
	const FileWeaponInfo_t *pInfo = GetFileWeaponInfoFromHandle( handle );
	return pInfo ? &static_cast< const CHL2MPSWeaponInfo & >( *pInfo ) : NULL;
}

static int FoFStatsBaseDamage(
	const FoFWeaponStatSource &source,
	const CHL2MPSWeaponInfo *pInfo )
{
	if ( !Q_stricmp( source.m_pszName, "coachgun" ) ||
		!Q_stricmp( source.m_pszName, "shotgun" ) )
	{
		return 55;
	}
	if ( !pInfo )
		return 0;
	if ( !Q_stricmp( source.m_pszName, "sawedoff_shotgun" ) )
		return pInfo->m_iPlayerDamage * 18;
	return pInfo->m_iPlayerDamage;
}

static float FoFStatsBaseAccuracy(
	const FoFWeaponStatSource &source,
	const CHL2MPSWeaponInfo *pInfo )
{
	if ( !pInfo )
		return 0.0f;

	float spread = pInfo->m_flFoFSpread[2];
	if ( !Q_stricmp( source.m_pszName, "henryrifle" ) ||
		!Q_stricmp( source.m_pszName, "carbine" ) ||
		!Q_stricmp( source.m_pszName, "sharps" ) )
	{
		spread *= 0.1f;
	}
	const float accuracyStart = RemapValClamped(
		spread, 0.40f, 0.01f, 0.25f, 0.07f );
	return RemapValClamped(
		spread, accuracyStart, 0.01f, 1.0f, 99.0f );
}

static void FoFDrawRoundedStatsRect(
	int wide,
	int tall,
	int radius,
	Color color )
{
	vgui::surface()->DrawSetColor( color );
	vgui::surface()->DrawFilledRect( 0, radius, wide, tall - radius );
	vgui::surface()->DrawFilledRect(
		radius, 0, wide - radius, radius );
	vgui::surface()->DrawFilledRect(
		radius, tall - radius, wide - radius, tall );
}

CFoFPersonalStatsPanel::CFoFPersonalStatsPanel()
	: BaseClass( NULL, "FoFPersonalStats" )
	, m_pCloseButton( NULL )
	, m_hSummaryFont( vgui::INVALID_FONT )
	, m_hGraphFont( vgui::INVALID_FONT )
	, m_iMetric( FOF_STATS_FRAGS )
	, m_iSummaryLines( 0 )
{
	m_wszSummary[0][0] = L'\0';
	m_wszSummary[1][0] = L'\0';
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetZPos( 2000 );
	MakePopup( false );

	for ( int i = 0; i < ARRAYSIZE( g_FoFStatsButtonMetrics ); ++i )
	{
		char name[32];
		char command[32];
		Q_snprintf( name, sizeof( name ), "StatsMetric%d", i );
		Q_snprintf(
			command,
			sizeof( command ),
			"stats_metric:%d",
			g_FoFStatsButtonMetrics[i] );
		CFoFPersonalStatsButton *pButton =
			new CFoFPersonalStatsButton(
				this,
				name,
				g_FoFStatsButtonLabels[i],
				this,
				command );
		pButton->SetSelected(
			g_FoFStatsButtonMetrics[i] == m_iMetric );
		m_MetricButtons.AddToTail( pButton );
	}

	m_pCloseButton = new CFoFPersonalStatsButton(
		this,
		"StatsCloseButton",
		"#GameUI_Close",
		this,
		"stats_close" );
	SetVisible( false );
}

CFoFPersonalStatsPanel::~CFoFPersonalStatsPanel()
{
}

void CFoFPersonalStatsPanel::Open()
{
	ReloadStats();
	RequestStats();
	SetVisible( true );
	MoveToFront();
	RequestFocus();
	InvalidateLayout( true );
	Repaint();
}

void CFoFPersonalStatsPanel::Close()
{
	SetVisible( false );
}

bool CFoFPersonalStatsPanel::IsOpen()
{
	return IsVisible();
}

void CFoFPersonalStatsPanel::ApplySchemeSettings(
	vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hSummaryFont = pFoFScheme ?
		pFoFScheme->GetFont( "Default", false ) :
		vgui::INVALID_FONT;
	m_hGraphFont = pFoFScheme ?
		pFoFScheme->GetFont( "DefaultFoF", false ) :
		vgui::INVALID_FONT;
	if ( m_hSummaryFont == vgui::INVALID_FONT )
		m_hSummaryFont = pScheme->GetFont( "Default", false );
	if ( m_hGraphFont == vgui::INVALID_FONT )
		m_hGraphFont = pScheme->GetFont( "DefaultSmall", false );
	SetPaintBorderEnabled( false );
}

void CFoFPersonalStatsPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int wide = MIN(
		FoFLauncherScalePixel( 600.0f, screenTall ),
		screenWide );
	const int tall = MIN(
		FoFLauncherScalePixel( 370.0f, screenTall ),
		screenTall );
	SetBounds(
		MAX( ( screenWide - wide ) / 2, 0 ),
		MAX( FoFLauncherScalePixel( 20.0f, screenTall ), 0 ),
		wide,
		tall );

	const int buttonWide = FoFLauncherScalePixel( 85.0f, screenTall );
	const int buttonTall = FoFLauncherScalePixel( 15.0f, screenTall );
	const int buttonY = FoFLauncherScalePixel( 325.0f, screenTall );
	for ( int i = 0; i < 5 && i < m_MetricButtons.Count(); ++i )
	{
		m_MetricButtons[i]->SetBounds(
			FoFLauncherScalePixel( 10.0f + i * 90.0f, screenTall ),
			buttonY,
			buttonWide,
			buttonTall );
	}
	if ( m_MetricButtons.Count() > 5 )
	{
		m_MetricButtons[5]->SetBounds(
			FoFLauncherScalePixel( 10.0f, screenTall ),
			FoFLauncherScalePixel( 345.0f, screenTall ),
			buttonWide,
			buttonTall );
	}
	m_pCloseButton->SetBounds(
		FoFLauncherScalePixel( 490.0f, screenTall ),
		buttonY,
		buttonWide,
		buttonTall );
}

void CFoFPersonalStatsPanel::PaintBackground()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenTall = 0;
	int screenWide = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int radius = MAX(
		FoFLauncherScalePixel( 3.0f, screenTall ), 2 );
	(void)screenWide;
	FoFDrawRoundedStatsRect(
		wide, tall, radius, Color( 0, 0, 0, 220 ) );
}

void CFoFPersonalStatsPanel::Paint()
{
	DrawSummary();
	DrawGraph();
}

void CFoFPersonalStatsPanel::OnCommand(
	const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "stats_close" ) ||
		!Q_stricmp( pszCommand, "Close" ) )
	{
		Close();
		return;
	}
	if ( !Q_strnicmp( pszCommand, "stats_metric:", 13 ) )
	{
		SelectMetric( atoi( pszCommand + 13 ) );
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFPersonalStatsPanel::OnKeyCodePressed(
	vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		Close();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

void CFoFPersonalStatsPanel::SelectMetric( int metric )
{
	if ( metric < 0 || metric >= FOF_STATS_METRIC_COUNT )
		return;
	m_iMetric = metric;
	for ( int i = 0; i < m_MetricButtons.Count(); ++i )
	{
		m_MetricButtons[i]->SetSelected(
			g_FoFStatsButtonMetrics[i] == m_iMetric );
	}
	ReloadStats();
	Repaint();
}

void CFoFPersonalStatsPanel::RequestStats()
{
	ISteamUserStats *pStats = steamapicontext ?
		steamapicontext->SteamUserStats() : NULL;
	ISteamUser *pUser = steamapicontext ?
		steamapicontext->SteamUser() : NULL;
	if ( !pStats || !pUser )
		return;

	const SteamAPICall_t call = pStats->RequestUserStats(
		pUser->GetSteamID() );
	if ( call == k_uAPICallInvalid )
	{
		pStats->RequestCurrentStats();
		return;
	}

	m_UserStatsCall.Set(
		call,
		this,
		&CFoFPersonalStatsPanel::OnUserStatsReceived );
}

void CFoFPersonalStatsPanel::OnUserStatsReceived(
	UserStatsReceived_t *pResult,
	bool bIOFailure )
{
	if ( bIOFailure || !pResult ||
		pResult->m_eResult != k_EResultOK )
	{
		return;
	}

	ISteamUtils *pUtils = steamapicontext ?
		steamapicontext->SteamUtils() : NULL;
	if ( pUtils && pResult->m_nGameID !=
		CGameID( pUtils->GetAppID() ).ToUint64() )
	{
		return;
	}

	ReloadStats();
	Repaint();
}

void CFoFPersonalStatsPanel::ReloadStats()
{
	m_Points.RemoveAll();
	m_SecondaryPoints.RemoveAll();
	m_iSummaryLines = 0;
	m_wszSummary[0][0] = L'\0';
	m_wszSummary[1][0] = L'\0';

	ISteamUserStats *pStats = steamapicontext ?
		steamapicontext->SteamUserStats() : NULL;

	float totalDamage = 0.0f;
	float totalFrags = 0.0f;
	float totalAccuracy = 0.0f;
	int accuracyCount = 0;
	float playTime = FoFReadSteamFloatStat(
		pStats, "stat_play_time" );
	// Steam stores this stat in seconds in released FoF builds.
	const float playMinutes = playTime > 0.0f ?
		playTime / 60.0f : 0.0f;

	for ( int i = 0; i < FOF_WEAPON_STAT_SOURCE_COUNT; ++i )
	{
		const FoFWeaponStatSource &source =
			g_FoFWeaponStatSources[i];
		char statName[80];
		int damage = 0;
		int frags = 0;
		int hits = 0;
		int misses = 0;
		Q_snprintf(
			statName, sizeof( statName ),
			"stat_%s_dmg", source.m_pszName );
		FoFReadSteamIntStat( pStats, statName, damage );
		Q_snprintf(
			statName, sizeof( statName ),
			"stat_%s_kills", source.m_pszName );
		FoFReadSteamIntStat( pStats, statName, frags );
		totalDamage += damage;
		totalFrags += frags;

		FoFPersonalStatsPoint point;
		Q_memset( &point, 0, sizeof( point ) );
		FoFUppercaseStatsLabel(
			source.m_pszName,
			point.m_szLabel,
			sizeof( point.m_szLabel ) );

		if ( m_iMetric == FOF_STATS_DAMAGE )
		{
			point.m_flValue = (float)damage;
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%d",
				damage );
		}
		else if ( m_iMetric == FOF_STATS_FRAGS )
		{
			point.m_flValue = (float)frags;
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%d",
				frags );
		}
		else if ( m_iMetric == FOF_STATS_DAMAGE_PER_FRAG )
		{
			point.m_flValue = frags > 0 ?
				(float)damage / (float)frags : 0.0f;
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%.2f",
				point.m_flValue );
		}
		else if ( m_iMetric == FOF_STATS_FRAGS_PER_MINUTE )
		{
			point.m_flValue = playMinutes > 0.0f ?
				(float)frags / playMinutes : 0.0f;
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%.2f",
				point.m_flValue );
		}
		else if ( m_iMetric == FOF_STATS_ACCURACY )
		{
			if ( source.m_iCategory != 2 &&
				source.m_iCategory != 3 &&
				source.m_iCategory != 13 )
			{
				continue;
			}
			Q_snprintf(
				statName, sizeof( statName ),
				"stat_%s_hits", source.m_pszName );
			FoFReadSteamIntStat( pStats, statName, hits );
			Q_snprintf(
				statName, sizeof( statName ),
				"stat_%s_misses", source.m_pszName );
			FoFReadSteamIntStat( pStats, statName, misses );
			if ( hits <= 0 )
				continue;
			const int shots = MAX( hits + misses, 1 );
			point.m_flValue =
				(float)hits / (float)shots * 100.0f;
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%.1f (%d/%d)",
				point.m_flValue,
				hits,
				shots );
			totalAccuracy += point.m_flValue;
			++accuracyCount;
		}
		else if ( m_iMetric == FOF_STATS_WEAPON_DATA )
		{
			if ( source.m_iCategory != 2 &&
				source.m_iCategory != 3 )
			{
				continue;
			}
			const CHL2MPSWeaponInfo *pInfo =
				FoFStatsWeaponInfo( source.m_pszName );
			point.m_flValue =
				FoFStatsBaseAccuracy( source, pInfo );
			Q_snprintf(
				point.m_szValue,
				sizeof( point.m_szValue ),
				"%.1f%%",
				point.m_flValue );

			FoFPersonalStatsPoint damagePoint = point;
			damagePoint.m_flValue =
				(float)FoFStatsBaseDamage( source, pInfo );
			Q_snprintf(
				damagePoint.m_szValue,
				sizeof( damagePoint.m_szValue ),
				"%ihp",
				(int)damagePoint.m_flValue );
			m_SecondaryPoints.AddToTail( damagePoint );
		}
		m_Points.AddToTail( point );
	}

	char summaryValue[64];
	if ( m_iMetric == FOF_STATS_DAMAGE )
	{
		Q_snprintf(
			summaryValue, sizeof( summaryValue ),
			"%d", (int)totalDamage );
		FoFSetStatsSummary(
			"#FoF_Weapon_Stats_TotalDmg",
			summaryValue,
			m_wszSummary[0],
			sizeof( m_wszSummary[0] ) );
		m_iSummaryLines = 1;
	}
	else if ( m_iMetric == FOF_STATS_FRAGS )
	{
		Q_snprintf(
			summaryValue, sizeof( summaryValue ),
			"%d", (int)totalFrags );
		FoFSetStatsSummary(
			"#FoF_Weapon_Stats_TotalFrags",
			summaryValue,
			m_wszSummary[0],
			sizeof( m_wszSummary[0] ) );
		int assists = 0;
		FoFReadSteamIntStat( pStats, "stat_assists", assists );
		FoFSetStatsAssistSummary(
			assists,
			totalFrags > 0.0f ? assists / totalFrags : 0.0f,
			m_wszSummary[1],
			sizeof( m_wszSummary[1] ) );
		m_iSummaryLines = 2;
	}
	else if ( m_iMetric == FOF_STATS_DAMAGE_PER_FRAG )
	{
		Q_snprintf(
			summaryValue, sizeof( summaryValue ),
			"%.2f",
			totalFrags > 0.0f ? totalDamage / totalFrags : 0.0f );
		FoFSetStatsSummary(
			"#FoF_Weapon_Stats_Dmg-Frags",
			summaryValue,
			m_wszSummary[0],
			sizeof( m_wszSummary[0] ) );
		m_iSummaryLines = 1;
	}
	else if ( m_iMetric == FOF_STATS_FRAGS_PER_MINUTE )
	{
		Q_snprintf(
			summaryValue, sizeof( summaryValue ),
			"%.2f",
			playMinutes > 0.0f ? totalFrags / playMinutes : 0.0f );
		FoFSetStatsSummary(
			"#FoF_Weapon_Stats_FragsMinunte",
			summaryValue,
			m_wszSummary[0],
			sizeof( m_wszSummary[0] ) );
		m_iSummaryLines = 1;
	}
	else if ( m_iMetric == FOF_STATS_ACCURACY )
	{
		Q_snprintf(
			summaryValue, sizeof( summaryValue ),
			"%.2f%%",
			accuracyCount > 0 ?
				totalAccuracy / (float)accuracyCount : 0.0f );
		FoFSetStatsSummary(
			"#FoF_Weapon_Stats_Acc",
			summaryValue,
			m_wszSummary[0],
			sizeof( m_wszSummary[0] ) );
		m_iSummaryLines = 1;
	}
	else if ( m_iMetric == FOF_STATS_WEAPON_DATA )
	{
		wchar_t wszFallback[128];
		V_wcsncpy(
			m_wszSummary[0],
			FoFLauncherLocalize(
				"#FoF_Weapon_Stats_BaseAcc",
				wszFallback,
				sizeof( wszFallback ) ),
			ARRAYSIZE( m_wszSummary[0] ) );
		V_wcsncpy(
			m_wszSummary[1],
			FoFLauncherLocalize(
				"#FoF_Weapon_Stats_BaseDmg",
				wszFallback,
				sizeof( wszFallback ) ),
			ARRAYSIZE( m_wszSummary[1] ) );
		m_iSummaryLines = 2;
	}
}

void CFoFPersonalStatsPanel::DrawSummary()
{
	if ( m_hSummaryFont == vgui::INVALID_FONT )
		return;
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int x = FoFLauncherScalePixel( 10.0f, screenTall );
	const int firstY = FoFLauncherScalePixel( 10.0f, screenTall );
	const int lineTall = vgui::surface()->GetFontTall( m_hSummaryFont );
	for ( int line = 0; line < m_iSummaryLines; ++line )
	{
		FoFLauncherDrawText(
			m_wszSummary[line],
			m_hSummaryFont,
			x,
			firstY + line * lineTall,
			Color( 242, 240, 235, 255 ) );
	}
}

void CFoFPersonalStatsPanel::DrawGraph()
{
	if ( m_Points.Count() <= 0 ||
		m_hGraphFont == vgui::INVALID_FONT )
	{
		return;
	}

	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int graphX = FoFLauncherScalePixel( 10.0f, screenTall );
	const int graphY = FoFLauncherScalePixel( 25.0f, screenTall );
	const int graphWide = FoFLauncherScalePixel( 580.0f, screenTall );
	const int graphTall = FoFLauncherScalePixel( 275.0f, screenTall );
	const int baseline = graphY + graphTall;
	float maximum = 0.0f;
	for ( int i = 0; i < m_Points.Count(); ++i )
		maximum = MAX( maximum, m_Points[i].m_flValue );
	for ( int i = 0; i < m_SecondaryPoints.Count(); ++i )
		maximum = MAX( maximum, m_SecondaryPoints[i].m_flValue );
	if ( maximum <= 0.0f )
		maximum = 1.0f;

	const Color labelColors[] =
	{
		Color( 242, 240, 235, 255 ),
		Color( 244, 204, 0, 255 ),
		Color( 20, 232, 20, 255 )
	};
	const float spacing =
		(float)graphWide / (float)m_Points.Count();
	int previousX = 0;
	int previousY = 0;
	for ( int i = 0; i < m_Points.Count(); ++i )
	{
		const FoFPersonalStatsPoint &point = m_Points[i];
		const int x = graphX + (int)( spacing * i );
		const int y = baseline - (int)(
			(float)graphTall * point.m_flValue / maximum );
		if ( i > 0 )
		{
			vgui::surface()->DrawSetColor( 238, 0, 0, 255 );
			vgui::surface()->DrawLine(
				previousX, previousY, x, y );
		}
		vgui::surface()->DrawSetColor( labelColors[i % 3] );
		vgui::surface()->DrawOutlinedCircle(
			x, y, MAX( FoFLauncherScalePixel(
				0.8f, screenTall ), 1 ), 8 );

		wchar_t wszValue[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			point.m_szValue, wszValue, sizeof( wszValue ) );
		const int valueWide = FoFLauncherTextWide(
			wszValue, m_hGraphFont );
		FoFLauncherDrawText(
			wszValue,
			m_hGraphFont,
			clamp(
				x - valueWide / 2,
				graphX,
				MAX( wide - graphX - valueWide, graphX ) ),
			MAX( y - vgui::surface()->GetFontTall(
				m_hGraphFont ), 0 ),
			labelColors[i % 3] );

		wchar_t wszLabel[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			point.m_szLabel, wszLabel, sizeof( wszLabel ) );
		const int labelWide = FoFLauncherTextWide(
			wszLabel, m_hGraphFont );
		FoFLauncherDrawText(
			wszLabel,
			m_hGraphFont,
			clamp(
				x - labelWide / 2,
				graphX,
				MAX( wide - graphX - labelWide, graphX ) ),
			baseline + FoFLauncherScalePixel(
				(float)( ( i % 3 ) + 1 ) * 5.0f,
				screenTall ),
			labelColors[i % 3] );
		previousX = x;
		previousY = y;
	}

	if ( m_SecondaryPoints.Count() == m_Points.Count() )
	{
		previousX = 0;
		previousY = 0;
		for ( int i = 0; i < m_SecondaryPoints.Count(); ++i )
		{
			const FoFPersonalStatsPoint &point =
				m_SecondaryPoints[i];
			const int x = graphX + (int)( spacing * i );
			const int y = baseline - (int)(
				(float)graphTall *
				point.m_flValue / maximum );
			if ( i > 0 )
			{
				vgui::surface()->DrawSetColor( 242, 170, 0, 255 );
				vgui::surface()->DrawLine(
					previousX, previousY, x, y );
			}
			vgui::surface()->DrawSetColor( 242, 170, 0, 255 );
			vgui::surface()->DrawOutlinedCircle(
				x, y, MAX( FoFLauncherScalePixel(
					0.8f, screenTall ), 1 ), 8 );
			wchar_t wszValue[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				point.m_szValue, wszValue, sizeof( wszValue ) );
			const int valueWide = FoFLauncherTextWide(
				wszValue, m_hGraphFont );
			FoFLauncherDrawText(
				wszValue,
				m_hGraphFont,
				clamp(
					x - valueWide / 2,
					graphX,
					MAX( wide - graphX - valueWide, graphX ) ),
				MAX( y - vgui::surface()->GetFontTall(
					m_hGraphFont ), 0 ),
				Color( 242, 170, 0, 255 ) );
			previousX = x;
			previousY = y;
		}
	}
}
