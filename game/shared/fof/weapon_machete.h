#ifndef FOF_WEAPON_MACHETE_H
#define FOF_WEAPON_MACHETE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasebasebludgeon.h"

#ifdef CLIENT_DLL
#define CWeaponMachete C_WeaponMachete
#endif

class CWeaponMachete : public CBaseHL2MPBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponMachete, CBaseHL2MPBludgeonWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponMachete();
	int FoFWeaponID( void ) const OVERRIDE { return 8; }

	void PrimaryAttack() OVERRIDE;
	void SecondaryAttack() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	float GetRange() OVERRIDE;
	float GetFireRate() OVERRIDE;
	float GetDamageForActivity( Activity activity ) OVERRIDE;
	void AddViewKick() OVERRIDE;

protected:
	void FoFPrimarySwing();

public:
#ifdef CLIENT_DLL
	float m_flDelayedFire;
	bool m_bShotDelayed;
#else
	CNetworkVar( float, m_flDelayedFire );
	CNetworkVar( bool, m_bShotDelayed );
#endif
};

#endif // FOF_WEAPON_MACHETE_H
