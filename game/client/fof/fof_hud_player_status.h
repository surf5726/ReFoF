#ifndef FOF_HUD_PLAYER_STATUS_H
#define FOF_HUD_PLAYER_STATUS_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "tier1/utlvector.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class C_BaseCombatWeapon;
class C_BasePlayer;
class CHudTexture;
class IGameEvent;

namespace vgui
{
class IScheme;
class Label;
}

struct FoFCashChange
{
	int amount;
	float startedAt;
};

struct FoFNotorietyRank
{
	int playerIndex;
	int notoriety;
	int multiKill;
	int reserved;
	int frags;
	bool localPlayer;
};

struct FoFNotorietyNotice
{
	wchar_t text[512];
	float receivedAt;
	int reason;
};

// Supplied by the main FoF HUD module so the health panel remains hidden
// while the full-screen hint presentation owns the HUD.
bool FoFHudIsHintVisible();

class CHudFoFHealth : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFHealth, vgui::Panel );
	DECLARE_MULTIPLY_INHERITED();

public:
	CHudFoFHealth( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	void ReceiveMaxHP( int maxHP ) { m_iMaxHP = maxHP; }
	void UpdatePosition();

private:
	C_BasePlayer *GetDisplayPlayer() const;
	int GetDisplayTeam( C_BasePlayer *player ) const;

	vgui::Label *m_pHealthLabel;
	CHudTexture *m_pFullIcons[6];
	CHudTexture *m_pEmptyIcons[6];
	CHudTexture *m_pPotionFull;
	CHudTexture *m_pPotionEmpty;
	int m_iHealth;
	int m_iMaxHP;
	int m_iTeam;
	int m_iMeterSize;
	int m_iFilledSize;
	int m_iPotionFilledSize;
	int m_iLastScreenWide;
	int m_iLastScreenTall;
	bool m_bGhostTown;
	float m_flHealthFraction;
	float m_flPotionFraction;
};

class CHudFoFAmmo : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudFoFAmmo, vgui::Panel );
	DECLARE_MULTIPLY_INHERITED();

public:
	CHudFoFAmmo( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

private:
	C_BasePlayer *GetDisplayPlayer() const;
	void DrawAmmo( int current, int maximum, int hand, int mode );
	float GetWeaponScale( C_BaseCombatWeapon *weapon, bool secondary, int &mode ) const;
	bool IsPrimaryAmmoWeapon( int weaponID ) const;

	CHudTexture *m_pAmmoFull;
	CHudTexture *m_pAmmoEmpty;
	int m_iRadius;
	int m_iCenterX[3];
	int m_iCenterY[3];
	int m_iCurrent[3];
	int m_iMaximum[3];
	float m_flScale[3];
	int m_iPrimaryMode;
};

class CHudCash : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudCash, vgui::Panel );
	DECLARE_MULTIPLY_INHERITED();

public:
	CHudCash( const char *elementName );

	virtual void Init();
	virtual void Reset();
	virtual bool ShouldDraw();
	virtual void Paint();

	void ReceiveCash( int amount );

private:
	C_BasePlayer *GetDisplayPlayer() const;
	bool ShouldDrawBuyHint(
		C_BasePlayer *localPlayer, C_BasePlayer *displayPlayer ) const;
	void DrawText( vgui::HFont font, const wchar_t *text, int x, int y,
		const Color &color ) const;

	CPanelAnimationVar( vgui::HFont, m_hLargeFont, "NumberFont", "MenuFontMed" );
	CPanelAnimationVar( vgui::HFont, m_hSmallFont, "NumberFontS", "MenuFontSmall" );
	CUtlVector< FoFCashChange > m_Changes;
};

class CHudFoFNotoriety : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudFoFNotoriety, vgui::Panel );
	DECLARE_MULTIPLY_INHERITED();

public:
	CHudFoFNotoriety( const char *elementName );

	virtual void Init();
	virtual bool ShouldDraw();
	virtual void OnThink();
	virtual void Paint();
	virtual void FireGameEvent( IGameEvent *event );

	void ReceiveNotoriety( int reason, int value, int eventCode, const char *text );

private:
	void ClearState();
	void BuildRanks( C_BasePlayer *localPlayer );
	Color GetRankColor( const FoFNotorietyRank &rank ) const;
	int DrawText( vgui::HFont font, const wchar_t *text, int x, int y,
		const Color &color ) const;
	const wchar_t *RankSuffix( int rank, wchar_t *buffer, int bufferBytes ) const;

	CPanelAnimationVar( vgui::HFont, m_hLargeFont, "NumberFont", "MenuFontMed" );
	CPanelAnimationVar( vgui::HFont, m_hSmallFont, "NumberFontS", "MenuFontSmall" );
	CPanelAnimationVar( vgui::HFont, m_hSSmallFont, "NumberFontSS", "HudSelectionNumbers4" );
	CPanelAnimationVar( vgui::HFont, m_hVerySmallFont, "NotoS", "NotorietyFont" );

	CUtlVector< FoFNotorietyRank > m_Ranks;
	bool m_bDrawEnabled;
	bool m_bRoundEndAutoCash;
	int m_iLocalMultiKill;
	float m_flRoundEndStartedAt;
	float m_flNextRankBuildTime;
	int m_iTotalNotoriety;
	CUtlVector< FoFNotorietyNotice > m_Notices;
};

#endif // FOF_HUD_PLAYER_STATUS_H
