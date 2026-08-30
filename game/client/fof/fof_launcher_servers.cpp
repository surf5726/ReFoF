#include "cbase.h"
#include "fof/fof_launcher_panel.h"
#include "filesystem.h"
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *g_pszFoFServerRulesURL =
	"https://docs.google.com/uc?"
	"id=0B7CcAN2HwhB4RmJreEtWd05DdjQ&export=download";

static bool FoFLauncherRuleMatchesServer(
	const FoFLauncherServerRule &rule,
	const gameserveritem_t &server )
{
	if ( rule.mode != 3 || rule.ip.IsEmpty() )
		return false;

	char serverIP[64];
	Q_strncpy(
		serverIP,
		server.m_NetAdr.GetConnectionAddressString(),
		sizeof( serverIP ) );
	char *pPortSeparator = Q_strrchr( serverIP, ':' );
	if ( pPortSeparator )
		*pPortSeparator = '\0';

	CSplitString ruleIPs( rule.ip.String(), "," );
	for ( int i = 0; i < ruleIPs.Count(); ++i )
	{
		char candidate[128];
		Q_strncpy( candidate, ruleIPs[i], sizeof( candidate ) );
		Q_StripPrecedingAndTrailingWhitespace( candidate );
		if ( Q_stricmp( candidate, serverIP ) )
			continue;

		if ( rule.port == 0 ||
			rule.port == server.m_NetAdr.GetConnectionPort() ||
			rule.port == server.m_NetAdr.GetQueryPort() )
		{
			return true;
		}
	}

	return false;
}

static int FoFLauncherServerMode( const char *pszDescription )
{
	if ( !pszDescription || !pszDescription[0] ||
		!Q_stricmp( pszDescription, "Shootout" ) )
	{
		return 1;
	}

	if ( !Q_stricmp( pszDescription, "2 Team Shootout" ) ||
		!Q_stricmp( pszDescription, "3 Team Shootout" ) ||
		!Q_stricmp( pszDescription, "4 Team Shootout" ) )
	{
		return 6;
	}
	if ( !Q_stricmp( pszDescription, "Course Mode" ) )
		return 7;
	if ( !Q_stricmp( pszDescription, "Teamplay" ) )
		return 2;
	if ( !Q_stricmp( pszDescription, "Versus" ) )
		return 12;
	if ( !Q_stricmp( pszDescription, "Ghost Town" ) )
		return 8;
	if ( !Q_stricmp( pszDescription, "Break Bad" ) )
		return 3;
	if ( !Q_stricmp( pszDescription, "Team Elimination" ) )
		return 9;
	if ( !Q_stricmp( pszDescription, "Grand Elimination" ) )
		return 10;
	if ( Q_stristr( pszDescription, "Classic" ) )
		return 19;
	return 1;
}

static bool FoFLauncherContainsCustomModeName(
	const char *pszName,
	const char *pszDescription )
{
	if ( pszName &&
		( Q_stristr( pszName, "gungame" ) ||
		  Q_stristr( pszName, "gun game" ) ||
		  Q_stristr( pszName, "royal" ) ||
		  Q_stristr( pszName, "zombies" ) ||
		  Q_stristr( pszName, "juggernaut" ) ||
		  Q_stristr( pszName, "wtf" ) ) )
	{
		return true;
	}

	// The shipped launcher also recognizes Gun Game from the server's game
	// description, so localized server names still enter the custom section.
	return pszDescription && Q_stristr( pszDescription, "Gun Game" );
}

static bool FoFLauncherContainsModdedModeName(
	const char *pszName,
	const char *pszMap )
{
	return ( pszName &&
		( Q_stristr( pszName, "!weapons" ) ||
		  Q_stristr( pszName, "super kick" ) ) ) ||
		( pszMap && Q_stristr( pszMap, "fofhr_" ) );
}

