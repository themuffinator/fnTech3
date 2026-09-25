"""Exercise exact module-owned renderer records and the shipping client boundary."""
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
#include "code/qcommon/qcommon.h"
#include "code/compat/render_record_adapter.hpp"
#include "code/cgame/cg_public.h"
#include "code/ui/ui_public.h"
#include <cassert>
#include <climits>
#include <memory>
#include <vector>
using namespace fntech3::render;
static comGameProfile_t profile;
static bool missingProfile;
const comGameProfile_t *Com_GameProfile() { return missingProfile ? nullptr : &profile; }
int Com_GameIsQuake3() { return profile.id == COM_GAME_QUAKE3; }
int Com_GameIsSTVEF() { return profile.id == COM_GAME_STVEF_SP || profile.id == COM_GAME_STVEF_MP; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static void *VM_ArgPtr(intptr_t address) { return reinterpret_cast<void *>(address); }
void *VM_ExplicitArgPtr(const vm_t *, intptr_t address) { return reinterpret_cast<void *>(address); }
static uintptr_t allowedAddress;
static std::size_t allowedBytes, checkedBytes;
static bool boundsFailure;
static void CheckBounds(const vm_t *, intptr_t address, std::size_t bytes) {
    checkedBytes = bytes;
    if (boundsFailure || static_cast<uintptr_t>(address) != allowedAddress || bytes > allowedBytes) throw 8;
}
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS CheckBounds
#include "code/client/client_render_compat.hpp"
static vm_t *cgvm = reinterpret_cast<vm_t *>(1), *uivm = reinterpret_cast<vm_t *>(2);
static bool hideHud;
static int cl_enemyHighlightNumPending, entityCalls, sceneCalls, liquidCalls, underwaterCalls, flushCalls;
static cvar_t capture{}, hideWeapon{};
static cvar_t *cl_captureActive = &capture, *r_levelshotHideViewWeapon = &hideWeapon;
static refEntity_t capturedEntity;
static refdef_t capturedView;
static qboolean capturedIntegerTime;
static void AddEntity(const refEntity_t *entity, qboolean intTime) {
    ++entityCalls; capturedEntity = *entity; capturedIntegerTime = intTime;
}
static void RenderScene(const refdef_t *view) { ++sceneCalls; capturedView = *view; }
static struct { decltype(&AddEntity) AddRefEntityToScene; decltype(&RenderScene) RenderScene; } re{AddEntity, RenderScene};
static void CL_AddEnemyHighlightRefEntity(const refEntity_t *entity, qboolean intTime) { AddEntity(entity, intTime); }
static bool CL_CaptureHidesHud() { return hideHud; }
static void CL_UpdateLocalViewLiquidInteraction(const refdef_t *) { ++liquidCalls; }
static void CL_UpdateUnderwaterView(const refdef_t *) { ++underwaterCalls; }
static void CL_FlushEnemyHighlightRefEntities(const refdef_t *) { ++flushCalls; }
static void CL_UIAdjustRefdef(refdef_t *) {}
static void CL_HudAdjustRefdef(refdef_t *) {} // Layout runs in hud_aspect_tests.
struct retailFramebufferScale_t { float x, y; };
static retailFramebufferScale_t CL_GetRetailFramebufferScale() { return {1.5f, 2.0f}; }
static int RoundToInt(float value) { return static_cast<int>(value + (value < 0 ? -0.5f : 0.5f)); }
static intptr_t Cgame(intptr_t, intptr_t *);
static intptr_t UI(intptr_t, intptr_t *);
template<class T> static intptr_t CG_Import_Syscall(int operation, const T *record) {
    allowedAddress = reinterpret_cast<uintptr_t>(record); allowedBytes = sizeof(T);
    intptr_t args[] = {operation, reinterpret_cast<intptr_t>(record)};
    return Cgame(operation, args);
}
static intptr_t UI_Import_Ptr(const void *record) { return reinterpret_cast<intptr_t>(record); }
static intptr_t UI_Import_Call(int operation, std::initializer_list<intptr_t> values) {
    assert(values.size() == 1);
    allowedAddress = *values.begin();
    allowedBytes = operation == UI_R_RENDERSCENE ? sizeof(fntech3_ql_refdef_t) : sizeof(fntech3_ql_refEntity_t);
    intptr_t args[] = {operation, *values.begin()};
    return UI(operation, args);
}
template<class T> struct Guarded {
    uint32_t before = 0x11223344;
    T value;
    uint32_t after = 0xaabbccdd;
    void Check() const { assert(before == 0x11223344 && after == 0xaabbccdd); }
};
template<class T> static void Zero(const T &value) {
    const T zero{}; assert(!std::memcmp(&value, &zero, sizeof(T)));
}
template<class T> static auto Unaligned(const T &value) {
    auto bytes = std::make_unique<unsigned char[]>(sizeof(T) + 1);
    bytes[0] = 0xe5; std::memcpy(bytes.get() + 1, &value, sizeof(T));
    return bytes;
}
template<class F> static void Reject(F action) {
    const int entities = entityCalls, scenes = sceneCalls;
    bool threw = false;
    try { action(); } catch (int) { threw = true; }
    assert(threw && entities == entityCalls && scenes == sceneCalls);
}
'''

CHECKS = r'''
template<class E, class V> static void Legacy(comGameId_t id) {
    profile.id = id;
    assert(EntityBytes(id) == 140 && RefdefBytes(id) == 368);
    // Every four-byte slot has distinct, finite bits. The frozen old prefix
    // remains exact while the enlarged canonical RTCW tail must be cleared.
    E entity;
    for (std::size_t i = 0; i < sizeof(entity) / 4; ++i) {
        const uint32_t bits = 0x3f000000u + static_cast<uint32_t>(i * 173);
        std::memcpy(reinterpret_cast<char *>(&entity) + i * 4, &bits, 4);
    }
    for (int type = 0; type < 8; ++type) for (int boolean : {0, 1}) {
        entity.reType = type; entity.nonNormalizedAxes = boolean;
        for (uint32_t time : {0u, 0x3fa00000u, 0x7fa00001u, 0x80000000u, UINT_MAX}) {
            entity.shaderTimeBits = time;
            const auto bytes = Unaligned(entity);
            Guarded<refEntity_t> output;
            std::memset(&output.value, 0xa5, sizeof(output.value));
            assert(ReadEntity(id, bytes.get() + 1, output.value)); output.Check();
            assert(!std::memcmp(&output.value, &entity, sizeof(entity))); Zero(output.value.rtcw);
            assert(bytes[0] == 0xe5 && !std::memcmp(bytes.get() + 1, &entity, sizeof(entity)));
        }
    }
    Guarded<refEntity_t> output;
    std::memset(&output.value, 0xb7, sizeof(output.value)); const auto saved = output;
    for (int type : {-1, INT_MIN, 8, 9, 10, INT_MAX}) {
        entity.reType = type;
        assert(!ReadEntity(id, &entity, output.value)); assert(!std::memcmp(&output, &saved, sizeof(output)));
    }
    entity.reType = 0;
    for (int boolean : {-1, 2, INT_MAX}) {
        entity.nonNormalizedAxes = boolean;
        assert(!ReadEntity(id, &entity, output.value)); assert(!std::memcmp(&output, &saved, sizeof(output)));
    }
    V view;
    for (std::size_t i = 0; i < sizeof(view) / 4; ++i) {
        const uint32_t bits = 0x3f200000u + static_cast<uint32_t>(i * 197);
        std::memcpy(reinterpret_cast<char *>(&view) + i * 4, &bits, 4);
    }
    auto bytes = Unaligned(view);
    Guarded<refdef_t> result; std::memset(&result.value, 0xa5, sizeof(result.value));
    assert(ReadRefdef(id, bytes.get() + 1, result.value)); result.Check();
    assert(!std::memcmp(&result.value, &view, sizeof(view))); Zero(result.value.rtcw);
    assert(bytes[0] == 0xe5 && !std::memcmp(bytes.get() + 1, &view, sizeof(view)));

    entity = {}; view = {};
    view.x = -10; view.y = 6; view.width = 300; view.height = 200;
    view.fov_x = 93; view.fov_y = 77; view.time = 1234;
    for (bool ui : {false, true}) {
        auto dispatch = ui ? UI : Cgame;
        for (bool integer : {false, true}) {
            const int operation = ui ? (integer ? UI_R_ADDREFENTITYTOSCENE2 : UI_R_ADDREFENTITYTOSCENE) :
                (integer ? CG_R_ADDREFENTITYTOSCENE2 : CG_R_ADDREFENTITYTOSCENE);
            auto raw = Unaligned(entity);
            intptr_t args[] = {operation, reinterpret_cast<intptr_t>(raw.get() + 1)};
            allowedAddress = args[1]; allowedBytes = sizeof(entity);
            assert(dispatch(operation, args) == 0 && checkedBytes == sizeof(entity));
            assert(capturedIntegerTime == (integer ? qtrue : qfalse)); Zero(capturedEntity.rtcw);
            boundsFailure = true; Reject([&] { dispatch(operation, args); }); boundsFailure = false;
            allowedBytes = sizeof(entity) - 1; Reject([&] { dispatch(operation, args); });
            args[1] = 0; Reject([&] { dispatch(operation, args); });
        }
        const int operation = ui ? UI_R_RENDERSCENE : CG_R_RENDERSCENE;
        auto raw = Unaligned(view);
        intptr_t args[] = {operation, reinterpret_cast<intptr_t>(raw.get() + 1)};
        allowedAddress = args[1]; allowedBytes = sizeof(view);
        const int scenes = sceneCalls;
        assert(dispatch(operation, args) == 0 && checkedBytes == sizeof(view) && sceneCalls == scenes + 1);
        Zero(capturedView.rtcw);
        assert(capturedView.x == -10 && capturedView.y == 6 && capturedView.width == 300 && capturedView.height == 200);
        assert(capturedView.fov_x == 93 && capturedView.fov_y == 77 && capturedView.time == 1234);
        assert(capturedView.rdflags == (ui && id == COM_GAME_QUAKE3 ? RDF_NOFOVCORRECTION : 0));
        boundsFailure = true; Reject([&] { dispatch(operation, args); }); boundsFailure = false;
        allowedBytes = sizeof(view) - 1; Reject([&] { dispatch(operation, args); });
        args[1] = 0; Reject([&] { dispatch(operation, args); });
    }
    // Existing capture behavior remains attached to the copied cgame view.
    view.rdflags = RDF_NOWORLDMODEL;
    allowedAddress = reinterpret_cast<uintptr_t>(&view); allowedBytes = sizeof(view);
    intptr_t args[] = {CG_R_RENDERSCENE, reinterpret_cast<intptr_t>(&view)};
    hideHud = true; cl_enemyHighlightNumPending = 7; const int scenes = sceneCalls;
    Cgame(CG_R_RENDERSCENE, args);
    assert(sceneCalls == scenes && cl_enemyHighlightNumPending == 0);
    hideHud = false; capture.integer = hideWeapon.integer = 1;
    Cgame(CG_R_RENDERSCENE, args);
    assert(capturedView.rdflags == (RDF_NOWORLDMODEL | RDF_NOFIRSTPERSON) && view.rdflags == RDF_NOWORLDMODEL);
    capture.integer = hideWeapon.integer = 0;
}

template<class E, class V> static void RTCW(comGameId_t id) {
    profile.id = id;
    assert(EntityBytes(id) == sizeof(E) && RefdefBytes(id) == sizeof(V));
    E entity{}; auto &c = entity.common; auto &a = entity.appearance;
    c.renderfx = -1; c.hModel = 83; c.nonNormalizedAxes = 1;
    c.shadowPlane = -17; c.frame = 19; c.torsoFrame = 37;
    for (int i = 0; i < 3; ++i) {
        c.lightingOrigin[i] = 10 + i; c.origin[i] = 20 + i;
        a.oldorigin[i] = 30 + i; a.fireRiseDir[i] = 40 + i;
        for (int j = 0; j < 3; ++j) { c.axis[i][j] = 50 + i * 3 + j; c.torsoAxis[i][j] = 60 + i * 3 + j; }
    }
    if constexpr (std::is_same_v<E, fntech3_rtcw_sp_refEntity_t>) {
        entity.scale[0] = 0.5f; entity.scale[1] = 2; entity.scale[2] = 3;
    }
    a.oldframe = 41; a.oldTorsoFrame = 57; a.backlerp = 0.25f; a.torsoBacklerp = 0.75f;
    a.skinNum = 71; a.customSkin = 73; a.customShader = 79;
    for (int i = 0; i < 4; ++i) a.shaderRGBA[i] = 100 + i;
    a.shaderTexCoord[0] = 0.125f; a.shaderTexCoord[1] = 0.625f;
    a.shaderTime = -0.5f; a.radius = 81; a.rotation = -90;
    a.fadeStartTime = INT_MIN; a.fadeEndTime = INT_MAX; a.hilightIntensity = 0.875f;
    a.reFlags = 0x12345678; a.entityNum = 317;
    constexpr refEntityType_t types[] = {RT_MODEL, RT_POLY, RT_SPRITE, RT_SPLASH, RT_BEAM,
        RT_RAIL_CORE, RT_RAIL_CORE_TAPER, RT_RAIL_RINGS, RT_LIGHTNING, RT_PORTALSURFACE};
    Guarded<refEntity_t> result;
    for (int type = 0; type < 10; ++type) {
        c.reType = type; const auto bytes = Unaligned(entity);
        std::memset(&result.value, 0xa5, sizeof(result.value));
        assert(ReadEntity(id, bytes.get() + 1, result.value)); result.Check(); const auto &out = result.value;
        assert(out.reType == types[type] && out.renderfx == 0x3cf && !(out.renderfx & RF_CROSSHAIR));
        assert(out.hModel == 83 && out.nonNormalizedAxes == qtrue && out.shadowPlane == -17);
        assert(out.frame == 19 && out.rtcw.torsoFrame == 37 && out.oldframe == 41 && out.rtcw.oldTorsoFrame == 57);
        assert(out.backlerp == 0.25f && out.rtcw.torsoBacklerp == 0.75f);
        assert(out.skinNum == 71 && out.customSkin == 73 && out.customShader == 79);
        for (int i = 0; i < 3; ++i) {
            assert(out.lightingOrigin[i] == 10 + i && out.origin[i] == 20 + i);
            assert(out.oldorigin[i] == 30 + i && out.rtcw.fireRiseDir[i] == 40 + i);
            for (int j = 0; j < 3; ++j) assert(out.axis[i][j] == 50 + i * 3 + j && out.rtcw.torsoAxis[i][j] == 60 + i * 3 + j);
        }
        for (int i = 0; i < 4; ++i) assert(out.shader.rgba[i] == 100 + i);
        assert(out.shaderTexCoord[0] == 0.125f && out.shaderTexCoord[1] == 0.625f && out.shaderTime.f == -0.5f);
        assert(out.radius == 81 && out.rotation == -90);
        assert(out.rtcw.fadeStartTime == INT_MIN && out.rtcw.fadeEndTime == INT_MAX && out.rtcw.hilightIntensity == 0.875f);
        assert(out.rtcw.reFlags == 0x12345678 && out.rtcw.entityNum == 317 && out.rtcw.highlight && out.rtcw.blink);
        if (id == COM_GAME_RTCW_SP) assert(out.rtcw.scale[0] == 0.5f && out.rtcw.scale[1] == 2 && out.rtcw.scale[2] == 3);
        else for (float scale : out.rtcw.scale) assert(scale == 1);
        assert(bytes[0] == 0xe5 && !std::memcmp(bytes.get() + 1, &entity, sizeof(entity)));
    }
    auto saved = result;
    for (int type : {-1, 10, INT_MIN, INT_MAX}) {
        c.reType = type; assert(!ReadEntity(id, &entity, result.value));
        assert(!std::memcmp(&result, &saved, sizeof(result)));
    }
    c.reType = 0;
    for (int boolean : {-1, 2, INT_MAX}) {
        c.nonNormalizedAxes = boolean; assert(!ReadEntity(id, &entity, result.value));
        assert(!std::memcmp(&result, &saved, sizeof(result)));
    }
    {
        // Tag queries do not consume scene flags or require a scene type.
        c.reType=INT_MIN;c.nonNormalizedAxes=INT_MAX;
        const auto bytes=Unaligned(entity);Guarded<refEntity_t> tag;
        assert(ReadRTCWTagEntity(id,bytes.get()+1,tag.value));tag.Check();
        assert(tag.value.reType==RT_MODEL&&tag.value.hModel==c.hModel&&tag.value.frame==c.frame);
        assert(tag.value.oldframe==a.oldframe&&tag.value.backlerp==a.backlerp);
        assert(tag.value.rtcw.torsoFrame==c.torsoFrame&&tag.value.rtcw.oldTorsoFrame==a.oldTorsoFrame);
        assert(!std::memcmp(tag.value.rtcw.torsoAxis,c.torsoAxis,sizeof(c.torsoAxis)));
        assert(tag.value.rtcw.torsoBacklerp==a.torsoBacklerp&&!tag.value.renderfx&&!tag.value.nonNormalizedAxes);
        const auto before=tag;assert(!ReadRTCWTagEntity(id,nullptr,tag.value));
        assert(!std::memcmp(&tag,&before,sizeof(tag)));
    }
    V view{};
    view.common.x = 11; view.common.y = 13; view.common.width = 1920; view.common.height = 1080;
    view.common.fov_x = 91; view.common.fov_y = 73; view.common.time = INT_MAX;
    view.common.vieworg[2] = 99; view.common.viewaxis[2][1] = 0.75f;
    std::memset(view.common.areamask, 0xb6, sizeof(view.common.areamask));
    std::memset(view.common.text, 'T', sizeof(view.common.text));
    auto &fog = view.glfog;
    fog.mode = 0x2601; fog.hint = 0x1100; fog.startTime = 123; fog.finishTime = 456;
    fog.color[0] = 0.125f; fog.color[1] = 0.25f; fog.color[2] = 0.5f; fog.color[3] = 1;
    fog.start = 150; fog.end = 1600; fog.density = 0.375f;
    fog.useEndForClip = fog.registered = fog.drawsky = fog.clearscreen = 1;
    if constexpr (std::is_same_v<V, fntech3_rtcw_sp_refdef_t>) view.fogDirty = 77;
    Guarded<refdef_t> output;
    for (int flags = 0; flags < 256; ++flags) {
        view.common.rdflags = flags; const auto bytes = Unaligned(view);
        std::memset(&output.value, 0xa5, sizeof(output.value));
        assert(ReadRefdef(id, bytes.get() + 1, output.value)); output.Check(); const auto &out = output.value;
        assert(out.rdflags == (flags & 5) && !(out.rdflags & (RDF_NOFIRSTPERSON | RDF_NOFOVCORRECTION)));
        assert(!!out.rtcw.skyboxPortal == !!(flags & 8) && !!out.rtcw.underwater == !!(flags & 16));
        assert(!!out.rtcw.drawingSky == !!(flags & 32) && !!out.rtcw.snooperView == !!(flags & 64));
        assert(!!out.rtcw.drawSkybox == (id == COM_GAME_RTCW_SP && !!(flags & 16)));
        assert(out.rtcw.fog.dirty == (id == COM_GAME_RTCW_SP ? 77 : 0));
        assert(out.x == 11 && out.y == 13 && out.width == 1920 && out.height == 1080);
        assert(out.fov_x == 91 && out.fov_y == 73 && out.time == INT_MAX);
        assert(out.vieworg[2] == 99 && out.viewaxis[2][1] == 0.75f);
        assert(!std::memcmp(out.areamask, view.common.areamask, 32) && !std::memcmp(out.text, view.common.text, 256));
        assert(out.rtcw.fog.mode == 0x2601 && out.rtcw.fog.hint == 0x1100);
        assert(out.rtcw.fog.startTime == 123 && out.rtcw.fog.finishTime == 456);
        assert(!std::memcmp(out.rtcw.fog.color, fog.color, 16));
        assert(out.rtcw.fog.start == 150 && out.rtcw.fog.end == 1600 && out.rtcw.fog.density == 0.375f);
        assert(out.rtcw.fog.useEndForClip && out.rtcw.fog.registered && out.rtcw.fog.drawsky && out.rtcw.fog.clearscreen);
        assert(bytes[0] == 0xe5 && !std::memcmp(bytes.get() + 1, &view, sizeof(view)));
    }
    const auto savedView = output;
    for (int fntech3_rtcw_fog_t::*field : {&fntech3_rtcw_fog_t::useEndForClip, &fntech3_rtcw_fog_t::registered,
            &fntech3_rtcw_fog_t::drawsky, &fntech3_rtcw_fog_t::clearscreen}) {
        for (int boolean : {-1, 2, INT_MAX}) {
            fog.*field = boolean; assert(!ReadRefdef(id, &view, output.value));
            assert(!std::memcmp(&output, &savedView, sizeof(output)));
        }
        fog.*field = 1;
    }
}
static void NativeQL() {
    profile.id = COM_GAME_QUAKELIVE;
    auto entity = std::make_unique<fntech3_ql_refEntity_t>();
    entity->hModel = 17; entity->shaderTimeBits = 0x7fa00001;
    QL_CG_trap_R_AddRefEntityToScene(entity.get());
    assert(capturedEntity.hModel == 17 && capturedEntity.shaderTime.i == 0x7fa00001); Zero(capturedEntity.rtcw);
    QL_UI_trap_R_AddRefEntityToScene(entity.get());
    assert(capturedEntity.hModel == 17 && capturedEntity.shaderTime.i == 0x7fa00001); Zero(capturedEntity.rtcw);
    auto view = std::make_unique<fntech3_ql_refdef_t>();
    view->x = -10; view->y = 6; view->width = 300; view->height = 200; view->fov_x = 93; view->fov_y = 77;
    QL_CG_trap_R_RenderScene(view.get());
    assert(capturedView.x == -15 && capturedView.y == 12 && capturedView.width == 450 && capturedView.height == 400);
    assert(capturedView.fov_x == 93 && capturedView.fov_y == 77); Zero(capturedView.rtcw);
    QL_UI_trap_R_RenderScene(view.get());
    assert(capturedView.x == -15 && capturedView.y == 12 && capturedView.width == 450 && capturedView.height == 400);
    assert(capturedView.fov_x == 93 && capturedView.fov_y == 77); Zero(capturedView.rtcw);
    assert(view->x == -10 && view->y == 6 && view->width == 300 && view->height == 200);
    Reject([] { QL_CG_trap_R_RenderScene(nullptr); });
    Reject([] { QL_UI_trap_R_RenderScene(nullptr); });
}
int main() {
    Legacy<fntech3_q3_refEntity_t, fntech3_q3_refdef_t>(COM_GAME_QUAKE3);
    Legacy<fntech3_ql_refEntity_t, fntech3_ql_refdef_t>(COM_GAME_QUAKELIVE);
    RTCW<fntech3_rtcw_sp_refEntity_t, fntech3_rtcw_sp_refdef_t>(COM_GAME_RTCW_SP);
    RTCW<fntech3_rtcw_mp_refEntity_t, fntech3_rtcw_mp_refdef_t>(COM_GAME_RTCW_MP);
    NativeQL();
    Guarded<refEntity_t> entity; Guarded<refdef_t> view;
    std::memset(&entity.value, 0xa5, sizeof(entity.value)); std::memset(&view.value, 0xb7, sizeof(view.value));
    const auto savedEntity = entity; const auto savedView = view;
    for (auto id : {COM_GAME_AUTO, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP,
            COM_GAME_COUNT, static_cast<comGameId_t>(UINT_MAX)}) {
        assert(!ReadEntity(id, nullptr, entity.value) && !ReadRefdef(id, nullptr, view.value));
        assert(!std::memcmp(&entity, &savedEntity, sizeof(entity)) && !std::memcmp(&view, &savedView, sizeof(view)));
        if (id == COM_GAME_AUTO || id >= COM_GAME_COUNT) {
            assert(EntityBytes(id) == 0 && RefdefBytes(id) == 0);
            assert(!ReadEntity(id, &entity.value, entity.value) && !ReadRefdef(id, &view.value, view.value));
        }
    }
    profile.id = COM_GAME_QUAKE3; missingProfile = true;
    Reject([&] { CL_ModuleRefEntity(cgvm, reinterpret_cast<intptr_t>(&entity.value)); });
    Reject([&] { CL_ModuleRefdef(uivm, reinterpret_cast<intptr_t>(&view.value)); });
    missingProfile = false;
    Reject([&] { CL_ModuleRefEntity(nullptr, reinterpret_cast<intptr_t>(&entity.value)); });
    Reject([&] { CL_ModuleRefdef(nullptr, reinterpret_cast<intptr_t>(&view.value)); });
    assert(liquidCalls && underwaterCalls && flushCalls);
}
'''


class RenderRecordAdapterTests(unittest.TestCase):
    def test_all_profiles_bounds_transactional_records_and_native_ql_dispatch(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        cgame = (ROOT / 'code/client/cl_cgame.cpp').read_text(encoding='utf-8')
        ui = (ROOT / 'code/client/cl_ui.cpp').read_text(encoding='utf-8')
        imports = (ROOT / 'code/client/ql_cgame_imports.inc').read_text(encoding='utf-8')
        bodies = []
        for name, source, prefix in [('Cgame', cgame, 'CG'), ('UI', ui, 'UI')]:
            cases = []
            for first, last in [('R_ADDREFENTITYTOSCENE', 'R_ADDPOLYTOSCENE'), ('R_RENDERSCENE', 'R_SETCOLOR')]:
                start = source.index(f'\tcase {prefix}_{first}:')
                end = source.index(f'\tcase {prefix}_{last}:', start)
                cases.append(source[start:end])
            bodies.append(f'static intptr_t {name}(intptr_t operation, intptr_t *args) {{\nswitch(operation) {{\n' +
                          '\n'.join(cases) + '\ndefault: throw 9;\n}\n}\n')
        bodies += [function(cgame, 'static int CL_ScaleRetailPixel('),
                   function(cgame, 'static void CL_ScaleRetailRefdef(')]
        for source, prefix in [(imports, 'QL_CG'), (ui, 'QL_UI')]:
            for name in ['AddRefEntityToScene', 'RenderScene']:
                bodies.append(function(source, f'static void QDECL {prefix}_trap_R_{name}('))
        # Every frozen legacy field is independently pinned to its source offset.
        facts = []
        for prefix in ['q3', 'ql']:
            facts += [f'#define ABI(type) fntech3_{prefix}_##type',
                '#define FNTECH3_RENDER_RECORD(type, bytes, alignment) static_assert(sizeof(ABI(type)) == bytes && alignof(ABI(type)) == alignment); static_assert(std::is_trivial_v<ABI(type)> && std::is_standard_layout_v<ABI(type)>);',
                '#define FNTECH3_RENDER_FIELD(type, own, original, offset, bytes) static_assert(offsetof(ABI(type), own) == offset && sizeof(((ABI(type) *)0)->own) == bytes);',
                '#define FNTECH3_RENDER_TYPE(name, value)',
                '#include "code/compat/legacy_render_layout.def"',
                '#undef ABI', '#undef FNTECH3_RENDER_RECORD', '#undef FNTECH3_RENDER_FIELD', '#undef FNTECH3_RENDER_TYPE']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='render-records-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PRELUDE + '\n'.join(facts + bodies) + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_all_renderers_preserve_the_translated_scene_tail(self):
        for backend in ['renderer', 'renderervk', 'rendererrtx']:
            source = (ROOT / 'code' / backend / 'tr_scene.c').read_text(encoding='utf-8')
            header = (ROOT / 'code' / backend / 'tr_local.h').read_text(encoding='utf-8')
            self.assertIn('tr.refdef.rtcw = fd->rtcw;', source)
            self.assertIn('rtcwRenderView_t rtcw;', header)


if __name__ == '__main__':
    unittest.main()
