#ifndef FOF_TEAM_MENU_H
#define FOF_TEAM_MENU_H
#ifdef _WIN32
#pragma once
#endif

// Replicated rules and faction-layout queries shared by the HUD coordinator
// and the team-menu presentation.
int FoFHudCurrentMode();
bool FoFHudTeamplayEnabled();
int FoFHudTeamFactionCount();
int FoFHudBuildTeamFactionList( int factionIds[4] );
int FoFHudTeamFactionMask();
int FoFHudTeamFactionOrdinal( int team );
const char *FoFHudTeamIntroSlideName();
const char *FoFHudTeamIntroLabel();

// Team-button resources are owned by this module.  Callers use indexed
// accessors so the backing tables remain private to the team menu.
const char *FoFHudTeamButtonMaterial( int buttonIndex );
const char *FoFHudTeamButtonCommand( int buttonIndex );

#endif // FOF_TEAM_MENU_H
