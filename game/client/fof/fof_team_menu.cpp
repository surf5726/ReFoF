// FoF team selection queries, presentation and initial-team offer.

#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_team_menu.h"
#include "hl2mp_gamerules.h"
#include "c_playerresource.h"
#include "game/client/iviewport.h"
#include "fof/fof_motd.h"
#include "viewport_panel_names.h"

#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Cancel, automatic assignment, spectator, then the four playable factions.
static const char *g_FoFTeamMaterials[FOF_TEAM_BUTTON_COUNT] =
{
	"vgui/tm_cancel",
	"vgui/tm_auto",
	"vgui/tm_spect",
	"vgui/tm_vigilantes_active",
	"vgui/tm_desperados_active",
	"vgui/tm_banditos",
	"vgui/tm_rangers"
};

static const char *g_FoFTeamCommands[FOF_TEAM_BUTTON_COUNT] =
{
	"fof_team_cancel",
	"fof_team_auto",
	"fof_team_spectator",
	"fof_team_2",
	"fof_team_3",
	"fof_team_4",
	"fof_team_5"
};

const char *FoFHudTeamButtonMaterial( int buttonIndex )
{
	Assert( buttonIndex >= 0 && buttonIndex < FOF_TEAM_BUTTON_COUNT );
	if ( buttonIndex < 0 || buttonIndex >= FOF_TEAM_BUTTON_COUNT )
		return "";
	return g_FoFTeamMaterials[buttonIndex];
}

const char *FoFHudTeamButtonCommand( int buttonIndex )
{
	Assert( buttonIndex >= 0 && buttonIndex < FOF_TEAM_BUTTON_COUNT );
	if ( buttonIndex < 0 || buttonIndex >= FOF_TEAM_BUTTON_COUNT )
		return "";
	return g_FoFTeamCommands[buttonIndex];
}

int FoFHudCurrentMode()
{
	return FoFHudConVarInt( "fof_sv_currentmode", 0 );
}

const char *FoFHudTeamIntroSlideName()
{
	switch ( FoFHudCurrentMode() )
	{
	case 1:
		return "mode_shootout";
	case 2:
		return "mode_teamplay";
	case 3:
		return "mode_breakbad";
	default:
		return "";
	}
}

const char *FoFHudTeamIntroLabel()
{
	switch ( FoFHudCurrentMode() )
	{
	case 1:
		return "#intro_shootout_gm";
	case 2:
		return "#intro_teamplay_gm";
	case 3:
		return "#intro_gm_bb";
	default:
		return "";
	}
}

bool FoFHudTeamplayEnabled()
{
	// The FoF rules proxy is authoritative once sign-on has created it.
	// Keep the replicated cvar fallback for the short interval before that
	// entity arrives (and for stripped compatibility test servers).
	if ( HL2MPRules() )
		return HL2MPRules()->IsTeamplay();

	return FoFHudConVarInt( "mp_teamplay", 0 ) != 0;
}

int FoFHudTeamFactionCount()
{
	// Course mode uses teamplay internally so auto-select can place the player
	// on the script's forced faction.  This is also true on a dedicated co-op
	// server: neither local nor remote course sessions expose faction portraits.
	if ( FoFHudCurrentMode() == 6 )
		return 0;

	if ( !FoFHudTeamplayEnabled() )
		return 0;

	// Shootout can run with two, three, or four factions. The rules proxy
	// carries the teamplay boolean; the original server publishes the
	// configured faction count separately.
	ConVar *maxTeams =
		cvar ? cvar->FindVar( "fof_sv_maxteams" ) : NULL;
	if ( maxTeams )
		return clamp( maxTeams->GetInt(), 2, 4 );

	// Keep stripped compatibility servers usable when they omit the FoF
	// cvar. Objective/Break Bad/Versus/Co-op modes are two-sided; for the
	// configurable modes, connected player teams are a useful fallback
	// before retaining all four faction buttons.
	const int currentMode = FoFHudCurrentMode();
	if ( currentMode == 2 || currentMode == 3 ||
		currentMode == 5 || currentMode == 6 )
	{
		return 2;
	}

	bool observed[4] = { false, false, false, false };
	int observedCount = 0;
	if ( g_PR )
	{
		for ( int playerIndex = 1;
			playerIndex <= MAX_PLAYERS;
			++playerIndex )
		{
			if ( !g_PR->IsConnected( playerIndex ) )
				continue;
			const int team = g_PR->GetTeam( playerIndex );
			if ( team < 2 || team > 5 || observed[team - 2] )
				continue;
			observed[team - 2] = true;
			++observedCount;
		}
	}
	return observedCount >= 2 ? clamp( observedCount, 2, 4 ) : 4;
}

