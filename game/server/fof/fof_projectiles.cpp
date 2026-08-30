//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-side FoF thrown and fired projectiles.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_projectiles.h"
#include "basecombatweapon.h"
#include "movevars_shared.h"
#include "physics.h"
#include "decals.h"
#include "basegrenade_shared.h"
#include "Sprite.h"
#include "IEffects.h"
#include "soundent.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_player_statistics.h"
#include "BasePropDoor.h"
#include "doors.h"
#include "effect_dispatch_data.h"
#include "explode.h"
#include "particle_system.h"
#include "particle_parse.h"
#include "props.h"
#include "recipientfilter.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pszFoFProjectileHitBody =
	"Weapon_Crossbow.BoltHitBody";
static const char *s_pszFoFProjectileHitWorld =
	"Weapon_Crossbow.BoltHitWorld";
static const char *s_pszFoFProjectileSkewer =
	"Weapon_Crossbow.BoltSkewer";

static void FoFPrecacheThrownMeleeProjectile(
	CBaseCombatCharacter *pProjectile,
	const char *pszModel,
	bool bPrecacheGlow,
	bool bPrecacheModelTwice )
{
	pProjectile->PrecacheScriptSound( s_pszFoFProjectileHitBody );
	pProjectile->PrecacheScriptSound( s_pszFoFProjectileHitWorld );
	pProjectile->PrecacheScriptSound( s_pszFoFProjectileSkewer );
	pProjectile->PrecacheModel( pszModel );
	if ( bPrecacheGlow )
		pProjectile->PrecacheModel( "sprites/light_glow02_noz.vmt" );
	if ( bPrecacheModelTwice )
		pProjectile->PrecacheModel( pszModel );
}

static void FoFSpawnThrownMeleeProjectile(
	CBaseCombatCharacter *pProjectile,
	const char *pszModel,
	float flGravityScale,
	bool bSetCollisionBounds )
{
	pProjectile->SetModel( pszModel );
	pProjectile->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_DEFAULT );
	if ( bSetCollisionBounds )
	{
		UTIL_SetSize( pProjectile,
			Vector( -1.0f, -1.0f, -1.0f ),
			Vector( 1.0f, 1.0f, 1.0f ) );
	}
	pProjectile->SetSolid( SOLID_VPHYSICS );
	pProjectile->SetGravity( sv_gravity.GetFloat() * flGravityScale );
}

