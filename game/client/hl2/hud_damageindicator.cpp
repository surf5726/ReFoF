//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Fistful of Frags directional damage indicator.
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "view.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

enum FoFPainDirection
{
	FOF_PAIN_UP = 0,
	FOF_PAIN_DOWN,
	FOF_PAIN_LEFT,
	FOF_PAIN_RIGHT,
	FOF_PAIN_DIRECTION_COUNT
};

static const char *const g_FoFPainTextureNames[FOF_PAIN_DIRECTION_COUNT] =
{
	"pain_up",
	"pain_down",
	"pain_left",
	"pain_right"
};

// Discard a direction once its remaining dot-product strength reaches 0.4.
// Letting it run to zero keeps the additive red sprite visible too long.
static const float FOF_PAIN_DRAW_THRESHOLD = 0.4f;

class CHudDamageIndicator : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDamageIndicator, vgui::Panel );

public:
	CHudDamageIndicator( const char *pElementName );

	virtual void Init();
	virtual void Reset();
	virtual bool ShouldDraw();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void MsgFunc_Damage( bf_read &msg );

private:
	void UpdateDamageDirection( const Vector &vecSource );
	void DrawPainDirection( FoFPainDirection direction, float flDecay );

	float m_flPainStrength[FOF_PAIN_DIRECTION_COUNT];
	Color m_PainColor;
	CHudTexture *m_pPainTexture[FOF_PAIN_DIRECTION_COUNT];
	float m_flPainEndTime;
};

DECLARE_HUDELEMENT( CHudDamageIndicator );
DECLARE_HUD_MESSAGE( CHudDamageIndicator, Damage );

CHudDamageIndicator::CHudDamageIndicator( const char *pElementName )
	: CHudElement( pElementName )
	, BaseClass( NULL, "HudDamageIndicator" )
	, m_PainColor( 250, 0, 0, 255 )
	, m_flPainEndTime( 0.0f )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( HIDEHUD_HEALTH );
	Q_memset( m_flPainStrength, 0, sizeof( m_flPainStrength ) );
	Q_memset( m_pPainTexture, 0, sizeof( m_pPainTexture ) );
}

void CHudDamageIndicator::Init()
{
	HOOK_HUD_MESSAGE( CHudDamageIndicator, Damage );
	Reset();
}

void CHudDamageIndicator::Reset()
{
	Q_memset( m_flPainStrength, 0, sizeof( m_flPainStrength ) );
	m_flPainEndTime = 0.0f;
	m_PainColor = Color( 250, 0, 0, 255 );
}

bool CHudDamageIndicator::ShouldDraw()
{
	for ( int direction = 0; direction < FOF_PAIN_DIRECTION_COUNT; ++direction )
	{
		if ( m_flPainStrength[direction] > 0.0f )
			return CHudElement::ShouldDraw();
	}

	return false;
}

void CHudDamageIndicator::DrawPainDirection(
	FoFPainDirection direction, float flDecay )
{
	float &strength = m_flPainStrength[direction];
	if ( strength <= FOF_PAIN_DRAW_THRESHOLD )
	{
		strength = 0.0f;
		return;
	}

	CHudTexture *&texture = m_pPainTexture[direction];
	if ( !texture )
		texture = gHUD.GetIcon( g_FoFPainTextureNames[direction] );

	if ( texture )
	{
		const int wide = texture->Width();
		const int tall = texture->Height();
		const int centerX = ScreenWidth() / 2;
		const int centerY = ScreenHeight() / 2;
		int x = centerX;
		int y = centerY;

		switch ( direction )
		{
		case FOF_PAIN_UP:
			x = centerX - wide / 2;
			y = RoundFloatToInt( centerY - tall * 2.25f );
			break;
		case FOF_PAIN_DOWN:
			x = centerX - wide / 2;
			y = RoundFloatToInt( centerY + tall * 1.5f );
			break;
		case FOF_PAIN_LEFT:
			x = centerX - wide * 3;
			y = centerY - tall / 2;
			break;
		case FOF_PAIN_RIGHT:
			x = centerX + wide * 2;
			y = centerY - tall / 2;
			break;
		default:
			break;
		}

		texture->DrawSelf( x, y, wide, tall, m_PainColor );
	}

	strength = MAX( 0.0f, strength - flDecay );
}

void CHudDamageIndicator::Paint()
{
	if ( !gpGlobals || m_flPainEndTime <= gpGlobals->curtime )
		return;

	const float decay = gpGlobals->frametime * 2.0f;
	DrawPainDirection( FOF_PAIN_UP, decay );
	DrawPainDirection( FOF_PAIN_DOWN, decay );
	DrawPainDirection( FOF_PAIN_LEFT, decay );
	DrawPainDirection( FOF_PAIN_RIGHT, decay );
}

void CHudDamageIndicator::MsgFunc_Damage( bf_read &msg )
{
	msg.ReadByte(); // armor damage is not used by FoF's indicator
	const int damageTaken = msg.ReadByte();
	msg.ReadLong(); // damage flags are not used by FoF's indicator

	Vector source;
	source.x = msg.ReadFloat();
	source.y = msg.ReadFloat();
	source.z = msg.ReadFloat();

	if ( damageTaken <= 0 )
		return;

	if ( gpGlobals )
		m_flPainEndTime = gpGlobals->curtime + 1.25f;
	UpdateDamageDirection( source );
}

void CHudDamageIndicator::UpdateDamageDirection( const Vector &source )
{
	if ( source == vec3_origin )
	{
		Q_memset( m_flPainStrength, 0, sizeof( m_flPainStrength ) );
		return;
	}

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	Vector delta = source - player->GetAbsOrigin();
	const float distance = VectorNormalize( delta );
	if ( distance <= 50.0f )
	{
		for ( int direction = 0;
			direction < FOF_PAIN_DIRECTION_COUNT;
			++direction )
		{
			m_flPainStrength[direction] = 1.0f;
		}
		return;
	}

	Vector forward;
	Vector right;
	AngleVectors( MainViewAngles(), &forward, &right, NULL );
	const float front = DotProduct( delta, forward );
	const float side = DotProduct( delta, right );

	if ( front > 0.3f )
		m_flPainStrength[FOF_PAIN_UP] =
			MAX( m_flPainStrength[FOF_PAIN_UP], front );
	else if ( front < -0.3f )
		m_flPainStrength[FOF_PAIN_DOWN] =
			MAX( m_flPainStrength[FOF_PAIN_DOWN], -front );

	if ( side > 0.3f )
		m_flPainStrength[FOF_PAIN_RIGHT] =
			MAX( m_flPainStrength[FOF_PAIN_RIGHT], side );
	else if ( side < -0.3f )
		m_flPainStrength[FOF_PAIN_LEFT] =
			MAX( m_flPainStrength[FOF_PAIN_LEFT], -side );
}

void CHudDamageIndicator::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );

	int x = 0;
	int y = 0;
	int wide = ScreenWidth();
	int tall = ScreenHeight();
	vgui::surface()->GetFullscreenViewport( x, y, wide, tall );
	NOTE_UNUSED( x );
	NOTE_UNUSED( y );
	SetForceStereoRenderToFrameBuffer( true );
	SetSize( wide, tall );
}
