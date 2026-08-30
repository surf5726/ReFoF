// FoF team-elimination roster, overtime clock and round result message.

#include "cbase.h"

#include "c_baseplayer.h"
#include "c_playerresource.h"
#include "cdll_util.h"
#include "const.h"
#include "engine/IEngineSound.h"
#include "fof/fof_elimination_hud.h"
#include "fof/fof_hud.h"
#include "fof/fof_team_menu.h"
#include "glow_outline_effect.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "vgui_avatarimage.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/CircularProgressBar.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool s_bFoFEliminatedPlayers[MAX_PLAYERS + 1];

bool FoFIsEliminationPlayerOut( int playerIndex )
{
	return playerIndex > 0 && playerIndex <= MAX_PLAYERS &&
		s_bFoFEliminatedPlayers[playerIndex];
}

static int FoFEliminationScale( float value )
{
	return MAX( RoundFloatToInt(
		value * (float)ScreenHeight() / 480.0f ), 1 );
}

static float FoFEliminationRoundDuration()
{
	ConVarRef roundTime( "fof_sv_elm_roundtime", true );
	return roundTime.IsValid() ? MAX( roundTime.GetFloat(), 1.0f ) : 180.0f;
}

static bool FoFEliminationWarmupActive()
{
	static ConVarRef warmup( "fof_warmup", true );
	return warmup.IsValid() && warmup.GetBool();
}

static bool FoFEliminationBattleRoyaleActive()
{
	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	return battleRoyale.IsValid() && battleRoyale.GetBool();
}

static bool FoFRoundEndMessageMode()
{
	const int currentMode = FoFHudCurrentMode();
	return currentMode == 2 || currentMode == 4;
}

static Color FoFEliminationTeamColor( int team )
{
	if ( g_PR && team >= 0 )
		return g_PR->GetTeamColor( team );

	switch ( team )
	{
	case 2:
		return Color( 55, 120, 245, 255 );
	case 3:
		return Color( 220, 45, 45, 255 );
	case 4:
		return Color( 235, 175, 40, 255 );
	case 5:
		return Color( 70, 190, 90, 255 );
	default:
		return Color( 235, 235, 225, 255 );
	}
}

static const char *FoFEliminationWinnerToken( int team )
{
	switch ( team )
	{
	case 2:
		return "#VigRound";
	case 3:
		return "#DespRound";
	case 4:
		return "#BandidoRound";
	case 5:
		return "#RangerRound";
	default:
		return "#NoTeamWins";
	}
}

static const wchar_t *FoFEliminationWinnerFallback( int team )
{
	switch ( team )
	{
	case 2:
		return L"Vigilantes win the round";
	case 3:
		return L"Desperados win the round";
	case 4:
		return L"Bandidos win the round";
	case 5:
		return L"Rangers win the round";
	default:
		return L"No one wins";
	}
}

DECLARE_HUDELEMENT( CHudFoFEliminationStatus );
DECLARE_HUDELEMENT( CHudFoFRoundEndMessage );

