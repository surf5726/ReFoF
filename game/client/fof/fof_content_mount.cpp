#include "cbase.h"
#include "fof/fof_content_mount.h"

#include "filesystem.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int CompareArchiveNames(
	const CUtlString *pLeft,
	const CUtlString *pRight )
{
	return Q_stricmp( pLeft->String(), pRight->String() );
}

static bool IsBaseContentArchive( const char *pszFileName )
{
	return !Q_stricmp( pszFileName, "fof_dir.vpk" );
}

void FoFMountContentArchives( IFileSystem *pFileSystem )
{
	if ( !pFileSystem )
		return;

	CUtlVector< CUtlString > archives;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszFileName =
		pFileSystem->FindFirstEx( "fof*_dir.vpk", "GAME", &findHandle );

	while ( pszFileName )
	{
		if ( !pFileSystem->FindIsDirectory( findHandle ) &&
			!IsBaseContentArchive( pszFileName ) )
		{
			archives.AddToTail( CUtlString( pszFileName ) );
		}

		pszFileName = pFileSystem->FindNext( findHandle );
	}

	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		pFileSystem->FindClose( findHandle );

	// AddSearchPath inserts each archive at the head. Sorting ascending before
	// insertion therefore reproduces the original final order:
	// fof_new_materials.vpk, fof2.vpk, fof.vpk.
	archives.Sort( CompareArchiveNames );

	for ( int i = 0; i < archives.Count(); ++i )
	{
		char szFullPath[MAX_PATH];
		if ( !pFileSystem->RelativePathToFullPath(
				archives[i].String(),
				"GAME",
				szFullPath,
				sizeof( szFullPath ) ) )
		{
			continue;
		}

		pFileSystem->AddSearchPath( szFullPath, "GAME", PATH_ADD_TO_HEAD );
		pFileSystem->AddSearchPath( szFullPath, "MOD", PATH_ADD_TO_HEAD );
	}
}

bool FoFMountWorkshopItemArchives(
	IFileSystem *pFileSystem,
	unsigned int appID,
	unsigned long long fileID )
{
	if ( !pFileSystem || appID == 0 || fileID == 0 )
		return false;

	char szWildcard[MAX_PATH];
	Q_snprintf(
		szWildcard,
		sizeof( szWildcard ),
		"..\\..\\..\\workshop\\content\\%u\\%llu\\*.vpk",
		appID,
		fileID );

	bool bMountedArchive = false;
	FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszFileName =
		pFileSystem->FindFirst( szWildcard, &findHandle );
	while ( pszFileName )
	{
		if ( !pFileSystem->FindIsDirectory( findHandle ) )
		{
			char szArchivePath[MAX_PATH];
			Q_snprintf(
				szArchivePath,
				sizeof( szArchivePath ),
				"..\\..\\..\\workshop\\content\\%u\\%llu\\%s",
				appID,
				fileID,
				pszFileName );

			pFileSystem->AddSearchPath(
				szArchivePath, "GAME", PATH_ADD_TO_HEAD );
			pFileSystem->AddSearchPath(
				szArchivePath, "MOD", PATH_ADD_TO_HEAD );
			bMountedArchive = true;
		}

		pszFileName = pFileSystem->FindNext( findHandle );
	}

	if ( findHandle != FILESYSTEM_INVALID_FIND_HANDLE )
		pFileSystem->FindClose( findHandle );

	return bMountedArchive;
}
