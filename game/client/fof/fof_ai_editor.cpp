#include "cbase.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "hud.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "inputsystem/iinputsystem.h"
#include "KeyValues.h"
#include "engine/ivdebugoverlay.h"
#include "fof/fof_ai_editor.h"
#include "fof/fof_course_editor.h"
#include "fof/fof_purchase_menu.h"
#include <vgui/IScheme.h>
#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include "cdll_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFAIPropDefinition
{
	const char *displayName;
	const char *entityName;
	const char *modelName;
	int normalOffset;
	int type;
};

// FoF client table.  The order is part of the editor protocol because
// number keys address the currently visible ten entries.
static const FoFAIPropDefinition s_FoFAIProps[] =
{
	{ "Whiskey", "item_whiskey", "1", 20, 10000 },
	{ "Small Explosive Barrel", "prop_physics_multiplayer", "", 15, 1 },
	{ "Explosive Barrel", "prop_physics_multiplayer", "models/elpaso/barrel1_explosive.mdl", 2, 4 },
	{ "Destructable Crate", "prop_physics_multiplayer", "models/elpaso/barrel2_explosive.mdl", 2, 4 },
	{ "Weapon Crate Blue", "fof_crate_low", "models/props_junk/wood_crate001a.mdl", 22, 6 },
	{ "Weapon Crate Red", "fof_crate_med", "", 12, 4 },
	{ "Weapon Crate Yellow", "fof_crate", "", 12, 4 },
	{ "Weapon Crate Special", "fof_crate_special", "", 16, 5 },
	{ "Knife", "weapon_knife", "", 12, 4 },
	{ "Hatchet", "weapon_axe", "", 5, 1 },
	{ "Machete", "weapon_machete", "", 5, 2 },
	{ "Deringer", "weapon_deringer", "", 6, 2 },
	{ "Hammerless", "weapon_hammerless", "", 6, 1 },
	{ "Volcanic", "weapon_volcanic", "", 6, 1 },
	{ "Navy", "weapon_coltnavy", "", 6, 1 },
	{ "Mare's Leg", "weapon_maresleg", "", 6, 1 },
	{ "Remington Army", "weapon_remington_army", "", 6, 1 },
	{ "Schofield", "weapon_schofield", "", 6, 1 },
	{ "Peacemaker", "weapon_peacemaker", "", 6, 1 },
	{ "Walker", "weapon_walker", "", 6, 1 },
	{ "Sawed off shotgun", "weapon_sawedoff_shotgun", "", 6, 2 },
	{ "Coachgun", "weapon_coachgun", "", 6, 3 },
	{ "Pump Shotgun", "weapon_shotgun", "", 6, 3 },
	{ "Bow", "weapon_bow", "", 19, 3 },
	{ "Bow Black", "weapon_bow_black", "", 19, 3 },
	{ "TNT Bow", "weapon_bow_tnt", "", 19, 3 },
	{ "Carbine", "weapon_carbine", "", 6, 3 },
	{ "Yellow Boy", "weapon_henryrifle", "", 14, 3 },
	{ "Spencer", "weapon_spencer", "", 13, 3 },
	{ "Sharps", "weapon_sharps", "", 6, 3 },
	{ "Portable Whiskey", "weapon_whiskey", "", 14, 1 },
	{ "GhostGun", "weapon_ghostgun", "", 6, 1 },
	{ "Dynamite", "weapon_dynamite", "", 7, 1 },
	{ "Dynamite black", "weapon_dynamite_black", "", 7, 1 },
	{ "Riding Horse", "fof_horse", "", 61, 14 },
	{ "Silouhette 1", "prop_physics_multiplayer", "models/props/native_silhouette1.mdl", 2, 4 },
	{ "Silouhette 2", "prop_physics_multiplayer", "models/props/native_silhouette2.mdl", 2, 4 },
	{ "Bottle 1", "prop_physics_multiplayer", "models/bar/bottle6.mdl", 2, 1 },
	{ "Bottle 2", "prop_physics_multiplayer", "models/bar/bottle5.mdl", 2, 1 },
	{ "Crate solid", "prop_dynamic_override", "models/props/forest/crate_single.mdl", 1, 8 },
	{ "Watermelon", "prop_physics_multiplayer", "models/props_junk/watermelon01.mdl", 8, 2 },
	{ "Watermelon spiky", "prop_physics_multiplayer", "models/props_junk/watermelon01_spiky.mdl", 8, 2 },
};

static ConVar fof_bot_stop(
	"fof_bot_stop", "0", FCVAR_CLIENTDLL,
	"Stops listen-server bots while the FoF AI editor is active.",
	true, 0.0f, true, 1.0f );

static const ButtonCode_t s_FoFEditorNumberKeys[10] =
{
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
	KEY_6, KEY_7, KEY_8, KEY_9, KEY_0
};

static int FoFAIParseEquipment(
	const char *equipment, int *itemIds, int maxItems )
{
	if ( !equipment || !itemIds || maxItems <= 0 )
		return 0;

	int count = 0;
	const char *cursor = equipment;
	while ( cursor[0] && count < maxItems )
	{
		const int itemId = Q_atoi( cursor );
		if ( itemId >= 0 )
			itemIds[count++] = itemId;
		const char *comma = Q_strstr( cursor, "," );
		if ( !comma )
			break;
		cursor = comma + 1;
	}
	return count;
}

static bool FoFAIItemUsesUtilityBackground( int itemId )
{
	switch ( itemId )
	{
	case 1:
	case 7:
	case 14:
	case 15:
	case 19:
	case 20:
	case 23:
	case 26:
	case 27:
	case 34:
	case 41:
	case 42:
	case 43:
	case 44:
		return true;
	default:
		return false;
	}
}

static void FoFAINormalizeFilename(
	const char *input, const char *extension,
	char *output, int outputSize )
{
	if ( !output || outputSize <= 0 )
		return;
	output[0] = '\0';
	const char *filename = input ? V_UnqualifiedFileName( input ) : NULL;
	Q_strncpy( output,
		filename && filename[0] ? filename : "untitled", outputSize );
	for ( char *character = output; *character; ++character )
	{
		if ( *character == '\\' || *character == '/' || *character == ':' ||
			*character == '*' || *character == '?' || *character == '"' ||
			*character == '<' || *character == '>' || *character == '|' ||
			(unsigned char)*character < 32 )
		{
			*character = '_';
		}
	}
	const char *currentExtension = V_GetFileExtension( output );
	if ( !currentExtension || Q_stricmp( currentExtension, extension ) )
	{
		Q_strncat( output, ".", outputSize );
		Q_strncat( output, extension, outputSize );
	}
}

static void FoFAIReadBotPreset(
	KeyValues *item, FoFAIBotPreset &bot )
{
	Q_memset( &bot, 0, sizeof( bot ) );
	Q_strncpy( bot.name,
		item->GetString( "bot_name", "BOT" ), sizeof( bot.name ) );
	Q_strncpy( bot.equipment,
		item->GetString( "bot_equipment", "" ), sizeof( bot.equipment ) );
	bot.rotationSpeed = item->GetInt( "bot_rotation_speed", 5 );
	bot.shootDelay = item->GetInt( "bot_shoot_delay", 5 );
	bot.aimTrailing = item->GetInt( "bot_aim_trailing", 5 );
	bot.strafe = item->GetInt( "bot_strafe", 5 );
	bot.forceTeam = item->GetInt( "bot_force_team", 0 );
	bot.aggression = item->GetInt( "bot_aggression", 5 );
}

static void FoFAIWriteBotPreset(
	KeyValues *item, const FoFAIBotPreset &bot )
{
	item->SetInt( "bot_rotation_speed", bot.rotationSpeed );
	item->SetInt( "bot_shoot_delay", bot.shootDelay );
	item->SetInt( "bot_aim_trailing", bot.aimTrailing );
	item->SetInt( "bot_strafe", bot.strafe );
	item->SetInt( "bot_force_team", bot.forceTeam );
	item->SetInt( "bot_aggression", bot.aggression );
	item->SetString( "bot_name", bot.name );
	item->SetString( "bot_equipment", bot.equipment );
}

DECLARE_HUDELEMENT( CHudFoFAIEditor );

CHudFoFAIEditor::CHudFoFAIEditor( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudMaterializeMenu" )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hRowFont( vgui::INVALID_FONT )
	, m_hSmallFont( vgui::INVALID_FONT )
	, m_bActive( false )
	, m_bPropMode( false )
	, m_bShowHelp( true )
	, m_iSelected( 0 )
	, m_iScroll( 0 )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
	, m_bMouseLeftDown( false )
	, m_bMouseRightDown( false )
	, m_bWheelUpDown( false )
	, m_bWheelDownDown( false )
	, m_bAltDown( false )
	, m_bDeleteDown( false )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetVisible( false );
	Q_memset( m_bNumberDown, 0, sizeof( m_bNumberDown ) );
	Q_memset( m_iItemTextures, 0xff, sizeof( m_iItemTextures ) );
	m_szMapName[0] = '\0';
	m_szBotPresetFilename[0] = '\0';
}

void CHudFoFAIEditor::Init()
{
	LoadBotPresets();
	Reset();
}

void CHudFoFAIEditor::Reset()
{
	if ( m_bActive )
		SetEditorActive( false );
	m_iSelected = 0;
	m_iScroll = 0;
	m_bPropMode = false;
	m_bShowHelp = true;
	m_PlacedEntries.RemoveAll();
}

