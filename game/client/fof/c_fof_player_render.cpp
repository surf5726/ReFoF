#include "cbase.h"
#include "fof/c_fof_player.h"
#include "fof/fof_client_settings.h"
#include "iinput.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "texture_group_names.h"
#include "tier0/vprof.h"
#include "particles_new.h"
#include "view.h"

#include "tier0/memdbgon.h"

extern ConVar fof_bodyawareness;

static ConVar cl_legs_origin_shift(
	"cl_legs_origin_shift",
	"-18",
	FCVAR_CHEAT,
	"Amount in game units to shift the player model relative to the direction the player is facing" );

static IMaterial *s_pFoFLootTargetMaterial = NULL;

static void FoFEmitLootTargetVertex(
	CMeshBuilder &meshBuilder,
	const Vector &center,
	const Vector &flatRight,
	const Vector &viewUp,
	float u,
	float v,
	const Color &color )
{
	const Vector position =
		center +
		flatRight * ( 8.0f * ( u - 0.5f ) ) -
		viewUp * ( 8.0f * ( v - 0.5f ) );

	meshBuilder.Position3fv( position.Base() );
	meshBuilder.TexCoord2f( 0, u, v );
	meshBuilder.Color4ub( color.r(), color.g(), color.b(), color.a() );
	meshBuilder.AdvanceVertex();
}

static bool FoFShouldDrawEliminationLootTarget(
	C_FoF_Player *pPlayer )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || !pLocalPlayer || pPlayer == pLocalPlayer )
	{
		return false;
	}

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	return currentMode.IsValid() && currentMode.GetInt() == 4 &&
		( !battleRoyale.IsValid() || !battleRoyale.GetBool() ) &&
		pPlayer->GetFoFPlayerKills() != -1;
}

static void FoFDrawEliminationLootTarget( C_FoF_Player *pPlayer )
{
	if ( !FoFShouldDrawEliminationLootTarget( pPlayer ) || !materials )
		return;

	if ( !s_pFoFLootTargetMaterial )
	{
		s_pFoFLootTargetMaterial = materials->FindMaterial(
			"vgui/loot_target",
			TEXTURE_GROUP_VGUI,
			true,
			NULL );
	}

	if ( !s_pFoFLootTargetMaterial ||
		IsErrorMaterial( s_pFoFLootTargetMaterial ) )
	{
		return;
	}

	const Vector &viewRight = CurrentViewRight();
	if ( fabsf( viewRight.z ) > 0.95f )
		return;

	const float flInvFlatLength = 1.0f / sqrtf(
		viewRight.x * viewRight.x +
		viewRight.y * viewRight.y +
		1.0e-10f );
	const Vector flatRight(
		viewRight.x * flInvFlatLength,
		viewRight.y * flInvFlatLength,
		0.0f );

	Vector forward;
	AngleVectors( pPlayer->GetRenderAngles(), &forward );
	Vector center = pPlayer->WorldSpaceCenter() + forward * 2.0f;
	center.z -= 25.0f;

	const Color color = pPlayer->GetTeamNumber() == 3 ?
		Color( 255, 0, 0, 255 ) :
		Color( 0, 0, 255, 255 );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( s_pFoFLootTargetMaterial, NULL );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	if ( !pMesh )
		return;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	FoFEmitLootTargetVertex(
		meshBuilder, center, flatRight, CurrentViewUp(),
		0.0f, 0.0f, color );
	FoFEmitLootTargetVertex(
		meshBuilder, center, flatRight, CurrentViewUp(),
		1.0f, 0.0f, color );
	FoFEmitLootTargetVertex(
		meshBuilder, center, flatRight, CurrentViewUp(),
		1.0f, 1.0f, color );
	FoFEmitLootTargetVertex(
		meshBuilder, center, flatRight, CurrentViewUp(),
		0.0f, 1.0f, color );
	meshBuilder.End();
	pMesh->Draw();
}

bool C_FoF_Player::IsFoFFirstPersonBody() const
{
	return fof_bodyawareness.GetBool() &&
		this == C_BasePlayer::GetLocalPlayer() &&
		m_lifeState == LIFE_ALIVE &&
		!input->CAM_IsThirdPerson() &&
		!C_BasePlayer::ShouldDrawLocalPlayer() &&
		( m_nPlayerInfo & 0x2 );
}

bool C_FoF_Player::ShouldDraw()
{
	if ( IsFoFFirstPersonBody() )
	{
		// The current SDK's C_BasePlayer::ShouldDraw deliberately suppresses
		// the local player in first person.  FoF bypasses that one test for
		// its server-approved body-awareness pass, while retaining the stock
		// alive/ragdoll and generic entity visibility checks.
		if ( !IsAlive() || IsRagdoll() )
			return false;
		return C_BaseAnimating::ShouldDraw();
	}

	return BaseClass::ShouldDraw();
}

const Vector &C_FoF_Player::GetRenderOrigin()
{
	// The visible first-person body is shifted along
	// GetRenderAngles by cl_legs_origin_shift (original default -18), rather
	// than being rendered directly on top of the camera.  Both the animated
	// skeleton and this shift deliberately share the FoF animstate angle.
	if ( IsFoFFirstPersonBody() && IsAlive() )
	{
		static Vector s_vecFoFFirstPersonBodyOrigin;
		const Vector &vecBaseRenderOrigin = BaseClass::GetRenderOrigin();
		Vector vecForward;
		AngleVectors( GetRenderAngles(), &vecForward );
		VectorMA(
			vecBaseRenderOrigin,
			cl_legs_origin_shift.GetFloat(),
			vecForward,
			s_vecFoFFirstPersonBodyOrigin );
		return s_vecFoFFirstPersonBodyOrigin;
	}

	return BaseClass::GetRenderOrigin();
}

