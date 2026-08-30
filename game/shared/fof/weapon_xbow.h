#ifndef FOF_WEAPON_XBOW_H
#define FOF_WEAPON_XBOW_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponXBow C_WeaponXBow
#endif

class CWeaponXBow : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponXBow, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponXBow();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 0.95f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.5f; }
	int FoFWeaponID( void ) const OVERRIDE { return 1; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.66f; }
	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	bool Reload() OVERRIDE;
	void PrimaryAttack() OVERRIDE;
	void SecondaryAttack() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	bool SendWeaponAnim( int iActivity ) OVERRIDE;
#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent(
		animevent_t *pEvent,
		CBaseCombatCharacter *pOperator ) OVERRIDE;
	void Spawn() OVERRIDE;
#endif

#ifdef CLIENT_DLL
	unsigned char m_FoFClientBowByte;
	bool m_bPressed;
	bool m_bMustReload;
	float m_flThrowPower;
	float m_flArrowChange;
	bool m_bNeedArrowChange;
	int m_nCurrentAmmo;
	int m_nFoFBowTrailingReserved;
#else
	CNetworkVar( bool, m_bPressed );
	CNetworkVar( bool, m_bMustReload );
	unsigned char m_FoFBowAlignmentPadding[2];
	CNetworkVar( float, m_flThrowPower );
	CNetworkVar( float, m_flArrowChange );
	CNetworkVar( bool, m_bNeedArrowChange );
	unsigned char m_FoFBowTrailingPadding[3];
	CNetworkVar( int, m_nCurrentAmmo );
#endif
};
#endif // FOF_WEAPON_XBOW_H
