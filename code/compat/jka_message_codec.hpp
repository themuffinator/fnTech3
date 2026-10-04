/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_MESSAGE_CODEC_HPP
#define FNTECH3_JKA_MESSAGE_CODEC_HPP

#include "jka_game_abi.h"
#include "rtcw_message_codec.hpp"

#include <string_view>

/* Independently implemented Jedi Academy multiplayer protocol-26 bodies.
 * The factual tables are pinned to Raven's released source in
 * jka_wire_fields.def. Retail assets1.pk3's two override files contain only
 * comments, so those defaults are also the installed retail data contract.
 * This is source/data evidence, not a claim of live retail interoperability.
 *
 * IO supplies checked Read/Write, Good and Fail. Reads publish transactionally.
 * Vehicle player states are a separate snapshot body; normal/pilot selection
 * is encoded in the first bit of a non-vehicle body. _ONEBIT_COMBO is disabled
 * in the reference contract and is deliberately not a runtime option here. */
namespace fntech3::jka::message {
using Entity = fntech3_jka_entityState_t;
using Player = fntech3_jka_playerState_t;
using Command = fntech3_jka_usercmd_t;
using rtcw::message::Word;
using rtcw::message::SetWord;
using rtcw::message::WriteScalar;
using rtcw::message::ReadScalar;
using rtcw::message::ArrayChanges;
using rtcw::message::WriteArray;
using rtcw::message::ReadArray;

struct Field {
    std::size_t offset;
    int bits;
    std::string_view name;
};

#define FNTECH3_JKA_ENTITY_FIELD(member, width, name) Field{offsetof(Entity, member), width, name},
#define FNTECH3_JKA_PLAYER_FIELD(member, width, name)
#define FNTECH3_JKA_PILOT_FIELD(member, width, name)
#define FNTECH3_JKA_VEHICLE_FIELD(member, width, name)
inline constexpr std::array entityFields{
#include "jka_wire_fields.def"
};
#undef FNTECH3_JKA_ENTITY_FIELD
#undef FNTECH3_JKA_PLAYER_FIELD
#define FNTECH3_JKA_ENTITY_FIELD(member, width, name)
#define FNTECH3_JKA_PLAYER_FIELD(member, width, name) Field{offsetof(Player, member), width, name},
inline constexpr std::array playerFields{
#include "jka_wire_fields.def"
};
#undef FNTECH3_JKA_PLAYER_FIELD
#undef FNTECH3_JKA_PILOT_FIELD
#define FNTECH3_JKA_PLAYER_FIELD(member, width, name)
#define FNTECH3_JKA_PILOT_FIELD(member, width, name) Field{offsetof(Player, member), width, name},
inline constexpr std::array pilotFields{
#include "jka_wire_fields.def"
};
#undef FNTECH3_JKA_PILOT_FIELD
#undef FNTECH3_JKA_VEHICLE_FIELD
#define FNTECH3_JKA_PILOT_FIELD(member, width, name)
#define FNTECH3_JKA_VEHICLE_FIELD(member, width, name) Field{offsetof(Player, member), width, name},
inline constexpr std::array vehicleFields{
#include "jka_wire_fields.def"
};
#undef FNTECH3_JKA_ENTITY_FIELD
#undef FNTECH3_JKA_PLAYER_FIELD
#undef FNTECH3_JKA_PILOT_FIELD
#undef FNTECH3_JKA_VEHICLE_FIELD
static_assert(entityFields.size() == 132 && playerFields.size() == 137 &&
    pilotFields.size() == 140 && vehicleFields.size() == 69);

template<class Record, std::size_t N>
constexpr bool ValidFields(const std::array<Field, N> &fields) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
        if (fields[i].offset > sizeof(Record) - 4 || fields[i].offset % 4 ||
            fields[i].bits < -31 || fields[i].bits > 32) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (fields[i].offset == fields[j].offset) return false;
    }
    return true;
}
static_assert(ValidFields<Entity>(entityFields) && ValidFields<Player>(playerFields) &&
    ValidFields<Player>(pilotFields) && ValidFields<Player>(vehicleFields));

/* Only entity and normal-player widths are overrideable in the reference;
 * the pilot/vehicle widths remain fixed. Keep one coherent table set per
 * process, loaded by the selected profile after filesystem startup. */
struct Tables {
    std::array<Field, entityFields.size()> entity = entityFields;
    std::array<Field, playerFields.size()> player = playerFields;
};

inline std::string_view Trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

