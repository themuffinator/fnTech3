/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_MODEL_LIGHT_H
#define FNTECH3_TR_RITUAL_MODEL_LIGHT_H

/* Model light from a Ritual map's compiled lights.

American McGee's Alice and Heavy Metal: F.A.K.K.2 light most models from the
light grid, as Quake III does. An entity its game module flags RF_EXTRALIGHT
(0x200) or RF_INVISIBLE (0x8000000) is different: the renderer never samples
the grid for it, and lights it from the map's compiled lights instead. Both
games' modules set 0x200 in every sentient's constructor, beside RF_SHADOW
and RF_SHADOW_PRECISE, and every weapon's (Alice's fgamex86.dll 0x101387b0
and 0x10165e40, F.A.K.K.2's gamex86.dll 0x100fc040 and 0x1012a6d2), so the
player, every actor and every weapon are lit that way.

What follows is read from the retail executables: alice.exe (addresses
without a prefix) and the plain fakk2.exe 1.02 (addresses marked F). The
two differ only where noted. Nothing is taken from Ritual's SDK or any
reconstruction.

The map's data:
 - lump 17, one record a light: origin, colour, intensity, the leaf holding
   it, whether it needs a trace, whether it is a spot, the spot's direction
   and its radius by distance -- 56 bytes -- and in Alice a trailing cap, 60
   bytes (loaders 0x0049a360, F 0x004830c0). A lump that is not a whole
   number of records, or that holds more than 1,532 lights, is refused with
   ERR_DROP;
 - lump 18, for every leaf in order, the numbers of the lights that reach it,
   each list ended by -1. A lump of one terminator a leaf lists nothing
   (0x0049a4d0);
 - worldspawn's `ambientlight`, `suncolor` and `sundirection`, found anywhere
   in the entity text (0x004ac3b0); the direction is a set of angles.

The law, per flagged entity (0x004abc50, F 0x0048e170):
 1. The point it is lit at is the middle of its current frame, moved and
    scaled as for the grid, and its reach is half that frame's radius,
    scaled (0x004aaac0). The leaves at that point and at eight points that
    reach away from it -- up, down, along +X, +Y, -X, -Y, and diagonally along
    X+Y both ways -- are gathered, at most four distinct leaves with an area,
    since a solid leaf's area is -1 (0x004aaca0).
 2. Every one of six directions, +X, -X, +Y, -Y, +Z and -Z of the entity's
    own frame, starts from `ambientlight` (0 by default), plus the entity's
    shader colour when it is flagged RF_LIGHTOFFSET (0x8000), or 32 when it is
    flagged RF_MINLIGHT (0x20000) (0x004aaea0). A map without lights, or an
    entity with no leaf, is drawn at the identity light instead.
 3. The sun, when the map names one, adds its colour times the cosine to its
    direction to each direction facing it, if a point trace 16,384 units
    toward it stops on sky (0x004aa820). Its colour was shifted as it was
    read, as step 5 shifts the rest (0x004ac467, F 0x0048e98b).
 4. The lights of the gathered leaves' lists (every light when the map has
    no visibility), less those in an area the view cannot see (0x004ab000),
    are added one at a time (0x004ab850, F 0x0048dd80). A positive light
    skips an entity flagged RF_INVISIBLE; a negative one lights only such an
    entity. A light that needs a trace is dropped when a solid point trace
    from the lighting point to it is blocked. Then:
     - a point light gives power = |intensity| x K / d^2, with K 30000 in
       Alice (0x005004a0) and 7500 in F.A.K.K.2 (F 0x004d6050), d the distance
       from the lighting point. Power below 1 is dropped; Alice clamps it to
       a set cap. Each direction D takes power x colour x the cosine between
       D and the light seen from a point the reach behind the entity, along
       D: a near light spills onto the sides;
     - a spot (0x004ab460, F 0x0048d990) gives power = intensity x K / d^2,
       with no cap, dropped below 1. The entity must lie in front of it and
       inside its cone -- its radius by distance times the distance along
       its direction -- whose last 32 units fade out, and it lights the
       directions facing against it by the cosine. A negative radius by
       distance makes it a disc of that radius, facing along its direction,
       lit from the disc's point nearest the entity, under a cone of 1.25.
 5. The six are shifted as each game shifts its lightmaps and grid -- Alice
    by its overbright bits, 1 << 0 at its default (0x004abb80), F.A.K.K.2 by
    its map overbright bits less those, 1 << 2 at its defaults (F 0x0048e090,
    r_mapOverBrightBits at F 0x00d3e420) -- and a direction whose brightest
    channel then passes 255 is scaled back, hue kept. F.A.K.K.2's maps are
    lit for that: their `ambientlight` runs 2 to 4 and `suncolor` 11 to 56,
    where Alice's are 6 to 32 and 223 or 224.
 6. Retail then draws each vertex with the sum of three directions, weighted
    by its normal's components (0x004ac1c0), adds dynamic lights per vertex
    (0x004abdd0), and takes its light direction for shadows from the six
    directions' lengths (0x004abad0).

Steps 1-5 decide how much of the map's light reaches the entity and from
where, which is the map's data; RitualModelLight_Cube computes them. Step 6 is
retail's presentation, and fnTech3 keeps its own: RitualModelLight_Fold
reduces the six directions to the ambient and directed light and direction
every renderer already shades models with, by least squares over all
directions, taking step 6's direction. A single light is reproduced exactly;
several keep their mean. Retail's per-vertex dynamic lights, its
`r_light_*` debug cvars and its fast reciprocal square root are not
reproduced.

Documented in docs/fntech3/2026-09-25-ritual-model-lights.md. */

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* The profiles whose renderers draw Ritual's maps. It expands where it is
 * used, in a renderer that knows the profiles: Alice's, and F.A.K.K.2's
 * wherever the tree names the Ritual pair. */
