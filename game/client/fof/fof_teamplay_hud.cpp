// FoF Teamplay and objective status panel.

#include "cbase.h"

#include "c_baseplayer.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "const.h"
#include "fof/fof_teamplay_hud.h"
#include "fof/fof_hud.h"
#include "fof/fof_team_menu.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "vgui_avatarimage.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/CircularProgressBar.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// CTeamplayDialog reads these two replicated clocks every frame.  The
// original client registers matching client-side ConVars rather than deriving
// the round clock from the one-shot tp_roundtimer event.
static ConVar fof_round_start_time_client(
	"round_start_time", "0", FCVAR_REPLICATED );
static ConVar fof_round_end_time_client(
	"round_end_time", "0", FCVAR_REPLICATED );

static int FoFTeamplayScale( float value )
{
	const int scaled = RoundFloatToInt(
		value * (float)ScreenHeight() / 480.0f );
	if ( value > 0.0f )
		return MAX( scaled, 1 );
	if ( value < 0.0f )
		return MIN( scaled, -1 );
	return 0;
}

static Color FoFTeamplayTeamColor( int team )
{
	if ( g_PR && team >= 0 )
		return g_PR->GetTeamColor( team );

	if ( team == 2 )
		return Color( 55, 120, 245, 255 );
	if ( team == 3 )
		return Color( 220, 45, 45, 255 );
	return Color( 235, 235, 225, 255 );
}

static const char *FoFTeamplayObjectiveIconName( int objective )
{
	switch ( objective )
	{
	case 1:
		return "obj_elimination";
	case 2:
		return "obj_cap_flag";
	case 3:
		return "obj_cart";
	default:
		return NULL;
	}
}

static void FoFTeamplayDrawText(
	vgui::HFont font,
	const wchar_t *text,
	int x,
	int y,
	const Color &color )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

DECLARE_HUDELEMENT( CHudFoFTeamplayStatus );

CHudFoFTeamplayStatus::CHudFoFTeamplayStatus(
	const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "TeamplayDialog" )
	, m_pExtraTimeBar( NULL )
	, m_pClockFace( NULL )
	, m_pRoundTimeBar( NULL )
	, m_hScoreFont( vgui::INVALID_FONT )
	, m_hTotalFont( vgui::INVALID_FONT )
	, m_hPlaceholderFont( vgui::INVALID_FONT )
	, m_iBotTexture( -1 )
	, m_flExtraTimeEnd( 0.0f )
	, m_flRoundTimeStart( 0.0f )
	, m_flRoundTimeEnd( 0.0f )
	, m_bDialogVisible( false )
	, m_bClockVisible( false )
	, m_bExtraTimerVisible( false )
	, m_bRoundTimerVisible( false )
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
	Q_memset( m_pObjectiveIcons, 0, sizeof( m_pObjectiveIcons ) );
	Q_memset( m_iObjective, 0, sizeof( m_iObjective ) );
	Q_memset( m_iReward, 0, sizeof( m_iReward ) );
	Q_memset( m_flCapturingUntil, 0, sizeof( m_flCapturingUntil ) );

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
	ClearState();
}

CHudFoFTeamplayStatus::~CHudFoFTeamplayStatus()
{
	for ( int playerIndex = 0; playerIndex <= MAX_PLAYERS; ++playerIndex )
		delete m_pAvatars[playerIndex];

	if ( m_iBotTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iBotTexture );
}

void CHudFoFTeamplayStatus::Init()
{
	ClearState();
	ListenForGameEvent( "tp_roundtimer" );
	ListenForGameEvent( "tp_timer" );
	ListenForGameEvent( "tp_teamobj" );
	ListenForGameEvent( "tp_teamreward" );
	ListenForGameEvent( "tp_capturing" );
	ListenForGameEvent( "tp_dlg_show" );
	ListenForGameEvent( "tp_dlg_hide" );
	ListenForGameEvent( "tp_dlg_hide_full" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "game_newmap" );
}

void CHudFoFTeamplayStatus::VidInit()
{
	CHudElement::VidInit();
	ClearState();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	LayoutForScreen();
}

bool CHudFoFTeamplayStatus::ShouldDraw()
{
	return FoFHudCurrentMode() == 2 && m_bDialogVisible &&
		FoFHudShouldDraw() && CHudElement::ShouldDraw();
}

