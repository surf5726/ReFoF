// FoF server crate offer catalogue and presentation.

#include "cbase.h"
#include "fof/fof_crate_menu.h"
#include "fof/fof_hud.h"
#include "c_baseplayer.h"
#include "fof/fof_player_shared.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// MenuFoF indexes these materials directly with the signed command value.
// The first eighteen slots intentionally share the shipped disabled preset.
static const char *const g_FoFCrateMaterials[FOF_CRATE_ITEM_COUNT] =
{
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/preset_disabled", "vgui/preset_disabled",
	"vgui/sawed_shotgun", "vgui/accuracy_right",
	"vgui/accuracy_left", "vgui/accuracy_fan",
	"vgui/accuracy_ambi", "vgui/gun_throw",
	"vgui/walljump", "vgui/slide",
	"vgui/heavyload", "vgui/brass_knuckles",
	"vgui/knife", "vgui/boots",
	"vgui/deringer", "vgui/dynamite",
	"vgui/heavyload", "vgui/volcanic",
	"vgui/coltnavy", "vgui/axe",
	"vgui/bow", "vgui/sawed_shotgun",
	"vgui/hammerless", "vgui/nma",
	"vgui/maresleg", "vgui/schofield",
	"vgui/carbine", "vgui/peacemaker",
	"vgui/bow_black", "vgui/henry",
	"vgui/coachgun", "vgui/spencer",
	"vgui/machete", "vgui/shotgun",
	"vgui/dynamite_black", "vgui/sharps",
	"vgui/walker", "vgui/dynamite_belt",
	"vgui/whiskey", "vgui/bow"
};

const char *FoFCrateMenuMaterial( int commandId )
{
	if ( commandId < 0 || commandId >= ARRAYSIZE( g_FoFCrateMaterials ) )
		return NULL;
	return g_FoFCrateMaterials[commandId];
}

void CHudFoF::PaintCrateMenu()
{
	EnsureMenuTextures();

	const int visibleCount = MIN(
		m_MenuEntries.Count(), FOF_CRATE_ROW_COUNT );
	if ( visibleCount <= 0 )
		return;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	const int availableCash = player ? (int)FoFCash( player ) : 0;
	const float scale = MAX( (float)ScreenHeight() / 480.0f, 0.1f );
	const int textInset = MAX(
		RoundFloatToInt( 10.0f * scale ), 1 );
	const vgui::HFont font =
		m_hSmallFont != vgui::INVALID_FONT ? m_hSmallFont : m_hFont;

	// CHudMenuFoF owns a plain background child behind the complete offer
	// list.  It covers the number label column as well as the bitmap rows;
	// drawing only the row artwork leaves the ordinals directly on the world.
	int firstX = 0;
	int firstY = 0;
	int firstWide = 0;
	int firstTall = 0;
	m_pCrateMenuButtons[0]->GetBounds(
		firstX, firstY, firstWide, firstTall );
	const int backgroundInset = MAX(
		RoundFloatToInt( 2.0f * scale ), 1 );
	vgui::surface()->DrawSetColor( Color( 50, 50, 55, 200 ) );
	vgui::surface()->DrawFilledRect(
		firstX - textInset - backgroundInset,
		firstY - backgroundInset,
		firstX + firstWide + backgroundInset,
		firstY + visibleCount * firstTall + backgroundInset );

	for ( int i = 0; i < visibleCount; ++i )
	{
		FoFMenuEntry &entry = m_MenuEntries[i];
		vgui::Button *button = m_pCrateMenuButtons[i];
		int x = 0;
		int y = 0;
		int wide = 0;
		int tall = 0;
		button->GetBounds( x, y, wide, tall );

		const bool mapped =
			entry.commandId >= 0 &&
			entry.commandId < FOF_CRATE_ITEM_COUNT;
		const bool selectable =
			entry.encodedValid &&
			entry.encodedKind == '$' &&
			mapped;
		const bool affordable =
			selectable &&
			availableCash >= entry.encodedValue;
		button->SetEnabled( selectable );

		// The original client only dims unaffordable rows.  It still forwards
		// their command to the server, which is authoritative for the purchase.
		const int alpha = affordable ? 255 : 50;
		if ( mapped )
		{
			DrawMenuTexture(
				m_iCrateItemTextures[entry.commandId],
				x, y, wide, tall,
				Color( 234, 234, 234, alpha ) );
		}

		wchar_t number[8];
		V_snwprintf(
			number,
			ARRAYSIZE( number ),
			L"%d.",
			( i + 1 ) % 10 );

		wchar_t body[512];
		body[0] = L'\0';
		if ( entry.encodedKind == '$' )
		{
			// The original only appends the server-provided localization token
			// when it resolves.  An empty or unknown token has no commandId
			// text fallback; the mapped icon still identifies the offer.
			const wchar_t *itemLabel = L"";
			if ( !entry.encodedLabel.IsEmpty() && g_pVGuiLocalize )
			{
				const wchar_t *localized =
					g_pVGuiLocalize->Find( entry.encodedLabel.String() );
				if ( localized )
					itemLabel = localized;
			}
			if ( entry.encodedQuantity != 0 )
			{
				V_snwprintf(
					body,
					ARRAYSIZE( body ),
					L"$%d  %ls x%d",
					entry.encodedValue,
					itemLabel,
					entry.encodedQuantity );
			}
			else
			{
				V_snwprintf(
					body,
					ARRAYSIZE( body ),
					L"$%d  %ls",
					entry.encodedValue,
					itemLabel );
			}
		}
		else if ( entry.encodedKind == '*' &&
			!entry.encodedLabel.IsEmpty() && g_pVGuiLocalize )
		{
			wchar_t formatBuffer[256];
			wchar_t value[32];
			const wchar_t *format = Localize(
				"#Hud.ProgressionLevel",
				formatBuffer,
				sizeof( formatBuffer ) );
			V_snwprintf(
				value,
				ARRAYSIZE( value ),
				L"%d",
				entry.encodedValue );
			g_pVGuiLocalize->ConstructString(
				body,
				sizeof( body ),
				format,
				1,
				value );
		}

		// Match the original CBitmapButton/Label path.  In particular, the
		// label's TextImage wraps long localized weapon names to a second line
		// within the 110x25 proportional row instead of drawing past its edge.
		const Color textColor( 234, 234, 234, 255 );
		button->SetFont( font );
		button->SetTextInset( textInset, 0 );
		button->SetWrap( true );
		button->SetContentAlignment( vgui::Label::a_west );
		button->SetDefaultColor( textColor, Color( 0, 0, 0, 0 ) );
		button->SetArmedColor( textColor, Color( 0, 0, 0, 0 ) );
		button->SetDepressedColor( textColor, Color( 0, 0, 0, 0 ) );
		button->SetAlpha( alpha );
		button->SetText( body );

		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor(
			Color( 234, 234, 234, alpha ) );
		const int textY = y + MAX(
			( tall - vgui::surface()->GetFontTall( font ) ) / 2, 0 );
		// The original number label begins at screen centre; the row artwork
		// begins ten proportional units to its right.
		vgui::surface()->DrawSetTextPos( x - textInset, textY );
		vgui::surface()->DrawPrintText(
			number, V_wcslen( number ) );
	}
}
