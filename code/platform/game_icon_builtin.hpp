// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once

// Built-in remakes of each game's application icon (assets/icons/games).
// Presentation metadata only: never changes the selected profile or loads code.

#include "game_icon_table.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

// Mark Adler's inflate (code/qcommon/puff.c), compiled as C.
extern "C" int32_t puff(uint8_t *dest, uint32_t *destlen, uint8_t *source, uint32_t *sourcelen);

namespace fntech3::icon {
// cl_gameIcon: 0 keeps the fnTech3 icon, 1 (default) shows the built-in remake
// of the active game's icon, 2 prefers the installation's own icon and falls
// back to the remake.
enum class Mode { Engine, BuiltIn, Installation };

// Which icon a window ended up with; None when no window or icon was set.
enum class Shown { None, Engine, BuiltIn, Installation };

inline Mode ModeFromCvar(int value)
{
    if (value <= 0) return Mode::Engine;
    return value == 2 ? Mode::Installation : Mode::BuiltIn;
}

inline const char *ModeDescription()
{
    return "Window icon: 0 = fnTech3, 1 = built-in remake of the active game's icon,"
           " 2 = the installation's own icon when present, else the remake. Applies at vid_restart.";
}

inline const BuiltinIcon *FindBuiltinIcon(const char *profileKey)
{
    if (!profileKey || !*profileKey) return nullptr;
    for (const BuiltinIcon &icon : kBuiltinIcons)
        if (!std::strcmp(icon.profile, profileKey)) return &icon;
    return nullptr;
}

// Desktops pair windows with launchers by application id (Wayland app_id, X11
// WM_CLASS). Each game's launcher template is fntech3-<profile key>.desktop.
inline std::string ApplicationId(const char *profileKey, Mode mode)
{
    if (mode == Mode::Engine || !FindBuiltinIcon(profileKey)) return "fntech3";
    return std::string("fntech3-") + profileKey;
}

struct EncodedImage {
    const char *profile;
    unsigned size;
    const unsigned char *png;
    size_t length;
};

struct Image {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<unsigned char> rgba; // straight alpha, rows top to bottom
};

inline uint32_t ReadBigEndian32(const unsigned char *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

// Decodes the icon frames this project generates: 8-bit RGBA, non-interlaced
// PNG no larger than 1024 px. Anything else is rejected rather than guessed.
inline bool DecodePng(const unsigned char *data, size_t size, Image &out)
{
    static const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    out = Image();
    if (!data || size < 8 || std::memcmp(data, signature, 8)) return false;
    uint32_t width = 0, height = 0;
    bool header = false, end = false;
    std::vector<unsigned char> stream;
    for (size_t pos = 8; pos + 12 <= size;) {
        const uint32_t length = ReadBigEndian32(data + pos);
        if (length > size - pos - 12) return false;
        const unsigned char *type = data + pos + 4;
        const unsigned char *body = data + pos + 8;
        if (!std::memcmp(type, "IHDR", 4)) {
            if (header || length != 13) return false;
            width = ReadBigEndian32(body);
            height = ReadBigEndian32(body + 4);
            if (!width || !height || width > 1024 || height > 1024 || body[8] != 8 ||
                body[9] != 6 || body[10] || body[11] || body[12]) return false;
            header = true;
        } else if (!std::memcmp(type, "IDAT", 4)) {
            if (!header) return false;
            stream.insert(stream.end(), body, body + length);
        } else if (!std::memcmp(type, "IEND", 4)) {
            end = true;
            break;
        }
        pos += 12 + size_t(length);
    }
    // zlib wrapper: deflate, no preset dictionary, valid check bits, Adler-32.
    if (!header || !end || stream.size() < 6 || (stream[0] & 0x0f) != 8 || (stream[0] >> 4) > 7 ||
        (stream[1] & 0x20) || ((unsigned(stream[0]) << 8) | stream[1]) % 31) return false;
    const size_t stride = size_t(width) * 4;
    std::vector<unsigned char> raw((stride + 1) * height);
    uint32_t rawLength = static_cast<uint32_t>(raw.size());
    uint32_t consumed = static_cast<uint32_t>(stream.size() - 6);
    if (puff(raw.data(), &rawLength, stream.data() + 2, &consumed) != 0 || rawLength != raw.size())
        return false;
    uint32_t a = 1, b = 0;
    for (unsigned char value : raw) {
        a = (a + value) % 65521;
        b = (b + a) % 65521;
    }
    if (((b << 16) | a) != ReadBigEndian32(stream.data() + stream.size() - 4)) return false;
    out.rgba.resize(stride * height);
    for (uint32_t y = 0; y < height; ++y) {
        const unsigned char *line = raw.data() + y * (stride + 1);
        unsigned char *row = out.rgba.data() + y * stride;
        const unsigned char *up = y ? row - stride : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            const int left = x >= 4 ? row[x - 4] : 0;
            const int above = up ? up[x] : 0;
            const int corner = up && x >= 4 ? up[x - 4] : 0;
            int predictor;
            switch (line[0]) {
            case 0: predictor = 0; break;
            case 1: predictor = left; break;
            case 2: predictor = above; break;
            case 3: predictor = (left + above) / 2; break;
            case 4: {
                const int pa = std::abs(above - corner), pb = std::abs(left - corner);
                const int pc = std::abs(left + above - 2 * corner);
                predictor = pa <= pb && pa <= pc ? left : pb <= pc ? above : corner;
                break;
            }
            default: out = Image(); return false;
            }
            row[x] = static_cast<unsigned char>((line[1 + x] + predictor) & 0xff);
        }
    }
    out.width = width;
    out.height = height;
    return true;
}

// Generated with the artwork: code/platform/game_icon_data.cpp. Clients link it
// everywhere except Windows, whose clients read the same frames from resources.
const EncodedImage *EmbeddedImages(size_t &count);

#ifdef _WIN32
// Windows clients link every game's ICO (code/win32/win_client_resource.rc).
// Their frames are PNG images; return them straight from the executable's
// read-only resource section, which outlives any caller.
inline std::vector<EncodedImage> BuiltinImages(const char *profileKey)
{
    std::vector<EncodedImage> images;
    const BuiltinIcon *icon = FindBuiltinIcon(profileKey);
    if (!icon) return images;
    const HMODULE module = GetModuleHandleW(nullptr);
    const auto resource = [module](unsigned id, unsigned type, DWORD &size) -> const unsigned char * {
        HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(type));
        HGLOBAL loaded = found ? LoadResource(module, found) : nullptr;
        size = found ? SizeofResource(module, found) : 0;
        return loaded ? static_cast<const unsigned char *>(LockResource(loaded)) : nullptr;
    };
    DWORD groupSize = 0;
    const unsigned char *group = resource(icon->resource, 14 /* RT_GROUP_ICON */, groupSize);
    if (!group || groupSize < 6) return images;
    const unsigned count = group[4] | (group[5] << 8);
    for (unsigned i = 0; i < count && 6 + (i + 1) * 14 <= groupSize; ++i) {
        const unsigned char *entry = group + 6 + i * 14;
        const unsigned id = entry[12] | (entry[13] << 8);
        DWORD length = 0;
        const unsigned char *png = resource(id, 3 /* RT_ICON */, length);
        if (!png || length < 8 || png[0] != 137 || png[1] != 'P') continue;
        images.push_back({icon->profile, entry[0] ? entry[0] : 256u, png, length});
    }
    return images;
}
#else
inline std::vector<EncodedImage> BuiltinImages(const char *profileKey)
{
    std::vector<EncodedImage> images;
    const BuiltinIcon *icon = FindBuiltinIcon(profileKey);
    if (!icon) return images;
    size_t count = 0;
    const EncodedImage *all = EmbeddedImages(count);
    for (size_t i = 0; i < count; ++i)
        if (!std::strcmp(all[i].profile, icon->profile)) images.push_back(all[i]);
    return images;
}
#endif

// Decoded frames, smallest first; frames that fail to decode are skipped.
inline std::vector<Image> DecodeBuiltinImages(const char *profileKey, unsigned maxSize = 1024)
{
    std::vector<Image> decoded;
    for (const EncodedImage &frame : BuiltinImages(profileKey)) {
        Image image;
        if (frame.size <= maxSize && DecodePng(frame.png, frame.length, image))
            decoded.push_back(std::move(image));
    }
    std::sort(decoded.begin(), decoded.end(),
              [](const Image &a, const Image &b) { return a.width < b.width; });
    return decoded;
}
} // namespace fntech3::icon
