/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_MODEL_TYPES_H
#define FNTECH3_TR_WOLFET_MODEL_TYPES_H

#include "tr_wolfet_lod.h"
#include "tr_wolfet_pose.h"

/* Enemy Territory keeps a mesh and the skeleton that poses it in separate
 * files, so each is a model of its own here. A skeleton owns no surfaces and is
 * never drawn by itself; a mesh owns no frames and cannot be drawn without one.
 * ET's reference entity names a skeleton beside every frame index, so which
 * skeleton a mesh is posed with is decided per entity, not at load. */
typedef struct wolfetMeshModel_s wolfetMeshModel_t;

typedef struct {
	surfaceType_t surfaceType;
	fnWolfETMeshSurfaceView_t view;
	const wolfetMeshModel_t *model;
	/* Native vertex descriptors point into the owned, immutable bytes. A mesh
	 * vertex is variable length, so it can be walked but not indexed. */
	fnWolfETVertexView_t *vertices;
	char name[MAX_QPATH];
	int shader;
} wolfetMeshSurface_t;

struct wolfetMeshModel_s {
	fnWolfETMeshView_t view;
	wolfetMeshSurface_t *surfaces;
};

typedef struct {
	fnWolfETSkeletonView_t view;
	md3Frame_t *frames;
} wolfetSkeletonModel_t;

#endif
