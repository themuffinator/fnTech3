"""Execute the server's SP tick loop and snapshot scheduler against retail.

The fixture comes from the user's stvoy.exe 1.20: SV_Frame (0x4265c0),
SV_CheckPaused (0x4265a0), SV_SendClientMessages (0x4287e0), and the loopback
deadline in SV_SendMessageToClient (0x427eb0). Only game/payload/network
callbacks were replaced in the local observation process. No game is started
by these tests. Holomatch follows stvoyHM.exe 1.20: a fixed tick, a server
clock that moves only with ticks (0x4353a4), each client's next snapshot
held to the deadline its last message left (0x435478..0x43550d), and a
dedicated server that does nothing in a frame owing no tick (0x43529c). The
other profiles retain their snapshot scheduler while exercising their own ticks.
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
#include "code/qcommon/profile_frame_time.h"
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
CVAR(sv_maxRate);
constexpr int PACKET_MASK = 31;
struct clientFrame_t { int messageSize = 0, messageSent = 0, messageAcked = 0; };
struct client_t {
    int state = CS_ACTIVE, lastSnapshotTime = 0, snapshotMsec = 50, rate = 0, snapshotDelayMsec = 0;
    bool rateDelayed = false, netchan_start_queue = false;
    struct {
        struct { int type = NA_LOOPBACK; } remoteAddress;
        int lastSentSize = 100, lastSentTime = 0, outgoingSequence = 0;
        bool unsentFragments = false;
    } netchan;
    std::array<clientFrame_t, PACKET_MASK + 1> frames{};
};
struct msg_t { int cursize = 0; };
static int messageBytes = 1000;
static std::array<client_t, 1> clients;
static struct { int time, timeResidual, restartTime; float timeResidualFraction; } sv;
static struct { int time, msgTime; client_t *clients; } svs;
static struct {
    bool Poll(bool, uint32_t, int) { return false; }
    void Reset() {}
} idleExitTimer;
static comGameId_t profile;
static comGameProfile_t selected;
static float fraction;
const comGameProfile_t *Com_GameProfile() { selected.id = profile; return &selected; }
float Com_FrameFractionMsec() { return fraction; }
static int wallTime, time_game, cvar_modifiedFlags, gvm, issued, saves, sent, timeouts, heartbeats;
static std::vector<int> ticks, botFrames;
static constexpr const char *HEARTBEAT_FOR_MASTER = "test";
const char *Com_GameHeartbeatName() { return HEARTBEAT_FOR_MASTER; }
int Com_GameIsSTVEFSP() { return profile == COM_GAME_STVEF_SP; }
int Com_GameIsSTVEF() { return profile == COM_GAME_STVEF_SP || profile == COM_GAME_STVEF_MP; }
int Com_GameIsSTVEFMP() { return profile == COM_GAME_STVEF_MP; }
// Q3Test and IHV use private snapshot callbacks; tick checks below observe
// their production loop while their own tests cover the packet payloads.
int Com_GameIsQ3Test() { return profile == COM_GAME_Q3TEST; }
int Com_GameIsQuake3IHV() { return profile == COM_GAME_QUAKE3_IHV; }
static void SV_Q3TestSendClientMessages() { ++sent; }
static void SV_IHVNetSendClientMessages() { ++sent; }
static void SV_IHVServerFrame() {}
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
static void SV_MOHAAGameSpyFrame() {}
static void SV_SteamP2PFrame() {}
static void SV_BotFrame(int time) { botFrames.push_back(time); }
static void SV_SetConfigstring(int, const char *) {}
static void SV_UpdateQLCvarConfigstrings(bool) {}
static void SV_UpdateRTCWCvarConfigstrings(bool) {}
static void SV_CalcPings() {}
static void VM_Call(int, int, int, int time) { ticks.push_back(time); }
static void SV_JKAGhoul2SetTime(int) {}
static void SV_STVEFSPTestSave() { ++saves; }
static void SV_CheckTimeouts() { ++timeouts; }
static void SV_IssueNewSnapshot() { ++issued; }
static void SV_MasterHeartbeat(const char *) { ++heartbeats; }
// A bot's snapshot is only built; anyone else's message goes through the
// production SV_SendMessageToClient, where Holomatch sets its next deadline.
void SV_SendMessageToClient( msg_t *msg, client_t *client );
static void SV_RecordDemoMessage(client_t *, msg_t *) {}
static void SV_Netchan_Transmit(client_t *client, msg_t *) { ++client->netchan.outgoingSequence; }
static void SV_SendClientSnapshot(client_t *client) {
    ++sent;
    if (client->netchan.remoteAddress.type == NA_BOT) return;
    msg_t msg; msg.cursize = messageBytes;
    SV_SendMessageToClient(&msg, client);
}

PRODUCTION_FUNCTIONS

static void Reset(int fps = 20, float scale = 1, int start = 1000, int residual = 0) {
    fraction = 0; sv = { start, residual, 0, 0 }; svs = { 0, 0, clients.data() }; clients[0] = {};
    sv_killserverValue = {}; com_dedicatedValue = {}; sv_idleExitValue = {};
    sv_idleRestartValue = {}; sv_levelTimeResetValue = {}; com_speedsValue = {};
    cl_pausedValue = {}; sv_pausedValue = {}; com_sv_runningValue = { 1, 1 };
    sv_fpsValue = { fps, static_cast<float>(fps) };
    com_timescaleValue = { static_cast<int>(scale), scale };
    sv_maxRateValue = {}; messageBytes = 1000;
    wallTime = cvar_modifiedFlags = issued = saves = sent = timeouts = heartbeats = 0;
    ticks.clear(); botFrames.clear();
}
static void Frame(int gameMsec, int realMsec) {
    wallTime += realMsec;
    SV_Frame(gameMsec, realMsec);
}
static void OtherProfiles() {
    // Per-profile tick expectations, including repeated
    // simulation frames inside one wall-clock snapshot interval.
    for (auto id : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
                    COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_JKA_SP, COM_GAME_JKA_MP,
                    COM_GAME_JK2_SP, COM_GAME_JK2_MP, COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA }) {
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

        const bool fixed = id != COM_GAME_QUAKELIVE && id != COM_GAME_FAKK2 && id != COM_GAME_MOHAA;
        Reset(20, .5f);
        for (int i = 0; i < 20; ++i) Frame(25, 25);
        assert(sv.time == 1500 && ticks.size() == (fixed ? 10u : 20u) && sent == 10 && issued == 20);
        Reset(20, 2); Frame(100, 50);
        assert(sv.time == 1100 && sent == 1);
        assert(ticks == (fixed ? std::vector<int>{1050,1100} : std::vector<int>{1100}));
        Reset(20, 3); Frame(150, 50); Frame(149, 50);
        assert(ticks.size() == (fixed ? 5u : 1u));
        assert(sv.time == (fixed ? 1250 : 1150) && sv.timeResidual == (fixed ? 49 : 149));
        Reset(20, 0); Frame(0, 100);
        assert(sv.time == 1000 && ticks.empty() && sent == 1 && issued == 1);
        // Their dedicated servers run every frame through, owing a tick or not.
        Reset(); com_dedicated->integer = 1; Frame(30, 30);
        assert(ticks.empty() && issued == 1 && timeouts == 1 && heartbeats == 1);
        assert(botFrames == std::vector<int>{1000});
    }
}
static void FractionalJediTime() {
    for (auto id : {COM_GAME_JKA_SP, COM_GAME_JK2_SP}) {
        profile = id; Reset(); fraction = .75f;
        for (int i=0; i<28; ++i) Frame(1, 7);
        assert(ticks.empty() && sv.timeResidual == 49 && sv.timeResidualFraction == 0);
        Frame(1,7); assert(ticks == std::vector<int>{1050} && sv.timeResidualFraction == .75f);
        cl_paused->integer=1; Frame(1,7); assert(sv.timeResidualFraction == .75f);
        cl_paused->integer=0; Frame(1,7); assert(sv.timeResidual == 2 && sv.timeResidualFraction == .5f);
        Reset(); assert(sv.timeResidualFraction == 0);
    }
    for (auto id : {COM_GAME_QUAKE3_IHV, COM_GAME_Q3TEST}) {
        profile=id; Reset(20,3); Frame(150,50);
        assert(ticks == (std::vector<int>{1050,1100,1150}));
        Reset(20,.5f); Frame(25,50); assert(ticks.empty());
        Frame(25,50); assert(ticks == std::vector<int>{1050});
    }
    profile=COM_GAME_QUAKELIVE;
    // Integer division precedes QL's x87 product (1000 / 30 * .7 = 23).
    Reset(30,.7f); Frame(23,33); assert(ticks == std::vector<int>{1023});
    Reset(30,3); Frame(99,33); assert(ticks == std::vector<int>{1099});
    Reset(20,.7f); Frame(34,50); assert(ticks == std::vector<int>{1034});
}

static void Holomatch() {
    // stvoyHM.exe 1.20 keeps a fixed tick under timescale (SV_Frame
    // 0x435252), and its server clock is the game clock, which moves only
    // with each tick (0x4353a4): it stands still between ticks and while
    // paused, and follows timescale.
    profile = COM_GAME_STVEF_MP;
    Reset(); clients[0].netchan.remoteAddress.type = NA_IP; clients[0].rate = 25000;
    Frame(50, 1);
    assert(sv.time == 1050 && svs.time == 50 && sent == 1 && clients[0].lastSnapshotTime == 50);
    // The rate takes 41 ms for 1,000 bytes and 48 more, under the 50 ms
    // interval, so the interval is the wait and no delay is flagged.
    assert(clients[0].snapshotDelayMsec == 50 && !clients[0].rateDelayed);
    Frame(25, 30);
    assert(svs.time == 50 && sent == 1);
    Frame(25, 30);
    assert(svs.time == 100 && sent == 2);
    cl_paused->integer = 1; Frame(50, 200);
    assert(svs.time == 100 && sent == 2 && ticks.size() == 2 && sv_paused->integer == 1);
    cl_paused->integer = 0;
    // A rate slower than the interval sets the wait and flags the next one.
    Reset(); clients[0].netchan.remoteAddress.type = NA_IP; clients[0].rate = 3000; messageBytes = 1400;
    Frame(50, 1);
    assert(sent == 1 && clients[0].snapshotDelayMsec == (1400 + 48) * 1000 / 3000 && clients[0].rateDelayed);
    for (int i = 0; i < 9; ++i) Frame(50, 1);
    assert(sent == 1 && svs.time == 500);
    Frame(50, 1);
    assert(sent == 2);
    // A message counts at most 1,500 bytes; sv_maxRate holds the rate.
    Reset(); clients[0].netchan.remoteAddress.type = NA_IP; clients[0].rate = 90000;
    sv_maxRate->integer = 4000; messageBytes = 9000;
    Frame(50, 1);
    assert(clients[0].snapshotDelayMsec == (1500 + 48) * 1000 / 4000 && clients[0].rateDelayed);
    // At timescale 3 a frame runs three ticks; one more tick 20 ms of wall
    // time later makes the client due again, on the game clock.
    Reset(20, 3); clients[0].netchan.remoteAddress.type = NA_IP; clients[0].rate = 25000;
    Frame(150, 50);
    assert(ticks.size() == 3 && svs.time == 150 && sent == 1);
    Frame(60, 20);
    assert(ticks.size() == 4 && svs.time == 200 && sent == 2 && sv.timeResidual == 10);
    Reset(20, .5f);
    for (int i = 0; i < 20; ++i) Frame(25, 25);
    assert(sv.time == 1500 && ticks.size() == 10u && ticks.front() == 1050 && svs.time == 500);
    Reset(20, 2); Frame(100, 50);
    assert(ticks == (std::vector<int>{1050, 1100}) && svs.time == 100);
    Reset(20, 0); Frame(0, 100);
    assert(sv.time == 1000 && ticks.empty() && svs.time == 0);
    // A local client gets a snapshot every frame, with a tick or without
    // (0x435481..0x43548d); a bot's is built every frame, as no message
    // ever sets its deadline.
    for (int address : { NA_LOOPBACK, NA_BOT }) {
        Reset(); clients[0].netchan.remoteAddress.type = address;
        Frame(10, 10); Frame(10, 10);
        assert(sent == 2 && ticks.empty() && clients[0].snapshotDelayMsec == 0 && !clients[0].rateDelayed);
    }
    // A full packet queue or unsent fragments hold the snapshot back without
    // touching the flag; nor can a free or loading client get one.
    for (int blocked = 0; blocked < 2; ++blocked) {
        Reset(); clients[0].netchan.remoteAddress.type = NA_IP;
        clients[0].netchan.unsentFragments = blocked == 0; clients[0].netchan_start_queue = blocked == 1;
        Frame(50, 1);
        assert(sent == 0 && !clients[0].rateDelayed && clients[0].lastSnapshotTime == 0);
    }
    for (int state : { CS_FREE, CS_CONNECTED }) {
        Reset(); clients[0].netchan.remoteAddress.type = NA_IP; clients[0].state = state;
        Frame(50, 1);
        assert(sent == 0);
    }
    // A dedicated server does nothing in a frame that owes no tick
    // (0x43529c..0x4352b4): no bot frame, timeouts, snapshot or heartbeat.
    // Its bot frame runs once before the ticks a frame owes, at the time of
    // the last one (0x43538e); a listen server runs its own every frame, at
    // the time it has reached (0x435280).
    for (int dedicated : { 1, 0 }) {
        Reset(); com_dedicated->integer = dedicated; clients[0].netchan.remoteAddress.type = NA_BOT;
        Frame(30, 30);
        assert(ticks.empty() && sent == !dedicated && issued == !dedicated);
        assert(timeouts == !dedicated && heartbeats == !dedicated);
        assert(botFrames == (dedicated ? std::vector<int>{} : std::vector<int>{1030}));
        Frame(30, 30);
        assert(ticks == std::vector<int>{1050} && sv.timeResidual == 10 && svs.time == 50);
        assert(sent == (dedicated ? 1 : 2) && timeouts == (dedicated ? 1 : 2));
        assert(botFrames == (dedicated ? std::vector<int>{1000} : (std::vector<int>{1030, 1060})));
    }
    // At timescale 20 a frame of 5 ms owes two ticks: one bot frame runs
    // before them and one snapshot after.
    Reset(20, 20); com_dedicated->integer = 1; clients[0].netchan.remoteAddress.type = NA_IP;
    clients[0].rate = 25000;
    Frame(100, 5);
    assert(ticks == (std::vector<int>{1050, 1100}) && botFrames == std::vector<int>{1000} && sent == 1);
    // At timescale 3, 15 ms of game time a frame: the fourth frame owes the
    // first tick, the seventh the second, and only those frames send.
    Reset(20, 3); com_dedicated->integer = 1; clients[0].netchan.remoteAddress.type = NA_IP;
    clients[0].rate = 25000;
    std::vector<int> sentAt;
    for (int frame = 1; frame <= 7; ++frame) {
        const int before = sent;
        Frame(15, 5);
        if (sent != before) sentAt.push_back(frame);
    }
    assert(ticks == (std::vector<int>{1050, 1100}) && sentAt == (std::vector<int>{4, 7}) && heartbeats == 2);
}
static void SnapshotBoundaries() {
    for (auto id : { COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
                    COM_GAME_RTCW_MP, COM_GAME_WOLFET, COM_GAME_STVEF_SP }) {
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
    OtherProfiles(); SnapshotBoundaries(); Holomatch(); FractionalJediTime();
    std::printf("%d retail cases x 18 clock combinations; six-profile boundary controls and Holomatch passed\n", rows);
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
            function(snapshot, 'static void SV_HolomatchSnapshotDelay('),
            function(snapshot, 'void SV_SendMessageToClient('),
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
