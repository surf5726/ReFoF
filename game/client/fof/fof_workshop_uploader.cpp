#include "cbase.h"
#include "fof/fof_workshop_uploader.h"
#include "filesystem.h"
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The original immediately creates a real FriendsOnly Workshop item. Keep
// that external account mutation behind an explicit per-session opt-in.
static ConVar fof_workshop_upload(
	"fof_workshop_upload",
	"0",
	FCVAR_NONE,
	"Allow FoF to create or update real Steam Workshop items.",
	true, 0.0f, true, 1.0f );

static bool FoFValidateWorkshopDirectoryRecursive(
	const char *pszDirectory,
	bool &bFoundMap,
	bool &bFoundNavigation )
{
	char wildcard[MAX_PATH * 2];
	Q_snprintf( wildcard, sizeof( wildcard ), "%s\\*", pszDirectory );

	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszName = filesystem->FindFirstEx(
		wildcard, NULL, &findHandle );
	if ( !pszName )
	{
		if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
			filesystem->FindClose( findHandle );
		return false;
	}

	bool bValid = true;
	do
	{
		if ( !Q_stricmp( pszName, "." ) || !Q_stricmp( pszName, ".." ) )
			continue;

		char fullPath[MAX_PATH * 2];
		Q_snprintf(
			fullPath, sizeof( fullPath ), "%s\\%s",
			pszDirectory, pszName );

		if ( filesystem->FindIsDirectory( findHandle ) )
		{
			if ( !FoFValidateWorkshopDirectoryRecursive(
				fullPath, bFoundMap, bFoundNavigation ) )
			{
				bValid = false;
				break;
			}
			continue;
		}

		if ( V_stristr( pszName, ".bsp" ) )
			bFoundMap = true;
		else if ( V_stristr( pszName, ".nav" ) )
			bFoundNavigation = true;
		else if ( !V_stristr( pszName, ".vpk" ) )
		{
			Warning(
				"Failed! Only .BSP .NAV and .VPK file types allowed\n" );
			bValid = false;
			break;
		}
	}
	while ( ( pszName = filesystem->FindNext( findHandle ) ) != NULL );

	filesystem->FindClose( findHandle );
	return bValid;
}

static bool FoFValidateWorkshopContent( const char *pszContent )
{
	if ( !pszContent || !pszContent[0] )
		return false;

	// The shipped command accepts a standalone VPK without opening it.
	if ( V_stristr( pszContent, ".vpk" ) )
		return true;

	bool bFoundMap = false;
	bool bFoundNavigation = false;
	if ( !FoFValidateWorkshopDirectoryRecursive(
		pszContent, bFoundMap, bFoundNavigation ) )
	{
		return false;
	}

	if ( !bFoundMap || !bFoundNavigation )
	{
		Warning(
			"Failed! Can't upload a folder without a map and navigation files\n" );
		return false;
	}

	return true;
}

static bool FoFCanMutateWorkshop()
{
	if ( fof_workshop_upload.GetBool() )
		return true;

	Warning(
		"[FoF] Workshop upload is disabled. Set fof_workshop_upload 1 "
		"for this session before using an upload command.\n" );
	return false;
}

class CFoFWorkshopUploader
{
public:
	CFoFWorkshopUploader()
		: m_bBusy( false )
		, m_bCreating( false )
		, m_nPublishedFileId( k_PublishedFileIdInvalid )
	{
		m_szTitle[0] = '\0';
		m_szDescription[0] = '\0';
		m_szContent[0] = '\0';
		m_szPreview[0] = '\0';
	}

