// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#include "../code/platform/game_icon.hpp"
#include "../code/platform/game_icon_builtin.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <iterator>
#include <set>
#include <string>
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

static std::vector<unsigned char> ReadFile(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
}

// Every profile in the catalogue has a remake; its embedded frames decode and
// are byte-identical to the committed PNGs they were generated from.
static void CheckBuiltinCatalogue(const fs::path &games)
{
    std::set<std::string> keys;
    std::set<unsigned> resources;
    for (const BuiltinIcon &icon : kBuiltinIcons) {
        CHECK(keys.insert(icon.profile).second);
        CHECK(resources.insert(icon.resource).second);
        CHECK(icon.resource > 100 && icon.resource < 200);
        CHECK(FindBuiltinIcon(icon.profile) == &icon);
    }
    for (unsigned id = COM_GAME_AUTO + 1; id < COM_GAME_COUNT; ++id) {
        const comGameProfile_t *profile = Com_GameProfileById(static_cast<comGameId_t>(id));
        CHECK(profile && profile->key);
        CHECK(FindBuiltinIcon(profile->key));
        CHECK(ApplicationId(profile->key, Mode::BuiltIn) == std::string("fntech3-") + profile->key);
        CHECK(ApplicationId(profile->key, Mode::Installation) == std::string("fntech3-") + profile->key);
        CHECK(ApplicationId(profile->key, Mode::Engine) == "fntech3");
    }
    CHECK(!FindBuiltinIcon(nullptr) && !FindBuiltinIcon("") && !FindBuiltinIcon("quake"));
    CHECK(ApplicationId(nullptr, Mode::BuiltIn) == "fntech3");
    CHECK(ApplicationId("../quake3", Mode::BuiltIn) == "fntech3");

    size_t count = 0;
    const EncodedImage *embedded = EmbeddedImages(count);
    const size_t sizes = sizeof(kEmbeddedIconSizes) / sizeof(kEmbeddedIconSizes[0]);
    CHECK(embedded && count == keys.size() * sizes);
    for (size_t i = 0; i < count; ++i) {
        const EncodedImage &frame = embedded[i];
        CHECK(keys.count(frame.profile));
        const fs::path png = games / frame.profile / (std::string(frame.profile) + "-" +
            std::to_string(frame.size) + ".png");
        const std::vector<unsigned char> committed = ReadFile(png);
        CHECK(committed.size() == frame.length && !std::memcmp(committed.data(), frame.png, frame.length));
        Image image;
        CHECK(DecodePng(frame.png, frame.length, image));
        CHECK(image.width == frame.size && image.height == frame.size);
        CHECK(image.rgba.size() == size_t(frame.size) * frame.size * 4);
        unsigned minAlpha = 255, maxAlpha = 0;
        for (size_t p = 3; p < image.rgba.size(); p += 4) {
            if (image.rgba[p] < minAlpha) minAlpha = image.rgba[p];
            if (image.rgba[p] > maxAlpha) maxAlpha = image.rgba[p];
        }
        // Thin strokes and subpixel margins need not hit exact 0/255 after
        // downsampling. Cutouts still have clear space and strong coverage;
        // the original square background designs may instead be all opaque.
        CHECK(maxAlpha >= 224 && (minAlpha <= 32 || minAlpha == 255));
    }
#ifndef _WIN32
    for (const BuiltinIcon &icon : kBuiltinIcons) {
        const std::vector<Image> decoded = DecodeBuiltinImages(icon.profile);
        CHECK(decoded.size() == sizes);
        for (size_t i = 0; i < decoded.size(); ++i) CHECK(decoded[i].width == kEmbeddedIconSizes[i]);
    }
#endif
}

static void CheckPngDecoder(const fs::path &games)
{
    std::vector<unsigned char> png = ReadFile(games / "quake3" / "quake3-32.png");
    Image image;
    CHECK(DecodePng(png.data(), png.size(), image) && image.width == 32);
    CHECK(!DecodePng(nullptr, 0, image) && image.rgba.empty());
    CHECK(!DecodePng(png.data(), 7, image));
    for (size_t cut : {size_t(16), size_t(40), png.size() / 2, png.size() - 13})
        CHECK(!DecodePng(png.data(), cut, image) && image.width == 0);
    std::vector<unsigned char> damaged = png;
    damaged[25] = 2; // colour type: RGB is not what the generator writes
    CHECK(!DecodePng(damaged.data(), damaged.size(), image));
    damaged = png;
    damaged[16] = 0x7f; // width far beyond the icon limit
    CHECK(!DecodePng(damaged.data(), damaged.size(), image));
    // The generator writes IDAT straight after IHDR: its data starts at 41.
    const size_t idat = 8 + 12 + 13;
    CHECK(!std::memcmp(png.data() + idat + 4, "IDAT", 4));
    damaged = png;
    damaged[idat + 8 + 28] ^= 0x55; // inside the compressed image data
    CHECK(!DecodePng(damaged.data(), damaged.size(), image));
    damaged = png;
    damaged[idat + 8 + 1] ^= 0x01; // zlib check bits
    CHECK(!DecodePng(damaged.data(), damaged.size(), image));
    CHECK(ModeFromCvar(0) == Mode::Engine && ModeFromCvar(-3) == Mode::Engine);
    CHECK(ModeFromCvar(1) == Mode::BuiltIn && ModeFromCvar(7) == Mode::BuiltIn);
    CHECK(ModeFromCvar(2) == Mode::Installation);
}

