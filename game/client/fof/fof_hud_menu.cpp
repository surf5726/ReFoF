// FoF menu controls, layout and shared presentation helpers.

#include "cbase.h"
#include "fof/fof_crate_menu.h"
#include "fof/fof_equipment_menu.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_menu.h"
#include "fof/fof_team_menu.h"
#include "cdll_client_int.h"
#include "game/client/iviewport.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "viewport_panel_names.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include "fof/fof_voice_menu.h"
#include "ienginevgui.h"
#include "iinput.h"
#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include "c_baseplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFoFEquipmentHelpFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFEquipmentHelpFrame, vgui::Frame );

public:
	CFoFEquipmentHelpFrame( CHudFoF *owner )
		: BaseClass( NULL, "FoFEquipmentHelpFrame", false, true )
		, m_pOwner( owner )
	{
		SetDeleteSelfOnClose( false );
		SetMoveable( false );
		SetSizeable( false );
		SetMenuButtonVisible( false );
		SetMinimizeButtonVisible( false );
		SetMaximizeButtonVisible( false );
		// The stock Frame caption is fixed-size and its TextImage truncates
		// short CJK titles with an ellipsis at some resolutions.  Keep Frame's
		// popup/background behaviour, but give FoF a separately scaled header.
		SetTitleBarVisible( false );
		SetTitle( L"", false );
		DisableFadeEffect();
		SetPaintBackgroundEnabled( true );
		SetPaintBackgroundType( 2 );
		SetBgColor( Color( 150, 150, 150, 255 ) );
		SetVisible( false );
	}

protected:
	virtual void OnClose()
	{
		if ( m_pOwner )
			m_pOwner->CloseEquipmentHelp();
		else
			SetVisible( false );
	}

private:
	CHudFoF *m_pOwner;
};

static void FoFEnsureMenuTexture( int &textureId, const char *material )
{
	if ( textureId >= 0 )
		return;

	textureId = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		textureId,
		material,
		true,
		false );
}

static void FoFEnsureTranslucentMenuTexture(
	int &textureId,
	const char *baseTexture,
	const char *materialName )
{
	if ( textureId >= 0 || !baseTexture || !materialName ||
		!g_pMatSystemSurface )
	{
		return;
	}

	KeyValues *vmt = new KeyValues( "UnlitGeneric" );
	vmt->SetString( "$basetexture", baseTexture );
	vmt->SetInt( "$vertexcolor", 1 );
	vmt->SetInt( "$vertexalpha", 1 );
	vmt->SetInt( "$translucent", 1 );
	vmt->SetInt( "$ignorez", 1 );
	vmt->SetInt( "$nofog", 1 );
	vmt->SetInt( "$no_fullbright", 1 );
	IMaterial *material = materials->CreateMaterial( materialName, vmt );
	if ( !material )
		return;

	textureId = vgui::surface()->CreateNewTextureID();
	g_pMatSystemSurface->DrawSetTextureMaterial( textureId, material );
}

bool FoFHudInfoPanelVisible()
{
	if ( !gViewPortInterface )
		return false;

	IViewPortPanel *infoPanel =
		gViewPortInterface->FindPanelByName( PANEL_INFO );
	return infoPanel && infoPanel->IsVisible();
}

