/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_WOLFET_GAME_HPP
#define FNTECH3_SERVER_WOLFET_GAME_HPP
#include "../compat/wolfet_module_abi.h"
#include "../compat/wolfet_state_adapter.hpp"
#include "../compat/wolfet_tags.hpp"
#include "server_wolfet_client.hpp"
#include "server_wolfet_botlib.hpp"
#include <utility>

struct WolfETTagFile { std::string name; std::vector<fntech3::wolfet::tags::Tag> tags; };
static std::vector<WolfETTagFile> wolfETTagFiles;

static int SV_WolfETRegisterTag(intptr_t address) {
    const auto name = VM_RTCWFileName(gvm, address);
    if (!name[0] || std::strstr(name.data(), "..") || std::strchr(name.data(), ':') ||
        name[0] == '/' || name[0] == '\\') Com_Error(ERR_DROP, "Invalid WolfET tag path");
    size_t total = 0;
    for (size_t i = 0; i < wolfETTagFiles.size(); ++i) {
        if (!Q_stricmp(wolfETTagFiles[i].name.c_str(), name.data())) return static_cast<int>(i + 1);
        total += wolfETTagFiles[i].tags.size();
    }
    if (wolfETTagFiles.size() >= 64) Com_Error(ERR_DROP, "WolfET tag file limit exceeded");
    void *data = nullptr;
    const int length = FS_ReadFile(name.data(), &data);
    if (length < 0 || !data) return 0;
    WolfETTagFile file{name.data(), {}};
    const bool valid = fntech3::wolfet::tags::Parse(data, static_cast<size_t>(length), file.tags);
    FS_FreeFile(data);
    if (!valid) { Com_Printf(S_COLOR_YELLOW "Invalid WolfET tag file: %s\n", name.data()); return 0; }
    if (total + file.tags.size() >= 256) Com_Error(ERR_DROP, "WolfET tag count limit exceeded");
    wolfETTagFiles.push_back(std::move(file));
    return static_cast<int>(wolfETTagFiles.size());
}

