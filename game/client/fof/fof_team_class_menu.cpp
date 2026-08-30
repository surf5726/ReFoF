// Teamplay fixed-class selector.

#include "cbase.h"
#include "c_baseplayer.h"
#include "cdll_util.h"
#include "fof/fof_hud.h"
#include "hl2mp_gamerules.h"
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include "fof/fof_team_class_menu.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum FoFTeamClassItemStyle
{
	FOF_TEAM_CLASS_ITEM_NONE = 0,
	FOF_TEAM_CLASS_ITEM_WEAPON,
	FOF_TEAM_CLASS_ITEM_UTILITY,
	FOF_TEAM_CLASS_ITEM_SKILL,
	FOF_TEAM_CLASS_ITEM_EXPLOSIVE
};

struct FoFTeamClassItemVisual
{
	const char *token;
	const char *material;
	int style;
};

static const FoFTeamClassItemVisual g_FoFTeamClassItemVisuals[] =
{
	{ "weapon_peacemaker",       "vgui/peacemaker",       FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_machete",          "vgui/machete",          FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_coachgun",         "vgui/coachgun",         FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_carbine",          "vgui/carbine",          FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_volcanic",         "vgui/volcanic",         FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_bow",              "vgui/bow",              FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_remington_army",   "vgui/nma",              FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_axe",              "vgui/axe",              FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_coltnavy",         "vgui/coltnavy",         FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_schofield",        "vgui/schofield",        FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_shotgun",          "vgui/shotgun",          FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_sawedoff_shotgun", "vgui/sawed_shotgun",    FOF_TEAM_CLASS_ITEM_WEAPON },
	{ "weapon_knife",            "vgui/knife",            FOF_TEAM_CLASS_ITEM_UTILITY },
	{ "weapon_deringer",         "vgui/deringer",         FOF_TEAM_CLASS_ITEM_UTILITY },
	{ "weapon_whiskey",          "vgui/whiskey",          FOF_TEAM_CLASS_ITEM_UTILITY },
	{ "weapon_dynamite",         "vgui/dynamite",         FOF_TEAM_CLASS_ITEM_EXPLOSIVE },
	{ "weapon_dynamite_belt",    "vgui/dynamite_belt",    FOF_TEAM_CLASS_ITEM_EXPLOSIVE },
	{ "skill_right",             "vgui/accuracy_right",   FOF_TEAM_CLASS_ITEM_SKILL },
	{ "skill_left",              "vgui/accuracy_left",    FOF_TEAM_CLASS_ITEM_SKILL },
	{ "skill_fan",               "vgui/accuracy_fan",     FOF_TEAM_CLASS_ITEM_SKILL },
	{ "skill_ambi",              "vgui/accuracy_ambi",    FOF_TEAM_CLASS_ITEM_SKILL },
	{ "boots",                   "vgui/boots",            FOF_TEAM_CLASS_ITEM_UTILITY },
	{ "brass_knuckles",          "vgui/brass_knuckles",   FOF_TEAM_CLASS_ITEM_UTILITY }
};

static const FoFTeamClassItemVisual *FoFFindTeamClassItemVisual(
	const char *token )
{
	if ( !token || !token[0] )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( g_FoFTeamClassItemVisuals ); ++i )
	{
		if ( !Q_stricmp( token, g_FoFTeamClassItemVisuals[i].token ) )
			return &g_FoFTeamClassItemVisuals[i];
	}
	return NULL;
}

static const char *FoFTrimTeamClassToken( char *token )
{
	if ( !token )
		return "";

	char *begin = token;
	while ( *begin == ' ' || *begin == '\t' )
		++begin;

	char *end = begin + Q_strlen( begin );
	while ( end > begin &&
		( end[-1] == ' ' || end[-1] == '\t' ) )
	{
		--end;
	}
	*end = '\0';
	return begin;
}

