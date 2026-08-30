#include "cbase.h"
#include "fof/fof_create_game.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include <vgui/IScheme.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoFCreateGameDialog *g_pFoFCreateGameDialog = NULL;

static int FoFCompareMenuNames(
	const CUtlString *pLeft,
	const CUtlString *pRight )
{
	return Q_stricmp( pLeft->String(), pRight->String() );
}

static void FoFStripMenuExtension(
	const char *pszFileName,
	char *pszResult,
	int nResultSize )
{
	const char *pszBaseName = pszFileName;
	for ( const char *pszScan = pszFileName; *pszScan; ++pszScan )
	{
		if ( *pszScan == '/' || *pszScan == '\\' )
			pszBaseName = pszScan + 1;
	}
	Q_StripExtension( pszBaseName, pszResult, nResultSize );
}

static bool FoFVectorContainsName(
	const CUtlVector<CUtlString> &names,
	const char *pszName )
{
	for ( int i = 0; i < names.Count(); ++i )
	{
		if ( !Q_stricmp( names[i].String(), pszName ) )
			return true;
	}
	return false;
}

static void FoFFindMenuFiles(
	const char *pszWildcard,
	const char *pszPathID,
	CUtlVector<CUtlString> &names )
{
	names.Purge();
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszFileName =
		filesystem->FindFirstEx( pszWildcard, pszPathID, &findHandle );
	while ( pszFileName )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
		{
			char szName[MAX_PATH];
			FoFStripMenuExtension( pszFileName, szName, sizeof( szName ) );
			if ( szName[0] && !FoFVectorContainsName( names, szName ) )
				names.AddToTail( CUtlString( szName ) );
		}
		pszFileName = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );
	names.Sort( FoFCompareMenuNames );
}

static void FoFAddMenuInt(
	vgui::ComboBox *pCombo,
	const char *pszLabel,
	const char *pszKey,
	int iValue )
{
	KeyValues *pData = new KeyValues( "data" );
	pData->SetInt( pszKey, iValue );
	pCombo->AddItem( pszLabel, pData );
	pData->deleteThis();
}

static void FoFAddMenuString(
	vgui::ComboBox *pCombo,
	const char *pszLabel,
	const char *pszKey,
	const char *pszValue )
{
	KeyValues *pData = new KeyValues( "data" );
	pData->SetString( pszKey, pszValue );
	pCombo->AddItem( pszLabel, pData );
	pData->deleteThis();
}

static bool FoFMapHasCourse( const char *pszMapName )
{
	CUtlVector<CUtlString> courses;
	FoFFindMenuFiles(
		"fof_scripts/courses/*.txt", "GAME", courses );
	for ( int i = 0; i < courses.Count(); ++i )
	{
		if ( Q_stristr( courses[i].String(), pszMapName ) )
			return true;
	}
	return false;
}

static bool FoFMapMatchesMode( const char *pszMapName, int iMode )
{
	switch ( iMode )
	{
	case 0:
		return true;
	case 1:
	case 3:
	case 4:
		return !Q_strnicmp( pszMapName, "fof_", 4 ) ||
			!Q_strnicmp( pszMapName, "fofhr_", 6 );
	case 2:
		return !Q_strnicmp( pszMapName, "tp_", 3 ) ||
			!Q_strnicmp( pszMapName, "tphr_", 5 );
	case 5:
		return !Q_strnicmp( pszMapName, "vs_", 3 );
	case 6:
		return FoFMapHasCourse( pszMapName );
	default:
		return false;
	}
}

