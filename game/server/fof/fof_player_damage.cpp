#include "cbase.h"
#include "basegrenade_shared.h"
#include "fof/fof_physics_entities.h"
#include "fof/fof_player.h"
#include "fof/fof_player_equipment.h"
#include "fof/fof_base_revolver.h"
#include "fof/weapon_dynamite.h"
#include "fof/weapon_dynamite_black.h"
#include "fof/weapon_dynamite_belt.h"
#include "fof/fof_weapon_properties.h"
#include "hl2/func_tank.h"
#include "fof/fof_breakbad_mode.h"
#include "fof/fof_rounds.h"
#include "fof/fof_item_catalog.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "nav_area.h"
#include "nav_mesh.h"
#include "particle_parse.h"
#include "team.h"
#include "fof/fof_player_damage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool FoFIsPrimedDeathDynamite( CBaseCombatWeapon *pWeapon )
{
	CWeaponDynamiteBlack *pDynamite =
		dynamic_cast< CWeaponDynamiteBlack * >( pWeapon );
	return pDynamite && pDynamite->IsFoFPrimedDeathDynamite();
}

static void FoFSpawnDeathDynamite(
	CBaseCombatWeapon *pWeapon,
	CFoF_Player *pOwner,
	CBaseEntity *pThrower,
	bool bBlastTriggered )
{
	if ( CWeaponDynamite *pDynamite =
		dynamic_cast< CWeaponDynamite * >( pWeapon ) )
	{
		pDynamite->SpawnFoFDeathDynamite(
			pOwner, pThrower, bBlastTriggered );
		return;
	}
	if ( CWeaponDynamiteBlack *pDynamite =
		dynamic_cast< CWeaponDynamiteBlack * >( pWeapon ) )
	{
		pDynamite->SpawnFoFDeathDynamite(
			pOwner, pThrower, bBlastTriggered );
		return;
	}
	if ( CWeaponDynamiteBelt *pDynamite =
		dynamic_cast< CWeaponDynamiteBelt * >( pWeapon ) )
	{
		pDynamite->SpawnFoFDeathDynamite(
			pOwner, pThrower, bBlastTriggered );
	}
}

static void FoFSendBoilerPlateImpact(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	if ( !pPlayer )
		return;

	Vector vecImpact = info.GetDamagePosition();
	if ( vecImpact == vec3_origin )
		vecImpact = pPlayer->WorldSpaceCenter();

	Vector vecDirection = pPlayer->GetAbsOrigin() - vecImpact;
	if ( VectorNormalize( vecDirection ) <= 0.0f )
		vecDirection.Init( 0.0f, 0.0f, 1.0f );

	QAngle angles;
	VectorAngles( vecDirection, angles );

	pPlayer->EmitSound( "FX_RicochetSound.BoilerPlate" );
	EntityMessageBegin( pPlayer, true );
		WRITE_BYTE( 4 );
		WRITE_VEC3COORD( vecImpact );
		WRITE_ANGLES( angles );
		WRITE_BYTE( 0 );
	MessageEnd();
}

static void FoFAppendDeathHintTag(
	char *pszTags, int nTagBufferSize, const char *pszTag )
{
	if ( !pszTags || nTagBufferSize <= 0 || !pszTag || !pszTag[0] )
		return;
	Q_strncat( pszTags, pszTag, nTagBufferSize );
	Q_strncat( pszTags, ",", nTagBufferSize );
}

static void FoFSendDeathHintTags(
	CFoF_Player *pVictim, const CTakeDamageInfo &info )
{
	if ( !pVictim || pVictim->IsBot() || pVictim->IsFoFBotGhost() )
		return;

	char szTags[512];
	szTags[0] = '\0';
	const int nDamageType = info.GetDamageType();
	if ( nDamageType & DMG_BLAST )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_blast" );
	if ( nDamageType & DMG_FALL )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_fall" );
	if ( nDamageType & DMG_BURN )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_burn" );
	if ( nDamageType & DMG_SLASH )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_slash" );
	if ( nDamageType & DMG_BULLET )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_bullet" );
	if ( nDamageType & DMG_CLUB )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_club" );
	if ( nDamageType & DMG_DROWN )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_drown" );
	if ( nDamageType & DMG_DIRECT )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_kick" );
	if ( nDamageType & DMG_BUCKSHOT )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_buckshot" );
	if ( nDamageType & DMG_SHOCK )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "dmg_headshot" );

	CBaseCombatWeapon *pDamageWeapon = dynamic_cast< CBaseCombatWeapon * >(
		info.GetWeapon() );
	if ( pDamageWeapon )
	{
		char szWeapon[128];
		Q_strncpy( szWeapon, pDamageWeapon->GetClassname(),
			sizeof( szWeapon ) );
		const int nLength = Q_strlen( szWeapon );
		if ( nLength > 0 && szWeapon[nLength - 1] == '2' )
			szWeapon[nLength - 1] = '\0';
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), szWeapon );
		if ( pDamageWeapon->CanDualWield() )
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "revolvers" );
	}

	CBaseEntity *pAttackerEntity = info.GetAttacker();
	if ( pAttackerEntity && pAttackerEntity != pVictim )
	{
		Vector vecToAttacker =
			pAttackerEntity->WorldSpaceCenter() - pVictim->WorldSpaceCenter();
		if ( VectorNormalize( vecToAttacker ) >= 50.0f )
		{
			Vector vecForward;
			pVictim->EyeVectors( &vecForward );
			FoFAppendDeathHintTag( szTags, sizeof( szTags ),
				DotProduct( vecForward, vecToAttacker ) > 0.0f ?
				"enemy_posfront" : "enemy_posback" );
		}
	}

	CBaseCombatWeapon *pActiveWeapon = pVictim->GetActiveWeapon();
	if ( pActiveWeapon && pActiveWeapon->Clip1() == 0 &&
		pActiveWeapon->GetMaxClip1() > 0 )
	{
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "weapon_unloaded" );
	}

	CFoF_Player *pAttacker = ToFoFPlayer( pAttackerEntity );
	if ( pAttacker )
	{
		if ( pAttacker == pVictim )
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "suicide" );
		}
		else if ( HL2MPRules() && HL2MPRules()->IsTeamplay() &&
			pAttacker->GetTeamNumber() == pVictim->GetTeamNumber() )
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "enemy_ff" );
		}
		else if ( pAttacker->GetHealth() > 80 )
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "enemy_health_high" );
		}
		else if ( pAttacker->GetHealth() > 40 )
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "enemy_health_mid" );
		}
		else if ( pAttacker->GetHealth() > 20 )
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "enemy_health_low" );
		}
		else
		{
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "enemy_health_verylow" );
		}
	}

	switch ( pVictim->LastHitGroup() )
	{
	case HITGROUP_GENERIC:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_generic" );
		break;
	case HITGROUP_HEAD:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_head" );
		break;
	case HITGROUP_CHEST:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_chest" );
		break;
	case HITGROUP_STOMACH:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_stomach" );
		break;
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_arm" );
		break;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "hit_leg" );
		break;
	}

	if ( pVictim->GetFoFMultiKill() > 3 )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "random_shooter" );
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	if ( nMode == 2 )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "gm_teamplay" );
	else if ( nMode == 3 )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "gm_breakbad" );
	else if ( nMode == 4 )
	{
		static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
		if ( !battleRoyale.IsValid() || !battleRoyale.GetBool() )
			FoFAppendDeathHintTag( szTags, sizeof( szTags ), "gm_elimination" );
	}
	if ( nMode == 3 && pVictim->GetFoFJailTime() > gpGlobals->curtime )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "bb_jailed" );
	if ( !engine->IsDedicatedServer() )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "listen_server" );
	if ( gEntList.FindEntityByClassname( NULL, "fof_teamplay" ) )
		FoFAppendDeathHintTag( szTags, sizeof( szTags ), "gm_zonecap" );

	if ( !szTags[0] )
		return;
	EntityMessageBegin( pVictim, false );
		WRITE_BYTE( 9 );
		WRITE_STRING( szTags );
		WRITE_SHORT( pVictim->entindex() );
	MessageEnd();
}

