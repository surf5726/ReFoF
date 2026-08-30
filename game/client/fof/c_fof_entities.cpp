//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for FoF weapon crates.
//
//=============================================================================//
#include "cbase.h"
#include "fof/c_fof_entities.h"
#include "clienteffectprecachesystem.h"
#include "glow_outline_effect.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "texture_group_names.h"
#include "view.h"
#include "hl2mp_gamerules.h"
#include "beamdraw.h"
#include "model_types.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectWCrate )
	CLIENTEFFECT_MATERIAL( "vgui/reload_icon_6_on" )
	CLIENTEFFECT_MATERIAL( "vgui/reload_icon_6_off" )
	CLIENTEFFECT_MATERIAL( "vgui/opencrate_on" )
	CLIENTEFFECT_MATERIAL( "vgui/opencrate_off" )
CLIENTEFFECT_REGISTER_END()

struct FoFCrateCircleSegment_t
{
	float m_flThreshold;
	float m_flStartU;
	float m_flStartV;
	float m_flEndU;
	float m_flEndV;
	int m_nDirectionU;
	int m_nDirectionV;
};

static const FoFCrateCircleSegment_t s_CrateCircleSegments[] =
{
	{ 0.125f, 0.5f, 0.0f, 1.0f, 0.0f,  1,  0 },
	{ 0.250f, 1.0f, 0.0f, 1.0f, 0.5f,  0,  1 },
	{ 0.375f, 1.0f, 0.5f, 1.0f, 1.0f,  0,  1 },
	{ 0.500f, 1.0f, 1.0f, 0.5f, 1.0f, -1,  0 },
	{ 0.625f, 0.5f, 1.0f, 0.0f, 1.0f, -1,  0 },
	{ 0.750f, 0.0f, 1.0f, 0.0f, 0.5f,  0, -1 },
	{ 0.875f, 0.0f, 0.5f, 0.0f, 0.0f,  0, -1 },
	{ 1.000f, 0.0f, 0.0f, 0.5f, 0.0f,  1,  0 }
};

static void EmitCrateCircleVertex(
	CMeshBuilder &meshBuilder,
	const Vector &center,
	const Vector &flatRight,
	const Vector &viewUp,
	float flDiameter,
	float u,
	float v,
	const Color &color )
{
	const Vector position =
		center +
		flatRight * ( flDiameter * ( u - 0.5f ) ) -
		viewUp * ( flDiameter * ( v - 0.5f ) );

	meshBuilder.Position3fv( position.Base() );
	meshBuilder.TexCoord2f( 0, u, v );
	meshBuilder.Color4ub( color.r(), color.g(), color.b(), color.a() );
	meshBuilder.AdvanceVertex();
}

C_FoF_Crate::C_FoF_Crate()
	: m_flMessageDuration( 0.0f )
	, m_flMessageStartTime( 0.0f )
	, m_pOpenCrateOffMaterial( NULL )
	, m_pOpenCrateOnMaterial( NULL )
	, m_pGlowEffect( NULL )
	, m_pReloadOffMaterial( NULL )
	, m_pReloadOnMaterial( NULL )
	, m_flNextRegen( 0.0f )
	, m_flTotalRegenTime( 0.0f )
	, m_nLastOverlayDrawFrame( 0 )
	, m_bRegenGlowCreated( false )
{
	Q_memset( m_BaseLayoutPadding, 0, sizeof( m_BaseLayoutPadding ) );
}

C_FoF_Crate::~C_FoF_Crate()
{
	DestroyGlowEffect();
}

void C_FoF_Crate::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	BaseClass::OnDataChanged( updateType );
}

void C_FoF_Crate::ClientThink()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() || currentMode.GetInt() != 4 )
		return;

	if ( m_flNextRegen == -1.0f )
	{
		if ( m_pGlowEffect )
		{
			DestroyGlowEffect();
			m_bRegenGlowCreated = false;
		}
		return;
	}

	if ( !m_bRegenGlowCreated )
	{
		CreateGlowEffect( -1 );
		m_bRegenGlowCreated = true;
	}
}