static int FoFLauncherServerGroup(
	int mode,
	const char *pszName,
	const char *pszDescription,
	const char *pszMap )
{
	if ( FoFLauncherContainsCustomModeName( pszName, pszDescription ) )
		return FOF_LAUNCHER_SERVER_CUSTOM;
	if ( mode == 18 || mode == 19 ||
		FoFLauncherContainsModdedModeName( pszName, pszMap ) )
	{
		return FOF_LAUNCHER_SERVER_MODDED;
	}
	if ( mode == 2 || mode == 12 )
		return FOF_LAUNCHER_SERVER_STOCK_TEAMPLAY;
	return FOF_LAUNCHER_SERVER_STOCK_SHOOTOUT;
}

void CFoFServersPanel::RequestServerRules()
{
	if ( m_bServerRulesRequested )
		return;

	// The shipped client starts with the bundled file as its failure path,
	// then replaces it with the current remote response when Steam HTTP is
	// available.  Loading it first also keeps the launcher usable offline.
	LoadServerRulesFromFile();
	ReleaseServerRulesRequest();

	ISteamHTTP *pHTTP = steamapicontext ?
		steamapicontext->SteamHTTP() : NULL;
	if ( !pHTTP )
		return;
	m_bServerRulesRequested = true;

	m_hServerRulesRequest = pHTTP->CreateHTTPRequest(
		k_EHTTPMethodGET, g_pszFoFServerRulesURL );
	if ( m_hServerRulesRequest == INVALID_HTTPREQUEST_HANDLE )
		return;

	pHTTP->SetHTTPRequestContextValue(
		m_hServerRulesRequest, 1 );
	pHTTP->SetHTTPRequestAbsoluteTimeoutMS(
		m_hServerRulesRequest, 5000 );

	SteamAPICall_t call = k_uAPICallInvalid;
	if ( !pHTTP->SendHTTPRequest(
		m_hServerRulesRequest, &call ) ||
		call == k_uAPICallInvalid )
	{
		pHTTP->ReleaseHTTPRequest( m_hServerRulesRequest );
		m_hServerRulesRequest = INVALID_HTTPREQUEST_HANDLE;
		return;
	}

	m_ServerRulesCall.Set(
		call,
		this,
		&CFoFServersPanel::OnServerRulesRequestCompleted );
}

void CFoFServersPanel::ReleaseServerRulesRequest()
{
	m_ServerRulesCall.Cancel();
	if ( m_hServerRulesRequest == INVALID_HTTPREQUEST_HANDLE )
		return;

	ISteamHTTP *pHTTP = steamapicontext ?
		steamapicontext->SteamHTTP() : NULL;
	if ( pHTTP )
		pHTTP->ReleaseHTTPRequest( m_hServerRulesRequest );
	m_hServerRulesRequest = INVALID_HTTPREQUEST_HANDLE;
}

void CFoFServersPanel::ClearServerRules()
{
	m_ServerRules.Purge();
	m_ServerSpamTags.Purge();
	m_bServerRuleWhitelisting = false;
}

bool CFoFServersPanel::LoadServerRulesFromFile()
{
	KeyValues *pConfig = new KeyValues( "server_bans" );
	const bool loaded = pConfig->LoadFromFile(
		filesystem, "fof_scripts/serverban.txt", "GAME" ) &&
		ParseServerRules( pConfig );
	pConfig->deleteThis();
	return loaded;
}

bool CFoFServersPanel::LoadServerRulesFromBuffer(
	const char *pszResourceName,
	const char *pBuffer )
{
	if ( !pBuffer || !pBuffer[0] )
		return false;

	KeyValues *pConfig = new KeyValues( "server_bans" );
	const bool loaded = pConfig->LoadFromBuffer(
		pszResourceName, pBuffer, filesystem ) &&
		ParseServerRules( pConfig );
	pConfig->deleteThis();
	return loaded;
}