CFoFCreateGameDialog::CFoFCreateGameDialog()
	: BaseClass( NULL, "FoFCreateGamePanel", true, true )
	, m_pMapImage( NULL )
	, m_pMapList( NULL )
	, m_pModesList( NULL )
	, m_pServerSlots( NULL )
	, m_pServerName( NULL )
	, m_pServerPass( NULL )
	, m_pBotsAllowed( NULL )
	, m_pBotsCustomScript( NULL )
	, m_pBotsSkill( NULL )
	, m_pBotDynamic( NULL )
	, m_pCourseScripts( NULL )
	, m_pDuration( NULL )
	, m_pTeamplay( NULL )
	, m_pTeamNumber( NULL )
	, m_pServerConfig( new KeyValues( "ServerConfig" ) )
	, m_bUpdatingControls( true )
{
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/SourceScheme.res", "SourceScheme" ) );
	SetProportional( false );
	SetDeleteSelfOnClose( false );

	m_pMapImage = new vgui::ImagePanel( this, "MapImage" );
	m_pMapList = new vgui::ComboBox( this, "MapList", 25, false );
	m_pModesList = new vgui::ComboBox( this, "ModesList", 10, false );
	m_pServerSlots = new vgui::ComboBox( this, "ServerSlots", 10, false );
	m_pServerName = new vgui::TextEntry( this, "ServerName" );
	m_pServerPass = new vgui::TextEntry( this, "ServerPass" );
	m_pBotsAllowed = new vgui::ComboBox( this, "BotsAllowed", 6, false );
	m_pBotsCustomScript =
		new vgui::ComboBox( this, "BotsCustScript", 10, false );
	m_pBotsSkill = new vgui::ComboBox( this, "BotsSkill", 7, false );
	m_pBotDynamic = new vgui::CheckButton(
		this, "BotDynamic", "#Bot_Amount_1" );
	m_pCourseScripts = new vgui::ComboBox(
		this, "CourseScripts", 10, false );
	m_pDuration = new vgui::TextEntry( this, "Duration" );
	m_pTeamplay = new vgui::CheckButton(
		this, "Teamplay", "#GCMenu_Teamplay" );
	m_pTeamNumber = new vgui::ComboBox( this, "TNumber", 3, false );

	m_pServerName->SetMaximumCharCount( 200 );
	m_pServerPass->SetMaximumCharCount( 32 );
	m_pDuration->SetMaximumCharCount( 3 );
	m_pDuration->SetAllowNumericInputOnly( true );
	m_pServerName->SetText( "My Fistful of Frags Listen Server" );
	m_pDuration->SetText( "10" );

	vgui::Panel *pSignals[] =
	{
		m_pMapList,
		m_pModesList,
		m_pBotsAllowed,
		m_pBotsCustomScript,
		m_pBotsSkill,
		m_pCourseScripts,
		m_pTeamNumber
	};
	for ( int i = 0; i < ARRAYSIZE( pSignals ); ++i )
		pSignals[i]->AddActionSignalTarget( this );

	PopulateModes();
	PopulateBotAmounts();
	PopulateBotSkills();
	PopulateBotScripts();
	PopulateTeamCounts();
	UpdateServerSlots( 1 );
	PopulateMaps( 1 );
	PopulateCourseScripts();

	LoadControlSettings( "resource/ui/FoFCreateGamePanel.res" );
	LoadServerConfig();
	m_bUpdatingControls = false;

	const int iMode = GetSelectedInt( m_pModesList, "mode", 1 );
	UpdateModeControls( iMode );
	UpdateMapImage();
	SetVisible( false );
}

CFoFCreateGameDialog::~CFoFCreateGameDialog()
{
	if ( m_pServerConfig )
		m_pServerConfig->deleteThis();
}

void CFoFCreateGameDialog::PopulateModes()
{
	struct ModeEntry
	{
		const char *pszLabel;
		int iMode;
	};
	static const ModeEntry modes[] =
	{
		{ "#GCMenu_Shootout", 1 },
		{ "#GCMenu_TP", 2 },
		{ "#GCMenu_BB", 3 },
		{ "#GCMenu_ELM", 4 },
		{ "#GCMenu_VS", 5 },
		{ "#GCMenu_Course", 6 },
		{ "#GCMenu_FullList", 0 }
	};
	m_pModesList->DeleteAllItems();
	for ( int i = 0; i < ARRAYSIZE( modes ); ++i )
		FoFAddMenuInt(
			m_pModesList, modes[i].pszLabel, "mode", modes[i].iMode );
	m_pModesList->SilentActivateItemByRow( 0 );
}

