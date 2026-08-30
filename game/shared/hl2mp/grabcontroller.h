//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Reusable declaration for the Source player/physcannon grab controller.
//
//=============================================================================//
#ifndef HL2MP_GRABCONTROLLER_H
#define HL2MP_GRABCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "datamap.h"
#include "ehandle.h"
#include "vphysics_interface.h"

class CBaseEntity;
class CBasePlayer;
class CWeaponPhysCannon;

struct game_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
	DECLARE_SIMPLE_DATADESC();
};

class CGrabController : public IMotionEvent
{
public:
	CGrabController( void );
	~CGrabController( void );

	void AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity,
		IPhysicsObject *pPhys, bool bIsMegaPhysCannon,
		const Vector &vGrabPosition, bool bUseGrabPosition );
	void DetachEntity( bool bClearVelocity );
	void OnRestore();

	bool UpdateObject( CBasePlayer *pPlayer, float flError );
	void SetTargetPosition(
		const Vector &target, const QAngle &targetOrientation );
	float ComputeError();
	float GetLoadWeight( void ) const { return m_flLoadWeight; }
	void SetAngleAlignment( float alignAngleCosine )
	{
		m_angleAlignment = alignAngleCosine;
	}
	void SetIgnorePitch( bool bIgnore )
	{
		m_bIgnoreRelativePitch = bIgnore;
	}
	QAngle TransformAnglesToPlayerSpace(
		const QAngle &anglesIn, CBasePlayer *pPlayer );
	QAngle TransformAnglesFromPlayerSpace(
		const QAngle &anglesIn, CBasePlayer *pPlayer );

	CBaseEntity *GetAttached()
	{
		return static_cast< CBaseEntity * >( m_attachedEntity.Get() );
	}

	IMotionEvent::simresult_e Simulate(
		IPhysicsMotionController *pController, IPhysicsObject *pObject,
		float deltaTime, Vector &linear, AngularImpulse &angular );
	float GetSavedMass( IPhysicsObject *pObject );

	QAngle m_attachedAnglesPlayerSpace;
	Vector m_attachedPositionObjectSpace;

private:
	void ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics );

	game_shadowcontrol_params_t m_shadow;
	float m_timeToArrive;
	float m_errorTime;
	float m_error;
	float m_contactAmount;
	float m_angleAlignment;
	bool m_bCarriedEntityBlocksLOS;
	bool m_bIgnoreRelativePitch;

	float m_flLoadWeight;
	float m_savedRotDamping[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	float m_savedMass[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	EHANDLE m_attachedEntity;
	QAngle m_vecPreferredCarryAngles;
	bool m_bHasPreferredCarryAngles;

	IPhysicsMotionController *m_controller;
	int m_frameCount;

	friend class CWeaponPhysCannon;
};

#endif // HL2MP_GRABCONTROLLER_H
