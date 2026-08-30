//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Break Bad mode authority.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_breakbad_mode.h"

#include "fof/fof_breakbad_entities.h"
#include "fof/fof_crate.h"
#include "fof/fof_player.h"
#include "hl2mp_gamerules.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "recipientfilter.h"
#include "team.h"
#include "util.h"

#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_breakbad, CBreakBad );

static int FoFMode();

static const int FOF_PLAYERINFO_UNARMED = 0x800;
static const int FOF_PLAYERINFO_CARRYING_LOOT = 0x2000;
static const int FOF_PLAYERINFO_SELF_DISARMED = 0x4000;

static CUtlVector< Vector >
	s_FoFBreakBadRecentLootDrops[FOF_BREAKBAD_PLAYER_STATE_COUNT];
static CUtlVector< Vector > *FoFBreakBadRecentLootDrops(
	CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return NULL;

	const int nIndex = pPlayer->entindex();
	if ( nIndex < 0 || nIndex >= ARRAYSIZE( s_FoFBreakBadRecentLootDrops ) )
		return NULL;

	return &s_FoFBreakBadRecentLootDrops[nIndex];
}

static void FoFClearBreakBadRecentLootDrops( CFoF_Player *pPlayer )
{
	CUtlVector< Vector > *pPositions = FoFBreakBadRecentLootDrops( pPlayer );
	if ( pPositions )
		pPositions->RemoveAll();
}

static bool FoFIsRecentBreakBadLootDrop(
	CFoF_Player *pPlayer, const Vector &vecPosition )
{
	CUtlVector< Vector > *pPositions = FoFBreakBadRecentLootDrops( pPlayer );
	if ( !pPositions )
		return false;

	FOR_EACH_VEC( ( *pPositions ), i )
	{
		if ( ( *pPositions )[i].DistTo( vecPosition ) < 50.0f )
			return true;
	}

	return false;
}

static void FoFRememberBreakBadLootDrop(
	CFoF_Player *pPlayer, const Vector &vecPosition )
{
	CUtlVector< Vector > *pPositions = FoFBreakBadRecentLootDrops( pPlayer );
	if ( !pPositions )
		return;

	pPositions->AddToTail( vecPosition );
	if ( pPositions->Count() > 10 )
		pPositions->Remove( 0 );
}

static CBBMulti *FoFFindOrCreateBreakBadZone( const char *pszClassname )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, pszClassname ) ) != NULL )
	{
		CBBMulti *pZone = dynamic_cast< CBBMulti * >( pEntity );
		if ( pZone && !pZone->GetOwnerEntity() )
			return pZone;
	}

	pEntity = CreateEntityByName( pszClassname );
	CBBMulti *pZone = dynamic_cast< CBBMulti * >( pEntity );
	if ( !pZone )
	{
		if ( pEntity )
			UTIL_Remove( pEntity );
		return NULL;
	}

	DispatchSpawn( pZone );
	pZone->Activate();
	return pZone;
}

class FoFBreakBadHidingSpotCollector
{
public:
	FoFBreakBadHidingSpotCollector(
		const Vector &vecAnchor, float flRadius )
		: m_vecAnchor( vecAnchor )
		, m_flRadiusSqr( flRadius * flRadius )
		, m_nTotalWeight( 0 )
	{
	}

	bool operator()( CNavArea *pArea )
	{
		if ( !pArea || pArea->HasAttributes( NAV_MESH_DONT_HIDE ) )
			return m_Positions.Count() < 256;

		const HidingSpotVector *pSpots = pArea->GetHidingSpots();
		if ( !pSpots )
			return m_Positions.Count() < 256;

		FOR_EACH_VEC( ( *pSpots ), i )
		{
			const HidingSpot *pSpot = ( *pSpots )[i];
			if ( !pSpot || !( pSpot->GetFlags() & HidingSpot::IN_COVER ) ||
				pSpot->GetPosition().DistToSqr( m_vecAnchor ) > m_flRadiusSqr )
			{
				continue;
			}

			m_Positions.AddToTail( &pSpot->GetPosition() );
			m_nTotalWeight += pArea->HasAttributes( NAV_MESH_AVOID ) ? 1 : 2;
			m_CumulativeWeights.AddToTail( m_nTotalWeight );
			if ( m_Positions.Count() >= 256 )
				return false;
		}

		return true;
	}

