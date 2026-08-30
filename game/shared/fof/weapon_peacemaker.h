#ifndef WEAPON_PEACEMAKER_H
#define WEAPON_PEACEMAKER_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CPeacemaker1 C_Peacemaker1
#define CPeacemaker2 C_Peacemaker2
#endif

class CPeacemaker1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CPeacemaker1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CPeacemaker1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};
class CPeacemaker2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CPeacemaker2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CPeacemaker2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};

#endif // WEAPON_PEACEMAKER_H
