#ifndef WEAPON_COLTNAVY_H
#define WEAPON_COLTNAVY_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CColtNavy1 C_ColtNavy1
#define CColtNavy2 C_ColtNavy2
#endif

class CColtNavy1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CColtNavy1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CColtNavy1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.75f; }
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
class CColtNavy2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CColtNavy2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CColtNavy2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.75f; }
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

#endif // WEAPON_COLTNAVY_H
