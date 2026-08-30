#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_schofield.h"
CSchofield1::CSchofield1() { m_bReloadsSingly = false; }
void CSchofield1::FinishReload() { FinishFullClipReload(); }
void CSchofield1::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CSchofield1::IsSecondGun() const { return false; }
bool CSchofield1::CanFan() const { return true; }
float CSchofield1::PrimaryPenalty() const { return 0.7f; }
float CSchofield1::SecondaryPenalty() const { return 1.9f; }
void CSchofield1::PerformFoFReload() { ReloadFullClip(); }
bool CSchofield1::NeedsPumpAfterShot() const { return false; }
int CSchofield1::FinishStyle() const { return 2; }
QAngle CSchofield1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, false ); }
QAngle CSchofield1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Schofield1, DT_Schofield1 )

BEGIN_NETWORK_TABLE( CSchofield1, DT_Schofield1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CSchofield1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_schofield, CSchofield1 )
#else
LINK_ENTITY_TO_CLASS( weapon_schofield, CSchofield1 );
PRECACHE_WEAPON_REGISTER( weapon_schofield );
#endif

CSchofield2::CSchofield2() { m_bReloadsSingly = false; }
void CSchofield2::FinishReload() { FinishFullClipReload(); }
void CSchofield2::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CSchofield2::IsSecondGun() const { return true; }
bool CSchofield2::CanFan() const { return true; }
float CSchofield2::PrimaryPenalty() const { return 0.7f; }
float CSchofield2::SecondaryPenalty() const { return 1.9f; }
void CSchofield2::PerformFoFReload() { ReloadFullClip(); }
bool CSchofield2::NeedsPumpAfterShot() const { return false; }
int CSchofield2::FinishStyle() const { return 2; }
QAngle CSchofield2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, false ); }
QAngle CSchofield2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_REMINGTON_SCHOFIELD, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Schofield2, DT_Schofield2 )

BEGIN_NETWORK_TABLE( CSchofield2, DT_Schofield2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CSchofield2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_schofield, CSchofield2 )
#else
LINK_ENTITY_TO_CLASS( weapon_schofield2, CSchofield2 );
PRECACHE_WEAPON_REGISTER( weapon_schofield2 );
#endif
