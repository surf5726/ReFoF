//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: FoF shared weapon-hand and dual-wield dispatch.
//
//=============================================================================//

#include "cbase.h"
#ifdef CLIENT_DLL
#include "iclientvehicle.h"
#else
#include "iservervehicle.h"
#include "recipientfilter.h"
#endif
#include "fof/fof_player_shared.h"
#include "fof/fof_player_weapons.h"
#include "fof/fof_weapon_properties.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

float CFoF_Player::BeginFoFDynamiteBeltThrow()
{
	const int nDenominator = MAX( 3, MIN( m_nFoFDynamiteBeltAttempts, 1000 ) );
	const float flAccuracy = static_cast< float >( m_nFoFDynamiteBeltHits ) /
		static_cast< float >( nDenominator );
	++m_nFoFDynamiteBeltAttempts;
	return flAccuracy;
}

bool FoFPotionLocksCurrentWeapons( CBasePlayer *pPlayer )
{
	CFoF_Player *pFoFPlayer = ToFoFPlayer( pPlayer );
	if ( !pFoFPlayer ||
		( pFoFPlayer->GetFoFPlayerInfo() & 0x40000 ) == 0 )
	{
		return false;
	}

	CBaseCombatWeapon *pFirst = pPlayer->GetActiveWeapon1();
	CBaseCombatWeapon *pSecond = pPlayer->GetActiveWeapon2();
	return ( pFirst &&
		!Q_stricmp( pFirst->GetClassname(), "weapon_peacemaker" ) ) ||
		( pSecond &&
		!Q_stricmp( pSecond->GetClassname(), "weapon_peacemaker2" ) );
}

bool FoFPotionBlocksWeaponSelection(
	CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon || !FoFPotionLocksCurrentWeapons( pPlayer ) )
		return false;

	return pWeapon != pPlayer->GetActiveWeapon1() &&
		pWeapon != pPlayer->GetActiveWeapon2();
}

bool FoFSelectExactWeapon(
	CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon )
		return false;

	// During the potion reward, the current dual Peacemakers own both hands.
	// Consume attempts to toggle either leaf so middle-button hand switching
	// cannot holster one side and leave the player with no active weapon.
	if ( FoFPotionLocksCurrentWeapons( pPlayer ) &&
		( pWeapon == pPlayer->GetActiveWeapon1() ||
		  pWeapon == pPlayer->GetActiveWeapon2() ) )
	{
		return true;
	}

	// The stock prediction path selects ordinary weapons by classname through
	// SelectItem.  Only FoF's paired revolver entities need entity-exact
	// dispatch; applying this lookup to every duplicated classname can swallow
	// a failed switch and leave the client on the previous weapon until the
	// server correction arrives.
	if ( !FoFIsRevolverWeapon( pWeapon ) )
		return false;

	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pOwned = pPlayer->GetWeapon( i );
		if ( !pOwned || pOwned == pWeapon )
			continue;

		if ( Q_stricmp( pOwned->GetName(), pWeapon->GetName() ) == 0 &&
			pOwned->GetSubType() == pWeapon->GetSubType() )
		{
			// The usercmd identifies the exact hand leaf.  If both leaves are
			// active, selecting either one again toggles that exact entity.
			// Returning Weapon_Switch's false result here used to fall through
			// to SelectItem(classname); duplicate classnames then always chose
			// the first/right leaf, so a left-hand toggle swapped both
			// viewmodels and idle clocks until the server correction arrived.
			if ( pPlayer->HasDualActiveWeapons() &&
				( pPlayer->GetActiveWeapon1() == pWeapon ||
				  pPlayer->GetActiveWeapon2() == pWeapon ) )
			{
				pWeapon->Holster( NULL );
				return true;
			}

			const int nViewModelIndex =
				pWeapon->IsSecondGun() ? 1 : 0;
			pPlayer->Weapon_Switch( pWeapon, nViewModelIndex );
			// Exact duplicate-entity dispatch owns this command even when the
			// requested switch is rejected.  A classname fallback cannot select
			// the intended hand safely.
			return true;
		}
	}

	return false;
}

