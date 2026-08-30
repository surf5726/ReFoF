//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTMODE_HLNORMAL_H )
#define CLIENTMODE_HLNORMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>

class CHudViewport;

namespace vgui
{
	typedef unsigned long HScheme;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class ClientModeHL2MPNormal : public ClientModeShared
{
public:
	DECLARE_CLASS( ClientModeHL2MPNormal, ClientModeShared );

	ClientModeHL2MPNormal();
	~ClientModeHL2MPNormal();

	virtual void	Init();
	virtual void	LevelInit( const char *newmap );
	virtual bool	DoPostScreenSpaceEffects( const CViewSetup *pSetup );
	virtual int		GetDeathMessageStartHeight( void );
};

extern IClientMode *GetClientModeNormal();
extern vgui::HScheme g_hVGuiCombineScheme;

extern ClientModeHL2MPNormal* GetClientModeHL2MPNormal();

#endif // CLIENTMODE_HLNORMAL_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fistful of Frags viewport and custom viewport panels.
//
//=============================================================================//

#ifndef FOF_VIEWPORT_H
#define FOF_VIEWPORT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseviewport.h"

class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual IViewPortPanel *CreatePanelByName( const char *szPanelName );
};

const char *FoFResolveViewportPanelName( const char *pszPanelName );
void FoFOnViewportPanelMessage( const char *pszPanelName, bool bShow );

#endif // FOF_VIEWPORT_H
