#include "cbase.h"
#include "ammodef.h"
#include "decals.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_ballistics.h"
#include "fof/fof_weapon_properties.h"
#include "fof/fof_weapon_activities.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_weapon_parse.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"

#ifdef CLIENT_DLL
#include "c_baseviewmodel.h"
#include "prediction.h"
#else
#include "basegrenade_shared.h"
#include "fof/fof_ghost.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_player_damage.h"
#include "fof/fof_player_statistics.h"
#include "ilagcompensationmanager.h"
#include "ndebugoverlay.h"
#include "te_hl2mp_shotgun_shot.h"

extern ConVar sv_showimpacts;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CFoF_Player::UpdateFoFSightExpansion()
{
	CBaseCombatWeapon *pFirstWeapon = GetActiveWeapon1();
	CBaseCombatWeapon *pSecondWeapon = GetActiveWeapon2();
	CBaseCombatWeapon *pWeapon = pFirstWeapon ?
		pFirstWeapon : pSecondWeapon;
	if ( !pWeapon )
		return;

	float flExpandRate = pWeapon->FoFSightExpandRate();
	float flContractRate = pWeapon->FoFSightContractRate();

	// The original client only enters either sight branch
	// while player-info bit 0x10 advertises that the current loadout can aim.
	// If the gate is closed the received factor is deliberately left alone.
	if ( !( m_nPlayerInfo & 0x10 ) )
		return;

	const bool bPistolSight = pWeapon->CanDualWield();
	// The original helper rejects the single-sidearm sight path when
	// two active handguns are present; attack2 then belongs to the other hand.
	if ( bPistolSight )
	{
		if ( HasDualActiveWeapons() )
			return;

		// The pistol-only path calls CFoF_Player::IsWalking at
		// before it processes IN_ATTACK2.  The original virtual
		// requires both a walk factor above 0.1 and absolute
		// speed above 5 units/s.  Omitting this gate lets a predicted jump
		// keep opening the sight while the server leaves it at zero, which
		// then changes the aperture belonging to either selected hand.
		const bool bFoFWalking = IsFoFWalking();
		if ( bFoFWalking )
			return;
	}

	// Read the button state of the command currently being predicted.  The
	// latest physical RMB state cannot be used here: prediction replays every
	// outstanding command, so one held key would otherwise be assigned to all
	// of them and sight expansion would be advanced repeatedly as latency
	// rises. The original client reads
	// player::m_nButtons for this exact reason.
	const bool bAttack2Held = ( m_nButtons & IN_ATTACK2 ) != 0;
	bool bWantsSight;
	if ( m_nPlayerInfo & 0x200 )
	{
		// The original branch and its pistol twin use bit 0x400 as the
		// server-compatible sight latch. Preserve the exact 0.01/0.50
		// hysteresis so local prediction does not fight the original server.
		if ( bAttack2Held )
		{
			if ( !( m_nPlayerInfo & 0x400 ) &&
				m_flSightExpFactor < 0.01f )
			{
				m_nPlayerInfo |= 0x400;
			}
			else if ( m_flSightExpFactor >= 0.50f )
			{
				m_nPlayerInfo &= ~0x400;
			}
		}
		bWantsSight = ( m_nPlayerInfo & 0x400 ) != 0;
	}
	else
	{
		bWantsSight = bAttack2Held;
	}

	// The local tested by both original sight branches is the return value of
	// original client helper, not the earlier throw-charge temporary that
	// occupied the same stack byte.  That helper returns true when either hand
	// is playing ACT_VM_RELOAD/RELOAD_START/RELOAD_INSERT/RELOAD_FINISH.
	// The original branches then force contraction for the whole reload
	// presentation even while IN_ATTACK2 remains held in the user command.
	const bool bReloadPresentation =
		FoFWeaponHasActualReloadPresentation( pFirstWeapon ) ||
		FoFWeaponHasActualReloadPresentation( pSecondWeapon );
	// The pistol branch checks GetGroundEntity. A sidearm may keep expanding in
	// the air only in
	// hand stance 3; every other stance contracts even while attack2 is held.
	// This check must use the predicted ground handle from the current command,
	// rather than the received walk factor used by the earlier IsWalking gate.
	const bool bPistolSightBlockedInAir =
		bPistolSight && GetGroundEntity() == NULL && m_nHandStance != 3;

	if ( bPistolSight )
	{
		const float flStanceScale =
			( m_nHandStance == 1 || m_nHandStance == 3 ) ?
				0.90f : 0.70f;
		flExpandRate *= flStanceScale;
		flContractRate *= flStanceScale;
	}

	const float flSightBefore = m_flSightExpFactor;
	if ( bWantsSight && !bReloadPresentation && !bPistolSightBlockedInAir )
		m_flSightExpFactor += flExpandRate * gpGlobals->frametime;
	else if ( !bWantsSight || bReloadPresentation || bPistolSightBlockedInAir )
		m_flSightExpFactor -= flContractRate * gpGlobals->frametime;
	m_flSightExpFactor = clamp( m_flSightExpFactor, 0.0f, 1.0f );

	if ( pWeapon->FoFWeaponID() == 3 )
	{
		// The original client reaches the zoom-time write
		// only from the branch which actually advanced sight expansion. Once
		// the factor is clamped at 1.0, control jumps around this block and the
		// timestamp remains the instant at which the transition completed.
		// Updating it unconditionally while sighted makes prediction diverge on
		// every subsequent command.
		if ( m_flSightExpFactor > flSightBefore &&
			m_flSightExpFactor >= 0.9f )
		{
			m_flTimeZoomed = gpGlobals->curtime;
#ifdef CLIENT_DLL
			m_iDefaultFOV = pWeapon->FoFZoomFOV();
#else
			SetDefaultFOV( pWeapon->FoFZoomFOV() );
#endif
		}
		// The matching contraction branch
		// restores the normal FOV only when a contraction step crosses the
		// same threshold.
		else if ( m_flSightExpFactor < flSightBefore &&
			m_flSightExpFactor <= 0.9f )
		{
#ifdef CLIENT_DLL
			m_iDefaultFOV = m_nFoFPlayerFOV;
#else
			SetDefaultFOV( m_nFoFPlayerFOV );
#endif
		}
	}
}

void CFoF_Player::UpdateFoFCaptureInput()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	const bool bCanCapture = FoFWeaponCanChargeThrow( pWeapon );
	const bool bHoldingCapture = ( m_nButtons & IN_ZOOM ) != 0;
	const bool bDual = HasDualActiveWeapons();

	// Every eligible weapon accumulates the input meter.  The equipment skill
	// controls throw power on the server; it does not gate client-side charging.
	if ( bCanCapture && bHoldingCapture )
	{
		const float flCaptureRate = bDual ? 0.5f : 0.8f;
		m_flCaptureInput += flCaptureRate * gpGlobals->frametime;
	}

#ifndef CLIENT_DLL
	// A throw still on cooldown abandons any newly captured command.
	if ( m_flNextFoFWeaponThrowTime > gpGlobals->curtime &&
		m_flCaptureInput != 0.0f )
	{
		m_flCaptureInput = 0.0f;
	}
#endif

	if ( !bCanCapture || m_flCaptureInput <= 0.0f )
		return;

	const bool bPrimary = ( m_nButtons & IN_ATTACK ) != 0;
	const bool bSecondary = ( m_nButtons & IN_ATTACK2 ) != 0;
	bool bReleased = !bHoldingCapture;
	const bool bFullCharge = m_flCaptureInput > 0.85f;
	const float flAttackEnd = gpGlobals->curtime + 0.5f;

