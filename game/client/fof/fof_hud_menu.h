#ifndef FOF_HUD_MENU_H
#define FOF_HUD_MENU_H
#ifdef _WIN32
#pragma once
#endif

// Shared state for the generic server menu and external viewport panels.
// Team-selection queries live in fof_team_menu.h.
bool FoFHudInfoPanelVisible();
bool FoFHudExternalUIOwnsInput();

// The weapon-selection input path owns slot1..slot0. It asks the FoF menu
// first so display rows can be mapped back to arbitrary signed command IDs.
bool FoFMenuIsOpen();
bool FoFMenuBlocksPlayerMovement();
bool FoFMenuSelectDisplaySlot( int slot );
bool FoFMenuClose();

#include "tier1/utlstring.h"

struct FoFMenuEntry
{
	CUtlString label;
	int commandId;
	CUtlString clientCommand;
	int presetItems[6];
	int presetCount;
	int nextPage;
	char encodedKind;
	int encodedValue;
	int encodedQuantity;
	CUtlString encodedLabel;
	bool encodedValid;
};

enum FoFMenuKind
{
	FOF_MENU_NONE = 0,
	FOF_MENU_TEXT,
	FOF_MENU_TEAM,
	FOF_MENU_TEAM_CLASS,
	FOF_MENU_EQUIPMENT,
	FOF_MENU_PURCHASE,
	FOF_MENU_CRATE
};

enum
{
	FOF_TEAM_BUTTON_COUNT = 7,
	FOF_TEAM_CLASS_COUNT = 8,
	FOF_TEAM_CLASS_ITEM_COUNT = 6,
	FOF_DM_ITEM_COUNT = 22,
	FOF_DM_GEAR_SLOTS = 5,
	FOF_DM_TOTAL_SLOTS = 6,
	FOF_DM_POINT_LIMIT = 11,
	FOF_PURCHASE_MAX_ITEMS = 6,
	FOF_PURCHASE_MAX_VISIBLE_PRESETS = 25,
	FOF_PURCHASE_FILTER_COUNT = 4,
	FOF_PURCHASE_ITEM_TEXTURE_COUNT = 56,
	FOF_CRATE_ITEM_COUNT = 56,
	FOF_CRATE_ROW_COUNT = 10
};

bool FoFShouldHideStockVoteMenu();

#endif // FOF_HUD_MENU_H
