//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF path-corner train used by older objective maps.
//
//=============================================================================//
#ifndef FOF_PHYSICS_ENTITIES_H
#define FOF_PHYSICS_ENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basetoggle.h"

class CSoundPatch;

class CBasePlatTrainFoF : public CBaseToggle
{
public:
	DECLARE_CLASS( CBasePlatTrainFoF, CBaseToggle );
	DECLARE_DATADESC();

	CBasePlatTrainFoF();
	virtual ~CBasePlatTrainFoF();
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Precache();
	virtual bool IsTogglePlat()
	{
		return HasSpawnFlags( 0x0001 );
	}

	void PlayMovingSound();
	void StopMovingSound();

protected:
	string_t m_NoiseMoving;
	string_t m_NoiseArrived;
	CSoundPatch *m_pMovementSound;
	float m_volume;
	float m_flTWidth;
	float m_flTLength;
};

class CFuncTrainFoF : public CBasePlatTrainFoF
{
public:
	DECLARE_CLASS( CFuncTrainFoF, CBasePlatTrainFoF );
	DECLARE_DATADESC();

	CFuncTrainFoF();
	virtual void Spawn();
	virtual void Precache();
	virtual void Activate();
	virtual void OnRestore();
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller,
		USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );

	void InputToggle( inputdata_t &inputData );
	void InputStart( inputdata_t &inputData );
	void InputStop( inputdata_t &inputData );

	void Start();
	void Stop();
	void Next();
	void Wait();

private:
	void SetupTarget();

	EHANDLE m_hCurrentTarget;
	bool m_bActivated;
	EHANDLE m_hEnemy;
	float m_flBlockDamage;
	float m_flNextBlockTime;
	string_t m_iszLastTarget;
};

class CBaseEntity;
class CPhysicsProp;
class IPhysicsObject;

void FoFPreparePhysicsPropMultiplayerSpawn( CPhysicsProp *pProp );
void FoFSetPhysicsPropMultiplayerCallbackFlags(
	CPhysicsProp *pProp, unsigned short nCallbackFlags );
void FoFConfigurePhysicsPropMultiplayerCollision( CPhysicsProp *pProp );
void FoFUpdatePhysicsPropMultiplayer(
	CPhysicsProp *pProp, IPhysicsObject *pPhysics );
bool FoFGetPhysicsPropBounceState(
	CBaseEntity *pEntity, int &nWallBounces, int &nFloorBounces );

#endif // FOF_PHYSICS_ENTITIES_H
