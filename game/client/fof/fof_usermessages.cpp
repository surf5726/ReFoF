#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_player_status.h"
#include "fof/fof_hud_mode_status.h"
#include "fof/fof_hints.h"
#include "fof/fof_usermessages.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "cdll_client_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void FoFUserMessage_ShowMenuFoF( bf_read &msg )
{
	char label[256];
	msg.ReadString( label, sizeof( label ) );
	const bool more = msg.ReadByte() != 0;
	const int commandId = (short)msg.ReadShort();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveMenuLine( label, more, commandId );
}

void FoFUserMessage_FoFHint( bf_read &msg )
{
	char text[128];
	msg.ReadString( text, sizeof( text ) );
	const int mode = msg.ReadByte();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveHint( text, mode );
}

void FoFUserMessage_Cash( bf_read &msg )
{
	const int cash = msg.ReadLong();
	CHudCash *hud = GET_HUDELEMENT( CHudCash );
	if ( hud ) hud->ReceiveCash( cash );
}

void FoFUserMessage_Notoriety( bf_read &msg )
{
	const int mode = msg.ReadByte();
	const int value = msg.ReadLong();
	const int eventCode = msg.ReadByte();
	char text[256];
	msg.ReadString( text, sizeof( text ) );
	CHudFoFNotoriety *hud = GET_HUDELEMENT( CHudFoFNotoriety );
	if ( hud ) hud->ReceiveNotoriety( mode, value, eventCode, text );
}

void FoFUserMessage_FoFSlide( bf_read &msg )
{
	char name[256];
	msg.ReadString( name, sizeof( name ) );
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveSlide( name );
	engine->ClientCmd( "spec_mode" );
}

void FoFUserMessage_HudEquipItems( bf_read &msg )
{
	const int item = (short)msg.ReadShort();
	FoFHudEquipItemReceive( item );
}

void FoFUserMessage_CourseHint( bf_read &msg )
{
	char title[256], body[256];
	msg.ReadString( title, sizeof( title ) );
	msg.ReadString( body, sizeof( body ) );
	const int mode = msg.ReadByte();
	FoFCourseHintReceive( title, body, mode );
}

void FoFUserMessage_MaxHP( bf_read &msg )
{
	const int maxHP = msg.ReadByte();
	CHudFoFHealth *health = GET_HUDELEMENT( CHudFoFHealth );
	if ( health ) health->ReceiveMaxHP( maxHP );
}

void FoFUserMessage_IconComm( bf_read &msg )
{
	const int playerIndex = msg.ReadByte();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveIconComm( playerIndex );
}

void FoFUserMessage_HudCircleProgressBar( bf_read &msg )
{
	const float endTime = msg.ReadFloat();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveCircleProgress( endTime );
}

void FoFUserMessage_CapMessage( bf_read &msg )
{
	const int count = msg.ReadByte();
	const int mode = msg.ReadByte();
	const int progress = msg.ReadByte();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveCapMessage( count, mode, progress );
}

void FoFUserMessage_HitRecon( bf_read &msg )
{
	const int damage = msg.ReadByte();
	Vector position;
	msg.ReadBitVec3Coord( position );
	const int weaponIndex = msg.ReadByte();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveHitRecon( damage, position, weaponIndex );
}

void FoFUserMessage_HitReconRank( bf_read &msg )
{
	const float rank = msg.ReadFloat();
	Vector position;
	msg.ReadBitVec3Coord( position );
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveHitReconRank( rank, position );
}

void FoFUserMessage_HUDBBMulti( bf_read &msg )
{
	const int operation = msg.ReadByte();
	const int lifetime = msg.ReadByte();
	const int iconType = msg.ReadByte();
	Vector position;
	msg.ReadBitVec3Coord( position );
	char text[128];
	msg.ReadString( text, sizeof( text ) );
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveBBMulti( operation, lifetime, iconType, position, text );
}

void FoFUserMessage_BBNotices( bf_read &msg )
{
	const int kind = msg.ReadByte();
	char format[256], argument1[256], argument2[256], argument3[256];
	msg.ReadString( format, sizeof( format ) );
	msg.ReadString( argument1, sizeof( argument1 ) );
	msg.ReadString( argument2, sizeof( argument2 ) );
	msg.ReadString( argument3, sizeof( argument3 ) );
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveBBNotice( kind, format, argument1, argument2, argument3 );
}

void FoFUserMessage_HitBow( bf_read &msg )
{
	const int result = msg.ReadByte();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveHitBow( result );
}

void FoFUserMessage_GoodBadYou( bf_read &msg )
{
	const float value0 = msg.ReadFloat();
	const int value1 = msg.ReadLong();
	const int value2 = msg.ReadLong();
	const int value3 = msg.ReadLong();
	const float value4 = msg.ReadFloat();
	const int value5 = msg.ReadLong();
	const int value6 = msg.ReadLong();
	const int value7 = msg.ReadLong();
	const float value8 = msg.ReadFloat();
	const float value9 = msg.ReadFloat();
	const float value10 = msg.ReadFloat();
	const float value11 = msg.ReadFloat();
	CHudFoF *hud = FoFHud();
	if ( hud ) hud->ReceiveGoodBadYou( value0, value1, value2, value3,
		value4, value5, value6, value7, value8, value9, value10, value11 );
}
