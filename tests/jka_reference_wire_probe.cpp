/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional observation of OpenJK's separately checked-out, pinned codec.
 * Compile with FNTECH3_JKA_REFERENCE_MSG and FNTECH3_JKA_REFERENCE_HUFFMAN
 * naming codemp/qcommon/msg.cpp and huffman.cpp. No implementation is copied
 * into this tree; its GPLv2 source is compiled in the disposable probe only.
 * The probe never launches a game or opens a retail installation. */
#include "jka_wire_cases.hpp"
#include <cstdlib>
#include <string>
#include <vector>
#include FNTECH3_JKA_REFERENCE_MSG
#include FNTECH3_JKA_REFERENCE_HUFFMAN

cvar_t *cl_shownet = nullptr;
server_t sv{};
sharedEntity_t *SV_GentityNum(int) { return nullptr; }
extern "C" void Q_strncpyz(char *dest, const char *src, int size) { std::snprintf(dest, size, "%s", src); }
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_Error(int, const char *fmt, ...) { std::fprintf(stderr, "%s\n", fmt); std::abort(); }
void *Z_Malloc(int size, memtag_t, qboolean zero, int) { return zero ? std::calloc(1, size) : std::malloc(size); }
long FS_FOpenFileRead(const char *, fileHandle_t *, qboolean) { return -1; }
int FS_Read(void *, int, fileHandle_t) { return 0; }
void FS_FCloseFile(fileHandle_t) {}
char *QDECL va(const char *, ...) { return nullptr; }

