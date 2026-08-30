#include "cbase.h"
#include "fof/fof_advanced_options.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include <vgui/IScheme.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFoFAdvancedValuePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFoFAdvancedValuePanel, vgui::Panel );

public:
	CFoFAdvancedValuePanel(
		vgui::Panel *pParent,
		const char *pszName,
		vgui::Panel *pControl = NULL )
		: BaseClass( pParent, pszName )
		, m_pControl( pControl )
	{
		SetTall( 28 );
		SetPaintBackgroundEnabled( false );
		if ( m_pControl )
			m_pControl->SetParent( this );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		if ( m_pControl )
			m_pControl->SetBounds( 0, 2, GetWide(), 24 );
	}

private:
	vgui::Panel *m_pControl;
};

CFoFAdvancedOptionsDialog::CFoFAdvancedOptionsDialog()
	: BaseClass( NULL, "MultiplayerAdvancedDialog", true, true )
	, m_pOptionsList( NULL )
{
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/SourceScheme.res", "SourceScheme" ) );
	SetProportional( false );
	SetDeleteSelfOnClose( false );
	SetSizeable( false );
	SetMoveable( false );
	SetTitle( "#GameUI_MultiplayerAdvanced", true );

	m_pOptionsList = new vgui::PanelListPanel( this, "PanelListPanel" );
	LoadControlSettings( "Resource\\MultiplayerAdvancedDialog.res" );
	m_pOptionsList->SetFirstColumnWidth( 320 );
	m_pOptionsList->SetNumColumns( 1 );
	m_pOptionsList->SetVerticalBufferPixels( 4 );

	if ( LoadOptions() )
		BuildOptionControls();
	SetVisible( false );
}

CFoFAdvancedOptionsDialog::~CFoFAdvancedOptionsDialog()
{
	m_Options.PurgeAndDeleteElements();
}

void CFoFAdvancedOptionsDialog::BuildOptionControls()
{
	for ( int i = 0; i < m_Options.Count(); ++i )
	{
		FoFAdvancedOption_t *pOption = m_Options[i];
		char szControlName[64];
		Q_snprintf(
			szControlName, sizeof( szControlName ), "AdvancedOption%d", i );

		if ( pOption->type == FOF_ADVANCED_BOOL )
		{
			vgui::CheckButton *pCheck = new vgui::CheckButton(
				m_pOptionsList, szControlName, pOption->prompt.String() );
			CFoFAdvancedValuePanel *pSpacer =
				new CFoFAdvancedValuePanel( m_pOptionsList, "BoolSpacer" );
			pOption->control = pCheck;
			m_pOptionsList->AddItem( pCheck, pSpacer );
			continue;
		}

		vgui::Label *pLabel = new vgui::Label(
			m_pOptionsList, "DescLabel", pOption->prompt.String() );
		pLabel->SetContentAlignment( vgui::Label::a_west );

		vgui::Panel *pValueControl = NULL;
		if ( pOption->type == FOF_ADVANCED_LIST )
		{
			vgui::ComboBox *pCombo = new vgui::ComboBox(
				m_pOptionsList, "DescComboBox", 5, false );
			for ( int j = 0; j < pOption->choices.Count(); ++j )
			{
				KeyValues *pData = new KeyValues( "data" );
				pData->SetString(
					"value", pOption->choices[j]->value.String() );
				pCombo->AddItem(
					pOption->choices[j]->label.String(), pData );
				pData->deleteThis();
			}
			pValueControl = pCombo;
		}
		else
		{
			vgui::TextEntry *pEntry = new vgui::TextEntry(
				m_pOptionsList, "DescTextEntry" );
			pEntry->SetMaximumCharCount( 255 );
			pValueControl = pEntry;
		}

		CFoFAdvancedValuePanel *pValuePanel =
			new CFoFAdvancedValuePanel(
				m_pOptionsList, szControlName, pValueControl );
		pOption->control = pValueControl;
		m_pOptionsList->AddItem( pLabel, pValuePanel );
	}
	m_pOptionsList->MoveScrollBarToTop();
	m_pOptionsList->InvalidateLayout( true );
}

void CFoFAdvancedOptionsDialog::PerformLayout()
{
	BaseClass::PerformLayout();
	const int iX = 60 + ( ScreenWidth() - 1024 ) / 2;
	const int iY = 108 + ( ScreenHeight() - 768 ) / 2;
	SetPos(
		clamp( iX, 0, MAX( ScreenWidth() - GetWide(), 0 ) ),
		clamp( iY, 0, MAX( ScreenHeight() - GetTall(), 0 ) ) );
}

void CFoFAdvancedOptionsDialog::ShowDialog()
{
	ReadSettings();
	PerformLayout();
	SetVisible( true );
	SetEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	DoModal();
}

