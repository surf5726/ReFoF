// FoF's Break Bad status display, notices, and key-binding hints.

#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_mode_status.h"
#include "fof/fof_hints.h"
#include "fof/fof_team_menu.h"
#include "fof/fof_player_shared.h"
#include "hud.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <math.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/CircularProgressBar.h>
#include <vgui_controls/ImagePanel.h>
#include "c_baseplayer.h"
#include "cdll_util.h"
#include "c_playerresource.h"
#include "fof/fof_motd.h"
#include "hl2mp_gamerules.h"
#include "vgui_avatarimage.h"
#include "fof/fof_equipment_menu.h"
#include "fof/fof_purchase_menu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudFoFBBStatus );

CHudFoFBBStatus::CHudFoFBBStatus( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HUDBBStatus" )
	, m_pJailBars( NULL )
	, m_pClockFace( NULL )
	, m_pTimeBar( NULL )
	, m_hLargeFont( vgui::INVALID_FONT )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
{
	SetParent( g_pClientMode->GetViewport() );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetHiddenBits( 0 );

	m_pClockFace = new vgui::ImagePanel( this, "clock" );
	m_pClockFace->SetProportional( false );
	m_pClockFace->SetShouldScaleImage( true );
	m_pClockFace->SetImage( "clock" );
	m_pClockFace->SetMouseInputEnabled( false );
	m_pClockFace->SetKeyBoardInputEnabled( false );
	m_pClockFace->SetZPos( 1 );

	m_pTimeBar = new vgui::CircularProgressBar( this, "rbar" );
	m_pTimeBar->SetProportional( false );
	m_pTimeBar->SetFgImage( "AmmoFull" );
	m_pTimeBar->SetBgImage( "AmmoEmpty" );
	m_pTimeBar->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pTimeBar->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pTimeBar->SetMouseInputEnabled( false );
	m_pTimeBar->SetKeyBoardInputEnabled( false );
	m_pTimeBar->SetZPos( 2 );
	m_pTimeBar->MakeReadyForUse();

	SetClockVisible( false );
}

void CHudFoFBBStatus::VidInit()
{
	CHudElement::VidInit();
	m_pJailBars = gHUD.GetIcon( "jailbars" );
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	LayoutForScreen();
	SetClockVisible( false );
}

C_BasePlayer *CHudFoFBBStatus::GetDisplayPlayer() const
{
	return FoFHudObservedPlayer( C_BasePlayer::GetLocalPlayer() );
}

bool CHudFoFBBStatus::IsJailTimerActive( C_BasePlayer *player ) const
{
	return player && gpGlobals &&
		FoFJailTime( player ) > gpGlobals->curtime;
}

bool CHudFoFBBStatus::IsUnarmedTimerActive( C_BasePlayer *player ) const
{
	return player && gpGlobals &&
		FoFUnarmedTime( player ) > gpGlobals->curtime;
}

float CHudFoFBBStatus::GetActiveTimerDeadline(
	C_BasePlayer *player ) const
{
	if ( IsJailTimerActive( player ) )
		return FoFJailTime( player );
	if ( IsUnarmedTimerActive( player ) )
		return FoFUnarmedTime( player );
	return 0.0f;
}

bool CHudFoFBBStatus::ShouldDraw()
{
	C_BasePlayer *player = GetDisplayPlayer();
	return FoFHudCurrentMode() == 3 &&
		( IsJailTimerActive( player ) ||
		  IsUnarmedTimerActive( player ) ) &&
		FoFHudShouldDraw() && CHudElement::ShouldDraw();
}

void CHudFoFBBStatus::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	m_hLargeFont = scheme ? scheme->GetFont( "MenuFontMed", true ) :
		vgui::INVALID_FONT;
	m_pTimeBar->MakeReadyForUse();
	LayoutForScreen();
}

void CHudFoFBBStatus::LayoutForScreen()
{
	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	if ( screenWide <= 0 || screenTall <= 0 )
		return;

	m_iLastScreenWide = screenWide;
	m_iLastScreenTall = screenTall;
	SetBounds( 0, 0, screenWide, screenTall );

	const float scale = (float)screenTall / 480.0f;
	const int clockX = RoundFloatToInt(
		(float)screenWide * 0.5f - 60.0f * scale );
	const int clockY = RoundFloatToInt( 300.0f * scale );
	const int clockSize = MAX( RoundFloatToInt( 30.0f * scale ), 1 );
	const int progressInset = MAX( RoundFloatToInt( 2.0f * scale ), 1 );
	const int progressSize = MAX( RoundFloatToInt( 26.0f * scale ), 1 );

	m_pClockFace->SetBounds(
		clockX, clockY, clockSize, clockSize );
	m_pTimeBar->SetBounds(
		clockX + progressInset,
		clockY + progressInset,
		progressSize,
		progressSize );
}

void CHudFoFBBStatus::SetClockVisible( bool visible )
{
	m_pClockFace->SetVisible( visible );
	m_pTimeBar->SetVisible( visible );
}

void CHudFoFBBStatus::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}

	C_BasePlayer *player = GetDisplayPlayer();
	const float deadline = GetActiveTimerDeadline( player );
	const bool visible = FoFHudCurrentMode() == 3 &&
		deadline > gpGlobals->curtime;
	SetClockVisible( visible );
	if ( visible )
	{
		const float remaining = deadline - gpGlobals->curtime;
		m_pTimeBar->SetProgress(
			clamp( 1.0f - remaining / 60.0f, 0.0f, 1.0f ) );
	}
}

