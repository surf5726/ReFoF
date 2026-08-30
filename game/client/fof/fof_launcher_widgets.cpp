#include "cbase.h"
#include "fof/fof_launcher_widgets.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>

#include <math.h>
#include <string.h>

#if defined( _WIN32 )
#define FOF_STRTOK_R strtok_s
#else
#define FOF_STRTOK_R strtok_r
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const wchar_t *FoFLauncherLocalize(
	const char *pszText,
	wchar_t *pBuffer,
	int nBufferBytes )
{
	if ( pszText && pszText[0] == '#' )
	{
		const wchar_t *pLocalized = g_pVGuiLocalize->Find( pszText );
		if ( pLocalized )
			return pLocalized;
	}

	g_pVGuiLocalize->ConvertANSIToUnicode(
		pszText ? pszText : "", pBuffer, nBufferBytes );
	return pBuffer;
}

void FoFLauncherDrawText(
	const wchar_t *pText,
	vgui::HFont hFont,
	int x,
	int y,
	Color color )
{
	if ( !pText || hFont == vgui::INVALID_FONT )
		return;

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( pText, V_wcslen( pText ) );
}

void FoFLauncherDrawLocalizedText(
	const char *pszText,
	vgui::HFont hFont,
	int x,
	int y,
	Color color )
{
	wchar_t wszFallback[256];
	FoFLauncherDrawText(
		FoFLauncherLocalize(
			pszText, wszFallback, sizeof( wszFallback ) ),
		hFont, x, y, color );
}

int FoFLauncherTextWide(
	const wchar_t *pText,
	vgui::HFont hFont )
{
	if ( !pText || hFont == vgui::INVALID_FONT )
		return 0;
	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( hFont, pText, wide, tall );
	return wide;
}

float FoFLauncherScreenScale( int screenTall )
{
	return (float)screenTall / 480.0f;
}

int FoFLauncherScalePixel( float logicalPixels, int screenTall )
{
	// FoF converts launcher coordinates with cvttss2si.  Truncation is
	// observable at 640x480 for half-pixel values such as 12.5 and 2.5.
	return (int)( logicalPixels * FoFLauncherScreenScale( screenTall ) );
}

void FoFLauncherDrawWrappedTitle(
	const char *pszText,
	vgui::HFont hFont,
	int x,
	int y,
	int wide,
	int maxLines,
	Color color )
{
	if ( !pszText || !pszText[0] || hFont == vgui::INVALID_FONT )
		return;

	char text[256];
	Q_strncpy( text, pszText, sizeof( text ) );
	char line[256] = "";
	int lineIndex = 0;
	char *pContext = NULL;
	for ( char *pWord = FOF_STRTOK_R( text, " ", &pContext );
		pWord && lineIndex < maxLines;
		pWord = FOF_STRTOK_R( NULL, " ", &pContext ) )
	{
		char candidate[256];
		Q_snprintf(
			candidate,
			sizeof( candidate ),
			line[0] ? "%s %s" : "%s",
			line[0] ? line : pWord,
			line[0] ? pWord : "" );
		if ( !line[0] )
			Q_strncpy( candidate, pWord, sizeof( candidate ) );

		wchar_t wszCandidate[256];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			candidate, wszCandidate, sizeof( wszCandidate ) );
		if ( line[0] &&
			FoFLauncherTextWide( wszCandidate, hFont ) > wide )
		{
			wchar_t wszLine[256];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				line, wszLine, sizeof( wszLine ) );
			FoFLauncherDrawText(
				wszLine,
				hFont,
				x,
				y + lineIndex * vgui::surface()->GetFontTall( hFont ),
				color );
			++lineIndex;
			Q_strncpy( line, pWord, sizeof( line ) );
		}
		else
		{
			Q_strncpy( line, candidate, sizeof( line ) );
		}
	}

	if ( line[0] && lineIndex < maxLines )
	{
		wchar_t wszLine[256];
		g_pVGuiLocalize->ConvertANSIToUnicode(
			line, wszLine, sizeof( wszLine ) );
		FoFLauncherDrawText(
			wszLine,
			hFont,
			x,
			y + lineIndex * vgui::surface()->GetFontTall( hFont ),
			color );
	}
}

CFoFSearchEntry::CFoFSearchEntry(
	vgui::Panel *pParent,
	const char *pszName,
	const char *pszHint )
	: BaseClass( pParent, pszName )
	, m_Hint( pszHint )
	, m_hHintFont( vgui::INVALID_FONT )
	, m_HintColor( 220, 220, 215, 255 )
{
}

