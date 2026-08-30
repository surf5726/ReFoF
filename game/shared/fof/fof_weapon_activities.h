//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared FoF weapon activity helpers.
//
//=============================================================================//

#ifndef FOF_WEAPON_ACTIVITIES_H
#define FOF_WEAPON_ACTIVITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_activity.h"

class CBaseAnimating;
class CBaseCombatWeapon;

Activity FoFModelActivity(
	const CBaseAnimating *pAnimating,
	const char *pszActivityName,
	Activity fallback );
Activity FoFShotgunReloadStartActivity(
	const CBaseAnimating *pAnimating );
Activity FoFVMReloadActivity( const CBaseAnimating *pAnimating );
Activity FoFVMReloadDeployedActivity(
	const CBaseAnimating *pAnimating );
Activity FoFShotgunReloadFinishActivity(
	const CBaseAnimating *pAnimating );
Activity FoFShotgunPumpActivity( const CBaseAnimating *pAnimating );

#endif // FOF_WEAPON_ACTIVITIES_H
