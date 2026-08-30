#ifndef FOF_HUD_SLIDE_H
#define FOF_HUD_SLIDE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"

class CFoFSlideVideoPanel;

struct FoFSlideItem
{
	int page;
	int fontSize;
	int color;
	int align;
	int sizeX;
	int sizeY;
	int posX;
	int posY;
	CUtlString text;
	int textureId;
	CFoFSlideVideoPanel *videoPanel;
};

#include "vgui_video.h"

class CFoFSlideVideoPanel : public VideoPanel
{
	DECLARE_CLASS_SIMPLE( CFoFSlideVideoPanel, VideoPanel );

public:
	CFoFSlideVideoPanel( vgui::Panel *parent );
	virtual ~CFoFSlideVideoPanel();

	bool StartPlayback( const char *filename );
	void StopPlayback();
	virtual void Paint();
	void PaintVideoFrame();
};

#endif // FOF_HUD_SLIDE_H
