#include "cbase.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "hud.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "inputsystem/iinputsystem.h"
#include "KeyValues.h"
#include "engine/ivdebugoverlay.h"
#include "steam/steam_api.h"
#include "fof/fof_ai_editor.h"
#include "fof/fof_course_editor.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFCourseInstructionDefinition
{
	const char *name;
	const char *typeName;
	int descriptionType;
	Color color;
};

static const FoFCourseInstructionDefinition s_FoFCourseInstructions[] =
{
	{ "Player Spawn",     "player_spawn",   0, Color(   0, 255,   0, 200 ) },
	{ "Wave Script",      "wave_spawn",     1, Color(  30, 200, 120, 200 ) },
	{ "Wave S Dynamic",   "wave_spawn_dyn", 2, Color(  70, 230, 180, 200 ) },
	{ "Resupply Script",  "object_spawn",   3, Color( 193, 255, 153, 200 ) },
	{ "HUD Message",      "message",        4, Color( 255, 140,  40, 200 ) },
	{ "Goto",             "goto",           5, Color( 150,  10, 150, 200 ) },
	{ "Check Location",   "check_location", 8, Color( 255,  80,   0, 200 ) },
	{ "Check Capture",    "check_capture",  9, Color( 205,  80, 100, 200 ) },
	{ "Check Other",      NULL,             -1, Color( 155,   0,   0, 200 ) },
	{ "Check Timer",      "check_timer",   10, Color( 255, 255,  20, 200 ) },
};

struct FoFCourseDescriptionType
{
	const char *typeName;
	const char *displayName;
	int rootRow;
};

// In the original description table the numeric value is
// only an editor-side lookup; course files store the stable string name.
static const FoFCourseDescriptionType s_FoFCourseDescriptionTypes[] =
{
	{ "player_spawn",     "Player Spawn",        0 },
	{ "wave_spawn",       "Wave Script",         1 },
	{ "wave_spawn_dyn",   "Wave S Dynamic",      2 },
	{ "object_spawn",     "Resupply Script",     3 },
	{ "message",          "HUD Message",         4 },
	{ "goto",             "Goto",                5 },
	{ "stop",             "Stop",                8 },
	{ "check_enemies",    "Check Enemies",       8 },
	{ "check_location",   "Check Location",      6 },
	{ "check_capture",    "Check Capture",       7 },
	{ "check_timer",      "Check Timer",         9 },
	{ "check_train",      "Check Other",         8 },
	{ "check_wait",       "Check Wait",          8 },
	{ "game_end",         "Game End",            8 },
	{ "entity_io",        "Entity IO",           8 },
	{ "check_sp",         "Check Singleplayer",  8 },
	{ "disable_check",    "Disable Check",       8 },
	{ "give_equip",       "Give equipment",      8 },
	{ "check_bot_orders", "Check Bot Orders",    8 },
	{ "play_audio",       "Play Audio",          8 },
	{ "challenge_end",    "Challenge End",       8 },
};

static int FoFCourseFindDescriptionType( const char *typeName )
{
	if ( !typeName || !typeName[0] )
		return -1;
	for ( int index = 0;
		index < ARRAYSIZE( s_FoFCourseDescriptionTypes ); ++index )
	{
		if ( !Q_stricmp(
			typeName, s_FoFCourseDescriptionTypes[index].typeName ) )
		{
			return index;
		}
	}
	return -1;
}

static int FoFCourseLegacyRootType( int rootType )
{
	return rootType >= 0 && rootType < ARRAYSIZE( s_FoFCourseInstructions ) ?
		( s_FoFCourseInstructions[rootType].descriptionType >= 0 ?
			s_FoFCourseInstructions[rootType].descriptionType : 7 ) : 0;
}

static void FoFCourseInitializeInstruction(
	FoFCourseEditorInstruction &instruction, int descriptionType )
{
	Q_memset( &instruction, 0, sizeof( instruction ) );
	instruction.type = clamp( descriptionType, 0,
		ARRAYSIZE( s_FoFCourseDescriptionTypes ) - 1 );
	Q_strncpy( instruction.entryLabel, "0",
		sizeof( instruction.entryLabel ) );
	Q_strncpy( instruction.typeName,
		s_FoFCourseDescriptionTypes[instruction.type].typeName,
		sizeof( instruction.typeName ) );
}

static const ButtonCode_t s_FoFCourseNumberKeys[10] =
{
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
	KEY_6, KEY_7, KEY_8, KEY_9, KEY_0
};

static void FoFCourseOpenHelp()
{
	ISteamFriends *friends = steamapicontext ?
		steamapicontext->SteamFriends() : NULL;
	if ( friends )
	{
		friends->ActivateGameOverlayToWebPage(
			"http://steamcommunity.com/sharedfiles/filedetails/?id=788516935" );
	}
}

