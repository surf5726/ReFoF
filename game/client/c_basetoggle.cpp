//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CBaseToggle entity.
//
//=============================================================================//
#include "cbase.h"
#include "c_basetoggle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_BaseToggle::C_BaseToggle()
{
	m_vecFinalDest.Init();
	flTSpeed = 0.0f;
}

IMPLEMENT_CLIENTCLASS_DT( C_BaseToggle, DT_BaseToggle, CBaseToggle )
	RecvPropVector( RECVINFO( m_vecFinalDest ) ),
	RecvPropFloat( RECVINFO( flTSpeed ) ),
END_RECV_TABLE()