RenderGroup_t C_FoF_Player::GetRenderGroup()
{
	// The original client places the local body-awareness model in the
	// opaque view-model bucket only for a genuine first-person render.  The
	// replicated player-info bit is the server-approved body-awareness state.
	if ( IsFoFFirstPersonBody() )
	{
		return RENDER_GROUP_VIEW_MODEL_OPAQUE;
	}

	return RENDER_GROUP_OPAQUE_ENTITY;
}

int C_FoF_Player::DrawModel( int flags )
{
	VPROF_BUDGET(
		"FoF::Player::DrawModel",
		VPROF_BUDGETGROUP_MODEL_RENDERING );
	// The original client selects two distinct player1.mdl presentations:
	// studio=0 leaves the base no-arms mesh for first person, while studio=1
	// adds the model's only-arms submodel to complete the third-person player.
	const bool bFirstPersonBody = IsFoFFirstPersonBody();

	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr )
	{
		return bFirstPersonBody ?
			C_BaseAnimating::DrawModel( flags ) :
			BaseClass::DrawModel( flags );
	}

	const int nChangedBodygroups =
		MIN( GetNumBodyGroups(), bFirstPersonBody ? 5 : 1 );
	for ( int i = 0; i < nChangedBodygroups; ++i )
		SetBodygroup( i, bFirstPersonBody ? 0 : ( i == 0 ? 1 : 0 ) );

	// FoF's original base draw path predates the current SDK's extra
	// C_BasePlayer::ShouldDrawThisPlayer gate.  Bypass only that gate for
	// this body-awareness pass; C_BaseAnimating still checks ready-to-draw.
	// The original client leaves these presentation bodygroups in
	// place after the draw. Restoring a different replicated body value made
	// later view/shadow passes observe a different mesh than the opaque body
	// pass, which could make the lower body blink while moving.
	if ( bFirstPersonBody )
		return C_BaseAnimating::DrawModel( flags );

	FoFDrawEliminationLootTarget( this );
	return BaseClass::DrawModel( flags );
}

#ifdef GLOWS_ENABLE
void C_FoF_Player::GetGlowEffectColor( float *r, float *g, float *b )
{
	// Exact color branches from the original client.  Teamplay and
	// elimination use the four FoF team colors; co-op uses a health warning
	// color, with the original strict health > 30 threshold.
	*r = 0.76f;
	*g = 0.76f;
	*b = 0.76f;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 0;
	if ( nMode == 2 || nMode == 4 )
	{
		switch ( GetTeamNumber() )
		{
		case 2:
			*r = 0.16f;
			*g = 0.16f;
			break;
		case 3:
			*g = 0.16f;
			*b = 0.16f;
			break;
		case 4:
			*r = 0.16f;
			*b = 0.16f;
			break;
		case 5:
			*b = 0.16f;
			break;
		}
	}

	if ( nMode == 6 )
	{
		*g = GetHealth() > 30 ? 0.76f : 0.0f;
		*b = 0.0f;
	}
}
#endif

void C_FoF_Player::StopFoFLowHealthBlood()
{
	if ( m_pFoFLowHealthBlood )
	{
		ParticleProp()->StopEmission(
			m_pFoFLowHealthBlood, false, false );
		m_pFoFLowHealthBlood = NULL;
	}

	m_iFoFLowHealthBloodAttachment = 0;
}

void C_FoF_Player::UpdateFoFLowHealthBlood()
{
	// The original client keeps this independent of visual quality.  The
	// effect is only suppressed by the global low-violence policy, just like
	// the shipped client.
	if ( !fof_blood_allowed.GetBool() )
	{
		StopFoFLowHealthBlood();
		return;
	}

	if ( !gpGlobals || gpGlobals->maxClients == 0 || UTIL_IsLowViolence() )
		return;

	if ( m_pFoFLowHealthBlood )
	{
		if ( GetHealth() >= 50 || !IsAlive() )
			StopFoFLowHealthBlood();
		return;
	}

	if ( !IsAlive() || GetHealth() >= 50 || m_bIsBotGhost )
		return;

	// LookupAttachment also performs the original lazy model-header lock and
	// fails cleanly while a player model is not ready to draw.
	m_iFoFLowHealthBloodAttachment = LookupAttachment( "chest" );
	if ( m_iFoFLowHealthBloodAttachment <= 0 )
	{
		m_iFoFLowHealthBloodAttachment = 0;
		return;
	}

	m_pFoFLowHealthBlood = ParticleProp()->Create(
		"chest_blood",
		PATTACH_POINT_FOLLOW,
		m_iFoFLowHealthBloodAttachment,
		vec3_origin );
	if ( !m_pFoFLowHealthBlood )
		return;

	// The original initializes control point zero from the attachment even
	// though PATTACH_POINT_FOLLOW keeps it updated on later frames.
	Vector vecChestOrigin;
	QAngle chestAngles;
	if ( GetAttachment(
			m_iFoFLowHealthBloodAttachment,
			vecChestOrigin,
			chestAngles ) )
	{
		m_pFoFLowHealthBlood->SetControlPoint( 0, vecChestOrigin );
	}
}
