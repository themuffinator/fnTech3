/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_RENDER_STATE_HPP
#define FNTECH3_CLIENT_JKA_CGAME_RENDER_STATE_HPP
#include "../compat/jka_client_abi.h"
#include "../ghoul2/ghoul2_mdr.hpp"
#include "../renderercommon/tr_types.h"
#include <map>

/* Working allocations survive a native error until cgame cleanup releases
 * them. Only engine-owned model bytes and copied records enter this state. */
namespace fntech3::jka::cgame::scene {
inline constexpr std::size_t MaxModels = 1024;
struct Runtime {
    fntech3::ghoul2::geometry::Geometry geometry;
    std::vector<int> visible;
    const std::vector<unsigned char> *meshBytes = nullptr;
    qhandle_t handle = 0;
    std::uint64_t owner = 0;
};
// One owned registration per model of an instance: its identity and index.
using Key = std::pair<std::uint32_t, int>;
struct State {
    std::map<Key, Runtime> models;
    std::vector<fntech3_jka_mp_refEntity_t> pending;
    std::vector<refEntity_t> prepared;
    std::vector<int> visible, traversal, order;
    std::vector<fntech3::ghoul2::pose::Palette> palettes;
    std::vector<unsigned char> bytes;
    std::vector<float> bones;
    void ClearScene() { pending.clear(); prepared.clear(); }
    void Clear() { *this = State{}; }
};
inline State state;
} // namespace fntech3::jka::cgame::scene
#endif
