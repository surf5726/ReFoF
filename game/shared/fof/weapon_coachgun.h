#ifndef FOF_WEAPON_COACHGUN_H
#define FOF_WEAPON_COACHGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponCoachgun C_WeaponCoachgun
#endif

class CWeaponCoachgun : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponCoachgun, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCoachgun();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.25f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 4; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.7f; }

	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void ItemBusyFrame() OVERRIDE;
	void ItemHolsterFrame() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	void FinishReload() OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;
public:
#ifdef CLIENT_DLL
	bool m_bDouble;
	bool m_bReloaded;
#else
	CNetworkVar( bool, m_bDouble );
	CNetworkVar( bool, m_bReloaded );
#endif

private:
	bool StartReload();
	void DryFire();
};
#endif // FOF_WEAPON_COACHGUN_H