#ifdef COM_GAME_ID_IS_RITUAL
#define RITUAL_ML_PROFILE( id ) COM_GAME_ID_IS_RITUAL( id )
#else
#define RITUAL_ML_PROFILE( id ) ( ( id ) == COM_GAME_ALICE )
#endif

#define RITUAL_ML_VERSION_ALICE			42
#define RITUAL_ML_VERSION_FAKK2			12
#define RITUAL_ML_RECORD_ALICE			60
#define RITUAL_ML_RECORD_FAKK2			56
#define RITUAL_ML_POWER_ALICE			30000.0f
#define RITUAL_ML_POWER_FAKK2			7500.0f
#define RITUAL_ML_MAX_LIGHTS			1532
#define RITUAL_ML_MAX_LEAVES			4
#define RITUAL_ML_SUN_DISTANCE			16384.0f
#define RITUAL_ML_SPOT_EDGE				32.0f
#define RITUAL_ML_DISC_CONE				1.25f
#define RITUAL_ML_MINLIGHT				32.0f
#define RITUAL_ML_FACE_MAX				255.0f

/* The module's render flags the law reads, at Ritual's bit positions. */
#define RITUAL_ML_RF_EXTRALIGHT			0x00000200
#define RITUAL_ML_RF_LIGHTOFFSET		0x00008000
#define RITUAL_ML_RF_MINLIGHT			0x00020000
#define RITUAL_ML_RF_FULLBRIGHT			0x00040000
#define RITUAL_ML_RF_INVISIBLE			0x08000000

/* The FAKK lump numbers read here (code/qcommon/bsp_fakk.h). */
#define RITUAL_ML_LUMP_LEAFS			8
#define RITUAL_ML_LUMP_ENTITIES			14
#define RITUAL_ML_LUMP_VISIBILITY		15
#define RITUAL_ML_LUMP_LIGHTS			17
#define RITUAL_ML_LUMP_LIGHTLISTS		18
#define RITUAL_ML_HEADER_LUMPS			20
#define RITUAL_ML_LEAF_BYTES			48

typedef struct {
	float origin[3];
	float color[3];
	float intensity;
	float spotDirection[3];
	float spotRadiusByDistance;
	float cap;						/* 0 for none, and always 0 in F.A.K.K.2 */
	int area;						/* its leaf's area; -1 for none */
	int needsTrace;
	int spot;
} ritualModelLight_t;

/* One position-independent block: the arrays follow the header at the
 * offsets it gives, so the block can be copied whole. */
typedef struct ritualModelLights_s {
	int size;						/* bytes, header included */
	int version;					/* 42 Alice, 12 F.A.K.K.2 */
	float power;					/* the point and spot law's constant */
	int numLights;
	int numLeafs;
	int leafLists;					/* 0: the map has no visibility, every light is a candidate */
	int numListed;
	float ambient[3];				/* `ambientlight` */
	int sun;						/* `suncolor` or `sundirection` named */
	float sunColor[3];
	float sunDirection[3];
	int ofsLights;					/* ritualModelLight_t[numLights] */
	int ofsLeafFirst;				/* int[numLeafs], into the listed lights */
	int ofsLeafCount;				/* int[numLeafs] */
	int ofsLeafArea;				/* int[numLeafs] */
	int ofsListed;					/* int[numListed], light numbers */
} ritualModelLights_t;

#define RITUAL_ML_AT( map, type, ofs ) ( (const type *)( (const unsigned char *)( map ) + ( map )->ofs ) )

/* The world the law asks about, answered by the renderer. */
typedef struct {
	void *context;
	/* The map's leaf holding point, or -1 when there is none. */
	int ( *leafAt )( void *context, const float point[3] );
	/* 1 when a point trace from start to end meets nothing solid. */
	int ( *clear )( void *context, const float start[3], const float end[3] );
	/* 1 when a point trace from start to end stops on sky. */
	int ( *sky )( void *context, const float start[3], const float end[3] );
	const unsigned char *areamask;	/* a set bit hides an area; may be NULL */
	int areamaskBytes;
} ritualModelLightWorld_t;

