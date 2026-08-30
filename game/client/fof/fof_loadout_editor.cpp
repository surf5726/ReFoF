// FoF cash-loadout editor controls, state transitions and persistence.

#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_loadout_editor.h"
#include "fof/fof_purchase_menu.h"
#include "filesystem.h"
#include "KeyValues.h"
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFoFLoadoutTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CFoFLoadoutTextEntry, vgui::TextEntry );

public:
	CFoFLoadoutTextEntry( CHudFoF *owner, const char *panelName )
		: BaseClass( owner, panelName )
		, m_pOwner( owner )
	{
	}

protected:
	virtual void OnKeyCodeTyped( vgui::KeyCode code )
	{
		if ( code == KEY_ESCAPE )
		{
			// EditPreset is still a non-modal in-game panel. Escape belongs to
			// GameUI just as it does in the surrounding purchase menu.
			engine->ClientCmd_Unrestricted( "gameui_activate" );
			return;
		}
		if ( code == KEY_ENTER )
		{
			if ( m_pOwner )
				m_pOwner->OnCommand( "fof_loadout_save" );
			return;
		}
		BaseClass::OnKeyCodeTyped( code );
	}

private:
	CHudFoF *m_pOwner;
};

static void FoFConfigureLoadoutEditorButton( vgui::Button *button )
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
	// The loadout editor is fully silent: item, slot and action buttons must
	// not inherit either the rollover cue or the ordinary menu click sounds.
	button->SetArmedSound( NULL );
	button->SetDepressedSound( NULL );
	button->SetReleasedSound( NULL );
	button->SetZPos( 70 );
	button->SetVisible( false );
}

static int __cdecl FoFCompareLoadoutSourceOrder(
	const FoFPurchasePreset *left,
	const FoFPurchasePreset *right )
{
	if ( left->sourceOrder == right->sourceOrder )
		return 0;
	return left->sourceOrder < right->sourceOrder ? -1 : 1;
}

void CHudFoF::EnsureLoadoutEditorControls()
{
	for ( int index = 0; index < FoFPurchaseEditorItemCount(); ++index )
	{
		const int itemId = FoFPurchaseEditorItemId( index );
		if ( itemId < 0 || itemId >= FOF_PURCHASE_ITEM_TEXTURE_COUNT ||
			m_pLoadoutEditorItemButtons[itemId] )
		{
			continue;
		}

		char name[48];
		char command[48];
		Q_snprintf( name, sizeof( name ), "FoFLoadoutEditorItem%d", itemId );
		Q_snprintf( command, sizeof( command ), "fof_loadout_item_%d", itemId );
		m_pLoadoutEditorItemButtons[itemId] = new vgui::Button(
			this, name, "", this, command );
		FoFConfigureLoadoutEditorButton(
			m_pLoadoutEditorItemButtons[itemId] );
	}

	for ( int slot = 0; slot < FOF_PURCHASE_MAX_ITEMS; ++slot )
	{
		if ( m_pLoadoutEditorSlotButtons[slot] )
			continue;

		char name[48];
		char command[48];
		Q_snprintf( name, sizeof( name ), "FoFLoadoutEditorSlot%d", slot );
		Q_snprintf( command, sizeof( command ), "fof_loadout_slot_%d", slot );
		m_pLoadoutEditorSlotButtons[slot] = new vgui::Button(
			this, name, "", this, command );
		FoFConfigureLoadoutEditorButton(
			m_pLoadoutEditorSlotButtons[slot] );
	}

	if ( !m_pLoadoutEditorName )
	{
		m_pLoadoutEditorName = new CFoFLoadoutTextEntry(
			this, "FoFLoadoutEditorName" );
		m_pLoadoutEditorName->SetMaximumCharCount( 63 );
		m_pLoadoutEditorName->SetAllowNonAsciiCharacters( true );
		m_pLoadoutEditorName->SetHorizontalScrolling( true );
		m_pLoadoutEditorName->SetMouseInputEnabled( true );
		m_pLoadoutEditorName->SetKeyBoardInputEnabled( true );
		m_pLoadoutEditorName->SetZPos( 70 );
		m_pLoadoutEditorName->SetVisible( false );
	}
	if ( !m_pLoadoutEditorFilename )
	{
		m_pLoadoutEditorFilename = new CFoFLoadoutTextEntry(
			this, "FoFLoadoutEditorFilename" );
		m_pLoadoutEditorFilename->SetMaximumCharCount( 63 );
		m_pLoadoutEditorFilename->SetAllowNonAsciiCharacters( true );
		m_pLoadoutEditorFilename->SetHorizontalScrolling( true );
		m_pLoadoutEditorFilename->SetMouseInputEnabled( true );
		m_pLoadoutEditorFilename->SetKeyBoardInputEnabled( true );
		m_pLoadoutEditorFilename->SetZPos( 70 );
		m_pLoadoutEditorFilename->SetVisible( false );
	}

	if ( !m_pLoadoutEditorCancelButton )
	{
		m_pLoadoutEditorCancelButton = new vgui::Button(
			this, "FoFLoadoutEditorCancel", "", this,
			"fof_loadout_cancel" );
		FoFConfigureLoadoutEditorButton( m_pLoadoutEditorCancelButton );
	}
	if ( !m_pLoadoutEditorDeleteButton )
	{
		m_pLoadoutEditorDeleteButton = new vgui::Button(
			this, "FoFLoadoutEditorDelete", "", this,
			"fof_loadout_delete" );
		FoFConfigureLoadoutEditorButton( m_pLoadoutEditorDeleteButton );
	}
	if ( !m_pLoadoutEditorSaveButton )
	{
		m_pLoadoutEditorSaveButton = new vgui::Button(
			this, "FoFLoadoutEditorSave", "", this,
			"fof_loadout_save" );
		FoFConfigureLoadoutEditorButton( m_pLoadoutEditorSaveButton );
	}
}

