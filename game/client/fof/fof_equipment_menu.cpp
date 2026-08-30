// Shootout equipment catalogue, selection state and presentation.

#include "cbase.h"
#include "cdll_util.h"
#include "fof/fof_equipment_menu.h"
#include "fof/fof_hud.h"
#include "fof/fof_hints.h"
#include "fof/fof_item_catalog.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The original deathmatch equipment panel persists exactly six item slots.
// Keep these public cvar names so existing FoF configurations retain their
// selected loadout and accepted changes survive a restart.
static ConVar fof_gear_0( "fof_gear_0", "2", FCVAR_ARCHIVE );
static ConVar fof_gear_1( "fof_gear_1", "1", FCVAR_ARCHIVE );
static ConVar fof_gear_2( "fof_gear_2", "20", FCVAR_ARCHIVE );
static ConVar fof_gear_3( "fof_gear_3", "43", FCVAR_ARCHIVE );
static ConVar fof_gear_4( "fof_gear_4", "-1", FCVAR_ARCHIVE );
static ConVar fof_gear_5( "fof_gear_5", "-1", FCVAR_ARCHIVE );

struct FoFEquipmentItemDef
{
	const char *label;
	const char *material;
	const char *helpToken;
	int itemId;
};

// Original Shootout order: 9 primary choices, 9 secondary/perk choices,
// then the 4 mutually-exclusive handgun skills.  Do not add arbitrary
// weapons here: the original server only accepts this deathmatch catalogue.
static const FoFEquipmentItemDef g_FoFEquipmentItems[] =
{
	{ "#ItemVolcanic", "vgui/volcanic",       NULL,                  28 },
	{ "#Item2",        "vgui/coltnavy",       NULL,                   2 },
	{ "#Item19",       "vgui/axe",            NULL,                  19 },
	{ "#Item11",       "vgui/bow",            NULL,                  11 },
	{ "#Item13b",      "vgui/sawed_shotgun",  NULL,                  13 },
	{ "#Item3",        "vgui/hammerless",      NULL,                  22 },
	{ "#Rem_Army",     "vgui/nma",             NULL,                  30 },
	{ "#MaresLeg",     "vgui/maresleg",        NULL,                  33 },
	{ "#Item5",        "vgui/carbine",         NULL,                   5 },

	{ "#Item14",       "vgui/gun_throw",       "#Help_WThrow",       14 },
	{ "#Item23e",      "vgui/walljump",        "#Help_Walljump",     15 },
	{ "#Item23d",      "vgui/slide",           "#Help_Slide",        23 },
	{ "#Item25",       "vgui/heavyload",       "#Help_HeavyLoad",    25 },
	{ "#Item34b",      "vgui/brass_knuckles",  "#Help_Knuckles",     34 },
	{ "#Item1b",       "vgui/knife",           "#Help_Knife",         1 },
	{ "#Item34",       "vgui/boots",           "#Help_Boots",        20 },
	{ "#Item3c",       "vgui/deringer",        NULL,                   3 },
	{ "#Item0",        "vgui/dynamite",        "#Help_Dynamite",      7 },

	{ "#Item41",       "vgui/accuracy_right",  "#AccSkillLabel_1b",  41 },
	{ "#Item42",       "vgui/accuracy_left",   "#AccSkillLabel_2",   42 },
	{ "#Item44",       "vgui/accuracy_fan",    "#AccSkillLabel_3",   44 },
	{ "#Item43",       "vgui/accuracy_ambi",   "#AccSkillLabel_0",   43 }
};

static const FoFEquipmentItemDef &FoFEquipmentItem(
	int catalogueIndex )
{
	Assert( catalogueIndex >= 0 &&
		catalogueIndex < ARRAYSIZE( g_FoFEquipmentItems ) );
	const int safeIndex = catalogueIndex < 0 ? 0 :
		MIN( catalogueIndex, ARRAYSIZE( g_FoFEquipmentItems ) - 1 );
	return g_FoFEquipmentItems[safeIndex];
}

int FoFEquipmentItemCount()
{
	return ARRAYSIZE( g_FoFEquipmentItems );
}

const char *FoFEquipmentItemLabel( int catalogueIndex )
{
	return FoFEquipmentItem( catalogueIndex ).label;
}

const char *FoFEquipmentItemMaterial( int catalogueIndex )
{
	return FoFEquipmentItem( catalogueIndex ).material;
}