void CHudFoFBBStatus::Paint()
{
	C_BasePlayer *player = GetDisplayPlayer();
	const bool jailed = IsJailTimerActive( player );
	const bool unarmed = IsUnarmedTimerActive( player );
	if ( ( !jailed && !unarmed ) ||
		m_hLargeFont == vgui::INVALID_FONT )
	{
		return;
	}

	if ( jailed && m_pJailBars )
	{
		m_pJailBars->DrawSelf(
			0, 0, ScreenWidth(), ScreenHeight(),
			Color( 255, 255, 255, 255 ) );
	}

	const wchar_t *text = g_pVGuiLocalize->Find(
		jailed ? "#BB_JailTimeLeft" : "#BB_UnarmedTimeLeft" );
	if ( !text || !text[0] )
		return;

	const float scale = (float)ScreenHeight() / 480.0f;
	const int x = RoundFloatToInt(
		(float)ScreenWidth() * 0.5f - 25.0f * scale );
	const int y = RoundFloatToInt( 302.0f * scale );
	vgui::surface()->DrawSetTextFont( m_hLargeFont );
	vgui::surface()->DrawSetTextColor( Color( 205, 205, 205, 255 ) );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

static const wchar_t *FoFLocalizeBBNotice(
	const char *text,
	wchar_t *buffer,
	int bufferBytes )
{
	if ( !text || !text[0] )
	{
		buffer[0] = L'\0';
		return buffer;
	}

	if ( text[0] == '#' )
	{
		const wchar_t *localized = g_pVGuiLocalize->Find( text );
		if ( localized )
			return localized;
	}

	g_pVGuiLocalize->ConvertANSIToUnicode( text, buffer, bufferBytes );
	return buffer;
}

static void FoFBuildBBNoticeText(
	const FoFBBNotice &notice,
	wchar_t *output,
	int outputBytes )
{
	wchar_t formatBuffer[1024];
	const wchar_t *format = FoFLocalizeBBNotice(
		notice.format.String(), formatBuffer, sizeof( formatBuffer ) );
	if ( !format || !format[0] )
	{
		output[0] = L'\0';
		return;
	}

	if ( notice.kind == 0 )
	{
		wchar_t arguments[3][256];
		for ( int i = 0; i < ARRAYSIZE( arguments ); ++i )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode(
				notice.arguments[i].String(),
				arguments[i],
				sizeof( arguments[i] ) );
		}
		g_pVGuiLocalize->ConstructString(
			output,
			outputBytes,
			format,
			3,
			arguments[0],
			arguments[1],
			arguments[2] );
		return;
	}

	if ( notice.kind == 1 )
	{
		FoFReplaceKeyBindings( format, 0, output, outputBytes );
		return;
	}

	V_wcsncpy( output, format, outputBytes );
}

void CHudFoF::PaintBBNotices()
{
	if ( m_hBBNoticeFont == vgui::INVALID_FONT )
		return;

	const float now = gpGlobals->curtime;
	const float screenScale = ScreenHeight() / 480.0f;
	const int baseY = RoundFloatToInt( 390.0f * screenScale );
	const int fontTall = MAX(
		vgui::surface()->GetFontTall( m_hBBNoticeFont ), 1 );

	for ( int noticeIndex = m_BBNotices.Count() - 1;
		noticeIndex >= 0;
		--noticeIndex )
	{
		const FoFBBNotice &notice = m_BBNotices[noticeIndex];
		if ( notice.format.IsEmpty() || now > notice.expiresAt )
			continue;

		wchar_t text[1024];
		FoFBuildBBNoticeText( notice, text, sizeof( text ) );
		if ( !text[0] )
			continue;

		const float age = MAX( now - notice.receivedAt, 0.0f );
		float redGreen = 201.0f;
		if ( age < 4.0f )
			redGreen = fabsf( sinf( now * 8.0f ) ) * 40.0f + 200.0f;
		const int brightness = clamp( static_cast< int >( redGreen ), 0, 255 );
		const int alpha = clamp(
			static_cast< int >(
				255.0f - clamp( age - 9.0f, 0.0f, 1.0f ) * 255.0f ),
			0,
			255 );

		int textWide = 0;
		int textTall = 0;
		vgui::surface()->GetTextSize(
			m_hBBNoticeFont, text, textWide, textTall );
		vgui::surface()->DrawSetTextFont( m_hBBNoticeFont );
		vgui::surface()->DrawSetTextColor(
			Color( brightness, brightness, 200, alpha ) );
		vgui::surface()->DrawSetTextPos(
			( ScreenWidth() - textWide ) / 2,
			baseY + noticeIndex * fontTall );
		vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
	}
}

// FoF objective capture status presentation.

bool CHudFoF::IsCaptureMessageVisible() const
{
	return gpGlobals && m_flCapUntil > gpGlobals->curtime &&
		( m_iCapMode == 1 || m_iCapMode == 2 );
}

static bool FoFCaptureMarkerPositionMatches(
	const FoFCaptureMarker &marker,
	const Vector &position )
{
	return marker.position.x == position.x &&
		marker.position.y == position.y &&
		marker.position.z == position.z;
}

