#include "cbase.h"
#include "c_basecombatweapon.h"
#include "c_baseplayer.h"
#include "c_playerresource.h"
#include "fof/fof_hints.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_player_status.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_team_menu.h"
#include "fof/fof_weapon_properties.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_weapon_parse.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "tier0/vprof.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Health HUD.

static void FoFHealthPositionChanged( IConVar *var, const char *oldValue, float oldFloat );

static ConVar fof_health_numericonly(
	"fof_health_numericonly", "0", FCVAR_ARCHIVE,
	"Health meter only uses numbers" );

static ConVar fof_health_posx(
	"fof_health_posx", "0", FCVAR_ARCHIVE,
	"Health meter horizontal position",
	true, 0.0f, true, 100.0f, FoFHealthPositionChanged );

static ConVar fof_health_posy(
	"fof_health_posy", "83", FCVAR_ARCHIVE,
	"Health meter vertical position",
	true, 0.0f, true, 100.0f, FoFHealthPositionChanged );

static int FoFHealthMeterSize()
{
	const float hudScale = (float)ScreenHeight() / 480.0f;
	return MAX(
		RoundFloatToInt(
			RemapValClamped(
				(float)ScreenHeight(), 768.0f, 2048.0f,
				135.0f * hudScale, 52.0f * hudScale ) ),
		1 );
}

DECLARE_HUDELEMENT( CHudFoFHealth );

CHudFoFHealth::CHudFoFHealth( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudFoFHealth" )
	, m_pHealthLabel( NULL )
	, m_pPotionFull( NULL )
	, m_pPotionEmpty( NULL )
	, m_iHealth( -1 )
	, m_iMaxHP( 100 )
	, m_iTeam( 2 )
	, m_iMeterSize( 0 )
	, m_iFilledSize( 0 )
	, m_iPotionFilledSize( 0 )
	, m_iLastScreenWide( -1 )
	, m_iLastScreenTall( -1 )
	, m_bGhostTown( false )
	, m_flHealthFraction( 0.0f )
	, m_flPotionFraction( 0.0f )
{
	Q_memset( m_pFullIcons, 0, sizeof( m_pFullIcons ) );
	Q_memset( m_pEmptyIcons, 0, sizeof( m_pEmptyIcons ) );

	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( 0 );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	m_pHealthLabel = new vgui::Label( this, "HP", "" );
	m_pHealthLabel->SetFgColor( Color( 204, 204, 204, 255 ) );
	m_pHealthLabel->SetContentAlignment( vgui::Label::a_center );
}

void CHudFoFHealth::Init()
{
	Reset();
}

void CHudFoFHealth::Reset()
{
	m_iHealth = -1;
	m_iFilledSize = 0;
	m_iPotionFilledSize = 0;
	m_bGhostTown = false;
	m_flHealthFraction = 0.0f;
	m_flPotionFraction = 0.0f;
}

void CHudFoFHealth::VidInit()
{
	char iconName[64];
	for ( int team = 1; team <= 5; ++team )
	{
		Q_snprintf( iconName, sizeof( iconName ), "hp_team%i_full", team );
		m_pFullIcons[team] = gHUD.GetIcon( iconName );
		Q_snprintf( iconName, sizeof( iconName ), "hp_team%i_empty", team );
		m_pEmptyIcons[team] = gHUD.GetIcon( iconName );
	}
	m_pPotionFull = gHUD.GetIcon( "potion2" );
	m_pPotionEmpty = gHUD.GetIcon( "potion1" );

	m_iMeterSize = FoFHealthMeterSize();
	m_iLastScreenWide = ScreenWidth();
	m_iLastScreenTall = ScreenHeight();
	if ( m_pHealthLabel )
	{
		m_pHealthLabel->SetSize( m_iMeterSize, FoFHudScale( 100.0f ) );
		m_pHealthLabel->SetPos( 0, 0 );
	}
	UpdatePosition();
}

void CHudFoFHealth::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
	if ( m_pHealthLabel )
	{
		m_pHealthLabel->SetFont(
			scheme->GetFont( "MenuFontMed", true ) );
		m_pHealthLabel->SetFgColor( Color( 204, 204, 204, 255 ) );
		m_pHealthLabel->SetContentAlignment( vgui::Label::a_center );
	}
	UpdatePosition();
}

void CHudFoFHealth::UpdatePosition()
{
	const int x = (int)( (float)ScreenWidth() * fof_health_posx.GetFloat() * 0.01f );
	const int y = (int)( (float)ScreenHeight() * fof_health_posy.GetFloat() * 0.01f );
	SetPos( x, y );
}

static void FoFHealthPositionChanged( IConVar *var, const char *oldValue, float oldFloat )
{
	CHudFoFHealth *health = GET_HUDELEMENT( CHudFoFHealth );
	if ( health )
		health->UpdatePosition();
}

C_BasePlayer *CHudFoFHealth::GetDisplayPlayer() const
{
	return FoFHudObservedPlayer( C_BasePlayer::GetLocalPlayer() );
}

