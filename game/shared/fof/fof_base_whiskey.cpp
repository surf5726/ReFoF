#include "cbase.h"
#include "fof/fof_player_shared.h"
#include "fof/fof_weapon_activities.h"
#include "fof/fof_base_whiskey.h"
#include "fof/fof_weapon_properties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CBaseWhiskey::CBaseWhiskey()
{
	m_bReloadsSingly = false;
}

void CBaseWhiskey::Precache()
{
	PrecacheScriptSound( "Whiskey.Glug" );
	BaseClass::Precache();
}

IMPLEMENT_NETWORKCLASS_ALIASED( BaseWhiskey, DT_BaseWhiskey )
BEGIN_NETWORK_TABLE( CBaseWhiskey, DT_BaseWhiskey )
END_NETWORK_TABLE()

void CBaseWhiskey::PrimaryAttack()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || !( gpGlobals->curtime > LastAttackTime() + 0.5f ) )
		return;

	if ( IsSecondGun() )
		WhiskeyAttack( true );
	else if ( !GetOtherRevolver( pOwner ) )
		WhiskeyAttack( false );
}

void CBaseWhiskey::SecondaryAttack()
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner || IsSecondGun() || !GetOtherRevolver( pOwner ) ||
		!( gpGlobals->curtime > LastAttackTime() + 0.5f ) )
	{
		return;
	}

	WhiskeyAttack( false );
}

void CBaseWhiskey::WhiskeyAttack( bool bSecondHand )
{
	CFoF_Player *pOwner = dynamic_cast< CFoF_Player * >( GetOwner() );
	if ( !pOwner ||
		FoFWeaponHasActualReloadPresentation( pOwner->GetActiveWeapon1() ) ||
		FoFWeaponHasActualReloadPresentation( pOwner->GetActiveWeapon2() ) )
		return;

	Vector vecForward;
	pOwner->EyeVectors( &vecForward, NULL, NULL );
	const Vector vecStart = pOwner->Weapon_ShootPosition();
	Vector vecEnd = vecStart + vecForward * 150.0f;
	trace_t traceHit;
	pOwner->EmitSound( "Player.WhiskeyMovement" );
	UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	bool bHit = false;
	if ( traceHit.fraction < 1.0f )
	{
		if ( traceHit.m_pEnt && ( traceHit.m_pEnt->IsPlayer() || traceHit.m_pEnt->IsNPC() ) )
			bHit = true;
	}
	if ( traceHit.fraction == 1.0f )
	{
		vecEnd -= vecForward * 17.32f;
		UTIL_TraceHull(
			vecStart,
			vecEnd,
			Vector( -10.0f, -10.0f, -10.0f ),
			Vector( 10.0f, 10.0f, 10.0f ),
			MASK_SHOT_HULL,
			pOwner,
			COLLISION_GROUP_NONE,
			&traceHit );
		if ( traceHit.fraction < 1.0f && traceHit.m_pEnt )
		{
			Vector vecTargetDirection = traceHit.m_pEnt->GetAbsOrigin() - vecStart;
			VectorNormalize( vecTargetDirection );
			if ( DotProduct( vecTargetDirection, vecForward ) < 0.70721f )
				traceHit.fraction = 1.0f;
			else
				bHit = true;
		}
	}

#ifdef GAME_DLL
	if ( bHit && traceHit.m_pEnt )
	{
		CBaseCombatCharacter *pTarget =
			traceHit.m_pEnt->MyCombatCharacterPointer();
		CFoF_Player *pTargetPlayer = ToFoFPlayer( traceHit.m_pEnt );
		if ( pTarget &&
			( !pTargetPlayer ||
			  pTargetPlayer->GetTeamNumber() == pOwner->GetTeamNumber() ) )
		{
			const int nHealthMissing = 100 - pTarget->GetHealth();
			const int nHealthGiven = MIN( nHealthMissing, m_iClip1.Get() );
			if ( nHealthGiven > 0 )
			{
				m_iClip1 -= nHealthGiven;
				pTarget->TakeHealth(
					static_cast< float >( nHealthGiven ), DMG_GENERIC );
				pOwner->EmitSound( "Whiskey.Glug" );

				if ( pTargetPlayer )
				{
					static ConVarRef currentMode(
						"fof_sv_currentmode", true );
					const float flMaximumOwnerHealth =
						currentMode.IsValid() && currentMode.GetInt() == 3 ?
						10.0f : 15.0f;
					const int nOwnerHealth = RoundFloatToInt(
						RemapValClamped(
							static_cast< float >( nHealthGiven ),
							0.0f, 25.0f, 1.0f, flMaximumOwnerHealth ) );
					pOwner->TakeHealth(
						static_cast< float >( nOwnerHealth ), DMG_GENERIC );

					const int nDrunkard = RoundFloatToInt(
						RemapValClamped(
							static_cast< float >( nHealthGiven ),
							0.0f, 25.0f, 5.0f, 15.0f ) );
					pOwner->m_flDrunkness += nDrunkard * 0.25f;
					pOwner->AddFoFDrunkardAmount( nDrunkard );
				}
			}
		}
	}
#endif

	// FoF's original activity table is shifted by one relative to this SDK in
	// a remote client, while a listen server may initialize the shared model
	// cache with the original table first.  Resolve the activity on the live
	// model so both process layouts select the same whiskey sequence and idle
	// duration as the server.
	const Activity attackActivity = bHit ?
		FoFModelActivity( this, "ACT_VM_HITCENTER", ACT_VM_HITCENTER ) :
		FoFModelActivity( this, "ACT_VM_MISSCENTER", ACT_VM_MISSCENTER );
	SendWeaponAnim( attackActivity );
	pOwner->SetAnimation( static_cast< PLAYER_ANIM >( bSecondHand ? 5 : 6 ) );
	pOwner->DoAnimationEvent(
		bSecondHand ? PLAYERANIMEVENT_ATTACK_PRIMARY : PLAYERANIMEVENT_ATTACK_SECONDARY,
		0 );

	SetLastAttackTime( gpGlobals->curtime );
}
