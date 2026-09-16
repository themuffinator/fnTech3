/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDC_MODEL_TYPES_H
#define FNTECH3_TR_MDC_MODEL_TYPES_H

#include "tr_mdc_decode.h"

/* Engine objects: disk bytes remain immutable and are never cast to these. */
typedef struct {
	surfaceType_t surfaceType;
	fnMdcSurfaceView_t view;
	char name[MAX_QPATH];
	int *shaders;
} mdcMeshSurface_t;

typedef struct {
	fnMdcView_t view;
	/* Canonical frame metadata shared with the existing mesh culling path. */
	md3Frame_t *frames;
	mdcMeshSurface_t *surfaces;
} mdcModel_t;

extern fnMdcNormals_t r_mdcNormals;

#endif
