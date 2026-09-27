/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor's terrain and static models, as world surfaces

A "2015" map keeps two kinds of world geometry outside its surface lump:
terrain patches, each a 9 by 9 heightmap over 512 by 512 units with its own
shader, lightmap block and texture corners; and static models, TIKI
definitions the map places, each with the vertex colours the map compiler
lit it with. 2015's renderer draws both with renderers of their own -- a
terrain tessellator that lowers its detail with distance, and a static model
list walked through each visible leaf. Here both are built once, as the map
loads, into ordinary triangle surfaces of the translated map, listed in the
same leafs the map lists them in, so every renderer draws them as world
geometry with its own lighting, visibility and batching.

Terrain is built at its full detail: the eighty-one points of each patch,
two triangles a square, split along the diagonal the collision model uses
(code/qcommon/cm_mohaa_terrain.c), less the triangles the patch's variance
tree takes out, which the collision model walks through and 2015's renderer
never drew (code/qcommon/mohaa_terrain_squares.h) -- so what is seen is what
is walked on. Its texture coordinates are the patch's four corners
interpolated across it, its lightmap coordinates a block of 8 * lmapScale
texels from (s, t) of its lightmap (OpenMoHAA renderergl1/tr_bsp.c
R_UnpackTerraPatch and tr_terrain.c, reference only). A patch keeps its
surface, in the map's order, however few triangles it has left, so a surface
names its patch.

A static model is skinned once, in the first frame of its first animation,
by the engine that loads its definition (code/renderercommon/
tr_mohaa_render.h), placed by its origin, angles and scale, and coloured by
its vertex colours in the order its surfaces list its vertices (OpenMoHAA
R_InitStaticModels and R_AddStaticModelSurfaces, reference only). A model
the map gives no colours is lit white. It keeps its full detail: 2015's
renderer lowered a static model's with distance as it does an entity's.

The level's permanent decals, which 2015's renderer draws from lists it
keeps beside each leaf, join the same way (see "Permanent decals" below).

Triangles run clockwise seen from the front, as the map's own do.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/dcl_mohaa.h"
#include "../qcommon/mohaa_terrain_squares.h"
#include "../renderercommon/tr_public.h"
#include "tr_mohaa_render.h"
#include "tr_mohaa_world.h"

#define TERRAIN_POINTS			9
#define TERRAIN_VERTS			( TERRAIN_POINTS * TERRAIN_POINTS )
#define TERRAIN_INDEXES			( 8 * 8 * 6 )
#define TERRAIN_LIGHTMAP_SIZE	128.0f
/* A vertex-lit surface's lightmap number, the renderers' LIGHTMAP_BY_VERTEX. */
#define MOHAA_LIGHTMAP_BY_VERTEX	-3

typedef struct {
	const void	*tiki;
	int			firstSurface;	/* in the new surfaces */
	int			numSurfaces;
} staticModelBuild_t;

typedef struct {
	char		name[MAX_QPATH];
} shaderName_t;

static const lump_t *R_MOHAALump( const dheader_t *header, int lump ) {
	return &header->lumps[lump];
}

static const void *R_MOHAALumpData( const dheader_t *header, int lump ) {
	return (const byte *)header + LittleLong( R_MOHAALump( header, lump )->fileofs );
}

static int R_MOHAALumpCount( const dheader_t *header, int lump, int size ) {
	return LittleLong( R_MOHAALump( header, lump )->filelen ) / size;
}

/* The part of a leaf's surface list that lies inside the map's own, as
 * [*lo, *hi), and its length: a damaged leaf's range may start or end
 * outside it, and the sum that would find its end can overflow. */
static int R_MOHAALeafSurfacesInside( const dleaf_t *leaf, int numLeafSurfaces, int *lo, int *hi ) {
	const int first = LittleLong( leaf->firstLeafSurface );
	const int count = LittleLong( leaf->numLeafSurfaces );

	*lo = first < 0 ? 0 : first > numLeafSurfaces ? numLeafSurfaces : first;
	*hi = count <= 0 ? *lo : first > numLeafSurfaces - count ? numLeafSurfaces : first + count;
	if ( *hi < *lo ) {
		*hi = *lo;
	}
	return *hi - *lo;
}

/* A shader appended for the static models, or the one already appended. */
static int R_MOHAAStaticShader( shaderName_t *names, int *count, int max, int base, const char *name ) {
	int i;
	if ( !name || !name[0] ) {
		name = "<default>";
	}
	for ( i = 0; i < *count; i++ ) {
		if ( !Q_stricmp( names[i].name, name ) ) {
			return base + i;
		}
	}
	if ( *count >= max ) {
		return base;
	}
	Q_strncpyz( names[*count].name, name, sizeof( names[*count].name ) );
	return base + ( *count )++;
}

/* Clockwise seen from the front: the cross of the edges points away from the
 * face's normal. */
static void R_MOHAAWindTriangle( const drawVert_t *verts, int *tri, const vec3_t up ) {
	vec3_t e1, e2, cross;
	VectorSubtract( verts[tri[1]].xyz, verts[tri[0]].xyz, e1 );
	VectorSubtract( verts[tri[2]].xyz, verts[tri[0]].xyz, e2 );
	CrossProduct( e1, e2, cross );
	if ( DotProduct( cross, up ) > 0.0f ) {
		const int swap = tri[1];
		tri[1] = tri[2];
		tri[2] = swap;
	}
}

static float R_MOHAATerrainHeight( const dterrainPatchMOHAA_t *patch, int i, int j ) {
	if ( i < 0 ) i = 0;
	if ( i > 8 ) i = 8;
	if ( j < 0 ) j = 0;
	if ( j > 8 ) j = 8;
	return (float)LittleShort( patch->iBaseHeight ) + 2.0f * patch->heightmap[j * TERRAIN_POINTS + i];
}

