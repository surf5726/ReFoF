//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Grand Elimination and Battle Royale server authority.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_elimination_mode.h"
#include "fof/fof_course_mode.h"

#include "fof/fof_objectives.h"
#include "fof/fof_player.h"
#include "hl2mp_gamerules.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "recipientfilter.h"
#include "team.h"
#include "util.h"

#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_elimination, CDarkMode );

static bool FoFIsModePlayer( CFoF_Player *pPlayer );

static bool FoFBattleRoyaleModePlayer( CFoF_Player *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() &&
		!pPlayer->IsFoFBotGhost() &&
		pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

static bool FoFEliminationProtectsSurvivor( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return false;

	CNavArea *pPlayerArea = NULL;
	if ( TheNavMesh && TheNavMesh->IsLoaded() )
	{
		pPlayerArea = TheNavMesh->GetNearestNavArea(
			pPlayer->GetAbsOrigin(), false, 10000.0f,
			false, true, TEAM_ANY );
	}

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pSurvivor =
			ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pSurvivor ) || pSurvivor == pPlayer ||
			!pSurvivor->IsAlive() || pSurvivor->GetFoFPlayerKills() < 0 ||
			pSurvivor->GetTeamNumber() != pPlayer->GetTeamNumber() )
		{
			continue;
		}

		if ( pSurvivor->GetAbsOrigin().DistTo(
			pPlayer->GetAbsOrigin() ) < 1000.0f &&
			pSurvivor->FVisible( pPlayer, 0x400b, NULL ) )
		{
			return true;
		}

		if ( !pPlayerArea )
			continue;
		CNavArea *pSurvivorArea = TheNavMesh->GetNearestNavArea(
			pSurvivor->GetAbsOrigin(), false, 10000.0f,
			false, true, TEAM_ANY );
		if ( !pSurvivorArea )
			continue;

		ShortestPathCost pathCost;
		const float flForward = NavAreaTravelDistance(
			pPlayerArea, pSurvivorArea, pathCost, 512.0f );
		const float flReverse = NavAreaTravelDistance(
			pSurvivorArea, pPlayerArea, pathCost, 512.0f );
		if ( ( flForward >= 0.0f && flForward < 512.0f ) ||
			( flReverse >= 0.0f && flReverse < 512.0f ) )
		{
			return true;
		}
	}
	return false;
}

static float FoFBattleRoyaleConVarFloat(
	const char *pszName, float flDefault, float flMinimum )
{
	ConVarRef value( pszName, true );
	return value.IsValid() ?
		MAX( flMinimum, value.GetFloat() ) : flDefault;
}

static int __cdecl FoFBattleRoyaleAreaLess(
	const FoFBattleRoyaleArea_t *pLeft,
	const FoFBattleRoyaleArea_t *pRight )
{
	if ( pLeft->m_nDistance < pRight->m_nDistance )
		return -1;
	if ( pLeft->m_nDistance > pRight->m_nDistance )
		return 1;
	return 0;
}

static void FoFFireBattleRoyaleExtraTime( float flDuration )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "elm_extra_time" ) : NULL;
	if ( !pEvent )
		return;

	pEvent->SetInt( "extra_time",
		MAX( 1, RoundFloatToInt( flDuration ) ) );
	gameeventmanager->FireEvent( pEvent );
}

static void FoFSendBattleRoyaleHealEffect(
	CFoF_Player *pPlayer, const Vector &vecSource )
{
	if ( !pPlayer )
		return;

	// In the original CDarkMode update this is an
	// unreliable player entity message whose vector is the cap entity origin.
	EntityMessageBegin( pPlayer, false );
		WRITE_BYTE( 16 );
		WRITE_VEC3COORD( vecSource );
	MessageEnd();
}

void CDarkMode::ResetBattleRoyaleState( bool bRemoveCap )
{
	if ( m_bBattleRoyaleSafeActive )
		SendBattleRoyaleMarker( false );

	CFoFCapEnt *pCap = m_hBattleRoyaleCap.Get();
	if ( pCap )
	{
		pCap->AddEffects( EF_NODRAW );
		if ( bRemoveCap )
			UTIL_Remove( pCap );
	}
	if ( bRemoveCap )
		m_hBattleRoyaleCap = NULL;

	CHL2MPRules *pRules = HL2MPRules();
	if ( pRules )
		pRules->ResetFoFSafeZoneNetwork();

	m_BattleRoyaleAreas.Purge();
	m_vecBattleRoyaleCenter.Init();
	m_nBattleRoyalePhase = 0;
	m_bBattleRoyaleSafeActive = false;
	m_flBattleRoyalePreSafeEnd = 0.0f;
	m_flBattleRoyaleShrinkEnd = 0.0f;
	m_flBattleRoyalePostSafeEnd = 0.0f;
	m_flBattleRoyaleNextTick = 0.0f;
	m_flBattleRoyaleAreaPerTick = 0.0f;
	m_flBattleRoyaleRemainingArea = 0.0f;
}

