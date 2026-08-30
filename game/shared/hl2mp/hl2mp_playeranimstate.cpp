#include "cbase.h"
#include "hl2mp/hl2mp_playeranimstate.h"
#include "fof/fof_player_activities.h"
#include "fof/fof_player_shared.h"

#ifdef CLIENT_DLL
#include "fof/c_fof_player.h"
#else
#include "fof/fof_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static Activity FoFAnimStateActivity( int nOriginalActivity )
{
	Activity activity = ACT_INVALID;
	FoFTranslateNetworkActivity( nOriginalActivity, activity );
	return activity;
}

static bool FoFPlayerAnimStateUsesFreeHorseAim(
	CBasePlayer *pBasePlayer )
{
	CFoF_Player *pPlayer =
		dynamic_cast< CFoF_Player * >( pBasePlayer );
	if ( !pPlayer || !pPlayer->IsOnFoFHorse() ||
		!FoFUsesHorseFreeAim( pPlayer ) )
	{
		return false;
	}

	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !pWeapon )
		return false;

	const int nWeaponID = pWeapon->FoFWeaponID();
	return nWeaponID != 0 && nWeaponID != 8;
}

#ifdef CLIENT_DLL
static void FoFPresentRemoteWeaponActivity(
	CBaseCombatWeapon *pWeapon, Activity activity )
{
	if ( !pWeapon || activity == ACT_INVALID )
		return;

	MDLCACHE_CRITICAL_SECTION();
	pWeapon->SendWeaponAnim( activity );
	pWeapon->ResetAnimationEventParity();
	pWeapon->DoAnimationEvents( pWeapon->GetModelPtr() );
}

static void FoFPresentRemoteWeaponActivity(
	CBasePlayer *pPlayer, Activity activity )
{
	if ( !pPlayer || pPlayer == C_BasePlayer::GetLocalPlayer() ||
		activity == ACT_INVALID )
	{
		return;
	}

	FoFPresentRemoteWeaponActivity(
		pPlayer->GetActiveWeapon(), activity );
	FoFPresentRemoteWeaponActivity(
		pPlayer->GetActiveWeapon2(), activity );
}
#endif

void CHL2MPPlayerAnimState::DoAnimationEvent(
	PlayerAnimEvent_t event, int nData )
{
	const bool bDucked = m_pFoFPlayer &&
		( m_pFoFPlayer->GetFlags() & FL_DUCKING ) != 0;
	Activity activity = ACT_INVALID;

	switch ( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
		activity = FoFAnimStateActivity( bDucked ? 0x481 : 0x47D );
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD, activity, true );
#ifdef CLIENT_DLL
		FoFPresentRemoteWeaponActivity(
			m_pFoFPlayer, FoFAnimStateActivity( 0x0B4 ) );
#endif
		return;

	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		activity = FoFAnimStateActivity( bDucked ? 0x483 : 0x47F );
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD, activity, true );
#ifdef CLIENT_DLL
		FoFPresentRemoteWeaponActivity(
			m_pFoFPlayer, FoFAnimStateActivity( 0x0B4 ) );
#endif
		return;

	case PLAYERANIMEVENT_RELOAD:
	case PLAYERANIMEVENT_RELOAD_END:
		activity = FoFAnimStateActivity( bDucked ? 0x48E : 0x48B );
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD, activity, true );
		return;

	case PLAYERANIMEVENT_RELOAD_LOOP:
		activity = FoFAnimStateActivity( bDucked ? 0x48F : 0x48C );
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD, activity, true );
		return;

	case PLAYERANIMEVENT_CANCEL:
		ResetGestureSlots();
		return;

	case PLAYERANIMEVENT_ATTACK_PRE:
		activity = FoFAnimStateActivity( bDucked ? 0x49A : 0x497 );
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD, activity, false );
#ifdef CLIENT_DLL
		FoFPresentRemoteWeaponActivity( m_pFoFPlayer, activity );
