//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef FOF_SCOREBOARD_H
#define FOF_SCOREBOARD_H
#ifdef _WIN32
#pragma once
#endif

#include <clientscoreboarddialog.h>

namespace vgui
{
	class ImagePanel;
	class Label;
}

//-----------------------------------------------------------------------------
// Purpose: Game ScoreBoard
//-----------------------------------------------------------------------------
class CFoFScoreboardDialog : public CClientScoreBoardDialog
{
private:
	DECLARE_CLASS_SIMPLE(CFoFScoreboardDialog, CClientScoreBoardDialog);

public:
	CFoFScoreboardDialog(IViewPort *pViewPort);
	~CFoFScoreboardDialog();

	virtual void Reset();
	virtual void Update();
	virtual void FireGameEvent( IGameEvent *event );
	virtual void ShowPanel( bool bShow );

protected:
	// scoreboard overrides
	virtual void InitScoreboardSections();
	virtual void UpdateTeamInfo();
	virtual bool GetPlayerScoreInfo(int playerIndex, KeyValues *outPlayerInfo);
	virtual void UpdatePlayerInfo();

	// vgui overrides
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	enum FoFScoreboardBadgeImage_t
	{
		FOF_SCOREBOARD_ICON = 0,
		FOF_SCOREBOARD_BADGE_BOT,
		FOF_SCOREBOARD_BADGE_DEV,
		FOF_SCOREBOARD_BADGE_CONTRIB,
		FOF_SCOREBOARD_BADGE_SPECIAL,
		FOF_SCOREBOARD_BADGE_L1,
		FOF_SCOREBOARD_BADGE_L2,
		FOF_SCOREBOARD_BADGE_L3,
		FOF_SCOREBOARD_BADGE_L4,
		FOF_SCOREBOARD_BADGE_ELM_TOP10,
		FOF_SCOREBOARD_BADGE_ELM_TOP50,
		FOF_SCOREBOARD_BADGE_ELM_TOP100,
		FOF_SCOREBOARD_BADGE_FIRST,
		FOF_SCOREBOARD_BADGE_SECOND,
		FOF_SCOREBOARD_BADGE_THIRD,
		FOF_SCOREBOARD_BADGE_ELM_TOP1,
		FOF_SCOREBOARD_BADGE_COUNT
	};

	virtual void AddHeader(); // add the start header of the scoreboard
	virtual void AddSection(int teamType, int teamNumber); // add a new section header for a team

	static bool FoFPlayerSortFunc( vgui::SectionedListPanel *list,
		int itemID1, int itemID2 );

	void AddFoFListSection( vgui::SectionedListPanel *list, int sectionID,
		bool includeCash );
	int GetFoFDeathmatchListWide();
	void ConfigureFoFListSections();
	void FitFoFListLineSpacing();
	void UpdateFoFModeVisibility();
	void LayoutFoFMuteHint();
	void UpdateFoFTimeLeft();
	vgui::SectionedListPanel *GetFoFListForTeam( int teamNumber,
		int &sectionID, const char *&listName ) const;
	int GetFoFMode() const;
	bool IsFoFRankedTeamMode() const;
	int GetBreakBadSectionFromTeamNumber( int teamNumber ) const;
	bool ShouldShowPlayer( int playerIndex ) const;
	Color GetFoFPlayerColor( int playerIndex ) const;
	int GetFoFBadgeImageIndex( int playerIndex ) const;

	int GetSectionFromTeamNumber( int teamNumber );

	vgui::SectionedListPanel	*m_pPlayerListDM;
	vgui::SectionedListPanel	*m_pPlayerListR;
	vgui::SectionedListPanel	*m_pPlayerListC;
	vgui::ImagePanel			*m_pBackground;
	vgui::ImagePanel			*m_pMuteButtonLeft;
	vgui::ImagePanel			*m_pMuteButtonRight;
	vgui::Label				*m_pTimeLeft;
	int						 m_iFoFPanelWide;
	int						 m_iFoFPanelTall;
	int						 m_iFoFBadgeImages[FOF_SCOREBOARD_BADGE_COUNT];
	char					 m_szFoFServerName[256];
};

enum
{
	FOF_FIRST_PLAYING_TEAM = 2,
	FOF_LAST_PLAYING_TEAM = 5
};

#endif // FOF_SCOREBOARD_H
