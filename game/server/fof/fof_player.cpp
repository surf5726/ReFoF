#include "cbase.h"
#include "fof/fof_breakbad_mode.h"
#include "fof/fof_bot.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_elimination_mode.h"
#include "fof/fof_gamerules.h"
#include "fof/fof_rounds.h"
#include "fof/fof_spawn.h"
#include "fof/fof_player.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_weapon_properties.h"
#include "fof/fof_ai_editor.h"
#include "fof/fof_player_activities.h"
#include "fof/fof_player_equipment.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_player_statistics.h"
#include "fof/fof_votekick.h"
#include "ammodef.h"
#include "ai_basenpc.h"
#include "baseviewmodel_shared.h"
#include "gamevars_shared.h"
#include "hl2/func_tank.h"
#include "hl2mp/hl2mp_playeranimstate.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_weapon_parse.h"
#include "in_buttons.h"
#include "npcevent.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"
#include "obstacle_pushaway.h"
#include "props_shared.h"
#include "recipientfilter.h"
#include "team.h"
#include "weapon_hl2mpbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CBaseEntity *g_pLastCombineSpawn;
extern CBaseEntity *g_pLastRebelSpawn;

static float s_flFoFLastActiveTime[MAX_PLAYERS + 1];

static void FoFSendBloodImpact(
	CFoF_Player *pAttacker, const Vector &vecOrigin,
	const Vector &vecDirection, float flDamage )
{
	if ( !pAttacker || pAttacker->IsBot() )
		return;

	// The original server sends this unreliable
	// presentation message to the attacking player after hit-group scaling.
	EntityMessageBegin( pAttacker, false );
		WRITE_BYTE( 6 );
		WRITE_VEC3COORD( vecOrigin );
		WRITE_VEC3COORD( vecDirection );
		WRITE_SHORT( static_cast< int >( flDamage ) );
	MessageEnd();
}

static void FoFEmitHeadshotSounds( CFoF_Player *pVictim )
{
	if ( !pVictim )
		return;

	// Nearby players hear the full-volume impact, while the victim receives
	// the quieter close sound.  Keeping the recipient sets disjoint avoids
	// stacking both scripts on the victim.
	CPASAttenuationFilter loudFilter( pVictim, "FoF.HeadshotLoud" );
	loudFilter.RemoveRecipient( pVictim );
	loudFilter.MakeReliable();
	CBaseEntity::EmitSound(
		loudFilter, pVictim->entindex(), "FoF.HeadshotLoud" );

	CSingleUserRecipientFilter victimFilter( pVictim );
	victimFilter.MakeReliable();
	CBaseEntity::EmitSound(
		victimFilter, pVictim->entindex(), "FoF.Headshot" );
}

static void FoFSendSpawnHudState( CFoF_Player *pPlayer )
{
	if ( !pPlayer || pPlayer->IsBot() )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "MaxHP" );
		WRITE_BYTE( 100 );
	MessageEnd();

	UserMessageBegin( filter, "FoFHint" );
		WRITE_STRING( "" );
		WRITE_BYTE( 99 );
	MessageEnd();
}

static void FoFSendSpawnStateMessage( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	// The original server sends this as an
	// unreliable entity message on every spawn.  The client clears the
	// player's decals and, on its first local receipt, returns the six
	// persisted Steam statistic values through fof_stat.
	EntityMessageBegin( pPlayer, false );
		WRITE_BYTE( 2 );
	MessageEnd();
}

static void FoFFireHatshotEvent( CBaseEntity *pAttackerEntity )
{
	CFoF_Player *pAttacker = ToFoFPlayer( pAttackerEntity );
	if ( !pAttacker )
		return;

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "hatshot" );
	if ( !pEvent )
		return;

	// Despite its historical name, the field carries the attacker's user ID;
	// the matching client achievement compares it with player_info_t::userID.
	pEvent->SetInt( "entindex_hatshot", pAttacker->GetUserID() );
	gameeventmanager->FireEvent( pEvent );
}

LINK_ENTITY_TO_CLASS( player, CFoF_Player );

// The shipped server exposes a distinct CFoF_Player datamap even though it
// does not add any save/restore fields of its own.
BEGIN_DATADESC( CFoF_Player )
END_DATADESC()

BEGIN_SEND_TABLE_NOBASE( CFoF_Player, DT_FoFLocalPlayerExclusive008 )
	SendPropInt( SENDINFO( m_nInBuyZone ) ),
	SendPropInt( SENDINFO( m_nPlayerAccuracy ) ),
	SendPropFloat( SENDINFO( m_flNextAccuracyChange ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flTargetCrosshairAperture ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flTargetCrosshairAperture2 ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nFoFPlayerFOV ) ),
	SendPropBool( SENDINFO( m_bSpawnInterpCounter ) ),
	SendPropFloat( SENDINFO( m_flDrunkness ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flCaptureInput ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nHandStance ) ),
	SendPropFloat( SENDINFO( m_flTimeZoomed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flNextPickupInteraction ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flWalkSpreadFactor ), 0, SPROP_NOSCALE ),
	SendPropArray3( SENDINFO_ARRAY3( m_nPlHighlight ),
		SendPropInt( SENDINFO_ARRAY( m_nPlHighlight ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_nPlTarget ),
		SendPropInt( SENDINFO_ARRAY( m_nPlTarget ) ) ),
	SendPropAngle( SENDINFO_VECTORELEM( vecPropCarryAngles, 0 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( vecPropCarryAngles, 1 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( vecPropCarryAngles, 2 ), 11 ),
	SendPropFloat( SENDINFO( m_flJailTime ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nPotionLevel ) ),
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST( CFoF_Player, DT_CFoF_Player_608 )
	SendPropInt( SENDINFO( m_nProgression ) ),
	SendPropAngle( SENDINFO_VECTORELEM( m_horseAngles, 0 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( m_horseAngles, 1 ), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM( m_horseAngles, 2 ), 11 ),
	SendPropDataTable( "foflocaldata", 0,
		&REFERENCE_SEND_TABLE( DT_FoFLocalPlayerExclusive008 ),
		SendProxy_SendLocalDataTable ),
	SendPropFloat( SENDINFO( m_flTransitionSpeed ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nPlayerInfo ), 25, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bPickupActive ) ),
	SendPropEHandle( SENDINFO( m_hAttachedObject ) ),
	SendPropVector( SENDINFO( m_attachedPositionObjectSpace ), -1,
		SPROP_COORD | SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 0 ),
		13, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 1 ),
		13, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM( m_attachedAnglesPlayerSpace, 2 ),
		13, SPROP_CHANGES_OFTEN ),
	SendPropFloat( SENDINFO( m_flFoFSpeedPenalty ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flSightExpFactor ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flWalkFactor ), 0, SPROP_NOSCALE ),
	SendPropEHandle( SENDINFO( hPlayerAssisted ) ),
	SendPropEHandle( SENDINFO( m_hKicker ) ),
	SendPropInt( SENDINFO( m_nMultiKill ) ),
	SendPropFloat( SENDINFO( m_flUnarmedTime ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flFoFCash ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nPlayerKills ) ),
	SendPropInt( SENDINFO( m_nLastRoundNotoriety ) ),
	SendPropFloat( SENDINFO( m_flCrosshairAperture ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flCrosshairAperture2 ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bIsBotGhost ) ),
END_SEND_TABLE()

CFoF_Player::CFoF_Player()
{
	m_nFoFProgressionCache = 0;
	m_nProgression = 0;
	m_horseAngles.Init();
	m_nInBuyZone = 0;
	m_nPlayerAccuracy = 0;
	m_nFoFDynamiteBeltAttempts = 0;
	m_nFoFDynamiteBeltHits = 0;
	m_flNextAccuracyChange = 0.0f;
	m_flTargetCrosshairAperture = 0.0f;
	m_flTargetCrosshairAperture2 = 0.0f;
	m_nFoFPlayerFOV = 90;
	m_nFoFPendingFOV = 0;
	m_bSpawnInterpCounter = false;
	m_flDrunkness = 0.0f;
	m_flCaptureInput = 0.0f;
	m_nHandStance = 0;
	m_flTimeZoomed = 0.0f;
	m_flNextPickupInteraction = 0.0f;
	m_flWalkSpreadFactor = 0.0f;
	for ( int i = 0; i < 25; ++i )
	{
		m_nPlHighlight.Set( i, 0 );
		m_nPlTarget.Set( i, 0 );
	}
	vecPropCarryAngles.Init();
	m_flJailTime = 0.0f;
	m_nPotionLevel = 0;
	m_flTransitionSpeed = 0.0f;
	m_nPlayerInfo = 0;
	m_bPickupActive = false;
	m_hAttachedObject = NULL;
	m_attachedPositionObjectSpace.Init();
	m_attachedAnglesPlayerSpace.Init();
	m_flFoFSpeedPenalty = 0.0f;
	m_flSightExpFactor = 0.0f;
	m_flWalkFactor = 0.0f;
	hPlayerAssisted = NULL;
	m_hKicker = NULL;
	m_nMultiKill = 0;
	m_flUnarmedTime = 0.0f;
	m_flFoFCash = 0.0f;
	m_nPlayerKills = 0;
	m_nLastRoundNotoriety = 0;
	m_flCrosshairAperture = 0.0f;
	m_flCrosshairAperture2 = 0.0f;
	m_bIsBotGhost = false;
	m_pFoFSpawnPoint = NULL;
	m_pFoFSpawnSearchStart = NULL;
	m_pFoFLastSpawnPoint = NULL;
	m_flFoFNextSpawnAttempt = 0.0f;
	m_nFoFSpawnAttempts = 0;
	m_bFoFSpawnPending = false;
	m_bFoFSpawnEquipmentFinalized = true;
	m_flFoFNextSpawnEquipmentAttempt = 0.0f;
	m_flFoFBuyZoneUntil = 0.0f;
	m_nFoFCrateMenuTier = -1;
	m_hFoFCrateMenuSource = NULL;
	m_flFoFLastCrateUseTime = -FLT_MAX;
	m_flNextFoFSpectatorTime = 0.0f;
	m_flNextFoFAutojoinTime = 0.0f;
	m_flNextFoFVoiceTime = 0.0f;
	m_bFoFRifleCrosshair = false;
	m_bFoFPlayTaunts = true;
	m_bFoFHasFriendOnTeam = false;
	m_flNextFoFHandSwitchTime = 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_iFoFPersonalStats ); ++i )
		m_iFoFPersonalStats[i] = -1;
	m_flFoFReportedAccuracy = 0.0f;
	m_flFoFMapPlayTime = 0.0f;
	m_flFoFCaptureContribution = 0.0f;
	m_nFoFCaptureObjectiveDuration = 0;
	m_nFoFBestMultiKill = 0;
	m_nFoFCurrentDrunkard = 0;
	m_nFoFBestDrunkard = 0;
	m_flFoFDamageAccumulated = 0.0f;
	m_nFoFTeamClass = -1;
	m_flFoFInvulnerableUntil = 0.0f;
	m_bFoFInvulnerabilityOwnsPlayerInfoBit = false;
	m_flNextFoFWeaponThrowTime = 0.0f;
	m_flFoFDeathChainReactionTime = 0.0f;
	m_nFoFEquipmentFlags = 0;
	m_flFoFLifeStartTime = 0.0f;
	m_flFoFStuckTouchTime = 0.0f;
	m_bFoFResetPickupOwner = false;
	m_nFoFHintKeyButton = 0;
	// Ordinary FoF players receive one of the four human appearances when the
	// player entity is constructed.  Grand Elimination starts everybody as a
	// lawman and changes the appearance later when the outlaw role is acquired.
	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	m_nFoFModelSelection =
		battleRoyale.IsValid() && battleRoyale.GetBool() ?
		0 : random->RandomInt( 0, 3 );
	m_nFoFHatModel = 0;
	m_bFoFHatPresent = false;
	m_flFoFLastWallJumpTime = -FLT_MAX;
	m_hFoFGhostGunImpulseSource = NULL;
	m_flFoFGhostGunImpulseTime = 0.0f;
	m_hFoFGhostGunLastCollision = NULL;
	m_flFoFKickerTime = 0.0f;
	m_flFoFPotionTickTime = 0.0f;
	m_flFoFPotionActivateTime = 0.0f;
	m_flNextFoFBurpTime = 0.0f;
	m_hFoFHorse = NULL;
	m_flFoFHorseDismountHold = 0.0f;
	m_flNextFoFHorseRamTime = 0.0f;
	m_flNextFoFHorseHintTime = 0.0f;
	// No retired account backend is available.  Zero is the original unloaded
	// profile value and avoids publishing a fabricated 1000 notoriety score.
	m_iFoFExperience = 0;
	m_iFoFResourceState = 0;
	m_iFoFProfileLoadState = 1;
	m_nFoFTotalNotoriety = 0;
	m_nFoFCombatNotoriety = 0;
	m_nFoFAssistDamage = 0;
	m_flFoFMultiKillAnnounceTime = 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_iFoFNemesisKills ); ++i )
		m_iFoFNemesisKills[i] = 0;
	m_hFoFVersusSpawn = NULL;
	m_bFoFDeathmatchLoadoutCommitted = false;
	m_nFoFPurchaseState = 0;
	m_nFoFVotekickMenuState = -1;
	m_szFoFVotekickDisplayName[0] = '\0';
	m_szFoFVotekickLastName[0] = '\0';
	m_nFoFVotekickNameChanges = 0;
	m_flFoFVotekickConnectionStartTime =
		gpGlobals ? gpGlobals->curtime : 0.0f;
	m_flFoFTrackingPickupTime = 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_vecFoFTrackingOrigins ); ++i )
		m_vecFoFTrackingOrigins[i].Init();
	m_bFoFMobileCannonOperatorAccess = false;
	m_flFoFNextMobileCannonWarningTime = 0.0f;
	m_nFoFHeavyLoadPropReserve = 0;
	m_bFoFHeavyLoadAlternateSpawn = false;
	// The inactive account service leaves the connected player's rating at zero.
	// This is separate from match notoriety and survives round respawns.
	m_flFoFGlobalRank = 0.0f;
	m_nFoFBuyZoneTier = 1;
	m_flFoFSpawnBuyUntil = 0.0f;
}

