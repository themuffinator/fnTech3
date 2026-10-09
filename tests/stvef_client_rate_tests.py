"""Holomatch reads a client's rate and snapshot rate as stvoyHM.exe does.

stvoyHM.exe 1.20's SV_UserinfoChanged (0x4306ca..0x4307bb) gives a LAN client
a rate of 99999 and anyone else its own, held to 1000..90000, or 3000 when it
names none; `snaps` is held to 1..30 a second, or 20 when absent, and a change
leaves the deadline already set. Both Elite Force servers take any sv_fps from
1 (stvoy.exe 1.20 0x426633, stvoyHM.exe 0x43523b). The production
SV_HolomatchRateAndSnaps and the shared SV_ClientRateAndSnaps are compiled
with stubs; which one a profile uses, and sv_fps's range, are checked in the
source. No server is started.
"""
from pathlib import Path
import os
import re
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
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
enum { NA_BOT, NA_BAD, NA_LOOPBACK, NA_IP };
struct netadr_t { int type = NA_IP; };
struct client_t {
    char userinfo[256] = "";
    int rate = -1, snapshotMsec = -1, lastSnapshotTime = 777;
    struct { bool isLANAddress = false; netadr_t remoteAddress; } netchan;
};
struct cvar_t { int integer = 0; };
static cvar_t com_dedicatedValue, sv_lanForceRateValue, sv_maxRateValue, sv_minRateValue, sv_fpsValue;
static cvar_t *com_dedicated = &com_dedicatedValue, *sv_lanForceRate = &sv_lanForceRateValue,
    *sv_maxRate = &sv_maxRateValue, *sv_minRate = &sv_minRateValue, *sv_fps = &sv_fpsValue;
static struct { int time; } svs = { 100000 };
static std::string found;
static const char *Info_ValueForKey( const char *info, const char *key ) {
    // "\key\value\key\value", as the userinfo strings below are written
    found.clear();
    std::string text = info;
    for ( std::size_t at = 0; at < text.size() && text[at] == '\\'; ) {
        const std::size_t keyEnd = text.find( '\\', at + 1 );
        if ( keyEnd == std::string::npos ) break;
        std::size_t valueEnd = text.find( '\\', keyEnd + 1 );
        if ( valueEnd == std::string::npos ) valueEnd = text.size();
        if ( text.compare( at + 1, keyEnd - at - 1, key ) == 0 ) {
            found = text.substr( keyEnd + 1, valueEnd - keyEnd - 1 );
            break;
        }
        at = valueEnd;
    }
    return found.c_str();
}
static int SV_ParseInt( const char *text ) { return std::atoi( text ); }
static bool NET_IsLocalAddress( const netadr_t *address ) { return address->type == NA_LOOPBACK; }
constexpr bool qtrue = true, qfalse = false;

PRODUCTION

static client_t Holomatch( const char *userinfo, bool lan = false, int type = NA_IP ) {
    client_t client;
    std::strcpy( client.userinfo, userinfo );
    client.netchan.isLANAddress = lan;
    client.netchan.remoteAddress.type = type;
    SV_HolomatchRateAndSnaps( &client );
    assert( client.lastSnapshotTime == 777 );
    return client;
}