void CHudFoFTeamplayStatus::ApplySchemeSettings(
	vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hScoreFont = scheme->GetFont( "FoFScoreboard", true );
	if ( m_hScoreFont == vgui::INVALID_FONT )
		m_hScoreFont = scheme->GetFont( "MenuFontSmall2", true );
	m_hTotalFont = scheme->GetFont( "MenuFontSmall", true );
	if ( m_hTotalFont == vgui::INVALID_FONT )
		m_hTotalFont = scheme->GetFont( "HudSelectionNumbers4", true );
	m_hPlaceholderFont = scheme->GetFont( "HudSelectionNumbers4", true );
	if ( m_hPlaceholderFont == vgui::INVALID_FONT )
		m_hPlaceholderFont = scheme->GetFont( "Default", true );

	if ( m_iBotTexture < 0 )
	{
		m_iBotTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iBotTexture, "vgui/icon_fof", true, false );
	}

	m_pExtraTimeBar->MakeReadyForUse();
	m_pRoundTimeBar->MakeReadyForUse();
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	LayoutForScreen();
}

void CHudFoFTeamplayStatus::ClearState()
{
	Q_memset( m_iObjective, 0, sizeof( m_iObjective ) );
	Q_memset( m_iReward, 0, sizeof( m_iReward ) );
	Q_memset( m_flCapturingUntil, 0, sizeof( m_flCapturingUntil ) );
	Q_memset( m_pObjectiveIcons, 0, sizeof( m_pObjectiveIcons ) );
	m_flExtraTimeEnd = 0.0f;
	m_flRoundTimeStart = 0.0f;
	m_flRoundTimeEnd = 0.0f;
	m_bDialogVisible = false;
	m_bClockVisible = false;
	m_bExtraTimerVisible = false;
	m_bRoundTimerVisible = false;
	UpdateTimerControls();
}

void CHudFoFTeamplayStatus::LayoutForScreen()
{
	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	if ( screenWide <= 0 || screenTall <= 0 )
		return;

	m_iLastScreenWide = screenWide;
	m_iLastScreenTall = screenTall;
	SetBounds( 0, 0, screenWide, screenTall );

	const int clockSize = FoFTeamplayScale( 30.0f );
	const int clockX = ( screenWide - clockSize ) / 2;
	const int clockY = FoFTeamplayScale( 18.0f );
	m_pExtraTimeBar->SetBounds( clockX, clockY, clockSize, clockSize );
	m_pClockFace->SetBounds( clockX, clockY, clockSize, clockSize );
	m_pRoundTimeBar->SetBounds( clockX, clockY, clockSize, clockSize );
}

void CHudFoFTeamplayStatus::UpdateTimerControls()
{
	if ( !m_pExtraTimeBar || !m_pClockFace || !m_pRoundTimeBar )
		return;

	const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
	const float replicatedRoundEnd =
		fof_round_end_time_client.GetFloat();
	if ( m_bClockVisible && replicatedRoundEnd > 0.0f )
	{
		m_flRoundTimeStart = fof_round_start_time_client.GetFloat();
		m_flRoundTimeEnd = replicatedRoundEnd;
		m_bRoundTimerVisible =
			m_flRoundTimeEnd > m_flRoundTimeStart;
	}

	if ( m_bExtraTimerVisible )
	{
		m_pExtraTimeBar->SetProgress( clamp(
			1.0f - ( m_flExtraTimeEnd - now ) * ( 1.0f / 60.0f ),
			0.0f,
			1.0f ) );
	}
	if ( m_bRoundTimerVisible )
	{
		const float duration = MAX(
			m_flRoundTimeEnd - m_flRoundTimeStart, 1.0f );
		m_pRoundTimeBar->SetProgress( clamp(
			( now - m_flRoundTimeStart ) / duration,
			0.0f,
			1.0f ) );
	}

	m_pExtraTimeBar->SetVisible( m_bExtraTimerVisible );
	m_pClockFace->SetVisible( m_bClockVisible );
	m_pRoundTimeBar->SetVisible( m_bRoundTimerVisible );
}

void CHudFoFTeamplayStatus::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}
	UpdateTimerControls();
}

void CHudFoFTeamplayStatus::UpdateAvatar( int playerIndex )
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