#ifndef CLIENT_DLL
	const bool bHasThrowChargeSkill = ( m_nPlayerInfo & 0x40 ) != 0;
	if ( !bHasThrowChargeSkill )
		m_flCaptureInput = 0.01f;

	const float flMaximumThrowPower = bHasThrowChargeSkill ? 9.5f : 1.0f;
	float flThrowPower = bReleased ? 1.0f : RemapValClamped(
		m_flCaptureInput, 0.0f, 0.85f, 1.0f,
		flMaximumThrowPower );

	// A non-dual-wieldable single weapon uses the same immediate action path
	// as a released charge.
	if ( !bDual && pWeapon && !pWeapon->CanDualWield() )
	{
		bReleased = true;
		flThrowPower = 1.0f;
	}
#endif

	if ( !bDual )
	{
		if ( !bPrimary && !bSecondary && !bReleased && !bFullCharge )
			return;

		if ( pWeapon )
		{
			pWeapon->m_flNextSecondaryAttack = flAttackEnd;
			pWeapon->m_flNextPrimaryAttack =
				pWeapon->m_flNextSecondaryAttack;
		}

#ifndef CLIENT_DLL
		if ( pWeapon )
			pWeapon->m_bFireOnEmpty = true;
		CBaseCombatWeapon *pSecondWeapon = GetActiveWeapon2();
		if ( pSecondWeapon )
			pSecondWeapon->m_bFireOnEmpty = true;
		ThrowFoFActiveWeapons( 0, flThrowPower );
#endif
		m_nButtons &= ~IN_ATTACK;
		m_flCaptureInput = 0.0f;
		return;
	}

	CBaseCombatWeapon *pSecondWeapon = GetActiveWeapon2();
	if ( bPrimary || bReleased || bFullCharge )
	{
		if ( pWeapon )
		{
			pWeapon->m_flNextSecondaryAttack = flAttackEnd;
			pWeapon->m_flNextPrimaryAttack =
				pWeapon->m_flNextSecondaryAttack;
		}
		if ( pSecondWeapon )
		{
			pSecondWeapon->m_flNextSecondaryAttack = flAttackEnd;
			pSecondWeapon->m_flNextPrimaryAttack =
				pSecondWeapon->m_flNextSecondaryAttack;
		}
#ifndef CLIENT_DLL
		if ( pWeapon )
			pWeapon->m_bFireOnEmpty = true;
		if ( pSecondWeapon )
			pSecondWeapon->m_bFireOnEmpty = true;
		ThrowFoFActiveWeapons( bFullCharge ? 0 : 2, flThrowPower );
#endif
		m_nButtons &= ~IN_ATTACK;
		m_flCaptureInput = 0.0f;
	}

	if ( bSecondary || bReleased || bFullCharge )
	{
		if ( pWeapon )
		{
			pWeapon->m_flNextSecondaryAttack = flAttackEnd;
			pWeapon->m_flNextPrimaryAttack =
				pWeapon->m_flNextSecondaryAttack;
		}
		if ( pSecondWeapon )
		{
			pSecondWeapon->m_flNextSecondaryAttack = flAttackEnd;
			pSecondWeapon->m_flNextPrimaryAttack =
				pSecondWeapon->m_flNextSecondaryAttack;
		}
#ifndef CLIENT_DLL
		if ( pWeapon )
			pWeapon->m_bFireOnEmpty = true;
		ThrowFoFActiveWeapons( 1, flThrowPower );
#endif
		m_nButtons &= ~IN_ATTACK2;
		m_flCaptureInput = 0.0f;
	}
}

