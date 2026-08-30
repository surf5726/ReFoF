#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_sawedoff_shotgun.h"
CSawedShotgun1::CSawedShotgun1() { m_bReloadsSingly = false; }
void CSawedShotgun1::FinishReload() { FinishTwoRoundReload(); }
bool CSawedShotgun1::IsSecondGun() const { return false; }
bool CSawedShotgun1::CanFan() const { return false; }
float CSawedShotgun1::PrimaryPenalty() const { return 0.7f; }
float CSawedShotgun1::SecondaryPenalty() const { return 1.9f; }
void CSawedShotgun1::PerformFoFReload() { ReloadTwoRoundClip(); }
bool CSawedShotgun1::NeedsPumpAfterShot() const { return false; }
bool CSawedShotgun1::UsesLoadedPrimaryInputPath() const { return true; }
int CSawedShotgun1::FinishStyle() const { return 3; }
QAngle CSawedShotgun1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, false ); }
QAngle CSawedShotgun1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( SawedShotgun1, DT_SawedShotgun1 )

BEGIN_NETWORK_TABLE( CSawedShotgun1, DT_SawedShotgun1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CSawedShotgun1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_sawedoff_shotgun, CSawedShotgun1 )
#else
LINK_ENTITY_TO_CLASS( weapon_sawedoff_shotgun, CSawedShotgun1 );
PRECACHE_WEAPON_REGISTER( weapon_sawedoff_shotgun );
#endif

CSawedShotgun2::CSawedShotgun2() { m_bReloadsSingly = false; }
void CSawedShotgun2::FinishReload() { FinishTwoRoundReload(); }
bool CSawedShotgun2::IsSecondGun() const { return true; }
bool CSawedShotgun2::CanFan() const { return false; }
float CSawedShotgun2::PrimaryPenalty() const { return 0.7f; }
float CSawedShotgun2::SecondaryPenalty() const { return 1.9f; }
void CSawedShotgun2::PerformFoFReload() { ReloadTwoRoundClip(); }
bool CSawedShotgun2::NeedsPumpAfterShot() const { return false; }
bool CSawedShotgun2::UsesLoadedPrimaryInputPath() const { return true; }
int CSawedShotgun2::FinishStyle() const { return 3; }
QAngle CSawedShotgun2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, false ); }
QAngle CSawedShotgun2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_SHOTGUN, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( SawedShotgun2, DT_SawedShotgun2 )

BEGIN_NETWORK_TABLE( CSawedShotgun2, DT_SawedShotgun2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CSawedShotgun2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_sawedoff_shotgun, CSawedShotgun2 )
#else
LINK_ENTITY_TO_CLASS( weapon_sawedoff_shotgun2, CSawedShotgun2 );
PRECACHE_WEAPON_REGISTER( weapon_sawedoff_shotgun2 );
#endif
