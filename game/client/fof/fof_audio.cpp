#include "cbase.h"
#include "fof/fof_audio.h"
#include "basecombatweapon_shared.h"
#include "c_recipientfilter.h"
#include "physics_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

float FoFSanitizeSoundscapeVolume( float flVolume )
{
	if ( !IsFinite( flVolume ) )
		return 0.0f;

	return clamp( flVolume, 0.0f, 1.0f );
}

interval_t FoFReadSoundscapeVolumeInterval( const char *pszValue )
{
	interval_t volume = ReadInterval( pszValue );
	float flStart = volume.start;
	float flEnd = volume.start + volume.range;

	// The shipped Presidio soundscape contains "0.7, 9.0", while all other
	// volume intervals use the engine's 0..1 scale.  Recover the evident 0.9
	// endpoint from this decimal-point typo without changing valid intervals.
	if ( flStart >= 0.0f && flStart <= 1.0f &&
		flEnd > 1.0f && flEnd <= 10.0f )
	{
		const float flDecimalEnd = flEnd * 0.1f;
		if ( flDecimalEnd >= flStart )
			flEnd = flDecimalEnd;
	}

	flStart = FoFSanitizeSoundscapeVolume( flStart );
	flEnd = FoFSanitizeSoundscapeVolume( flEnd );
	if ( flEnd < flStart )
		flEnd = flStart;

	volume.start = flStart;
	volume.range = flEnd - flStart;
	return volume;
}

static ConVar fof_gunshot_wallocclusion(
	"fof_gunshot_wallocclusion", "1", FCVAR_REPLICATED,
	"Attenuate gunshots as they pass through walls." );

static float FoFWeaponSoundMaterialScale( const trace_t &trace )
{
	if ( !physprops )
		return 1.0f;

	const surfacedata_t *pSurface =
		physprops->GetSurfaceData( trace.surface.surfaceProps );
	if ( !pSurface )
		return 1.0f;

	switch ( pSurface->game.material )
	{
	case 'C':
		return 1.3f;
	case 'G':
		return 0.2f;
	case 'M':
		return 1.5f;
	case 'P':
		return 0.9f;
	case 'W':
		return 0.7f;
	default:
		return 1.0f;
	}
}

static float FoFWeaponSoundWallOcclusion( const Vector &vecSource,
	const Vector &vecListener )
{
	Vector vecTraceStart = vecSource;
	const Vector vecTraceDelta = vecListener - vecSource;
	float flOcclusion = 0.0f;

	for ( int i = 0; i < 6; ++i )
	{
		trace_t forwardTrace;
		UTIL_TraceLine( vecTraceStart, vecListener, MASK_SOLID_BRUSHONLY,
			NULL, COLLISION_GROUP_NONE, &forwardTrace );
		if ( forwardTrace.fraction >= 1.0f )
			break;

		trace_t reverseTrace;
		UTIL_TraceLine( vecListener, forwardTrace.endpos,
			MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &reverseTrace );

		const float flThickness = clamp(
			( forwardTrace.endpos - reverseTrace.endpos ).Length(),
			0.0f, 20.0f );
		flOcclusion += flThickness *
			FoFWeaponSoundMaterialScale( forwardTrace ) * 0.35f;

		if ( flOcclusion > 200.0f )
			return 250.0f;

		// The shipped client advances by the original source-to-listener
		// displacement after locating the reverse side of the wall.
		vecTraceStart = reverseTrace.endpos + vecTraceDelta;
	}

	return flOcclusion;
}

bool FoFEmitOccludedWeaponSound( C_BaseCombatWeapon *pWeapon,
	const CSoundParameters &params )
{
	if ( !pWeapon || !fof_gunshot_wallocclusion.GetBool() )
		return false;

	C_BaseEntity *pOwner = pWeapon->GetOwner();
	if ( !pOwner )
		return false;

	const Vector vecSource = pOwner->EyePosition();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *pListener = UTIL_PlayerByIndex( i );
		if ( !pListener || pListener == pOwner || pListener->IsDormant() )
			continue;

		const float flOcclusion = FoFWeaponSoundWallOcclusion(
			vecSource, pListener->EarPosition() );
		const float flVolumeScale = clamp(
			1.0f - flOcclusion / 250.0f, 0.35f, 1.0f );

		CSingleUserRecipientFilter filter( pListener );
		if ( pWeapon->IsPredicted() && C_BaseEntity::GetPredictionPlayer() )
			filter.UsePredictionRules();

		EmitSound_t sound;
		sound.m_nChannel = CHAN_BODY;
		sound.m_pSoundName = params.soundname;
		sound.m_flVolume = params.volume * flVolumeScale;
		sound.m_SoundLevel = params.soundlevel;
		sound.m_nPitch = params.pitch;
		sound.m_pOrigin = &pOwner->GetAbsOrigin();
		C_BaseEntity::EmitSound( filter, pWeapon->entindex(), sound );
	}

	return true;
}

