//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF weapon script parsing.
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "hl2mp_weapon_parse.h"

FileWeaponInfo_t *CreateWeaponInfo()
{
	return new CHL2MPSWeaponInfo;
}

CHL2MPSWeaponInfo::CHL2MPSWeaponInfo()
{
	m_iPlayerDamage = 0;
	m_iBreakLimit = 0;
	m_iBreakDropPenalty = 0;
	Q_memset( m_flFoFSpread, 0, sizeof( m_flFoFSpread ) );
	m_flFoFSpread[1] = 0.02f;
	m_flFoFSpread[2] = 0.05f;
	m_flFoFSpread[3] = 0.10f;
	m_flFoFSpread[4] = 0.20f;
	m_flFoFSpread[5] = 0.35f;
	m_flFoFSpreadSpeed = 1.0f;
	m_flGunSmokeMult = 1.0f;
	m_vecFoFExpOffset.Init();
	m_angFoFExpOffset.Init();
	m_vecFoFExpOffsetFinal.Init();
	m_angFoFExpOffsetFinal.Init();
}

void CHL2MPSWeaponInfo::Parse(
	KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

#if defined( GAME_DLL )
	if ( Q_strstr( szWeaponName, "2" ) )
	{
		char szSecondWorldModel[MAX_WEAPON_STRING];
		Q_strncpy( szSecondWorldModel, szWorldModel,
			sizeof( szSecondWorldModel ) );

		char *pszExtension = Q_strstr( szSecondWorldModel, ".mdl" );
		if ( pszExtension )
			*pszExtension = '\0';

		Q_strncat( szSecondWorldModel, "2.mdl",
			sizeof( szSecondWorldModel ), COPY_ALL_CHARACTERS );
		Q_strncpy( szWorldModel, szSecondWorldModel,
			sizeof( szWorldModel ) );
		iSlot = 1;
		m_bBuiltRightHanded = false;
		m_bAllowFlipping = true;
	}
#endif

	m_iPlayerDamage = pKeyValuesData->GetInt( "damage", 0 );
	m_iBreakLimit = pKeyValuesData->GetInt( "break_limit", 20 );
	m_iBreakDropPenalty =
		pKeyValuesData->GetInt( "break_drop_penalty", 6 );
	m_flGunSmokeMult =
		pKeyValuesData->GetFloat( "GunSmokeMult", 1.0f );

	KeyValues *pExpOffset = pKeyValuesData->FindKey( "ExpOffset" );
	if ( pExpOffset )
	{
		m_vecFoFExpOffset.x = pExpOffset->GetFloat( "x", 0.0f );
		m_vecFoFExpOffset.y = pExpOffset->GetFloat( "y", 0.0f );
		m_vecFoFExpOffset.z = pExpOffset->GetFloat( "z", 0.0f );
		m_angFoFExpOffset.x = pExpOffset->GetFloat( "xori", 0.0f );
		m_angFoFExpOffset.y = pExpOffset->GetFloat( "yori", 0.0f );
		m_angFoFExpOffset.z = pExpOffset->GetFloat( "zori", 0.0f );
		m_vecFoFExpOffsetFinal.x = pExpOffset->GetFloat( "xf", 0.0f );
		m_vecFoFExpOffsetFinal.y = pExpOffset->GetFloat( "yf", 0.0f );
		m_vecFoFExpOffsetFinal.z = pExpOffset->GetFloat( "zf", 0.0f );
		m_angFoFExpOffsetFinal.x = pExpOffset->GetFloat( "xorif", 0.0f );
		m_angFoFExpOffsetFinal.y = pExpOffset->GetFloat( "yorif", 0.0f );
		m_angFoFExpOffsetFinal.z = pExpOffset->GetFloat( "zorif", 0.0f );
	}

	KeyValues *pSpread = pKeyValuesData->FindKey( "Spread" );
	if ( pSpread )
	{
		m_flFoFSpread[1] = pSpread->GetFloat( "crouch", 0.02f );
		m_flFoFSpread[2] = pSpread->GetFloat( "idle", 0.05f );
		m_flFoFSpread[3] = pSpread->GetFloat( "walk", 0.10f );
		m_flFoFSpread[4] = pSpread->GetFloat( "run", 0.20f );
		m_flFoFSpread[5] = pSpread->GetFloat( "jump", 0.35f );
		m_flFoFSpreadSpeed = pSpread->GetFloat( "speed", 1.0f );
	}
}
