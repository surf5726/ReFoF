#ifndef FOF_BASE_WHISKEY_H
#define FOF_BASE_WHISKEY_H

#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_revolver.h"

#ifdef CLIENT_DLL
#define CBaseWhiskey C_BaseWhiskey
#endif

class CBaseWhiskey : public CFoFBaseRevolver
{
public:
	DECLARE_CLASS( CBaseWhiskey, CFoFBaseRevolver );
	DECLARE_NETWORKCLASS();

	CBaseWhiskey();
	int FoFWeaponID( void ) const OVERRIDE { return 6; }

	void Precache() OVERRIDE;
	void PrimaryAttack() OVERRIDE;
	void SecondaryAttack() OVERRIDE;
	bool IsWhiskey() const OVERRIDE { return true; }
	float PrimaryPenalty() const OVERRIDE { return 0.5f; }
	float SecondaryPenalty() const OVERRIDE { return 1.0f; }

protected:
	void WhiskeyAttack( bool bSecondHand );
};

#endif // FOF_BASE_WHISKEY_H
