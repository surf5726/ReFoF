#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "fof/fof_player_activities.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/fof_base_revolver.h"
#include "hl2mp_weapon_parse.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "fof/fof_combat_effects.h"
#else
#include "fof/fof_course_mode.h"
#include "fof/fof_player_statistics.h"
#include "npcevent.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
static ConVar fof_sv_recoilamount(
	"fof_sv_recoilamount", "0.8", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Recoil amount for handguns", true, 0.8f, true, 0.8f );
#else
extern ConVar fof_sv_recoilamount;
#endif

static float FoFRevolverWalkFactor( const CFoF_Player *pOwner )
{
	return pOwner ? pOwner->GetFoFWalkFactor() : 0.0f;
}

static const int s_FoFRevolverAlternateProtocolActivities[][2] =
{
	{ 0x469, 0x458 },
	{ 0x46F, 0x459 },
	{ 0x70E, 0x70F },
	{ 0x46A, 0x45A },
	{ 0x472, 0x45B },
	{ 0x47D, 0x45C },
	{ 0x481, 0x45C },
	{ 0x47F, 0x45D },
	{ 0x483, 0x45D },
	{ 0x48B, 0x3FF },
	{ 0x48E, 0x3FF },
	{ 0x474, 0x460 },
	{ 0x48D, 0x3FF },
	{ 0x490, 0x3FF },
	{ 0x71A, 0x721 },
};

static acttable_t s_FoFRevolverAlternateActivities[
	ARRAYSIZE( s_FoFRevolverAlternateProtocolActivities )];
static bool s_bFoFRevolverAlternateActivitiesInitialized;

static void FoFInitializeRevolverAlternateActivities()
{
	if ( s_bFoFRevolverAlternateActivitiesInitialized )
		return;

	for ( int i = 0;
		i < ARRAYSIZE( s_FoFRevolverAlternateProtocolActivities ); ++i )
	{
		Activity baseActivity = ACT_INVALID;
		Activity weaponActivity = ACT_INVALID;
		FoFTranslateNetworkActivity(
			s_FoFRevolverAlternateProtocolActivities[i][0], baseActivity );
		FoFTranslateNetworkActivity(
			s_FoFRevolverAlternateProtocolActivities[i][1], weaponActivity );

		s_FoFRevolverAlternateActivities[i].baseAct = baseActivity;
		s_FoFRevolverAlternateActivities[i].weaponAct = weaponActivity;
		s_FoFRevolverAlternateActivities[i].required = false;
	}

	s_bFoFRevolverAlternateActivitiesInitialized = true;
}

void CFoFBaseRevolver::WeaponIdle( void )
{
	BaseClass::WeaponIdle();
}

float CFoFBaseRevolver::GetFireRate( void )
{
	return 1.5f;
}

acttable_t *CFoFBaseRevolver::ActivityListAlternate( void )
{
	FoFInitializeRevolverAlternateActivities();
	return s_FoFRevolverAlternateActivities;
}

int CFoFBaseRevolver::ActivityListAlternateCount( void )
{
	return ARRAYSIZE( s_FoFRevolverAlternateActivities );
}

int CFoFBaseRevolver::FoFWeaponID( void ) const
{
	return 2;
}

bool CFoFBaseRevolver::WeaponShouldBeLowered( void )
{
	return false;
}

CFoFBaseRevolver::CFoFBaseRevolver()
#ifdef CLIENT_DLL
	: m_flSoonestPrimaryAttack( 0.0f )
	, m_flLastAttackTime( 0.0f )
	, m_flAccuracyPenalty( 0.0f )
	, m_flDeployDualGunIn( 0.0f )
	, m_bDelayedReload( false )
	, m_bNeedPump( false )
	, m_bDouble( false )
	, m_nFoFRevolverReserved( 0 )
	, m_flTriggerHoldTime( 0.0f )
#endif
{
#ifndef CLIENT_DLL
	m_hFoFThrower = NULL;
	m_flSoonestPrimaryAttack = 0.0f;
	m_flLastAttackTime = 0.0f;
	m_flAccuracyPenalty = 0.0f;
	m_flDeployDualGunIn = 0.0f;
	m_bDelayedReload = false;
	m_bNeedPump = false;
	m_bDouble = false;
	m_nFoFRevolverReserved = 0;
	m_flTriggerHoldTime = 0.0f;
#endif
	m_bReloadsSingly = true;
	m_fMinRange1 = 0.0f;
	m_fMinRange2 = 0.0f;
	m_fMaxRange1 = 500.0f;
	m_fMaxRange2 = 200.0f;
	AddEffects( EF_ITEM_BLINK );
}

void CFoFBaseRevolver::PerformFoFReload( void )
{
	ReloadOneRoundClip();
}

void CFoFBaseRevolver::ReloadOneRoundClip()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner
#ifdef CLIENT_DLL
		|| !pOwner->IsAlive()
#endif
		|| pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
		m_iClip1 >= GetMaxClip1() )
	{
		return;
	}

	++m_iClip1;
}

void CFoFBaseRevolver::ReloadDeringerClip()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner
#ifdef CLIENT_DLL
		|| !pOwner->IsAlive()
#endif
		|| m_iClip1 >= GetMaxClip1() )
	{
		return;
	}

	const int nAmmo = pOwner->GetAmmoCount( m_iPrimaryAmmoType );
	if ( m_iClip1 == 0 && nAmmo > 1 )
		m_iClip1 = 2;
	if ( m_iClip1 == 1 && nAmmo > 0 )
		m_iClip1 = 1;
	if ( m_iClip1 == 0 && nAmmo == 1 )
		m_iClip1 = 1;
}

void CFoFBaseRevolver::ReloadTwoRoundClip()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner
#ifdef CLIENT_DLL
		|| !pOwner->IsAlive()
