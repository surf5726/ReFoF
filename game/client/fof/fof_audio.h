#ifndef FOF_AUDIO_H
#define FOF_AUDIO_H
#ifdef _WIN32
#pragma once
#endif

#include "interval.h"

interval_t FoFReadSoundscapeVolumeInterval( const char *pszValue );
float FoFSanitizeSoundscapeVolume( float flVolume );

class C_BaseCombatWeapon;
struct CSoundParameters;

bool FoFEmitOccludedWeaponSound( C_BaseCombatWeapon *pWeapon,
	const CSoundParameters &params );

#endif // FOF_AUDIO_H
