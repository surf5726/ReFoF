#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_remington_army.h"
CRemington_Army::CRemington_Army() { m_bReloadsSingly = false; }
void CRemington_Army::FinishReload() { FinishFullClipReload(); }
void CRemington_Army::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CRemington_Army::IsSecondGun() const { return false; }
bool CRemington_Army::CanFan() const { return true; }
float CRemington_Army::PrimaryPenalty() const { return 0.7f; }
float CRemington_Army::SecondaryPenalty() const { return 1.9f; }
void CRemington_Army::PerformFoFReload() { ReloadFullClip(); }
bool CRemington_Army::NeedsPumpAfterShot() const { return false; }
int CRemington_Army::FinishStyle() const { return 2; }
QAngle CRemington_Army::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, false ); }
QAngle CRemington_Army::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Remington_Army, DT_Remington_Army )

BEGIN_NETWORK_TABLE( CRemington_Army, DT_Remington_Army )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CRemington_Army )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_remington_army, CRemington_Army )
#else
LINK_ENTITY_TO_CLASS( weapon_remington_army, CRemington_Army );
PRECACHE_WEAPON_REGISTER( weapon_remington_army );
#endif

CRemington_Army2::CRemington_Army2() { m_bReloadsSingly = false; }
void CRemington_Army2::FinishReload() { FinishFullClipReload(); }
void CRemington_Army2::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CRemington_Army2::IsSecondGun() const { return true; }
bool CRemington_Army2::CanFan() const { return true; }
float CRemington_Army2::PrimaryPenalty() const { return 0.7f; }
float CRemington_Army2::SecondaryPenalty() const { return 1.9f; }
void CRemington_Army2::PerformFoFReload() { ReloadFullClip(); }
bool CRemington_Army2::NeedsPumpAfterShot() const { return false; }
int CRemington_Army2::FinishStyle() const { return 2; }
QAngle CRemington_Army2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, false ); }
QAngle CRemington_Army2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Remington_Army2, DT_Remington_Army2 )

BEGIN_NETWORK_TABLE( CRemington_Army2, DT_Remington_Army2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CRemington_Army2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_remington_army, CRemington_Army2 )
#else
LINK_ENTITY_TO_CLASS( weapon_remington_army2, CRemington_Army2 );
PRECACHE_WEAPON_REGISTER( weapon_remington_army2 );
#endif
