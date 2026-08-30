//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Authoritative FoF weapon-crate state machine and purchase protocol.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_crate.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_player.h"
#include "fof/fof_weapon_properties.h"

#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( fof_crate, FoF_Crate );
LINK_ENTITY_TO_CLASS( fof_crate_low, FoF_Crate );
LINK_ENTITY_TO_CLASS( fof_crate_med, FoF_Crate );
LINK_ENTITY_TO_CLASS( fof_crate_special, FoF_Crate );

IMPLEMENT_SERVERCLASS_ST( FoF_Crate, DT_FoF_Crate )
	SendPropFloat( SENDINFO( m_flNextRegen ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flTotalRegenTime ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

BEGIN_DATADESC( FoF_Crate )
	DEFINE_FIELD( m_nCrateState, FIELD_INTEGER ),
	DEFINE_FIELD( m_nCrateTier, FIELD_INTEGER ),
	DEFINE_FIELD( m_flOpenCompleteTime, FIELD_TIME ),
	DEFINE_FIELD( m_flOpeningDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRegenerationDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_hUsingPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flNextRegen, FIELD_TIME ),
	DEFINE_FIELD( m_flTotalRegenTime, FIELD_FLOAT ),
	DEFINE_OUTPUT( m_OnOpen, "OnOpen" ),
	DEFINE_OUTPUT( m_OnClose, "OnClose" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RestartCrate", InputRestart_Crate ),
	DEFINE_THINKFUNC( OpenThink ),
END_DATADESC()

struct FoFCrateOffer_t
{
	int m_nCommandId;
	const char *m_pszClassname;
	int m_nPrices[3];
	bool m_bBotAllowed;
	int m_nQuantity;
	const char *m_pszToken;
};

struct FoFFreeCrateOffer_t
{
	int m_nCommandId;
	const char *m_pszClassname;
	int m_nRequiredTier;
	const char *m_pszToken;
};

static const FoFCrateOffer_t s_FoFPricedCrateOffers[] =
{
	{ 32, "Wood_Crate",               { 15,  -1,  -1 }, false, 0, "" },
	{ 35, "weapon_axe",               { 15,  -1,  -1 }, false, 0, "#Item19" },
	{ 38, "weapon_hammerless",        { 25,  -1,  -1 }, true,  0, "#Item3" },
	{ 31, "weapon_dynamite",          { 25,  -1,  -1 }, true,  0, "#Item0" },
	{ 54, "weapon_whiskey",           { -1,  30,  -1 }, false, 0, "#Item15" },
	{ 36, "weapon_bow",               { 30,  -1,  -1 }, false, 0, "#Item11" },
	{ 37, "weapon_sawedoff_shotgun",  { 35,  -1,  -1 }, true,  0, "#Item13b" },
	{ 41, "weapon_schofield",         { 50,  -1,  -1 }, true,  0, "#Item31a" },
	{ 45, "weapon_henryrifle",        { 50,  -1,  -1 }, true,  0, "#Item4a" },
	{ 46, "weapon_coachgun",          { -1,  50,  -1 }, true,  0, "#Item6" },
	{ 50, "weapon_dynamite_black",    { -1,  50,  -1 }, false, 0, "#Item0b" },
	{ 44, "weapon_bow_black",         { -1,  60,  -1 }, true,  0, "#Item11b" },
	{ 47, "weapon_spencer",           { -1,  60,  -1 }, true,  0, "#Item18" },
	{ 43, "weapon_peacemaker",        { -1,  65,  -1 }, true,  0, "#Item21" },
	{ 53, "weapon_dynamite_belt",     { -1,  -1,  50 }, false, 0, "#Item17" },
	{ 48, "weapon_machete",           { -1,  -1,  55 }, true,  0, "#Item26" },
	{ 49, "weapon_shotgun",           { -1,  -1,  60 }, true,  0, "#Item30" },
	{ 51, "weapon_sharps",            { -1,  -1,  75 }, false, 0, "#Item12" },
	{ 52, "weapon_walker",            { -1,  -1, 100 }, true,  0, "#Item32" },
	{ 33, "weapon_volcanic",          { -1,  -1, 125 }, false, 5, "#ItemVolcanic" },
	{ 34, "weapon_coltnavy",          { -1,  -1, 150 }, false, 5, "#Item2" }
};

static const FoFFreeCrateOffer_t s_FoFFreeCrateOffers[] =
{
	{ 35, "weapon_axe",              1, "#Item19" },
	{ 36, "weapon_bow",              1, "#Item11" },
	{ 37, "weapon_sawedoff_shotgun", 1, "#Item13b" },
	{ 38, "weapon_hammerless",       1, "#Item3" },
	{ 45, "weapon_henryrifle",       1, "#Item4a" },
	{ 54, "weapon_whiskey",          1, "#Item15" },
	{ 43, "weapon_peacemaker",       2, "#Item21" },
	{ 44, "weapon_bow_black",        2, "#Item11b" },
	{ 46, "weapon_coachgun",         2, "#Item6" },
	{ 47, "weapon_spencer",          2, "#Item18" },
	{ 48, "weapon_machete",          3, "#Item26" },
	{ 49, "weapon_shotgun",          3, "#Item30" },
	{ 50, "weapon_dynamite_black",   3, "#Item0b" },
	{ 51, "weapon_sharps",           3, "#Item12" },
	{ 52, "weapon_walker",           3, "#Item32" },
	{ 53, "weapon_dynamite_belt",    3, "#Item17" }
};

// Course crates use the shipped tutorial subset.  Their menu lines are the
// localization tokens themselves; unlike ordinary free crates they do not
// carry a synthetic "$0" prefix.
static const FoFFreeCrateOffer_t s_FoFCourseCrateOffers[] =
{
	{ 35, "weapon_axe",              1, "#Item19" },
	{ 37, "weapon_sawedoff_shotgun", 1, "#Item13b" },
	{ 41, "weapon_schofield",        1, "#Item31a" },
	{ 45, "weapon_henryrifle",       1, "#Item4a" },
	{ 43, "weapon_peacemaker",       2, "#Item21" },
	{ 44, "weapon_bow_black",        2, "#Item11b" },
	{ 46, "weapon_coachgun",         2, "#Item6" },
	{ 47, "weapon_spencer",          2, "#Item18" },
	{ 48, "weapon_machete",          3, "#Item26" },
	{ 49, "weapon_shotgun",          3, "#Item30" },
	{ 51, "weapon_sharps",           3, "#Item12" },
	{ 52, "weapon_walker",           3, "#Item32" }
};

static int FoFCrateCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static bool FoFCrateClassicShootout()
{
	static ConVarRef classicShootout( "fof_sv_classic_shootout", true );
	return classicShootout.IsValid() && classicShootout.GetBool();
}

static bool FoFCrateGrandElimination()
{
	static ConVarRef noRespawn( "fof_sv_elm_norespawn", true );
	return FoFCrateCurrentMode() == 4 &&
		noRespawn.IsValid() && noRespawn.GetBool();
}

static bool FoFCrateUsesPricedOffers()
{
	const int nMode = FoFCrateCurrentMode();
	return ( nMode == 4 && !FoFCrateGrandElimination() ) ||
		( nMode == 1 && !FoFCrateClassicShootout() );
}

static const FoFCrateOffer_t *FoFFindPricedCrateOffer( int nCommandId )
{
	for ( int i = 0; i < ARRAYSIZE( s_FoFPricedCrateOffers ); ++i )
	{
		if ( s_FoFPricedCrateOffers[i].m_nCommandId == nCommandId )
			return &s_FoFPricedCrateOffers[i];
	}
	return NULL;
}

static const FoFFreeCrateOffer_t *FoFFindFreeCrateOffer( int nCommandId )
{
	for ( int i = 0; i < ARRAYSIZE( s_FoFFreeCrateOffers ); ++i )
	{
		if ( s_FoFFreeCrateOffers[i].m_nCommandId == nCommandId )
			return &s_FoFFreeCrateOffers[i];
	}
	return NULL;
}

static const FoFFreeCrateOffer_t *FoFFindCourseCrateOffer(
	int nCommandId )
{
	for ( int i = 0; i < ARRAYSIZE( s_FoFCourseCrateOffers ); ++i )
	{
		if ( s_FoFCourseCrateOffers[i].m_nCommandId == nCommandId )
			return &s_FoFCourseCrateOffers[i];
	}
	return NULL;
}

static bool FoFCrateOfferProgressionUnlocked(
	CFoF_Player *pPlayer, const char *pszClassname,
	int *pRequirement = NULL )
{
	const FoFItemDefinition_t *pItem =
		FoFFindItemDefinitionByToken( pszClassname );
	const int nRequirement = pItem ?
		pItem->m_nProgressionRequirement : 0;
	if ( pRequirement )
		*pRequirement = nRequirement;
	return !pItem || ( pPlayer && FoFItemProgressionUnlocked(
		pItem, pPlayer->GetFoFProgression() ) );
}

static void FoFSendCrateMenuLine( CFoF_Player *pPlayer,
	const char *pszLine, bool bMore, int nCommandId )
{
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "ShowMenuFoF" );
		WRITE_STRING( pszLine );
		WRITE_BYTE( bMore ? 1 : 0 );
		WRITE_SHORT( nCommandId );
	MessageEnd();
}

static void FoFSendCratePriceWarning( CFoF_Player *pPlayer )
{
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "#Crate_Price_Warning" );
		WRITE_STRING( "" );
		WRITE_STRING( "" );
		WRITE_STRING( "" );
	MessageEnd();
}

static bool FoFCreatePurchasedWoodCrate( CFoF_Player *pPlayer )
{
	CBaseEntity *pProp = CreateEntityByName( "prop_physics_multiplayer" );
	if ( !pProp )
		return false;

	Vector forward;
	AngleVectors( pPlayer->EyeAngles(), &forward );
	forward.z = 0.0f;
	VectorNormalize( forward );
	pProp->SetAbsOrigin( pPlayer->GetAbsOrigin() + forward * 48.0f +
		Vector( 0.0f, 0.0f, 16.0f ) );
	pProp->SetAbsAngles( QAngle( 0.0f, pPlayer->EyeAngles().y, 0.0f ) );
	pProp->KeyValue( "model", "models/props_junk/wood_crate001a_small.mdl" );
	DispatchSpawn( pProp );
	pProp->Activate();
	return true;
}

static bool FoFGiveCrateWeapon( CFoF_Player *pPlayer,
	const char *pszClassname, int nQuantity )
{
	if ( !pPlayer || !pszClassname || !pszClassname[0] )
		return false;
	if ( !Q_stricmp( pszClassname, "Wood_Crate" ) )
		return FoFCreatePurchasedWoodCrate( pPlayer );

	const char *pszGiveClassname = pszClassname;
	const char *pszSecondHand =
		FoFGetOppositeHandWeaponClassname( pszClassname );
	if ( pszSecondHand && pPlayer->Weapon_OwnsThisType( pszClassname ) &&
		!pPlayer->Weapon_OwnsThisType( pszSecondHand ) )
	{
		pszGiveClassname = pszSecondHand;
	}

	CBaseCombatWeapon *pExisting =
		pPlayer->Weapon_OwnsThisType( pszGiveClassname );
	CBaseCombatWeapon *pWeapon = dynamic_cast< CBaseCombatWeapon * >(
		pPlayer->GiveFoFNamedItem( pszGiveClassname ) );
	if ( pExisting && pWeapon && pWeapon != pExisting &&
		pWeapon->GetOwner() != pPlayer )
	{
		UTIL_Remove( pWeapon );
		pWeapon = pExisting;
	}
	if ( !pWeapon )
		pWeapon = pPlayer->Weapon_OwnsThisType( pszGiveClassname );
	if ( !pWeapon )
		return false;

	if ( nQuantity > 0 && pWeapon->GetPrimaryAmmoType() >= 0 )
		pPlayer->GiveAmmo( nQuantity, pWeapon->GetPrimaryAmmoType(), true );

	// Crate acquisition normally preserves the currently active weapon.  If
	// the menu transaction arrived while both active handles were empty,
	// restore a real owned weapon immediately instead of leaving an invisible
	// viewmodel until the player presses a weapon slot.
	if ( !pPlayer->GetActiveWeapon1() && !pPlayer->GetActiveWeapon2() )
	{
		CBaseCombatWeapon *pOwned = pPlayer->Weapon_OwnsThisType(
			pszGiveClassname, pWeapon->GetSubType() );
		if ( pOwned )
			pPlayer->Weapon_Switch( pOwned );
	}
	return true;
}

static void FoFApplyCrateWhiskeyHealth(
	CFoF_Player *pPlayer, const char *pszClassname, float flHealth )
{
	if ( pPlayer && pszClassname &&
		!Q_stricmp( pszClassname, "weapon_whiskey" ) )
	{
		pPlayer->TakeHealth( flHealth, DMG_GENERIC );
	}
}

FoF_Crate::FoF_Crate()
	: m_nCrateState( CRATE_READY )
	, m_nCrateTier( 3 )
	, m_flOpenCompleteTime( 0.0f )
	, m_flOpeningDuration( 1.0f )
	, m_flRegenerationDuration( 7.5f )
{
	m_hUsingPlayer = NULL;
	m_flNextRegen = -1.0f;
	m_flTotalRegenTime = 0.0f;
}

void FoF_Crate::Precache()
{
	BaseClass::Precache();
	PrecacheModel( "models/items_fof/safe_crate.mdl" );
	PrecacheModel( "models/items_fof/safe_crate_small.mdl" );
	PrecacheModel( "models/props_junk/wood_crate001a_small.mdl" );
	PrecacheScriptSound( "MoneyCrate.Open" );
	PrecacheScriptSound( "AmmoCrate.Close" );
}

void FoF_Crate::Spawn()
{
	Precache();
	const char *pszModel = "models/items_fof/safe_crate_small.mdl";
	if ( FClassnameIs( this, "fof_crate_low" ) )
	{
		m_nCrateTier = 1;
		m_nSkin = 2;
	}
	else if ( FClassnameIs( this, "fof_crate_med" ) )
	{
		m_nCrateTier = 2;
		m_nSkin = 1;
	}
	else
	{
		m_nCrateTier = 3;
		m_nSkin = 0;
		pszModel = "models/items_fof/safe_crate.mdl";
	}

	SetModel( pszModel );
	BaseClass::Spawn();
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	if ( !VPhysicsInitStatic() )
		Warning( "can't create physics for fof_crate!\n" );

	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_connect_fof" );
	SetCrateSequence( "Idle" );
	ResetCrate( true );
	SetTransmitState( FL_EDICT_ALWAYS );
}

int FoF_Crate::ObjectCaps()
{
	return ( BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) |
		FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS;
}

bool FoF_Crate::IsSpecialCrate()
{
	return FClassnameIs( this, "fof_crate_special" );
}

bool FoF_Crate::IsReadyForBotUse() const
{
	return m_nCrateState == CRATE_READY && !IsEffectActive( EF_NODRAW );
}

bool FoF_Crate::IsBeingOpenedBy( const CFoF_Player *pPlayer ) const
{
	return pPlayer && m_nCrateState == CRATE_OPENING &&
		m_hUsingPlayer.Get() == pPlayer;
}

void FoF_Crate::SetCrateSequence( const char *pszSequence )
{
	const int nSequence = LookupSequence( pszSequence );
	if ( nSequence >= 0 )
	{
		ResetSequence( nSequence );
		SetCycle( 0.0f );
	}
}

void FoF_Crate::SendCrateMessage( int nMessageType )
{
	EntityMessageBegin( this, true );
		WRITE_BYTE( nMessageType );
	MessageEnd();
}

void FoF_Crate::SendCrateMessage( int nMessageType, float flValue )
{
	EntityMessageBegin( this, true );
		WRITE_BYTE( nMessageType );
		WRITE_FLOAT( flValue );
	MessageEnd();
}

void FoF_Crate::ResetCrate( bool bClose )
{
	if ( m_nCrateState == CRATE_OPENING )
		SendCrateMessage( 1, 0.0f );
	m_hUsingPlayer = NULL;
	m_nCrateState = CRATE_READY;
	RemoveEffects( EF_NOSHADOW | EF_ITEM_BLINK | ( 1 << 11 ) );

	m_flRegenerationDuration = m_nCrateTier * 0.5f * 5.0f;
	if ( FoFCrateCurrentMode() == 6 )
		m_flOpeningDuration = 3.0f;
	else if ( FoFCrateGrandElimination() )
		m_flOpeningDuration = 2.0f;
	else
		m_flOpeningDuration = 1.0f;

	if ( FoFCrateCurrentMode() == 1 )
	{
		if ( IsSpecialCrate() )
		{
			m_flOpeningDuration = 1.0f;
			SendCrateMessage( 5, (float)m_nCrateTier );
		}
		else
		{
			m_flRegenerationDuration = RemapVal(
				(float)m_nCrateTier, 1.0f, 3.0f, 35.0f, 70.0f );
		}
		if ( !FoFCrateClassicShootout() )
			m_flRegenerationDuration *= 0.25f;
	}
	if ( FoFCrateCurrentMode() == 4 )
		m_flRegenerationDuration += 3.0f;

	if ( !bClose )
	{
		m_flNextRegen = -1.0f;
		m_flTotalRegenTime = 0.0f;
		SetThink( NULL );
		SetNextThink( TICK_NEVER_THINK );
		return;
	}

	SetCrateSequence( "Close" );
	StartRegeneration();
}

void FoF_Crate::StartRegeneration()
{
	m_nCrateState = CRATE_REGENERATING;
	SetThink( &FoF_Crate::OpenThink );

	const int nMode = FoFCrateCurrentMode();
	float flDelay = m_flRegenerationDuration;
	if ( nMode == 6 || FoFCrateGrandElimination() )
		flDelay = 0.0f;
	else if ( nMode == 4 || ( nMode == 1 && !FoFCrateClassicShootout() ) )
		flDelay *= 5.0f;

	if ( flDelay <= 0.0f )
	{
		m_flNextRegen = 0.0f;
		m_flTotalRegenTime = 0.0f;
		SetNextThink( gpGlobals->curtime );
		return;
	}

	m_flNextRegen = gpGlobals->curtime + flDelay;
	m_flTotalRegenTime = flDelay;
	SetNextThink( m_flNextRegen );
}

void FoF_Crate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller,
	USE_TYPE useType, float flValue )
{
	NOTE_UNUSED( pCaller );
	NOTE_UNUSED( useType );
	NOTE_UNUSED( flValue );
	if ( m_nCrateState != CRATE_READY )
		return;

	CFoF_Player *pPlayer = ToFoFPlayer( pActivator );
	if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->IsObserver() ||
		pPlayer->GetFoFCrateMenuTier() != -1 )
	{
		return;
	}

	const float flUseDistance = pPlayer->IsBot() ? 120.0f : 75.0f;
	if ( pPlayer->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() ) >
		flUseDistance * flUseDistance )
	{
		return;
	}

	float flDurationScale = 1.0f;
	if ( FoFCrateCurrentMode() == 1 && FoFCrateClassicShootout() &&
		!IsSpecialCrate() )
	{
		if ( m_nCrateTier < 3 )
			flDurationScale = 0.75f;
		else
		{
			const float flElapsed = gpGlobals->curtime -
				pPlayer->GetFoFLastCrateUseTime();
			flDurationScale = RemapValClamped(
				flElapsed, 0.0f, 180.0f, 1.5f, 1.0f );
		}
	}

	m_hUsingPlayer = pPlayer;
	m_flOpenCompleteTime = gpGlobals->curtime +
		m_flOpeningDuration * flDurationScale;
	m_nCrateState = CRATE_OPENING;
	if ( !pPlayer->IsBot() )
		SendCrateMessage( 1, m_flOpenCompleteTime );
	SetThink( &FoF_Crate::OpenThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void FoF_Crate::CancelOpen()
{
	SendCrateMessage( 1, 0.0f );
	m_hUsingPlayer = NULL;
	m_flOpenCompleteTime = 0.0f;
	m_nCrateState = CRATE_READY;
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

void FoF_Crate::OpenThink()
{
	if ( m_nCrateState == CRATE_REGENERATING )
	{
		EmitSound( "AmmoCrate.Close" );
		SetCrateSequence( "Open" );
		m_OnClose.FireOutput( this, this );
		ResetCrate( false );
		return;
	}

	if ( m_nCrateState != CRATE_OPENING )
		return;

	CFoF_Player *pPlayer = m_hUsingPlayer.Get();
	if ( !pPlayer || !pPlayer->IsAlive() )
	{
		CancelOpen();
		return;
	}

	const float flHoldDistance = pPlayer->IsBot() ? 100.0f : 75.0f;
	static ConVarRef botForceOpen( "fof_bot_forceopenchest", true );
	const bool bForceBot = pPlayer->IsBot() &&
		botForceOpen.IsValid() && botForceOpen.GetBool();
	if ( pPlayer->WorldSpaceCenter().DistToSqr( WorldSpaceCenter() ) >
		flHoldDistance * flHoldDistance ||
		( !bForceBot && !( pPlayer->m_nButtons & IN_USE ) ) )
	{
		CancelOpen();
		return;
	}

	if ( gpGlobals->curtime < m_flOpenCompleteTime )
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;
	}

	FinishOpen( pPlayer );
}

void FoF_Crate::FinishOpen( CFoF_Player *pPlayer )
{
	SetCrateSequence( "Close" );
	SendCrateMessage( 1, 0.0f );
	if ( FoFCrateCurrentMode() == 1 )
	{
		SendCrateMessage( 6 );
		SetTransmitState( FL_EDICT_PVSCHECK );
	}
	EmitSound( "MoneyCrate.Open" );
	AddEffects( EF_ITEM_BLINK | ( 1 << 11 ) );
	m_hUsingPlayer = NULL;
	m_OnOpen.FireOutput( pPlayer, this );
	FoFReportCourseStat( "open_chest", pPlayer );

	if ( IsSpecialCrate() )
	{
		pPlayer->TakeHealth( 50, DMG_GENERIC );
	}
	else if ( pPlayer->IsBot() )
	{
		AutoPurchaseForBot( pPlayer );
	}
	else
	{
		ShowCrateMenu( pPlayer );
	}

	if ( IsSpecialCrate() || FoFCrateCurrentMode() == 6 ||
		FoFCrateGrandElimination() )
	{
		m_nCrateState = CRATE_REMOVING;
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 5.0f );
		return;
	}

	StartRegeneration();
}

