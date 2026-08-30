//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF-only server trace filters used by player interaction and ballistics.
//
//=============================================================================//
#ifndef FOF_PLAYER_DAMAGE_H
#define FOF_PLAYER_DAMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "util_shared.h"

class CTraceFilterNoOwnerTestDR : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterNoOwnerTestDR, CTraceFilterSimple );

	CTraceFilterNoOwnerTestDR(
		const IHandleEntity *pPassEntity, int collisionGroup );
	bool ShouldHitEntity(
		IHandleEntity *pHandleEntity, int contentsMask ) OVERRIDE;

private:
	const IHandleEntity *m_pPassNotOwner;
};

class CTraceFilterSkipTwoEntitiesFoF : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterSkipTwoEntitiesFoF, CTraceFilterSimple );

	CTraceFilterSkipTwoEntitiesFoF(
		const IHandleEntity *pPassEntity,
		const IHandleEntity *pPassEntity2,
		int collisionGroup );
	bool ShouldHitEntity(
		IHandleEntity *pHandleEntity, int contentsMask ) OVERRIDE;
	virtual void SetPassEntity2(
		const IHandleEntity *pPassEntity2 );

private:
	const IHandleEntity *m_pPassEnt2;
};

#endif // FOF_PLAYER_DAMAGE_H
