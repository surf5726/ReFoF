//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Break Bad objective and purchase-zone entities.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_breakbad_entities.h"
#include "fof/fof_player.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_bb_buyzone, CBBMultiBuyZone );
LINK_ENTITY_TO_CLASS( fof_bb_buyzone_sale1, CBBMultiBuySale );
LINK_ENTITY_TO_CLASS( fof_bb_buyzone_sale2, CBBMultiBuySale );
LINK_ENTITY_TO_CLASS( fof_bb_disarmzone, CBBMultiDisarmZone );
LINK_ENTITY_TO_CLASS( fof_bb_lootdropzone, CBBLootDropZone );
LINK_ENTITY_TO_CLASS( fof_bb_lootpickzone, CBBLootPickZone );
LINK_ENTITY_TO_CLASS( fof_bb_whiskeyzone, CBBMultiWhiskeyZone );

IMPLEMENT_SERVERCLASS_ST( CBBMulti, DT_BBMulti )
	SendPropBool( SENDINFO( m_bVisibleByAll ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CBBMulti )
	DEFINE_THINKFUNC( TouchThink ),
END_DATADESC()

CBBMulti::CBBMulti()
{
	m_flNextWarningTime = 0.0f;
	m_nRadius = 96;
	Q_memset( &m_ZoneTrace, 0, sizeof( m_ZoneTrace ) );
	m_flExpireTime = 0.0f;
	m_nMarkerLifetime = 30;
	m_bAffectsAllPlayers = false;
	m_nCashValue = 0;
	m_bVisibleByAll = false;
}

void CBBMulti::Spawn()
{
	BaseClass::Spawn();
	PrecacheModel( "models/props/cap_circle_64.mdl" );
	SetModel( "models/props/cap_circle_64.mdl" );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_NONE );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_flExpireTime = gpGlobals->curtime +
		static_cast< float >( m_nMarkerLifetime );
}

void CBBMulti::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	RemoveEffects( EF_NODRAW );
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	DispatchUpdateTransmitState();
	m_flExpireTime = gpGlobals->curtime +
		static_cast< float >( m_nMarkerLifetime );
	SetThink( &CBBMulti::TouchThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBBMulti::TouchThink()
{
	CFoF_Player *pOwner = ToFoFPlayer( GetOwnerEntity() );
	if ( pOwner || m_bAffectsAllPlayers )
	{
		if ( gpGlobals->curtime >= m_flExpireTime )
		{
			DeactivateZone();
			return;
		}

		AffectZone();
		if ( !m_bAffectsAllPlayers &&
			( !pOwner || !pOwner->IsAlive() ) )
		{
			DeactivateZone();
			return;
		}
	}

	SetNextThink( gpGlobals->curtime + 0.5f );
}

bool CBBMulti::IsPlayerInsideZone( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->IsObserver() )
		return false;

	const Vector vecZoneCenter = GetAbsOrigin() + Vector( 0, 0, 15 );
	const Vector vecPlayerCenter =
		pPlayer->GetAbsOrigin() + Vector( 0, 0, 15 );
	if ( vecZoneCenter.z - 25.0f > pPlayer->GetAbsOrigin().z )
		return false;

	const float flRadiusSqr = static_cast< float >(
		m_nRadius * m_nRadius );
	if ( vecPlayerCenter.DistToSqr( vecZoneCenter ) > flRadiusSqr )
		return false;

	UTIL_TraceLine( vecPlayerCenter, vecZoneCenter, 0x1400B,
		this, COLLISION_GROUP_NONE, &m_ZoneTrace );
	return m_ZoneTrace.fraction >= 1.0f;
}

void CBBMulti::DeactivateZone()
{
	SetOwnerEntity( NULL );
	AddEffects( EF_NODRAW );
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

void CBBMulti::AffectZone()
{
}

void CBBMulti::SendMarker( CFoF_Player *pPlayer, bool bVisible,
	int nMarkerType, const char *pszNotice ) const
{
	if ( !pPlayer || nMarkerType < 0 )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "HUDBBMulti" );
		WRITE_BYTE( bVisible ? 1 : 0 );
		WRITE_BYTE( m_nMarkerLifetime );
		WRITE_BYTE( nMarkerType );
		WRITE_VEC3COORD( GetAbsOrigin() );
	MessageEnd();

	if ( !bVisible )
		return;

	if ( !pszNotice || !pszNotice[0] )
		return;

	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszNotice );
	MessageEnd();
}

#if !defined( _DEBUG ) && defined( _M_IX86 )
COMPILE_TIME_ASSERT( sizeof( CBBMulti ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBMultiBuyZone ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBMultiBuySale ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBLootDropZone ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBLootPickZone ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBMultiDisarmZone ) == 0x4d4 );
COMPILE_TIME_ASSERT( sizeof( CBBMultiWhiskeyZone ) == 0x4d4 );
#endif

static const int FOF_PLAYERINFO_CARRYING_LOOT = 0x2000;

static void FoFSendBBNotice( CFoF_Player *pPlayer, const char *pszToken )
{
	if ( !pPlayer || !pszToken )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
	MessageEnd();
}

