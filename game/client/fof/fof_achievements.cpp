#include "cbase.h"

#include "achievementmgr.h"
#include "baseachievement.h"
#include "cdll_client_int.h"
#include "fof/c_fof_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// FoF owns its achievement manager on the multiplayer client. GameUI asks
// the engine for this interface when it opens the standard achievements
// dialog, so the manager and registrations must exist before that dialog is
// shown.
CAchievementMgr g_FoFAchievementMgr;

enum FoFAchievementID
{
	ACHIEVEMENT_TRAITOR = 0,
	ACHIEVEMENT_FRAG_ROBBER,
	ACHIEVEMENT_ROBIN_HOOD,
	ACHIEVEMENT_HURT_PRIDE,
	ACHIEVEMENT_DUTCH_COURAGE,
	ACHIEVEMENT_DEFUSER,
	ACHIEVEMENT_CHAIN_REACTION,
	ACHIEVEMENT_UNFORGIVEN,
	ACHIEVEMENT_DEAD_THAN_ALIVE,
	ACHIEVEMENT_DETONATOR,
	ACHIEVEMENT_OVERWEIGHTED,
	ACHIEVEMENT_OVERPOWERED,
	ACHIEVEMENT_NOBODY,
	ACHIEVEMENT_RANCHER,
	ACHIEVEMENT_GUNFIGHTER,
	ACHIEVEMENT_LEGEND,
	ACHIEVEMENT_HATSHOT,
	ACHIEVEMENT_MOBILE_CANNON_OPERATOR2,
	ACHIEVEMENT_SLIDING_KILLER,
	ACHIEVEMENT_BOUNCING_AROUND,
};

#define FOF_KILL_ACHIEVEMENT_FLAGS \
	( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL )

static C_FoF_Player *FoFGetLocalAchievementPlayer()
{
	return dynamic_cast< C_FoF_Player * >(
		C_BasePlayer::GetLocalPlayer() );
}

static bool FoFIsLocalPlayerKill(
	CBaseEntity *pVictim, CBaseEntity *pAttacker )
{
	C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
	return pLocalPlayer && pAttacker == pLocalPlayer &&
		pVictim && pVictim->IsPlayer() && pVictim != pLocalPlayer;
}

static int FoFGetLocalAchievementUserID()
{
	C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
	if ( !pLocalPlayer )
		return -1;

	player_info_t playerInfo;
	if ( !engine->GetPlayerInfo( pLocalPlayer->entindex(), &playerInfo ) )
		return -1;

	return playerInfo.userID;
}

static bool FoFEventUserIsLocal(
	IGameEvent *pEvent, const char *pszFieldName )
{
	const int nLocalUserID = FoFGetLocalAchievementUserID();
	return pEvent && nLocalUserID >= 0 &&
		pEvent->GetInt( pszFieldName ) == nLocalUserID;
}

static bool FoFWeaponClassMatches(
	C_BaseCombatWeapon *pWeapon, const char *pszClassname )
{
	return pWeapon && pWeapon->GetClassname() &&
		!Q_stricmp( pWeapon->GetClassname(), pszClassname );
}

class CFoFAchievementRegistration : public CBaseAchievement
{
protected:
	void InitRegistration( int nFlags, int nGoal )
	{
		SetFlags( nFlags );
		SetGoal( nGoal );
		SetStoreProgressInSteam( true );
	}

	// Registration-only achievements deliberately suppress the stock base
	// behavior, which would otherwise count every qualifying player death.
	virtual void Event_EntityKilled(
		CBaseEntity *pVictim,
		CBaseEntity *pAttacker,
		CBaseEntity *pInflictor,
		IGameEvent *pEvent )
	{
	}
};

class CAchievementTraitor : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 5 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) || !pEvent ||
			!( pEvent->GetInt( "damagebits" ) & DMG_CLUB ) )
		{
			return;
		}

#ifndef NO_STEAM
		if ( !steamapicontext || !steamapicontext->SteamFriends() ||
			!steamapicontext->SteamUtils() )
		{
			return;
		}

		player_info_t victimInfo;
		if ( !engine->GetPlayerInfo( pVictim->entindex(), &victimInfo ) ||
			!victimInfo.friendsID )
		{
			return;
		}

		CSteamID victimSteamID(
			victimInfo.friendsID,
			1,
			steamapicontext->SteamUtils()->GetConnectedUniverse(),
			k_EAccountTypeIndividual );
		if ( steamapicontext->SteamFriends()->HasFriend(
			victimSteamID, k_EFriendFlagImmediate ) )
		{
			IncrementCount();
		}
#endif
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementTraitor,
	ACHIEVEMENT_TRAITOR,
	"ACHIEVEMENT_TRAITOR",
	1 );

class CAchievementFragRobber : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 20 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) )
			return;

		C_FoF_Player *pVictimPlayer =
			dynamic_cast< C_FoF_Player * >( pVictim );
		C_BasePlayer *pAssistingPlayer = pVictimPlayer ?
			dynamic_cast< C_BasePlayer * >(
				pVictimPlayer->GetFoFAssistingPlayer() ) : NULL;
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( !pAssistingPlayer || !pAssistingPlayer->IsAlive() ||
			pAssistingPlayer == pLocalPlayer )
		{
			return;
		}

		if ( ( pAssistingPlayer->GetAbsOrigin() -
			pVictim->GetAbsOrigin() ).Length() < 512.0f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementFragRobber,
	ACHIEVEMENT_FRAG_ROBBER,
	"ACHIEVEMENT_FRAG_ROBBER",
	1 );

