#include "cbase.h"
#include "fof/fof_postprocess_effects.h"

#include "c_world.h"
#include "cdll_client_int.h"
#include "clienteffectprecachesystem.h"
#include "fof/c_fof_player.h"
#include "fof/fof_weapon_properties.h"
#include "iviewrender.h"
#include "KeyValues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "texture_group_names.h"
#include "view.h"
#include "view_scene.h"
#include "view_shared.h"

#include "tier0/memdbgon.h"

// FoF depth-of-field post processing.

ConVar mat_dof_enabled( "mat_dof_enabled", "1" );
ConVar mat_dof_override( "mat_dof_override", "0" );
ConVar mat_dof_near_blur_depth( "mat_dof_near_blur_depth", "20.0" );
ConVar mat_dof_near_focus_depth( "mat_dof_near_focus_depth", "100.0" );
ConVar mat_dof_far_focus_depth( "mat_dof_far_focus_depth", "250.0" );
ConVar mat_dof_far_blur_depth( "mat_dof_far_blur_depth", "1000.0" );
ConVar mat_dof_near_blur_radius( "mat_dof_near_blur_radius", "10.0" );
ConVar mat_dof_far_blur_radius( "mat_dof_far_blur_radius", "5.0" );
ConVar mat_dof_quality( "mat_dof_quality", "0" );

// FoF carries the Portal 2 depth-of-field globals but no
// DT_EnvDOFController receive class. Normal gameplay therefore keeps them
// disabled; SFM view data or mat_dof_override can explicitly enable the pass.
static bool g_bFoFDepthOfFieldEnabled = false;
static float g_flFoFNearBlurDepth = 50.0f;
static float g_flFoFNearFocusDepth = 200.0f;
static float g_flFoFFarFocusDepth = 250.0f;
static float g_flFoFFarBlurDepth = 1000.0f;
static float g_flFoFNearBlurRadius = 0.0f;
static float g_flFoFFarBlurRadius = 5.0f;

CLIENTEFFECT_REGISTER_BEGIN( PrecacheDoFEffects )
	CLIENTEFFECT_MATERIAL( "dev/depth_of_field" )
	CLIENTEFFECT_MATERIAL( "dev/blurgaussian_3x3" )
CLIENTEFFECT_REGISTER_END()

static float FoFGetNearBlurDepth()
{
	return mat_dof_override.GetBool() ?
		mat_dof_near_blur_depth.GetFloat() : g_flFoFNearBlurDepth;
}

static float FoFGetNearFocusDepth()
{
	return mat_dof_override.GetBool() ?
		mat_dof_near_focus_depth.GetFloat() : g_flFoFNearFocusDepth;
}

static float FoFGetFarFocusDepth()
{
	return mat_dof_override.GetBool() ?
		mat_dof_far_focus_depth.GetFloat() : g_flFoFFarFocusDepth;
}

static float FoFGetFarBlurDepth()
{
	return mat_dof_override.GetBool() ?
		mat_dof_far_blur_depth.GetFloat() : g_flFoFFarBlurDepth;
}

static float FoFGetNearBlurRadius()
{
	return mat_dof_override.GetBool() ?
		mat_dof_near_blur_radius.GetFloat() : g_flFoFNearBlurRadius;
}

static float FoFGetFarBlurRadius()
{
	return mat_dof_override.GetBool() ?
		mat_dof_far_blur_radius.GetFloat() : g_flFoFFarBlurRadius;
}

static void FoFSetRenderTargetAndViewport( ITexture *pRenderTarget )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget( pRenderTarget );
	pRenderContext->Viewport( 0, 0, pRenderTarget->GetActualWidth(),
		pRenderTarget->GetActualHeight() );
}

static void FoFDownsampleFrameBufferQuarterSize(
	IMatRenderContext *pRenderContext, int nSourceWidth, int nSourceHeight,
	ITexture *pDestination )
{
	Assert( pRenderContext );
	Assert( pDestination );

	IMaterial *pDownsample = materials->FindMaterial(
		"dev/downsample", TEXTURE_GROUP_OTHER, true );

	Assert( pDestination->GetActualWidth() == nSourceWidth / 4 );
	Assert( pDestination->GetActualHeight() == nSourceHeight / 4 );

	FoFSetRenderTargetAndViewport( pDestination );
	pRenderContext->DrawScreenSpaceRectangle(
		pDownsample, 0, 0, nSourceWidth / 4, nSourceHeight / 4,
		0, 0, nSourceWidth - 2, nSourceHeight - 2,
		nSourceWidth, nSourceHeight );

	if ( IsX360() )
		pRenderContext->CopyRenderTargetToTextureEx(
			pDestination, 0, NULL, NULL );
}

