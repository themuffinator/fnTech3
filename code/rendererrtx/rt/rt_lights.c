/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Path tracer lights

Quake III maps keep the light entities q3map lit them with, and shaders keep
their q3map_surfacelight and q3map_sun values. The path tracer lights a map
at run time from those same sources, with the compiler's own models (the
light.c of id's q3map source release):

- a point light of intensity I delivers I * 7500 * cos / d^2, with d clamped
  to 16; a linear light (spawnflags 1) delivers cos * I * 7500 / 8000 - d;
  a light with a target is a spotlight whose cone reaches the target's
  distance at radius + 16 and fades over its outer 32 units. q3map2 reads
  "_light" before "light" and multiplies it by "scale", and drops the cosine
  from linear lights and from any light with spawnflags 2. RTCW's and ET's
  compilers light the Wolfenstein way: a light is linear and has no cosine
  unless spawnflags 1 (inverse square, with the cosine), spawnflags 2 adds
  the cosine, and "fade" scales a linear light's distance term. A spotlight
  is inverse square with the cosine in every model. A light whose target
  names no entity stays a point light for id's q3map and q3map2, but RTCW's
  own compiler lit nothing with it: beside each such light on the retail
  maps that have them, the luxels hold a fraction of what it would add. A
  lightJunior lights only the light grid - models, not world surfaces - and
  a Wolfenstein or q3map2 light with spawnflags 16 (nogrid) only world
  surfaces;
- a surface-light polygon delivers its point-to-polygon form factor times
  value * 0.75, in the colour of its light image; an autosprite emitter is a
  point light of intensity value. A point light a little in front of an
  emitter carries a fraction of its photons (backsplash, RT_AddBacksplash):
  id's q3map only for polygons it did not subdivide, q3map2 for every piece
  but on sky surfaces;
- the sun delivers q3map_sun's colour * intensity * cos where the path to it
  reaches the sky: id's q3map takes it from the first sky surface's shader
  with its colour scaled to unit length, q3map2 from the first shader that
  has one with its largest colour component scaled to 1 (RT_FindSun), as
  Wolfenstein's own compiler scaled it too (renderercommon/
  tr_sky_sun_units.h), and
  q3map2's q3map_sunExt spreads it over its deviance in its samples;
- q3map2's q3map_skyLight spreads its value over a dome of equal suns, each
  lighting a point as the sun does (RT_FindSkyLight);
- worldspawn ambient adds _color * ambient everywhere. id's q3map reads
  "ambient" and a missing _color leaves it black; q3map2 reads "_ambient"
  first, defaults _color to white, and raises every luxel to its _minlight
  (models: _mingridlight). q3map2 marks its maps with _q3map2_version; a map
  from a build that did not is known by worldspawn keys or a shader
  directive only q3map2 reads.

q3map stored those sums, /255, in lightmaps that the renderer shifts by
r_mapOverBrightBits - overbrightBits. Scaling every light by that shift /255
puts traced irradiance in the lightmap's units, so a surface lit only by its
direct light matches the baked result; traced bounce light adds to it.

A uniform grid over the world lists, for each cell, the lights whose reach
touches it and whose cluster its clusters can see (the BSP PVS), strongest
first, so a shading point samples among a few dozen candidates.

Included by vk.c.
===========================================================================
*/

#define RT_LIGHT_POINT_SCALE		7500.0f		// q3map pointScale
#define RT_LIGHT_LINEAR_SCALE		( 1.0f / 8000.0f )
#define RT_LIGHT_AREA_SCALE			0.25f		// q3map areaScale
#define RT_LIGHT_FORM_FACTOR_SCALE	3.0f		// q3map formFactorValueScale
#define RT_LIGHT_BACKSPLASH_FRACTION	0.05f		// both compilers' default
#define RT_LIGHT_BACKSPLASH_DISTANCE	24.0f		// id's q3map; q3map2's is 23
#define RT_LIGHT_BACKSPLASH_DISTANCE_Q3MAP2	23.0f
#define RT_LIGHT_SUBDIVIDE			999.0f		// both compilers' default light subdivision
#define RT_LIGHT_MIN_DISTANCE		16.0f
#define RT_LIGHT_CUTOFF				1.0f		// q3map skips contributions of 1/255 or less
#define RT_LIGHT_CELL_SIZE			128.0f
#define RT_LIGHT_MAX_CELLS			( 64 * 64 * 64 )
#define RT_LIGHT_CELL_MAX			64
#define RT_LIGHT_MAX_ENTITIES		8192
#define RT_LIGHT_MAX_COORD			1048576.0f	// map data beyond this is malformed
#define RT_LIGHT_MAX_VALUE			1.0e7f		// intensities, colours, radii

typedef struct {
	char	classname[64];
	char	target[64];
	char	targetname[64];
	vec3_t	origin;
	vec3_t	color;
	float	light;
	float	lightUnderscore;	// "_light": q3map reads it when "light" is zero, q3map2 first
	float	scale;			// q3map2: multiplies the intensity
	float	fade;			// Wolfenstein: a linear light's distance multiplier
	float	radius;
	float	ambient;		// worldspawn: "ambient"
	float	ambientQ3map2;	// worldspawn, q3map2: "_ambient", read before "ambient"
	float	minLight;		// worldspawn, q3map2: "_minlight"
	float	minVertexLight;	// worldspawn, q3map2: "_minvertexlight"
	float	minGridLight;	// worldspawn, q3map2: "_mingridlight"
	int		spawnflags;
	qboolean hasColor;
	qboolean hasOrigin;
	qboolean hasAmbientQ3map2;
	qboolean hasMinLight;
	qboolean hasMinVertexLight;
	qboolean hasMinGridLight;
	qboolean q3map2;		// worldspawn carries keys only q3map2 writes or reads
	qboolean noShaderSun;	// worldspawn, q3map2: "_noshadersun 1"
} rtLightEntity_t;

typedef struct {
	rtGpuLight_t	*lights;
	float			*reach;			// per light: influence radius (world units)
	vec3_t			*center;		// per light: position or triangle centroid
	int				*cluster;		// per light: BSP cluster, -1 if unknown
	uint32_t		numLights;
	uint32_t		maxLights;

	vec3_t			gridOrigin;
	float			cellSize;
	int				dims[3];
	uint32_t		*cells;			// offset/count per cell, then CDF/probability per static light
	uint32_t		*indices;
	uint32_t		numIndices;
	uint32_t		overflowCells;
	uint64_t		overflowEntries;
	double			overflowImportance, candidateImportance;

	float			scale;			// q3map units to lightmap units
	vec3_t			ambient;		// lightmap units
	vec3_t			minLight;		// q3map2 floors, lightmap units: lightmapped surfaces,
	vec3_t			minVertexLight;	// vertex-lit surfaces
	vec3_t			minGridLight;	// and models
	qboolean		q3map2;			// the map was compiled by q3map2
	qboolean		wolf;			// the Wolfenstein lighting model (RTCW, ET)
	qboolean		noShaderSun;
	qboolean		q3map2Shaders;	// the map's shaders use what only q3map2 reads
	qboolean		sunValid;
	qboolean		entitySun;		// Alice's or F.A.K.K.2's, from the entity keys
	vec3_t			sunLight;		// q3map units: colour * intensity
	vec3_t			sunDirection;	// toward the sun
	float			sunDeviance;	// q3map_sunExt's spread, radians (0 for none)
	int				sunSamples;		// ... over this many samples
	vec3_t			skyLight;		// q3map_skyLight: colour * value, q3map units
	vec3_t			skyDiffuse;		// Medal of Honor's diffuse sun: an open floor, q3map units
	int				skyLightIterations;	// its dome's rows + 1 (0 for none)
	uint32_t		pointLights;
	uint32_t		triangleLights;
	uint32_t		untargetedLights;	// RTCW lights left dark: their target names no entity
	uint32_t		entityLights;		// the map's light entities, lit or not
	// q3map2 lit the map from light entities it then left out of the BSP
	// (it keeps them only with -keeplights): the lightmaps alone hold them
	qboolean		lightsStripped;
	int				mode;				// the rtx_rt_lighting the lights were built for
	vec4_t			skyColor;			// how the sky lights (RT_FindSkyLightColor)
	uint32_t		emissiveShaders;	// shaders that glow without q3map_surfacelight
	uint32_t		emissiveTriangles;	// ... and the triangle lights drawn with them
	uint32_t		generation;
} rtLightScene_t;

static rtLightScene_t s_rtLights;

static void RT_LightsFree( void )
{
	if ( s_rtLights.lights ) RT_HostFree( s_rtLights.lights );
	if ( s_rtLights.reach ) RT_HostFree( s_rtLights.reach );
	if ( s_rtLights.center ) RT_HostFree( s_rtLights.center );
	if ( s_rtLights.cluster ) RT_HostFree( s_rtLights.cluster );
	if ( s_rtLights.cells ) RT_HostFree( s_rtLights.cells );
	if ( s_rtLights.indices ) RT_HostFree( s_rtLights.indices );
	{
		const uint32_t generation = s_rtLights.generation;
		Com_Memset( &s_rtLights, 0, sizeof( s_rtLights ) );
		s_rtLights.generation = generation + 1;
	}
}

static qboolean RT_LightsReserve( uint32_t count )
{
	uint32_t capacity;
	rtGpuLight_t *lights;
	float *reach;
	vec3_t *center;
	int *cluster;

	if ( count <= s_rtLights.maxLights ) {
		return qtrue;
	}
	capacity = s_rtLights.maxLights ? s_rtLights.maxLights * 2 : 1024;
	while ( capacity < count ) {
		capacity *= 2;
	}
	if ( capacity > ( 1u << 20 ) ) {
		return qfalse;
	}
	lights = RT_HostAlloc( (int)( capacity * sizeof( *lights ) ) );
	reach = RT_HostAlloc( (int)( capacity * sizeof( *reach ) ) );
	center = RT_HostAlloc( (int)( capacity * sizeof( *center ) ) );
	cluster = RT_HostAlloc( (int)( capacity * sizeof( *cluster ) ) );
	if ( !lights || !reach || !center || !cluster ) {
		if ( lights ) RT_HostFree( lights );
		if ( reach ) RT_HostFree( reach );
		if ( center ) RT_HostFree( center );
		if ( cluster ) RT_HostFree( cluster );
		return qfalse;
	}
	if ( s_rtLights.numLights ) {
		Com_Memcpy( lights, s_rtLights.lights, s_rtLights.numLights * sizeof( *lights ) );
		Com_Memcpy( reach, s_rtLights.reach, s_rtLights.numLights * sizeof( *reach ) );
		Com_Memcpy( center, s_rtLights.center, s_rtLights.numLights * sizeof( *center ) );
		Com_Memcpy( cluster, s_rtLights.cluster, s_rtLights.numLights * sizeof( *cluster ) );
	}
	if ( s_rtLights.lights ) RT_HostFree( s_rtLights.lights );
	if ( s_rtLights.reach ) RT_HostFree( s_rtLights.reach );
	if ( s_rtLights.center ) RT_HostFree( s_rtLights.center );
	if ( s_rtLights.cluster ) RT_HostFree( s_rtLights.cluster );
	s_rtLights.lights = lights;
	s_rtLights.reach = reach;
	s_rtLights.center = center;
	s_rtLights.cluster = cluster;
	s_rtLights.maxLights = capacity;
	return qtrue;
}

/*
==================
Validation

Entity strings and BSP geometry are map data. A light reaches the grid and
the GPU only when every value it carries is finite and in range: one NaN
would place it in every grid cell and poison the denoiser's history. The
exponent test holds under -ffast-math, which may fold v == v away.
==================
*/

static qboolean RT_FloatValid( float v, float limit )
{
	floatint_t bits;

	bits.f = v;
	if ( ( ( bits.u >> 23 ) & 0xffu ) == 0xffu ) {
		return qfalse;	// infinity or NaN
	}
	return ( v >= -limit && v <= limit ) ? qtrue : qfalse;
}

static qboolean RT_Vec3Valid( const vec3_t v, float limit )
{
	return ( RT_FloatValid( v[0], limit ) && RT_FloatValid( v[1], limit ) &&
		RT_FloatValid( v[2], limit ) ) ? qtrue : qfalse;
}

/*
==================
BSP queries
==================
*/

static const mnode_t *RT_PointInLeaf( const vec3_t p )
{
	const mnode_t *node;

	if ( !tr.world || !tr.world->nodes ) {
		return NULL;
	}
	node = tr.world->nodes;
	while ( node->contents == -1 ) {
		const cplane_t *plane = node->plane;
		const float d = DotProduct( p, plane->normal ) - plane->dist;
		node = d > 0 ? node->children[0] : node->children[1];
	}
	return node;
}

