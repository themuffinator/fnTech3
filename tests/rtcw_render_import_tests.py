"""Execute original SP/MP UI/cgame renderer imports against a synthetic backend."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/compat/render_record_adapter.hpp"
#include "code/compat/rtcw_module_abi.h"
#include "code/renderercommon/tr_rtcw_marks.h"
#include "code/renderercommon/tr_rtcw_corona.h"
#include <array>
#include <cassert>
#include <climits>
#include <memory>
#include <utility>
#include <vector>
// Audio dispatch is executed by rtcw_music_tests.
template<class VM> static bool CL_RTCWAudioCall(VM *, fntech3::rtcw::module::Call, intptr_t *) { return false; }
template<class VM> static bool CL_RTCWCinematicCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
template<class VM> static bool CL_RTCWCameraCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
using namespace fntech3::rtcw::module;
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile() { return &profile; }
int Com_GameIsRTCW() { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
static intptr_t CL_CGameTrapForProfile(intptr_t operation) { return operation; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static void *VM_ArgPtr(intptr_t address) { return reinterpret_cast<void *>(address); }
void *VM_ExplicitArgPtr(const vm_t *, intptr_t address) { return reinterpret_cast<void *>(address); }
static vm_t *cgvm = reinterpret_cast<vm_t *>(1), *uivm = reinterpret_cast<vm_t *>(2);
static std::vector<std::pair<uintptr_t, std::size_t>> ranges;
static int boundsCalls, failBoundsAt;
static void CheckBounds(const vm_t *, intptr_t address, std::size_t bytes) {
    ++boundsCalls;
    if (failBoundsAt && boundsCalls == failBoundsAt) throw 8;
    const auto start = static_cast<uintptr_t>(address);
    for (const auto &range : ranges)
        if (start >= range.first && start - range.first <= range.second && bytes <= range.second - (start - range.first)) return;
    throw 9;
}
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS CheckBounds
static int backendCalls, lastOperation;
static bool failBackend, nullColor, nullTime;
static refEntity_t submittedEntity;
static refdef_t submittedView;
static vec4_t submittedColor;
static std::array<float, 8> submittedQuad;
static std::array<float, 7> submittedLight;
static int submittedLightFlags;
static qhandle_t submittedShader;
static refDrawQuad_t submittedDrawQuad;
static std::vector<polyVert_t> submittedVertices;
static int submittedNumVerts, submittedNumPolys, tempAllocations;
static bool failAllocation;
void *Hunk_AllocateTempMemory(int size) {
    assert(size > 0); if (failAllocation) throw 11;
    auto memory = new unsigned char[static_cast<unsigned>(size)]; ++tempAllocations; return memory;
}
void Hunk_FreeTempMemory(void *memory) { assert(memory && tempAllocations > 0); --tempAllocations; delete[] static_cast<unsigned char *>(memory); }
static void Backend(int operation) { ++backendCalls; lastOperation = operation; if (failBackend) throw 10; }
static void AddRTCWLight(const vec3_t origin, float radius, float r, float g, float b, int flags) {
    Backend(18); submittedLight={origin[0],origin[1],origin[2],radius,r,g,b}; submittedLightFlags=flags;
}
static std::array<int,3> fogWords;
static std::array<float,4> fogValues;
static int MarkRTCWFragments(int, const vec3_t *, const vec3_t, int, fnRTCWMarkPoint_t *, int, markFragment_t *) { assert(false); return 0; }
static void SetRTCWFog(int command,int first,int second,float r,float g,float b,float density) {
    Backend(20); fogWords={command,first,second}; fogValues={r,g,b,density};
}
static fnRTCWCorona_t submittedCorona;
static void AddRTCWCoronaToScene(const fnRTCWCorona_t *corona) { Backend(21); submittedCorona=*corona; }
static void LoadWorld(const char *name) { assert(!std::strcmp(name, "synthetic/resource")); Backend(1); }
static qhandle_t RegisterModel(const char *name) { LoadWorld(name); Backend(2); return 101; }
static qhandle_t RegisterSkin(const char *name) { LoadWorld(name); Backend(3); return 102; }
static qboolean skinFound = qtrue;
static bool badSkinName;
static qboolean GetSkinModel(qhandle_t skin, const char *type, char *name) {
    assert(skin == 317 && !std::strcmp(type, "synthetic/resource"));
    if (badSkinName) std::memset(name, 'x', MAX_QPATH);
    else std::strcpy(name, "models/test/body.mds");
    Backend(16); // Fail after filling the local output buffer.
    return skinFound;
}
static qhandle_t RegisterShader(const char *name) { LoadWorld(name); Backend(4); return 103; }
static std::array<int, 3> modelShaderArgs;
static qhandle_t GetShaderFromModel(qhandle_t model, int surface, int withLightmap) {
    modelShaderArgs = {model, surface, withLightmap}; Backend(17); return 731;
}
static qhandle_t RegisterShaderNoMip(const char *name) { LoadWorld(name); Backend(5); return 104; }
static void ClearScene() { Backend(6); }
static void AddEntity(const refEntity_t *entity, qboolean integerTime) {
    assert(!integerTime); Backend(7); submittedEntity = *entity;
}
static void Render(const refdef_t *view) { Backend(8); submittedView = *view; }
static void SetColor(const float *color) {
    Backend(9); nullColor = color == nullptr;
    if (color) std::memcpy(submittedColor, color, sizeof(submittedColor));
}
static void Quad(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader) {
    Backend(10); submittedQuad = {x, y, w, h, s1, t1, s2, t2}; submittedShader = shader;
}
static void ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs) {
    assert(model == 317);
    mins[0] = -1; mins[1] = -2; mins[2] = -3;
    maxs[0] = 4; maxs[1] = 5; maxs[2] = 6;
    Backend(11);  // Deliberately fail after writing the staging buffers.
}
static void Remap(const char *oldName, const char *newName, const char *time) {
    assert(!std::strcmp(oldName, "synthetic/old") && !std::strcmp(newName, "synthetic/new"));
    Backend(12); nullTime = time == nullptr; if (time) assert(!std::strcmp(time, "1.25"));
}
static void Polys(qhandle_t shader, int numVerts, const polyVert_t *vertices, int numPolys) {
    Backend(13); submittedShader = shader; submittedNumVerts = numVerts; submittedNumPolys = numPolys;
    submittedVertices.assign(vertices, vertices + numVerts * numPolys);
}
static void DrawQuad(const refDrawQuad_t *quad) { Backend(14); submittedDrawQuad = *quad; }
static int tagIndex, submittedFirst;
static int LerpTagEntity(orientation_t *tag, const refEntity_t *entity, const char *name, int first) {
    assert(!std::strcmp(name, "synthetic/resource"));
    submittedEntity = *entity; submittedFirst = first;
    for (int i = 0; i < 3; ++i) {
        tag->origin[i] = i - 2.5f;
        for (int j = 0; j < 3; ++j) tag->axis[i][j] = i * 3 + j + 0.125f;
    }
    Backend(15); // Failure after writing must not publish this local record.
    return tagIndex;
}
static bool endTokens;
static qboolean GetEntityToken(char *output, int capacity) {
    const char *token = endTokens ? "" : "classname";
    std::strncpy(output, token, capacity); output[capacity - 1] = 0;
    Backend(19); // Failure must not publish partially filled output.
    return endTokens ? qfalse : qtrue;
}
static struct {
    decltype(&::AddRTCWCoronaToScene) AddRTCWCoronaToScene;
    decltype(&::MarkRTCWFragments) MarkRTCWFragments;
    decltype(&::LoadWorld) LoadWorld;
    decltype(&::RegisterModel) RegisterModel;
    decltype(&::RegisterSkin) RegisterSkin;
    decltype(&::RegisterShader) RegisterShader;
    decltype(&::RegisterShaderNoMip) RegisterShaderNoMip;
    decltype(&::ClearScene) ClearScene;
    decltype(&::AddEntity) AddRefEntityToScene;
    decltype(&::Render) RenderScene;
    decltype(&::SetColor) SetColor;
    decltype(&::Quad) DrawStretchPic;
    decltype(&::ModelBounds) ModelBounds;
    decltype(&::Remap) RemapShader;
    decltype(&::Polys) AddPolyToScene;
    decltype(&::DrawQuad) DrawQuad;
    decltype(&::LerpTagEntity) LerpTagEntity;
    decltype(&::GetSkinModel) GetSkinModel;
    decltype(&::GetShaderFromModel) GetShaderFromModel;
    decltype(&::AddRTCWLight) AddRTCWLightToScene;
    decltype(&::GetEntityToken) GetEntityToken;
    decltype(&::SetRTCWFog) SetRTCWFog;
} re{AddRTCWCoronaToScene, MarkRTCWFragments, LoadWorld, RegisterModel, RegisterSkin, RegisterShader, RegisterShaderNoMip,
     ClearScene, AddEntity, Render, SetColor, Quad, ModelBounds, Remap, Polys, DrawQuad, LerpTagEntity, GetSkinModel, GetShaderFromModel, AddRTCWLight, GetEntityToken, SetRTCWFog};
#include "code/client/client_rtcw_render_compat.hpp"
'''

CHECKS = r'''
static intptr_t Ptr(const void *value) { return reinterpret_cast<intptr_t>(value); }
static intptr_t Float(float value) { int32_t bits; std::memcpy(&bits, &value, 4); return bits; }
static void Allow(const void *address, std::size_t bytes) { ranges.emplace_back(reinterpret_cast<uintptr_t>(address), bytes); }
struct ForgetRange {
    void operator()(unsigned char *data) const {
        const auto start = reinterpret_cast<uintptr_t>(data + 1);
        ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
            [start](const auto &range) { return range.first == start; }), ranges.end());
        delete[] data;
    }
};
template<class T> static auto Unaligned(const T &value) {
    // Do not leave freed allocations in the synthetic VM's accessible ranges.
    // Reuse of a larger stale range made bounds-rejection checks intermittent.
    auto data = std::unique_ptr<unsigned char[], ForgetRange>(new unsigned char[sizeof(T) + 1]);
    data[0] = 0xe5; std::memcpy(data.get() + 1, &value, sizeof(T));
    Allow(data.get() + 1, sizeof(T)); return data;
}
static intptr_t Invoke(Module module, Call call, std::initializer_list<intptr_t> values) {
    // Allocation ends after the last actual argument, including zero-argument
    // imports; ASan detects reading a nonexistent trailing word.
    auto args = std::make_unique<intptr_t[]>(values.size() + 1);
    args[0] = Ordinal(profile.id, call); assert(args[0] >= 0);
    std::copy(values.begin(), values.end(), args.get() + 1);
    return module == Module::CGAME ? CL_CgameSystemCalls(args.get()) : CL_UISystemCalls(args.get());
}
template<class F> static void Reject(F action, bool backendMayRun = false, int line = __builtin_LINE()) {
    const int before = backendCalls;
    bool rejected = false; try { action(); } catch (int) { rejected = true; }
    if (!rejected || (!backendMayRun && before != backendCalls)) {
        std::fprintf(stderr, "Rejection contract failed at probe.cpp:%d\n", line);
        std::abort();
    }
}
template<class E, class V> static void Mode(comGameId_t id) {
    profile.id = id;
    {
        const vec3_t origin={1.25f,-2.5f,3.75f};ranges.clear();auto bytes=Unaligned(origin);
        for(auto module:{Module::CGAME,Module::UI}) {
            const auto call=module==Module::CGAME?Call::CG_R_ADDCORONATOSCENE:Call::UI_R_ADDCORONATOSCENE;
            for(int flags:{INT_MIN,-1,0,1,2,3,4,INT_MAX}) {
                assert(Invoke(module,call,{Ptr(bytes.get()+1),Float(0.2f),Float(0.4f),Float(0.6f),Float(2),-17,flags})==0&&lastOperation==21);
                assert(!memcmp(submittedCorona.origin,origin,12)&&submittedCorona.scale==2&&submittedCorona.id==-17&&submittedCorona.flags==flags);
                assert(submittedCorona.color[0]==0.2f&&submittedCorona.color[1]==0.4f&&submittedCorona.color[2]==0.6f);
            }
            Reject([&]{Invoke(module,call,{0,0,0,0,Float(1),0,1});});
            Reject([&]{Invoke(module,call,{Ptr(bytes.get()+2),0,0,0,Float(1),0,1});});
            for(int index=0;index<4;++index) {
                intptr_t values[]={Float(1),Float(1),Float(1),Float(1)};values[index]=Float(NAN);
                Reject([&]{Invoke(module,call,{Ptr(bytes.get()+1),values[0],values[1],values[2],values[3],0,1});});
            }
            if(sizeof(intptr_t)>4) {
                const auto wide=static_cast<intptr_t>(INT64_C(0x100000000));
                Reject([&]{Invoke(module,call,{Ptr(bytes.get()+1),0,0,0,Float(1),wide,1});});
                Reject([&]{Invoke(module,call,{Ptr(bytes.get()+1),0,0,0,Float(1),0,wide});});
            }
            failBackend=true;Reject([&]{Invoke(module,call,{Ptr(bytes.get()+1),0,0,0,Float(1),0,1});},true);failBackend=false;
        }
        ranges.clear();
    }
    assert(Invoke(Module::CGAME,Call::CG_R_SETFOG,{6,10,2000,Float(0.2f),Float(0.3f),Float(0.4f),Float(1.1f)})==0);
    assert((fogWords==std::array<int,3>{6,10,2000}));
    assert((fogValues==std::array<float,4>{0.2f,0.3f,0.4f,1.1f}));
    if(sizeof(intptr_t)>4)for(int word=0;word<3;++word){
        std::array<intptr_t,7> words{6,10,2000,Float(0.2f),Float(0.3f),Float(0.4f),Float(1.1f)};
        words[word]=static_cast<intptr_t>(INT64_C(0x100000000));
        Reject([&]{Invoke(Module::CGAME,Call::CG_R_SETFOG,{words[0],words[1],words[2],words[3],words[4],words[5],words[6]});});
        words[word]=static_cast<intptr_t>(INT64_C(-0x100000001));
        Reject([&]{Invoke(Module::CGAME,Call::CG_R_SETFOG,{words[0],words[1],words[2],words[3],words[4],words[5],words[6]});});
    }
    static const char resource[] = "synthetic/resource", oldName[] = "synthetic/old", newName[] = "synthetic/new", time[] = "1.25";
    ranges.clear(); Allow(resource, sizeof(resource)); Allow(oldName, sizeof(oldName)); Allow(newName, sizeof(newName)); Allow(time, sizeof(time));
    {
        std::array<char, MAX_TOKEN_CHARS + 4> output; output.fill(char(0x5a));
        Allow(output.data()+1, MAX_TOKEN_CHARS+2); const auto call=Call::CG_GET_ENTITY_TOKEN;
        for(int capacity:{1,4,MAX_TOKEN_CHARS,MAX_TOKEN_CHARS+2}) {
            output.fill(char(0x5a)); endTokens=false;
            assert(Invoke(Module::CGAME,call,{Ptr(output.data()+1),capacity})==1&&lastOperation==19);
            const auto length=std::min(capacity-1,9);assert(output[1+length]==0&&output[2+length]==0x5a);
            assert(output[0]==0x5a&&output.back()==0x5a);
        }
        endTokens=true; assert(Invoke(Module::CGAME,call,{Ptr(output.data()+1),16})==0&&output[1]==0);
        output.fill(char(0x5a));failBackend=true;
        Reject([&]{Invoke(Module::CGAME,call,{Ptr(output.data()+1),16});},true);failBackend=false;
        for(char c:output)assert(c==0x5a);endTokens=false;
        for(int capacity:{INT_MIN,-1,0,INT_MAX})Reject([&]{Invoke(Module::CGAME,call,{Ptr(output.data()+1),capacity});});
        Reject([&]{Invoke(Module::CGAME,call,{0,1});});
        Reject([&]{Invoke(Module::CGAME,call,{-1,1});});
        Reject([&]{Invoke(Module::CGAME,call,{Ptr(output.data()+MAX_TOKEN_CHARS+2),4});});
        if(sizeof(intptr_t)>4)Reject([&]{Invoke(Module::CGAME,call,{Ptr(output.data()+1),static_cast<intptr_t>(INT64_C(0x100000000))});});
    }
    ranges.resize(4); // The token output buffer has left scope.
    for (int surface : {INT_MIN, -1, 0, 1, INT_MAX}) for (int lightmap : {INT_MIN, -1, 0, 1, INT_MAX}) {
        assert(Invoke(Module::CGAME, Call::CG_R_GETMODELSHADER, {317, surface, lightmap}) == 731 && lastOperation == 17);
        assert((modelShaderArgs == std::array<int,3>{317, surface, lightmap}));
    }
    failBackend = true;
    Reject([&] { Invoke(Module::CGAME, Call::CG_R_GETMODELSHADER, {317, 0, 0}); }, true);
    failBackend = false;
    if (sizeof(intptr_t) > 4) for (int argument = 0; argument < 3; ++argument) {
        std::array<intptr_t,3> values{317, 0, 0};
        values[argument] = static_cast<intptr_t>(INT64_C(0x100000000));
        Reject([&] { Invoke(Module::CGAME, Call::CG_R_GETMODELSHADER, {values[0], values[1], values[2]}); });
        values[argument] = static_cast<intptr_t>(INT64_C(-0x100000001));
        Reject([&] { Invoke(Module::CGAME, Call::CG_R_GETMODELSHADER, {values[0], values[1], values[2]}); });
    }
    {
        const auto call = Call::CG_R_GETSKINMODEL;
        unsigned char output[MAX_QPATH + 2], saved[sizeof(output)];
        std::memset(output, 0xa5, sizeof(output)); std::memcpy(saved, output, sizeof(output));
        Allow(output + 1, MAX_QPATH);
        assert(Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 1)}) == 1 && lastOperation == 16);
        assert(!std::strcmp((char *)output + 1, "models/test/body.mds") && output[0] == 0xa5 && output[MAX_QPATH+1] == 0xa5);
        assert(output[1 + sizeof("models/test/body.mds")] == 0xa5); // Only the string and terminator are published.
        std::memcpy(output, saved, sizeof(output));
        skinFound = qfalse;
        assert(Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 1)}) == 0);
        assert(!std::memcmp(output, saved, sizeof(output))); skinFound = qtrue;
        failBackend = true;
        Reject([&] { Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 1)}); }, true);
        failBackend = false; assert(!std::memcmp(output, saved, sizeof(output)));
        badSkinName = true;
        Reject([&] { Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 1)}); }, true);
        badSkinName = false; assert(!std::memcmp(output, saved, sizeof(output)));
        Reject([&] { Invoke(Module::CGAME, call, {317, 0, Ptr(output + 1)}); });
        Reject([&] { Invoke(Module::CGAME, call, {317, Ptr(resource), 0}); });
        Reject([&] { Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 2)}); });
        if (sizeof(intptr_t) > 4) Reject([&] { Invoke(Module::CGAME, call, {static_cast<intptr_t>(INT64_C(0x100000001)), Ptr(resource), Ptr(output + 1)}); });
        for (int failAt : {1, 2}) {
            boundsCalls = 0; failBoundsAt = failAt;
            Reject([&] { Invoke(Module::CGAME, call, {317, Ptr(resource), Ptr(output + 1)}); });
            failBoundsAt = 0;
        }
        std::strcpy((char *)output + 1, resource);
        assert(Invoke(Module::CGAME, call, {317, Ptr(output + 1), Ptr(output + 1)}) == 1);
        assert(!std::strcmp((char *)output + 1, "models/test/body.mds"));
    }
    for (auto module : {Module::CGAME, Module::UI}) {
        ranges.resize(4);  // Only the four static names outlive a module pass.
        const bool cg = module == Module::CGAME;
        auto call = [&](Call cgame, Call ui) { return cg ? cgame : ui; };
        for (const auto &entry : {std::pair{call(Call::CG_R_REGISTERMODEL, Call::UI_R_REGISTERMODEL), 101},
                std::pair{call(Call::CG_R_REGISTERSKIN, Call::UI_R_REGISTERSKIN), 102},
                std::pair{call(Call::CG_R_REGISTERSHADERNOMIP, Call::UI_R_REGISTERSHADERNOMIP), 104}}) {
            assert(Invoke(module, entry.first, {Ptr(resource)}) == entry.second);
            if(entry.first==call(Call::CG_R_REGISTERSHADERNOMIP,Call::UI_R_REGISTERSHADERNOMIP))
                Reject([&] { Invoke(module, entry.first, {0}); });
            else {const auto before=backendCalls;assert(Invoke(module,entry.first,{0})==0&&backendCalls==before);}
            failBackend = true; Reject([&] { Invoke(module, entry.first, {Ptr(resource)}); }, true); failBackend = false;
        }
        if (cg) {
            assert(Invoke(module, Call::CG_R_REGISTERSHADER, {Ptr(resource)}) == 103 && lastOperation == 4);
            assert(Invoke(module, Call::CG_R_LOADWORLDMAP, {Ptr(resource)}) == 0 && lastOperation == 1);
        }
        assert(Invoke(module, call(Call::CG_R_CLEARSCENE, Call::UI_R_CLEARSCENE), {}) == 0 && lastOperation == 6);
        {
            const vec3_t origin={1.25f,-2.5f,3.75f}; auto lightBytes=Unaligned(origin);
            const auto lightCall=call(Call::CG_R_ADDLIGHTTOSCENE, Call::UI_R_ADDLIGHTTOSCENE);
            for(int flags : {0,1,2,10,11,INT_MIN,INT_MAX}) {
                assert(Invoke(module,lightCall,{Ptr(lightBytes.get()+1),Float(1000),Float(0.25f),Float(0.5f),Float(1),flags})==0);
                assert(lastOperation==18 && submittedLightFlags==flags);
                assert((submittedLight==std::array<float,7>{1.25f,-2.5f,3.75f,1000,0.25f,0.5f,1}));
            }
            Reject([&]{Invoke(module,lightCall,{0,Float(1000),0,0,0,0});});
            Reject([&]{Invoke(module,lightCall,{Ptr(lightBytes.get()+2),Float(1000),0,0,0,0});});
            if(sizeof(intptr_t)>4) Reject([&]{Invoke(module,lightCall,{Ptr(lightBytes.get()+1),Float(1000),0,0,0,static_cast<intptr_t>(INT64_C(0x100000000))});});
            failBackend=true; Reject([&]{Invoke(module,lightCall,{Ptr(lightBytes.get()+1),Float(1000),0,0,0,0});},true); failBackend=false;
        }
        const auto entityCall = call(Call::CG_R_ADDREFENTITYTOSCENE, Call::UI_R_ADDREFENTITYTOSCENE);
        E entity{}; entity.common.reType = 4; entity.common.hModel = 37; entity.common.torsoFrame = 17;
        entity.appearance.shaderTime = 1.25f; entity.appearance.oldTorsoFrame = 13;
        auto bytes = Unaligned(entity);
        assert(Invoke(module, entityCall, {Ptr(bytes.get() + 1)}) == 0 && lastOperation == 7);
        assert(submittedEntity.reType == RT_BEAM && submittedEntity.hModel == 37 && submittedEntity.shaderTime.f == 1.25f);
        assert(submittedEntity.rtcw.torsoFrame == 17 && submittedEntity.rtcw.oldTorsoFrame == 13);
        assert(bytes[0] == 0xe5 && !std::memcmp(bytes.get() + 1, &entity, sizeof(entity)));
        Reject([&] { Invoke(module, entityCall, {0}); });
        auto invalid = entity; invalid.common.reType = 10; auto bad = Unaligned(invalid);
        Reject([&] { Invoke(module, entityCall, {Ptr(bad.get() + 1)}); });
        boundsCalls = 0; failBoundsAt = 1;
        Reject([&] { Invoke(module, entityCall, {Ptr(bytes.get() + 1)}); }); failBoundsAt = 0;
        {
            const auto tagCall = call(Call::CG_R_LERPTAG, Call::UI_CM_LERPTAG);
            E partial=entity;partial.common.reType=INT_MAX;partial.common.nonNormalizedAxes=INT_MIN;
            const auto partialBytes=Unaligned(partial);
            unsigned char output[sizeof(fntech3_rtcw_orientation_t) + 2];
            std::memset(output, 0xa5, sizeof(output)); Allow(output + 1, sizeof(output) - 2);
            for (int index : {-1, 0, 7, INT_MAX}) for (int first : {INT_MIN, -1, 0, 6, INT_MAX}) {
                tagIndex = index;
                assert(Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), Ptr(resource), first}) == index);
                assert(lastOperation == 15 && submittedFirst == first && submittedEntity.hModel == 37);
                assert(submittedEntity.rtcw.torsoFrame == 17 && submittedEntity.rtcw.oldTorsoFrame == 13);
                fntech3_rtcw_orientation_t tag;
                std::memcpy(&tag, output + 1, sizeof(tag));
                for (int i = 0; i < 3; ++i) {
                    assert(tag.origin[i] == i - 2.5f);
                    for (int j = 0; j < 3; ++j) assert(tag.axis[i][j] == i * 3 + j + 0.125f);
                }
                assert(output[0] == 0xa5 && output[sizeof(output) - 1] == 0xa5);
            }
            const std::vector<unsigned char> saved(output, output + sizeof(output));
            assert(Invoke(module,tagCall,{Ptr(output+1),Ptr(partialBytes.get()+1),Ptr(resource),0})==tagIndex);
            assert(submittedEntity.reType==RT_MODEL&&!submittedEntity.nonNormalizedAxes&&!submittedEntity.renderfx);
            Reject([&] { Invoke(module, tagCall, {0, Ptr(bytes.get() + 1), Ptr(resource), 0}); });
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), 0, Ptr(resource), 0}); });
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), 0, 0}); });
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 2), Ptr(bytes.get() + 1), Ptr(resource), 0}); });
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 2), Ptr(resource), 0}); });
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), 1, 0}); });
            for (int check = 1; check <= 3; ++check) {
                boundsCalls = 0; failBoundsAt = check;
                Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), Ptr(resource), 0}); });
                failBoundsAt = 0;
            }
            if constexpr (sizeof(intptr_t) > sizeof(int32_t)) {
                const intptr_t wide = static_cast<intptr_t>(static_cast<uint64_t>(INT_MAX) + 1);
                for (intptr_t first : {wide, -wide - 1})
                    Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), Ptr(resource), first}); });
            }
            failBackend = true;
            Reject([&] { Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), Ptr(resource), 0}); }, true);
            failBackend = false;
            assert(!std::memcmp(output, saved.data(), sizeof(output)));
            // Entity and name may overlap the output. Both inputs must be read
            // completely before the frozen result overwrites module memory.
            tagIndex = 0;
            assert(Invoke(module, tagCall, {Ptr(bytes.get() + 1), Ptr(bytes.get() + 1), Ptr(resource), 0}) == 0);
            assert(submittedEntity.hModel == 37 && submittedEntity.rtcw.torsoFrame == 17);
            std::memcpy(bytes.get() + 1, &entity, sizeof(entity));
            std::strcpy(reinterpret_cast<char *>(output + 1), resource);
            assert(Invoke(module, tagCall, {Ptr(output + 1), Ptr(bytes.get() + 1), Ptr(output + 1), 0}) == 0);
            ranges.pop_back();
        }
        V view{}; view.common.rdflags = 8; view.common.x = 17; view.common.width = 640; view.common.height = 480;
        view.common.fov_x = 97; view.common.time = 12345; view.glfog.density = 0.25f;
        auto viewBytes = Unaligned(view);
        const auto sceneCall = call(Call::CG_R_RENDERSCENE, Call::UI_R_RENDERSCENE);
        assert(Invoke(module, sceneCall, {Ptr(viewBytes.get() + 1)}) == 0 && lastOperation == 8);
        assert(submittedView.rdflags == 0 && submittedView.rtcw.skyboxPortal);
        assert(submittedView.x == 17 && submittedView.width == 640 && submittedView.height == 480);
        assert(submittedView.fov_x == 97 && submittedView.time == 12345 && submittedView.rtcw.fog.density == 0.25f);
        assert(!std::memcmp(viewBytes.get() + 1, &view, sizeof(view)));
        Reject([&] { Invoke(module, sceneCall, {0}); });
        view.glfog.registered = 2; auto badView = Unaligned(view);
        Reject([&] { Invoke(module, sceneCall, {Ptr(badView.get() + 1)}); });
        const vec4_t color = {0.125f, 0.25f, 0.5f, 0.75f}; auto colorBytes = Unaligned(color);
        const auto colorCall = call(Call::CG_R_SETCOLOR, Call::UI_R_SETCOLOR);
        assert(Invoke(module, colorCall, {Ptr(colorBytes.get() + 1)}) == 0 && lastOperation == 9);
        assert(!nullColor && !std::memcmp(submittedColor, color, sizeof(color)));
        assert(Invoke(module, colorCall, {0}) == 0 && nullColor);
        const auto quadCall = call(Call::CG_R_DRAWSTRETCHPIC, Call::UI_R_DRAWSTRETCHPIC);
        assert(Invoke(module, quadCall, {Float(-10), Float(20), Float(30), Float(40), Float(0.125f), Float(0.25f), Float(0.5f), Float(0.75f), 317}) == 0);
        const std::array<float, 8> quad = {-10, 20, 30, 40, 0.125f, 0.25f, 0.5f, 0.75f};
        assert(lastOperation == 10 && submittedQuad == quad && submittedShader == 317);
        if (cg) {
            auto gradient = [&](intptr_t color, int type, float x = -10) {
                return Invoke(module, Call::CG_R_DRAWSTRETCHPIC_GRADIENT, {Float(x), Float(20), Float(30), Float(40),
                    Float(0.125f), Float(0.25f), Float(0.5f), Float(0.75f), 317, color, type});
            };
            for (int type : {INT_MIN, -1, 0, 1, INT_MAX}) {
                assert(gradient(Ptr(colorBytes.get() + 1), type) == 0 && lastOperation == 14);
                const auto &draw = submittedDrawQuad;
                assert(draw.colorMask == 12 && draw.shader == 317);
                const vec2_t expectedXY[] = {{-10, 20}, {20, 20}, {20, 60}, {-10, 60}};
                const vec2_t expectedST[] = {{0.125f, 0.25f}, {0.5f, 0.25f}, {0.5f, 0.75f}, {0.125f, 0.75f}};
                const uint8_t expectedColor[] = {31, 63, 127, 191};
                assert(!std::memcmp(draw.xy, expectedXY, sizeof(expectedXY)) && !std::memcmp(draw.st, expectedST, sizeof(expectedST)));
                for (int i : {2, 3}) assert(!std::memcmp(draw.colors[i].rgba, expectedColor, 4));
            }
            assert(gradient(0, 0) == 0);
            assert(submittedDrawQuad.colors[2].u32 == UINT32_MAX && submittedDrawQuad.colors[3].u32 == UINT32_MAX);
            boundsCalls = 0; failBoundsAt = 1;
            Reject([&] { gradient(Ptr(colorBytes.get() + 1), 0); }); failBoundsAt = 0;
            Reject([&] { gradient(0, 0, INFINITY); });
            const vec4_t invalidColor = {0, 0, NAN, 1}; const auto badColor = Unaligned(invalidColor);
            Reject([&] { gradient(Ptr(badColor.get() + 1), 0); });
            const auto rotated = Call::CG_R_DRAWROTATEDPIC;
            if (id == COM_GAME_RTCW_MP) {
                assert(Invoke(module, rotated, {Float(10), Float(20), Float(80), Float(20),
                    Float(0.125f), Float(0.25f), Float(0.5f), Float(0.75f), 317, Float(0)}) == 0);
                const auto &draw = submittedDrawQuad;
                assert(draw.shader == 317 && draw.colorMask == 0);
                const float radius = std::sqrt(1700.0f);
                assert(std::fabs(draw.xy[0][0] - (50 + radius)) < 0.0001f && draw.xy[0][1] == 30);
                assert(std::fabs(draw.xy[1][0] - 50) < 0.0001f && std::fabs(draw.xy[1][1] - (30 + radius)) < 0.0001f);
                assert(draw.st[0][0] == 0.125f && draw.st[1][0] == 0.5f && draw.st[2][1] == 0.75f);
                Reject([&] { Invoke(module, rotated, {Float(10), Float(20), Float(80), Float(20),
                    Float(0), Float(0), Float(1), Float(1), 317, Float(NAN)}); });
            } else {
                intptr_t result = 12345;
                assert(!CL_RTCWRenderCall(cgvm, rotated, nullptr, result) && result == 12345);
            }
        }
        const auto modelCall = call(Call::CG_R_MODELBOUNDS, Call::UI_R_MODELBOUNDS);
        struct Bounds { uint32_t before = 0x12345678; vec3_t mins{}, maxs{}; uint32_t after = 0xaabbccdd; } bounds;
        Allow(&bounds, sizeof(bounds));
        assert(Invoke(module, modelCall, {317, Ptr(bounds.mins), Ptr(bounds.maxs)}) == 0 && lastOperation == 11);
        for (int i = 0; i < 3; ++i) assert(bounds.mins[i] == -1 - i && bounds.maxs[i] == 4 + i);
        assert(bounds.before == 0x12345678 && bounds.after == 0xaabbccdd);
        const auto saved = bounds;
        failBackend = true;
        Reject([&] { Invoke(module, modelCall, {317, Ptr(bounds.mins), Ptr(bounds.maxs)}); }, true);
        failBackend = false; assert(!std::memcmp(&bounds, &saved, sizeof(bounds)));
        for (int failAt : {1, 2}) {
            boundsCalls = 0; failBoundsAt = failAt;
            Reject([&] { Invoke(module, modelCall, {317, Ptr(bounds.mins), Ptr(bounds.maxs)}); });
            failBoundsAt = 0; assert(!std::memcmp(&bounds, &saved, sizeof(bounds)));
        }
        Reject([&] { Invoke(module, modelCall, {317, 0, Ptr(bounds.maxs)}); });
        Reject([&] { Invoke(module, modelCall, {317, Ptr(bounds.mins), 0}); });
        assert(Invoke(module, modelCall, {317, Ptr(bounds.mins), Ptr(bounds.mins)}) == 0);
        for (int i = 0; i < 3; ++i) assert(bounds.mins[i] == 4 + i);
        const auto remapCall = call(Call::CG_R_REMAP_SHADER, Call::UI_R_REMAP_SHADER);
        assert(Invoke(module, remapCall, {Ptr(oldName), Ptr(newName), Ptr(time)}) == 0 && lastOperation == 12 && !nullTime);
        assert(Invoke(module, remapCall, {Ptr(oldName), Ptr(newName), 0}) == 0 && nullTime);
        Reject([&] { Invoke(module, remapCall, {0, Ptr(newName), 0}); });
        Reject([&] { Invoke(module, remapCall, {Ptr(oldName), 0, 0}); });
        const auto polyCall = call(Call::CG_R_ADDPOLYTOSCENE, Call::UI_R_ADDPOLYTOSCENE);
        const auto batchCall = call(Call::CG_R_ADDPOLYSTOSCENE, Call::UI_R_ADDPOLYSTOSCENE);
        for (int count : {1, 2, 3, 4, 256, 257, 512}) {
            const std::size_t length = count * sizeof(fntech3_rtcw_polyVert_t);
            auto input = std::make_unique<unsigned char[]>(length + 1); input[0] = 0x96;
            for (int i = 0; i < count; ++i) {
                fntech3_rtcw_polyVert_t vertex{};
                for (int j = 0; j < 3; ++j) vertex.xyz[j] = i * 3 + j - 100.5f;
                for (int j = 0; j < 2; ++j) vertex.st[j] = i * 2 + j + 0.125f;
                for (int j = 0; j < 4; ++j) vertex.modulate[j] = static_cast<uint8_t>(i + j);
                std::memcpy(input.get() + 1 + i * sizeof(vertex), &vertex, sizeof(vertex));
            }
            const std::vector<unsigned char> saved(input.get(), input.get() + length + 1);
            Allow(input.get() + 1, length);
            assert(Invoke(module, polyCall, {317, count, Ptr(input.get() + 1)}) == 0 && lastOperation == 13);
            assert(submittedShader == 317 && submittedNumVerts == count && submittedNumPolys == 1);
            assert(submittedVertices.size() == static_cast<std::size_t>(count) && tempAllocations == 0);
            for (int i = 0; i < count; ++i) {
                const auto &v = submittedVertices[i];
                for (int j = 0; j < 3; ++j) assert(v.xyz[j] == i * 3 + j - 100.5f);
                for (int j = 0; j < 2; ++j) assert(v.st[j] == i * 2 + j + 0.125f);
                for (int j = 0; j < 4; ++j) assert(v.modulate.rgba[j] == static_cast<uint8_t>(i + j));
            }
            const auto expected = submittedVertices;
            assert(Invoke(module, batchCall, {317, 1, Ptr(input.get() + 1), count}) == 0);
            assert(submittedNumVerts == 1 && submittedNumPolys == count);
            assert(!std::memcmp(submittedVertices.data(), expected.data(), count * sizeof(polyVert_t)));
            if (!(count % 4)) {
                assert(Invoke(module, batchCall, {317, 4, Ptr(input.get() + 1), count / 4}) == 0);
                assert(submittedNumVerts == 4 && submittedNumPolys == count / 4);
                assert(!std::memcmp(submittedVertices.data(), expected.data(), count * sizeof(polyVert_t)));
            }
            failBackend = true;
            Reject([&] { Invoke(module, polyCall, {317, count, Ptr(input.get() + 1)}); }, true);
            failBackend = false; assert(tempAllocations == 0);
            if (count > 256) {
                failAllocation = true;
                Reject([&] { Invoke(module, batchCall, {317, 1, Ptr(input.get() + 1), count}); });
                failAllocation = false; assert(tempAllocations == 0);
            }
            boundsCalls = 0; failBoundsAt = 1;
            Reject([&] { Invoke(module, polyCall, {317, count, Ptr(input.get() + 1)}); });
            failBoundsAt = 0;
            Reject([&] { Invoke(module, polyCall, {317, count + 1, Ptr(input.get() + 1)}); });
            assert(tempAllocations == 0 && !std::memcmp(input.get(), saved.data(), saved.size()));
            ranges.pop_back();
        }
        const int beforePolys = backendCalls;
        assert(Invoke(module, polyCall, {317, 0, 0}) == 0);
        assert(Invoke(module, batchCall, {317, 4, 0, 0}) == 0);
        assert(Invoke(module, batchCall, {317, 0, 0, 10}) == 0);
        assert(Invoke(module, polyCall, {0, 4, 0}) == 0);
        assert(backendCalls == beforePolys);
        for (int negative : {-1, INT_MIN}) {
            Reject([&] { Invoke(module, polyCall, {317, negative, 0}); });
            Reject([&] { Invoke(module, batchCall, {317, 4, 0, negative}); });
        }
        Reject([&] { Invoke(module, polyCall, {317, 4, 0}); });
        Reject([&] { Invoke(module, batchCall, {317, INT_MAX, 1, INT_MAX}); });
        Reject([&] { Invoke(module, batchCall, {317, INT_MAX / 24 + 1, 1, 1}); });
        Reject([&] { Invoke(module, batchCall, {317, INT_MAX / 24, 1, 2}); });
        if constexpr (sizeof(intptr_t) > sizeof(int32_t)) {
            const intptr_t wide = static_cast<intptr_t>(static_cast<uint64_t>(INT_MAX) + 1);
            Reject([&] { Invoke(module, polyCall, {317, wide, 1}); });
            Reject([&] { Invoke(module, batchCall, {317, 1, 1, wide}); });
        }
    }
    // Unimplemented imports must leave the result untouched and read no args.
    for (auto call : {Call::CG_LOADCAMERA, Call::CG_TESTPRINTINT}) {
        intptr_t result = 12345; const int calls = backendCalls;
        assert(!CL_RTCWRenderCall(cgvm, call, nullptr, result) && result == 12345 && backendCalls == calls);
    }
}
int main() {
    // Original SP's disabled zombie-hit entrypoint must never examine its
    // optional vectors or alter the renderer, even for reset/invalid entities.
    for (auto id : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE}) {
        profile.id = id; const int before = backendCalls, bounds = boundsCalls;
        intptr_t result = 12345;
        const bool sp = id == COM_GAME_RTCW_SP;
        assert(CL_RTCWRenderCall(cgvm, Call::CG_RB_ZOMBIEFXADDNEWHIT, nullptr, result) == sp);
        assert(result == (sp ? 0 : 12345));
        if (sp) for (intptr_t number : {intptr_t(-1), intptr_t(0), intptr_t(127), intptr_t(1<<30), INTPTR_MAX})
            assert(Invoke(Module::CGAME, Call::CG_RB_ZOMBIEFXADDNEWHIT, {number, -1, INTPTR_MAX}) == 0);
        assert(backendCalls == before && boundsCalls == bounds);
    }
    static_assert(sizeof(fntech3_rtcw_orientation_t) == 48);
    orientation_t orientation{};
    assert(!fntech3::render::WriteRTCWOrientation(nullptr, orientation));
    const uint32_t orientationWords[12] = {0x7fa00001, 0xff800000, 0x80000000, 0x3f800000, 0xffffffff,
        0x12345678, 0, 0x7fc10000, 0xff800000, 0x80000000, 0x123, 0xabcd};
    std::memcpy(&orientation, orientationWords, sizeof(orientationWords));
    unsigned char orientationBytes[49];
    assert(fntech3::render::WriteRTCWOrientation(orientationBytes + 1, orientation));
    assert(!std::memcmp(orientationBytes + 1, orientationWords, sizeof(orientationWords)));
    assert(fntech3::render::WriteRTCWOrientation(&orientation, orientation));
    assert(!std::memcmp(&orientation, orientationWords, sizeof(orientationWords)));
    polyVert_t vertex; std::memset(&vertex, 0xa5, sizeof(vertex)); const auto savedVertex = vertex;
    assert(!fntech3::render::ReadRTCWPolyVert(nullptr, vertex));
    assert(!std::memcmp(&vertex, &savedVertex, sizeof(vertex)));
    const uint32_t vertexWords[] = {0x7fa00001, 0xff800000, 0x80000000, 0x3f800000, 0xffffffff, 0x12345678};
    unsigned char unalignedVertex[25];
    std::memcpy(unalignedVertex + 1, vertexWords, sizeof(vertexWords));
    assert(fntech3::render::ReadRTCWPolyVert(unalignedVertex + 1, vertex));
    assert(!std::memcmp(&vertex, vertexWords, sizeof(vertexWords)));
    Mode<fntech3_rtcw_sp_refEntity_t, fntech3_rtcw_sp_refdef_t>(COM_GAME_RTCW_SP);
    Mode<fntech3_rtcw_mp_refEntity_t, fntech3_rtcw_mp_refdef_t>(COM_GAME_RTCW_MP);
    Reject([] { intptr_t result; CL_RTCWRenderCall(nullptr, Call::CG_R_CLEARSCENE, nullptr, result); });
    Reject([] { intptr_t result; CL_RTCWRenderCall(nullptr, Call::CG_R_LERPTAG, nullptr, result); });
    for (auto id : {COM_GAME_AUTO, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE}) {
        profile.id = id; intptr_t result = 12345; const int before = backendCalls;
        assert(!CL_RTCWRenderCall(cgvm, Call::CG_R_CLEARSCENE, nullptr, result));
        assert(!CL_RTCWRenderCall(cgvm, Call::CG_R_LERPTAG, nullptr, result));
        assert(!CL_RTCWRenderCall(cgvm, Call::CG_R_GETMODELSHADER, nullptr, result));
        assert(!CL_RTCWRenderCall(uivm, Call::UI_CM_LERPTAG, nullptr, result));
        intptr_t args[] = {42};
        assert(CL_CgameSystemCalls(args) == 10042 && CL_UISystemCalls(args) == 10042);
        assert(backendCalls == before && result == 12345);
    }
}
'''


class RTCWRenderImportTests(unittest.TestCase):
    def test_resource_scene_and_scalar_imports_preserve_each_mode_contract(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        bodies = []
        for filename, signature in [('cl_cgame.cpp', 'CL_CgameSystemCalls'), ('cl_ui.cpp', 'CL_UISystemCalls')]:
            source = (ROOT / 'code/client' / filename).read_text(encoding='utf-8')
            start = source.index(f'static intptr_t {signature}(')
            end = source.index('\t\tswitch ( call )', start)
            prefix = source[start:end]
            self.assertIn('CL_RTCWRenderCall(', prefix)
            bodies.append(prefix + '\t}\nreturn 10000 + operation;\n}\n')
        with tempfile.TemporaryDirectory(prefix='rtcw-render-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PRELUDE + '\n'.join(bodies) + CHECKS, encoding='utf-8')
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


if __name__ == '__main__':
    unittest.main()