void CFoFAdvancedOptionsDialog::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "Ok" ) )
	{
		ApplySettings();
		Close();
		return;
	}
	if ( !Q_stricmp( pszCommand, "Close" ) )
	{
		Close();
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFAdvancedOptionsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		Close();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

static bool FoFAdvancedReadToken(
	const char *&pCursor,
	char *pszToken,
	int nTokenSize )
{
	pszToken[0] = '\0';
	if ( !pCursor )
		return false;
	pCursor = engine->ParseFile( pCursor, pszToken, nTokenSize );
	return pszToken[0] != '\0';
}

static bool FoFAdvancedReadExpected(
	const char *&pCursor,
	const char *pszExpected )
{
	char szToken[512];
	return FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) &&
		!Q_stricmp( szToken, pszExpected );
}

static const char *FoFAdvancedGetCurrentValue(
	const FoFAdvancedOption_t *pOption )
{
	ConVarRef value( pOption->cvar.String(), true );
	return value.IsValid() ? value.GetString() : pOption->defaultValue.String();
}

static void FoFAdvancedWriteQuoted(
	CUtlBuffer &buffer,
	const char *pszValue )
{
	buffer.PutChar( '"' );
	for ( const char *p = pszValue ? pszValue : ""; *p; ++p )
	{
		if ( *p == '"' || *p == '\\' )
			buffer.PutChar( '\\' );
		if ( *p != '\r' && *p != '\n' )
			buffer.PutChar( *p );
	}
	buffer.PutChar( '"' );
}

bool CFoFAdvancedOptionsDialog::LoadOptions()
{
	CUtlBuffer script( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !filesystem->ReadFile( "cfg/user.scr", "GAME", script ) )
		return false;

	const char *pCursor = script.String();
	char szToken[512];
	bool bFoundDescription = false;
	while ( FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
	{
		if ( Q_stricmp( szToken, "DESCRIPTION" ) )
			continue;
		if ( !FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) ||
			Q_stricmp( szToken, "INFO_OPTIONS" ) ||
			!FoFAdvancedReadExpected( pCursor, "{" ) )
		{
			return false;
		}
		bFoundDescription = true;
		break;
	}
	if ( !bFoundDescription )
		return false;

	while ( FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
	{
		if ( !Q_stricmp( szToken, "}" ) )
			break;
		if ( !ParseOption( pCursor, szToken ) )
			return false;
	}
	return m_Options.Count() > 0;
}

bool CFoFAdvancedOptionsDialog::ParseOption(
	const char *&pCursor,
	const char *pszCvar )
{
	FoFAdvancedOption_t *pOption = new FoFAdvancedOption_t;
	pOption->cvar = pszCvar;

	char szToken[512];
	if ( !FoFAdvancedReadExpected( pCursor, "{" ) ||
		!FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
	{
		delete pOption;
		return false;
	}
	pOption->prompt = szToken;

	if ( !FoFAdvancedReadExpected( pCursor, "{" ) ||
		!FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
	{
		delete pOption;
		return false;
	}

	if ( !Q_stricmp( szToken, "BOOL" ) )
	{
		pOption->type = FOF_ADVANCED_BOOL;
		if ( !FoFAdvancedReadExpected( pCursor, "}" ) )
		{
			delete pOption;
			return false;
		}
	}
	else if ( !Q_stricmp( szToken, "STRING" ) )
	{
		pOption->type = FOF_ADVANCED_STRING;
		if ( !FoFAdvancedReadExpected( pCursor, "}" ) )
		{
			delete pOption;
			return false;
		}
	}
	else if ( !Q_stricmp( szToken, "NUMBER" ) )
	{
		pOption->type = FOF_ADVANCED_NUMBER;
		if ( !FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
		{
			delete pOption;
			return false;
		}
		pOption->minimum = (float)atof( szToken );
		if ( !FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
		{
			delete pOption;
			return false;
		}
		pOption->maximum = (float)atof( szToken );
		if ( !FoFAdvancedReadExpected( pCursor, "}" ) )
		{
			delete pOption;
			return false;
		}
	}
	else if ( !Q_stricmp( szToken, "LIST" ) )
	{
		pOption->type = FOF_ADVANCED_LIST;
		while ( FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
		{
			if ( !Q_stricmp( szToken, "}" ) )
				break;
			FoFAdvancedChoice_t *pChoice = new FoFAdvancedChoice_t;
			pChoice->label = szToken;
			if ( !FoFAdvancedReadToken(
				pCursor, szToken, sizeof( szToken ) ) )
			{
				delete pChoice;
				delete pOption;
				return false;
			}
			pChoice->value = szToken;
			pOption->choices.AddToTail( pChoice );
		}
	}
	else
	{
		delete pOption;
		return false;
	}

	if ( !FoFAdvancedReadExpected( pCursor, "{" ) ||
		!FoFAdvancedReadToken( pCursor, szToken, sizeof( szToken ) ) )
	{
		delete pOption;
		return false;
	}
	pOption->defaultValue = szToken;
	if ( !FoFAdvancedReadExpected( pCursor, "}" ) ||
		!FoFAdvancedReadExpected( pCursor, "}" ) )
	{
		delete pOption;
		return false;
	}

	m_Options.AddToTail( pOption );
	return true;
}

void CFoFAdvancedOptionsDialog::ReadSettings()
{
	for ( int i = 0; i < m_Options.Count(); ++i )
	{
		FoFAdvancedOption_t *pOption = m_Options[i];
		const char *pszValue = FoFAdvancedGetCurrentValue( pOption );
		if ( pOption->type == FOF_ADVANCED_BOOL )
		{
			static_cast<vgui::CheckButton *>( pOption->control )->SetSelected(
				atoi( pszValue ) != 0 );
		}
		else if ( pOption->type == FOF_ADVANCED_LIST )
		{
			vgui::ComboBox *pCombo =
				static_cast<vgui::ComboBox *>( pOption->control );
			int iSelectedRow = 0;
			for ( int j = 0; j < pCombo->GetItemCount(); ++j )
			{
				KeyValues *pData = pCombo->GetItemUserData(
					pCombo->GetItemIDFromRow( j ) );
				if ( pData && !Q_stricmp(
					pData->GetString( "value", "" ), pszValue ) )
				{
					iSelectedRow = j;
					break;
				}
			}
			if ( pCombo->GetItemCount() > 0 )
				pCombo->SilentActivateItemByRow( iSelectedRow );
		}
		else
		{
			static_cast<vgui::TextEntry *>( pOption->control )->SetText(
				pszValue );
		}
	}
	m_pOptionsList->MoveScrollBarToTop();
}

void CFoFAdvancedOptionsDialog::ApplySettings()
{
	for ( int i = 0; i < m_Options.Count(); ++i )
	{
		FoFAdvancedOption_t *pOption = m_Options[i];
		ConVarRef value( pOption->cvar.String(), true );
		if ( !value.IsValid() )
			continue;

		if ( pOption->type == FOF_ADVANCED_BOOL )
		{
			value.SetValue(
				static_cast<vgui::CheckButton *>(
					pOption->control )->IsSelected() ? 1 : 0 );
		}
		else if ( pOption->type == FOF_ADVANCED_LIST )
		{
			vgui::ComboBox *pCombo =
				static_cast<vgui::ComboBox *>( pOption->control );
			KeyValues *pData = pCombo->GetActiveItemUserData();
			if ( pData )
				value.SetValue( pData->GetString( "value", "" ) );
		}
		else
		{
			char szValue[256];
			static_cast<vgui::TextEntry *>( pOption->control )->GetText(
				szValue, sizeof( szValue ) );
			if ( pOption->type == FOF_ADVANCED_NUMBER )
			{
				float flValue = (float)atof( szValue );
				if ( pOption->minimum != -1.0f ||
					pOption->maximum != -1.0f )
				{
					flValue = clamp(
						flValue, pOption->minimum, pOption->maximum );
				}
				value.SetValue( flValue );
			}
			else
			{
				value.SetValue( szValue );
			}
		}
	}

	WriteUserScript();
	engine->ClientCmd_Unrestricted( "host_writeconfig\n" );
}

void CFoFAdvancedOptionsDialog::WriteUserScript()
{
	CUtlBuffer output( 0, 0, CUtlBuffer::TEXT_BUFFER );
	output.PutString(
		"// This file is automatically regenerated by the FoF advanced options.\n"
		"// User options script\n\n"
		"VERSION 1.0\n\n"
		"DESCRIPTION INFO_OPTIONS\n{\n" );

	for ( int i = 0; i < m_Options.Count(); ++i )
	{
		FoFAdvancedOption_t *pOption = m_Options[i];
		output.PutString( "\t" );
		FoFAdvancedWriteQuoted( output, pOption->cvar.String() );
		output.PutString( "\n\t{\n\t\t" );
		FoFAdvancedWriteQuoted( output, pOption->prompt.String() );
		output.PutString( "\n\t\t{ " );

		switch ( pOption->type )
		{
		case FOF_ADVANCED_BOOL:
			output.PutString( "BOOL }\n" );
			break;
		case FOF_ADVANCED_STRING:
			output.PutString( "STRING }\n" );
			break;
		case FOF_ADVANCED_NUMBER:
			output.Printf(
				"NUMBER %g %g }\n", pOption->minimum, pOption->maximum );
			break;
		case FOF_ADVANCED_LIST:
			output.PutString( "\n\t\t\tLIST\n" );
			for ( int j = 0; j < pOption->choices.Count(); ++j )
			{
				output.PutString( "\t\t\t" );
				FoFAdvancedWriteQuoted(
					output, pOption->choices[j]->label.String() );
				output.PutString( " " );
				FoFAdvancedWriteQuoted(
					output, pOption->choices[j]->value.String() );
				output.PutString( "\n" );
			}
			output.PutString( "\t\t}\n" );
			break;
		}

		output.PutString( "\t\t{ " );
		FoFAdvancedWriteQuoted(
			output, FoFAdvancedGetCurrentValue( pOption ) );
		output.PutString( " }\n\t}\n\n" );
	}
	output.PutString( "}\n" );

	filesystem->CreateDirHierarchy( "cfg", "MOD" );
	filesystem->WriteFile( "cfg/user.scr", "MOD", output );
}