void CHudFoF::EnsureMenuControls()
{
	EnsureTeamClassMenuControls();
	EnsurePurchaseMenuControls();

	for ( int i = 0; i < FOF_TEAM_BUTTON_COUNT; ++i )
	{
		if ( !m_pTeamMenuButtons[i] )
		{
			char name[32];
			Q_snprintf( name, sizeof( name ), "FoFTeamButton%d", i );
			m_pTeamMenuButtons[i] = new vgui::Button(
				this,
				name,
				"",
				this,
				FoFHudTeamButtonCommand( i ) );
			m_pTeamMenuButtons[i]->SetButtonBorderEnabled( false );
			m_pTeamMenuButtons[i]->SetPaintBorderEnabled( false );
			m_pTeamMenuButtons[i]->DrawFocusBox( false );
			m_pTeamMenuButtons[i]->SetPaintEnabled( false );
			m_pTeamMenuButtons[i]->SetPaintBackgroundEnabled( false );
			m_pTeamMenuButtons[i]->SetMouseInputEnabled( true );
			m_pTeamMenuButtons[i]->SetButtonActivationType(
				vgui::Button::ACTIVATE_ONPRESSED );
			// The original team menu gives the four faction cards the heavy
			// cylinder pair and its three utility buttons the light pair.
			// These bindings are installed by code rather than teammenufof.res.
			if ( i >= 3 )
			{
				m_pTeamMenuButtons[i]->SetArmedSound(
					"ui/rollover_cyl1.wav" );
				m_pTeamMenuButtons[i]->SetReleasedSound(
					"ui/release_fire2.wav" );
			}
			else
			{
				m_pTeamMenuButtons[i]->SetArmedSound(
					"ui/rollover_cyl2.wav" );
				m_pTeamMenuButtons[i]->SetReleasedSound(
					"ui/release_fire1.wav" );
			}
			m_pTeamMenuButtons[i]->SetKeyBoardInputEnabled( false );
			m_pTeamMenuButtons[i]->SetZPos( 30 );
			m_pTeamMenuButtons[i]->SetVisible( false );
		}
	}
	if ( !m_pTeamIntroButton )
	{
		m_pTeamIntroButton = new vgui::Button(
			this,
			"FoFTeamIntroButton",
			"",
			this,
			"fof_team_intro" );
		m_pTeamIntroButton->SetButtonBorderEnabled( false );
		m_pTeamIntroButton->SetPaintBorderEnabled( false );
		m_pTeamIntroButton->DrawFocusBox( false );
		m_pTeamIntroButton->SetPaintEnabled( false );
		m_pTeamIntroButton->SetPaintBackgroundEnabled( false );
		m_pTeamIntroButton->SetMouseInputEnabled( true );
		m_pTeamIntroButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		m_pTeamIntroButton->SetKeyBoardInputEnabled( false );
		m_pTeamIntroButton->SetZPos( 30 );
		m_pTeamIntroButton->SetVisible( false );
	}

	for ( int i = 0; i < FOF_DM_ITEM_COUNT; ++i )
	{
		if ( !m_pEquipmentMenuButtons[i] )
		{
			char name[32];
			char command[48];
			Q_snprintf( name, sizeof( name ), "FoFEquipmentButton%d", i );
			Q_snprintf( command, sizeof( command ), "fof_equip_item_%d", i );
			m_pEquipmentMenuButtons[i] = new vgui::Button(
				this,
				name,
				"",
				this,
				command );
			m_pEquipmentMenuButtons[i]->SetButtonBorderEnabled( false );
			m_pEquipmentMenuButtons[i]->SetPaintBorderEnabled( false );
			m_pEquipmentMenuButtons[i]->DrawFocusBox( false );
			m_pEquipmentMenuButtons[i]->SetPaintEnabled( false );
			m_pEquipmentMenuButtons[i]->SetPaintBackgroundEnabled( false );
			m_pEquipmentMenuButtons[i]->SetMouseInputEnabled( true );
			m_pEquipmentMenuButtons[i]->SetButtonActivationType(
				vgui::Button::ACTIVATE_ONPRESSED );
			// BuyMenuDM binds this shipped sample through Button's release
			// original sound slot.
			// DoClick owns playback; the equipment action callback must not
			// issue a second, unrelated ISurface::PlaySound request.
			m_pEquipmentMenuButtons[i]->SetReleasedSound(
				"common/banked2.wav" );
			m_pEquipmentMenuButtons[i]->SetKeyBoardInputEnabled( false );
			m_pEquipmentMenuButtons[i]->SetZPos( 30 );
			m_pEquipmentMenuButtons[i]->SetVisible( false );
		}

		if ( FoFEquipmentItemHelpToken( i ) &&
			!m_pEquipmentHelpButtons[i] )
		{
			char name[32];
			char command[48];
			Q_snprintf( name, sizeof( name ), "FoFEquipmentHelp%d", i );
			Q_snprintf( command, sizeof( command ), "fof_equip_help_%d", i );
			m_pEquipmentHelpButtons[i] = new vgui::Button(
				this, name, "", this, command );
			m_pEquipmentHelpButtons[i]->SetButtonBorderEnabled( false );
			m_pEquipmentHelpButtons[i]->SetPaintBorderEnabled( false );
			m_pEquipmentHelpButtons[i]->DrawFocusBox( false );
			m_pEquipmentHelpButtons[i]->SetPaintEnabled( false );
			m_pEquipmentHelpButtons[i]->SetPaintBackgroundEnabled( false );
			m_pEquipmentHelpButtons[i]->SetMouseInputEnabled( true );
			m_pEquipmentHelpButtons[i]->SetButtonActivationType(
				vgui::Button::ACTIVATE_ONPRESSED );
			m_pEquipmentHelpButtons[i]->SetKeyBoardInputEnabled( false );
			m_pEquipmentHelpButtons[i]->SetZPos( 40 );
			m_pEquipmentHelpButtons[i]->SetVisible( false );
		}
	}

	for ( int i = 0; i < FOF_CRATE_ROW_COUNT; ++i )
	{
		if ( !m_pCrateMenuButtons[i] )
		{
			char name[32];
			char command[48];
			Q_snprintf( name, sizeof( name ), "FoFCrateButton%d", i );
			Q_snprintf( command, sizeof( command ), "fof_crate_row_%d", i );
			m_pCrateMenuButtons[i] = new vgui::Button(
				this,
				name,
				"",
				this,
				command );
			m_pCrateMenuButtons[i]->SetButtonBorderEnabled( false );
			m_pCrateMenuButtons[i]->SetPaintBorderEnabled( false );
			m_pCrateMenuButtons[i]->DrawFocusBox( false );
			// The shipped MenuFoF writes each offer into the Button label and
			// enables wrapping.  Only the row artwork and number label are
			// custom-painted by CHudMenuFoF.
			m_pCrateMenuButtons[i]->SetPaintEnabled( true );
			m_pCrateMenuButtons[i]->SetPaintBackgroundEnabled( false );
			m_pCrateMenuButtons[i]->SetWrap( true );
			m_pCrateMenuButtons[i]->SetContentAlignment(
				vgui::Label::a_west );
			// MenuFoF is a CHudMenu-style number-key overlay.  These buttons
			// only provide row bounds for painting and must never claim the
			// system cursor.
			m_pCrateMenuButtons[i]->SetMouseInputEnabled( false );
			m_pCrateMenuButtons[i]->SetButtonActivationType(
				vgui::Button::ACTIVATE_ONPRESSED );
			m_pCrateMenuButtons[i]->SetKeyBoardInputEnabled( false );
			m_pCrateMenuButtons[i]->SetZPos( 30 );
			m_pCrateMenuButtons[i]->SetVisible( false );
		}
	}

	if ( !m_pEquipmentOkayButton )
	{
		m_pEquipmentOkayButton = new vgui::Button(
			this,
			"FoFEquipmentOkay",
			"",
			this,
			"fof_equip_accept" );
		m_pEquipmentOkayButton->SetButtonBorderEnabled( false );
		m_pEquipmentOkayButton->SetPaintBorderEnabled( false );
		m_pEquipmentOkayButton->DrawFocusBox( false );
		m_pEquipmentOkayButton->SetPaintEnabled( false );
		m_pEquipmentOkayButton->SetPaintBackgroundEnabled( false );
		m_pEquipmentOkayButton->SetMouseInputEnabled( true );
		m_pEquipmentOkayButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		// BuyMenuDM binds only its hover/armed cue here
		// in the original client.  Equipment cards use
		// common/banked2.wav, but the OKAY control uses the light cylinder cue.
		m_pEquipmentOkayButton->SetArmedSound(
			"ui/rollover_cyl2.wav" );
		m_pEquipmentOkayButton->SetKeyBoardInputEnabled( false );
		m_pEquipmentOkayButton->SetZPos( 30 );
		m_pEquipmentOkayButton->SetVisible( false );
	}

	if ( !m_pEquipmentCloseButton )
	{
		m_pEquipmentCloseButton = new vgui::Button(
			this,
			"FoFEquipmentClose",
			"",
			this,
			"fof_menu_cancel" );
		m_pEquipmentCloseButton->SetButtonBorderEnabled( false );
		m_pEquipmentCloseButton->SetPaintBorderEnabled( false );
		m_pEquipmentCloseButton->DrawFocusBox( false );
		m_pEquipmentCloseButton->SetPaintEnabled( false );
		m_pEquipmentCloseButton->SetPaintBackgroundEnabled( false );
		m_pEquipmentCloseButton->SetMouseInputEnabled( true );
		m_pEquipmentCloseButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		m_pEquipmentCloseButton->SetKeyBoardInputEnabled( false );
		m_pEquipmentCloseButton->SetZPos( 30 );
		m_pEquipmentCloseButton->SetVisible( false );
	}

	if ( !m_pEquipmentHelpFrame )
	{
		// Retain a real top-level Frame for popup ordering and rounded panel
		// treatment.  Its fixed-size stock caption is replaced below because it
		// overlaps the proportional FoF help text.
		m_pEquipmentHelpFrame = new CFoFEquipmentHelpFrame( this );
	}

	if ( !m_pEquipmentHelpTitle )
	{
		m_pEquipmentHelpTitle = new vgui::Label(
			m_pEquipmentHelpFrame,
			"FoFEquipmentHelpTitle",
			L"" );
		m_pEquipmentHelpTitle->SetPaintBorderEnabled( false );
		m_pEquipmentHelpTitle->SetPaintBackgroundEnabled( false );
		m_pEquipmentHelpTitle->SetMouseInputEnabled( false );
		m_pEquipmentHelpTitle->SetKeyBoardInputEnabled( false );
		m_pEquipmentHelpTitle->SetContentAlignment(
			vgui::Label::a_west );
		m_pEquipmentHelpTitle->SetFgColor(
			Color( 250, 250, 250, 255 ) );
		m_pEquipmentHelpTitle->SetZPos( 110 );
		m_pEquipmentHelpTitle->SetVisible( true );
	}

	if ( !m_pEquipmentHelpCloseButton )
	{
		m_pEquipmentHelpCloseButton = new vgui::Button(
			m_pEquipmentHelpFrame,
			"FoFEquipmentHelpClose",
			"X",
			this,
			"fof_equip_help_close" );
		m_pEquipmentHelpCloseButton->SetButtonBorderEnabled( false );
		m_pEquipmentHelpCloseButton->SetPaintBorderEnabled( false );
		m_pEquipmentHelpCloseButton->DrawFocusBox( false );
		m_pEquipmentHelpCloseButton->SetPaintBackgroundEnabled( false );
		m_pEquipmentHelpCloseButton->SetDefaultColor(
			Color( 250, 250, 250, 255 ), Color( 0, 0, 0, 0 ) );
		m_pEquipmentHelpCloseButton->SetArmedColor(
			Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 0 ) );
		m_pEquipmentHelpCloseButton->SetDepressedColor(
			Color( 210, 210, 210, 255 ), Color( 0, 0, 0, 0 ) );
		m_pEquipmentHelpCloseButton->SetContentAlignment(
			vgui::Label::a_center );
		m_pEquipmentHelpCloseButton->SetMouseInputEnabled( true );
		m_pEquipmentHelpCloseButton->SetKeyBoardInputEnabled( false );
		m_pEquipmentHelpCloseButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		m_pEquipmentHelpCloseButton->SetZPos( 120 );
		m_pEquipmentHelpCloseButton->SetVisible( true );
	}

	if ( !m_pEquipmentHelpText )
	{
		// The original Label is a child of the Frame, not BuyMenuDM.
		// Preserve that parent coordinate system and Label's native wrapping.
		m_pEquipmentHelpText = new vgui::Label(
			m_pEquipmentHelpFrame,
			"FoFEquipmentHelpText",
			L"" );
		m_pEquipmentHelpText->SetPaintBorderEnabled( false );
		m_pEquipmentHelpText->SetPaintBackgroundEnabled( true );
		m_pEquipmentHelpText->SetMouseInputEnabled( false );
		m_pEquipmentHelpText->SetKeyBoardInputEnabled( false );
		m_pEquipmentHelpText->SetContentAlignment(
			vgui::Label::a_north );
		m_pEquipmentHelpText->SetWrap( true );
		m_pEquipmentHelpText->SetFgColor(
			Color( 250, 250, 250, 255 ) );
		m_pEquipmentHelpText->SetBgColor(
			Color( 10, 10, 10, 10 ) );
		// The original client passes 100 to Panel::SetZPos, not SetAlpha.
		// The Label itself stays fully opaque.
		m_pEquipmentHelpText->SetAlpha( 255 );
		m_pEquipmentHelpText->SetZPos( 100 );
		m_pEquipmentHelpText->SetVisible( true );
	}
}