void CHudFoFAIEditor::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	UpdateLayout();
}

bool CHudFoFAIEditor::ShouldDraw()
{
	return m_bActive && CHudElement::ShouldDraw();
}

void CHudFoFAIEditor::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hTitleFont = scheme->GetFont( "DefaultFoF", true );
	m_hRowFont = scheme->GetFont( "Default", true );
	m_hSmallFont = scheme->GetFont( "DefaultFoF", true );
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = m_hSmallFont;
	if ( m_hRowFont == vgui::INVALID_FONT )
		m_hRowFont = m_hSmallFont;
	UpdateLayout();
}

void CHudFoFAIEditor::PerformLayout()
{
	BaseClass::PerformLayout();
	UpdateLayout();
}

void CHudFoFAIEditor::OnThink()
{
	BaseClass::OnThink();
	if ( !m_bActive )
		return;

	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		UpdateLayout();
	}
	if ( FoFAIProfileEditorIsActive() )
		return;
	HandleInput();
}

void CHudFoFAIEditor::Paint()
{
	if ( !m_bActive )
		return;

	DrawPlacementPreview();

	const int wide = GetWide();
	const int inset = FoFEditorScale( 2.0f );
	const int cardRadius = MAX( 2, FoFEditorScale( 3.0f ) );
	int rowY = 0;

	if ( m_bShowHelp )
	{
		const int helpInset = FoFEditorScale( 3.0f );
		const int helpTextY = helpInset;
		const int helpBottom = FoFEditorDrawLocalizedWrappedText(
			m_hSmallFont, Color( 0, 0, 0, 0 ),
			helpInset, helpTextY, wide - helpInset * 2,
			"#FoF_AI_Editor_Help",
			"NUMBER KEYS: select / LMB: spawn / RMB: toggle edit / MOUSE WHEEL: scroll / DEL remove all placed items / ALT item placement mode" );
		const int helpTall = helpBottom + helpInset;
		FoFEditorDrawRoundedRect(
			0, 0, wide, helpTall, cardRadius, Color( 70, 70, 65, 190 ) );
		FoFEditorDrawLocalizedWrappedText(
			m_hSmallFont, Color( 255, 20, 235, 255 ),
			helpInset, helpTextY, wide - helpInset * 2,
			"#FoF_AI_Editor_Help",
			"NUMBER KEYS: select / LMB: spawn / RMB: toggle edit / MOUSE WHEEL: scroll / DEL remove all placed items / ALT item placement mode" );
		rowY = helpTall + FoFEditorScale( 2.0f );
	}

	const int rowStep = FoFEditorScale( m_bPropMode ? 17.0f : 32.0f );
	const int cardTall = FoFEditorScale( m_bPropMode ? 15.0f : 30.0f );
	for ( int row = 0; row < 10; ++row )
	{
		const int entry = m_iScroll + row;
		if ( entry >= GetEntryCount() )
			break;

		const int y = rowY + row * rowStep;
		FoFEditorDrawRoundedRect(
			0, y, wide, cardTall, cardRadius,
			entry == m_iSelected ?
				Color( 150, 150, 150, 255 ) : Color( 0, 0, 0, 180 ) );

		if ( m_bPropMode )
		{
			char label[128];
			Q_snprintf( label, sizeof( label ), "%d. %s",
				row + 1, s_FoFAIProps[entry].displayName );
			FoFEditorDrawText(
				m_hRowFont, Color( 245, 210, 80, 255 ), inset,
				y + FoFEditorScale( 1.0f ), label );
		}
		else
		{
			const FoFAIBotPreset &bot = m_Bots[entry];
			const Color botColor = GetBotColor( bot.forceTeam );
			const float average = ( bot.rotationSpeed + bot.shootDelay +
				bot.aimTrailing + bot.strafe ) * 0.25f;
			char label[160];
			Q_snprintf( label, sizeof( label ), "%d. %s (Avg %.1f Agg %d)",
				row + 1, bot.name, average, bot.aggression );
			FoFEditorDrawText(
				m_hRowFont, botColor, inset,
				y + FoFEditorScale( 1.0f ), label );
			DrawBotEquipment(
				bot, inset, y + FoFEditorScale( 14.0f ), wide - inset * 2 );
		}
	}
}

bool CHudFoFAIEditor::IsActive() const
{
	return m_bActive;
}

void CHudFoFAIEditor::SetEditorActive( bool active )
{
	if ( active == m_bActive )
		return;

	m_bActive = active;
	SetVisible( active );
	if ( active )
	{
		// The original editor re-reads the active server profile when F4 is
		// opened.  Course maps commonly replace the default with
		// course_bots*.txt after the client HUD has initialized.
		LoadBotPresets();
		FoFEditorGetMapName( m_szMapName, sizeof( m_szMapName ) );
		m_iSelected = 0;
		m_iScroll = 0;
		fof_bot_stop.SetValue( 1 );
		FoFEditorClientCommand( "ai_editor_start" );
		SendSelection();
		UpdateLayout();
	}
	else
	{
		FoFAIProfileEditorClose();
		fof_bot_stop.SetValue( 0 );
		FoFEditorClientCommand( "ai_editor_end" );
	}
}

void CHudFoFAIEditor::ToggleEditor()
{
	SetEditorActive( !m_bActive );
}

void CHudFoFAIEditor::LoadBotPresets()
{
	ConVarRef botScript( "fof_bot_scriptname", true );
	const char *filename = botScript.IsValid() ? botScript.GetString() : "";
	if ( !filename || !filename[0] || !LoadBotPresetFile( filename ) )
	{
		if ( !LoadBotPresetFile( "1_default_bots.txt" ) )
		{
			m_Bots.RemoveAll();
			m_szBotPresetFilename[0] = '\0';
			AddFallbackBotPresets();
		}
	}
}

void CHudFoFAIEditor::AddFallbackBotPresets()
{
	static const char *names[] =
	{
		"BOT Tuco", "BOT Django", "BOT Monco", "BOT Sentenza",
		"BOT Sartana", "BOT Nobody", "BOT Sabata", "BOT Harmonica",
		"BOT Silence", "BOT Trinity"
	};
	static const char *equipment[] =
	{
		"2,1,", "28,20,", "28,28,", "6,", "21,",
		"4,", "5,1,", "13,13,", "31,31,", "29,1,"
	};
	static const int teams[] = { 2, 3, 2, 4, 3, 2, 5, 4, 3, 4 };
	for ( int index = 0; index < ARRAYSIZE( names ); ++index )
	{
		FoFAIBotPreset bot;
		Q_memset( &bot, 0, sizeof( bot ) );
		Q_strncpy( bot.name, names[index], sizeof( bot.name ) );
		Q_strncpy( bot.equipment, equipment[index], sizeof( bot.equipment ) );
		bot.rotationSpeed = 5;
		bot.shootDelay = 5;
		bot.aimTrailing = 5;
		bot.strafe = 5;
		bot.forceTeam = teams[index];
		bot.aggression = 5;
		m_Bots.AddToTail( bot );
	}
}

int CHudFoFAIEditor::GetBotPresetCount() const
{
	return m_Bots.Count();
}

const FoFAIBotPreset *CHudFoFAIEditor::GetBotPreset( int index ) const
{
	return index >= 0 && index < m_Bots.Count() ? &m_Bots[index] : NULL;
}

void CHudFoFAIEditor::SetBotPreset(
	int index, const FoFAIBotPreset &preset )
{
	if ( index >= 0 && index < m_Bots.Count() )
		m_Bots[index] = preset;
}

void CHudFoFAIEditor::AddBotPreset( const FoFAIBotPreset &preset )
{
	m_Bots.AddToTail( preset );
	NotifyBotPresetsChanged();
}

void CHudFoFAIEditor::RemoveBotPreset( int index )
{
	if ( index < 0 || index >= m_Bots.Count() )
		return;
	m_Bots.Remove( index );
	NotifyBotPresetsChanged();
}

bool CHudFoFAIEditor::LoadBotPresetFile( const char *filename )
{
	if ( !filename || !filename[0] || Q_strstr( filename, ".." ) )
		return false;
	char cleanName[MAX_PATH];
	FoFAINormalizeFilename(
		filename, "txt", cleanName, sizeof( cleanName ) );
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ),
		"fof_scripts/bots/%s", cleanName );

	KeyValues *root = new KeyValues( "BotList" );
	if ( !root->LoadFromFile( filesystem, path, "MOD" ) &&
		!root->LoadFromFile( filesystem, path, "GAME" ) )
	{
		root->deleteThis();
		Warning( "[FoF] unable to load bot definitions %s\n", path );
		return false;
	}

	CUtlVector< FoFAIBotPreset > presets;
	for ( KeyValues *item = root->GetFirstTrueSubKey();
		item; item = item->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( item->GetName(), "preset" ) )
			continue;
		FoFAIBotPreset preset;
		FoFAIReadBotPreset( item, preset );
		presets.AddToTail( preset );
	}
	root->deleteThis();
	if ( presets.Count() <= 0 )
		return false;

	m_Bots.RemoveAll();
	for ( int index = 0; index < presets.Count(); ++index )
		m_Bots.AddToTail( presets[index] );
	Q_strncpy( m_szBotPresetFilename, cleanName,
		sizeof( m_szBotPresetFilename ) );
	NotifyBotPresetsChanged();
	Msg( "=== %i bot definitions loaded from %s ===\n",
		m_Bots.Count(), cleanName );
	return true;
}