	bool BeginCreate(
		const char *pszContent,
		const char *pszTitle,
		const char *pszDescription,
		const char *pszPreview )
	{
		ISteamUGC *pUGC =
			steamapicontext ? steamapicontext->SteamUGC() : NULL;
		if ( m_bBusy || !pUGC || !pszContent || !pszTitle || !pszPreview )
			return false;

		Q_strncpy( m_szContent, pszContent, sizeof( m_szContent ) );
		Q_strncpy( m_szTitle, pszTitle, sizeof( m_szTitle ) );
		Q_strncpy(
			m_szDescription,
			pszDescription ? pszDescription : "",
			sizeof( m_szDescription ) );
		Q_strncpy( m_szPreview, pszPreview, sizeof( m_szPreview ) );

		const SteamAPICall_t call = pUGC->CreateItem(
			265630,
			k_EWorkshopFileTypeCommunity );
		if ( call == k_uAPICallInvalid )
			return false;

		m_bBusy = true;
		m_bCreating = true;
		m_CreateItemCall.Set(
			call,
			this,
			&CFoFWorkshopUploader::OnCreateItem );
		return true;
	}

	bool BeginUpdate(
		PublishedFileId_t nPublishedFileId,
		const char *pszContent,
		const char *pszPreview )
	{
		if ( m_bBusy || nPublishedFileId == k_PublishedFileIdInvalid ||
			!pszContent || !pszPreview )
		{
			return false;
		}

		Q_strncpy( m_szContent, pszContent, sizeof( m_szContent ) );
		Q_strncpy( m_szPreview, pszPreview, sizeof( m_szPreview ) );
		m_szTitle[0] = '\0';
		m_szDescription[0] = '\0';
		m_nPublishedFileId = nPublishedFileId;
		m_bBusy = true;
		m_bCreating = false;

		if ( !StartItemUpdate() )
		{
			m_bBusy = false;
			return false;
		}
		return true;
	}

private:
	void OnCreateItem( CreateItemResult_t *pResult, bool bIOFailure )
	{
		if ( bIOFailure || !pResult ||
			pResult->m_eResult != k_EResultOK )
		{
			Warning(
				"[FoF] Workshop CreateItem failed (io=%d result=%d)\n",
				bIOFailure ? 1 : 0,
				pResult ? static_cast< int >( pResult->m_eResult ) : -1 );
			m_bBusy = false;
			return;
		}

		ISteamUGC *pUGC =
			steamapicontext ? steamapicontext->SteamUGC() : NULL;
		if ( !pUGC )
		{
			m_bBusy = false;
			return;
		}

		m_nPublishedFileId = pResult->m_nPublishedFileId;
		if ( !StartItemUpdate() )
			m_bBusy = false;
	}

	bool StartItemUpdate()
	{
		ISteamUGC *pUGC =
			steamapicontext ? steamapicontext->SteamUGC() : NULL;
		if ( !pUGC )
			return false;

		const UGCUpdateHandle_t update = pUGC->StartItemUpdate(
			265630,
			m_nPublishedFileId );
		if ( update == k_UGCUpdateHandleInvalid )
			return false;

		bool configured = true;
		if ( m_bCreating )
		{
			const char *tags[] = { "need approval" };
			SteamParamStringArray_t tagArray;
			tagArray.m_ppStrings = tags;
			tagArray.m_nNumStrings = ARRAYSIZE( tags );

			if ( m_szTitle[0] != '0' )
				configured = pUGC->SetItemTitle( update, m_szTitle );
			if ( configured && m_szDescription[0] != '0' )
				configured = pUGC->SetItemDescription(
					update, m_szDescription );
			configured = configured && pUGC->SetItemVisibility(
				update,
				k_ERemoteStoragePublishedFileVisibilityFriendsOnly );
			configured = configured && pUGC->SetItemTags(
				update, &tagArray );
		}

		if ( configured && m_szContent[0] != '0' )
			configured = pUGC->SetItemContent( update, m_szContent );
		if ( configured && m_szPreview[0] != '0' )
			configured = pUGC->SetItemPreview( update, m_szPreview );
		if ( !configured )
		{
			Warning( "[FoF] Workshop item metadata setup failed\n" );
			return false;
		}

		const SteamAPICall_t call = pUGC->SubmitItemUpdate(
			update, m_bCreating ? "1st" : NULL );
		if ( call == k_uAPICallInvalid )
			return false;
		m_SubmitItemCall.Set(
			call,
			this,
			&CFoFWorkshopUploader::OnSubmitItem );
		return true;
	}