void CHudFoF::EnsureMenuTextures()
{
	EnsureTeamClassMenuTextures();
	EnsurePurchaseMenuTextures();

	FoFEnsureMenuTexture(
		m_iTeamBackgroundTexture,
		"vgui/teambg" );
	FoFEnsureMenuTexture(
		m_iTeamIntroTexture,
		"vgui/slide_bg_small" );
	FoFEnsureTranslucentMenuTexture(
		m_iEquipmentBackgroundTexture,
		"vgui/equipmentbg",
		"fof_equipment_background" );
	FoFEnsureTranslucentMenuTexture(
		m_iEquipmentItemBackgroundTexture,
		"vgui/equipment_item",
		"fof_equipment_item_background" );
	FoFEnsureTranslucentMenuTexture(
		m_iEquipmentOkayTexture,
		"vgui/tm_ok",
		"fof_equipment_okay" );
	FoFEnsureMenuTexture(
		m_iGoodBadBackgroundTexture,
		"vgui/endmap_bg" );
	FoFEnsureMenuTexture(
		m_iGoodBadBotTexture,
		"vgui/icon_fof" );
	FoFEnsureMenuTexture(
		m_iGoodBadLaurelTexture,
		"vgui/laurels" );
	FoFEnsureMenuTexture(
		m_iTeamAuto2Texture,
		"vgui/tm_auto2" );

	for ( int i = 0; i < FOF_TEAM_BUTTON_COUNT; ++i )
	{
		FoFEnsureMenuTexture(
			m_iTeamButtonTextures[i],
			FoFHudTeamButtonMaterial( i ) );
	}
	for ( int i = 0; i < FOF_DM_ITEM_COUNT; ++i )
	{
		const char *baseTexture = FoFEquipmentItemBaseTexture( i );
		char materialName[64];
		Q_snprintf(
			materialName,
			sizeof( materialName ),
			"fof_equipment_item_%d",
			i );
		FoFEnsureTranslucentMenuTexture(
			m_iEquipmentItemTextures[i],
			baseTexture,
			materialName );
	}
	for ( int i = 0; i < FOF_CRATE_ITEM_COUNT; ++i )
	{
		const char *material = FoFCrateMenuMaterial( i );
		if ( material )
		{
			FoFEnsureMenuTexture(
				m_iCrateItemTextures[i],
				material );
		}
	}
}

void CHudFoF::SetMenuControlsVisible( bool visible )
{
	SetTeamClassMenuControlsVisible( visible );
	SetPurchaseMenuControlsVisible( visible );

	const bool showTeamMenu =
		visible && m_iMenuKind == FOF_MENU_TEAM;
	const bool showEquipmentMenu =
		visible && m_iMenuKind == FOF_MENU_EQUIPMENT;
	const bool equipmentHelpOpen =
		showEquipmentMenu && m_iEquipmentHelpItem >= 0;
	if ( m_pEquipmentPaintPanel )
	{
		m_pEquipmentPaintPanel->SetAlpha(
			equipmentHelpOpen ? 35 : 255 );
		m_pEquipmentPaintPanel->SetVisible( showEquipmentMenu );
	}
	for ( int i = 0; i < FOF_TEAM_BUTTON_COUNT; ++i )
	{
		if ( m_pTeamMenuButtons[i] )
		{
			const bool activeFaction =
				i >= 3 &&
				( m_iTeamFactionMask & ( 1 << ( i - 3 ) ) ) != 0;
			m_pTeamMenuButtons[i]->SetVisible(
				showTeamMenu && ( i < 3 || activeFaction ) );
		}
	}
	if ( m_pTeamIntroButton )
	{
		m_pTeamIntroButton->SetVisible(
			showTeamMenu && FoFHudTeamIntroSlideName()[0] != '\0' );
	}
	for ( int i = 0; i < FOF_DM_ITEM_COUNT; ++i )
	{
		const bool itemAvailable = FoFEquipmentItemAvailable( i );
		if ( m_pEquipmentMenuButtons[i] )
		{
			m_pEquipmentMenuButtons[i]->SetVisible(
				showEquipmentMenu && itemAvailable );
			m_pEquipmentMenuButtons[i]->SetEnabled( !equipmentHelpOpen );
		}
		if ( m_pEquipmentHelpButtons[i] )
		{
			m_pEquipmentHelpButtons[i]->SetVisible(
				showEquipmentMenu && itemAvailable && !equipmentHelpOpen );
			m_pEquipmentHelpButtons[i]->SetEnabled( !equipmentHelpOpen );
		}
	}
	for ( int i = 0; i < FOF_CRATE_ROW_COUNT; ++i )
	{
		if ( m_pCrateMenuButtons[i] )
		{
			m_pCrateMenuButtons[i]->SetVisible(
				visible &&
				( m_iMenuKind == FOF_MENU_CRATE ||
					m_iMenuKind == FOF_MENU_TEXT ) &&
				i < m_MenuEntries.Count() );
		}
	}
	if ( m_pEquipmentOkayButton )
	{
		m_pEquipmentOkayButton->SetVisible(
			showEquipmentMenu );
		m_pEquipmentOkayButton->SetEnabled( !equipmentHelpOpen );
	}
	if ( m_pEquipmentCloseButton )
	{
		m_pEquipmentCloseButton->SetVisible(
			showEquipmentMenu );
		m_pEquipmentCloseButton->SetEnabled( !equipmentHelpOpen );
	}
	if ( m_pEquipmentHelpFrame )
	{
		m_pEquipmentHelpFrame->SetVisible( equipmentHelpOpen );
		if ( equipmentHelpOpen )
			m_pEquipmentHelpFrame->MoveToFront();
	}
	if ( m_pEquipmentHelpTitle )
		m_pEquipmentHelpTitle->SetVisible( true );
	if ( m_pEquipmentHelpText )
		m_pEquipmentHelpText->SetVisible( true );
	if ( m_pEquipmentHelpCloseButton )
		m_pEquipmentHelpCloseButton->SetVisible( true );
}

// FoF team, equipment and crate menu geometry.