#endif
		)
	{
		return;
	}

	const int nAmmo = pOwner->GetAmmoCount( m_iPrimaryAmmoType );
	const int nMissing = GetMaxClip1() - m_iClip1;
	m_iClip1 = nAmmo < nMissing ? nAmmo : 2;
}

void CFoFBaseRevolver::ReloadFullClip()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner
#ifdef CLIENT_DLL
		|| !pOwner->IsAlive()
#endif
		)
	{
		return;
	}

	m_iClip1 = GetMaxClip1();
}

bool CFoFBaseRevolver::NeedsPumpAfterShot( void ) const
{
	const char *pszClassname =
		const_cast< CFoFBaseRevolver * >( this )->GetClassname();
	return pszClassname &&
		( !Q_stricmp( pszClassname, "weapon_deringer" ) ||
		  !Q_stricmp( pszClassname, "weapon_deringer2" ) );
}

int CFoFBaseRevolver::FinishStyle( void ) const
{
	const char *pszClassname =
		const_cast< CFoFBaseRevolver * >( this )->GetClassname();
	if ( !pszClassname )
		return 0;

	if ( !Q_stricmp( pszClassname, "weapon_deringer" ) ||
		 !Q_stricmp( pszClassname, "weapon_deringer2" ) )
	{
		return 1;
	}

	if ( !Q_stricmp( pszClassname, "weapon_hammerless" ) ||
		 !Q_stricmp( pszClassname, "weapon_hammerless2" ) ||
		 !Q_stricmp( pszClassname, "weapon_remington_army" ) ||
		 !Q_stricmp( pszClassname, "weapon_remington_army2" ) ||
		 !Q_stricmp( pszClassname, "weapon_schofield" ) ||
		 !Q_stricmp( pszClassname, "weapon_schofield2" ) )
	{
		return 2;
	}

	if ( !Q_stricmp( pszClassname, "weapon_ghostgun" ) ||
		 !Q_stricmp( pszClassname, "weapon_ghostgun2" ) ||
		 !Q_stricmp( pszClassname, "weapon_sawedoff_shotgun" ) ||
		 !Q_stricmp( pszClassname, "weapon_sawedoff_shotgun2" ) )
	{
		return 3;
	}

	return 0;
}

float CFoFBaseRevolver::GetViewModelDuration()
{
#ifdef CLIENT_DLL
	float flDuration = SequenceDuration();
#else
	float flDuration = GetViewModelSequenceDuration();
#endif
	if ( !IsFinite( flDuration ) || flDuration <= 0.0f )
		flDuration = 0.1f;
	return flDuration;
}

bool CFoFBaseRevolver::HasBothRevolvers( const CBasePlayer *pOwner ) const
{
	return pOwner && pOwner->HasDualActiveWeapons();
}

bool CFoFBaseRevolver::HasInventoryPair( const CBasePlayer *pOwner ) const
{
	if ( !pOwner )
		return false;

	if ( !CanDualWield() )
		return false;

	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		const CFoFBaseRevolver *pOther =
			dynamic_cast< const CFoFBaseRevolver * >( pOwner->GetWeapon( i ) );
		if ( !pOther || pOther == this ||
			!pOther->CanDualWield() ||
			pOther->IsSecondGun() == IsSecondGun() )
		{
			continue;
		}

		// FoF pairs physical hand leaves, not matching classnames.  A left
		// Deringer and a right Peacemaker are a valid dual inventory and still
		// need the fixed key-2/key-3 hand buckets.
		return true;
	}

	return false;
}

int CFoFBaseRevolver::GetSlot( void ) const
{
	const CBasePlayer *pOwner =
		dynamic_cast< const CBasePlayer * >( GetOwner() );

	// The shared script bucket is 2 (the player's key 3).  FoF only splits a
	// revolver family across keys 2/3 when both hand leaves actually exist in
	// the inventory.  Physical interoperability testing against the original
	// server and physical viewmodels confirm that the second leaf feeds
	// viewmodel index 1 (the visible left hand), while the first leaf feeds
	// viewmodel index 0 (the visible right hand).  Therefore key 2 selects
	// the second/left leaf and key 3 selects the first/right leaf.  A lone
	// revolver still follows its script bucket and remains on key 3.
	if ( HasInventoryPair( pOwner ) )
		return IsSecondGun() ? 1 : 2;

	return BaseClass::GetSlot();
}

CFoFBaseRevolver *CFoFBaseRevolver::GetOtherRevolver( CBasePlayer *pOwner ) const
{
	if ( !pOwner )
		return NULL;

	CBaseCombatWeapon *pFirst = pOwner->GetActiveWeapon1();
	CBaseCombatWeapon *pOther = pFirst == this ? pOwner->GetActiveWeapon2() : pFirst;
	return dynamic_cast< CFoFBaseRevolver * >( pOther );
}

float CFoFBaseRevolver::ApplyReloadRate( CFoF_Player *pOwner )
{
	float flRate = 1.0f;
	CBaseViewModel *pViewModel = pOwner ?
		pOwner->GetViewModel( IsSecondGun() ? 1 : 0 ) : NULL;
	if ( !pViewModel )
		return flRate;

	if ( pOwner->GetFoFHandStance() == 3 && CanFan() )
		// Set the stance rate from its baseline, rather than multiplying the
		// viewmodel's current value.  Reload phase transitions and prediction
		// replay may call this more than once for the same sequence; multiplying
		// here compounded 1.3x into 1.69x, 2.197x, and so on.
		flRate = 1.3f;
	else if ( pOwner->GetFoFHandStance() == 0 )
		flRate = 1.0f;
	else
		flRate = 1.15f;

	pViewModel->SetPlaybackRate( flRate );
	return flRate;
}

void CFoFBaseRevolver::SetReloadEndTime( CFoF_Player *pOwner )
{
	const float flRate = ApplyReloadRate( pOwner );
	const float flEndTime = gpGlobals->curtime + GetViewModelDuration() / flRate;

	m_flNextPrimaryAttack = flEndTime;
	m_flNextSecondaryAttack = flEndTime;
	m_flTimeWeaponIdle = flEndTime;
	if ( pOwner )
		pOwner->SetNextAttack( flEndTime );
}