void FoF_Crate::ShowCrateMenu( CFoF_Player *pPlayer )
{
	pPlayer->SetFoFCrateMenuTier( m_nCrateTier, this );
	if ( FoFCrateCurrentMode() == 6 )
	{
		CUtlVector< const FoFFreeCrateOffer_t * > offers;
		for ( int i = 0; i < ARRAYSIZE( s_FoFCourseCrateOffers ); ++i )
		{
			if ( s_FoFCourseCrateOffers[i].m_nRequiredTier == m_nCrateTier )
				offers.AddToTail( &s_FoFCourseCrateOffers[i] );
		}
		for ( int i = 0; i < offers.Count(); ++i )
		{
			FoFSendCrateMenuLine( pPlayer, offers[i]->m_pszToken,
				i + 1 < offers.Count(), offers[i]->m_nCommandId );
		}
		return;
	}
	if ( FoFCrateUsesPricedOffers() )
	{
		CUtlVector< const FoFCrateOffer_t * > offers;
		for ( int i = 0; i < ARRAYSIZE( s_FoFPricedCrateOffers ); ++i )
		{
			const FoFCrateOffer_t &offer = s_FoFPricedCrateOffers[i];
			if ( offer.m_nPrices[m_nCrateTier - 1] > 0 )
				offers.AddToTail( &offer );
		}

		for ( int i = 0; i < offers.Count(); ++i )
		{
			const FoFCrateOffer_t &offer = *offers[i];
			char line[192];
			int nRequirement = 0;
			if ( FoFCrateOfferProgressionUnlocked(
				pPlayer, offer.m_pszClassname, &nRequirement ) )
			{
				Q_snprintf( line, sizeof( line ), " $%d ,%s,%d",
					offer.m_nPrices[m_nCrateTier - 1],
					offer.m_pszToken,
					offer.m_nQuantity );
			}
			else
			{
				Q_snprintf( line, sizeof( line ), " *%d ,%s ,0",
					nRequirement / 100, offer.m_pszToken );
			}
			FoFSendCrateMenuLine( pPlayer, line,
				i + 1 < offers.Count(), offer.m_nCommandId );
		}
		return;
	}

	CUtlVector< const FoFFreeCrateOffer_t * > offers;
	for ( int i = 0; i < ARRAYSIZE( s_FoFFreeCrateOffers ); ++i )
	{
		if ( s_FoFFreeCrateOffers[i].m_nRequiredTier == m_nCrateTier )
			offers.AddToTail( &s_FoFFreeCrateOffers[i] );
	}
	for ( int i = 0; i < offers.Count(); ++i )
	{
		char line[192];
		int nRequirement = 0;
		if ( FoFCrateOfferProgressionUnlocked(
			pPlayer, offers[i]->m_pszClassname, &nRequirement ) )
		{
			Q_snprintf( line, sizeof( line ), " $0 ,%s,0",
				offers[i]->m_pszToken );
		}
		else
		{
			Q_snprintf( line, sizeof( line ), " *%d ,%s ,0",
				nRequirement / 100, offers[i]->m_pszToken );
		}
		FoFSendCrateMenuLine( pPlayer, line,
			i + 1 < offers.Count(), offers[i]->m_nCommandId );
	}
}

