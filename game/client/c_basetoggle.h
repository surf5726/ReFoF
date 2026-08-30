//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CBaseToggle entity.
//
//=============================================================================//
#ifndef C_BASETOGGLE_H
#define C_BASETOGGLE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"

class C_BaseToggle : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_BaseToggle, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_BaseToggle();

protected:
	// FoF's base entity layout is eight bytes wider at this
	// inheritance point. Keep the toggle fields at their observed client ABI
	// offsets without perturbing every C_BaseEntity-derived class.
	unsigned char m_FoFClientLayoutPad[8];
	Vector m_vecFinalDest;
	float flTSpeed;
};

#endif // C_BASETOGGLE_H