static int RT_PointCluster( const vec3_t p )
{
	const mnode_t *leaf = RT_PointInLeaf( p );
	return leaf ? leaf->cluster : -1;
}

static qboolean RT_ClusterSees( int from, int to )
{
	const byte *vis;

	if ( from < 0 || to < 0 || !tr.world->vis || from >= tr.world->numClusters || to >= tr.world->numClusters ||
		( r_novis && r_novis->integer ) ) {
		return qtrue;
	}
	vis = tr.world->vis + from * tr.world->clusterBytes;
	return ( vis[ to >> 3 ] & ( 1 << ( to & 7 ) ) ) ? qtrue : qfalse;
}

/*
==================
Entities
==================
*/

static int RT_ParseLightEntities( rtLightEntity_t *entities, int maxEntities )
{
	const char *parse;
	int count = 0;

	if ( !tr.world || !tr.world->entityString ) {
		return 0;
	}
	parse = tr.world->entityString;
	while ( 1 ) {
		rtLightEntity_t ent;
		const char *token = COM_ParseExt( &parse, qtrue );

		if ( !token[0] ) {
			break;
		}
		if ( token[0] != '{' ) {
			break;
		}
		Com_Memset( &ent, 0, sizeof( ent ) );
		while ( 1 ) {
			char key[64];
			token = COM_ParseExt( &parse, qtrue );
			if ( !token[0] || token[0] == '}' ) {
				break;
			}
			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_ParseExt( &parse, qtrue );
			if ( !token[0] || token[0] == '}' ) {
				break;
			}
			// a repeated key keeps its last value, as it does for q3map (which
			// prepends each pair and reads the first match)
			if ( !Q_stricmp( key, "classname" ) ) {
				Q_strncpyz( ent.classname, token, sizeof( ent.classname ) );
			} else if ( !Q_stricmp( key, "origin" ) ) {
				ent.hasOrigin = ( sscanf( token, "%f %f %f", &ent.origin[0], &ent.origin[1], &ent.origin[2] ) == 3 &&
					RT_Vec3Valid( ent.origin, RT_LIGHT_MAX_COORD ) ) ? qtrue : qfalse;
			} else if ( !Q_stricmp( key, "_color" ) ) {
				ent.hasColor = ( sscanf( token, "%f %f %f", &ent.color[0], &ent.color[1], &ent.color[2] ) == 3 &&
					RT_Vec3Valid( ent.color, RT_LIGHT_MAX_VALUE ) ) ? qtrue : qfalse;
			} else if ( !Q_stricmp( key, "light" ) ) {
				ent.light = Q_atof( token );
			} else if ( !Q_stricmp( key, "_light" ) ) {
				ent.lightUnderscore = Q_atof( token );
			} else if ( !Q_stricmp( key, "scale" ) ) {
				ent.scale = Q_atof( token );
			} else if ( !Q_stricmp( key, "fade" ) ) {
				ent.fade = Q_atof( token );
			} else if ( !Q_stricmp( key, "ambient" ) ) {
				ent.ambient = Q_atof( token );
			} else if ( !Q_stricmp( key, "_ambient" ) ) {
				ent.ambientQ3map2 = Q_atof( token );
				ent.hasAmbientQ3map2 = ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "_minlight" ) ) {
				ent.minLight = Q_atof( token );
				ent.hasMinLight = ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "_minvertexlight" ) ) {
				ent.minVertexLight = Q_atof( token );
				ent.hasMinVertexLight = ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "_mingridlight" ) ) {
				ent.minGridLight = Q_atof( token );
				ent.hasMinGridLight = ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "_noshadersun" ) ) {
				ent.noShaderSun = ( token[0] == '1' ) ? qtrue : qfalse;
				ent.q3map2 = qtrue;
			} else if ( !Q_stricmpn( key, "_q3map2_", 8 ) ) {
				// _q3map2_version and _q3map2_cmdline
				ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "_farplanedist" ) ) {
				// q3map2's vis reads it, but so did Wolfenstein's own tools:
				// RTCW's mp_tram sets it beside its "fogclip" and keeps its 828
				// light entities, which q3map2 would have left out
				if ( ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP &&
					ri.gameId != COM_GAME_WOLFET ) {
					ent.q3map2 = qtrue;
				}
			} else if ( !Q_stricmp( key, "_lightmapscale" ) || !Q_stricmp( key, "_ls" ) ||
				!Q_stricmp( key, "_blocksize" ) || !Q_stricmp( key, "_celshader" ) ||
				!Q_stricmp( key, "_foghull" ) ||
				!Q_stricmp( key, "_keepLights" ) || !Q_stricmp( key, "_floodlight" ) ||
				!Q_stricmp( key, "_ignoreleaks" ) ) {
				// keys only q3map2 reads, in maps from builds that wrote no
				// _q3map2_version
				ent.q3map2 = qtrue;
			} else if ( !Q_stricmp( key, "radius" ) ) {
				ent.radius = Q_atof( token );
			} else if ( !Q_stricmp( key, "spawnflags" ) ) {
				ent.spawnflags = atoi( token );
			} else if ( !Q_stricmp( key, "target" ) ) {
				Q_strncpyz( ent.target, token, sizeof( ent.target ) );
			} else if ( !Q_stricmp( key, "targetname" ) ) {
				Q_strncpyz( ent.targetname, token, sizeof( ent.targetname ) );
			}
		}
		if ( count < maxEntities ) {
			entities[count++] = ent;
		}
		if ( !token[0] ) {
			break;
		}
	}
	return count;
}

// Whether q3map2 compiled the loaded map: its worldspawn carries keys only
// q3map2 writes or reads.
static qboolean RT_MapIsQ3map2( void )
{
	rtLightEntity_t worldspawn;
	return ( RT_ParseLightEntities( &worldspawn, 1 ) == 1 && !Q_stricmp( worldspawn.classname, "worldspawn" ) &&
		worldspawn.q3map2 ) ? qtrue : qfalse;
}

static rtGpuLight_t *RT_AddLight( const vec3_t center, float reach )
{
	rtGpuLight_t *light;

	if ( !RT_LightsReserve( s_rtLights.numLights + 1 ) ) {
		return NULL;
	}
	light = &s_rtLights.lights[ s_rtLights.numLights ];
	Com_Memset( light, 0, sizeof( *light ) );
	VectorCopy( center, s_rtLights.center[ s_rtLights.numLights ] );
	s_rtLights.reach[ s_rtLights.numLights ] = reach;
	s_rtLights.cluster[ s_rtLights.numLights ] = RT_PointCluster( center );
	s_rtLights.numLights++;
	return light;
}

/*
A point light of q3map photons (intensity * pointScale). A linear light
delivers angle * photons / 8000 - d * fade, any other photons * angle / d^2;
angle is the receiver's cosine where the light has one (extra[3] 0), else 1.
*/
static void RT_AddPointLight( const vec3_t origin, const vec3_t color, float photons, qboolean linear,
	qboolean angle, float fade, const vec3_t spotAxis, float radiusByDist, uint32_t skip )
{
	rtGpuLight_t *light;
	float reach;

	if ( !( photons > 0.0f ) || !RT_FloatValid( photons, RT_LIGHT_MAX_VALUE * RT_LIGHT_POINT_SCALE ) ||
		!RT_Vec3Valid( origin, RT_LIGHT_MAX_COORD ) || !RT_Vec3Valid( color, RT_LIGHT_MAX_VALUE ) ||
		!( fade > 0.0f ) || !RT_FloatValid( fade, RT_LIGHT_MAX_VALUE ) ||
		( spotAxis && ( !RT_Vec3Valid( spotAxis, 1.01f ) || !RT_FloatValid( radiusByDist, RT_LIGHT_MAX_VALUE ) ) ) ) {
		return;
	}
	reach = linear ? photons * RT_LIGHT_LINEAR_SCALE / fade : sqrtf( photons / RT_LIGHT_CUTOFF );
	reach = Com_Clamp( 32.0f, 65536.0f, reach );
	light = RT_AddLight( origin, reach );
	if ( !light ) {
		return;
	}
	VectorCopy( origin, light->position );
	light->position[3] = reach;
	if ( linear ) {
		VectorScale( color, s_rtLights.scale, light->color );
		light->color[3] = (float)( RT_LIGHT_POINT_LINEAR | skip );
		light->extra[1] = photons * RT_LIGHT_LINEAR_SCALE;
	} else {
		VectorScale( color, photons * s_rtLights.scale, light->color );
		light->color[3] = (float)( ( spotAxis ? RT_LIGHT_SPOT : RT_LIGHT_POINT ) | skip );
	}
	light->extra[0] = RT_LIGHT_MIN_DISTANCE;
	light->extra[2] = linear ? fade : 1.0f;
	light->extra[3] = angle ? 0.0f : 1.0f;
	if ( spotAxis ) {
		VectorCopy( spotAxis, light->dir );
		light->dir[3] = radiusByDist;
	}
	s_rtLights.pointLights++;
}

// The worldspawn's ambient term, and q3map2's floors. Colours stay
// unnormalised, as both compilers use them.
static void RT_WorldspawnLight( const rtLightEntity_t *ent )
{
	vec3_t color;
	float ambient;

	s_rtLights.q3map2 = ent->q3map2 || s_rtLights.q3map2Shaders;
	s_rtLights.noShaderSun = ent->noShaderSun;
	if ( ent->hasColor ) {
		VectorCopy( ent->color, color );
	} else {
		VectorClear( color );
	}
	if ( ent->q3map2 ) {
		if ( VectorCompare( color, vec3_origin ) ) {
			VectorSet( color, 1.0f, 1.0f, 1.0f );
		}
		ambient = ent->hasAmbientQ3map2 ? ent->ambientQ3map2 : ent->ambient;
		// _minlight also sets the vertex and grid floors unless they have their own
		if ( ent->hasMinLight && RT_FloatValid( ent->minLight, RT_LIGHT_MAX_VALUE ) ) {
			VectorScale( color, MAX( ent->minLight, 0.0f ) * s_rtLights.scale, s_rtLights.minLight );
			VectorCopy( s_rtLights.minLight, s_rtLights.minVertexLight );
			VectorCopy( s_rtLights.minLight, s_rtLights.minGridLight );
		}
		if ( ent->hasMinVertexLight && RT_FloatValid( ent->minVertexLight, RT_LIGHT_MAX_VALUE ) ) {
			VectorScale( color, MAX( ent->minVertexLight, 0.0f ) * s_rtLights.scale, s_rtLights.minVertexLight );
		}
		if ( ent->hasMinGridLight && RT_FloatValid( ent->minGridLight, RT_LIGHT_MAX_VALUE ) ) {
			VectorScale( color, MAX( ent->minGridLight, 0.0f ) * s_rtLights.scale, s_rtLights.minGridLight );
		}
	} else {
		ambient = ent->ambient;
	}
	if ( ambient > 0.0f && RT_FloatValid( ambient, RT_LIGHT_MAX_VALUE ) ) {
		VectorScale( color, ambient * s_rtLights.scale, s_rtLights.ambient );
	}
}

