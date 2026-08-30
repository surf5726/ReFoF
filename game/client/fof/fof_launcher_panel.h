#ifndef FOF_LAUNCHER_PANEL_H
#define FOF_LAUNCHER_PANEL_H
#ifdef _WIN32
#pragma once
#endif

void FoFCreateLauncherPanel();

#include "fof/fof_launcher_courses.h"
#include "fof/fof_launcher_ranked.h"
#include "fof/fof_launcher_servers.h"
#include "fof/fof_launcher_widgets.h"
#include "steam/steam_api.h"
#include "tier1/utlvector.h"

#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class ComboBox;
}

class CFoFWorkshopPanel;
class CFoFPersonalStatsPanel;
class KeyValues;

enum
{
	FOF_LAUNCHER_PAGE_SINGLEPLAYER = 0,
	FOF_LAUNCHER_PAGE_COOP,
	FOF_LAUNCHER_PAGE_TEAMS,
	FOF_LAUNCHER_PAGE_RANKED,
	FOF_LAUNCHER_PAGE_FRIENDS
};

enum
{
	FOF_LAUNCHER_SERVER_STOCK_SHOOTOUT = 0,
	FOF_LAUNCHER_SERVER_MODDED,
	FOF_LAUNCHER_SERVER_CUSTOM,
	FOF_LAUNCHER_SERVER_STOCK_TEAMPLAY,
	FOF_LAUNCHER_MAX_GROUPS
};

class CFoFServersPanel : public vgui::EditablePanel,
	public ISteamMatchmakingServerListResponse
{
	DECLARE_CLASS_SIMPLE( CFoFServersPanel, vgui::EditablePanel );

public:
	CFoFServersPanel();
	virtual ~CFoFServersPanel();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnTick();
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void Paint();
	virtual void PostChildPaint();
	virtual void OnCommand( const char *pszCommand );

	void OpenLauncher();
	void OpenWorkshop();
	void OpenStats();

	virtual void ServerResponded(
		HServerListRequest hRequest,
		int iServer );
	virtual void ServerFailedToRespond(
		HServerListRequest hRequest,
		int iServer );
	virtual void RefreshComplete(
		HServerListRequest hRequest,
		EMatchMakingServerResponse response );

private:
	void SelectPage( int page );
	void LoadTopBar();
	void ActivateTopItem( int index );

	void ClearCourseEntries();
	void LoadCourses();
	void ReloadCourseStats();
	bool CourseMatchesSearch( const FoFCourseEntry &entry ) const;
	void LayoutCourses();
	void LaunchCourse( int index );

	void ReleaseInternetRequest();
	void ClearServerEntries();
	void RememberFriendServer( uint32 ip, uint16 port );
	bool IsFriendServer( uint32 ip, uint16 port ) const;
	void RefreshSocialData();
	void RequestOnlinePlayerCount();
	void OnNumberOfCurrentPlayers(
		NumberOfCurrentPlayers_t *pResult,
		bool bIOFailure );
	void RequestServerRules();
	void ReleaseServerRulesRequest();
	void ClearServerRules();
	bool LoadServerRulesFromFile();
	bool LoadServerRulesFromBuffer(
		const char *pszResourceName,
		const char *pBuffer );
	bool ParseServerRules( KeyValues *pConfig );
	void OnServerRulesRequestCompleted(
		HTTPRequestCompleted_t *pResult,
		bool bIOFailure );
	bool ServerPassesRemoteRules(
		const gameserveritem_t &server ) const;
	void UpdateServerEntry( int serverIndex );
	bool ServerMatchesSearch( const FoFServerEntry &entry ) const;
	bool ServerMatchesCurrentPage(
		const FoFServerEntry &entry,
		bool includeFull ) const;
	void RecountServerTotals();
	bool ServerComesBefore( int left, int right ) const;
	void LayoutServers();
	void JoinServer( int index );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", pData );

	CFoFLauncherButton *m_pSinglePlayer;
	CFoFLauncherButton *m_pTeams;
	CFoFLauncherButton *m_pCoop;
	CFoFLauncherButton *m_pRanked;
	CFoFLauncherButton *m_pFriends;
	CFoFLauncherButton *m_pFindServers;
	CFoFBrowserWarningButton *m_pBrowserWarning;
	CFoFSearchEntry *m_pSearch;
	CFoFRefreshButton *m_pCourseRefresh;
	vgui::Button *m_pRefresh;
	vgui::ComboBox *m_pPingDrop;
	CFoFLauncherPageButton *m_pGroupPrevious[FOF_LAUNCHER_MAX_GROUPS];
	CFoFLauncherPageButton *m_pGroupNext[FOF_LAUNCHER_MAX_GROUPS];
	CFoFRankedPanel *m_pRankedPanel;
	CFoFWorkshopPanel *m_pWorkshopPanel;
	CFoFPersonalStatsPanel *m_pStatsPanel;
	HServerListRequest m_hInternetRequest;
	CCallResult< CFoFServersPanel, NumberOfCurrentPlayers_t >
		m_NumberOfCurrentPlayersCall;
	CCallResult< CFoFServersPanel, HTTPRequestCompleted_t >
		m_ServerRulesCall;
	HTTPRequestHandle m_hServerRulesRequest;
	int m_iOnlinePlayers;
	int m_iFullServers;
	int m_iFriendsPlaying;
	int m_iPingLimit;
	int m_iPage;
	bool m_bLauncherVisible;
	bool m_bServerRuleWhitelisting;
	bool m_bServerRulesRequested;
	CUtlVector<CFoFTopBarButton *> m_TopButtons;
	CUtlVector<CUtlString> m_TopCommands;
	CUtlVector<FoFCourseEntry *> m_Courses;
	CUtlVector<CFoFCourseButton *> m_CourseButtons;
	CUtlVector<FoFServerEntry *> m_Servers;
	CUtlVector<CFoFServerButton *> m_ServerButtons;
	CUtlVector<FoFLauncherServerRule> m_ServerRules;
	CUtlVector<CUtlString> m_ServerSpamTags;
	CUtlVector<uint64> m_FriendServerKeys;
	int m_iGroupScroll[FOF_LAUNCHER_MAX_GROUPS];
	char m_szLastSearch[128];
	int m_iBackgroundWide;
	int m_iBackgroundFourThree;
	vgui::HFont m_hSectionFont;
	vgui::HFont m_hSmallFont;
	int m_iTopTall;
	int m_iBodyY;
	int m_iLeftWide;
	int m_iHeaderTall;
	int m_iFooterTall;
	int m_iCourseCardTall;
	int m_iCourseGroupGap;
	float m_flNextWesternPassUpdate;
	int m_nWesternPassProgress;
};

#endif // FOF_LAUNCHER_PANEL_H