void CDarkMode::InitializeBattleRoyaleRound()
{
	ResetBattleRoyaleState( true );

	CUtlVector< Vector > centers;
	if ( FoFLoadEliminationSafeZoneCenters( centers ) )
	{
		m_vecBattleRoyaleCenter =
			centers[RandomInt( 0, centers.Count() - 1 )];
	}
	else if ( TheNavAreas.Count() > 0 )
	{
		CNavArea *pFallback =
			TheNavAreas[RandomInt( 0, TheNavAreas.Count() - 1 )];
		if ( pFallback )
			m_vecBattleRoyaleCenter = pFallback->GetCenter();
		Warning( "Grand Elimination: no check_capture entries for %s; "
			"using a nav-area fallback.\n", STRING( gpGlobals->mapname ) );
	}
	else
	{
		Warning( "Grand Elimination: map %s has neither safe-zone entries "
			"nor navigation areas.\n", STRING( gpGlobals->mapname ) );
	}

	CBaseEntity *pEntity = CreateEntityByName( "fof_cap_entity" );
	CFoFCapEnt *pCap = pEntity ?
		static_cast< CFoFCapEnt * >( pEntity ) : NULL;
	if ( pCap )
	{
		pCap->KeyValue( "ClassFilter", "99" );
		pCap->KeyValue( "Enabled", "1" );
		// Radius 256 selects the original cap_circle_512 model.  Safe-zone
		// gameplay uses the recovered 350-unit radius below.
		pCap->KeyValue( "Radius", "256" );
		pCap->SetAbsOrigin( m_vecBattleRoyaleCenter );
		pCap->SetAbsAngles( vec3_angle );
		DispatchSpawn( pCap );
		pCap->AddEffects( EF_NODRAW );
		m_hBattleRoyaleCap = pCap;
	}

	const float flSearchRadiusSqr = 1200.0f * 1200.0f;
	for ( int i = 0;
		i < TheNavAreas.Count() && m_BattleRoyaleAreas.Count() < 500; ++i )
	{
		CNavArea *pArea = TheNavAreas[i];
		if ( !pArea )
			continue;

		const float flArea = pArea->GetSizeX() * pArea->GetSizeY();
		if ( flArea < 2500.0f ||
			pArea->GetDistanceSquaredToPoint(
				m_vecBattleRoyaleCenter ) > flSearchRadiusSqr )
		{
			continue;
		}

		FoFBattleRoyaleArea_t area;
		area.m_vecNorthWest = pArea->GetCorner( NORTH_WEST );
		area.m_vecNorthEast = pArea->GetCorner( NORTH_EAST );
		area.m_vecSouthWest = pArea->GetCorner( SOUTH_WEST );
		area.m_vecSouthEast = pArea->GetCorner( SOUTH_EAST );
		area.m_vecNorthWest.z += 5.0f;
		area.m_vecNorthEast.z += 5.0f;
		area.m_vecSouthWest.z += 5.0f;
		area.m_vecSouthEast.z += 5.0f;
		area.m_vecCenter = pArea->GetCenter();
		area.m_nDistance = RoundFloatToInt(
			( area.m_vecCenter - m_vecBattleRoyaleCenter ).Length() );
		area.m_nArea = MAX( 1, RoundFloatToInt( flArea ) );
		m_BattleRoyaleAreas.AddToTail( area );
		m_flBattleRoyaleRemainingArea += (float)area.m_nArea;
	}

	m_BattleRoyaleAreas.Sort( FoFBattleRoyaleAreaLess );
	SetBattleRoyaleNetworkAreas( false );

	const float flPreSafe = FoFBattleRoyaleConVarFloat(
		"fof_sv_br_presafe_time", 25.0f, 0.0f );
	const float flSafe = FoFBattleRoyaleConVarFloat(
		"fof_sv_br_safe_time", 40.0f, 0.1f );
	const float flPostSafe = FoFBattleRoyaleConVarFloat(
		"fof_sv_br_postsafe_time", 90.0f, 0.1f );
	const float flTick = FoFBattleRoyaleConVarFloat(
		"fof_sv_br_healrate", 2.0f, 0.1f );

	m_nBattleRoyalePhase = 1;
	m_bBattleRoyaleSafeActive = false;
	m_flBattleRoyalePreSafeEnd = gpGlobals->curtime + flPreSafe;
	m_flBattleRoyaleShrinkEnd = m_flBattleRoyalePreSafeEnd + flSafe;
	m_flBattleRoyalePostSafeEnd =
		m_flBattleRoyaleShrinkEnd + flPostSafe;
	m_flBattleRoyaleNextTick = 0.0f;
	m_flBattleRoyaleAreaPerTick =
		m_flBattleRoyaleRemainingArea /
		MAX( 1.0f, flSafe / flTick );
	m_flRoundEndTime = m_flBattleRoyalePostSafeEnd;

	FoFFireBattleRoyaleExtraTime( flPreSafe );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( FoFBattleRoyaleModePlayer( pPlayer ) && !pPlayer->IsBot() )
			SendBattleRoyaleNotice( pPlayer, "#GELM_Hint_Start" );
	}

	Msg( "Areas added to safe zone: %i Total sq area to remove %f \n",
		m_BattleRoyaleAreas.Count(), m_flBattleRoyaleRemainingArea );
}

void CDarkMode::UpdateBattleRoyaleState()
{
	if ( m_nBattleRoyalePhase == 1 )
	{
		if ( !m_bBattleRoyaleSafeActive &&
			gpGlobals->curtime >= m_flBattleRoyalePreSafeEnd )
		{
			ActivateBattleRoyaleSafeZone();
		}

		if ( m_bBattleRoyaleSafeActive &&
			gpGlobals->curtime >= m_flBattleRoyaleNextTick &&
			gpGlobals->curtime < m_flBattleRoyaleShrinkEnd )
		{
			UpdateBattleRoyaleSafeZone();
		}

		if ( gpGlobals->curtime >= m_flBattleRoyaleShrinkEnd )
			EnterBattleRoyaleStandoff();
	}
	else if ( m_nBattleRoyalePhase == 2 )
	{
		CheckBattleRoyaleRound();
	}
}

void CDarkMode::ActivateBattleRoyaleSafeZone()
{
	if ( m_bBattleRoyaleSafeActive )
		return;

	m_bBattleRoyaleSafeActive = true;
	m_flBattleRoyaleNextTick = gpGlobals->curtime + 1.0f;
	if ( m_hBattleRoyaleCap )
		m_hBattleRoyaleCap->RemoveEffects( EF_NODRAW );
	SetBattleRoyaleNetworkAreas( true );
	SendBattleRoyaleMarker( true );

	FoFFireBattleRoyaleExtraTime(
		MAX( 0.1f, m_flBattleRoyaleShrinkEnd - gpGlobals->curtime ) );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( FoFBattleRoyaleModePlayer( pPlayer ) &&
			!pPlayer->IsBot() && pPlayer->IsFoFBattleRoyaleOutlaw() )
		{
			SendBattleRoyaleNotice( pPlayer, "#CW_SafeZone_Warning" );
		}
	}
}