static void FoFApplyYellowDynamiteSelfImpulse(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	CBaseEntity *pInflictor = info.GetInflictor();
	if ( !pPlayer || info.GetAttacker() != pPlayer ||
		!( pPlayer->m_nPlayerInfo & 0x80000 ) || !pInflictor ||
		Q_stricmp( pInflictor->GetClassname(), "dynamite_yellow" ) )
	{
		return;
	}

	Vector vecDirection =
		pPlayer->GetAbsOrigin() - info.GetDamagePosition();
	VectorNormalize( vecDirection );
	const float flImpulse = RemapValClamped(
		info.GetDamage(), 5.0f, 25.0f, 50.0f, 500.0f );

	pPlayer->SetGroundEntity( NULL );
	pPlayer->SetAbsVelocity( vec3_origin );
	pPlayer->ApplyAbsVelocityImpulse( vecDirection * flImpulse );
}

void CFoF_Player::SetFoFKicker( CBaseEntity *pKicker )
{
	m_hKicker = pKicker;
	m_flFoFKickerTime = pKicker ? gpGlobals->curtime : 0.0f;
}

void CFoF_Player::UpdateFoFKickerAttribution()
{
	if ( !m_hKicker.Get() )
	{
		m_flFoFKickerTime = 0.0f;
		return;
	}

	if ( GetGroundEntity() &&
		gpGlobals->curtime > m_flFoFKickerTime + 1.0f )
	{
		SetFoFKicker( NULL );
	}
}

static bool FoFPlayerControlsFoFTank( CFoF_Player *pPlayer )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, "func_tank_fof" ) ) != NULL )
	{
		CFuncTank *pTank = dynamic_cast< CFuncTank * >( pEntity );
		if ( pTank && pTank->GetController() == pPlayer )
			return true;
	}

	return false;
}

static void FoFDisarmActiveWeapon( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	int nHandSelection = 0;
	CBaseCombatWeapon *pFirst = pPlayer->GetActiveWeapon();
	CBaseCombatWeapon *pSecond = pPlayer->GetActiveWeapon2();
	if ( FoFWeaponCanChargeThrow( pFirst ) )
		nHandSelection += 1;
	if ( pSecond != pFirst && FoFWeaponCanChargeThrow( pSecond ) )
		nHandSelection += 2;

	if ( nHandSelection == 3 )
		nHandSelection = random->RandomInt( 1, 2 );
	if ( nHandSelection != 0 )
		pPlayer->ThrowFoFActiveWeapons( nHandSelection, -1.0f );
}

static void FoFApplyPhysicsPropDamageImpulse(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	if ( !pPlayer || !( info.GetDamageType() & DMG_CRUSH ) )
		return;

	int nWallBounces = 0;
	int nFloorBounces = 0;
	if ( !FoFGetPhysicsPropBounceState(
		info.GetInflictor(), nWallBounces, nFloorBounces ) )
	{
		return;
	}

	CFoF_Player *pScorer = NULL;
	if ( HL2MPRules() )
	{
		pScorer = ToFoFPlayer( HL2MPRules()->GetDeathScorer(
			info.GetAttacker(), info.GetInflictor(), pPlayer ) );
	}
	if ( !pScorer )
		return;

	float flHorizontalImpulse = RemapValClamped(
		info.GetDamage(), 10.0f, 80.0f, 150.0f, 275.0f );
	if ( pScorer->m_nPlayerInfo & 0x800000 )
		flHorizontalImpulse *= 1.3f;
	if ( nFloorBounces > 0 )
		flHorizontalImpulse *= 1.2f;

	if ( nWallBounces > 0 )
	{
		pPlayer->ViewPunch( QAngle(
			-RemapValClamped( info.GetDamage(),
				10.0f, 80.0f, 10.0f, 30.0f ),
			0.0f, 0.0f ) );
	}

	if ( FoFPlayerControlsFoFTank( pPlayer ) )
		return;

	pPlayer->SetGroundEntity( NULL );
	pPlayer->SetAbsVelocity( vec3_origin );
	if ( nWallBounces > 0 )
		FoFDisarmActiveWeapon( pPlayer );

	const Vector vecDamageForce = info.GetDamageForce();
	Vector vecImpulse(
		vecDamageForce.x * flHorizontalImpulse,
		vecDamageForce.y * flHorizontalImpulse,
		( nWallBounces > 0 && nFloorBounces > 0 ) ? 255.0f : 175.0f );

	pPlayer->ApplyAbsVelocityImpulse( vecImpulse );
	pPlayer->SetFoFKicker( pScorer );
}

static bool FoFSpawnExplosionChainDynamite(
	CFoF_Player *pPlayer, CFoF_Player *pAttacker,
	const CTakeDamageInfo &info )
{
	if ( !pPlayer || !pAttacker )
		return false;

	const bool bSelfDamage = pAttacker == pPlayer;
	if ( ( bSelfDamage && info.GetDamage() < 10.0f ) ||
		( !bSelfDamage && info.GetDamage() < 30.0f ) )
	{
		return false;
	}

	CBaseCombatWeapon *pDynamite =
		pPlayer->Weapon_OwnsThisType( "weapon_dynamite_black" );
	if ( pDynamite && pDynamite->HasPrimaryAmmo() &&
		info.GetDamage() >= 35.0f )
	{
		FoFSpawnDeathDynamite( pDynamite, pPlayer, pAttacker, true );
		return true;
	}

	pDynamite = pPlayer->Weapon_OwnsThisType( "weapon_dynamite" );
	if ( pDynamite && pDynamite->HasPrimaryAmmo() )
	{
		FoFSpawnDeathDynamite( pDynamite, pPlayer, pAttacker, true );
		return true;
	}

	if ( bSelfDamage || !( pPlayer->m_nPlayerInfo & 0x80000 ) )
		return false;

	pDynamite = pPlayer->Weapon_OwnsThisType( "weapon_dynamite_belt" );
	if ( !pDynamite )
		return false;

	FoFSpawnDeathDynamite( pDynamite, pPlayer, pAttacker, true );
	return true;
}

static void FoFFireChainReactionEvent( CFoF_Player *pAttacker )
{
	if ( !pAttacker || !gameeventmanager )
		return;

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "chain_reaction" );
	if ( !pEvent )
		return;

	pEvent->SetInt( "entindex_thrower", pAttacker->entindex() );
	gameeventmanager->FireEvent( pEvent );
}

