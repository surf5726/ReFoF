// FoF course objective panel used by the four built-in tutorial courses.

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "cdll_util.h"
#include "fof/fof_hints.h"
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include "filesystem.h"
#include "KeyValues.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "cdll_int.h"
#include "vgui_controls/Controls.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int FoFCourseHintScale( float value )
{
	return RoundFloatToInt(
		( (float)ScreenHeight() / 480.0f ) * value );
}

static void FoFCourseHintLocalize(
	const char *text, wchar_t *buffer, int bufferBytes )
{
	if ( !buffer || bufferBytes < (int)sizeof( wchar_t ) )
		return;

	buffer[0] = L'\0';
	if ( !text || !text[0] )
		return;

	if ( text[0] == '#' )
	{
		const wchar_t *localized = g_pVGuiLocalize->Find( text );
		if ( localized )
		{
			V_wcsncpy( buffer, localized, bufferBytes );
			return;
		}
	}

	g_pVGuiLocalize->ConvertANSIToUnicode( text, buffer, bufferBytes );
}

class CHudFoFCourseHint : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFCourseHint, vgui::Panel );

public:
	CHudFoFCourseHint( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnThink();

	void Receive( const char *title, const char *body, int mode );

private:
	void LayoutForScreen();
	void UpdateBodyWrap();

	vgui::Label *m_pTitle;
	vgui::Label *m_pBody;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hBodyFont;
	vgui::HFont m_hLargeBodyFont;
	wchar_t m_wszTitle[1024];
	wchar_t m_wszBody[1024];
	int m_iMode;
	float m_flLastUpdate;
	bool m_bHasTitle;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

DECLARE_HUDELEMENT( CHudFoFCourseHint );

CHudFoFCourseHint::CHudFoFCourseHint( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HUDCourseHint" )
	, m_pTitle( NULL )
	, m_pBody( NULL )
	, m_hTitleFont( vgui::INVALID_FONT )
	, m_hBodyFont( vgui::INVALID_FONT )
	, m_hLargeBodyFont( vgui::INVALID_FONT )
	, m_iMode( 0 )
	, m_flLastUpdate( 0.0f )
	, m_bHasTitle( false )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/ClientScheme.res", "ClientScheme" ) );
	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	m_pTitle = new vgui::Label( this, "msg1", L"" );
	m_pBody = new vgui::Label( this, "msg2", L"" );
	m_pTitle->SetProportional( false );
	m_pBody->SetProportional( false );
	m_pTitle->SetMouseInputEnabled( false );
	m_pTitle->SetKeyBoardInputEnabled( false );
	m_pBody->SetMouseInputEnabled( false );
	m_pBody->SetKeyBoardInputEnabled( false );
	m_pTitle->SetPaintBorderEnabled( false );
	m_pBody->SetPaintBorderEnabled( false );
	m_pTitle->SetPaintBackgroundEnabled( false );
	m_pBody->SetPaintBackgroundEnabled( false );

	m_wszTitle[0] = L'\0';
	m_wszBody[0] = L'\0';
}

void CHudFoFCourseHint::Init()
{
	Reset();
}

void CHudFoFCourseHint::Reset()
{
	m_iMode = 0;
	m_flLastUpdate = 0.0f;
	m_bHasTitle = false;
	m_wszTitle[0] = L'\0';
	m_wszBody[0] = L'\0';
	if ( m_pTitle )
		m_pTitle->SetText( L"" );
	if ( m_pBody )
		m_pBody->SetText( L"" );
}

void CHudFoFCourseHint::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	Reset();
	LayoutForScreen();
}

bool CHudFoFCourseHint::ShouldDraw()
{
	return m_iMode > 0 && CHudElement::ShouldDraw();
}

void CHudFoFCourseHint::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hTitleFont = scheme->GetFont( "HudNumbersSmall", true );
	m_hBodyFont = scheme->GetFont( "HudSelectionNumbers", true );
	m_hLargeBodyFont = scheme->GetFont( "HudNumbers", true );
	if ( m_hTitleFont == vgui::INVALID_FONT )
		m_hTitleFont = scheme->GetFont( "Default", true );
	if ( m_hBodyFont == vgui::INVALID_FONT )
		m_hBodyFont = m_hTitleFont;
	if ( m_hLargeBodyFont == vgui::INVALID_FONT )
		m_hLargeBodyFont = m_hBodyFont;

	SetBgColor( Color( 0, 0, 0, 200 ) );
	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 2 );
	SetPaintBorderEnabled( false );

	m_pTitle->SetFont( m_hTitleFont );
	m_pTitle->SetContentAlignment( vgui::Label::a_northwest );
	m_pTitle->SetWrap( true );
	m_pTitle->SetFgColor( Color( 220, 240, 20, 255 ) );

	m_pBody->SetContentAlignment( vgui::Label::a_center );
	m_pBody->SetWrap( true );
	m_pBody->SetFgColor( Color( 200, 200, 200, 255 ) );

	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	LayoutForScreen();
}