static void RT_AddEntityLights( void )
{
	rtLightEntity_t *entities;
	int count;
	int i;

	entities = RT_HostAlloc( RT_LIGHT_MAX_ENTITIES * sizeof( *entities ) );
	if ( !entities ) {
		return;
	}
	count = RT_ParseLightEntities( entities, RT_LIGHT_MAX_ENTITIES );
	for ( i = 0; i < count; i++ ) {
		const rtLightEntity_t *ent = &entities[i];
		vec3_t color;
		float intensity, fade;
		qboolean linear, angle;
		uint32_t skip;

		if ( i == 0 && !Q_stricmp( ent->classname, "worldspawn" ) ) {
			RT_WorldspawnLight( ent );
			continue;
		}
		// q3map compares classnames and targets case-sensitively
		if ( strncmp( ent->classname, "light", 5 ) ) {
			continue;
		}
		s_rtLights.entityLights++;
		if ( !ent->hasOrigin ) {
			continue;
		}
		if ( s_rtLights.q3map2 ) {
			intensity = ent->lightUnderscore != 0.0f ? ent->lightUnderscore : ent->light != 0.0f ? ent->light : 300.0f;
			if ( ent->scale != 0.0f ) {
				intensity *= ent->scale;
			}
		} else {
			intensity = ent->light != 0.0f ? ent->light : ent->lightUnderscore != 0.0f ? ent->lightUnderscore : 300.0f;
		}
		if ( !RT_FloatValid( intensity, RT_LIGHT_MAX_VALUE ) ) {
			continue;
		}
		// attenuation by lighting model: Wolfenstein, q3map2's Quake III, id's q3map
		if ( s_rtLights.wolf ) {
			linear = ( ent->spawnflags & 1 ) ? qfalse : qtrue;
			angle = ( ent->spawnflags & 3 ) ? qtrue : qfalse;
		} else if ( s_rtLights.q3map2 ) {
			linear = ( ent->spawnflags & 1 ) ? qtrue : qfalse;
			angle = ( ent->spawnflags & 3 ) ? qfalse : qtrue;
		} else {
			linear = ( ent->spawnflags & 1 ) ? qtrue : qfalse;
			angle = qtrue;
		}
		fade = ( linear && ent->fade > 0.0f && RT_FloatValid( ent->fade, RT_LIGHT_MAX_VALUE ) ) ? ent->fade : 1.0f;
		// a lightJunior lights the grid alone; nogrid keeps a light off it
		if ( !Q_stricmpn( ent->classname, "lightJunior", 11 ) ) {
			skip = RT_LIGHT_NO_SURFACES;
		} else if ( ( s_rtLights.wolf || s_rtLights.q3map2 ) && ( ent->spawnflags & 16 ) ) {
			skip = RT_LIGHT_NO_MODELS;
		} else {
			skip = 0;
		}
		if ( ent->hasColor ) {
			const float maxColor = MAX( ent->color[0], MAX( ent->color[1], ent->color[2] ) );
			if ( maxColor > 0.0f ) {
				VectorScale( ent->color, 1.0f / maxColor, color );
			} else {
				VectorSet( color, 1.0f, 1.0f, 1.0f );
			}
		} else {
			VectorSet( color, 1.0f, 1.0f, 1.0f );
		}

		if ( ent->target[0] ) {
			const rtLightEntity_t *target = NULL;
			int j;
			for ( j = 0; j < count; j++ ) {
				if ( entities[j].targetname[0] && !strcmp( entities[j].targetname, ent->target ) ) {
					target = &entities[j];
					break;
				}
			}
			if ( target && target->hasOrigin ) {
				vec3_t axis;
				float dist;
				float radius = ent->radius != 0.0f ? ent->radius : 64.0f;
				if ( !RT_FloatValid( radius, RT_LIGHT_MAX_COORD ) ) {
					continue;
				}
				VectorSubtract( target->origin, ent->origin, axis );
				dist = VectorNormalize( axis );
				if ( dist == 0.0f ) {
					dist = 64.0f;
				}
				// every compiler: a spotlight is inverse square with the cosine
				RT_AddPointLight( ent->origin, color, intensity * RT_LIGHT_POINT_SCALE, qfalse, qtrue, 1.0f, axis,
					( radius + 16.0f ) / dist, skip );
				continue;
			}
			// RTCW's compiler: no light from a missing target's light (once
			// every entity was read, so the target is known to be missing)
			if ( !target && s_rtLights.wolf && !s_rtLights.q3map2 && count < RT_LIGHT_MAX_ENTITIES ) {
				s_rtLights.untargetedLights++;
				continue;
			}
		}
		RT_AddPointLight( ent->origin, color, intensity * RT_LIGHT_POINT_SCALE, linear, angle, fade, NULL, 0.0f, skip );
	}
	RT_HostFree( entities );
}

/*
==================
Surface lights
==================
*/

// A triangle's centroid, computed one way for its light and for the world
// triangle it is matched to (RT_MapPrimitiveLights), so the two agree bit
// for bit.
static void RT_TriangleCentroid( const vec3_t v0, const vec3_t v1, const vec3_t v2, vec3_t centroid )
{
	centroid[0] = ( v0[0] + v1[0] + v2[0] ) * ( 1.0f / 3.0f );
	centroid[1] = ( v0[1] + v1[1] + v2[1] ) * ( 1.0f / 3.0f );
	centroid[2] = ( v0[2] + v1[2] + v2[2] ) * ( 1.0f / 3.0f );
}

// flags: RT_LIGHT_SKY for a sky surface's emitters
static void RT_AddTriangleLight( const vec3_t v0, const vec3_t v1, const vec3_t v2, const vec3_t normal,
	const vec3_t radiance, float emitValue, qboolean twoSided, uint32_t flags )
{
	vec3_t e1, e2, n, centroid;
	rtGpuLight_t *light;
	float area, reach;

	if ( !RT_Vec3Valid( v0, RT_LIGHT_MAX_COORD ) || !RT_Vec3Valid( v1, RT_LIGHT_MAX_COORD ) ||
		!RT_Vec3Valid( v2, RT_LIGHT_MAX_COORD ) || !RT_Vec3Valid( normal, 1.01f ) ||
		!RT_Vec3Valid( radiance, RT_LIGHT_MAX_VALUE ) ) {
		return;
	}
	VectorSubtract( v1, v0, e1 );
	VectorSubtract( v2, v0, e2 );
	CrossProduct( e1, e2, n );
	area = 0.5f * VectorLength( n );
	if ( !( area > 0.01f ) ) {
		return;
	}
	if ( DotProduct( n, normal ) < 0.0f ) {
		// wind so that e1 x e2 points out of the emitting face
		vec3_t t;
		VectorCopy( e1, t );
		VectorCopy( e2, e1 );
		VectorCopy( t, e2 );
	}
	RT_TriangleCentroid( v0, v1, v2, centroid );
	// value * 0.75 * area / (pi d^2) falls to the cutoff at this distance
	reach = sqrtf( emitValue * RT_LIGHT_AREA_SCALE * RT_LIGHT_FORM_FACTOR_SCALE * area / ( M_PI * RT_LIGHT_CUTOFF ) );
	reach = Com_Clamp( 16.0f, 65536.0f, reach + sqrtf( area ) );
	{
		// offset the cluster probe off the surface, into the lit side
		vec3_t probe;
		VectorMA( centroid, 2.0f, normal, probe );
		light = RT_AddLight( probe, reach );
	}
	if ( !light ) {
		return;
	}
	VectorCopy( v0, light->position );
	light->position[3] = reach;
	VectorCopy( radiance, light->color );
	light->color[3] = (float)( RT_LIGHT_TRIANGLE | flags );
	VectorCopy( e1, light->dir );
	light->dir[3] = area;
	VectorCopy( e2, light->extra );
	light->extra[3] = twoSided ? 1.0f : 0.0f;
	VectorCopy( centroid, s_rtLights.center[ s_rtLights.numLights - 1 ] );
	s_rtLights.triangleLights++;
}

/*
Backsplash photons gathered per subdivision cell: an emitter's pieces that
fall in one cell of RT_LIGHT_SUBDIVIDE-sized cells (or the shader's
q3map_lightsubdivide) become one point light at their photon-weighted centre,
so a surface of many small facets adds a light per cell rather than one per
facet, and a large one still spreads its backsplash over its extent.
*/
#define RT_BACKSPLASH_BUCKETS	64

typedef struct {
	int			cell[3];
	vec3_t		weighted;		// sum of photons * position
	vec3_t		normal;			// sum of photons * normal
	float		photons;
} rtBacksplashBucket_t;

typedef struct {
	rtBacksplashBucket_t	buckets[RT_BACKSPLASH_BUCKETS];
	int						count;
	float					subdivide;
} rtBacksplash_t;

static void RT_BacksplashAdd( rtBacksplash_t *bs, const vec3_t position, const vec3_t normal, float photons )
{
	int cell[3], i, b;

	if ( !( photons > 0.0f ) || !RT_Vec3Valid( position, RT_LIGHT_MAX_COORD ) ) {
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		cell[i] = (int)floorf( position[i] / bs->subdivide );
	}
	for ( b = 0; b < bs->count; b++ ) {
		if ( bs->buckets[b].cell[0] == cell[0] && bs->buckets[b].cell[1] == cell[1] && bs->buckets[b].cell[2] == cell[2] ) {
			break;
		}
	}
	if ( b == bs->count ) {
		if ( bs->count == RT_BACKSPLASH_BUCKETS ) {
			b = 0;	// more cells than buckets: fold the rest into the first
		} else {
			Com_Memset( &bs->buckets[b], 0, sizeof( bs->buckets[b] ) );
			VectorCopy( cell, bs->buckets[b].cell );
			bs->count++;
		}
	}
	VectorMA( bs->buckets[b].weighted, photons, position, bs->buckets[b].weighted );
	VectorMA( bs->buckets[b].normal, photons, normal, bs->buckets[b].normal );
	bs->buckets[b].photons += photons;
}

// Emit each bucket's light the backsplash distance beyond the area light's
// origin, which sits one unit off the surface.
static void RT_BacksplashEmit( const rtBacksplash_t *bs, const vec3_t color, float distance )
{
	int b;

	for ( b = 0; b < bs->count; b++ ) {
		const rtBacksplashBucket_t *bucket = &bs->buckets[b];
		vec3_t origin, normal;
		VectorScale( bucket->weighted, 1.0f / bucket->photons, origin );
		VectorCopy( bucket->normal, normal );
		if ( VectorNormalize( normal ) == 0.0f ) {
			continue;
		}
		VectorMA( origin, 1.0f + distance, normal, origin );
		RT_AddPointLight( origin, color, bucket->photons, qfalse, qtrue, 1.0f, NULL, 0.0f, 0 );
	}
}

/*
The map compilers' backsplash: a point light a little in front of an area
light, with a fraction of its photons (q3map_backsplash, 5% by default).
id's q3map gives one to each emitting winding it did not subdivide - a
planar face, or a patch's or model's facet, no larger than the light
subdivision (q3map_lightsubdivide, 999 units) on any axis - at the winding's
centre (light.c SubdivideAreaLight); a larger winding, such as a big sky
face, gets none. q3map2 gives one to every piece it subdivides an emitter
into, and none to a sky surface (light_bounce.c). Both put it the backsplash
distance beyond the area light's origin, one unit off the surface.
*/
static void RT_AddBacksplash( const shader_t *shader, qboolean planar, const rtxRtCpuGeometry_t *geometry,
	const vec3_t normal, const vec3_t color, float value )
{
	const float fraction = shader->rtBacksplashValid ? shader->rtBacksplashFraction : RT_LIGHT_BACKSPLASH_FRACTION;
	const float distance = shader->rtBacksplashValid ? shader->rtBacksplashDistance :
		( s_rtLights.q3map2 ? RT_LIGHT_BACKSPLASH_DISTANCE_Q3MAP2 : RT_LIGHT_BACKSPLASH_DISTANCE );
	const float photonsPerArea = value * RT_LIGHT_AREA_SCALE * fraction;
	rtBacksplash_t bs;
	uint32_t t, v;

	if ( !( fraction > 0.0f ) || geometry->numIndices < 3 || geometry->numVertices == 0 ) {
		return;
	}
	Com_Memset( &bs, 0, sizeof( bs ) );
	bs.subdivide = shader->surfaceLightSubdivide > 0.0f ? shader->surfaceLightSubdivide : RT_LIGHT_SUBDIVIDE;
	if ( !s_rtLights.q3map2 && planar ) {
		// the face is one winding: none if it was subdivided, else one at the
		// mean of its corners
		vec3_t mins, maxs, center;
		float area = 0.0f;
		ClearBounds( mins, maxs );
		VectorClear( center );
		for ( v = 0; v < geometry->numVertices; v++ ) {
			AddPointToBounds( geometry->vertices[v].xyz, mins, maxs );
			VectorAdd( center, geometry->vertices[v].xyz, center );
		}
		if ( maxs[0] - mins[0] > bs.subdivide || maxs[1] - mins[1] > bs.subdivide || maxs[2] - mins[2] > bs.subdivide ) {
			return;
		}
		for ( t = 0; t + 2 < geometry->numIndices; t += 3 ) {
			const float *a = geometry->vertices[ geometry->indices[t + 0] ].xyz;
			vec3_t e1, e2, n;
			VectorSubtract( geometry->vertices[ geometry->indices[t + 1] ].xyz, a, e1 );
			VectorSubtract( geometry->vertices[ geometry->indices[t + 2] ].xyz, a, e2 );
			CrossProduct( e1, e2, n );
			area += 0.5f * VectorLength( n );
		}
		VectorScale( center, 1.0f / (float)geometry->numVertices, center );
		RT_BacksplashAdd( &bs, center, normal, photonsPerArea * area );
		RT_BacksplashEmit( &bs, color, distance );
		return;
	}
	if ( s_rtLights.q3map2 && RT_ShaderIsSky( shader ) ) {
		return;
	}
	// id's q3map: each facet of a patch or model no larger than the
	// subdivision; q3map2: every piece of the surface
	for ( t = 0; t + 2 < geometry->numIndices; t += 3 ) {
		const float *a = geometry->vertices[ geometry->indices[t + 0] ].xyz;
		const float *b = geometry->vertices[ geometry->indices[t + 1] ].xyz;
		const float *c = geometry->vertices[ geometry->indices[t + 2] ].xyz;
		vec3_t mins, maxs, e1, e2, n, centroid;
		float area;
		ClearBounds( mins, maxs );
		AddPointToBounds( a, mins, maxs );
		AddPointToBounds( b, mins, maxs );
		AddPointToBounds( c, mins, maxs );
		if ( !s_rtLights.q3map2 && ( maxs[0] - mins[0] > bs.subdivide || maxs[1] - mins[1] > bs.subdivide ||
			maxs[2] - mins[2] > bs.subdivide ) ) {
			continue;
		}
		VectorSubtract( b, a, e1 );
		VectorSubtract( c, a, e2 );
		CrossProduct( e1, e2, n );
		area = 0.5f * VectorLength( n );
		if ( VectorNormalize( n ) == 0.0f ) {
			continue;
		}
		if ( DotProduct( n, normal ) < 0.0f ) {
			VectorNegate( n, n );
		}
		centroid[0] = ( a[0] + b[0] + c[0] ) * ( 1.0f / 3.0f );
		centroid[1] = ( a[1] + b[1] + c[1] ) * ( 1.0f / 3.0f );
		centroid[2] = ( a[2] + b[2] + c[2] ) * ( 1.0f / 3.0f );
		RT_BacksplashAdd( &bs, centroid, n, photonsPerArea * area );
	}
	RT_BacksplashEmit( &bs, color, distance );
}