void CHudFoF::ApplyLoadoutEditorScheme( vgui::IScheme *scheme )
{
	if ( !scheme )
		return;

	// EditPreset uses the ordinary VGUI control font.  At 768p this is the
	// 14-pixel, heavy Verdana face from ClientScheme; DefaultFoF is only 10
	// pixels tall and made the item column look compressed next to the
	// shipped editor.
	m_hLoadoutEditorItemFont = scheme->GetFont( "Default", true );
	if ( m_hLoadoutEditorItemFont == vgui::INVALID_FONT )
		m_hLoadoutEditorItemFont = m_hFont;
	m_hLoadoutEditorControlFont = scheme->GetFont( "Default", true );
	if ( m_hLoadoutEditorControlFont == vgui::INVALID_FONT )
		m_hLoadoutEditorControlFont = m_hFont;

	EnsureLoadoutEditorControls();
	if ( m_pLoadoutEditorName )
	{
		m_pLoadoutEditorName->SetFont( m_hLoadoutEditorControlFont );
		m_pLoadoutEditorName->SetFgColor( Color( 230, 230, 220, 255 ) );
		m_pLoadoutEditorName->SetBgColor( Color( 45, 37, 0, 235 ) );
		m_pLoadoutEditorName->SetSelectionTextColor(
			Color( 255, 255, 255, 255 ) );
		m_pLoadoutEditorName->SetSelectionBgColor(
			Color( 150, 55, 0, 255 ) );
		m_pLoadoutEditorName->SetBorder(
			scheme->GetBorder( "ButtonDepressedBorder" ) );
	}
	if ( m_pLoadoutEditorFilename )
	{
		m_pLoadoutEditorFilename->SetFont( m_hLoadoutEditorControlFont );
		m_pLoadoutEditorFilename->SetFgColor( Color( 230, 230, 220, 255 ) );
		m_pLoadoutEditorFilename->SetBgColor( Color( 45, 37, 0, 235 ) );
		m_pLoadoutEditorFilename->SetSelectionTextColor(
			Color( 255, 255, 255, 255 ) );
		m_pLoadoutEditorFilename->SetSelectionBgColor(
			Color( 150, 55, 0, 255 ) );
		m_pLoadoutEditorFilename->SetBorder(
			scheme->GetBorder( "ButtonDepressedBorder" ) );
	}
}