void CHudFoF::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	// The shipped client suppresses this objective-marker channel in
	// Elimination; that mode owns a separate safe-zone presentation.
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 4 )
		return;

	const char *eventName = event->GetName();
	if ( !Q_stricmp( eventName, "cap_zone" ) )
	{
		Vector position(
			event->GetFloat( "pos_x" ),
			event->GetFloat( "pos_y" ),
			event->GetFloat( "pos_z" ) );
		for ( int i = 0; i < m_CaptureMarkers.Count(); ++i )
		{
			if ( FoFCaptureMarkerPositionMatches(
				m_CaptureMarkers[i], position ) )
			{
				return;
			}
		}

		FoFCaptureMarker marker;
		marker.team = event->GetInt( "team" );
		marker.showMode = event->GetInt( "show_mode" );
		marker.position = position;
		m_CaptureMarkers.AddToTail( marker );
		return;
	}

	if ( !Q_stricmp( eventName, "cap_zone_off" ) )
	{
		Vector position(
			event->GetFloat( "pos_x" ),
			event->GetFloat( "pos_y" ),
			event->GetFloat( "pos_z" ) );
		for ( int i = 0; i < m_CaptureMarkers.Count(); ++i )
		{
			if ( FoFCaptureMarkerPositionMatches(
				m_CaptureMarkers[i], position ) )
			{
				m_CaptureMarkers.Remove( i );
				break;
			}
		}
		return;
	}

	if ( !Q_stricmp( eventName, "game_newmap" ) ||
		!Q_stricmp( eventName, "round_end" ) )
	{
		m_CaptureMarkers.Purge();
		m_flCapUntil = 0.0f;
	}
}

void CHudFoF::PaintCaptureMarkers()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer || !localPlayer->IsAlive() ||
		m_hCaptureFont == vgui::INVALID_FONT )
	{
		return;
	}

	const int localTeam = localPlayer->GetTeamNumber();
	for ( int i = 0; i < m_CaptureMarkers.Count(); ++i )
	{
		const FoFCaptureMarker &marker = m_CaptureMarkers[i];
		if ( marker.showMode != 0 && marker.showMode != localTeam )
			continue;

		int screenX = 0;
		int screenY = 0;
		if ( !GetVectorInScreenSpace(
			marker.position, screenX, screenY ) )
		{
			continue;
		}

		const bool capture = marker.team == localTeam;
		CHudTexture *icon = m_pCaptureMarkerIcons[capture ? 0 : 1];
		const int iconWide = icon ? icon->Width() : 0;
		const int iconTall = icon ? icon->Height() : 0;
		const int iconX = screenX - iconWide / 2;
		const int iconY = screenY - iconTall / 2;
		if ( icon )
		{
			icon->DrawSelf(
				iconX, iconY, Color( 255, 255, 255, 255 ) );
		}

		const float distance =
			( marker.position - localPlayer->GetAbsOrigin() ).Length();
		wchar_t distanceText[16];
		V_snwprintf(
			distanceText,
			ARRAYSIZE( distanceText ),
			L"%i",
			(int)( distance * 0.025400052f ) );

		const wchar_t *format = g_pVGuiLocalize->Find(
			capture ? "#FoF_CapIcon" : "#FoF_DefIcon" );
		wchar_t label[256];
		if ( format )
		{
			g_pVGuiLocalize->ConstructString(
				label, sizeof( label ), format, 1, distanceText );
		}
		else
		{
			V_snwprintf(
				label,
				ARRAYSIZE( label ),
				capture ? L"CAPTURE - %sm." : L"DEFEND - %sm.",
				distanceText );
		}

		vgui::surface()->DrawSetTextFont( m_hCaptureFont );
		vgui::surface()->DrawSetTextColor(
			Color( 255, 255, 255, 255 ) );
		vgui::surface()->DrawSetTextPos(
			iconX, iconY + iconTall );
		vgui::surface()->DrawPrintText( label, Q_wcslen( label ) );
	}
}

void CHudFoF::PaintCaptureMessage()
{
	PaintCaptureMarkers();

	if ( !IsCaptureMessageVisible() ||
		m_hCaptureFont == vgui::INVALID_FONT )
	{
		return;
	}

	wchar_t capture[192];
	capture[0] = L'\0';
	if ( m_iCapMode == 2 )
	{
		const wchar_t *blocked =
			g_pVGuiLocalize->Find( "#FoF_Cap_Blocked" );
		if ( blocked )
			V_wcsncpy( capture, blocked, sizeof( capture ) );
		else
			V_wcsncpy( capture, L"CAPTURE BLOCKED", sizeof( capture ) );
	}
	else
	{
		wchar_t count[16];
		V_snwprintf( count, ARRAYSIZE( count ), L"%d", m_iCapCount );

		const wchar_t *format =
			g_pVGuiLocalize->Find( "#FoF_Capturing" );
		if ( format )
		{
			g_pVGuiLocalize->ConstructString(
				capture, sizeof( capture ), format, 1, count );
		}
		else
		{
			V_snwprintf(
				capture, ARRAYSIZE( capture ),
				L"CAPTURING x%d", m_iCapCount );
		}

	}

	int captureWide = 0;
	int captureTall = 0;
	vgui::surface()->GetTextSize(
		m_hCaptureFont, capture, captureWide, captureTall );

	// CapMessage is a 150 x 100 proportional panel at c-75, y=370 in the
	// shipped HUD layout.  Its two localized lines share the same left edge;
	// centering each string independently visibly staggers Chinese text.
	const int x = ScreenWidth() / 2 - FoFHudScale( 75.0f );
	const int y = FoFHudScale( 370.0f );
	vgui::surface()->DrawSetTextFont( m_hCaptureFont );
	vgui::surface()->DrawSetTextColor( Color( 255, 255, 255, 255 ) );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( capture, Q_wcslen( capture ) );

	if ( m_iCapMode == 1 )
	{
		wchar_t progress[16];
		wchar_t progressText[192];
		V_snwprintf( progress, ARRAYSIZE( progress ), L"%d",
			clamp( m_iCapProgress, 0, 100 ) );

		const wchar_t *format =
			g_pVGuiLocalize->Find( "#FoF_Capturing_Progress" );
		if ( format )
		{
			g_pVGuiLocalize->ConstructString(
				progressText, sizeof( progressText ), format, 1, progress );
		}
		else
		{
			V_snwprintf( progressText, ARRAYSIZE( progressText ),
				L"%d%% done", clamp( m_iCapProgress, 0, 100 ) );
		}

		vgui::surface()->DrawSetTextPos( x, y + captureTall );
		vgui::surface()->DrawPrintText(
			progressText, Q_wcslen( progressText ) );
	}
}