CHudFoFEliminationStatus::CHudFoFEliminationStatus(
	const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudFoFEliminationStatus" )
	, m_pExtraTimeBar( NULL )
	, m_pClockFace( NULL )
	, m_pRoundTimeBar( NULL )
	, m_pLeftArrow( NULL )
	, m_pRightArrow( NULL )
	, m_iBotTexture( -1 )
	, m_iCrossTexture( -1 )
	, m_flExtraTimeEnd( 0.0f )
	, m_flEnemyArrowHideAt( 0.0f )
	, m_flRoundEndTime( 0.0f )
	, m_flRoundDuration( 180.0f )
	, m_bProgressVisible( false )
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

	Q_memset( m_pAvatars, 0, sizeof( m_pAvatars ) );
	Q_memset( m_nAvatarFriendsId, 0, sizeof( m_nAvatarFriendsId ) );

	m_pExtraTimeBar = new vgui::CircularProgressBar( this, "bar" );
	m_pExtraTimeBar->SetProportional( false );
	m_pExtraTimeBar->SetFgImage( "AmmoFull" );
	m_pExtraTimeBar->SetBgImage( "AmmoEmpty" );
	m_pExtraTimeBar->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pExtraTimeBar->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pExtraTimeBar->SetMouseInputEnabled( false );
	m_pExtraTimeBar->SetKeyBoardInputEnabled( false );
	m_pExtraTimeBar->SetZPos( 0 );

	m_pClockFace = new vgui::ImagePanel( this, "clock" );
	m_pClockFace->SetProportional( false );
	m_pClockFace->SetShouldScaleImage( true );
	m_pClockFace->SetImage( "clock" );
	m_pClockFace->SetMouseInputEnabled( false );
	m_pClockFace->SetKeyBoardInputEnabled( false );
	m_pClockFace->SetZPos( 1 );

	m_pRoundTimeBar = new vgui::CircularProgressBar( this, "rbar" );
	m_pRoundTimeBar->SetProportional( false );
	m_pRoundTimeBar->SetFgImage( "RoundTFull" );
	m_pRoundTimeBar->SetBgImage( "RoundTEmpty" );
	m_pRoundTimeBar->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pRoundTimeBar->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pRoundTimeBar->SetMouseInputEnabled( false );
	m_pRoundTimeBar->SetKeyBoardInputEnabled( false );
	m_pRoundTimeBar->SetZPos( 2 );

	m_pExtraTimeBar->MakeReadyForUse();
	m_pRoundTimeBar->MakeReadyForUse();
	ClearClockState();
}

CHudFoFEliminationStatus::~CHudFoFEliminationStatus()
{
	for ( int index = 0; index <= MAX_PLAYERS; ++index )
		delete m_pAvatars[index];
	if ( m_iCrossTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iCrossTexture );
}

void CHudFoFEliminationStatus::Init()
{
	ClearClockState();
	ClearEliminatedPlayers();
	ListenForGameEvent( "elm_round_end" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "game_newmap" );
	ListenForGameEvent( "elm_extra_time" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_death" );
}

void CHudFoFEliminationStatus::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	ClearClockState();
	LayoutForScreen();
}

bool CHudFoFEliminationStatus::ShouldDraw()
{
	return FoFHudCurrentMode() == 4 &&
		FoFHudShouldDraw() && CHudElement::ShouldDraw();
}

void CHudFoFEliminationStatus::ApplySchemeSettings(
	vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	m_pLeftArrow = gHUD.GetIcon( "left_arrow" );
	m_pRightArrow = gHUD.GetIcon( "right_arrow" );

	if ( m_iBotTexture < 0 )
	{
		m_iBotTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iBotTexture, "vgui/icon_fof", true, false );
	}
	if ( m_iCrossTexture < 0 )
	{
		m_iCrossTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iCrossTexture, "vgui/ff_crosshair", true, false );
	}
	m_pExtraTimeBar->MakeReadyForUse();
	m_pRoundTimeBar->MakeReadyForUse();
	LayoutForScreen();
}

void CHudFoFEliminationStatus::ClearClockState()
{
	m_flExtraTimeEnd = 0.0f;
	m_flEnemyArrowHideAt = 0.0f;
	m_flRoundEndTime = 0.0f;
	m_flRoundDuration = FoFEliminationRoundDuration();
	m_bProgressVisible = false;
	if ( m_pExtraTimeBar )
		m_pExtraTimeBar->SetVisible( false );
	if ( m_pClockFace )
		m_pClockFace->SetVisible( true );
	if ( m_pRoundTimeBar )
		m_pRoundTimeBar->SetVisible( false );
}

void CHudFoFEliminationStatus::ClearEliminatedPlayers()
{
	Q_memset(
		s_bFoFEliminatedPlayers,
		0,
		sizeof( s_bFoFEliminatedPlayers ) );
}

