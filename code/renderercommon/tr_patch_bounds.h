/* Conservative broad phase for the legacy patch stitching and LoD passes.
 * Keep the same 0.1-unit tolerance and float subtraction as the vertex tests.
 * Inserted rows/columns rebuild meshBounds, so this also covers new vertices. */
#ifndef TR_PATCH_BOUNDS_H
#define TR_PATCH_BOUNDS_H

static qboolean R_PatchBoundsOverlap( const vec3_t bounds1[2], const vec3_t bounds2[2] ) {
	int axis;
	for ( axis = 0; axis < 3; axis++ ) {
		if ( bounds1[0][axis] - bounds2[1][axis] > .1 ||
			bounds2[0][axis] - bounds1[1][axis] > .1 ) {
			return qfalse;
		}
	}
	return qtrue;
}

#endif