// End-of-round "The Good, The Bad and You" presentation.

static const int kGoodBadVisibleLeaders = 10;
static const int kGoodBadRows = 11;

static int FindLocalGoodBadRank(
	const CUtlVector< FoFGoodBadRank > &ranks )
{
	for ( int index = 0; index < ranks.Count(); ++index )
	{
		if ( ranks[index].localPlayer )
			return index;
	}
	return -1;
}

static int GoodBadRankForRow(
	const CUtlVector< FoFGoodBadRank > &ranks,
	int row )
{
	const int leaderCount = MIN( ranks.Count(), kGoodBadVisibleLeaders );
	return row >= 0 && row < leaderCount ? row : -1;
}

static int GoodBadRowCount(
	const CUtlVector< FoFGoodBadRank > &ranks )
{
	return MIN( ranks.Count(), kGoodBadVisibleLeaders );
}

static void DrawGoodBadText(
	vgui::HFont font,
	const wchar_t *text,
	int x,
	int y,
	const Color &color,
	bool centered = false,
	bool rightAligned = false )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( font, text, wide, tall );
	if ( centered )
		x -= wide / 2;
	else if ( rightAligned )
		x -= wide;

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

static int GoodBadMappedTeam( int team )
{
	if ( team < 2 || team > 5 )
		return team;

	char variableName[64];
	Q_snprintf(
		variableName,
		sizeof( variableName ),
		"fof_sv_team_remap_%i",
		team - 1 );
	ConVar *remap = cvar ? cvar->FindVar( variableName ) : NULL;
	return remap ? remap->GetInt() : team;
}

static Color GoodBadTeamColor( int team )
{
	return g_PR
		? g_PR->GetTeamColor( GoodBadMappedTeam( team ) )
		: Color( 255, 220, 0, 255 );
}

static const char *GoodBadTeamName( int team )
{
	switch ( GoodBadMappedTeam( team ) )
	{
	case 3:
		return "DESPERADOS";
	case 4:
		return "BANDIDOS";
	case 5:
		return "RANGERS";
	default:
		return "VIGILANTES";
	}
}

static Color GoodBadPlayerColor( int playerIndex, bool teamplay )
{
	// Shootout uses the neutral FoF gold for every player.  Team Shootout
	// uses the same remapped team colours as the shipped scoreboard.
	if ( !teamplay || !g_PR )
		return Color( 255, 220, 0, 255 );

	return GoodBadTeamColor( g_PR->GetTeam( playerIndex ) );
}

static void FindGoodBadLeadingTeams(
	int currentMode,
	int &winnerTeam,
	int &winnerScore,
	int &runnerUpTeam,
	int &runnerUpScore )
{
	winnerTeam = 2;
	runnerUpTeam = 3;
	winnerScore = g_PR ? g_PR->GetTeamScore( winnerTeam ) : 0;
	runnerUpScore = g_PR ? g_PR->GetTeamScore( runnerUpTeam ) : 0;
	if ( runnerUpScore > winnerScore )
	{
		V_swap( winnerTeam, runnerUpTeam );
		V_swap( winnerScore, runnerUpScore );
	}

	// Objective Teamplay is strictly Vigilantes versus Desperados.  The
	// original four-team scan is only valid for Break Bad and Elimination;
	// including dormant teams 4/5 can turn a 0-0 Teamplay result into a false
	// tie against factions which are not part of the match.
	const int lastTeam = currentMode == 2 ? 3 : 5;
	for ( int team = 4; team <= lastTeam; ++team )
	{
		const int score = g_PR ? g_PR->GetTeamScore( team ) : 0;
		if ( score > winnerScore )
		{
			runnerUpTeam = winnerTeam;
			runnerUpScore = winnerScore;
			winnerTeam = team;
			winnerScore = score;
		}
		else if ( score > runnerUpScore )
		{
			runnerUpTeam = team;
			runnerUpScore = score;
		}
	}
}

void CHudFoF::EnsureGoodBadAvatars()
{
	for ( int row = 0; row < ARRAYSIZE( m_pGoodBadAvatars ); ++row )
	{
		if ( m_pGoodBadAvatars[row] )
			continue;

		m_pGoodBadAvatars[row] = new CAvatarImage();
		m_pGoodBadAvatars[row]->SetDrawFriend( false );
		m_pGoodBadAvatars[row]->SetColor( Color( 255, 255, 255, 255 ) );
	}
}