void CHudFoF::LayoutMenuControls()
{
	EnsureMenuControls();
	LayoutTeamClassMenuControls();
	LayoutPurchaseMenuControls();
	if ( m_pEquipmentPaintPanel )
	{
		m_pEquipmentPaintPanel->SetBounds(
			0, 0, ScreenWidth(), ScreenHeight() );
	}
	if ( !m_bMenuVisible )
		return;

	// BuyMenuDM and MenuFoF use the Source 640x480 proportional canvas.
	// CTeamMenuFoF is the exception and keeps its 800x600 resource canvas.
	const float equipmentScale =
		MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int equipmentX =
		ScreenWidth() / 2 - RoundFloatToInt( 200.0f * equipmentScale );
	const int equipmentY =
		ScreenHeight() / 2 - RoundFloatToInt( 150.0f * equipmentScale );

	int equipmentRows[3] = { 0, 0, 0 };
	for ( int i = 0; i < FOF_DM_ITEM_COUNT; ++i )
	{
		if ( !FoFEquipmentItemAvailable( i ) )
			continue;

		const int column = FoFEquipmentItemColumn( i );
		const int row = equipmentRows[column]++;

		const int x = equipmentX + RoundFloatToInt(
			( 25.0f + column * 120.0f ) * equipmentScale );
		const int y = equipmentY + RoundFloatToInt(
			( column == 2
				? 50.0f + row * 50.0f
				: 50.0f + row * 27.0f ) * equipmentScale );
		const int wide = MAX(
			RoundFloatToInt( 100.0f * equipmentScale ), 1 );
		const int tall = MAX( RoundFloatToInt(
			( column == 2 ? 40.0f : 25.0f ) *
				equipmentScale ), 1 );
		m_pEquipmentMenuButtons[i]->SetBounds( x, y, wide, tall );
		m_pEquipmentMenuButtons[i]->MoveToFront();
		if ( m_pEquipmentHelpButtons[i] )
		{
			const int helpSize = MAX(
				RoundFloatToInt( 10.0f * equipmentScale ), 1 );
			m_pEquipmentHelpButtons[i]->SetBounds(
				x + RoundFloatToInt( 101.0f * equipmentScale ),
				y,
				helpSize,
				helpSize );
			m_pEquipmentHelpButtons[i]->MoveToFront();
		}
	}

	m_pEquipmentOkayButton->SetBounds(
		equipmentX + RoundFloatToInt( 270.0f * equipmentScale ),
		equipmentY + RoundFloatToInt( 250.0f * equipmentScale ),
		MAX( RoundFloatToInt( 100.0f * equipmentScale ), 1 ),
		MAX( RoundFloatToInt( 42.0f * equipmentScale ), 1 ) );
	m_pEquipmentCloseButton->SetBounds(
		equipmentX + RoundFloatToInt( 385.0f * equipmentScale ),
		equipmentY + RoundFloatToInt( 5.0f * equipmentScale ),
		MAX( RoundFloatToInt( 10.0f * equipmentScale ), 1 ),
		MAX( RoundFloatToInt( 10.0f * equipmentScale ), 1 ) );
	m_pEquipmentOkayButton->MoveToFront();
	m_pEquipmentCloseButton->MoveToFront();
	if ( m_pEquipmentHelpFrame || m_pEquipmentHelpTitle ||
		m_pEquipmentHelpText || m_pEquipmentHelpCloseButton )
	{
		UpdateEquipmentHelpTitleFont();
		int anchorX = equipmentX;
		int anchorY = equipmentY;
		if ( m_iEquipmentHelpItem >= 0 &&
			m_iEquipmentHelpItem < FOF_DM_ITEM_COUNT &&
			m_pEquipmentHelpButtons[m_iEquipmentHelpItem] )
		{
			int helpWide = 0;
			int helpTall = 0;
			m_pEquipmentHelpButtons[m_iEquipmentHelpItem]->GetBounds(
				anchorX, anchorY, helpWide, helpTall );
			anchorX += helpWide / 2;
			anchorY += helpTall / 2;
		}
		const int panelWide = MAX(
			RoundFloatToInt( 260.0f * equipmentScale ), 1 );
		const int panelTall = MAX(
			RoundFloatToInt( 90.0f * equipmentScale ), 1 );
		const int panelX = MAX(
			0,
			MIN(
				anchorX,
				ScreenWidth() -
					RoundFloatToInt( 9.0f * equipmentScale ) - panelWide ) );
		const int panelY = clamp(
			anchorY,
			0,
			MAX( ScreenHeight() - panelTall, 0 ) );
		if ( m_pEquipmentHelpFrame )
		{
			m_pEquipmentHelpFrame->SetBounds(
				panelX, panelY, panelWide, panelTall );
		}
		const int headerTall = MAX(
			RoundFloatToInt( 16.0f * equipmentScale ), 1 );
		const int horizontalInset = MAX(
			RoundFloatToInt( 6.0f * equipmentScale ), 1 );
		const int closeSize = MAX(
			RoundFloatToInt( 14.0f * equipmentScale ), 1 );
		const int headerTop = MAX(
			RoundFloatToInt( 2.0f * equipmentScale ), 1 );
		if ( m_pEquipmentHelpTitle )
		{
			m_pEquipmentHelpTitle->SetBounds(
				horizontalInset,
				headerTop,
				MAX( panelWide - closeSize -
					3 * horizontalInset, 1 ),
				MAX( headerTall - headerTop, 1 ) );
			m_pEquipmentHelpTitle->MoveToFront();
		}
		if ( m_pEquipmentHelpCloseButton )
		{
			m_pEquipmentHelpCloseButton->SetBounds(
				panelWide - horizontalInset - closeSize,
				headerTop,
				closeSize,
				closeSize );
			m_pEquipmentHelpCloseButton->MoveToFront();
		}
		if ( m_pEquipmentHelpText )
		{
			// Deliberately improve on FoF: its Label begins at y=7 and
			// collides with the Frame caption.  Restrict wrapping to a dedicated
			// content area below the complete proportional header instead.
			const int bodyTop = headerTall + MAX(
				RoundFloatToInt( 2.0f * equipmentScale ), 1 );
			const int bodyBottom = MAX(
				RoundFloatToInt( 5.0f * equipmentScale ), 1 );
			m_pEquipmentHelpText->SetBounds(
				horizontalInset,
				bodyTop,
				MAX( panelWide - 2 * horizontalInset, 1 ),
				MAX( panelTall - bodyTop - bodyBottom, 1 ) );
			m_pEquipmentHelpText->MoveToFront();
		}
	}

	// CTeamMenuFoF::ApplySchemeSettings replaces the resource position with
	// a 640x480 proportional layout: centered 400 units wide and anchored
	// 150 units above the bottom edge.  Use truncation like the original
	// proportional conversion so 1080p/1440p/4K do not accumulate 1px drift.
	const float teamScale =
		MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int teamX =
		ScreenWidth() / 2 - (int)( 200.0f * teamScale );
	const int teamY =
		ScreenHeight() - (int)( 150.0f * teamScale );
	static const float teamBounds[3][4] =
	{
		// CTeamMenuFoF layout: the three 256x128 labels are 70x35,
		// while the faction portraits are 70x70.
		{  65.0f, 0.0f, 70.0f, 35.0f },
		{ 155.0f, 0.0f, 70.0f, 35.0f },
		{ 245.0f, 0.0f, 70.0f, 35.0f }
	};
	const int factionCount = FoFHudTeamFactionCount();
	const bool freeForAllTeamMenu = factionCount == 0;
	const float factionPitch = 80.0f;
	const float factionWide = 70.0f;
	const float factionStart =
		( 400.0f -
			( factionWide +
				factionPitch * (float)MAX( factionCount - 1, 0 ) ) ) *
		0.5f;
	for ( int i = 0; i < FOF_TEAM_BUTTON_COUNT; ++i )
	{
		// The FFA variant has no faction portraits, so the original moves its
		// three label buttons into the vertical centre of the 100-unit panel.
		// Keeping the team-play y=0 row here made tm_auto2 touch the top border
		// and also left its VGUI hit box above the artwork.  Express the offset
		// in the same proportional canvas so 1080p, 1440p and 4K all match.
		float buttonX = i < 3 ? teamBounds[i][0] : 0.0f;
		const float buttonY =
			freeForAllTeamMenu && i < 3 ? 35.0f :
			( i < 3 ? teamBounds[i][1] : 25.0f );
		const float buttonWide =
			i < 3 ? teamBounds[i][2] : factionWide;
		const float buttonTall =
			i < 3 ? teamBounds[i][3] : 70.0f;
		if ( i >= 3 )
		{
			const int ordinal = FoFHudTeamFactionOrdinal( i - 1 );
			buttonX = factionStart +
				(float)MAX( ordinal, 0 ) * factionPitch;
		}
		m_pTeamMenuButtons[i]->SetBounds(
			teamX + (int)( buttonX * teamScale ),
			teamY + (int)( buttonY * teamScale ),
			MAX( (int)( buttonWide * teamScale ), 1 ),
			MAX( (int)( buttonTall * teamScale ), 1 ) );
		m_pTeamMenuButtons[i]->MoveToFront();
	}
	if ( m_pTeamIntroButton )
	{
		// FoF CTeamMenuFoF places the first queued intro at
		// (60,100), sized 70x35, in the same proportional 400x200 panel.
		m_pTeamIntroButton->SetBounds(
			teamX + (int)( 60.0f * teamScale ),
			teamY + (int)( 100.0f * teamScale ),
			MAX( (int)( 70.0f * teamScale ), 1 ),
			MAX( (int)( 35.0f * teamScale ), 1 ) );
		m_pTeamIntroButton->MoveToFront();
	}

	const int crateCount = MIN(
		m_MenuEntries.Count(), FOF_CRATE_ROW_COUNT );
	if ( m_iMenuKind == FOF_MENU_TEXT )
	{
		// Original CHudMenuFoF layout:
		// a 110-unit group is centred horizontally, its 100-unit bitmap rows
		// start after a 10-unit number column, and the list is bottom-anchored.
		const int textWide = MAX(
			RoundFloatToInt( 100.0f * equipmentScale ), 1 );
		const int textTall = MAX(
			RoundFloatToInt( 25.0f * equipmentScale ), 1 );
		const int textPitch = MAX(
			RoundFloatToInt( 27.0f * equipmentScale ), 1 );
		const int textX = ScreenWidth() / 2 -
			RoundFloatToInt( 45.0f * equipmentScale );
		int textY = ScreenHeight() -
			RoundFloatToInt(
				( 25.0f + 30.0f * crateCount ) * equipmentScale );
		if ( FoFHudCurrentMode() == 6 )
			textY -= RoundFloatToInt( 130.0f * equipmentScale );

		for ( int i = 0; i < FOF_CRATE_ROW_COUNT; ++i )
		{
			m_pCrateMenuButtons[i]->SetBounds(
				textX,
				textY + i * textPitch,
				textWide,
				textTall );
			m_pCrateMenuButtons[i]->MoveToFront();
		}
		return;
	}

	const int crateWide = MAX(
		RoundFloatToInt( 110.0f * equipmentScale ), 1 );
	const int cratePitch = MAX(
		RoundFloatToInt( 25.0f * equipmentScale ), 1 );
	// CHudMenuFoF anchors its number labels at the screen centre and starts
	// each graphical row ten proportional units to their right.  Centering the
	// row texture itself moved both the numbers and the offers 55 units left.
	const int crateX = ScreenWidth() / 2 +
		RoundFloatToInt( 10.0f * equipmentScale );
	int crateY = ScreenHeight() / 2 -
		RoundFloatToInt(
			( crateCount * 25.0f + 5.0f ) *
				equipmentScale * 0.5f );
	if ( FoFHudCurrentMode() == 6 )
		crateY -= RoundFloatToInt( 130.0f * equipmentScale );
	for ( int i = 0; i < FOF_CRATE_ROW_COUNT; ++i )
	{
		m_pCrateMenuButtons[i]->SetBounds(
			crateX,
			crateY + i * cratePitch,
			crateWide,
			cratePitch );
		m_pCrateMenuButtons[i]->MoveToFront();
	}
}