bool CDarkMode::IsInsideBattleRoyaleZone( CFoF_Player *pPlayer ) const
{
	if ( !pPlayer )
		return false;

	const Vector vecPosition = pPlayer->GetAbsOrigin();
	if ( ( vecPosition - m_vecBattleRoyaleCenter ).Length() < 350.0f )
		return true;

	for ( int i = 0; i < m_BattleRoyaleAreas.Count(); ++i )
	{
		const Vector &vecArea = m_BattleRoyaleAreas[i].m_vecCenter;
		if ( fabsf( vecPosition.z - vecArea.z ) < 50.0f &&
			( vecPosition - vecArea ).Length() < 150.0f )
		{
			return true;
		}
	}
	return false;
}

void CDarkMode::UpdateBattleRoyaleSafeZone()
{
	const float flSafeDuration = MAX( 0.1f,
		m_flBattleRoyaleShrinkEnd - m_flBattleRoyalePreSafeEnd );
	const float flProgress = clamp(
		( gpGlobals->curtime - m_flBattleRoyalePreSafeEnd ) /
		flSafeDuration, 0.0f, 1.0f );
	const float flDamage = 2.0f + 6.0f * flProgress;
	CFoFCapEnt *pCap = m_hBattleRoyaleCap.Get();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFBattleRoyaleModePlayer( pPlayer ) || !pPlayer->IsAlive() )
			continue;

		const Vector vecDelta =
			pPlayer->GetAbsOrigin() - m_vecBattleRoyaleCenter;
		if ( !IsInsideBattleRoyaleZone( pPlayer ) )
		{
			CTakeDamageInfo damageInfo( this, this, flDamage, DMG_BURN );
			damageInfo.SetDamagePosition( pPlayer->WorldSpaceCenter() );
			pPlayer->TakeDamage( damageInfo );
		}
		else if ( pCap && vecDelta.Length() < 350.0f &&
			fabsf( vecDelta.z ) < 100.0f )
		{
			pPlayer->TakeHealth( 5.0f, DMG_GENERIC );
			FoFSendBattleRoyaleHealEffect(
				pPlayer, pCap->GetAbsOrigin() );
		}
	}

	int nRemovedArea = 0;
	while ( m_BattleRoyaleAreas.Count() > 0 &&
		(float)nRemovedArea < m_flBattleRoyaleAreaPerTick )
	{
		const int nIndex = m_BattleRoyaleAreas.Count() - 1;
		const FoFBattleRoyaleArea_t &area = m_BattleRoyaleAreas[nIndex];
		nRemovedArea += area.m_nArea;

		CHL2MPRules *pRules = HL2MPRules();
		if ( pRules )
		{
			pRules->SetFoFSafeZoneQuad( nIndex,
				area.m_vecNorthWest, area.m_vecNorthEast,
				area.m_vecSouthWest, area.m_vecSouthEast, false );
		}
		m_BattleRoyaleAreas.Remove( nIndex );
	}
	m_flBattleRoyaleRemainingArea = MAX( 0.0f,
		m_flBattleRoyaleRemainingArea - (float)nRemovedArea );
	m_flBattleRoyaleNextTick = gpGlobals->curtime +
		FoFBattleRoyaleConVarFloat(
			"fof_sv_br_healrate", 2.0f, 0.1f );
}

void CDarkMode::EnterBattleRoyaleStandoff()
{
	if ( m_nBattleRoyalePhase != 1 )
		return;

	if ( m_hBattleRoyaleCap )
	{
		m_hBattleRoyaleCap->AddEffects( EF_NODRAW );
		m_hBattleRoyaleCap->EmitSound( "BB.MostWantedWarning" );
	}
	else
	{
		EmitSound( "BB.MostWantedWarning" );
	}
	SendBattleRoyaleMarker( false );
	if ( HL2MPRules() )
		HL2MPRules()->ResetFoFSafeZoneNetwork();

	m_bBattleRoyaleSafeActive = false;
	m_nBattleRoyalePhase = 2;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFBattleRoyaleModePlayer( pPlayer ) || !pPlayer->IsAlive() )
			continue;

		pPlayer->EmitSound( "Standoff_Music" );
		const bool bOutlawInCircle = pPlayer->IsFoFBattleRoyaleOutlaw() &&
			( pPlayer->GetAbsOrigin() -
				m_vecBattleRoyaleCenter ).Length() < 350.0f;
		if ( bOutlawInCircle )
		{
			pPlayer->SetFoFPlayerKills( 0 );
			if ( !pPlayer->IsBot() )
				SendBattleRoyaleNotice( pPlayer, "#GELM_Hint_Clash1" );
		}
		else
		{
			pPlayer->SetFoFBattleRoyaleOutlaw( false );
			pPlayer->SetFoFPlayerKills( -1 );
			pPlayer->SetFoFBattleRoyaleRoleAppearance( false );
			if ( !pPlayer->IsBot() )
				SendBattleRoyaleNotice( pPlayer, "#GELM_Hint_Clash2" );
		}
	}
}

void CDarkMode::SetBattleRoyaleNetworkAreas( bool bActive )
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules )
		return;

	pRules->ResetFoFSafeZoneNetwork();
	for ( int i = 0; i < m_BattleRoyaleAreas.Count(); ++i )
	{
		const FoFBattleRoyaleArea_t &area = m_BattleRoyaleAreas[i];
		pRules->SetFoFSafeZoneQuad( i,
			area.m_vecNorthWest, area.m_vecNorthEast,
			area.m_vecSouthWest, area.m_vecSouthEast, bActive );
	}
}

void CDarkMode::SendBattleRoyaleNotice(
	CFoF_Player *pPlayer, const char *pszToken ) const
{
	if ( !pPlayer || !pszToken || !pszToken[0] )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
		WRITE_STRING( "" );
		WRITE_STRING( "" );
		WRITE_STRING( "" );
	MessageEnd();
}

