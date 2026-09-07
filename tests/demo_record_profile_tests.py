"""Execute the client recording initializer/finalizer with frozen wire profiles."""

from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DemoRecordProfileTests(unittest.TestCase):
    def test_actual_finalizer_keeps_ql91_and_q3_legacy_compatibility(self) -> None:
        compiler = next((path for name in ("clang++", "g++", "c++")
            if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        source = (ROOT / "code/client/cl_main.cpp").read_text(encoding="utf-8")
        start = source.index("void CL_StopRecord_f( void )")
        finalizer = source[start:source.index("\n}", start)+2]
        start = source.index("static void CL_Record_f( void )")
        initializer = re.search(r"clc\.dm68compat\s*=\s*[^;]+;", source[start:]).group(0)
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
'''
        fixture += finalizer + "\nstatic void InitializeCompatibility() { " + initializer + " }\n"
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
    assert(clc.dm68compat == (profile != NETCHAN_WIRE_QL_RETAIL));
    if (extended) clc.dm68compat = qfalse;
    if (staleCompat) clc.dm68compat = qtrue;
    writes.clear(); closes = removals = 0; finalName.clear(); printed.clear();
    CL_StopRecord_f();
    assert(finalName == "demos/probe.dm_" + std::to_string(expected));
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
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="demo-record-profile-", dir=scratch) as folder:
            path = Path(folder)
            cpp = path / "record.cpp"
            executable = path / ("record.exe" if os.name == "nt" else "record")
            cpp.write_text(fixture, encoding="utf-8")
            result = subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror",
                "-I",str(ROOT),str(cpp),"-o",str(executable)], text=True,
                capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
