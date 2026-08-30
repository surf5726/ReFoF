#ifndef FOF_PLAYER_H
#define FOF_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2mp_player.h"
#include "hl2mp/grabcontroller.h"
#include "Multiplayer/multiplayer_animstate.h"
#include "tier1/utlvector.h"

class CFoF_Horse;
class CCourseMode;
class CWTraceEnum;
class CHL2MPPlayerAnimState;

class CFoF_Player : public CHL2MP_Player
{
public:
	DECLARE_CLASS( CFoF_Player, CHL2MP_Player );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFoF_Player();
	~CFoF_Player();

	virtual void Precache( void );
	virtual void UpdateOnRemove( void );
	virtual void StopLoopingSounds( void );
	virtual void Spawn( void );
	virtual void PreThink( void );
	virtual void PostThink( void );
	virtual void PlayerDeathThink( void );
	virtual void PlayerUse( void );
	virtual void Touch( CBaseEntity *pOther );
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual bool FoFIsReloading( void );
	virtual int GetFoFTotalNotoriety( void );
	virtual void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	virtual void ImpulseCommands( void );
	virtual void PlayerRunCommand(
		CUserCmd *pUserCmd, IMoveHelper *pMoveHelper );
	virtual void SetPlayerModel( void );
	virtual void TraceAttack(
		const CTakeDamageInfo &info, const Vector &vecDir,
		trace_t *pTrace, CDmgAccumulator *pAccumulator );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void OnDamagedByExplosion( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void ItemPostFrame( void );
	virtual void FireBullets( const FireBulletsInfo_t &info );
	virtual bool Weapon_Switch(
		CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual void Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual void Weapon_Drop(
		CBaseCombatWeapon *pWeapon,
		const Vector *pvecTarget = NULL,
		const Vector *pVelocity = NULL );
	virtual void PlayStepSound(
		Vector &vecOrigin, surfacedata_t *pSurface,
		float flVolume, bool bForce );
	virtual void DeathSound( const CTakeDamageInfo &info );
	virtual bool ClientCommand( const CCommand &args );
	using BaseClass::ChangeTeam;
	virtual void ChangeTeam( int iTeam, bool bDontKill,
		bool bAutoTeam, bool bSilent );
	virtual void GiveAllItems( void );
	virtual void GiveDefaultItems( void );
	CBaseEntity *GiveFoFNamedItem(
		const char *pszName, int iSubType = 0 );
	virtual void ResetScores( void );
	virtual void StartSprinting( void );
	virtual void StopSprinting( void );
	virtual void StartWalking( void );
	virtual void StopWalking( void );
	virtual bool IsWalking( void );
	// CFoF_Player adds these eight slots after the inherited FoF
	// CHL2MP_Player vtable.  The base implementations of the three bot-oriented
	// actions are deliberately empty in the shipped server.
	virtual bool HandleFoFTeamJoin( int nTeam, bool bAutoTeam );
	virtual bool FoFReservedQuery479( void ) const;
	virtual void DelayFoFNextAction( void );
	virtual void ThrowFoFActiveWeapons(
		int nHandSelection, float flPower );
	virtual void RefreshFoFEquipment( void );
	virtual void AddFoFEquipmentFlags( int nFlags );
	virtual void ActivateFoFInvulnerability( void );
	virtual void SelectFoFEquipment( void );
	void SetFoFEquipmentSelection(
		const int *pItems, int nItemCount );
	void ApplyFoFEquipmentSelection( void );
	void RetryPendingFoFSpawn( void );
	void PrepareFoFRoundRespawn( void );
	bool FinalizeFoFSpawn( bool bForce );
	bool FinalizeFoFSpawnAt(
		const Vector &origin, const QAngle &angles );
	void DoAnimationEvent(
		PlayerAnimEvent_t event, int nData = 0 );
	void SendFoFClearViewModelParticles( void );
	void RecalculateWeaponSpeed( void );
	float GetFoFLoadFactor( void ) const;
	float GetFoFLastWallJumpTime( void ) const
	{
		return m_flFoFLastWallJumpTime;
	}
	void SetFoFLastWallJumpTime( float flTime )
	{
		m_flFoFLastWallJumpTime = flTime;
	}
	void SetFoFKicker( CBaseEntity *pKicker );
	CFoF_Horse *GetFoFHorse( void ) const { return m_hFoFHorse.Get(); }
	void DismountFoFHorse( bool bForced );
	void UpdateMountedFoFHorse( void );
	void ApplyFoFGhostGunImpulse(
		CFoF_Player *pSource, const Vector &vecDirection,
		float flDamage, bool bRememberSource );

	void SetFoFBuyZone( int nTier, float flDuration );
	void AddFoFCash( float flCash );
	void AwardFoFCash( float flCash, const char *pszReason );
	void FineFoFCash( int nCash, const char *pszReason );
	void ForceFoFJail( float flReleaseTime );
	int GetFoFInventoryBaseValue( void ) const;
	void SelfDisarmFoFWeapons( void );
	void RestoreFoFUnarmedLoadout( CBaseCombatWeapon *pWeapon = NULL );
	void AddFoFNotoriety( int nAmount, int nReason = 0,
		int nEventCode = 0, const char *pszText = NULL );
	void SetFoFCrateMenuTier( int nTier, CBaseEntity *pSource );
	void ClearFoFCrateMenu( void );
	bool HandleFoFCrateMenuSelection( int nCommandId );
	bool ConsumeFoFWhiskey( int nHealth );
	void AddFoFPotion( int nAmount );
	int GetFoFInBuyZone( void ) const { return m_nInBuyZone; }
	float GetFoFCaptureInput( void ) const { return m_flCaptureInput; }
	int GetFoFHandStance( void ) const { return m_nHandStance; }
	int GetFoFPlayerInfo( void ) const { return m_nPlayerInfo; }
	int GetFoFEquipmentFlags( void ) const
	{
		return m_nFoFEquipmentFlags;
	}
	void ClearFoFEquipmentFlags( void )
	{
		m_nFoFEquipmentFlags = 0;
	}
	void RecordFoFHeavyLoadPhysicsKill( void );
	void ResetFoFHeavyLoadRespawnCycle( void );
	bool PlaysFoFTaunts( void ) const { return m_bFoFPlayTaunts; }
	bool ShowsFoFRifleCrosshair( void ) const
	{
		return m_bFoFRifleCrosshair;
	}
	bool HasFoFFriendOnTeam( void ) const
	{
		return m_bFoFHasFriendOnTeam;
	}
	bool IsFoFPublicEnemy( void ) const
	{
		return ( m_nPlayerInfo & 0x8000 ) != 0;
	}
	void SetFoFPublicEnemy( bool bPublicEnemy )
	{
		if ( bPublicEnemy )
			m_nPlayerInfo |= 0x8000;
		else
			m_nPlayerInfo &= ~0x8000;
	}
	bool IsFoFBattleRoyaleOutlaw( void ) const
	{
		return IsFoFPublicEnemy();
	}
	void SetFoFBattleRoyaleOutlaw( bool bOutlaw )
	{
		SetFoFPublicEnemy( bOutlaw );
	}
	void SetFoFBattleRoyaleRoleAppearance( bool bOutlaw );
	bool IsFoFBotGhost( void ) const { return m_bIsBotGhost; }
	void SetFoFBotGhost( bool bGhost ) { m_bIsBotGhost = bGhost; }
	bool IsFoFPickupActive( void ) const { return m_bPickupActive; }
	CBaseEntity *GetFoFKicker( void ) const { return m_hKicker.Get(); }
	QAngle GetFoFHorseAngles( void ) const { return m_horseAngles; }
	void SetFoFHorseAngles( const QAngle &angles ) { m_horseAngles = angles; }
	float GetFoFSpeedPenalty( void ) const { return m_flFoFSpeedPenalty; }
	void SetFoFSpeedPenalty( float flPenalty )
	{
		m_flFoFSpeedPenalty = flPenalty;
	}
	float GetFoFSightExpFactor( void ) const { return m_flSightExpFactor; }
	float GetFoFWeaponThrowProgress( void ) const { return m_flCaptureInput; }
	float GetFoFWalkFactor( void ) const { return m_flWalkFactor; }
	bool IsFoFWalking( void ) const;
	float GetFoFWalkSpreadFactor( void ) const
	{
		return m_flWalkSpreadFactor;
	}
	void SetFoFWalkSpreadFactor( float flFactor )
	{
		m_flWalkSpreadFactor = flFactor;
	}
	float GetFoFJailTime( void ) const { return m_flJailTime; }
	float GetFoFUnarmedTime( void ) const { return m_flUnarmedTime; }
	void SetFoFUnarmedTime( float flTime ) { m_flUnarmedTime = flTime; }
	float GetFoFCash( void ) const { return m_flFoFCash; }
	float GetFoFLifeStartTime( void ) const { return m_flFoFLifeStartTime; }
	bool IsFoFSpawnEquipmentFinalized( void ) const
	{
		return m_bFoFSpawnEquipmentFinalized;
	}
	int GetFoFCrateMenuTier( void ) const { return m_nFoFCrateMenuTier; }
	float GetFoFLastCrateUseTime( void ) const
	{
		return m_flFoFLastCrateUseTime;
	}
	int GetFoFVotekickMenuState( void ) const
	{
		return m_nFoFVotekickMenuState;
	}
	void SetFoFVotekickMenuState( int nState )
	{
		m_nFoFVotekickMenuState = nState;
	}
	int GetFoFVotekickVotes( void ) const
	{
		return m_FoFVotekickVoters.Count();
	}
	bool HasFoFVotekickVoter( uint32 nAccountID ) const;
	void AddFoFVotekickVoter( uint32 nAccountID );
	void UpdateFoFVotekickNameHistory( void );
	int GetFoFVotekickNameChanges( void ) const
	{
		return m_nFoFVotekickNameChanges;
	}
	float GetFoFVotekickConnectionStartTime( void ) const
	{
		return m_flFoFVotekickConnectionStartTime;
	}
	const char *GetFoFVotekickDisplayName( void )
	{
		return m_szFoFVotekickDisplayName[0] ?
			m_szFoFVotekickDisplayName : GetPlayerName();
	}
	int GetFoFPlayerKills( void ) const { return m_nPlayerKills; }
	void SetFoFPlayerKills( int nKills ) { m_nPlayerKills = nKills; }
	void IncrementFoFPlayerKills( void ) { ++m_nPlayerKills; }
	void ResetFoFPlayerTargets( int nValue )
	{
		for ( int i = 0; i < 25; ++i )
			m_nPlTarget.Set( i, nValue );
	}
	int GetFoFLastRoundNotoriety( void ) const
	{
		return m_nLastRoundNotoriety;
	}
	float GetFoFGlobalRank() const { return m_flFoFGlobalRank; }
	void SetFoFGlobalRank( float flRank ) { m_flFoFGlobalRank = flRank; }
	int GetFoFExperience( void ) const { return m_iFoFExperience; }
	int GetFoFResourceState( void ) const { return m_iFoFResourceState; }
	bool HasFoFProfileData( void ) const
	{
		return m_iFoFProfileLoadState >= 1;
	}
	int GetFoFPersonalStat( int nStat ) const
	{
		return nStat >= 0 && nStat < ARRAYSIZE( m_iFoFPersonalStats ) ?
			m_iFoFPersonalStats[nStat] : 0;
	}
	int GetFoFTeamClass( void ) const { return m_nFoFTeamClass; }
	int GetFoFMultiKill( void ) const { return m_nMultiKill; }
	float GetFoFMapPlayTime( void ) const { return m_flFoFMapPlayTime; }
	int GetFoFBestMultiKill( void ) const { return m_nFoFBestMultiKill; }
	int GetFoFBestDrunkard( void ) const { return m_nFoFBestDrunkard; }
	float GetFoFReportedAccuracy( void ) const;
	float BeginFoFDynamiteBeltThrow();
	void RecordFoFDynamiteBeltHit() { ++m_nFoFDynamiteBeltHits; }
	int GetFoFPlayerAccuracy( void ) const
	{
		return m_nPlayerAccuracy;
	}
	bool IsFoFInvulnerable( void ) const
	{
		return m_flFoFInvulnerableUntil > gpGlobals->curtime;
	}
	bool HasFoFMobileCannonOperatorAccess( void ) const
	{
		return m_bFoFMobileCannonOperatorAccess;
	}
	void GrantFoFMobileCannonOperatorAccess( void )
	{
		m_bFoFMobileCannonOperatorAccess = true;
	}
	float GetFoFNextMobileCannonWarningTime( void ) const
	{
		return m_flFoFNextMobileCannonWarningTime;
	}
	void SetFoFNextMobileCannonWarningTime( float flTime )
	{
		m_flFoFNextMobileCannonWarningTime = flTime;
	}
	void CommitFoFMapStatistics( bool bIntermission );
	void AddFoFDrunkardAmount( int nAmount );
	void RecordFoFCaptureParticipation(
		float flContribution, int nObjectiveDuration );
	void AwardFoFCaptureNotoriety( void );
	int GetFoFPotionLevel( void ) const { return m_nPotionLevel; }
	CBaseEntity *GetFoFVersusSpawn( void ) const
	{
		return m_hFoFVersusSpawn.Get();
	}
	void SetFoFVersusSpawn( CBaseEntity *pSpawn )
	{
		m_hFoFVersusSpawn = pSpawn;
	}
	bool PreventFoFLocalAmmoUse() const
	{
		return ( m_nPlayerInfo & 0x40000 ) != 0;
	}
	float GetFoFCrosshairAperture( int nHand ) const
	{
		return nHand == 1 ?
			static_cast< float >( m_flCrosshairAperture2 ) :
			static_cast< float >( m_flCrosshairAperture );
	}
	void SetFoFMeleeActionState( bool bActive )
	{
		if ( bActive )
			m_nPlayerInfo |= 0x10000;
		else
			m_nPlayerInfo &= ~0x10000;
	}

	// FoF keeps the decoded userinfo value immediately before the
	// replicated progression value.  A zero cache makes Spawn query the
	// client again; bots deliberately leave it at zero and publish 10000.
	int m_nFoFProgressionCache;
	CNetworkVar( int, m_nProgression );
	int GetFoFProgression() const
	{
		return m_nProgression;
	}
	CNetworkQAngle( m_horseAngles );

	// DT_FoFLocalPlayerExclusive008.
	CNetworkVar( int, m_nInBuyZone );
	CNetworkVar( int, m_nPlayerAccuracy );
	CNetworkVar( float, m_flNextAccuracyChange );
	CNetworkVar( float, m_flTargetCrosshairAperture );
	CNetworkVar( float, m_flTargetCrosshairAperture2 );
	CNetworkVar( int, m_nFoFPlayerFOV );
	int GetFoFPlayerFOV( void ) const { return m_nFoFPlayerFOV; }
	void QueueFoFPlayerFOV( int nFOV )
	{
		m_nFoFPendingFOV = clamp( nFOV, 75, 90 );
	}
	CNetworkVar( bool, m_bSpawnInterpCounter );
	CNetworkVar( float, m_flDrunkness );
	CNetworkVar( float, m_flCaptureInput );
	CNetworkVar( int, m_nHandStance );
	CNetworkVar( float, m_flTimeZoomed );
	CNetworkVar( float, m_flNextPickupInteraction );
	CNetworkVar( float, m_flWalkSpreadFactor );
	CNetworkArray( int, m_nPlHighlight, 25 );
	CNetworkArray( int, m_nPlTarget, 25 );
	CNetworkQAngle( vecPropCarryAngles );
	CNetworkVar( float, m_flJailTime );
	CNetworkVar( int, m_nPotionLevel );

	CNetworkVar( float, m_flTransitionSpeed );
	CNetworkVar( int, m_nPlayerInfo );
	CNetworkVar( bool, m_bPickupActive );
	CNetworkHandle( CBaseEntity, m_hAttachedObject );
	CNetworkVector( m_attachedPositionObjectSpace );
	CNetworkQAngle( m_attachedAnglesPlayerSpace );
	CNetworkVar( float, m_flFoFSpeedPenalty );
	CNetworkVar( float, m_flSightExpFactor );
	CNetworkVar( float, m_flWalkFactor );
	CNetworkHandle( CBaseEntity, hPlayerAssisted );
	CNetworkHandle( CBaseEntity, m_hKicker );
	CNetworkVar( int, m_nMultiKill );
	CNetworkVar( float, m_flUnarmedTime );
	CNetworkVar( float, m_flFoFCash );
	CNetworkVar( int, m_nPlayerKills );
	CNetworkVar( int, m_nLastRoundNotoriety );
	CNetworkVar( float, m_flCrosshairAperture );
	CNetworkVar( float, m_flCrosshairAperture2 );
	CNetworkVar( bool, m_bIsBotGhost );

private:
	friend class CCourseMode;
	friend class CWTraceEnum;
	friend void FoFTrackLevelWeaponUse(
		CBaseCombatWeapon *pWeapon, CBasePlayer *pPlayer );
	bool CanStartFoFKick( void );
	void UpdateFoFKick( void );
	void ResetFoFSharedSpawnState( void );
	void ApplyFoFClientPreferences( void );
	void ApplyFoFSpawnFOV( void );
	void UpdateFoFDrunkness( void );
	void UpdateFoFCaptureInput( void );
	void UpdateFoFSightExpansion( void );
	void UpdateFoFAccuracyAperture( void );
	void UpdateFoFCommandState( void );
	void UpdateFoFHandSideSwitch( void );
	bool CanPickupFoFObject( CBaseEntity *pObject );
	bool AttachFoFCarriedObject(
		CBaseEntity *pObject, const Vector &vecGrabPosition );
	void DropFoFCarriedObject( bool bClearVelocity, bool bThrown );
	bool KickFoFCarriedObject( void );
	void UpdateFoFCarry( void );
	CBaseCombatWeapon *ReplaceFoFWeaponHand(
		CBaseCombatWeapon *pWeapon, bool bSwitchNewWeapon );
	void UpdateFoFGhostGunCollision( void );
	void UpdateFoFKickerAttribution( void );
	bool TryFoFUseEntity( CBaseEntity *pEntity );
	bool UpdateFoFUseInteractions( void );
	bool TryMountFoFHorse( void );
	bool FindFoFHorseDismountPosition(
		CFoF_Horse *pHorse, Vector &vecDismountPosition );
	void ActivateFoFPotionReward( void );
	void UpdateFoFPotionReward( void );
	int GetFoFVoiceStyle( void ) const;
	void HandleFoFVoiceCommand( int nBiasedSlot, int nMenuKind );
	void ChangeFoFTeam( int nTeam, bool bDontKill,
		bool bAutoTeam, bool bSilent );
	bool SelectFoFSpawnPoint( bool bForce );
	void SetFoFInvulnerability( float flDuration );
	bool HandleFoFEquipmentCommand( const CCommand &args );
	void FinalizeFoFSpawnEquipment( void );
	void ApplyFoFSpawnEquipment( void );
	void ShowFoFSpawnEquipmentMenuIfNeeded( void );
	void ApplyFoFCashPurchase( bool bCharge );
	void AccumulateFoFNotoriety(
		int nAmount, bool bCombat, bool bExcludeFromCombatTotal );
	void SendFoFNotorietyNotice(
		int nReason, int nAmount, int nEventCode,
		const char *pszText = NULL );
	void ResetFoFDeathScoringState( void );
	void UpdateFoFDeathScoringState( void );
	void UpdateFoFIdleState( void );
	void UpdateFoFTrackingFootsteps( void );
	void UpdateFoFHatAppearance( void );
	void KnockOffFoFHat(
		const CTakeDamageInfo &info, bool bReduceVelocity );
	void SpawnFoFHeavyLoadRespawnProp( void );
	void UpdateFoFCrateMenuRange( void );
	void UpdateFoFHorseRam( void );

	float m_flFoFBuyZoneUntil;
	int m_nFoFPendingFOV;
	CBaseEntity *m_pFoFSpawnPoint;
	CBaseEntity *m_pFoFSpawnSearchStart;
	CBaseEntity *m_pFoFLastSpawnPoint;
	float m_flFoFNextSpawnAttempt;
	int m_nFoFSpawnAttempts;
	bool m_bFoFSpawnPending;
	bool m_bFoFSpawnEquipmentFinalized;
	float m_flFoFNextSpawnEquipmentAttempt;
	int m_nFoFCrateMenuTier;
	CHandle< CBaseEntity > m_hFoFCrateMenuSource;
	float m_flFoFLastCrateUseTime;
	float m_flNextFoFSpectatorTime;
	float m_flNextFoFAutojoinTime;
	float m_flNextFoFVoiceTime;
	bool m_bFoFRifleCrosshair;
	bool m_bFoFPlayTaunts;
	bool m_bFoFHasFriendOnTeam;
	float m_flNextFoFHandSwitchTime;
	int m_iFoFPersonalStats[6];
	int m_nFoFDynamiteBeltAttempts;
	int m_nFoFDynamiteBeltHits;
	float m_flFoFReportedAccuracy;
	float m_flFoFMapPlayTime;
	float m_flFoFCaptureContribution;
	int m_nFoFCaptureObjectiveDuration;
	int m_nFoFBestMultiKill;
	int m_nFoFCurrentDrunkard;
	int m_nFoFBestDrunkard;
	float m_flFoFDamageAccumulated;
	int m_nFoFTeamClass;
	float m_flFoFInvulnerableUntil;
	bool m_bFoFInvulnerabilityOwnsPlayerInfoBit;
	float m_flNextFoFWeaponThrowTime;
	float m_flFoFDeathChainReactionTime;
	int m_nFoFEquipmentFlags;
	float m_flFoFLifeStartTime;
	float m_flFoFStuckTouchTime;
	CGrabController m_FoFGrabController;
	bool m_bFoFResetPickupOwner;
	int m_nFoFHintKeyButton;
	int m_nFoFModelSelection;
	int m_nFoFHatModel;
	bool m_bFoFHatPresent;
	float m_flFoFLastWallJumpTime;
	CHandle< CFoF_Player > m_hFoFGhostGunImpulseSource;
	float m_flFoFGhostGunImpulseTime;
	CHandle< CBaseEntity > m_hFoFGhostGunLastCollision;
	float m_flFoFKickerTime;
	float m_flFoFPotionTickTime;
	float m_flFoFPotionActivateTime;
	float m_flNextFoFBurpTime;
	CHandle< CFoF_Horse > m_hFoFHorse;
	float m_flFoFHorseDismountHold;
	float m_flNextFoFHorseRamTime;
	float m_flNextFoFHorseHintTime;
	int m_iFoFExperience;
	int m_iFoFResourceState;
	int m_iFoFProfileLoadState;
	int m_nFoFTotalNotoriety;
	int m_nFoFCombatNotoriety;
	int m_nFoFAssistDamage;
	float m_flFoFMultiKillAnnounceTime;
	int m_iFoFNemesisKills[MAX_PLAYERS + 1];
	CHandle< CBaseEntity > m_hFoFVersusSpawn;
	CUtlVector< int > m_FoFDeathmatchEquipment;
	bool m_bFoFDeathmatchLoadoutCommitted;
	CUtlVector< int > m_FoFPendingPurchase;
	CUtlVector< int > m_FoFCashEquipment;
	int m_nFoFPurchaseState;
	int m_nFoFVotekickMenuState;
	CUtlVector< uint32 > m_FoFVotekickVoters;
	char m_szFoFVotekickDisplayName[32];
	char m_szFoFVotekickLastName[32];
	int m_nFoFVotekickNameChanges;
	float m_flFoFVotekickConnectionStartTime;
	float m_flFoFTrackingPickupTime;
	Vector m_vecFoFTrackingOrigins[MAX_PLAYERS + 1];
	bool m_bFoFMobileCannonOperatorAccess;
	float m_flFoFNextMobileCannonWarningTime;
	int m_nFoFHeavyLoadPropReserve;
	bool m_bFoFHeavyLoadAlternateSpawn;
	float m_flFoFGlobalRank;
	int m_nFoFBuyZoneTier;
	float m_flFoFSpawnBuyUntil;
};

inline CFoF_Player *ToFoFPlayer( CBaseEntity *pEntity )
{
	return pEntity && pEntity->IsPlayer() ?
		dynamic_cast< CFoF_Player * >( pEntity ) : NULL;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF-specific player activation notifications.
//
//=============================================================================//

class CFoF_Player;
class CBasePlayer;
class CBaseCombatWeapon;
class CUserCmd;
class CHL2MP_Player;

void FoFPreparePlayerConnection( CFoF_Player *pPlayer );
void FoFPlayerActivated( CFoF_Player *pPlayer );

#endif // FOF_PLAYER_H
