#include "cbase.h"
#include "fof/fof_player_activities.h"
#include "activitylist.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/fof_weapon_properties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static_assert( ACT_VM_DRYFIRE2 == 0x0BA,
	"FoF activity ABI mismatch at ACT_VM_DRYFIRE2" );
static_assert( ACT_RANGE_ATTACK_PUMPSHOTGUN == 0x120,
	"FoF activity ABI mismatch at ACT_RANGE_ATTACK_PUMPSHOTGUN" );
static_assert( ACT_HL2MP_GESTURE_RANGE_ATTACK2 == 0x3E9,
	"FoF activity ABI mismatch at ACT_HL2MP_GESTURE_RANGE_ATTACK2" );
static_assert( ACT_HL2MP_IDLE_PISTOL_RH_AIM == 0x3EE,
	"FoF activity ABI mismatch at ACT_HL2MP_IDLE_PISTOL_RH_AIM" );
static_assert( ACT_MP_STAND_IDLE == 0x469,
	"FoF activity ABI mismatch at ACT_MP_STAND_IDLE" );
static_assert( ACT_VM_UNUSABLE == 0x61E,
	"FoF activity ABI mismatch at ACT_VM_UNUSABLE" );
static_assert( ACT_MP_RUN_RAGGED == 0x70E,
	"FoF activity ABI mismatch at ACT_MP_RUN_RAGGED" );
static_assert( ACT_ONHORSE_SHOTGUN == 0x726,
	"FoF activity ABI mismatch at ACT_ONHORSE_SHOTGUN" );

bool FoFTranslateNetworkActivity(
	int nOriginalActivity, Activity &localActivity )
{
	if ( nOriginalActivity == ACT_INVALID )
	{
		localActivity = ACT_INVALID;
		return true;
	}

	if ( nOriginalActivity < ACT_RESET ||
		nOriginalActivity > ACT_ONHORSE_SHOTGUN )
	{
		localActivity = ACT_INVALID;
		return false;
	}

	localActivity = static_cast< Activity >( nOriginalActivity );
	return true;
}

CBaseCombatWeapon *FoFSelectGestureWeapon(
	CBasePlayer *pPlayer )
{
	// The original first queries the player's virtual active-weapon accessor,
	// which falls back to hand two when hand one is empty.
	CBaseCombatWeapon *pFirst = pPlayer->GetActiveWeapon();
	if ( !pFirst )
		return NULL;

	// FoF's TranslateActivity override uses the second active weapon while that
	// hand is in one of its three reload activities; otherwise it uses hand one.
	if ( pPlayer->HasDualActiveWeapons() )
	{
		CBaseCombatWeapon *pSecond = pPlayer->GetActiveWeapon2();
		if ( pSecond )
		{
			const Activity activity = pSecond->GetActivity();
			if ( activity == ACT_VM_RELOAD ||
				activity == FoFShotgunReloadStartActivity( pSecond ) ||
				activity == FoFShotgunReloadFinishActivity( pSecond ) )
			{
				return pSecond;
			}
		}
	}

	return pFirst;
}

struct FoFOriginalActTableEntry_t
{
	Activity m_InputActivity;
	int m_nOriginalOutputActivity;
};

static Activity FoFTranslateOriginalActTableOutput(
	int nOriginalOutputActivity, Activity fallback )
{
	Activity translated = ACT_INVALID;
	return FoFTranslateNetworkActivity(
		nOriginalOutputActivity, translated ) ? translated : fallback;
}

static Activity FoFApplyOriginalActTable(
	Activity activity,
	const FoFOriginalActTableEntry_t *pEntries,
	int nEntryCount,
	int nRaggedOutput,
	int nHorseOutput )
{
	for ( int i = 0; i < nEntryCount; ++i )
	{
		if ( pEntries[i].m_InputActivity == activity )
		{
			return FoFTranslateOriginalActTableOutput(
				pEntries[i].m_nOriginalOutputActivity, activity );
		}
	}

	const char *pszActivityName = ActivityList_NameForIndex( activity );
	if ( pszActivityName && nRaggedOutput != ACT_INVALID &&
		!Q_strcmp( pszActivityName, "ACT_MP_RUN_RAGGED" ) )
	{
		return FoFTranslateOriginalActTableOutput(
			nRaggedOutput, activity );
	}
	if ( pszActivityName && nHorseOutput != ACT_INVALID &&
		!Q_strcmp( pszActivityName, "ACT_ONHORSE_IDLE" ) )
	{
		return FoFTranslateOriginalActTableOutput(
			nHorseOutput, activity );
	}

	return activity;
}

