#include "cbase.h"
#include "fof/fof_spectator_ui.h"

#include "game/client/iviewport.h"
#include "igameresources.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CFoFSpectatorGUI::CFoFSpectatorGUI( IViewPort *pViewPort )
	: CSpectatorGUI( pViewPort )
	, m_nLastSpecMode( OBS_MODE_NONE )
	, m_nLastSpecTarget( NULL )
	, m_nLastSpecHealth( -1 )
	, m_bLastSpecTargetAlive( false )
{
}

void CFoFSpectatorGUI::PerformLayout()
{
	int w, h, x, y;
	GetHudSize( w, h );
	SetBounds( 0, 0, w, h );

	m_pTopBar->GetPos( x, y );
	if ( y < 0 || y >= h || m_pTopBar->GetTall() <= 0 )
		m_pTopBar->SetBounds( 0, 0, w, MAX( h * 52 / 480, 1 ) );
	else
		m_pTopBar->SetSize( w, m_pTopBar->GetTall() );

	m_pBottomBarBlank->GetPos( x, y );
	if ( y <= 0 || y >= h )
	{
		y = clamp( h * 428 / 480, 1, h - 1 );
		m_pBottomBarBlank->SetPos( 0, y );
	}
	m_pBottomBarBlank->SetSize( w, h - y );
}

bool CFoFSpectatorGUI::NeedsUpdate( void )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return false;

	if ( m_nLastSpecMode != pLocalPlayer->GetObserverMode() )
		return true;

	CBaseEntity *pTarget = pLocalPlayer->GetObserverTarget();
	if ( m_nLastSpecTarget != pTarget )
		return true;

	IGameResources *pResources = GameResources();
	const int nTargetIndex = pTarget ? pTarget->entindex() : 0;
	const bool bValidTarget = pResources && nTargetIndex > 0 &&
		nTargetIndex <= gpGlobals->maxClients;
	const int nHealth = bValidTarget ?
		pResources->GetHealth( nTargetIndex ) : -1;
	const bool bAlive = bValidTarget &&
		pResources->IsAlive( nTargetIndex );
	if ( m_nLastSpecHealth != nHealth ||
		m_bLastSpecTargetAlive != bAlive )
	{
		return true;
	}

	return BaseClass::NeedsUpdate();
}

void CFoFSpectatorGUI::Update()
{
	BaseClass::Update();

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		m_nLastSpecMode = pLocalPlayer->GetObserverMode();
		m_nLastSpecTarget = pLocalPlayer->GetObserverTarget();

		IGameResources *pResources = GameResources();
		const int nTargetIndex = m_nLastSpecTarget ?
			m_nLastSpecTarget->entindex() : 0;
		const bool bValidTarget = pResources && nTargetIndex > 0 &&
			nTargetIndex <= gpGlobals->maxClients;
		m_nLastSpecHealth = bValidTarget ?
			pResources->GetHealth( nTargetIndex ) : -1;
		m_bLastSpecTargetAlive = bValidTarget &&
			pResources->IsAlive( nTargetIndex );
	}
}

CFoFSpectatorMenu::CFoFSpectatorMenu( IViewPort *pViewPort )
	: CSpectatorMenu( pViewPort )
{
	// BottomSpectator.res owns FoF's visible X. Suppress Frame's title-bar
	// close control, which otherwise survives as a tiny square at (0, 0).
	SetCloseButtonVisible( false );
}

void CFoFSpectatorMenu::PerformLayout()
{
	int w, h;
	GetHudSize( w, h );
	SetSize( w, GetTall() );

	// Frame::ApplySettings defaults its system close button back to visible
	// whenever BottomSpectator.res or the scheme is reapplied.  FoF supplies
	// its own CancelButton X, so keep every title-bar system control hidden on
	// the layout pass that follows every observer transition and scheme reload.
	SetTitleBarVisible( false );
}

void CFoFSpectatorMenu::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "specnext" ) )
	{
		engine->ClientCmd( "spec_next" );
	}
	else if ( !Q_stricmp( command, "specprev" ) )
	{
		engine->ClientCmd( "spec_prev" );
	}
	else if ( !Q_stricmp( command, "Close" ) ||
		!Q_stricmp( command, "vguicancel" ) )
	{
		// Route both possible close controls through the viewport so panel
		// visibility and mouse/keyboard ownership are released together.
		gViewPortInterface->ShowPanel( PANEL_SPECMENU, false );
	}
}
