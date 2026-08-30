//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF HUD target ID element
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
#include "vgui/ISurface.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_hud.h"
#include "fof/fof_client_settings.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLAYER_HINT_DISTANCE	150
#define PLAYER_HINT_DISTANCE_SQ	(PLAYER_HINT_DISTANCE*PLAYER_HINT_DISTANCE)

static ConVar hud_centerid( "hud_centerid", "1" );
static ConVar hud_showtargetid( "hud_showtargetid", "1" );

static void FoFTargetIdEnemySettingChanged(
	IConVar *pVariable,
	const char *,
	float )
{
	ConVarRef value( pVariable->GetName(), true );
	if ( value.IsValid() )
	{
		FoFWritePersistentClientInt(
			pVariable->GetName(), value.GetBool() ? 1 : 0 );
	}
}

static ConVar fof_hud_targetid_show_enemies(
	"fof_hud_targetid_show_enemies",
	"0",
	0,
	"Show enemy player information in the target ID HUD.",
	true,
	0.0f,
	true,
	1.0f,
	FoFTargetIdEnemySettingChanged );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTargetID : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTargetID, vgui::Panel );

public:
	CTargetID( const char *pElementName );
	void Init( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	OnScreenSizeChanged( int iOldWide, int iOldTall );
	virtual void	Paint( void );
	void VidInit( void );

private:
	Color			GetColorForTargetTeam( int iTeamNumber );

	vgui::HFont		m_hFont;
	int				m_iFontTall;
	int				m_iLastEntIndex;
	float			m_flLastChangeTime;
};

DECLARE_HUDELEMENT( CTargetID );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTargetID::CTargetID( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "TargetID" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hFont = vgui::INVALID_FONT;
	m_iFontTall = -1;
	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: Setup
//-----------------------------------------------------------------------------
void CTargetID::Init( void )
{
	const int showEnemies = clamp(
		FoFReadPersistentClientInt(
			"fof_hud_targetid_show_enemies",
			fof_hud_targetid_show_enemies.GetInt() ),
		0,
		1 );
	fof_hud_targetid_show_enemies.SetValue( showEnemies );
};

void CTargetID::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	// The fixed scheme face produces a roughly 14-pixel ink box at 1080p,
	// while full proportional scaling produces about 28 pixels.  The shipped
	// HUD reference is about 18 pixels, so retain one quarter of the scaling
	// above its 16-pixel, 640x480 baseline instead of using either extreme.
	const int proportionalTall = YRES( 16 );
	const int requestedTall = 16 + RoundFloatToInt(
		(float)( proportionalTall - 16 ) * 0.25f );
	if ( m_hFont == vgui::INVALID_FONT || m_iFontTall != requestedTall )
	{
		const vgui::HFont font = vgui::surface()->CreateFont();
		if ( font != vgui::INVALID_FONT &&
			vgui::surface()->SetFontGlyphSet(
				font,
				"Typodermic",
				requestedTall,
				200,
				0,
				0,
				vgui::ISurface::FONTFLAG_ANTIALIAS |
					vgui::ISurface::FONTFLAG_DROPSHADOW ) )
		{
			m_hFont = font;
			m_iFontTall = requestedTall;
		}
	}

	if ( m_hFont == vgui::INVALID_FONT )
		m_hFont = scheme->GetFont( "TargetID", false );
	if ( m_hFont == vgui::INVALID_FONT )
		m_hFont = scheme->GetFont( "Default", false );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: clear out string etc between levels
//-----------------------------------------------------------------------------
void CTargetID::VidInit()
{
	CHudElement::VidInit();

	// Dynamically created VGUI fonts do not survive every video-mode reset.
	// Force the next scheme pass to rebuild this resolution-scaled face rather
	// than retaining a stale, numerically valid handle after a hot mode change.
	m_hFont = vgui::INVALID_FONT;
	m_iFontTall = -1;
	InvalidateLayout( true, true );
	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;
}

void CTargetID::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	BaseClass::OnScreenSizeChanged( iOldWide, iOldTall );

	// Hot video-mode changes notify VGUI independently of HudVidInit.  Rebuild
	// the dynamic face on this path as well so neither callback ordering can
	// leave TargetID holding a font from the old surface.
	m_hFont = vgui::INVALID_FONT;
	m_iFontTall = -1;
	InvalidateLayout( true, true );
}

