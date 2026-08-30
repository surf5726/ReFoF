#ifndef WEAPON_GHOSTGUN_H
#define WEAPON_GHOSTGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CGhostGun1 C_GhostGun1
#define CGhostGun2 C_GhostGun2
#endif

class CGhostGun1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CGhostGun1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CGhostGun1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
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
class CGhostGun2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CGhostGun2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CGhostGun2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
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

#endif // WEAPON_GHOSTGUN_H