void C_FoF_Crate::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	const int nMessageType = msg.ReadByte();
	switch ( nMessageType )
	{
	case 1:
		m_flMessageDuration = msg.ReadFloat();
		m_flMessageStartTime = gpGlobals->curtime;
		break;
	case 5:
	{
		const int nGlowType = static_cast< int >( msg.ReadFloat() );
		CreateGlowEffect( nGlowType );
		break;
	}
	case 6:
		DestroyGlowEffect();
		break;
	}
}

int C_FoF_Crate::DrawModel( int flags )
{
	const int result = BaseClass::DrawModel( flags );

	if ( m_nLastOverlayDrawFrame == gpGlobals->framecount )
		return result;

	if ( m_flNextRegen > gpGlobals->curtime )
	{
		const float flProgress = RemapValClamped(
			m_flNextRegen - gpGlobals->curtime,
			0.0f,
			m_flTotalRegenTime,
			1.0f,
			0.0f );

		// The original client writes D3DCOLOR values
		// 0xFF0AFA0F and 0xC8C80000:
		//   off/remainder = reload_icon_6_off, RGBA 10 250 15 255
		//   on/covered    = reload_icon_6_on,  RGBA 200 0 0 200
		// The full green quad is drawn first, then the red remaining
		// sectors.  The resulting green/red ring is the crate-regeneration
		// indicator; it is intentionally distinct from the keyhole ring.
		DrawCircleOverlay(
			m_pReloadOffMaterial,
			m_pReloadOnMaterial,
			"vgui/reload_icon_6_off",
			"vgui/reload_icon_6_on",
			1.0f,
			10.0f,
			flProgress,
			Color( 10, 250, 15, 255 ),
			Color( 200, 0, 0, 200 ) );
	}

	if ( m_flMessageDuration > 0.0f )
	{
		const float flProgress =
			1.0f -
			( m_flMessageDuration - gpGlobals->curtime ) /
			( m_flMessageDuration - m_flMessageStartTime );

		// The original client writes D3DCOLOR values
		// 0xFFC8C8C8 and 0xC8FAC800:
		//   off/remainder = opencrate_off, RGBA 200 200 200 255
		//   on/covered    = opencrate_on,  RGBA 250 200 0 200
		// As above, the full off layer precedes the partial on layer.  The
		// textures themselves supply the white keyhole/yellow fill seen
		// while the player holds USE to open a crate.
		DrawCircleOverlay(
			m_pOpenCrateOffMaterial,
			m_pOpenCrateOnMaterial,
			"vgui/opencrate_off",
			"vgui/opencrate_on",
			11.0f,
			6.0f,
			flProgress,
			Color( 200, 200, 200, 255 ),
			Color( 250, 200, 0, 200 ) );
	}

	m_nLastOverlayDrawFrame = gpGlobals->framecount;
	return result;
}

void C_FoF_Crate::GetRenderBounds( Vector &mins, Vector &maxs )
{
	mins.Init( -20.0f, -20.0f, -20.0f );
	maxs.Init( 20.0f, 20.0f, 20.0f );
}

RenderGroup_t C_FoF_Crate::GetRenderGroup()
{
	return RENDER_GROUP_TRANSLUCENT_ENTITY;
}

