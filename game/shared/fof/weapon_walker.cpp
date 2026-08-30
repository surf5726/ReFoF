#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_walker.h"
CWalker1::CWalker1() { m_bReloadsSingly = true; }
bool CWalker1::IsSecondGun() const { return false; }
bool CWalker1::CanFan() const { return false; }
float CWalker1::PrimaryPenalty() const { return 1.5f; }
float CWalker1::SecondaryPenalty() const { return 2.0f; }
bool CWalker1::NeedsPumpAfterShot() const { return false; }
int CWalker1::FinishStyle() const { return 0; }
QAngle CWalker1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_WALKER, false ); }
QAngle CWalker1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_WALKER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Walker1, DT_Walker1 )

BEGIN_NETWORK_TABLE( CWalker1, DT_Walker1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWalker1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_walker, CWalker1 )
#else
LINK_ENTITY_TO_CLASS( weapon_walker, CWalker1 );
PRECACHE_WEAPON_REGISTER( weapon_walker );
#endif

CWalker2::CWalker2() { m_bReloadsSingly = true; }
bool CWalker2::IsSecondGun() const { return true; }
bool CWalker2::CanFan() const { return false; }
float CWalker2::PrimaryPenalty() const { return 1.5f; }
float CWalker2::SecondaryPenalty() const { return 2.0f; }
bool CWalker2::NeedsPumpAfterShot() const { return false; }
int CWalker2::FinishStyle() const { return 0; }
QAngle CWalker2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_WALKER, false ); }
QAngle CWalker2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_WALKER, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Walker2, DT_Walker2 )

BEGIN_NETWORK_TABLE( CWalker2, DT_Walker2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWalker2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_walker, CWalker2 )
#else
LINK_ENTITY_TO_CLASS( weapon_walker2, CWalker2 );
PRECACHE_WEAPON_REGISTER( weapon_walker2 );
#endif
