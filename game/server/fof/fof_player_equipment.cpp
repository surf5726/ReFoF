//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-authoritative equipment catalogue and grant helpers.
//
//=============================================================================//
#include "cbase.h"
#include "fof/fof_player_equipment.h"
#include "fof/fof_item_catalog.h"
#include "fof/fof_player.h"
#include "fof/fof_player_shared.h"
#include "player.h"
#include <stdlib.h>
#include <string.h>
#include "hl2mp_gamerules.h"
#include "baseviewmodel.h"
#include "fof/fof_course_mode.h"
#include "fof/fof_elimination_mode.h"
#include "fof/fof_breakbad_mode.h"
#include "fof/fof_weapon_properties.h"
#include "in_buttons.h"
#include "fof/fof_horse.h"
#include "fof/fof_item_entities.h"
#include "fof/fof_player_damage.h"
#include "fof/fof_projectiles.h"
#include "items.h"
#include "physics.h"
#include "player_pickup.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void FoFTrimEquipmentToken( char *pszToken )
{
	if ( !pszToken )
		return;

	char *pStart = pszToken;
	while ( *pStart == ' ' || *pStart == '\t' ||
		*pStart == '\r' || *pStart == '\n' )
	{
		++pStart;
	}
	if ( pStart != pszToken )
		memmove( pszToken, pStart, Q_strlen( pStart ) + 1 );

	int nLength = Q_strlen( pszToken );
	while ( nLength > 0 )
	{
		const char last = pszToken[nLength - 1];
		if ( last != ' ' && last != '\t' && last != '\r' && last != '\n' )
			break;
		pszToken[--nLength] = '\0';
	}
}

static bool FoFNextEquipmentToken(
	const char *&pCursor, char *pszToken, int nTokenSize )
{
	if ( !pCursor || !*pCursor || !pszToken || nTokenSize <= 0 )
		return false;

	int nWritten = 0;
	while ( *pCursor && *pCursor != ',' )
	{
		if ( nWritten + 1 < nTokenSize )
			pszToken[nWritten++] = *pCursor;
		++pCursor;
	}
	pszToken[nWritten] = '\0';
	if ( *pCursor == ',' )
		++pCursor;
	FoFTrimEquipmentToken( pszToken );
	return true;
}

int FoFEquipmentItemId( const char *pszToken )
{
	if ( !pszToken || !pszToken[0] )
		return -1;

	char *pEnd = NULL;
	const long nNumeric = strtol( pszToken, &pEnd, 10 );
	if ( pEnd != pszToken && pEnd && *pEnd == '\0' )
		return static_cast< int >( nNumeric );

	if ( !Q_stricmp( pszToken, "gun_throw" ) ||
		!Q_stricmp( pszToken, "skill_throw" ) )
		return 14;
	if ( !Q_stricmp( pszToken, "walljump" ) ||
		!Q_stricmp( pszToken, "wall jump" ) )
		return 15;
	if ( !Q_stricmp( pszToken, "boots" ) )
		return 20;
	if ( !Q_stricmp( pszToken, "slide" ) )
		return 23;
	if ( !Q_stricmp( pszToken, "heavyload" ) ||
		!Q_stricmp( pszToken, "heavy_load" ) )
		return 25;
	if ( !Q_stricmp( pszToken, "brass_knuckles" ) )
		return 34;
	if ( !Q_stricmp( pszToken, "skill_right" ) )
		return 41;
	if ( !Q_stricmp( pszToken, "skill_left" ) )
		return 42;
	if ( !Q_stricmp( pszToken, "skill_ambi" ) )
		return 43;
	if ( !Q_stricmp( pszToken, "skill_fan" ) )
		return 44;

	const char *pszLookup = pszToken;
	if ( !Q_stricmp( pszLookup, "navy" ) )
		pszLookup = "coltnavy";
	else if ( !Q_stricmp( pszLookup, "yellowboy" ) )
		pszLookup = "henryrifle";

	const FoFItemDefinition_t *pItem =
		FoFFindItemDefinitionByToken( pszLookup );
	return pItem ? pItem->m_nItem : -1;
}

static int FoFCalculateInitialWeaponWear(
	CFoF_Player *pPlayer, CBaseHL2MPCombatWeapon *pWeapon,
	float flPerformanceScale, float flKillsPerMinuteMax,
	float flThresholdMin, float flThresholdMax )
{
	const float flLifeMinutes =
		( gpGlobals->curtime - pPlayer->GetFoFLifeStartTime() ) *
		( 1.0f / 60.0f );
	const float flKillsPerMinute = flLifeMinutes > 0.0f ?
		static_cast< float >( pPlayer->FragCount() ) / flLifeMinutes : 0.0f;
	const float flPerformance =
		RemapValClamped( flKillsPerMinute,
			1.0f, flKillsPerMinuteMax, 0.0f, flPerformanceScale ) +
		RemapValClamped( pPlayer->GetFoFReportedAccuracy(),
			0.25f, 0.85f, 0.0f, flPerformanceScale );
	const float flThreshold = random->RandomFloat(
		flThresholdMin, flThresholdMax );
	const int nBreakLimit = pWeapon->GetHL2MPWpnData().m_iBreakLimit;
	return RoundFloatToInt( RemapValClamped(
		flPerformance, 1.0f, flThreshold,
		0.0f, static_cast< float >( nBreakLimit ) ) );
}

static void FoFInitializeWeaponDropState(
	CFoF_Player *pPlayer, CBaseHL2MPCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon )
		return;

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	static ConVarRef weaponDrop( "fof_sv_weapon_drop", true );
	const int nMode = currentMode.IsValid() ? currentMode.GetInt() : 1;
	const bool bClassicShootout = classicShootout.IsValid() &&
		classicShootout.GetBool();
	const int nTier = FoFWeaponTier( pWeapon );

	// Event_Killed drops inventory candidates whose wear is below break_limit,
	// so initial wear determines which carried weapons survive as drops.
	if ( nMode == 1 || nMode == 3 )
	{
		int nWear = FoFCalculateInitialWeaponWear(
			pPlayer, pWeapon, 0.75f, 5.0f, 0.57f, 0.70f );

		// The premium bow receives an additional randomized age in Shootout
		// when the player's performance factor is below 0.8.  Recompute the
		// same factor because the helper above deliberately returns only wear.
		if ( nMode == 1 && pWeapon->FoFWeaponID() == 1 &&
			FClassnameIs( pWeapon, "weapon_bow_black" ) )
		{
			const float flLifeMinutes =
				( gpGlobals->curtime - pPlayer->GetFoFLifeStartTime() ) *
				( 1.0f / 60.0f );
			const float flKillsPerMinute = flLifeMinutes > 0.0f ?
				static_cast< float >( pPlayer->FragCount() ) /
					flLifeMinutes : 0.0f;
			const float flPerformance =
				RemapValClamped( flKillsPerMinute,
					1.0f, 5.0f, 0.0f, 0.75f ) +
				RemapValClamped( pPlayer->GetFoFReportedAccuracy(),
					0.25f, 0.85f, 0.0f, 0.75f );
			if ( flPerformance < 0.8f )
			{
				nWear += RoundFloatToInt(
					pWeapon->GetHL2MPWpnData().m_iBreakLimit *
					random->RandomFloat( 0.35f, 0.65f ) );
			}
		}

		pWeapon->SetFoFShotCounter( nWear );
		if ( pWeapon->FoFWeaponID() == 3 )
		{
			pWeapon->SetFoFShotCounter(
				pWeapon->GetHL2MPWpnData().m_iBreakLimit );
		}
	}
	else if ( bClassicShootout )
	{
		pWeapon->SetFoFShotCounter( 0 );
	}
	else
	{
		const int nDropMode = weaponDrop.IsValid() ?
			weaponDrop.GetInt() : 0;
		if ( nDropMode == 0 )
		{
			if ( nTier > 1 )
				pWeapon->SetFoFShotCounter( 99 );
		}
		else if ( nDropMode == 1 )
		{
			pWeapon->SetFoFShotCounter( -1000 );
		}
		else if ( nDropMode == 3 )
		{
			pWeapon->SetFoFShotCounter( 100 );
		}
		else
		{
			pWeapon->SetFoFShotCounter(
				FoFCalculateInitialWeaponWear(
					pPlayer, pWeapon, 0.45f, 3.0f,
					0.10f, 0.15f ) );
		}
	}

	if ( nTier > 1 && bClassicShootout )
		pWeapon->SetFoFShotCounter( -10000 );
}

static bool FoFEquipGrantedCourseBotWeapon(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon );

CBaseEntity *CFoF_Player::GiveFoFNamedItem(
	const char *pszName, int iSubType )
{
	EHANDLE hEntity = CreateEntityByName( pszName );
	if ( !hEntity )
	{
		Msg( "NULL Ent in GiveNamedItem!\n" );
		return NULL;
	}

	CBaseEntity *pEntity = hEntity.Get();
	pEntity->SetLocalOrigin( GetLocalOrigin() );
	pEntity->AddSpawnFlags( SF_NORESPAWN );
	CBaseHL2MPCombatWeapon *pWeapon =
		dynamic_cast< CBaseHL2MPCombatWeapon * >( hEntity.Get() );
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	const bool bDirectCourseBotGrant = pWeapon && IsBot() &&
		currentMode.IsValid() && currentMode.GetInt() == 6;
	if ( bDirectCourseBotGrant )
	{
		pWeapon->SetOwner( this );
		pWeapon->SetOwnerEntity( this );
	}
	DispatchSpawn( pEntity );
	if ( !hEntity || hEntity->IsMarkedForDeletion() )
		return NULL;

	if ( pWeapon )
	{
		pWeapon->SetSubType( iSubType );
		FoFInitializeWeaponDropState( this, pWeapon );
	}

	if ( bDirectCourseBotGrant )
	{
		pWeapon->SetOwner( NULL );
		pWeapon->SetOwnerEntity( NULL );
		if ( !FoFEquipGrantedCourseBotWeapon( this, pWeapon ) )
			UTIL_Remove( pWeapon );
	}
	else if ( hEntity && !hEntity->IsMarkedForDeletion() )
	{
		hEntity->Touch( this );
	}

	// Black dynamite is granted as a three-stick ammo pool.  The weapon's
	// default clip contributes the first stick; GiveAmmo clamps the total to
	// the shipped Dynamite_B carry limit.
	if ( !Q_stricmp( pszName, "weapon_dynamite_black" ) )
		CBasePlayer::GiveAmmo( 3, "Dynamite_B", true );
	return hEntity.Get();
}