void CHudFoFEliminationStatus::LayoutForScreen()
{
	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	if ( screenWide <= 0 || screenTall <= 0 )
		return;

	m_iLastScreenWide = screenWide;
	m_iLastScreenTall = screenTall;
	// Enemy reveal arrows are painted by this same HUD panel near the screen
	// edges.  A top-strip-only panel clips every arrow below that strip.
	SetBounds( 0, 0, screenWide, screenTall );

	const int clockSize = FoFEliminationScale( 30.0f );
	const int clockX = ( screenWide - clockSize ) / 2;
	const int clockY = FoFEliminationScale( 7.0f );
	m_pExtraTimeBar->SetBounds( clockX, clockY, clockSize, clockSize );
	m_pClockFace->SetBounds( clockX, clockY, clockSize, clockSize );
	m_pRoundTimeBar->SetBounds( clockX, clockY, clockSize, clockSize );
}

void CHudFoFEliminationStatus::UpdateClock()
{
	if ( !gpGlobals )
		return;

	const float now = gpGlobals->curtime;
	if ( m_bProgressVisible && m_flExtraTimeEnd <= now )
		m_bProgressVisible = false;

	m_pExtraTimeBar->SetVisible( m_bProgressVisible );
	m_pClockFace->SetVisible( true );
	m_pRoundTimeBar->SetVisible( m_bProgressVisible );
	if ( !m_bProgressVisible )
		return;

	const float extraProgress = clamp(
		1.0f - ( m_flExtraTimeEnd - now ) * ( 1.0f / 60.0f ),
		0.0f,
		1.0f );
	const float roundProgress = clamp(
		1.0f - ( m_flRoundEndTime - now ) / MAX( m_flRoundDuration, 1.0f ),
		0.0f,
		1.0f );
	m_pExtraTimeBar->SetProgress( extraProgress );
	m_pRoundTimeBar->SetProgress( roundProgress );
}

void CHudFoFEliminationStatus::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}
	UpdateClock();
}

void CHudFoFEliminationStatus::UpdateAvatar( int playerIndex )
{
	if ( playerIndex <= 0 || playerIndex > MAX_PLAYERS || !g_PR ||
		!g_PR->IsConnected( playerIndex ) ||
		g_PR->IsFakePlayer( playerIndex ) )
	{
		if ( playerIndex > 0 && playerIndex <= MAX_PLAYERS &&
			m_pAvatars[playerIndex] )
		{
			m_pAvatars[playerIndex]->ClearAvatarSteamID();
			m_nAvatarFriendsId[playerIndex] = 0;
		}
		return;
	}

	player_info_t playerInfo;
	Q_memset( &playerInfo, 0, sizeof( playerInfo ) );
	if ( !engine->GetPlayerInfo( playerIndex, &playerInfo ) ||
		playerInfo.fakeplayer || playerInfo.friendsID == 0 ||
		!steamapicontext->SteamUtils() )
	{
		if ( m_pAvatars[playerIndex] )
			m_pAvatars[playerIndex]->ClearAvatarSteamID();
		m_nAvatarFriendsId[playerIndex] = 0;
		return;
	}

	if ( !m_pAvatars[playerIndex] )
	{
		m_pAvatars[playerIndex] = new CAvatarImage();
		m_pAvatars[playerIndex]->SetDrawFriend( false );
		m_pAvatars[playerIndex]->SetColor( Color( 255, 255, 255, 255 ) );
	}
	if ( m_nAvatarFriendsId[playerIndex] == playerInfo.friendsID )
		return;

	CSteamID steamId(
		playerInfo.friendsID,
		1,
		steamapicontext->SteamUtils()->GetConnectedUniverse(),
		k_EAccountTypeIndividual );
	m_pAvatars[playerIndex]->SetAvatarSteamID(
		steamId, k_EAvatarSize32x32 );
	m_nAvatarFriendsId[playerIndex] = playerInfo.friendsID;
}

