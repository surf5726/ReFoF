#include "cbase.h"

#include "baseanimatedtextureproxy.h"
#include "c_basecombatweapon.h"
#include "c_baseviewmodel.h"
#include "cliententitylist.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "fof/fof_combat_effects.h"
#include "fof/c_fof_entities.h"
#include "fof/c_fof_player.h"
#include "fof/fof_weapon_properties.h"
#include "functionproxy.h"
#include "fx.h"
#include "fx_sparks.h"
#include "glow_outline_effect.h"
#include "hl2mp_gamerules.h"
#include "hl2mp_weapon_parse.h"
#include "IEffects.h"
#include "materialsystem/imaterialvar.h"
#include "particle_parse.h"
#include "particles_simple.h"
#include "prediction.h"
#include "teamplayroundbased_gamerules.h"
#include "tempent.h"
#include "toolframework_client.h"
#include "vphysics/constraints.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar fof_firequality(
	"fof_firequality",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Diverse fire based particle effects quality. 0 = min, 1 = normal, 2 = max",
	true, 0.0f,
	true, 2.0f );

const char *FoFSelectFireParticle(
	int nMode,
	const C_BaseEntity *pAttachedEntity )
{
	const int nFireQuality = fof_firequality.GetInt();

	if ( nMode == 1 )
	{
		switch ( nFireQuality )
		{
		case 0:
			return "arrow_splash_flame_low";
		case 1:
			return "arrow_splash_flame";
		case 2:
			return "arrow_splash_flame_high";
		default:
			return "";
		}
	}

	if ( nMode == 2 )
	{
		switch ( nFireQuality )
		{
		case 1:
			return "barrel_trails";
		case 2:
			return "barrel_trails_high";
		default:
			return "barrel_trails_low";
		}
	}

	if ( nMode > 0 )
		return "";

	const bool bLocalPlayer =
		pAttachedEntity &&
		pAttachedEntity->IsPlayer() &&
		pAttachedEntity == C_BasePlayer::GetLocalPlayer();

	return !bLocalPlayer && nFireQuality > 1 ?
		"burning_character" :
		"burning_character_low";
}

// FoF material proxies used by the shipped weapon and sleeve materials.

ConVar player_local_team( "player_local_team", "0", 0, "" );

ConVar fof_sv_team_remap_1(
	"fof_sv_team_remap_1", "2", FCVAR_REPLICATED,
	"reassign a team to team slot 1: "
	"2-vigilantes, 3-desperados, 4-bandidos, 5-rangers",
	true, 2.0f, true, 5.0f );
ConVar fof_sv_team_remap_2(
	"fof_sv_team_remap_2", "3", FCVAR_REPLICATED,
	"reassign a team to team slot 2: "
	"2-vigilantes, 3-desperados, 4-bandidos, 5-rangers",
	true, 2.0f, true, 5.0f );
ConVar fof_sv_team_remap_3(
	"fof_sv_team_remap_3", "4", FCVAR_REPLICATED,
	"reassign a team to team slot 3: "
	"2-vigilantes, 3-desperados, 4-bandidos, 5-rangers",
	true, 2.0f, true, 5.0f );
ConVar fof_sv_team_remap_4(
	"fof_sv_team_remap_4", "5", FCVAR_REPLICATED,
	"reassign a team to team slot 4: "
	"2-vigilantes, 3-desperados, 4-bandidos, 5-rangers",
	true, 2.0f, true, 5.0f );

ConVar fof_player_voice(
	"fof_player_voice", "1",
	FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_SERVER_CAN_EXECUTE,
	"Selects a voice taunt for shootout game "
	"0 vigilante, 1 desperado, 2 bandido, 3 ranger",
	true, 0.0f, true, 3.0f );

int FoFResolvePlayerTeamSleeveFrame( const C_BasePlayer *pPlayer )
{
	if ( !pPlayer )
		return 0;

	int nMappedTeam = 0;
	switch ( pPlayer->GetTeamNumber() )
	{
	case 2:
		nMappedTeam = fof_sv_team_remap_1.GetInt();
		break;
	case 3:
		nMappedTeam = fof_sv_team_remap_2.GetInt();
		break;
	case 4:
		nMappedTeam = fof_sv_team_remap_3.GetInt();
		break;
	case 5:
		nMappedTeam = fof_sv_team_remap_4.GetInt();
		break;
	case 6:
		nMappedTeam = 6;
		break;
	default:
		return 0;
	}

	return clamp( nMappedTeam - 2, 0, 3 );
}