void CHudFoF::LayoutLoadoutEditorControls()
{
	EnsureLoadoutEditorControls();
	if ( m_pLoadoutEditorPaintPanel )
		m_pLoadoutEditorPaintPanel->SetBounds(
			0, 0, ScreenWidth(), ScreenHeight() );

	const FoFPurchaseMenuLayout layout = FoFBuildPurchaseMenuLayout(
		false, ScreenWidth(), ScreenHeight() );
	const float scale = layout.scale;
	const int itemCount = MAX( FoFPurchaseEditorItemCount(), 1 );
	const int listX = layout.gridX + FoFScalePurchasePixel( 106.0f, scale );
	const int listY = layout.gridY + FoFScalePurchasePixel( 5.0f, scale );
	// EditPreset leaves a full 12 virtual-pixel gutter before the name field.
	const int listWide = FoFScalePurchasePixel( 82.0f, scale );
	const int listTall = layout.gridTall -
		FoFScalePurchasePixel( 10.0f, scale );
	const int rowTall = MAX( listTall / itemCount, 1 );
	for ( int index = 0; index < FoFPurchaseEditorItemCount(); ++index )
	{
		const int itemId = FoFPurchaseEditorItemId( index );
		vgui::Button *button = itemId >= 0 &&
			itemId < FOF_PURCHASE_ITEM_TEXTURE_COUNT ?
			m_pLoadoutEditorItemButtons[itemId] : NULL;
		if ( !button )
			continue;
		button->SetBounds(
			listX,
			listY + index * rowTall,
			listWide,
			rowTall );
	}

	const int formX = layout.gridX +
		FoFScalePurchasePixel( 207.0f, scale );
	const int formWide = FoFScalePurchasePixel( 104.0f, scale );
	if ( m_pLoadoutEditorName )
	{
		m_pLoadoutEditorName->SetBounds(
			formX,
			layout.gridY + FoFScalePurchasePixel( 5.0f, scale ),
			formWide,
			FoFScalePurchasePixel( 20.0f, scale ) );
	}
	if ( m_pLoadoutEditorFilename )
	{
		m_pLoadoutEditorFilename->SetBounds(
			formX,
			layout.gridY + FoFScalePurchasePixel( 151.0f, scale ),
			formWide,
			FoFScalePurchasePixel( 15.0f, scale ) );
	}

	for ( int slot = 0; slot < FOF_PURCHASE_MAX_ITEMS; ++slot )
	{
		m_pLoadoutEditorSlotButtons[slot]->SetBounds(
			formX,
			layout.gridY + FoFScalePurchasePixel(
				30.0f + slot * 16.0f, scale ),
			formWide,
			FoFScalePurchasePixel( 15.0f, scale ) );
	}

	const int actionY = layout.gridY +
		FoFScalePurchasePixel( 134.0f, scale );
	const int actionGap = FoFScalePurchasePixel( 2.0f, scale );
	const int actionWide = ( formWide - actionGap ) / 2;
	m_pLoadoutEditorCancelButton->SetBounds(
		formX, actionY, actionWide,
		FoFScalePurchasePixel( 15.0f, scale ) );
	m_pLoadoutEditorDeleteButton->SetBounds(
		formX + actionWide + actionGap, actionY, actionWide,
		FoFScalePurchasePixel( 15.0f, scale ) );
	m_pLoadoutEditorSaveButton->SetBounds(
		formX,
		layout.gridY + FoFScalePurchasePixel( 168.0f, scale ),
		formWide,
		FoFScalePurchasePixel( 17.0f, scale ) );
}