bool CFoFServersPanel::ParseServerRules( KeyValues *pConfig )
{
	if ( !pConfig )
		return false;

	KeyValues *pRoot = pConfig;
	if ( Q_stricmp( pRoot->GetName(), "server_bans" ) )
		pRoot = pConfig->FindKey( "server_bans", false );
	if ( !pRoot )
		return false;

	ClearServerRules();
	KeyValues *pMode = pRoot->FindKey( "mode", false );
	if ( pMode )
	{
		m_bServerRuleWhitelisting =
			pMode->GetInt( "whitelisting", 0 ) == 1;
	}

	KeyValues *pSpam = pRoot->FindKey( "spam", false );
	if ( pSpam )
	{
		CSplitString tags( pSpam->GetString( "tags", "" ), "," );
		for ( int i = 0; i < tags.Count(); ++i )
		{
			char tag[128];
			Q_strncpy( tag, tags[i], sizeof( tag ) );
			Q_StripPrecedingAndTrailingWhitespace( tag );
			if ( tag[0] )
				m_ServerSpamTags.AddToTail( CUtlString( tag ) );
		}
	}

	KeyValues *pRanked = pRoot->FindKey( "ranked", false );
	if ( pRanked && m_pRankedPanel )
	{
		m_pRankedPanel->SetSeasonData(
			pRanked->GetInt( "season", 0 ),
			pRanked->GetInt( "end_day", 0 ),
			pRanked->GetInt( "end_month", 0 ),
			pRanked->GetInt( "end_year", 0 ) );
	}

	KeyValues *pEntries = pRoot->FindKey( "entries", false );
	if ( pEntries )
	{
		for ( KeyValues *pEntry = pEntries->GetFirstSubKey();
			pEntry;
			pEntry = pEntry->GetNextKey() )
		{
			FoFLauncherServerRule rule;
			rule.name = pEntry->GetString( "name", "" );
			rule.ip = pEntry->GetString( "ip", "" );
			rule.port = pEntry->GetInt( "port", 0 );
			rule.mode = pEntry->GetInt( "mode", 0 );
			if ( !rule.ip.IsEmpty() )
				m_ServerRules.AddToTail( rule );
		}
	}

	return true;
}

void CFoFServersPanel::OnServerRulesRequestCompleted(
	HTTPRequestCompleted_t *pResult,
	bool bIOFailure )
{
	ISteamHTTP *pHTTP = steamapicontext ?
		steamapicontext->SteamHTTP() : NULL;
	bool loadedRemote = false;
	if ( pHTTP && !bIOFailure && pResult &&
		pResult->m_bRequestSuccessful &&
		pResult->m_eStatusCode == k_EHTTPStatusCode200OK &&
		pResult->m_unBodySize > 0 &&
		pResult->m_unBodySize < 0xFA00 )
	{
		uint32 bodySize = 0;
		if ( pHTTP->GetHTTPResponseBodySize(
			pResult->m_hRequest, &bodySize ) &&
			bodySize == pResult->m_unBodySize )
		{
			CUtlVector<uint8> body;
			body.SetCount( bodySize + 1 );
			if ( pHTTP->GetHTTPResponseBodyData(
				pResult->m_hRequest, body.Base(), bodySize ) )
			{
				body[bodySize] = 0;
				loadedRemote = LoadServerRulesFromBuffer(
					g_pszFoFServerRulesURL,
					reinterpret_cast<const char *>( body.Base() ) );
			}
		}
	}

	if ( !loadedRemote )
		LoadServerRulesFromFile();

	const HTTPRequestHandle completedRequest = pResult ?
		pResult->m_hRequest : m_hServerRulesRequest;
	if ( pHTTP &&
		completedRequest != INVALID_HTTPREQUEST_HANDLE )
	{
		pHTTP->ReleaseHTTPRequest( completedRequest );
	}
	if ( completedRequest == m_hServerRulesRequest )
		m_hServerRulesRequest = INVALID_HTTPREQUEST_HANDLE;

	// If a Steam server query beat the HTTP request, rebuild it so entries
	// accepted before the whitelist arrived cannot remain in the panel.
	if ( m_hInternetRequest )
		RefreshSocialData();
}

