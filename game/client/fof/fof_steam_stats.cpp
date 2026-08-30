#include "cbase.h"
#include "fof/fof_steam_stats.h"
#include "achievementmgr.h"
#include "filesystem.h"
#include "steam/steam_api.h"

#include "tier0/memdbgon.h"

// FoF walks the original 56-entry gameplay table and discards disabled,
// non-damaging and category-12 entries.  These are the 38 remaining sources,
// in the exact order used by fof_mystats and the personal-statistics panel.
const FoFWeaponStatSource
	g_FoFWeaponStatSources[FOF_WEAPON_STAT_SOURCE_COUNT] =
{
	{ "horse-ram", 11 },
	{ "kick-fall", 11 },
	{ "flame", 11 },
	{ "thrown_gun", 11 },
	{ "kick", 11 },
	{ "blast", 11 },
	{ "physics", 11 },
	{ "dynamite_black", 5 },
	{ "dynamite_yellow", 5 },
	{ "dynamite", 5 },
	{ "arrow", 13 },
	{ "arrow_black", 13 },
	{ "x_arrow", 13 },
	{ "thrown_knife", 8 },
	{ "thrown_axe", 8 },
	{ "thrown_machete", 8 },
	{ "fists", 0 },
	{ "fists_brass", 0 },
	{ "ghostgun", 2 },
	{ "knife", 8 },
	{ "deringer", 2 },
	{ "volcanic", 2 },
	{ "coltnavy", 2 },
	{ "axe", 8 },
	{ "sawedoff_shotgun", 2 },
	{ "hammerless", 2 },
	{ "remington_army", 2 },
	{ "maresleg", 2 },
	{ "schofield", 2 },
	{ "carbine", 3 },
	{ "peacemaker", 2 },
	{ "henryrifle", 3 },
	{ "coachgun", 3 },
	{ "spencer", 3 },
	{ "machete", 8 },
	{ "shotgun", 3 },
	{ "sharps", 3 },
	{ "walker", 2 }
};

bool FoFReadSteamIntStat(
	ISteamUserStats *pStats,
	const char *pszName,
	int &value )
{
	value = 0;
	if ( !pStats || !pszName || !pszName[0] )
		return false;

	int32 steamValue = 0;
	if ( !pStats->GetStat( pszName, &steamValue ) )
		return false;

	value = steamValue;
	return true;
}

float FoFReadSteamFloatStat(
	ISteamUserStats *pStats,
	const char *pszName )
{
	if ( !pStats || !pszName || !pszName[0] )
		return 0.0f;

	float floatValue = 0.0f;
	if ( pStats->GetStat( pszName, &floatValue ) )
		return floatValue;

	int intValue = 0;
	FoFReadSteamIntStat( pStats, pszName, intValue );
	return (float)intValue;
}

static ISteamUserStats *FoFCurrentUserStats()
{
	return steamapicontext ? steamapicontext->SteamUserStats() : NULL;
}

CON_COMMAND( reset_user_stats, "" )
{
	ISteamUserStats *pStats = FoFCurrentUserStats();
	if ( pStats )
		pStats->ResetAllStats( false );
}

CON_COMMAND( reset_achievement, "" )
{
	CAchievementMgr *pAchievementMgr =
		dynamic_cast< CAchievementMgr * >( engine->GetAchievementMgr() );
	if ( !pAchievementMgr || args.ArgC() <= 1 )
		return;

	CBaseAchievement *pAchievement =
		pAchievementMgr->GetAchievementByName( args[1] );
	if ( pAchievement )
	{
		pAchievementMgr->ResetAchievement(
			pAchievement->GetAchievementID() );
	}
}

CON_COMMAND( delete_soundcache, "" )
{
	if ( !filesystem )
		return;

	filesystem->RemoveFile( "sound/sound.cache", "MOD" );
	filesystem->RemoveFile( "fof.vpk.sound.cache", "MOD" );
}

