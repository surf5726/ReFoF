// FoF HUD lifecycle coordinator and shared presentation state.

#include "cbase.h"
// Main FoF HUD implementation.
#include "fof/fof_hud.h"
#include "fof/fof_hud_menu.h"
#include "fof/fof_loadout_editor.h"
#include "fof/fof_team_class_menu.h"
#include "fof/fof_team_menu.h"
#include "fof/fof_hud_player_status.h"
#include "hl2mp_gamerules.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "c_basecombatweapon.h"
#include "c_baseviewmodel.h"
#include "c_playerresource.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "engine/IEngineSound.h"
#include "filesystem.h"
#include "game/client/iviewport.h"
#include "hl2mp_weapon_parse.h"
#include "fof/fof_weapon_properties.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_motd.h"
#include "hud.h"
#include "ienginevgui.h"
#include "iinput.h"
#include "KeyValues.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier0/vprof.h"
#include "vgui_bitmapbutton.h"
#include "vgui_avatarimage.h"
#include "vgui_video.h"
#include "viewport_panel_names.h"
#include <vgui/ILocalize.h>
#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CircularProgressBar.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudFoF );

// Equipment is painted on its own VGUI layer so BuyMenuDM's modal alpha is
// established before ISurface chooses the material blend mode.  CHudFoF also
// paints unrelated HUD notices, so lowering the coordinator's alpha would
// incorrectly fade those elements as well.
class CFoFEquipmentPaintPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFoFEquipmentPaintPanel, vgui::Panel );

public:
	CFoFEquipmentPaintPanel( CHudFoF *owner )
		: BaseClass( owner, "FoFEquipmentPaintLayer" )
		, m_pOwner( owner )
	{
		SetPaintBackgroundEnabled( false );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetZPos( 0 );
		SetVisible( false );
	}

	virtual void Paint()
	{
		if ( m_pOwner )
			m_pOwner->PaintEquipmentMenu();
	}

private:
	CHudFoF *m_pOwner;
};

// FoF's VideoPanelFoF is a persistent slide child.  It uses the stock video
// material creation path, but unlike VideoPanel it loops, ignores a false
// Update() result, and never closes or deletes itself when playback ends.

