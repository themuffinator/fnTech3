/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/game_profile.h"
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

enum qboolean { qfalse, qtrue };
#define MAX_OSPATH 256
static void Q_strncpyz( char *out, const char *in, int size ) {
    assert(size > 0);
    std::snprintf(out, static_cast<size_t>(size), "%s", in);
}
static void Q_strcat( char *out, int size, const char *in ) {
    assert(std::strlen(out) + std::strlen(in) < static_cast<size_t>(size));
    std::strcat(out, in);
}
static int Q_stricmp( const char *a, const char *b ) {
    for (;; ++a, ++b) {
        int delta = std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
        if (delta || !*a) return delta;
    }
}
static qboolean Sys_RegularFileExists( const char *path ) {
    std::error_code error;
    return std::filesystem::is_regular_file(std::filesystem::u8path(path), error) && !error ? qtrue : qfalse;
}
#ifdef _WIN32
#define SYS_STEAM_PATH_SEPARATOR_CHAR '\\'
#define SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR '/'
#else
#define SYS_STEAM_PATH_SEPARATOR_CHAR '/'
#define SYS_STEAM_PATH_ALTERNATE_SEPARATOR_CHAR '\\'
#endif
#include "../code/qcommon/steam_path_shared.h"

namespace fs = std::filesystem;
static void Write(const fs::path &path, const std::string &data) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    assert(out.good());
}
static std::string Manifest(const comGameProfile_t *profile, const std::string &dir) {
    return "\"AppState\" { \"appid\" \"" + std::string(profile->steamAppId)
        + "\" \"installdir\" \"" + dir + "\" \"nested\" { \"ignored\" \"value\" } }";
}
static void Install(const fs::path &library, const comGameProfile_t *profile, const std::string &dir) {
    Write(library / "steamapps" / "common" / dir / profile->baseGame / profile->basePakName, "synthetic marker");
    Write(library / "steamapps" / ("appmanifest_" + std::string(profile->steamAppId) + ".acf"), Manifest(profile, dir));
}
static std::string Discover(const comGameProfile_t *profile, const fs::path &root) {
    char out[MAX_OSPATH] = "stale output";
    if (!Sys_SteamAppPathFromRoot(profile, root.string().c_str(), out, sizeof(out))) {
        assert(!out[0]);
        return "";
    }
    return fs::path(out).lexically_normal().generic_string();
}
int main(int argc, char **argv) {
    const auto *q3 = Com_GameProfileById(COM_GAME_QUAKE3);
    const auto *ql = Com_GameProfileById(COM_GAME_QUAKELIVE);
    assert(!std::strcmp(q3->steamAppId, "2200"));
    assert(!std::strcmp(ql->steamAppId, "282440"));
    assert(!q3->usesSteam && ql->usesSteam);
    if (argc == 2) {
        for (const auto *profile : {q3, ql}) {
            std::string result = Discover(profile, fs::u8path(argv[1]));
            assert(!result.empty());
            std::printf("%s: %s\n", profile->key, result.c_str());
        }
        return 0;
    }
    const fs::path root = fs::temp_directory_path() / ("fntech3-steam-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    // This unique, newly created directory contains only test-authored fixtures.
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code error; fs::remove_all(path, error); } } cleanup{root};
    const fs::path library = root / "secondary library";
    const fs::path primary = root / "primary";
    Install(library, q3, "custom q3");
    Install(library, ql, "custom ql");
    const auto q3path = library / "steamapps/common/custom q3";
    const auto qlpath = library / "steamapps/common/custom ql";
    assert(Discover(q3, library) == q3path.generic_string());
    assert(Discover(ql, library) == qlpath.generic_string());
    assert(Discover(q3, root / "missing").empty());
    assert(Discover(nullptr, library).empty());
    const fs::path folders = primary / "steamapps/libraryfolders.vdf";
    for (const auto &data : std::initializer_list<std::string>{
        "\"libraryfolders\" { \"1\" \"" + library.generic_string() + "\" }",
        "// comment\n\"libraryfolders\" { \"0\" { \"apps\" { \"2200\" \"1\" } \"path\" \"" + library.generic_string() + "\" } }",
        "\"libraryfolders\" { \"0\" { \"path\" \"" + library.generic_string() + "\" \"apps\" {} } }"
    }) {
        Write(folders, data);
        assert(Discover(q3, primary) == q3path.generic_string());
        assert(Discover(ql, primary) == qlpath.generic_string());
    }
    for (const auto &data : std::initializer_list<std::string>{
        "\"libraryfolders\" { \"0\" { \"path\" \"" + library.generic_string() + "\" }",
        "\"libraryfolders\" { \"0\" { \"path\" \"" + library.generic_string() + "\" \"path\" \"duplicate\" } }",
        "\"libraryfolders\" { \"0\" \"" + library.generic_string() + "\" } garbage"
    }) {
        Write(folders, data);
        assert(Discover(q3, primary).empty());
        assert(Discover(ql, primary).empty());
    }
    for (const auto *profile : {q3, ql}) {
        const fs::path invalid = root / profile->key;
        const fs::path manifest = invalid / "steamapps" / ("appmanifest_" + std::string(profile->steamAppId) + ".acf");
        char dir[MAX_OSPATH];
        for (const char *name : {"..", ".", "../escape", "/absolute", "C:\\\\escape", "bad.", "bad ", "bad:name", "bad?name"}) {
            Write(manifest, Manifest(profile, name));
            assert(!Sys_SteamManifestInstallDir(profile, invalid.string().c_str(), dir, sizeof(dir)));
            assert(!dir[0]);
        }
        for (const auto &data : std::initializer_list<std::string>{
            Manifest(profile, std::string(MAX_OSPATH + 1, 'x')),
            Manifest(profile, "good") + " trailing",
            "\"AppState\" { \"appid\" \"999\" \"installdir\" \"good\" }",
            "\"AppState\" { \"appid\" \"" + std::string(profile->steamAppId) + "\" \"installdir\" \"unterminated",
            "\"AppState\" { \"appid\" \"" + std::string(profile->steamAppId) + "\" \"installdir\" \"good\" \"installdir\" \"second\" }",
            Manifest(profile, "good") + std::string(1, '\0'),
            std::string(1024 * 1024 + 1, 'x')
        }) {
            Write(manifest, data);
            assert(!Sys_SteamManifestInstallDir(profile, invalid.string().c_str(), dir, sizeof(dir)));
        }
        const fs::path fallback = invalid / "steamapps/common" / profile->steamInstallName;
        Write(fallback / "baseq3/default.cfg", "not game identity");
        assert(Discover(profile, invalid).empty());
        Write(fallback / "baseq3" / profile->basePakName, "synthetic");
        assert(Discover(profile, invalid) == fallback.generic_string());
    }
    char small[8] = "stale";
    assert(!Sys_SteamJoinPath(small, sizeof(small), "1234567", "x"));
    assert(!small[0]);
    assert(!Sys_CopyValidSteamInstall(q3, q3path.string().c_str(), small, sizeof(small)));
    char slash[] = "/";
    Sys_NormalizeSteamPath(slash);
    assert(slash[0] && !slash[1]);
    const auto q3str = q3path.string(), qlstr = qlpath.string();
    comGameDetectionInputs_t input = {"auto", nullptr, nullptr, q3str.c_str(), qlstr.c_str()};
    assert(Com_DetectGameProfile(&input).status == COM_GAME_DETECTION_AMBIGUOUS);
    input.basePath = q3str.c_str();
    assert(Com_DetectGameProfile(&input).profile == q3);
    input.basePath = nullptr;
    input.qlSteamPath = nullptr;
    assert(Com_DetectGameProfile(&input).profile == q3);
    input.q3SteamPath = nullptr;
    input.qlSteamPath = qlstr.c_str();
#ifndef FNTECH3_Q3_ONLY
    assert(Com_DetectGameProfile(&input).profile == ql);
#endif
    assert(!Com_GameProfile()); // Discovery never activates a profile or services.
    return 0;
}
