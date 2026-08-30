//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef FOF_MOTD_H
#define FOF_MOTD_H
#ifdef _WIN32
#pragma once
#endif

#include "vguitextwindow.h"

class CBitmapButton;
namespace vgui
{
	class Label;
}

// Presents the server's InfoPanel "motd" entry before the FoF tutorial/team
// flow on every map, matching the shipped client's changelevel behavior.
bool FoFIsListenServerSession();
bool FoFIsLocalCourseSession();
bool FoFShowFirstMotd();
bool FoFFirstMotdReadyForNextPanel();
void FoFResetFirstMotdForLevel();

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------

class CFoFMotd : public CTextWindow
{
private:
	DECLARE_CLASS_SIMPLE( CFoFMotd, CTextWindow );

public:
	CFoFMotd(IViewPort *pViewPort);
	virtual ~CFoFMotd();

	virtual void Update();
	virtual void ShowURL( const char *URL,
		bool bAllowUserToDisable = true );
	virtual void SetVisible(bool state);
	virtual void ShowPanel( bool bShow );
	virtual void OnThink();
	virtual void OnKeyCodePressed(vgui::KeyCode code);

protected:
	virtual void OnCommand( const char *command );
	void LayoutFoFMotd();
	void BeginFoFMotdCountdown();
	void EndFoFMotdCountdown();

	ButtonCode_t m_iScoreBoardKey;
	CBitmapButton *m_pFoFOK;
	CBitmapButton *m_pFoFStayBackground;
	vgui::Label *m_pFoFMotdCountdown;
	float m_flFoFMotdCountdown;
	int m_iFoFMotdBackgroundTexture;
	int m_iFoFLastContentType;
	int m_iFoFLayoutParentWide;
	int m_iFoFLayoutParentTall;
	int m_iFoFLayoutScreenWide;
	int m_iFoFLayoutScreenTall;
	char m_szFoFLastMessage[2048];

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};

#endif // FOF_MOTD_H
