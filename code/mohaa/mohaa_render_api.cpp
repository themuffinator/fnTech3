/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor's model definitions for the renderers

The engine side of code/renderercommon/tr_mohaa_render.h: a definition's
surfaces, an entity's pose and a surface skinned by it. The skeletor that
poses a skeleton is the engine's, the same one 2015's game and client game
drive through their TIKI imports, so the renderer asks rather than reading
the skeletor's records itself.

The skinning follows OpenMoHAA's RB_SkelMesh (renderergl1/tr_model.cpp,
GPL-2.0-or-later), written here against the skeletor's own bone frame: a
vertex's position is the weighted sum of its offsets through each weight's
bone, the first offset moved first by the vertex's morphs; its normal is
turned by the bone of its first weight; the first mesh's weights name its
bones directly and every later mesh's name them by channel. Every position
is scaled by the definition's load scale and the entity's scale.

A surface loses vertices as its model grows small on screen, as 2015's
renderer draws it (OpenMoHAA GetLodCutoff, GetToolLodCutoff, R_CalcLod and
RB_SkelMesh, reference only). A mesh with collapse data has a
level-of-detail record (tiki_skel.cpp GetLODFile): the metric at which it
starts losing detail and the one at which it has lost all it will, and a
curve of five points between them, each a position and a cutoff. The metric
is the model's radius over its distance from the view, times 100 over the
view's horizontal field, the radius being 92 units over the entity's scale
for a character and its pose's radius otherwise. The player's r_lodscale
stretches the metric toward the far end and r_lodcap holds it back from the
near one; a view weapon takes r_lodviewmodelcap's place on the curve
instead; r_uselod 0 draws every model at its curve's first cutoff. The
vertices come ordered by their collapse index, which falls along them: the
ones at or above the cutoff are drawn, each later one collapses onto an
earlier one through the collapse map, and the triangles stop at the first
whose corners meet (the loader sorted them so, TIKI_SortLOD). A surface
whose third vertex is below the cutoff draws nothing.

The retail menus drive a level-of-detail tool (Pak0.pk3 ui/LODTool.URC,
bound to `l` in development.cfg): the game's `lod_spawn` sets `lod_tool`
and `lod_tikiname`, stands the player at a fixed spot and places its model
at `lod_position` less `lod_zee_val` every frame (gamex86.dll's LODSlave,
OpenMoHAA fgame/lodthing.cpp). The tool's sliders are the record of the
mesh `lod_mesh` picks: the two metrics and the five cutoffs as a share of
the mesh's largest level-of-detail index, which 1.00's own records bear
out (see MOHAA_LodToolFrame); `lod_save 1` writes it beside the skeleton
as its `.lod`. What the engine does with the rest is not in any retail file
this tree reads, so it is inferred from the menu's own comments: the LOD
slider sets `lod_LOD`, a metric between the two ends, and the model is
placed along the view where it measures that; each `lod_edit_N` button
moves that slider to the curve's point N; `lod_metric`, `lod_tris` and
`lod_meshname` show the metric the model was last drawn at, the triangles
its mesh drew of its total, and the mesh's skeleton.
===========================================================================
*/

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "corepp/tiki.h"
#include "skeletor/skeletor.h"
#include "tiki/tiki_skel.h"
#include "mohaa_tiki.h"
#include "../renderercommon/tr_mohaa_render.h"

#include <cmath>
#include <cstring>

static_assert( sizeof( mohaaFrameInfo_t ) == sizeof( frameInfo_t ), "a frame info is 2015's" );
static_assert( sizeof( SkelMat4 ) == 12 * sizeof( float ), "a bone frame is four rows of three" );

namespace {

constexpr int kBoneFloats = 12;

/* The definition's meshes' surfaces in walk order: mesh by mesh, each
 * mesh's linked list from its first. */
skelSurfaceGame_t *SurfaceAt( const dtiki_t *tiki, int index, int *meshOut ) {
	if ( !tiki || index < 0 ) {
		return nullptr;
	}
	for ( int mesh = 0; mesh < tiki->numMeshes; mesh++ ) {
		skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[mesh] );
		if ( !skel ) {
			continue;
		}
		if ( index < skel->numSurfaces ) {
			skelSurfaceGame_t *surface = skel->pSurfaces;
			for ( int i = 0; i < index && surface; i++ ) {
				surface = surface->pNext;
			}
			if ( meshOut ) {
				*meshOut = mesh;
			}
			return surface;
		}
		index -= skel->numSurfaces;
	}
	return nullptr;
}