static void FoFGiveCatalogWeapon(
	CFoF_Player *pPlayer, const FoFItemDefinition_t *pItem )
{
	if ( !pPlayer || !pItem || !pItem->m_pszClassname )
		return;

	const char *pszClassname = pItem->m_pszClassname;
	if ( pItem->m_pszOppositeHandClassname &&
		pPlayer->GetFoFHandStance() == 2 )
	{
		pszClassname = pItem->m_pszOppositeHandClassname;
	}
	pPlayer->GiveFoFNamedItem( pszClassname );
}

void FoFApplyEquipmentItem( CFoF_Player *pPlayer, int nItem )
{
	if ( !pPlayer || nItem < 0 )
		return;

	switch ( nItem )
	{
	case 14:
		pPlayer->m_nPlayerInfo |= 0x40;
		break;
	case 15:
		pPlayer->m_nPlayerInfo |= 0x200000;
		break;
	case 20:
		pPlayer->m_nPlayerInfo |= 0x80;
		break;
	case 23:
		pPlayer->m_nPlayerInfo |= 0x100000;
		break;
	case 25:
		pPlayer->m_nPlayerInfo |= 0x800000;
		break;
	case 34:
		pPlayer->m_nPlayerInfo |= 0x4;
		break;
	case 41:
		pPlayer->m_nHandStance = 1;
		if ( !pPlayer->HasDualActiveWeapons() )
		{
			CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon1();
			if ( !pWeapon )
				pWeapon = pPlayer->GetActiveWeapon2();
			if ( pWeapon && pWeapon->CanDualWield() )
				pPlayer->m_nPlayerInfo |= 0x10;
		}
		break;
	case 42:
		pPlayer->m_nHandStance = 2;
		break;
	case 43:
		pPlayer->m_nHandStance = 0;
		break;
	case 44:
		pPlayer->m_nHandStance = 3;
		break;
	default:
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( nItem );
		if ( !pItem || !pItem->m_pszClassname )
			break;

		if ( nItem == 17 )
			pPlayer->m_nPlayerInfo |= 0x80000;

		// Throwable knives are a three-item bundle. Sidearms remain one item per
		// catalogue entry; a repeated entry is resolved to the other physical
		// hand by the pickup path.
		if ( nItem == 1 )
		{
			FoFGiveCatalogWeapon( pPlayer, pItem );
			FoFGiveCatalogWeapon( pPlayer, pItem );
			FoFGiveCatalogWeapon( pPlayer, pItem );
			break;
		}

		FoFGiveCatalogWeapon( pPlayer, pItem );
		break;
	}
}

void FoFSendEquipmentItem( CFoF_Player *pPlayer, int nItem )
{
	if ( !pPlayer || pPlayer->IsBot() )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "HudEquipItems" );
		WRITE_SHORT( nItem );
	MessageEnd();
}

void FoFGiveEquipmentList(
	CFoF_Player *pPlayer, const char *pszItems, bool bSendHud,
	bool bApplyInReverse )
{
	if ( !pPlayer || !pszItems )
		return;

	CUtlVector< int > items;
	const char *pCursor = pszItems;
	char szToken[64];
	while ( FoFNextEquipmentToken(
		pCursor, szToken, sizeof( szToken ) ) )
	{
		if ( !szToken[0] )
			continue;
		// FoF bot profiles use -1 for every unused equipment slot.  It is
		// an empty-slot sentinel, not an item ID or malformed token.
		if ( !Q_stricmp( szToken, "-1" ) )
			continue;

		const int nItem = FoFEquipmentItemId( szToken );
		if ( nItem < 0 )
		{
			Warning( "Unknown FoF equipment token '%s'.\n", szToken );
			continue;
		}

		items.AddToTail( nItem );
		if ( bSendHud )
			FoFSendEquipmentItem( pPlayer, nItem );
	}

	if ( bApplyInReverse )
	{
		for ( int i = items.Count() - 1; i >= 0; --i )
			FoFApplyEquipmentItem( pPlayer, items[i] );
	}
	else
	{
		for ( int i = 0; i < items.Count(); ++i )
			FoFApplyEquipmentItem( pPlayer, items[i] );
	}
}

bool FoFGetTeamClassDefinition(
	int nClass, char *pszName, int nNameSize, float &flShare )
{
	if ( nClass < 0 || nClass >= 8 || !pszName || nNameSize <= 0 )
		return false;

	pszName[0] = '\0';
	flShare = 0.0f;

	char szConVar[32];
	Q_snprintf( szConVar, sizeof( szConVar ),
		"fof_sv_tp_classes_c%d", nClass );
	ConVarRef definition( szConVar, true );
	if ( !definition.IsValid() )
		return false;

	const char *pCursor = definition.GetString();
	char szToken[64];
	if ( !FoFNextEquipmentToken(
		pCursor, szToken, sizeof( szToken ) ) ||
		!szToken[0] || !Q_stricmp( szToken, "empty" ) ||
		!Q_stricmp( szToken, "0" ) )
	{
		return false;
	}

	Q_strncpy( pszName, szToken, nNameSize );
	if ( FoFNextEquipmentToken(
		pCursor, szToken, sizeof( szToken ) ) )
	{
		flShare = Q_atof( szToken );
	}
	return true;
}

void FoFApplyTeamClass( CFoF_Player *pPlayer, int nClass )
{
	if ( !pPlayer || nClass < 0 || nClass >= 8 )
		return;

	char szConVar[32];
	Q_snprintf( szConVar, sizeof( szConVar ),
		"fof_sv_tp_classes_c%d", nClass );
	ConVarRef definition( szConVar, true );
	if ( !definition.IsValid() )
		return;

	const char *pCursor = definition.GetString();
	char szToken[64];
	for ( int nToken = 0; nToken < 8 &&
		FoFNextEquipmentToken(
			pCursor, szToken, sizeof( szToken ) ); ++nToken )
	{
		// The first two fields are the display name and class share.  FoF
		// applies only fields 3..8, giving every class at most six items.
		if ( nToken < 2 || !szToken[0] )
			continue;

		const int nItem = FoFEquipmentItemId( szToken );
		if ( nItem >= 0 )
			FoFApplyEquipmentItem( pPlayer, nItem );
	}
}

void FoFResetEquipmentState( CFoF_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	// These are the passive item bits represented in the equipment catalogue.
	pPlayer->m_nPlayerInfo &=
		~( 0x4 | 0x40 | 0x80 | 0x80000 |
			0x100000 | 0x200000 | 0x800000 );
	pPlayer->ClearFoFEquipmentFlags();
	pPlayer->m_nHandStance = 0;
}

// Server authority for Shootout equipment and cash-loadout transactions.

enum FoFPurchaseState_t
{
	FOF_PURCHASE_NONE = 0,
	FOF_PURCHASE_REUSE_SAVED = 1,
	FOF_PURCHASE_REPLACE_SAVED = 2,
	FOF_PURCHASE_APPEND_SAVED = 3,
};

static int FoFPurchaseCurrentMode()
{
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	return currentMode.IsValid() ? currentMode.GetInt() : 1;
}

static bool FoFUsesDeathmatchEquipment( int nMode )
{
	return nMode == 1 || nMode == 4;
}

static bool FoFSelectedEquipmentAllowsRestrictedItems(
	const CFoF_Player *pPlayer, int nMode )
{
	if ( !pPlayer )
		return false;

	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	return pPlayer->IsBot() || nMode == 5 || nMode == 6 ||
		( forceWeapons.IsValid() && forceWeapons.GetBool() ) ||
		( teamClasses.IsValid() && teamClasses.GetBool() );
}

static bool FoFSelectedEquipmentItemAllowed(
	const CFoF_Player *pPlayer, int nItem, int nMode )
{
	static ConVarRef classicShootout(
		"fof_sv_classic_shootout", true );
	if ( ( nItem == 15 || nItem == 23 ) &&
		classicShootout.IsValid() && classicShootout.GetBool() )
	{
		return false;
	}

	if ( FoFSelectedEquipmentAllowsRestrictedItems( pPlayer, nMode ) )
		return true;

	switch ( nItem )
	{
	case 4:  // Henry rifle
	case 6:  // Coachgun
	case 12: // Sharps
	case 16: // Whiskey
	case 17: // Dynamite belt
	case 18: // Spencer
	case 21: // Peacemaker
	case 24: // Black bow
	case 27: // Black dynamite
	case 29: // Shotgun
	case 31: // Schofield
	case 32: // Walker
		return false;
	default:
		return true;
	}
}

static bool FoFDeathmatchEquipmentIsValid(
	CFoF_Player *pPlayer, const CUtlVector< int > &items )
{
	if ( !pPlayer )
		return false;

	int nPoints = 0;
	int nPrimaryWeapons = 0;
	int nHandgunSkills = 0;
	for ( int i = 0; i < items.Count(); ++i )
	{
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( items[i] );
		if ( !pItem || pItem->m_nDeathmatchCategory < 0 ||
			pItem->m_nDeathmatchCategory > 2 ||
			!FoFItemProgressionUnlocked(
				pItem, pPlayer->GetFoFProgression() ) )
			return false;

		nPoints += pItem->m_nDeathmatchCost;
		if ( pItem->m_nDeathmatchCategory == 1 )
			++nPrimaryWeapons;
		else if ( pItem->m_nDeathmatchCategory == 2 )
			++nHandgunSkills;
	}

	return nPoints <= FOF_ITEM_DM_POINT_LIMIT &&
		nPrimaryWeapons <= 1 && nHandgunSkills <= 1;
}

