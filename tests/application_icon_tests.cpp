// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#include "../code/platform/game_icon.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include "../code/win32/win_game_icon.h"
#endif
#ifdef FNTECH3_ICON_TEST_SDL
static void Com_DPrintf(const char *, ...) {}
#include "../code/sdl/sdl_icon.h"
#endif

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "line %d: %s\n", __LINE__, #expression); std::exit(1); \
} } while (0)
namespace fs = std::filesystem;
using namespace fntech3::icon;

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    const auto work = fs::path(argv[1]) / ("icon-test-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto root = work / fs::u8path("game-\xc3\xa9");
    fs::create_directories(root);
    const auto rootName = root.u8string();
    CHECK(!Candidate(COM_GAME_AUTO, Source::Executable, 0));
    CHECK(!Candidate(COM_GAME_COUNT, Source::Executable, 0));
    CHECK(!Candidate(COM_GAME_QUAKE3, Source::Executable, 999));
    CHECK(Resolve(nullptr, COM_GAME_QUAKE3, Source::Executable, 0).empty());
    CHECK(Resolve("", COM_GAME_QUAKE3, Source::Executable, 0).empty());
    CHECK(Resolve(rootName.c_str(), COM_GAME_QUAKE3, Source::Executable, 0).empty());
    std::ofstream(root / "quake3.exe") << "not an executable";
    CHECK(!Resolve(rootName.c_str(), COM_GAME_QUAKE3, Source::Executable, 0).empty());
    CHECK(Resolve(rootName.c_str(), COM_GAME_QUAKELIVE, Source::Executable, 0).empty());
    CHECK(Resolve(rootName.c_str(), COM_GAME_RTCW_SP, Source::Executable, 0).empty());
    CHECK(Resolve(rootName.c_str(), COM_GAME_RTCW_MP, Source::Executable, 0).empty());
    fs::create_directory(root / "quakelive_steam.exe");
    CHECK(Resolve(rootName.c_str(), COM_GAME_QUAKELIVE, Source::Executable, 0).empty());
    fs::remove(root / "quakelive_steam.exe");
    fs::create_directory(work / "outside.app");
    std::ofstream(work / "outside.png") << "external file";
    std::error_code error;
    fs::create_symlink(work / "outside.png", root / "quakelive.png", error);
    if (!error) CHECK(Resolve(rootName.c_str(), COM_GAME_QUAKELIVE, Source::Png, 0).empty());
    else std::puts("SKIP file symlink: host does not permit symlinks");
    error.clear();
    fs::create_directory_symlink(work / "outside.app", root / "Quake3.app", error);
    if (!error) CHECK(Resolve(rootName.c_str(), COM_GAME_QUAKE3, Source::Bundle, 0).empty());
    else std::puts("SKIP directory symlink: host does not permit symlinks");

    unsigned char png[33] = {137,80,78,71,13,10,26,10, 0,0,0,13,'I','H','D','R',
                            0,0,0,32, 0,0,0,32, 8,6};
    CHECK(ReasonablePng(png, sizeof(png)));
    CHECK(!ReasonablePng(png, 32));
    CHECK(!ReasonablePng(png, 4 * 1024 * 1024 + 1));
    png[16] = 0x7f;
    CHECK(!ReasonablePng(png, sizeof(png)));
    png[16] = png[19] = 0;
    CHECK(!ReasonablePng(png, sizeof(png)));
    png[19] = 32;
    png[12] = 'X';
    CHECK(!ReasonablePng(png, sizeof(png)));

#ifdef _WIN32
    HICON largeIcon = nullptr, smallIcon = nullptr;
    ExtractGameIcons(rootName.c_str(), COM_GAME_QUAKE3, largeIcon, smallIcon);
    CHECK(!largeIcon && !smallIcon); // corrupt executable must fall back
    // The test's own PE has the fnTech3 resource: no retail asset is copied.
    fs::copy_file(fs::u8path(argv[0]), root / "quake3.exe", fs::copy_options::overwrite_existing);
    ExtractGameIcons(rootName.c_str(), COM_GAME_QUAKE3, largeIcon, smallIcon);
    CHECK(largeIcon && smallIcon);
    std::vector<unsigned char> pixels;
    int width = 0, height = 0;
    CHECK(CopyIconPixels(largeIcon, pixels, width, height));
    bool transparent = false, opaque = false, orange = false, black = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        transparent |= pixels[i + 3] == 0;
        opaque |= pixels[i + 3] == 255;
        orange |= pixels[i] > 200 && pixels[i + 1] > 30 && pixels[i + 2] < 40 && pixels[i + 3] > 200;
        black |= pixels[i] < 20 && pixels[i + 1] < 20 && pixels[i + 2] < 20 && pixels[i + 3] > 200;
    }
    CHECK(transparent && opaque && orange && black);
    DestroyIcon(largeIcon); DestroyIcon(smallIcon);
    unsigned char andMask[32], xorMask[32]{};
    std::memset(andMask, 255, sizeof(andMask));
    andMask[0] = 0x3f; // two opaque black pixels, otherwise transparent
    HICON legacy = CreateIcon(nullptr, 16, 16, 1, 1, andMask, xorMask);
    CHECK(legacy && CopyIconPixels(legacy, pixels, width, height));
    CHECK(pixels[3] == 255 && pixels[7] == 255 && pixels[11] == 0);
    DestroyIcon(legacy);
    HWND window = CreateWindowExW(0, L"STATIC", L"fnTech3 icon test", 0,
        0, 0, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    CHECK(window && !IsWindowVisible(window));
    {
        WindowIcons icons;
        // Warm the process-wide LR_SHARED resource cache before counting.
        CHECK(!icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, false));
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, true));
        CHECK(SendMessageW(window, WM_GETICON, ICON_BIG, 0));
        CHECK(SendMessageW(window, WM_GETICON, ICON_SMALL, 0));
        const DWORD baseline = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
        for (int i = 0; i < 30; ++i) {
            CHECK(!icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, false));
            CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, true));
        }
        CHECK(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) <= baseline + 1);
        CHECK(!icons.Apply(window, rootName.c_str(), COM_GAME_QUAKELIVE, true));
        CHECK(SendMessageW(window, WM_GETICON, ICON_BIG, 0)); // embedded fallback
        CHECK(!icons.Apply(nullptr, rootName.c_str(), COM_GAME_QUAKE3, true));
        DestroyWindow(window);
    }
