#ifndef FOF_PURCHASE_MENU_H
#define FOF_PURCHASE_MENU_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_hud_menu.h"
#include "tier1/utlstring.h"

#include <vgui_controls/Panel.h>

class CHudFoF;

struct FoFPurchasePreset
{
	FoFPurchasePreset()
		: itemCount( 0 )
		, price( 0 )
		, sourceOrder( 0 )
	{
		for ( int i = 0; i < FOF_PURCHASE_MAX_ITEMS; ++i )
			items[i] = -1;
	}

	CUtlString name;
	int items[FOF_PURCHASE_MAX_ITEMS];
	int itemCount;
	int price;
	int sourceOrder;
};

// FoF uses two genuinely different layouts rather than shrinking the
// full preset menu.  Keep the geometry in one shared description so the
// invisible input buttons and the custom paint layer cannot drift apart.
struct FoFPurchaseMenuLayout
{
	bool quick;
	float scale;
	int columns;
	int rows;
	int visiblePresets;
	int cardWide;
	int cardTall;
	int gap;
	int pitchX;
	int pitchY;
	int gridX;
	int gridY;
	int gridWide;
	int gridTall;
};

class CFoFPurchasePaintPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFoFPurchasePaintPanel, vgui::Panel );

public:
	explicit CFoFPurchasePaintPanel( CHudFoF *owner );
	virtual void Paint();

private:
	CHudFoF *m_pOwner;
};

const char *FoFPurchaseItemMaterial( int itemId );
const char *FoFPurchaseItemNameToken( int itemId );
const char *FoFPurchaseItemFallbackName( int itemId );
int FoFPurchaseItemBasePrice( int itemId );
int FoFPurchaseItemTier( int itemId );
int FoFPurchaseItemRange( int itemId );
int FoFPurchaseEditorItemCount();
int FoFPurchaseEditorItemId( int index );
int FoFScalePurchasePixel( float virtualPixels, float scale );
FoFPurchaseMenuLayout FoFBuildPurchaseMenuLayout(
	bool quick,
	int screenWide,
	int screenTall );
int FoFPurchasePresetPriceForBuyZone(
	const FoFPurchasePreset &preset,
	int currentMode,
	int inBuyZone );

#endif
