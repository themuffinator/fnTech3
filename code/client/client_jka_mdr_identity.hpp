/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_MDR_IDENTITY_HPP
#define FNTECH3_CLIENT_JKA_MDR_IDENTITY_HPP
#include <cstdint>
#include <limits>

/* UI and cgame share the renderer's registration namespace. Keep one
 * process-lifetime allocator across both translation units and restarts. */
namespace fntech3::jka::mdr {
inline std::uint64_t ownerCounter = 0;
inline std::uint64_t NextOwner() noexcept {
    if (ownerCounter == (std::numeric_limits<std::uint64_t>::max)()) return 0;
    return ++ownerCounter;
}
} // namespace fntech3::jka::mdr
#endif
