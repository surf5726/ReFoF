#ifndef FOF_ELIMINATION_HUD_H
#define FOF_ELIMINATION_HUD_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "const.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class CAvatarImage;
class CHudTexture;

bool FoFIsEliminationPlayerOut( int playerIndex );

namespace vgui
{
	class CircularProgressBar;
	class ImagePanel;
	class IScheme;
}

class CHudFoFEliminationStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFEliminationStatus, vgui::Panel );

public:
	CHudFoFEliminationStatus( const char *elementName );
	~CHudFoFEliminationStatus();

	virtual void Init();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();
	virtual void Paint();
	virtual void FireGameEvent( IGameEvent *event );

private:
	void ClearClockState();
	void ClearEliminatedPlayers();
	void LayoutForScreen();
	void UpdateClock();
	void DrawTeamGroup(
		int team,
		bool drawOnLeft,
		int row,
		const int *players,
		int playerCount );
	void DrawPlayerSlot( int playerIndex, int team, int x, int y, int size );
	void DrawEnemyRevealArrows();
	void UpdateAvatar( int playerIndex );

	vgui::CircularProgressBar *m_pExtraTimeBar;
	vgui::ImagePanel *m_pClockFace;
	vgui::CircularProgressBar *m_pRoundTimeBar;
	CAvatarImage *m_pAvatars[MAX_PLAYERS + 1];
	unsigned int m_nAvatarFriendsId[MAX_PLAYERS + 1];
	CHudTexture *m_pLeftArrow;
	CHudTexture *m_pRightArrow;
	int m_iBotTexture;
	int m_iCrossTexture;
	float m_flExtraTimeEnd;
	float m_flEnemyArrowHideAt;
	float m_flRoundEndTime;
	float m_flRoundDuration;
	bool m_bProgressVisible;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

class CHudFoFRoundEndMessage : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFRoundEndMessage, vgui::Panel );

public:
	CHudFoFRoundEndMessage( const char *elementName );

	virtual void Init();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();
	virtual void Paint();
	virtual void FireGameEvent( IGameEvent *event );

private:
	void ClearMessage();
	void LayoutForScreen();
	void ShowRoundResult( IGameEvent *event );
	void DrawCenteredText(
		vgui::HFont font,
		const wchar_t *text,
		int y,
		const Color &color );

	vgui::HFont m_hWinnerFont;
	vgui::HFont m_hMvpFont;
	wchar_t m_wszWinner[256];
	wchar_t m_wszMvp[512];
	Color m_WinnerColor;
	Color m_MvpColor;
	float m_flHideAt;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

#endif // FOF_ELIMINATION_HUD_H
