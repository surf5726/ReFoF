#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_maresleg.h"
CMaresLeg1::CMaresLeg1() { m_bReloadsSingly = true; }
void CMaresLeg1::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CMaresLeg1::IsSecondGun() const { return false; }
bool CMaresLeg1::CanFan() const { return true; }
float CMaresLeg1::PrimaryPenalty() const { return 1.2f; }
float CMaresLeg1::SecondaryPenalty() const { return 1.7f; }
bool CMaresLeg1::NeedsPumpAfterShot() const { return false; }
int CMaresLeg1::FinishStyle() const { return 0; }
QAngle CMaresLeg1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MARESLEG, false ); }
QAngle CMaresLeg1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MARESLEG, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( MaresLeg1, DT_MaresLeg1 )

BEGIN_NETWORK_TABLE( CMaresLeg1, DT_MaresLeg1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CMaresLeg1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_maresleg, CMaresLeg1 )
#else
LINK_ENTITY_TO_CLASS( weapon_maresleg, CMaresLeg1 );
PRECACHE_WEAPON_REGISTER( weapon_maresleg );
#endif

CMaresLeg2::CMaresLeg2() { m_bReloadsSingly = true; }
void CMaresLeg2::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CMaresLeg2::IsSecondGun() const { return true; }
bool CMaresLeg2::CanFan() const { return true; }
float CMaresLeg2::PrimaryPenalty() const { return 1.2f; }
float CMaresLeg2::SecondaryPenalty() const { return 1.4f; }
bool CMaresLeg2::NeedsPumpAfterShot() const { return false; }
int CMaresLeg2::FinishStyle() const { return 0; }
QAngle CMaresLeg2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MARESLEG, false ); }
QAngle CMaresLeg2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_MARESLEG, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( MaresLeg2, DT_MaresLeg2 )

BEGIN_NETWORK_TABLE( CMaresLeg2, DT_MaresLeg2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CMaresLeg2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_maresleg, CMaresLeg2 )
#else
LINK_ENTITY_TO_CLASS( weapon_maresleg2, CMaresLeg2 );
PRECACHE_WEAPON_REGISTER( weapon_maresleg2 );
#endif
