#include "cbase.h"
#include "fof/fof_launcher_workshop.h"

#include "filesystem.h"
#include "fof/fof_content_mount.h"
#include "fof/fof_launcher_widgets.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/SectionedListPanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const AppId_t FOF_WORKSHOP_APP_ID = 265630;

static bool FoFWorkshopSortBySize(
	vgui::SectionedListPanel *pList,
	int leftItemID,
	int rightItemID )
{
	KeyValues *pLeft = pList->GetItemData( leftItemID );
	KeyValues *pRight = pList->GetItemData( rightItemID );
	if ( !pLeft || !pRight )
		return leftItemID < rightItemID;

	const int leftSize = pLeft->GetInt( "size", 0 );
	const int rightSize = pRight->GetInt( "size", 0 );
	if ( leftSize != rightSize )
		return leftSize < rightSize;
	return Q_stricmp(
		pLeft->GetString( "items", "" ),
		pRight->GetString( "items", "" ) ) < 0;
}

CFoFWorkshopPanel::CFoFWorkshopPanel( vgui::Panel *pParent )
	: BaseClass( pParent, "FoFWorkshop" )
	, m_pItemList( NULL )
	, m_pCloseButton( NULL )
	, m_hQuery( k_UGCQueryHandleInvalid )
	, m_ItemInstalledCallback(
		this, &CFoFWorkshopPanel::OnItemInstalled )
	, m_hHeaderFont( vgui::INVALID_FONT )
	, m_hItemFont( vgui::INVALID_FONT )
{
	SetProportional( false );
	SetPaintBorderEnabled( false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetZPos( 1000 );

	m_pItemList = new vgui::SectionedListPanel( this, "UGCList" );
	m_pItemList->SetProportional( false );
	m_pItemList->SetVerticalScrollbar( true );
	m_pItemList->AddSection( 0, "WorkshopItems", FoFWorkshopSortBySize );
	m_pItemList->SetSectionAlwaysVisible( 0, true );
	m_pItemList->AddColumnToSection(
		0,
		"items",
		"#UGC_Name",
		vgui::SectionedListPanel::COLUMN_BRIGHT,
		4096 );
	m_pItemList->AddActionSignalTarget( this );

	m_pCloseButton = new vgui::Button(
		this,
		"CloseButton",
		"#GameUI_Close",
		this,
		"workshop_close" );
	m_pCloseButton->DrawFocusBox( false );

	SetVisible( false );
}

CFoFWorkshopPanel::~CFoFWorkshopPanel()
{
	CancelQuery();
}

void CFoFWorkshopPanel::Open()
{
	SetVisible( true );
	MoveToFront();
	RequestFocus();
	RefreshItems();
}

void CFoFWorkshopPanel::Close()
{
	SetVisible( false );
}

void CFoFWorkshopPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	vgui::IScheme *pFoFScheme = vgui::scheme()->GetIScheme(
		vgui::scheme()->GetScheme( "ClientScheme" ) );
	m_hHeaderFont = pFoFScheme ?
		pFoFScheme->GetFont( "DefaultFoF", false ) :
		vgui::INVALID_FONT;
	m_hItemFont = pFoFScheme ?
		pFoFScheme->GetFont( "Default", false ) :
		vgui::INVALID_FONT;
	if ( m_hHeaderFont == vgui::INVALID_FONT )
		m_hHeaderFont = pScheme->GetFont( "DefaultSmall", false );
	if ( m_hItemFont == vgui::INVALID_FONT )
		m_hItemFont = pScheme->GetFont( "Default", false );

	m_pItemList->SetHeaderFont( m_hHeaderFont );
	m_pItemList->SetRowFont( m_hItemFont );
	m_pCloseButton->SetFont( m_hItemFont );
	SetPaintBorderEnabled( false );
}

void CFoFWorkshopPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	int screenWide = 0;
	int screenTall = 0;
	vgui::surface()->GetScreenSize( screenWide, screenTall );

	const int margin = MAX(
		FoFLauncherScalePixel( 2.0f, screenTall ), 1 );
	const int closeWide = MAX(
		FoFLauncherScalePixel( 60.0f, screenTall ), 1 );
	const int closeTall = MAX(
		FoFLauncherScalePixel( 15.0f, screenTall ), 1 );
	const int listBottom = MAX( tall - closeTall, margin + 1 );
	m_pItemList->SetBounds(
		margin,
		margin,
		MAX( wide - margin * 2, 1 ),
		MAX( listBottom - margin, 1 ) );
	m_pItemList->SetLineSpacing( MAX(
		FoFLauncherScalePixel( 13.0f, screenTall ),
		vgui::surface()->GetFontTall( m_hItemFont ) + 2 ) );
	m_pCloseButton->SetBounds(
		MAX( wide - closeWide, 0 ),
		MAX( tall - closeTall, 0 ),
		closeWide,
		closeTall );
}

void CFoFWorkshopPanel::PaintBackground()
{
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	vgui::surface()->DrawSetColor( 0, 0, 0, 250 );
	vgui::surface()->DrawFilledRect( 0, 0, wide, tall );
}

void CFoFWorkshopPanel::OnCommand( const char *pszCommand )
{
	if ( !Q_stricmp( pszCommand, "workshop_close" ) ||
		!Q_stricmp( pszCommand, "Close" ) )
	{
		Close();
		return;
	}
	BaseClass::OnCommand( pszCommand );
}

void CFoFWorkshopPanel::RefreshItems()
{
	CancelQuery();
	m_pItemList->RemoveAll();
	m_pItemList->ClearSelection();

	ISteamUGC *pUGC = steamapicontext ?
		steamapicontext->SteamUGC() : NULL;
	if ( !pUGC )
		return;

	const uint32 subscribedCount = pUGC->GetNumSubscribedItems();
	const uint32 queryCount = MIN(
		subscribedCount,
		static_cast< uint32 >( kNumUGCResultsPerPage ) );
	if ( queryCount == 0 )
		return;

	CUtlVector< PublishedFileId_t > itemIDs;
	itemIDs.SetCount( queryCount );
	const uint32 receivedCount = pUGC->GetSubscribedItems(
		itemIDs.Base(), queryCount );
	if ( receivedCount == 0 )
		return;
	itemIDs.SetCount( MIN( receivedCount, queryCount ) );

	m_hQuery = pUGC->CreateQueryUGCDetailsRequest(
		itemIDs.Base(), itemIDs.Count() );
	if ( m_hQuery == k_UGCQueryHandleInvalid )
		return;

	const SteamAPICall_t call = pUGC->SendQueryUGCRequest( m_hQuery );
	if ( call == k_uAPICallInvalid )
	{
		pUGC->ReleaseQueryUGCRequest( m_hQuery );
		m_hQuery = k_UGCQueryHandleInvalid;
		return;
	}
	m_QueryCall.Set(
		call,
		this,
		&CFoFWorkshopPanel::OnQueryCompleted );
}

void CFoFWorkshopPanel::CancelQuery()
{
	m_QueryCall.Cancel();
	if ( m_hQuery == k_UGCQueryHandleInvalid )
		return;

	ISteamUGC *pUGC = steamapicontext ?
		steamapicontext->SteamUGC() : NULL;
	if ( pUGC )
		pUGC->ReleaseQueryUGCRequest( m_hQuery );
	m_hQuery = k_UGCQueryHandleInvalid;
}

void CFoFWorkshopPanel::AddItem( const SteamUGCDetails_t &details )
{
	if ( details.m_eResult != k_EResultOK )
		return;

	uint64 fileSize = details.m_nFileSize > 0 ?
		static_cast< uint64 >( details.m_nFileSize ) : 0;
	if ( fileSize == 0 )
	{
		ISteamUGC *pUGC = steamapicontext ?
			steamapicontext->SteamUGC() : NULL;
		uint64 installSize = 0;
		uint32 installTime = 0;
		char installPath[MAX_PATH];
		if ( pUGC && pUGC->GetItemInstallInfo(
			details.m_nPublishedFileId,
			&installSize,
			installPath,
			sizeof( installPath ),
			&installTime ) )
		{
			fileSize = installSize;
		}
	}

	// FoF labels this as MB and uses the decimal file-size unit.  Using
	// a binary MiB divisor turns the original 8.16 MB item into 7.79 MB.
	const double megabytes =
		static_cast< double >( fileSize ) / 1000000.0;
	char itemText[512];
	Q_snprintf(
		itemText,
		sizeof( itemText ),
		"%s (%.2f MB)",
		details.m_rgchTitle[0] ? details.m_rgchTitle : "Workshop item",
		megabytes );

	KeyValues *pData = new KeyValues( "workshop_item" );
	pData->SetString( "items", itemText );
	pData->SetUint64( "file_id", details.m_nPublishedFileId );
	pData->SetInt(
		"size",
		fileSize > static_cast< uint64 >( INT_MAX ) ?
			INT_MAX : static_cast< int >( fileSize ) );
	const int itemID = m_pItemList->AddItem( 0, pData );
	// ClientScheme's generic SectionedListPanel text is orange, while the
	// shipped Workshop explicitly paints unselected project rows light grey.
	// The selected row still uses ClientScheme's black text on red.
	m_pItemList->SetItemFgColor(
		itemID, Color( 190, 190, 190, 255 ) );
	pData->deleteThis();
}