void CHudFoF::DrawMenuTexture(
	int textureId,
	int x,
	int y,
	int wide,
	int tall,
	const Color &color ) const
{
	if ( textureId < 0 || wide <= 0 || tall <= 0 )
		return;

	vgui::surface()->DrawSetColor( color );
	vgui::surface()->DrawSetTexture( textureId );
	vgui::surface()->DrawTexturedRect( x, y, x + wide, y + tall );
}

// FoF menu command bridges and local console entry points.

static CHudFoF *FoFHudMenu()
{
	return GET_HUDELEMENT( CHudFoF );
}

bool FoFMenuIsOpen()
{
	CHudFoF *hud = FoFHudMenu();
	return FoFVoiceMenuIsOpen() || ( hud && hud->IsMenuOpen() );
}

bool FoFMenuBlocksPlayerMovement()
{
	CHudFoF *hud = FoFHudMenu();
	return hud && hud->BlocksPlayerMovement();
}

bool FoFMenuSelectDisplaySlot( int slot )
{
	if ( FoFVoiceMenuIsOpen() )
		return FoFVoiceMenuSelectDisplaySlot( slot );
	CHudFoF *hud = FoFHudMenu();
	return hud && hud->SelectDisplaySlot( slot );
}

bool FoFMenuClose()
{
	if ( FoFVoiceMenuClose() )
		return true;
	CHudFoF *hud = FoFHudMenu();
	if ( !hud )
		return false;
	if ( hud->IsSlideOpen() )
	{
		hud->CloseSlide();
		return true;
	}
	if ( !hud->IsMenuOpen() )
		return false;
	hud->CloseMenu();
	return true;
}

CON_COMMAND( chooseteam, "Open the FoF team menu." )
{
	CHudFoF *hud = FoFHudMenu();
	if ( hud )
		hud->ShowTeamMenu();
}

CON_COMMAND( changeteam, "Open the FoF team menu." )
{
	CHudFoF *hud = FoFHudMenu();
	if ( hud )
		hud->ShowTeamMenu();
}

CON_COMMAND( equipmenu, "Open the FoF loadout preset menu." )
{
	CHudFoF *hud = FoFHudMenu();
	if ( hud )
		hud->ShowEquipmentMenu();
}

CON_COMMAND( autobuy, "Open the quick FoF purchase panel." )
{
	CHudFoF *hud = FoFHudMenu();
	if ( hud )
		hud->ToggleAutoBuyMenu();
}

CON_COMMAND( rebuy, "Buy the last selected FoF loadout again." )
{
	CHudFoF *hud = FoFHudMenu();
	if ( hud )
		hud->RebuyEquipment();
}

// FoF menu input ownership and local panel interaction.

bool FoFHudExternalUIOwnsInput()
{
	return ( enginevgui && enginevgui->IsGameUIVisible() ) ||
		( engine && engine->Con_IsVisible() );
}

void CHudFoF::BeginLocalMenuInput()
{
	const bool keyboardOnlyCrate =
		m_iMenuKind == FOF_MENU_CRATE;
	const bool pointerMenu =
		m_iMenuKind == FOF_MENU_TEAM ||
		m_iMenuKind == FOF_MENU_TEAM_CLASS ||
		m_iMenuKind == FOF_MENU_EQUIPMENT ||
		m_iMenuKind == FOF_MENU_PURCHASE;
	// MenuFoF is a number-key overlay, not a cursor-driven VGUI dialog. Keep
	// the game mouse active so the original server can observe the player
	// turning away from the crate and close the offer at its normal angle/range.
	// Team/equipment panels retain their shipped hover/click interaction.
	if ( keyboardOnlyCrate )
	{
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
			vgui::input()->ReleaseAppModalSurface();
		vgui::surface()->SetCursorAlwaysVisible( false );
		if ( ::input )
			::input->ActivateMouse();
		return;
	}

	// The original team and BuyMenuDM panels are mouse-interactive but are
	// not app-modal.
	// Making this full-screen HUD panel modal prevents the higher GameUI and
	// console surfaces from receiving Escape, tilde, or mouse clicks. Keep
	// keyboard focus in the engine and only expose the menu's child buttons.
	if ( pointerMenu )
	{
		if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
			vgui::input()->ReleaseAppModalSurface();
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( false );
		SetCursor( vgui::dc_arrow );
		// The HUD viewport itself is normally mouse-disabled.  A non-modal
		// popup lets these child buttons receive pointer input without making
		// the full-screen panel an app-modal keyboard owner.
		MakePopup( false );
		MoveToFront();
		if ( ::input )
			::input->DeactivateMouse();
		// DeactivateMouse() intentionally does nothing when the engine mouse
		// was already inactive.  In that case the client viewport can still
		// own dc_none from ClientModeShared::Enable(), leaving working menu
		// buttons with no visible pointer.  Restore the cursor explicitly.
		vgui::surface()->UnlockCursor();
		vgui::surface()->SetCursor( vgui::dc_arrow );
		vgui::surface()->SetCursorAlwaysVisible( true );
		return;
	}

	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	MakePopup( false );
	MoveToFront();
	RequestFocus();
	if ( ::input )
		::input->DeactivateMouse();
	vgui::input()->SetAppModalSurface( GetVPanel() );
	vgui::surface()->SetCursorAlwaysVisible( true );
}

void CHudFoF::PaintMenu()
{
	if ( !m_bMenuVisible )
		return;

	if ( m_iMenuKind == FOF_MENU_TEAM )
	{
		PaintTeamMenu();
		return;
	}
	if ( m_iMenuKind == FOF_MENU_EQUIPMENT )
	{
		// A dedicated child layer paints this after PaintTraverse has applied
		// BuyMenuDM's modal alpha.
		return;
	}
	if ( m_iMenuKind == FOF_MENU_TEAM_CLASS )
	{
		// A dedicated child layer owns the textured class cards.
		return;
	}
	if ( m_iMenuKind == FOF_MENU_PURCHASE )
	{
		// The dedicated purchase child layer owns all card artwork.
		return;
	}
	if ( m_iMenuKind == FOF_MENU_CRATE )
	{
		PaintCrateMenu();
		return;
	}
	if ( m_iMenuKind == FOF_MENU_TEXT )
	{
		PaintTextMenu();
		return;
	}
}

