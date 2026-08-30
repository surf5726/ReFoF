//========= Copyright Valve Corporation, All rights reserved. ============//
//
// FoF listen-server AI editor command protocol.
//
//=============================================================================//
#ifndef FOF_SERVER_AI_EDITOR_H
#define FOF_SERVER_AI_EDITOR_H
#ifdef _WIN32
#pragma once
#endif

class CCommand;
class CFoF_Player;

bool FoFHandleAIEditorCommand(
	CFoF_Player *pPlayer, const CCommand &args );
bool FoFLoadShootoutCustomPreset( const char *pszMapName );

#endif // FOF_SERVER_AI_EDITOR_H