const char *FoFEquipmentItemBaseTexture( int catalogueIndex )
{
	const FoFEquipmentItemDef &item = FoFEquipmentItem( catalogueIndex );
	// accuracy_fan.vmt is the one catalogue material whose VMT and VTF names
	// differ.  The translucent menu wrapper needs the VTF named by
	// $basetexture, whereas ImagePanel and DrawSetTextureFile need the VMT.
	return item.itemId == 44 ? "vgui/accuracy_fanning" : item.material;
}

const char *FoFEquipmentItemHelpToken( int catalogueIndex )
{
	return FoFEquipmentItem( catalogueIndex ).helpToken;
}

int FoFEquipmentItemId( int catalogueIndex )
{
	return FoFEquipmentItem( catalogueIndex ).itemId;
}

int FoFEquipmentItemCost( int catalogueIndex )
{
	const FoFItemDefinition_t *pItem = FoFFindItemDefinitionById(
		FoFEquipmentItem( catalogueIndex ).itemId );
	return pItem ? pItem->m_nDeathmatchCost : 0;
}

int FoFEquipmentItemColumn( int catalogueIndex )
{
	return FoFItemDeathmatchColumn(
		FoFEquipmentItem( catalogueIndex ).itemId );
}

bool FoFEquipmentItemAvailable( int catalogueIndex )
{
	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	if ( !classicShootout.IsValid() || !classicShootout.GetBool() )
		return true;

	// FoF removes only walljump and slide from BuyMenuDM when
	// classic Shootout is enabled.
	const int itemId = FoFEquipmentItemId( catalogueIndex );
	return itemId != 15 && itemId != 23;
}

static int FoFEquipmentColumnForItemId( int itemId )
{
	return FoFItemDeathmatchColumn( itemId );
}

static void RemoveSelectedEquipmentGearAt(
	int *selectedItems,
	int &selectedCount,
	int selectedIndex )
{
	Assert( selectedIndex >= 0 && selectedIndex < selectedCount );
	if ( selectedIndex < 0 || selectedIndex >= selectedCount )
		return;

	for ( int move = selectedIndex; move + 1 < selectedCount; ++move )
		selectedItems[move] = selectedItems[move + 1];

	--selectedCount;
	selectedItems[selectedCount] = -1;
}

bool CHudFoF::IsEquipmentItemSelected( int itemId ) const
{
	if ( itemId == m_iSelectedAimItem )
		return true;

	for ( int i = 0; i < m_iSelectedGearCount; ++i )
	{
		if ( m_SelectedGearItems[i] == itemId )
			return true;
	}
	return false;
}

int CHudFoF::EquipmentPointTotal() const
{
	int total = 0;
	for ( int selected = 0; selected < m_iSelectedGearCount; ++selected )
	{
		for ( int item = 0; item < FOF_DM_ITEM_COUNT; ++item )
		{
			if ( FoFEquipmentItemColumn( item ) < 2 &&
				FoFEquipmentItemId( item ) ==
					m_SelectedGearItems[selected] )
			{
				total += FoFEquipmentItemCost( item );
				break;
			}
		}
	}

	for ( int item = 0; item < FOF_DM_ITEM_COUNT; ++item )
	{
		if ( FoFEquipmentItemColumn( item ) == 2 &&
			FoFEquipmentItemId( item ) == m_iSelectedAimItem )
		{
			total += FoFEquipmentItemCost( item );
			break;
		}
	}
	return total;
}

bool CHudFoF::RemoveFirstEquipmentGearForBudget(
	int protectedItemId )
{
	for ( int candidateIndex = 0;
		candidateIndex < FOF_DM_ITEM_COUNT;
		++candidateIndex )
	{
		const int candidateColumn =
			FoFEquipmentItemColumn( candidateIndex );
		const int candidateItemId =
			FoFEquipmentItemId( candidateIndex );
		if ( candidateColumn >= 2 ||
			candidateItemId == protectedItemId )
		{
			continue;
		}

		for ( int selected = 0;
			selected < m_iSelectedGearCount;
			++selected )
		{
			if ( m_SelectedGearItems[selected] != candidateItemId )
				continue;

			RemoveSelectedEquipmentGearAt(
				m_SelectedGearItems,
				m_iSelectedGearCount,
				selected );
			return true;
		}
	}
	return false;
}