	bool SelectPosition( Vector &vecPosition ) const
	{
		if ( m_Positions.Count() <= 0 || m_nTotalWeight <= 0 )
			return false;

		const int nTicket = RandomInt( 0, m_nTotalWeight - 1 );
		for ( int i = 0; i < m_CumulativeWeights.Count(); ++i )
		{
			if ( nTicket < m_CumulativeWeights[i] )
			{
				vecPosition = *m_Positions[i];
				return true;
			}
		}

		vecPosition = *m_Positions.Tail();
		return true;
	}

private:
	Vector m_vecAnchor;
	float m_flRadiusSqr;
	CUtlVector< const Vector * > m_Positions;
	CUtlVector< int > m_CumulativeWeights;
	int m_nTotalWeight;
};

static bool FoFSelectBreakBadHidingSpot(
	const Vector &vecAnchor, Vector &vecPosition )
{
	if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
		return false;

	CNavArea *pAnchorArea = TheNavMesh->GetNearestNavArea(
		vecAnchor, false, 10000.0f, false, true, TEAM_ANY );
	if ( !pAnchorArea )
		return false;

	const float flRadius = static_cast< float >( RandomInt( 200, 300 ) );
	FoFBreakBadHidingSpotCollector collector( vecAnchor, flRadius );
	SearchSurroundingAreas(
		pAnchorArea, vecAnchor, collector, flRadius, 0, TEAM_ANY );
	return collector.SelectPosition( vecPosition );
}
static int FoFMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}
static bool FoFModeUsesTeamplay()
{
	return HL2MPRules() && HL2MPRules()->IsTeamplay();
}

static float FoFBreakBadKillCashMultiplier(
	CFoF_Player *pPlayer, float flRateEndpoint )
{
	if ( !pPlayer )
		return 0.0f;

	const float flExperienceFactor = pPlayer->IsBot() ? 0.05f :
		RemapValClamped(
			static_cast< float >( pPlayer->GetFoFTotalNotoriety() ),
			10.0f, 200.0f, 0.01f, 0.25f );
	const int nExperience = clamp(
		pPlayer->GetFoFExperience(), 1, 1000 );
	const float flMinutesAlive = clamp(
		( gpGlobals->curtime - pPlayer->GetFoFLifeStartTime() ) / 60.0f,
		3.0f, 1000.0f );
	const float flExperienceRate =
		static_cast< float >( nExperience ) / flMinutesAlive;
	const float flRateFactor = RemapValClamped(
		flExperienceRate, 1.0f, flRateEndpoint, 0.01f, 0.25f );
	const float flCashFactor = RemapValClamped(
		pPlayer->GetFoFCash(), 0.0f, 175.0f, 0.01f, 0.5f );
	return flExperienceFactor + flRateFactor + flCashFactor;
}

static void FoFSendBreakBadNotice(
	CFoF_Player *pPlayer, const char *pszToken )
{
	if ( !pPlayer || !pszToken || !pszToken[0] )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
	MessageEnd();
}

static void FoFSendBreakBadPublicEnemyMarker(
	CFoF_Player *pPublicEnemy )
{
	if ( !pPublicEnemy )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pRecipient =
			ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pRecipient || pRecipient->GetTeamNumber() == TEAM_SPECTATOR ||
			pRecipient->IsBot() || pRecipient == pPublicEnemy )
		{
			continue;
		}
		if ( FoFModeUsesTeamplay() &&
			pRecipient->GetTeamNumber() == pPublicEnemy->GetTeamNumber() )
		{
			continue;
		}

		if ( ( pRecipient->GetAbsOrigin() -
			pPublicEnemy->GetAbsOrigin() ).Length() < 2500.0f )
		{
			pRecipient->EmitSound( "BB.MostWantedWarning" );
		}

		CSingleUserRecipientFilter filter( pRecipient );
		filter.MakeReliable();
		UserMessageBegin( filter, "HUDBBMulti" );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 7 );
			WRITE_BYTE( 5 );
			WRITE_VEC3COORD( pPublicEnemy->GetAbsOrigin() );
			WRITE_STRING( pPublicEnemy->GetPlayerName() );
		MessageEnd();
	}
}