Color CTargetID::GetColorForTargetTeam( int iTeamNumber )
{
	return GameResources()->GetTeamColor( iTeamNumber );
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CTargetID::Paint()
{
#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	sIDString[0] = 0;
	if ( FoFHudIsSpectatorClient() )
	{
		m_flLastChangeTime = 0.0f;
		m_iLastEntIndex = 0;
		return;
	}

	C_HL2MP_Player *pPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();

	if ( !pPlayer )
		return;

	Color c;

	// Get our target's ent index
	int iEntIndex = pPlayer->GetIDTarget();
	// Didn't find one?
	if ( !iEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (gpGlobals->curtime > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			m_iLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iEntIndex = m_iLastEntIndex;
		}
	}
	else
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}

	// Is this an entindex sent by the server?
	if ( iEntIndex )
	{
		C_BasePlayer *pPlayer = static_cast<C_BasePlayer*>(cl_entitylist->GetEnt( iEntIndex ));
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		const char *printFormatString = NULL;
		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszHealthText[ 10 ];
		bool bShowHealth = false;
		bool bShowPlayerName = false;
		bool bEnemyTarget = false;

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
		// Is it a player?
		if ( IsPlayerIndex( iEntIndex ) )
		{
			if ( !pPlayer || !pLocalPlayer )
			{
				return;
			}

			c = GetColorForTargetTeam( pPlayer->GetTeamNumber() );

			bShowPlayerName = true;
			g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );

			const bool bSameTeam =
				HL2MPRules()->IsTeamplay() == true &&
				pPlayer->InSameTeam( pLocalPlayer );
			if ( bSameTeam )
			{
				printFormatString = "#Playerid_sameteam";
				bShowHealth = true;
			}
			else
			{
				if ( !fof_hud_targetid_show_enemies.GetBool() )
				{
					// Do not retain an enemy in the half-second target-ID
					// grace period after the persistent option is disabled.
					m_flLastChangeTime = 0.0f;
					m_iLastEntIndex = 0;
					return;
				}
				printFormatString = "#Playerid_diffteam";
				bShowHealth = true;
				bEnemyTarget = true;
			}


			if ( bShowHealth )
			{
				_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f%%",  ((float)pPlayer->GetHealth() / (float)pPlayer->GetMaxHealth() ) );
				wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';
			}
		}

		if ( printFormatString )
		{
			if ( bEnemyTarget && bShowPlayerName && bShowHealth )
			{
				// Reuse installed localization tokens so the enemy health display
				// remains language-aware without additional resources.
				const wchar_t *pEnemyFormat =
					g_pVGuiLocalize->Find( "#Playerid_diffteam" );
				const wchar_t *pHealthFormat =
					g_pVGuiLocalize->Find( "#Playerid_noteam" );
				if ( pEnemyFormat && pHealthFormat )
				{
					wchar_t wszEnemyText[ MAX_ID_STRING ];
					wchar_t wszHealthSuffix[ MAX_ID_STRING ];
					wchar_t wszEmpty[] = L"";
					g_pVGuiLocalize->ConstructString(
						wszEnemyText, sizeof( wszEnemyText ),
						pEnemyFormat, 1, wszPlayerName );
					g_pVGuiLocalize->ConstructString(
						wszHealthSuffix, sizeof( wszHealthSuffix ),
						pHealthFormat, 2, wszEmpty, wszHealthText );
					_snwprintf(
						sIDString, ARRAYSIZE( sIDString ) - 1,
						L"%s%s", wszEnemyText, wszHealthSuffix );
					sIDString[ ARRAYSIZE( sIDString ) - 1 ] = L'\0';
				}
				else if ( pEnemyFormat )
				{
					g_pVGuiLocalize->ConstructString(
						sIDString, sizeof( sIDString ), pEnemyFormat,
						1, wszPlayerName );
				}
				else if ( pHealthFormat )
				{
					g_pVGuiLocalize->ConstructString(
						sIDString, sizeof( sIDString ), pHealthFormat,
						2, wszPlayerName, wszHealthText );
				}
			}
			else if ( bShowPlayerName && bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 2, wszPlayerName, wszHealthText );
			}
			else if ( bShowPlayerName )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszPlayerName );
			}
			else if ( bShowHealth )
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 1, wszHealthText );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), g_pVGuiLocalize->Find(printFormatString), 0 );
			}
		}

		if ( sIDString[0] )
		{
			int wide, tall;
			const int ypos = YRES(400);

			vgui::surface()->GetTextSize( m_hFont, sIDString, wide, tall );
			const int xpos = ( ScreenWidth() - wide ) / 2;

			vgui::surface()->DrawSetTextFont( m_hFont );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawSetTextColor( c );
			vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );
		}
	}
}
