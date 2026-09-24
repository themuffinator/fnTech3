/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Actual BSP translation and renderer light loader, with owned CPU allocations.
 * Optional callers may supply legitimate map bytes; none are embedded here. */
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/qcommon/bsp_v43.h"
#include "code/renderercommon/tr_q3_ihv_model_light.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <cassert>

void Q_strncpyz(char *out, const char *in, int size) {
    if (size <= 0) return;
    size_t length = strlen(in); if (length >= size_t(size)) length = size - 1;
    memcpy(out, in, length); out[length] = 0;
}

namespace LightWorldFixture {
struct shader_t { qboolean ihvModelLightmap = qtrue; } shader;
struct msurface_t { shader_t *shader; };
struct mnode_t {
    int contents = 0;
    cplane_t *plane = nullptr;
    mnode_t *children[2]{};
    int nummarksurfaces = 0;
    msurface_t **firstmarksurface = nullptr;
};
struct world_t {
    int numnodes = 0, numsurfaces = 0, nummarksurfaces = 0;
    mnode_t *nodes = nullptr;
    msurface_t *surfaces = nullptr;
    msurface_t **marksurfaces = nullptr;
    q3IHVModelLightNode_t *q3IHVModelLight = nullptr;
} s_worldData;
static byte *fileBase;
static std::vector<std::unique_ptr<byte[]>> allocations;
static void *Allocate(int bytes, ha_pref preference) {
    assert(bytes >= 0 && preference == h_low);
    auto allocation = std::make_unique<byte[]>(size_t(bytes) + 1);
    void *result = allocation.get(); allocations.push_back(std::move(allocation)); return result;
}
static void Error(int, const char *message, ...) { throw std::runtime_error(message); }
static struct { comGameId_t gameId; decltype(&Allocate) Hunk_Alloc; decltype(&Error) Error; }
    ri{COM_GAME_QUAKE3_IHV, Allocate, Error};
#include "code/renderercommon/tr_q3_ihv_model_light_bsp.h"
static std::vector<mnode_t> nodes;
static std::vector<cplane_t> planes;
static std::vector<msurface_t> surfaces;
static std::vector<msurface_t *> marks;

static q3IHVModelLightNode_t *Build(const std::vector<unsigned char> &raw) {
    bsp43_translation_t translated{}; char error[128]{};
    if (!BSP43_TranslateToV46(raw.data(), int(raw.size()), [](int bytes) { return malloc(size_t(bytes)); }, free, &translated, error, sizeof(error)))
        throw std::runtime_error(error);
    std::unique_ptr<void, decltype(&free)> owner(translated.data, free);
    fileBase = static_cast<byte *>(translated.data);
    const auto &header = *static_cast<dheader_t *>(translated.data);
    auto count = [&](int index, size_t stride) { return size_t(LittleLong(header.lumps[index].filelen)) / stride; };
    auto bytes = [&](int index) { return fileBase + LittleLong(header.lumps[index].fileofs); };
    const auto *diskNodes = reinterpret_cast<const dnode_t *>(bytes(LUMP_NODES));
    const auto *diskLeaves = reinterpret_cast<const dleaf_t *>(bytes(LUMP_LEAFS));
    const auto *diskPlanes = reinterpret_cast<const dplane_t *>(bytes(LUMP_PLANES));
    const auto *diskMarks = reinterpret_cast<const int *>(bytes(LUMP_LEAFSURFACES));
    const size_t internal = count(LUMP_NODES, sizeof(dnode_t)), leaves = count(LUMP_LEAFS, sizeof(dleaf_t));
    allocations.clear(); nodes.assign(internal + leaves, {}); planes.assign(count(LUMP_PLANES, sizeof(dplane_t)), {});
    surfaces.assign(count(LUMP_SURFACES, sizeof(dsurface_t)), {&shader}); marks.resize(count(LUMP_LEAFSURFACES, sizeof(int)));
    for (size_t i = 0; i < planes.size(); ++i) {
        for (int a = 0; a < 3; ++a) planes[i].normal[a] = LittleFloat(diskPlanes[i].normal[a]);
        planes[i].dist = LittleFloat(diskPlanes[i].dist);
    }
    for (size_t i = 0; i < marks.size(); ++i) marks[i] = &surfaces.at(LittleLong(diskMarks[i]));
    for (size_t i = 0; i < internal; ++i) {
        nodes[i].contents = -1; nodes[i].plane = &planes.at(LittleLong(diskNodes[i].planeNum));
        for (int a = 0; a < 2; ++a) {
            int child = LittleLong(diskNodes[i].children[a]);
            nodes[i].children[a] = &nodes.at(child >= 0 ? size_t(child) : internal + size_t(-1 - child));
        }
    }
    for (size_t i = 0; i < leaves; ++i) {
        nodes[internal + i].nummarksurfaces = LittleLong(diskLeaves[i].numLeafSurfaces);
        nodes[internal + i].firstmarksurface = marks.data() + LittleLong(diskLeaves[i].firstLeafSurface);
    }
    s_worldData = {int(nodes.size()), int(surfaces.size()), int(marks.size()), nodes.data(), surfaces.data(), marks.data(), nullptr};
    lump_t lumps[HEADER_LUMPS];
    for (int i = 0; i < HEADER_LUMPS; ++i) lumps[i] = {LittleLong(header.lumps[i].fileofs), LittleLong(header.lumps[i].filelen)};
    R_LoadQ3IHVModelLighting(lumps + LUMP_SURFACES, lumps + LUMP_DRAWVERTS, lumps + LUMP_LIGHTMAPS);
    fileBase = nullptr; return s_worldData.q3IHVModelLight;
}
}
