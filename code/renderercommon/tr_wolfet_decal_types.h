/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_DECAL_TYPES_H
#define FNTECH3_TR_WOLFET_DECAL_TYPES_H

#include "tr_wolfet_decal.h"

/* One Enemy Territory decal as it is drawn this frame: a copy of the kept
 * decal, taken before the kept one fades again. */
typedef struct {
	surfaceType_t surfaceType;
	int numVerts;
	fnWolfETDecalVert_t verts[FN_WOLFET_MAX_DECAL_VERTS];
} srfWolfETDecal_t;

#endif
