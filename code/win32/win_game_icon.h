// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once
#include <windows.h>
#include <shellapi.h>
#include "resource.h"
#include "../platform/game_icon.hpp"
#include <vector>
#include <algorithm>

namespace fntech3::icon {
inline void ExtractGameIcons(const char *root, comGameId_t profile, HICON &largeIcon, HICON &smallIcon)
{
    largeIcon = smallIcon = nullptr;
    for (unsigned i = 0; Candidate(profile, Source::Executable, i); ++i) {
        const auto path = Resolve(root, profile, Source::Executable, i);
        if (path.empty()) continue;
        // Read icon resources; never load or execute the game's code.
        ExtractIconExW(path.c_str(), 0, &largeIcon, &smallIcon, 1);
        if (largeIcon || smallIcon) return;
    }
}

// Convert a Windows icon to straight RGBA for SDL, which owns the window icon
// across fullscreen, DPI and window-style changes. Draw only into memory DIBs.
// Old icons use a monochrome AND mask; newer ones have premultiplied alpha.
inline bool CopyIconPixels(HICON icon, std::vector<unsigned char> &rgba, int &width, int &height)
{
    ICONINFO info{};
    if (!icon || !GetIconInfo(icon, &info)) return false;
    BITMAP bitmap{};
    const bool found = GetObjectW(info.hbmColor ? info.hbmColor : info.hbmMask,
                                  sizeof(bitmap), &bitmap) != 0;
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    width = bitmap.bmWidth;
    height = info.hbmColor ? bitmap.bmHeight : bitmap.bmHeight / 2;
    if (!found || width <= 0 || height <= 0 || width > 256 || height > 256) return false;
    BITMAPINFO format{};
    format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    format.bmiHeader.biWidth = width;
    format.bmiHeader.biHeight = -height;
    format.bmiHeader.biPlanes = 1;
    format.bmiHeader.biBitCount = 32;
    format.bmiHeader.biCompression = BI_RGB;
    const size_t bytes = size_t(width) * height * 4;
    rgba.resize(bytes);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    void *pixels = nullptr;
    HBITMAP dib = CreateDIBSection(dc, &format, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dib) { DeleteDC(dc); return false; }
    HGDIOBJ previous = SelectObject(dc, dib);
    if (!previous || previous == HGDI_ERROR) { DeleteObject(dib); DeleteDC(dc); return false; }
    auto *bgra = static_cast<unsigned char *>(pixels);
    std::fill(bgra, bgra + bytes, static_cast<unsigned char>(0));
    const bool drawn = DrawIconEx(dc, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL) != FALSE;
    GdiFlush();
    std::copy(bgra, bgra + bytes, rgba.begin());
    bool alpha = false;
    for (size_t i = 3; i < bytes; i += 4) alpha = alpha || rgba[i] != 0;
    bool maskDrawn = true;
    if (!alpha) {
        maskDrawn = DrawIconEx(dc, 0, 0, icon, width, height, 0, nullptr, DI_MASK) != FALSE;
        GdiFlush();
    }
    for (size_t i = 0; i < bytes; i += 4) {
        std::swap(rgba[i], rgba[i + 2]);
        if (!alpha) rgba[i + 3] = bgra[i] ? 0 : 255;
        else if (rgba[i + 3]) {
            for (unsigned c = 0; c < 3; ++c)
                rgba[i + c] = static_cast<unsigned char>((std::min)(255u,
                    (unsigned(rgba[i + c]) * 255 + rgba[i + 3] / 2) / rgba[i + 3]));
        }
    }
    SelectObject(dc, previous);
    DeleteObject(dib);
    DeleteDC(dc);
    return drawn && maskDrawn;
}

// The window borrows these handles. Keep them until it is rebound or destroyed;
// never destroy handles owned by SDL or the executable's shared resources.
class WindowIcons {
public:
    WindowIcons() = default;
    WindowIcons(const WindowIcons &) = delete;
    WindowIcons &operator=(const WindowIcons &) = delete;
    ~WindowIcons() { Clear(); }

    bool Apply(HWND window, const char *root, comGameId_t profile, bool inherit)
    {
        if (!window) return false;
        HICON largeIcon = nullptr, smallIcon = nullptr;
        if (inherit) ExtractGameIcons(root, profile, largeIcon, smallIcon);
        const bool inherited = largeIcon || smallIcon;
        HICON displayLarge = largeIcon ? largeIcon : smallIcon;
        HICON displaySmall = smallIcon ? smallIcon : largeIcon;
        if (!inherited) {
            const auto instance = GetModuleHandleW(nullptr);
            displayLarge = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1),
                IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
            displaySmall = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1),
                IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        }
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(displayLarge));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(displaySmall));
        Clear();
        large_ = largeIcon;
        small_ = smallIcon;
        return inherited;
    }

private:
    void Clear()
    {
        if (large_) DestroyIcon(large_);
        if (small_ && small_ != large_) DestroyIcon(small_);
        large_ = small_ = nullptr;
    }
    HICON large_ = nullptr;
    HICON small_ = nullptr;
};
} // namespace fntech3::icon