int FoFHudBuildTeamFactionList( int factionIds[4] )
{
	const int factionCount = FoFHudTeamFactionCount();
	for ( int slot = 0; slot < factionCount; ++slot )
		factionIds[slot] = slot + 2;
	return factionCount;
}

static int FoFHudTeamVisualFaction( int logicalTeam )
{
	if ( logicalTeam < 2 || logicalTeam > 5 )
		return logicalTeam;

	char remapName[32];
	Q_snprintf(
		remapName,
		sizeof( remapName ),
		"fof_sv_team_remap_%d",
		logicalTeam - 1 );
	return clamp(
		FoFHudConVarInt( remapName, logicalTeam ),
		2,
		5 );
}

int FoFHudTeamFactionMask()
{
	int factionIds[4] = { 0, 0, 0, 0 };
	const int count = FoFHudBuildTeamFactionList( factionIds );
	int mask = 0;
	for ( int i = 0; i < count; ++i )
		mask |= 1 << ( factionIds[i] - 2 );
	return mask;
}

int FoFHudTeamFactionOrdinal( int team )
{
	int factionIds[4] = { 0, 0, 0, 0 };
	const int count = FoFHudBuildTeamFactionList( factionIds );
	for ( int i = 0; i < count; ++i )
	{
		if ( factionIds[i] == team )
			return i;
	}
	return -1;
}

static bool FoFTeamInfoPanelVisible()
{
	if ( !gViewPortInterface )
		return false;

	IViewPortPanel *infoPanel =
		gViewPortInterface->FindPanelByName( PANEL_INFO );
	return infoPanel && infoPanel->IsVisible();
}

void CHudFoF::PaintTeamMenu()
{
	EnsureMenuTextures();

	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int x =
		ScreenWidth() / 2 - (int)( 200.0f * scale );
	const int y =
		ScreenHeight() - (int)( 150.0f * scale );
	DrawMenuTexture(
		m_iTeamBackgroundTexture,
		x,
		y,
		MAX( (int)( 400.0f * scale ), 1 ),
		MAX( (int)( 100.0f * scale ), 1 ) );

	for ( int i = 0; i < FOF_TEAM_BUTTON_COUNT; ++i )
	{
		if ( !m_pTeamMenuButtons[i] ||
			!m_pTeamMenuButtons[i]->IsVisible() )
		{
			continue;
		}

		int buttonX = 0;
		int buttonY = 0;
		int buttonWide = 0;
		int buttonTall = 0;
		m_pTeamMenuButtons[i]->GetBounds(
			buttonX, buttonY, buttonWide, buttonTall );
		const bool armed = m_pTeamMenuButtons[i]->IsArmed();
		Color modulation( 255, 255, 255, 255 );
		if ( armed )
		{
			if ( i < 3 )
			{
				// The original uses white for the normal state and
				// 0xFFAAE6FF (RGBA 255,230,170,255) for armed.
				modulation = Color( 255, 230, 170, 255 );
			}
			else
			{
				static const Color fallbackTeamColors[] =
				{
					Color( 62, 117, 225, 200 ),
					Color( 225, 60, 60, 200 ),
					Color( 220, 180, 20, 200 ),
					Color( 22, 150, 20, 200 )
				};
				const int visualTeam = FoFHudTeamVisualFaction( i - 1 );
				modulation = g_PR
					? g_PR->GetTeamColor( visualTeam )
					: fallbackTeamColors[visualTeam - 2];
				modulation.SetColor(
					modulation.r(),
					modulation.g(),
					modulation.b(),
					200 );
			}
		}
		int texture = m_iTeamButtonTextures[i];
		if ( i >= 3 )
		{
			const int visualTeam = FoFHudTeamVisualFaction( i - 1 );
			texture = m_iTeamButtonTextures[visualTeam + 1];
		}
		DrawMenuTexture(
			( i == 1 && !FoFHudTeamplayEnabled() )
				? m_iTeamAuto2Texture
				: texture,
			buttonX,
			buttonY,
			buttonWide,
			buttonTall,
			modulation );
	}

	if ( !m_pTeamIntroButton || !m_pTeamIntroButton->IsVisible() )
		return;

	int pillX = 0;
	int pillY = 0;
	int pillWide = 0;
	int pillTall = 0;
	m_pTeamIntroButton->GetBounds(
		pillX, pillY, pillWide, pillTall );

	Color modulation( 255, 200, 0, 255 );
	if ( !m_pTeamIntroButton->IsArmed() )
	{
		// CTeamMenuFoF::OnThink modulates unarmed intro buttons between
		// RGB(255,255,140) and RGB(205,205,140) at eight radians/second.
		const int pulse = 255 - RoundFloatToInt(
			fabsf( sinf( gpGlobals->realtime * 8.0f ) ) * 50.0f );
		modulation = Color( pulse, pulse, 140, 255 );
	}
	DrawMenuTexture(
		m_iTeamIntroTexture,
		pillX,
		pillY,
		pillWide,
		pillTall,
		modulation );

	wchar_t howToBuffer[128];
	wchar_t modeBuffer[128];
	const wchar_t *howTo = Localize(
		"#intro_howto", howToBuffer, sizeof( howToBuffer ) );
	const wchar_t *mode = Localize(
		FoFHudTeamIntroLabel(),
		modeBuffer,
		sizeof( modeBuffer ) );
	const vgui::HFont font =
		m_hTeamIntroFont != vgui::INVALID_FONT
			? m_hTeamIntroFont
			: m_hSmallFont;
	const int fontTall = MAX( vgui::surface()->GetFontTall( font ), 1 );
	const int textX = pillX + RoundFloatToInt( 4.0f * scale );
	const int textY = pillY + MAX( ( pillTall - fontTall * 2 ) / 2, 0 );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( 231, 224, 202, 255 );
	vgui::surface()->DrawSetTextPos( textX, textY );
	vgui::surface()->DrawPrintText( howTo, Q_wcslen( howTo ) );
	vgui::surface()->DrawSetTextPos( textX, textY + fontTall );
	vgui::surface()->DrawPrintText( mode, Q_wcslen( mode ) );
}