// Native ET arguments are decoded before the shared engine dispatcher.
// Only imports with checked argument contracts enter the common switch.
static bool SV_WolfETGameCall(const intptr_t *args, intptr_t &operation, intptr_t &result) {
    using namespace fntech3::wolfet::module;
    const auto call = Lookup(Module::GAME, Direction::IMPORT, args[0]);
    const auto pointer = [args](int n) { return VM_ArgPtr(args[n]); };
    if (SV_WolfETBotLibCall(gvm, call, args, result)) return true;
    if (SV_WolfETGoalCall(gvm, call, args, result)) return true;
    if (SV_WolfETActionCall(gvm, call, args, result)) return true;
    if (SV_WolfETAIStateCall(gvm, call, args, result)) return true;
    if (SV_WolfETNavigationCall(gvm, call, args, result)) return true;
    switch (call) {
    case Call::G_CVAR_LATCHEDVARIABLESTRINGBUFFER: {
        if (args[3] < 0 || args[3] > INT_MAX) Com_Error(ERR_DROP, "Invalid WolfET cvar buffer size");
        if (args[3]) {
            const auto name = VM_RTCWFileName(gvm, args[1]);
            auto *output = static_cast<char *>(VM_RTCWFilePointer(gvm, args[2], args[3]));
            Cvar_LatchedVariableStringBufferSafe(name.data(), output, static_cast<int>(args[3]), gvm->privateFlag);
        }
        result = 0; return true;
    }
    case Call::G_DROP_CLIENT: {
        // ET adds a temporary-ban length that Quake's two-argument import lacks.
        const int client = static_cast<int>(args[1]);
        // Drop reasons are module text, not paths: a long one must not be fatal.
        const auto reason = SV_RTCWString<MAX_STRING_CHARS>(gvm, args[2]);
        if (args[3] < INT_MIN || args[3] > INT_MAX)
            Com_Error(ERR_DROP, "Invalid WolfET drop-client ban length");
        if (fntech3::wolfet::server::ValidClient(client)) {
            SV_DropClient(&SV_ClientForIndex(client), reason.data());
            if (args[3])
                fntech3::wolfet::server::TempBanAddress(
                    SV_ClientForIndex(client).netchan.remoteAddress, static_cast<int>(args[3]));
        }
        result = 0; return true;
    }
    case Call::G_BOT_ALLOCATE_CLIENT:
        // ET passes a preferred slot; the shared import takes none.
        result = SV_WolfETAllocateBotClient(static_cast<int>(args[1])); return true;
    case Call::G_SENDMESSAGE: {
        if (args[3] < 0 || args[3] > fntech3::wolfet::server::MaxBinaryMessage)
            Com_Error(ERR_DROP, "Invalid WolfET binary message length");
        const auto *payload = args[3]
            ? static_cast<const char *>(VM_RTCWFilePointer(gvm, args[2], args[3])) : nullptr;
        fntech3::wolfet::server::SendBinaryMessage(
            static_cast<int>(args[1]), payload, static_cast<int>(args[3]));
        result = 0; return true;
    }
    case Call::G_MESSAGESTATUS:
        result = fntech3::wolfet::server::BinaryMessageStatus(static_cast<int>(args[1]));
        return true;
    case Call::G_REGISTERSOUND:
        // ET's dedicated server has no sound system; its stub returns no handle.
        result = 0; return true;
    case Call::G_GET_SOUND_LENGTH:
        // Matching ET's dedicated stub, which drops rather than inventing a length.
        Com_Error(ERR_DROP, "WolfET sound length is unavailable on a dedicated server");
        result = 0; return true;
    case Call::PB_STAT_REPORT:
        // Original ET exposes a void, inert report import. No service success.
        result = 0; return true;
    case Call::G_REGISTERTAG:
        result = SV_WolfETRegisterTag(args[1]); return true;
    case Call::G_GETTAG: {
        result = qfalse;
        if (args[2] < 1 || static_cast<uintptr_t>(args[2]) > wolfETTagFiles.size()) return true;
        const auto name = VM_RTCWFileName(gvm, args[3]);
        auto *output = static_cast<orientation_t *>(VM_RTCWFilePointer(gvm, args[4], sizeof(orientation_t)));
        for (const auto &tag : wolfETTagFiles[args[2] - 1].tags) {
            if (Q_stricmp(tag.name.data(), name.data())) continue;
            std::memcpy(output->origin, tag.transform.data(), sizeof(output->origin));
            std::memcpy(output->axis, tag.transform.data() + 3, sizeof(output->axis));
            result = qtrue; break;
        }
        return true;
    }
    case Call::G_GET_USERCMD: {
        fntech3_wolfet_usercmd_t output{};
        usercmd_t input{}; SV_GetUsercmd(args[1], &input);
        if (!fntech3::wolfet::state::FromInternal(input, output))
            Com_Error(ERR_DROP, "Invalid WolfET user command");
        std::memcpy(VM_RTCWFilePointer(gvm, args[2], sizeof(output)), &output, sizeof(output));
        result = 0; return true;
    }
    case Call::G_TRACE:
    case Call::G_TRACECAPSULE: {
        trace_t internal{}; fntech3_wolfet_trace_t external{};
        auto *output = VM_RTCWFilePointer(gvm, args[1], sizeof(external));
        VM_RTCWFilePointer(gvm, args[2], sizeof(vec3_t));
        VM_RTCWFilePointer(gvm, args[5], sizeof(vec3_t));
        for (int n = 3; n <= 4; ++n) if (args[n]) VM_RTCWFilePointer(gvm, args[n], sizeof(vec3_t));
        SV_Trace(&internal, static_cast<float *>(pointer(2)), static_cast<float *>(pointer(3)),
            static_cast<float *>(pointer(4)), static_cast<float *>(pointer(5)), args[6], args[7],
            call == Call::G_TRACECAPSULE ? qtrue : qfalse);
        if (!fntech3::wolfet::state::FromInternal(internal, external)) Com_Error(ERR_DROP, "Invalid WolfET trace");
        std::memcpy(output, &external, sizeof(external)); result = 0; return true;
    }
    case Call::G_CVAR_REGISTER:
        VM_CHECKBOUNDS(gvm, args[1], sizeof(fntech3_rtcw_vmCvar_t));
        Cvar_RegisterRTCW(pointer(1), static_cast<char *>(pointer(2)), static_cast<char *>(pointer(3)), args[4], gvm->privateFlag);
        result = 0; return true;
    case Call::G_CVAR_UPDATE:
        VM_CHECKBOUNDS(gvm, args[1], sizeof(fntech3_rtcw_vmCvar_t));
        Cvar_UpdateRTCW(pointer(1), gvm->privateFlag); result = 0; return true;
    case Call::G_FS_RENAME:
        VM_RTCWRenameFile(gvm, args[1], args[2]); result = 0; return true;
    case Call::G_FS_WRITE:
        result = VM_RTCWWriteFile(gvm, args[1], args[2], args[3], H_QAGAME); return true;
    case Call::G_FS_READ: {
        if (args[2] < 0 || args[2] > INT_MAX || args[3] < INT_MIN || args[3] > INT_MAX)
            Com_Error(ERR_DROP, "Invalid WolfET file read arguments");
        auto *buffer = args[2] ? VM_RTCWFilePointer(gvm, args[1], args[2]) : nullptr;
        FS_VM_ReadFile(buffer, static_cast<int>(args[2]), static_cast<int>(args[3]), H_QAGAME);
        result = 0; return true;
    }
    case Call::G_REAL_TIME:
        VM_CHECKBOUNDS(gvm, args[1], sizeof(fntech3_rtcw_qtime_t));
        result = Com_RealTimeRTCW(pointer(1)); return true;
    case Call::BOTLIB_PC_READ_TOKEN: {
        fntech3_wolfet_pc_token_t output{};
        pc_token_t token{};
        auto *destination = VM_RTCWFilePointer(gvm, args[2], sizeof(output));
        result = botlib_export->PC_ReadTokenWithLocation(args[1], &token, &output.line, &output.linescrossed);
        if (result) {
            output.type = token.type; output.subtype = token.subtype;
            output.intvalue = token.intvalue; output.floatvalue = token.floatvalue;
            std::memcpy(output.string, token.string, sizeof(output.string));
            std::memcpy(destination, &output, sizeof(output));
        }
        return true;
    }
    case Call::BOTLIB_PC_UNREAD_TOKEN:
        result = botlib_export->PC_UnreadTokenHandle(args[1]); return true;
    case Call::BOTLIB_PC_SOURCE_FILE_AND_LINE: {
        // Original ET's game diagnostic callers reserve 128 filename bytes.
        auto *name = static_cast<char *>(VM_RTCWFilePointer(gvm, args[2], 128));
        auto *line = static_cast<int *>(VM_RTCWFilePointer(gvm, args[3], sizeof(int)));
        char internalName[MAX_STRING_CHARS]{};
        int internalLine = 0;
        result = botlib_export->PC_SourceFileAndLine(args[1], internalName, &internalLine);
        if (result) {
            if (!std::memchr(internalName, 0, sizeof(internalName)))
                Com_Error(ERR_DROP, "Unterminated WolfET parser source filename");
            Q_strncpyz(name, internalName, 128);
            *line = internalLine;
        }
        return true;
    }
#define ET_SHARED(name) case Call::name: operation = name; return false;
    ET_SHARED(G_PRINT)
    ET_SHARED(G_ERROR)
    ET_SHARED(G_MILLISECONDS)
    ET_SHARED(G_CVAR_SET)
    ET_SHARED(G_CVAR_VARIABLE_INTEGER_VALUE)
    ET_SHARED(G_CVAR_VARIABLE_STRING_BUFFER)
    ET_SHARED(G_ARGC)
    ET_SHARED(G_ARGV)
    ET_SHARED(G_FS_FOPEN_FILE)
    ET_SHARED(G_FS_FCLOSE_FILE)
    ET_SHARED(G_FS_GETFILELIST)
    ET_SHARED(G_SEND_CONSOLE_COMMAND)
    ET_SHARED(G_LOCATE_GAME_DATA)
    ET_SHARED(G_SEND_SERVER_COMMAND)
    ET_SHARED(G_BOT_FREE_CLIENT)
    ET_SHARED(G_DEBUG_POLYGON_CREATE)
    ET_SHARED(G_DEBUG_POLYGON_DELETE)
    ET_SHARED(G_SET_CONFIGSTRING)
    ET_SHARED(G_GET_CONFIGSTRING)
    ET_SHARED(G_GET_USERINFO)
    ET_SHARED(G_SET_USERINFO)
    ET_SHARED(G_GET_SERVERINFO)
    ET_SHARED(G_GET_ENTITY_TOKEN)
    ET_SHARED(G_SNAPVECTOR)
    ET_SHARED(G_SET_BRUSH_MODEL)
    ET_SHARED(G_LINKENTITY)
    ET_SHARED(G_UNLINKENTITY)
    ET_SHARED(G_ENTITIES_IN_BOX)
    ET_SHARED(G_ENTITY_CONTACT)
    ET_SHARED(G_ENTITY_CONTACTCAPSULE)
    ET_SHARED(G_POINT_CONTENTS)
    ET_SHARED(G_IN_PVS)
    ET_SHARED(G_IN_PVS_IGNORE_PORTALS)
    ET_SHARED(G_ADJUST_AREA_PORTAL_STATE)
    ET_SHARED(G_AREAS_CONNECTED)
    ET_SHARED(BOTLIB_PC_ADD_GLOBAL_DEFINE)
    ET_SHARED(BOTLIB_PC_LOAD_SOURCE)
    ET_SHARED(BOTLIB_PC_FREE_SOURCE)
#undef ET_SHARED
    default:
        const auto *description = Describe(call);
        Com_Error(ERR_DROP, "Unsupported WolfET game import %ld (%s)", static_cast<long>(args[0]),
            description ? description->name : "unknown ordinal");
        result = 0; return true;
    }
}
#endif
