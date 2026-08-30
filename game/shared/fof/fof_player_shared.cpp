#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
extern ConVar fof_horse_freeaim;
#endif

int FoFPlayerInfo( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFPlayerInfo() : 0;
}

int FoFHandStance( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFHandStance() : 0;
}

int FoFInBuyZone( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFInBuyZone() : 0;
}

float FoFCash( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFCash() : 0.0f;
}

float FoFJailTime( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFJailTime() : 0.0f;
}

float FoFUnarmedTime( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFUnarmedTime() : 0.0f;
}

int FoFPlayerKills( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFPlayerKills() : 0;
}

int FoFLastRoundNotoriety( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFLastRoundNotoriety() : 0;
}

int FoFMultiKill( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFMultiKill() : 0;
}

int FoFPotionLevel( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFPotionLevel() : 0;
}

unsigned int FoFTeamCollisionContents( int teamNumber )
{
	switch ( teamNumber )
	{
	case 2:
		return CONTENTS_TEAM1;
	case 3:
		return CONTENTS_TEAM2;
	case 4:
		return CONTENTS_UNUSED;
	case 5:
		return CONTENTS_UNUSED6;
	default:
		return 0;
	}
}

bool FoFPickupActive( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer && pFoFPlayer->IsFoFPickupActive();
}

bool FoFUsesHorseFreeAim( const CBasePlayer *pPlayer )
{
#ifdef CLIENT_DLL
	(void)pPlayer;
	return fof_horse_freeaim.GetBool();
#else
	if ( !pPlayer || !pPlayer->IsNetClient() || pPlayer->IsFakeClient() )
		return false;
	const char *pszValue = engine->GetClientConVarValue(
		pPlayer->entindex(), "fof_horse_freeaim" );
	return pszValue && Q_atoi( pszValue ) != 0;
#endif
}

bool FoFShowsRifleCrosshair( const CFoF_Player *pPlayer )
{
#ifdef CLIENT_DLL
	static ConVarRef rifleCrosshair( "fof_crosshair_rifle", true );
	return rifleCrosshair.IsValid() && rifleCrosshair.GetBool();
#else
	return pPlayer && pPlayer->ShowsFoFRifleCrosshair();
#endif
}

float FoFCrosshairAperture( const CBasePlayer *pPlayer, int nHand )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFCrosshairAperture( nHand ) : 0.0f;
}

float FoFSightExpFactor( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFSightExpFactor() : 0.0f;
}

float FoFWeaponThrowProgress( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFWeaponThrowProgress() : 0.0f;
}

float FoFWalkFactor( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFWalkFactor() : 0.0f;
}

#ifdef CLIENT_DLL
void FoFStabilizeLocalPresentationVelocity(
	const CBasePlayer *pPlayer, Vector &velocity )
{
	// Prediction restores the last acknowledged local state between command
	// simulations.  At render rate that can expose a one-frame zero horizontal
	// velocity while a movement key is still held.  Keep a short-lived sample
	// only for the first-person viewmodel presentation.  Player gait animation
	// must use EstimateAbsVelocity directly, matching the shipped client.
	// This helper never writes movement, origin, collision, or networked state.
	static EHANDLE s_hOwner;
	static Vector s_vecLastVelocity;
	static float s_flLastSampleTime = -1.0f;

	if ( !pPlayer || pPlayer != CBasePlayer::GetLocalPlayer() )
		return;

	if ( s_hOwner.Get() != pPlayer )
	{
		s_hOwner = const_cast< CBasePlayer * >( pPlayer );
		s_vecLastVelocity.Init();
		s_flLastSampleTime = -1.0f;
	}

	const int nMoveButtons =
		IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT;
	const bool bMovementHeld =
		( pPlayer->m_nButtons & nMoveButtons ) != 0;
	const float flMovingThresholdSqr = 0.5f * 0.5f;

	if ( velocity.Length2DSqr() > flMovingThresholdSqr )
	{
		s_vecLastVelocity = velocity;
		// curtime is rewound while outstanding user commands are replayed.
		// realtime is monotonic across prediction and therefore suitable for a
		// render-only cache shared by the animstate and viewmodel paths.
		s_flLastSampleTime = gpGlobals->realtime;
		return;
	}

	const float flElapsed = gpGlobals->realtime - s_flLastSampleTime;
	const float flPredictionRestoreWindow =
		MAX( 0.10f, TICK_INTERVAL * 6.0f );
	if ( bMovementHeld && flElapsed >= 0.0f &&
		flElapsed <= flPredictionRestoreWindow &&
		s_vecLastVelocity.Length2DSqr() > flMovingThresholdSqr )
	{
		velocity.x = s_vecLastVelocity.x;
		velocity.y = s_vecLastVelocity.y;
		return;
	}

	if ( !bMovementHeld || flElapsed < 0.0f ||
		flElapsed > flPredictionRestoreWindow )
	{
		s_vecLastVelocity.Init();
		s_flLastSampleTime = -1.0f;
	}
}
#else
void FoFStabilizeLocalPresentationVelocity(
	const CBasePlayer *pPlayer, Vector &velocity )
{
	(void)pPlayer;
	(void)velocity;
}
#endif

