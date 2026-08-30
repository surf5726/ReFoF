#ifndef WEAPON_SCHOFIELD_H
#define WEAPON_SCHOFIELD_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CSchofield1 C_Schofield1
#define CSchofield2 C_Schofield2
#endif

class CSchofield1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CSchofield1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSchofield1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 2; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.4f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 2.5f; }
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
class CSchofield2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CSchofield2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSchofield2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 2; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.4f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 2.5f; }
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

#endif // WEAPON_SCHOFIELD_H
