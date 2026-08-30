#include "cbase.h"
#include "fof/fof_player.h"
#include "fof/fof_player_statistics.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct FoFAccuracyState_t
{
	FoFAccuracyState_t()
		: m_nShots( 0 ), m_nHits( 0 ), m_flDamage( 0.0f )
	{
	}

	CHandle< CFoF_Player > m_hPlayer;
	int m_nShots;
	int m_nHits;
	float m_flDamage;
};

static FoFAccuracyState_t s_FoFAccuracyStates[MAX_PLAYERS + 1];

class CFoFAccuracyStateSystem : public CAutoGameSystem
{
public:
	CFoFAccuracyStateSystem()
		: CAutoGameSystem( "CFoFAccuracyStateSystem" )
	{
	}

	virtual void LevelShutdownPostEntity()
	{
		for ( int i = 0; i < ARRAYSIZE( s_FoFAccuracyStates ); ++i )
			s_FoFAccuracyStates[i] = FoFAccuracyState_t();
	}
};

static CFoFAccuracyStateSystem s_FoFAccuracyStateSystem;

static FoFAccuracyState_t *FoFAccuracyStateFor(
	const CFoF_Player *pPlayer, bool bCreate )
{
	if ( !pPlayer )
		return NULL;

	const int nPlayerIndex = pPlayer->entindex();
	if ( nPlayerIndex < 1 ||
		nPlayerIndex >= ARRAYSIZE( s_FoFAccuracyStates ) )
	{
		return NULL;
	}

	FoFAccuracyState_t &state = s_FoFAccuracyStates[nPlayerIndex];
	if ( state.m_hPlayer.Get() != pPlayer )
	{
		if ( !bCreate )
			return NULL;
		state = FoFAccuracyState_t();
		state.m_hPlayer = const_cast< CFoF_Player * >( pPlayer );
	}
	return &state;
}

void FoFRecordAccuracyShots( CFoF_Player *pPlayer, int nShots )
{
	FoFAccuracyState_t *pState = FoFAccuracyStateFor( pPlayer, true );
	if ( !pState || nShots <= 0 )
		return;
	pState->m_nShots += nShots;
}

void FoFResetAccuracyStats( CFoF_Player *pPlayer )
{
	FoFAccuracyState_t *pState = FoFAccuracyStateFor( pPlayer, true );
	if ( !pState )
		return;
	CHandle< CFoF_Player > hPlayer = pState->m_hPlayer;
	*pState = FoFAccuracyState_t();
	pState->m_hPlayer = hPlayer;
}

void FoFRecordAccuracyHit(
	CFoF_Player *pPlayer, float flDamage )
{
	FoFAccuracyState_t *pState = FoFAccuracyStateFor( pPlayer, true );
	if ( !pState || pState->m_nHits >= pState->m_nShots )
		return;
	++pState->m_nHits;
	pState->m_flDamage += MAX( flDamage, 0.0f );
}

float FoFGetAuthoritativeAccuracy( const CFoF_Player *pPlayer )
{
	FoFAccuracyState_t *pState = FoFAccuracyStateFor( pPlayer, false );
	if ( !pState || pState->m_flDamage < 750.0f )
		return 0.0f;
	const int nDenominator = MAX( pState->m_nShots, 25 );
	return nDenominator > 0 ?
		(float)pState->m_nHits / (float)nDenominator * 100.0f : 0.0f;
}

float CFoF_Player::GetFoFReportedAccuracy( void ) const
{
	return FoFGetAuthoritativeAccuracy( this );
}

static int FoFStatisticsPlayerCount( bool bHumansOnly )
{
	int nPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsConnected() &&
			( !bHumansOnly || !pPlayer->IsFakeClient() ) )
			++nPlayers;
	}
	return nPlayers;
}

static int FoFStatisticsCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static void FoFSendPersonalStatUpdate(
	CFoF_Player *pPlayer, int nStat, int nValue )
{
	EntityMessageBegin( pPlayer, false );
		WRITE_BYTE( 10 );
		WRITE_SHORT( nStat );
		WRITE_SHORT( nValue );
		WRITE_SHORT( pPlayer->entindex() );
	MessageEnd();
}

