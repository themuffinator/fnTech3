/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_DLIGHT_VIEW_CULL_H
#define FNTECH3_TR_DLIGHT_VIEW_CULL_H

/* A dynamic light whose reach lies wholly outside one of a view's four side
 * planes lights nothing that view draws: the per-pixel pass ends at the
 * light's radius, and every fragment the view draws lies inside those planes.
 * The world walk already passes such a light by (R_CullDlight), but a model
 * tested its lights only against its own bounds, so a light behind the eye
 * still listed every model in view that its sphere touched, and the shadow
 * planner gave it a cube. Medal of Honor's truck ride starts so, with the
 * lantern hanging behind the player
 * (docs/fntech3/2026-10-26-mohaa-truck-ride-lights.md).
 *
 * R_DlightOutsideFrustum takes world-space points: a point light's origin,
 * or both ends of a linear light, whose reach is the capsule around them.
 * R_MarkDlightsOutsideView marks the view's lights once, before any surface is
 * added, and R_LightCullBounds refuses a marked light; r_nocull keeps them. */
static ID_INLINE qboolean R_DlightOutsideFrustum( const vec3_t origin, const vec3_t origin2, qboolean linear,
	float radius, const cplane_t *frustum ) {
	int i;

	for ( i = 0; i < 4; i++ ) {
		const float d = DotProduct( origin, frustum[i].normal ) - frustum[i].dist;
		if ( d >= -radius ) {
			continue;
		}
		if ( !linear || DotProduct( origin2, frustum[i].normal ) - frustum[i].dist < -radius ) {
			return qtrue;
		}
	}
	return qfalse;
}

#ifdef USE_PMLIGHT
static void R_MarkDlightsOutsideView( void ) {
	const qboolean keepAll = ( r_nocull && r_nocull->integer ) ? qtrue : qfalse;
	int i;

	for ( i = 0; i < tr.viewParms.num_dlights; i++ ) {
		dlight_t *dl = &tr.viewParms.dlights[i];
		dl->outsideView = !keepAll &&
			R_DlightOutsideFrustum( dl->origin, dl->origin2, dl->linear, dl->radius, tr.viewParms.frustum );
	}
}
#endif

#endif