static void FoFApplyExplosionPlayerImpulse(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	if ( !pPlayer || info.GetDamage() < 25.0f ||
		FoFPlayerControlsFoFTank( pPlayer ) )
		return;

	Vector vecDirection = pPlayer->EyePosition() - info.GetDamagePosition();
	VectorNormalize( vecDirection );
	const float flImpulse = RemapValClamped(
		info.GetDamage(), 25.0f, 100.0f, 170.0f, 450.0f );
	pPlayer->SetGroundEntity( NULL );
	pPlayer->SetAbsVelocity( vec3_origin );
	FoFDisarmActiveWeapon( pPlayer );
	// The explosion path scales only the horizontal direction. The shared
	// impact launcher uses a fixed upward speed, independent of blast height.
	pPlayer->ApplyAbsVelocityImpulse( Vector(
		vecDirection.x * flImpulse, vecDirection.y * flImpulse, 175.0f ) );
}

int CFoF_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info( inputInfo );

	// FoF treats health above the normal 100-point limit as a metal
	// plate.  The client receives the same reliable type-4 entity payload used
	// by the shipped server for the impact particle.
	if ( GetHealth() > 100 )
		FoFSendBoilerPlateImpact( this, info );

	FoFApplyPhysicsPropDamageImpulse( this, info );

	// The dynamite belt grants the yellow self-blast jump.  FoF clears
	// the current velocity before applying this remapped radial impulse.
	FoFApplyYellowDynamiteSelfImpulse( this, info );

	return BaseClass::OnTakeDamage( info );
}

void CFoF_Player::OnDamagedByExplosion( const CTakeDamageInfo &info )
{
	// FoF CFoF_Player::OnDamagedByExplosion
	// Chain dynamite and corpse/player impulse are
	// explosion behavior; placing them in OnTakeDamage_Dying added an override
	// that does not exist in the shipped CFoF_Player vtable.
	CFoF_Player *pAttacker = ToFoFPlayer( info.GetAttacker() );
	if ( FoFSpawnExplosionChainDynamite( this, pAttacker, info ) &&
		pAttacker != this &&
		gpGlobals->curtime >= m_flFoFDeathChainReactionTime )
	{
		m_flFoFDeathChainReactionTime = gpGlobals->curtime + 2.0f;
		FoFFireChainReactionEvent( pAttacker );
	}

	if ( info.GetDamage() >= 25.0f )
	{
		KnockOffFoFHat( info, false );
		if ( info.GetDamage() >= 60.0f && info.GetInflictor() )
		{
			UTIL_ScreenShake( info.GetInflictor()->GetAbsOrigin(),
				4.0f, 1.0f, 0.5f, 1000.0f, SHAKE_START );
		}
		FoFApplyExplosionPlayerImpulse( this, info );
		if ( pAttacker )
			SetFoFKicker( pAttacker );
	}

	BaseClass::OnDamagedByExplosion( info );
}

struct FoFKillValueEntry
{
	const char *pszName;
	int nValue;
};

static const FoFKillValueEntry s_FoFKillValues[] =
{
	{ "horse-ram", 9 },
	{ "kick-fall", 20 },
	{ "flame", 20 },
	{ "thrown_gun", 14 },
	{ "kick", 15 },
	{ "blast", 15 },
	{ "physics", 15 },
	{ "dynamite_black", 17 },
	{ "dynamite_yellow", 14 },
	{ "dynamite", 15 },
	{ "arrow", 17 },
	{ "arrow_black", 14 },
	{ "x_arrow", 13 },
	{ "thrown_knife", 16 },
	{ "thrown_axe", 15 },
	{ "thrown_machete", 14 },
	{ "weapon_fists", 20 },
	{ "fists_brass", 17 },
	{ "weapon_ghostgun", 12 },
	{ "weapon_ghostgun2", 12 },
	{ "skill_right", 0 },
	{ "skill_left", 0 },
	{ "skill_fan", 0 },
	{ "skill_ambi", 0 },
	{ "handgun_throw", 0 },
	{ "wall jump", 0 },
	{ "slide", 0 },
	{ "heavyload", 0 },
	{ "brass_knuckles", 0 },
	{ "weapon_knife", 15 },
	{ "boots", 0 },
	{ "weapon_deringer", 16 },
	{ "weapon_deringer2", 16 },
	{ "weapon_dynamite", 0 },
	{ "Wood_Crate", 0 },
	{ "weapon_volcanic", 16 },
	{ "weapon_volcanic2", 16 },
	{ "weapon_coltnavy", 15 },
	{ "weapon_coltnavy2", 15 },
	{ "weapon_axe", 13 },
	{ "weapon_bow", 0 },
	{ "weapon_sawedoff_shotgun", 12 },
	{ "weapon_sawedoff_shotgun2", 12 },
	{ "weapon_hammerless", 14 },
	{ "weapon_hammerless2", 14 },
	{ "weapon_remington_army", 14 },
	{ "weapon_remington_army2", 14 },
	{ "weapon_maresleg", 12 },
	{ "weapon_maresleg2", 12 },
	{ "weapon_schofield", 13 },
	{ "weapon_schofield2", 13 },
	{ "weapon_carbine", 13 },
	{ "weapon_peacemaker", 12 },
	{ "weapon_peacemaker2", 12 },
	{ "weapon_bow_black", 0 },
	{ "weapon_henryrifle", 12 },
	{ "weapon_coachgun", 11 },
	{ "weapon_spencer", 11 },
	{ "weapon_machete", 11 },
	{ "weapon_shotgun", 10 },
	{ "weapon_dynamite_black", 11 },
	{ "weapon_sharps", 10 },
	{ "weapon_walker", 9 },
	{ "weapon_walker2", 9 },
	{ "weapon_dynamite_belt", 0 },
	{ "weapon_whiskey", 0 },
	{ "weapon_whiskey2", 0 },
	{ "weapon_xbow", 0 },
};

// FoF iterates this exact 37-entry server table when a player dies.
// It is intentionally independent of the purchase catalogue: the dormant
// Mauser and both ghost guns are represented, while fists, whiskey and the
// dynamite belt are not death-drop candidates.
static const char *s_FoFDeathDropWeapons[] =
{
	"weapon_hammerless",
	"weapon_hammerless2",
	"weapon_deringer",
	"weapon_deringer2",
	"weapon_coltnavy",
	"weapon_coltnavy2",
	"weapon_volcanic",
	"weapon_volcanic2",
	"weapon_remington_army",
	"weapon_remington_army2",
	"weapon_schofield",
	"weapon_schofield2",
	"weapon_peacemaker",
	"weapon_peacemaker2",
	"weapon_walker2",
	"weapon_walker",
	"weapon_mauser",
	"weapon_maresleg2",
	"weapon_maresleg",
	"weapon_carbine",
	"weapon_henryrifle",
	"weapon_spencer",
	"weapon_sharps",
	"weapon_bow",
	"weapon_bow_black",
	"weapon_xbow",
	"weapon_coachgun",
	"weapon_shotgun",
	"weapon_dynamite",
	"weapon_dynamite_black",
	"weapon_knife",
	"weapon_axe",
	"weapon_machete",
	"weapon_sawedoff_shotgun",
	"weapon_sawedoff_shotgun2",
	"weapon_ghostgun",
	"weapon_ghostgun2",
};