void CFoF_Player::UpdateFoFAccuracyAperture()
{
	CBaseCombatWeapon *pFirst = GetActiveWeapon1();
	CBaseCombatWeapon *pSecond = GetActiveWeapon2();
	if ( !pFirst && !pSecond )
		return;

	CBaseCombatWeapon *pStateWeapon = pFirst ? pFirst : pSecond;
	const int nWeaponID = pStateWeapon->FoFWeaponID();
	const bool bPistolAccuracy = pStateWeapon->CanDualWield();
	const bool bOnGround = GetGroundEntity() != NULL;
	// Read the predicted m_vecVelocity components directly. GetAbsVelocity()
	// can lag that value
	// by one prediction step around starts/stops, which flips accuracy 2 <-> 4
	// and then rewinds both crosshair apertures on each server correction.
	const float flSpeed = GetLocalVelocity().Length();

	// The original code derives the five-value protocol state.
	// A sight value of -1 means that weapon family is not in its aimed
	// classification branch.
	const float flPistolSight =
		bPistolAccuracy && m_flSightExpFactor > 0.75f ?
			m_flSightExpFactor : -1.0f;
	const float flLongGunSight =
		nWeaponID == 3 && m_flSightExpFactor > 0.9f ?
			m_flSightExpFactor : -1.0f;
	const float flCoachSight =
		nWeaponID == 4 && m_flSightExpFactor > 0.9f ?
			m_flSightExpFactor : -1.0f;
	const float flBowSight =
		nWeaponID == 1 && m_flSightExpFactor > 0.9f ?
			m_flSightExpFactor : -1.0f;

	const bool bAirDualAim =
		flPistolSight > 0.75f && m_nHandStance == 3;
	const bool bGroundedSingleAim =
		bOnGround && flPistolSight > 0.75f && m_nHandStance == 1;
	// The shipped client treats sliding and riding as forced locomotion.  In
	// particular, horse acceleration is not an accuracy category: using it as
	// one makes prediction alternate with the server's state on every snapshot.
	const bool bSpecialLocomotion =
		GetFoFSlideForce() > 0.0f || IsOnFoFHorse();

	int nAccuracy = 2; // idle
	if ( !bOnGround && !bAirDualAim )
	{
		nAccuracy = 5; // jump
	}
	else if ( bSpecialLocomotion )
	{
		nAccuracy = 2;
	}
	else
	{
		const bool bCanUseRunSpread =
			flCoachSight < 0.5f &&
			flLongGunSight < 0.5f &&
			flPistolSight <= 0.75f &&
			!bGroundedSingleAim &&
			flBowSight < 0.75f &&
			!bSpecialLocomotion;

		if ( bCanUseRunSpread && flSpeed > 135.0f )
		{
			nAccuracy = 4; // run
		}
		else if ( bSpecialLocomotion || bGroundedSingleAim )
		{
			nAccuracy = 2;
		}
		else if ( !bOnGround && bAirDualAim )
		{
			nAccuracy = 3; // aimed dual stance remains at walk spread in air
		}
		else if ( bOnGround && bAirDualAim )
		{
			nAccuracy = 2;
		}
		else if ( flSpeed > 135.0f && flCoachSight <= 0.5f )
		{
			nAccuracy = 3;
		}
		else if ( flLongGunSight > 0.0f && flSpeed > 50.0f )
		{
			nAccuracy = 3;
		}
		else if ( flBowSight > 0.75f && flSpeed > 50.0f )
		{
			nAccuracy = 3;
		}
		else if ( m_Local.m_bDucked &&
			flCoachSight <= 0.5f && flPistolSight <= 0.5f )
		{
			nAccuracy = 1; // crouch
		}
	}

	// The captured/forced-locomotion bit overrides every movement branch.
	if ( m_nPlayerInfo & 0x40000 )
		nAccuracy = 2;
	m_nPlayerAccuracy = nAccuracy;

	if ( pFirst && pSecond )
	{
		m_flTargetCrosshairAperture =
			FoFWeaponAccuracySpread( this, pFirst, nAccuracy );
		m_flTargetCrosshairAperture2 =
			FoFWeaponAccuracySpread( this, pSecond, nAccuracy );
	}
	else if ( pSecond )
	{
		m_flTargetCrosshairAperture = 0.0f;
		m_flCrosshairAperture = 0.0f;
		m_flTargetCrosshairAperture2 =
			FoFWeaponAccuracySpread( this, pSecond, nAccuracy );
	}
	else
	{
		m_flTargetCrosshairAperture2 = 0.0f;
		m_flCrosshairAperture2 = 0.0f;
		m_flTargetCrosshairAperture =
			FoFWeaponAccuracySpread( this, pFirst, nAccuracy );
	}

	// Sidearms use the dedicated sight remap and update whichever hand owns
	// the sighted weapon. This branch exits before ordinary interpolation in
	// the original.
	CBaseCombatWeapon *pSightWeapon = pFirst ? pFirst : pSecond;
	if ( pSightWeapon && !HasDualActiveWeapons() &&
		pSightWeapon->CanDualWield() &&
		m_flSightExpFactor > 0.01f )
	{
		const float flStanceScale =
			m_nHandStance == 1 ? 0.97f : 1.75f;
		if ( pSightWeapon == pSecond )
		{
			m_flCrosshairAperture2 = RemapValClamped(
				m_flSightExpFactor, 0.01f, 1.0f, 0.15f,
				flStanceScale * m_flTargetCrosshairAperture2 );
		}
		else
		{
			m_flCrosshairAperture = RemapValClamped(
				m_flSightExpFactor, 0.01f, 1.0f, 0.15f,
				flStanceScale * m_flTargetCrosshairAperture );
		}
		return;
	}

	// ID 3 has a separate long-gun presentation: fixed hip aperture, then a
	// 1.0 -> 0.1 multiplier as the sight finishes expanding.
	if ( pFirst && pFirst->FoFWeaponID() == 3 )
	{
		// The original client first converts the sight
		// factor to -1 until an ID-3 long gun has crossed 0.90.  The later
		// subsequent test and remap consume that classified local, not the
		// raw expansion factor.  Feeding the raw value here starts shrinking
		// the aperture halfway through the transition and makes prediction
		// replay a state the original server never produced.
		if ( flLongGunSight > 0.25f )
		{
			m_flCrosshairAperture =
				RemapValClamped(
					flLongGunSight, 0.1f, 1.0f, 1.0f, 0.1f ) *
				m_flTargetCrosshairAperture;
		}
		else
		{
			m_flCrosshairAperture = 0.35f;
		}
		return;
	}

	m_flCrosshairAperture = FoFSmoothCrosshairAperture(
		m_flCrosshairAperture,
		m_flTargetCrosshairAperture,
		FoFWeaponSpreadSpeed( pFirst ) );
	m_flCrosshairAperture2 = FoFSmoothCrosshairAperture(
		m_flCrosshairAperture2,
		m_flTargetCrosshairAperture2,
		FoFWeaponSpreadSpeed( pSecond ) );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared FoF player bullet preparation and deterministic spread,
//          with client prediction and server authority kept in one family.
//
//=============================================================================//

bool FoFPlayersAreEnemies(
	const CFoF_Player *pShooter,
	const CFoF_Player *pTarget )
{
	if ( !pShooter || !pTarget )
		return false;

	// The shipped client/server permit a ghost shooter to affect every player,
	// while a ghost target, the shooter and spectators are never valid enemies.
	if ( pShooter->IsFoFBotGhost() )
		return true;
	if ( pTarget->IsFoFBotGhost() || pTarget == pShooter ||
		pTarget->GetTeamNumber() == TEAM_SPECTATOR )
	{
		return false;
	}

#ifndef CLIENT_DLL
	if ( FoFCourseBotsAreAllied(
		pShooter->GetTeamNumber(), pTarget->GetTeamNumber() ) )
	{
		return false;
	}
#endif

	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	if ( battleRoyale.IsValid() && battleRoyale.GetBool() )
	{
		// Grand Elimination stores its actual two-sided role in player-info
		// bit 15; ordinary Source team numbers do not describe BR alliances.
		return ( ( pShooter->GetFoFPlayerInfo() ^
			pTarget->GetFoFPlayerInfo() ) & 0x8000 ) != 0;
	}

	if ( pShooter->GetTeamNumber() == pTarget->GetTeamNumber() &&
		HL2MPRules() && HL2MPRules()->IsTeamplay() )
	{
		return false;
	}

	return true;
}

static CBaseCombatWeapon *FoFGetBulletWeapon(
	CFoF_Player *pPlayer,
	const FireBulletsInfo_t &info )
{
	if ( !pPlayer )
		return NULL;

	return info.m_bPrimaryAttack ?
		pPlayer->GetActiveWeapon1() : pPlayer->GetActiveWeapon2();
}

static bool FoFIsGhostGunWeapon( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	const char *pszClassname = pWeapon->GetClassname();
	return pszClassname &&
		( !Q_stricmp( pszClassname, "weapon_ghostgun" ) ||
		  !Q_stricmp( pszClassname, "weapon_ghostgun2" ) );
}

static bool FoFPrepareBulletInfo(
	CFoF_Player *pPlayer,
	const FireBulletsInfo_t &sourceInfo,
	CAmmoDef *pAmmoDef,
	CBaseCombatWeapon *pWeapon,
	FireBulletsInfo_t &effectiveInfo )
{
	if ( !pPlayer || !pAmmoDef || !pWeapon ||
		sourceInfo.m_iAmmoType < 0 || sourceInfo.m_iShots < 1 ||
		!pAmmoDef->GetAmmoOfIndex( sourceInfo.m_iAmmoType ) )
	{
		return false;
	}

	effectiveInfo = sourceInfo;
	if ( effectiveInfo.m_flDamage == 0.0f )
	{
		CWeaponHL2MPBase *pHL2MPWeapon =
			dynamic_cast< CWeaponHL2MPBase * >( pWeapon );
		if ( pHL2MPWeapon )
		{
			effectiveInfo.m_flDamage =
				(float)pHL2MPWeapon->GetHL2MPWpnData().m_iPlayerDamage;
			effectiveInfo.m_iPlayerDamage =
				(int)effectiveInfo.m_flDamage;
		}
	}

	// The original server applies the shipped 1.5x damage
	// power-up before any trace.  The same effective value must drive client
	// hit-state prediction and server-authoritative damage.
	if ( ( pPlayer->GetFoFPlayerInfo() & 0x01000000 ) != 0 )
	{
		effectiveInfo.m_flDamage *= 1.5f;
		effectiveInfo.m_iPlayerDamage =
			(int)effectiveInfo.m_flDamage;
	}

	return true;
}

static void FoFUpdateBulletWalkSpread(
	CFoF_Player *pPlayer,
	const FireBulletsInfo_t &sourceInfo )
{
	if ( !pPlayer )
		return;

	const bool bWalking = pPlayer->IsFoFWalking();
	if ( bWalking )
	{
		pPlayer->SetFoFWalkSpreadFactor(
			pPlayer->GetFoFWalkSpreadFactor() + RemapValClamped(
				sourceInfo.m_flDamage, 25.0f, 75.0f, 0.75f, 1.5f ) );
	}
}

static bool FoFUsesPerimeterBulletSpread(
	CFoF_Player *pPlayer,
	CBaseCombatWeapon *pWeapon,
	const FireBulletsInfo_t &info )
{
	if ( !pPlayer || !pWeapon || !pWeapon->CanDualWield() ||
		info.m_iShots != 1 || pPlayer->GetFoFHandStance() == 1 ||
		pPlayer->GetFoFPlayerAccuracy() < 3 )
	{
		return false;
	}

	const int nHand =
		pWeapon == pPlayer->GetActiveWeapon2() &&
		pWeapon != pPlayer->GetActiveWeapon1() ? 1 : 0;
	return pPlayer->GetFoFCrosshairAperture( nHand ) > 0.01f;
}

#ifdef CLIENT_DLL

void CFoF_Player::FireBullets( const FireBulletsInfo_t &info )
{
	// The original client predicts only the local shooter.  Replayed
	// commands must not emit duplicate decals or tracers; remote shots arrive
	// through C_TEHL2MPFireBullets.
	if ( this != C_BasePlayer::GetLocalPlayer() ||
		!prediction->InPrediction() ||
		!prediction->IsFirstTimePredicted() )
	{
		return;
	}

	CAmmoDef *pAmmoDef = GetAmmoDef();
	CBaseCombatWeapon *pWeapon = FoFGetBulletWeapon( this, info );
	FireBulletsInfo_t effectiveInfo;
	if ( !FoFPrepareBulletInfo(
		this, info, pAmmoDef, pWeapon, effectiveInfo ) )
	{
		return;
	}

	// Player-info bit 25 adds the first-person boost particle to this hand's
	// muzzle.  fof_boost_world remains a separate remote presentation message.
	if ( m_nPlayerInfo & 0x02000000 )
	{
		const bool bSecondViewModel =
			pWeapon == GetActiveWeapon2() &&
			pWeapon != GetActiveWeapon1();
		C_BaseViewModel *pViewModel = GetViewModel(
			bSecondViewModel ? 1 : 0, true );
		if ( pViewModel && pViewModel->LookupAttachment( "muzzle" ) > 0 )
		{
			pViewModel->ParticleProp()->Create(
				"fof_boost", PATTACH_POINT_FOLLOW, "muzzle" );
		}
	}

	FoFUpdateBulletWalkSpread( this, info );
	if ( FoFIsGhostGunWeapon( pWeapon ) )
	{
		// The shipped client and server both continue with ordinary bullets at
		// one third damage after the GhostGun's 150-unit impulse trace.
		effectiveInfo.m_flDamage *= 0.33f;
		effectiveInfo.m_iPlayerDamage =
			(int)effectiveInfo.m_flDamage;
	}

	const bool bFixedPattern = FoFAmmoUsesFixedBulletPattern(
		pAmmoDef, effectiveInfo.m_iAmmoType, effectiveInfo.m_iShots );
	const bool bPerimeterSpread = FoFUsesPerimeterBulletSpread(
		this, pWeapon, effectiveInfo );
	CTraceFilterSkipTwoEntities traceFilter(
		this, effectiveInfo.m_pAdditionalIgnoreEnt, COLLISION_GROUP_NONE );
	const int iSeed = CBaseEntity::GetPredictionRandomSeed(
		effectiveInfo.m_bUseServerRandomSeed ) & 255;
	const int iDamageType =
		pAmmoDef->DamageType( effectiveInfo.m_iAmmoType );
	const int iTracerType =
		pAmmoDef->TracerType( effectiveInfo.m_iAmmoType );
	static int s_iTracerCount = 0;

	for ( int iShot = 0; iShot < effectiveInfo.m_iShots; ++iShot )
	{
		const Vector vecDir = FoFComputeBulletDirection(
			effectiveInfo, iShot, iSeed, bFixedPattern,
			bPerimeterSpread );
		const Vector vecTraceSrc = FoFComputeBulletTraceSource(
			effectiveInfo, iShot, bFixedPattern );
		const Vector vecEnd = vecTraceSrc +
			vecDir * effectiveInfo.m_flDistance;
		trace_t tr;
		UTIL_TraceHull(
			vecTraceSrc,
			vecEnd,
			Vector( -5.0f, -5.0f, -5.0f ),
			Vector(  5.0f,  5.0f,  5.0f ),
			MASK_SHOT | CONTENTS_WATER,
			&traceFilter,
			&tr );

		if ( tr.startsolid )
		{
			tr.endpos = tr.startpos;
			tr.fraction = 0.0f;
		}

		if ( tr.fraction < 1.0f )
		{
			pWeapon->DoImpactEffect( tr, iDamageType );

			CFoF_Player *pHitPlayer =
				dynamic_cast< CFoF_Player * >( tr.m_pEnt );
			if ( pHitPlayer &&
				FoFPlayersAreEnemies( this, pHitPlayer ) &&
				( pHitPlayer->GetFoFPlayerInfo() & 0x800 ) == 0 )
			{
				pHitPlayer->SetFoFSpeedPenalty( clamp(
					pHitPlayer->GetFoFSpeedPenalty() +
						effectiveInfo.m_flDamage * 0.4f,
					0.0f,
					100.0f ) );
			}
		}

		if ( effectiveInfo.m_iTracerFreq > 0 &&
			( s_iTracerCount++ % effectiveInfo.m_iTracerFreq ) == 0 &&
			iTracerType != TRACER_NONE )
		{
			pWeapon->MakeTracer(
				effectiveInfo.m_vecSrc, tr, iTracerType );
		}
	}
}

#else

void CFoF_Player::ApplyFoFGhostGunImpulse(
	CFoF_Player *pSource, const Vector &vecDirection,
	float flDamage, bool bRememberSource )
{
	SetAbsVelocity( vec3_origin );

	Vector vecImpulse = vecDirection * ( 15.0f * flDamage );
	if ( GetGroundEntity() )
		vecImpulse.z = 225.0f;
	ApplyAbsVelocityImpulse( vecImpulse );

	if ( bRememberSource )
		m_hFoFGhostGunImpulseSource = pSource;
	m_flFoFGhostGunImpulseTime = gpGlobals->curtime;
}

static bool FoFGhostGunCanAffectPlayer(
	const CFoF_Player *pShooter, const CFoF_Player *pTarget )
{
	if ( !pShooter || !pTarget )
		return false;

	return pShooter->GetTeamNumber() != pTarget->GetTeamNumber() ||
		!HL2MPRules() || !HL2MPRules()->IsTeamplay();
}

static void FoFSendGhostGunMuzzleEffect(
	CFoF_Player *pShooter, CBaseCombatWeapon *pWeapon )
{
	if ( !pShooter || !pWeapon )
		return;

	// Original entity message 13 carries a short local-player index followed
	// by the 32-bit world-weapon index.  The client suppresses the duplicate
	// first-person effect and attaches fof_boost_world for other viewers.
	EntityMessageBegin( pShooter, true );
		WRITE_BYTE( 13 );
		WRITE_SHORT( pShooter->entindex() );
		WRITE_LONG( pWeapon->entindex() );
	MessageEnd();
}

static void FoFApplyGhostGunPhysicsImpulse(
	IPhysicsObject *pPhysicsObject, float flScale,
	const Vector &vecDirection )
{
	if ( !pPhysicsObject )
		return;

	// The original server remaps mass 30..60 to a 250..600-unit
	// impulse, scales it for the current power-up, and adds a random
	// -600..600 angular impulse on every axis.
	const float flMassFactor = clamp(
		( pPhysicsObject->GetMass() - 30.0f ) * ( 1.0f / 30.0f ),
		0.0f, 1.0f );
	const Vector vecVelocity = vecDirection *
		( ( 250.0f + flMassFactor * 350.0f ) * flScale );
	AngularImpulse angularVelocity =
		RandomAngularImpulse( -600.0f, 600.0f );
	pPhysicsObject->AddVelocity( &vecVelocity, &angularVelocity );
}

static void FoFApplyGhostGunBlast(
	CFoF_Player *pShooter, CBaseCombatWeapon *pWeapon,
	FireBulletsInfo_t &info )
{
	if ( !pShooter || !pWeapon )
		return;

	FoFSendGhostGunMuzzleEffect( pShooter, pWeapon );

	const float flTotalDamage =
		(float)info.m_iShots * info.m_flDamage;
	const Vector vecSource = pShooter->Weapon_ShootPosition();
	Vector vecForward;
	pShooter->EyeVectors( &vecForward );
	VectorNormalize( vecForward );

	trace_t tr;
	CTraceFilterSkipTwoEntitiesFoF traceFilter(
		pShooter, NULL, COLLISION_GROUP_NONE );
	UTIL_TraceHull(
		vecSource,
		vecSource + vecForward * 150.0f,
		Vector( -5.0f, -5.0f, -5.0f ),
		Vector(  5.0f,  5.0f,  5.0f ),
		MASK_SHOT | CONTENTS_WATER,
		&traceFilter,
		&tr );

	CBaseEntity *pHitEntity = tr.m_pEnt;
	if ( pHitEntity )
	{
		// An airborne shooter recoils only when the short hull terminates on
		// worldspawn.  Entity hits continue through the ordinary branches.
		if ( !pShooter->GetGroundEntity() && pHitEntity->entindex() == 0 )
		{
			pShooter->ApplyFoFGhostGunImpulse(
				NULL, -vecForward, flTotalDamage * ( 10.0f / 15.0f ),
				false );
		}
		else if ( pHitEntity->IsPlayer() )
		{
			CFoF_Player *pTarget = ToFoFPlayer( pHitEntity );
			if ( FoFGhostGunCanAffectPlayer( pShooter, pTarget ) )
			{
				pTarget->ApplyFoFGhostGunImpulse(
					pShooter, vecForward * 0.5f,
					flTotalDamage, true );
			}
		}
		else if ( pHitEntity->ClassMatches( "fof_ghost" ) ||
			pHitEntity->ClassMatches( "C_BaseGhost" ) )
		{
			CBaseGhost *pGhost = dynamic_cast< CBaseGhost * >( pHitEntity );
			if ( pGhost )
			{
				pGhost->ApplyFoFGhostGunShot(
					(int)flTotalDamage, vecForward );
			}
		}
		else if ( pHitEntity->VPhysicsGetObject() )
		{
			const float flScale =
				( pShooter->GetFoFPlayerInfo() & 0x00800000 ) ?
				7.0f : 4.0f;
			FoFApplyGhostGunPhysicsImpulse(
				pHitEntity->VPhysicsGetObject(), flScale, vecForward );
		}
	}

	// The original query is centered on the short trace endpoint.  Ghost-state
	// players in the narrow forward cone receive a pull toward the shooter.
	for ( CEntitySphereQuery sphere( tr.endpos, 150.0f, 0 );
		CBaseEntity *pEntity = sphere.GetCurrentEntity();
		sphere.NextEntity() )
	{
		if ( pEntity == pShooter || !pEntity->IsPlayer() )
			continue;

		CFoF_Player *pTarget = ToFoFPlayer( pEntity );
		if ( !pTarget ||
			( pTarget->GetFoFPlayerInfo() & 0x800 ) == 0 )
		{
			continue;
		}

		Vector vecToShooter =
			pShooter->EyePosition() - pTarget->EyePosition();
		VectorNormalize( vecToShooter );
		if ( DotProduct( vecForward, vecToShooter ) >= -0.98f )
			continue;
		if ( !FoFGhostGunCanAffectPlayer( pShooter, pTarget ) )
			continue;

		pTarget->ApplyFoFGhostGunImpulse(
			pShooter, vecToShooter, flTotalDamage, true );
	}

	info.m_flDamage *= 0.33f;
	info.m_iPlayerDamage = (int)info.m_flDamage;
}

static void FoFDetonateAimedDynamite( CFoF_Player *pShooter )
{
	static ConVarRef dynamiteShot( "fof_sv_dynamite_shot", true );
	if ( !pShooter ||
		( dynamiteShot.IsValid() && !dynamiteShot.GetBool() ) )
	{
		return;
	}

	Vector vecAim;
	pShooter->EyeVectors( &vecAim );
	VectorNormalize( vecAim );
	const Vector vecShooterOrigin = pShooter->EyePosition();
	static const char *s_pszDynamiteClasses[] =
	{
		"dynamite",
		"dynamite_black",
	};

	for ( int iClass = 0; iClass < ARRAYSIZE( s_pszDynamiteClasses );
		++iClass )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByClassname(
			pEntity, s_pszDynamiteClasses[iClass] ) ) != NULL )
		{
			if ( pEntity->GetMoveType() == MOVETYPE_NONE )
				continue;

			Vector vecToShooter =
				vecShooterOrigin - pEntity->GetAbsOrigin();
			const float flDistance = VectorNormalize( vecToShooter );
			const float flAimDot =
				fabsf( DotProduct( vecAim, vecToShooter ) );
			const float flRequiredDot =
				0.98f + clamp( flDistance / 300.0f,
					0.0f, 1.0f ) * 0.019989967f;
			if ( flAimDot <= flRequiredDot )
				continue;

			CBaseGrenade *pGrenade =
				dynamic_cast< CBaseGrenade * >( pEntity );
			if ( !pGrenade )
				continue;

			pGrenade->SetOwnerEntity( pShooter );
			pGrenade->SetThrower( pShooter );
			pGrenade->m_bHasWarnedAI = true;
			pGrenade->SetThink( &CBaseGrenade::Detonate );
			pGrenade->SetNextThink( gpGlobals->curtime );

			IGameEvent *pEvent =
				gameeventmanager->CreateEvent( "detonator" );
			if ( pEvent )
			{
				pEvent->SetInt(
					"entindex_detonator", pShooter->entindex() );
				gameeventmanager->FireEvent( pEvent );
			}
		}
	}
}