typedef struct {
	float origin[3];				/* the entity's origin */
	float axis[3][3];
	float point[3];					/* where it is lit, in the world */
	float localPoint[3];			/* the same point in its frame, scaled */
	float reach;					/* half its frame's radius, scaled */
	int renderfx;					/* its module flags, Ritual's bits */
	unsigned char lightOffset[3];	/* its shader colour, for RF_LIGHTOFFSET */
} ritualModelLightEntity_t;

typedef struct {
	int leaves;						/* distinct leaves with an area, up to four */
	int candidates;					/* lights their lists offered */
	int lit;						/* lights that reached a direction */
	int sun;						/* whether the sun did */
} ritualModelLightStats_t;

enum {
	RITUAL_ML_IDENTITY = 0,			/* no lights or no leaf: retail's identity light */
	RITUAL_ML_LIT = 1
};

/* The six directions, in retail's order (0x00500458). */
static const float ritualModelLightFaces[6][3] = {
	{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
};

/* The nine points leaves are gathered at, in retail's order, in units of the
 * reach (0x004aaca8..0x004aad7a). */
static const float ritualModelLightSamples[9][3] = {
	{ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 },
	{ 0, -1, 0 }, { 1, 1, 0 }, { -1, -1, 0 }, { 0, 0, -1 }
};

static inline float RitualModelLight_Dot( const float a[3], const float b[3] ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline int RitualModelLight_ReadInt( const unsigned char *p ) {
	return (int)( (unsigned int)p[0] | ( (unsigned int)p[1] << 8 ) | ( (unsigned int)p[2] << 16 )
		| ( (unsigned int)p[3] << 24 ) );
}

static inline float RitualModelLight_ReadFloat( const unsigned char *p ) {
	const unsigned int bits = (unsigned int)RitualModelLight_ReadInt( p );
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

static inline int RitualModelLight_FiniteBits( unsigned int bits ) {
	unsigned int limitBits;
	const float limit = 3.4e38f;
	/* These values originate in map bytes and entity text. Floating-point
	 * self-comparisons can disappear under a renderer's fast-math options.
	 * Inspect binary32 before applying the reader's existing finite bounds. */
	memcpy( &limitBits, &limit, sizeof( limitBits ) );
	return ( bits & 0x7fffffffU ) <= limitBits;
}

static inline int RitualModelLight_Finite( const float *input ) {
	unsigned int bits;
	memcpy( &bits, input, sizeof( bits ) );
	return RitualModelLight_FiniteBits( bits );
}

static inline void RitualModelLight_SetError( char *error, int errorSize, const char *text ) {
	if ( error && errorSize > 0 ) {
		size_t n = strlen( text );
		if ( n >= (size_t)errorSize ) n = (size_t)errorSize - 1;
		memcpy( error, text, n );
		error[n] = 0;
	}
}

/* A lump's bounds, or 0 when it lies outside the file. */
static inline int RitualModelLight_Lump( const unsigned char *file, int length, int lump, int *offset, int *size ) {
	const unsigned char *entry = file + 12 + 8 * lump;
	*offset = RitualModelLight_ReadInt( entry );
	*size = RitualModelLight_ReadInt( entry + 4 );
	return *offset >= 0 && *size >= 0 && *offset <= length && *size <= length - *offset;
}

/* The next token of an entity text, as the engine's parser reads it: quoted
 * strings, braces, words, skipping blanks and `//` comments. Returns 0 at the
 * end. */
static inline int RitualModelLight_Token( const char **cursor, const char *end, char *token, int tokenSize ) {
	const char *p = *cursor;
	int n = 0;
	for ( ;; ) {
		while ( p < end && (unsigned char)*p <= ' ' ) p++;
		if ( p + 1 < end && p[0] == '/' && p[1] == '/' ) {
			while ( p < end && *p != '\n' ) p++;
			continue;
		}
		break;
	}
	if ( p >= end || !*p ) {
		*cursor = p;
		return 0;
	}
	if ( *p == '"' ) {
		p++;
		while ( p < end && *p && *p != '"' ) {
			if ( n < tokenSize - 1 ) token[n++] = *p;
			p++;
		}
		if ( p < end && *p == '"' ) p++;
	} else {
		while ( p < end && (unsigned char)*p > ' ' ) {
			if ( n < tokenSize - 1 ) token[n++] = *p;
			p++;
		}
	}
	token[n] = 0;
	*cursor = p;
	return 1;
}

/* Up to three floats from a value, the rest left as they were. */
static inline void RitualModelLight_Floats( const char *text, float out[3] ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		char *next;
		const double value = strtod( text, &next );
		const float component = (float)value;
		if ( next == text ) break;
		/* Malformed numeric components keep the last valid value. This is an
		 * engine input-validation policy, not a retail presentation rule. */
		if ( RitualModelLight_Finite( &component ) ) out[i] = component;
		text = next;
	}
}

/* Angles to the direction they face, as AngleVectors gives forward. */
static inline void RitualModelLight_AngleForward( const float angles[3], float forward[3] ) {
	const double pitch = angles[0] * ( 3.14159265358979323846 / 180.0 );
	const double yaw = angles[1] * ( 3.14159265358979323846 / 180.0 );
	forward[0] = (float)( cos( pitch ) * cos( yaw ) );
	forward[1] = (float)( cos( pitch ) * sin( yaw ) );
	forward[2] = (float)( -sin( pitch ) );
}

/* The worldspawn keys retail reads for model light. It walks the whole entity
 * text a token at a time and takes the value after any matching token, so the
 * last match wins (0x004ac3b0). */
static inline void RitualModelLight_Keys( const char *text, int length, ritualModelLights_t *map ) {
	const char *cursor = text, *end = text + length;
	char token[1024];
	float sunAngles[3] = { 0, 0, 0 };
	int haveAngles = 0;
	while ( RitualModelLight_Token( &cursor, end, token, (int)sizeof( token ) ) ) {
		float *target = NULL;
		if ( !strcmp( token, "ambientlight" ) ) {
			target = map->ambient;
		} else if ( !strcmp( token, "suncolor" ) ) {
			target = map->sunColor;
			map->sun = 1;
		} else if ( !strcmp( token, "sundirection" ) ) {
			target = sunAngles;
			haveAngles = 1;
			map->sun = 1;
		}
		if ( !target ) continue;
		if ( !RitualModelLight_Token( &cursor, end, token, (int)sizeof( token ) ) ) break;
		RitualModelLight_Floats( token, target );
	}
	if ( haveAngles ) {
		RitualModelLight_AngleForward( sunAngles, map->sunDirection );
	}
}

/*
The compiled lights of a FAKK map, read from the map file itself: lumps 17
and 18, the leaves' areas from lump 8, whether it has visibility from lump 15
and worldspawn's keys from lump 14. Returns a block from allocFn, or NULL:
with an empty error for a file that is not a FAKK 42 or 12 map, and with the
reason for a malformed one -- a lump outside the file, a light lump that is
not a whole number of records or holds more than 1,532, a light list naming a
light or ending past the lump, or more lists than leaves. Retail refuses such
a map; the caller lights its models from the grid instead.
*/
static inline ritualModelLights_t *RitualModelLights_Parse( const unsigned char *file, int length,
	void *( *allocFn )( int size ), char *error, int errorSize ) {
	int version, record, i;
	int lightOfs, lightLen, listOfs, listLen, leafOfs, leafLen, entOfs, entLen, visOfs, visLen;
	int numLights, numLeafs, numInts, numListed = 0, size;
	const unsigned char *lists;
	ritualModelLights_t *map;
	ritualModelLight_t *lights;
	int *leafFirst, *leafCount, *leafArea, *listed;

	RitualModelLight_SetError( error, errorSize, "" );
	if ( !file || length < 12 + 8 * RITUAL_ML_HEADER_LUMPS || memcmp( file, "FAKK", 4 ) ) {
		return NULL;
	}
	version = RitualModelLight_ReadInt( file + 4 );
	if ( version == RITUAL_ML_VERSION_ALICE ) {
		record = RITUAL_ML_RECORD_ALICE;
	} else if ( version == RITUAL_ML_VERSION_FAKK2 ) {
		record = RITUAL_ML_RECORD_FAKK2;
	} else {
		return NULL;
	}
	if ( !RitualModelLight_Lump( file, length, RITUAL_ML_LUMP_LIGHTS, &lightOfs, &lightLen )
		|| !RitualModelLight_Lump( file, length, RITUAL_ML_LUMP_LIGHTLISTS, &listOfs, &listLen )
		|| !RitualModelLight_Lump( file, length, RITUAL_ML_LUMP_LEAFS, &leafOfs, &leafLen )
		|| !RitualModelLight_Lump( file, length, RITUAL_ML_LUMP_ENTITIES, &entOfs, &entLen )
		|| !RitualModelLight_Lump( file, length, RITUAL_ML_LUMP_VISIBILITY, &visOfs, &visLen ) ) {
		RitualModelLight_SetError( error, errorSize, "a lump lies outside the file" );
		return NULL;
	}
	if ( lightLen % record ) {
		RitualModelLight_SetError( error, errorSize, "the light lump is not a whole number of records" );
		return NULL;
	}
	numLights = lightLen / record;
	if ( numLights > RITUAL_ML_MAX_LIGHTS ) {
		RitualModelLight_SetError( error, errorSize, "the map has more than 1532 lights" );
		return NULL;
	}
	if ( leafLen % RITUAL_ML_LEAF_BYTES ) {
		RitualModelLight_SetError( error, errorSize, "the leaf lump is not a whole number of records" );
		return NULL;
	}
	numLeafs = leafLen / RITUAL_ML_LEAF_BYTES;
	if ( listLen % 4 ) {
		RitualModelLight_SetError( error, errorSize, "the light list lump is not a whole number of numbers" );
		return NULL;
	}
	numInts = listLen / 4;
	lists = file + listOfs;

	/* Count and check the lists first. One terminator a leaf lists nothing. */
	if ( numInts != numLeafs ) {
		int leaf = 0, pos = 0;
		while ( pos < numInts ) {
			if ( leaf >= numLeafs ) {
				RitualModelLight_SetError( error, errorSize, "the light lists outnumber the leaves" );
				return NULL;
			}
			for ( ;; ) {
				int n;
				if ( pos >= numInts ) {
					RitualModelLight_SetError( error, errorSize, "a light list runs past its lump" );
					return NULL;
				}
				n = RitualModelLight_ReadInt( lists + 4 * pos++ );
				if ( n == -1 ) break;
				if ( n < 0 || n >= numLights ) {
					RitualModelLight_SetError( error, errorSize, "a light list names no light" );
					return NULL;
				}
				numListed++;
			}
			leaf++;
		}
	}

	size = (int)sizeof( ritualModelLights_t );
	size = ( size + 7 ) & ~7;
	{
		const int ofsLights = size;
		const int ofsLeafFirst = ofsLights + numLights * (int)sizeof( ritualModelLight_t );
		const int ofsLeafCount = ofsLeafFirst + numLeafs * (int)sizeof( int );
		const int ofsLeafArea = ofsLeafCount + numLeafs * (int)sizeof( int );
		const int ofsListed = ofsLeafArea + numLeafs * (int)sizeof( int );
		size = ofsListed + numListed * (int)sizeof( int );
		map = (ritualModelLights_t *)allocFn( size );
		if ( !map ) {
			RitualModelLight_SetError( error, errorSize, "no memory for the lights" );
			return NULL;
		}
		memset( map, 0, (size_t)size );
		map->ofsLights = ofsLights;
		map->ofsLeafFirst = ofsLeafFirst;
		map->ofsLeafCount = ofsLeafCount;
		map->ofsLeafArea = ofsLeafArea;
		map->ofsListed = ofsListed;
	}
	map->size = size;
	map->version = version;
	map->power = version == RITUAL_ML_VERSION_ALICE ? RITUAL_ML_POWER_ALICE : RITUAL_ML_POWER_FAKK2;
	map->numLights = numLights;
	map->numLeafs = numLeafs;
	map->numListed = numListed;
	map->leafLists = visLen > 0;

	lights = (ritualModelLight_t *)( (unsigned char *)map + map->ofsLights );
	leafFirst = (int *)( (unsigned char *)map + map->ofsLeafFirst );
	leafCount = (int *)( (unsigned char *)map + map->ofsLeafCount );
	leafArea = (int *)( (unsigned char *)map + map->ofsLeafArea );
	listed = (int *)( (unsigned char *)map + map->ofsListed );

	for ( i = 0; i < numLeafs; i++ ) {
		leafArea[i] = RitualModelLight_ReadInt( file + leafOfs + RITUAL_ML_LEAF_BYTES * i + 4 );
		leafFirst[i] = 0;
		leafCount[i] = 0;
	}
	for ( i = 0; i < numLights; i++ ) {
		const unsigned char *r = file + lightOfs + record * i;
		ritualModelLight_t *light = &lights[i];
		int k, leaf;
		for ( k = 0; k < 3; k++ ) {
			light->origin[k] = RitualModelLight_ReadFloat( r + 4 * k );
			light->color[k] = RitualModelLight_ReadFloat( r + 12 + 4 * k );
			light->spotDirection[k] = RitualModelLight_ReadFloat( r + 40 + 4 * k );
		}
		light->intensity = RitualModelLight_ReadFloat( r + 24 );
		leaf = RitualModelLight_ReadInt( r + 28 );
		light->needsTrace = RitualModelLight_ReadInt( r + 32 ) != 0;
		light->spot = RitualModelLight_ReadInt( r + 36 ) != 0;
		light->spotRadiusByDistance = RitualModelLight_ReadFloat( r + 52 );
		light->cap = record == RITUAL_ML_RECORD_ALICE ? RitualModelLight_ReadFloat( r + 56 ) : 0.0f;
		/* A light naming no leaf is never hidden by the area mask. */
		light->area = leaf >= 0 && leaf < numLeafs ? leafArea[leaf] : -1;
		for ( k = 0; k < 3; k++ ) {
			/* Check the file bytes, before a floating-point return or
			 * assignment can carry finite-only compiler assumptions. */
			if ( !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 4 * k ) )
				|| !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 12 + 4 * k ) )
				|| !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 40 + 4 * k ) ) ) {
				light->intensity = 0.0f;
			}
		}
		if ( !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 24 ) )
			|| !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 52 ) )
			|| ( record == RITUAL_ML_RECORD_ALICE
				&& !RitualModelLight_FiniteBits( (unsigned int)RitualModelLight_ReadInt( r + 56 ) ) ) ) {
			light->intensity = 0.0f;
		}
	}
	if ( numInts != numLeafs ) {
		int leaf = 0, pos = 0, out = 0;
		while ( pos < numInts ) {
			leafFirst[leaf] = out;
			for ( ;; ) {
				const int n = RitualModelLight_ReadInt( lists + 4 * pos++ );
				if ( n == -1 ) break;
				listed[out++] = n;
			}
			leafCount[leaf] = out - leafFirst[leaf];
			leaf++;
		}
	}
	RitualModelLight_Keys( (const char *)file + entOfs, entLen, map );
	return map;
}

