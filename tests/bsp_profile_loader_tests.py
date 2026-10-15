"""Execute all four map-loader controllers with synthetic files and services.

No graphics, game modules, installed assets or user input are used. The actual
manager decides the format; only activation and downstream lump services are
replaced. Existing BSP43/QL lump tests cover those codecs separately.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/ql_bsp.h"
#include "code/qcommon/game_manager.h"
#include "code/renderercommon/tr_rtcw_world_shader.h"
#include "code/renderercommon/tr_raven_bsp_format.h"
#include "code/renderercommon/tr_jka_distance_cull.h"
#include "code/renderercommon/tr_map_shader_flags.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static const comGameProfile_t *profile;
static std::vector<byte> input;
static std::set<void *> files, allocations;
static int lengthOverride = INT_MAX, formatCalls, translations, rbspTranslations, fakkTranslations, mohaaTranslations, lumpLoads, advertisements;
static int q3testTranslations, q3testCurveRules;
static int fileReleases, allocationReleases, checksumCalls;
static bool missing, failTranslation, rebuildWorld;
static std::string diagnostic;
static comGameBspFormat_t Format(int version) {
    ++formatCalls;
    return Com_GameManagerBspFormat(profile, version);
}
void QDECL Com_Error(errorParm_t, const char *format, ...) {
    char message[512]; va_list args;
    va_start(args, format); vsnprintf(message, sizeof(message), format, args); va_end(args);
    diagnostic = message; throw 7;
}
static void Com_DPrintf(const char *, ...) {}
static void QDECL Print(printParm_t, const char *, ...) {}
// The real entity reader and owned distance getter are covered separately
// by jka_distance_cull_tests. This fixture stubs downstream lump readers.
static float s_jkaDistanceCull;
static qboolean s_jkaDistanceCullReady;
static unsigned Com_BlockChecksum(const void *data, int length) {
    // An IHV conversion changes both the header and length; peers identify
    // the original installed bytes, never the transient retail layout.
    assert(length == int(input.size()));
    assert(!std::memcmp(data, input.data(), input.size()));
    ++checksumCalls; return 12345;
}
static cvar_t variable, *cm_noAreas, *cm_noCurves, *cm_playerCurveClip, *cm_curveClipHack;
static cvar_t *Cvar_Get(const char *, const char *, int) { return &variable; }
static void Cvar_SetDescription(cvar_t *, const char *) {}
static int FS_ReadFile(const char *, void **out) {
    *out = nullptr;
    if (missing) return -1;
    *out = std::malloc(std::max<std::size_t>(1, input.size())); assert(*out);
    if (!input.empty()) std::memcpy(*out, input.data(), input.size());
    files.insert(*out);
    return lengthOverride == INT_MAX ? int(input.size()) : lengthOverride;
}
static void FS_FreeFile(void *value) {
    assert(files.erase(value) == 1); std::free(value); ++fileReleases;
}
static void *Allocate(int size) {
    auto *value = std::malloc(std::max(1, size)); assert(value);
    allocations.insert(value); return value;
}
static void Z_Free(void *value) {
    assert(allocations.erase(value) == 1); std::free(value); ++allocationReleases;
}
static void *CM_BSP43Alloc(int size) { return Allocate(size); }
static void CM_BSP43Free(void *value) { Z_Free(value); }
struct bsp43_translation_t { void *data; int length; };
static qboolean BSP43_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), bsp43_translation_t *out,
        char *error, size_t capacity) {
    ++translations;
    assert(length >= int(sizeof(dheader43_t)));
    assert(QLBSP_ReadLE32((const byte *)data + 4) == 43);
    if (failTranslation) {
        Q_strncpyz(error, "fixture translation rejected", int(capacity)); return qfalse;
    }
    auto *header = (dheader_t *)alloc(sizeof(dheader_t)); std::memset(header, 0, sizeof(*header));
    header->ident = BSP_IDENT; header->version = 46;
    out->data = header; out->length = sizeof(*header); return qtrue;
}
/* Jedi Academy's maps arrive through their own translator, which the RBSP
   codec tests cover; here it only has to be reached and released. */
struct dheaderRBSP_t { int ident; int version; lump_t lumps[18]; };
struct rbsp_translation_t { void *data; int length; };
static qboolean RBSP_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), rbsp_translation_t *out,
        char *error, size_t capacity) {
    ++rbspTranslations;
    assert(length >= int(sizeof(dheaderRBSP_t)));
    assert(QLBSP_ReadLE32((const byte *)data + 4) == 1);
    if (failTranslation) {
        Q_strncpyz(error, "fixture translation rejected", int(capacity)); return qfalse;
    }
    auto *header = (dheader_t *)alloc(sizeof(dheader_t)); std::memset(header, 0, sizeof(*header));
    header->ident = BSP_IDENT; header->version = 46;
    out->data = header; out->length = sizeof(*header); return qtrue;
}
/* Alice's maps arrive through their own translator, which the FAKK codec
   tests cover; here it only has to be reached and released. */
struct dheaderFAKK_t { int ident; int version; int checksum; lump_t lumps[20]; };
struct fakk_translation_t { void *data; int length;
    const int32_t *shaderSubdivisions; int numShaderSubdivisions; };
/* The renderers ask the translator to carry the light grid too, and take its
   192x192x320 cell; the collision loader asks for the shader subdivision
   counts; and F.A.K.K.2's loaders ask for its own version 12. */
#define FAKK_TRANSLATE_LIGHTGRID 0x0001
#define FAKK_TRANSLATE_SHADER_SUBDIVISIONS 0x0002
#define FAKK_TRANSLATE_VERSION_FAKK2 0x0004
#define FAKK_VERSION_ALICE 42
#define FAKK_VERSION_FAKK2 12
#define FAKK_LIGHTGRID_SIZE_X 192.0f
#define FAKK_LIGHTGRID_SIZE_Y 192.0f
#define FAKK_LIGHTGRID_SIZE_Z 320.0f
static qboolean FAKK_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), fakk_translation_t *out,
        char *error, size_t capacity);
static qboolean FAKK_TranslateToV46Ex(const void *data, int length,
        void *(*alloc)(int), void (*release)(void *), fakk_translation_t *out,
        char *error, size_t capacity, int) {
    return FAKK_TranslateToV46(data, length, alloc, release, out, error, capacity);
}
static qboolean FAKK_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), fakk_translation_t *out,
        char *error, size_t capacity) {
    ++fakkTranslations;
    assert(length >= int(sizeof(dheaderFAKK_t)));
    assert(QLBSP_ReadLE32((const byte *)data + 4) == 42);
    if (failTranslation) {
        Q_strncpyz(error, "fixture translation rejected", int(capacity)); return qfalse;
    }
    auto *header = (dheader_t *)alloc(sizeof(dheader_t)); std::memset(header, 0, sizeof(*header));
    header->ident = BSP_IDENT; header->version = 46;
    out->data = header; out->length = sizeof(*header); return qtrue;
}
/* Medal of Honor's "2015" 19 maps arrive through 2015's own translator
   (code/qcommon/bsp_mohaa.h), which carries each shader's subdivision count
   and the terrain past the last lump; the collision loader reads both. Only
   the members the loaders here read are declared. */
#define MOHAA_BSP_IDENT (('5' << 24) + ('1' << 16) + ('0' << 8) + '2')
#define MOHAA_BSP_VERSION 19
struct dheaderMOHAA_t { int ident; int version; int checksum; lump_t lumps[28]; };
struct mohaa_translation_t { void *data; int length;
    const int32_t *shaderSubdivisions; int numShaders; };
static qboolean MOHAA_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), mohaa_translation_t *out,
        char *error, size_t capacity) {
    *out = {}; ++mohaaTranslations;
    assert(length >= int(sizeof(dheaderMOHAA_t)));
    assert(QLBSP_ReadLE32((const byte *)data + 4) == MOHAA_BSP_VERSION);
    if (failTranslation) {
        Q_strncpyz(error, "fixture translation rejected", int(capacity)); return qfalse;
    }
    auto *header = (dheader_t *)alloc(sizeof(dheader_t)); std::memset(header, 0, sizeof(*header));
    header->ident = BSP_IDENT; header->version = 46;
    out->data = header; out->length = sizeof(*header); return qtrue;
}
/* Q3Test's IBSP 44 and 45 maps arrive through its own translator
   (code/qcommon/bsp_q3test.h), which tests/q3test_bsp_tests.cpp covers; here
   it only has to be reached and released, and the collision loader has to
   apply the pinned release's curve rules. */
#define Q3TEST_BSP_VERSION_44 44
#define Q3TEST_BSP_LUMPS_44 15
struct q3tBspTranslation_t { void *data; int length; };
static qboolean Q3TestBSP_TranslateToV46(const void *data, int length,
        void *(*alloc)(int), void (*)(void *), q3tBspTranslation_t *out,
        char *error, size_t capacity) {
    *out = {}; ++q3testTranslations;
    const int version = int(QLBSP_ReadLE32((const byte *)data + 4));
    assert(length >= 8 + Q3TEST_BSP_LUMPS_44 * 8 && (version == 44 || version == 45));
    if (failTranslation) {
        Q_strncpyz(error, "fixture translation rejected", int(capacity)); return qfalse;
    }
    auto *header = (dheader_t *)alloc(sizeof(dheader_t)); std::memset(header, 0, sizeof(*header));
    header->ident = BSP_IDENT; header->version = 46;
    out->data = header; out->length = sizeof(*header); return qtrue;
}
static void CM_Q3TestCurveRules() { ++q3testCurveRules; }
/* The renderers rebuild a world with terrain, static models or decals into
   a new buffer, which replaces the translation; one with none keeps it. The
   map's own name finds its decal file. */
struct mohaaBuiltSurfaces_t { int firstTerrain, numTerrain, firstDecal, numDecals, firstBuiltSurface, numBuiltSurfaces; };
static qboolean R_MOHAABuildWorld(const mohaa_translation_t *translation, const char *mapName, void **data,
        int *length, mohaaBuiltSurfaces_t *built) {
    *data = nullptr; *length = 0; *built = {};
    assert(mapName && !std::strcmp(mapName, "maps/fixture.bsp"));
    if (!rebuildWorld) return qfalse;
    *data = Allocate(translation->length);
    std::memcpy(*data, translation->data, translation->length);
    *length = translation->length; return qtrue;
}
static byte *cmod_base, *fileBase;
static int c_gridVerts;
static struct { char name[MAX_QPATH]; int checksum;
    qboolean brushSolidKeepsContents, curveInsideKeepsContents, tempModelsAreBoxes; } cm;
static void CM_ClearMap() { std::memset(&cm, 0, sizeof(cm)); }
static void CM_InitBoxHull() {}
static void CM_FloodAreaConnections() {}
static void CMod_CheckLeafBrushes() {}
/* Alice's loading stages. No profile loaded here is Alice's, so neither the
   collision loader nor a renderer's world load may report one. */
static std::vector<int> aliceStages;
static void Com_AliceLoadingStage(int stage) { aliceStages.push_back(stage); }
#define LUMP_SERVICE(name) template<class... Args> static void name(Args...) { ++lumpLoads; }
LUMP_SERVICE(CMod_LoadShaders) LUMP_SERVICE(CMod_LoadLeafs) LUMP_SERVICE(CMod_LoadLeafBrushes)
LUMP_SERVICE(CMod_LoadLeafSurfaces) LUMP_SERVICE(CMod_LoadPlanes) LUMP_SERVICE(CMod_LoadBrushSides)
LUMP_SERVICE(CMod_LoadBrushes) LUMP_SERVICE(CMod_LoadSubmodels) LUMP_SERVICE(CMod_LoadNodes)
LUMP_SERVICE(CMod_LoadEntityString) LUMP_SERVICE(CMod_LoadVisibility) LUMP_SERVICE(CMod_LoadPatches)
LUMP_SERVICE(CMod_LoadMOHAATerrain) LUMP_SERVICE(CMod_LoadMOHAALights)
LUMP_SERVICE(R_LoadLightmaps) LUMP_SERVICE(R_LoadShaders) LUMP_SERVICE(R_LoadPlanes)
LUMP_SERVICE(R_LoadFogs) LUMP_SERVICE(R_LoadSurfaces) LUMP_SERVICE(R_LoadMarksurfaces)
LUMP_SERVICE(R_LoadNodesAndLeafs) LUMP_SERVICE(R_LoadSubmodels) LUMP_SERVICE(R_LoadVisibility)
LUMP_SERVICE(R_LoadEntities) LUMP_SERVICE(R_LoadLightGrid)
#undef LUMP_SERVICE
// This controller fixture stubs sub-loaders. Native lighting data/ownership
// are exercised by q3_ihv_model_lighting_tests, including the actual loader.
static void R_LoadQ3IHVModelLighting(const lump_t *, const lump_t *, const lump_t *) {}
static void R_RitualModelLightsRead(const byte *, int, const char *) {}
static void R_RitualModelLightsKeep() {}
static void R_PreLoadFogs(const lump_t *) {}
static void R_ClearWorldSun() {}
static void R_ClearStaticMapLights() {}
static void R_ClearSurfaceLightProxies() {}
static void R_ShutdownAdvertisements() {}
static void R_LoadGlobalFogForWorld() {}
static void R_BuildSurfaceLightProxiesForWorld() {}
static void R_LoadStaticMapLightsForWorld() {}
static void R_LoadWolfETExternalLightmaps(const lump_t *) {}
// These fixtures select other profiles. The real Raven reader/grid boundary
// is executed by raven_bsp_render_tests; accidental cross-profile calls fail.
static void R_RavenPrepareWorldBuffer(void **, int *, fnRavenBspView_t *, const char *) { assert(false); }
static void R_RavenKeepWorldRecords(const fnRavenBspView_t *) { assert(false); }
static void R_LoadRavenLightGrid(const lump_t *) { assert(false); }
static cvar_t vertexLight;
static cvar_t *r_vertexLight = &vertexLight;
constexpr int GLHW_PERMEDIA2 = 4;
static struct { int hardwareType; } glConfig;
static void R_LoadAdvertisements(const lump_t *lump) {
    ++advertisements;
    assert(lump->fileofs == 152 && lump->filelen == 128);
    assert(QLBSP_ReadLE32(fileBase + lump->fileofs) == 0x12345678);
}
static byte arena[16];
static byte *HunkAlloc(int size, int) { assert(size == 0); return arena; }
static struct {
    decltype(&Com_Error) Error;
    decltype(&Print) Printf;
    decltype(&::FS_ReadFile) FS_ReadFile;
    decltype(&::FS_FreeFile) FS_FreeFile;
    decltype(&Allocate) Malloc;
    decltype(&Z_Free) Free;
    decltype(&HunkAlloc) Hunk_Alloc;
    decltype(&Format) GameBspFormat;
    comGameId_t gameId;
    decltype(&Com_AliceLoadingStage) AliceLoadingStage;
} ri{Com_Error, Print, FS_ReadFile, FS_FreeFile, Allocate, Z_Free, HunkAlloc, Format};
static struct World { char name[MAX_QPATH], baseName[MAX_QPATH]; int dataSize; float lightGridSize[3];
    int mohaaFirstDecal, mohaaNumDecals, mohaaFirstTerrain, mohaaNumTerrain;
    int mohaaFirstBuiltSurface, mohaaNumBuiltSurfaces; dshader_t *shaders; int numShaders; } s_worldData;
static struct { qboolean worldMapLoaded, mapLoading; World *world;
    fnRTCWWorldShader_t rtcwWorld; void *sunShader; } tr;
constexpr int LIGHTMAP_NONE = -1;
static void *R_FindShader(const char *, int, qboolean) { assert(false); return nullptr; }
'''


HARNESS = r'''
static void Reset() {
    // Com_Error's enclosing engine cleanup owns cached file/hunk allocations.
    for (void *p : files) std::free(p);
    for (void *p : allocations) std::free(p);
    files.clear(); allocations.clear(); diagnostic.clear();
    formatCalls = translations = rbspTranslations = fakkTranslations = mohaaTranslations = lumpLoads = advertisements = 0;
    q3testTranslations = q3testCurveRules = 0;
    fileReleases = allocationReleases = checksumCalls = 0;
    missing = failTranslation = rebuildWorld = false; lengthOverride = INT_MAX;
    cm = {}; tr = {}; s_worldData = {}; ri.GameBspFormat = Format;
    ri.gameId = profile ? profile->id : COM_GAME_AUTO;
    // Seed a stale Academy value: every non-Academy or failed world load
    // must clear it before publishing a different world.
    s_jkaDistanceCull = 6000.0f; s_jkaDistanceCullReady = qtrue;
}
static void Word(std::size_t offset, uint32_t value) {
    assert(offset + 4 <= input.size());
    for (unsigned i = 0; i < 4; ++i) input[offset + i] = byte(value >> (8 * i));
}
static void Map(int version, bool ql = false) {
    input.assign(ql ? 280 : sizeof(dheader_t) + 16, 0);
    Word(0, BSP_IDENT); Word(4, version);
    if (ql) {
        Word(144, 152); Word(148, 128); Word(152, 0x12345678);
    } else {
        // This is ordinary RTCW entity payload at 144, not a QL descriptor.
        Word(8 + 8 * LUMP_ENTITIES, 144); Word(12 + 8 * LUMP_ENTITIES, 16);
        std::memcpy(input.data() + 144, "{\n\"classname\"\n}", 16);
    }
}
static void MOHAAMap(int version) {
    input.assign(sizeof(dheaderMOHAA_t), 0);
    Word(0, MOHAA_BSP_IDENT); Word(4, version);
}
static void Collision(const char *name) { int checksum = -1; CM_LoadMap(name, qfalse, &checksum); assert(checksum == 12345); }
using Loader = void (*)(const char *);
static void Load(Loader loader, bool success, const char *error = nullptr) {
    bool passed = true;
    try { loader("maps/fixture.bsp"); } catch(int code) { assert(code == 7); passed = false; }
    assert(passed == success);
    if (error) assert(diagnostic.find(error) != std::string::npos);
    if (loader != Collision) assert(!s_jkaDistanceCullReady && s_jkaDistanceCull == 0.0f);
    if (success) {
        assert(formatCalls == 1 && lumpLoads > 0 && fileReleases == 1);
        assert(files.empty() && allocations.empty());
        if (loader == Collision) assert(!std::strcmp(cm.name, "maps/fixture.bsp"));
        else assert(tr.world == &s_worldData && !tr.mapLoading);
    }
}
int main() {
    ri.AliceLoadingStage = Com_AliceLoadingStage;
    for (auto loader : {Collision, RE_LoadWorldMap_GLx, RE_LoadWorldMap_VK, RE_LoadWorldMap_RTX}) {
        for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
            profile = Com_GameProfileById(id);
            const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW;
            Reset(); Map(47, !rtcw); Load(loader, true);
            assert(advertisements == (!rtcw && loader != Collision));
            // RTCW's brush sweep and curve position test keep the trace's
            // contents; only RTCW SP traces its capsules as boxes.
            if (loader == Collision) {
                assert(bool(cm.brushSolidKeepsContents) == rtcw);
                assert(bool(cm.curveInsideKeepsContents) == rtcw);
                assert(!cm.tempModelsAreBoxes || rtcw);
            }
            assert(!translations && !allocationReleases);
            if (rtcw) {
                // Exactly 144 bytes is a complete RTCW header. Every shorter
                // prefix is rejected before publishing a world or checksum.
                Reset(); input.assign(144, 0); Word(0, BSP_IDENT); Word(4, 47);
                Load(loader, true); assert(!advertisements);
            } else {
                Reset(); Map(46); Load(loader, true); assert(!advertisements);
                Reset(); Map(43); Load(loader, true);
                assert(translations == 1 && allocationReleases == 1 && !advertisements);
                Reset(); Map(43); failTranslation = true;
                Load(loader, false, "fixture translation rejected"); assert(!lumpLoads && translations == 1);
                if (loader == Collision) assert(cm.checksum == 0);
                Reset(); Map(47); Load(loader, false, "advertisement lump");
                Reset(); input.assign(144, 0); Word(0, BSP_IDENT); Word(4, 47);
                Load(loader, false, "truncated QL extra lump header");
                Reset(); Map(47, true); Word(148, 127);
                Load(loader, false, "advertisement lump");
                Reset(); Map(47, true); Word(144, 0xffffffffu);
                Load(loader, false, "advertisement lump");
            }
            for (int version : {INT_MIN, -1, 0, 42, 43, 44, 45, 46, 48, INT_MAX}) {
                if (!rtcw && (version == 43 || version == 46)) continue;
                Reset(); Map(version); Load(loader, false, "unsupported by the selected game");
                assert(!translations && !lumpLoads && !checksumCalls && !advertisements);
            }
            for (unsigned size = 0; size < 144; ++size) {
                Reset(); Map(47, !rtcw); input.resize(size);
                Load(loader, false, "truncated header"); assert(!lumpLoads && !checksumCalls);
            }
            Reset(); Map(47, !rtcw); lengthOverride = -1;
            Load(loader, false, "truncated header"); assert(!formatCalls && !lumpLoads);
            Reset(); missing = true; Load(loader, false, "couldn't load"); assert(!formatCalls);
            Reset(); Map(47, !rtcw); Word(0, 0); Load(loader, false, "wrong file identifier");
            for (int lump = 0; lump < 17; ++lump) {
                for (bool offset : {false, true}) {
                    Reset(); Map(47, !rtcw); Word(8 + 8 * lump + (offset ? 0 : 4), 0xffffffffu);
                    Load(loader, false, "size/offset"); assert(!lumpLoads && !advertisements);
                }
            }
        }
        // Medal of Honor admits only "2015" 19. Every loader hands it to 2015's
        // translator and releases what that returns, and a renderer also
        // releases the translation when it rebuilds the world from it.
        profile = Com_GameProfileById(COM_GAME_MOHAA);
        for (bool rebuilt : {false, true}) {
            Reset(); MOHAAMap(MOHAA_BSP_VERSION); rebuildWorld = rebuilt; Load(loader, true);
            assert(mohaaTranslations == 1 && !translations && !rbspTranslations && !fakkTranslations);
            assert(!advertisements && allocationReleases == (rebuilt && loader != Collision ? 2 : 1));
        }
        Reset(); MOHAAMap(MOHAA_BSP_VERSION); failTranslation = true;
        Load(loader, false, "fixture translation rejected"); assert(!lumpLoads && mohaaTranslations == 1);
        if (loader == Collision) assert(cm.checksum == 0);
        for (int version : {INT_MIN, -1, 0, 12, 17, 18, 20, 21, 42, 43, 46, 47, INT_MAX}) {
            Reset(); MOHAAMap(version); Load(loader, false, "unsupported by the selected game");
            assert(!mohaaTranslations && !lumpLoads && !checksumCalls);
        }
        if (loader == Collision) {
            // Peers advertise the installed file's checksum, so a map shorter
            // than 2015's header is refused before that is taken.
            for (unsigned size = 8; size < sizeof(dheaderMOHAA_t); ++size) {
                Reset(); MOHAAMap(MOHAA_BSP_VERSION); input.resize(size);
                Load(loader, false, "truncated header"); assert(!checksumCalls && !mohaaTranslations);
            }
        }
        // Q3Test admits IBSP 44 and 45 while no release is pinned, as here;
        // every loader translates and releases them, and only the collision
        // loader applies the release's curve rules, once.
        profile = Com_GameProfileById(COM_GAME_Q3TEST);
        for (int version : {44, 45}) {
            Reset(); Map(version); Load(loader, true);
            assert(q3testTranslations == 1 && !translations && !rbspTranslations && !fakkTranslations);
            assert(!mohaaTranslations && allocationReleases == 1 && !advertisements);
            assert(q3testCurveRules == (loader == Collision ? 1 : 0));
            Reset(); Map(version); failTranslation = true;
            Load(loader, false, "fixture translation rejected");
            assert(!lumpLoads && q3testTranslations == 1 && !q3testCurveRules);
            if (loader == Collision) assert(cm.checksum == 0);
        }
        for (int version : {INT_MIN, -1, 0, 12, 19, 42, 43, 46, 47, INT_MAX}) {
            Reset(); Map(version); Load(loader, false, "unsupported by the selected game");
            assert(!q3testTranslations && !lumpLoads && !checksumCalls);
        }
        Reset(); profile = nullptr; Map(47);
        Load(loader, false, "unsupported by the selected game"); assert(!lumpLoads);
        if (loader != Collision) {
            Reset(); profile = Com_GameProfileById(COM_GAME_RTCW_SP); Map(47); ri.GameBspFormat = nullptr;
            Load(loader, false, "game BSP contract unavailable"); assert(!lumpLoads && !formatCalls);
        }
    }
    Reset(); assert(!Com_GameProfile());
    assert(aliceStages.empty());
}
'''


class BspProfileLoaderTests(unittest.TestCase):
    def run_cpp(self, source, manager=False):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'bsp.cpp'
            exe = Path(directory) / 'bsp.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
            extra = [str(ROOT / 'code/qcommon/game_profile.cpp')] if manager else []
            built = subprocess.run([compiler, *target, *abi, '-std=c++17', '-I', str(ROOT),
                str(cpp), *extra, '-o', str(exe)],
                cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=20, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_collision_and_all_renderers_obey_the_game_contract(self):
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        source = PRELUDE
        for signature in ['void Q_strncpyz(', 'char *COM_SkipPath (', 'void COM_StripExtension(']:
            source += '\n' + function(shared, signature)
        source += '\n#define Com_GameBspFormat Format\n'
        source += function((ROOT / 'code/qcommon/cm_load.c').read_text(), 'void CM_LoadMap(')
        source += '\n#undef Com_GameBspFormat\n'
        for directory, suffix in [('renderer', 'GLx'), ('renderervk', 'VK'), ('rendererrtx', 'RTX')]:
            loader = function((ROOT / f'code/{directory}/tr_bsp.c').read_text(), 'void RE_LoadWorldMap( const char *name ) {')
            source += '\n' + loader.replace('void RE_LoadWorldMap(', f'void RE_LoadWorldMap_{suffix}(', 1)
        self.run_cpp(source + '\n' + HARNESS, manager=True)

    def test_renderer_import_is_bound_before_module_entry(self):
        source = (ROOT / 'code/client/cl_main.cpp').read_text()
        loader = function(source, 'static void CL_InitRef( void ) {')
        self.assertLess(loader.index('rimp.GameBspFormat = Com_GameBspFormat;'),
                        loader.index('GetRefAPI( REF_API_VERSION, &rimp )'))
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text()
        self.assertIn('comGameBspFormat_t (*GameBspFormat)( int version );', public)

    def test_renderer_rejects_short_old_imports_before_copying(self):
        source = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/renderercommon/tr_public.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
refimport_t ri;
static int messages, publications;
static void QDECL Print(printParm_t, const char *, ...) { ++messages; }
static comGameBspFormat_t Format(int) { return COM_GAME_BSP_RTCW; }
static void GLX_CompatSetImports(const refimport_t *value) { assert(value == &ri); ++publications; }
'''
        for renderer in ['renderer', 'renderervk', 'rendererrtx']:
            code = (ROOT / f'code/{renderer}/tr_init.c').read_text()
            start = code.index('\tstatic refexport_t\tre;', code.index('refexport_t *GetRefAPI ('))
            end = code.index('// the RE_ functions', start)
            source += f'\nstatic refexport_t *Probe_{renderer}(int apiVersion, refimport_t *rimp) {{\n'
            source += code[start:end] + '\nreturn &re;\n}\n'
        source += r'''
int main() {
    const size_t legacyBytes = offsetof(refimport_t, GameBspFormat);
    // Allocate only API 14's actual prefix. ASan must catch any speculative
    // full-table read before version rejection, even if no field is then used.
    auto *legacy = (refimport_t *)std::calloc(1, legacyBytes); assert(legacy);
    const auto print = &Print; std::memcpy(legacy, &print, sizeof(print));
    for (auto probe : {Probe_renderer, Probe_renderervk, Probe_rendererrtx}) {
        ri = {}; ri.GameBspFormat = Format;
        const auto previous = ri;
        publications = messages = 0;
        for (int version : {0, REF_API_VERSION - 2, REF_API_VERSION - 1, REF_API_VERSION + 1}) {
            assert(!probe(version, legacy));
            assert(!std::memcmp(&ri, &previous, sizeof(ri)) && !publications);
        }
        assert(messages == 4);
        assert(!probe(REF_API_VERSION, nullptr));
        refimport_t invalid{};
        assert(!probe(REF_API_VERSION, &invalid));
        invalid.Printf = Print;
        assert(!probe(REF_API_VERSION, &invalid));
        assert(messages == 5 && !publications && !std::memcmp(&ri, &previous, sizeof(ri)));
        refimport_t valid{}; valid.Printf = Print; valid.GameBspFormat = Format;
        assert(probe(REF_API_VERSION, &valid));
        assert(!std::memcmp(&ri, &valid, sizeof(ri)));
        assert(publications == (probe == Probe_renderer ? 1 : 0));
    }
    std::free(legacy);
}
'''
        self.run_cpp(source)


if __name__ == '__main__':
    unittest.main()
