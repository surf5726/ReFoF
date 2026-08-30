#ifndef FOF_CREDITS_DIALOG_H
#define FOF_CREDITS_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui
{
	class Button;
	class IScheme;
	class RichText;
}

class CFoFCreditsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFCreditsDialog, vgui::Frame );

public:
	CFoFCreditsDialog();
	void ShowDialog();
	void HideDialog();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnCommand( const char *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void LoadCredits();
	vgui::RichText *m_pCredits;
	vgui::Button *m_pClose;
};

#endif // FOF_CREDITS_DIALOG_H
