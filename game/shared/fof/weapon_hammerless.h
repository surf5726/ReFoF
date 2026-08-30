#ifndef WEAPON_HAMMERLESS_H
#define WEAPON_HAMMERLESS_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CHammerless1 C_Hammerless1
#define CHammerless2 C_Hammerless2
#endif

class CHammerless1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CHammerless1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHammerless1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 2; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.85f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void FinishReload() OVERRIDE;
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
class CHammerless2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CHammerless2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CHammerless2();
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.85f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void FinishReload() OVERRIDE;
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

#endif // WEAPON_HAMMERLESS_H
