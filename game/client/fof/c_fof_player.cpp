#include "cbase.h"
#include "fof/c_fof_player.h"
#include "cdll_client_int.h"
#include "ScreenSpaceEffects.h"
#include "clienteffectprecachesystem.h"
#include "fof/fof_combat_effects.h"
#include "hl2mp_gamerules.h"
#include "c_hl2mp_player.h"
#include "fof/fof_player_shared.h"
#include "c_gib.h"
#include "tier0/vprof.h"
#include "util_shared.h"
#include "view.h"
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool FoFHasSteamFriendOnCurrentTeam( C_FoF_Player *pLocalPlayer )
{
#ifndef NO_STEAM
	if ( !pLocalPlayer || !steamapicontext ||
		!steamapicontext->SteamFriends() )
	{
		return false;
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsPlayer() ||
			pPlayer->GetTeamNumber() != pLocalPlayer->GetTeamNumber() )
		{
			continue;
		}

		CSteamID steamID;
		if ( pPlayer->GetSteamID( &steamID ) &&
			steamapicontext->SteamFriends()->HasFriend(
				steamID, k_EFriendFlagImmediate ) )
		{
			return true;
		}
	}
#endif

	return false;
}

C_FoF_Player::C_FoF_Player() :
	m_ivFoFHorseAngles( "C_FoF_Player::m_ivFoFHorseAngles" )
	{
		m_nProgression = 0;
		m_horseAngles.Init();
		AddVar( &m_horseAngles, &m_ivFoFHorseAngles,
			LATCH_SIMULATION_VAR );
		m_nInBuyZone = 0;
		m_nPlayerAccuracy = 0;
		m_flNextAccuracyChange = 0.0f;
		m_flTargetCrosshairAperture = 0.0f;
		m_flTargetCrosshairAperture2 = 0.0f;
		m_nFoFPlayerFOV = 0;
		m_bSpawnInterpCounter = false;
		m_flDrunkness = 0.0f;
		m_flCaptureInput = 0.0f;
		m_nHandStance = 0;
		m_flTimeZoomed = 0.0f;
		m_flNextPickupInteraction = 0.0f;
		m_flWalkSpreadFactor = 0.0f;
		Q_memset( m_nPlHighlight, 0, sizeof( m_nPlHighlight ) );
		Q_memset( m_nPlTarget, 0, sizeof( m_nPlTarget ) );
		vecPropCarryAngles.Init();
		m_flJailTime = 0.0f;
		m_nPotionLevel = 0;
		m_flTransitionSpeed = 0.0f;
		m_nPlayerInfo = 0;
		m_bPickupActive = false;
		m_hAttachedObject = NULL;
		m_attachedPositionObjectSpace.Init();
		m_attachedAnglesPlayerSpace.Init();
		m_flFoFSpeedPenalty = 0.0f;
		m_flSightExpFactor = 0.0f;
		m_flWalkFactor = 0.0f;
		hPlayerAssisted = NULL;
		m_hKicker = NULL;
		m_nMultiKill = 0;
		m_flUnarmedTime = 0.0f;
		m_flFoFCash = 0.0f;
		m_nPlayerKills = 0;
		m_nLastRoundNotoriety = 0;
		m_flCrosshairAperture = 0.0f;
		m_flCrosshairAperture2 = 0.0f;
		m_bIsBotGhost = false;
		m_nFoFDynamiteBeltAttempts = 10;
		m_nFoFDynamiteBeltHits = 5;
		m_flFoFJumpReleaseTime = 0.0f;
		m_flFoFLastWallJumpTime = -FLT_MAX;
		m_bFoFRawAttack2 = false;
		m_bFoFRawReload = false;
		m_nFoFMoveTypeBeforeNetworkUpdate = MOVETYPE_NONE;
		m_nFoFObserverModeBeforeNetworkUpdate = OBS_MODE_NONE;
		m_bFoFServerFrozen = false;
		m_bFoFServerAtControls = false;
		m_bFoFMapIntermission = false;
		m_bFoFReloadPressConsumed = false;
		m_bFoFInitialMessageHandled = false;
		Q_memset( m_iFoFPersonalStats, 0, sizeof( m_iFoFPersonalStats ) );
		m_flFoFRoundPlayTime = 0.0f;
		m_flFoFSessionPlayTime = 0.0f;
		m_bFoFSteamPlayTimeLoaded = false;
		m_flFoFNextStatAccuracyReport = 0.0f;
		m_flFoFImpactHintTime = 0.0f;
		m_flFoFLastHintTime = 0.0f;
		m_flFoFHintDelayScale = 0.0f;
		m_bFoFScreenshotHandled = false;
		m_hFoFCarryGib = NULL;
		m_hFoFCarrySource = NULL;
		m_flFoFCarryStateInvalidTime = -1.0f;
		m_bFoFCarryCancelPending = false;
		m_hFoFCarryCanceledObject = NULL;
		m_flFoFCarryCanceledInteraction = 0.0f;
		m_nFoFLastObserverMode = OBS_MODE_NONE;
		m_bFoFLastAlive = false;
		m_bFoFRestoreViewModelVisibility = false;
		m_flFoFReconcileViewModelsUntil = 0.0f;
		m_hFoFLastActiveWeapon1 = NULL;
		m_hFoFLastActiveWeapon2 = NULL;
		m_pFoFLowHealthBlood = NULL;
		m_iFoFLowHealthBloodAttachment = 0;
		for ( int i = 0; i < ARRAYSIZE( m_pFoFFootsteps ); ++i )
		{
			m_pFoFFootsteps[i] = NULL;
			m_flFoFFootstepEnd[i] = 0.0f;
		}
		if ( g_pScreenSpaceEffects )
			g_pScreenSpaceEffects->EnableScreenSpaceEffect( "fof_drunk" );
	}