void CHudFoFEliminationStatus::DrawPlayerSlot(
	int playerIndex, int team, int x, int y, int size )
{
	const int border = FoFEliminationScale( 1.5f );
	const int innerSize = MAX( size - border * 2, 1 );
	const Color teamColor = FoFEliminationTeamColor( team );
	// Warmup presents every slot as unavailable. Once a player dies in a live
	// round, keep the X latched until the next round even if a late respawn
	// updates C_PlayerResource::IsAlive back to true.
	const bool alive = !FoFEliminationWarmupActive() &&
		!FoFIsEliminationPlayerOut( playerIndex ) &&
		g_PR && g_PR->IsAlive( playerIndex );

	vgui::surface()->DrawSetColor( 0, 0, 0, 190 );
	if ( alive )
	{
		vgui::surface()->DrawFilledRect(
			x - 1, y - 1, x + size + 1, y + size + 1 );
	}
	if ( !alive )
	{
		// The antialiased rounded stroke is part of FoF's 32x32 HUD texture;
		// line primitives cannot reproduce its shape or connected spacing.
		if ( m_iCrossTexture >= 0 )
		{
			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTexture( m_iCrossTexture );
			vgui::surface()->DrawTexturedRect(
				x, y, x + size, y + size );
		}
		return;
	}

	vgui::surface()->DrawSetColor( teamColor );
	vgui::surface()->DrawFilledRect( x, y, x + size, y + size );

	const int innerX = x + border;
	const int innerY = y + border;
	UpdateAvatar( playerIndex );
	CAvatarImage *avatar = m_pAvatars[playerIndex];
	if ( avatar && avatar->IsValid() )
	{
		avatar->SetAvatarSize( innerSize, innerSize );
		avatar->SetPos( innerX, innerY );
		avatar->Paint();
		return;
	}

	vgui::surface()->DrawSetColor( 30, 30, 25, 190 );
	vgui::surface()->DrawFilledRect(
		innerX, innerY, innerX + innerSize, innerY + innerSize );
	if ( m_iBotTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iBotTexture );
		vgui::surface()->DrawTexturedRect(
			innerX, innerY, innerX + innerSize, innerY + innerSize );
	}
}

void CHudFoFEliminationStatus::DrawTeamGroup(
	int team,
	bool drawOnLeft,
	int row,
	const int *players,
	int playerCount )
{
	if ( !players || playerCount <= 0 )
		return;

	const int clockSize = FoFEliminationScale( 30.0f );
	const int clockX = ( ScreenWidth() - clockSize ) / 2;
	const int gap = FoFEliminationScale( drawOnLeft ? 13.0f : 7.0f );
	const int plateTall = FoFEliminationScale( 22.0f );
	const int slotSize = FoFEliminationScale( 16.0f );
	const int pitch = FoFEliminationScale( 16.0f );
	const int leftInset = FoFEliminationScale( 8.0f );
	const int rightInset = FoFEliminationScale( 6.0f );
	const int groupWide = leftInset + slotSize +
		( playerCount - 1 ) * pitch + rightInset;
	const int y = FoFEliminationScale( 15.0f ) +
		row * FoFEliminationScale( 20.0f );
	const int contentY = y + ( plateTall - slotSize ) / 2;
	const int startX = drawOnLeft
		? clockX - gap - groupWide
		: clockX + clockSize + gap;

	// The shipped HUD uses a single LMS Label with background type 2. DrawBox
	// uses the same VGUI corner textures, so the plate stays seamless and gains
	// the original rounded, translucent silhouette at every resolution.
	const Color teamColor = FoFEliminationTeamColor( team );
	DrawBox(
		startX,
		y,
		groupWide,
		plateTall,
		Color( teamColor.r(), teamColor.g(), teamColor.b(), 75 ),
		1.0f );

	for ( int slot = 0; slot < playerCount; ++slot )
	{
		DrawPlayerSlot(
			players[slot],
			team,
			startX + leftInset + slot * pitch,
			contentY,
			slotSize );
	}
}

