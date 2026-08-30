//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-side FoF map items.
//
//=============================================================================//
#ifndef FOF_ITEM_ENTITIES_H
#define FOF_ITEM_ENTITIES_H
#ifdef _WIN32
#pragma once
#endif

#include "items.h"

class CItem_FoFResupply : public CItem
{
public:
	DECLARE_CLASS( CItem_FoFResupply, CItem );
	DECLARE_DATADESC();

	CItem_FoFResupply();

	virtual void Spawn();
	virtual void Precache();
	virtual CBaseEntity *Respawn();
	virtual bool MyTouch( CBasePlayer *pPlayer );

private:
	int m_nHealth;
};

class CItem_FoFPotion : public CItem
{
public:
	DECLARE_CLASS( CItem_FoFPotion, CItem );
	DECLARE_DATADESC();

	CItem_FoFPotion();

	virtual void Spawn();
	virtual void Precache();
	virtual CBaseEntity *Respawn();
	virtual bool MyTouch( CBasePlayer *pPlayer );

private:
	int m_nPotion;
};

class CItem_FoFSkull : public CItem
{
public:
	DECLARE_CLASS( CItem_FoFSkull, CItem );

	virtual void Spawn();
	virtual void Precache();
	virtual bool MyTouch( CBasePlayer *pPlayer );
};

class CItem_XBowSpawn : public CItem
{
public:
	DECLARE_CLASS( CItem_XBowSpawn, CItem );

	CItem_XBowSpawn();

	virtual void Spawn();
	virtual void Precache();
	virtual bool MyTouch( CBasePlayer *pPlayer );

private:
	float m_flNextWrongTeamNotice;
};

#endif // FOF_ITEM_ENTITIES_H
