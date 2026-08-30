#ifndef FOF_LOADOUT_EDITOR_H
#define FOF_LOADOUT_EDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

class CHudFoF;

// EditPreset is a child of PresetMenu in the shipped client.  Keep its paint
// layer separate here as well: the regular purchase menu remains visible
// underneath while this layer replaces only the 4x5 preset grid.
class CFoFLoadoutEditorPaintPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFoFLoadoutEditorPaintPanel, vgui::Panel );

public:
	explicit CFoFLoadoutEditorPaintPanel( CHudFoF *owner );
	virtual void Paint();

private:
	CHudFoF *m_pOwner;
};

#endif
