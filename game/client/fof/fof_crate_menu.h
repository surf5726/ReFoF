#ifndef FOF_CRATE_MENU_H
#define FOF_CRATE_MENU_H
#ifdef _WIN32
#pragma once
#endif

// Returns the material mapped by the original MenuFoF command id, or NULL
// when the id is outside the crate catalogue.
const char *FoFCrateMenuMaterial( int commandId );

#endif