static void FoFSendPlayTimeUpdate( CFoF_Player *pPlayer )
{
	static ConVarRef warmup( "fof_warmup", true );
	if ( !pPlayer || pPlayer->IsFakeClient() ||
		!engine->IsDedicatedServer() ||
		FoFStatisticsPlayerCount( false ) <= 5 ||
		( warmup.IsValid() && warmup.GetBool() ) ||
		pPlayer->GetFoFLifeStartTime() == 0.0f )
	{
		return;
	}

	EntityMessageBegin( pPlayer, false );
		WRITE_BYTE( 14 );
		WRITE_FLOAT(
			gpGlobals->curtime - pPlayer->GetFoFLifeStartTime() );
	MessageEnd();
}

void CFoF_Player::RecordFoFCaptureParticipation(
	float flContribution, int nObjectiveDuration )
{
	m_nFoFCaptureObjectiveDuration = nObjectiveDuration;
	m_flFoFCaptureContribution += flContribution;
}

void CFoF_Player::AwardFoFCaptureNotoriety()
{
	if ( m_flFoFCaptureContribution > 0.0f &&
		m_nFoFCaptureObjectiveDuration > 0 )
	{
		static ConVarRef captureNotoriety(
			"fof_sv_capture_notoriety", true );
		const int nMaximum = captureNotoriety.IsValid() ?
			captureNotoriety.GetInt() : 50;
		const int nAward = RoundFloatToInt( RemapValClamped(
			m_flFoFCaptureContribution,
			0.0f,
			static_cast< float >( m_nFoFCaptureObjectiveDuration ),
			0.0f,
			static_cast< float >( nMaximum ) ) );

		SendFoFNotorietyNotice( 5, nAward, 0, NULL );
		AccumulateFoFNotoriety( nAward, false, false );
	}

	m_flFoFCaptureContribution = 0.0f;
	m_nFoFCaptureObjectiveDuration = 0;
}

void CFoF_Player::CommitFoFMapStatistics( bool bIntermission )
{
	if ( gpGlobals )
	{
		m_flFoFMapPlayTime +=
			gpGlobals->curtime - m_flFoFLifeStartTime;
	}

	if ( bIntermission )
	{
		static ConVarRef timeLimit( "mp_timelimit", true );
		if ( timeLimit.IsValid() && timeLimit.GetFloat() >= 10.0f )
			++m_iFoFPersonalStats[2];
		FoFSendPersonalStatUpdate( this, 2, m_iFoFPersonalStats[2] );
	}

	m_nFoFBestMultiKill = MAX( m_nFoFBestMultiKill,
		static_cast< int >( m_nMultiKill ) );
	m_nFoFBestDrunkard = MAX(
		m_nFoFBestDrunkard, m_nFoFCurrentDrunkard );

	const int nMode = FoFStatisticsCurrentMode();
	if ( nMode != 6 && FoFStatisticsPlayerCount( true ) > 6 )
	{
		if ( m_iFoFPersonalStats[0] != -1 &&
			m_nMultiKill > m_iFoFPersonalStats[0] )
		{
			m_iFoFPersonalStats[0] = m_nMultiKill;
			FoFSendPersonalStatUpdate(
				this, 0, m_iFoFPersonalStats[0] );
		}
		else if ( nMode != 3 && nMode != 5 &&
			m_flFoFLifeStartTime != 0.0f &&
			m_iFoFPersonalStats[1] != -1 &&
			gpGlobals->curtime - m_flFoFLifeStartTime >
				static_cast< float >( m_iFoFPersonalStats[1] ) )
		{
			m_iFoFPersonalStats[1] = static_cast< int >(
				gpGlobals->curtime - m_flFoFLifeStartTime );
			FoFSendPersonalStatUpdate(
				this, 1, m_iFoFPersonalStats[1] );
		}
		else if ( nMode != 3 && m_iFoFPersonalStats[3] != -1 &&
			static_cast< int >( m_flFoFDamageAccumulated ) >
				m_iFoFPersonalStats[3] )
		{
			m_iFoFPersonalStats[3] =
				static_cast< int >( m_flFoFDamageAccumulated );
			FoFSendPersonalStatUpdate(
				this, 3, m_iFoFPersonalStats[3] );
		}
		else if ( nMode != 3 && m_iFoFPersonalStats[4] != -1 &&
			m_nFoFCurrentDrunkard > m_iFoFPersonalStats[4] )
		{
			m_iFoFPersonalStats[4] = m_nFoFCurrentDrunkard;
			FoFSendPersonalStatUpdate(
				this, 4, m_iFoFPersonalStats[4] );
		}
	}

	FoFSendPlayTimeUpdate( this );
}

void CFoF_Player::AddFoFDrunkardAmount( int nAmount )
{
	if ( nAmount > 0 )
		m_nFoFCurrentDrunkard += nAmount;
}

