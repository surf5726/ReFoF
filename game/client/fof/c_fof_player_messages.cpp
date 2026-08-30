#include "cbase.h"
#include "fof/c_fof_player.h"
#include "c_playerresource.h"
#include "fof/fof_hints.h"
#include "fof/fof_hud.h"
#include "fof/fof_steam_stats.h"
#include "fof/fof_workshop_uploader.h"
#include "KeyValues.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "ScreenSpaceEffects.h"
#include "steam/steam_api.h"
#include "cliententitylist.h"
#include "fof/c_fof_entities.h"
#include "fof/fof_client_settings.h"
#include "fx.h"
#include "physpropclientside.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void C_FoF_Player::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	const int nMessageType = msg.ReadByte();
	switch ( nMessageType )
	{
	case 1:
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
	case 13:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
		ReceiveFoFPresentationMessage( nMessageType, msg );
		break;

	case 2:
	case 7:
	case 9:
	case 10:
	case 12:
	case 14:
	case 15:
		ReceiveFoFStateMessage( nMessageType, msg );
		break;

	default:
		break;
	}
}

// Original hidden client ConVar selected by player entity message 2.
static ConVar rotator_frame(
	"rotator_frame",
	"0",
	FCVAR_HIDDEN,
	"FoF rotating presentation frame." );

// The shipped client uses these two ConVars for its kill-streak screenshot path.
static ConVar fof_listenserver(
	"fof_listenserver",
	"0",
	FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE,
	"Whether this FoF client is also hosting the listen server.",
	true, 0.0f, true, 1.0f );

static ConVar scr_count(
	"scr_count",
	"0",
	FCVAR_ARCHIVE,
	"FoF kill-streak screenshot sequence number." );

struct FoFPersonalStat_t
{
	const char *m_pszSteamName;
	const char *m_pszHudLabel;
	bool m_bShowHudNotice;
};

// Shipped personal-stat table. The third entry is persisted but
// deliberately has no HUD label/notice.
static const FoFPersonalStat_t s_FoFPersonalStats[] =
{
	{ "max_killstreak",    "#FoF_Stat_Killstreak",   true  },
	{ "max_survival_time", "#FoF_Stat_SurvivalTime", true  },
	{ "max_games_played",  NULL,                     false },
	{ "max_damage_acc",    "#FoF_Stat_MaxDmg",        true  },
	{ "max_whiskey_acc",   "#FoF_Stat_MaxWhiskey",    true  },
	{ "max_weapons_used",  "#FoF_Stat_MaxWeapons",    true  },
};

void C_FoF_Player::Simulate()
{
	BaseClass::Simulate();
	UpdateFoFStatAccuracyReport();
}

void C_FoF_Player::UpdateFoFStatAccuracyReport()
{
	// Original C_FoF_Player::Simulate suppresses this private
	// client-to-server statistic while watching SourceTV.  A local player sends
	// the current aggregate accuracy immediately, then at random 5--20 second
	// intervals through the original fof_stat_acc command.
	if ( this != C_BasePlayer::GetLocalPlayer() ||
		!engine->IsInGame() || engine->IsHLTV() ||
		gpGlobals->curtime <= m_flFoFNextStatAccuracyReport )
	{
		return;
	}

	char command[64];
	Q_snprintf(
		command,
		sizeof( command ),
		"fof_stat_acc %f",
		FoFComputeReportedAccuracy() );
	engine->ClientCmd( command );
	m_flFoFNextStatAccuracyReport = gpGlobals->curtime +
		random->RandomFloat( 5.0f, 20.0f );
}

