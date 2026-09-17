// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once
#include <SDL3/SDL.h>
#include "sdl_raii.h"
#include "../platform/application_icon_data.h"
#include "../platform/game_icon.hpp"
#include <fstream>
#include <vector>
#ifdef _WIN32
#include "../win32/win_game_icon.h"
#endif
#ifdef MACOS_X
#include "sdl_macos_window.h"
#endif

namespace fntech3::icon {
#ifdef _WIN32
inline fnql::sdl::ScopedSurface LoadGameExecutableIcon(const char *root, comGameId_t profile)
{
    HICON largeIcon = nullptr, smallIcon = nullptr;
    ExtractGameIcons(root, profile, largeIcon, smallIcon);
    std::vector<unsigned char> pixels;
    int width = 0, height = 0;
    const bool copied = CopyIconPixels(largeIcon ? largeIcon : smallIcon, pixels, width, height);
    if (largeIcon) DestroyIcon(largeIcon);
    if (smallIcon && smallIcon != largeIcon) DestroyIcon(smallIcon);
    if (!copied) return {};
    fnql::sdl::ScopedSurface surface(SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32));
    if (!surface) return {};
    for (int y = 0; y < height; ++y)
        SDL_memcpy(static_cast<unsigned char *>(surface->pixels) + y * surface->pitch,
                   pixels.data() + y * width * 4, width * 4);
    return surface;
}
#endif

inline fnql::sdl::ScopedSurface LoadGamePng(const char *root, comGameId_t profile)
{
#if SDL_VERSION_ATLEAST(3, 4, 0)
    for (unsigned i = 0; Candidate(profile, Source::Png, i); ++i) {
        const auto path = Resolve(root, profile, Source::Png, i);
        if (path.empty()) continue;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto size = file.tellg();
        if (size < 33 || size > 4 * 1024 * 1024) continue;
        std::vector<unsigned char> bytes(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), size) ||
            !ReasonablePng(bytes.data(), bytes.size())) continue;
        SDL_IOStream *stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
        if (!stream) continue;
        fnql::sdl::ScopedSurface image(SDL_LoadPNG_IO(stream, true));
        if (image) return image;
    }
#else
    (void)root;
    (void)profile;
#endif
    return {};
}

// Call on the video thread, also when reusing a window for vid_restart. SDL
// copies the pixels; the surface and input buffer need not outlive this call.
inline void SetWindowIcon(SDL_Window *window, const char *root,
                          comGameId_t profile, bool inherit)
{
    fnql::sdl::ScopedSurface icon;
#ifdef _WIN32
    if (inherit) icon = LoadGameExecutableIcon(root, profile);
#endif
    if (inherit && !icon) icon = LoadGamePng(root, profile);
    if (!icon) {
        icon.reset(SDL_CreateSurfaceFrom(FNTECH3_ICON_SIZE, FNTECH3_ICON_SIZE,
            SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char *>(FNTECH3_ICON_RGBA),
            FNTECH3_ICON_SIZE * 4));
    }
    if (icon && !SDL_SetWindowIcon(window, icon.get()))
        Com_DPrintf("Window icon unavailable: %s\n", SDL_GetError());
#ifdef MACOS_X
    if (inherit) {
        for (unsigned i = 0; Candidate(profile, Source::Bundle, i); ++i) {
            const auto path = Resolve(root, profile, Source::Bundle, i);
            if (!path.empty() && FNTECH3_MacSetGameIcon(path.u8string().c_str())) break;
        }
    }
#endif
}
} // namespace fntech3::icon
