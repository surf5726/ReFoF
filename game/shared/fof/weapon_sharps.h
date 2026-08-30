#ifndef FOF_WEAPON_SHARPS_H
#define FOF_WEAPON_SHARPS_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponSharps1874 C_WeaponSharps1874
#endif

class CWeaponSharps1874 : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponSharps1874, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSharps1874();
	int FoFWeaponWeight( void ) const OVERRIDE { return 8; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.0f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 2.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 3; }
	bool FoFUsesScope( void ) const OVERRIDE { return true; }

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
#endif // FOF_WEAPON_SHARPS_H
