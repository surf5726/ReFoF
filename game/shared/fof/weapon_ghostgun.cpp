#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_ghostgun.h"
CGhostGun1::CGhostGun1() { m_bReloadsSingly = false; }
void CGhostGun1::FinishReload() { FinishTwoRoundReload(); }
bool CGhostGun1::IsSecondGun() const { return false; }
bool CGhostGun1::CanFan() const { return false; }
float CGhostGun1::PrimaryPenalty() const { return 0.7f; }
float CGhostGun1::SecondaryPenalty() const { return 1.9f; }
void CGhostGun1::PerformFoFReload() { ReloadTwoRoundClip(); }
bool CGhostGun1::NeedsPumpAfterShot() const { return false; }
bool CGhostGun1::UsesLoadedPrimaryInputPath() const { return true; }
int CGhostGun1::FinishStyle() const { return 3; }
QAngle CGhostGun1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, false ); }
QAngle CGhostGun1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( GhostGun1, DT_GhostGun1 )

BEGIN_NETWORK_TABLE( CGhostGun1, DT_GhostGun1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CGhostGun1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_ghostgun, CGhostGun1 )
#else
LINK_ENTITY_TO_CLASS( weapon_ghostgun, CGhostGun1 );
PRECACHE_WEAPON_REGISTER( weapon_ghostgun );
#endif

CGhostGun2::CGhostGun2() { m_bReloadsSingly = false; }
void CGhostGun2::FinishReload() { FinishTwoRoundReload(); }
bool CGhostGun2::IsSecondGun() const { return true; }
bool CGhostGun2::CanFan() const { return false; }
float CGhostGun2::PrimaryPenalty() const { return 0.7f; }
float CGhostGun2::SecondaryPenalty() const { return 1.9f; }
void CGhostGun2::PerformFoFReload() { ReloadTwoRoundClip(); }
bool CGhostGun2::NeedsPumpAfterShot() const { return false; }
bool CGhostGun2::UsesLoadedPrimaryInputPath() const { return true; }
int CGhostGun2::FinishStyle() const { return 3; }
QAngle CGhostGun2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, false ); }
QAngle CGhostGun2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( GhostGun2, DT_GhostGun2 )

BEGIN_NETWORK_TABLE( CGhostGun2, DT_GhostGun2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CGhostGun2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_ghostgun, CGhostGun2 )
#else
LINK_ENTITY_TO_CLASS( weapon_ghostgun2, CGhostGun2 );
PRECACHE_WEAPON_REGISTER( weapon_ghostgun2 );
#endif