static CBaseEntity *FoFCreateCabinetGlass(
	CBBMultiBuySale *pCabinet, const char *pszModel )
{
	CDynamicProp *pGlass = dynamic_cast< CDynamicProp * >(
		CreateEntityByName( "prop_dynamic" ) );
	if ( !pGlass )
		return NULL;

	pGlass->SetAbsOrigin( pCabinet->GetAbsOrigin() );
	pGlass->SetAbsAngles( pCabinet->GetAbsAngles() );
	pGlass->SetParent( pCabinet );
	pGlass->SetModelName( AllocPooledString( pszModel ) );
	pGlass->Precache();
	DispatchSpawn( pGlass );
	return pGlass;
}

void CBBMultiBuyZone::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	CFoF_Player *pPlayer = ToFoFPlayer( pOwnerEntity );
	if ( pPlayer )
	{
		SendMarker( pPlayer, true, 0, "#BB_Notice_BuyArea" );
	}
}

void CBBMultiBuyZone::DeactivateZone()
{
	SendMarker( ToFoFPlayer( GetOwnerEntity() ), false, 0 );
	BaseClass::DeactivateZone();
}

void CBBMultiBuyZone::AffectZone()
{
	CFoF_Player *pPlayer = ToFoFPlayer( GetOwnerEntity() );
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	const Vector vecCenter = GetAbsOrigin() + Vector( 0, 0, 15 );
	if ( vecCenter.DistToSqr( pPlayer->GetAbsOrigin() ) >
		static_cast< float >( m_nRadius * m_nRadius ) )
	{
		return;
	}
	UTIL_TraceLine( vecCenter,
		pPlayer->GetAbsOrigin() + Vector( 0, 0, 15 ), 0x1400B,
		this, COLLISION_GROUP_NONE, &m_ZoneTrace );
	if ( m_ZoneTrace.fraction >= 1.0f )
		pPlayer->SetFoFBuyZone( 2, 1.5f );
}

void CBBMultiBuySale::Spawn()
{
	BaseClass::Spawn();
	PrecacheModel( "models/props/gun_cabinet/gun_cabinet.mdl" );
	PrecacheModel( "models/props/gun_cabinet/gun_cabinet_glass.mdl" );
	PrecacheModel( "models/props/gun_cabinet/gun_cabinet_gold.mdl" );
	PrecacheModel( "models/props/gun_cabinet/gun_cabinet_gold_glass.mdl" );

	SetModel( CabinetModelName() );
	m_nSkin = BuyTier() == 2 ? 3 : 0;
	FoFCreateCabinetGlass( this, GlassModelName() );
	m_bVisibleByAll = true;
	SetSolid( SOLID_VPHYSICS );
	SetCollisionGroup( COLLISION_GROUP_PLAYER_MOVEMENT );
	AddEffects( EF_ITEM_BLINK );
	m_nMarkerLifetime = 9999999;
	m_bAffectsAllPlayers = true;
}

void CBBMultiBuySale::AffectZone()
{
	const Vector vecCenter = GetAbsOrigin() + Vector( 0, 0, 15 );
	CBaseEntity *pEntities[256];
	const int nCount = UTIL_EntitiesInSphere( pEntities,
		ARRAYSIZE( pEntities ) - 1, vecCenter,
		static_cast< float >( m_nRadius ), FL_CLIENT );
	for ( int i = 0; i < nCount; ++i )
	{
		CFoF_Player *pPlayer = ToFoFPlayer( pEntities[i] );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		// Cabinets are offset from the floor by their model origin. The
		// original uses a sphere query and an outward visibility trace, not a
		// separate limit on how far the player's feet may sit below that origin.
		UTIL_TraceLine( vecCenter,
			pPlayer->GetAbsOrigin() + Vector( 0, 0, 15 ), 0x1400B,
			this, COLLISION_GROUP_NONE, &m_ZoneTrace );
		if ( m_ZoneTrace.fraction >= 1.0f )
			pPlayer->SetFoFBuyZone( BuyTier(), 1.5f );
	}
}

int CBBMultiBuySale::BuyTier()
{
	return !Q_stricmp( GetClassname(), "fof_bb_buyzone_sale1" ) ? 2 : 3;
}

const char *CBBMultiBuySale::CabinetModelName()
{
	return BuyTier() == 2
		? "models/props/gun_cabinet/gun_cabinet.mdl"
		: "models/props/gun_cabinet/gun_cabinet_gold.mdl";
}

const char *CBBMultiBuySale::GlassModelName()
{
	return BuyTier() == 2
		? "models/props/gun_cabinet/gun_cabinet_glass.mdl"
		: "models/props/gun_cabinet/gun_cabinet_gold_glass.mdl";
}

void CBBLootDropZone::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	CFoF_Player *pPlayer = ToFoFPlayer( pOwnerEntity );
	if ( pPlayer )
	{
		SendMarker( pPlayer, true, 2, "#BB_Notice_LootDropArea" );
	}
}