static void FoFCourseAddComboItem(
	vgui::ComboBox *combo, const char *text, int value )
{
	if ( !combo )
		return;
	KeyValues *data = new KeyValues( "data" );
	data->SetInt( "value", value );
	combo->AddItem( text, data );
	data->deleteThis();
}

static int FoFCourseGetComboValue(
	vgui::ComboBox *combo, int fallback )
{
	KeyValues *data = combo ? combo->GetActiveItemUserData() : NULL;
	return data ? data->GetInt( "value", fallback ) : fallback;
}

static void FoFCourseActivateComboValue(
	vgui::ComboBox *combo, int value )
{
	if ( !combo )
		return;
	for ( int row = 0; row < combo->GetItemCount(); ++row )
	{
		const int itemID = combo->GetItemIDFromRow( row );
		KeyValues *data = combo->GetItemUserData( itemID );
		if ( data && data->GetInt( "value", -9999 ) == value )
		{
			combo->SilentActivateItem( itemID );
			return;
		}
	}
}

static const char *FoFCourseScriptDirectory()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() && currentMode.GetInt() == 4 ?
		"fof_scripts/elimination" : "fof_scripts/courses";
}

static void FoFCourseBuildScriptPath(
	char *path, int pathSize, const char *filename )
{
	Q_snprintf( path, pathSize, "%s/%s",
		FoFCourseScriptDirectory(), filename ? filename : "" );
}

static void FoFCourseBuildSearchPattern( char *pattern, int patternSize )
{
	Q_snprintf( pattern, patternSize, "%s/*.txt",
		FoFCourseScriptDirectory() );
}

DECLARE_HUDELEMENT( CHudFoFCourseEditor );

CHudFoFCourseEditor::CHudFoFCourseEditor( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudCourseEditor" )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hRowFont( vgui::INVALID_FONT )
	, m_hSmallFont( vgui::INVALID_FONT )
	, m_bActive( false )
	, m_bSaveLoadVisible( false )
	, m_bPanelEditing( false )
	, m_bShowHelp( true )
	, m_iSelected( 0 )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
	, m_pCourseList( NULL )
	, m_pFilenameEntry( NULL )
	, m_pLoadButton( NULL )
	, m_pSaveButton( NULL )
	, m_pHumanTeam( NULL )
	, m_pMaxPlayers( NULL )
	, m_pBotAlliance( NULL )
	, m_pTotalEnemies( NULL )
	, m_bMouseLeftDown( false )
	, m_bMouseRightDown( false )
	, m_bWheelUpDown( false )
	, m_bWheelDownDown( false )
	, m_bAltDown( false )
	, m_bDeleteDown( false )
	, m_bInsertDown( false )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetVisible( false );

	m_pCourseList = new vgui::ComboBox(
		this, "available courses", 10, false );
	m_pFilenameEntry = new vgui::TextEntry( this, "course filename" );
	m_pLoadButton = new vgui::Button(
		this, "course load", "Load", this, "course_load" );
	m_pSaveButton = new vgui::Button(
		this, "course save", "Save", this, "course_save" );
	m_pHumanTeam = new vgui::ComboBox(
		this, "Human Team", 5, false );
	m_pMaxPlayers = new vgui::ComboBox(
		this, "Max Players", 9, false );
	m_pBotAlliance = new vgui::ComboBox(
		this, "Bot Alliance", 5, false );
	m_pTotalEnemies = new vgui::TextEntry( this, "Total Enemies" );
	m_pTotalEnemies->SetAllowNumericInputOnly( true );
	m_pTotalEnemies->SetMaximumCharCount( 3 );
	m_pTotalEnemies->SetText( "0" );

	FoFCourseAddComboItem( m_pHumanTeam, "auto", 0 );
	FoFCourseAddComboItem( m_pHumanTeam, "Vigilantes", 2 );
	FoFCourseAddComboItem( m_pHumanTeam, "Desperados", 3 );
	FoFCourseAddComboItem( m_pHumanTeam, "Bandidos", 4 );
	FoFCourseAddComboItem( m_pHumanTeam, "Rangers", 5 );
	m_pHumanTeam->SilentActivateItemByRow( 0 );

	FoFCourseAddComboItem( m_pMaxPlayers, "SP - Tutorial", 1 );
	FoFCourseAddComboItem( m_pMaxPlayers, "SP - Challenge", 1 );
	FoFCourseAddComboItem( m_pMaxPlayers, "SP - Mission", 1 );
	for ( int players = 2; players <= 6; ++players )
	{
		char label[32];
		Q_snprintf( label, sizeof( label ), "%d Co-op", players );
		FoFCourseAddComboItem( m_pMaxPlayers, label, players );
	}
	m_pMaxPlayers->SilentActivateItemByRow( 2 );

	FoFCourseAddComboItem( m_pBotAlliance, "auto", 0 );
	FoFCourseAddComboItem( m_pBotAlliance, "none", 1 );
	FoFCourseAddComboItem( m_pBotAlliance, "allied", 2 );
	FoFCourseAddComboItem( m_pBotAlliance, "hostile", 3 );
	m_pBotAlliance->SilentActivateItemByRow( 0 );

	Q_memset( m_bNumberDown, 0, sizeof( m_bNumberDown ) );
	m_szMapName[0] = '\0';
	m_szFilename[0] = '\0';
	UpdateSaveLoadControls();
}

