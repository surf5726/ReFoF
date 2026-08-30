//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF movement vtable implementations shared by prediction/server.
//
//=============================================================================//

#include "cbase.h"
#include "gamemovement.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/fof_weapon_properties.h"
#include <xmmintrin.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool g_bMovementOptimizations;

static ConVar sv_ladder_dampen(
	"sv_ladder_dampen", "0.0", FCVAR_REPLICATED,
	"Amount to dampen perpendicular movement on a ladder",
	true, 0.0f, true, 1.0f );
static ConVar sv_ladder_angle(
	"sv_ladder_angle", "-1", FCVAR_REPLICATED,
	"Cos of angle of incidence to ladder perpendicular for applying ladder_dampen",
	true, -1.0f, true, 1.0f );

ConVar fof_sv_viewspring(
	"fof_sv_viewspring", "25", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"View recover speed after viewpunch", true, 25.0f, true, 25.0f );

void CGameMovement::FoFAdjustLadderLateral(
	Vector &lateral, const Vector &ladderUp,
	const Vector &ladderPerpendicular,
	const Vector &ladderNormalVelocity,
	const Vector &ladderNormal ) const
{
	const float flUpDistance = DotProduct( ladderUp, lateral );
	const float flPerpendicularDistance =
		DotProduct( ladderPerpendicular, lateral );

	Vector incidence =
		ladderPerpendicular * flPerpendicularDistance;
	incidence += ladderNormalVelocity;
	VectorNormalize( incidence );

	if ( DotProduct( incidence, ladderNormal ) <
		sv_ladder_angle.GetFloat() )
	{
		lateral = ladderUp * flUpDistance + ladderPerpendicular *
			sv_ladder_dampen.GetFloat() * flPerpendicularDistance;
	}
}

// FoF exposes CGameMovement directly.  fof_fistful's func_ladder was
// compiled into a CONTENTS_LADDER world brush, so the HL2-specific
// CHL2GameMovement singleton (which searches only networked
// func_useableladder entities) cannot predict the original server's mount.
// Keep the concrete client movement singleton in the FoF tree; the shared
// CGameMovement vtable remains the engine ABI and already contains the FoF
// movement slots implemented below.
static CGameMovement g_GameMovement;
IGameMovement *g_pGameMovement =
	static_cast< IGameMovement * >( &g_GameMovement );

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(
	CGameMovement,
	IGameMovement,
	INTERFACENAME_GAMEMOVEMENT,
	g_GameMovement );

float CGameMovement::ClimbSpeed( void ) const
{
	// FoF CGameMovement vtable slot 43 loads the exact 150.0f
	// constant; the stock Source 2013 implementation returns 200.0f.
	return 150.0f;
}

unsigned int CGameMovement::PlayerSolidMask( bool brushOnly )
{
	unsigned int mask =
		brushOnly ? MASK_PLAYERSOLID_BRUSHONLY : MASK_PLAYERSOLID;
	if ( player )
		mask |= FoFTeamCollisionContents( player->GetTeamNumber() );
	return mask;
}

Vector CGameMovement::GetPlayerMins( bool ducked ) const
{
	if ( player && player->m_bOnHorse )
		return Vector( -16.0f, -16.0f, 0.0f ) * player->GetModelScale();
	return ducked ? VEC_DUCK_HULL_MIN_SCALED( player ) :
		VEC_HULL_MIN_SCALED( player );
}

Vector CGameMovement::GetPlayerMaxs( bool ducked ) const
{
	if ( player && player->m_bOnHorse )
		return Vector( 16.0f, 16.0f, 100.0f ) * player->GetModelScale();
	return ducked ? VEC_DUCK_HULL_MAX_SCALED( player ) :
		VEC_HULL_MAX_SCALED( player );
}

Vector CGameMovement::GetPlayerMins( void ) const
{
	if ( player && player->m_bOnHorse )
		return Vector( -16.0f, -16.0f, 0.0f ) * player->GetModelScale();
	if ( player->IsObserver() )
		return VEC_OBS_HULL_MIN_SCALED( player );
	return player->m_Local.m_bDucked ? VEC_DUCK_HULL_MIN_SCALED( player ) :
		VEC_HULL_MIN_SCALED( player );
}

Vector CGameMovement::GetPlayerMaxs( void ) const
{
	if ( player && player->m_bOnHorse )
		return Vector( 16.0f, 16.0f, 100.0f ) * player->GetModelScale();
	if ( player->IsObserver() )
		return VEC_OBS_HULL_MAX_SCALED( player );
	return player->m_Local.m_bDucked ? VEC_DUCK_HULL_MAX_SCALED( player ) :
		VEC_HULL_MAX_SCALED( player );
}

void CGameMovement::CheckParameters( void )
{
	QAngle v_angle;

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		player->GetMoveType() != MOVETYPE_NOCLIP &&
		player->GetMoveType() != MOVETYPE_OBSERVER )
	{
		float spd =
			mv->m_flForwardMove * mv->m_flForwardMove +
			mv->m_flSideMove * mv->m_flSideMove +
			mv->m_flUpMove * mv->m_flUpMove;

		const float maxspeed = mv->m_flClientMaxSpeed;
		if ( maxspeed != 0.0f )
			mv->m_flMaxSpeed = MIN( maxspeed, mv->m_flMaxSpeed );

		float flSpeedFactor = 1.0f;
		if ( player->m_pSurfaceData )
			flSpeedFactor = player->m_pSurfaceData->game.maxSpeedFactor;

		const float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if ( flConstraintSpeedFactor < flSpeedFactor )
			flSpeedFactor = flConstraintSpeedFactor;
		mv->m_flMaxSpeed *= flSpeedFactor;

		if ( g_bMovementOptimizations )
		{
			if ( spd != 0.0f &&
				spd > mv->m_flMaxSpeed * mv->m_flMaxSpeed )
			{
				// Keep the original x86 sqrtss/divss/mulss sequence in this
				// function. Returning the ratio through a helper adds a rounding
				// boundary and turns 234.999985 into 235.0.
				float fRatio = mv->m_flMaxSpeed / sqrtf( spd );
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove *= fRatio;
				mv->m_flUpMove *= fRatio;
			}
		}
		else
		{
			spd = sqrt( spd );
			if ( spd != 0.0f && spd > mv->m_flMaxSpeed )
			{
				const float fRatio = mv->m_flMaxSpeed / spd;
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove *= fRatio;
				mv->m_flUpMove *= fRatio;
			}
		}
	}

	if ( ( player->GetFlags() & FL_FROZEN ) ||
		( player->GetFlags() & FL_ONTRAIN ) ||
		( IsDead() && !player->IsObserver() ) )
	{
		mv->m_flForwardMove = 0.0f;
		mv->m_flSideMove = 0.0f;
		mv->m_flUpMove = 0.0f;
	}

	DecayPunchAngle();
	if ( !IsDead() || player->IsObserver() )
	{
		v_angle = mv->m_vecAngles;
		v_angle += player->m_Local.m_vecPunchAngle;
		if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
			player->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecAngles[ROLL] = CalcRoll( v_angle, mv->m_vecVelocity,
				sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0f;
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW] = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	if ( IsDead() && !player->IsObserver() )
		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT_SCALED( player ) );

	if ( mv->m_vecAngles[YAW] > 180.0f )
		mv->m_vecAngles[YAW] -= 360.0f;
}