static inline void RitualModelLight_Add( float face[3], float weight, const float color[3] ) {
	face[0] += weight * color[0];
	face[1] += weight * color[1];
	face[2] += weight * color[2];
}

/* The light's position against the lighting point, in the entity's frame.
 * Retail measures it from the entity's origin and takes the lighting point's
 * own offset away, which is the same point unless the entity lights from a
 * separate origin (0x004ab8ea..0x004ab965). */
static inline void RitualModelLight_Local( const ritualModelLightEntity_t *e, const float world[3], float v[3] ) {
	const float delta[3] = { world[0] - e->origin[0], world[1] - e->origin[1], world[2] - e->origin[2] };
	int k;
	for ( k = 0; k < 3; k++ ) {
		v[k] = RitualModelLight_Dot( delta, e->axis[k] ) - e->localPoint[k];
	}
}

static inline int RitualModelLight_Point( const ritualModelLights_t *map, const ritualModelLight_t *light,
	const ritualModelLightEntity_t *e, float cube[6][3] ) {
	float v[3], d2, power;
	int i;
	RitualModelLight_Local( e, light->origin, v );
	d2 = RitualModelLight_Dot( v, v );
	if ( d2 < 1.0f ) d2 = 1.0f;
	power = (float)fabs( light->intensity ) * map->power / d2;
	if ( power < 1.0f ) return 0;
	if ( light->cap != 0.0f && power > light->cap ) power = light->cap;
	for ( i = 0; i < 6; i++ ) {
		const float *face = ritualModelLightFaces[i];
		const float t[3] = { v[0] + e->reach * face[0], v[1] + e->reach * face[1], v[2] + e->reach * face[2] };
		const float len = (float)sqrt( RitualModelLight_Dot( t, t ) );
		float w;
		if ( len <= 0.0f ) continue;
		w = RitualModelLight_Dot( t, face ) / len;
		if ( w < 0.0f ) continue;
		RitualModelLight_Add( cube[i], w * power, light->color );
	}
	return 1;
}

