#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_hammerless.h"
CHammerless1::CHammerless1() { m_bReloadsSingly = false; }
void CHammerless1::FinishReload() { FinishFullClipReload(); }
bool CHammerless1::IsSecondGun() const { return false; }
bool CHammerless1::CanFan() const { return false; }
float CHammerless1::PrimaryPenalty() const { return 0.7f; }
float CHammerless1::SecondaryPenalty() const { return 1.9f; }
void CHammerless1::PerformFoFReload() { ReloadFullClip(); }
bool CHammerless1::NeedsPumpAfterShot() const { return false; }
int CHammerless1::FinishStyle() const { return 2; }
QAngle CHammerless1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_HAMMERLESS, false ); }
QAngle CHammerless1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_HAMMERLESS, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Hammerless1, DT_Hammerless1 )

BEGIN_NETWORK_TABLE( CHammerless1, DT_Hammerless1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CHammerless1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_hammerless, CHammerless1 )
#else
LINK_ENTITY_TO_CLASS( weapon_hammerless, CHammerless1 );
PRECACHE_WEAPON_REGISTER( weapon_hammerless );
#endif

CHammerless2::CHammerless2() { m_bReloadsSingly = false; }
void CHammerless2::FinishReload() { FinishFullClipReload(); }
bool CHammerless2::IsSecondGun() const { return true; }
bool CHammerless2::CanFan() const { return false; }
float CHammerless2::PrimaryPenalty() const { return 0.7f; }
float CHammerless2::SecondaryPenalty() const { return 1.9f; }
void CHammerless2::PerformFoFReload() { ReloadFullClip(); }
bool CHammerless2::NeedsPumpAfterShot() const { return false; }
int CHammerless2::FinishStyle() const { return 2; }
QAngle CHammerless2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_HAMMERLESS, false ); }
QAngle CHammerless2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_HAMMERLESS, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Hammerless2, DT_Hammerless2 )

BEGIN_NETWORK_TABLE( CHammerless2, DT_Hammerless2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CHammerless2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_hammerless, CHammerless2 )
#else
LINK_ENTITY_TO_CLASS( weapon_hammerless2, CHammerless2 );
PRECACHE_WEAPON_REGISTER( weapon_hammerless2 );
#endif