void CGameMovement::AirAccelerate(
	Vector &wishdir, float wishspeed, float accel )
{
	float wishspd = wishspeed;

	if ( player->pl.deadflag || player->m_flWaterJumpTime ||
		player->m_flKickedPenaltyTime > gpGlobals->curtime )
	{
		return;
	}

	float flAirSpeedCap = 20.0f;
	CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
	if ( pWeapon && player->m_flKickTime <= gpGlobals->curtime )
	{
		const int nWeaponID = pWeapon ? pWeapon->FoFWeaponID() : -1;
		if ( nWeaponID == 0 || nWeaponID == 8 )
			flAirSpeedCap = GetAirSpeedCap();
	}
	if ( wishspd > flAirSpeedCap )
		wishspd = flAirSpeedCap;

	const float currentspeed = mv->m_vecVelocity.Dot( wishdir );
	const float addspeed = wishspd - currentspeed;
	if ( addspeed <= 0.0f )
		return;

	float accelspeed = accel * wishspeed * gpGlobals->frametime *
		player->m_surfaceFriction;
	if ( accelspeed > addspeed )
		accelspeed = addspeed;

	for ( int i = 0; i < 3; ++i )
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];
		mv->m_outWishVel[i] += accelspeed * wishdir[i];
	}
}

void CGameMovement::AirMove( void )
{
	Vector wishvel;
	Vector wishdir;
	Vector forward, right, up;
	AngleVectors( mv->m_vecViewAngles, &forward, &right, &up );

	float fmove = mv->m_flForwardMove;
	float smove = mv->m_flSideMove;
	CBaseEntity *pKicker = FoFKicker( player );
	if ( pKicker && pKicker->IsPlayer() )
	{
		fmove *= 0.25f;
		smove *= 0.25f;
	}

	forward[2] = 0.0f;
	right[2] = 0.0f;
	VectorNormalize( forward );
	VectorNormalize( right );

	for ( int i = 0; i < 2; ++i )
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] = 0.0f;

	VectorCopy( wishvel, wishdir );
	float wishspeed = VectorNormalize( wishdir );
	if ( wishspeed != 0.0f && wishspeed > mv->m_flMaxSpeed )
	{
		VectorScale( wishvel, mv->m_flMaxSpeed / wishspeed, wishvel );
		wishspeed = mv->m_flMaxSpeed;
	}

	AirAccelerate( wishdir, wishspeed, sv_airaccelerate.GetFloat() );
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	TryPlayerMove();
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
		mv->m_vecVelocity );
}