static CBreakBad *FoFGetBreakBadController()
{
	CBaseEntity *pEntity =
		gEntList.FindEntityByClassname( NULL, "fof_breakbad" );
	return pEntity ? static_cast< CBreakBad * >( pEntity ) : NULL;
}

void FoFUpdateBreakBadMode()
{
	if ( FoFMode() != 3 )
		return;

	CBreakBad *pController = FoFGetBreakBadController();
	if ( pController )
		pController->ModeThink();
}

void FoFBreakBadPlayerSpawn( CFoF_Player *pPlayer )
{
	if ( FoFMode() != 3 )
		return;
	CBreakBad *pController = FoFGetBreakBadController();
	if ( pController )
		pController->OnPlayerSpawn( pPlayer );
}

void FoFBreakBadPlayerDamaged(
	CFoF_Player *pVictim, const CTakeDamageInfo &info )
{
	if ( FoFMode() != 3 )
		return;
	CBreakBad *pController = FoFGetBreakBadController();
	if ( pController )
		pController->OnPlayerDamaged( pVictim, info );
}

void FoFBreakBadPlayerKilled(
	CFoF_Player *pVictim, const CTakeDamageInfo &info )
{
	if ( FoFMode() != 3 )
		return;
	CBreakBad *pController = FoFGetBreakBadController();
	if ( pController )
		pController->OnPlayerKilled( pVictim, info );
}

BEGIN_DATADESC( CBreakBad )
END_DATADESC()

CBreakBad::CBreakBad()
{
}

void CBreakBad::Spawn()
{
	BaseClass::Spawn();
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_connect_fof" );
	ResetAllPlayerStates();

	if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
	{
		Warning(
			"============ MAP HAS NO NAV MESH, CAN'T START BREAK BAD MODE ==============\n" );
		return;
	}

	static const char *s_CrateClasses[] =
	{
		"fof_crate_low",
		"fof_crate_med",
		"fof_crate"
	};
	for ( int nTier = 3; nTier >= 1; --nTier )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByClassname(
			pEntity, s_CrateClasses[nTier - 1] ) ) != NULL )
		{
			static_cast< FoF_Crate * >( pEntity )->
				DisableForBreakBadRound();
		}
	}

	m_nNextZonePlayer = 0;
}

void CBreakBad::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent )
		return;
	const char *pszName = pEvent->GetName();
	if ( !Q_stricmp( pszName, "round_start" ) )
	{
		InitializeRoundEntities();
	}
	else if ( !Q_stricmp( pszName, "player_disconnect" ) ||
		!Q_stricmp( pszName, "player_connect_fof" ) )
	{
		CFoF_Player *pPlayer = ToFoFPlayer(
			UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
		if ( pPlayer )
			ResetPlayerState( pPlayer );
	}
}

void CBreakBad::ResetAllPlayerStates()
{
	const float flCurrentTime = gpGlobals ? gpGlobals->curtime : 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_PlayerStates ); ++i )
	{
		s_FoFBreakBadRecentLootDrops[i].RemoveAll();
		FoFBreakBadPlayerState_t &state = m_PlayerStates[i];
		state.m_flLastSpawnTime = -1.0f;
		state.m_flDamageFinePool = 0.0f;
		state.m_flReservedState0 = 0.0f;
		state.m_flLastDamageFineTime = 0.0f;
		state.m_flDisarmZoneTime = 0.0f;
		state.m_flNextLootPickTime = 0.0f;
		state.m_flNextLootDropTime = 0.0f;
		state.m_flNextWhiskeyTime = flCurrentTime + 60.0f;
	}
}

FoFBreakBadPlayerState_t *CBreakBad::PlayerState( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return NULL;
	const int nIndex = pPlayer->entindex();
	if ( nIndex < 0 || nIndex >= ARRAYSIZE( m_PlayerStates ) )
		return NULL;
	return &m_PlayerStates[nIndex];
}