void CHudFoF::ShowTeamMenu()
{
	if ( FoFHudIsSourceTVClient() )
	{
		m_bTeamMenuOffered = true;
		if ( m_bMenuVisible &&
			( m_iMenuKind == FOF_MENU_TEAM ||
				m_iMenuKind == FOF_MENU_TEAM_CLASS ||
				m_iMenuKind == FOF_MENU_EQUIPMENT ) )
		{
			ClearMenu();
		}
		return;
	}

	static const char *labels[] =
	{
		"Auto-assign",
		"Join team 2",
		"Join team 3",
		"Join team 4",
		"Join team 5",
		"Spectate"
	};
	static const char *commands[] =
	{
		"autojoin",
		"jointeam 2",
		"jointeam 3",
		"jointeam 4",
		"jointeam 5",
		"spectator"
	};
	const bool teamplay = FoFHudTeamplayEnabled();
	const bool courseMode = FoFHudCurrentMode() == 6;
	const bool autoSelectOnly = courseMode || !teamplay;

	// The server can ask for this panel before its InfoPanel string table has
	// finished arriving.  Preserve the request as the standard pending
	// initial-team offer instead of allowing two independent mouse-owning
	// panels to overlap.
	if ( !FoFFirstMotdReadyForNextPanel() ||
		FoFTeamInfoPanelVisible() )
	{
		m_bTeamMenuOffered = false;
		return;
	}

	// CHudMenuFoF and CTeamMenuFoF share one coordinator here. Replacing an
	// assembling or visible server selector here makes its final choice list
	// appear for only a frame.  Do not key this arbitration on currentmode:
	// replicated ConVars can arrive one frame after the first ShowMenuFoF row
	// on a dedicated-server sign-on.  Keep the server-owned selector
	// authoritative; the pending initial-team offer is recreated after that
	// menu is answered.
	if ( m_iMenuKind == FOF_MENU_TEXT )
	{
		m_bTeamMenuOffered = false;
		return;
	}

	if ( IsSlideOpen() )
		CloseSlide();
	ClearMenu();
	m_MenuTitle = "Choose team";
	for ( int i = 0; i < ARRAYSIZE( labels ); ++i )
	{
		// In free-for-all Shootout, the original keeps the same panel and
		// top-row controls but omits all four faction buttons.  tm_auto2 is
		// the shipped "JOIN FREE FOR ALL SHOOTOUT" artwork for the middle
		// autojoin control.
		if ( autoSelectOnly && i >= 1 && i <= 4 )
			continue;

		FoFMenuEntry entry;
		entry.label = ( !teamplay && i == 0 )
			? "Join free-for-all Shootout"
			: labels[i];
		entry.commandId = 0;
		entry.clientCommand = commands[i];
		entry.presetCount = 0;
		entry.nextPage = -1;
		entry.encodedKind = '\0';
		entry.encodedValue = 0;
		entry.encodedQuantity = 0;
		entry.encodedLabel.Clear();
		entry.encodedValid = false;
		m_MenuEntries.AddToTail( entry );
	}
	m_bMenuVisible = true;
	m_bLocalMenu = true;
	m_iMenuKind = FOF_MENU_TEAM;
	m_bTeamMenuOffered = true;
	// Set the mask before the first layout pass.  The constructor's -1
	// sentinel otherwise makes every faction button visible until OnThink
	// gets a chance to refresh it, which is especially noticeable when a
	// local course opens its three-control team menu.
	m_iTeamFactionMask = FoFHudTeamFactionMask();
	LayoutMenuControls();
	SetMenuControlsVisible( true );
	BeginLocalMenuInput();
}