void CHudFoFCourseHint::PerformLayout()
{
	BaseClass::PerformLayout();
	LayoutForScreen();
}

void CHudFoFCourseHint::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}
}

void CHudFoFCourseHint::LayoutForScreen()
{
	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	if ( screenWide <= 0 || screenTall <= 0 )
		return;

	m_iLastScreenWide = screenWide;
	m_iLastScreenTall = screenTall;

	const int panelWide = FoFCourseHintScale( 250.0f );
	const int panelTall = FoFCourseHintScale( 60.0f );
	SetBounds(
		( screenWide - panelWide ) / 2,
		screenTall - FoFCourseHintScale( 150.0f ),
		panelWide,
		panelTall );

	m_pTitle->SetBounds(
		0,
		FoFCourseHintScale( -2.0f ),
		panelWide,
		FoFCourseHintScale( 35.0f ) );
	m_pTitle->SetTextInset(
		FoFCourseHintScale( 10.0f ),
		FoFCourseHintScale( 8.0f ) );

	m_pBody->SetTextInset(
		FoFCourseHintScale( 14.0f ),
		FoFCourseHintScale( 8.0f ) );
	if ( m_bHasTitle )
	{
		m_pBody->SetBounds(
			0,
			FoFCourseHintScale( 21.0f ),
			panelWide,
			FoFCourseHintScale( 37.0f ) );
		m_pBody->SetFont( m_hBodyFont );
	}
	else
	{
		m_pBody->SetBounds(
			0,
			FoFCourseHintScale( -6.0f ),
			panelWide,
			panelTall );
		m_pBody->SetFont( m_hLargeBodyFont );
	}

	UpdateBodyWrap();
}

void CHudFoFCourseHint::UpdateBodyWrap()
{
	vgui::HFont font = m_bHasTitle ? m_hBodyFont : m_hLargeBodyFont;
	if ( font == vgui::INVALID_FONT || !m_wszBody[0] )
	{
		m_pBody->SetWrap( false );
		return;
	}

	int textWide = 0;
	int textTall = 0;
	vgui::surface()->GetTextSize(
		font, m_wszBody, textWide, textTall );
	m_pBody->SetWrap(
		textWide > FoFCourseHintScale( 230.0f ) );
}

void CHudFoFCourseHint::Receive(
	const char *title, const char *body, int mode )
{
	m_iMode = mode;
	const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
	if ( mode <= 0 )
	{
		// FoF leaves the labels intact and suppresses repeated clear
		// updates for one tenth of a second; ShouldDraw gates them by mode.
		if ( m_flLastUpdate + 0.1f > now )
			return;
		m_flLastUpdate = now;
		return;
	}

	m_flLastUpdate = now;
	m_bHasTitle = title && title[0] && title[0] != ' ';
	wchar_t localizedTitle[1024];
	wchar_t localizedBody[1024];
	FoFCourseHintLocalize(
		title, localizedTitle, sizeof( localizedTitle ) );
	FoFCourseHintLocalize(
		body, localizedBody, sizeof( localizedBody ) );

	// CourseHint strings use the stock %command% tokens.  FoF
	// localizes first and then resolves the player's live bindings, so hints
	// such as %use%, %attack% and %slot3% follow custom key configurations.
	UTIL_ReplaceKeyBindings(
		localizedTitle, 0, m_wszTitle, sizeof( m_wszTitle ) );
	UTIL_ReplaceKeyBindings(
		localizedBody, 0, m_wszBody, sizeof( m_wszBody ) );
	m_pTitle->SetText( m_wszTitle );
	m_pBody->SetText( m_wszBody );
	m_pTitle->SetVisible( m_bHasTitle );
	LayoutForScreen();
}

void FoFCourseHintReceive( const char *title, const char *body, int mode )
{
	CHudFoFCourseHint *hint = GET_HUDELEMENT( CHudFoFCourseHint );
	if ( hint )
		hint->Receive( title, body, mode );
}

