// Break Bad and team-play cash loadout menu data and interaction.

#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_purchase_menu.h"
#include "fof/fof_team_menu.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_player_shared.h"
#include "filesystem.h"
#include "iinput.h"
#include "KeyValues.h"
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui/ILocalize.h>
#include <string.h>

#if defined( _WIN32 )
#define FOF_STRTOK_R strtok_s
#else
#define FOF_STRTOK_R strtok_r
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar fof_loadout_mini(
	"fof_loadout_mini",
	"1",
	FCVAR_ARCHIVE,
	"Use FoF's compact twenty-five-preset purchase layout." );

static ConVar fof_loadout_filename(
	"fof_loadout_filename",
	"default.txt",
	FCVAR_ARCHIVE,
	"FoF cash-loadout preset file." );

struct FoFPurchaseItemInfo
{
	int id;
	int range;
	const char *material;
	const char *nameToken;
	const char *fallbackName;
};

// This is the purchasable subset of FoF's 56-entry item table at
// Range zero denotes a skill or utility which remains
// available in every range filter. Gameplay price and tier fields live in
// shared/fof/items so client and server cannot drift apart.
static const FoFPurchaseItemInfo s_FoFPurchaseItems[] =
{
	{  1, 1, "vgui/knife",            "#Item1b",       "Knife" },
	{  2, 2, "vgui/coltnavy",         "#Item2",        "Colt Navy 1851" },
	{  3, 1, "vgui/deringer",         "#Item3b",       "Deringer" },
	{  4, 2, "vgui/henry",            "#Item4a",       "Yellowboy 1866" },
	{  5, 3, "vgui/carbine",          "#Item5",        "Smith Carbine" },
	{  6, 1, "vgui/coachgun",         "#Item6",        "Coachgun" },
	{  7, 2, "vgui/dynamite",         "#Item0",        "Dynamite" },
	{ 11, 3, "vgui/bow",              "#Item11",       "Bow" },
	{ 12, 3, "vgui/sharps",           "#Item12",       "Sharps Rifle" },
	{ 13, 1, "vgui/sawed_shotgun",    "#Item13b",      "Sawed Shotgun" },
	{ 14, 0, "vgui/gun_throw",        "#Item14",       "Handgun Throw" },
	{ 15, 0, "vgui/walljump",         "#Item23e",      "Jumpmaster" },
	{ 18, 2, "vgui/spencer",          "#Item18",       "Spencer Carbine" },
	{ 19, 1, "vgui/axe",              "#Item19",       "Hatchet" },
	{ 20, 0, "vgui/boots",            "#Item34",       "Boots" },
	{ 21, 2, "vgui/peacemaker",       "#Item21",       "Colt Peacemaker" },
	{ 22, 1, "vgui/hammerless",       NULL,             "Hammerless" },
	{ 23, 0, "vgui/slide",            "#Item23d",      "Slide" },
	{ 24, 3, "vgui/bow_black",        "#Item11b",      "Black Bow" },
	{ 26, 1, "vgui/machete",          "#Item26",       "Machete" },
	{ 27, 2, "vgui/dynamite_black",   "#Item0b",       "Black Dynamite" },
	{ 28, 1, "vgui/volcanic",         "#ItemVolcanic", "Volcanic Pistol" },
	{ 29, 1, "vgui/shotgun",          "#Item30",       "Pump Shotgun W1893" },
	{ 30, 2, "vgui/nma",              NULL,             "Remington Army" },
	{ 31, 2, "vgui/schofield",        "#Item31a",      "S&W Schofield" },
	{ 32, 3, "vgui/walker",           "#Item32",       "Colt Walker" },
	{ 33, 2, "vgui/maresleg",         "#MaresLeg",     "Mare's Leg" },
	{ 34, 0, "vgui/brass_knuckles",   "#Item34b",      "Brass Knuckles" },
	{ 41, 0, "vgui/accuracy_right",   "#Item41",       "Right-handed" },
	{ 42, 0, "vgui/accuracy_left",    "#Item42",       "Left-handed" },
	{ 43, 0, "vgui/accuracy_ambi",    "#Item43",       "Ambidextrous" },
	{ 44, 0, "vgui/accuracy_fan",     "#Item44",       "Fanning" }
};

// EditPreset presents skills first, followed by utility, sidearms and long
// guns.  Keep this independent from numeric IDs so the editor follows the
// shipped menu instead of exposing a sparse 0..55 table.
static const int s_FoFPurchaseEditorItemOrder[] =
{
	41, 42, 43, 44, 14, 15, 23, 34,
	20, 1, 3, 22, 28, 2, 13, 19,
	11, 30, 31, 33, 5, 21, 7, 4,
	6, 18, 24, 26, 27, 29, 32, 12
};

static const FoFPurchaseItemInfo *FoFFindPurchaseItem( int itemId )
{
	for ( int i = 0; i < ARRAYSIZE( s_FoFPurchaseItems ); ++i )
	{
		if ( s_FoFPurchaseItems[i].id == itemId )
			return &s_FoFPurchaseItems[i];
	}
	return NULL;
}

const char *FoFPurchaseItemMaterial( int itemId )
{
	const FoFPurchaseItemInfo *item = FoFFindPurchaseItem( itemId );
	return item ? item->material : NULL;
}

const char *FoFPurchaseItemNameToken( int itemId )
{
	const FoFPurchaseItemInfo *item = FoFFindPurchaseItem( itemId );
	return item ? item->nameToken : NULL;
}

const char *FoFPurchaseItemFallbackName( int itemId )
{
	const FoFPurchaseItemInfo *item = FoFFindPurchaseItem( itemId );
	return item ? item->fallbackName : "";
}

int FoFPurchaseItemBasePrice( int itemId )
{
	const FoFItemDefinition_t *item = FoFFindItemDefinitionById( itemId );
	return item ? item->m_nBasePrice : -1;
}

int FoFPurchaseItemTier( int itemId )
{
	const FoFItemDefinition_t *item = FoFFindItemDefinitionById( itemId );
	return item ? clamp( item->m_nPurchaseTier + 1, 1, 3 ) : 1;
}

int FoFPurchaseItemRange( int itemId )
{
	const FoFPurchaseItemInfo *item = FoFFindPurchaseItem( itemId );
	return item ? item->range : 0;
}

int FoFPurchaseEditorItemCount()
{
	return ARRAYSIZE( s_FoFPurchaseEditorItemOrder );
}

int FoFPurchaseEditorItemId( int index )
{
	return index >= 0 && index < FoFPurchaseEditorItemCount() ?
		s_FoFPurchaseEditorItemOrder[index] : -1;
}