bool CFoFServersPanel::ServerPassesRemoteRules(
	const gameserveritem_t &server ) const
{
	if ( !m_bServerRuleWhitelisting )
		return true;

	bool hasAddressWhitelist = false;
	for ( int i = 0; i < m_ServerRules.Count(); ++i )
	{
		const FoFLauncherServerRule &rule = m_ServerRules[i];
		if ( rule.mode != 3 )
			continue;
		hasAddressWhitelist = true;
		if ( FoFLauncherRuleMatchesServer( rule, server ) )
			return true;
	}

	// A valid empty or future-format rule set must not make the launcher
	// unusable.  FoF only ships mode-3 address whitelist entries.
	return !hasAddressWhitelist;
}

void CFoFServersPanel::ServerResponded(
	HServerListRequest hRequest,
	int iServer )
{
	if ( hRequest != m_hInternetRequest )
		return;
	UpdateServerEntry( iServer );
	RecountServerTotals();
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::ServerFailedToRespond(
	HServerListRequest hRequest,
	int iServer )
{
	(void)hRequest;
	(void)iServer;
}

void CFoFServersPanel::RefreshComplete(
	HServerListRequest hRequest,
	EMatchMakingServerResponse response )
{
	if ( hRequest != m_hInternetRequest )
		return;
	(void)response;
	ISteamMatchmakingServers *pServers = steamapicontext ?
		steamapicontext->SteamMatchmakingServers() : NULL;
	if ( pServers )
	{
		const int serverCount = pServers->GetServerCount(
			m_hInternetRequest );
		for ( int i = 0; i < serverCount; ++i )
			UpdateServerEntry( i );
	}
	RecountServerTotals();
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::ReleaseInternetRequest()
{
	ISteamMatchmakingServers *pServers = steamapicontext ?
		steamapicontext->SteamMatchmakingServers() : NULL;
	if ( pServers && m_hInternetRequest )
		pServers->ReleaseRequest( m_hInternetRequest );
	m_hInternetRequest = NULL;
}

void CFoFServersPanel::ClearServerEntries()
{
	for ( int i = 0; i < m_ServerButtons.Count(); ++i )
		delete m_ServerButtons[i];
	m_ServerButtons.Purge();
	for ( int i = 0; i < m_Servers.Count(); ++i )
		delete m_Servers[i];
	m_Servers.Purge();
}

void CFoFServersPanel::RememberFriendServer( uint32 ip, uint16 port )
{
	if ( !ip || !port )
		return;
	const uint64 key = ( (uint64)ip << 16 ) | port;
	for ( int i = 0; i < m_FriendServerKeys.Count(); ++i )
	{
		if ( m_FriendServerKeys[i] == key )
			return;
	}
	m_FriendServerKeys.AddToTail( key );
}

bool CFoFServersPanel::IsFriendServer( uint32 ip, uint16 port ) const
{
	const uint64 key = ( (uint64)ip << 16 ) | port;
	for ( int i = 0; i < m_FriendServerKeys.Count(); ++i )
	{
		if ( m_FriendServerKeys[i] == key )
			return true;
	}
	return false;
}

void CFoFServersPanel::RefreshSocialData()
{
	RequestOnlinePlayerCount();

	ISteamFriends *pFriends = steamapicontext ?
		steamapicontext->SteamFriends() : NULL;
	m_iFriendsPlaying = 0;
	m_FriendServerKeys.Purge();
	if ( pFriends )
	{
		const int friendCount = pFriends->GetFriendCount(
			k_EFriendFlagImmediate );
		for ( int i = 0; i < friendCount; ++i )
		{
			const CSteamID friendId = pFriends->GetFriendByIndex(
				i, k_EFriendFlagImmediate );
			FriendGameInfo_t gameInfo;
			if ( pFriends->GetFriendGamePlayed( friendId, &gameInfo ) &&
				gameInfo.m_gameID.AppID() == 265630 )
			{
				++m_iFriendsPlaying;
				RememberFriendServer(
					gameInfo.m_unGameIP,
					gameInfo.m_usGamePort );
				RememberFriendServer(
					gameInfo.m_unGameIP,
					gameInfo.m_usQueryPort );
			}
		}
	}

	ReleaseInternetRequest();
	ClearServerEntries();
	m_iFullServers = 0;
	ISteamMatchmakingServers *pServers = steamapicontext ?
		steamapicontext->SteamMatchmakingServers() : NULL;
	if ( pServers )
	{
		m_hInternetRequest = pServers->RequestInternetServerList(
			265630, NULL, 0, this );
	}
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::RequestOnlinePlayerCount()
{
	ISteamUserStats *pStats = steamapicontext ?
		steamapicontext->SteamUserStats() : NULL;
	if ( !pStats )
		return;

	const SteamAPICall_t call = pStats->GetNumberOfCurrentPlayers();
	if ( call == k_uAPICallInvalid )
		return;

	m_NumberOfCurrentPlayersCall.Set(
		call,
		this,
		&CFoFServersPanel::OnNumberOfCurrentPlayers );
}

void CFoFServersPanel::OnNumberOfCurrentPlayers(
	NumberOfCurrentPlayers_t *pResult,
	bool bIOFailure )
{
	if ( bIOFailure || !pResult || !pResult->m_bSuccess )
		return;

	m_iOnlinePlayers = MAX( pResult->m_cPlayers, 0 );
	InvalidateLayout();
	Repaint();
}

void CFoFServersPanel::UpdateServerEntry( int serverIndex )
{
	ISteamMatchmakingServers *pServers = steamapicontext ?
		steamapicontext->SteamMatchmakingServers() : NULL;
	if ( !pServers || !m_hInternetRequest )
		return;

	const gameserveritem_t *pServer = pServers->GetServerDetails(
		m_hInternetRequest, serverIndex );
	if ( !pServer || !pServer->m_bHadSuccessfulResponse ||
		pServer->m_nPing <= 0 || pServer->m_nPing > 500 )
	{
		return;
	}
	if ( !ServerPassesRemoteRules( *pServer ) )
		return;

	const uint32 ip = pServer->m_NetAdr.GetIP();
	const uint16 port = pServer->m_NetAdr.GetConnectionPort();
	FoFServerEntry *pEntry = NULL;
	for ( int i = 0; i < m_Servers.Count(); ++i )
	{
		if ( m_Servers[i]->ip == ip && m_Servers[i]->port == port )
		{
			pEntry = m_Servers[i];
			break;
		}
	}
	const bool isNew = pEntry == NULL;
	if ( isNew )
		pEntry = new FoFServerEntry;

	pEntry->name = pServer->GetName();
	pEntry->map = pServer->m_szMap;
	pEntry->description = pServer->m_szGameDescription;
	pEntry->address = pServer->m_NetAdr.GetConnectionAddressString();
	pEntry->ip = ip;
	pEntry->port = port;
	pEntry->ping = pServer->m_nPing;
	pEntry->bots = pServer->m_nBotPlayers;
	// Steam's server response includes bots in m_nPlayers.  The shipped FoF
	// launcher presents and sorts by occupied human slots instead.
	pEntry->players = MAX( pServer->m_nPlayers - pEntry->bots, 0 );
	pEntry->maxPlayers = pServer->m_nMaxPlayers;
	pEntry->mode = FoFLauncherServerMode(
		pServer->m_szGameDescription );
	pEntry->group = FoFLauncherServerGroup(
		pEntry->mode,
		pServer->GetName(),
		pServer->m_szGameDescription,
		pServer->m_szMap );
	pEntry->password = pServer->m_bPassword;
	pEntry->secure = pServer->m_bSecure;
	pEntry->friendServer =
		IsFriendServer( ip, port ) ||
		IsFriendServer( ip, pServer->m_NetAdr.GetQueryPort() );

	if ( isNew )
	{
		const int index = m_Servers.AddToTail( pEntry );
		m_ServerButtons.AddToTail(
			new CFoFServerButton( this, pEntry, index ) );
	}
}

bool CFoFServersPanel::ServerMatchesSearch(
	const FoFServerEntry &entry ) const
{
	return !m_szLastSearch[0] ||
		Q_stristr( entry.name.String(), m_szLastSearch ) ||
		Q_stristr( entry.map.String(), m_szLastSearch ) ||
		Q_stristr( entry.description.String(), m_szLastSearch ) ||
		Q_stristr( entry.address.String(), m_szLastSearch );
}

static bool FoFLauncherIsRankedServer(
	const FoFServerEntry &entry )
{
	// The shipped launcher checks the Steam game description at
	// In the original client hostname text is deliberately not
	// part of the decision, making the regular and ranked pages complements.
	return Q_stristr( entry.description.String(), "Ranked" ) != NULL;
}

bool CFoFServersPanel::ServerMatchesCurrentPage(
	const FoFServerEntry &entry,
	bool includeFull ) const
{
	if ( entry.ping > m_iPingLimit || !ServerMatchesSearch( entry ) )
		return false;
	int page = m_iPage;
	if ( page == FOF_LAUNCHER_PAGE_SINGLEPLAYER )
		page = FOF_LAUNCHER_PAGE_TEAMS;
	if ( page == FOF_LAUNCHER_PAGE_TEAMS )
	{
		if ( entry.mode == 7 || FoFLauncherIsRankedServer( entry ) )
		{
			return false;
		}
	}
	else if ( page == FOF_LAUNCHER_PAGE_COOP )
	{
		if ( entry.mode != 7 )
			return false;
	}
	else if ( page == FOF_LAUNCHER_PAGE_RANKED )
	{
		if ( !FoFLauncherIsRankedServer( entry ) )
			return false;
	}
	else if ( page == FOF_LAUNCHER_PAGE_FRIENDS &&
		!entry.friendServer )
	{
		return false;
	}

	const bool full = entry.maxPlayers > 0 &&
		entry.players >= entry.maxPlayers;
	return includeFull || !full;
}

void CFoFServersPanel::RecountServerTotals()
{
	int fullServers = 0;
	for ( int i = 0; i < m_Servers.Count(); ++i )
	{
		const FoFServerEntry &entry = *m_Servers[i];
		if ( entry.maxPlayers > 0 &&
			entry.players >= entry.maxPlayers &&
			ServerMatchesCurrentPage( entry, true ) )
		{
			++fullServers;
		}
	}
	m_iFullServers = fullServers;
}

bool CFoFServersPanel::ServerComesBefore( int left, int right ) const
{
	const FoFServerEntry &a = *m_Servers[left];
	const FoFServerEntry &b = *m_Servers[right];
	if ( a.players != b.players )
		return a.players > b.players;
	if ( a.ping != b.ping )
		return a.ping < b.ping;
	return Q_stricmp( a.name.String(), b.name.String() ) < 0;
}

void CFoFServersPanel::LayoutServers()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );
	const int contentX = m_iLeftWide +
		FoFLauncherScalePixel( 2.5f, screenTall );
	const int contentWide = MAX(
		wide - contentX - FoFLauncherScalePixel( 10.0f, screenTall ),
		1 );
	const int columns = 6;
	const int cardGap = 2;
	const int cardWide = MAX(
		( contentWide - cardGap * ( columns - 1 ) ) / columns,
		36 );
	m_iCourseCardTall = MAX(
		FoFLauncherScalePixel( 65.0f, screenTall ), 1 );
	m_iCourseGroupGap = MAX(
		FoFLauncherScalePixel( 2.5f, screenTall ), 1 );
	const int groupTitleTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	int groupY = m_iBodyY + m_iHeaderTall;

	for ( int i = 0; i < m_CourseButtons.Count(); ++i )
		m_CourseButtons[i]->SetVisible( false );
	for ( int i = 0; i < m_ServerButtons.Count(); ++i )
		m_ServerButtons[i]->SetVisible( false );
	for ( int group = 0;
		group < FOF_LAUNCHER_MAX_GROUPS;
		++group )
	{
		m_pGroupPrevious[group]->SetVisible( false );
		m_pGroupNext[group]->SetVisible( false );
	}

	const int groupCount =
		m_iPage == FOF_LAUNCHER_PAGE_TEAMS ?
		FOF_LAUNCHER_MAX_GROUPS : 1;
	for ( int group = 0; group < groupCount; ++group )
	{
		CUtlVector<int> matches;
		for ( int i = 0; i < m_Servers.Count(); ++i )
		{
			if ( !ServerMatchesCurrentPage( *m_Servers[i], false ) )
				continue;
			if ( m_iPage == FOF_LAUNCHER_PAGE_TEAMS &&
				m_Servers[i]->group != group )
			{
				continue;
			}
			matches.AddToTail( i );
		}

		for ( int i = 1; i < matches.Count(); ++i )
		{
			const int value = matches[i];
			int insert = i;
			while ( insert > 0 &&
				ServerComesBefore( value, matches[insert - 1] ) )
			{
				matches[insert] = matches[insert - 1];
				--insert;
			}
			matches[insert] = value;
		}

		const int maxScroll = MAX( matches.Count() - columns, 0 );
		m_iGroupScroll[group] = clamp(
			m_iGroupScroll[group], 0, maxScroll );
		for ( int column = 0; column < columns; ++column )
		{
			const int visibleIndex = m_iGroupScroll[group] + column;
			if ( visibleIndex >= matches.Count() )
				break;
			CFoFServerButton *pButton =
				m_ServerButtons[matches[visibleIndex]];
			pButton->SetBounds(
				contentX + column * ( cardWide + cardGap ),
				groupY + groupTitleTall,
				cardWide,
				m_iCourseCardTall );
			pButton->SetVisible( true );
		}

		const int arrowWide = MAX(
			FoFLauncherScalePixel( 14.0f, screenTall ), 1 );
		const int arrowTall = arrowWide;
		m_pGroupPrevious[group]->SetBounds(
			contentX,
			groupY + groupTitleTall +
				( m_iCourseCardTall - arrowTall ) / 2,
			arrowWide,
			arrowTall );
		m_pGroupNext[group]->SetBounds(
			wide - arrowWide,
			groupY + groupTitleTall +
				( m_iCourseCardTall - arrowTall ) / 2,
			arrowWide,
			arrowTall );
		m_pGroupPrevious[group]->SetVisible(
			m_iGroupScroll[group] > 0 );
		m_pGroupNext[group]->SetVisible(
			m_iGroupScroll[group] < maxScroll );
		// Server cards are created after the navigation controls.  Keep both
		// arrows above those cards so the left arrow is not hidden by column 0
		// after the first right-page click.
		m_pGroupPrevious[group]->MoveToFront();
		m_pGroupNext[group]->MoveToFront();
		groupY += groupTitleTall +
			m_iCourseCardTall + m_iCourseGroupGap;
	}
}

void CFoFServersPanel::JoinServer( int index )
{
	if ( index < 0 || index >= m_Servers.Count() )
		return;
	char command[128];
	Q_snprintf(
		command,
		sizeof( command ),
		"connect %s\n",
		m_Servers[index]->address.String() );
	engine->ClientCmd_Unrestricted( command );
}

CFoFServerButton::CFoFServerButton(
	vgui::Panel *pParent,
	FoFServerEntry *pEntry,
	int index )
	: BaseClass( pParent, "FoFServerButton", "", pParent, "" )
	, m_pEntry( pEntry )
	, m_iMapTexture( -1 )
	, m_iLockTexture( -1 )
	, m_iSecureTexture( -1 )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hStatusFont( vgui::INVALID_FONT )
{
	char command[64];
	Q_snprintf( command, sizeof( command ), "server:%d", index );
	SetCommand( command );
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	DrawFocusBox( false );

	char materialFile[MAX_PATH];
	Q_snprintf(
		materialFile,
		sizeof( materialFile ),
		"materials/vgui/maps/menu_thumb_%s.vtf",
		m_pEntry->map.String() );
	char material[MAX_PATH];
	if ( filesystem->FileExists( materialFile, "GAME" ) )
	{
		Q_snprintf(
			material,
			sizeof( material ),
			"vgui/maps/menu_thumb_%s",
			m_pEntry->map.String() );
	}
	else
	{
		Q_strncpy(
			material,
			"vgui/maps/menu_thumb_default_download",
			sizeof( material ) );
	}
	m_iMapTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iMapTexture, material, true, false );

	m_iLockTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iLockTexture, "vgui/lock", true, false );
	m_iSecureTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iSecureTexture,
		"vgui/servers/icon_secure_deny",
		true,
		false );
}

