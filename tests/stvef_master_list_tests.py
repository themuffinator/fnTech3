"""Read Holomatch master server lists as stvoyHM.exe 1.20 does.

Retail Holomatch reads a master's getserversResponse as text: after each
backslash eight hex digits of address and four of port, ending at "\\EOT"
(CL_ServersResponsePacket 0x40e6e0). These tests compile the production
parser (code/client/client_stvef_masters.hpp) and run it on synthetic lists
with documentation addresses, well-formed and damaged, and check that the
client hands Holomatch's lists to it
(docs/fntech3/2026-10-24-stvef-playability.md).
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
    opening = source.index('{', start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError(signature)


CHECKS = r'''
#include "code/client/client_stvef_masters.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int Parse( const std::string &text, std::vector<STVEFMasterEntry> &out, int max = 256 ) {
    // an exact-size copy, so a read past the packet is a sanitizer error
    std::vector<std::uint8_t> bytes( text.begin(), text.end() );
    out.assign( max > 0 ? max : 1, STVEFMasterEntry{} );
    const std::uint8_t *data = bytes.empty() ? nullptr : bytes.data();
    return CL_STVEFParseMasterList( data, data ? data + bytes.size() : nullptr, out.data(), max );
}

static void Expect( const STVEFMasterEntry &e, int a, int b, int c, int d, int port ) {
    assert( e.ip[0] == a && e.ip[1] == b && e.ip[2] == c && e.ip[3] == d );
    assert( e.port == port );
}

int main() {
    const std::string head = std::string( 4, '\xff' ) + "getserversResponse ";
    std::vector<STVEFMasterEntry> out;
    // two servers, upper and lower case, ended by \EOT and its padding
    assert( Parse( head + "\\C00002016D38\\c63364076d39\\EOT" + std::string( 3, '\0' ), out ) == 2 );
    Expect( out[0], 192, 0, 2, 1, 27960 );
    Expect( out[1], 198, 51, 100, 7, 27961 );
    // no \EOT: an entry counts only when a backslash follows it
    assert( Parse( head + "\\C00002016D38\\CB0071096D38\\", out ) == 2 );
    Expect( out[1], 203, 0, 113, 9, 27960 );
    assert( Parse( head + "\\C00002016D38\\CB0071096D38", out ) == 1 );
    // a short entry at the end of the packet is not read past
    for ( int cut = 1; cut < 13; ++cut ) {
        assert( Parse( head + "\\C00002016D38\\" + std::string( "CB0071096D38" ).substr( 0, cut ), out ) == 1 );
    }
    // a character that is not a hex digit ends the list
    assert( Parse( head + "\\C00002016D38\\C0000G016D38\\C00002036D38\\", out ) == 1 );
    // Quake III's binary entries are not Holomatch's
    assert( Parse( head + std::string( "\\\xc0\x00\x02\x01\x6d\x38\\EOT", 11 ), out ) == 0 );
    // an empty list, nothing before the first backslash, nothing at all
    assert( Parse( head + "\\EOT" + std::string( 3, '\0' ), out ) == 0 );
    assert( Parse( head, out ) == 0 );
    assert( Parse( "", out ) == 0 );
    assert( CL_STVEFParseMasterList( nullptr, nullptr, nullptr, 4 ) == 0 );
    // text before the first entry is skipped, as retail skips to its first backslash
    assert( Parse( head + "junk\\C00002016D38\\", out ) == 1 );
    // at most `max` entries a packet: retail stops at 256
    std::string many = head;
    for ( int i = 0; i < 300; ++i ) {
        char entry[16];
        std::snprintf( entry, sizeof( entry ), "\\C00002%02X6D38", i & 0xff );
        many += entry;
    }
    many += "\\";
    assert( Parse( many, out, 256 ) == 256 );
    Expect( out[255], 192, 0, 2, 255, 27960 );
    assert( Parse( many, out, 3 ) == 3 );
    assert( Parse( many, out, 0 ) == 0 );
    std::puts( "ok" );
    return 0;
}
'''


class STVEFMasterListTests(unittest.TestCase):
    def test_parser_reads_retail_text_lists(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-masters-', dir=ROOT / '.tmp') as folder:
            cpp, exe = Path(folder) / 'probe.cpp', Path(folder) / 'probe.exe'
            cpp.write_text(CHECKS, encoding='utf-8')
            sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', '-Wall',
                                    '-D_CRT_SECURE_NO_WARNINGS', *sanitizer, '-I', str(ROOT), str(cpp), '-o',
                                    str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertEqual(ran.stdout.strip(), 'ok')

    def test_client_reads_holomatch_lists_as_text(self):
        client = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        self.assertIn('#include "client_stvef_masters.hpp"', client)
        packet = function(client, 'static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {')
        self.assertIn('const bool holomatchText = Com_GameIsSTVEFMP() && !extended;', packet)
        self.assertIn('numservers = CL_STVEFMasterAddresses( buffptr, buffend, addresses );', packet)
        self.assertIn('while (!holomatchText && buffptr + 1 < buffend)', packet)
        convert = function(client, 'static int CL_STVEFMasterAddresses( const byte *buffptr, const byte *buffend, netadr_t *addresses )')
        self.assertIn('CL_STVEFParseMasterList( buffptr, buffend, entries, kMaxServersPerPacket )', convert)
        self.assertIn('addresses[i].type = NA_IP;', convert)
        self.assertIn('addresses[i].port = BigShort( static_cast<short>( entries[i].port ) );', convert)


if __name__ == '__main__':
    unittest.main()