int FoFResolveLocalSleeveFrame()
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && HL2MPRules() && HL2MPRules()->IsTeamplay() )
		return FoFResolvePlayerTeamSleeveFrame( pLocalPlayer );

	return clamp( fof_player_voice.GetInt(), 0, 3 );
}

void FoFSetLocalSleeveFrame( int nSleeveFrame )
{
	nSleeveFrame = clamp( nSleeveFrame, 0, 3 );
	if ( player_local_team.GetInt() != nSleeveFrame )
		player_local_team.SetValue( nSleeveFrame );
}

void FoFUpdateLocalSleeveFrame()
{
	FoFSetLocalSleeveFrame( FoFResolveLocalSleeveFrame() );
}

static bool FoFIsBoosterActive( C_BaseEntity *pEntity )
{
	C_BaseCombatWeapon *pWeapon = dynamic_cast< C_BaseCombatWeapon * >( pEntity );
	if ( pWeapon && pWeapon->m_bFiresUnderwater )
		return true;

	C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel * >( pEntity );
	return pViewModel && pViewModel->m_bReadyToDraw;
}

class CProxyBooster : public CResultProxy
{
public:
	virtual void OnBind( void *pBindArgument )
	{
		if ( !pBindArgument )
			return;

		C_BaseEntity *pEntity = BindArgToEntity( pBindArgument );
		if ( !pEntity )
			return;

		SetFloatResult( FoFIsBoosterActive( pEntity ) ? 1.0f : 0.0f );
		if ( ToolsEnabled() )
			ToolFramework_RecordMaterialParams( GetMaterial() );
	}
};

class CProxyBoosterEnv : public CResultProxy
{
public:
	virtual void OnBind( void *pBindArgument )
	{
		if ( !pBindArgument )
			return;

		C_BaseEntity *pEntity = BindArgToEntity( pBindArgument );
		if ( !pEntity )
			return;

		if ( FoFIsBoosterActive( pEntity ) )
			m_pResult->SetVecValue( 2.5f, 2.5f, 1.15f );
		if ( ToolsEnabled() )
			ToolFramework_RecordMaterialParams( GetMaterial() );
	}
};

