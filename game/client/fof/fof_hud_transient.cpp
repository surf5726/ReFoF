// FoF transient HUD presentation and network-fed notices.

#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_player_status.h"
#include "hud.h"
#include "c_baseplayer.h"
#include "c_basecombatweapon.h"
#include "c_playerresource.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_properties.h"
#include "cdll_util.h"
#include "engine/IEngineSound.h"
#include "tier0/vprof.h"
#include "view.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "hud_macros.h"
#include <vgui_controls/CircularProgressBar.h>
#include "fof/fof_hud_transient.h"
#include "fof/fof_team_menu.h"
#include "game/client/iviewport.h"
#include "hl2mp_gamerules.h"
#include "hltvcamera.h"
#include "iclientmode.h"
#include "viewport_panel_names.h"
#include <vgui/IScheme.h>
#include "client_textmessage.h"
#include <vgui_controls/Controls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CHudTexture *FoFBBMarkerIcon(
	CHudTexture *const *icons, int iconType )
{
	switch ( iconType )
	{
	case 1:
		return icons[1];
	case 2:
		return icons[2];
	case 3:
		return icons[4];
	case 6:
		return icons[3];
	default:
		return icons[0];
	}
}

static const char *FoFBBMarkerTextToken( int iconType )
{
	static const char *tokens[] =
	{
		"#BB_BuyArea",
		"#BB_LootPickArea",
		"#BB_LootDropArea",
		"#BB_DisarmArea",
		"#GT_SkullSpawn",
		"#BB_PublicEnemy_Obj",
		"#BB_WhiskeyArea",
	};
	return iconType >= 0 && iconType < ARRAYSIZE( tokens ) ?
		tokens[iconType] : NULL;
}

static Color FoFBBMarkerColor( int iconType, float currentTime )
{
	const float pulse = fabsf( sinf( currentTime * 8.0f ) );
	switch ( iconType )
	{
	case 1:
		return Color( 160, 230, 160, 255 );
	case 2:
	case 4:
		{
			const int shade = 105 + RoundFloatToInt( pulse * 80.0f );
			return Color( shade, 255, shade, 255 );
		}
	case 3:
		return Color( 240, 180, 220, 255 );
	case 5:
		return Color(
			clamp( 200 + RoundFloatToInt( pulse * 80.0f ), 0, 255 ),
			50, 50, 255 );
	case 6:
		return Color( 217, 190, 43, 255 );
	case 7:
		{
			const int shade = 155 + RoundFloatToInt( pulse * 30.0f );
			return Color( shade, shade, shade, 255 );
		}
	default:
		return Color( 255, 255, 255, 255 );
	}
}

