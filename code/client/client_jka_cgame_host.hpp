/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_HOST_HPP
#define FNTECH3_CLIENT_JKA_CGAME_HOST_HPP
#include "client_jka_cgame.hpp"
#include "client_jka_command_values.hpp"
#include "client_jka_glconfig.hpp"
#include "client_jka_font.hpp"
#include "client_jka_strings.hpp"
#include "client_jka_sound.hpp"
#include "../compat/jka_render_adapter.hpp"
#include "../compat/jka_lightstyle.hpp"
#include "../compat/jka_ui_exports.h"
#include <climits>
#include <limits>
#include <vector>

/* Include after CL_FindSnapshot, CL_GetUserCmd and the canonical HUD helpers.
 * All native record publication uses memcpy so the module cannot force an
 * unaligned engine-POD dereference. Unimplemented imports stop by name before
 * the shared dispatcher can interpret a Raven ordinal as another operation. */
namespace {
void *jkaCgameSharedBuffer = nullptr;
fntech3::jka::lightstyle::Store jkaCgameLightStyles;
static void CL_JKACgameGhoul2Clear();
static void CL_JKACgameAmbientClear();
static void CL_JKACgameEffectsClear();

static void CL_JKACgameClear() {
    fntech3::jka::command::selections.Reset();
    jkaCgameLightStyles.Reset();
    S_JKASetSoundRegistrationQuiet(qfalse);
    S_ClearJKAMusicPreload();
    CL_JKACgameGhoul2Clear();
    CL_JKACgameAmbientClear();
    CL_JKACgameEffectsClear();
    jkaCgameSharedBuffer = nullptr;
    jkaFonts.clear();
    CL_JKAStringReset();
}

static void *CL_JKACgameSpan(intptr_t address, std::size_t bytes) {
    if (!address || bytes > (std::numeric_limits<std::uintptr_t>::max)() - std::uintptr_t(address))
        Com_Error(ERR_DROP, "Invalid Jedi Academy cgame record address");
    VM_CHECKBOUNDS(cgvm, address, bytes);
    return VM_ExplicitArgPtr(cgvm, address);
}

static std::array<char,MAX_QPATH> CL_JKACgameMusicName(intptr_t address) {
    std::array<char,MAX_QPATH> name{};
    if(!address)return name;
    for(std::size_t i=0;i<name.size();++i){
        if(std::uintptr_t(address)>(std::numeric_limits<std::uintptr_t>::max)()-i)
            Com_Error(ERR_DROP,"Invalid Academy cgame music name address");
        name[i]=*static_cast<const char *>(CL_JKACgameSpan(std::uintptr_t(address)+i,1));
        if(!name[i])return name;
    }
    Com_Error(ERR_DROP,"Academy cgame music name exceeds MAX_QPATH");
    return name;
}

static const float *CL_JKACgameVector(intptr_t address, bool optional = false) {
    if (!address && optional) return nullptr;
    const auto *value = static_cast<const float *>(CL_JKACgameSpan(address, sizeof(vec3_t)));
    if (!fntech3::jka::client::FiniteVector(value)) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame collision vector");
    return value;
}

static void CL_JKACgameTrace(fntech3::jka::module::Call call, const intptr_t *args) {
    using fntech3::jka::module::Call;
    void *destination = CL_JKACgameSpan(args[1], sizeof(fntech3_jka_trace_t));
    const bool transformed = call == Call::CG_CM_TRANSFORMEDBOXTRACE || call == Call::CG_CM_TRANSFORMEDCAPSULETRACE;
    const qboolean capsule = call == Call::CG_CM_CAPSULETRACE || call == Call::CG_CM_TRANSFORMEDCAPSULETRACE ? qtrue : qfalse;
    const float *start = CL_JKACgameVector(args[2]), *end = CL_JKACgameVector(args[3]);
    const float *mins = CL_JKACgameVector(args[4], true), *maxs = CL_JKACgameVector(args[5], true);
    const float zero[3]{};
    if (!fntech3::jka::client::Bounds(mins ? mins : zero, maxs ? maxs : zero))
        Com_Error(ERR_DROP, "Invalid Jedi Academy cgame collision bounds");
    trace_t internal{};
    if (transformed) CM_TransformedBoxTrace(&internal, start, end, mins, maxs,
        static_cast<int>(args[6]), static_cast<int>(args[7]), CL_JKACgameVector(args[8]),
        CL_JKACgameVector(args[9]), capsule);
    else CM_BoxTrace(&internal, start, end, mins, maxs, static_cast<int>(args[6]), static_cast<int>(args[7]), capsule);
    fntech3_jka_trace_t output{};
    if (!fntech3::jka::state::FromInternal(internal, output))
        Com_Error(ERR_DROP, "Invalid Jedi Academy cgame collision result");
    std::memcpy(destination, &output, sizeof(output));
}
} // namespace