void CFoF_Player::AddFoFNotoriety(
	int nAmount, int nReason, int nEventCode, const char *pszText )
{
	if ( nAmount == 0 )
		return;

	const bool bCombat = nReason >= 0 && nReason <= 3;
	AccumulateFoFNotoriety( nAmount, bCombat, false );
	SendFoFNotorietyNotice( nReason, nAmount, nEventCode, pszText );
}

void CFoF_Player::RecordFoFHeavyLoadPhysicsKill( void )
{
	++m_nFoFHeavyLoadPropReserve;
}

void CFoF_Player::ResetFoFHeavyLoadRespawnCycle( void )
{
	m_bFoFHeavyLoadAlternateSpawn = false;
}

void CFoF_Player::Precache( void )
{
	BaseClass::Precache();

	static const char *const s_pszPlayerSounds[] =
	{
		"FoFPlayer.Equipment",
		"FoFPlayer.Resupply",
		"FoFPlayer.WeaponPickUp",
		"FoFPlayer.KnifePickUp",
		"FoFPlayer.AxePickUp",
		"Whiskey.Use1",
		"Whiskey.StopCall",
		"Whiskey.Use1_p2",
		"Whiskey.StopCall_p2",
		"Whiskey.Use1_p3",
		"Whiskey.StopCall_p3",
		"Whiskey.Use1_p4",
		"Whiskey.StopCall_p4"
	};
	for ( int i = 0; i < ARRAYSIZE( s_pszPlayerSounds ); ++i )
		PrecacheScriptSound( s_pszPlayerSounds[i] );

	static const int s_nVoiceIndices[] =
	{
		1, 2, 3, 4, 5, 6, 7,
		11, 12, 13, 14, 15, 16, 17,
		21, 22, 23, 24, 25, 26, 27, 28, 29
	};
	static const char *const s_pszVoiceSuffixes[] =
	{
		"", "_p2", "_p4", "_p3"
	};
	char szSound[32];
	for ( int suffix = 0; suffix < ARRAYSIZE( s_pszVoiceSuffixes ); ++suffix )
	{
		for ( int voice = 0; voice < ARRAYSIZE( s_nVoiceIndices ); ++voice )
		{
			Q_snprintf( szSound, sizeof( szSound ), "voicecomm.%d%s",
				s_nVoiceIndices[voice], s_pszVoiceSuffixes[suffix] );
			PrecacheScriptSound( szSound );
		}
	}
}

void CFoF_Player::SpawnFoFHeavyLoadRespawnProp( void )
{
	if ( !( m_nPlayerInfo & 0x800000 ) )
		return;

	const int nReserve = m_nFoFHeavyLoadPropReserve;
	const bool bSpawnProp = nReserve > 0 ||
		!m_bFoFHeavyLoadAlternateSpawn;
	if ( bSpawnProp )
	{
		Vector vecForward;
		EyeVectors( &vecForward );
		Vector vecOrigin = GetAbsOrigin() + vecForward * 60.0f;

		if ( TheNavMesh && TheNavMesh->GetNavArea( vecOrigin, 0.0f ) )
		{
			CNavArea *pArea = TheNavMesh->GetNearestNavArea(
				vecOrigin, false, 10000.0f, false, true, TEAM_ANY );
			if ( pArea )
				vecOrigin = pArea->GetCenter() + Vector( 0.0f, 0.0f, 10.0f );
		}

		const char *pszModel =
			nReserve < 1 ? "models/props_junk/watermelon01_spiky.mdl" :
			nReserve < 3 ? "models/elpaso/barrel1_explosive.mdl" :
			"models/elpaso/barrel2_explosive.mdl";
		CBaseEntity *pProp = CreateEntityByName(
			"prop_physics_multiplayer" );
		if ( pProp )
		{
			pProp->SetAbsOrigin( vecOrigin );
			pProp->KeyValue( "model", pszModel );
			pProp->AddSpawnFlags( SF_PHYSPROP_START_ASLEEP );
			DispatchSpawn( pProp );
			if ( nReserve == 0 )
			{
				pProp->SetThink( &CBaseEntity::SUB_Remove );
				pProp->SetNextThink( gpGlobals->curtime + 60.0f );
			}
		}
	}

	m_bFoFHeavyLoadAlternateSpawn = !m_bFoFHeavyLoadAlternateSpawn;
	m_nFoFHeavyLoadPropReserve = clamp(
		nReserve - ( nReserve > 3 ? 2 : 1 ), 0, 99 );
}

CFoF_Player::~CFoF_Player()
{
	CBaseEntity *pAttached = m_FoFGrabController.GetAttached();
	if ( pAttached )
		m_FoFGrabController.DetachEntity( false );
	if ( m_bFoFResetPickupOwner && pAttached &&
		pAttached->GetOwnerEntity() == this )
	{
		pAttached->SetOwnerEntity( NULL );
	}
	m_bFoFResetPickupOwner = false;
	DismountFoFHorse( true );
}

void CFoF_Player::DoAnimationEvent(
	PlayerAnimEvent_t event, int nData )
{
	CHL2MPPlayerAnimState *pAnimState = GetFoFPlayerAnimState();
	if ( pAnimState )
		pAnimState->DoAnimationEvent( event, nData );

	SendFoFPlayerAnimEvent( event, nData );
}

bool CFoF_Player::HasFoFVotekickVoter( uint32 nAccountID ) const
{
	for ( int i = 0; i < m_FoFVotekickVoters.Count(); ++i )
	{
		if ( m_FoFVotekickVoters[i] == nAccountID )
			return true;
	}
	return false;
}

void CFoF_Player::AddFoFVotekickVoter( uint32 nAccountID )
{
	if ( nAccountID && !HasFoFVotekickVoter( nAccountID ) )
		m_FoFVotekickVoters.AddToTail( nAccountID );
}

void CFoF_Player::UpdateFoFVotekickNameHistory( void )
{
	const char *pszName = GetPlayerName();
	if ( !pszName )
		pszName = "";

	if ( !m_szFoFVotekickDisplayName[0] )
	{
		Q_strncpy( m_szFoFVotekickDisplayName, pszName,
			sizeof( m_szFoFVotekickDisplayName ) );
		Q_strncpy( m_szFoFVotekickLastName, pszName,
			sizeof( m_szFoFVotekickLastName ) );
		return;
	}

	if ( Q_strcmp( m_szFoFVotekickLastName, pszName ) )
	{
		Q_strncpy( m_szFoFVotekickLastName, pszName,
			sizeof( m_szFoFVotekickLastName ) );
		++m_nFoFVotekickNameChanges;
	}
}

void CFoF_Player::Spawn( void )
{
	// FoF resolves the FoF player model before checking the selected
	// respawn point.  This order matters for newly-created fake clients: the
	// no-spawn fallback changes them to spectator, and the inherited team
	// transition otherwise tries to install HL2DM's unprecached Combine model.
	if ( GetModelPtr() == NULL )
		SetPlayerModel();

	CBaseEntity *pSpawnPoint = m_pFoFSpawnPoint;
	if ( !pSpawnPoint )
	{
		ChangeTeam( TEAM_SPECTATOR, false, false, false );
		DevMsg( "NO SPAWN POINT, this shouldn't happen! %s\n",
			GetPlayerName() );
		return;
	}

	DropFoFCarriedObject( false, false );
	DismountFoFHorse( true );
	BaseClass::Spawn();
	ApplyFoFSpawnFOV();
	// The FoF player owns one predicted viewmodel per weapon hand.  The base
	// spawn creates index 0; create index 1 before any left-hand weapon can be
	// deployed or reloaded.
	CreateViewModel( 1 );
	// FoF explicitly restores the gameplay health value after the base
	// spawn reset and publishes 100 through the MaxHP message below.
	SetHealth( 100 );
	// The original FoF spawn path writes 120 to m_flMaxspeed after the stock
	// player reset.  The selected loadout replaces it during the one-shot
	// equipment transaction; until then this is the authoritative menu/spawn
	// speed for humans and bots alike.
	SetMaxSpeed( 120.0f );
	// FoF spawn explicitly clears CBasePlayer's HEV-suit state. Leaving the
	// HL2MP state set makes stock suit and battery HUD elements eligible.
	RemoveSuit();

	// FoF's base-player spawn does not perform the SDK 2013 HL2DM spawn
	// lookup.  Placement comes exclusively from the point selected by the FoF
	// original join/respawn finalizer.
	Vector vecSpawnOrigin = pSpawnPoint->GetAbsOrigin();
	vecSpawnOrigin.z += 1.0f;
	const QAngle angSpawn = pSpawnPoint->GetAbsAngles();
	const Vector vecSpawnVelocity = vec3_origin;
	Teleport( &vecSpawnOrigin, &angSpawn, &vecSpawnVelocity );
	SetLocalAngles( angSpawn );
	SnapEyeAngles( angSpawn );
	pl.v_angle = angSpawn;
	ViewPunchReset();
	// CFoF_Player owns a second spawn-interpolation bit in
	// DT_FoFLocalPlayerExclusive008.  FoF flips it after the selected
	// spawn point has been applied.  The inherited
	// CHL2MP bit is flipped by BaseClass::Spawn, but it does not replace this
	// FoF-local transition signal used by the original client.
	m_bSpawnInterpCounter = !m_bSpawnInterpCounter;
	m_bFoFSpawnPending = false;
	m_bFoFSpawnEquipmentFinalized = false;
	m_nFoFSpawnAttempts = 0;
	UpdateFoFHatAppearance();
	// The belt is an equipment bodygroup, not part of the persistent model.
	// A new loadout will enable it again only if a dynamite belt is granted.
	SetBodygroup( 4, 0 );
	UpdateFoFVotekickNameHistory();
	ApplyFoFClientPreferences();
	ResetFoFSharedSpawnState();
	ResetFoFDeathScoringState();
	m_hFoFGhostGunImpulseSource = NULL;
	m_flFoFGhostGunImpulseTime = 0.0f;
	m_hFoFGhostGunLastCollision = NULL;
	m_flFoFPotionTickTime = 0.0f;
	m_flFoFPotionActivateTime = 0.0f;
	m_flFoFHorseDismountHold = 0.0f;
	m_flNextFoFHandSwitchTime = 0.0f;
	m_flFoFTrackingPickupTime = 0.0f;
	m_nFoFHintKeyButton = 0;
	// The shipped spawn reset selects right-hand accuracy as the baseline.
	// A committed loadout may replace it later in the equipment transaction.
	m_nHandStance = 1;
	m_nPotionLevel = 0;
	m_nPlayerInfo &= ~0x40000;
	for ( int i = 0; i < 25; ++i )
		m_nPlHighlight.Set( i, 0 );
	// Temporary through-wall target highlighting never survives a respawn in
	// the original server (mask 0xFEFFFFFF).
	m_nPlayerInfo &= ~0x01000000;
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef spawnInvulTime( "fof_sv_spawn_invul_time", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 2 &&
		spawnInvulTime.IsValid() )
	{
		SetFoFInvulnerability( spawnInvulTime.GetFloat() );
	}
	else
	{
		m_flFoFInvulnerableUntil = 0.0f;
		m_bFoFInvulnerabilityOwnsPlayerInfoBit = false;
	}
	m_nInBuyZone = 0;
	m_flFoFBuyZoneUntil = 0.0f;
	m_nFoFBuyZoneTier = 1;
	m_flFoFSpawnBuyUntil = 0.0f;
	ClearFoFCrateMenu();
	m_flFoFLifeStartTime = gpGlobals->curtime;
	FoFSendSpawnStateMessage( this );
	FoFSendSpawnHudState( this );
	if ( currentMode.IsValid() && currentMode.GetInt() == 2 &&
		HL2MPRules() && HL2MPRules()->IsFoFTeamplayWarmup() )
	{
		AwardFoFCash( 75.0f, "#Cash_Added" );
	}
	// A burning effect belongs to the previous life.  Extinguish removes the
	// attached flame and emits General.StopBurning through the stock entity
	// flame path when one is present.
	Extinguish();
	SpawnFoFHeavyLoadRespawnProp();
}