static void FoFApplyEquipmentVector(
	CFoF_Player *pPlayer, const CUtlVector< int > &items )
{
	for ( int i = 0; i < items.Count(); ++i )
	{
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( items[i] );
		if ( FoFItemProgressionUnlocked(
			pItem, pPlayer->GetFoFProgression() ) )
		{
			FoFApplyEquipmentItem( pPlayer, items[i] );
		}
	}
}

static void FoFApplySelectedEquipmentVector(
	CFoF_Player *pPlayer, const CUtlVector< int > &items )
{
	if ( !pPlayer || items.Count() <= 0 )
		return;

	// A selected loadout is marked active before its item transaction. Cash
	// purchases use a different routine and deliberately do not set this bit.
	pPlayer->m_nPlayerInfo |= 0x100;
	const int nMode = FoFPurchaseCurrentMode();
	bool bAppliedItem = false;
	for ( int i = 0; i < items.Count(); ++i )
	{
		const int nItem = items[i];
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( nItem );
		if ( FoFSelectedEquipmentItemAllowed(
			pPlayer, nItem, nMode ) &&
			FoFItemProgressionUnlocked(
				pItem, pPlayer->GetFoFProgression() ) )
		{
			FoFApplyEquipmentItem( pPlayer, nItem );
			bAppliedItem = true;
		}
	}
	if ( bAppliedItem )
		pPlayer->EmitSound( "FoFPlayer.Equipment" );
}

static bool FoFGiveForcedBotWeapon( CFoF_Player *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsBot() )
		return false;

	static ConVarRef forceWeapon( "fof_bot_forceweapon", true );
	const char *pszWeapon = forceWeapon.IsValid() ?
		forceWeapon.GetString() : NULL;
	if ( !pszWeapon || Q_strnicmp( pszWeapon, "weapon_", 7 ) )
		return false;

	// The shipped path clears the transient selection, then grants exactly the
	// named entity.  It does not enable the global force-weapons transaction.
	pPlayer->SetFoFEquipmentSelection( NULL, 0 );
	pPlayer->GiveFoFNamedItem( pszWeapon );
	return true;
}

void CFoF_Player::SetFoFEquipmentSelection(
	const int *pItems, int nItemCount )
{
	const int nMode = FoFPurchaseCurrentMode();
	CUtlVector< int > *pSelection = NULL;
	if ( FoFUsesDeathmatchEquipment( nMode ) )
	{
		pSelection = &m_FoFDeathmatchEquipment;
	}
	else if ( nMode == 2 || nMode == 3 )
	{
		pSelection = &m_FoFPendingPurchase;
	}

	if ( !pSelection )
		return;

	pSelection->RemoveAll();
	for ( int i = 0; pItems && i < nItemCount; ++i )
	{
		if ( pItems[i] >= 0 )
			pSelection->AddToTail( pItems[i] );
	}
}

void CFoF_Player::ApplyFoFEquipmentSelection( void )
{
	const int nMode = FoFPurchaseCurrentMode();
	if ( nMode != 2 && nMode != 3 )
		return;

	// FoF writes state 2 and re-enters GiveDefaultItems.  That path
	// validates the selected prices, replaces the saved cash loadout and gives
	// the selected items in the same order as a human buy_end command.
	m_nFoFPurchaseState = FOF_PURCHASE_REPLACE_SAVED;
	GiveDefaultItems();
}

void CFoF_Player::ApplyFoFCashPurchase( bool bCharge )
{
	const int nMode = FoFPurchaseCurrentMode();
	if ( nMode != 2 && nMode != 3 )
	{
		m_FoFPendingPurchase.RemoveAll();
		m_nFoFPurchaseState = FOF_PURCHASE_NONE;
		return;
	}

	if ( !bCharge )
	{
		FoFApplyEquipmentVector( this, m_FoFCashEquipment );
		if ( m_FoFCashEquipment.Count() > 0 )
			EmitSound( "FoFPlayer.Equipment" );
		m_nFoFPurchaseState = FOF_PURCHASE_NONE;
		return;
	}

	if ( m_nFoFPurchaseState == FOF_PURCHASE_REPLACE_SAVED && nMode != 3 )
		m_FoFCashEquipment.RemoveAll();

	const int nBuyZone = GetFoFInBuyZone();
	bool bAppliedItem = false;
	for ( int i = 0; i < m_FoFPendingPurchase.Count(); ++i )
	{
		const int nItem = m_FoFPendingPurchase[i];
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( nItem );
		if ( !pItem || pItem->m_nBasePrice < 0 ||
			!FoFItemProgressionUnlocked( pItem, GetFoFProgression() ) )
			continue;

		const int nPrice = FoFItemAdjustedPrice( pItem, nMode, nBuyZone );
		if ( nPrice < 0 || GetFoFCash() < (float)nPrice )
			continue;

		if ( nPrice > 0 )
			AddFoFCash( (float)-nPrice );
		FoFApplyEquipmentItem( this, nItem );
		m_FoFCashEquipment.AddToTail( nItem );
		bAppliedItem = true;
	}
	if ( bAppliedItem )
		EmitSound( "FoFPlayer.Equipment" );

	if ( nMode == 3 )
	{
		if ( m_FoFPendingPurchase.Count() > 0 &&
			!Weapon_OwnsThisType( "weapon_fists" ) )
		{
			GiveFoFNamedItem( "weapon_fists" );
		}
		m_flFoFSpawnBuyUntil = 0.0f;
	}

	m_FoFPendingPurchase.RemoveAll();
	m_nFoFPurchaseState = FOF_PURCHASE_NONE;
}

void CFoF_Player::FinalizeFoFSpawnEquipment( void )
{
	if ( m_bFoFSpawnEquipmentFinalized || m_bFoFSpawnPending )
		return;

	if ( gpGlobals->curtime <= m_flFoFNextSpawnEquipmentAttempt )
		return;
	m_flFoFNextSpawnEquipmentAttempt = gpGlobals->curtime + 0.25f;

	// The shipped ItemPostFrame transaction first verifies that the selected
	// spawn did not leave the player inside solid world geometry.  The same
	// retry flag is used when no spawn point could be selected.
	trace_t spawnTrace;
	const unsigned int nSpawnMask =
		FoFPurchaseCurrentMode() == 6 ? 0x400b : MASK_PLAYERSOLID;
	UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin(),
		WorldAlignMins(), WorldAlignMaxs(), nSpawnMask, this,
		COLLISION_GROUP_PLAYER_MOVEMENT, &spawnTrace );
	const bool bScriptedCourseSpawn = FoFPurchaseCurrentMode() == 6;
	if ( ( spawnTrace.startsolid || spawnTrace.allsolid ) &&
		!bScriptedCourseSpawn )
	{
		DevMsg( "Player %s stuck at spawn, retrying spawn...\n",
			GetPlayerName() );
		m_bFoFSpawnPending = true;
	}

	// CFoF_Player::ItemPostFrame in FoF finalizes a newly spawned
	// inventory exactly once.  Spawn itself only places and resets the player;
	// granting equipment there executes this transaction before the client and
	// the selected spawn are ready, then executes it a second time on warmup.
	const int nMode = FoFPurchaseCurrentMode();
	if ( !m_bFoFSpawnPending && nMode != 5 )
	{
		if ( !( m_Local.m_iHideHUD & HIDEHUD_HEALTH ) &&
			GetTeamNumber() != TEAM_SPECTATOR )
		{
			ApplyFoFClientPreferences();
			RemoveAllAmmo();
			RemoveAllItems( true );

			// BreakBad intentionally starts unarmed.  Every other ordinary spawn
			// is seeded with fists before the selected equipment transaction.
			if ( nMode != 3 )
			{
				if ( IsBot() )
					SelectFoFEquipment();
				GiveFoFNamedItem( "weapon_fists" );
				GiveDefaultItems();
			}
		}
		// Teamplay bots perform their cash/class refresh after the ordinary
		// spawn equipment pass, regardless of the HUD equipment gate above.
		if ( nMode == 2 && IsBot() )
			RefreshFoFEquipment();
		if ( nMode == 3 )
		{
			m_nPlayerInfo |= 0x800;
			FoFBreakBadPlayerSpawn( this );
			m_nFoFBuyZoneTier = 1;
			m_flFoFSpawnBuyUntil = gpGlobals->curtime + 10.0f;
			if ( GetFoFJailTime() <= gpGlobals->curtime &&
				m_FoFPendingPurchase.Count() > 0 )
			{
				GiveDefaultItems();
			}
		}
	}

	m_pFoFSpawnPoint = NULL;
	m_bFoFSpawnEquipmentFinalized = true;
}

