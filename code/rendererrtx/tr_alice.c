/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

American McGee's Alice's models in the RTX renderer

Two kinds of model reach this renderer from Alice's client game besides the
world's own brush models: TIKI definitions -- skeletal or vertex-animated
meshes, loaded once by the engine and shared with both game modules -- and
sprites, a shader drawn as a quad facing the view.

A TIKI model's surfaces are drawn with the shaders its definition binds, the
skin an entity's per-surface byte selects, and a pose computed once for the
entity each frame (tr_alice_tiki.cpp). A sprite takes its size from its
shader's first image and its orientation from the shader's `spritegen`
keyword, as retail alice.exe's R_RegisterModel (0x004895f0) and sprite
surface (0x004af480) do.

This is the Vulkan renderer's code/renderervk/tr_alice.c, itself the OpenGL
renderer's with the Vulkan family's tessellation rules: a surface that writes
into tess flushes any queued world VBO items first and names its surface
type, and an entity surface whose shader samples the screen asks the scene
for a screen map. These models, sprites and swipes are drawn by the raster
pass only; the ray-traced scene's dynamic geometry (vk.c) does not carry them.

See docs/fntech3/2026-09-23-alice-cgame.md.
===========================================================================
*/

#include "tr_local.h"
#include "../renderercommon/tr_alice_tiki.h"
#include "../compat/alice_render_abi.h"

/*
==================
R_LoadAliceTiki

A `.tik` name registers the engine's definition and builds one draw surface
for each of the model's surfaces, with every skin's shader resolved now.
==================
*/
qboolean R_LoadAliceTiki( model_t *mod, const char *name ) {
	const void *model;
	int handle, i, count;

	if ( !ri.AliceTikiRegister || !ri.AliceTikiGet ) {
		return qfalse;
	}
	handle = ri.AliceTikiRegister( name );
	model = handle >= 0 ? ri.AliceTikiGet( handle ) : NULL;
	if ( !model ) {
		/* Retail's renderer says nothing (0x0048982e); the engine's loader
		 * has already said why, to a developer. */
		ri.Printf( PRINT_DEVELOPER, "R_LoadAliceTiki: %s did not load\n", name );
		return qfalse;
	}

	count = RA_TikiNumSurfaces( model );
	mod->type = MOD_ALICE_TIKI;
	mod->aliceTiki = handle;
	mod->aliceModel = model;
	mod->numAliceSurfaces = count;
	mod->aliceSurfaces = count > 0 ? ri.Hunk_Alloc( sizeof( aliceSurface_t ) * count, h_low ) : NULL;
	mod->dataSize += sizeof( aliceSurface_t ) * count;

	for ( i = 0; i < count; i++ ) {
		aliceSurface_t *surface = &mod->aliceSurfaces[i];
		const int flags = RA_TikiSurfaceFlags( model, i );
		int skin;

		surface->surfaceType = SF_ALICE_TIKI;
		surface->model = mod;
		surface->index = i;
		surface->flags = flags;
		RA_TikiSurfaceSize( model, i, &surface->numVerts, &surface->numIndexes );
		surface->numSkins = RA_TikiSurfaceNumSkins( model, i );
		for ( skin = 0; skin < surface->numSkins && skin < 4; skin++ ) {
			const char *shaderName = RA_TikiSurfaceShader( model, i, skin );
			shader_t *shader = shaderName ? R_FindAliceSurfaceShader( shaderName,
				( flags & ALICE_SURFACE_NOMIPMAPS ) ? qfalse : qtrue,
				( flags & ALICE_SURFACE_NOPICMIP ) ? qfalse : qtrue ) : NULL;
			surface->shaders[skin] = shader ? shader : tr.defaultShader;
		}
	}
	return qtrue;
}

/*
==================
R_LoadAliceSprite

A `.spr` name is a shader named by the same name without its extension. The
sprite is as large as the shader's first image, centred on its origin, and
scaled by the shader's `spritescale`.
==================
*/
qboolean R_LoadAliceSprite( model_t *mod, const char *name ) {
	char shaderName[MAX_QPATH];
	shader_t *shader;
	const image_t *image = NULL;
	aliceSprite_t *sprite;
	int stage;

	COM_StripExtension( name, shaderName, sizeof( shaderName ) );
	/* Retail asks for the shader without mipmaps, picmip or wrapping
	 * (0x004afb4d..0x004afb57), which only an image with no script notices.
	 * With neither script nor image, its R_FindShader hands back the default
	 * shader itself (FinishShader 0x0048e3ed..0x0048e410), so the sprite is
	 * the default image's size and the model still registers. */
	shader = R_FindShader( shaderName, LIGHTMAP_NONE, qfalse );
	if ( !shader ) {
		return qfalse;
	}
	if ( shader->defaultShader ) {
		shader = tr.defaultShader;
	}
	for ( stage = 0; stage < MAX_SHADER_STAGES && !image; stage++ ) {
		if ( shader->stages[stage] && shader->stages[stage]->bundle[0].image[0] ) {
			image = shader->stages[stage]->bundle[0].image[0];
		}
	}
	if ( !image ) {
		ri.Printf( PRINT_WARNING, "Could not find image for sprite in shader %s\n", shaderName );
		return qfalse;
	}

	sprite = ri.Hunk_Alloc( sizeof( *sprite ), h_low );
	sprite->width = (float)image->width;
	sprite->height = (float)image->height;
	sprite->originX = sprite->width * 0.5f;
	sprite->originY = sprite->height * 0.5f;
	sprite->shader = shader;
	mod->type = MOD_ALICE_SPRITE;
	mod->aliceSprite = sprite;
	mod->dataSize += sizeof( *sprite );
	return qtrue;
}

void R_AliceModelBounds( const model_t *mod, vec3_t mins, vec3_t maxs ) {
	if ( mod->type == MOD_ALICE_TIKI ) {
		RA_TikiModelBounds( mod->aliceModel, mins, maxs );
		return;
	}
	if ( mod->type == MOD_ALICE_SPRITE ) {
		const shader_t *shader = mod->aliceSprite->shader;
		const float scale = shader && shader->aliceSpriteScale > 0.0f ? shader->aliceSpriteScale : 1.0f;
		const float r = MAX( mod->aliceSprite->originX, mod->aliceSprite->originY ) * scale;
		VectorSet( mins, -r, -r, -r );
		VectorSet( maxs, r, r, r );
		return;
	}
	VectorClear( mins );
	VectorClear( maxs );
}

