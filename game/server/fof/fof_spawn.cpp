#include "cbase.h"
#include "baseentity.h"
#include "modelentities.h"
#include "fof/fof_player.h"
#include "fof/fof_spawn.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( info_player_fof, CTeamSpawn );
LINK_ENTITY_TO_CLASS( info_player_vigilante, CTeamSpawn );
LINK_ENTITY_TO_CLASS( info_player_desperado, CTeamSpawn );
LINK_ENTITY_TO_CLASS( info_player_bandido, CTeamSpawn );
LINK_ENTITY_TO_CLASS( info_player_ranger, CTeamSpawn );

BEGIN_DATADESC( CTeamSpawn )
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

CTeamSpawn::CTeamSpawn() : m_bDisabled( false )
{
}

void CTeamSpawn::InputEnable( inputdata_t &inputData )
{
	m_bDisabled = false;
}

void CTeamSpawn::InputDisable( inputdata_t &inputData )
{
	m_bDisabled = true;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF team respawn-room barrier.
//
//=============================================================================//

LINK_ENTITY_TO_CLASS(
	func_respawnroomvisualizer,
	CFuncRespawnRoomVisualizer );

IMPLEMENT_SERVERCLASS_ST(
	CFuncRespawnRoomVisualizer,
	DT_FuncRespawnRoomVisualizer )
END_SEND_TABLE()

BEGIN_DATADESC( CFuncRespawnRoomVisualizer )
	DEFINE_KEYFIELD( m_nTeamBlock, FIELD_INTEGER, "TeamBlock" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

CFuncRespawnRoomVisualizer::CFuncRespawnRoomVisualizer()
	: m_nTeamBlock( 0 )
{
}

void CFuncRespawnRoomVisualizer::Spawn()
{
	BaseClass::Spawn();

	RemoveSolidFlags( FSOLID_TRIGGER );
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	// The networked team is the team that is allowed through the brush. The
	// map key stores the opposite team: the one that this brush blocks.
	if ( m_nTeamBlock > 1 )
		ChangeTeam( m_nTeamBlock == 2 ? 3 : 2 );
	else if ( m_nTeamBlock == 1 )
		ChangeTeam( 1 );

	SetCollisionGroup( COLLISION_GROUP_PLAYER_MOVEMENT );
}

bool CFuncRespawnRoomVisualizer::ShouldCollide(
	int collisionGroup, int contentsMask ) const
{
	if ( GetTeamNumber() == 0 ||
		collisionGroup != COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		return false;
	}

	if ( GetTeamNumber() == 2 )
		return ( contentsMask & CONTENTS_TEAM2 ) != 0;

	if ( GetTeamNumber() == 3 )
		return ( contentsMask & CONTENTS_TEAM1 ) != 0;

	return true;
}

int CFuncRespawnRoomVisualizer::ShouldTransmit(
	const CCheckTransmitInfo *pInfo )
{
	if ( GetTeamNumber() == 0 )
		return FL_EDICT_DONTSEND;

	CBaseEntity *pRecipient = pInfo && pInfo->m_pClientEnt ?
		CBaseEntity::Instance( pInfo->m_pClientEnt ) : NULL;
	if ( !pRecipient || pRecipient->GetTeamNumber() <= 1 ||
		InSameTeam( pRecipient ) )
	{
		return FL_EDICT_DONTSEND;
	}

	return FL_EDICT_ALWAYS;
}

void CFuncRespawnRoomVisualizer::InputDisable(
	inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );
}

void CFuncRespawnRoomVisualizer::InputEnable(
	inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	RemoveSolidFlags( FSOLID_TRIGGER );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	RemoveEffects( EF_NODRAW );
	RespawnBlockedPlayersInside();
}

void CFuncRespawnRoomVisualizer::RespawnBlockedPlayersInside()
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() ||
			pPlayer->GetTeamNumber() != m_nTeamBlock )
		{
			continue;
		}

		const Vector &vecOrigin = pPlayer->GetAbsOrigin();
		Ray_t ray;
		ray.Init( vecOrigin, vecOrigin );

		trace_t trace;
		enginetrace->ClipRayToCollideable(
			ray, MASK_ALL, CollisionProp(), &trace );
		if ( trace.startsolid )
			pPlayer->FinalizeFoFSpawn( true );
	}
}
