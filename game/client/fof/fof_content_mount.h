#ifndef FOF_CONTENT_MOUNT_H
#define FOF_CONTENT_MOUNT_H
#ifdef _WIN32
#pragma once
#endif

class IFileSystem;

// Mount FoF update archives ahead of the base content archive. The original
// client performs this at runtime, so these archives intentionally do not
// appear in gameinfo.txt.
void FoFMountContentArchives( IFileSystem *pFileSystem );

// Mount every VPK shipped by a newly installed Workshop item.  FoF listens
// for Steam's ItemInstalled_t callback and adds these archives immediately,
// so a client restart is not required before their maps and course data can
// be discovered.
bool FoFMountWorkshopItemArchives(
	IFileSystem *pFileSystem,
	unsigned int appID,
	unsigned long long fileID );

#endif // FOF_CONTENT_MOUNT_H
