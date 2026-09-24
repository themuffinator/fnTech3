/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_MODEL_LIGHT_H
#define FNTECH3_TR_Q3_IHV_MODEL_LIGHT_H
#include "tr_q3_ihv_entities.h"
#include <math.h>
#include <stddef.h>

/* These routines also define NaN/overflow handling at the byte boundary.
 * Do not let a fast-math caller attach finite-only assumptions to arguments
 * or turn a division into a reciprocal multiply. Restore its mode afterward. */
#if defined(_MSC_VER) || defined(__clang__)
#pragma float_control(precise, on, push)
#elif defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize("no-fast-math")
#endif

/* Engine-owned lighting records, independent of the native renderer ABI.
 * Observed with the hash-pinned February 1999 executable; see the optional
 * tests/q3_ihv_reference_model_lighting_probe.cpp. Explicit spills preserve
 * its double arithmetic and binary32 rounding under optimized builds. */
typedef struct {
    float plane[4], origin[3], reciprocal[2][3];
    int width, height;
    const unsigned char *pixels; /* RGB subrectangle; original pitch is 128. */
} q3IHVModelLightFace_t;

typedef struct q3IHVModelLightNode_s {
    float plane[4];
    const struct q3IHVModelLightNode_s *children[2];
    const q3IHVModelLightFace_t *const *faces;
    int numFaces; /* -1 means an internal node. */
} q3IHVModelLightNode_t;

typedef struct {
    float start[3], end[3], origin[3], direction[3];
    float distance, color[3], hit[3];
    int sampled;
} q3IHVModelLightTrace_t;

static inline double Q3IHV_LightDot(const float a[3], const float b[3]) {
    volatile double v = (double)a[2] * b[2];
    volatile double y = (double)a[1] * b[1];
    volatile double x = (double)a[0] * b[0];
    v = v + y; v = v + x; return v;
}

static inline double Q3IHV_LightLength(const float v[3]) {
    volatile double x = (double)v[0] * v[0], y = (double)v[1] * v[1], z = (double)v[2] * v[2];
    x = x + y; x = x + z; return sqrt(x);
}

static inline int Q3IHV_LightReciprocal(const float vector[3], float result[3]) {
    float next[3];
    volatile double length;
    int a;
    for (a = 0; a < 3; ++a) if (!Q3IHV_EntityFinite(vector + a)) return 0;
    length = Q3IHV_LightLength(vector);
    if (length == 0.0) { memset(result, 0, 3 * sizeof(float)); return 1; }
    length = 1.0 / length;
    for (a = 0; a < 3; ++a) {
        volatile float normalized = (float)((double)vector[a] * length);
        volatile double value = (double)normalized * length;
        if (value > FLT_MAX || value < -FLT_MAX) return 0;
        next[a] = (float)value;
    }
    memcpy(result, next, sizeof(next)); return 1;
}

static inline void Q3IHV_LightDirection(const float axis[3][3], float result[3]) {
    const float world[3] = {0.4418349862098694f, 0.5680699944496155f, -0.6943129897117615f};
    int a;
    for (a = 0; a < 3; ++a) result[a] = (float)Q3IHV_LightDot(world, axis[a]);
}

static inline void Q3IHV_LightAddDynamic(const float origin[3], const float lightOrigin[3],
    const float lightColor[3], float radius, float color[3]) {
    float delta[3];
    volatile double length, power, factor;
    volatile float storedPower;
    int a;
    for (a = 0; a < 3; ++a) delta[a] = (float)((double)lightOrigin[a] - origin[a]);
    length = Q3IHV_LightLength(delta);
    if (length > radius) return;
    power = (double)radius * radius; power = power * 16.0; storedPower = (float)power;
    if (length < 16.0) length = 16.0;
    length = length * length; factor = (double)storedPower / length;
    for (a = 0; a < 3; ++a) {
        volatile double v = factor * lightColor[a];
        v = v + color[a]; color[a] = (float)v;
    }
}

