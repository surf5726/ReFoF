//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
//=============================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_hl2mp_player.h"
#include "view.h"
#include "takedamageinfo.h"
#include "hl2mp_gamerules.h"
#include "in_buttons.h"
#include "fof/c_fof_player.h"
#include "fof/fof_client_settings.h"
#include "fof/fof_player_shared.h"
#include "hl2mp/hl2mp_playeranimstate.h"
#include "dlight.h"
#include "flashlighteffect.h"
#include "iviewrender_beams.h"
#include "physics_shared.h"
#include "r_efx.h"
#include "ragdoll.h"
#include "vphysics/constraints.h"

#include "tier0/memdbgon.h"

// Don't alias here
#if defined( CHL2MP_Player )
#undef CHL2MP_Player	
#endif

LINK_ENTITY_TO_CLASS( player, C_HL2MP_Player );

static void RecvProxy_CycleLatch(
	const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	(void)pOut;
	C_HL2MP_Player *pPlayer = static_cast< C_HL2MP_Player * >( pStruct );
	const float flServerCycle =
		static_cast< float >( pData->m_Value.m_Int ) * ( 1.0f / 16.0f );
	if ( fabsf( pPlayer->GetCycle() - flServerCycle ) > 0.15f )
		pPlayer->SetServerIntendedCycle( flServerCycle );
}