static void FoFDrawCenteredHintText(
	vgui::HFont font,
	const wchar_t *text,
	int centerX,
	int y,
	const Color &color )
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( font, text, wide, tall );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( centerX - wide / 2, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

static void FoFBBMarkerText(
	const FoFBBMarker &marker,
	int distanceMeters,
	wchar_t *output,
	int outputBytes )
{
	if ( !output || outputBytes < (int)sizeof( wchar_t ) )
		return;
	output[0] = L'\0';

	if ( marker.iconType == 7 )
	{
		const wchar_t *localized = marker.text.IsEmpty() ? NULL :
			g_pVGuiLocalize->Find( marker.text.String() );
		if ( localized )
		{
			V_wcsncpy( output, localized, outputBytes );
			return;
		}

		char fallback[256];
		Q_snprintf(
			fallback,
			sizeof( fallback ),
			"%s %im.",
			marker.text.IsEmpty() ? "error" : marker.text.String(),
			distanceMeters );
		g_pVGuiLocalize->ConvertANSIToUnicode(
			fallback, output, outputBytes );
		return;
	}

	const char *token = FoFBBMarkerTextToken( marker.iconType );
	const wchar_t *format = token ? g_pVGuiLocalize->Find( token ) : NULL;
	if ( format )
	{
		wchar_t distance[16];
		V_snwprintf(
			distance, ARRAYSIZE( distance ), L"%i", distanceMeters );
		g_pVGuiLocalize->ConstructString(
			output, outputBytes, format, 1, distance );
		return;
	}

	g_pVGuiLocalize->ConvertANSIToUnicode(
		token ? token : "error", output, outputBytes );
}

const wchar_t *CHudFoF::Localize( const char *text, wchar_t *buffer, int bufferBytes ) const
{
	if ( !text || !text[0] )
	{
		buffer[0] = L'\0';
		return buffer;
	}

	if ( text[0] == '#' )
	{
		const wchar_t *localized = g_pVGuiLocalize->Find( text );
		if ( localized )
			return localized;
	}

	g_pVGuiLocalize->ConvertANSIToUnicode( text, buffer, bufferBytes );
	return buffer;
}

void CHudFoF::PaintSourceTVPlayerInfo()
{
	if ( !FoFHudIsSourceTVClient() || !g_PR ||
		m_hSourceTVPlayerFont == vgui::INVALID_FONT )
	{
		return;
	}

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer || localPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		return;

	C_BasePlayer *focusedPlayer = NULL;
	const int cameraMode = HLTVCamera()->GetMode();
	if ( cameraMode == OBS_MODE_IN_EYE || cameraMode == OBS_MODE_CHASE )
	{
		focusedPlayer = ToBasePlayer(
			HLTVCamera()->GetPrimaryTarget() );
	}

	for ( int playerIndex = 1;
		playerIndex <= gpGlobals->maxClients && playerIndex <= MAX_PLAYERS;
		++playerIndex )
	{
		C_BasePlayer *player = UTIL_PlayerByIndex( playerIndex );
		if ( !player || player == focusedPlayer || !player->IsAlive() )
			continue;

		const float distance =
			( MainViewOrigin() - player->GetAbsOrigin() ).Length();
		Vector screenOffset(
			0.0f,
			0.0f,
			RemapValClamped( distance, 100.0f, 4000.0f, 80.0f, 55.0f ) );
		int screenX = 0;
		int screenY = 0;
		if ( !GetVectorInScreenSpace(
			player->GetAbsOrigin(), screenX, screenY, &screenOffset ) )
		{
			continue;
		}

		const char *playerName = g_PR->GetPlayerName( playerIndex );
		if ( !playerName || !playerName[0] )
			continue;

		char text[96];
		wchar_t wideText[96];
		Q_snprintf(
			text, sizeof( text ), "%s (%ihp)",
			playerName, player->GetHealth() );
		g_pVGuiLocalize->ConvertANSIToUnicode(
			text, wideText, sizeof( wideText ) );

		vgui::surface()->DrawSetTextColor(
			g_PR->GetTeamColor( player->GetTeamNumber() ) );
		vgui::surface()->DrawSetTextFont( m_hSourceTVPlayerFont );
		vgui::surface()->DrawSetTextPos( screenX, screenY );
		vgui::surface()->DrawPrintText(
			wideText, Q_wcslen( wideText ) );
	}
}

void CHudFoF::Paint()
{
	VPROF_BUDGET( "FoF::HUD::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	if ( m_bGoodBadVisible )
	{
		PaintGoodBad();
		return;
	}

	PaintMenu();

	const float now = gpGlobals->curtime;
	PaintSlide();
	if ( m_bSlideVisible )
		return;

	const bool drawSourceTVPlayerInfo = FoFHudIsSourceTVClient();
	const bool hasAdditionalContent =
		( m_bHintVisible && !m_Hint.IsEmpty() ) ||
		( m_bStatVisible && !m_StatLabel.IsEmpty() ) ||
		IsCaptureMessageVisible() ||
		m_CaptureMarkers.Count() > 0 ||
		m_IconComms.Count() > 0 ||
		m_BBMarkers.Count() > 0 ||
		m_BBNotices.Count() > 0 ||
		m_HitMarkers.Count() > 0 ||
		drawSourceTVPlayerInfo;
	if ( !hasAdditionalContent )
		return;

	if ( m_bHintVisible && !m_Hint.IsEmpty() )
	{
		wchar_t localizedHintBuffer[512];
		const wchar_t *localizedHint = Localize(
			m_Hint.String(),
			localizedHintBuffer,
			sizeof( localizedHintBuffer ) );
		const float mainProgress = clamp(
			( now - m_flHintStartedAt ) / 1.5f,
			0.0f,
			1.0f );
		const int mainY = FoFHudScale(
			490.0f - 70.0f * mainProgress );
		FoFDrawCenteredHintText(
			m_hHintFont,
			localizedHint,
			ScreenWidth() / 2,
			mainY,
			Color( 255, 238, 190, 255 ) );

		static const char *bindings[] =
		{
			"forward", "back", "jump", "duck", "walk", "reload", "attack"
		};
		if ( m_iHintMode >= 0 &&
			m_iHintMode < ARRAYSIZE( bindings ) &&
			now >= m_flHintPromptAt )
		{
			const char *key =
				engine->Key_LookupBinding( bindings[m_iHintMode] );
			if ( !key || !key[0] )
				key = "not bound";

			wchar_t keyWide[128];
			wchar_t formatBuffer[256];
			wchar_t prompt[512];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				key, keyWide, sizeof( keyWide ) );
			const wchar_t *format = Localize(
				"#FoF_Hint_Key", formatBuffer, sizeof( formatBuffer ) );
			g_pVGuiLocalize->ConstructString(
				prompt, sizeof( prompt ), format, 1, keyWide );
			const float promptProgress = RemapValClamped(
				now - m_flHintPromptAt,
				0.0f,
				1.5f,
				0.0f,
				1.0f );
			const int promptY = FoFHudScale(
				490.0f - 50.0f * promptProgress );
			FoFDrawCenteredHintText(
				m_hHintFont,
				prompt,
				ScreenWidth() / 2,
				promptY,
				Color( 225, 202, 154, 255 ) );
		}
	}

	if ( m_bStatVisible && !m_StatLabel.IsEmpty() )
	{
		wchar_t labelBuffer[512];
		const wchar_t *format =
			Localize( m_StatLabel.String(), labelBuffer, sizeof( labelBuffer ) );
		wchar_t newValue[32];
		wchar_t oldValue[32];
		wchar_t output[1024];
		V_snwprintf( newValue, ARRAYSIZE( newValue ), L"%d", m_iStatNewValue );
		V_snwprintf( oldValue, ARRAYSIZE( oldValue ), L"%d", m_iStatOldValue );
		g_pVGuiLocalize->ConstructString(
			output, sizeof( output ), format, 2, newValue, oldValue );

		const int boxWide = MIN(
			FoFHudScale( 680.0f ),
			MAX( ScreenWidth() - FoFHudScale( 40.0f ), 1 ) );
		const bool showPrompt = now >= m_flStatPromptAt;
		const int boxTall = FoFHudScale(
			showPrompt ? 82.0f : 54.0f );
		const int boxX = ( ScreenWidth() - boxWide ) / 2;
		const int boxY = ScreenHeight() / 5 + FoFHudScale( 30.0f );
		vgui::surface()->DrawSetColor( 18, 12, 8, 215 );
		vgui::surface()->DrawFilledRect(
			boxX, boxY, boxX + boxWide, boxY + boxTall );
		vgui::surface()->DrawSetColor( 190, 145, 72, 245 );
		vgui::surface()->DrawOutlinedRect(
			boxX, boxY, boxX + boxWide, boxY + boxTall );
		DrawWide(
			output,
			ScreenWidth() / 2,
			boxY + FoFHudScale( 16.0f ),
			Color( 255, 238, 190, 255 ),
			true );

		if ( showPrompt )
		{
			wchar_t promptBuffer[256];
			DrawWide(
				Localize(
					"#FoF_Stat_Key",
					promptBuffer,
					sizeof( promptBuffer ) ),
				ScreenWidth() / 2,
				boxY + FoFHudScale( 46.0f ),
				Color( 225, 202, 154, 255 ),
				true );
		}
	}

	PaintCaptureMessage();

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	for ( int commIndex = 0; commIndex < m_IconComms.Count(); ++commIndex )
	{
		const FoFIconComm &comm = m_IconComms[commIndex];
		if ( comm.expiresAt < now || !localPlayer || m_iIconCommTexture < 0 )
			continue;
		C_BaseEntity *speaker = cl_entitylist->GetEnt( comm.playerIndex );
		if ( !speaker || speaker == localPlayer )
			continue;

		const float distance =
			( speaker->GetAbsOrigin() - localPlayer->GetAbsOrigin() ).Length();
		const float distanceRoot = sqrtf( MAX( distance, 0.0f ) );
		const int verticalOffset = MAX(
			50, (int)( 95.0f - distanceRoot * 0.5f ) );
		const int iconSize = FoFHudScale( (float)MAX(
			7, (int)( 70.0f - distanceRoot * 2.0f ) ) );

		Vector iconPosition = speaker->GetAbsOrigin();
		iconPosition.z += verticalOffset;
		int screenX = 0;
		int screenY = 0;
		if ( !GetVectorInScreenSpace( iconPosition, screenX, screenY ) )
			continue;

		DrawMenuTexture(
			m_iIconCommTexture,
			screenX - iconSize / 2,
			screenY,
			iconSize,
			iconSize );
	}

	for ( int markerIndex = 0; markerIndex < m_BBMarkers.Count(); ++markerIndex )
	{
		const FoFBBMarker &marker = m_BBMarkers[markerIndex];
		if ( marker.expiresAt < now )
			continue;
		int screenX = 0;
		int screenY = 0;
		if ( !GetVectorInScreenSpace(
				marker.position, screenX, screenY ) )
		{
			continue;
		}

		const int iconSize = MAX( FoFHudScale( 20.0f ), 1 );
		const int iconX = screenX - iconSize / 2;
		const int iconY = screenY - iconSize / 2;
		const Color color = FoFBBMarkerColor( marker.iconType, now );
		CHudTexture *icon = FoFBBMarkerIcon(
			m_pBBMarkerIcons, marker.iconType );
		if ( icon )
			icon->DrawSelf( iconX, iconY, iconSize, iconSize, color );

		C_BasePlayer *markerPlayer = C_BasePlayer::GetLocalPlayer();
		const float distance = markerPlayer ?
			( marker.position - markerPlayer->GetAbsOrigin() ).Length() : 0.0f;
		const int distanceMeters = (int)( distance * 0.025400052f );
		wchar_t markerText[256];
		FoFBBMarkerText(
			marker, distanceMeters, markerText, sizeof( markerText ) );
		if ( markerText[0] && m_hBBMarkerFont != vgui::INVALID_FONT )
		{
			vgui::surface()->DrawSetTextFont( m_hBBMarkerFont );
			vgui::surface()->DrawSetTextColor( color );
			vgui::surface()->DrawSetTextPos(
				iconX, iconY + iconSize );
			vgui::surface()->DrawPrintText(
				markerText, Q_wcslen( markerText ) );
		}
	}

	PaintBBNotices();

	for ( int hitIndex = 0; hitIndex < m_HitMarkers.Count(); ++hitIndex )
	{
		FoFHitMarker &hit = m_HitMarkers[hitIndex];
		if ( hit.expiresAt < now )
			continue;
		int screenX = 0, screenY = 0;
		const bool projected = GetVectorInScreenSpace( hit.position, screenX, screenY );
		const float lifetime = MAX( hit.expiresAt - hit.receivedAt, 0.01f );
		const int alpha = clamp( (int)( 255.0f * ( hit.expiresAt - now ) / lifetime ), 0, 255 );
		if ( projected && m_hHitReconFont != vgui::INVALID_FONT )
		{
			char hitText[64];
			if ( hit.rankOnly )
				Q_snprintf( hitText, sizeof( hitText ), "%.1f", hit.rank );
			else
				Q_snprintf( hitText, sizeof( hitText ), "%d", hit.damage );

			const float damageRatio = clamp( hit.damage * 0.01f, 0.0f, 1.0f );
			const int damageShade = (int)( 255.0f - damageRatio * 255.0f );
			const Color hitColor = hit.rankOnly
				? Color( 255, 225, 0, alpha )
				: Color( 255, damageShade, damageShade, alpha );
			wchar_t hitWide[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(
				hitText, hitWide, sizeof( hitWide ) );
			vgui::surface()->DrawSetTextFont( m_hHitReconFont );
			vgui::surface()->DrawSetTextColor( hitColor );
			vgui::surface()->DrawSetTextPos( screenX, screenY );
			vgui::surface()->DrawPrintText(
				hitWide, Q_wcslen( hitWide ) );
		}

		// The original advances the world-space Z coordinate after each paint,
		// producing a slow five-unit-per-second rise before the next projection.
		hit.position.z += gpGlobals->frametime * 5.0f;
	}

	if ( drawSourceTVPlayerInfo )
		PaintSourceTVPlayerInfo();

}

// FoF transient HUD network-fed notifications.

static ConVar fof_show_hitinfo(
	"fof_show_hitinfo",
	"6",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Displays damage done each attack during defined seconds." );

void CHudFoF::ReceiveHint( const char *text, int mode )
{
	if ( mode == 99 )
	{
		m_bHintVisible = false;
		m_Hint.Clear();
		m_iHintMode = 99;
		m_flHintStartedAt = 0.0f;
		m_flHintPromptAt = 0.0f;
		return;
	}

	m_Hint = text ? text : "";
	m_bHintVisible = true;
	m_iHintMode = mode;
	m_flHintStartedAt = gpGlobals->curtime;
	if ( mode >= 0 && mode <= 6 )
	{
		char command[32];
		Q_snprintf( command, sizeof( command ), "hintkey %d", mode );
		engine->ClientCmd( command );
	}

	float promptDelay = 3.0f;
	if ( m_hHintFont != vgui::INVALID_FONT )
	{
		wchar_t localizedBuffer[512];
		const wchar_t *localized =
			Localize( m_Hint.String(), localizedBuffer, sizeof( localizedBuffer ) );
		int textWide = 0;
		int textTall = 0;
		vgui::surface()->GetTextSize(
			m_hHintFont, localized, textWide, textTall );
		promptDelay = RemapValClamped(
			(float)textWide,
			200.0f,
			(float)MAX( ScreenWidth(), 201 ),
			2.0f,
			3.5f );
	}
	m_flHintPromptAt = gpGlobals->curtime + promptDelay;
}

void CHudFoF::ReceiveIconComm( int playerIndex )
{
	if ( playerIndex <= 0 || playerIndex > MAX_PLAYERS )
		return;

	// The original keeps one three-second entry per IconComm message.  Voice
	// rate limiting normally prevents overlap, but preserving the entries also
	// keeps its expiry/removal behavior exact.
	FoFIconComm comm;
	comm.playerIndex = playerIndex;
	comm.expiresAt = gpGlobals->curtime + 3.0f;
	m_IconComms.AddToTail( comm );
}

void CHudFoF::ReceiveCircleProgress( float endTime )
{
	m_flCircleProgressStart = gpGlobals->curtime;
	m_flCircleProgressEnd = endTime;
	UpdateCircleProgressBar();
}

void CHudFoF::ReceiveCapMessage( int count, int mode, int progress )
{
	m_iCapCount = count;
	m_iCapMode = mode;
	m_iCapProgress = progress;
	m_flCapUntil = gpGlobals->curtime + 1.1f;
}

void CHudFoF::ReceiveBBMulti( int operation, int lifetime, int iconType, const Vector &position, const char *text )
{
	if ( operation != 1 )
	{
		for ( int i = m_BBMarkers.Count() - 1; i >= 0; --i )
		{
			if ( m_BBMarkers[i].iconType == iconType &&
				m_BBMarkers[i].position.x == position.x &&
				m_BBMarkers[i].position.y == position.y &&
				m_BBMarkers[i].position.z == position.z )
				m_BBMarkers.FastRemove( i );
		}
		return;
	}

	FoFBBMarker marker;
	marker.operation = operation;
	marker.iconType = iconType;
	marker.position = position;
	marker.text = text ? text : "";
	marker.expiresAt = gpGlobals->curtime + MAX( lifetime, 0 );
	m_BBMarkers.AddToTail( marker );
}

void CHudFoF::ReceiveBBNotice( int kind, const char *format, const char *argument1, const char *argument2, const char *argument3 )
{
	if ( format && !Q_stricmp( format, "#BB_Notice_LootDropArea" ) )
	{
		for ( int i = m_BBNotices.Count() - 1; i >= 0; --i )
		{
			if ( !Q_stricmp( m_BBNotices[i].format.String(), "#BB_Notice_LootPickArea" ) )
				m_BBNotices.FastRemove( i );
		}
	}

	FoFBBNotice notice;
	notice.kind = kind;
	notice.format = format ? format : "";
	notice.arguments[0] = argument1 ? argument1 : "";
	notice.arguments[1] = argument2 ? argument2 : "";
	notice.arguments[2] = argument3 ? argument3 : "";
	notice.receivedAt = gpGlobals->curtime;
	notice.expiresAt = gpGlobals->curtime + 10.0f;
	m_BBNotices.AddToTail( notice );
}

void CHudFoF::ReceiveGoodBadYou( float value0, int value1, int value2, int value3,
	float value4, int value5, int value6, int value7,
	float value8, float value9, float value10, float value11 )
{
	m_GoodBadFloats[0] = value0;
	m_GoodBadFloats[1] = value4;
	m_GoodBadFloats[2] = value8;
	m_GoodBadFloats[3] = value9;
	m_GoodBadFloats[4] = value10;
	m_GoodBadFloats[5] = value11;
	m_GoodBadInts[0] = value1;
	m_GoodBadInts[1] = value2;
	m_GoodBadInts[2] = value3;
	m_GoodBadInts[3] = value5;
	m_GoodBadInts[4] = value6;
	m_GoodBadInts[5] = value7;
	BuildGoodBadRanks();
	m_bGoodBadVisible = true;
	if ( enginesound )
	{
		enginesound->EmitAmbientSound(
			"#/common/victory.mp3",
			0.7f,
			PITCH_NORM,
			0 );
	}
}

void CHudFoF::ReceiveHitRecon( int damage, const Vector &position, int weaponIndex )
{
	const int lifetime = MAX( fof_show_hitinfo.GetInt(), 0 );
	if ( weaponIndex >= 0 && weaponIndex < ARRAYSIZE( m_WeaponDamage ) )
		m_WeaponDamage[weaponIndex] += MAX( damage, 1 );

	damage = MAX( damage, 1 );
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	const float distance = localPlayer ? ( localPlayer->GetAbsOrigin() - position ).Length() : 0.0f;
	const float height = RemapValClamped( distance, 0.0f, 2500.0f, 80.0f, 120.0f );
	const Vector markerPosition = position + Vector( 0, 0, height );

	// CHudHitRecon only considers the most recently appended marker.  It
	// compares the already height-adjusted positions and leaves the original
	// timestamp untouched when consecutive pellets are accumulated.
	if ( m_HitMarkers.Count() > 0 )
	{
		FoFHitMarker &existing = m_HitMarkers.Tail();
		if ( gpGlobals->curtime - existing.receivedAt < 0.1f &&
			( existing.position - markerPosition ).LengthSqr() <= Square( 30.0f ) )
		{
			existing.damage += damage;
			return;
		}
	}

	FoFHitMarker marker;
	marker.damage = damage;
	marker.weaponIndex = weaponIndex;
	marker.rank = 0.0f;
	marker.rankOnly = false;
	marker.sourcePosition = position;
	marker.position = markerPosition;
	marker.receivedAt = gpGlobals->curtime;
	marker.expiresAt = gpGlobals->curtime + lifetime;
	m_HitMarkers.AddToTail( marker );
}

void CHudFoF::ReceiveHitReconRank( float rank, const Vector &position )
{
	const int lifetime = MAX( fof_show_hitinfo.GetInt(), 0 );

	FoFHitMarker marker;
	marker.damage = 0;
	marker.weaponIndex = -1;
	marker.rank = rank;
	marker.rankOnly = true;
	marker.sourcePosition = position;
	marker.position = position + Vector( 0, 0, 20 );
	marker.receivedAt = gpGlobals->curtime;
	marker.expiresAt = gpGlobals->curtime + lifetime;
	m_HitMarkers.AddToTail( marker );
}

void CHudFoF::ReceiveHitBow( int result )
{
	if ( result == 1 )
		++m_iBowHits;
	else
		++m_iBowAttempts;
}

void CHudFoF::ReceiveStatUpdate(
	const char *label, int newValue, int oldValue )
{
	if ( !label || !label[0] )
		return;

	m_StatLabel = label;
	m_iStatNewValue = newValue;
	m_iStatOldValue = oldValue;
	m_bStatVisible = true;
	// CHudFoFHint::NewStats delays the continue prompt; the
	// record notice itself remains visible until the HUD is reset/replaced.
	m_flStatPromptAt = gpGlobals->curtime + 1.5f;
}

void CHudFoF::EnsureCircleProgressBar()
{
	if ( m_pCircleProgressBar )
		return;

	m_pCircleProgressBar = new vgui::CircularProgressBar( this, "bar" );
	m_pCircleProgressBar->SetProportional( true );
	m_pCircleProgressBar->SetFgImage( "shotgun_hud_on" );
	m_pCircleProgressBar->SetBgImage( "shotgun_hud_off" );
	// SetImage only queues the VGUI material until scheme settings are
	// applied. This child is created hidden during VidInit, so realize it
	// immediately instead of depending on a later visible paint traversal.
	m_pCircleProgressBar->MakeReadyForUse();
	m_pCircleProgressBar->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pCircleProgressBar->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pCircleProgressBar->SetVisible( false );
	LayoutCircleProgressBar();
}

void CHudFoF::LayoutCircleProgressBar()
{
	if ( !m_pCircleProgressBar )
		return;

	// The shipped panel is a 40x40 HUD-space square centred on the
	// crosshair. FoF scales those coordinates from a 480-pixel-tall HUD.
	const float scale = (float)ScreenHeight() / 480.0f;
	const int size = MAX( RoundFloatToInt( 40.0f * scale ), 1 );
	const int x = RoundFloatToInt( ScreenWidth() * 0.5f - 20.0f * scale );
	const int y = RoundFloatToInt( ScreenHeight() * 0.5f - 20.0f * scale );
	m_pCircleProgressBar->SetBounds( x, y, size, size );
}

void CHudFoF::UpdateCircleProgressBar()
{
	EnsureCircleProgressBar();
	if ( !m_pCircleProgressBar || !gpGlobals )
		return;

	const float now = gpGlobals->curtime;
	const bool active = m_flCircleProgressEnd > now;
	if ( m_pCircleProgressBar->IsVisible() != active )
		m_pCircleProgressBar->SetVisible( active );
	if ( !active )
		return;

	const float interval = m_flCircleProgressEnd - m_flCircleProgressStart;
	float progress = interval > 0.0f
		? ( m_flCircleProgressEnd - now ) / interval
		: 0.0f;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( player && g_PR )
	{
		const float latencyAllowance = clamp(
			g_PR->GetPing( player->entindex() ) * 0.001f, 0.1f, 0.5f );
		progress -= latencyAllowance;
	}

	m_pCircleProgressBar->SetProgress( progress );
}

// FoF map timer and map-progress text.

static ConVarRef &FoFTimerShowConVar()
{
	static ConVarRef value( "fof_timer_show", true );
	return value;
}

static ConVarRef &FoFTimerMapStartConVar()
{
	static ConVarRef value( "map_start_time", true );
	return value;
}

static ConVarRef &FoFTimerMaxRoundsConVar()
{
	static ConVarRef value( "fof_sv_maxrounds", true );
	return value;
}

static ConVarRef &FoFTimerRoundsPlayedConVar()
{
	static ConVarRef value( "fof_sv_roundsplayed", true );
	return value;
}

static ConVarRef &FoFTimerCompetitiveConVar()
{
	static ConVarRef value( "fof_sv_dm_comp", true );
	return value;
}

static ConVarRef &FoFTimerCompetitivePointsConVar()
{
	static ConVarRef value( "fof_sv_dm_comp_points", true );
	return value;
}

static void FoFTimerFormatInteger(
	wchar_t *buffer,
	int bufferCount,
	int value,
	bool padToTwoDigits = false )
{
	V_snwprintf( buffer, bufferCount,
		padToTwoDigits ? L"%02d" : L"%d", value );
}

DECLARE_HUDELEMENT( CHudFoFTimer );

CHudFoFTimer::CHudFoFTimer( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HUDFoFTimer" )
	, m_hTextFont( vgui::INVALID_FONT )
	, m_iTextWide( 0 )
	, m_iLastScreenWide( 0 )
	, m_iLastScreenTall( 0 )
{
	SetParent( g_pClientMode->GetViewport() );
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetHiddenBits( 0 );
	m_wszText[0] = L'\0';
}

void CHudFoFTimer::VidInit()
{
	CHudElement::VidInit();
	m_iLastScreenWide = 0;
	m_iLastScreenTall = 0;
	LayoutForScreen();
	UpdateText();
}

bool CHudFoFTimer::ShouldDraw()
{
	if ( FoFHudCurrentMode() == 6 || !FoFHudShouldDraw() )
		return false;

	IViewPortPanel *scoreboard = gViewPortInterface
		? gViewPortInterface->FindPanelByName( PANEL_SCOREBOARD )
		: NULL;
	const bool forceVisible = FoFTimerShowConVar().IsValid() &&
		FoFTimerShowConVar().GetBool();
	if ( !forceVisible && ( !scoreboard || !scoreboard->IsVisible() ) )
		return false;

	const bool hasTimer = HL2MPRules() &&
		HL2MPRules()->GetFoFScoreboardTimeRemaining() >= 0.0f;
	const bool hasRounds = FoFTimerMaxRoundsConVar().IsValid() &&
		FoFTimerMaxRoundsConVar().GetInt() > 0;
	if ( !hasTimer && !hasRounds )
		return false;

	return m_wszText[0] && CHudElement::ShouldDraw();
}

void CHudFoFTimer::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_hTextFont = scheme->GetFont( "Default", true );
	if ( m_hTextFont == vgui::INVALID_FONT )
		m_hTextFont = scheme->GetFont( "MenuFontSmall", true );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	LayoutForScreen();
	UpdateText();
}

void CHudFoFTimer::LayoutForScreen()
{
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void CHudFoFTimer::UpdateText()
{
	m_wszText[0] = L'\0';
	m_iTextWide = 0;

	if ( !g_pVGuiLocalize )
		return;

	const int currentMode = FoFHudCurrentMode();
	wchar_t value1[32];
	wchar_t value2[32];
	const wchar_t *format = NULL;

	if ( FoFTimerCompetitiveConVar().IsValid() &&
		FoFTimerCompetitiveConVar().GetBool() )
	{
		const int points = FoFTimerCompetitivePointsConVar().IsValid()
			? FoFTimerCompetitivePointsConVar().GetInt() : 0;
		FoFTimerFormatInteger( value1, ARRAYSIZE( value1 ), points );
		format = g_pVGuiLocalize->Find( "#ScoreBoard_Comp" );
		if ( format )
		{
			g_pVGuiLocalize->ConstructString(
				m_wszText, sizeof( m_wszText ), format, 1, value1 );
		}
	}
	else
	{
		const int mapStartTime = FoFTimerMapStartConVar().IsValid()
			? FoFTimerMapStartConVar().GetInt() : -1;
		if ( mapStartTime != -1 )
		{
			CHL2MPRules *rules = HL2MPRules();
			const float remaining = rules
				? rules->GetFoFScoreboardTimeRemaining() : -1.0f;
			if ( remaining >= 0.0f )
			{
				const int totalSeconds = MAX( 0, (int)remaining );
				const int minutes = clamp( totalSeconds / 60, 0, 100000 );
				const int seconds = clamp( totalSeconds % 60, 0, 60 );
				FoFTimerFormatInteger(
					value1, ARRAYSIZE( value1 ), minutes );
				FoFTimerFormatInteger(
					value2, ARRAYSIZE( value2 ), seconds,
					seconds <= 9 );
				format = g_pVGuiLocalize->Find( "#ScoreBoard_Timer" );
				if ( format )
				{
					g_pVGuiLocalize->ConstructString(
						m_wszText, sizeof( m_wszText ), format, 2,
						value1, value2 );
				}
			}
		}
		else if ( ( currentMode == 2 || currentMode == 4 ) &&
			FoFTimerMaxRoundsConVar().IsValid() &&
			FoFTimerMaxRoundsConVar().GetInt() > 0 )
		{
			const int roundsPlayed = FoFTimerRoundsPlayedConVar().IsValid()
				? FoFTimerRoundsPlayedConVar().GetInt() : 0;
			FoFTimerFormatInteger(
				value1, ARRAYSIZE( value1 ), roundsPlayed );
			FoFTimerFormatInteger(
				value2, ARRAYSIZE( value2 ),
				FoFTimerMaxRoundsConVar().GetInt() );
			format = g_pVGuiLocalize->Find( "#FoF_RoundsToPlay" );
			if ( format )
			{
				g_pVGuiLocalize->ConstructString(
					m_wszText, sizeof( m_wszText ), format, 2,
					value1, value2 );
			}
		}
	}

	if ( m_hTextFont != vgui::INVALID_FONT && m_wszText[0] )
	{
		int textTall = 0;
		vgui::surface()->GetTextSize(
			m_hTextFont, m_wszText, m_iTextWide, textTall );
	}
}

void CHudFoFTimer::OnThink()
{
	BaseClass::OnThink();
	if ( m_iLastScreenWide != ScreenWidth() ||
		m_iLastScreenTall != ScreenHeight() )
	{
		LayoutForScreen();
	}
	UpdateText();
}

void CHudFoFTimer::Paint()
{
	if ( m_hTextFont == vgui::INVALID_FONT || !m_wszText[0] )
		return;

	const int yBase = FoFHudCurrentMode() == 2 ? 460 : 10;
	const int y = (int)( (float)yBase * (float)ScreenHeight() / 480.0f );
	const int x = ( ScreenWidth() - m_iTextWide ) / 2;

	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextColor( 205, 205, 205, 255 );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText(
		m_wszText, Q_wcslen( m_wszText ) );
}

// FoF HUD text drawing helpers.

struct FoFWrappedLine
{
	wchar_t text[512];
	int length;
	int wide;
	int tall;
};

void CHudFoF::DrawWide( const wchar_t *text, int x, int y, const Color &color, bool centered ) const
{
	if ( !text || !text[0] || m_hFont == vgui::INVALID_FONT )
		return;

	int wide = 0, tall = 0;
	vgui::surface()->GetTextSize( m_hFont, text, wide, tall );
	if ( centered )
		x -= wide / 2;
	vgui::surface()->DrawSetTextFont( m_hFont );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

void CHudFoF::DrawWrappedWide(
	const wchar_t *text,
	vgui::HFont font,
	int x,
	int y,
	int maxWide,
	int maxTall,
	const Color &color,
	int align ) const
{
	if ( !text || !text[0] || font == vgui::INVALID_FONT ||
		maxWide <= 0 || maxTall <= 0 )
	{
		return;
	}

	const wchar_t *cursor = text;
	CUtlVector< FoFWrappedLine > lines;
	int totalTall = 0;
	while ( *cursor )
	{
		while ( *cursor == L' ' )
			++cursor;
		if ( !*cursor )
			break;

		FoFWrappedLine line;
		Q_memset( &line, 0, sizeof( line ) );
		int length = 0;
		int lastSpace = -1;
		const wchar_t *scan = cursor;
		const wchar_t *next = cursor;
		while ( *scan && *scan != L'\n' &&
			length + 1 < ARRAYSIZE( line.text ) )
		{
			line.text[length++] = *scan;
			line.text[length] = L'\0';
			if ( *scan == L' ' )
				lastSpace = length - 1;

			int candidateWide = 0;
			int candidateTall = 0;
			vgui::surface()->GetTextSize(
				font, line.text, candidateWide, candidateTall );
			if ( candidateWide > maxWide )
			{
				if ( lastSpace > 0 )
				length = lastSpace;
				else
					length = MAX( length - 1, 1 );
				next = cursor + length;
				while ( *next == L' ' )
					++next;
				break;
			}

			++scan;
			next = scan;
		}

		if ( *scan == L'\n' )
			next = scan + 1;
		while ( length > 0 && line.text[length - 1] == L' ' )
			--length;
		line.text[length] = L'\0';

		line.length = length;
		vgui::surface()->GetTextSize(
			font, line.text, line.wide, line.tall );
		line.tall = MAX(
			line.tall,
			vgui::surface()->GetFontTall( font ) );
		totalTall += MAX( line.tall, 1 );
		lines.AddToTail( line );

		if ( next <= cursor )
			++cursor;
		else
			cursor = next;
	}

	int drawY = y + MAX( ( maxTall - totalTall ) / 2, 0 );
	const int bottom = y + maxTall;
	for ( int i = 0; i < lines.Count(); ++i )
	{
		const FoFWrappedLine &line = lines[i];
		if ( drawY + line.tall > bottom )
			break;

		int drawX = x;
		if ( align == 0 )
			drawX += ( maxWide - line.wide ) / 2;
		else if ( align == 1 )
			drawX += maxWide - line.wide;

		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor( color );
		vgui::surface()->DrawSetTextPos( drawX, drawY );
		vgui::surface()->DrawPrintText(
			line.text, line.length );
		drawY += MAX( line.tall, 1 );
	}
}

void CHudFoF::DrawAnsi( const char *text, int x, int y, const Color &color, bool centered ) const
{
	wchar_t buffer[512];
	DrawWide( Localize( text, buffer, sizeof( buffer ) ), x, y, color, centered );
}

// FoF-specific styling for messages rendered by the stock CHudMessage.

static bool FoFHudMessageUsesMenuFont( const char *text )
{
	if ( !text )
		return false;

	return !Q_strnicmp( text, "WARM-UP:", 8 ) ||
		!Q_strnicmp( text, "TIME LEFT", 9 );
}

static vgui::HFont FoFClientSchemeFont( const char *fontName )
{
	vgui::IScheme *scheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	return scheme ?
		scheme->GetFont( fontName, false ) :
		vgui::INVALID_FONT;
}

vgui::HFont FoFHudMessageFont(
	const client_textmessage_t *message,
	const char *vguiFontName )
{
	// FoF's warm-up and course countdown counters are plain network
	// HudMsg messages and therefore carry no VGUI scheme-font name.
	// CHudMessage's shipped constructor initializes its default face from
	// ClientScheme/MenuFont
	// in the original client.  MenuFontSmall belongs to the separate
	// named-message path and made WARM-UP roughly half
	// the original size.
	if ( message && FoFHudMessageUsesMenuFont( message->pMessage ) )
		return FoFClientSchemeFont( "MenuFont" );

	// FoF ignores a named message's requested face and resolves the
	// original MenuFontSmall entry from ClientScheme instead.
	if ( vguiFontName && vguiFontName[0] )
		return FoFClientSchemeFont( "MenuFontSmall" );

	return vgui::INVALID_FONT;
}
