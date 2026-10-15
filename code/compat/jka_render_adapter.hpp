/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_RENDER_ADAPTER_HPP
#define FNTECH3_JKA_RENDER_ADAPTER_HPP
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "jka_client_abi.h"
#include <climits>
#include <cstring>

/* Independently implemented record conversion. Retail jamp.exe (identity in
 * jka_client_abi.h), RE_AddRefEntityToScene at 0x4ab300 copies 53 dwords and
 * reads Ghoul2 at 208. RE_RenderScene at 0x4abc90 reads viewport at 0, FOV at
 * 16, origin at 24, axis at 48, time at 88, flags at 92, mask at 96 and copies
 * 256 text bytes from 128. Common type/flag meanings are pinned OpenJK layout
 * facts. Unserved effects are rejected instead of acquiring Q3 flag aliases.
 * The converted records enter the engine's existing renderer feature path.
 *
 * Retail's RB_SurfaceEntity (0x4bdcd0, table 0x4bddb8) draws the saber glow
 * (5) from saberLength at 180, the line (8) from origin, oldorigin and radius,
 * and the oriented line (9) from axis[1] and the union's line width at 148 and
 * texture scale at 156 (renderercommon/tr_jka_surface.h). Those are the three
 * Raven primitives the client game submits; the oriented quad (3), electricity
 * (6), cylinder (10) and mini-entity chain (11) stay refused by type. */
