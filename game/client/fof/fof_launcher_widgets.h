#ifndef FOF_LAUNCHER_WIDGETS_H
#define FOF_LAUNCHER_WIDGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"
#include "vgui_bitmapbutton.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>

const wchar_t *FoFLauncherLocalize(
	const char *pszText,
	wchar_t *pBuffer,
	int nBufferBytes );
void FoFLauncherDrawText(
	const wchar_t *pText,
	vgui::HFont hFont,
	int x,
	int y,
	Color color );
void FoFLauncherDrawLocalizedText(
	const char *pszText,
	vgui::HFont hFont,
	int x,
	int y,
	Color color );
int FoFLauncherTextWide( const wchar_t *pText, vgui::HFont hFont );
float FoFLauncherScreenScale( int screenTall );
int FoFLauncherScalePixel( float logicalPixels, int screenTall );
void FoFLauncherDrawWrappedTitle(
	const char *pszText,
	vgui::HFont hFont,
	int x,
	int y,
	int wide,
	int maxLines,
	Color color );

class CFoFSearchEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CFoFSearchEntry, vgui::TextEntry );

public:
	CFoFSearchEntry(
		vgui::Panel *pParent,
		const char *pszName,
		const char *pszHint );

	void SetHintStyle( vgui::HFont hFont, Color color );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();

private:
	CUtlString m_Hint;
	vgui::HFont m_hHintFont;
	Color m_HintColor;
};

class CFoFPingComboBox : public vgui::ComboBox
{
	DECLARE_CLASS_SIMPLE( CFoFPingComboBox, vgui::ComboBox );

public:
	CFoFPingComboBox(
		vgui::Panel *pParent,
		const char *pszName,
		int visibleLines );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnShowMenu( vgui::Menu *pMenu );

private:
	void ApplyFoFStyle();
};

class CFoFLauncherPageButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFLauncherPageButton, vgui::Button );

public:
	CFoFLauncherPageButton(
		vgui::Panel *pParent,
		const char *pszName,
		const char *pszText,
		vgui::Panel *pTarget,
		const char *pszCommand );

	void PaintOnTop();
};

class CFoFLauncherButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFLauncherButton, vgui::Button );

public:
	CFoFLauncherButton(
		vgui::Panel *pParent,
		const char *pszName,
		const char *pszText,
		vgui::Panel *pTarget,
		const char *pszCommand );

	void SetCustomText( const wchar_t *pText );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();

private:
	CUtlString m_Text;
	wchar_t m_wszCustomText[128];
	vgui::HFont m_hFont;
};

class CFoFBrowserWarningButton : public CBitmapButton
{
	DECLARE_CLASS_SIMPLE( CFoFBrowserWarningButton, CBitmapButton );

public:
	CFoFBrowserWarningButton(
		vgui::Panel *pParent,
		const char *pszName,
		vgui::Panel *pTarget,
		const char *pszCommand );

	void BeginWarning();
	void DismissWarning();
	virtual void OnThink();

private:
	float m_flEnableTime;
};

class CFoFTopBarButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFTopBarButton, vgui::Button );

public:
	CFoFTopBarButton(
		vgui::Panel *pParent,
		const char *pszName,
		const char *pszTitle,
		const char *pszIcon,
		vgui::Panel *pTarget,
		const char *pszCommand );
	virtual ~CFoFTopBarButton();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	int DesiredWide( int iconWide, int margin ) const;
	virtual void Paint();

private:
	CUtlString m_Title;
	int m_iIcon;
	vgui::HFont m_hFont;
};

class CFoFRefreshButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CFoFRefreshButton, vgui::Button );

public:
	CFoFRefreshButton(
		vgui::Panel *pParent,
		const char *pszName,
		vgui::Panel *pTarget,
		const char *pszCommand,
		bool bBright );
	virtual ~CFoFRefreshButton();

	virtual void Paint();

private:
	bool m_bBright;
	int m_iTexture;
};

#endif // FOF_LAUNCHER_WIDGETS_H