void CHudFoFEliminationStatus::DrawEnemyRevealArrows()
{
	// CHudLMS in the shipped client gives arrows their own six-second
	// elm_extra_time deadline.  Enemy glow visibility is controlled separately
	// by the server's five-second fof_sv_elm_freevision window, so the two
	// effects are intentionally not forced to end on the same frame.
	if ( !g_PR || !gpGlobals || !m_pLeftArrow || !m_pRightArrow ||
		FoFEliminationBattleRoyaleActive() ||
		gpGlobals->curtime > m_flEnemyArrowHideAt )
	{
		return;
	}

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer )
		return;

	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	const int margin = FoFEliminationScale( 100.0f );
	const int arrowBaseSize = FoFEliminationScale( 18.0f );
	const int arrowDrawSize = FoFEliminationScale( 27.0f );
	const int arrowOffset = MAX( arrowBaseSize / 4, 1 );
	const float alphaNear = (float)FoFEliminationScale( 150.0f );
	const float alphaFar = (float)FoFEliminationScale( 300.0f );

	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients && playerIndex <= MAX_PLAYERS;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) ||
			!g_PR->IsAlive( playerIndex ) ||
			FoFIsEliminationPlayerOut( playerIndex ) )
		{
			continue;
		}

		C_BasePlayer *player = UTIL_PlayerByIndex( playerIndex );
		if ( !player || player == localPlayer || player->IsDormant() ||
			player->GetTeamNumber() == localPlayer->GetTeamNumber() )
		{
			continue;
		}

		int screenX = 0;
		int screenY = 0;
		GetVectorInScreenSpace( player->GetAbsOrigin(), screenX, screenY );
		screenX = clamp( screenX, margin, screenWide - margin );
		screenY = clamp( screenY, margin, screenTall - margin );

		const float deltaX = (float)screenX - (float)screenWide * 0.5f;
		const float deltaY = (float)screenY - (float)screenTall * 0.5f;
		const float distance = FastSqrt( deltaX * deltaX + deltaY * deltaY );
		const int alpha = clamp( RoundFloatToInt( RemapValClamped(
			distance, alphaNear, alphaFar, 10.0f, 255.0f ) ), 10, 255 );
		const int team = player->GetTeamNumber();
		const Color color(
			team == 3 ? 255 : 0,
			30,
			team == 2 ? 255 : 0,
			alpha );
		CHudTexture *arrow = screenX > screenWide / 2
			? m_pRightArrow
			: m_pLeftArrow;
		arrow->DrawSelf(
			screenX - arrowOffset,
			screenY - arrowOffset,
			arrowDrawSize,
			arrowDrawSize,
			color );
	}
}

void CHudFoFEliminationStatus::Paint()
{
	if ( !g_PR || !gpGlobals )
		return;

	int teamPlayers[4][MAX_PLAYERS];
	int teamCounts[4] = { 0, 0, 0, 0 };
	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients && playerIndex <= MAX_PLAYERS;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) )
			continue;
		const int team = g_PR->GetTeam( playerIndex );
		if ( team < 2 || team > 5 )
			continue;
		teamPlayers[team - 2][teamCounts[team - 2]++] = playerIndex;
	}

	DrawTeamGroup( 2, true, 0, teamPlayers[0], teamCounts[0] );
	DrawTeamGroup( 3, false, 0, teamPlayers[1], teamCounts[1] );
	DrawTeamGroup( 4, true, 1, teamPlayers[2], teamCounts[2] );
	DrawTeamGroup( 5, false, 1, teamPlayers[3], teamCounts[3] );
	DrawEnemyRevealArrows();
}

void CHudFoFEliminationStatus::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *name = event->GetName();
	if ( !Q_strcmp( name, "game_newmap" ) )
	{
		ClearClockState();
		ClearEliminatedPlayers();
		return;
	}

	if ( FoFHudCurrentMode() != 4 )
		return;

	if ( !Q_strcmp( name, "round_start" ) )
	{
		ClearEliminatedPlayers();
		m_flRoundDuration = FoFEliminationRoundDuration();
		m_flRoundEndTime = ( gpGlobals ? gpGlobals->curtime : 0.0f ) +
			m_flRoundDuration;
		m_flExtraTimeEnd = 0.0f;
		m_bProgressVisible = false;
		UpdateClock();
	}
	else if ( !Q_strcmp( name, "elm_extra_time" ) )
	{
		const float extraTime = (float)MAX(
			event->GetInt( "extra_time", 0 ), 0 );
		m_flExtraTimeEnd = ( gpGlobals ? gpGlobals->curtime : 0.0f ) +
			extraTime;
		m_flEnemyArrowHideAt =
			( gpGlobals ? gpGlobals->curtime : 0.0f ) + 6.0f;
		m_bProgressVisible = extraTime > 0.0f;
		UpdateClock();
	}
	else if ( !Q_strcmp( name, "round_end" ) )
	{
		m_bProgressVisible = false;
		UpdateClock();
	}
	else if ( !Q_strcmp( name, "player_death" ) &&
		!FoFEliminationWarmupActive() )
	{
		const int playerIndex = engine->GetPlayerForUserID(
			event->GetInt( "userid" ) );
		if ( playerIndex > 0 && playerIndex <= MAX_PLAYERS )
			s_bFoFEliminatedPlayers[playerIndex] = true;
	}
}

