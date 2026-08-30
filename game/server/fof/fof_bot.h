//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF server bot player entity.
//
//=============================================================================//
#ifndef FOF_BOT_H
#define FOF_BOT_H
#ifdef _WIN32
#pragma once
#endif

#include "fof/fof_player.h"

class CFoFBot : public CFoF_Player
{
public:
	DECLARE_CLASS( CFoFBot, CFoF_Player );

	CFoFBot();
	virtual void Spawn();
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual bool Weapon_Switch(
		CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual void DelayFoFNextAction( void );
	virtual void RefreshFoFEquipment( void );
	virtual void ActivateFoFInvulnerability( void );
	virtual void SelectFoFEquipment( void );
	void SetFoFGhost( bool bGhost );

private:
	bool m_bFoFGhost;
};

struct FoFBotProfile_t
{
	FoFBotProfile_t();

	int m_nRotationSpeed;
	int m_nShootDelay;
	int m_nAimTrailing;
	int m_nStrafe;
	int m_nForceTeam;
	int m_nAggression;
	char m_szName[64];
	char m_szEquipment[128];
};

CFoFBot *FoFPutBotInServer( bool bFrozen, int nInitialHealth,
	const char *pszRequestedName = NULL,
	bool bPopulationManaged = false );
CFoFBot *FoFPutConfiguredBotInServer(
	const FoFBotProfile_t &profile,
	const Vector &origin, const Vector &direction );
CFoFBot *FoFPutCourseCompanionInServer(
	int nTeam, const Vector &origin, const QAngle &angles,
	int nRemaining );
CFoFBot *FoFPutGhostBotInServer();
bool FoFLoadBotProfile( int nProfile, FoFBotProfile_t &profile );
void FoFConfigureBotRuntime( CFoFBot *pBot,
	const FoFBotProfile_t *pProfile, bool bPopulationManaged );
void FoFResetBotSpawnState( CFoFBot *pBot );
void FoFUpdateBotEquipment( CFoFBot *pBot );
bool FoFGetBotProfileEquipment(
	CFoFBot *pBot, int *pItems, int nItemCount );
void FoFResetBotCombatState(
	CFoFBot *pBot, float flAttackDelay, bool bClearTarget );
void FoFClearBotTarget( CFoFBot *pBot );
void FoFDelayBotNextAction( CFoFBot *pBot, float flDelay );
void FoFNotifyBotDamaged(
	CFoFBot *pBot, CFoF_Player *pAttacker );
void FoFSetBotDynamiteThrowVelocity(
	CFoF_Player *pPlayer, const Vector &vecVelocity );
bool FoFConsumeBotDynamiteThrowVelocity(
	CFoF_Player *pPlayer, Vector &vecVelocity );
bool FoFIsCourseCompanionBot( const CFoF_Player *pPlayer );
void FoFRunBots();
void FoFUpdateBotPopulation();
void FoFUpdateGhostTownBots();

#endif // FOF_BOT_H
