#include "cbase.h"
#include "fof/fof_hud.h"
#include "fof/fof_hud_slide.h"
#include "hud_macros.h"
#include "iinput.h"
#include "vgui_bitmapbutton.h"
#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include "fof/fof_motd.h"
#include "filesystem.h"
#include "KeyValues.h"
#include <vgui/Cursor.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CHudFoF::LayoutSlideVideos()
{
	for ( int i = 0; i < m_SlideItems.Count(); ++i )
	{
		if ( m_SlideItems[i].videoPanel )
			m_SlideItems[i].videoPanel->StopPlayback();
	}

	if ( !m_bSlideVisible )
		return;

	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int canvasX =
		ScreenWidth() / 2 - RoundFloatToInt( 200.0f * scale );
	const int canvasY =
		ScreenHeight() / 2 - RoundFloatToInt( 150.0f * scale );

	int videoIndex = 0;
	for ( int i = 0; i < m_SlideItems.Count(); ++i )
	{
		FoFSlideItem &item = m_SlideItems[i];
		if ( !item.videoPanel || item.page != m_iSlidePage ||
			item.text.IsEmpty() )
		{
			continue;
		}
		if ( videoIndex++ >= 15 )
			continue;

		item.videoPanel->SetBounds(
			canvasX + RoundFloatToInt( item.posX * scale ),
			canvasY + RoundFloatToInt( item.posY * scale ),
			MAX( RoundFloatToInt( item.sizeX * scale ), 1 ),
			MAX( RoundFloatToInt( item.sizeY * scale ), 1 ) );
		item.videoPanel->SetVisible( true );
		item.videoPanel->SetZPos( 5 );
		item.videoPanel->StartPlayback( item.text.String() );
	}
}

void CHudFoF::SetSlideControlsVisible( bool visible )
{
	const bool lastPage =
		m_iSlidePageCount <= 1 ||
		m_iSlidePage + 1 >= m_iSlidePageCount;
	if ( m_pSlideBack )
		m_pSlideBack->SetVisible(
			visible && m_iSlidePage > 0 );
	if ( m_pSlideForward )
		m_pSlideForward->SetVisible(
			visible && !lastPage );
	if ( m_pSlideClose )
		m_pSlideClose->SetVisible(
			visible && lastPage );
}

void CHudFoF::LayoutSlideControls()
{
	// The original slide layout tears down every video material and starts
	// only those on the newly active page.
	LayoutSlideVideos();

	if ( !m_pSlideBack || !m_pSlideForward || !m_pSlideClose )
		return;

	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int center = ScreenWidth() / 2;

	m_pSlideBack->SetBounds(
		center - RoundFloatToInt( 325.0f * scale ),
		RoundFloatToInt( 215.0f * scale ),
		MAX( RoundFloatToInt( 80.0f * scale ), 1 ),
		MAX( RoundFloatToInt( 40.0f * scale ), 1 ) );
	m_pSlideForward->SetBounds(
		center + RoundFloatToInt( 270.0f * scale ),
		RoundFloatToInt( 215.0f * scale ),
		MAX( RoundFloatToInt( 80.0f * scale ), 1 ),
		MAX( RoundFloatToInt( 40.0f * scale ), 1 ) );
	m_pSlideClose->SetBounds(
		center + RoundFloatToInt( 175.0f * scale ),
		RoundFloatToInt( 355.0f * scale ),
		MAX( RoundFloatToInt( 100.0f * scale ), 1 ),
		MAX( RoundFloatToInt( 55.0f * scale ), 1 ) );
	m_pSlideBack->MoveToFront();
	m_pSlideForward->MoveToFront();
	m_pSlideClose->MoveToFront();
	SetSlideControlsVisible( m_bSlideVisible );
}

