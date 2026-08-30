//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_hl2mp_player.h"
#include "c_playerresource.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"
#include "hl2mp_gamerules.h"
#include "c_team.h"
#include <vgui_controls/AnimationController.h>
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CTeamPlayHud : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CTeamPlayHud, vgui::Panel );

public:
	CTeamPlayHud( const char *pElementName );
	void Reset();

	virtual void PerformLayout();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

private:
	vgui::HFont m_hFont;
	Color		m_bgColor;

	vgui::Label *m_pWarmupLabel;	// "Warmup Mode"

	vgui::Label *m_pBackground;		// black box

	bool m_bSuitAuxPowerUsed;

	CPanelAnimationVarAliasType( int, m_iTextX, "text_xpos", "8", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextY, "text_ypos", "8", "proportional_int" );
};

DECLARE_HUDELEMENT( CTeamPlayHud );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTeamPlayHud::CTeamPlayHud( const char *pElementName ) : BaseClass(NULL, "TeamDisplay"), CHudElement( pElementName )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetVisible( false );
	SetAlpha( 255 );

	m_pBackground = new vgui::Label( this, "Background", "" );

	m_pWarmupLabel = new vgui::Label( this, "RoundState_warmup", "test label" /*g_pVGuiLocalize->Find( "#Clan_warmup_mode" )*/ );
	m_pWarmupLabel->SetPaintBackgroundEnabled( false );
	m_pWarmupLabel->SetPaintBorderEnabled( false );
	m_pWarmupLabel->SizeToContents();
	m_pWarmupLabel->SetContentAlignment( vgui::Label::a_west );
	m_pWarmupLabel->SetFgColor( GetFgColor() );

	m_bSuitAuxPowerUsed = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamPlayHud::Reset()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamPlayHud::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( Color(0,0,0,0) );	//GetSchemeColor("RoundStateFg", pScheme) );
	m_hFont = pScheme->GetFont( "Default", true );

	m_pBackground->SetBgColor( GetSchemeColor("BgColor", pScheme) );
	m_pBackground->SetPaintBackgroundType( 2 );

	SetAlpha( 255 );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBackgroundType( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Resizes the label
//-----------------------------------------------------------------------------
void CTeamPlayHud::PerformLayout()
{

	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );

	// find the widest line
	int labelWide = m_pWarmupLabel->GetWide();

	// find the total height
	int fontTall = vgui::surface()->GetFontTall( m_hFont );
	int labelTall = fontTall;

	labelWide += m_iTextX*2;
	labelTall += m_iTextY*2;

	m_pBackground->SetBounds( 0, 0, labelWide, labelTall );

	int xOffset = (labelWide - m_pWarmupLabel->GetWide())/2;
	m_pWarmupLabel->SetPos( 0 + xOffset, 0 + m_iTextY );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CTeamPlayHud::OnThink()
{
	// FoF supplies its own team and mode HUDs.  The stock HL2DM panel is
	// disabled in HudLayout.res, but its inherited implementation calls
	// SetVisible( true ) again every frame while mp_teamplay is enabled.  That
	// leaves the empty rounded TeamDisplay background at the lower left.
	SetVisible( false );
}