// Implemented by the FoF HUD presentation module.  Keeping these as narrow
// sinks leaves hint selection and persistence independent of the HUD panel's
// concrete type and lifetime.
extern bool FoFHasPlayerHintPresenter();
extern void FoFPresentPlayerHint( const char *text, int mode );

struct FoFHintDefinition
{
	CUtlString name;
	CUtlString keywords;
	int weight;
};

struct FoFHintStat
{
	CUtlString name;
	bool shownThisSession;
	int value;
};

static CUtlVector< FoFHintDefinition > s_FoFHints;
static CUtlVector< FoFHintStat > s_FoFHintStats;
static bool s_bFoFHintsInitialized = false;
static float s_flFoFHintAverage = 0.0f;
static float s_flFoFHintCompletion = 0.0f;

static int FoFAbsoluteHintValue( int value )
{
	return value < 0 ? -value : value;
}

static int FoFFindHintStat( const char *name )
{
	for ( int i = 0; i < s_FoFHintStats.Count(); ++i )
	{
		if ( !Q_strcmp( s_FoFHintStats[i].name.String(), name ) )
			return i;
	}
	return -1;
}

float FoFInitializePlayerHints()
{
	// C_FoFPlayer::LoadHints rebuilds both vectors for each
	// local-player lifetime.  MOD is significant: hint_stats.txt is writable.
	s_FoFHints.Purge();
	s_FoFHintStats.Purge();
	s_flFoFHintAverage = 0.0f;
	s_flFoFHintCompletion = 0.0f;

	KeyValues *hintRoot = new KeyValues( "HintList" );
	if ( hintRoot->LoadFromFile(
			filesystem, "fof_scripts/hints.txt", "MOD" ) )
	{
		for ( KeyValues *item = hintRoot->GetFirstSubKey();
			item;
			item = item->GetNextKey() )
		{
			char name[64];
			char keywords[256];
			Q_strncpy( name, item->GetString( "name", "" ), sizeof( name ) );
			Q_strncpy(
				keywords, item->GetString( "keywords", "" ),
				sizeof( keywords ) );

			FoFHintDefinition definition;
			definition.name = name;
			definition.keywords = keywords;
			definition.weight = item->GetInt( "weight", 1 );
			s_FoFHints.AddToTail( definition );
		}
	}
	hintRoot->deleteThis();

	int valueSum = 0;
	KeyValues *statRoot = new KeyValues( "Hint_Stats" );
	if ( statRoot->LoadFromFile(
			filesystem, "fof_scripts/hint_stats.txt", "MOD" ) )
	{
		for ( KeyValues *item = statRoot->GetFirstSubKey();
			item;
			item = item->GetNextKey() )
		{
			char name[129];
			Q_strncpy( name, item->GetString( "name", "" ), sizeof( name ) );

			FoFHintStat stat;
			stat.name = name;
			stat.shownThisSession = false;
			stat.value = FoFAbsoluteHintValue(
				item->GetInt( "value", 0 ) );
			valueSum += stat.value;
			s_FoFHintStats.AddToTail( stat );
		}
	}
	statRoot->deleteThis();

	const int statDivisor =
		clamp( s_FoFHintStats.Count(), 1, 1000 );
	s_flFoFHintAverage =
		(float)valueSum / (float)statDivisor;

	float completionSum = 0.0f;
	for ( int i = 0; i < s_FoFHints.Count(); ++i )
	{
		const int statIndex =
			FoFFindHintStat( s_FoFHints[i].name.String() );
		if ( statIndex >= 0 && s_FoFHints[i].weight != 0 )
		{
			completionSum +=
				(float)s_FoFHintStats[statIndex].value /
				(float)s_FoFHints[i].weight;
		}
	}

	const int hintDivisor = clamp( s_FoFHints.Count(), 1, 1000 );
	s_flFoFHintCompletion = completionSum / (float)hintDivisor;
	s_bFoFHintsInitialized = true;

	return s_flFoFHintCompletion;
}

static bool FoFNextHintTag(
	const char *&cursor, char *tag, int tagBytes )
{
	if ( !cursor || !tag || tagBytes <= 0 )
		return false;

	// The shipped parser uses strtok(",", ...): repeated commas are skipped,
	// but token case and leading/trailing whitespace are deliberately left
	// untouched.
	while ( *cursor == ',' )
		++cursor;
	if ( !*cursor )
		return false;

	int length = 0;
	while ( *cursor && *cursor != ',' )
	{
		if ( length + 1 < tagBytes )
			tag[length++] = *cursor;
		++cursor;
	}
	tag[length] = '\0';
	return length > 0;
}