void CBreakBad::ResetPlayerState( CFoF_Player *pPlayer )
{
	FoFBreakBadPlayerState_t *pState = PlayerState( pPlayer );
	if ( !pState )
		return;
	pState->m_flLastSpawnTime = -1.0f;
	pState->m_flDamageFinePool = 0.0f;
	pState->m_flReservedState0 = 0.0f;
	pState->m_flLastDamageFineTime = 0.0f;
	pState->m_flDisarmZoneTime = 0.0f;
	pState->m_flNextLootPickTime =
		CalculateNextLootPickTime( pPlayer->GetTeamNumber() );
	pState->m_flNextLootDropTime = 0.0f;
	pState->m_flNextWhiskeyTime = gpGlobals->curtime + 60.0f;
	FoFClearBreakBadRecentLootDrops( pPlayer );
}

void CBreakBad::InitializeRoundEntities()
{
	static const char *s_CrateClasses[] =
	{
		"fof_crate_low",
		"fof_crate_med",
		"fof_crate"
	};
	for ( int nTier = 3; nTier >= 1; --nTier )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByClassname(
			pEntity, s_CrateClasses[nTier - 1] ) ) != NULL )
		{
			FoF_Crate *pCrate = static_cast< FoF_Crate * >( pEntity );
			if ( !pCrate->GetMoveParent() )
			{
				FoFBreakBadLocation_t location;
				location.m_vecOrigin = pCrate->GetAbsOrigin();
				location.m_nTier = nTier;
				m_RoundLocations.AddToTail( location );
			}

			if ( nTier > 1 )
			{
				const char *pszSaleClass = nTier == 2 ?
					"fof_bb_buyzone_sale1" : "fof_bb_buyzone_sale2";
				CBaseEntity *pSale = CreateEntityByName( pszSaleClass );
				if ( pSale )
				{
					pSale->SetAbsOrigin( pCrate->GetAbsOrigin() );
					pSale->SetAbsAngles( pCrate->GetAbsAngles() );
					DispatchSpawn( pSale );
					pSale->SetOwnerEntity( NULL );
					if ( pCrate->GetMoveParent() )
						pSale->SetParent( pCrate->GetMoveParent(), -1 );
				}
			}

			pCrate->DisableForBreakBadRound();
		}
	}

	CBaseEntity *pSpawn = NULL;
	while ( ( pSpawn = gEntList.FindEntityByClassname(
		pSpawn, "info_player_fof" ) ) != NULL )
	{
		FoFBreakBadLocation_t location;
		location.m_vecOrigin = pSpawn->GetAbsOrigin();
		location.m_nTier = 0;
		m_RoundLocations.AddToTail( location );
	}

	for ( int i = 1; i < ARRAYSIZE( m_PlayerStates ); ++i )
		m_PlayerStates[i].m_flLastSpawnTime = 0.0f;
}

void CBreakBad::OnPlayerSpawn( CFoF_Player *pPlayer )
{
	FoFBreakBadPlayerState_t *pState = PlayerState( pPlayer );
	if ( !pState || !pPlayer->IsAlive() )
		return;
	FoFClearBreakBadRecentLootDrops( pPlayer );

	const float flElapsed = gpGlobals->curtime - pState->m_flLastSpawnTime;
	if ( pPlayer->DeathCount() == 0 && pPlayer->GetFoFCash() == 0.0f )
	{
		pPlayer->AwardFoFCash( 50.0f, "#Cash_Added_Spawn" );
	}
	else if ( pPlayer->GetFoFCash() < 100.0f )
	{
		const float flReward = RemapValClamped(
			flElapsed, 5.0f, 30.0f, 0.0f, 30.0f );
		if ( flReward > 0.0f )
			pPlayer->AwardFoFCash( flReward, "#Cash_Added_Spawn" );
	}

	pPlayer->SetFoFUnarmedTime( gpGlobals->curtime + 30.0f );
	pState->m_flLastSpawnTime = gpGlobals->curtime;
	pState->m_flDisarmZoneTime = gpGlobals->curtime;
	pPlayer->SetMaxSpeed( 120.0f );
	if ( pPlayer->IsBot() )
		pPlayer->RefreshFoFEquipment();
}