int FoFPurchasePresetPriceForBuyZone(
	const FoFPurchasePreset &preset,
	int currentMode,
	int inBuyZone )
{
	int price = 0;
	for ( int item = 0; item < preset.itemCount; ++item )
	{
		price += FoFItemAdjustedPrice(
			FoFFindItemDefinitionById( preset.items[item] ),
			currentMode,
			inBuyZone );
	}
	return price;
}

int FoFScalePurchasePixel( float virtualPixels, float scale )
{
	// VGUI's proportional conversion truncates positive values.  Rounding here
	// moves every compact-menu column by one pixel at common resolutions.
	return MAX( (int)( virtualPixels * scale ), 1 );
}

FoFPurchaseMenuLayout FoFBuildPurchaseMenuLayout(
	bool quick,
	int screenWide,
	int screenTall )
{
	FoFPurchaseMenuLayout layout;
	layout.quick = quick;
	layout.scale = MAX( (float)screenTall / 480.0f, 0.1f );
	layout.columns = quick ? 5 : 4;
	layout.rows = 5;
	layout.visiblePresets = quick ? 25 : 20;
	layout.cardWide = FoFScalePurchasePixel(
		quick ? 70.0f : 100.0f, layout.scale );
	layout.cardTall = FoFScalePurchasePixel(
		quick ? 35.0f : 50.0f, layout.scale );
	layout.gap = FoFScalePurchasePixel(
		quick ? 4.0f : 5.0f, layout.scale );
	layout.pitchX = layout.cardWide + layout.gap;
	layout.pitchY = layout.cardTall + layout.gap;
	layout.gridWide = layout.columns * layout.cardWide +
		( layout.columns - 1 ) * layout.gap;
	layout.gridTall = layout.rows * layout.cardTall +
		( layout.rows - 1 ) * layout.gap;
	layout.gridX = screenWide / 2 - layout.gridWide / 2;
	layout.gridY = quick ?
		screenTall - layout.gridTall -
			FoFScalePurchasePixel( 40.0f, layout.scale ) :
		screenTall / 2 - layout.gridTall / 2;
	return layout;
}

static int __cdecl FoFComparePurchasePresets(
	const FoFPurchasePreset *left,
	const FoFPurchasePreset *right )
{
	if ( left->price != right->price )
		return left->price < right->price ? -1 : 1;
	if ( left->sourceOrder != right->sourceOrder )
		return left->sourceOrder < right->sourceOrder ? -1 : 1;
	return 0;
}

static void FoFConfigurePurchaseButton( vgui::Button *button )
{
	if ( !button )
		return;
	button->SetButtonBorderEnabled( false );
	button->SetPaintBorderEnabled( false );
	button->DrawFocusBox( false );
	button->SetPaintEnabled( false );
	button->SetPaintBackgroundEnabled( false );
	button->SetMouseInputEnabled( true );
	button->SetKeyBoardInputEnabled( false );
	button->SetButtonActivationType( vgui::Button::ACTIVATE_ONPRESSED );
	// FoF's cash purchase panel keeps the rollover cue, but its controls
	// do not emit a separate press/release click.
	button->SetArmedSound( "ui/rollover_cyl2.wav" );
	button->SetDepressedSound( NULL );
	button->SetReleasedSound( NULL );
	button->SetZPos( 40 );
	button->SetVisible( false );
}

static void FoFConfigureSilentPurchaseButton( vgui::Button *button )
{
	FoFConfigurePurchaseButton( button );
	if ( button )
		button->SetArmedSound( NULL );
}

CFoFPurchasePaintPanel::CFoFPurchasePaintPanel( CHudFoF *owner )
	: BaseClass( owner, "FoFPurchasePaintLayer" )
	, m_pOwner( owner )
{
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetZPos( 0 );
	SetVisible( false );
}

void CFoFPurchasePaintPanel::Paint()
{
	if ( m_pOwner )
		m_pOwner->PaintPurchaseMenu();
}

void CHudFoF::EnsurePurchaseMenuControls()
{
	for ( int i = 0; i < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++i )
	{
		if ( m_pPurchasePresetButtons[i] )
			continue;
		char name[40];
		char command[48];
		Q_snprintf( name, sizeof( name ), "FoFPurchasePreset%d", i );
		Q_snprintf( command, sizeof( command ), "fof_purchase_preset_%d", i );
		m_pPurchasePresetButtons[i] = new vgui::Button(
			this, name, "", this, command );
		FoFConfigurePurchaseButton( m_pPurchasePresetButtons[i] );
	}

	if ( !m_pPurchasePageUpButton )
	{
		m_pPurchasePageUpButton = new vgui::Button(
			this, "FoFPurchasePageUp", "", this, "fof_purchase_page_up" );
		FoFConfigurePurchaseButton( m_pPurchasePageUpButton );
	}
	if ( !m_pPurchasePageDownButton )
	{
		m_pPurchasePageDownButton = new vgui::Button(
			this, "FoFPurchasePageDown", "", this, "fof_purchase_page_down" );
		FoFConfigurePurchaseButton( m_pPurchasePageDownButton );
	}
	if ( !m_pPurchaseCloseButton )
	{
		m_pPurchaseCloseButton = new vgui::Button(
			this, "FoFPurchaseClose", "", this, "fof_menu_cancel" );
		FoFConfigureSilentPurchaseButton( m_pPurchaseCloseButton );
	}
	if ( !m_pPurchaseSwitchButton )
	{
		m_pPurchaseSwitchButton = new vgui::Button(
			this, "FoFPurchaseSwitch", "", this, "fof_purchase_switch" );
		FoFConfigureSilentPurchaseButton( m_pPurchaseSwitchButton );
	}
	for ( int i = 0; i < FOF_PURCHASE_FILTER_COUNT; ++i )
	{
		if ( m_pPurchaseFilterButtons[i] )
			continue;
		char name[40];
		char command[48];
		Q_snprintf( name, sizeof( name ), "FoFPurchaseFilter%d", i );
		Q_snprintf( command, sizeof( command ), "fof_purchase_filter_%d", i );
		m_pPurchaseFilterButtons[i] = new vgui::Button(
			this, name, "", this, command );
		FoFConfigurePurchaseButton( m_pPurchaseFilterButtons[i] );
	}
	if ( !m_pPurchaseEditorButton )
	{
		m_pPurchaseEditorButton = new vgui::Button(
			this, "FoFPurchaseEditor", "", this, "fof_purchase_editor" );
		FoFConfigureSilentPurchaseButton( m_pPurchaseEditorButton );
		// Keep the pressed state visible until mouse release.  EditPreset uses
		// yellow for rollover and red while this full-width control is held.
		m_pPurchaseEditorButton->SetButtonActivationType(
			vgui::Button::ACTIVATE_ONPRESSEDANDRELEASED );
	}
	if ( !m_pPurchaseFileList )
	{
		// FoF's PresetMenu creates FileList as a fourteen-row,
		// non-editable ComboBox.  It belongs only to the full layout.
		m_pPurchaseFileList = new vgui::ComboBox(
			this, "FoFPurchaseFileList", 14, false );
		m_pPurchaseFileList->AddActionSignalTarget( this );
		m_pPurchaseFileList->SetOpenDirection( vgui::Menu::UP );
		m_pPurchaseFileList->SetMouseInputEnabled( true );
		m_pPurchaseFileList->SetKeyBoardInputEnabled( false );
		m_pPurchaseFileList->SetZPos( 50 );
		m_pPurchaseFileList->SetVisible( false );
	}
}

