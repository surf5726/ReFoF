//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterpart for FoF weapon crates.
//
//=============================================================================//
#ifndef C_FOF_ENTITIES_H
#define C_FOF_ENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"
#include "c_basecombatcharacter.h"

class bf_read;
class CGlowObject;
class IMaterial;

class C_FoF_Crate : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_FoF_Crate, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_FoF_Crate();
	virtual ~C_FoF_Crate();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();
	virtual void ReceiveMessage( int classID, bf_read &msg );
	virtual int DrawModel( int flags );
	virtual void GetRenderBounds( Vector &mins, Vector &maxs );
	virtual RenderGroup_t GetRenderGroup();

private:
	void DrawCircleOverlay(
		IMaterial *&pOffMaterial,
		IMaterial *&pOnMaterial,
		const char *pszOffMaterial,
		const char *pszOnMaterial,
		float flHeight,
		float flDiameter,
		float flProgress,
		const Color &offColor,
		const Color &onColor );
	virtual void CreateGlowEffect( int nType );
	virtual void DestroyGlowEffect();

	// The stock SDK base is eight bytes shorter than FoF's original base.
	// Preserve the recovered derived-member offsets used by RecvProps.
	unsigned char m_BaseLayoutPadding[8];
	float m_flMessageDuration;
	float m_flMessageStartTime;
	IMaterial *m_pOpenCrateOffMaterial;
	IMaterial *m_pOpenCrateOnMaterial;
	CGlowObject *m_pGlowEffect;
	IMaterial *m_pReloadOffMaterial;
	IMaterial *m_pReloadOnMaterial;
	float m_flNextRegen;
	float m_flTotalRegenTime;
	int m_nLastOverlayDrawFrame;
	bool m_bRegenGlowCreated;
};

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterparts for FoF world and objective entities.
//
//=============================================================================//

class CGlowObject;
class ConVar;
class IMaterial;

// These original client controls are also consumed by player and weapon
// presentation code.  Keep one definition and export it to those modules.
extern ConVar fof_smoke_trails;
extern ConVar fof_visual_quality;

class C_BaseGhost : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_BaseGhost, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_BaseGhost();

	virtual void ImpactTrace(
		trace_t *pTrace, int iDamageType, const char *pCustomImpactName );
	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;
	virtual bool IsPredicted() const;

private:
	int m_nGhostReserved;
};

class C_BBMulti : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_BBMulti, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_BBMulti();
	virtual ~C_BBMulti();

	virtual void Spawn();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();
	virtual int DrawModel( int flags );
	virtual void CreateGlowEffect();
	virtual void DestroyGlowEffect();

private:
	CGlowObject *m_pGlowEffect;
	bool m_bGlowCreated;
	bool m_bVisibleByAll;
};

class C_FoFCapEnt : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_FoFCapEnt, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_FoFCapEnt();

	virtual void Spawn();
	virtual int DrawModel( int flags );
	virtual void GetRenderBounds( Vector &mins, Vector &maxs );
	virtual void ComputeWorldSpaceSurroundingBox(
		Vector *pWorldMins, Vector *pWorldMaxs );
	virtual bool IsPredicted() const;

private:
	void DrawSafeZoneMesh();

	// The stock SDK base is twelve bytes shorter before the recovered material
	// pointer.  Preserve the original derived-member offsets used by RecvProps.
	unsigned char m_BaseLayoutPadding[12];
	IMaterial *m_pSafeZoneMaterial;
	int m_nClassFilter;
	int m_nAnnounceFilter;
	bool m_bCapActive;
	bool m_bBeingCaptured;
	float m_flCapProgress;
};

class C_FoFPushCart : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_FoFPushCart, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_FoFPushCart();
	virtual ~C_FoFPushCart();

	virtual void Spawn();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual bool IsPredicted() const;

private:
	virtual void CreateGlowEffect();
	virtual void DestroyGlowEffect();

	bool m_bEnabled;
	CGlowObject *m_pGlowEffect;
};

class C_FuncRespawnRoomVisualizer : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncRespawnRoomVisualizer, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;
};

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Client counterparts for FoF's thrown and fired world projectiles.
//
//=============================================================================//

struct FoFProjectilePresentation_t
{
	FoFProjectilePresentation_t();

	Vector m_vecLastOrigin;
	bool m_bUpdated;
};

class C_AxeBolt : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_AxeBolt, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink( void );
	virtual int DrawModel( int flags );
	virtual RenderGroup_t GetRenderGroup( void );

private:
	FoFProjectilePresentation_t m_Presentation;
};

class C_BowarrowBolt : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_BowarrowBolt, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink( void );
	virtual int DrawModel( int flags );
	virtual RenderGroup_t GetRenderGroup( void );

private:
	FoFProjectilePresentation_t m_Presentation;
};

class C_KnifeBolt : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_KnifeBolt, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink( void );
	virtual int DrawModel( int flags );
	virtual RenderGroup_t GetRenderGroup( void );

private:
	FoFProjectilePresentation_t m_Presentation;
};

class C_XArrow : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_XArrow, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink( void );
	virtual int DrawModel( int flags );
	virtual RenderGroup_t GetRenderGroup( void );

private:
	FoFProjectilePresentation_t m_Presentation;
};

#endif // C_FOF_ENTITIES_H