void CHudFoF::BuildGoodBadRanks()
{
	m_GoodBadRanks.RemoveAll();
	if ( !g_PR || !gpGlobals )
		return;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	const int localIndex = localPlayer ? localPlayer->entindex() : 0;
	const bool courseMode =
		FoFHudCurrentMode() == 6 || FoFIsLocalCourseSession();
	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) )
			continue;

		const int teamNumber = g_PR->GetTeam( playerIndex );
		if ( teamNumber == TEAM_SPECTATOR )
			continue;

		const char *playerName = g_PR->GetPlayerName( playerIndex );
		if ( !playerName || !playerName[0] ||
			!Q_stricmp( playerName, "ERRORNAME" ) )
		{
			continue;
		}

		FoFGoodBadRank rank;
		rank.playerIndex = playerIndex;
		rank.playerName = playerName;
		rank.teamNumber = teamNumber;
		rank.score = g_PR->GetFoFExp( playerIndex );
		rank.frags = g_PR->GetFrags( playerIndex );
		rank.friendsId = 0;
		rank.fakePlayer = g_PR->IsFakePlayer( playerIndex );
		rank.localPlayer = playerIndex == localIndex;

		player_info_t playerInfo;
		Q_memset( &playerInfo, 0, sizeof( playerInfo ) );
		if ( engine->GetPlayerInfo( playerIndex, &playerInfo ) )
		{
			rank.friendsId = playerInfo.friendsID;
			rank.fakePlayer = playerInfo.fakeplayer;
		}
		if ( courseMode && rank.fakePlayer )
			continue;

		m_GoodBadRanks.AddToTail( rank );
	}

	// Snapshot notoriety order at the end of the round.  Frags provide a
	// stable tie-breaker so rows do not exchange places while the card is up.
	for ( int left = 0; left < m_GoodBadRanks.Count(); ++left )
	{
		for ( int right = left + 1;
			right < m_GoodBadRanks.Count();
			++right )
		{
			const bool moveRight =
				m_GoodBadRanks[right].score > m_GoodBadRanks[left].score ||
				( m_GoodBadRanks[right].score == m_GoodBadRanks[left].score &&
				  m_GoodBadRanks[right].frags > m_GoodBadRanks[left].frags );
			if ( moveRight )
			{
				FoFGoodBadRank temporary = m_GoodBadRanks[left];
				m_GoodBadRanks[left] = m_GoodBadRanks[right];
				m_GoodBadRanks[right] = temporary;
			}
		}
	}

	EnsureGoodBadAvatars();
	for ( int row = 0; row < kGoodBadRows; ++row )
	{
		m_pGoodBadAvatars[row]->ClearAvatarSteamID();
		const int rankIndex = GoodBadRankForRow( m_GoodBadRanks, row );
		if ( rankIndex < 0 )
			continue;

		const FoFGoodBadRank &rank = m_GoodBadRanks[rankIndex];
		if ( rank.fakePlayer || rank.friendsId == 0 ||
			!steamapicontext->SteamUtils() )
		{
			continue;
		}

		CSteamID steamId(
			rank.friendsId,
			1,
			steamapicontext->SteamUtils()->GetConnectedUniverse(),
			k_EAccountTypeIndividual );
		m_pGoodBadAvatars[row]->SetAvatarSteamID(
			steamId, k_EAvatarSize32x32 );
	}
}

