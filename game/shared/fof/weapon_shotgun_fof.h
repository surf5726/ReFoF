#ifndef FOF_WEAPON_SHOTGUN_FOF_H
#define FOF_WEAPON_SHOTGUN_FOF_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponShotgunFoF C_WeaponShotgunFoF
#endif

class CWeaponShotgunFoF : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponShotgunFoF, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponShotgunFoF();
	int FoFWeaponWeight( void ) const OVERRIDE { return 7; }
	int FoFWeaponID( void ) const OVERRIDE { return 10; }

	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	void ItemHolsterFrame() OVERRIDE;
	void FinishReload() OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;

public:
#ifdef CLIENT_DLL
	bool m_bNeedPump;
	bool m_bDelayedFire1;
	bool m_bDelayedFire2;
	bool m_bDelayedReload;
#else
	CNetworkVar( bool, m_bNeedPump );
	CNetworkVar( bool, m_bDelayedFire1 );
	CNetworkVar( bool, m_bDelayedFire2 );
	CNetworkVar( bool, m_bDelayedReload );
#endif

private:
	void ContinueReload( CBasePlayer *pOwner );
	bool StartReload();
	void Pump();
	void DryFire();
};

#endif // FOF_WEAPON_SHOTGUN_FOF_H