Activity FoFNoWeaponActivityOverride( Activity activity )
{
	switch ( activity )
	{
	case ACT_MP_STAND_IDLE:
		return ACT_IDLE;
	case ACT_MP_RUN:
		return ACT_RUN;
	case ACT_MP_WALK:
		return ACT_WALK;
	case ACT_MP_CROUCH_IDLE:
		return ACT_CROUCH;
	case ACT_MP_CROUCHWALK:
		return ACT_WALK_CROUCH;
	case ACT_MP_JUMP:
		return ACT_JUMP;
	default:
		return activity;
	}
}

static Activity FoFPistolActivityOverride(
	CBasePlayer *pPlayer,
	CBaseCombatWeapon *pWeapon,
	Activity activity )
{
	static const FoFOriginalActTableEntry_t s_DualPistol[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x458 },
		{ ACT_MP_RUN,                         0x459 },
		{ ACT_MP_CROUCH_IDLE,                 0x45A },
		{ ACT_MP_CROUCHWALK,                  0x45B },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x45C },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x45C },
		{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,  0x45D },
		{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE, 0x45D },
		{ ACT_MP_RELOAD_STAND,                0x3FF },
		{ ACT_MP_RELOAD_CROUCH,               0x3FF },
		{ ACT_MP_RELOAD_STAND_END,            0x3FF },
		{ ACT_MP_RELOAD_CROUCH_END,           0x3FF },
		{ ACT_MP_JUMP,                        0x460 },
	};
	if ( pPlayer && pPlayer->HasDualActiveWeapons() )
	{
		return FoFApplyOriginalActTable(
			activity, s_DualPistol, ARRAYSIZE( s_DualPistol ),
			0x70F, 0x721 );
	}

	const char *pszClassname = pWeapon ? pWeapon->GetClassname() : NULL;
	if ( !pszClassname )
		return activity;

	int nReloadRight = 0x3F4;
	int nReloadLeft = 0x3FE;
	if ( !Q_stricmp( pszClassname, "weapon_deringer" ) ||
		!Q_stricmp( pszClassname, "weapon_hammerless" ) ||
		!Q_stricmp( pszClassname, "weapon_maresleg" ) ||
		!Q_stricmp( pszClassname, "weapon_remington_army" ) ||
		!Q_stricmp( pszClassname, "weapon_schofield" ) ||
		!Q_stricmp( pszClassname, "weapon_volcanic" ) ||
		!Q_stricmp( pszClassname, "weapon_walker" ) )
	{
		nReloadRight = 0x3F5;
		nReloadLeft = 0x3FF;
	}
	else if ( !Q_stricmp( pszClassname, "weapon_ghostgun" ) ||
		!Q_stricmp( pszClassname, "weapon_sawedoff_shotgun" ) )
	{
		nReloadRight = 0x3F6;
		nReloadLeft = 0x400;
	}

	const bool bSecondWeapon =
		pWeapon->IsSecondGun() ||
		( pPlayer && pPlayer->GetActiveWeapon2() == pWeapon );
	int nOriginalOutput = ACT_INVALID;
	if ( bSecondWeapon )
	{
		switch ( activity )
		{
		case ACT_MP_STAND_IDLE:
			nOriginalOutput = 0x3F8;
			break;
		case ACT_MP_RUN:
			nOriginalOutput = 0x3F9;
			break;
		case ACT_MP_CROUCH_IDLE:
			nOriginalOutput = 0x3FA;
			break;
		case ACT_MP_CROUCHWALK:
			nOriginalOutput = 0x3FB;
			break;
		case ACT_MP_ATTACK_STAND_PRIMARYFIRE:
		case ACT_MP_ATTACK_CROUCH_PRIMARYFIRE:
			nOriginalOutput =
				!Q_stricmp( pszClassname, "weapon_maresleg" ) ?
				0x461 : 0x3FC;
			break;
		case ACT_MP_ATTACK_STAND_SECONDARYFIRE:
		case ACT_MP_ATTACK_CROUCH_SECONDARYFIRE:
			nOriginalOutput = 0x3FD;
			break;
		case ACT_MP_RELOAD_STAND:
		case ACT_MP_RELOAD_CROUCH:
			nOriginalOutput = nReloadLeft;
			break;
		case ACT_MP_JUMP:
			nOriginalOutput = 0x401;
			break;
		case ACT_RANGE_ATTACK1:
			nOriginalOutput = 0x123;
			break;
		default:
			break;
		}
		if ( nOriginalOutput != ACT_INVALID )
		{
			return FoFTranslateOriginalActTableOutput(
				nOriginalOutput, activity );
		}
		return FoFApplyOriginalActTable(
			activity, NULL, 0, 0x715, 0x720 );
	}

	const bool bAimRightHand =
		pWeapon->FoFWeaponID() == 2 &&
		pPlayer &&
		FoFHandStance( pPlayer ) == 1 &&
		FoFSightExpFactor( pPlayer ) >= 0.01f;
	switch ( activity )
	{
	case ACT_MP_STAND_IDLE:
		nOriginalOutput = bAimRightHand ? 0x3EE : 0x3EC;
		break;
	case ACT_MP_RUN:
		nOriginalOutput = bAimRightHand ? 0x3EF : 0x3ED;
		break;
	case ACT_MP_CROUCH_IDLE:
		nOriginalOutput = 0x3F0;
		break;
	case ACT_MP_CROUCHWALK:
		nOriginalOutput = 0x3F1;
		break;
	case ACT_MP_ATTACK_STAND_PRIMARYFIRE:
	case ACT_MP_ATTACK_CROUCH_PRIMARYFIRE:
		nOriginalOutput =
			!Q_stricmp( pszClassname, "weapon_maresleg" ) ?
			0x465 : 0x3F2;
		break;
	case ACT_MP_ATTACK_STAND_SECONDARYFIRE:
	case ACT_MP_ATTACK_CROUCH_SECONDARYFIRE:
		nOriginalOutput = 0x3F3;
		break;
	case ACT_MP_RELOAD_STAND:
	case ACT_MP_RELOAD_CROUCH:
		nOriginalOutput = nReloadRight;
		break;
	case ACT_MP_JUMP:
		nOriginalOutput = 0x3F7;
		break;
	case ACT_RANGE_ATTACK1:
		nOriginalOutput = 0x123;
		break;
	case ACT_GESTURE_RANGE_ATTACK1:
		nOriginalOutput = 0x13D;
		break;
	default:
		break;
	}
	if ( nOriginalOutput != ACT_INVALID )
	{
		return FoFTranslateOriginalActTableOutput(
			nOriginalOutput, activity );
	}
	return FoFApplyOriginalActTable(
		activity, NULL, 0, 0x714, 0x71F );
}