void CFoF_Player::ApplyFoFSpawnEquipment( void )
{
	const int nMode = FoFPurchaseCurrentMode();
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	if ( nMode == 2 && teamClasses.IsValid() && teamClasses.GetBool() )
	{
		RemoveAllAmmo();
		RemoveAllItems( true );
		FoFResetEquipmentState( this );

		CHL2MPRules *pRules = HL2MPRules();
		if ( IsBot() && pRules &&
			( m_nFoFTeamClass < 0 ||
				m_nFoFTeamClass >= pRules->GetFoFTeamClassCount() ) )
		{
			m_nFoFTeamClass =
				pRules->SelectFoFAvailableTeamClass( GetTeamNumber() );
		}

		if ( !pRules || m_nFoFTeamClass < 0 ||
			m_nFoFTeamClass >= pRules->GetFoFTeamClassCount() )
		{
			if ( !IsBot() )
				ShowViewPortPanel( "buypreset_main", true );
			return;
		}

		GiveFoFNamedItem( "weapon_fists" );
		Weapon_Switch( Weapon_OwnsThisType( "weapon_fists" ) );
		m_nPlayerInfo |= 0x100;
		FoFApplyTeamClass( this, m_nFoFTeamClass );
		return;
	}

	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	if ( forceWeapons.IsValid() && forceWeapons.GetBool() )
	{
		static ConVarRef forceWeaponsList(
			"fof_sv_force_weapons_list", true );
		RemoveAllAmmo();
		RemoveAllItems( true );
		FoFResetEquipmentState( this );
		FoFSendEquipmentItem( this, -1 );

		const char *pszList = forceWeaponsList.IsValid() ?
			forceWeaponsList.GetString() : "";
		if ( pszList && pszList[0] &&
			Q_stricmp( pszList, "empty" ) && Q_stricmp( pszList, "0" ) )
		{
			m_nPlayerInfo |= 0x100;
			FoFGiveEquipmentList( this, pszList, true );
		}
		return;
	}

	if ( FoFGiveForcedBotWeapon( this ) )
		return;

	if ( FoFUsesDeathmatchEquipment( nMode ) )
	{
		FoFResetEquipmentState( this );
		FoFApplySelectedEquipmentVector(
			this, m_FoFDeathmatchEquipment );
		return;
	}

	if ( nMode != 2 && nMode != 3 )
		return;

	FoFResetEquipmentState( this );
	if ( m_nFoFPurchaseState == FOF_PURCHASE_REPLACE_SAVED ||
		m_nFoFPurchaseState == FOF_PURCHASE_APPEND_SAVED )
	{
		ApplyFoFCashPurchase( true );
	}
	else if ( m_nFoFPurchaseState == FOF_PURCHASE_REUSE_SAVED )
	{
		// Purchase history is not a respawn loadout. Only an explicit restore
		// transaction may replay it without charging for the items again.
		ApplyFoFCashPurchase( false );
	}
}

void CFoF_Player::ShowFoFSpawnEquipmentMenuIfNeeded( void )
{
	if ( IsObserver() || IsFakeClient() ||
		GetTeamNumber() == TEAM_SPECTATOR )
	{
		return;
	}

	static ConVarRef weaponMenu( "fof_sv_weaponmenu", true );
	static ConVarRef forceWeapons( "fof_sv_force_weapons", true );
	if ( ( weaponMenu.IsValid() && !weaponMenu.GetBool() ) ||
		( forceWeapons.IsValid() && forceWeapons.GetBool() ) )
	{
		return;
	}

	const int nMode = FoFPurchaseCurrentMode();
	static ConVarRef teamClasses( "fof_sv_tp_classes", true );
	if ( nMode == 2 && teamClasses.IsValid() && teamClasses.GetBool() )
	{
		// Class selection has its own first-selection prompt. It must not
		// also enter the cash buy-zone menu path on every equipment refresh.
		return;
	}

	if ( ( nMode == 1 || nMode == 4 ) &&
		m_FoFDeathmatchEquipment.Count() == 0 )
	{
		// GiveDefaultItems asks the client to open its local equipment menu only
		// when no deathmatch loadout has been committed.
		engine->ClientCommand( edict(), "equipmenu\n" );
		return;
	}

	if ( ( nMode == 2 || nMode == 3 ) &&
		m_FoFPendingPurchase.Count() == 0 && GetFoFInBuyZone() > 0 )
	{
		static ConVarRef warmup( "fof_warmup", true );
		if ( !warmup.IsValid() || !warmup.GetBool() )
		{
			// FoF gates this path on the replicated buy-zone tier, not the
			// player's cash balance.  The menu may therefore open with zero cash,
			// while a player outside a purchase area must not receive it at spawn.
			engine->ClientCommand( edict(), "equipmenu\n" );
		}
	}
}

bool CFoF_Player::HandleFoFEquipmentCommand( const CCommand &args )
{
	if ( args.ArgC() <= 0 )
		return false;

	const int nMode = FoFPurchaseCurrentMode();
	if ( FStrEq( args[0], "new" ) )
	{
		if ( nMode == 1 || nMode == 4 )
			m_FoFDeathmatchEquipment.RemoveAll();
		return true;
	}

	if ( FStrEq( args[0], "item_dm" ) )
	{
		if ( ( nMode != 1 && nMode != 4 ) || args.ArgC() < 2 )
			return true;

		const int nItem = Q_atoi( args[1] );
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( nItem );
		if ( !pItem || pItem->m_nDeathmatchCategory < 0 ||
			pItem->m_nDeathmatchCategory > 2 ||
			!FoFItemProgressionUnlocked( pItem, GetFoFProgression() ) )
		{
			return true;
		}

		m_FoFDeathmatchEquipment.AddToTail( nItem );
		if ( !FoFDeathmatchEquipmentIsValid(
			this, m_FoFDeathmatchEquipment ) )
		{
			m_FoFDeathmatchEquipment.RemoveAll();
		}
		return true;
	}

	if ( FStrEq( args[0], "item_dm_end" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		m_Local.m_iHideHUD = Q_atoi( args[1] ) > 0 ? 10 : 0;
		const int nEquipmentFlag = args.ArgC() >= 3 ?
			clamp( Q_atoi( args[2] ), 0, 10 ) : 0;
		if ( nEquipmentFlag == 1 )
			AddFoFEquipmentFlags( 0x80 );

		static ConVarRef weaponMenu( "fof_sv_weaponmenu", true );
		if ( !IsAlive() || m_bFoFDeathmatchLoadoutCommitted ||
			( nMode != 1 && nMode != 4 ) ||
			( weaponMenu.IsValid() && !weaponMenu.GetBool() ) )
			return true;
		if ( !FoFDeathmatchEquipmentIsValid(
			this, m_FoFDeathmatchEquipment ) )
		{
			return true;
		}

		RemoveAllItems( true );
		GiveFoFNamedItem( "weapon_fists" );
		Weapon_Switch( Weapon_OwnsThisType( "weapon_fists" ) );
		FoFResetEquipmentState( this );
		m_bFoFDeathmatchLoadoutCommitted = true;
		GiveDefaultItems();
		return true;
	}

	if ( FStrEq( args[0], "fof_buy" ) )
	{
		if ( ( nMode != 2 && nMode != 3 ) || args.ArgC() < 2 ||
			m_FoFPendingPurchase.Count() >= FOF_ITEM_PURCHASE_LIMIT )
		{
			return true;
		}

		const int nItem = Q_atoi( args[1] );
		const FoFItemDefinition_t *pItem =
			FoFFindItemDefinitionById( nItem );
		if ( pItem && pItem->m_nBasePrice >= 0 )
		{
			if ( FoFItemProgressionUnlocked(
				pItem, GetFoFProgression() ) )
			{
				m_FoFPendingPurchase.AddToTail( nItem );
			}
			else
			{
				char szRequiredRank[16];
				Q_snprintf( szRequiredRank, sizeof( szRequiredRank ),
					"%i", pItem->m_nProgressionRequirement / 100 );
				ClientPrint( this, HUD_PRINTTALK,
					"#FoF_NotEnoughRank", pItem->m_pszLabelToken,
					szRequiredRank );
			}
		}
		return true;
	}

	if ( FStrEq( args[0], "buy_end" ) )
	{
		if ( nMode != 2 && nMode != 3 )
		{
			m_FoFPendingPurchase.RemoveAll();
			return true;
		}
		if ( m_FoFPendingPurchase.Count() <= 0 )
			return true;
		if ( !IsAlive() || GetFoFInBuyZone() <= 0 ||
			( nMode == 3 && GetFoFJailTime() > gpGlobals->curtime ) )
		{
			m_FoFPendingPurchase.RemoveAll();
			return true;
		}

		m_nFoFPurchaseState = FOF_PURCHASE_REPLACE_SAVED;
		GiveDefaultItems();
		return true;
	}

	return false;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-authoritative consumable and potion-reward state.
//
//=============================================================================//

static bool FoFIsStackableWeaponPickup(
	CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	const char *pszClassname = pWeapon->GetClassname();
	return pszClassname &&
		( !Q_stricmp( pszClassname, "dynamite" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_black" ) ||
		  !Q_stricmp( pszClassname, "weapon_dynamite_belt" ) ||
		  !Q_stricmp( pszClassname, "weapon_knife" ) ||
		  !Q_stricmp( pszClassname, "weapon_axe" ) ||
		  !Q_stricmp( pszClassname, "weapon_machete" ) );
}

static bool FoFCanTouchWeaponPickup(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon || !pPlayer->IsAlive() ||
		gpGlobals->curtime < pPlayer->m_flNextPickupInteraction ||
		!pPlayer->IsAllowedToPickupWeapons() || pWeapon->GetOwner() )
	{
		return false;
	}

	if ( !pPlayer->Weapon_CanUse( pWeapon ) ||
		!g_pGameRules->CanHavePlayerItem( pPlayer, pWeapon ) )
	{
		return false;
	}

	return true;
}

static void FoFConsumeWeaponPickup( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return;

	UTIL_Remove( pWeapon );
}

static CBaseCombatWeapon *FoFCreateHandReplacement(
	CFoF_Player *pPlayer, const char *pszClassname,
	CBaseCombatWeapon *pOldWeapon )
{
	if ( !pPlayer || !pszClassname || !pOldWeapon )
		return NULL;

	CBaseEntity *pEntity = CreateEntityByName( pszClassname );
	CBaseCombatWeapon *pNewWeapon =
		dynamic_cast< CBaseCombatWeapon * >( pEntity );
	if ( !pNewWeapon )
	{
		if ( pEntity )
			UTIL_Remove( pEntity );
		return NULL;
	}

	pEntity->SetLocalOrigin( pPlayer->GetLocalOrigin() );
	pEntity->AddSpawnFlags( SF_NORESPAWN );
	pNewWeapon->SetOwner( pPlayer );
	pNewWeapon->SetOwnerEntity( pPlayer );
	DispatchSpawn( pEntity );
	if ( pEntity->IsMarkedForDeletion() )
		return NULL;
	pNewWeapon->SetOwner( NULL );
	pNewWeapon->SetOwnerEntity( NULL );

	pNewWeapon->m_bFiresUnderwater = pOldWeapon->m_bFiresUnderwater;
	pNewWeapon->m_iClip1 = pOldWeapon->m_iClip1;
	pNewWeapon->SetFoFShotCounter( pOldWeapon->GetFoFShotCounter() );

	pPlayer->Weapon_Equip( pNewWeapon );
	return pNewWeapon;
}

static bool FoFEquipFreshWeaponPickup(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon,
	bool bSwitchWeapon )
{
	if ( !pPlayer || !pWeapon )
		return false;

	// Course display weapons begin fixed to the world. Equip destroys a live
	// constraint, but the spawn/solid markers must also be cleared when no
	// constraint object survived initialization; otherwise the next Drop calls
	// FallInit and fixes the weapon to the world again.
	pWeapon->RemoveSpawnFlags( SF_WEAPON_START_CONSTRAINED );
	pWeapon->RemoveSolidFlags( FOF_COURSE_USE_PICKUP_SOLID_FLAG );
	pWeapon->RemoveEffects( FOF_COURSE_USE_PROMPT_EFFECT );

	pPlayer->Weapon_Equip( pWeapon );
	// Some FoF pickups become the active weapon during Equip.  Finish that
	// transition through the normal deployment path, while leaving an existing
	// active weapon selected for inventory-only pickups.
	if ( bSwitchWeapon || pPlayer->GetActiveWeapon() == pWeapon )
		pPlayer->Weapon_Switch( pWeapon );
	pPlayer->RecalculateWeaponSpeed();
	return true;
}

static int FoFCountFirstHandWeapons( CFoF_Player *pPlayer )
{
	int nCount = 0;
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pOwned = pPlayer->GetWeapon( i );
		if ( pOwned && pOwned->CanDualWield() && !pOwned->IsSecondGun() )
			++nCount;
	}
	return nCount;
}

static int FoFCountSecondHandWeapons( CFoF_Player *pPlayer )
{
	int nCount = 0;
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pOwned = pPlayer->GetWeapon( i );
		if ( pOwned && pOwned->IsSecondGun() )
			++nCount;
	}
	return nCount;
}

