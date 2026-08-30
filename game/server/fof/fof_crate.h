//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Authoritative FoF weapon-crate entity.
//
//=============================================================================//
#ifndef FOF_CRATE_H
#define FOF_CRATE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "GameEventListener.h"

class CFoF_Player;

class FoF_Crate : public CBaseAnimating, public CGameEventListener
{
public:
	DECLARE_CLASS( FoF_Crate, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	FoF_Crate();

	virtual void Precache();
	virtual void Spawn();
	virtual int ObjectCaps();
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller,
		USE_TYPE useType, float flValue );
	virtual void FireGameEvent( IGameEvent *pEvent );

	void OpenThink();
	void InputRestart_Crate( inputdata_t &inputData );
	void DisableForBreakBadRound();
	bool IsReadyForBotUse() const;
	bool IsBeingOpenedBy( const CFoF_Player *pPlayer ) const;

private:
	enum CrateState_t
	{
		CRATE_REGENERATING = 1,
		CRATE_REMOVING = 2,
		CRATE_READY = 3,
		CRATE_OPENING = 4
	};

	void ResetCrate( bool bClose );
	void StartRegeneration();
	void CancelOpen();
	void FinishOpen( CFoF_Player *pPlayer );
	void SendCrateMessage( int nMessageType );
	void SendCrateMessage( int nMessageType, float flValue );
	void SetCrateSequence( const char *pszSequence );
	void ShowCrateMenu( CFoF_Player *pPlayer );
	void AutoPurchaseForBot( CFoF_Player *pPlayer );
	bool IsSpecialCrate();

	CNetworkVar( float, m_flNextRegen );
	CNetworkVar( float, m_flTotalRegenTime );

	int m_nCrateState;
	int m_nCrateTier;
	float m_flOpenCompleteTime;
	float m_flOpeningDuration;
	float m_flRegenerationDuration;
	CHandle< CFoF_Player > m_hUsingPlayer;
	COutputEvent m_OnOpen;
	COutputEvent m_OnClose;
};

#endif // FOF_CRATE_H