/*
==================
RE_GetAliceModelRadius

Retail's R_ModelRadius (0x0048a980): a definition answers its own radius --
the one its setup names, else the farthest corner of its bounds times its
scale -- and a sprite half its larger side times its shader's scale.
==================
*/
float RE_GetAliceModelRadius( qhandle_t handle ) {
	const model_t *mod = R_GetModelByHandle( handle );
	if ( mod && mod->type == MOD_ALICE_TIKI ) {
		return RA_TikiModelRadius( mod->aliceModel );
	}
	if ( mod && mod->type == MOD_ALICE_SPRITE ) {
		const shader_t *shader = mod->aliceSprite->shader;
		const float scale = shader && shader->aliceSpriteScale > 0.0f ? shader->aliceSpriteScale : 1.0f;
		return MAX( mod->aliceSprite->width, mod->aliceSprite->height ) * scale * 0.5f;
	}
	return -1.0f;
}

int RE_GetAliceTikiHandle( qhandle_t handle ) {
	const model_t *mod = R_GetModelByHandle( handle );
	return mod && mod->type == MOD_ALICE_TIKI ? mod->aliceTiki : -1;
}

void RE_GetShaderImageSize( qhandle_t handle, int *width, int *height ) {
	const shader_t *shader = R_GetShaderByHandle( handle );
	int stage;

	*width = *height = 0;
	if ( !shader ) {
		return;
	}
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
		if ( shader->stages[stage] && shader->stages[stage]->bundle[0].image[0] ) {
			*width = shader->stages[stage]->bundle[0].image[0]->width;
			*height = shader->stages[stage]->bundle[0].image[0]->height;
			return;
		}
	}
}

/*
==================
R_AliceTikiFogNum
==================
*/
static int R_AliceTikiFogNum( const trRefEntity_t *ent, float radius ) {
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

/*
==================
R_AliceProjectRadius

Quake III's ProjectRadius, which retail keeps as it was (alice.exe
0x004a1770): how much of the view's height a sphere of radius r at a point
fills, 0 when the point is at or behind the view, never more than 1.
==================
*/
static float R_AliceProjectRadius( float r, const vec3_t location ) {
	const viewParms_t *view = &tr.viewParms;
	const float c = DotProduct( view->or.axis[0], view->or.origin );
	const float dist = DotProduct( view->or.axis[0], location ) - c;
	float y, w, projected;

	if ( dist <= 0.0f ) {
		return 0.0f;
	}
	y = fabsf( r ) * view->projectionMatrix[5] - dist * view->projectionMatrix[9] + view->projectionMatrix[13];
	w = fabsf( r ) * view->projectionMatrix[7] - dist * view->projectionMatrix[11] + view->projectionMatrix[15];
	projected = y / w;
	return projected > 1.0f ? 1.0f : projected;
}

/* Retail keeps two levels of detail an entity: one for the main view, one for
 * a portal view (entity +0x200, +0x204). Its sky portal's view is not a portal
 * view (alice.exe 0x004922d9 sets only its own flag), so it uses the first. */
static int R_AliceLodSlot( const viewParms_t *view ) {
	return R_ViewPassIsPortal( view ) && !( view->passFlags & VPF_ALICE_SKY_PORTAL ) ? 1 : 0;
}

/*
==================
R_AliceTikiLod

The level of detail an entity is drawn at in this view, chosen once it has
survived culling (alice.exe 0x004a67b1..0x004a6853 for a vertex-animated
model, 0x004a3cac..0x004a3dca for a skeletal one): its current frame's
radius, projected at its origin plus its model's scaled origin, through
r_lodscale and r_lodbias and the definition's lod_scale and lod_bias.
==================
*/
static void R_AliceTikiLod( trRefEntity_t *ent, const model_t *mod ) {
	vec3_t center;
	float radius, projected = 0.0f;
	const int slot = R_AliceLodSlot( &tr.viewParms );

	if ( RA_TikiLodSphere( mod->aliceModel, &ent->e.alice, center, &radius ) ) {
		VectorAdd( ent->e.origin, center, center );
		projected = R_AliceProjectRadius( radius, center );
	}
	ent->aliceLod[slot] = RA_TikiLodFraction( mod->aliceModel, projected, r_lodscale->value, r_lodbias->value );
}

/*
==================
R_AliceTikiLightOrigin

Where retail samples a TIKI entity's light (alice.exe 0x004aaac0): the middle
of its current frame, moved by its model's origin and `lightoffset`, scaled,
turned by its axis, and added to its lighting origin, or to its origin.
==================
*/
static qboolean R_AliceTikiLightOrigin( const trRefEntity_t *ent, const model_t *mod, vec3_t out ) {
	vec3_t local;
	int k;

	if ( !RA_TikiLightingPoint( mod->aliceModel, &ent->e.alice, local ) ) {
		return qfalse;
	}
	VectorCopy( ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) ? ent->e.lightingOrigin : ent->e.origin, out );
	for ( k = 0; k < 3; k++ ) {
		VectorMA( out, local[k], ent->e.axis[k], out );
	}
	return qtrue;
}

/* A surface's skin, or its first when the number is past its last. */
static shader_t *R_AliceSkinShader( const aliceSurface_t *surface, int skin ) {
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
R_AddAliceTikiSurfaces

Cull the entity against its model's bounds, pose it once, and add each
surface it shows, as retail's R_AddTikiSurfaces (0x004a6320, the choice at
0x004a695c..0x004a6b6f) does: a surface the model flags `nodraw`, or whose
per-entity byte has bit 2 set, is hidden. A custom shader replaces every
surface's shader, unless the entity asks for it as an extra pass, when it is
drawn over the surface as well; failing that a custom skin names the shader;
failing that the byte's low two bits, plus the entity's skin number, choose
the skin, and a byte with bit 6 set on a surface of several skins draws two
of them, the pair (byte & 2) + skin number and the one after, which is how a
surface crossfades between skins. A skin past the surface's last is its first.
==================
*/
void R_AddAliceTikiSurfaces( trRefEntity_t *ent ) {
	const model_t *mod = tr.currentModel;
	const float entityScale = ent->e.alice.scale > 0.0f ? ent->e.alice.scale : 1.0f;
	vec3_t bounds[2];
	qboolean personalModel;
	int fogNum, i;
#ifdef USE_PMLIGHT
	dlight_t *dlights[ ARRAY_LEN( backEndData->dlights ) ];
	int numDlights = 0;
	int n;
#endif

	/* Retail corrects the animation and frame numbers before anything reads
	 * them, culling included (alice.exe 0x004a63a0..0x004a64e3, 0x004a1f60). */
	RA_TikiCheckAnimation( mod->aliceModel, &ent->e.alice, mod->name, ri.Printf );

	personalModel = ( ent->e.renderfx & RF_THIRD_PERSON ) && !R_ViewPassIsPortal( &tr.viewParms );

	RA_TikiModelBounds( mod->aliceModel, bounds[0], bounds[1] );
	VectorScale( bounds[0], entityScale, bounds[0] );
	VectorScale( bounds[1], entityScale, bounds[1] );
	if ( R_CullLocalBox( bounds ) == CULL_OUT ) {
		return;
	}

	ent->e.alice.bones = NULL;
	if ( RA_TikiIsSkeletal( mod->aliceModel ) ) {
		ent->e.alice.bones = RA_TikiPose( mod->aliceModel, &ent->e.alice );
		if ( !ent->e.alice.bones ) {
			return;
		}
	}

	R_AliceTikiLod( ent, mod );

	if ( !personalModel || r_shadows->integer > 1 ) {
		vec3_t lightOrigin;
		if ( !R_AliceTikiLightOrigin( ent, mod, lightOrigin ) ) {
			VectorCopy( ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) ? ent->e.lightingOrigin : ent->e.origin, lightOrigin );
		}
		R_SetupAliceEntityLighting( &tr.refdef, ent, lightOrigin );
	}