static void RT_AddSurfaceLights( void )
{
	const bmodel_t *worldModel = &tr.world->bmodels[0];
	const shader_t *reported[64];	// each shader's value, printed once (developer)
	int numReported = 0;
	int i;

	for ( i = 0; i < worldModel->numSurfaces; i++ ) {
		const msurface_t *surface = &worldModel->firstSurface[i];
		const shader_t *shader = surface->shader;
		rtxRtCpuGeometry_t geometry;
		vec3_t color, radiance, normal, centroid;
		float value, totalArea = 0.0f;
		qboolean autosprite = qfalse;
		qboolean twoSided;
		uint32_t t;
		int d;

		if ( !shader || !shader->surfaceLightValid || !( shader->surfaceLight > 0.0f ) ||
			!RT_FloatValid( shader->surfaceLight, RT_LIGHT_MAX_VALUE ) ) {
			continue;
		}
		if ( s_rtLights.mode == RT_LIGHTING_EMITTERS && RT_ShaderIsSky( shader ) ) {
			// the sky lights the scene with its own colour (rt_sample_sky_light)
			continue;
		}
		value = shader->surfaceLight;
		{
			int r;
			for ( r = 0; r < numReported && reported[r] != shader; r++ ) {
			}
			if ( r == numReported && numReported < (int)ARRAY_LEN( reported ) ) {
				reported[numReported++] = shader;
				ri.Printf( PRINT_DEVELOPER, "RTX RT: %s is a surface light of %.0f\n", shader->name, value );
			}
		}
		if ( shader->rtEmitColorValid && RT_Vec3Valid( shader->rtEmitColor, RT_LIGHT_MAX_VALUE ) ) {
			VectorCopy( shader->rtEmitColor, color );
		} else {
			VectorSet( color, 1.0f, 1.0f, 1.0f );
		}
		for ( d = 0; d < shader->numDeforms; d++ ) {
			if ( shader->deforms[d].deformation == DEFORM_AUTOSPRITE ||
				shader->deforms[d].deformation == DEFORM_AUTOSPRITE2 ) {
				autosprite = qtrue;
			}
		}
		twoSided = ( shader->cullType == CT_TWO_SIDED || ( shader->contentFlags & CONTENTS_FOG ) ) ? qtrue : qfalse;

		vk_rt_cpu_geometry_init( &geometry );
		switch ( *surface->data ) {
		case SF_FACE:
			vk_rt_append_surface_face_geometry( &geometry, (const srfSurfaceFace_t *)surface->data, shader, 0 );
			break;
		case SF_GRID:
			vk_rt_append_surface_grid_geometry( &geometry, (const srfGridMesh_t *)surface->data, shader, 0 );
			break;
		case SF_TRIANGLES:
			vk_rt_append_surface_triangles_geometry( &geometry, (const srfTriangles_t *)surface->data, shader, 0 );
			break;
		default:
			break;
		}
		if ( geometry.numIndices < 3 ) {
			vk_rt_cpu_geometry_free( &geometry );
			continue;
		}

		VectorClear( centroid );
		VectorClear( normal );
		for ( t = 0; t + 2 < geometry.numIndices; t += 3 ) {
			const float *a = geometry.vertices[ geometry.indices[t + 0] ].xyz;
			const float *b = geometry.vertices[ geometry.indices[t + 1] ].xyz;
			const float *c = geometry.vertices[ geometry.indices[t + 2] ].xyz;
			vec3_t e1, e2, n;
			float area;
			VectorSubtract( b, a, e1 );
			VectorSubtract( c, a, e2 );
			CrossProduct( e1, e2, n );
			area = 0.5f * VectorLength( n );
			totalArea += area;
			centroid[0] += ( a[0] + b[0] + c[0] ) * ( area / 3.0f );
			centroid[1] += ( a[1] + b[1] + c[1] ) * ( area / 3.0f );
			centroid[2] += ( a[2] + b[2] + c[2] ) * ( area / 3.0f );
			VectorAdd( normal, geometry.vertices[ geometry.indices[t] ].normal, normal );
		}
		if ( totalArea > 0.0f ) {
			VectorScale( centroid, 1.0f / totalArea, centroid );
		}
		VectorNormalize( normal );

		if ( autosprite ) {
			// q3map lights an autosprite emitter as a point of intensity value
			RT_AddPointLight( centroid, color, value * RT_LIGHT_POINT_SCALE, qfalse, qtrue, 1.0f, NULL, 0.0f, 0 );
			vk_rt_cpu_geometry_free( &geometry );
			continue;
		}

		// radiance whose uniform-area estimate yields form factor * value * 0.75
		VectorScale( color, value * RT_LIGHT_AREA_SCALE * RT_LIGHT_FORM_FACTOR_SCALE * s_rtLights.scale / M_PI, radiance );
		for ( t = 0; t + 2 < geometry.numIndices; t += 3 ) {
			const rtxRtPackedVertex_t *a = &geometry.vertices[ geometry.indices[t + 0] ];
			const rtxRtPackedVertex_t *b = &geometry.vertices[ geometry.indices[t + 1] ];
			const rtxRtPackedVertex_t *c = &geometry.vertices[ geometry.indices[t + 2] ];
			vec3_t n;
			VectorAdd( a->normal, b->normal, n );
			VectorAdd( n, c->normal, n );
			if ( VectorNormalize( n ) == 0.0f ) {
				VectorCopy( normal, n );
			}
			RT_AddTriangleLight( a->xyz, b->xyz, c->xyz, n, radiance, value, twoSided,
				RT_ShaderIsSky( shader ) ? RT_LIGHT_SKY : 0u );
		}

		// q3map's backsplash stood in for the light an emitter's own fixture
		// bounces back; traced bounces carry it
		if ( !RT_LightingEmits( s_rtLights.mode ) ) {
			RT_AddBacksplash( shader, *surface->data == SF_FACE ? qtrue : qfalse, &geometry, normal, color, value );
		}
		vk_rt_cpu_geometry_free( &geometry );
	}
}

/*
Shaders that glow without q3map_surfacelight (RT_ShaderEmission): every
triangle of a world surface drawn with one is a triangle light of the
radiance its stages show, times rtx_rt_emissive_light, as Q2RTX lights from
its emissive textures. The q3map value that would give that radiance sets
how far the light reaches.
*/
#define RT_EMISSIVE_SHADER_CACHE	MAX_SHADERS

// A display colour in linear light: gamma 2.2, as rt_common.glsl's
// rt_linear decodes what the shaders show
static void RT_LinearColor( vec3_t c )
{
	int i;
	for ( i = 0; i < 3; i++ ) {
		c[i] = c[i] > 0.0f ? powf( c[i], 2.2f ) : 0.0f;
	}
}

static void RT_AddEmissiveSurfaces( void )
{
	const bmodel_t *worldModel = &tr.world->bmodels[0];
	const float boost = rtx_rt_emissive_light ? MAX( 0.0f, rtx_rt_emissive_light->value ) : 1.0f;
	static byte known[RT_EMISSIVE_SHADER_CACHE];	// 0 unknown, 1 dark, 2 emits
	static vec3_t emissions[RT_EMISSIVE_SHADER_CACHE];
	int i;

	if ( !( boost > 0.0f ) ) {
		return;
	}
	Com_Memset( known, 0, sizeof( known ) );
	for ( i = 0; i < worldModel->numSurfaces; i++ ) {
		const msurface_t *surface = &worldModel->firstSurface[i];
		const shader_t *shader = surface->shader;
		rtxRtCpuGeometry_t geometry;
		vec3_t radiance;
		float value, peak;
		qboolean twoSided;
		uint32_t t;

		if ( !shader || ( shader->surfaceLightValid && shader->surfaceLight > 0.0f ) ||
			shader->index < 0 || shader->index >= RT_EMISSIVE_SHADER_CACHE ) {
			continue;
		}
		if ( !known[shader->index] ) {
			known[shader->index] = RT_ShaderEmission( shader, emissions[shader->index] ) ? 2 : 1;
			if ( known[shader->index] == 2 ) {
				// the light it gives off is what it shows: a display colour, as
				// a lightmap's light is, or in linear units where the camera
				// takes linear light (the trace decodes the images' pixels)
				if ( RT_LightingCamera( s_rtLights.mode ) == RT_CAMERA_LINEAR ) {
					RT_LinearColor( emissions[shader->index] );
				}
				s_rtLights.emissiveShaders++;
				ri.Printf( PRINT_DEVELOPER, "RTX RT: %s glows %.3f %.3f %.3f\n", shader->name,
					emissions[shader->index][0], emissions[shader->index][1], emissions[shader->index][2] );
			}
		}
		if ( known[shader->index] != 2 ) {
			continue;
		}
		// the diffuse light a white surface returns is kept as radiance / pi
		VectorScale( emissions[shader->index], boost / M_PI, radiance );
		peak = MAX( radiance[0], MAX( radiance[1], radiance[2] ) );
		value = peak * M_PI / ( RT_LIGHT_AREA_SCALE * RT_LIGHT_FORM_FACTOR_SCALE * s_rtLights.scale );
		if ( !( peak > 0.0f ) || !RT_FloatValid( value, RT_LIGHT_MAX_VALUE ) ) {
			continue;
		}
		twoSided = ( shader->cullType == CT_TWO_SIDED ) ? qtrue : qfalse;

		vk_rt_cpu_geometry_init( &geometry );
		switch ( *surface->data ) {
		case SF_FACE:
			vk_rt_append_surface_face_geometry( &geometry, (const srfSurfaceFace_t *)surface->data, shader, 0 );
			break;
		case SF_GRID:
			vk_rt_append_surface_grid_geometry( &geometry, (const srfGridMesh_t *)surface->data, shader, 0 );
			break;
		case SF_TRIANGLES:
			vk_rt_append_surface_triangles_geometry( &geometry, (const srfTriangles_t *)surface->data, shader, 0 );
			break;
		default:
			break;
		}
		for ( t = 0; t + 2 < geometry.numIndices; t += 3 ) {
			const rtxRtPackedVertex_t *a = &geometry.vertices[ geometry.indices[t + 0] ];
			const rtxRtPackedVertex_t *b = &geometry.vertices[ geometry.indices[t + 1] ];
			const rtxRtPackedVertex_t *c = &geometry.vertices[ geometry.indices[t + 2] ];
			vec3_t n, e1, e2;
			VectorAdd( a->normal, b->normal, n );
			VectorAdd( n, c->normal, n );
			if ( VectorNormalize( n ) == 0.0f ) {
				VectorSubtract( b->xyz, a->xyz, e1 );
				VectorSubtract( c->xyz, a->xyz, e2 );
				CrossProduct( e1, e2, n );
				if ( VectorNormalize( n ) == 0.0f ) {
					continue;
				}
			}
			RT_AddTriangleLight( a->xyz, b->xyz, c->xyz, n, radiance, value, twoSided, 0u );
			s_rtLights.emissiveTriangles++;
		}
		vk_rt_cpu_geometry_free( &geometry );
	}
}