namespace {
namespace codec = fntech3::jka::message;
struct Writer {
    msg_t &msg;
    void Write(std::uint32_t value, int bits) { MSG_WriteBits(&msg, static_cast<int>(value), bits); }
    bool Good() const { return !msg.overflowed; }
    bool Fail() { msg.overflowed = qtrue; return false; }
};
struct Reader {
    msg_t &msg;
    std::uint32_t Read(int bits) { return MSG_ReadBits(&msg, bits); }
    bool Good() const { return msg.readcount <= msg.cursize; }
    bool Fail() { msg.readcount = msg.cursize + 1; return false; }
};
void Check(bool good, const char *reason, unsigned kind, std::size_t index, unsigned alignment) {
    if (good) return;
    std::fprintf(stderr, "kind %u, case %zu, alignment %u: %s\n", kind, index, alignment, reason);
    std::exit(1);
}
template<class State> void Record(jka_cases::Digest &digest, std::size_t index,
    unsigned alignment, const msg_t &msg, const State &state) {
    digest.Word(static_cast<std::uint32_t>(index)); digest.Word(alignment);
    digest.Word(msg.bit); digest.Word(msg.cursize);
    digest.Bytes(msg.data, msg.cursize); digest.Bytes(&state, sizeof(state));
}
void Run(unsigned kind, std::size_t count) {
    jka_cases::Digest digest;
    codec::Tables tables;
    for (std::size_t n = 0; n < count; ++n) for (unsigned align = 0; align < 8; ++align) {
        unsigned char bytes[4096]{}, modern[4096]{};
        msg_t ref{}, ours{};
        MSG_Init(&ref, bytes, sizeof(bytes)); MSG_Init(&ours, modern, sizeof(modern));
        if (align) { MSG_WriteBits(&ref, 0x55, align); MSG_WriteBits(&ours, 0x55, align); }
        Writer out{ours};
        if (kind == 0) {
            const auto c = jka_cases::MakeEntity(n);
            entityState_t base{}, sent{}, expected{};
            std::memcpy(&base, &c.from, sizeof(base)); std::memcpy(&sent, &c.to, sizeof(sent));
            MSG_WriteDeltaEntity(&ref, &base, c.remove ? nullptr : &sent, c.force ? qtrue : qfalse);
            Check(codec::WriteEntity(out, &c.from, c.remove ? nullptr : &c.to, c.force, tables), "encode failure", kind, n, align);
            Check(ref.bit == ours.bit && ref.cursize == ours.cursize && !std::memcmp(bytes, modern, ref.cursize), "packet mismatch", kind, n, align);
            const auto written = ref;
            expected = base; auto decoded = c.from;
            MSG_BeginReading(&ref); MSG_BeginReading(&ours);
            if (align) { MSG_ReadBits(&ref, align); MSG_ReadBits(&ours, align); }
            Reader in{ours};
            if (written.bit != static_cast<int>(align)) {
                const int number = MSG_ReadBits(&ref, 10);
                Check(MSG_ReadBits(&ours, 10) == number, "number mismatch", kind, n, align);
                MSG_ReadDeltaEntity(&ref, &base, &expected, number);
                Check(codec::ReadEntity(in, &c.from, &decoded, number, tables), "decode failure", kind, n, align);
            }
            Check(ref.bit == ours.bit && !std::memcmp(&expected, &decoded, sizeof(expected)), "decode mismatch", kind, n, align);
            Record(digest, n, align, written, expected);
        } else if (kind <= 3) {
            const auto c = jka_cases::MakePlayer(n, kind - 1);
            playerState_t base{}, sent{}, expected{};
            std::memcpy(&base, &c.from, sizeof(base)); std::memcpy(&sent, &c.to, sizeof(sent));
            MSG_WriteDeltaPlayerstate(&ref, &base, &sent, c.vehicle ? qtrue : qfalse);
            Check(codec::WritePlayer(out, &c.from, &c.to, c.vehicle, tables), "encode failure", kind, n, align);
            Check(ref.bit == ours.bit && ref.cursize == ours.cursize && !std::memcmp(bytes, modern, ref.cursize), "packet mismatch", kind, n, align);
            const auto written = ref;
            MSG_BeginReading(&ref); MSG_BeginReading(&ours);
            if (align) { MSG_ReadBits(&ref, align); MSG_ReadBits(&ours, align); }
            Reader in{ours}; codec::Player decoded{};
            MSG_ReadDeltaPlayerstate(&ref, &base, &expected, c.vehicle ? qtrue : qfalse);
            Check(codec::ReadPlayer(in, &c.from, &decoded, c.vehicle, tables), "decode failure", kind, n, align);
            Check(ref.bit == ours.bit && !std::memcmp(&expected, &decoded, sizeof(expected)), "decode mismatch", kind, n, align);
            Record(digest, n, align, written, expected);
        } else {
            const auto c = jka_cases::MakeCommand(n);
            usercmd_t base{}, sent{}, expected{};
            std::memcpy(&base, &c.from, sizeof(base)); std::memcpy(&sent, &c.to, sizeof(sent));
            MSG_WriteDeltaUsercmdKey(&ref, c.key, &base, &sent);
            Check(codec::WriteCommand(out, c.key, &c.from, &c.to), "encode failure", kind, n, align);
            Check(ref.bit == ours.bit && ref.cursize == ours.cursize && !std::memcmp(bytes, modern, ref.cursize), "packet mismatch", kind, n, align);
            const auto written = ref;
            MSG_BeginReading(&ref); MSG_BeginReading(&ours);
            if (align) { MSG_ReadBits(&ref, align); MSG_ReadBits(&ours, align); }
            Reader in{ours}; codec::Command decoded{};
            MSG_ReadDeltaUsercmdKey(&ref, c.key, &base, &expected);
            Check(codec::ReadCommand(in, c.key, &c.from, &decoded), "decode failure", kind, n, align);
            Check(ref.bit == ours.bit && !std::memcmp(&expected, &decoded, sizeof(expected)), "decode mismatch", kind, n, align);
            Record(digest, n, align, written, expected);
        }
    }
    std::printf("kind %u: %zu cases x 8 alignments, fnv1a64 %016llx\n", kind, count, static_cast<unsigned long long>(digest.value));
}
} // namespace
int main() {
    static_assert(sizeof(entityState_t) == sizeof(codec::Entity));
    static_assert(sizeof(playerState_t) == sizeof(codec::Player));
    static_assert(sizeof(usercmd_t) == sizeof(codec::Command));
    g_nOverrideChecked = true;
    Run(0, jka_cases::EntityCount); Run(1, jka_cases::NormalCount);
    Run(2, jka_cases::PilotCount); Run(3, jka_cases::VehicleCount);
    Run(4, jka_cases::CommandCount);
}
