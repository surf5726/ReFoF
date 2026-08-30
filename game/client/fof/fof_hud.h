#ifndef FOF_HUD_H
#define FOF_HUD_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "fof/fof_hud_menu.h"
#include "fof/fof_hud_transient.h"
#include "fof/fof_purchase_menu.h"
#include "fof/fof_hud_slide.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include <vgui_controls/EditablePanel.h>

class CBitmapButton;
class C_BaseCombatWeapon;
class C_BasePlayer;
class CAvatarImage;
class CHudTexture;

namespace vgui
{
	class Button;
	class CircularProgressBar;
	class ComboBox;
	class Frame;
	class IScheme;
	class Label;
	class TextEntry;
}

class CFoFEquipmentHelpFrame;
class CFoFLoadoutEditorPaintPanel;
class CFoFTeamClassPaintPanel;

// Main FoF HUD coordinator.  Network message names and payloads remain in the
// user-message module; this type owns only presentation state and interaction.
class CHudFoF : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudFoF, vgui::EditablePanel );

public:
	CHudFoF( const char *elementName );
	virtual ~CHudFoF();

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void FireGameEvent( IGameEvent *event );
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	bool HandleMenuKeyInput( int down, int keynum );

	void ReceiveMenuLine( const char *label, bool more, int commandId );
	bool IsMenuOpen() const { return m_bMenuVisible; }
	bool BlocksPlayerMovement() const
	{
		return m_bMenuVisible && m_bLocalMenu &&
			( m_iMenuKind == FOF_MENU_TEAM_CLASS ||
				m_iMenuKind == FOF_MENU_EQUIPMENT ||
				m_iMenuKind == FOF_MENU_PURCHASE );
	}
	bool IsHintVisible() const { return m_bHintVisible; }
	bool IsSlideOpen() const { return m_bSlideVisible || m_bSlidePending; }
	bool StepSlide( int direction );
	void CloseSlide();
	bool SelectDisplaySlot( int slot );
	void ReceiveHint( const char *text, int mode );
	void ReceiveSlide( const char *name );
	void ReceiveIconComm( int playerIndex );
	void ReceiveCircleProgress( float endTime );
	void ReceiveCapMessage( int count, int mode, int progress );
	void ReceiveBBMulti( int operation, int lifetime, int iconType, const Vector &position, const char *text );
	void ReceiveBBNotice( int kind, const char *format, const char *argument1, const char *argument2, const char *argument3 );
	void ReceiveGoodBadYou( float value0, int value1, int value2, int value3,
		float value4, int value5, int value6, int value7,
		float value8, float value9, float value10, float value11 );
	void ReceiveHitRecon( int damage, const Vector &position, int weaponIndex );
	void ReceiveHitReconRank( float rank, const Vector &position );
	void ReceiveHitBow( int result );
	void ReceiveStatUpdate( const char *label, int newValue, int oldValue );
	void ShowTeamMenu();
	void SetTeamMenuVisible( bool visible );
	void SetTeamClassMenuVisible( bool visible );
	void ShowEquipmentMenu( int page = 0 );
	void SetEquipmentMenuVisible( bool visible );
	void ToggleAutoBuyMenu();
	void RebuyEquipment();
	void CloseMenu() { ClearMenu(); }