const char *CHudFoFAIEditor::GetBotPresetFilename() const
{
	return m_szBotPresetFilename;
}

bool CHudFoFAIEditor::SaveBotPresetFile( const char *filename ) const
{
	if ( !filename || !filename[0] || Q_strstr( filename, ".." ) )
		return false;
	char cleanName[MAX_PATH];
	FoFAINormalizeFilename(
		filename, "txt", cleanName, sizeof( cleanName ) );
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ),
		"fof_scripts/bots/%s", cleanName );

	KeyValues *root = new KeyValues( "BotList" );
	for ( int index = 0; index < m_Bots.Count(); ++index )
	{
		KeyValues *item = new KeyValues( "preset" );
		FoFAIWriteBotPreset( item, m_Bots[index] );
		root->AddSubKey( item );
	}
	filesystem->CreateDirHierarchy( "fof_scripts/bots", "MOD" );
	const bool saved = root->SaveToFile( filesystem, path, "MOD" );
	root->deleteThis();
	if ( saved )
		Msg( "Saved %s successfully\n", cleanName );
	else
		Warning( "[FoF] unable to save bot definitions %s\n", path );
	return saved;
}

bool CHudFoFAIEditor::IsPropMode() const
{
	return m_bPropMode;
}

void CHudFoFAIEditor::NotifyBotPresetsChanged()
{
	if ( m_Bots.Count() <= 0 )
	{
		m_iSelected = 0;
		m_iScroll = 0;
		return;
	}
	if ( !m_bPropMode )
	{
		m_iSelected = clamp( m_iSelected, 0, m_Bots.Count() - 1 );
		m_iScroll = clamp( m_iScroll, 0, GetMaxScroll() );
		if ( m_bActive )
			SendSelection();
	}
}

void CHudFoFAIEditor::UpdateLayout()
{
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	const int wide = FoFEditorScale( 250.0f );
	SetBounds(
		MAX( 0, ScreenWidth() - wide - FoFEditorScale( 10.0f ) ),
		FoFEditorScale( 16.0f ),
		wide,
		MAX( FoFEditorScale( 350.0f ), ScreenHeight() - FoFEditorScale( 24.0f ) ) );
}

void CHudFoFAIEditor::DrawPlacementPreview()
{
	if ( !debugoverlay || m_iSelected < 0 ||
		m_iSelected >= GetEntryCount() )
	{
		return;
	}

	const float offset = m_bPropMode ?
		(float)s_FoFAIProps[m_iSelected].normalOffset : 0.0f;
	Vector origin;
	QAngle angles;
	if ( !FoFEditorTracePlacement( offset, origin, angles ) )
		return;

	Vector mins;
	Vector maxs;
	Color faceColor;
	if ( m_bPropMode )
	{
		mins.Init( -8.0f, -8.0f, 0.0f );
		maxs.Init( 8.0f, 8.0f, 16.0f );
		faceColor = Color( 200, 200, 200, 10 );
	}
	else
	{
		mins.Init( -16.0f, -16.0f, 0.0f );
		maxs.Init( 16.0f, 16.0f, 72.0f );
		const Color teamColor = GetBotColor( m_Bots[m_iSelected].forceTeam );
		faceColor = Color(
			teamColor.r(), teamColor.g(), teamColor.b(), 10 );
	}

	debugoverlay->AddBoxOverlay2(
		origin, mins, maxs, angles, faceColor,
		Color( 10, 10, 10, 255 ), 0.01f );
}

int CHudFoFAIEditor::EnsureItemTexture( int itemId )
{
	if ( itemId < 0 || itemId >= ARRAYSIZE( m_iItemTextures ) )
		return -1;
	if ( m_iItemTextures[itemId] >= 0 )
		return m_iItemTextures[itemId];

	const char *material = FoFPurchaseItemMaterial( itemId );
	if ( !material || !material[0] )
		return -1;
	m_iItemTextures[itemId] = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iItemTextures[itemId], material, true, false );
	return m_iItemTextures[itemId];
}

void CHudFoFAIEditor::DrawBotEquipment(
	const FoFAIBotPreset &bot, int x, int y, int maxWide )
{
	int itemIds[4];
	const int itemCount = FoFAIParseEquipment(
		bot.equipment, itemIds, ARRAYSIZE( itemIds ) );
	const int gap = FoFEditorScale( 2.0f );
	const int slotWide = FoFEditorScale( 62.0f );
	const int slotTall = FoFEditorScale( 15.0f );
	for ( int index = 0; index < itemCount; ++index )
	{
		const int slotX = x + index * ( slotWide + gap );
		if ( slotX + slotWide > x + maxWide )
			break;

		const Color background = FoFAIItemUsesUtilityBackground( itemIds[index] ) ?
			Color( 70, 48, 55, 230 ) : Color( 37, 55, 72, 230 );
		vgui::surface()->DrawSetColor( background );
		vgui::surface()->DrawFilledRect(
			slotX, y, slotX + slotWide, y + slotTall );

		const int texture = EnsureItemTexture( itemIds[index] );
		if ( texture >= 0 )
		{
			vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
			vgui::surface()->DrawSetTexture( texture );
			vgui::surface()->DrawTexturedRect(
				slotX + 1, y + 1, slotX + slotWide - 1, y + slotTall - 1 );
		}
		vgui::surface()->DrawSetColor( Color( 210, 210, 210, 255 ) );
		vgui::surface()->DrawOutlinedRect(
			slotX, y, slotX + slotWide, y + slotTall );
	}
}

void CHudFoFAIEditor::HandleInput()
{
	for ( int row = 0; row < 10; ++row )
	{
		if ( FoFEditorButtonPressed(
			s_FoFEditorNumberKeys[row], m_bNumberDown[row] ) )
		{
			SelectVisibleRow( row );
		}
	}

	if ( FoFEditorButtonPressed( MOUSE_WHEEL_UP, m_bWheelUpDown ) )
	{
		m_iScroll = MAX( 0, m_iScroll - 1 );
		m_iSelected = clamp( m_iSelected, m_iScroll, m_iScroll + 9 );
	}
	if ( FoFEditorButtonPressed( MOUSE_WHEEL_DOWN, m_bWheelDownDown ) )
	{
		m_iScroll = MIN( GetMaxScroll(), m_iScroll + 1 );
		m_iSelected = clamp( m_iSelected, m_iScroll, m_iScroll + 9 );
	}

	const bool altDown = inputsystem &&
		( inputsystem->IsButtonDown( KEY_LALT ) ||
		  inputsystem->IsButtonDown( KEY_RALT ) ) ||
		vgui::input()->IsKeyDown( KEY_LALT ) ||
		vgui::input()->IsKeyDown( KEY_RALT );
	if ( altDown && !m_bAltDown )
	{
		m_bPropMode = !m_bPropMode;
		m_iSelected = 0;
		m_iScroll = 0;
		SendSelection();
	}
	m_bAltDown = altDown;

	if ( FoFEditorButtonPressed( KEY_DELETE, m_bDeleteDown ) )
	{
		FoFEditorClientCommand( "remove_all_bots" );
		m_PlacedEntries.RemoveAll();
	}
	if ( FoFEditorButtonPressed( MOUSE_LEFT, m_bMouseLeftDown ) )
		PlaceSelection();
	if ( FoFEditorButtonPressed( MOUSE_RIGHT, m_bMouseRightDown ) )
		FoFAIProfileEditorOpen();
}

void CHudFoFAIEditor::SelectVisibleRow( int visibleRow )
{
	const int entry = m_iScroll + visibleRow;
	if ( entry < 0 || entry >= GetEntryCount() )
		return;
	m_iSelected = entry;
	SendSelection();
}

void CHudFoFAIEditor::SendSelection()
{
	if ( m_iSelected < 0 || m_iSelected >= GetEntryCount() )
		return;

	if ( m_bPropMode )
	{
		const FoFAIPropDefinition &prop = s_FoFAIProps[m_iSelected];
		FoFEditorClientCommand(
			"select_bot \"%s\" \"%s\" %d %d 0 0 0 0",
			prop.entityName, prop.modelName, prop.normalOffset, prop.type );
	}
	else
	{
		const FoFAIBotPreset &bot = m_Bots[m_iSelected];
		FoFEditorClientCommand(
			"select_bot \"%s\" \"%s\" %d %d %d %d %d %d",
			bot.name, bot.equipment, bot.rotationSpeed, bot.shootDelay,
			bot.aimTrailing, bot.strafe, bot.forceTeam, bot.aggression );
	}
}

void CHudFoFAIEditor::PlaceSelection()
{
	if ( m_iSelected < 0 || m_iSelected >= GetEntryCount() )
		return;

	const float offset = m_bPropMode ?
		(float)s_FoFAIProps[m_iSelected].normalOffset : 2.0f;
	Vector origin;
	QAngle angles;
	if ( !FoFEditorTracePlacement( offset, origin, angles ) )
		return;

	FoFAIPlacedEntry entry;
	Q_memset( &entry, 0, sizeof( entry ) );
	entry.prop = m_bPropMode;
	entry.origin = origin;
	entry.direction = angles;
	if ( m_bPropMode )
	{
		const FoFAIPropDefinition &prop = s_FoFAIProps[m_iSelected];
		Q_strncpy( entry.entity, prop.entityName, sizeof( entry.entity ) );
		Q_strncpy( entry.data,
			prop.modelName && !Q_strnicmp( prop.modelName, "models/", 7 ) ?
				prop.modelName : "null",
			sizeof( entry.data ) );
		entry.normalOffset = prop.normalOffset;
		entry.type = prop.type;
	}
	else
	{
		entry.bot = m_Bots[m_iSelected];
	}
	m_PlacedEntries.AddToTail( entry );

	FoFEditorClientCommand(
		m_bPropMode ?
			"create_prop %i %i %i %.3f %.3f %.3f" :
			"create_bot %i %i %i %.3f %.3f %.3f",
		RoundFloatToInt( origin.x ), RoundFloatToInt( origin.y ),
		RoundFloatToInt( origin.z ), angles.x, angles.y, angles.z );
}

