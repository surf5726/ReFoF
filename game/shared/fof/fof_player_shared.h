#ifndef FOF_PLAYER_SHARED_H
#define FOF_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

enum FoFTeamNumber_t
{
	FOF_TEAM_UNASSIGNED = 0,
	FOF_TEAM_SPECTATOR = 1,
	FOF_TEAM_VIGILANTES = 2,
	FOF_TEAM_DESPERADOS = 3,
	FOF_TEAM_BANDIDOS = 4,
	FOF_TEAM_RANGERS = 5,
	FOF_TEAM_ZOMBIES = 6,
};

#ifdef CLIENT_DLL
#include "fof/c_fof_player.h"
#define CFoF_Player C_FoF_Player

inline CFoF_Player *ToFoFPlayer( CBaseEntity *pEntity )
{
	return dynamic_cast< CFoF_Player * >( pEntity );
}
#else
#include "fof/fof_player.h"
#endif

bool FoFPlayersAreEnemies(
	const CFoF_Player *pShooter,
	const CFoF_Player *pTarget );

class CBaseEntity;
class CBasePlayer;
class CBaseCombatWeapon;
class QAngle;
class Vector;

int FoFPlayerInfo( const CBasePlayer *pPlayer );
int FoFHandStance( const CBasePlayer *pPlayer );
int FoFInBuyZone( const CBasePlayer *pPlayer );
float FoFCash( const CBasePlayer *pPlayer );
float FoFJailTime( const CBasePlayer *pPlayer );
float FoFUnarmedTime( const CBasePlayer *pPlayer );
int FoFPlayerKills( const CBasePlayer *pPlayer );
int FoFLastRoundNotoriety( const CBasePlayer *pPlayer );
int FoFMultiKill( const CBasePlayer *pPlayer );
int FoFPotionLevel( const CBasePlayer *pPlayer );
unsigned int FoFTeamCollisionContents( int teamNumber );
bool FoFPickupActive( const CBasePlayer *pPlayer );
bool FoFUsesHorseFreeAim( const CBasePlayer *pPlayer );
bool FoFShowsRifleCrosshair( const CFoF_Player *pPlayer );
float FoFCrosshairAperture( const CBasePlayer *pPlayer, int nHand );
float FoFSightExpFactor( const CBasePlayer *pPlayer );
float FoFWeaponThrowProgress( const CBasePlayer *pPlayer );
float FoFWalkFactor( const CBasePlayer *pPlayer );
void FoFStabilizeLocalPresentationVelocity(
	const CBasePlayer *pPlayer, Vector &velocity );
CBaseEntity *FoFKicker( const CBasePlayer *pPlayer );
QAngle FoFHorseAngles( const CBasePlayer *pPlayer );
void FoFSetHorseAngles( CBasePlayer *pPlayer, const QAngle &angles );
float FoFSpeedPenalty( const CBasePlayer *pPlayer );
void FoFSetSpeedPenalty( CBasePlayer *pPlayer, float flPenalty );

enum FoFPlayerCollisionDecision_t
{
	FOF_PLAYER_COLLISION_DEFER,
	FOF_PLAYER_COLLISION_REJECT,
	FOF_PLAYER_COLLISION_ACCEPT
};

FoFPlayerCollisionDecision_t FoFResolvePlayerTeamCollision(
	int nTeamNumber, int nCollisionGroup, int &nContentsMask );

#endif // FOF_PLAYER_SHARED_H