static bool FoFIsBulletBreakable( CBaseEntity *pEntity )
{
	return pEntity &&
		( pEntity->ClassMatches( "func_breakable" ) ||
		  pEntity->ClassMatches( "func_breakable_surf" ) );
}

static bool FoFFindBulletExit(
	const trace_t &entryTrace,
	const Vector &vecDir,
	trace_t &exitTrace,
	float &flExitDepth )
{
	Vector vecOutside;
	flExitDepth = 0.0f;
	do
	{
		flExitDepth += 8.0f;
		vecOutside = entryTrace.endpos + vecDir * flExitDepth;
		if ( ( enginetrace->GetPointContents( vecOutside ) &
			MASK_SHOT_HULL ) == 0 )
		{
			break;
		}
	}
	while ( flExitDepth <= 48.0f );

	if ( flExitDepth > 48.0f )
		return false;

	// The original server first traces back without a filter.  If an
	// unrelated entity is reached before the entry entity, it repeats while
	// skipping that obstruction so the actual back face is recovered.
	UTIL_TraceLine(
		vecOutside,
		entryTrace.endpos,
		MASK_SHOT,
		(CBaseEntity *)NULL,
		COLLISION_GROUP_NONE,
		&exitTrace );
	if ( exitTrace.m_pEnt &&
		exitTrace.m_pEnt != entryTrace.m_pEnt )
	{
		CTraceFilterSimple exitFilter(
			exitTrace.m_pEnt, COLLISION_GROUP_NONE );
		UTIL_TraceLine(
			vecOutside,
			entryTrace.endpos,
			MASK_SHOT,
			&exitFilter,
			&exitTrace );
	}

	return exitTrace.fraction < 1.0f;
}