/*
==================
Light grid
==================
*/

// Squared distance from a point to an axial box.
static float RT_BoxDistanceSquared( const vec3_t p, const vec3_t mins, const vec3_t maxs )
{
	float d = 0.0f;
	int i;
	for ( i = 0; i < 3; i++ ) {
		float v = 0.0f;
		if ( p[i] < mins[i] ) v = mins[i] - p[i];
		else if ( p[i] > maxs[i] ) v = p[i] - maxs[i];
		d += v * v;
	}
	return d;
}

// Rough unshadowed strength of a light at distance-squared d2, for ranking.
static float RT_LightImportance( const rtGpuLight_t *light, float d2 )
{
	const float lum = light->color[0] * 0.2126f + light->color[1] * 0.7152f + light->color[2] * 0.0722f;
	const uint32_t type = (uint32_t)light->color[3] & RT_LIGHT_TYPE_MASK;
	if ( type == RT_LIGHT_TRIANGLE ) {
		return lum * light->dir[3] / MAX( d2, light->dir[3] );
	}
	if ( type == RT_LIGHT_POINT_LINEAR ) {
		return lum * MAX( light->extra[1] - sqrtf( d2 ) * light->extra[2], 1.0f );
	}
	return lum / MAX( d2, RT_LIGHT_MIN_DISTANCE * RT_LIGHT_MIN_DISTANCE );
}

typedef struct {
	float importance;
	uint32_t light;
} rtCellCandidate_t;

static int RT_CompareCandidates( const void *a, const void *b )
{
	const float ia = ( (const rtCellCandidate_t *)a )->importance;
	const float ib = ( (const rtCellCandidate_t *)b )->importance;
	return ia > ib ? -1 : ia < ib ? 1 : 0;
}

// The grid cells a light's reach may touch: its bounding box, a cell wider
// on each side. A reach that is not a finite number touches every cell, as
// the exact test would let it.
static qboolean RT_LightCellRange( uint32_t l, float cellSize, int lo[3], int hi[3] )
{
	const float reach = s_rtLights.reach[l];
	int i;

	for ( i = 0; i < 3; i++ ) {
		const float last = (float)( s_rtLights.dims[i] - 1 );
		float a, b;
		if ( !RT_FloatValid( reach, 1e30f ) ) {
			lo[i] = 0;
			hi[i] = s_rtLights.dims[i] - 1;
			continue;
		}
		a = floorf( ( s_rtLights.center[l][i] - reach - s_rtLights.gridOrigin[i] ) / cellSize ) - 1.0f;
		b = floorf( ( s_rtLights.center[l][i] + reach - s_rtLights.gridOrigin[i] ) / cellSize ) + 1.0f;
		if ( b < 0.0f || a > last ) {
			return qfalse;
		}
		lo[i] = a < 0.0f ? 0 : (int)a;
		hi[i] = b > last ? s_rtLights.dims[i] - 1 : (int)b;
	}
	return qtrue;
}

/*
Each cell's candidate lights in light order, listed from the cells each
light's reach may touch, so a cell tests only lights that can reach it
instead of every light in the map (RTCW's mp_ice has 4,529 lights over
168,168 cells). Candidates still take the exact test in the order a scan of
every light would, so the grid comes out the same. NULL, and the cells scan
every light, when the lists would pass RT_LIGHT_MAX_LISTED entries.
*/
#define RT_LIGHT_MAX_LISTED		( 1u << 24 )

static uint32_t *RT_ListCellLights( uint32_t numCells, float cellSize, uint32_t **outStart )
{
	uint32_t *start, *listed;
	uint64_t total = 0;
	uint32_t l, cell;
	int lo[3], hi[3], x, y, z;

	*outStart = NULL;
	start = RT_HostAlloc( (int)( ( numCells + 1 ) * sizeof( uint32_t ) ) );
	if ( !start ) {
		return NULL;
	}
	Com_Memset( start, 0, ( numCells + 1 ) * sizeof( uint32_t ) );
	for ( l = 0; l < s_rtLights.numLights; l++ ) {
		if ( !RT_LightCellRange( l, cellSize, lo, hi ) ) {
			continue;
		}
		total += (uint64_t)( hi[0] - lo[0] + 1 ) * (uint64_t)( hi[1] - lo[1] + 1 ) * (uint64_t)( hi[2] - lo[2] + 1 );
		if ( total > RT_LIGHT_MAX_LISTED ) {
			RT_HostFree( start );
			return NULL;
		}
		for ( z = lo[2]; z <= hi[2]; z++ ) {
			for ( y = lo[1]; y <= hi[1]; y++ ) {
				for ( x = lo[0]; x <= hi[0]; x++ ) {
					start[ ( z * s_rtLights.dims[1] + y ) * s_rtLights.dims[0] + x + 1 ]++;
				}
			}
		}
	}
	for ( cell = 0; cell < numCells; cell++ ) {
		start[cell + 1] += start[cell];
	}
	listed = RT_HostAlloc( (int)( MAX( total, 1u ) * sizeof( uint32_t ) ) );
	if ( !listed ) {
		RT_HostFree( start );
		return NULL;
	}
	// fill in light order, each cell's cursor running from its start
	for ( l = 0; l < s_rtLights.numLights; l++ ) {
		if ( !RT_LightCellRange( l, cellSize, lo, hi ) ) {
			continue;
		}
		for ( z = lo[2]; z <= hi[2]; z++ ) {
			for ( y = lo[1]; y <= hi[1]; y++ ) {
				for ( x = lo[0]; x <= hi[0]; x++ ) {
					listed[ start[ ( z * s_rtLights.dims[1] + y ) * s_rtLights.dims[0] + x ]++ ] = l;
				}
			}
		}
	}
	// the cursors now sit at each cell's end: shift them back to the starts
	for ( cell = numCells; cell > 0; cell-- ) {
		start[cell] = start[cell - 1];
	}
	start[0] = 0;
	*outStart = start;
	return listed;
}

static qboolean RT_BuildLightGrid( void )
{
	const bmodel_t *worldModel = &tr.world->bmodels[0];
	rtCellCandidate_t *candidates;
	uint32_t *listed, *listStart;
	uint32_t numCells, cell, capacity;
	vec3_t size;
	float cellSize = RT_LIGHT_CELL_SIZE;
	int x, y, z, i;

	if ( !RT_Vec3Valid( worldModel->bounds[0], RT_LIGHT_MAX_COORD ) ||
		!RT_Vec3Valid( worldModel->bounds[1], RT_LIGHT_MAX_COORD ) ) {
		ri.Printf( PRINT_WARNING, "RTX RT: world bounds are malformed; no light grid\n" );
		return qfalse;
	}
	VectorCopy( worldModel->bounds[0], s_rtLights.gridOrigin );
	VectorSubtract( worldModel->bounds[1], worldModel->bounds[0], size );
	for ( ;; ) {
		for ( i = 0; i < 3; i++ ) {
			s_rtLights.dims[i] = MAX( 1, (int)ceilf( size[i] / cellSize ) );
		}
		// each dimension is at most 2^14 cells, so the product needs 64 bits
		if ( (uint64_t)s_rtLights.dims[0] * (uint64_t)s_rtLights.dims[1] * (uint64_t)s_rtLights.dims[2] <=
			RT_LIGHT_MAX_CELLS ) {
			break;
		}
		cellSize *= 1.25f;
	}
	s_rtLights.cellSize = cellSize;
	numCells = (uint32_t)( s_rtLights.dims[0] * s_rtLights.dims[1] * s_rtLights.dims[2] );

	s_rtLights.cells = RT_HostAlloc( (int)( ( numCells + s_rtLights.numLights ) * 2 * sizeof( uint32_t ) ) );
	candidates = RT_HostAlloc( (int)( MAX( s_rtLights.numLights, 1u ) * sizeof( *candidates ) ) );
	capacity = numCells * 8;
	s_rtLights.indices = RT_HostAlloc( (int)( capacity * sizeof( uint32_t ) ) );
	if ( !s_rtLights.cells || !candidates || !s_rtLights.indices ) {
		if ( candidates ) RT_HostFree( candidates );
		return qfalse;
	}
	s_rtLights.numIndices = 0;
	s_rtLights.overflowCells = s_rtLights.overflowEntries = 0;
	s_rtLights.overflowImportance = s_rtLights.candidateImportance = 0.0;
	// A power-weighted global proposal preserves support for every emitter,
	// including ones omitted by the bounded cell lists or conservative PVS.
	// A 10% uniform floor keeps every CDF interval representable at the
	// maximum 2^20 lights. Store the actual quantized interval as its PDF.
	{
		double total = 0.0, cumulative = 0.0;
		float previous = 0.0f;
		uint32_t l;
		for ( l = 0; l < s_rtLights.numLights; l++ ) {
			float w = RT_LightImportance( &s_rtLights.lights[l], 1024.0f * 1024.0f );
			candidates[l].importance = RT_FloatValid( w, 1e30f ) ? MAX( w, 0.0f ) : 0.0f;
			total += candidates[l].importance;
		}
		for ( l = 0; l < s_rtLights.numLights; l++ ) {
			floatint_t cdf, probability;
			cumulative += candidates[l].importance;
			cdf.f = l + 1u == s_rtLights.numLights ? 1.0f : (float)(
				total > 0.0 ? 0.9 * cumulative / total + 0.1 * ( l + 1.0 ) / s_rtLights.numLights :
				( l + 1.0 ) / s_rtLights.numLights );
			probability.f = cdf.f - previous;
			s_rtLights.cells[( numCells + l ) * 2u] = cdf.u;
			s_rtLights.cells[( numCells + l ) * 2u + 1u] = probability.u;
			previous = cdf.f;
		}
	}
	listed = RT_ListCellLights( numCells, cellSize, &listStart );

	cell = 0;
	for ( z = 0; z < s_rtLights.dims[2]; z++ ) {
		for ( y = 0; y < s_rtLights.dims[1]; y++ ) {
			for ( x = 0; x < s_rtLights.dims[0]; x++, cell++ ) {
				vec3_t mins, maxs, center;
				int clusters[9];
				int numClusters = 0;
				uint32_t numCandidates = 0;
				uint32_t keep, l, entry, first, last;
				int c;

				mins[0] = s_rtLights.gridOrigin[0] + x * cellSize;
				mins[1] = s_rtLights.gridOrigin[1] + y * cellSize;
				mins[2] = s_rtLights.gridOrigin[2] + z * cellSize;
				maxs[0] = mins[0] + cellSize;
				maxs[1] = mins[1] + cellSize;
				maxs[2] = mins[2] + cellSize;
				VectorAdd( mins, maxs, center );
				VectorScale( center, 0.5f, center );

				// the clusters the cell's centre and inset corners are in
				for ( c = 0; c < 9; c++ ) {
					vec3_t p;
					int cl;
					if ( c == 0 ) {
						VectorCopy( center, p );
					} else {
						p[0] = ( c & 1 ) ? maxs[0] - 1.0f : mins[0] + 1.0f;
						p[1] = ( c & 2 ) ? maxs[1] - 1.0f : mins[1] + 1.0f;
						p[2] = ( c & 4 ) ? maxs[2] - 1.0f : mins[2] + 1.0f;
					}
					cl = RT_PointCluster( p );
					if ( cl >= 0 ) {
						int k;
						for ( k = 0; k < numClusters && clusters[k] != cl; k++ ) {
						}
						if ( k == numClusters ) {
							clusters[numClusters++] = cl;
						}
					}
				}

				first = listed ? listStart[cell] : 0;
				last = listed ? listStart[cell + 1] : s_rtLights.numLights;
				for ( entry = first; entry < last; entry++ ) {
					float reach, d2;
					qboolean visible;

					l = listed ? listed[entry] : entry;
					reach = s_rtLights.reach[l];
					d2 = RT_BoxDistanceSquared( s_rtLights.center[l], mins, maxs );

					if ( d2 > reach * reach ) {
						continue;
					}
					visible = numClusters == 0 ? qtrue : qfalse;
					for ( c = 0; c < numClusters && !visible; c++ ) {
						visible = RT_ClusterSees( clusters[c], s_rtLights.cluster[l] );
					}
					if ( !visible ) {
						continue;
					}
					candidates[numCandidates].light = l;
					candidates[numCandidates].importance = RT_LightImportance( &s_rtLights.lights[l], d2 );
					numCandidates++;
				}

				if ( numCandidates > RT_LIGHT_CELL_MAX ) {
					qsort( candidates, numCandidates, sizeof( *candidates ), RT_CompareCandidates );
				}
				keep = MIN( numCandidates, (uint32_t)RT_LIGHT_CELL_MAX );
				if ( numCandidates > keep ) {
					s_rtLights.overflowCells++;
					s_rtLights.overflowEntries += numCandidates - keep;
				}
				for ( l = 0; l < numCandidates; l++ ) {
					s_rtLights.candidateImportance += candidates[l].importance;
					if ( l >= keep ) s_rtLights.overflowImportance += candidates[l].importance;
				}
				if ( s_rtLights.numIndices + keep > capacity ) {
					uint32_t *grown;
					while ( s_rtLights.numIndices + keep > capacity ) {
						capacity *= 2;
					}
					grown = RT_HostAlloc( (int)( capacity * sizeof( uint32_t ) ) );
					if ( !grown ) {
						RT_HostFree( candidates );
						if ( listed ) {
							RT_HostFree( listed );
							RT_HostFree( listStart );
						}
						return qfalse;
					}
					Com_Memcpy( grown, s_rtLights.indices, s_rtLights.numIndices * sizeof( uint32_t ) );
					RT_HostFree( s_rtLights.indices );
					s_rtLights.indices = grown;
				}
				s_rtLights.cells[cell * 2 + 0] = s_rtLights.numIndices;
				s_rtLights.cells[cell * 2 + 1] = keep;
				for ( l = 0; l < keep; l++ ) {
					s_rtLights.indices[ s_rtLights.numIndices++ ] = candidates[l].light;
				}
				/* Membership/PDF queries happen for every globally drawn GPU
				 * candidate. Sort the retained IDs once here for bounded binary
				 * search; selection is still uniform over this same top-K set. */
				for ( l = 1; l < keep; ++l ) {
					uint32_t *ids = &s_rtLights.indices[s_rtLights.cells[cell * 2]];
					uint32_t key = ids[l], j = l;
					while ( j && ids[j - 1] > key ) { ids[j] = ids[j - 1]; --j; }
					ids[j] = key;
				}
			}
		}
	}
	RT_HostFree( candidates );
	if ( listed ) {
		RT_HostFree( listed );
		RT_HostFree( listStart );
	}
	return qtrue;
}

