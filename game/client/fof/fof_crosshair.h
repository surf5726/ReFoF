//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fistful of Frags crosshair presentation.
//
//=============================================================================//

#ifndef FOF_CROSSHAIR_H
#define FOF_CROSSHAIR_H
#ifdef _WIN32
#pragma once
#endif

class C_BasePlayer;

namespace vgui
{
	class IScheme;
}

struct FoFCrosshairState;

FoFCrosshairState *FoFCreateCrosshairState();
void FoFDestroyCrosshairState( FoFCrosshairState *pState );
void FoFResetCrosshairState( FoFCrosshairState *pState );
void FoFApplyCrosshairScheme(
	FoFCrosshairState *pState, vgui::IScheme *pScheme );

bool FoFShouldSuppressWeaponCrosshair( C_BasePlayer *pLocalPlayer );

void FoFPaintCrosshair(
	FoFCrosshairState *pState,
	C_BasePlayer *pLocalPlayer,
	float flX,
	float flY );

#endif // FOF_CROSSHAIR_H
