#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_peacemaker.h"
CPeacemaker1::CPeacemaker1() { m_bReloadsSingly = true; }
bool CPeacemaker1::IsSecondGun() const { return false; }
bool CPeacemaker1::CanFan() const { return true; }
float CPeacemaker1::PrimaryPenalty() const { return 0.7f; }
float CPeacemaker1::SecondaryPenalty() const { return 1.75f; }
bool CPeacemaker1::NeedsPumpAfterShot() const { return false; }
int CPeacemaker1::FinishStyle() const { return 0; }
QAngle CPeacemaker1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_PEACEMAKER, false ); }
QAngle CPeacemaker1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_PEACEMAKER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Peacemaker1, DT_Peacemaker1 )

BEGIN_NETWORK_TABLE( CPeacemaker1, DT_Peacemaker1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CPeacemaker1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_peacemaker, CPeacemaker1 )
#else
LINK_ENTITY_TO_CLASS( weapon_peacemaker, CPeacemaker1 );
PRECACHE_WEAPON_REGISTER( weapon_peacemaker );
#endif

CPeacemaker2::CPeacemaker2() { m_bReloadsSingly = true; }
bool CPeacemaker2::IsSecondGun() const { return true; }
bool CPeacemaker2::CanFan() const { return true; }
float CPeacemaker2::PrimaryPenalty() const { return 0.7f; }
float CPeacemaker2::SecondaryPenalty() const { return 1.75f; }
bool CPeacemaker2::NeedsPumpAfterShot() const { return false; }
int CPeacemaker2::FinishStyle() const { return 0; }
QAngle CPeacemaker2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_PEACEMAKER, false ); }
QAngle CPeacemaker2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_PEACEMAKER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Peacemaker2, DT_Peacemaker2 )

BEGIN_NETWORK_TABLE( CPeacemaker2, DT_Peacemaker2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CPeacemaker2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_peacemaker, CPeacemaker2 )
#else
LINK_ENTITY_TO_CLASS( weapon_peacemaker2, CPeacemaker2 );
PRECACHE_WEAPON_REGISTER( weapon_peacemaker2 );
#endif
