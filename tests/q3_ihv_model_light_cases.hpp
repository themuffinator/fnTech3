/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Generated fixtures and pinned original-function digests; no game assets. */
#include "code/renderercommon/tr_q3_ihv_model_light.h"
#include <array>
#include <cassert>
#include <limits>

namespace LightCases {
static uint32_t seed;
static uint32_t Random() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static uint64_t digest;
static void Hash(const void *data, size_t size) {
    const auto *p = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i) { digest ^= p[i]; digest *= UINT64_C(1099511628211); }
}
template<class Shade> static void Diffuse(Shade shade) {
    float axis[3][3], direction[3], ambient[3], directed[3], normals[17][4];
    unsigned char colors[17][4];
    seed = 0x91874362; digest = UINT64_C(14695981039346656037);
    for (unsigned sample = 0; sample < 2048; ++sample) {
        for (auto &row : axis) for (float &value : row) value = float(int(Random() % 8192) - 4096) / 1024;
        for (int a = 0; a < 3; ++a) {
            ambient[a] = float(int(Random() % 16384) - 8192) / 8;
            directed[a] = float(int(Random() % 16384) - 8192) / 8;
        }
        for (auto &row : normals) for (float &value : row) value = float(int(Random() % 8192) - 4096) / 1024;
        memset(normals[0], 0, 12);
        shade(axis, ambient, directed, normals, direction, colors);
        Hash(direction, 12); Hash(colors, sizeof(colors));
    }
    assert(digest == UINT64_C(0xfdd6653145e344f2));
}
static void Dynamic() {
    struct Light { float origin[3], color[3], radius; } lights[8];
    seed = 0x97135264; digest = UINT64_C(14695981039346656037);
    for (unsigned sample = 0; sample < 1024; ++sample) {
        float origin[3], base[3];
        for (float &x : origin) x = float(int(Random() % 4096) - 2048) / 8;
        for (float &x : base) x = float(int(Random() % 4096) - 1024) / 16;
        for (auto &light : lights) {
            for (int a = 0; a < 3; ++a) {
                light.origin[a] = origin[a] + float(int(Random() % 2048) - 1024) / 8;
                light.color[a] = float(int(Random() % 2048) - 1024) / 256;
            }
            light.radius = float(Random() % 4096) / 8;
        }
        if (sample % 4 == 0 || sample % 4 == 1) {
            memcpy(lights[0].origin, origin, 12); lights[0].radius = 32;
            if (sample % 4 == 1) lights[0].origin[0] += 32;
        }
        for (unsigned count : {0u, 1u, 2u, 8u}) for (unsigned noWorld : {0u, 1u}) for (unsigned flags : {0u, 0x40u, 0x80000000u}) {
            float color[3]; for (int a = 0; a < 3; ++a) color[a] = noWorld ? 255 : base[a];
            for (unsigned i = 0; i < count; ++i) Q3IHV_LightAddDynamic(origin, lights[i].origin, lights[i].color, lights[i].radius, color);
            Q3IHV_LightFinish(flags, color); Hash(color, 12); Hash(color, 12);
        }
    }
    assert(digest == UINT64_C(0x0349e9b926854cfd));
}
static void Surface() {
    q3IHVModelLightFace_t face{}; face.plane[2] = 1; face.width = face.height = 4;
    face.reciprocal[0][0] = face.reciprocal[1][1] = 1.0f / 16;
    std::array<unsigned char, 128 * 4 * 3> pixels{};
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<unsigned char>((i * 13 + 17) % 256);
    face.pixels = pixels.data(); digest = UINT64_C(14695981039346656037);
    for (float x : {-.5f, 0.f, 7.999f, 15.999f, 16.f, 63.99f, 64.f, 65.f})
    for (float y : {-.5f, 0.f, 7.999f, 15.999f, 16.f, 63.99f, 64.f, 65.f})
    for (float z : {-1.f, 0.f, 16.f, 2048.f, 4096.f}) for (float limit : {0.f, 32.f, 2048.f}) {
        const float origin[3] = {x, y, z}; q3IHVModelLightTrace_t trace;
        Q3IHV_LightTraceInit(origin, &trace); trace.distance = limit; Q3IHV_LightSample(&face, &trace);
        float packed[24]{}; memcpy(packed, trace.start, 12); memcpy(packed + 3, trace.end, 12);
        memcpy(packed + 6, trace.origin, 12); memcpy(packed + 9, trace.direction, 12);
        packed[12] = trace.distance; memcpy(packed + 13, trace.color, 12); memcpy(packed + 21, trace.hit, 12);
        if (trace.sampled) memcpy(packed + 16, face.plane, 16);
        Hash(packed, sizeof(packed));
    }
    assert(digest == UINT64_C(0xe3c9b7700ac668f1));
}