// FoF retains the older HL2MP split local/non-local player tables.  Besides
// matching the server ABI, keeping the origin in these proxy tables preserves
// the different local prediction and remote interpolation encodings.
BEGIN_RECV_TABLE_NOBASE( C_HL2MP_Player, DT_HL2MPLocalPlayerExclusive )
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( C_HL2MP_Player, DT_HL2MPNonLocalPlayerExclusive )
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropInt( RECVINFO( m_cycleLatch ), 0, RecvProxy_CycleLatch ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_HL2MP_Player, DT_HL2MP_Player, CHL2MP_Player)
	RecvPropDataTable( "hl2mplocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_HL2MPLocalPlayerExclusive ) ),
	RecvPropDataTable( "hl2mpnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_HL2MPNonLocalPlayerExclusive ) ),

	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO_NAME( m_iSpawnInterpCounter, m_bSpawnInterpCounter ) ),
	RecvPropInt( RECVINFO( m_iPlayerSoundType) ),

	RecvPropBool( RECVINFO( m_fIsWalking ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_HL2MP_Player )
	// FoF's original C_HL2MP_Player prediction map overrides the animation
	// fields inherited from C_BaseAnimating.  Keeping these private/no-error
	// entries prevents an acknowledged server snapshot from rewinding the
	// locally replayed player animation while commands are still in flight.
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT,
		FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE |
		FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_fIsWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER,
		FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE |
		FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT,
		FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE |
		FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_ARRAY_TOL( m_flEncodedController, FIELD_FLOAT,
		MAXSTUDIOBONECTRLS,
		FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE, 0.02f ),
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER,
		FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE |
		FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

#define	HL2_WALK_SPEED 150
#define	HL2_NORM_SPEED 190
#define	HL2_SPRINT_SPEED 320

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");
static ConVar cl_defaultweapon( "cl_defaultweapon", "weapon_physcannon", FCVAR_USERINFO | FCVAR_ARCHIVE, "Default Spawn Weapon");

class C_FoFFlashlightEffect : public CFlashlightEffect
{
public:
	explicit C_FoFFlashlightEffect( int nEntIndex ) :
		CFlashlightEffect( nEntIndex )
	{
	}
};

static void CompleteThirdPersonPlayerBody( C_BaseAnimating *pAnimating )
{
	if ( !pAnimating || pAnimating->GetNumBodyGroups() <= 0 ||
		pAnimating->GetBodygroupCount( 0 ) <= 1 )
	{
		return;
	}

	pAnimating->SetBodygroup( 0, 1 );
}

IPhysicsObject *GetWorldPhysObject( void );

void C_HL2MP_Player::UpdateFlashlightBeam()
{
	if ( !IsEffectActive( EF_DIMLIGHT ) )
	{
		ReleaseFlashlight();
		return;
	}

	const bool bLocalPlayer = this == C_BasePlayer::GetLocalPlayer();
	if ( bLocalPlayer && !C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		ReleaseFlashlight();
		return;
	}

	const int iAttachment = LookupAttachment( "anim_attachment_RH" );
	if ( iAttachment < 0 )
		return;

	Vector vecOrigin;
	QAngle attachmentAngles = GetRenderAngles();
	GetAttachment( iAttachment, vecOrigin, attachmentAngles );

	Vector vForward;
	AngleVectors( attachmentAngles, &vForward );

	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + vForward * 200.0f,
		MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	if ( !m_pFlashlightBeam )
	{
		BeamInfo_t beamInfo;
		beamInfo.m_nType = TE_BEAMPOINTS;
		beamInfo.m_vecStart = tr.startpos;
		beamInfo.m_vecEnd = tr.endpos;
		beamInfo.m_pszModelName = "sprites/glow01.vmt";
		beamInfo.m_pszHaloName = "sprites/glow01.vmt";
		beamInfo.m_flHaloScale = 3.0f;
		beamInfo.m_flWidth = 8.0f;
		beamInfo.m_flEndWidth = 35.0f;
		beamInfo.m_flFadeLength = 300.0f;
		beamInfo.m_flAmplitude = 0.0f;
		beamInfo.m_flBrightness = 60.0f;
		beamInfo.m_flSpeed = 0.0f;
		beamInfo.m_nStartFrame = 0;
		beamInfo.m_flFrameRate = 0.0f;
		beamInfo.m_flRed = 255.0f;
		beamInfo.m_flGreen = 255.0f;
		beamInfo.m_flBlue = 255.0f;
		beamInfo.m_nSegments = 8;
		beamInfo.m_bRenderable = true;
		beamInfo.m_flLife = 0.5f;
		beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE |
			FBEAM_NOTILE | FBEAM_HALOBEAM;
		m_pFlashlightBeam = beams->CreateBeamPoints( beamInfo );
	}

	if ( !m_pFlashlightBeam )
		return;

	BeamInfo_t beamInfo;
	beamInfo.m_vecStart = tr.startpos;
	beamInfo.m_vecEnd = tr.endpos;
	beamInfo.m_flRed = 255.0f;
	beamInfo.m_flGreen = 255.0f;
	beamInfo.m_flBlue = 255.0f;
	beams->UpdateBeamInfo( m_pFlashlightBeam, beamInfo );

	if ( bLocalPlayer )
		return;

	dlight_t *pLight = effects->CL_AllocDlight( 0 );
	pLight->origin = tr.endpos;
	pLight->radius = 50.0f;
	pLight->color.r = 200;
	pLight->color.g = 200;
	pLight->color.b = 200;
	pLight->die = gpGlobals->curtime + 0.1f;
}

void C_HL2MP_Player::ReleaseFlashlight()
{
	if ( !m_pFlashlightBeam )
		return;

	m_pFlashlightBeam->flags = 0;
	m_pFlashlightBeam->die = gpGlobals->curtime - 1.0f;
	m_pFlashlightBeam = NULL;
}

void C_HL2MP_Player::ReleasePlayerFlashlight()
{
	delete m_pPlayerFlashlight;
	m_pPlayerFlashlight = NULL;
}

void C_HL2MP_Player::UpdatePlayerFlashlight()
{
	if ( !IsEffectActive( EF_DIMLIGHT ) )
	{
		ReleasePlayerFlashlight();
		return;
	}

	if ( !m_pPlayerFlashlight )
	{
		m_pPlayerFlashlight = new C_FoFFlashlightEffect( entindex() );
		if ( !m_pPlayerFlashlight )
			return;

		m_pPlayerFlashlight->TurnOn();
	}

	Vector lightOrigin = EyePosition();
	Vector forward, right, up;
	if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		const int iAttachment = LookupAttachment( "anim_attachment_RH" );
		if ( iAttachment >= 0 )
		{
			QAngle attachmentAngles = GetRenderAngles();
			GetAttachment( iAttachment, lightOrigin, attachmentAngles );
			AngleVectors( attachmentAngles, &forward, &right, &up );
		}
		else
		{
			EyeVectors( &forward, &right, &up );
		}
	}
	else
	{
		EyeVectors( &forward, &right, &up );
	}

	m_pPlayerFlashlight->UpdateLight(
		lightOrigin, forward, right, up, 1000 );
}

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

ShadowType_t C_HL2MP_Player::ShadowCastType( void )
{
	return SHADOWS_NONE;
}

bool C_HL2MP_Player::ShouldDraw( void )
{
	if ( !IsAlive() )
		return false;

	if ( IsObserver() || GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if ( IsLocalPlayer() && IsRagdoll() )
		return true;

	if ( IsRagdoll() )
		return false;

	return BaseClass::ShouldDraw();
}

void C_HL2MP_Player::CalcView(
	Vector &eyeOrigin,
	QAngle &eyeAngles,
	float &zNear,
	float &zFar,
	float &fov )
{
	const int nObserverMode = GetObserverMode();
	if ( m_lifeState != LIFE_ALIVE &&
		( !IsObserver() || nObserverMode == OBS_MODE_NONE ||
			nObserverMode == OBS_MODE_DEATHCAM ) )
	{
		Vector origin = EyePosition();
		IRagdoll *pRagdoll = GetRepresentativeRagdoll();
		if ( pRagdoll )
		{
			origin = pRagdoll->GetRagdollOrigin();
			origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z;
		}

		BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );
		eyeOrigin = origin;

		Vector forward;
		AngleVectors( eyeAngles, &forward );
		VectorNormalize( forward );
		VectorMA( origin, -CHASE_CAM_DISTANCE_MAX, forward, eyeOrigin );

		const Vector wallMin( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
		const Vector wallMax( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );
		trace_t trace;
		C_BaseEntity::PushEnableAbsRecomputations( false );
		UTIL_TraceHull(
			origin, eyeOrigin, wallMin, wallMax,
			MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();
		if ( trace.fraction < 1.0f )
			eyeOrigin = trace.endpos;
		return;
	}

	BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );
}

C_BaseAnimating *C_HL2MP_Player::BecomeRagdollOnClient()
{
	if ( !IsPlayer() || !( FoFPlayerInfo( this ) & 0x400000 ) )
		return NULL;

	C_BaseAnimating *pRagdoll = C_BaseAnimating::BecomeRagdollOnClient();
	CompleteThirdPersonPlayerBody( pRagdoll );
	return pRagdoll;
}

C_HL2MP_Player::C_HL2MP_Player() : m_iv_angEyeAngles( "C_HL2MP_Player::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreateFoFPlayerAnimState( this );
	m_iIDEntIndex = 0;
	m_iSpawnInterpCounterCache = 0;
	m_cycleLatch = 0;
	m_flServerIntendedCycle = -1.0f;

	m_angEyeAngles.Init();

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	m_blinkTimer.Invalidate();

	m_pFlashlightBeam = NULL;
	m_pPlayerFlashlight = NULL;
}

C_HL2MP_Player::~C_HL2MP_Player( void )
{
	if ( m_PlayerAnimState )
		m_PlayerAnimState->Release();
	m_PlayerAnimState = NULL;
	ReleaseFlashlight();
	ReleasePlayerFlashlight();
}

int C_HL2MP_Player::GetIDTarget() const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_HL2MP_Player::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		 GetObserverMode() == OBS_MODE_DEATHCAM )
		 return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 1500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

void C_HL2MP_Player::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;
	
	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();
		
		CBaseEntity *pAttacker = info.GetAttacker();

		if ( pAttacker )
		{
			if ( HL2MPRules()->IsTeamplay() && pAttacker->InSameTeam( this ) == true )
				return;
		}

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}