void CHudFoFTeamplayStatus::DrawPlayerIcon(
	int playerIndex, int x, int y, int size )
{
	if ( !g_PR || playerIndex <= 0 || playerIndex > MAX_PLAYERS )
		return;

	if ( g_PR->IsFakePlayer( playerIndex ) )
	{
		if ( m_iBotTexture >= 0 )
		{
			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTexture( m_iBotTexture );
			vgui::surface()->DrawTexturedRect( x, y, x + size, y + size );
		}
		return;
	}

	UpdateAvatar( playerIndex );
	CAvatarImage *avatar = m_pAvatars[playerIndex];
	if ( avatar && avatar->IsValid() )
	{
		avatar->SetAvatarSize( size, size );
		avatar->SetPos( x, y );
		avatar->Paint();
		return;
	}

	vgui::surface()->DrawSetColor( 24, 24, 23, 230 );
	vgui::surface()->DrawFilledRect( x, y, x + size, y + size );
	vgui::surface()->DrawSetColor( 210, 210, 200, 210 );
	vgui::surface()->DrawOutlinedRect( x, y, x + size, y + size );
	wchar_t question[] = L"?";
	int textWide = 0;
	int textTall = 0;
	vgui::surface()->GetTextSize(
		m_hPlaceholderFont, question, textWide, textTall );
	FoFTeamplayDrawText(
		m_hPlaceholderFont,
		question,
		x + ( size - textWide ) / 2,
		y + ( size - textTall ) / 2,
		Color( 220, 220, 210, 255 ) );
}

void CHudFoFTeamplayStatus::DrawTeamRoster(
	int team,
	bool rightAligned,
	int anchorX,
	int y,
	const int *players,
	int playerCount )
{
	if ( !players || playerCount <= 0 )
		return;

	const int slotSize = FoFTeamplayScale( 16.0f );
	const int pitch = FoFTeamplayScale( 17.0f );
	const int inset = FoFTeamplayScale( 3.0f );
	const int plateTall = FoFTeamplayScale( 22.0f );
	const int plateWide = inset * 2 + slotSize +
		( playerCount - 1 ) * pitch;
	const int plateX = rightAligned ? anchorX - plateWide : anchorX;
	const int iconY = y + ( plateTall - slotSize ) / 2;
	const Color teamColor = FoFTeamplayTeamColor( team );
	int alpha = 75;
	if ( gpGlobals && team >= 0 && team < ARRAYSIZE( m_flCapturingUntil ) &&
		m_flCapturingUntil[team] > gpGlobals->curtime )
	{
		const float pulse = 0.5f +
			0.5f * sinf( gpGlobals->realtime * 12.0f );
		alpha = 150 + RoundFloatToInt( pulse * 80.0f );
	}

	DrawBox(
		plateX,
		y,
		plateWide,
		plateTall,
		Color( teamColor.r(), teamColor.g(), teamColor.b(), alpha ),
		1.0f );

	for ( int slot = 0; slot < playerCount; ++slot )
	{
		DrawPlayerIcon(
			players[slot],
			plateX + inset + slot * pitch,
			iconY,
			slotSize );
	}
}

void CHudFoFTeamplayStatus::DrawTeamObjective(
	int team, bool drawOnLeft )
{
	if ( team < 0 || team >= ARRAYSIZE( m_iReward ) )
		return;

	const int centerX = ScreenWidth() / 2;
	const int currentX = centerX + FoFTeamplayScale(
		drawOnLeft ? -66.0f : 44.0f );
	const int totalX = currentX + FoFTeamplayScale( 10.0f );
	const int scoreY = FoFTeamplayScale( 19.0f );
	const int totalY = FoFTeamplayScale( 27.0f );
	const int objectiveX = centerX + FoFTeamplayScale(
		drawOnLeft ? -36.0f : 20.0f );
	const int objectiveY = FoFTeamplayScale( 22.0f );
	const int objectiveSize = FoFTeamplayScale( 20.0f );
	const int reward = m_iReward[team];
	if ( reward > 0 )
	{
		// FoF's teamplay manager stores the cart/objective counter in
		// DT_Team::m_iRoundsWon. SetVigScore/SetDespScore update this field,
		// not the generic scoreboard score in m_iScore.
		C_Team *pTeam = GetGlobalTeam( team );
		wchar_t current[32];
		wchar_t total[32];
		V_snwprintf(
			current,
			ARRAYSIZE( current ),
			L"%i",
			pTeam ? pTeam->GetRoundsWon() : 0 );
		V_snwprintf( total, ARRAYSIZE( total ), L"/%i", reward );
		const Color scoreColor( 205, 205, 198, 255 );
		FoFTeamplayDrawText(
			m_hScoreFont, current, currentX, scoreY, scoreColor );
		FoFTeamplayDrawText(
			m_hTotalFont, total, totalX, totalY, scoreColor );
	}

	CHudTexture *objective = m_pObjectiveIcons[team];
	if ( objective )
	{
		const Color teamColor = FoFTeamplayTeamColor( team );
		objective->DrawSelf(
			objectiveX,
			objectiveY,
			objectiveSize,
			objectiveSize,
			Color( teamColor.r(), teamColor.g(), teamColor.b(), 255 ) );
	}
}

