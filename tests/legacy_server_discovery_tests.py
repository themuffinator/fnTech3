"""Run the production server-discovery contract for every profile.

Each profile's retail engine fixes the port its servers listen on, the master
it publishes to and the port that master listens on, the game a heartbeat
names, how it announces a shutdown and the keys its info reply carries beyond
Quake III's. The values below were read from the retail executables
(docs/fntech3/2026-10-05-legacy-server-hosting.md names every address).
These tests compile the production functions out of game_profile.cpp and
sv_main.cpp and run them under each profile id, including none selected.
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

PROFILE_IDS = ['AUTO', 'QUAKE3', 'QUAKELIVE', 'RTCW_SP', 'RTCW_MP', 'WOLFET', 'STVEF_SP', 'STVEF_MP',
               'QUAKE3_IHV', 'JKA_SP', 'JKA_MP', 'JK2_SP', 'JK2_MP', 'ALICE', 'FAKK2', 'MOHAA', 'Q3TEST']

QUAKE = ('QuakeArena-1', 'QuakeArena-1', 2)
# serverPort, default master, master port, heartbeat, flatline, flatline sends
EXPECTED = {name: (27960, None, 27950) + QUAKE for name in PROFILE_IDS}
EXPECTED.update({
    'QUAKE3': (27960, 'master.quake3arena.com', 27950) + QUAKE,
    'RTCW_MP': (27960, 'wolfmaster.idsoftware.com', 27950, 'Wolfenstein-1', 'WolfFlatline-1', 1),
    'WOLFET': (27960, 'etmaster.idsoftware.com', 27950, 'EnemyTerritory-1', 'ETFlatline-1', 1),
    'STVEF_MP': (27960, 'master.stef1.ravensoft.com:27953', 27953, 'STEF1', 'STEF1', 2),
    'JKA_MP': (29070, 'masterjk3.ravensoft.com', 29060) + QUAKE,
    'JK2_MP': (28070, 'masterjk2.ravensoft.com', 28060) + QUAKE,
    'MOHAA': (12203, None, 27950) + QUAKE,
})
# sv_master2's default where the retail master has gone: Raven's Elite Force
# master no longer answers, and Lilium Voyager's community one does
# (docs/fntech3/2026-10-24-stvef-playability.md).
COMMUNITY = {'STVEF_MP': 'efmaster.tjps.eu'}

# The keys each retail SVC_Info adds beyond Quake III's, in its order, for
# the cvar values given in VALUES below.
VALUES = dict(sv_allowAnonymous='1', g_gameskill='3', g_friendlyFire='1', g_maxlives='5',
              g_noTeamSwitching='1', g_antilag='1', g_needpass='1', g_jediVmerc='1',
              g_weaponDisable='524279', g_duelWeaponDisable='65531', g_forcePowerDisable='163837')
INFO = {name: [] for name in PROFILE_IDS}
INFO.update({
    'QUAKE3': [['punkbuster', '0']],
    'RTCW_MP': [['sv_allowAnonymous', '1'], ['gameskill', '3'], ['friendlyFire', '1'], ['maxlives', '1'],
                ['tourney', '1'], ['punkbuster', '0'], ['gamename', 'wolfmp'], ['g_antilag', '1']],
    'JKA_MP': [['needpass', '1'], ['truejedi', '1'], ['wdisable', '524279'], ['fdisable', '163837']],
    'JK2_MP': [['sv_allowAnonymous', '1']],
})


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if not depth:
                return source[start:i + 1]
    raise AssertionError(signature)


PREDICATES = r'''
enum { ''' + ', '.join(f'ID_{name}' for name in PROFILE_IDS) + r''' };
// This synthetic host changes profiles within one process. Keep the test
// selector observable across calls even under MinGW's whole-unit analysis.
static volatile int id;
extern "C" {
int Com_GameIsQuake3(void) { return id == ID_QUAKE3; }
int Com_GameIsRTCWMP(void) { return id == ID_RTCW_MP; }
int Com_GameIsWolfET(void) { return id == ID_WOLFET; }
int Com_GameIsSTVEFMP(void) { return id == ID_STVEF_MP; }
int Com_GameIsJKAMP(void) { return id == ID_JKA_MP; }
int Com_GameIsJK2MP(void) { return id == ID_JK2_MP; }
int Com_GameIsMOHAA(void) { return id == ID_MOHAA; }
}
'''


class LegacyServerDiscoveryTests(unittest.TestCase):
    def compile_run(self, source, arguments=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='legacy-discovery-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', '-Wall',
                                    '-D_CRT_SECURE_NO_WARNINGS', *sanitizer, str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe), *arguments], cwd=ROOT, capture_output=True, text=True,
                                 timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            return ran.stdout

    def test_ports_masters_and_heartbeat_names(self):
        profile = (ROOT / 'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
        functions = '\n'.join(function(profile, signature) for signature in (
            'int Com_GameDefaultServerPort( void )', 'const char *Com_GameDefaultMasterServer( void )',
            'const char *Com_GameCommunityMasterServer( void )',
            'int Com_GameMasterPort( void )', 'const char *Com_GameHeartbeatName( void )',
            'const char *Com_GameFlatlineName( void )'))
        header = (ROOT / 'code/qcommon/game_profile.h').read_text(encoding='utf-8')
        for name in ('Com_GameDefaultMasterServer', 'Com_GameCommunityMasterServer', 'Com_GameMasterPort',
                     'Com_GameHeartbeatName', 'Com_GameFlatlineName'):
            self.assertIn(name + '( void );', header)
        source = '#include <cstdio>\n#include <cstddef>\n' + PREDICATES + 'extern "C" {\n' + functions + r'''
}
int main() {
    for (id = 0; id <= ID_Q3TEST; ++id) {
        const char *master = Com_GameDefaultMasterServer();
        const char *community = Com_GameCommunityMasterServer();
        std::printf("%d %d %s %d %s %s %s\n", id, Com_GameDefaultServerPort(), master ? master : "-",
            Com_GameMasterPort(), Com_GameHeartbeatName(), Com_GameFlatlineName(), community ? community : "-");
    }
}
'''
        rows = [line.split() for line in self.compile_run(source).splitlines()]
        self.assertEqual(len(rows), len(PROFILE_IDS))
        for row in rows:
            name = PROFILE_IDS[int(row[0])]
            port, master, master_port, heartbeat, flatline, _ = EXPECTED[name]
            with self.subTest(profile=name):
                self.assertEqual(int(row[1]), port)
                self.assertEqual(row[2], master or '-')
                self.assertEqual(int(row[3]), master_port)
                self.assertEqual(row[4], heartbeat)
                self.assertEqual(row[5], flatline)
                self.assertEqual(row[6], COMMUNITY.get(name, '-'))

    def test_info_keys_and_shutdown(self):
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        retail_keys = function(main, 'static void SV_RetailInfoKeys( char *info )')
        shutdown = function(main, 'void SV_MasterShutdown( void )')
        publishing = function(main, 'static bool SV_MasterPublishing( int netenabled )')
        values = ''.join('{"%s","%s"},' % item for item in VALUES.items())
        source = r'''
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
''' + PREDICATES + r'''
constexpr int NET_ENABLEV4 = 1, NET_ENABLEV6 = 2;
struct cvar_t { int integer; };
static cvar_t gametype{0}, dedicated{0}, advertise{1};
static cvar_t *sv_gametype = &gametype, *com_dedicated = &dedicated, *sv_masterAdvertise = &advertise;
static struct { int nextHeartbeatTime, time; } svs;
static std::map<std::string, std::string> cvars = {''' + values + r'''};
static std::vector<std::pair<std::string, std::string>> keys;
static std::vector<std::string> beats;
static void Info_SetValueForKey(char *, const char *key, const char *value) {
    for (auto it = keys.begin(); it != keys.end(); ++it)
        if (it->first == key) { keys.erase(it); break; }
    if (value && *value) keys.emplace_back(key, value);
}
static int Cvar_VariableIntegerValue(const char *name) {
    auto it = cvars.find(name); return it == cvars.end() ? 0 : std::atoi(it->second.c_str());
}
static const char *Cvar_VariableString(const char *name) {
    auto it = cvars.find(name); return it == cvars.end() ? "" : it->second.c_str();
}
static const char *va(const char *format, ...) {
    static char buffer[256]; va_list args; va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); return buffer;
}
extern "C" const char *Com_GameFlatlineName(void) {
    return id == ID_RTCW_MP ? "WolfFlatline-1" : id == ID_WOLFET ? "ETFlatline-1"
        : id == ID_STVEF_MP ? "STEF1" : "QuakeArena-1";
}
static void SV_MasterHeartbeat(const char *message, bool stopping = false) {
    if (svs.nextHeartbeatTime - svs.time <= 0) beats.push_back(std::string(stopping ? "stop:" : "") + message);
    svs.nextHeartbeatTime = svs.time + 300000;
}
''' + retail_keys + '\n' + publishing + '\n' + shutdown + r'''
int main() {
    for (id = 0; id <= ID_Q3TEST; ++id) {
        std::printf("%d info", id);
        for (int type : {0, 3, 4, 8}) {
            keys.clear(); gametype.integer = type; char unused[1] = "";
            SV_RetailInfoKeys(unused);
            std::printf(" [");
            for (const auto &k : keys) std::printf("[\"%s\",\"%s\"],", k.first.c_str(), k.second.c_str());
            std::printf("[]]");
        }
        beats.clear(); svs.time = 1000; svs.nextHeartbeatTime = 900000;
        SV_MasterShutdown();
        std::printf(" status 0 flatlines %zu", beats.size());
        for (const auto &b : beats) std::printf(" %s", b.c_str());
        std::printf("\n");
    }
    // Only a public dedicated server with a network open, and the gate on.
    int gate = 0;
    for (int d : {0, 1, 2}) for (int net : {0, NET_ENABLEV4, NET_ENABLEV6, 3}) for (int a : {0, 1}) {
        dedicated.integer = d; advertise.integer = a;
        if (SV_MasterPublishing(net) != (d == 2 && net != 0 && a == 1)) return 1;
        gate += SV_MasterPublishing(net);
    }
    sv_masterAdvertise = nullptr; dedicated.integer = 2;
    if (!SV_MasterPublishing(NET_ENABLEV4)) return 2;
    std::printf("gate %d\n", gate);
}
'''
        output = self.compile_run(source).splitlines()
        self.assertEqual(output[-1], 'gate 3')
        for line in output[:-1]:
            head, rest = line.split(' info ', 1)
            name = PROFILE_IDS[int(head)]
            info_part, tail = rest.split(' status ')
            groups = [json.loads(group)[:-1] for group in info_part.split(' ')]
            _, flatlines, *names = tail.replace('flatlines ', '').split()
            with self.subTest(profile=name):
                expected = INFO[name]
                # A duel (3) or power duel (4) publishes Academy's duel weapon set.
                if name == 'JKA_MP':
                    duel = [row if row[0] != 'wdisable' else ['wdisable', '65531'] for row in expected]
                    self.assertEqual(groups, [expected, duel, duel, expected])
                else:
                    self.assertEqual(groups, [expected] * 4)
                _, _, _, _, flatline, sends = EXPECTED[name]
                self.assertEqual(int(flatlines), sends)
                # Elite Force's second shutdown packet is its heartstop.
                expected = [flatline] * sends if name != 'STVEF_MP' else [flatline, 'stop:' + flatline]
                self.assertEqual(names, expected)

    def test_retail_gate_keys_reach_the_info_reply(self):
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        info = function(main, 'static void SVC_Info( const netadr_t *from )')
        # The retail keys join the reply before its game directory and the
        # profile's own send, so every reply path carries them.
        self.assertLess(info.index('SV_RetailInfoKeys( infostring.data() );'), info.index('"game", gamedir'))
        self.assertLess(info.index('SV_RetailInfoKeys( infostring.data() );'), info.index('NET_OutOfBandPrint'))
        frame = function(main, 'void SV_Frame( int msec, int realMsec )')
        self.assertIn('SV_MasterHeartbeat( Com_GameHeartbeatName() );', frame)
        self.assertNotIn('HEARTBEAT_FOR_MASTER', main)
        resolve = function(main, 'static bool SV_ResolveMaster( int i, int netenabled )')
        self.assertEqual(resolve.count('BigShort(Com_GameMasterPort())'), 2)
        # Elite Force's key-value heartbeat names the profile's game and turns
        # into a heartstop as the server shuts down.
        beat = function(main, 'static void SV_MasterHeartbeat( const char *message, bool stopping = false )')
        self.assertIn('const char *kind = stopping ? "heartstop" : "heartbeat";', beat)
        self.assertEqual(beat.count('kind, Cvar_VariableIntegerValue( "net_port'), 2)
        self.assertNotIn('STVEF_HEARTBEAT_GAME', main)
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        self.assertIn('index == 0 && defaultMaster ? defaultMaster :\n\t\t\tindex == 1 && communityMaster ? communityMaster : ""',
                      init.replace('\r\n', '\n'))
        client = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        browse = function(client, 'static void CL_GlobalServers_f( void ) {')
        self.assertIn('BigShort( static_cast<short>( Com_GameMasterPort() ) )', browse)
        self.assertNotIn('PORT_MASTER', browse)

    def test_ping_limits_measure_the_challenge_round_trip(self):
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        client = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        start = client.index('struct SV_ChallengeTime {')
        table = client[start:client.index('/*', start)]
        source = '#include <array>\n#include <cstddef>\n#include <cstdio>\n#include <string>\n' + PREDICATES + r'''
struct netadr_t { int ip, port; };
struct cvar_t { int integer; };
static cvar_t minValue{0}, maxValue{0};
static cvar_t *sv_minPing = &minValue, *sv_maxPing = &maxValue;
static struct { int time; } svs;
static bool lan;
static std::string printed;
static bool NET_CompareAdr(const netadr_t *a, const netadr_t *b) { return a->ip == b->ip && a->port == b->port; }
static bool Sys_IsLANAddress(const netadr_t *) { return lan; }
enum { NS_SERVER };
static void NET_OutOfBandPrint(int, const netadr_t *, const char *text) { printed = text; }
static void Com_Printf(const char *, ...) {}
static void Com_DPrintf(const char *, ...) {}
''' + function(main, 'int SV_PingLimitVerdict( int ping, int minPing, int maxPing )') + '\n' + \
            function(main, 'bool SV_ProfileHasPingLimits( void )') + '\n' + table + \
            function(client, 'static bool SV_PingLimitsRefuse( const netadr_t *from )') + r'''
#define CHECK(x) do { if (!(x)) { std::printf("line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    CHECK(SV_PingLimitVerdict(50, 0, 0) == 0 && SV_PingLimitVerdict(50, 100, 0) == 1);
    CHECK(SV_PingLimitVerdict(100, 100, 0) == 0 && SV_PingLimitVerdict(301, 0, 300) == 2);
    CHECK(SV_PingLimitVerdict(300, 100, 300) == 0);
    const netadr_t a{1, 27960}, b{2, 27960};
    // Unknown addresses and LAN clients are never refused.
    minValue.integer = 100; maxValue.integer = 200;
    CHECK(!SV_PingLimitsRefuse(&a));
    svs.time = 1000; SV_RecordChallengeTime(&a); lan = true; svs.time = 1010;
    CHECK(!SV_PingLimitsRefuse(&a));
    // Too low a ping is refused and the challenge forgotten.
    lan = false; printed.clear();
    CHECK(SV_PingLimitsRefuse(&a) && printed == "print\nServer is for high pings only\n");
    CHECK(!SV_PingLimitsRefuse(&a));
    // A retry measures from the newest challenge; too high is refused and kept.
    svs.time = 2000; SV_RecordChallengeTime(&a); svs.time = 2300;
    CHECK(SV_PingLimitsRefuse(&a) && printed == "print\nServer is for low pings only\n");
    CHECK(SV_PingLimitsRefuse(&a));
    svs.time = 3000; SV_RecordChallengeTime(&a); svs.time = 3150;
    CHECK(!SV_PingLimitsRefuse(&a));
    // Another address keeps its own time; limits off admit everyone.
    svs.time = 4000; SV_RecordChallengeTime(&b); svs.time = 4001;
    CHECK(SV_PingLimitsRefuse(&b));
    SV_RecordChallengeTime(&b); svs.time = 4002;
    minValue.integer = maxValue.integer = 0;
    CHECK(!SV_PingLimitsRefuse(&b));
    // The table keeps the 64 most recent addresses.
    minValue.integer = 1000;
    for (int i = 0; i < 64; ++i) { netadr_t c{100 + i, 1}; SV_RecordChallengeTime(&c); }
    CHECK(!SV_PingLimitsRefuse(&b));
    const netadr_t newest{163, 1};
    CHECK(SV_PingLimitsRefuse(&newest));
    // Six profiles carry the limits; Quake Live and the rest do not.
    int count = 0;
    for (id = 0; id <= ID_Q3TEST; ++id) count += SV_ProfileHasPingLimits() ? 1 : 0;
    CHECK(count == 6);
    id = ID_QUAKELIVE; CHECK(!SV_ProfileHasPingLimits());
    id = ID_JK2_MP; CHECK(SV_ProfileHasPingLimits());
    std::printf("ok\n");
}
'''
        self.assertEqual(self.compile_run(source).strip(), 'ok')
        connect = function(client, 'void SV_DirectConnect( const netadr_t *from )')
        self.assertLess(connect.index('if ( !SV_VerifyChallenge( challenge, from ) )'),
                        connect.index('if ( SV_PingLimitsRefuse( from ) ) {'))
        challenge = function(client, 'void SV_GetChallenge( const netadr_t *from, const msg_t *msg )')
        self.assertLess(challenge.index('SV_RecordChallengeTime( from );'), challenge.index('challengeResponse'))
        info = function(main, 'static void SVC_Info( const netadr_t *from )')
        self.assertIn('"minPing", va( "%i", sv_minPing->integer )', info)
        self.assertIn('"maxPing", va( "%i", sv_maxPing->integer )', info)
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        self.assertIn('Cvar_Get( "sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO )', init)
        self.assertIn('Cvar_Get( "sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO )', init)

    def test_a_listen_server_answers_remote_clients(self):
        # Info and challenge queries are refused while ui_singlePlayerActive
        # is set; only Quake Live's local match sets it from a client's map.
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
        start = commands.index('static void SV_Map_f( void ) {')
        body = commands[start:commands.index('\n}\n', start)]
        self.assertIn('if ( quakeLive && !com_dedicated->integer && !singlePlayerCommand ) {\n'
                      '\t\tCvar_Set( "ui_singlePlayerActive", "1" );', body)
        self.assertEqual(body.count('Cvar_Set( "ui_singlePlayerActive", "1" );'), 1)
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        info = function(main, 'static void SVC_Info( const netadr_t *from )')
        self.assertIn('Cvar_VariableIntegerValue("ui_singlePlayerActive")', info)

    def test_match_completion_reports_to_masters(self):
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8')
        report = function(main, 'void SV_MasterGameCompleteStatus( void )')
        self.assertIn('if ( !SV_MasterPublishing( netenabled ) )', report)
        self.assertIn('"gameCompleteStatus\\n%s\\n%s"', report)
        self.assertIn('SV_ResolveMaster( i, netenabled )', report)
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
        self.assertIn('if ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) '
                      'Cmd_AddCommand("gameCompleteStatus", SV_GameCompleteStatus_f);', commands)
        self.assertIn('SV_MasterGameCompleteStatus();', function(commands, 'static void SV_GameCompleteStatus_f( void )'))

    def test_a_pure_pak_list_forgets_references_to_paks_it_leaves_out(self):
        # A pak the server does not list cannot be read while its list is in
        # force, so references to it predate the list and must not reach the
        # pure proof: retail servers drop a client naming a pak they lack.
        files = (ROOT / 'code/qcommon/files.c').read_text(encoding='utf-8')
        source = '#include <cstdio>\n#include <cstdlib>\n#include <cstring>\n#include <string>\n#include <vector>\n' + r'''
typedef int qboolean;
enum { qfalse, qtrue };
#define ARRAY_LEN(x) (int)(sizeof(x) / sizeof(*(x)))
struct pack_t { int checksum; int referenced; };
struct searchpath_t { searchpath_t *next; pack_t *pack; };
enum dirPolicy_t { DIR_ALLOW, DIR_DENY };
static searchpath_t *fs_searchpaths;
static int fs_serverPaks[1024], fs_numServerPaks, fs_checksumFeed, restarts;
static char *fs_serverPakNames[1024];
static qboolean fs_reordered;
static std::vector<std::string> args;
static void Cmd_TokenizeString(const char *text) {
    args.clear(); std::string word;
    for (const char *p = text; ; ++p) {
        if (!*p || *p == ' ') { if (!word.empty()) args.push_back(word); word.clear(); if (!*p) break; }
        else word += *p;
    }
}
static int Cmd_Argc(void) { return int(args.size()); }
static const char *Cmd_Argv(int i) { return i < int(args.size()) ? args[i].c_str() : ""; }
static void FS_SetDirPolicy(dirPolicy_t) {}
static void Com_DPrintf(const char *, ...) {}
static qboolean ritual;
static qboolean Com_GameIsRitual(void) { return ritual; }
static void FS_Restart(int) { ++restarts; }
static void Z_Free(void *p) { std::free(p); }
static char *FS_CopyString(const char *s) { char *c = (char *)std::malloc(std::strlen(s) + 1); std::strcpy(c, s); return c; }
''' + function(files, 'static qboolean FS_PakIsPure( const pack_t *pack )') + '\n' + \
            function(files, 'void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames )') + r'''
#define CHECK(x) do { if (!(x)) { std::printf("line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    pack_t a{100, 1}, b{200, 3}, c{300, 1};
    searchpath_t pc{nullptr, &c}, pb{&pc, &b}, pa{&pb, &a};
    fs_searchpaths = &pa;
    FS_PureServerSetLoadedPaks("100 200", "pak0 pak1");
    CHECK(fs_numServerPaks == 2 && a.referenced == 1 && b.referenced == 3 && c.referenced == 0);
    c.referenced = 1;
    FS_PureServerSetLoadedPaks("", "");
    CHECK(fs_numServerPaks == 0 && c.referenced == 1 && !restarts);
    fs_reordered = qtrue;
    FS_PureServerSetLoadedPaks("", "");
    CHECK(restarts == 1 && c.referenced == 1);
    // Ritual's engines keep the first 64 checksums (alice.exe 0x0043e426).
    std::string many;
    for (int i = 0; i < 70; ++i) many += std::to_string(1000 + i) + ' ';
    FS_PureServerSetLoadedPaks(many.c_str(), "");
    CHECK(fs_numServerPaks == 70);
    ritual = qtrue;
    FS_PureServerSetLoadedPaks(many.c_str(), "");
    CHECK(fs_numServerPaks == 64 && fs_serverPaks[63] == 1063);
    std::printf("ok\n");
}
'''
        self.assertEqual(self.compile_run(source).strip(), 'ok')

    def test_a_latched_client_count_sizes_the_first_client_array(self):
        # A server cfg's sv_maxclients is latched; every retail engine takes it
        # in SV_Startup before allocating clients, so the game's first
        # map_restart (RTCW's and ET's warm-up end) restarts in place.
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        startup = function(init, 'static void SV_Startup( void ) {')
        self.assertIn('SV_AllocClients( SV_BoundMaxClients( 1 ) );', startup)
        self.assertNotIn('SV_AllocClients( sv_maxclients->integer );', startup)
        self.assertLess(init.index('static int SV_BoundMaxClients( int minimum ) {'),
                        init.index('static void SV_Startup( void ) {'))
        bound = function(init, 'static int SV_BoundMaxClients( int minimum ) {')
        # Cvar_Get takes a latched value; the flag it leaves is cleared.
        self.assertLess(bound.index('Cvar_Get( "sv_maxclients"'), bound.index('sv_maxclients->modified = qfalse;'))

    def test_raven_console_chat_is_svsay(self):
        # Elite Force, Jedi Academy and Jedi Outcast's engines name their
        # console chat svsay; each refuses unless dedicated and running.
        commands = (ROOT / 'code/server/sv_ccmds.cpp').read_text(encoding='utf-8')
        self.assertIn('if ( Com_GameIsSTVEFMP() || Com_GameIsJKAMP() || Com_GameIsJK2MP() ) '
                      'Cmd_AddCommand("svsay", SV_ConSvSay_f);',
                      function(commands, 'void SV_AddOperatorCommands( void ) {'))
        source = '#include <array>\n#include <cstdarg>\n#include <cstdio>\n#include <cstring>\n#include <string>\n' + \
            PREDICATES + r'''
#define MAX_STRING_CHARS 1024
struct cvar_t { int integer; };
struct client_t;
static cvar_t dedicatedValue{1}, runningValue{1};
static cvar_t *com_dedicated = &dedicatedValue, *com_sv_running = &runningValue;
static std::string args, printed, sent;
static int argc;
static int Cmd_Argc(void) { return argc; }
static char *Cmd_ArgsFrom(int) { static char buffer[2048]; std::snprintf(buffer, sizeof buffer, "%s", args.c_str()); return buffer; }
static void Com_Printf(const char *text, ...) { printed += text; }
static void Com_sprintf(char *out, int size, const char *format, ...) {
    va_list list; va_start(list, format); std::vsnprintf(out, size, format, list); va_end(list);
}
static void SV_SendServerCommand(client_t *, const char *format, ...) {
    char buffer[2048]; va_list list; va_start(list, format); std::vsnprintf(buffer, sizeof buffer, format, list);
    va_end(list); sent = buffer;
}
template <typename T, std::size_t N> static constexpr int SV_ArraySize(const std::array<T, N> &) { return int(N); }
''' + function(commands, 'static void SV_ConSvSay_f( void )') + r'''
#define CHECK(x) do { if (!(x)) { std::printf("line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static void run(const char *text, int count) { args = text; argc = count; printed.clear(); sent.clear(); SV_ConSvSay_f(); }
int main() {
    id = ID_STVEF_MP;
    run("hello there", 3);
    CHECK(sent == "chat \"console: hello there\n\"" && printed.empty());
    run("\"quoted words\"", 2);
    CHECK(sent == "chat \"console: quoted words\n\"");
    run("", 1);
    CHECK(sent.empty());
    id = ID_JK2_MP; run("hi", 2); CHECK(sent == "chat \"console: hi\n\"");
    id = ID_JKA_MP; run("hi", 2); CHECK(sent == "chat \"Server: hi\n\"");
    runningValue.integer = 0; run("hi", 2);
    CHECK(sent.empty() && printed == "Server is not running.\n");
    dedicatedValue.integer = 0; run("hi", 2);
    CHECK(sent.empty() && printed == "Server is not dedicated.\n");
    std::printf("ok\n");
}
'''
        self.assertEqual(self.compile_run(source).strip(), 'ok')


if __name__ == '__main__':
    unittest.main()
