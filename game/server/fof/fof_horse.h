//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF riding horse.
//
//=============================================================================//
#ifndef FOF_HORSE_H
#define FOF_HORSE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CFoF_Player;
class CTriggerDangerTraceEnum;

class CFoF_Horse : public CBaseAnimating
{
public:
	DECLARE_CLASS( CFoF_Horse, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFoF_Horse();
	virtual void Precache();
	virtual void Spawn();
	virtual void UpdateOnRemove();

	bool IsAvailableForMount();
	CFoF_Player *GetRider() const;
	void EnterMountedState( CFoF_Player *pRider );
	void LeaveMountedState();
	void UpdateMountedState( float flRiderYaw, CFoF_Player *pRider );
	void HorseIdle();

private:
	friend class CTriggerDangerTraceEnum;

	void SetHorseSequence( const char *pszSequence, float flPlaybackRate );
	void StopHorseSounds();
	void NotifyDangerTrigger();
	bool UpdateHorseIdleLifecycle();
	void ResetToSpawnPosition();
	void UpdateHorseRaceRemovalTime();

	CHandle< CFoF_Player > m_hRider;
	bool m_bOccupied;
	bool m_bRemovalPending;
	bool m_bHorseRaceMap;
	float m_flMountAvailableTime;
	float m_flPreviousSpeed;
	float m_flCrashUntil;
	float m_flNextIdleAnimation;
	float m_flNextHoofSound;
	float m_flPreviousRiderYaw;
	float m_flDangerResetTime;
	float m_flRemovalTime;
	float m_flNextDangerScan;
	float m_flIdleSince;
	int m_nMoveXPoseParameter;
	int m_nMoveYPoseParameter;
	Vector m_vecSpawnOrigin;
	QAngle m_angSpawnAngles;
	const char *m_pszGallopSound;
};

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF ambient horse NPC.
//
//=============================================================================//

#include "ai_basenpc.h"

class CNPC_Horse : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CNPC_Horse, CAI_BaseNPC );
	DECLARE_DATADESC();

	CNPC_Horse();
	virtual void Precache();
	virtual void Spawn();
	virtual bool CreateVPhysics();
	virtual Class_T Classify();
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual int SelectSchedule();
	virtual int GetSoundInterests();
	virtual void IdleSound();
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual float MaxYawSpeed();
	void InputKick( inputdata_t &inputdata );

	DEFINE_CUSTOM_AI;

private:
	CBasePlayer *FindKickTarget() const;
	void ApplyKick( CBasePlayer *pTarget );

	CHandle< CBaseEntity > m_hKickAttacker;
	bool m_bSaddle;
	float m_flNextAngryTime;
};

#endif // FOF_HORSE_H
