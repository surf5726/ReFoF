//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fistful of Frags crosshair presentation.
//
//=============================================================================//

#include "cbase.h"
#include "fof/fof_crosshair.h"
#include "fof/fof_hints.h"

#include "cdll_util.h"
#include "hud.h"
#include "hl2mp_gamerules.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_properties.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_crosshair_r( "cl_crosshair_r", "0", FCVAR_ARCHIVE,
	"Crosshair redness. 0-255" );
ConVar cl_crosshair_g( "cl_crosshair_g", "250", FCVAR_ARCHIVE,
	"Crosshair greenness. 0-255" );
ConVar cl_crosshair_b( "cl_crosshair_b", "0", FCVAR_ARCHIVE,
	"Crosshair blueness. 0-255" );
ConVar cl_crosshair_a( "cl_crosshair_a", "255", FCVAR_ARCHIVE,
	"Crosshair opacity(alpha). 0-255" );
ConVar fof_ch_drawstats( "fof_ch_drawstats", "1", FCVAR_ARCHIVE,
	"Crosshair accuracy stats" );
ConVar fof_ch_switch_crosshair( "fof_ch_switch_crosshair", "0", FCVAR_ARCHIVE,
	"Switches primary and secondary crosshair styles" );
ConVar fof_ch_scalesize( "fof_ch_scalesize", "10", FCVAR_ARCHIVE,
	"Sets the crosshair scale", true, 1.0f, true, 20.0f );
ConVar fof_ch_thickness( "fof_ch_thickness", "1", FCVAR_ARCHIVE,
	"Sets the crosshair thickness", true, 0.0f, true, 10.0f );
ConVar fof_ch_drawdot( "fof_ch_drawdot", "1", FCVAR_ARCHIVE,
	"Draws crosshair's central dot", true, 0.0f, true, 1.0f );
ConVar fof_crosshair_ff( "fof_crosshair_ff", "1", FCVAR_ARCHIVE,
	"Shows the friendly fire warning crosshair", true, 0.0f, true, 1.0f );

struct FoFCrosshairState
{
	FoFCrosshairState()
		: m_hTextFont( vgui::INVALID_FONT )
		, m_pFriendlyCrosshair( NULL )
		, m_pUseCrosshair( NULL )
		, m_pWeaponThrow( NULL )
		, m_pWeaponThrow2( NULL )
		, m_flFriendlyUntil( 0.0f )
		, m_flUseUntil( 0.0f )
	{
	}

	vgui::HFont m_hTextFont;
	CHudTexture *m_pFriendlyCrosshair;
	CHudTexture *m_pUseCrosshair;
	CHudTexture *m_pWeaponThrow;
	CHudTexture *m_pWeaponThrow2;
	float m_flFriendlyUntil;
	float m_flUseUntil;
};

// FoF extends the stock entity-effects field with a server-authored bit
// that marks entities which should show the USE crosshair.  FoF's
// original client tests m_fEffects bit 11 here; this is deliberately not
// an EFL_* transform-cache flag.
static const int FOF_EF_USE_PROMPT = ( 1 << 11 );

static float FoFMarkerExpirationTime()
{
	return static_cast< float >(
		static_cast< double >( gpGlobals->curtime ) + 0.2 );
}

static void DrawFoFPlusCrosshair( float flArmLength, float flGap, float flX,
	float flY, const Color &color )

{
	const float flOuterArm = flGap + flArmLength * 0.5f;
	const float flThickness = MAX( 1.0f, fof_ch_thickness.GetFloat() );

	surface()->DrawSetColor( color );
	surface()->DrawFilledRect(
		static_cast< int >( flX + flGap ),
		static_cast< int >( flY - flThickness ),
		static_cast< int >( flX + flOuterArm ),
		static_cast< int >( flY + flThickness ) );
	surface()->DrawFilledRect(
		static_cast< int >( flX - flOuterArm ),
		static_cast< int >( flY - flThickness ),
		static_cast< int >( flX - flGap ),
		static_cast< int >( flY + flThickness ) );
	surface()->DrawFilledRect(
		static_cast< int >( flX - flThickness ),
		static_cast< int >( flY + flGap ),
		static_cast< int >( flX + flThickness ),
		static_cast< int >( flY + flOuterArm ) );
	surface()->DrawFilledRect(
		static_cast< int >( flX - flThickness ),
		static_cast< int >( flY - flOuterArm ),
		static_cast< int >( flX + flThickness ),
		static_cast< int >( flY - flGap ) );
}

