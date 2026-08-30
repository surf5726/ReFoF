//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF shared weapon-hand and dual-wield dispatch helpers.
//
//=============================================================================//

#ifndef FOF_PLAYER_WEAPONS_H
#define FOF_PLAYER_WEAPONS_H
#ifdef _WIN32
#pragma once
#endif

class CBaseCombatWeapon;
class CBaseCombatCharacter;
class CBasePlayer;

bool FoFPotionBlocksWeaponSelection(
	CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
bool FoFPotionLocksCurrentWeapons( CBasePlayer *pPlayer );

// Selects the exact predicted weapon entity when FoF owns duplicate weapon
// types in separate hands. Returns true when the FoF path handled selection.
bool FoFSelectExactWeapon(
	CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

#endif // FOF_PLAYER_WEAPONS_H