void CHudFoF::PopulatePurchaseLoadoutFiles()
{
	if ( !m_pPurchaseFileList )
		return;

	m_pPurchaseFileList->RemoveAll();
	const char *selectedFilename = fof_loadout_filename.GetString();
	int selectedRow = -1;
	int defaultRow = -1;
	int row = 0;

	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *foundFilename = filesystem->FindFirstEx(
		"fof_scripts/loadouts/*.txt", "MOD", &findHandle );
	while ( foundFilename )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
		{
			const char *filename = V_UnqualifiedFileName( foundFilename );
			char path[MAX_PATH];
			Q_snprintf( path, sizeof( path ),
				"fof_scripts/loadouts/%s", filename );
			if ( filename[0] && filesystem->FileExists( path, "MOD" ) )
			{
				KeyValues *item = new KeyValues( "data" );
				item->SetString( "filename", filename );
				m_pPurchaseFileList->AddItem( filename, item );
				item->deleteThis();

				if ( selectedFilename &&
					!Q_stricmp( filename, selectedFilename ) )
				{
					selectedRow = row;
				}
				if ( !Q_stricmp( filename, "default.txt" ) )
					defaultRow = row;
				++row;
			}
		}
		foundFilename = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );

	if ( selectedRow < 0 )
		selectedRow = defaultRow >= 0 ? defaultRow : ( row > 0 ? 0 : -1 );
	if ( selectedRow < 0 )
		return;

	m_pPurchaseFileList->SilentActivateItemByRow( selectedRow );
	KeyValues *activeItem = m_pPurchaseFileList->GetActiveItemUserData();
	const char *activeFilename = activeItem ?
		activeItem->GetString( "filename", "" ) : "";
	if ( activeFilename[0] &&
		Q_stricmp( activeFilename, fof_loadout_filename.GetString() ) )
	{
		fof_loadout_filename.SetValue( activeFilename );
	}
}

void CHudFoF::OnPurchaseLoadoutChanged( KeyValues *data )
{
	if ( !data || data->GetPtr( "panel" ) != m_pPurchaseFileList )
		return;

	KeyValues *activeItem = m_pPurchaseFileList->GetActiveItemUserData();
	const char *filename = activeItem ?
		activeItem->GetString( "filename", "" ) : "";
	if ( !filename[0] )
		return;

	if ( Q_stricmp( filename, fof_loadout_filename.GetString() ) )
		fof_loadout_filename.SetValue( filename );

	if ( m_iMenuKind == FOF_MENU_PURCHASE &&
		!m_bPurchaseQuickLayout )
	{
		LoadPurchasePresets();
		m_iPurchasePage = 0;
		SetPurchaseMenuControlsVisible( true );
	}
}

void CHudFoF::LoadPurchasePresets()
{
	m_PurchasePresets.Purge();

	// Compact PresetMenu uses its own authored 5x5 list. The shipped client
	// selects auto.txt here; fof_loadout_filename belongs to full mode only.
	const bool quick = m_bPurchaseQuickLayout;
	const char *filename = quick ? "auto.txt" :
		fof_loadout_filename.GetString();
	if ( !filename || !filename[0] )
		filename = "default.txt";
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "fof_scripts/loadouts/%s", filename );

	KeyValues *root = new KeyValues( "List" );
	if ( !root->LoadFromFile( filesystem, path, "GAME" ) )
	{
		Q_strncpy( path, "fof_scripts/loadouts/default.txt", sizeof( path ) );
		if ( !root->LoadFromFile( filesystem, path, "GAME" ) )
		{
			Warning( "[FoF] unable to load purchase presets from %s\n", path );
			root->deleteThis();
			return;
		}
	}

	int sourceOrder = 0;
	for ( KeyValues *entry = root->GetFirstSubKey(); entry;
		entry = entry->GetNextKey() )
	{
		if ( Q_stricmp( entry->GetName(), "preset" ) )
			continue;

		FoFPurchasePreset preset;
		preset.name = entry->GetString( "name", "" );
		preset.sourceOrder = sourceOrder++;
		char itemList[256];
		Q_strncpy( itemList, entry->GetString( "item_list", "" ),
			sizeof( itemList ) );
		char *context = NULL;
		for ( char *token = FOF_STRTOK_R( itemList, ",", &context ); token;
			token = FOF_STRTOK_R( NULL, ",", &context ) )
		{
			if ( preset.itemCount >= FOF_PURCHASE_MAX_ITEMS )
				break;
			const int itemId = Q_atoi( token );
			if ( FoFPurchaseItemBasePrice( itemId ) < 0 )
				continue;
			preset.items[preset.itemCount++] = itemId;
		}
		if ( preset.itemCount > 0 )
			m_PurchasePresets.AddToTail( preset );
	}
	root->deleteThis();
	UpdatePurchasePresetPrices();
}

void CHudFoF::UpdatePurchasePresetPrices()
{
	for ( int presetIndex = 0; presetIndex < m_PurchasePresets.Count();
		++presetIndex )
	{
		FoFPurchasePreset &preset = m_PurchasePresets[presetIndex];
		// Mode 2 bypasses buy-zone multipliers and therefore gives the normal
		// price used by the original preset ordering. Zone-adjusted prices are
		// calculated live without reshuffling cards when the player moves.
		preset.price = FoFPurchasePresetPriceForBuyZone( preset, 2, 0 );
	}
	// auto.txt already encodes the original quick-grid order. Sorting it by
	// price turns quick mode into a compressed copy of the full menu.
	if ( !m_bPurchaseQuickLayout )
		m_PurchasePresets.Sort( FoFComparePurchasePresets );
}

int CHudFoF::PurchaseVisiblePresetCount() const
{
	return m_bPurchaseQuickLayout ? 25 : 20;
}