	void OnSubmitItem(
		SubmitItemUpdateResult_t *pResult,
		bool bIOFailure )
	{
		const bool succeeded =
			!bIOFailure && pResult &&
			pResult->m_eResult == k_EResultOK;
		if ( succeeded && steamapicontext &&
			steamapicontext->SteamFriends() )
		{
			char url[160];
			Q_snprintf(
				url,
				sizeof( url ),
				"steam://url/CommunityFilePage/%llu",
				static_cast< unsigned long long >( m_nPublishedFileId ) );
			steamapicontext->SteamFriends()->
				ActivateGameOverlayToWebPage( url );
		}
		else
		{
			Warning(
				"[FoF] Workshop SubmitItemUpdate failed (io=%d result=%d)\n",
				bIOFailure ? 1 : 0,
				pResult ? static_cast< int >( pResult->m_eResult ) : -1 );
		}
		m_bBusy = false;
	}

	bool m_bBusy;
	bool m_bCreating;
	PublishedFileId_t m_nPublishedFileId;
	char m_szTitle[256];
	char m_szDescription[256];
	char m_szContent[MAX_PATH * 2];
	char m_szPreview[MAX_PATH * 2];
	CCallResult< CFoFWorkshopUploader, CreateItemResult_t > m_CreateItemCall;
	CCallResult< CFoFWorkshopUploader, SubmitItemUpdateResult_t >
		m_SubmitItemCall;
};

static CFoFWorkshopUploader g_FoFWorkshopUploader;

bool FoFWorkshopUploadEnabled()
{
	return fof_workshop_upload.GetBool();
}

bool FoFBeginWorkshopUpload(
	const char *pszTitle,
	const char *pszDescription,
	const char *pszPreview )
{
	return g_FoFWorkshopUploader.BeginCreate(
		"0", pszTitle, pszDescription, pszPreview );
}

CON_COMMAND(
	ugc_create,
	"creates and upload to workshop a new user generated item. Usage: "
	"path-to-content-folder path-to-preview-image title description" )
{
	if ( args.ArgC() != 5 )
	{
		Warning(
			"Not enough arguments (4)! CONTENT-URL PREVIEW-IMG-URL "
			"TITLE DESCRIPTION (check each argument is inside quotation marks)\n" );
		return;
	}
	if ( !FoFCanMutateWorkshop() ||
		!FoFValidateWorkshopContent( args[1] ) )
	{
		return;
	}

	if ( !g_FoFWorkshopUploader.BeginCreate(
		args[1], args[3], args[4], args[2] ) )
	{
		Warning( "[FoF] Workshop create request could not be started.\n" );
	}
}

CON_COMMAND(
	ugc_update,
	"updates an UGC item. Usage: item_id path-to-content-folder "
	"path-to-preview-image" )
{
	if ( args.ArgC() != 4 )
	{
		Warning(
			"Not enough arguments (3)! WORKSHOP-ITEM-ID CONTENT-URL "
			"PREVIEW-IMG-URL (check each argument is inside quotation marks)\n" );
		return;
	}
	if ( !FoFCanMutateWorkshop() ||
		!FoFValidateWorkshopContent( args[2] ) )
	{
		return;
	}

	const PublishedFileId_t publishedFileId =
		static_cast< PublishedFileId_t >( Q_atoui64( args[1] ) );
	if ( publishedFileId == k_PublishedFileIdInvalid ||
		publishedFileId == 0 )
	{
		Warning( "[FoF] Invalid Workshop item ID.\n" );
		return;
	}

	if ( !g_FoFWorkshopUploader.BeginUpdate(
		publishedFileId, args[2], args[3] ) )
	{
		Warning( "[FoF] Workshop update request could not be started.\n" );
	}
}