static float FoFBulletMaterialCost(
	const trace_t &entryTrace,
	float flThickness )
{
	const surfacedata_t *pSurface =
		physprops->GetSurfaceData( entryTrace.surface.surfaceProps );
	if ( !pSurface )
		return flThickness * 1.5f;

	switch ( pSurface->game.material )
	{
	case CHAR_TEX_METAL:
		return flThickness * 8.0f;

	case CHAR_TEX_GLASS:
		return flThickness;

	case CHAR_TEX_WOOD:
		return flThickness * 2.15f;

	case CHAR_TEX_FLESH:
		return clamp( flThickness * 1.85f, 20.0f, 128.0f );

	case CHAR_TEX_DIRT:
		return flThickness * 2.7f;

	case CHAR_TEX_CONCRETE:
		return flThickness * 5.0f;

	case CHAR_TEX_SAND:
		return flThickness * 2.2f;

	default:
		return flThickness * 1.5f;
	}
}

static bool FoFAttenuatePenetratingBullet(
	const trace_t &entryTrace,
	float flMaterialCost,
	float &flDamage )
{
	float flPenetrationScale = 2.0f -
		clamp( ( flDamage * flDamage - 1000.0f ) *
			( 1.0f / 6000.0f ), 0.0f, 1.0f ) * 1.8f;

	// The shipped helper reserves a distance term here.  Its accumulator is
	// zero-initialized and never advanced, so the second multiplier is 1.0.
	// Keep the effective FoF behavior instead of inventing range loss.
	if ( flDamage < 5.0f ||
		( flMaterialCost / flDamage ) * flPenetrationScale > 1.0f )
	{
		return false;
	}

	if ( !FoFIsBulletBreakable( entryTrace.m_pEnt ) &&
		entryTrace.hitgroup != 8 && entryTrace.hitgroup != 10 )
	{
		const float flDamageScale = RemapValClamped(
			flMaterialCost,
			flPenetrationScale * 2.0f,
			flPenetrationScale * 32.0f,
			0.85f,
			0.20f );
		flDamage *= flDamageScale;
	}

	return flDamage > 0.0f;
}