void CHudFoF::PaintSlide()
{
	if ( !m_bSlideVisible )
		return;

	// FoF's proportional design surface is 640x480.  The parchment
	// background occupies (0,40)-(640,440), while intro item coordinates
	// use a centered 400x300 canvas.
	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int boxWide = RoundFloatToInt( 640.0f * scale );
	const int boxTall = RoundFloatToInt( 400.0f * scale );
	const int boxX =
		ScreenWidth() / 2 - RoundFloatToInt( 320.0f * scale );
	const int boxY = RoundFloatToInt( 40.0f * scale );
	const int canvasX =
		ScreenWidth() / 2 - RoundFloatToInt( 200.0f * scale );
	const int canvasY =
		ScreenHeight() / 2 - RoundFloatToInt( 150.0f * scale );

	// The 640x400 slide texture contains the parchment and its local black
	// surround.  The shipped panel also clears the complete viewport to black,
	// so non-16:9 resolutions never expose the live game around that texture.
	vgui::surface()->DrawSetColor( 0, 0, 0, 255 );
	vgui::surface()->DrawFilledRect(
		0, 0, ScreenWidth(), ScreenHeight() );

	if ( m_iSlideBackgroundTexture >= 0 )
	{
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_iSlideBackgroundTexture );
		vgui::surface()->DrawTexturedRect(
			boxX, boxY, boxX + boxWide, boxY + boxTall );
	}
	else
	{
		vgui::surface()->DrawSetColor( 7, 5, 3, 246 );
		vgui::surface()->DrawFilledRect(
			boxX, boxY, boxX + boxWide, boxY + boxTall );
	}

	// FoF creates fixed LabelN/ImageN/VideoN child pools.  Equal-z media
	// therefore paints in Image0, Video0, Image1, Video1... pool order,
	// regardless of how image and video records are interleaved in the file.
	for ( int mediaIndex = 0; mediaIndex < 15; ++mediaIndex )
	{
		const FoFSlideItem *imageItem = NULL;
		const FoFSlideItem *videoItem = NULL;
		int imageIndex = 0;
		int videoIndex = 0;
		for ( int i = 0; i < m_SlideItems.Count(); ++i )
		{
			const FoFSlideItem &candidate = m_SlideItems[i];
			if ( candidate.page != m_iSlidePage ||
				candidate.text.IsEmpty() )
			{
				continue;
			}

			if ( candidate.fontSize <= 0 &&
				candidate.fontSize != -1 )
			{
				if ( imageIndex++ == mediaIndex )
					imageItem = &candidate;
			}
			else if ( candidate.fontSize == -1 )
			{
				if ( videoIndex++ == mediaIndex )
					videoItem = &candidate;
			}
		}

		if ( !imageItem && !videoItem )
			break;

		if ( imageItem && imageItem->textureId >= 0 )
		{
			const int x = canvasX +
				RoundFloatToInt( imageItem->posX * scale );
			const int y = canvasY +
				RoundFloatToInt( imageItem->posY * scale );
			const int wide = MAX(
				RoundFloatToInt( imageItem->sizeX * scale ), 1 );
			const int tall = MAX(
				RoundFloatToInt( imageItem->sizeY * scale ), 1 );
			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTexture( imageItem->textureId );
			vgui::surface()->DrawTexturedRect(
				x, y, x + wide, y + tall );
		}
		if ( videoItem && videoItem->videoPanel )
			videoItem->videoPanel->PaintVideoFrame();
	}

	// The original assigns z=5 to image/video panels and z=10 to labels.
	// Draw all labels in a second pass so they remain above every media item.
	int labelIndex = 0;
	for ( int i = 0; i < m_SlideItems.Count(); ++i )
	{
		const FoFSlideItem &item = m_SlideItems[i];
		if ( item.page != m_iSlidePage || item.text.IsEmpty() ||
			item.fontSize <= 0 )
		{
			continue;
		}
		if ( labelIndex++ >= 15 )
			continue;

		const int x = canvasX + RoundFloatToInt( item.posX * scale );
		const int y = canvasY + RoundFloatToInt( item.posY * scale );
		const int wide = MAX(
			RoundFloatToInt( item.sizeX * scale ), 1 );
		const int tall = MAX(
			RoundFloatToInt( item.sizeY * scale ), 1 );
		wchar_t localizedBuffer[2048];
		const wchar_t *localized = Localize(
			item.text.String(),
			localizedBuffer,
			sizeof( localizedBuffer ) );
		const int fontIndex =
			item.fontSize == 2 ? 2 : ( item.fontSize == 1 ? 1 : 0 );
		Color textColor;
		if ( item.color == 1 )
			textColor = Color( 10, 10, 10, 250 );
		else if ( item.color == 0 )
			textColor = Color( 205, 205, 205, 250 );
		else
			textColor = Color( 205, 205, 155, 250 );
		DrawWrappedWide(
			localized,
			m_hSlideFonts[fontIndex],
			x,
			y,
			wide,
			tall,
			textColor,
			item.align );
	}

}