C_FoF_Player::~C_FoF_Player()
	{
		StopFoFLowHealthBlood();
	}

void C_FoF_Player::Spawn()
{
	// The original client starts with C_BasePlayer::Spawn, then warms the
	// presentation assets used directly by the FoF player and bullet paths.
	BaseClass::Spawn();
	ResetFoFSharedSpawnState();

	static const char *s_pParticleSystems[] =
	{
		"muzzle_fof_revolver",
		"muzzle_fof_shotgun",
		"muzzle_fof_revolver_w",
		"muzzle_fof_shotgun_w",
		"muzzle_smoke2",
		"dynamite_yellow",
		"dynamite_base",
		"dynamite_black",
		"chest_blood",
		"footsteps",
		"heal_safezone",
		"horse_steps_dust",
		"horse_steps_dust_hq",
		"fof_selfexplosion",
		"fof_xbow_explosion",
		"fof_boost",
		"fof_boost_world",
		"plate_sparks",
		"plate_sparks_pellet",
		"bigboom_blood",
		"impact_concrete",
		"blood_impact_red_01_droplets_headshot"
	};
	for ( int i = 0; i < ARRAYSIZE( s_pParticleSystems ); ++i )
		PrecacheParticleSystem( s_pParticleSystems[i] );

	PrecacheMaterial( "shaders/Radial_Blur" );
	for ( int i = 1; i <= 16; ++i )
	{
		char materialName[64];
		Q_snprintf( materialName, sizeof( materialName ),
			"particle/smokesprites_%04d", i );
		PrecacheMaterial( materialName );
	}
	PrecacheMaterial( "vgui/cap_empty" );
	PrecacheMaterial( "vgui/loot_target" );
	PrecacheMaterial( "vgui/ask_weapon" );
	PrecacheMaterial( "effects/footsteps" );

	if ( GetFoFPlayerAnimState() )
		GetFoFPlayerAnimState()->ClearAnimationState();
	m_flFoFJumpReleaseTime = 0.0f;
	m_bFoFRawAttack2 = false;
	m_bFoFRawReload = false;
	m_bFoFReloadPressConsumed = false;
	m_flFoFImpactHintTime = 0.0f;
	m_flFoFLastHintTime = 0.0f;
	m_flFoFHintDelayScale = 0.0f;
	ClearFoFCarryPresentation();
	StopFoFLowHealthBlood();
	m_bFoFCarryCancelPending = false;
	m_hFoFCarryCanceledObject = NULL;
	m_flFoFCarryCanceledInteraction = 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_pFoFFootsteps ); ++i )
	{
		if ( m_pFoFFootsteps[i].GetObject() )
			m_pFoFFootsteps[i]->StopEmission( false, false, false );
		m_pFoFFootsteps[i] = NULL;
		m_flFoFFootstepEnd[i] = 0.0f;
	}
	if ( GetFoFPlayerAnimState() )
		GetFoFPlayerAnimState()->ResetGestureSlots();
	if ( this == C_BasePlayer::GetLocalPlayer() )
	{
		FoFUpdateLocalSleeveFrame();
		engine->ClientCmd( FoFHasSteamFriendOnCurrentTeam( this ) ?
			"fcp 77" : "fcn 25" );
	}
}

