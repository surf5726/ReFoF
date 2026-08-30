#ifndef FOF_COURSE_EDITOR_H
#define FOF_COURSE_EDITOR_H

#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "mathlib/vector.h"
#include "utlvector.h"
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Button;
	class ComboBox;
	class TextEntry;
}

struct FoFCourseEditorInstruction
{
	int type;
	char entryLabel[32];
	char typeName[32];
	char data[256];
	char value[32];
	char output[32];
	bool hasData;
	bool hasValue;
	bool hasOutput;
	Vector origin;
	QAngle angles;
};

class CHudFoFCourseEditor : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFCourseEditor, vgui::Panel );

public:
	CHudFoFCourseEditor( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void Paint();
	virtual void OnCommand( const char *command );

	bool IsActive() const;
	void SetEditorActive( bool active );
	void ToggleEditor();
	bool SaveCourse( const char *filename );
	bool LoadCourse( const char *filename );

private:
	void UpdateLayout();
	void HandleInput();
	void SelectVisibleRow( int visibleRow );
	void AddInstruction();
	void DrawPlacementPreview();
	void DrawPlacedInstructions();
	void UpdateDefaultFilename();
	void RefreshCourseList();
	void UpdateSaveLoadControls();
	void LayoutSaveLoadControls();
	void SetPanelEditing( bool editing );
	void NormalizeFilename(
		const char *input, char *output, int outputSize ) const;
	Color GetInstructionColor( int type ) const;

	CUtlVector< FoFCourseEditorInstruction > m_Instructions;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hRowFont;
	vgui::HFont m_hSmallFont;
	bool m_bActive;
	bool m_bSaveLoadVisible;
	bool m_bPanelEditing;
	bool m_bShowHelp;
	int m_iSelected;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
	char m_szMapName[64];
	char m_szFilename[MAX_PATH];
	vgui::ComboBox *m_pCourseList;
	vgui::TextEntry *m_pFilenameEntry;
	vgui::Button *m_pLoadButton;
	vgui::Button *m_pSaveButton;
	vgui::ComboBox *m_pHumanTeam;
	vgui::ComboBox *m_pMaxPlayers;
	vgui::ComboBox *m_pBotAlliance;
	vgui::TextEntry *m_pTotalEnemies;

	bool m_bNumberDown[10];
	bool m_bMouseLeftDown;
	bool m_bMouseRightDown;
	bool m_bWheelUpDown;
	bool m_bWheelDownDown;
	bool m_bAltDown;
	bool m_bDeleteDown;
	bool m_bInsertDown;
};

bool FoFCourseEditorIsActive();
void FoFCourseEditorClose();

#endif // FOF_COURSE_EDITOR_H
