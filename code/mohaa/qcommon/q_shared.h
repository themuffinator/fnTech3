/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_MOHAA_Q_SHARED_H
#define FNTECH3_MOHAA_Q_SHARED_H

/* The shared definitions OpenMoHAA's TIKI, skeletor and alias sources expect
 * from their own q_shared.h, on top of fnTech3's.
 *
 * Those sources are kept under code/mohaa/ with their upstream names and
 * licence headers (see code/mohaa/README.md), and include "q_shared.h" and
 * "qcommon.h" by bare name. Only their own translation units put this
 * directory ahead of the engine's on the include path; an engine file that
 * includes one of their headers by path reaches this file too, and nothing
 * here changes what the engine's own headers declare.
 *
 * Medal of Honor's paths are longer than Quake III's: its models and
 * animations are named up to 256 characters deep, and its TIKI records size
 * their names by that. The upstream sources' MAX_QPATH is spelled
 * MOHAA_MAX_QPATH here so that no engine record changes size. */

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"

#include <math.h>

/* The upstream sources compare names with the Microsoft runtime's stricmp and
 * strnicmp. POSIX's strcasecmp and strncasecmp fold case the same way (to
 * lower case), which the skeletor's sorted channel names depend on; the
 * engine's Q_stricmp folds to upper case and would order '_' differently. */
#if !defined( _WIN32 )
#include <strings.h>
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#endif

#define MOHAA_MAX_QPATH 256
#define MOHAA_MAX_STRING_CHARS 2048
#define MAX_RES_NAME 64

/* An entity blends up to sixteen animations (code/compat/mohaa_game_abi.h). */
#define MAX_FRAMEINFOS 16

#ifndef Q_max
#define Q_max( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
#endif
#ifndef Q_min
#define Q_min( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#endif

/* Medal of Honor's sound channels, which are not Quake III's: its alias
 * files name them, and the numbers travel to the game and the client game.
 * The upstream alias parser's CHAN_ names are spelled MOHAA_CHAN_ here. */
enum {
	MOHAA_CHAN_AUTO,
	MOHAA_CHAN_BODY,
	MOHAA_CHAN_ITEM,
	MOHAA_CHAN_WEAPONIDLE,
	MOHAA_CHAN_VOICE,
	MOHAA_CHAN_LOCAL,
	MOHAA_CHAN_WEAPON,
	MOHAA_CHAN_DIALOG_SECONDARY,
	MOHAA_CHAN_DIALOG,
	MOHAA_CHAN_MENU,
	MOHAA_CHAN_LOCAL_SOUND,
	MOHAA_CHAN_ANNOUNCER,
	MOHAA_CHAN_MAX
};

#ifdef __cplusplus
extern "C" {
#endif

typedef struct frameInfo_s {
	int index;
	float time;
	float weight;
} frameInfo_t;

/* Unaligned little-endian reads. Every platform fnTech3 builds for is
 * little-endian, as the upstream sources' "no swap" names assume. */
short ShortNoSwapPtr( const void *l );
int LongNoSwapPtr( const void *l );
float FloatNoSwapPtr( const void *l );
#define LittleLongPtr( x ) LongNoSwapPtr( &( x ) )

/* The rotation helpers the skeletor's matrices and quaternions are built on
 * (code/mohaa/qcommon/mohaa_q_math.c). */
void QuatToMat( const float q[4], float m[3][3] );
void MatToQuat( float srcMatrix[3][3], float destQuat[4] );
void EulerToQuat( float ang[3], float q[4] );
void QuatToAngles( const float q[4], vec3_t angles );
void MatrixToEulerAngles( const float mat[3][3], vec3_t ang );
void MatrixTransformVector( const vec3_t in, const float mat[3][3], vec3_t out );
void TransposeMatrix( float in[3][3], float out[3][3] );
void VectorMatrixInverse( void *DstMatrix, const void *SrcMatrix );
void SlerpQuaternion( float from[4], float to[4], float t, float res[4] );
float randweight( void );

/* Ritual's tokenizer (mohaa_q_shared.c): the next token, or "" at the end of
 * the text or, unless `crossline`, at the end of the line. */
const char *COM_GetToken( char **data_p, qboolean crossline );

static ID_INLINE int VectorCompareEpsilon( const vec3_t v1, const vec3_t v2, float fEpsilon ) {
	int i;

	for ( i = 0; i < 3; i++ ) {
		if ( fabs( v1[i] - v2[i] ) > fEpsilon ) {
			return 0;
		}
	}
	return 1;
}

static ID_INLINE void QuatClear( float q[4] ) {
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}

static ID_INLINE void QuatInverse( float q[4] ) {
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
}

#ifdef __cplusplus
}

/* The upstream sources pass read-only matrices where fnTech3's helpers take
 * writable ones; neither helper writes its inputs. */
inline void AxisCopy( const vec3_t in[3], vec3_t out[3] ) {
	VectorCopy( in[0], out[0] );
	VectorCopy( in[1], out[1] );
	VectorCopy( in[2], out[2] );
}

inline void MatrixMultiply( const float in1[3][3], const float in2[3][3], float out[3][3] ) {
	MatrixMultiply( const_cast<float( * )[3]>( in1 ), const_cast<float( * )[3]>( in2 ), out );
}
#endif

#endif
