/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/server/server_jka_snapshot.hpp"
#include <cassert>
#include <cstdint>
#include <initializer_list>
int main() {
    using fntech3::jka::snapshot::Broadcast;
    const std::uint32_t empty[2]{};
    const std::uint32_t selected[2]{0x80000001u, 0xffffffffu};
    for (int viewer = 0; viewer < 32; ++viewer) {
        const bool marked = viewer == 0 || viewer == 31;
        // SVF_BROADCASTCLIENTS (0x2) excludes nobody: retail jamp.exe 1.01
        // never reads the bit, and the mask only adds (0x46104f).
        auto d = Broadcast(100, viewer, 2, selected);
        assert(d.admitted && d.forced == marked);
        d = Broadcast(100, viewer, 2, empty);
        assert(d.admitted && !d.forced);
        d = Broadcast(100, viewer, 0, selected);
        assert(d.admitted && d.forced == marked);
        // The viewer's own entity is always sent (0x461028).
        d = Broadcast(viewer, viewer, 2, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(viewer, viewer, 0, empty);
        assert(d.admitted && d.forced);
        // SVF_BROADCAST (0x20) forces it (0x461011), with or without 0x2.
        d = Broadcast(100, viewer, 32, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, 34, empty);
        assert(d.admitted && d.forced);
        // No other bit decides anything here.
        d = Broadcast(100, viewer, ~std::uint32_t(0x20), empty);
        assert(d.admitted && !d.forced);
        // Only the viewer's own bit of the mask counts.
        const std::uint32_t others[2]{~(std::uint32_t(1) << viewer), 0xffffffffu};
        d = Broadcast(100, viewer, 2, others);
        assert(d.admitted && !d.forced);
    }
    for (int viewer : {-1, 32, 63, 64, 2147483647}) {
        const auto d = Broadcast(0, viewer, 2, selected);
        assert(!d.admitted && !d.forced);
    }
    for (int entity : {-1, 1024, 2147483647}) {
        const auto d = Broadcast(entity, 0, 0, selected);
        assert(!d.admitted && !d.forced);
    }
    // An entity seen from the sky portal (isPortalEnt) is always sent, as
    // retail jamp.exe 1.01 sends it (0x46105c), whatever the viewer can see;
    // the flag adds nothing to an entity that was not admitted at all.
    for (int viewer = 0; viewer < 32; ++viewer) {
        auto d = Broadcast(100, viewer, 0, empty, true);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, 0, empty, false);
        assert(d.admitted && !d.forced);
        d = Broadcast(100, viewer, 0, empty);
        assert(d.admitted && !d.forced);
        d = Broadcast(100, viewer, 32, empty, true);
        assert(d.admitted && d.forced);
    }
    for (int entity : {-1, 1024}) {
        const auto d = Broadcast(entity, 0, 0, empty, true);
        assert(!d.admitted && !d.forced);
    }
    for (int viewer : {-1, 32}) {
        const auto d = Broadcast(100, viewer, 0, empty, true);
        assert(!d.admitted && !d.forced);
    }
    // A permanent entity (EF_PERMANENT, 0x80 in eFlags) is never sent, before
    // anything that would force it (0x460f90): the client game draws it from
    // its baseline. No other eFlags bit matters here.
    for (int viewer = 0; viewer < 32; ++viewer) {
        for (std::uint32_t eFlags : {0x80u, 0xffu, 0x80000080u, 0xffffffffu}) {
            auto d = Broadcast(100, viewer, 0x22, selected, true, eFlags);
            assert(!d.admitted && !d.forced);
            d = Broadcast(viewer, viewer, 0, empty, false, eFlags);
            assert(!d.admitted && !d.forced);
            d = Broadcast(100, viewer, 0, empty, false, eFlags);
            assert(!d.admitted && !d.forced);
        }
        for (std::uint32_t eFlags : {0x0u, 0x7fu, 0x100u, 0xffffff7fu}) {
            auto d = Broadcast(100, viewer, 0, empty, false, eFlags);
            assert(d.admitted && !d.forced);
            d = Broadcast(100, viewer, 32, empty, false, eFlags);
            assert(d.admitted && d.forced);
            d = Broadcast(100, viewer, 0, empty, true, eFlags);
            assert(d.admitted && d.forced);
        }
    }
    // Jedi Outcast (jk2mp.exe 1.02, 0x44fcf0) adds for the same three reasons
    // and has neither the permanent skip nor the portal-scene add.
    for (int viewer = 0; viewer < 32; ++viewer) {
        const bool marked = viewer == 0 || viewer == 31;
        using fntech3::jk2::snapshot::Broadcast;
        auto d = Broadcast(100, viewer, 2, selected);
        assert(d.admitted && d.forced == marked);
        d = Broadcast(100, viewer, 2, empty);
        assert(d.admitted && !d.forced);
        d = Broadcast(100, viewer, 0, selected);
        assert(d.admitted && d.forced == marked);
        d = Broadcast(viewer, viewer, 0, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, 32, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, ~std::uint32_t(0x20), empty);
        assert(d.admitted && !d.forced);
    }
    for (int viewer : {-1, 32}) {
        const auto d = fntech3::jk2::snapshot::Broadcast(0, viewer, 0, selected);
        assert(!d.admitted && !d.forced);
    }
    for (int entity : {-1, 1024}) {
        const auto d = fntech3::jk2::snapshot::Broadcast(entity, 0, 32, selected);
        assert(!d.admitted && !d.forced);
    }
}
