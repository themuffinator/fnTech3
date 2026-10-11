/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_GHOUL2_CLOCK_HPP
#define FNTECH3_COMPAT_JKA_GHOUL2_CLOCK_HPP
#include <array>
#include <cstdint>
#include <cstring>

namespace fntech3::jka::clock {
/* Retail jamp.exe 1.01 uses one pair for game, cgame and UI. SetTime
 * (0x4de0d0) clears the client clock beyond the wrapping server+200 bound;
 * The bolt core (0x4dfa00) prefers the nonzero client clock. Module destruction
 * does not clear either clock. Model ownership remains per module.
 * See docs/fntech3/2026-10-07-jedi-compatibility-audit.md. */
class State {
    std::array<std::int32_t, 2> clocks_{};
    bool frameReady_ = false, localServer_ = false;
public:
    void ClearFrame() noexcept { frameReady_ = localServer_ = false; }
    void Reset() noexcept { clocks_ = {}; ClearFrame(); }
    bool Set(int index, std::int32_t value) noexcept {
        if (index < 0 || index > 1) return false;
        clocks_[index] = value;
        const auto bits = static_cast<std::uint32_t>(clocks_[0]) + 200u;
        std::int32_t limit;
        std::memcpy(&limit, &bits, sizeof(limit));
        if (clocks_[1] > limit) clocks_[1] = 0;
        return true;
    }
    void BeginFrame(std::int32_t value, bool localServer) noexcept {
        frameReady_ = true; localServer_ = localServer;
        if (!localServer) Set(0, value);
        Set(1, value);
    }
    std::int32_t Time() const noexcept { return clocks_[1] ? clocks_[1] : clocks_[0]; }
    bool FrameReady() const noexcept { return frameReady_; }
    bool LocalServer() const noexcept { return localServer_; }
};
// The engine executable owns this state; renderer DLLs never include it.
inline State shared;
// Retail 0x10bcb6c is shared too: a failed no-rotation bolt request leaves
// this set for the next successful request, even across module boundaries.
inline bool noRotationPending = false;
} // namespace fntech3::jka::clock
#endif
