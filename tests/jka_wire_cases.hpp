/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_WIRE_CASES_HPP
#define FNTECH3_JKA_WIRE_CASES_HPP
#include "../code/compat/jka_message_codec.hpp"

/* Synthetic inputs shared by the optional pinned-source observation and the
 * engine regression test. No retail packets, modules or assets are fixtures. */
namespace jka_cases {
namespace codec = fntech3::jka::message;
inline constexpr std::size_t Variants = 6;
inline constexpr std::size_t EntityCount = codec::entityFields.size() * Variants + 4;
inline constexpr std::size_t NormalCount = codec::playerFields.size() * Variants + 68;
inline constexpr std::size_t PilotCount = codec::pilotFields.size() * Variants + 68;
inline constexpr std::size_t VehicleCount = codec::vehicleFields.size() * Variants + 68;
inline constexpr std::size_t CommandCount = codec::commandFields.size() * Variants + 7;

inline std::uint32_t Value(int bits, std::size_t variant) {
    if (bits == 0) {
        constexpr std::uint32_t words[]{0, 0xc5800000u, 0x457ff000u, 0x45800000u, 0xbe800000u, 0x80000000u};
        return words[variant % Variants]; // zero, -4096, 4095, 4096, -.25, -0
    }
    const int width = bits < 0 ? -bits : bits;
    const std::uint32_t mask = width == 32 ? ~0u : (1u << width) - 1;
    constexpr std::uint32_t words[]{0, 1, ~0u, 0x55555555u, 0xaaaaaaaau, 0x80000000u};
    auto value = words[variant % Variants] & mask;
    if (bits < 0 && (value & (1u << (width-1)))) value |= ~mask;
    return value;
}

struct EntityCase { codec::Entity from{}, to{}; bool force = false, remove = false; };
inline EntityCase MakeEntity(std::size_t index) {
    EntityCase out;
    out.from.number = out.to.number = 7;
    if (index < codec::entityFields.size() * Variants) {
        const auto field = codec::entityFields[index / Variants];
        codec::SetWord(out.from, field.offset, Value(field.bits, 3));
        codec::SetWord(out.to, field.offset, Value(field.bits, index % Variants));
    } else {
        const auto variant = index - codec::entityFields.size() * Variants;
        out.force = variant == 1;
        out.remove = variant == 2;
        if (variant == 3) {
            out.to.pos.trTime = 0x12345678;
            out.to.pos.trBase[0] = 23.5f;
            out.to.modelindex = -1;
            out.to.boneAngles4[2] = -4096;
            out.to.userVec2[2] = 1.0f;
        }
    }
    return out;
}

struct PlayerCase { codec::Player from{}, to{}; bool vehicle = false; };
inline PlayerCase MakePlayer(std::size_t index, unsigned kind) {
    PlayerCase out;
    out.vehicle = kind == 2;
    if (kind == 1) { out.from.eFlags = out.to.eFlags = 256; out.from.m_iVehicleNum = out.to.m_iVehicleNum = 13; }
    const codec::Field *fields = kind == 2 ? codec::vehicleFields.data() : kind == 1 ? codec::pilotFields.data() : codec::playerFields.data();
    const std::size_t count = kind == 2 ? codec::vehicleFields.size() : kind == 1 ? codec::pilotFields.size() : codec::playerFields.size();
    if (index < count * Variants) {
        const auto field = fields[index / Variants];
        codec::SetWord(out.from, field.offset, Value(field.bits, 3));
        codec::SetWord(out.to, field.offset, Value(field.bits, index % Variants));
    } else {
        const auto tail = index - count * Variants;
        if (tail < 64) {
            const auto group = tail / 16, n = tail % 16;
            auto *old = group == 0 ? out.from.stats : group == 1 ? out.from.persistant : group == 2 ? out.from.ammo : out.from.powerups;
            auto *next = group == 0 ? out.to.stats : group == 1 ? out.to.persistant : group == 2 ? out.to.ammo : out.to.powerups;
            old[n] = -1;
            next[n] = group == 0 && n == 4 ? (1 << 18) | 5 : group == 3 ? 0x12345678 : -1234;
        } else if (tail == 65) {
            out.to.ammo[16] = 12; out.to.ammo[17] = 34; out.to.ammo[18] = 56;
        } else if (tail == 66) {
            for (int n = 0; n < 16; ++n) {
                out.to.stats[n] = n == 4 ? (1 << 18) | 5 : -n;
                out.to.persistant[n] = 100+n;
                out.to.ammo[n] = 300-n;
                out.to.powerups[n] = 0x12345678+n;
            }
        } else if (tail == 67) {
            out.to.commandTime = 0x12345678;
            out.to.origin[0] = -4096; out.to.origin[1] = 12.5f;
            out.to.speed = -20;
            out.to.vehOrientation[2] = .75f;
            out.to.userInt3 = 1;
        }
    }
    return out;
}

struct CommandCase { codec::Command from{}, to{}; std::uint32_t key = 0x10203040u; };
inline CommandCase MakeCommand(std::size_t index) {
    CommandCase out;
    out.from.serverTime = 1000; out.to.serverTime = 1123;
    if (index < codec::commandFields.size() * Variants) {
        const auto field = codec::commandFields[index / Variants];
        codec::SetCommandValue(out.from, field, Value(field.bits, 3));
        codec::SetCommandValue(out.to, field, Value(field.bits, index % Variants));
    } else {
        switch (index - codec::commandFields.size() * Variants) {
        case 0: break;
        case 1: out.to.serverTime = 1255; break;
        case 2: out.to.serverTime = 1256; break;
        case 3: out.to.serverTime = 900; break;
        case 4: out.from.serverTime = 0x7fffffff; out.to.serverTime = static_cast<std::int32_t>(0x80000002u); break;
        case 5: out.from.forwardmove = out.to.forwardmove = -128; out.to.weapon = 5; break;
        case 6:
            out.to.buttons = 0xffff; out.to.generic_cmd = 255;
            out.to.invensel = 19; out.to.forcesel = 17;
            out.to.forwardmove = -128; out.to.rightmove = -127; out.to.upmove = 127;
            break;
        }
    }
    return out;
}

struct Digest {
    std::uint64_t value = 14695981039346656037ULL;
    void Bytes(const void *data, std::size_t count) {
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        for (std::size_t n = 0; n < count; ++n) { value ^= bytes[n]; value *= 1099511628211ULL; }
    }
    void Word(std::uint32_t word) { for (unsigned n = 0; n < 4; ++n) { const auto byte = static_cast<std::uint8_t>(word >> (8*n)); Bytes(&byte, 1); } }
};
} // namespace jka_cases
#endif
