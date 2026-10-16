// Controlled I/O fixture for the production filesystem's FNZ read path.
#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include "code/qcommon/game_profile.h"

using byte = unsigned char;
using qboolean = bool;
constexpr bool qfalse = false, qtrue = true;
#define MAX_ZPATH 256
#define MAX_OSPATH 1024
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define S_COLOR_YELLOW ""
#define Com_sprintf std::snprintf
struct Cvar { const char *string = ""; int integer = 0; } debug, game, basepath;
Cvar *fs_debug = &debug, *fs_gamedirvar = &game, *fs_basepath = &basepath, *fs_apppath = nullptr;
const char *basegame = nullptr;
struct Pack { const char *pakGamename; };
struct Dir { const char *gamedir; };
struct searchpath_t { Pack *pack; Dir *dir; searchpath_t *next; };
searchpath_t *fs_searchpaths = nullptr;
int fs_loadCount = 0, fs_loadStack = 0, allocations = 0, handles = 0;
int Q_stricmp(const char *a, const char *b) {
    for (; *a && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b); ++a, ++b) {}
    return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
int Q_stricmpn(const char *a, const char *b, int count) {
    return Q_stricmp(std::string(a).substr(0, count).c_str(), std::string(b).substr(0, count).c_str());
}
int FS_FilenameCompare(const char *a, const char *b) { return Q_stricmp(a, b); }
bool COM_CompareExtension(const char *path, const char *ext) {
    size_t a = strlen(path), b = strlen(ext);
    return a >= b && !Q_stricmp(path + a - b, ext);
}
void Q_strncpyz(char *out, const char *in, int size) { snprintf(out, size, "%s", in); }
void Com_Printf(const char *, ...) {}
const char *Sys_Pwd() { return "/app"; }
const char *Sys_DefaultAppPath() { return "/app"; }
const char *FS_BuildOSPath(const char *base, const char *name, const char *) {
    static std::string path;
    path = std::string(base) + "/" + name;
    return path.c_str();
}
byte *Hunk_AllocateTempMemory(int n) { ++allocations; return new byte[n]; }
void Hunk_FreeTempMemory(void *p) { --allocations; delete[] static_cast<byte *>(p); }

// INSERT_CRC

using Entries = std::map<std::string, std::string>;
std::map<std::string, Entries> archives;
struct Zip { Entries *entries; const std::string *payload = nullptr; size_t position = 0; };
using unzFile = Zip *;
struct unz_file_info { unsigned long compression_method, uncompressed_size, crc; };
constexpr int UNZ_OK = 0;
bool corruptCRC = false, truncated = false;
unsigned long compression = 8;
unzFile unzOpen(const char *path) {
    auto found = archives.find(path);
    if (found == archives.end()) return nullptr;
    ++handles;
    return new Zip{&found->second};
}
int unzClose(unzFile zip) { --handles; delete zip; return 0; }
int unzGoToFirstFile(unzFile) { return 0; }
int unzLocateFile(unzFile zip, const char *name, int) {
    for (const auto &entry : *zip->entries) if (!Q_stricmp(entry.first.c_str(), name)) {
        zip->payload = &entry.second;
        return 0;
    }
    return -1;
}
int unzGetCurrentFileInfo(unzFile zip, unz_file_info *info, void *, int, void *, int, void *, int) {
    assert(zip->payload);
    info->compression_method = compression;
    info->uncompressed_size = (unsigned long)zip->payload->size();
    info->crc = crc32_buffer(reinterpret_cast<const byte *>(zip->payload->data()), (unsigned)zip->payload->size());
    if (corruptCRC) info->crc ^= 1;
    return 0;
}
int unzOpenCurrentFile(unzFile zip) { zip->position = 0; return 0; }
int unzCloseCurrentFile(unzFile) { return 0; }
int unzReadCurrentFile(unzFile zip, byte *out, int count) {
    if (truncated) return 0;
    const size_t size = std::min({(size_t)count, zip->payload->size() - zip->position, (size_t)3});
    memcpy(out, zip->payload->data() + zip->position, size);
    zip->position += size;
    return (int)size;
}

// INSERT_READER