void CFoFBaseRevolver::PresentMuzzleFlash(
	CFoF_Player *pOwner )
{
	if ( !pOwner )
		return;

	const bool bSecondGun = IsSecondGun();
	CBaseViewModel *pViewModel =
		pOwner->GetViewModel( bSecondGun ? 1 : 0, true );
	if ( pViewModel )
		pViewModel->DoMuzzleFlash();

	CBaseCombatWeapon *pActiveWeapon = bSecondGun ?
		pOwner->GetActiveWeapon2() : pOwner->GetActiveWeapon1();
	if ( pActiveWeapon == this )
		DoMuzzleFlash();
}

bool CFoFBaseRevolver::Deploy( void )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
#ifndef CLIENT_DLL
	if ( pOwner && m_hFoFThrower.Get() != pOwner )
		m_hFoFThrower = pOwner;
#endif
#ifdef CLIENT_DLL
	CBaseViewModel *pExistingViewModel = pOwner ?
		pOwner->GetViewModel( m_nViewModelIndex, true ) : NULL;
	const bool bAlreadyActiveInOwnHand = pOwner &&
		( IsSecondGun() ?
			pOwner->GetActiveWeapon2() == this :
			pOwner->GetActiveWeapon1() == this );
	if ( bAlreadyActiveInOwnHand && pExistingViewModel &&
		pExistingViewModel->GetWeapon() == this &&
		pExistingViewModel->ShouldDraw() )
	{
		// Prediction must not replay the untouched hand's draw sequence when
		// the other hand selects a new weapon of the same classname.
		return true;
	}
#endif

	if ( pOwner && pOwner->IsAlive() )
	{
	#ifndef CLIENT_DLL
		pOwner->m_nPlayerInfo &= ~0x100;
	#endif
		pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pOwner->RecalculateWeaponSpeed();
#ifndef CLIENT_DLL
		if ( CanFan() ||
			( CanDualWield() && pOwner->GetFoFHandStance() == 1 ) )
		{
			pOwner->m_nPlayerInfo |= 0x10;
		}
#endif
		m_bInReload = false;
		CBaseViewModel *pViewModel =
			pOwner->GetViewModel( m_nViewModelIndex, true );
		if ( pViewModel &&
#ifdef CLIENT_DLL
			!pViewModel->IsDormant() &&
#endif
			pViewModel->GetModelPtr() )
		{
			pViewModel->m_nBody =
				m_bFiresUnderwater ? 1 : 0;
			pViewModel->SetBodygroup( 1, 0 );
		}
		m_flTriggerHoldTime = 0.0f;
	}

	const bool bDeployed = BaseClass::Deploy();
	return bDeployed;
}

#ifndef CLIENT_DLL
void CFoFBaseRevolver::DefaultTouch( CBaseEntity *pOther )
{
	BaseClass::DefaultTouch( pOther );

	CFoF_Player *pThrower = m_hFoFThrower.Get();
	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if ( !pThrower || !pOther || pOther == pThrower ||
		WeaponState() != WEAPON_NOT_CARRIED || !pPhysics )
	{
		return;
	}

	Vector vecPhysicsVelocity;
	pPhysics->GetVelocity( &vecPhysicsVelocity, NULL );
	const float flSpeed = vecPhysicsVelocity.Length();
	if ( flSpeed < 350.0f )
	{
		RemoveSolidFlags( FSOLID_TRIGGER );
		SetCollisionGroup( COLLISION_GROUP_WEAPON );
		m_hFoFThrower = NULL;
		return;
	}

	if ( !pOther->IsPlayer() )
		return;

	trace_t traceHit;
	traceHit = GetTouchTrace();
	Vector vecDirection = GetAbsVelocity();
	VectorNormalize( vecDirection );

	const float flWeightDamage = RemapValClamped(
		static_cast< float >( FoFWeaponWeight() ),
		1.0f, 5.0f, 25.0f, 50.0f );
	const float flAmmoScale = RemapValClamped(
		static_cast< float >( m_iClip1 ) /
			static_cast< float >( GetMaxClip1() ),
		0.0f, 1.0f, 0.5f, 1.0f );
	const float flSpeedScale = RemapValClamped(
		flSpeed, 300.0f, 1000.0f, 0.3f, 1.0f );
	const float flDamage = flWeightDamage * flAmmoScale * flSpeedScale;
	CFoF_Player *pTarget = ToFoFPlayer( pOther );
	if ( pTarget && FoFPlayersAreEnemies( pThrower, pTarget ) )
		FoFRecordAccuracyHit( pThrower, flDamage );

	ClearMultiDamage();
	CTakeDamageInfo info(
		this, pThrower, flDamage,
		DMG_CLUB | DMG_PREVENT_PHYSICS_FORCE );
	info.SetWeapon( this );
	pOther->DispatchTraceAttack( info, vecDirection, &traceHit );
	ApplyMultiDamage();

	Vector vecRebound = pOther->GetAbsVelocity() * -0.1f;
	pPhysics->SetVelocity( &vecRebound, NULL );
	m_flUnlockTime = gpGlobals->curtime - 1.0f;
	m_hFoFThrower = NULL;
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	RemoveSolidFlags( FSOLID_TRIGGER );
}
#endif

bool CFoFBaseRevolver::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bInReload = false;

#ifndef CLIENT_DLL
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( pOwner && pOwner->IsAlive() &&
		( pOwner->GetFoFPlayerInfo() & 0x10 ) &&
		pOwner->HasDualActiveWeapons() )
	{
		CBaseCombatWeapon *pFirst = pOwner->GetActiveWeapon1();
		CBaseCombatWeapon *pOther =
			pFirst == this ? pOwner->GetActiveWeapon2() : pFirst;
		if ( pOther && !pOther->CanFan() &&
			( !pOther->CanDualWield() || pOwner->GetFoFHandStance() != 1 ) )
		{
			pOwner->m_nPlayerInfo &= ~0x10;
		}
	}