bool CHudFoF::MakeEquipmentBudgetRoom(
	int additionalPoints,
	int protectedItemId )
{
	for ( int eviction = 0;
		eviction < 3 &&
			EquipmentPointTotal() + additionalPoints >
				FOF_DM_POINT_LIMIT;
		++eviction )
	{
		if ( !RemoveFirstEquipmentGearForBudget( protectedItemId ) )
			break;
	}

	return EquipmentPointTotal() + additionalPoints <=
		FOF_DM_POINT_LIMIT;
}

void CHudFoF::LoadEquipmentSelection()
{
	ConVar *gearCvars[FOF_DM_TOTAL_SLOTS] =
	{
		&fof_gear_0,
		&fof_gear_1,
		&fof_gear_2,
		&fof_gear_3,
		&fof_gear_4,
		&fof_gear_5
	};

	m_iSelectedGearCount = 0;
	m_iSelectedAimItem = 43;
	Q_memset( m_SelectedGearItems, 0xff, sizeof( m_SelectedGearItems ) );

	int loadedItems[FOF_DM_TOTAL_SLOTS];
	for ( int slot = 0; slot < FOF_DM_TOTAL_SLOTS; ++slot )
	{
		loadedItems[slot] = gearCvars[slot]->GetInt();
		for ( int item = 0; item < FOF_DM_ITEM_COUNT; ++item )
		{
			if ( FoFEquipmentItemId( item ) != loadedItems[slot] ||
				FoFEquipmentItemAvailable( item ) )
			{
				continue;
			}

			loadedItems[slot] = -1;
			gearCvars[slot]->SetValue( -1 );
			break;
		}
	}

	// Resolve the mutually-exclusive handgun skill before loading gear.  Its
	// point cost participates in the same eleven-point budget; processing the
	// cvars in a single pass would incorrectly budget against the fallback
	// skill until the saved skill (normally the sixth entry) was encountered.
	for ( int slot = 0; slot < FOF_DM_TOTAL_SLOTS; ++slot )
	{
		for ( int item = 0; item < FOF_DM_ITEM_COUNT; ++item )
		{
			if ( FoFEquipmentItemColumn( item ) != 2 ||
				FoFEquipmentItemId( item ) != loadedItems[slot] )
			{
				continue;
			}

			m_iSelectedAimItem = loadedItems[slot];
			break;
		}
	}

	for ( int slot = 0; slot < FOF_DM_TOTAL_SLOTS; ++slot )
	{
		for ( int item = 0; item < FOF_DM_ITEM_COUNT; ++item )
		{
			if ( FoFEquipmentItemColumn( item ) >= 2 ||
				FoFEquipmentItemId( item ) != loadedItems[slot] )
			{
				continue;
			}

			const int itemColumn = FoFEquipmentItemColumn( item );
			if ( itemColumn == 0 )
			{
				// BuyMenuDM has one primary-weapon slot. Sanitize legacy presets
				// that contain more than one selected primary tile.
				bool alreadyHasPrimary = false;
				for ( int selected = 0;
					selected < m_iSelectedGearCount;
					++selected )
				{
					if ( FoFEquipmentColumnForItemId(
						m_SelectedGearItems[selected] ) == 0 )
					{
						alreadyHasPrimary = true;
						break;
					}
				}
				if ( alreadyHasPrimary )
					break;
			}

			// Secondary equipment remains an ordered multiset.  In
			// particular, [3,3] is the canonical dual Deringer loadout and
			// must survive opening/accepting this menu.
			if ( m_iSelectedGearCount >= FOF_DM_GEAR_SLOTS ||
				EquipmentPointTotal() + FoFEquipmentItemCost( item ) >
					FOF_DM_POINT_LIMIT )
			{
				break;
			}
			m_SelectedGearItems[m_iSelectedGearCount++] =
				loadedItems[slot];
			break;
		}
	}
}