Activity FoFOriginalWeaponActivityOverride(
	CBasePlayer *pPlayer,
	CBaseCombatWeapon *pWeapon,
	Activity activity )
{
	if ( !pWeapon )
		return FoFNoWeaponActivityOverride( activity );

	// These weapon acttable rows accept this SDK's activity values; output
	// values remain protocol indices and are translated
	// by name before being returned.
	static const FoFOriginalActTableEntry_t s_Carbine[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x41E },
		{ ACT_IDLE_RIFLE,                     0x34F },
		{ ACT_MP_RUN,                         0x41F },
		{ ACT_RUN_RIFLE,                      0x350 },
		{ ACT_MP_CROUCH_IDLE,                 0x420 },
		{ ACT_MP_CROUCHWALK,                  0x421 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x422 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x422 },
		{ ACT_MP_RELOAD_STAND,                0x423 },
		{ ACT_MP_RELOAD_CROUCH,               0x423 },
		{ ACT_MP_JUMP,                        0x424 },
		{ ACT_RANGE_ATTACK1,                  0x12C },
	};
	static const FoFOriginalActTableEntry_t s_Coachgun[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x410 },
		{ ACT_MP_RUN,                         0x411 },
		{ ACT_MP_CROUCH_IDLE,                 0x412 },
		{ ACT_MP_CROUCHWALK,                  0x413 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x414 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x414 },
		{ ACT_MP_RELOAD_STAND,                0x415 },
		{ ACT_MP_RELOAD_CROUCH,               0x415 },
		{ ACT_MP_JUMP,                        0x416 },
		{ ACT_RANGE_ATTACK1,                  0x11F },
		{ ACT_GESTURE_RANGE_ATTACK1,          0x13C },
	};
	static const FoFOriginalActTableEntry_t s_Sharps[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x402 },
		{ ACT_IDLE_RIFLE,                     0x353 },
		{ ACT_MP_RUN,                         0x403 },
		{ ACT_RUN_RIFLE,                      0x354 },
		{ ACT_MP_CROUCH_IDLE,                 0x404 },
		{ ACT_MP_CROUCHWALK,                  0x405 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x406 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x406 },
		{ ACT_MP_RELOAD_STAND,                0x440 },
		{ ACT_MP_RELOAD_CROUCH,               0x440 },
		{ ACT_MP_JUMP,                        0x408 },
		{ ACT_RANGE_ATTACK1,                  0x11C },
	};
	static const FoFOriginalActTableEntry_t s_PumpShotgun[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x417 },
		{ ACT_MP_RUN,                         0x418 },
		{ ACT_MP_CROUCH_IDLE,                 0x419 },
		{ ACT_MP_CROUCHWALK,                  0x41A },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x41B },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x41B },
		{ ACT_MP_RELOAD_STAND,                0x41C },
		{ ACT_MP_RELOAD_CROUCH,               0x41C },
		{ ACT_MP_JUMP,                        0x41D },
		{ ACT_RANGE_ATTACK1,                  0x120 },
	};
	static const FoFOriginalActTableEntry_t s_HenrySpencer[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x409 },
		{ ACT_IDLE_RIFLE,                     0x351 },
		{ ACT_MP_RUN,                         0x40A },
		{ ACT_RUN_RIFLE,                      0x352 },
		{ ACT_MP_CROUCH_IDLE,                 0x40B },
		{ ACT_MP_CROUCHWALK,                  0x40C },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x40D },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x40D },
		{ ACT_MP_RELOAD_STAND,                0x40E },
		{ ACT_MP_RELOAD_CROUCH,               0x40E },
		{ ACT_MP_JUMP,                        0x40F },
		{ ACT_RANGE_ATTACK1,                  0x117 },
		{ ACT_GESTURE_RANGE_ATTACK2,          0x135 },
	};
	static const FoFOriginalActTableEntry_t s_AxeMachete[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x43A },
		{ ACT_MP_RUN,                         0x43B },
		{ ACT_MP_CROUCH_IDLE,                 0x43C },
		{ ACT_MP_CROUCHWALK,                  0x43D },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x43E },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x43E },
		{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,  0x43F },
		{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE, 0x43F },
		{ ACT_MP_JUMP,                        0x441 },
	};
	static const FoFOriginalActTableEntry_t s_Knife[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x442 },
		{ ACT_MP_RUN,                         0x443 },
		{ ACT_MP_CROUCH_IDLE,                 0x444 },
		{ ACT_MP_CROUCHWALK,                  0x445 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x446 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x446 },
		{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,  0x447 },
		{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE, 0x447 },
		{ ACT_MP_JUMP,                        0x449 },
	};
	static const FoFOriginalActTableEntry_t s_Bow[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x433 },
		{ ACT_MP_RUN,                         0x434 },
		{ ACT_MP_CROUCH_IDLE,                 0x435 },
		{ ACT_MP_CROUCHWALK,                  0x436 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x437 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x437 },
		{ ACT_MP_RELOAD_STAND,                0x438 },
		{ ACT_MP_RELOAD_CROUCH,               0x438 },
		{ ACT_MP_JUMP,                        0x439 },
	};
	static const FoFOriginalActTableEntry_t s_Dynamite[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x425 },
		{ ACT_MP_RUN,                         0x426 },
		{ ACT_MP_CROUCH_IDLE,                 0x427 },
		{ ACT_MP_CROUCHWALK,                  0x428 },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x429 },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x429 },
		{ ACT_MP_RELOAD_STAND,                0x42A },
		{ ACT_MP_RELOAD_CROUCH,               0x42A },
		{ ACT_MP_JUMP,                        0x42B },
	};
	static const FoFOriginalActTableEntry_t s_Fists[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x44A },
		{ ACT_MP_RUN,                         0x44B },
		{ ACT_MP_CROUCH_IDLE,                 0x44C },
		{ ACT_MP_CROUCHWALK,                  0x44D },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x44E },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x44E },
		{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,  0x44F },
		{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE, 0x44F },
		{ ACT_MP_JUMP,                        0x450 },
	};
	static const FoFOriginalActTableEntry_t s_GhostFists[] =
	{
		{ ACT_MP_STAND_IDLE,                  0x469 },
		{ ACT_MP_RUN,                         0x46F },
		{ ACT_MP_CROUCH_IDLE,                 0x46A },
		{ ACT_MP_CROUCHWALK,                  0x46A },
		{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,    0x47D },
		{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,   0x47D },
		{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,  0x47D },
		{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE, 0x47D },
		{ ACT_MP_JUMP,                        0x474 },
	};

	const char *pszClassname = pWeapon->GetClassname();
	if ( !pszClassname )
		return activity;

	if ( !Q_stricmp( pszClassname, "weapon_carbine" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Carbine, ARRAYSIZE( s_Carbine ), 0x710, 0x71B );
	}
	if ( !Q_stricmp( pszClassname, "weapon_coachgun" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Coachgun, ARRAYSIZE( s_Coachgun ), 0x719, 0x726 );
	}
	if ( !Q_stricmp( pszClassname, "weapon_sharps" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Sharps, ARRAYSIZE( s_Sharps ), 0x710, 0x71B );
	}
	if ( !Q_stricmp( pszClassname, "weapon_shotgun" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_PumpShotgun, ARRAYSIZE( s_PumpShotgun ), 0x719, 0x726 );
	}
	if ( !Q_stricmp( pszClassname, "weapon_spencer" ) ||
		!Q_stricmp( pszClassname, "weapon_henryrifle" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_HenrySpencer,
			ARRAYSIZE( s_HenrySpencer ), 0x710, 0x71B );
	}
	if ( !Q_stricmp( pszClassname, "weapon_axe" ) ||
		!Q_stricmp( pszClassname, "weapon_machete" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_AxeMachete,
			ARRAYSIZE( s_AxeMachete ), 0x711, 0x71C );
	}
	if ( !Q_stricmp( pszClassname, "weapon_knife" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Knife, ARRAYSIZE( s_Knife ), 0x712, 0x71D );
	}
	if ( !Q_stricmp( pszClassname, "weapon_bow" ) ||
		!Q_stricmp( pszClassname, "weapon_bow_black" ) ||
		!Q_stricmp( pszClassname, "weapon_xbow" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Bow, ARRAYSIZE( s_Bow ), 0x718, 0x725 );
	}
	if ( !Q_stricmp( pszClassname, "weapon_dynamite" ) ||
		!Q_stricmp( pszClassname, "weapon_dynamite_black" ) ||
		!Q_stricmp( pszClassname, "weapon_dynamite_belt" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Dynamite,
			ARRAYSIZE( s_Dynamite ), 0x716, 0x723 );
	}
	if ( !Q_stricmp( pszClassname, "weapon_fists" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_Fists, ARRAYSIZE( s_Fists ), 0x713, ACT_INVALID );
	}
	if ( !Q_stricmp( pszClassname, "weapon_fists_ghost" ) )
	{
		return FoFApplyOriginalActTable(
			activity, s_GhostFists,
			ARRAYSIZE( s_GhostFists ), 0x713, ACT_INVALID );
	}
	if ( pWeapon->FoFWeaponID() == 2 ||
		!Q_stricmp( pszClassname, "weapon_whiskey" ) )
	{
		return FoFPistolActivityOverride(
			pPlayer, pWeapon, activity );
	}

	return activity;
}