template<std::size_t N>
bool ApplyOverrides(std::string_view text, std::array<Field, N> &fields) noexcept {
    if (text.size() >= 4096 || text.find('\0') != std::string_view::npos) return false;
    auto next = fields;
    while (!text.empty()) {
        const auto end = text.find_first_of("\r\n");
        auto line = Trim(text.substr(0, end));
        if (end == std::string_view::npos) text = {};
        else text.remove_prefix(end + 1);
        if (line.empty() || line.front() == ';') continue;
        const auto comma = line.find(',');
        if (comma == std::string_view::npos) return false;
        const auto name = Trim(line.substr(0, comma));
        auto encoded = Trim(line.substr(comma + 1));
        if (name.empty() || encoded.empty()) return false;
        int width = 0;
        if (encoded == "GENTITYNUM_BITS") width = 10;
        else {
            int sign = 1;
            if (encoded.front() == '-' || encoded.front() == '+') {
                sign = encoded.front() == '-' ? -1 : 1;
                encoded.remove_prefix(1);
            }
            if (encoded.empty()) return false;
            for (const char digit : encoded) {
                if (digit < '0' || digit > '9' || width > 32) return false;
                width = width * 10 + digit - '0';
            }
            width *= sign;
        }
        if (width < -31 || width > 32) return false;
        // Unknown keys are ignored, as in retail. Stock commented examples
        // include obsolete keys that no longer appear in the shipped table.
        for (auto &field : next) if (field.name == name) { field.bits = width; break; }
    }
    fields = next;
    return true;
}

inline bool Configure(Tables &tables, std::string_view entity, std::string_view player) noexcept {
    Tables next;
    if (!ApplyOverrides(entity, next.entity) || !ApplyOverrides(player, next.player)) return false;
    tables = next;
    return true;
}

template<class Writer> bool WriteEntity(Writer &io, const Entity *from,
    const Entity *to, bool force, const Tables &tables) {
    if (!from && !to) return io.Good();
    const int number = to ? to->number : from->number;
    // 1023 is the packet-entity terminator, never an encodable entity.
    if (number < 0 || number >= 1023) return io.Fail();
    if (!to) { io.Write(number, 10); io.Write(1, 1); return io.Good(); }
    const Entity empty{};
    if (!from) from = &empty;
    const auto &fields = tables.entity;
    std::size_t last = fields.size();
    while (last && Word(*from, fields[last-1].offset) == Word(*to, fields[last-1].offset)) --last;
    if (!last && !force) return io.Good();
    io.Write(number, 10); io.Write(0, 1); io.Write(last != 0, 1);
    if (last) {
        io.Write(static_cast<std::uint32_t>(last), 8);
        for (std::size_t n = 0; n < last; ++n) {
            const auto field = fields[n];
            const auto value = Word(*to, field.offset);
            const bool changed = value != Word(*from, field.offset);
            io.Write(changed, 1);
            if (changed) WriteScalar(io, value, field.bits, true);
        }
    }
    return io.Good();
}

template<class Reader> bool ReadEntity(Reader &io, const Entity *from,
    Entity *to, int number, const Tables &tables) {
    if (!to || number < 0 || number >= 1023) return io.Fail();
    Entity next{};
    if (from) next = *from;
    next.number = number;
    if (io.Read(1)) { next = {}; next.number = 1023; }
    else if (io.Read(1)) {
        const auto count = io.Read(8);
        if (count > tables.entity.size()) return io.Fail();
        for (std::size_t n = 0; n < count && io.Good(); ++n) {
            const auto field = tables.entity[n];
            if (io.Read(1)) SetWord(next, field.offset, ReadScalar(io, field.bits, true));
        }
    }
    if (!io.Good()) return false;
    *to = next;
    return true;
}

inline constexpr bool IsPilot(const Player &state) noexcept {
    return state.m_iVehicleNum != 0 && (state.eFlags & (1 << 8)) != 0;
}

template<class Writer> void WriteStats(Writer &io, const Player &to, std::uint32_t mask) {
    io.Write(mask != 0, 1);
    if (!mask) return;
    io.Write(mask, 16);
    for (unsigned n = 0; n < 16; ++n)
        if (mask & (1u << n)) io.Write(static_cast<std::uint32_t>(to.stats[n]), n == 4 ? 19 : 16);
}
template<class Reader> void ReadStats(Reader &io, Player &to) {
    if (!io.Read(1)) return;
    const auto mask = io.Read(16);
    for (unsigned n = 0; n < 16 && io.Good(); ++n) if (mask & (1u << n))
        SetWord(to, offsetof(Player, stats) + n * 4, io.Read(n == 4 ? 19 : -16));
}

