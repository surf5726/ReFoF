#ifndef FOF_TEAM_CLASS_MENU_H
#define FOF_TEAM_CLASS_MENU_H
#ifdef _WIN32
#pragma once
#endif

class CHudFoF;

namespace vgui
{
	class Panel;
}

vgui::Panel *FoFCreateTeamClassPaintPanel( CHudFoF *owner );

#endif
