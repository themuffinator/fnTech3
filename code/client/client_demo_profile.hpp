/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_DEMO_PROFILE_HPP
#define FNTECH3_CLIENT_DEMO_PROFILE_HPP
#include "../qcommon/protocol_contract_profile.hpp"
#include "../qcommon/q3_demo_protocol.hpp"

namespace fntech3::demo {
inline constexpr unsigned RawQuake3Families = COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 |
    COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES;

/* A filename's protocol number never selects a different game's codec.
 * JKA and ioEF both name dm_26 but have distinct transport/message layouts.
 * Only the Q3 family has the older raw dm3/dm_43..48 containers. */
inline constexpr bool RawQuake3(unsigned families, int protocol, bool dm3) noexcept {
    return (families & RawQuake3Families) != 0 && (dm3 || q3::RawDemoProtocol(protocol));
}
inline constexpr bool Wire(unsigned families, int protocol, netchanWireProfile_t &output) noexcept {
    if (const auto *contract = fnql::protocol::FindForProfile(protocol, families)) {
        output = contract->wireProfile;
        return true;
    }
    if (RawQuake3(families, protocol, false)) {
        output = NETCHAN_WIRE_LEGACY_Q3;
        return true;
    }
    return false;
}
} // namespace fntech3::demo
#endif