void CHudFoFAIEditor::RemoveAtCrosshair()
{
	Vector origin;
	QAngle angles;
	if ( !FoFEditorTracePlacement( 0.0f, origin, angles ) )
		return;
	FoFEditorClientCommand(
		"remove_bot %i %i %i",
		RoundFloatToInt( origin.x ), RoundFloatToInt( origin.y ),
		RoundFloatToInt( origin.z ) );
}

int CHudFoFAIEditor::GetEntryCount() const
{
	return m_bPropMode ? ARRAYSIZE( s_FoFAIProps ) : m_Bots.Count();
}

int CHudFoFAIEditor::GetMaxScroll() const
{
	return MAX( 0, GetEntryCount() - 10 );
}

Color CHudFoFAIEditor::GetBotColor( int team ) const
{
	switch ( team )
	{
	case 2: return Color( 55, 125, 245, 255 );
	case 3: return Color( 235, 55, 45, 255 );
	case 4: return Color( 235, 190, 45, 255 );
	case 5: return Color( 45, 210, 65, 255 );
	default: return Color( 235, 235, 235, 255 );
	}
}

bool CHudFoFAIEditor::SaveSpawnScript( const char *filename ) const
{
	if ( !filename || !filename[0] || Q_strstr( filename, ".." ) )
		return false;
	char cleanName[MAX_PATH];
	FoFAINormalizeFilename(
		filename, "ai", cleanName, sizeof( cleanName ) );
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ),
		"fof_scripts/ai_editor/%s", cleanName );

	KeyValues *root = new KeyValues( m_bPropMode ? "PropList" : "BotList" );
	int savedEntries = 0;
	for ( int index = 0; index < m_PlacedEntries.Count(); ++index )
	{
		const FoFAIPlacedEntry &entry = m_PlacedEntries[index];
		if ( entry.prop != m_bPropMode )
			continue;
		KeyValues *item = new KeyValues( "preset" );
		if ( entry.prop )
		{
			item->SetString( "entity", entry.entity );
			item->SetString( "data", entry.data[0] ? entry.data : "null" );
		}
		else
		{
			FoFAIWriteBotPreset( item, entry.bot );
		}
		item->SetFloat( "origin_x", entry.origin.x );
		item->SetFloat( "origin_y", entry.origin.y );
		item->SetFloat( "origin_z", entry.origin.z );
		item->SetFloat( "dir_x", entry.direction.x );
		item->SetFloat( "dir_y", entry.direction.y );
		item->SetFloat( "dir_z", entry.direction.z );
		root->AddSubKey( item );
		++savedEntries;
	}

	filesystem->CreateDirHierarchy( "fof_scripts/ai_editor", "MOD" );
	const bool saved = root->SaveToFile( filesystem, path, "MOD" );
	root->deleteThis();
	if ( saved )
		Msg( "Saved %s successfully (%i entries)\n", cleanName, savedEntries );
	else
		Warning( "[FoF] unable to save AI editor script %s\n", path );
	return saved;
}

bool CHudFoFAIEditor::RunSpawnScript( const char *filename )
{
	if ( !filename || !filename[0] || Q_strstr( filename, ".." ) )
		return false;

	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ),
		"fof_scripts/ai_editor/%s", filename );
	if ( !V_GetFileExtension( path ) )
		Q_strncat( path, ".ai", sizeof( path ) );

	KeyValues *root = new KeyValues( "BotList" );
	if ( !root->LoadFromFile( filesystem, path, "MOD" ) &&
		!root->LoadFromFile( filesystem, path, "GAME" ) &&
		!root->LoadFromFile( filesystem, filename, "MOD" ) &&
		!root->LoadFromFile( filesystem, filename, "GAME" ) )
	{
		root->deleteThis();
		Warning( "[FoF] unable to load AI editor script %s\n", filename );
		return false;
	}

	const bool propList = !Q_stricmp( root->GetName(), "PropList" );
	int created = 0;
	for ( KeyValues *item = root->GetFirstTrueSubKey();
		item; item = item->GetNextTrueSubKey() )
	{
		FoFAIPlacedEntry entry;
		Q_memset( &entry, 0, sizeof( entry ) );
		entry.prop = propList;
		entry.origin.Init(
			item->GetFloat( "origin_x", 0.0f ),
			item->GetFloat( "origin_y", 0.0f ),
			item->GetFloat( "origin_z", 0.0f ) );
		entry.direction.Init(
			item->GetFloat( "dir_x", 0.0f ),
			item->GetFloat( "dir_y", 0.0f ),
			item->GetFloat( "dir_z", 0.0f ) );
		const int x = RoundFloatToInt( entry.origin.x );
		const int y = RoundFloatToInt( entry.origin.y );
		const int z = RoundFloatToInt( entry.origin.z );

		if ( propList )
		{
			Q_strncpy( entry.entity,
				item->GetString( "entity", item->GetString( "item_name", "" ) ),
				sizeof( entry.entity ) );
			Q_strncpy( entry.data,
				item->GetString( "data", item->GetString( "model", "null" ) ),
				sizeof( entry.data ) );
			entry.normalOffset = item->GetInt( "z_offset", 0 );
			entry.type = item->GetInt( "type", 0 );
			FoFEditorClientCommand(
				"select_bot \"%s\" \"%s\" %d %d 0 0 0 0",
				entry.entity, entry.data,
				entry.normalOffset, entry.type );
			FoFEditorClientCommand(
				"create_prop %d %d %d %.3f %.3f %.3f",
				x, y, z, entry.direction.x,
				entry.direction.y, entry.direction.z );
		}
		else
		{
			FoFAIReadBotPreset( item, entry.bot );
			FoFEditorClientCommand(
				"select_bot \"%s\" \"%s\" %d %d %d %d %d %d",
				entry.bot.name, entry.bot.equipment,
				entry.bot.rotationSpeed, entry.bot.shootDelay,
				entry.bot.aimTrailing, entry.bot.strafe,
				entry.bot.forceTeam, entry.bot.aggression );
			FoFEditorClientCommand(
				"create_bot %d %d %d %.3f %.3f %.3f",
				x, y, z, entry.direction.x,
				entry.direction.y, entry.direction.z );
		}
		m_PlacedEntries.AddToTail( entry );
		++created;
	}
	root->deleteThis();
	Msg( "=== %i AI editor entries loaded ===\n", created );
	return true;
}

bool FoFAIEditorIsActive()
{
	CHudFoFAIEditor *editor = GET_HUDELEMENT( CHudFoFAIEditor );
	return editor && editor->IsActive();
}

void FoFAIEditorClose()
{
	CHudFoFAIEditor *editor = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( editor )
		editor->SetEditorActive( false );
}

CON_COMMAND( fof_ai_editor, "Toggle the FoF listen-server AI editor." )
{
	if ( !FoFEditorCheatsEnabled() || FoFCourseEditorIsActive() )
		return;

	CHudFoFAIEditor *editor = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( editor )
		editor->ToggleEditor();
}

CON_COMMAND( fof_run_spawn_script, "Load and instantiate an FoF AI editor script." )
{
	if ( args.ArgC() < 2 )
		return;

	CHudFoFAIEditor *editor = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( editor )
		editor->RunSpawnScript( args[1] );
}

static void FoFAIProfileAddComboInt(
	vgui::ComboBox *combo, const char *text, int value )
{
	if ( !combo )
		return;
	KeyValues *data = new KeyValues( "data" );
	data->SetInt( "value", value );
	combo->AddItem( text, data );
	data->deleteThis();
}

static int FoFAIProfileGetComboInt(
	vgui::ComboBox *combo, int fallback )
{
	KeyValues *data = combo ? combo->GetActiveItemUserData() : NULL;
	return data ? data->GetInt( "value", fallback ) : fallback;
}

static void FoFAIProfileActivateComboInt(
	vgui::ComboBox *combo, int value )
{
	if ( !combo )
		return;
	for ( int row = 0; row < combo->GetItemCount(); ++row )
	{
		const int itemID = combo->GetItemIDFromRow( row );
		KeyValues *data = combo->GetItemUserData( itemID );
		if ( data && data->GetInt( "value", -9999 ) == value )
		{
			combo->SilentActivateItem( itemID );
			return;
		}
	}
	if ( combo->GetItemCount() > 0 )
		combo->SilentActivateItemByRow( 0 );
}