void CFoFSearchEntry::SetHintStyle( vgui::HFont hFont, Color color )
{
	m_hHintFont = hFont;
	m_HintColor = color;
	Repaint();
}

void CFoFSearchEntry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	const Color textColor = pScheme->GetColor(
		"TextEntry.TextColor", Color( 220, 220, 215, 255 ) );
	SetFgColor( textColor );
	SetBgColor( Color( 30, 30, 30, 255 ) );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( true );
	m_HintColor = textColor;
}

void CFoFSearchEntry::Paint()
{
	BaseClass::Paint();

	char text[2];
	GetText( text, sizeof( text ) );
	if ( text[0] || HasFocus() || m_hHintFont == vgui::INVALID_FONT )
		return;

	wchar_t wszFallback[128];
	const wchar_t *pHint = FoFLauncherLocalize(
		m_Hint.String(), wszFallback, sizeof( wszFallback ) );
	FoFLauncherDrawText(
		pHint,
		m_hHintFont,
		3,
		MAX( ( GetTall() - vgui::surface()->GetFontTall(
			m_hHintFont ) ) / 2, 0 ),
		m_HintColor );
}

CFoFPingComboBox::CFoFPingComboBox(
	vgui::Panel *pParent,
	const char *pszName,
	int visibleLines )
	: BaseClass( pParent, pszName, visibleLines, false )
{
	// This filter is mouse-only.  Stock non-editable ComboBox still accepts
	// type-ahead keys and silently selects the first matching row.
	SetKeyBoardInputEnabled( false );
}

void CFoFPingComboBox::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyFoFStyle();
}

void CFoFPingComboBox::PerformLayout()
{
	BaseClass::PerformLayout();
	// ComboBoxButton applies its own scheme independently of its parent.
	// Reapply the launcher styling after child layout so SourceScheme cannot
	// restore the stock framed button.
	ApplyFoFStyle();
}

void CFoFPingComboBox::ApplyFoFStyle()
{
	const Color textColor( 196, 191, 180, 255 );
	const Color fieldColor( 30, 30, 26, 255 );
	const Color arrowColor( 10, 10, 8, 255 );
	SetFgColor( textColor );
	SetBgColor( fieldColor );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );

	vgui::ComboBoxButton *pButton = GetComboButton();
	if ( !pButton )
		return;

	pButton->SetButtonBorderEnabled( false );
	pButton->SetPaintBorderEnabled( false );
	pButton->SetPaintBackgroundEnabled( true );
	pButton->SetDefaultColor( textColor, arrowColor );
	pButton->SetArmedColor( textColor, arrowColor );
	pButton->SetDepressedColor( textColor, arrowColor );
	pButton->SetSelectedColor( textColor, arrowColor );
}

void CFoFPingComboBox::OnShowMenu( vgui::Menu *pMenu )
{
	// The shipped ping selector opens with no pre-armed row.  Stock ComboBox
	// arms its active item, which creates the red/orange strip absent from
	// the FoF launcher.
	if ( pMenu )
	{
		pMenu->SetKeyBoardInputEnabled( false );
		pMenu->ClearCurrentlyHighlightedItem();
	}
}

CFoFLauncherPageButton::CFoFLauncherPageButton(
	vgui::Panel *pParent,
	const char *pszName,
	const char *pszText,
	vgui::Panel *pTarget,
	const char *pszCommand )
	: BaseClass( pParent, pszName, pszText, pTarget, pszCommand )
{
	// The cards are painted after these controls, so the launcher redraws the
	// arrows from its PostChildPaint pass.  Suppress the normal child pass:
	// painting both passes made the translucent ClientScheme border opaque and
	// unlike the shipped launcher.
	SetPaintBackgroundEnabled( false );
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
}

void CFoFLauncherPageButton::PaintOnTop()
{
	// Match Panel::PaintTraverse exactly, including the content inset used for
	// Button text.  This keeps the original one-pass border blending and glyph
	// placement while still drawing above the overlapping course/server cards.
	vgui::surface()->PushMakeCurrent( GetVPanel(), false );
	BaseClass::PaintBackground();
	vgui::surface()->PopMakeCurrent( GetVPanel() );

	vgui::surface()->PushMakeCurrent( GetVPanel(), true );
	BaseClass::Paint();
	vgui::surface()->PopMakeCurrent( GetVPanel() );

	vgui::surface()->PushMakeCurrent( GetVPanel(), false );
	BaseClass::PaintBorder();
	vgui::surface()->PopMakeCurrent( GetVPanel() );
}