void FoF_Crate::AutoPurchaseForBot( CFoF_Player *pPlayer )
{
	if ( !FoFCrateUsesPricedOffers() )
	{
		CUtlVector< const FoFFreeCrateOffer_t * > offers;
		for ( int i = 0; i < ARRAYSIZE( s_FoFFreeCrateOffers ); ++i )
		{
			if ( s_FoFFreeCrateOffers[i].m_nRequiredTier == m_nCrateTier )
				offers.AddToTail( &s_FoFFreeCrateOffers[i] );
		}
		if ( offers.Count() > 0 )
		{
			const FoFFreeCrateOffer_t &offer = *offers[
				random->RandomInt( 0, offers.Count() - 1 )];
			if ( FoFGiveCrateWeapon(
				pPlayer, offer.m_pszClassname, 0 ) )
			{
				FoFApplyCrateWhiskeyHealth(
					pPlayer, offer.m_pszClassname, 100.0f );
			}
		}
		return;
	}

	CUtlVector< const FoFCrateOffer_t * > offers;
	for ( int i = 0; i < ARRAYSIZE( s_FoFPricedCrateOffers ); ++i )
	{
		const FoFCrateOffer_t &offer = s_FoFPricedCrateOffers[i];
		const int nPrice = offer.m_nPrices[m_nCrateTier - 1];
		if ( offer.m_bBotAllowed && nPrice > 0 &&
			pPlayer->GetFoFCash() >= nPrice )
		{
			offers.AddToTail( &offer );
		}
	}
	if ( offers.Count() <= 0 )
		return;

	const FoFCrateOffer_t &offer = *offers[
		random->RandomInt( 0, offers.Count() - 1 )];
	const int nPrice = offer.m_nPrices[m_nCrateTier - 1];
	if ( FoFGiveCrateWeapon( pPlayer, offer.m_pszClassname,
		offer.m_nQuantity ) )
	{
		pPlayer->AddFoFCash( (float)-nPrice );
	}
}

