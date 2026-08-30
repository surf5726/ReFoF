// Standalone FoF voice-chat player list.

#include "cbase.h"
#include "c_playerresource.h"
#include "fof/fof_player_list.h"
#include "iclientmode.h"
#include "iinput.h"
#include "vgui_bitmapbutton.h"
#include "voice_status.h"

#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/ScrollBarSlider.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoFPlayerList *g_pFoFPlayerList = NULL;

static float FoFPlayerListRawScale()
{
	return MAX( (float)ScreenHeight() / 480.0f, 0.1f );
}

static float FoFPlayerListLayoutScale()
{
	// FoF scales this desktop-style dialog without an upper bound.
	// That makes it nearly fill 1080p and larger screens while its stock
	// controls retain much smaller internal metrics.  Preserve the original
	// 1024x768 size (1.6x), but cap further growth to keep the composition
	// balanced at modern resolutions.
	return MIN( FoFPlayerListRawScale(), 1.75f );
}

static int FoFPlayerListScale( int logical )
{
	return RoundFloatToInt( logical * FoFPlayerListLayoutScale() );
}

class CFoFScrollArrowButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFScrollArrowButton, vgui::Button );

public:
	CFoFScrollArrowButton(
		vgui::Panel *parent,
		const char *name,
		bool pointsUp )
		: BaseClass( parent, name, "" )
		, m_bPointsUp( pointsUp )
	{
		SetButtonActivationType( ACTIVATE_ONPRESSED );
		SetContentAlignment( vgui::Label::a_center );
		SetKeyBoardInputEnabled( false );
		// Its bounds are already final client pixels.  Inheriting the parent's
		// proportional flag would make VGUI scale the paint metrics a second
		// time after the ListPanel has positioned the scrollbar.
		SetProportional( false );
	}

	virtual void ApplySchemeSettings( vgui::IScheme *scheme )
	{
		BaseClass::ApplySchemeSettings( scheme );
		SetDefaultBorder(
			scheme->GetBorder( "ScrollBarButtonBorder" ) );
		SetDepressedBorder(
			scheme->GetBorder( "ScrollBarButtonDepressedBorder" ) );
		SetDefaultColor(
			scheme->GetColor(
				"ScrollBarButton.FgColor", Color( 235, 235, 235, 255 ) ),
			scheme->GetColor(
				"ScrollBarButton.BgColor", Color( 65, 65, 65, 255 ) ) );
		SetArmedColor(
			scheme->GetColor(
				"ScrollBarButton.ArmedFgColor", Color( 255, 255, 255, 255 ) ),
			scheme->GetColor(
				"ScrollBarButton.ArmedBgColor", Color( 85, 85, 85, 255 ) ) );
		SetDepressedColor(
			scheme->GetColor(
				"ScrollBarButton.DepressedFgColor", Color( 255, 255, 255, 255 ) ),
			scheme->GetColor(
				"ScrollBarButton.DepressedBgColor", Color( 45, 45, 45, 255 ) ) );
	}

	virtual void OnMouseFocusTicked()
	{
		CallParentFunction( new KeyValues( "MouseFocusTicked" ) );
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		if ( !IsEnabled() || !IsMouseClickEnabled( code ) )
			return;
		if ( IsUseCaptureMouseEnabled() )
		{
			SetSelected( true );
			Repaint();
			vgui::input()->SetMouseCapture( GetVPanel() );
		}
	}

	virtual void OnMouseReleased( vgui::MouseCode code )
	{
		if ( !IsEnabled() || !IsMouseClickEnabled( code ) )
			return;
		if ( IsUseCaptureMouseEnabled() )
		{
			SetSelected( false );
			Repaint();
			vgui::input()->SetMouseCapture( NULL );
		}
		if ( vgui::input()->GetMouseOver() == GetVPanel() )
			SetArmed( true );
	}

	virtual void Paint()
	{
		BaseClass::Paint();

		int wide = 0;
		int tall = 0;
		GetSize( wide, tall );
		vgui::Panel *parent = GetParent();
		if ( parent )
		{
			// A late Scheme pass may temporarily leave stale child bounds.  Paint
			// only against the scrollbar's visible width, never its clipped area.
			wide = MIN( wide, parent->GetWide() );
			tall = MIN( tall, parent->GetWide() );
		}
		const int side = MIN( wide, tall );
		if ( side <= 2 )
			return;

		// Paint the arrow from the live button bounds instead of rendering a
		// Marlett glyph.  It therefore stays inside the scrollbar at every
		// resolution and Windows DPI setting.
		const int margin = MAX( side / 4, 1 );
		const int maxHalfWide = MAX( ( side - 2 * margin ) / 2, 1 );
		const int arrowTall = maxHalfWide + 1;
		int centerX = wide / 2;
		int top = ( tall - arrowTall ) / 2;
		if ( IsDepressed() )
		{
			++centerX;
			++top;
		}

		vgui::surface()->DrawSetColor( GetButtonFgColor() );
		for ( int row = 0; row < arrowTall; ++row )
		{
			const int halfWide = m_bPointsUp
				? row
				: arrowTall - row - 1;
			const int y = top + row;
			const int left = MAX( centerX - halfWide, 1 );
			const int right = MIN( centerX + halfWide + 1, wide - 1 );
			if ( y >= 1 && y < tall - 1 && right > left )
				vgui::surface()->DrawFilledRect( left, y, right, y + 1 );
		}
	}