void CFoF_Player::GiveAllItems( void )
{
	CBasePlayer::GiveAmmo( 255, "Buckshot" );
	CBasePlayer::GiveAmmo( 32, "357" );
	CBasePlayer::GiveAmmo( 10, "Grenade" );
	CBasePlayer::GiveAmmo( 30, "XBowBolt" );
	CBasePlayer::GiveAmmo( 30, "XBowBolt2" );
	CBasePlayer::GiveAmmo( 26, "Rifle" );
	CBasePlayer::GiveAmmo( 26, "Rifle2" );

	static const char *s_FoFAllWeapons[] =
	{
		"weapon_bow",
		"weapon_bow_black",
		"weapon_xbow",
		"weapon_axe",
		"weapon_machete",
		"weapon_henryrifle",
		"weapon_coltnavy",
		"weapon_coltnavy2",
		"weapon_deringer",
		"weapon_deringer2",
		"weapon_hammerless",
		"weapon_hammerless2",
		"weapon_remington_army",
		"weapon_remington_army2",
		"weapon_maresleg",
		"weapon_maresleg2",
		"weapon_peacemaker",
		"weapon_peacemaker2",
		"weapon_walker",
		"weapon_walker2",
		"weapon_volcanic",
		"weapon_volcanic2",
		"weapon_schofield",
		"weapon_schofield2",
		"weapon_carbine",
		"weapon_coachgun",
		"weapon_dynamite",
		"weapon_dynamite_black",
		"weapon_dynamite_belt",
		"weapon_knife",
		"weapon_sawedoff_shotgun",
		"weapon_sawedoff_shotgun2",
		"weapon_shotgun",
		"weapon_sharps",
		"weapon_spencer",
		"weapon_whiskey",
		"weapon_whiskey2"
	};

	for ( int i = 0; i < ARRAYSIZE( s_FoFAllWeapons ); ++i )
		GiveFoFNamedItem( s_FoFAllWeapons[i] );
}

void CFoF_Player::GiveDefaultItems( void )
{
	CHL2MPRules *pRules = HL2MPRules();
	if ( ( pRules && pRules->IsTeamplay() &&
		GetTeamNumber() < TEAM_COMBINE ) ||
		GetTeamNumber() == TEAM_SPECTATOR || IsFoFBotGhost() )
	{
		return;
	}

	// FoF presents an empty selection before it applies a committed
	// loadout.  Keeping that order prevents a spawn-time purchase from hiding
	// the very menu that is supposed to collect it.
	ShowFoFSpawnEquipmentMenuIfNeeded();
	ApplyFoFSpawnEquipment();

	// FoF CFoF_Player::GiveDefaultItems finishes every equipment path by
	// replenishing the FoF ammunition pools and selecting the heaviest weapon
	// that can currently deploy.  In particular, item_dm_end first equips the
	// fists and then reaches this loop; omitting it leaves a purchased sidearm
	// carried but inactive, so the original client continues to show fists and
	// never runs the purchased weapon's predicted attack/recoil path.
	CBasePlayer::GiveAmmo( 1000, "Buckshot", true );
	CBasePlayer::GiveAmmo( 1000, "357", true );
	CBasePlayer::GiveAmmo( 1000, "XBowBolt", true );
	CBasePlayer::GiveAmmo( 1000, "XBowBolt2", true );
	CBasePlayer::GiveAmmo( 1000, "Rifle", true );
	CBasePlayer::GiveAmmo( 1000, "Rifle2", true );

	// FoF calls CBaseCombatCharacter::SwitchToNextBestWeapon(NULL)
	// here before walking all 48 weapon handles.  This establishes a valid
	// active weapon after the ammunition pools have been replenished.
	SwitchToNextBestWeapon( NULL );

	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
		if ( !pWeapon || pWeapon == pActiveWeapon ||
			pWeapon->FoFWeaponID() == 0 )
			continue;

		if ( !pActiveWeapon ||
			pActiveWeapon->GetWeight() <= pWeapon->GetWeight() )
		{
			Weapon_Switch( pWeapon );
		}
	}

	m_nPlayerInfo &= ~0x800;
}

bool CFoF_Player::FoFReservedQuery479( void ) const
{
	// The original vtable entry always returns false.
	return false;
}

void CFoF_Player::DelayFoFNextAction( void )
{
	// The original override is intentionally empty.
}

void CFoF_Player::RefreshFoFEquipment( void )
{
	// The original override is intentionally empty.
}

void CFoF_Player::AddFoFEquipmentFlags( int nFlags )
{
	// This unsent equipment/profile mask is distinct from the protocol-facing
	// m_nPlayerInfo field.
	m_nFoFEquipmentFlags |= nFlags;
}

void CFoF_Player::SetFoFInvulnerability( float flDuration )
{
	if ( flDuration <= 0.0f )
		return;

	if ( ( m_nPlayerInfo & 0x800 ) == 0 )
	{
		m_nPlayerInfo |= 0x800;
		m_bFoFInvulnerabilityOwnsPlayerInfoBit = true;
	}
	m_flFoFInvulnerableUntil = gpGlobals->curtime + flDuration;
}

void CFoF_Player::ActivateFoFInvulnerability( void )
{
	// The original implementation always grants three seconds.
	SetFoFInvulnerability( 3.0f );
}

void CFoF_Player::SelectFoFEquipment( void )
{
	// The original override is intentionally empty.
}

void CFoF_Player::PlayerRunCommand(
	CUserCmd *pUserCmd, IMoveHelper *pMoveHelper )
{
	// FoF clears this per-command contact latch before trigger touches
	// repopulate it for the current simulation step.
	if ( m_nInBuyZone != 0 )
		m_nInBuyZone = 0;

	static ConVarRef playerAttackAllowed(
		"fof_sv_playerattack_allowed", true );
	if ( pUserCmd && playerAttackAllowed.IsValid() &&
		!playerAttackAllowed.GetBool() )
	{
		pUserCmd->buttons &= ~( IN_ATTACK | IN_ATTACK2 );
	}
	BaseClass::PlayerRunCommand( pUserCmd, pMoveHelper );
}

void CFoF_Player::Touch( CBaseEntity *pOther )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bEligibleTouch =
		!IsFakeClient() && IsAlive() &&
		( !currentMode.IsValid() || currentMode.GetInt() != 6 ) &&
		( pOther->IsPlayer() ||
		  ( pOther->GetOwnerEntity() &&
			pOther->GetOwnerEntity()->IsPlayer() ) );

	if ( !bEligibleTouch )
	{
		m_flFoFStuckTouchTime = 0.0f;
	}
	else
	{
		trace_t trace;
		UTIL_TraceHull(
			GetAbsOrigin(), GetAbsOrigin(),
			WorldAlignMins(), WorldAlignMaxs(),
			MASK_SOLID_BRUSHONLY, this,
			COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		// The shipped server performs this same active-weapon lookup twice.
		// Preserve that behavior rather than folding the redundant calls away.
		if ( trace.startsolid && GetActiveWeapon() && GetActiveWeapon() )
		{
			m_flFoFStuckTouchTime += gpGlobals->frametime;
			if ( m_flFoFStuckTouchTime > 0.25f )
				CommitSuicide( false, false );
		}
	}

	BaseClass::Touch( pOther );
}

void CFoF_Player::HandleAnimEvent( animevent_t *pEvent )
{
	// FoF consumes these two raw player animation events server-side.
	if ( pEvent->event == 43 || pEvent->event == 44 )
		return;

	BaseClass::HandleAnimEvent( pEvent );
}

void CFoF_Player::ImpulseCommands( void )
{
	// FoF accepts player impulses only on a listen server. Dedicated
	// servers leave them to server-side administration instead.
	if ( !engine->IsDedicatedServer() )
		BaseClass::ImpulseCommands();
}

struct FoFBulletDamageRange_t
{
	float flMinimumScale;
	float flMaximumScale;
	float flFalloffStart;
	float flFalloffDistance;
};

static bool FoFIsEitherWeapon(
	CBaseCombatWeapon *pWeapon,
	const char *pszFirst,
	const char *pszSecond = NULL )
{
	const char *pszClassname = pWeapon ? pWeapon->GetClassname() : NULL;
	return pszClassname &&
		( !Q_stricmp( pszClassname, pszFirst ) ||
		  ( pszSecond && !Q_stricmp( pszClassname, pszSecond ) ) );
}

