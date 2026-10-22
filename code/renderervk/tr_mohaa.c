/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault's models in the Vulkan renderer

This is the OpenGL renderer's code/renderer/tr_mohaa.c with this renderer's
tessellation rules: a surface that writes into tess flushes any queued world
VBO items first and names its surface type, and an entity surface whose
shader samples the screen asks the scene for a screen map.

A TIKI definition is the engine's, shared with 2015's game and client game;
the client game's host registers each one here by name and definition
(RE_RegisterMOHAATiki). Its surfaces are drawn with the shaders the
definition binds, the skin an entity's per-surface byte selects, and a pose
the engine computes once a frame for the entity and skins each surface by
in the back end (code/renderercommon/tr_mohaa_render.h), at the level of
detail the engine reads off the model's size in the main view.

The choice of shader follows OpenMoHAA's R_AddSkelSurfaces
(renderergl1/tr_model.cpp, reference only): a custom shader replaces every
surface's unless the entity asks for it as an extra pass; otherwise the
entity's skin number plus the byte's low two bits pick the skin, a skin
past the surface's last is its first, and a surface flagged for crossfade
with more than one skin draws the pair its second bit names. A surface the
definition marks `nodraw`, or whose byte has bit 2 set, is hidden.
===========================================================================
*/

#include "tr_local.h"
#include "../renderercommon/tr_mohaa_render.h"

/* 2015's render flag for a custom shader drawn over the skins. */
#define MOHAA_RF_CUSTOMSHADERPASS	( 1 << 16 )
/* ... and for an entity seen only through the sky portal. */
#define MOHAA_RF_SKYENTITY			( 1 << 14 )

/* Poses computed this frame. A view drawing an entity again uses the one
 * its first view computed; a new frame starts the pool over. */
#define MOHAA_POSE_POOL_FLOATS	( 1 << 20 )

static float	mohaaPosePool[MOHAA_POSE_POOL_FLOATS];
static int		mohaaPosePoolUsed;
static int		mohaaPosePoolFrame = -1;

/*
==================
RE_RegisterMOHAATiki
==================
*/
qhandle_t RE_RegisterMOHAATiki( const char *name, const void *tiki ) {
	const mohaaRenderImport_t *engine = ri.MOHAARender;
	model_t *mod;
	int i, count;

	if ( !name || !name[0] || !tiki || !engine ) {
		return 0;
	}
	for ( i = 1; i < tr.numModels; i++ ) {
		mod = tr.models[i];
		if ( !Q_stricmp( mod->name, name ) ) {
			return mod->type == MOD_MOHAA_TIKI && mod->mohaaTiki == tiki ? mod->index : 0;
		}
	}
	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterMOHAATiki: model name %s too long\n", name );
		return 0;
	}

	mod = R_AllocModel();
	if ( !mod ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterMOHAATiki: R_AllocModel() failed for '%s'\n", name );
		return 0;
	}
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	count = engine->NumSurfaces( tiki );
	mod->type = MOD_MOHAA_TIKI;
	mod->mohaaTiki = tiki;
	mod->numMohaaSurfaces = count;
	mod->mohaaSurfaces = count > 0 ? ri.Hunk_Alloc( sizeof( mohaaSurface_t ) * count, h_low ) : NULL;
	mod->dataSize += sizeof( mohaaSurface_t ) * count;

	for ( i = 0; i < count; i++ ) {
		mohaaSurface_t *surface = &mod->mohaaSurfaces[i];
		mohaaTikiSurface_t info;
		int skin;

		surface->surfaceType = SF_MOHAA_TIKI;
		surface->model = mod;
		surface->index = i;
		if ( !engine->Surface( tiki, i, &info ) ) {
			continue;
		}
		surface->flags = info.flags;
		surface->numVerts = info.numVerts;
		surface->numIndexes = info.numIndexes;
		surface->numSkins = info.numSkins;
		for ( skin = 0; skin < surface->numSkins; skin++ ) {
			/* 2015's model loader asks for its shaders with the surface's own
			 * mip and picmip choices, wrapping on, as Ritual's does. */
			shader_t *shader = info.shaders[skin] && info.shaders[skin][0]
				? R_FindAliceSurfaceShader( info.shaders[skin],
					( info.flags & MOHAA_TIKI_SURF_NOMIPMAPS ) ? qfalse : qtrue,
					( info.flags & MOHAA_TIKI_SURF_NOPICMIP ) ? qfalse : qtrue )
				: NULL;
			surface->shaders[skin] = shader ? shader : tr.defaultShader;
			if ( ( surface->shaders[skin] == tr.defaultShader || surface->shaders[skin]->defaultShader )
				&& info.shaders[skin] && info.shaders[skin][0] ) {
				ri.Printf( PRINT_DEVELOPER, "RE_RegisterMOHAATiki: %s surface %d skin %d has no shader '%s'\n",
					name, i, skin, info.shaders[skin] ? info.shaders[skin] : "" );
			}
		}
	}
	return mod->index;
}

