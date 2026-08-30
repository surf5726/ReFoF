#ifndef FOF_WEAPON_DYNAMITE_H
#define FOF_WEAPON_DYNAMITE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#define CWeaponDynamite C_WeaponDynamite
#ifndef CFoF_Player
#define CFoF_Player C_FoF_Player
#endif
class C_FoF_Player;
class C_BaseViewModel;
#else
class CFoF_Player;
#endif

class CWeaponDynamite : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponDynamite, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponDynamite();
	int FoFWeaponID( void ) const OVERRIDE { return 5; }
	void Precache() OVERRIDE;
	bool Deploy() OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void PrimaryAttack() OVERRIDE;
	void SecondaryAttack() OVERRIDE;
	void ItemPostFrame() OVERRIDE;
	bool Reload() OVERRIDE;
#ifdef CLIENT_DLL
	int DrawModel( int flags ) OVERRIDE;
	void SetDormant( bool bDormant ) OVERRIDE;
	bool OnFireEvent( C_BaseViewModel *pViewModel,
		const Vector &origin, const QAngle &angles,
		int event, const char *options ) OVERRIDE;
#else
	void Operator_HandleAnimEvent( animevent_t *pEvent,
		CBaseCombatCharacter *pOperator ) OVERRIDE;
	bool IsFoFPrimedDeathDynamite() const;
	void SpawnFoFDeathDynamite( CFoF_Player *pOwner,
		CBaseEntity *pThrower, bool bBlastTriggered );
#endif

#ifdef CLIENT_DLL
	bool m_bRedraw;
	int m_AttackPaused;
	bool m_fDrawbackFinished;
	float m_flCountdown;
#else
	CNetworkVar( bool, m_bRedraw );
	unsigned char m_FoFDynamitePadding0[3];
	CNetworkVar( int, m_AttackPaused );
	CNetworkVar( bool, m_fDrawbackFinished );
	unsigned char m_FoFDynamitePadding1[3];
	CNetworkVar( float, m_flCountdown );
#endif
	float m_flReleaseTime;
	float m_flDrawbackFinishTime;
	float m_flRedrawTime;

private:
	bool IsBlackDynamite() const { return false; }
	const char *ProjectileClassname() const { return "dynamite"; }
	Activity PrimaryPullbackActivity() const;
	float PrimaryReleaseDelay() const;
	float PrimaryCountdownDuration() const;
	bool ShouldThrowOnHolster() const;
	bool ShouldReleasePrimaryThrow( CFoF_Player *pOwner ) const;
	bool ShouldDecrementAmmoOnThrow() const;
	void PreparePrimaryPullback( CFoF_Player *pOwner );
	float PrimaryDrawbackPredictionLeadTime() const;
	float ThrowEventPredictionLeadTime() const;
	float RedrawFallbackTime();
	void BeginThrowAnimation( Activity activity, float flEventCycle );
	void CompleteThrow( CFoF_Player *pOwner, int nEvent );
	void ThrowPresentation( CFoF_Player *pOwner );
	void DecrementAmmo( CBaseCombatCharacter *pOwner );
#ifndef CLIENT_DLL
	void SpawnDynamite( CFoF_Player *pOwner, int nEvent );
	void SpawnForcedDropDynamite( CFoF_Player *pOwner );
#endif
};

#endif // FOF_WEAPON_DYNAMITE_H