static bool FoFSetMaterialVarFloat(
	IMaterial *pMaterial, const char *pszName, float flValue )
{
	Assert( pMaterial );
	Assert( pszName );
	if ( !pMaterial || !pszName )
		return false;

	bool bFound = false;
	IMaterialVar *pVariable = pMaterial->FindVar( pszName, &bFound );
	if ( bFound )
		pVariable->SetFloatValue( flValue );
	return bFound;
}

static bool FoFSetMaterialVarInt(
	IMaterial *pMaterial, const char *pszName, int nValue )
{
	Assert( pMaterial );
	Assert( pszName );
	if ( !pMaterial || !pszName )
		return false;

	bool bFound = false;
	IMaterialVar *pVariable = pMaterial->FindVar( pszName, &bFound );
	if ( bFound )
		pVariable->SetIntValue( nValue );
	return bFound;
}

bool FoFIsDepthOfFieldEnabled()
{
	const CViewSetup *pViewSetup = view->GetViewSetup();
	if ( !pViewSetup )
		return false;

	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_FLOAT )
		return false;
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 92 )
		return false;

	if ( pViewSetup->m_bDoDepthOfField )
		return true;
	if ( !mat_dof_enabled.GetBool() )
		return false;
	if ( mat_dof_override.GetBool() )
		return mat_dof_enabled.GetBool();

	return g_bFoFDepthOfFieldEnabled;
}

