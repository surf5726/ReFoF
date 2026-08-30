#ifndef FOF_PLAYER_ACTIVITIES_H
#define FOF_PLAYER_ACTIVITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "activitylist.h"

class CBaseCombatWeapon;
class CBasePlayer;

bool FoFTranslateNetworkActivity(
	int nOriginalActivity, Activity &localActivity );

CBaseCombatWeapon *FoFSelectGestureWeapon( CBasePlayer *pPlayer );
Activity FoFNoWeaponActivityOverride( Activity activity );
Activity FoFOriginalWeaponActivityOverride(
	CBasePlayer *pPlayer,
	CBaseCombatWeapon *pWeapon,
	Activity activity );

#endif // FOF_PLAYER_ACTIVITIES_H