void CBBLootDropZone::Spawn()
{
	m_nMarkerLifetime = 75;
	m_bVisibleByAll = true;
	BaseClass::Spawn();
}

void CBBLootDropZone::SetCashValue( int nCashValue )
{
	m_nCashValue = MAX( 0, nCashValue );
}

void CBBLootDropZone::SetTeamSkin( int nTeam )
{
	m_nSkin = nTeam;
}

void CBBLootDropZone::DeactivateZone()
{
	SendMarker( ToFoFPlayer( GetOwnerEntity() ), false, 2 );
	BaseClass::DeactivateZone();
}

void CBBLootDropZone::AffectZone()
{
	CFoF_Player *pPlayer = ToFoFPlayer( GetOwnerEntity() );
	if ( !IsPlayerInsideZone( pPlayer ) ||
		( pPlayer->m_nPlayerInfo & FOF_PLAYERINFO_CARRYING_LOOT ) == 0 )
	{
		return;
	}

	pPlayer->m_nPlayerInfo &= ~FOF_PLAYERINFO_CARRYING_LOOT;
	pPlayer->AwardFoFCash(
		static_cast< float >( m_nCashValue ),
		"#Cash_Added_LootDeliver" );
	pPlayer->AddFoFNotoriety( 20, 1, 8 );
	DeactivateZone();
}

void CBBLootPickZone::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	CFoF_Player *pPlayer = ToFoFPlayer( pOwnerEntity );
	if ( pPlayer )
	{
		SendMarker( pPlayer, true, 1, "#BB_Notice_LootPickArea" );
	}
}

void CBBLootPickZone::DeactivateZone()
{
	SendMarker( ToFoFPlayer( GetOwnerEntity() ), false, 1 );
	BaseClass::DeactivateZone();
}

void CBBLootPickZone::AffectZone()
{
	CFoF_Player *pPlayer = ToFoFPlayer( GetOwnerEntity() );
	if ( !IsPlayerInsideZone( pPlayer ) ||
		( pPlayer->m_nPlayerInfo & FOF_PLAYERINFO_CARRYING_LOOT ) != 0 )
	{
		return;
	}

	if ( !pPlayer->GetActiveWeapon() )
	{
		if ( gpGlobals->curtime > m_flNextWarningTime )
		{
			FoFSendBBNotice( pPlayer, "#bb_lootpickup_warning" );
			m_flNextWarningTime = gpGlobals->curtime + 30.0f;
		}
		return;
	}

	pPlayer->m_nPlayerInfo |= FOF_PLAYERINFO_CARRYING_LOOT;
	pPlayer->AddFoFNotoriety( 2 );
	DeactivateZone();
}

void CBBMultiDisarmZone::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	CFoF_Player *pPlayer = ToFoFPlayer( pOwnerEntity );
	if ( pPlayer )
	{
		SendMarker( pPlayer, true, 3, "#BB_Notice_DisarmArea" );
	}
}

void CBBMultiDisarmZone::Spawn()
{
	m_nSkin = 0;
	m_nMarkerLifetime = 45;
	m_nRadius = 25;
	m_bVisibleByAll = false;
	BaseClass::Spawn();
}

void CBBMultiDisarmZone::DeactivateZone()
{
	SendMarker( ToFoFPlayer( GetOwnerEntity() ), false, 3 );
	BaseClass::DeactivateZone();
}

void CBBMultiDisarmZone::AffectZone()
{
	CFoF_Player *pPlayer = ToFoFPlayer( GetOwnerEntity() );
	if ( !IsPlayerInsideZone( pPlayer ) || !pPlayer->GetActiveWeapon() )
		return;

	pPlayer->SelfDisarmFoFWeapons();
	pPlayer->TakeHealth( 100, DMG_GENERIC );
	DeactivateZone();
}

void CBBMultiWhiskeyZone::SetOwnerEntity( CBaseEntity *pOwnerEntity )
{
	BaseClass::SetOwnerEntity( pOwnerEntity );
	CFoF_Player *pPlayer = ToFoFPlayer( pOwnerEntity );
	if ( pPlayer )
	{
		SendMarker( pPlayer, true, 6, "#BB_Notice_WhiskeyArea" );
	}
}

void CBBMultiWhiskeyZone::Spawn()
{
	m_nSkin = 0;
	m_nMarkerLifetime = 35;
	m_bVisibleByAll = false;
	BaseClass::Spawn();
}

void CBBMultiWhiskeyZone::DeactivateZone()
{
	SendMarker( ToFoFPlayer( GetOwnerEntity() ), false, 6 );
	BaseClass::DeactivateZone();
}

void CBBMultiWhiskeyZone::AffectZone()
{
	CFoF_Player *pPlayer = ToFoFPlayer( GetOwnerEntity() );
	if ( !IsPlayerInsideZone( pPlayer ) || !pPlayer->GetActiveWeapon() )
		return;

	pPlayer->GiveFoFNamedItem( "weapon_whiskey" );
	pPlayer->TakeHealth( 100, DMG_GENERIC );
	DeactivateZone();
}