/*
==================
R_MOHAABuildTerrain

One patch's points, at (x, y) * 64 plus 64 units a point, heights two
units a step above its base; its texture corners texCoord[x][y]. Returns
the indexes written for the triangles its squares keep.
==================
*/
static int R_MOHAABuildTerrain( const dterrainPatchMOHAA_t *patch, const mohaaTerrainSquareTable_t *squares,
	drawVert_t *verts, int *indexes ) {
	const float x0 = (float)( (int)patch->x * 64 );
	const float y0 = (float)( (int)patch->y * 64 );
	const float lmapScale = patch->lmapScale > 0 ? (float)patch->lmapScale : 1.0f;
	const float ls = ( (float)patch->s + 0.5f ) / TERRAIN_LIGHTMAP_SIZE;
	const float lt = ( (float)patch->t + 0.5f ) / TERRAIN_LIGHTMAP_SIZE;
	const float lsize = ( lmapScale * 8.0f ) / TERRAIN_LIGHTMAP_SIZE;
	const vec3_t up = { 0.0f, 0.0f, 1.0f };
	float sMin, tMin;
	float corner[2][2][2];
	int i, j, k, n;

	/* The corners are made small by their lowest whole number, as 2015's
	 * terrain loader makes them, which changes nothing a repeating texture
	 * shows and keeps the interpolation precise. */
	sMin = LittleFloat( patch->texCoord[0][0][0] );
	tMin = LittleFloat( patch->texCoord[0][0][1] );
	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			const float s = LittleFloat( patch->texCoord[i][j][0] );
			const float t = LittleFloat( patch->texCoord[i][j][1] );
			if ( s < sMin ) sMin = s;
			if ( t < tMin ) tMin = t;
		}
	}
	sMin = floorf( sMin );
	tMin = floorf( tMin );
	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			corner[i][j][0] = LittleFloat( patch->texCoord[i][j][0] ) - sMin;
			corner[i][j][1] = LittleFloat( patch->texCoord[i][j][1] ) - tMin;
		}
	}

	for ( j = 0; j < TERRAIN_POINTS; j++ ) {
		for ( i = 0; i < TERRAIN_POINTS; i++ ) {
			drawVert_t *v = &verts[j * TERRAIN_POINTS + i];
			const float u = i / 8.0f, w = j / 8.0f;
			vec3_t normal;

			v->xyz[0] = x0 + i * 64.0f;
			v->xyz[1] = y0 + j * 64.0f;
			v->xyz[2] = R_MOHAATerrainHeight( patch, i, j );
			for ( k = 0; k < 2; k++ ) {
				v->st[k] = ( 1.0f - u ) * ( 1.0f - w ) * corner[0][0][k] + u * ( 1.0f - w ) * corner[1][0][k]
					+ ( 1.0f - u ) * w * corner[0][1][k] + u * w * corner[1][1][k];
			}
			v->lightmap[0] = ls + u * lsize;
			v->lightmap[1] = lt + w * lsize;
			normal[0] = R_MOHAATerrainHeight( patch, i - 1, j ) - R_MOHAATerrainHeight( patch, i + 1, j );
			normal[1] = R_MOHAATerrainHeight( patch, i, j - 1 ) - R_MOHAATerrainHeight( patch, i, j + 1 );
			normal[2] = 128.0f;
			VectorNormalize( normal );
			VectorCopy( normal, v->normal );
			v->color.rgba[0] = v->color.rgba[1] = v->color.rgba[2] = v->color.rgba[3] = 255;
		}
	}

	n = 0;
	for ( j = 0; j < 8; j++ ) {
		for ( i = 0; i < 8; i++ ) {
			const int v1 = j * TERRAIN_POINTS + i;
			const int v2 = v1 + 1;
			const int v3 = v2 + TERRAIN_POINTS;
			const int v4 = v1 + TERRAIN_POINTS;
			const int mode = MOHAA_TerrainSquareMode( squares, patch, i, j );
			int tris[2][3];
			int t;

			/* the first triangle, then the second, as the collision
			 * model's two planes take them */
			if ( ( i + j ) & 1 ) {
				tris[0][0] = v2; tris[0][1] = v4; tris[0][2] = v3;
				tris[1][0] = v4; tris[1][1] = v2; tris[1][2] = v1;
			} else {
				tris[0][0] = v3; tris[0][1] = v1; tris[0][2] = v4;
				tris[1][0] = v1; tris[1][1] = v3; tris[1][2] = v2;
			}
			for ( t = 0; t < 2; t++ ) {
				if ( !MOHAA_TerrainSquareHasTriangle( mode, t ) ) {
					continue;
				}
				R_MOHAAWindTriangle( verts, tris[t], up );
				indexes[n++] = tris[t][0];
				indexes[n++] = tris[t][1];
				indexes[n++] = tris[t][2];
			}
		}
	}
	return n;
}

/*
===========================================================================

THE LIGHT GRID

2015's map keeps one colour for each 32-unit cell of its world model's
bounds, from a 256-colour palette, stored column by column up the z axis and
run-length coded: a negative signed byte starts a block of that many
entries, any other byte n a run of n + 2 copies of the one entry after it; 0
is a cell with no light, inside a wall. The column for (x, y) starts at
offsets[bx + y + by * x] + ( offsets[x] << 8 ), bx and by being the grid's
width and depth (OpenMoHAA renderergl1/tr_bsp.c R_LoadLightGrid and
tr_light.c R_GetLightingGridValue, reference only). Its lights for models are
kept apart, as sphere lights, and each leaf lists the ones that reach it, the
sun first when it does.

Retail lights a model with the worldspawn's `ambientlight` plus 0.18 of the
grid's colour, and with those lights, the sun among them, each scaled by an
entity light scale of 1.3 (tr_sphere_shade.cpp RB_Sphere_ResetPointColors,
RB_Sphere_AddLight, RB_Sphere_Light_Sun; tr_init.c r_entlight_scale). Here
that becomes the renderers' own light grid, which their entity lighting
already reads: at each of its points the ambient is that share of the grid
and the directed light the sum of the lights the point's leaf lists, from
their weighted mean direction (AGENTS.md invariant 11). The grid is as fine
as a set size allows, so its spacing is written into the worldspawn as
`gridsize`. Lights that retail traces before using are taken untraced, and
the sun is taken wherever the leaf lists it.
===========================================================================
*/

#define MOHAA_GRID_CELL			32.0f
#define MOHAA_GRID_BUDGET		( 8 * 1024 * 1024 )
#define MOHAA_GRID_AMBIENT		0.18f
#define MOHAA_ENTLIGHT_SCALE	1.3f
#define MOHAA_LIGHT_SCALE		7500.0f
#define MOHAA_LIGHT_THRESHOLD	5.0f

typedef struct {
	const mohaa_translation_t	*translation;
	vec3_t			origin;			/* 2015's grid */
	int				bounds[3];
	vec3_t			ambient;		/* worldspawn `ambientlight` */
	qboolean		sun;
	vec3_t			sunColor;
	vec3_t			sunDirection;
	const dnode_t	*nodes;
	int				numNodes;
	const dplane_t	*planes;
	int				numPlanes;
	int				numLeafs;
	int				*leafLights;	/* numLeafs pairs: first, count in sphereLightVis */
} mohaaGrid_t;

/* The worldspawn's lighting keys (OpenMoHAA R_Sphere_InitLights): a sun
 * exists once either its colour or its direction is given, its colour 70
 * until given. */
static void R_MOHAAGridWorldspawn( const char *entities, int length, mohaaGrid_t *grid ) {
	char *text;
	const char *p;
	int depth = 0;

	VectorClear( grid->ambient );
	VectorSet( grid->sunColor, 70.0f, 70.0f, 70.0f );
	VectorClear( grid->sunDirection );
	grid->sun = qfalse;
	if ( !entities || length <= 0 ) {
		return;
	}
	text = ri.Malloc( length + 1 );
	Com_Memcpy( text, entities, length );
	text[length] = '\0';
	p = text;
	for ( ;; ) {
		char key[MAX_TOKEN_CHARS];
		const char *token = COM_ParseExt( &p, qtrue );
		if ( !token[0] ) {
			break;
		}
		if ( token[0] == '{' ) {
			depth++;
			continue;
		}
		if ( token[0] == '}' ) {
			break;
		}
		if ( depth != 1 ) {
			break;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_ParseExt( &p, qtrue );
		if ( !Q_stricmp( key, "ambientlight" ) ) {
			sscanf( token, "%f %f %f", &grid->ambient[0], &grid->ambient[1], &grid->ambient[2] );
		} else if ( !Q_stricmp( key, "suncolor" ) || !Q_stricmp( key, "sunlight" ) ) {
			sscanf( token, "%f %f %f", &grid->sunColor[0], &grid->sunColor[1], &grid->sunColor[2] );
			grid->sun = qtrue;
		} else if ( !Q_stricmp( key, "sundirection" ) ) {
			vec3_t angles = { 0.0f, 0.0f, 0.0f };
			sscanf( token, "%f %f %f", &angles[0], &angles[1], &angles[2] );
			AngleVectors( angles, grid->sunDirection, NULL, NULL );
			grid->sun = qtrue;
		}
	}
	ri.Free( text );
}

/* The palette entries at heights z and z + 1 of a column. */
static void R_MOHAAGridColumn( const mohaaGrid_t *grid, int x, int y, int z, int values[2] ) {
	const mohaa_translation_t *t = grid->translation;
	const byte *data = t->lightGridData;
	const int index = grid->bounds[0] + y + grid->bounds[1] * x;
	int offset, height = 0;

	values[0] = values[1] = 0;
	if ( x < 0 || x >= grid->bounds[0] || index < 0 || index >= t->numLightGridOffsets ) {
		return;
	}
	offset = t->lightGridOffsets[index] + ( t->lightGridOffsets[x] << 8 );
	while ( offset < t->lightGridDataLength && height <= z + 1 ) {
		const int header = (signed char)data[offset++];
		if ( header < 0 ) {
			int k;
			for ( k = 0; k < -header && offset < t->lightGridDataLength; k++, offset++, height++ ) {
				if ( height == z ) {
					values[0] = data[offset];
				} else if ( height == z + 1 ) {
					values[1] = data[offset];
				}
			}
		} else {
			const int run = header + 2;
			int value;
			if ( offset >= t->lightGridDataLength ) {
				break;
			}
			value = data[offset++];
			if ( z >= height && z < height + run ) {
				values[0] = value;
			}
			if ( z + 1 >= height && z + 1 < height + run ) {
				values[1] = value;
			}
			height += run;
		}
	}
}

/* The grid's colour at a point, blended from the eight cells around it
 * without the ones in walls, as retail's R_GetLightingGridValue does.
 * False when all eight are in walls. */
static qboolean R_MOHAAGridSample( const mohaaGrid_t *grid, const vec3_t point, vec3_t color ) {
	const byte *palette = grid->translation->lightGridPalette;
	int pos[3], dx, dy, i;
	float frac[3], total = 0.0f;

	VectorClear( color );
	for ( i = 0; i < 3; i++ ) {
		const float v = ( point[i] - grid->origin[i] ) / MOHAA_GRID_CELL;
		pos[i] = (int)floorf( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] > grid->bounds[i] - 2 ) {
			pos[i] = grid->bounds[i] - 2;
		}
	}
	for ( dx = 0; dx < 2; dx++ ) {
		for ( dy = 0; dy < 2; dy++ ) {
			const float wxy = ( dx ? frac[0] : 1.0f - frac[0] ) * ( dy ? frac[1] : 1.0f - frac[1] );
			const float weights[2] = { wxy * ( 1.0f - frac[2] ), wxy * frac[2] };
			int values[2], k;
			R_MOHAAGridColumn( grid, pos[0] + dx, pos[1] + dy, pos[2], values );
			for ( k = 0; k < 2; k++ ) {
				if ( values[k] ) {
					const byte *c = &palette[values[k] * 3];
					color[0] += weights[k] * c[0];
					color[1] += weights[k] * c[1];
					color[2] += weights[k] * c[2];
					total += weights[k];
				}
			}
		}
	}
	if ( total <= 0.0f ) {
		return qfalse;
	}
	if ( total < 0.99f ) {
		VectorScale( color, 1.0f / total, color );
	}
	return qtrue;
}