static const char *FoFSelectFreshWeaponHandClassname(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon,
	const char *pszOpposite )
{
	const char *pszClassname = pWeapon->GetClassname();
	bool bUseSecondHand;
	switch ( pPlayer->GetFoFHandStance() )
	{
	case 0:
		bUseSecondHand =
			FoFCountFirstHandWeapons( pPlayer ) >
			FoFCountSecondHandWeapons( pPlayer );
		break;
	case 1:
		bUseSecondHand = false;
		break;
	case 2:
		bUseSecondHand = true;
		break;
	default:
		return pszClassname;
	}

	return pWeapon->IsSecondGun() == bUseSecondHand ?
		pszClassname : pszOpposite;
}

static bool FoFEquipDualWeaponPickup(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon )
{
	const char *pszClassname = pWeapon->GetClassname();
	const char *pszOpposite =
		FoFGetOppositeHandWeaponClassname( pszClassname );
	if ( !pszClassname || !pszOpposite )
		return FoFEquipFreshWeaponPickup( pPlayer, pWeapon, false );

	CBaseCombatWeapon *pSame = pPlayer->Weapon_OwnsThisType(
		pszClassname, pWeapon->GetSubType() );
	CBaseCombatWeapon *pOpposite =
		pPlayer->Weapon_OwnsThisType( pszOpposite );

	if ( !pSame )
	{
		const char *pszSelectedClassname =
			FoFSelectFreshWeaponHandClassname(
				pPlayer, pWeapon, pszOpposite );
		if ( !Q_stricmp( pszSelectedClassname, pszClassname ) )
			return FoFEquipFreshWeaponPickup( pPlayer, pWeapon, false );

		CBaseCombatWeapon *pNewWeapon = FoFCreateHandReplacement(
			pPlayer, pszSelectedClassname, pWeapon );
		if ( !pNewWeapon )
			return false;

		FoFConsumeWeaponPickup( pWeapon );
		pPlayer->RecalculateWeaponSpeed();
		return true;
	}

	if ( !pOpposite )
	{
		CBaseCombatWeapon *pNewWeapon = FoFCreateHandReplacement(
			pPlayer, pszOpposite, pWeapon );
		if ( !pNewWeapon )
			return false;

		// Acquiring the partner hand adds it to inventory without changing the
		// current hand. A later explicit slot command performs the deployment.
		FoFConsumeWeaponPickup( pWeapon );
		pPlayer->RecalculateWeaponSpeed();
		return true;
	}

	CBaseCombatWeapon *pReplace =
		pSame->m_iClip1 <= pOpposite->m_iClip1 ? pSame : pOpposite;
	if ( pWeapon->m_iClip1 <= pReplace->m_iClip1 )
		return false;

	const bool bWasActive = pReplace == pPlayer->GetActiveWeapon1() ||
		pReplace == pPlayer->GetActiveWeapon2();
	const char *pszReplacementClass = pReplace->GetClassname();
	char szReplacementClass[64];
	Q_strncpy( szReplacementClass, pszReplacementClass,
		sizeof( szReplacementClass ) );
	pPlayer->Weapon_Drop( pReplace );

	if ( !Q_stricmp( szReplacementClass, pszClassname ) )
	{
		return FoFEquipFreshWeaponPickup(
			pPlayer, pWeapon, bWasActive );
	}

	CBaseCombatWeapon *pNewWeapon = FoFCreateHandReplacement(
		pPlayer, szReplacementClass, pWeapon );
	if ( !pNewWeapon )
		return false;

	if ( bWasActive )
		pPlayer->Weapon_Switch( pNewWeapon );
	FoFConsumeWeaponPickup( pWeapon );
	pPlayer->RecalculateWeaponSpeed();
	return true;
}

static bool FoFEquipGrantedCourseBotWeapon(
	CFoF_Player *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon )
		return false;
	return pWeapon->CanDualWield() ?
		FoFEquipDualWeaponPickup( pPlayer, pWeapon ) :
		FoFEquipFreshWeaponPickup( pPlayer, pWeapon, false );
}

static void FoFSendWeaponPickupNotice(
	CFoF_Player *pPlayer, const char *pszToken )
{
	if ( !pPlayer || !pszToken )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "BBNotices" );
		WRITE_BYTE( 1 );
		WRITE_STRING( pszToken );
	MessageEnd();
}

bool CFoF_Player::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;

	if ( IsBot() && GetTeamNumber() == FOF_TEAM_ZOMBIES &&
		!FClassnameIs( pWeapon, "weapon_fists" ) )
	{
		return false;
	}

	if ( !FoFCanTouchWeaponPickup( this, pWeapon ) )
		return false;

	if ( IsBot() && pWeapon->IsRemoveable() )
	{
		Vector vecVelocity;
		pWeapon->GetVelocity( &vecVelocity, NULL );
		if ( vecVelocity.LengthSqr() > Square( 10.0f ) )
			return false;
	}

	if ( IsFoFBotGhost() &&
		!FClassnameIs( pWeapon, "weapon_fists_ghost" ) )
	{
		return false;
	}

	if ( IsBot() && FClassnameIs( pWeapon, "weapon_xbow" ) )
		return false;

	if ( FClassnameIs( pWeapon, "weapon_xbow" ) &&
		pWeapon->GetTeamNumber() > TEAM_UNASSIGNED &&
		pWeapon->GetTeamNumber() != GetTeamNumber() )
	{
		if ( gpGlobals->curtime > m_flNextPickupInteraction )
		{
			EmitSound( "Player.DenyWeaponSelection" );
			FoFSendWeaponPickupNotice(
				this, "#fof_xbow_pickup_wrong_team" );
			m_flNextPickupInteraction = gpGlobals->curtime + 5.0f;
		}
		return false;
	}

	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 3 &&
		gpGlobals->curtime < GetFoFJailTime() )
	{
		if ( gpGlobals->curtime > m_flNextPickupInteraction )
		{
			FoFSendWeaponPickupNotice(
				this, "#bb_jail_pickup_warning" );
			m_flNextPickupInteraction = gpGlobals->curtime + 10.0f;
		}
		return false;
	}

	if ( FClassnameIs( pWeapon, "weapon_dynamite_belt" ) )
	{
		m_nPlayerInfo |= 0x80000;
		SetBodygroup( 4, 1 );
		pWeapon->SetFoFShotCounter( 100 );
		CBasePlayer::GiveAmmo( 1, "dynamite_weak" );
	}

	if ( FoFBattleRoyaleCanAcquireOutlawRole() &&
		!IsFoFBattleRoyaleOutlaw() && FoFWeaponTier( pWeapon ) > 1 )
	{
		SetFoFBattleRoyaleRoleAppearance( true );
		SetFoFBattleRoyaleOutlaw( true );
		SetFoFPlayerKills( 0 );
	}

	bool bPickedUp = false;
	if ( pWeapon->CanDualWield() )
	{
		bPickedUp = FoFEquipDualWeaponPickup( this, pWeapon );
	}
	else
	{
		CBaseCombatWeapon *pOwned = Weapon_OwnsThisType(
			pWeapon->GetClassname(), pWeapon->GetSubType() );
		if ( !pOwned )
		{
			bPickedUp = FoFEquipFreshWeaponPickup(
				this, pWeapon, false );
		}
		else if ( FoFIsStackableWeaponPickup( pWeapon ) &&
			Weapon_EquipAmmoOnly( pWeapon ) )
		{
			FoFConsumeWeaponPickup( pWeapon );
			RecalculateWeaponSpeed();
			bPickedUp = true;
		}
	}

	if ( bPickedUp )
	{
		FoFReportCourseStat( "pick_wep", this );
	}
	return bPickedUp;
}

