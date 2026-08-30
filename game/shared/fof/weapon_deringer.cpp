#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_deringer.h"
CDeringer1::CDeringer1() { m_bReloadsSingly = false; }
void CDeringer1::FinishReload() { FinishDeringerReload(); }
bool CDeringer1::IsSecondGun() const { return false; }
bool CDeringer1::CanFan() const { return false; }
float CDeringer1::PrimaryPenalty() const { return 1.9f; }
float CDeringer1::SecondaryPenalty() const { return 1.6f; }
void CDeringer1::PerformFoFReload() { ReloadDeringerClip(); }
bool CDeringer1::NeedsPumpAfterShot() const { return true; }
int CDeringer1::FinishStyle() const { return 1; }
QAngle CDeringer1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_DERINGER, false ); }
QAngle CDeringer1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_DERINGER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Deringer1, DT_Deringer1 )

BEGIN_NETWORK_TABLE( CDeringer1, DT_Deringer1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CDeringer1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_deringer, CDeringer1 )
#else
LINK_ENTITY_TO_CLASS( weapon_deringer, CDeringer1 );
PRECACHE_WEAPON_REGISTER( weapon_deringer );
#endif

CDeringer2::CDeringer2() { m_bReloadsSingly = false; }
void CDeringer2::FinishReload() { FinishDeringerReload(); }
bool CDeringer2::IsSecondGun() const { return true; }
bool CDeringer2::CanFan() const { return false; }
float CDeringer2::PrimaryPenalty() const { return 1.9f; }
float CDeringer2::SecondaryPenalty() const { return 1.6f; }
void CDeringer2::PerformFoFReload() { ReloadDeringerClip(); }
bool CDeringer2::NeedsPumpAfterShot() const { return true; }
int CDeringer2::FinishStyle() const { return 1; }
QAngle CDeringer2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_DERINGER_SECOND, false ); }
QAngle CDeringer2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_DERINGER_SECOND, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Deringer2, DT_Deringer2 )

BEGIN_NETWORK_TABLE( CDeringer2, DT_Deringer2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CDeringer2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_deringer, CDeringer2 )
#else
LINK_ENTITY_TO_CLASS( weapon_deringer2, CDeringer2 );
PRECACHE_WEAPON_REGISTER( weapon_deringer2 );
#endif