static void FoFSendMetalImpactPresentation(
	CFoF_Player *pShooter, const trace_t &trace )
{
	if ( !pShooter || !physprops )
		return;

	surfacedata_t *pSurface =
		physprops->GetSurfaceData( trace.surface.surfaceProps );
	if ( !pSurface || pSurface->game.material != CHAR_TEX_METAL ||
		random->RandomInt( 0, 100 ) <= 30 )
	{
		return;
	}
	// The original server emits this reliable
	// shooter-entity message for seventy percent of metal impacts.
	EntityMessageBegin( pShooter, true );
		WRITE_BYTE( 18 );
		WRITE_VEC3COORD( trace.endpos );
		WRITE_VEC3NORMAL( trace.plane.normal );
	MessageEnd();
}

static void FoFApplyAuthoritativeBullet(
	CFoF_Player *pShooter,
	CAmmoDef *pAmmoDef,
	const FireBulletsInfo_t &info,
	const Vector &vecDir,
	const Vector &vecInitialTraceSrc,
	bool bFixedPattern,
	CTraceFilterSkipTwoEntitiesFoF &traceFilter )
{
	CBaseEntity *pAttacker = info.m_pAttacker ?
		info.m_pAttacker : pShooter;
	const int nDamageType = pAmmoDef->DamageType( info.m_iAmmoType );
	const int nImpactDamageType = nDamageType | DMG_NEVERGIB;
	float flDamage = info.m_iPlayerDamage != 0 ?
		(float)info.m_iPlayerDamage : info.m_flDamage;
	Vector vecTraceSrc = vecInitialTraceSrc;
	float flTraceDistance = 9999.0f;
	bool bRecordedAccuracyHit = false;

	for ( int iPass = 0; iPass < 20 && flDamage > 0.0f; ++iPass )
	{
		const Vector vecEnd = vecTraceSrc + vecDir * flTraceDistance;
		trace_t tr;
		if ( bFixedPattern &&
			( info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER ) != 0 )
		{
			// The original server gives FoF's fixed
			// pellet ammunition a 0.25-unit sweep.
			UTIL_TraceHull(
				vecTraceSrc,
				vecEnd,
				Vector( -0.25f, -0.25f, -0.25f ),
				Vector(  0.25f,  0.25f,  0.25f ),
				MASK_SHOT,
				&traceFilter,
				&tr );
		}
		else
		{
			UTIL_TraceLine(
				vecTraceSrc,
				vecEnd,
				MASK_SHOT,
				&traceFilter,
				&tr );
		}

		// FoF retains Source's player clipping pass, extending forty units
		// past the world trace so close player hitboxes are not lost.
		UTIL_ClipTraceToPlayers(
			vecTraceSrc,
			tr.endpos + vecDir * 40.0f,
			MASK_SHOT,
			&traceFilter,
			&tr );

		if ( tr.startsolid )
		{
			tr.endpos = tr.startpos;
			tr.fraction = 0.0f;
		}

		CTakeDamageInfo triggerInfo(
			pShooter, pAttacker, info.m_flDamage, nDamageType );
		CalculateBulletDamageForce(
			&triggerInfo, info.m_iAmmoType, vecDir, tr.endpos );
		triggerInfo.ScaleDamageForce( info.m_flDamageForceScale );
		triggerInfo.SetAmmoType( info.m_iAmmoType );
		pShooter->TraceAttackToTriggers(
			triggerInfo, tr.startpos, tr.endpos, vecDir );

		if ( tr.fraction >= 1.0f || !tr.m_pEnt )
			break;

		const int nShowImpacts = sv_showimpacts.GetInt();
		if ( nShowImpacts == 1 || nShowImpacts == 3 )
		{
			NDebugOverlay::Box(
				tr.endpos,
				Vector( -2.0f, -2.0f, -2.0f ),
				Vector( 2.0f, 2.0f, 2.0f ),
				0, 0, 255, 127, 10.0f );

			if ( !pShooter->IsBot() )
			{
				DebugDrawLine(
					vecTraceSrc, tr.endpos,
					0, 0, 255, true, 10.0f );
			}

			CBasePlayer *pHitPlayer = tr.m_pEnt->IsPlayer() ?
				ToBasePlayer( tr.m_pEnt ) : NULL;
			if ( pHitPlayer )
				pHitPlayer->DrawServerHitboxes( 10.0f, true );
		}

		const bool bUnderwater =
			( enginetrace->GetPointContents( tr.endpos ) &
				( CONTENTS_WATER | CONTENTS_SLIME ) ) != 0;
		if ( bUnderwater &&
			( info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER ) )
		{
			break;
		}

		// FoF player hits are presented by TraceAttack's blood/headshot path.
		// Dispatching Source's generic material impact on the player entity adds
		// the HL2MP smoke/spark effect that the shipped result does not show.
		if ( !tr.m_pEnt->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, nImpactDamageType );
			FoFSendMetalImpactPresentation( pShooter, tr );
		}

		const int nCourseMarkerSurfaceMask =
			SURF_SKY | SURF_NODRAW | SURF_HINT | SURF_SKIP;
		if ( !bUnderwater &&
			( tr.surface.flags & nCourseMarkerSurfaceMask ) == 0 &&
			FoFShouldDrawCourseImpactMarker( pShooter ) )
		{
			NDebugOverlay::Cross3D(
				tr.endpos,
				Vector( -2.0f, -2.0f, -2.0f ),
				Vector( 2.0f, 2.0f, 2.0f ),
				0, 0, 255, true, 3.0f );
		}

		CTakeDamageInfo damageInfo(
			pShooter, pAttacker, flDamage, nImpactDamageType );
		pShooter->ModifyFireBulletsDamage( &damageInfo );
		CalculateBulletDamageForce(
			&damageInfo, info.m_iAmmoType, vecDir, tr.endpos );
		damageInfo.ScaleDamageForce( info.m_flDamageForceScale );
		damageInfo.SetAmmoType( info.m_iAmmoType );
		damageInfo.SetPlayerPenetrationCount( iPass > 0 ? 1 : 0 );
		tr.m_pEnt->DispatchTraceAttack( damageInfo, vecDir, &tr );

		// The shipped penetration pass remembers a player hit as the trace
		// filter's second excluded entity.  Without this update, the next pass
		// enters the same player's exit side and applies the attenuated bullet a
		// second time, making body damage depend on hull thickness.
		if ( tr.m_pEnt->IsPlayer() )
			traceFilter.SetPassEntity2( tr.m_pEnt );

		CFoF_Player *pHitPlayer = ToFoFPlayer( tr.m_pEnt );
		if ( pHitPlayer &&
			FoFPlayersAreEnemies( pShooter, pHitPlayer ) &&
			( pHitPlayer->GetFoFPlayerInfo() & 0x800 ) == 0 )
		{
			if ( !bRecordedAccuracyHit )
			{
				FoFRecordAccuracyHit( pShooter, flDamage );
				bRecordedAccuracyHit = true;
			}
			pHitPlayer->SetFoFSpeedPenalty( clamp(
				pHitPlayer->GetFoFSpeedPenalty() + flDamage * 0.4f,
				0.0f,
				100.0f ) );
		}

		trace_t exitTrace;
		float flExitDepth = 0.0f;
		if ( !FoFFindBulletExit(
			tr, vecDir, exitTrace, flExitDepth ) )
		{
			break;
		}

		const float flThickness =
			( exitTrace.endpos - tr.endpos ).Length();
		const float flMaterialCost =
			FoFBulletMaterialCost( tr, flThickness );
		if ( !FoFAttenuatePenetratingBullet(
			tr, flMaterialCost, flDamage ) )
		{
			break;
		}

		UTIL_ImpactTrace( &exitTrace, nImpactDamageType );

		// Preserve FoF's recovered range update exactly.  It adds the
		// distance to the entry face, then subtracts the 8-unit exit scan.
		flTraceDistance += ( tr.startpos - tr.endpos ).Length();
		flTraceDistance -= flExitDepth;
		vecTraceSrc = exitTrace.endpos;
	}
}