static int FoFBaseKillValue( const char *pszName )
{
	if ( pszName )
	{
		for ( int i = 0; i < ARRAYSIZE( s_FoFKillValues ); ++i )
		{
			if ( !Q_stricmp( pszName, s_FoFKillValues[i].pszName ) )
				return s_FoFKillValues[i].nValue;
		}
	}

	return 0;
}

static const char *FoFKillingWeaponName(
	const CTakeDamageInfo &info, CFoF_Player *pScorer )
{
	if ( info.GetDamageCustom() && g_pGameRules )
	{
		const char *pszCustom = g_pGameRules->GetDamageCustomString( info );
		if ( pszCustom && pszCustom[0] )
			return pszCustom;
	}

	if ( pScorer && pScorer->IsOnFoFHorse() &&
		( info.GetDamageType() & DMG_AIRBOAT ) )
	{
		return "horse-ram";
	}

	CBaseEntity *pWeapon = info.GetWeapon();
	if ( pWeapon && pWeapon->GetClassname() )
		return pWeapon->GetClassname();

	CBaseEntity *pInflictor = info.GetInflictor();
	if ( pInflictor && pInflictor != pScorer &&
		pInflictor->GetClassname() )
	{
		return pInflictor->GetClassname();
	}

	CBaseCombatWeapon *pActiveWeapon =
		pScorer ? pScorer->GetActiveWeapon() : NULL;
	if ( pActiveWeapon )
	{
		const char *pszNoticeName = pActiveWeapon->GetDeathNoticeName();
		if ( pszNoticeName && pszNoticeName[0] )
			return pszNoticeName;
		return pActiveWeapon->GetClassname();
	}

	return NULL;
}

static void FoFSendHitRecon(
	CFoF_Player *pVictim, CFoF_Player *pAttacker,
	const CTakeDamageInfo &info )
{
	if ( !pVictim || !pAttacker || pVictim == pAttacker )
		return;

	const int nDamage = static_cast< int >( info.GetDamage() );
	if ( nDamage <= 0 )
		return;

	const int nStatisticsIndex = FoFItemStatisticsIndex(
		FoFKillingWeaponName( info, pAttacker ) );
	CSingleUserRecipientFilter filter( pAttacker );
	filter.MakeReliable();
	UserMessageBegin( filter, "HitRecon" );
		WRITE_BYTE( nDamage );
		WRITE_VEC3COORD( pVictim->GetAbsOrigin() );
		WRITE_BYTE( nStatisticsIndex );
	MessageEnd();
}

static int FoFFreezeCamPreference( const CFoF_Player *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsNetClient() )
		return 0;

	const char *pszValue = engine->GetClientConVarValue(
		pPlayer->entindex(), "fof_freezecam" );
	if ( !pszValue || !pszValue[0] )
		return 1;

	return clamp( Q_atoi( pszValue ), 0, 2 );
}

static const char *FoFKillCamWeaponName(
	const CTakeDamageInfo &info, CFoF_Player *pScorer )
{
	const char *pszName = FoFKillingWeaponName( info, pScorer );
	if ( !pszName || !pszName[0] )
		return "world";

	if ( !Q_strnicmp( pszName, "weapon_", 7 ) )
		return pszName + 7;
	if ( !Q_strnicmp( pszName, "npc_", 4 ) )
		return pszName + 4;
	if ( !Q_strnicmp( pszName, "func_", 5 ) )
		return pszName + 5;
	if ( Q_stristr( pszName, "physics" ) )
	{
		return ( info.GetDamageType() & DMG_BLAST ) ?
			"blast" : "physics";
	}

	return pszName;
}

static bool FoFShouldUseFreezeCam(
	CFoF_Player *pVictim, CFoF_Player *pScorer, bool bFarKill )
{
	const int nPreference = FoFFreezeCamPreference( pVictim );
	if ( nPreference == 0 || ( nPreference == 2 && !bFarKill ) )
		return false;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !currentMode.IsValid() )
		return false;

	const int nMode = currentMode.GetInt();
	if ( nMode == 3 )
		return true;
	if ( nMode != 1 )
		return false;

	return bFarKill || pScorer->GetFoFMultiKill() > 10;
}

static void FoFStartFreezeCam(
	CFoF_Player *pVictim, CFoF_Player *pScorer,
	const CTakeDamageInfo &info )
{
	if ( !pVictim || pVictim->IsBot() ||
		!pScorer || pScorer == pVictim )
	{
		return;
	}

	// FoF selects the killer before consulting the per-client freeze-cam
	// preference. This leaves the normal death camera with the same preferred
	// observer target even when the freeze frame itself is disabled.
	pVictim->SetObserverTarget( pScorer );

	const bool bFarKill =
		pVictim->GetAbsOrigin().DistTo( pScorer->GetAbsOrigin() ) > 1500.0f;
	if ( !FoFShouldUseFreezeCam( pVictim, pScorer, bFarKill ) )
		return;

	pVictim->SetObserverMode( OBS_MODE_FREEZECAM );

	char szPlayerName[64];
	char szHealth[32];
	char szWeaponName[64];
	Q_snprintf( szPlayerName, sizeof( szPlayerName ),
		"%s", pScorer->GetPlayerName() );
	Q_snprintf( szHealth, sizeof( szHealth ),
		"%i", pScorer->GetHealth() );
	Q_snprintf( szWeaponName, sizeof( szWeaponName ),
		"%s", FoFKillCamWeaponName( info, pScorer ) );

	CSingleUserRecipientFilter filter( pVictim );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 0 );
		WRITE_STRING( "#KillCam_Msg" );
		WRITE_STRING( szPlayerName );
		WRITE_STRING( szHealth );
		WRITE_STRING( szWeaponName );
	MessageEnd();
}

static int FoFKillNotoriety(
	const CTakeDamageInfo &info, CFoF_Player *pScorer )
{
	const char *pszKillName = FoFKillingWeaponName( info, pScorer );
	const int nValue = FoFBaseKillValue( pszKillName );
	if ( pScorer && pszKillName &&
		!Q_stricmp( pszKillName, "physics" ) )
	{
		pScorer->RecordFoFHeavyLoadPhysicsKill();
	}
	return nValue > 0 ? nValue : 10;
}

static int FoFAssistNotoriety( int nDamage )
{
	return RoundFloatToInt( RemapValClamped(
		static_cast< float >( nDamage ),
		40.0f, 90.0f, 4.0f, 12.0f ) );
}

static bool FoFIsEnemyPlayer(
	CFoF_Player *pAttacker, CFoF_Player *pVictim )
{
	if ( !pAttacker || !pVictim || pAttacker == pVictim )
		return false;

	return !g_pGameRules ||
		g_pGameRules->PlayerRelationship(
			pAttacker, pVictim ) != GR_TEAMMATE;
}

