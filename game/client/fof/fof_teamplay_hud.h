#ifndef FOF_TEAMPLAY_HUD_H
#define FOF_TEAMPLAY_HUD_H
#ifdef _WIN32
#pragma once
#endif

#include "const.h"
#include "hudelement.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class CAvatarImage;
class CHudTexture;

namespace vgui
{
	class CircularProgressBar;
	class ImagePanel;
	class IScheme;
}

class CHudFoFTeamplayStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFTeamplayStatus, vgui::Panel );

public:
	CHudFoFTeamplayStatus( const char *elementName );
	~CHudFoFTeamplayStatus();

	virtual void Init();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();
	virtual void Paint();
	virtual void FireGameEvent( IGameEvent *event );

private:
	void ClearState();
	void LayoutForScreen();
	void UpdateTimerControls();
	void UpdateAvatar( int playerIndex );
	void DrawTeamRoster(
		int team,
		bool rightAligned,
		int anchorX,
		int y,
		const int *players,
		int playerCount );
	void DrawPlayerIcon( int playerIndex, int x, int y, int size );
	void DrawTeamObjective( int team, bool drawOnLeft );

	vgui::CircularProgressBar *m_pExtraTimeBar;
	vgui::ImagePanel *m_pClockFace;
	vgui::CircularProgressBar *m_pRoundTimeBar;
	CAvatarImage *m_pAvatars[MAX_PLAYERS + 1];
	unsigned int m_nAvatarFriendsId[MAX_PLAYERS + 1];
	CHudTexture *m_pObjectiveIcons[6];
	vgui::HFont m_hScoreFont;
	vgui::HFont m_hTotalFont;
	vgui::HFont m_hPlaceholderFont;
	int m_iBotTexture;
	int m_iObjective[6];
	int m_iReward[6];
	float m_flCapturingUntil[6];
	float m_flExtraTimeEnd;
	float m_flRoundTimeStart;
	float m_flRoundTimeEnd;
	bool m_bDialogVisible;
	bool m_bClockVisible;
	bool m_bExtraTimerVisible;
	bool m_bRoundTimerVisible;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

#endif // FOF_TEAMPLAY_HUD_H