void C_FoF_Player::ReceiveFoFStateMessage(
	int nMessageType, bf_read &msg )
{
	switch ( nMessageType )
	{
	case 2:
		// The original handler always removes model decals first.  Its remaining
		// statistics/bootstrap work is a local one-shot.
		RemoveAllDecals();
		if ( m_bFoFInitialMessageHandled ||
			this != C_BasePlayer::GetLocalPlayer() )
		{
			break;
		}
		{
			static ConVarRef currentMode( "fof_sv_currentmode", true );
			random->SetSeed( gpGlobals->framecount );
			const int nMaximumFrame =
				currentMode.IsValid() && currentMode.GetInt() == 1 ? 1 : 0;
			rotator_frame.SetValue(
				random->RandomInt( 0, nMaximumFrame ) );
			m_bFoFInitialMessageHandled = true;

			// The original client reads the six integer table entries and submits
			// each one to the original server
			// through "fof_stat <index> <value>", then caches it locally.
			// GetStat failures fall back to zero in the original, but all six
			// commands are still queued before their values are cached.
			ISteamUserStats *pUserStats =
				steamapicontext ? steamapicontext->SteamUserStats() : NULL;
			if ( engine->GetAchievementMgr() )
			{
				for ( int i = 0;
					i < ARRAYSIZE( s_FoFPersonalStats ); ++i )
				{
					int nValue = 0;
					if ( pUserStats )
					{
						pUserStats->GetStat(
							s_FoFPersonalStats[i].m_pszSteamName,
							&nValue );
					}

					char command[64];
					Q_snprintf(
						command, sizeof( command ),
						"fof_stat %d %d", i, nValue );
					engine->ClientCmd( command );
					m_iFoFPersonalStats[i] = nValue;
				}
			}

			m_flFoFLastHintTime = 0.0f;
			m_flFoFHintDelayScale =
				FoFInitializePlayerHints();

			// The retired account service can no longer resolve this entitlement.
			// Preserve the local mobile-cannon capability for standalone clients.
			engine->ClientCmd( "mco" );
		}
		break;

	case 7:
		// Pickup cancellation carries one currently-unused byte.
		// Restore the attached world entity and release only our presentation
		// copy.  The original server queues this reliable message before it
		// changes m_bPickupActive and m_hAttachedObject differently, so suppress
		// the canceled generation until
		// those authoritative fields advance.
		msg.ReadByte();
		m_bFoFCarryCancelPending = true;
		m_hFoFCarryCanceledObject = m_hAttachedObject;
		m_flFoFCarryCanceledInteraction = m_flNextPickupInteraction;
		ClearFoFCarryPresentation();
		break;

	case 9:
		{
			// Hint tag requests always carry their bounded string and signed
			// target short before any instance/local-player filtering.
			char tags[128];
			msg.ReadString( tags, sizeof( tags ), false );
			const int nTargetEntIndex = (short)msg.ReadShort();
			if ( nTargetEntIndex != entindex() ||
				this != C_BasePlayer::GetLocalPlayer() ||
				m_flFoFHintDelayScale >= 1.0f )
			{
				break;
			}

			const float flDelay = RemapValClamped(
				m_flFoFHintDelayScale,
				0.0f, 1.0f,
				150.0f, 250.0f );
			if ( gpGlobals->curtime - m_flFoFLastHintTime <= flDelay )
				break;

			if ( FoFShowPlayerHintTags(
					tags, random->RandomInt( 0, 6 ) ) )
			{
				m_flFoFLastHintTime = gpGlobals->curtime;
			}
		}
		break;

	case 10:
		{
			// All three signed shorts are consumed before the local/target
			// filter, matching the original and keeping malformed indices bounded.
			const int nStatIndex = (short)msg.ReadShort();
			const int nNewValue = (short)msg.ReadShort();
			const int nTargetEntIndex = (short)msg.ReadShort();
			if ( nTargetEntIndex != entindex() ||
				this != C_BasePlayer::GetLocalPlayer() ||
				nStatIndex < 0 || nStatIndex >= 6 )
			{
				break;
			}

			const FoFPersonalStat_t &stat =
				s_FoFPersonalStats[nStatIndex];
			int nOldValue = 0;
			if ( steamapicontext && steamapicontext->SteamUserStats() )
			{
				ISteamUserStats *pUserStats =
					steamapicontext->SteamUserStats();
				pUserStats->GetStat( stat.m_pszSteamName, &nOldValue );

				// The original helper treats zero as a read-only query, while
				// every non-zero value replaces the server-owned maximum.
				if ( nNewValue != 0 &&
					pUserStats->SetStat( stat.m_pszSteamName, nNewValue ) )
				{
					// FoF's private stats object marks its store state dirty
					// here.  This compatibility layer has no equivalent
					// callback owner, so commit the same authoritative value
					// immediately through the public Steam interface.
					pUserStats->StoreStats();
				}
			}

			if ( stat.m_bShowHudNotice )
			{
				FoFPresentStatUpdate(
					stat.m_pszHudLabel, nNewValue, nOldValue );
			}
		}
		break;

	case 12:
		// The shipped local groggy effect does not consume the
		// payload on pre-DX8 hardware, then expects target index and duration
		// as signed shorts and enables the stock EP2 screen-space effect.
		if ( !g_pMaterialSystemHardwareConfig ||
			g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 80 )
		{
			break;
		}
		{
			const int nTargetEntIndex = (short)msg.ReadShort();
			if ( nTargetEntIndex != entindex() ||
				this != C_BasePlayer::GetLocalPlayer() )
			{
				break;
			}

			const float flDuration = (float)(short)msg.ReadShort();
			if ( g_pScreenSpaceEffects )
			{
				KeyValues *pKeys = new KeyValues( "keys" );
				pKeys->SetFloat( "duration", flDuration );
				pKeys->SetInt( "fadeout", 1 );
				g_pScreenSpaceEffects->SetScreenSpaceEffectParams(
					"ep2_groggy", pKeys );
				g_pScreenSpaceEffects->EnableScreenSpaceEffect(
					"ep2_groggy" );
				pKeys->deleteThis();
			}
		}
		break;

	case 14:
		// The original deliberately does not consume this local-only float for
		// remote player instances.
		if ( this == C_BasePlayer::GetLocalPlayer() )
		{
			const float flDelta =
				clamp( msg.ReadFloat(), 0.0f, 200.0f );
			m_flFoFRoundPlayTime += flDelta;

			ISteamUserStats *pUserStats =
				steamapicontext ? steamapicontext->SteamUserStats() : NULL;
			if ( pUserStats && !m_bFoFSteamPlayTimeLoaded )
			{
				float flStoredPlayTime = 0.0f;
				if ( pUserStats->GetStat(
						"stat_play_time", &flStoredPlayTime ) )
				{
					m_flFoFSessionPlayTime +=
						MAX( flStoredPlayTime, 0.0f );
					m_bFoFSteamPlayTimeLoaded = true;
				}
			}

			m_flFoFSessionPlayTime += flDelta;
			if ( pUserStats && m_bFoFSteamPlayTimeLoaded &&
				pUserStats->SetStat(
					"stat_play_time", m_flFoFSessionPlayTime ) )
			{
				pUserStats->StoreStats();
			}
		}
		break;

	case 15:
		// The shipped one-shot kill-streak capture creates a JPEG and then a
		// real FriendsOnly Workshop item.  Reproduce its exact eligibility and
		// metadata, but require an explicit per-session opt-in before making
		// either filesystem or Steam-account changes.
		if ( !m_bFoFScreenshotHandled &&
			!fof_listenserver.GetBool() &&
			m_nMultiKill == random->RandomInt( 5, 10 ) &&
			g_PR &&
			( g_PR->GetFoFState( entindex() ) & 0xE00 ) != 0 )
		{
			m_bFoFScreenshotHandled = true;
			if ( !FoFWorkshopUploadEnabled() )
				break;

			const int screenshotIndex = scr_count.GetInt() + 1;
			scr_count.SetValue( screenshotIndex );

			char command[64];
			Q_snprintf(
				command,
				sizeof( command ),
				"jpeg %d 30",
				screenshotIndex );
			engine->ClientCmd( command );

			const char *playerName =
				g_PR->GetPlayerName( entindex() );
			if ( !playerName || !playerName[0] )
				playerName = "player";

			char title[256];
			char description[64];
			char preview[MAX_PATH];
			Q_snprintf(
				title,
				sizeof( title ),
				"%s_KSx%d",
				playerName,
				m_nMultiKill );
			Q_snprintf(
				description,
				sizeof( description ),
				"%d",
				m_nPlayerKills );
			Q_snprintf(
				preview,
				sizeof( preview ),
				"%s\\screenshots\\%d.jpg",
				engine->GetGameDirectory(),
				screenshotIndex );

			if ( !FoFBeginWorkshopUpload(
					title,
					description,
					preview ) )
			{
				Warning(
					"[FoF] unable to start Workshop upload\n" );
			}
		}
		break;

	default:
		break;
	}
}

