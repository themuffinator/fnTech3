// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once
#include <SDL3/SDL.h>
#include "sdl_raii.h"
#include "../platform/application_icon_data.h"
#include "../platform/game_icon.hpp"
#include "../platform/game_icon_builtin.hpp"
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

inline SDL_Surface *CreateSurface(const Image &image)
{
    SDL_Surface *surface = SDL_CreateSurface(static_cast<int>(image.width),
        static_cast<int>(image.height), SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    for (unsigned y = 0; y < image.height; ++y)
        SDL_memcpy(static_cast<unsigned char *>(surface->pixels) + size_t(y) * surface->pitch,
                   image.rgba.data() + size_t(y) * image.width * 4, size_t(image.width) * 4);
    return surface;
}

// The built-in remake as one surface: the largest frame up to 128 px is the
// 100% image and every other frame an alternate. SDL passes the alternates to
// Wayland and Cocoa (Dock); its Windows and X11 backends use the main image.
inline fnql::sdl::ScopedSurface LoadBuiltinIcon(const char *profileKey)
{
    const std::vector<Image> frames = DecodeBuiltinImages(profileKey);
    size_t primary = frames.size();
    for (size_t i = 0; i < frames.size(); ++i)
        if (frames[i].width <= 128) primary = i;
    if (primary == frames.size()) return {};
    fnql::sdl::ScopedSurface surface(CreateSurface(frames[primary]));
    if (!surface) return {};
    for (size_t i = 0; i < frames.size(); ++i) {
        if (i == primary) continue;
        if (SDL_Surface *alternate = CreateSurface(frames[i])) {
            SDL_AddSurfaceAlternateImage(surface.get(), alternate); // takes its own reference
            SDL_DestroySurface(alternate);
        }
    }
    return surface;
}

// Call on the video thread, also when reusing a window for vid_restart. SDL
// copies the pixels; the surface and input buffer need not outlive this call.
inline Shown SetWindowIcon(SDL_Window *window, const char *root, comGameId_t profile,
                           const char *profileKey, Mode mode)
{
    fnql::sdl::ScopedSurface icon;
    Shown shown = Shown::Installation;
    if (mode == Mode::Installation) {
#ifdef _WIN32
        icon = LoadGameExecutableIcon(root, profile);
#endif
        if (!icon) icon = LoadGamePng(root, profile);
    }
    if (!icon && mode != Mode::Engine) {
        icon = LoadBuiltinIcon(profileKey);
        shown = Shown::BuiltIn;
    }
    if (!icon) {
        icon.reset(SDL_CreateSurfaceFrom(FNTECH3_ICON_SIZE, FNTECH3_ICON_SIZE,
            SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char *>(FNTECH3_ICON_RGBA),
            FNTECH3_ICON_SIZE * 4));
        shown = Shown::Engine;
    }
    if (!icon || !SDL_SetWindowIcon(window, icon.get())) {
        Com_DPrintf("Window icon unavailable: %s\n", SDL_GetError());
        return Shown::None;
    }
#ifdef _WIN32
    // SDL gives Windows one bitmap for both caption and taskbar and lets it be
    // rescaled; the executable's own icon groups carry exact frames instead.
    if (shown != Shown::Installation) {
        HWND native = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        const BuiltinIcon *builtin = shown == Shown::BuiltIn ? FindBuiltinIcon(profileKey) : nullptr;
        BindResourceIcons(native, builtin ? builtin->resource : IDI_ICON1);
    }
#endif
#ifdef MACOS_X
    if (mode == Mode::Installation) {
        for (unsigned i = 0; Candidate(profile, Source::Bundle, i); ++i) {
            const auto path = Resolve(root, profile, Source::Bundle, i);
            if (!path.empty() && FNTECH3_MacSetGameIcon(path.u8string().c_str())) {
                shown = Shown::Installation;
                break;
            }
        }
    }
#endif
    return shown;
}
} // namespace fntech3::icon
