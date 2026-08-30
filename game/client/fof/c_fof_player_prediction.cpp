#include "cbase.h"
#include "c_baseviewmodel.h"
#include "cliententitylist.h"
#include "collisionutils.h"
#include "fof/c_fof_horse.h"
#include "fof/c_fof_player.h"
#include "fof/fof_client_settings.h"
#include "fof/fof_hud_menu.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_properties.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_weapon_parse.h"
#include "in_buttons.h"
#include "mathlib/mathlib.h"
#include "prediction.h"
#include "predictioncopy.h"
#include "usercmd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar fof_sv_battle_royale(
	"fof_sv_battle_royale",
	"0",
	FCVAR_REPLICATED,
	"Use FoF battle-royale inventory weight limits for client prediction." );

// The shipped client registers both Grand Elimination switches even though
// only the server reads the FFA variant at runtime.  Registering the replica
// keeps the client/server ConVar contract intact when joining either variant.
static ConVar fof_sv_battle_royale_ffa(
	"fof_sv_battle_royale_ffa",
	"0",
	FCVAR_REPLICATED,
	"Use the free-for-all Grand Elimination ruleset." );

static void FoFApplyClientPlayerAvoidance(
	C_FoF_Player *pPlayer, CUserCmd *pCmd );
static void FoFApplyClientHorseAvoidance(
	C_FoF_Player *pPlayer, CUserCmd *pCmd );

static void FoFSuppressMenuPlayerMovement( CUserCmd *pCmd )
{
	if ( !pCmd )
		return;

	pCmd->forwardmove = 0.0f;
	pCmd->sidemove = 0.0f;
	pCmd->upmove = 0.0f;
	pCmd->buttons &= ~( IN_JUMP | IN_DUCK |
		IN_FORWARD | IN_BACK | IN_LEFT | IN_RIGHT |
		IN_MOVELEFT | IN_MOVERIGHT | IN_RUN |
		IN_SPEED | IN_WALK );
}

static bool FoFIsFixedWeaponControlActive( C_FoF_Player *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )
		return false;

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon1();
	if ( !pWeapon || Q_stricmp( pWeapon->GetClassname(), "weapon_fists" ) )
		return false;

	C_BaseViewModel *pViewModel = pPlayer->GetViewModel( 0, false );
	if ( !pViewModel || pViewModel->GetWeapon() != pWeapon ||
		!pViewModel->IsEffectActive( EF_NODRAW ) )
	{
		return false;
	}

	return pViewModel->GetSequenceActivity( pViewModel->GetSequence() ) ==
		ACT_VM_HOLSTER;
}

void C_FoF_Player::OnPreDataChanged( DataUpdateType_t updateType )
{
	m_nFoFMoveTypeBeforeNetworkUpdate = GetMoveType();
	m_nFoFObserverModeBeforeNetworkUpdate = GetObserverMode();
	BaseClass::OnPreDataChanged( updateType );
}

void C_FoF_Player::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED ||
		this != C_BasePlayer::GetLocalPlayer() ||
		prediction->InPrediction() )
	{
		return;
	}

	const bool bEnteredObserverMovement =
		m_nFoFMoveTypeBeforeNetworkUpdate != MOVETYPE_OBSERVER &&
		GetMoveType() == MOVETYPE_OBSERVER;
	const bool bEnteredRoamingMode =
		m_nFoFObserverModeBeforeNetworkUpdate != OBS_MODE_ROAMING &&
		GetObserverMode() == OBS_MODE_ROAMING;
	if ( bEnteredObserverMovement || bEnteredRoamingMode )
	{
		// Death, spectator-team and roaming-mode state arrive in independent
		// fields.  If the optimized prediction history is retained across that
		// boundary, a rare packet ordering can keep one old FLYGRAVITY command in
		// every later observer replay.  Reset only the command-history cursor;
		// roaming movement itself remains fully predicted.
		prediction->OnReceivedUncompressedPacket();
	}
}

