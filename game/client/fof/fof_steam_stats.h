#ifndef FOF_STEAM_STATS_H
#define FOF_STEAM_STATS_H
#ifdef _WIN32
#pragma once
#endif

class ISteamUserStats;

struct FoFWeaponStatSource
{
	const char *m_pszName;
	int m_iCategory;
};

enum { FOF_WEAPON_STAT_SOURCE_COUNT = 38 };

extern const FoFWeaponStatSource
	g_FoFWeaponStatSources[FOF_WEAPON_STAT_SOURCE_COUNT];

bool FoFReadSteamIntStat(
	ISteamUserStats *pStats,
	const char *pszName,
	int &value );
float FoFReadSteamFloatStat(
	ISteamUserStats *pStats,
	const char *pszName );
float FoFComputeReportedAccuracy();

#endif // FOF_STEAM_STATS_H
