"""Execute profile cache policy and transactional reads against controlled disk I/O."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/qcommon/game_manager.h"
#include "code/client/client_cpp.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <map>
#include <memory>
#include <string>
#include <vector>
using fnql::FileReadObject;
using fnql::FileWriteObject;
using fnql::OpenHomeFileRead;
using fnql::ScopedFileHandle;
static std::map<std::string, std::vector<byte>> files;
static std::vector<byte> *current;
static size_t position;
static int opens, closes, reads, writes, warnings, badRead, badWrite, lengthDelta;
static bool throwRead, throwWrite, missingHandle;
static const comGameProfile_t *cacheProfile;
#define Com_GameProfile() cacheProfile
void QDECL Com_Printf(const char *, ...) { ++warnings; }
int FS_Home_FOpenFileRead(const char *name, fileHandle_t *handle) {
    assert(!current); ++opens; *handle = FS_INVALID_HANDLE;
    const auto found = files.find(name);
    if (found == files.end()) return -1;
    if (!missingHandle) { current = &found->second; position = 0; *handle = 1; }
    return int(found->second.size()) + lengthDelta;
}
fileHandle_t FS_FOpenFileWrite(const char *name) {
    assert(!current); ++opens;
    if (missingHandle) return FS_INVALID_HANDLE;
    current = &files[name]; current->clear(); position = 0; return 1;
}
void FS_FCloseFile(fileHandle_t handle) { assert(handle == 1 && current); ++closes; current = nullptr; }
int FS_Read(void *out, int bytes, fileHandle_t handle) {
    assert(handle == 1 && current && bytes >= 0); ++reads;
    if (reads == badRead && throwRead) throw 1;
    int count = std::min(bytes, int(current->size() - position));
    if (reads == badRead && count) --count;
    std::memcpy(out, current->data() + position, count); position += count; return count;
}
int FS_Write(const void *in, int bytes, fileHandle_t handle) {
    assert(handle == 1 && current && bytes >= 0); ++writes;
    if (writes == badWrite && throwWrite) throw 2;
    const int count = bytes - (writes == badWrite && bytes ? 1 : 0);
    const auto *data = static_cast<const byte *>(in); current->insert(current->end(), data, data + count);
    return count;
}
static void ResetIO() {
    assert(!current); opens = closes = reads = writes = warnings = badRead = badWrite = lengthDelta = 0;
    throwRead = throwWrite = missingHandle = false;
}
'''

CHECKS = r'''
static void SetInt(std::vector<byte> &bytes, size_t offset, int value) {
    assert(offset + sizeof(value) <= bytes.size()); std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
static void Populate() {
    std::memset(&cls, 0, sizeof(cls)); cls.numglobalservers = 2; cls.numfavoriteservers = 1; cls.numGlobalServerAddresses = 19;
    auto &server = cls.globalServers[0]; server.adr.type = NA_IP; server.adr.ipv._4[0] = 127;
    server.adr.port = 27960; server.visible = qtrue; server.ping = 37;
    std::strcpy(server.hostName, "Known server"); std::strcpy(server.mapName, "testmap"); std::strcpy(server.game, "mod");
    cls.favoriteServers[0] = server; cls.favoriteServers[0].ping = -1;
    cls.globalServers[1].adr.type = NA_IP6; cls.globalServers[1].adr.ipv._6[15] = 1;
    cls.globalServers[1].adr.scope_id = 7;
}
static void Reject(const std::vector<byte> &bytes) {
    Populate(); auto saved = std::make_unique<decltype(cls)>(cls);
    files[cacheProfile->serverCacheFile] = bytes;
    bool threw = false; try { LAN_LoadCachedServers(); } catch (int e) { assert(e == 1 && throwRead); threw = true; }
    assert(threw == throwRead && !current && closes == (missingHandle ? 0 : 1));
    assert(!cls.numglobalservers && !cls.numfavoriteservers && !cls.numGlobalServerAddresses);
    assert(!std::memcmp(cls.globalServers, saved->globalServers, sizeof(cls.globalServers)));
    assert(!std::memcmp(cls.favoriteServers, saved->favoriteServers, sizeof(cls.favoriteServers)));
}
int main() {
    assert(!Com_GameManagerServerCacheFile(nullptr));
    for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        cacheProfile = Com_GameProfileById(id); assert(cacheProfile);
        auto forged = *cacheProfile; assert(!Com_GameManagerServerCacheFile(&forged));
        Populate(); ResetIO();
        if (id == COM_GAME_RTCW_SP) {
            assert(!cacheProfile->serverCacheFile);
            const auto disk = files; auto saved = std::make_unique<decltype(cls)>(cls);
            LAN_LoadCachedServers(); LAN_SaveServersToCache();
            assert(!opens && !closes && !reads && !writes && !std::memcmp(&cls, saved.get(), sizeof(cls)) && disk == files);
            continue;
        }
        const char *name = Com_GameManagerServerCacheFile(cacheProfile);
        assert(!std::strcmp(name, id == COM_GAME_RTCW_MP ? "fntech3-rtcw-mp-servercache.dat" : "servercache.dat"));
        const std::string other = id == COM_GAME_RTCW_MP ? "servercache.dat" : "fntech3-rtcw-mp-servercache.dat";
        files[other] = {1,2,3,4}; LAN_SaveServersToCache();
        assert(opens == 1 && closes == 1 && writes == 3 && !warnings && files[other] == std::vector<byte>({1,2,3,4}));
        auto good = files[name];
        const int header[] = {2,1,sizeof(cls.globalServers) + sizeof(cls.favoriteServers)};
        assert(good.size() == sizeof(header) + header[2]);
        assert(!std::memcmp(good.data(), header, sizeof(header)));
        assert(!std::memcmp(good.data() + sizeof(header), cls.globalServers, sizeof(cls.globalServers)));
        assert(!std::memcmp(good.data() + sizeof(header) + sizeof(cls.globalServers), cls.favoriteServers, sizeof(cls.favoriteServers)));
        auto saved = std::make_unique<decltype(cls)>(cls);
        std::memset(&cls, 0, sizeof(cls)); ResetIO(); LAN_LoadCachedServers();
        assert(opens == 1 && closes == 1 && reads == 3 && cls.numglobalservers == 2 && cls.numfavoriteservers == 1);
        assert(!cls.numGlobalServerAddresses && !std::memcmp(cls.globalServers, saved->globalServers, sizeof(cls.globalServers)));
        assert(!std::memcmp(cls.favoriteServers, saved->favoriteServers, sizeof(cls.favoriteServers)));
        for (size_t bytes : {size_t(0), size_t(1), size_t(11), size_t(12), good.size()/2, good.size()-1, good.size()+1}) {
            auto bad = good; bad.resize(bytes); ResetIO(); Reject(bad);
        }
        for (int index : {0,1,2}) for (int value : {-1, INT_MIN, INT_MAX}) {
            auto bad = good; SetInt(bad, index * sizeof(int), value); ResetIO(); Reject(bad);
        }
        for (int index : {0,1}) {
            auto bad = good; SetInt(bad, index * sizeof(int), (index ? MAX_OTHER_SERVERS : MAX_GLOBAL_SERVERS) + 1);
            ResetIO(); Reject(bad);
        }
        const size_t first = sizeof(header), last = good.size() - sizeof(serverInfo_t);
        for (size_t record : {first, last}) {
            for (size_t offset : {offsetof(serverInfo_t,hostName), offsetof(serverInfo_t,mapName), offsetof(serverInfo_t,game)}) {
                auto bad = good; std::memset(bad.data() + record + offset, 'x', MAX_NAME_LENGTH); ResetIO(); Reject(bad);
            }
            for (int value : {INT_MIN, INT_MAX}) {
                auto bad = good; SetInt(bad, record + offsetof(serverInfo_t,adr) + offsetof(netadr_t,type), value);
                ResetIO(); Reject(bad);
            }
            for (int value : {-1,2,INT_MAX}) {
                auto bad = good; SetInt(bad, record + offsetof(serverInfo_t,visible), value); ResetIO(); Reject(bad);
            }
        }
        for (int call : {1,2,3}) for (bool throws : {false,true}) {
            ResetIO(); badRead = call; throwRead = throws; Reject(good); assert(reads == call);
        }
        ResetIO(); missingHandle = true; Reject(good); assert(!reads);
        ResetIO(); lengthDelta = -1; Reject(good); assert(!reads);
        // A file truncated after open still has to pass every read count.
        ResetIO(); auto truncated = good; truncated.pop_back(); lengthDelta = 1; Reject(truncated);
        for (int call : {1,2,3}) for (bool throws : {false,true}) {
            Populate(); ResetIO(); badWrite = call; throwWrite = throws;
            bool threw = false; try { LAN_SaveServersToCache(); } catch (int e) { assert(e == 2); threw = true; }
            assert(threw == throws && !current && closes == 1 && writes == call && warnings == (throws ? 0 : 1));
        }
        Populate(); ResetIO(); missingHandle = true; LAN_SaveServersToCache(); assert(opens == 1 && !closes && !writes);
        for (int value : {-2, INT_MIN, MAX_GLOBAL_SERVERS + 1, INT_MAX}) {
            Populate(); ResetIO(); cls.numglobalservers = value; LAN_SaveServersToCache(); assert(!opens);
        }
        Populate(); ResetIO(); cls.numglobalservers = -1; LAN_SaveServersToCache();
        int count; std::memcpy(&count, files[name].data(), sizeof(count)); assert(!count && cls.numglobalservers == -1);
        Populate(); ResetIO(); cls.numglobalservers = MAX_GLOBAL_SERVERS; cls.numfavoriteservers = MAX_OTHER_SERVERS;
        LAN_SaveServersToCache(); cls.numglobalservers = cls.numfavoriteservers = 0; ResetIO(); LAN_LoadCachedServers();
        assert(cls.numglobalservers == MAX_GLOBAL_SERVERS && cls.numfavoriteservers == MAX_OTHER_SERVERS);
        Populate(); ResetIO(); std::memset(cls.favoriteServers[0].game, 'x', MAX_NAME_LENGTH);
        LAN_SaveServersToCache(); assert(!opens);
        Populate(); ResetIO(); files.erase(name); LAN_LoadCachedServers();
        assert(opens == 1 && !closes && !reads && !cls.numglobalservers && !cls.numfavoriteservers);
    }
}
'''


class ServerCacheTests(unittest.TestCase):
    def test_profile_policy_layout_bounds_and_partial_io(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/cl_ui.cpp').read_text()
        record = (ROOT / 'code/client/client.h').read_text()
        start = record.index('struct serverInfo_t {')
        record = record[start:record.index('\n};', start) + 3]
        body = PRELUDE + record + r'''
static struct {
    serverInfo_t globalServers[MAX_GLOBAL_SERVERS], favoriteServers[MAX_OTHER_SERVERS];
    int numglobalservers, numfavoriteservers, numGlobalServerAddresses;
} cls;
'''
        for signature in ['static bool LAN_CacheRecordValid(', 'static void LAN_LoadCachedServers(', 'static void LAN_SaveServersToCache(']:
            body += function(source, signature) + '\n'
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='server-cache-', dir=ROOT / '.tmp') as temporary:
            fixture = Path(temporary) / 'cache.cpp'; fixture.write_text(body + CHECKS)
            executable = Path(temporary) / ('cache.exe' if os.name == 'nt' else 'cache')
            flags = compiler_target_args(compiler)
            if os.name == 'nt' and not flags:
                flags += ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, '-std=c++17', *flags, '-I', str(ROOT), str(fixture),
                str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