static void DrawFoFXCrosshair( float flArmLength, float flGap, float flX,
	float flY, const Color &color )

{
	const float flDiagonal = 0.7f;
	const float flOuterGap =
		flGap + MAX( 1.0f, fof_ch_thickness.GetFloat() );
	int x[4];
	int y[4];
	(void)flArmLength;

	surface()->DrawSetColor( color );

#define FOF_CROSSHAIR_POINT( index, pointX, pointY ) \
	x[index] = static_cast< int >( flX + ( pointX ) ); \
	y[index] = static_cast< int >( flY + ( pointY ) )

	FOF_CROSSHAIR_POINT( 0, -flGap, flGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 1, -flGap * flDiagonal, flGap );
	FOF_CROSSHAIR_POINT( 2, -flOuterGap * flDiagonal, flOuterGap );
	FOF_CROSSHAIR_POINT( 3, -flOuterGap, flOuterGap * flDiagonal );
	surface()->DrawPolyLine( x, y, ARRAYSIZE( x ) );

	FOF_CROSSHAIR_POINT( 0, flGap * flDiagonal, flGap );
	FOF_CROSSHAIR_POINT( 1, flGap, flGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 2, flOuterGap, flOuterGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 3, flOuterGap * flDiagonal, flOuterGap );
	surface()->DrawPolyLine( x, y, ARRAYSIZE( x ) );

	FOF_CROSSHAIR_POINT( 0, flGap, -flGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 1, flGap * flDiagonal, -flGap );
	FOF_CROSSHAIR_POINT( 2, flOuterGap * flDiagonal, -flOuterGap );
	FOF_CROSSHAIR_POINT( 3, flOuterGap, -flOuterGap * flDiagonal );
	surface()->DrawPolyLine( x, y, ARRAYSIZE( x ) );

	FOF_CROSSHAIR_POINT( 0, -flGap * flDiagonal, -flGap );
	FOF_CROSSHAIR_POINT( 1, -flGap, -flGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 2, -flOuterGap, -flOuterGap * flDiagonal );
	FOF_CROSSHAIR_POINT( 3, -flOuterGap * flDiagonal, -flOuterGap );
	surface()->DrawPolyLine( x, y, ARRAYSIZE( x ) );

#undef FOF_CROSSHAIR_POINT
}

static void DrawFoFHandCrosshair( bool bPlusStyle, float flArmLength,
	float flGap, float flX, float flY, const Color &color )

{
	if ( bPlusStyle )
		DrawFoFPlusCrosshair( flArmLength, flGap, flX, flY, color );
	else
		DrawFoFXCrosshair( flArmLength, flGap, flX, flY, color );
}

static void DrawFoFTextureCentered( CHudTexture *pTexture, float flX,
	float flY, const Color &color )

{
	if ( !pTexture )
		return;

	pTexture->DrawSelf(
		static_cast< int >( flX - pTexture->Width() * 0.5f ),
		static_cast< int >( flY - pTexture->Height() * 0.5f ),
		color );
}

static void DrawFoFThrowIndicator( CHudTexture *pFilled,
	CHudTexture *pUnfilled, float flFraction, float flX, float flY,
	const Color &color )

{
	if ( !pFilled || !pUnfilled )
		return;

	const int nWide = pUnfilled->Width();
	const int nTall = pUnfilled->Height();
	const int nSplit = static_cast< int >(
		nTall * clamp( flFraction, 0.0f, 1.0f ) );
	const int nX = static_cast< int >( flX - pFilled->Width() * 0.5f );
	const int nY = static_cast< int >( flY - pFilled->Height() * 0.5f );

	pFilled->DrawSelfCropped(
		nX, nY, 0, 0, nWide, nSplit, nWide, nSplit, color );
	pUnfilled->DrawSelfCropped(
		nX, nY + nSplit, 0, nSplit, nWide, nTall - nSplit,
		nWide, nTall - nSplit, color );
}

