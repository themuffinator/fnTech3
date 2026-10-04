/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_COMMAND_VALUES_HPP
#define FNTECH3_CLIENT_JKA_COMMAND_VALUES_HPP

#include "../qcommon/q_shared.h"

namespace fntech3::jka::command {

/* Engine-owned selections supplied by cgame, separate from key/button state.
 * Retail import235 stores integers at jamp.exe 0x00411964/0x0041196a; the
 * command builder publishes their low bytes. No external POD changes here. */
class Selections {
    unsigned char force_ = 0, inventory_ = 0;
public:
    void Reset() noexcept { force_ = inventory_ = 0; }
    void Set(int force, int inventory) noexcept {
        force_ = static_cast<unsigned char>(force);
        inventory_ = static_cast<unsigned char>(inventory);
    }
    void Publish(usercmd_t &destination) const noexcept {
        destination.raven.forcesel = force_;
        destination.raven.invensel = inventory_;
    }
};

inline Selections selections;

} // namespace fntech3::jka::command
#endif