void CHudFoFCourseEditor::Init()
{
	Reset();
}

void CHudFoFCourseEditor::Reset()
{
	if ( m_bActive )
		SetEditorActive( false );
	m_bSaveLoadVisible = false;
	m_bPanelEditing = false;
	m_bShowHelp = true;
	m_iSelected = 0;
}

void CHudFoFCourseEditor::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	UpdateLayout();
}

bool CHudFoFCourseEditor::ShouldDraw()
{
	return m_bActive && CHudElement::ShouldDraw();
}

void CHudFoFCourseEditor::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hTitleFont = scheme->GetFont( "DefaultFoF", true );
	m_hRowFont = scheme->GetFont( "Default", true );
	m_hSmallFont = scheme->GetFont( "DefaultFoF", true );
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = m_hSmallFont;
	if ( m_hRowFont == vgui::INVALID_FONT )
		m_hRowFont = m_hSmallFont;
	m_pCourseList->SetFont( m_hSmallFont );
	m_pFilenameEntry->SetFont( m_hSmallFont );
	m_pLoadButton->SetFont( m_hSmallFont );
	m_pSaveButton->SetFont( m_hSmallFont );
	m_pHumanTeam->SetFont( m_hSmallFont );
	m_pMaxPlayers->SetFont( m_hSmallFont );
	m_pBotAlliance->SetFont( m_hSmallFont );
	m_pTotalEnemies->SetFont( m_hSmallFont );
	UpdateLayout();
}

void CHudFoFCourseEditor::PerformLayout()
{
	BaseClass::PerformLayout();
	UpdateLayout();
}

void CHudFoFCourseEditor::OnThink()
{
	BaseClass::OnThink();
	if ( !m_bActive )
		return;
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		UpdateLayout();
	}
	HandleInput();
}