int Channels( const dtiki_t *tiki ) {
	return tiki ? tiki->m_boneList.NumChannels() : 0;
}

/* The most morph weights a pose of this definition can carry: every mesh's
 * morph targets, which is what the skeletor's own morph list is built from. */
int MorphTargets( const dtiki_t *tiki ) {
	int count = 0;
	for ( int mesh = 0; tiki && mesh < tiki->numMeshes; mesh++ ) {
		const skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[mesh] );
		if ( skel ) {
			count += skel->numMorphTargets;
		}
	}
	return count;
}

/* A weight's bone as an index into the pose: the first mesh names it
 * directly, every later mesh by the channel it drives. */
int PoseBone( const dtiki_t *tiki, const skelHeaderGame_t *skel, int mesh, int boneIndex ) {
	if ( boneIndex < 0 || boneIndex >= skel->numBones ) {
		return -1;
	}
	if ( mesh == 0 ) {
		return boneIndex;
	}
	return tiki->m_boneList.GetLocalFromGlobal( skel->pBones[boneIndex].channel );
}

int NumSurfaces( const void *handle ) {
	const dtiki_t *tiki = static_cast<const dtiki_t *>( handle );
	int count = 0;
	for ( int mesh = 0; tiki && mesh < tiki->numMeshes; mesh++ ) {
		const skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[mesh] );
		if ( skel ) {
			count += skel->numSurfaces;
		}
	}
	return count;
}

qboolean Surface( const void *handle, int index, mohaaTikiSurface_t *out ) {
	const dtiki_t *tiki = static_cast<const dtiki_t *>( handle );
	skelSurfaceGame_t *surface = SurfaceAt( tiki, index, nullptr );
	if ( !surface || !out ) {
		return qfalse;
	}
	std::memset( out, 0, sizeof( *out ) );
	out->name = surface->name;
	out->numVerts = surface->numVerts;
	out->numIndexes = surface->numTriangles * 3;
	/* The definition's surface records run in the same order as the
	 * meshes' surfaces (OpenMoHAA R_AddSkelSurfaces walks both together). */
	if ( index < tiki->num_surfaces ) {
		const dtikisurface_t *definition = &tiki->surfaces[index];
		out->numSkins = definition->numskins;
		if ( out->numSkins > MOHAA_RENDER_MAX_SKINS ) {
			out->numSkins = MOHAA_RENDER_MAX_SKINS;
		}
		for ( int skin = 0; skin < out->numSkins; skin++ ) {
			out->shaders[skin] = definition->shader[skin];
		}
		out->flags = definition->flags;
	}
	return qtrue;
}

int PoseFloats( const void *handle ) {
	const dtiki_t *tiki = static_cast<const dtiki_t *>( handle );
	return Channels( tiki ) * kBoneFloats + MorphTargets( tiki );
}

qboolean Pose( const mohaaPoseInput_t *input, float *pose ) {
	if ( !input || !input->tiki || !pose ) {
		return qfalse;
	}
	dtiki_t *tiki = const_cast<dtiki_t *>( static_cast<const dtiki_t *>( input->tiki ) );
	const int channels = Channels( tiki );
	if ( channels <= 0 ) {
		return qfalse;
	}
	skeletor_c *skeletor = static_cast<skeletor_c *>( TIKI_GetSkeletor( tiki, input->entityNumber ) );
	if ( !skeletor ) {
		return qfalse;
	}
	TIKI_SetPoseInternal( skeletor, reinterpret_cast<const frameInfo_t *>( input->frameInfo ),
		input->boneTag, input->boneQuat, input->actionWeight );

	/* The skeletor's frame is a radius, bounds, and a bone per channel. */
	const size_t frameBytes = sizeof( skelAnimFrame_t ) + channels * sizeof( SkelMat4 );
	skelAnimFrame_t *frame = static_cast<skelAnimFrame_t *>( Hunk_AllocateTempMemory( static_cast<int>( frameBytes ) ) );
	skeletor->GetFrame( frame );
	std::memcpy( pose, frame->bones, channels * sizeof( SkelMat4 ) );
	Hunk_FreeTempMemory( frame );

	float *morphs = pose + channels * kBoneFloats;
	const int targets = MorphTargets( tiki );
	if ( targets > 0 ) {
		std::memset( morphs, 0, targets * sizeof( float ) );
		/* The skeletor writes whole-number weights, one per target it keeps. */
		int *weights = static_cast<int *>( Hunk_AllocateTempMemory( static_cast<int>( targets * sizeof( int ) ) ) );
		std::memset( weights, 0, targets * sizeof( int ) );
		const int written = skeletor->GetMorphWeightFrame( input->frameInfo[0].index, input->frameInfo[0].time, weights );
		for ( int i = 0; i < written && i < targets; i++ ) {
			morphs[i] = static_cast<float>( weights[i] );
		}
		Hunk_FreeTempMemory( weights );
	}
	return qtrue;
}