#ifdef USE_PMLIGHT
	if ( !personalModel && R_GetDlightMode() >= 2 ) {
		R_TransformDlights( tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.or );
		for ( n = 0; n < tr.viewParms.num_dlights; n++ ) {
			dlight_t *dl = &tr.viewParms.dlights[ n ];
			if ( !R_LightCullBounds( dl, bounds[0], bounds[1] ) ) dlights[ numDlights++ ] = dl;
		}
	}
#endif

	fogNum = R_AliceTikiFogNum( ent, RA_TikiModelRadius( mod->aliceModel ) * entityScale );

	for ( i = 0; i < mod->numAliceSurfaces; i++ ) {
		aliceSurface_t *surface = &mod->aliceSurfaces[i];
		const int state = i < (int)ARRAY_LEN( ent->e.alice.surfaces ) ? ent->e.alice.surfaces[i] : 0;
		shader_t *shader, *second = NULL;

		if ( ( surface->flags & ALICE_SURFACE_NODRAW ) || ( state & 4 ) ) {
			continue;
		}
		if ( !surface->numVerts || !surface->numIndexes ) {
			continue;
		}
		if ( ent->e.customShader && !ent->e.alice.customShaderPass ) {
			shader = R_GetShaderByHandle( ent->e.customShader );
		} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
			const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
			shader = skin->numSurfaces > 0 ? skin->surfaces[i % skin->numSurfaces].shader : tr.defaultShader;
		} else if ( ( state & 0x40 ) && surface->numSkins > 1 ) {
			shader = R_AliceSkinShader( surface, ( state & 2 ) + ent->e.skinNum );
			second = R_AliceSkinShader( surface, ( state & 2 ) + ent->e.skinNum + 1 );
		} else {
			shader = R_AliceSkinShader( surface, ( state & 3 ) + ent->e.skinNum );
		}

		/* Shadows go ahead of the surface, for the shader it is drawn with
		 * (alice.exe 0x004a6a04..0x004a6aa3): with cg_shadows 2 a stencil
		 * volume for an opaque surface of an entity flagged both shadow
		 * and precise shadow, never a personal or depth-hacked one; with
		 * cg_shadows 3 a projected shadow for one flagged shadow plane and
		 * precise shadow, which the client game flags only once it has
		 * found the ground. Neither is drawn in fog. Retail's sort 4 is its
		 * opaque (0x0048acbc), this renderer's SS_OPAQUE. */
		if ( !personalModel && r_shadows->integer == 2 && fogNum == 0
			&& !( ent->e.alice.renderfx & FNTECH3_ALICE_RF_DEPTHHACK )
			&& ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_SHADOW )
			&& ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_SHADOW_PRECISE )
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( &surface->surfaceType, tr.shadowShader, 0, 0 );
		}
		if ( r_shadows->integer == 3 && fogNum == 0
			&& ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_SHADOW_PLANE )
			&& ( ent->e.alice.renderfx & FNTECH3_ALICE_RF_SHADOW_PRECISE )
			&& shader->sort == SS_OPAQUE && tr.projectionShadowShader ) {
			R_AddDrawSurf( &surface->surfaceType, tr.projectionShadowShader, 0, 0 );
		}

		if ( !personalModel ) {
			R_AddDrawSurf( &surface->surfaceType, shader, fogNum, 0 );
			tr.needScreenMap |= shader->hasScreenMap;
			if ( second ) {
				R_AddDrawSurf( &surface->surfaceType, second, fogNum, 0 );
				tr.needScreenMap |= second->hasScreenMap;
			}
			if ( ent->e.customShader && ent->e.alice.customShaderPass ) {
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
RB_SurfaceAliceTiki
==================
*/
void RB_SurfaceAliceTiki( void *data ) {
	const aliceSurface_t *surface = (const aliceSurface_t *)data;
	const trRefEntity_t *ent = backEnd.currentEntity;
	int verts, indexes;

	/* A dynamic light's shadow faces draw the whole surface, so every face
	 * sees one silhouette; a view draws the detail it chose. */
	const float lod = ( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW )
		? 1.0f : ent->aliceLod[R_AliceLodSlot( &backEnd.viewParms )];

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( surface->numVerts, surface->numIndexes );

#ifdef USE_VBO
	tess.surfType = SF_ALICE_TIKI;
#endif

	if ( !RA_TikiTessellateLod( surface->model->aliceModel, surface->index, &ent->e.alice,
			ent->e.alice.bones, lod, tess.xyz + tess.numVertexes, tess.normal + tess.numVertexes,
			tess.texCoords[0] + tess.numVertexes, tess.indexes + tess.numIndexes,
			(unsigned int)tess.numVertexes, SHADER_MAX_VERTEXES - tess.numVertexes,
			SHADER_MAX_INDEXES - tess.numIndexes, &verts, &indexes ) ) {
		return;
	}
	tess.numVertexes += verts;
	tess.numIndexes += indexes;
}

/*
==================
RB_SurfaceAliceSprite

A sprite is a quad around the entity's origin, its half-extents half the
image's size, scaled by the shader's `spritescale` and the entity's own scale,
and turned by its shader's `spritegen`: toward the view, toward the view but
rolled with the entity, standing upright, or along the entity's own axis.
==================
*/
void RB_SurfaceAliceSprite( void ) {
	const trRefEntity_t *ent = backEnd.currentEntity;
	const model_t *mod = R_GetModelByHandle( ent->e.hModel );
	const aliceSprite_t *sprite;
	vec3_t left, up;
	float scale;

	if ( !mod || mod->type != MOD_ALICE_SPRITE ) {
		return;
	}
	sprite = mod->aliceSprite;
	/* A shader that never names a `spritegen` or `spritescale` draws at its
	 * image's own size, which is the scale retail's parser starts from. */
	scale = ( sprite->shader->aliceSpriteScale > 0.0f ? sprite->shader->aliceSpriteScale : 1.0f )
		* ( ent->e.alice.scale > 0.0f ? ent->e.alice.scale : 1.0f );

	switch ( sprite->shader->aliceSpriteGen ) {
	case ALICE_SPRITEGEN_ORIENTED:
		VectorNegate( ent->e.axis[1], left );
		VectorCopy( ent->e.axis[2], up );
		break;
	case ALICE_SPRITEGEN_PARALLEL_UPRIGHT: {
		const float *forward = backEnd.viewParms.or.axis[0];
		VectorSet( up, 0.0f, 0.0f, 1.0f );
		VectorSet( left, -forward[1], forward[0], 0.0f );
		if ( VectorNormalize( left ) == 0.0f ) {
			VectorCopy( backEnd.viewParms.or.axis[1], left );
		}
		break;
	}
	case ALICE_SPRITEGEN_PARALLEL_ORIENTED: {
		const float angle = atan2f( ent->e.axis[1][2], ent->e.axis[1][1] );
		const float s = sinf( angle ), c = cosf( angle );
		int k;
		for ( k = 0; k < 3; k++ ) {
			left[k] = c * backEnd.viewParms.or.axis[1][k] + s * backEnd.viewParms.or.axis[2][k];
			up[k] = c * backEnd.viewParms.or.axis[2][k] - s * backEnd.viewParms.or.axis[1][k];
		}
		break;
	}
	default:
		VectorCopy( backEnd.viewParms.or.axis[1], left );
		VectorCopy( backEnd.viewParms.or.axis[2], up );
		break;
	}
	if ( R_ViewPassIsMirror( &backEnd.viewParms ) ) {
		VectorNegate( left, left );
	}
	VectorScale( left, sprite->originX * scale, left );
	VectorScale( up, sprite->originY * scale, up );
	RB_AddQuadStamp( ent->e.origin, left, up, ent->e.shader );
}

/*
===============================================================================

SWIPES

The trail a swung weapon leaves. Retail keeps 32 swipes of up to 1,024 point
pairs each (0x007a7218, 0x7018 bytes apart) and starts over at the first
swipe on the first begin of a new frame (RE_SwipeBegin 0x00497e40). A point
joins the latest swipe (0x00497ee0); the end call does nothing (0x00497f60).
Each view adds every swipe with points as a world surface (0x00497f70),
first moving its clock on by the time the scene's clock moved since the last
view that drew it, and the back end draws each point pair as a rung of a
strip that fades out over the swipe's lifetime (0x00498000).

===============================================================================
*/

#define ALICE_MAX_SWIPES		32
#define ALICE_MAX_SWIPE_POINTS	1024

typedef struct {
	vec3_t	p1, p2;
	float	time;
} aliceSwipePoint_t;

typedef struct {
	surfaceType_t		surfaceType;
	float				life;
	float				thistime;
	int					lastRefdefTime;
	qhandle_t			shader;
	int					numPoints;
	aliceSwipePoint_t	points[ALICE_MAX_SWIPE_POINTS];
} aliceSwipe_t;

static aliceSwipe_t	aliceSwipes[ALICE_MAX_SWIPES];
static int			aliceNumSwipes;
static int			aliceSwipeFrame = -1;

void RE_AliceSwipeBegin( float thistime, float life, qhandle_t shader ) {
	aliceSwipe_t *swipe;

	if ( tr.frameCount != aliceSwipeFrame ) {
		aliceNumSwipes = 0;
	} else if ( aliceNumSwipes >= ALICE_MAX_SWIPES ) {
		/* Retail keeps going: the points that follow join the last swipe. */
		return;
	}
	swipe = &aliceSwipes[aliceNumSwipes++];
	swipe->surfaceType = SF_ALICE_SWIPE;
	swipe->life = life;
	swipe->thistime = thistime;
	swipe->lastRefdefTime = 0;
	swipe->shader = shader;
	swipe->numPoints = 0;
	if ( shader < 0 && thistime != 0.0f ) {
		ri.Printf( PRINT_WARNING, "RE_SwipeBegin: Invalid shader handle\n" );
	}
	aliceSwipeFrame = tr.frameCount;
}

void RE_AliceSwipePoint( const vec3_t p1, const vec3_t p2, float time ) {
	aliceSwipe_t *swipe;

	/* Retail writes before its array when no swipe has begun; a point with no
	 * swipe to join is dropped instead. */
	if ( aliceNumSwipes <= 0 ) {
		return;
	}
	swipe = &aliceSwipes[aliceNumSwipes - 1];
	if ( swipe->numPoints >= ALICE_MAX_SWIPE_POINTS ) {
		return;
	}
	VectorCopy( p1, swipe->points[swipe->numPoints].p1 );
	VectorCopy( p2, swipe->points[swipe->numPoints].p2 );
	swipe->points[swipe->numPoints].time = time;
	swipe->numPoints++;
}

void R_AddAliceSwipeSurfaces( void ) {
	int i;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;
	for ( i = 0; i < aliceNumSwipes; i++ ) {
		aliceSwipe_t *swipe = &aliceSwipes[i];
		shader_t *shader;
		if ( swipe->numPoints <= 0 ) {
			continue;
		}
		if ( swipe->lastRefdefTime ) {
			swipe->thistime += (float)( tr.refdef.time - swipe->lastRefdefTime );
		}
		swipe->lastRefdefTime = tr.refdef.time;
		shader = R_GetShaderByHandle( swipe->shader );
		R_AddDrawSurf( &swipe->surfaceType, shader, 0, 0 );
		tr.needScreenMap |= shader->hasScreenMap;
	}
}

void RB_SurfaceAliceSwipe( void *data ) {
	aliceSwipe_t *swipe = (aliceSwipe_t *)data;
	const int n = swipe->numPoints;
	const float inverse = swipe->life != 0.0f ? 1.0f / swipe->life : 0.0f;
	int first, i, count;

	if ( n <= 0 ) {
		return;
	}

	/* A swipe is a world surface, so it may share a batch with world
	 * geometry queued for the VBO. */
#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 2 * n, 6 * n );

#ifdef USE_VBO
	tess.surfType = SF_ALICE_SWIPE;
#endif

	first = tess.numVertexes;
	for ( i = 0; i < n; i++ ) {
		const aliceSwipePoint_t *point = &swipe->points[i];
		float alpha = swipe->life != 0.0f ? 1.0f - ( swipe->thistime - point->time ) * inverse : -1.0f;
		if ( alpha > 0.0f && swipe->thistime >= point->time ) {
			const float s = (float)i / (float)n;
			const int ndx = tess.numVertexes;
			byte rgb, a;
			if ( alpha > 1.0f ) {
				alpha = 1.0f;
			}
			rgb = (byte)( tr.identityLightByte * alpha );
			a = (byte)( alpha * 255.0f );
			VectorCopy( point->p1, tess.xyz[ndx] );
			tess.texCoords[0][ndx][0] = s;
			tess.texCoords[0][ndx][1] = 1.0f;
			VectorCopy( point->p2, tess.xyz[ndx + 1] );
			tess.texCoords[0][ndx + 1][0] = s;
			tess.texCoords[0][ndx + 1][1] = 0.0f;
			tess.vertexColors[ndx].rgba[0] = tess.vertexColors[ndx].rgba[1] = tess.vertexColors[ndx].rgba[2] = rgb;
			tess.vertexColors[ndx].rgba[3] = a;
			tess.vertexColors[ndx + 1] = tess.vertexColors[ndx];
			tess.numVertexes += 2;
		} else if ( i == n - 1 ) {
			/* The swipe dies with its newest point. */
			swipe->numPoints = 0;
		}
	}

	/* A strip of fewer than three vertices is discarded (0x00497450). */
	count = tess.numVertexes - first;
	if ( count < 3 ) {
		tess.numVertexes = first;
		return;
	}
	for ( i = 0; i + 2 < count; i++ ) {
		const int a = first + i;
		if ( i & 1 ) {
			tess.indexes[tess.numIndexes++] = a + 1;
			tess.indexes[tess.numIndexes++] = a;
			tess.indexes[tess.numIndexes++] = a + 2;
		} else {
			tess.indexes[tess.numIndexes++] = a;
			tess.indexes[tess.numIndexes++] = a + 1;
			tess.indexes[tess.numIndexes++] = a + 2;
		}
	}
}


/*
===============================================================================

VIEWS

Retail draws an entity in the views its flags ask for (R_AddEntitySurfaces
0x00478b83..0x00478bb7; its sprite list at 0x00478ff6 and its polygon list at
0x00491398 filter the same way): a sky entity only in the sky portal's view
and no other entity there; in a portal view, only an entity flagged for a
portal or for both; outside one, never an entity flagged for a portal alone.

===============================================================================
*/

qboolean R_AliceEntityInView( const trRefEntity_t *ent ) {
	const int fx = ent->e.alice.renderfx;
	const qboolean skyPass = ( tr.viewParms.passFlags & VPF_ALICE_SKY_PORTAL ) ? qtrue : qfalse;
	const qboolean portal = R_ViewPassIsPortal( &tr.viewParms ) && !skyPass;

	if ( skyPass != ( ( fx & FNTECH3_ALICE_RF_SKYENTITY ) ? qtrue : qfalse ) ) {
		return qfalse;
	}
	if ( portal ) {
		return ( fx & ( FNTECH3_ALICE_RF_PORTALENTITY | FNTECH3_ALICE_RF_DUALENTITY ) ) ? qtrue : qfalse;
	}
	return ( fx & FNTECH3_ALICE_RF_PORTALENTITY ) ? qfalse : qtrue;
}


/*
===============================================================================

LENS FLARES

Retail's lens flares are three `lens_flare` objects -- the sun, an entity and a
dynamic light -- each loaded from its own section of scripts/lensflare2.txt
when the renderer registers (0x004a88f0; the loader 0x004a7ff0), and drawn at
the end of every view but the sky portal's, after its surfaces, as quads in
the view's normalised device coordinates (0x004a8480, called at 0x00486888).

A flare is projected to the screen (0x004a7950) and, facing the viewer more
closely than the section's dot_min, traced from the eye to its source with the
client game's trace (the shared block's +0x1c, "lens_flare::CheckRay"). If
nothing is in the way, each `flare <size> <where> <shader> [alphascale]` of
the section is drawn on the line from the source through the middle of the
screen, `where` 0 at the source and 1 mirrored through the centre, `size` a
half-width in the same units, with an alpha of the facing fraction times its
alphascale (0x004a7a80). A dynamic light's flare then adds a corona of
r_lightcoronasize over the source (0x004a7d00), and the sun's section fades a
full-screen blend out over its fullfade milliseconds (0x004a7f00).

The sun is placed by the level (worldspawn `sunflare`, 0x004ac53a) or by
r_sunflare; r_place_sunflare puts r_sunflare where the view looks. An entity
flagged lens flare gets the dynamic light's flare, one flagged view lens
flare the entity's, both coloured by its shader colour; a dynamic light added
with type 1 or 2 gets the dynamic light's flare in its own colour.

===============================================================================
*/

#define ALICE_MAX_FLARE_ELEMENTS	20	// what fits before the object's next field

typedef struct {
	float		size;
	float		where;
	shader_t	*shader;
	float		alphaScale;
} aliceFlareElement_t;

typedef struct {
	float		dotMin;
	float		fullScale;
	int			numFlares;
	aliceFlareElement_t	flares[ALICE_MAX_FLARE_ELEMENTS];
	shader_t	*fullscreen;
	int			fullFade;
	qboolean	fullActive;
	int			fullTime;
	float		fullAlpha;
	vec3_t		farPoint;		// what the occlusion trace aims at
	vec3_t		position;		// what is projected
	vec4_t		color;
	float		screen[2];
	qboolean	inPortalSky;	// the source is seen through the sky portal
	qboolean	loaded;
	shader_t	*corona;
} aliceLensFlare_t;

static aliceLensFlare_t	aliceSunFlare, aliceEntityFlare, aliceDlightFlare;
static vec3_t			aliceSunOrigin;
static qboolean			aliceSunSet;

static cvar_t	*r_sunflare;
static cvar_t	*r_place_sunflare;
static cvar_t	*r_sunflare_inportalsky;
static cvar_t	*r_lightcoronasize;

/* Retail's names, defaults and flags (alice.exe 0x00475aa7..0x00475b00). */
void R_AliceRegisterCvars( void ) {
	r_sunflare = ri.Cvar_Get( "r_sunflare", "0", 0 );
	ri.Cvar_SetDescription( r_sunflare, "Places a lens-flare sun at \"x y z\"; 0 leaves the level's own." );
	r_place_sunflare = ri.Cvar_Get( "r_place_sunflare", "0", 0 );
	ri.Cvar_SetDescription( r_place_sunflare, "Sets r_sunflare every frame to the point the view looks at." );
	r_sunflare_inportalsky = ri.Cvar_Get( "r_sunflare_inportalsky", "0", 0 );
	ri.Cvar_SetDescription( r_sunflare_inportalsky, "Treats r_sunflare as a point seen through the sky portal." );
	r_lightcoronasize = ri.Cvar_Get( "r_lightcoronasize", ".1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_lightcoronasize, "Half-size, as a fraction of the view, of a flaring dynamic light's corona." );
}

/*
==================
R_AliceLoadLensFlare

One section of scripts/lensflare2.txt (alice.exe 0x004a7ff0): after `begin
<name>`, keywords up to `end`. Each flare resets its alpha scale to 1 before
its arguments are read; one missing an argument is warned about and not
kept.
==================
*/
static void R_AliceLoadLensFlare( aliceLensFlare_t *flare, const char *section ) {
	char *buffer = NULL;
	const char *p;
	const char *token;
	qboolean found = qfalse;

	flare->loaded = qfalse;
	if ( ri.FS_ReadFile( "scripts/lensflare2.txt", (void **)&buffer ) < 0 || !buffer ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: Could not open lens flare info file!\n" );
		return;
	}

	p = buffer;
	while ( p ) {
		token = COM_ParseExt( &p, qtrue );
		if ( !p ) {
			break;
		}
		if ( !Q_stricmp( token, "begin" ) ) {
			token = COM_ParseExt( &p, qfalse );
			if ( !Q_stricmp( token, section ) ) {
				found = qtrue;
				break;
			}
		}
	}
	if ( !found ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: could not find begin for section '%s' in lensflares\n", section );
		ri.FS_FreeFile( buffer );
		return;
	}

	flare->dotMin = 0.8f;
	flare->fullScale = 0.7f;
	flare->numFlares = 0;
	flare->fullscreen = NULL;
	flare->fullFade = 0;
	flare->fullActive = qfalse;
	flare->loaded = qtrue;

	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( !p ) {
			ri.Printf( PRINT_DEVELOPER, "WARNING: could not find end for section '%s' in lensflares\n", section );
			break;
		}
		if ( !Q_stricmp( token, "dot_min" ) ) {
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: invalid dot_min in lensflares\n" );
			} else {
				flare->dotMin = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "flare" ) ) {
			aliceFlareElement_t element;
			if ( flare->numFlares >= ALICE_MAX_FLARE_ELEMENTS ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: too many flares in section '%s' in lensflares\n", section );
				SkipRestOfLine( &p );
				continue;
			}
			element.alphaScale = 1.0f;
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no flare arg 1 for flare '%d' in lensflares\n", flare->numFlares );
				continue;
			}
			element.size = Q_atof( token );
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no flare arg 2 for flare '%d' in lensflares\n", flare->numFlares );
				continue;
			}
			element.where = Q_atof( token );
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no flare arg 3 for flare '%d' in lensflares\n", flare->numFlares );
				continue;
			}
			element.shader = R_FindShader( token, LIGHTMAP_NONE, qfalse );
			token = COM_ParseExt( &p, qfalse );
			if ( token[0] ) {
				element.alphaScale = Q_atof( token );
			}
			flare->flares[flare->numFlares++] = element;
		} else if ( !Q_stricmp( token, "fullscale" ) ) {
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no arg for fullscale, assuming default (in lensflares)\n" );
			} else {
				flare->fullScale = Q_atof( token );
			}
		} else if ( !Q_stricmp( token, "fullscreen" ) ) {
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no arg for fullsreen in lensflares\n" );
			} else {
				flare->fullscreen = R_FindShader( token, LIGHTMAP_NONE, qfalse );
			}
		} else if ( !Q_stricmp( token, "fullfade" ) ) {
			token = COM_ParseExt( &p, qfalse );
			if ( !token[0] ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: no arg for fullfade in lensflares\n" );
			} else {
				flare->fullFade = atoi( token );
			}
		} else if ( !Q_stricmp( token, "end" ) ) {
			break;
		}
	}

	if ( !flare->numFlares ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: no lensflares defined!\n" );
	}
	if ( !flare->fullscreen && !Q_stricmp( section, "sun" ) ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: no full screen blend shader!\n" );
	}
	/* The blend starts out already faded. */
	flare->fullTime = tr.refdef.time - flare->fullFade;
	ri.FS_FreeFile( buffer );
}