CBaseEntity *FoFKicker( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFKicker() : NULL;
}

QAngle FoFHorseAngles( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFHorseAngles() : vec3_angle;
}

void FoFSetHorseAngles( CBasePlayer *pPlayer, const QAngle &angles )
{
	CFoF_Player *pFoFPlayer = dynamic_cast< CFoF_Player * >( pPlayer );
	if ( pFoFPlayer )
		pFoFPlayer->SetFoFHorseAngles( angles );
}

float FoFSpeedPenalty( const CBasePlayer *pPlayer )
{
	const CFoF_Player *pFoFPlayer =
		dynamic_cast< const CFoF_Player * >( pPlayer );
	return pFoFPlayer ? pFoFPlayer->GetFoFSpeedPenalty() : 0.0f;
}

void FoFSetSpeedPenalty( CBasePlayer *pPlayer, float flPenalty )
{
	CFoF_Player *pFoFPlayer = dynamic_cast< CFoF_Player * >( pPlayer );
	if ( pFoFPlayer )
		pFoFPlayer->SetFoFSpeedPenalty( flPenalty );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF predicted player state shared by spawn and command updates.
//
//=============================================================================//

void CFoF_Player::ResetFoFSharedSpawnState()
{
	m_nFoFDynamiteBeltAttempts = 10;
	m_nFoFDynamiteBeltHits = 5;
	m_flTargetCrosshairAperture = 0.0f;
	m_flTargetCrosshairAperture2 = 0.0f;
	m_flWalkSpreadFactor = 0.0f;
	m_flCrosshairAperture = 0.0f;
	m_flCrosshairAperture2 = 0.0f;
	m_flFoFLastWallJumpTime = -FLT_MAX;
	m_Local.m_bPoisoned = false;
	m_bPickupActive = false;
	m_hAttachedObject = NULL;
	m_attachedPositionObjectSpace.Init();
	m_attachedAnglesPlayerSpace.Init();
#ifndef CLIENT_DLL
	SetFoFKicker( NULL );
#endif
}

void CFoF_Player::UpdateFoFDrunkness()
{
	// The shipped shared update deliberately does not clamp the final
	// subtraction. Keeping the same update on both sides avoids a one-command
	// correction around zero.
	if ( m_flDrunkness > 0.0f )
		m_flDrunkness -= gpGlobals->frametime * 1.6f;
}

void CFoF_Player::UpdateFoFCommandState()
{
	// FoF ItemPostFrame processes throw capture first, then the drunk
	// timer, kick state, sight state and finally accuracy. Several of these
	// routines consume attack buttons or alter attack clocks, so this ordering
	// must remain identical on both simulation sides.
	UpdateFoFCaptureInput();
	UpdateFoFDrunkness();
	UpdateFoFKick();
	UpdateFoFSightExpansion();
	UpdateFoFAccuracyAperture();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF player locomotion state shared by prediction and authority.
//
//=============================================================================//

bool CFoF_Player::IsFoFWalking() const
{
	// Both shipped modules use the continuous FoF factor rather than HL2's
	// m_fIsWalking latch. GetAbsVelocity also preserves the original lazy
	// absolute-velocity refresh before the length test.
	return m_flWalkFactor > 0.1f && GetAbsVelocity().Length() > 5.0f;
}

void CFoF_Player::StartSprinting()
{
	// Deliberately empty in the shipped FoF server.
}

void CFoF_Player::StopSprinting()
{
	// Deliberately empty in the shipped FoF server.
}

void CFoF_Player::StartWalking()
{
	// Deliberately empty in the shipped FoF server.
}

void CFoF_Player::StopWalking()
{
#ifdef CLIENT_DLL
	// The original client first restores CHL2MP_Player's normal-speed and
	// walking state, then tail-calls FoF's complete weapon-speed recalculation.
	BaseClass::StopWalking();
	RecalculateWeaponSpeed();
#else
	// The original server first executes CHL2_Player::StopSprinting and
	// then tail-calls CFoF_Player::RecalculateWeaponSpeed.
	BaseClass::StopSprinting();
	RecalculateWeaponSpeed();
#endif
}

bool CFoF_Player::IsWalking()
{
	return IsFoFWalking();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF player motion state shared by prediction and authority.
//
//=============================================================================//

#ifndef CLIENT_DLL

class CFoFGhostGunCollisionEnumerator : public IEntityEnumerator
{
public:
	CFoFGhostGunCollisionEnumerator(
		CFoF_Player *pPlayer, CBaseEntity *pImpulseSource,
		CBaseEntity *pLastCollision )
		: m_pPlayer( pPlayer )
		, m_pImpulseSource( pImpulseSource )
		, m_hLastCollision( pLastCollision )
		, m_nPlayerCollisions( 0 )
	{
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEntity = pHandleEntity ?
			gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() ) : NULL;
		if ( !pEntity || pEntity == m_pPlayer )
			return true;

		if ( pEntity->ClassMatches( "prop_physics_respawnable" ) )
		{
			if ( pEntity == m_hLastCollision )
				return true;

			if ( pEntity->GetHealth() > 0 )
			{
				CTakeDamageInfo propDamage(
					m_pImpulseSource, m_pImpulseSource,
					50.0f, DMG_FALL );
				pEntity->TakeDamage( propDamage );

				CTakeDamageInfo playerDamage(
					m_pImpulseSource, m_pImpulseSource,
					15.0f, DMG_FALL );
				m_pPlayer->TakeDamage( playerDamage );
			}

			m_hLastCollision = pEntity;
			return true;
		}

		if ( !pEntity->IsPlayer() ||
			pEntity == m_hLastCollision ||
			m_nPlayerCollisions >= 2 )
		{
			return true;
		}

		CFoF_Player *pTarget = ToFoFPlayer( pEntity );
		if ( !pTarget || pTarget == m_pImpulseSource )
			return true;

		CTakeDamageInfo playerDamage(
			m_pImpulseSource, m_pImpulseSource,
			15.0f, DMG_FALL );
		pTarget->TakeDamage( playerDamage );
		m_hLastCollision = pTarget;
		++m_nPlayerCollisions;
		return true;
	}

	CBaseEntity *GetLastCollision() const
	{
		return m_hLastCollision.Get();
	}

private:
	CFoF_Player *m_pPlayer;
	CBaseEntity *m_pImpulseSource;
	CHandle< CBaseEntity > m_hLastCollision;
	int m_nPlayerCollisions;
};

void CFoF_Player::UpdateFoFGhostGunCollision()
{
	CFoF_Player *pImpulseSource = m_hFoFGhostGunImpulseSource.Get();
	if ( !pImpulseSource )
		return;

	if ( GetGroundEntity() )
	{
		// The original server retains attribution for one
		// second after landing, then clears both the source and its timestamp.
		if ( gpGlobals->curtime > m_flFoFGhostGunImpulseTime + 1.0f )
		{
			m_hFoFGhostGunImpulseSource = NULL;
			m_flFoFGhostGunImpulseTime = 0.0f;
		}
		return;
	}

	Vector vecDirection = GetAbsVelocity();
	VectorNormalize( vecDirection );
	const Vector vecOrigin = GetAbsOrigin();

	Ray_t ray;
	ray.Init(
		vecOrigin,
		vecOrigin + vecDirection * 16.0f,
		WorldAlignMins(),
		WorldAlignMaxs() );

	// The original sweep damages respawnable physics props and up to two
	// players per command.  The source handle supplies damage attribution;
	// the last-contact handle prevents repeated damage while bodies overlap.
	CFoFGhostGunCollisionEnumerator enumerator(
		this, pImpulseSource, m_hFoFGhostGunLastCollision.Get() );
	enginetrace->EnumerateEntities( ray, false, &enumerator );
	m_hFoFGhostGunLastCollision = enumerator.GetLastCollision();
}

#endif // !CLIENT_DLL

void CFoF_Player::PostThink()
{
	BaseClass::PostThink();

	// CFoF_Player::PostThink removes the
	// transitional animation-duck flag as soon as the duck button is released.
	if ( IsAlive() && ( GetFlags() & FL_ANIMDUCKING ) &&
		!( m_nButtons & IN_DUCK ) )
	{
		RemoveFlag( FL_ANIMDUCKING );
	}

#ifndef CLIENT_DLL
	UpdateFoFGhostGunCollision();
	UpdateFoFKickerAttribution();
	UpdateMountedFoFHorse();
	UpdateFoFTrackingFootsteps();
	UpdateFoFCrateMenuRange();
#endif

	// CBasePlayer restores the stock standing hull each command. Mounted FoF
	// players use the original server's 100-unit hull and 94-unit eye height on
	// both sides so prediction and authority finish the command identically.
	if ( IsAlive() && IsOnFoFHorse() )
	{
		const float flScale = GetModelScale();
		SetCollisionBounds(
			Vector( -16.0f, -16.0f, 0.0f ) * flScale,
			Vector( 16.0f, 16.0f, 100.0f ) * flScale );
		SetViewOffset( Vector( 0.0f, 0.0f, 94.0f ) * flScale );
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF shared player team-collision filtering.
//
//=============================================================================//

#ifdef CLIENT_DLL
static const int s_nFoFTeamCollisionFlags =
	FCVAR_REPLICATED | FCVAR_NOTIFY;
#else
static const int s_nFoFTeamCollisionFlags =
	FCVAR_GAMEDLL | FCVAR_REPLICATED | FCVAR_NOTIFY;
#endif

ConVar fof_sv_team_collisions(
	"fof_sv_team_collisions",
	"0",
	s_nFoFTeamCollisionFlags,
	"Enables own team collisions",
	true,
	0.0f,
	true,
	1.0f );

FoFPlayerCollisionDecision_t FoFResolvePlayerTeamCollision(
	int nTeamNumber, int nCollisionGroup, int &nContentsMask )
{
	if ( nCollisionGroup != COLLISION_GROUP_PLAYER_MOVEMENT &&
		nCollisionGroup != COLLISION_GROUP_PROJECTILE )
		return FOF_PLAYER_COLLISION_DEFER;

	if ( fof_sv_team_collisions.GetBool() )
		return FOF_PLAYER_COLLISION_ACCEPT;

	const unsigned int nTeamContents =
		FoFTeamCollisionContents( nTeamNumber );
	if ( nContentsMask == MASK_PLAYERSOLID )
		nContentsMask |= nTeamContents;

	if ( nTeamNumber == 0 )
		return FOF_PLAYER_COLLISION_ACCEPT;

	if ( nTeamContents != 0 &&
		( static_cast< unsigned int >( nContentsMask ) & nTeamContents ) != 0 )
	{
		return FOF_PLAYER_COLLISION_REJECT;
	}

	return FOF_PLAYER_COLLISION_DEFER;
}
