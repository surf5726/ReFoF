#ifndef HL2MP_PLAYERANIMSTATE_H
#define HL2MP_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "Multiplayer/multiplayer_animstate.h"

class CHL2MPPlayerAnimState : public CMultiPlayerAnimState
{
public:
	CHL2MPPlayerAnimState(
		CBasePlayer *pPlayer,
		MultiPlayerMovementData_t &movementData )
		: CMultiPlayerAnimState( pPlayer, movementData )
		, m_pFoFPlayer( pPlayer )
	{
	}

	virtual void DoAnimationEvent(
		PlayerAnimEvent_t event, int nData = 0 );
	virtual Activity TranslateActivity( Activity activity );

private:
	virtual int SelectWeightedSequence( Activity activity );
	virtual bool HandleJumping( Activity &idealActivity );
	virtual bool HandleDucking( Activity &idealActivity );
	virtual bool HandleMoving( Activity &idealActivity );
	virtual bool HandleSwimming( Activity &idealActivity );
	virtual void ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );

	CBasePlayer *m_pFoFPlayer;
};

CHL2MPPlayerAnimState *CreateFoFPlayerAnimState( CBasePlayer *pPlayer );

#endif // HL2MP_PLAYERANIMSTATE_H