inline void Rotate( const float *bone, const float *in, float *out ) {
	for ( int j = 0; j < 3; j++ ) {
		out[j] = in[0] * bone[0 * 3 + j] + in[1] * bone[1 * 3 + j] + in[2] * bone[2 * 3 + j];
	}
}

/* ---- level of detail ---------------------------------------------------- */

constexpr float kCharacterLodRadius = 92.0f;

cvar_t *r_uselod;
cvar_t *r_lodscale;
cvar_t *r_lodcap;
cvar_t *r_lodviewmodelcap;

struct LodTool {
	cvar_t *tool, *tikiname, *mesh, *meshname, *metric, *tris, *save, *position, *zee;
	cvar_t *lod, *minLOD, *maxLOD, *slider;
	cvar_t *edit[MAX_LOD_CURVE_POINTS];
	cvar_t *value[MAX_LOD_CURVE_POINTS];
	cvar_t *curve[MAX_LOD_CURVE_POINTS];
	const dtiki_t *tiki;	/* the model on the tool, while it runs */
	int meshNum;
	float range;			/* the cutoff a slider at 1 stands for */
	float measured;			/* its metric and radius as last drawn, or -1 */
	float radius;
};
LodTool lodTool;

/* A cutoff held to what an int holds, as a malformed record can ask for
 * anything. */
int Truncate( float value ) {
	return value > -1.0e6f && value < 1.0e6f ? static_cast<int>( value ) : 0;
}

bool HasLod( const dtiki_t *tiki ) {
	for ( int mesh = 0; tiki && mesh < tiki->numMeshes; mesh++ ) {
		const skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[mesh] );
		if ( skel && skel->pLOD ) {
			return true;
		}
	}
	return false;
}

/* The cutoff a metric reads off a mesh's curve (GetLodCutoff). */
int LodCutoff( const lodControl_t *lod, float metric, bool viewModel ) {
	float cap;
	if ( viewModel ) {
		cap = lod->maxMetric + ( lod->minMetric - lod->maxMetric ) * r_lodviewmodelcap->value;
	} else {
		const float limit = ( lod->minMetric - lod->maxMetric ) * r_lodcap->value + lod->maxMetric;
		cap = ( metric - lod->maxMetric ) * r_lodscale->value + lod->maxMetric;
		if ( cap > limit ) {
			cap = limit;
		}
	}
	if ( cap >= lod->minMetric || !r_uselod->integer ) {
		return Truncate( lod->curve[0].val );
	}
	if ( cap <= lod->maxMetric ) {
		return Truncate( lod->curve[MAX_LOD_CURVE_POINTS - 1].val );
	}
	for ( int i = MAX_LOD_CURVE_CONSTS - 1; i > 0; i-- ) {
		if ( cap <= lod->consts[i].cutoff ) {
			return Truncate( cap * lod->consts[i].scale + lod->consts[i].base );
		}
	}
	return Truncate( cap * lod->consts[0].scale + lod->consts[0].base );
}

/* How many of a surface's vertices a cutoff keeps: the leading ones whose
 * collapse index reaches it, never fewer than three, or none when even the
 * third falls short (RB_SkelMesh's search). */
int LodVertexCount( const skelSurfaceGame_t *surface, int cutoff ) {
	const skelIndex_t *collapseIndex = surface->pCollapseIndex;
	if ( collapseIndex[2] < cutoff ) {
		return 0;
	}
	int low = 3, mid = 3, high = surface->numVerts;
	while ( high >= low ) {
		mid = ( low + high ) >> 1;
		if ( collapseIndex[mid] < cutoff ) {
			high = mid - 1;
			if ( collapseIndex[mid - 1] >= cutoff ) {
				break;
			}
		} else {
			mid++;
			low = mid;
			if ( high == mid || collapseIndex[mid] < cutoff ) {
				break;
			}
		}
	}
	return mid < 3 ? 3 : mid > surface->numVerts ? surface->numVerts : mid;
}

/* Each vertex's stand-in when `drawn` are kept: itself among those, else
 * what the vertex it collapses onto stands for. A collapse onto a later
 * vertex, which a well-formed skeleton never holds, falls on the first. */
