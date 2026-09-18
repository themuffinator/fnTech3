/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_ADD_SURFACES_H
#define FNTECH3_TR_WOLFET_ADD_SURFACES_H

#include "tr_wolfet_entity.h"
#include "tr_wolfet_material.h"

/* Enemy Territory's skeletal meshes take their own path through the frontend.
 * The shared mesh path cannot serve them: it reads frames from the model being
 * drawn, and an ET mesh has none -- its bounds, its radius and its fog test all
 * come from the skeleton the entity names beside the frame index. */

static inline int R_WolfETCullModel( const trRefEntity_t *ent, vec3_t bounds[] ) {
	const md3Frame_t *newFrame = R_WolfETEntityFrame( ent->e.wolfet.frameModel, ent->e.frame );
	const md3Frame_t *oldFrame = R_WolfETEntityFrame( ent->e.wolfet.oldframeModel, ent->e.oldframe );
	int i;
	/* A frame this entity cannot name is not drawn at all. */
	if ( !newFrame || !oldFrame ) return CULL_OUT;
	/* The two frames' boxes are merged corner for corner, as ET's own culling
	 * does, and the result is then ordered low to high. Every skeleton ET ships
	 * writes the greater corner of a frame's box first, so the merge is the two
	 * boxes' intersection rather than their union -- which is what ET draws
	 * with. Ordering afterwards changes no corner of that box, so the frustum
	 * test sees exactly what ET's does; it only lets the callers that read this
	 * pair as a range, such as the dynamic light test, read it correctly. */
	for ( i = 0; i < 3; i++ ) {
		const float first = MIN( oldFrame->bounds[0][i], newFrame->bounds[0][i] );
		const float second = MAX( oldFrame->bounds[1][i], newFrame->bounds[1][i] );
		bounds[0][i] = MIN( first, second );
		bounds[1][i] = MAX( first, second );
	}
	if ( !ent->e.nonNormalizedAxes ) {
		/* One silhouette only when both the index and the skeleton match: the
		 * same index in two skeletons is two different poses. */
		if ( ent->e.frame == ent->e.oldframe &&
			ent->e.wolfet.frameModel == ent->e.wolfet.oldframeModel ) {
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) ) {
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;
			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;
			default:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			}
		} else {
			const int sphereCull = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			const int sphereCullB = newFrame == oldFrame ? sphereCull :
				R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			if ( sphereCull == sphereCullB ) {
				if ( sphereCull == CULL_OUT ) {
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				} else if ( sphereCull == CULL_IN ) {
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				} else {
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
	}
	switch ( R_CullLocalBox( bounds ) ) {
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

static inline int R_WolfETComputeFogNum( const trRefEntity_t *ent ) {
	const md3Frame_t *frame;
	vec3_t localOrigin;
	int i, j;
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) return 0;
	frame = R_WolfETEntityFrame( ent->e.wolfet.frameModel, ent->e.frame );
	if ( !frame ) return 0;
	VectorAdd( ent->e.origin, frame->localOrigin, localOrigin );
	for ( i = 1; i < tr.world->numfogs; i++ ) {
		const fog_t *fog = &tr.world->fogs[i];
		for ( j = 0; j < 3; j++ ) {
			if ( localOrigin[j] - frame->radius >= fog->bounds[1][j] ) break;
			if ( localOrigin[j] + frame->radius <= fog->bounds[0][j] ) break;
		}
		if ( j == 3 ) return i;
	}
	return 0;
}

void R_AddWolfETSurfaces( trRefEntity_t *ent ) {
	const wolfetMeshModel_t *mesh = tr.currentModel->wolfetMesh;
	vec3_t bounds[2];
	uint32_t i;
	int fogNum;
	qboolean personalModel;
#ifdef USE_PMLIGHT
	dlight_t *dlights[ ARRAY_LEN( backEndData->dlights ) ];
	int numDlights = 0;
	int n;
#endif

	if ( !mesh ) return;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !R_ViewPassIsPortal( &tr.viewParms );

	if ( R_WolfETCullModel( ent, bounds ) == CULL_OUT ) {
		return;
	}

	if ( !personalModel || r_shadows->integer > 1 ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

#ifdef USE_PMLIGHT
	if ( !personalModel && R_GetDlightMode() >= 2 ) {
		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );
		for ( n = 0; n < tr.viewParms.num_dlights; n++ ) {
			dlight_t *dl = &tr.viewParms.dlights[ n ];
			if ( dl->rtcwShader ) continue; // Original material lights do not light models.
			if ( !R_LightCullBounds( dl, bounds[0], bounds[1] ) ) dlights[ numDlights++ ] = dl;
		}
	}
#endif

	fogNum = R_WolfETComputeFogNum( ent );

	for ( i = 0; i < mesh->view.numSurfaces; i++ ) {
		wolfetMeshSurface_t *surface = mesh->surfaces + i;
		shader_t *shader = R_WolfETSurfaceShader( &ent->e, surface );

		/* ET queues no stencil or projection shadow for a skeletal mesh; only
		 * its vertex-animated models cast those. */
		if ( !personalModel ) {
			R_AddDrawSurf( &surface->surfaceType, shader, fogNum, 0 );
		}
#ifdef USE_PMLIGHT
		if ( !personalModel && numDlights && shader->lightingStage >= 0 ) {
			for ( n = 0; n < numDlights; n++ ) {
				tr.light = dlights[ n ];
				R_AddLitSurfFlags( &surface->surfaceType, shader, fogNum, 0 );
			}
		}
#endif
	}
}

#endif
