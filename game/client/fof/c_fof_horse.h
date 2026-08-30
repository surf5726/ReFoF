//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for FoF horses.
//
//=============================================================================//
#ifndef C_FOF_HORSE_H
#define C_FOF_HORSE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"

class C_FoF_Horse : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_FoF_Horse, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	virtual void Spawn();
	virtual void UpdateOnRemove();
	virtual bool IsPredicted() const;
};

int FoFClientHorseCount();
C_FoF_Horse *FoFClientHorseAt( int index );

#endif // C_FOF_HORSE_H