void C_FoF_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	// PostDataUpdate runs immediately after the packet has decoded, before
	// prediction restores and replays local state.  OnDataChanged can run while
	// that replay is active and therefore is too late to preserve m_fFlags.
	if ( IsLocalPlayer() )
	{
		const bool bServerFrozen = ( GetFlags() & FL_FROZEN ) != 0;
		const bool bFreezeStateChanged =
			updateType != DATA_UPDATE_CREATED &&
			bServerFrozen != m_bFoFServerFrozen;

		m_bFoFServerFrozen = bServerFrozen;
		m_bFoFServerAtControls = ( GetFlags() & FL_ATCONTROLS ) != 0;

		if ( bFreezeStateChanged && !prediction->InPrediction() )
		{
			// The server changes the accepted attack stream at this boundary.
			// Retaining an optimized command from the other side can replay an
			// attack edge that the server discarded. Flush only the prediction-
			// history cursor when the authoritative flag changes.
			prediction->OnReceivedUncompressedPacket();
		}
	}
}

bool C_FoF_Player::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	if ( pCmd && this == C_BasePlayer::GetLocalPlayer() )
	{
		// Capture physical intent before any weapon-specific command reservation.
		// Any filtering below is applied to the actual usercmd so local prediction
		// and the server receive the same button stream.
		m_bFoFRawAttack2 = ( pCmd->buttons & IN_ATTACK2 ) != 0;
		m_bFoFRawReload = ( pCmd->buttons & IN_RELOAD ) != 0;
		if ( !m_bFoFRawReload )
			m_bFoFReloadPressConsumed = false;
	}

	const bool bMenuBlocksMovement = pCmd &&
		this == C_BasePlayer::GetLocalPlayer() &&
		FoFMenuBlocksPlayerMovement();
	if ( bMenuBlocksMovement )
	{
		// Apply this before FoF's horse, slide and airborne-jump command shaping
		// so keys owned by the menu cannot alter any deferred local input state.
		FoFSuppressMenuPlayerMovement( pCmd );
	}

	// The original client applies the drunk camera wobble directly to the
	// local command before forwarding it through the stock player input path.
	if ( pCmd && this == C_BasePlayer::GetLocalPlayer() && m_flDrunkness > 0.0f )
	{
		const float flScale = RemapValClamped( m_flDrunkness, 0.0f, 10.0f, 1.0f, 7.0f );
		const float flPhase = gpGlobals->curtime * 2.0f;
		pCmd->viewangles[PITCH] += sinf( flPhase ) * flScale * 0.005f;
		pCmd->viewangles[YAW] += sinf( flPhase * 1.3f ) * flScale * 0.01f;
	}

	if ( pCmd && IsAlive() )
	{
		// This replicated state owns the command while
		// the player is captured.  Both jump and the FoF interaction button
		// are removed and crouch is forced.
		if ( IsFoFPotionWeaponLockActive() )
		{
			pCmd->buttons &= ~( 0x40002 | IN_ALT1 );
			pCmd->buttons |= IN_DUCK;

			// The original server permits an ordinary weapon-selection command
			// (and FoF's MOUSE3/+alt1 hand swap) during the full-potion reward.
			// Either path can holster the forced dual Peacemakers without a valid
			// replacement and leave the player with no visible weapon.  Consume
			// every client weapon-selection request until the reward bit clears;
			// the authoritative grant itself does not use CUserCmd::weaponselect.
			pCmd->weaponselect = 0;
			pCmd->weaponsubtype = 0;
		}

		// Preserve forward momentum once a mounted player has accelerated.
		// The original only injects this when forward is not already held.
		if ( IsOnFoFHorse() &&
			!( pCmd->buttons & IN_FORWARD ) &&
			GetFoFHorseAcceleration() > 0.75f )
		{
			pCmd->buttons &= ~IN_BACK;
			pCmd->buttons |= IN_FORWARD;
			pCmd->forwardmove = 200.0f;
		}

		if ( !IsOnFoFHorse() && GetFoFSlideForce() > 0.5f )
			pCmd->buttons |= IN_DUCK;

		// The original client suppresses repeated high-speed airborne
		// jumps for 0.5 seconds.  The local jump
		// timer, not m_flWaterJumpTime; using the water-jump timer clears
		// IN_JUMP while an ordinary moving wall jump is still eligible.
		if ( this == C_BasePlayer::GetLocalPlayer() &&
			( pCmd->buttons & IN_JUMP ) &&
			!GetGroundEntity() &&
			m_Local.m_flJumpTime < 200.0f &&
			GetAbsVelocity().Length() > 250.0f )
		{
			m_flFoFJumpReleaseTime = gpGlobals->curtime + 0.5f;
		}
		if ( m_flFoFJumpReleaseTime > gpGlobals->curtime )
			pCmd->buttons &= ~IN_JUMP;

		// The original client shapes the command yaw while mounted.  FoF's
		// free-horse-aim test then lets the client
		// ConVar must be enabled and the active weapon must be neither fists
		// (FoF weapon id 0) nor a melee weapon (id 8).  Free aim is clamped to
		// 80 degrees either side of the horse.  Without free aim, the view is
		// eased back toward the horse only while the active weapon is drawing.
		if ( IsOnFoFHorse() )
		{
			const float flHorseYaw = GetFoFHorseAngles()[YAW];
			const float flYawDelta =
				AngleNormalize( pCmd->viewangles[YAW] - flHorseYaw );
			C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
			const int nWeaponID = pWeapon ? pWeapon->FoFWeaponID() : -1;
			const bool bFreeHorseAim = FoFUsesHorseFreeAim( this ) &&
				pWeapon && nWeaponID != 0 && nWeaponID != 8;

			if ( bFreeHorseAim )
			{
				if ( fabsf( flYawDelta ) > 80.0f )
				{
					pCmd->viewangles[YAW] = flHorseYaw +
						( flYawDelta > 0.0f ? 80.0f : -80.0f );
				}
			}
			else if ( pWeapon && pWeapon->GetActivity() == ACT_VM_DRAW )
			{
				if ( fabsf( flYawDelta ) <= 1.0f )
				{
					pCmd->viewangles[YAW] = flHorseYaw;
				}
				else
				{
					const float flReturnSpeed = RemapValClamped(
						fabsf( flYawDelta ), 0.1f, 80.0f, 10.0f, 600.0f );
					pCmd->viewangles[YAW] += flReturnSpeed *
						gpGlobals->frametime *
						( flYawDelta > 0.0f ? -1.0f : 1.0f );
				}
			}
		}
	}

	// Static initializer evidence identifies the parent ConVar read at
	// as fof_sv_playerattack_allowed.  FoF also disallows both
	// attack buttons while FL_ONTRAIN is set.
	if ( pCmd )
	{
		static ConVarRef playerAttackAllowed(
			"fof_sv_playerattack_allowed", true );
		const bool bAttackDisabled =
			( playerAttackAllowed.IsValid() &&
				!playerAttackAllowed.GetBool() ) ||
			( GetFlags() & FL_ONTRAIN );
		if ( bAttackDisabled )
		{
			pCmd->buttons &= ~( IN_ATTACK | IN_ATTACK2 );
		}
	}

	const bool bUpdateViewAngles =
		BaseClass::CreateMove( flInputSampleTime, pCmd );

	// The shipped C_HL2MP_Player::CreateMove appends these two command-space
	// avoidance passes after C_BaseHLPlayer::CreateMove.  Keeping them here
	// preserves the stock SDK base class while restoring FoF's exact ordering.
	FoFApplyClientPlayerAvoidance( this, pCmd );
	FoFApplyClientHorseAvoidance( this, pCmd );

	if ( bMenuBlocksMovement )
	{
		// BuyMenuDM and PresetMenu own the pointer while they are open, but
		// deliberately remain non-modal so Escape and the developer console can
		// still reach the engine.  Suppress only the gameplay movement encoded in
		// the resulting usercmd; closing the menu immediately restores the live
		// physical key state without leaving a latched +forward/+moveleft command.
		FoFSuppressMenuPlayerMovement( pCmd );
	}

	return bUpdateViewAngles;
}