CHudFoFRoundEndMessage::CHudFoFRoundEndMessage(
	const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "RndEndMsg" )
	, m_hWinnerFont( vgui::INVALID_FONT )
	, m_hMvpFont( vgui::INVALID_FONT )
	, m_WinnerColor( 235, 235, 225, 255 )
	, m_MvpColor( 235, 235, 225, 255 )
	, m_flHideAt( 0.0f )
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
	m_wszWinner[0] = L'\0';
	m_wszMvp[0] = L'\0';
}

void CHudFoFRoundEndMessage::Init()
{
	ClearMessage();
	ListenForGameEvent( "round_end" );
}

void CHudFoFRoundEndMessage::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	ClearMessage();
	LayoutForScreen();
}

bool CHudFoFRoundEndMessage::ShouldDraw()
{
	return FoFRoundEndMessageMode() &&
		m_flHideAt > ( gpGlobals ? gpGlobals->curtime : 0.0f ) &&
		m_wszWinner[0] && FoFHudShouldDraw() &&
		CHudElement::ShouldDraw();
}

void CHudFoFRoundEndMessage::ApplySchemeSettings(
	vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hWinnerFont = scheme->GetFont( "MenuFontMed", true );
	if ( m_hWinnerFont == vgui::INVALID_FONT )
		m_hWinnerFont = scheme->GetFont( "ClientTitleFontSmall", true );
	m_hMvpFont = scheme->GetFont( "MenuFontSmall", true );
	if ( m_hMvpFont == vgui::INVALID_FONT )
		m_hMvpFont = scheme->GetFont( "Default", true );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	LayoutForScreen();
}

void CHudFoFRoundEndMessage::ClearMessage()
{
	m_wszWinner[0] = L'\0';
	m_wszMvp[0] = L'\0';
	m_flHideAt = 0.0f;
}

void CHudFoFRoundEndMessage::LayoutForScreen()
{
	const int panelWide = FoFEliminationScale( 350.0f );
	const int panelTall = FoFEliminationScale( 140.0f );
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	SetBounds(
		( ScreenWidth() - panelWide ) / 2,
		FoFEliminationScale( 100.0f ),
		panelWide,
		panelTall );
}

void CHudFoFRoundEndMessage::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}
	if ( m_flHideAt > 0.0f && gpGlobals &&
		gpGlobals->curtime >= m_flHideAt )
	{
		ClearMessage();
	}
}

