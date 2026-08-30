#ifndef WEAPON_WALKER_H
#define WEAPON_WALKER_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CWalker1 C_Walker1
#define CWalker2 C_Walker2
#endif

class CWalker1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CWalker1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWalker1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
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
class CWalker2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CWalker2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWalker2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 5; }
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

#endif // WEAPON_WALKER_H