void CFoFCreateGameDialog::PopulateMaps( int iMode )
{
	CUtlVector<CUtlString> maps;
	FoFFindMenuFiles( "maps/*.bsp", "GAME", maps );
	m_pMapList->DeleteAllItems();
	for ( int i = 0; i < maps.Count(); ++i )
	{
		const char *pszMapName = maps[i].String();
		if ( !FoFMapMatchesMode( pszMapName, iMode ) )
			continue;
		FoFAddMenuString(
			m_pMapList, pszMapName, "mapname", pszMapName );
	}
	if ( m_pMapList->GetItemCount() > 0 )
		m_pMapList->SilentActivateItemByRow( 0 );
}

void CFoFCreateGameDialog::PopulateBotAmounts()
{
	struct BotEntry
	{
		const char *pszLabel;
		int iAmount;
	};
	static const BotEntry bots[] =
	{
		{ "#Bot_Amount_0", 0 },
		{ "15%", 3 },
		{ "30%", 6 },
		{ "50%", 10 },
		{ "75%", 15 },
		{ "90%", 18 }
	};
	for ( int i = 0; i < ARRAYSIZE( bots ); ++i )
		FoFAddMenuInt(
			m_pBotsAllowed,
			bots[i].pszLabel,
			"bot_amount",
			bots[i].iAmount );
	m_pBotsAllowed->SilentActivateItemByRow( 3 );
}

void CFoFCreateGameDialog::PopulateBotSkills()
{
	for ( int i = 0; i <= 6; ++i )
	{
		char szLabel[32];
		Q_snprintf( szLabel, sizeof( szLabel ), "#Bot_Skill_%d", i );
		FoFAddMenuInt( m_pBotsSkill, szLabel, "bot_skill", i );
	}
	m_pBotsSkill->SilentActivateItemByRow( 5 );
}

void CFoFCreateGameDialog::PopulateBotScripts()
{
	CUtlVector<CUtlString> scripts;
	FoFFindMenuFiles( "fof_scripts/bots/*.txt", "GAME", scripts );
	m_pBotsCustomScript->DeleteAllItems();
	for ( int i = 0; i < scripts.Count(); ++i )
		FoFAddMenuString(
			m_pBotsCustomScript,
			scripts[i].String(),
			"bot_scriptname",
			scripts[i].String() );
	if ( m_pBotsCustomScript->GetItemCount() > 0 )
		m_pBotsCustomScript->SilentActivateItemByRow( 0 );
}

void CFoFCreateGameDialog::PopulateCourseScripts()
{
	const char *pszMapName =
		GetSelectedString( m_pMapList, "mapname", "" );
	CUtlVector<CUtlString> scripts;
	FoFFindMenuFiles( "fof_scripts/courses/*.txt", "GAME", scripts );
	m_pCourseScripts->DeleteAllItems();
	FoFAddMenuString(
		m_pCourseScripts,
		"#GameCreate_Courses_NotSel",
		"course_name",
		"none" );
	for ( int i = 0; i < scripts.Count(); ++i )
	{
		if ( !pszMapName[0] ||
			!Q_stristr( scripts[i].String(), pszMapName ) )
		{
			continue;
		}
		FoFAddMenuString(
			m_pCourseScripts,
			scripts[i].String(),
			"course_name",
			scripts[i].String() );
	}
	m_pCourseScripts->SilentActivateItemByRow( 0 );
}

void CFoFCreateGameDialog::PopulateTeamCounts()
{
	for ( int i = 2; i <= 4; ++i )
	{
		char szValue[8];
		Q_snprintf( szValue, sizeof( szValue ), "%d", i );
		FoFAddMenuInt( m_pTeamNumber, szValue, "team_number", i );
	}
	m_pTeamNumber->SilentActivateItemByRow( 2 );
}