void CFoF_Player::FireBullets( const FireBulletsInfo_t &info )
{
	CAmmoDef *pAmmoDef = GetAmmoDef();
	CBaseCombatWeapon *pWeapon = FoFGetBulletWeapon( this, info );
	FireBulletsInfo_t effectiveInfo;
	if ( !FoFPrepareBulletInfo(
		this, info, pAmmoDef, pWeapon, effectiveInfo ) )
	{
		return;
	}

	lagcompensation->StartLagCompensation(
		this, GetCurrentCommand() );
	NoteWeaponFired();
	FoFReportCourseStat( "shot_fired", this );
	FoFUpdateBulletWalkSpread( this, info );

	if ( g_MultiDamage.GetTarget() != NULL )
		ApplyMultiDamage();
	ClearMultiDamage();
	g_MultiDamage.SetDamageType(
		pAmmoDef->DamageType( effectiveInfo.m_iAmmoType ) |
		DMG_NEVERGIB );

	const bool bFixedPattern = FoFAmmoUsesFixedBulletPattern(
		pAmmoDef, effectiveInfo.m_iAmmoType, effectiveInfo.m_iShots );
	const bool bPerimeterSpread = FoFUsesPerimeterBulletSpread(
		this, pWeapon, effectiveInfo );
	const int iSeed = CBaseEntity::GetPredictionRandomSeed(
		effectiveInfo.m_bUseServerRandomSeed ) & 255;
	if ( FoFIsGhostGunWeapon( pWeapon ) )
		FoFApplyGhostGunBlast( this, pWeapon, effectiveInfo );
	FoFDetonateAimedDynamite( this );
	FoFRecordAccuracyShots( this, effectiveInfo.m_iShots );

	for ( int iShot = 0; iShot < effectiveInfo.m_iShots; ++iShot )
	{
		CTraceFilterSkipTwoEntitiesFoF traceFilter(
			this, effectiveInfo.m_pAdditionalIgnoreEnt,
			COLLISION_GROUP_NONE );
		const Vector vecDir = FoFComputeBulletDirection(
			effectiveInfo, iShot, iSeed, bFixedPattern,
			bPerimeterSpread );
		const Vector vecTraceSrc = FoFComputeBulletTraceSource(
			effectiveInfo, iShot, bFixedPattern );
		FoFApplyAuthoritativeBullet(
			this,
			pAmmoDef,
			effectiveInfo,
			vecDir,
			vecTraceSrc,
			bFixedPattern,
			traceFilter );
	}

	ApplyMultiDamage();

	// Emit one event for the complete shot after all authoritative traces and
	// preserve the dual-wield hand selector.  The server enables both remote
	// tracers and client-side impact effects here.
	TE_HL2MPFireBullets(
		entindex(),
		effectiveInfo.m_vecSrc,
		effectiveInfo.m_vecDirShooting,
		effectiveInfo.m_iAmmoType,
		iSeed,
		effectiveInfo.m_iShots,
		effectiveInfo.m_vecSpread.x,
		true,
		true,
		effectiveInfo.m_bPrimaryAttack );

	lagcompensation->FinishLagCompensation( this );
}

#endif

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF shared predicted kick state and server-authoritative impact.
//
//=============================================================================//

