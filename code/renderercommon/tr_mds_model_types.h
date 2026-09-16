/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDS_MODEL_TYPES_H
#define FNTECH3_TR_MDS_MODEL_TYPES_H

#include "tr_mds_pose.h"

typedef struct mdsModel_s mdsModel_t;
typedef struct {
	surfaceType_t surfaceType;
	fnMdsSurfaceView_t view;
	const mdsModel_t *model;
	/* Native vertex descriptors point into the owned, immutable weight bytes. */
	fnMdsVertexView_t *vertices;
	char name[MAX_QPATH];
	int shader;
} mdsMeshSurface_t;

struct mdsModel_s {
	fnMdsView_t view;
	md3Frame_t *frames;
	mdsMeshSurface_t *surfaces;
};

#endif