void C_FoF_Crate::DrawCircleOverlay(
	IMaterial *&pOffMaterial,
	IMaterial *&pOnMaterial,
	const char *pszOffMaterial,
	const char *pszOnMaterial,
	float flHeight,
	float flDiameter,
	float flProgress,
	const Color &offColor,
	const Color &onColor )
{
	if ( !pOnMaterial )
	{
		pOnMaterial = materials->FindMaterial(
			pszOnMaterial, TEXTURE_GROUP_VGUI, true, NULL );
	}

	if ( !pOffMaterial )
	{
		pOffMaterial = materials->FindMaterial(
			pszOffMaterial, TEXTURE_GROUP_VGUI, true, NULL );
	}

	if ( !pOnMaterial || !pOffMaterial )
		return;

	Vector center = GetRenderOrigin();
	center.z += flHeight;

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
	const Vector &viewUp = CurrentViewUp();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( pOffMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	EmitCrateCircleVertex(
		meshBuilder, center, flatRight, viewUp,
		flDiameter, 0.0f, 0.0f, offColor );
	EmitCrateCircleVertex(
		meshBuilder, center, flatRight, viewUp,
		flDiameter, 1.0f, 0.0f, offColor );
	EmitCrateCircleVertex(
		meshBuilder, center, flatRight, viewUp,
		flDiameter, 1.0f, 1.0f, offColor );
	EmitCrateCircleVertex(
		meshBuilder, center, flatRight, viewUp,
		flDiameter, 0.0f, 1.0f, offColor );
	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->Bind( pOnMaterial );
	pMesh = pRenderContext->GetDynamicMesh();

	for ( int i = 0; i < ARRAYSIZE( s_CrateCircleSegments ); ++i )
	{
		const FoFCrateCircleSegment_t &segment =
			s_CrateCircleSegments[i];
		if ( segment.m_flThreshold <= flProgress )
			continue;

		const float flSegmentFraction = RemapValClamped(
			flProgress,
			segment.m_flThreshold - 0.125f,
			segment.m_flThreshold,
			0.0f,
			1.0f );
		const float flRemainingHalf =
			0.5f * ( 1.0f - flSegmentFraction );
		const float flPartialU =
			segment.m_flEndU -
			static_cast< float >( segment.m_nDirectionU ) *
			flRemainingHalf;
		const float flPartialV =
			segment.m_flEndV -
			static_cast< float >( segment.m_nDirectionV ) *
			flRemainingHalf;

		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 3 );
		EmitCrateCircleVertex(
			meshBuilder, center, flatRight, viewUp,
			flDiameter, 0.5f, 0.5f, onColor );
		EmitCrateCircleVertex(
			meshBuilder, center, flatRight, viewUp,
			flDiameter, flPartialU, flPartialV, onColor );
		EmitCrateCircleVertex(
			meshBuilder, center, flatRight, viewUp,
			flDiameter, segment.m_flEndU, segment.m_flEndV, onColor );
		meshBuilder.End();
		pMesh->Draw();
	}
}

void C_FoF_Crate::CreateGlowEffect( int nType )
{
	DestroyGlowEffect();

	Vector color( 1.0f, 0.1f, 1.0f );
	switch ( nType )
	{
	case 1:
		color.Init( 0.1f, 0.1f, 0.9f );
		break;
	case 2:
		color.Init( 1.0f, 0.24f, 0.1f );
		break;
	case 3:
		color.Init( 1.0f, 0.94f, 0.3f );
		break;
	}

	// FoF registers both flags: crates stay outlined when directly
	// visible and remain visible through world geometry during the reveal.
	m_pGlowEffect = new CGlowObject(
		this, color, 0.85f, true, true );
}

void C_FoF_Crate::DestroyGlowEffect()
{
	delete m_pGlowEffect;
	m_pGlowEffect = NULL;
}

IMPLEMENT_CLIENTCLASS_DT( C_FoF_Crate, DT_FoF_Crate, FoF_Crate )
	RecvPropFloat( RECVINFO( m_flNextRegen ) ),
	RecvPropFloat( RECVINFO( m_flTotalRegenTime ) ),
END_RECV_TABLE()

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterparts for FoF world and objective entities.
//
//=============================================================================//

ConVar fof_smoke_trails(
	"fof_smoke_trails",
	"0",
	FCVAR_ARCHIVE,
	"Gun smoke trails" );