void FoF_Crate::InputRestart_Crate( inputdata_t &inputData )
{
	NOTE_UNUSED( inputData );
	ResetCrate( true );
}

void FoF_Crate::DisableForBreakBadRound()
{
	if ( m_nCrateState == CRATE_OPENING )
		SendCrateMessage( 1, 0.0f );
	m_hUsingPlayer = NULL;
	m_flOpenCompleteTime = 0.0f;
	m_flNextRegen = -1.0f;
	m_flTotalRegenTime = 0.0f;
	m_nCrateState = CRATE_REMOVING;
	AddEffects( EF_NODRAW );
	SetSolid( SOLID_NONE );
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

void FoF_Crate::FireGameEvent( IGameEvent *pEvent )
{
	if ( !pEvent )
		return;
	if ( !Q_stricmp( pEvent->GetName(), "round_start" ) )
	{
		ResetCrate( true );
	}
	else if ( !Q_stricmp( pEvent->GetName(), "player_connect_fof" ) &&
		IsSpecialCrate() )
	{
		SendCrateMessage( 5, (float)m_nCrateTier );
	}
}

bool CFoF_Player::HandleFoFCrateMenuSelection( int nCommandId )
{
	const int nTier = GetFoFCrateMenuTier();
	ClearFoFCrateMenu();
	if ( nTier < 1 || nTier > 3 || nCommandId < 0 || nCommandId >= 56 )
		return false;

	if ( FoFCrateCurrentMode() == 6 )
	{
		const FoFFreeCrateOffer_t *pOffer =
			FoFFindCourseCrateOffer( nCommandId );
		if ( !pOffer || pOffer->m_nRequiredTier != nTier ||
			!FoFCrateOfferProgressionUnlocked(
				this, pOffer->m_pszClassname ) )
		{
			return false;
		}
		if ( !FoFGiveCrateWeapon( this, pOffer->m_pszClassname, 0 ) )
			return false;
		FoFApplyCrateWhiskeyHealth(
			this, pOffer->m_pszClassname, 100.0f );
		return true;
	}

	if ( FoFCrateUsesPricedOffers() )
	{
		const FoFCrateOffer_t *pOffer = FoFFindPricedCrateOffer( nCommandId );
		if ( !pOffer )
			return false;
		if ( !FoFCrateOfferProgressionUnlocked(
			this, pOffer->m_pszClassname ) )
		{
			return false;
		}
		const int nPrice = pOffer->m_nPrices[nTier - 1];
		if ( nPrice <= 0 || GetFoFCash() < nPrice )
		{
			FoFSendCratePriceWarning( this );
			return false;
		}
		if ( !FoFGiveCrateWeapon( this, pOffer->m_pszClassname,
			pOffer->m_nQuantity ) )
		{
			return false;
		}
		FoFApplyCrateWhiskeyHealth(
			this, pOffer->m_pszClassname, 50.0f );
		if ( nTier == 3 )
			TakeHealth( 25.0f, DMG_GENERIC );
		AddFoFCash( (float)-nPrice );
		return true;
	}

	const FoFFreeCrateOffer_t *pOffer = FoFFindFreeCrateOffer( nCommandId );
	if ( !pOffer || pOffer->m_nRequiredTier != nTier )
		return false;
	if ( !FoFCrateOfferProgressionUnlocked(
		this, pOffer->m_pszClassname ) )
	{
		return false;
	}
	if ( !FoFGiveCrateWeapon( this, pOffer->m_pszClassname, 0 ) )
		return false;
	FoFApplyCrateWhiskeyHealth(
		this, pOffer->m_pszClassname, 100.0f );
	return true;
}