void CDarkMode::SendBattleRoyaleMarker( bool bAdd ) const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFBattleRoyaleModePlayer( pPlayer ) || pPlayer->IsBot() )
			continue;

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "HUDBBMulti" );
			WRITE_BYTE( bAdd ? 1 : 0 );
			WRITE_BYTE( bAdd ? 999 : 0 );
			WRITE_BYTE( 7 );
			WRITE_VEC3COORD( m_vecBattleRoyaleCenter );
			WRITE_STRING( "SAFE ZONE" );
		MessageEnd();
	}
}
static int FoFMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static bool FoFWarmupActive()
{
	static ConVarRef warmup( "fof_warmup", true );
	return warmup.IsValid() && warmup.GetBool();
}

static bool FoFBattleRoyaleActive()
{
	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	return battleRoyale.IsValid() && battleRoyale.GetBool();
}

static bool FoFBattleRoyaleFFA()
{
	static ConVarRef battleRoyaleFFA( "fof_sv_battle_royale_ffa", true );
	return battleRoyaleFFA.IsValid() && battleRoyaleFFA.GetBool();
}

bool FoFBattleRoyaleCanAcquireOutlawRole()
{
	CBaseEntity *pEntity =
		gEntList.FindEntityByClassname( NULL, "fof_elimination" );
	CDarkMode *pMode = pEntity ? static_cast< CDarkMode * >( pEntity ) : NULL;
	return pMode && pMode->CanAcquireBattleRoyaleOutlawRole();
}

static bool FoFModeUsesTeamplay()
{
	return HL2MPRules() && HL2MPRules()->IsTeamplay();
}
static bool FoFIsModePlayer( CFoF_Player *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() &&
		!pPlayer->IsFoFBotGhost() &&
		pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

CBaseEntity *CDarkMode::GetRoundSpawnPoint( CFoF_Player *pPlayer ) const
{
	if ( !m_bSpawningRoundPlayers || FoFBattleRoyaleActive() || !pPlayer )
		return NULL;
	const int nTeam = pPlayer->GetTeamNumber();
	return nTeam == TEAM_REBELS ? m_hTeamSpawnPoints[1].Get() :
		m_hTeamSpawnPoints[0].Get();
}

bool CDarkMode::SelectTeamSpawnPoints()
{
	CUtlVector< CBaseEntity * > points;
	CBaseEntity *pPoint = NULL;
	while ( ( pPoint = gEntList.FindEntityByClassname(
		pPoint, "info_player_fof" ) ) != NULL )
	{
		points.AddToTail( pPoint );
	}
	if ( points.Count() < 2 )
		return false;

	float flMinimumDistance = 4000.0f;
	for ( int nAttempt = 0; nAttempt < 2000; ++nAttempt )
	{
		CBaseEntity *pFirst = points[RandomInt( 0, points.Count() - 1 )];
		CBaseEntity *pSecond = points[RandomInt( 0, points.Count() - 1 )];
		if ( nAttempt > 0 && nAttempt % 50 == 0 )
			flMinimumDistance -= 200.0f;
		if ( pFirst == pSecond || pFirst->GetAbsOrigin().DistTo(
			pSecond->GetAbsOrigin() ) < flMinimumDistance )
		{
			continue;
		}
		trace_t trace;
		UTIL_TraceLine( pFirst->GetAbsOrigin() + Vector( 0, 0, 70 ),
			pSecond->GetAbsOrigin() + Vector( 0, 0, 70 ),
			MASK_SHOT & ~CONTENTS_GRATE, NULL, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction >= 1.0f )
			continue;
		m_hTeamSpawnPoints[0] = pFirst;
		m_hTeamSpawnPoints[1] = pSecond;
		m_vecTeamSpawnOrigins[0] = pFirst->GetAbsOrigin();
		m_vecTeamSpawnOrigins[1] = pSecond->GetAbsOrigin();
		m_bHaveTeamSpawnPoints = true;
		return true;
	}
	Warning( "Elimination: no separated, occluded spawn pair on %s.\n",
		STRING( gpGlobals->mapname ) );
	return false;
}

bool CDarkMode::ResetRoundScene()
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules )
		return false;
	const bool bBattleRoyale = FoFBattleRoyaleActive();
	if ( !bBattleRoyale && !m_bHaveTeamSpawnPoints && !SelectTeamSpawnPoints() )
		return false;

	UpdatePlayerGlowRegistration( false );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;
		pPlayer->PrepareFoFRoundRespawn();
		pPlayer->RemoveFlag( FL_FROZEN | FL_ATCONTROLS );
		if ( !m_bHadRound )
			pPlayer->ResetScores();
		pPlayer->m_nLastRoundNotoriety = 0;
		if ( !bBattleRoyale )
			pPlayer->m_flFoFCash = 30.0f;
	}
	pRules->CleanUpMap();

	if ( !bBattleRoyale )
	{
		V_swap( m_vecTeamSpawnOrigins[0], m_vecTeamSpawnOrigins[1] );
		for ( int nTeam = 0; nTeam < 2; ++nTeam )
		{
			m_hTeamSpawnPoints[nTeam] = NULL;
			CBaseEntity *pPoint = NULL;
			while ( ( pPoint = gEntList.FindEntityByClassname(
				pPoint, "info_player_fof" ) ) != NULL )
			{
				if ( pPoint->GetAbsOrigin().DistToSqr( m_vecTeamSpawnOrigins[nTeam] ) < 1.0f )
				{
					m_hTeamSpawnPoints[nTeam] = pPoint;
					break;
				}
			}
		}
		if ( !m_hTeamSpawnPoints[0] || !m_hTeamSpawnPoints[1] )
		{
			m_bHaveTeamSpawnPoints = false;
			if ( !SelectTeamSpawnPoints() )
				return false;
		}
	}

	// Round cleanup invalidates raw map-entity cursors. Select the new FoF
	// point before Spawn, preserving each player's team and selected loadout.
	m_bSpawningRoundPlayers = true;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;
		if ( pPlayer->IsObserver() )
		{
			pPlayer->StopObserverMode();
			pPlayer->State_Transition( STATE_ACTIVE );
		}
		pPlayer->FinalizeFoFSpawn( true );
	}
	m_bSpawningRoundPlayers = false;
	m_bHadRound = true;
	ResetRoundPlayers();
	return true;
}