static inline int RitualModelLight_Spot( const ritualModelLights_t *map, const ritualModelLight_t *light,
	const ritualModelLightEntity_t *e, float cube[6][3] ) {
	float source[3], v[3], local[3], d2, power, along, cone;
	int i, k, added = 0;
	for ( k = 0; k < 3; k++ ) source[k] = light->origin[k];
	cone = light->spotRadiusByDistance;
	if ( cone < 0.0f ) {
		/* A disc: from its point nearest the lighting point's projection
		 * onto its plane, no farther than its radius (0x004ab47c..0x004ab55c). */
		float offset[3], length2, t;
		const float toLight[3] = { light->origin[0] - e->point[0], light->origin[1] - e->point[1],
			light->origin[2] - e->point[2] };
		t = RitualModelLight_Dot( toLight, light->spotDirection );
		for ( k = 0; k < 3; k++ ) offset[k] = e->point[k] + t * light->spotDirection[k] - light->origin[k];
		length2 = RitualModelLight_Dot( offset, offset );
		if ( length2 > cone * cone ) {
			const float s = -cone / (float)sqrt( length2 );
			for ( k = 0; k < 3; k++ ) offset[k] *= s;
		}
		for ( k = 0; k < 3; k++ ) source[k] += offset[k];
		cone = RITUAL_ML_DISC_CONE;
	}
	RitualModelLight_Local( e, source, v );
	d2 = RitualModelLight_Dot( v, v );
	if ( d2 < 1.0f ) d2 = 1.0f;
	power = light->intensity * map->power / d2;
	if ( power < 1.0f ) return 0;
	for ( k = 0; k < 3; k++ ) local[k] = RitualModelLight_Dot( light->spotDirection, e->axis[k] );
	along = -RitualModelLight_Dot( local, v );
	if ( along < -e->reach || along < 0.0f ) return 0;
	{
		const float q[3] = { v[0] + along * local[0], v[1] + along * local[1], v[2] + along * local[2] };
		const float offAxis = (float)sqrt( RitualModelLight_Dot( q, q ) );
		const float radius = along * cone;
		float fade;
		if ( offAxis >= radius ) return 0;
		fade = offAxis <= radius - RITUAL_ML_SPOT_EDGE ? 1.0f : ( radius - offAxis ) / RITUAL_ML_SPOT_EDGE;
		for ( i = 0; i < 6; i++ ) {
			const float facing = -RitualModelLight_Dot( local, ritualModelLightFaces[i] );
			const float w = fade * facing * power;
			if ( facing < 0.0f || w <= 0.0f ) continue;
			RitualModelLight_Add( cube[i], w, light->color );
			added = 1;
		}
	}
	return added;
}