bool C_FoF_Player::IsAllowedToSwitchWeapons( void )
{
	// Keep the weapon carousel closed as well as filtering its eventual
	// usercmd.  This avoids presenting a selectable weapon that cannot safely
	// replace the forced potion-reward pair.
	return !IsFoFPotionWeaponLockActive() &&
		BaseClass::IsAllowedToSwitchWeapons();
}

void C_FoF_Player::ItemPostFrame( void )
{
	// Round locks reject attacks, and fixed emplacements reserve those inputs for
	// the mounted weapon. Strip only the attack edges while the FoF player-state
	// and inventory-weapon state machines continue to advance.
	if ( m_bFoFServerFrozen || m_bFoFServerAtControls )
	{
		const int nAttackButtons = IN_ATTACK | IN_ATTACK2;
		m_nButtons &= ~nAttackButtons;
		m_afButtonPressed &= ~nAttackButtons;
		m_afButtonReleased &= ~nAttackButtons;
	}

	C_BaseCombatWeapon *pWeapon = GetActiveWeapon1();
	// Keep the predicted weapon command stream identical to the usercmd sent to
	// the server.  Reload/secondary-attack exclusion belongs to each weapon's
	// ItemPostFrame state machine; clearing the player button fields here makes
	// the local weapon simulate a different command from an original FoF server.
	UpdateFoFCommandState();
	if ( pWeapon && pWeapon->FoFWeaponID() == 1 )
	{
		/*
		 * The original client
		 * drives the bow viewmodel from the player's sight-expansion value
		 * during ItemPostFrame, before the viewmodel is rendered.  Both
		 * v_bow.mdl and v_xbow.mdl use aim_pitch (0..1) to blend the
		 * bow_off/bow_mid1..bow_mid3/bow_on poses in their aim_idle sequence.
		 */
		C_BaseViewModel *pViewModel = GetViewModel( 0, true );
		if ( pViewModel && pViewModel->GetModelPtr() )
		{
			pViewModel->SetPoseParameter(
				pViewModel->GetModelPtr(),
				"aim_pitch",
				clamp( m_flSightExpFactor, 0.0f, 1.0f ) );
		}
	}
	// The original client updates all FoF player state before its final call
	// to C_BasePlayer::ItemPostFrame.
	// Keeping that order is important: the server snapshots the FoF accuracy
	// fields from the pre-weapon state, so running the weapon first makes the
	// client predict those fields one command ahead and forces a full replay.
	// func_tank_fof keeps fists as the active inventory item, but holsters and
	// hides their viewmodel while the mounted gun owns IN_ATTACK.  The stock
	// client continues to simulate those hidden fists, restarting their idle or
	// melee sequence on every predicted command even though the server freezes
	// all hand-weapon state.  That original bug produces an unbounded idle-time
	// delta and a sequence/parity correction every snapshot.  Preserve the FoF
	// player-state updates above, but do not dispatch the captured command to the
	// holstered fists.  The activity/effect pair is authoritative for both the
	// mobile cannon and Gatling gun and avoids treating an ordinary hidden HUD or
	// third-person view as fixed-weapon control.
	// The original client skips this call for every FL_FROZEN frame,
	// but the shipped server's CBasePlayer::PostThink keeps running ItemPostFrame
	// during ordinary FoF round locks. That client bug leaves idle time and
	// viewmodel parity one command behind for the entire lock. Continue the
	// weapon state machine with attacks filtered above, and stop only for the map
	// intermission where server g_fGameOver really suppresses PostThink. Do not
	// use C_HL2MP_Player's virtual override: FoF calls C_BasePlayer directly.
	const bool bMapIntermission =
		( GetFlags() & FL_FROZEN ) && m_bFoFMapIntermission;
	if ( !bMapIntermission && !FoFIsFixedWeaponControlActive( this ) )
	{
		C_BasePlayer::ItemPostFrame();
	}

}