void LodCollapse( const skelSurfaceGame_t *surface, int drawn, short *collapse ) {
	for ( int i = 0; i < surface->numVerts; i++ ) {
		const int onto = surface->pCollapse[i];
		collapse[i] = static_cast<short>( i < drawn ? i : onto >= 0 && onto < i ? collapse[onto] : 0 );
	}
}

/* A surface's triangles at a count of kept vertices: up to the first whose
 * corners meet. */
int LodTriangles( const skelSurfaceGame_t *surface, const short *collapse, unsigned int *indexes,
	unsigned int baseVertex ) {
	const int verts = surface->numVerts;
	int written = 0;
	for ( int i = 0; i + 2 < surface->numTriangles * 3; i += 3 ) {
		int corner[3];
		for ( int j = 0; j < 3; j++ ) {
			const int vertex = surface->pTriangles[i + j];
			corner[j] = collapse[vertex >= 0 && vertex < verts ? vertex : 0];
		}
		if ( corner[0] == corner[1] || corner[1] == corner[2] || corner[2] == corner[0] ) {
			break;
		}
		if ( indexes ) {
			for ( int j = 0; j < 3; j++ ) {
				indexes[written + j] = baseVertex + static_cast<unsigned int>( corner[j] );
			}
		}
		written += 3;
	}
	return written;
}

/* Whether a surface loses detail: it has collapse data to lose it by. */
bool LodSurface( const skelSurfaceGame_t *surface ) {
	return surface->numVerts > 3 && surface->pCollapse && surface->pCollapseIndex;
}

bool LodToolMesh( const dtiki_t *tiki, int mesh ) {
	return lodTool.tool && lodTool.tool->integer && tiki == lodTool.tiki && mesh == lodTool.meshNum;
}

int Skin( const void *handle, int index, const float *pose, float scale, float lod, int lodFlags,
	float ( *xyz )[4], float ( *normal )[4], float ( *st )[2], unsigned int *indexes,
	unsigned int baseVertex, int maxVerts, int maxIndexes, int *numIndexes ) {
	const dtiki_t *tiki = static_cast<const dtiki_t *>( handle );
	int mesh = 0;
	skelSurfaceGame_t *surface = SurfaceAt( tiki, index, &mesh );

	if ( numIndexes ) {
		*numIndexes = 0;
	}
	if ( !surface || !pose || !xyz || !normal || !st || !indexes ) {
		return 0;
	}
	const skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[mesh] );
	const int verts = surface->numVerts;
	const int count = surface->numTriangles * 3;
	if ( !skel || verts <= 0 || count <= 0 || verts > maxVerts || count > maxIndexes ) {
		return 0;
	}

	/* the vertices its level of detail keeps; a surface on the tool is the
	 * tool's to judge, never capped as a view weapon */
	int drawn = verts;
	if ( lod >= 0.0f && skel->pLOD && r_uselod && LodSurface( surface ) ) {
		const bool viewModel = ( lodFlags & MOHAA_LOD_VIEWMODEL ) && !LodToolMesh( tiki, mesh );
		drawn = LodVertexCount( surface, LodCutoff( skel->pLOD, lod, viewModel ) );
		if ( drawn <= 0 ) {
			return 0;
		}
	}

	const int channels = Channels( tiki );
	const float *morphs = pose + channels * kBoneFloats;
	const float fullScale = tiki->load_scale * scale;

	const byte *cursor = reinterpret_cast<const byte *>( surface->pVerts );
	for ( int v = 0; v < drawn; v++ ) {
		const skeletorVertex_t *vertex = reinterpret_cast<const skeletorVertex_t *>( cursor );
		const skeletorMorph_t *morph = reinterpret_cast<const skeletorMorph_t *>( vertex + 1 );
		const skelWeight_t *weight = reinterpret_cast<const skelWeight_t *>( morph + vertex->numMorphs );

		vec3_t totalMorph = { 0.0f, 0.0f, 0.0f };
		for ( int m = 0; m < vertex->numMorphs; m++ ) {
			const float amount = morph[m].morphIndex >= 0 ? morphs[morph[m].morphIndex] : 0.0f;
			if ( amount != 0.0f ) {
				VectorMA( totalMorph, amount, morph[m].offset, totalMorph );
			}
		}

		vec3_t position = { 0.0f, 0.0f, 0.0f };
		vec3_t turned = { 0.0f, 0.0f, 1.0f };
		for ( int w = 0; w < vertex->numWeights; w++ ) {
			const int boneNum = PoseBone( tiki, skel, mesh, weight[w].boneIndex );
			if ( boneNum < 0 || boneNum >= channels ) {
				continue;
			}
			const float *bone = pose + boneNum * kBoneFloats;
			vec3_t offset, rotated;
			VectorCopy( weight[w].offset, offset );
			if ( w == 0 ) {
				VectorAdd( offset, totalMorph, offset );
				Rotate( bone, vertex->normal, turned );
			}
			Rotate( bone, offset, rotated );
			for ( int j = 0; j < 3; j++ ) {
				position[j] += ( rotated[j] + bone[9 + j] ) * weight[w].boneWeight;
			}
		}

		xyz[v][0] = position[0] * fullScale;
		xyz[v][1] = position[1] * fullScale;
		xyz[v][2] = position[2] * fullScale;
		xyz[v][3] = 1.0f;
		normal[v][0] = turned[0];
		normal[v][1] = turned[1];
		normal[v][2] = turned[2];
		normal[v][3] = 0.0f;
		st[v][0] = vertex->texCoords[0];
		st[v][1] = vertex->texCoords[1];

		cursor += sizeof( skeletorVertex_t ) + sizeof( skeletorMorph_t ) * vertex->numMorphs
			+ sizeof( skelWeight_t ) * vertex->numWeights;
	}

	int written = count;
	if ( drawn == verts ) {
		for ( int i = 0; i < count; i++ ) {
			const int vertex = surface->pTriangles[i];
			indexes[i] = baseVertex + static_cast<unsigned int>( vertex >= 0 && vertex < verts ? vertex : 0 );
		}
	} else {
		short collapse[TIKI_MAX_VERTEXES];
		LodCollapse( surface, drawn, collapse );
		written = LodTriangles( surface, collapse, indexes, baseVertex );
	}
	if ( numIndexes ) {
		*numIndexes = written;
	}
	return written > 0 ? drawn : 0;
}