void CBreakBad::OnPlayerDamaged(
	CFoF_Player *pVictim, const CTakeDamageInfo &info )
{
	CFoF_Player *pAttacker = ToFoFPlayer( info.GetAttacker() );
	if ( !pVictim || !pAttacker || pAttacker == pVictim )
		return;

	const int nVictimInfo = pVictim->GetFoFPlayerInfo();
	if ( !( nVictimInfo & FOF_PLAYERINFO_UNARMED ) &&
		!( nVictimInfo & FOF_PLAYERINFO_SELF_DISARMED ) )
	{
		return;
	}

	FoFBreakBadPlayerState_t *pState = PlayerState( pAttacker );
	if ( !pState || info.GetDamage() <= 0.0f )
		return;

	pState->m_flDamageFinePool += info.GetDamage();
	float flFine = clamp(
		pState->m_flDamageFinePool * 0.005f, 0.0f, 1.0f ) *
		75.0f + 25.0f;
	if ( nVictimInfo & FOF_PLAYERINFO_SELF_DISARMED )
	{
		flFine *= 1.25f;
		pVictim->AwardFoFCash(
			flFine * 0.35f, "#Cash_Added_SelfDisarmed" );
	}

	pAttacker->FineFoFCash(
		static_cast< int >( flFine ), "#Cash_Fine_Unarmed" );
	pState->m_flLastDamageFineTime = gpGlobals->curtime;
	if ( pAttacker->GetFoFCash() == 0.0f )
		JailPlayer( pAttacker );
}

void CBreakBad::OnPlayerKilled(
	CFoF_Player *pVictim, const CTakeDamageInfo &info )
{
	CFoF_Player *pScorer = ToFoFPlayer( info.GetAttacker() );
	if ( !pVictim || !pScorer || pScorer == pVictim )
		return;

	const int nVictimInfo = pVictim->GetFoFPlayerInfo();
	if ( ( nVictimInfo & FOF_PLAYERINFO_UNARMED ) &&
		!( nVictimInfo & FOF_PLAYERINFO_SELF_DISARMED ) )
	{
		return;
	}
	if ( nVictimInfo & FOF_PLAYERINFO_SELF_DISARMED )
	{
		const int nCash = static_cast< int >( pScorer->GetFoFCash() );
		pScorer->FineFoFCash( nCash, "#Cash_Fine_Unarmed" );
		JailPlayer( pScorer );
		FoFSendBreakBadNotice(
			pScorer, "#bb_selfdisarm_kill_warning" );
		return;
	}

	const float flMultiplier =
		FoFBreakBadKillCashMultiplier( pScorer, 50.0f );
	const int nMultiKill = pScorer->GetFoFMultiKill();
	int nCashReward;
	if ( nMultiKill < 4 )
	{
		nCashReward = RoundFloatToInt( RemapValClamped(
			flMultiplier, 0.25f, 1.0f, 1.0f, 10.0f ) );
	}
	else
	{
		if ( !pScorer->IsFoFPublicEnemy() )
		{
			pScorer->SetFoFPublicEnemy( true );
			FoFSendBreakBadNotice(
				pScorer, "#bb_public_enemy_warning" );
			if ( nMultiKill == 4 || nMultiKill == 9 || nMultiKill == 14 )
				FoFSendBreakBadPublicEnemyMarker( pScorer );
		}

		const float flMultiKillBonus = RemapValClamped(
			static_cast< float >( nMultiKill - 4 ),
			0.0f, 10.0f, 1.0f, 5.0f );
		const int nBaseReward = RoundFloatToInt( RemapValClamped(
			flMultiplier, 0.1f, 1.0f, 2.0f, 10.0f ) );
		nCashReward = RoundFloatToInt(
			static_cast< float >( nBaseReward ) + flMultiKillBonus );
	}

	if ( nVictimInfo & FOF_PLAYERINFO_CARRYING_LOOT )
		nCashReward += 10;
	if ( nVictimInfo & 0x8000 )
	{
		nCashReward = RoundFloatToInt(
			static_cast< float >( nCashReward ) + RemapValClamped(
				static_cast< float >( nMultiKill ),
				4.0f, 15.0f, 5.0f, 10.0f ) );
	}
	if ( pScorer->GetFoFCash() < 50.0f && flMultiplier < 0.25f )
		nCashReward *= 2;

	pScorer->AwardFoFCash(
		static_cast< float >( nCashReward ), "#Cash_Added_Kill" );
}