int CHudFoFHealth::GetDisplayTeam( C_BasePlayer *player ) const
{
	int team = player ? player->GetTeamNumber() : 2;
	if ( !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
	{
		static ConVar *voice = NULL;
		if ( !voice && cvar )
			voice = cvar->FindVar( "fof_player_voice" );
		team = ( voice ? voice->GetInt() : 0 ) + 2;
	}
	return clamp( team, 2, 5 );
}

void CHudFoFHealth::OnThink()
{
	VPROF_BUDGET( "FoF::Health::OnThink", VPROF_BUDGETGROUP_OTHER_VGUI );

	const int screenWide = ScreenWidth();
	const int screenTall = ScreenHeight();
	if ( screenWide != m_iLastScreenWide ||
		screenTall != m_iLastScreenTall )
	{
		m_iLastScreenWide = screenWide;
		m_iLastScreenTall = screenTall;
		m_iMeterSize = FoFHealthMeterSize();
		UpdatePosition();
		if ( m_pHealthLabel )
		{
			m_pHealthLabel->SetSize(
				m_iMeterSize, FoFHudScale( 100.0f ) );
			m_pHealthLabel->SetPos( 0, 0 );
		}
	}

	C_BasePlayer *player = GetDisplayPlayer();
	if ( !player )
		return;

	m_iTeam = GetDisplayTeam( player );

	const float target = RemapValClamped(
		(float)MAX( player->GetHealth(), 0 ),
		0.0f,
		(float)m_iMaxHP,
		-0.2f,
		1.0f );
	if ( target > m_flHealthFraction )
	{
		m_flHealthFraction += gpGlobals->frametime * 0.3f;
	}
	else if ( fabsf( m_flHealthFraction - target ) > 0.05f )
	{
		m_flHealthFraction -= gpGlobals->frametime * 0.3f;
	}
	m_iFilledSize = (int)( (float)m_iMeterSize * m_flHealthFraction );

	static ConVar *ghostTown = NULL;
	if ( !ghostTown && cvar )
		ghostTown = cvar->FindVar( "fof_sv_ghost_town" );
	m_bGhostTown = ghostTown && ghostTown->GetBool();
	if ( m_bGhostTown )
	{
		const float potionTarget = RemapValClamped(
			(float)FoFPotionLevel( player ),
			0.0f,
			100.0f,
			0.01f,
			1.0f );
		if ( potionTarget > m_flPotionFraction )
		{
			m_flPotionFraction += gpGlobals->frametime * 0.3f;
		}
		else if ( fabsf( m_flPotionFraction - potionTarget ) > 0.05f )
		{
			m_flPotionFraction -= gpGlobals->frametime * 0.3f;
		}
		m_iPotionFilledSize =
			(int)( (float)m_iMeterSize * m_flPotionFraction );
	}

	const int health = MAX( player->GetHealth(), 0 );
	if ( health != m_iHealth )
	{
		m_iHealth = health;
		if ( m_pHealthLabel )
		{
			char healthText[64];
			// Typodermic maps '@' to FoF's circled-plus health glyph.
			Q_snprintf( healthText, sizeof( healthText ), "@%i", m_iHealth );
			m_pHealthLabel->SetText( healthText );
		}
	}
}

bool CHudFoFHealth::ShouldDraw()
{
	if ( FoFHudIsHintVisible() )
		return false;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer )
		return false;

	if ( localPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		if ( !localPlayer->GetObserverTarget() ||
			localPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
			return false;
	}

	C_BasePlayer *player = GetDisplayPlayer();
	if ( !player || !player->IsAlive() )
		return false;

	return CHudElement::ShouldDraw();
}

void CHudFoFHealth::Paint()
{
	VPROF_BUDGET( "FoF::Health::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	const Color color( 204, 204, 204, 255 );

	if ( !fof_health_numericonly.GetBool() )
	{
		CHudTexture *full = m_pFullIcons[m_iTeam];
		CHudTexture *empty = m_pEmptyIcons[m_iTeam];
		const int filledSize =
			clamp( m_iFilledSize, 0, m_iMeterSize );
		const int emptySize = m_iMeterSize - filledSize;

		if ( full && filledSize > 0 )
		{
			full->DrawSelfCropped(
				0, emptySize,
				0, emptySize,
				m_iMeterSize, filledSize,
				m_iMeterSize, filledSize,
				color );
		}
		if ( empty && emptySize > 0 )
		{
			empty->DrawSelfCropped(
				0, 0,
				0, 0, m_iMeterSize, emptySize,
				m_iMeterSize, emptySize,
				color );
		}
	}

	if ( m_bGhostTown )
	{
		const int potionFilledSize =
			clamp( m_iPotionFilledSize, 0, m_iMeterSize );
		const int potionEmptySize =
			m_iMeterSize - potionFilledSize;
		if ( m_pPotionFull && potionFilledSize > 0 )
		{
			m_pPotionFull->DrawSelfCropped(
				m_iMeterSize, potionEmptySize,
				0, potionEmptySize,
				m_iMeterSize, potionFilledSize,
				m_iMeterSize, potionFilledSize,
				color );
		}
		if ( m_pPotionEmpty && potionEmptySize > 0 )
		{
			m_pPotionEmpty->DrawSelfCropped(
				m_iMeterSize, 0,
				0, 0,
				m_iMeterSize, potionEmptySize,
				m_iMeterSize, potionEmptySize,
				color );
		}
	}
}

// Ammo HUD.

DECLARE_HUDELEMENT( CHudFoFAmmo );

CHudFoFAmmo::CHudFoFAmmo( const char *elementName )
	: BaseClass( NULL, "HudFoFAmmo" )
	, CHudElement( elementName )
	, m_pAmmoFull( NULL )
	, m_pAmmoEmpty( NULL )
	, m_iRadius( 0 )
	, m_iPrimaryMode( 0 )
{
	Q_memset( m_iCenterX, 0, sizeof( m_iCenterX ) );
	Q_memset( m_iCenterY, 0, sizeof( m_iCenterY ) );
	Q_memset( m_iCurrent, 0, sizeof( m_iCurrent ) );
	Q_memset( m_iMaximum, 0, sizeof( m_iMaximum ) );
	for ( int hand = 0; hand < ARRAYSIZE( m_flScale ); ++hand )
		m_flScale[hand] = 1.0f;

	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( 0 );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CHudFoFAmmo::Init()
{
	Reset();
}

void CHudFoFAmmo::Reset()
{
	Q_memset( m_iCurrent, 0, sizeof( m_iCurrent ) );
	Q_memset( m_iMaximum, 0, sizeof( m_iMaximum ) );
	m_iPrimaryMode = 0;
}

void CHudFoFAmmo::VidInit()
{
	m_iRadius = FoFHudScale( 7.0f );
	const int horizontalOffset = FoFHudScale( 140.0f );
	const int bottomOffset = FoFHudScale( 50.0f );
	m_iCenterX[1] = ScreenWidth() / 2 + horizontalOffset;
	m_iCenterX[2] = ScreenWidth() / 2 - horizontalOffset;
	m_iCenterY[1] = ScreenHeight() - bottomOffset;
	m_iCenterY[2] = m_iCenterY[1];

	m_pAmmoFull = gHUD.GetIcon( "AmmoFull" );
	m_pAmmoEmpty = gHUD.GetIcon( "AmmoEmpty" );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
}

void CHudFoFAmmo::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetAlpha( 255 );
}

C_BasePlayer *CHudFoFAmmo::GetDisplayPlayer() const
{
	return FoFHudObservedPlayer( C_BasePlayer::GetLocalPlayer() );
}

bool CHudFoFAmmo::IsPrimaryAmmoWeapon( int weaponID ) const
{
	switch ( weaponID )
	{
	case 2:  // handguns
	case 3:  // rifles
	case 4:  // coachgun
	case 5:  // dynamite
	case 8:  // throwable melee
	case 10: // shotgun
		return true;
	default:
		return false;
	}
}

float CHudFoFAmmo::GetWeaponScale(
	C_BaseCombatWeapon *weapon, bool secondary, int &mode ) const
{
	if ( !weapon )
		return 1.0f;

	const CHL2MPSWeaponInfo &weaponInfo =
		static_cast< const CHL2MPSWeaponInfo & >(
			weapon->GetWpnData() );
	int damage = weaponInfo.m_iPlayerDamage;
	const int weaponID = weapon->FoFWeaponID();

	if ( !secondary && damage == -1 )
	{
		mode = 2;
	}
	else if ( damage <= 5 )
	{
		if ( !secondary )
			mode = 0;
		damage = ( weaponID == 4 ) ? 70 : 45;
	}
	else if ( !secondary )
	{
		mode = 1;
	}

	return RemapValClamped( (float)damage, 30.0f, 55.0f, 0.8f, 1.5f );
}

void CHudFoFAmmo::OnThink()
{
	VPROF_BUDGET( "FoF::Ammo::OnThink", VPROF_BUDGETGROUP_OTHER_VGUI );

	Q_memset( m_iCurrent, 0, sizeof( m_iCurrent ) );
	Q_memset( m_iMaximum, 0, sizeof( m_iMaximum ) );
	m_iPrimaryMode = 0;

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	C_BasePlayer *player = GetDisplayPlayer();
	if ( !localPlayer || !player )
		return;

	C_BaseCombatWeapon *primary = player->GetActiveWeapon1();
	if ( primary )
	{
		const int weaponID = primary->FoFWeaponID();
		if ( IsPrimaryAmmoWeapon( weaponID ) )
		{
			m_flScale[1] = GetWeaponScale( primary, false, m_iPrimaryMode );
			if ( weaponID == 5 )
			{
				const int ammoType = primary->GetPrimaryAmmoType();
				if ( ammoType >= 0 )
				{
					m_iCurrent[1] = localPlayer->GetAmmoCount( ammoType );
					m_iMaximum[1] = m_iCurrent[1];
					m_iPrimaryMode = 3;
				}
			}
			else if ( weaponID == 8 )
			{
				const int ammoType = primary->GetSecondaryAmmoType();
				if ( ammoType >= 0 )
				{
					m_iCurrent[1] = localPlayer->GetAmmoCount( ammoType ) + 1;
					m_iMaximum[1] = m_iCurrent[1];
					m_iPrimaryMode = 3;
				}
			}
			else
			{
				m_iCurrent[1] = primary->Clip1();
				m_iMaximum[1] = primary->GetMaxClip1();
			}
		}
	}

	C_BaseCombatWeapon *secondary = player->GetActiveWeapon2();
	if ( secondary )
	{
		int unusedMode = 1;
		m_iCurrent[2] = secondary->Clip1();
		m_iMaximum[2] = secondary->GetMaxClip1();
		m_flScale[2] = GetWeaponScale( secondary, true, unusedMode );
	}
}

bool CHudFoFAmmo::ShouldDraw()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer )
		return false;

	// Only an in-eye observer owns the viewed player's weapon HUD.  Chase and
	// roaming cameras can retain an observer target (SourceTV always does), but
	// that target is camera state rather than the player's current HUD owner.
	// Without this gate, the final followed player's cartridge display remains
	// visible after switching to a free chase/free-look camera.
	if ( localPlayer->GetObserverMode() != OBS_MODE_NONE &&
		( localPlayer->GetObserverMode() != OBS_MODE_IN_EYE ||
		  !localPlayer->GetObserverTarget() ) )
	{
		return false;
	}

	C_BasePlayer *player = GetDisplayPlayer();
	if ( !player )
		return false;
	if ( !player->GetActiveWeapon1() && !player->GetActiveWeapon2() )
		return false;
	if ( FoFPlayerInfo( player ) & 0x40000 )
		return false;
	return CHudElement::ShouldDraw();
}

