#include "cbase.h"
#include "fof/c_fof_player.h"
#include "fof/c_te_playeranimevent.h"
#include "hl2mp/hl2mp_playeranimstate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_TEPlayerAnimEvent::C_TEPlayerAnimEvent()
	: m_iEvent( 0 )
	, m_nData( 0 )
{
}

void C_TEPlayerAnimEvent::PostDataUpdate( DataUpdateType_t updateType )
{
	C_BaseEntity *pEntity = m_hPlayer.Get();
	C_FoF_Player *pPlayer = dynamic_cast< C_FoF_Player * >( pEntity );
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		pPlayer->DoAnimationEvent(
			static_cast< PlayerAnimEvent_t >( m_iEvent ), m_nData );
	}
}

IMPLEMENT_CLIENTCLASS_EVENT(
	C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );
BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) ),
END_RECV_TABLE()