bool CGameMovement::CanAccelerate( void )
{
	// FoF CGameMovement vtable slot 23
	// only rejects acceleration while water-jumping. In particular, it does
	// not reject dead roaming observers, so FullNoClipMove can replenish the
	// speed removed by spectator friction each predicted command.
	return player->m_flWaterJumpTime == 0.0f;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF ground movement, weapon movement modifiers, and legacy slide.
//
//=============================================================================//

extern bool g_bMovementOptimizations;

static bool FoFWeaponHasMovementReloadActivity(
	const CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	const Activity activity = pWeapon->GetActivity();
	return activity == ACT_VM_RELOAD ||
		activity == ACT_VM_IDLE_8 ||
		activity == FoFShotgunReloadStartActivity( pWeapon ) ||
		activity == FoFShotgunReloadFinishActivity( pWeapon );
}

// The shipped FoF client and server update this strict, unscaled network
// float with scalar SSE instructions.  Keeping the multiply and add/subtract
// as separate single-precision operations matters: an x87 expression retains
// the product in extended precision and differs by one ULP after prediction.
static float FoFIncreaseMovementSpeedPenalty( float flPenalty )
{
	const __m128 frameTime = _mm_set_ss( gpGlobals->frametime );
	const __m128 step = _mm_mul_ss( frameTime, _mm_set_ss( 30.0f ) );
	return _mm_cvtss_f32( _mm_add_ss( _mm_set_ss( flPenalty ), step ) );
}

static float FoFRelaxMovementSpeedPenalty( float flPenalty )
{
	const __m128 frameTime = _mm_set_ss( gpGlobals->frametime );
	const __m128 step = _mm_mul_ss( frameTime, _mm_set_ss( 10.0f ) );
	return _mm_cvtss_f32( _mm_sub_ss( _mm_set_ss( flPenalty ), step ) );
}

void CGameMovement::WalkMove( void )
{
	Vector wishvel;
	Vector wishdir;
	Vector dest;
	trace_t pm;
	Vector forward, right, up;
	AngleVectors( mv->m_vecViewAngles, &forward, &right, &up );

	CHandle< CBaseEntity > oldground = player->GetGroundEntity();
	float fmove = mv->m_flForwardMove;
	float smove = mv->m_flSideMove;

	// The original client decays the repeated-jump
	// accumulator while grounded, then applies its 1.0 -> 0.65 movement scale.
	if ( oldground != NULL && player->consecutiveJumps > 0.0f )
		player->consecutiveJumps -= gpGlobals->frametime * 0.5f;

	const float flJumpMoveScale = RemapValClamped( player->consecutiveJumps,
		0.01f, 1.0f, 1.0f, 0.65f );
	fmove *= flJumpMoveScale;
	smove *= flJumpMoveScale;

	// CFoF_Player::IsWalking (original virtual slot 309).
	const bool bFoFWalking = FoFWalkFactor( player ) > 0.1f &&
		player->GetAbsVelocity().Length() > 5.0f;

	float flSpeedPenalty = FoFSpeedPenalty( player );
	if ( !bFoFWalking && !player->m_Local.m_bDucked &&
		fmove < 0.0f && flSpeedPenalty < 50.0f )
	{
		flSpeedPenalty = FoFIncreaseMovementSpeedPenalty( flSpeedPenalty );
		FoFSetSpeedPenalty( player, flSpeedPenalty );
	}

	if ( FoFPlayerInfo( player ) & 0x10 )
	{
		CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
		if ( pWeapon )
		{
			float flSightMoveEndpoint = pWeapon->FoFSightMoveEndpoint();
			const int nHandStance = FoFHandStance( player );
			if ( FoFIsRevolverWeapon( pWeapon ) )
			{
				if ( nHandStance == 1 )
					flSightMoveEndpoint = 0.7f;
				else if ( nHandStance == 3 )
					flSightMoveEndpoint = 0.75f;
			}

			const float flSightMoveScale = RemapValClamped(
				FoFSightExpFactor( player ),
				0.0f, 1.0f, 1.0f, flSightMoveEndpoint );
			fmove *= flSightMoveScale;
			smove *= flSightMoveScale;
		}
	}

	if ( flSpeedPenalty > 0.0f && FoFSightExpFactor( player ) < 0.5f )
	{
		flSpeedPenalty = FoFRelaxMovementSpeedPenalty( flSpeedPenalty );
		FoFSetSpeedPenalty( player, flSpeedPenalty );

		// The original client keeps both the decay and
		// movement scaling inside the same sight < 0.5 branch. Once ADS has
		// crossed 0.5, an existing backwards-movement penalty is retained but
		// deliberately does not reduce movement until the sight is lowered.
		static ConVarRef fofSpeedPenalty( "fof_sv_speedpenalty", true );
		const float flPenaltyFloor = fofSpeedPenalty.IsValid() ?
			fofSpeedPenalty.GetFloat() : 1.0f;
		const float flPenaltyScale = RemapValClamped( flSpeedPenalty,
			100.0f, 0.0f, flPenaltyFloor, 1.0f );
		fmove *= flPenaltyScale;
		smove *= flPenaltyScale;
	}

	if ( FoFPickupActive( player ) )
	{
		const float flPickupScale =
			( FoFPlayerInfo( player ) & 0x800000 ) ? 0.9f : 0.75f;
		fmove *= flPickupScale;
		smove *= flPickupScale;
	}

	if ( player->m_Local.m_bDucking )
	{
		fmove *= 0.75f;
		smove *= 0.75f;
	}

	const bool bFoFReloadMovement =
		FoFWeaponHasMovementReloadActivity( player->GetActiveWeapon1() ) ||
		FoFWeaponHasMovementReloadActivity( player->GetActiveWeapon2() );
	if ( bFoFReloadMovement )
	{
		CBaseCombatWeapon *pActiveWeapon = player->GetActiveWeapon();
		if ( pActiveWeapon )
		{
			const int nWeaponID = pActiveWeapon->FoFWeaponID();
			float flReloadMoveScale = 1.0f;
			if ( nWeaponID == 4 || nWeaponID == 10 )
				flReloadMoveScale = 0.85f;
			else if ( nWeaponID != 1 )
				flReloadMoveScale = 0.65f;

			fmove *= flReloadMoveScale;
			smove *= flReloadMoveScale;
		}
	}

	if ( g_bMovementOptimizations )
	{
		if ( forward[2] != 0.0f )
		{
			forward[2] = 0.0f;
			VectorNormalize( forward );
		}
		if ( right[2] != 0.0f )
		{
			right[2] = 0.0f;
			VectorNormalize( right );
		}
	}
	else
	{
		forward[2] = 0.0f;
		right[2] = 0.0f;
		VectorNormalize( forward );
		VectorNormalize( right );
	}

	for ( int i = 0; i < 2; ++i )
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] = 0.0f;
	VectorCopy( wishvel, wishdir );
	float wishspeed = VectorNormalize( wishdir );

	if ( wishspeed != 0.0f && wishspeed > mv->m_flMaxSpeed )
	{
		VectorScale( wishvel, mv->m_flMaxSpeed / wishspeed, wishvel );
		wishspeed = mv->m_flMaxSpeed;
	}

	mv->m_vecVelocity[2] = 0.0f;
	float flAcceleration = sv_accelerate.GetFloat();
	if ( ( FoFPlayerInfo( player ) & 0x10 ) &&
		FoFSightExpFactor( player ) > 0.5f )
	{
		flAcceleration *= 1.45f;
	}
	else if ( bFoFWalking )
	{
		flAcceleration *= 1.7f;
	}
	else if ( player->GetFlags() & ( FL_DUCKING | FL_ANIMDUCKING ) )
	{
		flAcceleration *= 1.9f;
	}
	if ( wishspeed < 1.0f )
		flAcceleration *= 0.1f;

	Accelerate( wishdir, wishspeed, flAcceleration );
	mv->m_vecVelocity[2] = 0.0f;
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	if ( VectorLength( mv->m_vecVelocity ) < 1.0f )
	{
		mv->m_vecVelocity.Init();
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
			mv->m_vecVelocity );
		return;
	}

	dest[0] = mv->GetAbsOrigin()[0] +
		mv->m_vecVelocity[0] * gpGlobals->frametime;
	dest[1] = mv->GetAbsOrigin()[1] +
		mv->m_vecVelocity[1] * gpGlobals->frametime;
	dest[2] = mv->GetAbsOrigin()[2];
	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(),
		COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	mv->m_outWishVel += wishdir * wishspeed;

	if ( pm.fraction == 1.0f )
	{
		mv->SetAbsOrigin( pm.endpos );
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
			mv->m_vecVelocity );
		StayOnGround();
		return;
	}

	if ( oldground == NULL && player->GetWaterLevel() == WL_NotInWater )
	{
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
			mv->m_vecVelocity );
		return;
	}

	if ( player->m_flWaterJumpTime )
	{
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
			mv->m_vecVelocity );
		return;
	}

	StepMove( dest, pm );
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(),
		mv->m_vecVelocity );
	StayOnGround();
}