class CAchievementRobinHood : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 10 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) || !pEvent ||
			!pEvent->GetBool( "headshot" ) )
		{
			return;
		}

		const char *pszWeapon = pEvent->GetString( "weapon" );
		if ( Q_stricmp( pszWeapon, "arrow" ) &&
			Q_stricmp( pszWeapon, "arrow_black" ) )
		{
			return;
		}

		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( pLocalPlayer &&
			( pLocalPlayer->GetAbsOrigin() -
			pVictim->GetAbsOrigin() ).Length() > 850.0f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementRobinHood,
	ACHIEVEMENT_ROBIN_HOOD,
	"ACHIEVEMENT_ROBIN_HOOD",
	1 );

class CAchievementHurtPride : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 10 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) || !pEvent ||
			!( pEvent->GetInt( "damagebits" ) &
				( DMG_BURN | DMG_FALL | DMG_DROWN ) ) )
		{
			return;
		}

		C_FoF_Player *pVictimPlayer =
			dynamic_cast< C_FoF_Player * >( pVictim );
		if ( pVictimPlayer &&
			pVictimPlayer->GetFoFKicker() == FoFGetLocalAchievementPlayer() )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementHurtPride,
	ACHIEVEMENT_HURT_PRIDE,
	"ACHIEVEMENT_HURT_PRIDE",
	1 );

class CAchievementDutchCourage : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 20 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) &&
			pLocalPlayer && pLocalPlayer->GetFoFDrunkness() > 0.0f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementDutchCourage,
	ACHIEVEMENT_DUTCH_COURAGE,
	"ACHIEVEMENT_DUTCH_COURAGE",
	1 );

#define DECLARE_FOF_USER_EVENT_ACHIEVEMENT( \
	className, achievementID, goal, eventName, userField ) \
	class className : public CFoFAchievementRegistration \
	{ \
		virtual void Init() \
		{ \
			InitRegistration( ACH_SAVE_GLOBAL, goal ); \
		} \
		virtual void ListenForEvents() \
		{ \
			ListenForGameEvent( eventName ); \
		} \
		virtual void FireGameEvent_Internal( IGameEvent *pEvent ) \
		{ \
			if ( pEvent && !Q_stricmp( pEvent->GetName(), eventName ) && \
				FoFEventUserIsLocal( pEvent, userField ) ) \
			{ \
				IncrementCount(); \
			} \
		} \
	}; \
	DECLARE_ACHIEVEMENT( className, achievementID, #achievementID, 1 )

DECLARE_FOF_USER_EVENT_ACHIEVEMENT(
	CAchievementDefuser,
	ACHIEVEMENT_DEFUSER,
	10,
	"defuser",
	"entindex_defuser" );

DECLARE_FOF_USER_EVENT_ACHIEVEMENT(
	CAchievementChainReaction,
	ACHIEVEMENT_CHAIN_REACTION,
	10,
	"chain_reaction",
	"entindex_thrower" );

DECLARE_FOF_USER_EVENT_ACHIEVEMENT(
	CAchievementUnforgiven,
	ACHIEVEMENT_UNFORGIVEN,
	10,
	"unforgiven",
	"entindex_unforgiven" );

class CAchievementMoreDeadThanAlive : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 30 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) &&
			pLocalPlayer && pLocalPlayer->GetHealth() <= 15 )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementMoreDeadThanAlive,
	ACHIEVEMENT_DEAD_THAN_ALIVE,
	"ACHIEVEMENT_DEAD_THAN_ALIVE",
	1 );

class CAchievementDetonator : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 15 );
		m_flLastDetonatorTime = 0.0f;
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "detonator" );
	}

	virtual void FireGameEvent_Internal( IGameEvent *pEvent )
	{
		if ( pEvent && !Q_stricmp( pEvent->GetName(), "detonator" ) &&
			FoFEventUserIsLocal( pEvent, "entindex_detonator" ) )
		{
			m_flLastDetonatorTime = gpGlobals->curtime;
		}
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) || !pEvent ||
			!( pEvent->GetInt( "damagebits" ) & DMG_BLAST ) )
		{
			return;
		}

		const float flElapsed = gpGlobals->curtime - m_flLastDetonatorTime;
		if ( flElapsed > 0.0f && flElapsed < 1.0f )
			IncrementCount();
	}

	float m_flLastDetonatorTime;
};
DECLARE_ACHIEVEMENT(
	CAchievementDetonator,
	ACHIEVEMENT_DETONATOR,
	"ACHIEVEMENT_DETONATOR",
	1 );

class CAchievementOverweighted : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 10 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) &&
			pLocalPlayer && pLocalPlayer->GetFoFLoadFactor() == 0.5f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementOverweighted,
	ACHIEVEMENT_OVERWEIGHTED,
	"ACHIEVEMENT_OVERWEIGHTED",
	1 );

