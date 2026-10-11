/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
extern cvar_t *showpackets, *showdrop, *qport;

/* Process real production netchan packets with an inert host. No sockets,
 * filesystem, input devices or game process are involved. */
extern "C" {
cvar_t quiet{};
cvar_t *cl_packetdelay = &quiet, *sv_packetdelay = &quiet;
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_Error(errorParm_t, const char *fmt, ...) { std::fprintf(stderr, "%s\n", fmt); std::abort(); }
cvar_t *Cvar_Get(const char *, const char *, int) { return &quiet; }
void Cvar_SetDescription(cvar_t *, const char *) {}
int Sys_Milliseconds() { return 0; }
void Sys_SendPacket(int, const void *, const netadr_t *) { std::abort(); }
qboolean Sys_StringToAdr(const char *, netadr_t *, netadrtype_t) { return qfalse; }
qboolean Sys_IsLANAddress(const netadr_t *) { return qfalse; }
const char *NET_AdrToString(const netadr_t *) { return "test"; }
void *Z_Malloc(int size) { return std::calloc(1, size); }
void *S_Malloc(int size) { return std::calloc(1, size); }
void *Z_MallocDebug(int size, const char *, const char *, int) { return std::calloc(1, size); }
void Z_Free(void *memory) { std::free(memory); }
int Com_GameIsMOHAA() { return 0; }
int Com_GameDefaultServerPort() { return 29070; }
void Huff_Compress(msg_t *, int) { std::abort(); }
short BigShort(short value) { const auto word = static_cast<unsigned short>(value); return static_cast<short>((word << 8) | (word >> 8)); }
const char *QDECL va(const char *, ...) { return ""; }
void Q_strncpyz(char *to, const char *from, int size) { std::snprintf(to, size, "%s", from); }
int Q_CountChar(const char *text, char value) { int n = 0; for (; *text; ++text) n += *text == value; return n; }
#ifndef Q_vsnprintf
int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap) { return std::vsnprintf(str, size, format, ap); }
#endif
}

namespace {
constexpr int Fragment = MAX_PACKETLEN - 100;
void Packet(msg_t &msg, std::vector<byte> &storage, netsrc_t socket,
    int sequence, int offset, int length, const byte *data) {
    MSG_InitOOB(&msg, storage.data(), static_cast<int>(storage.size()));
    MSG_WriteLong(&msg, static_cast<int>(static_cast<unsigned>(sequence) | 0x80000000u));
    if (socket == NS_SERVER) MSG_WriteShort(&msg, 1234);
    MSG_WriteShort(&msg, offset); MSG_WriteShort(&msg, length);
    if (length > 0) MSG_WriteData(&msg, data, length);
}

int FullReassembly(netchanWireProfile_t profile, netsrc_t socket, int size) {
    std::vector<byte> payload(static_cast<std::size_t>(size));
    for (int n = 0; n < size; ++n) payload[n] = static_cast<byte>(n * 29 + (n >> 8));
    std::vector<byte> packet(static_cast<std::size_t>(MAX_GAME_MSGLEN) + 4);
    netchan_t chan{}; netadr_t remote{};
    Netchan_Setup(socket, &chan, &remote, 1234, 99, profile);
    for (int offset = 0; offset <= size; offset += Fragment) {
        const int length = std::min(Fragment, size-offset);
        msg_t msg{}; Packet(msg, packet, socket, 5, offset, length, payload.data()+offset);
        const bool final = length < Fragment;
        CHECK(!!Netchan_Process(&chan, &msg) == final);
        if (final) {
            CHECK(chan.incomingSequence == 5 && chan.fragmentLength == 0);
            CHECK(msg.cursize == size+4 && msg.readcount == 4 && msg.bit == 32);
            CHECK(!std::memcmp(msg.data+4, payload.data(), payload.size()));
        } else CHECK(chan.fragmentLength == offset+length);
    }
    return 0;
}

int InvalidFragments() {
    std::array<byte, Fragment> payload{};
    std::vector<byte> packet(static_cast<std::size_t>(MAX_GAME_MSGLEN) + 4);
    netchan_t chan{}; netadr_t remote{};
    Netchan_Setup(NS_CLIENT, &chan, &remote, 0, 0, NETCHAN_WIRE_JKA_MP);
    msg_t msg{};
    Packet(msg, packet, NS_CLIENT, 5, 0, Fragment, payload.data());
    CHECK(!Netchan_Process(&chan, &msg) && chan.fragmentLength == Fragment);
    const auto baseline = chan;
    for (const int offset : {32768, 49152, 65535}) {
        Packet(msg, packet, NS_CLIENT, 5, offset, 1, payload.data());
        CHECK(!Netchan_Process(&chan, &msg));
        CHECK(chan.fragmentLength == baseline.fragmentLength && chan.incomingSequence == baseline.incomingSequence);
    }
    Packet(msg, packet, NS_CLIENT, 5, Fragment, 1, payload.data());
    msg.cursize = 5; // short offset truncation must not become 65535
    CHECK(!Netchan_Process(&chan, &msg) && chan.fragmentLength == baseline.fragmentLength);
    Packet(msg, packet, NS_CLIENT, 5, Fragment, -1, payload.data());
    CHECK(!Netchan_Process(&chan, &msg));
    CHECK(chan.fragmentLength == baseline.fragmentLength);
    Packet(msg, packet, NS_CLIENT, 5, Fragment, 1, payload.data());
    packet[6] = 2; // declared payload exceeds the datagram's actual bytes
    CHECK(!Netchan_Process(&chan, &msg));
    CHECK(chan.fragmentLength == baseline.fragmentLength);
    // A terminal datagram cannot extend the coherent message past 48 KiB.
    chan.fragmentLength = Netchan_MessageLimit(NETCHAN_WIRE_JKA_MP);
    chan.fragmentSequence = 5;
    Packet(msg, packet, NS_CLIENT, 5, chan.fragmentLength, 1, payload.data());
    CHECK(!Netchan_Process(&chan, &msg));
    CHECK(chan.fragmentLength == Netchan_MessageLimit(NETCHAN_WIRE_JKA_MP));
    return 0;
}
} // namespace
int main() {
    showpackets = showdrop = qport = &quiet;
    for (const auto socket : {NS_CLIENT, NS_SERVER}) {
        for (const int size : {32767, 32768, 32769, 48000, 49151, 49152, Fragment*30})
            if (const auto result = FullReassembly(NETCHAN_WIRE_JKA_MP, socket, size)) return result;
        if (const auto result = FullReassembly(NETCHAN_WIRE_LEGACY_Q3, socket, 16000)) return result;
    }
    return InvalidFragments();
}
