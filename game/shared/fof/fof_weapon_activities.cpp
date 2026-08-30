//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared FoF weapon activity helpers.
//
//=============================================================================//

#include "cbase.h"

#include "fof/fof_weapon_activities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Activity FoFModelActivity(
	const CBaseAnimating *pAnimating,
	const char *pszActivityName,
	Activity fallback )
{
	CBaseAnimating *pMutableAnimating =
		const_cast< CBaseAnimating * >( pAnimating );
	if ( !pMutableAnimating || !pMutableAnimating->GetModelPtr() )
		return fallback;

	const int nActivity =
		pMutableAnimating->LookupActivity( pszActivityName );
	return nActivity == ACT_INVALID ? fallback :
		static_cast< Activity >( nActivity );
}

Activity FoFShotgunReloadStartActivity(
	const CBaseAnimating *pAnimating )
{
	return FoFModelActivity(
		pAnimating, "ACT_SHOTGUN_RELOAD_START",
		ACT_SHOTGUN_RELOAD_START );
}

Activity FoFVMReloadActivity( const CBaseAnimating *pAnimating )
{
	return FoFModelActivity(
		pAnimating, "ACT_VM_RELOAD", ACT_VM_RELOAD );
}

Activity FoFVMReloadDeployedActivity(
	const CBaseAnimating *pAnimating )
{
	return FoFModelActivity(
		pAnimating, "ACT_VM_RELOAD_DEPLOYED",
		ACT_VM_RELOAD_DEPLOYED );
}

Activity FoFShotgunReloadFinishActivity(
	const CBaseAnimating *pAnimating )
{
	return FoFModelActivity(
		pAnimating, "ACT_SHOTGUN_RELOAD_FINISH",
		ACT_SHOTGUN_RELOAD_FINISH );
}

Activity FoFShotgunPumpActivity( const CBaseAnimating *pAnimating )
{
	return FoFModelActivity(
		pAnimating, "ACT_SHOTGUN_PUMP", ACT_SHOTGUN_PUMP );
}