#endif
		return;

	case PLAYERANIMEVENT_ATTACK_POST:
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD,
			FoFAnimStateActivity( 0x498 ), true );
		return;

	case PLAYERANIMEVENT_VOICE_COMMAND_GESTURE:
		if ( !IsGestureSlotActive( GESTURE_SLOT_ATTACK_AND_RELOAD ) )
		{
			RestartGesture(
				GESTURE_SLOT_ATTACK_AND_RELOAD,
				static_cast< Activity >( nData ), true );
		}
		return;

	case PLAYERANIMEVENT_PASSTIME_THROW_BEGIN:
		RestartGesture(
			GESTURE_SLOT_CUSTOM,
			FoFAnimStateActivity( 0x47C ), true );
		return;

	case PLAYERANIMEVENT_PASSTIME_THROW_MIDDLE:
		RestartGesture(
			GESTURE_SLOT_CUSTOM,
			FoFAnimStateActivity( 0x08C ), true );
		return;

	case PLAYERANIMEVENT_PASSTIME_THROW_END:
		RestartGesture(
			GESTURE_SLOT_ATTACK_AND_RELOAD,
			FoFAnimStateActivity( 0x144 ), true );
		return;

	case PLAYERANIMEVENT_PASSTIME_THROW_CANCEL:
		RestartGesture(
			GESTURE_SLOT_CUSTOM,
			FoFAnimStateActivity( 0x08D ), true );
		return;

	default:
		CMultiPlayerAnimState::DoAnimationEvent( event, nData );
		return;
	}
}

Activity CHL2MPPlayerAnimState::TranslateActivity( Activity activity )
{
	if ( !m_pFoFPlayer || !m_pFoFPlayer->GetActiveWeapon() )
		return activity;

	CBaseCombatWeapon *pWeapon =
		FoFSelectGestureWeapon( m_pFoFPlayer );
	if ( !pWeapon )
		return activity;

	bool bRequired = false;
	const Activity overridden =
		pWeapon->ActivityOverride( activity, &bRequired );
	if ( overridden != activity || bRequired )
		return overridden;

	return FoFOriginalWeaponActivityOverride(
		m_pFoFPlayer, pWeapon, activity );
}

int CHL2MPPlayerAnimState::SelectWeightedSequence(
	Activity activity )
{
	// This vtable slot forwards directly to the player model's sequence
	// selector.
	return m_pFoFPlayer->SelectWeightedSequence( activity );
}

bool CHL2MPPlayerAnimState::HandleJumping(
	Activity &idealActivity )
{
	// The shipped client samples outer velocity here even though the jump
	// branch does not consume the returned vector directly.
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();
		}

		if ( m_pFoFPlayer->GetWaterLevel() >= WL_Waist )
		{
			m_bJumping = false;
			RestartMainSequence();
		}
		else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f &&
			( m_pFoFPlayer->GetFlags() & FL_ONGROUND ) )
		{
			m_bJumping = false;
			RestartMainSequence();
		}
	}

	if ( !m_bJumping )
		return false;

	// FoF's alternate start/float/land branch is guarded by an unwritten
	// compatibility flag. Its default remains false: armed players use the
	// multiplayer jump activity and unarmed players use the base jump activity.
	idealActivity = m_pFoFPlayer->GetActiveWeapon() ?
		ACT_MP_JUMP : ACT_JUMP;
	return true;
}

bool CHL2MPPlayerAnimState::HandleDucking(
	Activity &idealActivity )
{
	if ( !m_pFoFPlayer ||
		!( m_pFoFPlayer->GetFlags() & FL_DUCKING ) )
	{
		return false;
	}

	const bool bMoving = GetOuterXYSpeed() >= 0.5f;
	idealActivity = bMoving ? ACT_MP_CROUCHWALK : ACT_MP_CROUCH_IDLE;

	CFoF_Player *pPlayer =
		dynamic_cast< CFoF_Player * >( m_pFoFPlayer );
	if ( !pPlayer )
		return true;

	if ( ( pPlayer->GetFoFPlayerInfo() & 0x40000 ) != 0 ||
		pPlayer->GetFoFSlideForce() > 0.0f )
	{
		idealActivity = FoFAnimStateActivity( 0x70E );
	}
	else if ( !pPlayer->GetActiveWeapon() )
	{
		idealActivity = bMoving ? ACT_WALK_CROUCH : ACT_CROUCH;
	}
	return true;
}