static FoFBulletDamageRange_t FoFGetBulletDamageRange(
	CBaseCombatWeapon *pWeapon,
	const char *pszAmmoName,
	int nHitGroup )
{
	FoFBulletDamageRange_t range = { 0.10f, 1.0f, 250.0f, 1000.0f };

	if ( pszAmmoName && !Q_stricmp( pszAmmoName, "Gatling" ) )
	{
		range.flMinimumScale = 0.75f;
		range.flFalloffDistance = 2500.0f;
	}

	if ( FoFIsEitherWeapon(
		pWeapon, "weapon_deringer", "weapon_deringer2" ) )
	{
		range.flMinimumScale = 0.10f;
		range.flMaximumScale =
			nHitGroup == HITGROUP_HEAD ? 0.75f : 1.0f;
		range.flFalloffStart = 150.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_hammerless", "weapon_hammerless2" ) )
	{
		range.flMinimumScale = 0.05f;
		range.flMaximumScale =
			( nHitGroup == HITGROUP_HEAD ||
			  nHitGroup == HITGROUP_CHEST ) ? 0.85f : 0.75f;
		range.flFalloffStart = 200.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_coltnavy", "weapon_coltnavy2" ) )
	{
		range.flMinimumScale = 0.35f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_volcanic", "weapon_volcanic2" ) )
	{
		range.flMinimumScale = 0.15f;
		range.flMaximumScale =
			nHitGroup == HITGROUP_HEAD ? 0.75f : 1.0f;
		range.flFalloffStart = 150.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_peacemaker", "weapon_peacemaker2" ) )
	{
		range.flMinimumScale = 0.25f;
		range.flMaximumScale = 1.12f;
		range.flFalloffStart = 300.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_remington_army", "weapon_remington_army2" ) )
	{
		range.flMinimumScale = 0.30f;
		range.flMaximumScale = 1.08f;
		range.flFalloffStart = 400.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_schofield", "weapon_schofield2" ) )
	{
		range.flMinimumScale = 0.25f;
		range.flFalloffStart = 350.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_walker", "weapon_walker2" ) )
	{
		range.flMinimumScale = 0.30f;
		range.flMaximumScale = 1.40f;
		range.flFalloffStart = 350.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_mauser", "weapon_mauser2" ) )
	{
		range.flMinimumScale = 0.35f;
		range.flFalloffStart = 350.0f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_maresleg", "weapon_maresleg2" ) )
	{
		range.flMinimumScale = 0.40f;
		range.flMaximumScale = 1.15f;
		range.flFalloffStart = 150.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_coachgun" ) )
	{
		range.flMinimumScale = 0.12f;
		range.flMaximumScale = 1.10f;
		range.flFalloffStart = 300.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_shotgun" ) )
	{
		range.flMinimumScale = 0.40f;
	}
	else if ( FoFIsEitherWeapon(
		pWeapon, "weapon_sawedoff_shotgun",
		"weapon_sawedoff_shotgun2" ) )
	{
		range.flMinimumScale = 0.45f;
		range.flMaximumScale =
			nHitGroup == HITGROUP_HEAD ? 0.48f : 0.52f;
		range.flFalloffStart = 300.0f;
		range.flFalloffDistance = 1220.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_carbine" ) )
	{
		range.flMinimumScale = 0.45f;
		range.flMaximumScale = 0.80f;
		range.flFalloffStart = 250.0f;
		range.flFalloffDistance = 1400.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_henryrifle" ) )
	{
		range.flMinimumScale = 0.55f;
		range.flMaximumScale =
			nHitGroup == HITGROUP_HEAD ? 0.70f : 0.88f;
		range.flFalloffStart = 300.0f;
		range.flFalloffDistance = 1300.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_spencer" ) )
	{
		range.flMinimumScale = 0.60f;
		range.flMaximumScale =
			nHitGroup == HITGROUP_HEAD ? 0.93f : 0.97f;
		range.flFalloffStart = 1.0f;
		range.flFalloffDistance = 2000.0f;
	}
	else if ( FoFIsEitherWeapon( pWeapon, "weapon_sharps" ) )
	{
		range.flMinimumScale = 0.70f;
		range.flFalloffDistance = 3000.0f;
	}

	return range;
}

static void FoFScaleBulletDamageForRange(
	CFoF_Player *pVictim,
	CTakeDamageInfo &info,
	int nHitGroup )
{
	if ( !pVictim ||
		!( info.GetDamageType() & ( DMG_BULLET | DMG_BUCKSHOT ) ) )
	{
		return;
	}

	CFoF_Player *pAttacker = ToFoFPlayer( info.GetAttacker() );
	CBaseCombatWeapon *pWeapon =
		pAttacker ? pAttacker->GetActiveWeapon1() : NULL;
	if ( !pAttacker || !pWeapon )
		return;

	const char *pszAmmoName = "Unknown";
	CAmmoDef *pAmmoDef = GetAmmoDef();
	if ( pAmmoDef && info.GetAmmoType() >= 0 )
	{
		Ammo_t *pAmmo = pAmmoDef->GetAmmoOfIndex( info.GetAmmoType() );
		if ( pAmmo && pAmmo->pName )
			pszAmmoName = pAmmo->pName;
	}

	const FoFBulletDamageRange_t range = FoFGetBulletDamageRange(
		pWeapon, pszAmmoName, nHitGroup );
	const float flDistance =
		( pAttacker->GetAbsOrigin() - pVictim->GetAbsOrigin() ).Length();
	float flScale = range.flMaximumScale;
	if ( flDistance > range.flFalloffStart )
	{
		flScale = RemapValClamped(
			flDistance - range.flFalloffStart,
			0.0f,
			range.flFalloffDistance,
			range.flMaximumScale,
			range.flMinimumScale );
	}
	info.ScaleDamage( flScale );
}

void CFoF_Player::TraceAttack(
	const CTakeDamageInfo &info, const Vector &vecDir,
	trace_t *pTrace, CDmgAccumulator *pAccumulator )
{
	if ( m_flFoFInvulnerableUntil > gpGlobals->curtime ||
		!m_takedamage || !pTrace )
		return;

	static ConVarRef headshotsOnly( "fof_sv_headshots_only", true );
	if ( !engine->IsDedicatedServer() && headshotsOnly.IsValid() &&
		headshotsOnly.GetBool() && pTrace &&
		pTrace->hitgroup != HITGROUP_HEAD )
	{
		return;
	}

	CTakeDamageInfo adjustedInfo = info;
	CBaseEntity *pAttackerEntity = adjustedInfo.GetAttacker();
	if ( pAttackerEntity )
	{
		CAI_BaseNPC *pAttackerNPC = pAttackerEntity->MyNPCPointer();
		if ( pAttackerNPC &&
			( pAttackerNPC->CapabilitiesGet() & bits_CAP_NO_HIT_PLAYER ) &&
			pAttackerNPC->IRelationType( this ) != D_HT )
		{
			return;
		}

		if ( pAttackerEntity->IsPlayer() && g_pGameRules &&
			!g_pGameRules->FPlayerCanTakeDamage(
				this, pAttackerEntity, adjustedInfo ) )
		{
			return;
		}
	}

	FoFScaleBulletDamageForRange(
		this, adjustedInfo, pTrace->hitgroup );

	SetLastHitGroup( pTrace->hitgroup );
	// The original server uses fixed FoF body-part
	// multipliers rather than the stock skill cvars.  Hit group 8 is the hat
	// volume: it drives the separate hat-shot presentation but deals no health
	// damage.  DMG_SHOCK is FoF's headshot marker consumed by death handling.
	switch ( pTrace->hitgroup )
	{
	case HITGROUP_HEAD:
		adjustedInfo.ScaleDamage( 2.0f );
		adjustedInfo.AddDamageType( DMG_SHOCK );
		FoFEmitHeadshotSounds( this );
		KnockOffFoFHat( adjustedInfo, false );
		break;
	case HITGROUP_CHEST:
		adjustedInfo.ScaleDamage( 1.3f );
		break;
	case HITGROUP_STOMACH:
		adjustedInfo.ScaleDamage( 1.25f );
		break;
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		adjustedInfo.ScaleDamage( 1.2f );
		break;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		adjustedInfo.ScaleDamage( 0.75f );
		break;
	case 8:
		// Arrows and buckshot cannot score the separate hat-volume hit in the
		// shipped server.  They still deal no health damage when the trace lands
		// in this hit group; ordinary head hits continue to knock the hat off in
		// HITGROUP_HEAD above.
		if ( !( adjustedInfo.GetDamageType() &
			( DMG_BULLET | DMG_BUCKSHOT ) ) )
		{
			CFoF_Player *pFoFAttacker = ToFoFPlayer( pAttackerEntity );
			if ( pFoFAttacker )
			{
				const int nFragRatio =
					clamp( FragCount(), 5, 1000 ) /
					clamp( DeathCount(), 5, 1000 );
				const int nAward = static_cast< int >( RemapValClamped(
					static_cast< float >( nFragRatio ),
					0.0f, 3.0f, 2.0f, 10.0f ) );
				pFoFAttacker->AccumulateFoFNotoriety(
					nAward, true, IsFakeClient() );
				pFoFAttacker->SendFoFNotorietyNotice(
					6, nAward, 0, NULL );
				FoFFireHatshotEvent( pFoFAttacker );
			}

			KnockOffFoFHat( adjustedInfo, false );
		}
		adjustedInfo.SetDamage( 0.0f );
		break;
	default:
		break;
	}

	const bool bCanShowBlood =
		( !g_pGameRules ||
		  !g_pGameRules->Damage_ShouldNotBleed(
			  adjustedInfo.GetDamageType() ) ) &&
		pTrace->hitgroup != 8 && pAttackerEntity &&
		!m_bIsBotGhost && GetHealth() <= 100 &&
		( pAttackerEntity->IsPlayer() ||
		  pAttackerEntity->MyNPCPointer() != NULL ) &&
		BloodColor() == BLOOD_COLOR_RED &&
		g_MultiDamage.GetDamage() < 12.0f;
	if ( bCanShowBlood )
	{
		SpawnBlood(
			pTrace->endpos, vecDir, BloodColor(),
			adjustedInfo.GetDamage() );
		TraceBleed(
			adjustedInfo.GetDamage(), vecDir, pTrace,
			adjustedInfo.GetDamageType() );
		FoFSendBloodImpact(
			ToFoFPlayer( pAttackerEntity ), pTrace->endpos,
			vecDir, adjustedInfo.GetDamage() );
	}

	AddMultiDamage( adjustedInfo, this );
}

void CFoF_Player::UpdateFoFIdleState( void )
{
	if ( !engine->IsDedicatedServer() || !IsConnected() || IsFakeClient() ||
		IsHLTV() || IsReplay() || IsAutoKickDisabled() )
	{
		return;
	}

	static ConVarRef maxIdle( "fof_sv_maxidle_secs", true );
	if ( !maxIdle.IsValid() || maxIdle.GetFloat() <= 0.0f )
		return;

	const int nPlayerIndex = entindex();
	if ( nPlayerIndex <= 0 ||
		nPlayerIndex >= ARRAYSIZE( s_flFoFLastActiveTime ) )
	{
		return;
	}

	float &flLastActive = s_flFoFLastActiveTime[nPlayerIndex];
	if ( flLastActive <= 0.0f )
		flLastActive = gpGlobals->curtime;

	const CUserCmd *pCommand = GetLastUserCommand();
	if ( m_nButtons != 0 ||
		( pCommand && ( pCommand->buttons != 0 ||
		  pCommand->forwardmove != 0.0f || pCommand->sidemove != 0.0f ||
		  pCommand->upmove != 0.0f ) ) )
	{
		flLastActive = gpGlobals->curtime;
		return;
	}

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode == 2 && GetTeamNumber() > TEAM_SPECTATOR &&
		HL2MPRules() && !HL2MPRules()->IsFoFTeamplayRoundActive() )
	{
		return;
	}

	int nConnectedClients = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( pOther && pOther->IsConnected() )
			++nConnectedClients;
	}

	const float flIdleTime = gpGlobals->curtime - flLastActive;
	const bool bServerFull = nConnectedClients >= gpGlobals->maxClients;
	const bool bSpectatorNearFull =
		GetTeamNumber() == TEAM_SPECTATOR &&
		nConnectedClients >= gpGlobals->maxClients - 2;
	if ( ( bServerFull || bSpectatorNearFull ) &&
		flIdleTime > maxIdle.GetFloat() )
	{
		flLastActive = gpGlobals->curtime;
		engine->ServerCommand( UTIL_VarArgs(
			"kickid %d you were idling while server is full\n", GetUserID() ) );
		return;
	}

	if ( !bServerFull && GetTeamNumber() > TEAM_SPECTATOR &&
		flIdleTime > 30.0f )
	{
		flLastActive = gpGlobals->curtime;
		ChangeTeam( TEAM_SPECTATOR, false, false, true );
	}
}

void CFoF_Player::ItemPostFrame( void )
{
	UpdateFoFIdleState();

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 3 &&
		( gpGlobals->curtime < m_flFoFSpawnBuyUntil ||
		  gpGlobals->curtime < m_flFoFBuyZoneUntil ) )
	{
		// PlayerRunCommand clears the contact latch. BreakBad restores it from
		// the spawn window or the last timed purchase-zone contact.
		m_nInBuyZone = m_nFoFBuyZoneTier;
	}

	// FoF processes these phases in this order.  Carry/use interaction
	// sits between sight expansion and aperture convergence on the server; it
	// can consume IN_USE and alter the active object before weapon dispatch.
	UpdateFoFCaptureInput();
	UpdateFoFDrunkness();
	UpdateFoFKick();
	UpdateFoFSightExpansion();
	UpdateFoFHandSideSwitch();
	UpdateFoFCarry();
	UpdateFoFAccuracyAperture();
	FinalizeFoFSpawnEquipment();
	UpdateFoFHorseRam();

	// Dropping the last active gun leaves both hands empty for a short,
	// deliberate interval.  Restore the owned fists from the weapon-frame
	// path so the newly selected weapon participates in the same ItemPostFrame
	// dispatch as in the shipped game.
	if ( !GetActiveWeapon() && m_flUnarmedTime != 0.0f &&
		m_flUnarmedTime < gpGlobals->curtime )
	{
		m_flUnarmedTime = 0.0f;
		CBaseCombatWeapon *pFists =
			Weapon_OwnsThisType( "weapon_fists", 0 );
		if ( pFists )
			Weapon_Switch( pFists, 0 );
	}

	UpdateFoFPotionReward();

	BaseClass::ItemPostFrame();

	if ( currentMode.IsValid() && currentMode.GetInt() == 3 &&
		IsAlive() && !IsObserver() &&
		( m_nPlayerInfo & 0x4000 ) && ( m_nButtons & IN_ATTACK ) )
	{
		// A voluntary disarm can be ended with primary attack. The original
		// grants brass knuckles with the fists; deployment ends the unarmed state.
		m_nPlayerInfo |= 0x4;
		RestoreFoFUnarmedLoadout();
	}
}

bool CFoF_Player::FoFIsReloading( void )
{
	return
		FoFWeaponHasActualReloadPresentation( GetActiveWeapon1() ) ||
		FoFWeaponHasActualReloadPresentation( GetActiveWeapon2() );
}

int CFoF_Player::GetFoFTotalNotoriety( void )
{
	return m_nFoFTotalNotoriety;
}

bool CFoF_Player::Weapon_Switch(
	CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	if ( !pWeapon || PreventFoFLocalAmmoUse() )
		return false;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( pWeapon->FoFWeaponID() != 0 &&
		currentMode.IsValid() && currentMode.GetInt() == 5 &&
		( GetFlags() & FL_ATCONTROLS ) )
	{
		return false;
	}

	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, "func_tank_fof" ) ) != NULL )
	{
		CFuncTank *pTank = dynamic_cast< CFuncTank * >( pEntity );
		if ( pTank && pTank->GetController() == this )
		{
			ClearUseEntity();
			break;
		}
	}

	if ( GetNextAttack() < gpGlobals->curtime )
		DoAnimationEvent( PLAYERANIMEVENT_CANCEL, 0 );

	// FoF resets the hand-transition network value before attempting
	// the switch, including attempts rejected by the frozen-state gate.
	m_flTransitionSpeed = 0.0f;
	SendFoFClearViewModelParticles();

	if ( GetFlags() & FL_FROZEN )
		return false;

	const bool bSwitched =
		BaseClass::Weapon_Switch( pWeapon, viewmodelindex );
	if ( bSwitched && ( m_nPlayerInfo & 0x10000 ) )
		m_nPlayerInfo &= ~0x10000;

	CBaseViewModel *pViewModel = GetViewModel( 0, true );
	if ( pViewModel && pViewModel->GetModelPtr() )
	{
		const bool bHideFistBodygroups =
			( m_nPlayerInfo & 0x4 ) && pWeapon->FoFWeaponID() == 0;
		pViewModel->SetBodygroup( 1, bHideFistBodygroups ? 1 : 0 );
		SetBodygroup( 3, bHideFistBodygroups ? 1 : 0 );
	}

	const char *pszClassname = pWeapon ? pWeapon->GetClassname() : NULL;
	if ( bSwitched )
	{
		FoFReportCourseStat(
			pszClassname && !Q_stricmp( pszClassname, "weapon_fists" ) ?
				"draw_fists" : "draw_wep",
			this );
	}
	return bSwitched;
}

void CFoF_Player::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_Equip( pWeapon );

	if ( pWeapon &&
		( FClassnameIs( pWeapon, "weapon_ghostgun" ) ||
		  FClassnameIs( pWeapon, "weapon_ghostgun2" ) ) )
	{
		Weapon_Switch( pWeapon, 0 );
	}
}

void CFoF_Player::Weapon_Drop(
	CBaseCombatWeapon *pWeapon,
	const Vector *pvecTarget,
	const Vector *pVelocity )
{
	if ( !pWeapon )
		return;

	// Match CFoF_Player::Weapon_Drop.
	// The weapon-ID 8 family is never handed to the SDK drop path.
	if ( pWeapon->FoFWeaponID() == 8 )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( IsBot() && nMode != 4 && forceWeapons.IsValid() &&
		forceWeapons.GetBool() )
	{
		return;
	}
	if ( nMode == 2 && teamClasses.IsValid() && teamClasses.GetBool() )
		return;

	if ( pWeapon->CanDualWield() &&
		( pWeapon == GetActiveWeapon1() ||
		  pWeapon == GetActiveWeapon2() ) )
	{
		CBaseViewModel *pViewModel = GetViewModel(
			pWeapon->IsSecondGun() ? 1 : 0, true );
		if ( pViewModel )
			pViewModel->AddEffects( EF_NODRAW );
	}

	if ( !IsBot() )
		SendFoFClearViewModelParticles();
	if ( nMode == 6 )
		FoFReportCourseStat( "drop_wep", this );

	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );
}

void CFoF_Player::SendFoFClearViewModelParticles( void )
{
	if ( IsBot() )
		return;

	// Current shipped Event_Killed/Weapon_Drop/Weapon_Switch paths send this
	// reliable player entity message to clear both first-person hand effects.
	EntityMessageBegin( this, true );
		WRITE_BYTE( 8 );
	MessageEnd();
}

void CFoF_Player::ThrowFoFActiveWeapons(
	int nHandSelection, float flPower )
{
	if ( !IsAlive() || PreventFoFLocalAmmoUse() ||
		gpGlobals->curtime < m_flNextFoFWeaponThrowTime )
	{
		return;
	}

	// Hand selection 1 addresses the ordinary active handle and selection 2
	// addresses the off-hand handle.  GetActiveWeapon deliberately falls back
	// to the off hand for a single left-hand weapon.
	const bool bThrowFirst = nHandSelection == 0 || nHandSelection == 1;
	const bool bThrowSecond = nHandSelection == 0 || nHandSelection == 2;
	CHandle< CBaseCombatWeapon > hFirst = GetActiveWeapon();
	CHandle< CBaseCombatWeapon > hSecond = GetActiveWeapon2();
	if ( ( !bThrowFirst || !FoFWeaponCanChargeThrow( hFirst.Get() ) ) &&
		( !bThrowSecond || !FoFWeaponCanChargeThrow( hSecond.Get() ) ) )
	{
		return;
	}

	const bool bRedeployActiveWeapons =
		HasDualActiveWeapons() && FoFIsReloading();
	m_flNextFoFWeaponThrowTime = gpGlobals->curtime + 1.0f;
	Vector vecForward;
	EyeVectors( &vecForward );
	const Vector vecVelocity = vecForward * ( flPower * 180.0f );
	bool bDroppedWeapon = false;
	bool bDroppedSecond = false;
	int nDroppedWeapons = 0;

	if ( bThrowSecond &&
		FoFWeaponCanChargeThrow( hSecond.Get() ) )
	{
		CBaseCombatWeapon *pWeapon = hSecond.Get();
		Weapon_Drop( pWeapon, NULL, &vecVelocity );
		CBaseViewModel *pViewModel = GetViewModel( 1, true );
		if ( pViewModel )
			pViewModel->SetWeaponModel( NULL, NULL );
		bDroppedSecond = pWeapon->GetOwner() != this;
		if ( bDroppedSecond )
			++nDroppedWeapons;
		bDroppedWeapon = bDroppedWeapon || bDroppedSecond;
	}

	// The off hand is processed first.  This matters for selection 0 because
	// dropping it changes the fallback returned by GetActiveWeapon while the
	// first-hand entity remains valid for the second half of the transaction.
	if ( bThrowFirst && ( hFirst != hSecond || !bDroppedSecond ) &&
		FoFWeaponCanChargeThrow( hFirst.Get() ) )
	{
		CBaseCombatWeapon *pWeapon = hFirst.Get();
		Weapon_Drop( pWeapon, NULL, &vecVelocity );
		CBaseViewModel *pViewModel = GetViewModel( 0, true );
		if ( pViewModel )
			pViewModel->SetWeaponModel( NULL, NULL );
		const bool bDroppedFirst = pWeapon->GetOwner() != this;
		if ( bDroppedFirst )
			++nDroppedWeapons;
		bDroppedWeapon = bDroppedWeapon || bDroppedFirst;
	}

	if ( !bDroppedWeapon )
		return;
	FoFRecordAccuracyShots( this, nDroppedWeapons );

	if ( m_nPlayerInfo & 0x200 )
		m_nPlayerInfo &= ~0x400;
	m_nPlayerInfo &= ~0x10;
	if ( flPower > 1.0f )
		EmitSound( "Weapon_Handgun.Throw" );
	if ( !GetActiveWeapon1() && !GetActiveWeapon2() )
		m_flUnarmedTime = gpGlobals->curtime + 0.5f;

	if ( bRedeployActiveWeapons )
	{
		CBaseCombatWeapon *pFirst = GetActiveWeapon1();
		CBaseCombatWeapon *pSecond = GetActiveWeapon2();
		if ( pFirst )
			pFirst->Deploy();
		if ( pSecond )
			pSecond->Deploy();
	}

	m_nPlayerInfo &= ~0x400;
	m_flTransitionSpeed = 0.0f;
	RecalculateWeaponSpeed();

	if ( !HasDualActiveWeapons() )
	{
		CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon1();
		if ( pActiveWeapon &&
			( ( pActiveWeapon->CanDualWield() &&
				( pActiveWeapon->CanFan() || m_nHandStance == 1 ) ) ||
			  pActiveWeapon->FoFWeaponID() == 3 ) )
		{
			m_nPlayerInfo |= 0x10;
		}
	}
}

void CFoF_Player::SetFoFBuyZone( int nTier, float flDuration )
{
	m_nFoFBuyZoneTier = nTier;
	m_flFoFBuyZoneUntil = gpGlobals->curtime + MAX( flDuration, 0.0f );
}

void CFoF_Player::AddFoFCash( float flCash )
{
	const float flOldCash = m_flFoFCash;
	m_flFoFCash = MAX( flOldCash + flCash, 0.0f );
	const int nActualChange = RoundFloatToInt( m_flFoFCash - flOldCash );
	if ( nActualChange == 0 || IsBot() )
		return;

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "Cash" );
		WRITE_LONG( nActualChange );
	MessageEnd();
}

void CFoF_Player::ForceFoFJail( float flReleaseTime )
{
	RemoveAllAmmo();
	RemoveAllItems( true );
	SetMaxSpeed( 120.0f );
	m_flJailTime = flReleaseTime;
	m_nPlayerInfo &= ~0x20000;
	RecalculateWeaponSpeed();
	ActivateFoFInvulnerability();
}

int CFoF_Player::GetFoFInventoryBaseValue( void ) const
{
	int nValue = 0;
	for ( int i = 0; i < WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( !pWeapon )
			continue;

		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionByToken( pWeapon->GetClassname() );
		if ( pItem && pItem->m_nBasePrice > 0 )
			nValue += pItem->m_nBasePrice;
	}
	return nValue;
}

void CFoF_Player::SelfDisarmFoFWeapons( void )
{
	const float flCashReward =
		static_cast< float >( GetFoFInventoryBaseValue() ) * 0.4f;
	RemoveAllAmmo();
	RemoveAllItems( true );
	SetMaxSpeed( 120.0f );
	m_flUnarmedTime = gpGlobals->curtime + 60.0f;
	SetBodygroup( 1, 3 );
	m_nFoFHatModel = 3;
	m_bFoFHatPresent = true;
	AwardFoFCash( flCashReward, "#Cash_Added_SelfDisarm" );
	m_nPlayerInfo |= 0x4000;
	RecalculateWeaponSpeed();
	ActivateFoFInvulnerability();

	if ( !IsBot() )
	{
		CSingleUserRecipientFilter filter( this );
		filter.MakeReliable();
		UserMessageBegin( filter, "BBNotices" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "#bb_selfdisarm_fists_hint" );
		MessageEnd();
	}
}

void CFoF_Player::RestoreFoFUnarmedLoadout( CBaseCombatWeapon *pWeapon )
{
	SetBodygroup( 2, 1 );
	m_flUnarmedTime = 0.0f;
	m_nPlayerInfo &= ~( 0x800 | 0x4000 | 0x20000 );
	CBasePlayer::GiveAmmo( 100, "Buckshot", true );
	CBasePlayer::GiveAmmo( 100, "357", true );
	CBasePlayer::GiveAmmo( 100, "XBowBolt", true );
	CBasePlayer::GiveAmmo( 100, "Rifle", true );
	CBasePlayer::GiveAmmo( 100, "Rifle2", true );

	CBaseCombatWeapon *pFists = Weapon_OwnsThisType( "weapon_fists" );
	if ( !pFists )
	{
		GiveFoFNamedItem( "weapon_fists" );
		pFists = Weapon_OwnsThisType( "weapon_fists" );
	}
	if ( pWeapon )
		Weapon_Switch( pWeapon );
	else if ( !GetActiveWeapon() && pFists )
		Weapon_Switch( pFists );
	RecalculateWeaponSpeed();
}

void CFoF_Player::SetFoFCrateMenuTier(
	int nTier, CBaseEntity *pSource )
{
	m_nFoFCrateMenuTier = clamp( nTier, 1, 3 );
	m_hFoFCrateMenuSource = pSource;
	m_flFoFLastCrateUseTime = gpGlobals->curtime;
}

void CFoF_Player::ClearFoFCrateMenu( void )
{
	m_nFoFCrateMenuTier = -1;
	m_hFoFCrateMenuSource = NULL;
}

void CFoF_Player::UpdateFoFCrateMenuRange( void )
{
	if ( m_nFoFCrateMenuTier < 1 || m_nFoFCrateMenuTier > 3 )
		return;

	CBaseEntity *pSource = m_hFoFCrateMenuSource.Get();
	const float flMaximumDistance = IsBot() ? 120.0f : 75.0f;
	if ( pSource && GetAbsOrigin().DistToSqr( pSource->GetAbsOrigin() ) <=
		flMaximumDistance * flMaximumDistance )
	{
		return;
	}

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "ShowMenuFoF" );
		WRITE_STRING( "" );
		WRITE_BYTE( 0 );
		WRITE_SHORT( -999 );
	MessageEnd();
	ClearFoFCrateMenu();
}

static int FoFSpawnMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static const char *FoFSpawnClassForPlayer( const CFoF_Player *pPlayer )
{
	if ( !pPlayer || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return "info_player_start";

	if ( FoFSpawnMode() == 2 )
	{
		switch ( pPlayer->GetTeamNumber() )
		{
		case 2: return "info_player_vigilante";
		case 3: return "info_player_desperado";
		case 4: return "info_player_bandido";
		case 5: return "info_player_ranger";
		default: return "info_player_vigilante";
		}
	}

	return "info_player_fof";
}

static float FoFFallbackSpawnSafetyRadius(
	CFoF_Player *pPlayer, int nSpawnAttempts )
{
	float flRadius = 900.0f;
	if ( nSpawnAttempts == 0 )
		flRadius = 1300.0f;
	else if ( nSpawnAttempts == 1 )
		flRadius = 1200.0f;
	else if ( nSpawnAttempts == 2 )
		flRadius = 1100.0f;

	// The shipped fallback gives Bots a shorter initial safety radius.
	if ( pPlayer && pPlayer->IsBot() )
		flRadius = 800.0f;

	const int nMapSize = HL2MPRules() ?
		HL2MPRules()->GetFoFMapSize() : 0;
	if ( ( nSpawnAttempts > 5 && nMapSize == 0 ) ||
		nSpawnAttempts > 20 )
	{
		flRadius *= 0.8f;
	}

	return flRadius;
}

static bool FoFFallbackSpawnBlockedByPlayers(
	CFoF_Player *pPlayer, const Vector &vecSpawnOrigin,
	int nSpawnAttempts )
{
	const int nMode = FoFSpawnMode();
	if ( nMode == 2 )
		return false;

	static ConVarRef maxTeams( "fof_sv_maxteams", true );
	const bool bTwoTeamShootout = nMode == 1 && HL2MPRules() &&
		HL2MPRules()->IsTeamplay() && maxTeams.IsValid() &&
		maxTeams.GetInt() == 2;
	const float flBaseSafetyRadius =
		FoFFallbackSpawnSafetyRadius( pPlayer, nSpawnAttempts );

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pOther = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pOther || pOther == pPlayer || !pOther->IsAlive() )
			continue;

		const float flDistance =
			vecSpawnOrigin.DistTo( pOther->GetAbsOrigin() );
		// The ordinary non-probe fallback uses a 128-unit occupancy query.
		if ( flDistance < 128.0f )
			return true;

		if ( nMode != 1 )
			continue;

		// Ghost players participate only in the original 150-unit hard gate.
		if ( pOther->IsFoFBotGhost() )
		{
			if ( flDistance <= 150.0f )
				return true;
			continue;
		}

		const bool bEnemy = FoFPlayersAreEnemies( pOther, pPlayer );
		if ( !bEnemy && !bTwoTeamShootout )
			continue;

		const float flSafetyRadius = bEnemy ?
			flBaseSafetyRadius : flBaseSafetyRadius * 0.75f;

		// A visible enemy inside 2000 units invalidates the fallback point before
		// the path-distance test in the shipped selector.
		if ( bEnemy && flDistance < 2000.0f &&
			pOther->FVisible(
				vecSpawnOrigin + Vector( 0.0f, 0.0f, 70.0f ),
				0x4041, NULL ) )
		{
			return true;
		}

		if ( flDistance >= flSafetyRadius )
			continue;

		CNavArea *pSpawnArea = TheNavMesh ?
			TheNavMesh->GetNearestNavArea(
				vecSpawnOrigin, false, 10000.0f, false, true, TEAM_ANY ) : NULL;
		CNavArea *pPlayerArea = TheNavMesh ?
			TheNavMesh->GetNearestNavArea(
				pOther->GetAbsOrigin(), false, 10000.0f,
				false, true, TEAM_ANY ) : NULL;
		if ( !pSpawnArea || !pPlayerArea )
			return true;

		ShortestPathCost pathCost;
		const float flTowardSpawn = NavAreaTravelDistance(
			pPlayerArea, pSpawnArea, pathCost ) * 0.75f;
		const float flAwayFromSpawn = NavAreaTravelDistance(
			pSpawnArea, pPlayerArea, pathCost ) * 0.75f;
		if ( flTowardSpawn < flSafetyRadius ||
			flAwayFromSpawn < flSafetyRadius )
		{
			return true;
		}
	}

	return false;
}

static CBaseEntity *FoFFindValidSpawnPoint(
	CFoF_Player *pPlayer, CBaseEntity *pStart,
	const char *pszClassname, int nSpawnAttempts,
	CBaseEntity **ppSearchStart )
{
	CBaseEntity *pFirst = gEntList.FindEntityByClassname(
		NULL, pszClassname );
	if ( !pFirst )
		return NULL;

	CBaseEntity *pSpot = pStart;
	if ( !pSpot || Q_stricmp( pSpot->GetClassname(), pszClassname ) )
		pSpot = pFirst;
	else
	{
		pSpot = gEntList.FindEntityByClassname( pSpot, pszClassname );
		if ( !pSpot )
			pSpot = pFirst;
	}

	CBaseEntity *pCandidate = pSpot;
	do
	{
		bool bBlocked = !pCandidate->IsTriggered( pPlayer );
		if ( FoFSpawnMode() == 2 )
		{
			CTeamSpawn *pTeamSpawn = dynamic_cast< CTeamSpawn * >( pCandidate );
			bBlocked = !pTeamSpawn || !pTeamSpawn->IsEnabled();
		}
		if ( !bBlocked )
			bBlocked = FoFFallbackSpawnBlockedByPlayers(
				pPlayer, pCandidate->GetAbsOrigin(), nSpawnAttempts );

		if ( !bBlocked )
		{
			if ( ppSearchStart )
				*ppSearchStart = pCandidate;
			return pCandidate;
		}

		pCandidate = gEntList.FindEntityByClassname(
			pCandidate, pszClassname );
		if ( !pCandidate )
			pCandidate = pFirst;
	} while ( pCandidate != pSpot );

	return NULL;
}

bool CFoF_Player::SelectFoFSpawnPoint( bool bForce )
{
	if ( !bForce && gpGlobals->curtime < m_flFoFNextSpawnAttempt )
		return false;
	if ( bForce )
	{
		// A forced round rebuild follows CleanUpMap, so any raw search cursor
		// into the previous map entity list is no longer valid.
		m_pFoFSpawnPoint = NULL;
		m_pFoFSpawnSearchStart = NULL;
		m_flFoFNextSpawnAttempt = 0.0f;
		m_nFoFSpawnAttempts = 0;
	}

	CBaseEntity *pSpawnPoint = NULL;
	const int nMode = FoFSpawnMode();
	if ( nMode == 5 )
		pSpawnPoint = GetFoFVersusSpawn();
	else if ( nMode == 4 )
	{
		CDarkMode *pMode = dynamic_cast< CDarkMode * >(
			gEntList.FindEntityByClassname( NULL, "fof_elimination" ) );
		if ( pMode )
			pSpawnPoint = pMode->GetRoundSpawnPoint( this );
	}

	// Shootout uses the original danger/visibility-weighted info_player_fof
	// selector.  Its probe map may not be ready during initial activation, in
	// which case the ordinary round-robin path below is the shipped fallback.
	if ( !pSpawnPoint && nMode == 1 && FoFShouldUseDynamicRespawns() )
		pSpawnPoint = FoFSelectRespawnPoint( this );

	const char *pszSpawnClass = FoFSpawnClassForPlayer( this );
	CBaseEntity *pSearchStart = m_pFoFSpawnSearchStart;
	if ( nMode == 2 )
	{
		if ( GetTeamNumber() == TEAM_COMBINE )
			pSearchStart = g_pLastCombineSpawn;
		else if ( GetTeamNumber() == TEAM_REBELS )
			pSearchStart = g_pLastRebelSpawn;
	}
	if ( !pSpawnPoint )
	{
		pSpawnPoint = FoFFindValidSpawnPoint(
			this, pSearchStart, pszSpawnClass,
			m_nFoFSpawnAttempts,
			&m_pFoFSpawnSearchStart );
	}

	if ( !pSpawnPoint && nMode != 2 &&
		Q_stricmp( pszSpawnClass, "info_player_fof" ) )
	{
		pSpawnPoint = FoFFindValidSpawnPoint(
			this, m_pFoFSpawnSearchStart, "info_player_fof",
			m_nFoFSpawnAttempts,
			&m_pFoFSpawnSearchStart );
	}

	if ( !pSpawnPoint )
	{
		++m_nFoFSpawnAttempts;
		m_flFoFNextSpawnAttempt = gpGlobals->curtime + 0.25f;
		m_bFoFSpawnPending = true;
		DevMsg( "_NO SPAWN PLACE FOUND! %s %i\n",
			GetPlayerName(), m_nFoFSpawnAttempts );
		return false;
	}

	m_pFoFLastSpawnPoint = pSpawnPoint;
	if ( nMode == 2 )
	{
		if ( GetTeamNumber() == TEAM_COMBINE )
			g_pLastCombineSpawn = pSpawnPoint;
		else if ( GetTeamNumber() == TEAM_REBELS )
			g_pLastRebelSpawn = pSpawnPoint;
	}
	m_pFoFSpawnPoint = pSpawnPoint;
	m_flFoFNextSpawnAttempt = 0.0f;
	m_nFoFSpawnAttempts = 0;
	m_bFoFSpawnPending = false;
	return true;
}

void CFoF_Player::RetryPendingFoFSpawn( void )
{
	if ( !m_bFoFSpawnPending || !IsConnected() ||
		GetTeamNumber() == TEAM_SPECTATOR || IsHLTV() || IsReplay() )
	{
		return;
	}

	// A pending retry must preserve the original attempt counter so the
	// 1300/1200/1100/900 safety radius can relax on later attempts.  Forced
	// round rebuilds reset the cursor explicitly at their own call sites.
	FinalizeFoFSpawn( false );
}

void CFoF_Player::PrepareFoFRoundRespawn( void )
{
	DropFoFCarriedObject( false, false );
	DismountFoFHorse( true );
	RemoveAllItems( true );
	SetGroundEntity( NULL );
	m_nButtons = 0;
}

bool CFoF_Player::FinalizeFoFSpawn( bool bForce )
{
	if ( GetTeamNumber() == TEAM_SPECTATOR )
	{
		if ( !IsObserver() )
			State_Transition( STATE_OBSERVER_MODE );
		return false;
	}

	static ConVarRef warmup( "fof_warmup", true );
	if ( !bForce && FoFSpawnMode() == 5 &&
		( !warmup.IsValid() || !warmup.GetBool() ) )
	{
		// Versus participants wait for the arena scheduler after warmup.
		// Ordinary death input must not respawn a resolved match participant.
		if ( !IsObserver() )
			State_Transition( STATE_OBSERVER_MODE );
		return false;
	}

	// The round controller can keep eliminated players in observation.
	// Only its forced round rebuild bypasses this gate.
	static ConVarRef forceSpectator( "fof_sv_force_spect", true );
	if ( !bForce && forceSpectator.IsValid() && forceSpectator.GetBool() )
	{
		if ( !IsObserver() )
			State_Transition( STATE_OBSERVER_MODE );
		return false;
	}

	if ( !SelectFoFSpawnPoint( bForce ) )
		return false;

	if ( FoFSpawnMode() == 5 && IsObserver() )
	{
		StopObserverMode();
		State_Transition( STATE_ACTIVE );
	}

	Spawn();
	return IsAlive();
}

bool CFoF_Player::FinalizeFoFSpawnAt(
	const Vector &origin, const QAngle &angles )
{
	CBaseEntity *pSpawnPoint = CreateEntityByName( "info_player_fof" );
	if ( !pSpawnPoint )
		return false;

	pSpawnPoint->SetAbsOrigin( origin );
	pSpawnPoint->SetAbsAngles( angles );
	m_pFoFSpawnPoint = pSpawnPoint;
	m_pFoFSpawnSearchStart = NULL;
	m_flFoFNextSpawnAttempt = 0.0f;
	m_nFoFSpawnAttempts = 0;
	m_bFoFSpawnPending = false;
	Spawn();

	const bool bSpawned = IsAlive();
	m_pFoFSpawnPoint = NULL;
	UTIL_RemoveImmediate( pSpawnPoint );
	if ( bSpawned )
	{
		const Vector velocity = vec3_origin;
		Teleport( &origin, &angles, &velocity );
	}
	return bSpawned;
}

void CFoF_Player::UpdateOnRemove()
{
	const int nPlayerIndex = entindex();
	if ( nPlayerIndex > 0 && nPlayerIndex < ARRAYSIZE( s_flFoFLastActiveTime ) )
		s_flFoFLastActiveTime[nPlayerIndex] = 0.0f;

	BaseClass::UpdateOnRemove();
}

void CFoF_Player::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();
}

void CFoF_Player::PreThink()
{
	UpdateFoFIdleState();
	BaseClass::PreThink();
}

void CFoF_Player::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
	if ( !IsBot() )
		BaseClass::VPhysicsShadowUpdate( pPhysics );
}

void CFoF_Player::PlayerDeathThink()
{
	UpdateFoFIdleState();

	// FoF owns the complete death state machine instead of delegating to
	// CHL2MP_Player/CBasePlayer.  In particular, the stock LIFE_RESPAWNABLE
	// branch calls the global respawn() helper and bypasses FoF spawn selection.
	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( GetFlags() & FL_ONGROUND )
	{
		const float flSpeed = GetAbsVelocity().Length() - 20.0f;
		if ( flSpeed <= 0.0f )
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vecVelocity = GetAbsVelocity();
			VectorNormalize( vecVelocity );
			SetAbsVelocity( vecVelocity * flSpeed );
		}
	}

	if ( HasWeapons() )
		PackDeadPlayerItems();

	if ( GetModelIndex() && !IsSequenceFinished() &&
		m_lifeState == LIFE_DYING )
	{
		StudioFrameAdvance();
		++m_iRespawnFrames;
		if ( m_iRespawnFrames < 60 )
			return;
	}

	if ( m_lifeState == LIFE_DYING )
	{
		m_lifeState = LIFE_DEAD;
		m_flDeathAnimTime = gpGlobals->curtime;
	}

	StopAnimation();
	IncrementInterpolationFrame();
	m_flPlaybackRate = 0.0f;
	if ( FoFShouldBlockCourseRespawn( this ) )
	{
		m_lifeState = LIFE_DEAD;
		m_nButtons = 0;
		SetNextThink( TICK_NEVER_THINK );
		return;
	}

	int nAnyButton = m_nButtons & ~IN_SCORE;
	if ( ( nAnyButton & IN_DUCK ) && GetToggledDuckState() )
		nAnyButton &= ~IN_DUCK;

	// FoF requires a completely released input frame before a dead
	// player becomes respawnable.  This prevents a held movement/fire key from
	// skipping the death presentation.
	if ( m_lifeState == LIFE_DEAD )
	{
		if ( nAnyButton != 0 )
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
			m_lifeState = LIFE_RESPAWNABLE;
		return;
	}

	if ( m_lifeState != LIFE_RESPAWNABLE )
		return;

	// FoF enters the death observer after 1.5 seconds; the SDK default waits
	// DEATH_ANIMATION_TIME (3 seconds).
	if ( g_pGameRules->IsMultiplayer() &&
		gpGlobals->curtime > m_flDeathTime + 1.5f && !IsObserver() )
	{
		StartObserverMode( m_iObserverLastMode );
	}

	if ( nAnyButton == 0 )
		return;

	// FoF's death hint selects one explicit continue key.  During the
	// first two seconds no input may skip the death presentation; for the next
	// six seconds only that advertised key is accepted.  After eight seconds
	// the ordinary any-key fallback is restored.
	if ( gpGlobals->curtime <= m_flDeathTime + 2.0f )
		return;
	if ( m_nFoFHintKeyButton != 0 &&
		gpGlobals->curtime < m_flDeathTime + 8.0f &&
		( m_nButtons & m_nFoFHintKeyButton ) == 0 )
	{
		return;
	}

	m_nFoFHintKeyButton = 0;
	m_nButtons = 0;
	m_iRespawnFrames = 0;
	SetThink( NULL );
	if ( IsObserver() )
	{
		StopObserverMode();
		State_Transition( STATE_ACTIVE );
	}
	FinalizeFoFSpawn( false );
	SetNextThink( TICK_NEVER_THINK );
}

