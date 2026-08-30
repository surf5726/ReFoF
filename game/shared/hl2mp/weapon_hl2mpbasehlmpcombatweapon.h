//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef WEAPON_BASEHL2MPCOMBATWEAPON_SHARED_H
#define WEAPON_BASEHL2MPCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbase.h"

#if defined( CLIENT_DLL )
#define CBaseHL2MPCombatWeapon C_BaseHL2MPCombatWeapon
#endif

class CBaseHL2MPCombatWeapon : public CWeaponHL2MPBase
{
#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	DECLARE_CLASS( CBaseHL2MPCombatWeapon, CWeaponHL2MPBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseHL2MPCombatWeapon();

	virtual bool	WeaponShouldBeLowered( void );

	virtual bool	Ready( void );
	virtual bool	Lower( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	WeaponIdle( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	virtual Vector	GetBulletSpread( WeaponProficiency_t proficiency );
	virtual float	GetSpreadBias( WeaponProficiency_t proficiency );

	virtual const	WeaponProficiencyInfo_t *GetProficiencyValues();
	static const	WeaponProficiencyInfo_t *GetDefaultProficiencyValues();

	virtual void	ItemHolsterFrame( void );

protected:
	float FoFActionSequenceDuration(
		float flFallbackDuration,
		bool bSetWeaponIdleTime = true );
	bool FoFAutoReloadEnabled( const CBasePlayer *pOwner ) const;
	void FoFEmitPrimaryAttack(
		CBasePlayer *pOwner,
		int nShots,
		float flAutoAimScale,
		float flDamage = 0.0f,
		int iPlayerDamage = 0,
		int nFlags = 0 );
	void FoFEmitReloadAnimationEvent( CBasePlayer *pOwner );
	void FoFPlayDryFire(
		float flFallbackDuration,
		bool bSetWeaponIdleTime = true );
	bool FoFRejectUnderwaterPrimaryAttack( CBasePlayer *pOwner );

#ifdef CLIENT_DLL
	// FoF's SDK2013 client ancestor is 16 bytes larger than this public SDK
	// branch.  Preserve that client-only gap so the real lowered/raise/holster
	// fields, and every FoF-derived member, retain the original x86 layout.
	unsigned char	m_FoFClientLayoutPad[0x10];
#endif

	bool			m_bLowered;			// Whether the viewmodel is raised or lowered
	float			m_flRaiseTime;		// If lowered, the time we should raise the viewmodel
	float			m_flHolsterTime;	// When the weapon was holstered

private:
	
	CBaseHL2MPCombatWeapon( const CBaseHL2MPCombatWeapon & );
};

#endif // WEAPON_BASEHL2MPCOMBATWEAPON_SHARED_H
