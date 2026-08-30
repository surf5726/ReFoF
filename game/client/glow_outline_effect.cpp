//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Functionality to render a glowing outline around client renderable objects.
//
//===============================================================================

#include "cbase.h"
#include "glow_outline_effect.h"
#include "model_types.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/itexture.h"
#include "view_shared.h"
#include "fof/fof_combat_effects.h"

#define FULL_FRAME_TEXTURE "_rt_FullFrameFB"

#ifdef GLOWS_ENABLE

ConVar glow_outline_effect_enable( "glow_outline_effect_enable", "1", FCVAR_ARCHIVE, "Enable entity outline glow effects." );
ConVar glow_outline_effect_width( "glow_outline_width", "10.0f", FCVAR_CHEAT, "Width of glow outline effect in screen space." );

CGlowObjectManager g_GlowObjectManager;

struct ShaderStencilState_t
{
	bool m_bEnable;
	StencilOperation_t m_FailOp;
	StencilOperation_t m_ZFailOp;
	StencilOperation_t m_PassOp;
	StencilComparisonFunction_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = STENCILOPERATION_KEEP;
		m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;
	}

	void SetStencilState( CMatRenderContextPtr &pRenderContext )
	{
		pRenderContext->SetStencilEnable( m_bEnable );
		pRenderContext->SetStencilFailOperation( m_FailOp );
		pRenderContext->SetStencilZFailOperation( m_ZFailOp );
		pRenderContext->SetStencilPassOperation( m_PassOp );
		pRenderContext->SetStencilCompareFunction( m_CompareFunc );
		pRenderContext->SetStencilReferenceValue( m_nReferenceValue );
		pRenderContext->SetStencilTestMask( m_nTestMask );
		pRenderContext->SetStencilWriteMask( m_nWriteMask );
	}
};

void CGlowObjectManager::RenderGlowEffects(
	const CViewSetup *pSetup, int nSplitScreenSlot )
{
	if ( !g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_0() ||
		!glow_outline_effect_enable.GetBool() )
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( materials );
	int nX, nY, nWidth, nHeight;
	pRenderContext->GetViewport( nX, nY, nWidth, nHeight );

	PIXEvent pixEvent( pRenderContext, "EntityGlowEffects" );
	ApplyEntityGlowEffects( pSetup, nSplitScreenSlot, pRenderContext,
		glow_outline_effect_width.GetFloat(), nX, nY, nWidth, nHeight );
}

void CGlowObjectManager::RenderGlowModelsWhenUnoccluded(
	int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext )
{
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_nReferenceValue = 1;
	stencilState.SetStencilState( pRenderContext );

	pRenderContext->OverrideDepthEnable( true, false );
	render->SetBlend( 1.0f );

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		GlowObjectDefinition_t &glow = m_GlowObjectDefinitions[i];
		if ( glow.IsUnused() || !glow.ShouldDraw( nSplitScreenSlot ) ||
			glow.m_bRenderWhenOccluded ||
			!glow.m_bRenderWhenUnoccluded )
		{
			continue;
		}

		Vector vGlowColor = glow.m_vGlowColor * glow.m_flGlowAlpha;
		render->SetColorModulation( vGlowColor.Base() );
		glow.DrawModel();
	}
}