void CHudFoFAmmo::DrawAmmo( int current, int maximum, int hand, int mode )
{
	if ( maximum <= 0 || hand < 1 || hand > 2 )
		return;

	if ( maximum == 50 )
	{
		// Whiskey uses a vertical bottle gauge.  The shipped x86 client casts
		// every coordinate through _ftol2_sse, so preserve truncation rather
		// than using the rounded FoFHudScale helper.
		const double screenScale = (double)ScreenHeight() / 480.0;
		const double tickScale = screenScale * (double)0.6f;
		const int gaugeHeight = (int)( tickScale * 50.0 );
		const int centerX = m_iCenterX[hand];
		const int centerY = m_iCenterY[hand];

		vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		vgui::surface()->DrawOutlinedRect(
			(int)( (double)centerX - screenScale ),
			(int)( (double)centerY - 11.0 * screenScale ),
			(int)( (double)centerX + 16.0 * screenScale ),
			(int)( (double)( centerY + gaugeHeight ) -
				9.0 * screenScale ) );

		for ( int index = 0; index < current; index += 5 )
		{
			const float yPosition = (float)(
				(double)( centerY + gaugeHeight ) -
				12.0 * screenScale -
				tickScale * (double)index );
			const int y = (int)yPosition;
			vgui::surface()->DrawLine(
				centerX,
				y,
				(int)( (double)centerX + 15.0 * screenScale ),
				y );
		}
		return;
	}

	static const float sixRound[6][2] =
	{
		{  0.0f, -1.0f   },
		{  0.8f, -0.475f },
		{  0.8f,  0.475f },
		{  0.0f,  1.0f   },
		{ -0.8f,  0.475f },
		{ -0.8f, -0.475f },
	};
	static const float fiveRound[5][2] =
	{
		{  0.0f, -1.0f  },
		{  0.8f, -0.32f },
		{  0.5f,  0.7f  },
		{ -0.5f,  0.7f  },
		{ -0.8f, -0.32f },
	};

	const Color fullColor( 255, 255, 255, 255 );
	const Color emptyColor( 95, 25, 25, 255 );
	const float handScale = m_flScale[hand];
	const int radius = (int)( (float)m_iRadius * handScale );

	if ( mode == 1 && ( maximum == 5 || maximum == 6 ) )
	{
		const float positionScale =
			( (float)ScreenHeight() / 480.0f ) * 17.0f * handScale;
		for ( int index = 0; index < maximum; ++index )
		{
			const float *point = maximum == 6 ? sixRound[index] : fiveRound[index];
			const int x = (int)( (float)m_iCenterX[hand] + point[0] * positionScale );
			const int y = (int)( (float)m_iCenterY[hand] + point[1] * positionScale );
			const bool filled = index < current;
			vgui::surface()->DrawSetColor( filled ? fullColor : emptyColor );
			vgui::surface()->DrawOutlinedCircle( x, y, radius, filled ? 16 : 8 );
		}
		return;
	}

	const int direction = hand == 1 ? 1 : -1;
	bool upperRow = true;
	int horizontalStep = 0;
	for ( int index = 0; index < maximum; ++index )
	{
		const float spacing =
			( (float)ScreenHeight() / 480.0f ) * (float)horizontalStep * handScale;
		int x = (int)( (float)m_iCenterX[hand] + spacing * (float)direction );
		int y = m_iCenterY[hand];

		if ( !upperRow )
		{
			if ( maximum > 2 )
			{
				y = (int)( (float)y +
					( (float)ScreenHeight() / 480.0f ) * 10.0f * handScale );
			}
			else
			{
				x = (int)( (float)x +
					( (float)direction * (float)ScreenHeight() / 480.0f ) *
					9.0f * handScale );
			}
		}

		const bool filled = index < current;
		vgui::surface()->DrawSetColor( filled ? fullColor : emptyColor );
		if ( filled && mode == 3 )
		{
			vgui::surface()->DrawOutlinedRect( x, y, x + radius, y + radius );
		}
		else
		{
			vgui::surface()->DrawOutlinedCircle(
				x, y, radius, filled ? 16 : 8 );
		}

		upperRow = !upperRow;
		horizontalStep += 10;
	}
}