void CHudFoF::EnsureTeamClassMenuControls()
{
	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		if ( m_pTeamClassButtons[classIndex] )
			continue;

		char name[40];
		char command[48];
		Q_snprintf( name, sizeof( name ),
			"FoFTeamClassButton%d", classIndex );
		Q_snprintf( command, sizeof( command ),
			"fof_team_class_%d", classIndex );
		m_pTeamClassButtons[classIndex] = new vgui::Button(
			this, name, "", this, command );
		m_pTeamClassButtons[classIndex]->SetButtonBorderEnabled( false );
		m_pTeamClassButtons[classIndex]->SetPaintBorderEnabled( false );
		m_pTeamClassButtons[classIndex]->DrawFocusBox( false );
		m_pTeamClassButtons[classIndex]->SetPaintEnabled( false );
		m_pTeamClassButtons[classIndex]->SetPaintBackgroundEnabled( false );
		m_pTeamClassButtons[classIndex]->SetMouseInputEnabled( true );
		m_pTeamClassButtons[classIndex]->SetKeyBoardInputEnabled( false );
		m_pTeamClassButtons[classIndex]->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		m_pTeamClassButtons[classIndex]->SetReleasedSound(
			"common/banked2.wav" );
		m_pTeamClassButtons[classIndex]->SetZPos( 30 );
		m_pTeamClassButtons[classIndex]->SetVisible( false );
	}

	if ( !m_pTeamClassCloseButton )
	{
		m_pTeamClassCloseButton = new vgui::Button(
			this,
			"FoFTeamClassClose",
			"X",
			this,
			"fof_menu_cancel" );
		m_pTeamClassCloseButton->SetButtonBorderEnabled( true );
		m_pTeamClassCloseButton->SetPaintBorderEnabled( true );
		m_pTeamClassCloseButton->DrawFocusBox( false );
		m_pTeamClassCloseButton->SetPaintEnabled( true );
		m_pTeamClassCloseButton->SetPaintBackgroundEnabled( true );
		m_pTeamClassCloseButton->SetMouseInputEnabled( true );
		m_pTeamClassCloseButton->SetKeyBoardInputEnabled( false );
		m_pTeamClassCloseButton->SetContentAlignment(
			vgui::Label::a_center );
		m_pTeamClassCloseButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSED );
		m_pTeamClassCloseButton->SetZPos( 30 );
		m_pTeamClassCloseButton->SetVisible( false );
	}
}

void CHudFoF::EnsureTeamClassMenuTextures()
{
	if ( m_iTeamClassBackgroundTexture < 0 )
	{
		m_iTeamClassBackgroundTexture =
			vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iTeamClassBackgroundTexture,
			"vgui/equipmentbg",
			true,
			false );
	}

	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		for ( int item = 0;
			item < FOF_TEAM_CLASS_ITEM_COUNT;
			++item )
		{
			if ( m_iTeamClassItemTextures[classIndex][item] >= 0 ||
				m_TeamClassItemMaterials[classIndex][item].IsEmpty() )
			{
				continue;
			}

			m_iTeamClassItemTextures[classIndex][item] =
				vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile(
				m_iTeamClassItemTextures[classIndex][item],
				m_TeamClassItemMaterials[classIndex][item].String(),
				true,
				false );
		}
	}
}

void CHudFoF::ApplyTeamClassMenuScheme( vgui::IScheme *scheme )
{
	if ( !scheme )
		return;

	// ClassMenuFoF::m_hLargeFont is a proportional panel-animation font whose
	// shipped default is HudSelectionNumbers2.  The same handle is assigned to
	// label%i and to the close button.
	m_hTeamClassFont = scheme->GetFont(
		"HudSelectionNumbers2", true );
	if ( m_hTeamClassFont == vgui::INVALID_FONT )
		m_hTeamClassFont = scheme->GetFont( "MenuFontSmall", true );
	if ( m_hTeamClassFont == vgui::INVALID_FONT )
		m_hTeamClassFont = m_hFont;
	if ( m_pTeamClassCloseButton )
	{
		m_pTeamClassCloseButton->SetFont( m_hTeamClassFont );
		m_pTeamClassCloseButton->SetContentAlignment(
			vgui::Label::a_center );
	}
}