void CFoF_Player::ResetScores()
{
	// FoF resets FoF round counters before the ordinary frag/death pair.
	m_nPlayerKills = 0;
	m_nLastRoundNotoriety = 0;
	m_nFoFTotalNotoriety = 0;
	BaseClass::ResetScores();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF-specific player activation notifications.
//
//=============================================================================//

void FoFPreparePlayerConnection( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	// FoF deliberately does not Spawn here.  A newly connected player
	// first becomes a roaming spectator; the later team/loadout commands own
	// the first playable spawn.
	// FoF calls the four-argument team transition with auto-team and
	// silent both set before entering roaming observer mode.
	pPlayer->ChangeTeam( TEAM_SPECTATOR, false, true, true );
	pPlayer->StartObserverMode( OBS_MODE_ROAMING );

	CBaseEntity *pSpot =
		gEntList.FindEntityByClassname( NULL, "info_player_start" );
	if ( !pSpot )
	{
		pSpot = gEntList.FindEntityByClassname(
			NULL, "info_player_fof" );
	}

	if ( pSpot )
	{
		Vector vecOrigin = pSpot->GetAbsOrigin();
		vecOrigin.z += 50.0f;
		const QAngle angView = pSpot->GetAbsAngles();
		const Vector vecVelocity = vec3_origin;
		pPlayer->Teleport( &vecOrigin, &angView, &vecVelocity );
		pPlayer->SnapEyeAngles( angView );
	}

	if ( engine->IsDedicatedServer() )
		engine->ClientCommand( pPlayer->edict(), "fof_listenserver 0\n" );
}

static const char *FoFModeSlideName()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	switch ( currentMode.IsValid() ? currentMode.GetInt() : 1 )
	{
	case 2: return "mode_teamplay";
	case 3: return "mode_breakbad";
	case 4: return "mode_elimination";
	case 5: return "mode_versus";
	case 6: return "mode_course";
	default: return "mode_shootout";
	}
}