static void FoFVisualQualityChanged(
	IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	NOTE_UNUSED( pOldValue );
	NOTE_UNUSED( flOldValue );

	ConVarRef quality( pConVar );
	const int nQuality = quality.IsValid() ? quality.GetInt() : 2;
	static ConVarRef drawFlecks( "r_drawflecks", true );
	static ConVarRef drawModelDecals( "r_drawmodeldecals", true );
	static ConVarRef decalCoverCount( "r_decal_cover_count", true );
	static ConVarRef newImpactEffects( "cl_new_impact_effects", true );

	const bool bAtLeastMedium = nQuality >= 1;
	if ( drawFlecks.IsValid() )
		drawFlecks.SetValue( bAtLeastMedium ? 1 : 0 );
	if ( drawModelDecals.IsValid() )
		drawModelDecals.SetValue( bAtLeastMedium ? 1 : 0 );
	if ( decalCoverCount.IsValid() )
		decalCoverCount.SetValue( nQuality >= 2 ? 50 : ( nQuality == 1 ? 15 : 4 ) );
	fof_smoke_trails.SetValue( bAtLeastMedium ? 1 : 0 );
	if ( newImpactEffects.IsValid() )
		newImpactEffects.SetValue( nQuality >= 2 ? 1 : 0 );
}

ConVar fof_visual_quality(
	"fof_visual_quality",
	"2",
	FCVAR_ARCHIVE,
	"Adjust several visual quality features, as blood particles, body decals, impacts, weapon smoke trails and other effects. Set 0 to low (some effects may not show at all), 1 to medium, 2 to high",
	true,
	0.0f,
	true,
	2.0f,
	FoFVisualQualityChanged );

C_BaseGhost::C_BaseGhost()
	: m_nGhostReserved( 0 )
{
}

void C_BaseGhost::ImpactTrace(
	trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	const int nQuality = fof_visual_quality.GetInt();
	if ( nQuality == 1 || nQuality == 2 )
	{
		BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
	}
}

bool C_BaseGhost::ShouldCollide(
	int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ||
		 collisionGroup == COLLISION_GROUP_PROJECTILE )
	{
		return false;
	}

	if ( GetCollisionGroup() == COLLISION_GROUP_DEBRIS &&
		 !( contentsMask & CONTENTS_DEBRIS ) )
	{
		return false;
	}

	return true;
}

bool C_BaseGhost::IsPredicted() const
{
	return false;
}

IMPLEMENT_CLIENTCLASS_DT( C_BaseGhost, DT_BaseGhost, CBaseGhost )
END_RECV_TABLE()

C_BBMulti::C_BBMulti()
	: m_pGlowEffect( NULL )
	, m_bGlowCreated( false )
	, m_bVisibleByAll( false )
{
}

C_BBMulti::~C_BBMulti()
{
	DestroyGlowEffect();
}

void C_BBMulti::Spawn()
{
	m_bGlowCreated = false;
}

void C_BBMulti::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	BaseClass::OnDataChanged( updateType );
}

