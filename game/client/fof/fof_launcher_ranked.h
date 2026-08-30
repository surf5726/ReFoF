#ifndef FOF_LAUNCHER_RANKED_H
#define FOF_LAUNCHER_RANKED_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class Button;
	class HTML;
	class RichText;
}

class CFoFRankedPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CFoFRankedPanel, vgui::EditablePanel );

public:
	CFoFRankedPanel( vgui::Panel *pParent );
	virtual ~CFoFRankedPanel();

	void Open();
	void Close();
	bool IsOpen();
	void SetSeasonData(
		int season,
		int endDay,
		int endMonth,
		int endYear );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void Paint();
	virtual void OnCommand( const char *pszCommand );

private:
	void ReloadSeasonText();
	void FormatSeasonText(
		int season,
		int endDay,
		int endMonth,
		int endYear );
	void ShowRules( bool showRules );

	vgui::HTML *m_pLeaderboard;
	vgui::RichText *m_pRules;
	vgui::Button *m_pModeButton;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hInfoFont;
	bool m_bShowingRules;
	wchar_t m_wszTitle[192];
	wchar_t m_wszEnd[128];
};

#endif // FOF_LAUNCHER_RANKED_H
