//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CBaseButton entity.
//
//=============================================================================//
#ifndef C_BASEBUTTON_H
#define C_BASEBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basetoggle.h"

class C_BaseButton : public C_BaseToggle
{
public:
	DECLARE_CLASS( C_BaseButton, C_BaseToggle );
	DECLARE_CLIENTCLASS();

	C_BaseButton();

private:
	int m_usable;
};

#endif // C_BASEBUTTON_H