CHudFoF::CHudFoF( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudFoF" )
	, m_iNextWrappedTextSlot( 0 )
	, m_hFont( vgui::INVALID_FONT )
	, m_hSmallFont( vgui::INVALID_FONT )
	, m_hHintFont( vgui::INVALID_FONT )
	, m_hCaptureFont( vgui::INVALID_FONT )
	, m_hBBNoticeFont( vgui::INVALID_FONT )
	, m_hBBMarkerFont( vgui::INVALID_FONT )
	, m_hTeamIntroFont( vgui::INVALID_FONT )
	, m_iTeamIntroFontTall( 0 )
	, m_hTeamClassFont( vgui::INVALID_FONT )
	, m_hHitReconFont( vgui::INVALID_FONT )
	, m_hSourceTVPlayerFont( vgui::INVALID_FONT )
	, m_hEquipmentCreditFont( vgui::INVALID_FONT )
	, m_hEquipmentCostFont( vgui::INVALID_FONT )
	, m_hEquipmentHeaderFont( vgui::INVALID_FONT )
	, m_hEquipmentFooterFont( vgui::INVALID_FONT )
	, m_hEquipmentHelpTitleFont( vgui::INVALID_FONT )
	, m_iEquipmentHelpTitleFontTall( 0 )
	, m_hEquipmentHelpFont( vgui::INVALID_FONT )
	, m_hPurchaseTitleFont( vgui::INVALID_FONT )
	, m_hPurchaseWarningFont( vgui::INVALID_FONT )
	, m_hPurchaseCashFont( vgui::INVALID_FONT )
	, m_hPurchasePresetFont( vgui::INVALID_FONT )
	, m_hPurchasePriceFont( vgui::INVALID_FONT )
	, m_hPurchaseFooterFont( vgui::INVALID_FONT )
	, m_hLoadoutEditorItemFont( vgui::INVALID_FONT )
	, m_hLoadoutEditorControlFont( vgui::INVALID_FONT )
	, m_hGoodBadRankFont( vgui::INVALID_FONT )
	, m_hGoodBadAwardFont( vgui::INVALID_FONT )
	, m_hGoodBadAwardLabelFont( vgui::INVALID_FONT )
	, m_hGoodBadLocalFont( vgui::INVALID_FONT )
	, m_pSlideBack( NULL )
	, m_pSlideForward( NULL )
	, m_pSlideClose( NULL )
	, m_bMenuVisible( false )
	, m_bLocalMenu( false )
	, m_iMenuKind( FOF_MENU_NONE )
	, m_pEquipmentPaintPanel( NULL )
	, m_pPurchasePaintPanel( NULL )
	, m_pLoadoutEditorPaintPanel( NULL )
	, m_pTeamClassPaintPanel( NULL )
	, m_pTeamIntroButton( NULL )
	, m_pTeamClassCloseButton( NULL )
	, m_pEquipmentOkayButton( NULL )
	, m_pEquipmentCloseButton( NULL )
	, m_pEquipmentHelpFrame( NULL )
	, m_pEquipmentHelpTitle( NULL )
	, m_pEquipmentHelpText( NULL )
	, m_pEquipmentHelpCloseButton( NULL )
	, m_pPurchasePageUpButton( NULL )
	, m_pPurchasePageDownButton( NULL )
	, m_pPurchaseCloseButton( NULL )
	, m_pPurchaseSwitchButton( NULL )
	, m_pPurchaseEditorButton( NULL )
	, m_pPurchaseFileList( NULL )
	, m_pLoadoutEditorName( NULL )
	, m_pLoadoutEditorFilename( NULL )
	, m_pLoadoutEditorSaveButton( NULL )
	, m_pLoadoutEditorDeleteButton( NULL )
	, m_pLoadoutEditorCancelButton( NULL )
	, m_iTeamBackgroundTexture( -1 )
	, m_iTeamIntroTexture( -1 )
	, m_iEquipmentBackgroundTexture( -1 )
	, m_iEquipmentItemBackgroundTexture( -1 )
	, m_iEquipmentOkayTexture( -1 )
	, m_iPurchaseEnabledTexture( -1 )
	, m_iPurchaseDisabledTexture( -1 )
	, m_iPurchaseMouseOverTexture( -1 )
	, m_iPurchaseArrowUpTexture( -1 )
	, m_iPurchaseArrowDownTexture( -1 )
	, m_iPurchaseEditTexture( -1 )
	, m_iGoodBadBackgroundTexture( -1 )
	, m_iGoodBadBotTexture( -1 )
	, m_iGoodBadLaurelTexture( -1 )
	, m_iTeamAuto2Texture( -1 )
	, m_iTeamClassBackgroundTexture( -1 )
	, m_flMenuReadyAt( 0.0f )
	, m_iSelectedGearCount( 0 )
	, m_iSelectedAimItem( 43 )
	, m_iEquipmentHelpItem( -1 )
	, m_iPurchasePage( 0 )
	, m_iPurchaseFilter( 0 )
	, m_bPurchaseQuickLayout( true )
	, m_bPurchaseEditMode( false )
	, m_bLoadoutEditorVisible( false )
	, m_iLoadoutEditorSourceOrder( -1 )
	, m_iLoadoutEditorItemCount( 0 )
	, m_bHintVisible( false )
	, m_iHintMode( 99 )
	, m_flHintStartedAt( 0.0f )
	, m_flHintPromptAt( 0.0f )
	, m_iSlideBackgroundTexture( -1 )
	, m_bSlideVisible( false )
	, m_bSlidePending( false )
	, m_iSlidePage( 0 )
	, m_iSlidePageCount( 0 )
	, m_iIconCommTexture( -1 )
	, m_flCircleProgressStart( 0.0f )
	, m_flCircleProgressEnd( 0.0f )
	, m_pCircleProgressBar( NULL )
	, m_iCapCount( 0 )
	, m_iCapMode( 0 )
	, m_iCapProgress( 0 )
	, m_flCapUntil( 0.0f )
	, m_bGoodBadVisible( false )
	, m_iBowAttempts( 0 )
	, m_iBowHits( 0 )
	, m_iStatNewValue( 0 )
	, m_iStatOldValue( 0 )
	, m_bStatVisible( false )
	, m_flStatPromptAt( 0.0f )
	, m_bTeamMenuOffered( false )
	, m_bWasInitialSpectator( false )
	, m_bIntentionalSpectator( false )
	, m_iTeamFactionMask( -1 )
{
	for ( int i = 0; i < ARRAYSIZE( m_hSlideFonts ); ++i )
		m_hSlideFonts[i] = vgui::INVALID_FONT;
	Q_memset( m_pTeamMenuButtons, 0, sizeof( m_pTeamMenuButtons ) );
	Q_memset( m_pTeamClassButtons, 0,
		sizeof( m_pTeamClassButtons ) );
	Q_memset( m_pEquipmentMenuButtons, 0, sizeof( m_pEquipmentMenuButtons ) );
	Q_memset( m_pEquipmentHelpButtons, 0, sizeof( m_pEquipmentHelpButtons ) );
	Q_memset( m_pPurchasePresetButtons, 0, sizeof( m_pPurchasePresetButtons ) );
	Q_memset( m_pPurchaseFilterButtons, 0, sizeof( m_pPurchaseFilterButtons ) );
	Q_memset( m_pLoadoutEditorItemButtons, 0,
		sizeof( m_pLoadoutEditorItemButtons ) );
	Q_memset( m_pLoadoutEditorSlotButtons, 0,
		sizeof( m_pLoadoutEditorSlotButtons ) );
	Q_memset( m_pCrateMenuButtons, 0, sizeof( m_pCrateMenuButtons ) );
	Q_memset( m_iTeamButtonTextures, 0xff, sizeof( m_iTeamButtonTextures ) );
	Q_memset( m_iTeamClassItemTextures, 0xff,
		sizeof( m_iTeamClassItemTextures ) );
	Q_memset( m_iEquipmentItemTextures, 0xff, sizeof( m_iEquipmentItemTextures ) );
	Q_memset( m_iPurchaseItemTextures, 0xff, sizeof( m_iPurchaseItemTextures ) );
	Q_memset( m_iCrateItemTextures, 0xff, sizeof( m_iCrateItemTextures ) );
	Q_memset( m_pGoodBadAvatars, 0, sizeof( m_pGoodBadAvatars ) );
	Q_memset( m_pCaptureMarkerIcons, 0,
		sizeof( m_pCaptureMarkerIcons ) );
	Q_memset( m_pBBMarkerIcons, 0, sizeof( m_pBBMarkerIcons ) );
	Q_memset( m_SelectedGearItems, 0xff, sizeof( m_SelectedGearItems ) );
	Q_memset( m_TeamClassItemStyles, 0,
		sizeof( m_TeamClassItemStyles ) );
	Q_memset( m_TeamClassValid, 0,
		sizeof( m_TeamClassValid ) );
	Q_memset( m_LastPresetItems, 0, sizeof( m_LastPresetItems ) );
	Q_memset( m_LoadoutEditorItems, 0xff,
		sizeof( m_LoadoutEditorItems ) );
	Q_memset( m_GoodBadFloats, 0, sizeof( m_GoodBadFloats ) );
	Q_memset( m_GoodBadInts, 0, sizeof( m_GoodBadInts ) );
	Q_memset( m_WeaponDamage, 0, sizeof( m_WeaponDamage ) );
	SetParent( g_pClientMode->GetViewport() );
	m_pEquipmentPaintPanel = new CFoFEquipmentPaintPanel( this );
	m_pPurchasePaintPanel = new CFoFPurchasePaintPanel( this );
	m_pLoadoutEditorPaintPanel =
		new CFoFLoadoutEditorPaintPanel( this );
	m_pTeamClassPaintPanel = FoFCreateTeamClassPaintPanel( this );
	m_pSlideBack = new CBitmapButton(
		this, "FoFSlideBack", "" );
	m_pSlideBack->AddActionSignalTarget( this );
	m_pSlideBack->SetCommand( "go_back" );
	m_pSlideBack->SetMouseInputEnabled( true );
	m_pSlideBack->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	m_pSlideBack->SetArmedSound( "ui/rollover_cyl2.wav" );
	m_pSlideBack->SetReleasedSound( "ui/release_fire2.wav" );
	m_pSlideBack->SetButtonBorderEnabled( false );
	m_pSlideBack->SetPaintBorderEnabled( false );
	m_pSlideBack->DrawFocusBox( false );
	m_pSlideBack->SetZPos( 20 );
	m_pSlideForward = new CBitmapButton(
		this, "FoFSlideForward", "" );
	m_pSlideForward->AddActionSignalTarget( this );
	m_pSlideForward->SetCommand( "go_forward" );
	m_pSlideForward->SetMouseInputEnabled( true );
	m_pSlideForward->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	m_pSlideForward->SetArmedSound( "ui/rollover_cyl1.wav" );
	m_pSlideForward->SetReleasedSound( "ui/release_fire1.wav" );
	m_pSlideForward->SetButtonBorderEnabled( false );
	m_pSlideForward->SetPaintBorderEnabled( false );
	m_pSlideForward->DrawFocusBox( false );
	m_pSlideForward->SetZPos( 20 );
	m_pSlideClose = new CBitmapButton(
		this, "FoFSlideClose", "" );
	m_pSlideClose->AddActionSignalTarget( this );
	m_pSlideClose->SetCommand( "vguicancel" );
	m_pSlideClose->SetMouseInputEnabled( true );
	m_pSlideClose->SetButtonActivationType(
		vgui::Button::ACTIVATE_ONPRESSED );
	m_pSlideClose->SetArmedSound( "ui/rollover_cyl1.wav" );
	m_pSlideClose->SetReleasedSound( "ui/release_fire1.wav" );
	m_pSlideClose->SetButtonBorderEnabled( false );
	m_pSlideClose->SetPaintBorderEnabled( false );
	m_pSlideClose->DrawFocusBox( false );
	m_pSlideClose->SetZPos( 20 );
	EnsureMenuControls();
	SetSlideControlsVisible( false );
	SetHiddenBits( 0 );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

CHudFoF::~CHudFoF()
{
	delete m_pEquipmentHelpFrame;
	m_pEquipmentHelpFrame = NULL;
	m_pEquipmentHelpTitle = NULL;
	m_pEquipmentHelpText = NULL;
	m_pEquipmentHelpCloseButton = NULL;

	for ( int i = 0; i < ARRAYSIZE( m_pGoodBadAvatars ); ++i )
	{
		delete m_pGoodBadAvatars[i];
		m_pGoodBadAvatars[i] = NULL;
	}
}

void CHudFoF::Init()
{
	// The stock engine auto-loads localization using the game-directory
	// basename.  Explicitly add FoF's table as well so an isolated
	// compatibility stage (whose folder is intentionally not named "fof")
	// resolves the same tokens as the installed game.
	static bool s_bFoFLocalizationLoaded = false;
	if ( !s_bFoFLocalizationLoaded )
	{
			s_bFoFLocalizationLoaded = g_pVGuiLocalize->AddFile(
			"resource/fof_%language%.txt",
			"MOD",
			true );
	}
	ListenForGameEvent( "cap_zone" );
	ListenForGameEvent( "cap_zone_off" );
	ListenForGameEvent( "game_newmap" );
	ListenForGameEvent( "round_end" );
	Reset();
}

void CHudFoF::Reset()
{
	ClearWrappedTextCache();
	if ( m_iMenuKind != FOF_MENU_TEXT )
		ClearMenu();
	m_Hint.Clear();
	m_Slide.Clear();
	ClearSlideItems();
	m_bHintVisible = false;
	m_iHintMode = 99;
	m_flHintStartedAt = 0.0f;
	m_flHintPromptAt = 0.0f;
	m_bSlideVisible = false;
	m_bSlidePending = false;
	m_iSlidePage = 0;
	m_iSlidePageCount = 0;
	SetSlideControlsVisible( false );
	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
		vgui::input()->ReleaseAppModalSurface();
	vgui::surface()->SetCursorAlwaysVisible( false );
	if ( ::input && engine->IsInGame() )
		::input->ActivateMouse();
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	m_IconComms.Purge();
	m_flCircleProgressStart = 0.0f;
	m_flCircleProgressEnd = 0.0f;
	if ( m_pCircleProgressBar )
		m_pCircleProgressBar->SetVisible( false );
	m_iCapCount = 0;
	m_iCapMode = 0;
	m_iCapProgress = 0;
	m_flCapUntil = 0.0f;
	m_BBMarkers.Purge();
	m_BBNotices.Purge();
	m_bGoodBadVisible = false;
	m_GoodBadRanks.Purge();
	for ( int i = 0; i < ARRAYSIZE( m_pGoodBadAvatars ); ++i )
	{
		if ( m_pGoodBadAvatars[i] )
			m_pGoodBadAvatars[i]->ClearAvatarSteamID();
	}
	Q_memset( m_GoodBadFloats, 0, sizeof( m_GoodBadFloats ) );
	Q_memset( m_GoodBadInts, 0, sizeof( m_GoodBadInts ) );
	m_HitMarkers.Purge();
	Q_memset( m_WeaponDamage, 0, sizeof( m_WeaponDamage ) );
	m_iBowAttempts = 0;
	m_iBowHits = 0;
	m_StatLabel.Clear();
	m_iStatNewValue = 0;
	m_iStatOldValue = 0;
	m_bStatVisible = false;
	m_flStatPromptAt = 0.0f;
	m_bIntentionalSpectator = false;
}

void CHudFoF::VidInit()
{
	ClearWrappedTextCache();
	// Unlike a spawn ResetHUD, a video/level initialization must not retain a
	// server text menu from the previous map.
	ClearMenu();
	m_bTeamMenuOffered = false;
	m_bWasInitialSpectator = false;
	m_bIntentionalSpectator = false;
	m_CaptureMarkers.Purge();
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	m_pCaptureMarkerIcons[0] = gHUD.GetIcon( "icon_cap" );
	m_pCaptureMarkerIcons[1] = gHUD.GetIcon( "icon_def" );
	static const char *markerIconNames[] =
	{
		"icon_cap",
		"icon_loot_pick",
		"icon_loot_drop",
		"icon_whiskey_obj",
		"icon_disarm",
	};
	for ( int i = 0; i < ARRAYSIZE( markerIconNames ); ++i )
		m_pBBMarkerIcons[i] = gHUD.GetIcon( markerIconNames[i] );
	EnsureCircleProgressBar();
	LayoutCircleProgressBar();
	UpdateTeamIntroFont();
	LayoutMenuControls();
	LayoutSlideControls();
}

bool CHudFoF::ShouldDraw()
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	return m_bMenuVisible || m_bHintVisible || m_bSlideVisible ||
		m_flCircleProgressEnd > gpGlobals->curtime || player != NULL;
}

CON_COMMAND( fof_close_menu, "Close the current FoF menu without sending a selection." )
{
	CHudFoF *hud = GET_HUDELEMENT( CHudFoF );
	if ( hud )
		hud->CloseMenu();
}

void CHudFoF::OnThink()
{
	VPROF_BUDGET( "FoF::HUD::OnThink", VPROF_BUDGETGROUP_OTHER_VGUI );

	if ( GetWide() != ScreenWidth() || GetTall() != ScreenHeight() )
	{
		SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
		LayoutCircleProgressBar();
		UpdateTeamIntroFont();
		LayoutMenuControls();
		LayoutSlideControls();
	}

	UpdateCircleProgressBar();

	for ( int i = m_BBMarkers.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime > m_BBMarkers[i].expiresAt )
			m_BBMarkers.FastRemove( i );
	}
	for ( int i = m_HitMarkers.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime > m_HitMarkers[i].expiresAt )
			m_HitMarkers.FastRemove( i );
	}
	for ( int i = m_IconComms.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime > m_IconComms[i].expiresAt )
			m_IconComms.FastRemove( i );
	}
	for ( int i = m_BBNotices.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime > m_BBNotices[i].expiresAt )
			m_BBNotices.FastRemove( i );
	}

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	const bool sourceTV = FoFHudIsSourceTVClient();
	const bool localCourse = FoFIsLocalCourseSession();
	const bool blockedTeamMenu = m_bMenuVisible &&
		m_iMenuKind == FOF_MENU_TEAM &&
		sourceTV;
	const bool blockedTeamClassMenu = m_bMenuVisible &&
		m_iMenuKind == FOF_MENU_TEAM_CLASS &&
		sourceTV;
	const bool blockedEquipmentMenu = m_bMenuVisible &&
		m_iMenuKind == FOF_MENU_EQUIPMENT &&
		sourceTV;
	const bool blockedPurchaseMenu = m_bMenuVisible &&
		m_iMenuKind == FOF_MENU_PURCHASE &&
		sourceTV;
	if ( blockedTeamMenu || blockedTeamClassMenu ||
		blockedEquipmentMenu || blockedPurchaseMenu )
	{
		if ( blockedTeamMenu )
			m_bTeamMenuOffered = true;
		ClearMenu();
	}

	if ( localPlayer && !FoFFirstMotdReadyForNextPanel() )
		FoFShowFirstMotd();

	// A server may issue chooseteam while the asynchronously populated MOTD
	// is still being opened.  Do not leave that full-screen input owner
	// visible behind the MOTD; queue the normal initial-spectator offer and
	// recreate it only after the player dismisses the information panel.
	if ( m_bMenuVisible && m_iMenuKind == FOF_MENU_TEAM &&
		!FoFFirstMotdReadyForNextPanel() )
	{
		ClearMenu();
		m_bTeamMenuOffered = false;
	}

	const bool pointerMenu = m_bMenuVisible &&
		( m_iMenuKind == FOF_MENU_TEAM ||
			m_iMenuKind == FOF_MENU_TEAM_CLASS ||
			m_iMenuKind == FOF_MENU_EQUIPMENT ||
			m_iMenuKind == FOF_MENU_PURCHASE );
	if ( pointerMenu || m_bSlideVisible )
	{
		const bool externalUIOwnsInput =
			FoFHudExternalUIOwnsInput();
		const bool loadoutTextInput = pointerMenu &&
			m_iMenuKind == FOF_MENU_PURCHASE &&
			m_bLoadoutEditorVisible;
		// This HUD spans the entire screen.  Keep it mouse-interactive while
		// choosing a team/equipment or reading an intro, but get it out of the
		// hit-test path as soon as Escape GameUI or the developer console
		// appears.  The overlay remains visible behind those surfaces and
		// resumes click input when they close.
		const bool enableLoadoutTextInput =
			loadoutTextInput && !externalUIOwnsInput;
		// Panel::SetKeyBoardInputEnabled recursively changes every child.  The
		// editor's invisible click targets must remain mouse-only; otherwise a
		// click can move keyboard focus away from the preset-name TextEntry.
		vgui::ipanel()->SetKeyBoardInputEnabled(
			GetVPanel(), enableLoadoutTextInput );
		if ( m_pLoadoutEditorName )
		{
			m_pLoadoutEditorName->SetKeyBoardInputEnabled(
				enableLoadoutTextInput );
		}
		if ( m_pLoadoutEditorFilename )
		{
			m_pLoadoutEditorFilename->SetKeyBoardInputEnabled(
				enableLoadoutTextInput );
		}
		if ( externalUIOwnsInput )
		{
			SetMouseInputEnabled( false );
		}
		else if ( !IsMouseInputEnabled() )
		{
			SetMouseInputEnabled( true );
			SetCursor( vgui::dc_arrow );
			MakePopup( false );
			MoveToFront();
			if ( ::input )
				::input->DeactivateMouse();
			vgui::surface()->UnlockCursor();
			vgui::surface()->SetCursor( vgui::dc_arrow );
			vgui::surface()->SetCursorAlwaysVisible( true );
		}
		const bool loadoutTextFieldHasFocus =
			( m_pLoadoutEditorName && m_pLoadoutEditorName->HasFocus() ) ||
			( m_pLoadoutEditorFilename &&
				m_pLoadoutEditorFilename->HasFocus() );
		if ( loadoutTextInput && !externalUIOwnsInput &&
			m_pLoadoutEditorName && !loadoutTextFieldHasFocus )
		{
			m_pLoadoutEditorName->RequestFocus();
		}
		if ( pointerMenu && m_iMenuKind == FOF_MENU_TEAM )
		{
			const int factionMask = FoFHudTeamFactionMask();
			if ( factionMask != m_iTeamFactionMask )
			{
				m_iTeamFactionMask = factionMask;
				LayoutMenuControls();
				SetMenuControlsVisible( true );
			}

			// The server normally follows a successful selection with
			// VGUIMenu("team", false). Also close on the replicated player
			// transition so packet ordering cannot leave an input owner behind.
			if ( m_bWasInitialSpectator && localPlayer &&
				( localPlayer->IsAlive() ||
					localPlayer->GetTeamNumber() > 1 ) )
			{
				ClearMenu();
			}
		}
		else if ( pointerMenu && m_iMenuKind == FOF_MENU_PURCHASE )
		{
			// Cash changes immediately after a purchase/reward. Keep disabled
			// cards synchronized without rebuilding or re-sorting the list.
			UpdatePurchaseButtonStates();
		}
		else if ( pointerMenu &&
			m_iMenuKind == FOF_MENU_TEAM_CLASS )
		{
			UpdateTeamClassButtonStates();
		}
	}
	if ( m_bSlidePending && localPlayer &&
		!FoFHudInfoPanelVisible() &&
		FoFFirstMotdReadyForNextPanel() )
	{
		const CUtlString pendingName = m_Slide;
		m_bSlidePending = false;
		LoadSlide( pendingName.String() );
	}
	const bool initialSpectator = localPlayer && !localPlayer->IsAlive() &&
		localPlayer->GetTeamNumber() == 1 && localPlayer->GetObserverMode() == 7;
	if ( initialSpectator && !m_bWasInitialSpectator &&
		!m_bIntentionalSpectator )
		m_bTeamMenuOffered = false;
	if ( m_bIntentionalSpectator && m_bWasInitialSpectator && localPlayer &&
		( localPlayer->IsAlive() || localPlayer->GetTeamNumber() > 1 ) )
	{
		m_bIntentionalSpectator = false;
	}
	m_bWasInitialSpectator = initialSpectator;

	if ( !m_bTeamMenuOffered && !m_bMenuVisible && localPlayer &&
		!sourceTV &&
		!localPlayer->IsAlive() && localPlayer->GetTeamNumber() <= 1 &&
		!FoFHudInfoPanelVisible() && !IsSlideOpen() &&
		FoFFirstMotdReadyForNextPanel() &&
		( FoFIsListenServerSession() || localCourse ||
		  gpGlobals->curtime >= 3.0f ) )
	{
		ShowTeamMenu();
	}
}

