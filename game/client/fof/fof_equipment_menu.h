#ifndef FOF_EQUIPMENT_MENU_H
#define FOF_EQUIPMENT_MENU_H
#ifdef _WIN32
#pragma once
#endif

// Read-only view of the original Shootout equipment catalogue.  Menu control
// construction and layout use these accessors without taking ownership of the
// loadout data or depending on its storage representation.
int FoFEquipmentItemCount();
const char *FoFEquipmentItemLabel( int catalogueIndex );
const char *FoFEquipmentItemMaterial( int catalogueIndex );
const char *FoFEquipmentItemBaseTexture( int catalogueIndex );
const char *FoFEquipmentItemHelpToken( int catalogueIndex );
int FoFEquipmentItemId( int catalogueIndex );
int FoFEquipmentItemCost( int catalogueIndex );
int FoFEquipmentItemColumn( int catalogueIndex );
bool FoFEquipmentItemAvailable( int catalogueIndex );

#endif
