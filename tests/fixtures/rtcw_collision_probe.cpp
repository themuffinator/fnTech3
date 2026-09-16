/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Offline collision probe: game assets arrive through an owned stdin pipe.
 * Link the shipping collision sources, replacing only startup selection and
 * filesystem/allocator/cvar services. No module, renderer or input system. */
#include "code/qcommon/q_shared.h"
extern "C" {
#include "code/qcommon/cm_local.h"
}
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const comGameProfile_t *selected;
static std::vector<byte> mapData;
static std::vector<byte> arena(64u * 1024u * 1024u);
static size_t arenaUsed;
static int reads, releases, traces, hits;
static const char *mapName;
static std::array<cvar_t, 32> variables;
static std::array<std::string, 32> variableNames, variableValues;
static size_t variableCount;

extern "C" const comGameProfile_t *FnFixture_GameProfile(void) { return selected; }
extern "C" comGameBspFormat_t FnFixture_GameBspFormat(int version) {
    return Com_GameManagerBspFormat(selected, version);
}
void QDECL Com_Error(errorParm_t, const char *format, ...) {
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::fputc('\n', stderr); std::exit(2);
}
void QDECL Com_Printf(const char *format, ...) {
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
}
void QDECL Com_DPrintf(const char *, ...) {}
extern "C" void BotDrawDebugPolygons(void (*)(int, int, float *), int) {}
void *Hunk_Alloc(int size, ha_pref) {
    if (size < 0 || size_t(size) > arena.size() - arenaUsed)
        Com_Error(ERR_DROP, "collision probe hunk capacity");
    const size_t padded = (size_t(size) + 15u) & ~size_t(15u);
    if (padded > arena.size() - arenaUsed) Com_Error(ERR_DROP, "collision probe hunk alignment");
    void *result = arena.data() + arenaUsed; arenaUsed += padded; return result;
}
void *Z_Malloc(int size) {
    if (size < 0) Com_Error(ERR_DROP, "negative collision allocation");
    void *result = std::calloc(size ? size : 1, 1);
    if (!result) Com_Error(ERR_DROP, "collision allocation failed");
    return result;
}
void Z_Free(void *value) { std::free(value); }
void *Hunk_AllocateTempMemory(int size) { return Z_Malloc(size); }
void Hunk_FreeTempMemory(void *value) { Z_Free(value); }
int FS_ReadFile(const char *name, void **destination) {
    if (std::strcmp(name, mapName)) Com_Error(ERR_DROP, "unexpected collision file read");
    if (destination) {
        *destination = Z_Malloc(int(mapData.size()));
        std::memcpy(*destination, mapData.data(), mapData.size()); ++reads;
    }
    return int(mapData.size());
}
void FS_FreeFile(void *value) { Z_Free(value); ++releases; }
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    for (size_t i = 0; i < variableCount; ++i)
        if (variableNames[i] == name) return &variables[i];
    if (variableCount == variables.size()) Com_Error(ERR_DROP, "collision probe cvar capacity");
    const size_t i = variableCount++;
    variableNames[i] = name; variableValues[i] = value;
    auto &var = variables[i]; var.name = variableNames[i].data(); var.string = variableValues[i].data();
    var.flags = flags; var.value = float(std::atof(value)); var.integer = std::atoi(value); return &var;
}
void Cvar_SetDescription(cvar_t *, const char *) {}
void Cvar_Set(const char *name, const char *value) { Cvar_Get(name, value, 0); }
int Cvar_VariableIntegerValue(const char *name) { return Cvar_Get(name, "0", 0)->integer; }
float Cvar_VariableValue(const char *name) { return Cvar_Get(name, "0", 0)->value; }

static void CheckTrace(const trace_t &trace) {
    if (!std::isfinite(trace.fraction) || trace.fraction < 0 || trace.fraction > 1)
        Com_Error(ERR_DROP, "invalid collision fraction");
    for (float value : trace.endpos)
        if (!std::isfinite(value)) Com_Error(ERR_DROP, "invalid collision end position");
    ++traces; if (trace.fraction < 1 || trace.startsolid) ++hits;
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    selected = Com_GameManagerFindProfile(argv[1]); mapName = argv[2];
    if (!selected || selected->titleId != COM_GAME_TITLE_RTCW) return 2;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::array<byte, 65536> chunk;
    while (const size_t bytes = std::fread(chunk.data(), 1, chunk.size(), stdin)) {
        if (mapData.size() + bytes > 128u * 1024u * 1024u) return 2;
        mapData.insert(mapData.end(), chunk.data(), chunk.data() + bytes);
    }
    if (std::ferror(stdin)) return 2;
    int checksum;
    CM_LoadMap(mapName, qfalse, &checksum);
    if (reads != 1 || releases != 1 || CM_NumInlineModels() < 1 || CM_NumClusters() < 1)
        Com_Error(ERR_DROP, "incomplete collision map load");
    for (int index = 0; index < CM_NumInlineModels(); ++index) {
        vec3_t mins, maxs;
        CM_ModelBounds(CM_InlineModel(index), mins, maxs);
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(mins[axis]) || !std::isfinite(maxs[axis]) || mins[axis] > maxs[axis])
                Com_Error(ERR_DROP, "invalid inline-model bounds");
    }
    vec3_t mins, maxs;
    CM_ModelBounds(0, mins, maxs);
    uint32_t random = 0x12345678u;
    const vec3_t playerMins{-15,-15,-24}, playerMaxs{15,15,32};
    for (int index = 0; index < 128; ++index) {
        vec3_t start, end;
        for (int axis = 0; axis < 3; ++axis) {
            random = random * 1664525u + 1013904223u;
            start[axis] = mins[axis] + (maxs[axis] - mins[axis]) * float(random >> 8) / float(0x1000000);
            random = random * 1664525u + 1013904223u;
            end[axis] = mins[axis] + (maxs[axis] - mins[axis]) * float(random >> 8) / float(0x1000000);
        }
        trace_t trace;
        CM_BoxTrace(&trace, start, end, playerMins, playerMaxs, 0, -1, index & 1 ? qtrue : qfalse);
        CheckTrace(trace);
        (void)CM_PointContents(start, 0);
    }
    if (!hits) Com_Error(ERR_DROP, "collision fixture produced no world contacts");
    const int box = CM_TempBoxModel(playerMins, playerMaxs, qfalse);
    const int capsule = CM_TempBoxModel(playerMins, playerMaxs, qtrue);
    if (box != 511 || capsule != 510 || CM_ClipHandleToModel(box) != CM_ClipHandleToModel(capsule))
        Com_Error(ERR_DROP, "wrong RTCW temporary-hull contract");
    const vec3_t start{80,0,0}, end{20,0,0}; trace_t trace;
    CM_BoxTrace(&trace, start, end, playerMins, playerMaxs, capsule, CONTENTS_BODY, qtrue);
    CheckTrace(trace);
    if (trace.fraction >= 1) Com_Error(ERR_DROP, "missing capsule contact");
    std::printf("{\"models\":%d,\"planes\":%d,\"brushes\":%d,\"surfaces\":%d,"
        "\"clusters\":%d,\"traces\":%d,\"contacts\":%d,\"hunkBytes\":%zu,\"checksum\":%u,"
        "\"boxHandle\":%d,\"capsuleHandle\":%d}\n", CM_NumInlineModels(), cm.numPlanes,
        cm.numBrushes, cm.numSurfaces, CM_NumClusters(), traces, hits, arenaUsed, unsigned(checksum), box, capsule);
    CM_ClearMap();
}