void CHudFoFCourseEditor::Paint()
{
	if ( !m_bActive )
		return;

	DrawPlacementPreview();
	DrawPlacedInstructions();

	const int wide = GetWide();
	const int inset = FoFEditorScale( 1.0f );
	const int cardRadius = MAX( 2, FoFEditorScale( 3.0f ) );
	int rowY = 0;
	if ( m_bShowHelp )
	{
		const int helpInset = FoFEditorScale( 2.0f );
		const int helpBottom = FoFEditorDrawLocalizedWrappedText(
			m_hSmallFont, Color( 0, 0, 0, 0 ),
			helpInset, helpInset, wide - helpInset * 2,
			"#FoF_Course_Editor_Help",
			"0-9: select / LMB: spawn / RMB: panel edit / MOUSE WHEEL: scroll / DEL remove all / ALT save-load / INSERT Help" );
		const int helpTall = helpBottom + helpInset;
		FoFEditorDrawRoundedRect(
			0, 0, wide, helpTall, cardRadius, Color( 70, 70, 65, 190 ) );
		FoFEditorDrawLocalizedWrappedText(
			m_hSmallFont, Color( 255, 20, 235, 255 ),
			helpInset, helpInset, wide - helpInset * 2,
			"#FoF_Course_Editor_Help",
			"0-9: select / LMB: spawn / RMB: panel edit / MOUSE WHEEL: scroll / DEL remove all / ALT save-load / INSERT Help" );
		rowY = helpTall + FoFEditorScale( 2.0f );
	}

	const int rowStep = FoFEditorScale( 17.0f );
	const int cardTall = FoFEditorScale( 15.0f );
	for ( int row = 0; row < ARRAYSIZE( s_FoFCourseInstructions ); ++row )
	{
		const int y = rowY + row * rowStep;
		FoFEditorDrawRoundedRect(
			0, y, wide, cardTall, cardRadius,
			s_FoFCourseInstructions[row].color );

		char label[128];
		Q_snprintf( label, sizeof( label ), "%d. %s",
			row + 1, s_FoFCourseInstructions[row].name );
		FoFEditorDrawText(
			m_hRowFont,
			row == m_iSelected ?
				Color( 255, 255, 255, 255 ) : Color( 85, 85, 85, 255 ),
			inset, y, label );
	}

	if ( m_bSaveLoadVisible )
	{
		const int panelY = rowY + rowStep * 10 + FoFEditorScale( 6.0f );
		const int panelTall = FoFEditorScale( 128.0f );
		vgui::surface()->DrawSetColor( Color( 15, 15, 15, 190 ) );
		vgui::surface()->DrawFilledRect( 0, panelY, wide, panelY + panelTall );
		vgui::surface()->DrawSetColor( Color( 150, 150, 150, 220 ) );
		vgui::surface()->DrawOutlinedRect( 0, panelY, wide, panelY + panelTall );

		FoFEditorDrawText(
			m_hSmallFont, Color( 245, 245, 245, 255 ), inset,
			panelY + FoFEditorScale( 4.0f ),
			"Press right mouse button to use this panel" );
		FoFEditorDrawText(
			m_hSmallFont, Color( 210, 210, 210, 255 ), inset,
			panelY + FoFEditorScale( 72.0f ), "Human Team" );
		FoFEditorDrawText(
			m_hSmallFont, Color( 210, 210, 210, 255 ),
			FoFEditorScale( 133.0f ), panelY + FoFEditorScale( 72.0f ),
			"Max Players" );
		FoFEditorDrawText(
			m_hSmallFont, Color( 210, 210, 210, 255 ), inset,
			panelY + FoFEditorScale( 99.0f ), "Bot Alliance" );
		FoFEditorDrawText(
			m_hSmallFont, Color( 210, 210, 210, 255 ),
			FoFEditorScale( 133.0f ), panelY + FoFEditorScale( 99.0f ),
			"Total Enemies" );

		char line[128];
		Q_snprintf( line, sizeof( line ),
			"Instructions placed: %d", m_Instructions.Count() );
		FoFEditorDrawText(
			m_hSmallFont, Color( 245, 220, 75, 255 ), inset,
			panelY + FoFEditorScale( 115.0f ), line );
	}
}

void CHudFoFCourseEditor::OnCommand( const char *command )
{
	if ( command && !Q_stricmp( command, "course_save" ) )
	{
		char filename[MAX_PATH];
		m_pFilenameEntry->GetText( filename, sizeof( filename ) );
		SaveCourse( filename );
		return;
	}
	if ( command && !Q_stricmp( command, "course_load" ) )
	{
		char filename[MAX_PATH];
		filename[0] = '\0';
		KeyValues *data = m_pCourseList->GetActiveItemUserData();
		if ( data )
			Q_strncpy( filename,
				data->GetString( "filename", "" ), sizeof( filename ) );
		if ( !filename[0] )
			m_pFilenameEntry->GetText( filename, sizeof( filename ) );
		if ( filename[0] )
		{
			m_pFilenameEntry->SetText( filename );
			LoadCourse( filename );
		}
		return;
	}
	BaseClass::OnCommand( command );
}

bool CHudFoFCourseEditor::IsActive() const
{
	return m_bActive;
}

void CHudFoFCourseEditor::SetEditorActive( bool active )
{
	if ( active == m_bActive )
		return;
	m_bActive = active;
	SetVisible( active );
	if ( active )
	{
		FoFEditorGetMapName( m_szMapName, sizeof( m_szMapName ) );
		UpdateDefaultFilename();
		m_pFilenameEntry->SetText( m_szFilename );
		RefreshCourseList();
		m_iSelected = 0;
		m_bSaveLoadVisible = false;
		SetPanelEditing( false );
		UpdateLayout();
	}
	else
	{
		SetPanelEditing( false );
		m_bSaveLoadVisible = false;
		UpdateSaveLoadControls();
	}
}

void CHudFoFCourseEditor::ToggleEditor()
{
	SetEditorActive( !m_bActive );
}

void CHudFoFCourseEditor::UpdateLayout()
{
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	const int wide = FoFEditorScale(
		m_bSaveLoadVisible ? 260.0f : 150.0f );
	SetBounds(
		MAX( 0, ScreenWidth() - wide - FoFEditorScale( 10.0f ) ),
		FoFEditorScale( 16.0f ),
		wide,
		MAX( FoFEditorScale( 350.0f ), ScreenHeight() - FoFEditorScale( 24.0f ) ) );
	LayoutSaveLoadControls();
}