void CBreakBad::ModeThink()
{
	UpdateTeamCash();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR ||
			!pPlayer->IsAlive() )
		{
			continue;
		}

		FoFBreakBadPlayerState_t &state = m_PlayerStates[i];
		CBaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
		if ( !pActiveWeapon && pPlayer->GetFoFJailTime() <= gpGlobals->curtime )
		{
			if ( pPlayer->GetFoFPlayerInfo() &
				FOF_PLAYERINFO_SELF_DISARMED )
			{
				if ( gpGlobals->curtime > pPlayer->GetFoFUnarmedTime() )
					pPlayer->RestoreFoFUnarmedLoadout();
			}
			else if ( gpGlobals->curtime >
				state.m_flLastSpawnTime + 30.0f )
			{
				pPlayer->RestoreFoFUnarmedLoadout();
			}

			pActiveWeapon = pPlayer->GetActiveWeapon();
			if ( !pActiveWeapon )
			{
				for ( int nWeapon = 0;
					nWeapon < pPlayer->WeaponCount(); ++nWeapon )
				{
					CBaseCombatWeapon *pWeapon =
						pPlayer->GetWeapon( nWeapon );
					if ( pWeapon && pPlayer->Weapon_Switch( pWeapon ) )
						break;
				}
			}
		}

		if ( state.m_flDamageFinePool > 0.0f )
		{
			const float flDecay = clamp(
				( gpGlobals->curtime -
					state.m_flLastDamageFineTime ) / 60.0f,
				0.0f, 1.0f ) * 0.9f + 0.1f;
			state.m_flDamageFinePool = clamp(
				state.m_flDamageFinePool - flDecay,
				0.0f, 1000.0f );
		}
	}

	for ( int nAttempt = 0; nAttempt < 10; ++nAttempt )
	{
		const int nPlayerIndex = m_nNextZonePlayer;
		CFoF_Player *pPlayer = ToFoFPlayer(
			UTIL_PlayerByIndex( nPlayerIndex ) );

		if ( m_nNextZonePlayer > gpGlobals->maxClients )
			m_nNextZonePlayer = 1;
		else
			++m_nNextZonePlayer;

		if ( !pPlayer || !pPlayer->IsConnected() ||
			pPlayer->GetTeamNumber() == TEAM_SPECTATOR ||
			!pPlayer->IsAlive() || pPlayer->IsObserver() )
		{
			continue;
		}

		UpdatePlayerZones( pPlayer, m_PlayerStates[nPlayerIndex] );
	}
}

#if !defined( _DEBUG ) && defined( _M_IX86 )
COMPILE_TIME_ASSERT( sizeof( CBreakBad ) == 0x704 );
#endif

float CBreakBad::GetMapDistanceScale() const
{
	if ( gpGlobals && gpGlobals->mapname != NULL_STRING &&
		!Q_strnicmp( STRING( gpGlobals->mapname ), "fofhr_", 6 ) )
	{
		return 3.0f;
	}

	// Original CHL2MPRules::m_nMapSize mapping used by all four dynamic
	// BreakBad markers: small=0.75, normal=1.0, large=1.25.
	CHL2MPRules *pRules = HL2MPRules();
	const int nMapSize = pRules ? pRules->GetFoFMapSize() : 1;
	if ( nMapSize == 0 )
		return 0.75f;
	if ( nMapSize == 2 )
		return 1.25f;
	return 1.0f;
}

float CBreakBad::GetLootDropDistanceScale(
	CFoF_Player *pPlayer ) const
{
	int nTeamPlayers = 0;
	if ( pPlayer )
	{
		CTeam *pTeam = GetGlobalTeam( pPlayer->GetTeamNumber() );
		if ( pTeam )
			nTeamPlayers = pTeam->GetNumPlayers();
	}

	const float flPlayerScale = RemapValClamped(
		static_cast< float >( nTeamPlayers ),
		0.0f, 4.0f, 0.35f, 1.0f );
	return GetMapDistanceScale() * flPlayerScale;
}

bool CBreakBad::IsZonePositionClear(
	const Vector &vecPosition ) const
{
	trace_t trace;
	const Vector vecTest = vecPosition + Vector( 0.0f, 0.0f, 2.0f );
	UTIL_TraceHull( vecTest, vecTest,
		Vector( -16.0f, -16.0f, 0.0f ),
		Vector( 16.0f, 16.0f, 40.0f ),
		0x1400B, NULL, COLLISION_GROUP_NONE, &trace );
	if ( trace.startsolid || trace.allsolid )
		return false;
	return true;
}

