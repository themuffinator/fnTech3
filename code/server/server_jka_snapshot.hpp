/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_SNAPSHOT_HPP
#define FNTECH3_SERVER_JKA_SNAPSHOT_HPP
#include <cstdint>

namespace fntech3::jka::snapshot {
/* Raven's SVF_BROADCASTCLIENTS is 0x2, where Q3 keeps CLIENTMASK. Its
 * two-word mask is independent of singleClient, and its own entity always
 * reaches its viewer. Pinned OpenJK codemp/server/sv_snapshot.cpp:437-450.
 * An entity the game marks as seen from the level's sky portal (its state's
 * isPortalEnt) is sent too, after those rules and before any test of what
 * the viewer can see, as retail jamp.exe 1.01 sends it (0x46105c): the
 * client game draws it in the portal's own view, from a camera the viewer's
 * own visibility does not cover. */
struct BroadcastDecision { bool admitted, forced; };
inline BroadcastDecision Broadcast(int entity, int viewer, std::uint32_t flags,
    const std::uint32_t (&clients)[2], bool portal = false) noexcept {
    if (entity < 0 || entity >= 1024 || viewer < 0 || viewer >= 32) return {false, false};
    const bool self = entity == viewer;
    const bool named = (clients[viewer / 32] & (std::uint32_t(1) << (viewer % 32))) != 0;
    if (!self && (flags & 0x2u) && !named) return {false, false};
    return {true, self || named || (flags & 0x20u) != 0 || portal};
}
}
#endif
