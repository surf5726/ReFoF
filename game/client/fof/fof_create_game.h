#ifndef FOF_CREATE_GAME_DIALOG_H
#define FOF_CREATE_GAME_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class KeyValues;

namespace vgui
{
	class CheckButton;
	class ComboBox;
	class ImagePanel;
	class IScheme;
	class TextEntry;
}

class CFoFCreateGameDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFCreateGameDialog, vgui::Frame );

public:
	CFoFCreateGameDialog();
	virtual ~CFoFCreateGameDialog();

	void ShowDialog();
	void HideDialog();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnCommand( const char *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void PopulateModes();
	void PopulateMaps( int iMode );
	void PopulateBotAmounts();
	void PopulateBotSkills();
	void PopulateBotScripts();
	void PopulateCourseScripts();
	void PopulateTeamCounts();
	void UpdateServerSlots( int iMode );
	void UpdateModeControls( int iMode );
	void UpdateMapImage();
	void LoadServerConfig();
	void SaveServerConfig();
	void ApplyImmediateSetting( vgui::Panel *pPanel );
	void StartListenServer();

	int GetSelectedInt(
		vgui::ComboBox *pCombo,
		const char *pszKey,
		int iFallback ) const;
	const char *GetSelectedString(
		vgui::ComboBox *pCombo,
		const char *pszKey,
		const char *pszFallback ) const;
	int FindItemByInt(
		vgui::ComboBox *pCombo,
		const char *pszKey,
		int iValue ) const;
	void SelectRow( vgui::ComboBox *pCombo, int iRow, int iFallbackRow );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", pData );

	vgui::ImagePanel *m_pMapImage;
	vgui::ComboBox *m_pMapList;
	vgui::ComboBox *m_pModesList;
	vgui::ComboBox *m_pServerSlots;
	vgui::TextEntry *m_pServerName;
	vgui::TextEntry *m_pServerPass;
	vgui::ComboBox *m_pBotsAllowed;
	vgui::ComboBox *m_pBotsCustomScript;
	vgui::ComboBox *m_pBotsSkill;
	vgui::CheckButton *m_pBotDynamic;
	vgui::ComboBox *m_pCourseScripts;
	vgui::TextEntry *m_pDuration;
	vgui::CheckButton *m_pTeamplay;
	vgui::ComboBox *m_pTeamNumber;
	KeyValues *m_pServerConfig;
	bool m_bUpdatingControls;
};

#endif // FOF_CREATE_GAME_DIALOG_H
