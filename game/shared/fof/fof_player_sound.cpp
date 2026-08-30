//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF shared player footstep dispatch.
//
//=============================================================================//

#include "cbase.h"
#ifdef CLIENT_DLL
#include "c_recipientfilter.h"
#include "prediction.h"
#define CRecipientFilter C_RecipientFilter
#endif
#include "fof/fof_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
static float FoFFootstepOcclusion( const Vector &origin, const Vector &listener )
{
	Vector direction = listener - origin;
	VectorNormalize( direction );
	Vector start = origin;
	float loss = 0.0f;
	const unsigned int brushMask = CONTENTS_SOLID | CONTENTS_MOVEABLE |
		CONTENTS_WINDOW | CONTENTS_GRATE;
	for ( int i = 0; i < 6; ++i )
	{
		trace_t entry;
		UTIL_TraceLine( start, listener, brushMask,
			NULL, COLLISION_GROUP_NONE, &entry );
		if ( entry.fraction >= 1.0f )
			break;

		trace_t exit;
		UTIL_TraceLine( listener, entry.endpos, brushMask,
			NULL, COLLISION_GROUP_NONE, &exit );
		float materialScale = 1.0f;
		surfacedata_t *pSurface = physprops->GetSurfaceData( entry.surface.surfaceProps );
		if ( pSurface )
		{
			switch ( pSurface->game.material )
			{
			case 'C': materialScale = 1.3f; break;
			case 'G': materialScale = 0.2f; break;
			case 'M': materialScale = 1.5f; break;
			case 'P': materialScale = 0.9f; break;
			case 'W': materialScale = 0.7f; break;
			}
		}
		loss += clamp( entry.endpos.DistTo( exit.endpos ), 0.0f, 20.0f ) * materialScale;
		if ( loss > 200.0f )
			break;
		start = exit.endpos + direction;
	}
	return loss;
}
#endif

void CFoF_Player::PlayStepSound(
	Vector &vecOrigin, surfacedata_t *pSurface, float flVolume, bool bForce )
{
#ifdef CLIENT_DLL
	// Predicted footsteps are presentation events and execute once per command.
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
#endif

	// FoF does not consult the client copy of sv_footsteps here.  The server
	// owns that policy, while both sides use the same surface and volume rules.
	if ( m_bIsBotGhost || !pSurface )
		return;

	// Bit 0x40000 is FoF's forced crouch/ghost locomotion state.  It is the
	// only crouched state that produces a step, using the butt-walk sound.
	if ( ( GetFlags() & FL_DUCKING ) &&
		!( m_nPlayerInfo & 0x40000 ) )
	{
		return;
	}

	const int nSide = m_Local.m_nStepside;
	const unsigned short nStepSound =
		nSide ? pSurface->sounds.stepleft : pSurface->sounds.stepright;
	if ( !nStepSound )
		return;

	IPhysicsSurfaceProps *pPhysicsProps = MoveHelper()->GetSurfaceProps();
	if ( !pPhysicsProps )
		return;

	const char *pszStepSound =
		GetOverrideStepSound( pPhysicsProps->GetString( nStepSound ) );
	CSoundParameters params;
	if ( !pszStepSound ||
		!CBaseEntity::GetParametersForSound( pszStepSound, params, NULL ) )
	{
		return;
	}

	m_Local.m_nStepside = !nSide;
	if ( m_nPlayerInfo & 0x40000 )
	{
		EmitSound( "Player.ButtWalk" );
		return;
	}

#ifndef CLIENT_DLL
	if ( IsFoFWalking() )
		return;
#endif

	const float flFoFVolume =
		params.volume * ( ( m_nPlayerInfo & 0x80 ) ? 0.25f : 0.20f );

#ifdef CLIENT_DLL
	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );
#else
	CSingleUserRecipientFilter filter( this );
	filter.UsePredictionRules();
#endif

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = flFoFVolume;
#ifdef CLIENT_DLL
	ep.m_SoundLevel = params.soundlevel;
#else
	ep.m_SoundLevel = SNDLVL_50dB;
#endif
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;
	EmitSound( filter, entindex(), ep );

#ifndef CLIENT_DLL
	// Only the owner predicts this step. Nearby listeners, including those
	// watching bots, still need the server's per-listener sound event.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pListener = UTIL_PlayerByIndex( i );
		if ( !pListener || pListener == this )
			continue;
		const Vector earPosition = pListener->EarPosition();
		const float distance = vecOrigin.DistTo( earPosition );
		if ( distance > 1800.0f )
			continue;
		const float loss = FoFFootstepOcclusion( vecOrigin, earPosition ) *
			( ( m_nPlayerInfo & 0x80 ) ? 0.85f : 1.0f );
		if ( loss >= 180.0f )
			continue;

		CSingleUserRecipientFilter listenerFilter( pListener );
		listenerFilter.UsePredictionRules();
		ep.m_flVolume = clamp( 1.0f - loss / 180.0f, 0.0f, 1.0f ) *
			( distance < 700.0f ? 1.0f : params.volume );
		ep.m_SoundLevel = SNDLVL_80dB;
		const float pitchOffset = distance > 100.0f ?
			( fabsf( vecOrigin.z - earPosition.z ) - 100.0f ) * ( -20.0f / 300.0f ) : 0.0f;
		ep.m_nPitch = clamp( params.pitch + static_cast< int >( pitchOffset ), 75, 100 );
		EmitSound( listenerFilter, entindex(), ep );
	}
#endif
	OnEmitFootstepSound( params, vecOrigin, flFoFVolume );

	// Equipment bit 0x80 adds the spur layer while running.  FoF suppresses
	// it while walking or while the sight transition is in its low range.
	const bool bWalking = IsFoFWalking();
	if ( ( m_nPlayerInfo & 0x80 ) && !bWalking &&
		m_flSightExpFactor < 0.25f )
	{
		EmitSound( "Player.Spurs" );
	}

	(void)flVolume;
	(void)bForce;
}

#ifndef CLIENT_DLL

void CFoF_Player::DeathSound( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll && m_hRagdoll->GetBaseAnimating()->IsDissolving() )
		return;

	// The original server suppresses electrocution deaths.
	if ( m_bitsDamageType & DMG_SHOCK )
		return;

	if ( m_bitsDamageType & DMG_FALL )
	{
		EmitSound( "Player.FallGib" );
		return;
	}

	if ( m_bIsBotGhost )
	{
		EmitSound( "Ghost.Death" );
		return;
	}

	const int nVoice = GetFoFVoiceStyle();

	const char *pszSound = "FoF.Death_p4";
	if ( nVoice == 6 )
		pszSound = "Zombie.Die";
	else if ( nVoice == 3 )
		pszSound = "FoF.Death";
	else if ( nVoice == 2 )
		pszSound = "FoF.Death_p2";
	else if ( nVoice == 4 )
		pszSound = "FoF.Death_p3";

	CSoundParameters params;
	const char *pszModelName = STRING( GetModelName() );
	if ( !GetParametersForSound( pszSound, params, pszModelName ) )
		return;

	Vector vecOrigin = GetAbsOrigin();
	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CFoF_Player *pRecipient = ToFoFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pRecipient && pRecipient != this &&
			!pRecipient->PlaysFoFTaunts() )
		{
			filter.RemoveRecipient( pRecipient );
		}
	}

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;
	EmitSound( filter, entindex(), ep );

	(void)info;
}

#endif
