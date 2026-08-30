#ifndef FOF_TRIGGERS_H
#define FOF_TRIGGERS_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"

class CBuyZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBuyZone, CBaseTrigger );
	DECLARE_DATADESC();

	CBuyZone();
	virtual void Spawn();
	virtual void InputEnable( inputdata_t &inputdata );
	virtual void InputDisable( inputdata_t &inputdata );
	void BuyZoneTouch( CBaseEntity *pOther );

private:
	int m_iFoFTeam;
	bool m_bFoFEnabled;
};

class CDynamiteRemover : public CBaseTrigger
{
public:
	DECLARE_CLASS( CDynamiteRemover, CBaseTrigger );
	DECLARE_DATADESC();

	CDynamiteRemover();
	virtual void Spawn();
	virtual void InputEnable( inputdata_t &inputdata );
	virtual void InputDisable( inputdata_t &inputdata );
	void DynamiteTouch( CBaseEntity *pOther );

private:
	int m_iFoFTeam;
	bool m_bFoFEnabled;
};

class CTriggerSpeed : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerSpeed, CBaseTrigger );
	DECLARE_DATADESC();

	CTriggerSpeed();
	virtual void Spawn();
	virtual void InputEnable( inputdata_t &inputdata );
	virtual void InputDisable( inputdata_t &inputdata );
	void TriggerSpeedTouch( CBaseEntity *pOther );

private:
	COutputEvent m_OnMinSpeedReached;
	COutputEvent m_OnMaxSpeedReached;
	bool m_bFoFEnabled;
	float m_flMinSpeed;
	float m_flMaxSpeed;
	float m_flSpeedSamples[30];
	int m_nNextSpeedSample;
};

class CTriggerHurtFoF : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerHurtFoF, CBaseTrigger );
	DECLARE_DATADESC();

	CTriggerHurtFoF();
	virtual void Spawn();
	virtual void Touch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );

	void RadiationThink();
	void HurtThink();
	bool HurtEntity( CBaseEntity *pOther, float flDamage );
	int HurtAllTouchers( float flDeltaTime );

private:
	float m_flOriginalDamage;
	float m_flDamage;
	float m_flDamageCap;
	float m_flLastDmgTime;
	float m_flDmgResetTime;
	int m_bitsDamageInflict;
	int m_damageModel;
	bool m_bNoDmgForce;
	bool m_bOnlyKicked;
	int m_nTeamFilter;
	EHANDLE m_hLastKickedEntity;
	COutputEvent m_OnHurt;
	COutputEvent m_OnHurtPlayer;
	CUtlVector< EHANDLE > m_hurtEntities;

	enum
	{
		DAMAGEMODEL_NORMAL = 0,
		DAMAGEMODEL_DOUBLE_FORGIVENESS,
	};
};

#endif // FOF_TRIGGERS_H