static bool FoFCanAwardDeathCash(
	CFoF_Player *pVictim, CFoF_Player *pScorer )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !pVictim || !pScorer ||
		!currentMode.IsValid() || currentMode.GetInt() != 1 ||
		pScorer->IsBot() || !pScorer->HasFoFProfileData() )
	{
		return false;
	}

	return pVictim->IsBot() || pVictim->HasFoFProfileData();
}

static float FoFDeathCashReward(
	CFoF_Player *pVictim, int nNotoriety )
{
	const float flVictimScale = pVictim->IsBot() ? 0.1f : 0.2f;
	const float flExperienceScale = RemapValClamped(
		static_cast< float >( pVictim->GetFoFExperience() ),
		0.0f, 10000.0f, 1.0f, 5.0f );
	return clamp(
		static_cast< float >( nNotoriety ) *
			flVictimScale * flExperienceScale,
		1.0f, 100.0f );
}

static void FoFSendNemesisNotice(
	CFoF_Player *pPlayer, const char *pszToken,
	const char *pszArgument1, const char *pszArgument2 = "" )
{
	if ( !pPlayer || !pszToken )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 0 );
		WRITE_STRING( pszToken );
		WRITE_STRING( pszArgument1 ? pszArgument1 : "" );
		WRITE_STRING( pszArgument2 ? pszArgument2 : "" );
		WRITE_STRING( "" );
	MessageEnd();
}

static void FoFPresentNewNemesis( CFoF_Player *pVictim )
{
	if ( !pVictim )
		return;

	pVictim->EmitSound( "Course.Stinger2" );
	DispatchParticleEffect(
		"bigboom_blood", pVictim->GetAbsOrigin(), vec3_angle, pVictim );
}

static void FoFPresentNemesisRevenge( CFoF_Player *pVictim )
{
	if ( !pVictim )
		return;

	for ( int i = 1; i <= 5; ++i )
	{
		Vector vecOrigin;
		QAngle angles;
		if ( pVictim->GetAttachment( i, vecOrigin, angles ) )
		{
			DispatchParticleEffect(
				"nemesis_smoke", vecOrigin, angles, pVictim );
		}
	}
}

static void FoFSendHeadshotPresentation(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	const int nDamageType = info.GetDamageType();
	if ( !pPlayer || !( nDamageType & DMG_SHOCK ) ||
		( nDamageType & DMG_BUCKSHOT ) )
	{
		return;
	}

	// FoF Event_Killed sends presentation message 19 reliably to the
	// victim entity.  The client places the head-shot droplets at the damage
	// position and orients them against the normalized incoming force.
	Vector vecNormal = info.GetDamageForce();
	VectorNormalize( vecNormal );
	vecNormal.Negate();

	EntityMessageBegin( pPlayer, true );
		WRITE_BYTE( 19 );
		WRITE_VEC3COORD( info.GetDamagePosition() );
		WRITE_VEC3NORMAL( vecNormal );
	MessageEnd();
}

static void FoFSendGroggyDamagePresentation(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	if ( !pPlayer || info.GetDamage() <= 30.0f ||
		!( info.GetDamageType() &
			( DMG_BULLET | DMG_BLAST | DMG_BUCKSHOT ) ) )
	{
		return;
	}

	// The original server damage path maps damage from 30 to
	// 70 maps to a one-to-four-second local EP2 groggy presentation.
	const int nDuration = static_cast< int >( RemapValClamped(
		info.GetDamage(), 30.0f, 70.0f, 1.0f, 4.0f ) );
	EntityMessageBegin( pPlayer, false );
		WRITE_BYTE( 12 );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( nDuration );
	MessageEnd();
}

static bool FoFShouldDropDeathInventory( CFoF_Player *pPlayer )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;

	if ( pPlayer->IsFakeClient() && nMode != 4 &&
		forceWeapons.IsValid() && forceWeapons.GetBool() )
	{
		return false;
	}

	return nMode != 2 || !teamClasses.IsValid() ||
		!teamClasses.GetBool();
}

static bool FoFIsShootoutDynamite( const char *pszClassname )
{
	return pszClassname &&
		( !Q_stricmp( pszClassname, "weapon_dynamite" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_black" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_belt" ) );
}

static void FoFDisarmDeathWeaponThrows( CFoF_Player *pPlayer )
{
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CFoFBaseRevolver *pRevolver =
			dynamic_cast< CFoFBaseRevolver * >( pPlayer->GetWeapon( i ) );
		if ( pRevolver )
			pRevolver->ClearFoFThrower();
	}
}

static void FoFDropDeathInventory( CFoF_Player *pPlayer )
{
	if ( !FoFShouldDropDeathInventory( pPlayer ) )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bShootout = !currentMode.IsValid() ||
		currentMode.GetInt() == 1;
	const Vector vecVelocity = pPlayer->BodyDirection2D() * 20.0f;

	for ( int i = 0; i < ARRAYSIZE( s_FoFDeathDropWeapons ); ++i )
	{
		const char *pszClassname = s_FoFDeathDropWeapons[i];
		CBaseHL2MPCombatWeapon *pWeapon =
			dynamic_cast< CBaseHL2MPCombatWeapon * >(
				pPlayer->Weapon_OwnsThisType( pszClassname ) );
		if ( !pWeapon ||
			( bShootout && FoFIsShootoutDynamite( pszClassname ) ) )
		{
			continue;
		}

		const CHL2MPSWeaponInfo &weaponInfo = pWeapon->GetHL2MPWpnData();
		if ( pWeapon->GetFoFShotCounter() >= weaponInfo.m_iBreakLimit )
			continue;

		pWeapon->AddFoFShotCounter( weaponInfo.m_iBreakDropPenalty );
		pPlayer->Weapon_Drop( pWeapon, NULL, &vecVelocity );
	}
}

static bool FoFIsDeathDropCandidate( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	for ( int i = 0; i < ARRAYSIZE( s_FoFDeathDropWeapons ); ++i )
	{
		if ( FClassnameIs( pWeapon, s_FoFDeathDropWeapons[i] ) )
			return true;
	}
	return false;
}

static void FoFDropRemainingActiveDeathWeapon( CFoF_Player *pPlayer )
{
	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !FoFIsDeathDropCandidate( pWeapon ) )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( ( !currentMode.IsValid() || currentMode.GetInt() == 1 ) &&
		FoFIsShootoutDynamite( pWeapon->GetClassname() ) )
	{
		return;
	}

	CFoFBaseRevolver *pRevolver =
		dynamic_cast< CFoFBaseRevolver * >( pWeapon );
	if ( pRevolver )
		pRevolver->ClearFoFThrower();

	const Vector vecVelocity = pPlayer->BodyDirection2D() * 20.0f;
	pPlayer->Weapon_Drop( pWeapon, NULL, &vecVelocity );
}

static void FoFRemoveOwnedProjectiles(
	CFoF_Player *pPlayer, const char *pszClassname )
{
	CBaseEntity *pProjectile = NULL;
	while ( ( pProjectile = gEntList.FindEntityByClassname(
		pProjectile, pszClassname ) ) != NULL )
	{
		if ( !pProjectile->IsMarkedForDeletion() &&
			pProjectile->GetOwnerEntity() == pPlayer )
		{
			UTIL_Remove( pProjectile );
		}
	}
}