CFoFLauncherButton::CFoFLauncherButton(
	vgui::Panel *pParent,
	const char *pszName,
	const char *pszText,
	vgui::Panel *pTarget,
	const char *pszCommand )
	: BaseClass( pParent, pszName, "", pTarget, pszCommand )
	, m_Text( pszText )
	, m_hFont( vgui::INVALID_FONT )
{
	m_wszCustomText[0] = L'\0';
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	DrawFocusBox( false );
}

void CFoFLauncherButton::SetCustomText( const wchar_t *pText )
{
	V_wcsncpy(
		m_wszCustomText,
		pText ? pText : L"",
		ARRAYSIZE( m_wszCustomText ) );
	Repaint();
}

void CFoFLauncherButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hFont = pFoFScheme ?
		pFoFScheme->GetFont( "MenuFontMed", false ) :
		vgui::INVALID_FONT;
	if ( m_hFont == vgui::INVALID_FONT )
		m_hFont = pScheme->GetFont( "Default", false );
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
}

void CFoFLauncherButton::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	Color background( 0, 0, 0, 0 );
	Color foreground( 190, 190, 180, 255 );
	if ( IsSelected() )
	{
		background = Color( 55, 54, 45, 255 );
		foreground = Color( 225, 220, 35, 255 );
	}
	else if ( IsArmed() )
	{
		background = Color( 166, 37, 25, 235 );
		foreground = Color( 226, 205, 198, 255 );
	}

	vgui::surface()->DrawSetColor( background );
	vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
	wchar_t wszFallback[256];
	const wchar_t *pText = m_wszCustomText[0] ?
		m_wszCustomText :
		FoFLauncherLocalize(
			m_Text.String(), wszFallback, sizeof( wszFallback ) );
	const int fontTall = vgui::surface()->GetFontTall( m_hFont );
	FoFLauncherDrawText(
		pText,
		m_hFont,
		3,
		MAX( ( tall - fontTall ) / 2, 0 ),
		foreground );
}

CFoFBrowserWarningButton::CFoFBrowserWarningButton(
	vgui::Panel *pParent,
	const char *pszName,
	vgui::Panel *pTarget,
	const char *pszCommand )
	: BaseClass( pParent, pszName, "" )
	, m_flEnableTime( 0.0f )
{
	const color32 warning = { 255, 55, 55, 255 };
	const color32 armed = { 255, 200, 200, 255 };
	SetImage( BUTTON_ENABLED, "vgui/slide_bg_small_clear", warning );
	SetImage( BUTTON_ENABLED_MOUSE_OVER, "vgui/slide_bg_small", armed );
	SetImage( BUTTON_PRESSED, "vgui/slide_bg_small", armed );
	SetImage( BUTTON_DISABLED, "vgui/slide_bg_small_clear", warning );

	SetText( "#stock_browser_warning" );
	SetCommand( pszCommand );
	AddActionSignalTarget( pTarget );
	SetContentAlignment( vgui::Label::a_center );
	SetZPos( 20 );
	SetButtonBorderEnabled( true );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetWrap( true );
	DrawFocusBox( false );
	SetEnabled( false );
	SetVisible( false );
}

void CFoFBrowserWarningButton::BeginWarning()
{
	SetEnabled( false );
	SetVisible( true );
	m_flEnableTime = gpGlobals->curtime + 7.0f;
	Repaint();
}

void CFoFBrowserWarningButton::DismissWarning()
{
	m_flEnableTime = 0.0f;
	SetVisible( false );
}

void CFoFBrowserWarningButton::OnThink()
{
	BaseClass::OnThink();
	if ( !IsVisible() || m_flEnableTime == 0.0f )
		return;

	if ( gpGlobals->curtime >= m_flEnableTime )
	{
		SetEnabled( true );
		m_flEnableTime = 0.0f;
		return;
	}

	const int red = clamp(
		(int)( fabsf( sinf( gpGlobals->curtime * 2.0f ) ) * 50.0f + 80.0f ),
		0,
		255 );
	const color32 flashing = { (byte)red, 55, 55, 255 };
	SetImage( BUTTON_DISABLED, "vgui/slide_bg_small_clear", flashing );
	Repaint();
}