static void FoFFireModeEvent( const char *pszName )
{
	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( pszName ) : NULL;
	if ( pEvent )
		gameeventmanager->FireEvent( pEvent );
}

static int FoFCountModeTeams( bool bAliveOnly, int *pLastTeam )
{
	bool present[4] = { false, false, false, false };
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;

		const int nTeam = pPlayer->GetTeamNumber();
		if ( nTeam < 2 || nTeam > 5 )
			continue;
		if ( bAliveOnly && !pPlayer->IsAlive() )
			continue;
		present[nTeam - 2] = true;
	}

	int nCount = 0;
	int nLastTeam = 0;
	for ( int i = 0; i < ARRAYSIZE( present ); ++i )
	{
		if ( !present[i] )
			continue;
		++nCount;
		nLastTeam = i + 2;
	}
	if ( pLastTeam )
		*pLastTeam = nLastTeam;
	return nCount;
}

BEGIN_DATADESC( CDarkMode )
	DEFINE_FIELD( m_bRoundPending, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bRoundActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bRoundFinished, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLastRound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRoundStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextFreeVisionChange, FIELD_TIME ),
	DEFINE_FIELD( m_flRoundEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flRoundRestartTime, FIELD_TIME ),
	DEFINE_FIELD( m_hBattleRoyaleCap, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecBattleRoyaleCenter, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_nBattleRoyalePhase, FIELD_INTEGER ),
	DEFINE_FIELD( m_bBattleRoyaleSafeActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flBattleRoyalePreSafeEnd, FIELD_TIME ),
	DEFINE_FIELD( m_flBattleRoyaleShrinkEnd, FIELD_TIME ),
	DEFINE_FIELD( m_flBattleRoyalePostSafeEnd, FIELD_TIME ),
	DEFINE_FIELD( m_flBattleRoyaleNextTick, FIELD_TIME ),
	DEFINE_FIELD( m_flBattleRoyaleAreaPerTick, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBattleRoyaleRemainingArea, FIELD_FLOAT ),
	DEFINE_ARRAY( m_hTeamSpawnPoints, FIELD_EHANDLE, 2 ),
	DEFINE_ARRAY( m_vecTeamSpawnOrigins, FIELD_POSITION_VECTOR, 2 ),
	DEFINE_FIELD( m_bHaveTeamSpawnPoints, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSpawningRoundPlayers, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHadRound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nLastBuyTick, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextModeUpdate, FIELD_TIME ),
END_DATADESC()

CDarkMode::CDarkMode()
	: m_bRoundPending( false )
	, m_bRoundActive( false )
	, m_bRoundFinished( false )
	, m_bLastRound( false )
	, m_flRoundStartTime( 0.0f )
	, m_flNextFreeVisionChange( 0.0f )
	, m_flRoundEndTime( 0.0f )
	, m_flRoundRestartTime( 0.0f )
	, m_nBattleRoyalePhase( 0 )
	, m_bBattleRoyaleSafeActive( false )
	, m_flBattleRoyalePreSafeEnd( 0.0f )
	, m_flBattleRoyaleShrinkEnd( 0.0f )
	, m_flBattleRoyalePostSafeEnd( 0.0f )
	, m_flBattleRoyaleNextTick( 0.0f )
	, m_flBattleRoyaleAreaPerTick( 0.0f )
	, m_flBattleRoyaleRemainingArea( 0.0f )
	, m_bHaveTeamSpawnPoints( false )
	, m_bSpawningRoundPlayers( false )
	, m_bHadRound( false )
	, m_nLastBuyTick( -1 )
	, m_flNextModeUpdate( 0.0f )
{
	m_vecBattleRoyaleCenter.Init();
	m_vecTeamSpawnOrigins[0].Init();
	m_vecTeamSpawnOrigins[1].Init();
}

void CDarkMode::Spawn()
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
	{
		Warning( "============ MAP HAS NO NAV MESH, CAN'T START ELIMINATION MODE ==============\n" );
		return;
	}

	PrecacheScriptSound( "Standoff_Music" );
	PrecacheScriptSound( "BB.MostWantedWarning" );
	BaseClass::Spawn();
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_connect_fof" );
	if ( !FoFBattleRoyaleActive() )
		SelectTeamSpawnPoints();
	BeginRound();
}

void CDarkMode::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent )
		return;
	const char *pszName = pEvent->GetName();
	if ( !Q_stricmp( pszName, "player_connect_fof" ) )
		HandlePlayerConnect( pEvent );
	else if ( !Q_stricmp( pszName, "player_death" ) )
		HandlePlayerDeath( pEvent );
}

void CDarkMode::BeginRound()
{
	static ConVarRef freeVision( "fof_sv_elm_freevision", true );
	if ( freeVision.IsValid() )
		freeVision.SetValue( 0 );

	m_bRoundPending = true;
	m_bRoundActive = false;
	m_bRoundFinished = false;
	m_bLastRound = false;
	SetBuyRoundPlayerLock( false );
	m_flRoundStartTime = 0.0f;
	m_flNextFreeVisionChange = 0.0f;
	m_flRoundEndTime = 0.0f;
	m_flRoundRestartTime = 0.0f;
	m_nLastBuyTick = -1;
	ResetBattleRoyaleState( true );
	if ( FoFBattleRoyaleActive() )
		ResetRoundPlayers();
	UpdatePlayerGlowRegistration( false );
}