static void FoFRemoveOwnedDeathProjectiles( CFoF_Player *pPlayer )
{
	// The original deliberately visits only these two classes.  Black bow
	// arrows are not part of this death cleanup loop.
	FoFRemoveOwnedProjectiles( pPlayer, "arrow" );
	FoFRemoveOwnedProjectiles( pPlayer, "x_arrow" );
}

static int FoFConnectedPlayerCount()
{
	int nPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( UTIL_PlayerByIndex( i ) )
			++nPlayers;
	}
	return nPlayers;
}

static void FoFSpawnSpecialDeathCrate( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !TheNavMesh )
		return;

	CNavArea *pArea = TheNavMesh->GetNearestNavArea(
		pPlayer->GetAbsOrigin(), false, 10000.0f, false, true, TEAM_ANY );
	if ( !pArea )
		return;

	CBaseEntity *pCrate = CreateEntityByName( "fof_crate_special" );
	if ( !pCrate )
		return;

	pCrate->SetAbsOrigin( pArea->GetCenter() + Vector( 0.0f, 0.0f, 1.0f ) );
	DispatchSpawn( pCrate );
	pCrate->Activate();
	IPhysicsObject *pPhysics = pCrate->VPhysicsGetObject();
	if ( pPhysics )
		pPhysics->Wake();
}

static void FoFMaybeSpawnSpecialDeathCrate(
	CFoF_Player *pPlayer, float flLifeStartTime )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( !pPlayer || !currentMode.IsValid() || currentMode.GetInt() != 1 )
		return;

	const float flKillScore = clamp(
		static_cast< float >( pPlayer->GetFoFMultiKill() ) * 0.1f,
		0.0f, 1.0f ) * 140.0f + 10.0f;
	const float flLifeScore = clamp(
		( gpGlobals->curtime - flLifeStartTime ) * ( 1.0f / 180.0f ),
		0.0f, 1.0f ) * 40.0f + 10.0f;
	const int nScore = static_cast< int >( flKillScore ) +
		static_cast< int >( flLifeScore );
	const float flThreshold = clamp(
		static_cast< float >( FoFConnectedPlayerCount() - 15 ) * 0.1f,
		0.0f, 1.0f ) * 50.0f + 150.0f;
	if ( static_cast< float >( nScore ) >= flThreshold )
		FoFSpawnSpecialDeathCrate( pPlayer );
}

static void FoFReleaseControlledTank( CFoF_Player *pPlayer )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = gEntList.FindEntityByClassname(
		pEntity, "func_tank_fof" ) ) != NULL )
	{
		CFuncTank *pTank = dynamic_cast< CFuncTank * >( pEntity );
		if ( pTank && pTank->GetController() == pPlayer )
		{
			pPlayer->ClearUseEntity();
			return;
		}
	}
}

static CBaseCombatWeapon *FoFFindOwnedDynamite(
	CFoF_Player *pPlayer, const char *pszClassname )
{
	return pPlayer->Weapon_OwnsThisType( pszClassname );
}

static void FoFHandleDeathDynamite(
	CFoF_Player *pPlayer, const CTakeDamageInfo &info )
{
	CBaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
	if ( !pActiveWeapon )
		return;

	CBaseEntity *pAttacker = info.GetAttacker();
	if ( ( info.GetDamageType() & DMG_BLAST ) &&
		pAttacker && pAttacker != pPlayer )
	{
		// FoF checks black dynamite first. Spawning it clears both
		// dynamite ammo pools, so at most one carried stick cooks off.
		CBaseCombatWeapon *pDynamite = FoFFindOwnedDynamite(
			pPlayer, "weapon_dynamite_black" );
		if ( pDynamite && pDynamite->HasPrimaryAmmo() )
		{
			FoFSpawnDeathDynamite(
				pDynamite, pPlayer, pAttacker, true );
		}

		pDynamite = FoFFindOwnedDynamite(
			pPlayer, "weapon_dynamite" );
		if ( pDynamite && pDynamite->HasPrimaryAmmo() )
		{
			FoFSpawnDeathDynamite(
				pDynamite, pPlayer, pAttacker, true );
		}
		return;
	}

	if ( FoFIsPrimedDeathDynamite( pActiveWeapon ) )
	{
		FoFSpawnDeathDynamite(
			pActiveWeapon, pPlayer, pPlayer, false );
	}
}

static void FoFFireUnforgivenEvent(
	CFoF_Player *pScorer, CFoF_Player *pVictim )
{
	if ( !pScorer || !pVictim || pScorer == pVictim ||
		pVictim->GetFoFMultiKill() < 7 || !gameeventmanager )
	{
		return;
	}

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "unforgiven" );
	if ( !pEvent )
		return;

	pEvent->SetInt( "entindex_unforgiven", pScorer->entindex() );
	gameeventmanager->FireEvent( pEvent );
}

void CFoF_Player::SendFoFNotorietyNotice(
	int nReason, int nAmount, int nEventCode, const char *pszText )
{
	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "Notoriety" );
		WRITE_BYTE( nReason );
		WRITE_LONG( nAmount );
		WRITE_BYTE( nEventCode );
		WRITE_STRING( pszText ? pszText : "" );
	MessageEnd();
}

void CFoF_Player::AccumulateFoFNotoriety(
	int nAmount, bool bCombat, bool bExcludeFromCombatTotal )
{
	if ( nAmount == 0 )
		return;

	m_nLastRoundNotoriety += nAmount;
	m_nFoFTotalNotoriety += nAmount;
	if ( bCombat && !bExcludeFromCombatTotal )
		m_nFoFCombatNotoriety += nAmount;

	CTeam *pTeam = GetTeam();
	UTIL_LogPrintf(
		"\"%s<%i><%s><%s>\" triggered \"%s\" (notoriety \"%i\")\n",
		GetPlayerName(), GetUserID(), GetNetworkIDString(),
		pTeam ? pTeam->GetName() : "",
		bCombat ? "combat" : "capture", nAmount );
}

void CFoF_Player::ResetFoFDeathScoringState( void )
{
	hPlayerAssisted = NULL;
	m_nFoFAssistDamage = 0;
	m_nMultiKill = 0;
	m_nFoFCurrentDrunkard = 0;
	m_flFoFDamageAccumulated = 0.0f;
	m_flFoFMultiKillAnnounceTime = 0.0f;
}

void CFoF_Player::AwardFoFCash(
	float flCash, const char *pszReason )
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	bool bReachedTeamplayLimit = false;
	if ( currentMode.IsValid() && currentMode.GetInt() == 2 )
	{
		if ( teamClasses.IsValid() && teamClasses.GetBool() )
			return;
		if ( m_flFoFCash + flCash > 175.0f )
		{
			m_flFoFCash = clamp( m_flFoFCash.Get(), 0.0f, 175.0f );
			flCash = 175.0f - m_flFoFCash;
			bReachedTeamplayLimit = flCash <= 0.0f;
		}
	}
	if ( flCash <= 0.0f )
	{
		if ( bReachedTeamplayLimit )
			ClientPrint( this, HUD_PRINTTALK, "#Cash_LimitReached" );
		return;
	}

	// CFoF_Player::AddCash in FoF presents the cash award before it
	// updates the network balance. Event_Killed then sends the separate Cash
	// delta message using a truncating float-to-int conversion.
	EmitSound( "FoF.LootIn" );
	const int nCash = static_cast< int >( flCash );
	SendFoFNotorietyNotice( 9, nCash, 0, pszReason );
	m_flFoFCash += flCash;

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "Cash" );
		WRITE_LONG( nCash );
	MessageEnd();
}