void CHudFoF::OnCommand( const char *command )
{
	if ( !command )
		return;
	if ( FoFHudIsSourceTVClient() &&
		( m_iMenuKind == FOF_MENU_TEAM ||
			m_iMenuKind == FOF_MENU_TEAM_CLASS ||
			m_iMenuKind == FOF_MENU_EQUIPMENT ||
			m_iMenuKind == FOF_MENU_PURCHASE ) )
	{
		ClearMenu();
		return;
	}

	if ( !Q_stricmp( command, "fof_equip_help_close" ) )
	{
		CloseEquipmentHelp();
		return;
	}
	if ( !Q_strnicmp( command, "fof_equip_help_", 15 ) )
	{
		ShowEquipmentHelp( Q_atoi( command + 15 ) );
		return;
	}

	if ( !Q_stricmp( command, "fof_menu_cancel" ) ||
		!Q_stricmp( command, "fof_team_cancel" ) )
	{
		ClearMenu();
		return;
	}
	if ( !Q_stricmp( command, "fof_team_auto" ) )
	{
		m_bIntentionalSpectator = false;
		// FoF CTeamMenuFoF::OnCommand sends autojoin through
		// IVEngineClient::ClientCmd.  The local
		// command path preserves the Shootout voice/model userinfo handshake.
		engine->ClientCmd( "autojoin" );
		ClearMenu();
		return;
	}
	if ( !Q_stricmp( command, "fof_team_intro" ) )
	{
		if ( m_iMenuKind == FOF_MENU_TEAM )
		{
			ClearMenu();
			m_Slide = FoFHudTeamIntroSlideName();
			m_bSlidePending = false;
			if ( m_Slide.IsEmpty() ||
				!LoadSlide( m_Slide.String() ) )
				m_Slide.Clear();
		}
		return;
	}
	if ( !Q_stricmp( command, "fof_team_spectator" ) )
	{
		m_bIntentionalSpectator = true;
		m_bTeamMenuOffered = true;
		engine->ServerCmd( "spectator" );
		ClearMenu();
		return;
	}
	if ( !Q_strnicmp( command, "fof_team_", 9 ) &&
		command[9] >= '2' && command[9] <= '5' &&
		command[10] == '\0' )
	{
		m_bIntentionalSpectator = false;
		// A stale panel can survive the sign-on boundary for a frame while a
		// local "map" command replaces a team server with Shootout FFA.  Never
		// submit a numbered team in FFA: the stock server rejects those as
		// #FullTeam even though the server itself has free player slots.
		if ( !FoFHudTeamplayEnabled() )
		{
			engine->ClientCmd( "autojoin" );
			ClearMenu();
			return;
		}

		char serverCommand[32];
		Q_snprintf(
			serverCommand,
			sizeof( serverCommand ),
			"jointeam %c",
			command[9] );
		engine->ServerCmd( serverCommand );
		ClearMenu();
		return;
	}
	if ( !Q_strnicmp( command, "fof_equip_item_", 15 ) )
	{
		if ( m_iMenuKind == FOF_MENU_EQUIPMENT )
			ToggleEquipmentItem( Q_atoi( command + 15 ) );
		return;
	}
	if ( !Q_strnicmp( command, "fof_team_class_", 15 ) )
	{
		const int classIndex = Q_atoi( command + 15 );
		if ( m_iMenuKind == FOF_MENU_TEAM_CLASS &&
			classIndex >= 0 && classIndex < FOF_TEAM_CLASS_COUNT &&
			IsTeamClassAvailable( classIndex ) )
		{
			char classCommand[32];
			Q_snprintf( classCommand, sizeof( classCommand ),
				"tp_class %d", classIndex );
			// ClassMenuFoF dispatches this through ClientCmd, then closes the
			// coordinator through the same local path as vgui_close.
			engine->ClientCmd( classCommand );
			ClearMenu();
		}
		return;
	}
	if ( !Q_strnicmp( command, "fof_purchase_preset_", 20 ) )
	{
		SelectPurchasePreset( Q_atoi( command + 20 ) );
		return;
	}
	if ( !Q_stricmp( command, "fof_purchase_page_up" ) )
	{
		ChangePurchasePage( -1 );
		return;
	}
	if ( !Q_stricmp( command, "fof_purchase_page_down" ) )
	{
		ChangePurchasePage( 1 );
		return;
	}
	if ( !Q_stricmp( command, "fof_purchase_switch" ) )
	{
		TogglePurchaseLayout();
		return;
	}
	if ( !Q_strnicmp( command, "fof_purchase_filter_", 20 ) )
	{
		SetPurchaseFilter( Q_atoi( command + 20 ) );
		return;
	}
	if ( !Q_stricmp( command, "fof_purchase_editor" ) )
	{
		ToggleLoadoutEditorMode();
		return;
	}
	if ( !Q_strnicmp( command, "fof_loadout_item_", 17 ) )
	{
		AddLoadoutEditorItem( Q_atoi( command + 17 ) );
		return;
	}
	if ( !Q_strnicmp( command, "fof_loadout_slot_", 17 ) )
	{
		RemoveLoadoutEditorItem( Q_atoi( command + 17 ) );
		return;
	}
	if ( !Q_stricmp( command, "fof_loadout_save" ) )
	{
		SaveLoadoutEditor();
		return;
	}
	if ( !Q_stricmp( command, "fof_loadout_delete" ) )
	{
		DeleteLoadoutEditor();
		return;
	}
	if ( !Q_stricmp( command, "fof_loadout_cancel" ) )
	{
		CloseLoadoutEditor();
		return;
	}
	if ( !Q_strnicmp( command, "fof_crate_row_", 14 ) )
	{
		if ( m_iMenuKind == FOF_MENU_CRATE )
		{
			const int row = Q_atoi( command + 14 );
			if ( row >= 0 && row < FOF_CRATE_ROW_COUNT )
				SelectDisplaySlot( row == 9 ? 0 : row + 1 );
		}
		return;
	}
	if ( !Q_stricmp( command, "fof_equip_accept" ) )
	{
		if ( m_iMenuKind == FOF_MENU_EQUIPMENT )
			AcceptEquipmentSelection();
		return;
	}
	if ( !Q_stricmp( command, "go_forward" ) )
	{
		StepSlide( 1 );
		return;
	}
	if ( !Q_stricmp( command, "go_back" ) )
	{
		StepSlide( -1 );
		return;
	}
	if ( !Q_stricmp( command, "vguicancel" ) )
	{
		CloseSlide();
		return;
	}
	BaseClass::OnCommand( command );
}

void CHudFoF::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( m_bMenuVisible && m_bLocalMenu )
	{
		if ( m_iMenuKind == FOF_MENU_PURCHASE )
		{
			if ( m_bPurchaseEditMode && !m_bLoadoutEditorVisible &&
				code == KEY_SPACE )
			{
				OpenNewLoadoutEditor();
				return;
			}
		}
		if ( m_iEquipmentHelpItem >= 0 )
		{
			if ( code == KEY_ESCAPE || code == KEY_ENTER ||
				code == KEY_SPACE )
			{
				CloseEquipmentHelp();
			}
			return;
		}

		int nDisplaySlot = -1;
		if ( code >= KEY_0 && code <= KEY_9 )
			nDisplaySlot = code == KEY_0 ? 0 : code - KEY_0;
		else if ( code >= KEY_PAD_0 && code <= KEY_PAD_9 )
			nDisplaySlot = code == KEY_PAD_0 ? 0 : code - KEY_PAD_0;

		// Preserve direct VGUI key handling as a fallback.  Normal team and
		// equipment input remains non-modal and reaches the engine's existing
		// slot-command bridge first.
		if ( nDisplaySlot >= 0 && SelectDisplaySlot( nDisplaySlot ) )
			return;

		if ( code == KEY_ESCAPE )
		{
			ClearMenu();
			return;
		}
		if ( code == KEY_ENTER &&
			m_iMenuKind == FOF_MENU_EQUIPMENT )
		{
			AcceptEquipmentSelection();
			return;
		}
	}
	if ( m_bSlideVisible )
	{
		if ( code == KEY_ESCAPE )
		{
			CloseSlide();
			return;
		}
		if ( code == KEY_LEFT )
		{
			StepSlide( -1 );
			return;
		}
		if ( code == KEY_RIGHT ||
			code == KEY_ENTER ||
			code == KEY_SPACE )
		{
			StepSlide( 1 );
			return;
		}
	}
	BaseClass::OnKeyCodePressed( code );
}

bool CHudFoF::HandleMenuKeyInput( int down, int keynum )
{
	if ( !down || keynum != KEY_SPACE || !m_bMenuVisible ||
		!m_bLocalMenu || m_iMenuKind != FOF_MENU_PURCHASE ||
		!m_bPurchaseEditMode || m_bLoadoutEditorVisible )
	{
		return false;
	}

	OpenNewLoadoutEditor();
	return true;
}

// FoF menu wire protocol and menu state transitions.

static bool FoFParseEncodedMenuLine(
	const char *text,
	FoFMenuEntry &entry )
{
	entry.encodedKind = '\0';
	entry.encodedValue = 0;
	entry.encodedQuantity = 0;
	entry.encodedLabel.Clear();
	entry.encodedValid = false;

	// The first byte is a deliberate spacer in the server's wire format.  The
	// second byte discriminates a cash offer from a progression lock.
	if ( !text || !text[0] ||
		( text[1] != '$' && text[1] != '*' ) )
	{
		return false;
	}

	const char *value = text + 2;
	const char *firstComma = Q_strstr( value, "," );
	if ( !firstComma )
		return false;

	const char *label = firstComma + 1;
	while ( *label == ' ' || *label == '\t' )
		++label;
	const char *secondComma = Q_strstr( label, "," );
	if ( !secondComma )
		return false;

	const char *labelEnd = secondComma;
	while ( labelEnd > label &&
		( labelEnd[-1] == ' ' || labelEnd[-1] == '\t' ) )
	{
		--labelEnd;
	}

	// The item-name field is optional.  The original comma-token parser keeps
	// rows such as " $15, ,0", draws the commandId-mapped icon and price, and
	// simply appends no localized name.
	char labelToken[128];
	const int labelLength = MIN(
		(int)( labelEnd - label ),
		(int)sizeof( labelToken ) - 1 );
	Q_memcpy( labelToken, label, labelLength );
	labelToken[labelLength] = '\0';

	entry.encodedKind = text[1];
	entry.encodedValue = Q_atoi( value );
	entry.encodedQuantity = Q_atoi( secondComma + 1 );
	entry.encodedLabel = labelToken;
	entry.encodedValid = true;
	return true;
}