static inline void Q3IHV_LightFinish(uint32_t flags, float color[3]) {
    float maximum;
    int a;
    if (flags) for (a = 0; a < 3; ++a) color[a] = (float)((double)color[a] + 32.0);
    maximum = color[0];
    for (a = 1; a < 3; ++a) if (color[a] > maximum) maximum = color[a];
    if (maximum > 255.0f) for (a = 0; a < 3; ++a) {
        volatile double divisor = maximum;
        volatile double quotient = (double)color[a] / divisor;
        volatile float scaled = (float)quotient;
        volatile double value = (double)scaled * 255.0;
        color[a] = (float)value;
    }
}

/* The original truncates to an x87 signed 64-bit integer then consumes its
 * low 32 bits. Guard the conversion, including x87's integer-indefinite case. */
static inline int32_t Q3IHV_LightInteger(const double *input) {
    double value;
    int64_t integer;
    uint32_t low;
    int32_t result;
    uint64_t bits;
    /* A pointer avoids finite-only ABI assumptions on a scalar argument in
     * Clang fast-math callers, so malformed bits reach this guard intact. */
    memcpy(&bits, input, sizeof(bits));
    if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000)) return 0;
    memcpy(&value, &bits, sizeof(value));
    if (value >= 9223372036854775808.0 || value < -9223372036854775808.0) return 0;
    integer = (int64_t)value; low = (uint32_t)(uint64_t)integer;
    memcpy(&result, &low, sizeof(result)); return result;
}

static inline void Q3IHV_LightDiffuse(const float normal[3], const float direction[3],
    const float ambient[3], const float directed[3], unsigned char rgba[4]) {
    const double incoming = Q3IHV_LightDot(normal, direction);
    int a;
    for (a = 0; a < 3; ++a) {
        volatile double value = ambient[a];
        int32_t integer;
        if (incoming > 0.0) { value = incoming * directed[a]; value = value + ambient[a]; }
        { const double finalValue = value; integer = Q3IHV_LightInteger(&finalValue); }
        if (incoming > 0.0 && integer > 255) integer = 255;
        rgba[a] = (unsigned char)integer;
    }
    rgba[3] = 255;
}

static inline void Q3IHV_LightTraceInit(const float origin[3], q3IHVModelLightTrace_t *trace) {
    memset(trace, 0, sizeof(*trace));
    memcpy(trace->start, origin, 3 * sizeof(float));
    memcpy(trace->origin, origin, 3 * sizeof(float));
    memcpy(trace->end, origin, 3 * sizeof(float));
    trace->end[2] = (float)((double)origin[2] - 2048.0);
    trace->direction[2] = -1.0f; trace->distance = 2048.0f;
    trace->color[0] = trace->color[1] = trace->color[2] = 255.0f;
}