int main(int argc, char **argv)
{
    CHECK(argc == 4);
    const fs::path games = fs::u8path(argv[3]);
    CheckBuiltinCatalogue(games);
    CheckPngDecoder(games);
    const auto work = fs::path(argv[1]) / ("icon-test-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto root = work / fs::u8path("game-\xc3\xa9");
    fs::create_directories(root);
    const auto rootName = root.u8string();
    CHECK(!Candidate(COM_GAME_AUTO, Source::Executable, 0));
    CHECK(!Candidate(COM_GAME_COUNT, Source::Executable, 0));
    CHECK(!Candidate(COM_GAME_QUAKE3, Source::Executable, 999));
    CHECK(!std::strcmp(Candidate(COM_GAME_QUAKE3_IHV, Source::Executable, 0), "quake3.exe"));
    CHECK(!Candidate(COM_GAME_QUAKE3_IHV, Source::Png, 0));
    CHECK(!Candidate(COM_GAME_QUAKE3_IHV, Source::Bundle, 0));
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
    // The client's resources carry every remake as exact PNG frames.
    for (const BuiltinIcon &icon : kBuiltinIcons) {
        const std::vector<EncodedImage> frames = BuiltinImages(icon.profile);
        CHECK(frames.size() == 10);
        std::set<unsigned> seen;
        for (const EncodedImage &frame : frames) {
            Image image;
            CHECK(DecodePng(frame.png, frame.length, image) && image.width == frame.size);
            seen.insert(frame.size);
        }
        CHECK(seen == std::set<unsigned>({16, 20, 24, 32, 40, 48, 64, 96, 128, 256}));
        HICON largeIcon = nullptr, smallIcon = nullptr;
        CHECK(LoadResourceIcons(nullptr, icon.resource, largeIcon, smallIcon));
        CHECK(largeIcon && smallIcon);
    }
    CHECK(BuiltinImages("unknown").empty());

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
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Engine) == Shown::Engine);
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::BuiltIn) == Shown::BuiltIn);
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Installation) ==
              Shown::Installation);
        CHECK(SendMessageW(window, WM_GETICON, ICON_BIG, 0));
        CHECK(SendMessageW(window, WM_GETICON, ICON_SMALL, 0));
        const DWORD baseline = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
        for (int i = 0; i < 30; ++i) {
            CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Engine) == Shown::Engine);
            CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Installation) ==
                  Shown::Installation);
            CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::BuiltIn) == Shown::BuiltIn);
        }
        CHECK(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) <= baseline + 1);
        // No Quake Live executable here: the installation mode falls back to
        // the remake, and an unknown profile to the fnTech3 icon.
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_QUAKELIVE, "quakelive", Mode::Installation) ==
              Shown::BuiltIn);
        CHECK(icons.Apply(window, rootName.c_str(), COM_GAME_AUTO, nullptr, Mode::BuiltIn) == Shown::Engine);
        CHECK(SendMessageW(window, WM_GETICON, ICON_BIG, 0)); // embedded fallback
        CHECK(icons.Apply(nullptr, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::BuiltIn) == Shown::None);
        DestroyWindow(window);
    }
#endif

#ifdef FNTECH3_ICON_TEST_SDL
    {
        auto builtin = LoadBuiltinIcon("rtcw-sp");
        CHECK(builtin && builtin->w == 128 && builtin->h == 128);
        int images = 0;
        SDL_Surface **all = SDL_GetSurfaceImages(builtin.get(), &images);
        CHECK(all && images >= 4);
        SDL_free(all);
        CHECK(!LoadBuiltinIcon("unknown"));
    }
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
        CHECK(SetWindowIcon(sdlWindow, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Installation) ==
              Shown::Installation);
        HWND native = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        CHECK(native && !IsWindowVisible(native));
        CHECK(SendMessageW(native, WM_GETICON, ICON_BIG, 0));
        CHECK(SetWindowIcon(sdlWindow, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::BuiltIn) == Shown::BuiltIn);
        // The exact resource icons replace SDL's single rescaled bitmap.
        HICON expected = nullptr, expectedSmall = nullptr;
        CHECK(LoadResourceIcons(native, FindBuiltinIcon("quake3")->resource, expected, expectedSmall));
        CHECK(reinterpret_cast<HICON>(SendMessageW(native, WM_GETICON, ICON_BIG, 0)) == expected);
        CHECK(reinterpret_cast<HICON>(SendMessageW(native, WM_GETICON, ICON_SMALL, 0)) == expectedSmall);
        CHECK(SetWindowIcon(sdlWindow, rootName.c_str(), COM_GAME_QUAKE3, "quake3", Mode::Engine) == Shown::Engine);
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
