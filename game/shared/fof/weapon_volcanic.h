#ifndef WEAPON_VOLCANIC_H
#define WEAPON_VOLCANIC_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CVolcanic1 C_Volcanic1
#define CVolcanic2 C_Volcanic2
#endif

class CVolcanic1 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CVolcanic1, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CVolcanic1();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};
class CVolcanic2 : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CVolcanic2, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CVolcanic2();
	int FoFWeaponWeight( void ) const OVERRIDE { return 3; }
	float FoFSightExpandRate( void ) const OVERRIDE { return 1.5f; }
	float FoFSightContractRate( void ) const OVERRIDE { return 3.0f; }
	void WeaponIdle() OVERRIDE;
	bool IsSecondGun() const OVERRIDE;
	bool CanFan() const OVERRIDE;
	float PrimaryPenalty() const OVERRIDE;
	float SecondaryPenalty() const OVERRIDE;
	bool NeedsPumpAfterShot() const;
	int FinishStyle() const;
	QAngle PrimaryViewPunch() const OVERRIDE;
	QAngle SecondaryViewPunch() const OVERRIDE;
};

#endif // WEAPON_VOLCANIC_H