CHudFoF *FoFHud()
{
	return GET_HUDELEMENT( CHudFoF );
}

int FoFHudScale( float value )
{
	return RoundFloatToInt( ( (float)ScreenHeight() / 480.0f ) * value );
}

int FoFHudConVarInt( const char *name, int defaultValue )
{
	ConVar *variable = cvar ? cvar->FindVar( name ) : NULL;
	return variable ? variable->GetInt() : defaultValue;
}

C_BasePlayer *FoFHudObservedPlayer( C_BasePlayer *localPlayer )
{
	if ( !localPlayer )
		return NULL;

	if ( localPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		C_BasePlayer *observed =
			dynamic_cast< C_BasePlayer * >( localPlayer->GetObserverTarget() );
		if ( observed )
			return observed;
	}

	return localPlayer;
}

bool FoFHudIsHintVisible()
{
	CHudFoF *hud = FoFHud();
	return hud && hud->IsHintVisible();
}

bool FoFHasPlayerHintPresenter()
{
	return FoFHud() != NULL;
}

void FoFPresentPlayerHint( const char *text, int mode )
{
	CHudFoF *hud = FoFHud();
	if ( hud )
		hud->ReceiveHint( text, mode );
}

void FoFPresentStatUpdate(
	const char *label, int newValue, int oldValue )
{
	CHudFoF *hud = FoFHud();
	if ( hud )
		hud->ReceiveStatUpdate( label, newValue, oldValue );
}