private:
	bool m_bPointsUp;
};

static void FoFConfigurePlayerListScrollBar(
	vgui::ListPanel *playerList )
{
	if ( !playerList )
		return;

	vgui::ScrollBar *scrollBar =
		playerList->FindControl<vgui::ScrollBar>(
			"VertScrollBar", true );
	if ( !scrollBar )
		return;

	// ListPanel geometry below is supplied in final client pixels.  Prevent a
	// later ScrollBar::ApplySchemeSettings pass from proportionally scaling its
	// stock width once more; the live list-height ratio below owns the width.
	scrollBar->SetProportional( false );

	static const char *buttonNames[2] =
	{
		"FoFScrollUpButton",
		"FoFScrollDownButton"
	};
	for ( int button = 0; button < 2; ++button )
	{
		vgui::Button *oldButton = scrollBar->GetButton( button );
		if ( oldButton &&
			!Q_stricmp( oldButton->GetName(), buttonNames[button] ) )
		{
			continue;
		}

		CFoFScrollArrowButton *newButton =
			new CFoFScrollArrowButton(
				NULL, buttonNames[button], button == 0 );
		scrollBar->SetButton( newButton, button );
		newButton->InvalidateLayout( true, true );
		delete oldButton;
	}

	// Match the bar to the live ListPanel height.  Eleven units is the stock
	// FoF width on its 278-unit list; there is no physical-pixel constant.
	const int scrollWide = MAX( RoundFloatToInt(
		(float)playerList->GetTall() * 11.0f / 278.0f ), 1 );
	scrollBar->SetWide( scrollWide );
	playerList->InvalidateLayout( true );
	// ScrollBar::PerformLayout uses border paint insets that can still reflect
	// the earlier proportional Scheme pass.  Finish from the actual visible
	// bar rectangle so the two arrow buttons are square and wholly internal.
	scrollBar->SetWide( scrollWide );
	scrollBar->InvalidateLayout( true );
	const int scrollTall = scrollBar->GetTall();
	vgui::Button *upButton = scrollBar->GetButton( 0 );
	vgui::Button *downButton = scrollBar->GetButton( 1 );
	if ( upButton )
		upButton->SetBounds( 0, 0, scrollWide, scrollWide );
	if ( downButton )
		downButton->SetBounds(
			0, MAX( scrollTall - scrollWide, 0 ),
			scrollWide, scrollWide );
	vgui::ScrollBarSlider *slider = scrollBar->GetSlider();
	if ( slider )
	{
		slider->SetBounds(
			0,
			scrollWide,
			scrollWide,
			MAX( scrollTall - 2 * scrollWide + 1, 1 ) );
	}
}