bool CBreakBad::FindDynamicZonePosition(
	CFoF_Player *pPlayer, float flDistanceScale,
	bool bTrackRecentLootDrop, Vector &vecPosition ) const
{
	if ( !pPlayer || !TheNavMesh || !TheNavMesh->IsLoaded() )
		return false;

	const float flMinimumDistance = flDistanceScale * 2500.0f;
	const float flMaximumDistance = flDistanceScale * 3500.0f;
	if ( flMaximumDistance <= 0.0f )
		return false;

	CNavArea *pPlayerArea = TheNavMesh->GetNearestNavArea(
		pPlayer->GetAbsOrigin(), false, 10000.0f,
		false, true, TEAM_ANY );
	if ( !pPlayerArea )
		return false;

	ShortestPathCost pathCost;
	for ( int i = 0; i < m_RoundLocations.Count(); ++i )
	{
		const FoFBreakBadLocation_t &location = m_RoundLocations[i];
		if ( location.m_nTier != 0 ||
			fabsf( location.m_vecOrigin.z -
				pPlayer->GetAbsOrigin().z ) > 250.0f )
		{
			continue;
		}
		if ( bTrackRecentLootDrop &&
			FoFIsRecentBreakBadLootDrop(
				pPlayer, location.m_vecOrigin ) )
		{
			continue;
		}
		if ( location.m_vecOrigin.DistTo(
			pPlayer->GetAbsOrigin() ) >= flMaximumDistance )
		{
			continue;
		}

		CNavArea *pLocationArea = TheNavMesh->GetNearestNavArea(
			location.m_vecOrigin, false, 10000.0f,
			false, true, TEAM_ANY );
		if ( !pLocationArea )
			continue;

		const float flTravelDistance = NavAreaTravelDistance(
			pPlayerArea, pLocationArea, pathCost,
			flMaximumDistance );
		if ( flTravelDistance <= flMinimumDistance ||
			flTravelDistance >= flMaximumDistance )
		{
			continue;
		}

		vecPosition = location.m_vecOrigin;
		if ( bTrackRecentLootDrop )
			FoFRememberBreakBadLootDrop( pPlayer, vecPosition );
		return true;
	}

	Vector vecCandidate;
	if ( !FoFSelectBreakBadHidingSpot(
		pPlayer->GetAbsOrigin(), vecCandidate ) ||
		!IsZonePositionClear( vecCandidate ) )
	{
		return false;
	}

	vecPosition = vecCandidate;
	return true;
}

CBBMulti *CBreakBad::CreatePlayerZone(
	const char *pszClassname, CFoF_Player *pPlayer,
	float flDistanceScale, bool bTrackRecentLootDrop )
{
	Vector vecPosition;
	if ( !FindDynamicZonePosition(
		pPlayer, flDistanceScale,
		bTrackRecentLootDrop, vecPosition ) )
	{
		return NULL;
	}

	CBBMulti *pZone = FoFFindOrCreateBreakBadZone( pszClassname );
	if ( !pZone )
		return NULL;

	pZone->SetAbsOrigin( vecPosition );
	pZone->SetOwnerEntity( pPlayer );
	return pZone;
}