// FoF HUD fonts, textures and scheme-bound presentation resources.

void CHudFoF::UpdateTeamIntroFont()
{
	// The original 1080p short-intro label has a 14-pixel CJK ink box.
	// FoFSlideSmall's proportional face produced a 23-25-pixel box here;
	// an explicit 23-pixel Arial face reproduces the original glyph size.
	const int requestedTall = clamp(
		RoundFloatToInt( 10.25f * (float)ScreenHeight() / 480.0f ),
		16,
		31 );
	if ( m_hTeamIntroFont == vgui::INVALID_FONT ||
		m_iTeamIntroFontTall != requestedTall )
	{
		const vgui::HFont font = vgui::surface()->CreateFont();
		if ( font != vgui::INVALID_FONT &&
			vgui::surface()->SetFontGlyphSet(
				font,
				"Arial",
				requestedTall,
				0,
				0,
				0,
				vgui::ISurface::FONTFLAG_ANTIALIAS ) )
		{
			m_hTeamIntroFont = font;
			m_iTeamIntroFontTall = requestedTall;
		}
	}

	if ( m_hTeamIntroFont == vgui::INVALID_FONT )
		m_hTeamIntroFont = m_hSmallFont;
}

void CHudFoF::UpdateEquipmentHelpTitleFont()
{
	// Scheme proportional fonts are affected by the desktop DPI selected when
	// the surface is created.  That made the same client-area resolution use
	// different title sizes on different machines.  Build this display face at
	// an explicit client-pixel height derived from the live 640x480 canvas.
	const int requestedTall = MAX( RoundFloatToInt(
		11.0f * (float)ScreenHeight() / 480.0f ), 1 );
	if ( m_hEquipmentHelpTitleFont == vgui::INVALID_FONT ||
		m_iEquipmentHelpTitleFontTall != requestedTall )
	{
		const vgui::HFont font = vgui::surface()->CreateFont();
		if ( font != vgui::INVALID_FONT &&
			vgui::surface()->SetFontGlyphSet(
				font,
				"Verdana",
				requestedTall,
				700,
				0,
				0,
				vgui::ISurface::FONTFLAG_ANTIALIAS |
					vgui::ISurface::FONTFLAG_DROPSHADOW ) )
		{
			m_hEquipmentHelpTitleFont = font;
			m_iEquipmentHelpTitleFontTall = requestedTall;
		}
	}

	if ( m_hEquipmentHelpTitleFont == vgui::INVALID_FONT )
		m_hEquipmentHelpTitleFont = m_hSmallFont;
	if ( m_pEquipmentHelpTitle )
		m_pEquipmentHelpTitle->SetFont( m_hEquipmentHelpTitleFont );
	if ( m_pEquipmentHelpCloseButton )
		m_pEquipmentHelpCloseButton->SetFont(
			m_hEquipmentHelpTitleFont );
}

