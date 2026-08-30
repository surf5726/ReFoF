#include "cbase.h"
#include "fof/fof_combat_effects.h"
#include "fof/c_fof_player.h"
#include "fof/fof_player_activities.h"
#include "hl2mp/hl2mp_playeranimstate.h"
#include "prediction.h"
#include "tier0/vprof.h"
#include "c_baseviewmodel.h"

#include "tier0/memdbgon.h"

void C_FoF_Player::AddEntity( void )
{
	VPROF_BUDGET(
		"FoF::Player::AddEntity",
		VPROF_BUDGETGROUP_CLIENT_SIM );

	BaseClass::AddEntity();
	UpdateFoFLowHealthBlood();
	if ( this == C_BasePlayer::GetLocalPlayer() )
	{
		// Keep FoF-only presentation repair in the render update.  The original
		// player keeps C_HL2MP_Player::PostDataUpdate unchanged; performing these
		// repairs there adds an extra network-update pass during round refreshes
		// and leaves predicted state one snapshot behind.
		FoFUpdateLocalSleeveFrame();
		ReconcileFoFViewModels( false );
		UpdateFoFCarryPresentation();
	}
}

void C_FoF_Player::DoAnimationEvent(
	PlayerAnimEvent_t event, int nData )
{
	if ( this == C_BasePlayer::GetLocalPlayer() &&
		prediction->InPrediction() &&
		!prediction->IsFirstTimePredicted() )
	{
		return;
	}

	MDLCACHE_CRITICAL_SECTION();
	if ( GetFoFPlayerAnimState() )
		GetFoFPlayerAnimState()->DoAnimationEvent( event, nData );
}

CStudioHdr *C_FoF_Player::OnNewModel()
{
	CStudioHdr *pStudioHdr = BaseClass::OnNewModel();
	if ( GetFoFPlayerAnimState() )
		GetFoFPlayerAnimState()->OnNewModel();
	return pStudioHdr;
}

// First-person weapon state arrives through three independent snapshots:
// the player's two active handles, each viewmodel's weapon/model state, and
// the observer/life-state fields.  A ground pickup or spectator rejoin can
// therefore expose a valid active weapon before its viewmodel is rebound.
// Reconcile only at those transition boundaries; normal weapon animation
// remains owned by prediction and the original server.
void C_FoF_Player::ReconcileFoFViewModels( bool bRespawned )
{
	if ( this != C_BasePlayer::GetLocalPlayer() || !gpGlobals )
		return;

	const int nObserverMode = GetObserverMode();
	const bool bAlive = IsAlive();
	C_BaseCombatWeapon *pFirst = GetActiveWeapon1();
	C_BaseCombatWeapon *pSecond = GetActiveWeapon2();

	const bool bReturnedFromObserver =
		m_nFoFLastObserverMode != OBS_MODE_NONE &&
		nObserverMode == OBS_MODE_NONE;
	const bool bBecameAlive = !m_bFoFLastAlive && bAlive;
	const bool bActiveWeaponsChanged =
		m_hFoFLastActiveWeapon1.Get() != pFirst ||
		m_hFoFLastActiveWeapon2.Get() != pSecond;

	m_nFoFLastObserverMode = nObserverMode;
	m_bFoFLastAlive = bAlive;
	m_hFoFLastActiveWeapon1 = pFirst;
	m_hFoFLastActiveWeapon2 = pSecond;

	if ( bRespawned || bReturnedFromObserver || bBecameAlive )
	{
		m_bFoFRestoreViewModelVisibility = true;
		m_flFoFReconcileViewModelsUntil = gpGlobals->curtime + 2.0f;
	}
	else if ( bActiveWeaponsChanged )
	{
		// A normal pickup/switch commonly delivers the player handle one
		// snapshot before the corresponding viewmodel weapon/model.  Do not
		// manufacture that missing binding locally: SetWeaponModel at this
		// boundary races the authoritative viewmodel update and can leave a
		// second set of arms at the player's feet.  Keep only a short window in
		// which stale bindings are hidden until the real snapshot catches up.
		m_flFoFReconcileViewModelsUntil = MAX(
			m_flFoFReconcileViewModelsUntil,
			gpGlobals->curtime + 0.5f );
	}

	if ( !bAlive || nObserverMode != OBS_MODE_NONE ||
		gpGlobals->curtime > m_flFoFReconcileViewModelsUntil )
	{
		return;
	}

	if ( m_bFoFRestoreViewModelVisibility )
		m_Local.m_bDrawViewmodel = true;

	C_BaseCombatWeapon *pExpected[2] = { NULL, NULL };
	if ( HasDualActiveWeapons() )
	{
		pExpected[0] = pFirst;
		pExpected[1] = pSecond;
	}
	else if ( pFirst )
	{
		pExpected[0] = pFirst;
	}
	else if ( pSecond )
	{
		// The left-hand skill can legitimately leave only physical hand two.
		pExpected[1] = pSecond;
	}

	bool bEveryExpectedModelReady = true;
	for ( int nSlot = 0; nSlot < 2; ++nSlot )
	{
		C_BaseCombatWeapon *pWeapon = pExpected[nSlot];
		C_BaseViewModel *pViewModel = GetViewModel( nSlot, false );
		if ( !pViewModel )
		{
			if ( pWeapon )
				bEveryExpectedModelReady = false;
			continue;
		}

		if ( !pWeapon )
		{
			// The old hand can outlive the active handle for one snapshot.  It is
			// never valid to draw that stale binding during the transition.
			if ( pViewModel->GetWeapon() ||
				pViewModel->GetModelIndex() > 0 )
			{
				pViewModel->AddEffects( EF_NODRAW );
			}
			continue;
		}

		const bool bBindingMatches =
			pViewModel->GetWeapon() == pWeapon &&
			pViewModel->GetModelIndex() > 0;
		if ( !bBindingMatches )
		{
			pViewModel->AddEffects( EF_NODRAW );

			// A respawn or observer return can legitimately arrive without an
			// engine-side deploy.  Only those lifecycle transitions are allowed
			// to repair a missing binding; ordinary acquisition remains entirely
			// authoritative.
			if ( m_bFoFRestoreViewModelVisibility )
			{
				const char *pszViewModel = pWeapon->GetViewModel( nSlot );
				if ( pszViewModel && pszViewModel[0] )
					pViewModel->SetWeaponModel( pszViewModel, pWeapon );
			}
		}

		if ( pViewModel->GetWeapon() != pWeapon ||
			pViewModel->GetModelIndex() <= 0 )
		{
			bEveryExpectedModelReady = false;
			continue;
		}

		// A matching authoritative binding is safe to show regardless of why
		// this reconciliation window was opened.
		pViewModel->RemoveEffects( EF_NODRAW );
	}

	if ( bEveryExpectedModelReady && ( pExpected[0] || pExpected[1] ) )
		m_bFoFRestoreViewModelVisibility = false;
}

