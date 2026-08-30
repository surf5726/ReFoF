#ifndef FOF_COURSE_MODE_H
#define FOF_COURSE_MODE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "utlvector.h"

class CFoF_Player;
class CBasePlayer;
class CTakeDamageInfo;

// The original AI-editor spawn path marks course props with this private
// server-side solid flag. Course weapons use it to require an explicit +use
// without affecting weapons granted directly to a player.
enum
{
	FOF_COURSE_USE_PICKUP_SOLID_FLAG = 0x0800,
	FOF_COURSE_USE_PROMPT_EFFECT = ( 1 << 11 )
};

enum FoFCourseInstructionType_t
{
	FOF_COURSE_PLAYER_SPAWN = 0,
	FOF_COURSE_WAVE_SPAWN,
	FOF_COURSE_WAVE_SPAWN_DYNAMIC,
	FOF_COURSE_OBJECT_SPAWN,
	FOF_COURSE_MESSAGE,
	FOF_COURSE_GOTO,
	FOF_COURSE_STOP,
	FOF_COURSE_CHECK_ENEMIES,
	FOF_COURSE_CHECK_LOCATION,
	FOF_COURSE_CHECK_CAPTURE,
	FOF_COURSE_CHECK_TIMER,
	FOF_COURSE_CHECK_TRAIN,
	FOF_COURSE_CHECK_WAIT,
	FOF_COURSE_GAME_END,
	FOF_COURSE_ENTITY_IO,
	FOF_COURSE_CHECK_SINGLEPLAYER,
	FOF_COURSE_DISABLE_CHECK,
	FOF_COURSE_GIVE_EQUIPMENT,
	FOF_COURSE_CHECK_BOT_ORDERS,
	FOF_COURSE_PLAY_AUDIO,
	FOF_COURSE_CHALLENGE_END,
	FOF_COURSE_INSTRUCTION_COUNT
};

struct FoFCourseInstruction_t
{
	int m_nType;
	char m_szEntry[32];
	char m_szData[256];
	char m_szValue[32];
	char m_szOutput[32];
	Vector m_vecOrigin;
	Vector m_vecDirection;
	float m_flEntryTime;
	int m_nGotoRemaining;
	bool m_bNumericEntry;
};

struct FoFCourseActiveCheck_t
{
	int m_nInstruction;
	float m_flStartedAt;
	float m_flTriggerAt;
	float m_flProgress;
	float m_flLastUpdate;
	int m_nCheckValue;
	int m_nStatId;
	int m_nStatRemaining;
	int m_nStatPending;
	bool m_bOwnsHint;
	EHANDLE m_hVisual;
};

struct FoFCourseBotSpawn_t
{
	int m_nRotationSpeed;
	int m_nShootDelay;
	int m_nAimTrailing;
	int m_nStrafe;
	int m_nForceTeam;
	int m_nAggression;
	char m_szName[64];
	char m_szEquipment[128];
	bool m_bDynamic;
	float m_flDynamicRadius;
	Vector m_vecDynamicCenter;
	Vector m_vecOrigin;
	Vector m_vecDirection;
};

struct FoFCourseChoice_t
{
	char m_szScript[MAX_PATH];
	char m_szLabel[128];
	int m_nVotes;
};

class CCourseMode : public CBaseEntity
{
public:
	DECLARE_CLASS( CCourseMode, CBaseEntity );
	DECLARE_DATADESC();