void CHudFoF::SetLoadoutEditorControlsVisible( bool visible )
{
	const bool show = visible && m_bMenuVisible &&
		m_iMenuKind == FOF_MENU_PURCHASE && m_bLoadoutEditorVisible;
	if ( m_pLoadoutEditorPaintPanel )
		m_pLoadoutEditorPaintPanel->SetVisible( show );

	for ( int index = 0; index < FoFPurchaseEditorItemCount(); ++index )
	{
		const int itemId = FoFPurchaseEditorItemId( index );
		if ( itemId >= 0 && itemId < FOF_PURCHASE_ITEM_TEXTURE_COUNT &&
			m_pLoadoutEditorItemButtons[itemId] )
		{
			m_pLoadoutEditorItemButtons[itemId]->SetVisible( show );
			m_pLoadoutEditorItemButtons[itemId]->SetEnabled(
				show && m_iLoadoutEditorItemCount < FOF_PURCHASE_MAX_ITEMS );
		}
	}
	for ( int slot = 0; slot < FOF_PURCHASE_MAX_ITEMS; ++slot )
	{
		if ( m_pLoadoutEditorSlotButtons[slot] )
		{
			m_pLoadoutEditorSlotButtons[slot]->SetVisible( show );
			m_pLoadoutEditorSlotButtons[slot]->SetEnabled(
				show && slot < m_iLoadoutEditorItemCount );
		}
	}
	if ( m_pLoadoutEditorName )
		m_pLoadoutEditorName->SetVisible( show );
	if ( m_pLoadoutEditorFilename )
		m_pLoadoutEditorFilename->SetVisible( show );
	if ( m_pLoadoutEditorCancelButton )
		m_pLoadoutEditorCancelButton->SetVisible( show );
	if ( m_pLoadoutEditorDeleteButton )
	{
		m_pLoadoutEditorDeleteButton->SetVisible( show );
		m_pLoadoutEditorDeleteButton->SetEnabled(
			show && m_iLoadoutEditorSourceOrder >= 0 );
	}
	if ( m_pLoadoutEditorSaveButton )
	{
		m_pLoadoutEditorSaveButton->SetVisible( show );
		m_pLoadoutEditorSaveButton->SetEnabled(
			show && m_iLoadoutEditorItemCount > 0 );
	}

	if ( show )
	{
		if ( m_pLoadoutEditorPaintPanel )
			m_pLoadoutEditorPaintPanel->MoveToFront();
		for ( int index = 0; index < FoFPurchaseEditorItemCount(); ++index )
		{
			const int itemId = FoFPurchaseEditorItemId( index );
			if ( itemId >= 0 && itemId < FOF_PURCHASE_ITEM_TEXTURE_COUNT &&
				m_pLoadoutEditorItemButtons[itemId] )
			{
				m_pLoadoutEditorItemButtons[itemId]->MoveToFront();
			}
		}
		for ( int slot = 0; slot < FOF_PURCHASE_MAX_ITEMS; ++slot )
			m_pLoadoutEditorSlotButtons[slot]->MoveToFront();
		m_pLoadoutEditorName->MoveToFront();
		m_pLoadoutEditorFilename->MoveToFront();
		m_pLoadoutEditorCancelButton->MoveToFront();
		m_pLoadoutEditorDeleteButton->MoveToFront();
		m_pLoadoutEditorSaveButton->MoveToFront();
	}
}

void CHudFoF::ToggleLoadoutEditorMode()
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE ||
		m_bPurchaseQuickLayout )
		return;

	if ( m_bLoadoutEditorVisible )
	{
		CloseLoadoutEditor();
		return;
	}

	m_bPurchaseEditMode = !m_bPurchaseEditMode;
	SetPurchaseMenuControlsVisible( true );
}

void CHudFoF::OpenLoadoutEditor( int presetIndex )
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE ||
		presetIndex < 0 || presetIndex >= m_PurchasePresets.Count() )
	{
		return;
	}

	const FoFPurchasePreset &preset = m_PurchasePresets[presetIndex];
	m_bPurchaseEditMode = true;
	m_bLoadoutEditorVisible = true;
	m_iLoadoutEditorSourceOrder = preset.sourceOrder;
	m_iLoadoutEditorItemCount = preset.itemCount;
	Q_memset( m_LoadoutEditorItems, 0xff,
		sizeof( m_LoadoutEditorItems ) );
	for ( int item = 0; item < preset.itemCount; ++item )
		m_LoadoutEditorItems[item] = preset.items[item];

	EnsureLoadoutEditorControls();
	m_pLoadoutEditorName->SetText( preset.name.String() );
	m_pLoadoutEditorName->GotoTextEnd();
	ConVar *loadoutFilename = cvar ?
		cvar->FindVar( "fof_loadout_filename" ) : NULL;
	const char *filename = loadoutFilename ?
		loadoutFilename->GetString() : NULL;
	if ( !filename || !filename[0] || !Q_stricmp( filename, "default.txt" ) ||
		!Q_stricmp( filename, "auto.txt" ) )
	{
		filename = "my_loadouts.txt";
	}
	m_pLoadoutEditorFilename->SetText( filename );
	m_pLoadoutEditorFilename->GotoTextEnd();
	LayoutLoadoutEditorControls();
	SetPurchaseMenuControlsVisible( true );
	SetLoadoutEditorControlsVisible( true );
	m_pLoadoutEditorName->RequestFocus();
}

