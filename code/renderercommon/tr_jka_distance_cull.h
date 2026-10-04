/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_DISTANCE_CULL_H
#define FNTECH3_JKA_DISTANCE_CULL_H
#include "../qcommon/q_shared.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* jamp.exe 0x490c11 initializes the world-owned value to 0x45bb8000.
 * The worldspawn key writes that same float at 0x490dfd/0x490e9c.
 * This limit also reaches cgame traces; it is not a renderer preset toggle. */
#define FN_JKA_DEFAULT_DISTANCE_CULL 6000.0f

static inline qboolean FnJKA_DistanceCullValid( float value ) {
    uint32_t bits;
    memcpy( &bits, &value, sizeof( bits ) );
    /* Positive distances of at least one unit, with room for the diagonal.
     * Bit checks remain valid under fast-math and reject NaNs and infinities. */
    return bits >= UINT32_C(0x3f800000) && bits <= UINT32_C(0x7effffff) ? qtrue : qfalse;
}

static inline qboolean FnJKA_ParseDistanceCull( const char *text, float *output ) {
    char *end;
    float value;
    int savedError, parseError;
    if ( !text || !output ) return qfalse;
    savedError = errno;
    errno = 0;
    value = strtof( text, &end );
    parseError = errno;
    errno = savedError;
    if ( end == text || parseError || !FnJKA_DistanceCullValid( value ) ) return qfalse;
    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'
        || *end == '\v' || *end == '\f' ) ++end;
    if ( *end ) return qfalse;
    *output = value;
    return qtrue;
}

/* Apply the authored axial limit to Fn's existing radial far-plane result.
 * Keep the active near plane valid, without replacing PVS, lighting or fog. */
static inline float FnJKA_FarClip( float current, float distance, float nearPlane ) {
    float maximum = distance * 1.7320508075688772f;
    const float minimum = nearPlane + 1.0f;
    if ( maximum < minimum ) maximum = minimum;
    return current < maximum ? current : maximum;
}
#endif