/* An entity's level-of-detail metric from a view (R_CalcLod): the model's
 * radius over the distance from the view to its centre, the load origin
 * turned by the entity's axis, times 100 over the view's field. */
float LodMetric( const mohaaPoseInput_t *input, float scale, const vec3_t origin, const float *axis,
	const vec3_t viewOrigin, float fovX ) {
	dtiki_t *tiki = input ? const_cast<dtiki_t *>( static_cast<const dtiki_t *>( input->tiki ) ) : nullptr;
	if ( !tiki || !origin || !axis || !viewOrigin || !HasLod( tiki ) ) {
		return -1.0f;
	}
	if ( scale <= 0.0f ) {
		scale = 1.0f;
	}
	vec3_t local, centre;
	VectorScale( tiki->load_origin, tiki->load_scale * scale, local );
	VectorCopy( origin, centre );
	for ( int i = 0; i < 3; i++ ) {
		VectorMA( centre, local[i], axis + i * 3, centre );
	}
	const float radius = tiki->a && tiki->a->bIsCharacter ? kCharacterLodRadius / scale
		: TIKI_GetRadiusInternal( tiki, input->entityNumber, scale ) / scale;
	const float distance = Distance( viewOrigin, centre );
	const float field = fovX > 1.0f ? fovX : 90.0f;
	const float metric = distance > 1.0f ? std::fabs( radius ) * ( 100.0f / field ) / distance : 1000.0f;
	if ( tiki == lodTool.tiki ) {
		lodTool.measured = metric;
		lodTool.radius = std::fabs( radius );
	}
	return metric;
}

/* The model the tool edits: `lod_tikiname` as the game module was handed
 * it, with or without the leading `models/`. */
dtiki_t *LodToolTiki( const char *name ) {
	if ( !name || !name[0] ) {
		return nullptr;
	}
	dtiki_t *tiki = TIKI_FindTiki( name );
	if ( !tiki && Q_stricmpn( name, "models/", 7 ) ) {
		tiki = TIKI_FindTiki( va( "models/%s", name ) );
	}
	return tiki;
}

/* `lod_save`: the record, little-endian, beside the skeleton it is for,
 * `.skd` made `.lod` (GetLODFile reads it back). */