class CSleeveTexture : public CBaseAnimatedTextureProxy
{
public:
	virtual void OnBind( void *pBindArgument )
	{
		NOTE_UNUSED( pBindArgument );
		if ( !m_AnimatedTextureVar || !m_AnimatedTextureFrameNumVar ||
			m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
			return;

		// The original client resolves the displayed faction
		// through the server's team-remap cvars. Shootout players instead use
		// their selected voice/faction. Resolve again at bind time so a
		// replicated remap change cannot leave an old sleeve frame cached.
		m_AnimatedTextureFrameNumVar->SetIntValue(
			FoFResolveLocalSleeveFrame() );
	}

protected:
	virtual float GetAnimationStartTime( void *pBindArgument )
	{
		NOTE_UNUSED( pBindArgument );
		return 0.0f;
	}
};

class CRotatorHelpTexture : public CBaseAnimatedTextureProxy
{
public:
	virtual void OnBind( void *pBindArgument )
	{
		NOTE_UNUSED( pBindArgument );
		if ( !m_AnimatedTextureVar || !m_AnimatedTextureFrameNumVar ||
			m_AnimatedTextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
		{
			return;
		}

		static ConVarRef rotatorFrame( "rotator_frame", true );
		if ( rotatorFrame.IsValid() )
			m_AnimatedTextureFrameNumVar->SetIntValue( rotatorFrame.GetInt() );
	}

protected:
	virtual float GetAnimationStartTime( void *pBindArgument )
	{
		NOTE_UNUSED( pBindArgument );
		return 0.0f;
	}
};

EXPOSE_INTERFACE( CProxyBooster, IMaterialProxy, "Booster" IMATERIAL_PROXY_INTERFACE_VERSION );
EXPOSE_INTERFACE( CProxyBoosterEnv, IMaterialProxy, "BoosterEnv" IMATERIAL_PROXY_INTERFACE_VERSION );
EXPOSE_INTERFACE( CSleeveTexture, IMaterialProxy, "SleeveProxy" IMATERIAL_PROXY_INTERFACE_VERSION );
EXPOSE_INTERFACE( CRotatorHelpTexture, IMaterialProxy, "RotatorHelpTexture" IMATERIAL_PROXY_INTERFACE_VERSION );

static ConVar fof_smoke_amount(
	"fof_smoke_amount",
	"5.0",
	FCVAR_ARCHIVE,
	"How many smoke pufs are emitted by weapons",
	true, 0.0f, true, 16.0f );

static ConVar fof_smoke_alpha_max(
	"fof_smoke_alpha_max",
	"0.4",
	FCVAR_ARCHIVE,
	"How translucid smoke is initially",
	true, 0.0f, true, 1.0f );

static ConVar fof_smoke_alpha_min(
	"fof_smoke_alpha_min",
	"0.05",
	FCVAR_ARCHIVE,
	"How translucid smoke is when removed",
	true, 0.0f, true, 1.0f );

static ConVar fof_smoke_size(
	"fof_smoke_size",
	"0.6",
	FCVAR_ARCHIVE,
	"Smoke's initial size",
	true, 0.0f, true, 1.0f );

static ConVar fof_smoke_life(
	"fof_smoke_life",
	"4.0",
	FCVAR_ARCHIVE,
	"How many seconds take the smoke to dissipate",
	true, 0.0f, true, 10.0f );

// These are client children of the matching server-owned replicated
// ConVars.  The shipped client uses a positive server value as an override
// and otherwise falls back to the five archived client settings above.
static ConVar fof_sv_smoke_amount(
	"fof_sv_smoke_amount",
	"0",
	FCVAR_REPLICATED,
	"How many smoke pufs are emitted by weapons (forced on clients)",
	true, 0.0f, true, 10.0f );

static ConVar fof_sv_smoke_alpha_max(
	"fof_sv_smoke_alpha_max",
	"0",
	FCVAR_REPLICATED,
	"How translucid smoke is initially (forced on clients)",
	true, 0.0f, true, 1.0f );

static ConVar fof_sv_smoke_alpha_min(
	"fof_sv_smoke_alpha_min",
	"0",
	FCVAR_REPLICATED,
	"How translucid smoke is when removed (forced on clients)",
	true, 0.0f, true, 1.0f );

static ConVar fof_sv_smoke_size(
	"fof_sv_smoke_size",
	"0",
	FCVAR_REPLICATED,
	"Smoke's initial size (forced on clients)",
	true, 0.0f, true, 1.0f );

static ConVar fof_sv_smoke_life(
	"fof_sv_smoke_life",
	"0",
	FCVAR_REPLICATED,
	"How many seconds take the smoke to dissipate (forced on clients)",
	true, 0.0f, true, 10.0f );

static void FoFGunSmokeQualityChanged(
	IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	NOTE_UNUSED( pOldValue );
	NOTE_UNUSED( flOldValue );

	ConVarRef quality( pConVar );
	const float flQuality = quality.IsValid() ? quality.GetFloat() : 3.0f;
	if ( flQuality <= 0.0f )
	{
		fof_smoke_amount.SetValue( 0 );
		return;
	}

	// The original client maps quality levels 1..5 onto
	// the individual archived controls using these exact endpoints.
	fof_smoke_amount.SetValue(
		RemapValClamped( flQuality, 1.0f, 5.0f, 3.0f, 7.0f ) );
	fof_smoke_alpha_max.SetValue(
		RemapValClamped( flQuality, 1.0f, 5.0f, 0.4f, 0.6f ) );
	fof_smoke_alpha_min.SetValue(
		RemapValClamped( flQuality, 1.0f, 5.0f, 0.2f, 0.01f ) );
	fof_smoke_size.SetValue(
		RemapValClamped( flQuality, 1.0f, 5.0f, 0.5f, 0.85f ) );
	fof_smoke_life.SetValue(
		RemapValClamped( flQuality, 1.0f, 5.0f, 3.0f, 7.0f ) );
}

static ConVar fof_smoke(
	"fof_smoke",
	"3.0",
	FCVAR_ARCHIVE,
	"Gun smoke global quality, 0 to disable any gun smoke",
	true, 0.0f, true, 5.0f,
	FoFGunSmokeQualityChanged );

class CBG2SmokeEmitter : public CSimpleEmitter
{
public:
	DECLARE_CLASS( CBG2SmokeEmitter, CSimpleEmitter );