void CHudFoFRoundEndMessage::DrawCenteredText(
	vgui::HFont font,
	const wchar_t *text,
	int y,
	const Color &color )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( font, text, wide, tall );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( ( GetWide() - wide ) / 2, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

void CHudFoFRoundEndMessage::Paint()
{
	DrawCenteredText( m_hWinnerFont, m_wszWinner, 0, m_WinnerColor );
	DrawCenteredText(
		m_hMvpFont,
		m_wszMvp,
		FoFEliminationScale( 100.0f ),
		m_MvpColor );
}

void CHudFoFRoundEndMessage::ShowRoundResult( IGameEvent *event )
{
	if ( !event || !g_pVGuiLocalize )
		return;

	// C_RndEndMsg in the shipped client plays this stinger once for every
	// accepted round_end event, before it fills the winner and MVP labels.
	if ( enginesound )
	{
		enginesound->EmitAmbientSound(
			"#/music/round_end_stinger.mp3",
			1.0f,
			PITCH_NORM,
			0 );
	}

	// Mode 4 uses a separate last-player path: TWinner is a player entity
	// index rather than a team number.
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( FoFHudCurrentMode() == 4 && localPlayer &&
		!localPlayer->IsDormant() )
	{
		const wchar_t *lastMan = g_pVGuiLocalize->Find( "#LastMan" );
		V_wcsncpy(
			m_wszWinner,
			lastMan ? lastMan : L"LAST MAN STANDING",
			sizeof( m_wszWinner ) );
		m_WinnerColor = Color( 235, 235, 225, 255 );

		const int winnerIndex = event->GetInt( "TWinner", 0 );
		C_BasePlayer *winnerPlayer = UTIL_PlayerByIndex( winnerIndex );
		const char *winnerName = winnerPlayer ?
			winnerPlayer->GetPlayerName() : NULL;
		if ( winnerName && winnerName[0] &&
			Q_stricmp( winnerName, "ERRORNAME" ) )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode(
				winnerName, m_wszMvp, sizeof( m_wszMvp ) );
		}
		else
		{
			const wchar_t *noWinner =
				g_pVGuiLocalize->Find( "#NoTeamWins" );
			V_wcsncpy(
				m_wszMvp,
				noWinner ? noWinner : L"NO ONE WINS!",
				sizeof( m_wszMvp ) );
		}
		m_MvpColor = Color( 235, 235, 225, 255 );
		m_flHideAt = ( gpGlobals ? gpGlobals->curtime : 0.0f ) + 7.0f;
		return;
	}

	const int winnerTeam = event->GetInt( "TWinner", 0 );
	const wchar_t *winner = g_pVGuiLocalize->Find(
		FoFEliminationWinnerToken( winnerTeam ) );
	V_wcsncpy(
		m_wszWinner,
		winner ? winner : FoFEliminationWinnerFallback( winnerTeam ),
		sizeof( m_wszWinner ) );
	m_WinnerColor = FoFEliminationTeamColor( winnerTeam );
	if ( winnerTeam < 2 || winnerTeam > 5 )
		m_WinnerColor = Color( 235, 235, 225, 255 );

	m_wszMvp[0] = L'\0';
	m_MvpColor = m_WinnerColor;
	const int mvpIndex = event->GetInt( "MVP_Index", 0 );
	if ( g_PR && mvpIndex > 0 && mvpIndex <= MAX_PLAYERS &&
		g_PR->IsConnected( mvpIndex ) )
	{
		const char *playerName = g_PR->GetPlayerName( mvpIndex );
		if ( playerName && playerName[0] &&
			Q_stricmp( playerName, "ERRORNAME" ) )
		{
			wchar_t playerNameWide[128];
			wchar_t scoreWide[32];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				playerName, playerNameWide, sizeof( playerNameWide ) );
			V_snwprintf(
				scoreWide,
				ARRAYSIZE( scoreWide ),
				L"%i",
				event->GetInt( "MVP_Score", 0 ) );
			const wchar_t *format = g_pVGuiLocalize->Find( "#FoF_MVP" );
			if ( format )
			{
				g_pVGuiLocalize->ConstructString(
					m_wszMvp,
					sizeof( m_wszMvp ),
					format,
					2,
					playerNameWide,
					scoreWide );
			}
			else
			{
				V_snwprintf(
					m_wszMvp,
					ARRAYSIZE( m_wszMvp ),
					L"%ls was the most notorious (%ls)",
					playerNameWide,
					scoreWide );
			}
			m_MvpColor = FoFEliminationTeamColor(
				g_PR->GetTeam( mvpIndex ) );
		}
	}

	m_flHideAt = ( gpGlobals ? gpGlobals->curtime : 0.0f ) + 7.0f;
}

void CHudFoFRoundEndMessage::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	if ( !Q_strcmp( event->GetName(), "round_end" ) &&
		FoFRoundEndMessageMode() )
	{
		ShowRoundResult( event );
	}
}