void CHudFoF::PaintGoodBad()
{
	EnsureMenuTextures();
	EnsureGoodBadAvatars();

	// CGoodBadYou is a proportional 450x285 card.  The source texture is a
	// square whose lower 165 logical pixels are transparent, so clip it to the
	// original panel height rather than stretching the artwork vertically.
	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int boxWide = MAX( RoundFloatToInt( 450.0f * scale ), 1 );
	const int boxTall = MAX( RoundFloatToInt( 285.0f * scale ), 1 );
	const int boxX = ( ScreenWidth() - boxWide ) / 2;
	const int boxY = ( ScreenHeight() - boxTall ) / 2;

	if ( m_iGoodBadBackgroundTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iGoodBadBackgroundTexture );
		vgui::surface()->DrawTexturedSubRect(
			boxX, boxY, boxX + boxWide, boxY + boxTall,
			0.0f, 0.0f, 1.0f, 285.0f / 450.0f );
	}

	const vgui::HFont titleFont =
		m_hEquipmentCreditFont != vgui::INVALID_FONT
			? m_hEquipmentCreditFont : m_hFont;
	const vgui::HFont runnerUpFont =
		m_hEquipmentHeaderFont != vgui::INVALID_FONT
			? m_hEquipmentHeaderFont : m_hSmallFont;
	ConVar *mode = cvar ? cvar->FindVar( "fof_sv_currentmode" ) : NULL;
	const int currentMode = mode ? mode->GetInt() : 0;
	const bool showTeamResults = g_PR &&
		( currentMode == 2 || currentMode == 3 || currentMode == 4 );
	if ( showTeamResults )
	{
		int winnerTeam = 0;
		int winnerScore = 0;
		int runnerUpTeam = 0;
		int runnerUpScore = 0;
		FindGoodBadLeadingTeams(
			currentMode,
			winnerTeam, winnerScore, runnerUpTeam, runnerUpScore );

		if ( winnerScore == runnerUpScore )
		{
			wchar_t noWinnerBuffer[256];
			DrawGoodBadText(
				titleFont,
				Localize( "#NoTeamWins", noWinnerBuffer,
					sizeof( noWinnerBuffer ) ),
				boxX + RoundFloatToInt( 24.0f * scale ),
				boxY + RoundFloatToInt( 10.0f * scale ),
				Color( 245, 245, 245, 255 ) );
		}
		else
		{
			const int resultTeams[] = { winnerTeam, runnerUpTeam };
			const int resultScores[] = { winnerScore, runnerUpScore };
			const char *resultFormats[] =
				{ "#Awards_Team_Score", "#Awards_Team_Score2" };
			const vgui::HFont resultFonts[] =
				{ titleFont, runnerUpFont };
			const float resultY[] = { 10.0f, 40.0f };
			for ( int result = 0; result < ARRAYSIZE( resultTeams ); ++result )
			{
				wchar_t teamName[64];
				wchar_t teamScore[32];
				wchar_t formatBuffer[256];
				wchar_t resultText[512];
				g_pVGuiLocalize->ConvertANSIToUnicode(
					GoodBadTeamName( resultTeams[result] ),
					teamName, sizeof( teamName ) );
				V_snwprintf(
					teamScore,
					ARRAYSIZE( teamScore ),
					currentMode == 3 ? L"$%i" : L"%i",
					resultScores[result] );
				const wchar_t *format = Localize(
					resultFormats[result],
					formatBuffer,
					sizeof( formatBuffer ) );
				g_pVGuiLocalize->ConstructString(
					resultText,
					sizeof( resultText ),
					format,
					2,
					teamName,
					teamScore );
				DrawGoodBadText(
					resultFonts[result],
					resultText,
					boxX + RoundFloatToInt( 24.0f * scale ),
					boxY + RoundFloatToInt( resultY[result] * scale ),
					GoodBadTeamColor( resultTeams[result] ) );
			}
		}
	}
	else
	{
		wchar_t titleBuffer[256];
		const wchar_t *title = Localize(
			"#Awards_GoodBadYou", titleBuffer, sizeof( titleBuffer ) );
		DrawGoodBadText(
			titleFont,
			title,
			boxX + RoundFloatToInt( 24.0f * scale ),
			boxY + RoundFloatToInt( 10.0f * scale ),
			Color( 245, 245, 245, 255 ) );
	}

	const bool teamplay = HL2MPRules() && HL2MPRules()->IsTeamplay();
	const vgui::HFont firstRankFont =
		m_hEquipmentHeaderFont != vgui::INVALID_FONT
			? m_hEquipmentHeaderFont : m_hFont;
	const vgui::HFont rankFont =
		m_hGoodBadRankFont != vgui::INVALID_FONT
			? m_hGoodBadRankFont : m_hSmallFont;
	const int rankRows = GoodBadRowCount( m_GoodBadRanks );
	for ( int row = 0; row < rankRows; ++row )
	{
		const int rankIndex = GoodBadRankForRow( m_GoodBadRanks, row );
		if ( rankIndex < 0 || rankIndex >= m_GoodBadRanks.Count() )
			continue;

		const FoFGoodBadRank &rank = m_GoodBadRanks[rankIndex];
		// Scale the start and pitch independently.  Scaling the accumulated
		// logical coordinate made the 18-unit pitch alternate between 40 and
		// 41 pixels at 1080p, so the lower rows drifted away from the original.
		const int regularRowStart = (int)( 107.0f * scale );
		const int regularRowPitch = MAX( (int)( 18.0f * scale ), 1 );
		const int rowY = row == 0
			? boxY + RoundFloatToInt( 80.0f * scale )
			: boxY + regularRowStart + ( row - 1 ) * regularRowPitch;
		const Color playerColor = GoodBadPlayerColor(
			rank.playerIndex, teamplay );

		const int iconX = boxX + RoundFloatToInt( 25.0f * scale );
		const int iconY = row == 0
			? rowY + MAX( RoundFloatToInt( 0.5f * scale ), 1 )
			: rowY - MAX( (int)( 2.0f * scale ), 1 );
		const int iconSize = MAX( (int)( 15.0f * scale ), 1 );
		CAvatarImage *avatar = m_pGoodBadAvatars[row];
		if ( !rank.fakePlayer && avatar && avatar->IsValid() )
		{
			avatar->SetAvatarSize( iconSize, iconSize );
			avatar->SetPos( iconX, iconY );
			avatar->Paint();
		}
		else if ( rank.fakePlayer && m_iGoodBadBotTexture >= 0 )
		{
			DrawMenuTexture(
				m_iGoodBadBotTexture,
				iconX, iconY, iconSize, iconSize );
		}

		wchar_t playerName[128];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			rank.playerName.String(), playerName, sizeof( playerName ) );
		wchar_t score[32];
		V_snwprintf( score, ARRAYSIZE( score ), L"%i", rank.score );
		const int nameY = row == 0
			? rowY - MAX( (int)( 1.0f * scale ), 1 ) : rowY;
		const int scoreY = row == 0
			? rowY + MAX( (int)( 2.0f * scale ), 1 ) : rowY;
		DrawGoodBadText(
			row == 0 ? firstRankFont : rankFont,
			playerName,
			boxX + RoundFloatToInt( 47.0f * scale ),
			nameY,
			playerColor );
		DrawGoodBadText(
			rankFont,
			score,
			boxX + RoundFloatToInt( 232.0f * scale ),
			scoreY,
			playerColor,
			false,
			true );
	}

	// The eleventh PlayerScoreName control in the shipped panel is not an
	// extra ranking row.  When the local player falls outside the top ten it
	// is reused for one localized summary line, without an avatar, score
	// column, or selection background.
	const int localRank = FindLocalGoodBadRank( m_GoodBadRanks );
	if ( localRank >= kGoodBadVisibleLeaders )
	{
		wchar_t position[32];
		wchar_t score[32];
		wchar_t formatBuffer[256];
		wchar_t localText[512];
		V_snwprintf( position, ARRAYSIZE( position ), L"%i", localRank + 1 );
		V_snwprintf(
			score,
			ARRAYSIZE( score ),
			L"%i",
			m_GoodBadRanks[localRank].score );
		const wchar_t *format = Localize(
			"#Awards_Local_Pos", formatBuffer, sizeof( formatBuffer ) );
		g_pVGuiLocalize->ConstructString(
			localText,
			sizeof( localText ),
			format,
			2,
			position,
			score );
		DrawGoodBadText(
			rankFont,
			localText,
			boxX + RoundFloatToInt( 47.0f * scale ),
			boxY + RoundFloatToInt( 269.0f * scale ),
			GoodBadPlayerColor(
				m_GoodBadRanks[localRank].playerIndex,
				teamplay ) );
	}

	static const char *awardLabels[] =
	{
		"#Awards_NotorietyMinute",
		"#Awards_Killstreak",
		"#Awards_Accuracy",
		"#Awards_Drunkard"
	};
	const int winnerIndices[] =
	{
		m_GoodBadInts[0],
		m_GoodBadInts[2],
		m_GoodBadInts[3],
		m_GoodBadInts[5]
	};
	const float winnerValues[] =
	{
		m_GoodBadFloats[0],
		(float)m_GoodBadInts[1],
		m_GoodBadFloats[1],
		(float)m_GoodBadInts[4]
	};
	const float localValues[] =
	{
		m_GoodBadFloats[2],
		m_GoodBadFloats[3],
		m_GoodBadFloats[4],
		m_GoodBadFloats[5]
	};
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	const int localPlayerIndex = localPlayer ? localPlayer->entindex() : 0;
	const bool localCourse = FoFIsLocalCourseSession();
	const vgui::HFont awardFont =
		m_hGoodBadAwardFont != vgui::INVALID_FONT
			? m_hGoodBadAwardFont : rankFont;
	const vgui::HFont awardLabelFont =
		m_hGoodBadAwardLabelFont != vgui::INVALID_FONT
			? m_hGoodBadAwardLabelFont : m_hSmallFont;
	const vgui::HFont awardLocalFont =
		m_hGoodBadLocalFont != vgui::INVALID_FONT
			? m_hGoodBadLocalFont : m_hSmallFont;

	for ( int award = 0; award < ARRAYSIZE( awardLabels ); ++award )
	{
		const int winnerIndex = winnerIndices[award];
		if ( localCourse && g_PR && winnerIndex > 0 &&
			winnerIndex <= MAX_PLAYERS &&
			g_PR->IsFakePlayer( winnerIndex ) )
		{
			continue;
		}

		const char *winnerName = NULL;
		for ( int rankIndex = 0;
			rankIndex < m_GoodBadRanks.Count();
			++rankIndex )
		{
			if ( m_GoodBadRanks[rankIndex].playerIndex == winnerIndex )
			{
				winnerName = m_GoodBadRanks[rankIndex].playerName.String();
				break;
			}
		}
		if ( g_PR && winnerIndex > 0 && winnerIndex <= MAX_PLAYERS &&
			g_PR->IsConnected( winnerIndex ) &&
			( !winnerName || !winnerName[0] ) )
		{
			winnerName = g_PR->GetPlayerName( winnerIndex );
		}
		if ( !winnerName || !winnerName[0] ||
			!Q_stricmp( winnerName, "ERRORNAME" ) )
		{
			continue;
		}

		char winnerValue[64];
		char localValue[64];
		switch ( award )
		{
		case 0:
			Q_snprintf( winnerValue, sizeof( winnerValue ),
				"%.2f", winnerValues[award] );
			Q_snprintf( localValue, sizeof( localValue ),
				"%.2f", localValues[award] );
			break;
		case 1:
			Q_snprintf( winnerValue, sizeof( winnerValue ),
				"%d", RoundFloatToInt( winnerValues[award] ) );
			Q_snprintf( localValue, sizeof( localValue ),
				"%d", RoundFloatToInt( localValues[award] ) );
			break;
		case 2:
			Q_snprintf( winnerValue, sizeof( winnerValue ),
				"%.2f%%", winnerValues[award] );
			Q_snprintf( localValue, sizeof( localValue ),
				"%.2f%%", localValues[award] );
			break;
		default:
			Q_snprintf( winnerValue, sizeof( winnerValue ),
				"%d", RoundFloatToInt( winnerValues[award] ) );
			Q_snprintf( localValue, sizeof( localValue ),
				"%d", RoundFloatToInt( localValues[award] ) );
			break;
		}

		const int rowY = boxY + RoundFloatToInt(
			( 88.0f + award * 50.0f ) * scale );
		const int laurelSize = MAX( RoundFloatToInt( 50.0f * scale ), 1 );
		if ( m_iGoodBadLaurelTexture >= 0 )
		{
			DrawMenuTexture(
				m_iGoodBadLaurelTexture,
				boxX + RoundFloatToInt( 239.0f * scale ),
				rowY - RoundFloatToInt( 10.0f * scale ),
				laurelSize,
				laurelSize );
		}

		wchar_t awardLabelBuffer[128];
		const wchar_t *awardLabel = Localize(
			awardLabels[award],
			awardLabelBuffer,
			sizeof( awardLabelBuffer ) );
		DrawGoodBadText(
			awardLabelFont,
			awardLabel,
			boxX + RoundFloatToInt( 264.0f * scale ),
			rowY + RoundFloatToInt( 2.0f * scale ),
			Color( 225, 220, 205, 255 ),
			true );

		char winnerLine[256];
		Q_snprintf(
			winnerLine,
			sizeof( winnerLine ),
			"%s: %s",
			winnerName,
			winnerValue );
		wchar_t winnerLineWide[256];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			winnerLine, winnerLineWide, sizeof( winnerLineWide ) );
		DrawGoodBadText(
			awardFont,
			winnerLineWide,
			boxX + RoundFloatToInt( 294.0f * scale ),
			rowY,
			GoodBadPlayerColor( winnerIndex, teamplay ) );

		// Original CGoodBadYou suppresses the redundant local line when the
		// local player won the award, and also hides zero-value local results.
		if ( localPlayerIndex == winnerIndex || localValues[award] <= 0.0f )
			continue;

		wchar_t localValueWide[64];
		wchar_t localFormatBuffer[128];
		wchar_t localOutput[256];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			localValue, localValueWide, sizeof( localValueWide ) );
		const wchar_t *localFormat = Localize(
			"#Awards_Local",
			localFormatBuffer,
			sizeof( localFormatBuffer ) );
		g_pVGuiLocalize->ConstructString(
			localOutput,
			sizeof( localOutput ),
			localFormat,
			1,
			localValueWide );
		DrawGoodBadText(
			awardLocalFont,
			localOutput,
			boxX + RoundFloatToInt( 310.0f * scale ),
			rowY + RoundFloatToInt( 14.0f * scale ),
			Color( 220, 216, 205, 255 ) );
	}
}