void CHudFoF::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	ClearWrappedTextCache();
	EnsureMenuTextures();
	if ( m_iIconCommTexture < 0 )
	{
		m_iIconCommTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iIconCommTexture,
			"vgui/iconcomm",
			true,
			false );
	}
	m_hFont = scheme->GetFont( "HudHintText", true );
	if ( m_hFont == vgui::INVALID_FONT )
		m_hFont = scheme->GetFont( "Default", true );
	m_hSmallFont = scheme->GetFont( "DefaultSmall", true );
	if ( m_hSmallFont == vgui::INVALID_FONT )
		m_hSmallFont = m_hFont;
	// CHudFoFHint's NumberFont animation variable defaults to HudNumbers in
	// the shipped client.  Keep it separate from the coordinator's general
	// text face: menus and unrelated notices intentionally use other fonts.
	// CHudFoFHint is a non-proportional panel in the shipped client.  Its
	// CPanelAnimationVar therefore resolves HudNumbers with the unscaled
	// scheme handle; asking for the proportional variant makes the glyphs
	// about four pixels too tall at 1024x768.
	m_hHintFont = scheme->GetFont( "HudNumbers", false );
	if ( m_hHintFont == vgui::INVALID_FONT )
		m_hHintFont = m_hFont;
	ApplyTeamClassMenuScheme( scheme );
	ApplyPurchaseMenuScheme( scheme );
	ApplyLoadoutEditorScheme( scheme );
	m_hCaptureFont = scheme->GetFont( "ClientTitleFontSmall", true );
	if ( m_hCaptureFont == vgui::INVALID_FONT )
		m_hCaptureFont = m_hSmallFont;
	m_hBBNoticeFont = scheme->GetFont( "MenuFontSmall", true );
	if ( m_hBBNoticeFont == vgui::INVALID_FONT )
		m_hBBNoticeFont = m_hSmallFont;
	m_hBBMarkerFont = scheme->GetFont( "DefaultFoF", false );
	if ( m_hBBMarkerFont == vgui::INVALID_FONT )
		m_hBBMarkerFont = m_hSmallFont;
	m_hHitReconFont = scheme->GetFont( "HudSelectionNumbers2", true );
	if ( m_hHitReconFont == vgui::INVALID_FONT )
		m_hHitReconFont = m_hSmallFont;
	// Keep the original Typodermic face, but use the intermediate proportional
	// size: fixed HudNumbers is too small while proportional HudNumbers grows
	// much larger than the shipped SourceTV labels at 1080p.
	m_hSourceTVPlayerFont = scheme->GetFont( "HudNumbersSmall", true );
	if ( m_hSourceTVPlayerFont == vgui::INVALID_FONT )
		m_hSourceTVPlayerFont = m_hFont;
	m_hEquipmentCreditFont =
		scheme->GetFont( "MenuFontMed", true );
	if ( m_hEquipmentCreditFont == vgui::INVALID_FONT )
		m_hEquipmentCreditFont = m_hFont;
	m_hEquipmentCostFont =
		scheme->GetFont( "HudSelectionNumbers2", true );
	if ( m_hEquipmentCostFont == vgui::INVALID_FONT )
		m_hEquipmentCostFont = m_hSmallFont;
	m_hEquipmentHeaderFont =
		scheme->GetFont( "MenuFontSmall", true );
	if ( m_hEquipmentHeaderFont == vgui::INVALID_FONT )
		m_hEquipmentHeaderFont = m_hSmallFont;
	m_hEquipmentFooterFont =
		scheme->GetFont( "HudSelectionNumbers3", true );
	if ( m_hEquipmentFooterFont == vgui::INVALID_FONT )
		m_hEquipmentFooterFont = m_hSmallFont;
	// BuyMenuDM::m_hVerySmallFont is declared as
	// CPanelAnimationVar( HFont, ..., "NotoS", "NotorietyFont" ).  The
	// shipped 768p face is bold Verdana with a drop shadow; DefaultSmall has
	// different CJK metrics and made every help line visibly too narrow.
	m_hEquipmentHelpFont = scheme->GetFont( "NotorietyFont", true );
	if ( m_hEquipmentHelpFont == vgui::INVALID_FONT )
		m_hEquipmentHelpFont = m_hSmallFont;
	if ( m_pEquipmentHelpText )
		m_pEquipmentHelpText->SetFont( m_hEquipmentHelpFont );
	UpdateEquipmentHelpTitleFont();
	m_hGoodBadRankFont =
		scheme->GetFont( "HudSelectionNumbers2", true );
	if ( m_hGoodBadRankFont == vgui::INVALID_FONT )
		m_hGoodBadRankFont = m_hSmallFont;
	m_hGoodBadAwardFont =
		scheme->GetFont( "HudSelectionNumbers4", true );
	if ( m_hGoodBadAwardFont == vgui::INVALID_FONT )
		m_hGoodBadAwardFont = m_hGoodBadRankFont;
	m_hGoodBadAwardLabelFont =
		scheme->GetFont( "HudNumbersSmall", true );
	if ( m_hGoodBadAwardLabelFont == vgui::INVALID_FONT )
		m_hGoodBadAwardLabelFont = m_hSmallFont;
	m_hGoodBadLocalFont =
		scheme->GetFont( "HudHintTextSmall", true );
	if ( m_hGoodBadLocalFont == vgui::INVALID_FONT )
		m_hGoodBadLocalFont = m_hSmallFont;
	static const char *slideFontNames[] =
	{
		"FoFSlideSmall", "FoFSlideMed", "FoFSlideBig", "FoFSlideSmall"
	};
	for ( int i = 0; i < ARRAYSIZE( m_hSlideFonts ); ++i )
	{
		m_hSlideFonts[i] = scheme->GetFont( slideFontNames[i], true );
		if ( m_hSlideFonts[i] == vgui::INVALID_FONT )
			m_hSlideFonts[i] = i == 0 ? m_hSmallFont : m_hFont;
	}
	UpdateTeamIntroFont();
	if ( m_iSlideBackgroundTexture < 0 )
	{
		m_iSlideBackgroundTexture =
			vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iSlideBackgroundTexture,
			"vgui/slide_bg",
			true,
			false );
	}
	if ( m_pSlideBack )
	{
		const color32 white = { 255, 255, 255, 255 };
		const color32 armed = { 255, 238, 190, 255 };
		const color32 pressed = { 190, 165, 120, 255 };
		const color32 disabled = { 100, 100, 100, 180 };
		m_pSlideBack->SetImage(
			CBitmapButton::BUTTON_ENABLED, "vgui/back", white );
		m_pSlideBack->SetImage(
			CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/back", armed );
		m_pSlideBack->SetImage(
			CBitmapButton::BUTTON_PRESSED, "vgui/back", pressed );
		m_pSlideBack->SetImage(
			CBitmapButton::BUTTON_DISABLED, "vgui/back", disabled );
	}
	if ( m_pSlideForward )
	{
		const color32 white = { 255, 255, 255, 255 };
		const color32 armed = { 255, 238, 190, 255 };
		const color32 pressed = { 190, 165, 120, 255 };
		const color32 disabled = { 100, 100, 100, 180 };
		m_pSlideForward->SetImage(
			CBitmapButton::BUTTON_ENABLED, "vgui/forward", white );
		m_pSlideForward->SetImage(
			CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/forward", armed );
		m_pSlideForward->SetImage(
			CBitmapButton::BUTTON_PRESSED, "vgui/forward", pressed );
		m_pSlideForward->SetImage(
			CBitmapButton::BUTTON_DISABLED, "vgui/forward", disabled );
	}
	if ( m_pSlideClose )
	{
		const color32 white = { 255, 255, 255, 255 };
		const color32 armed = { 255, 238, 190, 255 };
		const color32 pressed = { 190, 165, 120, 255 };
		const color32 disabled = { 100, 100, 100, 180 };
		m_pSlideClose->SetImage(
			CBitmapButton::BUTTON_ENABLED, "vgui/tm_ok", white );
		m_pSlideClose->SetImage(
			CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/tm_ok", armed );
		m_pSlideClose->SetImage(
			CBitmapButton::BUTTON_PRESSED, "vgui/tm_ok", pressed );
		m_pSlideClose->SetImage(
			CBitmapButton::BUTTON_DISABLED, "vgui/tm_ok", disabled );
	}
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	LayoutCircleProgressBar();
	LayoutSlideControls();
	SetPaintBackgroundEnabled( false );
}

// FoF's archived HUD visibility switch and its default P-key command.

static ConVar fof_hud_draw( "fof_hud_draw", "1", FCVAR_ARCHIVE );

bool FoFHudShouldDraw()
{
	return fof_hud_draw.GetBool();
}

bool FoFHudIsSourceTVClient()
{
	return engine && engine->IsHLTV();
}

bool FoFHudIsSpectatorClient()
{
	if ( FoFHudIsSourceTVClient() )
		return true;

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	return pLocalPlayer &&
		pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR;
}

bool FoFHudUsesProgrammaticLayout( const char *panelName )
{
	static const char *const programmaticPanels[] =
	{
		"TeamplayDialog",
		"HudDeathNotice",
		"HudFoF",
		"HUDBBStatus",
		"HudVersus",
		"HUDFoFTimer",
		"HudFoFEliminationStatus",
		"HUDCourseHint",
		"HudCourseEditor",
		"NPCProfileManagement",
		"HudMaterializeMenu"
	};

	if ( !panelName || !panelName[0] )
		return false;

	for ( int i = 0; i < ARRAYSIZE( programmaticPanels ); ++i )
	{
		if ( !Q_stricmp( panelName, programmaticPanels[i] ) )
			return true;
	}

	return false;
}

CON_COMMAND( toggle_hud_draw, "Toggles FoF UI elements" )
{
	fof_hud_draw.SetValue( !fof_hud_draw.GetBool() );
}