void CGlowObjectManager::RenderGlowModelsWhenOccluded(
	int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext )
{
	pRenderContext->OverrideDepthEnable( true, false );

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_nReferenceValue = 2;
	stencilState.m_nTestMask = 2;
	stencilState.m_nWriteMask = 2;
	stencilState.SetStencilState( pRenderContext );

	render->SetBlend( 0.0f );
	pRenderContext->OverrideAlphaWriteEnable( true, false );
	pRenderContext->OverrideColorWriteEnable( true, false );

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		GlowObjectDefinition_t &glow = m_GlowObjectDefinitions[i];
		if ( glow.IsUnused() || !glow.ShouldDraw( nSplitScreenSlot ) ||
			!glow.m_bRenderWhenOccluded ||
			glow.m_bRenderWhenUnoccluded )
		{
			continue;
		}

		glow.DrawModel();
	}

	pRenderContext->OverrideAlphaWriteEnable( false, true );
	pRenderContext->OverrideColorWriteEnable( false, true );
	pRenderContext->OverrideDepthEnable( false, false );

	stencilState.m_FailOp = STENCILOPERATION_REPLACE;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_NOTEQUAL;
	stencilState.m_nReferenceValue = 3;
	stencilState.m_nTestMask = 2;
	stencilState.m_nWriteMask = 1;
	stencilState.SetStencilState( pRenderContext );
	render->SetBlend( 1.0f );

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		GlowObjectDefinition_t &glow = m_GlowObjectDefinitions[i];
		if ( glow.IsUnused() || !glow.ShouldDraw( nSplitScreenSlot ) ||
			!glow.m_bRenderWhenOccluded ||
			glow.m_bRenderWhenUnoccluded )
		{
			continue;
		}

		Vector vGlowColor = glow.m_vGlowColor * glow.m_flGlowAlpha;
		render->SetColorModulation( vGlowColor.Base() );
		glow.DrawModel();
	}
}

void CGlowObjectManager::RenderGlowModelsWhenOccludedAndUnoccluded(
	int nSplitScreenSlot, CMatRenderContextPtr &pRenderContext )
{
	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_REPLACE;
	stencilState.m_PassOp = STENCILOPERATION_REPLACE;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_ALWAYS;
	stencilState.m_nReferenceValue = 1;
	stencilState.SetStencilState( pRenderContext );

	pRenderContext->OverrideDepthEnable( false, false );
	render->SetBlend( 1.0f );

	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		GlowObjectDefinition_t &glow = m_GlowObjectDefinitions[i];
		if ( glow.IsUnused() || !glow.ShouldDraw( nSplitScreenSlot ) ||
			FoFShouldSuppressPlayerGlow( glow.m_hEntity.Get() ) ||
			!glow.m_bRenderWhenOccluded ||
			!glow.m_bRenderWhenUnoccluded )
		{
			continue;
		}

		Vector vGlowColor = glow.m_vGlowColor * glow.m_flGlowAlpha;
		render->SetColorModulation( vGlowColor.Base() );
		glow.DrawModel();
	}
}

