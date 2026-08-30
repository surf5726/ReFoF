#include "cbase.h"
#include "fof/fof_credits_dialog.h"

#include "filesystem.h"
#include "ienginevgui.h"
#include "tier1/utlvector.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/RichText.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoFCreditsDialog *g_pFoFCreditsDialog = NULL;

static int FoFCreditsScale( int iLogicalPixels )
{
	return MAX( RoundFloatToInt(
		iLogicalPixels * (float)ScreenHeight() / 480.0f ), 1 );
}

CFoFCreditsDialog::CFoFCreditsDialog()
	: BaseClass( NULL, "FoFCreditsPanel", false, true )
	, m_pCredits( NULL )
	, m_pClose( NULL )
{
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( "ClientScheme" );
	SetProportional( false );
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );
	SetMoveable( false );
	SetSizeable( false );
	SetDeleteSelfOnClose( false );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );
	SetBgColor( Color( 22, 2, 1, 255 ) );

	m_pCredits = new vgui::RichText( this, "Label1" );
	m_pCredits->SetVerticalScrollbar( true );
	m_pCredits->SetUnusedScrollbarInvisible( false );
	m_pCredits->SetMouseInputEnabled( true );
	m_pCredits->SetKeyBoardInputEnabled( true );
	m_pCredits->SetBgColor( Color( 22, 2, 1, 255 ) );

	m_pClose = new vgui::Button(
		this, "CloseButton", "#GameUI_Close", this, "Close" );

	SetVisible( false );
}

void CFoFCreditsDialog::LoadCredits()
{
	FileHandle_t hFile = filesystem->Open( "CREDITS.txt", "rb", "GAME" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		m_pCredits->SetText( "Unable to load CREDITS.txt" );
		return;
	}

	const unsigned int nSize = filesystem->Size( hFile );
	CUtlVector<unsigned char> bytes;
	bytes.SetCount( nSize + 2 );
	const int nRead = filesystem->Read( bytes.Base(), nSize, hFile );
	filesystem->Close( hFile );
	if ( nRead <= 0 )
	{
		m_pCredits->SetText( "" );
		return;
	}

	bytes[nRead] = 0;
	bytes[nRead + 1] = 0;
	if ( nRead >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE )
	{
		const int nCharacters = ( nRead - 2 ) / 2;
		CUtlVector<wchar_t> credits;
		credits.SetCount( nCharacters + 1 );
		for ( int i = 0; i < nCharacters; ++i )
		{
			credits[i] = (wchar_t)(
				bytes[2 + i * 2] |
				( bytes[3 + i * 2] << 8 ) );
		}
		credits[nCharacters] = L'\0';
		m_pCredits->SetText( credits.Base() );
	}
	else
	{
		m_pCredits->SetText( (const char *)bytes.Base() );
	}
	m_pCredits->GotoTextStart();
}

void CFoFCreditsDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );
	SetBgColor( Color( 22, 2, 1, 255 ) );
	m_pCredits->SetBgColor( Color( 22, 2, 1, 255 ) );

	vgui::HFont hCreditsFont = pScheme->GetFont( "MenuFontMed", false );
	if ( hCreditsFont != vgui::INVALID_FONT )
		m_pCredits->SetFont( hCreditsFont );
	vgui::HFont hButtonFont = pScheme->GetFont( "MenuFont", false );
	if ( hButtonFont != vgui::INVALID_FONT )
		m_pClose->SetFont( hButtonFont );

	vgui::IBorder *pBorder = pScheme->GetBorder( "BaseBorder" );
	if ( pBorder )
		m_pCredits->SetBorder( pBorder );
	PerformLayout();
}

void CFoFCreditsDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	const int iMargin = FoFCreditsScale( 25 );
	const int iButtonWide = FoFCreditsScale( 100 );
	const int iButtonTall = FoFCreditsScale( 20 );
	const int iButtonY = FoFCreditsScale( 455 );
	const int iCreditsTall = FoFCreditsScale( 420 );

	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	m_pCredits->SetBounds(
		iMargin,
		iMargin,
		MAX( ScreenWidth() - 2 * iMargin, 1 ),
		MIN( iCreditsTall, MAX( ScreenHeight() - iMargin, 1 ) ) );
	m_pClose->SetBounds(
		iMargin,
		MIN( iButtonY, MAX( ScreenHeight() - iButtonTall, 0 ) ),
		iButtonWide,
		iButtonTall );
}

void CFoFCreditsDialog::ShowDialog()
{
	LoadCredits();
	PerformLayout();
	SetVisible( true );
	SetEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	Activate();
}

void CFoFCreditsDialog::HideDialog()
{
	SetVisible( false );
}

void CFoFCreditsDialog::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "Close" ) ||
		!Q_stricmp( pszCommand, "close" ) )
	{
		HideDialog();
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFCreditsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		HideDialog();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

CON_COMMAND( OpenCreditsDialog, "Open the Fistful of Frags credits." )
{
	if ( !g_pFoFCreditsDialog )
		g_pFoFCreditsDialog = new CFoFCreditsDialog();
	g_pFoFCreditsDialog->ShowDialog();
}