void LodSave( const char *skeleton, const lodControl_t *lod ) {
	char path[MAX_QPATH];
	Q_strncpyz( path, skeleton, sizeof( path ) );
	char *extension = strrchr( path, '.' );
	if ( !extension || strchr( extension, '/' ) || Q_stricmp( extension, ".skd" ) ) {
		Com_Printf( "lod_save: %s is not a skeletal model\n", skeleton );
		return;
	}
	Q_strncpyz( extension, ".lod", static_cast<int>( sizeof( path ) - ( extension - path ) ) );

	lodControl_t out = *lod;
	out.minMetric = LittleFloat( out.minMetric );
	out.maxMetric = LittleFloat( out.maxMetric );
	for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
		out.curve[i].pos = LittleFloat( out.curve[i].pos );
		out.curve[i].val = LittleFloat( out.curve[i].val );
	}
	for ( int i = 0; i < MAX_LOD_CURVE_CONSTS; i++ ) {
		out.consts[i].base = LittleFloat( out.consts[i].base );
		out.consts[i].scale = LittleFloat( out.consts[i].scale );
		out.consts[i].cutoff = LittleFloat( out.consts[i].cutoff );
	}
	const fileHandle_t file = FS_FOpenFileWrite( path );
	if ( !file ) {
		Com_Printf( "lod_save: could not write %s\n", path );
		return;
	}
	FS_Write( &out, sizeof( out ), file );
	FS_FCloseFile( file );
	Com_Printf( "Wrote %s\n", path );
}

void SetCvarValue( cvar_t *cvar, float value ) {
	if ( cvar && cvar->value != value ) {
		Cvar_SetValue( cvar->name, value );
	}
}

void SetCvarString( cvar_t *cvar, const char *value ) {
	if ( cvar && strcmp( cvar->string, value ) ) {
		Cvar_Set( cvar->name, value );
	}
}

void Bounds( const void *handle, float scale, vec3_t mins, vec3_t maxs ) {
	dtiki_t *tiki = const_cast<dtiki_t *>( static_cast<const dtiki_t *>( handle ) );
	if ( !tiki ) {
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}
	TIKI_CalculateBounds( tiki, scale, mins, maxs );
}

float Radius( const void *handle ) {
	dtiki_t *tiki = const_cast<dtiki_t *>( static_cast<const dtiki_t *>( handle ) );
	return tiki ? TIKI_GlobalRadius( tiki ) : 0.0f;
}

float LoadScale( const void *handle ) {
	const dtiki_t *tiki = static_cast<const dtiki_t *>( handle );
	return tiki ? tiki->load_scale : 1.0f;
}

/* A static model's path is relative to `models/` unless it starts with it
 * (OpenMoHAA R_InitStaticModels), canonicalised as the TIKI cache keys it. */
const void *RegisterStatic( const char *path ) {
	char name[1024];
	if ( !path || !path[0] ) {
		return nullptr;
	}
	if ( !Q_stricmpn( path, "models", 6 ) ) {
		Q_strncpyz( name, path, sizeof( name ) );
	} else {
		Com_sprintf( name, sizeof( name ), "models/%s", path );
	}
	FS_CanonicalFilename( name );
	return TIKI_RegisterTikiFlags( name, qfalse );
}

/* The skeleton at its first animation's first frame, as the static model's
 * pose (TIKI_GetSkelAnimFrame); a definition without animations stays at
 * its bones' identity. */
qboolean StaticPose( const void *handle, float *pose ) {
	dtiki_t *tiki = const_cast<dtiki_t *>( static_cast<const dtiki_t *>( handle ) );
	const int channels = Channels( tiki );
	if ( !tiki || !pose || channels <= 0 ) {
		return qfalse;
	}
	skelBoneCache_t *bones = static_cast<skelBoneCache_t *>(
		Hunk_AllocateTempMemory( static_cast<int>( channels * sizeof( skelBoneCache_t ) ) ) );
	for ( int i = 0; i < channels; i++ ) {
		std::memset( &bones[i], 0, sizeof( bones[i] ) );
		bones[i].matrix[0][0] = bones[i].matrix[1][1] = bones[i].matrix[2][2] = 1.0f;
	}
	float radius;
	TIKI_GetSkelAnimFrame( tiki, bones, &radius, nullptr, nullptr );
	for ( int i = 0; i < channels; i++ ) {
		float *bone = pose + i * kBoneFloats;
		for ( int row = 0; row < 3; row++ ) {
			for ( int column = 0; column < 3; column++ ) {
				bone[row * 3 + column] = bones[i].matrix[row][column];
			}
		}
		bone[9] = bones[i].offset[0];
		bone[10] = bones[i].offset[1];
		bone[11] = bones[i].offset[2];
	}
	Hunk_FreeTempMemory( bones );
	const int targets = MorphTargets( tiki );
	std::memset( pose + channels * kBoneFloats, 0, targets * sizeof( float ) );
	return qtrue;
}