template<class Writer> bool WritePlayer(Writer &io, const Player *from, const Player *to,
    bool vehicle, const Tables &tables) {
    if (!to) return io.Fail();
    const Player empty{};
    if (!from) from = &empty;
    const bool pilot = !vehicle && IsPilot(*to);
    const Field *fields = vehicle ? vehicleFields.data() : pilot ? pilotFields.data() : tables.player.data();
    std::size_t count = vehicle ? vehicleFields.size() : pilot ? pilotFields.size() : tables.player.size();
    if (!vehicle) io.Write(pilot, 1);
    while (count && Word(*from, fields[count-1].offset) == Word(*to, fields[count-1].offset)) --count;
    io.Write(static_cast<std::uint32_t>(count), 8);
    for (std::size_t n = 0; n < count; ++n) {
        const auto value = Word(*to, fields[n].offset);
        const bool changed = value != Word(*from, fields[n].offset);
        io.Write(changed, 1);
        if (changed) WriteScalar(io, value, fields[n].bits, false);
    }
    const std::uint32_t masks[]{ArrayChanges(from->stats, to->stats),
        ArrayChanges(from->persistant, to->persistant), ArrayChanges(from->ammo, to->ammo),
        ArrayChanges(from->powerups, to->powerups)};
    const bool any = (masks[0] | masks[1] | masks[2] | masks[3]) != 0;
    io.Write(any, 1);
    if (any) {
        WriteStats(io, *to, masks[0]);
        WriteArray(io, to->persistant, masks[1]);
        WriteArray(io, to->ammo, masks[2]);
        WriteArray(io, to->powerups, masks[3], 32);
    }
    return io.Good();
}

template<class Reader> bool ReadPlayer(Reader &io, const Player *from, Player *to,
    bool vehicle, const Tables &tables) {
    if (!to) return io.Fail();
    Player next{};
    if (from) next = *from;
    const bool pilot = !vehicle && io.Read(1) != 0;
    const Field *fields = vehicle ? vehicleFields.data() : pilot ? pilotFields.data() : tables.player.data();
    const std::size_t capacity = vehicle ? vehicleFields.size() : pilot ? pilotFields.size() : tables.player.size();
    const auto count = io.Read(8);
    if (count > capacity) return io.Fail();
    for (std::size_t n = 0; n < count && io.Good(); ++n)
        if (io.Read(1)) SetWord(next, fields[n].offset, ReadScalar(io, fields[n].bits, false));
    if (io.Read(1)) {
        ReadStats(io, next); ReadArray(io, next.persistant);
        ReadArray(io, next.ammo); ReadArray(io, next.powerups, 32);
    }
    if (!io.Good()) return false;
    *to = next;
    return true;
}

using rtcw::message::CommandField;
using rtcw::message::CommandValue;
using rtcw::message::SetCommandValue;
#define C(member, bytes, width) CommandField{offsetof(Command, member), bytes, width}
inline constexpr std::array commandFields{
    C(angles[0], 4, 16), C(angles[1], 4, 16), C(angles[2], 4, 16),
    C(forwardmove, 1, 8), C(rightmove, 1, 8), C(upmove, 1, 8),
    C(buttons, 4, 16), C(weapon, 1, 8), C(forcesel, 1, 8),
    C(invensel, 1, 8), C(generic_cmd, 1, 8)
};
#undef C

template<class Writer> bool WriteCommand(Writer &io, std::uint32_t key,
    const Command *from, const Command *to) {
    if (!to) return io.Fail();
    const Command empty{};
    if (!from) from = &empty;
    const auto time = static_cast<std::uint32_t>(to->serverTime);
    const auto elapsed = time - static_cast<std::uint32_t>(from->serverTime);
    std::int32_t signedElapsed;
    std::memcpy(&signedElapsed, &elapsed, 4);
    const bool shortTime = signedElapsed < 256;
    io.Write(shortTime, 1); io.Write(shortTime ? elapsed : time, shortTime ? 8 : 32);
    bool any = false;
    for (auto field : commandFields) any |= CommandValue(*from, field) != CommandValue(*to, field);
    io.Write(any, 1);
    if (any) for (auto field : commandFields) {
        const auto value = CommandValue(*to, field);
        const bool changed = value != CommandValue(*from, field);
        io.Write(changed, 1);
        if (changed) io.Write(value ^ key ^ time, field.bits);
    }
    return io.Good();
}

template<class Reader> bool ReadCommand(Reader &io, std::uint32_t key,
    const Command *from, Command *to) {
    if (!to) return io.Fail();
    Command next{};
    if (from) next = *from;
    const auto time = io.Read(1) ? static_cast<std::uint32_t>(next.serverTime) + io.Read(8) : io.Read(32);
    std::memcpy(&next.serverTime, &time, 4);
    if (io.Read(1)) {
        key ^= time;
        for (auto field : commandFields) if (io.Read(1)) {
            const auto mask = (std::uint32_t{1} << field.bits) - 1;
            SetCommandValue(next, field, io.Read(field.bits) ^ (key & mask));
        }
        // Raven clamps the forbidden axis value after a changed command body,
        // including an inherited axis in a body where only another field changed.
        if (next.forwardmove == -128) next.forwardmove = -127;
        if (next.rightmove == -128) next.rightmove = -127;
        if (next.upmove == -128) next.upmove = -127;
    }
    if (!io.Good()) return false;
    *to = next;
    return true;
}
} // namespace fntech3::jka::message
#endif