int main() {
    // Rate: 3000 unless named; held to 1000..90000; 99999 on the LAN or loopback.
    assert( Holomatch( "" ).rate == 3000 );
    assert( Holomatch( "\\rate\\" ).rate == 3000 );
    const struct { const char *value; int rate; } rates[] = {
        { "500", 1000 }, { "999", 1000 }, { "1000", 1000 }, { "25000", 25000 }, { "90000", 90000 },
        { "90001", 90000 }, { "abc", 1000 }, { "-5", 1000 } };
    for ( const auto &row : rates ) {
        assert( Holomatch( ( std::string( "\\rate\\" ) + row.value ).c_str() ).rate == row.rate );
    }
    assert( Holomatch( "\\rate\\500", true ).rate == 99999 );
    assert( Holomatch( "\\rate\\500", false, NA_LOOPBACK ).rate == 99999 );
    // sv_maxRate holds the rate when a snapshot's wait is reckoned, not here.
    sv_maxRate->integer = 4000;
    assert( Holomatch( "\\rate\\25000" ).rate == 25000 );
    sv_maxRate->integer = 0;
    // Snaps: 20 a second unless named, held to 1..30, whatever sv_fps is.
    sv_fps->integer = 10;
    assert( Holomatch( "" ).snapshotMsec == 50 );
    assert( Holomatch( "\\snaps\\" ).snapshotMsec == 50 );
    const struct { const char *value; int msec; } snaps[] = {
        { "0", 1000 }, { "-3", 1000 }, { "1", 1000 }, { "7", 142 }, { "20", 50 }, { "30", 33 }, { "40", 33 } };
    for ( const auto &row : snaps ) {
        assert( Holomatch( ( std::string( "\\snaps\\" ) + row.value ).c_str() ).snapshotMsec == row.msec );
        assert( Holomatch( ( std::string( "\\snaps\\" ) + row.value ).c_str(), true ).snapshotMsec == row.msec );
    }

    // The shared rule the other profiles keep: no choke on the LAN, 10000
    // unless named, sv_maxRate and sv_minRate applied, snaps held to sv_fps
    // and the next snapshot made due at once when the interval changes.
    sv_fps->integer = 20; sv_lanForceRate->integer = 1; com_dedicated->integer = 1;
    client_t shared;
    shared.netchan.isLANAddress = true;
    SV_ClientRateAndSnaps( &shared );
    assert( shared.rate == 0 && shared.snapshotMsec == 50 && shared.lastSnapshotTime == svs.time - 9999 );
    shared = client_t{};
    sv_maxRate->integer = 20000; sv_minRate->integer = 5000;
    std::strcpy( shared.userinfo, "\\rate\\90000\\snaps\\40" );
    SV_ClientRateAndSnaps( &shared );
    assert( shared.rate == 20000 && shared.snapshotMsec == 50 );
    shared = client_t{};
    std::strcpy( shared.userinfo, "\\rate\\100\\snaps\\10" );
    SV_ClientRateAndSnaps( &shared );
    assert( shared.rate == 5000 && shared.snapshotMsec == 100 );
    shared = client_t{};
    shared.snapshotMsec = 50; shared.lastSnapshotTime = 5;
    SV_ClientRateAndSnaps( &shared );
    assert( shared.rate == 10000 && shared.lastSnapshotTime == 5 );
    return 0;
}
'''


class STVEFClientRateTests(unittest.TestCase):
    def test_holomatch_reads_rate_and_snaps_as_retail(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        client = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        production = '\n'.join((
            function(client, 'static void SV_ClientRateAndSnaps('),
            function(client, 'static void SV_HolomatchRateAndSnaps('),
        ))
        target = compiler_target_args(compiler)
        sanitizers = (['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
                      if os.name != 'nt' else [])
        with tempfile.TemporaryDirectory(prefix='stvef-client-rate-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(HARNESS.replace('PRODUCTION', production), encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-D_CRT_SECURE_NO_WARNINGS',
                                    *sanitizers, str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_only_holomatch_takes_the_retail_rule(self):
        changed = function((ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8'),
                           'void SV_UserinfoChanged(')
        dispatch = re.search(r'if \( Com_GameIsSTVEFMP\(\) \) \{\s*SV_HolomatchRateAndSnaps\( cl \);\s*\}'
                             r'\s*else \{\s*SV_ClientRateAndSnaps\( cl \);\s*\}', changed)
        self.assertIsNotNone(dispatch)
        self.assertEqual(changed.count('SV_HolomatchRateAndSnaps'), 1)
        self.assertEqual(changed.count('SV_ClientRateAndSnaps'), 1)
        self.assertNotIn('Info_ValueForKey( cl->userinfo, "snaps" )', changed)

    def test_elite_force_takes_any_server_rate_from_one(self):
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8')
        ranges = re.findall(r'Cvar_CheckRange\( sv_fps, "(\d+)", "(\d+)", CV_INTEGER \);', init)
        self.assertEqual(sorted(ranges), [('1', '1000'), ('10', '125')])
        self.assertRegex(init, r'if \( Com_GameIsSTVEF\(\) \) \{\s*Cvar_CheckRange\( sv_fps, "1", "1000", '
                               r'CV_INTEGER \);\s*\} else \{\s*Cvar_CheckRange\( sv_fps, "10", "125", CV_INTEGER \);')


if __name__ == '__main__':
    unittest.main()