void CHudFoFAmmo::Paint()
{
	VPROF_BUDGET( "FoF::Ammo::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	if ( m_iMaximum[1] > 0 )
		DrawAmmo( m_iCurrent[1], m_iMaximum[1], 1, m_iPrimaryMode );
	if ( m_iMaximum[2] > 0 )
		DrawAmmo( m_iCurrent[2], m_iMaximum[2], 2, 1 );
}

// Cash HUD.

DECLARE_HUDELEMENT( CHudCash );

CHudCash::CHudCash( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudCash" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CHudCash::Init()
{
	Reset();
}

void CHudCash::Reset()
{
	m_Changes.RemoveAll();
}

C_BasePlayer *CHudCash::GetDisplayPlayer() const
{
	return FoFHudObservedPlayer( C_BasePlayer::GetLocalPlayer() );
}

bool CHudCash::ShouldDrawBuyHint(
	C_BasePlayer *localPlayer, C_BasePlayer *displayPlayer ) const
{
	if ( !localPlayer || displayPlayer != localPlayer ||
		!localPlayer->IsAlive() || FoFInBuyZone( localPlayer ) <= 0 )
	{
		return false;
	}

	const int currentMode = FoFHudCurrentMode();
	return currentMode == 2 || currentMode == 3;
}

bool CHudCash::ShouldDraw()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	C_BasePlayer *player = GetDisplayPlayer();
	if ( !localPlayer || !player )
		return false;

	if ( localPlayer->GetObserverMode() != OBS_MODE_NONE )
	{
		if ( !localPlayer->GetObserverTarget() ||
			localPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
		{
			return false;
		}
	}

	if ( !player->IsAlive() )
		return false;

	if ( (int)FoFCash( player ) == 0 &&
		!ShouldDrawBuyHint( localPlayer, player ) )
		return false;

	return CHudElement::ShouldDraw();
}

void CHudCash::ReceiveCash( int amount )
{
	FoFCashChange change;
	change.amount = amount;
	const float queueOffset =
		clamp( (float)m_Changes.Count() * 0.25f, 0.0f, 1.0f );
	change.startedAt = ( gpGlobals ? gpGlobals->curtime : 0.0f ) - queueOffset;
	m_Changes.AddToTail( change );
}

void CHudCash::DrawText(
	vgui::HFont font, const wchar_t *text, int x, int y,
	const Color &color ) const
{
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return;

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
}

void CHudCash::Paint()
{
	VPROF_BUDGET( "FoF::Cash::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	C_BasePlayer *player = GetDisplayPlayer();
	if ( !localPlayer || !player )
		return;

	char ansi[64];
	wchar_t wide[64];
	const bool drawCash = (int)FoFCash( player ) != 0;
	if ( drawCash )
	{
		Q_snprintf( ansi, sizeof( ansi ), "$%i", (int)FoFCash( player ) );
		g_pVGuiLocalize->ConvertANSIToUnicode( ansi, wide, sizeof( wide ) );
		DrawText( m_hLargeFont, wide, 0, FoFHudScale( 57.0f ),
			Color( 205, 205, 205, 255 ) );
	}

	const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
	for ( int index = 0; drawCash && index < m_Changes.Count(); ++index )
	{
		const FoFCashChange &change = m_Changes[index];
		const float age = MAX( now - change.startedAt, 0.0f );
		const int alpha = (int)( 255.0f -
			clamp( age * ( 1.0f / 3.0f ), 0.0f, 1.0f ) * 255.0f );
		if ( alpha <= 0 )
		{
			m_Changes.Remove( index );
			--index;
			continue;
		}

		const char *format =
			( FoFHudCurrentMode() == 1 && change.amount <= 15 )
			? " +$%i"
			: "-$%i";
		Q_snprintf( ansi, sizeof( ansi ), format, change.amount );
		g_pVGuiLocalize->ConvertANSIToUnicode( ansi, wide, sizeof( wide ) );

		const int x = FoFHudScale( 40.0f + 25.0f * (float)index );
		const float rise =
			clamp( age * ( 1.0f / 3.5f ), 0.0f, 1.0f ) * 40.0f;
		const int y = FoFHudScale( 65.0f - rise );
		DrawText( m_hLargeFont, wide, x, y,
			Color( 255, 150, 10, alpha ) );
	}

	if ( ShouldDrawBuyHint( localPlayer, player ) )
	{
		const wchar_t *format = g_pVGuiLocalize->Find( "#InBuyArea" );
		wchar_t buyText[512];
		FoFReplaceKeyBindings(
			format ? format : L"", 0, buyText, sizeof( buyText ) );
		DrawText( m_hSmallFont, buyText, 0, FoFHudScale( 45.0f ),
			Color( 105, 205, 105, 255 ) );
	}
}

// Notoriety HUD.

static ConVar fof_notoriety_show(
	"fof_notoriety_show", "1", FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Show notoriety meter, 0 disables it, 1 shows all elements, "
	"2 show only notoriety feed, 3 show only killstreak, 4 show feed + ks" );

static const wchar_t *FoFNotorietyVisibleText( const wchar_t *text )
{
	// FoF's reward localization strings retain Source chat color controls.
	// ISurface does not interpret them and otherwise draws a missing-glyph box.
	while ( text && *text >= L'\x01' && *text <= L'\x08' )
		++text;
	return text;
}

DECLARE_HUDELEMENT( CHudFoFNotoriety );

CHudFoFNotoriety::CHudFoFNotoriety( const char *elementName )
	: CHudElement( elementName )
	, BaseClass( NULL, "HudFoFNotoriety" )
	, m_bDrawEnabled( false )
	, m_bRoundEndAutoCash( false )
	, m_iLocalMultiKill( 0 )
	, m_flRoundEndStartedAt( 0.0f )
	, m_flNextRankBuildTime( 0.0f )
	, m_iTotalNotoriety( 0 )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( 0 );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CHudFoFNotoriety::Init()
{
	ClearState();
	ListenForGameEvent( "game_newmap" );
	ListenForGameEvent( "round_end_autocash" );
	ListenForGameEvent( "round_start" );
}

void CHudFoFNotoriety::ClearState()
{
	m_Ranks.RemoveAll();
	m_Notices.RemoveAll();
	m_bDrawEnabled = false;
	m_bRoundEndAutoCash = false;
	m_iLocalMultiKill = 0;
	m_flRoundEndStartedAt = 0.0f;
	m_flNextRankBuildTime = 0.0f;
	m_iTotalNotoriety = 0;
}

bool CHudFoFNotoriety::ShouldDraw()
{
	m_bDrawEnabled =
		FoFHudConVarInt( "fof_sv_disable_killstreak", 0 ) == 0 &&
		fof_notoriety_show.GetInt() > 0 &&
		FoFHudConVarInt( "fof_sv_bot_edit_active", 0 ) == 0;

	if ( !m_bDrawEnabled )
		return false;

	return CHudElement::ShouldDraw();
}

void CHudFoFNotoriety::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *name = event->GetName();
	if ( !Q_strcmp( name, "game_newmap" ) )
	{
		ClearState();
		return;
	}

	if ( FoFHudCurrentMode() != 2 )
		return;

	if ( !Q_strcmp( name, "round_end_autocash" ) )
	{
		m_bRoundEndAutoCash = true;
		m_flRoundEndStartedAt = gpGlobals ? gpGlobals->curtime : 0.0f;
		m_flNextRankBuildTime = 0.0f;
	}
	else if ( !Q_strcmp( name, "round_start" ) )
	{
		m_bRoundEndAutoCash = false;
		m_flNextRankBuildTime = 0.0f;
	}
}

void CHudFoFNotoriety::BuildRanks( C_BasePlayer *localPlayer )
{
	VPROF_BUDGET( "FoF::Notoriety::BuildRanks", VPROF_BUDGETGROUP_OTHER_VGUI );

	m_Ranks.RemoveAll();
	m_iLocalMultiKill = 0;
	m_iTotalNotoriety = 0;
	if ( !localPlayer || !g_PR || !gpGlobals )
		return;

	const int currentMode = FoFHudCurrentMode();
	const bool lastRoundTeamRanking =
		currentMode == 2 || currentMode == 4;

	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		C_BasePlayer *player = UTIL_PlayerByIndex( index );
		if ( !player || player->GetTeamNumber() == 1 )
			continue;
		if ( lastRoundTeamRanking &&
			player->GetTeamNumber() != localPlayer->GetTeamNumber() )
		{
			continue;
		}

		FoFNotorietyRank rank;
		rank.playerIndex = index;
		rank.notoriety = lastRoundTeamRanking
			? FoFLastRoundNotoriety( player )
			: g_PR->GetFoFExp( index );
		rank.multiKill = FoFMultiKill( player );
		rank.reserved = 0;
		rank.frags = g_PR->GetFrags( index );
		rank.localPlayer = player == localPlayer;
		m_iTotalNotoriety += rank.notoriety;
		if ( rank.localPlayer )
			m_iLocalMultiKill = rank.multiKill;
		m_Ranks.AddToTail( rank );
	}

	// The original comparator orders notoriety descending and uses frags as
	// its deterministic tie break.
	for ( int left = 0; left < m_Ranks.Count(); ++left )
	{
		for ( int right = left + 1; right < m_Ranks.Count(); ++right )
		{
			const bool moveRight =
				m_Ranks[right].notoriety > m_Ranks[left].notoriety ||
				( m_Ranks[right].notoriety == m_Ranks[left].notoriety &&
				  m_Ranks[right].frags > m_Ranks[left].frags );
			if ( moveRight )
			{
				FoFNotorietyRank temporary = m_Ranks[left];
				m_Ranks[left] = m_Ranks[right];
				m_Ranks[right] = temporary;
			}
		}
	}
}

void CHudFoFNotoriety::OnThink()
{
	VPROF_BUDGET( "FoF::Notoriety::OnThink", VPROF_BUDGETGROUP_OTHER_VGUI );

	const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( fof_notoriety_show.GetInt() > 0 &&
		localPlayer && g_PR && now >= m_flNextRankBuildTime )
	{
		BuildRanks( localPlayer );
		m_flNextRankBuildTime = now + 0.10f;
	}

	for ( int index = m_Notices.Count() - 1; index >= 0; --index )
	{
		if ( now > m_Notices[index].receivedAt + 10.0f )
			m_Notices.Remove( index );
	}
}

Color CHudFoFNotoriety::GetRankColor(
	const FoFNotorietyRank &rank ) const
{
	// FoF deliberately keeps the local rank neutral.  Only the adjacent
	// player ranks use their team colour; tinting the enlarged local row as
	// well makes the two/three-line rank stack disagree with the original
	// (most visibly blue-over-red in team modes and yellow-over-yellow in
	// free-for-all).
	if ( rank.localPlayer )
		return Color( 205, 205, 205, 255 );

	if ( !g_PR )
		return Color( 205, 205, 205, 255 );

	const int team = g_PR->GetTeam( rank.playerIndex );
	int mappedTeam = team;
	if ( team >= 2 && team <= 5 )
	{
		char variableName[64];
		Q_snprintf( variableName, sizeof( variableName ),
			"fof_sv_team_remap_%i", team - 1 );
		mappedTeam = FoFHudConVarInt( variableName, team );
	}
	return g_PR->GetTeamColor( mappedTeam );
}

int CHudFoFNotoriety::DrawText(
	vgui::HFont font, const wchar_t *text, int x, int y,
	const Color &color ) const
{
	text = FoFNotorietyVisibleText( text );
	if ( font == vgui::INVALID_FONT || !text || !text[0] )
		return 0;

	int wide = 0;
	int tall = 0;
	vgui::surface()->GetTextSize( font, text, wide, tall );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( text, Q_wcslen( text ) );
	return wide;
}

const wchar_t *CHudFoFNotoriety::RankSuffix(
	int rank, wchar_t *buffer, int bufferBytes ) const
{
	const char *token = "#FoF_xth";
	if ( rank == 1 )
		token = "#FoF_1st";
	else if ( rank == 2 )
		token = "#FoF_2nd";
	else if ( rank == 3 )
		token = "#FoF_3rd";

	const wchar_t *localized = g_pVGuiLocalize->Find( token );
	if ( localized )
		return localized;

	V_wcsncpy( buffer, rank == 1 ? L"st" :
		( rank == 2 ? L"nd" : ( rank == 3 ? L"rd" : L"th" ) ),
		bufferBytes );
	return buffer;
}

void CHudFoFNotoriety::Paint()
{
	VPROF_BUDGET( "FoF::Notoriety::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	const int displayMode = fof_notoriety_show.GetInt();
	int y = FoFHudScale( 18.0f );

	if ( displayMode == 1 && m_Ranks.Count() > 0 )
	{
		int localRankIndex = 0;
		for ( int index = 0; index < m_Ranks.Count(); ++index )
		{
			if ( m_Ranks[index].localPlayer )
			{
				localRankIndex = index;
				break;
			}
		}

		const float roundEndAge =
			( gpGlobals ? gpGlobals->curtime : 0.0f ) -
			m_flRoundEndStartedAt;
		for ( int index = 0; index < m_Ranks.Count(); ++index )
		{
			if ( m_bRoundEndAutoCash )
			{
				if ( (float)index * 0.6f > roundEndAge )
					continue;
			}
			else if ( index < localRankIndex - 1 ||
				index > localRankIndex + 1 )
			{
				continue;
			}

			const FoFNotorietyRank &rank = m_Ranks[index];
			vgui::HFont font;
			if ( m_bRoundEndAutoCash )
				font = rank.localPlayer ? m_hSmallFont : m_hSSmallFont;
			else
				font = rank.localPlayer ? m_hLargeFont : m_hSmallFont;

			const Color teamColor = GetRankColor( rank );
			if ( m_bRoundEndAutoCash )
			{
				wchar_t playerName[128];
				g_pVGuiLocalize->ConvertANSIToUnicode(
					g_PR ? g_PR->GetPlayerName( rank.playerIndex ) : "",
					playerName, sizeof( playerName ) );
				V_wcsncat( playerName, L" ", ARRAYSIZE( playerName ) );
				int x = DrawText( font, playerName, 0, y, teamColor );

				wchar_t score[64];
				V_snwprintf( score, ARRAYSIZE( score ), L"[%i] ",
					rank.notoriety );
				x += DrawText( font, score, x, y,
					Color( 180, 207, 121, 255 ) );

				const CHL2MPRules *rules = HL2MPRules();
				if ( rules )
				{
					const int payout = rules->GetFoFNotorietyPayout(
						m_iTotalNotoriety,
						rank.notoriety,
						m_Ranks.Count() );
					wchar_t cash[64];
					V_snwprintf( cash, ARRAYSIZE( cash ),
						L"+$%i", payout );
					DrawText( font, cash, x, y,
						Color( 180, 207, 121, 255 ) );
				}
			}
			else
			{
				wchar_t suffixBuffer[16];
				const wchar_t *suffix =
					RankSuffix( index + 1, suffixBuffer,
						sizeof( suffixBuffer ) );
				wchar_t line[128];
				V_snwprintf( line, ARRAYSIZE( line ),
					L"%i%ls/ %i x%i", index + 1, suffix,
					rank.notoriety, rank.multiKill );
				DrawText( font, line, 0, y, teamColor );
			}

			y += FoFHudScale( rank.localPlayer ? 30.0f : 17.0f );
			if ( m_bRoundEndAutoCash )
				y -= FoFHudScale( rank.localPlayer ? 9.0f : 4.0f );
		}
		y += FoFHudScale( 10.0f );
	}

	if ( displayMode > 2 )
	{
		wchar_t streak[64];
		V_snwprintf( streak, ARRAYSIZE( streak ), L"KS x%i",
			m_iLocalMultiKill );
		DrawText( m_hLargeFont, streak, 0, FoFHudScale( 35.0f ),
			Color( 205, 205, 205, 255 ) );
		y = FoFHudScale( 80.0f );
	}

	if ( displayMode == 3 || m_Notices.Count() == 0 )
		return;

	const float now = gpGlobals ? gpGlobals->curtime : 0.0f;
	for ( int index = m_Notices.Count() - 1; index >= 0; --index )
	{
		const FoFNotorietyNotice &notice = m_Notices[index];
		const float fade =
			clamp( ( now - notice.receivedAt - 7.0f ) *
				( 1.0f / 3.0f ), 0.0f, 1.0f );
		const int alpha = (int)( 255.0f - fade * 255.0f );

		Color color( 200, 255, 150, alpha );
		if ( notice.reason == 9 )
			color = Color( 100, 255, 100, alpha );
		else if ( notice.reason == 10 )
			color = Color( 200, 150, 100, alpha );
		else if ( notice.reason == 7 ||
			notice.reason == 4 || notice.reason == 14 )
		{
			color = Color( 250, 200, 100, alpha );
		}

		DrawText( m_hVerySmallFont, notice.text, 0, y, color );
		y += FoFHudScale( 10.0f );
	}
}

void CHudFoFNotoriety::ReceiveNotoriety(
	int reason, int value, int eventCode, const char *text )
{
	if ( !m_bDrawEnabled || reason < 0 || reason > 14 )
		return;

	static const char *tokens[15] =
	{
		"#FoFKillScore",
		"#FoFKillScore_Bonus",
		"#FoFAssistScore",
		"#FoFNemesis_Bonus",
		"#FoFGentleman_Malus",
		"#FoFCaptureNotoriety",
		"#FoFDropHatBonus",
		"#FoFPenaltyScore",
		"#FoFLootScore",
		NULL,
		NULL,
		"#FoF_Weapon_Gift",
		NULL,
		NULL,
		"#FoFPenalty_Comp_Score"
	};

	const char *token = tokens[reason];
	if ( !token )
		token = text;
	if ( !token || !token[0] )
		return;

	const wchar_t *format = g_pVGuiLocalize->Find( token );
	if ( !format )
		return;

	char valueAnsi[64];
	char eventAnsi[64];
	wchar_t valueWide[64];
	wchar_t eventWide[64];
	Q_snprintf( valueAnsi, sizeof( valueAnsi ), "%i", value );
	Q_snprintf( eventAnsi, sizeof( eventAnsi ), "%i", eventCode );
	g_pVGuiLocalize->ConvertANSIToUnicode(
		valueAnsi, valueWide, sizeof( valueWide ) );
	g_pVGuiLocalize->ConvertANSIToUnicode(
		eventAnsi, eventWide, sizeof( eventWide ) );

	FoFNotorietyNotice notice;
	Q_memset( &notice, 0, sizeof( notice ) );
	if ( reason == 1 )
	{
		g_pVGuiLocalize->ConstructString(
			notice.text, sizeof( notice.text ), format, 2,
			valueWide, eventWide );
	}
	else
	{
		g_pVGuiLocalize->ConstructString(
			notice.text, sizeof( notice.text ), format, 1,
			valueWide );
	}

	notice.receivedAt = gpGlobals ? gpGlobals->curtime : 0.0f;
	notice.reason = reason;
	m_Notices.AddToTail( notice );
	m_flNextRankBuildTime = 0.0f;
}