struct Tree {
    q3IHVModelLightNode_t nodes[63]{};
    q3IHVModelLightFace_t faces[64]{};
    const q3IHVModelLightFace_t *marks[32][12]{};
    unsigned char pixels[128 * 128 * 3]{};
    void initialize() {
        for (size_t i = 0; i < sizeof(pixels); ++i) pixels[i] = static_cast<unsigned char>((i * 13 + 17) % 256);
        for (int i = 0; i < 64; ++i) {
            auto &face = faces[i]; face = {};
            face.plane[0] = float(int(Random() % 512) - 256) / 1024;
            face.plane[1] = float(int(Random() % 512) - 256) / 1024;
            face.plane[2] = i % 3 ? 1.f : -1.f;
            face.plane[3] = float(int(Random() % 4096) - 2048) / 4;
            face.origin[0] = face.origin[1] = -128;
            face.reciprocal[0][0] = face.reciprocal[1][1] = 1.0f / 16;
            face.width = face.height = i % 7 ? 16 : 0; face.pixels = pixels;
        }
        for (int i = 0; i < 63; ++i) {
            auto &node = nodes[i]; node = {};
            if (i < 31) {
                node.numFaces = -1;
                for (int a = 0; a < 3; ++a) node.plane[a] = float(int(Random() % 2048) - 1024) / 1024;
                node.plane[3] = float(int(Random() % 2048) - 1024) / 4;
                node.children[0] = nodes + i * 2 + 1; node.children[1] = nodes + i * 2 + 2;
            } else {
                node.numFaces = 12; node.faces = marks[i - 31];
                for (int a = 0; a < 12; ++a) marks[i - 31][a] = faces + Random() % 64;
            }
        }
    }
};
template<class Sample> static uint64_t Trees(Sample sample) {
    seed = 0x92763481; digest = UINT64_C(14695981039346656037);
    Tree tree;
    for (int t = 0; t < 32; ++t) {
        tree.initialize();
        for (int i = 0; i < 1024; ++i) {
            float origin[3]; for (float &v : origin) v = float(int(Random() % 8192) - 4096) / 8;
            q3IHVModelLightTrace_t trace; Q3IHV_LightTraceInit(origin, &trace);
            sample(tree, trace);
            Hash(trace.start, 12); Hash(trace.end, 12); Hash(&trace.distance, 4); Hash(trace.color, 12); Hash(trace.hit, 12);
        }
    }
    return digest;
}
static void Malformed() {
    float out[3] = {7, 8, 9}, saved[3]; memcpy(saved, out, 12);
    const float tiny[3] = {1e-40f, 0, 0};
    assert(!Q3IHV_LightReciprocal(tiny, out) && !memcmp(out, saved, 12));
    float nan[3] = {}; const uint32_t nanBits = UINT32_C(0x7fc00123); memcpy(nan, &nanBits, 4);
    assert(!Q3IHV_LightReciprocal(nan, out) && !memcmp(out, saved, 12));
    const float zero[3] = {0, 0, 0}; assert(Q3IHV_LightReciprocal(zero, out) && !memcmp(out, zero, 12));
    const double lowNegative = 4294967295.0, lowZero = 4294967296.0;
    assert(Q3IHV_LightInteger(&lowNegative) == -1 && Q3IHV_LightInteger(&lowZero) == 0);
    for (double x : {1e30, -1e30}) assert(Q3IHV_LightInteger(&x) == 0);
    for (uint64_t bits : {UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8000000000123)}) {
        double value; memcpy(&value, &bits, 8); assert(Q3IHV_LightInteger(&value) == 0);
    }
    q3IHVModelLightNode_t cycle{}; cycle.numFaces = -1; cycle.children[0] = cycle.children[1] = &cycle;
    q3IHVModelLightTrace_t trace; Q3IHV_LightTraceInit(zero, &trace); assert(!Q3IHV_LightTrace(&cycle, &trace));
    cycle.numFaces = 1; cycle.faces = nullptr; assert(!Q3IHV_LightTrace(&cycle, &trace));
    q3IHVModelLightFace_t face{}; face.width = 129; face.height = 1;
    const auto before = trace; Q3IHV_LightSample(&face, &trace); assert(!memcmp(&trace, &before, sizeof(trace)));
}
}
