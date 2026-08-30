#ifndef FOF_WEAPON_CARBINE_H
#define FOF_WEAPON_CARBINE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponCarbine C_WeaponCarbine
#endif

class CWeaponCarbine : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponCarbine, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCarbine();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 3; }
	int FoFZoomFOV( void ) const OVERRIDE { return 65; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.7f; }

	bool Deploy() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	void FinishReload() OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;

#ifdef CLIENT_DLL
	bool m_bDelayedFire1;
	bool m_bDelayedFire2;
	bool m_bDelayedReload;
	bool m_bReloaded;
#else
	CNetworkVar( bool, m_bDelayedFire1 );
	CNetworkVar( bool, m_bDelayedFire2 );
	CNetworkVar( bool, m_bDelayedReload );
	CNetworkVar( bool, m_bReloaded );
#endif

private:
	bool StartSingleShotReload();
	bool PlaySingleShotReload(
		CBasePlayer *pOwner,
		bool bLockOwnerUntilComplete );
	void CompleteSingleShotReload( CBasePlayer *pOwner );
	bool HandleActiveReloadFrame( CBasePlayer *pOwner );
};
#endif // FOF_WEAPON_CARBINE_H
