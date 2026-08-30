#ifndef WEAPON_MARESLEG_H
#define WEAPON_MARESLEG_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CMaresLeg1 C_MaresLeg1
#define CMaresLeg2 C_MaresLeg2
#endif

class CMaresLeg1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CMaresLeg1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMaresLeg1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.2f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	float FoFProperty378( void ) const OVERRIDE { return 0.35f; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.55f; }
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};
class CMaresLeg2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CMaresLeg2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMaresLeg2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.2f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	float FoFProperty378( void ) const OVERRIDE { return 0.35f; }
	float FoFSightMoveEndpoint( void ) const OVERRIDE { return 0.55f; }
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};

#endif // WEAPON_MARESLEG_H
