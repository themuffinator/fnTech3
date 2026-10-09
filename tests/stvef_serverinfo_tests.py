"""Holomatch's serverinfo holds retail's keys.

stvoyHM.exe 1.20's SV_Init (0x433e00..0x434180) registers sv_pure archived,
in its serverinfo and its systeminfo (0x433f88), and registers none of
fnTech3's own sv_dlURL, sv_dlRate, sv_minRate and sv_playingDemo. The
production SV_HolomatchInfoKeys is compiled with stubbed cvars; the profile
side of sv_pure is in tests/rtcw_server_policy_tests.py. No server is started.
"""
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
    return source[start:source.index('\n}\n', start) + 3]


HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
constexpr int CVAR_ARCHIVE = 1, CVAR_SERVERINFO = 4, CVAR_SYSTEMINFO = 8, CVAR_ROM = 64,
    CVAR_ARCHIVE_ND = 0x4000;
struct cvar_t { int flags; };
static std::map<std::string, cvar_t> cvars;
static cvar_t *Cvar_Get( const char *name, const char *, int flags ) {
    auto found = cvars.find( name );
    if ( found == cvars.end() ) found = cvars.emplace( name, cvar_t{ flags } ).first;
    else found->second.flags |= flags;
    return &found->second;
}

PRODUCTION

int main() {
    // SV_Init's registrations, then the profile's own keys taken out.
    const std::map<std::string, int> registered = {
        { "sv_dlURL", CVAR_SERVERINFO | CVAR_ARCHIVE }, { "sv_dlRate", CVAR_ARCHIVE | CVAR_SERVERINFO },
        { "sv_minRate", CVAR_ARCHIVE_ND | CVAR_SERVERINFO }, { "sv_playingDemo", CVAR_SERVERINFO | CVAR_ROM },
        { "sv_allowDownload", CVAR_SERVERINFO }, { "sv_hostname", CVAR_SERVERINFO | CVAR_ARCHIVE },
        { "sv_pure", CVAR_SYSTEMINFO | CVAR_ARCHIVE | CVAR_SERVERINFO } };
    for ( const auto &entry : registered ) Cvar_Get( entry.first.c_str(), "", entry.second );
    SV_HolomatchInfoKeys();
    for ( const auto &entry : registered ) {
        const bool own = entry.first == "sv_dlURL" || entry.first == "sv_dlRate" || entry.first == "sv_minRate"
            || entry.first == "sv_playingDemo";
        assert( cvars.at( entry.first ).flags == ( own ? entry.second & ~CVAR_SERVERINFO : entry.second ) );
    }
    // Run again, nothing changes; no other cvar is made.
    SV_HolomatchInfoKeys();
    assert( cvars.size() == registered.size() );
    std::puts( "Holomatch serverinfo keys passed" );
}
'''


class STVEFServerinfoTests(unittest.TestCase):
    def setUp(self):
        self.init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')

    def test_the_engine_keeps_its_own_keys_out(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        production = function(self.init, 'static void SV_HolomatchInfoKeys( void ) {')
        with tempfile.TemporaryDirectory(prefix='stvef-serverinfo-') as folder:
            source = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(HARNESS.replace('PRODUCTION', production), encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Wextra',
                                    str(source), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_holomatch_alone_takes_them_out_after_registering_them(self):
        init = function(self.init, 'void SV_Init( void )\n{')
        call = init.index('\tif ( Com_GameIsSTVEFMP() ) {\n\t\tSV_HolomatchInfoKeys();\n\t}\n')
        for registration in ('Cvar_Get( "sv_playingDemo", "0", CVAR_SERVERINFO | CVAR_ROM )',
                             'sv_minRate = Cvar_Get( "sv_minRate"', 'sv_dlRate = Cvar_Get( "sv_dlRate"',
                             'Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);'):
            self.assertLess(init.index(registration), call)
        self.assertEqual(self.init.count('SV_HolomatchInfoKeys();'), 1)
        # No other server source puts them back into the serverinfo.
        for name in ('code/server/sv_main.cpp', 'code/server/sv_ccmds.cpp', 'code/server/sv_client.cpp'):
            text = (ROOT / name).read_text(encoding='utf-8')
            for key in ('"sv_dlURL"', '"sv_dlRate"', '"sv_minRate"', '"sv_playingDemo"'):
                for line in text.splitlines():
                    if key in line and 'Cvar_Get' in line:
                        self.assertNotIn('CVAR_SERVERINFO', line, f'{name}: {line.strip()}')


if __name__ == '__main__':
    unittest.main()