void CFoFWorkshopPanel::OnQueryCompleted(
	SteamUGCQueryCompleted_t *pResult,
	bool bIOFailure )
{
	ISteamUGC *pUGC = steamapicontext ?
		steamapicontext->SteamUGC() : NULL;
	if ( !pUGC || m_hQuery == k_UGCQueryHandleInvalid )
	{
		m_hQuery = k_UGCQueryHandleInvalid;
		return;
	}

	const UGCQueryHandle_t completedQuery = m_hQuery;
	if ( !bIOFailure && pResult &&
		pResult->m_handle == completedQuery &&
		pResult->m_eResult == k_EResultOK )
	{
		for ( uint32 index = 0;
			index < pResult->m_unNumResultsReturned;
			++index )
		{
			SteamUGCDetails_t details;
			if ( pUGC->GetQueryUGCResult(
				completedQuery, index, &details ) )
			{
				AddItem( details );
			}
		}
	}

	pUGC->ReleaseQueryUGCRequest( completedQuery );
	m_hQuery = k_UGCQueryHandleInvalid;
	m_pItemList->InvalidateLayout( true );
	m_pItemList->Repaint();
}

bool CFoFWorkshopPanel::HasItem( PublishedFileId_t fileID ) const
{
	for ( int row = 0; row < m_pItemList->GetItemCount(); ++row )
	{
		const int itemID = m_pItemList->GetItemIDFromRow( row );
		KeyValues *pData = m_pItemList->GetItemData( itemID );
		if ( pData && pData->GetUint64(
			"file_id", k_PublishedFileIdInvalid ) == fileID )
		{
			return true;
		}
	}

	return false;
}

void CFoFWorkshopPanel::OnItemInstalled( ItemInstalled_t *pResult )
{
	if ( !pResult )
		return;

	if ( pResult->m_unAppID != FOF_WORKSHOP_APP_ID ||
		HasItem( pResult->m_nPublishedFileId ) )
	{
		return;
	}

	if ( !FoFMountWorkshopItemArchives(
		filesystem,
		pResult->m_unAppID,
		static_cast< unsigned long long >(
			pResult->m_nPublishedFileId ) ) )
	{
		return;
	}

	// Reload the current launcher contents after the new archives become
	// searchable, then query Steam again so the Workshop row appears too.
	if ( GetParent() )
		GetParent()->OnCommand( "workshop_content_installed" );
	RefreshItems();
}

void CFoFWorkshopPanel::OnItemLeftClick( int itemID )
{
	OpenItem( itemID );
}

void CFoFWorkshopPanel::OpenItem( int itemID )
{
	if ( !m_pItemList->IsItemIDValid( itemID ) )
		return;
	KeyValues *pData = m_pItemList->GetItemData( itemID );
	if ( !pData )
		return;

	const PublishedFileId_t fileID =
		pData->GetUint64( "file_id", k_PublishedFileIdInvalid );
	if ( fileID == k_PublishedFileIdInvalid )
		return;

	// Selection is applied only by an actual row click.  Merely arming or
	// hovering a row never changes its background to the ClientScheme red.
	m_pItemList->SetSelectedItem( itemID );
	ISteamFriends *pFriends = steamapicontext ?
		steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	char url[160];
	Q_snprintf(
		url,
		sizeof( url ),
		"steam://url/CommunityFilePage/%llu",
		static_cast< unsigned long long >( fileID ) );
	pFriends->ActivateGameOverlayToWebPage( url );
}