	static CSmartPtr< CBG2SmokeEmitter > Create()
	{
		CBG2SmokeEmitter *pEmitter = new CBG2SmokeEmitter;
		pEmitter->SetDynamicallyAllocated( true );
		return pEmitter;
	}

protected:
	CBG2SmokeEmitter()
		: CSimpleEmitter( "MuzzleFlash_Smoke_Shared" )
	{
		m_vecDrift.Init(
			random->RandomFloat( -10.0f, 10.0f ),
			random->RandomFloat( -10.0f, 10.0f ),
			random->RandomFloat( 3.0f, 4.0f ) );
	}

	virtual void UpdateVelocity(
		SimpleParticle *pParticle, float flTimeDelta ) OVERRIDE
	{
		Vector vecDirection = pParticle->m_vecVelocity;
		VectorNormalize( vecDirection );

		const float flLifeFraction = pParticle->m_flDieTime > 0.0f ?
			pParticle->m_flLifetime / pParticle->m_flDieTime : 1.0f;
		const float flProbeDistance = 3.0f + Lerp(
			flLifeFraction,
			static_cast< float >( pParticle->m_uchStartSize ),
			static_cast< float >( pParticle->m_uchEndSize ) );
		const Vector vecProbeEnd =
			pParticle->m_Pos + vecDirection * flProbeDistance;

		trace_t trace;
		UTIL_TraceLine(
			pParticle->m_Pos,
			vecProbeEnd,
			MASK_SOLID_BRUSHONLY,
			NULL,
			COLLISION_GROUP_NONE,
			&trace );

		if ( ( trace.fraction < 1.0f || trace.startsolid || trace.allsolid ) &&
			 !( trace.surface.flags & SURF_SKY ) )
		{
			const float flNormalSpeed =
				DotProduct( pParticle->m_vecVelocity, trace.plane.normal );
			pParticle->m_vecVelocity +=
				trace.plane.normal * ( -2.0f * flNormalSpeed );
			pParticle->m_vecVelocity *= random->RandomFloat( 0.0f, 0.2f );
		}

		pParticle->m_vecVelocity *= powf( 2.0f, -1.5f * flTimeDelta );
		pParticle->m_vecVelocity += m_vecDrift * flTimeDelta;
	}

