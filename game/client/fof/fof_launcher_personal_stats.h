#ifndef FOF_LAUNCHER_PERSONAL_STATS_H
#define FOF_LAUNCHER_PERSONAL_STATS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "steam/steam_api.h"

#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class Button;
}

struct FoFPersonalStatsPoint
{
	char m_szLabel[40];
	char m_szValue[48];
	float m_flValue;
};

class CFoFPersonalStatsPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CFoFPersonalStatsPanel, vgui::EditablePanel );

public:
	CFoFPersonalStatsPanel();
	virtual ~CFoFPersonalStatsPanel();

	void Open();
	void Close();
	bool IsOpen();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void Paint();
	virtual void OnCommand( const char *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void SelectMetric( int metric );
	void RequestStats();
	void ReloadStats();
	void OnUserStatsReceived(
		UserStatsReceived_t *pResult,
		bool bIOFailure );
	void DrawSummary();
	void DrawGraph();

	CUtlVector< vgui::Button * > m_MetricButtons;
	CUtlVector< FoFPersonalStatsPoint > m_Points;
	CUtlVector< FoFPersonalStatsPoint > m_SecondaryPoints;
	vgui::Button *m_pCloseButton;
	vgui::HFont m_hSummaryFont;
	vgui::HFont m_hGraphFont;
	int m_iMetric;
	int m_iSummaryLines;
	wchar_t m_wszSummary[2][256];
	CCallResult< CFoFPersonalStatsPanel, UserStatsReceived_t >
		m_UserStatsCall;
};

#endif // FOF_LAUNCHER_PERSONAL_STATS_H