static int FoFAccuracyPercent( float flAperture )
{
	const float flAccurateEdge = RemapValClamped(
		flAperture, 0.4f, 0.01f, 0.25f, 0.07f );
	return static_cast< int >( RemapValClamped(
		flAperture, flAccurateEdge, 0.01f, 1.0f, 99.0f ) );
}

static void DrawFoFAccuracyStats( C_BasePlayer *pPlayer,
	vgui::HFont hFont, float flX, float flY, const Color &color )

{
	C_BaseCombatWeapon *pWeapon =
		pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( !fof_ch_drawstats.GetBool() || hFont == vgui::INVALID_FONT ||
		!pPlayer || !pWeapon )
	{
		return;
	}

	const int nWeaponID = pWeapon ? pWeapon->FoFWeaponID() : -1;
	if ( nWeaponID == 0 || nWeaponID == 1 ||
		nWeaponID == 8 || nWeaponID == 5 )
	{
		return;
	}

	const float flPrimary = FoFCrosshairAperture( pPlayer, 0 );
	const float flSecondary = FoFCrosshairAperture( pPlayer, 1 );
	if ( flPrimary <= 0.0f && flSecondary <= 0.0f )
		return;

	wchar_t wszPrimary[16];
	wchar_t wszSecondary[16];
	wchar_t wszText[256];
	V_snwprintf( wszPrimary, ARRAYSIZE( wszPrimary ), L"%d",
		FoFAccuracyPercent( flPrimary > 0.0f ? flPrimary : flSecondary ) );
	V_snwprintf( wszSecondary, ARRAYSIZE( wszSecondary ), L"%d",
		FoFAccuracyPercent( flSecondary ) );

	const bool bDual = flPrimary > 0.0f && flSecondary > 0.0f;
	const wchar_t *pFormat = g_pVGuiLocalize->Find(
		bDual ? "#CH_Accuracy_Stats_Dual" : "#CH_Accuracy_Stats" );
	if ( !pFormat )
		return;

	if ( bDual )
	{
		g_pVGuiLocalize->ConstructString(
			wszText, sizeof( wszText ), pFormat, 2,
			wszPrimary, wszSecondary );
	}
	else
	{
		g_pVGuiLocalize->ConstructString(
			wszText, sizeof( wszText ), pFormat, 1, wszPrimary );
	}

	int nWide = 0;
	int nTall = 0;
	surface()->GetTextSize( hFont, wszText, nWide, nTall );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( color );
	surface()->DrawSetTextPos(
		static_cast< int >( flX - nWide * 0.5f ),
		static_cast< int >( flY + 30.0f * ScreenHeight() / 480.0f ) );
	surface()->DrawPrintText( wszText, V_wcslen( wszText ) );
}

FoFCrosshairState *FoFCreateCrosshairState()
{
	return new FoFCrosshairState;
}

void FoFDestroyCrosshairState( FoFCrosshairState *pState )
{
	delete pState;
}

void FoFResetCrosshairState( FoFCrosshairState *pState )
{
	if ( !pState )
		return;

	pState->m_flFriendlyUntil = 0.0f;
	pState->m_flUseUntil = 0.0f;
}

void FoFApplyCrosshairScheme(
	FoFCrosshairState *pState, vgui::IScheme *pScheme )
{
	if ( !pState || !pScheme )
		return;

	pState->m_hTextFont = pScheme->GetFont( "DefaultFoF", true );
	pState->m_pFriendlyCrosshair = gHUD.GetIcon( "ff_crosshair" );
	pState->m_pUseCrosshair = gHUD.GetIcon( "whiskey" );
	pState->m_pWeaponThrow = gHUD.GetIcon( "weapon_throw" );
	pState->m_pWeaponThrow2 = gHUD.GetIcon( "weapon_throw2" );
}

static C_BasePlayer *FoFResolveCrosshairPlayer( C_BasePlayer *pLocalPlayer )
{
	if ( pLocalPlayer &&
		pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BasePlayer *pObservedPlayer =
			dynamic_cast< C_BasePlayer * >(
				pLocalPlayer->GetObserverTarget() );
		if ( pObservedPlayer )
			return pObservedPlayer;
	}

	return pLocalPlayer;
}

