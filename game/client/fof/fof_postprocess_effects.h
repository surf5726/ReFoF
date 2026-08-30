#ifndef FOF_POSTPROCESS_EFFECTS_H
#define FOF_POSTPROCESS_EFFECTS_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/MaterialSystemUtil.h"
#include "ScreenSpaceEffects.h"

class CViewSetup;

bool FoFIsDepthOfFieldEnabled();
void FoFDoDepthOfField( const CViewSetup &viewSetup );
void FoFApplyDepthOfField( const CViewSetup &viewSetup );
void FoFDrawIronsightEffect( const CViewSetup *pView );

class CDrunkEffect : public IScreenSpaceEffect
{
public:
	CDrunkEffect();

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *pParams );
	virtual void Render( int x, int y, int w, int h );
	virtual void Enable( bool bEnable );
	virtual bool IsEnabled( void );

private:
	unsigned char GetFadeAlpha( void ) const;

	CTextureReference m_DrunkTexture;
	CMaterialReference m_DrunkMaterial;
	bool m_bUpdateView;
	bool m_bEnabled;
	int m_nLastAlpha;
};

class CStunEffect : public IScreenSpaceEffect
{
public:
	CStunEffect();

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *pParams );
	virtual void Render( int x, int y, int w, int h );
	virtual void Enable( bool bEnable );
	virtual bool IsEnabled( void );

private:
	CTextureReference m_StunTexture;
	CMaterialReference m_StunMaterial;
	float m_flDuration;
	float m_flFinishTime;
	bool m_bUpdateView;
};

class CEP1IntroEffect : public IScreenSpaceEffect
{
public:
	CEP1IntroEffect();

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *pParams );
	virtual void Render( int x, int y, int w, int h );
	virtual void Enable( bool bEnable );
	virtual bool IsEnabled( void );

private:
	unsigned char GetFadeAlpha( void ) const;

	CTextureReference m_StunTexture;
	CMaterialReference m_StunMaterial;
	float m_flDuration;
	float m_flFinishTime;
	bool m_bUpdateView;
	bool m_bEnabled;
	bool m_bFadeOut;
};

class CEP2StunEffect : public IScreenSpaceEffect
{
public:
	CEP2StunEffect();

	virtual void Init( void );
	virtual void Shutdown( void );
	virtual void SetParameters( KeyValues *pParams );
	virtual void Render( int x, int y, int w, int h );
	virtual void Enable( bool bEnable );
	virtual bool IsEnabled( void );

private:
	unsigned char GetFadeAlpha( void ) const;

	CTextureReference m_StunTexture;
	CMaterialReference m_StunMaterial;
	float m_flDuration;
	float m_flFinishTime;
	bool m_bUpdateView;
	bool m_bEnabled;
	bool m_bFadeOut;
};

#endif // FOF_POSTPROCESS_EFFECTS_H
