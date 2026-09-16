/*
===========================================================================
Copyright (C) 2026 FnQL contributors

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

#ifndef FNQL_QCOMMON_CM_MODEL_HANDLES_H
#define FNQL_QCOMMON_CM_MODEL_HANDLES_H

#include "q_shared.h"
#include "game_profile.h"

/*
 * Q3/QL reserve 254/255 for the temporary capsule/box. RTCW reserves 510/511
 * in its larger inline-model range. The immutable catalog selects the range;
 * both handles resolve to the same bounds-owning cmodel_t. Keep the legacy
 * constants for the standalone Q3 BSP tool and source-contract fixtures.
 */
#define MAX_SUBMODELS            256
#define CAPSULE_MODEL_HANDLE     ( MAX_SUBMODELS - 2 )
#define BOX_MODEL_HANDLE         ( MAX_SUBMODELS - 1 )

static ID_INLINE int CM_ModelHandleLimit( void ) {
#ifdef BSPC
	return MAX_SUBMODELS;
#else
	const comGameProfile_t *profile = Com_GameProfile();
	return profile ? profile->collisionModelHandleCount : 0;
#endif
}

static ID_INLINE clipHandle_t CM_TemporaryModelHandle( qboolean capsule ) {
	const int count = CM_ModelHandleLimit();
	return count >= 2 ? count - ( capsule != qfalse ? 2 : 1 ) : -1;
}

static ID_INLINE qboolean CM_IsTemporaryModelHandle( clipHandle_t handle ) {
	const int count = CM_ModelHandleLimit();
	return count >= 2 && ( handle == count - 1 || handle == count - 2 )
		? qtrue : qfalse;
}

#endif