void CDarkMode::ModeThink()
{
	if ( gpGlobals->curtime < m_flNextModeUpdate )
		return;
	m_flNextModeUpdate = gpGlobals->curtime + 0.1f;

	if ( FoFMode() != 4 )
	{
		if ( m_flRoundStartTime > 0.0f )
		{
			SetBuyRoundPlayerLock( false );
			m_flRoundStartTime = 0.0f;
		}
		m_flNextModeUpdate = gpGlobals->curtime + 0.25f;
		return;
	}

	if ( FoFWarmupActive() )
	{
		if ( m_bRoundActive || m_bRoundFinished ||
			m_flRoundStartTime > 0.0f )
			BeginRound();
		return;
	}

	if ( m_bRoundPending )
	{
		if ( m_flRoundStartTime <= 0.0f )
			StartBuyRound();
		else if ( gpGlobals->curtime >= m_flRoundStartTime )
			ActivateRound();
		else
		{
			SetBuyRoundPlayerLock( true );
			const int nSeconds = Ceil2Int( m_flRoundStartTime - gpGlobals->curtime );
			if ( nSeconds != m_nLastBuyTick )
			{
				m_nLastBuyTick = nSeconds;
				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
					if ( FoFIsModePlayer( pPlayer ) )
						pPlayer->EmitSound( nSeconds <= 1 ? "FoF.BuyTickEnd" : "FoF.BuyTick" );
				}
			}
		}
	}

	if ( m_bRoundActive && !m_bRoundFinished )
	{
		UpdatePlayerGlowRegistration( true );
		if ( FoFBattleRoyaleActive() )
			UpdateBattleRoyaleState();
		else
		{
			UpdateFreeVision();
			CheckNormalEliminationRound();
		}
	}
	else if ( m_bRoundFinished && m_flRoundRestartTime > 0.0f &&
		gpGlobals->curtime >= m_flRoundRestartTime )
	{
		RestartRound();
	}

}

void CDarkMode::StartBuyRound()
{
	if ( !ResetRoundScene() )
		return;
	static ConVarRef noRespawn( "fof_sv_elm_norespawn", true );
	static ConVarRef forceSpectator( "fof_sv_force_spect", true );
	if ( forceSpectator.IsValid() )
	{
		if ( FoFBattleRoyaleActive() )
			forceSpectator.SetValue( 0 );
		else if ( noRespawn.IsValid() && noRespawn.GetBool() )
			forceSpectator.SetValue( 1 );
	}
	static ConVarRef buyTime( "fof_sv_elm_buytime", true );
	const float flDuration = buyTime.IsValid() ?
		MAX( 0.0f, buyTime.GetFloat() ) : 7.0f;
	m_flRoundStartTime = gpGlobals->curtime + flDuration;
	SetBuyRoundPlayerLock( true );
	static ConVarRef freeVision( "fof_sv_elm_freevision", true );
	if ( freeVision.IsValid() && !FoFBattleRoyaleActive() )
		freeVision.SetValue( 1 );
	UpdatePlayerGlowRegistration( true );

	static ConVarRef weaponMenu( "fof_sv_weaponmenu", true );
	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	const bool bShowMenu =
		( !weaponMenu.IsValid() || weaponMenu.GetBool() ) &&
		( !forceWeapons.IsValid() || !forceWeapons.GetBool() );
	if ( !bShowMenu )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) || pPlayer->IsFakeClient() )
			continue;
		engine->ClientCommand( pPlayer->edict(), "equipmenu\n" );
	}
}

void CDarkMode::SetBuyRoundPlayerLock( bool bLocked )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;

		if ( bLocked )
			pPlayer->AddFlag( FL_ATCONTROLS );
		else
			pPlayer->RemoveFlag( FL_ATCONTROLS );
	}
}

void CDarkMode::ActivateRound()
{
	static ConVarRef roundTime( "fof_sv_elm_roundtime", true );
	const float flDuration = roundTime.IsValid() ?
		MAX( 1.0f, roundTime.GetFloat() ) : 240.0f;

	m_bRoundPending = false;
	m_bRoundActive = true;
	m_bRoundFinished = false;
	SetBuyRoundPlayerLock( false );
	static ConVarRef freeVision( "fof_sv_elm_freevision", true );
	if ( freeVision.IsValid() )
		freeVision.SetValue( 0 );
	m_flNextFreeVisionChange = gpGlobals->curtime + 0.2f;
	m_flRoundEndTime = gpGlobals->curtime + flDuration;
	UpdatePlayerGlowRegistration( true );
	FoFFireModeEvent( "round_start" );

	if ( FoFBattleRoyaleActive() )
		InitializeBattleRoyaleRound();
	else
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( FoFIsModePlayer( pPlayer ) )
				SendBattleRoyaleNotice( pPlayer, "#ELM_Spawn_Notice3" );
		}
	}
}