static bool FoFCreateThrownMeleeVPhysics(
	CBaseCombatCharacter *pProjectile )
{
	pProjectile->VPhysicsInitNormal(
		SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

static unsigned int FoFThrownMeleeSolidMask(
	const CBaseCombatCharacter *pProjectile )
{
	return ( pProjectile->CBaseCombatCharacter::PhysicsSolidMaskForEntity() |
		CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

static CBaseCombatCharacter *FoFCreateThrownMeleeProjectile(
	const char *pszClassname,
	const Vector &vecOrigin,
	const QAngle &angAngles,
	CBasePlayer *pOwner )
{
	CBaseCombatCharacter *pProjectile = dynamic_cast< CBaseCombatCharacter * >(
		CreateEntityByName( pszClassname ) );
	if ( !pProjectile )
		return NULL;

	UTIL_SetOrigin( pProjectile, vecOrigin );
	pProjectile->SetAbsAngles( angAngles );
	pProjectile->Spawn();
	pProjectile->SetOwnerEntity( pOwner );
	return pProjectile;
}

static void FoFCreateThrownWeaponPickup(
	CBaseCombatCharacter *pProjectile,
	const char *pszWeaponClassname,
	Vector &vecImpactDirection,
	bool bKnifePickup )
{
	Vector vecForward;
	AngleVectors( pProjectile->GetAbsAngles(), &vecForward );
	VectorNormalize( vecForward );
	vecImpactDirection = vecForward;

	Vector vecOrigin;
	if ( bKnifePickup )
	{
		vecOrigin = pProjectile->GetAbsOrigin() + vecForward * 2.0f;
	}
	else
	{
		vecOrigin = pProjectile->GetAbsOrigin() - vecForward * 20.0f;
		vecOrigin.z += 10.0f;
	}

	CBaseCombatWeapon *pWeapon = dynamic_cast< CBaseCombatWeapon * >(
		CBaseEntity::Create(
			pszWeaponClassname, vecOrigin,
			pProjectile->GetAbsAngles(), NULL ) );
	if ( pWeapon )
	{
		pWeapon->AddEFlags( EFL_NO_PHYSCANNON_INTERACTION );
		pWeapon->RemoveSolidFlags( FSOLID_TRIGGER );
		pWeapon->AddEffects( EF_ITEM_BLINK );

		if ( bKnifePickup )
		{
			trace_t traceWood;
			UTIL_TraceLine( vecOrigin, vecOrigin + vecForward * 7.0f,
				MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE,
				&traceWood );
			const surfacedata_t *pSurface = physprops->GetSurfaceData(
				traceWood.surface.surfaceProps );
			if ( pSurface && pSurface->game.material == CHAR_TEX_WOOD )
				pWeapon->SetMoveType( MOVETYPE_NONE );
		}

		pWeapon->SetThink( &CBaseEntity::SUB_Remove );
		pWeapon->SetNextThink( gpGlobals->curtime + 15.0f );
		// The original thrown axe/knife/machete paths refresh the message
		// after converting the new weapon into a timed pickup.
		pWeapon->SendFoFWorldGlow();
	}

	UTIL_Remove( pProjectile );
}

static void FoFThrownMeleeTouch(
	CBaseCombatCharacter *pProjectile,
	CBaseEntity *pOther,
	int iDamage,
	const char *pszWeaponClassname,
	Vector &vecImpactDirection,
	bool bKnifePickup )
{
	if ( !pOther )
		return;
	if ( !pOther->IsSolid() ||
		pOther->IsSolidFlagSet( FSOLID_VOLUME_CONTENTS ) )
	{
		return;
	}

	CBaseEntity *pOwner = pProjectile->GetOwnerEntity();
	if ( !pOwner )
		return;
	CFoF_Player *pPlayerOwner = ToFoFPlayer( pOwner );

	if ( pOther->GetMoveParent() )
	{
		pProjectile->SetAbsVelocity( vec3_origin );
		pProjectile->SetTouch( NULL );
		UTIL_Remove( pProjectile );
		return;
	}

	trace_t traceHit;
	traceHit = pProjectile->GetTouchTrace();
	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		CFoF_Player *pTargetPlayer = ToFoFPlayer( pOther );
		if ( pPlayerOwner && pTargetPlayer &&
			FoFPlayersAreEnemies( pPlayerOwner, pTargetPlayer ) )
		{
			FoFRecordAccuracyHit( pPlayerOwner, (float)iDamage );
		}
		Vector vecDirection = pProjectile->GetAbsVelocity();
		VectorNormalize( vecDirection );
		const bool bPlayerHitNPC = pOwner->IsPlayer() &&
			pOther->IsNPC();
		const int iDamageType = bPlayerHitNPC ? DMG_NEVERGIB :
			( DMG_SLASH | DMG_NEVERGIB );

		ClearMultiDamage();
		CTakeDamageInfo damageInfo(
			pProjectile, pOwner,
			(float)iDamage, iDamageType );
		if ( bPlayerHitNPC )
			damageInfo.AdjustPlayerDamageInflictedForSkillLevel();
		CalculateMeleeDamageForce(
			&damageInfo, vecDirection, traceHit.endpos, 0.7f );
		damageInfo.SetDamagePosition( traceHit.endpos );
		pOther->DispatchTraceAttack(
			 damageInfo, vecDirection, &traceHit );
		ApplyMultiDamage();
		UTIL_ImpactTrace( &traceHit, DMG_SLASH );

		if ( pOther->GetCollisionGroup() ==
			COLLISION_GROUP_BREAKABLE_GLASS )
		{
			return;
		}

		pProjectile->SetAbsVelocity( vec3_origin );
		pProjectile->EmitSound( s_pszFoFProjectileHitBody );
		pProjectile->SetTouch( NULL );
		pProjectile->SetThink( NULL );
		FoFCreateThrownWeaponPickup(
			pProjectile, pszWeaponClassname,
			vecImpactDirection, bKnifePickup );
		return;
	}

	if ( pOther->GetMoveType() == MOVETYPE_NONE &&
		( traceHit.surface.flags & SURF_SKY ) == 0 )
	{
		pProjectile->EmitSound( s_pszFoFProjectileHitWorld );
		UTIL_ImpactTrace( &traceHit, DMG_SLASH );
		pProjectile->SetMoveType( MOVETYPE_NONE );
		pProjectile->AddEffects( EF_NODRAW );
		pProjectile->SetTouch( NULL );
		pProjectile->SetAbsVelocity( vec3_origin );
		FoFCreateThrownWeaponPickup(
			pProjectile, pszWeaponClassname,
			vecImpactDirection, bKnifePickup );
		return;
	}

	if ( ( traceHit.surface.flags & SURF_SKY ) == 0 )
		UTIL_ImpactTrace( &traceHit, DMG_SLASH );
	UTIL_Remove( pProjectile );
}

LINK_ENTITY_TO_CLASS( thrown_axe, CAxeBolt );
LINK_ENTITY_TO_CLASS( thrown_knife, CKnifeBolt );
LINK_ENTITY_TO_CLASS( thrown_machete, CMacheteBolt );

IMPLEMENT_SERVERCLASS_ST( CAxeBolt, DT_AxeBolt )
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST( CKnifeBolt, DT_KnifeBolt )
END_SEND_TABLE()

BEGIN_DATADESC( CAxeBolt )
END_DATADESC()

BEGIN_DATADESC( CKnifeBolt )
END_DATADESC()

BEGIN_DATADESC( CMacheteBolt )
END_DATADESC()

CAxeBolt::CAxeBolt() : m_iDamage( 0 ), m_vecImpactDirection( vec3_origin )
{
}

void CAxeBolt::Precache( void )
{
	FoFPrecacheThrownMeleeProjectile(
		this, "models/weapons/w_axe.mdl", false, true );
}

void CAxeBolt::Spawn( void )
{
	Precache();
	FoFSpawnThrownMeleeProjectile(
		this, "models/weapons/w_axe.mdl", 0.0025f, true );
	SetTouch( &CAxeBolt::BoltTouch );
}

bool CAxeBolt::CreateVPhysics( void )
{
	return FoFCreateThrownMeleeVPhysics( this );
}

unsigned int CAxeBolt::PhysicsSolidMaskForEntity() const
{
	return FoFThrownMeleeSolidMask( this );
}

void CAxeBolt::BoltTouch( CBaseEntity *pOther )
{
	FoFThrownMeleeTouch( this, pOther, m_iDamage, "weapon_axe",
		m_vecImpactDirection, false );
}

CAxeBolt *CAxeBolt::BoltCreate(
	const Vector &vecOrigin, const QAngle &angAngles,
	int iDamage, CBasePlayer *pOwner )
{
	CAxeBolt *pBolt = static_cast< CAxeBolt * >(
		FoFCreateThrownMeleeProjectile(
			"thrown_axe", vecOrigin, angAngles, pOwner ) );
	if ( pBolt )
	{
		pBolt->m_iDamage = iDamage;
		FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	}
	return pBolt;
}

CKnifeBolt::CKnifeBolt() : m_iDamage( 0 ), m_vecImpactDirection( vec3_origin )
{
}

void CKnifeBolt::Precache( void )
{
	FoFPrecacheThrownMeleeProjectile(
		this, "models/weapons/w_knife.mdl", true, true );
}

void CKnifeBolt::Spawn( void )
{
	Precache();
	FoFSpawnThrownMeleeProjectile(
		this, "models/weapons/w_knife.mdl",
		0.0016666667f, false );
	SetTouch( &CKnifeBolt::BoltTouch );
}

bool CKnifeBolt::CreateVPhysics( void )
{
	return FoFCreateThrownMeleeVPhysics( this );
}

unsigned int CKnifeBolt::PhysicsSolidMaskForEntity() const
{
	return FoFThrownMeleeSolidMask( this );
}

void CKnifeBolt::BoltTouch( CBaseEntity *pOther )
{
	FoFThrownMeleeTouch( this, pOther, m_iDamage, "weapon_knife",
		m_vecImpactDirection, true );
}

CKnifeBolt *CKnifeBolt::BoltCreate(
	const Vector &vecOrigin, const QAngle &angAngles,
	int iDamage, CBasePlayer *pOwner )
{
	CKnifeBolt *pBolt = static_cast< CKnifeBolt * >(
		FoFCreateThrownMeleeProjectile(
			"thrown_knife", vecOrigin, angAngles, pOwner ) );
	if ( pBolt )
	{
		pBolt->m_iDamage = iDamage;
		FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	}
	return pBolt;
}

CMacheteBolt::CMacheteBolt() : m_iDamage( 0 ), m_vecImpactDirection( vec3_origin )
{
}

void CMacheteBolt::Precache( void )
{
	FoFPrecacheThrownMeleeProjectile(
		this, "models/weapons/w_machete.mdl", false, false );
}

void CMacheteBolt::Spawn( void )
{
	Precache();
	FoFSpawnThrownMeleeProjectile(
		this, "models/weapons/w_machete.mdl", 0.0025f, false );
	SetTouch( &CMacheteBolt::BoltTouch );
}

bool CMacheteBolt::CreateVPhysics( void )
{
	return FoFCreateThrownMeleeVPhysics( this );
}

unsigned int CMacheteBolt::PhysicsSolidMaskForEntity() const
{
	return FoFThrownMeleeSolidMask( this );
}

void CMacheteBolt::BoltTouch( CBaseEntity *pOther )
{
	FoFThrownMeleeTouch(
		this, pOther, m_iDamage, "weapon_machete",
		m_vecImpactDirection, false );
}

CMacheteBolt *CMacheteBolt::BoltCreate(
	const Vector &vecOrigin, const QAngle &angAngles,
	int iDamage, CBasePlayer *pOwner )
{
	CMacheteBolt *pBolt = static_cast< CMacheteBolt * >(
		FoFCreateThrownMeleeProjectile(
			"thrown_machete", vecOrigin, angAngles, pOwner ) );
	if ( pBolt )
	{
		pBolt->m_iDamage = iDamage;
		FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	}
	return pBolt;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-side dynamite projectile entities.
//
//=============================================================================//

#define FOF_DYNAMITE_MODEL        "models/weapons/w_dynamite.mdl"
#define FOF_DYNAMITE_BLACK_MODEL  "models/weapons/w_dynamite_black.mdl"
#define FOF_DYNAMITE_YELLOW_MODEL "models/weapons/w_dynamite_yellow.mdl"
#define FOF_DYNAMITE_GLOW         "sprites/glow1.vmt"

static const float s_flFoFDynamiteRestitution = 0.2f;

static int FoFDynamiteType( CBaseEntity *pEntity )
{
	const char *pszClassname = pEntity ? pEntity->GetClassname() : NULL;
	if ( pszClassname && !Q_stricmp( pszClassname, "dynamite_black" ) )
		return 1;
	if ( pszClassname && !Q_stricmp( pszClassname, "dynamite_yellow" ) )
		return 2;
	return 0;
}

static const char *FoFDynamiteModel( CBaseEntity *pEntity )
{
	switch ( FoFDynamiteType( pEntity ) )
	{
	case 1:
		return FOF_DYNAMITE_BLACK_MODEL;
	case 2:
		return FOF_DYNAMITE_YELLOW_MODEL;
	default:
		return FOF_DYNAMITE_MODEL;
	}
}

static void FoFDynamiteDamage( CBaseEntity *pEntity,
	float &flDamage, float &flRadius )
{
	switch ( FoFDynamiteType( pEntity ) )
	{
	case 1:
		flDamage = 225.0f;
		flRadius = 275.0f;
		break;
	case 2:
		flDamage = 85.0f;
		flRadius = 175.0f;
		break;
	default:
		flDamage = 140.0f;
		flRadius = 240.0f;
		break;
	}
}

class CFoFDynamiteProjectile : public CBaseGrenade
{
	DECLARE_CLASS( CFoFDynamiteProjectile, CBaseGrenade );
	DECLARE_DATADESC();

public:
	~CFoFDynamiteProjectile();

	void Spawn() OVERRIDE;
	void OnRestore() OVERRIDE;
	void UpdateOnRemove() OVERRIDE;
	void Precache() OVERRIDE;
	bool CreateVPhysics() OVERRIDE;
	int OnTakeDamage( const CTakeDamageInfo &info ) OVERRIDE;
	void VPhysicsUpdate( IPhysicsObject *pPhysics ) OVERRIDE;
	void OnPhysGunPickup(
		CBasePlayer *pPhysGunUser, PhysGunPickup_t reason ) OVERRIDE;

	void CreateEffects();
	void SetTimer( float flDetonateDelay, float flWarnDelay );
	void DelayThink();
	void InputSetTimer( inputdata_t &inputdata );
	void Defuse();

private:
	CHandle< CSprite > m_hFuseGlow;
	float m_flNextFuseSoundTime;
	bool m_bInSolid;
	bool m_bYellowImpacted;
};

LINK_ENTITY_TO_CLASS( dynamite, CFoFDynamiteProjectile );
LINK_ENTITY_TO_CLASS( dynamite_black, CFoFDynamiteProjectile );
LINK_ENTITY_TO_CLASS( dynamite_yellow, CFoFDynamiteProjectile );

BEGIN_DATADESC( CFoFDynamiteProjectile )
	DEFINE_FIELD( m_hFuseGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flNextFuseSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_bInSolid, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bYellowImpacted, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC( DelayThink ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTimer", InputSetTimer ),
END_DATADESC()

CFoFDynamiteProjectile::~CFoFDynamiteProjectile()
{
	if ( m_hFuseGlow )
		UTIL_Remove( m_hFuseGlow );
}

void CFoFDynamiteProjectile::Spawn()
{
	Precache();
	SetModel( FoFDynamiteModel( this ) );

	float flDamage;
	float flRadius;
	FoFDynamiteDamage( this, flDamage, flRadius );
	SetDamage( flDamage );
	SetDamageRadius( flRadius );

	m_takedamage = DAMAGE_NO;
	m_iHealth = 1;
	SetSize( -Vector( 6, 6, 6 ), Vector( 6, 6, 6 ) );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	CreateVPhysics();

	EmitSound( "Weapon_Dynamite.Burn" );
	m_flNextFuseSoundTime = gpGlobals->curtime + 100.0f;
	m_bInSolid = false;
	m_bYellowImpacted = false;
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	BaseClass::Spawn();
}

void CFoFDynamiteProjectile::OnRestore()
{
	if ( m_flDetonateTime > 0.0f )
		CreateEffects();
	BaseClass::OnRestore();
}

void CFoFDynamiteProjectile::UpdateOnRemove()
{
	StopSound( "Weapon_Dynamite.Burn" );
	BaseClass::UpdateOnRemove();
}

void CFoFDynamiteProjectile::Precache()
{
	PrecacheModel( FOF_DYNAMITE_MODEL );
	PrecacheModel( FOF_DYNAMITE_BLACK_MODEL );
	PrecacheModel( FOF_DYNAMITE_YELLOW_MODEL );
	PrecacheModel( FOF_DYNAMITE_GLOW );
	PrecacheScriptSound( "Weapon_Dynamite.Burn" );
	BaseClass::Precache();
}

bool CFoFDynamiteProjectile::CreateVPhysics()
{
	VPhysicsInitNormal( SOLID_BBOX, 0, false );
	return true;
}

int CFoFDynamiteProjectile::OnTakeDamage( const CTakeDamageInfo &info )
{
	VPhysicsTakeDamage( info );
	return BaseClass::OnTakeDamage( info );
}

void CFoFDynamiteProjectile::CreateEffects()
{
	if ( m_hFuseGlow )
		UTIL_Remove( m_hFuseGlow );

	m_hFuseGlow = CSprite::SpriteCreate(
		FOF_DYNAMITE_GLOW, GetLocalOrigin(), false );
	if ( !m_hFuseGlow )
		return;

	const int nAttachment = LookupAttachment( "fuse1" );
	m_hFuseGlow->FollowEntity( this );
	m_hFuseGlow->SetAttachment( this, nAttachment );
	m_hFuseGlow->SetTransparency(
		kRenderTransAdd, 150, 150, 150, 100,
		kRenderFxFlickerFast );
	m_hFuseGlow->SetScale( 0.2f );
}

void CFoFDynamiteProjectile::SetTimer(
	float flDetonateDelay, float flWarnDelay )
{
	m_flDetonateTime = gpGlobals->curtime + flDetonateDelay;
	m_flWarnAITime = gpGlobals->curtime + flWarnDelay;
	SetThink( &CFoFDynamiteProjectile::DelayThink );
	SetNextThink( gpGlobals->curtime );
	CreateEffects();
}

void CFoFDynamiteProjectile::InputSetTimer( inputdata_t &inputdata )
{
	const float flTimer = inputdata.value.Float();
	SetTimer( flTimer, flTimer - 1.5f );
}

void CFoFDynamiteProjectile::Defuse()
{
	// The shot-detonation pass skips inert (MOVETYPE_NONE) sticks.
	SetMoveType( MOVETYPE_NONE );
	SetThink( NULL );
	m_flDetonateTime = gpGlobals->curtime + 99.0f;
	StopSound( "Weapon_Dynamite.Burn" );
	if ( m_hFuseGlow )
	{
		UTIL_Remove( m_hFuseGlow );
		m_hFuseGlow = NULL;
	}
}

bool FoFDefuseDynamite( CBaseEntity *pEntity )
{
	CFoFDynamiteProjectile *pDynamite =
		dynamic_cast< CFoFDynamiteProjectile * >( pEntity );
	if ( !pDynamite )
		return false;

	pDynamite->Defuse();
	return true;
}

void CFoFDynamiteProjectile::OnPhysGunPickup(
	CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	SetThrower( pPhysGunUser );
	SetTimer( 2.5f, 1.25f );
	StopSound( "Weapon_Dynamite.Burn" );
	m_flNextFuseSoundTime = gpGlobals->curtime + 10.0f;
	m_bHasWarnedAI = true;
	BaseClass::OnPhysGunPickup( pPhysGunUser, reason );
}

void CFoFDynamiteProjectile::DelayThink()
{
	if ( gpGlobals->curtime > m_flDetonateTime )
	{
		Detonate();
		return;
	}

	if ( !m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime )
	{
		CSoundEnt::InsertSound(
			SOUND_DANGER, GetAbsOrigin(), 400, 1.5f, this );
		m_bHasWarnedAI = true;
	}

	Vector vecFuseOrigin = GetAbsOrigin();
	QAngle angFuse = GetAbsAngles();
	const int nAttachment = LookupAttachment( "fuse1" );
	if ( nAttachment > 0 )
		GetAttachment( nAttachment, vecFuseOrigin, angFuse );
	g_pEffects->Smoke( vecFuseOrigin, 0, 2.0f, 20.0f );

	if ( m_hFuseGlow )
	{
		const float flRemaining = m_flDetonateTime - gpGlobals->curtime;
		const int nRed = RoundFloatToInt( RemapValClamped(
			flRemaining, 2.0f, 0.0f, 100.0f, 255.0f ) );
		const int nGreen = RoundFloatToInt( RemapValClamped(
			flRemaining, 2.0f, 0.0f, 100.0f, 25.0f ) );
		const int nBlue = RoundFloatToInt( RemapValClamped(
			flRemaining, 2.0f, 0.0f, 0.0f, 75.0f ) );
		m_hFuseGlow->SetTransparency(
			kRenderTransAdd, nRed, nGreen, nBlue, 175,
			kRenderFxFlickerFast );
	}

	if ( gpGlobals->curtime > m_flNextFuseSoundTime )
	{
		EmitSound( "Weapon_Dynamite.Burn" );
		m_flNextFuseSoundTime = gpGlobals->curtime + 10.0f;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

class CFoFDynamiteCollisionFilter : public CTraceFilterEntitiesOnly
{
public:
	DECLARE_CLASS_NOBASE( CFoFDynamiteCollisionFilter );

	CFoFDynamiteCollisionFilter(
		const IHandleEntity *pPassEntity,
		int nOldCollisionGroup,
		int nNewCollisionGroup ) :
		m_pPassEntity( pPassEntity ),
		m_nOldCollisionGroup( nOldCollisionGroup ),
		m_nNewCollisionGroup( nNewCollisionGroup )
	{
	}

	bool ShouldHitEntity(
		IHandleEntity *pHandleEntity, int contentsMask ) OVERRIDE
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEntity ) )
			return false;

		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;
		if ( g_pGameRules->ShouldCollide(
			m_nOldCollisionGroup, pEntity->GetCollisionGroup() ) )
		{
			return false;
		}
		return g_pGameRules->ShouldCollide(
			m_nNewCollisionGroup, pEntity->GetCollisionGroup() );
	}

private:
	const IHandleEntity *m_pPassEntity;
	int m_nOldCollisionGroup;
	int m_nNewCollisionGroup;
};

void CFoFDynamiteProjectile::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate( pPhysics );

	Vector vecVelocity;
	AngularImpulse angVelocity;
	pPhysics->GetVelocity( &vecVelocity, &angVelocity );

	CFoFDynamiteCollisionFilter filter(
		this, GetCollisionGroup(), COLLISION_GROUP_NONE );
	trace_t trace;
	UTIL_TraceLine(
		GetAbsOrigin(),
		GetAbsOrigin() + vecVelocity * gpGlobals->frametime,
		CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID,
		&filter, &trace );

	if ( trace.startsolid )
	{
		if ( !m_bInSolid )
		{
			vecVelocity *= -s_flFoFDynamiteRestitution;
			pPhysics->SetVelocity( &vecVelocity, NULL );
		}
		m_bInSolid = true;
		return;
	}

	m_bInSolid = false;
	if ( !trace.DidHit() )
	{
		if ( FoFDynamiteType( this ) == 2 && !m_bYellowImpacted &&
			pPhysics->GetContactPoint( NULL, NULL ) )
		{
			m_bYellowImpacted = true;
			m_flDetonateTime = gpGlobals->curtime + 0.4f;
		}
		return;
	}

	CTakeDamageInfo damageInfo(
		this, GetThrower(), pPhysics->GetMass() * vecVelocity,
		GetAbsOrigin(), 0.1f, DMG_CRUSH );
	trace.m_pEnt->TakeDamage( damageInfo );
	vecVelocity = -2.0f * trace.plane.normal *
		DotProduct( vecVelocity, trace.plane.normal ) + vecVelocity;
	vecVelocity *= s_flFoFDynamiteRestitution;
	angVelocity *= -0.5f;
	pPhysics->SetVelocity( &vecVelocity, &angVelocity );

	if ( FoFDynamiteType( this ) == 2 && !m_bYellowImpacted )
	{
		m_bYellowImpacted = true;
		m_flDetonateTime = gpGlobals->curtime + 0.4f;
	}
}

static const char *s_pszFoFArrowModel =
	"models/weapons/bowarrow_bolt.mdl";
static const char *s_pszFoFArrowHitBody =
	"Weapon_Crossbow.BoltHitBody";
static const char *s_pszFoFArrowHitWorld =
	"Weapon_Crossbow.BoltHitWorld";
static const char *s_pszFoFArrowHitWorldWood =
	"Weapon_Crossbow.BoltHitWorldWood";
static const char *s_pszFoFArrowSkewer =
	"Weapon_Crossbow.BoltSkewer";
static const char *s_pszFoFNormalArrowFly =
	"Weapon_Bow.ArrowFly";
static const char *s_pszFoFBlackArrowFly =
	"Weapon_BowBlack.ArrowFly";
static const char *s_pszFoFXArrowExplosion =
	"TNTBow.Explosion";

class CFoFArrowTriggerEnumerator : public IEntityEnumerator
{
public:
	explicit CFoFArrowTriggerEnumerator( CBowarrowBolt *pArrow )
		: m_pArrow( pArrow )
	{
	}

	bool EnumEntity( IHandleEntity *pHandleEntity ) OVERRIDE
	{
		CBaseEntity *pEntity = gEntList.GetBaseEntity(
			pHandleEntity->GetRefEHandle() );
		if ( !pEntity || pEntity->IsSolid() ||
			!pEntity->IsEFlagSet( EFL_DONTWALKON ) ||
			!FClassnameIs( pEntity, "trigger_hurt_fof" ) )
		{
			return true;
		}

		m_pArrow->EnableEnhancedDamage();
		return true;
	}

private:
	CBowarrowBolt *m_pArrow;
};

static void FoFSendBowResult( CBasePlayer *pOwner, int nResult )
{
	if ( !pOwner )
		return;

	CSingleUserRecipientFilter filter( pOwner );
	UserMessageBegin( filter, "HitBow" );
		WRITE_BYTE( nResult );
	MessageEnd();
}

static CParticleSystem *FoFCreateParticleSystem(
	const char *pszEffectName, const Vector &vecOrigin,
	CBaseEntity *pParent, float flLifetime )
{
	CParticleSystem *pParticle = dynamic_cast< CParticleSystem * >(
		CreateEntityByName( "info_particle_system" ) );
	if ( !pParticle )
		return NULL;

	pParticle->KeyValue( "start_active", "1" );
	pParticle->KeyValue( "effect_name", pszEffectName );
	pParticle->SetAbsOrigin( vecOrigin );
	if ( pParent )
	{
		pParticle->SetParent( pParent );
		pParticle->SetLocalOrigin( vec3_origin );
	}
	DispatchSpawn( pParticle );
	pParticle->Activate();
	if ( flLifetime > 0.0f )
	{
		pParticle->SetThink( &CBaseEntity::SUB_Remove );
		pParticle->SetNextThink( gpGlobals->curtime + flLifetime );
	}
	return pParticle;
}

static void FoFDispatchBoltImpact(
	const trace_t &traceHit, const Vector &vecDirection, int nEntIndex )
{
	CEffectData data;
	data.m_vOrigin = traceHit.endpos;
	data.m_vNormal = vecDirection;
	data.m_nEntIndex = nEntIndex;
	DispatchEffect( "BoltImpact", data );
}

static void FoFAttachBowArrow(
	CBowarrowBolt *pArrow, CBaseEntity *pParent, int nAttachment )
{
	pArrow->SetOwnerEntity( pParent );
	pArrow->SetParent( pParent, nAttachment );
	pArrow->SetMoveType( MOVETYPE_NONE );
	pArrow->AddSolidFlags( FSOLID_NOT_SOLID );
	pArrow->SUB_StartFadeOut( 20.0f, true );
}

static bool FoFCanAttachBowArrowToProp( CBaseEntity *pEntity )
{
	CPhysicsProp *pPhysicsProp = dynamic_cast< CPhysicsProp * >( pEntity );
	if ( pPhysicsProp && pPhysicsProp->HasSpawnFlags( 0x80000 ) )
		return true;

	return dynamic_cast< CDynamicProp * >( pEntity ) != NULL ||
		dynamic_cast< CBasePropDoor * >( pEntity ) != NULL ||
		dynamic_cast< CBaseDoor * >( pEntity ) != NULL;
}

LINK_ENTITY_TO_CLASS( arrow, CBowarrowBolt );
LINK_ENTITY_TO_CLASS( arrow_black, CBowarrowBolt );
LINK_ENTITY_TO_CLASS( x_arrow, CXArrow );

IMPLEMENT_SERVERCLASS_ST( CBowarrowBolt, DT_BowarrowBolt )
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST( CXArrow, DT_XArrow )
END_SEND_TABLE()

BEGIN_DATADESC( CBowarrowBolt )
	DEFINE_ENTITYFUNC( BoltTouch ),
	DEFINE_THINKFUNC( ArrowWhizSoundThink ),
END_DATADESC()

BEGIN_DATADESC( CXArrow )
END_DATADESC()

CBowarrowBolt::CBowarrowBolt()
	: m_bBlack( false )
	, m_iDamage( 0 )
	, m_flNextFlybyTime( 0.0f )
	, m_hTrail( NULL )
	, m_bEnhancedDamage( false )
	, m_bTrailCreated( false )
{
}

void CBowarrowBolt::Precache( void )
{
	PrecacheModel( s_pszFoFArrowModel );
	PrecacheParticleSystem( "arrow_trail_normal" );
	PrecacheParticleSystem( "arrow_trail_black" );
	PrecacheModel( s_pszFoFArrowModel );
	PrecacheScriptSound( s_pszFoFArrowHitBody );
	PrecacheScriptSound( s_pszFoFArrowHitWorld );
	PrecacheScriptSound( s_pszFoFArrowHitWorldWood );
	PrecacheScriptSound( s_pszFoFArrowSkewer );
	PrecacheScriptSound( s_pszFoFNormalArrowFly );
	PrecacheScriptSound( s_pszFoFBlackArrowFly );
}

void CBowarrowBolt::Spawn( void )
{
	Precache();
	SetModel( s_pszFoFArrowModel );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this,
		Vector( -1.0f, -1.0f, -1.0f ),
		Vector( 1.0f, 1.0f, 1.0f ) );
	SetSolid( SOLID_BBOX );
	SetGravity( 2.7f );
	UpdateWaterState();
	SetTouch( &CBowarrowBolt::BoltTouch );

	m_flNextFlybyTime = 0.0f;
	m_bEnhancedDamage = false;
	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );
	TraceEnhancementTriggers( vecForward, 100.0f );
	m_bTrailCreated = false;
	m_flNextFlybyTime = gpGlobals->curtime + 0.1f;
	SetThink( &CBowarrowBolt::ArrowWhizSoundThink );
	SetNextThink( gpGlobals->curtime + 0.015f );
}

bool CBowarrowBolt::CreateVPhysics( void )
{
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CBowarrowBolt::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() |
		CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CBowarrowBolt::CreateTrail( void )
{
	if ( m_bTrailCreated )
		return;

	m_hTrail = FoFCreateParticleSystem(
		m_bBlack ? "arrow_trail_black" : "arrow_trail_normal",
		GetAbsOrigin(), this, 0.0f );
	m_bTrailCreated = true;
}

void CBowarrowBolt::StopTrail( void )
{
	if ( m_hTrail )
	{
		m_hTrail->SetThink( &CBaseEntity::SUB_Remove );
		m_hTrail->SetNextThink( gpGlobals->curtime );
		m_hTrail = NULL;
	}
}

void CBowarrowBolt::TraceEnhancementTriggers(
	const Vector &vecDirection, float flDistance )
{
	Ray_t ray;
	const Vector vecOrigin = GetAbsOrigin();
	ray.Init( vecOrigin - vecDirection * flDistance,
		vecOrigin + vecDirection * flDistance );
	CFoFArrowTriggerEnumerator enumerator( this );
	enginetrace->EnumerateEntities( ray, true, &enumerator );
}

void CBowarrowBolt::EnableEnhancedDamage( void )
{
	if ( m_bEnhancedDamage )
		return;

	FoFCreateParticleSystem(
		"burning_gib_01", GetAbsOrigin(), this, 4.0f );
	m_bEnhancedDamage = true;
}

void CBowarrowBolt::ArrowWhizSoundThink( void )
{
	CreateTrail();
	Vector vecDirection = GetAbsVelocity();
	const float flSpeed = VectorNormalize( vecDirection );
	if ( flSpeed > 0.0f )
	{
		QAngle angVelocity;
		VectorAngles( vecDirection, angVelocity );
		SetAbsAngles( angVelocity );
		if ( !m_bEnhancedDamage )
		{
			TraceEnhancementTriggers(
				vecDirection, flSpeed * gpGlobals->frametime );
		}
	}

	if ( gpGlobals->curtime > m_flNextFlybyTime )
	{
		trace_t traceFlyby;
		UTIL_TraceLine( GetAbsOrigin(),
			GetAbsOrigin() + vecDirection * 60.0f,
			MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE,
			&traceFlyby );

		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || !pPlayer->IsAlive() )
				continue;

			if ( pPlayer->WorldSpaceCenter().DistTo(
				GetAbsOrigin() ) < 60.0f &&
				traceFlyby.m_pEnt != pPlayer )
			{
				CSingleUserRecipientFilter filter( pPlayer );
				CBaseEntity::EmitSound( filter, entindex(),
					FClassnameIs( this, "arrow" ) ?
					s_pszFoFNormalArrowFly : s_pszFoFBlackArrowFly );
				m_flNextFlybyTime = gpGlobals->curtime + 0.15f;
				break;
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.01f );
}

void CBowarrowBolt::BoltTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsSolid() ||
		pOther->IsSolidFlagSet(
			FSOLID_NOT_SOLID | FSOLID_VOLUME_CONTENTS ) )
	{
		return;
	}

	CFoF_Player *pPlayerOwner = ToFoFPlayer( GetOwnerEntity() );
	if ( !pPlayerOwner )
	{
		SetAbsVelocity( vec3_origin );
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	CFoF_Player *pTarget = ToFoFPlayer( pOther );
	if ( pOther->m_takedamage == DAMAGE_NO && !pTarget )
	{
		StopTrail();
		FoFSendBowResult( pPlayerOwner, 0 );
		trace_t traceHit;
		traceHit = GetTouchTrace();
		if ( pOther->GetMoveType() == MOVETYPE_NONE )
		{
			if ( traceHit.surface.flags & SURF_SKY )
			{
				UTIL_Remove( this );
				return;
			}

			EmitSound(
				traceHit.surface.surfaceProps == 0x0E ||
				traceHit.surface.surfaceProps == 0x13 ?
				s_pszFoFArrowHitWorldWood : s_pszFoFArrowHitWorld );
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 0.5f );
			SetMoveType( MOVETYPE_NONE );
			Vector vecForward;
			AngleVectors( GetAbsAngles(), &vecForward );
			VectorNormalize( vecForward );
			FoFDispatchBoltImpact( traceHit, vecForward, 0 );
			UTIL_ImpactTrace( &traceHit, DMG_BULLET );
			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			return;
		}

		if ( !( traceHit.surface.flags & SURF_SKY ) )
			UTIL_ImpactTrace( &traceHit, DMG_BULLET );
		UTIL_Remove( this );
		return;
	}

	trace_t traceHit;
	traceHit = GetTouchTrace();
	Vector vecDirection = GetAbsVelocity();
	const float flSpeed = VectorNormalize( vecDirection );
	trace_t traceSwept;
	UTIL_TraceLine(
		GetAbsOrigin() - vecDirection * flSpeed * gpGlobals->frametime,
		GetAbsOrigin() + vecDirection * flSpeed * gpGlobals->frametime,
		MASK_SHOT, this, COLLISION_GROUP_NONE, &traceSwept );
	if ( traceSwept.m_pEnt &&
		( traceSwept.m_pEnt->IsPlayer() || traceSwept.m_pEnt->IsNPC() ) )
	{
		traceHit = traceSwept;
	}
	if ( pTarget && traceHit.m_pEnt != pOther )
		traceHit = GetTouchTrace();

	const bool bEnemyPlayer = pTarget &&
		FoFPlayersAreEnemies( pPlayerOwner, pTarget );

	float flDamage = RemapValClamped(
		pPlayerOwner->GetAbsOrigin().DistTo( GetAbsOrigin() ),
		0.0f, 2000.0f, (float)m_iDamage, (float)m_iDamage * 2.0f );
	if ( traceHit.hitgroup == HITGROUP_HEAD && m_iDamage == 45 )
		flDamage = 55.0f;
	if ( m_bEnhancedDamage )
		flDamage *= 1.25f;
	if ( bEnemyPlayer )
	{
		FoFSendBowResult( pPlayerOwner, 1 );
		FoFRecordAccuracyHit( pPlayerOwner, flDamage );
	}

	ClearMultiDamage();
	CTakeDamageInfo damageInfo(
		this, pPlayerOwner, flDamage, DMG_SLASH );
	CalculateMeleeDamageForce(
		&damageInfo, vecDirection, traceHit.endpos, 0.7f );
	damageInfo.SetDamagePosition( traceHit.endpos );
	pOther->DispatchTraceAttack(
		damageInfo, vecDirection, &traceHit );
	ApplyMultiDamage();

	if ( pOther->GetCollisionGroup() ==
		COLLISION_GROUP_BREAKABLE_GLASS )
	{
		return;
	}

	StopTrail();
	if ( bEnemyPlayer )
		pTarget->EmitSound( s_pszFoFArrowHitBody );
	SetAbsVelocity( vec3_origin );
	SetTouch( NULL );

	if ( pTarget && pTarget->IsAlive() )
	{
		SetAbsOrigin( traceHit.endpos );
		const char *pszAttachment =
			traceHit.hitgroup == HITGROUP_HEAD || traceHit.hitgroup == 8 ?
			"anim_attachment_head" : "chest";
		FoFAttachBowArrow(
			this, pTarget, pTarget->LookupAttachment( pszAttachment ) );
		return;
	}

	if ( FoFCanAttachBowArrowToProp( pOther ) )
	{
		SetAbsOrigin( traceHit.endpos );
		FoFAttachBowArrow( this, pOther, -1 );
		EmitSound( s_pszFoFArrowHitWorld );
		return;
	}

	trace_t traceWall;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecDirection * 128.0f,
		MASK_OPAQUE, pOther, COLLISION_GROUP_NONE, &traceWall );
	if ( traceWall.fraction != 1.0f &&
		( !traceWall.m_pEnt || !traceWall.m_pEnt->IsAlive() ) )
	{
		FoFDispatchBoltImpact(
			traceWall, vecDirection, traceWall.fraction != 1.0f );
	}
	UTIL_Remove( this );
}

