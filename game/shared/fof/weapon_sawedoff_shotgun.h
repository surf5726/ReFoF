#ifndef WEAPON_SAWEDOFF_SHOTGUN_H
#define WEAPON_SAWEDOFF_SHOTGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CSawedShotgun1 C_SawedShotgun1
#define CSawedShotgun2 C_SawedShotgun2
#endif

class CSawedShotgun1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CSawedShotgun1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSawedShotgun1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.0f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void FinishReload() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	void PerformFoFReload() OVERRIDE;
	bool NeedsPumpAfterShot() const;
	bool UsesLoadedPrimaryInputPath() const OVERRIDE;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};
class CSawedShotgun2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CSawedShotgun2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSawedShotgun2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.0f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void FinishReload() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	void PerformFoFReload() OVERRIDE;
	bool NeedsPumpAfterShot() const;
	bool UsesLoadedPrimaryInputPath() const OVERRIDE;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};

#endif // WEAPON_SAWEDOFF_SHOTGUN_H