void CDarkMode::HandlePlayerConnect( IGameEvent *pEvent )
{
	CFoF_Player *pPlayer = ToFoFPlayer(
		UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
	if ( !pPlayer )
		return;

	pPlayer->SetFoFPlayerKills( -1 );
	pPlayer->SetFoFBattleRoyaleOutlaw( false );
	if ( FoFBattleRoyaleActive() )
		pPlayer->SetFoFBattleRoyaleRoleAppearance( false );
	pPlayer->ResetFoFPlayerTargets( 1 );
}

void CDarkMode::HandlePlayerDeath( IGameEvent *pEvent )
{
	if ( !m_bRoundActive || m_bRoundFinished ||
		FoFWarmupActive() )
	{
		return;
	}

	CFoF_Player *pVictim = ToFoFPlayer(
		UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
	if ( !FoFIsModePlayer( pVictim ) )
		return;

	if ( !FoFBattleRoyaleActive() )
	{
		CFoF_Player *pAttacker = ToFoFPlayer(
			UTIL_PlayerByUserId( pEvent->GetInt( "attacker" ) ) );
		const bool bTeamKill = FoFModeUsesTeamplay() && pAttacker &&
			pAttacker->GetTeamNumber() == pVictim->GetTeamNumber();
		const int nVictimIndex = pVictim->entindex();
		if ( FoFIsModePlayer( pAttacker ) && pAttacker != pVictim &&
			!bTeamKill && pAttacker->GetFoFPlayerKills() >= 0 &&
			nVictimIndex > 0 && nVictimIndex < 25 &&
			pAttacker->m_nPlTarget[nVictimIndex] == 1 )
		{
			pAttacker->IncrementFoFPlayerKills();
			pAttacker->AddFoFNotoriety( 5, 12, 0, "#ELM_Survivor_Kill" );
		}
		if ( FoFIsModePlayer( pVictim ) && FoFIsModePlayer( pAttacker ) &&
			pAttacker != pVictim && pAttacker->GetFoFPlayerKills() < 0 &&
			FoFEliminationProtectsSurvivor( pAttacker ) )
		{
			pAttacker->AddFoFNotoriety(
				3, 12, 0, "#ELM_Survivor_Protect" );
		}
		if ( ( !pAttacker || pAttacker == pVictim ) &&
			nVictimIndex > 0 && nVictimIndex < 25 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
				if ( FoFIsModePlayer( pPlayer ) && pPlayer != pVictim )
					pPlayer->m_nPlTarget.Set( nVictimIndex, 0 );
			}
		}
	}

	pVictim->SetFoFPlayerKills( -1 );
#ifdef GLOWS_ENABLE
	if ( pVictim->IsGlowEffectActive() )
		pVictim->RemoveGlowEffect();
#endif
	m_flNextModeUpdate = gpGlobals->curtime;
}

void CDarkMode::ResetRoundPlayers()
{
	const bool bBattleRoyale = FoFBattleRoyaleActive();
	const int nInitialState = bBattleRoyale ? -1 : 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;

		pPlayer->SetFoFPlayerKills( nInitialState );
		if ( bBattleRoyale )
		{
			pPlayer->SetFoFBattleRoyaleOutlaw( false );
			pPlayer->SetFoFBattleRoyaleRoleAppearance( false );
		}
		pPlayer->ResetFoFPlayerTargets( 1 );
	}
}

void CDarkMode::UpdatePlayerGlowRegistration( bool bEnable )
{
#ifdef GLOWS_ENABLE
	const bool bBattleRoyale = FoFBattleRoyaleActive();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		const bool bShouldGlow = bEnable && FoFIsModePlayer( pPlayer ) &&
			pPlayer->IsAlive() &&
			( bBattleRoyale || pPlayer->GetFoFPlayerKills() >= 0 );
		if ( bShouldGlow )
		{
			if ( !pPlayer->IsGlowEffectActive() )
				pPlayer->AddGlowEffect();
		}
		else if ( pPlayer->IsGlowEffectActive() )
		{
			pPlayer->RemoveGlowEffect();
		}
	}
#else
	NOTE_UNUSED( bEnable );
#endif
}

void CDarkMode::UpdateFreeVision()
{
	if ( gpGlobals->curtime < m_flNextFreeVisionChange )
		return;

	static ConVarRef freeVision( "fof_sv_elm_freevision", true );
	static ConVarRef extraTime( "fof_sv_elm_extratime", true );
	const float flInterval = extraTime.IsValid() ?
		MAX( 1.0f, extraTime.GetFloat() ) : 45.0f;
	const bool bVisible = freeVision.IsValid() && freeVision.GetBool();

	if ( !bVisible )
	{
		if ( freeVision.IsValid() )
			freeVision.SetValue( 1 );
		m_flNextFreeVisionChange = gpGlobals->curtime + 5.0f;

		IGameEvent *pEvent = gameeventmanager ?
			gameeventmanager->CreateEvent( "elm_extra_time" ) : NULL;
		if ( pEvent )
		{
			pEvent->SetInt( "extra_time",
				MAX( 1, RoundFloatToInt( flInterval + 5.0f ) ) );
			gameeventmanager->FireEvent( pEvent );
		}
	}
	else
	{
		if ( freeVision.IsValid() )
			freeVision.SetValue( 0 );
		m_flNextFreeVisionChange = gpGlobals->curtime + flInterval;
	}
}

void CDarkMode::CheckNormalEliminationRound()
{
	if ( FoFModeUsesTeamplay() )
	{
		bool bTeamPresent[2] = { false, false };
		bool bCandidateWon[2] = { true, true };
		int nSurvivors[2] = { 0, 0 };

		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !FoFIsModePlayer( pPlayer ) )
				continue;

			const int nTeam = pPlayer->GetTeamNumber();
			if ( nTeam != TEAM_COMBINE && nTeam != TEAM_REBELS )
				continue;

			const int nSlot = nTeam - TEAM_COMBINE;
			bTeamPresent[nSlot] = true;
			if ( pPlayer->GetFoFPlayerKills() >= 0 )
			{
				++nSurvivors[nSlot];
				if ( pPlayer->IsAlive() )
					bCandidateWon[1 - nSlot] = false;
			}
		}

		if ( !bTeamPresent[0] || !bTeamPresent[1] )
			return;

		if ( bCandidateWon[0] && bCandidateWon[1] )
			FinishRound( 0, 0 );
		else if ( bCandidateWon[0] )
			FinishRound( TEAM_COMBINE, 0 );
		else if ( bCandidateWon[1] )
			FinishRound( TEAM_REBELS, 0 );
		else if ( gpGlobals->curtime >= m_flRoundEndTime )
		{
			if ( nSurvivors[0] > nSurvivors[1] )
				FinishRound( TEAM_COMBINE, 0 );
			else if ( nSurvivors[1] > nSurvivors[0] )
				FinishRound( TEAM_REBELS, 0 );
			else
				FinishRound( 0, 0 );
		}
		return;
	}

	int nParticipants = 0;
	int nAlivePlayers = 0;
	int nLastAlivePlayer = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;
		++nParticipants;
		if ( pPlayer->IsAlive() )
		{
			++nAlivePlayers;
			nLastAlivePlayer = pPlayer->entindex();
		}
	}

	if ( nParticipants < 2 )
		return;
	if ( nAlivePlayers == 1 )
		FinishRound( 0, nLastAlivePlayer );
	else if ( nAlivePlayers == 0 || gpGlobals->curtime >= m_flRoundEndTime )
		FinishRound( 0, 0 );
}

