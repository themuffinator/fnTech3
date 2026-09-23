"""Execute the client recording initializer/finalizer with frozen wire profiles."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]


class DemoRecordProfileTests(unittest.TestCase):
    def test_actual_finalizer_keeps_each_profiles_demo_protocol(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        start = source.index("void CL_StopRecord_f( void )")
        finalizer = source[start:source.index("\n}", start)+2]
        start = source.index("static void CL_Record_f( void )")
        initializer = re.search(r"clc\.dm68compat\s*=\s*[^;]+;", source[start:]).group(0)
        start = source.index("static bool CL_WireProfileKeepsDm68Fallback(")
        fallback = source[start:source.index("\n}", start)+2]
        fixture = r'''
#include "code/qcommon/protocol_contract.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
typedef int qboolean;
enum { qfalse, qtrue, FS_INVALID_HANDLE = -1, MAX_OSPATH = 128, OLD_PROTOCOL_VERSION = 68 };
#define DEMOEXT "dm_"
#define Com_sprintf snprintf
static qboolean ToQboolean(bool value) { return value ? qtrue : qfalse; }
static int LittleLong(int value) { return value; }
static struct {
    int recordfile, demoLegacyProtocol;
    qboolean dm68compat, demoplaying, explicitRecordName, demorecording, spDemoRecording;
    char recordName[MAX_OSPATH];
} clc;
static netchanWireProfile_t activeProfile;
static netchanWireProfile_t CL_ActiveWireProfile() { return activeProfile; }
constexpr const char *STVEF_DEMOEXT = "efdemo";
static qboolean CL_STVEFFraming() { return ToQboolean(activeProfile == NETCHAN_WIRE_STVEF_HM); }
static std::vector<int> writes;
static int closes, removals;
static std::string finalName, printed;
static void FileWrite(int handle,const int *data,int length) {
    assert(handle == 4 && length == 4); writes.push_back(*data);
}
static void CloseFile(int &handle) { assert(handle == 4); handle = FS_INVALID_HANDLE; ++closes; }
static void FS_HomeRemove(const char *) { ++removals; }
static bool FS_FileExists(const char *) { return false; }
static void FS_Rename(const char *from,const char *to) {
    assert(std::string(from) == "demos/probe.tmp"); finalName = to;
}
static void Com_Printf(const char *text) { printed = text; }
static void CL_SetDemoRecordingCvars(const char *) {}
'''
        fixture += fallback + "\n" + finalizer
        fixture += "\nstatic void InitializeCompatibility() { " + initializer + " }\n"
        fixture += r'''
static void Check(netchanWireProfile_t profile,bool extended,bool staleCompat,
    bool playback,int legacyProtocol,int expected) {
    clc = {};
    clc.recordfile = 4;
    clc.demorecording = clc.spDemoRecording = clc.explicitRecordName = qtrue;
    clc.demoplaying = playback;
    clc.demoLegacyProtocol = legacyProtocol;
    std::memcpy(clc.recordName,"demos/probe",sizeof("demos/probe"));
    activeProfile = profile;
    InitializeCompatibility();
    assert(clc.dm68compat == (profile == NETCHAN_WIRE_LEGACY_Q3 || profile == NETCHAN_WIRE_IOQ3));
    if (extended) clc.dm68compat = qfalse;
    if (staleCompat) clc.dm68compat = qtrue;
    writes.clear(); closes = removals = 0; finalName.clear(); printed.clear();
    CL_StopRecord_f();
    // Elite Force names its demos after the game, not the protocol.
    assert(finalName == (profile == NETCHAN_WIRE_STVEF_HM ? std::string("demos/probe.efdemo")
        : "demos/probe.dm_" + std::to_string(expected)));
    assert(writes == std::vector<int>({-1,-1}) && closes == 1 && removals == 1);
    assert(!clc.demorecording && !clc.spDemoRecording);
    assert(printed == "Stopped demo recording.\n");
    CL_StopRecord_f();
    assert(closes == 1 && removals == 1 && writes.size() == 2);
    assert(printed == "Not recording a demo.\n");
}
int main() {
    Check(NETCHAN_WIRE_LEGACY_Q3,false,false,false,0,68);
    Check(NETCHAN_WIRE_LEGACY_Q3,true,false,false,0,68);
    Check(NETCHAN_WIRE_IOQ3,false,false,false,0,68); /* Q3 without extension messages. */
    Check(NETCHAN_WIRE_IOQ3,true,false,false,0,71);  /* Q3 extension messages require 71. */
    Check(NETCHAN_WIRE_QL_RETAIL,false,false,false,0,91);
    Check(NETCHAN_WIRE_QL_RETAIL,true,false,false,0,91);
    Check(NETCHAN_WIRE_QL_RETAIL,false,true,false,0,91); /* Stale compat flag cannot relabel QL. */
    Check(NETCHAN_WIRE_QL_RETAIL,false,false,true,0,91);
    Check(NETCHAN_WIRE_LEGACY_Q3,true,false,true,66,66); /* Existing legacy source policy. */
    /* Original RTCW names each demo after its single protocol; a Q3 dm_68
       label would hand RTCW bodies to Q3's field tables. */
    Check(NETCHAN_WIRE_RTCW_SP,false,false,false,0,49);
    Check(NETCHAN_WIRE_RTCW_SP,false,true,false,0,49);
    Check(NETCHAN_WIRE_RTCW_SP,false,false,true,0,49);
    Check(NETCHAN_WIRE_RTCW_MP,false,false,false,0,60);
    Check(NETCHAN_WIRE_RTCW_MP,false,true,false,0,60);
    Check(NETCHAN_WIRE_RTCW_MP,false,false,true,0,60);
    Check(NETCHAN_WIRE_STVEF_HM,false,false,false,0,24);
    Check(NETCHAN_WIRE_STVEF_HM,false,true,true,0,24);
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="demo-record-profile-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "record.cpp"
            executable = path / ("record.exe" if os.name == "nt" else "record")
            cpp.write_text(fixture, encoding="utf-8")
            result = subprocess.run([compiler, *compiler_target_args(compiler),"-std=c++17","-Wall","-Wextra","-Werror",
                "-I",str(ROOT),str(cpp),"-o",str(executable)], text=True,
                capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


    def test_record_command_keeps_rtcw_sp_synchronous_client_precondition(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        def function(signature):
            start = source.index(signature)
            return source[start:source.index("\n}", start)+2] + "\n"
        fixture = r'''
#include "code/qcommon/protocol_contract.hpp"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
typedef int qboolean;
enum { qfalse, qtrue, FS_INVALID_HANDLE = -1, MAX_OSPATH = 128, CA_CONNECTED = 5, CA_ACTIVE = 8 };
enum { COM_GAME_QUAKE3, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_STVEF_MP };
#define S_COLOR_YELLOW "^3"
#define Com_sprintf snprintf
struct qtime_t { int tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec; };
struct netadr_t { int local; };
static qboolean ToQboolean(bool value) { return value ? qtrue : qfalse; }
static struct { int state; } cls;
static struct {
    netadr_t serverAddress;
    int recordfile;
    qboolean demorecording, spDemoRecording, explicitRecordName, demowaiting, dm68compat;
    char recordName[MAX_OSPATH], recordNameShort[32];
} clc;
static int game = COM_GAME_QUAKE3, gamestates;
static float synchronous;
static std::string argument, printed, opened;
static int Com_GameIsRTCWSP() { return game == COM_GAME_RTCW_SP; }
static int Com_GameIsSTVEFMP() { return game == COM_GAME_STVEF_MP; }
static netchanWireProfile_t CL_ActiveWireProfile() {
    return game == COM_GAME_RTCW_SP ? NETCHAN_WIRE_RTCW_SP :
        game == COM_GAME_RTCW_MP ? NETCHAN_WIRE_RTCW_MP :
        game == COM_GAME_STVEF_MP ? NETCHAN_WIRE_STVEF_HM : NETCHAN_WIRE_LEGACY_Q3;
}
static int Cmd_Argc() { return argument.empty() ? 1 : 2; }
static const char *Cmd_Argv(int) { return argument.c_str(); }
static float Cvar_VariableValue(const char *name) {
    assert(!std::strcmp(name, "g_synchronousClients")); return synchronous;
}
static int Cvar_VariableIntegerValue(const char *name) {
    return !std::strcmp(name, "g_synchronousClients") ? static_cast<int>(synchronous) : 0;
}
static bool NET_IsLocalAddress(const netadr_t *address) { return address->local != 0; }
static void Com_Printf(const char *format, ...) {
    char text[512]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); printed += text;
}
static const char *COM_GetExtension(const char *name) {
    const char *dot = std::strrchr(name, '.'); return dot ? dot + 1 : "";
}
static qboolean CL_IsLegacyDemoExt(const char *) { return qfalse; }
static bool CL_IsSTVEFDemoExt(const char *) { return false; }
static bool CL_ParseDemoProtocolExtension(const char *, int *) { return false; }
static bool CL_DemoProtocolSupported(int) { return false; }
static void Q_strncpyz(char *out, const char *in, int size) { snprintf(out, size, "%s", in); }
static void Q_strcat(char *out, int size, const char *in) {
    const std::size_t used = std::strlen(out);
    snprintf(out + used, static_cast<std::size_t>(size) - used, "%s", in);
}
static void Com_RealTime(qtime_t *t) { *t = {126, 8, 18, 9, 0, 0}; }
static int FS_FOpenFileWrite(const char *name) { opened = name; return 4; }
static void Com_TruncateLongString(char *out, const char *in) { snprintf(out, 32, "%s", in); }
static void CL_WebView_PublishGameDemo(const char *, const char *) {}
static void CL_SetDemoRecordingCvars(const char *) {}
static void CL_WriteGamestate(qboolean initial) { assert(initial); ++gamestates; }
'''
        fixture += function("static bool CL_WireProfileKeepsDm68Fallback(")
        fixture += function("static void CL_Record_f( void ) {")
        fixture += r'''
static void Record(int profile, float sync, bool local, bool accepted, bool refused, bool advised) {
    game = profile; synchronous = sync; clc = {}; clc.serverAddress.local = local;
    cls.state = CA_ACTIVE; argument = "probe"; printed.clear(); opened.clear(); gamestates = 0;
    CL_Record_f();
    assert((clc.demorecording == qtrue) == accepted);
    assert(opened == (accepted ? "demos/probe.tmp" : ""));
    assert(gamestates == (accepted ? 1 : 0));
    const bool sawRefusal = printed.find(
        "The server must have 'g_synchronousClients 1' set for demos\n") != std::string::npos;
    const bool sawAdvice = printed.find(
        "WARNING: You should set 'g_synchronousClients 1'") != std::string::npos;
    assert(sawRefusal == refused && sawAdvice == advised);
    if (accepted) assert(clc.dm68compat == (profile == COM_GAME_QUAKE3));
}
int main() {
    // Original SP refuses without synchronous clients, even against a remote server.
    Record(COM_GAME_RTCW_SP, 0.0f, true, false, true, false);
    Record(COM_GAME_RTCW_SP, 0.0f, false, false, true, false);
    Record(COM_GAME_RTCW_SP, 1.0f, true, true, false, false);
    /* Original SP tests the float value; the shared advice tests the integer. */
    Record(COM_GAME_RTCW_SP, 0.5f, true, true, false, true);
    // Retail Elite Force Holomatch refuses as original RTCW SP does.
    Record(COM_GAME_STVEF_MP, 0.0f, true, false, true, false);
    Record(COM_GAME_STVEF_MP, 1.0f, false, true, false, false);
    // MP and Q3 keep their warning and record anyway.
    Record(COM_GAME_RTCW_MP, 0.0f, true, true, false, true);
    Record(COM_GAME_RTCW_MP, 0.0f, false, true, false, false);
    Record(COM_GAME_QUAKE3, 0.0f, true, true, false, true);
    Record(COM_GAME_QUAKE3, 1.0f, true, true, false, false);
    // Other preconditions still come first.
    game = COM_GAME_RTCW_SP; synchronous = 0.0f; clc = {}; cls.state = CA_CONNECTED;
    printed.clear(); CL_Record_f();
    assert(printed == "You must be in a level to record.\n" && !clc.demorecording);
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="demo-record-command-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "record.cpp"
            executable = path / ("record.exe" if os.name == "nt" else "record")
            cpp.write_text(fixture, encoding="utf-8")
            result = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17", "-Wall",
                "-Wextra", "-Werror", "-Wno-unused-function", "-I", str(ROOT), str(cpp), "-o",
                str(executable)], text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