#ifdef CLIENT_DLL
static void FoFApplyKickImpact( CFoF_Player *pAttacker )
{
	if ( !pAttacker )
		return;

	Vector vecForward;
	pAttacker->EyeVectors( &vecForward, NULL, NULL );
	vecForward.z = 0.0f;
	VectorNormalize( vecForward );

	Vector vecStart = pAttacker->EyePosition();
	vecStart.z -= 30.0f;
	const Vector vecEnd = vecStart + vecForward * 60.0f;
	const float flHullExtent =
		pAttacker->GetGroundEntity() ? 3.0f : 1.0f;

	trace_t traceHit;
	UTIL_TraceHull(
		vecStart,
		vecEnd,
		Vector( -flHullExtent, -flHullExtent, -flHullExtent ),
		Vector(  flHullExtent,  flHullExtent,  flHullExtent ),
		MASK_SHOT_HULL,
		pAttacker,
		COLLISION_GROUP_NONE,
		&traceHit );

	C_BaseEntity *pHitEntity = traceHit.m_pEnt;
	if ( pHitEntity && pHitEntity->IsPlayer() &&
		HL2MPRules() && HL2MPRules()->IsTeamplay() &&
		pHitEntity->GetTeamNumber() == pAttacker->GetTeamNumber() )
	{
		CTraceFilterSkipTwoEntities traceFilter(
			pAttacker, pHitEntity, COLLISION_GROUP_NONE );
		UTIL_TraceHull(
			vecStart,
			vecEnd,
			Vector( -0.25f, -0.25f, -0.25f ),
			Vector(  0.25f,  0.25f,  0.25f ),
			MASK_SHOT,
			&traceFilter,
			&traceHit );
		pHitEntity = traceHit.m_pEnt;
	}

	CFoF_Player *pTarget = ToFoFPlayer( pHitEntity );
	if ( !pTarget || !pTarget->IsAlive() ||
		( HL2MPRules() && HL2MPRules()->IsTeamplay() &&
		  pTarget->GetTeamNumber() == pAttacker->GetTeamNumber() ) )
	{
		return;
	}

	const bool bBoots =
		( pAttacker->GetFoFPlayerInfo() & 0x80 ) != 0;
	pTarget->SetFoFKickedPenaltyTime( gpGlobals->curtime + 1.0f );
	pTarget->SetGroundEntity( NULL );
	pTarget->SetAbsVelocity( vec3_origin );

	Vector vecImpulse = vecForward * ( bBoots ? 210.0f : 175.0f );
	vecImpulse.z = bBoots ? 180.0f : 150.0f;
	pTarget->ApplyAbsVelocityImpulse( vecImpulse );
}
#else
static void FoFApplyKickImpact( CFoF_Player *pAttacker )
{
	if ( !pAttacker )
		return;
	FoFRecordAccuracyShots( pAttacker, 1 );

	CHL2MP_Player *pLagPlayer = ToHL2MPPlayer( pAttacker );
	if ( pLagPlayer )
	{
		lagcompensation->StartLagCompensation(
			pLagPlayer, pLagPlayer->GetCurrentCommand() );
	}

	Vector vecForward;
	pAttacker->EyeVectors( &vecForward, NULL, NULL );
	// The shipped server projects the eye direction onto the ground plane
	// before both the trace and the physical kick.  Keeping view pitch here
	// makes an upward/downward look add a second vertical component and can
	// launch a moving target sideways.
	vecForward.z = 0.0f;
	VectorNormalize( vecForward );

	// The original server lowers the trace from the
	// eye by 30 units and sweeps a short hull 60 units along the view vector.
	Vector vecStart = pAttacker->EyePosition();
	vecStart.z -= 30.0f;
	const Vector vecEnd = vecStart + vecForward * 60.0f;
	const float flHullExtent =
		pAttacker->GetGroundEntity() ? 3.0f : 1.0f;
	const Vector vecKickMins(
		-flHullExtent, -flHullExtent, -flHullExtent );
	const Vector vecKickMaxs(
		flHullExtent, flHullExtent, flHullExtent );

	trace_t traceHit;
	UTIL_TraceHull(
		vecStart,
		vecEnd,
		vecKickMins,
		vecKickMaxs,
		MASK_SHOT_HULL,
		pAttacker,
		COLLISION_GROUP_NONE,
		&traceHit );

	CBaseEntity *pHitEntity = traceHit.m_pEnt;
	if ( pHitEntity && pHitEntity->IsPlayer() &&
		HL2MPRules() && HL2MPRules()->IsTeamplay() &&
		pHitEntity->GetTeamNumber() == pAttacker->GetTeamNumber() )
	{
		CTraceFilterSkipTwoEntitiesFoF traceFilter(
			pAttacker, pHitEntity, COLLISION_GROUP_NONE );
		UTIL_TraceHull(
			vecStart,
			vecEnd,
			Vector( -0.25f, -0.25f, -0.25f ),
			Vector(  0.25f,  0.25f,  0.25f ),
			MASK_SHOT,
			&traceFilter,
			&traceHit );
		pHitEntity = traceHit.m_pEnt;
	}

	if ( pHitEntity )
	{
		CFoF_Player *pTarget = ToFoFPlayer( pHitEntity );
		const bool bBoots =
			( pAttacker->GetFoFPlayerInfo() & 0x80 ) != 0;
		float flDamage = bBoots ? 35.0f : 25.0f;
		if ( pAttacker->GetFoFLastWallJumpTime() >
			gpGlobals->curtime - 1.2f &&
			( pAttacker->GetFoFPlayerInfo() & 0x200000 ) != 0 )
		{
			flDamage += 5.0f;
		}

		CTakeDamageInfo info(
			pAttacker, pAttacker, flDamage, DMG_DIRECT );
		info.SetDamageCustom( 11 );
		Vector vecDamageDirection;
		pAttacker->GetVectors( &vecDamageDirection, NULL, NULL );
		VectorNormalize( vecDamageDirection );
		CalculateMeleeDamageForce(
			&info, vecDamageDirection, traceHit.endpos );

		if ( pTarget && pTarget->IsAlive() &&
			g_pGameRules->FPlayerCanTakeDamage(
				pTarget, pAttacker, info ) )
		{
			if ( FoFPlayersAreEnemies( pAttacker, pTarget ) )
				FoFRecordAccuracyHit( pAttacker, flDamage );
			pTarget->SetFoFKickedPenaltyTime( gpGlobals->curtime + 1.0f );
			pTarget->DoAnimationEvent(
				PLAYERANIMEVENT_PASSTIME_THROW_BEGIN, 0 );
			pTarget->SetGroundEntity( NULL );
			pTarget->SetAbsVelocity( vec3_origin );

			const float flHorizontalForce = bBoots ? 210.0f : 175.0f;
			const float flVerticalForce = bBoots ? 180.0f : 150.0f;
			Vector vecImpulse = vecForward * flHorizontalForce;
			vecImpulse.z += flVerticalForce;
			pTarget->ApplyAbsVelocityImpulse( vecImpulse );

			ClearMultiDamage();
			pTarget->DispatchTraceAttack(
				info, vecDamageDirection, &traceHit );
			ApplyMultiDamage();
			pTarget->SetFoFKicker( pAttacker );
			pTarget->ViewPunch( QAngle( -10.0f, -15.0f, -5.0f ) );
			pTarget->EmitSound( bBoots ?
				"FoFPlayer.KickHitBoots" : "FoFPlayer.KickHit" );
		}
		else if ( !pTarget )
		{
			pHitEntity->DispatchTraceAttack(
				info, vecForward, &traceHit );
			ApplyMultiDamage();
			pAttacker->TraceAttackToTriggers(
				info, traceHit.startpos, traceHit.endpos, vecForward );
		}
	}

	if ( pLagPlayer )
		lagcompensation->FinishLagCompensation( pLagPlayer );
}
#endif

bool CFoF_Player::CanStartFoFKick()
{
	// Shared client/server eligibility from the original ItemPostFrame branch.
	// Bit 0x10000 means another melee action owns the arms.
	if ( ( m_nPlayerInfo & 0x10000 ) != 0 ||
		gpGlobals->curtime <= GetFoFKickTime() ||
		!GetActiveWeapon() ||
		IsDucked() || IsDucking() ||
		GetFOV() <= 50 ||
		IsOnFoFHorse() )
	{
		return false;
	}

	return
		!FoFWeaponHasActualReloadPresentation( GetActiveWeapon1() ) &&
		!FoFWeaponHasActualReloadPresentation( GetActiveWeapon2() );
}

void CFoF_Player::UpdateFoFKick()
{
#ifdef CLIENT_DLL
	if ( this != C_BasePlayer::GetLocalPlayer() )
		return;
#endif

	if ( !IsAlive() )
		return;

	const bool bWantsKick = ( m_nButtons & IN_SPEED ) != 0;
	const bool bCanStartKick = bWantsKick && CanStartFoFKick();

	if ( bCanStartKick )
	{
		// The original client emits event 35 and the swoosh before it
		// advances the predicted kick window.
		DoAnimationEvent(
			PLAYERANIMEVENT_PASSTIME_THROW_MIDDLE, 0 );
		EmitSound( "FoFPlayer.KickSwoosh" );

		const float flKickEnd = gpGlobals->curtime + 1.0f;
		SetNextAttack( flKickEnd );
		SetFoFKickTime( flKickEnd );
		m_Local.m_bPoisoned = true;

#ifndef CLIENT_DLL
		// Carried props leave the grab controller as soon as the kick starts.
		// The later impact frame is only for the ordinary kick trace.
		KickFoFCarriedObject();
#endif
	}

	const float flKickEnd = GetFoFKickTime();
	if ( flKickEnd > gpGlobals->curtime &&
		gpGlobals->curtime > flKickEnd - 0.7f &&
		m_Local.m_bPoisoned )
	{
		m_Local.m_bPoisoned = false;
		FoFApplyKickImpact( this );
	}
}
