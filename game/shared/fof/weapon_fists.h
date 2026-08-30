#ifndef FOF_WEAPON_FISTS_H
#define FOF_WEAPON_FISTS_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasebasebludgeon.h"

#ifdef CLIENT_DLL
#define CWeaponFists C_WeaponFists
#endif

class CWeaponFists : public CBaseHL2MPBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponFists, CBaseHL2MPBludgeonWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponFists();
	int FoFWeaponID( void ) const OVERRIDE { return 0; }

	void PrimaryAttack() OVERRIDE;
	void SecondaryAttack() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	float GetRange() OVERRIDE;
	float GetFireRate() OVERRIDE;
	float GetDamageForActivity( Activity activity ) OVERRIDE;
	void AddViewKick() OVERRIDE;

protected:
	void BeginFistSwing( bool bSecondary );
	void ResolveFistHit();
	bool IsGhostFists() const { return false; }

public:
	Activity m_nDelayedActivity;
#ifdef CLIENT_DLL
	float m_flLastAttackTime;
	float m_flLastAttackDuration;
	int nLastActivity;
	float m_flHitDelay;
#else
	CNetworkVar( float, m_flLastAttackTime );
	CNetworkVar( float, m_flLastAttackDuration );
	CNetworkVar( int, nLastActivity );
	CNetworkVar( float, m_flHitDelay );
#endif
};

#endif // FOF_WEAPON_FISTS_H