void CHudFoF::LoadTeamClassDefinitions()
{
	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		m_TeamClassNames[classIndex].Clear();
		m_TeamClassValid[classIndex] = false;
		for ( int item = 0;
			item < FOF_TEAM_CLASS_ITEM_COUNT;
			++item )
		{
			m_TeamClassItemStyles[classIndex][item] =
				FOF_TEAM_CLASS_ITEM_NONE;
		}

		char cvarName[40];
		Q_snprintf( cvarName, sizeof( cvarName ),
			"fof_sv_tp_classes_c%d", classIndex );
		ConVarRef definition( cvarName, true );
		if ( !definition.IsValid() )
			continue;

		const char *cursor = definition.GetString();
		if ( !cursor || !cursor[0] || !Q_stricmp( cursor, "empty" ) )
			continue;

		char token[256];
		cursor = nexttoken( token, cursor, ',' );
		const char *className = FoFTrimTeamClassToken( token );
		if ( !className[0] || !Q_stricmp( className, "empty" ) )
			continue;
		m_TeamClassNames[classIndex] = className;
		m_TeamClassValid[classIndex] = true;

		// The second token is the server-side class share.  The original menu
		// does not display it, but consumes it before the six fixed item rows.
		cursor = nexttoken( token, cursor, ',' );
		for ( int item = 0;
			item < FOF_TEAM_CLASS_ITEM_COUNT;
			++item )
		{
			cursor = nexttoken( token, cursor, ',' );
			const char *itemToken = FoFTrimTeamClassToken( token );
			const FoFTeamClassItemVisual *visual =
				FoFFindTeamClassItemVisual( itemToken );
			const char *material = visual ? visual->material : "";
			if ( Q_stricmp(
				m_TeamClassItemMaterials[classIndex][item].String(),
				material ) )
			{
				m_iTeamClassItemTextures[classIndex][item] = -1;
			}
			m_TeamClassItemMaterials[classIndex][item] = material;
			m_TeamClassItemStyles[classIndex][item] = visual ?
				visual->style : FOF_TEAM_CLASS_ITEM_NONE;
		}
	}

	EnsureTeamClassMenuTextures();
}

bool CHudFoF::IsTeamClassAvailable( int classIndex ) const
{
	if ( classIndex < 0 || classIndex >= FOF_TEAM_CLASS_COUNT ||
		!m_TeamClassValid[classIndex] )
	{
		return false;
	}

	const CHL2MPRules *rules = HL2MPRules();
	if ( !rules || rules->GetFoFTeamClassCount() <= 0 )
	{
		// The menu can be requested in the same packet window in which the
		// game-rules proxy is installed.  Preserve the parsed cards for that
		// transient frame; normal OnThink synchronization takes over at once.
		return true;
	}
	if ( classIndex >= rules->GetFoFTeamClassCount() )
		return false;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	const int teamNumber = localPlayer ?
		localPlayer->GetTeamNumber() : TEAM_REBELS;
	return rules->IsFoFTeamClassAvailable( classIndex, teamNumber );
}

void CHudFoF::UpdateTeamClassButtonStates()
{
	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		if ( m_pTeamClassButtons[classIndex] )
		{
			m_pTeamClassButtons[classIndex]->SetEnabled(
				IsTeamClassAvailable( classIndex ) );
		}
	}
}