static void FoFAIProfileNormalizeFilename(
	const char *input, const char *extension,
	char *output, int outputSize )
{
	if ( !output || outputSize <= 0 )
		return;
	output[0] = '\0';
	const char *name = input ? V_UnqualifiedFileName( input ) : NULL;
	Q_strncpy( output, name && name[0] ? name : "untitled", outputSize );
	for ( char *character = output; *character; ++character )
	{
		if ( *character == '\\' || *character == '/' || *character == ':' ||
			*character == '*' || *character == '?' || *character == '"' ||
			*character == '<' || *character == '>' || *character == '|' ||
			(unsigned char)*character < 32 )
		{
			*character = '_';
		}
	}
	const char *currentExtension = V_GetFileExtension( output );
	if ( !currentExtension || Q_stricmp( currentExtension, extension ) )
	{
		Q_strncat( output, ".", outputSize );
		Q_strncat( output, extension, outputSize );
	}
}

static bool FoFAIProfileComboHasFilename(
	vgui::ComboBox *combo, const char *filename )
{
	if ( !combo || !filename )
		return false;
	for ( int row = 0; row < combo->GetItemCount(); ++row )
	{
		const int itemID = combo->GetItemIDFromRow( row );
		KeyValues *data = combo->GetItemUserData( itemID );
		if ( data && !Q_stricmp(
			data->GetString( "filename", "" ), filename ) )
		{
			return true;
		}
	}
	return false;
}

static void FoFAIProfileAddFiles(
	vgui::ComboBox *combo, const char *pattern, const char *pathID )
{
	if ( !combo || !pattern || !pathID )
		return;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *found = filesystem->FindFirstEx(
		pattern, pathID, &findHandle );
	while ( found )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
		{
			const char *filename = V_UnqualifiedFileName( found );
			if ( !FoFAIProfileComboHasFilename( combo, filename ) )
			{
				char displayName[MAX_PATH];
				Q_FileBase( filename, displayName, sizeof( displayName ) );
				KeyValues *data = new KeyValues( "data" );
				data->SetString( "filename", filename );
				combo->AddItem( displayName, data );
				data->deleteThis();
			}
		}
		found = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );
}

static void FoFAIProfileParseEquipment(
	const char *equipment, int itemIDs[4] )
{
	for ( int slot = 0; slot < 4; ++slot )
		itemIDs[slot] = -1;
	if ( !equipment )
		return;

	const char *cursor = equipment;
	for ( int slot = 0; slot < 4 && cursor[0]; ++slot )
	{
		itemIDs[slot] = Q_atoi( cursor );
		const char *comma = Q_strstr( cursor, "," );
		if ( !comma )
			break;
		cursor = comma + 1;
	}
}

static const int s_FoFAIProfileWeaponIDs[] =
{
	1, 2, 3, 4, 5, 6, 11, 12, 13, 16, 17, 19,
	20, 21, 23, 26, 28, 29, 30, 31, 32, 33, 34
};

static const char *FoFAIProfileWeaponName( int itemID )
{
	if ( itemID == 16 )
		return "Gentleman";
	if ( itemID == 17 )
		return "Dynamite Belt";
	const char *name = FoFPurchaseItemFallbackName( itemID );
	return name && name[0] ? name : "Unknown";
}

static const char *FoFAIProfileWeaponToken( int itemID )
{
	if ( itemID == 16 )
		return "#Item16b";
	if ( itemID == 17 )
		return "#Item17";
	return FoFPurchaseItemNameToken( itemID );
}

static void FoFAIProfileAddWeaponComboInt(
	vgui::ComboBox *combo, int itemID )
{
	if ( !combo )
		return;
	KeyValues *data = new KeyValues( "data" );
	data->SetInt( "value", itemID );
	const char *token = FoFAIProfileWeaponToken( itemID );
	const wchar_t *localized = token && g_pVGuiLocalize ?
		g_pVGuiLocalize->Find( token ) : NULL;
	if ( localized && localized[0] )
		combo->AddItem( localized, data );
	else
		combo->AddItem( FoFAIProfileWeaponName( itemID ), data );
	data->deleteThis();
}

static void FoFAIProfilePopulateWeaponCombo( vgui::ComboBox *combo )
{
	if ( !combo )
		return;
	combo->RemoveAll();
	FoFAIProfileAddComboInt( combo, "auto", -1 );
	for ( int item = 0; item < ARRAYSIZE( s_FoFAIProfileWeaponIDs ); ++item )
		FoFAIProfileAddWeaponComboInt(
			combo, s_FoFAIProfileWeaponIDs[item] );
}

static Color FoFAIProfileTeamColor( int team )
{
	switch ( team )
	{
	case 2: return Color( 45, 105, 255, 255 );
	case 3: return Color( 255, 55, 55, 255 );
	case 4: return Color( 245, 205, 20, 255 );
	case 5: return Color( 35, 210, 75, 255 );
	default: return Color( 235, 235, 235, 255 );
	}
}

static void FoFAIProfileStyleButton( vgui::Button *button )
{
	if ( !button )
		return;
	button->SetDefaultColor(
		Color( 245, 245, 245, 255 ), Color( 73, 62, 0, 245 ) );
	button->SetArmedColor(
		Color( 255, 255, 255, 255 ), Color( 108, 88, 0, 255 ) );
	button->SetDepressedColor(
		Color( 255, 255, 255, 255 ), Color( 125, 35, 15, 255 ) );
}

static void FoFAIProfileStyleField( vgui::Panel *field )
{
	if ( !field )
		return;
	field->SetFgColor( Color( 235, 235, 235, 255 ) );
	field->SetBgColor( Color( 39, 10, 0, 245 ) );
}

DECLARE_HUDELEMENT( CNPCProfileManagement );

CNPCProfileManagement::CNPCProfileManagement( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "NPCProfileManagement" )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hTextFont( vgui::INVALID_FONT )
	, m_bVisible( false )
	, m_bRefreshing( false )
	, m_bRightWasDown( false )
	, m_bRightReleasedSinceOpen( false )
	, m_iFirstRow( 0 )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
	, m_pPresets( NULL )
	, m_pPresetSaveName( NULL )
	, m_pPresetSave( NULL )
	, m_pAddEmpty( NULL )
	, m_pWaves( NULL )
	, m_pWaveSaveName( NULL )
	, m_pWaveSave( NULL )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetVisible( false );
	Q_memset( m_bSkillEditing, 0, sizeof( m_bSkillEditing ) );

	m_pPresets = new vgui::ComboBox(
		this, "presetscombo", 10, false );
	m_pPresetSaveName = new vgui::TextEntry( this, "SaveName" );
	m_pPresetSave = new vgui::Button(
		this, "Save As", "Save As", this, "save" );
	m_pAddEmpty = new vgui::Button(
		this, "Add Empty", "Add Empty", this, "add_empty" );
	m_pWaves = new vgui::ComboBox(
		this, "wavecombo", 10, false );
	m_pWaveSaveName = new vgui::TextEntry( this, "WaveSaveName" );
	m_pWaveSave = new vgui::Button(
		this, "Save As Wave", "Save As", this, "save_wave" );
	m_pPresets->AddActionSignalTarget( this );
	m_pWaves->AddActionSignalTarget( this );
	m_pPresetSaveName->SetMaximumCharCount( MAX_PATH - 1 );
	m_pWaveSaveName->SetMaximumCharCount( MAX_PATH - 1 );

	for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
	{
		FoFAIProfileRowControls &controls = m_Rows[row];
		Q_memset( &controls, 0, sizeof( controls ) );
		char name[32];
		Q_snprintf( name, sizeof( name ), "bot_name%i", row );
		controls.name = new vgui::TextEntry( this, name );
		controls.name->SetMaximumCharCount( 63 );

		Q_snprintf( name, sizeof( name ), "team%i", row );
		controls.team = new vgui::ComboBox( this, name, 8, false );
		FoFAIProfileAddComboInt( controls.team, "Auto", -1 );
		FoFAIProfileAddComboInt( controls.team, "Vigilante", 2 );
		FoFAIProfileAddComboInt( controls.team, "Ranger", 5 );
		FoFAIProfileAddComboInt( controls.team, "Bot", 1 );
		FoFAIProfileAddComboInt( controls.team, "Team", 0 );
		FoFAIProfileAddComboInt( controls.team, "Zombie", 6 );
		FoFAIProfileAddComboInt( controls.team, "Desperado", 3 );
		FoFAIProfileAddComboInt( controls.team, "Bandido", 4 );

		char command[16];
		Q_snprintf( name, sizeof( name ), "Skill%i", row );
		Q_snprintf( command, sizeof( command ), "skill%i", row );
		controls.skillToggle = new vgui::Button(
			this, name, "Skill Avg 5.0", this, command );

		for ( int skill = 0; skill < ARRAYSIZE( controls.skills ); ++skill )
		{
			Q_snprintf( name, sizeof( name ), "skill%i_%i", row, skill );
			controls.skills[skill] = new vgui::ComboBox(
				this, name, 11, false );
			for ( int value = 0; value <= 10; ++value )
			{
				char valueText[8];
				Q_snprintf( valueText, sizeof( valueText ), "%i", value );
				FoFAIProfileAddComboInt(
					controls.skills[skill], valueText, value );
			}
		}

		for ( int slot = 0; slot < ARRAYSIZE( controls.weapons ); ++slot )
		{
			Q_snprintf( name, sizeof( name ), "weapon%i_%i", row, slot );
			controls.weapons[slot] = new vgui::ComboBox(
				this, name, 12, false );
			FoFAIProfilePopulateWeaponCombo( controls.weapons[slot] );
		}

		Q_snprintf( name, sizeof( name ), "Remove%i", row );
		Q_snprintf( command, sizeof( command ), "del%i", row );
		controls.remove = new vgui::Button(
			this, name, "X", this, command );
	}

	Q_strncpy( m_szPresetFilename,
		"1_default_bots.txt", sizeof( m_szPresetFilename ) );
	m_pPresetSaveName->SetText( "my_bot_script.txt" );
	m_pWaveSaveName->SetText( "my_spawn_group.ai" );
}

