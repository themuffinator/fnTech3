/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/server/server_jka_snapshot.hpp"
#include <cassert>
#include <initializer_list>
int main() {
    using fntech3::jka::snapshot::Broadcast;
    const std::uint32_t empty[2]{};
    const std::uint32_t selected[2]{0x80000001u, 0xffffffffu};
    for (int viewer = 0; viewer < 32; ++viewer) {
        const bool marked = viewer == 0 || viewer == 31;
        auto d = Broadcast(100, viewer, 2, selected);
        assert(d.admitted == marked && d.forced == marked);
        d = Broadcast(100, viewer, 0, selected);
        assert(d.admitted && d.forced == marked);
        d = Broadcast(viewer, viewer, 2, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, 32, empty);
        assert(d.admitted && d.forced);
        d = Broadcast(100, viewer, 34, empty);
        assert(!d.admitted && !d.forced);
    }
    for (int viewer : {-1, 32, 63, 64, 2147483647}) {
        const auto d = Broadcast(0, viewer, 2, selected);
        assert(!d.admitted && !d.forced);
    }
    for (int entity : {-1, 1024, 2147483647}) {
        const auto d = Broadcast(entity, 0, 0, selected);
        assert(!d.admitted && !d.forced);
    }
}