/*
The six directions' light for an entity (steps 1-5 above): 255 is full.
shift is step 5's, the factor the renderer shifts the map's lightmaps and
grid by. Returns RITUAL_ML_LIT, or RITUAL_ML_IDENTITY when retail draws the
entity at its identity light (a map without lights, or no leaf with an area
around the entity), with each direction then 255 plus the entity's added
light, unshifted.
*/
static inline int RitualModelLight_Cube( const ritualModelLights_t *map, const ritualModelLightWorld_t *world,
	const ritualModelLightEntity_t *e, float shift, float cube[6][3], ritualModelLightStats_t *stats ) {
	const ritualModelLight_t *lights = RITUAL_ML_AT( map, ritualModelLight_t, ofsLights );
	const int *leafFirst = RITUAL_ML_AT( map, int, ofsLeafFirst );
	const int *leafCount = RITUAL_ML_AT( map, int, ofsLeafCount );
	const int *leafArea = RITUAL_ML_AT( map, int, ofsLeafArea );
	const int *listed = RITUAL_ML_AT( map, int, ofsListed );
	float added[3] = { 0, 0, 0 };
	int leaves[RITUAL_ML_MAX_LEAVES];
	unsigned char seen[RITUAL_ML_MAX_LIGHTS];
	int numLeaves = 0, i, j, k;

	memset( stats, 0, sizeof( *stats ) );
	if ( e->renderfx & RITUAL_ML_RF_LIGHTOFFSET ) {
		for ( k = 0; k < 3; k++ ) added[k] = e->lightOffset[k];
	} else if ( e->renderfx & RITUAL_ML_RF_MINLIGHT ) {
		added[0] = added[1] = added[2] = RITUAL_ML_MINLIGHT;
	}

	for ( i = 0; i < 9 && numLeaves < RITUAL_ML_MAX_LEAVES; i++ ) {
		float p[3];
		int leaf, known = 0;
		for ( k = 0; k < 3; k++ ) p[k] = e->point[k] + e->reach * ritualModelLightSamples[i][k];
		leaf = world->leafAt( world->context, p );
		if ( leaf < 0 || leaf >= map->numLeafs || leafArea[leaf] == -1 ) continue;
		for ( j = 0; j < numLeaves; j++ ) {
			if ( leaves[j] == leaf ) known = 1;
		}
		if ( !known ) leaves[numLeaves++] = leaf;
	}
	stats->leaves = numLeaves;

	if ( !map->numLights || !numLeaves ) {
		for ( i = 0; i < 6; i++ ) {
			for ( k = 0; k < 3; k++ ) cube[i][k] = RITUAL_ML_FACE_MAX + added[k];
		}
		return RITUAL_ML_IDENTITY;
	}
	for ( i = 0; i < 6; i++ ) {
		for ( k = 0; k < 3; k++ ) cube[i][k] = map->ambient[k] + added[k];
	}

	if ( map->sun ) {
		float end[3], local[3], length;
		for ( k = 0; k < 3; k++ ) end[k] = e->point[k] + RITUAL_ML_SUN_DISTANCE * map->sunDirection[k];
		if ( world->sky( world->context, e->point, end ) ) {
			for ( k = 0; k < 3; k++ ) local[k] = RitualModelLight_Dot( map->sunDirection, e->axis[k] );
			length = (float)sqrt( RitualModelLight_Dot( local, local ) );
			if ( length > 0.0f ) {
				for ( i = 0; i < 6; i++ ) {
					const float w = RitualModelLight_Dot( local, ritualModelLightFaces[i] ) / length;
					if ( w > 0.0f ) RitualModelLight_Add( cube[i], w * shift, map->sunColor );
				}
				stats->sun = 1;
			}
		}
	}

	/* With visibility, the gathered leaves' lists, less the lights of areas
	 * the view cannot see; without it, every light, unmasked (0x004ab000). */
	memset( seen, 0, (size_t)map->numLights );
	for ( j = 0; j < ( map->leafLists ? numLeaves : 1 ); j++ ) {
		const int count = map->leafLists ? leafCount[leaves[j]] : map->numLights;
		for ( i = 0; i < count; i++ ) {
			const int n = map->leafLists ? listed[leafFirst[leaves[j]] + i] : i;
			const ritualModelLight_t *light = &lights[n];
			const int invisible = ( e->renderfx & RITUAL_ML_RF_INVISIBLE ) != 0;
			int lit;
			if ( seen[n] ) continue;
			if ( map->leafLists && light->area >= 0 && world->areamask && ( light->area >> 3 ) < world->areamaskBytes
				&& ( world->areamask[light->area >> 3] & ( 1 << ( light->area & 7 ) ) ) ) {
				continue;
			}
			seen[n] = 1;
			stats->candidates++;
			/* A positive light never lights an invisible entity, and a
			 * negative one lights nothing else (0x004ab853..0x004ab88f). */
			if ( light->intensity > 0.0f ? invisible : !( light->intensity < 0.0f && invisible ) ) {
				continue;
			}
			if ( light->needsTrace && !world->clear( world->context, e->point, light->origin ) ) {
				continue;
			}
			lit = light->spot ? RitualModelLight_Spot( map, light, e, cube ) : RitualModelLight_Point( map, light, e, cube );
			stats->lit += lit;
		}
	}

	for ( i = 0; i < 6; i++ ) {
		float peak;
		for ( k = 0; k < 3; k++ ) cube[i][k] *= shift;
		peak = cube[i][0];
		if ( cube[i][1] > peak ) peak = cube[i][1];
		if ( cube[i][2] > peak ) peak = cube[i][2];
		if ( peak > RITUAL_ML_FACE_MAX ) {
			for ( k = 0; k < 3; k++ ) cube[i][k] *= RITUAL_ML_FACE_MAX / peak;
		}
	}
	return RITUAL_ML_LIT;
}