void CNPCProfileManagement::Init()
{
	Reset();
}

void CNPCProfileManagement::Reset()
{
	SetEditorVisible( false );
	m_iFirstRow = 0;
}

void CNPCProfileManagement::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	UpdateLayout();
}

bool CNPCProfileManagement::ShouldDraw()
{
	return m_bVisible && CHudElement::ShouldDraw();
}

void CNPCProfileManagement::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hTitleFont = scheme->GetFont( "DefaultFoF", true );
	m_hTextFont = scheme->GetFont( "Default", true );
	if ( m_hTextFont == vgui::INVALID_FONT )
		m_hTextFont = m_hTitleFont;
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = m_hTextFont;

	m_pPresets->SetFont( m_hTextFont );
	m_pPresetSaveName->SetFont( m_hTextFont );
	m_pPresetSave->SetFont( m_hTextFont );
	m_pAddEmpty->SetFont( m_hTextFont );
	m_pWaves->SetFont( m_hTextFont );
	m_pWaveSaveName->SetFont( m_hTextFont );
	m_pWaveSave->SetFont( m_hTextFont );
	FoFAIProfileStyleField( m_pPresets );
	FoFAIProfileStyleField( m_pPresetSaveName );
	FoFAIProfileStyleField( m_pWaves );
	FoFAIProfileStyleField( m_pWaveSaveName );
	FoFAIProfileStyleButton( m_pPresetSave );
	FoFAIProfileStyleButton( m_pAddEmpty );
	FoFAIProfileStyleButton( m_pWaveSave );
	for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
	{
		m_Rows[row].name->SetFont( m_hTextFont );
		m_Rows[row].team->SetFont( m_hTextFont );
		m_Rows[row].skillToggle->SetFont( m_hTextFont );
		m_Rows[row].remove->SetFont( m_hTextFont );
		FoFAIProfileStyleField( m_Rows[row].name );
		FoFAIProfileStyleField( m_Rows[row].team );
		FoFAIProfileStyleButton( m_Rows[row].skillToggle );
		FoFAIProfileStyleButton( m_Rows[row].remove );
		for ( int skill = 0; skill < ARRAYSIZE( m_Rows[row].skills ); ++skill )
		{
			m_Rows[row].skills[skill]->SetFont( m_hTextFont );
			FoFAIProfileStyleField( m_Rows[row].skills[skill] );
		}
		for ( int slot = 0; slot < ARRAYSIZE( m_Rows[row].weapons ); ++slot )
		{
			m_Rows[row].weapons[slot]->SetFont( m_hTextFont );
			FoFAIProfileStyleField( m_Rows[row].weapons[slot] );
		}
	}
	UpdateLayout();
}

void CNPCProfileManagement::PerformLayout()
{
	BaseClass::PerformLayout();
	UpdateLayout();
}

void CNPCProfileManagement::OnThink()
{
	BaseClass::OnThink();
	if ( !m_bVisible )
		return;
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		UpdateLayout();
	}

	const bool rightDown =
		( inputsystem && inputsystem->IsButtonDown( MOUSE_RIGHT ) ) ||
		vgui::input()->IsMouseDown( MOUSE_RIGHT );
	if ( !rightDown )
		m_bRightReleasedSinceOpen = true;
	if ( rightDown && !m_bRightWasDown && m_bRightReleasedSinceOpen )
	{
		SetEditorVisible( false );
		return;
	}
	m_bRightWasDown = rightDown;
}

void CNPCProfileManagement::Paint()
{
	if ( !m_bVisible )
		return;
	vgui::surface()->DrawSetColor( Color( 58, 17, 0, 238 ) );
	vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
}

void CNPCProfileManagement::OnCommand( const char *command )
{
	CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( !owner || !command )
	{
		BaseClass::OnCommand( command );
		return;
	}

	if ( !Q_stricmp( command, "add_empty" ) )
	{
		CommitRows();
		FoFAIBotPreset preset;
		Q_memset( &preset, 0, sizeof( preset ) );
		Q_strncpy( preset.name, "name...", sizeof( preset.name ) );
		Q_strncpy( preset.equipment, "-1,-1,-1,-1,",
			sizeof( preset.equipment ) );
		preset.rotationSpeed = 5;
		preset.shootDelay = 5;
		preset.aimTrailing = 5;
		preset.strafe = 5;
		preset.forceTeam = -1;
		preset.aggression = 5;
		owner->AddBotPreset( preset );
		m_iFirstRow = MAX( 0, owner->GetBotPresetCount() - 10 );
		Q_memset( m_bSkillEditing, 0, sizeof( m_bSkillEditing ) );
		PopulateRows();
		return;
	}
	if ( !Q_stricmp( command, "save" ) )
	{
		CommitRows();
		char input[MAX_PATH];
		char filename[MAX_PATH];
		m_pPresetSaveName->GetText( input, sizeof( input ) );
		FoFAIProfileNormalizeFilename(
			input, "txt", filename, sizeof( filename ) );
		if ( owner->SaveBotPresetFile( filename ) )
		{
			owner->LoadBotPresetFile( filename );
			Q_strncpy( m_szPresetFilename, filename,
				sizeof( m_szPresetFilename ) );
			RefreshPresetList();
			UpdatePresetSaveName( filename );
		}
		return;
	}
	if ( !Q_stricmp( command, "save_wave" ) )
	{
		CommitRows();
		char input[MAX_PATH];
		char filename[MAX_PATH];
		m_pWaveSaveName->GetText( input, sizeof( input ) );
		FoFAIProfileNormalizeFilename(
			input, "ai", filename, sizeof( filename ) );
		if ( owner->SaveSpawnScript( filename ) )
		{
			RefreshWaveList();
			m_pWaveSaveName->SetText( filename );
		}
		return;
	}
	if ( !Q_strnicmp( command, "skill", 5 ) )
	{
		const int visibleRow = Q_atoi( command + 5 );
		if ( visibleRow >= 0 && visibleRow < ARRAYSIZE( m_Rows ) )
		{
			CommitRows();
			m_bSkillEditing[visibleRow] = !m_bSkillEditing[visibleRow];
			PopulateRows();
		}
		return;
	}
	if ( !Q_strnicmp( command, "del", 3 ) )
	{
		const int visibleRow = Q_atoi( command + 3 );
		if ( visibleRow >= 0 && visibleRow < ARRAYSIZE( m_Rows ) )
		{
			CommitRows();
			owner->RemoveBotPreset( m_iFirstRow + visibleRow );
			m_iFirstRow = clamp( m_iFirstRow, 0,
				MAX( 0, owner->GetBotPresetCount() - 10 ) );
			Q_memset( m_bSkillEditing, 0, sizeof( m_bSkillEditing ) );
			PopulateRows();
		}
		return;
	}
	BaseClass::OnCommand( command );
}

void CNPCProfileManagement::OnMouseWheeled( int delta )
{
	if ( m_bVisible && delta != 0 )
	{
		ShiftRows( delta > 0 ? -1 : 1 );
		return;
	}
	BaseClass::OnMouseWheeled( delta );
}

void CNPCProfileManagement::OnTextChanged( KeyValues *pData )
{
	if ( m_bRefreshing || !pData )
		return;
	vgui::Panel *panel = static_cast<vgui::Panel *>( pData->GetPtr( "panel" ) );
	CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( !owner )
		return;

	if ( panel == m_pPresets )
	{
		KeyValues *data = m_pPresets->GetActiveItemUserData();
		const char *filename = data ? data->GetString( "filename", "" ) : "";
		if ( filename[0] )
		{
			CommitRows();
			if ( owner->LoadBotPresetFile( filename ) )
			{
				Q_strncpy( m_szPresetFilename, filename,
					sizeof( m_szPresetFilename ) );
				m_iFirstRow = 0;
				Q_memset( m_bSkillEditing, 0,
					sizeof( m_bSkillEditing ) );
				UpdatePresetSaveName( filename );
				PopulateRows();
				m_bRefreshing = true;
				m_pPresets->SilentActivateItemByRow( 0 );
				m_bRefreshing = false;
			}
		}
	}
	else if ( panel == m_pWaves )
	{
		KeyValues *data = m_pWaves->GetActiveItemUserData();
		const char *filename = data ? data->GetString( "filename", "" ) : "";
		if ( filename[0] )
		{
			m_pWaveSaveName->SetText( filename );
			m_bRefreshing = true;
			m_pWaves->SilentActivateItemByRow( 0 );
			m_bRefreshing = false;
		}
	}
}

