#ifndef FOF_PLAYER_LIST_H
#define FOF_PLAYER_LIST_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CBitmapButton;

namespace vgui
{
	class Button;
	class ImagePanel;
	class IScheme;
	class Label;
	class ListPanel;
}

class CFoFPlayerList : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFPlayerList, vgui::Frame );

public:
	CFoFPlayerList( vgui::Panel *parent );
	virtual ~CFoFPlayerList();
	void ShowDialog();
	void HideDialog();

	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnTick();
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void UpdateScaledFonts();
	void RefreshPlayers();
	void RefreshVoiceButton();
	void ToggleSelectedPlayer();
	void ToggleAllPlayers();
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	vgui::ImagePanel *m_pBackground;
	vgui::ListPanel *m_pPlayerList;
	vgui::Label *m_pTitle;
	vgui::Button *m_pVoiceControl;
	vgui::Button *m_pMuteAll;
	CBitmapButton *m_pOkay;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hControlFont;
	int m_iTitleFontTall;
	int m_iControlFontTall;
	bool m_bRefreshing;
};

#endif // FOF_PLAYER_LIST_H