void CHudFoFCourseEditor::DrawPlacementPreview()
{
	if ( !debugoverlay || m_iSelected < 0 ||
		m_iSelected >= ARRAYSIZE( s_FoFCourseInstructions ) )
	{
		return;
	}

	Vector origin;
	QAngle angles;
	if ( !FoFEditorTracePlacement( 0.0f, origin, angles ) )
		return;

	Vector mins;
	Vector maxs;
	if ( m_iSelected == 0 )
	{
		mins.Init( -16.0f, -16.0f, 0.0f );
		maxs.Init( 16.0f, 16.0f, 32.0f );
	}
	else
	{
		mins.Init( -8.0f, -8.0f, 0.0f );
		maxs.Init( 8.0f, 8.0f, 16.0f );
	}

	debugoverlay->AddBoxOverlay2(
		origin, mins, maxs, QAngle( 0.0f, 0.0f, 0.0f ),
		s_FoFCourseInstructions[m_iSelected].color,
		Color( 10, 10, 10, 255 ), 0.01f );
}

void CHudFoFCourseEditor::DrawPlacedInstructions()
{
	if ( !debugoverlay )
		return;

	for ( int index = 0; index < m_Instructions.Count(); ++index )
	{
		const FoFCourseEditorInstruction &instruction = m_Instructions[index];
		Vector mins;
		Vector maxs;
		if ( instruction.type == 0 )
		{
			mins.Init( -16.0f, -16.0f, 0.0f );
			maxs.Init( 16.0f, 16.0f, 32.0f );
		}
		else
		{
			mins.Init( -8.0f, -8.0f, 0.0f );
			maxs.Init( 8.0f, 8.0f, 16.0f );
		}

		debugoverlay->AddBoxOverlay2(
			instruction.origin, mins, maxs,
			QAngle( 0.0f, 0.0f, 0.0f ),
			GetInstructionColor( instruction.type ),
			Color( 10, 10, 10, 255 ), 0.01f );
	}
}

void CHudFoFCourseEditor::RefreshCourseList()
{
	m_pCourseList->RemoveAll();
	KeyValues *empty = new KeyValues( "data" );
	empty->SetString( "filename", "" );
	m_pCourseList->AddItem( "no course selected", empty );
	empty->deleteThis();

	int selectedRow = 0;
	int row = 1;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	char searchPattern[MAX_PATH];
	FoFCourseBuildSearchPattern( searchPattern, sizeof( searchPattern ) );
	const char *found = filesystem->FindFirstEx(
		searchPattern, "MOD", &findHandle );
	while ( found )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
		{
			const char *filename = V_UnqualifiedFileName( found );
			KeyValues *data = new KeyValues( "data" );
			data->SetString( "filename", filename );
			m_pCourseList->AddItem( filename, data );
			data->deleteThis();
			if ( !Q_stricmp( filename, m_szFilename ) )
				selectedRow = row;
			++row;
		}
		found = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );
	m_pCourseList->SilentActivateItemByRow( selectedRow );
}

void CHudFoFCourseEditor::UpdateSaveLoadControls()
{
	const bool visible = m_bActive && m_bSaveLoadVisible;
	vgui::Panel *controls[] =
	{
		m_pCourseList, m_pFilenameEntry, m_pLoadButton, m_pSaveButton,
		m_pHumanTeam, m_pMaxPlayers, m_pBotAlliance, m_pTotalEnemies
	};
	for ( int index = 0; index < ARRAYSIZE( controls ); ++index )
	{
		if ( !controls[index] )
			continue;
		controls[index]->SetVisible( visible );
		controls[index]->SetEnabled( visible && m_bPanelEditing );
		controls[index]->SetMouseInputEnabled( visible && m_bPanelEditing );
		controls[index]->SetKeyBoardInputEnabled( visible && m_bPanelEditing );
	}
	SetMouseInputEnabled( visible && m_bPanelEditing );
	SetKeyBoardInputEnabled( visible && m_bPanelEditing );
}