bool CHudFoF::PurchasePresetMatchesFilter(
	const FoFPurchasePreset &preset ) const
{
	if ( m_iPurchaseFilter <= 0 )
		return true;

	bool onlyUniversalItems = true;
	for ( int i = 0; i < preset.itemCount; ++i )
	{
		const int range = FoFPurchaseItemRange( preset.items[i] );
		if ( range > 0 )
			onlyUniversalItems = false;
		if ( range == m_iPurchaseFilter )
			return true;
	}
	return onlyUniversalItems;
}

int CHudFoF::PurchaseFilteredPresetCount() const
{
	int count = 0;
	for ( int i = 0; i < m_PurchasePresets.Count(); ++i )
	{
		if ( PurchasePresetMatchesFilter( m_PurchasePresets[i] ) )
			++count;
	}
	return count;
}

int CHudFoF::PurchasePresetIndexForVisibleSlot( int visibleSlot ) const
{
	if ( visibleSlot < 0 || visibleSlot >= PurchaseVisiblePresetCount() )
		return -1;
	const int target = m_iPurchasePage * PurchaseVisiblePresetCount() +
		visibleSlot;
	int filtered = 0;
	for ( int i = 0; i < m_PurchasePresets.Count(); ++i )
	{
		if ( !PurchasePresetMatchesFilter( m_PurchasePresets[i] ) )
			continue;
		if ( filtered++ == target )
			return i;
	}
	return -1;
}

void CHudFoF::ShowPurchaseMenu( int page )
{
	ShowPurchaseMenu( page, fof_loadout_mini.GetBool() );
}

void CHudFoF::ShowPurchaseMenu( int page, bool quickLayout )
{
	if ( FoFHudIsSourceTVClient() )
		return;
	if ( IsSlideOpen() )
		CloseSlide();
	ClearMenu();
	m_bPurchaseQuickLayout = quickLayout;
	PopulatePurchaseLoadoutFiles();
	LoadPurchasePresets();
	m_iPurchaseFilter = 0;
	const int visible = MAX( PurchaseVisiblePresetCount(), 1 );
	const int pageCount = MAX(
		( PurchaseFilteredPresetCount() + visible - 1 ) / visible, 1 );
	m_iPurchasePage = clamp( page, 0, pageCount - 1 );
	m_bMenuVisible = true;
	m_bLocalMenu = true;
	m_iMenuKind = FOF_MENU_PURCHASE;
	LayoutMenuControls();
	SetMenuControlsVisible( true );
	BeginLocalMenuInput();
}

void CHudFoF::ToggleAutoBuyMenu()
{
	if ( FoFHudIsSourceTVClient() )
		return;

	// FoF's autobuy command toggles HudPresetLauncher itself.  Opening
	// it always selects the compact 5x5 auto.txt layout, independently of the
	// player's archived preference for the regular equipmenu command.
	if ( m_bMenuVisible && m_iMenuKind == FOF_MENU_PURCHASE )
	{
		ClearMenu();
		return;
	}

	ShowPurchaseMenu( 0, true );
}

void CHudFoF::SelectPurchasePreset( int visibleSlot )
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE )
		return;
	const int presetIndex = PurchasePresetIndexForVisibleSlot( visibleSlot );
	if ( presetIndex < 0 || presetIndex >= m_PurchasePresets.Count() )
		return;
	if ( m_bPurchaseEditMode )
	{
		OpenLoadoutEditor( presetIndex );
		return;
	}

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	const FoFPurchasePreset &preset = m_PurchasePresets[presetIndex];
	const int currentPrice = FoFPurchasePresetPriceForBuyZone(
		preset,
		FoFHudCurrentMode(),
		FoFInBuyZone( player ) );
	if ( !player || (int)FoFCash( player ) < currentPrice )
		return;
	SubmitEquipment( preset.items, preset.itemCount );
	// A purchase is one complete loadout transaction.  The shipped panel
	// disappears as soon as buy_end is submitted; additional purchases are
	// made by reopening it while standing in a Break Bad buy zone.
	ClearMenu();
}

void CHudFoF::ChangePurchasePage( int direction )
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE || direction == 0 )
		return;
	const int visible = MAX( PurchaseVisiblePresetCount(), 1 );
	const int pageCount = MAX(
		( PurchaseFilteredPresetCount() + visible - 1 ) / visible, 1 );
	m_iPurchasePage = clamp( m_iPurchasePage + direction, 0, pageCount - 1 );
	// Arrow visibility depends on the new page.  Updating only Enabled left
	// the initially hidden up arrow hidden forever after scrolling down.
	SetPurchaseMenuControlsVisible( true );
}

void CHudFoF::TogglePurchaseLayout()
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE )
		return;
	if ( m_bLoadoutEditorVisible )
		CloseLoadoutEditor();
	m_bPurchaseEditMode = false;
	m_bPurchaseQuickLayout = !m_bPurchaseQuickLayout;
	fof_loadout_mini.SetValue( m_bPurchaseQuickLayout );
	PopulatePurchaseLoadoutFiles();
	LoadPurchasePresets();
	m_iPurchasePage = 0;
	LayoutPurchaseMenuControls();
	SetPurchaseMenuControlsVisible( true );
}

void CHudFoF::SetPurchaseFilter( int filter )
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE )
		return;
	m_iPurchaseFilter = clamp( filter, 0, FOF_PURCHASE_FILTER_COUNT - 1 );
	m_iPurchasePage = 0;
	UpdatePurchaseButtonStates();
}

void CHudFoF::UpdatePurchaseButtonStates()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	const int cash = player ? (int)FoFCash( player ) : 0;
	const int currentMode = FoFHudCurrentMode();
	const int inBuyZone = FoFInBuyZone( player );
	for ( int slot = 0; slot < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++slot )
	{
		if ( !m_pPurchasePresetButtons[slot] )
			continue;
		const int presetIndex = PurchasePresetIndexForVisibleSlot( slot );
		const bool hasPreset = presetIndex >= 0 &&
			presetIndex < m_PurchasePresets.Count();
		const bool enabled = hasPreset &&
			( m_bPurchaseEditMode ||
				cash >= FoFPurchasePresetPriceForBuyZone(
					m_PurchasePresets[presetIndex], currentMode, inBuyZone ) );
		if ( m_pPurchasePresetButtons[slot]->IsEnabled() != enabled )
			m_pPurchasePresetButtons[slot]->SetEnabled( enabled );
	}

	const int visible = MAX( PurchaseVisiblePresetCount(), 1 );
	const int filteredCount = PurchaseFilteredPresetCount();
	if ( m_pPurchasePageUpButton )
	{
		const bool enabled = m_iPurchasePage > 0;
		if ( m_pPurchasePageUpButton->IsEnabled() != enabled )
			m_pPurchasePageUpButton->SetEnabled( enabled );
	}
	if ( m_pPurchasePageDownButton )
	{
		const bool enabled =
			( m_iPurchasePage + 1 ) * visible < filteredCount;
		if ( m_pPurchasePageDownButton->IsEnabled() != enabled )
			m_pPurchasePageDownButton->SetEnabled( enabled );
	}
}