// FoF equipment-grant overlay used by CHudVersus.

static const float FOF_EQUIP_ITEMS_LIFETIME = 7.0f;
static const int FOF_EQUIP_ITEM_COUNT = 5;

DECLARE_HUDELEMENT( CHudVersus );

CHudVersus::CHudVersus( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudVersus" )
	, m_iEquipItemCount( 0 )
	, m_flEquipItemsExpireTime( 0.0f )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
{
	SetParent( g_pClientMode->GetViewport() );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetHiddenBits( 0 );

	for ( int item = 0; item < FOF_EQUIP_ITEM_COUNT; ++item )
	{
		char panelName[24];
		Q_snprintf( panelName, sizeof( panelName ), "EquipItem%d", item );
		m_pEquipItems[item] = new vgui::ImagePanel( this, panelName );
		m_pEquipItems[item]->SetShouldScaleImage( true );
		m_pEquipItems[item]->SetShouldCenterImage( true );
		m_pEquipItems[item]->SetMouseInputEnabled( false );
		m_pEquipItems[item]->SetVisible( false );
	}
}

void CHudVersus::Init()
{
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "game_newmap" );
	ClearEquipItems();
}

void CHudVersus::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	ClearEquipItems();
	LayoutEquipItems();
}

bool CHudVersus::ShouldDraw()
{
	const int currentMode = FoFHudCurrentMode();
	if ( currentMode != 4 && currentMode != 5 && currentMode != 6 )
		return false;

	if ( m_iEquipItemCount <= 0 ||
		m_flEquipItemsExpireTime <= gpGlobals->curtime )
	{
		return false;
	}

	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutEquipItems();
	}

	return CHudElement::ShouldDraw();
}

