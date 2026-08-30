#ifndef FOF_BASE_REVOLVER_H
#define FOF_BASE_REVOLVER_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

class CFoF_Player;

enum FoFRevolverPunchProfile_t
{
	FOF_REVOLVER_PUNCH_COLT = 0,
	FOF_REVOLVER_PUNCH_DERINGER,
	FOF_REVOLVER_PUNCH_DERINGER_SECOND,
	FOF_REVOLVER_PUNCH_SHOTGUN,
	FOF_REVOLVER_PUNCH_HAMMERLESS,
	FOF_REVOLVER_PUNCH_MARESLEG,
	FOF_REVOLVER_PUNCH_MAUSER,
	FOF_REVOLVER_PUNCH_PEACEMAKER,
	FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD,
	FOF_REVOLVER_PUNCH_VOLCANIC,
	FOF_REVOLVER_PUNCH_VOLCANIC_SECOND,
	FOF_REVOLVER_PUNCH_WALKER,
	FOF_REVOLVER_PUNCH_PROFILE_COUNT
};

QAngle FoFBuildRevolverPunch(
	FoFRevolverPunchProfile_t profile, bool bSecondary );

#ifdef CLIENT_DLL
#define CFoFBaseRevolver C_FoFBaseRevolver
#endif

class CFoFBaseRevolver : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CFoFBaseRevolver, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CFoFBaseRevolver();

	bool Deploy( void ) OVERRIDE;
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL ) OVERRIDE;
	void ItemPostFrame( void ) OVERRIDE;
	void ItemHolsterFrame( void ) OVERRIDE;
	void WeaponIdle( void ) OVERRIDE;
	bool Reload( void ) OVERRIDE;
	void FinishReload( void ) OVERRIDE;
	void HandleFireOnEmpty( void ) OVERRIDE;
	void PrimaryAttack( void ) OVERRIDE;
	void SecondaryAttack( void ) OVERRIDE;
	float GetFireRate( void ) OVERRIDE;
	int GetSlot( void ) const OVERRIDE;
	acttable_t *ActivityListAlternate( void ) OVERRIDE;
	int ActivityListAlternateCount( void ) OVERRIDE;
	int FoFWeaponID( void ) const OVERRIDE;
	bool WeaponShouldBeLowered( void ) OVERRIDE;
	bool CanDualWield( void ) const OVERRIDE { return true; }
	bool IsSecondGun( void ) const OVERRIDE { return false; }
	bool CanFan( void ) const OVERRIDE { return false; }

#ifndef CLIENT_DLL
	void Operator_HandleAnimEvent( animevent_t *pEvent,
		CBaseCombatCharacter *pOperator ) OVERRIDE;
	void DefaultTouch( CBaseEntity *pOther ) OVERRIDE;
	void ClearFoFThrower( void ) { m_hFoFThrower = NULL; }
#endif

	// Exact CFoFBaseRevolver extension slots 383..393.  Several names remain
	// descriptive until their stripped symbols are recovered, but their return
	// types, defaults, order, and override patterns come from the original DLL.
	virtual float FoFRevolverProperty383( void ) const { return 0.9f; }
	virtual float PrimaryPenalty( void ) const { return 0.5f; }
	virtual float SecondaryPenalty( void ) const { return 1.0f; }
	virtual float FoFRevolverProperty386( void ) const { return 2.0f; }
	virtual bool UsesLoadedPrimaryInputPath( void ) const { return false; }
	virtual QAngle PrimaryViewPunch( void ) const { return QAngle( 0.0f, 0.0f, 0.0f ); }
	virtual QAngle SecondaryViewPunch( void ) const { return QAngle( 0.0f, 0.0f, 0.0f ); }
	virtual float FoFRevolverProperty390( void ) const { return 1.5f; }
	virtual void PerformFoFReload( void );
	virtual bool IsWhiskey( void ) const { return false; }
	virtual acttable_t *FoFRevolverActivityList( void ) { return NULL; }

	// These convenience queries are deliberately non-virtual so the weapon
	// class ABI remains unchanged.
	bool NeedsPumpAfterShot( void ) const;
	int FinishStyle( void ) const;

protected:
	float LastAttackTime() const { return m_flLastAttackTime; }
	void SetLastAttackTime( float flTime ) { m_flLastAttackTime = flTime; }
	CFoFBaseRevolver *GetOtherRevolver( CBasePlayer *pOwner ) const;
	void AlternateSightWeaponIdle();
	void ReloadOneRoundClip();
	void ReloadDeringerClip();
	void ReloadTwoRoundClip();
	void ReloadFullClip();
	void FinishDeringerReload();
	void FinishTwoRoundReload();
	void FinishFullClipReload();

private:
	bool StartReload( int nRequestedHand );
	bool ContinueReload();
	bool SelectReloadHand( CFoF_Player *pOwner, int nRequestedHand );
	bool HasBothRevolvers( const CBasePlayer *pOwner ) const;
	bool HasInventoryPair( const CBasePlayer *pOwner ) const;
	void Pump();
	void DryFire();
	void WaterDryFire();
	void PresentMuzzleFlash( CFoF_Player *pOwner );
	float GetViewModelDuration();
	float ApplyReloadRate( CFoF_Player *pOwner );
	void SetReloadEndTime( CFoF_Player *pOwner );

	// These fields are authoritative on the server and predicted from the same
	// shared state machine on the client. Keep their order wire-compatible.
public:
#ifdef CLIENT_DLL
	float m_flSoonestPrimaryAttack;
	float m_flLastAttackTime;
	float m_flAccuracyPenalty;
	float m_flDeployDualGunIn;
	bool m_bDelayedReload;
	bool m_bNeedPump;
	bool m_bDouble;
	// The shipped weapon layout leaves one non-networked dword between the three
	// state bytes and m_flTriggerHoldTime. Its storage is prediction ABI.
	int m_nFoFRevolverReserved;
	float m_flTriggerHoldTime;
#else
	CHandle< CFoF_Player > m_hFoFThrower;
	CNetworkVar( float, m_flSoonestPrimaryAttack );
	CNetworkVar( float, m_flLastAttackTime );
	CNetworkVar( float, m_flAccuracyPenalty );
	CNetworkVar( float, m_flDeployDualGunIn );
	CNetworkVar( bool, m_bDelayedReload );
	CNetworkVar( bool, m_bNeedPump );
	CNetworkVar( bool, m_bDouble );
	int m_nFoFRevolverReserved;
	CNetworkVar( float, m_flTriggerHoldTime );
#endif
};

#endif // FOF_BASE_REVOLVER_H