void CHudFoF::LayoutPurchaseMenuControls()
{
	EnsurePurchaseMenuControls();
	if ( m_pPurchasePaintPanel )
		m_pPurchasePaintPanel->SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );

	const FoFPurchaseMenuLayout layout = FoFBuildPurchaseMenuLayout(
		m_bPurchaseQuickLayout, ScreenWidth(), ScreenHeight() );

	for ( int slot = 0; slot < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++slot )
	{
		const int column = slot % layout.columns;
		const int row = slot / layout.columns;
		m_pPurchasePresetButtons[slot]->SetBounds(
			layout.gridX + column * layout.pitchX,
			layout.gridY + row * layout.pitchY,
			layout.cardWide,
			layout.cardTall );
		m_pPurchasePresetButtons[slot]->MoveToFront();
	}

	const int arrowWide = FoFScalePurchasePixel( 15.0f, layout.scale );
	const int arrowTall = FoFScalePurchasePixel( 18.0f, layout.scale );
	const int arrowX = MIN(
		layout.gridX + layout.gridWide +
			FoFScalePurchasePixel( 5.0f, layout.scale ),
		ScreenWidth() - arrowWide );
	m_pPurchasePageUpButton->SetBounds(
		arrowX,
		layout.gridY,
		arrowWide,
		arrowTall );
	m_pPurchasePageDownButton->SetBounds(
		arrowX,
		layout.gridY + layout.gridTall - arrowTall,
		arrowWide,
		arrowTall );

	const int closeWide = FoFScalePurchasePixel( 20.0f, layout.scale );
	const int switchWide = FoFScalePurchasePixel( 80.0f, layout.scale );
	const int controlGap = FoFScalePurchasePixel( 5.0f, layout.scale );
	const int controlsWide = closeWide + controlGap + switchWide;
	const int controlsX = MIN(
		layout.gridX + layout.gridWide +
			FoFScalePurchasePixel( 20.0f, layout.scale ),
		MAX( ScreenWidth() - controlsWide, 0 ) );
	const int controlsY = layout.gridY -
		FoFScalePurchasePixel( 36.0f, layout.scale );
	m_pPurchaseCloseButton->SetBounds(
		controlsX, controlsY, closeWide, closeWide );
	m_pPurchaseSwitchButton->SetBounds(
		controlsX + closeWide + controlGap,
		controlsY,
		switchWide,
		closeWide );

	const int footerY = layout.gridY + layout.rows * layout.pitchY +
		FoFScalePurchasePixel( 3.0f, layout.scale );
	static const float filterOffsets[] = { 47.0f, 75.0f, 137.0f, 213.0f };
	static const float filterWidths[] = { 25.0f, 58.0f, 72.0f, 60.0f };
	for ( int filter = 0; filter < FOF_PURCHASE_FILTER_COUNT; ++filter )
	{
		m_pPurchaseFilterButtons[filter]->SetBounds(
			layout.gridX + FoFScalePurchasePixel(
				filterOffsets[filter], layout.scale ),
			footerY,
			FoFScalePurchasePixel( filterWidths[filter], layout.scale ),
			FoFScalePurchasePixel( 16.0f, layout.scale ) );
		m_pPurchaseFilterButtons[filter]->MoveToFront();
	}
	m_pPurchaseFileList->SetBounds(
		layout.gridX + FoFScalePurchasePixel( 280.0f, layout.scale ),
		footerY,
		FoFScalePurchasePixel( 135.0f, layout.scale ),
		FoFScalePurchasePixel( 16.0f, layout.scale ) );
	m_pPurchaseEditorButton->SetBounds(
		layout.gridX,
		footerY + FoFScalePurchasePixel( 19.0f, layout.scale ),
		layout.gridWide,
		FoFScalePurchasePixel( 20.0f, layout.scale ) );

	for ( int slot = layout.visiblePresets;
		slot < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++slot )
	{
		m_pPurchasePresetButtons[slot]->SetVisible( false );
	}
	m_pPurchasePageUpButton->MoveToFront();
	m_pPurchasePageDownButton->MoveToFront();
	m_pPurchaseCloseButton->MoveToFront();
	m_pPurchaseSwitchButton->MoveToFront();
	m_pPurchaseFileList->MoveToFront();
	m_pPurchaseEditorButton->MoveToFront();
	LayoutLoadoutEditorControls();
}

void CHudFoF::SetPurchaseMenuControlsVisible( bool visible )
{
	const bool show = visible && m_iMenuKind == FOF_MENU_PURCHASE;
	const bool showFullControls = show && !m_bPurchaseQuickLayout;
	const bool showPresetControls = show && !m_bLoadoutEditorVisible;
	if ( m_pPurchasePaintPanel )
		m_pPurchasePaintPanel->SetVisible( show );
	const int visibleSlots = PurchaseVisiblePresetCount();
	for ( int slot = 0; slot < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++slot )
	{
		if ( m_pPurchasePresetButtons[slot] )
		{
			m_pPurchasePresetButtons[slot]->SetVisible(
				showPresetControls && slot < visibleSlots &&
					PurchasePresetIndexForVisibleSlot( slot ) >= 0 );
		}
	}
	if ( m_pPurchaseCloseButton )
		m_pPurchaseCloseButton->SetVisible( show );
	if ( m_pPurchaseSwitchButton )
		m_pPurchaseSwitchButton->SetVisible( show );
	for ( int filter = 0; filter < FOF_PURCHASE_FILTER_COUNT; ++filter )
	{
		if ( m_pPurchaseFilterButtons[filter] )
		{
			m_pPurchaseFilterButtons[filter]->SetVisible( showFullControls );
			m_pPurchaseFilterButtons[filter]->SetEnabled(
				showFullControls && !m_bLoadoutEditorVisible );
		}
	}
	if ( m_pPurchaseEditorButton )
		m_pPurchaseEditorButton->SetVisible( showFullControls );
	if ( m_pPurchaseFileList )
	{
		const bool showFileList =
			showFullControls && !m_bLoadoutEditorVisible;
		m_pPurchaseFileList->SetVisible( showFileList );
		m_pPurchaseFileList->SetEnabled( showFileList );
	}
	if ( m_pPurchasePageUpButton )
		m_pPurchasePageUpButton->SetVisible( show && m_iPurchasePage > 0 );
	if ( m_pPurchasePageDownButton )
	{
		m_pPurchasePageDownButton->SetVisible(
			show && ( m_iPurchasePage + 1 ) * visibleSlots <
				PurchaseFilteredPresetCount() );
	}
	if ( show )
		UpdatePurchaseButtonStates();
	SetLoadoutEditorControlsVisible( show );
}