bool C_FoF_Player::InSameTeam( C_BaseEntity *pEntity )
{
	// FoF deliberately requires both matching team
	// numbers and an active team-play ruleset.  In shootout, two unassigned
	// players therefore never become accidental team-mates.
	return pEntity && pEntity->GetTeamNumber() == GetTeamNumber() &&
		HL2MPRules() && HL2MPRules()->IsTeamplay();
}

BEGIN_RECV_TABLE_NOBASE( C_FoF_Player, DT_FoFLocalPlayerExclusive008 )
	RecvPropInt( RECVINFO( m_nInBuyZone ) ),
	RecvPropInt( RECVINFO( m_nPlayerAccuracy ) ),
	RecvPropFloat( RECVINFO( m_flNextAccuracyChange ) ),
	RecvPropFloat( RECVINFO( m_flTargetCrosshairAperture ) ),
	RecvPropFloat( RECVINFO( m_flTargetCrosshairAperture2 ) ),
	RecvPropInt( RECVINFO( m_nFoFPlayerFOV ) ),
	RecvPropBool( RECVINFO( m_bSpawnInterpCounter ) ),
	RecvPropFloat( RECVINFO( m_flDrunkness ) ),
	RecvPropFloat( RECVINFO( m_flCaptureInput ) ),
	RecvPropInt( RECVINFO( m_nHandStance ) ),
	RecvPropFloat( RECVINFO( m_flTimeZoomed ) ),
	RecvPropFloat( RECVINFO( m_flNextPickupInteraction ) ),
	RecvPropFloat( RECVINFO( m_flWalkSpreadFactor ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_nPlHighlight ), RecvPropInt( RECVINFO( m_nPlHighlight[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_nPlTarget ), RecvPropInt( RECVINFO( m_nPlTarget[0] ) ) ),
	RecvPropFloat( RECVINFO( vecPropCarryAngles[0] ) ),
	RecvPropFloat( RECVINFO( vecPropCarryAngles[1] ) ),
	RecvPropFloat( RECVINFO( vecPropCarryAngles[2] ) ),
	RecvPropFloat( RECVINFO( m_flJailTime ) ),
	RecvPropInt( RECVINFO( m_nPotionLevel ) ),
END_RECV_TABLE()

// Keep the server wire name literal.  The shared player source maps
// CFoF_Player to C_FoF_Player on the client, and passing that token through
// IMPLEMENT_CLIENTCLASS_DT would expand it before the stock registration
// macro stringizes the network name.
IMPLEMENT_CLIENTCLASS( C_FoF_Player, DT_CFoF_Player_608, CFoF_Player )
BEGIN_RECV_TABLE( C_FoF_Player, DT_CFoF_Player_608 )
	RecvPropInt( RECVINFO( m_nProgression ) ),
	RecvPropFloat( RECVINFO( m_horseAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_horseAngles[1] ) ),
	RecvPropFloat( RECVINFO( m_horseAngles[2] ) ),
	RecvPropDataTable( "foflocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_FoFLocalPlayerExclusive008 ) ),
	RecvPropFloat( RECVINFO( m_flTransitionSpeed ) ),
	RecvPropInt( RECVINFO( m_nPlayerInfo ) ),
	RecvPropBool( RECVINFO( m_bPickupActive ) ),
	RecvPropEHandle( RECVINFO( m_hAttachedObject ) ),
	RecvPropVector( RECVINFO( m_attachedPositionObjectSpace ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[0] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[1] ) ),
	RecvPropFloat( RECVINFO( m_attachedAnglesPlayerSpace[2] ) ),
	RecvPropFloat( RECVINFO( m_flFoFSpeedPenalty ) ),
	RecvPropFloat( RECVINFO( m_flSightExpFactor ) ),
	RecvPropFloat( RECVINFO( m_flWalkFactor ) ),
	RecvPropEHandle( RECVINFO( hPlayerAssisted ) ),
	RecvPropEHandle( RECVINFO( m_hKicker ) ),
	RecvPropInt( RECVINFO( m_nMultiKill ) ),
	RecvPropFloat( RECVINFO( m_flUnarmedTime ) ),
	RecvPropFloat( RECVINFO( m_flFoFCash ) ),
	RecvPropInt( RECVINFO( m_nPlayerKills ) ),
	RecvPropInt( RECVINFO( m_nLastRoundNotoriety ) ),
	RecvPropFloat( RECVINFO( m_flCrosshairAperture ) ),
	RecvPropFloat( RECVINFO( m_flCrosshairAperture2 ) ),
	RecvPropBool( RECVINFO( m_bIsBotGhost ) ),
END_RECV_TABLE()

// These fields are updated by ItemPostFrame/CreateMove, so they
// must be restored to the acknowledged command before replaying outstanding
// commands.  Without this map, higher latency applies sight/accuracy changes
// repeatedly every render frame and a later server snapshot visibly pulls
// them back.
BEGIN_PREDICTION_DATA( C_FoF_Player )
	DEFINE_PRED_FIELD( m_nPlayerInfo, FIELD_INTEGER,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerAccuracy, FIELD_INTEGER,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flCrosshairAperture, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flTargetCrosshairAperture, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flCrosshairAperture2, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flTargetCrosshairAperture2, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flNextAccuracyChange, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD( m_flFoFSpeedPenalty, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flTransitionSpeed, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flSightExpFactor, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.001f ),
	DEFINE_PRED_FIELD_TOL( m_flWalkFactor, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.01f ),
	DEFINE_PRED_FIELD_TOL( m_flWalkSpreadFactor, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE, 0.01f ),
	DEFINE_PRED_FIELD( m_flTimeZoomed, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDrunkness, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCaptureInput, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	// The shipped client registers m_flCaptureInput twice. Preserve that
	// seemingly redundant entry for exact datamap parity.
	DEFINE_PRED_FIELD( m_flCaptureInput, FIELD_FLOAT,
		FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_horseAngles, FIELD_VECTOR,
		FTYPEDESC_INSENDTABLE, 1.0f ),
	DEFINE_PRED_FIELD_TOL( m_attachedAnglesPlayerSpace, FIELD_VECTOR,
		FTYPEDESC_INSENDTABLE, 1.0f ),
	DEFINE_PRED_FIELD( m_bPickupActive, FIELD_BOOLEAN,
		FTYPEDESC_INSENDTABLE ),
	// Local yellow-dynamite sample state must rewind with the command that
	// starts a pullback even though it has no network field.
	DEFINE_PRED_FIELD( m_nFoFDynamiteBeltAttempts, FIELD_INTEGER,
		FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nFoFDynamiteBeltHits, FIELD_INTEGER,
		FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF client player collision filtering.
//
//=============================================================================//

void C_FoF_Player::ClearFoFCarryPresentation()
{
	C_BaseEntity *pCarrySource = m_hFoFCarrySource.Get();
	if ( !pCarrySource )
		pCarrySource = m_hAttachedObject.Get();
	if ( pCarrySource )
	{
		pCarrySource->RemoveEffects( EF_NODRAW );
		pCarrySource->UpdateVisibility();
	}
	if ( m_hFoFCarryGib.Get() )
		m_hFoFCarryGib->Release();
	m_hFoFCarryGib = NULL;
	m_hFoFCarrySource = NULL;
	m_flFoFCarryStateInvalidTime = -1.0f;
}

void C_FoF_Player::UpdateFoFCarryPresentation()
{
	VPROF_BUDGET(
		"FoF::Player::CarryPresentation",
		VPROF_BUDGETGROUP_CLIENT_SIM );

	C_BaseEntity *pAttached = m_hAttachedObject.Get();
	C_Gib *pCarryGib =
		dynamic_cast< C_Gib * >( m_hFoFCarryGib.Get() );
	if ( m_bFoFCarryCancelPending )
	{
		const bool bCarryGenerationAdvanced =
			!m_bPickupActive ||
			m_hAttachedObject != m_hFoFCarryCanceledObject ||
			m_flNextPickupInteraction != m_flFoFCarryCanceledInteraction;
		if ( !bCarryGenerationAdvanced )
		{
			// A reliable cancellation can arrive before (or without) the
			// snapshot that clears the two received carry fields. Recreating
			// the gib here would undo the cancellation one render frame later.
			if ( pCarryGib || m_hFoFCarrySource.Get() )
				ClearFoFCarryPresentation();
			return;
		}

		m_bFoFCarryCancelPending = false;
		m_hFoFCarryCanceledObject = NULL;
		m_flFoFCarryCanceledInteraction = 0.0f;
	}

	if ( !m_bPickupActive || !pAttached )
	{
		if ( pCarryGib || m_hFoFCarrySource.Get() )
		{
			// The local-player bool and EHANDLE can arrive in separate packet
			// updates. Do not destroy the presentation on that transient
			// mismatch, and never write back into either RecvTable field here.
			if ( m_flFoFCarryStateInvalidTime < 0.0f )
				m_flFoFCarryStateInvalidTime = gpGlobals->curtime;
			else if ( gpGlobals->curtime - m_flFoFCarryStateInvalidTime >= 0.20f )
				ClearFoFCarryPresentation();
		}
		else
		{
			m_flFoFCarryStateInvalidTime = -1.0f;
		}
		return;
	}
	m_flFoFCarryStateInvalidTime = -1.0f;

	C_BaseEntity *pCarrySource = m_hFoFCarrySource.Get();
	if ( pCarryGib && pCarrySource != pAttached )
	{
		if ( pCarrySource )
		{
			pCarrySource->RemoveEffects( EF_NODRAW );
			pCarrySource->UpdateVisibility();
		}
		pCarryGib->Release();
		pCarryGib = NULL;
		m_hFoFCarryGib = NULL;
		m_hFoFCarrySource = NULL;
		m_flFoFCarryStateInvalidTime = -1.0f;
	}

	if ( !pCarryGib )
	{
		const model_t *pModel = pAttached->GetModel();
		const char *pszModelName =
			pModel ? modelinfo->GetModelName( pModel ) : NULL;
		if ( !pszModelName || !pszModelName[0] )
			return;

		// Seed the presentation copy from the authoritative entity's
		// interpolated transform so picking an object up remains continuous.
		const Vector vecCarryStart = pAttached->GetRenderOrigin();
		const QAngle angCarryStart = pAttached->GetRenderAngles();
		pCarryGib = C_Gib::CreateClientsideGib(
			pszModelName,
			vecCarryStart,
			vec3_origin,
			vec3_origin,
			600.0f );
		if ( !pCarryGib )
			return;

		pCarryGib->SetAbsAngles( angCarryStart );
		pCarryGib->SetCollisionGroup( COLLISION_GROUP_NONE );
		C_BaseAnimating *pAttachedAnimating =
			dynamic_cast< C_BaseAnimating * >( pAttached );
		if ( pAttachedAnimating )
			pCarryGib->m_nSkin = pAttachedAnimating->GetSkin();
		pCarryGib->SetNextClientThink( CLIENT_THINK_NEVER );
		pCarryGib->AddEffects( EF_NOINTERP | EF_NOSHADOW );
		IPhysicsObject *pCarryPhysics = pCarryGib->VPhysicsGetObject();
		if ( pCarryPhysics )
		{
			pCarryPhysics->EnableGravity( false );
			pCarryPhysics->EnableCollisions( false );
			pCarryPhysics->EnableMotion( false );
			pCarryPhysics->SetPosition(
				vecCarryStart, angCarryStart, true );
		}
		m_hFoFCarryGib = pCarryGib;
		m_hFoFCarrySource = pAttached;

		pAttached->AddEffects( EF_NODRAW );
		pAttached->UpdateVisibility();
	}
	else if ( !pAttached->IsEffectActive( EF_NODRAW ) )
	{
		pAttached->AddEffects( EF_NODRAW );
		pAttached->UpdateVisibility();
	}

	const QAngle viewAngles = MainViewAngles();
	Vector vecForward;
	Vector vecRight;
	Vector vecUp;
	AngleVectors( viewAngles, &vecForward, &vecRight, &vecUp );
	const Vector vecTarget =
		EyePosition() + vecForward * 40.0f + vecUp * -25.0f;
	const Vector vecCurrent = pCarryGib->GetAbsOrigin();
	const Vector vecSmoothed =
		vecCurrent +
		( vecTarget - vecCurrent ) * ( gpGlobals->frametime * 10.0f );

	QAngle smoothedAngles;
	InterpolateAngles(
		pCarryGib->GetAbsAngles(),
		viewAngles,
		smoothedAngles,
		gpGlobals->frametime * 15.0f );
	pCarryGib->SetAbsAngles( smoothedAngles );

	Vector vecMins = pAttached->CollisionProp()->OBBMins() * 0.5f;
	Vector vecMaxs = pAttached->CollisionProp()->OBBMaxs() * 0.5f;
	trace_t trace;
	CTraceFilterSkipTwoEntities traceFilter(
		this, pAttached, COLLISION_GROUP_NONE );
	UTIL_TraceHull(
		vecCurrent,
		vecSmoothed,
		vecMins,
		vecMaxs,
		MASK_SOLID,
		&traceFilter,
		&trace );
	if ( trace.fraction >= 1.0f && !trace.startsolid )
		pCarryGib->SetAbsOrigin( vecSmoothed );
	else
		pCarryGib->SetAbsOrigin( trace.endpos );

	IPhysicsObject *pCarryPhysics = pCarryGib->VPhysicsGetObject();
	if ( pCarryPhysics )
	{
		pCarryPhysics->SetPosition(
			pCarryGib->GetAbsOrigin(),
			pCarryGib->GetAbsAngles(),
			true );
	}
}

void C_FoF_Player::SetObserverTarget( EHANDLE hObserverTarget )
{
	C_BasePlayer::SetObserverTarget( hObserverTarget );

	if ( this != C_BasePlayer::GetLocalPlayer() )
		return;

	C_BasePlayer *pTarget = ToBasePlayer( hObserverTarget.Get() );
	C_FoF_Player *pFoFTarget = dynamic_cast< C_FoF_Player * >( pTarget );
	if ( !pFoFTarget || !pFoFTarget->IsAlive() )
		return;

	FoFSetLocalSleeveFrame(
		FoFResolvePlayerTeamSleeveFrame( pFoFTarget ) );
}

bool C_FoF_Player::Weapon_Switch(
	C_BaseCombatWeapon *pWeapon, int viewmodelindex )
{
	// FoF C_FoF_Player::Weapon_Switch refuses every route into a
	// different weapon while the potion loadout is forced.  Keeping this at
	// the virtual player boundary also covers programmatic switches that do
	// not pass through SelectItem.
	if ( IsFoFPotionWeaponLockActive() )
		return false;

	// Versus uses FL_ATCONTROLS while a player is held at the round start.
	// FoF weapons cannot be switched during that state; fists (weapon id 0)
	// remain available to the course/round setup code.
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( pWeapon && pWeapon->FoFWeaponID() != 0 &&
		currentMode.IsValid() && currentMode.GetInt() == 5 &&
		( GetFlags() & FL_ATCONTROLS ) )
	{
		return false;
	}

	// The shipped client cancels the previous attack/reload gesture only once
	// the next-attack gate has strictly expired.  Equality is intentionally
	// excluded to retain its tick-boundary behavior.
	if ( GetNextAttack() < gpGlobals->curtime )
		DoAnimationEvent( PLAYERANIMEVENT_CANCEL, 0 );

	return BaseClass::Weapon_Switch( pWeapon, viewmodelindex );
}