/*
==================
R_MOHAAEntityInView

A sky entity is drawn only in the sky portal's view, and no other entity
there (OpenMoHAA's R_AddEntitySurfaces and R_AddSpriteSurfaces). The flags
are the client game's own, which the engine keeps beside its own mapping.
==================
*/
qboolean R_MOHAAEntityInView( const trRefEntity_t *ent ) {
	const qboolean skyPass = ( tr.viewParms.passFlags & VPF_ALICE_SKY_PORTAL ) ? qtrue : qfalse;
	const qboolean skyEntity = ( ent->e.mohaa.renderfx & MOHAA_RF_SKYENTITY ) ? qtrue : qfalse;
	return skyPass == skyEntity ? qtrue : qfalse;
}

void R_MOHAAModelBounds( const model_t *mod, vec3_t mins, vec3_t maxs ) {
	if ( mod->type == MOD_MOHAA_TIKI && ri.MOHAARender ) {
		ri.MOHAARender->Bounds( mod->mohaaTiki, 1.0f, mins, maxs );
		return;
	}
	VectorClear( mins );
	VectorClear( maxs );
}

/* The pose, computed on the entity's first view this frame. */
static const float *R_MOHAAPose( trRefEntity_t *ent, const model_t *mod ) {
	const mohaaRenderImport_t *engine = ri.MOHAARender;
	int floats;
	float *pose;

	if ( ent->e.mohaa.bones ) {
		return ent->e.mohaa.bones;
	}
	if ( mohaaPosePoolFrame != tr.frameCount ) {
		mohaaPosePoolFrame = tr.frameCount;
		mohaaPosePoolUsed = 0;
	}
	floats = engine->PoseFloats( mod->mohaaTiki );
	if ( floats <= 0 || floats > MOHAA_POSE_POOL_FLOATS - mohaaPosePoolUsed ) {
		ri.Printf( PRINT_DEVELOPER, "R_AddMOHAATikiSurfaces: too many posed models this frame for '%s'\n", mod->name );
		return NULL;
	}
	pose = mohaaPosePool + mohaaPosePoolUsed;
	ent->e.mohaa.pose.tiki = mod->mohaaTiki;
	if ( !engine->Pose( &ent->e.mohaa.pose, pose ) ) {
		return NULL;
	}
	mohaaPosePoolUsed += floats;
	ent->e.mohaa.bones = pose;
	return pose;
}

static int R_MOHAAFogNum( const trRefEntity_t *ent, float radius ) {
	int i, j;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}
	for ( i = 1; i < tr.world->numfogs; i++ ) {
		const fog_t *fog = &tr.world->fogs[i];
		for ( j = 0; j < 3; j++ ) {
			if ( ent->e.origin[j] - radius >= fog->bounds[1][j] ) break;
			if ( ent->e.origin[j] + radius <= fog->bounds[0][j] ) break;
		}
		if ( j == 3 ) {
			return i;
		}
	}
	return 0;
}

static shader_t *R_MOHAASkinShader( const mohaaSurface_t *surface, int skin ) {
	if ( surface->numSkins <= 0 ) {
		return tr.defaultShader;
	}
	if ( skin < 0 || skin >= surface->numSkins ) {
		skin = 0;
	}
	return surface->shaders[skin];
}

