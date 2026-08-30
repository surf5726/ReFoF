#ifndef C_TE_PLAYERANIMEVENT_H
#define C_TE_PLAYERANIMEVENT_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basetempentity.h"

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEPlayerAnimEvent();
	virtual void PostDataUpdate( DataUpdateType_t updateType );

private:
	EHANDLE m_hPlayer;
	int m_iEvent;
	int m_nData;
};

#endif // C_TE_PLAYERANIMEVENT_H