C_HL2MP_Player* C_HL2MP_Player::GetLocalHL2MPPlayer()
{
	return (C_HL2MP_Player*)C_BasePlayer::GetLocalPlayer();
}

void C_HL2MP_Player::Initialize( void )
{
	m_headYawPoseParam = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	CStudioHdr *hdr = GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}
}

CStudioHdr *C_HL2MP_Player::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();
	
	Initialize( );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
 * Orient head and eyes towards m_lookAt.
 */
void C_HL2MP_Player::UpdateLookAt( void )
{
	// head yaw
	if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	// orient eyes
	m_viewtarget = m_vLookAtTarget;

	// blinking
	if (m_blinkTimer.IsElapsed())
	{
		m_blinktoggle = !m_blinktoggle;
		m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = m_vLookAtTarget - EyePosition();
	VectorAngles( to, desiredAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetLocalAngles()[YAW];


	float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
	m_flLastBodyYaw = bodyAngles[YAW];
	

	// Set the head's yaw.
	float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, 130 * gpGlobals->frametime );

	// Counterrotate the head from the body rotation so it doesn't rotate past its target.
	m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	
	SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );

	
	// Set the head's yaw.
	desired = AngleNormalize( desiredAngles[PITCH] );
	desired = clamp( desired, m_headPitchMin, m_headPitchMax );
	
	m_flCurrentHeadPitch = ApproachAngle( desired, m_flCurrentHeadPitch, 130 * gpGlobals->frametime );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );
}

