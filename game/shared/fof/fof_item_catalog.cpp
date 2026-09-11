// Shared gameplay fields from FoF's 56-entry FoF item catalogue.

#include "cbase.h"
#include "fof/fof_item_catalog.h"
#include "tier1/utlvector.h"

#include <xmmintrin.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Presentation order, materials and localized labels remain client-only.
// These fields affect command validation, inventory grants or cash and must
// be identical in both game DLLs.
static const FoFItemDefinition_t s_FoFItemDefinitions[] =
{
	{ 45, -1,  -1, -2, 0, -1,     0, "fists",            "#Item1",        "weapon_fists",            NULL },
	{ 41,  2,   1, -2, 12, 0,     0, "skill_right",      "#Item41",       NULL,                      NULL },
	{ 42,  2,   1, -2, 12, 1,   400, "skill_left",       "#Item42",       NULL,                      NULL },
	{ 44,  2,   1, -2, 12, 1,  1500, "skill_fan",        "#Item44",       NULL,                      NULL },
	{ 43,  2,   0, -2, 12, 2,   800, "skill_ambi",       "#Item43",       NULL,                      NULL },
	{ 14,  0,   5,  0, 12, 1,   800, "handgun_throw",    "#Item14",       NULL,                      NULL },
	{ 15,  0,   5,  0, 12, 2,   800, "wall jump",        "#Item23e",      NULL,                      NULL },
	{ 23,  0,  10,  0, 12, 2,  1000, "slide",            "#Item23d",      NULL,                      NULL },
	{ 25,  0,  -1,  0, 12, 3,  1000, "heavyload",        "#Item25",       NULL,                      NULL },
	{ 34,  0,   7,  0, 12, 3,   400, "brass_knuckles",   "#Item34b",      NULL,                      NULL },
	{  1,  0,  10,  0, 8,  3,     0, "knife",            "#Item1b",       "weapon_knife",            NULL },
	{ 20,  0,   7,  0, 12, 4,     0, "boots",            "#Item34",       NULL,                      NULL },
	{  3,  0,  10,  0, 2,  5,  1300, "deringer",         "#Item3c",       "weapon_deringer",         "weapon_deringer2" },
	{  7,  0,  50,  0, 5,  6,     0, "dynamite",         "#Item0",        "weapon_dynamite",         NULL },
	{ 28,  1,  20,  0, 2,  3,     0, "volcanic",         "#ItemVolcanic", "weapon_volcanic",         "weapon_volcanic2" },
	{  2,  1,  25,  0, 2,  4,     0, "coltnavy",         "#Item2",        "weapon_coltnavy",         "weapon_coltnavy2" },
	{ 19,  1,  20,  1, 8,  4,     0, "axe",              "#Item19",       "weapon_axe",              NULL },
	{ 11,  1,  25,  1, 1,  4,  1000, "bow",              "#Item11",       "weapon_bow",              NULL },
	{ 13,  1,  30,  1, 2,  4,   200, "sawedoff_shotgun", "#Item13b",      "weapon_sawedoff_shotgun", "weapon_sawedoff_shotgun2" },
	{ 22,  1,  22,  0, 2,  5,   600, "hammerless",       "#Item3",        "weapon_hammerless",       "weapon_hammerless2" },
	{ 30,  1,  33,  0, 2,  5,  1700, "remington_army",   "#Rem_Army",     "weapon_remington_army",   "weapon_remington_army2" },
	{ 33,  1,  35,  0, 2,  6,  2000, "maresleg",         "#MaresLeg",     "weapon_maresleg",         "weapon_maresleg2" },
	{ 31, -1,  38,  1, 2, 20,  2500, "schofield",        "#Item31a",      "weapon_schofield",        "weapon_schofield2" },
	{  5,  1,  35,  0, 3,  7,  3200, "carbine",          "#Item5",        "weapon_carbine",          NULL },
	{ 21, -1,  50,  2, 2, 20,  5000, "peacemaker",       "#Item21",       "weapon_peacemaker",       "weapon_peacemaker2" },
	{ 24, -1,  55,  2, 1, 20,  4000, "bow_black",        "#Item11b",      "weapon_bow_black",        NULL },
	{  4, -1,  50,  1, 3, 20,  4000, "henryrifle",       "#Item4a",       "weapon_henryrifle",       NULL },
	{  6, -1,  65,  2, 3, 20,  1000, "coachgun",         "#Item6",        "weapon_coachgun",         NULL },
	{ 18, -1,  80,  2, 3, 20,  7500, "spencer",          "#Item18",       "weapon_spencer",          NULL },
	{ 26, -1,  85,  3, 8, 20,  2500, "machete",          "#Item26",       "weapon_machete",          NULL },
	{ 29, -1,  95,  3, 3, 20,  4000, "shotgun",          "#Item30",       "weapon_shotgun",          NULL },
	{ 27, -1, 125,  2, 5, 20,  5000, "dynamite_black",   "#Item0b",       "weapon_dynamite_black",   NULL },
	{ 12, -1, 115,  3, 3, 20,  9000, "sharps",           "#Item12",       "weapon_sharps",           NULL },
	{ 32, -1, 125,  3, 2, 20, 10000, "walker",           "#Item32",       "weapon_walker",           "weapon_walker2" },
	{ 17, -1,  -1,  3, 5, 20,  5000, "dynamite_belt",    "#Item17",       "weapon_dynamite_belt",    NULL },
	{ 16, -1,  -1,  1, 6, 20,   200, "whiskey",          "#Item15",       "weapon_whiskey",          "weapon_whiskey2" },
	{  9, -1,  -1, -2, 1, 20, 10000, "xbow",             "#Item11",       "weapon_xbow",             NULL },
};