void R_AliceInitLensFlares( void ) {
	static const vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };

	Com_Memset( &aliceSunFlare, 0, sizeof( aliceSunFlare ) );
	Com_Memset( &aliceEntityFlare, 0, sizeof( aliceEntityFlare ) );
	Com_Memset( &aliceDlightFlare, 0, sizeof( aliceDlightFlare ) );
	Vector4Copy( white, aliceSunFlare.color );
	Vector4Copy( white, aliceEntityFlare.color );
	Vector4Copy( white, aliceDlightFlare.color );
	aliceSunSet = qfalse;

	R_AliceLoadLensFlare( &aliceSunFlare, "sun" );
	R_AliceLoadLensFlare( &aliceEntityFlare, "entity" );
	R_AliceLoadLensFlare( &aliceDlightFlare, "dlight" );
	aliceDlightFlare.corona = R_FindShader( "textures/sprites/corona", LIGHTMAP_NONE, qfalse );
}

/* The level's sun (0x004a88b0), or none for a level that names none. */
void R_AliceSetSunFlare( const vec3_t origin ) {
	if ( origin ) {
		VectorCopy( origin, aliceSunOrigin );
		aliceSunSet = qtrue;
	} else {
		aliceSunSet = qfalse;
		aliceSunFlare.inPortalSky = qfalse;
	}
}