CFoFTopBarButton::CFoFTopBarButton(
	vgui::Panel *pParent,
	const char *pszName,
	const char *pszTitle,
	const char *pszIcon,
	vgui::Panel *pTarget,
	const char *pszCommand )
	: BaseClass( pParent, pszName, "", pTarget, pszCommand )
	, m_Title( pszTitle )
	, m_iIcon( -1 )
	, m_hFont( vgui::INVALID_FONT )
{
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	DrawFocusBox( false );
	if ( pszIcon && pszIcon[0] )
	{
		char material[MAX_PATH];
		Q_snprintf( material, sizeof( material ), "vgui/%s", pszIcon );
		m_iIcon = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(
			m_iIcon, material, true, false );
	}
}

CFoFTopBarButton::~CFoFTopBarButton()
{
	if ( m_iIcon >= 0 )
		vgui::surface()->DestroyTextureID( m_iIcon );
}

void CFoFTopBarButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hFont = pFoFScheme ?
		pFoFScheme->GetFont( "MenuFontMed", false ) :
		vgui::INVALID_FONT;
	if ( m_hFont == vgui::INVALID_FONT )
		m_hFont = pScheme->GetFont( "Default", false );
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
}

int CFoFTopBarButton::DesiredWide( int iconWide, int margin ) const
{
	wchar_t wszFallback[256];
	const wchar_t *pTitle = FoFLauncherLocalize(
		m_Title.String(), wszFallback, sizeof( wszFallback ) );
	return margin * 3 + iconWide * 2 +
		FoFLauncherTextWide( pTitle, m_hFont );
}

void CFoFTopBarButton::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );

	Color background;
	Color foreground;
	if ( IsBlinking() && !IsArmed() )
	{
		// FoF keeps the update-notes item blinking while idle, even after
		// another top-bar item is opened.  Hovering it suspends the blink and
		// uses the same dim armed state as the other links.
		const bool bBright =
			( vgui::system()->GetTimeMillis() % 2000 ) < 750;
		background = bBright ?
			Color( 178, 88, 2, 255 ) : Color( 101, 47, 2, 255 );
		foreground = bBright ?
			Color( 221, 221, 221, 255 ) : Color( 126, 126, 126, 255 );
	}
	else
	{
		// The four ordinary top-bar links keep the same transparent-black plate
		// while armed and dim their contents instead.  Only the update-notes
		// blink owns a colored background.
		background = Color( 5, 5, 5, 255 );
		foreground = IsArmed() ?
			Color( 126, 126, 126, 255 ) : Color( 225, 220, 205, 255 );
	}
	vgui::surface()->DrawSetColor( background );
	vgui::surface()->DrawFilledRect( 0, 0, wide, tall );

	const int margin = MAX( tall / 10, 2 );
	const int iconWide = MAX( tall - margin * 2, 1 );
	if ( m_iIcon >= 0 )
	{
		vgui::surface()->DrawSetColor( foreground );
		vgui::surface()->DrawSetTexture( m_iIcon );
		vgui::surface()->DrawTexturedRect(
			margin, margin, margin + iconWide, tall - margin );
	}

	wchar_t wszFallback[256];
	const wchar_t *pTitle = FoFLauncherLocalize(
		m_Title.String(), wszFallback, sizeof( wszFallback ) );
	const int fontTall = vgui::surface()->GetFontTall( m_hFont );
	FoFLauncherDrawText(
		pTitle,
		m_hFont,
		margin * 2 + iconWide,
		MAX( ( tall - fontTall ) / 2, 0 ),
		foreground );
}

CFoFRefreshButton::CFoFRefreshButton(
	vgui::Panel *pParent,
	const char *pszName,
	vgui::Panel *pTarget,
	const char *pszCommand,
	bool bBright )
	: BaseClass( pParent, pszName, "", pTarget, pszCommand )
	, m_bBright( bBright )
	, m_iTexture( -1 )
{
	SetButtonBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	DrawFocusBox( false );
	m_iTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(
		m_iTexture, "vgui/primary_switch", true, false );
}

CFoFRefreshButton::~CFoFRefreshButton()
{
	if ( m_iTexture >= 0 )
		vgui::surface()->DestroyTextureID( m_iTexture );
}

void CFoFRefreshButton::Paint()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	const Color normal = m_bBright ?
		Color( 220, 220, 210, 255 ) : Color( 180, 205, 0, 255 );
	const Color armed = m_bBright ?
		Color( 255, 245, 120, 255 ) : Color( 235, 240, 35, 255 );
	vgui::surface()->DrawSetColor( IsArmed() ? armed : normal );
	if ( m_iTexture >= 0 )
	{
		vgui::surface()->DrawSetTexture( m_iTexture );
		vgui::surface()->DrawTexturedRect( 1, 1, wide - 1, tall - 1 );
	}
}