void CFoF_Player::FineFoFCash(
	int nCash, const char *pszReason )
{
	if ( pszReason && pszReason[0] )
		SendFoFNotorietyNotice( 10, nCash, 0, pszReason );
	m_flFoFCash -= static_cast< float >( nCash );
}

void CFoF_Player::UpdateFoFDeathScoringState( void )
{
	if ( m_flFoFMultiKillAnnounceTime <= 0.0f ||
		gpGlobals->curtime <= m_flFoFMultiKillAnnounceTime )
	{
		return;
	}

	m_flFoFMultiKillAnnounceTime = 0.0f;
	if ( m_nMultiKill < 4 )
		EmitSound( "FoF.MultiKill1" );
	else if ( m_nMultiKill < 7 )
		EmitSound( "FoF.MultiKill2" );
	else if ( m_nMultiKill < 10 )
		EmitSound( "FoF.MultiKill3" );
	else
		EmitSound( "FoF.MultiKill_Special" );
}

int CFoF_Player::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( m_flFoFInvulnerableUntil > gpGlobals->curtime )
		return 0;
	CBaseGrenade *pGrenade = dynamic_cast< CBaseGrenade * >( info.GetInflictor() );
	if ( info.GetDamage() > 10.0f && pGrenade &&
		FClassnameIs( pGrenade, "dynamite_yellow" ) )
	{
		CFoF_Player *pThrower = ToFoFPlayer( pGrenade->GetThrower() );
		if ( pThrower )
			pThrower->RecordFoFDynamiteBeltHit();
	}

	if ( m_bPickupActive && info.GetDamage() > 45.0f )
		DropFoFCarriedObject( true, false );

	FoFBreakBadPlayerDamaged( this, info );
	if ( gpGlobals->curtime > GetFoFPainFinishedTime() )
		FoFSendGroggyDamagePresentation( this, info );

	// ItemPostFrame applies FoF's kick impulse before it dispatches custom
	// damage 11. CBasePlayer::OnTakeDamage_Alive adds the SDK's ordinary
	// damage knockback as well; retaining that second force makes a kicked
	// player travel much farther on a flatter arc than the shipped game.
	// Preserve the already-applied 175/150 (or boots 210/180) velocity across
	// the base damage path while leaving every other damage source unchanged.
	const bool bKickDamage = info.GetDamageCustom() == 11;
	const Vector vecKickVelocity = bKickDamage ?
		GetAbsVelocity() : vec3_origin;
	const int nResult = BaseClass::OnTakeDamage_Alive( info );
	if ( bKickDamage )
		SetAbsVelocity( vecKickVelocity );
	if ( !nResult )
		return nResult;

	CFoF_Player *pAttacker = ToFoFPlayer( info.GetAttacker() );
	if ( pAttacker )
		m_flFoFDamageAccumulated += info.GetDamage();

	if ( !pAttacker || pAttacker == this )
		return nResult;

	FoFSendHitRecon( this, pAttacker, info );
	if ( !IsAlive() )
		return nResult;

	const int nDamage = static_cast< int >( info.GetDamage() );
	if ( nDamage <= 0 )
		return nResult;

	CFoF_Player *pAssister = ToFoFPlayer( hPlayerAssisted.Get() );
	if ( !pAssister )
	{
		hPlayerAssisted = pAttacker;
		m_nFoFAssistDamage = 0;
	}
	else if ( pAssister != pAttacker &&
		nDamage > m_nFoFAssistDamage )
	{
		hPlayerAssisted = pAttacker;
		m_nFoFAssistDamage = 0;
	}

	if ( hPlayerAssisted.Get() == pAttacker )
		m_nFoFAssistDamage += nDamage;

	return nResult;
}

