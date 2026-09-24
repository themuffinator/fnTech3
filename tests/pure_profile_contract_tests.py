"""Run the actual filesystem/client/server pure-proof path with synthetic paks."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "code/qcommon/game_profile.h"
#include "code/qcommon/protocol_contract.hpp"
static netchanWireProfile_t wireProfile = NETCHAN_WIRE_LEGACY_Q3;
static netchanWireProfile_t CL_ActiveWireProfile() { return wireProfile; }
typedef int qboolean;
enum {qfalse, qtrue, ERR_FATAL, ERR_DROP, CS_ZOMBIE, CS_ACTIVE};
#define MAX_STRING_CHARS 1024
#define MAX_QPATH 64
#define ARCH_STRING "x86"
#define DLL_EXT ".dll"
#define S_COLOR_YELLOW ""
#define QL_NATIVE_CGAME_DLL "cgamex86.dll"
#define Com_sprintf snprintf
static bool q3, mp, unavailable, missingUi;
static int uiChecksum = 22;
static bool cl_connectedToPureServer = true;
static struct { int serverId; } cl = {17};
static struct { bool demoplaying; } clc;
static struct { int pure, serverId, checksumFeed, checksumFeedServerId; } sv = {1,17,7,17};
static struct { int time; } svs = {10000};
struct client_t { int gotCP, pureAuthentic, lastSnapshotTime, state; const char *name;
    struct { netchanWireProfile_t wireProfile; } netchan; };
struct pack_t { int referenced, pure_checksum; };
struct searchpath_t { pack_t *pack; const searchpath_t *next; };
static pack_t packs[] = {{FS_CGAME_REF|FS_GENERAL_REF,11}, {FS_UI_REF|FS_GENERAL_REF,22}, {FS_GENERAL_REF,99}};
static searchpath_t paths[] = {{packs,paths+1}, {packs+1,paths+2}, {packs+2,nullptr}};
static const searchpath_t *fs_searchpaths = paths;
static int fs_checksumFeed = 7;
static int drops, snapshots, binaryLookups, reliableCommands;
static std::string sent;
static std::vector<std::string> arguments;
int Com_GameIsQuake3() { return q3; }
const comGameProfile_t *Com_GameProfile() {
    static comGameProfile_t profile{};
    profile.pureProof = unavailable ? COM_PURE_PROOF_UNAVAILABLE : q3 ? COM_PURE_PROOF_QVM :
        mp ? COM_PURE_PROOF_RTCW_NATIVE : COM_PURE_PROOF_QL_NATIVE;
    profile.key = "fixture";
    return &profile;
}
static int Com_PureProofIsNativeModulePair(const comGameProfile_t *profile) {
    return profile && (profile->pureProof == COM_PURE_PROOF_RTCW_NATIVE
        || profile->pureProof == COM_PURE_PROOF_WOLFET_NATIVE);
}
static int Com_GameManagerPureModuleFilename(const comGameProfile_t *, unsigned slot,
    const char *, const char *, char *destination, unsigned capacity) {
    assert(mp && slot < 2);
    const char *name = slot ? "ui_mp_x86.dll" : "cgame_mp_x86.dll";
    const auto length = std::strlen(name);
    assert(length < capacity); std::memcpy(destination, name, length + 1); return 1;
}
static void Com_Printf(const char *, ...) {}
static void Com_DPrintf(const char *, ...) {}
[[noreturn]] static void Com_Error(int, const char *, ...) { throw std::runtime_error("engine error"); }
static char *Q_stradd(char *to, const char *from) {
    const size_t length = std::strlen(from); std::memcpy(to,from,length+1); return to + length;
}
static void Q_strncpyz(char *to, const char *from, int length) {
    assert(length > 0); std::snprintf(to,static_cast<size_t>(length),"%s",from);
}
static const char *va(const char *format, ...) {
    static char text[128]; va_list args;
    va_start(args,format); std::vsnprintf(text,sizeof(text),format,args); va_end(args);
    return text;
}
static bool FS_FileIsInPAK(const char *name,int *checksum,void *) {
    if (mp) {
        if (!std::strcmp(name,"cgame_mp_x86.dll")) { *checksum = 11; return true; }
        assert(!std::strcmp(name,"ui_mp_x86.dll")); *checksum = uiChecksum; return !missingUi;
    }
    if (!std::strcmp(name,QL_NATIVE_CGAME_DLL)) {
        assert(!q3); ++binaryLookups; *checksum = 99; return true;
    }
    assert(q3);
    if (!std::strcmp(name,"vm/cgame.qvm")) { *checksum = 11; return true; }
    assert(!std::strcmp(name,"vm/ui.qvm")); *checksum = uiChecksum; return !missingUi;
}
static void CL_AddReliableCommand(const char *command,qboolean) { sent = command; ++reliableCommands; }
static int Cmd_Argc() { return static_cast<int>(arguments.size()); }
static const char *Cmd_Argv(int index) {
    return index >= 0 && index < Cmd_Argc() ? arguments[index].c_str() : "";
}
static std::vector<int> SV_Indices(int begin,int end) {
    std::vector<int> indices; for(int i=begin;i<end;++i) indices.push_back(i); return indices;
}
static std::vector<int> SV_Indices(int count) { return SV_Indices(0,count); }
template<typename T,size_t N> static int SV_ArraySize(const std::array<T,N> &) { return static_cast<int>(N); }
static bool FS_IsPureChecksum(int sum) { return sum == 11 || sum == 22 || sum == 99 || sum == INT32_MIN; }
static void SV_SendClientSnapshot(client_t *client) { assert(client->state == CS_ZOMBIE); ++snapshots; }
static void SV_DropClient(client_t *,const char *) { ++drops; }
static bool legacyFraming; // Elite Force Holomatch: its cp names no server id.
static bool SV_LegacyFraming(const client_t *) { return legacyFraming; }
static qboolean CL_LegacyFraming() { return legacyFraming ? qtrue : qfalse; }
'''


CHECKS = r'''
static client_t Verify(const std::string &command) {
    std::istringstream input(command); std::string token;
    arguments.clear();
    while(input >> token) arguments.push_back(token);
    drops = snapshots = 0;
    client_t client = {0,0,0,CS_ACTIVE,"fixture",{wireProfile}};
    SV_VerifyPaks_f(&client);
    return client;
}
static void Reject(const std::string &command) {
    client_t client = Verify(command);
    assert(!client.pureAuthentic && client.gotCP && drops == 1 && snapshots == 1);
}
int main() {
    q3 = true;
    CL_SendPureChecksums();
    assert(sent == "cp 17 11 22 @ 11 22 99 122 " && binaryLookups == 0);
    assert(Verify(sent).pureAuthentic && !drops && !snapshots);
    Reject("cp 17 99 @ 11 22 99 122"); /* QL shape cannot authenticate Q3. */
    Reject("cp 17 11 22 @oops 11 22 99 122");
    Reject("cp 17 11x 22 @ 11 22 99 122");
    Reject("cp 17 11 22 @ 11junk 22 99 122");
    Reject("cp 17 11 22 @ 2147483648 122");
    Reject("cp 17 11 22 @ -2147483649 122");
    Reject("cp 17 11 22 @ 11 11 5"); /* Duplicate references. */
    Reject("cp 17 11 22 @ 55 49"); /* Unloaded pak, even with correct XOR. */
    Reject("cp 17 11 22 @ 11 22 99 123");
    Reject("cp 17 11 22 @");
    Reject("cp 17garbage 11 22 @ 11 22 99 122");
    std::string excessive = "cp 17 11 22 @";
    for(int i=0;i<508;++i) excessive += " 0";
    Reject(excessive); /* More than 512 arguments must not be silently clipped. */
    assert(Verify("cp 17 11 22 @ 7").pureAuthentic); /* Pinned Q3 allows no general refs. */
    client_t stale = Verify("cp 16 11 22 @ 11 22 99 122");
    assert(!stale.gotCP && !stale.pureAuthentic && !drops && !snapshots);
    legacyFraming = true; /* Holomatch: cp cgame ui @ refs... mixed. */
    CL_SendPureChecksums();
    assert(sent == "cp 11 22 @ 11 22 99 122 ");
    assert(Verify(sent).pureAuthentic && !drops);
    assert(Verify("cp 11 22 @ 11 22 99 122").pureAuthentic && !drops);
    assert(Verify("cp 11 22 @ 7").pureAuthentic);
    Reject("cp 17 11 22 @ 11 22 99 122");
    Reject("cp 11 22 @");
    legacyFraming = false;
    for (const auto selected : {NETCHAN_WIRE_Q3_66, NETCHAN_WIRE_Q3_67}) {
        wireProfile = selected;
        CL_SendPureChecksums();
        assert(sent == "cp 11 22 @ 11 22 99 122 ");
        assert(Verify(sent).pureAuthentic && !drops);
        Reject("cp 17 11 22 @ 11 22 99 122");
        Reject("cp 11 22 @ 11 11 5");
        Reject("cp 11 22 @ 55 49");
        Reject("cp 11 22 @ 11 22 99 123");
        Reject("cp 11 23 @ 11 22 99 122");
        Reject("cp 11 22 @");
    }
    wireProfile = NETCHAN_WIRE_LEGACY_Q3;
    CL_SendPureChecksums();
    assert(sent == "cp 17 11 22 @ 11 22 99 122 ");
    missingUi = true;
    Reject(sent);
    missingUi = false;
    int parsed = 42;
    for(const char *bad : {"", "+", "-", "0x1", " 1", "1 ", "--1", "99999999999999999999999999"}) {
        assert(!SV_ParsePureChecksum(bad,parsed));
    }
    assert(SV_ParsePureChecksum("-2147483648",parsed) && parsed == INT32_MIN);
    assert(SV_ParsePureChecksum("+2147483647",parsed) && parsed == INT32_MAX);
    assert(SV_ParsePureChecksum("-0",parsed) && parsed == 0);

    std::string proof = FS_ReferencedPakPureChecksums(1023);
    assert(proof == "11 22 @ 11 22 99 122 ");
    assert(proof == FS_ReferencedPakPureChecksums(static_cast<int>(proof.size())));
    assert(!FS_ReferencedPakPureChecksums(static_cast<int>(proof.size())-1)[0]);
    assert(!FS_ReferencedPakPureChecksums(-1)[0]);
    assert(!FS_ReferencedPakPureChecksums(0)[0]);
    assert(proof == FS_ReferencedPakPureChecksums(INT32_MAX));
    q3 = false;
    CL_SendPureChecksums();
    assert(sent == "cp 17 99 @ 11 22 99 122 " && binaryLookups == 1);
    assert(Verify(sent).pureAuthentic && !drops);
    Reject("cp 17 11 22 @ 11 22 99 122");
    Reject("cp 17 99 @x 11 22 99 122");
    Reject("cp 17 99 @ 11 11 5");
    Reject("cp 17 99 @ 55 49");
    assert(!Verify("cp 16 99 @ 11 22 99 122").gotCP && !drops);
    assert(std::string(FS_ReferencedPakPureChecksums(1023)) == "@ 11 22 99 122 ");
    const int qlLookups = binaryLookups;
    mp = true;
    CL_SendPureChecksums();
    assert(sent == "cp 17 11 22 @ 11 22 99 122 " && binaryLookups == qlLookups);
    assert(Verify(sent).pureAuthentic && !drops);
    Reject("cp 17 99 @ 11 22 99 122");
    Reject("cp 17 11 23 @ 11 22 99 122");
    Reject("cp 17 11 22 @ 11 11 5");
    Reject("cp 17 11 22 @ 55 49");
    missingUi = true; Reject(sent); missingUi = false;
    sv.serverId = 20;
    assert(!Verify("cp 16 11 22 @ 7").gotCP && !drops);
    assert(Verify("cp 17 11 22 @ 7").pureAuthentic); // Feed survives a fast restart.
    assert(Verify("cp 18 11 22 @ 7").pureAuthentic);
    assert(Verify("cp 21 11 22 @ 7").pureAuthentic); // Original MP uses a lower bound.
    sv.serverId = 17;
    packs[0].referenced |= FS_UI_REF; packs[1].referenced = 0; uiChecksum = 11;
    CL_SendPureChecksums();
    assert(sent == "cp 17 11 11 @ 11 99 109 " && Verify(sent).pureAuthentic);
    packs[0].referenced &= ~FS_UI_REF; packs[1].referenced = FS_UI_REF | FS_GENERAL_REF; uiChecksum = 22;
    unavailable = true;
    const auto prior = sent;
    try { CL_SendPureChecksums(); assert(false); } catch(const std::runtime_error &) {}
    assert(sent == prior && !FS_ReferencedPakPureChecksums(1023)[0]);
    unavailable = mp = false;
    clc.demoplaying = true;
    int commands = reliableCommands;
    CL_SendPureChecksums();
    assert(reliableCommands == commands);
    clc.demoplaying = false; cl_connectedToPureServer = false;
    CL_SendPureChecksums();
    assert(reliableCommands == commands);
    cl_connectedToPureServer = true;
    std::vector<pack_t> manyPacks(600,{FS_GENERAL_REF,INT32_MIN});
    std::vector<searchpath_t> manyPaths(600);
    for(size_t i=0;i<manyPaths.size();++i) {
        manyPaths[i] = {&manyPacks[i],i+1 < manyPaths.size() ? &manyPaths[i+1] : nullptr};
    }
    fs_searchpaths = manyPaths.data();
    assert(!FS_ReferencedPakPureChecksums(INT32_MAX)[0]);
    try { CL_SendPureChecksums(); assert(false); } catch(const std::runtime_error &) {}
    assert(reliableCommands == commands); /* No truncated proof is sent. */
    return 0;
}
'''


class PureProfileContractTests(unittest.TestCase):
    def test_actual_profile_proof_generation_and_bounded_server_verification(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        files = (ROOT / "code/qcommon/files.c").read_text(encoding="utf-8")
        client = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        server = (ROOT / "code/server/sv_client.cpp").read_text(encoding="utf-8")
        public = (ROOT / "code/qcommon/qcommon.h").read_text(encoding="utf-8")
        constants = "\n".join(re.findall(r"^#define\s+FS_(?:GENERAL|UI|CGAME)_REF\s+\S+", public, re.M))
        extracted = "\n".join(function(source, signature) for source, signature in (
            (files, "static char *FS_AddPureChecksumToken"),
            (files, "const char *FS_ReferencedPakPureChecksums"),
            (client, "static void CL_SendPureChecksums"),
            (server, "static bool SV_HasDuplicateClientChecksums"),
            (server, "static bool SV_ClientChecksumsArePure"),
            (server, "static int SV_MixedClientChecksumFeed"),
            (server, "static bool SV_ParsePureChecksum"),
            (server, "static void SV_VerifyPaks_f"),
        ))
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="pure-profile-", dir=scratch) as folder:
            path = Path(folder)
            fixture = path / "pure.cpp"
            executable = path / ("pure.exe" if os.name == "nt" else "pure")
            fixture.write_text(constants + "\n" + PRELUDE + extracted + CHECKS, encoding="utf-8")
            build = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT),
                str(fixture), "-o", str(executable)], text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