void CHudFoF::SaveEquipmentSelection()
{
	ConVar *gearCvars[FOF_DM_TOTAL_SLOTS] =
	{
		&fof_gear_0,
		&fof_gear_1,
		&fof_gear_2,
		&fof_gear_3,
		&fof_gear_4,
		&fof_gear_5
	};
	int values[FOF_DM_TOTAL_SLOTS];
	Q_memset( values, 0xff, sizeof( values ) );

	int count = 0;
	if ( m_iSelectedAimItem >= 0 &&
		count < FOF_DM_TOTAL_SLOTS )
	{
		values[count++] = m_iSelectedAimItem;
	}
	for ( int i = 0;
		i < m_iSelectedGearCount &&
			count < FOF_DM_TOTAL_SLOTS;
		++i )
	{
		values[count++] = m_SelectedGearItems[i];
	}

	for ( int i = 0; i < FOF_DM_TOTAL_SLOTS; ++i )
		gearCvars[i]->SetValue( values[i] );
}

void CHudFoF::ToggleEquipmentItem( int catalogueIndex )
{
	if ( catalogueIndex < 0 ||
		catalogueIndex >= FOF_DM_ITEM_COUNT ||
		!FoFEquipmentItemAvailable( catalogueIndex ) )
	{
		return;
	}

	const int itemId = FoFEquipmentItemId( catalogueIndex );
	const int itemCost = FoFEquipmentItemCost( catalogueIndex );
	const int itemColumn = FoFEquipmentItemColumn( catalogueIndex );
	if ( itemColumn == 2 )
	{
		if ( m_iSelectedAimItem == itemId )
		{
			m_iSelectedAimItem = -1;
			return;
		}
		const int previousAimItem = m_iSelectedAimItem;
		m_iSelectedAimItem = itemId;
		if ( !MakeEquipmentBudgetRoom( 0, -1 ) )
			m_iSelectedAimItem = previousAimItem;
		return;
	}

	for ( int selected = 0;
		selected < m_iSelectedGearCount;
		++selected )
	{
		if ( m_SelectedGearItems[selected] != itemId )
			continue;
		RemoveSelectedEquipmentGearAt(
			m_SelectedGearItems,
			m_iSelectedGearCount,
			selected );
		return;
	}

	int previousGearItems[FOF_DM_GEAR_SLOTS];
	const int previousGearCount = m_iSelectedGearCount;
	Q_memcpy( previousGearItems,
		m_SelectedGearItems,
		sizeof( previousGearItems ) );

	if ( itemColumn == 0 )
	{
		// The shipped panel treats the primary column as one slot: clicking
		// another primary replaces the old one before the eleven-point
		// budget is resolved.  Secondary gear is deliberately still multi-
		// select, including duplicate Deringers for a dual-wield loadout.
		for ( int selected = m_iSelectedGearCount - 1;
			selected >= 0;
			--selected )
		{
			if ( FoFEquipmentColumnForItemId(
				m_SelectedGearItems[selected] ) != 0 )
			{
				continue;
			}

			RemoveSelectedEquipmentGearAt(
				m_SelectedGearItems,
				m_iSelectedGearCount,
				selected );
		}
	}

	// BuyMenuDM in the shipped client gives the newly clicked item priority.
	// When it would exceed eleven points, it removes the first selected item
	// in catalogue order (excluding the new item), repeating at most three
	// times before rejecting the click.  Preserve that ordering rather than
	// silently leaving an over-budget click unhandled.
	const bool hasBudgetRoom =
		MakeEquipmentBudgetRoom( itemCost, itemId );

	if ( m_iSelectedGearCount >= FOF_DM_GEAR_SLOTS ||
		!hasBudgetRoom ||
		EquipmentPointTotal() + itemCost > FOF_DM_POINT_LIMIT )
	{
		if ( itemColumn == 0 )
		{
			m_iSelectedGearCount = previousGearCount;
			Q_memcpy( m_SelectedGearItems,
				previousGearItems,
				sizeof( previousGearItems ) );
		}
		return;
	}
	m_SelectedGearItems[m_iSelectedGearCount++] = itemId;
}

void CHudFoF::AcceptEquipmentSelection()
{
	int items[FOF_DM_TOTAL_SLOTS];
	int count = 0;
	if ( m_iSelectedAimItem >= 0 &&
		count < FOF_DM_TOTAL_SLOTS )
	{
		items[count++] = m_iSelectedAimItem;
	}
	for ( int i = 0;
		i < m_iSelectedGearCount &&
			count < FOF_DM_TOTAL_SLOTS;
		++i )
	{
		items[count++] = m_SelectedGearItems[i];
	}

	SaveEquipmentSelection();
	if ( count > 0 )
		SubmitEquipment( items, count );
	ClearMenu();
}

// Shootout equipment menu painting and help presentation.

