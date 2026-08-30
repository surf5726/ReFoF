//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include "shake.h"
#include "voice_gamemgr.h"

#ifdef CLIENT_DLL
#include "fof/fof_usermessages.h"
#define REGISTER_FOF_USER_MESSAGE( name ) \
	do { \
		usermessages->Register( #name, -1 ); \
		usermessages->HookMessage( #name, FoFUserMessage_##name ); \
	} while ( 0 )
#else
#define REGISTER_FOF_USER_MESSAGE( name ) \
	usermessages->Register( #name, -1 )
#endif

// NVNT include to register in haptic user messages
#include "haptics/haptic_msgs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void RegisterUserMessages( void )
{
	usermessages->Register( "Geiger", 1 );
	usermessages->Register( "Train", 1 );
	usermessages->Register( "HudText", -1 );
	usermessages->Register( "SayText", -1 );
	usermessages->Register( "SayText2", -1 );
	usermessages->Register( "TextMsg", -1 );
	usermessages->Register( "HudMsg", -1 );
	usermessages->Register( "ResetHUD", 1);		// called every respawn
	usermessages->Register( "GameTitle", 0 );
	usermessages->Register( "ItemPickup", -1 );
	usermessages->Register( "ShowMenu", -1 );
	usermessages->Register( "Shake", 13 );
	usermessages->Register( "Fade", 10 );
	usermessages->Register( "VGUIMenu", -1 );	// Show VGUI menu
	usermessages->Register( "Rumble", 3 );	// Send a rumble to a controller
	usermessages->Register( "Battery", 2 );
	usermessages->Register( "Damage", 18 );		// BUG: floats are sent for coords, no variable bitfields in hud & fixed size Msg
	usermessages->Register( "VoiceMask", VOICE_MAX_PLAYERS_DW*4 * 2 + 1 );
	usermessages->Register( "RequestState", 0 );
	usermessages->Register( "CloseCaption", -1 ); // Show a caption (by string id number)(duration in 10th of a second)
	usermessages->Register( "HintText", -1 );	// Displays hint text display
	usermessages->Register( "KeyHintText", -1 );	// Displays hint text display
	usermessages->Register( "SquadMemberDied", 0 );
	usermessages->Register( "AmmoDenied", 2 );
	usermessages->Register( "CreditsMsg", 1 );
	usermessages->Register( "LogoTimeMsg", 4 );
	usermessages->Register( "AchievementEvent", -1 );
	usermessages->Register( "UpdateJalopyRadar", -1 );

	REGISTER_FOF_USER_MESSAGE( IconComm );
	REGISTER_FOF_USER_MESSAGE( HudCircleProgressBar );
	REGISTER_FOF_USER_MESSAGE( CapMessage );
	REGISTER_FOF_USER_MESSAGE( ShowMenuFoF );
	REGISTER_FOF_USER_MESSAGE( HitRecon );
	REGISTER_FOF_USER_MESSAGE( HitReconRank );
	REGISTER_FOF_USER_MESSAGE( FoFHint );
	REGISTER_FOF_USER_MESSAGE( Cash );
	REGISTER_FOF_USER_MESSAGE( Notoriety );
	REGISTER_FOF_USER_MESSAGE( HUDBBMulti );
	REGISTER_FOF_USER_MESSAGE( BBNotices );
	REGISTER_FOF_USER_MESSAGE( FoFSlide );
	REGISTER_FOF_USER_MESSAGE( HitBow );
	REGISTER_FOF_USER_MESSAGE( GoodBadYou );
	REGISTER_FOF_USER_MESSAGE( HudEquipItems );
	REGISTER_FOF_USER_MESSAGE( CourseHint );
	REGISTER_FOF_USER_MESSAGE( MaxHP );

#ifndef _X360
	// NVNT register haptic user messages
	RegisterHapticMessages();
#endif
}

#undef REGISTER_FOF_USER_MESSAGE