void CHudVersus::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *eventName = event->GetName();
	if ( !Q_stricmp( eventName, "round_start" ) ||
		!Q_stricmp( eventName, "game_newmap" ) )
	{
		ClearEquipItems();
	}
}

const char *CHudVersus::FindEquipItemMaterial( int itemId ) const
{
	// The Versus-only dynamite-belt item is absent from the normal purchase
	// catalogue, but FoF still presents it in the arena grant overlay.
	if ( itemId == 17 )
		return "vgui/dynamite_belt";

	for ( int item = 0; item < FoFEquipmentItemCount(); ++item )
	{
		if ( FoFEquipmentItemId( item ) == itemId )
			return FoFEquipmentItemMaterial( item );
	}

	return FoFPurchaseItemMaterial( itemId );
}

void CHudVersus::ReceiveEquipItem( int itemId )
{
	// The -1 record starts a new grant list.  FoF sets the seven-second
	// deadline here, then receives each item in a separate HudEquipItems record.
	if ( itemId == -1 )
	{
		ClearEquipItems();
		m_flEquipItemsExpireTime = gpGlobals->curtime +
			FOF_EQUIP_ITEMS_LIFETIME;
		SetAlpha( 255 );
		return;
	}

	if ( m_iEquipItemCount >= FOF_EQUIP_ITEM_COUNT )
		return;

	const char *material = FindEquipItemMaterial( itemId );
	if ( !material || !material[0] )
		return;

	// ImagePanel resolves names relative to materials/vgui.  The shared item
	// catalogues keep the full "vgui/..." name for DrawSetTextureFile, so
	// passing that same string here asks VGUI for "vgui/vgui/..." and renders
	// its purple error texture.  HudVersus is the only catalogue consumer that
	// uses ImagePanel::SetImage; remove only its relative-directory prefix.
	const char *imageName = material;
	if ( !Q_strnicmp( imageName, "vgui/", 5 ) )
		imageName += 5;

	vgui::ImagePanel *panel = m_pEquipItems[m_iEquipItemCount];
	panel->SetImage( imageName );
	panel->SetVisible( true );
	++m_iEquipItemCount;
	LayoutEquipItems();
}

void CHudVersus::ClearEquipItems()
{
	m_iEquipItemCount = 0;
	m_flEquipItemsExpireTime = 0.0f;
	for ( int item = 0; item < FOF_EQUIP_ITEM_COUNT; ++item )
	{
		if ( m_pEquipItems[item] )
			m_pEquipItems[item]->SetVisible( false );
	}
}

void CHudVersus::LayoutEquipItems()
{
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	SetBounds( 0, 0, m_iLastScreenWide, m_iLastScreenTall );

	const int itemWide = FoFHudScale( 100.0f );
	const int itemTall = FoFHudScale( 25.0f );
	const int itemStep = FoFHudScale( 27.0f );
	const int itemX = FoFHudScale( 170.0f );
	const int bottomInset = FoFHudScale( 50.0f );

	for ( int item = 0; item < FOF_EQUIP_ITEM_COUNT; ++item )
	{
		vgui::ImagePanel *panel = m_pEquipItems[item];
		if ( !panel )
			continue;

		panel->SetBounds(
			itemX,
			m_iLastScreenTall - bottomInset - itemStep * item,
			itemWide,
			itemTall );
	}
}

void FoFHudEquipItemReceive( int itemId )
{
	CHudVersus *hud = GET_HUDELEMENT( CHudVersus );
	if ( hud )
		hud->ReceiveEquipItem( itemId );
}

int FoFHudEquipItemCount()
{
	CHudVersus *hud = GET_HUDELEMENT( CHudVersus );
	return hud ? hud->GetEquipItemCount() : 0;
}