static void FoFSelectFullPotionWeapon(
	CFoF_Player *pPlayer, const char *pszClassname )
{
	CBaseCombatWeapon *pWeapon =
		pPlayer->Weapon_OwnsThisType( pszClassname );
	if ( !pWeapon )
		return;

	pPlayer->Weapon_Switch( pWeapon );
	pWeapon->m_iClip1 = 6;
}

CBaseCombatWeapon *CFoF_Player::ReplaceFoFWeaponHand(
	CBaseCombatWeapon *pWeapon, bool bSwitchNewWeapon )
{
	const char *pszReplacement =
		FoFGetOppositeHandWeaponClassname(
			pWeapon ? pWeapon->GetClassname() : NULL );
	if ( !pWeapon || !pszReplacement )
		return NULL;

	const int nReserveAmmoType = pWeapon->GetPrimaryAmmoType();
	const int nReserveAmmo = nReserveAmmoType >= 0 ?
		GetAmmoCount( nReserveAmmoType ) : 0;
	const int nViewModelIndex = pWeapon->IsSecondGun() ? 1 : 0;

	pWeapon->Holster( NULL );
	Vector vecWeaponVelocity;
	pWeapon->GetVelocity( &vecWeaponVelocity, NULL );
	Weapon_Detach( pWeapon );
	CBaseViewModel *pViewModel = GetViewModel( nViewModelIndex );
	if ( pViewModel )
		pViewModel->AddEffects( EF_NODRAW );

	m_flNextFoFHandSwitchTime = gpGlobals->curtime + 1.0f;
	CBaseCombatWeapon *pNewWeapon = FoFCreateHandReplacement(
		this, pszReplacement, pWeapon );
	if ( !pNewWeapon )
		return NULL;

	const int nNewAmmoType = pNewWeapon->GetPrimaryAmmoType();
	if ( nReserveAmmoType >= 0 && nNewAmmoType >= 0 )
		SetAmmoCount( nReserveAmmo, nNewAmmoType );

	if ( bSwitchNewWeapon )
		Weapon_Switch( pNewWeapon );

	UTIL_Remove( pWeapon );
	FoFReportCourseStat( "switch_side", this );
	return pNewWeapon;
}

void CFoF_Player::UpdateFoFHandSideSwitch()
{
	if ( !( m_afButtonPressed & IN_ALT1 ) ||
		( m_nPlayerInfo & 0x40000 ) ||
		( GetFlags() & FL_FROZEN ) ||
		gpGlobals->curtime <= m_flNextFoFHandSwitchTime )
	{
		return;
	}

	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	if ( !pActiveWeapon )
		return;

	if ( HasDualActiveWeapons() )
	{
		CBaseCombatWeapon *pFirstWeapon = GetActiveWeapon1();
		CBaseCombatWeapon *pSecondWeapon = GetActiveWeapon2();
		if ( !pFirstWeapon || !pSecondWeapon ||
			gpGlobals->curtime <= pFirstWeapon->m_flNextPrimaryAttack ||
			gpGlobals->curtime <= pSecondWeapon->m_flNextPrimaryAttack )
		{
			return;
		}

		CBaseCombatWeapon *pNewFirst =
			ReplaceFoFWeaponHand( pFirstWeapon, false );
		ReplaceFoFWeaponHand( pSecondWeapon, true );
		if ( pNewFirst )
			Weapon_Switch( pNewFirst );
		return;
	}

	if ( !pActiveWeapon->CanDualWield() ||
		gpGlobals->curtime <= pActiveWeapon->m_flNextPrimaryAttack )
	{
		return;
	}

	ReplaceFoFWeaponHand( pActiveWeapon, true );
}

bool CFoF_Player::ConsumeFoFWhiskey( int nHealth )
{
	if ( !IsAlive() || IsOnFoFHorse() || FoFIsReloading() )
		return false;

	// The shipped server checks the ordinary 100-health ceiling separately
	// from GetMaxHealth(). Players above either limit only produce the throttled
	// burp response and leave the pickup in the world.
	if ( GetHealth() >= 100 || GetHealth() >= GetMaxHealth() )
	{
		if ( gpGlobals->curtime > m_flNextFoFBurpTime )
		{
			m_flNextFoFBurpTime = gpGlobals->curtime + 1.5f;
			EmitSound( "Player.Burp" );
		}
		return false;
	}

	// CItem_FoFResupply::MyTouch in the shipped server adds one quarter of
	// the restored health to the networked drunkness value.  A normal
	// 25-health bottle therefore contributes 6.25, not 0.25.
	m_flDrunkness += nHealth * 0.25f;
	AddFoFDrunkardAmount( nHealth );
	m_nPlayerAccuracy = clamp( m_nPlayerAccuracy.Get() - nHealth, 0, 100 );
	TakeHealth( static_cast< float >( nHealth ), DMG_GENERIC );
	EmitSound( "Whiskey.Glug" );
	static ConVarRef currentMode( "fof_sv_currentmode", true );
	if ( currentMode.IsValid() && currentMode.GetInt() == 3 &&
		nHealth > 10 )
	{
		const int nCash = RoundFloatToInt( RemapValClamped(
			static_cast< float >( nHealth ),
			10.0f, 50.0f, 0.0f, 5.0f ) );
		AwardFoFCash(
			static_cast< float >( nCash ), "#Cash_Added_Whiskey" );
	}
	FoFReportCourseStat( "drink_whiskey", this );
	return true;
}

void CFoF_Player::AddFoFPotion( int nAmount )
{
	m_nPotionLevel = clamp( m_nPotionLevel.Get() + nAmount, 0, 100 );
	if ( m_nPotionLevel == 100 && !( m_nPlayerInfo & 0x40000 ) )
		ActivateFoFPotionReward();
}

void CFoF_Player::ActivateFoFPotionReward()
{
	EmitSound( "Ragged_Powerup.Full" );

	m_nHandStance = 0;

	// The reward's grant policy skips an already complete pair. Pickup may
	// replace a hand leaf, so select from the final inventory, not from the
	// temporary entity returned by GiveFoFNamedItem.
	if ( !Weapon_OwnsThisType( "weapon_peacemaker" ) ||
		!Weapon_OwnsThisType( "weapon_peacemaker2" ) )
	{
		GiveFoFNamedItem( "weapon_peacemaker2" );
	}
	if ( !Weapon_OwnsThisType( "weapon_peacemaker" ) ||
		!Weapon_OwnsThisType( "weapon_peacemaker2" ) )
	{
		GiveFoFNamedItem( "weapon_peacemaker" );
	}
	FoFSelectFullPotionWeapon( this, "weapon_peacemaker" );
	FoFSelectFullPotionWeapon( this, "weapon_peacemaker2" );

	m_flFoFPotionTickTime = gpGlobals->curtime;
	m_flFoFPotionActivateTime = gpGlobals->curtime + 0.2f;
}

