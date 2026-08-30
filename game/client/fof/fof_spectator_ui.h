#ifndef FOF_SPECTATOR_UI_H
#define FOF_SPECTATOR_UI_H
#ifdef _WIN32
#pragma once
#endif

#include <spectatorgui.h>

class CFoFSpectatorGUI : public CSpectatorGUI
{
private:
	DECLARE_CLASS_SIMPLE( CFoFSpectatorGUI, CSpectatorGUI );

public:
	CFoFSpectatorGUI( IViewPort *pViewPort );

	virtual void Update( void );
	virtual bool NeedsUpdate( void );
	virtual void PerformLayout();

protected:
	int m_nLastSpecMode;
	CBaseEntity *m_nLastSpecTarget;
	int m_nLastSpecHealth;
	bool m_bLastSpecTargetAlive;
};

class CFoFSpectatorMenu : public CSpectatorMenu
{
private:
	DECLARE_CLASS_SIMPLE( CFoFSpectatorMenu, CSpectatorMenu );

public:
	CFoFSpectatorMenu( IViewPort *pViewPort );
	virtual void PerformLayout();

private:
	virtual void OnCommand( const char *command );
};

#endif // FOF_SPECTATOR_UI_H