CFoFPlayerList::CFoFPlayerList( vgui::Panel *parent )
	: BaseClass( parent, "FoFPlayerList", false )
	, m_pBackground( NULL )
	, m_pPlayerList( NULL )
	, m_pTitle( NULL )
	, m_pVoiceControl( NULL )
	, m_pMuteAll( NULL )
	, m_pOkay( NULL )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hControlFont( vgui::INVALID_FONT )
	, m_iTitleFontTall( 0 )
	, m_iControlFontTall( 0 )
	, m_bRefreshing( false )
{
	SetScheme( "ClientScheme" );
	// The original frame inherits the viewport's proportional mode.  Keeping
	// this false left stock Button/ListPanel text at the unscaled desktop font
	// while the frame geometry was scaled to the 640x480 canvas.
	SetProportional( true );
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );
	SetMoveable( false );
	SetSizeable( false );
	SetPaintBackgroundEnabled( false );
	SetDeleteSelfOnClose( false );

	m_pBackground = new vgui::ImagePanel( this, "mutebg" );
	m_pBackground->SetImage( "mutebg" );
	m_pBackground->SetShouldScaleImage( true );
	m_pBackground->SetMouseInputEnabled( false );
	m_pBackground->SetKeyBoardInputEnabled( false );

	m_pPlayerList = new vgui::ListPanel( this, "PlayerList" );
	m_pPlayerList->SetMultiselectEnabled( false );
	m_pPlayerList->SetAllowUserModificationOfColumns( false );
	m_pPlayerList->SetEmptyListText( "#GameUI_NoOtherPlayersInGame" );
	m_pPlayerList->AddColumnHeader(
		// The original client passes a null display
		// caption: "Name" is the data key, not a visible English header.
		0, "name", NULL, 350,
		vgui::ListPanel::COLUMN_FIXEDSIZE );
	m_pPlayerList->AddColumnHeader(
		// The same applies to Properties.
		1, "properties", NULL, 120,
		vgui::ListPanel::COLUMN_FIXEDSIZE );
	m_pPlayerList->AddActionSignalTarget( this );

	m_pTitle = new vgui::Label(
		this, "Title", "#MuteList_Title" );
	m_pTitle->SetContentAlignment( vgui::Label::a_west );
	m_pTitle->SetMouseInputEnabled( false );

	m_pVoiceControl = new vgui::Button(
		this, "voice_control", "", this, "voice_control" );
	m_pMuteAll = new vgui::Button(
		this,
		"MuteAll",
		"#GameUI_GameMenu_PlayerList",
		this,
		"muteall" );
	m_pOkay = new CBitmapButton( this, "Okay", "" );
	m_pOkay->AddActionSignalTarget( this );
	m_pOkay->SetCommand( "okay" );
	m_pOkay->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	m_pOkay->SetButtonBorderEnabled( false );
	m_pOkay->SetPaintBorderEnabled( false );
	m_pOkay->DrawFocusBox( false );
	// Exact bindings from the FoF constructor
	// in the original client.
	m_pOkay->SetReleasedSound( "ui/release_fire1.wav" );
	m_pOkay->SetArmedSound( "ui/rollover_cyl2.wav" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 500 );
	SetVisible( false );
}

CFoFPlayerList::~CFoFPlayerList()
{
	vgui::ivgui()->RemoveTickSignal( GetVPanel() );
	if ( g_pFoFPlayerList == this )
		g_pFoFPlayerList = NULL;
}

void CFoFPlayerList::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );

	// FoFPlayerList.res contains a legacy pink FgColor, while brighttext=1
	// selects the scheme's bright label colour.
	m_pTitle->SetFgColor( scheme->GetColor(
		"Label.TextBrightColor", Color( 217, 212, 199, 200 ) ) );

	const color32 white = { 255, 255, 255, 255 };
	const color32 armed = { 255, 238, 190, 255 };
	const color32 pressed = { 190, 165, 120, 255 };
	const color32 disabled = { 100, 100, 100, 180 };
	m_pOkay->SetImage(
		CBitmapButton::BUTTON_ENABLED, "vgui/tm_ok", white );
	m_pOkay->SetImage(
		CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/tm_ok", armed );
	m_pOkay->SetImage(
		CBitmapButton::BUTTON_PRESSED, "vgui/tm_ok", pressed );
	m_pOkay->SetImage(
		CBitmapButton::BUTTON_DISABLED, "vgui/tm_ok", disabled );
	FoFConfigurePlayerListScrollBar( m_pPlayerList );
	PerformLayout();
}

void CFoFPlayerList::UpdateScaledFonts()
{
	// Use the actual laid-out panel height as the source of truth.  These
	// explicit pixel faces avoid Scheme/DPI scaling being applied a second
	// time on some Windows configurations.
	const int titleTall = MAX( RoundFloatToInt(
		(float)GetTall() * 18.0f / 388.0f ), 1 );
	const int controlTall = MAX( RoundFloatToInt(
		(float)GetTall() * 10.0f / 388.0f ), 1 );

	if ( m_hTitleFont == vgui::INVALID_FONT ||
		m_iTitleFontTall != titleTall )
	{
		const vgui::HFont font = vgui::surface()->CreateFont();
		if ( font != vgui::INVALID_FONT &&
			vgui::surface()->SetFontGlyphSet(
				font, "Verdana", titleTall, 400, 0, 0,
				vgui::ISurface::FONTFLAG_ANTIALIAS |
					vgui::ISurface::FONTFLAG_DROPSHADOW ) )
		{
			m_hTitleFont = font;
			m_iTitleFontTall = titleTall;
		}
	}
	if ( m_hControlFont == vgui::INVALID_FONT ||
		m_iControlFontTall != controlTall )
	{
		const vgui::HFont font = vgui::surface()->CreateFont();
		if ( font != vgui::INVALID_FONT &&
			vgui::surface()->SetFontGlyphSet(
				font, "Verdana", controlTall, 700, 0, 0,
				vgui::ISurface::FONTFLAG_ANTIALIAS |
					vgui::ISurface::FONTFLAG_DROPSHADOW ) )
		{
			m_hControlFont = font;
			m_iControlFontTall = controlTall;
		}
	}

	if ( m_hTitleFont != vgui::INVALID_FONT )
		m_pTitle->SetFont( m_hTitleFont );
	if ( m_hControlFont != vgui::INVALID_FONT )
	{
		m_pPlayerList->SetFont( m_hControlFont );
		m_pVoiceControl->SetFont( m_hControlFont );
		m_pMuteAll->SetFont( m_hControlFont );
	}
}