void expectRead(const char *path, const char *expected) {
    void *buffer = reinterpret_cast<void *>(1);
    const int length = FS_ReadFileFromRootArchive(path, &buffer);
    if (expected) {
        assert(length == (int)strlen(expected) && buffer && !strcmp((const char *)buffer, expected));
        assert(FS_ReadFileFromRootArchive(path, nullptr) == length);
        Hunk_FreeTempMemory(buffer);
        --fs_loadStack;
    } else {
        assert(length == -1 && buffer == nullptr);
    }
    assert(allocations == 0 && handles == 0 && fs_loadStack == 0);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    char candidates[64][MAX_ZPATH];
    // Partial initialization has no implicit Q3/QL namespace.
    assert(FS_RootArchiveBuildCandidates("maps/shared.azb", candidates, 64) == 0);
    expectRead("maps/shared.azb", nullptr);
    assert(Com_ActivateGameProfile((comGameId_t)atoi(argv[1])));
    const auto *profile = Com_GameProfile();
    assert(!strcmp(profile->key, argv[2]));
    const std::string prefix = std::string("games/") + argv[2] + "/";
    const std::string base = prefix + profile->baseGame + "/";
    const std::string appArchive = "/app/fntech3-pkg.fnz", installArchive = "/install/fntech3-pkg.fnz";
    basepath.string = "/install";
    game.string = "mod";
    basegame = profile->baseGame;
    Pack modPack{"mod"};
    Dir baseDir{profile->baseGame};
    searchpath_t tail{nullptr, &baseDir, nullptr}, head{&modPack, nullptr, &tail};
    fs_searchpaths = &head;
    assert(FS_RootArchiveBuildCandidates("maps/shared.azb", candidates, 64) == 2);
    assert(candidates[0] == prefix + "mod/maps/shared.azb");
    assert(candidates[1] == base + "maps/shared.azb");
    // All titles/modes coexist with identical map names and legacy decoys.
    for (int id = 1; id < COM_GAME_COUNT; ++id) {
        const auto *p = Com_GameProfileById((comGameId_t)id);
        archives[appArchive][std::string("games/") + p->key + "/" + p->baseGame + "/maps/shared.azb"] = p->key;
    }
    archives[appArchive]["baseq3/maps/shared.azb"] = "legacy";
    archives[appArchive]["maps/shared.azb"] = "unscoped";
    expectRead("maps/shared.azb", argv[2]);
    archives[appArchive][prefix + "mod/maps/shared.azb"] = "mod override";
    expectRead("maps/shared.azb", "mod override");
    // Restarting into another mod must not retain the previous mod's entry.
    modPack.pakGamename = game.string = "othermod";
    expectRead("maps/shared.azb", argv[2]);
    archives[appArchive].erase(base + "maps/shared.azb");
    expectRead("maps/shared.azb", nullptr); // signals ordinary filesystem fallback
    archives[installArchive][base + "maps/shared.azb"] = "installation fallback";
    expectRead("maps/shared.azb", "installation fallback");
    archives[appArchive][base + "maps/shared.azb"] = "adjacent package";
    expectRead("maps/shared.azb", "adjacent package");
    expectRead("MAPS/SHARED.AZB", "adjacent package");
    archives[appArchive][base + "maps/shared.fog"] = "fog";
    archives[appArchive][base + "maps/shared.lights.json"] = "static lights";
    archives[appArchive][base + "sound/fnql-weapon-sounds.sndshd"] = "sound";
    archives[appArchive][base + "scripts/fnql.shader"] = "shader";
    expectRead("maps/shared.fog", "fog");
    expectRead("maps/shared.lights.json", "static lights");
    assert(!FS_RootArchiveAllowsFile("shared.lights.json"));
    expectRead("sound/fnql-weapon-sounds.sndshd", "sound");
    expectRead("scripts/fnql.shader", "shader");
    for (const char *path : {"fnq3-hud.json", "fntech3-hud.json"}) {
        archives[appArchive][base + path] = "profile HUD rules";
        const bool supported = Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsWolfET() || Com_GameIsSTVEF()
            || Com_GameIsRitual() || Com_GameIsJKAMP() || Com_GameIsJK2MP();
        assert(bool(FS_RootArchiveAllowsFile(path)) == supported);
        assert(FS_ReadFileFromRootArchive(path, nullptr) == (supported ? 17 : -1));
    }
    for (const char *path : {"default.cfg", "vm/cgame.qvm", "scripts/custom.shader", "maps/shared.bsp", "fntech3-hud-dump.json", "custom-hud.json",
                             "maps/shared.json", "maps/shared.lights"}) {
        archives[appArchive][base + path] = "not allowlisted";
        expectRead(path, nullptr);
    }
    for (const char *path : {"maps/../shared.azb", "maps//shared.azb", "maps/./shared.azb", "maps/\\shared.azb",
                             "maps/c:shared.azb", "/maps/shared.azb", "maps/bad\nname.azb", ""}) {
        assert(!FS_RootArchiveAllowsFile(path));
        expectRead(path, nullptr);
    }
    expectRead(nullptr, nullptr);
    for (const char *dir : {"../quakelive/baseq3", "..", "baseq3/", "C:baseq3", "a\\b", "a//b"}) {
        assert(FS_RootArchiveAddGameCandidate(candidates, 64, 0, argv[2], dir, "maps/shared.azb") == 0);
    }
    const std::string longPath(MAX_ZPATH, 'x');
    assert(FS_RootArchiveBuildCandidates(("maps/" + longPath + ".azb").c_str(), candidates, 64) == 0);
    assert(FS_RootArchiveBuildCandidates("maps/shared.azb", candidates, 0) == 0);
    assert(FS_RootArchiveBuildCandidates("maps/shared.azb", candidates, 1) == 1);
    corruptCRC = true; expectRead("maps/shared.azb", nullptr); corruptCRC = false;
    truncated = true; expectRead("maps/shared.azb", nullptr); truncated = false;
    compression = 12; expectRead("maps/shared.azb", nullptr); compression = 8;
    archives.clear();
    archives["/app/FnQL-pkg.fnz"][base + "maps/shared.azb"] = "obsolete filename";
    expectRead("maps/shared.azb", nullptr);
}
