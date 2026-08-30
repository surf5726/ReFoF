#include "cbase.h"
#include "fof/fof_launcher_panel.h"

#include "filesystem.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int FoFCompareLauncherCourseNames(
	const CUtlString *pLeft,
	const CUtlString *pRight )
{
	return Q_stricmp( pLeft->String(), pRight->String() );
}

static bool FoFParseCourseFileName(
	const char *pszFileName,
	FoFCourseEntry &entry )
{
	char script[MAX_PATH];
	Q_StripExtension( pszFileName, script, sizeof( script ) );
	if ( !script[0] )
		return false;

	char parts[MAX_PATH];
	Q_strncpy( parts, script, sizeof( parts ) );
	char *pAuthor = Q_strrchr( parts, '-' );
	if ( !pAuthor )
		return false;
	*pAuthor++ = '\0';
	char *pMap = Q_strrchr( parts, '-' );
	if ( !pMap )
		return false;
	*pMap++ = '\0';
	char *pCategory = Q_strrchr( parts, '-' );
	if ( !pCategory )
		return false;
	*pCategory++ = '\0';

	int group = -1;
	if ( !Q_stricmp( pCategory, "tut" ) )
		group = 0;
	else if ( !Q_stricmp( pCategory, "chg" ) )
		group = 1;
	else if ( !Q_stricmp( pCategory, "coop" ) )
		group = 2;
	if ( group < 0 || !parts[0] || !pMap[0] )
		return false;

	for ( char *pCharacter = parts; *pCharacter; ++pCharacter )
	{
		if ( *pCharacter == '_' )
			*pCharacter = ' ';
		else if ( *pCharacter >= 'a' && *pCharacter <= 'z' )
			*pCharacter = *pCharacter - 'a' + 'A';
	}

	// The server prepends "fof_scripts/courses/" but does not append an
	// extension when it loads fof_course_script.  Preserve the filename from
	// FindFirstEx (including .txt), matching the shipped launcher and the
	// Create Game course selector.
	entry.script = pszFileName;
	entry.title = parts;
	entry.category = pCategory;
	entry.map = pMap;
	entry.maxPlayers = 1;
	entry.group = group;
	entry.timesCompleted = 0;
	entry.topAward = 0;
	return true;
}

static int FoFReadCourseMaxPlayers( const char *pszScript )
{
	char path[MAX_PATH];
	Q_snprintf(
		path, sizeof( path ), "fof_scripts/courses/%s", pszScript );
	KeyValues *pCourse = new KeyValues( "Course" );
	int maxPlayers = 1;
	if ( pCourse->LoadFromFile( filesystem, path, "GAME" ) )
		maxPlayers = clamp( pCourse->GetInt( "max_players", 1 ), 1, 32 );
	pCourse->deleteThis();
	return maxPlayers;
}

CFoFCourseButton::CFoFCourseButton(
	vgui::Panel *pParent,
	const FoFCourseEntry &entry,
	int index )
	: BaseClass( pParent, "FoFCourseButton", "", pParent, "" )
	, m_Title( entry.title )
	, m_iTimesCompleted( MAX( entry.timesCompleted, 0 ) )
	, m_iTopAward( clamp( entry.topAward, 0, 3 ) )
	, m_iTexture( -1 )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hStatusFont( vgui::INVALID_FONT )
{
	char command[64];
	Q_snprintf( command, sizeof( command ), "course:%d", index );
	SetCommand( command );
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	DrawFocusBox( false );

	char material[MAX_PATH];
	Q_snprintf(
		material,
		sizeof( material ),
		"vgui/maps/menu_thumb_%s",
		entry.map.String() );
	m_iTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iTexture, material, true, false );
}

void CFoFCourseButton::SetCourseStats(
	int timesCompleted,
	int topAward )
{
	m_iTimesCompleted = MAX( timesCompleted, 0 );
	m_iTopAward = clamp( topAward, 0, 3 );
	Repaint();
}

CFoFCourseButton::~CFoFCourseButton()
{
	if ( m_iTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iTexture );
}

void CFoFCourseButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	// The shipped launcher uses ServerItem::count_label for both server
	// population and course-completion text.  That label explicitly uses the
	// ClientScheme MenuFontSmall handle, regardless of the parent panel scheme.
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