const mohaaRenderImport_t renderImport = {
	NumSurfaces,
	Surface,
	PoseFloats,
	Pose,
	Skin,
	Bounds,
	Radius,
	LoadScale,
	RegisterStatic,
	StaticPose,
	LodMetric,
};

} // namespace

extern "C" const mohaaRenderImport_t *MOHAA_RenderImport( void ) {
	return &renderImport;
}

/* 2015's level-of-detail cvars ([O] renderergl1/tr_init.c R_Register): the
 * player's, archived, and the tool's, which live for the session. */
extern "C" void MOHAA_LodRegister( void ) {
	r_uselod = Cvar_Get( "r_uselod", "1", CVAR_TEMP );
	Cvar_SetDescription( r_uselod, "Medal of Honor: 0 draws every skeletal model at its nearest level of detail." );
	r_lodscale = Cvar_Get( "r_lodscale", "1.1", CVAR_ARCHIVE );
	r_lodcap = Cvar_Get( "r_lodcap", "1.0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_lodcap, "Medal of Honor: the share of its level-of-detail range a model's detail may reach." );
	r_lodviewmodelcap = Cvar_Get( "r_lodviewmodelcap", "0.25", CVAR_ARCHIVE );
	Cvar_SetDescription( r_lodviewmodelcap, "Medal of Honor: the share of its level-of-detail range the view weapon is drawn at." );

	LodTool &t = lodTool;
	t.tool = Cvar_Get( "lod_tool", "0", CVAR_TEMP );
	t.tikiname = Cvar_Get( "lod_tikiname", "", CVAR_TEMP );
	t.mesh = Cvar_Get( "lod_mesh", "0", CVAR_TEMP );
	t.meshname = Cvar_Get( "lod_meshname", "", CVAR_TEMP );
	t.metric = Cvar_Get( "lod_metric", "0.0", CVAR_TEMP );
	t.tris = Cvar_Get( "lod_tris", "", CVAR_TEMP );
	t.save = Cvar_Get( "lod_save", "0", CVAR_TEMP );
	t.position = Cvar_Get( "lod_position", "0 0 0", CVAR_TEMP );
	t.zee = Cvar_Get( "lod_zee_val", "0", CVAR_TEMP );
	Cvar_Get( "lod_pitch_val", "0", CVAR_TEMP );
	t.lod = Cvar_Get( "lod_LOD", "0", CVAR_TEMP );
	t.minLOD = Cvar_Get( "lod_minLOD", "1.0", CVAR_TEMP );
	t.maxLOD = Cvar_Get( "lod_maxLOD", "0.3", CVAR_TEMP );
	t.slider = Cvar_Get( "lod_LOD_slider", "0.5", CVAR_TEMP );
	for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
		t.edit[i] = Cvar_Get( va( "lod_edit_%i", i ), "0", CVAR_TEMP );
		t.value[i] = Cvar_Get( va( "lod_curve_%i_val", i ), "0", CVAR_TEMP );
		t.curve[i] = Cvar_Get( va( "lod_curve_%i_slider", i ), "0", CVAR_TEMP );
	}
	t.tiki = nullptr;
	t.meshNum = -1;
	t.measured = -1.0f;
	t.radius = 0.0f;
}

