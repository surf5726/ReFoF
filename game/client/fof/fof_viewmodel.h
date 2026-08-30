#ifndef FOF_VIEWMODEL_H
#define FOF_VIEWMODEL_H
#ifdef _WIN32
#pragma once
#endif

class CBaseViewModel;
class CBasePlayer;
class CBaseCombatWeapon;
class ConVar;
class Vector;
class QAngle;

extern ConVar fof_viewmodel_protected_sequences;

void FoFApplyViewModelTransform(
	CBaseViewModel *pViewModel,
	CBasePlayer *pOwner,
	CBaseCombatWeapon *pWeapon,
	const Vector &eyePosition,
	Vector &viewModelOrigin,
	QAngle &viewModelAngles );

class C_BaseViewModel;

bool FoFShouldFlipViewModel( C_BaseViewModel *pViewModel );
bool FoFShouldDrawViewModel( C_BaseViewModel *pViewModel );
void FoFRefreshSourceTVViewModelVisibility();

#endif // FOF_VIEWMODEL_H