struct FoFItemStatisticsName_t
{
	const char *m_pszClassname;
	const char *m_pszOppositeHandClassname;
};

// FoF's HitRecon message uses the index in the complete 56-entry
// statistics table, not the shorter purchase catalogue above.  The first
// sixteen rows are damage aliases and the original table also includes the
// dormant ghost gun and Wood_Crate rows.  Preserve those positions so the
// client's per-item damage counters receive the same indices.
static const FoFItemStatisticsName_t s_FoFItemStatisticsNames[] =
{
	{ "horse-ram", NULL },
	{ "kick-fall", NULL },
	{ "flame", NULL },
	{ "thrown_gun", NULL },
	{ "kick", NULL },
	{ "blast", NULL },
	{ "physics", NULL },
	{ "dynamite_black", NULL },
	{ "dynamite_yellow", NULL },
	{ "dynamite", NULL },
	{ "arrow", NULL },
	{ "arrow_black", NULL },
	{ "x_arrow", NULL },
	{ "thrown_knife", NULL },
	{ "thrown_axe", NULL },
	{ "thrown_machete", NULL },
	{ "weapon_fists", NULL },
	{ "fists_brass", NULL },
	{ "weapon_ghostgun", "weapon_ghostgun2" },
	{ "skill_right", NULL },
	{ "skill_left", NULL },
	{ "skill_fan", NULL },
	{ "skill_ambi", NULL },
	{ "handgun_throw", NULL },
	{ "wall jump", NULL },
	{ "slide", NULL },
	{ "heavyload", NULL },
	{ "brass_knuckles", NULL },
	{ "weapon_knife", NULL },
	{ "boots", NULL },
	{ "weapon_deringer", "weapon_deringer2" },
	{ "weapon_dynamite", NULL },
	{ "Wood_Crate", "Wood_Crate" },
	{ "weapon_volcanic", "weapon_volcanic2" },
	{ "weapon_coltnavy", "weapon_coltnavy2" },
	{ "weapon_axe", NULL },
	{ "weapon_bow", NULL },
	{ "weapon_sawedoff_shotgun", "weapon_sawedoff_shotgun2" },
	{ "weapon_hammerless", "weapon_hammerless2" },
	{ "weapon_remington_army", "weapon_remington_army2" },
	{ "weapon_maresleg", "weapon_maresleg2" },
	{ "weapon_schofield", "weapon_schofield2" },
	{ "weapon_carbine", NULL },
	{ "weapon_peacemaker", "weapon_peacemaker2" },
	{ "weapon_bow_black", NULL },
	{ "weapon_henryrifle", NULL },
	{ "weapon_coachgun", NULL },
	{ "weapon_spencer", NULL },
	{ "weapon_machete", NULL },
	{ "weapon_shotgun", NULL },
	{ "weapon_dynamite_black", NULL },
	{ "weapon_sharps", NULL },
	{ "weapon_walker", "weapon_walker2" },
	{ "weapon_dynamite_belt", NULL },
	{ "weapon_whiskey", "weapon_whiskey2" },
	{ "weapon_xbow", NULL },
};

int FoFItemDefinitionCount()
{
	return ARRAYSIZE( s_FoFItemDefinitions );
}

const FoFItemDefinition_t *FoFItemDefinitionAt( int nIndex )
{
	return nIndex >= 0 && nIndex < ARRAYSIZE( s_FoFItemDefinitions ) ?
		&s_FoFItemDefinitions[nIndex] : NULL;
}

const FoFItemDefinition_t *FoFFindItemDefinitionById( int nItem )
{
	// Build the sparse ID index from the catalogue so its presentation order
	// and future item additions do not require a second hand-maintained table.
	struct FoFItemIdIndex
	{
		FoFItemIdIndex()
		{
			int nMaxId = 0;
			for ( int i = 0; i < ARRAYSIZE( s_FoFItemDefinitions ); ++i )
				nMaxId = MAX( nMaxId, s_FoFItemDefinitions[i].m_nItem );
			items.SetCount( nMaxId + 1 );
			for ( int i = 0; i < items.Count(); ++i )
				items[i] = NULL;
			for ( int i = 0; i < ARRAYSIZE( s_FoFItemDefinitions ); ++i )
			{
				const FoFItemDefinition_t &item = s_FoFItemDefinitions[i];
				Assert( item.m_nItem >= 0 && !items[item.m_nItem] );
				items[item.m_nItem] = &item;
			}
		}

		CUtlVector< const FoFItemDefinition_t * > items;
	};
	static const FoFItemIdIndex index;
	return index.items.IsValidIndex( nItem ) ? index.items[nItem] : NULL;
}