void CDarkMode::CheckBattleRoyaleRound()
{
	if ( FoFBattleRoyaleFFA() || !FoFModeUsesTeamplay() )
	{
		int nOutlaws = 0;
		int nLastOutlaw = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !FoFIsModePlayer( pPlayer ) || !pPlayer->IsAlive() ||
				!pPlayer->IsFoFBattleRoyaleOutlaw() )
				continue;
			pPlayer->SetFoFPlayerKills( 0 );
			++nOutlaws;
			nLastOutlaw = pPlayer->entindex();
		}
		if ( gpGlobals->curtime >= m_flBattleRoyalePostSafeEnd ||
			nOutlaws == 0 )
		{
			FinishRound( 0, 0 );
		}
		else if ( nOutlaws == 1 )
		{
			FinishRound( 0, nLastOutlaw );
		}
		return;
	}

	bool bOutlawTeam[4] = { false, false, false, false };
	int nOutlaws = 0;
	int nWinningTeam = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) || !pPlayer->IsAlive() ||
			!pPlayer->IsFoFBattleRoyaleOutlaw() )
		{
			continue;
		}

		pPlayer->SetFoFPlayerKills( 0 );
		++nOutlaws;
		const int nTeam = pPlayer->GetTeamNumber();
		if ( nTeam >= 2 && nTeam <= 5 )
			bOutlawTeam[nTeam - 2] = true;
	}

	int nOutlawTeams = 0;
	for ( int i = 0; i < ARRAYSIZE( bOutlawTeam ); ++i )
	{
		if ( bOutlawTeam[i] )
		{
			++nOutlawTeams;
			nWinningTeam = i + 2;
		}
	}

	if ( gpGlobals->curtime >= m_flBattleRoyalePostSafeEnd ||
		nOutlaws == 0 )
	{
		FinishRound( 0, 0 );
	}
	else if ( nOutlawTeams == 1 )
		FinishRound( nWinningTeam, 0 );
}

bool CDarkMode::CanAcquireBattleRoyaleOutlawRole() const
{
	return FoFBattleRoyaleActive() && m_bRoundActive &&
		!m_bRoundFinished && m_nBattleRoyalePhase <= 1;
}

void CDarkMode::FinishRound( int nWinningTeam, int nWinningPlayer )
{
	if ( m_bRoundFinished )
		return;

	m_bRoundPending = false;
	m_bRoundFinished = true;
	m_bRoundActive = false;
	m_flRoundRestartTime = gpGlobals->curtime + 7.0f;

	if ( FoFBattleRoyaleActive() )
	{
		UpdatePlayerGlowRegistration( false );
		ResetBattleRoyaleState( true );
	}
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;
		pPlayer->AddFlag( FL_FROZEN );
		pPlayer->m_nButtons = 0;
		if ( !FoFBattleRoyaleActive() && pPlayer->GetFoFPlayerKills() >= 0 )
			pPlayer->AddFoFNotoriety( 25, 13, 0, "#ELM_Survived_Round" );
	}

	if ( FoFModeUsesTeamplay() &&
		( nWinningTeam == TEAM_COMBINE || nWinningTeam == TEAM_REBELS ) )
	{
		CTeam *pWinner = GetGlobalTeam( nWinningTeam );
		if ( pWinner )
			pWinner->AddScore( 1 );
	}

	static ConVarRef roundsPlayed( "fof_sv_roundsplayed", true );
	const int nRoundsPlayed = roundsPlayed.IsValid() ?
		roundsPlayed.GetInt() + 1 : 1;
	if ( roundsPlayed.IsValid() )
		roundsPlayed.SetValue( nRoundsPlayed );

	static ConVarRef maxRounds( "fof_sv_maxrounds", true );
	static ConVarRef winLimit( "fof_sv_winlimit", true );
	m_bLastRound = maxRounds.IsValid() && maxRounds.GetInt() > 0 &&
		nRoundsPlayed >= maxRounds.GetInt();
	if ( !m_bLastRound && HL2MPRules() &&
		HL2MPRules()->GetFoFTimeLimitMinutes() > 0.0f &&
		HL2MPRules()->GetMapRemainingTime() <= 0.0f )
	{
		m_bLastRound = true;
	}
	if ( !m_bLastRound && FoFModeUsesTeamplay() &&
		winLimit.IsValid() && winLimit.GetInt() > 0 )
	{
		CTeam *pWinner = GetGlobalTeam( nWinningTeam );
		m_bLastRound = pWinner &&
			pWinner->GetScore() >= winLimit.GetInt();
	}

	int nMVP = 0;
	int nMVPScore = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !FoFIsModePlayer( pPlayer ) )
			continue;
		if ( pPlayer->GetFoFLastRoundNotoriety() > nMVPScore )
		{
			nMVP = pPlayer->entindex();
			nMVPScore = pPlayer->GetFoFLastRoundNotoriety();
		}
	}

	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "round_end" ) : NULL;
	if ( pEvent )
	{
		pEvent->SetInt( "LScr", 0 );
		pEvent->SetInt( "OScr", 0 );
		pEvent->SetBool( "LRnd", m_bLastRound );
		pEvent->SetInt( "TWinner", FoFModeUsesTeamplay() ?
			nWinningTeam : nWinningPlayer );
		pEvent->SetInt( "MVP_Index", nMVP );
		pEvent->SetInt( "MVP_Score", nMVPScore );
		gameeventmanager->FireEvent( pEvent );
	}
}

void CDarkMode::RestartRound()
{
	m_flRoundRestartTime = 0.0f;
	CHL2MPRules *pRules = HL2MPRules();
	if ( !pRules )
		return;

	if ( m_bLastRound )
		pRules->GoToIntermission();
	else
		BeginRound();
}