#endif

	return BaseClass::Holster( pSwitchingTo );
}

void CFoFBaseRevolver::ItemHolsterFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && !pOwner->IsAlive() )
		return;

	if ( pOwner &&
		( pOwner->GetActiveWeapon1() == this ||
		  pOwner->GetActiveWeapon2() == this ) )
	{
		return;
	}

	if ( m_bInReload && m_iClip1 < GetMaxClip1() )
		m_bInReload = false;
}

#ifndef CLIENT_DLL
void CFoFBaseRevolver::Operator_HandleAnimEvent(
	animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	if ( pEvent && pEvent->event == EVENT_WEAPON_RELOAD_FILL_CLIP )
	{
		PerformFoFReload();
		return;
	}

	BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
}
#endif

void CFoFBaseRevolver::DryFire()
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
	m_flNextSecondaryAttack = m_flNextPrimaryAttack;
}

void CFoFBaseRevolver::WaterDryFire()
{
	WeaponSound( EMPTY );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
	m_flNextSecondaryAttack = m_flNextPrimaryAttack;
}

void CFoFBaseRevolver::HandleFireOnEmpty( void )
{
	DryFire();
}

void CFoFBaseRevolver::AlternateSightWeaponIdle( void )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetFoFHandStance() == 1 )
	{
		// The original client tail-calls the ordinary HL2MP idle
		// implementation when the alternate sight path is unavailable.  In
		// particular, right-hand aim stance 1 must not simply return: Volcanic
		// and the other alternate-idle revolvers otherwise leave their predicted
		// idle clock and viewmodel parity one command behind the server forever.
		BaseClass::WeaponIdle();
		return;
	}

	if ( !HasWeaponIdleTimeElapsed() )
		return;

	Activity idleActivity = ACT_VM_IDLE;
	if ( pOwner->GetFoFSightExpFactor() >= 0.75f )
	{
		// The original passes protocol activity 0xCA, named ACT_VM_SWINGHIT.
		// Its live model ID is 202 when a listen server initializes the shared
		// model cache first, but 201 in a remote-client process.  Resolve the
		// activity stored on this model so one DLL predicts both layouts.
		idleActivity = FoFModelActivity(
			this, "ACT_VM_SWINGHIT", ACT_VM_SWINGHIT );
	}

	// In the original client only Mares Leg, Remington Army,
	// Schofield and Volcanic override vtable slot 252 with this alternate
	// idle. Other revolvers, including Colt Navy, inherit the shared HL2MP
	// WeaponIdle implementation.
	SendWeaponAnim( idleActivity );
}

void CFoFBaseRevolver::PrimaryAttack( void )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	const float flNow = gpGlobals->curtime;
	if ( !pOwner || !pOwner->IsAlive() || pOwner->GetFoFCaptureInput() > 0.0f ||
		m_flSoonestPrimaryAttack > flNow || m_flNextSecondaryAttack > flNow )
	{
		return;
	}

#ifdef CLIENT_DLL
	if ( flNow + 3.0f > m_flLastAttackTime )
		FoFCreateMuzzleSmoke( pOwner, IsSecondGun() );
#endif

	if ( HasBothRevolvers( pOwner ) )
	{
		CFoFBaseRevolver *pOther = GetOtherRevolver( pOwner );
		if ( pOther && flNow > pOther->m_flNextPrimaryAttack )
			pOther->m_flNextPrimaryAttack = flNow + 0.25f;
	}

	PresentMuzzleFlash( pOwner );
	WeaponSound( m_bDouble ? WPN_DOUBLE : SINGLE );

	if ( UsesClipsForAmmo1() )
	{
		if ( !pOwner->PreventFoFLocalAmmoUse() )
			--m_iClip1;
	}

	SendWeaponAnim( m_bDouble && FinishStyle() == 3 ?
		ACT_VM_SECONDARYATTACK : ACT_VM_PRIMARYATTACK );

	const bool bDualFirst = HasBothRevolvers( pOwner ) && !IsSecondGun();
	pOwner->SetAnimation( static_cast< PLAYER_ANIM >( bDualFirst ? 6 : 5 ) );
	pOwner->DoAnimationEvent(
		bDualFirst ? PLAYERANIMEVENT_ATTACK_SECONDARY : PLAYERANIMEVENT_ATTACK_PRIMARY,
		0 );

	const int nPrimaryShots = FinishStyle() == 3 ? 18 : 1;
	float flPrimaryAperture =
		pOwner->GetFoFCrosshairAperture( IsSecondGun() ? 1 : 0 );
	if ( FinishStyle() == 3 )
		flPrimaryAperture *= 0.5f;
	CBasePlayer *pBaseOwner = pOwner;
	FireBulletsInfo_t primaryBulletInfo;
	primaryBulletInfo.m_iShots = nPrimaryShots;
	primaryBulletInfo.m_vecSrc = pOwner->Weapon_ShootPosition();
	primaryBulletInfo.m_vecDirShooting =
		pBaseOwner->GetAutoaimVector( AUTOAIM_10DEGREES );
	primaryBulletInfo.m_vecSpread.Init(
		flPrimaryAperture, flPrimaryAperture, flPrimaryAperture );
	primaryBulletInfo.m_flDistance = MAX_TRACE_LENGTH;
	primaryBulletInfo.m_iAmmoType = m_iPrimaryAmmoType;
	primaryBulletInfo.m_pAttacker = pOwner;
	primaryBulletInfo.m_bPrimaryAttack =
		pOwner->GetActiveWeapon1() == this;
