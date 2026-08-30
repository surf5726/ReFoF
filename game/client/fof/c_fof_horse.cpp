//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for FoF horses.
//
//=============================================================================//
#include "cbase.h"
#include "fof/c_fof_horse.h"

#include "tier0/memdbgon.h"

static CUtlVector< CHandle< C_FoF_Horse > > s_FoFHorses;

int FoFClientHorseCount()
{
	return s_FoFHorses.Count();
}

C_FoF_Horse *FoFClientHorseAt( int index )
{
	if ( index < 0 || index >= s_FoFHorses.Count() )
		return NULL;

	return s_FoFHorses[index].Get();
}

void C_FoF_Horse::Spawn()
{
	const CHandle< C_FoF_Horse > hHorse( this );
	if ( s_FoFHorses.Find( hHorse ) == s_FoFHorses.InvalidIndex() )
		s_FoFHorses.AddToTail( hHorse );
}

void C_FoF_Horse::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	s_FoFHorses.FindAndRemove( CHandle< C_FoF_Horse >( this ) );
}

bool C_FoF_Horse::IsPredicted() const
{
	return true;
}

static void RecvProxy_HorseLocalOrigin(
	const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pHorse = static_cast< C_BaseEntity * >( pStruct );
	if ( !pHorse )
		return;

	const Vector origin(
		pData->m_Value.m_Vector[0],
		pData->m_Value.m_Vector[1],
		pData->m_Value.m_Vector[2] );
	pHorse->SetLocalOrigin( origin );
}

static void RecvProxy_HorseNetworkAngles(
	const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BaseEntity *pHorse = static_cast< C_BaseEntity * >( pStruct );
	if ( !pHorse )
		return;

	const QAngle angles(
		pData->m_Value.m_Vector[0],
		pData->m_Value.m_Vector[1],
		pData->m_Value.m_Vector[2] );
	// FoF's DT_FoF_Horse entry named m_angRotation targets the
	// C_BaseEntity network-angle slot, like DT_BaseEntity's m_angRotation.
	// Updating the network angle preserves interpolation for remote horses.
	pHorse->SetNetworkAngles( angles );
}

IMPLEMENT_CLIENTCLASS_DT( C_FoF_Horse, DT_FoF_Horse, CFoF_Horse )
	RecvPropVector(
		"m_vecOrigin", 664, sizeof( Vector ), 0,
		RecvProxy_HorseLocalOrigin ),
	RecvPropQAngles(
		"m_angRotation", 844, sizeof( QAngle ), 0,
		RecvProxy_HorseNetworkAngles ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_FoF_Horse )
END_PREDICTION_DATA()