	CCourseMode();
	virtual void Spawn();
	virtual void UpdateOnRemove();
	void Update();
	void ReportStat( const char *pszStat );
	bool HandleEndMenuSelection( CFoF_Player *pPlayer, int nSelection );
	bool HandleCourseSelection( CFoF_Player *pPlayer, int nSelection );
	void ReportEnemyKilled() { ++m_nEnemiesKilled; }
	bool ShouldBlockRespawn( const CFoF_Player *pPlayer ) const;
	int GetMaxPlayers() const { return m_nMaxPlayers; }
	int GetBotOrder( const Vector &origin,
		Vector &orderOrigin, float &orderRadius ) const;
	int GetCoursePlayerTeam() const;
	bool AreBotTeamsAllied(
		int nFirstTeam, int nSecondTeam ) const;
	bool IsImpactMarkerActive() const;
	bool ShouldCourseCompanionsFollowPlayer() const;
	bool HasActiveTrainCheck() const;
	bool GetActiveCaptureZone( Vector &origin, float &radius ) const;

private:
	bool LoadConfiguredCourse();
	bool LoadCourseScript( const char *pszScript );
	void FindCourseChoices();
	void BeginCourseSelection();
	void UpdateCourseSelection();
	void CloseCourseSelection();
	void ResetRuntimeState( bool bRemoveSpawnedEntities );
	void CleanupMapCourseEntities( bool bRemoveWhiskey );
	CFoF_Player *FindCoursePlayer() const;
	void StartCourse( CFoF_Player *pPlayer );
	void UpdateCourseCompanions();
	void RestartCourse( bool bResetScores = true );
	void RespawnCoursePlayers( bool bResetScores );
	void EndCourse( bool bSuccess );
	void UpdateEndState();
	void UpdateEndVote();
	void CloseEndMenu();
	void ExecuteEndMenuSelection( int nSelection );
	void UpdateSchedule();
	void UpdateChecks();
	void UpdatePendingBotSpawns();
	void ExecuteInstruction( int nInstruction, int nCaller );
	void DispatchOutput( const char *pszOutput, int nCaller );
	void AddCheck( int nInstruction );
	void RemoveCheck( int nCheck, bool bRemoveMarker );
	bool EvaluateCheck( FoFCourseActiveCheck_t &check );
	void ExecutePlayerSpawn( const FoFCourseInstruction_t &instruction );
	void SpawnWave( const FoFCourseInstruction_t &instruction, bool bDynamic );
	void SpawnObjects( const FoFCourseInstruction_t &instruction );
	void ExecuteEntityIO( const FoFCourseInstruction_t &instruction );
	void GiveEquipment( const FoFCourseInstruction_t &instruction );
	void ExecuteGoto( int nInstruction );
	void ExecuteChallengeEnd( const FoFCourseInstruction_t &instruction );
	void SendCourseHint( const char *pszTitle,
		const char *pszBody, int nMode ) const;
	void SendNotice( const char *pszToken,
		const char *pszArgument1 = "", int nKind = 1 ) const;
	void SendEquipmentItem( CFoF_Player *pPlayer, int nItem ) const;
	void SendLocationMarker( const FoFCourseInstruction_t &instruction,
		bool bAdd ) const;
	int CountAliveCourseBots() const;

	CUtlVector< FoFCourseInstruction_t > m_Instructions;
	CUtlVector< FoFCourseActiveCheck_t > m_ActiveChecks;
	CUtlVector< FoFCourseBotSpawn_t > m_PendingBotSpawns;
	CUtlVector< EHANDLE > m_SpawnedEntities;
	CHandle< CBaseEntity > m_hPlayerSpawn;
	CHandle< CFoF_Player > m_hCoursePlayer;
	char m_szLoadedScript[MAX_PATH];
	float m_flNextLoadAttempt;
	float m_flNextUpdate;
	float m_flStartTime;
	float m_flNextScheduledAt;
	float m_flEndMenuAt;
	int m_nNextScheduledInstruction;
	int m_nPlayerTeam;
	int m_nMaxPlayers;
	int m_nBotAlliance;
	int m_nTotalEnemies;
	int m_nEnemiesKilled;
	int m_nAward;
	int m_nDispatchBudget;
	bool m_bLoaded;
	bool m_bStarted;
	bool m_bEnded;
	bool m_bScheduleStopped;
	bool m_bEndMenuShown;
	bool m_bPlayerSpawnExecuted;
	char m_szConfiguredScript[MAX_PATH];
	CUtlVector< FoFCourseChoice_t > m_CourseChoices;
	CUtlVector< EHANDLE > m_CourseMenuPlayers;
	float m_flCourseVoteEnd;
	bool m_bCourseSelectionPending;
	bool m_bCourseVoteOpen;
	int m_nSelectedCourse;
	int m_nEndVotes[3];
	int m_nPendingEndSelection;
	float m_flEndVoteDeadline;
	bool m_bPrivateListenSession;
	char m_szPrivatePassword[128];
};

void FoFUpdateCourseMode();
bool FoFStartCourseMode( const char *pszMapName );
bool FoFHasConfiguredCourseForMap();
int FoFGetCourseMaxPlayers();
bool FoFShouldDrawCourseImpactMarker( const CBasePlayer *pShooter );
bool FoFShouldBlockCourseRespawn( const CFoF_Player *pPlayer );
bool FoFCourseBotsAreAllied( int nFirstTeam, int nSecondTeam );
bool FoFLoadEliminationSafeZoneCenters( CUtlVector< Vector > &centers );
void FoFReportCourseStat( const char *pszStat, CBaseEntity *pActor );
void FoFReportCourseKill(
	CBasePlayer *pScorer,
	CBasePlayer *pVictim,
	const CTakeDamageInfo &info );
bool FoFHandleCourseEndMenuSelection(
	CFoF_Player *pPlayer, int nSelection );
bool FoFHandleCourseSelection(
	CFoF_Player *pPlayer, int nSelection );

#endif // FOF_COURSE_MODE_H
