//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-side FoF map items.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_item_entities.h"
#include "fof/fof_player.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void FoFSendWrongTeamXBowNotice( CFoF_Player *pPlayer )
{
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "#fof_xbow_pickup_wrong_team" );
	MessageEnd();
}

LINK_ENTITY_TO_CLASS( item_whiskey, CItem_FoFResupply );
LINK_ENTITY_TO_CLASS( item_potion, CItem_FoFPotion );
LINK_ENTITY_TO_CLASS( item_potion_small, CItem_FoFPotion );
LINK_ENTITY_TO_CLASS( item_golden_skull, CItem_FoFSkull );
LINK_ENTITY_TO_CLASS( item_xbow_spawn, CItem_XBowSpawn );

BEGIN_DATADESC( CItem_FoFResupply )
	DEFINE_KEYFIELD( m_nHealth, FIELD_INTEGER, "Health" ),
END_DATADESC()

BEGIN_DATADESC( CItem_FoFPotion )
	DEFINE_KEYFIELD( m_nPotion, FIELD_INTEGER, "PotionAmount" ),
END_DATADESC()

CItem_FoFResupply::CItem_FoFResupply()
	: m_nHealth( 25 )
{
}

void CItem_FoFResupply::Precache()
{
	PrecacheModel( "models/items_fof/whiskey_world.mdl" );
	PrecacheScriptSound( "Player.Burp" );
	PrecacheScriptSound( "Whiskey.Glug" );
}

void CItem_FoFResupply::Spawn()
{
	Precache();
	SetModel( "models/items_fof/whiskey_world.mdl" );
	AddSpawnFlags( SF_ITEM_START_CONSTRAINED );
	BaseClass::Spawn();
	RemoveSolidFlags( FSOLID_TRIGGER );
	AddEffects( 0x800 | EF_ITEM_BLINK | EF_NOSHADOW );
	RemoveSpawnFlags( SF_NORESPAWN );
}

CBaseEntity *CItem_FoFResupply::Respawn()
{
	return BaseClass::Respawn();
}

bool CItem_FoFResupply::MyTouch( CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	return pFoFPlayer && pFoFPlayer->ConsumeFoFWhiskey( m_nHealth );
}

CItem_FoFPotion::CItem_FoFPotion()
	: m_nPotion( 0 )
{
}

void CItem_FoFPotion::Precache()
{
	PrecacheModel( "models/props/potion_bottle.mdl" );
	PrecacheModel( "models/props/potion_bottle_small.mdl" );
	PrecacheScriptSound( "Potion.Spawn" );
	PrecacheScriptSound( "Whiskey.Glug" );
	PrecacheParticleSystem( "ghost_weapon_spawn" );
}

void CItem_FoFPotion::Spawn()
{
	Precache();
	SetModel( "models/props/potion_bottle.mdl" );
	if ( !FClassnameIs( this, "item_potion_small" ) )
		AddSpawnFlags( SF_ITEM_START_CONSTRAINED );
	BaseClass::Spawn();
	RemoveSolidFlags( FSOLID_TRIGGER );
	AddEffects( 0x800 | EF_ITEM_BLINK | EF_NOSHADOW );

	if ( FClassnameIs( this, "item_potion_small" ) )
	{
		SetModel( "models/props/potion_bottle_small.mdl" );
		m_nPotion = 15;
	}
}

CBaseEntity *CItem_FoFPotion::Respawn()
{
	EmitSound( "Potion.Spawn" );
	DispatchParticleEffect(
		"ghost_weapon_spawn", GetAbsOrigin(), vec3_angle, NULL );
	return BaseClass::Respawn();
}

bool CItem_FoFPotion::MyTouch( CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( IsEffectActive( EF_NODRAW ) ||
		!pFoFPlayer || !pFoFPlayer->IsAlive() ||
		( pFoFPlayer->m_nPlayerInfo & 0x40000 ) )
	{
		return false;
	}

	pFoFPlayer->AddFoFPotion( m_nPotion );
	pFoFPlayer->EmitSound( "Whiskey.Glug" );
	return true;
}

void CItem_FoFSkull::Precache()
{
	PrecacheModel( "models/items_fof/golden_skull.mdl" );
	PrecacheModel( "sprites/lgtning.vmt" );
	PrecacheScriptSound( "FoF.ThunderClose" );
}

void CItem_FoFSkull::Spawn()
{
	Precache();
	SetModel( "models/items_fof/golden_skull.mdl" );
	BaseClass::Spawn();
}

bool CItem_FoFSkull::MyTouch( CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( !pFoFPlayer || !pFoFPlayer->IsAlive() ||
		( pFoFPlayer->m_nPlayerInfo & 0x800 ) )
	{
		return false;
	}

	pFoFPlayer->EmitSound( "FoF.BountyObjective" );
	pFoFPlayer->GiveFoFNamedItem( "weapon_ghostgun" );
	return true;
}

CItem_XBowSpawn::CItem_XBowSpawn()
	: m_flNextWrongTeamNotice( 0.0f )
{
}

void CItem_XBowSpawn::Precache()
{
	PrecacheModel( "models/weapons/w_xbow.mdl" );
	PrecacheModel( "sprites/lgtning.vmt" );
	PrecacheScriptSound( "FoF.ThunderClose" );
}

void CItem_XBowSpawn::Spawn()
{
	Precache();
	SetModel( "models/weapons/w_xbow.mdl" );
	BaseClass::Spawn();
}

bool CItem_XBowSpawn::MyTouch( CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( !pFoFPlayer || !pFoFPlayer->IsAlive() ||
		( pFoFPlayer->m_nPlayerInfo & 0x800 ) )
	{
		return false;
	}

	if ( GetTeamNumber() != pFoFPlayer->GetTeamNumber() )
	{
		if ( gpGlobals->curtime > m_flNextWrongTeamNotice )
		{
			FoFSendWrongTeamXBowNotice( pFoFPlayer );
			m_flNextWrongTeamNotice = gpGlobals->curtime + 5.0f;
		}
		return false;
	}

	pFoFPlayer->EmitSound( "FoF.BountyObjective" );
	CBaseEntity *pWeapon = pFoFPlayer->GiveFoFNamedItem( "weapon_xbow" );
	if ( pWeapon )
	{
		pWeapon->AddEffects( EF_NOINTERP );
		pWeapon->ChangeTeam( GetTeamNumber() );
	}
	return true;
}
