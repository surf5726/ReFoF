#ifndef WEAPON_WHISKEY_H
#define WEAPON_WHISKEY_H

#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_base_whiskey.h"

#ifdef CLIENT_DLL
#define CWhiskey1 C_Whiskey1
#define CWhiskey2 C_Whiskey2
#endif

class CWhiskey1 : public CBaseWhiskey
{
public:
	DECLARE_CLASS( CWhiskey1, CBaseWhiskey );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	bool IsSecondGun() const OVERRIDE;
};

class CWhiskey2 : public CBaseWhiskey
{
public:
	DECLARE_CLASS( CWhiskey2, CBaseWhiskey );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	bool IsSecondGun() const OVERRIDE;
};

#endif // WEAPON_WHISKEY_H