#ifndef CLIENT_DLL
	const CHL2MPSWeaponInfo &primaryWeaponInfo = GetHL2MPWpnData();
	primaryBulletInfo.m_flDamage =
		static_cast< float >( primaryWeaponInfo.m_iPlayerDamage );
	primaryBulletInfo.m_iPlayerDamage =
		primaryWeaponInfo.m_iPlayerDamage;
#endif
	pOwner->FireBullets( primaryBulletInfo );

	const float flWalkRecoilScale = RemapValClamped(
		FoFRevolverWalkFactor( pOwner ), 0.0f, 1.0f, 1.0f, 1.35f );
	const float flPrimaryRecoilScale =
		flWalkRecoilScale * 1.5f * fof_sv_recoilamount.GetFloat();
	QAngle primaryPunch = PrimaryViewPunch();
	primaryPunch.x *= flPrimaryRecoilScale;
	primaryPunch.y *= flPrimaryRecoilScale;
	primaryPunch.z *= flPrimaryRecoilScale;
	pOwner->ViewPunch( primaryPunch );

	if ( m_flTriggerHoldTime == 0.0f )
		m_flTriggerHoldTime = flNow;
	m_flAccuracyPenalty += PrimaryPenalty();
	m_flLastAttackTime = flNow;

	float flDurationScale = 1.0f;
	if ( pOwner->GetFoFHandStance() == 1 )
	{
		flDurationScale = RemapValClamped(
			flNow - m_flTriggerHoldTime,
			0.0f,
			GetMaxClip1() * 0.33f,
			1.0f,
			0.85f );
	}

	const float flAttackEnd = flNow + GetViewModelDuration() * flDurationScale;
	m_flSoonestPrimaryAttack = flAttackEnd;
	m_flNextPrimaryAttack = flAttackEnd;
	m_flNextSecondaryAttack = flAttackEnd;
	m_bInReload = false;
	m_bDouble = false;
	if ( NeedsPumpAfterShot() )
		m_bNeedPump = true;
}

void CFoFBaseRevolver::SecondaryAttack( void )
{
	if ( FinishStyle() == 3 )
		return;

	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	const float flNow = gpGlobals->curtime;
	if ( pOwner->GetFoFCaptureInput() > 0.0f ||
		m_flNextPrimaryAttack > flNow || m_flNextSecondaryAttack > flNow ||
		m_flSoonestPrimaryAttack > flNow )
	{
		return;
	}

#ifdef CLIENT_DLL
	if ( flNow + 3.0f > m_flLastAttackTime )
		FoFCreateMuzzleSmoke( pOwner, IsSecondGun() );
#endif

	WeaponSound( SINGLE );
	PresentMuzzleFlash( pOwner );
	if ( UsesClipsForAmmo1() )
	{
		if ( !pOwner->PreventFoFLocalAmmoUse() )
			--m_iClip1;
	}

	const int nHandStance = pOwner->GetFoFHandStance();
	if ( nHandStance == 1 )
	{
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		pOwner->SetAnimation( PLAYER_ATTACK1 );
		pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );
	}
	else
	{
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
		pOwner->SetAnimation( static_cast< PLAYER_ANIM >( 6 ) );
		pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_SECONDARY, 0 );
	}

	float flSecondaryAperture =
		pOwner->GetFoFCrosshairAperture( IsSecondGun() ? 1 : 0 );
	if ( pOwner->GetFoFSightExpFactor() < 0.5f )
		flSecondaryAperture = 0.5f;
	CBasePlayer *pBaseOwner = pOwner;
	FireBulletsInfo_t secondaryBulletInfo;
	secondaryBulletInfo.m_iShots = 1;
	secondaryBulletInfo.m_vecSrc = pOwner->Weapon_ShootPosition();
	secondaryBulletInfo.m_vecDirShooting =
		pBaseOwner->GetAutoaimVector( AUTOAIM_10DEGREES );
	secondaryBulletInfo.m_vecSpread.Init(
		flSecondaryAperture, flSecondaryAperture, flSecondaryAperture );
	secondaryBulletInfo.m_flDistance = MAX_TRACE_LENGTH;
	secondaryBulletInfo.m_iAmmoType = m_iPrimaryAmmoType;
	secondaryBulletInfo.m_pAttacker = pOwner;
	secondaryBulletInfo.m_bPrimaryAttack =
		pOwner->GetActiveWeapon1() == this;
	const CHL2MPSWeaponInfo &weaponInfo = GetHL2MPWpnData();
	float flSecondaryDamage = (float)weaponInfo.m_iPlayerDamage;
	if ( nHandStance != 1 )
		flSecondaryDamage *= 0.85f;
	secondaryBulletInfo.m_flDamage = flSecondaryDamage;
	secondaryBulletInfo.m_iPlayerDamage = (int)flSecondaryDamage;
	pOwner->FireBullets( secondaryBulletInfo );

	QAngle secondaryPunch = SecondaryViewPunch();
	if ( nHandStance != 1 )
	{
		const float flSecondaryRecoilScale =
			fof_sv_recoilamount.GetFloat() * 2.35f;
		secondaryPunch.x *= flSecondaryRecoilScale;
		secondaryPunch.y *= flSecondaryRecoilScale;
		secondaryPunch.z *= flSecondaryRecoilScale;
	}
	pOwner->ViewPunch( secondaryPunch );

	m_flAccuracyPenalty += 2.0f * SecondaryPenalty();
	if ( m_flTriggerHoldTime == 0.0f )
		m_flTriggerHoldTime = flNow;

	float flDurationScale = 1.0f;
	if ( nHandStance == 3 )
	{
		flDurationScale = RemapValClamped(
			flNow - m_flTriggerHoldTime,
			0.0f,
			GetMaxClip1() * 0.33f,
			1.0f,
			0.7f );
	}

	const float flAttackEnd = flNow + GetViewModelDuration() * flDurationScale;
	m_flNextPrimaryAttack = flAttackEnd;
	m_flNextSecondaryAttack = flAttackEnd;
	pOwner->SetNextAttack( flAttackEnd );
	m_flLastAttackTime = flNow;
	m_flSoonestPrimaryAttack = flAttackEnd;
	m_bInReload = false;
}

