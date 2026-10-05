"""Execute the server's SP tick loop and snapshot scheduler against retail.

The fixture comes from the user's stvoy.exe 1.20: SV_Frame (0x4265c0),
SV_CheckPaused (0x4265a0), SV_SendClientMessages (0x4287e0), and the loopback
deadline in SV_SendMessageToClient (0x427eb0). Only game/payload/network
callbacks were replaced in the local observation process. No game is started
by these tests. The six other profiles retain their shared scheduler.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


HARNESS = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "code/qcommon/game_profile.h"
enum { CS_FREE, CS_ZOMBIE, CS_CONNECTED, CS_PRIMED, CS_ACTIVE };
enum { NA_LOOPBACK, NA_IP, NA_BOT };
enum { CVG_SERVER, ERR_FATAL, CS_SERVERINFO, CS_SYSTEMINFO, GAME_RUN_FRAME };
constexpr int CVAR_SERVERINFO = 4, CVAR_SYSTEMINFO = 8;
constexpr bool qtrue = true, qfalse = false;
struct cvar_t { int integer = 0; float value = 0; };
#define CVAR(name) cvar_t name##Value; cvar_t *name = &name##Value
CVAR(sv_killserver); CVAR(com_sv_running); CVAR(com_dedicated);
CVAR(sv_idleExit); CVAR(com_timescale); CVAR(sv_fps); CVAR(sv_idleRestart);
CVAR(sv_levelTimeReset); CVAR(com_speeds); CVAR(cl_paused); CVAR(sv_paused);
struct client_t {
    int state = CS_ACTIVE, lastSnapshotTime = 0, snapshotMsec = 50, rate = 0;
    bool rateDelayed = false, netchan_start_queue = false;
    struct {
        struct { int type = NA_LOOPBACK; } remoteAddress;
        int lastSentSize = 100, lastSentTime = 0;
        bool unsentFragments = false;
    } netchan;
};
static std::array<client_t, 1> clients;
static struct { int time, timeResidual, restartTime; } sv;
static struct { int time, msgTime; client_t *clients; } svs;
static struct {
    bool Poll(bool, uint32_t, int) { return false; }
    void Reset() {}
} idleExitTimer;
static comGameId_t profile;
static int wallTime, time_game, cvar_modifiedFlags, gvm, issued, saves, sent;
static std::vector<int> ticks;
static constexpr const char *HEARTBEAT_FOR_MASTER = "test";
const char *Com_GameHeartbeatName() { return HEARTBEAT_FOR_MASTER; }
int Com_GameIsSTVEFSP() { return profile == COM_GAME_STVEF_SP; }
// Q3Test and the IHV test send and simulate through their own paths, which
// their own tests cover; no profile here reaches them.
int Com_GameIsQ3Test() { return profile == COM_GAME_Q3TEST; }
int Com_GameIsQuake3IHV() { return profile == COM_GAME_QUAKE3_IHV; }
static void SV_Q3TestSendClientMessages() { assert(false); }
static void SV_IHVServerFrame() { assert(false); }
static void SV_UpdateSysteminfoConfigstring() {}
static std::array<client_t, 1> &SV_Clients() { return clients; }
static int Sys_Milliseconds() { return wallTime; }
static void Sys_Sleep(int) { assert(false); }
static bool Cvar_CheckGroup(int) { return false; }
static void Cvar_Set(const char *name, const char *value) {
    assert(!std::strcmp(name, "sv_paused"));
    sv_paused->integer = std::atoi(value);
}
static const char *Cvar_InfoString(int, void *) { return ""; }
static const char *Cvar_InfoString_Big(int, void *) { return ""; }
static void Com_Error(int, const char *, ...) { std::abort(); }
static void Com_Printf(const char *, ...) {}
static void SV_Shutdown(const char *) { assert(false); }
static void SV_Restart(const char *) { assert(false); }
static bool SV_HandleQuitOnExitLevel(const char *) { return false; }
static bool SV_HasHumanClients() { return true; }
static void Cbuf_AddText(const char *) { assert(false); }
static void SV_TrackCvarChanges() {}
static void Zmq_UpdatePasswords() {}
static void Zmq_PumpRcon() {}
static void SV_SteamP2PFrame() {}
static void SV_BotFrame(int) {}
static void SV_SetConfigstring(int, const char *) {}
static void SV_UpdateQLCvarConfigstrings(bool) {}
static void SV_UpdateRTCWCvarConfigstrings(bool) {}
static void SV_CalcPings() {}
static void VM_Call(int, int, int, int time) { ticks.push_back(time); }
static void SV_STVEFSPTestSave() { ++saves; }
static void SV_CheckTimeouts() {}
static void SV_IssueNewSnapshot() { ++issued; }
static void SV_MasterHeartbeat(const char *) {}
static void SV_SendClientSnapshot(client_t *) { ++sent; }

PRODUCTION_FUNCTIONS

static void Reset(int fps = 20, float scale = 1, int start = 1000, int residual = 0) {
    sv = { start, residual, 0 }; svs = { 0, 0, clients.data() }; clients[0] = {};
    sv_killserverValue = {}; com_dedicatedValue = {}; sv_idleExitValue = {};
    sv_idleRestartValue = {}; sv_levelTimeResetValue = {}; com_speedsValue = {};
    cl_pausedValue = {}; sv_pausedValue = {}; com_sv_runningValue = { 1, 1 };
    sv_fpsValue = { fps, static_cast<float>(fps) };
    com_timescaleValue = { static_cast<int>(scale), scale };
    wallTime = cvar_modifiedFlags = issued = saves = sent = 0; ticks.clear();
}
static void Frame(int gameMsec, int realMsec) {
    wallTime += realMsec;
    SV_Frame(gameMsec, realMsec);
}
static void OtherProfiles() {
    // Concrete controls for the six unchanged profiles, including repeated
    // simulation frames inside one wall-clock snapshot interval.
    for (auto id : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
                    COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_STVEF_MP }) {
        profile = id; Reset();
        for (int i = 0; i < 10; ++i) Frame(50, 1);
        assert(sv.time == 1500 && ticks.size() == 10 && sent == 0 && issued == 10);
        Frame(0, 40);
        assert(sv.time == 1500 && sent == 1 && issued == 11 && saves == 0);
        assert(clients[0].lastSnapshotTime == 50);
        cl_paused->integer = 1; Frame(50, 200);
        assert(sv.time == 1500 && sv.timeResidual == 0 && sent == 1 && issued == 11);
        assert(svs.time == 250 && sv_paused->integer == 1);
        cl_paused->integer = 0; Frame(50, 1);
        assert(sv.time == 1550 && sent == 2 && issued == 12);

        Reset(20, .5f);
        for (int i = 0; i < 20; ++i) Frame(25, 25);
        assert(sv.time == 1500 && ticks.size() == 20 && sent == 10 && issued == 20);
        Reset(20, 2); Frame(100, 50);
        assert(sv.time == 1100 && ticks == std::vector<int>{1100} && sent == 1);
        Reset(20, 0); Frame(0, 100);
        assert(sv.time == 1000 && ticks.empty() && sent == 1 && issued == 1);
    }
}
static void SnapshotBoundaries() {
    for (auto id : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
                    COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_STVEF_SP, COM_GAME_STVEF_MP }) {
        profile = id;
        for (int address : { NA_LOOPBACK, NA_IP, NA_BOT }) {
            const bool everyTick = id == COM_GAME_STVEF_SP && address == NA_LOOPBACK;
            Reset(); clients[0].netchan.remoteAddress.type = address;
            SV_SendClientMessages();
            assert(sent == (everyTick ? 1 : 0));
            // Free and pre-gamestate clients cannot receive snapshots.
            for (int state : { CS_FREE, CS_CONNECTED }) {
                Reset(); clients[0].netchan.remoteAddress.type = address;
                clients[0].state = state; svs.time = 100;
                SV_SendClientMessages(); assert(sent == 0);
            }
            for (int state : { CS_PRIMED, CS_ACTIVE }) {
                Reset(); clients[0].netchan.remoteAddress.type = address;
                clients[0].state = state; svs.time = 50;
                SV_SendClientMessages();
                assert(sent == 1 && clients[0].lastSnapshotTime == 50 && !clients[0].rateDelayed);
            }
            // Neither fragments, a pending netchan message nor a byte-rate
            // limit can be bypassed by SP's local tick policy.
            for (int blocked = 0; blocked < 3; ++blocked) {
                Reset(); clients[0].netchan.remoteAddress.type = address;
                clients[0].netchan.unsentFragments = blocked == 0;
                clients[0].netchan_start_queue = blocked == 1;
                if (blocked == 2) clients[0].rate = 1000;
                svs.time = 50; wallTime = 50;
                SV_SendClientMessages();
                assert(sent == 0 && clients[0].lastSnapshotTime == 0 && clients[0].rateDelayed);
                clients[0].netchan.unsentFragments = clients[0].netchan_start_queue = false;
                clients[0].rate = 0;
                SV_SendClientMessages();
                assert(sent == 1 && clients[0].lastSnapshotTime == 50 && !clients[0].rateDelayed);
            }
        }
    }
    profile = COM_GAME_STVEF_SP; Reset();
    // Pause keeps fractional tick progress; a map/save clock rewind cannot
    // strand a local snapshot behind a deadline from the preceding level.
    Frame(49, 500); assert(ticks.empty() && sent == 0);
    cl_paused->integer = 1; Frame(200, 1000);
    assert(sv.timeResidual == 49 && ticks.empty() && sent == 0);
    cl_paused->integer = 0; Frame(1, 1);
    assert(ticks == std::vector<int>{1050} && sent == 1);
    sv.time = 1000; clients[0].lastSnapshotTime = 50000; Frame(50, 1);
    assert(ticks == (std::vector<int>{1050,1050}) && sent == 2);
    com_sv_running->integer = 0; Frame(50, 1);
    assert(ticks.size() == 2 && sent == 2);
}
int main() {
    int fps, msec, residual, paused, start, time, remaining, sends, count, rows = 0;
    while (std::scanf("%d%d%d%d%d%d%d%d%d", &fps, &msec, &residual, &paused,
                      &start, &time, &remaining, &sends, &count) == 9) {
        std::vector<int> expected(count);
        for (int &tick : expected) assert(std::scanf("%d", &tick) == 1);
        profile = COM_GAME_STVEF_SP;
        for (float scale : {0.f, .25f, .5f, 1.f, 2.f, 4.f}) {
            for (int realMsec : {1,16,100}) {
                Reset(fps, scale, start, residual); cl_paused->integer = paused;
                Frame(msec, realMsec);
                if (sv.time != time || sv.timeResidual != remaining || ticks != expected ||
                    sent != sends || issued != (count > 0) || saves != (count > 0)) {
                    std::fprintf(stderr, "Retail case %d (fps %d msec %d residual %d pause %d scale %.2f real %d): "
                        "time %d/%d residual %d/%d ticks %zu/%d snapshots %d/%d issued %d saved %d\n",
                        rows,fps,msec,residual,paused,scale,realMsec,sv.time,time,
                        sv.timeResidual,remaining,ticks.size(),count,sent,sends,issued,saves);
                    return 1;
                }
                assert(svs.time == realMsec);
            }
        }
        ++rows;
    }
    assert(rows == 780);
    OtherProfiles(); SnapshotBoundaries();
    std::printf("%d retail cases x 18 clock combinations; seven-profile boundary controls passed\n", rows);
}
'''


class STVEFSPCadenceTests(unittest.TestCase):
    def test_tick_and_snapshot_cadence(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        snapshot = (ROOT / 'code/server/sv_snapshot.cpp').read_text(encoding='utf-8')
        production = '\n'.join((
            function(main, 'static bool SV_CheckPaused('),
            'static constexpr int UDPIP_HEADER_SIZE = 28;',
            function(main, 'int SV_RateMsec('),
            function(snapshot, 'void SV_SendClientMessages('),
            function(main, 'void SV_Frame('),
        ))
        fixture = json.loads((ROOT / 'tests/fixtures/stvef_sp_cadence_retail.json').read_text())
        self.assertEqual(len(fixture['cases']), 780)
        inputs = []
        for row in fixture['cases']:
            self.assertEqual(row['next'], row['time'] - 1)
            values = [row[key] for key in ('fps', 'msec', 'residual', 'paused', 'start',
                                         'time', 'remaining', 'sends')]
            inputs.append(' '.join(map(str, [*values, len(row['ticks']), *row['ticks']])))
        target = compiler_target_args(compiler)
        sanitizers = (['-fsanitize=address,undefined,float-cast-overflow',
                       '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
                      if os.name != 'nt' else [])
        with tempfile.TemporaryDirectory(prefix='stvef-sp-cadence-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(HARNESS.replace('PRODUCTION_FUNCTIONS', production), encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-D_CRT_SECURE_NO_WARNINGS',
                                    *sanitizers, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], input='\n'.join(inputs) + '\n', cwd=ROOT,
                                 capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
