#ifndef FOF_WORKSHOP_UPLOADER_H
#define FOF_WORKSHOP_UPLOADER_H
#ifdef _WIN32
#pragma once
#endif

bool FoFWorkshopUploadEnabled();
bool FoFBeginWorkshopUpload(
	const char *pszTitle,
	const char *pszDescription,
	const char *pszPreview );

#endif // FOF_WORKSHOP_UPLOADER_H
