/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef RTX_RT_MOTION_H
#define RTX_RT_MOTION_H

typedef struct {
	qhandle_t model;
	int cls, identity;             /* profile-owned entity number, -1 if absent */
	uint32_t first, count;
	uint32_t materialGeneration;
	uint64_t topology;             /* connectivity, vertex order, materials and UVs */
	vec3_t origin;
	qboolean taken;
} rtMotionEntity_t;

static qboolean RT_MotionSamePart( const rtMotionEntity_t *a, const rtMotionEntity_t *b )
{
	return a->model == b->model && a->cls == b->cls && a->identity == b->identity &&
		a->count == b->count && a->materialGeneration == b->materialGeneration && a->topology == b->topology;
}

/* Without an identity, only a unique part in both frames is safe. A nearest
 * neighbour heuristic can exchange two players or pickups when they cross or
 * submission order changes. Ambiguity discards history, never current pose. */
static int RT_MotionMatch( const rtMotionEntity_t *current, uint32_t numCurrent, uint32_t index,
	const rtMotionEntity_t *previous, uint32_t numPrevious, float maxDistance )
{
	const rtMotionEntity_t *now = &current[index];
	int match = -1;
	uint32_t k;
	float d2 = 0.0f;
	for ( k = 0; k < numCurrent; k++ ) {
		if ( k != index && RT_MotionSamePart( now, &current[k] ) ) return -1;
	}
	for ( k = 0; k < numPrevious; k++ ) {
		if ( !RT_MotionSamePart( now, &previous[k] ) ) continue;
		if ( match >= 0 || previous[k].taken ) return -1;
		match = (int)k;
	}
	if ( match < 0 ) return -1;
	for ( k = 0; k < 3; k++ ) {
		float delta = now->origin[k] - previous[match].origin[k];
		d2 += delta * delta;
	}
	return d2 <= maxDistance * maxDistance ? match : -1;
}

#endif