static bool FoFTrackingIsEnemy(
	CFoF_Player *pTracker, CFoF_Player *pTarget )
{
	if ( !pTracker || !pTarget || pTracker == pTarget ||
		pTarget->GetTeamNumber() <= TEAM_SPECTATOR ||
		pTarget->IsFoFBotGhost() )
	{
		return false;
	}

	if ( pTracker->IsFoFBotGhost() )
		return true;

	return !g_pGameRules ||
		g_pGameRules->PlayerRelationship(
			pTracker, pTarget ) != GR_TEAMMATE;
}

static void FoFSendTrackingFootstep(
	CFoF_Player *pTracker, const Vector &vecOrigin,
	const QAngle &angles )
{
	EntityMessageBegin( pTracker, false );
		WRITE_BYTE( 17 );
		WRITE_BYTE( pTracker->entindex() );
		WRITE_VEC3COORD( vecOrigin );
		WRITE_ANGLES( angles );
	MessageEnd();
}

void FoFTrackLevelWeaponUse(
	CBaseCombatWeapon *pWeapon, CBasePlayer *pPlayer )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( !pWeapon || !pFoFPlayer || pWeapon->IsRemoveable() ||
		!currentMode.IsValid() || currentMode.GetInt() != 1 ||
		!classicShootout.IsValid() || classicShootout.GetBool() )
	{
		return;
	}

	pFoFPlayer->m_flFoFTrackingPickupTime = gpGlobals->curtime;
}

void CFoF_Player::UpdateFoFTrackingFootsteps( void )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() )
		return;

	const int nMode = currentMode.GetInt();
	if ( nMode == 4 && m_nPlayerKills == -1 )
	{
		const Vector vecTrackerOrigin = GetAbsOrigin();
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CFoF_Player *pTarget =
				ToFoFPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pTarget || !pTarget->IsAlive() ||
				!FoFTrackingIsEnemy( this, pTarget ) )
			{
				continue;
			}

			CBaseEntity *pGround = pTarget->GetGroundEntity();
			if ( pGround && pGround->IsBaseTrain() )
				continue;

			const Vector vecTargetOrigin = pTarget->GetAbsOrigin();
			const float flDistance =
				( vecTargetOrigin - vecTrackerOrigin ).Length();
			if ( flDistance > 1200.0f ||
				( flDistance < 500.0f &&
					FVisible( pTarget, 0x4041, NULL ) ) )
			{
				continue;
			}

			Vector vecDirection =
				vecTargetOrigin - m_vecFoFTrackingOrigins[i];
			if ( vecDirection.Length() <= 40.0f || !pGround )
				continue;

			QAngle angles;
			VectorAngles( vecDirection, angles );
			Vector vecEffectOrigin = vecTargetOrigin;
			vecEffectOrigin.z += 0.5f;
			FoFSendTrackingFootstep(
				this, vecEffectOrigin, angles );
			m_vecFoFTrackingOrigins[i] = vecTargetOrigin;
		}
		return;
	}

	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	if ( nMode != 1 || !classicShootout.IsValid() ||
		classicShootout.GetBool() ||
		m_flFoFTrackingPickupTime <= gpGlobals->curtime - 15.0f ||
		m_Local.m_bDucked )
	{
		return;
	}

	CBaseEntity *pTrackerGround = GetGroundEntity();
	if ( !pTrackerGround || pTrackerGround->IsBaseTrain() )
		return;

	const Vector vecTrackerOrigin = GetAbsOrigin();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pTarget = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pTarget || !pTarget->IsAlive() ||
			( pTarget != this &&
				!FoFTrackingIsEnemy( this, pTarget ) ) )
		{
			continue;
		}

		const Vector vecTargetOrigin = pTarget->GetAbsOrigin();
		if ( ( vecTargetOrigin - vecTrackerOrigin ).Length() > 1200.0f )
			continue;

		Vector vecDirection =
			vecTargetOrigin - m_vecFoFTrackingOrigins[i];
		if ( vecDirection.Length() <= 40.0f ||
			!pTarget->GetGroundEntity() )
		{
			continue;
		}

		QAngle angles;
		VectorAngles( vecDirection, angles );
		Vector vecEffectOrigin = vecTargetOrigin;
		vecEffectOrigin.z += 0.5f;
		FoFSendTrackingFootstep( this, vecEffectOrigin, angles );
		m_vecFoFTrackingOrigins[i] = vecTargetOrigin;
	}
}