/* The mean over all directions of max(0, n.a) max(0, n.b), for unit a and b
 * at cosine c: ((pi - theta) c + sin theta) / (6 pi). */
static inline float RitualModelLight_LobeOverlap( float c ) {
	const double pi = 3.14159265358979323846;
	double theta;
	if ( c > 1.0f ) c = 1.0f;
	if ( c < -1.0f ) c = -1.0f;
	theta = acos( c );
	return (float)( ( ( pi - theta ) * c + sin( theta ) ) / ( 6.0 * pi ) );
}

/*
fnTech3's model light for six directions of light: an ambient term plus one
directional light, as every renderer shades a model (ambient + directed x
max(0, n.L)). The direction is retail's own, the six directions summed by
their lengths (0x004abad0), in the entity's frame; the entity's up when they
cancel. Retail draws a normal n with the sum of max(0, n.D) x the light of
each direction D, so its light is six clamped-cosine lobes, and ambient and
directed are fitted to that by least squares over all directions, per
channel. With <h> = 1/4 and <h^2> = 1/6 for h = max(0, n.L) over the sphere,
<c> = sum/4 and <ch> = S, the sum over directions of each one's light times
its lobe's overlap with L:

	ambient = 2/5 sum - 12/5 S, directed = 48/5 S - 3/5 sum

A single direction's light, or several along L, come back exactly; the
mean over all directions always does. A negative term is dropped and the
other refitted alone.
*/
static inline void RitualModelLight_Fold( const float cube[6][3], float ambient[3], float directed[3],
	float direction[3] ) {
	float sum[3] = { 0, 0, 0 }, overlap[6], length = 0.0f;
	int i, k;
	direction[0] = direction[1] = direction[2] = 0.0f;
	for ( i = 0; i < 6; i++ ) {
		const float magnitude = (float)sqrt( RitualModelLight_Dot( cube[i], cube[i] ) );
		for ( k = 0; k < 3; k++ ) direction[k] += magnitude * ritualModelLightFaces[i][k];
	}
	length = (float)sqrt( RitualModelLight_Dot( direction, direction ) );
	if ( length > 1e-4f ) {
		for ( k = 0; k < 3; k++ ) direction[k] /= length;
	} else {
		direction[0] = 0.0f;
		direction[1] = 0.0f;
		direction[2] = 1.0f;
	}
	for ( i = 0; i < 6; i++ ) {
		overlap[i] = RitualModelLight_LobeOverlap( RitualModelLight_Dot( direction, ritualModelLightFaces[i] ) );
		for ( k = 0; k < 3; k++ ) sum[k] += cube[i][k];
	}
	for ( k = 0; k < 3; k++ ) {
		float s = 0.0f, a, d;
		for ( i = 0; i < 6; i++ ) s += cube[i][k] * overlap[i];
		a = 0.4f * sum[k] - 2.4f * s;
		d = 9.6f * s - 0.6f * sum[k];
		if ( d < 0.0f ) {
			d = 0.0f;
			a = 0.25f * sum[k];
		} else if ( a < 0.0f ) {
			a = 0.0f;
			d = 6.0f * s;
		}
		ambient[k] = a;
		directed[k] = d;
	}
}

#endif