void CHudFoF::LayoutTeamClassMenuControls()
{
	EnsureTeamClassMenuControls();
	if ( m_pTeamClassPaintPanel )
	{
		m_pTeamClassPaintPanel->SetBounds(
			0, 0, ScreenWidth(), ScreenHeight() );
	}

	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int panelX = ScreenWidth() / 2 -
		RoundFloatToInt( 200.0f * scale );
	const int panelY = ScreenHeight() / 2 -
		RoundFloatToInt( 150.0f * scale );
	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		const int column = classIndex & 3;
		const int row = classIndex >> 2;
		m_pTeamClassButtons[classIndex]->SetBounds(
			panelX + RoundFloatToInt(
				( 5.0f + column * 100.0f ) * scale ),
			panelY + RoundFloatToInt(
				( 5.0f + row * 150.0f ) * scale ),
			MAX( RoundFloatToInt( 90.0f * scale ), 1 ),
			MAX( RoundFloatToInt( 140.0f * scale ), 1 ) );
		m_pTeamClassButtons[classIndex]->MoveToFront();
	}

	if ( m_pTeamClassCloseButton )
	{
		m_pTeamClassCloseButton->SetBounds(
			panelX + RoundFloatToInt( 400.0f * scale ),
			panelY,
			MAX( RoundFloatToInt( 10.0f * scale ), 1 ),
			MAX( RoundFloatToInt( 10.0f * scale ), 1 ) );
		m_pTeamClassCloseButton->MoveToFront();
	}
}

void CHudFoF::SetTeamClassMenuControlsVisible( bool visible )
{
	const bool show = visible && m_iMenuKind == FOF_MENU_TEAM_CLASS;
	if ( m_pTeamClassPaintPanel )
		m_pTeamClassPaintPanel->SetVisible( show );

	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		if ( m_pTeamClassButtons[classIndex] )
		{
			m_pTeamClassButtons[classIndex]->SetVisible(
				show && m_TeamClassValid[classIndex] );
		}
	}
	if ( m_pTeamClassCloseButton )
		m_pTeamClassCloseButton->SetVisible( show );
	if ( show )
		UpdateTeamClassButtonStates();
}

void CHudFoF::ShowTeamClassMenu()
{
	if ( FoFHudIsSourceTVClient() )
	{
		if ( m_bMenuVisible && m_iMenuKind == FOF_MENU_TEAM_CLASS )
			ClearMenu();
		return;
	}

	if ( IsSlideOpen() )
		CloseSlide();
	ClearMenu();
	LoadTeamClassDefinitions();
	m_bMenuVisible = true;
	m_bLocalMenu = true;
	m_iMenuKind = FOF_MENU_TEAM_CLASS;
	LayoutMenuControls();
	SetMenuControlsVisible( true );
	BeginLocalMenuInput();
}

void CHudFoF::SetTeamClassMenuVisible( bool visible )
{
	if ( visible )
	{
		ShowTeamClassMenu();
		return;
	}

	// A delayed server close for PANEL_CLASS must not dismiss a different
	// local menu that the player opened in the meantime.
	if ( m_bMenuVisible && m_iMenuKind == FOF_MENU_TEAM_CLASS )
		ClearMenu();
}

// ClassMenuFoF card and item-row painting.

class CFoFTeamClassPaintPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFoFTeamClassPaintPanel, vgui::Panel );

public:
	CFoFTeamClassPaintPanel( CHudFoF *owner )
		: BaseClass( owner, "FoFTeamClassPaintLayer" )
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
			m_pOwner->PaintTeamClassMenu();
	}

private:
	CHudFoF *m_pOwner;
};

vgui::Panel *FoFCreateTeamClassPaintPanel( CHudFoF *owner )
{
	return new CFoFTeamClassPaintPanel( owner );
}

static Color FoFTeamClassItemColor( int style, bool enabled )
{
	Color color;
	switch ( style )
	{
	case 1:
		color = Color( 24, 42, 54, 205 );
		break;
	case 2:
		color = Color( 64, 42, 54, 205 );
		break;
	case 3:
		color = Color( 116, 82, 38, 205 );
		break;
	case 4:
		color = Color( 91, 42, 31, 215 );
		break;
	default:
		color = Color( 0, 0, 0, 0 );
		break;
	}
	if ( !enabled )
	{
		color.SetColor(
			color.r() / 2,
			color.g() / 2,
			color.b() / 2,
			MIN( color.a(), 120 ) );
	}
	return color;
}