#include "client_jka_cgame_ghoul2.hpp"
#include "client_jka_cgame_render.hpp"
#include "client_jka_cgame_ambient.hpp"
#include "client_jka_cgame_effects.hpp"

static bool CL_JKACgameCall(intptr_t *args, intptr_t &operation, intptr_t &result) {
    using namespace fntech3::jka;
    using module::Call;
    if (!cgvm || !cgvm->entryPoint || cgvm->dllExports)
        Com_Error(ERR_DROP, "Jedi Academy cgame requires its legacy native module");
    result = 0;
    const auto call = module::Lookup(COM_GAME_JKA_MP, module::Module::CGAME, module::Direction::IMPORT, args[0]);
    if (CL_JKACgameGhoul2Call(args, call, result)) return true;
    if (CL_JKACgameAmbientCall(args, call, result)) return true;
    if (CL_JKACgameEffectsCall(args, call, result)) return true;
    if (CL_JKAFontCall(cgvm, args, result, FNTECH3_JKA_CG_R_REGISTERFONT,
        [](float x, float y, float width, float height, const auto &glyph, qhandle_t shader) {
            if (!CL_CaptureHidesHud()) CL_HudDrawStretchPic(x, y, width, height,
                glyph.s, glyph.t, glyph.s2, glyph.t2, shader);
        })) return true;
    switch (call) {
    case Call::CG_R_CLEARSCENE:
        fntech3::jka::cgame::scene::state.ClearScene();
        operation=CG_R_CLEARSCENE;
        return false;
    case Call::CG_R_ADDREFENTITYTOSCENE: {
        fntech3_jka_mp_refEntity_t source;
        std::memcpy(&source,CL_JKACgameSpan(args[1],sizeof(source)),sizeof(source));
        if (source.ghoul2) { CL_JKACgameRenderQueue(source); return true; }
        refEntity_t value{};
        const auto status = render::ReadEntity(&source, value);
        if (status != render::Status::Ok) Com_Error(ERR_DROP, "Jedi Academy cgame %s (type=%d, flags=0x%x)",
            render::StatusName(status),source.reType,static_cast<unsigned>(source.renderfx));
        operation = CG_R_ADDREFENTITYTOSCENE;
        return false; // canonical submission retains the engine's scene features
    }
    case Call::CG_R_RENDERSCENE: {
        refdef_t value{};
        const auto status = render::ReadView(CL_JKACgameSpan(args[1], sizeof(fntech3_jka_mp_refdef_t)), value);
        if (status != render::Status::Ok) Com_Error(ERR_DROP, "Jedi Academy cgame %s", render::StatusName(status));
        operation = CG_R_RENDERSCENE;
        return false; // canonical HUD/capture/liquid/lighting path uses the same adapter
    }
    case Call::CG_R_ADDLIGHTTOSCENE:
    case Call::CG_R_ADDADDITIVELIGHTTOSCENE: {
        // Retail MP207/208: one vector and four float words, void return.
        // Marshal into Fn's ordinary renderer lighting and shadow controls.
        if (sizeof(void *) != sizeof(std::uint32_t) || cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP,"Academy scene light requires its x86 native cgame");
        vec3_t origin;
        std::memcpy(origin,CL_JKACgameSpan(args[1],sizeof(origin)),sizeof(origin));
        float values[4];
        for (int index=0;index<4;++index) {
            const auto word=static_cast<std::uint32_t>(args[index+2]);
            std::memcpy(&values[index],&word,sizeof(word));
            if (!render::Finite(values[index]))
                Com_Error(ERR_DROP,"Invalid Academy scene light scalar");
        }
        if (!render::Finite(origin))
            Com_Error(ERR_DROP,"Invalid Academy scene light origin");
        const auto submit=call==Call::CG_R_ADDLIGHTTOSCENE
            ? re.AddLightToScene : re.AddAdditiveLightToScene;
        if (!submit) Com_Error(ERR_DROP,"Academy Fn scene light service is unavailable");
        submit(origin,values[0],values[1],values[2],values[3]);
        return true;
    }
    case Call::CG_R_LERPTAG: {
        CL_JKACgameGhoul2Native();
        if (cgvm->index!=VM_CGAME || !args[6] || args[2]<0 || args[2]>INT_MAX ||
            args[3]<0 || args[3]>INT_MAX || args[4]<0 || args[4]>INT_MAX)
            Com_Error(ERR_DROP,"Invalid Academy cgame tag query scalars or native module");
        void *destination=CL_JKACgameSpan(args[1],sizeof(fntech3_jka_mp_orientation_t));
        const auto name=CL_JKACgameGhoul2Text(args[6]);
        const auto word=static_cast<std::uint32_t>(args[5]);
        float fraction; std::memcpy(&fraction,&word,sizeof(fraction));
        if (!render::Finite(fraction) || !re.LerpTag)
            Com_Error(ERR_DROP,"Invalid Academy cgame tag interpolation or unavailable Fn query");
        orientation_t canonical{};
        const int found=re.LerpTag(&canonical,static_cast<int>(args[2]),static_cast<int>(args[3]),
            static_cast<int>(args[4]),fraction,name.data());
        if ((found!=0 && found!=1) || !render::Finite(canonical.origin))
            Com_Error(ERR_DROP,"Invalid Academy cgame tag query result");
        for (const auto &axis:canonical.axis) if (!render::Finite(axis))
            Com_Error(ERR_DROP,"Invalid Academy cgame tag query axes");
        fntech3_jka_mp_orientation_t external{};
        render::Copy(external.origin,canonical.origin);
        render::Copy(external.axis,canonical.axis);
        std::memcpy(destination,&external,sizeof(external));
        result=found;
        return true;
    }
    case Call::CG_LANGUAGE_ISASIAN:
        result = CL_JKAStringIsAsian();
        return true;
    case Call::CG_LANGUAGE_USESSPACES:
        result = CL_JKAStringUsesSpaces();
        return true;
    case Call::CG_ANYLANGUAGE_READCHARFROMSTRING:
        result = CL_JKAStringReadChar(cgvm, args[1], args[2], args[3]);
        return true;
    case Call::CG_CVAR_REGISTER: {
        vmCvar_t value{};
        void *output = args[1] ? CL_JKACgameSpan(args[1], sizeof(fntech3_jka_vmCvar_t)) : nullptr;
        Cvar_Register(output ? &value : nullptr, VMA(2), VMA(3),
            cgame::CvarFlags(static_cast<uint32_t>(args[4])), cgvm->privateFlag);
        if (output) {
            fntech3_jka_vmCvar_t external{}; records::CvarToExternal(value, external);
            std::memcpy(output, &external, sizeof(external));
        }
        return true;
    }
    case Call::CG_CVAR_UPDATE: {
        void *output = CL_JKACgameSpan(args[1], sizeof(fntech3_jka_vmCvar_t));
        fntech3_jka_vmCvar_t external; std::memcpy(&external, output, sizeof(external));
        vmCvar_t value{}; records::CvarToInternal(external, value); Cvar_Update(&value, cgvm->privateFlag);
        records::CvarToExternal(value, external); std::memcpy(output, &external, sizeof(external));
        return true;
    }
    case Call::CG_FS_GETFILELIST:
        if (args[4] < 1 || args[4] > INT_MAX) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame file list size");
        result = FS_GetFileList(VMA(1), VMA(2),
            static_cast<char *>(CL_JKACgameSpan(args[3], std::size_t(args[4]))), static_cast<int>(args[4]));
        return true;
    case Call::CG_S_STARTLOCALSOUND: {
        // Retail 0x412934 passes the handle and channel to S_StartLocalSound
        // 0x46aed0, which plays it on the listener's own entity, and returns
        // zero. Fn's local-sound path plays it with the channel translated.
        if (args[1] < INT_MIN || args[1] > INT_MAX || args[2] < INT_MIN || args[2] > INT_MAX)
            Com_Error(ERR_DROP, "Invalid Academy local sound handle or channel");
        const int channel = static_cast<int>(args[2]);
        S_StartLocalSoundVolume(static_cast<sfxHandle_t>(args[1]), sound::EngineChannel(channel),
            sound::ListenerVolume(channel));
        return true;
    }
    case Call::CG_S_GETVOICEVOLUME:
        if (sizeof(void *) != sizeof(std::uint32_t) || cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP, "Academy voice volume requires its x86 native cgame");
        result = S_JKAVoiceVolume(args[1]);
        return true;
    case Call::CG_S_REGISTERSOUND:
        // Retail 0x412a25 reads only the name, never a Q3 compression argument.
        result = S_RegisterSound(VMA(1), qfalse);
        return true;
    case Call::CG_S_SHUTUP:
        // Retail0x412a13 stores args[1] at0x10dfe70 and returns zero.
        // Fn applies this diagnostic setting to optional missing samples;
        // it leaves playback and the actual registration result untouched.
        S_JKASetSoundRegistrationQuiet(args[1] ? qtrue : qfalse);
        return true;
    case Call::CG_S_CLEARLOOPINGSOUNDS:
        // Retail 0x41294c supplies no arguments. 0x46b0f0 clears frame activity
        // and the submitted loop count, matching the host's per-frame service.
        S_ClearLoopingSoundsFrame();
        return true;
    case Call::CG_S_STARTBACKGROUNDTRACK: {
        const auto intro=CL_JKACgameMusicName(args[1]);
        const auto loop=CL_JKACgameMusicName(args[2]);
        S_JKAStartBackgroundTrack(intro.data(),loop.data(),args[3]?qtrue:qfalse);
        return true;
    }
    case Call::CG_R_INITWIREFRAMEAUTO: {
        // Retail0x4a2603 registers this optional overlay as disabled by
        // default;0x4c4090 then returns false without generating any data.
        // Fn's renderers do not yet own an Academy wireframe-map cache.
        // Serve the measured disabled mode and refuse a request to enable it.
        const auto *disabled=Cvar_Get("r_autoMapDisable","1",0);
        if(!disabled->integer)
            Com_Error(ERR_DROP,"Academy wireframe automap generation is not implemented; use r_autoMapDisable 1");
        result=qfalse;
        return true;
    }
    case Call::CG_R_SET_LIGHT_STYLE: {
        if(args[1]<0||args[1]>=lightstyle::Count)
            Com_Error(ERR_DROP,"Invalid Academy light-style index");
        if(static_cast<int64_t>(args[2])<INT32_MIN||static_cast<int64_t>(args[2])>UINT32_MAX)
            Com_Error(ERR_DROP,"Invalid Academy light-style color word");
        const auto status=jkaCgameLightStyles.Set(static_cast<int>(args[1]),static_cast<uint32_t>(args[2]));
        if(status==lightstyle::Status::Modulation)
            Com_Error(ERR_DROP,"Academy normal-style RBSP rendering cannot apply colored style zero");
        if(status!=lightstyle::Status::Ok)
            Com_Error(ERR_DROP,"Invalid Academy light-style index");
        return true;
    }
    case Call::CG_GET_ENTITY_TOKEN:
        if (args[2] == -1) {
            // The reset consumes no output buffer, including a null address.
            Com_DPrintf("Academy entity token reset requested (261)\n");
            result = re.GetEntityToken(nullptr, -1);
        } else {
            if (args[2] < 1 || args[2] > INT_MAX)
                Com_Error(ERR_DROP, "Invalid Academy entity token buffer size");
            result = re.GetEntityToken(static_cast<char *>(CL_JKACgameSpan(args[1],
                static_cast<std::size_t>(args[2]))), static_cast<int>(args[2]));
        }
        return true;
    case Call::CG_R_GETDISTANCECULL: {
        // Retail0x4132cf publishes the renderer's four-byte float to args[1].
        void *output = CL_JKACgameSpan(args[1], sizeof(float));
        float distance = 0.0f;
        if (!re.GetJKADistanceCull || !re.GetJKADistanceCull(&distance))
            Com_Error(ERR_DROP, "Academy distanceCull requires a completed Fn renderer world");
        std::memcpy(output, &distance, sizeof(distance));
        return true;
    }
    case Call::CG_R_GET_LIGHT_STYLE: {
        if(args[1]<0||args[1]>=lightstyle::Count)
            Com_Error(ERR_DROP,"Invalid Academy light-style index");
        lightstyle::Color color;
        if(!jkaCgameLightStyles.Get(static_cast<int>(args[1]),color))
            Com_Error(ERR_DROP,"Invalid Academy light-style index");
        std::memcpy(CL_JKACgameSpan(args[2],color.size()),color.data(),color.size());
        return true;
    }
    case Call::CG_R_WORLDEFFECTCOMMAND: {
        if (cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP, "Academy world-effect commands require their native cgame");
        if (!args[1]) return true; // Primary4890ac skips a null command.
        std::array<char, 4> command{};
        const auto address = std::uintptr_t(args[1]);
        if (address > (std::numeric_limits<std::uintptr_t>::max)() - command.size())
            Com_Error(ERR_DROP, "Invalid Academy world-effect command address");
        for (std::size_t i = 0; i < command.size(); ++i) {
            command[i] = *static_cast<const char *>(CL_JKACgameSpan(address + i, 1));
            if (!command[i]) break;
        }
        if (!command[0]) return true;
        if ((command[0] == 'd' || command[0] == 'D') &&
            (command[1] == 'i' || command[1] == 'I') &&
            (command[2] == 'e' || command[2] == 'E') && !command[3]) {
            // No Academy weather creation command is served, so this owned
            // lane has no particle/wind cache to release. Preserve all Fn
            // world, pose, light, fog and audio resources during empty cleanup.
            return true;
        }
        Com_Error(ERR_DROP, "Jedi Academy cgame active world-effect commands are not served yet");
        return true;
    }
    case Call::CG_ROFF_CLEAN:
        if (cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP, "Academy ROFF cleanup requires its native cgame");
        // Primary426807 skips unloading for an empty cache;426823 returns1.
        // Client Cache/Play remain refused, so this owned lane cannot allocate
        // ROFF files or trajectories. Independent server storage is untouched.
        result = 1;
        return true;
    case Call::CG_ROFF_UPDATE_ENTITIES:
        // Retail0x41379b selects only the client lane and returns zero.
        // Its empty-list branches0x42693c/0x4269a9 perform no work. Client
        // Cache/Play imports remain named refusals, so this lane admits no
        // trajectories. The independent server player is never ticked here.
        return true;
    case Call::CG_GETGLCONFIG:
        CL_JKAGetGlconfig(cgvm, args[1]);
        return true;
    case Call::CG_OPENUIMENU: {
        if (sizeof(void *) != sizeof(std::uint32_t) || !CL_UIModuleReady() || !uivm || uivm->index != VM_UI ||
            !uivm->entryPoint || uivm->dllExports)
            Com_Error(ERR_DROP, "Jedi Academy cgame menu request requires its x86 legacy native UI");
        const int menu = FNTech3_JKAUIMenuToInternal(static_cast<int>(args[1]));
        if (menu < 0)
            Com_Error(ERR_DROP, "Jedi Academy cgame UI menu %ld is not served yet", static_cast<long>(args[1]));
        Com_DPrintf("Academy cgame requested UI menu: %ld\n", static_cast<long>(args[1]));
        VM_Call(uivm, 1, UI_SET_ACTIVE_MENU, menu);
        result = 0;
        return true;
    }
    case Call::CG_SETUSERCMDVALUE: {
        if (sizeof(void *) != sizeof(std::uint32_t) || cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP, "Academy command values require its x86 native cgame");
        const float sensitivity = _vmf(args[2]), pitch = _vmf(args[3]);
        const float yaw = _vmf(args[4]), overrideSensitivity = _vmf(args[5]);
        if (!records::Finite(sensitivity) || sensitivity < 0.0f ||
            !records::Finite(pitch) || !records::Finite(yaw) || !records::Finite(overrideSensitivity))
            Com_Error(ERR_DROP, "Invalid Academy cgame command sensitivity values");
        if (pitch != 0.0f || yaw != 0.0f || overrideSensitivity != 0.0f || args[8] != 0)
            Com_Error(ERR_DROP, "Academy cgame angle/sensitivity overrides and fighter controls are not served yet");
        // Retail0x412fe4 carries eight words; the first five contain weapon
        // and four floats, followed by the two selections and fighter flag.
        // Supported neutral overrides retain the existing Fn input pipeline.
        CL_SetUserCmdValue(static_cast<int>(args[1]), 0, sensitivity, 0);
        command::selections.Set(static_cast<int>(args[6]), static_cast<int>(args[7]));
        return true;
    }
    case Call::CG_R_ADDPOLYTOSCENE: {
        if (sizeof(void *) != sizeof(std::uint32_t) || cgvm->index != VM_CGAME)
            Com_Error(ERR_DROP, "Academy polygon records require its x86 native cgame");
        if (!re.AddPolyToScene)
            Com_Error(ERR_DROP, "Academy polygon renderer service is unavailable");
        if (args[2] <= 0 || args[2] > static_cast<intptr_t>(render::MaxPolyVertices))
            Com_Error(ERR_DROP, "Academy polygon vertex count exceeds its Fn scene budget");
        const auto count = static_cast<std::size_t>(args[2]);
        const auto *source = static_cast<const byte *>(CL_JKACgameSpan(args[3],
            count * sizeof(fntech3_jka_mp_polyVert_t)));
        std::vector<polyVert_t> vertices(count);
        for (std::size_t index = 0; index < count; ++index)
            if (!render::ReadPolyVertex(source + index * sizeof(fntech3_jka_mp_polyVert_t), vertices[index]))
                Com_Error(ERR_DROP, "Invalid Academy cgame polygon vertex");
        // Import203 has only shader, count and caller vertices; the retail
        // dispatcher explicitly supplies one polygon at0x412c9b.
        // Fn queues own copies and retain their shader, fog and scene paths.
        re.AddPolyToScene(static_cast<int>(args[1]), static_cast<int>(count), vertices.data(), 1);
        return true;
    }
    case Call::CG_GETGAMESTATE: {
        void *destination = CL_JKACgameSpan(args[1], sizeof(fntech3_jka_mp_gameState_t));
        fntech3_jka_mp_gameState_t output{};
        if (!client::GameState(cl.gameState, output)) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame gamestate transfer");
        std::memcpy(destination, &output, sizeof(output));
        return true;
    }
    case Call::CG_GETSNAPSHOT: {
        void *destination = CL_JKACgameSpan(args[2], sizeof(fntech3_jka_mp_snapshot_t));
        if (args[1] < 0 || args[1] > INT_MAX) return true;
        const auto *source = CL_FindSnapshot(static_cast<int>(args[1]));
        if (!source) return true;
        const client::SnapshotView view{source->snapFlags, source->ping, source->serverTime, source->serverCommandNum,
            source->areamask, &source->ps, &source->jkaVehiclePs, cl.parseEntities,
            static_cast<uint32_t>(source->parseEntitiesNum), MAX_PARSE_ENTITIES, source->numEntities};
        fntech3_jka_mp_snapshot_t output{};
        if (!client::Snapshot(view, output)) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame snapshot transfer");
        std::memcpy(destination, &output, sizeof(output));
        result = 1;
        return true;
    }
    case Call::CG_GETDEFAULTSTATE: {
        void *destination = CL_JKACgameSpan(args[2], sizeof(fntech3_jka_entityState_t));
        if (args[1] < 0 || args[1] >= FNTECH3_JKA_MAX_GENTITIES) return true;
        fntech3_jka_entityState_t output{};
        const entityState_t &source = cl.entityBaselines[static_cast<int>(args[1])];
        if (!(source.eFlags & 0x80)) return true;
        if (!client::DefaultState(source, output)) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame default-state transfer");
        std::memcpy(destination, &output, sizeof(output));
        result = 1;
        return true;
    }
    case Call::CG_GETUSERCMD: {
        void *destination = CL_JKACgameSpan(args[2], sizeof(fntech3_jka_usercmd_t));
        if (args[1] < 0 || args[1] > INT_MAX) return true;
        usercmd_t source{};
        if (!CL_GetUserCmd(static_cast<int>(args[1]), &source)) return true;
        fntech3_jka_usercmd_t output{};
        if (!state::FromInternal(source, output)) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame usercmd transfer");
        std::memcpy(destination, &output, sizeof(output));
        result = 1;
        return true;
    }
    case Call::CG_PC_READ_TOKEN: {
        void *destination = CL_JKACgameSpan(args[2], sizeof(fntech3_jka_mp_pc_token_t));
        if (!botlib_export) Com_Error(ERR_DROP, "Jedi Academy cgame parser is unavailable");
        pc_token_t source{};
        result = botlib_export->PC_ReadTokenHandle(static_cast<int>(args[1]), &source);
        if (result) {
            fntech3_jka_mp_pc_token_t output{}; records::Token(source, output);
            std::memcpy(destination, &output, sizeof(output));
        }
        return true;
    }
    case Call::CG_REAL_TIME: {
        void *destination = CL_JKACgameSpan(args[1], sizeof(fntech3_jka_mp_qtime_t));
        qtime_t source{}; result = Com_RealTime(&source);
        fntech3_jka_mp_qtime_t output{}; records::Time(source, output);
        std::memcpy(destination, &output, sizeof(output));
        return true;
    }
    case Call::CG_SP_GETSTRINGTEXTSTRING:
        if (args[3] < 1 || args[3] > INT_MAX) Com_Error(ERR_DROP, "Invalid Jedi Academy cgame translation size");
        result = CL_JKAStringText(cgvm, args[1], args[2], static_cast<int>(args[3]), true);
        return true;
    case Call::CG_CM_BOXTRACE: case Call::CG_CM_CAPSULETRACE:
    case Call::CG_CM_TRANSFORMEDBOXTRACE: case Call::CG_CM_TRANSFORMEDCAPSULETRACE:
        CL_JKACgameTrace(call, args);
        return true;
    case Call::CG_SET_SHARED_BUFFER:
        // Retail 0x41418a retains this borrowed address. Callback exports stay
        // gated until their own data and reentrancy contracts are implemented.
        jkaCgameSharedBuffer = CL_JKACgameSpan(args[1], FNTECH3_JKA_CGAME_SHARED_BUFFER_BYTES);
        return true;
    default:
        operation = cgame::SharedImport(call);
        if (operation >= 0) return false;
        const auto *descriptor = module::Describe(call);
        Com_Error(ERR_DROP, "Unsupported jka-mp cgame import %ld (%s)", static_cast<long>(args[0]),
            descriptor ? descriptor->name : "unknown ordinal");
        return true;
    }
}
#endif