static inline void Q3IHV_LightSample(const q3IHVModelLightFace_t *face, q3IHVModelLightTrace_t *trace) {
    float point[3], texel[2];
    volatile double direction, first, last, fraction, distance, delta[3];
    int a, x, y;
    if (!face || !face->pixels || face->width <= 0 || face->height <= 0
        || face->width > 128 || face->height > 128) return;
    direction = (double)trace->direction[1] * face->plane[1];
    first = (double)trace->direction[0] * face->plane[0]; direction = direction + first;
    first = (double)trace->direction[2] * face->plane[2]; direction = direction + first;
    if (direction > 0.0) return;
    first = Q3IHV_LightDot(trace->start, face->plane); first = first - face->plane[3];
    last = Q3IHV_LightDot(trace->end, face->plane); last = last - face->plane[3];
    last = first - last; if (last == 0.0) return;
    fraction = first / last;
    for (a = 0; a < 3; ++a) {
        volatile double v = (double)trace->end[a] - trace->start[a];
        v = v * fraction; v = v + trace->start[a]; point[a] = (float)v;
        delta[a] = (double)point[a] - face->origin[a];
    }
    for (a = 0; a < 2; ++a) {
        volatile double v = delta[2] * face->reciprocal[a][2], term = delta[1] * face->reciprocal[a][1];
        v = v + term; term = delta[0] * face->reciprocal[a][0]; v = v + term; texel[a] = (float)v;
    }
    if (!Q3IHV_EntityFinite(texel) || !Q3IHV_EntityFinite(texel + 1)
        || texel[0] < 0 || texel[0] >= face->width || texel[1] < 0 || texel[1] >= face->height) return;
    x = (int)floor((double)texel[0]); y = (int)floor((double)texel[1]);
    for (a = 0; a < 3; ++a) delta[a] = (double)point[a] - trace->origin[a];
    distance = delta[1] * trace->direction[1]; first = delta[0] * trace->direction[0];
    distance = distance + first; first = delta[2] * trace->direction[2]; distance = distance + first;
    if (distance < 0 || distance > trace->distance) return;
    trace->distance = (float)distance; trace->sampled = 1;
    memcpy(trace->hit, point, sizeof(point));
    for (a = 0; a < 3; ++a) trace->color[a] = face->pixels[(y * 128 + x) * 3 + a];
}

/* Iterative counterpart of the original front-to-back walk, preserving its
 * mutable segment, equal-distance handling and asymmetric dot-product order.
 * The BSP43 validator limits trees to 2048 levels; no renderer recursion. */
static inline int Q3IHV_LightTrace(const q3IHVModelLightNode_t *node, q3IHVModelLightTrace_t *trace) {
    struct { const q3IHVModelLightNode_t *farNode; float middle[3], end[3]; } stack[2048];
    int depth = 0, visits = 0;
    for (;;) {
        while (node && node->numFaces == -1) {
            volatile double first, last, term;
            int side, other, a;
            if (++visits > 262144) return 0;
            first = (double)trace->start[2] * node->plane[2]; term = (double)trace->start[0] * node->plane[0];
            first = first + term; term = (double)trace->start[1] * node->plane[1]; first = first + term; first = first - node->plane[3];
            last = (double)trace->end[1] * node->plane[1]; term = (double)trace->end[2] * node->plane[2];
            last = last + term; term = (double)trace->end[0] * node->plane[0]; last = last + term; last = last - node->plane[3];
            side = first < 0.0; other = last < 0.0;
            if (side != other) {
                volatile double fraction = first - last; fraction = first / fraction;
                if (depth == 2048) return 0;
                stack[depth].farNode = node->children[other];
                memcpy(stack[depth].end, trace->end, sizeof(trace->end));
                for (a = 0; a < 3; ++a) {
                    volatile double v = (double)trace->end[a] - trace->start[a];
                    v = v * fraction; v = v + trace->start[a]; stack[depth].middle[a] = (float)v;
                }
                memcpy(trace->end, stack[depth].middle, sizeof(trace->end)); ++depth;
            }
            node = node->children[side];
        }
        if (node) {
            int i;
            if (node->numFaces < 0 || node->numFaces > 131072 || (node->numFaces && !node->faces)) return 0;
            for (i = 0; i < node->numFaces; ++i) Q3IHV_LightSample(node->faces[i], trace);
        }
        node = NULL;
        while (depth) {
            volatile double distance, term;
            int a;
            --depth;
            distance = 0;
            for (a = 2; a >= 0; --a) {
                term = (double)stack[depth].middle[a] - trace->origin[a];
                term = term * trace->direction[a]; distance = distance + term;
            }
            if (distance >= trace->distance) continue;
            memcpy(trace->start, stack[depth].middle, sizeof(trace->start));
            memcpy(trace->end, stack[depth].end, sizeof(trace->end));
            node = stack[depth].farNode; break;
        }
        if (!node) return 1;
    }
}
#if defined(_MSC_VER) || defined(__clang__)
#pragma float_control(pop)
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#endif
