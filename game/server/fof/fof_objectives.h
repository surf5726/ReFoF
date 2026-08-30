#ifndef FOF_OBJECTIVES_H
#define FOF_OBJECTIVES_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "basecombatcharacter.h"
#include "basetoggle.h"
#include "GameEventListener.h"
#include "utlvector.h"

class CMobilePoint : public CBaseAnimating
{
public:
	DECLARE_CLASS( CMobilePoint, CBaseAnimating );
	DECLARE_DATADESC();

	CMobilePoint();
	virtual void Precache();
	virtual void Spawn();

	void MoveThink();
	void InputEnable( inputdata_t &inputData );
	void InputDisable( inputdata_t &inputData );

private:
	COutputEvent m_OutputStartMoving;
	COutputEvent m_OutputStopMoving;
	CBasePlayer *m_pOperator;
	bool m_bDisabled;
	bool m_bMoving;
	float m_flLastPushTime;
	float m_flWheelAngle;
	bool m_bReverse;
	Vector m_vecMoveDirection;
	int m_nTeam;
};

class CCapturePoint : public CBaseToggle
{
public:
	DECLARE_CLASS( CCapturePoint, CBaseToggle );
	DECLARE_DATADESC();

	CCapturePoint();
	virtual void Spawn();
	virtual void StartTouch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );
	virtual bool CreateVPhysics();

	void PlayerUpdateThink();
	void InputEnable( inputdata_t &inputData );
	void InputDisable( inputdata_t &inputData );

private:
	void SendCaptureMessage( CBasePlayer *pPlayer,
		float flCurrentCaptureTime, bool bCapturing );

	COutputEvent m_OnPlayerCapture;
	COutputEvent m_OnPlayerCaptureStart;
	COutputEvent m_OnPlayerHalfCapture;
	CUtlVector< EHANDLE > m_hTouchingPlayers;
	int m_nClassFilter;
	float m_flCapture_time;
	int m_nShowProgressBar;
	float m_flCurrentCaptureTime;
	bool m_bActive;
	bool m_bCaptureStarted;
	bool m_bHalfCaptureFired;
};

class CCannonBall : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CCannonBall, CBaseCombatCharacter );
	DECLARE_DATADESC();

	CCannonBall();
	virtual void NetworkStateChanged_m_iAmmo( void ) {}
	virtual void Precache();
	virtual void Spawn();
	virtual bool CreateVPhysics();
	virtual int ObjectCaps();

	void BallTouch( CBaseEntity *pOther );
	void InputLaunch( inputdata_t &inputData );

private:
	void ExplodeBall();

	bool m_bLaunched;
	int m_iDamage;
	int m_iRadius;
	int m_iAttachmentIndex;
	bool m_bDirection;
	float m_fGravity;
	EHANDLE m_hCannonModel;
	string_t m_iszCannonModel;
};

class CFuncTrackTrain;

class CFoFCapEnt : public CBaseAnimating,
	public CGameEventListener
{
public:
	DECLARE_CLASS( CFoFCapEnt, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFoFCapEnt();
	virtual void Precache();
	virtual void Spawn();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void PickUpTouch( CBaseEntity *pOther );
	void CapThink();
	void InputEnableZone( inputdata_t &inputData );
	void InputDisableZone( inputdata_t &inputData );
	void InputSetCapProgressTime( inputdata_t &inputData );

private:
	void UpdateCapture();
	void EnableZone();
	void DisableZone();
	void FireCapZoneEvent() const;
	void FireCapZoneOffEvent() const;
	void SendCaptureMessage( CBasePlayer *pPlayer,
		bool bContested ) const;

	// FoF layout (x86): CGameEventListener occupies 0x464..0x46b,
	// followed by three 0x18-byte outputs. Keep this order exact because
	// the shipped datamap and secondary RTTI vtable expose these offsets.
	COutputEvent m_OnPlayerCapture;
	COutputEvent m_OnZoneEnabled;
	COutputEvent m_OnCapStarted;
	CNetworkVar( int, m_nClassFilter );
	int m_nAnnounceFilter;
	float m_flCaptureTime;
	CNetworkVar( bool, m_bCapActive );
	CNetworkVar( bool, m_bBeingCaptured );
	int m_nRadius;
	int m_nMaxCapturers;
	float m_flCaptureScale;
	float m_flCurrentCaptureTime;
	bool m_bTraceWall;
	CUtlVector< int > m_CapturingPlayers;
	CNetworkVar( float, m_flCapProgress );
	trace_t m_WallTrace;
	float m_flDecrementCaptureRate;
};

class CFoFPushCart : public CBaseAnimating, public CGameEventListener
{
public:
	DECLARE_CLASS( CFoFPushCart, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CFoFPushCart();
	virtual void Spawn();
	virtual void FireGameEvent( IGameEvent *pEvent );

	void CapThink();
	void InputToggleCart( inputdata_t &inputData );

private:
	void ScanForPushers();
	void StartTrain( int nDirection );
	void StopTrain();
	void StartDirectionalAnimation( const char *pszSequence,
		int nDirection );

	string_t m_szTrainName;
	string_t m_szMoveAnim;
	string_t m_szIdleAnim;
	int m_nRadius;
	int m_nVigPushDir;
	int m_nDespPushDir;
	int m_nTotalPushTime;
	bool m_bIdleAnimation;
	float m_flNextScan;
	int m_nAnimationDirection;
	int m_nTrainDirection;
	CHandle< CFuncTrackTrain > m_hTrain;
	CNetworkVar( bool, m_bEnabled );
};

#endif // FOF_OBJECTIVES_H
