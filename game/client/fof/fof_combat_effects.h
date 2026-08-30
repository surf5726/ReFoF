#ifndef FOF_COMBAT_EFFECTS_H
#define FOF_COMBAT_EFFECTS_H
#ifdef _WIN32
#pragma once
#endif

class C_BaseEntity;

const char *FoFSelectFireParticle(
	int nMode,
	const C_BaseEntity *pAttachedEntity );

class C_BasePlayer;

int FoFResolvePlayerTeamSleeveFrame( const C_BasePlayer *pPlayer );
int FoFResolveLocalSleeveFrame();
void FoFSetLocalSleeveFrame( int nSleeveFrame );
void FoFUpdateLocalSleeveFrame();

#include "basehandle.h"

enum FoFMuzzleParticleFamily_t
{
	FOF_MUZZLE_PARTICLE_REVOLVER = 0,
	FOF_MUZZLE_PARTICLE_SHOTGUN,
};

void FoFDispatchLegacyMuzzleParticle(
	FoFMuzzleParticleFamily_t nFamily,
	bool bFirstPerson,
	CBaseHandle hEntity,
	int nAttachmentIndex );

class C_BaseEntity;

bool FoFShouldSuppressPlayerGlow( C_BaseEntity *pEntity );

class C_BasePlayer;
class C_BaseCombatWeapon;
class CStudioHdr;

void FoFPresentMuzzleFlash(
	C_BasePlayer *pOwner,
	C_BaseCombatWeapon *pWeapon,
	bool bSecondViewModel );
void FoFCreateMuzzleSmoke( C_BasePlayer *pOwner, bool bSecondViewModel );

class C_BaseAnimating;

// Shared by the stick and belt variants. One per-entity throttle table keeps
// shadow/reflection DrawModel passes from multiplying the fuse particle rate.
void FoFDrawDynamiteFuseFX( C_BaseAnimating *pWeapon, bool bBlackDynamite );

#endif // FOF_COMBAT_EFFECTS_H
