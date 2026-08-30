#ifndef FOF_WEAPON_YELLOWBOY_H
#define FOF_WEAPON_YELLOWBOY_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponYellowboy C_WeaponYellowboy
#endif

class CWeaponYellowboy : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponYellowboy, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponYellowboy();
	int FoFWeaponWeight( void ) const OVERRIDE { return 6; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 3; }

	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void ItemBusyFrame() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	void ItemHolsterFrame() OVERRIDE;
	void FinishReload() OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;

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
	bool StartReload();
	void Pump();
	void DryFire();
};
#endif // FOF_WEAPON_YELLOWBOY_H