/* The leaf of the translated map a point is in, or -1. */
static int R_MOHAAPointLeaf( const mohaaGrid_t *grid, const vec3_t point ) {
	int node = 0, steps = 0;
	while ( node >= 0 ) {
		const dnode_t *n;
		int planeNum;
		float d;
		if ( node >= grid->numNodes || ++steps > 4096 ) {
			return -1;
		}
		n = &grid->nodes[node];
		planeNum = LittleLong( n->planeNum );
		if ( planeNum < 0 || planeNum >= grid->numPlanes ) {
			return -1;
		}
		d = DotProduct( point, grid->planes[planeNum].normal ) - LittleFloat( grid->planes[planeNum].dist );
		node = LittleLong( n->children[d >= 0.0f ? 0 : 1] );
	}
	node = -1 - node;
	return node < grid->numLeafs ? node : -1;
}

/* A colour held to a byte's range with its hue kept. */
static void R_MOHAANormalizeColor( vec3_t color ) {
	const float peak = MAX( color[0], MAX( color[1], color[2] ) );
	if ( peak > 255.0f ) {
		VectorScale( color, 255.0f / peak, color );
	}
}

/* The light from the sun and the sphere lights a leaf lists, at a point:
 * their summed colour and their mean direction weighted by brightness. */
static void R_MOHAAGridLights( const mohaaGrid_t *grid, int leaf, const vec3_t point, vec3_t color, vec3_t direction ) {
	const mohaa_translation_t *t = grid->translation;
	int first, count, i;

	VectorClear( color );
	VectorClear( direction );
	if ( leaf < 0 || !grid->leafLights ) {
		return;
	}
	first = grid->leafLights[leaf * 2];
	count = grid->leafLights[leaf * 2 + 1];
	for ( i = 0; i < count; i++ ) {
		const int entry = LittleLong( t->sphereLightVis[first + i] );
		vec3_t light, toLight;
		float weight;

		if ( entry == -2 ) {
			if ( !grid->sun ) {
				continue;
			}
			VectorScale( grid->sunColor, MOHAA_ENTLIGHT_SCALE, light );
			VectorCopy( grid->sunDirection, toLight );
		} else if ( entry >= 0 && entry < t->numSphereLights ) {
			const dsphereLightMOHAA_t *sl = &t->sphereLights[entry];
			const float intensity = LittleFloat( sl->intensity );
			vec3_t origin, lightColor;
			float distanceSquared, falloff;
			int k;

			if ( intensity <= 0.0f ) {
				continue;	/* lights only the invisible */
			}
			for ( k = 0; k < 3; k++ ) {
				origin[k] = LittleFloat( sl->origin[k] );
				lightColor[k] = LittleFloat( sl->color[k] );
			}
			VectorSubtract( origin, point, toLight );
			distanceSquared = DotProduct( toLight, toLight );
			if ( distanceSquared < 1.0f ) {
				distanceSquared = 1.0f;
			}
			falloff = intensity * MOHAA_LIGHT_SCALE / distanceSquared;
			if ( falloff < MOHAA_LIGHT_THRESHOLD ) {
				continue;
			}
			if ( LittleLong( sl->spotLight ) ) {
				/* lit along the spot direction, full to nine tenths of the
				 * cone's radius, fading out at its edge */
				const float radius = LittleFloat( sl->spotRadiusByDistance );
				vec3_t dir, axis;
				float depth, q2, r2, cone;
				for ( k = 0; k < 3; k++ ) {
					dir[k] = LittleFloat( sl->spotDir[k] );
				}
				depth = -DotProduct( toLight, dir );
				if ( depth <= 0.0f || radius <= 0.0f ) {
					continue;
				}
				VectorMA( toLight, depth, dir, axis );
				q2 = DotProduct( axis, axis ) / ( depth * depth );
				r2 = radius * radius;
				cone = ( r2 - q2 ) / ( r2 * 0.19f );
				if ( cone <= 0.0f ) {
					continue;
				}
				if ( cone < 1.0f ) {
					falloff *= cone;
				}
			}
			VectorScale( lightColor, falloff * MOHAA_ENTLIGHT_SCALE, light );
			VectorNormalize( toLight );
		} else {
			continue;
		}
		VectorAdd( color, light, color );
		weight = light[0] * 0.299f + light[1] * 0.587f + light[2] * 0.114f;
		VectorMA( direction, weight, toLight, direction );
	}
	R_MOHAANormalizeColor( color );
	if ( VectorNormalize( direction ) == 0.0f ) {
		VectorSet( direction, 0.0f, 0.0f, 1.0f );
	}
}

/* The Quake III grid's spacing: the finest multiple of 2015's cell from two
 * cells up whose grid fits the budget, as the renderers will size it from
 * the world model's bounds. */
static float R_MOHAAGridSpacing( const vec3_t mins, const vec3_t maxs, int bounds[3], vec3_t origin ) {
	float size = MOHAA_GRID_CELL * 2.0f;
	for ( ;; size += MOHAA_GRID_CELL ) {
		int i;
		double points = 1.0;
		for ( i = 0; i < 3; i++ ) {
			const float top = size * floor( maxs[i] / size );
			origin[i] = size * ceil( mins[i] / size );
			bounds[i] = (int)( ( top - origin[i] ) / size + 1 );
			if ( bounds[i] < 1 ) {
				bounds[i] = 1;
			}
			points *= bounds[i];
		}
		if ( points * 8.0 <= MOHAA_GRID_BUDGET || size >= 1024.0f ) {
			return size;
		}
	}
}

/* One Quake III grid point: ambient and directed colour, then the
 * direction to the light as two angles, each a turn in 256 steps. */