void CNPCProfileManagement::SetEditorVisible( bool visible )
{
	if ( visible == m_bVisible )
		return;
	if ( !visible && m_bVisible )
		CommitRows();

	m_bVisible = visible;
	SetVisible( visible );
	SetMouseInputEnabled( visible );
	SetKeyBoardInputEnabled( visible );
	if ( visible )
	{
		CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
		const char *filename = owner ? owner->GetBotPresetFilename() : "";
		if ( filename && filename[0] )
		{
			Q_strncpy( m_szPresetFilename, filename,
				sizeof( m_szPresetFilename ) );
			UpdatePresetSaveName( filename );
		}
		m_iFirstRow = 0;
		Q_memset( m_bSkillEditing, 0, sizeof( m_bSkillEditing ) );
		m_bRefreshing = true;
		for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
		{
			for ( int slot = 0;
				slot < ARRAYSIZE( m_Rows[row].weapons ); ++slot )
			{
				FoFAIProfilePopulateWeaponCombo(
					m_Rows[row].weapons[slot] );
			}
		}
		m_bRefreshing = false;
		RefreshPresetList();
		RefreshWaveList();
		PopulateRows();
		MakePopup( false );
		MoveToFront();
		RequestFocus();
		vgui::surface()->SetCursorAlwaysVisible( true );
		m_bRightWasDown = true;
		m_bRightReleasedSinceOpen = false;
		UpdateLayout();
	}
	else
	{
		vgui::surface()->SetCursorAlwaysVisible( false );
		CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
		if ( owner )
			owner->NotifyBotPresetsChanged();
	}
}

bool CNPCProfileManagement::IsEditorVisible() const
{
	return m_bVisible;
}

void CNPCProfileManagement::UpdateLayout()
{
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	const int wide = MIN( ScreenWidth(), FoFEditorScale( 260.0f ) );
	const int y = MIN( ScreenHeight(), FoFEditorScale( 41.0f ) );
	const int tall = MAX( 0, MIN( FoFEditorScale( 424.0f ),
		ScreenHeight() - y - FoFEditorScale( 15.0f ) ) );
	SetBounds( MAX( 0, ScreenWidth() - wide ), y, wide, tall );

	const int topTall = MAX( 16, FoFEditorScale( 20.0f ) );
	const int rowTall = MAX( 14, FoFEditorScale( 15.0f ) );
	m_pPresets->SetBounds( 0, FoFEditorScale( 4.0f ),
		FoFEditorScale( 125.0f ), topTall );
	m_pPresetSaveName->SetBounds(
		FoFEditorScale( 128.0f ), FoFEditorScale( 4.0f ),
		FoFEditorScale( 82.0f ), topTall );
	m_pPresetSave->SetBounds(
		FoFEditorScale( 213.0f ), FoFEditorScale( 4.0f ),
		FoFEditorScale( 40.0f ), topTall );
	m_pAddEmpty->SetBounds( 0, FoFEditorScale( 360.0f ),
		FoFEditorScale( 75.0f ), rowTall );
	m_pWaves->SetBounds( 0, FoFEditorScale( 400.0f ),
		FoFEditorScale( 125.0f ), topTall );
	m_pWaveSaveName->SetBounds(
		FoFEditorScale( 128.0f ), FoFEditorScale( 400.0f ),
		FoFEditorScale( 82.0f ), topTall );
	m_pWaveSave->SetBounds(
		FoFEditorScale( 213.0f ), FoFEditorScale( 400.0f ),
		FoFEditorScale( 40.0f ), topTall );

	for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
	{
		const int rowY = FoFEditorScale( 31.0f + row * 32.0f );
		const int detailY = FoFEditorScale( 47.0f + row * 32.0f );
		m_Rows[row].name->SetBounds( 0, rowY,
			FoFEditorScale( 82.0f ), rowTall );
		m_Rows[row].team->SetBounds( FoFEditorScale( 83.0f ), rowY,
			FoFEditorScale( 82.0f ), rowTall );
		m_Rows[row].skillToggle->SetBounds(
			FoFEditorScale( 166.0f ), rowY,
			FoFEditorScale( 83.0f ), rowTall );
		for ( int skill = 0; skill < ARRAYSIZE( m_Rows[row].skills ); ++skill )
		{
			m_Rows[row].skills[skill]->SetBounds(
				FoFEditorScale( skill * 50.0f ), detailY,
				FoFEditorScale( 48.0f ), rowTall );
		}
		for ( int slot = 0; slot < ARRAYSIZE( m_Rows[row].weapons ); ++slot )
		{
			m_Rows[row].weapons[slot]->SetBounds(
				FoFEditorScale( slot * 63.0f ), detailY,
				FoFEditorScale( 61.0f ), rowTall );
		}
		m_Rows[row].remove->SetBounds( FoFEditorScale( 251.0f ), rowY,
			FoFEditorScale( 9.0f ), rowTall );
	}
}

void CNPCProfileManagement::RefreshPresetList()
{
	m_bRefreshing = true;
	m_pPresets->RemoveAll();
	KeyValues *heading = new KeyValues( "data" );
	heading->SetString( "filename", "" );
	m_pPresets->AddItem( "BOT DEFINITIONS", heading );
	heading->deleteThis();
	FoFAIProfileAddFiles(
		m_pPresets, "fof_scripts/bots/*.txt", "MOD" );
	FoFAIProfileAddFiles(
		m_pPresets, "fof_scripts/bots/*.txt", "GAME" );
	m_pPresets->SilentActivateItemByRow( 0 );
	m_bRefreshing = false;
}

void CNPCProfileManagement::RefreshWaveList()
{
	m_bRefreshing = true;
	m_pWaves->RemoveAll();
	KeyValues *heading = new KeyValues( "data" );
	heading->SetString( "filename", "" );
	m_pWaves->AddItem( "SPAWN GROUPS", heading );
	heading->deleteThis();
	FoFAIProfileAddFiles(
		m_pWaves, "fof_scripts/ai_editor/*.ai", "MOD" );
	FoFAIProfileAddFiles(
		m_pWaves, "fof_scripts/ai_editor/*.ai", "GAME" );
	m_pWaves->SilentActivateItemByRow( 0 );
	m_bRefreshing = false;
}

void CNPCProfileManagement::PopulateRows()
{
	CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( !owner )
		return;
	m_bRefreshing = true;
	m_iFirstRow = clamp( m_iFirstRow, 0,
		MAX( 0, owner->GetBotPresetCount() - 10 ) );
	for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
	{
		const FoFAIBotPreset *preset = owner->GetBotPreset( m_iFirstRow + row );
		SetRowVisible( row, preset != NULL );
		if ( !preset )
			continue;
		m_Rows[row].name->SetText( preset->name );
		m_Rows[row].name->SetFgColor(
			FoFAIProfileTeamColor( preset->forceTeam ) );
		FoFAIProfileActivateComboInt( m_Rows[row].team, preset->forceTeam );
		char averageText[32];
		const float average = ( preset->rotationSpeed + preset->shootDelay +
			preset->aimTrailing + preset->strafe ) * 0.25f;
		Q_snprintf( averageText, sizeof( averageText ),
			"Skill Avg %.1f", average );
		m_Rows[row].skillToggle->SetText( averageText );
		FoFAIProfileActivateComboInt(
			m_Rows[row].skills[0], preset->rotationSpeed );
		FoFAIProfileActivateComboInt(
			m_Rows[row].skills[1], preset->shootDelay );
		FoFAIProfileActivateComboInt(
			m_Rows[row].skills[2], preset->aimTrailing );
		FoFAIProfileActivateComboInt(
			m_Rows[row].skills[3], preset->strafe );
		FoFAIProfileActivateComboInt(
			m_Rows[row].skills[4], preset->aggression );
		int itemIDs[4];
		FoFAIProfileParseEquipment( preset->equipment, itemIDs );
		for ( int slot = 0; slot < ARRAYSIZE( m_Rows[row].weapons ); ++slot )
			FoFAIProfileActivateComboInt( m_Rows[row].weapons[slot], itemIDs[slot] );
	}
	m_bRefreshing = false;
}

void CNPCProfileManagement::CommitRows()
{
	if ( m_bRefreshing )
		return;
	CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( !owner )
		return;
	for ( int row = 0; row < ARRAYSIZE( m_Rows ); ++row )
	{
		const int index = m_iFirstRow + row;
		const FoFAIBotPreset *existing = owner->GetBotPreset( index );
		if ( !existing )
			continue;
		FoFAIBotPreset preset = *existing;
		m_Rows[row].name->GetText( preset.name, sizeof( preset.name ) );
		if ( !preset.name[0] )
			Q_strncpy( preset.name, "name...", sizeof( preset.name ) );
		preset.forceTeam = FoFAIProfileGetComboInt(
			m_Rows[row].team, preset.forceTeam );
		preset.rotationSpeed = FoFAIProfileGetComboInt(
			m_Rows[row].skills[0], preset.rotationSpeed );
		preset.shootDelay = FoFAIProfileGetComboInt(
			m_Rows[row].skills[1], preset.shootDelay );
		preset.aimTrailing = FoFAIProfileGetComboInt(
			m_Rows[row].skills[2], preset.aimTrailing );
		preset.strafe = FoFAIProfileGetComboInt(
			m_Rows[row].skills[3], preset.strafe );
		preset.aggression = FoFAIProfileGetComboInt(
			m_Rows[row].skills[4], preset.aggression );
		Q_snprintf( preset.equipment, sizeof( preset.equipment ),
			"%i,%i,%i,%i,",
			FoFAIProfileGetComboInt( m_Rows[row].weapons[0], -1 ),
			FoFAIProfileGetComboInt( m_Rows[row].weapons[1], -1 ),
			FoFAIProfileGetComboInt( m_Rows[row].weapons[2], -1 ),
			FoFAIProfileGetComboInt( m_Rows[row].weapons[3], -1 ) );
		owner->SetBotPreset( index, preset );
	}
	owner->NotifyBotPresetsChanged();
}

