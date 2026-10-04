/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_RECORDS_HPP
#define FNTECH3_CLIENT_JKA_RECORDS_HPP
#include "../qcommon/q_shared.h"
#include "../compat/jka_game_abi.h"
#include "../compat/jka_ui_abi.h"
#include "../renderercommon/tr_types.h"
#include <cmath>
#include <cstring>
#include <limits>

/* Profile output records shared by the legacy UI and cgame boundaries.
 * They are marshalled by fields; canonical structures never cross directly. */
namespace fntech3::jka::records {
inline bool Finite(float value) noexcept {
    std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
    volatile std::uint32_t encoded = bits;
    return (encoded & 0x7f800000u) != 0x7f800000u;
}
namespace detail {
template<std::size_t N, std::size_t M>
void Text(char (&to)[N], const char (&from)[M]) noexcept {
    std::size_t n = 0;
    for (; n+1 < N && n < M && from[n]; ++n) to[n] = from[n];
    to[n] = '\0';
}
}

inline void CvarToExternal(const vmCvar_t &internal, fntech3_jka_vmCvar_t &external) noexcept {
    external = {};
    external.handle = internal.handle; external.modificationCount = internal.modificationCount;
    external.value = internal.value; external.integer = internal.integer;
    detail::Text(external.string, internal.string);
}
inline void CvarToInternal(const fntech3_jka_vmCvar_t &external, vmCvar_t &internal) noexcept {
    internal = {};
    internal.handle = external.handle; internal.modificationCount = external.modificationCount;
    internal.value = external.value; internal.integer = external.integer;
    detail::Text(internal.string, external.string);
}
inline void Token(const pc_token_t &internal, fntech3_jka_mp_pc_token_t &external) noexcept {
    external = {};
    external.type = internal.type; external.subtype = internal.subtype;
    external.intvalue = internal.intvalue; external.floatvalue = internal.floatvalue;
    detail::Text(external.string, internal.string);
}
inline void Time(const qtime_t &internal, fntech3_jka_mp_qtime_t &external) noexcept {
    external = {internal.tm_sec, internal.tm_min, internal.tm_hour, internal.tm_mday,
        internal.tm_mon, internal.tm_year, internal.tm_wday, internal.tm_yday, internal.tm_isdst};
}

inline constexpr int TextureCompression(int internal) noexcept {
    switch (internal) {
    case TC_NONE: return 0;
    case TC_S3TC: return 1;
    case TC_S3TC_ARB: return 3; // Raven's 2 is the separate S3TC_DXT method.
    default: return -1;
    }
}

/* The host admits this transfer only for the legacy native module. Engine-owned
 * strings remain in its stable renderer configuration; no strings point into
 * a temporary converted record. The two extra capability values must come
 * from the selected backend's actual contract, not a module's request. */
inline bool Glconfig(const glconfig_t &internal, float maximumAnisotropy,
    bool clampToEdgeAvailable, fntech3_jka_mp_glconfig_t &external) noexcept {
    if (!Finite(maximumAnisotropy) || maximumAnisotropy < 0) return false;
    int compression;
    static_assert(sizeof(compression) == sizeof(internal.textureCompression));
    std::memcpy(&compression, &internal.textureCompression, sizeof(compression));
    compression = TextureCompression(compression);
    if (compression < 0) return false;
    fntech3_jka_mp_glconfig_t next{};
    const char *strings[]{internal.renderer_string, internal.vendor_string,
        internal.version_string, internal.extensions_string};
    std::uint32_t *pointers[]{&next.renderer_string, &next.vendor_string,
        &next.version_string, &next.extensions_string};
    for (unsigned n = 0; n < 4; ++n) {
        const auto address = reinterpret_cast<std::uintptr_t>(strings[n]);
        if (address > std::numeric_limits<std::uint32_t>::max()) return false;
        *pointers[n] = static_cast<std::uint32_t>(address);
    }
    next.maxTextureSize = internal.maxTextureSize;
    next.maxActiveTextures = internal.numTextureUnits;
    next.maxTextureFilterAnisotropy = maximumAnisotropy;
    next.colorBits = internal.colorBits; next.depthBits = internal.depthBits;
    next.stencilBits = internal.stencilBits;
    next.deviceSupportsGamma = internal.deviceSupportsGamma != qfalse;
    next.textureCompression = compression;
    next.textureEnvAddAvailable = internal.textureEnvAddAvailable != qfalse;
    next.clampToEdgeAvailable = clampToEdgeAvailable;
    next.vidWidth = internal.vidWidth; next.vidHeight = internal.vidHeight;
    next.displayFrequency = internal.displayFrequency;
    next.isFullscreen = internal.isFullscreen != qfalse;
    next.stereoEnabled = internal.stereoEnabled != qfalse;
    external = next;
    return true;
}

} // namespace fntech3::jka::records
#endif