namespace fntech3::jka::render {
enum class Status { Ok, InvalidRecord, Ghoul2, EntityType, EntityFlags, ViewFlags };
inline const char *StatusName(Status value) noexcept {
    switch (value) {
    case Status::Ok: return "valid record";
    case Status::Ghoul2: return "unsupported client Ghoul2 renderer object";
    case Status::EntityType: return "unsupported Raven renderer entity type";
    case Status::EntityFlags: return "unsupported Raven renderer entity flags";
    case Status::ViewFlags: return "unsupported Raven renderer view flags";
    default: return "invalid renderer record";
    }
}
inline bool Finite(float value) noexcept {
    uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
    volatile uint32_t encoded = bits;
    return (encoded & 0x7f800000u) != 0x7f800000u;
}
template<std::size_t N> bool Finite(const float (&value)[N]) noexcept {
    for (float component : value) if (!Finite(component)) return false;
    return true;
}
template<class D, class S> void Copy(D &destination, const S &source) noexcept {
    static_assert(sizeof(D) == sizeof(S));
    std::memcpy(&destination, &source, sizeof(destination));
}
/* This is the existing Fn polygon-queue baseline in all three backends,
 * an engine allocation bound rather than a new retail record constant. */
inline constexpr std::size_t MaxPolyVertices = 32768;
inline bool ReadPolyVertex(const void *source, polyVert_t &destination) noexcept {
    if (!source) return false;
    fntech3_jka_mp_polyVert_t value;
    std::memcpy(&value, source, sizeof(value));
    if (!Finite(value.xyz) || !Finite(value.st)) return false;
    polyVert_t output{};
    Copy(output.xyz, value.xyz); Copy(output.st, value.st);
    Copy(output.modulate, value.modulate);
    destination = output;
    return true;
}
inline Status Entity(const fntech3_jka_mp_refEntity_t &source, refEntity_t &destination) noexcept {
    if (source.ghoul2) return Status::Ghoul2;
    refEntityType_t type;
    switch (source.reType) {
    case 0: type = RT_MODEL; break;
    case 2: type = RT_SPRITE; break;
    case 4: type = RT_BEAM; break;
    case 5: type = RT_JKA_SABER_GLOW; break;
    case 7: type = RT_PORTALSURFACE; break;
    case 8: type = RT_JKA_LINE; break;
    case 9: type = RT_JKA_ORIENTEDLINE; break;
    default: return Status::EntityType;
    }
    // 0x400 and 0x800 are Academy's own overrides (retail jamp.exe 1.01's
    // stage iterator, measured in renderercommon/tr_jka_entity.h). 0x2000
    // marks a distortion copy, which retail draws only as a refraction of
    // the screen behind it (backend 0x48af26; renderercommon/
    // tr_entity_refraction.h). None has a Q3 flag, so they cross as
    // engine-owned refEntity_t.jka fields.
    constexpr uint32_t commonFlags = 0x000fu | 0x00c0u | 0x0300u;
    constexpr uint32_t forceEntityAlpha = 0x0400u, rgbTint = 0x0800u, distortion = 0x2000u;
    if (uint32_t(source.renderfx) & ~(commonFlags | forceEntityAlpha | rgbTint | distortion))
        return Status::EntityFlags;
    if ((source.nonNormalizedAxes != 0 && source.nonNormalizedAxes != 1) ||
        !Finite(source.origin) || !Finite(source.oldorigin) || !Finite(source.lightingOrigin) ||
        !Finite(source.shaderTexCoord) || !Finite(source.shadowPlane) ||
        !Finite(source.backlerp) || !Finite(source.radius) || !Finite(source.rotation) ||
        !Finite(source.shaderTime) || source.radius < 0 || source.backlerp < 0 || source.backlerp > 1)
        return Status::InvalidRecord;
    for (const auto &axis : source.axis) if (!Finite(axis)) return Status::InvalidRecord;
    // Only the type that draws from a shape field must carry a number there;
    // the union's other members are the module's leftovers.
    if ((type == RT_JKA_SABER_GLOW && !Finite(source.saberLength)) ||
        (type == RT_JKA_ORIENTEDLINE && (!Finite(source.data.line.width) || !Finite(source.data.line.stscale))))
        return Status::InvalidRecord;
    refEntity_t output{};
    output.reType = type;
    if (type == RT_JKA_SABER_GLOW) output.jka.saberLength = source.saberLength;
    if (type == RT_JKA_ORIENTEDLINE) {
        output.jka.lineWidth = source.data.line.width;
        output.jka.lineSTScale = source.data.line.stscale;
    }
    constexpr struct { uint32_t external; int internal; } flags[] = {
        {0x1, RF_MINLIGHT}, {0x2, RF_THIRD_PERSON}, {0x4, RF_FIRST_PERSON}, {0x8, RF_DEPTHHACK},
        {0x40, RF_NOSHADOW}, {0x80, RF_LIGHTING_ORIGIN}, {0x100, RF_SHADOW_PLANE}, {0x200, RF_WRAP_FRAMES}
    };
    for (const auto &flag : flags) if (uint32_t(source.renderfx) & flag.external) output.renderfx |= flag.internal;
    output.jka.forceAlpha = (uint32_t(source.renderfx) & forceEntityAlpha) ? qtrue : qfalse;
    output.jka.rgbTint = (uint32_t(source.renderfx) & rgbTint) ? qtrue : qfalse;
    output.jka.distortion = (uint32_t(source.renderfx) & distortion) ? qtrue : qfalse;
    // Retail's copy draws only the screen behind it, so it casts no shadow.
    if (output.jka.distortion) output.renderfx |= RF_NOSHADOW;
    output.hModel = source.hModel;
    Copy(output.axis, source.axis);
    output.nonNormalizedAxes = source.nonNormalizedAxes ? qtrue : qfalse;
    Copy(output.origin, source.origin); Copy(output.oldorigin, source.oldorigin);
    Copy(output.lightingOrigin, source.lightingOrigin); output.shadowPlane = source.shadowPlane;
    output.frame = source.frame; output.oldframe = source.oldframe; output.backlerp = source.backlerp;
    output.skinNum = source.skinNum; output.customSkin = source.customSkin; output.customShader = source.customShader;
    Copy(output.shader, source.shaderRGBA); Copy(output.shaderTexCoord, source.shaderTexCoord);
    output.shaderTime.f = source.shaderTime; output.radius = source.radius; output.rotation = source.rotation;
    // Mini chains and the remaining procedural union/tail fields belong to
    // refused entity types. angles/modelScale are used by Ghoul2, which was
    // refused above.
    destination = output;
    return Status::Ok;
}
inline Status View(const fntech3_jka_mp_refdef_t &source, refdef_t &destination) noexcept {
    if (uint32_t(source.rdflags) & ~uint32_t(0x1u | 0x4u | 0x10u)) return Status::ViewFlags;
    if (source.x < 0 || source.y < 0 || source.width <= 0 || source.height <= 0 ||
        source.x > INT_MAX - source.width || source.y > INT_MAX - source.height ||
        !Finite(source.fov_x) || !Finite(source.fov_y) || source.fov_x <= 0 || source.fov_x >= 180 ||
        source.fov_y <= 0 || source.fov_y >= 180 || !Finite(source.vieworg)) return Status::InvalidRecord;
    for (const auto &axis : source.viewaxis) if (!Finite(axis)) return Status::InvalidRecord;
    // Fn's text deformation consumes each slot as a C string. Do not let a
    // malformed module record make it read into the next slot or view field.
    for (const auto &text : source.text)
        if (!std::memchr(text, 0, sizeof(text))) return Status::InvalidRecord;
    // Raven's normal-view draw-sky request (0x10) enters Fn's regular sky
    // material/cloud/fog/lighting path, which already draws ordinary skies.
    // It has no Q3 rdflag alias. Portal scenes (0x08) and other Raven-only
    // controls remain refused above; no portal state can have been enabled.
    refdef_t output{};
    output.x = source.x; output.y = source.y; output.width = source.width; output.height = source.height;
    output.fov_x = source.fov_x; output.fov_y = source.fov_y;
    Copy(output.vieworg, source.vieworg); Copy(output.viewaxis, source.viewaxis);
    output.time = source.time;
    if (source.rdflags & 0x1) output.rdflags |= RDF_NOWORLDMODEL;
    if (source.rdflags & 0x4) output.rdflags |= RDF_HYPERSPACE;
    Copy(output.areamask, source.areamask); Copy(output.text, source.text);
    // Retail does not consume viewangles/viewContents here. The host obtains
    // its liquid/underwater samples from its own collision and scene hooks.
    destination = output;
    return Status::Ok;
}
inline Status ReadEntity(const void *source, refEntity_t &destination) noexcept {
    if (!source) return Status::InvalidRecord;
    fntech3_jka_mp_refEntity_t value; std::memcpy(&value, source, sizeof(value));
    return Entity(value, destination);
}
inline Status ReadView(const void *source, refdef_t &destination) noexcept {
    if (!source) return Status::InvalidRecord;
    fntech3_jka_mp_refdef_t value; std::memcpy(&value, source, sizeof(value));
    return View(value, destination);
}
} // namespace fntech3::jka::render
#endif