static bool FoFUsesDeathmatchLoadoutProtocol()
{
	const int mode = FoFHudCurrentMode();
	return mode == 1 || mode == 4;
}

void CHudFoF::ClearMenu()
{
	const bool wasLocalMenu = m_bLocalMenu;
	m_iEquipmentHelpItem = -1;
	m_bPurchaseEditMode = false;
	m_bLoadoutEditorVisible = false;
	m_iLoadoutEditorSourceOrder = -1;
	m_iLoadoutEditorItemCount = 0;
	Q_memset( m_LoadoutEditorItems, 0xff,
		sizeof( m_LoadoutEditorItems ) );
	SetMenuControlsVisible( false );
	m_MenuEntries.Purge();
	m_MenuTitle.Clear();
	m_bMenuVisible = false;
	m_bLocalMenu = false;
	m_iMenuKind = FOF_MENU_NONE;
	m_flMenuReadyAt = 0.0f;

	if ( wasLocalMenu && !m_bSlideVisible )
	{
		if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
			vgui::input()->ReleaseAppModalSurface();
		// Do not hide or recapture a cursor that currently belongs to the
		// Escape menu or developer console.  Their owner will restore the
		// gameplay mouse when that external surface closes.
		if ( !FoFHudExternalUIOwnsInput() )
		{
			vgui::surface()->SetCursorAlwaysVisible( false );
			if ( ::input && engine->IsInGame() )
				::input->ActivateMouse();
		}
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
	}
}

void CHudFoF::ShowEquipmentMenu( int page )
{
	const int mode = FoFHudCurrentMode();
	// Original equipmenu callback is disabled by this replicated server switch
	// and is never available in Course mode.
	if ( FoFHudConVarInt( "fof_sv_weaponmenu", 1 ) == 0 || mode == 5 )
	{
		if ( m_bMenuVisible &&
			( m_iMenuKind == FOF_MENU_TEAM_CLASS ||
				m_iMenuKind == FOF_MENU_EQUIPMENT ||
				m_iMenuKind == FOF_MENU_PURCHASE ) )
		{
			ClearMenu();
		}
		return;
	}

	if ( mode == 2 &&
		FoFHudConVarInt( "fof_sv_tp_classes", 0 ) != 0 )
	{
		ShowTeamClassMenu();
		return;
	}
	if ( mode == 2 || mode == 3 )
	{
		ShowPurchaseMenu( page );
		return;
	}

	if ( FoFHudIsSourceTVClient() )
	{
		if ( m_bMenuVisible &&
			( m_iMenuKind == FOF_MENU_TEAM ||
				m_iMenuKind == FOF_MENU_TEAM_CLASS ||
				m_iMenuKind == FOF_MENU_EQUIPMENT ||
				m_iMenuKind == FOF_MENU_PURCHASE ) )
		{
			ClearMenu();
		}
		return;
	}

	if ( IsSlideOpen() )
		CloseSlide();
	ClearMenu();
	LoadEquipmentSelection();
	m_bMenuVisible = true;
	m_bLocalMenu = true;
	m_iMenuKind = FOF_MENU_EQUIPMENT;
	LayoutMenuControls();
	SetMenuControlsVisible( true );
	BeginLocalMenuInput();
}

void CHudFoF::SetEquipmentMenuVisible( bool visible )
{
	if ( visible )
	{
		ShowEquipmentMenu();
		return;
	}

	// buypreset_main is shared by the deathmatch loadout, Teamplay class and
	// cash-purchase panels.  A delayed close must not dismiss the team menu or
	// an unrelated server text menu that replaced it.
	if ( m_bMenuVisible &&
		( m_iMenuKind == FOF_MENU_TEAM_CLASS ||
			m_iMenuKind == FOF_MENU_EQUIPMENT ||
			m_iMenuKind == FOF_MENU_PURCHASE ) )
	{
		ClearMenu();
	}
}

void CHudFoF::SubmitEquipment( const int *items, int count )
{
	if ( FoFHudIsSourceTVClient() || !items || count <= 0 )
		return;

	count = MIN( count, ARRAYSIZE( m_LastPresetItems ) );
	// The original server accepts item_dm_end in Shootout (mode 1) and Team
	// Elimination (mode 4).  Other team modes use the cash-purchase
	// fof_buy/buy_end transaction instead.  mp_teamplay cannot distinguish
	// these protocols because it is enabled for both mode 2 and mode 4.
	const bool deathmatchLoadout = FoFUsesDeathmatchLoadoutProtocol();
	if ( deathmatchLoadout )
		engine->ServerCmd( "new" );

	for ( int i = 0; i < count; ++i )
	{
		char command[32];
		Q_snprintf( command, sizeof( command ), deathmatchLoadout ? "item_dm %d" : "fof_buy %d", items[i] );
		// PresetMenu dispatches the cash protocol through
		// IVEngineClient::ClientCmd.  Sending fof_buy directly with ServerCmd
		// bypasses the local command path used by the shipped client and the
		// original server does not complete the preset transaction reliably.
		if ( deathmatchLoadout )
			engine->ServerCmd( command );
		else
			engine->ClientCmd( command );
	}
	if ( !deathmatchLoadout )
	{
		Q_memset( m_LastPresetItems, 0, sizeof( m_LastPresetItems ) );
		for ( int i = 0; i < count; ++i )
			m_LastPresetItems[i] = items[i];
	}
	if ( deathmatchLoadout )
		engine->ServerCmd( "item_dm_end 0 0" );
	else
	{
		// PresetMenu::BuyPreset asks FoFGameRules::IsTeamplay at original
		// The original client selects buy_end 1 for Break Bad/team modes,
		// otherwise buy_end 0.
		engine->ClientCmd(
			FoFHudTeamplayEnabled() ? "buy_end 1" : "buy_end 0" );
	}
}

void CHudFoF::RebuyEquipment()
{
	if ( FoFHudIsSourceTVClient() )
		return;

	// FoF's PresetMenu::Rebuy walks all six remembered cash-purchase
	// slots, submits each positive item through ClientCmd, then sends buy_end
	// even when no prior preset exists.  It deliberately does not reuse the
	// Shootout item_dm protocol or the current deathmatch equipment selection.
	for ( int i = 0; i < ARRAYSIZE( m_LastPresetItems ); ++i )
	{
		if ( m_LastPresetItems[i] <= 0 )
			continue;
		char command[32];
		Q_snprintf( command, sizeof( command ),
			"fof_buy %d", m_LastPresetItems[i] );
		engine->ClientCmd( command );
	}
	engine->ClientCmd( "buy_end" );
}

void CHudFoF::ReceiveMenuLine( const char *label, bool more, int commandId )
{
	if ( commandId == -999 )
	{
		ClearMenu();
		return;
	}

	if ( m_bLocalMenu )
		ClearMenu();

	// Generic text menus use command 1 as their first-item marker.  Encoded
	// crate rows index an image table with that same signed value, so command
	// 1 is data there and must not discard preceding rows.
	const bool encodedLine =
		label && label[0] &&
		( label[1] == '$' || label[1] == '*' );
	if ( commandId == 1 && !encodedLine )
	{
		SetMenuControlsVisible( false );
		m_MenuEntries.Purge();
		m_bMenuVisible = false;
	}

	FoFMenuEntry entry;
	entry.label = label ? label : "";
	entry.commandId = commandId;
	entry.clientCommand.Clear();
	entry.presetCount = 0;
	entry.nextPage = -1;
	FoFParseEncodedMenuLine( label, entry );
	if ( m_MenuEntries.Count() < FOF_CRATE_ROW_COUNT )
		m_MenuEntries.AddToTail( entry );
	m_bLocalMenu = false;
	m_iMenuKind = FOF_MENU_TEXT;
	if ( more )
		return;

	bool encodedMenu = m_MenuEntries.Count() > 0;
	for ( int i = 0; i < m_MenuEntries.Count(); ++i )
	{
		if ( !m_MenuEntries[i].encodedValid )
		{
			encodedMenu = false;
			break;
		}
	}

	m_bMenuVisible = true;
	if ( encodedMenu )
	{
		m_bLocalMenu = true;
		m_iMenuKind = FOF_MENU_CRATE;
		m_flMenuReadyAt =
			( gpGlobals ? gpGlobals->curtime : 0.0f ) + 1.0f;
		LayoutMenuControls();
		SetMenuControlsVisible( true );
		BeginLocalMenuInput();
	}
	else
	{
		// CHudMenuFoF is a non-modal number-key overlay.  The original starts
		// its one-second alpha ramp only after the final ShowMenuFoF fragment.
		m_flMenuReadyAt =
			( gpGlobals ? gpGlobals->curtime : 0.0f ) + 1.0f;
		LayoutMenuControls();
		SetMenuControlsVisible( true );
	}
}