void R_AliceSetSunFlareInPortalSky( void ) {
	aliceSunFlare.inPortalSky = qtrue;
}

/*
==================
R_AliceFlareSetup

Where a flare is drawn from and what its trace aims at (0x004a76f0). A source
seen through the sky portal is turned from the portal's frame into the view's
and put at its distance from the eye, and its trace aims 16,384 units along
that line, where only the sky should be.
==================
*/
static void R_AliceFlareSetup( aliceLensFlare_t *flare, const vec3_t origin ) {
	if ( flare->inPortalSky ) {
		vec3_t d, local;
		int k;
		VectorSubtract( origin, backEnd.refdef.alice.skyOrigin, d );
		for ( k = 0; k < 3; k++ ) {
			local[k] = DotProduct( d, backEnd.refdef.alice.skyAxis[k] );
		}
		VectorAdd( backEnd.refdef.vieworg, local, flare->position );
		VectorNormalize( local );
		VectorMA( backEnd.refdef.vieworg, 16384.0f, local, flare->farPoint );
		return;
	}
	VectorCopy( origin, flare->position );
	VectorCopy( origin, flare->farPoint );
}

/* The source through the view's matrices: its place on the screen, and
 * whether it lies inside the view volume (0x004a7950). */
static qboolean R_AliceFlareProject( aliceLensFlare_t *flare ) {
	const float *m = backEnd.viewParms.world.modelMatrix;
	const float *proj = backEnd.viewParms.projectionMatrix;
	float eye[4], clip[4];
	int i;

	for ( i = 0; i < 4; i++ ) {
		eye[i] = flare->position[0] * m[i] + flare->position[1] * m[4 + i] + flare->position[2] * m[8 + i] + m[12 + i];
	}
	for ( i = 0; i < 4; i++ ) {
		clip[i] = eye[0] * proj[i] + eye[1] * proj[4 + i] + eye[2] * proj[8 + i] + eye[3] * proj[12 + i];
	}
	if ( clip[3] == 0.0f ) {
		return qfalse;
	}
	flare->screen[0] = clip[0] / clip[3];
	flare->screen[1] = clip[1] / clip[3];
	for ( i = 0; i < 3; i++ ) {
		if ( !( clip[i] < clip[3] && -clip[3] < clip[i] ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

/* How squarely the view faces the source. */
static float R_AliceFlareDot( const aliceLensFlare_t *flare ) {
	vec3_t dir;
	VectorSubtract( flare->position, backEnd.viewParms.or.origin, dir );
	VectorNormalize( dir );
	return DotProduct( backEnd.viewParms.or.axis[0], dir );
}

/*
==================
R_AliceFlareVisible

The occlusion trace (CheckRay): from the eye to the flare's far point, against
solids and bodies. A body hit hides it; a source in the sky portal needs the
trace to end on sky, any other one needs it to reach the source. The client
game's own trace clips its entities as retail's does; without one the world
alone is traced.
==================
*/
static qboolean R_AliceFlareVisible( const aliceLensFlare_t *flare ) {
	enum { CHECKRAY_MASK = 0x2010001, BODY = 0x2000000, SKY = 0x4 };
	float fraction = 1.0f;
	int surfaceFlags = 0, contents = 0;

	if ( !ri.AliceTrace || !ri.AliceTrace( backEnd.viewParms.or.origin, flare->farPoint, CHECKRAY_MASK,
			&fraction, &surfaceFlags, &contents ) ) {
		trace_t trace;
		ri.CM_BoxTrace( &trace, backEnd.viewParms.or.origin, flare->farPoint, NULL, NULL, 0, CHECKRAY_MASK, qfalse );
		fraction = trace.fraction;
		surfaceFlags = trace.surfaceFlags;
		contents = trace.contents;
	}
	if ( contents & BODY ) {
		return qfalse;
	}
	if ( flare->inPortalSky ) {
		return ( surfaceFlags & SKY ) ? qtrue : qfalse;
	}
	return fraction == 1.0f ? qtrue : qfalse;
}

/* One quad centred at (x, y) with half-size s, in the view's normalised
 * device coordinates, texture row 0 at its top. */
static void RB_AliceFlareQuad( shader_t *shader, float x, float y, float s, const float rgb[3], float alpha ) {
	color4ub_t c;
	int k;

	if ( !shader ) {
		return;
	}
	for ( k = 0; k < 3; k++ ) {
		const float v = rgb[k] < 0.0f ? 0.0f : rgb[k] > 1.0f ? 1.0f : rgb[k];
		c.rgba[k] = (byte)( v * 255.0f );
	}
	c.rgba[3] = (byte)( ( alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha ) * 255.0f );
	RB_BeginSurface( shader, 0 );
	RB_AddQuadStamp2( x - s, y - s, 2.0f * s, 2.0f * s, 0.0f, 1.0f, 1.0f, 0.0f, c );
	RB_EndSurface();
}

/* A section's flares along the line through the centre (0x004a7a80). */
static void RB_AliceFlareChain( aliceLensFlare_t *flare ) {
	float dot, intensity;
	int i;

	if ( !R_AliceFlareProject( flare ) ) {
		return;
	}
	dot = R_AliceFlareDot( flare );
	if ( dot <= flare->dotMin || !R_AliceFlareVisible( flare ) ) {
		return;
	}
	intensity = flare->dotMin < 1.0f ? ( dot - flare->dotMin ) / ( 1.0f - flare->dotMin ) : 1.0f;
	for ( i = 0; i < flare->numFlares; i++ ) {
		const aliceFlareElement_t *e = &flare->flares[i];
		const float x = flare->screen[0] - 2.0f * flare->screen[0] * e->where;
		const float y = flare->screen[1] - 2.0f * flare->screen[1] * e->where;
		RB_AliceFlareQuad( e->shader, x, y, e->size, flare->color, intensity * e->alphaScale * flare->color[3] );
	}
	flare->fullActive = qtrue;
	flare->fullTime = backEnd.refdef.time;
	flare->fullAlpha = intensity * intensity * intensity * intensity * flare->fullScale;
}

/* A dynamic light's flares and then its corona, which needs only to be in
 * front of the view and unobstructed (0x004a7d00). */
static void RB_AliceFlareLight( aliceLensFlare_t *flare ) {
	const float size = r_lightcoronasize ? r_lightcoronasize->value : 0.1f;

	RB_AliceFlareChain( flare );
	if ( !R_AliceFlareVisible( flare ) || R_AliceFlareDot( flare ) <= 0.0f ) {
		return;
	}
	RB_AliceFlareQuad( flare->corona, flare->screen[0], flare->screen[1], size, flare->color, flare->color[3] );
}

/* The sun's full-screen blend, faded out over fullfade milliseconds from
 * the last frame its flares were drawn (0x004a7f00). */
static void RB_AliceFlareFullscreen( aliceLensFlare_t *flare ) {
	static const float white[3] = { 1.0f, 1.0f, 1.0f };
	const int elapsed = backEnd.refdef.time - flare->fullTime;
	float alpha;

	if ( !flare->fullscreen || !flare->fullActive ) {
		return;
	}
	if ( flare->fullFade <= 0 || elapsed > flare->fullFade ) {
		flare->fullActive = qfalse;
		return;
	}
	alpha = (float)( flare->fullFade - elapsed ) / (float)flare->fullFade * flare->fullAlpha;
	if ( !( alpha > 0.0001f ) ) {
		flare->fullActive = qfalse;
		return;
	}
	RB_AliceFlareQuad( flare->fullscreen, 0.0f, 0.0f, 1.0f, white, alpha );
}

static void RB_AliceFlareShaderColor( aliceLensFlare_t *flare, const color4ub_t shaderColor ) {
	flare->color[0] = shaderColor.rgba[0] * ( 1.0f / 255.0f );
	flare->color[1] = shaderColor.rgba[1] * ( 1.0f / 255.0f );
	flare->color[2] = shaderColor.rgba[2] * ( 1.0f / 255.0f );
}

/*
==================
R_AlicePlaceSunFlare

r_place_sunflare: the sun goes where the view's line meets the world, just
short of it (0x004a84c7..0x004a8596).
==================
*/
static void R_AlicePlaceSunFlare( void ) {
	trace_t trace;
	vec3_t end, sun;
	const float *forward = backEnd.viewParms.or.axis[0];

	VectorMA( backEnd.viewParms.or.origin, 16384.0f, forward, end );
	ri.CM_BoxTrace( &trace, backEnd.viewParms.or.origin, end, NULL, NULL, 0, CONTENTS_SOLID, qfalse );
	VectorMA( trace.endpos, -0.01f, forward, sun );
	ri.Cvar_Set( "r_sunflare", va( "%f %f %f", sun[0], sun[1], sun[2] ) );
}

/* Clip space from the flares' normalised device coordinates (column-major,
 * as vk_update_mvp takes it). */
static const float aliceFlareNDC[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.5f, 1.0f
};

void RB_AliceLensFlares( void ) {
	const trRefEntity_t *ent;
	const dlight_t *dl;
	int i;

	/* (A scene that samples the screen draws its views twice, the first
	 * time into the screen map; the flares go on the copy that is shown.) */
	if ( ri.gameId != COM_GAME_ALICE || !aliceSunFlare.loaded || !aliceSunFlare.numFlares
		|| ( backEnd.viewParms.passFlags & VPF_ALICE_SKY_PORTAL ) || backEnd.isHyperspace
		|| ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) || !r_sunflare
		|| vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( r_place_sunflare->integer ) {
		R_AlicePlaceSunFlare();
	}

	/* The quads are in the view's normalised device coordinates, as GL's
	 * glOrtho( -1, 1, -1, 1 ) over an identity model view puts them: x and y
	 * kept, every vertex at mid depth -- the flare shaders do not depth test
	 * -- and y turned for the Vulkan family's clip space, as vk.c's own
	 * matrices turn it. The identity model view also keeps Alice's far-plane
	 * fog off them, as GL's eye space does; a portal view's oblique near
	 * plane lives in its projection, which this replaces. */
	backEnd.currentEntity = &tr.worldEntity;
	Com_Memset( &backEnd.or, 0, sizeof( backEnd.or ) );
	backEnd.or.axis[0][0] = backEnd.or.axis[1][1] = backEnd.or.axis[2][2] = 1.0f;
	backEnd.or.modelMatrix[0] = backEnd.or.modelMatrix[5] = 1.0f;
	backEnd.or.modelMatrix[10] = backEnd.or.modelMatrix[15] = 1.0f;
	tess.depthRange = DEPTH_RANGE_NORMAL;
	vk_update_mvp( aliceFlareNDC );

	if ( Q_stricmp( r_sunflare->string, "0" ) ) {
		vec3_t sun = { 0.0f, 0.0f, 0.0f };
		aliceSunFlare.inPortalSky = r_sunflare_inportalsky->integer ? qtrue : qfalse;
		if ( sscanf( r_sunflare->string, "%f %f %f", &sun[0], &sun[1], &sun[2] ) > 0 ) {
			R_AliceFlareSetup( &aliceSunFlare, sun );
			RB_AliceFlareChain( &aliceSunFlare );
		}
	} else if ( aliceSunSet ) {
		R_AliceFlareSetup( &aliceSunFlare, aliceSunOrigin );
		RB_AliceFlareChain( &aliceSunFlare );
	}

	for ( i = 0, ent = backEnd.refdef.entities; i < backEnd.refdef.num_entities; i++, ent++ ) {
		const int fx = ent->e.alice.renderfx;
		if ( fx & FNTECH3_ALICE_RF_SKYENTITY ) {
			continue;
		}
		if ( fx & FNTECH3_ALICE_RF_LENSFLARE ) {
			aliceDlightFlare.inPortalSky = qfalse;
			R_AliceFlareSetup( &aliceDlightFlare, ent->e.origin );
			RB_AliceFlareShaderColor( &aliceDlightFlare, ent->e.shader );
			RB_AliceFlareLight( &aliceDlightFlare );
		}
		if ( fx & FNTECH3_ALICE_RF_VIEWLENSFLARE ) {
			aliceEntityFlare.inPortalSky = qfalse;
			R_AliceFlareSetup( &aliceEntityFlare, ent->e.origin );
			RB_AliceFlareShaderColor( &aliceEntityFlare, ent->e.shader );
			RB_AliceFlareChain( &aliceEntityFlare );
		}
	}

	for ( i = 0, dl = backEnd.refdef.dlights; i < (int)backEnd.refdef.num_dlights; i++, dl++ ) {
		if ( !( dl->aliceFlare & 3 ) ) {
			continue;
		}
		aliceDlightFlare.inPortalSky = qfalse;
		R_AliceFlareSetup( &aliceDlightFlare, dl->aliceFlareOrigin );
		VectorCopy( dl->aliceFlareColor, aliceDlightFlare.color );
		RB_AliceFlareLight( &aliceDlightFlare );
	}

	if ( !r_place_sunflare->integer ) {
		RB_AliceFlareFullscreen( &aliceSunFlare );
	}

	backEnd.or = backEnd.viewParms.world;
	Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelMatrix, 64 );
	vk_update_mvp( NULL );
}