void CFoF_Player::UpdateFoFPotionReward()
{
	if ( m_flFoFPotionActivateTime > 0.0f &&
		gpGlobals->curtime > m_flFoFPotionActivateTime )
	{
		m_nPlayerInfo |= 0x40000;
		m_flFoFPotionActivateTime = 0.0f;
	}

	if ( !( m_nPlayerInfo & 0x40000 ) ||
		gpGlobals->curtime <= m_flFoFPotionTickTime )
	{
		return;
	}

	m_nPotionLevel -= 3;
	m_flFoFPotionTickTime = gpGlobals->curtime + 1.0f;
	if ( m_nPotionLevel <= 0 )
	{
		m_nPotionLevel = 0;
		m_nPlayerInfo &= ~0x40000;
	}
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Server-authoritative FoF +use interactions.
//
//=============================================================================//

static bool FoFIsUseMeleeWeapon( CBaseEntity *pEntity )
{
	return pEntity &&
		( FClassnameIs( pEntity, "weapon_knife" ) ||
		  FClassnameIs( pEntity, "weapon_axe" ) ||
		  FClassnameIs( pEntity, "weapon_machete" ) );
}

class CWTraceEnum : public IEntityEnumerator
{
public:
	explicit CWTraceEnum( CFoF_Player *pPlayer ) :
		m_pPlayer( pPlayer ),
		m_pBestEntity( NULL ),
		m_flBestScore( -FLT_MAX )
	{
	}

	bool EnumEntity( IHandleEntity *pHandleEntity ) OVERRIDE
	{
		if ( !m_pPlayer || pHandleEntity == m_pPlayer )
			return true;

		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		const bool bCourseUsePickup = pEntity &&
			pEntity->IsSolidFlagSet( FOF_COURSE_USE_PICKUP_SOLID_FLAG );
		if ( !pEntity ||
			( !bCourseUsePickup && pEntity->GetGroundEntity() &&
			  !FoFIsUseMeleeWeapon( pEntity ) ) )
			return true;

		const Vector vecOrigin = pEntity->WorldSpaceCenter();
		Vector vecForward;
		m_pPlayer->EyeVectors( &vecForward );
		Vector vecToEntity = vecOrigin - m_pPlayer->EyePosition();
		const float flDistance = VectorNormalize( vecToEntity );
		const float flAlignment = DotProduct( vecForward, vecToEntity );
		if ( !m_pPlayer->FVisible(
			vecOrigin, MASK_SHOT_HULL, NULL ) ||
			flAlignment <= 0.93f )
		{
			return true;
		}

		const float flScore = flAlignment * 1000.0f - flDistance;
		if ( flScore > m_flBestScore )
		{
			m_flBestScore = flScore;
			m_pBestEntity = pEntity;
		}
		return true;
	}

	bool TryBestEntity()
	{
		return m_pBestEntity &&
			m_pPlayer->TryFoFUseEntity( m_pBestEntity );
	}

private:
	CFoF_Player *m_pPlayer;
	CHandle< CBaseEntity > m_pBestEntity;
	float m_flBestScore;
};

bool CFoF_Player::TryFoFUseEntity( CBaseEntity *pEntity )
{
	if ( !pEntity || pEntity == this )
		return false;

	CItem *pItem = NULL;
	if ( dynamic_cast< CItem_FoFResupply * >( pEntity ) ||
		dynamic_cast< CItem_FoFPotion * >( pEntity ) ||
		dynamic_cast< CItem_FoFSkull * >( pEntity ) ||
		dynamic_cast< CItem_XBowSpawn * >( pEntity ) )
	{
		pItem = dynamic_cast< CItem * >( pEntity );
	}

	if ( pItem )
	{
		if ( pItem->IsEffectActive( EF_NODRAW ) )
			return false;

		m_flNextPickupInteraction = gpGlobals->curtime + 0.3f;
		// ItemTouch owns the successful-pickup lifecycle: it invokes MyTouch,
		// fires the item outputs, then hides the item for respawn or removes it.
		// Calling MyTouch directly applies the effect but leaves the bottle in
		// the world forever.
		pItem->ItemTouch( this );
		return true;
	}

	CBaseCombatWeapon *pWeapon =
		dynamic_cast< CBaseCombatWeapon * >( pEntity );
	if ( pWeapon )
	{
		if ( pWeapon->IsEffectActive( EF_NODRAW ) )
			return false;

		if ( !FoFIsUseMeleeWeapon( pWeapon ) )
		{
			CBaseEntity *pOwner = pWeapon->GetOwnerEntity();
			if ( pOwner )
			{
				Vector vecVelocity;
				pOwner->GetVelocity( &vecVelocity, NULL );
				if ( vecVelocity.Length() > 10.0f )
					return false;
			}
		}

		if ( !BumpWeapon( pWeapon ) )
			return false;

		pWeapon->OnPickedUp( this );
		m_flNextPickupInteraction = gpGlobals->curtime + 0.3f;
		return true;
	}

	CFoF_Horse *pHorse = dynamic_cast< CFoF_Horse * >( pEntity );
	if ( pHorse && !pHorse->GetMoveParent() )
		return TryMountFoFHorse();

	return false;
}

bool CFoF_Player::UpdateFoFUseInteractions()
{
	bool bHandled = false;
	CBaseEntity *pTank = NULL;
	while ( ( pTank = gEntList.FindEntityByClassname(
		pTank, "func_tank_fof" ) ) != NULL )
	{
		if ( GetAbsOrigin().DistTo( pTank->GetAbsOrigin() ) >= 75.0f )
			continue;

		pTank->Use( this, this, USE_TOGGLE, 0.0f );
		m_flNextPickupInteraction = gpGlobals->curtime + 1.1f;
		bHandled = true;
		break;
	}

	Vector vecForward;
	EyeVectors( &vecForward );
	VectorNormalize( vecForward );
	const Vector vecStart = EyePosition();
	const Vector vecEnd = vecStart + vecForward * 75.0f;

	trace_t trace;
	UTIL_TraceLine(
		vecStart, vecEnd, MASK_SHOT,
		this, COLLISION_GROUP_NONE, &trace );
	if ( trace.m_pEnt && !trace.m_pEnt->IsWorld() &&
		TryFoFUseEntity( trace.m_pEnt ) )
	{
		return true;
	}

	Ray_t ray;
	ray.Init(
		vecStart, vecEnd,
		Vector( -10.0f, -10.0f, -10.0f ),
		Vector( 10.0f, 10.0f, 10.0f ) );
	CWTraceEnum enumerator( this );
	enginetrace->EnumerateEntities( ray, false, &enumerator );
	return bHandled || enumerator.TryBestEntity();
}

void CFoF_Player::PlayerUse()
{
	if ( ( m_nButtons & IN_USE ) && !m_bPickupActive &&
		gpGlobals->curtime >= m_flNextPickupInteraction &&
		UpdateFoFUseInteractions() )
	{
		return;
	}

	BaseClass::PlayerUse();
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server-authoritative physics-object carry state.
//
//=============================================================================//

extern ConVar fof_sv_pickup_maxweight;

static bool FoFIsRegularDynamite( CBaseEntity *pObject )
{
	return pObject && FClassnameIs( pObject, "dynamite" );
}

static bool FoFIsBlackDynamite( CBaseEntity *pObject )
{
	return pObject && FClassnameIs( pObject, "dynamite_black" );
}

static bool FoFTryDefuseDynamite(
	CFoF_Player *pPlayer, CBaseEntity *pDynamite )
{
	if ( !pPlayer || !FoFIsRegularDynamite( pDynamite ) )
		return false;

	Vector vecVelocity;
	pDynamite->GetVelocity( &vecVelocity, NULL );
	if ( vecVelocity.Length2D() >= 25.0f )
		return false;

	const Vector vecStart = pDynamite->GetAbsOrigin();
	Vector vecEnd = vecStart;
	vecEnd.z -= 20.0f;

	trace_t trace;
	UTIL_TraceLine(
		vecStart, vecEnd, MASK_SOLID_BRUSHONLY,
		pPlayer, COLLISION_GROUP_NONE, &trace );
	if ( trace.fraction >= 1.0f )
		return false;

	if ( pDynamite->GetOwnerEntity() != pPlayer )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "defuser" );
		if ( pEvent )
		{
			pEvent->SetInt( "entindex_defuser",
				engine->GetPlayerUserId( pPlayer->edict() ) );
			gameeventmanager->FireEvent( pEvent );
		}
	}

	return FoFDefuseDynamite( pDynamite );
}

static CBaseEntity *FoFFindPickupObjectInCone(
	CFoF_Player *pPlayer, const Vector &vecOrigin,
	const Vector &vecDirection )
{
	CBaseEntity *pEntities[256];
	const float flSearchDistance = 101.0f;
	const Vector vecExtents(
		flSearchDistance, flSearchDistance, flSearchDistance );
	const int nEntities = UTIL_EntitiesInBox(
		pEntities, ARRAYSIZE( pEntities ),
		vecOrigin - vecExtents, vecOrigin + vecExtents, 0 );

	CBaseEntity *pNearest = NULL;
	float flNearestDistance = flSearchDistance;
	CTraceFilterNoOwnerTestDR traceFilter(
		pPlayer, COLLISION_GROUP_NONE );

	for ( int i = 0; i < nEntities; ++i )
	{
		CBaseEntity *pEntity = pEntities[i];
		IPhysicsObject *pPhysics =
			pEntity ? pEntity->VPhysicsGetObject() : NULL;
		if ( !pPhysics || !pPhysics->IsMoveable() )
			continue;

		Vector vecToEntity = pEntity->WorldSpaceCenter() - vecOrigin;
		const float flDistance = VectorNormalize( vecToEntity );
		if ( flDistance >= flNearestDistance ||
			DotProduct( vecToEntity, vecDirection ) <= 0.97f )
		{
			continue;
		}

		trace_t trace;
		UTIL_TraceLine(
			vecOrigin, pEntity->WorldSpaceCenter(), MASK_SHOT,
			&traceFilter, &trace );
		if ( trace.m_pEnt != pEntity )
			continue;

		flNearestDistance = flDistance;
		pNearest = pEntity;
	}

	return pNearest;
}

static CBaseEntity *FoFTracePickupObject(
	CFoF_Player *pPlayer, Vector &vecGrabPosition )
{
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	const Vector vecStart = pPlayer->EyePosition();
	const Vector vecEnd = vecStart + vecForward * 200.0f;

	trace_t trace;
	CTraceFilterNoOwnerTestDR traceFilter(
		pPlayer, COLLISION_GROUP_NONE );
	UTIL_TraceLine(
		vecStart, vecEnd, MASK_SHOT,
		&traceFilter, &trace );
	if ( trace.fraction == 1.0f || !trace.m_pEnt || trace.m_pEnt->IsWorld() )
	{
		UTIL_TraceHull(
			vecStart, vecEnd,
			Vector( -4.0f, -4.0f, -4.0f ),
			Vector( 4.0f, 4.0f, 4.0f ),
			MASK_SHOT, &traceFilter, &trace );
	}

	CBaseEntity *pObject = NULL;
	if ( trace.fraction != 1.0f && trace.fraction <= 0.4f &&
		trace.m_pEnt && !trace.m_pEnt->IsWorld() )
	{
		pObject = trace.m_pEnt;
		vecGrabPosition = trace.endpos;
	}
	else
	{
		pObject = FoFFindPickupObjectInCone(
			pPlayer, vecStart, vecForward );
		if ( pObject )
			vecGrabPosition = pObject->WorldSpaceCenter();
	}

	if ( !pObject ||
		( pObject->WorldSpaceCenter() - vecStart ).LengthSqr() > 40000.0f )
	{
		return NULL;
	}

	return pObject;
}

static Vector FoFCarriedObjectLaunchObstruction(
	IPhysicsObject *pPhysics, CFoF_Player *pPlayer,
	const Vector &vecDirection )
{
	if ( !pPhysics || !pPlayer )
		return vec3_origin;

	const CPhysCollide *pCollide = pPhysics->GetCollide();
	if ( !pCollide )
		return vec3_origin;

	Vector vecMins;
	Vector vecMaxs;
	physcollision->CollideGetAABB(
		&vecMins, &vecMaxs, pCollide, vec3_origin, vec3_angle );

	Vector vecPosition;
	QAngle angPosition;
	pPhysics->GetPosition( &vecPosition, &angPosition );
	if ( !pPhysics->GetGameData() )
		return vec3_origin;

	const Vector vecCenter = pPlayer->WorldSpaceCenter();
	trace_t trace;
	UTIL_TraceLine(
		vecCenter - vecDirection * 32.0f,
		vecCenter + vecDirection * 96.0f,
		MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &trace );

	if ( trace.startsolid )
		return -vecDirection;
	if ( trace.fraction > 0.0f && trace.fraction < 1.0f )
		return trace.plane.normal;
	return vec3_origin;
}