void CHudFoFCourseEditor::LayoutSaveLoadControls()
{
	const int rowY = FoFEditorScale( m_bShowHelp ? 34.0f : 0.0f );
	const int panelY = rowY + FoFEditorScale( 17.0f ) * 10 +
		FoFEditorScale( 6.0f );
	const int inset = FoFEditorScale( 5.0f );
	const int fieldTall = MAX( FoFEditorScale( 15.0f ), 18 );
	const int halfWide = FoFEditorScale( 122.0f );
	const int rightX = FoFEditorScale( 133.0f );

	m_pCourseList->SetBounds(
		inset, panelY + FoFEditorScale( 19.0f ),
		FoFEditorScale( 199.0f ), fieldTall );
	m_pLoadButton->SetBounds(
		FoFEditorScale( 207.0f ), panelY + FoFEditorScale( 19.0f ),
		FoFEditorScale( 48.0f ), fieldTall );
	m_pFilenameEntry->SetBounds(
		inset, panelY + FoFEditorScale( 39.0f ),
		FoFEditorScale( 199.0f ), fieldTall );
	m_pSaveButton->SetBounds(
		FoFEditorScale( 207.0f ), panelY + FoFEditorScale( 39.0f ),
		FoFEditorScale( 48.0f ), fieldTall );
	m_pHumanTeam->SetBounds(
		inset, panelY + FoFEditorScale( 82.0f ), halfWide, fieldTall );
	m_pMaxPlayers->SetBounds(
		rightX, panelY + FoFEditorScale( 82.0f ), halfWide, fieldTall );
	m_pBotAlliance->SetBounds(
		inset, panelY + FoFEditorScale( 109.0f ), halfWide, fieldTall );
	m_pTotalEnemies->SetBounds(
		rightX, panelY + FoFEditorScale( 109.0f ), halfWide, fieldTall );
	UpdateSaveLoadControls();
}

void CHudFoFCourseEditor::SetPanelEditing( bool editing )
{
	if ( !m_bSaveLoadVisible )
		editing = false;
	if ( m_bPanelEditing == editing )
	{
		UpdateSaveLoadControls();
		return;
	}

	m_bPanelEditing = editing;
	UpdateSaveLoadControls();
	if ( editing )
	{
		vgui::surface()->SetCursorAlwaysVisible( true );
		m_pFilenameEntry->RequestFocus();
	}
	else
	{
		vgui::surface()->SetCursorAlwaysVisible( false );
	}
}

void CHudFoFCourseEditor::HandleInput()
{
	for ( int row = 0; row < 10; ++row )
	{
		if ( FoFEditorButtonPressed(
			s_FoFCourseNumberKeys[row], m_bNumberDown[row] ) )
		{
			SelectVisibleRow( row );
		}
	}

	// The shipped panel has ten root instruction categories.  Wheel events
	// therefore wrap the selection instead of scrolling an empty page.
	if ( FoFEditorButtonPressed( MOUSE_WHEEL_UP, m_bWheelUpDown ) )
		m_iSelected = ( m_iSelected + 9 ) % 10;
	if ( FoFEditorButtonPressed( MOUSE_WHEEL_DOWN, m_bWheelDownDown ) )
		m_iSelected = ( m_iSelected + 1 ) % 10;

	const bool altDown = inputsystem &&
		( inputsystem->IsButtonDown( KEY_LALT ) ||
		  inputsystem->IsButtonDown( KEY_RALT ) ) ||
		vgui::input()->IsKeyDown( KEY_LALT ) ||
		vgui::input()->IsKeyDown( KEY_RALT );
	if ( altDown && !m_bAltDown )
	{
		SetPanelEditing( false );
		m_bSaveLoadVisible = !m_bSaveLoadVisible;
		UpdateLayout();
	}
	m_bAltDown = altDown;

	if ( FoFEditorButtonPressed( KEY_INSERT, m_bInsertDown ) )
		FoFCourseOpenHelp();
	if ( FoFEditorButtonPressed( KEY_DELETE, m_bDeleteDown ) )
	{
		m_Instructions.RemoveAll();
		Msg( "[FoF] all placed course instructions removed\n" );
	}
	if ( FoFEditorButtonPressed( MOUSE_RIGHT, m_bMouseRightDown ) &&
		m_bSaveLoadVisible )
	{
		SetPanelEditing( !m_bPanelEditing );
	}
	if ( FoFEditorButtonPressed( MOUSE_LEFT, m_bMouseLeftDown ) &&
		!( m_bSaveLoadVisible && m_bPanelEditing ) )
	{
		AddInstruction();
	}
}

void CHudFoFCourseEditor::SelectVisibleRow( int visibleRow )
{
	if ( visibleRow >= 0 && visibleRow < 10 )
		m_iSelected = visibleRow;
}

void CHudFoFCourseEditor::AddInstruction()
{
	FoFCourseEditorInstruction instruction;
	FoFCourseInitializeInstruction(
		instruction, FoFCourseLegacyRootType( m_iSelected ) );
	if ( !FoFEditorTracePlacement(
		0.0f, instruction.origin, instruction.angles ) )
	{
		return;
	}
	m_Instructions.AddToTail( instruction );
	Msg( "[FoF] course instruction %s placed at %.0f %.0f %.0f\n",
		s_FoFCourseDescriptionTypes[instruction.type].displayName,
		instruction.origin.x, instruction.origin.y, instruction.origin.z );
}