void CHudFoF::OpenNewLoadoutEditor()
{
	if ( m_iMenuKind != FOF_MENU_PURCHASE ||
		m_bPurchaseQuickLayout || !m_bPurchaseEditMode )
	{
		return;
	}

	m_bLoadoutEditorVisible = true;
	m_iLoadoutEditorSourceOrder = -1;
	m_iLoadoutEditorItemCount = 0;
	Q_memset( m_LoadoutEditorItems, 0xff,
		sizeof( m_LoadoutEditorItems ) );
	EnsureLoadoutEditorControls();
	m_pLoadoutEditorName->SetText( "" );
	m_pLoadoutEditorName->GotoTextEnd();
	ConVar *loadoutFilename = cvar ?
		cvar->FindVar( "fof_loadout_filename" ) : NULL;
	const char *filename = loadoutFilename ?
		loadoutFilename->GetString() : NULL;
	if ( !filename || !filename[0] || !Q_stricmp( filename, "default.txt" ) ||
		!Q_stricmp( filename, "auto.txt" ) )
	{
		filename = "my_loadouts.txt";
	}
	m_pLoadoutEditorFilename->SetText( filename );
	m_pLoadoutEditorFilename->GotoTextEnd();
	LayoutLoadoutEditorControls();
	SetPurchaseMenuControlsVisible( true );
	SetLoadoutEditorControlsVisible( true );
	m_pLoadoutEditorName->RequestFocus();
}

void CHudFoF::CloseLoadoutEditor()
{
	SetLoadoutEditorControlsVisible( false );
	m_bLoadoutEditorVisible = false;
	m_iLoadoutEditorSourceOrder = -1;
	m_iLoadoutEditorItemCount = 0;
	Q_memset( m_LoadoutEditorItems, 0xff,
		sizeof( m_LoadoutEditorItems ) );
	if ( m_pLoadoutEditorName )
		m_pLoadoutEditorName->SetText( "" );
	if ( m_pLoadoutEditorFilename )
		m_pLoadoutEditorFilename->SetText( "" );
	SetPurchaseMenuControlsVisible( true );
	BeginLocalMenuInput();
}

void CHudFoF::AddLoadoutEditorItem( int itemId )
{
	if ( !m_bLoadoutEditorVisible ||
		FoFPurchaseItemBasePrice( itemId ) < 0 ||
		m_iLoadoutEditorItemCount >= FOF_PURCHASE_MAX_ITEMS )
	{
		return;
	}

	m_LoadoutEditorItems[m_iLoadoutEditorItemCount++] = itemId;
	SetLoadoutEditorControlsVisible( true );
}

void CHudFoF::RemoveLoadoutEditorItem( int slot )
{
	if ( !m_bLoadoutEditorVisible || slot < 0 ||
		slot >= m_iLoadoutEditorItemCount )
	{
		return;
	}

	for ( int item = slot; item + 1 < m_iLoadoutEditorItemCount; ++item )
		m_LoadoutEditorItems[item] = m_LoadoutEditorItems[item + 1];
	--m_iLoadoutEditorItemCount;
	m_LoadoutEditorItems[m_iLoadoutEditorItemCount] = -1;
	SetLoadoutEditorControlsVisible( true );
}

int CHudFoF::FindLoadoutEditorPreset() const
{
	if ( m_iLoadoutEditorSourceOrder < 0 )
		return -1;
	for ( int preset = 0; preset < m_PurchasePresets.Count(); ++preset )
	{
		if ( m_PurchasePresets[preset].sourceOrder ==
			m_iLoadoutEditorSourceOrder )
		{
			return preset;
		}
	}
	return -1;
}

int CHudFoF::LoadoutEditorPrice() const
{
	FoFPurchasePreset preset;
	preset.itemCount = m_iLoadoutEditorItemCount;
	for ( int item = 0; item < preset.itemCount; ++item )
		preset.items[item] = m_LoadoutEditorItems[item];
	return FoFPurchasePresetPriceForBuyZone( preset, 2, 0 );
}