void CHudFoF::PaintTeamClassMenu()
{
	if ( !m_bMenuVisible || m_iMenuKind != FOF_MENU_TEAM_CLASS )
		return;

	EnsureMenuTextures();
	EnsureTeamClassMenuTextures();
	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int panelX = ScreenWidth() / 2 -
		RoundFloatToInt( 200.0f * scale );
	const int panelY = ScreenHeight() / 2 -
		RoundFloatToInt( 150.0f * scale );
	DrawMenuTexture(
		m_iTeamClassBackgroundTexture,
		panelX,
		panelY,
		MAX( RoundFloatToInt( 400.0f * scale ), 1 ),
		MAX( RoundFloatToInt( 300.0f * scale ), 1 ),
		Color( 255, 255, 255, 255 ) );

	const vgui::HFont titleFont =
		m_hTeamClassFont != vgui::INVALID_FONT ?
			m_hTeamClassFont : m_hFont;
	for ( int classIndex = 0;
		classIndex < FOF_TEAM_CLASS_COUNT;
		++classIndex )
	{
		vgui::Button *button = m_pTeamClassButtons[classIndex];
		if ( !button || !button->IsVisible() )
			continue;

		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		button->GetBounds( x, y, wide, tall );
		const bool enabled = button->IsEnabled();
		// ClassMenuFoF uses the same equipmentbg bitmap for every state of
		// bg%i%i.  These are the exact four-state modulation colors from the
		// original constructor (normal, disabled, armed/depressed).
		Color cardColor( 184, 184, 184, 255 );
		if ( !enabled )
			cardColor = Color( 130, 90, 90, 155 );
		else if ( button->IsDepressed() )
			cardColor = Color( 255, 255, 0, 255 );
		else if ( button->IsArmed() )
			cardColor = Color( 255, 255, 0, 255 );

		DrawMenuTexture(
			m_iTeamClassBackgroundTexture,
			x,
			y,
			wide,
			tall,
			cardColor );

		wchar_t title[128];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			m_TeamClassNames[classIndex].String(),
			title,
			sizeof( title ) );
		vgui::surface()->DrawSetTextFont( titleFont );
		vgui::surface()->DrawSetTextColor(
			enabled ? Color( 200, 200, 200, 255 ) :
				Color( 130, 90, 90, 155 ) );
		vgui::surface()->DrawSetTextPos(
			x + RoundFloatToInt( 5.0f * scale ),
			y + RoundFloatToInt( 4.0f * scale ) );
		vgui::surface()->DrawPrintText( title, V_wcslen( title ) );

		for ( int item = 0;
			item < FOF_TEAM_CLASS_ITEM_COUNT;
			++item )
		{
			if ( m_TeamClassItemMaterials[classIndex][item].IsEmpty() )
				continue;

			const int itemX = x + RoundFloatToInt( 5.0f * scale );
			const int itemY = y + RoundFloatToInt(
				( 21.0f + item * 21.0f ) * scale );
			const int itemWide = MAX(
				RoundFloatToInt( 80.0f * scale ), 1 );
			const int itemTall = MAX(
				RoundFloatToInt( 20.0f * scale ), 1 );
			vgui::surface()->DrawSetColor( FoFTeamClassItemColor(
				m_TeamClassItemStyles[classIndex][item], enabled ) );
			vgui::surface()->DrawFilledRect(
				itemX, itemY, itemX + itemWide, itemY + itemTall );
			vgui::surface()->DrawSetColor(
				enabled ? Color( 205, 205, 205, 225 ) :
					Color( 105, 85, 85, 145 ) );
			vgui::surface()->DrawOutlinedRect(
				itemX, itemY, itemX + itemWide, itemY + itemTall );
			DrawMenuTexture(
				m_iTeamClassItemTextures[classIndex][item],
				itemX,
				itemY,
				itemWide,
				itemTall,
				enabled ? Color( 255, 255, 255, 255 ) :
					Color( 120, 100, 100, 135 ) );
		}
	}

	// The close control is an ordinary VGUI Button in the original.  Let its
	// scheme paint the dark face, border and centred X instead of drawing a
	// white substitute here.
}