void FoFDoDepthOfField( const CViewSetup &viewSetup )
{
	if ( !FoFIsDepthOfFieldEnabled() )
		return;

	UpdateScreenEffectTexture( 0, viewSetup.x, viewSetup.y,
		viewSetup.width, viewSetup.height, false );

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *pSource = materials->FindTexture(
		"_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	const int nSourceWidth = pSource->GetActualWidth();
	const int nSourceHeight = pSource->GetActualHeight();

	if ( mat_dof_quality.GetInt() < 2 )
	{
		pRenderContext->PushRenderTargetAndViewport();
		ITexture *pSmallFrameBuffer0 = materials->FindTexture(
			"_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );

		Assert( pSmallFrameBuffer0->GetActualWidth() ==
			pSource->GetActualWidth() / 4 );
		Assert( pSmallFrameBuffer0->GetActualHeight() ==
			pSource->GetActualHeight() / 4 );

		FoFDownsampleFrameBufferQuarterSize( pRenderContext,
			nSourceWidth, nSourceHeight, pSmallFrameBuffer0 );

		IMaterial *pGaussianBlur = materials->FindMaterial(
			"dev/blurgaussian_3x3", TEXTURE_GROUP_OTHER, true );
		if ( !pGaussianBlur )
		{
			pRenderContext->PopRenderTargetAndViewport();
			return;
		}

		FoFSetMaterialVarFloat( pGaussianBlur, "$c0_x",
			0.5f / pSmallFrameBuffer0->GetActualWidth() );
		FoFSetMaterialVarFloat( pGaussianBlur, "$c0_y",
			0.5f / pSmallFrameBuffer0->GetActualHeight() );
		FoFSetMaterialVarFloat( pGaussianBlur, "$c1_x",
			-0.5f / pSmallFrameBuffer0->GetActualWidth() );
		FoFSetMaterialVarFloat( pGaussianBlur, "$c1_y",
			0.5f / pSmallFrameBuffer0->GetActualHeight() );

		ITexture *pSmallFrameBuffer1 = materials->FindTexture(
			"_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
		FoFSetRenderTargetAndViewport( pSmallFrameBuffer1 );
		pRenderContext->DrawScreenSpaceRectangle(
			pGaussianBlur, 0, 0, nSourceWidth / 4, nSourceHeight / 4,
			0, 0, pSmallFrameBuffer0->GetActualWidth() - 1,
			pSmallFrameBuffer0->GetActualHeight() - 1,
			pSmallFrameBuffer0->GetActualWidth(),
			pSmallFrameBuffer0->GetActualHeight() );

		if ( IsX360() )
			pRenderContext->CopyRenderTargetToTextureEx(
				pSmallFrameBuffer1, 0, NULL, NULL );

		pRenderContext->PopRenderTargetAndViewport();
	}

	int nViewportWidth = 0;
	int nViewportHeight = 0;
	int nDummy = 0;
	pRenderContext->GetViewport(
		nDummy, nDummy, nViewportWidth, nViewportHeight );

	IMaterial *pDepthOfField = materials->FindMaterial(
		"dev/depth_of_field", TEXTURE_GROUP_OTHER, true );
	if ( !pDepthOfField )
		return;

	FoFSetMaterialVarFloat( pDepthOfField, "$nearPlane", viewSetup.zNear );
	FoFSetMaterialVarFloat( pDepthOfField, "$farPlane", viewSetup.zFar );

	if ( viewSetup.m_bDoDepthOfField )
	{
		FoFSetMaterialVarFloat( pDepthOfField, "$nearBlurDepth",
			viewSetup.m_flNearBlurDepth );
		FoFSetMaterialVarFloat( pDepthOfField, "$nearFocusDepth",
			viewSetup.m_flNearFocusDepth );
		FoFSetMaterialVarFloat( pDepthOfField, "$farFocusDepth",
			viewSetup.m_flFarFocusDepth );
		FoFSetMaterialVarFloat( pDepthOfField, "$farBlurDepth",
			viewSetup.m_flFarBlurDepth );
		FoFSetMaterialVarFloat( pDepthOfField, "$nearBlurRadius",
			viewSetup.m_flNearBlurRadius );
		FoFSetMaterialVarFloat( pDepthOfField, "$farBlurRadius",
			viewSetup.m_flFarBlurRadius );
		FoFSetMaterialVarInt( pDepthOfField, "$quality",
			viewSetup.m_nDoFQuality );
	}
	else
	{
		FoFSetMaterialVarFloat( pDepthOfField, "$nearBlurDepth",
			FoFGetNearBlurDepth() );
		FoFSetMaterialVarFloat( pDepthOfField, "$nearFocusDepth",
			FoFGetNearFocusDepth() );
		FoFSetMaterialVarFloat( pDepthOfField, "$farFocusDepth",
			FoFGetFarFocusDepth() );
		FoFSetMaterialVarFloat( pDepthOfField, "$farBlurDepth",
			FoFGetFarBlurDepth() );
		FoFSetMaterialVarFloat( pDepthOfField, "$nearBlurRadius",
			FoFGetNearBlurRadius() );
		FoFSetMaterialVarFloat( pDepthOfField, "$farBlurRadius",
			FoFGetFarBlurRadius() );
		FoFSetMaterialVarInt( pDepthOfField, "$quality",
			mat_dof_quality.GetInt() );
	}

	pRenderContext->DrawScreenSpaceRectangle(
		pDepthOfField, 0, 0, nViewportWidth, nViewportHeight,
		0, 0, nSourceWidth - 1, nSourceHeight - 1,
		nSourceWidth, nSourceHeight,
		GetClientWorldEntity()->GetClientRenderable() );
}

void FoFApplyDepthOfField( const CViewSetup &viewSetup )
{
	if ( !FoFIsDepthOfFieldEnabled() )
		return;

	CMatRenderContextPtr pRenderContext;
	pRenderContext.GetFrom( materials );
	PIXEVENT( pRenderContext, "FoFDoDepthOfField" );
	FoFDoDepthOfField( viewSetup );
	pRenderContext.SafeRelease();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF glow material lifetime registration.
//
//=============================================================================//

// FindMaterial does not own a reference.  The shipped FoF client registers
// this exact pair for the lifetime of each level so the glow compositor never
// attempts to bind a reference-count-zero material.
CLIENTEFFECT_REGISTER_BEGIN( PrecacheFoFGlowMaterials )
	CLIENTEFFECT_MATERIAL( "dev/glow_color" )
	CLIENTEFFECT_MATERIAL( "dev/halo_add_to_screen" )
CLIENTEFFECT_REGISTER_END()

static ConVar fof_ironsight_fx(
	"fof_ironsight_fx",
	"1",
	FCVAR_ARCHIVE,
	"Enables the long-gun ironsight radial blur effect" );

static C_FoF_Player *FoFGetIronsightViewPlayer()
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return NULL;

	C_BasePlayer *pViewPlayer = pLocalPlayer;
	if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BasePlayer *pTarget = ToBasePlayer(
			pLocalPlayer->GetObserverTarget() );
		if ( pTarget )
			pViewPlayer = pTarget;
	}

	return dynamic_cast< C_FoF_Player * >( pViewPlayer );
}

void FoFDrawIronsightEffect( const CViewSetup *pView )
{
	if ( !pView || pView->width < 90 || !fof_ironsight_fx.GetBool() )
		return;

	C_FoF_Player *pPlayer = FoFGetIronsightViewPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() ||
		pPlayer->GetFoFSightExpFactor() <= 0.0f )
	{
		return;
	}

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !pWeapon || pWeapon->FoFWeaponID() != 3 )
		return;

	IMaterial *pMaterial = materials->FindMaterial(
		"shaders/Radial_Blur", TEXTURE_GROUP_CLIENT_EFFECTS, true );
	if ( !pMaterial || pMaterial->IsErrorMaterial() )
		return;

	bool bFoundThreshold = false;
	IMaterialVar *pThreshold = pMaterial->FindVar(
		"$scopethreshold", &bFoundThreshold, false );
	if ( !bFoundThreshold || !pThreshold )
		return;

	// Match the original client's post-process setup:
	// RemapValClamped( sight, 1, 0, 0.15, 1 ).
	pThreshold->SetFloatValue( RemapValClamped(
		pPlayer->GetFoFSightExpFactor(), 1.0f, 0.0f, 0.15f, 1.0f ) );
	DrawScreenEffectMaterial(
		pMaterial, pView->x, pView->y, pView->width, pView->height );
}