void C_HL2MP_Player::ClientThink( void )
{
	bool bFoundViewTarget = false;
	
	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );

	for( int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( iClient );
		if(!pEnt || !pEnt->IsPlayer())
			continue;

		if ( pEnt->entindex() == entindex() )
			continue;

		Vector vTargetOrigin = pEnt->GetAbsOrigin();
		Vector vMyOrigin =  GetAbsOrigin();

		Vector vDir = vTargetOrigin - vMyOrigin;
		
		if ( vDir.Length() > 128 ) 
			continue;

		VectorNormalize( vDir );

		if ( DotProduct( vForward, vDir ) < 0.0f )
			 continue;

		m_vLookAtTarget = pEnt->EyePosition();
		bFoundViewTarget = true;
		break;
	}

	if ( bFoundViewTarget == false )
	{
		m_vLookAtTarget = GetAbsOrigin() + vForward * 512;
	}

	UpdateIDTarget();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_HL2MP_Player::DrawModel( int flags )
{
	if ( !m_bReadyToDraw )
		return 0;

    return BaseClass::DrawModel(flags);
}

//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		 return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_HL2MP_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_HL2MP_Player::PreThink( void )
{
	QAngle vTempAngles = GetLocalAngles();

	if ( GetLocalPlayer() == this )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();

	HandleSpeedChanges();

	if ( m_HL2Local.m_flSuitPower <= 0.0f )
	{
		if( IsSprinting() )
		{
			StopSprinting();
		}
	}
}

const QAngle &C_HL2MP_Player::EyeAngles()
{
	if( IsLocalPlayer() )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_HL2MP_Player::AddEntity( void )
{
	BaseClass::AddEntity();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );
		
	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	UpdateFlashlightBeam();
	UpdatePlayerFlashlight();
}

void C_HL2MP_Player::UpdateClientSideAnimation( void )
{
	if ( m_PlayerAnimState )
	{
		const QAngle &eyeAngles = EyeAngles();
		m_PlayerAnimState->Update(
			eyeAngles[YAW], eyeAngles[PITCH] );
	}

	BaseClass::UpdateClientSideAnimation();
}

void C_HL2MP_Player::SetServerIntendedCycle( float intended )
{
	m_flServerIntendedCycle = intended;
}

float C_HL2MP_Player::GetServerIntendedCycle( void )
{
	return m_flServerIntendedCycle;
}

bool C_HL2MP_Player::ShouldCollide(
	int nCollisionGroup, int nContentsMask ) const
{
	const FoFPlayerCollisionDecision_t nDecision =
		FoFResolvePlayerTeamCollision(
			GetTeamNumber(), nCollisionGroup, nContentsMask );
	if ( nDecision == FOF_PLAYER_COLLISION_ACCEPT )
		return true;
	if ( nDecision == FOF_PLAYER_COLLISION_REJECT )
		return false;

	if ( GetObserverMode() == OBS_MODE_DEATHCAM &&
		!( nContentsMask & CONTENTS_DEBRIS ) )
	{
		return false;
	}

	return true;
}

const QAngle& C_HL2MP_Player::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return m_PlayerAnimState ?
			m_PlayerAnimState->GetRenderAngles() : GetAbsAngles();
	}
}

void C_HL2MP_Player::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	if ( state == SHOULDTRANSMIT_END )
	{
		if( m_pFlashlightBeam != NULL )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::NotifyShouldTransmit( state );
}

void C_HL2MP_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}

void C_HL2MP_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

	BaseClass::PostDataUpdate( updateType );
}

