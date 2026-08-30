#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_coltnavy.h"

CColtNavy1::CColtNavy1() { m_bReloadsSingly = true; }
bool CColtNavy1::IsSecondGun() const { return false; }
bool CColtNavy1::CanFan() const { return true; }
float CColtNavy1::PrimaryPenalty() const { return 0.5f; }
float CColtNavy1::SecondaryPenalty() const { return 1.25f; }
bool CColtNavy1::NeedsPumpAfterShot() const { return false; }
int CColtNavy1::FinishStyle() const { return 0; }
QAngle CColtNavy1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_COLT, false ); }
QAngle CColtNavy1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_COLT, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( ColtNavy1, DT_ColtNavy1 )

BEGIN_NETWORK_TABLE( CColtNavy1, DT_ColtNavy1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CColtNavy1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_coltnavy, CColtNavy1 )
#else
LINK_ENTITY_TO_CLASS( weapon_coltnavy, CColtNavy1 );
PRECACHE_WEAPON_REGISTER( weapon_coltnavy );
#endif

CColtNavy2::CColtNavy2() { m_bReloadsSingly = true; }
bool CColtNavy2::IsSecondGun() const { return true; }
bool CColtNavy2::CanFan() const { return true; }
float CColtNavy2::PrimaryPenalty() const { return 0.5f; }
float CColtNavy2::SecondaryPenalty() const { return 1.25f; }
bool CColtNavy2::NeedsPumpAfterShot() const { return false; }
int CColtNavy2::FinishStyle() const { return 0; }
QAngle CColtNavy2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_COLT, false ); }
QAngle CColtNavy2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_COLT, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( ColtNavy2, DT_ColtNavy2 )

BEGIN_NETWORK_TABLE( CColtNavy2, DT_ColtNavy2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CColtNavy2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_coltnavy, CColtNavy2 )
#else
LINK_ENTITY_TO_CLASS( weapon_coltnavy2, CColtNavy2 );
PRECACHE_WEAPON_REGISTER( weapon_coltnavy2 );
#endif