bool CHudFoF::SaveLoadoutEditorPresets()
{
	char filename[128];
	if ( m_pLoadoutEditorFilename )
		m_pLoadoutEditorFilename->GetText( filename, sizeof( filename ) );
	else
		filename[0] = '\0';
	if ( !filename[0] )
		Q_strncpy( filename, "my_loadouts.txt", sizeof( filename ) );
	for ( char *character = filename; *character; ++character )
	{
		if ( *character == '/' || *character == '\\' || *character == ':' ||
			*character == '*' || *character == '?' || *character == '"' ||
			*character == '<' || *character == '>' || *character == '|' ||
			(unsigned char)*character < 32 )
		{
			*character = '_';
		}
	}
	const char *extension = V_GetFileExtension( filename );
	if ( !extension || Q_stricmp( extension, "txt" ) )
		Q_strncat( filename, ".txt", sizeof( filename ) );
	m_pLoadoutEditorFilename->SetText( filename );

	CUtlVector< FoFPurchasePreset > ordered;
	for ( int preset = 0; preset < m_PurchasePresets.Count(); ++preset )
		ordered.AddToTail( m_PurchasePresets[preset] );
	ordered.Sort( FoFCompareLoadoutSourceOrder );

	KeyValues *root = new KeyValues( "List" );
	for ( int preset = 0; preset < ordered.Count(); ++preset )
	{
		if ( ordered[preset].itemCount <= 0 )
			continue;

		KeyValues *entry = new KeyValues( "preset" );
		if ( !ordered[preset].name.IsEmpty() )
			entry->SetString( "name", ordered[preset].name.String() );

		char itemList[128];
		Q_strncpy( itemList, ",", sizeof( itemList ) );
		for ( int item = 0; item < ordered[preset].itemCount; ++item )
		{
			char itemText[16];
			Q_snprintf( itemText, sizeof( itemText ), "%d,",
				ordered[preset].items[item] );
			Q_strncat( itemList, itemText, sizeof( itemList ) );
		}
		entry->SetString( "item_list", itemList );
		root->AddSubKey( entry );
	}

	filesystem->CreateDirHierarchy( "fof_scripts/loadouts", "MOD" );
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ),
		"fof_scripts/loadouts/%s", filename );
	const bool saved = root->SaveToFile(
		filesystem,
		path,
		"MOD" );
	root->deleteThis();
	if ( !saved )
	{
		Warning( "[FoF] unable to save %s\n", path );
		return false;
	}

	ConVar *loadoutFilenameVar = cvar ?
		cvar->FindVar( "fof_loadout_filename" ) : NULL;
	if ( loadoutFilenameVar )
		loadoutFilenameVar->SetValue( filename );
	return true;
}

void CHudFoF::SaveLoadoutEditor()
{
	if ( !m_bLoadoutEditorVisible || m_iLoadoutEditorItemCount <= 0 )
		return;

	int presetIndex = FindLoadoutEditorPreset();
	if ( presetIndex < 0 )
	{
		int sourceOrder = -1;
		for ( int preset = 0; preset < m_PurchasePresets.Count(); ++preset )
			sourceOrder = MAX( sourceOrder,
				m_PurchasePresets[preset].sourceOrder );
		FoFPurchasePreset preset;
		preset.sourceOrder = sourceOrder + 1;
		presetIndex = m_PurchasePresets.AddToTail( preset );
		m_iLoadoutEditorSourceOrder = preset.sourceOrder;
	}

	FoFPurchasePreset &preset = m_PurchasePresets[presetIndex];
	char name[256];
	m_pLoadoutEditorName->GetText( name, sizeof( name ) );
	preset.name = name;
	preset.itemCount = m_iLoadoutEditorItemCount;
	Q_memset( preset.items, 0xff, sizeof( preset.items ) );
	for ( int item = 0; item < preset.itemCount; ++item )
		preset.items[item] = m_LoadoutEditorItems[item];
	preset.price = FoFPurchasePresetPriceForBuyZone( preset, 2, 0 );

	if ( !SaveLoadoutEditorPresets() )
		return;
	PopulatePurchaseLoadoutFiles();
	LoadPurchasePresets();
	CloseLoadoutEditor();
}

void CHudFoF::DeleteLoadoutEditor()
{
	if ( !m_bLoadoutEditorVisible )
		return;
	const int presetIndex = FindLoadoutEditorPreset();
	if ( presetIndex < 0 )
		return;

	m_PurchasePresets.Remove( presetIndex );
	if ( !SaveLoadoutEditorPresets() )
		return;
	PopulatePurchaseLoadoutFiles();
	LoadPurchasePresets();
	CloseLoadoutEditor();
}

// FoF cash-loadout editor presentation.

