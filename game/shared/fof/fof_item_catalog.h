#ifndef FOF_ITEM_CATALOG_H
#define FOF_ITEM_CATALOG_H
#ifdef _WIN32
#pragma once
#endif

struct FoFItemDefinition_t
{
	int m_nItem;
	int m_nDeathmatchCategory;
	int m_nBasePrice;
	int m_nPurchaseTier;
	int m_nWeaponType;
	int m_nDeathmatchCost;
	int m_nProgressionRequirement;
	const char *m_pszToken;
	const char *m_pszLabelToken;
	const char *m_pszClassname;
	const char *m_pszOppositeHandClassname;
};

enum
{
	FOF_ITEM_DM_POINT_LIMIT = 11,
	FOF_ITEM_PURCHASE_LIMIT = 6,
};

const FoFItemDefinition_t *FoFFindItemDefinitionById( int nItem );
const FoFItemDefinition_t *FoFFindItemDefinitionByToken(
	const char *pszToken );
int FoFItemStatisticsIndex( const char *pszToken );
int FoFItemDefinitionCount();
const FoFItemDefinition_t *FoFItemDefinitionAt( int nIndex );
bool FoFItemProgressionUnlocked(
	const FoFItemDefinition_t *pItem, int nProgression );
int FoFItemDeathmatchColumn( int nItem );
float FoFItemPriceMultiplier( int nItem );
float FoFItemBuyZonePriceMultiplier(
	int nCurrentMode, int nBuyZone, int nPurchaseTier );
int FoFItemAdjustedPrice(
	const FoFItemDefinition_t *pItem,
	int nCurrentMode, int nBuyZone );

#endif // FOF_ITEM_CATALOG_H