void CFoFCourseButton::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	const int statusTall = MAX( tall * 17 / 104, 1 );
	if ( m_iTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iTexture );
		// Course thumbnails are square VTFs whose lower quarter is transparent.
		// The original launcher samples only the 4:3 image payload and reserves
		// a separate strip for completion state.
		vgui::surface()->DrawTexturedSubRect(
			1,
			1,
			wide - 1,
			tall - statusTall,
			0.0f,
			0.0f,
			1.0f,
			0.75f );
	}
	vgui::surface()->DrawSetColor( 0, 0, 0, 30 );
	vgui::surface()->DrawFilledRect( 1, 1, wide - 1, tall - 1 );

	vgui::surface()->DrawSetColor( 0, 0, 0, 86 );
	vgui::surface()->DrawFilledRect(
		1, 1, wide - 1,
		MIN( tall - statusTall, 4 +
			vgui::surface()->GetFontTall( m_hTitleFont ) * 2 ) );
	vgui::surface()->DrawSetColor( 0, 0, 0, 180 );
	vgui::surface()->DrawFilledRect(
		1, tall - statusTall, wide - 1, tall - 1 );

	if ( IsArmed() )
	{
		vgui::surface()->DrawSetColor( 205, 20, 8, 175 );
		vgui::surface()->DrawFilledRect( 1, 1, wide - 1, tall - 1 );
	}

	FoFLauncherDrawWrappedTitle(
		m_Title.String(),
		m_hTitleFont,
		4,
		2,
		MAX( wide - 8, 1 ),
		2,
		Color( 245, 236, 0, 255 ) );

	wchar_t wszStatus[128];
	const wchar_t *pStatus = NULL;
	const char *pAwardToken = NULL;
	switch ( m_iTopAward )
	{
	case 1:
		pAwardToken = "#Challenge_Gold";
		break;
	case 2:
		pAwardToken = "#Challenge_Silver";
		break;
	case 3:
		pAwardToken = "#Challenge_Bronze";
		break;
	}
	if ( pAwardToken )
		pStatus = g_pVGuiLocalize->Find( pAwardToken );
	else if ( m_iTimesCompleted > 0 )
	{
		wchar_t wszCount[16];
		V_snwprintf(
			wszCount,
			ARRAYSIZE( wszCount ),
			L"%d",
			m_iTimesCompleted );
		const wchar_t *pFormat = g_pVGuiLocalize->Find(
			"#CourseStats_Completed" );
		if ( pFormat )
		{
			g_pVGuiLocalize->ConstructString(
				wszStatus,
				sizeof( wszStatus ),
				pFormat,
				1,
				wszCount );
			pStatus = wszStatus;
		}
	}
	if ( !pStatus )
	{
		pStatus = FoFLauncherLocalize(
			"#CourseStats_NotCompleted",
			wszStatus,
			sizeof( wszStatus ) );
	}
	const int statusWide = FoFLauncherTextWide( pStatus, m_hStatusFont );
	FoFLauncherDrawText(
		pStatus,
		m_hStatusFont,
		MAX( wide - statusWide - 3, 2 ),
		tall - statusTall + 1,
		Color( 225, 225, 215, 255 ) );

	vgui::surface()->DrawSetColor( 83, 54, 25, 255 );
	vgui::surface()->DrawOutlinedRect( 0, 0, wide, tall );
}

void CFoFServersPanel::ClearCourseEntries()
{
	for ( int i = 0; i < m_CourseButtons.Count(); ++i )
		delete m_CourseButtons[i];
	m_CourseButtons.Purge();
	for ( int i = 0; i < m_Courses.Count(); ++i )
		delete m_Courses[i];
	m_Courses.Purge();
}

void CFoFServersPanel::LoadCourses()
{
	CUtlVector<CUtlString> names;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pFileName = filesystem->FindFirstEx(
		"fof_scripts/courses/*.txt", "GAME", &findHandle );
	while ( pFileName )
	{
		if ( !filesystem->FindIsDirectory( findHandle ) )
			names.AddToTail( CUtlString( pFileName ) );
		pFileName = filesystem->FindNext( findHandle );
	}
	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		filesystem->FindClose( findHandle );
	names.Sort( FoFCompareLauncherCourseNames );

	for ( int i = names.Count() - 1; i >= 0; --i )
	{
		FoFCourseEntry parsed;
		if ( !FoFParseCourseFileName( names[i].String(), parsed ) )
			continue;
		parsed.maxPlayers = FoFReadCourseMaxPlayers(
			parsed.script.String() );
		FoFCourseEntry *pEntry = new FoFCourseEntry;
		*pEntry = parsed;
		const int index = m_Courses.AddToTail( pEntry );
		m_CourseButtons.AddToTail(
			new CFoFCourseButton( this, *pEntry, index ) );
	}

	ReloadCourseStats();
}