void CFoF_Player::RecalculateWeaponSpeed()
{
	CBaseCombatWeapon *pFirst = GetActiveWeapon1();
	CBaseCombatWeapon *pSecond = GetActiveWeapon2();
	if ( !pFirst && !pSecond )
	{
		if ( IsOnFoFHorse() )
			SetMaxSpeed( 150.0f );
		return;
	}

	m_flTransitionSpeed = 2.0f;
	CBaseCombatWeapon *pActive = pFirst ? pFirst : pSecond;
	const char *pszName = pActive->GetName();
	float flBaseSpeed = 200.0f;

	if ( pszName && !Q_stricmp( pszName, "weapon_knife" ) )
	{
		flBaseSpeed = 235.0f;
	}
	else if ( pszName &&
		( !Q_stricmp( pszName, "weapon_axe" ) ||
		  !Q_stricmp( pszName, "weapon_machete" ) ) )
	{
		flBaseSpeed = 255.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_fists" ) )
	{
		flBaseSpeed = 235.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_fists_ghost" ) )
	{
		flBaseSpeed = 120.0f;
	}
	else if ( FoFIsRevolverWeapon( pActive ) )
	{
		int nActiveWeight = pFirst ?
			pFirst->FoFWeaponWeight() : pActive->FoFWeaponWeight();
		if ( pFirst && pSecond )
			nActiveWeight += pSecond->FoFWeaponWeight();
		m_flTransitionSpeed = RemapValClamped(
			static_cast< float >( nActiveWeight ),
			1.0f, 6.0f, 2.25f, 1.1f );
		flBaseSpeed = 220.0f;
	}
	else if ( pszName &&
		( !Q_stricmp( pszName, "weapon_dynamite" ) ||
		  !Q_stricmp( pszName, "weapon_dynamite_black" ) ) )
	{
		flBaseSpeed = 200.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_dynamite_belt" ) )
	{
		flBaseSpeed = 220.0f;
	}
	else if ( pszName &&
		( !Q_stricmp( pszName, "weapon_dualnavy" ) ||
		  !Q_stricmp( pszName, "weapon_dualpeacemaker" ) ) )
	{
		flBaseSpeed = 200.0f;
	}
	else if ( pszName &&
		( !Q_stricmp( pszName, "weapon_carbine" ) ||
		  !Q_stricmp( pszName, "weapon_coachgun" ) ) )
	{
		flBaseSpeed = 200.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_shotgun" ) )
	{
		flBaseSpeed = 220.0f;
	}
	else if ( pszName &&
		( !Q_stricmp( pszName, "weapon_henryrifle" ) ||
		  !Q_stricmp( pszName, "weapon_spencer" ) ) )
	{
		flBaseSpeed = 200.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_sharps" ) )
	{
		flBaseSpeed = 180.0f;
	}
	else if ( pszName && !Q_stricmp( pszName, "weapon_bow" ) )
	{
		flBaseSpeed = 220.0f;
	}

	const float flLoadFactor = GetFoFLoadFactor();
#ifndef CLIENT_DLL
	if ( !IsOnFoFHorse() && flLoadFactor < 0.85f &&
		gpGlobals->curtime > m_flNextPickupInteraction )
	{
		char szLoadPercent[32];
		Q_snprintf( szLoadPercent, sizeof( szLoadPercent ), "%i",
			RoundFloatToInt( RemapValClamped(
				flLoadFactor, 1.0f, 0.5f, 0.0f, 100.0f ) ) );
		CSingleUserRecipientFilter filter( this );
		filter.MakeReliable();
		UserMessageBegin( filter, "BBNotices" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "#FoF_Overweight2" );
			WRITE_STRING( szLoadPercent );
		MessageEnd();
		m_flNextPickupInteraction = gpGlobals->curtime + 40.0f;
	}
#endif
	if ( IsOnFoFHorse() )
	{
		SetMaxSpeed( RemapValClamped(
			flLoadFactor, 1.0f, 0.5f, 1.0f, 0.85f ) * 400.0f );
	}
	else
	{
		SetMaxSpeed( clamp( flBaseSpeed * flLoadFactor, 145.0f, 300.0f ) );
	}
}

float CFoF_Player::GetFoFLoadFactor() const
{
	int nTotalWeight = 0;
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( pWeapon )
			nTotalWeight += pWeapon->FoFWeaponWeight();
	}

	static ConVarRef battleRoyale( "fof_sv_battle_royale", true );
	const float flMaxCarryWeight =
		battleRoyale.IsValid() && battleRoyale.GetBool() ? 15.0f : 20.0f;
	return RemapValClamped(
		static_cast< float >( nTotalWeight ),
		10.0f, flMaxCarryWeight, 1.0f, 0.5f );
}