#ifdef _X360
#define FOF_STUN_TEXTURE "_rt_FullFrameFB2"
#else
#define FOF_STUN_TEXTURE "_rt_FullScreen"
#endif

CDrunkEffect::CDrunkEffect()
	: m_bUpdateView( true )
	, m_bEnabled( false )
	, m_nLastAlpha( 0 )
{
}

ADD_SCREENSPACE_EFFECT( CDrunkEffect, fof_drunk );

void CDrunkEffect::Init( void )
{
	m_bUpdateView = true;
	m_nLastAlpha = 0;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "_rt_FullScreen" );
	m_DrunkMaterial.Init( "__DrunkEffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_DrunkTexture.Init( "_rt_FullScreen", TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CDrunkEffect::Shutdown( void )
{
	m_DrunkMaterial.Shutdown();
	m_DrunkTexture.Shutdown();
}

void CDrunkEffect::SetParameters( KeyValues *pParams )
{
	NOTE_UNUSED( pParams );
}

void CDrunkEffect::Enable( bool bEnable )
{
	m_bEnabled = bEnable;
}

bool CDrunkEffect::IsEnabled( void )
{
	return m_bEnabled;
}

unsigned char CDrunkEffect::GetFadeAlpha( void ) const
{
	C_FoF_Player *pPlayer = dynamic_cast< C_FoF_Player * >(
		C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayer || !pPlayer->IsAlive() )
		return 0;

	// The original client calls IMaterialSystemHardwareConfig::GetHDRType:
	// HDR caps the effect at 85, while the non-HDR path caps it at 125.
	const bool bHDR = g_pMaterialSystemHardwareConfig &&
		g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
	const float flMaxAlpha = bHDR ? 85.0f : 125.0f;
	return static_cast< unsigned char >( RemapValClamped(
		pPlayer->GetFoFDrunkness(), 0.0f, 10.0f, 0.0f, flMaxAlpha ) );
}

void CDrunkEffect::Render( int x, int y, int w, int h )
{
	const int nAlpha = GetFadeAlpha();
	if ( m_nLastAlpha == 0 && nAlpha > 0 )
		m_bUpdateView = true;
	m_nLastAlpha = nAlpha;

	if ( nAlpha == 0 || !IsEnabled() )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = w;
	srcRect.height = h;
	if ( m_bUpdateView )
	{
		pRenderContext->CopyRenderTargetToTextureEx( m_DrunkTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	byte overlayColor[4] = { 215, 255, 200, static_cast< byte >( nAlpha ) };
	// The original client calls UsesSRGBCorrectBlending here.  Border-color
	// support is unrelated and can select the wrong alpha path on DX9.
	if ( g_pMaterialSystemHardwareConfig &&
		g_pMaterialSystemHardwareConfig->UsesSRGBCorrectBlending() )
	{
		overlayColor[3] = static_cast< byte >( overlayColor[3] * 0.7f );
	}

	const float flTime = gpGlobals->curtime;
	const float flX = -2.0f * fabsf( cosf( flTime ) * cosf( flTime * 6.0f ) );
	const float flY = 2.0f * cosf( flTime ) * cosf( flTime * 5.0f );
	const float flScale = 0.03f +
		0.01f * cosf( flTime * 2.0f ) * cosf( flTime * 0.5f );
	const float flUOffset = ( m_DrunkTexture->GetActualWidth() - 1 ) * flScale * 0.5f;
	const float flVOffset = ( m_DrunkTexture->GetActualHeight() - 1 ) * flScale * 0.5f;
	const float flU1 = flUOffset;
	const float flU2 = ( m_DrunkTexture->GetActualWidth() - 1 ) - flUOffset;
	const float flV1 = flVOffset;
	const float flV2 = ( m_DrunkTexture->GetActualHeight() - 1 ) - flVOffset;

	pRenderContext->DrawScreenSpaceRectangle(
		m_DrunkMaterial,
		static_cast< int >( flX ), static_cast< int >( flY ), w, h,
		flU1, flV1, flU2, flV2,
		m_DrunkTexture->GetActualWidth(), m_DrunkTexture->GetActualHeight() );
	render->ViewDrawFade( overlayColor, m_DrunkMaterial );
	pRenderContext->CopyRenderTargetToTextureEx( m_DrunkTexture, 0, &srcRect, NULL );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

CStunEffect::CStunEffect()
	: m_flDuration( 0.0f )
	, m_flFinishTime( 0.0f )
	, m_bUpdateView( true )
{
}

ADD_SCREENSPACE_EFFECT( CStunEffect, episodic_stun );

void CStunEffect::Init( void )
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", FOF_STUN_TEXTURE );
	m_StunMaterial.Init( "__stuneffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( FOF_STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CStunEffect::Shutdown( void )
{
	m_StunMaterial.Shutdown();
	m_StunTexture.Shutdown();
}

void CStunEffect::SetParameters( KeyValues *pParams )
{
	if ( pParams->FindKey( "duration" ) )
	{
		m_flDuration = pParams->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
		m_bUpdateView = true;
	}
}

void CStunEffect::Enable( bool bEnable )
{
	NOTE_UNUSED( bEnable );
}

bool CStunEffect::IsEnabled( void )
{
	return true;
}

void CStunEffect::Render( int x, int y, int w, int h )
{
	if ( m_flFinishTime < gpGlobals->curtime || m_flDuration <= 0.0f )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	Rect_t srcRect = { x, y, w, h };
	if ( m_bUpdateView )
	{
		pRenderContext->CopyRenderTargetToTextureEx(
			m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	const float flEffectPercent =
		( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;
	const float flViewOffset = flEffectPercent * 32.0f *
		cosf( gpGlobals->curtime * 40.0f ) *
		sinf( gpGlobals->curtime * 17.0f );
	const float flTextureX = x + flViewOffset;

	if ( g_pMaterialSystemHardwareConfig &&
		g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
	{
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
		{
			m_StunMaterial->ColorModulate( 1.0f, 1.0f, 1.0f );
		}
		else
		{
			const float flToneMap =
				pRenderContext->GetToneMappingScaleLinear().x;
			const float flUnToneMap = powf(
				1.0f / MAX( flToneMap, 0.0001f ), 1.0f / 2.2f );
			m_StunMaterial->ColorModulate(
				flUnToneMap, flUnToneMap, flUnToneMap );
		}

		m_StunMaterial->AlphaModulate( clamp(
			( 150.0f / 255.0f ) * flEffectPercent, 0.0f, 1.0f ) );
		pRenderContext->DrawScreenSpaceRectangle(
			m_StunMaterial, 0, 0, w, h,
			flTextureX, 0.0f,
			( m_StunTexture->GetActualWidth() - 1 ) + flTextureX,
			m_StunTexture->GetActualHeight() - 1,
			m_StunTexture->GetActualWidth(),
			m_StunTexture->GetActualHeight() );
	}

	pRenderContext->CopyRenderTargetToTextureEx(
		m_StunTexture, 0, &srcRect, NULL );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

CEP1IntroEffect::CEP1IntroEffect()
	: m_flDuration( 0.0f )
	, m_flFinishTime( 0.0f )
	, m_bUpdateView( true )
	, m_bEnabled( false )
	, m_bFadeOut( false )
{
}

ADD_SCREENSPACE_EFFECT( CEP1IntroEffect, episodic_intro );

void CEP1IntroEffect::Init( void )
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", FOF_STUN_TEXTURE );
	m_StunMaterial.Init(
		"__ep1introeffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( FOF_STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CEP1IntroEffect::Shutdown( void )
{
	m_StunMaterial.Shutdown();
	m_StunTexture.Shutdown();
}

void CEP1IntroEffect::SetParameters( KeyValues *pParams )
{
	if ( pParams->FindKey( "duration" ) )
	{
		m_flDuration = pParams->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
	}
	if ( pParams->FindKey( "fadeout" ) )
		m_bFadeOut = pParams->GetInt( "fadeout" ) == 1;
}

void CEP1IntroEffect::Enable( bool bEnable )
{
	m_bEnabled = bEnable;
}

bool CEP1IntroEffect::IsEnabled( void )
{
	return m_bEnabled;
}

unsigned char CEP1IntroEffect::GetFadeAlpha( void ) const
{
	float flEffectPercent = m_flDuration == 0.0f
		? 0.0f
		: ( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;
	flEffectPercent = clamp( flEffectPercent, 0.0f, 1.0f );

	const bool bHDR = g_pMaterialSystemHardwareConfig &&
		g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
	if ( m_bFadeOut )
	{
		const float flMaxAlpha = bHDR ? 50.0f : 64.0f;
		return static_cast< unsigned char >(
			clamp( flMaxAlpha * flEffectPercent, 0.0f, flMaxAlpha ) );
	}

	const float flMaxAlpha = bHDR ? 64.0f : 128.0f;
	const float flMinAlpha = bHDR ? 50.0f : 64.0f;
	return static_cast< unsigned char >( clamp(
		flMaxAlpha * flEffectPercent, flMinAlpha, flMaxAlpha ) );
}

void CEP1IntroEffect::Render( int x, int y, int w, int h )
{
	if ( m_flFinishTime == 0.0f || !IsEnabled() )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	Rect_t srcRect = { x, y, w, h };
	if ( m_bUpdateView )
	{
		pRenderContext->CopyRenderTargetToTextureEx(
			m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	byte overlayColor[4] = { 255, 255, 255, GetFadeAlpha() };
	if ( g_pMaterialSystemHardwareConfig &&
		g_pMaterialSystemHardwareConfig->UsesSRGBCorrectBlending() )
	{
		overlayColor[3] = static_cast< byte >( overlayColor[3] * 0.7f );
	}
	if ( m_bFadeOut && overlayColor[3] == 0 )
	{
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "episodic_intro" );
		m_bUpdateView = true;
	}

	const float flTime = gpGlobals->curtime;
	const float flX = -2.0f * fabsf( cosf( flTime ) * cosf( flTime * 6.0f ) );
	const float flY = 2.0f * cosf( flTime ) * cosf( flTime * 5.0f );
	const float flScale = 0.02f +
		0.01f * cosf( flTime * 2.0f ) * cosf( flTime * 0.5f );
	const float flUOffset =
		( m_StunTexture->GetActualWidth() - 1 ) * flScale * 0.5f;
	const float flVOffset =
		( m_StunTexture->GetActualHeight() - 1 ) * flScale * 0.5f;

	pRenderContext->DrawScreenSpaceRectangle(
		m_StunMaterial,
		static_cast< int >( flX ), static_cast< int >( flY ), w, h,
		flUOffset, flVOffset,
		( m_StunTexture->GetActualWidth() - 1 ) - flUOffset,
		( m_StunTexture->GetActualHeight() - 1 ) - flVOffset,
		m_StunTexture->GetActualWidth(),
		m_StunTexture->GetActualHeight() );
	render->ViewDrawFade( overlayColor, m_StunMaterial );
	pRenderContext->CopyRenderTargetToTextureEx(
		m_StunTexture, 0, &srcRect, NULL );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

CEP2StunEffect::CEP2StunEffect()
	: m_flDuration( 0.0f )
	, m_flFinishTime( 0.0f )
	, m_bUpdateView( true )
	, m_bEnabled( false )
	, m_bFadeOut( false )
{
}

ADD_SCREENSPACE_EFFECT( CEP2StunEffect, ep2_groggy );

void CEP2StunEffect::Init( void )
{
	m_flDuration = 0.0f;
	m_flFinishTime = 0.0f;
	m_bUpdateView = true;
	m_bFadeOut = false;

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", FOF_STUN_TEXTURE );
	m_StunMaterial.Init( "__ep2stuneffect", TEXTURE_GROUP_CLIENT_EFFECTS, pVMTKeyValues );
	m_StunTexture.Init( FOF_STUN_TEXTURE, TEXTURE_GROUP_CLIENT_EFFECTS );
}

void CEP2StunEffect::Shutdown( void )
{
	m_StunMaterial.Shutdown();
	m_StunTexture.Shutdown();
}

void CEP2StunEffect::SetParameters( KeyValues *pParams )
{
	if ( pParams->FindKey( "duration" ) )
	{
		m_flDuration = pParams->GetFloat( "duration" );
		m_flFinishTime = gpGlobals->curtime + m_flDuration;
	}

	if ( pParams->FindKey( "fadeout" ) )
		m_bFadeOut = pParams->GetInt( "fadeout" ) == 1;
}

void CEP2StunEffect::Enable( bool bEnable )
{
	m_bEnabled = bEnable;
}

bool CEP2StunEffect::IsEnabled( void )
{
	return m_bEnabled;
}

unsigned char CEP2StunEffect::GetFadeAlpha( void ) const
{
	// The original alpha routine depends only on duration,
	// finish time, fadeout, and HDR mode.  Target/local-player filtering is
	// performed by the entity-message receiver before this effect is enabled.
	float flEffectPercent = m_flDuration == 0.0f
		? 0.0f
		: ( m_flFinishTime - gpGlobals->curtime ) / m_flDuration;
	flEffectPercent = clamp( flEffectPercent, 0.0f, 1.0f );

	if ( m_bFadeOut )
	{
		const float flMaxAlpha = g_pMaterialSystemHardwareConfig &&
			g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE
			? 50.0f
			: 64.0f;
		return static_cast< unsigned char >(
			clamp( flMaxAlpha * flEffectPercent, 0.0f, flMaxAlpha ) );
	}

	return static_cast< unsigned char >(
		clamp( 164.0f * flEffectPercent, 128.0f, 164.0f ) );
}

void CEP2StunEffect::Render( int x, int y, int w, int h )
{
	if ( m_flFinishTime == 0.0f || !IsEnabled() )
		return;

	const unsigned char nFadeAlpha = GetFadeAlpha();
	if ( m_bFadeOut && nFadeAlpha == 0 )
	{
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "ep2_groggy" );
		m_bUpdateView = true;
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	Rect_t srcRect = { x, y, w, h };
	if ( m_bUpdateView )
	{
		pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );
		m_bUpdateView = false;
	}

	// The original client seeds this RGBA value with 0x00B9B9FF
	// before replacing alpha.
	byte overlayColor[4] = { 255, 185, 185, nFadeAlpha };
	const float flTime = gpGlobals->curtime;
	const float flX = 4.0f * cosf( flTime ) * cosf( flTime * 6.0f );
	const float flY = 2.0f * cosf( flTime ) * cosf( flTime * 5.0f );
	const float flScale = 0.2f + 0.005f * sinf( flTime * 4.0f ) +
		0.01f * cosf( flTime * 2.0f ) * cosf( flTime * 0.5f );
	const float flUOffset = ( m_StunTexture->GetActualWidth() - 1 ) * flScale * 0.5f;
	const float flVOffset = ( m_StunTexture->GetActualHeight() - 1 ) * flScale * 0.5f;

	pRenderContext->DrawScreenSpaceRectangle(
		m_StunMaterial,
		static_cast< int >( flX ), static_cast< int >( flY ), w, h,
		flUOffset,
		flVOffset,
		( m_StunTexture->GetActualWidth() - 1 ) - flUOffset,
		( m_StunTexture->GetActualHeight() - 1 ) - flVOffset,
		m_StunTexture->GetActualWidth(),
		m_StunTexture->GetActualHeight() );
	render->ViewDrawFade( overlayColor, m_StunMaterial );
	pRenderContext->CopyRenderTargetToTextureEx( m_StunTexture, 0, &srcRect, NULL );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}