bool CHudFoF::SelectDisplaySlot( int slot )
{
	if ( !m_bMenuVisible )
		return false;
	if ( FoFHudIsSourceTVClient() &&
		( m_iMenuKind == FOF_MENU_TEAM ||
			m_iMenuKind == FOF_MENU_TEAM_CLASS ||
			m_iMenuKind == FOF_MENU_EQUIPMENT ||
			m_iMenuKind == FOF_MENU_PURCHASE ) )
	{
		ClearMenu();
		return true;
	}

	if ( m_iMenuKind == FOF_MENU_EQUIPMENT )
	{
		if ( slot == 0 )
			AcceptEquipmentSelection();
		else if ( slot >= 1 && slot <= 9 )
			ToggleEquipmentItem( slot - 1 );
		return true;
	}
	if ( m_iMenuKind == FOF_MENU_PURCHASE )
	{
		// The original preset grid is pointer-driven. Consume number keys so
		// opening it cannot also switch the player's active weapon.
		return true;
	}
	if ( m_iMenuKind == FOF_MENU_TEAM_CLASS )
	{
		// ClassMenuFoF is pointer-driven.  Consume number keys while it owns
		// movement input so they cannot also switch the active weapon.
		return true;
	}

	const int row = slot == 0 ? 9 : slot - 1;
	if ( row < 0 || row >= m_MenuEntries.Count() || row >= 10 )
		return true;

	FoFMenuEntry &entry = m_MenuEntries[row];
	if ( m_iMenuKind == FOF_MENU_CRATE )
	{
		const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
		if ( now < m_flMenuReadyAt )
			return true;

		// Do not gate this on local cash.  The original forwards even a dimmed,
		// unaffordable numeric selection and lets the server reject it.  A
		// progression-lock row is displayed but cannot be selected.
		if ( !entry.encodedValid ||
			entry.encodedKind != '$' ||
			entry.commandId < 0 ||
			entry.commandId >= FOF_CRATE_ITEM_COUNT )
		{
			return true;
		}
	}
	if ( entry.nextPage >= 0 )
	{
		const int nextPage = entry.nextPage;
		ShowEquipmentMenu( nextPage );
		return true;
	}
	if ( entry.presetCount > 0 )
	{
		int items[6];
		const int count = MIN( entry.presetCount, ARRAYSIZE( items ) );
		for ( int i = 0; i < count; ++i )
			items[i] = entry.presetItems[i];
		SubmitEquipment( items, count );
		ClearMenu();
		return true;
	}

	char command[64];
	if ( !entry.clientCommand.IsEmpty() )
	{
		Q_strncpy( command, entry.clientCommand.String(), sizeof( command ) );
		if ( !Q_stricmp( command, "spectator" ) )
		{
			m_bIntentionalSpectator = true;
			m_bTeamMenuOffered = true;
		}
		else if ( !Q_stricmp( command, "autojoin" ) ||
			!Q_strnicmp( command, "jointeam ", 9 ) )
		{
			m_bIntentionalSpectator = false;
		}
	}
	else
	{
		Q_snprintf( command, sizeof( command ), "menuselect_fof %d", entry.commandId );
	}
	// FoF's team-menu handler sends autojoin through ClientCmd
	// in the original client.  Bypassing that local path skips the
	// Shootout voice/model userinfo handshake.  Keep numbered team choices
	// and encoded menu selections on the proven direct server path.
	if ( !Q_stricmp( command, "autojoin" ) )
		engine->ClientCmd( command );
	else
		engine->ServerCmd( command );
	ClearMenu();
	return true;
}

// FoF's plain ShowMenuFoF presentation, used by the server-owned votekick UI.

static int FoFTextMenuAlpha( float readyAt )
{
	if ( !gpGlobals || readyAt <= gpGlobals->curtime )
		return 255;

	// CHudMenuFoF::OnThink remaps the remaining time from [2, 0] to
	// [0, 255].  ShowMenuFoF initializes one second, so a fresh menu begins
	// at half alpha and reaches full opacity after the first second.
	return clamp(
		RoundFloatToInt( RemapValClamped(
			readyAt - gpGlobals->curtime,
			2.0f,
			0.0f,
			0.0f,
			255.0f ) ),
		0,
		255 );
}

void CHudFoF::PaintTextMenu()
{
	EnsureMenuTextures();

	const int visibleCount = MIN(
		m_MenuEntries.Count(), FOF_CRATE_ROW_COUNT );
	if ( visibleCount <= 0 )
		return;

	const float scale = MAX(
		(float)ScreenHeight() / 480.0f, 0.1f );
	const int numberWide = MAX(
		RoundFloatToInt( 10.0f * scale ), 1 );
	const int backgroundInset = MAX(
		RoundFloatToInt( 2.0f * scale ), 1 );
	const int menuAlpha = FoFTextMenuAlpha( m_flMenuReadyAt );
	vgui::IScheme *scheme =
		vgui::scheme()->GetIScheme( GetScheme() );
	const vgui::HFont font = scheme ?
		scheme->GetFont( "Default", true ) : m_hSmallFont;

	int firstX = 0;
	int firstY = 0;
	int firstWide = 0;
	int firstTall = 0;
	m_pCrateMenuButtons[0]->GetBounds(
		firstX, firstY, firstWide, firstTall );
	int lastX = 0;
	int lastY = 0;
	int lastWide = 0;
	int lastTall = 0;
	m_pCrateMenuButtons[visibleCount - 1]->GetBounds(
		lastX, lastY, lastWide, lastTall );

	vgui::surface()->DrawSetColor(
		Color( 50, 50, 55, 200 * menuAlpha / 255 ) );
	vgui::surface()->DrawFilledRect(
		firstX - numberWide - backgroundInset,
		firstY - backgroundInset,
		MAX( firstX + firstWide, lastX + lastWide ) +
			backgroundInset,
		lastY + lastTall + backgroundInset );

	wchar_t localizedBuffer[512];
	for ( int i = 0; i < visibleCount; ++i )
	{
		FoFMenuEntry &entry = m_MenuEntries[i];
		vgui::Button *button = m_pCrateMenuButtons[i];
		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		button->GetBounds( x, y, wide, tall );

		// Player indexes 0..55 use the command-id bitmap table. Negative
		// pagination commands and CLOSE (99) use the disabled preset.
		int textureId = m_iCrateItemTextures[0];
		if ( entry.commandId >= 0 &&
			entry.commandId < FOF_CRATE_ITEM_COUNT )
		{
			textureId = m_iCrateItemTextures[entry.commandId];
		}
		DrawMenuTexture(
			textureId,
			x,
			y,
			wide,
			tall,
			Color( 234, 234, 234, menuAlpha ) );

		const wchar_t *label = Localize(
			entry.label.String(),
			localizedBuffer,
			sizeof( localizedBuffer ) );
		button->SetEnabled( true );
		button->SetFont( font );
		button->SetTextInset( 6, 0 );
		button->SetWrap( true );
		button->SetContentAlignment( vgui::Label::a_west );
		button->SetDefaultColor(
			Color( 255, 255, 255, 255 ),
			Color( 0, 0, 0, 0 ) );
		button->SetArmedColor(
			Color( 255, 255, 255, 255 ),
			Color( 0, 0, 0, 0 ) );
		button->SetDepressedColor(
			Color( 128, 0, 0, 200 ),
			Color( 0, 0, 0, 0 ) );
		button->SetAlpha( menuAlpha );
		button->SetText( label );

		wchar_t number[8];
		V_snwprintf(
			number,
			ARRAYSIZE( number ),
			L"%d.",
			( i + 1 ) % 10 );
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor(
			Color( 234, 234, 234, menuAlpha ) );
		vgui::surface()->DrawSetTextPos(
			x - numberWide,
			y + MAX(
				( tall - vgui::surface()->GetFontTall( font ) ) / 2,
				0 ) );
		vgui::surface()->DrawPrintText(
			number, V_wcslen( number ) );
	}
}

ConVar fof_hide_vote_menu(
	"fof_hide_vote_menu", "0",
	FCVAR_ARCHIVE | FCVAR_SERVER_CANNOT_QUERY,
	"Hides stock vote menu, 1 always 2 only when alive " );

bool FoFShouldHideStockVoteMenu()
{
	const int nHideMode = fof_hide_vote_menu.GetInt();
	if ( nHideMode == 1 )
		return true;
	if ( nHideMode != 2 )
		return false;

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	return pLocalPlayer && pLocalPlayer->IsAlive();
}