void C_FoF_Player::ReceiveFoFPresentationMessage(
	int nMessageType, bf_read &msg )
{
	switch ( nMessageType )
	{
	case 1:
		{
			// Knocked-off hat.  The second compressed vector is
			// part of the shipped wire payload but the original helper never
			// reads it.  The first vector becomes one fifth of the prop's
			// linear velocity; angular velocity is randomized independently.
			Vector vecVelocity;
			Vector vecReserved;
			msg.ReadBitVec3Coord( vecVelocity );
			msg.ReadBitVec3Coord( vecReserved );
			const int nModelIndex = (short)msg.ReadShort();
			NOTE_UNUSED( vecReserved );

			const model_t *pModel = modelinfo->GetModel( nModelIndex );
			const int nHeadAttachment =
				LookupAttachment( "anim_attachment_head" );
			Vector vecOrigin;
			QAngle angles;
			if ( !pModel || nHeadAttachment <= 0 ||
				!GetAttachment( nHeadAttachment, vecOrigin, angles ) )
			{
				break;
			}

			// The original allocates this presentation prop directly and does
			// not apply cl_phys_props_max to a gameplay-significant hat shot.
			C_PhysPropClientside *pHat =
				C_PhysPropClientside::CreateNew( true );
			if ( !pHat )
				break;

			vecOrigin.z += 5.0f;
			pHat->SetModelName( modelinfo->GetModelName( pModel ) );
			pHat->SetAbsOrigin( vecOrigin );
			pHat->SetAbsAngles( angles );
			pHat->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );
			if ( !pHat->Initialize() )
			{
				pHat->Release();
				break;
			}

			IPhysicsObject *pPhysicsObject = pHat->VPhysicsGetObject();
			if ( !pPhysicsObject )
			{
				pHat->Release();
				break;
			}

			vecVelocity *= 0.2f;
			AngularImpulse angularVelocity =
				RandomAngularImpulse( 200.0f, 500.0f );
			pPhysicsObject->AddVelocity(
				&vecVelocity, &angularVelocity );
			pHat->StartFadeOut( 10.0f );
		}
		break;

	case 3:
		// Self-explosion presentation.  Both effects are local
		// particles; the original client suppresses them under low-violence
		// settings and when human blood is disabled.
		if ( fof_blood_allowed.GetBool() &&
			UTIL_ShouldShowBlood( BLOOD_COLOR_RED ) &&
			!UTIL_IsLowViolence() )
		{
			DispatchParticleEffect(
				"fof_selfexplosion",
				PATTACH_POINT_FOLLOW,
				this,
				"anim_attachment_head" );
			DispatchParticleEffect(
				"fof_xbow_explosion",
				PATTACH_ABSORIGIN,
				NULL,
				-1 );
		}
		break;

	case 4:
		{
			// Metal-impact message: origin, angles, then a byte
			// selecting the ordinary or pellet spark system.
			Vector vecOrigin;
			QAngle angles;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitAngles( angles );
			const bool bPellet = msg.ReadByte() != 0;
			DispatchParticleEffect(
				bPellet ? "plate_sparks_pellet" : "plate_sparks",
				vecOrigin,
				angles );
		}
		break;

	case 5:
		// Blood burst has no payload.  It is placed 35 units
		// above this player and follows the original quality/violence gates.
		if ( fof_blood_allowed.GetBool() &&
			fof_visual_quality.GetInt() != 0 &&
			UTIL_ShouldShowBlood( BLOOD_COLOR_RED ) &&
			!UTIL_IsLowViolence() )
		{
			Vector vecOrigin = GetAbsOrigin();
			vecOrigin.z += 35.0f;
			DispatchParticleEffect(
				"bigboom_blood", vecOrigin, vec3_angle );
		}
		break;

	case 6:
		if ( !fof_blood_allowed.GetBool() ||
			!UTIL_ShouldShowBlood( BLOOD_COLOR_RED ) ||
			UTIL_IsLowViolence() )
		{
			break;
		}
		{
			// General blood impact: impact origin, direction,
			// then signed damage amount. The shipped client applies its violence gate
			// before consuming this optional presentation payload.
			Vector vecOrigin;
			Vector vecDirection;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitVec3Coord( vecDirection );
			const int nAmount = (short)msg.ReadShort();
			UTIL_BloodDrips(
				vecOrigin, vecDirection, BloodColor(), nAmount );

			if ( fof_visual_quality.GetInt() != 0 )
			{
				int nDecalCount = 2;
				float flNoise = 0.1f;
				if ( nAmount >= 50 )
				{
					nDecalCount = 12;
					flNoise = 0.3f;
				}
				else if ( nAmount >= 30 )
				{
					nDecalCount = 8;
					flNoise = 0.2f;
				}
				else if ( nAmount >= 15 )
				{
					nDecalCount = 4;
				}

				for ( int i = 0; i < nDecalCount; ++i )
				{
					Vector vecTraceDirection = -vecDirection;
					vecTraceDirection.x +=
						random->RandomFloat( -flNoise, flNoise );
					vecTraceDirection.y +=
						random->RandomFloat( -flNoise, flNoise );
					vecTraceDirection.z +=
						random->RandomFloat( -flNoise, flNoise );

					trace_t trace;
					UTIL_TraceLine(
						vecOrigin,
						vecOrigin + vecTraceDirection * -172.0f,
						MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE,
						this,
						COLLISION_GROUP_NONE,
						&trace );
					if ( trace.fraction != 1.0f )
						UTIL_BloodDecalTrace( &trace, BloodColor() );
				}
			}
		}
		break;

	case 8:
		// Clear lingering first- and second-hand viewmodel particles.
		for ( int i = 0; i < 2; ++i )
		{
			C_BaseViewModel *pViewModel = GetViewModel( i, true );
			if ( pViewModel )
				pViewModel->ParticleProp()->StopEmissionAndDestroyImmediately();
		}
		break;

	case 13:
		// World muzzle boost.  The first short identifies the
		// player for whom the first-person effect was already presented; the
		// following 32-bit entity index names the world weapon to decorate.
		if ( fof_visual_quality.GetInt() != 0 )
		{
			const int nFirstPersonEntIndex = (short)msg.ReadShort();
			if ( this == C_BasePlayer::GetLocalPlayer() &&
				nFirstPersonEntIndex == entindex() )
			{
				break;
			}

			const int nWeaponEntIndex = msg.ReadLong();
			C_BaseCombatWeapon *pWeapon =
				dynamic_cast< C_BaseCombatWeapon * >(
					cl_entitylist->GetBaseEntity( nWeaponEntIndex ) );
			if ( pWeapon && pWeapon->GetModel() &&
				pWeapon->LookupAttachment( "muzzle" ) > 0 )
			{
				DispatchParticleEffect(
					"fof_boost_world",
					PATTACH_POINT_FOLLOW,
					pWeapon,
					"muzzle" );
			}
		}
		break;

	case 16:
		{
			Vector vecPosition;
			msg.ReadBitVec3Coord( vecPosition );
			CNewParticleEffect *pEffect = ParticleProp()->Create(
				"heal_safezone", PATTACH_POINT_FOLLOW, "forward" );
			if ( pEffect )
				pEffect->SetControlPoint( 1, vecPosition );
		}
		break;

	case 17:
		{
			// The shipped local footstep-particle message keeps
			// 200 ownerless effects alive for exactly five seconds and assigns
			// their sort origin, two control points, and attachment basis.
			const int nTargetEntIndex = msg.ReadByte();
			Vector vecOrigin;
			QAngle angles;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitAngles( angles );
			if ( this == C_BasePlayer::GetLocalPlayer() &&
				nTargetEntIndex == entindex() )
			{
				for ( int i = 0; i < ARRAYSIZE( m_pFoFFootsteps ); ++i )
				{
					if ( m_flFoFFootstepEnd[i] != 0.0f ||
						m_pFoFFootsteps[i].GetObject() )
					{
						continue;
					}

					m_pFoFFootsteps[i] =
						CNewParticleEffect::Create(
							NULL, "footsteps" );
					if ( m_pFoFFootsteps[i].GetObject() )
					{
						Vector vecForward;
						Vector vecRight;
						Vector vecUp;
						AngleVectors(
							angles,
							&vecForward,
							&vecRight,
							&vecUp );
						m_pFoFFootsteps[i]->SetSortOrigin( vecOrigin );
						m_pFoFFootsteps[i]->SetControlPoint(
							0, vecOrigin );
						m_pFoFFootsteps[i]->SetControlPoint(
							1, vecOrigin );
						m_pFoFFootsteps[i]->SetControlPointOrientation(
							0,
							vecForward,
							vecRight,
							vecUp );
						m_flFoFFootstepEnd[i] =
							gpGlobals->curtime + 5.0f;
					}
					break;
				}

				for ( int i = 0; i < ARRAYSIZE( m_pFoFFootsteps ); ++i )
				{
					if ( m_flFoFFootstepEnd[i] != 0.0f &&
						gpGlobals->curtime > m_flFoFFootstepEnd[i] )
					{
						if ( m_pFoFFootsteps[i].GetObject() )
						{
							m_pFoFFootsteps[i]->StopEmission(
								false, false, false );
						}
						m_flFoFFootstepEnd[i] = 0.0f;
						m_pFoFFootsteps[i] = NULL;
					}
				}
			}
		}
		break;

	case 18:
		{
			// Concrete impact.  The original always consumes
			// both compressed vectors and advances this presentation timer,
			// even if the selected visual-quality level hides the particle.
			Vector vecOrigin;
			Vector vecNormal;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitVec3Normal( vecNormal );
			FX_RicochetSound( vecOrigin );
			m_flFoFImpactHintTime =
				gpGlobals->curtime + random->RandomFloat( 1.5f, 10.0f );
			if ( fof_visual_quality.GetInt() != 0 )
			{
				QAngle angles;
				VectorAngles( -vecNormal, angles );
				DispatchParticleEffect(
					"impact_concrete", vecOrigin, angles );
			}
		}
		break;

	case 19:
		// Head-shot droplets deliberately leave the optional
		// payload unread when blood or high-quality effects are disabled.
		if ( fof_blood_allowed.GetBool() &&
			fof_visual_quality.GetInt() != 0 &&
			UTIL_ShouldShowBlood( BLOOD_COLOR_RED ) &&
			!UTIL_IsLowViolence() )
		{
			Vector vecOrigin;
			Vector vecNormal;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitVec3Normal( vecNormal );
			QAngle angles;
			VectorAngles( -vecNormal, angles );
			DispatchParticleEffect(
				"blood_impact_red_01_droplets_headshot",
				vecOrigin,
				angles );
		}
		break;

	case 20:
		{
			const int nQuality = fof_visual_quality.GetInt();
			if ( nQuality == 0 )
				break;

			Vector vecOrigin;
			QAngle angles;
			msg.ReadBitVec3Coord( vecOrigin );
			msg.ReadBitAngles( angles );
			DispatchParticleEffect(
				nQuality == 1 ?
					"horse_steps_dust" : "horse_steps_dust_hq",
				vecOrigin,
				angles );
		}
		break;

	default:
		break;
	}
}