void CHudFoFCourseEditor::UpdateDefaultFilename()
{
	Q_snprintf( m_szFilename, sizeof( m_szFilename ),
		"-sp-%s-course.txt", m_szMapName[0] ? m_szMapName : "course" );
}

void CHudFoFCourseEditor::NormalizeFilename(
	const char *input, char *output, int outputSize ) const
{
	if ( !output || outputSize <= 0 )
		return;
	Q_strncpy( output, input && input[0] ? input : m_szFilename, outputSize );
	for ( char *character = output; *character; ++character )
	{
		if ( *character == '/' || *character == '\\' || *character == ':' ||
			*character == '*' || *character == '?' || *character == '"' ||
			*character == '<' || *character == '>' || *character == '|' ||
			(unsigned char)*character < 32 )
		{
			*character = '_';
		}
	}
	const char *extension = V_GetFileExtension( output );
	if ( !extension || Q_stricmp( extension, "txt" ) )
		Q_strncat( output, ".txt", outputSize );
}

bool CHudFoFCourseEditor::SaveCourse( const char *filename )
{
	if ( !m_bActive )
		return false;

	char cleanName[MAX_PATH];
	NormalizeFilename( filename, cleanName, sizeof( cleanName ) );
	char path[MAX_PATH];
	FoFCourseBuildScriptPath( path, sizeof( path ), cleanName );

	KeyValues *root = new KeyValues( "Course" );
	root->SetInt( "player_team",
		FoFCourseGetComboValue( m_pHumanTeam, 0 ) );
	root->SetInt( "max_players",
		FoFCourseGetComboValue( m_pMaxPlayers, 1 ) );
	root->SetInt( "bot_alliance",
		FoFCourseGetComboValue( m_pBotAlliance, 0 ) );
	char totalEnemies[16];
	m_pTotalEnemies->GetText( totalEnemies, sizeof( totalEnemies ) );
	root->SetInt( "total_enemies", MAX( 0, Q_atoi( totalEnemies ) ) );
	for ( int index = 0; index < m_Instructions.Count(); ++index )
	{
		const FoFCourseEditorInstruction &instruction = m_Instructions[index];
		KeyValues *item = new KeyValues(
			instruction.entryLabel[0] ? instruction.entryLabel : "0" );
		if ( instruction.hasData )
			item->SetString( "data", instruction.data );
		if ( instruction.hasValue )
			item->SetString( "value", instruction.value );
		if ( instruction.hasOutput )
			item->SetString( "output", instruction.output );
		const int safeType = clamp( instruction.type, 0,
			ARRAYSIZE( s_FoFCourseDescriptionTypes ) - 1 );
		item->SetString( "type", instruction.typeName[0] ?
			instruction.typeName :
			s_FoFCourseDescriptionTypes[safeType].typeName );
		item->SetFloat( "origin_x", instruction.origin.x );
		item->SetFloat( "origin_y", instruction.origin.y );
		item->SetFloat( "origin_z", instruction.origin.z );
		item->SetFloat( "dir_x", instruction.angles.x );
		item->SetFloat( "dir_y", instruction.angles.y );
		item->SetFloat( "dir_z", instruction.angles.z );
		root->AddSubKey( item );
	}

	filesystem->CreateDirHierarchy( FoFCourseScriptDirectory(), "MOD" );
	const bool saved = root->SaveToFile( filesystem, path, "MOD" );
	root->deleteThis();
	if ( saved )
	{
		Q_strncpy( m_szFilename, cleanName, sizeof( m_szFilename ) );
		m_pFilenameEntry->SetText( m_szFilename );
		RefreshCourseList();
		Msg( "Course %s saved successfully\n", cleanName );
	}
	else
	{
		Warning( "[FoF] unable to save course %s\n", path );
	}
	return saved;
}

