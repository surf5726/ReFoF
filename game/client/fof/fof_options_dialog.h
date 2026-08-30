#ifndef FOF_OPTIONS_DIALOG_H
#define FOF_OPTIONS_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CFoFAdvancedOptionsDialog;

namespace vgui
{
	class CheckButton;
	class Slider;
}

class CFoFOptionsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFOptionsDialog, vgui::Frame );

public:
	CFoFOptionsDialog();
	void ShowDialog();
	void HideDialog();

	virtual void PerformLayout();
	virtual void Paint();
	virtual void OnCommand( const char *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void ReadSettings();
	void ApplySettings();
	void ConfigureSlider(
		vgui::Slider *pSlider,
		int iMinimum,
		int iMaximum,
		int iTicks = 0 );

	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", pData );

	vgui::Slider *m_pRed;
	vgui::Slider *m_pGreen;
	vgui::Slider *m_pBlue;
	vgui::Slider *m_pSmoke;
	vgui::Slider *m_pVisualQuality;
	vgui::Slider *m_pFov;
	vgui::Slider *m_pViewmodelFov;
	vgui::CheckButton *m_pBodyAwareness;
	CFoFAdvancedOptionsDialog *m_pAdvancedOptions;
};

#endif // FOF_OPTIONS_DIALOG_H