void C_BBMulti::ClientThink()
{
	if ( !m_bGlowCreated && IsEffectActive( EF_ITEM_BLINK ) )
	{
		CreateGlowEffect();
		m_bGlowCreated = true;
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

int C_BBMulti::DrawModel( int flags )
{
	if ( !m_bVisibleByAll &&
		 GetOwnerEntity() != C_BasePlayer::GetLocalPlayer() )
	{
		return 0;
	}

	return BaseClass::DrawModel( flags );
}

void C_BBMulti::CreateGlowEffect()
{
	DestroyGlowEffect();

	const Vector color = ( GetTeamNumber() == 3 )
		? Vector( 1.0f, 0.1f, 0.0f )
		: Vector( 0.8f, 1.0f, 0.1f );
	m_pGlowEffect = new CGlowObject( this, color, 0.7f, true );
}

void C_BBMulti::DestroyGlowEffect()
{
	delete m_pGlowEffect;
	m_pGlowEffect = NULL;
}

IMPLEMENT_CLIENTCLASS_DT( C_BBMulti, DT_BBMulti, CBBMulti )
	RecvPropBool( RECVINFO( m_bVisibleByAll ) ),
END_RECV_TABLE()

bool C_FoFCapEnt::IsPredicted() const
{
	return false;
}

bool C_FoFPushCart::IsPredicted() const
{
	return false;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterparts for FoF's thrown and fired world projectiles.
//
//=============================================================================//

FoFProjectilePresentation_t::FoFProjectilePresentation_t()
	: m_vecLastOrigin( vec3_origin )
	, m_bUpdated( false )
{
}

static void ProjectileOnDataChanged(
	C_BaseCombatCharacter *pProjectile,
	FoFProjectilePresentation_t &presentation,
	DataUpdateType_t updateType )
{
	pProjectile->C_BaseCombatCharacter::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		presentation.m_bUpdated = false;
		presentation.m_vecLastOrigin = pProjectile->GetAbsOrigin();
		pProjectile->SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

static int DrawOwnerHiddenProjectile(
	C_BaseCombatCharacter *pProjectile, int flags )
{
	// First-person bow/crossbow presentation supplies the local projectile;
	// drawing the world entity as well produces a duplicate arrow at release.
	if ( pProjectile->GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() )
		return 0;

	return pProjectile->C_BaseCombatCharacter::DrawModel( flags );
}

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectFoFKnifeBolt )
	CLIENTEFFECT_MATERIAL( "effects/blueflare1" )
CLIENTEFFECT_REGISTER_END()

static int DrawKnifeProjectile(
	C_BaseCombatCharacter *pProjectile,
	FoFProjectilePresentation_t &presentation,
	int flags )
{
	if ( !( flags & STUDIO_TRANSPARENCY ) )
		return pProjectile->C_BaseCombatCharacter::DrawModel( flags );

	IMaterial *pBlurMaterial = materials->FindMaterial(
		"effects/blueflare1", NULL, false );

	Vector vecDir = pProjectile->GetAbsOrigin() - presentation.m_vecLastOrigin;
	float speed = VectorNormalize( vecDir );
	speed = clamp( speed, 0.0f, 32.0f );

	if ( speed > 0.0f )
	{
		const float stepSize = MIN( speed, 5.0f );
		Vector spawnPos = pProjectile->GetAbsOrigin() - vecDir * 6.0f;
		spawnPos.z += 4.0f;
		const Vector spawnStep = -vecDir * stepSize;

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( pBlurMaterial );

		for ( int i = 0; i < 35; ++i )
		{
			spawnPos += spawnStep;
			const float alpha = RemapValClamped(
				i, 5, 15, 0.35f, 0.05f );
			const float color[3] = { alpha, alpha, alpha };
			DrawHalo( pBlurMaterial, spawnPos, 1.5f, color );
		}
	}

	if ( gpGlobals->frametime > 0.0f && !presentation.m_bUpdated )
	{
		presentation.m_bUpdated = true;
		presentation.m_vecLastOrigin = pProjectile->GetAbsOrigin();
	}

	return 1;
}

#define IMPLEMENT_PROJECTILE_COMMON_METHODS( className ) \
	void className::OnDataChanged( DataUpdateType_t updateType ) \
	{ \
		ProjectileOnDataChanged( this, m_Presentation, updateType ); \
	} \
	void className::ClientThink( void ) \
	{ \
		m_Presentation.m_bUpdated = false; \
	} \
	RenderGroup_t className::GetRenderGroup( void ) \
	{ \
		return RENDER_GROUP_TWOPASS; \
	}

IMPLEMENT_PROJECTILE_COMMON_METHODS( C_AxeBolt )

int C_AxeBolt::DrawModel( int flags )
{
	return BaseClass::DrawModel( flags );
}

IMPLEMENT_CLIENTCLASS_DT( C_AxeBolt, DT_AxeBolt, CAxeBolt )
END_RECV_TABLE()

IMPLEMENT_PROJECTILE_COMMON_METHODS( C_BowarrowBolt )

int C_BowarrowBolt::DrawModel( int flags )
{
	return DrawOwnerHiddenProjectile( this, flags );
}

IMPLEMENT_CLIENTCLASS_DT( C_BowarrowBolt, DT_BowarrowBolt, CBowarrowBolt )
END_RECV_TABLE()

IMPLEMENT_PROJECTILE_COMMON_METHODS( C_KnifeBolt )

int C_KnifeBolt::DrawModel( int flags )
{
	return DrawKnifeProjectile( this, m_Presentation, flags );
}

IMPLEMENT_CLIENTCLASS_DT( C_KnifeBolt, DT_KnifeBolt, CKnifeBolt )
END_RECV_TABLE()

IMPLEMENT_PROJECTILE_COMMON_METHODS( C_XArrow )

int C_XArrow::DrawModel( int flags )
{
	return DrawOwnerHiddenProjectile( this, flags );
}

IMPLEMENT_CLIENTCLASS_DT( C_XArrow, DT_XArrow, CXArrow )
END_RECV_TABLE()

#if defined( _M_IX86 )
	COMPILE_TIME_ASSERT( sizeof( FoFProjectilePresentation_t ) == 0x10 );
	COMPILE_TIME_ASSERT( sizeof( C_AxeBolt ) == 0xE18 );
	COMPILE_TIME_ASSERT( sizeof( C_BowarrowBolt ) == 0xE18 );
	COMPILE_TIME_ASSERT( sizeof( C_KnifeBolt ) == 0xE18 );
	COMPILE_TIME_ASSERT( sizeof( C_XArrow ) == 0xE18 );
#endif

#undef IMPLEMENT_PROJECTILE_COMMON_METHODS

// Client rendering and collision for FoF push-cart objectives.

static ConVar fof_sv_cart_highlight(
	"fof_sv_cart_highlight",
	"1",
	FCVAR_REPLICATED,
	"Outline effect active for cart model" );

C_FoFPushCart::C_FoFPushCart()
	: m_bEnabled( false )
	, m_pGlowEffect( NULL )
{
}

C_FoFPushCart::~C_FoFPushCart()
{
	DestroyGlowEffect();
}

void C_FoFPushCart::Spawn()
{
	if ( fof_sv_cart_highlight.GetInt() != 0 )
	{
		CreateGlowEffect();
	}
}

void C_FoFPushCart::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		if ( m_bEnabled )
		{
			if ( !m_pGlowEffect && fof_sv_cart_highlight.GetInt() != 0 )
			{
				CreateGlowEffect();
			}
		}
		else if ( m_pGlowEffect )
		{
			DestroyGlowEffect();
		}
	}

	BaseClass::OnDataChanged( updateType );
}

void C_FoFPushCart::CreateGlowEffect()
{
	DestroyGlowEffect();

	Vector color( 1.0f, 0.94f, 0.3f );
	switch ( GetTeamNumber() )
	{
	case 0:
		color.Init( 1.0f, 0.1f, 1.0f );
		break;
	case 2:
		color.Init( 0.0f, 0.1f, 1.0f );
		break;
	case 3:
		color.Init( 1.0f, 0.1f, 0.0f );
		break;
	}

	m_pGlowEffect = new CGlowObject( this, color, 0.65f, true );
}

void C_FoFPushCart::DestroyGlowEffect()
{
	delete m_pGlowEffect;
	m_pGlowEffect = NULL;
}

IMPLEMENT_CLIENTCLASS_DT( C_FoFPushCart, DT_FoFPushCart, CFoFPushCart )
	RecvPropBool( RECVINFO( m_bEnabled ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_FoFPushCart )
END_PREDICTION_DATA()

bool C_FuncRespawnRoomVisualizer::ShouldCollide(
	int collisionGroup, int contentsMask ) const
{
	const int nTeam = GetTeamNumber();
	if ( nTeam == 0 || collisionGroup != COLLISION_GROUP_PLAYER_MOVEMENT )
		return false;

	if ( nTeam == 2 )
		return ( contentsMask & CONTENTS_TEAM2 ) != 0;

	if ( nTeam == 3 )
		return ( contentsMask & CONTENTS_TEAM1 ) != 0;

	return true;
}

IMPLEMENT_CLIENTCLASS_DT(
	C_FuncRespawnRoomVisualizer,
	DT_FuncRespawnRoomVisualizer,
	CFuncRespawnRoomVisualizer )
END_RECV_TABLE()

// Client rendering for FoF capture and elimination safe zones.

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectFoFSafeZone )
	CLIENTEFFECT_MATERIAL( "effects/safezone_quad" )
CLIENTEFFECT_REGISTER_END()

static void EmitSafeZoneVertex(
	CMeshBuilder &meshBuilder,
	const Vector &position,
	float u,
	float v )
{
	meshBuilder.Position3fv( position.Base() );
	meshBuilder.TexCoord2f( 0, u, v );
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();
}

C_FoFCapEnt::C_FoFCapEnt()
	: m_pSafeZoneMaterial( NULL )
	, m_nClassFilter( 0 )
	, m_nAnnounceFilter( 0 )
	, m_bCapActive( false )
	, m_bBeingCaptured( false )
	, m_flCapProgress( 0.0f )
{
	Q_memset( m_BaseLayoutPadding, 0, sizeof( m_BaseLayoutPadding ) );
}

void C_FoFCapEnt::Spawn()
{
	m_pSafeZoneMaterial = materials->FindMaterial(
		"effects/safezone_quad",
		TEXTURE_GROUP_CLIENT_EFFECTS,
		true,
		NULL );
	m_pSafeZoneMaterial->IncrementReferenceCount();
}

int C_FoFCapEnt::DrawModel( int flags )
{
	if ( !m_bCapActive )
		return 0;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 4 )
	{
		DrawSafeZoneMesh();
	}

	return BaseClass::DrawModel( flags );
}

