/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_SNAPSHOT_HPP
#define FNTECH3_SERVER_JKA_SNAPSHOT_HPP
#include <cstdint>

namespace fntech3::jka::snapshot {
/* Raven's own snapshot rules, after the no-client and single-client tests the
 * shared walk keeps. Retail jamp.exe 1.01 runs them in its
 * SV_AddEntitiesVisibleFromPoint walk (0x460f70..0x46105c):
 *
 * - An entity the game marks permanent (EF_PERMANENT, 0x80 in its state's
 *   eFlags) is never sent (0x460f90). The client game draws it every frame
 *   from its baseline (CG_GETDEFAULTSTATE), so one sent too is drawn twice.
 * - SVF_BROADCAST (0x20), the viewer's own entity and an entity whose
 *   two-word broadcastClients mask names the viewer are sent past every
 *   visibility test (0x461011, 0x461028, 0x46104f).
 * - So is an entity the game marks as seen from the level's sky portal (its
 *   state's isPortalEnt, 0x46105c): the client game draws it in the portal's
 *   own view, from a camera the viewer's own visibility does not cover.
 *
 * Raven's SVF_BROADCASTCLIENTS is 0x2, where Q3 keeps CLIENTMASK, and it
 * excludes nothing: retail never reads the bit, and the mask only adds.
 * OpenJK's exclusion of the clients a flagged entity's mask leaves out
 * (codemp/server/sv_snapshot.cpp:437) is its own. */
struct BroadcastDecision { bool admitted, forced; };
inline BroadcastDecision Broadcast(int entity, int viewer, std::uint32_t flags,
    const std::uint32_t (&clients)[2], bool portal = false, std::uint32_t entityFlags = 0) noexcept {
    if (entity < 0 || entity >= 1024 || viewer < 0 || viewer >= 32) return {false, false};
    if (entityFlags & 0x80u) return {false, false};
    const bool named = (clients[viewer / 32] & (std::uint32_t(1) << (viewer % 32))) != 0;
    return {true, entity == viewer || named || (flags & 0x20u) != 0 || portal};
}
}

namespace fntech3::jk2::snapshot {
/* Jedi Outcast's walk (jk2mp.exe 1.02, 0x44fcf0) has the same three adds
 * (0x44fdde, 0x44fdf1, 0x44fe1a) and nothing else of Academy's: no permanent
 * skip, and no portal-scene add, as its entity state has no isPortalEnt. Its
 * mask is the word pair at +104 in the engine's half, which the retail
 * module's G_UpdateClientBroadcasts fills for Force Sight and the Jedi Master.
 * Bit 0x2 is no client mask here either. */
inline jka::snapshot::BroadcastDecision Broadcast(int entity, int viewer, std::uint32_t flags,
    const std::uint32_t (&clients)[2]) noexcept {
    return jka::snapshot::Broadcast(entity, viewer, flags, clients);
}
}
#endif
