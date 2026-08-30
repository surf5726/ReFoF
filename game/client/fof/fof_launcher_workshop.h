#ifndef FOF_LAUNCHER_WORKSHOP_H
#define FOF_LAUNCHER_WORKSHOP_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"

#include <vgui_controls/EditablePanel.h>

namespace vgui
{
	class Button;
	class SectionedListPanel;
}

class CFoFWorkshopPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CFoFWorkshopPanel, vgui::EditablePanel );

public:
	CFoFWorkshopPanel( vgui::Panel *pParent );
	virtual ~CFoFWorkshopPanel();

	void Open();
	void Close();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void OnCommand( const char *pszCommand );

private:
	void RefreshItems();
	void CancelQuery();
	void AddItem( const SteamUGCDetails_t &details );
	void OnQueryCompleted(
		SteamUGCQueryCompleted_t *pResult,
		bool bIOFailure );
	void OnItemInstalled( ItemInstalled_t *pResult );
	bool HasItem( PublishedFileId_t fileID ) const;
	void OpenItem( int itemID );

	MESSAGE_FUNC_INT( OnItemLeftClick, "ItemLeftClick", itemID );

	vgui::SectionedListPanel *m_pItemList;
	vgui::Button *m_pCloseButton;
	UGCQueryHandle_t m_hQuery;
	CCallResult< CFoFWorkshopPanel, SteamUGCQueryCompleted_t >
		m_QueryCall;
	CCallback< CFoFWorkshopPanel, ItemInstalled_t, false >
		m_ItemInstalledCallback;
	vgui::HFont m_hHeaderFont;
	vgui::HFont m_hItemFont;
};

#endif // FOF_LAUNCHER_WORKSHOP_H
