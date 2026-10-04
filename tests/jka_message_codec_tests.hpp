/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_MESSAGE_CODEC_TESTS_HPP
#define FNTECH3_JKA_MESSAGE_CODEC_TESTS_HPP
#include "jka_wire_cases.hpp"
#include "../code/compat/jka_state_adapter.hpp"

namespace jka_wire_tests {
namespace codec = fntech3::jka::message;

template<class State> void Record(jka_cases::Digest &digest, std::size_t index,
    unsigned alignment, const msg_t &msg, const State &state) {
    digest.Word(static_cast<std::uint32_t>(index)); digest.Word(alignment);
    digest.Word(msg.bit); digest.Word(msg.cursize);
    digest.Bytes(msg.data, msg.cursize); digest.Bytes(&state, sizeof(state));
}

template<class State, class Read>
int CheckTruncation(const msg_t &written, const State &baseline,
    const State &expected, unsigned alignment, Read read) {
    for (int length = 0; length <= written.cursize; ++length) for (const bool alias : {false, true}) {
        struct Guarded { std::uint32_t before; State state; std::uint32_t after; } guarded{};
        std::memset(&guarded, 0xa5, sizeof(guarded));
        if (alias) guarded.state = baseline;
        const auto unchanged = guarded;
        msg_t reader{};
        MSG_Init(&reader, written.data, written.maxsize);
        reader.cursize = length; MSG_BeginReading(&reader);
        if (alignment) { reader.bit = alignment; reader.readcount = 1; }
        const bool complete = length == written.cursize;
        CHECK(!!read(&reader, alias ? &guarded.state : &baseline, &guarded.state) == complete);
        CHECK(!std::memcmp(&guarded.state, complete ? &expected : &unchanged.state, sizeof(expected)));
        CHECK(guarded.before == unchanged.before && guarded.after == unchanged.after);
    }
    return 0;
}

int Run(unsigned kind, std::size_t count, std::uint64_t expectedDigest) {
    jka_cases::Digest digest;
    for (std::size_t n = 0; n < count; ++n) for (unsigned align = 0; align < 8; ++align) {
        std::array<byte, 4096> bytes{};
        msg_t msg{};
        MSG_Init(&msg, bytes.data(), static_cast<int>(bytes.size()));
        if (align) MSG_WriteBits(&msg, 0x55, align);
        if (kind == 0) {
            const auto c = jka_cases::MakeEntity(n);
            CHECK(MSG_WriteDeltaJKAEntity(&msg, &c.from, c.remove ? nullptr : &c.to, c.force ? qtrue : qfalse));
            const auto written = msg;
            auto decoded = c.from;
            MSG_BeginReading(&msg); if (align) MSG_ReadBitsExternal(&msg, align);
            if (written.bit != static_cast<int>(align)) {
                const int number = MSG_ReadEntitynum(&msg);
                CHECK(MSG_ReadDeltaJKAEntity(&msg, &c.from, &decoded, number));
                if (n == count - 1) {
                    const auto read = [](msg_t *io, const auto *base, auto *result) {
                        const int id = MSG_ReadEntitynum(io);
                        return MSG_ReadDeltaJKAEntity(io, base, result, id);
                    };
                    if (const auto result = CheckTruncation(written, c.from, decoded, align, read)) return result;
                }
            }
            CHECK(msg.bit == written.bit);
            Record(digest, n, align, written, decoded);
        } else if (kind <= 3) {
            const auto c = jka_cases::MakePlayer(n, kind - 1);
            const auto vehicle = c.vehicle ? qtrue : qfalse;
            CHECK(MSG_WriteDeltaJKAPlayerstate(&msg, &c.from, &c.to, vehicle));
            const auto written = msg;
            codec::Player decoded{};
            MSG_BeginReading(&msg); if (align) MSG_ReadBitsExternal(&msg, align);
            CHECK(MSG_ReadDeltaJKAPlayerstate(&msg, &c.from, &decoded, vehicle));
            CHECK(msg.bit == written.bit);
            Record(digest, n, align, written, decoded);
            if (n == count - 1) {
                const auto read = [vehicle](msg_t *io, const auto *base, auto *result) { return MSG_ReadDeltaJKAPlayerstate(io, base, result, vehicle); };
                if (const auto result = CheckTruncation(written, c.from, decoded, align, read)) return result;
                // The canonical adapters must preserve every profile field,
                // including the second vehicle body, without exposing its POD.
                playerState_t base{}, sent{}, canonical{};
                CHECK(fntech3::jka::state::ToInternal(c.from, base));
                CHECK(fntech3::jka::state::ToInternal(c.to, sent));
                std::array<byte, 4096> canonicalBytes{};
                MSG_Init(&msg, canonicalBytes.data(), static_cast<int>(canonicalBytes.size()));
                if (align) MSG_WriteBits(&msg, 0x55, align);
                CHECK(vehicle ? MSG_WriteDeltaJKAVehiclePlayerstate(&msg, &base, &sent) :
                    MSG_WriteDeltaPlayerstateForWireProfile(&msg, &base, &sent, NETCHAN_WIRE_JKA_MP));
                CHECK(msg.bit == written.bit && msg.cursize == written.cursize && !std::memcmp(bytes.data(), canonicalBytes.data(), written.cursize));
                MSG_BeginReading(&msg); if (align) MSG_ReadBitsExternal(&msg, align);
                CHECK(vehicle ? MSG_ReadDeltaJKAVehiclePlayerstate(&msg, &base, &canonical) :
                    MSG_ReadDeltaPlayerstateForWireProfile(&msg, &base, &canonical, NETCHAN_WIRE_JKA_MP));
                codec::Player external{};
                CHECK(fntech3::jka::state::FromInternal(canonical, external));
                CHECK(!std::memcmp(&external, &decoded, sizeof(external)));
            }
        } else {
            const auto c = jka_cases::MakeCommand(n);
            CHECK(MSG_WriteDeltaJKAUsercmdKey(&msg, c.key, &c.from, &c.to));
            const auto written = msg;
            codec::Command decoded{};
            MSG_BeginReading(&msg); if (align) MSG_ReadBitsExternal(&msg, align);
            CHECK(MSG_ReadDeltaJKAUsercmdKey(&msg, c.key, &c.from, &decoded));
            CHECK(msg.bit == written.bit);
            Record(digest, n, align, written, decoded);
            if (n == count - 1) {
                const auto read = [&c](msg_t *io, const auto *base, auto *result) { return MSG_ReadDeltaJKAUsercmdKey(io, c.key, base, result); };
                if (const auto result = CheckTruncation(written, c.from, decoded, align, read)) return result;
            }
        }
    }
    if (digest.value != expectedDigest) {
        std::fprintf(stderr, "JKA source vector kind %u: digest %016llx expected %016llx\n", kind,
            static_cast<unsigned long long>(digest.value), static_cast<unsigned long long>(expectedDigest));
        return __LINE__;
    }
    return 0;
}

int CheckStringsAndHash() {
    const char text[]{'\x80', '\xff', '%', '\0'};
    CHECK(MSG_HashKeyForWireProfile(NETCHAN_WIRE_JKA_MP, text, 32) == -10865);
    CHECK(MSG_HashKeyForWireProfile(NETCHAN_WIRE_JKA_MP, "say 100% ready", 32) == 140014);
    for (const bool big : {false, true}) {
        std::array<byte, 64> bytes{};
        msg_t msg{}; MSG_Init(&msg, bytes.data(), static_cast<int>(bytes.size()));
        if (big) MSG_WriteBigStringForWireProfile(&msg, text, NETCHAN_WIRE_JKA_MP);
        else MSG_WriteStringForWireProfile(&msg, text, NETCHAN_WIRE_JKA_MP);
        MSG_BeginReading(&msg);
        const char *decoded = big ? MSG_ReadBigStringForWireProfile(&msg, NETCHAN_WIRE_JKA_MP) :
            MSG_ReadCommandStringForWireProfile(&msg, NETCHAN_WIRE_JKA_MP);
        CHECK(static_cast<unsigned char>(decoded[0]) == 0x80 && static_cast<unsigned char>(decoded[1]) == 0xff);
        CHECK(decoded[2] == '.' && decoded[3] == '\0');
    }
    return 0;
}

int CheckFloatPayloads() {
    for (const auto word : {0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u,
        0x45800000u, 0xc5800000u, 0x46000000u, 0xc6000000u,
        0x7f7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u, 0x7f800001u}) {
        const auto expected = (word & 0x7fffffffu) ? word : 0u;
        for (const bool vehicle : {false, true}) for (const bool pilot : {false, true}) {
            codec::Player baseline{}, sent{}, decoded{};
            baseline.origin[0] = 2.0f; sent = baseline;
            if (pilot && !vehicle) { sent.m_iVehicleNum = 9; sent.eFlags = 256; }
            codec::SetWord(sent, offsetof(codec::Player, origin), word);
            playerState_t base{}, state{}, received{};
            CHECK(fntech3::jka::state::ToInternal(baseline, base));
            CHECK(fntech3::jka::state::ToInternal(sent, state));
            std::array<byte, 2048> storage{};
            msg_t msg{}; MSG_Init(&msg, storage.data(), static_cast<int>(storage.size()));
            CHECK(vehicle ? MSG_WriteDeltaJKAVehiclePlayerstate(&msg, &base, &state) :
                MSG_WriteDeltaPlayerstateForWireProfile(&msg, &base, &state, NETCHAN_WIRE_JKA_MP));
            MSG_BeginReading(&msg);
            CHECK(vehicle ? MSG_ReadDeltaJKAVehiclePlayerstate(&msg, &base, &received) :
                MSG_ReadDeltaPlayerstateForWireProfile(&msg, &base, &received, NETCHAN_WIRE_JKA_MP));
            CHECK(fntech3::jka::state::FromInternal(received, decoded));
            CHECK(codec::Word(decoded, offsetof(codec::Player, origin)) == expected);
        }
    }
    return 0;
}
} // namespace jka_wire_tests

int TestJKAMessageCodecs() {
    // Numeric fixtures measured by the optional source probe against OpenJK
    // 1a6a643427aa347553e9073dac5570b33337c4d9; all 25,192 compressed packets
    // and cross-decoded PODs matched. These are source facts, not retail runs.
    CHECK(MSG_SetJKAFieldOverrides(nullptr, 0, nullptr, 0));
    using namespace jka_wire_tests;
    if (const int result = Run(0, jka_cases::EntityCount, 0x64569a88bb4f1696ULL)) return result;
    if (const int result = Run(1, jka_cases::NormalCount, 0x58d4b8e2a293bf94ULL)) return result;
    if (const int result = Run(2, jka_cases::PilotCount, 0x6597d0b5b57ab5b9ULL)) return result;
    if (const int result = Run(3, jka_cases::VehicleCount, 0x3e166d59d5e286d1ULL)) return result;
    if (const int result = Run(4, jka_cases::CommandCount, 0xf0fd208f3b18b3e9ULL)) return result;
    if (const int result = CheckFloatPayloads()) return result;
    return CheckStringsAndHash();
}
#endif
