#ifndef FOF_RENDER_TARGETS_H
#define FOF_RENDER_TARGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclientrendertargets.h"
#include "materialsystem/MaterialSystemUtil.h"

class CFoFClientRenderTargets : public CBaseClientRenderTargets
{
public:
	virtual void InitClientRenderTargets(
		IMaterialSystem *pMaterialSystem,
		IMaterialSystemHardwareConfig *pHardwareConfig );
	virtual void ShutdownClientRenderTargets( void );

private:
	CTextureReference m_ScopeTexture;
};

#endif // FOF_RENDER_TARGETS_H
