#ifndef FOF_ADVANCED_OPTIONS_DIALOG_H
#define FOF_ADVANCED_OPTIONS_DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

#include <vgui_controls/Frame.h>

struct FoFAdvancedOption_t;

namespace vgui
{
	class IScheme;
	class PanelListPanel;
}

class CFoFAdvancedOptionsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFoFAdvancedOptionsDialog, vgui::Frame );

public:
	CFoFAdvancedOptionsDialog();
	virtual ~CFoFAdvancedOptionsDialog();

	void ShowDialog();

	virtual void PerformLayout();
	virtual void OnCommand( const char *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	bool LoadOptions();
	bool ParseOption( const char *&pCursor, const char *pszCvar );
	void BuildOptionControls();
	void ReadSettings();
	void ApplySettings();
	void WriteUserScript();

	vgui::PanelListPanel *m_pOptionsList;
	CUtlVector<FoFAdvancedOption_t *> m_Options;
};

namespace vgui
{
	class Panel;
}

enum FoFAdvancedOptionType_t
{
	FOF_ADVANCED_BOOL,
	FOF_ADVANCED_STRING,
	FOF_ADVANCED_NUMBER,
	FOF_ADVANCED_LIST
};

struct FoFAdvancedChoice_t
{
	CUtlString label;
	CUtlString value;
};

struct FoFAdvancedOption_t
{
	FoFAdvancedOption_t()
		: type( FOF_ADVANCED_STRING )
		, minimum( -1.0f )
		, maximum( -1.0f )
		, control( NULL )
	{
	}

	~FoFAdvancedOption_t()
	{
		choices.PurgeAndDeleteElements();
	}

	CUtlString cvar;
	CUtlString prompt;
	CUtlString defaultValue;
	FoFAdvancedOptionType_t type;
	float minimum;
	float maximum;
	CUtlVector<FoFAdvancedChoice_t *> choices;
	vgui::Panel *control;
};

#endif // FOF_ADVANCED_OPTIONS_DIALOG_H
