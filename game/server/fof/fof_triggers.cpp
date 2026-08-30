//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-side FoF trigger entities.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_player.h"
#include "fof/fof_triggers.h"
#include "props.h"
#include "saverestore_utlvector.h"
#include "triggers.h"
#include "world.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_buyzone, CBuyZone );
LINK_ENTITY_TO_CLASS( fof_dynamite_remover, CDynamiteRemover );
LINK_ENTITY_TO_CLASS( fof_trigger_speed, CTriggerSpeed );
LINK_ENTITY_TO_CLASS( trigger_hurt_fof, CTriggerHurtFoF );

BEGIN_DATADESC( CBuyZone )
	DEFINE_KEYFIELD( m_iFoFTeam, FIELD_INTEGER, "team" ),
	DEFINE_FIELD( m_bFoFEnabled, FIELD_BOOLEAN ),
	DEFINE_ENTITYFUNC( BuyZoneTouch ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

BEGIN_DATADESC( CDynamiteRemover )
	DEFINE_KEYFIELD( m_iFoFTeam, FIELD_INTEGER, "team" ),
	DEFINE_FIELD( m_bFoFEnabled, FIELD_BOOLEAN ),
	DEFINE_ENTITYFUNC( DynamiteTouch ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

BEGIN_DATADESC( CTriggerSpeed )
	DEFINE_ENTITYFUNC( TriggerSpeedTouch ),
	DEFINE_KEYFIELD( m_flMinSpeed, FIELD_FLOAT, "minSpeed" ),
	DEFINE_KEYFIELD( m_flMaxSpeed, FIELD_FLOAT, "maxSpeed" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_OUTPUT( m_OnMinSpeedReached, "OnMinSpeedReached" ),
	DEFINE_OUTPUT( m_OnMaxSpeedReached, "OnMaxSpeedReached" ),
END_DATADESC()

BEGIN_DATADESC( CTriggerHurtFoF )
	DEFINE_FUNCTION( RadiationThink ),
	DEFINE_FUNCTION( HurtThink ),
	DEFINE_FIELD( m_flOriginalDamage, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flDamage, FIELD_FLOAT, "damage" ),
	DEFINE_KEYFIELD( m_flDamageCap, FIELD_FLOAT, "damagecap" ),
	DEFINE_FIELD( m_flLastDmgTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDmgResetTime, FIELD_TIME ),
	DEFINE_KEYFIELD( m_bitsDamageInflict, FIELD_INTEGER, "damagetype" ),
	DEFINE_KEYFIELD( m_damageModel, FIELD_INTEGER, "damagemodel" ),
	DEFINE_KEYFIELD( m_bNoDmgForce, FIELD_BOOLEAN, "nodmgforce" ),
	DEFINE_KEYFIELD( m_bOnlyKicked, FIELD_BOOLEAN, "onlykicked" ),
	DEFINE_KEYFIELD( m_nTeamFilter, FIELD_INTEGER, "teamfilter" ),
	DEFINE_FIELD( m_hLastKickedEntity, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_hurtEntities, FIELD_EHANDLE ),
	DEFINE_INPUT( m_flDamage, FIELD_FLOAT, "SetDamage" ),
	DEFINE_OUTPUT( m_OnHurt, "OnHurt" ),
	DEFINE_OUTPUT( m_OnHurtPlayer, "OnHurtPlayer" ),
END_DATADESC()

CBuyZone::CBuyZone()
	: m_iFoFTeam( 2 ),
	  m_bFoFEnabled( true )
{
}

void CBuyZone::Spawn()
{
	InitTrigger();
	ChangeTeam( m_iFoFTeam );
	SetTouch( &CBuyZone::BuyZoneTouch );
}

void CBuyZone::InputEnable( inputdata_t &inputdata )
{
	m_bFoFEnabled = true;
}

void CBuyZone::InputDisable( inputdata_t &inputdata )
{
	m_bFoFEnabled = false;
}

void CBuyZone::BuyZoneTouch( CBaseEntity *pOther )
{
	if ( !m_bFoFEnabled )
		return;

	CFoF_Player *pPlayer = ToFoFPlayer( pOther );
	if ( pPlayer && pPlayer->GetTeamNumber() == GetTeamNumber() )
		pPlayer->m_nInBuyZone = 4;
}

CDynamiteRemover::CDynamiteRemover()
	: m_iFoFTeam( 0 ),
	  m_bFoFEnabled( true )
{
}

void CDynamiteRemover::Spawn()
{
	BaseClass::Spawn();
	ChangeTeam( m_iFoFTeam );
	SetTouch( &CDynamiteRemover::DynamiteTouch );
}

void CDynamiteRemover::InputEnable( inputdata_t &inputdata )
{
	m_bFoFEnabled = true;
}

void CDynamiteRemover::InputDisable( inputdata_t &inputdata )
{
	m_bFoFEnabled = false;
}

void CDynamiteRemover::DynamiteTouch( CBaseEntity *pOther )
{
	if ( !m_bFoFEnabled || !pOther )
		return;

	if ( !FClassnameIs( pOther, "dynamite" ) &&
		!FClassnameIs( pOther, "dynamite_black" ) )
	{
		return;
	}

	if ( GetTeamNumber() == 0 ||
		GetTeamNumber() == pOther->GetTeamNumber() )
	{
		UTIL_Remove( pOther );
	}
}

CTriggerSpeed::CTriggerSpeed()
	: m_bFoFEnabled( true ),
	  m_flMinSpeed( 20.0f ),
	  m_flMaxSpeed( 100.0f ),
	  m_nNextSpeedSample( 0 )
{
	for ( int i = 0; i < ARRAYSIZE( m_flSpeedSamples ); ++i )
		m_flSpeedSamples[i] = -1.0f;
}

void CTriggerSpeed::Spawn()
{
	InitTrigger();
	SetTouch( &CTriggerSpeed::TriggerSpeedTouch );
	for ( int i = 0; i < ARRAYSIZE( m_flSpeedSamples ); ++i )
		m_flSpeedSamples[i] = -1.0f;
	m_nNextSpeedSample = 0;
}

void CTriggerSpeed::InputEnable( inputdata_t &inputdata )
{
	m_bFoFEnabled = true;
}

void CTriggerSpeed::InputDisable( inputdata_t &inputdata )
{
	m_bFoFEnabled = false;
}

void CTriggerSpeed::TriggerSpeedTouch( CBaseEntity *pOther )
{
	if ( !m_bFoFEnabled )
		return;
	if ( !pOther || !pOther->IsPlayer() )
	{
		for ( int i = 0; i < ARRAYSIZE( m_flSpeedSamples ); ++i )
			m_flSpeedSamples[i] = -1.0f;
		return;
	}

	m_flSpeedSamples[m_nNextSpeedSample] =
		pOther->GetAbsVelocity().Length2D();
	m_nNextSpeedSample = ( m_nNextSpeedSample + 1 ) %
		ARRAYSIZE( m_flSpeedSamples );

	float flTotal = 0.0f;
	int nSamples = 0;
	for ( int i = 0; i < ARRAYSIZE( m_flSpeedSamples ); ++i )
	{
		if ( m_flSpeedSamples[i] != -1.0f )
		{
			flTotal += m_flSpeedSamples[i];
			++nSamples;
		}
	}

	if ( nSamples > 0 && flTotal / nSamples < m_flMinSpeed )
		m_OnMinSpeedReached.FireOutput( pOther, this );
}

CTriggerHurtFoF::CTriggerHurtFoF()
	: m_flOriginalDamage( 0.0f ),
	  m_flDamage( 0.0f ),
	  m_flDamageCap( 20.0f ),
	  m_flLastDmgTime( 0.0f ),
	  m_flDmgResetTime( 0.0f ),
	  m_bitsDamageInflict( 0 ),
	  m_damageModel( DAMAGEMODEL_NORMAL ),
	  m_bNoDmgForce( false ),
	  m_bOnlyKicked( false ),
	  m_nTeamFilter( 0 ),
	  m_hLastKickedEntity( NULL )
{
}

void CTriggerHurtFoF::Spawn()
{
	BaseClass::Spawn();
	InitTrigger();

	m_flOriginalDamage = m_flDamage;
	m_hLastKickedEntity = NULL;
	ChangeTeam( m_nTeamFilter );

	// These two retail additions make burning volumes non-walkable and allow
	// lethal volumes to receive physics props even when the map omitted flag 8.
	if ( m_bitsDamageInflict & DMG_BURN )
		AddEFlags( EFL_DONTWALKON );
	if ( m_flDamage >= 100.0f )
		AddSpawnFlags( SF_TRIGGER_ALLOW_PHYSICS );

	SetNextThink( TICK_NEVER_THINK );
	SetThink( NULL );
}

void CTriggerHurtFoF::RadiationThink()
{
	Vector vecMins;
	Vector vecMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
	CBasePlayer *pPlayer = static_cast< CBasePlayer * >(
		UTIL_FindClientInPVS( vecMins, vecMaxs ) );
	if ( pPlayer )
	{
		float flRange = CollisionProp()->CalcDistanceFromPoint(
			pPlayer->WorldSpaceCenter() );
		pPlayer->NotifyNearbyRadiationSource( flRange * 3.0f );
	}

	const float flDeltaTime = gpGlobals->curtime - m_flLastDmgTime;
	if ( flDeltaTime >= 0.5f )
		HurtAllTouchers( flDeltaTime );
	SetNextThink( gpGlobals->curtime + 0.25f );
}

void CTriggerHurtFoF::Touch( CBaseEntity *pOther )
{
	CPhysicsProp *pPhysicsProp = dynamic_cast< CPhysicsProp * >( pOther );
	if ( pPhysicsProp && pPhysicsProp->IsOnFire() )
	{
		return;
	}

	if ( m_pfnThink == NULL )
	{
		SetThink( &CTriggerHurtFoF::HurtThink );
		SetNextThink( gpGlobals->curtime );
	}
}

void CTriggerHurtFoF::EndTouch( CBaseEntity *pOther )
{
	if ( PassesTriggerFilters( pOther ) )
	{
		const EHANDLE hOther = pOther;
		if ( !m_hurtEntities.HasElement( hOther ) )
			HurtEntity( pOther, m_flDamage * 0.5f );
		m_hLastKickedEntity = NULL;
	}

	BaseClass::EndTouch( pOther );
}

void CTriggerHurtFoF::HurtThink()
{
	if ( HurtAllTouchers( 0.5f ) < 1 )
		SetThink( NULL );
	else
		SetNextThink( gpGlobals->curtime + 0.5f );
}

bool CTriggerHurtFoF::HurtEntity( CBaseEntity *pOther, float flDamage )
{
	if ( !pOther || !pOther->m_takedamage ||
		!PassesTriggerFilters( pOther ) || pOther->IsMarkedForDeletion() )
	{
		return false;
	}

	CFoF_Player *pPlayer = ToFoFPlayer( pOther );
	if ( pPlayer && pPlayer->IsObserver() )
		return false;

	if ( pPlayer && m_nTeamFilter > 0 &&
		pPlayer->GetTeamNumber() == m_nTeamFilter )
	{
		return false;
	}

	bool bKicked = false;
	CBaseEntity *pAttacker = this;
	if ( m_bOnlyKicked )
	{
		if ( !pPlayer )
			return false;

		pAttacker = pPlayer->GetFoFKicker();
		if ( !pAttacker || m_hLastKickedEntity.Get() == pOther )
			return false;

		bKicked = true;
	}

	flDamage = clamp( flDamage, 0.0f, 500.0f );
	CBreakableProp *pBreakable = dynamic_cast< CBreakableProp * >( pOther );
	if ( pBreakable && flDamage >= pBreakable->GetHealth() )
	{
		CTakeDamageInfo info;
		info.SetAttacker( GetWorldEntity() );
		pBreakable->Event_Killed( info );
		return false;
	}

	if ( bKicked )
		flDamage *= 2.0f;

	if ( flDamage < 0.0f )
	{
		pOther->TakeHealth( -flDamage, m_bitsDamageInflict );
	}
	else
	{
		const Vector vecCenter = CollisionProp()->WorldSpaceCenter();
		Vector vecDamagePosition;
		pOther->CollisionProp()->CalcNearestPoint(
			vecCenter, &vecDamagePosition );

		CTakeDamageInfo info( this, pAttacker, flDamage,
			m_bitsDamageInflict );
		info.SetDamagePosition( vecDamagePosition );
		if ( m_bNoDmgForce )
			info.SetDamageForce( vec3_origin );
		else
			GuessDamageForce( &info, vecDamagePosition - vecCenter,
				vecDamagePosition );

		pOther->TakeDamage( info );
	}

	if ( pPlayer )
		m_OnHurtPlayer.FireOutput( pOther, this );
	else
		m_OnHurt.FireOutput( pOther, this );

	m_hurtEntities.AddToTail( EHANDLE( pOther ) );
	if ( pPlayer )
		m_hLastKickedEntity = pOther;
	return true;
}

int CTriggerHurtFoF::HurtAllTouchers( float flDeltaTime )
{
	int nHurtCount = 0;
	const float flDamage = m_flDamage * flDeltaTime;
	m_flLastDmgTime = gpGlobals->curtime;
	m_hurtEntities.RemoveAll();

	touchlink_t *pRoot = static_cast< touchlink_t * >(
		GetDataObject( TOUCHLINK ) );
	if ( pRoot )
	{
		for ( touchlink_t *pLink = pRoot->nextLink;
			pLink && pLink != pRoot; pLink = pLink->nextLink )
		{
			CBaseEntity *pTouched = pLink->entityTouched;
			if ( pTouched && HurtEntity( pTouched, flDamage ) )
				++nHurtCount;
		}
	}

	if ( m_damageModel == DAMAGEMODEL_DOUBLE_FORGIVENESS )
	{
		if ( nHurtCount == 0 )
		{
			if ( gpGlobals->curtime > m_flDmgResetTime )
				m_flDamage = m_flOriginalDamage;
		}
		else
		{
			m_flDamage = MIN( m_flDamage * 2.0f, m_flDamageCap );
			m_flDmgResetTime = gpGlobals->curtime + 3.0f;
		}
	}

	return nHurtCount;
}

