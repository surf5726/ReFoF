//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF first-person viewmodel presentation transforms.
//
//=============================================================================//

#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "c_baseviewmodel.h"
#include "prediction.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_client_settings.h"
#include "fof/fof_viewmodel.h"
#include "fof/fof_weapon_activities.h"
#include "hltvcamera.h"
#include "hl2mp_weapon_parse.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "fof/fof_weapon_properties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern float g_lateralBob;
extern float g_verticalBob;

#define FOF_BOB_CYCLE_MAX 0.45f
#define FOF_BOB_UP 0.5f

// Retain the original client ConVars even though FoF's recovered bob formula
// uses fixed values.  Existing configurations and the console interface still
// expect these names to be registered.
static ConVar cl_bobcycle( "cl_bobcycle", "0.8" );
static ConVar cl_bob( "cl_bob", "0.002" );
static ConVar cl_bobup( "cl_bobup", "0.5" );

static ConVar v_iyaw_cycle(
	"v_iyaw_cycle", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar v_iroll_cycle(
	"v_iroll_cycle", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar v_ipitch_cycle(
	"v_ipitch_cycle", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar v_iyaw_level(
	"v_iyaw_level", "0.3", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar v_iroll_level(
	"v_iroll_level", "0.1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar v_ipitch_level(
	"v_ipitch_level", "0.3", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar fof_viewmodel_protected_sequences(
	"fof_viewmodel_protected_sequences", "0", FCVAR_CHEAT,
	"Keep a client-only viewmodel sequence clock across prediction replay." );

static float FoFViewmodelMovementFactor( const CBasePlayer *pPlayer )
{
	static EHANDLE s_hOwner;
	static float s_flAirTime = 0.0f;
	static int s_nLastUpdateFrame = -1;

	if ( !pPlayer )
	{
		s_hOwner = NULL;
		s_flAirTime = 0.0f;
		s_nLastUpdateFrame = -1;
		return 0.0f;
	}

	if ( s_hOwner.Get() != pPlayer )
	{
		s_hOwner = const_cast< CBasePlayer * >( pPlayer );
		s_flAirTime = 0.0f;
		s_nLastUpdateFrame = -1;
	}

	Vector vecPresentationVelocity = pPlayer->GetAbsVelocity();
	FoFStabilizeLocalPresentationVelocity( pPlayer, vecPresentationVelocity );
	const float flSpeedFactor = RemapValClamped(
		vecPresentationVelocity.Length2D(),
		0.0f, 300.0f, 0.0f, 1.0f );

	if ( s_nLastUpdateFrame != gpGlobals->framecount )
	{
		s_nLastUpdateFrame = gpGlobals->framecount;
		if ( ( pPlayer->GetFlags() & FL_ONGROUND ) == 0 )
			s_flAirTime += gpGlobals->frametime * 2.0f;
		else
			s_flAirTime -= gpGlobals->frametime * 3.5f;

		s_flAirTime = clamp( s_flAirTime, 0.0f, 0.5f );
	}

	return flSpeedFactor + RemapValClamped(
		s_flAirTime, 0.0f, 0.5f, 0.0f, 1.0f );
}

float CBaseHL2MPCombatWeapon::CalcViewmodelBob( void )
{
	static float s_flBobTime = 0.0f;
	static EHANDLE s_hBobOwner;
	static int s_nLastBobFrame = -1;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !gpGlobals || gpGlobals->frametime == 0.0f || !pPlayer )
		return 0.0f;

	if ( s_hBobOwner.Get() != pPlayer )
	{
		s_hBobOwner = pPlayer;
		s_flBobTime = 0.0f;
		s_nLastBobFrame = -1;
	}

	Vector vecPresentationVelocity = pPlayer->GetLocalVelocity();
	FoFStabilizeLocalPresentationVelocity(
		pPlayer, vecPresentationVelocity );
	const float flSpeed = clamp(
		vecPresentationVelocity.Length2D(), -320.0f, 320.0f );

	// Dual viewmodels can evaluate the same shared weapon bob more than once
	// per rendered frame.  Advance the presentation-only phase once per frame
	// so it cannot depend on render-pass count or predicted curtime rewinds.
	if ( s_nLastBobFrame != gpGlobals->framecount )
	{
		s_nLastBobFrame = gpGlobals->framecount;
		const float flFrameTime = clamp(
			gpGlobals->frametime, 0.0f, 0.1f );
		s_flBobTime += flFrameTime * flSpeed * ( 1.1f / 240.0f );
	}

	float flCycle = s_flBobTime -
		(int)( s_flBobTime / FOF_BOB_CYCLE_MAX ) * FOF_BOB_CYCLE_MAX;
	flCycle /= FOF_BOB_CYCLE_MAX;
	if ( flCycle < FOF_BOB_UP )
		flCycle = M_PI * flCycle / FOF_BOB_UP;
	else
		flCycle = M_PI + M_PI *
			( flCycle - FOF_BOB_UP ) / ( 1.0f - FOF_BOB_UP );

	g_verticalBob = flSpeed *
		( FoFWeaponID() == 3 ? 0.008f : 0.005f );
	g_verticalBob = g_verticalBob * 0.3f +
		g_verticalBob * 0.7f * sin( flCycle );
	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	flCycle = s_flBobTime -
		(int)( s_flBobTime / ( FOF_BOB_CYCLE_MAX * 2.0f ) ) *
		FOF_BOB_CYCLE_MAX * 2.0f;
	flCycle /= FOF_BOB_CYCLE_MAX * 2.0f;
	if ( flCycle < FOF_BOB_UP )
		flCycle = M_PI * flCycle / FOF_BOB_UP;
	else
		flCycle = M_PI + M_PI *
			( flCycle - FOF_BOB_UP ) / ( 1.0f - FOF_BOB_UP );

	g_lateralBob = flSpeed * 0.005f;
	g_lateralBob = g_lateralBob * 0.3f +
		g_lateralBob * 0.7f * sin( flCycle );
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );

	return 0.0f;
}

void CBaseHL2MPCombatWeapon::AddViewmodelBob(
	CBaseViewModel *pViewModel,
	Vector &origin,
	QAngle &angles )
{
	if ( !gpGlobals || gpGlobals->frametime == 0.0f )
		return;

	Vector forward, right, up;
	AngleVectors( angles, &forward, &right, &up );
	CalcViewmodelBob();

	VectorMA( origin, g_verticalBob * 0.7f, forward, origin );
	VectorMA( origin, g_verticalBob * 0.4f, up, origin );
	angles[ROLL] += g_verticalBob * 0.5f;
	angles[PITCH] -= g_verticalBob * 0.4f;
	angles[YAW] -= g_lateralBob * 0.6f;
	VectorMA( origin, g_lateralBob * 0.8f, right, origin );

	(void)pViewModel;
}

void FoFApplyViewModelTransform(
	CBaseViewModel *pViewModel,
	CBasePlayer *pOwner,
	CBaseCombatWeapon *pWeapon,
	const Vector &eyePosition,
	Vector &viewModelOrigin,
	QAngle &viewModelAngles )
{
	if ( pOwner && !prediction->InPrediction() &&
		pOwner->GetFoFKickTime() > gpGlobals->curtime )
	{
		const float flRemaining = clamp(
			pOwner->GetFoFKickTime() - gpGlobals->curtime,
			0.0f, 1.0f );
		const bool bMelee =
			pOwner->GetActiveWeapon() &&
			pOwner->GetActiveWeapon()->FoFWeaponID() == 0;

		Vector vecForward, vecRight, vecUp;
		pOwner->EyeVectors( &vecForward, &vecRight, &vecUp );
		const float flPitch = pOwner->EyeAngles()[PITCH];
		const float flRight = bMelee ? 0.0f : RemapValClamped(
			flPitch, 90.0f, 0.0f, 7.0f, 3.5f );
		const float flUp = bMelee ? 5.0f : 3.0f;
		const float flForward = bMelee ? -1.0f : RemapValClamped(
			flPitch, 90.0f, 0.0f, -16.0f, -12.0f );
		const Vector vecKickOffset =
			vecRight * flRight + vecUp * flUp + vecForward * flForward;

		const float flPhase = flRemaining > 0.5f ?
			RemapValClamped(
				flRemaining, 1.0f, 0.5f, 0.0f, 1.0f ) :
			RemapValClamped(
				flRemaining, 0.0f, 0.5f, 0.0f, 1.0f );
		const float flEase = 1.0f - powf(
			0.5f, 2.857143f * flRemaining );
		const Vector vecAppliedOffset =
			vecKickOffset * ( flPhase * flEase );
		viewModelOrigin += vecAppliedOffset;

	}

	if ( pOwner && pWeapon && !prediction->InPrediction() &&
		fof_weapon_lean.GetBool() )
	{
		const int nFoFWeaponID = pWeapon->FoFWeaponID();
		if ( nFoFWeaponID == 2 || nFoFWeaponID == 3 ||
			nFoFWeaponID == 4 )
		{
			const float flSight =
				clamp( FoFSightExpFactor( pOwner ), 0.0f, 1.0f );
			const float flMovementAndSight =
				FoFViewmodelMovementFactor( pOwner ) + flSight;
			const bool bLongGun = nFoFWeaponID == 3;
			const float flPitchSwing = RemapValClamped(
				flMovementAndSight, 0.0f, 1.0f, 0.0f,
				bLongGun ? 5.0f : 8.0f );
			const float flYawSwing = RemapValClamped(
				flMovementAndSight, 0.0f, 1.0f, 0.0f,
				bLongGun ? 1.0f : 4.0f );
			const float flHipBlend =
				1.0f - SimpleSpline( flSight );

			viewModelAngles[PITCH] += flPitchSwing * flHipBlend;
			viewModelAngles[YAW] += flYawSwing * flHipBlend;
		}
	}

	CBaseCombatWeapon *pSightWeapon = pWeapon;
	if ( !pSightWeapon && pOwner )
	{
		pSightWeapon = pViewModel->ViewModelIndex() == 1 ?
			pOwner->GetActiveWeapon2() : pOwner->GetActiveWeapon1();
	}

	const float flSight = FoFSightExpFactor( pOwner );
	if ( pSightWeapon && flSight >= 0.01f )
	{
		const CHL2MPSWeaponInfo &weaponInfo =
			static_cast< const CHL2MPSWeaponInfo & >(
				pSightWeapon->GetWpnData() );
		const bool bUseFinalOffset =
			pSightWeapon->FoFWeaponID() == 2 &&
			FoFHandStance( pOwner ) != 1;
		const Vector &vecOffset = bUseFinalOffset ?
			weaponInfo.m_vecFoFExpOffsetFinal :
			weaponInfo.m_vecFoFExpOffset;
		const QAngle &angOffset = bUseFinalOffset ?
			weaponInfo.m_angFoFExpOffsetFinal :
			weaponInfo.m_angFoFExpOffset;

		Vector vecTargetOrigin = viewModelOrigin;
		viewModelAngles += angOffset;

		Vector vecForward, vecRight, vecUp;
		AngleVectors( viewModelAngles, &vecForward, &vecRight, &vecUp );
		vecTargetOrigin += vecForward * vecOffset.x;
		vecTargetOrigin += vecRight * vecOffset.y;
		vecTargetOrigin += vecUp * vecOffset.z;

		VectorLerp(
			eyePosition,
			vecTargetOrigin,
			SimpleSpline( clamp( flSight, 0.0f, 1.0f ) ),
			viewModelOrigin );
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF viewmodel hand assignment and render validation.
//
//=============================================================================//

bool FoFShouldFlipViewModel( C_BaseViewModel *pViewModel )
{
	if ( !pViewModel )
		return false;

	// The viewmodel slot is the authoritative physical-hand fallback while
	// dual-wield weapon handles arrive in separate network updates.
	if ( pViewModel->ViewModelIndex() == 1 )
		return true;

	CBaseCombatWeapon *pWeapon = pViewModel->GetWeapon();
	return pWeapon &&
		( pWeapon->m_bFlipViewModel ||
			pWeapon->m_nViewModelIndex.Get() == 1 );
}

bool FoFShouldDrawViewModel( C_BaseViewModel *pViewModel )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return false;

	C_BasePlayer *pPresentationPlayer = pLocalPlayer;
	if ( engine->IsHLTV() )
	{
		if ( HLTVCamera()->GetMode() != OBS_MODE_IN_EYE )
			return false;
		pPresentationPlayer = ToBasePlayer(
			HLTVCamera()->GetPrimaryTarget() );
	}
	else if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		if ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
			return false;
		pPresentationPlayer = ToBasePlayer(
			pLocalPlayer->GetObserverTarget() );
	}
	else if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		return true;
	}
	else
	{
		// The shipped client draws the local first-person viewmodel from its
		// replicated model/effect state even when m_hWeapon has not yet survived
		// the next prediction restore.  Spawn equipment and respawn can leave that
		// handle null until the player explicitly selects a slot; requiring it here
		// made an otherwise valid model invisible.  Keep the stricter hand/weapon
		// ownership check below only for spectator and SourceTV presentation.
		const int nViewModelIndex = pViewModel->ViewModelIndex();
		if ( nViewModelIndex < 0 || nViewModelIndex >= MAX_VIEWMODELS ||
			pLocalPlayer->GetViewModel( nViewModelIndex, false ) != pViewModel )
		{
			return false;
		}
		return !pViewModel->IsEffectActive( EF_NODRAW );
	}

	if ( !pPresentationPlayer || pPresentationPlayer->IsObserver() )
		return false;

	const int nViewModelIndex = pViewModel->ViewModelIndex();
	C_BaseViewModel *pRegisteredViewModel =
		nViewModelIndex >= 0 && nViewModelIndex < MAX_VIEWMODELS ?
			pPresentationPlayer->GetViewModel(
				nViewModelIndex, false ) : NULL;
	if ( nViewModelIndex < 0 || nViewModelIndex >= MAX_VIEWMODELS ||
		pRegisteredViewModel != pViewModel )
	{
		return false;
	}

	// The active-weapon handles, the viewmodel's weapon handle and the
	// viewmodel itself are separate network properties.  Requiring those
	// handles or their effect fields to match in one client snapshot drops the
	// observed player's second (left-hand) viewmodel even though its registered
	// viewmodel is ready to draw.  SetWeaponVisible updates the corresponding
	// viewmodel as well as the weapon, so the viewmodel's own replicated render
	// state also preserves the hidden hand during a dual reload.
	return !pViewModel->IsEffectActive( EF_NODRAW );
}

void FoFRefreshSourceTVViewModelVisibility()
{
	if ( !engine->IsHLTV() )
		return;

	C_BasePlayer *pTarget = ToBasePlayer(
		HLTVCamera()->GetPrimaryTarget() );
	if ( !pTarget )
		return;

	// Viewmodel membership in the leaf-system render list is cached.  A
	// SourceTV target or camera-mode change does not itself update the target's
	// already-received viewmodels, so a valid second-hand model can remain
	// absent even after ShouldDraw starts returning true.
	for ( int i = 0; i < MAX_VIEWMODELS; ++i )
	{
		C_BaseViewModel *pViewModel =
			pTarget->GetViewModel( i, false );
		if ( pViewModel )
			pViewModel->UpdateVisibility();
	}
}
