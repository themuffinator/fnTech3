/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_mesh.c: triangle model functions

#include "tr_local.h"
#include "../renderercommon/tr_q3_ihv_model.h"
#include "../renderercommon/tr_mesh_view.h"
#include "../renderercommon/tr_mds_entity.h"
#include "../renderercommon/tr_mds_material.h"
#include "../renderercommon/tr_wolfet_add_surfaces.h"

static float ProjectRadius( float r, vec3_t location )
{
	float pr;
	float dist;
	float c;
	vec3_t	p;
	float	projected[4];

	c = DotProduct( tr.viewParms.or.axis[0], tr.viewParms.or.origin );
	dist = DotProduct( tr.viewParms.or.axis[0], location ) - c;

	if ( dist <= 0 )
		return 0;

	p[0] = 0;
	p[1] = fabs( r );
	p[2] = -dist;

#if 0
	projected[0] = p[0] * tr.viewParms.projectionMatrix[0] +
					p[1] * tr.viewParms.projectionMatrix[4] +
					p[2] * tr.viewParms.projectionMatrix[8] +
					tr.viewParms.projectionMatrix[12];
#endif
	projected[1] = p[0] * tr.viewParms.projectionMatrix[1] +
					p[1] * tr.viewParms.projectionMatrix[5] +
					p[2] * tr.viewParms.projectionMatrix[9] +
					tr.viewParms.projectionMatrix[13];
#if 0
	projected[2] = p[0] * tr.viewParms.projectionMatrix[2] +
					p[1] * tr.viewParms.projectionMatrix[6] +
					p[2] * tr.viewParms.projectionMatrix[10] +
					tr.viewParms.projectionMatrix[14];
#endif
	projected[3] = p[0] * tr.viewParms.projectionMatrix[3] +
					p[1] * tr.viewParms.projectionMatrix[7] +
					p[2] * tr.viewParms.projectionMatrix[11] +
					tr.viewParms.projectionMatrix[15];

	pr = projected[1] / projected[3];

	if ( pr > 1.0f )
		pr = 1.0f;

	return pr;
}


/*
=============
R_MD3ModelBounds
=============
*/
static void R_MD3ModelBounds( const model_t *model, int lod, const trRefEntity_t *ent,
	vec3_t mins, vec3_t maxs ) {
	const md3Frame_t	*oldFrame, *newFrame;
	int			i;

	newFrame = R_MeshFrame( model, lod, ent->e.frame );
	oldFrame = R_MeshFrame( model, lod, ent->e.oldframe );

	for ( i = 0; i < 3; i++ ) {
		mins[i] = MIN( oldFrame->bounds[0][i], newFrame->bounds[0][i] );
		maxs[i] = MAX( oldFrame->bounds[1][i], newFrame->bounds[1][i] );
	}
}

/*
=============
R_CullModel
=============
*/
static int R_CullModel( const model_t *model, int lod, const trRefEntity_t *ent, vec3_t bounds[] ) {
	//vec3_t bounds[2];
	const md3Frame_t	*oldFrame, *newFrame;

	// compute frame pointers
	newFrame = R_MeshFrame( model, lod, ent->e.frame );
	oldFrame = R_MeshFrame( model, lod, ent->e.oldframe );

	R_MD3ModelBounds( model, lod, ent, bounds[0], bounds[1] );

	/* Keep bounds available to lighting/caster callers even on a sphere return.
	 * Native 0x4420b0 expands by origin-oldorigin, independently of backlerp. */
	if ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active &&
		!Q3IHV_ModelBounds( ent->e.q3IHV.flags, oldFrame->bounds, newFrame->bounds,
			ent->e.origin, ent->e.oldorigin, bounds ) ) return CULL_OUT;

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;

			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;

			case CULL_CLIP:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			}
		}
		else
		{
			int sphereCull, sphereCullB;

			sphereCull  = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			if ( newFrame == oldFrame && !( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active ) ) {
				sphereCullB = sphereCull;
			} else {
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == CULL_OUT )
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				else if ( sphereCull == CULL_IN )
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
	}

	switch ( R_CullLocalBox( bounds ) )
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}