/*
==================
MOHAA_LodToolFrame

Once a frame of the world view: while the game's tool runs, its sliders
become the level-of-detail record of the mesh it edits, a button press moves
the LOD slider to its curve point, `lod_save` writes the record, the model
is placed where it measures the slider's metric, and what it drew is shown.
==================
*/
extern "C" void MOHAA_LodToolFrame( const vec3_t viewOrigin, const vec3_t viewAxis[3], float fovX ) {
	LodTool &t = lodTool;
	if ( !t.tool || !t.tool->integer || !viewOrigin || !viewAxis ) {
		t.tiki = nullptr;
		t.meshNum = -1;
		return;
	}
	dtiki_t *tiki = LodToolTiki( t.tikiname->string );
	if ( !tiki || tiki->numMeshes <= 0 ) {
		t.tiki = nullptr;
		return;
	}
	const int meshNum = t.mesh->integer < 0 ? 0 : t.mesh->integer >= tiki->numMeshes ? tiki->numMeshes - 1
		: t.mesh->integer;
	skelHeaderGame_t *skel = TIKI_GetSkel( tiki->mesh[meshNum] );
	if ( !skel ) {
		return;
	}
	const skelcache_t *cache = TIKI_FindSkelByHeader( skel );
	SetCvarString( t.meshname, cache ? cache->path : skel->name );
	lodControl_t *lod = skel->pLOD;
	if ( !lod ) {
		t.tiki = tiki;
		t.meshNum = meshNum;
		SetCvarString( t.tris, "no LOD" );
		return;
	}

	/* a new mesh starts the sliders at its record. They span its largest
	 * level-of-detail index, or its record's largest cutoff when that is
	 * larger: 1.00's records reach that index (175 of its 580 end on it)
	 * and seldom pass it (29), where OpenMoHAA's range, the first positive
	 * index, would leave 540 of them past the sliders' reach */
	if ( tiki != t.tiki || meshNum != t.meshNum || t.mesh->modified ) {
		t.mesh->modified = qfalse;
		t.tiki = tiki;
		t.meshNum = meshNum;
		t.measured = -1.0f;
		t.range = 0.0f;
		for ( int i = 0; i < TIKI_SKEL_LOD_INDEXES; i++ ) {
			t.range = std::max( t.range, static_cast<float>( skel->lodIndex[i] ) );
		}
		for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
			t.range = std::max( t.range, lod->curve[i].val );
		}
		if ( !( t.range > 0.0f && t.range < 1.0e6f ) ) {
			t.range = 1.0f;
		}
		SetCvarValue( t.minLOD, lod->minMetric );
		SetCvarValue( t.maxLOD, lod->maxMetric );
		SetCvarValue( t.slider, 0.5f );
		for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
			SetCvarValue( t.curve[i], lod->curve[i].val / t.range );
		}
	}
	for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
		if ( t.edit[i]->integer ) {
			Cvar_Set( t.edit[i]->name, "0" );
			SetCvarValue( t.slider, 1.0f - lod->curve[i].pos );
		}
	}

	/* the sliders make the record */
	for ( int i = 0; i < MAX_LOD_CURVE_POINTS; i++ ) {
		lod->curve[i].val = t.curve[i]->value * t.range;
		SetCvarValue( t.value[i], lod->curve[i].val );
	}
	if ( t.minLOD->value > t.maxLOD->value && t.maxLOD->value > 0.0f ) {
		lod->minMetric = t.minLOD->value;
		lod->maxMetric = t.maxLOD->value;
	}
	TIKI_CalcLodConsts( lod );
	if ( t.save->integer ) {
		Cvar_Set( t.save->name, "0" );
		if ( cache ) {
			LodSave( cache->path, lod );
		}
	}

	/* the metric the slider asks for, and where along the view the model
	 * measures it, the middle of its height on the view's axis; the game
	 * lowers it by the zee slider */
	const float wanted = lod->maxMetric + ( lod->minMetric - lod->maxMetric ) * t.slider->value;
	SetCvarValue( t.lod, wanted );
	const float radius = t.radius > 0.0f ? t.radius
		: tiki->a && tiki->a->bIsCharacter ? kCharacterLodRadius : TIKI_GlobalRadius( tiki );
	const float field = fovX > 1.0f ? fovX : 90.0f;
	const float distance = wanted > 0.0001f ? radius * ( 100.0f / field ) / wanted : 8192.0f;
	vec3_t spot, mins, maxs;
	VectorMA( viewOrigin, distance, viewAxis[0], spot );
	TIKI_CalculateBounds( tiki, 1.0f, mins, maxs );
	spot[2] -= ( mins[2] + maxs[2] ) * 0.5f;
	SetCvarString( t.position, va( "%f %f %f", spot[0], spot[1], spot[2] ) );

	/* what the mesh drew at the metric it was last measured at */
	if ( t.measured >= 0.0f ) {
		SetCvarValue( t.metric, t.measured );
		const int cutoff = LodCutoff( lod, t.measured, false );
		int drawn = 0, total = 0;
		short collapse[TIKI_MAX_VERTEXES];
		const skelSurfaceGame_t *surface = skel->pSurfaces;
		for ( int s = 0; s < skel->numSurfaces && surface; s++, surface = surface->pNext ) {
			total += surface->numTriangles;
			if ( !LodSurface( surface ) ) {
				drawn += surface->numTriangles;
				continue;
			}
			const int kept = LodVertexCount( surface, cutoff );
			if ( kept <= 0 ) {
				continue;
			}
			LodCollapse( surface, kept, collapse );
			drawn += kept == surface->numVerts ? surface->numTriangles
				: LodTriangles( surface, collapse, nullptr, 0 ) / 3;
		}
		SetCvarString( t.tris, va( "%i / %i tris", drawn, total ) );
	}
}