/*
==================
Emitters a reflection meets
==================

A glossy reflection ray that meets a q3map surface light weighs that light
against the light sampler (multiple importance sampling, rt_path.glsl). The
shaders find the light from the world triangle the ray hit: map[t] is the
light world triangle t (in the partitioned index buffer) emits as, ~0 for
none. A world triangle and its light share a centroid, computed the same way
(RT_TriangleCentroid), so they are matched by it, among the first `traced`
triangles: the filter partition, which follows them, is never met by a
reflection. A light a reflection can meet is marked RT_LIGHT_TRACED; the
sampler weighs only those against reflections. NULL when out of memory.
*/
static uint32_t RT_CentroidHash( const vec3_t centroid )
{
	uint32_t bits[3], h = 2166136261u;
	int i;

	Com_Memcpy( bits, centroid, sizeof( bits ) );
	for ( i = 0; i < 3; i++ ) {
		h = ( h ^ bits[i] ) * 16777619u;
		h ^= h >> 15;
	}
	return h;
}

static uint32_t *RT_MapPrimitiveLights( const rtxRtCpuGeometry_t *geometry, uint32_t traced )
{
	const uint32_t triangles = geometry->numIndices / 3u;
	uint32_t *map, *table;
	uint32_t size = 16, i, t, slot;

	map = RT_HostAlloc( (int)( MAX( triangles, 1u ) * sizeof( uint32_t ) ) );
	if ( !map ) {
		return NULL;
	}
	Com_Memset( map, 0xFF, MAX( triangles, 1u ) * sizeof( uint32_t ) );
	if ( !s_rtLights.triangleLights ) {
		return map;
	}
	while ( size < s_rtLights.triangleLights * 2u ) {
		size <<= 1;
	}
	table = RT_HostAlloc( (int)( size * sizeof( uint32_t ) ) );
	if ( !table ) {
		RT_HostFree( map );
		return NULL;
	}
	Com_Memset( table, 0xFF, size * sizeof( uint32_t ) );
	for ( i = 0; i < s_rtLights.numLights; i++ ) {
		if ( ( (uint32_t)s_rtLights.lights[i].color[3] & RT_LIGHT_TYPE_MASK ) != RT_LIGHT_TRIANGLE ) {
			continue;
		}
		for ( slot = RT_CentroidHash( s_rtLights.center[i] ) & ( size - 1 ); table[slot] != ~0u;
			slot = ( slot + 1 ) & ( size - 1 ) ) {
		}
		table[slot] = i;
	}
	for ( t = 0; t < triangles && t < traced; t++ ) {
		const float *a = geometry->vertices[ geometry->indices[t * 3 + 0] ].xyz;
		const float *b = geometry->vertices[ geometry->indices[t * 3 + 1] ].xyz;
		const float *c = geometry->vertices[ geometry->indices[t * 3 + 2] ].xyz;
		vec3_t centroid;

		RT_TriangleCentroid( a, b, c, centroid );
		for ( slot = RT_CentroidHash( centroid ) & ( size - 1 ); table[slot] != ~0u; slot = ( slot + 1 ) & ( size - 1 ) ) {
			const uint32_t light = table[slot];
			if ( !memcmp( s_rtLights.center[light], centroid, sizeof( vec3_t ) ) ) {
				map[t] = light;
				s_rtLights.lights[light].color[3] =
					(float)( (uint32_t)s_rtLights.lights[light].color[3] | RT_LIGHT_TRACED );
				break;
			}
		}
	}
	RT_HostFree( table );
	// the lights' flags changed: every frame copies them again
	s_rtLights.generation++;
	return map;
}

/*
The unshadowed irradiance one light delivers at P to an upward surface, one
colour channel, by rt_lighting.glsl's models (a triangle is integrated over
a 32 x 32 grid of sample points). A diagnostic.
*/
static float RT_LightUnshadowed( const rtGpuLight_t *light, const vec3_t P, int channel )
{
	const uint32_t type = (uint32_t)light->color[3] & RT_LIGHT_TYPE_MASK;
	const vec3_t N = { 0.0f, 0.0f, 1.0f };
	vec3_t d;
	float dist, cosR;

	if ( type == RT_LIGHT_TRIANGLE ) {
		const int steps = 32;
		vec3_t n;
		float sum = 0.0f;
		int i, j;

		CrossProduct( light->dir, light->extra, n );
		VectorNormalize( n );
		for ( i = 0; i < steps; i++ ) {
			for ( j = 0; j < steps - i; j++ ) {
				const float u = ( i + 1.0f / 3.0f ) / steps;
				const float v = ( j + 1.0f / 3.0f ) / steps;
				vec3_t q;
				float d2, cosL;
				VectorMA( light->position, u, light->dir, q );
				VectorMA( q, v, light->extra, q );
				VectorSubtract( q, P, d );
				d2 = DotProduct( d, d );
				dist = sqrtf( d2 );
				if ( dist < 1e-3f ) {
					continue;
				}
				VectorScale( d, 1.0f / dist, d );
				cosR = DotProduct( N, d );
				cosL = -DotProduct( n, d );
				if ( light->extra[3] > 0.5f ) {
					cosL = fabsf( cosL );
				}
				if ( cosR > 0.0f && cosL > 0.0f ) {
					sum += cosR * cosL / MAX( d2, 16.0f );
				}
			}
		}
		// each sample stands for 2 / steps^2 of the area
		return light->color[channel] * sum * light->dir[3] * 2.0f / (float)( steps * steps );
	}

	VectorSubtract( light->position, P, d );
	dist = VectorNormalize( d );
	if ( dist < 1e-3f || dist > light->position[3] ) {
		return 0.0f;
	}
	cosR = DotProduct( N, d );
	if ( cosR <= 0.0f ) {
		return 0.0f;
	}
	if ( light->extra[3] > 0.5f && type != RT_LIGHT_SPOT ) {
		cosR = 1.0f;	// a light without the cosine
	}
	if ( type == RT_LIGHT_POINT_LINEAR ) {
		return light->color[channel] * MAX( cosR * light->extra[1] - dist * light->extra[2], 0.0f );
	}
	{
		const float dc = MAX( dist, light->extra[0] );
		float e = light->color[channel] * cosR / ( dc * dc );
		if ( type == RT_LIGHT_SPOT ) {
			vec3_t toPoint, radial;
			float along, radiusAtDist, sampleRadius;
			VectorSubtract( P, light->position, toPoint );
			along = DotProduct( toPoint, light->dir );
			if ( along <= 0.0f ) {
				return 0.0f;
			}
			radiusAtDist = light->dir[3] * along;
			VectorMA( toPoint, -along, light->dir, radial );
			sampleRadius = VectorLength( radial );
			if ( sampleRadius >= radiusAtDist ) {
				return 0.0f;
			}
			e *= Com_Clamp( 0.0f, 1.0f, ( radiusAtDist - sampleRadius ) / 32.0f );
		}
		return e * Com_Clamp( 0.0f, 1.0f, ( light->position[3] - dist ) / ( 0.2f * light->position[3] ) );
	}
}

/*
==================
vk_rt_lightinfo_f

rtx_rt_lightinfo [x y z]: the light-grid cell at a point (by default the
last view's origin) and the lights a shading point there samples among.
==================
*/
void vk_rt_lightinfo_f( void )
{
	static const char *const typeNames[] = { "point", "linear", "spot", "triangle", "dlight" };
	vec3_t p;
	int c[3];
	int i;
	uint32_t cell, first, count, n;

	if ( !s_rtLights.cells || !s_rtLights.lights ) {
		ri.Printf( PRINT_ALL, "rtx_rt_lightinfo: no traced lights (rtx_rt_mode 2 with a map loaded)\n" );
		return;
	}
	if ( ri.Cmd_Argc() >= 4 ) {
		for ( i = 0; i < 3; i++ ) {
			p[i] = Q_atof( ri.Cmd_Argv( i + 1 ) );
		}
	} else {
		VectorCopy( tr.viewParms.or.origin, p );
	}
	for ( i = 0; i < 3; i++ ) {
		c[i] = (int)floorf( ( p[i] - s_rtLights.gridOrigin[i] ) / s_rtLights.cellSize );
		c[i] = Com_Clamp( 0, s_rtLights.dims[i] - 1, c[i] );
	}
	cell = ( (uint32_t)c[2] * (uint32_t)s_rtLights.dims[1] + (uint32_t)c[1] ) * (uint32_t)s_rtLights.dims[0] + (uint32_t)c[0];
	first = s_rtLights.cells[cell * 2 + 0];
	count = s_rtLights.cells[cell * 2 + 1];
	ri.Printf( PRINT_ALL, "(%.0f %.0f %.0f) cluster %d, cell %d %d %d: %u of %u lights; ambient %.3f %.3f %.3f\n",
		p[0], p[1], p[2], RT_PointCluster( p ), c[0], c[1], c[2], count, s_rtLights.numLights,
		s_rtLights.ambient[0], s_rtLights.ambient[1], s_rtLights.ambient[2] );
	for ( n = 0; n < count; n++ ) {
		const uint32_t index = s_rtLights.indices[first + n];
		const rtGpuLight_t *light = &s_rtLights.lights[index];
		const uint32_t word = (uint32_t)light->color[3];
		const uint32_t type = word & RT_LIGHT_TYPE_MASK;
		const float d2 = DistanceSquared( s_rtLights.center[index], p );
		ri.Printf( PRINT_ALL, "  %4u %-8s%s at (%.0f %.0f %.0f) cluster %d reach %.0f colour %.4f %.4f %.4f importance %.5f"
			" E(up) %.4f %.4f %.4f\n",
			index, type < ARRAY_LEN( typeNames ) ? typeNames[type] : "?",
			( word & RT_LIGHT_NO_SURFACES ) ? " (models only)" : ( word & RT_LIGHT_NO_MODELS ) ? " (surfaces only)" : "",
			s_rtLights.center[index][0], s_rtLights.center[index][1], s_rtLights.center[index][2],
			s_rtLights.cluster[index], s_rtLights.reach[index],
			light->color[0], light->color[1], light->color[2], RT_LightImportance( light, d2 ),
			RT_LightUnshadowed( light, p, 0 ), RT_LightUnshadowed( light, p, 1 ), RT_LightUnshadowed( light, p, 2 ) );
	}
}

