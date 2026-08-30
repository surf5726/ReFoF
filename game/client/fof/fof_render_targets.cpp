#include "cbase.h"
#include "fof/fof_render_targets.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

#include "tier0/memdbgon.h"

void CFoFClientRenderTargets::InitClientRenderTargets(
	IMaterialSystem *pMaterialSystem,
	IMaterialSystemHardwareConfig *pHardwareConfig )
{
	// The Sharps scope material samples this fixed-size HDR render target.
	m_ScopeTexture.Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Scope",
		1024,
		1024,
		RT_SIZE_OFFSCREEN,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR ) );

	CBaseClientRenderTargets::InitClientRenderTargets(
		pMaterialSystem,
		pHardwareConfig,
		1024,
		256 );
}

void CFoFClientRenderTargets::ShutdownClientRenderTargets( void )
{
	m_ScopeTexture.Shutdown();
	CBaseClientRenderTargets::ShutdownClientRenderTargets();
}

static CFoFClientRenderTargets g_FoFClientRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(
	CFoFClientRenderTargets,
	IClientRenderTargets,
	CLIENTRENDERTARGETS_INTERFACE_VERSION,
	g_FoFClientRenderTargets );