static bool FoFHintTagListContains(
	const char *tagList, const char *wanted )
{
	const char *cursor = tagList ? tagList : "";
	char tag[256];
	while ( FoFNextHintTag( cursor, tag, sizeof( tag ) ) )
	{
		if ( !Q_strcmp( tag, wanted ) )
			return true;
	}
	return false;
}

static int FoFCountHintTagMatches(
	const char *requestTags, const char *hintTags )
{
	const char *cursor = hintTags ? hintTags : "";
	char tag[256];
	int matches = 0;
	while ( FoFNextHintTag( cursor, tag, sizeof( tag ) ) )
	{
		if ( !Q_strcmp( tag, "history" ) ||
			!Q_strcmp( tag, "quote" ) )
		{
			return -1;
		}
		if ( FoFHintTagListContains( requestTags, tag ) )
			++matches;
	}
	return matches;
}

static bool FoFIsHintEligible( int hintIndex )
{
	const FoFHintDefinition &hint = s_FoFHints[hintIndex];
	const int statIndex = FoFFindHintStat( hint.name.String() );
	if ( statIndex < 0 )
		return true;

	const FoFHintStat &stat = s_FoFHintStats[statIndex];
	return !stat.shownThisSession &&
		stat.value + 1 <= hint.weight &&
		(float)stat.value <= s_flFoFHintAverage;
}

static void FoFSaveHintStat( const char *name, int value )
{
	KeyValues *root = new KeyValues( "Hint_Stats" );
	root->LoadFromFile(
		filesystem, "fof_scripts/hint_stats.txt", "MOD" );

	for ( KeyValues *item = root->GetFirstTrueSubKey(); item; )
	{
		KeyValues *next = item->GetNextTrueSubKey();
		if ( !Q_strcmp( item->GetString( "name", "" ), name ) )
		{
			root->RemoveSubKey( item );
			item->deleteThis();
		}
		item = next;
	}

	KeyValues *item = new KeyValues( "item" );
	item->SetString( "name", name );
	item->SetInt( "value", value );
	root->AddSubKey( item );

	filesystem->CreateDirHierarchy( "fof_scripts", "MOD" );
	root->SaveToFile(
		filesystem, "fof_scripts/hint_stats.txt", "MOD" );
	root->deleteThis();
}

static void FoFRecordHintShown( int hintIndex )
{
	const char *name = s_FoFHints[hintIndex].name.String();
	int statIndex = FoFFindHintStat( name );
	if ( statIndex < 0 )
	{
		FoFHintStat stat;
		stat.name = name;
		stat.shownThisSession = false;
		stat.value = 0;
		statIndex = s_FoFHintStats.AddToTail( stat );
	}

	s_FoFHintStats[statIndex].shownThisSession = true;
	++s_FoFHintStats[statIndex].value;
	FoFSaveHintStat( name, s_FoFHintStats[statIndex].value );
}

bool FoFShowPlayerHintTags(
	const char *tags, int mode, bool forceAny )
{
	if ( !s_bFoFHintsInitialized )
		FoFInitializePlayerHints();

	if ( !FoFHasPlayerHintPresenter() || s_FoFHints.Count() == 0 )
		return false;

	const char *requestTags = tags ? tags : "";
	int best[64];
	int bestCount = 0;
	int bestMatches = 0;

	for ( int pass = 0; pass < 2 && bestCount == 0; ++pass )
	{
		const char *activeTags = pass == 0 ? requestTags : "hint_random";
		bestMatches = 0;

		for ( int i = 0; i < s_FoFHints.Count(); ++i )
		{
			const FoFHintDefinition &hint = s_FoFHints[i];
			if ( hint.name.IsEmpty() || hint.keywords.IsEmpty() ||
				!FoFIsHintEligible( i ) )
			{
				continue;
			}

			int matches = FoFCountHintTagMatches(
				activeTags, hint.keywords.String() );
			// C_FoF_Player::ShowHint's third argument seeds every ordinary
			// hint with one match.  The console showhint command passes true,
			// making an empty tag request choose from the full eligible pool.
			if ( forceAny && matches >= 0 )
				++matches;
			if ( matches <= 0 || matches < bestMatches )
				continue;
			if ( matches > bestMatches )
			{
				bestMatches = matches;
				bestCount = 0;
			}
			if ( bestCount < ARRAYSIZE( best ) )
				best[bestCount++] = i;
		}
	}

	if ( bestCount == 0 )
		return false;

	int selected = best[0];
	if ( bestCount > 1 )
	{
		bool found = false;
		while ( !found )
		{
			for ( int i = 0; i < bestCount; ++i )
			{
				if ( random->RandomInt( 0, 1000 ) > 950 )
				{
					selected = best[i];
					found = true;
					break;
				}
			}
		}
	}

	FoFPresentPlayerHint( s_FoFHints[selected].name.String(), mode );
	FoFRecordHintShown( selected );
	return true;
}

