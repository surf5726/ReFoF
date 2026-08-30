#ifndef FOF_LAUNCHER_COURSES_H
#define FOF_LAUNCHER_COURSES_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>

struct FoFCourseEntry
{
	CUtlString script;
	CUtlString title;
	CUtlString category;
	CUtlString map;
	int maxPlayers;
	int group;
	int timesCompleted;
	int topAward;
};

class CFoFCourseButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFCourseButton, vgui::Button );

public:
	CFoFCourseButton(
		vgui::Panel *pParent,
		const FoFCourseEntry &entry,
		int index );
	virtual ~CFoFCourseButton();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();
	void SetCourseStats( int timesCompleted, int topAward );

private:
	CUtlString m_Title;
	int m_iTimesCompleted;
	int m_iTopAward;
	int m_iTexture;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hStatusFont;
};

#endif // FOF_LAUNCHER_COURSES_H