void CFoFPlayerList::PerformLayout()
{
	const int panelWide = MAX( FoFPlayerListScale( 467 ), 1 );
	const int panelTall = MAX( FoFPlayerListScale( 388 ), 1 );
	// Keep the centre of the original 640x480 placement while shrinking only
	// its excessive high-resolution extent.  Thus 1024x768 is unchanged and
	// widescreen layouts do not jump toward the upper-left when the cap binds.
	const float rawScale = FoFPlayerListRawScale();
	const int originalCenterX = RoundFloatToInt(
		( 168.0f + 467.0f * 0.5f ) * rawScale );
	const int originalCenterY = RoundFloatToInt(
		( 55.0f + 388.0f * 0.5f ) * rawScale );
	SetBounds(
		clamp( originalCenterX - panelWide / 2,
			0, MAX( ScreenWidth() - panelWide, 0 ) ),
		clamp( originalCenterY - panelTall / 2,
			0, MAX( ScreenHeight() - panelTall, 0 ) ),
		panelWide,
		panelTall );
	UpdateScaledFonts();
	m_pBackground->SetBounds( 0, 0, GetWide(), GetTall() );
	m_pTitle->SetBounds(
		FoFPlayerListScale( 28 ),
		FoFPlayerListScale( 8 ),
		FoFPlayerListScale( 410 ),
		FoFPlayerListScale( 32 ) );
	m_pPlayerList->SetBounds(
		FoFPlayerListScale( 28 ),
		FoFPlayerListScale( 48 ),
		FoFPlayerListScale( 410 ),
		FoFPlayerListScale( 278 ) );
	FoFConfigurePlayerListScrollBar( m_pPlayerList );
	m_pVoiceControl->SetBounds(
		FoFPlayerListScale( 48 ),
		FoFPlayerListScale( 342 ),
		FoFPlayerListScale( 165 ),
		FoFPlayerListScale( 28 ) );
	m_pMuteAll->SetBounds(
		FoFPlayerListScale( 230 ),
		FoFPlayerListScale( 342 ),
		FoFPlayerListScale( 95 ),
		FoFPlayerListScale( 28 ) );
	m_pOkay->SetBounds(
		FoFPlayerListScale( 350 ),
		FoFPlayerListScale( 337 ),
		FoFPlayerListScale( 86 ),
		FoFPlayerListScale( 38 ) );
	m_pOkay->MoveToFront();
	BaseClass::PerformLayout();
}

void CFoFPlayerList::ShowDialog()
{
	RefreshPlayers();
	RefreshVoiceButton();
	PerformLayout();
	SetVisible( true );
	SetEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	MakePopup( false );
	// Visibility can trigger the deferred child Scheme pass.  Reapply the
	// responsive scrollbar geometry after that pass, not just in the frame's
	// earlier layout phase.
	m_pPlayerList->InvalidateLayout( true, true );
	FoFConfigurePlayerListScrollBar( m_pPlayerList );
	MoveToFront();
	RequestFocus();
	if ( ::input )
		::input->DeactivateMouse();
	vgui::input()->SetAppModalSurface( GetVPanel() );
	vgui::surface()->UnlockCursor();
	vgui::surface()->SetCursor( vgui::dc_arrow );
	vgui::surface()->SetCursorAlwaysVisible( true );
}

void CFoFPlayerList::HideDialog()
{
	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
		vgui::input()->ReleaseAppModalSurface();
	SetVisible( false );
	vgui::surface()->SetCursorAlwaysVisible( false );
	if ( ::input && engine->IsInGame() )
		::input->ActivateMouse();
}