void CBreakBad::UpdatePlayerZones(
	CFoF_Player *pPlayer, FoFBreakBadPlayerState_t &state )
{
	if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->IsObserver() )
	{
		return;
	}

	const float flCurrentTime = gpGlobals->curtime;
	if ( pPlayer->GetFoFJailTime() > flCurrentTime )
		return;

	CBaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
	if ( !pActiveWeapon )
		return;

	const bool bCarryingLoot =
		( pPlayer->GetFoFPlayerInfo() &
			FOF_PLAYERINFO_CARRYING_LOOT ) != 0;
	if ( !bCarryingLoot &&
		flCurrentTime > state.m_flNextLootPickTime )
	{
		if ( CreatePlayerZone(
			"fof_bb_lootpickzone", pPlayer,
			GetMapDistanceScale() * 0.1f, false ) )
		{
			state.m_flNextLootPickTime =
				CalculateNextLootPickTime( pPlayer->GetTeamNumber() );
			state.m_flNextWhiskeyTime += 16.0f;
		}
	}
	else if ( bCarryingLoot &&
		flCurrentTime > state.m_flNextLootDropTime )
	{
		CBBLootDropZone *pDrop = dynamic_cast< CBBLootDropZone * >(
			CreatePlayerZone( "fof_bb_lootdropzone", pPlayer,
				GetLootDropDistanceScale( pPlayer ), true ) );
		if ( pDrop )
		{
			const int nCashValue = RoundFloatToInt( RemapValClamped(
				pDrop->GetAbsOrigin().z,
				2000.0f, 3500.0f, 37.5f, 62.5f ) );
			pDrop->SetCashValue( nCashValue );
			pDrop->SetTeamSkin( pPlayer->GetTeamNumber() );
			state.m_flNextLootDropTime = flCurrentTime + 75.0f;
		}
	}

	if ( flCurrentTime > state.m_flDisarmZoneTime + 60.0f )
	{
		if ( CreatePlayerZone(
			"fof_bb_disarmzone", pPlayer,
			GetMapDistanceScale() * 0.6f, false ) )
		{
			state.m_flDisarmZoneTime = flCurrentTime + 20.0f;
		}
	}

	if ( flCurrentTime > state.m_flNextWhiskeyTime )
	{
		if ( CreatePlayerZone(
			"fof_bb_whiskeyzone", pPlayer,
			GetMapDistanceScale() * 0.4f, false ) )
		{
			state.m_flNextWhiskeyTime = flCurrentTime + 140.0f;
		}
	}
}

float CBreakBad::CalculateNextLootPickTime( int nTeam ) const
{
	if ( !FoFModeUsesTeamplay() )
		return gpGlobals->curtime + 75.0f;

	int nTotalCash = 0;
	int nTeamCash = 0;
	for ( int i = 2; i <= 5; ++i )
	{
		CTeam *pTeam = GetGlobalTeam( i );
		const int nCash = pTeam ? pTeam->GetScore() : 0;
		nTotalCash += nCash;
		if ( i == nTeam )
			nTeamCash = nCash;
	}

	nTotalCash = clamp( nTotalCash, 100, 10000 );
	const float flTeamRatio =
		static_cast< float >( nTeamCash ) /
		static_cast< float >( nTotalCash );
	const float flDelayFactor = clamp(
		( flTeamRatio - 0.1f ) * 3.3333333f, 0.0f, 1.0f );
	return gpGlobals->curtime + 90.0f + flDelayFactor * 150.0f;
}

void CBreakBad::JailPlayer( CFoF_Player *pPlayer )
{
	FoFBreakBadPlayerState_t *pState = PlayerState( pPlayer );
	if ( !pState )
		return;

	pPlayer->SetFoFPublicEnemy( false );
	pPlayer->m_nPlayerInfo &= ~FOF_PLAYERINFO_CARRYING_LOOT;
	const float flJailDuration = clamp(
		pState->m_flDamageFinePool * 0.005f, 0.0f, 1.0f ) *
		40.0f + 20.0f;
	const int nReleaseTime = static_cast< int >(
		gpGlobals->curtime + flJailDuration );
	pPlayer->ForceFoFJail( static_cast< float >( nReleaseTime ) );
	pPlayer->m_nPlayerInfo &= ~FOF_PLAYERINFO_CARRYING_LOOT;
	pPlayer->SetBodygroup( 2, 0 );
}

void CBreakBad::UpdateTeamCash()
{
	int teamCash[4] = { 0, 0, 0, 0 };
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsConnected() )
			continue;
		const int nTeam = pPlayer->GetTeamNumber();
		if ( nTeam >= 2 && nTeam <= 5 )
			teamCash[nTeam - 2] += MAX( 0, (int)pPlayer->m_flFoFCash );
	}

	for ( int i = 0; i < ARRAYSIZE( teamCash ); ++i )
	{
		CTeam *pTeam = GetGlobalTeam( i + 2 );
		if ( pTeam && pTeam->GetScore() != teamCash[i] )
			pTeam->SetScore( teamCash[i] );
	}
}