static bool FoFGetAvoidanceBounds(
	C_BaseEntity *pEntity,
	Vector &center,
	Vector &worldMins,
	Vector &worldMaxs )
{
	if ( !pEntity )
		return false;

	center = pEntity->GetAbsOrigin();
	worldMins = pEntity->WorldAlignMins();
	worldMaxs = pEntity->WorldAlignMaxs();
	center.z += ( worldMaxs.z - worldMins.z ) * 0.5f;
	worldMins += center;
	worldMaxs += center;
	return true;
}

static float FoFGetAvoidancePushStrength(
	const Vector &delta, C_BaseEntity *pAvoidEntity )
{
	if ( !pAvoidEntity )
		return 0.0f;

	Vector radius =
		pAvoidEntity->WorldAlignMaxs() - pAvoidEntity->WorldAlignMins();
	radius.z = 0.0f;

	return RemapValClamped(
		delta.Length(), radius.Length(), 0.0f,
		0.0f, hl2mp_max_separation_force.GetFloat() );
}

static void FoFClampAvoidanceCommand( CUserCmd *pCmd )
{
	float forwardScale = 1.0f;
	if ( pCmd->forwardmove > fabsf( cl_forwardspeed.GetFloat() ) )
	{
		forwardScale =
			fabsf( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabsf( cl_backspeed.GetFloat() ) )
	{
		forwardScale = fabsf( cl_backspeed.GetFloat() ) /
			fabsf( pCmd->forwardmove );
	}

	float sideScale = 1.0f;
	if ( fabsf( pCmd->sidemove ) > fabsf( cl_sidespeed.GetFloat() ) )
	{
		sideScale = fabsf( cl_sidespeed.GetFloat() ) /
			fabsf( pCmd->sidemove );
	}

	const float scale = MIN( forwardScale, sideScale );
	pCmd->forwardmove *= scale;
	pCmd->sidemove *= scale;
}

static void FoFApplyAvoidanceCommand(
	C_FoF_Player *pPlayer,
	CUserCmd *pCmd,
	C_BaseEntity *pAvoidEntity,
	const Vector &playerCenter )
{
	Vector delta = pAvoidEntity->WorldSpaceCenter() - playerCenter;
	const float pushStrength =
		FoFGetAvoidancePushStrength( delta, pAvoidEntity );
	if ( pushStrength < 0.01f )
		return;

	static const Vector up( 0.0f, 0.0f, 1.0f );
	Vector push;
	if ( pPlayer->GetAbsVelocity().Length2DSqr() > 0.1f )
	{
		Vector velocity = pPlayer->GetAbsVelocity();
		velocity.z = 0.0f;
		CrossProduct( up, velocity, push );
		VectorNormalize( push );
	}
	else
	{
		QAngle viewAngles = pCmd->viewangles;
		viewAngles.x = 0.0f;
		AngleVectors( viewAngles, NULL, &push, NULL );
	}

	Vector separationVelocity = push *
		( delta.Dot( push ) < 0.0f ? pushStrength : -pushStrength );

	float maxPlayerSpeed = pPlayer->MaxSpeed();
	if ( ( pPlayer->GetFlags() & FL_DUCKING ) &&
		pPlayer->GetGroundEntity() != NULL )
	{
		maxPlayerSpeed *= 1.33333333f;
	}

	if ( separationVelocity.LengthSqr() >
		maxPlayerSpeed * maxPlayerSpeed )
	{
		separationVelocity.NormalizeInPlace();
		separationVelocity *= maxPlayerSpeed;
	}

	QAngle moveAngles = pCmd->viewangles;
	moveAngles.x = 0.0f;
	Vector forward;
	Vector right;
	AngleVectors( moveAngles, &forward, &right, NULL );

	Vector direction = separationVelocity;
	VectorNormalize( direction );
	pCmd->forwardmove += forward.Dot( direction ) * pushStrength;
	pCmd->sidemove += right.Dot( direction ) * pushStrength;
	FoFClampAvoidanceCommand( pCmd );
}

static void FoFApplyClientPlayerAvoidance(
	C_FoF_Player *pPlayer, CUserCmd *pCmd )
{
	// The original client gates player avoidance on
	// C_HL2MPRules::m_bTeamPlayEnabled. Horse avoidance is independent.
	if ( !pPlayer || !pCmd || !pPlayer->IsAlive() ||
		!HL2MPRules() || !HL2MPRules()->IsTeamplay() )
	{
		return;
	}

	Vector playerCenter;
	Vector playerMins;
	Vector playerMaxs;
	FoFGetAvoidanceBounds(
		pPlayer, playerCenter, playerMins, playerMaxs );

	const int highestEntityIndex =
		ClientEntityList().GetHighestEntityIndex();
	for ( int index = 0; index <= highestEntityIndex; ++index )
	{
		C_BaseEntity *pEntity = ClientEntityList().GetBaseEntity( index );
		if ( !pEntity || !pEntity->IsPlayer() || pEntity == pPlayer ||
			pEntity->IsDormant() ||
			pEntity->IsEFlagSet( EFL_NOCLIP_ACTIVE ) )
		{
			continue;
		}

		Vector avoidCenter;
		Vector avoidMins;
		Vector avoidMaxs;
		FoFGetAvoidanceBounds(
			pEntity, avoidCenter, avoidMins, avoidMaxs );
		if ( !IsBoxIntersectingBox(
			playerMins, playerMaxs, avoidMins, avoidMaxs ) )
		{
			continue;
		}

		FoFApplyAvoidanceCommand(
			pPlayer, pCmd, pEntity, playerCenter );
		return;
	}
}

static void FoFApplyClientHorseAvoidance(
	C_FoF_Player *pPlayer, CUserCmd *pCmd )
{
	if ( !pPlayer || !pCmd || !pPlayer->IsAlive() )
		return;

	Vector playerCenter;
	Vector playerMins;
	Vector playerMaxs;
	FoFGetAvoidanceBounds(
		pPlayer, playerCenter, playerMins, playerMaxs );

	for ( int index = 0; index < FoFClientHorseCount(); ++index )
	{
		C_FoF_Horse *pHorse = FoFClientHorseAt( index );
		if ( !pHorse || pHorse->IsDormant() ||
			pPlayer->GetAbsOrigin().DistTo( pHorse->GetAbsOrigin() ) > 150.0f )
		{
			continue;
		}

		const Vector horseOrigin = pHorse->GetAbsOrigin();
		const Vector horseMins =
			horseOrigin + Vector( -30.0f, -30.0f, 30.0f );
		const Vector horseMaxs =
			horseOrigin + Vector( 30.0f, 30.0f, 90.0f );
		if ( !IsBoxIntersectingBox(
			playerMins, playerMaxs, horseMins, horseMaxs ) )
		{
			continue;
		}

		FoFApplyAvoidanceCommand(
			pPlayer, pCmd, pHorse, playerCenter );
		return;
	}
}

void CPrediction::CheckMovingGround(
	C_BasePlayer *pPlayer,
	double flFrameTime )
{
	if ( pPlayer->GetFlags() & FL_ONGROUND )
	{
		C_BaseEntity *pGroundEntity = pPlayer->GetGroundEntity();
		if ( pGroundEntity && ( pGroundEntity->GetFlags() & FL_CONVEYOR ) )
		{
			Vector vecNewVelocity;
			pGroundEntity->GetGroundVelocityToApply( vecNewVelocity );
			if ( pPlayer->GetFlags() & FL_BASEVELOCITY )
				vecNewVelocity += pPlayer->GetBaseVelocity();

			pPlayer->SetBaseVelocity( vecNewVelocity );
			pPlayer->AddFlag( FL_BASEVELOCITY );
		}
	}

	if ( !( pPlayer->GetFlags() & FL_BASEVELOCITY ) )
	{
		pPlayer->ApplyAbsVelocityImpulse(
			( 1.0 + flFrameTime * 0.5 ) * pPlayer->GetBaseVelocity() );
		pPlayer->SetBaseVelocity( vec3_origin );
	}

	pPlayer->RemoveFlag( FL_BASEVELOCITY );
}
