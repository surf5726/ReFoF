#ifndef FOF_WEAPON_SPENCER_H
#define FOF_WEAPON_SPENCER_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponSpencer C_WeaponSpencer
#endif

class CWeaponSpencer : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponSpencer, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSpencer();
	int FoFWeaponWeight( void ) const OVERRIDE { return 7; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.2f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 3; }
	int FoFZoomFOV( void ) const OVERRIDE { return 65; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.75f; }

	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void ItemBusyFrame() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	void ItemHolsterFrame() OVERRIDE;
	void FinishReload() OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;

#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent(
		animevent_t *pEvent,
		CBaseCombatCharacter *pOperator ) OVERRIDE;
#endif

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
#endif // FOF_WEAPON_SPENCER_H
