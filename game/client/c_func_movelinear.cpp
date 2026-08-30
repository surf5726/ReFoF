//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for the networked CFuncMoveLinear entity.
//
//=============================================================================//
#include "cbase.h"
#include "c_func_movelinear.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void RecvProxy_MoveLinearVelocity(
	const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pEntity = static_cast< C_BaseEntity * >( pStruct );
	if ( !pEntity )
		return;

	const Vector velocity(
		pData->m_Value.m_Vector[0],
		pData->m_Value.m_Vector[1],
		pData->m_Value.m_Vector[2] );
	pEntity->SetLocalVelocity( velocity );
}

static void RecvProxy_MoveLinearFlags(
	const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pEntity = static_cast< C_BaseEntity * >( pStruct );
	if ( !pEntity )
		return;

	pEntity->RemoveFlag( pEntity->GetFlags() );
	pEntity->AddFlag( pData->m_Value.m_Int );
}

IMPLEMENT_CLIENTCLASS_DT( C_FuncMoveLinear, DT_FuncMoveLinear, CFuncMoveLinear )
	RecvPropVector(
		"m_vecVelocity", 252, sizeof( Vector ), 0,
		RecvProxy_MoveLinearVelocity ),
	RecvPropInt(
		"m_fFlags", 856, sizeof( int ), 0,
		RecvProxy_MoveLinearFlags ),
END_RECV_TABLE()
