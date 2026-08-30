#ifndef FOF_CLIENT_AI_EDITOR_H
#define FOF_CLIENT_AI_EDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "inputsystem/ButtonCode.h"
#include "mathlib/vector.h"
#include "utlvector.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

struct FoFAIBotPreset
{
	char name[64];
	char equipment[64];
	int rotationSpeed;
	int shootDelay;
	int aimTrailing;
	int strafe;
	int forceTeam;
	int aggression;
};

struct FoFAIPlacedEntry
{
	bool prop;
	Vector origin;
	QAngle direction;
	FoFAIBotPreset bot;
	char entity[64];
	char data[128];
	int normalOffset;
	int type;
};

class CHudFoFAIEditor : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFAIEditor, vgui::Panel );

public:
	CHudFoFAIEditor( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void Paint();

	bool IsActive() const;
	void SetEditorActive( bool active );
	void ToggleEditor();
	bool RunSpawnScript( const char *filename );
	bool SaveSpawnScript( const char *filename ) const;

	int GetBotPresetCount() const;
	const FoFAIBotPreset *GetBotPreset( int index ) const;
	void SetBotPreset( int index, const FoFAIBotPreset &preset );
	void AddBotPreset( const FoFAIBotPreset &preset );
	void RemoveBotPreset( int index );
	bool LoadBotPresetFile( const char *filename );
	bool SaveBotPresetFile( const char *filename ) const;
	const char *GetBotPresetFilename() const;
	bool IsPropMode() const;
	void NotifyBotPresetsChanged();

private:
	void LoadBotPresets();
	void AddFallbackBotPresets();
	void UpdateLayout();
	void HandleInput();
	void SelectVisibleRow( int visibleRow );
	void SendSelection();
	void PlaceSelection();
	void RemoveAtCrosshair();
	void DrawPlacementPreview();
	void DrawBotEquipment(
		const FoFAIBotPreset &bot, int x, int y, int maxWide );
	int EnsureItemTexture( int itemId );
	int GetEntryCount() const;
	int GetMaxScroll() const;
	Color GetBotColor( int team ) const;

	CUtlVector< FoFAIBotPreset > m_Bots;
	CUtlVector< FoFAIPlacedEntry > m_PlacedEntries;
	vgui::HFont m_hTitleFont;
	vgui::HFont m_hRowFont;
	vgui::HFont m_hSmallFont;
	bool m_bActive;
	bool m_bPropMode;
	bool m_bShowHelp;
	int m_iSelected;
	int m_iScroll;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
	int m_iItemTextures[56];
	char m_szMapName[64];
	char m_szBotPresetFilename[MAX_PATH];

	bool m_bNumberDown[10];
	bool m_bMouseLeftDown;
	bool m_bMouseRightDown;
	bool m_bWheelUpDown;
	bool m_bWheelDownDown;
	bool m_bAltDown;
	bool m_bDeleteDown;
};

bool FoFAIEditorIsActive();
void FoFAIEditorClose();

namespace vgui
{
	class Button;
	class ComboBox;
	class TextEntry;
}

struct FoFAIProfileRowControls
{
	vgui::TextEntry *name;
	vgui::ComboBox *team;
	vgui::Button *skillToggle;
	vgui::ComboBox *skills[5];
	vgui::ComboBox *weapons[4];
	vgui::Button *remove;
};

class CNPCProfileManagement : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CNPCProfileManagement, vgui::Panel );

public:
	CNPCProfileManagement( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void Paint();
	virtual void OnCommand( const char *command );
	virtual void OnMouseWheeled( int delta );

	virtual void SetEditorVisible( bool visible );
	bool IsEditorVisible() const;

private:
	void UpdateLayout();
	void RefreshPresetList();
	void RefreshWaveList();
	void PopulateRows();
	void CommitRows();
	void SetRowVisible( int row, bool visible );
	void ShiftRows( int delta );
	void UpdatePresetSaveName( const char *filename );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", pData );

	vgui::HFont m_hTitleFont;
	vgui::HFont m_hTextFont;
	bool m_bVisible;
	bool m_bRefreshing;
	bool m_bRightWasDown;
	bool m_bRightReleasedSinceOpen;
	bool m_bSkillEditing[10];
	int m_iFirstRow;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
	char m_szPresetFilename[MAX_PATH];

	vgui::ComboBox *m_pPresets;
	vgui::TextEntry *m_pPresetSaveName;
	vgui::Button *m_pPresetSave;
	vgui::Button *m_pAddEmpty;
	vgui::ComboBox *m_pWaves;
	vgui::TextEntry *m_pWaveSaveName;
	vgui::Button *m_pWaveSave;
	FoFAIProfileRowControls m_Rows[10];
};

bool FoFAIProfileEditorIsActive();
void FoFAIProfileEditorOpen();
void FoFAIProfileEditorClose();

class Color;

int FoFEditorScale( float value );
bool FoFEditorCheatsEnabled();
void FoFEditorGetMapName( char *buffer, int bufferSize );
bool FoFEditorButtonPressed( ButtonCode_t code, bool &wasDown );
bool FoFEditorTracePlacement(
	float normalOffset, Vector &origin, QAngle &angles );
void FoFEditorClientCommand( const char *format, ... );
void FoFEditorDrawText(
	vgui::HFont font, const Color &color, int x, int y,
	const char *text );
void FoFEditorDrawRoundedRect(
	int x, int y, int wide, int tall, int radius, const Color &color );
void FoFEditorDrawLocalizedText(
	vgui::HFont font, const Color &color, int x, int y,
	const char *token, const char *fallback );
int FoFEditorDrawLocalizedWrappedText(
	vgui::HFont font, const Color &color, int x, int y, int maxWide,
	const char *token, const char *fallback );

#endif // FOF_CLIENT_AI_EDITOR_H