bool CHudFoF::StepSlide( int direction )
{
	if ( !m_bSlideVisible || direction == 0 )
		return false;

	if ( direction < 0 )
	{
		if ( m_iSlidePage > 0 )
			--m_iSlidePage;
		LayoutSlideControls();
		return true;
	}

	if ( m_iSlidePage + 1 < m_iSlidePageCount )
	{
		++m_iSlidePage;
		LayoutSlideControls();
		return true;
	}
	return false;
}

void CHudFoF::CloseSlide()
{
	m_bSlideVisible = false;
	m_bSlidePending = false;
	m_iSlidePage = 0;
	LayoutSlideVideos();
	SetSlideControlsVisible( false );
	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
		vgui::input()->ReleaseAppModalSurface();
	vgui::surface()->SetCursorAlwaysVisible( false );
	if ( ::input && engine->IsInGame() )
		::input->ActivateMouse();
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

CON_COMMAND( go_forward, "Advance the FoF introduction slide." )
{
	CHudFoF *hud = FoFHud();
	if ( hud )
		hud->StepSlide( 1 );
}

CON_COMMAND( go_back, "Return to the previous FoF introduction slide." )
{
	CHudFoF *hud = FoFHud();
	if ( hud )
		hud->StepSlide( -1 );
}

// FoF slide loading, persistence and message staging.

static bool FoFIntroWasWatched( const char *name )
{
	if ( !name || !name[0] )
		return false;

	KeyValues *records = new KeyValues( "Intros_Watched" );
	const bool loaded = records->LoadFromFile(
		filesystem,
		"fof_scripts/intro_records.txt",
		"MOD" );
	bool watched = false;
	if ( loaded )
	{
		for ( KeyValues *item = records->GetFirstTrueSubKey();
			item;
			item = item->GetNextTrueSubKey() )
		{
			if ( !Q_stricmp( item->GetString( "slide", "" ), name ) )
			{
				watched = true;
				break;
			}
		}
	}
	records->deleteThis();
	return watched;
}

static void FoFRecordWatchedIntro( const char *name )
{
	if ( !name || !name[0] || FoFIntroWasWatched( name ) )
		return;

	KeyValues *records = new KeyValues( "Intros_Watched" );
	records->LoadFromFile(
		filesystem,
		"fof_scripts/intro_records.txt",
		"MOD" );
	KeyValues *item = new KeyValues( "item" );
	item->SetString( "slide", name );
	records->AddSubKey( item );
	filesystem->CreateDirHierarchy( "fof_scripts", "MOD" );
	records->SaveToFile(
		filesystem,
		"fof_scripts/intro_records.txt",
		"MOD" );
	records->deleteThis();
}

bool CHudFoF::LoadSlide( const char *name )
{
	ClearSlideItems();
	m_iSlidePage = 0;
	m_iSlidePageCount = 0;
	m_bSlideVisible = false;

	if ( !name || !name[0] || Q_strstr( name, ".." ) ||
		Q_strstr( name, "/" ) || Q_strstr( name, "\\" ) )
	{
		return false;
	}

	char path[MAX_PATH];
	Q_snprintf(
		path, sizeof( path ),
		"fof_scripts/intros/%s.txt", name );

	KeyValues *root = new KeyValues( "Intro" );
	if ( !root->LoadFromFile( filesystem, path, "MOD" ) )
	{
		root->deleteThis();
		return false;
	}

	int highestPage = -1;
	for ( KeyValues *item = root->GetFirstSubKey();
		item;
		item = item->GetNextKey() )
	{
		FoFSlideItem slideItem;
		slideItem.page = MAX( item->GetInt( "page", 0 ), 0 );
		// The shipped panel uses font_size as a content discriminator:
		// positive values are labels, exactly -1 is VideoPanelFoF, and every
		// other non-positive value is an ImagePanel.
		slideItem.fontSize = item->GetInt( "font_size", 0 );
		slideItem.color = item->GetInt( "color", 0 );
		slideItem.align = item->GetInt( "align", 2 );
		slideItem.sizeX = MAX( item->GetInt( "sizex", 1 ), 1 );
		slideItem.sizeY = MAX( item->GetInt( "sizey", 1 ), 1 );
		slideItem.posX = item->GetInt( "posx", 1 );
		slideItem.posY = item->GetInt( "posy", 1 );
		slideItem.text = item->GetString( "text", "" );
		slideItem.textureId = -1;
		slideItem.videoPanel = NULL;
		if ( slideItem.fontSize <= 0 && slideItem.fontSize != -1 &&
			!slideItem.text.IsEmpty() )
		{
			char textureName[MAX_PATH];
			Q_snprintf(
				textureName,
				sizeof( textureName ),
				"vgui/%s",
				slideItem.text.String() );
			slideItem.textureId =
				vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile(
				slideItem.textureId,
				textureName,
				true,
				false );
		}
		else if ( slideItem.fontSize == -1 &&
			!slideItem.text.IsEmpty() )
		{
			slideItem.videoPanel =
				new CFoFSlideVideoPanel( this );
		}
		m_SlideItems.AddToTail( slideItem );
		highestPage = MAX( highestPage, slideItem.page );
	}
	root->deleteThis();

	m_iSlidePageCount = highestPage + 1;
	m_bSlideVisible =
		m_iSlidePageCount > 0 && m_SlideItems.Count() > 0;
	if ( m_bSlideVisible )
	{
		// The shipped client records an intro as watched as soon as its first
		// page has loaded successfully, not only after the user reaches the
		// final page.
		FoFRecordWatchedIntro( name );
		// Slides need pointer input for their navigation buttons, but the
		// full-screen HUD must not own keyboard focus or become app-modal.
		// Otherwise Escape and the developer-console bind never reach GameUI
		// or the engine while an intro is visible.
		if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
			vgui::input()->ReleaseAppModalSurface();
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( false );
		SetCursor( vgui::dc_arrow );
		MakePopup( false );
		MoveToFront();
		if ( ::input )
			::input->DeactivateMouse();
		vgui::surface()->UnlockCursor();
		vgui::surface()->SetCursor( vgui::dc_arrow );
		vgui::surface()->SetCursorAlwaysVisible( true );
	}
	LayoutSlideControls();
	return m_bSlideVisible;
}

void CHudFoF::ClearSlideItems()
{
	for ( int i = 0; i < m_SlideItems.Count(); ++i )
	{
		const int textureId = m_SlideItems[i].textureId;
		if ( textureId >= 0 &&
			vgui::surface()->IsTextureIDValid( textureId ) )
		{
			vgui::surface()->DeleteTextureByID( textureId );
		}
		if ( m_SlideItems[i].videoPanel )
		{
			m_SlideItems[i].videoPanel->StopPlayback();
			m_SlideItems[i].videoPanel->MarkForDeletion();
			m_SlideItems[i].videoPanel = NULL;
		}
	}
	m_SlideItems.Purge();
}

void CHudFoF::ReceiveSlide( const char *name )
{
	// Fully leave an existing modal slide before staging its replacement.
	// Otherwise a missing or malformed intro could leave an invisible modal
	// surface with the game mouse still deactivated.
	if ( IsSlideOpen() ||
		vgui::input()->GetAppModalSurface() == GetVPanel() )
	{
		CloseSlide();
	}
	FoFShowFirstMotd();
	m_Slide = name ? name : "";
	ClearSlideItems();
	if ( FoFIntroWasWatched( m_Slide.String() ) )
	{
		m_Slide.Clear();
		m_bSlidePending = false;
		return;
	}
	m_bSlidePending = !m_Slide.IsEmpty();
	m_iSlidePageCount = 0;
}

// FoF slide video-panel implementation.

CFoFSlideVideoPanel::CFoFSlideVideoPanel( vgui::Panel *parent )
	: BaseClass( 0, 0, 1, 1, true )
{
	SetParent( parent );
	SetBlackBackground( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	SetVisible( false );
}

CFoFSlideVideoPanel::~CFoFSlideVideoPanel()
{
	StopPlayback();
}

bool CFoFSlideVideoPanel::StartPlayback( const char *filename )
{
	if ( !filename || !filename[0] )
		return false;

	if ( !BeginPlayback( filename ) || !m_VideoMaterial )
		return false;

	m_VideoMaterial->SetLooping( true );
	return true;
}

void CFoFSlideVideoPanel::StopPlayback()
{
	SetVisible( false );
	if ( g_pVideo && m_VideoMaterial )
	{
		// This intentionally mirrors VideoPanelFoF rather than stock
		// VideoPanel::OnClose: FoF does not notify end-of-movie or mark
		// the persistent child for deletion during a page transition.
		g_pVideo->DestroyVideoMaterial( m_VideoMaterial );
		m_VideoMaterial = NULL;
	}
	m_pMaterial = NULL;
}

void CFoFSlideVideoPanel::Paint()
{
	// CHudFoF paints video frames in its z=5 media pass so its
	// z=10 slide labels remain above them, matching the original panel.
}

void CFoFSlideVideoPanel::PaintVideoFrame()
{
	if ( !m_VideoMaterial || !m_pMaterial )
		return;
	if ( m_nPlaybackWidth <= 0 || m_nPlaybackHeight <= 0 )
		return;

	// VideoPanelFoF deliberately ignores the return value.  Playback is
	// looping, and slide layout/close owns the material lifetime.
	m_VideoMaterial->Update();

	int xpos = 0;
	int ypos = 0;
	// VideoPanelFoF uses the child's slide-relative item position, not
	// stock VideoPanel's centered letterbox offset.
	GetPos( xpos, ypos );

	CMatRenderContextPtr renderContext( materials );
	int viewportX = 0;
	int viewportY = 0;
	int viewportWide = 0;
	int viewportTall = 0;
	renderContext->GetViewport(
		viewportX, viewportY, viewportWide, viewportTall );
	if ( viewportWide <= 0 || viewportTall <= 0 )
		return;

	renderContext->MatrixMode( MATERIAL_VIEW );
	renderContext->PushMatrix();
	renderContext->LoadIdentity();
	renderContext->MatrixMode( MATERIAL_PROJECTION );
	renderContext->PushMatrix();
	renderContext->LoadIdentity();
	renderContext->Bind( m_pMaterial, NULL );

	CMeshBuilder meshBuilder;
	IMesh *mesh = renderContext->GetDynamicMesh( true );
	meshBuilder.Begin( mesh, MATERIAL_QUADS, 1 );

	float leftX = (float)xpos;
	float rightX = (float)( xpos + m_nPlaybackWidth - 1 );
	float topY = (float)ypos;
	float bottomY = (float)( ypos + m_nPlaybackHeight - 1 );
	const float leftU = 0.0f;
	const float topV = 0.0f;
	const float rightU =
		m_flU - ( 1.0f / (float)m_nPlaybackWidth );
	const float bottomV =
		m_flV - ( 1.0f / (float)m_nPlaybackHeight );

	rightX = FLerp( -1, 1, 0, viewportWide, rightX );
	leftX = FLerp( -1, 1, 0, viewportWide, leftX );
	topY = FLerp( 1, -1, 0, viewportTall, topY );
	bottomY = FLerp( 1, -1, 0, viewportTall, bottomY );

	const float alpha = (float)GetFgColor()[3] / 255.0f;
	for ( int corner = 0; corner < 4; ++corner )
	{
		const bool left = corner == 0 || corner == 3;
		meshBuilder.Position3f(
			left ? leftX : rightX,
			( corner & 2 ) ? bottomY : topY,
			0.0f );
		meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		meshBuilder.TexCoord2f(
			0,
			left ? leftU : rightU,
			( corner & 2 ) ? bottomV : topV );
		meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
		meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
		meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, alpha );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	mesh->Draw();
	renderContext->MatrixMode( MATERIAL_VIEW );
	renderContext->PopMatrix();
	renderContext->MatrixMode( MATERIAL_PROJECTION );
	renderContext->PopMatrix();
}