/*
==================
R_AddMOHAATikiSurfaces

With a caster context the surfaces go to that dynamic light's shadow alone,
the renderer's own, whether or not the view sees the entity: the bounds are
tested against the light, and an entity the view did not pose is posed. Its
detail is the one the main view measured, whole for one it has not measured
(the client game's entity starts at -1), so every shadow face sees one shape.
==================
*/
void R_AddMOHAATikiSurfaces( trRefEntity_t *ent, dlightShadowCasterContext_t *casterContext ) {
	const model_t *mod = tr.currentModel;
	const float scale = ent->e.mohaa.scale > 0.0f ? ent->e.mohaa.scale : 1.0f;
	const qboolean customPass = ( ent->e.mohaa.renderfx & MOHAA_RF_CUSTOMSHADERPASS ) ? qtrue : qfalse;
	vec3_t bounds[2];
	qboolean personalModel;
	qboolean personalShadowCaster;
	int fogNum, i;
#ifdef USE_PMLIGHT
	dlight_t *dlights[ ARRAY_LEN( backEndData->dlights ) ];
	int numDlights = 0;
	int n;
#endif

	if ( !ri.MOHAARender || !mod->mohaaTiki ) {
		return;
	}
	personalModel = ( ent->e.renderfx & RF_THIRD_PERSON ) && !R_ViewPassIsPortal( &tr.viewParms );
	personalShadowCaster = personalModel && !( ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) );

	ri.MOHAARender->Bounds( mod->mohaaTiki, scale, bounds[0], bounds[1] );
	/* A first-person model is never culled. The player's body stands behind
	 * the eye and only its pose brings the arms in front, so its unposed
	 * bounds fall outside a field wider than retail's 4:3 one
	 * (the FOV correction) and the arms vanished. OpenMoHAA culls by the posed
	 * skeleton instead (R_AddSkelSurfaces, reference only). */
#ifdef USE_PMLIGHT
	if ( casterContext ) {
		if ( R_DlightShadowEntityBoundsCulled( casterContext, ent, bounds[0], bounds[1] ) ) {
			return;
		}
	} else
#endif
	if ( !( ent->e.renderfx & RF_DEPTHHACK ) && R_CullLocalBox( bounds ) == CULL_OUT ) {
		return;
	}
	if ( !R_MOHAAPose( ent, mod ) ) {
		return;
	}

	/* its level of detail, measured from the main view, whose measure a
	 * portal's view draws it by too (OpenMoHAA R_AddSkelSurfaces, reference
	 * only); a sky entity is only ever seen through the sky portal. The field
	 * is the one the game gave: widened for a wider screen by the correction,
	 * a model fills the same share of the screen's height as on a 4:3 one,
	 * and keeps the detail it had there. */
	if ( !casterContext ) {
		if ( !R_ViewPassIsPortal( &tr.viewParms ) || ( ent->e.mohaa.renderfx & MOHAA_RF_SKYENTITY ) ) {
			ent->e.mohaa.lod = ri.MOHAARender->LodMetric( &ent->e.mohaa.pose, scale, ent->e.origin, ent->e.axis[0],
				tr.viewParms.or.origin, tr.refdef.fovXAuthored > 0.0f ? tr.refdef.fovXAuthored : tr.viewParms.fovX );
		}
	}

	if ( !casterContext && ( !personalModel || r_shadows->integer > 1 ) ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

#ifdef USE_PMLIGHT
	if ( !casterContext && !personalModel && R_GetDlightMode() >= 2 ) {
		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );
		for ( n = 0; n < tr.viewParms.num_dlights; n++ ) {
			dlight_t *dl = &tr.viewParms.dlights[ n ];
			if ( !R_LightCullBounds( dl, bounds[0], bounds[1] ) ) dlights[ numDlights++ ] = dl;
		}
	}