const FoFItemDefinition_t *FoFFindItemDefinitionByClassname(
	const char *pszClassname )
{
	if ( !pszClassname || !pszClassname[0] )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( s_FoFItemDefinitions ); ++i )
	{
		const FoFItemDefinition_t &item = s_FoFItemDefinitions[i];
		if ( ( item.m_pszClassname &&
			   !Q_stricmp( pszClassname, item.m_pszClassname ) ) ||
			( item.m_pszOppositeHandClassname &&
			  !Q_stricmp( pszClassname, item.m_pszOppositeHandClassname ) ) )
		{
			return &item;
		}
	}
	return NULL;
}

const FoFItemDefinition_t *FoFFindItemDefinitionByToken(
	const char *pszToken )
{
	if ( !pszToken || !pszToken[0] )
		return NULL;

	for ( int i = 0; i < ARRAYSIZE( s_FoFItemDefinitions ); ++i )
	{
		const FoFItemDefinition_t &item = s_FoFItemDefinitions[i];
		if ( item.m_pszToken &&
			!Q_stricmp( pszToken, item.m_pszToken ) )
		{
			return &item;
		}
		if ( item.m_pszClassname &&
			( !Q_stricmp( pszToken, item.m_pszClassname ) ||
			  ( !Q_strnicmp( item.m_pszClassname, "weapon_", 7 ) &&
			    !Q_stricmp( pszToken, item.m_pszClassname + 7 ) ) ) )
		{
			return &item;
		}
		if ( item.m_pszOppositeHandClassname &&
			!Q_stricmp( pszToken, item.m_pszOppositeHandClassname ) )
		{
			return &item;
		}
	}
	return NULL;
}

bool FoFItemProgressionUnlocked(
	const FoFItemDefinition_t *pItem, int nProgression )
{
	return pItem && nProgression >= pItem->m_nProgressionRequirement;
}

int FoFItemStatisticsIndex( const char *pszToken )
{
	if ( !pszToken || !pszToken[0] )
		return -1;

	for ( int i = 0; i < ARRAYSIZE( s_FoFItemStatisticsNames ); ++i )
	{
		const FoFItemStatisticsName_t &item =
			s_FoFItemStatisticsNames[i];
		if ( !Q_strcmp( pszToken, item.m_pszClassname ) ||
			( item.m_pszOppositeHandClassname &&
			  !Q_strcmp( pszToken, item.m_pszOppositeHandClassname ) ) )
		{
			return i;
		}
	}

	return -1;
}

int FoFItemDeathmatchColumn( int nItem )
{
	const FoFItemDefinition_t *pItem = FoFFindItemDefinitionById( nItem );
	if ( !pItem )
		return -1;

	if ( pItem->m_nDeathmatchCategory == 1 )
		return 0;
	if ( pItem->m_nDeathmatchCategory == 0 )
		return 1;
	if ( pItem->m_nDeathmatchCategory == 2 )
		return 2;
	return -1;
}

static float FoFItemConVarMultiplier( const char *pszName )
{
	ConVarRef value( pszName, true );
	return value.IsValid() ? value.GetFloat() : 1.0f;
}

float FoFItemPriceMultiplier( int nItem )
{
	switch ( nItem )
	{
	case 7:
	case 27:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_dynamite" );
	case 12:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_sharps" );
	case 11:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_bow" );
	case 4:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_henry" );
	case 5:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_carbine" );
	case 32:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_walker" );
	case 41:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_righthanded" );
	case 42:
		return FoFItemConVarMultiplier( "fof_sv_pricemult_lefthanded" );
	default:
		return 1.0f;
	}
}

float FoFItemBuyZonePriceMultiplier(
	int nCurrentMode, int nBuyZone, int nPurchaseTier )
{
	if ( nCurrentMode == 2 || nBuyZone >= nPurchaseTier )
		return 1.0f;
	if ( nBuyZone == 2 && nPurchaseTier == 3 )
		return 1.5f;
	return 2.0f;
}

int FoFItemAdjustedPrice(
	const FoFItemDefinition_t *pItem,
	int nCurrentMode, int nBuyZone )
{
	if ( !pItem || pItem->m_nBasePrice < 0 )
		return 0;

	// Both shipped modules use scalar single-precision multiplies followed by
	// cvttss2si. Keeping those operations explicit prevents one-credit drift.
	__m128 price = _mm_set_ss( (float)pItem->m_nBasePrice );
	price = _mm_mul_ss(
		price, _mm_set_ss( FoFItemPriceMultiplier( pItem->m_nItem ) ) );
	price = _mm_mul_ss( price, _mm_set_ss(
		FoFItemBuyZonePriceMultiplier(
			nCurrentMode, nBuyZone, pItem->m_nPurchaseTier ) ) );
	return _mm_cvttss_si32( price );
}