CBowarrowBolt *CBowarrowBolt::BoltCreate(
	const Vector &vecOrigin, const QAngle &angAngles,
	int iDamage, bool bBlack, CBasePlayer *pOwner )
{
	CBowarrowBolt *pArrow = dynamic_cast< CBowarrowBolt * >(
		CreateEntityByName( bBlack ? "arrow_black" : "arrow" ) );
	if ( !pArrow )
		return NULL;

	pArrow->m_bBlack = bBlack;
	UTIL_SetOrigin( pArrow, vecOrigin );
	pArrow->SetAbsAngles( angAngles );
	pArrow->Spawn();
	pArrow->SetOwnerEntity( pOwner );
	pArrow->m_iDamage = iDamage;
	FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	return pArrow;
}

CXArrow::CXArrow()
	: m_iDamage( 0 )
	, m_bTrailCreated( false )
{
}

void CXArrow::Precache( void )
{
	PrecacheModel( s_pszFoFArrowModel );
	PrecacheParticleSystem( "arrow_trail_dotted" );
	PrecacheParticleSystem( "xbow_fx_dust" );
	PrecacheScriptSound( s_pszFoFXArrowExplosion );
}

void CXArrow::Spawn( void )
{
	Precache();
	SetModel( s_pszFoFArrowModel );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this,
		Vector( -1.0f, -1.0f, -1.0f ),
		Vector( 1.0f, 1.0f, 1.0f ) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.6f );
	UpdateWaterState();
	SetTouch( &CXArrow::BoltTouch );
	m_bTrailCreated = false;
	CreateTrail();
}