void CFoFBaseRevolver::ItemPostFrame( void )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() || !pOwner->GetActiveWeapon() )
		return;

	const float flNow = gpGlobals->curtime;
	const int nButtons = pOwner->m_nButtons;
	const bool bHasBoth = HasBothRevolvers( pOwner );
	const bool bVisible = !IsEffectActive( EF_NODRAW );

	if ( !( pOwner->m_nButtons & IN_ATTACK ) )
		m_flTriggerHoldTime = 0.0f;

	if ( m_flDeployDualGunIn > 0.0f && flNow > m_flDeployDualGunIn )
	{
		CFoFBaseRevolver *pOther = GetOtherRevolver( pOwner );
		if ( pOther )
		{
#ifdef CLIENT_DLL
			CBaseViewModel *pOtherViewModel =
				pOwner->GetViewModel( pOther->m_nViewModelIndex, true );
			const bool bOtherAlreadyVisible =
				pOtherViewModel &&
				pOtherViewModel->GetWeapon() == pOther &&
				pOtherViewModel->ShouldDraw();
			if ( !bOtherAlreadyVisible )
				pOther->Deploy();
#else
			pOther->Deploy();
#endif
		}
		m_flDeployDualGunIn = 0.0f;
	}

	if ( m_bNeedPump && m_flNextPrimaryAttack <= flNow )
	{
		Pump();
		return;
	}

	// FoF advances the reload state from the primary-attack clock.  The
	// original client calls the virtual Reload method for every subsequent
	// round/phase, so derived weapons can preserve their own reload sequence.
	if ( m_bInReload && m_flNextPrimaryAttack <= flNow )
	{
		if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 ||
			( m_iClip1 > 0 && !( nButtons & IN_RELOAD ) &&
				( nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) ) ||
			m_iClip1 >= GetMaxClip1() )
		{
			FinishReload();
		}
		else
		{
			Reload();
		}
		return;
	}

	// The authoritative server routes a single revolver's primary + secondary
	// chord through SecondaryAttack in every hand stance, including weapons
	// such as the Hammerless that report CanFan() == false.  Stance 1 still
	// uses the primary animation inside SecondaryAttack, but its server-side
	// recoil is the unscaled secondary punch and its accuracy impulse is twice
	// the secondary penalty.  Restricting this gate by stance or CanFan made
	// ADS fire predict the ordinary PrimaryAttack path instead.
	const bool bSecondaryChordGate =
		( ( pOwner->GetFoFPlayerInfo() & 0x400 ) || ( nButtons & IN_ATTACK2 ) ) &&
		!( nButtons & IN_RELOAD ) && bVisible && m_flNextSecondaryAttack <= flNow &&
		!bHasBoth;
	if ( bSecondaryChordGate && ( nButtons & IN_ATTACK ) )
	{
		if ( UsesClipsForAmmo1() && m_iClip1 <= 0 )
			DryFire();
		else if ( pOwner->GetWaterLevel() == 3 )
			WaterDryFire();
		else
			SecondaryAttack();
	}

	bool bWantsOrdinaryShot = false;
	if ( bVisible && !( nButtons & IN_RELOAD ) && m_flNextPrimaryAttack <= flNow )
	{
		bWantsOrdinaryShot = bHasBoth ?
			( IsSecondGun() ? ( nButtons & IN_ATTACK ) != 0 : ( nButtons & IN_ATTACK2 ) != 0 ) :
			( nButtons & IN_ATTACK ) != 0;
	}

	if ( bWantsOrdinaryShot )
	{
		if ( UsesClipsForAmmo1() && m_iClip1 <= 0 )
		{
			// FoF's sawed-off leaves override the original revolver virtual at
			// original client/server virtual.  Their ItemPostFrame path
			// tests m_iClip1 before dispatching PrimaryAttack and deliberately
			// falls through without DryFire when empty.  Replaying the generic
			// revolver empty-fire path here advances both attack clocks from the
			// predicted curtime while the original server leaves them untouched.
			if ( !UsesLoadedPrimaryInputPath() )
				DryFire();
		}
		else if ( pOwner->GetWaterLevel() == 3 )
			WaterDryFire();
		else if ( IsWhiskey() && bHasBoth && !IsSecondGun() &&
			( nButtons & IN_ATTACK2 ) )
			SecondaryAttack();
		else
			PrimaryAttack();
	}

	// The original client has a dedicated held reload + attack path.  Apart
	// from selecting the physical hand, this also determines when the server
	// starts a reload while both inputs remain down.
	if ( bVisible && !m_bInReload && ( nButtons & IN_RELOAD ) &&
		( nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		const int nReloadHand = ( nButtons & IN_ATTACK ) ? 1 : 2;
		StartReload( nReloadHand );
		return;
	}

	// The ordinary path is release-triggered. Using the press edge starts
	// full-cylinder reloads one input phase early and repeatedly restarts the
	// viewmodel sequence.
	if ( bVisible && !m_bInReload &&
		( pOwner->m_afButtonReleased & IN_RELOAD ) )
	{
		StartReload( 0 );
		return;
	}

	m_bFireOnEmpty = false;
	if ( !HasAnyAmmo() && m_flNextPrimaryAttack < flNow &&
		( GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY ) == 0 &&
		g_pGameRules->SwitchToNextBestWeapon( pOwner, this ) )
	{
		m_flNextPrimaryAttack = flNow + 0.3f;
		return;
	}

	if ( m_iClip1 <= 0 && ( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) == 0 &&
		m_flNextPrimaryAttack < flNow && !bHasBoth &&
		( pOwner->GetFoFPlayerInfo() & 0x20 ) && StartReload( 0 ) )
	{
		return;
	}

	WeaponIdle();
}