void CFoFCreateGameDialog::UpdateServerSlots( int iMode )
{
	const int iOldRow = MAX( m_pServerSlots->GetActiveItem(), 0 );
	const int iMaximum =
		( iMode == 1 || iMode == 3 || iMode == 4 ||
			iMode == 5 || iMode == 6 ) ? 24 : 21;
	m_pServerSlots->DeleteAllItems();
	for ( int i = 21; i <= iMaximum; ++i )
	{
		char szSlots[8];
		Q_snprintf( szSlots, sizeof( szSlots ), "%d", i );
		FoFAddMenuInt( m_pServerSlots, szSlots, "slots", i );
	}
	SelectRow(
		m_pServerSlots,
		iMode == 6 ? 0 : iOldRow,
		0 );
}

void CFoFCreateGameDialog::UpdateModeControls( int iMode )
{
	const bool bTeamplay = iMode == 1 || iMode == 3 || iMode == 4;
	const bool bHasTeamCount = iMode != 2 && iMode != 6;
	const bool bCourse = iMode == 6;

	SetControlVisible( "Teamplay", bTeamplay );
	SetControlVisible( "TNumber", bHasTeamCount );
	SetControlVisible( "TNumberLabel", bHasTeamCount );
	SetControlVisible( "CourseScripts", bCourse );
	SetControlVisible( "CourseScriptsLabel", bCourse );
	SetControlVisible( "BotsAllowed", !bCourse );
	SetControlVisible( "BotsAllowedLabel", !bCourse );
	SetControlVisible( "BotDynamic", !bCourse );
	SetControlVisible( "BotsSkill", !bCourse );
	SetControlVisible( "BotsSkillLabel", !bCourse );
	SetControlVisible( "BotsCustScript", !bCourse );
	SetControlVisible( "BotsCustScriptLabel", !bCourse );
	SetControlVisible( "Duration", true );
	SetControlVisible( "DurationLabel", true );
}

void CFoFCreateGameDialog::UpdateMapImage()
{
	const char *pszMapName =
		GetSelectedString( m_pMapList, "mapname", "" );
	char szMaterialPath[MAX_PATH];
	char szImageName[MAX_PATH];
	Q_snprintf(
		szMaterialPath,
		sizeof( szMaterialPath ),
		"materials/vgui/maps/menu_thumb_%s.vtf",
		pszMapName );
	if ( pszMapName[0] && filesystem->FileExists( szMaterialPath, "GAME" ) )
	{
		Q_snprintf(
			szImageName,
			sizeof( szImageName ),
			"maps/menu_thumb_%s",
			pszMapName );
		m_pMapImage->SetImage( szImageName );
	}
	else
	{
		m_pMapImage->SetImage( "maps/menu_thumb_default_download" );
	}
}

void CFoFCreateGameDialog::OnTextChanged( KeyValues *pData )
{
	if ( m_bUpdatingControls )
		return;
	vgui::Panel *pPanel = reinterpret_cast<vgui::Panel *>(
		pData->GetPtr( "panel" ) );
	if ( pPanel == m_pModesList )
	{
		m_bUpdatingControls = true;
		const int iMode = GetSelectedInt( m_pModesList, "mode", 1 );
		UpdateServerSlots( iMode );
		PopulateMaps( iMode );
		PopulateCourseScripts();
		UpdateModeControls( iMode );
		UpdateMapImage();
		m_bUpdatingControls = false;
		return;
	}
	if ( pPanel == m_pMapList )
	{
		m_bUpdatingControls = true;
		UpdateMapImage();
		if ( GetSelectedInt( m_pModesList, "mode", 1 ) == 6 )
			PopulateCourseScripts();
		m_bUpdatingControls = false;
		return;
	}
	ApplyImmediateSetting( pPanel );
}
void CFoFCreateGameDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IBorder *pBorder = pScheme->GetBorder( "BaseBorder" );
	if ( pBorder )
		m_pMapImage->SetBorder( pBorder );
}

void CFoFCreateGameDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	const int iWide = 600;
	const int iTall = 400;
	const int iX = ( ScreenWidth() - iWide ) / 2;
	const int iY = ScreenHeight() / 2 - 100;
	SetBounds(
		clamp( iX, 0, MAX( ScreenWidth() - iWide, 0 ) ),
		clamp( iY, 0, MAX( ScreenHeight() - iTall, 0 ) ),
		iWide,
		iTall );
}