static int FoFReadWeaponIntStat(
	ISteamUserStats *pStats,
	const char *pszSource,
	const char *pszSuffix )
{
	char statName[80];
	Q_snprintf(
		statName,
		sizeof( statName ),
		"stat_%s_%s",
		pszSource ? pszSource : "",
		pszSuffix ? pszSuffix : "" );
	int value = 0;
	FoFReadSteamIntStat( pStats, statName, value );
	return value;
}

float FoFComputeReportedAccuracy()
{
	ISteamUserStats *pStats = FoFCurrentUserStats();
	if ( !pStats )
		return 0.0f;

	int totalHits = 0;
	int totalMisses = 0;
	int totalDamage = 0;
	for ( int i = 0; i < FOF_WEAPON_STAT_SOURCE_COUNT; ++i )
	{
		const FoFWeaponStatSource &source = g_FoFWeaponStatSources[i];
		// C_FoF_Player::GetAccuracy excludes these four gameplay categories.
		// Category 12 entries were already removed from the compact table.
		if ( source.m_iCategory == 7 || source.m_iCategory == 4 ||
			source.m_iCategory == 10 || source.m_iCategory == 12 )
		{
			continue;
		}

		const int hits = FoFReadWeaponIntStat(
			pStats, source.m_pszName, "hits" );
		const int misses = FoFReadWeaponIntStat(
			pStats, source.m_pszName, "misses" );
		if ( hits > 0 )
		{
			totalHits += hits;
			totalDamage += FoFReadWeaponIntStat(
				pStats, source.m_pszName, "dmg" );
		}
		if ( misses > 0 )
			totalMisses += misses;
	}

	// The shipped client reports zero until the accumulated damage reaches
	// 750, and protects a fresh profile with a 25-shot denominator floor.
	if ( totalDamage < 750 )
		return 0.0f;
	const int denominator = MAX( totalHits + totalMisses, 25 );
	return (float)totalHits / (float)denominator * 100.0f;
}

CON_COMMAND( fof_mystats, "" )
{
	ISteamUserStats *pStats = FoFCurrentUserStats();
	if ( !pStats )
		return;

	int totalFrags = 0;
	float totalAccuracy = 0.0f;
	int accuracyEntries = 0;
	for ( int i = 0; i < FOF_WEAPON_STAT_SOURCE_COUNT; ++i )
	{
		const FoFWeaponStatSource &source = g_FoFWeaponStatSources[i];
		const int damage = FoFReadWeaponIntStat(
			pStats, source.m_pszName, "dmg" );
		const int frags = FoFReadWeaponIntStat(
			pStats, source.m_pszName, "kills" );
		Msg( "%s damage %i ", source.m_pszName, damage );
		Msg( "%s kills %i ", source.m_pszName, frags );
		totalFrags += frags;

		if ( source.m_iCategory == 2 || source.m_iCategory == 3 ||
			source.m_iCategory == 13 )
		{
			const int misses = FoFReadWeaponIntStat(
				pStats, source.m_pszName, "misses" );
			const int hits = FoFReadWeaponIntStat(
				pStats, source.m_pszName, "hits" );
			const int shots = hits + misses;
			const float accuracy = shots > 0 ?
				(float)hits / (float)shots : 0.0f;
			Msg( "%s accuracy %.2f ", source.m_pszName, accuracy );
			totalAccuracy += accuracy;
			++accuracyEntries;
		}
	}

	int assists = 0;
	FoFReadSteamIntStat( pStats, "stat_assists", assists );
	const float effectiveMinutes =
		FoFReadSteamFloatStat( pStats, "stat_play_time" ) / 60.0f;
	const float fragsPerMinute = effectiveMinutes > 0.0f ?
		(float)totalFrags / effectiveMinutes : 0.0f;
	const float assistsPerMinute = effectiveMinutes > 0.0f ?
		(float)assists / effectiveMinutes : 0.0f;
	Msg( "Total Accuracy %f ", accuracyEntries > 0 ?
		totalAccuracy / (float)accuracyEntries : 0.0f );
	Msg( "Total Frags %i (%f per minute) ",
		totalFrags, fragsPerMinute );
	Msg( "Assists %i (%f per minute) ",
		assists, assistsPerMinute );
	Msg( "Total effective play time %.2f minutes ", effectiveMinutes );
}