bool CFoFBaseRevolver::SelectReloadHand( CFoF_Player *pOwner, int nRequestedHand )
{
	CFoFBaseRevolver *pFirst = pOwner ?
		dynamic_cast< CFoFBaseRevolver * >( pOwner->GetActiveWeapon1() ) : NULL;
	CFoFBaseRevolver *pSecond = pOwner ?
		dynamic_cast< CFoFBaseRevolver * >( pOwner->GetActiveWeapon2() ) : NULL;
	if ( !pFirst || !pSecond )
		return false;

	CFoFBaseRevolver *pSelected = NULL;
	float flFirstScore = pFirst->m_iClip1 <= 0 ? 3.402823466e+38F :
		static_cast< float >( pFirst->GetMaxClip1() ) / pFirst->m_iClip1;
	float flSecondScore = pSecond->m_iClip1 <= 0 ? 3.402823466e+38F :
		static_cast< float >( pSecond->GetMaxClip1() ) / pSecond->m_iClip1;
	if ( pFirst->GetMaxClip1() > 30 )
		flFirstScore = 1.0f;
	if ( pSecond->GetMaxClip1() > 30 )
		flSecondScore = 1.0f;

	if ( nRequestedHand == 2 && pFirst->m_iClip1 < pFirst->GetMaxClip1() )
	{
		pSecond->Holster( NULL );
		pSelected = pFirst;
	}
	else if ( nRequestedHand == 1 && pSecond->m_iClip1 < pSecond->GetMaxClip1() )
	{
		pFirst->Holster( NULL );
		pSelected = pSecond;
	}
	else if ( flFirstScore > flSecondScore )
	{
		pSecond->Holster( NULL );
		pSelected = pFirst;
	}
	else
	{
		pFirst->Holster( NULL );
		pSelected = pSecond;
	}

	return pSelected == this;
}

bool CFoFBaseRevolver::StartReload( int nRequestedHand )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( m_bNeedPump || IsWhiskey() || !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 || m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	if ( HasBothRevolvers( pOwner ) && !SelectReloadHand( pOwner, nRequestedHand ) )
	{
		return false;
	}

	if ( FinishStyle() == 3 )
	{
		CBaseViewModel *pViewModel =
			pOwner->GetViewModel( m_nViewModelIndex );
		if ( pViewModel )
			pViewModel->SetBodygroup( 1, 1 );
	}

	if ( m_bReloadsSingly )
	{
		SendWeaponAnim( FoFShotgunReloadStartActivity( this ) );
		SetReloadEndTime( pOwner );
	}

	m_bInReload = true;
	return true;
}

bool CFoFBaseRevolver::ContinueReload()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() ||
		pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 || m_iClip1 >= GetMaxClip1() )
	{
		return false;
	}

	m_bInReload = true;
	WeaponSound( RELOAD );
	SendWeaponAnim( ACT_VM_RELOAD );

	pOwner->DoAnimationEvent(
		HasBothRevolvers( pOwner ) && IsSecondGun() ?
			static_cast< PlayerAnimEvent_t >( 5 ) : PLAYERANIMEVENT_RELOAD,
		0 );
	SetReloadEndTime( pOwner );
	return true;
}

bool CFoFBaseRevolver::Reload( void )
{
	return ContinueReload();
}

void CFoFBaseRevolver::FinishReload( void )
{
	switch ( FinishStyle() )
	{
	case 1:
		FinishDeringerReload();
		return;
	case 2:
		FinishFullClipReload();
		return;
	case 3:
		FinishTwoRoundReload();
		return;
	default:
		break;
	}

	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

#ifndef CLIENT_DLL
	FoFReportCourseStat( "reload_wep", pOwner );
#endif

	m_bInReload = false;
	SendWeaponAnim( FoFShotgunReloadFinishActivity( this ) );
	if ( HasBothRevolvers( pOwner ) )
		m_flDeployDualGunIn = gpGlobals->curtime + GetViewModelDuration();
	SetReloadEndTime( pOwner );
}

void CFoFBaseRevolver::FinishDeringerReload()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner )
		return;
#ifdef CLIENT_DLL
	if ( !pOwner->IsAlive() )
		return;
#endif

	m_bInReload = false;
	m_bNeedPump = true;
	if ( HasBothRevolvers( pOwner ) )
		m_flDeployDualGunIn = gpGlobals->curtime;

	m_flNextPrimaryAttack = gpGlobals->curtime;
	pOwner->SetNextAttack( m_flNextPrimaryAttack );
}

void CFoFBaseRevolver::FinishFullClipReload()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner )
		return;
#ifdef CLIENT_DLL
	if ( !pOwner->IsAlive() )
		return;
#else
	if ( pOwner->IsAlive() )
		FoFReportCourseStat( "reload_wep", pOwner );
#endif

	m_bInReload = false;
	m_bNeedPump = false;
	if ( HasBothRevolvers( pOwner ) )
		m_flDeployDualGunIn = gpGlobals->curtime;

	m_flNextPrimaryAttack = gpGlobals->curtime;
	pOwner->SetNextAttack( m_flNextPrimaryAttack );
}

void CFoFBaseRevolver::FinishTwoRoundReload()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;
#ifndef CLIENT_DLL
	FoFReportCourseStat( "reload_wep", pOwner );
#endif

	m_bInReload = false;
	m_bNeedPump = false;
	if ( HasBothRevolvers( pOwner ) )
		m_flDeployDualGunIn = gpGlobals->curtime;

	CBaseViewModel *pViewModel = pOwner->GetViewModel( m_nViewModelIndex );
	if ( pViewModel )
		pViewModel->SetBodygroup( 1, 0 );

	m_flNextPrimaryAttack = gpGlobals->curtime;
	pOwner->SetNextAttack( m_flNextPrimaryAttack );
}