void CFoFPlayerList::RefreshVoiceButton()
{
	ConVar *voiceEnable = cvar ? cvar->FindVar( "voice_enable" ) : NULL;
	m_pVoiceControl->SetText(
		voiceEnable && voiceEnable->GetBool()
			? "#FoF.VoiceDisable"
			: "#FoF.VoiceEnable" );
}

void CFoFPlayerList::RefreshPlayers()
{
	if ( !m_pPlayerList )
		return;

	int selectedPlayer = -1;
	if ( m_pPlayerList->GetSelectedItemsCount() > 0 )
	{
		const int item = m_pPlayerList->GetSelectedItem( 0 );
		KeyValues *data = m_pPlayerList->GetItem( item );
		if ( data )
			selectedPlayer = data->GetInt( "playerIndex", -1 );
	}

	m_bRefreshing = true;
	m_pPlayerList->RemoveAll();
	if ( !g_PR || !GetClientVoiceMgr() )
	{
		m_bRefreshing = false;
		return;
	}

	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients;
		++playerIndex )
	{
		if ( !g_PR->IsConnected( playerIndex ) )
			continue;

		KeyValues *row = new KeyValues( "player" );
		row->SetInt( "playerIndex", playerIndex );
		row->SetString( "name", g_PR->GetPlayerName( playerIndex ) );
		if ( GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
			row->SetString( "properties", "Muted" );
		else if ( g_PR->IsFakePlayer( playerIndex ) )
			row->SetString( "properties", "CPU Player" );
		else
			row->SetString( "properties", "" );

		const int item = m_pPlayerList->AddItem(
			row, playerIndex, false, false );
		row->deleteThis();
		if ( playerIndex == selectedPlayer )
			m_pPlayerList->SetSingleSelectedItem( item );
	}
	m_pPlayerList->SortList();
	m_pPlayerList->ResetScrollBar();
	m_bRefreshing = false;
}

void CFoFPlayerList::ToggleSelectedPlayer()
{
	if ( !GetClientVoiceMgr() ||
		m_pPlayerList->GetSelectedItemsCount() <= 0 )
	{
		return;
	}
	const int item = m_pPlayerList->GetSelectedItem( 0 );
	KeyValues *data = m_pPlayerList->GetItem( item );
	if ( !data )
		return;
	const int playerIndex = data->GetInt( "playerIndex", -1 );
	if ( playerIndex <= 0 || playerIndex == GetLocalPlayerIndex() )
		return;
	GetClientVoiceMgr()->SetPlayerBlockedState(
		playerIndex,
		!GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) );
	RefreshPlayers();
}

void CFoFPlayerList::ToggleAllPlayers()
{
	if ( !g_PR || !GetClientVoiceMgr() )
		return;
	bool shouldBlock = false;
	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients;
		++playerIndex )
	{
		if ( g_PR->IsConnected( playerIndex ) &&
			playerIndex != GetLocalPlayerIndex() &&
			!GetClientVoiceMgr()->IsPlayerBlocked( playerIndex ) )
		{
			shouldBlock = true;
			break;
		}
	}
	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients;
		++playerIndex )
	{
		if ( g_PR->IsConnected( playerIndex ) &&
			playerIndex != GetLocalPlayerIndex() )
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(
				playerIndex, shouldBlock );
		}
	}
	RefreshPlayers();
}

void CFoFPlayerList::OnItemSelected()
{
	if ( !m_bRefreshing )
		ToggleSelectedPlayer();
}

void CFoFPlayerList::OnTick()
{
	BaseClass::OnTick();
	if ( IsVisible() )
	{
		RefreshVoiceButton();
		RefreshPlayers();
		FoFConfigurePlayerListScrollBar( m_pPlayerList );
	}
}

void CFoFPlayerList::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "okay" ) )
	{
		HideDialog();
		return;
	}
	if ( !Q_stricmp( command, "voice_control" ) )
	{
		ConVar *voiceEnable = cvar ? cvar->FindVar( "voice_enable" ) : NULL;
		if ( voiceEnable )
			voiceEnable->SetValue( voiceEnable->GetBool() ? 0 : 1 );
		RefreshVoiceButton();
		return;
	}
	if ( !Q_stricmp( command, "muteall" ) )
	{
		ToggleAllPlayers();
		return;
	}
	BaseClass::OnCommand( command );
}

void CFoFPlayerList::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		HideDialog();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

CON_COMMAND( fof_mutelist, "Open the FoF voice blocking player list." )
{
	if ( !g_pFoFPlayerList )
	{
		g_pFoFPlayerList = new CFoFPlayerList(
			g_pClientMode->GetViewport() );
	}
	g_pFoFPlayerList->ShowDialog();
}