void CFoFServersPanel::ReloadCourseStats()
{
	KeyValues *pStats = new KeyValues( "CourseStats" );
	const bool loaded = pStats->LoadFromFile(
		filesystem, "fof_scripts/course_stats.txt", "GAME" );
	for ( int i = 0; i < m_Courses.Count(); ++i )
	{
		int timesCompleted = 0;
		int topAward = 0;
		if ( loaded )
		{
			KeyValues *pCourse = pStats->FindKey(
				m_Courses[i]->script.String(), false );
			if ( pCourse )
			{
				timesCompleted = MAX(
					pCourse->GetInt( "timesCompleted", 0 ), 0 );
				topAward = clamp(
					pCourse->GetInt( "topAward", 0 ), 0, 3 );
			}
		}

		m_Courses[i]->timesCompleted = timesCompleted;
		m_Courses[i]->topAward = topAward;
		if ( i < m_CourseButtons.Count() )
		{
			m_CourseButtons[i]->SetCourseStats(
				timesCompleted, topAward );
		}
	}
	pStats->deleteThis();
}

bool CFoFServersPanel::CourseMatchesSearch(
	const FoFCourseEntry &entry ) const
{
	return !m_szLastSearch[0] ||
		Q_stristr( entry.title.String(), m_szLastSearch ) ||
		Q_stristr( entry.map.String(), m_szLastSearch );
}

void CFoFServersPanel::LayoutCourses()
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

	for ( int group = 0; group < 3; ++group )
	{
		CUtlVector<int> matches;
		for ( int i = 0; i < m_Courses.Count(); ++i )
		{
			if ( m_Courses[i]->group == group &&
				CourseMatchesSearch( *m_Courses[i] ) )
			{
				matches.AddToTail( i );
			}
		}

		const int maxScroll = MAX( matches.Count() - columns, 0 );
		m_iGroupScroll[group] = clamp(
			m_iGroupScroll[group], 0, maxScroll );
		for ( int column = 0; column < columns; ++column )
		{
			const int visibleIndex = m_iGroupScroll[group] + column;
			if ( visibleIndex >= matches.Count() )
				break;
			CFoFCourseButton *pButton =
				m_CourseButtons[matches[visibleIndex]];
			pButton->SetBounds(
				contentX + column * ( cardWide + cardGap ),
				groupY + groupTitleTall,
				cardWide,
				m_iCourseCardTall );
			pButton->SetVisible( true );
		}

		// FoF uses a square 14x14 launcher control scaled from the 640x480
		// coordinate space; its width must not be derived from the card width.
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
		m_pGroupPrevious[group]->MoveToFront();
		m_pGroupNext[group]->MoveToFront();
		groupY += groupTitleTall +
			m_iCourseCardTall + m_iCourseGroupGap;
	}
}

void CFoFServersPanel::LaunchCourse( int index )
{
	if ( index < 0 || index >= m_Courses.Count() )
		return;
	const FoFCourseEntry &entry = *m_Courses[index];
	char password[16];
	Q_snprintf(
		password,
		sizeof( password ),
		"%d",
		random->RandomInt( 12130, 99454 ) );
	char command[1024];
	Q_snprintf(
		command,
		sizeof( command ),
		"disconnect\n"
		"wait\n"
		"wait\n"
		"sv_lan 1\n"
		"setmaster enable\n"
		"maxplayers 32\n"
		"sv_password \"%s\"\n"
		"hostname \"local sp course\"\n"
		"progress_enable\n"
		"fof_sv_currentmode 6\n"
		"mp_teamplay 1\n"
		"fof_sv_bot_dynamicjoin 0\n"
		"map %s\n"
		"fof_listenserver 1\n"
		"fof_sv_maxteams 4\n"
		"fof_course_script %s\n",
		password,
		entry.map.String(),
		entry.script.String() );
	engine->ClientCmd_Unrestricted( command );
}
