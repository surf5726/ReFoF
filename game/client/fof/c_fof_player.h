#ifndef C_FOF_PLAYER_H
#define C_FOF_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "c_hl2mp_player.h"
#include "hl2mp/hl2mp_playeranimstate.h"

class CNewParticleEffect;

// FoF's player table extends the stock HL2MP player table.  Keeping that base
// is important: the local-player factory still yields a C_HL2MP_Player and the
// stock movement/view code can consume all common player fields.
class C_FoF_Player : public C_HL2MP_Player
{
public:
	DECLARE_CLASS( C_FoF_Player, C_HL2MP_Player );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_FoF_Player();
	virtual ~C_FoF_Player();

	virtual void Spawn( void );
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void FireBullets( const FireBulletsInfo_t &info );
	virtual bool Weapon_Switch(
		C_BaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual void ReceiveMessage( int classID, bf_read &msg );
	virtual void AddEntity( void );
	virtual void Simulate( void );
	virtual CStudioHdr *OnNewModel();
	virtual bool InSameTeam( C_BaseEntity *pEntity );
	virtual bool ShouldDraw();
	virtual const Vector &GetRenderOrigin();
	virtual RenderGroup_t GetRenderGroup();
	virtual int DrawModel( int flags );
#ifdef GLOWS_ENABLE
	virtual void GetGlowEffectColor( float *r, float *g, float *b );
#endif
	virtual bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual bool IsAllowedToSwitchWeapons( void );
	virtual void PostThink( void );
	virtual void ItemPostFrame( void );
	virtual void PlayStepSound(
		Vector &vecOrigin, surfacedata_t *pSurface, float flVolume, bool bForce );
	virtual void StartSprinting( void );
	virtual void StopSprinting( void );
	virtual void StartWalking( void );
	virtual void StopWalking( void );
	virtual bool IsWalking( void );
	virtual void SetObserverTarget( EHANDLE hObserverTarget );
	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	void RecalculateWeaponSpeed();
	int GetFoFInBuyZone() const { return m_nInBuyZone; }
	float GetFoFCaptureInput() const { return m_flCaptureInput; }
	void SetFoFMapIntermission( bool bActive )
	{
		m_bFoFMapIntermission = bActive;
	}
	int GetFoFHandStance() const { return m_nHandStance; }
	int GetFoFPlayerInfo() const { return m_nPlayerInfo; }
	bool IsFoFBotGhost() const { return m_bIsBotGhost; }
	bool IsFoFPotionWeaponLockActive() const
	{
		return ( m_nPlayerInfo & 0x40000 ) != 0;
	}
	bool IsFoFPickupActive() const { return m_bPickupActive; }
	C_BaseEntity *GetFoFAssistingPlayer() const
	{
		return hPlayerAssisted.Get();
	}
	C_BaseEntity *GetFoFKicker() const { return m_hKicker.Get(); }
	QAngle GetFoFHorseAngles() const { return m_horseAngles; }
	void SetFoFHorseAngles( const QAngle &angles )
	{
		m_horseAngles = angles;
		// The server serializes horse yaw in [0, 360).  Keep the locally
		// predicted value in the same representation so crossing zero does not
		// look like a 360-degree prediction error and rewind horse movement.
		m_horseAngles[YAW] = AngleNormalizePositive( m_horseAngles[YAW] );
	}
	float GetFoFSpeedPenalty() const { return m_flFoFSpeedPenalty; }
	void SetFoFSpeedPenalty( float flPenalty ) { m_flFoFSpeedPenalty = flPenalty; }
	float GetFoFSightExpFactor() const { return m_flSightExpFactor; }
	float GetFoFWeaponThrowProgress() const
	{
		return m_flCaptureInput;
	}
	float GetFoFWalkFactor() const { return m_flWalkFactor; }
	bool IsFoFWalking() const;
	float GetFoFWalkSpreadFactor() const { return m_flWalkSpreadFactor; }
	void SetFoFWalkSpreadFactor( float flFactor )
	{
		m_flWalkSpreadFactor = flFactor;
	}
	float GetFoFDrunkness() const { return m_flDrunkness; }
	float GetFoFJailTime() const { return m_flJailTime; }
	float GetFoFUnarmedTime() const { return m_flUnarmedTime; }
	float GetFoFCash() const { return m_flFoFCash; }
	bool IsFoFRawReloadHeld() const { return m_bFoFRawReload; }
	int GetFoFPlayerKills() const { return m_nPlayerKills; }
	int GetFoFLastRoundNotoriety() const { return m_nLastRoundNotoriety; }
	int GetFoFMultiKill() const { return m_nMultiKill; }
	int GetFoFPotionLevel() const { return m_nPotionLevel; }
	float GetFoFCrosshairAperture( int nHand ) const
	{
		return nHand == 1 ? m_flCrosshairAperture2 : m_flCrosshairAperture;
	}
	int GetFoFPlayerAccuracy() const { return m_nPlayerAccuracy; }
	int GetFoFMaxGamesPlayed() const { return m_iFoFPersonalStats[2]; }
	float GetFoFLoadFactor() const;
	float GetFoFLastWallJumpTime() const
	{
		return m_flFoFLastWallJumpTime;
	}
	void SetFoFLastWallJumpTime( float flTime )
	{
		m_flFoFLastWallJumpTime = flTime;
	}
	float BeginFoFDynamiteBeltThrow();
	float GetFoFTargetCrosshairAperture( int nHand ) const
	{
		return nHand == 1 ?
			m_flTargetCrosshairAperture2 : m_flTargetCrosshairAperture;
	}
	void SetFoFSightExpFactor( float flFactor ) { m_flSightExpFactor = flFactor; }
	void SetFoFMeleeActionState( bool bActive )
	{
		if ( bActive )
			m_nPlayerInfo |= 0x10000;
		else
			m_nPlayerInfo &= ~0x10000;
	}
	bool PreventFoFLocalAmmoUse() const { return ( m_nPlayerInfo & 0x40000 ) != 0; }
private:
	void ReceiveFoFPresentationMessage( int nMessageType, bf_read &msg );
	void ReceiveFoFStateMessage( int nMessageType, bf_read &msg );
	bool IsFoFFirstPersonBody() const;
	void ReconcileFoFViewModels( bool bRespawned );
	void ClearFoFCarryPresentation();
	void UpdateFoFCarryPresentation();
	void UpdateFoFCaptureInput();
	void UpdateFoFSightExpansion();
	void UpdateFoFAccuracyAperture();
	void UpdateFoFCommandState();
	void ResetFoFSharedSpawnState();
	void UpdateFoFDrunkness();
	bool CanStartFoFKick();
	void UpdateFoFKick();
	void UpdateFoFLowHealthBlood();
	void UpdateFoFStatAccuracyReport();
	void StopFoFLowHealthBlood();
	int m_nProgression;
	QAngle m_horseAngles;

	// DT_FoFLocalPlayerExclusive008.
	int m_nInBuyZone;
	int m_nPlayerAccuracy;
	float m_flNextAccuracyChange;
	float m_flTargetCrosshairAperture;
	float m_flTargetCrosshairAperture2;
	int m_nFoFPlayerFOV;
	bool m_bSpawnInterpCounter;
	float m_flDrunkness;
	float m_flCaptureInput;
	int m_nHandStance;
	float m_flTimeZoomed;
	float m_flNextPickupInteraction;
	float m_flWalkSpreadFactor;
	int m_nPlHighlight[25];
	int m_nPlTarget[25];
	QAngle vecPropCarryAngles;
	float m_flJailTime;
	int m_nPotionLevel;

	float m_flTransitionSpeed;
	int m_nPlayerInfo;
	bool m_bPickupActive;
	EHANDLE m_hAttachedObject;
	Vector m_attachedPositionObjectSpace;
	QAngle m_attachedAnglesPlayerSpace;
	float m_flFoFSpeedPenalty;
	float m_flSightExpFactor;
	float m_flWalkFactor;
	EHANDLE hPlayerAssisted;
	EHANDLE m_hKicker;
	int m_nMultiKill;
	float m_flUnarmedTime;
	float m_flFoFCash;
	int m_nPlayerKills;
	int m_nLastRoundNotoriety;
	float m_flCrosshairAperture;
	float m_flCrosshairAperture2;
	bool m_bIsBotGhost;

	// Client-only presentation state. It is not part of either FoF RecvTable
	// and therefore cannot alter the server wire ABI.
	// The original server seeds the yellow-dynamite accuracy sample at 5/10
	// on player spawn.  Keeping the two counters on the player (rather than on
	// the weapon entity) also preserves them across ordinary weapon switches.
	int m_nFoFDynamiteBeltAttempts;
	int m_nFoFDynamiteBeltHits;
	CInterpolatedVar< QAngle > m_ivFoFHorseAngles;
	float m_flFoFJumpReleaseTime;
	float m_flFoFLastWallJumpTime;
	// Raw command intent is retained for sight interpolation;
	// the command itself is forwarded unchanged to match the shipped client.
	bool m_bFoFRawAttack2;
	bool m_bFoFRawReload;
	MoveType_t m_nFoFMoveTypeBeforeNetworkUpdate;
	int m_nFoFObserverModeBeforeNetworkUpdate;
	bool m_bFoFServerFrozen;
	bool m_bFoFServerAtControls;
	bool m_bFoFMapIntermission;
	// Reload is edge-triggered even though Source exposes IN_RELOAD as a held
	// button.  Keep one latch per physical press so prediction cannot replay a
	// completed reload sequence on every subsequent command.
	bool m_bFoFReloadPressConsumed;
	bool m_bFoFInitialMessageHandled;
	int m_iFoFPersonalStats[6];
	float m_flFoFRoundPlayTime;
	float m_flFoFSessionPlayTime;
	bool m_bFoFSteamPlayTimeLoaded;
	float m_flFoFNextStatAccuracyReport;
	float m_flFoFImpactHintTime;
	float m_flFoFLastHintTime;
	float m_flFoFHintDelayScale;
	bool m_bFoFScreenshotHandled;
	EHANDLE m_hFoFCarryGib;
	EHANDLE m_hFoFCarrySource;
	float m_flFoFCarryStateInvalidTime;
	bool m_bFoFCarryCancelPending;
	EHANDLE m_hFoFCarryCanceledObject;
	float m_flFoFCarryCanceledInteraction;
	int m_nFoFLastObserverMode;
	bool m_bFoFLastAlive;
	bool m_bFoFRestoreViewModelVisibility;
	float m_flFoFReconcileViewModelsUntil;
	EHANDLE m_hFoFLastActiveWeapon1;
	EHANDLE m_hFoFLastActiveWeapon2;
	CNewParticleEffect *m_pFoFLowHealthBlood;
	int m_iFoFLowHealthBloodAttachment;
	CSmartPtr< CNewParticleEffect > m_pFoFFootsteps[200];
	float m_flFoFFootstepEnd[200];
};

#endif // C_FOF_PLAYER_H
