//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-side FoF thrown and fired projectiles.
//
//=============================================================================//
#ifndef FOF_PROJECTILES_H
#define FOF_PROJECTILES_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatcharacter.h"

bool FoFDefuseDynamite( CBaseEntity *pEntity );

class CAxeBolt : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CAxeBolt, CBaseCombatCharacter );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CAxeBolt();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	void Spawn( void ) OVERRIDE;
	void Precache( void ) OVERRIDE;
	bool CreateVPhysics( void ) OVERRIDE;
	unsigned int PhysicsSolidMaskForEntity() const OVERRIDE;
	void BoltTouch( CBaseEntity *pOther );

	static CAxeBolt *BoltCreate( const Vector &vecOrigin,
		const QAngle &angAngles, int iDamage, CBasePlayer *pOwner );

private:
	int m_iDamage;
	Vector m_vecImpactDirection;
};

class CBowarrowBolt : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CBowarrowBolt, CBaseCombatCharacter );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CBowarrowBolt();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	void Spawn( void ) OVERRIDE;
	void Precache( void ) OVERRIDE;
	bool CreateVPhysics( void ) OVERRIDE;
	unsigned int PhysicsSolidMaskForEntity() const OVERRIDE;
	void BoltTouch( CBaseEntity *pOther );
	void ArrowWhizSoundThink( void );

	static CBowarrowBolt *BoltCreate( const Vector &vecOrigin,
		const QAngle &angAngles, int iDamage, bool bBlack,
		CBasePlayer *pOwner );

private:
	friend class CFoFArrowTriggerEnumerator;

	void CreateTrail( void );
	void StopTrail( void );
	void TraceEnhancementTriggers(
		const Vector &vecDirection, float flDistance );
	void EnableEnhancedDamage( void );

	bool m_bBlack;
	int m_iDamage;
	float m_flNextFlybyTime;
	CHandle< CBaseEntity > m_hTrail;
	bool m_bEnhancedDamage;
	bool m_bTrailCreated;
};

class CKnifeBolt : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CKnifeBolt, CBaseCombatCharacter );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CKnifeBolt();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	void Spawn( void ) OVERRIDE;
	void Precache( void ) OVERRIDE;
	bool CreateVPhysics( void ) OVERRIDE;
	unsigned int PhysicsSolidMaskForEntity() const OVERRIDE;
	void BoltTouch( CBaseEntity *pOther );

	static CKnifeBolt *BoltCreate( const Vector &vecOrigin,
		const QAngle &angAngles, int iDamage, CBasePlayer *pOwner );

private:
	int m_iDamage;
	Vector m_vecImpactDirection;
};

class CXArrow : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CXArrow, CBaseCombatCharacter );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CXArrow();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	void Spawn( void ) OVERRIDE;
	void Precache( void ) OVERRIDE;
	bool CreateVPhysics( void ) OVERRIDE;
	unsigned int PhysicsSolidMaskForEntity() const OVERRIDE;
	void BoltTouch( CBaseEntity *pOther );

	static CXArrow *BoltCreate( const Vector &vecOrigin,
		const QAngle &angAngles, int iDamage, CBasePlayer *pOwner );

private:
	void CreateTrail( void );

	int m_iDamage;
	bool m_bTrailCreated;
};

class CMacheteBolt : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CMacheteBolt, CBaseCombatCharacter );
	DECLARE_DATADESC();

	CMacheteBolt();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	void Spawn( void ) OVERRIDE;
	void Precache( void ) OVERRIDE;
	bool CreateVPhysics( void ) OVERRIDE;
	unsigned int PhysicsSolidMaskForEntity() const OVERRIDE;
	void BoltTouch( CBaseEntity *pOther );

	static CMacheteBolt *BoltCreate( const Vector &vecOrigin,
		const QAngle &angAngles, int iDamage, CBasePlayer *pOwner );

private:
	int m_iDamage;
	Vector m_vecImpactDirection;
};

#endif // FOF_PROJECTILES_H
