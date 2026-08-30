//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CFuncMoveLinear entity.
//
//=============================================================================//
#ifndef C_FUNC_MOVELINEAR_H
#define C_FUNC_MOVELINEAR_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basetoggle.h"

class C_FuncMoveLinear : public C_BaseToggle
{
public:
	DECLARE_CLASS( C_FuncMoveLinear, C_BaseToggle );
	DECLARE_CLIENTCLASS();
};

#endif // C_FUNC_MOVELINEAR_H