static void R_MOHAAGridPoint( const mohaaGrid_t *grid, const vec3_t point, byte *out ) {
	vec3_t sample, ambient, directed, direction;
	float lat, lng;
	int leaf, k;

	Com_Memset( out, 0, 8 );
	if ( !R_MOHAAGridSample( grid, point, sample ) ) {
		return;		/* in a wall: the renderers skip an unlit point */
	}
	for ( k = 0; k < 3; k++ ) {
		ambient[k] = ( grid->ambient[k] + sample[k] * MOHAA_GRID_AMBIENT ) * MOHAA_ENTLIGHT_SCALE;
	}
	R_MOHAANormalizeColor( ambient );
	leaf = R_MOHAAPointLeaf( grid, point );
	R_MOHAAGridLights( grid, leaf, point, directed, direction );
	for ( k = 0; k < 3; k++ ) {
		out[k] = (byte)ambient[k];
		out[3 + k] = (byte)directed[k];
	}
	/* an unlit point's zero bytes would read as one in a wall */
	if ( !out[0] && !out[1] && !out[2] ) {
		out[0] = out[1] = out[2] = 1;
	}
	lng = acosf( direction[2] < -1.0f ? -1.0f : direction[2] > 1.0f ? 1.0f : direction[2] );
	lat = atan2f( direction[1], direction[0] );
	if ( lat < 0.0f ) {
		lat += 2.0f * M_PI;
	}
	out[6] = (byte)( (int)( lng * 256.0f / ( 2.0f * M_PI ) + 0.5f ) & 255 );
	out[7] = (byte)( (int)( lat * 256.0f / ( 2.0f * M_PI ) + 0.5f ) & 255 );
}

/*
===========================================================================
Permanent decals

A level's decal file (code/qcommon/dcl_mohaa.h) is built into the map as its
terrain and static models are. Each fragment of three or more points becomes
a fan of triangles with its own shader, coloured by its points' own colours,
which tr_bsp.c's ParseTriSurf leaves unshifted as the scene's polygons are,
2015's own marks among them. It is listed in each leaf one of its points
lies in, a unit in front of the fragment, and in the leaf of their centre:
where 2015's editor links a fragment it has just cut (OpenMoHAA
R_ApplyLevelDecal, reference only), and so where its renderer draws it from.
The file's own lists are not read: eight of the eleven 1.00 files whose map
ships were saved before that map's last compile, and number leafs it no
longer has (docs/fntech3/2026-09-24-mohaa-contracts.md).

A fragment on a brush model would move with the model and is left out;
1.00's files have none. A version 2 file cut against another compile of its
map is refused, as later engines refuse it. Nothing is built while
`dcl_editmode` is set, when the editor draws the decals itself; the
renderers skip the built surfaces while `r_drawstaticdecals` is 0.
===========================================================================
*/

typedef struct {
	const byte	*points;		/* in the file */
	int			numPoints;
	char		shader[MAX_QPATH];
	vec3_t		normal;			/* the front, which the points wind clockwise around */
} mohaaDecal_t;

typedef struct {
	void			*file;
	mohaaDecal_t	*fragments;		/* the ones built */
	int				numFragments;
	int				numVerts;
	int				numIndexes;
	int				*leafFirst;		/* each leaf's first in leafDecals, and one past the last */
	int				*leafDecals;	/* built fragments' numbers, leaf by leaf */
} mohaaDecals_t;