#endif

#ifdef FNTECH3_ICON_TEST_SDL
#if SDL_VERSION_ATLEAST(3, 4, 0)
    fs::copy_file(fs::u8path(argv[2]), root / "quake3.png");
    auto image = LoadGamePng(rootName.c_str(), COM_GAME_QUAKE3);
    CHECK(image && image->w == 128 && image->h == 128);
    image.reset();
    CHECK(!LoadGamePng(rootName.c_str(), COM_GAME_RTCW_MP));
    { std::ofstream file(root / "quake3.png", std::ios::binary); file.write(reinterpret_cast<char *>(png), sizeof(png)); }
    CHECK(!LoadGamePng(rootName.c_str(), COM_GAME_QUAKE3));
#endif
#ifdef _WIN32
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *sdlWindow = SDL_CreateWindow("fnTech3 icon test", 32, 32, SDL_WINDOW_HIDDEN);
    CHECK(sdlWindow);
    for (int i = 0; i < 10; ++i) {
        SetWindowIcon(sdlWindow, rootName.c_str(), COM_GAME_QUAKE3, true);
        HWND native = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        CHECK(native && !IsWindowVisible(native));
        CHECK(SendMessageW(native, WM_GETICON, ICON_BIG, 0));
        SetWindowIcon(sdlWindow, rootName.c_str(), COM_GAME_QUAKE3, false);
        CHECK(SendMessageW(native, WM_GETICON, ICON_BIG, 0));
    }
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
#endif
#endif
    fs::remove_all(work);
    std::puts("Application icon tests passed");
    return 0;
}