void CHudFoF::PaintEquipmentMenu()
{
	EnsureMenuTextures();

	// The dedicated equipment child layer applies BuyMenuDM's modal alpha
	// before VGUI enters this Paint call.  Keep draw colours unmodified here.

	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int boxX =
		ScreenWidth() / 2 - RoundFloatToInt( 200.0f * scale );
	const int boxY =
		ScreenHeight() / 2 - RoundFloatToInt( 150.0f * scale );
	DrawMenuTexture(
		m_iEquipmentBackgroundTexture,
		boxX,
		boxY,
		MAX( RoundFloatToInt( 400.0f * scale ), 1 ),
		MAX( RoundFloatToInt( 300.0f * scale ), 1 ) );

	const int points = EquipmentPointTotal();
	wchar_t creditStars[FOF_DM_POINT_LIMIT + 1];
	for ( int star = 0; star < FOF_DM_POINT_LIMIT; ++star )
	{
		// The shipped meter distinguishes spent credits from available ones:
		// U+2605 is the filled star, U+2606 is its hollow counterpart.  Drawing
		// eleven filled glyphs made a 0/11 loadout look fully spent.
		creditStars[star] =
			star < clamp( points, 0, FOF_DM_POINT_LIMIT ) ?
				L'\x2605' : L'\x2606';
	}
	creditStars[FOF_DM_POINT_LIMIT] = L'\0';
	wchar_t creditValue[16];
	V_snwprintf(
		creditValue,
		ARRAYSIZE( creditValue ),
		L"%d/%d",
		points,
		FOF_DM_POINT_LIMIT );
	vgui::surface()->DrawSetTextFont( m_hEquipmentCreditFont );
	vgui::surface()->DrawSetTextColor( 155, 155, 155, 255 );
	for ( int star = 0; star < FOF_DM_POINT_LIMIT; ++star )
	{
		vgui::surface()->DrawSetTextPos(
			boxX + RoundFloatToInt(
				( 25.0f + star * 25.0f ) * scale ),
			boxY + RoundFloatToInt( 5.0f * scale ) );
		vgui::surface()->DrawPrintText( &creditStars[star], 1 );
	}
	vgui::surface()->DrawSetTextPos(
		boxX + RoundFloatToInt( 300.0f * scale ),
		boxY + RoundFloatToInt( 5.0f * scale ) );
	vgui::surface()->DrawPrintText(
		creditValue, V_wcslen( creditValue ) );

	static const char *headerTokens[] =
	{
		"#Equipment_Primary",
		"#Equipment_Secondary",
		"#Equipment_Aim_Skill"
	};
	for ( int column = 0; column < ARRAYSIZE( headerTokens ); ++column )
	{
		wchar_t localizedBuffer[256];
		const wchar_t *header = Localize(
			headerTokens[column],
			localizedBuffer,
			sizeof( localizedBuffer ) );
		vgui::surface()->DrawSetTextFont( m_hEquipmentHeaderFont );
		vgui::surface()->DrawSetTextColor( 192, 192, 196, 255 );
		vgui::surface()->DrawSetTextPos(
			boxX + RoundFloatToInt(
				( 25.0f + column * 120.0f ) * scale ),
			boxY + RoundFloatToInt( 30.0f * scale ) );
		vgui::surface()->DrawPrintText(
			header, V_wcslen( header ) );
	}

	for ( int i = 0; i < FOF_DM_ITEM_COUNT; ++i )
	{
		if ( !FoFEquipmentItemAvailable( i ) )
			continue;

		const int itemId = FoFEquipmentItemId( i );
		const int itemCost = FoFEquipmentItemCost( i );
		const int itemColumn = FoFEquipmentItemColumn( i );
		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		m_pEquipmentMenuButtons[i]->GetBounds( x, y, wide, tall );
		vgui::Button *button = m_pEquipmentMenuButtons[i];

		Color itemColor( 234, 234, 234, 255 );
		if ( !button->IsEnabled() )
			itemColor = Color( 100, 100, 100, 255 );
		else if ( button->IsDepressed() )
			itemColor = Color( 255, 255, 0, 255 );
		else if ( button->IsArmed() )
			itemColor = Color( 155, 155, 155, 255 );
		else if ( IsEquipmentItemSelected( itemId ) )
			itemColor = Color( 244, 213, 50, 255 );

		DrawMenuTexture(
			m_iEquipmentItemBackgroundTexture,
			x, y, wide, tall,
			Color( 234, 234, 234, 255 ) );
		DrawMenuTexture(
			m_iEquipmentItemTextures[i],
			x, y, wide, tall,
			itemColor );

		wchar_t localizedBuffer[256];
		const wchar_t *label = Localize(
			FoFEquipmentItemLabel( i ),
			localizedBuffer,
			sizeof( localizedBuffer ) );
		int labelWide = 0;
		int labelTall = 0;
		vgui::surface()->GetTextSize(
			m_hEquipmentCostFont,
			label,
			labelWide,
			labelTall );
		// The original BuyMenuDM applies its HudSelectionNumbers2 handle
		// to both the weapon and aim-skill name labels.  DefaultSmall
		// is a point taller and uses the wrong face once proportional scaling
		// is applied at 1024x768.
		vgui::surface()->DrawSetTextFont( m_hEquipmentCostFont );
		vgui::surface()->DrawSetTextColor( 234, 234, 234, 255 );
		const int labelY = itemColumn == 2
			? y + MIN(
				MAX(
					( tall - labelTall ) / 2 +
						RoundFloatToInt( 2.0f * scale ),
					0 ),
				MAX( tall - labelTall, 0 ) )
			: y + MAX( tall - labelTall - 1, 1 );
		vgui::surface()->DrawSetTextPos(
			x + MAX( ( wide - labelWide ) / 2, 1 ),
			labelY );
		vgui::surface()->DrawPrintText(
			label,
			V_wcslen( label ) );

		if ( itemCost > 0 )
		{
			const int cost = MIN( itemCost, 7 );
			wchar_t costWide[16];
			for ( int star = 0; star < cost; ++star )
				costWide[star] = L'\x2605';
			costWide[cost] = L'\0';
			int costPixelWide = 0;
			int costPixelTall = 0;
			vgui::surface()->GetTextSize(
				m_hEquipmentCostFont,
				costWide,
				costPixelWide,
				costPixelTall );
			vgui::surface()->DrawSetTextFont(
				m_hEquipmentCostFont );
			// Cost stars remain neutral in the shipped panel; selection
			// only modulates the item image.
			vgui::surface()->DrawSetTextColor( 155, 155, 155, 255 );
			vgui::surface()->DrawSetTextPos(
				x + wide - costPixelWide - 3,
				y + 1 );
			vgui::surface()->DrawPrintText(
				costWide,
				V_wcslen( costWide ) );
		}

		if ( FoFEquipmentItemHelpToken( i ) )
		{
			const int helpSize =
				MAX( RoundFloatToInt( 10.0f * scale ), 1 );
			const int helpX =
				x + RoundFloatToInt( 101.0f * scale );
			// The shipped help child is transparent and keeps its own
			// modulation; hovering the adjacent item must not tint it.
			vgui::surface()->DrawSetColor( 192, 192, 192, 180 );
			vgui::surface()->DrawOutlinedRect(
				helpX,
				y,
				helpX + helpSize,
				y + helpSize );
			DrawAnsi(
				"?",
				helpX + helpSize / 2,
				y,
				Color( 217, 212, 199, 200 ),
				true );
		}
	}

	int okayX = 0;
	int okayY = 0;
	int okayWide = 0;
	int okayTall = 0;
	m_pEquipmentOkayButton->GetBounds(
		okayX, okayY, okayWide, okayTall );
	DrawMenuTexture(
		m_iEquipmentOkayTexture,
		okayX,
		okayY,
		okayWide,
		okayTall,
		m_pEquipmentOkayButton->IsArmed()
			? Color( 255, 224, 120, 255 )
			: Color( 255, 255, 255, 255 ) );

	int closeX = 0;
	int closeY = 0;
	int closeWide = 0;
	int closeTall = 0;
	m_pEquipmentCloseButton->GetBounds(
		closeX, closeY, closeWide, closeTall );
	if ( m_pEquipmentCloseButton->IsArmed() )
	{
		vgui::surface()->DrawSetColor( 153, 43, 35, 255 );
		vgui::surface()->DrawFilledRect(
			closeX,
			closeY,
			closeX + closeWide,
			closeY + closeTall );
	}
	vgui::surface()->DrawSetColor( 206, 192, 180, 255 );
	vgui::surface()->DrawOutlinedRect(
		closeX,
		closeY,
		closeX + closeWide,
		closeY + closeTall );
	DrawAnsi(
		"X",
		closeX + closeWide / 2,
		closeY,
		Color( 225, 225, 225, 255 ),
		true );

	// BuyMenuDM's "warning" label occupies the otherwise transparent strip
	// immediately below the 400x300 artwork.  It is a centered 400x25 label
	// using HudSelectionNumbers3.  The original panel alpha is not applied a
	// second time when this label is painted directly through ISurface.
	wchar_t footerLocalized[256];
	const wchar_t *footerRaw = Localize(
		"#FoF_Loadout_panel",
		footerLocalized,
		sizeof( footerLocalized ) );
	wchar_t footerText[256];
	FoFReplaceKeyBindings(
		footerRaw,
		0,
		footerText,
		sizeof( footerText ) );
	if ( m_hEquipmentFooterFont != vgui::INVALID_FONT && footerText[0] )
	{
		const int footerY = boxY +
			RoundFloatToInt( 300.0f * scale );
		const int footerWide = MAX(
			RoundFloatToInt( 400.0f * scale ), 1 );
		const int footerTall = MAX(
			RoundFloatToInt( 25.0f * scale ), 1 );
		int textWide = 0;
		int textTall = 0;
		vgui::surface()->GetTextSize(
			m_hEquipmentFooterFont,
			footerText,
			textWide,
			textTall );
		vgui::surface()->DrawSetTextFont( m_hEquipmentFooterFont );
		vgui::surface()->DrawSetTextColor( 250, 250, 250, 255 );
		vgui::surface()->DrawSetTextPos(
			boxX + MAX( ( footerWide - textWide ) / 2, 0 ),
			footerY + MAX( ( footerTall - textTall ) / 2, 0 ) );
		vgui::surface()->DrawPrintText(
			footerText,
			V_wcslen( footerText ) );
	}

}

