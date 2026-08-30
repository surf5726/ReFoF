#ifndef WEAPON_DERINGER_H
#define WEAPON_DERINGER_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CDeringer1 C_Deringer1
#define CDeringer2 C_Deringer2
#endif

class CDeringer1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CDeringer1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CDeringer1();
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.0f; }
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
class CDeringer2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CDeringer2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CDeringer2();
	float FoFSightExpandRate( void ) const OVERRIDE { return 2.0f; }
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

#endif // WEAPON_DERINGER_H
