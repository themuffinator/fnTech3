/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_H
#define FNTECH3_TR_Q3_IHV_MODEL_H
#include "tr_q3_ihv_entities.h"
#include <math.h>

/* Native r_lerpmodels tests the float, not its integer conversion. Both
 * signed zeros and unordered (NaN) comparisons disable animation. */
static inline int Q3IHV_ModelLerpEnabled( const float *value ) {
	uint32_t bits;
	if ( !value ) return 1;
	memcpy( &bits, value, sizeof( bits ) );
	bits &= UINT32_C(0x7fffffff);
	return bits != 0 && bits <= UINT32_C(0x7f800000);
}

static inline float Q3IHV_ModelFraction( int frame, int oldframe,
	float backlerp, const float *control ) {
	return frame != oldframe && Q3IHV_ModelLerpEnabled( control ) ? backlerp : 0.0f;
}

/* Independently implemented from hash-pinned quake3.exe observations at
 * 0x42a685 (table), 0x445020 (animation), and 0x4420b0 (bounds).
 * See tests/q3_ihv_reference_model_animation_probe.cpp. Volatile stores retain
 * the observed double intermediates and selected binary32 spills even under
 * fast-math. This table is private: retail sine tables have different samples. */
static inline void Q3IHV_ModelSines(float table[1024]) {
    int i;
    volatile float degreeStep = 360.0f / 1023.0f;
    for (i = 0; i < 1024; ++i) {
        volatile double radians = (double)i * degreeStep;
        radians = radians * 3.141592653589793;
        radians = radians * (1.0 / 180.0);
        table[i] = (float)sin(radians);
    }
}

static inline void Q3IHV_ModelNormal(uint16_t packed, const float table[1024], double out[3]) {
    const unsigned lat = (packed >> 8) * 4, lng = (packed & 255) * 4;
    volatile double x = (double)table[(lat + 256) & 1023] * table[lng];
    volatile float y = (float)((double)table[lat] * table[lng]);
    out[0] = x; out[1] = y; out[2] = table[(lng + 256) & 1023];
}

/* Records contain three host-endian signed shorts and a packed normal.
 * The caller supplies the private initialized table and validates record spans.
 * A zero fraction ignores the move and does not normalize the decoded normal.
 * Stage both outputs so malformed/excessive extrapolation publishes neither. */
static inline int Q3IHV_ModelVertex(const void *oldRecord, const void *newRecord,
    float fraction, const float move[3], const float table[1024], float xyz[3], float normal[3]) {
    int16_t oldWords[4], newWords[4];
    float nextXYZ[3], nextNormal[3];
    double oldNormal[3], newNormal[3];
    volatile float front, scaledFront, scaledBack;
    int i;
    if (!newRecord || !move || !table || !xyz || !normal || !Q3IHV_EntityFinite(&fraction)) return 0;
    if (fraction != 0 && !oldRecord) return 0;
    for (i = 0; i < 3; ++i) if (!Q3IHV_EntityFinite(move + i)) return 0;
    memcpy(newWords, newRecord, sizeof(newWords));
    Q3IHV_ModelNormal((uint16_t)newWords[3], table, newNormal);
    front = (float)(1.0 - (double)fraction);
    scaledFront = (float)((1.0 - (double)fraction) / 64.0);
    scaledBack = (float)((double)fraction / 64.0);
    if (fraction != 0) {
        memcpy(oldWords, oldRecord, sizeof(oldWords));
        Q3IHV_ModelNormal((uint16_t)oldWords[3], table, oldNormal);
    }
    for (i = 0; i < 3; ++i) {
        volatile double position = (double)newWords[i] * scaledFront;
        volatile double direction = newNormal[i];
        if (fraction != 0) {
            volatile double oldPosition = (double)oldWords[i] * scaledBack;
            volatile double oldDirection = oldNormal[i] * fraction;
            position = position + oldPosition;
            position = position + move[i];
            direction = direction * front;
            direction = direction + oldDirection;
        }
        if (position > FLT_MAX || position < -FLT_MAX || direction > FLT_MAX || direction < -FLT_MAX) return 0;
        nextXYZ[i] = (float)position;
        nextNormal[i] = (float)direction;
    }
    if (fraction != 0) {
        volatile double x = (double)nextNormal[0] * nextNormal[0];
        volatile double y = (double)nextNormal[1] * nextNormal[1];
        volatile double z = (double)nextNormal[2] * nextNormal[2];
        volatile double length = x + y;
        length = length + z; length = sqrt(length);
        if (length != 0) {
            volatile double inverse = 1.0 / length;
            for (i = 0; i < 3; ++i) {
                volatile double value = inverse * nextNormal[i];
                nextNormal[i] = (float)value;
            }
        }
    }
    memcpy(xyz, nextXYZ, sizeof(nextXYZ));
    memcpy(normal, nextNormal, sizeof(nextNormal));
    return 1;
}

/* Origin expansion uses origin-oldorigin in model bounds without axes or
 * the animation fraction. It is distinct from the vertex origin offset. */
static inline int Q3IHV_ModelBounds(uint32_t flags, const float oldBounds[2][3],
    const float newBounds[2][3], const float origin[3], const float oldorigin[3], float output[2][3]) {
    float next[2][3];
    int i;
    if (!oldBounds || !newBounds || !output) return 0;
    if ((flags & Q3IHV_ENTITY_ORIGIN_LERP) && (!origin || !oldorigin)) return 0;
    for (i = 0; i < 3; ++i) {
        volatile double difference = 0, expanded;
        volatile float delta;
        int side;
        if (!Q3IHV_EntityFinite(&oldBounds[0][i]) || !Q3IHV_EntityFinite(&oldBounds[1][i]) ||
            !Q3IHV_EntityFinite(&newBounds[0][i]) || !Q3IHV_EntityFinite(&newBounds[1][i]) ||
            oldBounds[0][i] > oldBounds[1][i] || newBounds[0][i] > newBounds[1][i]) return 0;
        next[0][i] = oldBounds[0][i] < newBounds[0][i] ? oldBounds[0][i] : newBounds[0][i];
        next[1][i] = oldBounds[1][i] > newBounds[1][i] ? oldBounds[1][i] : newBounds[1][i];
        if (flags & Q3IHV_ENTITY_ORIGIN_LERP) {
            if (!Q3IHV_EntityFinite(origin + i) || !Q3IHV_EntityFinite(oldorigin + i)) return 0;
            difference = (double)origin[i] - oldorigin[i];
            if (difference > FLT_MAX || difference < -FLT_MAX) return 0;
        }
        delta = (float)difference; side = delta > 0 ? 1 : 0;
        expanded = (double)next[side][i] + delta;
        if (expanded > FLT_MAX || expanded < -FLT_MAX) return 0;
        next[side][i] = (float)expanded;
    }
    memcpy(output, next, sizeof(next));
    return 1;
}
#endif