void FoFPlayerActivated( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	IGameEvent *pEvent = gameeventmanager ?
		gameeventmanager->CreateEvent( "player_connect_fof" ) : NULL;
	if ( pEvent )
	{
		pEvent->SetInt( "userid", pPlayer->GetUserID() );
		gameeventmanager->FireEvent( pEvent );
	}

	// FoF sends FoFSlide for both listen and dedicated servers.  On a
	// listen server the shipped client skips the MOTD/intro and uses the
	// slide's exit action to open CTeamMenuFoF immediately; remote clients
	// stage the same slide behind the server MOTD.
	if ( pPlayer->IsBot() )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "FoFSlide" );
		WRITE_STRING( FoFModeSlideName() );
	MessageEnd();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-side application of client userinfo preferences.
//
//=============================================================================//

static int FoFReadClientPreference(
	const CFoF_Player *pPlayer, const char *pszName, int nBotValue )
{
	if ( !pPlayer || pPlayer->IsFakeClient() )
		return nBotValue;
	if ( !pPlayer->IsNetClient() )
		return 0;

	const char *pszValue = engine->GetClientConVarValue(
		pPlayer->entindex(), pszName );
	return pszValue ? Q_atoi( pszValue ) : 0;
}

void CFoF_Player::ApplyFoFSpawnFOV()
{
	int nFOV = m_nFoFPendingFOV > 0 ?
		m_nFoFPendingFOV : m_nFoFPlayerFOV.Get();
	if ( nFOV <= 0 )
		nFOV = 90;
	nFOV = clamp( nFOV, 75, 90 );

	SetDefaultFOV( nFOV );
	SetFOV( this, 0, 0.0f );
	m_nFoFPlayerFOV = nFOV;
	m_nFoFPendingFOV = 0;
}