void CGlowObjectManager::ApplyEntityGlowEffects(
	const CViewSetup *pSetup, int nSplitScreenSlot,
	CMatRenderContextPtr &pRenderContext, float flBloomScale,
	int x, int y, int w, int h )
{
	int iNumGlowObjects = 0;
	for ( int i = 0; i < m_GlowObjectDefinitions.Count(); ++i )
	{
		GlowObjectDefinition_t &glow = m_GlowObjectDefinitions[i];
		if ( !glow.IsUnused() && glow.ShouldDraw( nSplitScreenSlot ) )
			++iNumGlowObjects;
	}

	if ( iNumGlowObjects <= 0 )
		return;

	ITexture *pRtFullFrame = materials->FindTexture(
		FULL_FRAME_TEXTURE, TEXTURE_GROUP_RENDER_TARGET );
	ITexture *pRtFullFrame1 = materials->FindTexture(
		"_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );

	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTargetEx( 0, NULL );
	pRenderContext->CopyRenderTargetToTexture( pRtFullFrame1 );
	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false, true );

	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );
	const float flOrigBlend = render->GetBlend();

	IMaterial *pMatGlowColor = materials->FindMaterial(
		"dev/glow_color", TEXTURE_GROUP_OTHER, true );
	g_pStudioRender->ForcedMaterialOverride( pMatGlowColor );
	pRenderContext->OverrideAlphaWriteEnable( true, true );
	pRenderContext->OverrideColorWriteEnable( true, true );

	RenderGlowModelsWhenUnoccluded( nSplitScreenSlot, pRenderContext );
	RenderGlowModelsWhenOccluded( nSplitScreenSlot, pRenderContext );
	RenderGlowModelsWhenOccludedAndUnoccluded(
		nSplitScreenSlot, pRenderContext );

	g_pStudioRender->ForcedMaterialOverride( NULL );
	render->SetColorModulation( vOrigColor.Base() );
	render->SetBlend( flOrigBlend );
	pRenderContext->OverrideDepthEnable( false, false );
	pRenderContext->CopyRenderTargetToTexture( pRtFullFrame );

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	stencilStateDisable.SetStencilState( pRenderContext );

	IMaterial *pMatRestoreFrame = materials->FindMaterial(
		"debug/debugfbtexture1", TEXTURE_GROUP_RENDER_TARGET, true );
	pMatRestoreFrame->IncrementReferenceCount();
	pRenderContext->Bind( pMatRestoreFrame );

	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport(
		nViewportX, nViewportY, nViewportWidth, nViewportHeight );
	pRenderContext->OverrideDepthEnable( true, false );
	pRenderContext->DrawScreenSpaceRectangle( pMatRestoreFrame,
		0, 0, nViewportWidth, nViewportHeight,
		0.0f, 0.0f,
		static_cast< float >( nViewportWidth - 1 ),
		static_cast< float >( nViewportHeight - 1 ),
		pRtFullFrame1->GetActualWidth(),
		pRtFullFrame1->GetActualHeight() );
	pRenderContext->OverrideDepthEnable( false, false );
	pMatRestoreFrame->DecrementReferenceCount();

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_FailOp = STENCILOPERATION_KEEP;
	stencilState.m_ZFailOp = STENCILOPERATION_KEEP;
	stencilState.m_PassOp = STENCILOPERATION_KEEP;
	stencilState.m_CompareFunc = STENCILCOMPARISONFUNCTION_NOTEQUAL;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_nTestMask = 1;
	stencilState.m_nWriteMask = 0;
	stencilState.SetStencilState( pRenderContext );

	ITexture *pRtQuarterSize1 = materials->FindTexture(
		"_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
	IMaterial *pMatHaloAddToScreen = materials->FindMaterial(
		"dev/halo_add_to_screen", TEXTURE_GROUP_OTHER, true );
	pRenderContext->OverrideAlphaWriteEnable( true, true );
	pRenderContext->OverrideColorWriteEnable( true, true );
	pRenderContext->DrawScreenSpaceRectangle( pMatHaloAddToScreen,
		0, 0, nViewportWidth, nViewportHeight,
		0.0f, -0.5f,
		static_cast< float >( pSetup->width / 4 - 1 ),
		static_cast< float >( pSetup->height / 4 - 1 ),
		pRtQuarterSize1->GetActualWidth(),
		pRtQuarterSize1->GetActualHeight() );

	stencilStateDisable.SetStencilState( pRenderContext );
	pRenderContext->OverrideAlphaWriteEnable( false, false );
	pRenderContext->OverrideColorWriteEnable( false, false );
	pRenderContext->OverrideDepthEnable( false, false );
	pRenderContext->PopRenderTargetAndViewport();
}

void CGlowObjectManager::GlowObjectDefinition_t::DrawModel()
{
	if ( !m_hEntity.Get() )
		return;

	m_hEntity->DrawModel( STUDIO_RENDER );
	C_BaseEntity *pAttachment = m_hEntity->FirstMoveChild();
	while ( pAttachment != NULL )
	{
		if ( !g_GlowObjectManager.HasGlowEffect( pAttachment ) &&
			pAttachment->ShouldDraw() )
		{
			pAttachment->DrawModel( STUDIO_RENDER );
		}
		pAttachment = pAttachment->NextMovePeer();
	}
}

#endif // GLOWS_ENABLE