float C_HL2MP_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV() + GetZoom();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();
	
	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_HL2MP_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool C_HL2MP_Player::CanSprint( void )
{
	return ( (!m_Local.m_bDucked && !m_Local.m_bDucking) && (GetWaterLevel() != 3) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StartSprinting( void )
{
	if( m_HL2Local.m_flSuitPower < 10 )
	{
		// Don't sprint unless there's a reasonable
		// amount of suit power.
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), "HL2Player.SprintNoPower" );
		return;
	}

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "HL2Player.SprintStart" );

	SetMaxSpeed( HL2_SPRINT_SPEED );
	m_fIsSprinting = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StopSprinting( void )
{
	SetMaxSpeed( HL2_NORM_SPEED );
	m_fIsSprinting = false;
}

void C_HL2MP_Player::HandleSpeedChanges( void )
{
	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	if( buttonsChanged & IN_SPEED )
	{
		// The state of the sprint/run button has changed.
		if ( IsSuitEquipped() )
		{
			if ( !(m_afButtonPressed & IN_SPEED)  && IsSprinting() )
			{
				StopSprinting();
			}
			else if ( (m_afButtonPressed & IN_SPEED) && !IsSprinting() )
			{
				if ( CanSprint() )
				{
					StartSprinting();
				}
				else
				{
					// Reset key, so it will be activated post whatever is suppressing it.
					m_nButtons &= ~IN_SPEED;
				}
			}
		}
	}
	else if( buttonsChanged & IN_WALK )
	{
		if ( IsSuitEquipped() )
		{
			// The state of the WALK button has changed. 
			if( IsWalking() && !(m_afButtonPressed & IN_WALK) )
			{
				StopWalking();
			}
			else if( !IsWalking() && !IsSprinting() && (m_afButtonPressed & IN_WALK) && !(m_nButtons & IN_DUCK) )
			{
				StartWalking();
			}
		}
	}

	if ( IsSuitEquipped() && m_fIsWalking && !(m_nButtons & IN_WALK)  ) 
		StopWalking();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StartWalking( void )
{
	SetMaxSpeed( HL2_WALK_SPEED );
	m_fIsWalking = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_HL2MP_Player::StopWalking( void )
{
	SetMaxSpeed( HL2_NORM_SPEED );
	m_fIsWalking = false;
}

void C_HL2MP_Player::ItemPreFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		 return;

	BaseClass::ItemPreFrame();

}
	
void C_HL2MP_Player::ItemPostFrame( void )
{
	if ( GetFlags() & FL_FROZEN )
		 return;

	BaseClass::ItemPostFrame();
}

IRagdoll* C_HL2MP_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_HL2MPRagdoll *pRagdoll = (C_HL2MPRagdoll*)m_hRagdoll.Get();

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

//HL2MPRAGDOLL


IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_HL2MPRagdoll, DT_HL2MPRagdoll, CHL2MPRagdoll )
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_nModelIndex ) ),
	RecvPropInt( RECVINFO(m_nForceBone) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropBool( RECVINFO( m_bStickRagdoll ) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) )
END_RECV_TABLE()



C_HL2MPRagdoll::C_HL2MPRagdoll()
{
	m_bStickRagdoll = false;
}

C_HL2MPRagdoll::~C_HL2MPRagdoll()
{
	PhysCleanupFrictionSounds( this );

	if ( m_hPlayer )
	{
		m_hPlayer->CreateModelInstance();
	}
}

void C_HL2MPRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		const char *pszName = pDestEntry->watcher->GetDebugName();
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pszName ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

void C_HL2MPRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;

	if ( iDamageType == DMG_BLAST )
	{
		dir *= 4000;  // adjust impact strenght
				
		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		dir *= 4000;  // adjust impact strenght

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );	

		// Blood spray!
//		FX_CS_BloodSpray( hitpos, dir, 10 );
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}