/* A worldspawn key's value, or "". */
static void R_MOHAAWorldspawnValue( const char *entities, int length, const char *want, char *out, int size ) {
	char *text;
	const char *p;
	int depth = 0;

	out[0] = '\0';
	if ( !entities || length <= 0 ) {
		return;
	}
	text = ri.Malloc( length + 1 );
	Com_Memcpy( text, entities, length );
	text[length] = '\0';
	p = text;
	for ( ;; ) {
		char key[MAX_TOKEN_CHARS];
		const char *token = COM_ParseExt( &p, qtrue );
		if ( !token[0] || token[0] == '}' ) {
			break;
		}
		if ( token[0] == '{' ) {
			if ( ++depth > 1 ) {
				break;
			}
			continue;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_ParseExt( &p, qtrue );
		if ( !Q_stricmp( key, want ) ) {
			Q_strncpyz( out, token, size );
			break;
		}
	}
	ri.Free( text );
}

static void R_MOHAADecalPoint( const mohaaDecal_t *decal, int i, vec3_t out ) {
	const byte *p = decal->points + i * MOHAA_DCL_VERT_BYTES;
	out[0] = MOHAA_DCLFloat( p );
	out[1] = MOHAA_DCLFloat( p + 4 );
	out[2] = MOHAA_DCLFloat( p + 8 );
}

/* Every point inside the world's reach, none of them NaN. */
static qboolean R_MOHAADecalValid( const mohaaDecal_t *decal ) {
	int i, k;
	for ( i = 0; i < decal->numPoints; i++ ) {
		const byte *p = decal->points + i * MOHAA_DCL_VERT_BYTES;
		for ( k = 0; k < 5; k++ ) {
			const float value = MOHAA_DCLFloat( p + k * 4 );
			if ( !( fabs( value ) < 262144.0f ) ) {
				return qfalse;
			}
		}
	}
	return qtrue;
}

/* The side the points wind clockwise around, which faces away from the
 * surface under them; false for points on one line. */
static qboolean R_MOHAADecalNormal( mohaaDecal_t *decal ) {
	vec3_t first, a, b, e1, e2;
	int i;

	R_MOHAADecalPoint( decal, 0, first );
	for ( i = 1; i + 1 < decal->numPoints; i++ ) {
		R_MOHAADecalPoint( decal, i, a );
		R_MOHAADecalPoint( decal, i + 1, b );
		VectorSubtract( b, first, e1 );
		VectorSubtract( a, first, e2 );
		CrossProduct( e1, e2, decal->normal );
		if ( VectorNormalize( decal->normal ) > 0.0001f ) {
			return qtrue;
		}
	}
	return qfalse;
}

/* The leafs a fragment is drawn from, none of them solid. */
static int R_MOHAADecalLeafs( const mohaaGrid_t *tree, const dleaf_t *leafs, const mohaaDecal_t *decal,
	int out[MOHAA_DCL_MAX_FRAGMENT_VERTS + 1] ) {
	vec3_t point, centre;
	int count = 0, i, j;

	VectorClear( centre );
	for ( i = 0; i <= decal->numPoints; i++ ) {
		int leaf;
		if ( i < decal->numPoints ) {
			R_MOHAADecalPoint( decal, i, point );
			VectorAdd( point, decal->normal, point );
			VectorAdd( centre, point, centre );
		} else {
			VectorScale( centre, 1.0f / decal->numPoints, point );
		}
		leaf = R_MOHAAPointLeaf( tree, point );
		if ( leaf < 0 || LittleLong( leafs[leaf].cluster ) < 0 ) {
			continue;
		}
		for ( j = 0; j < count; j++ ) {
			if ( out[j] == leaf ) {
				break;
			}
		}
		if ( j == count ) {
			out[count++] = leaf;
		}
	}
	return count;
}

static void R_MOHAAFreeDecals( mohaaDecals_t *decals ) {
	if ( decals->leafDecals ) {
		ri.Free( decals->leafDecals );
	}
	if ( decals->leafFirst ) {
		ri.Free( decals->leafFirst );
	}
	if ( decals->fragments ) {
		ri.Free( decals->fragments );
	}
	if ( decals->file ) {
		ri.FS_FreeFile( decals->file );
	}
	Com_Memset( decals, 0, sizeof( *decals ) );
}

/*
==================
R_MOHAALoadDecals

The fragments of the map's decal file that will be built, and the leafs
that list each of them.
==================
*/
static void R_MOHAALoadDecals( const dheader_t *in, const char *mapName, mohaaDecals_t *decals ) {
	char name[MAX_QPATH];
	dclLayoutMOHAA_t layout;
	mohaaGrid_t tree;
	const dleaf_t *leafs;
	int leafList[MOHAA_DCL_MAX_FRAGMENT_VERTS + 1];
	int *fill;
	void *data = NULL;
	int length, numLeafs, total, i, j, offset, leftOut = 0, unseen = 0;

	Com_Memset( decals, 0, sizeof( *decals ) );
	if ( !mapName || !mapName[0] || ri.Cvar_VariableIntegerValue( "dcl_editmode" ) ) {
		return;
	}
	COM_StripExtension( mapName, name, sizeof( name ) );
	Q_strcat( name, sizeof( name ), ".dcl" );
	length = ri.FS_ReadFile( name, &data );
	if ( length <= 0 || !data ) {
		return;
	}
	if ( !MOHAA_DCLLayout( (const byte *)data, length, &layout ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: %s is not a decal file this engine reads\n", name );
		ri.FS_FreeFile( data );
		return;
	}
	if ( layout.mapTime >= 0 ) {
		char fileTime[MOHAA_DCL_MAP_TIME_BYTES], mapTime[MOHAA_DCL_MAP_TIME_BYTES];
		Q_strncpyz( fileTime, (const char *)data + layout.mapTime, sizeof( fileTime ) );
		R_MOHAAWorldspawnValue( (const char *)R_MOHAALumpData( in, LUMP_ENTITIES ),
			LittleLong( in->lumps[LUMP_ENTITIES].filelen ), "map_time", mapTime, sizeof( mapTime ) );
		if ( !mapTime[0] || strcmp( fileTime, mapTime ) ) {
			ri.Printf( PRINT_WARNING, "WARNING: %s was cut against another compile of its map; "
				"its decals are not drawn\n", name );
			ri.FS_FreeFile( data );
			return;
		}
	}

	numLeafs = R_MOHAALumpCount( in, LUMP_LEAFS, sizeof( dleaf_t ) );
	leafs = (const dleaf_t *)R_MOHAALumpData( in, LUMP_LEAFS );
	Com_Memset( &tree, 0, sizeof( tree ) );
	tree.nodes = (const dnode_t *)R_MOHAALumpData( in, LUMP_NODES );
	tree.numNodes = R_MOHAALumpCount( in, LUMP_NODES, sizeof( dnode_t ) );
	tree.planes = (const dplane_t *)R_MOHAALumpData( in, LUMP_PLANES );
	tree.numPlanes = R_MOHAALumpCount( in, LUMP_PLANES, sizeof( dplane_t ) );
	tree.numLeafs = numLeafs;
	if ( numLeafs <= 0 || tree.numNodes <= 0 ) {
		ri.FS_FreeFile( data );
		return;
	}

	decals->file = data;
	decals->fragments = ri.Malloc( sizeof( mohaaDecal_t ) * ( layout.numFragments > 0 ? layout.numFragments : 1 ) );
	decals->leafFirst = ri.Malloc( sizeof( int ) * ( numLeafs + 1 ) );
	Com_Memset( decals->leafFirst, 0, sizeof( int ) * ( numLeafs + 1 ) );

	/* the fragments that build, and how many each leaf lists */
	offset = layout.fragments;
	for ( i = 0; i < layout.numFragments; i++ ) {
		const byte *record = (const byte *)data + offset;
		mohaaDecal_t *decal = &decals->fragments[decals->numFragments];
		const int index = MOHAA_DCLInt( record + MOHAA_DCL_FRAGMENT_INDEX );
		int count;

		decal->numPoints = MOHAA_DCLInt( record + MOHAA_DCL_FRAGMENT_VERTS );
		decal->points = record + MOHAA_DCL_FRAGMENT_BYTES;
		offset += MOHAA_DCL_FRAGMENT_BYTES + decal->numPoints * MOHAA_DCL_VERT_BYTES;
		if ( index < 0 || decal->numPoints < 3 || !R_MOHAADecalValid( decal ) || !R_MOHAADecalNormal( decal ) ) {
			leftOut++;
			continue;
		}
		MOHAA_DCLShader( record, decal->shader, sizeof( decal->shader ) );
		count = R_MOHAADecalLeafs( &tree, leafs, decal, leafList );
		if ( !count ) {
			unseen++;
			continue;
		}
		for ( j = 0; j < count; j++ ) {
			decals->leafFirst[leafList[j]]++;
		}
		decals->numVerts += decal->numPoints;
		decals->numIndexes += ( decal->numPoints - 2 ) * 3;
		decals->numFragments++;
	}

	/* each leaf's list in turn */
	total = 0;
	for ( i = 0; i < numLeafs; i++ ) {
		const int count = decals->leafFirst[i];
		decals->leafFirst[i] = total;
		total += count;
	}
	decals->leafFirst[numLeafs] = total;
	decals->leafDecals = ri.Malloc( sizeof( int ) * ( total > 0 ? total : 1 ) );
	fill = ri.Malloc( sizeof( int ) * numLeafs );
	Com_Memcpy( fill, decals->leafFirst, sizeof( int ) * numLeafs );
	for ( i = 0; i < decals->numFragments; i++ ) {
		const int count = R_MOHAADecalLeafs( &tree, leafs, &decals->fragments[i], leafList );
		for ( j = 0; j < count; j++ ) {
			decals->leafDecals[fill[leafList[j]]++] = i;
		}
	}
	ri.Free( fill );

	ri.Printf( PRINT_DEVELOPER, "%s: %i of %i decal fragments built in %i leaf places, %i left out, %i in no open leaf\n",
		name, decals->numFragments, layout.numFragments, total, leftOut, unseen );
}

static void R_MOHAAInitSurface( dsurface_t *surface, int shaderNum, int lightmapNum, int firstVert, int numVerts,
	int firstIndex, int numIndexes ) {
	Com_Memset( surface, 0, sizeof( *surface ) );
	surface->shaderNum = LittleLong( shaderNum );
	surface->fogNum = LittleLong( -1 );
	surface->surfaceType = LittleLong( MST_TRIANGLE_SOUP );
	surface->firstVert = LittleLong( firstVert );
	surface->numVerts = LittleLong( numVerts );
	surface->firstIndex = LittleLong( firstIndex );
	surface->numIndexes = LittleLong( numIndexes );
	surface->lightmapNum = LittleLong( lightmapNum );
}

qboolean R_MOHAABuildWorld( const mohaa_translation_t *translation, const char *mapName, void **data, int *length,
	mohaaBuiltSurfaces_t *built ) {
	const mohaaRenderImport_t *engine = ri.MOHAARender;
	const dheader_t *in;
	dheader_t *header;
	staticModelBuild_t *models = NULL;
	shaderName_t *staticShaders = NULL;
	mohaaDecals_t decals;
	int numStaticShaders = 0, maxStaticShaders;
	int numShaders, numVerts, numIndexes, numSurfaces, numLeafSurfaces, numLeafs;
	int addVerts = 0, addIndexes = 0, addSurfaces = 0, addLeafSurfaces = 0;
	int terrainFirstSurface, decalFirstSurface = 0, terrainTriangles = 0, i, j, k;
	int offsets[HEADER_LUMPS], lengths[HEADER_LUMPS];
	int total;
	byte *out;
	float *pose = NULL;
	int poseFloats = 0;
	mohaaGrid_t grid;
	char gridText[64];
	float gridSize = 0.0f;
	int gridBounds[3] = { 0, 0, 0 }, gridPoints = 0;
	vec3_t gridOrigin;

	Com_Memset( &grid, 0, sizeof( grid ) );
	Com_Memset( &decals, 0, sizeof( decals ) );
	VectorClear( gridOrigin );
	*data = NULL;
	*length = 0;
	Com_Memset( built, 0, sizeof( *built ) );
	if ( !translation || !translation->data ) {
		return qfalse;
	}

	in = (const dheader_t *)translation->data;
	/* The leafs' own lists are copied into a list the size of the map's, so
	 * together they must fit it; a damaged map's may overlap. That is known
	 * before anything is allocated. */
	{
		const dleaf_t *leafs = (const dleaf_t *)R_MOHAALumpData( in, LUMP_LEAFS );
		const int leafCount = R_MOHAALumpCount( in, LUMP_LEAFS, sizeof( dleaf_t ) );
		const int listLength = R_MOHAALumpCount( in, LUMP_LEAFSURFACES, sizeof( int ) );
		int listed = 0, lo, hi;

		for ( i = 0; i < leafCount; i++ ) {
			const int part = R_MOHAALeafSurfacesInside( &leafs[i], listLength, &lo, &hi );
			if ( part > listLength - listed ) {
				ri.Printf( PRINT_WARNING, "WARNING: %s's leafs list more surfaces than it has; its terrain, "
					"static models and decals are not built\n", mapName ? mapName : "the map" );
				return qfalse;
			}
			listed += part;
		}
	}
	R_MOHAALoadDecals( in, mapName, &decals );
	if ( translation->numTerrain <= 0 && translation->numStaticModels <= 0 && !translation->lightGridPalette
		&& decals.numFragments <= 0 ) {
		R_MOHAAFreeDecals( &decals );
		return qfalse;
	}

	numShaders = R_MOHAALumpCount( in, LUMP_SHADERS, sizeof( dshader_t ) );
	numVerts = R_MOHAALumpCount( in, LUMP_DRAWVERTS, sizeof( drawVert_t ) );
	numIndexes = R_MOHAALumpCount( in, LUMP_DRAWINDEXES, sizeof( int ) );
	numSurfaces = R_MOHAALumpCount( in, LUMP_SURFACES, sizeof( dsurface_t ) );
	numLeafSurfaces = R_MOHAALumpCount( in, LUMP_LEAFSURFACES, sizeof( int ) );
	numLeafs = R_MOHAALumpCount( in, LUMP_LEAFS, sizeof( dleaf_t ) );

	/* terrain: one surface a patch */
	terrainFirstSurface = numSurfaces;
	addSurfaces += translation->numTerrain;
	addVerts += translation->numTerrain * TERRAIN_VERTS;
	addIndexes += translation->numTerrain * TERRAIN_INDEXES;

	/* static models: each surface of each definition that loads */
	maxStaticShaders = 1 + decals.numFragments;
	if ( translation->numStaticModels > 0 && engine ) {
		models = ri.Malloc( sizeof( *models ) * translation->numStaticModels );
		Com_Memset( models, 0, sizeof( *models ) * translation->numStaticModels );
		for ( i = 0; i < translation->numStaticModels; i++ ) {
			const dstaticModelMOHAA_t *sm = &translation->staticModels[i];
			char path[sizeof( sm->model ) + 1];
			int count;

			Com_Memcpy( path, sm->model, sizeof( sm->model ) );
			path[sizeof( sm->model )] = '\0';
			models[i].tiki = engine->RegisterStatic( path );
			if ( !models[i].tiki ) {
				ri.Printf( PRINT_WARNING, "WARNING: static model %s did not load\n", path );
				continue;
			}
			count = engine->NumSurfaces( models[i].tiki );
			models[i].firstSurface = numSurfaces + addSurfaces;
			for ( j = 0; j < count; j++ ) {
				mohaaTikiSurface_t info;
				if ( !engine->Surface( models[i].tiki, j, &info ) ) {
					continue;
				}
				addVerts += info.numVerts;
				addIndexes += info.numIndexes;
			}
			/* every surface keeps its slot, so a leaf lists them by range */
			models[i].numSurfaces = count;
			addSurfaces += count;
			maxStaticShaders += count;
			if ( engine->PoseFloats( models[i].tiki ) > poseFloats ) {
				poseFloats = engine->PoseFloats( models[i].tiki );
			}
		}
	}
	staticShaders = ri.Malloc( sizeof( *staticShaders ) * maxStaticShaders );

	/* permanent decals: a surface a fragment, after the static models' */
	addSurfaces += decals.numFragments;
	addVerts += decals.numVerts;
	addIndexes += decals.numIndexes;
	addLeafSurfaces += decals.leafFirst ? decals.leafFirst[numLeafs] : 0;

	/* each leaf's own surfaces, then its terrain, then its static models',
	 * then its decals */
	for ( i = 0; i < numLeafs; i++ ) {
		const int terrain = translation->leafTerrain ? translation->leafTerrain[i * 2 + 1] : 0;
		const int statics = translation->leafStaticModels ? translation->leafStaticModels[i * 2 + 1] : 0;
		const int firstStatic = translation->leafStaticModels ? translation->leafStaticModels[i * 2] : 0;
		addLeafSurfaces += terrain > 0 ? terrain : 0;
		for ( j = 0; j < statics && models; j++ ) {
			const int index = firstStatic + j;
			if ( index >= 0 && index < translation->numStaticModelIndexes ) {
				const int model = LittleShort( (short)translation->staticModelIndexes[index] ) & 0xffff;
				if ( model < translation->numStaticModels ) {
					addLeafSurfaces += models[model].numSurfaces;
				}
			}
		}
	}

	/* the new layout: every lump as it was, with the four that grow grown */
	Com_Memset( offsets, 0, sizeof( offsets ) );
	Com_Memset( lengths, 0, sizeof( lengths ) );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		lengths[i] = LittleLong( in->lumps[i].filelen );
	}
	lengths[LUMP_SHADERS] += maxStaticShaders * (int)sizeof( dshader_t );
	lengths[LUMP_DRAWVERTS] += addVerts * (int)sizeof( drawVert_t );
	lengths[LUMP_DRAWINDEXES] += addIndexes * (int)sizeof( int );
	lengths[LUMP_SURFACES] += addSurfaces * (int)sizeof( dsurface_t );
	lengths[LUMP_LEAFSURFACES] += addLeafSurfaces * (int)sizeof( int );

	/* the light grid, and the worldspawn key that sizes it */
	gridText[0] = '\0';
	if ( translation->lightGridPalette && R_MOHAALumpCount( in, LUMP_MODELS, sizeof( dmodel_t ) ) > 0 ) {
		const dmodel_t *world = (const dmodel_t *)R_MOHAALumpData( in, LUMP_MODELS );
		vec3_t mins, maxs, maxsGrid;
		for ( i = 0; i < 3; i++ ) {
			mins[i] = LittleFloat( world->mins[i] );
			maxs[i] = LittleFloat( world->maxs[i] );
			grid.origin[i] = MOHAA_GRID_CELL * ceil( mins[i] / MOHAA_GRID_CELL );
			maxsGrid[i] = MOHAA_GRID_CELL * floor( maxs[i] / MOHAA_GRID_CELL );
			grid.bounds[i] = (int)( ( maxsGrid[i] - grid.origin[i] ) / MOHAA_GRID_CELL + 1 );
		}
		/* the offsets must be the grid's: its width, and a column per
		 * (x, y) after them */
		if ( grid.bounds[0] > 1 && grid.bounds[1] > 1 && grid.bounds[2] > 1
			&& translation->numLightGridOffsets == grid.bounds[0] * grid.bounds[1] + grid.bounds[0] ) {
			gridSize = R_MOHAAGridSpacing( mins, maxs, gridBounds, gridOrigin );
			gridPoints = gridBounds[0] * gridBounds[1] * gridBounds[2];
			Com_sprintf( gridText, sizeof( gridText ), "\"gridsize\" \"%i %i %i\"\n",
				(int)gridSize, (int)gridSize, (int)gridSize );
			lengths[LUMP_LIGHTGRID] = gridPoints * 8;
			lengths[LUMP_ENTITIES] += (int)strlen( gridText );
		} else {
			ri.Printf( PRINT_WARNING, "WARNING: light grid of %i offsets does not fit its %i by %i world\n",
				translation->numLightGridOffsets, grid.bounds[0], grid.bounds[1] );
		}
	}
	total = (int)sizeof( dheader_t );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		total = ( total + 3 ) & ~3;
		offsets[i] = total;
		total += lengths[i];
	}

	out = ri.Malloc( total );
	Com_Memset( out, 0, total );
	header = (dheader_t *)out;
	header->ident = LittleLong( BSP_IDENT );
	header->version = LittleLong( BSP_VERSION );
	for ( i = 0; i < HEADER_LUMPS; i++ ) {
		header->lumps[i].fileofs = LittleLong( offsets[i] );
		header->lumps[i].filelen = LittleLong( lengths[i] );
		if ( i != LUMP_LEAFSURFACES && !( i == LUMP_ENTITIES && gridText[0] ) ) {
			Com_Memcpy( out + offsets[i], R_MOHAALumpData( in, i ), LittleLong( in->lumps[i].filelen ) );
		}
	}

	if ( gridText[0] ) {
		/* the spacing goes last in the worldspawn, so no key of the map's
		 * own can override it */
		const char *text = (const char *)R_MOHAALumpData( in, LUMP_ENTITIES );
		const int textLength = LittleLong( in->lumps[LUMP_ENTITIES].filelen );
		const int addLength = (int)strlen( gridText );
		int split = 0;
		while ( split < textLength && text[split] != '}' ) {
			split++;
		}
		Com_Memcpy( out + offsets[LUMP_ENTITIES], text, split );
		Com_Memcpy( out + offsets[LUMP_ENTITIES] + split, gridText, addLength );
		Com_Memcpy( out + offsets[LUMP_ENTITIES] + split + addLength, text + split, textLength - split );

		/* the grid itself */
		grid.translation = translation;
		grid.nodes = (const dnode_t *)R_MOHAALumpData( in, LUMP_NODES );
		grid.numNodes = R_MOHAALumpCount( in, LUMP_NODES, sizeof( dnode_t ) );
		grid.planes = (const dplane_t *)R_MOHAALumpData( in, LUMP_PLANES );
		grid.numPlanes = R_MOHAALumpCount( in, LUMP_PLANES, sizeof( dplane_t ) );
		grid.numLeafs = numLeafs;
		R_MOHAAGridWorldspawn( text, textLength, &grid );
		if ( numLeafs > 0 && translation->numSphereLightVis > 0 ) {
			/* each leaf's list in turn, each ending in -1 */
			int cursor = 0;
			grid.leafLights = ri.Malloc( sizeof( int ) * 2 * numLeafs );
			for ( i = 0; i < numLeafs; i++ ) {
				const int first = cursor;
				while ( cursor < translation->numSphereLightVis && LittleLong( translation->sphereLightVis[cursor] ) != -1 ) {
					cursor++;
				}
				grid.leafLights[i * 2] = first;
				grid.leafLights[i * 2 + 1] = cursor - first;
				if ( cursor < translation->numSphereLightVis ) {
					cursor++;
				}
			}
		}
		{
			byte *points = out + offsets[LUMP_LIGHTGRID];
			int x, y, z;
			for ( z = 0; z < gridBounds[2]; z++ ) {
				for ( y = 0; y < gridBounds[1]; y++ ) {
					for ( x = 0; x < gridBounds[0]; x++ ) {
						vec3_t point;
						point[0] = gridOrigin[0] + x * gridSize;
						point[1] = gridOrigin[1] + y * gridSize;
						point[2] = gridOrigin[2] + z * gridSize;
						R_MOHAAGridPoint( &grid, point, points );
						points += 8;
					}
				}
			}
		}
		if ( grid.leafLights ) {
			ri.Free( grid.leafLights );
		}
		ri.Printf( PRINT_DEVELOPER, "light grid: %i by %i by %i points %i units apart, %i sphere lights\n",
			gridBounds[0], gridBounds[1], gridBounds[2], (int)gridSize, translation->numSphereLights );
	}

	{
		drawVert_t *verts = (drawVert_t *)( out + offsets[LUMP_DRAWVERTS] );
		int *indexes = (int *)( out + offsets[LUMP_DRAWINDEXES] );
		dsurface_t *surfaces = (dsurface_t *)( out + offsets[LUMP_SURFACES] );
		dshader_t *shaders = (dshader_t *)( out + offsets[LUMP_SHADERS] );
		int vertCursor = numVerts, indexCursor = numIndexes, surfaceCursor = numSurfaces;

		/* terrain */
		if ( translation->numTerrain > 0 ) {
			mohaaTerrainSquareTable_t squares;
			MOHAA_BuildTerrainSquareTable( &squares );
			for ( i = 0; i < translation->numTerrain; i++ ) {
				const dterrainPatchMOHAA_t *patch = &translation->terrain[i];
				int shaderNum = LittleShort( (short)patch->iShader ) & 0xffff;
				int count;
				if ( shaderNum >= numShaders ) {
					shaderNum = 0;
				}
				count = R_MOHAABuildTerrain( patch, &squares, verts + vertCursor, indexes + indexCursor );
				R_MOHAAInitSurface( &surfaces[surfaceCursor++], shaderNum, LittleShort( (short)patch->iLightMap ) & 0xffff,
					vertCursor, TERRAIN_VERTS, indexCursor, count );
				vertCursor += TERRAIN_VERTS;
				indexCursor += count;
				terrainTriangles += count / 3;
			}
			ri.Printf( PRINT_DEVELOPER, "terrain: %i of %i triangles in %i patches, the rest taken out\n",
				terrainTriangles, translation->numTerrain * ( TERRAIN_INDEXES / 3 ), translation->numTerrain );
		}

		/* static models */
		if ( models && poseFloats > 0 ) {
			pose = ri.Malloc( sizeof( float ) * poseFloats );
		}
		if ( models && !pose ) {
			for ( i = 0; i < translation->numStaticModels; i++ ) {
				models[i].numSurfaces = 0;
			}
		}
		for ( i = 0; models && pose && i < translation->numStaticModels; i++ ) {
			const dstaticModelMOHAA_t *sm = &translation->staticModels[i];
			const int colorFirst = LittleLong( sm->firstVertexData );
			const int colorCount = LittleLong( sm->numVertexData );
			vec3_t origin, angles, axis[3];
			float scale;
			int colorCursor = 0;

			if ( !models[i].tiki ) {
				models[i].numSurfaces = 0;
				continue;
			}
			for ( k = 0; k < 3; k++ ) {
				origin[k] = LittleFloat( sm->origin[k] );
				angles[k] = LittleFloat( sm->angles[k] );
			}
			scale = LittleFloat( sm->scale );
			if ( scale <= 0.0f ) {
				scale = 1.0f;
			}
			AnglesToAxis( angles, axis );
			if ( !engine->StaticPose( models[i].tiki, pose ) ) {
				models[i].numSurfaces = 0;
				continue;
			}

			/* the surfaces that skin keep their places, the rest none */
			const int surfacesInModel = models[i].numSurfaces;
			models[i].firstSurface = surfaceCursor;
			models[i].numSurfaces = 0;
			for ( j = 0; j < surfacesInModel; j++ ) {
				dsurface_t *surface;
				mohaaTikiSurface_t info;
				static float xyz[SHADER_MAX_VERTEXES][4], normal[SHADER_MAX_VERTEXES][4], st[SHADER_MAX_VERTEXES][2];
				static unsigned int tri[SHADER_MAX_INDEXES];
				int written = 0, count = 0, v, shaderNum;

				if ( !engine->Surface( models[i].tiki, j, &info ) ) {
					continue;
				}
				written = engine->Skin( models[i].tiki, j, pose, scale, -1.0f, 0, xyz, normal, st, tri, 0,
					SHADER_MAX_VERTEXES, SHADER_MAX_INDEXES, &count );
				if ( written <= 0 || written != info.numVerts || count <= 0 ) {
					colorCursor += info.numVerts;
					continue;
				}
				if ( info.numSkins <= 0 || !info.shaders[0] || !info.shaders[0][0] ) {
					ri.Printf( PRINT_DEVELOPER, "static model %s surface %s has no shader\n",
						sm->model, info.name ? info.name : "?" );
				}
				shaderNum = R_MOHAAStaticShader( staticShaders, &numStaticShaders, maxStaticShaders, numShaders,
					info.numSkins > 0 ? info.shaders[0] : NULL );

				for ( v = 0; v < written; v++ ) {
					drawVert_t *dv = &verts[vertCursor + v];
					vec3_t local, n;
					VectorCopy( xyz[v], local );
					VectorCopy( origin, dv->xyz );
					VectorMA( dv->xyz, local[0], axis[0], dv->xyz );
					VectorMA( dv->xyz, local[1], axis[1], dv->xyz );
					VectorMA( dv->xyz, local[2], axis[2], dv->xyz );
					VectorScale( axis[0], normal[v][0], n );
					VectorMA( n, normal[v][1], axis[1], n );
					VectorMA( n, normal[v][2], axis[2], n );
					VectorNormalize( n );
					VectorCopy( n, dv->normal );
					dv->st[0] = st[v][0];
					dv->st[1] = st[v][1];
					dv->lightmap[0] = dv->lightmap[1] = 0.0f;
					if ( colorCount > 0 && colorCursor + v < colorCount
						&& colorFirst + ( colorCursor + v ) * 3 + 2 < translation->staticModelDataLength ) {
						const byte *c = translation->staticModelData + colorFirst + ( colorCursor + v ) * 3;
						dv->color.rgba[0] = c[0];
						dv->color.rgba[1] = c[1];
						dv->color.rgba[2] = c[2];
					} else {
						dv->color.rgba[0] = dv->color.rgba[1] = dv->color.rgba[2] = 255;
					}
					dv->color.rgba[3] = 255;
				}
				for ( v = 0; v < count; v++ ) {
					indexes[indexCursor + v] = (int)tri[v];
				}
				surface = &surfaces[surfaceCursor++];
				models[i].numSurfaces++;
				R_MOHAAInitSurface( surface, shaderNum, MOHAA_LIGHTMAP_BY_VERTEX, vertCursor, written, indexCursor, count );
				vertCursor += written;
				indexCursor += count;
				colorCursor += info.numVerts;
			}
		}

		/* permanent decals: a fan each, its points' colours kept as the
		 * file has them */
		decalFirstSurface = surfaceCursor;
		for ( i = 0; i < decals.numFragments; i++ ) {
			const mohaaDecal_t *decal = &decals.fragments[i];
			const int shaderNum = R_MOHAAStaticShader( staticShaders, &numStaticShaders, maxStaticShaders,
				numShaders, decal->shader );
			const int count = ( decal->numPoints - 2 ) * 3;
			int v;

			for ( v = 0; v < decal->numPoints; v++ ) {
				drawVert_t *dv = &verts[vertCursor + v];
				const byte *p = decal->points + v * MOHAA_DCL_VERT_BYTES;
				R_MOHAADecalPoint( decal, v, dv->xyz );
				dv->st[0] = MOHAA_DCLFloat( p + 12 );
				dv->st[1] = MOHAA_DCLFloat( p + 16 );
				dv->lightmap[0] = dv->lightmap[1] = 0.0f;
				VectorCopy( decal->normal, dv->normal );
				for ( k = 0; k < 4; k++ ) {
					dv->color.rgba[k] = p[20 + k];
				}
			}
			for ( v = 1; v + 1 < decal->numPoints; v++ ) {
				indexes[indexCursor + ( v - 1 ) * 3 + 0] = 0;
				indexes[indexCursor + ( v - 1 ) * 3 + 1] = v;
				indexes[indexCursor + ( v - 1 ) * 3 + 2] = v + 1;
			}
			R_MOHAAInitSurface( &surfaces[surfaceCursor++], shaderNum, MOHAA_LIGHTMAP_BY_VERTEX, vertCursor,
				decal->numPoints, indexCursor, count );
			vertCursor += decal->numPoints;
			indexCursor += count;
		}

		/* the appended shaders */
		for ( i = 0; i < numStaticShaders; i++ ) {
			dshader_t *shader = &shaders[numShaders + i];
			Q_strncpyz( shader->shader, staticShaders[i].name, sizeof( shader->shader ) );
			shader->surfaceFlags = 0;
			shader->contentFlags = 0;
		}
		header->lumps[LUMP_SHADERS].filelen = LittleLong( ( numShaders + numStaticShaders ) * (int)sizeof( dshader_t ) );
		header->lumps[LUMP_DRAWVERTS].filelen = LittleLong( vertCursor * (int)sizeof( drawVert_t ) );
		header->lumps[LUMP_DRAWINDEXES].filelen = LittleLong( indexCursor * (int)sizeof( int ) );
		header->lumps[LUMP_SURFACES].filelen = LittleLong( surfaceCursor * (int)sizeof( dsurface_t ) );
	}

	/* the leafs' surface lists */
	{
		const int *oldLeafSurfaces = (const int *)R_MOHAALumpData( in, LUMP_LEAFSURFACES );
		int *leafSurfaces = (int *)( out + offsets[LUMP_LEAFSURFACES] );
		dleaf_t *leafs = (dleaf_t *)( out + offsets[LUMP_LEAFS] );
		int cursor = 0;

		for ( i = 0; i < numLeafs; i++ ) {
			dleaf_t *leaf = &leafs[i];
			const int start = cursor;
			int lo, hi;

			/* the part of its own list inside the map's, which fits (above) */
			R_MOHAALeafSurfacesInside( leaf, numLeafSurfaces, &lo, &hi );
			for ( j = lo; j < hi; j++ ) {
				leafSurfaces[cursor++] = oldLeafSurfaces[j];
			}
			if ( translation->leafTerrain ) {
				const int firstTerrain = translation->leafTerrain[i * 2];
				const int terrain = translation->leafTerrain[i * 2 + 1];
				for ( j = 0; j < terrain; j++ ) {
					const int index = firstTerrain + j;
					if ( index >= 0 && index < translation->numTerrainIndexes ) {
						const int patch = LittleShort( translation->terrainIndexes[index] );
						if ( patch >= 0 && patch < translation->numTerrain ) {
							leafSurfaces[cursor++] = LittleLong( terrainFirstSurface + patch );
						}
					}
				}
			}
			if ( translation->leafStaticModels && models ) {
				const int firstStatic = translation->leafStaticModels[i * 2];
				const int statics = translation->leafStaticModels[i * 2 + 1];
				for ( j = 0; j < statics; j++ ) {
					const int index = firstStatic + j;
					if ( index >= 0 && index < translation->numStaticModelIndexes ) {
						const int model = LittleShort( (short)translation->staticModelIndexes[index] ) & 0xffff;
						if ( model < translation->numStaticModels && models[model].tiki ) {
							for ( k = 0; k < models[model].numSurfaces; k++ ) {
								leafSurfaces[cursor++] = LittleLong( models[model].firstSurface + k );
							}
						}
					}
				}
			}
			if ( decals.leafFirst ) {
				for ( j = decals.leafFirst[i]; j < decals.leafFirst[i + 1]; j++ ) {
					leafSurfaces[cursor++] = LittleLong( decalFirstSurface + decals.leafDecals[j] );
				}
			}
			leaf->firstLeafSurface = LittleLong( start );
			leaf->numLeafSurfaces = LittleLong( cursor - start );
		}
		header->lumps[LUMP_LEAFSURFACES].filelen = LittleLong( cursor * (int)sizeof( int ) );
	}

	if ( pose ) {
		ri.Free( pose );
	}
	if ( models ) {
		ri.Free( models );
	}
	ri.Free( staticShaders );
	built->firstTerrain = terrainFirstSurface;
	built->numTerrain = translation->numTerrain;
	built->firstDecal = decalFirstSurface;
	built->numDecals = decals.numFragments;
	R_MOHAAFreeDecals( &decals );
	*data = out;
	*length = total;
	return qtrue;
}
