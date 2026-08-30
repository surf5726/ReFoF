#ifndef FOF_VOICE_MENU_H
#define FOF_VOICE_MENU_H
#ifdef _WIN32
#pragma once
#endif

bool FoFVoiceMenuIsOpen();
bool FoFVoiceMenuSelectDisplaySlot( int slot );
bool FoFVoiceMenuClose();
void FoFVoiceMenuToggle( int kind );

#endif