void C_HL2MPRagdoll::CreateHL2MPRagdoll( void )
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_HL2MP_Player *pPlayer = dynamic_cast< C_HL2MP_Player* >( m_hPlayer.Get() );
	
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.
		bool bRemotePlayer = (pPlayer != C_BasePlayer::GetLocalPlayer());			
		if ( bRemotePlayer )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( m_vecRagdollOrigin );
			
			SetAbsAngles( pPlayer->GetRenderAngles() );

			SetAbsVelocity( m_vecRagdollVelocity );

			int iSeq = pPlayer->GetSequence();
			if ( iSeq == -1 )
			{
				Assert( false );	// missing walk_lower?
				iSeq = 0;
			}
			
			SetSequence( iSeq );	// walk_lower, basic pose
			SetCycle( 0.0 );

			Interp_Reset( varMap );
		}		
	}
	else
	{
		// overwrite network origin so later interpolation will
		// use this position
		SetNetworkOrigin( m_vecRagdollOrigin );

		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
		
	}

	SetModelIndex( m_nModelIndex );
	if ( pPlayer )
	{
		m_nBody = pPlayer->GetBody();
		const int nBodyGroups =
			MIN( GetNumBodyGroups(), pPlayer->GetNumBodyGroups() );
		for ( int i = 0; i < nBodyGroups; ++i )
			SetBodygroup( i, pPlayer->GetBodygroup( i ) );

		CompleteThirdPersonPlayerBody( this );
	}

	// Make us a ragdoll..
	m_nRenderFX = kRenderFxRagdoll;

	matrix3x4_t boneDelta0[MAXSTUDIOBONES];
	matrix3x4_t boneDelta1[MAXSTUDIOBONES];
	matrix3x4_t currentBones[MAXSTUDIOBONES];
	const float boneDt = 0.05f;

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
	}
	else
	{
		GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
	}

	InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	if ( !pPlayer )
		return;

	m_nSkin = pPlayer->GetSkin();
	SetPoseParameter( 4, pPlayer->GetPoseParameter( 4 ) );
	SetPoseParameter( 5, pPlayer->GetPoseParameter( 5 ) );

	if ( !m_bStickRagdoll || !m_pRagdoll )
		return;

	ragdoll_t *pRagdollData = m_pRagdoll->GetRagdoll();
	IPhysicsObject *pAttached =
		( pRagdollData && pRagdollData->listCount > 1 ) ?
			pRagdollData->list[1].pObject : NULL;
	IPhysicsObject *pReference = GetWorldPhysObject();
	if ( !pAttached || !pReference || !physenv )
		return;

	const Vector vecStart = GetAbsOrigin();
	const Vector vecEnd = vecStart + m_vecForce * 100.0f;
	Ray_t ray;
	ray.Init(
		vecStart, vecEnd,
		Vector( -5.0f, -5.0f, -5.0f ),
		Vector( 5.0f, 5.0f, 5.0f ) );

	trace_t trace;
	CTraceFilterWorldOnly traceFilter;
	enginetrace->TraceRay(
		ray, MASK_PLAYERSOLID_BRUSHONLY, &traceFilter, &trace );

	pAttached->SetMass( pAttached->GetMass() * 2.0f );

	Vector vecAttachedOrigin;
	QAngle angAttached;
	pAttached->GetPosition( &vecAttachedOrigin, &angAttached );

	constraint_ballsocketparams_t ballsocket;
	ballsocket.Defaults();
	pReference->WorldToLocal(
		&ballsocket.constraintPosition[0], trace.endpos );
	pAttached->WorldToLocal(
		&ballsocket.constraintPosition[1], vecAttachedOrigin );
	physenv->CreateBallsocketConstraint(
		pReference, pAttached, NULL, ballsocket );

	if ( fof_blood_allowed.GetBool() )
	{
		UTIL_BloodDrips(
			trace.endpos, -m_vecForce,
			BLOOD_COLOR_RED, 200 );
	}
}


void C_HL2MPRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreateHL2MPRagdoll();
	}
}

IRagdoll* C_HL2MPRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

void C_HL2MPRagdoll::UpdateOnRemove( void )
{
	VPhysicsSetObject( NULL );

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: clear out any face/eye values stored in the material system
//-----------------------------------------------------------------------------
void C_HL2MPRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );

	static float destweight[128];
	static bool bIsInited = false;

	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return;

	int nFlexDescCount = hdr->numflexdesc();
	if ( nFlexDescCount )
	{
		Assert( !pFlexDelayedWeights );
		memset( pFlexWeights, 0, nFlexWeightCount * sizeof(float) );
	}

	if ( m_iEyeAttachment > 0 )
	{
		matrix3x4_t attToWorld;
		if (GetAttachment( m_iEyeAttachment, attToWorld ))
		{
			Vector local, tmp;
			local.Init( 1000.0f, 0.0f, 0.0f );
			VectorTransform( local, attToWorld, tmp );
			modelrender->SetViewTarget( GetModelPtr(), GetBody(), tmp );
		}
	}
}

void C_HL2MP_Player::PostThink( void )
{
	BaseClass::PostThink();

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();
}
