//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF ghost entity.
//
//=============================================================================//
#ifndef FOF_GHOST_H
#define FOF_GHOST_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CFoF_Player;

class CBaseGhost : public CBaseAnimating
{
public:
	DECLARE_CLASS( CBaseGhost, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CBaseGhost();

	virtual void Precache();
	virtual void Spawn();
	virtual bool ShouldCollide(
		int collisionGroup, int contentsMask ) const;
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void ImpactTrace(
		trace_t *pTrace, int iDamageType,
		const char *pCustomImpactName = NULL );
	virtual CBaseEntity *GetEnemy( void );
	virtual void UpdateOnRemove();
	virtual bool CreateVPhysics();

	void ApplyFoFGhostGunShot(
		int nDamage, const Vector &vecDirection );
	void GhostThink();
	void SpawnThink();
	void DieThink();
	void GhostTouch( CBaseEntity *pOther );

private:
	bool IsValidTarget( CBaseEntity *pEntity ) const;
	void AlertTarget( CFoF_Player *pTarget );
	bool FindTargetPosition( Vector &vecTarget );
	void ChooseWanderDestination();
	void SteerToward( const Vector &vecTarget, float flWeight );
	void FaceTarget( const Vector &vecTarget );
	void TryAttack();
	void BeginDeath();
	void SpawnRandomWeapon();
	void SetGhostSequence( const char *pszSequence );

	EHANDLE m_hGhostTarget;
	Vector m_vecSteeringVelocity;
	Vector m_vecWanderDestination;
	bool m_bHasWanderDestination;
	float m_flNextTurnTime;
	float m_flFadeTimer;
	float m_flNextAttackTime;
	bool m_bAttackAnimation;
	float m_flAttackStartTime;
	bool m_bWeaponEffectSpawned;
	Vector m_vecWeaponSpawnPosition;
	bool m_bDying;
	float m_flGhostGunForceUntil;
};

#endif // FOF_GHOST_H
