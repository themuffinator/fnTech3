/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_CLOCK_HPP
#define FNTECH3_CLIENT_JKA_CGAME_CLOCK_HPP
#include <array>
#include <cstdint>

namespace fntech3::jka::cgame::clock {
/* Retail0x4de0d0 stores either clock, clears clock1 when it exceeds the
 * wrapping clock0+200 signed bound, and uses nonzero clock1 before clock0.
 * Retail frame0x4148b0 publishes the same client time before DrawActiveFrame.
 * Pose ownership stays per module; local-server clock sharing is unserved. */
class State {
    std::array<std::int32_t,2> clocks_{};
    bool frameReady_ = false, localServer_ = false;
public:
    void Reset() noexcept { clocks_ = {}; frameReady_ = localServer_ = false; }
    bool Set(int index, std::int32_t value) noexcept {
        if (index < 0 || index > 1) return false;
        clocks_[index] = value;
        const auto limit = static_cast<std::int32_t>(static_cast<std::uint32_t>(clocks_[0]) + 200u);
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
} // namespace fntech3::jka::cgame::clock
#endif