CFoFServerButton::~CFoFServerButton()
{
	if ( m_iMapTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iMapTexture );
	if ( m_iLockTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iLockTexture );
	if ( m_iSecureTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iSecureTexture );
}

void CFoFServerButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	// ServerItem's title_label uses m_hTextFont2 (DefaultBold).
	m_hTitleFont = pFoFScheme ?
		pFoFScheme->GetFont( "DefaultBold", false ) :
		vgui::INVALID_FONT;
	m_hStatusFont = pFoFScheme ?
		pFoFScheme->GetFont( "MenuFontSmall", false ) :
		vgui::INVALID_FONT;
	if ( m_hStatusFont == vgui::INVALID_FONT )
		m_hStatusFont = pScheme->GetFont( "MenuFontSmall", false );
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = pScheme->GetFont( "DefaultBold", false );
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = m_hStatusFont;
	if ( m_hStatusFont == vgui::INVALID_FONT )
		m_hStatusFont = m_hTitleFont;
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
}

void CFoFServerButton::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	const int statusTall = MAX( tall * 17 / 104, 1 );
	const int imageBottom = tall - statusTall;
	if ( m_iMapTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iMapTexture );
		vgui::surface()->DrawTexturedSubRect(
			1, 1, wide - 1, imageBottom,
			0.0f, 0.0f, 1.0f, 0.75f );
	}

	vgui::surface()->DrawSetColor( 0, 0, 0, 48 );
	vgui::surface()->DrawFilledRect( 1, 1, wide - 1, imageBottom );
	vgui::surface()->DrawSetColor( 0, 0, 0, 180 );
	vgui::surface()->DrawFilledRect(
		1, imageBottom, wide - 1, tall - 1 );

	if ( IsArmed() )
	{
		vgui::surface()->DrawSetColor( 205, 20, 8, 160 );
		vgui::surface()->DrawFilledRect( 1, 1, wide - 1, tall - 1 );
	}

	FoFLauncherDrawWrappedTitle(
		m_pEntry->name.String(),
		m_hTitleFont,
		4,
		2,
		MAX( wide - 8, 1 ),
		4,
		Color( 245, 236, 0, 255 ) );

	int iconX = 2;
	const int iconTall = MAX( statusTall - 4, 1 );
	if ( m_pEntry->password && m_iLockTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 235, 225, 30, 255 );
		vgui::surface()->DrawSetTexture( m_iLockTexture );
		vgui::surface()->DrawTexturedRect(
			iconX,
			imageBottom + 2,
			iconX + iconTall,
			imageBottom + 2 + iconTall );
		iconX += iconTall + 1;
	}
	if ( m_pEntry->secure && m_iSecureTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 120, 190, 250, 255 );
		vgui::surface()->DrawSetTexture( m_iSecureTexture );
		vgui::surface()->DrawTexturedRect(
			iconX,
			imageBottom + 2,
			iconX + iconTall,
			imageBottom + 2 + iconTall );
	}

	char status[64];
	Q_snprintf(
		status,
		sizeof( status ),
		"%d/%d (%dms)",
		m_pEntry->players,
		m_pEntry->maxPlayers,
		m_pEntry->ping );
	wchar_t wszStatus[64];
	g_pVGuiLocalize->ConvertANSIToUnicode(
		status, wszStatus, sizeof( wszStatus ) );
	const int statusWide = FoFLauncherTextWide(
		wszStatus, m_hStatusFont );
	FoFLauncherDrawText(
		wszStatus,
		m_hStatusFont,
		MAX( wide - statusWide - 2, 2 ),
		imageBottom + 1,
		Color( 225, 225, 215, 255 ) );

	vgui::surface()->DrawSetColor( 83, 54, 25, 255 );
	vgui::surface()->DrawOutlinedRect( 0, 0, wide, tall );
}
