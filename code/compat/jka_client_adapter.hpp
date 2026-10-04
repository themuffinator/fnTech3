/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_CLIENT_ADAPTER_HPP
#define FNTECH3_JKA_CLIENT_ADAPTER_HPP
#include "jka_client_abi.h"
#include "jka_state_adapter.hpp"
#include <algorithm>
#include <cstring>

namespace fntech3::jka::client {

inline bool FiniteVector(const float *value) noexcept {
    if (!value) return false;
    for (int axis = 0; axis < 3; ++axis) {
        uint32_t bits; std::memcpy(&bits, &value[axis], sizeof(bits));
        volatile uint32_t encoded = bits;
        if ((encoded & 0x7f800000u) == 0x7f800000u) return false;
    }
    return true;
}
inline bool Bounds(const float *mins, const float *maxs) noexcept {
    if (!FiniteVector(mins) || !FiniteVector(maxs)) return false;
    for (int axis = 0; axis < 3; ++axis) if (mins[axis] > maxs[axis]) return false;
    return true;
}

inline bool GameState(const gameState_t &source, fntech3_jka_mp_gameState_t &destination) noexcept {
    if (source.dataCount < 0 || source.dataCount > FNTECH3_JKA_MAX_GAMESTATE_CHARS) return false;
    if (source.stringData[0] != 0) return false;
    for (int index = 0; index < MAX_GAME_CONFIGSTRINGS; ++index) {
        const int offset = source.stringOffsets[index];
        if (index >= FNTECH3_JKA_MAX_CONFIGSTRINGS) {
            if (offset) return false;
        } else if (offset < 0 || (offset && (offset >= source.dataCount
            || !std::memchr(source.stringData + offset, 0, std::size_t(source.dataCount - offset))))) {
            return false;
        }
    }
    fntech3_jka_mp_gameState_t next{};
    std::copy_n(source.stringOffsets, FNTECH3_JKA_MAX_CONFIGSTRINGS, next.stringOffsets);
    std::memcpy(next.stringData, source.stringData, std::size_t(source.dataCount));
    next.dataCount = source.dataCount;
    destination = next;
    return true;
}

/* A validated host snapshot and its canonical entity ring. The packet parser
 * owns vehicle delta history separately; it must never substitute the rider's
 * player state for the vehicle's state. Ring counts are validated before any
 * module-owned output is written. */
struct SnapshotView {
    int flags = 0, ping = 0, time = 0, commandSequence = 0;
    const uint8_t *areamask = nullptr;
    const playerState_t *player = nullptr, *vehicle = nullptr;
    const entityState_t *entities = nullptr;
    uint32_t firstEntity = 0;
    std::size_t entityCapacity = 0;
    int entityCount = 0;
};

inline bool Snapshot(const SnapshotView &source, fntech3_jka_mp_snapshot_t &destination) noexcept {
    if (!source.areamask || !source.player || !source.vehicle || source.entityCount < 0
        || !source.entityCapacity || (source.entityCapacity & (source.entityCapacity - 1))
        || std::size_t(source.entityCount) > source.entityCapacity
        || (source.entityCount && !source.entities)) return false;
    fntech3_jka_mp_snapshot_t next{};
    next.snapFlags = source.flags; next.ping = source.ping; next.serverTime = source.time;
    next.serverCommandSequence = source.commandSequence;
    std::copy_n(source.areamask, FNTECH3_JKA_CLIENT_AREAMASK_BYTES, next.areamask);
    if (!state::FromInternal(*source.player, next.ps) || !state::FromInternal(*source.vehicle, next.vps)) return false;
    next.numEntities = (std::min)(source.entityCount, FNTECH3_JKA_MAX_ENTITIES_IN_SNAPSHOT);
    for (int index = 0; index < next.numEntities; ++index) {
        const auto at = (source.firstEntity + uint32_t(index)) & (source.entityCapacity - 1);
        if (!state::FromInternal(source.entities[at], next.entities[index])) return false;
    }
    /* Retail 0x4117d0 never fills the legacy numServerCommands field. Command
     * delivery is bounded by serverCommandSequence, which it does publish. */
    destination = next;
    return true;
}

inline bool DefaultState(const entityState_t &source, fntech3_jka_entityState_t &destination) noexcept {
    // Retail 0x411901 tests the sign bit of the byte at entityState.eFlags+0.
    return (source.eFlags & 0x80) && state::FromInternal(source, destination);
}
} // namespace fntech3::jka::client
#endif
