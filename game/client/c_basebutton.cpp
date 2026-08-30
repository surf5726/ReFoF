//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CBaseButton entity.
//
//=============================================================================//
#include "cbase.h"
#include "c_basebutton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_BaseButton::C_BaseButton()
{
	m_usable = 0;
}

IMPLEMENT_CLIENTCLASS_DT( C_BaseButton, DT_BaseButton, CBaseButton )
	RecvPropInt( RECVINFO( m_usable ) ),
END_RECV_TABLE()