void CFoF_Player::ApplyFoFClientPreferences()
{
	// FoF clears all 26 protocol-facing player-info bits at the start of
	// this spawn reset, then restores map actor flags and user preferences.
	m_nPlayerInfo = 0;
	m_nFoFEquipmentFlags = 0;
	m_bFoFInvulnerabilityOwnsPlayerInfoBit = false;
	if ( HasSpawnFlags( 0x800 ) )
		m_nPlayerInfo |= 0x800;
	if ( HasSpawnFlags( 0x400000 ) )
		m_nPlayerInfo |= 0x400000;

	if ( FoFReadClientPreference( this, "fof_bodyawareness", 1 ) > 0 )
		m_nPlayerInfo |= 0x2;
	if ( FoFReadClientPreference( this, "fof_autoreload", 1 ) > 0 )
		m_nPlayerInfo |= 0x20;
	if ( FoFReadClientPreference( this, "fof_secondary_toggle", 0 ) > 0 )
		m_nPlayerInfo |= 0x200;
	m_bFoFRifleCrosshair =
		FoFReadClientPreference( this, "fof_crosshair_rifle", 0 ) == 1;

	// The original keeps this outside m_nPlayerInfo because it controls which
	// recipients remain in server-created voice and death-sound filters.
	m_bFoFPlayTaunts =
		FoFReadClientPreference( this, "fof_play_taunts", 0 ) == 1;

	// Match the original FoF server.  The original client
	// obscures its Western Pass value by adding 911 for every player edict
	// index.  The server removes that bias once, caches the decoded value and
	// replicates it to the equipment menu.  Bots use the original 10000
	// sentinel; a listen server grants its local player the 10001 bypass.
	if ( m_nFoFProgressionCache == 0 )
	{
		if ( IsFakeClient() )
		{
			m_nProgression = 10000;
		}
		else
		{
			const int nEncodedProgression = FoFReadClientPreference(
				this, "fof_browserseefull", 0 );
			const int nProgression =
				nEncodedProgression - entindex() * 911;
			m_nProgression = nProgression;
			m_nFoFProgressionCache = nProgression;
		}

		if ( !engine->IsDedicatedServer() )
			m_nProgression = 10001;
	}
}

static const char *s_FoFPlayerModels[] =
{
	"models/playermodels/player1.mdl",
	"models/playermodels/player2.mdl",
	"models/playermodels/bandito.mdl",
	"models/playermodels/frank.mdl",
	"models/zombies/fof_zombie.mdl",
};

static const char *s_FoFHatModels[] =
{
	"models/hats/hat1.mdl",
	"models/hats/hat2.mdl",
	"models/hats/hat3.mdl",
	"models/hats/hat4.mdl",
	"models/hats/hat5.mdl",
	"models/hats/hat6.mdl",
	"models/hats/hat7.mdl",
	"models/hats/hat8.mdl",
};

static int FoFCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static int FoFLastActiveTeam()
{
	static ConVarRef maxTeams( "fof_sv_maxteams", true );
	const int nMaxTeams = maxTeams.IsValid() ? maxTeams.GetInt() : 4;
	return clamp( nMaxTeams, 1, 4 ) + 1;
}

static bool FoFUsesTeamplay()
{
	return HL2MPRules() && HL2MPRules()->IsTeamplay();
}

static bool FoFUsesRandomPlayerAppearance( const CFoF_Player *pPlayer )
{
	return !FoFUsesTeamplay() ||
		( pPlayer && FoFCurrentMode() == 1 &&
		  pPlayer->GetTeamNumber() == FOF_TEAM_UNASSIGNED );
}

static int FoFTeamPlayerCount( int nTeam )
{
	CTeam *pTeam = GetGlobalTeam( nTeam );
	return pTeam ? pTeam->GetNumPlayers() : 0;
}

static int FoFFindAutoTeam()
{
	const int nLastTeam = FoFLastActiveTeam();
	int nSelectedTeam = FOF_TEAM_VIGILANTES;
	int nSelectedCount = FoFTeamPlayerCount( nSelectedTeam );

	for ( int nTeam = FOF_TEAM_DESPERADOS; nTeam <= nLastTeam; ++nTeam )
	{
		const int nPlayers = FoFTeamPlayerCount( nTeam );
		if ( nPlayers < nSelectedCount )
		{
			nSelectedTeam = nTeam;
			nSelectedCount = nPlayers;
		}
	}

	return nSelectedTeam;
}

static bool FoFCanSelectActiveTeam( int nTeam )
{
	if ( nTeam < FOF_TEAM_VIGILANTES || nTeam > FoFLastActiveTeam() )
		return false;

	static ConVarRef balanceAllowed(
		"fof_sv_teambalance_allowed", true );
	const bool bBalanceAllowed =
		!balanceAllowed.IsValid() || balanceAllowed.GetBool();
	if ( !bBalanceAllowed &&
		( FoFCurrentMode() == 2 || !FoFUsesTeamplay() ) )
	{
		return true;
	}

	const int nRequestedPlayers = FoFTeamPlayerCount( nTeam );
	for ( int nOtherTeam = FOF_TEAM_VIGILANTES;
		nOtherTeam <= FoFLastActiveTeam(); ++nOtherTeam )
	{
		if ( nOtherTeam != nTeam &&
			FoFTeamPlayerCount( nOtherTeam ) < nRequestedPlayers )
		{
			return false;
		}
	}

	return true;
}

static int FoFRemappedTeamAppearance( int nTeam )
{
	static ConVarRef teamRemap1( "fof_sv_team_remap_1", true );
	static ConVarRef teamRemap2( "fof_sv_team_remap_2", true );
	static ConVarRef teamRemap3( "fof_sv_team_remap_3", true );
	static ConVarRef teamRemap4( "fof_sv_team_remap_4", true );

	switch ( nTeam )
	{
	case FOF_TEAM_VIGILANTES:
		return teamRemap1.IsValid() ? teamRemap1.GetInt() : 2;
	case FOF_TEAM_DESPERADOS:
		return teamRemap2.IsValid() ? teamRemap2.GetInt() : 3;
	case FOF_TEAM_BANDIDOS:
		return teamRemap3.IsValid() ? teamRemap3.GetInt() : 4;
	case FOF_TEAM_RANGERS:
		return teamRemap4.IsValid() ? teamRemap4.GetInt() : 5;
	case FOF_TEAM_ZOMBIES:
		return 6;
	default:
		return 0;
	}
}

static void FoFFormatVoiceSoundName(
	char *pszSound, int nSoundSize, int nVoiceStyle, int nSoundIndex )
{
	const char *pszSuffix = "_p4";
	if ( nVoiceStyle == 2 )
		pszSuffix = "_p2";
	else if ( nVoiceStyle == 3 )
		pszSuffix = "";
	else if ( nVoiceStyle == 4 )
		pszSuffix = "_p3";

	Q_snprintf(
		pszSound, nSoundSize, "voicecomm.%d%s", nSoundIndex, pszSuffix );
}

static void FoFFilterTauntRecipients(
	CRecipientFilter &filter, const CFoF_Player *pSpeaker )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pRecipient = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pRecipient && pRecipient != pSpeaker &&
			!pRecipient->PlaysFoFTaunts() )
		{
			filter.RemoveRecipient( pRecipient );
		}
	}
}

int CFoF_Player::GetFoFVoiceStyle( void ) const
{
	// Team modes derive appearance and voice from the four team remaps.
	// Free-for-all modes keep the per-player appearance selected when the
	// player entity was constructed or changed by a special role.
	if ( !FoFUsesRandomPlayerAppearance( this ) )
		return FoFRemappedTeamAppearance( GetTeamNumber() );

	return m_nFoFModelSelection + 2;
}

void CFoF_Player::HandleFoFVoiceCommand(
	int nBiasedSlot, int nMenuKind )
{
	if ( !IsAlive() || GetTeamNumber() == FOF_TEAM_SPECTATOR ||
		( engine->IsDedicatedServer() &&
		  gpGlobals->curtime < m_flNextFoFVoiceTime ) )
	{
		return;
	}

	static const int s_nMenuCounts[] = { 0, 6, 7, 9 };
	if ( nMenuKind < 1 || nMenuKind >= ARRAYSIZE( s_nMenuCounts ) )
		return;

	// The client sends the displayed 1-based slot with the original +1 wire
	// bias.  FoF removes that bias before applying the menu's 0/10/20
	// sound-script bank offset.
	const int nDisplaySlot = nBiasedSlot - 1;
	if ( nDisplaySlot < 1 || nDisplaySlot > s_nMenuCounts[nMenuKind] )
		return;

	int nSoundIndex = nDisplaySlot;
	if ( nMenuKind == 2 )
		nSoundIndex += 10;
	else if ( nMenuKind == 3 )
		nSoundIndex += 20;

	if ( nSoundIndex == 11 )
		FoFReportCourseStat( "say_yeah", this );
	else if ( nSoundIndex == 12 )
		FoFReportCourseStat( "say_no", this );

	char szSound[32];
	FoFFormatVoiceSoundName(
		szSound, sizeof( szSound ), GetFoFVoiceStyle(), nSoundIndex );

	CSoundParameters params;
	if ( !GetParametersForSound( szSound, params, NULL ) )
		return;

	CPASAttenuationFilter soundRecipients( this, params.soundlevel );
	soundRecipients.AddRecipient( this );
	FoFFilterTauntRecipients( soundRecipients, this );
	EmitSound( soundRecipients, entindex(), szSound );

	// The speech bubble is a team-only notification in team modes.  The
	// original deliberately omits it in free-for-all while still playing the
	// voice line to nearby/all eligible recipients.
	if ( FoFUsesTeamplay() && GetTeam() )
	{
		CTeamRecipientFilter iconRecipients( GetTeamNumber() );
		UserMessageBegin( iconRecipients, "IconComm" );
			WRITE_BYTE( entindex() );
		MessageEnd();
	}

	m_flNextFoFVoiceTime = gpGlobals->curtime + 3.0f;
}

void CFoF_Player::SetPlayerModel( void )
{
	int nAppearance = FoFRemappedTeamAppearance( GetTeamNumber() );
	if ( FoFUsesRandomPlayerAppearance( this ) )
		nAppearance = m_nFoFModelSelection + 2;

	const int nModelSelection =
		( nAppearance >= 2 && nAppearance <= 6 ) ?
		nAppearance - 2 : 0;

	const char *pszModel = IsFoFBotGhost() ?
		"models/npc/ghost.mdl" : s_FoFPlayerModels[nModelSelection];
	SetModel( pszModel );
	if ( !IsFakeClient() && IsNetClient() )
	{
		engine->ClientCommand( edict(), UTIL_VarArgs(
			"fof_player_voice %i\n", nModelSelection ) );
	}

	// The shipped function refreshes CBasePlayer's cached pitch pose after
	// changing the studio model.  Hat/bodygroup state is updated by its own
	// helper at the original call sites (spawn, role and team transitions).
	m_nBodyPitchPoseParam = LookupPoseParameter( "body_pitch" );
}

void CFoF_Player::UpdateFoFHatAppearance( void )
{
	if ( IsFoFBotGhost() )
	{
		m_nFoFHatModel = 0;
		m_bFoFHatPresent = false;
		return;
	}

	int nAppearance = FoFRemappedTeamAppearance( GetTeamNumber() );
	if ( FoFUsesRandomPlayerAppearance( this ) )
		nAppearance = m_nFoFModelSelection + 2;

	switch ( nAppearance )
	{
	case 0:
	case FOF_TEAM_VIGILANTES:
		SetBodygroup( 1, 4 );
		m_nFoFHatModel = 4;
		break;
	case FOF_TEAM_DESPERADOS:
		SetBodygroup( 1, 1 );
		m_nFoFHatModel = 2;
		break;
	case FOF_TEAM_BANDIDOS:
		SetBodygroup( 1, 6 );
		m_nFoFHatModel = 6;
		break;
	case FOF_TEAM_RANGERS:
		SetBodygroup( 1, 1 );
		m_nFoFHatModel = 1;
		break;
	case 6:
		SetBodygroup( 1, 0 );
		m_nFoFHatModel = 0;
		break;
	default:
		SetBodygroup( 1, 1 );
		break;
	}

	const int nTeam = GetTeamNumber();
	m_bFoFHatPresent =
		nTeam != FOF_TEAM_SPECTATOR && nTeam != 6;
}