/*
=================
R_ComputeLOD
=================
*/
int R_ComputeLOD( trRefEntity_t *ent ) {
	float radius;
	float flod, lodscale;
	float projectedRadius;
	const md3Frame_t *frame;
	mdrHeader_t *mdr;
	mdrFrame_t *mdrframe;
	int lod;

	if ( tr.currentModel->numLods < 2 )
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD

		if(tr.currentModel->type == MOD_MDR)
		{
			int frameSize;
			mdr = (mdrHeader_t *) tr.currentModel->modelData;
			frameSize = (size_t) (&((mdrFrame_t *)0)->bones[mdr->numBones]);
			
			mdrframe = (mdrFrame_t *) ((byte *) mdr + mdr->ofsFrames + frameSize * ent->e.frame);
			
			radius = RadiusFromBounds(mdrframe->bounds[0], mdrframe->bounds[1]);
		}
		else
		{
			frame = R_MeshFrame( tr.currentModel, 0, ent->e.frame );

			radius = RadiusFromBounds( frame->bounds[0], frame->bounds[1] );
		}

		if ( ( projectedRadius = ProjectRadius( radius, ent->e.origin ) ) != 0 )
		{
			lodscale = r_lodscale->value;
			if (lodscale > 20) lodscale = 20;
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		lod = myftol( flod );

		if ( lod < 0 )
		{
			lod = 0;
		}
		else if ( lod >= tr.currentModel->numLods )
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodbias->integer;
	
	if ( lod >= tr.currentModel->numLods )
		lod = tr.currentModel->numLods - 1;
	if ( lod < 0 )
		lod = 0;

	return lod;
}


/*
=================
R_ComputeFogNum
=================
*/
static int R_ComputeFogNum( const model_t *model, int lod, const trRefEntity_t *ent ) {
	int				i, j;
	const fog_t			*fog;
	const md3Frame_t		*md3Frame;
	vec3_t			localOrigin;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	// FIXME: non-normalized axis issues
	md3Frame = R_MeshFrame( model, lod, ent->e.frame );
	VectorAdd( ent->e.origin, md3Frame->localOrigin, localOrigin );
	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( localOrigin[j] - md3Frame->radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( localOrigin[j] + md3Frame->radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}


/*
=================
R_AddMD3Surfaces
=================
*/
void R_AddMD3Surfaces( trRefEntity_t *ent,
	dlightShadowCasterContext_t *casterContext ) {
	vec3_t			bounds[2];
	int				i;
	meshSurfaceView_t surface;
	const int frameCount = R_MeshFrameCount( tr.currentModel );
	int numSurfaces;
	shader_t		*shader = NULL;
	int				cull;
	int				lod;
	int				fogNum = 0;
	qboolean		personalModel;
	qboolean		personalShadowCaster;
#ifdef USE_PMLIGHT
	dlight_t		*dl;
	int				n;
	dlight_t		*dlights[ ARRAY_LEN( backEndData->dlights ) ];
	int				numDlights;
#endif

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !R_ViewPassIsPortal( &tr.viewParms );
	personalShadowCaster = personalModel && !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ));

	if ( tr.currentModel->type == MOD_MDS ) {
		R_MdsSanitizeFrames( &tr.currentModel->mds->view, &ent->e, ri.gameId );
	} else if ( ent->e.renderfx & RF_CAP_FRAMES ) {
		// Held on the last frame before any wrap; a negative frame is not
		// held (stvoy.exe R_AddMD3Surfaces 0x461f8b).
		if ( ent->e.frame > frameCount - 1 ) ent->e.frame = frameCount - 1;
		if ( ent->e.oldframe > frameCount - 1 ) ent->e.oldframe = frameCount - 1;
	} else if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		ent->e.frame %= frameCount;
		ent->e.oldframe %= frameCount;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ( (ent->e.frame >= frameCount)
		|| (ent->e.frame < 0)
		|| (ent->e.oldframe >= frameCount)
		|| (ent->e.oldframe < 0) ) {
			ri.Printf( PRINT_DEVELOPER, "R_AddMD3Surfaces: no such frame %d to %d for '%s'\n",
				ent->e.oldframe, ent->e.frame,
				tr.currentModel->name );
			ent->e.frame = 0;
			ent->e.oldframe = 0;
	}

	//
	// The canonical shadow-caster list is independent of the camera and uses
	// the model's highest-detail geometry for stable sustained-light shadows.
#ifdef USE_PMLIGHT
	/* ET's limbo panel asks for its heads at full detail, which the other
	 * profiles have no flag for; the field is only ever set from ET's own
	 * render record. */
	lod = ( casterContext || ent->e.wolfet.forceNoLod ) ? 0 : R_ComputeLOD( ent );
#else
	(void)casterContext;
	lod = R_ComputeLOD( ent );
#endif

	numSurfaces = R_MeshSurfaceCount( tr.currentModel, lod );

#ifdef USE_PMLIGHT
	numDlights = 0;
	if ( casterContext ) {
		const md3Frame_t *newFrame = R_MeshFrame( tr.currentModel, lod, ent->e.frame );
		const md3Frame_t *oldFrame = R_MeshFrame( tr.currentModel, lod, ent->e.oldframe );
		R_MD3ModelBounds( tr.currentModel, lod, ent, bounds[0], bounds[1] );
		if ( ri.gameId == COM_GAME_QUAKE3_IHV && ent->e.q3IHV.active &&
			!Q3IHV_ModelBounds( ent->e.q3IHV.flags, oldFrame->bounds, newFrame->bounds,
				ent->e.origin, ent->e.oldorigin, bounds ) ) return;
		if ( R_DlightShadowEntityBoundsCulled( casterContext, ent,
			bounds[0], bounds[1] ) ) {
			return;
		}
	} else
#endif
	{
		//
		// cull the entire model if merged bounding box of both frames
		// is outside the view frustum.
		//
		cull = R_CullModel( tr.currentModel, lod, ent, bounds );
		if ( cull == CULL_OUT ) {
			return;
		}

		//
		// set up lighting now that we know we aren't culled
		//
		if ( !personalModel || r_shadows->integer > 1 ) {
			R_SetupEntityLighting( &tr.refdef, ent );
		}

#ifdef USE_PMLIGHT
		if ( R_GetDlightMode() >= 2 && !personalModel ) {
			R_TransformDlights( tr.viewParms.num_dlights,
				tr.viewParms.dlights, &tr.or );
			for ( n = 0; n < tr.viewParms.num_dlights; n++ ) {
				dl = &tr.viewParms.dlights[ n ];
				if ( dl->rtcwShader ) continue; // Original material lights do not light models.
				if ( !R_LightCullBounds( dl, bounds[0], bounds[1] ) )
					dlights[ numDlights++ ] = dl;
			}
		}
#endif

		//
		// see if we are in a fog volume
		//
		fogNum = R_ComputeFogNum( tr.currentModel, lod, ent );
	}

	//
	// draw all surfaces
	//
	if ( !numSurfaces ) return;
	R_MeshFirstSurface( tr.currentModel, lod, &surface );
	for ( i = 0 ; i < numSurfaces ; i++ ) {
		R_MeshSurfaceDescribe( &surface );

		if ( R_MeshSurfaceHidden( &ent->e, surface.name ) ) {
			R_MeshNextSurface( &surface );
			continue;
		}
		if ( surface.mds ) {
			shader = R_MdsSurfaceShader( &ent->e, surface.mds );
		} else if ( ent->e.customShader ) {
			shader = R_GetShaderByHandle( ent->e.customShader );
		} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
			const skin_t *skin;

			skin = R_GetSkinByHandle( ent->e.customSkin );

			shader = R_MeshSkinShaderAt( &ent->e, skin, surface.name, i );
			if ( !casterContext && shader == tr.defaultShader ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no shader for surface %s in skin %s\n", surface.name, skin->name);
			}
			else if ( !casterContext && shader->defaultShader ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: shader %s in skin %s not found\n", shader->name, skin->name);
			}
		} else if ( surface.numShaders <= 0 ) {
			shader = tr.defaultShader;
		} else {
			shader = R_GetShaderByHandle( R_MeshShaderIndex( &surface, ent->e.skinNum ) );
		}

#ifdef USE_PMLIGHT
		if ( casterContext ) {
			if ( !R_AddDlightShadowEntityCasterSurface( casterContext,
				surface.data, shader ) ) {
				return;
			}
			R_MeshNextSurface( &surface );
			continue;
		}
#endif

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if ( !personalModel
			&& r_shadows->integer == 2 
			&& fogNum == 0
			&& !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) ) 
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( surface.data, tr.shadowShader, 0, 0 );
		}

		// projection shadows work fine with personal models
		if ( r_shadows->integer == 3
			&& fogNum == 0
			&& (ent->e.renderfx & RF_SHADOW_PLANE )
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( surface.data, tr.projectionShadowShader, 0, 0 );
		}

		if ( personalShadowCaster && shader->sort == SS_OPAQUE ) {
			R_AddDrawSurfFlags( surface.data, shader, fogNum, 0, DSF_SHADOW_CASTER_ONLY );
		}

		// don't add third_person objects if not viewing through a portal
		if ( !personalModel ) {
			R_AddDrawSurf( surface.data, shader, fogNum, 0 );
			tr.needScreenMap |= shader->hasScreenMap;
		}

#ifdef USE_PMLIGHT
		if ( !personalModel && numDlights && shader->lightingStage >= 0 ) {
			for ( n = 0; n < numDlights; n++ ) {
				dl = dlights[ n ];
				tr.light = dl;
				R_AddLitSurf( surface.data, shader, fogNum );
			}
		}
#endif

		R_MeshNextSurface( &surface );
	}
}
