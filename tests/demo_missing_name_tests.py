"""Execute production demo lookup and check that a failure names its demo."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class DemoMissingNameTests(unittest.TestCase):
    def test_a_demo_that_does_not_open_is_named(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        # Use the production table so the search order cannot drift.
        common = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        table = re.search(r"const int demo_protocols\[\] = \{([^}]*)\};", common, re.S)
        demo_protocols = ", ".join(entry.strip() for entry in
            re.sub(r"/\*.*?\*/", "", table.group(1), flags=re.S).split(",") if entry.strip())

        def function(signature):
            start = source.index(signature)
            return source[start:source.index("\n}", start)+2]

        # CL_PlayDemo_f up to the point where a found demo replaces the
        # session; reaching it counts as a transition.
        play_start = source.index("static void CL_PlayDemo_f(")
        play = source[play_start:source.index("\n\thFile.reset();", play_start)]
        harness = r'''
#include <array>
#include <charconv>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "code/qcommon/q3_demo_protocol.hpp"
#include "code/client/client_alice_demo.hpp"
enum { COM_GAME_QUAKE3 = 1, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP,
       COM_GAME_WOLFET, COM_GAME_STVEF_SP, COM_GAME_STVEF_MP, COM_GAME_ALICE, COM_GAME_FAKK2,
       COM_GAME_MOHAA,
       OLD_PROTOCOL_VERSION = 68, NEW_PROTOCOL_VERSION = 71,
       QL_RETAIL_PROTOCOL_VERSION = 91, NETCHAN_RTCW_SP_PROTOCOL_VERSION = 49,
       NETCHAN_RTCW_MP_PROTOCOL_VERSION = 60, NETCHAN_STVEF_HM_PROTOCOL_VERSION = 24,
       NETCHAN_ALICE_PROTOCOL_VERSION = 1, NETCHAN_MOHAA_PROTOCOL_VERSION = 6,
       NETCHAN_WOLFET_PROTOCOL_VERSION = 84, NETCHAN_IOEF_PROTOCOL_VERSION = 26 };
struct Profile { int id; const char *displayName; } profile;
Profile *Com_GameProfile() { return &profile; }
int Com_GameIsRTCWSP() { return profile.id == COM_GAME_RTCW_SP; }
int Com_GameIsSTVEFMP() { return profile.id == COM_GAME_STVEF_MP; }
int Com_GameIsAlice() { return profile.id == COM_GAME_ALICE; }
int Com_GameIsRitual() { return profile.id == COM_GAME_ALICE || profile.id == COM_GAME_FAKK2; }
// Q3Test plays its demos with its own reader (cl_q3test.cpp).
int Com_GameIsQ3Test() { return 0; }
void CL_Q3TestPlayDemo_f() {}
const int demo_protocols[] = {''' + demo_protocols + r'''};
bool Com_DemoProtocolSupported(int p) {
    for (int value : demo_protocols) if (value && value == p) return true;
    return false;
}
struct ScopedFileHandle {
    bool open = false;
    int get() const { return 1; }
    void reset() { open = false; }
    explicit operator bool() const { return open; }
};
std::set<std::string> files;
std::vector<std::string> attempts, printed;
int CL_DetectLegacyDemoProtocol(int) { return 68; }
void OpenPureFileRead(const char *name, ScopedFileHandle &h) {
    attempts.emplace_back(name); h.open = files.count(name) != 0;
}
void Com_Printf(const char *format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    printed.emplace_back(text);
}
#define Com_sprintf snprintf
#define DEMOEXT "dm_"
#define LEGACY_DEMOEXT "dm3"
#define STVEF_DEMOEXT "efdemo"
#define MAX_OSPATH 256
#define ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#define S_COLOR_YELLOW "^3"
using qboolean = int;
int ToQboolean(bool b) { return b; }
int Q_stricmpn(const char *a, const char *b, int n) {
    for (int i=0; i<n; ++i) {
        int x=std::tolower(static_cast<unsigned char>(a[i]));
        int y=std::tolower(static_cast<unsigned char>(b[i]));
        if (x!=y || !x || !y) return x-y;
    }
    return 0;
}
int Q_stricmp(const char *a,const char *b) { return Q_stricmpn(a,b,256); }
void Q_strncpyz(char *out,const char *in,int size) { snprintf(out,size,"%s",in); }
const char *COM_GetExtension(const char *p) {
    auto dot=std::strrchr(p,'.'); return dot ? dot+1 : "";
}
std::string argument;
int transitions=0;
int Cmd_Argc() { return 2; }
const char *Cmd_Argv(int) { return argument.c_str(); }

''' + function("static bool CL_DemoProtocolSupported(") + "\n" + function(
            "static bool CL_TryDemoProtocol(") + "\n" + function(
            "static int CL_WalkDemoExt(") + "\n" + function(
            "static qboolean CL_IsLegacyDemoExt(") + "\n" + function(
            "static bool CL_IsSTVEFDemoExt(") + "\n" + function(
            "static bool CL_ParseDemoProtocolExtension(") + "\n" + play + \
            "\n++transitions;\n}\n" + r'''
static std::string shown(const std::string &text) {
    std::string out;
    for (unsigned char c : text) {
        char escaped[8];
        if (c >= 0x20 && c < 0x7f) { out += static_cast<char>(c); continue; }
        std::snprintf(escaped, sizeof(escaped), "\\x%02x", c);
        out += escaped;
    }
    return out;
}
static void check(bool held, const char *what, int line) {
    if (held) return;
    std::fprintf(stderr, "line %d: %s\nprinted:\n", line, what);
    for (const auto &text : printed) std::fprintf(stderr, "  %s\n", shown(text).c_str());
    std::fprintf(stderr, "attempted:\n");
    for (const auto &path : attempts) std::fprintf(stderr, "  %s\n", shown(path).c_str());
    std::exit(1);
}
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)
static const std::string yellow = "^3";
static void play(int id, const char *displayName, const char *demo) {
    profile.id = id; profile.displayName = displayName; argument = demo;
    attempts.clear(); printed.clear();
    CL_PlayDemo_f();
}
static int walk(int id, const char *displayName, const char *demo, char (&name)[256]) {
    ScopedFileHandle handle;
    profile.id = id; profile.displayName = displayName;
    attempts.clear(); printed.clear();
    // Whatever the caller's buffer held must not reach the console.
    std::memset(name, 0xa5, sizeof(name));
    const int protocol = CL_WalkDemoExt(demo, name, sizeof(name), handle);
    CHECK(protocol != -1 || !handle);
    return protocol;
}

int main() {
    char name[256];
    const char *et = "Wolfenstein: Enemy Territory";
    const std::string unplayable = std::string(et) + " demo playback is not implemented.\n";

    // ET searches its own dm_84 alone, never another profile's file of the
    // same name, and names the demo it looked for (2026-09-24: `+demo
    // enginelights` printed `couldn't open` and an uninitialised buffer).
    files = {"demos/enginelights.dm_60", "demos/enginelights.dm_68", "demos/enginelights.dm3",
             "demos/enginelights.efdemo", "demos/enginelights.dm_6"};
    CHECK(walk(COM_GAME_WOLFET, et, "enginelights", name) == -1);
    CHECK(attempts == std::vector<std::string>{"demos/enginelights.dm_84"});
    CHECK(std::string(name) == "demos/enginelights.dm_84");
    CHECK(printed == std::vector<std::string>{"Not found: demos/enginelights.dm_84\n"});
    play(COM_GAME_WOLFET, et, "enginelights");
    CHECK(transitions == 0);
    CHECK(printed == std::vector<std::string>{"Not found: demos/enginelights.dm_84\n",
        yellow + "couldn't open demos/enginelights.dm_84\n"});
    files.insert("demos/enginelights.dm_84");
    play(COM_GAME_WOLFET, et, "enginelights.dm_84");
    CHECK(transitions == 1 && printed == std::vector<std::string>{});
    transitions = 0;

    // A profile with one format and no such file names the path it tried.
    files.clear();
    struct Missing { int id; const char *displayName; std::string path; };
    for (const Missing &missing : {
            Missing{COM_GAME_QUAKELIVE, "Quake Live", "demos/missing.dm_91"},
            Missing{COM_GAME_RTCW_SP, "Return to Castle Wolfenstein (Single Player)",
                "demos/missing.dm_49"},
            Missing{COM_GAME_RTCW_MP, "Return to Castle Wolfenstein (Multiplayer)",
                "demos/missing.dm_60"},
            // Medal of Honor names its demos after protocol 6 alone.
            Missing{COM_GAME_MOHAA, "Medal of Honor: Allied Assault",
                "demos/missing.dm_6"}}) {
        CHECK(walk(missing.id, missing.displayName, "missing", name) == -1);
        CHECK(std::string(name) == missing.path);
        CHECK(attempts == std::vector<std::string>{missing.path});
        CHECK(printed == std::vector<std::string>{"Not found: " + missing.path + "\n"});
        play(missing.id, missing.displayName, "missing");
        CHECK(transitions == 0);
        CHECK(printed == std::vector<std::string>{"Not found: " + missing.path + "\n",
            yellow + "couldn't open " + missing.path + "\n"});
    }

    // Holomatch looks for retail's .efdemo, then ioEF's protocol-26 .dm_26, and
    // names the last path it tried.
    const char *holomatch = "Star Trek: Voyager - Elite Force (Holomatch)";
    CHECK(walk(COM_GAME_STVEF_MP, holomatch, "missing", name) == -1);
    CHECK(std::string(name) == "demos/missing.dm_26");
    CHECK(attempts == std::vector<std::string>{"demos/missing.efdemo", "demos/missing.dm_26"});
    CHECK(printed == std::vector<std::string>{"Not found: demos/missing.efdemo\n",
        "Not found: demos/missing.dm_26\n"});
    play(COM_GAME_STVEF_MP, holomatch, "missing");
    CHECK(transitions == 0);
    CHECK(printed == std::vector<std::string>{"Not found: demos/missing.efdemo\n",
        "Not found: demos/missing.dm_26\n", yellow + "couldn't open demos/missing.dm_26\n"});

    // Quake III searches each of its formats, then its pre-1.27 dm3, and still
    // names that last fallback.
    play(COM_GAME_QUAKE3, "Quake III Arena", "missing");
    CHECK(transitions == 0 && attempts.size() > 1 && attempts.back() == "demos/missing.dm3");
    CHECK(printed.size() == attempts.size() + 1);
    for (std::size_t i = 0; i < attempts.size(); ++i)
        CHECK(printed[i] == "Not found: " + attempts[i] + "\n");
    CHECK(printed.back() == yellow + "couldn't open demos/missing.dm3\n");

    // A demo that exists still opens under the name that found it.
    files = {"demos/present.dm_60"};
    CHECK(walk(COM_GAME_RTCW_MP, "Return to Castle Wolfenstein (Multiplayer)", "present", name) == 60);
    CHECK(std::string(name) == "demos/present.dm_60");
    play(COM_GAME_RTCW_MP, "Return to Castle Wolfenstein (Multiplayer)", "present");
    CHECK(transitions == 1 && printed == std::vector<std::string>{"Demo file: demos/present.dm_60\n"});
    files = {"demos/present.dm_6"};
    CHECK(walk(COM_GAME_MOHAA, "Medal of Honor: Allied Assault", "present", name) == 6);
    CHECK(std::string(name) == "demos/present.dm_6");
    play(COM_GAME_MOHAA, "Medal of Honor: Allied Assault", "present");
    CHECK(transitions == 2 && printed == std::vector<std::string>{"Demo file: demos/present.dm_6\n"});
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp)/"probe.cpp", Path(tmp)/"probe.exe"
            cpp.write_text(harness, encoding="utf-8")
            subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                            "-I", str(ROOT), str(cpp), "-o", str(exe)], check=True,
                           capture_output=True, text=True)
            result = subprocess.run([str(exe)], capture_output=True, text=True,
                                    errors="replace")
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