void CFoFCreateGameDialog::ShowDialog()
{
	PerformLayout();
	SetVisible( true );
	SetEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	Activate();
}

void CFoFCreateGameDialog::HideDialog()
{
	SetVisible( false );
}

void CFoFCreateGameDialog::OnCommand( const char *pszCommand )
{
	// The resource's OK button sends lower-case "close", while Frame's title
	// bar X sends "Close".  FoF relies on that case distinction: only
	// the former starts the listen server; the latter must reach Frame::OnCommand
	// and simply close the dialog.
	if ( !Q_strcmp( pszCommand, "close" ) )
	{
		SaveServerConfig();
		StartListenServer();
		HideDialog();
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFCreateGameDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		HideDialog();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

CON_COMMAND( OpenCreateGamePanel, "Open the Fistful of Frags listen-server setup." )
{
	if ( !g_pFoFCreateGameDialog )
		g_pFoFCreateGameDialog = new CFoFCreateGameDialog();
	g_pFoFCreateGameDialog->ShowDialog();
}

static void FoFSanitizeQuotedCommandText(
	const char *pszInput,
	char *pszOutput,
	int nOutputSize )
{
	int iOutput = 0;
	for ( int i = 0;
		pszInput[i] && iOutput < nOutputSize - 1;
		++i )
	{
		const char c = pszInput[i];
		pszOutput[iOutput++] =
			( c == '"' || c == ';' || c == '\r' || c == '\n' ) ? ' ' : c;
	}
	pszOutput[iOutput] = '\0';
}

static void FoFSanitizeCommandToken(
	const char *pszInput,
	char *pszOutput,
	int nOutputSize )
{
	int iOutput = 0;
	for ( int i = 0;
		pszInput[i] && iOutput < nOutputSize - 1;
		++i )
	{
		const unsigned char c = (unsigned char)pszInput[i];
		if ( V_isalnum( c ) || c == '_' || c == '-' || c == '.' ||
			c == '[' || c == ']' )
		{
			pszOutput[iOutput++] = (char)c;
		}
	}
	pszOutput[iOutput] = '\0';
}

static void FoFSetConVarInt( const char *pszName, int iValue )
{
	ConVarRef value( pszName, true );
	if ( value.IsValid() )
		value.SetValue( iValue );
}

static void FoFSetConVarFloat( const char *pszName, float flValue )
{
	ConVarRef value( pszName, true );
	if ( value.IsValid() )
		value.SetValue( flValue );
}

static void FoFSetConVarString( const char *pszName, const char *pszValue )
{
	ConVarRef value( pszName, true );
	if ( value.IsValid() )
		value.SetValue( pszValue );
}

void CFoFCreateGameDialog::LoadServerConfig()
{
	if ( !m_pServerConfig )
		return;
	m_pServerConfig->LoadFromFile(
		filesystem, "ServerConfig.vdf", "MOD" );

	const int iMode = m_pServerConfig->GetInt( "mode", 1 );
	const int iModeRow = FindItemByInt(
		m_pModesList, "mode", iMode );
	SelectRow( m_pModesList, iModeRow, 0 );
	UpdateServerSlots( iMode );
	PopulateMaps( iMode );
	SelectRow(
		m_pMapList,
		m_pServerConfig->GetInt( "map_item", 0 ),
		0 );
	SelectRow(
		m_pBotsAllowed,
		m_pServerConfig->GetInt( "bot_item", 3 ),
		3 );
	SelectRow(
		m_pServerSlots,
		m_pServerConfig->GetInt( "slots_item", 10 ),
		0 );
	m_pDuration->SetText(
		m_pServerConfig->GetString( "playtime", "10" ) );
	m_pTeamplay->SetSelected(
		m_pServerConfig->GetBool( "teamplay", false ) );
	SelectRow(
		m_pTeamNumber,
		m_pServerConfig->GetInt( "team_item", 3 ),
		2 );
	SelectRow(
		m_pBotsSkill,
		m_pServerConfig->GetInt( "skill_item", 1 ),
		1 );
	SelectRow(
		m_pBotsCustomScript,
		m_pServerConfig->GetInt( "bot_script", 1 ),
		0 );

	ConVarRef password( "sv_password", true );
	if ( password.IsValid() )
		m_pServerPass->SetText( password.GetString() );

	PopulateCourseScripts();
	UpdateModeControls( iMode );
	UpdateMapImage();
}

void CFoFCreateGameDialog::SaveServerConfig()
{
	if ( !m_pServerConfig )
		return;
	char szDuration[64];
	m_pDuration->GetText( szDuration, sizeof( szDuration ) );
	m_pServerConfig->SetInt( "map_item", m_pMapList->GetActiveItem() );
	m_pServerConfig->SetInt(
		"mode", GetSelectedInt( m_pModesList, "mode", 1 ) );
	m_pServerConfig->SetInt(
		"bot_item", m_pBotsAllowed->GetActiveItem() );
	m_pServerConfig->SetInt(
		"slots_item", m_pServerSlots->GetActiveItem() );
	m_pServerConfig->SetString( "playtime", szDuration );
	m_pServerConfig->SetBool( "teamplay", m_pTeamplay->IsSelected() );
	m_pServerConfig->SetInt(
		"team_item", m_pTeamNumber->GetActiveItem() );
	m_pServerConfig->SetInt(
		"skill_item", m_pBotsSkill->GetActiveItem() );
	m_pServerConfig->SetInt(
		"bot_script", m_pBotsCustomScript->GetActiveItem() );
	m_pServerConfig->SaveToFile(
		filesystem, "ServerConfig.vdf", "MOD" );
}

void CFoFCreateGameDialog::ApplyImmediateSetting( vgui::Panel *pPanel )
{
	if ( pPanel == m_pBotsAllowed )
	{
		const int iAmount = GetSelectedInt(
			m_pBotsAllowed, "bot_amount", 0 );
		FoFSetConVarFloat( "fof_sv_bot_slotpct", iAmount * 0.05f );
	}
	else if ( pPanel == m_pBotsSkill )
	{
		FoFSetConVarInt(
			"fof_bot_skill",
			GetSelectedInt( m_pBotsSkill, "bot_skill", 0 ) );
	}
	else if ( pPanel == m_pBotsCustomScript )
	{
		FoFSetConVarString(
			"fof_bot_scriptname",
			GetSelectedString(
				m_pBotsCustomScript, "bot_scriptname", "" ) );
	}
	else if ( pPanel == m_pTeamNumber )
	{
		FoFSetConVarInt(
			"fof_sv_maxteams",
			GetSelectedInt( m_pTeamNumber, "team_number", 2 ) );
	}
	else if ( pPanel == m_pCourseScripts )
	{
		char szCourse[256];
		FoFSanitizeCommandToken(
			GetSelectedString(
				m_pCourseScripts, "course_name", "none" ),
			szCourse,
			sizeof( szCourse ) );
		char szCommand[320];
		Q_snprintf(
			szCommand,
			sizeof( szCommand ),
			"fof_course_script %s\n",
			szCourse[0] ? szCourse : "none" );
		engine->ClientCmd_Unrestricted( szCommand );
	}
}

void CFoFCreateGameDialog::StartListenServer()
{
	if ( m_pMapList->GetItemCount() <= 0 )
		return;

	char szServerName[256];
	char szPassword[64];
	char szDuration[64];
	m_pServerName->GetText( szServerName, sizeof( szServerName ) );
	m_pServerPass->GetText( szPassword, sizeof( szPassword ) );
	m_pDuration->GetText( szDuration, sizeof( szDuration ) );

	char szSafeServerName[256];
	char szSafePassword[64];
	char szMapName[128];
	char szBotScript[256];
	FoFSanitizeQuotedCommandText(
		szServerName, szSafeServerName, sizeof( szSafeServerName ) );
	FoFSanitizeQuotedCommandText(
		szPassword, szSafePassword, sizeof( szSafePassword ) );
	FoFSanitizeCommandToken(
		GetSelectedString( m_pMapList, "mapname", "" ),
		szMapName,
		sizeof( szMapName ) );
	FoFSanitizeCommandToken(
		GetSelectedString(
			m_pBotsCustomScript, "bot_scriptname", "1_default_bots" ),
		szBotScript,
		sizeof( szBotScript ) );

	const int iMode = GetSelectedInt( m_pModesList, "mode", 1 );
	const int iSlots = GetSelectedInt( m_pServerSlots, "slots", 21 );
	const int iTeams = GetSelectedInt( m_pTeamNumber, "team_number", 2 );
	const int iBotSkill = GetSelectedInt( m_pBotsSkill, "bot_skill", 0 );
	const int iTimeLimit = clamp( atoi( szDuration ), 0, 999 );
	const int iLan = szSafePassword[0] ? 0 : 1;

	FoFSetConVarInt( "mp_timelimit", iTimeLimit );
	ApplyImmediateSetting( m_pBotsAllowed );
	ApplyImmediateSetting( m_pBotsSkill );
	ApplyImmediateSetting( m_pBotsCustomScript );
	ApplyImmediateSetting( m_pTeamNumber );
	if ( iMode == 6 )
		ApplyImmediateSetting( m_pCourseScripts );

	char szCommand[2048];
	Q_snprintf(
		szCommand,
		sizeof( szCommand ),
		"disconnect\n"
		"wait\n"
		"wait\n"
		"sv_lan %d\n"
		"setmaster enable\n"
		"maxplayers %d\n"
		"sv_password \"%s\"\n"
		"hostname \"%s\"\n"
		"progress_enable\n"
		"fof_sv_currentmode %d\n"
		"mp_teamplay %d\n"
		"fof_sv_bot_dynamicjoin %d\n"
		"map %s\n"
		"fof_listenserver 1\n"
		"fof_sv_maxteams %d\n"
		"fof_bot_scriptname %s\n"
		"fof_bot_skill %d\n",
		iLan,
		iSlots,
		szSafePassword,
		szSafeServerName,
		iMode,
		m_pTeamplay->IsSelected() ? 1 : 0,
		m_pBotDynamic->IsSelected() ? 1 : 0,
		szMapName,
		iTeams,
		szBotScript[0] ? szBotScript : "1_default_bots",
		iBotSkill );
	engine->ClientCmd_Unrestricted( szCommand );
}

int CFoFCreateGameDialog::GetSelectedInt(
	vgui::ComboBox *pCombo,
	const char *pszKey,
	int iFallback ) const
{
	KeyValues *pData = pCombo ? pCombo->GetActiveItemUserData() : NULL;
	return pData ? pData->GetInt( pszKey, iFallback ) : iFallback;
}

const char *CFoFCreateGameDialog::GetSelectedString(
	vgui::ComboBox *pCombo,
	const char *pszKey,
	const char *pszFallback ) const
{
	KeyValues *pData = pCombo ? pCombo->GetActiveItemUserData() : NULL;
	return pData ? pData->GetString( pszKey, pszFallback ) : pszFallback;
}

int CFoFCreateGameDialog::FindItemByInt(
	vgui::ComboBox *pCombo,
	const char *pszKey,
	int iValue ) const
{
	for ( int i = 0; i < pCombo->GetItemCount(); ++i )
	{
		KeyValues *pData = pCombo->GetItemUserData(
			pCombo->GetItemIDFromRow( i ) );
		if ( pData && pData->GetInt( pszKey ) == iValue )
			return i;
	}
	return -1;
}

void CFoFCreateGameDialog::SelectRow(
	vgui::ComboBox *pCombo,
	int iRow,
	int iFallbackRow )
{
	if ( !pCombo || pCombo->GetItemCount() <= 0 )
		return;
	if ( iRow < 0 || iRow >= pCombo->GetItemCount() )
		iRow = clamp( iFallbackRow, 0, pCombo->GetItemCount() - 1 );
	pCombo->SilentActivateItemByRow( iRow );
}