	virtual float UpdateRoll(
		SimpleParticle *pParticle, float flTimeDelta ) OVERRIDE
	{
		pParticle->m_flRoll +=
			pParticle->m_flRollDelta * 0.25f * flTimeDelta;
		return pParticle->m_flRoll;
	}

private:
	Vector m_vecDrift;
};

static float FoFSmokeSetting(
	const ConVar &serverValue, const ConVar &clientValue )
{
	return serverValue.GetFloat() > 0.0f ?
		serverValue.GetFloat() : clientValue.GetFloat();
}

static C_BaseCombatWeapon *FoFMuzzleWeapon( C_BaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;

	C_BaseCombatWeapon *pWeapon = pEntity->MyCombatWeaponPointer();
	if ( pWeapon )
		return pWeapon;

	C_BaseViewModel *pViewModel =
		dynamic_cast< C_BaseViewModel * >( pEntity );
	return pViewModel ? pViewModel->GetWeapon() : NULL;
}

static void FoFCreateSharedMuzzleSmoke(
	C_BaseEntity *pEntity,
	C_BaseCombatWeapon *pWeapon,
	const Vector &vecOrigin,
	const Vector &vecForward,
	bool bFirstPerson )
{
	const CHL2MPSWeaponInfo *pWeaponInfo = FoFWeaponInfo( pWeapon );
	if ( !pWeaponInfo )
		return;

	const int nAmount = fof_sv_smoke_amount.GetInt() > 0 ?
		fof_sv_smoke_amount.GetInt() : fof_smoke_amount.GetInt();
	if ( nAmount <= 0 )
		return;

	const float flGunSmokeMult = MAX( pWeaponInfo->m_flGunSmokeMult, 0.0f );
	const float flSmokeScale = 1.4f;
	const float flLife = FoFSmokeSetting(
		fof_sv_smoke_life, fof_smoke_life );
	float flStartAlpha = FoFSmokeSetting(
		fof_sv_smoke_alpha_max, fof_smoke_alpha_max );
	const float flEndAlpha = FoFSmokeSetting(
		fof_sv_smoke_alpha_min, fof_smoke_alpha_min );
	float flSize = FoFSmokeSetting(
		fof_sv_smoke_size, fof_smoke_size ) * flSmokeScale;

	if ( bFirstPerson )
	{
		flStartAlpha *= 0.65f;
		flSize *= 1.35f;
	}

	CSmartPtr< CBG2SmokeEmitter > pEmitter = CBG2SmokeEmitter::Create();
	pEmitter->SetSortOrigin( vecOrigin );
	pEmitter->SetDrawBeforeViewModel( bFirstPerson );

	const float flMaxOffset = clamp(
		static_cast< float >( nAmount * 3 ) * flGunSmokeMult,
		10.0f,
		70.0f );
	const float flMaxSpeed = clamp(
		static_cast< float >( nAmount * 15 ) *
			flSmokeScale * flGunSmokeMult,
		10.0f,
		100.0f );

	for ( int i = 1; i <= nAmount; ++i )
	{
		const float flFraction = clamp(
			static_cast< float >( i - 1 ) /
				static_cast< float >( nAmount ),
			0.0f,
			1.0f );
		const Vector vecParticleOrigin = vecOrigin + vecForward *
			Lerp( flFraction, 10.0f, flMaxOffset );

		const int nMaterial = clamp( i, 1, 16 );
		char szMaterial[64];
		if ( nMaterial >= 10 )
		{
			Q_snprintf(
				szMaterial, sizeof( szMaterial ),
				"particle/smokesprites_00%i", nMaterial );
		}
		else
		{
			Q_snprintf(
				szMaterial, sizeof( szMaterial ),
				"particle/smokesprites_000%i", nMaterial );
		}

		SimpleParticle *pParticle = static_cast< SimpleParticle * >(
			pEmitter->AddParticle(
				sizeof( SimpleParticle ),
				pEmitter->GetPMaterial( szMaterial ),
				vecParticleOrigin ) );
		if ( !pParticle )
			continue;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime =
			random->RandomFloat( 0.95f, 1.05f ) * flLife;
		pParticle->m_vecVelocity = vecForward *
			Lerp( flFraction, 10.0f, flMaxSpeed );

		const int nColor = random->RandomInt( 150, 180 );
		pParticle->m_uchColor[0] = nColor;
		pParticle->m_uchColor[1] = nColor;
		pParticle->m_uchColor[2] = nColor;
		pParticle->m_uchStartAlpha = static_cast< unsigned char >(
			Square( flStartAlpha ) * 255.0f );
		pParticle->m_uchEndAlpha = static_cast< unsigned char >(
			Square( flEndAlpha ) * 255.0f );

		const float flSizePosition = Lerp( flFraction, 3.0f, 8.0f );
		const int nStartSize = static_cast< int >(
			random->RandomFloat( 1.25f, 1.5f ) *
				flSizePosition * flSize );
		const float flExpansion = RemapValClamped(
			flSize, 0.1f, 1.0f, 2.6f, 1.25f );
		const int nEndSize = static_cast< int >(
			nStartSize * 10.0f * flExpansion * flSize );
		pParticle->m_uchStartSize =
			static_cast< unsigned char >( nStartSize );
		pParticle->m_uchEndSize =
			static_cast< unsigned char >( nEndSize );
		pParticle->m_iFlags = 0;
		pParticle->m_flRoll =
			static_cast< float >( random->RandomInt( 0, 360 ) );
		pParticle->m_flRollDelta = random->RandomFloat( -1.5f, 1.5f );
	}
}

void FoFDispatchLegacyMuzzleParticle(
	FoFMuzzleParticleFamily_t nFamily,
	bool bFirstPerson,
	CBaseHandle hEntity,
	int nAttachmentIndex )
{
	C_BaseEntity *pEntity =
		ClientEntityList().GetBaseEntityFromHandle( hEntity );
	if ( !pEntity || nAttachmentIndex <= 0 )
		return;

	const char *pszParticleName = NULL;
	switch ( nFamily )
	{
	case FOF_MUZZLE_PARTICLE_REVOLVER:
		pszParticleName = bFirstPerson ?
			"muzzle_fof_revolver" :
			"muzzle_fof_revolver_w";
		break;

	case FOF_MUZZLE_PARTICLE_SHOTGUN:
		pszParticleName = bFirstPerson ?
			"muzzle_fof_shotgun" :
			"muzzle_fof_shotgun_w";
		break;

	default:
		return;
	}

	Vector vecOrigin;
	QAngle angMuzzle;
	if ( !FX_GetAttachmentTransform(
			pEntity->GetRefEHandle(),
			nAttachmentIndex,
			&vecOrigin,
			&angMuzzle ) )
	{
		return;
	}

	Vector vecForward;
	AngleVectors( angMuzzle, &vecForward );
	VectorNormalize( vecForward );

	DispatchParticleEffect(
		pszParticleName,
		bFirstPerson ? PATTACH_POINT_FOLLOW : PATTACH_POINT,
		pEntity,
		nAttachmentIndex );

	FoFCreateSharedMuzzleSmoke(
		pEntity,
		FoFMuzzleWeapon( pEntity ),
		vecOrigin,
		vecForward,
		bFirstPerson );
}

static ConVar *FoFFindGlowConVar( const char *pszName )
{
	return cvar ? cvar->FindVar( pszName ) : NULL;
}

bool FoFShouldSuppressPlayerGlow( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return false;

	ConVar *pCurrentMode = FoFFindGlowConVar( "fof_sv_currentmode" );
	if ( !pCurrentMode )
		return false;

	const int nCurrentMode = pCurrentMode->GetInt();
	if ( nCurrentMode == 3 || nCurrentMode == 4 )
	{
		// Bounty and Elimination share the original team-only filter.  During
		// Elimination reveal windows the server raises freevision and the
		// registered enemy glows are allowed through temporarily.
		if ( nCurrentMode == 4 )
		{
			if ( engine && engine->IsHLTV() )
				return false;

			ConVar *pFreeVision =
				FoFFindGlowConVar( "fof_sv_elm_freevision" );
			if ( pFreeVision && pFreeVision->GetBool() )
				return false;
		}

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( !pLocalPlayer || !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
			return false;

		if ( pEntity == pLocalPlayer )
			return true;

		return pEntity->GetTeamNumber() != pLocalPlayer->GetTeamNumber();
	}

	C_FoF_Player *pFoFEntity = dynamic_cast< C_FoF_Player * >( pEntity );
	if ( !pFoFEntity )
		return false;

	ConVar *pTeamGlow = FoFFindGlowConVar( "fof_sv_team_glow" );
	const bool bTeamGlow = pTeamGlow && pTeamGlow->GetBool();
	const bool bTeamplay = HL2MPRules() && HL2MPRules()->IsTeamplay();

	if ( nCurrentMode == 1 )
	{
		// Bit 24 marks entities whose glow must bypass the normal Shootout
		// filtering (the original tests bit zero of the player state byte).
		if ( pFoFEntity->GetFoFPlayerInfo() & 0x1000000 )
			return false;

		if ( bTeamGlow && !bTeamplay )
			return true;

		C_FoF_Player *pLocalPlayer = dynamic_cast< C_FoF_Player * >(
			C_BasePlayer::GetLocalPlayer() );
		if ( bTeamGlow && pLocalPlayer && pEntity == pLocalPlayer )
			return true;

		if ( !bTeamplay || !pLocalPlayer || pEntity == pLocalPlayer )
			return false;

		if ( !bTeamGlow )
			return true;

		return pEntity->GetTeamNumber() != pLocalPlayer->GetTeamNumber();
	}

	if ( nCurrentMode == 2 )
	{
		if ( engine && engine->IsHLTV() )
			return false;

		C_FoF_Player *pLocalPlayer = dynamic_cast< C_FoF_Player * >(
			C_BasePlayer::GetLocalPlayer() );
		if ( !pLocalPlayer )
			return false;
		if ( pEntity == pLocalPlayer )
			return true;
		if ( pEntity->GetTeamNumber() != pLocalPlayer->GetTeamNumber() )
			return true;

		// While jailed, teammates stay visible regardless of the server team
		// glow switch.  This is the original deadline comparison.
		if ( gpGlobals && gpGlobals->curtime <= pLocalPlayer->GetFoFJailTime() )
			return false;

		return !bTeamGlow;
	}

	return false;
}

void FoFPresentMuzzleFlash(
	C_BasePlayer *pOwner,
	C_BaseCombatWeapon *pWeapon,
	bool bSecondViewModel )
{
	if ( !pOwner || !pWeapon )
		return;

	C_BaseViewModel *pViewModel =
		pOwner->GetViewModel( bSecondViewModel ? 1 : 0, true );
	if ( pViewModel )
		pViewModel->DoMuzzleFlash();

	C_BaseCombatWeapon *pActiveWeapon = bSecondViewModel ?
		pOwner->GetActiveWeapon2() : pOwner->GetActiveWeapon1();
	if ( pActiveWeapon == pWeapon )
		pWeapon->DoMuzzleFlash();
}

void FoFCreateMuzzleSmoke( C_BasePlayer *pOwner, bool bSecondViewModel )
{
	if ( !pOwner || pOwner != C_BasePlayer::GetLocalPlayer() )
		return;
	if ( !fof_smoke_trails.GetBool() )
		return;
	if ( pOwner->GetFlags() & FL_INWATER )
		return;
	if ( !prediction || !prediction->InPrediction() || !prediction->IsFirstTimePredicted() )
		return;

	C_BaseViewModel *pViewModel = pOwner->GetViewModel( bSecondViewModel ? 1 : 0 );
	if ( !pViewModel || pViewModel->LookupAttachment( "muzzle" ) <= 0 )
		return;

	pViewModel->ParticleProp()->Create( "muzzle_smoke2", PATTACH_POINT_FOLLOW, "muzzle" );
}

// The original long-gun PrimaryAttack implementations all construct this same
// prediction-seeded request immediately after their player animation event.
// Only the pellet count, auto-aim cone and shotgun damage fields vary.
// C_FoF_Player::FireBullets predicts local traces, impacts and tracers;
// the original server remains authoritative for hits and damage.

void FoFDrawDynamiteFuseFX( C_BaseAnimating *pWeapon, bool bBlackDynamite )
{
	if ( !pWeapon )
		return;

	static float s_flNextFuseFX[MAX_EDICTS];
	const int nEntityIndex = pWeapon->entindex();
	const float flNow = gpGlobals ? gpGlobals->curtime : 0.0f;
	if ( nEntityIndex > 0 && nEntityIndex < ARRAYSIZE( s_flNextFuseFX ) )
	{
		const float flNext = s_flNextFuseFX[nEntityIndex];
		if ( flNext > flNow && flNext - flNow < 1.0f )
			return;
		s_flNextFuseFX[nEntityIndex] = flNow + 0.05f;
	}

	Vector vecPosition;
	QAngle angAttachment;
	const int nAttachment = pWeapon->LookupAttachment( "fuse1" );
	if ( nAttachment <= 0 ||
		!pWeapon->GetAttachment( nAttachment, vecPosition, angAttachment ) )
	{
		return;
	}

	g_pEffects->Smoke( vecPosition, 0, bBlackDynamite ? 3.5f : 2.5f, 20.0f );
	Vector vecUnusedForward;
	AngleVectors( angAttachment, &vecUnusedForward );

	CSmartPtr< CTrailParticles > pEmitter = CTrailParticles::Create( "FX_ElectricSpark 2" );
	if ( !pEmitter )
		return;

	PMaterialHandle hMaterial = pEmitter->GetPMaterial( "effects/spark" );
	pEmitter->SetSortOrigin( vecPosition );
	const int nSparks = random->RandomInt( 16, 32 );
	for ( int i = 0; i < nSparks; ++i )
	{
		TrailParticle *pParticle = static_cast< TrailParticle * >(
			pEmitter->AddParticle( sizeof( TrailParticle ), hMaterial, vecPosition ) );
		if ( !pParticle )
			return;

		pParticle->m_flLifetime = 0.0f;
		Vector vecDirection;
		vecDirection.Random( -1.0f, 1.0f );
		pParticle->m_flWidth = random->RandomFloat( 0.7f, 0.9f );
		pParticle->m_flLength = random->RandomFloat( 0.005f, 0.01f ) * 0.01f;
		pParticle->m_flDieTime = ( bBlackDynamite ?
			random->RandomFloat( 0.20f, 0.25f ) :
			random->RandomFloat( 0.15f, 0.20f ) ) * 0.20f;
		pParticle->m_vecVelocity = vecDirection * random->RandomFloat( 34.0f, 66.0f );
		Color32Init( pParticle->m_color, 128, 128, 128, 128 );
	}
}