class CAchievementOverpowered : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 15 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) &&
			pLocalPlayer && pLocalPlayer->IsAlive() &&
			FoFWeaponClassMatches(
				pLocalPlayer->GetActiveWeapon1(), "weapon_walker" ) &&
			FoFWeaponClassMatches(
				pLocalPlayer->GetActiveWeapon2(), "weapon_walker2" ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementOverpowered,
	ACHIEVEMENT_OVERPOWERED,
	"ACHIEVEMENT_OVERPOWERED",
	1 );

class CAchievementNobody : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 1 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		if ( !FoFIsLocalPlayerKill( pVictim, pAttacker ) )
			return;

		int nHumanPlayers = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && !( pPlayer->GetFlags() & FL_FAKECLIENT ) )
				++nHumanPlayers;
		}

		if ( nHumanPlayers >= 4 )
			IncrementCount();
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementNobody,
	ACHIEVEMENT_NOBODY,
	"ACHIEVEMENT_NOBODY",
	1 );

// The original field is the third entry in C_FoF_Player's six-value
// private Steam-stat cache: max_games_played. These achievements are awarded
// on the next local enemy kill after the corresponding threshold is reached.
#define DECLARE_FOF_MATCH_COUNT_ACHIEVEMENT( \
	className, achievementID, requiredMatches ) \
	class className : public CFoFAchievementRegistration \
	{ \
		virtual void Init() \
		{ \
			InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 1 ); \
		} \
		virtual void Event_EntityKilled( \
			CBaseEntity *pVictim, CBaseEntity *pAttacker, \
			CBaseEntity *pInflictor, IGameEvent *pEvent ) \
		{ \
			C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer(); \
			if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) && \
				pLocalPlayer && pLocalPlayer->GetFoFMaxGamesPlayed() >= \
					requiredMatches ) \
			{ \
				IncrementCount(); \
			} \
		} \
	}; \
	DECLARE_ACHIEVEMENT( className, achievementID, #achievementID, 1 )

DECLARE_FOF_MATCH_COUNT_ACHIEVEMENT(
	CAchievementRancher,
	ACHIEVEMENT_RANCHER,
	25 );
DECLARE_FOF_MATCH_COUNT_ACHIEVEMENT(
	CAchievementGunfighter,
	ACHIEVEMENT_GUNFIGHTER,
	100 );
DECLARE_FOF_MATCH_COUNT_ACHIEVEMENT(
	CAchievementLegend,
	ACHIEVEMENT_LEGEND,
	500 );

DECLARE_FOF_USER_EVENT_ACHIEVEMENT(
	CAchievementHatShot,
	ACHIEVEMENT_HATSHOT,
	15,
	"hatshot",
	"entindex_hatshot" );

DECLARE_FOF_USER_EVENT_ACHIEVEMENT(
	CAchievementMobileCannonOperator,
	ACHIEVEMENT_MOBILE_CANNON_OPERATOR2,
	1,
	"course_mobile_completed",
	"entindex_player" );

class CAchievementSlidingKiller : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( FOF_KILL_ACHIEVEMENT_FLAGS, 25 );
	}

	virtual void Event_EntityKilled(
		CBaseEntity *pVictim, CBaseEntity *pAttacker,
		CBaseEntity *pInflictor, IGameEvent *pEvent )
	{
		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( FoFIsLocalPlayerKill( pVictim, pAttacker ) &&
			pLocalPlayer && pLocalPlayer->GetFoFSlideForce() > 0.1f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementSlidingKiller,
	ACHIEVEMENT_SLIDING_KILLER,
	"ACHIEVEMENT_SLIDING_KILLER",
	1 );

class CAchievementBouncingAround : public CFoFAchievementRegistration
{
	virtual void Init()
	{
		InitRegistration( ACH_SAVE_GLOBAL, 25 );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "player_death" );
	}

	virtual void FireGameEvent_Internal( IGameEvent *pEvent )
	{
		if ( !pEvent || Q_stricmp( pEvent->GetName(), "player_death" ) )
			return;

		const int nLocalUserID = FoFGetLocalAchievementUserID();
		const int nVictimUserID = pEvent->GetInt( "userid" );
		if ( nLocalUserID < 0 ||
			pEvent->GetInt( "attacker" ) != nLocalUserID ||
			nVictimUserID == nLocalUserID ||
			Q_stricmp( pEvent->GetString( "weapon" ), "kick" ) )
		{
			return;
		}

		C_FoF_Player *pLocalPlayer = FoFGetLocalAchievementPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetFoFLastWallJumpTime() >
			gpGlobals->curtime - 1.2f )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT(
	CAchievementBouncingAround,
	ACHIEVEMENT_BOUNCING_AROUND,
	"ACHIEVEMENT_BOUNCING_AROUND",
	1 );

#undef DECLARE_FOF_MATCH_COUNT_ACHIEVEMENT
#undef DECLARE_FOF_USER_EVENT_ACHIEVEMENT
#undef FOF_KILL_ACHIEVEMENT_FLAGS
