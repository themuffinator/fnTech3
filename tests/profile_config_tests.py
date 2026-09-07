"""Execute the actual config startup sequence for both profile/safe-mode lanes."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProfileConfigTests(unittest.TestCase):
    def test_config_persistence_selects_profile_and_preserves_ql_retry(self) -> None:
        compiler = shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
        if compiler is None:
            self.skipTest("requires a C11 compiler")
        source = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        start = source.index("void Com_WriteConfiguration( void )")
        body = source[start:source.index("\n}", start) + 2]
        fixture = r'''
#include <assert.h>
#include <string.h>
#define CVAR_ARCHIVE 1
#define CVAR_CLOUD 2
#define Q3CONFIG_CFG "q3config.cfg"
static int com_fullyInitialized, cvar_modifiedFlags, q3writes, qlwrites, keywrites;
static int quakeLive, failQL, dedicated;
static void Com_WriteConfigToFile(const char *name) {
    assert(!strcmp(name, "q3config.cfg")); ++q3writes;
}
#ifndef DEDICATED
static struct { int integer; } dedicatedCvar;
#define com_dedicated (&dedicatedCvar)
static char cl_cdkey[34];
static int Com_GameIsQuakeLive(void) { return quakeLive; }
static int Com_WriteQLConfigurationFiles(void) {
    assert(quakeLive && !dedicated); ++qlwrites; return !failQL;
}
static const char *FS_GetCurrentGameDir(void) { return "baseq3"; }
static const char *FS_GetBaseGameDir(void) { return "baseq3"; }
static int UI_usesUniqueCDKey(void) { return 0; }
static int Q_stricmp(const char *a, const char *b) { return strcmp(a,b); }
static void Com_WriteCDKey(const char *dir, const char *key) {
    assert(!strcmp(dir,"baseq3") && key==cl_cdkey); ++keywrites;
}
#endif
''' + body + r'''
int main(void) {
    for (quakeLive=0; quakeLive<=1; ++quakeLive)
    for (dedicated=0; dedicated<=1; ++dedicated)
    for (failQL=0; failQL<=1; ++failQL)
    for (com_fullyInitialized=0; com_fullyInitialized<=1; ++com_fullyInitialized)
    for (int modified=0; modified<=1; ++modified) {
        q3writes=qlwrites=keywrites=0;
        cvar_modifiedFlags=8 | (modified ? CVAR_ARCHIVE | CVAR_CLOUD : 0);
        int qlPath=0;
#ifndef DEDICATED
        dedicatedCvar.integer=dedicated;
        qlPath=quakeLive && !dedicated;
#endif
        Com_WriteConfiguration();
        if (!com_fullyInitialized || !modified) {
            assert(!q3writes && !qlwrites && !keywrites);
            assert(cvar_modifiedFlags==(8 | (modified ? 3 : 0)));
        } else {
            assert(q3writes==!qlPath && qlwrites==qlPath);
            assert(cvar_modifiedFlags==(qlPath && failQL ? 11 : 8));
#ifndef DEDICATED
            assert(keywrites==!(qlPath && failQL));
#endif
        }
    }
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="profile-write-config-", dir=scratch) as folder:
            cfile = Path(folder) / "write-config.c"
            cfile.write_text(fixture, encoding="utf-8")
            for dedicated in (False, True):
                with self.subTest(dedicated=dedicated):
                    executable = Path(folder) / ("write-config.exe" if os.name == "nt" else "write-config")
                    command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
                    if dedicated:
                        command.append("-DDEDICATED")
                    build = subprocess.run([*command, str(cfile), "-o", str(executable)],
                                           capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_startup_replay_preserves_canonical_profile_and_other_overrides(self) -> None:
        compiler = shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
        if compiler is None:
            self.skipTest("requires a C11 compiler")
        source = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        start = source.index("void Com_StartupVariable( const char *match )")
        body = source[start:source.index("\n}", start) + 2]
        fixture = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#define CVAR_NONEXISTENT -1
#define CVAR_USER_CREATED 1
#define qtrue 1
static int active, com_numConsoleLines = 2;
static const char *com_consoleLines[2];
static char tokens[3][128], profile[128], basepath[128];
static const void *Com_GameProfile(void) { return active ? &active : NULL; }
static int Q_stricmp(const char *a, const char *b) {
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
static void Cmd_TokenizeString(const char *s) {
    assert(sscanf(s, "%127s %127s %127s", tokens[0],tokens[1],tokens[2]) == 3);
}
static const char *Cmd_Argv(int n) { return tokens[n]; }
static const char *Cmd_ArgsFrom(int n) { return tokens[n]; }
static int Cvar_Flags(const char *n) {
    return !Q_stricmp(n,"com_gameProfile") && !profile[0] ? CVAR_NONEXISTENT : 0;
}
static void Cvar_Set2(const char *n,const char *v,int force) {
    assert(force); strcpy(!Q_stricmp(n,"com_gameProfile") ? profile : basepath, v);
}
static void Cvar_Get(const char *n,const char *v,int flags) {
    assert(flags==CVAR_USER_CREATED); Cvar_Set2(n,v,1);
}
''' + body + r'''
int main(void) {
    const char *aliases[] = {"auto", "q3", "ql", "quake3", "quakelive"};
    char line[128];
    for (unsigned i=0;i<sizeof(aliases)/sizeof(aliases[0]);++i) {
        snprintf(line,sizeof(line),"set COM_gameProfile %s",aliases[i]);
        com_consoleLines[0]=line; com_consoleLines[1]="set fs_basepath installation";
        active=0; profile[0]=0; basepath[0]=0;
        Com_StartupVariable(NULL);
        assert(!strcmp(profile,aliases[i])); assert(!strcmp(basepath,"installation"));
        active=1; strcpy(profile,"quake3"); strcpy(basepath,"config_override");
        Com_StartupVariable(NULL);
        assert(!strcmp(profile,"quake3")); assert(!strcmp(basepath,"installation"));
        strcpy(basepath,"config_override");
        Com_StartupVariable("com_gameProfile");
        assert(!strcmp(profile,"quake3")); assert(!strcmp(basepath,"config_override"));
        strcpy(profile,"quakelive"); Com_StartupVariable(NULL);
        assert(!strcmp(profile,"quakelive"));
    }
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="profile-startup-", dir=scratch) as folder:
            cfile = Path(folder) / "startup.c"
            executable = Path(folder) / ("startup.exe" if os.name == "nt" else "startup")
            cfile.write_text(fixture, encoding="utf-8")
            build = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                    str(cfile), "-o", str(executable)], capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_config_order_and_safe_mode_do_not_cross_profiles(self) -> None:
        compiler = next((path for name in ("clang", "gcc", "cc")
                         if (path := shutil.which(name))), None)
        if compiler is None:
            self.skipTest("requires a C11 compiler")
        source = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        start = source.index("static void Com_ExecuteCfg( void )")
        body = source[start:source.index("\n}", start) + 2]
        fixture = r'''
#include <assert.h>
#include <string.h>
#define EXEC_NOW 0
#define Q3CONFIG_CFG "q3config.cfg"
#define QL_CONFIG_HARDWARE_FILE "qzconfig.cfg"
#define QL_CONFIG_REPLICATE_FILE "repconfig.cfg"
static int quake3, safe, hardware, probes, count, executions;
static const char *commands[8];
static int Com_GameIsQuake3(void) { return quake3; }
static int Com_SafeMode(void) { return safe; }
static int FS_ProfileFileExists(const char *name) {
    assert(!quake3); assert(!strcmp(name, QL_CONFIG_HARDWARE_FILE));
    ++probes; return hardware;
}
static void Cbuf_ExecuteText(int mode, const char *command) {
    assert(mode == EXEC_NOW); assert(count < 8); commands[count++] = command;
}
static void Cbuf_Execute(void) { ++executions; }
''' + body + r'''
int main(void) {
    for (quake3 = 0; quake3 <= 1; ++quake3)
    for (safe = 0; safe <= 1; ++safe)
    for (hardware = 0; hardware <= 1; ++hardware) {
        count = executions = probes = 0;
        Com_ExecuteCfg();
        assert(count == executions);
        assert(!strcmp(commands[0], "exec default.cfg\n"));
        if (quake3) {
            assert(probes == 0);
            assert(count == (safe ? 1 : 3));
            if (!safe) {
                assert(!strcmp(commands[1], "exec q3config.cfg\n"));
                assert(!strcmp(commands[2], "exec autoexec.cfg\n"));
            }
        } else {
            assert(probes == (safe ? 0 : 1));
            assert(count == (safe ? 3 : 4));
            if (!safe)
                assert(!strcmp(commands[1], hardware ? "execq qzconfig.cfg\n" : "execq q3config.cfg\n"));
#ifdef DEDICATED
            assert(!strcmp(commands[count-2], "execq server.cfg\n"));
#else
            assert(!strcmp(commands[count-2], "execq repconfig.cfg\n"));
#endif
            assert(!strcmp(commands[count-1], "execq autoexec.cfg\n"));
        }
    }
}
'''
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="profile-config-", dir=scratch) as folder:
            path = Path(folder)
            cfile = path / "config.c"
            cfile.write_text(fixture, encoding="utf-8")
            for dedicated in (False, True):
                with self.subTest(dedicated=dedicated):
                    executable = path / ("config.exe" if os.name == "nt" else "config")
                    command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
                    if dedicated:
                        command.append("-DDEDICATED")
                    build = subprocess.run([*command, str(cfile), "-o", str(executable)],
                                           text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], text=True, capture_output=True, timeout=15)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
