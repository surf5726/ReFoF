#ifndef FOF_PLAYER_EQUIPMENT_H
#define FOF_PLAYER_EQUIPMENT_H
#ifdef _WIN32
#pragma once
#endif

class CFoF_Player;
class CBaseCombatWeapon;
class CBasePlayer;

// Server-authoritative FoF item catalogue used by course scripts and Versus
// arena loadouts.  The client only presents these IDs; inventory mutation
// remains server-only.
int FoFEquipmentItemId( const char *pszToken );
void FoFApplyEquipmentItem( CFoF_Player *pPlayer, int nItem );
void FoFGiveEquipmentList(
	CFoF_Player *pPlayer, const char *pszItems, bool bSendHud,
	bool bApplyInReverse = false );
void FoFSendEquipmentItem( CFoF_Player *pPlayer, int nItem );
void FoFResetEquipmentState( CFoF_Player *pPlayer );
bool FoFGetTeamClassDefinition(
	int nClass, char *pszName, int nNameSize, float &flShare );
void FoFApplyTeamClass( CFoF_Player *pPlayer, int nClass );

#endif // FOF_PLAYER_EQUIPMENT_H