void CFoF_Player::KnockOffFoFHat(
	const CTakeDamageInfo &info, bool bReduceVelocity )
{
	if ( !m_bFoFHatPresent || m_bIsBotGhost ||
		GetTeamNumber() == 6 || m_nFoFHatModel < 1 ||
		m_nFoFHatModel > ARRAYSIZE( s_FoFHatModels ) )
	{
		return;
	}

	Vector vecVelocity = info.GetDamagePosition();
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( pAttacker && pAttacker->IsPlayer() && pAttacker->IsAlive() )
	{
		Vector vecSource = pAttacker->GetAbsOrigin();
		vecSource.z -= 40.0f;
		vecVelocity = vecSource - EyePosition();
		const float flDamage = info.BaseDamageIsValid() ?
			info.GetBaseDamage() : info.GetDamage();
		vecVelocity *= flDamage * -0.15f;
	}
	if ( bReduceVelocity )
		vecVelocity *= 0.15f;

	const int nHatlessSequence = LookupSequence( "hatless" );
	if ( nHatlessSequence >= 0 )
		ResetSequence( nHatlessSequence );
	SetBodygroup( 1, 0 );
	m_bFoFHatPresent = false;

	const int nHatModelIndex =
		PrecacheModel( s_FoFHatModels[m_nFoFHatModel - 1] );
	EntityMessageBegin( this, false );
		WRITE_BYTE( 1 );
		WRITE_VEC3COORD( vecVelocity );
		WRITE_VEC3COORD( info.GetDamageForce() );
		WRITE_SHORT( nHatModelIndex );
	MessageEnd();
}

void CFoF_Player::SetFoFBattleRoyaleRoleAppearance( bool bOutlaw )
{
	// Original Grand Elimination switches to model-selection 3 for outlaws
	// and 2 for lawmen.  Those selections map to player2/player1 after the
	// original team-offset conversion, with bodygroup 8/4 respectively.
	m_nFoFModelSelection = bOutlaw ? 1 : 0;
	const char *pszModel = s_FoFPlayerModels[m_nFoFModelSelection];
	SetModel( pszModel );
	SetupPlayerSoundsByModel( pszModel );
	SetBodygroup( 1, bOutlaw ? 8 : 4 );
	m_nFoFHatModel = 0;
	m_bFoFHatPresent = false;
}

void CFoF_Player::ChangeFoFTeam( int nTeam, bool bDontKill,
	bool bAutoTeam, bool bSilent )
{
	SetFoFVersusSpawn( NULL );

	if ( !FoFUsesTeamplay() && nTeam != FOF_TEAM_SPECTATOR )
		nTeam = FOF_TEAM_UNASSIGNED;

	// A second team change can occur inside CBasePlayer's five-second suicide
	// cooldown.  The shipped team transition still kills a live player every
	// time, so make the team-change suicide eligible before entering HL2MP.
	if ( nTeam != GetTeamNumber() && !bDontKill && IsAlive() )
		m_fNextSuicideTime = gpGlobals->curtime;

	// The shipped CFoF_Player::ChangeTeam first enters the complete
	// CHL2MP_Player team transition.  Calling CBasePlayer directly skips the
	// observer state, team-change timing and suicide/respawn path, leaving a
	// spectator-origin player alive in an observer movement state.
	BaseClass::ChangeTeam( nTeam, bDontKill, bAutoTeam, bSilent );
	m_nFoFTeamClass = -1;

	if ( nTeam != FOF_TEAM_SPECTATOR )
	{
		// CHL2MP_Player still selects an HL2DM team model.  FoF immediately
		// replaces it with the mode/team model after the base transition.
		SetPlayerModel();
	}

	// The original CFoF_Player::ChangeTeam opens the
	// Teamplay class selector after entering an active team.  The four
	// original conditions are: team > spectator, mode 2, at least one class
	// definition, and a non-bot player.
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	CHL2MPRules *pRules = HL2MPRules();
	if ( nTeam > FOF_TEAM_SPECTATOR &&
		currentMode.IsValid() && currentMode.GetInt() == 2 &&
		pRules && pRules->GetFoFTeamClassCount() > 0 && !IsBot() )
	{
		ShowViewPortPanel( "buypreset_main", true );
	}
}

void CFoF_Player::ChangeTeam( int iTeam, bool bDontKill,
	bool bAutoTeam, bool bSilent )
{
	ChangeFoFTeam( iTeam, bDontKill, bAutoTeam, bSilent );
}

bool CFoF_Player::HandleFoFTeamJoin( int nTeam, bool bAutoTeam )
{
	if ( !GetGlobalTeam( nTeam ) || nTeam == GetTeamNumber() )
		return false;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( nTeam > FOF_TEAM_SPECTATOR && currentMode.IsValid() &&
		currentMode.GetInt() == 6 )
	{
		FoFStartCourseMode( NULL );
	}

	// Preserve whether this player already belongs to an active team.  The
	// shipped join finalizer observes the death-time delay after ChangeTeam
	// commits suicide; it does not immediately respawn either a live switcher
	// or a player who changes teams again while dead.  Only an initial join from
	// unassigned/spectator enters the explicit FoF spawn path.
	const bool bExistingActiveTeamPlayer =
		GetTeamNumber() > FOF_TEAM_SPECTATOR;

	if ( nTeam == FOF_TEAM_SPECTATOR )
	{
		if ( !mp_allowspectators.GetBool() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			return false;
		}

		// Match the shipped join-spectator path: an active player dies before
		// the team transition, while the compensating frag keeps this voluntary
		// move from counting as a gameplay suicide.  Letting ChangeTeam enter
		// observer state first makes CommitSuicide a no-op and produces the
		// incorrect silent, instantaneous switch.
		if ( GetTeamNumber() != FOF_TEAM_UNASSIGNED && !IsDead() )
		{
			m_fNextSuicideTime = gpGlobals->curtime;
			CommitSuicide();
			IncrementFragCount( 1 );
		}

		ChangeFoFTeam( FOF_TEAM_SPECTATOR, false, false, false );
		ShowViewPortPanel( "team", false );
		return true;
	}

	if ( FoFUsesTeamplay() )
	{
		if ( nTeam < FOF_TEAM_VIGILANTES || nTeam > FoFLastActiveTeam() )
			return false;
		if ( !bAutoTeam && !FoFCanSelectActiveTeam( nTeam ) )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#FullTeam" );
			return false;
		}
	}
	else if ( nTeam != FOF_TEAM_UNASSIGNED )
	{
		return false;
	}

	// CFoF_Player::HandleFoFTeamJoin only
	// leaves observer mode when PFLAG_OBSERVER is set.  More importantly,
	// the original calls its FoF join-finalization routine
	// after ChangeTeam; that routine selects a spawn and invokes virtual
	// Spawn().  Omitting that final call leaves a newly joined player in the
	// impossible LIFE_DEAD + non-observer state, where FoF movement rejects
	// every command and the client renders an uninitialized player model.
	if ( IsObserver() )
	{
		StopObserverMode();
		State_Transition( STATE_ACTIVE );
	}

	// Free-for-all joins use TEAM_UNASSIGNED as the active gameplay team.
	// Do not expose that internal transition as "joined team Unassigned" in
	// chat; the shipped client receives it as a silent player_team event.
	ChangeFoFTeam( nTeam, false, bAutoTeam,
		nTeam == FOF_TEAM_UNASSIGNED );

	if ( !bExistingActiveTeamPlayer )
		FinalizeFoFSpawn( false );

	ShowViewPortPanel( "team", false );
	return true;
}

bool CFoF_Player::ClientCommand( const CCommand &args )
{
	if ( FoFHandleAIEditorCommand( this, args ) )
		return true;

	if ( FStrEq( args[0], "hintkey" ) )
	{
		static const int s_nFoFHintButtons[] =
		{
			IN_FORWARD,
			IN_BACK,
			IN_JUMP,
			IN_DUCK,
			IN_WALK,
			IN_RELOAD,
			IN_ATTACK
		};
		const int nHint = clamp(
			args.ArgC() >= 2 ? Q_atoi( args[1] ) : 0,
			0, ARRAYSIZE( s_nFoFHintButtons ) - 1 );
		m_nFoFHintKeyButton = s_nFoFHintButtons[nHint];
		return true;
	}

	if ( FStrEq( args[0], "fcp" ) || FStrEq( args[0], "fcn" ) )
	{
		const bool bHasFriend = FStrEq( args[0], "fcp" );
		const int nExpectedValue = bHasFriend ? 77 : 25;
		const int nValue = args.ArgC() >= 2 ? Q_atoi( args[1] ) : 0;
		if ( nValue != nExpectedValue )
			ChangeTeam( FOF_TEAM_UNASSIGNED, false, false, false );

		m_bFoFHasFriendOnTeam = bHasFriend;
		return true;
	}

	if ( FStrEq( args[0], "mco" ) )
	{
		GrantFoFMobileCannonOperatorAccess();
		return true;
	}

	if ( FStrEq( args[0], "spectator" ) )
	{
		if ( gpGlobals->curtime >= m_flNextFoFSpectatorTime )
		{
			m_flNextFoFSpectatorTime = gpGlobals->curtime + 0.3f;
			HandleFoFTeamJoin( FOF_TEAM_SPECTATOR, false );
		}
		return true;
	}

	if ( FStrEq( args[0], "jointeam" ) )
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad jointeam syntax\n" );
			return true;
		}

		HandleFoFTeamJoin( Q_atoi( args[1] ), false );
		return true;
	}

	if ( HandleFoFEquipmentCommand( args ) )
		return true;

	if ( FStrEq( args[0], "vc" ) )
	{
		if ( args.ArgC() >= 3 )
		{
			HandleFoFVoiceCommand(
				Q_atoi( args[1] ), Q_atoi( args[2] ) );
		}
		return true;
	}

	if ( FStrEq( args[0], "tp_class" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		CHL2MPRules *pRules = HL2MPRules();
		const int nClass = Q_atoi( args[1] );
		if ( !pRules || nClass < 0 ||
			nClass >= pRules->GetFoFTeamClassCount() )
		{
			return true;
		}

		if ( !pRules->IsFoFTeamClassAvailable(
			nClass, GetTeamNumber() ) )
		{
			Warning( "Class %i not available!\n", nClass );
			ShowViewPortPanel( "buypreset_main", true );
			return true;
		}

		const bool bFirstSelection = m_nFoFTeamClass == -1;
		m_nFoFTeamClass = nClass;
		if ( IsAlive() && bFirstSelection )
		{
			// The original command reaches CFoF_Player virtual slot 477 here,
			// which is GiveDefaultItems(), not ForceRespawn().
			GiveDefaultItems();
			return true;
		}

		char szClassName[64];
		float flClassShare = 0.0f;
		if ( FoFGetTeamClassDefinition(
			nClass, szClassName, sizeof( szClassName ), flClassShare ) )
		{
			ClientPrint( this, HUD_PRINTTALK,
				UTIL_VarArgs( "You'll respawn as %s", szClassName ) );
		}
		return true;
	}

	if ( FStrEq( args[0], "fof_stat" ) )
	{
		if ( args.ArgC() >= 3 )
		{
			const int nStat = clamp( Q_atoi( args[1] ), 0,
				ARRAYSIZE( m_iFoFPersonalStats ) - 1 );
			m_iFoFPersonalStats[nStat] = Q_atoi( args[2] );
		}
		return true;
	}

	if ( FStrEq( args[0], "fof_stat_acc" ) )
	{
		if ( args.ArgC() < 2 )
		{
			m_nPlayerInfo |= 0x80;
			return true;
		}

		const float flClientAccuracy = Q_atof( args[1] );
		if ( flClientAccuracy > 100.0f )
		{
			m_nPlayerInfo |= 0x80;
		}
		return true;
	}

	if ( FStrEq( args[0], "drop_prop" ) )
	{
		if ( m_bPickupActive )
			DropFoFCarriedObject( true, false );
		return BaseClass::ClientCommand( args );
	}

	if ( FStrEq( args[0], "menuselect_fof" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			const int nCommandId = Q_atoi( args[1] );
			if ( !FoFHandleCourseSelection( this, nCommandId ) &&
				!FoFHandleCourseEndMenuSelection( this, nCommandId ) &&
				!FoFHandleVotekickSelection( this, nCommandId ) )
				HandleFoFCrateMenuSelection( nCommandId );
		}
		return true;
	}

	if ( FStrEq( args[0], "autojoin" ) )
	{
		if ( gpGlobals->curtime < m_flNextFoFAutojoinTime )
			return true;
		m_flNextFoFAutojoinTime = gpGlobals->curtime + 0.3f;

		if ( !FoFUsesTeamplay() )
		{
			HandleFoFTeamJoin( FOF_TEAM_UNASSIGNED, false );
			return true;
		}

		static ConVarRef currentMode( "fof_sv_currentmode", true );
		if ( currentMode.IsValid() && currentMode.GetInt() == 6 )
			FoFStartCourseMode( NULL );

		static ConVarRef forcedTeam( "fof_course_forced_team", true );
		const int nForcedTeam = forcedTeam.IsValid() ? forcedTeam.GetInt() : 0;
		const int nTeam = nForcedTeam >= FOF_TEAM_VIGILANTES &&
			nForcedTeam <= FoFLastActiveTeam() ? nForcedTeam : FoFFindAutoTeam();
		HandleFoFTeamJoin( nTeam, true );
		return true;
	}

	return BaseClass::ClientCommand( args );
}