static void FoFDrawLoadoutEditorText(
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

static void FoFFitLoadoutEditorText(
	vgui::HFont font,
	const wchar_t *text,
	int maxWide,
	wchar_t *output,
	int outputBytes )
{
	if ( !output || outputBytes < (int)sizeof( wchar_t ) )
		return;

	output[0] = L'\0';
	if ( !text )
		return;

	V_wcsncpy( output, text, outputBytes );
	if ( font == vgui::INVALID_FONT || maxWide <= 0 )
		return;

	int textWide = 0;
	int textTall = 0;
	vgui::surface()->GetTextSize( font, output, textWide, textTall );
	if ( textWide <= maxWide )
		return;

	const int outputChars = outputBytes / sizeof( wchar_t );
	int keep = MIN( V_wcslen( output ), outputChars - 4 );
	while ( keep >= 0 )
	{
		output[keep] = L'.';
		output[keep + 1] = L'.';
		output[keep + 2] = L'.';
		output[keep + 3] = L'\0';
		vgui::surface()->GetTextSize( font, output, textWide, textTall );
		if ( textWide <= maxWide )
			return;
		--keep;
	}

	output[0] = L'\0';
}

static void FoFDrawLoadoutEditorRoundedBox(
	int x,
	int y,
	int wide,
	int tall,
	int radius,
	const Color &color )
{
	radius = clamp( radius, 1, MIN( wide, tall ) / 2 );
	vgui::surface()->DrawSetColor( color );
	vgui::surface()->DrawFilledRect(
		x, y + radius, x + wide, y + tall - radius );
	vgui::surface()->DrawFilledRect(
		x + radius, y, x + wide - radius, y + tall );

	for ( int row = 0; row < radius; ++row )
	{
		const int inset = MAX( radius - row * 2 - 1, 0 );
		vgui::surface()->DrawFilledRect(
			x + inset, y + row,
			x + wide - inset, y + row + 1 );
		vgui::surface()->DrawFilledRect(
			x + inset, y + tall - row - 1,
			x + wide - inset, y + tall - row );
	}
}

static void FoFDrawLoadoutEditorButton(
	vgui::Button *button,
	const wchar_t *text,
	vgui::HFont font,
	const Color &normalBackground,
	const Color &armedBackground )
{
	if ( !button || !button->IsVisible() )
		return;

	int x = 0;
	int y = 0;
	int wide = 0;
	int tall = 0;
	button->GetBounds( x, y, wide, tall );
	vgui::surface()->DrawSetColor(
		button->IsArmed() ? armedBackground : normalBackground );
	vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
	vgui::surface()->DrawSetColor( 210, 205, 185, 225 );
	vgui::surface()->DrawOutlinedRect( x, y, x + wide, y + tall );

	const int fontTall = font != vgui::INVALID_FONT ?
		vgui::surface()->GetFontTall( font ) : 0;
	FoFDrawLoadoutEditorText(
		font,
		text,
		x + FoFScalePurchasePixel( 2.0f,
			MAX( (float)ScreenHeight() / 480.0f, 0.1f ) ),
		y + MAX( ( tall - fontTall ) / 2, 0 ),
		Color( 235, 232, 218, 255 ),
		-1 );
}

CFoFLoadoutEditorPaintPanel::CFoFLoadoutEditorPaintPanel(
	CHudFoF *owner )
	: BaseClass( owner, "FoFLoadoutEditorPaintLayer" )
	, m_pOwner( owner )
{
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetZPos( 50 );
	SetVisible( false );
}

void CFoFLoadoutEditorPaintPanel::Paint()
{
	if ( m_pOwner )
		m_pOwner->PaintLoadoutEditor();
}

void CHudFoF::PaintLoadoutEditor()
{
	if ( !m_bMenuVisible || m_iMenuKind != FOF_MENU_PURCHASE ||
		!m_bLoadoutEditorVisible )
	{
		return;
	}

	const FoFPurchaseMenuLayout layout = FoFBuildPurchaseMenuLayout(
		false, ScreenWidth(), ScreenHeight() );
	const float scale = layout.scale;
	FoFDrawLoadoutEditorRoundedBox(
		layout.gridX,
		layout.gridY,
		layout.gridWide,
		layout.gridTall,
		FoFScalePurchasePixel( 4.0f, scale ),
		Color( 111, 87, 0, 244 ) );

	wchar_t localized[256];
	for ( int index = 0; index < FoFPurchaseEditorItemCount(); ++index )
	{
		const int itemId = FoFPurchaseEditorItemId( index );
		vgui::Button *button = itemId >= 0 &&
			itemId < FOF_PURCHASE_ITEM_TEXTURE_COUNT ?
			m_pLoadoutEditorItemButtons[itemId] : NULL;
		if ( !button || !button->IsVisible() )
			continue;

		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		button->GetBounds( x, y, wide, tall );
		if ( button->IsArmed() )
		{
			vgui::surface()->DrawSetColor( 160, 52, 0, 210 );
			vgui::surface()->DrawFilledRect( x, y, x + wide, y + tall );
		}

		const char *token = FoFPurchaseItemNameToken( itemId );
		const wchar_t *name = Localize(
			token ? token : FoFPurchaseItemFallbackName( itemId ),
			localized,
			sizeof( localized ) );
		wchar_t fittedName[256];
		const int textInset = FoFScalePurchasePixel( 2.0f, scale );
		FoFFitLoadoutEditorText(
			m_hLoadoutEditorItemFont,
			name,
			wide - textInset * 2,
			fittedName,
			sizeof( fittedName ) );
		const int fontTall = m_hLoadoutEditorItemFont != vgui::INVALID_FONT ?
			vgui::surface()->GetFontTall( m_hLoadoutEditorItemFont ) : 0;
		FoFDrawLoadoutEditorText(
			m_hLoadoutEditorItemFont,
			fittedName,
			x + textInset,
			y + MAX( ( tall - fontTall ) / 2, 0 ),
			button->IsArmed() ? Color( 255, 235, 180, 255 ) :
				Color( 232, 229, 216, 255 ),
			-1 );
	}

	for ( int slot = 0; slot < FOF_PURCHASE_MAX_ITEMS; ++slot )
	{
		vgui::Button *button = m_pLoadoutEditorSlotButtons[slot];
		if ( !button || !button->IsVisible() )
			continue;

		const wchar_t *name = L"";
		if ( slot < m_iLoadoutEditorItemCount )
		{
			const int itemId = m_LoadoutEditorItems[slot];
			const char *token = FoFPurchaseItemNameToken( itemId );
			name = Localize(
				token ? token : FoFPurchaseItemFallbackName( itemId ),
				localized,
				sizeof( localized ) );
		}
		FoFDrawLoadoutEditorButton(
			button,
			name,
			m_hLoadoutEditorControlFont,
			Color( 69, 55, 0, 225 ),
			Color( 156, 48, 0, 225 ) );
	}

	// The shipped editor leaves the price area empty for a new, empty
	// loadout.  Drawing "$0" here made the blank slot column look like an
	// unexplained seventh control.
	if ( m_iLoadoutEditorItemCount > 0 )
	{
		char priceAnsi[32];
		Q_snprintf( priceAnsi, sizeof( priceAnsi ), "$%d",
			LoadoutEditorPrice() );
		wchar_t priceWide[32];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			priceAnsi, priceWide, sizeof( priceWide ) );
		FoFDrawLoadoutEditorText(
			m_hLoadoutEditorControlFont,
			priceWide,
			layout.gridX + FoFScalePurchasePixel( 325.0f, scale ),
			layout.gridY + FoFScalePurchasePixel( 30.0f, scale ),
			Color( 235, 230, 210, 255 ),
			-1 );
	}

	const wchar_t *cancelText = Localize(
		"#GameUI_Cancel", localized, sizeof( localized ) );
	FoFDrawLoadoutEditorButton(
		m_pLoadoutEditorCancelButton,
		cancelText,
		m_hLoadoutEditorControlFont,
		Color( 83, 65, 0, 235 ),
		Color( 172, 43, 0, 235 ) );
	const wchar_t *deleteText = Localize(
		"#FoF_Delete", localized, sizeof( localized ) );
	FoFDrawLoadoutEditorButton(
		m_pLoadoutEditorDeleteButton,
		deleteText,
		m_hLoadoutEditorControlFont,
		Color( 83, 65, 0, 235 ),
		Color( 172, 43, 0, 235 ) );

	const wchar_t *saveText = Localize(
		"#FoF_Save", localized, sizeof( localized ) );
	FoFDrawLoadoutEditorButton(
		m_pLoadoutEditorSaveButton,
		saveText,
		m_hLoadoutEditorControlFont,
		Color( 83, 65, 0, 235 ),
		Color( 172, 43, 0, 235 ) );
}