// Break Bad and team-play cash loadout menu presentation.

static void FoFEnsurePurchaseTexture( int &textureId, const char *material )
{
	if ( textureId >= 0 || !material || !material[0] )
		return;

	textureId = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		textureId, material, true, false );
}

static void FoFDrawPurchaseText(
	vgui::HFont font,
	const wchar_t *text,
	int x,
	int y,
	const Color &color,
	int horizontalAlignment )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( font, text, wide, tall );
	if ( horizontalAlignment == 0 )
		x -= wide / 2;
	else if ( horizontalAlignment > 0 )
		x -= wide;

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

static void FoFDrawPurchaseButtonFrame(
	vgui::Button *button,
	const Color &normalBorder,
	const Color &armedBorder,
	const Color &armedBackground )
{
	if ( !button || !button->IsVisible() )
		return;

	int x = 0;
	int y = 0;
	int wide = 0;
	int tall = 0;
	button->GetBounds( x, y, wide, tall );
	if ( button->IsArmed() )
	{
		vgui::surface()->DrawSetColor( armedBackground );
		vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
	}
	vgui::surface()->DrawSetColor(
		button->IsArmed() ? armedBorder : normalBorder );
	vgui::surface()->DrawOutlinedRect( x, y, x + wide, y + tall );
}

void CHudFoF::EnsurePurchaseMenuTextures()
{
	FoFEnsurePurchaseTexture(
		m_iPurchaseEnabledTexture, "vgui/preset_enabled" );
	FoFEnsurePurchaseTexture(
		m_iPurchaseDisabledTexture, "vgui/preset_disabled" );
	FoFEnsurePurchaseTexture(
		m_iPurchaseMouseOverTexture, "vgui/preset_mo" );
	FoFEnsurePurchaseTexture(
		m_iPurchaseArrowUpTexture, "vgui/arrow_up" );
	FoFEnsurePurchaseTexture(
		m_iPurchaseArrowDownTexture, "vgui/arrow_down" );
	FoFEnsurePurchaseTexture(
		m_iPurchaseEditTexture, "vgui/preset_edit" );

	for ( int itemId = 0;
		itemId < FOF_PURCHASE_ITEM_TEXTURE_COUNT; ++itemId )
	{
		const char *material = FoFPurchaseItemMaterial( itemId );
		if ( material )
			FoFEnsurePurchaseTexture(
				m_iPurchaseItemTextures[itemId], material );
	}
}

void CHudFoF::ApplyPurchaseMenuScheme( vgui::IScheme *scheme )
{
	if ( !scheme )
		return;

	// PresetMenu's original CPanelAnimationVar defaults resolve to FoF's
	// proportional display faces. These names reproduce the title, card and
	// footer metrics on the same 640x480 virtual canvas as the shipped panel.
	m_hPurchaseTitleFont = scheme->GetFont( "MenuFontMed", true );
	if ( m_hPurchaseTitleFont == vgui::INVALID_FONT )
		m_hPurchaseTitleFont = m_hFont;
	m_hPurchaseWarningFont = scheme->GetFont( "MenuFont", true );
	if ( m_hPurchaseWarningFont == vgui::INVALID_FONT )
		m_hPurchaseWarningFont = m_hPurchaseTitleFont;
	m_hPurchaseCashFont = scheme->GetFont( "MenuFontMed", true );
	if ( m_hPurchaseCashFont == vgui::INVALID_FONT )
		m_hPurchaseCashFont = m_hPurchaseTitleFont;
	m_hPurchasePresetFont =
		scheme->GetFont( "HudSelectionNumbers2", true );
	if ( m_hPurchasePresetFont == vgui::INVALID_FONT )
		m_hPurchasePresetFont = m_hSmallFont;
	m_hPurchasePriceFont =
		scheme->GetFont( "HudSelectionNumbers2", true );
	if ( m_hPurchasePriceFont == vgui::INVALID_FONT )
		m_hPurchasePriceFont = m_hPurchasePresetFont;
	m_hPurchaseFooterFont =
		scheme->GetFont( "HudSelectionNumbers", true );
	if ( m_hPurchaseFooterFont == vgui::INVALID_FONT )
		m_hPurchaseFooterFont = m_hSmallFont;

	if ( m_pPurchaseFileList )
	{
		// FoF leaves FileList on ComboBox's scheme-default font.  Do not
		// reuse the Typodermic footer font here: at 1024x768 the original uses
		// the resolution-aware Verdana 14 "Default" face.
		m_pPurchaseFileList->SetFont(
			scheme->GetFont( "Default", true ) );
		m_pPurchaseFileList->SetFgColor( Color( 225, 225, 218, 255 ) );
		m_pPurchaseFileList->SetBgColor( Color( 0, 0, 0, 180 ) );
		m_pPurchaseFileList->SetSelectionTextColor(
			Color( 255, 255, 255, 255 ) );
		m_pPurchaseFileList->SetSelectionBgColor(
			Color( 175, 35, 0, 255 ) );
		m_pPurchaseFileList->SetBorder(
			scheme->GetBorder( "ComboBoxBorder" ) );
	}
}