#endif

	fogNum = casterContext ? 0 : R_MOHAAFogNum( ent, ri.MOHAARender->Radius( mod->mohaaTiki ) * scale );

	for ( i = 0; i < mod->numMohaaSurfaces; i++ ) {
		mohaaSurface_t *surface = &mod->mohaaSurfaces[i];
		const int state = i < (int)ARRAY_LEN( ent->e.mohaa.surfaces ) ? ent->e.mohaa.surfaces[i] : 0;
		shader_t *shader, *second = NULL;

		if ( ( surface->flags & MOHAA_TIKI_SURF_NODRAW ) || ( state & 4 ) ) {
			continue;
		}
		if ( !surface->numVerts || !surface->numIndexes ) {
			continue;
		}
		if ( ent->e.customShader && !customPass ) {
			shader = R_GetShaderByHandle( ent->e.customShader );
		} else if ( ( state & 0x40 ) && surface->numSkins > 1 ) {
			shader = R_MOHAASkinShader( surface, ( state & 2 ) + ent->e.skinNum );
			second = R_MOHAASkinShader( surface, ( state & 2 ) + ent->e.skinNum + 1 );
		} else {
			shader = R_MOHAASkinShader( surface, ( state & 3 ) + ent->e.skinNum );
		}

#ifdef USE_PMLIGHT
		/* A crossfade's second skin and a custom shader's extra pass draw the
		 * same shape again, so the surface casts once. */
		if ( casterContext ) {
			if ( !R_AddDlightShadowEntityCasterSurface( casterContext, &surface->surfaceType, shader ) ) {
				return;
			}
			continue;
		}
#endif

		/* The renderer's own sun shadow takes a personal model's too. */
		if ( personalShadowCaster && shader->sort == SS_OPAQUE ) {
			R_AddDrawSurfFlags( &surface->surfaceType, shader, fogNum, 0, DSF_SHADOW_CASTER_ONLY );
		}
		if ( !personalModel ) {
			R_AddDrawSurf( &surface->surfaceType, shader, fogNum, 0 );
			tr.needScreenMap |= shader->hasScreenMap;
			if ( second ) {
				R_AddDrawSurf( &surface->surfaceType, second, fogNum, 0 );
				tr.needScreenMap |= second->hasScreenMap;
			}
			if ( ent->e.customShader && customPass ) {
				shader_t *pass = R_GetShaderByHandle( ent->e.customShader );
				R_AddDrawSurf( &surface->surfaceType, pass, fogNum, 0 );
				tr.needScreenMap |= pass->hasScreenMap;
			}
		}
#ifdef USE_PMLIGHT
		if ( !personalModel && numDlights && shader->lightingStage >= 0 ) {
			for ( n = 0; n < numDlights; n++ ) {
				tr.light = dlights[ n ];
				R_AddLitSurf( &surface->surfaceType, shader, fogNum );
			}
		}
#endif
	}
}

/*
==================
RB_SurfaceMOHAATiki
==================
*/
void RB_SurfaceMOHAATiki( void *data ) {
	const mohaaSurface_t *surface = (const mohaaSurface_t *)data;
	const trRefEntity_t *ent = backEnd.currentEntity;
	const float scale = ent->e.mohaa.scale > 0.0f ? ent->e.mohaa.scale : 1.0f;
	int verts, indexes = 0;

	if ( !ri.MOHAARender || !ent->e.mohaa.bones ) {
		return;
	}
#ifdef USE_VBO
	VBO_Flush();
#endif
	RB_CHECKOVERFLOW( surface->numVerts, surface->numIndexes );
#ifdef USE_VBO
	tess.surfType = SF_MOHAA_TIKI;
#endif
	verts = ri.MOHAARender->Skin( surface->model->mohaaTiki, surface->index, ent->e.mohaa.bones, scale,
		ent->e.mohaa.lod, ( ent->e.renderfx & RF_DEPTHHACK ) ? MOHAA_LOD_VIEWMODEL : 0, tess.xyz + tess.numVertexes, tess.normal + tess.numVertexes, tess.texCoords[0] + tess.numVertexes,
		tess.indexes + tess.numIndexes, (unsigned int)tess.numVertexes, SHADER_MAX_VERTEXES - tess.numVertexes,
		SHADER_MAX_INDEXES - tess.numIndexes, &indexes );
	if ( verts <= 0 ) {
		return;
	}
	tess.numVertexes += verts;
	tess.numIndexes += indexes;
}
