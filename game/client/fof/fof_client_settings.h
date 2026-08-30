#ifndef FOF_CLIENT_SETTINGS_H
#define FOF_CLIENT_SETTINGS_H
#ifdef _WIN32
#pragma once
#endif

float FoFStableMousePitch( float flConfiguredPitch );

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF client network settings.
//
//===========================================================================//

namespace FoFClientNetworkSettings
{
	const float kInterpMinimum = 0.1f;
	const float kInterpMaximum = 0.5f;
	const float kInterpRatioMinimum = 1.0f;
	const float kInterpRatioMaximum = 2.0f;
}

class ConVar;
class C_BasePlayer;

extern ConVar fof_blood_allowed;
extern ConVar fof_weapon_lean;
extern ConVar cl_sidespeed;
extern ConVar cl_upspeed;
extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar hl2mp_max_separation_force;
extern ConVar v_viewmodel_fov;

bool FoFResolveZoomSensitivity(
	C_BasePlayer *pPlayer,
	int nLocalFOV,
	int nDefaultFOV,
	float &flZoomSensitivityRatio );

int FoFReadPersistentClientInt( const char *pszName, int nFallback );
void FoFWritePersistentClientInt( const char *pszName, int nValue );
int FoFLauncherPingLimitFromIndex( int nIndex );
int FoFLauncherPingLimitIndexFromValue( int nValue );

#endif // FOF_CLIENT_SETTINGS_H