void C_FoFCapEnt::GetRenderBounds( Vector &mins, Vector &maxs )
{
	BaseClass::GetRenderBounds( mins, maxs );

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 4 )
	{
		mins.x -= 1500.0f;
		mins.y -= 1500.0f;
		maxs.x += 1500.0f;
		maxs.y += 1500.0f;
		maxs.z += 1500.0f;
	}
}

void C_FoFCapEnt::ComputeWorldSpaceSurroundingBox(
	Vector *pWorldMins, Vector *pWorldMaxs )
{
	const Vector &origin = GetAbsOrigin();
	*pWorldMins = origin + Vector( -1000.0f, -1000.0f, 0.0f );
	*pWorldMaxs = origin + Vector( 1000.0f, 1000.0f, 1000.0f );
}

void C_FoFCapEnt::DrawSafeZoneMesh()
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules || !materials )
		return;

	if ( !m_pSafeZoneMaterial )
	{
		m_pSafeZoneMaterial = materials->FindMaterial(
			"effects/safezone_quad",
			TEXTURE_GROUP_CLIENT_EFFECTS,
			true,
			NULL );
	}

	if ( !m_pSafeZoneMaterial || IsErrorMaterial( m_pSafeZoneMaterial ) )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( m_pSafeZoneMaterial, NULL );

	for ( int safeZoneIndex = 0; ; ++safeZoneIndex )
	{
		Vector corner0;
		Vector corner1;
		Vector corner2;
		Vector corner3;
		if ( !pRules->GetFoFSafeZoneQuad(
			safeZoneIndex, corner0, corner1, corner2, corner3 ) )
		{
			break;
		}

		IMesh *pMesh = pRenderContext->GetDynamicMesh(
			true, NULL, NULL, NULL );
		if ( !pMesh )
			break;

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
		EmitSafeZoneVertex( meshBuilder, corner0, 0.0f, 0.0f );
		EmitSafeZoneVertex( meshBuilder, corner1, 1.0f, 0.0f );
		EmitSafeZoneVertex( meshBuilder, corner2, 1.0f, 1.0f );
		EmitSafeZoneVertex( meshBuilder, corner3, 0.0f, 1.0f );
		meshBuilder.End();
		pMesh->Draw();
	}
}

IMPLEMENT_CLIENTCLASS_DT( C_FoFCapEnt, DT_FoFCapEnt, CFoFCapEnt )
	RecvPropBool( RECVINFO( m_bCapActive ) ),
	RecvPropBool( RECVINFO( m_bBeingCaptured ) ),
	RecvPropInt( RECVINFO( m_nClassFilter ) ),
	RecvPropInt( RECVINFO( m_nAnnounceFilter ) ),
	RecvPropFloat( RECVINFO( m_flCapProgress ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_FoFCapEnt )
END_PREDICTION_DATA()
