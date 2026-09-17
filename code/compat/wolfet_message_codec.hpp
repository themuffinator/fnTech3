/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_MESSAGE_CODEC_HPP
#define FNTECH3_WOLFET_MESSAGE_CODEC_HPP
#include "wolfet_game_abi.h"
#include "rtcw_message_codec.hpp"

/* Protocol 84 bodies. ET deliberately has its own fields and command schema.
 * Reuse only fnTech3's checked scalar/array primitives. Reads publish a complete
 * record transactionally; truncated or malformed bodies never alter a caller. */
namespace fntech3::wolfet::message {
using rtcw::message::Field;
using rtcw::message::Word;
using rtcw::message::SetWord;
using rtcw::message::WriteScalar;
using rtcw::message::ReadScalar;
using rtcw::message::ArrayChanges;
using rtcw::message::WriteArray;
using rtcw::message::ReadArray;
using Entity = fntech3_wolfet_entityState_t;
using Player = fntech3_wolfet_playerState_t;
using Command = fntech3_wolfet_usercmd_t;

#define FNTECH3_WOLFET_ENTITY_FIELD(member, width) Field{offsetof(Entity, member), width},
#define FNTECH3_WOLFET_PLAYER_FIELD(member, width)
inline constexpr std::array entityFields{
#include "wolfet_wire_fields.def"
};
#undef FNTECH3_WOLFET_ENTITY_FIELD
#undef FNTECH3_WOLFET_PLAYER_FIELD
#define FNTECH3_WOLFET_ENTITY_FIELD(member, width)
#define FNTECH3_WOLFET_PLAYER_FIELD(member, width) Field{offsetof(Player, member), width},
inline constexpr std::array playerFields{
#include "wolfet_wire_fields.def"
};
#undef FNTECH3_WOLFET_ENTITY_FIELD
#undef FNTECH3_WOLFET_PLAYER_FIELD
static_assert(entityFields.size() == 71 && playerFields.size() == 77);

template<class Writer> bool WriteEntity(Writer &io, const Entity *from,
    const Entity *to, bool force) {
    if (!to) {
        if (!from) return io.Good();
        if (from->number < 0 || from->number >= 1023) return io.Fail();
        io.Write(from->number, 10); io.Write(1, 1); return io.Good();
    }
    if (to->number < 0 || to->number >= 1023) return io.Fail();
    const Entity empty{}; if (!from) from = &empty;
    size_t last = entityFields.size();
    while (last && Word(*from, entityFields[last-1].offset) == Word(*to, entityFields[last-1].offset)) --last;
    if (!last && !force) return io.Good();
    io.Write(to->number, 10); io.Write(0, 1); io.Write(last != 0, 1);
    if (last) {
        io.Write(static_cast<uint32_t>(last), 8);
        for (size_t n = 0; n < last; ++n) {
            const auto field = entityFields[n];
            const auto value = Word(*to, field.offset);
            const bool changed = value != Word(*from, field.offset);
            io.Write(changed, 1);
            if (changed) WriteScalar(io, value, field.bits, true);
        }
    }
    return io.Good();
}

template<class Reader> bool ReadEntity(Reader &io, const Entity *from,
    Entity *to, int number) {
    if (!to || number < 0 || number >= 1023) return io.Fail();
    Entity next{}; if (from) next = *from;
    next.number = number;
    if (io.Read(1)) { next = {}; next.number = 1023; }
    else if (io.Read(1)) {
        const auto count = io.Read(8);
        if (count > entityFields.size()) return io.Fail();
        for (size_t n = 0; n < count && io.Good(); ++n) {
            const auto field = entityFields[n];
            if (io.Read(1)) SetWord(next, field.offset, ReadScalar(io, field.bits, true));
        }
    }
    if (!io.Good()) return false;
    *to = next; return true;
}

template<class Writer> bool WritePlayer(Writer &io, const Player *from, const Player *to) {
    if (!to) return io.Fail();
    const Player empty{}; if (!from) from = &empty;
    size_t count = playerFields.size();
    while (count && Word(*from, playerFields[count-1].offset) == Word(*to, playerFields[count-1].offset)) --count;
    io.Write(static_cast<uint32_t>(count), 8);
    for (size_t n = 0; n < count; ++n) {
        const auto field = playerFields[n];
        const auto value = Word(*to, field.offset);
        const bool changed = value != Word(*from, field.offset);
        io.Write(changed, 1);
        if (changed) WriteScalar(io, value, field.bits, false);
    }
    const uint32_t masks[]{ArrayChanges(from->stats, to->stats),
        ArrayChanges(from->persistant, to->persistant), ArrayChanges(from->holdable, to->holdable),
        ArrayChanges(from->powerups, to->powerups)};
    const bool any = (masks[0] | masks[1] | masks[2] | masks[3]) != 0;
    io.Write(any, 1);
    if (any) {
        WriteArray(io, to->stats, masks[0]); WriteArray(io, to->persistant, masks[1]);
        WriteArray(io, to->holdable, masks[2]); WriteArray(io, to->powerups, masks[3], 32);
    }
    uint32_t ammo[4]{};
    for (unsigned n = 0; n < 4; ++n) ammo[n] = ArrayChanges(from->ammo+n*16, to->ammo+n*16);
    const bool anyAmmo = (ammo[0] | ammo[1] | ammo[2] | ammo[3]) != 0;
    io.Write(anyAmmo, 1);
    if (anyAmmo) for (unsigned n = 0; n < 4; ++n) WriteArray(io, to->ammo+n*16, ammo[n]);
    for (unsigned n = 0; n < 4; ++n)
        WriteArray(io, to->ammoclip+n*16, ArrayChanges(from->ammoclip+n*16, to->ammoclip+n*16));
    return io.Good();
}

template<class Reader> bool ReadPlayer(Reader &io, const Player *from, Player *to) {
    if (!to) return io.Fail();
    Player next{}; if (from) next = *from;
    const auto count = io.Read(8);
    if (count > playerFields.size()) return io.Fail();
    for (size_t n = 0; n < count && io.Good(); ++n) {
        const auto field = playerFields[n];
        if (io.Read(1)) SetWord(next, field.offset, ReadScalar(io, field.bits, false));
    }
    if (io.Read(1)) {
        ReadArray(io, next.stats); ReadArray(io, next.persistant);
        ReadArray(io, next.holdable); ReadArray(io, next.powerups, 32);
    }
    if (io.Read(1)) for (unsigned n = 0; n < 4; ++n) ReadArray(io, next.ammo+n*16);
    for (unsigned n = 0; n < 4; ++n) ReadArray(io, next.ammoclip+n*16);
    if (!io.Good()) return false;
    *to = next; return true;
}

using rtcw::message::CommandField;
using rtcw::message::CommandValue;
using rtcw::message::SetCommandValue;
#define C(member, bytes, width) CommandField{offsetof(Command, member), bytes, width}
inline constexpr std::array commandFields{
    C(angles[0],4,16), C(angles[1],4,16), C(angles[2],4,16),
    C(forwardmove,1,8), C(rightmove,1,8), C(upmove,1,8),
    C(buttons,1,8), C(wbuttons,1,8), C(weapon,1,8), C(flags,1,8),
    C(doubleTap,1,3), C(identClient,1,8)
};
#undef C
template<class Writer> bool WriteCommand(Writer &io, uint32_t key, const Command *from, const Command *to) {
    if (!to || to->doubleTap > 7) return io.Fail();
    const Command empty{}; if (!from) from = &empty;
    const auto time = static_cast<uint32_t>(to->serverTime);
    const auto elapsed = time - static_cast<uint32_t>(from->serverTime);
    io.Write(elapsed < 256, 1); io.Write(elapsed < 256 ? elapsed : time, elapsed < 256 ? 8 : 32);
    bool any = false;
    for (auto field : commandFields) any |= CommandValue(*from,field) != CommandValue(*to,field);
    io.Write(any,1);
    if (any) {
        key ^= time;
        for (auto field : commandFields) {
            const auto value = CommandValue(*to,field);
            const bool changed = value != CommandValue(*from,field);
            io.Write(changed,1);
            if (changed) io.Write(value ^ key,field.bits);
        }
    }
    return io.Good();
}
template<class Reader> bool ReadCommand(Reader &io, uint32_t key, const Command *from, Command *to) {
    if (!to) return io.Fail();
    Command next{}; if (from) next = *from;
    const uint32_t time = io.Read(1) ? static_cast<uint32_t>(next.serverTime)+io.Read(8) : io.Read(32);
    std::memcpy(&next.serverTime,&time,4);
    if (io.Read(1)) {
        key ^= time;
        for (auto field : commandFields) {
            if (!io.Read(1)) continue;
            // Original ET indexes kbitmask[bits], retaining one extra key bit
            // in 32-bit angle storage. Byte fields truncate it; doubleTap is
            // explicitly masked to three bits. Preserve this observed quirk.
            auto value = io.Read(field.bits) ^ (key & ((1u << (field.bits+1))-1));
            if (field.bits == 3) value &= 7;
            SetCommandValue(next,field,value);
        }
    }
    if (!io.Good()) return false;
    *to = next; return true;
}
} // namespace fntech3::wolfet::message
#endif
