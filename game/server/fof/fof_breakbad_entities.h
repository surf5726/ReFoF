//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Break Bad objective and purchase-zone entities.
//
//=============================================================================//
#ifndef FOF_BREAKBAD_ENTITIES_H
#define FOF_BREAKBAD_ENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"

class CFoF_Player;

class CBBMulti : public CBaseAnimating
{
public:
	DECLARE_CLASS( CBBMulti, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CBBMulti();
	virtual void Spawn();
	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );

	void TouchThink();

protected:
	// These are the only two virtuals added by CBBMulti in the shipped server.
	virtual void DeactivateZone();
	virtual void AffectZone();

	bool IsPlayerInsideZone( CFoF_Player *pPlayer );
	void SendMarker( CFoF_Player *pPlayer, bool bVisible,
		int nMarkerType, const char *pszNotice = NULL ) const;

	// Keep this order in sync with the shipped 0x4d4-byte class layout.
	float m_flNextWarningTime;
	int m_nRadius;
	trace_t m_ZoneTrace;
	float m_flExpireTime;
	int m_nMarkerLifetime;
	bool m_bAffectsAllPlayers;
	int m_nCashValue;
	CNetworkVar( bool, m_bVisibleByAll );
};

class CBBMultiBuyZone : public CBBMulti
{
public:
	DECLARE_CLASS( CBBMultiBuyZone, CBBMulti );

	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );

protected:
	virtual void DeactivateZone();
	virtual void AffectZone();
};

class CBBMultiBuySale : public CBBMulti
{
public:
	DECLARE_CLASS( CBBMultiBuySale, CBBMulti );

	virtual void Spawn();

protected:
	virtual void AffectZone();

private:
	int BuyTier();
	const char *CabinetModelName();
	const char *GlassModelName();
};

class CBBLootDropZone : public CBBMulti
{
public:
	DECLARE_CLASS( CBBLootDropZone, CBBMulti );

	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );
	virtual void Spawn();

	void SetCashValue( int nCashValue );
	void SetTeamSkin( int nTeam );

protected:
	virtual void DeactivateZone();
	virtual void AffectZone();
};

class CBBLootPickZone : public CBBMulti
{
public:
	DECLARE_CLASS( CBBLootPickZone, CBBMulti );

	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );

protected:
	virtual void DeactivateZone();
	virtual void AffectZone();
};

class CBBMultiDisarmZone : public CBBMulti
{
public:
	DECLARE_CLASS( CBBMultiDisarmZone, CBBMulti );

	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );
	virtual void Spawn();

protected:
	virtual void DeactivateZone();
	virtual void AffectZone();
};

class CBBMultiWhiskeyZone : public CBBMulti
{
public:
	DECLARE_CLASS( CBBMultiWhiskeyZone, CBBMulti );

	virtual void SetOwnerEntity( CBaseEntity *pOwnerEntity );
	virtual void Spawn();

protected:
	virtual void DeactivateZone();
	virtual void AffectZone();
};

#endif // FOF_BREAKBAD_ENTITIES_H