static void FoFThrowCarriedPhysicsObject(
	IPhysicsObject *pPhysics, float flScale,
	const Vector &vecDirection, CFoF_Player *pPlayer )
{
	if ( !pPhysics || !pPlayer )
		return;

	const Vector vecPlayerCenter = pPlayer->WorldSpaceCenter();
	const float flSpeed = RemapValClamped(
		pPhysics->GetMass(), 30.0f, 60.0f, 250.0f, 600.0f ) *
		flScale;

	if ( FoFCarriedObjectLaunchObstruction(
			pPhysics, pPlayer, vecDirection ) != vec3_origin )
	{
		Vector vecPosition;
		QAngle angPosition;
		pPhysics->GetPosition( &vecPosition, &angPosition );

		const CPhysCollide *pCollide = pPhysics->GetCollide();
		if ( pCollide )
		{
			const float flOffset = RemapValClamped(
				physcollision->CollideVolume(
					const_cast< CPhysCollide * >( pCollide ) ),
				2000.0f, 65000.0f, 0.0f, 32.0f );
			vecPosition = vecPlayerCenter - vecDirection * flOffset;
			pPhysics->SetPosition( vecPosition, angPosition, false );
		}
	}

	Vector vecVelocity = vecDirection * flSpeed;
	AngularImpulse angVelocity = RandomAngularImpulse( -600.0f, 600.0f );
	pPhysics->AddVelocity( &vecVelocity, &angVelocity );
}

bool CFoF_Player::CanPickupFoFObject( CBaseEntity *pObject )
{
	if ( !pObject || m_bPickupActive )
		return false;

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	if ( pPhysics )
	{
		Vector vecVelocity;
		pObject->GetVelocity( &vecVelocity, NULL );
		if ( vecVelocity.Length2D() > 75.0f ||
			( pPhysics->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
		{
			return false;
		}
	}

	const Vector &vecSize = pObject->CollisionProp()->OBBSize();
	if ( vecSize.x > 64.0f || vecSize.y > 64.0f || vecSize.z > 80.0f )
		return false;

	// Live dynamite is handled by its own use path after the common physical
	// speed and size checks in the original server.
	if ( FClassnameIs( pObject, "dynamite" ) )
		return true;

	CBaseAnimating *pAnimating = pObject->GetBaseAnimating();
	if ( ( pAnimating && pAnimating->IsDissolving() ) ||
		pObject->IsEFlagSet( EFL_NO_PHYSCANNON_INTERACTION ) )
	{
		return false;
	}

	CBaseEntity *pGround = GetGroundEntity();
	if ( !pGround || pGround == pObject || pObject->VPhysicsIsFlesh() )
		return false;

	if ( dynamic_cast< CBaseCombatWeapon * >( pObject ) ||
		dynamic_cast< CItem * >( pObject ) )
	{
		return false;
	}

	return CBasePlayer::CanPickupObject(
		pObject, fof_sv_pickup_maxweight.GetFloat(), 0.0f );
}

bool CFoF_Player::AttachFoFCarriedObject(
	CBaseEntity *pObject, const Vector &vecGrabPosition )
{
	if ( !CanPickupFoFObject( pObject ) )
		return false;

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	if ( !pPhysics )
		return false;

	m_FoFGrabController.SetIgnorePitch( true );
	m_FoFGrabController.SetAngleAlignment( 0.0f );
	pPhysics->EnableCollisions( false );
	m_bPickupActive = true;

	Pickup_OnPhysGunPickup( pObject, this );
	m_FoFGrabController.AttachEntity(
		this, pObject, pPhysics, false, vecGrabPosition, false );
	m_hAttachedObject = pObject;
	m_attachedPositionObjectSpace =
		m_FoFGrabController.m_attachedPositionObjectSpace;
	m_attachedAnglesPlayerSpace =
		m_FoFGrabController.m_attachedAnglesPlayerSpace;

	pObject->SetOwnerEntity( this );
	m_bFoFResetPickupOwner = true;

	FoFReportCourseStat( "grab_phys", this );
	m_flNextPickupInteraction = gpGlobals->curtime + 0.25f;
	return true;
}

void CFoF_Player::DropFoFCarriedObject(
	bool bClearVelocity, bool bThrown )
{
	if ( !m_bPickupActive && !m_FoFGrabController.GetAttached() )
		return;

	CBaseEntity *pObject = m_FoFGrabController.GetAttached();
	if ( !pObject )
		pObject = m_hAttachedObject.Get();
	IPhysicsObject *pPhysics = pObject ? pObject->VPhysicsGetObject() : NULL;
	if ( pPhysics )
	{
		pPhysics->EnableCollisions( true );

		Vector vecForward;
		EyeVectors( &vecForward );
		if ( !bThrown &&
			FoFCarriedObjectLaunchObstruction(
				pPhysics, this, vecForward ) != vec3_origin )
		{
			Vector vecPosition;
			QAngle angPosition;
			pPhysics->GetPosition( &vecPosition, &angPosition );
			pPhysics->SetPosition(
				WorldSpaceCenter(), angPosition, true );
		}
	}

	// The reliable presentation cancellation precedes the network bool and
	// handle changes in the shipped server.
	EntityMessageBegin( this, true );
		WRITE_BYTE( 7 );
		WRITE_BYTE( bThrown ? 1 : 0 );
	MessageEnd();

	// A launched object must not inherit the grab controller's final correction
	// velocity.  The original drop path feeds its thrown flag directly to
	// DetachEntity; preserving that relationship prevents the object from first
	// accelerating back toward the carry target before the launch impulse wins.
	m_FoFGrabController.DetachEntity( bThrown );
	if ( pObject )
	{
		Pickup_OnPhysGunDrop( pObject, this, DROPPED_BY_CANNON );
		if ( m_bFoFResetPickupOwner &&
			pObject->GetOwnerEntity() == this )
		{
			pObject->SetOwnerEntity( NULL );
		}
	}

	FoFReportCourseStat( "drop_phys", this );
	vecPropCarryAngles.Init();
	m_bFoFResetPickupOwner = false;
	m_bPickupActive = false;
	m_hAttachedObject = NULL;
}

bool CFoF_Player::KickFoFCarriedObject()
{
	CBaseEntity *pObject = m_FoFGrabController.GetAttached();
	if ( !pObject )
		pObject = m_hAttachedObject.Get();
	if ( !m_bPickupActive || !pObject )
		return false;

	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	DropFoFCarriedObject( false, true );

	Vector vecForward;
	EyeVectors( &vecForward );
	FoFThrowCarriedPhysicsObject(
		pPhysics, 3.0f, vecForward, this );

	m_flNextPickupInteraction = gpGlobals->curtime + 0.25f;
	return true;
}

void CFoF_Player::UpdateFoFCarry()
{
	if ( !IsAlive() || IsObserver() || IsOnFoFHorse() )
	{
		DropFoFCarriedObject( false, false );
		return;
	}

	if ( m_bPickupActive )
	{
		CBaseEntity *pObject = m_FoFGrabController.GetAttached();
		if ( !pObject || pObject != m_hAttachedObject.Get() )
		{
			DropFoFCarriedObject( false, false );
			return;
		}

		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		const bool bWrongWeapon = pWeapon && pWeapon->FoFWeaponID() != 0;
		const bool bInteracting =
			( m_nButtons & ( IN_ATTACK | IN_ATTACK2 | IN_USE ) ) != 0;
		if ( bWrongWeapon ||
			( bInteracting &&
			  gpGlobals->curtime > m_flNextPickupInteraction ) )
		{
			const bool bThrow =
				( m_nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) != 0;
			IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
			DropFoFCarriedObject( false, bThrow );
			if ( bThrow )
			{
				Vector vecForward;
				EyeVectors( &vecForward );
				FoFThrowCarriedPhysicsObject(
					pPhysics, 1.0f, vecForward, this );

				IPhysicsObject *pPlayerPhysics = VPhysicsGetObject();
				if ( pPhysics && pPlayerPhysics )
				{
					PhysicsImpactSound(
						this, pPhysics, CHAN_STATIC,
						pPhysics->GetMaterialIndex(),
						pPlayerPhysics->GetMaterialIndex(),
						1.0f, 256.0f );
				}
			}
			m_flNextPickupInteraction = gpGlobals->curtime + 0.25f;
			return;
		}

		if ( !m_FoFGrabController.UpdateObject( this, 7.0f ) )
			DropFoFCarriedObject( false, false );
		return;
	}

	if ( !( m_nButtons & IN_USE ) ||
		gpGlobals->curtime <= m_flNextPickupInteraction )
	{
		return;
	}

	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon || pWeapon->FoFWeaponID() != 0 )
		return;

	Vector vecGrabPosition;
	CBaseEntity *pObject = FoFTracePickupObject( this, vecGrabPosition );
	if ( !pObject )
		return;

	if ( FoFIsRegularDynamite( pObject ) )
	{
		if ( FoFTryDefuseDynamite( this, pObject ) )
			m_flNextPickupInteraction = gpGlobals->curtime + 0.2f;
		return;
	}
	if ( FoFIsBlackDynamite( pObject ) )
	{
		m_flNextPickupInteraction = gpGlobals->curtime + 0.2f;
		pObject->SetOwnerEntity( this );
	}

	AttachFoFCarriedObject( pObject, vecGrabPosition );
}
