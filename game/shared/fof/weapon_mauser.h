#ifndef WEAPON_MAUSER_H
#define WEAPON_MAUSER_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CMauser1 C_Mauser1
#define CMauser2 C_Mauser2
#endif

class CMauser1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CMauser1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMauser1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
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
class CMauser2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CMauser2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CMauser2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 4; }
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

#endif // WEAPON_MAUSER_H