/*
The map's sun, as its compiler found it. id's q3map lights with the q3map_sun
of the first draw surface whose shader is sky (surfaceparm sky, whatever its
skyparms), its colour scaled to unit length, and none if that shader has none.
q3map2 lights with the sun of every draw surface's shader that has one (the
trace keeps the first), its colour scaled so its largest component is 1,
unless worldspawn sets _noshadersun. Return to Castle Wolfenstein's compiler
picks its sun as id's does but keeps the largest component at 1, as its
lightmaps show; so do Quake Live's and Enemy Territory's maps, all q3map2's
(R_SkySunUnitLengthColour).
*/
static void RT_FindSun( void )
{
	int i;

	for ( i = 0; i < tr.world->numsurfaces; i++ ) {
		const shader_t *shader = tr.world->surfaces[i].shader;
		vec3_t color;
		float length;

		if ( !shader ) {
			continue;
		}
		if ( s_rtLights.q3map2 ) {
			if ( s_rtLights.noShaderSun ) {
				return;
			}
			if ( !shader->rtSunValid ) {
				continue;
			}
		} else if ( !RT_ShaderIsSky( shader ) ) {
			continue;
		} else if ( !shader->rtSunValid ) {
			ri.Printf( PRINT_DEVELOPER, "RTX RT: the first sky surface's shader %s has no q3map_sun\n", shader->name );
			return;
		}
		VectorCopy( shader->rtSunColor, color );
		if ( R_SkySunUnitLengthColour( ri.gameId, s_rtLights.q3map2 ) ) {
			length = VectorLength( color );
			if ( !( length > 0.0f ) ) {
				return;
			}
			VectorScale( color, 1.0f / length, color );
		}
		VectorScale( color, shader->rtSunIntensity, s_rtLights.sunLight );
		if ( !RT_Vec3Valid( s_rtLights.sunLight, RT_LIGHT_MAX_VALUE ) ||
			!RT_Vec3Valid( shader->rtSunDirection, RT_LIGHT_MAX_COORD ) ||
			VectorNormalize2( shader->rtSunDirection, s_rtLights.sunDirection ) == 0.0f ||
			!( VectorLengthSquared( s_rtLights.sunLight ) > 0.0f ) ) {
			VectorClear( s_rtLights.sunLight );
			return;
		}
		s_rtLights.sunValid = qtrue;
		// q3map2's extended sun: as many suns, the first in the sun's own
		// direction, the rest spread over the deviance (CreateSunLight)
		if ( shader->rtSunSamples > 1 && shader->rtSunDeviance > 0.0f &&
			RT_FloatValid( shader->rtSunDeviance, 180.0f ) ) {
			s_rtLights.sunDeviance = DEG2RAD( shader->rtSunDeviance );
			s_rtLights.sunSamples = shader->rtSunSamples;
		}
		return;
	}
}

/*
Medal of Honor's sun, which its maps give in worldspawn and none of its
shaders does (docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md): `suncolor`,
or `sunlight` under its other name, is the light 2015's compiler stored in the
lightmaps times the cosine to the sun -- a q3map sun's colour times its
intensity -- and `sundirection` the angles that point toward it. A sun
without a direction lights nothing; one without a colour takes 70, as 2015's
renderer does. `sundiffuse` (1 when absent; the lightmaps hold no more for a
larger one) of `sundiffusecolor` (the sun's colour when absent) is what an
open level floor stored from the sky. Only the worldspawn's keys count, each
matched exactly as the renderer matches them, and a repeated key keeps its
last value.
*/
#define RT_MOHAA_SUN_DEFAULT	70.0f
#define RT_MOHAA_SKY_ITERATIONS	8

typedef struct {
	vec3_t		light;		// lightmap bytes, facing the sun
	vec3_t		direction;	// toward the sun
	vec3_t		diffuse;	// lightmap bytes, an open level floor
} rtMOHAASun_t;

static qboolean RT_ParseMOHAASun( const char *entities, rtMOHAASun_t *sun )
{
	const char *parse = entities;
	vec3_t color, diffuseColor, angles;
	float diffuse = 1.0f;
	qboolean hasDirection = qfalse, hasDiffuseColor = qfalse;
	const char *token;

	Com_Memset( sun, 0, sizeof( *sun ) );
	VectorSet( color, RT_MOHAA_SUN_DEFAULT, RT_MOHAA_SUN_DEFAULT, RT_MOHAA_SUN_DEFAULT );
	VectorClear( diffuseColor );
	VectorClear( angles );
	if ( !parse ) {
		return qfalse;
	}
	token = COM_ParseExt( &parse, qtrue );
	if ( token[0] != '{' ) {
		return qfalse;
	}
	while ( 1 ) {
		char key[64];
		vec3_t v;
		token = COM_ParseExt( &parse, qtrue );
		if ( !token[0] || token[0] == '}' ) {
			break;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_ParseExt( &parse, qtrue );
		if ( !token[0] || token[0] == '}' ) {
			break;
		}
		// a value that is not three numbers leaves the sun without that key
		if ( !strcmp( key, "suncolor" ) || !strcmp( key, "sunlight" ) ) {
			if ( sscanf( token, "%f %f %f", &v[0], &v[1], &v[2] ) == 3 && RT_Vec3Valid( v, RT_LIGHT_MAX_VALUE ) ) {
				VectorCopy( v, color );
			} else {
				VectorSet( color, RT_MOHAA_SUN_DEFAULT, RT_MOHAA_SUN_DEFAULT, RT_MOHAA_SUN_DEFAULT );
			}
		} else if ( !strcmp( key, "sundirection" ) ) {
			hasDirection = ( sscanf( token, "%f %f %f", &v[0], &v[1], &v[2] ) == 3 && RT_Vec3Valid( v, RT_LIGHT_MAX_COORD ) ) ?
				qtrue : qfalse;
			if ( hasDirection ) {
				VectorCopy( v, angles );
			}
		} else if ( !strcmp( key, "sundiffuse" ) ) {
			diffuse = Q_atof( token );
		} else if ( !strcmp( key, "sundiffusecolor" ) ) {
			hasDiffuseColor = ( sscanf( token, "%f %f %f", &v[0], &v[1], &v[2] ) == 3 &&
				RT_Vec3Valid( v, RT_LIGHT_MAX_VALUE ) ) ? qtrue : qfalse;
			if ( hasDiffuseColor ) {
				VectorCopy( v, diffuseColor );
			}
		}
	}
	if ( !hasDirection ) {
		return qfalse;
	}
	AngleVectors( angles, sun->direction, NULL, NULL );
	if ( VectorNormalize( sun->direction ) == 0.0f ) {
		return qfalse;
	}
	sun->light[0] = MAX( color[0], 0.0f );
	sun->light[1] = MAX( color[1], 0.0f );
	sun->light[2] = MAX( color[2], 0.0f );
	if ( !RT_FloatValid( diffuse, RT_LIGHT_MAX_VALUE ) ) {
		diffuse = 0.0f;
	}
	diffuse = Com_Clamp( 0.0f, 1.0f, diffuse );
	if ( hasDiffuseColor ) {
		sun->diffuse[0] = MAX( diffuseColor[0], 0.0f );
		sun->diffuse[1] = MAX( diffuseColor[1], 0.0f );
		sun->diffuse[2] = MAX( diffuseColor[2], 0.0f );
		VectorScale( sun->diffuse, diffuse, sun->diffuse );
	} else {
		VectorScale( sun->light, diffuse, sun->diffuse );
	}
	return VectorLengthSquared( sun->light ) > 0.0f || VectorLengthSquared( sun->diffuse ) > 0.0f;
}

/*
What a level surface under the whole sky takes of a dome's value: the mean
sine of its suns' elevations (rt_sample_sky_light's dome), so the dome can
give that surface what was asked of it.
*/
static float RT_SkyDomeFloorShare( int iterations )
{
	const int rows = iterations - 1;
	double sum = 1.0;	// the sun overhead
	int row;

	for ( row = 0; row < rows; row++ ) {
		sum += 4.0 * rows * sin( ( 0.5 * M_PI / iterations ) * ( row + 0.5 ) );
	}
	return (float)( sum / ( 4.0 * rows * rows + 1.0 ) );
}

// The worldspawn sun of a Medal of Honor map, and its diffuse sky as a dome
// of suns (RT_FindSkyLight's) that gives an open floor what it stored, or
// the emitters' even sky.
static void RT_FindMOHAASun( void )
{
	rtMOHAASun_t sun;

	if ( !tr.world || !RT_ParseMOHAASun( tr.world->entityString, &sun ) ) {
		ri.Printf( PRINT_DEVELOPER, "RTX RT: the worldspawn names no sun\n" );
		return;
	}
	if ( VectorLengthSquared( sun.light ) > 0.0f ) {
		VectorCopy( sun.light, s_rtLights.sunLight );
		VectorCopy( sun.direction, s_rtLights.sunDirection );
		s_rtLights.sunValid = qtrue;
	}
	if ( VectorLengthSquared( sun.diffuse ) > 0.0f ) {
		// the emitters' even sky asks for it (RT_FindSkyLightColor)
		VectorCopy( sun.diffuse, s_rtLights.skyDiffuse );
		if ( s_rtLights.mode != RT_LIGHTING_EMITTERS ) {
			VectorMA( s_rtLights.skyLight, 1.0f / RT_SkyDomeFloorShare( RT_MOHAA_SKY_ITERATIONS ), sun.diffuse,
				s_rtLights.skyLight );
			s_rtLights.skyLightIterations = MAX( s_rtLights.skyLightIterations, RT_MOHAA_SKY_ITERATIONS );
		}
	}
}

/*
Alice's and F.A.K.K.2's sun, which their maps give in their entities and no
sky of theirs does (docs/fntech3/2026-10-22-fnq-shadow-formats.md):
`suncolor` and `sundirection` from anywhere in the entity text, the last of
each winning, as the engines' model light reads them (RitualModelLight_Keys),
the angles' forward vector pointing toward the sun. [I] Their compiler
stored the sun as Medal of Honor's did, `suncolor` times the cosine in
lightmap bytes: between opposite walls the sunward one is brighter by about
that on `fakkhouse`, `gruff`, `end` and `homes1` (1.0 to 1.3 times it), and
by less on maps whose sunward walls stand in shade. A sun at or below the
horizon -- F.A.K.K.2's `otto` -- lights nothing,
as the sun shadows take none. A level that names no sun keeps its sky's, if
any (RT_FindSun).
*/
static qboolean RT_FindRitualSun( void )
{
	ritualModelLights_t keys;
	vec3_t direction;
	int i;

	if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) || !tr.world || !tr.world->entityString ) {
		return qfalse;
	}
	Com_Memset( &keys, 0, sizeof( keys ) );
	RitualModelLight_Keys( tr.world->entityString, (int)strlen( tr.world->entityString ), &keys );
	if ( !keys.sun || !RT_Vec3Valid( keys.sunColor, RT_LIGHT_MAX_VALUE ) ||
		!RT_Vec3Valid( keys.sunDirection, 2.0f ) ||
		VectorNormalize2( keys.sunDirection, direction ) == 0.0f || !( direction[2] > 0.0f ) ) {
		return qfalse;
	}
	for ( i = 0; i < 3; i++ ) {
		s_rtLights.sunLight[i] = MAX( keys.sunColor[i], 0.0f );
	}
	if ( !( VectorLengthSquared( s_rtLights.sunLight ) > 0.0f ) ) {
		VectorClear( s_rtLights.sunLight );
		return qfalse;
	}
	VectorCopy( direction, s_rtLights.sunDirection );
	s_rtLights.sunValid = qtrue;
	s_rtLights.entitySun = qtrue;
	return qtrue;
}