bool CXArrow::CreateVPhysics( void )
{
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
	return true;
}

unsigned int CXArrow::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() |
		CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CXArrow::CreateTrail( void )
{
	if ( m_bTrailCreated )
		return;

	FoFCreateParticleSystem(
		"arrow_trail_dotted", GetAbsOrigin(), this, 4.0f );
	m_bTrailCreated = true;
}

void CXArrow::BoltTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsSolid() ||
		pOther->IsSolidFlagSet(
			FSOLID_NOT_SOLID | FSOLID_VOLUME_CONTENTS ) )
	{
		return;
	}

	CBaseEntity *pOwnerEntity = GetOwnerEntity();
	if ( !pOwnerEntity )
	{
		SetAbsVelocity( vec3_origin );
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
	if ( !pOwnerEntity->IsPlayer() )
		return;
	CBasePlayer *pOwner = ToBasePlayer( pOwnerEntity );
	CFoF_Player *pFoFOwner = ToFoFPlayer( pOwner );
	CFoF_Player *pTarget = ToFoFPlayer( pOther );
	if ( pFoFOwner && pTarget &&
		FoFPlayersAreEnemies( pFoFOwner, pTarget ) )
	{
		FoFRecordAccuracyHit( pFoFOwner, 70.0f );
	}

	const Vector vecOrigin = GetAbsOrigin();
	CPASFilter filter( vecOrigin );
	te->Explosion( filter, -1.0f, &vecOrigin, g_sModelIndexFireball,
		1.0f, 25, TE_EXPLFLAG_NONE, 350, 60, NULL, 'C' );
	CTakeDamageInfo damageInfo(
		this, pOwner, 70.0f, DMG_BLAST );
	RadiusDamage( damageInfo, vecOrigin, 350.0f,
		CLASS_NONE, NULL );
	EmitSound( s_pszFoFXArrowExplosion );
	FoFCreateParticleSystem(
		"xbow_fx_dust", vecOrigin, NULL, 2.0f );

	SetAbsVelocity( vec3_origin );
	SetTouch( NULL );
	UTIL_Remove( this );
}

CXArrow *CXArrow::BoltCreate(
	const Vector &vecOrigin, const QAngle &angAngles,
	int iDamage, CBasePlayer *pOwner )
{
	CXArrow *pArrow = dynamic_cast< CXArrow * >(
		CreateEntityByName( "x_arrow" ) );
	if ( !pArrow )
		return NULL;

	UTIL_SetOrigin( pArrow, vecOrigin );
	pArrow->SetAbsAngles( angAngles );
	pArrow->Spawn();
	pArrow->SetOwnerEntity( pOwner );
	pArrow->m_iDamage = iDamage;
	FoFRecordAccuracyShots( ToFoFPlayer( pOwner ), 1 );
	return pArrow;
}