void CGameMovement::FoFGroundMove( const QAngle &moveAngles,
	float flForwardMove, float flSideMove, float flWishSpeedCap,
	float flAcceleration )
{
	Vector forward, right, up;
	AngleVectors( moveAngles, &forward, &right, &up );

	CHandle< CBaseEntity > oldground = player->GetGroundEntity();
	forward.z = 0.0f;
	right.z = 0.0f;
	VectorNormalize( forward );
	VectorNormalize( right );

	Vector wishvel;
	wishvel.x = forward.x * flForwardMove + right.x * flSideMove;
	wishvel.y = forward.y * flForwardMove + right.y * flSideMove;
	wishvel.z = 0.0f;

	Vector wishdir = wishvel;
	float wishspeed = VectorNormalize( wishdir );
	if ( wishspeed > 0.0f && flWishSpeedCap > 0.0f && wishspeed > flWishSpeedCap )
	{
		VectorScale( wishvel, flWishSpeedCap / wishspeed, wishvel );
		wishspeed = flWishSpeedCap;
	}

	mv->m_vecVelocity.z = 0.0f;
	Accelerate( wishdir, wishspeed,
		wishspeed < 1.0f ? flAcceleration * 0.1f : flAcceleration );
	mv->m_vecVelocity.z = 0.0f;

	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	if ( mv->m_vecVelocity.Length() < 1.0f )
	{
		mv->m_vecVelocity.Init();
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	Vector dest;
	dest.x = mv->GetAbsOrigin().x + mv->m_vecVelocity.x * gpGlobals->frametime;
	dest.y = mv->GetAbsOrigin().y + mv->m_vecVelocity.y * gpGlobals->frametime;
	dest.z = mv->GetAbsOrigin().z;

	trace_t pm;
	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(),
		COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	mv->m_outWishVel += wishdir * wishspeed;

	if ( pm.fraction == 1.0f )
	{
		mv->SetAbsOrigin( pm.endpos );
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		StayOnGround();
		return;
	}

	if ( oldground == NULL && player->GetWaterLevel() == WL_NotInWater )
	{
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	if ( player->m_flWaterJumpTime )
	{
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	StepMove( dest, pm );
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	StayOnGround();
}

void CGameMovement::FoFLegacySlideMove( void )
{
	FoFGroundMove( player->m_angSlideView, 0.0f, 0.0f,
		mv->m_flMaxSpeed, sv_accelerate.GetFloat() );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF mounted movement and horse sliding on both simulation sides.
//
//=============================================================================//

bool CGameMovement::FoFTryHorseWalkMove( void )
{
	if ( !player->m_bOnHorse )
		return false;

	if ( player->GetGroundEntity() != NULL )
	{
		mv->m_vecVelocity.z = 0.0f;
		player->m_Local.m_flFallVelocity = 0.0f;
		Friction();
	}

	CBaseCombatWeapon *pHorseWeapon = player->GetActiveWeapon();
	const int nHorseWeaponID = pHorseWeapon ?
		pHorseWeapon->FoFWeaponID() : -1;
	const bool bFreeHorseAim = FoFUsesHorseFreeAim( player ) &&
		pHorseWeapon && nHorseWeaponID != 0 && nHorseWeaponID != 8;
	if ( !bFreeHorseAim )
		FoFSetHorseAngles( player, mv->m_vecViewAngles );

	CheckVelocity();

	if ( ( mv->m_nButtons & IN_WALK ) && !( mv->m_nButtons & IN_DUCK ) &&
		player->m_flSlideForce < 0.1f )
	{
		if ( player->m_flHorseAcc <= 1.85f )
			player->m_flHorseAcc += gpGlobals->frametime * 0.25f;
	}
	else if ( player->m_flHorseAcc > 0.75f )
	{
		player->m_flHorseAcc -= gpGlobals->frametime * 0.5f;
	}

	const float flHorseSpeed = mv->m_vecVelocity.Length2D();
	if ( player->m_flSlideForce < 0.0f && flHorseSpeed > 325.0f )
	{
		player->m_flSlideForce = clamp( player->m_flSlideForce +
			gpGlobals->frametime * 0.8f, -1.0f, 0.0f );
	}
	// FoF uses a second, independent condition here.  In particular,
	// an exact zero is preserved while the horse is faster than 325 units/s;
	// making this the else arm above incorrectly nudges zero negative every
	// predicted command on fast slopes.
	if ( player->m_flSlideForce > 0.0f || flHorseSpeed <= 325.0f )
	{
		player->m_flSlideForce = clamp( player->m_flSlideForce -
			gpGlobals->frametime * 0.86f, -1.0f, 1.0f );
		if ( player->m_flSlideForce > 0.0f &&
			player->m_flSlideForce < 0.05f )
		{
			player->m_flSlideForce = -0.75f;
		}
	}

	if ( player->IsAlive() && player->m_flSlideForce > 0.0f )
		FoFHorseSlideMove();
	else if ( player->GetGroundEntity() != NULL )
		FoFHorseMove( 0.0f );
	else
		AirMove();

	CategorizePosition();
	CheckVelocity();
	if ( !CheckWater() )
		FinishGravity();
	if ( player->GetGroundEntity() != NULL )
		mv->m_vecVelocity.z = 0.0f;
	CheckFalling();

	if ( ( m_nOldWaterLevel == WL_NotInWater &&
		player->GetWaterLevel() != WL_NotInWater ) ||
		( m_nOldWaterLevel != WL_NotInWater &&
		player->GetWaterLevel() == WL_NotInWater ) )
	{
		PlaySwimSound();
	}

	return true;
}

void CGameMovement::FoFHorseSlideMove( void )
{
	const float flScale = 0.6f +
		clamp( ( player->m_flSlideForce - 0.25f ) * 2.0f, 0.0f, 1.0f ) * 0.4f;
	const QAngle slideView = player->m_angSlideView;
	const Vector slideMove = player->m_vecSlide;
	FoFGroundMove( slideView,
		slideMove.x * flScale, slideMove.y * flScale,
		mv->m_flMaxSpeed, sv_accelerate.GetFloat() );
}

void CGameMovement::FoFHorseMove( float flForwardScale )
{
	CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
	const int nWeaponID = pWeapon ? pWeapon->FoFWeaponID() : -1;
	const bool bFreeHorseAim = FoFUsesHorseFreeAim( player ) &&
		player->IsOnFoFHorse() && pWeapon && nWeaponID != 0 && nWeaponID != 8;

	QAngle moveAngles = bFreeHorseAim ? FoFHorseAngles( player ) : mv->m_vecViewAngles;

	float flScale = flForwardScale;
	if ( flScale <= 0.0f )
		flScale = mv->m_flForwardMove < 0.0f ? 0.6f : player->m_flHorseAcc;

	float flForwardMove = mv->m_flForwardMove * flScale;
	float flSideMove = bFreeHorseAim ? 0.0f : mv->m_flSideMove * 0.55f;

	float flPenalty = FoFSpeedPenalty( player );
	if ( flPenalty > 0.0f )
	{
		flPenalty = MAX( 0.0f, flPenalty - gpGlobals->frametime * 5.0f );
		FoFSetSpeedPenalty( player, flPenalty );
		const float flPenaltyScale = RemapValClamped( flPenalty,
			100.0f, 0.0f, 0.5f, 1.0f );
		flForwardMove *= flPenaltyScale;
		flSideMove *= flPenaltyScale;
	}

	FoFGroundMove( moveAngles, flForwardMove, flSideMove, 500.0f, 3.4f );

	if ( bFreeHorseAim )
	{
		QAngle horseAngles = FoFHorseAngles( player );
		const float flTurn = ( 170.0f - 95.0f *
			clamp( ( mv->m_vecVelocity.Length() - 50.0f ) / 350.0f, 0.0f, 1.0f ) ) *
			gpGlobals->frametime;
		if ( mv->m_nButtons & IN_MOVELEFT )
			horseAngles.y += flTurn;
		if ( mv->m_nButtons & IN_MOVERIGHT )
			horseAngles.y -= flTurn;
		FoFSetHorseAngles( player, horseAngles );
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF jump, wall-jump, landing, and crouch state machines.
//
//=============================================================================//

void CGameMovement::FoFDuckTransition( float duckFraction )
{
	if ( duckFraction >= 1.0f )
	{
		player->AddFlag( FL_DUCKING );
		player->m_Local.m_bDucked = true;
		player->m_Local.m_bDucking = false;
		player->SetViewOffset( GetPlayerViewOffset( true ) );
		return;
	}

	const float flFraction = clamp( duckFraction, 0.0f, 1.0f );
	SetDuckedEyeOffset( flFraction );
	const Vector vecStandHull =
		GetPlayerMaxs( false ) - GetPlayerMins( false );
	const Vector vecDuckHull =
		GetPlayerMaxs( true ) - GetPlayerMins( true );
	mv->SetAbsOrigin(
		mv->GetAbsOrigin() +
		( vecStandHull - vecDuckHull ) * ( 0.1f * flFraction ) );
	FixPlayerCrouchStuck( true );
	CategorizePosition();
}

void CGameMovement::FoFUnDuckTransition( float duckFraction )
{
	if ( duckFraction <= 0.0f )
	{
		player->m_Local.m_bDucked = false;
		player->RemoveFlag( FL_DUCKING | FL_ANIMDUCKING );
		player->m_Local.m_bDucking = false;
		player->m_Local.m_bInDuckJump = false;
		player->SetViewOffset( GetPlayerViewOffset( false ) );
		player->m_Local.m_flDucktime = 0.0f;
		return;
	}

	const float flFraction = clamp( duckFraction, 0.0f, 1.0f );
	SetDuckedEyeOffset( flFraction );
	const Vector vecStandHull =
		GetPlayerMaxs( false ) - GetPlayerMins( false );
	const Vector vecDuckHull =
		GetPlayerMaxs( true ) - GetPlayerMins( true );
	mv->SetAbsOrigin(
		mv->GetAbsOrigin() -
		( vecStandHull - vecDuckHull ) * ( 0.1f * flFraction ) );
	FixPlayerCrouchStuck( true );
	CategorizePosition();
}

bool CGameMovement::CheckJumpButton( void )
{
	if ( player->pl.deadflag )
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;
	}

	// FoF tests the latch before both wall-jump and ordinary jump paths.
	if ( mv->m_nOldButtons & IN_JUMP )
		return false;

	if ( !( player->GetFlags() & FL_ONGROUND ) &&
		!FoFPickupActive( player ) && player->m_flJWallForce < 0.65f &&
		player->m_Local.m_flJumpTime < 240.0f )
	{
		trace_t trace;
		const Vector vecWallMins( -25.0f, -25.0f, 0.0f );
		const Vector vecWallMaxs( 25.0f, 25.0f, 72.0f );
		UTIL_TraceHull(
			mv->GetAbsOrigin(), mv->GetAbsOrigin(),
			vecWallMins, vecWallMaxs, PlayerSolidMask(), player,
			COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		if ( trace.m_pEnt && !trace.m_pEnt->IsPlayer() )
		{
			const float flStartZ = mv->m_vecVelocity.z;
			player->m_Local.m_flJumpTime += 410.0f;

			if ( ( FoFPlayerInfo( player ) & 0x200000 ) &&
				FoFKicker( player ) == NULL )
			{
				Vector forward;
				AngleVectors( mv->m_vecViewAngles, &forward );
				forward.z = 0.0f;
				const float flForwardSpeed = RemapValClamped(
					player->m_flJWallForce,
					0.0f, 0.6f, 250.0f, 130.0f );
				mv->m_vecVelocity.x = forward.x * flForwardSpeed;
				mv->m_vecVelocity.y = forward.y * flForwardSpeed;
				mv->m_vecVelocity.z = RemapValClamped(
					player->m_flJWallForce,
					0.0f, 1.0f, 170.0f, 150.0f );
#ifndef CLIENT_DLL
				MoveHelper()->StartSound(
					mv->GetAbsOrigin(), "FoFPlayer.SideJump" );
#endif
			}
			else
			{
				mv->m_vecVelocity.z = RemapValClamped(
					player->m_flJWallForce,
					0.0f, 0.6f, 170.0f, 150.0f );
			}

			FinishGravity();
			mv->m_outJumpVel.z += mv->m_vecVelocity.z - flStartZ;
			mv->m_outStepHeight += 0.1f;
			OnJump( mv->m_outJumpVel.z );
			MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
			player->m_flJWallForce = 1.0f;
			CFoF_Player *pFoFPlayer =
				dynamic_cast< CFoF_Player * >( player );
			if ( pFoFPlayer )
				pFoFPlayer->SetFoFLastWallJumpTime( gpGlobals->curtime );
			return true;
		}
	}

	if ( player->m_flWaterJumpTime )
	{
		player->m_flWaterJumpTime -= gpGlobals->frametime;
		if ( player->m_flWaterJumpTime < 0.0f )
			player->m_flWaterJumpTime = 0.0f;
		return false;
	}

	if ( player->GetWaterLevel() >= WL_Waist )
	{
		SetGroundEntity( NULL );
		if ( player->GetWaterType() == CONTENTS_WATER )
			mv->m_vecVelocity[2] = 100.0f;
		else if ( player->GetWaterType() == CONTENTS_SLIME )
			mv->m_vecVelocity[2] = 80.0f;

		if ( player->m_flSwimSoundTime <= 0.0f )
		{
			player->m_flSwimSoundTime = 1000.0f;
			PlaySwimSound();
		}
		return false;
	}

	if ( player->GetGroundEntity() == NULL )
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;
	}

	if ( player->m_Local.m_bDucking &&
		( player->GetFlags() & FL_DUCKING ) )
	{
		return false;
	}
	if ( player->m_Local.m_flDuckJumpTime > 0.0f )
		return false;

	SetGroundEntity( NULL );
	player->m_Local.m_flJumpTime = 410.0f;
	player->m_Local.m_bInDuckJump = false;

	const float flHorizontalSpeed = mv->m_vecVelocity.Length2D();
	if ( FoFPickupActive( player ) )
	{
		const float flPickupMultiplier =
			( FoFPlayerInfo( player ) & 0x800000 ) ? 0.85f : 0.8f;
		mv->m_vecVelocity.x *= flPickupMultiplier;
		mv->m_vecVelocity.y *= flPickupMultiplier;
	}
	else
	{
		static ConVarRef classicShootout(
			"fof_sv_classic_shootout", true );
		static ConVarRef currentMode( "fof_sv_currentmode", true );
		const bool bClassicShootout =
			classicShootout.IsValid() && classicShootout.GetBool();

		float flHorizontalMultiplier = 1.0f;
		if ( !bClassicShootout )
		{
			if ( player->consecutiveJumps > 0.2f )
			{
				flHorizontalMultiplier = RemapValClamped(
					player->consecutiveJumps,
					0.2f, 0.5f, 1.0f, 0.75f );
			}
		}
		else
		{
			const int nCurrentMode =
				currentMode.IsValid() ? currentMode.GetInt() : 1;
			if ( nCurrentMode == 1 )
			{
				flHorizontalMultiplier = RemapValClamped(
					player->consecutiveJumps,
					0.2f, 0.5f, 1.0f, 1.15f );
			}
		}

		mv->m_vecVelocity.x *= flHorizontalMultiplier;
		mv->m_vecVelocity.y *= flHorizontalMultiplier;
	}

	if ( flHorizontalSpeed > 0.0f &&
		flHorizontalSpeed > mv->m_flMaxSpeed )
	{
		VectorScale(
			mv->m_vecVelocity,
			mv->m_flMaxSpeed / flHorizontalSpeed,
			mv->m_vecVelocity );
	}

	player->PlayStepSound(
		(Vector &)mv->GetAbsOrigin(),
		player->m_pSurfaceData, 1.0f, true );
	MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );

	const float flStartZ = mv->m_vecVelocity.z;
	mv->m_vecVelocity.z =
		( player->GetFlags() & FL_FAKECLIENT ) ? 240.0f : 225.0f;
	FinishGravity();

	mv->m_outJumpVel.z += mv->m_vecVelocity.z - flStartZ;
	mv->m_outStepHeight += 0.1f;
	OnJump( mv->m_outJumpVel.z );

	if ( !( player->GetFlags() & FL_FAKECLIENT ) )
		player->consecutiveJumps += 0.23f;

	mv->m_nOldButtons |= IN_JUMP;
	return true;
}

void CGameMovement::HandleDuckingSpeedCrop( void )
{
	const int nDuckFlags = FL_DUCKING | FL_ANIMDUCKING;
	const float flDuckScale =
		( FoFPlayerInfo( player ) & 0x40000 ) ? 0.75f : 0.45f;

	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) &&
		( player->GetFlags() & nDuckFlags ) &&
		player->GetGroundEntity() != NULL )
	{
		mv->m_flForwardMove *= flDuckScale;
		mv->m_flSideMove *= flDuckScale;
		mv->m_flUpMove *= flDuckScale;
		m_iSpeedCropped |= SPEED_CROPPED_DUCK;
	}
}

void CGameMovement::Duck( void )
{
	// The original client uses a reduced HL2MP crouch
	// state machine.  Kicking and mounted movement suppress crouching.
	if ( player->m_flKickTime > gpGlobals->curtime || player->m_bOnHorse )
		return;

	const int buttonsChanged = mv->m_nOldButtons ^ mv->m_nButtons;
	const int buttonsReleased = buttonsChanged & mv->m_nOldButtons;
	const bool bInAir = player->GetGroundEntity() == NULL;
	const bool bInDuck = ( player->GetFlags() & FL_DUCKING ) != 0;
	const bool bDuckJumpTime =
		player->m_Local.m_flDuckJumpTime > 0.0f;

	if ( bInAir && !player->m_Local.m_bDucked )
		return;

	if ( mv->m_nButtons & IN_DUCK )
		mv->m_nOldButtons |= IN_DUCK;
	else
		mv->m_nOldButtons &= ~IN_DUCK;

	if ( IsDead() )
		return;

	HandleDuckingSpeedCrop();

	if ( !( mv->m_nButtons & IN_DUCK ) &&
		!player->m_Local.m_bDucking && !bInDuck )
	{
		return;
	}

	if ( ( mv->m_nButtons & IN_DUCK ) &&
		!player->m_Local.m_bInDuckJump )
	{
		player->AddFlag( FL_ANIMDUCKING );

		if ( !bInDuck && !player->m_Local.m_bDucking )
		{
			player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
			player->m_Local.m_bDucking = true;
		}

		if ( !player->m_Local.m_bDucking )
			return;

		const float flDuckMilliseconds = MAX(
			0.0f, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_flDucktime );
		const float flDuckSeconds = flDuckMilliseconds * 0.001f;
		if ( ( flDuckSeconds > TIME_TO_DUCK || bInDuck ) && !bInAir )
			FinishDuck();
		else
			SetDuckedEyeOffset(
				SimpleSpline( flDuckSeconds / TIME_TO_DUCK ) );
		return;
	}

	player->RemoveFlag( FL_ANIMDUCKING );
	if ( bDuckJumpTime )
		return;

	if ( !player->m_Local.m_bAllowAutoMovement &&
		!player->m_Local.m_bDucking )
	{
		return;
	}

	if ( buttonsReleased & IN_DUCK )
	{
		player->m_Local.m_bInDuckJump = true;
		if ( bInDuck )
		{
			player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
		}
		else if ( player->m_Local.m_bDucking &&
			!player->m_Local.m_bDucked )
		{
			const float flUnduckMilliseconds = 1000.0f * TIME_TO_DUCK;
			const float flDuckMilliseconds = 1000.0f * TIME_TO_DUCK;
			const float flElapsedMilliseconds =
				GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_flDucktime;
			const float flDuckedFraction =
				flElapsedMilliseconds / flDuckMilliseconds;
			player->m_Local.m_flDucktime =
				GAMEMOVEMENT_DUCK_TIME - flUnduckMilliseconds +
				flDuckedFraction * flUnduckMilliseconds;
		}
	}

	if ( CanUnduck() )
	{
		if ( !player->m_Local.m_bDucking && !player->m_Local.m_bDucked )
			return;

		const float flDuckMilliseconds = MAX(
			0.0f, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_flDucktime );
		const float flDuckSeconds = flDuckMilliseconds * 0.001f;
		if ( flDuckSeconds > TIME_TO_DUCK || bInAir )
		{
			FinishUnDuck();
		}
		else
		{
			SetDuckedEyeOffset(
				SimpleSpline( 1.0f - flDuckSeconds / TIME_TO_DUCK ) );
			player->m_Local.m_bDucking = true;
		}
	}
	else if ( player->m_Local.m_flDucktime != GAMEMOVEMENT_DUCK_TIME )
	{
		SetDuckedEyeOffset( 1.0f );
		player->m_Local.m_flDucktime = GAMEMOVEMENT_DUCK_TIME;
		player->m_Local.m_bDucked = true;
		player->m_Local.m_bDucking = false;
		player->AddFlag( FL_DUCKING );
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF slide state, steering, movement, and slope decay on both sides.
//
//=============================================================================//

void CGameMovement::FoFSlideMove( void )
{
	// Keep the ordinary slide path self-contained.  The shipped client has a
	// complete movement routine in this virtual slot rather than forwarding to
	// the shared horse/legacy helper.  Besides removing the runtime wish-speed
	// cap branch, that keeps the scale, direction normalization, acceleration
	// and destination trace in the same floating-point instruction stream.
	Vector forward, right, up;
	AngleVectors( player->m_angSlideView, &forward, &right, &up );

	CHandle< CBaseEntity > oldground = player->GetGroundEntity();
	// /fp:fast otherwise reassociates (force - .25f) * 2.  The original x86
	// client emits subss followed by mulss, and the one-ULP difference can move
	// the following displacement trace onto another plane.
	__m128 slideScale = _mm_sub_ss(
		_mm_set_ss( player->m_flSlideForce ), _mm_set_ss( 0.25f ) );
	slideScale = _mm_mul_ss( slideScale, _mm_set_ss( 2.0f ) );
	slideScale = _mm_max_ss( slideScale, _mm_setzero_ps() );
	slideScale = _mm_min_ss( slideScale, _mm_set_ss( 1.0f ) );
	// The shipped image stores 0x3ECCCCCC, one ULP below 0.4f.
	slideScale = _mm_mul_ss(
		slideScale, _mm_set_ss( 0.39999997615814208984375f ) );
	slideScale = _mm_add_ss( slideScale, _mm_set_ss( 1.0f ) );
	const float flScale = _mm_cvtss_f32( slideScale );

	if ( g_bMovementOptimizations )
	{
		if ( forward.z != 0.0f )
		{
			forward.z = 0.0f;
			VectorNormalize( forward );
		}
		if ( right.z != 0.0f )
		{
			right.z = 0.0f;
			VectorNormalize( right );
		}
	}
	else
	{
		forward.z = 0.0f;
		right.z = 0.0f;
		VectorNormalize( forward );
		VectorNormalize( right );
	}

	Vector wishvel;
	wishvel.x = forward.x * ( player->m_vecSlide[0] * flScale ) +
		right.x * ( player->m_vecSlide[1] * flScale );
	wishvel.y = forward.y * ( player->m_vecSlide[0] * flScale ) +
		right.y * ( player->m_vecSlide[1] * flScale );
	wishvel.z = 0.0f;

	Vector wishdir = wishvel;
	const float wishspeed = VectorNormalize( wishdir );

	mv->m_vecVelocity.z = 0.0f;
	Accelerate(
		wishdir, wishspeed,
		wishspeed < 1.0f ?
			sv_accelerate.GetFloat() * 0.1f : sv_accelerate.GetFloat() );
	mv->m_vecVelocity.z = 0.0f;

	VectorAdd(
		mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	if ( mv->m_vecVelocity.Length() < 1.0f )
	{
		mv->m_vecVelocity.Init();
		VectorSubtract(
			mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	Vector dest;
	dest.x = mv->GetAbsOrigin().x +
		mv->m_vecVelocity.x * gpGlobals->frametime;
	dest.y = mv->GetAbsOrigin().y +
		mv->m_vecVelocity.y * gpGlobals->frametime;
	dest.z = mv->GetAbsOrigin().z;

	trace_t pm;
	TracePlayerBBox(
		mv->GetAbsOrigin(), dest, PlayerSolidMask(),
		COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	mv->m_outWishVel += wishdir * wishspeed;

	if ( pm.fraction == 1.0f )
	{
		mv->SetAbsOrigin( pm.endpos );
		VectorSubtract(
			mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		StayOnGround();
		return;
	}

	if ( oldground == NULL && player->GetWaterLevel() == WL_NotInWater )
	{
		VectorSubtract(
			mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	if ( player->m_flWaterJumpTime )
	{
		VectorSubtract(
			mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	StepMove( dest, pm );
	VectorSubtract(
		mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
	StayOnGround();
}

void CGameMovement::FoFUpdateSlideState( void )
{
	const bool bKickActive = player->m_flKickTime > gpGlobals->curtime;
	// The original computes this with an explicit sqrtss before comparing 200.
	const float flSpeedSqr =
		mv->m_vecVelocity.x * mv->m_vecVelocity.x +
		mv->m_vecVelocity.y * mv->m_vecVelocity.y;
	const float flSpeed = _mm_cvtss_f32(
		_mm_sqrt_ss( _mm_set_ss( flSpeedSqr ) ) );

	if ( player->m_flSlideForce < 0.0f && flSpeed > 200.0f )
	{
		player->m_flSlideForce = clamp(
			player->m_flSlideForce + gpGlobals->frametime * 1.1f,
			-1.0f, 0.0f );
	}
	else if ( player->m_flSlideForce > 0.0f || flSpeed <= 200.0f )
	{
		const float flDecay =
			player->GetGroundEntity() != NULL ? 0.36f : 2.5f;
		player->m_flSlideForce = clamp(
			player->m_flSlideForce - gpGlobals->frametime * flDecay,
			-1.0f, 1.0f );
	}

	if ( player->m_flJWallForce > 0.0f )
	{
		player->m_flJWallForce = clamp(
			player->m_flJWallForce - gpGlobals->frametime * 0.4f,
			0.0f, 1.0f );
	}

	if ( ( FoFPlayerInfo( player ) & 0x100000 ) &&
		FoFKicker( player ) == NULL && !bKickActive &&
		player->GetGroundEntity() != NULL &&
		( mv->m_nButtons & IN_DUCK ) &&
		player->m_flSlideForce <= 0.0f &&
		player->m_flSlideForce > -0.8f && flSpeed > 200.0f )
	{
		player->m_vecSlide.Init(
			mv->m_flForwardMove, mv->m_flSideMove, 0.0f );
		player->m_flSlideForce = RemapValClamped(
			player->m_flSlideForce, -0.8f, 0.0f, 0.55f, 1.0f );
		// Preserve command angles verbatim; prediction error tolerances handle
		// the server's ten-bit network angle quantization.
		player->m_angSlideView = mv->m_vecViewAngles;

#ifndef CLIENT_DLL
		if ( player->m_flSlideForce > 0.8f )
		{
			Vector forward;
			AngleVectors( mv->m_vecViewAngles, &forward );
			const Vector vecOrigin = mv->GetAbsOrigin();
			const Vector vecTraceStart = vecOrigin +
				forward * 25.0f + Vector( 0.0f, 0.0f, 25.0f );
			const Vector vecTraceEnd =
				vecTraceStart - Vector( 0.0f, 0.0f, 50.0f );
			trace_t traceGround;
			UTIL_TraceLine(
				vecTraceStart, vecTraceEnd, PlayerSolidMask(), player,
				COLLISION_GROUP_PLAYER_MOVEMENT, &traceGround );
			if ( vecOrigin.z - traceGround.endpos.z > -5.0f )
			{
				MoveHelper()->StartSound(
					mv->GetAbsOrigin(), "Player.Slide" );
			}
		}
#endif
	}
}

bool CGameMovement::FoFShouldCheckJumpButton( void ) const
{
	return player->m_flSlideForce <= 0.0f;
}

bool CGameMovement::FoFKickBlocksGroundMove( void ) const
{
	return player->m_flKickTime > gpGlobals->curtime;
}

bool CGameMovement::FoFTrySlideMove( void )
{
	if ( !player->IsAlive() || player->m_flSlideForce <= 0.0f )
		return false;

	if ( !( mv->m_nButtons & IN_DUCK ) ||
		player->m_flKickedPenaltyTime > gpGlobals->curtime )
	{
		player->m_flSlideForce = 0.05f;
	}

	const bool bTurnRight = ( mv->m_nButtons & IN_MOVERIGHT ) != 0;
	const bool bTurnLeft = ( mv->m_nButtons & IN_MOVELEFT ) != 0;
	if ( bTurnRight || bTurnLeft )
	{
#ifdef CLIENT_DLL
		float &x = player->m_vecSlide.x;
		float &y = player->m_vecSlide.y;
#else
		float x = player->m_vecSlide.GetX();
		float y = player->m_vecSlide.GetY();
#endif

		// The shipped x86 client keeps this product in x87 until each component
		// is stored, and updates the two components sequentially.
		const double flTurnStep =
			(double)RemapValClamped(
				player->m_flSlideForce, 0.0f, 1.0f, 1.0f, 500.0f ) *
			(double)gpGlobals->frametime;

		if ( bTurnRight )
		{
			if ( x > y )
			{
				y = (float)( (double)y +
					( x > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
				x = (float)( (double)x -
					( y > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
			}
			else
			{
				x = (float)( (double)x +
					( y > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
				y = (float)( (double)y -
					( x > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
			}
		}

		if ( bTurnLeft )
		{
			if ( x > y )
			{
				y = (float)( (double)y -
					( x > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
				x = (float)( (double)x +
					( y > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
			}
			else
			{
				x = (float)( (double)x -
					( y > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
				y = (float)( (double)y +
					( x > 0.0f ? 1.0 : -1.0 ) * flTurnStep );
			}
		}

#ifndef CLIENT_DLL
		player->m_vecSlide.SetX( x );
		player->m_vecSlide.SetY( y );
#endif
	}

	FoFSlideMove();

	const Vector vecOrigin = mv->GetAbsOrigin();
	Vector vecGroundStart = vecOrigin;
	Vector vecGroundEnd = vecOrigin;
	vecGroundStart.z += 2.0f;
	vecGroundEnd.z -= player->GetStepSize();
	trace_t groundTrace;
	TracePlayerBBox(
		vecGroundStart, vecGroundEnd, PlayerSolidMask(),
		COLLISION_GROUP_PLAYER_MOVEMENT, groundTrace );

	Vector vecForward;
	AngleVectors( mv->m_vecViewAngles, &vecForward );
	// The original client probes vertically ten units ahead.
	const Vector vecLineStart = vecOrigin + vecForward * 10.0f;
	Vector vecLineEnd = vecLineStart;
	vecLineEnd.z -= 20.0f;
	trace_t lineTrace;
	UTIL_TraceLine(
		vecLineStart, vecLineEnd, PlayerSolidMask(), player,
		COLLISION_GROUP_PLAYER_MOVEMENT, &lineTrace );

	float flSlopeDecay = 0.55f;
	if ( mv->m_vecVelocity.z > 0.0f )
	{
		flSlopeDecay = RemapValClamped(
			groundTrace.plane.normal.z, 1.0f, 0.7f, 0.55f, 5.0f );
	}
	// The original comparison uses the player origin, not the lowered endpoint.
	else if ( vecOrigin.z > lineTrace.endpos.z )
	{
		flSlopeDecay = RemapValClamped(
			groundTrace.plane.normal.z, 1.0f, 0.7f, 0.55f, 0.1f );
	}

	player->m_flSlideForce -= gpGlobals->frametime * flSlopeDecay;
	if ( player->m_flSlideForce < 0.05f )
		player->m_flSlideForce = -1.0f;
	return true;
}