bool CHL2MPPlayerAnimState::HandleMoving(
	Activity &idealActivity )
{
	CFoF_Player *pPlayer =
		dynamic_cast< CFoF_Player * >( m_pFoFPlayer );
	if ( pPlayer && pPlayer->IsOnFoFHorse() )
	{
		idealActivity = FoFAnimStateActivity( 0x71A );
		return true;
	}

	const float flSpeed = GetOuterXYSpeed();
	if ( flSpeed > 0.5f )
	{
		idealActivity = ACT_MP_RUN;
		if ( pPlayer && !pPlayer->GetActiveWeapon() )
		{
			idealActivity = flSpeed < 200.0f ? ACT_WALK : ACT_RUN;
		}
		else if ( pPlayer && pPlayer->GetActiveWeapon() &&
			pPlayer->GetActiveWeapon()->FoFWeaponID() == 3 &&
			pPlayer->GetFoFSightExpFactor() > 0.75f )
		{
			idealActivity = ACT_RUN_RIFLE;
		}
	}
	return true;
}

bool CHL2MPPlayerAnimState::HandleSwimming(
	Activity &idealActivity )
{
	// FoF disables the stock multiplayer swim state. The parameter is
	// intentionally unused.
	(void)idealActivity;
	m_bInSwim = false;
	if ( !m_bFirstSwimFrame )
		m_bFirstSwimFrame = true;
	return false;
}

void CHL2MPPlayerAnimState::ComputePoseParam_AimYaw(
	CStudioHdr *pStudioHdr )
{
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );
	const bool bMoving = vecVelocity.Length() > 1.0f;

	CFoF_Player *pPlayer =
		dynamic_cast< CFoF_Player * >( m_pFoFPlayer );
	const bool bOnHorse = pPlayer && pPlayer->IsOnFoFHorse();
	const bool bFreeHorseAim =
		FoFPlayerAnimStateUsesFreeHorseAim( m_pFoFPlayer );

	if ( bMoving || m_bForceAimYaw )
	{
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else if ( m_PoseParameterData.m_flLastAimTurnTime <= 0.0f )
	{
		m_flGoalFeetYaw = m_flEyeYaw;
		m_flCurrentFeetYaw = m_flEyeYaw;
		m_PoseParameterData.m_flLastAimTurnTime = gpGlobals->curtime;
	}
	else
	{
		const float flYawDelta =
			AngleNormalize( m_flGoalFeetYaw - m_flEyeYaw );
		const float flMaxBodyYaw =
			bOnHorse && !bFreeHorseAim ? 40.0f : 25.0f;
		if ( fabs( flYawDelta ) > flMaxBodyYaw )
		{
			const float flSide = flYawDelta > 0.0f ? -1.0f : 1.0f;
			m_flGoalFeetYaw += flMaxBodyYaw * flSide;
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );
	if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
	{
		if ( m_bForceAimYaw )
		{
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		}
		else
		{
			ConvergeYawAngles(
				m_flGoalFeetYaw, 720.0f, gpGlobals->frametime,
				m_flCurrentFeetYaw );
			m_flLastAimTurnTime = gpGlobals->curtime;
		}
	}

	if ( bFreeHorseAim )
	{
		const float flHorseYaw = FoFHorseAngles( m_pFoFPlayer )[YAW];
		m_angRender[YAW] = flHorseYaw;
		m_flCurrentFeetYaw = flHorseYaw;
	}
	else
	{
		m_angRender[YAW] = m_flCurrentFeetYaw;
	}

	const float flAimYaw =
		AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );
	m_pFoFPlayer->SetPoseParameter(
		pStudioHdr, m_PoseParameterData.m_iAimYaw, flAimYaw );
	m_DebugAnimData.m_flAimYaw = flAimYaw;

	m_bForceAimYaw = !bFreeHorseAim &&
		m_pFoFPlayer->m_Local.m_bPoisoned;

#ifndef CLIENT_DLL
	QAngle angles = m_pFoFPlayer->GetAbsAngles();
	angles[YAW] = m_flCurrentFeetYaw;
	m_pFoFPlayer->SetAbsAngles( angles );
#endif
}

CHL2MPPlayerAnimState *CreateFoFPlayerAnimState(
	CBasePlayer *pPlayer )
{
	MultiPlayerMovementData_t movementData;
	movementData.m_flWalkSpeed = 75.0f;
	movementData.m_flRunSpeed = 320.0f;
	movementData.m_flSprintSpeed = -1.0f;
	movementData.m_flBodyYawRate = 720.0f;
	return new CHL2MPPlayerAnimState( pPlayer, movementData );
}
