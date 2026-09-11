#ifndef FOF_HUD_TRANSIENT_H
#define FOF_HUD_TRANSIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "mathlib/vector.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
class IScheme;
}

// Original FoF map timer/map-progress text.  This is deliberately a
// full-screen HUD panel rather than a HudLayout.res entry.
class CHudFoFTimer : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFTimer, vgui::Panel );

public:
	CHudFoFTimer( const char *elementName );

	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void OnThink();
	virtual void Paint();

private:
	void LayoutForScreen();
	void UpdateText();

	vgui::HFont m_hTextFont;
	wchar_t m_wszText[256];
	int m_iTextWide;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
};

struct client_textmessage_t;

struct FoFWrappedLine
{
	wchar_t text[512];
	int length;
	int wide;
	int tall;
};

struct FoFWrappedTextLayout
{
	FoFWrappedTextLayout()
		: font( vgui::INVALID_FONT ), maxWide( 0 ), totalTall( 0 )
	{
	}

	vgui::HFont font;
	int maxWide;
	int totalTall;
	CUtlVector< wchar_t > text;
	CUtlVector< FoFWrappedLine > lines;
};

// Returns the FoF font selected by the original CHudMessage paths, or
// INVALID_FONT when the ordinary unnamed Source message font should remain.
vgui::HFont FoFHudMessageFont(
	const client_textmessage_t *message,
	const char *vguiFontName );

struct FoFBBMarker
{
	int operation;
	int iconType;
	Vector position;
	CUtlString text;
	float expiresAt;
};

struct FoFCaptureMarker
{
	int team;
	int showMode;
	Vector position;
};

struct FoFBBNotice
{
	int kind;
	CUtlString format;
	CUtlString arguments[3];
	float receivedAt;
	float expiresAt;
};

struct FoFIconComm
{
	int playerIndex;
	float expiresAt;
};

struct FoFHitMarker
{
	int damage;
	int weaponIndex;
	float rank;
	bool rankOnly;
	Vector sourcePosition;
	Vector position;
	float receivedAt;
	float expiresAt;
};

struct FoFGoodBadRank
{
	int playerIndex;
	CUtlString playerName;
	int teamNumber;
	int score;
	int frags;
	unsigned int friendsId;
	bool fakePlayer;
	bool localPlayer;
};

#endif // FOF_HUD_TRANSIENT_H