bool CHudFoFCourseEditor::LoadCourse( const char *filename )
{
	if ( !m_bActive )
		return false;

	char cleanName[MAX_PATH];
	NormalizeFilename( filename, cleanName, sizeof( cleanName ) );
	char path[MAX_PATH];
	FoFCourseBuildScriptPath( path, sizeof( path ), cleanName );

	KeyValues *root = new KeyValues( "Course" );
	if ( !root->LoadFromFile( filesystem, path, "MOD" ) &&
		!root->LoadFromFile( filesystem, path, "GAME" ) )
	{
		root->deleteThis();
		Warning( "[FoF] unable to load course %s\n", path );
		return false;
	}

	m_Instructions.RemoveAll();
	const int playerTeam = root->FindKey( "player_team", false ) ?
		root->GetInt( "player_team", 0 ) :
		root->GetInt( "human_team", 0 );
	FoFCourseActivateComboValue(
		m_pHumanTeam, playerTeam );
	FoFCourseActivateComboValue(
		m_pMaxPlayers, root->GetInt( "max_players", 1 ) );
	FoFCourseActivateComboValue(
		m_pBotAlliance, root->GetInt( "bot_alliance", 0 ) );
	char totalEnemies[16];
	Q_snprintf( totalEnemies, sizeof( totalEnemies ), "%d",
		MAX( 0, root->GetInt( "total_enemies", 0 ) ) );
	m_pTotalEnemies->SetText( totalEnemies );
	for ( KeyValues *item = root->GetFirstTrueSubKey();
		item; item = item->GetNextTrueSubKey() )
	{
		FoFCourseEditorInstruction instruction;
		const char *typeName = item->GetString( "type", "" );
		int descriptionType = FoFCourseFindDescriptionType( typeName );
		if ( descriptionType < 0 && !Q_stricmp( item->GetName(), "instruction" ) )
			descriptionType = FoFCourseLegacyRootType( item->GetInt( "type", 0 ) );
		if ( descriptionType < 0 )
			continue;

		FoFCourseInitializeInstruction( instruction, descriptionType );
		if ( Q_stricmp( item->GetName(), "instruction" ) )
		{
			Q_strncpy( instruction.entryLabel, item->GetName(),
				sizeof( instruction.entryLabel ) );
		}
		instruction.hasData = item->FindKey( "data", false ) != NULL;
		instruction.hasValue = item->FindKey( "value", false ) != NULL;
		instruction.hasOutput = item->FindKey( "output", false ) != NULL;
		if ( instruction.hasData )
			Q_strncpy( instruction.data, item->GetString( "data", "" ),
				sizeof( instruction.data ) );
		if ( instruction.hasValue )
			Q_strncpy( instruction.value, item->GetString( "value", "" ),
				sizeof( instruction.value ) );
		if ( instruction.hasOutput )
			Q_strncpy( instruction.output, item->GetString( "output", "" ),
				sizeof( instruction.output ) );
		instruction.origin.Init(
			item->GetFloat( "origin_x", 0.0f ),
			item->GetFloat( "origin_y", 0.0f ),
			item->GetFloat( "origin_z", 0.0f ) );
		instruction.angles.Init(
			item->GetFloat( "dir_x", 0.0f ),
			item->GetFloat( "dir_y", 0.0f ),
			item->GetFloat( "dir_z", 0.0f ) );
		m_Instructions.AddToTail( instruction );
	}
	root->deleteThis();
	Q_strncpy( m_szFilename, cleanName, sizeof( m_szFilename ) );
	m_pFilenameEntry->SetText( m_szFilename );
	Msg( "=== %i course instructions loaded ===\n", m_Instructions.Count() );
	return true;
}

Color CHudFoFCourseEditor::GetInstructionColor( int type ) const
{
	const int descriptionType = clamp( type, 0,
		ARRAYSIZE( s_FoFCourseDescriptionTypes ) - 1 );
	const int rootRow = clamp(
		s_FoFCourseDescriptionTypes[descriptionType].rootRow,
		0, ARRAYSIZE( s_FoFCourseInstructions ) - 1 );
	return s_FoFCourseInstructions[rootRow].color;
}

bool FoFCourseEditorIsActive()
{
	CHudFoFCourseEditor *editor = GET_HUDELEMENT( CHudFoFCourseEditor );
	return editor && editor->IsActive();
}

void FoFCourseEditorClose()
{
	CHudFoFCourseEditor *editor = GET_HUDELEMENT( CHudFoFCourseEditor );
	if ( editor )
		editor->SetEditorActive( false );
}

CON_COMMAND( fof_course_editor, "Toggle the FoF listen-server course editor." )
{
	if ( !FoFEditorCheatsEnabled() || FoFAIEditorIsActive() )
		return;

	CHudFoFCourseEditor *editor = GET_HUDELEMENT( CHudFoFCourseEditor );
	if ( editor )
		editor->ToggleEditor();
}

CON_COMMAND( fof_course_save, "Save the active FoF course editor document." )
{
	CHudFoFCourseEditor *editor = GET_HUDELEMENT( CHudFoFCourseEditor );
	if ( editor && editor->IsActive() )
		editor->SaveCourse( args.ArgC() >= 2 ? args[1] : NULL );
}

CON_COMMAND( fof_course_load, "Load an FoF course editor document." )
{
	CHudFoFCourseEditor *editor = GET_HUDELEMENT( CHudFoFCourseEditor );
	if ( editor && editor->IsActive() && args.ArgC() >= 2 )
		editor->LoadCourse( args[1] );
}