void CNPCProfileManagement::SetRowVisible( int row, bool visible )
{
	if ( row < 0 || row >= ARRAYSIZE( m_Rows ) )
		return;
	vgui::Panel *baseControls[] =
	{
		m_Rows[row].name, m_Rows[row].team,
		m_Rows[row].skillToggle, m_Rows[row].remove
	};
	for ( int index = 0; index < ARRAYSIZE( baseControls ); ++index )
	{
		baseControls[index]->SetVisible( visible );
		baseControls[index]->SetEnabled( visible );
		baseControls[index]->SetMouseInputEnabled( visible );
		baseControls[index]->SetKeyBoardInputEnabled( visible );
	}
	for ( int skill = 0; skill < ARRAYSIZE( m_Rows[row].skills ); ++skill )
	{
		const bool show = visible && m_bSkillEditing[row];
		m_Rows[row].skills[skill]->SetVisible( show );
		m_Rows[row].skills[skill]->SetEnabled( show );
		m_Rows[row].skills[skill]->SetMouseInputEnabled( show );
		m_Rows[row].skills[skill]->SetKeyBoardInputEnabled( show );
	}
	for ( int slot = 0; slot < ARRAYSIZE( m_Rows[row].weapons ); ++slot )
	{
		const bool show = visible && !m_bSkillEditing[row];
		m_Rows[row].weapons[slot]->SetVisible( show );
		m_Rows[row].weapons[slot]->SetEnabled( show );
		m_Rows[row].weapons[slot]->SetMouseInputEnabled( show );
		m_Rows[row].weapons[slot]->SetKeyBoardInputEnabled( show );
	}
}

void CNPCProfileManagement::ShiftRows( int delta )
{
	CHudFoFAIEditor *owner = GET_HUDELEMENT( CHudFoFAIEditor );
	if ( !owner || delta == 0 )
		return;
	CommitRows();
	m_iFirstRow = clamp( m_iFirstRow + delta, 0,
		MAX( 0, owner->GetBotPresetCount() - 10 ) );
	Q_memset( m_bSkillEditing, 0, sizeof( m_bSkillEditing ) );
	PopulateRows();
}

void CNPCProfileManagement::UpdatePresetSaveName( const char *filename )
{
	if ( filename && !Q_stricmp( filename, "1_default_bots.txt" ) )
		m_pPresetSaveName->SetText( "my_bot_script.txt" );
	else
		m_pPresetSaveName->SetText( filename && filename[0] ? filename :
			"my_bot_script.txt" );
}

bool FoFAIProfileEditorIsActive()
{
	CNPCProfileManagement *panel = GET_HUDELEMENT( CNPCProfileManagement );
	return panel && panel->IsEditorVisible();
}

void FoFAIProfileEditorOpen()
{
	CNPCProfileManagement *panel = GET_HUDELEMENT( CNPCProfileManagement );
	if ( panel )
		panel->SetEditorVisible( true );
}

void FoFAIProfileEditorClose()
{
	CNPCProfileManagement *panel = GET_HUDELEMENT( CNPCProfileManagement );
	if ( panel )
		panel->SetEditorVisible( false );
}

int FoFEditorScale( float value )
{
	return RoundFloatToInt(
		( (float)ScreenHeight() / 480.0f ) * value );
}

bool FoFEditorCheatsEnabled()
{
	ConVarRef cheats( "sv_cheats", true );
	return cheats.IsValid() && cheats.GetBool();
}

void FoFEditorGetMapName( char *buffer, int bufferSize )
{
	if ( !buffer || bufferSize <= 0 )
		return;

	buffer[0] = '\0';
	if ( engine && engine->GetLevelName() && engine->GetLevelName()[0] )
		Q_FileBase( engine->GetLevelName(), buffer, bufferSize );
}

bool FoFEditorButtonPressed( ButtonCode_t code, bool &wasDown )
{
	bool down = inputsystem && inputsystem->IsButtonDown( code );
	if ( code >= MOUSE_FIRST && code <= MOUSE_LAST )
		down = down || vgui::input()->IsMouseDown( code );
	else if ( code >= KEY_FIRST && code <= KEY_LAST )
		down = down || vgui::input()->IsKeyDown( code );
	const bool pressed = down && !wasDown;
	wasDown = down;
	return pressed;
}

bool FoFEditorTracePlacement(
	float normalOffset, Vector &origin, QAngle &angles )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return false;

	Vector forward;
	player->EyeVectors( &forward );

	trace_t trace;
	const Vector start = player->EyePosition();
	UTIL_TraceLine(
		start,
		start + forward * MAX_TRACE_LENGTH,
		MASK_SHOT,
		player,
		COLLISION_GROUP_NONE,
		&trace );
	if ( !trace.DidHit() )
		return false;

	origin = trace.endpos + trace.plane.normal * normalOffset;
	// FoF stores the three components of the placement view vector in
	// dir_x/dir_y/dir_z.  These are not Euler angles despite the legacy field
	// names used by the editor scripts.
	angles.Init( forward.x, forward.y, forward.z );
	return true;
}

void FoFEditorClientCommand( const char *format, ... )
{
	if ( !engine || !format || !format[0] )
		return;

	char command[1024];
	va_list args;
	va_start( args, format );
	V_vsnprintf( command, sizeof( command ), format, args );
	va_end( args );
	engine->ClientCmd( command );
}

void FoFEditorDrawText(
	vgui::HFont font, const Color &color, int x, int y,
	const char *text )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	wchar_t wideText[512];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		text, wideText, sizeof( wideText ) );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText(
		wideText, V_wcslen( wideText ) );
}

void FoFEditorDrawRoundedRect(
	int x, int y, int wide, int tall, int radius, const Color &color )
{
	if ( wide <= 0 || tall <= 0 )
		return;

	radius = clamp( radius, 0, MIN( wide, tall ) / 2 );
	vgui::surface()->DrawSetColor( color );
	if ( radius <= 0 )
	{
		vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
		return;
	}

	vgui::surface()->DrawFilledRect(
		x + radius, y, x + wide - radius, y + tall );
	vgui::surface()->DrawFilledRect(
		x, y + radius, x + wide, y + tall - radius );
	for ( int row = 0; row < radius; ++row )
	{
		const float dy = (float)radius - (float)row - 0.5f;
		const int inset = MAX( 0, radius - RoundFloatToInt(
			sqrtf( (float)( radius * radius ) - dy * dy ) ) );
		vgui::surface()->DrawFilledRect(
			x + inset, y + row, x + wide - inset, y + row + 1 );
		vgui::surface()->DrawFilledRect(
			x + inset, y + tall - row - 1,
			x + wide - inset, y + tall - row );
	}
}

void FoFEditorDrawLocalizedText(
	vgui::HFont font, const Color &color, int x, int y,
	const char *token, const char *fallback )
{
	if ( font == vgui::INVALID_FONT )
		return;

	const wchar_t *text = token ? g_pVGuiLocalize->Find( token ) : NULL;
	wchar_t fallbackText[512];
	if ( !text )
	{
		g_pVGuiLocalize->ConvertANSIToUnicode(
			fallback ? fallback : "", fallbackText, sizeof( fallbackText ) );
		text = fallbackText;
	}

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, V_wcslen( text ) );
}

static void FoFEditorDrawWideTextLine(
	vgui::HFont font, const Color &color, int x, int y,
	const wchar_t *text, int length )
{
	if ( length <= 0 )
		return;

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, length );
}

int FoFEditorDrawLocalizedWrappedText(
	vgui::HFont font, const Color &color, int x, int y, int maxWide,
	const char *token, const char *fallback )
{
	if ( font == vgui::INVALID_FONT || maxWide <= 0 )
		return y;

	const wchar_t *source = token ? g_pVGuiLocalize->Find( token ) : NULL;
	wchar_t fallbackText[512];
	if ( !source )
	{
		g_pVGuiLocalize->ConvertANSIToUnicode(
			fallback ? fallback : "", fallbackText, sizeof( fallbackText ) );
		source = fallbackText;
	}

	const int lineTall = MAX( 1, vgui::surface()->GetFontTall( font ) );
	const int sourceLength = MIN( (int)V_wcslen( source ), 511 );
	int lineStart = 0;
	int lineWide = 0;
	int lastBreak = -1;
	int drawY = y;

	for ( int index = 0; index <= sourceLength; ++index )
	{
		const wchar_t character = index < sourceLength ? source[index] : L'\0';
		const bool explicitBreak = character == L'\n' || character == L'\0';
		const int characterWide = explicitBreak ? 0 :
			vgui::surface()->GetCharacterWidth( font, character );
		if ( character == L' ' || character == L'/' )
			lastBreak = index + 1;

		if ( explicitBreak || ( lineWide + characterWide > maxWide && index > lineStart ) )
		{
			int lineEnd = index;
			if ( !explicitBreak && lastBreak > lineStart )
				lineEnd = lastBreak;
			while ( lineEnd > lineStart && source[lineEnd - 1] == L' ' )
				--lineEnd;
			FoFEditorDrawWideTextLine(
				font, color, x, drawY, source + lineStart, lineEnd - lineStart );
			drawY += lineTall;

			lineStart = explicitBreak ? index + 1 : lineEnd;
			while ( lineStart < sourceLength && source[lineStart] == L' ' )
				++lineStart;
			index = lineStart - 1;
			lineWide = 0;
			lastBreak = -1;
			continue;
		}
		lineWide += characterWide;
	}

	return drawY;
}