void CHudFoF::PaintPurchaseMenu()
{
	if ( !m_bMenuVisible || m_iMenuKind != FOF_MENU_PURCHASE )
		return;

	EnsurePurchaseMenuTextures();

	const FoFPurchaseMenuLayout layout = FoFBuildPurchaseMenuLayout(
		m_bPurchaseQuickLayout,
		ScreenWidth(),
		ScreenHeight() );
	const float scale = layout.scale;

	// PresetMenu darkens the world but does not draw an opaque dialog box.
	vgui::surface()->DrawSetColor( 0, 0, 0, 166 );
	vgui::surface()->DrawFilledRect( 0, 0, ScreenWidth(), ScreenHeight() );

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	const int cash = player ? (int)FoFCash( player ) : 0;
	const int currentMode = FoFHudCurrentMode();
	const int inBuyZone = FoFInBuyZone( player );
	const bool editSelection = m_bPurchaseEditMode &&
		!m_bLoadoutEditorVisible;
	const bool showBuyWarning =
		!m_bPurchaseEditMode && player && player->IsAlive() &&
		inBuyZone == 0;

	wchar_t localized[256];
	const int titleY = layout.gridY -
		FoFScalePurchasePixel( 29.0f, scale );
	if ( !layout.quick && !showBuyWarning )
	{
		const wchar_t *title = Localize(
			"#PresetMenu_Title", localized, sizeof( localized ) );
		FoFDrawPurchaseText(
			m_hPurchaseTitleFont,
			title,
			layout.gridX,
			titleY,
			Color( 225, 225, 218, 255 ),
			-1 );
	}
	if ( !layout.quick )
	{
		char cashAnsi[32];
		Q_snprintf( cashAnsi, sizeof( cashAnsi ), "$%d", cash );
		wchar_t cashWide[32];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			cashAnsi, cashWide, sizeof( cashWide ) );
		FoFDrawPurchaseText(
			m_hPurchaseCashFont,
			cashWide,
			layout.gridX + layout.gridWide -
				FoFScalePurchasePixel( 15.0f, scale ),
			titleY,
			Color( 225, 105, 0, 255 ),
			1 );
	}

	for ( int slot = 0;
		slot < FOF_PURCHASE_MAX_VISIBLE_PRESETS; ++slot )
	{
		vgui::Button *button = m_pPurchasePresetButtons[slot];
		if ( !button || !button->IsVisible() )
			continue;

		const int presetIndex = PurchasePresetIndexForVisibleSlot( slot );
		if ( presetIndex < 0 || presetIndex >= m_PurchasePresets.Count() )
			continue;

		const FoFPurchasePreset &preset = m_PurchasePresets[presetIndex];
		const int currentPrice = FoFPurchasePresetPriceForBuyZone(
			preset, currentMode, inBuyZone );
		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		button->GetBounds( x, y, wide, tall );
		const bool affordable = cash >= currentPrice;
		const bool fullyVisible = affordable || editSelection;
		DrawMenuTexture(
			editSelection ? m_iPurchaseEditTexture :
				( affordable ? m_iPurchaseEnabledTexture :
					m_iPurchaseDisabledTexture ),
			x, y, wide, tall );
		if ( fullyVisible && button->IsArmed() )
		{
			DrawMenuTexture(
				m_iPurchaseMouseOverTexture,
				x, y, wide, tall );
		}

		int iconWide = 0;
		int iconTall = 0;
		int iconStartX = 0;
		int iconStartY = 0;
		int iconPitchX = 0;
		int quickTier = 0;
		if ( layout.quick )
		{
			quickTier = 1;
			for ( int item = 0; item < preset.itemCount; ++item )
				quickTier = MAX( quickTier,
					FoFPurchaseItemTier( preset.items[item] ) );

			// The original item material fills roughly the upper half of a
			// 70x35 quick card. A 10-pixel strip visibly crushed every weapon.
			const int sideInset = FoFScalePurchasePixel( 2.0f, scale );
			const int itemGap = FoFScalePurchasePixel( 1.0f, scale );
			const int contentWide = MAX( wide - 2 * sideInset, 1 );
			iconWide = preset.itemCount == 1 ? contentWide :
				MAX( ( contentWide - itemGap ) / 2, 1 );
			iconTall = FoFScalePurchasePixel( 16.0f, scale );
			iconStartX = x + sideInset;
			iconStartY = y + FoFScalePurchasePixel( 1.0f, scale );
			iconPitchX = iconWide + itemGap;
		}
		else
		{
			wchar_t presetName[128];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				preset.name.String(), presetName, sizeof( presetName ) );
			FoFDrawPurchaseText(
				m_hPurchasePresetFont,
				presetName,
				x + FoFScalePurchasePixel( 3.0f, scale ),
				y + FoFScalePurchasePixel( 2.0f, scale ),
				fullyVisible ? Color( 230, 230, 222, 255 ) :
					Color( 150, 145, 130, 220 ),
				-1 );

			iconWide = FoFScalePurchasePixel( 48.0f, scale );
			iconTall = FoFScalePurchasePixel( 11.0f, scale );
			iconStartX = x + FoFScalePurchasePixel( 3.0f, scale );
			iconStartY = y + FoFScalePurchasePixel( 13.0f, scale );
			iconPitchX = FoFScalePurchasePixel( 49.0f, scale );
		}
		for ( int item = 0; item < preset.itemCount; ++item )
		{
			const int itemId = preset.items[item];
			if ( itemId < 0 ||
				itemId >= FOF_PURCHASE_ITEM_TEXTURE_COUNT )
			{
				continue;
			}
			DrawMenuTexture(
				m_iPurchaseItemTextures[itemId],
				iconStartX + ( item & 1 ) * iconPitchX,
				iconStartY + ( item / 2 ) * iconTall,
				iconWide,
				iconTall,
					fullyVisible ? Color( 255, 255, 255, 255 ) :
						Color( 155, 155, 155, 190 ) );
		}
		if ( layout.quick )
		{
			// The tier numeral is painted over the material, not underneath it.
			wchar_t tierText[8];
			V_snwprintf( tierText, ARRAYSIZE( tierText ), L"%d", quickTier );
			FoFDrawPurchaseText(
				m_hPurchasePresetFont,
				tierText,
				x + FoFScalePurchasePixel( 2.0f, scale ),
				y + FoFScalePurchasePixel( 1.0f, scale ),
				fullyVisible ? Color( 12, 12, 10, 255 ) :
					Color( 50, 48, 42, 220 ),
				-1 );
		}

		char priceAnsi[24];
		Q_snprintf( priceAnsi, sizeof( priceAnsi ), "$%d", currentPrice );
		wchar_t priceWide[24];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			priceAnsi, priceWide, sizeof( priceWide ) );
		const int priceTall = m_hPurchasePriceFont != vgui::INVALID_FONT ?
			vgui::surface()->GetFontTall( m_hPurchasePriceFont ) : 0;
		FoFDrawPurchaseText(
			m_hPurchasePriceFont,
			priceWide,
			x + wide - FoFScalePurchasePixel(
				layout.quick ? 2.0f : 5.0f, scale ),
			y + tall - priceTall - FoFScalePurchasePixel(
				layout.quick ? 1.0f : 2.0f, scale ),
			fullyVisible ? Color( 255, 220, 0, 255 ) :
				Color( 170, 135, 25, 220 ),
			1 );
	}

	if ( showBuyWarning )
	{
		// The original WarningLabel sits above the five-row preset grid. Its
		// opaque-looking warning state is a black panel with alpha 200; cash,
		// paging controls and the footer remain outside that panel.
		vgui::surface()->DrawSetColor( 0, 0, 0, 200 );
		vgui::surface()->DrawFilledRect(
			layout.gridX,
			layout.gridY,
			layout.gridX + layout.gridWide,
			layout.gridY + layout.gridTall );

		const wchar_t *warning = Localize(
			currentMode == 3 ? "#PresetMenu_Warning_CannotBuy" :
				"#PresetMenu_Warning_OutBuyZone",
			localized,
			sizeof( localized ) );
		DrawWrappedWide(
			warning,
			m_hPurchaseWarningFont,
			layout.gridX,
			layout.gridY + layout.pitchY,
			layout.gridWide,
			layout.pitchY * 3,
			Color( 200, 0, 0, 250 ),
			-1 );
	}

	if ( m_pPurchasePageUpButton &&
		m_pPurchasePageUpButton->IsVisible() )
	{
		int x, y, wide, tall;
		m_pPurchasePageUpButton->GetBounds( x, y, wide, tall );
		DrawMenuTexture(
			m_iPurchaseArrowUpTexture, x, y, wide, tall,
			m_pPurchasePageUpButton->IsArmed() ?
				Color( 255, 255, 255, 255 ) : Color( 230, 230, 225, 255 ) );
	}
	if ( m_pPurchasePageDownButton &&
		m_pPurchasePageDownButton->IsVisible() )
	{
		int x, y, wide, tall;
		m_pPurchasePageDownButton->GetBounds( x, y, wide, tall );
		DrawMenuTexture(
			m_iPurchaseArrowDownTexture, x, y, wide, tall,
			m_pPurchasePageDownButton->IsArmed() ?
				Color( 255, 255, 255, 255 ) : Color( 230, 230, 225, 255 ) );
	}

	FoFDrawPurchaseButtonFrame(
		m_pPurchaseCloseButton,
		Color( 220, 220, 215, 220 ),
		Color( 192, 192, 192, 180 ),
		Color( 192, 28, 0, 140 ) );
	if ( m_pPurchaseCloseButton && m_pPurchaseCloseButton->IsVisible() )
	{
		int x, y, wide, tall;
		m_pPurchaseCloseButton->GetBounds( x, y, wide, tall );
		FoFDrawPurchaseText(
			m_hPurchaseFooterFont, L"X", x + wide / 2,
			y + MAX( ( tall - vgui::surface()->GetFontTall(
				m_hPurchaseFooterFont ) ) / 2, 0 ),
			m_pPurchaseCloseButton->IsArmed() ?
				Color( 217, 212, 199, 200 ) : Color( 225, 225, 218, 255 ), 0 );
	}
	FoFDrawPurchaseButtonFrame(
		m_pPurchaseSwitchButton,
		Color( 220, 220, 215, 220 ),
		Color( 192, 192, 192, 180 ),
		Color( 192, 28, 0, 140 ) );
	if ( m_pPurchaseSwitchButton &&
		m_pPurchaseSwitchButton->IsVisible() )
	{
		int x, y, wide, tall;
		m_pPurchaseSwitchButton->GetBounds( x, y, wide, tall );
		const wchar_t *modeText = Localize(
			layout.quick ? "#FoF_Loadout_BigMode" :
				"#FoF_Loadout_QuickMode",
			localized, sizeof( localized ) );
		FoFDrawPurchaseText(
			m_hPurchaseFooterFont, modeText,
			x + RoundFloatToInt( 4.0f * scale ),
			y + MAX( ( tall - vgui::surface()->GetFontTall(
				m_hPurchaseFooterFont ) ) / 2, 0 ),
			m_pPurchaseSwitchButton->IsArmed() ?
				Color( 217, 212, 199, 200 ) : Color( 225, 225, 218, 255 ), -1 );
	}

	if ( layout.quick )
		return;

	const int footerY = layout.gridY + layout.rows * layout.pitchY +
		FoFScalePurchasePixel( 3.0f, scale );
	const wchar_t *filterLabel = Localize(
		"#PresetMenu_Filter", localized, sizeof( localized ) );
	FoFDrawPurchaseText(
		m_hPurchaseFooterFont,
		filterLabel,
		layout.gridX,
		footerY,
		Color( 210, 210, 202, 235 ),
		-1 );

	static const char *filterTokens[FOF_PURCHASE_FILTER_COUNT] =
	{
		"#PresetMenu_Filter_No",
		"#PresetMenu_Filter_Short",
		"#PresetMenu_Filter_Medium",
		"#PresetMenu_Filter_Long"
	};
	for ( int filter = 0; filter < FOF_PURCHASE_FILTER_COUNT; ++filter )
	{
		vgui::Button *button = m_pPurchaseFilterButtons[filter];
		if ( !button || !button->IsVisible() )
			continue;
		int x, y, wide, tall;
		button->GetBounds( x, y, wide, tall );
		const wchar_t *text = Localize(
			filterTokens[filter], localized, sizeof( localized ) );
		Color color( 205, 205, 197, 235 );
		if ( filter == m_iPurchaseFilter || button->IsArmed() )
			color = Color( 255, 255, 255, 255 );
		FoFDrawPurchaseText(
			m_hPurchaseFooterFont, text, x + wide / 2, y,
			color, 0 );
	}

	if ( m_pPurchaseEditorButton &&
		m_pPurchaseEditorButton->IsVisible() )
	{
		int x, y, wide, tall;
		m_pPurchaseEditorButton->GetBounds( x, y, wide, tall );
		const bool editorPressed =
			m_pPurchaseEditorButton->IsDepressed();
		const bool editorArmed =
			m_pPurchaseEditorButton->IsArmed();
		// The shipped normal state is a translucent black strip with a fine
		// light outline.  Both labels share its yellow rollover and red pressed
		// state; entering edit selection only changes the text.
		const Color editorBackground = editorPressed ?
			Color( 190, 35, 0, 225 ) :
			( editorArmed ? Color( 235, 225, 0, 225 ) :
				Color( 0, 0, 0, 118 ) );
		vgui::surface()->DrawSetColor( editorBackground );
		vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
		vgui::surface()->DrawSetColor(
			editorArmed ? Color( 192, 192, 192, 180 ) :
				Color( 215, 215, 205, 210 ) );
		vgui::surface()->DrawOutlinedRect( x, y, x + wide, y + tall );
		const wchar_t *editorText = Localize(
			m_bPurchaseEditMode ? "#PresetMenu_Edit_Button_On" :
				"#PresetMenu_Edit_Button",
			localized, sizeof( localized ) );
		FoFDrawPurchaseText(
			m_hPurchaseFooterFont,
			editorText,
			x + wide / 2,
			y + MAX( ( tall - vgui::surface()->GetFontTall(
				m_hPurchaseFooterFont ) ) / 2, 0 ),
			editorArmed ?
				Color( 217, 212, 199, 200 ) : Color( 225, 225, 218, 255 ),
			0 );
	}
}
