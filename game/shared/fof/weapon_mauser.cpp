#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_mauser.h"
CMauser1::CMauser1() { m_bReloadsSingly = false; }
bool CMauser1::IsSecondGun() const { return false; }
bool CMauser1::CanFan() const { return false; }
float CMauser1::PrimaryPenalty() const { return 0.5f; }
float CMauser1::SecondaryPenalty() const { return 1.25f; }
bool CMauser1::NeedsPumpAfterShot() const { return false; }
int CMauser1::FinishStyle() const { return 0; }
QAngle CMauser1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MAUSER, false ); }
QAngle CMauser1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MAUSER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Mauser1, DT_Mauser1 )

BEGIN_NETWORK_TABLE( CMauser1, DT_Mauser1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CMauser1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_mauser, CMauser1 )
#else
LINK_ENTITY_TO_CLASS( weapon_mauser, CMauser1 );
PRECACHE_WEAPON_REGISTER( weapon_mauser );
#endif

CMauser2::CMauser2() { m_bReloadsSingly = false; }
bool CMauser2::IsSecondGun() const { return true; }
bool CMauser2::CanFan() const { return false; }
float CMauser2::PrimaryPenalty() const { return 0.5f; }
float CMauser2::SecondaryPenalty() const { return 1.25f; }
bool CMauser2::NeedsPumpAfterShot() const { return false; }
int CMauser2::FinishStyle() const { return 0; }
QAngle CMauser2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MAUSER, false ); }
QAngle CMauser2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MAUSER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Mauser2, DT_Mauser2 )

BEGIN_NETWORK_TABLE( CMauser2, DT_Mauser2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CMauser2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_mauser, CMauser2 )
#else
LINK_ENTITY_TO_CLASS( weapon_mauser2, CMauser2 );
PRECACHE_WEAPON_REGISTER( weapon_mauser2 );
#endif