/*
A map whose shaders use what only q3map2 reads was compiled by it, whatever
its worldspawn says: q3map_sunExt, which id's q3map would not have lit with,
and q3map_skyLight.
*/
static qboolean RT_WorldUsesQ3map2Shaders( void )
{
	int i;

	for ( i = 0; i < tr.world->numsurfaces; i++ ) {
		const shader_t *shader = tr.world->surfaces[i].shader;
		if ( shader && ( ( shader->rtSunValid && shader->rtSunExtended ) || shader->rtSkyLightValue > 0.0f ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
q3map2's sky light (q3map_skyLight value iterations): each shader of the
map's surfaces that has one lights the map, once, with a dome of suns sharing
its value in its colour (light.c CreateSkyLights), whatever _noshadersun
says. The domes add up; the densest one's suns stand in for all of them.
*/
static void RT_FindSkyLight( void )
{
	const shader_t *seen[16];
	int numSeen = 0;
	int i, s;

	for ( i = 0; i < tr.world->numsurfaces; i++ ) {
		const shader_t *shader = tr.world->surfaces[i].shader;
		vec3_t color;

		if ( !shader || !( shader->rtSkyLightValue > 0.0f ) ||
			!RT_FloatValid( shader->rtSkyLightValue, RT_LIGHT_MAX_VALUE ) ) {
			continue;
		}
		for ( s = 0; s < numSeen && seen[s] != shader; s++ ) {
		}
		if ( s < numSeen ) {
			continue;
		}
		if ( numSeen == ARRAY_LEN( seen ) ) {
			ri.Printf( PRINT_DEVELOPER, "RTX RT: more than %d sky-light shaders; %s is left out\n",
				(int)ARRAY_LEN( seen ), shader->name );
			continue;
		}
		seen[numSeen++] = shader;
		if ( shader->rtEmitColorValid && RT_Vec3Valid( shader->rtEmitColor, RT_LIGHT_MAX_VALUE ) ) {
			VectorCopy( shader->rtEmitColor, color );
		} else {
			VectorSet( color, 1.0f, 1.0f, 1.0f );
		}
		VectorMA( s_rtLights.skyLight, shader->rtSkyLightValue, color, s_rtLights.skyLight );
		s_rtLights.skyLightIterations = MAX( s_rtLights.skyLightIterations,
			MIN( MAX( shader->rtSkyLightIterations, 2 ), 64 ) );
	}
	if ( !RT_Vec3Valid( s_rtLights.skyLight, RT_LIGHT_MAX_VALUE ) || !( VectorLengthSquared( s_rtLights.skyLight ) > 0.0f ) ) {
		VectorClear( s_rtLights.skyLight );
		s_rtLights.skyLightIterations = 0;
	}
}

/*
How the sky lights the scene where the sky and its emitters light it
(rt_sample_sky_light). A sky shader that asks for light - id's q3map lit a
map's sky from its q3map_surfacelight, q3map2 also from q3map_skyLight -
lights as the compilers did: evenly, in that light's colour. Under a whole
sky either gave a level surface about value * 0.75 (a surface light's form
factor), or two thirds of a sky light's value (its dome's mean cosine), in
lightmap units, and the lightmap held no more than one unit; the sky here
gives the lesser of that and RT_SKY_LIGHT_CEILING. A sky that asks for
nothing lights with its own images as they are.
out: the even sky's colour (out[3] 1), or the gain on the images (out[3] 0).
*/
#define RT_SKY_LIGHT_CEILING	0.75f

static void RT_FindSkyLightColor( vec4_t out )
{
	const shader_t *sky = NULL;
	vec3_t wanted, color;
	float luminance;
	int i;

	VectorSet( out, 1.0f, 1.0f, 1.0f );
	out[3] = 0.0f;
	VectorClear( wanted );
	for ( i = 0; i < tr.world->numsurfaces; i++ ) {
		const shader_t *shader = tr.world->surfaces[i].shader;
		if ( !shader || !RT_ShaderIsSky( shader ) ) {
			continue;
		}
		if ( !sky ) {
			sky = shader;
		}
		if ( shader->rtEmitColorValid && RT_Vec3Valid( shader->rtEmitColor, RT_LIGHT_MAX_VALUE ) ) {
			VectorCopy( shader->rtEmitColor, color );
		} else {
			VectorSet( color, 1.0f, 1.0f, 1.0f );
		}
		if ( shader->surfaceLightValid && RT_FloatValid( shader->surfaceLight, RT_LIGHT_MAX_VALUE ) &&
			shader->surfaceLight > 0.0f ) {
			VectorMA( wanted, shader->surfaceLight * RT_LIGHT_AREA_SCALE * RT_LIGHT_FORM_FACTOR_SCALE *
				s_rtLights.scale, color, wanted );
		}
		if ( RT_FloatValid( shader->rtSkyLightValue, RT_LIGHT_MAX_VALUE ) && shader->rtSkyLightValue > 0.0f ) {
			VectorMA( wanted, shader->rtSkyLightValue * s_rtLights.scale * ( 2.0f / 3.0f ), color, wanted );
		}
	}
	// Medal of Honor's diffuse sun, which its worldspawn asks for
	VectorMA( wanted, s_rtLights.scale, s_rtLights.skyDiffuse, wanted );
	luminance = 0.2126f * wanted[0] + 0.7152f * wanted[1] + 0.0722f * wanted[2];
	if ( !sky || !( luminance > 1e-4f ) || !RT_Vec3Valid( wanted, RT_LIGHT_MAX_VALUE ) ) {
		return;
	}
	VectorScale( wanted, MIN( luminance, RT_SKY_LIGHT_CEILING ) / luminance, out );
	out[3] = 1.0f;
	ri.Printf( PRINT_DEVELOPER, "RTX RT: %s asks for sky light %.3f %.3f %.3f; it lights with %.3f %.3f %.3f\n",
		sky->name, wanted[0], wanted[1], wanted[2], out[0], out[1], out[2] );
}

// Rebuild the static lights and their grid for the loaded world.
static qboolean RT_BuildLights( void )
{
	const int shift = r_mapOverBrightBits->integer - tr.overbrightBits;
	const int64_t start = vk_rt_now_microseconds();

	RT_LightsFree();
	if ( !tr.world || !tr.world->bmodels ) {
		return qfalse;
	}
	s_rtLights.scale = ( shift >= 0 ? (float)( 1 << shift ) : 1.0f / (float)( 1 << -shift ) ) / 255.0f;
	s_rtLights.wolf = ( ri.gameId == COM_GAME_RTCW_SP || ri.gameId == COM_GAME_RTCW_MP ||
		ri.gameId == COM_GAME_WOLFET ) ? qtrue : qfalse;
	s_rtLights.q3map2Shaders = RT_WorldUsesQ3map2Shaders();
	s_rtLights.q3map2 = s_rtLights.q3map2Shaders;
	s_rtLights.mode = RT_LightingMode();
	RT_ResetImageAverages();

	if ( RT_LightingEmits( s_rtLights.mode ) ) {
		// The sky, its sun and the surfaces that give off light, nothing
		// baked. The level's lights add the light entities the map keeps, the
		// lights its designer placed and its compiler baked into the
		// lightmaps (Quake III's and RTCW's maps keep them; q3map2 left none
		// in Quake Live's and Enemy Territory's, which their glowing fixtures
		// light instead). The compilers' floors (ambient, _minlight) stood in
		// for bounced light, which is traced here.
		// The level's sky is its compiler's - q3map's sky surface lights,
		// q3map2's dome of suns - as bright as its lightmaps hold it; the
		// emitters alone light from an even sky in its light colour
		if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {
			RT_AddEntityLights();
			s_rtLights.lightsStripped = ( s_rtLights.q3map2 && s_rtLights.entityLights == 0 ) ? qtrue : qfalse;
			VectorClear( s_rtLights.ambient );
			VectorClear( s_rtLights.minLight );
			VectorClear( s_rtLights.minVertexLight );
			VectorClear( s_rtLights.minGridLight );
		}
		RT_AddSurfaceLights();
		RT_AddEmissiveSurfaces();
		if ( ri.gameId == COM_GAME_MOHAA ) {
			RT_FindMOHAASun();
		} else if ( !RT_FindRitualSun() ) {
			RT_FindSun();
		}
		if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {
			RT_FindSkyLight();
		} else {
			RT_FindSkyLightColor( s_rtLights.skyColor );
		}
	} else {
		RT_AddEntityLights();
		s_rtLights.lightsStripped = ( s_rtLights.q3map2 && s_rtLights.entityLights == 0 ) ? qtrue : qfalse;
		RT_AddSurfaceLights();
		if ( ri.gameId == COM_GAME_MOHAA ) {
			RT_FindMOHAASun();
		} else if ( !RT_FindRitualSun() ) {
			RT_FindSun();
		}
		RT_FindSkyLight();
	}
	if ( !RT_BuildLightGrid() ) {
		RT_LightsFree();
		return qfalse;
	}
	ri.Printf( PRINT_ALL, "RTX RT: %u lights (%u point, %u surface triangles, %s model), grid %dx%dx%d of %.0f units, "
		"%u list entries, %.1f ms\n",
		s_rtLights.numLights, s_rtLights.pointLights, s_rtLights.triangleLights,
		s_rtLights.wolf ? "Wolfenstein" : s_rtLights.q3map2 ? "q3map2" : "q3map",
		s_rtLights.dims[0], s_rtLights.dims[1], s_rtLights.dims[2], s_rtLights.cellSize, s_rtLights.numIndices,
		vk_rt_elapsed_ms( start, vk_rt_now_microseconds() ) );
	ri.Printf( PRINT_ALL, "RTX RT light grid: %u overflow cells, %llu omitted local entries, %.2f%% local importance; "
		"all %u lights retained by the weighted global proposal\n",
		s_rtLights.overflowCells, (unsigned long long)s_rtLights.overflowEntries,
		s_rtLights.candidateImportance > 0.0 ? 100.0 * s_rtLights.overflowImportance / s_rtLights.candidateImportance : 0.0,
		s_rtLights.numLights );
	if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {
		ri.Printf( PRINT_ALL, "RTX RT: the level's own lights: %u light entities and %u glowing shaders over %u triangles\n",
			s_rtLights.entityLights, s_rtLights.emissiveShaders, s_rtLights.emissiveTriangles );
	} else if ( s_rtLights.mode == RT_LIGHTING_EMITTERS ) {
		if ( s_rtLights.skyColor[3] > 0.5f ) {
			ri.Printf( PRINT_ALL, "RTX RT: lit by an even sky of %.2f %.2f %.2f and %u glowing shaders over %u triangles\n",
				s_rtLights.skyColor[0], s_rtLights.skyColor[1], s_rtLights.skyColor[2],
				s_rtLights.emissiveShaders, s_rtLights.emissiveTriangles );
		} else {
			ri.Printf( PRINT_ALL, "RTX RT: lit by the sky's own images and %u glowing shaders over %u triangles\n",
				s_rtLights.emissiveShaders, s_rtLights.emissiveTriangles );
		}
	}
	if ( s_rtLights.untargetedLights ) {
		ri.Printf( PRINT_ALL, "RTX RT: %u lights whose target names no entity left dark, as RTCW's compiler left them\n",
			s_rtLights.untargetedLights );
	}
	if ( s_rtLights.lightsStripped ) {
		ri.Printf( PRINT_ALL, "RTX RT: q3map2 kept none of the map's light entities; %s\n",
			s_rtLights.mode == RT_LIGHTING_COMPILERS ? "tracing the lights that remain" :
			s_rtLights.mode == RT_LIGHTING_LEVEL ? "its sky, sun and glowing surfaces light it" :
			"its lightmaps light the world" );
	}
	if ( s_rtLights.sunValid ) {
		ri.Printf( PRINT_ALL, "RTX RT: %s sun %.1f %.1f %.1f toward %.3f %.3f %.3f\n",
			ri.gameId == COM_GAME_MOHAA ? "worldspawn" : s_rtLights.entitySun ? "entity" :
			s_rtLights.q3map2 ? "q3map2" : "q3map",
			s_rtLights.sunLight[0], s_rtLights.sunLight[1], s_rtLights.sunLight[2],
			s_rtLights.sunDirection[0], s_rtLights.sunDirection[1], s_rtLights.sunDirection[2] );
		if ( s_rtLights.sunSamples > 1 ) {
			ri.Printf( PRINT_ALL, "RTX RT: the sun spreads over %.1f degrees in %d samples\n",
				RAD2DEG( s_rtLights.sunDeviance ), s_rtLights.sunSamples );
		}
	}
	if ( s_rtLights.skyLightIterations >= 2 ) {
		const int rows = s_rtLights.skyLightIterations - 1;
		ri.Printf( PRINT_ALL, "RTX RT: %s sky light %.1f %.1f %.1f from %d suns\n",
			ri.gameId == COM_GAME_MOHAA ? "worldspawn diffuse" : "q3map2",
			s_rtLights.skyLight[0], s_rtLights.skyLight[1], s_rtLights.skyLight[2], rows * rows * 4 + 1 );
	}
	return qtrue;
}
