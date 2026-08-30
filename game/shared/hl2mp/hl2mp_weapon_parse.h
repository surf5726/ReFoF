//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF weapon script data shared by the client and server.
//
//=============================================================================//

#ifndef HL2MP_WEAPON_PARSE_H
#define HL2MP_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_parse.h"
#include "networkvar.h"

class CHL2MPSWeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CHL2MPSWeaponInfo, FileWeaponInfo_t );

	CHL2MPSWeaponInfo();
	virtual void Parse(
		::KeyValues *pKeyValuesData, const char *szWeaponName );

	int m_iPlayerDamage;
	int m_iBreakLimit;
	int m_iBreakDropPenalty;
	float m_flFoFSpread[6];
	float m_flFoFSpreadSpeed;
	float m_flGunSmokeMult;
	Vector m_vecFoFExpOffset;
	QAngle m_angFoFExpOffset;
	Vector m_vecFoFExpOffsetFinal;
	QAngle m_angFoFExpOffsetFinal;
};

#endif // HL2MP_WEAPON_PARSE_H
