#include "cbase.h"
#include "fof/fof_weapon_activities.h"
#ifdef CLIENT_DLL
#include "fof/fof_weapon_classmap.h"
#endif
#include "fof/weapon_whiskey.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CWhiskey1::IsSecondGun() const { return false; }

IMPLEMENT_NETWORKCLASS_ALIASED( Whiskey1, DT_Whiskey1 )

BEGIN_NETWORK_TABLE( CWhiskey1, DT_Whiskey1 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWhiskey1 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_whiskey, CWhiskey1 )
#else
LINK_ENTITY_TO_CLASS( weapon_whiskey, CWhiskey1 );
PRECACHE_WEAPON_REGISTER( weapon_whiskey );
#endif

bool CWhiskey2::IsSecondGun() const { return true; }

IMPLEMENT_NETWORKCLASS_ALIASED( Whiskey2, DT_Whiskey2 )

BEGIN_NETWORK_TABLE( CWhiskey2, DT_Whiskey2 )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWhiskey2 )
END_PREDICTION_DATA()

FOF_LINK_WEAPON_CLASS( weapon_whiskey, CWhiskey2 )
#else
LINK_ENTITY_TO_CLASS( weapon_whiskey2, CWhiskey2 );
PRECACHE_WEAPON_REGISTER( weapon_whiskey2 );
#endif