void CFoF_Player::Event_Killed( const CTakeDamageInfo &info )
{
	// The shipped player commits the completed lifetime and the two per-life
	// maxima before any death state is reset on the following spawn.
	CommitFoFMapStatistics( false );

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 2 )
		AwardFoFCaptureNotoriety();

	CFoF_Player *pScorer = NULL;
	if ( HL2MPRules() )
	{
		pScorer = ToFoFPlayer( HL2MPRules()->GetDeathScorer(
			info.GetAttacker(), info.GetInflictor(), this ) );
	}
	FoFStartFreezeCam( this, pScorer, info );
	FoFSetPlayerRespawnThreat(
		this, pScorer ? static_cast< CBaseEntity * >( pScorer ) :
		info.GetAttacker() );
	FoFSendHeadshotPresentation( this, info );
	SendFoFClearViewModelParticles();
	FoFSendDeathHintTags( this, info );

	const bool bVictimIsBot = IsFakeClient();
	CFoF_Player *pAssister = ToFoFPlayer( hPlayerAssisted.Get() );
	const bool bEnemyScorer = FoFIsEnemyPlayer( pScorer, this );
	FoFFireUnforgivenEvent( pScorer, this );
	if ( bEnemyScorer )
	{
		static ConVarRef disableKillstreak(
			"fof_sv_disable_killstreak", true );
		if ( !disableKillstreak.IsValid() ||
			!disableKillstreak.GetBool() )
		{
			if ( pScorer->m_nMultiKill > 0 )
			{
				pScorer->m_flFoFMultiKillAnnounceTime =
					gpGlobals->curtime + 0.7f;
			}
			++pScorer->m_nMultiKill;
		}

		int nAssistAward = 0;
		if ( m_nFoFAssistDamage > 40 && pAssister &&
			pAssister != pScorer && FoFIsEnemyPlayer( pAssister, this ) )
		{
			nAssistAward = FoFAssistNotoriety( m_nFoFAssistDamage );
			pAssister->AccumulateFoFNotoriety(
				nAssistAward, true, bVictimIsBot );
			pAssister->SendFoFNotorietyNotice(
				2, nAssistAward, 0, NULL );
		}

		const int nBaseAward = clamp(
			FoFKillNotoriety( info, pScorer ) - nAssistAward, 4, 30 );
		const float flMultiplier = RemapValClamped(
			static_cast< float >( pScorer->m_nMultiKill ),
			1.0f, 10.0f, 1.0f, 2.0f );
		const int nTotalAward = RoundFloatToInt(
			static_cast< float >( nBaseAward ) * flMultiplier );
		const int nKillstreakBonus = nTotalAward - nBaseAward;
		pScorer->AccumulateFoFNotoriety(
			nTotalAward, true, bVictimIsBot );
		pScorer->SendFoFNotorietyNotice(
			nKillstreakBonus > 0 ? 1 : 0,
			nBaseAward, MAX( 0, nKillstreakBonus ), NULL );

		if ( FoFCanAwardDeathCash( this, pScorer ) )
		{
			pScorer->AwardFoFCash(
				FoFDeathCashReward( this, nTotalAward ),
				"#Cash_Added_Kill" );
			if ( nAssistAward > 0 && pAssister && pAssister != pScorer )
			{
				pAssister->AwardFoFCash(
					FoFDeathCashReward( this, nAssistAward ),
					"#Cash_Added" );
			}
		}

		static ConVarRef nemesis( "fof_sv_nemesis", true );
		if ( currentMode.IsValid() && currentMode.GetInt() == 1 &&
			( !nemesis.IsValid() || nemesis.GetBool() ) )
		{
			const int nVictimIndex = entindex();
			const int nScorerIndex = pScorer->entindex();
			if ( nScorerIndex > 0 &&
				nScorerIndex < ARRAYSIZE( m_iFoFNemesisKills ) )
			{
				const int nKills =
					++m_iFoFNemesisKills[nScorerIndex];
				if ( nKills == 5 )
				{
					FoFSendNemesisNotice(
						this, "#Nemesis_new", pScorer->GetPlayerName() );
					FoFSendNemesisNotice(
						pScorer, "#Nemesis_new_killer", GetPlayerName() );
					FoFPresentNewNemesis( this );
				}
			}

			if ( nVictimIndex > 0 &&
				nVictimIndex < ARRAYSIZE( pScorer->m_iFoFNemesisKills ) &&
				pScorer->m_iFoFNemesisKills[nVictimIndex] >= 5 )
			{
				const int nNemesisBonus =
					2 * pScorer->m_iFoFNemesisKills[nVictimIndex];
				pScorer->AccumulateFoFNotoriety(
					nNemesisBonus, true, bVictimIsBot );
				pScorer->SendFoFNotorietyNotice(
					3, nNemesisBonus, 0, NULL );

				char szBonus[16];
				Q_snprintf( szBonus, sizeof( szBonus ),
					"%i", nNemesisBonus );
				FoFSendNemesisNotice(
					pScorer, "#Nemesis_kill", GetPlayerName(), szBonus );
				FoFPresentNemesisRevenge( this );
				pScorer->m_iFoFNemesisKills[nVictimIndex] = 0;
			}
		}
	}
	else if ( m_nFoFAssistDamage > 40 &&
		pAssister && pAssister != this )
	{
		// FoF retains assist credit when the final damage came from the
		// world or another non-player entity. This alternate path intentionally
		// grants notoriety only; the Shootout cash award belongs to a normal
		// player-scored kill above.
		const int nAssistAward = FoFAssistNotoriety( m_nFoFAssistDamage );
		pAssister->AccumulateFoFNotoriety(
			nAssistAward, true, bVictimIsBot );
		pAssister->SendFoFNotorietyNotice(
			2, nAssistAward, 0, NULL );
	}

	if ( pScorer == this )
	{
		static ConVarRef competitive( "fof_sv_dm_comp", true );
		int nPenalty = ( info.GetDamageType() & DMG_BLAST ) ? -20 : -10;
		if ( pScorer == this && competitive.IsValid() &&
			competitive.GetBool() )
		{
			nPenalty = -50;
		}
		AccumulateFoFNotoriety( nPenalty, true, false );
		SendFoFNotorietyNotice( 7, nPenalty, 0, NULL );
	}

	const int nCurrentMode = currentMode.IsValid() ?
		currentMode.GetInt() : 1;
	static ConVarRef warmup( "fof_warmup", true );
	bool bActiveRound = !g_fGameOver &&
		( !warmup.IsValid() || !warmup.GetBool() );
	if ( nCurrentMode == 2 && HL2MPRules() )
		bActiveRound = bActiveRound && HL2MPRules()->IsFoFTeamplayRoundActive();
	if ( HasFoFProfileData() && nCurrentMode != 5 && bActiveRound )
	{
		const char *pszCashReason = NULL;
		if ( nCurrentMode == 3 )
			pszCashReason = "#Cash_Fine_Unarmed";
		else if ( nCurrentMode == 1 )
			pszCashReason = "#Cash_Removed_SelfHarm";
		FineFoFCash( 2 * ( DeathCount() + 1 ), pszCashReason );
	}

	FoFReleaseControlledTank( this );
	FoFHandleDeathDynamite( this, info );
	DropFoFCarriedObject( false, false );
	DismountFoFHorse( true );
	FoFDisarmDeathWeaponThrows( this );
	FoFDropDeathInventory( this );
	FoFDropRemainingActiveDeathWeapon( this );
	HideViewModels();
	KnockOffFoFHat( info, true );
	FoFRemoveOwnedDeathProjectiles( this );
	FoFMaybeSpawnSpecialDeathCrate( this, m_flFoFLifeStartTime );
	BaseClass::Event_Killed( info );
	// CHL2MP_Player finalizes LIFE_DEAD immediately, but the shipped FoF
	// override enters PlayerDeathThink as LIFE_DYING and lets that function
	// own its 60-think death presentation before making respawn possible.
	m_lifeState = LIFE_DYING;
	hPlayerAssisted = NULL;
	m_nFoFAssistDamage = 0;
	FoFBreakBadPlayerKilled( this, info );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF-only server trace filters used by player interaction and ballistics.
//
//=============================================================================//

CTraceFilterNoOwnerTestDR::CTraceFilterNoOwnerTestDR(
	const IHandleEntity *pPassEntity, int collisionGroup ) :
	BaseClass( NULL, collisionGroup ),
	m_pPassNotOwner( pPassEntity )
{
}

bool CTraceFilterNoOwnerTestDR::ShouldHitEntity(
	IHandleEntity *pHandleEntity, int contentsMask )
{
	// Unlike CTraceFilterSimple, the shipped filter rejects only the passed
	// entity itself.  Entities owned by the player remain valid interaction
	// targets, which is required for live dynamite pickup/defuse traces.
	if ( pHandleEntity == m_pPassNotOwner )
		return false;

	return BaseClass::ShouldHitEntity(
		pHandleEntity, contentsMask );
}

CTraceFilterSkipTwoEntitiesFoF::CTraceFilterSkipTwoEntitiesFoF(
	const IHandleEntity *pPassEntity,
	const IHandleEntity *pPassEntity2,
	int collisionGroup ) :
	BaseClass( pPassEntity, collisionGroup ),
	m_pPassEnt2( pPassEntity2 )
{
}

bool CTraceFilterSkipTwoEntitiesFoF::ShouldHitEntity(
	IHandleEntity *pHandleEntity, int contentsMask )
{
	Assert( pHandleEntity );
	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt2 ) )
		return false;

	// FoF deliberately lets entities carrying this flag bypass the ordinary
	// collision rules.  This is shipped penetration behavior despite the
	// Source flag's rotorwash-oriented name.
	if ( !staticpropmgr->IsStaticProp( pHandleEntity ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity &&
			pEntity->IsEFlagSet( EFL_NO_ROTORWASH_PUSH ) )
		{
			return true;
		}
	}

	return BaseClass::ShouldHitEntity(
		pHandleEntity, contentsMask );
}

void CTraceFilterSkipTwoEntitiesFoF::SetPassEntity2(
	const IHandleEntity *pPassEntity2 )
{
	m_pPassEnt2 = pPassEntity2;
}