void CHudFoF::ShowEquipmentHelp( int catalogueIndex )
{
	if ( m_iMenuKind != FOF_MENU_EQUIPMENT ||
		catalogueIndex < 0 || catalogueIndex >= FOF_DM_ITEM_COUNT ||
		!FoFEquipmentItemAvailable( catalogueIndex ) ||
		!FoFEquipmentItemHelpToken( catalogueIndex ) )
	{
		return;
	}

	EnsureMenuControls();
	m_iEquipmentHelpItem = catalogueIndex;
	if ( m_pEquipmentHelpFrame )
	{
		m_pEquipmentHelpFrame->SetTitle( L"", false );
		m_pEquipmentHelpFrame->SetBgColor(
			Color( 150, 150, 150, 255 ) );
		m_pEquipmentHelpFrame->SetPaintBackgroundType( 2 );
	}
	if ( m_pEquipmentHelpTitle )
	{
		wchar_t localizedTitle[256];
		const wchar_t *title = Localize(
			FoFEquipmentItemLabel( catalogueIndex ),
			localizedTitle,
			sizeof( localizedTitle ) );
		m_pEquipmentHelpTitle->SetText( title );
	}
	if ( m_pEquipmentHelpText )
	{
		wchar_t localizedHelp[2048];
		const wchar_t *rawHelp = Localize(
			FoFEquipmentItemHelpToken( catalogueIndex ),
			localizedHelp,
			sizeof( localizedHelp ) );
		wchar_t boundHelp[2048];
		FoFReplaceKeyBindings(
			rawHelp,
			0,
			boundHelp,
			sizeof( boundHelp ) );
		// BuyMenuDM::ApplySchemeSettings assigns this exact foreground; the
		// NotorietyFont supplies the opaque black drop shadow.
		m_pEquipmentHelpText->SetFgColor(
			Color( 250, 250, 250, 255 ) );
		m_pEquipmentHelpText->SetBgColor(
			Color( 10, 10, 10, 10 ) );
		m_pEquipmentHelpText->SetAlpha( 255 );
		m_pEquipmentHelpText->SetText( boundHelp );
	}
	LayoutMenuControls();
	SetMenuControlsVisible( true );
}

void CHudFoF::CloseEquipmentHelp()
{
	if ( m_iEquipmentHelpItem < 0 )
		return;
	m_iEquipmentHelpItem = -1;
	LayoutMenuControls();
	SetMenuControlsVisible( m_bMenuVisible );
}