void CHudFoFTeamplayStatus::Paint()
{
	if ( !g_PR || !gpGlobals )
		return;

	int vigilantes[MAX_PLAYERS];
	int desperados[MAX_PLAYERS];
	int vigilanteCount = 0;
	int desperadoCount = 0;
	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients && playerIndex <= MAX_PLAYERS;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) ||
			!g_PR->IsAlive( playerIndex ) )
		{
			continue;
		}

		const int team = g_PR->GetTeam( playerIndex );
		if ( team == 2 )
			vigilantes[vigilanteCount++] = playerIndex;
		else if ( team == 3 )
			desperados[desperadoCount++] = playerIndex;
	}

	const int rosterY = FoFTeamplayScale( 22.0f );
	const int centerX = ScreenWidth() / 2;
	DrawTeamRoster(
		3,
		true,
		centerX - FoFTeamplayScale( 73.0f ),
		rosterY,
		desperados,
		desperadoCount );
	DrawTeamRoster(
		2,
		false,
		centerX + FoFTeamplayScale( 77.0f ),
		rosterY,
		vigilantes,
		vigilanteCount );
	DrawTeamObjective( 3, true );
	DrawTeamObjective( 2, false );
}

void CHudFoFTeamplayStatus::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *name = event->GetName();
	if ( !Q_strcmp( name, "game_newmap" ) )
	{
		ClearState();
		return;
	}

	if ( FoFHudCurrentMode() != 2 )
		return;

	if ( !Q_strcmp( name, "round_start" ) ||
		!Q_strcmp( name, "tp_dlg_hide_full" ) )
	{
		m_bDialogVisible = false;
	}
	else if ( !Q_strcmp( name, "tp_dlg_show" ) )
	{
		m_bDialogVisible = true;
		// A newly constructed original ImagePanel is visible before the first
		// timer event. Preserve that clock-face-only state on initial show.
		m_bClockVisible = true;
	}
	else if ( !Q_strcmp( name, "tp_dlg_hide" ) )
	{
		m_bClockVisible = false;
		m_bExtraTimerVisible = false;
		m_bRoundTimerVisible = false;
	}
	else if ( !Q_strcmp( name, "tp_timer" ) )
	{
		const float extraTime = (float)MAX(
			event->GetInt( "extra_time", 0 ), 0 ) + 1.0f;
		// The event field is the server's absolute end timestamp.  Adding
		// curtime here a second time makes the hand start late by an entire
		// elapsed map time.
		m_flExtraTimeEnd = extraTime;
		m_bClockVisible = true;
		m_bExtraTimerVisible = extraTime > 0.0f;
	}
	else if ( !Q_strcmp( name, "tp_roundtimer" ) )
	{
		const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
		const float roundTime = (float)MAX(
			event->GetInt( "round_time", 0 ), 0 );
		m_flRoundTimeStart = now;
		m_flRoundTimeEnd = now + roundTime;
		m_bClockVisible = true;
		m_bRoundTimerVisible = roundTime > 0.0f;
	}
	else if ( !Q_strcmp( name, "tp_teamobj" ) )
	{
		const int team = event->GetInt( "team", 0 );
		if ( team >= 0 && team < ARRAYSIZE( m_iObjective ) )
		{
			m_iObjective[team] = event->GetInt( "obj", 0 );
			const char *iconName = FoFTeamplayObjectiveIconName(
				m_iObjective[team] );
			m_pObjectiveIcons[team] = iconName
				? gHUD.GetIcon( iconName ) : NULL;
		}
	}
	else if ( !Q_strcmp( name, "tp_teamreward" ) )
	{
		const int team = event->GetInt( "team", 0 );
		if ( team >= 0 && team < ARRAYSIZE( m_iReward ) )
			m_iReward[team] = event->GetInt( "reward", 0 );
	}
	else if ( !Q_strcmp( name, "tp_capturing" ) )
	{
		const int team = event->GetInt( "team", 0 );
		if ( team >= 0 && team < ARRAYSIZE( m_flCapturingUntil ) )
		{
			m_flCapturingUntil[team] =
				( gpGlobals ? gpGlobals->curtime : 0.0f ) + 1.1f;
		}
	}

	UpdateTimerControls();
}