private:
	friend class CFoFEquipmentPaintPanel;
	friend class CFoFEquipmentHelpFrame;
	friend class CFoFPurchasePaintPanel;
	friend class CFoFLoadoutEditorPaintPanel;
	friend class CFoFTeamClassPaintPanel;

	const wchar_t *Localize( const char *text, wchar_t *buffer, int bufferBytes ) const;
	void DrawWide( const wchar_t *text, int x, int y, const Color &color, bool centered = false ) const;
	void DrawWrappedWide( const wchar_t *text, vgui::HFont font, int x, int y,
		int maxWide, int maxTall, const Color &color, int align ) const;
	void DrawAnsi( const char *text, int x, int y, const Color &color, bool centered = false ) const;
	void PaintBBNotices();
	void PaintSourceTVPlayerInfo();
	void PaintMenu();
	void ClearMenu();
	void EnsureMenuControls();
	void EnsureMenuTextures();
	void UpdateTeamIntroFont();
	void UpdateEquipmentHelpTitleFont();
	void LayoutMenuControls();
	void SetMenuControlsVisible( bool visible );
	void BeginLocalMenuInput();
	void PaintTeamMenu();
	void ShowTeamClassMenu();
	void EnsureTeamClassMenuControls();
	void EnsureTeamClassMenuTextures();
	void ApplyTeamClassMenuScheme( vgui::IScheme *scheme );
	void LayoutTeamClassMenuControls();
	void SetTeamClassMenuControlsVisible( bool visible );
	void LoadTeamClassDefinitions();
	void UpdateTeamClassButtonStates();
	void PaintTeamClassMenu();
	bool IsTeamClassAvailable( int classIndex ) const;
	void PaintEquipmentMenu();
	void PaintPurchaseMenu();
	void ShowEquipmentHelp( int catalogueIndex );
	void CloseEquipmentHelp();
	void PaintTextMenu();
	void PaintCrateMenu();
	void DrawMenuTexture( int textureId, int x, int y, int wide, int tall,
		const Color &color = Color( 255, 255, 255, 255 ) ) const;
	void LoadEquipmentSelection();
	void SaveEquipmentSelection();
	void ToggleEquipmentItem( int catalogueIndex );
	bool IsEquipmentItemSelected( int itemId ) const;
	int EquipmentPointTotal() const;
	bool RemoveFirstEquipmentGearForBudget( int protectedItemId );
	bool MakeEquipmentBudgetRoom( int additionalPoints, int protectedItemId );
	void AcceptEquipmentSelection();
	void SubmitEquipment( const int *items, int count );
	void ShowPurchaseMenu( int page );
	void ShowPurchaseMenu( int page, bool quickLayout );
	void EnsurePurchaseMenuControls();
	void EnsurePurchaseMenuTextures();
	void ApplyPurchaseMenuScheme( vgui::IScheme *scheme );
	void LayoutPurchaseMenuControls();
	void SetPurchaseMenuControlsVisible( bool visible );
	void PopulatePurchaseLoadoutFiles();
	MESSAGE_FUNC_PARAMS(
		OnPurchaseLoadoutChanged, "TextChanged", data );
	void LoadPurchasePresets();
	void UpdatePurchasePresetPrices();
	void UpdatePurchaseButtonStates();
	void SelectPurchasePreset( int visibleSlot );
	void ChangePurchasePage( int direction );
	void TogglePurchaseLayout();
	void SetPurchaseFilter( int filter );
	void ToggleLoadoutEditorMode();
	void OpenLoadoutEditor( int presetIndex );
	void OpenNewLoadoutEditor();
	void CloseLoadoutEditor();
	void EnsureLoadoutEditorControls();
	void ApplyLoadoutEditorScheme( vgui::IScheme *scheme );
	void LayoutLoadoutEditorControls();
	void SetLoadoutEditorControlsVisible( bool visible );
	void PaintLoadoutEditor();
	void AddLoadoutEditorItem( int itemId );
	void RemoveLoadoutEditorItem( int slot );
	void SaveLoadoutEditor();
	void DeleteLoadoutEditor();
	bool SaveLoadoutEditorPresets();
	int FindLoadoutEditorPreset() const;
	int LoadoutEditorPrice() const;
	int PurchaseFilteredPresetCount() const;
	int PurchasePresetIndexForVisibleSlot( int visibleSlot ) const;
	int PurchaseVisiblePresetCount() const;
	bool PurchasePresetMatchesFilter( const FoFPurchasePreset &preset ) const;
	void EnsureCircleProgressBar();
	void LayoutCircleProgressBar();
	void UpdateCircleProgressBar();
	bool LoadSlide( const char *name );
	void ClearSlideItems();
	void LayoutSlideVideos();
	void LayoutSlideControls();
	void SetSlideControlsVisible( bool visible );
	void PaintSlide();
	void BuildGoodBadRanks();
	void EnsureGoodBadAvatars();
	void PaintGoodBad();
	bool IsCaptureMessageVisible() const;
	void PaintCaptureMessage();
	void PaintCaptureMarkers();

	vgui::HFont m_hFont;
	vgui::HFont m_hSmallFont;
	vgui::HFont m_hHintFont;
	vgui::HFont m_hCaptureFont;
	vgui::HFont m_hBBNoticeFont;
	vgui::HFont m_hBBMarkerFont;
	vgui::HFont m_hTeamIntroFont;
	int m_iTeamIntroFontTall;
	vgui::HFont m_hTeamClassFont;
	vgui::HFont m_hHitReconFont;
	vgui::HFont m_hSourceTVPlayerFont;
	vgui::HFont m_hEquipmentCreditFont;
	vgui::HFont m_hEquipmentCostFont;
	vgui::HFont m_hEquipmentHeaderFont;
	vgui::HFont m_hEquipmentFooterFont;
	vgui::HFont m_hEquipmentHelpTitleFont;
	int m_iEquipmentHelpTitleFontTall;
	vgui::HFont m_hEquipmentHelpFont;
	vgui::HFont m_hPurchaseTitleFont;
	vgui::HFont m_hPurchaseWarningFont;
	vgui::HFont m_hPurchaseCashFont;
	vgui::HFont m_hPurchasePresetFont;
	vgui::HFont m_hPurchasePriceFont;
	vgui::HFont m_hPurchaseFooterFont;
	vgui::HFont m_hLoadoutEditorItemFont;
	vgui::HFont m_hLoadoutEditorControlFont;
	vgui::HFont m_hGoodBadRankFont;
	vgui::HFont m_hGoodBadAwardFont;
	vgui::HFont m_hGoodBadAwardLabelFont;
	vgui::HFont m_hGoodBadLocalFont;
	vgui::HFont m_hSlideFonts[4];
	CBitmapButton *m_pSlideBack;
	CBitmapButton *m_pSlideForward;
	CBitmapButton *m_pSlideClose;
	CUtlVector< FoFMenuEntry > m_MenuEntries;
	CUtlString m_MenuTitle;
	bool m_bMenuVisible;
	bool m_bLocalMenu;
	int m_iMenuKind;
	vgui::Panel *m_pEquipmentPaintPanel;
	vgui::Panel *m_pPurchasePaintPanel;
	vgui::Panel *m_pLoadoutEditorPaintPanel;
	vgui::Panel *m_pTeamClassPaintPanel;
	vgui::Button *m_pTeamMenuButtons[FOF_TEAM_BUTTON_COUNT];
	vgui::Button *m_pTeamIntroButton;
	vgui::Button *m_pTeamClassButtons[FOF_TEAM_CLASS_COUNT];
	vgui::Button *m_pTeamClassCloseButton;
	vgui::Button *m_pEquipmentMenuButtons[FOF_DM_ITEM_COUNT];
	vgui::Button *m_pEquipmentHelpButtons[FOF_DM_ITEM_COUNT];
	vgui::Button *m_pCrateMenuButtons[FOF_CRATE_ROW_COUNT];
	vgui::Button *m_pEquipmentOkayButton;
	vgui::Button *m_pEquipmentCloseButton;
	vgui::Frame *m_pEquipmentHelpFrame;
	vgui::Label *m_pEquipmentHelpTitle;
	vgui::Label *m_pEquipmentHelpText;
	vgui::Button *m_pEquipmentHelpCloseButton;
	vgui::Button *m_pPurchasePresetButtons[FOF_PURCHASE_MAX_VISIBLE_PRESETS];
	vgui::Button *m_pPurchasePageUpButton;
	vgui::Button *m_pPurchasePageDownButton;
	vgui::Button *m_pPurchaseCloseButton;
	vgui::Button *m_pPurchaseSwitchButton;
	vgui::Button *m_pPurchaseFilterButtons[FOF_PURCHASE_FILTER_COUNT];
	vgui::Button *m_pPurchaseEditorButton;
	vgui::ComboBox *m_pPurchaseFileList;
	vgui::Button *m_pLoadoutEditorItemButtons[FOF_PURCHASE_ITEM_TEXTURE_COUNT];
	vgui::Button *m_pLoadoutEditorSlotButtons[FOF_PURCHASE_MAX_ITEMS];
	vgui::TextEntry *m_pLoadoutEditorName;
	vgui::TextEntry *m_pLoadoutEditorFilename;
	vgui::Button *m_pLoadoutEditorSaveButton;
	vgui::Button *m_pLoadoutEditorDeleteButton;
	vgui::Button *m_pLoadoutEditorCancelButton;
	int m_iTeamBackgroundTexture;
	int m_iTeamIntroTexture;
	int m_iEquipmentBackgroundTexture;
	int m_iEquipmentItemBackgroundTexture;
	int m_iEquipmentOkayTexture;
	int m_iPurchaseEnabledTexture;
	int m_iPurchaseDisabledTexture;
	int m_iPurchaseMouseOverTexture;
	int m_iPurchaseArrowUpTexture;
	int m_iPurchaseArrowDownTexture;
	int m_iPurchaseEditTexture;
	int m_iPurchaseItemTextures[FOF_PURCHASE_ITEM_TEXTURE_COUNT];
	int m_iGoodBadBackgroundTexture;
	int m_iGoodBadBotTexture;
	int m_iGoodBadLaurelTexture;
	CAvatarImage *m_pGoodBadAvatars[11];
	int m_iTeamAuto2Texture;
	int m_iTeamClassBackgroundTexture;
	int m_iTeamButtonTextures[FOF_TEAM_BUTTON_COUNT];
	int m_iTeamClassItemTextures[FOF_TEAM_CLASS_COUNT]
		[FOF_TEAM_CLASS_ITEM_COUNT];
	int m_iEquipmentItemTextures[FOF_DM_ITEM_COUNT];
	int m_iCrateItemTextures[FOF_CRATE_ITEM_COUNT];
	float m_flMenuReadyAt;
	CUtlString m_TeamClassNames[FOF_TEAM_CLASS_COUNT];
	CUtlString m_TeamClassItemMaterials[FOF_TEAM_CLASS_COUNT]
		[FOF_TEAM_CLASS_ITEM_COUNT];
	int m_TeamClassItemStyles[FOF_TEAM_CLASS_COUNT]
		[FOF_TEAM_CLASS_ITEM_COUNT];
	bool m_TeamClassValid[FOF_TEAM_CLASS_COUNT];
	int m_SelectedGearItems[FOF_DM_GEAR_SLOTS];
	int m_iSelectedGearCount;
	int m_iSelectedAimItem;
	int m_iEquipmentHelpItem;
	CUtlVector< FoFPurchasePreset > m_PurchasePresets;
	int m_iPurchasePage;
	int m_iPurchaseFilter;
	bool m_bPurchaseQuickLayout;
	bool m_bPurchaseEditMode;
	bool m_bLoadoutEditorVisible;
	int m_iLoadoutEditorSourceOrder;
	int m_LoadoutEditorItems[FOF_PURCHASE_MAX_ITEMS];
	int m_iLoadoutEditorItemCount;

	CUtlString m_Hint;
	bool m_bHintVisible;
	int m_iHintMode;
	float m_flHintStartedAt;
	float m_flHintPromptAt;
	CUtlString m_Slide;
	CUtlVector< FoFSlideItem > m_SlideItems;
	int m_iSlideBackgroundTexture;
	bool m_bSlideVisible;
	bool m_bSlidePending;
	int m_iSlidePage;
	int m_iSlidePageCount;

	CUtlVector< FoFIconComm > m_IconComms;
	int m_iIconCommTexture;
	float m_flCircleProgressStart;
	float m_flCircleProgressEnd;
	vgui::CircularProgressBar *m_pCircleProgressBar;
	int m_iCapCount;
	int m_iCapMode;
	int m_iCapProgress;
	float m_flCapUntil;
	CUtlVector< FoFCaptureMarker > m_CaptureMarkers;
	CHudTexture *m_pCaptureMarkerIcons[2];
	CUtlVector< FoFBBMarker > m_BBMarkers;
	CHudTexture *m_pBBMarkerIcons[5];
	CUtlVector< FoFBBNotice > m_BBNotices;
	float m_GoodBadFloats[6];
	int m_GoodBadInts[6];
	CUtlVector< FoFGoodBadRank > m_GoodBadRanks;
	bool m_bGoodBadVisible;
	CUtlVector< FoFHitMarker > m_HitMarkers;
	int m_WeaponDamage[56];
	int m_iBowAttempts;
	int m_iBowHits;
	CUtlString m_StatLabel;
	int m_iStatNewValue;
	int m_iStatOldValue;
	bool m_bStatVisible;
	float m_flStatPromptAt;
	bool m_bTeamMenuOffered;
	bool m_bWasInitialSpectator;
	bool m_bIntentionalSpectator;
	int m_iTeamFactionMask;
	int m_LastPresetItems[6];
};

// Shared HUD queries used by the coordinator, status panels and user-message
// handlers. Keep these in one translation unit so every panel follows the
// same observer and screen-scaling rules.
CHudFoF *FoFHud();
int FoFHudScale( float value );
int FoFHudConVarInt( const char *name, int defaultValue );
C_BasePlayer *FoFHudObservedPlayer( C_BasePlayer *localPlayer );

bool FoFHudShouldDraw();
bool FoFHudIsSourceTVClient();
bool FoFHudIsSpectatorClient();
bool FoFHudUsesProgrammaticLayout( const char *panelName );
void FoFPresentStatUpdate(
	const char *label, int newValue, int oldValue );

#endif // FOF_HUD_H
