/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_RENDER_ADAPTER_HPP
#define FNTECH3_JK2_RENDER_ADAPTER_HPP
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "jk2_render_abi.h"
#include <climits>
#include <cstring>

// Outcast menu records enter the existing Fn model/material/lighting path.
// Raven procedural entities and Ghoul2 require their own implementation;
// their flags must never acquire unrelated canonical renderer meanings.
namespace fntech3::jk2::render {
inline bool Finite(float value) noexcept {
    uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
    volatile uint32_t encoded = bits;
    return (encoded & 0x7f800000u) != 0x7f800000u;
}
template<std::size_t N> bool Finite(const float (&values)[N]) noexcept {
    for (float value : values) if (!Finite(value)) return false;
    return true;
}
template<class To, class From> void Copy(To &to, const From &from) noexcept {
    static_assert(sizeof(to) == sizeof(from));
    std::memcpy(&to, &from, sizeof(to));
}
enum class Status { Ok, Invalid, EntityType, EntityFlags, Ghoul2, ViewFlags };
inline const char *Description(Status status) noexcept {
    switch (status) {
    case Status::EntityType: return "procedural entity renderer boundary is open";
    case Status::EntityFlags: return "Raven entity flag renderer boundary is open";
    case Status::Ghoul2: return "Ghoul2 renderer boundary is open";
    case Status::ViewFlags: return "Raven view flag renderer boundary is open";
    default: return "invalid renderer record";
    }
}
inline Status Entity(const fntech3_jk2_mp_refEntity_t &source, refEntity_t &destination) noexcept {
    if (source.ghoul2) return Status::Ghoul2;
    if (source.reType != 0) return Status::EntityType;
    // 0x400 forces the entity's alpha on single-texture stages, blended, as
    // Academy's does: jk2mp.exe's stage path (0x488e7a..0x488e93) writes the
    // record's alpha (+0x53) to every vertex and draws with state 0x65.
    constexpr uint32_t forceEntityAlpha = 0x0400u;
    // 0x800 reaches nothing: no test or mask in jk2mp.exe's renderer
    // (0x466000..0x4b0000) carries that bit, so it is accepted and dropped.
    constexpr uint32_t unread = 0x0800u;
    constexpr uint32_t supportedFlags = 0x000fu | 0x00c0u | 0x0300u | forceEntityAlpha | unread;
    if (uint32_t(source.renderfx) & ~supportedFlags) return Status::EntityFlags;
    if ((source.nonNormalizedAxes != 0 && source.nonNormalizedAxes != 1) ||
        !Finite(source.origin) || !Finite(source.oldorigin) || !Finite(source.lightingOrigin) ||
        !Finite(source.shaderTexCoord) || !Finite(source.shadowPlane) || !Finite(source.shaderTime) ||
        !Finite(source.backlerp) || source.backlerp < 0 || source.backlerp > 1)
        return Status::Invalid;
    for (const auto &axis : source.axis) if (!Finite(axis)) return Status::Invalid;
    refEntity_t next{};
    next.reType = RT_MODEL;
    constexpr struct { uint32_t external; int internal; } flags[] = {
        {1, RF_MINLIGHT}, {2, RF_THIRD_PERSON}, {4, RF_FIRST_PERSON}, {8, RF_DEPTHHACK},
        {0x40, RF_NOSHADOW}, {0x80, RF_LIGHTING_ORIGIN}, {0x100, RF_SHADOW_PLANE}, {0x200, RF_WRAP_FRAMES}
    };
    for (const auto &flag : flags) if (uint32_t(source.renderfx) & flag.external) next.renderfx |= flag.internal;
    next.jka.forceAlpha = (uint32_t(source.renderfx) & forceEntityAlpha) ? qtrue : qfalse;
    next.hModel = source.hModel;
    Copy(next.axis, source.axis); Copy(next.origin, source.origin); Copy(next.oldorigin, source.oldorigin);
    Copy(next.lightingOrigin, source.lightingOrigin); next.shadowPlane = source.shadowPlane;
    next.nonNormalizedAxes = source.nonNormalizedAxes ? qtrue : qfalse;
    next.frame = source.frame; next.oldframe = source.oldframe; next.backlerp = source.backlerp;
    next.skinNum = source.skinNum; next.customSkin = source.customSkin; next.customShader = source.customShader;
    Copy(next.shader, source.shaderRGBA); Copy(next.shaderTexCoord, source.shaderTexCoord);
    next.shaderTime.f = source.shaderTime;
    destination = next;
    return Status::Ok;
}
inline Status View(const fntech3_jk2_mp_refdef_t &source, refdef_t &destination) noexcept {
    if (uint32_t(source.rdflags) & ~uint32_t(1 | 4)) return Status::ViewFlags;
    // Retail main.menu intentionally places logo models partly offscreen
    // (authored x=-78/-19). Keep signed origins; only dimensions/extents are
    // constrained here. The UI boundary also checks Fn's GL-y conversion.
    if (source.width <= 0 || source.height <= 0 ||
        source.x > INT_MAX - source.width || source.y > INT_MAX - source.height ||
        !Finite(source.fov_x) || !Finite(source.fov_y) || source.fov_x <= 0 || source.fov_x >= 180 ||
        source.fov_y <= 0 || source.fov_y >= 180 || !Finite(source.vieworg)) return Status::Invalid;
    for (const auto &axis : source.viewaxis) if (!Finite(axis)) return Status::Invalid;
    // Fn's text deformation consumes each slot as a C string. Do not let a
    // malformed module record make it read into the next slot or view field.
    for (const auto &text : source.text)
        if (!std::memchr(text, 0, sizeof(text))) return Status::Invalid;
    refdef_t next{};
    next.x = source.x; next.y = source.y; next.width = source.width; next.height = source.height;
    next.fov_x = source.fov_x; next.fov_y = source.fov_y;
    Copy(next.vieworg, source.vieworg); Copy(next.viewaxis, source.viewaxis);
    next.time = source.time;
    if (source.rdflags & 1) next.rdflags |= RDF_NOWORLDMODEL;
    if (source.rdflags & 4) next.rdflags |= RDF_HYPERSPACE;
    Copy(next.areamask, source.areamask); Copy(next.text, source.text);
    destination = next;
    return Status::Ok;
}
inline bool PolyVertex(const void *source, polyVert_t &destination) noexcept {
    fntech3_jk2_mp_polyVert_t value;
    std::memcpy(&value, source, sizeof(value));
    if (!Finite(value.xyz) || !Finite(value.st)) return false;
    Copy(destination.xyz, value.xyz); Copy(destination.st, value.st); Copy(destination.modulate, value.modulate);
    return true;
}
} // namespace fntech3::jk2::render
#endif
