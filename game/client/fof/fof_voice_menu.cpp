// FoF voice-command HUD and its exact client/server wire command.

#include "cbase.h"
#include "fof/fof_voice_menu.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudVoiceComm : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudVoiceComm, vgui::Panel );

public:
	CHudVoiceComm( const char *elementName )
		: CHudElement( elementName )
		, BaseClass( NULL, "HudVoiceComm" )
		, m_iMenuKind( 0 )
		, m_flCloseAt( 0.0f )
		, m_hFont( vgui::INVALID_FONT )
	{
		SetParent( g_pClientMode->GetViewport() );
		// scripts/hudlayout.res gives HudVoiceComm a type-2 rounded panel.
		// Use ClientScheme's Panel.BgColor instead of painting an opaque block.
		SetPaintBackgroundEnabled( true );
		SetPaintBackgroundType( 2 );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetHiddenBits( 0 );
	}

	virtual void Init() { Reset(); }
	virtual void Reset()
	{
		m_iMenuKind = 0;
		m_flCloseAt = 0.0f;
	}

	virtual bool ShouldDraw()
	{
		return m_iMenuKind > 0 && CHudElement::ShouldDraw();
	}

	virtual void ApplySchemeSettings( vgui::IScheme *scheme )
	{
		BaseClass::ApplySchemeSettings( scheme );
		SetPaintBackgroundEnabled( true );
		SetPaintBackgroundType( 2 );
		SetBgColor( scheme->GetColor(
			"Panel.BgColor", Color( 162, 118, 23, 76 ) ) );
		m_hFont = scheme->GetFont( "DefaultSmall", true );
		if ( m_hFont == vgui::INVALID_FONT )
			m_hFont = scheme->GetFont( "Default", true );
		Layout();
	}

	virtual void OnThink()
	{
		Layout();
		if ( m_iMenuKind > 0 && gpGlobals &&
			gpGlobals->curtime >= m_flCloseAt )
		{
			Reset();
		}
	}

	virtual void Paint()
	{
		if ( m_iMenuKind < 1 || m_iMenuKind > 3 )
			return;

		static const char *prefixes[] =
		{
			"", "Command", "Alert", "Taunt"
		};
		static const int counts[] = { 0, 6, 7, 9 };
		const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );

		vgui::surface()->DrawSetTextFont( m_hFont );
		vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
		for ( int item = 1; item <= counts[m_iMenuKind]; ++item )
		{
			char token[32];
			Q_snprintf(
				token,
				sizeof( token ),
				"#%s%d",
				prefixes[m_iMenuKind],
				item );
			const wchar_t *text = g_pVGuiLocalize->Find( token );
			wchar_t fallback[64];
			if ( !text )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(
					token, fallback, sizeof( fallback ) );
				text = fallback;
			}
			vgui::surface()->DrawSetTextPos(
				RoundFloatToInt( 5.0f * scale ),
				RoundFloatToInt(
					( 5.0f + ( item - 1 ) * 13.0f ) * scale ) );
			vgui::surface()->DrawPrintText( text, V_wcslen( text ) );
		}
	}

	void Toggle( int kind )
	{
		if ( m_iMenuKind > 0 )
		{
			Reset();
			return;
		}
		if ( kind < 1 || kind > 3 )
			return;
		m_iMenuKind = kind;
		m_flCloseAt = ( gpGlobals ? gpGlobals->curtime : 0.0f ) + 5.0f;
		Layout();
	}

	bool SelectDisplaySlot( int slot )
	{
		static const int counts[] = { 0, 6, 7, 9 };
		if ( m_iMenuKind < 1 || m_iMenuKind > 3 ||
			slot < 1 || slot > counts[m_iMenuKind] )
		{
			return false;
		}
		char command[32];
		// FoF sends the display slot first, biased by one, followed by
		// the 1-based menu kind.  The original server subtracts that bias and
		// adds 0/10/20 for command/alert/taunt respectively.  Reversing these
		// arguments makes most entries play the wrong voice (or sound 0/10/20,
		// which do not exist).
		Q_snprintf(
			command, sizeof( command ), "vc %i %i", slot + 1, m_iMenuKind );
		engine->ClientCmd( command );
		Reset();
		return true;
	}

	bool Close()
	{
		if ( m_iMenuKind <= 0 )
			return false;
		Reset();
		return true;
	}

	bool IsOpen() const { return m_iMenuKind > 0; }

private:
	void Layout()
	{
		const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
		SetBounds(
			RoundFloatToInt( 110.0f * scale ),
			RoundFloatToInt( 330.0f * scale ),
			MAX( RoundFloatToInt( 110.0f * scale ), 1 ),
			MAX( RoundFloatToInt( 135.0f * scale ), 1 ) );
	}

	int m_iMenuKind;
	float m_flCloseAt;
	vgui::HFont m_hFont;
};

DECLARE_HUDELEMENT( CHudVoiceComm );

static CHudVoiceComm *FoFVoiceMenu()
{
	return GET_HUDELEMENT( CHudVoiceComm );
}

bool FoFVoiceMenuIsOpen()
{
	CHudVoiceComm *menu = FoFVoiceMenu();
	return menu && menu->IsOpen();
}

bool FoFVoiceMenuSelectDisplaySlot( int slot )
{
	CHudVoiceComm *menu = FoFVoiceMenu();
	return menu && menu->SelectDisplaySlot( slot );
}

bool FoFVoiceMenuClose()
{
	CHudVoiceComm *menu = FoFVoiceMenu();
	return menu && menu->Close();
}

void FoFVoiceMenuToggle( int kind )
{
	CHudVoiceComm *menu = FoFVoiceMenu();
	if ( menu )
		menu->Toggle( kind );
}

CON_COMMAND( voicecomm, "Open the FoF command voice menu." )
{
	FoFVoiceMenuToggle( 1 );
}

CON_COMMAND( voicealert, "Open the FoF alert voice menu." )
{
	FoFVoiceMenuToggle( 2 );
}

CON_COMMAND( voicetaunt, "Open the FoF taunt voice menu." )
{
	FoFVoiceMenuToggle( 3 );
}
