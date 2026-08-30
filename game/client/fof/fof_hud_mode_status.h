#ifndef FOF_HUD_MODE_STATUS_H
#define FOF_HUD_MODE_STATUS_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class C_BasePlayer;
class CHudTexture;
class IGameEvent;

namespace vgui
{
class CircularProgressBar;
class ImagePanel;
class IScheme;
}

class CHudFoFBBStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFBBStatus, vgui::Panel );
	DECLARE_MULTIPLY_INHERITED();

public:
	CHudFoFBBStatus( const char *elementName );

	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();
	virtual void Paint();

private:
	C_BasePlayer *GetDisplayPlayer() const;
	bool IsJailTimerActive( C_BasePlayer *player ) const;
	bool IsUnarmedTimerActive( C_BasePlayer *player ) const;
	float GetActiveTimerDeadline( C_BasePlayer *player ) const;
	void LayoutForScreen();
	void SetClockVisible( bool visible );

	CHudTexture *m_pJailBars;
	vgui::ImagePanel *m_pClockFace;
	vgui::CircularProgressBar *m_pTimeBar;
	vgui::HFont m_hLargeFont;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

// FoF calls this HUD element CHudVersus, although its network-facing
// job is to briefly show the equipment granted at the start of a round.
class CHudVersus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudVersus, vgui::Panel );

public:
	CHudVersus( const char *elementName );

	virtual void Init();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void FireGameEvent( IGameEvent *event );

	void ReceiveEquipItem( int itemId );
	int GetEquipItemCount() const { return m_iEquipItemCount; }

private:
	void ClearEquipItems();
	void LayoutEquipItems();
	const char *FindEquipItemMaterial( int itemId ) const;

	vgui::ImagePanel *m_pEquipItems[5];
	int m_iEquipItemCount;
	float m_flEquipItemsExpireTime;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

void FoFHudEquipItemReceive( int itemId );
int FoFHudEquipItemCount();

#endif // FOF_HUD_MODE_STATUS_H
