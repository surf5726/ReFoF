#ifndef WEAPON_REMINGTON_ARMY_H
#define WEAPON_REMINGTON_ARMY_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CRemington_Army C_Remington_Army
#define CRemington_Army2 C_Remington_Army2
#endif

class CRemington_Army : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CRemington_Army, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CRemington_Army();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.1f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 2.5f; }
	float FoFProperty378( void ) const OVERRIDE { return 0.22f; }
	void FinishReload() OVERRIDE;
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	void PerformFoFReload() OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};
class CRemington_Army2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CRemington_Army2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CRemington_Army2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.1f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 2.5f; }
	float FoFProperty378( void ) const OVERRIDE { return 0.22f; }
	void FinishReload() OVERRIDE;
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	void PerformFoFReload() OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};

#endif // WEAPON_REMINGTON_ARMY_H
