#ifndef FOF_LAUNCHER_SERVERS_H
#define FOF_LAUNCHER_SERVERS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/utlstring.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>

struct FoFServerEntry
{
	CUtlString name;
	CUtlString map;
	CUtlString description;
	CUtlString address;
	uint32 ip;
	uint16 port;
	int ping;
	int players;
	int maxPlayers;
	int bots;
	int mode;
	int group;
	bool password;
	bool secure;
	bool friendServer;
};

struct FoFLauncherServerRule
{
	CUtlString name;
	CUtlString ip;
	int port;
	int mode;
};

class CFoFServerButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFServerButton, vgui::Button );

public:
	CFoFServerButton(
		vgui::Panel *pParent,
		FoFServerEntry *pEntry,
		int index );
	virtual ~CFoFServerButton();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();

private:
	FoFServerEntry *m_pEntry;
	int m_iMapTexture;
	int m_iLockTexture;
	int m_iSecureTexture;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hStatusFont;
};

#endif // FOF_LAUNCHER_SERVERS_H
