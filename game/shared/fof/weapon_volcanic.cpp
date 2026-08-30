#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_volcanic.h"
CVolcanic1::CVolcanic1() { m_bReloadsSingly = true; }
void CVolcanic1::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CVolcanic1::IsSecondGun() const { return false; }
bool CVolcanic1::CanFan() const { return true; }
float CVolcanic1::PrimaryPenalty() const { return 0.4f; }
float CVolcanic1::SecondaryPenalty() const { return 0.7f; }
bool CVolcanic1::NeedsPumpAfterShot() const { return false; }
int CVolcanic1::FinishStyle() const { return 0; }
QAngle CVolcanic1::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_VOLCANIC, false ); }
QAngle CVolcanic1::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_VOLCANIC, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Volcanic1, DT_Volcanic1 )

BEGIN_NETWORK_TABLE( CVolcanic1, DT_Volcanic1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVolcanic1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_volcanic, CVolcanic1 )
#else
LINK_ENTITY_TO_CLASS( weapon_volcanic, CVolcanic1 );
PRECACHE_WEAPON_REGISTER( weapon_volcanic );
#endif

CVolcanic2::CVolcanic2() { m_bReloadsSingly = true; }
void CVolcanic2::WeaponIdle() { AlternateSightWeaponIdle(); }
bool CVolcanic2::IsSecondGun() const { return true; }
bool CVolcanic2::CanFan() const { return true; }
float CVolcanic2::PrimaryPenalty() const { return 0.4f; }
float CVolcanic2::SecondaryPenalty() const { return 0.7f; }
bool CVolcanic2::NeedsPumpAfterShot() const { return false; }
int CVolcanic2::FinishStyle() const { return 0; }
QAngle CVolcanic2::PrimaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_VOLCANIC_SECOND, false ); }
QAngle CVolcanic2::SecondaryViewPunch() const { return FoFBuildRevolverPunch( FOF_REVOLVER_PUNCH_VOLCANIC_SECOND, true ); }

IMPLEMENT_NETWORKCLASS_ALIASED( Volcanic2, DT_Volcanic2 )

BEGIN_NETWORK_TABLE( CVolcanic2, DT_Volcanic2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CVolcanic2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_volcanic, CVolcanic2 )
#else
LINK_ENTITY_TO_CLASS( weapon_volcanic2, CVolcanic2 );
PRECACHE_WEAPON_REGISTER( weapon_volcanic2 );
#endif