void CFoFBaseRevolver::Pump()
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsAlive() )
		return;

	m_bNeedPump = false;
	if ( m_bDelayedReload )
	{
		m_bDelayedReload = false;
		StartReload( 0 );
	}

	WeaponSound( SPECIAL1 );
	SendWeaponAnim( FoFShotgunPumpActivity( this ) );
	const float flEndTime = gpGlobals->curtime + GetViewModelDuration();
	m_flNextPrimaryAttack = flEndTime;
	m_flNextSecondaryAttack = flEndTime;
	pOwner->SetNextAttack( flEndTime );
}

// memdbgon must be the last include file in a .cpp file!!!

struct FoFRevolverPunchDefinition_t
{
	const char *m_pszXSeed;
	const char *m_pszYSeed;
	float m_flXMin;
	float m_flXMax;
	float m_flYMin;
	float m_flYMax;
};

// Parameter order deliberately preserves reversed RandomFloat ranges such as
// (-3,-4). Source's shared RNG supports them and reordering changes prediction.
QAngle FoFBuildRevolverPunch(
	FoFRevolverPunchProfile_t profile, bool bSecondary )
{
	static const FoFRevolverPunchDefinition_t s_Primary[] =
	{
		{ "coltx",    "colty",    -3.0f, -4.0f,  -0.5f,  0.5f },
		{ "derx",     "dery",     -1.5f, -2.5f,  -0.3f,  0.4f },
		{ "derx",     "dery",     -3.5f, -4.5f,  -1.5f,  1.5f },
		{ "shotgunx", "shotguny", -7.0f, -10.5f, -1.7f,  1.6f },
		{ "coltx",    "colty",    -2.0f, -4.5f,  -0.7f,  0.6f },
		{ "pmx",      "pmy",      -7.5f, -8.5f,  -1.5f,  1.8f },
		{ "mauserx",  "mausery",  -5.0f, -5.3f,  -0.75f, 0.75f },
		{ "pmx",      "pmy",      -5.5f, -6.0f,  -0.6f,  0.8f },
		{ "coltx",    "colty",    -5.0f, -5.5f,  -0.7f,  0.6f },
		{ "vmx",      "vmy",      -2.5f, -3.5f,  -1.5f,  1.8f },
		{ "wmx",      "wmy",      -2.5f, -3.5f,  -1.5f,  1.8f },
		{ "wmx",      "wmy",      -6.5f, -7.5f,  -1.5f,  1.8f }
	};
	static const FoFRevolverPunchDefinition_t s_Secondary[] =
	{
		{ "coltx",    "colty",    -2.5f, -3.5f, -3.0f,  3.0f },
		{ "derx",     "dery",     -1.5f, -2.5f, -0.1f,  0.1f },
		{ "derx",     "dery",     -3.5f, -4.5f, -1.5f,  1.5f },
		{ "shotgunx", "shotguny", -8.0f, -9.5f, -0.6f,  0.5f },
		{ "schx",     "schy",     -2.0f, -2.5f, -2.8f,  2.8f },
		{ "pmx",      "pmy",      -2.5f, -3.5f, -2.0f,  2.0f },
		{ "mauserx",  "mausery",  -6.5f, -6.7f, -2.0f,  2.0f },
		{ "pmx",      "pmy",      -2.5f, -3.5f, -3.2f,  3.3f },
		{ "schx",     "schy",     -2.0f, -2.5f, -2.8f,  2.8f },
		{ "vmx",      "vmy",      -2.5f, -3.5f, -2.6f,  2.5f },
		{ "wmx",      "wmy",      -2.5f, -3.5f, -2.6f,  2.5f },
		{ "wmx",      "wmy",      -6.5f, -7.5f, -1.6f,  1.5f }
	};

	if ( profile < 0 || profile >= FOF_REVOLVER_PUNCH_PROFILE_COUNT )
		return QAngle( 0.0f, 0.0f, 0.0f );

	const FoFRevolverPunchDefinition_t &definition =
		bSecondary ? s_Secondary[profile] : s_Primary[profile];
	return QAngle(
		SharedRandomFloat( definition.m_pszXSeed,
			definition.m_flXMin, definition.m_flXMax ),
		SharedRandomFloat( definition.m_pszYSeed,
			definition.m_flYMin, definition.m_flYMax ),
		0.0f );
}

// memdbgon must be the last include file in a .cpp file!!!

IMPLEMENT_NETWORKCLASS_ALIASED( FoFBaseRevolver, DT_FoFBaseRevolver )

BEGIN_NETWORK_TABLE( CFoFBaseRevolver, DT_FoFBaseRevolver )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flSoonestPrimaryAttack ) ),
	RecvPropFloat( RECVINFO( m_flLastAttackTime ) ),
	RecvPropFloat( RECVINFO( m_flAccuracyPenalty ) ),
	RecvPropBool( RECVINFO( m_bDelayedReload ) ),
	RecvPropFloat( RECVINFO( m_flDeployDualGunIn ) ),
	RecvPropBool( RECVINFO( m_bNeedPump ) ),
	RecvPropBool( RECVINFO( m_bDouble ) ),
	RecvPropFloat( RECVINFO( m_flTriggerHoldTime ) ),
#else
	SendPropBool( SENDINFO( m_bDelayedReload ) ),
	SendPropFloat( SENDINFO( m_flSoonestPrimaryAttack ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flLastAttackTime ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flAccuracyPenalty ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flDeployDualGunIn ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bNeedPump ) ),
	SendPropBool( SENDINFO( m_bDouble ) ),
	SendPropFloat( SENDINFO( m_flTriggerHoldTime ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CFoFBaseRevolver )
	// Keep this list separate from the RecvTable: FoF predicts all eight revolver
	// state fields, including the byte-sized delayed/pump/dual flags.
	DEFINE_PRED_FIELD( m_bDelayedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flSoonestPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastAttackTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeployDualGunIn, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bNeedPump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDouble, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flTriggerHoldTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif
