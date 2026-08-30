#include "cbase.h"
#include "fof/fof_advanced_options.h"
#include "fof/fof_options_dialog.h"

#include "ienginevgui.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CFoFOptionsDialog *g_pFoFOptionsDialog = NULL;

static int FoFReadConVarInt( const char *pszName, int iFallback )
{
	ConVarRef value( pszName, true );
	return value.IsValid() ? value.GetInt() : iFallback;
}

static void FoFWriteConVarInt( const char *pszName, int iValue )
{
	ConVarRef value( pszName, true );
	if ( value.IsValid() )
		value.SetValue( iValue );
}

CFoFOptionsDialog::CFoFOptionsDialog()
	: BaseClass( NULL, "FoFOptionsPanel", true, true )
	, m_pRed( NULL )
	, m_pGreen( NULL )
	, m_pBlue( NULL )
	, m_pSmoke( NULL )
	, m_pVisualQuality( NULL )
	, m_pFov( NULL )
	, m_pViewmodelFov( NULL )
	, m_pBodyAwareness( NULL )
	, m_pAdvancedOptions( NULL )
{
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetScheme( vgui::scheme()->LoadSchemeFromFile(
		"resource/SourceScheme.res", "SourceScheme" ) );
	SetProportional( false );
	SetDeleteSelfOnClose( false );

	m_pRed = new vgui::Slider( this, "RedCrosshairSlider" );
	m_pGreen = new vgui::Slider( this, "GreenCrosshairSlider" );
	m_pBlue = new vgui::Slider( this, "BlueCrosshairSlider" );
	m_pSmoke = new vgui::Slider( this, "SmokeSlider" );
	m_pVisualQuality = new vgui::Slider( this, "FireSlider" );
	m_pFov = new vgui::Slider( this, "FOVSlider" );
	m_pViewmodelFov = new vgui::Slider( this, "viewmodelFOVSlider" );
	m_pBodyAwareness = new vgui::CheckButton(
		this, "BodyAwarenessButton", "#Options_BodyAwareness" );

	ConfigureSlider( m_pRed, 0, 255 );
	ConfigureSlider( m_pGreen, 0, 255 );
	ConfigureSlider( m_pBlue, 0, 255 );
	ConfigureSlider( m_pSmoke, 0, 5, 6 );
	ConfigureSlider( m_pVisualQuality, 0, 2, 2 );
	ConfigureSlider( m_pFov, 75, 90 );
	ConfigureSlider( m_pViewmodelFov, 40, 50 );
	m_pBodyAwareness->AddActionSignalTarget( this );

	LoadControlSettings( "resource/ui/FoFOptionsPanel.res" );
	ReadSettings();
	SetVisible( false );
}

void CFoFOptionsDialog::ConfigureSlider(
	vgui::Slider *pSlider,
	int iMinimum,
	int iMaximum,
	int iTicks )
{
	pSlider->SetRange( iMinimum, iMaximum );
	if ( iTicks > 0 )
		pSlider->SetNumTicks( iTicks );
	pSlider->AddActionSignalTarget( this );
}

void CFoFOptionsDialog::ReadSettings()
{
	m_pRed->SetValue( FoFReadConVarInt( "cl_crosshair_r", 0 ), false );
	m_pGreen->SetValue( FoFReadConVarInt( "cl_crosshair_g", 250 ), false );
	m_pBlue->SetValue( FoFReadConVarInt( "cl_crosshair_b", 0 ), false );
	m_pSmoke->SetValue( FoFReadConVarInt( "fof_smoke", 3 ), false );
	m_pVisualQuality->SetValue(
		FoFReadConVarInt( "fof_visual_quality", 2 ), false );
	m_pFov->SetValue( FoFReadConVarInt( "fov_desired", 90 ), false );
	m_pViewmodelFov->SetValue(
		FoFReadConVarInt( "viewmodel_fov", 45 ), false );
	m_pBodyAwareness->SetSelected(
		FoFReadConVarInt( "fof_bodyawareness", 1 ) != 0 );
	Repaint();
}

void CFoFOptionsDialog::ApplySettings()
{
	FoFWriteConVarInt( "cl_crosshair_r", m_pRed->GetValue() );
	FoFWriteConVarInt( "cl_crosshair_g", m_pGreen->GetValue() );
	FoFWriteConVarInt( "cl_crosshair_b", m_pBlue->GetValue() );
	FoFWriteConVarInt(
		"fof_bodyawareness", m_pBodyAwareness->IsSelected() ? 1 : 0 );
	FoFWriteConVarInt( "fof_smoke", m_pSmoke->GetValue() );
	FoFWriteConVarInt(
		"fof_visual_quality", m_pVisualQuality->GetValue() );
	// FoF deliberately keeps both fire-quality controls in lockstep.
	FoFWriteConVarInt( "fof_firequality", m_pVisualQuality->GetValue() );
	FoFWriteConVarInt( "fov_desired", m_pFov->GetValue() );
	FoFWriteConVarInt( "viewmodel_fov", m_pViewmodelFov->GetValue() );
}

void CFoFOptionsDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	// Preserve FoF's 1024x768 placement and move it with the centre of
	// other resolutions instead of leaving the fixed resource coordinates
	// stranded in a corner.
	const int iX = 368 + ( ScreenWidth() - 1024 ) / 2;
	const int iY = 233 + ( ScreenHeight() - 768 ) / 2;
	SetPos(
		clamp( iX, 0, MAX( ScreenWidth() - GetWide(), 0 ) ),
		clamp( iY, 0, MAX( ScreenHeight() - GetTall(), 0 ) ) );
}

void CFoFOptionsDialog::Paint()
{
	BaseClass::Paint();
	vgui::surface()->DrawSetColor(
		m_pRed->GetValue(),
		m_pGreen->GetValue(),
		m_pBlue->GetValue(),
		255 );
	vgui::surface()->DrawFilledRect( 6, 65, 9, 160 );
}

void CFoFOptionsDialog::OnSliderMoved( KeyValues *pData )
{
	NOTE_UNUSED( pData );
	Repaint();
}

void CFoFOptionsDialog::ShowDialog()
{
	ReadSettings();
	PerformLayout();
	SetVisible( true );
	SetEnabled( true );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	Activate();
}

void CFoFOptionsDialog::HideDialog()
{
	SetVisible( false );
}

void CFoFOptionsDialog::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "close" ) )
	{
		ApplySettings();
		HideDialog();
		return;
	}
	if ( !Q_stricmp( pszCommand, "Advanced" ) )
	{
		if ( !m_pAdvancedOptions )
			m_pAdvancedOptions = new CFoFAdvancedOptionsDialog();
		m_pAdvancedOptions->ShowDialog();
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFOptionsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		HideDialog();
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

CON_COMMAND( OpenFoFOptionsDialog, "Open the Fistful of Frags options." )
{
	if ( !g_pFoFOptionsDialog )
		g_pFoFOptionsDialog = new CFoFOptionsDialog();
	g_pFoFOptionsDialog->ShowDialog();
}