static bool FoFShouldShowObservedWeaponCrosshair(
	C_BasePlayer *pLocalPlayer,
	C_BasePlayer *pCrosshairPlayer,
	C_BaseCombatWeapon *pWeapon )
{
	if ( !pLocalPlayer || !pCrosshairPlayer || !pWeapon )
		return false;

	const bool bObservedInEye =
		pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE &&
		pCrosshairPlayer != pLocalPlayer;
	return bObservedInEye &&
		FoFSightExpFactor( pCrosshairPlayer ) <= 0.0f;
}

bool FoFShouldSuppressWeaponCrosshair( C_BasePlayer *pLocalPlayer )
{
	C_BasePlayer *pCrosshairPlayer =
		FoFResolveCrosshairPlayer( pLocalPlayer );
	C_BaseCombatWeapon *pWeapon = pCrosshairPlayer ?
		pCrosshairPlayer->GetActiveWeapon() : NULL;
	if ( !pWeapon || pWeapon->ShouldDrawCrosshair() )
		return false;

	return !FoFShouldShowObservedWeaponCrosshair(
		pLocalPlayer, pCrosshairPlayer, pWeapon );
}

void FoFPaintCrosshair(
	FoFCrosshairState *pState,
	C_BasePlayer *pLocalPlayer,
	float flX,
	float flY )
{
	if ( !pState || !pLocalPlayer )
		return;

	C_BasePlayer *pCrosshairPlayer =
		FoFResolveCrosshairPlayer( pLocalPlayer );
	if ( !pCrosshairPlayer )
		return;

	const float flScreenScale =
		static_cast< float >( ScreenHeight() ) / 480.0f;
	const float flConfiguredScale =
		fof_ch_scalesize.GetFloat() * flScreenScale;
	const float flApertureScale = RemapValClamped(
		flConfiguredScale, 1.0f, 20.0f, 0.25f, 2.0f );
	const float flFadeEnd = 30.0f * flScreenScale;
	const bool bSwitchHands = fof_ch_switch_crosshair.GetBool();
	const int nRed = cl_crosshair_r.GetInt();
	const int nGreen = cl_crosshair_g.GetInt();
	const int nBlue = cl_crosshair_b.GetInt();
	const int nCrosshairAlpha = cl_crosshair_a.GetInt();
	const Color crosshairColor(
		nRed, nGreen, nBlue, nCrosshairAlpha );
	float flRawAperture[2];
	float flGap[2];

	for ( int nHand = 0; nHand < 2; ++nHand )
	{
		flRawAperture[nHand] =
			FoFCrosshairAperture( pCrosshairPlayer, nHand );
		flGap[nHand] = clamp(
			flRawAperture[nHand] * flApertureScale * 145.0f * flScreenScale,
			0.0f, 40.0f * flScreenScale );
	}

	if ( pCrosshairPlayer == pLocalPlayer && pLocalPlayer->IsAlive() )
	{
		C_BaseCombatWeapon *pActiveWeapon =
			pLocalPlayer->GetActiveWeapon();
		if ( pActiveWeapon && pActiveWeapon->ShouldDrawCrosshair() &&
			flRawAperture[0] <= 0.0f && flRawAperture[1] <= 0.0f )
		{
			const bool bDual = pLocalPlayer->HasDualActiveWeapons();
			for ( int nHand = 0; nHand < ( bDual ? 2 : 1 ); ++nHand )
			{
				flRawAperture[nHand] = 0.04f;
				flGap[nHand] = clamp(
					flRawAperture[nHand] * flApertureScale * 145.0f *
						flScreenScale,
					0.0f, 40.0f * flScreenScale );
			}
		}
	}

	if ( pCrosshairPlayer == pLocalPlayer && pLocalPlayer->IsAlive() &&
		pLocalPlayer->GetActiveWeapon() && !friendlyfire.GetBool() )
	{
		Vector vecForward;
		pLocalPlayer->EyeVectors( &vecForward );
		const Vector vecStart = pLocalPlayer->Weapon_ShootPosition();
		trace_t tr;
		UTIL_TraceLine( vecStart, vecStart + vecForward * 2000.0f,
			MASK_SHOT, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			if ( HL2MPRules() && HL2MPRules()->IsTeamplay() &&
				pLocalPlayer->InSameTeam( tr.m_pEnt ) )
			{
				pState->m_flFriendlyUntil = FoFMarkerExpirationTime();
			}
		}
		else if ( tr.m_pEnt &&
			tr.m_pEnt->IsEffectActive( FOF_EF_USE_PROMPT ) )
		{
			const char *pClassname = tr.m_pEnt->GetClassname();
			const float flUseDistance =
				pClassname && V_stristr( pClassname, "crate" ) ?
				40.0f : 75.0f;
			if ( ( tr.endpos - vecStart ).Length() <= flUseDistance )
				pState->m_flUseUntil = FoFMarkerExpirationTime();
		}
	}

	if ( pCrosshairPlayer == pLocalPlayer )
	{
		DrawFoFAccuracyStats(
			pCrosshairPlayer, pState->m_hTextFont,
			flX, flY, crosshairColor );
	}

	const float flThrowProgress =
		FoFWeaponThrowProgress( pCrosshairPlayer );
	if ( flThrowProgress > 0.0f )
	{
		DrawFoFThrowIndicator(
			pState->m_pWeaponThrow, pState->m_pWeaponThrow2,
			1.0f - flThrowProgress, flX, flY, crosshairColor );
		return;
	}

	if ( pState->m_flUseUntil > gpGlobals->curtime )
	{
		DrawFoFTextureCentered(
			pState->m_pUseCrosshair, flX, flY, crosshairColor );

		const wchar_t *pUseText =
			g_pVGuiLocalize->Find( "#UseHelpText" );
		if ( pUseText && pState->m_hTextFont != vgui::INVALID_FONT )
		{
			wchar_t wszUseText[256];
			FoFReplaceKeyBindings(
				pUseText, 0, wszUseText, sizeof( wszUseText ) );

			int nWide = 0;
			int nTall = 0;
			surface()->GetTextSize(
				pState->m_hTextFont, wszUseText, nWide, nTall );
			surface()->DrawSetTextFont( pState->m_hTextFont );
			surface()->DrawSetTextColor( crosshairColor );
			surface()->DrawSetTextPos(
				static_cast< int >( flX - nWide * 0.5f ),
				static_cast< int >(
					flY + 20.0f * ScreenHeight() / 480.0f ) );
			surface()->DrawPrintText(
				wszUseText, V_wcslen( wszUseText ) );
		}
		return;
	}

	if ( fof_crosshair_ff.GetBool() &&
		pState->m_flFriendlyUntil > gpGlobals->curtime )
	{
		DrawFoFTextureCentered(
			pState->m_pFriendlyCrosshair, flX, flY, crosshairColor );
		return;
	}

	const int nAlpha = clamp( RoundFloatToInt( RemapValClamped(
		flGap[0], 0.0f, flFadeEnd, 255.0f, 1.0f ) ), 1, 255 );

	for ( int nHand = 0; nHand < 2; ++nHand )
	{
		if ( flRawAperture[nHand] <= 0.0f )
			continue;

		const bool bPlusStyle = ( nHand == 0 ) != bSwitchHands;
		DrawFoFHandCrosshair(
			bPlusStyle, flConfiguredScale, flGap[nHand],
			flX + 1.0f, flY + 1.0f, Color( 1, 1, 1, nAlpha ) );
		DrawFoFHandCrosshair(
			bPlusStyle, flConfiguredScale, flGap[nHand],
			flX, flY, Color( nRed, nGreen, nBlue, nAlpha ) );
	}

	if ( fof_ch_drawdot.GetBool() )
	{
		const float flThickness =
			MAX( 1.0f, fof_ch_thickness.GetFloat() );
		surface()->DrawSetColor( nRed, nGreen, nBlue, 255 );
		surface()->DrawFilledRect(
			static_cast< int >( flX - flThickness ),
			static_cast< int >( flY - flThickness ),
			static_cast< int >( flX + flThickness ),
			static_cast< int >( flY + flThickness ) );
	}
}