CON_COMMAND( showhint, "" )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	// FoF checks the adjacent IsAlive/IsPlayer virtuals before casting
	// to C_FoF_Player.  This command is the random death-screen hint action.
	if ( !pLocalPlayer || pLocalPlayer->IsAlive() ||
		!pLocalPlayer->IsPlayer() )
	{
		return;
	}

	FoFShowPlayerHintTags( "", 0, true );
}

// FoF-specific localized key-binding presentation.

static void AppendWideBounded(
	wchar_t *output,
	int outputCharacters,
	int &position,
	const wchar_t *text )
{
	if ( !output || outputCharacters <= 0 || !text )
		return;

	while ( *text && position + 1 < outputCharacters )
		output[position++] = *text++;
	output[position] = L'\0';
}

void FoFReplaceKeyBindings(
	const wchar_t *input,
	int inputBytes,
	wchar_t *output,
	int outputBytes )
{
	Assert( output );
	Assert( outputBytes >= static_cast< int >( sizeof( output[0] ) ) );
	if ( !output || outputBytes < static_cast< int >( sizeof( output[0] ) ) )
		return;

	const int outputCharacters = outputBytes / sizeof( output[0] );
	output[0] = L'\0';
	if ( !input || !input[0] )
		return;

	const wchar_t *inputEnd = inputBytes > 0 ?
		input + inputBytes / sizeof( input[0] ) : NULL;
	int position = 0;

	while ( input != inputEnd && *input && position + 1 < outputCharacters )
	{
		if ( *input != L'%' )
		{
			output[position++] = *input++;
			output[position] = L'\0';
			continue;
		}

		const wchar_t *tokenStart = input + 1;
		const wchar_t *tokenEnd = tokenStart;
		while ( tokenEnd != inputEnd && *tokenEnd && *tokenEnd != L'%' )
			++tokenEnd;

		const int tokenLength = static_cast< int >( tokenEnd - tokenStart );
		if ( tokenEnd == inputEnd || !*tokenEnd || tokenLength <= 0 ||
			tokenLength >= 64 )
		{
			output[position++] = *input++;
			output[position] = L'\0';
			continue;
		}

		wchar_t token[64];
		V_wcsncpy(
			token,
			tokenStart,
			( tokenLength + 1 ) * sizeof( token[0] ) );
		token[tokenLength] = L'\0';

		char binding[64];
		g_pVGuiLocalize->ConvertUnicodeToANSI(
			token, binding, sizeof( binding ) );
		const char *key = engine->Key_LookupBinding(
			binding[0] == '+' ? binding + 1 : binding );
		if ( !key )
			key = IsX360() ? "" : "< not bound >";

		char friendlyName[80];
		bool addSquareBrackets = false;
		if ( IsX360() )
		{
			if ( !key[0] )
			{
				Q_strncpy( friendlyName, "#GameUI_None",
					sizeof( friendlyName ) );
				addSquareBrackets = true;
			}
			else
			{
				Q_snprintf( friendlyName, sizeof( friendlyName ),
					"#GameUI_KeyNames_%s", key );
			}
		}
		else
		{
			// FoF's client uses "<%s>" here; the stock SDK uses "%s".
			Q_snprintf( friendlyName, sizeof( friendlyName ), "<%s>", key );
		}
		Q_strupr( friendlyName );

		const wchar_t *localizedName =
			g_pVGuiLocalize->Find( friendlyName );
		if ( localizedName && localizedName[0] )
		{
			if ( addSquareBrackets )
				AppendWideBounded(
					output, outputCharacters, position, L"[" );
			AppendWideBounded(
				output, outputCharacters, position, localizedName );
			if ( addSquareBrackets )
				AppendWideBounded(
					output, outputCharacters, position, L"]" );
		}
		else
		{
			wchar_t friendlyNameWide[80];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				friendlyName,
				friendlyNameWide,
				sizeof( friendlyNameWide ) );
			AppendWideBounded(
				output, outputCharacters, position, friendlyNameWide );
		}

		input = tokenEnd + 1;
	}

	output[position] = L'\0';
}
