"""Ritual's connectionless handshake, as retail fakk2.exe 1.02 speaks it.

A retail F.A.K.K.2 client asked an fnTech3 server for a challenge, took the
answer, and was never heard from again: it prints its connect as plain text,
and the server Huffman-decoded every connect but Elite Force's, which failed
and dropped the packet before it was logged. fnTech3's own client compressed
its connect and asked for protocol 68, which the server refused. Retail sends
`connect "<userinfo>"` as a plain print with protocol 1 (0x004215ef), its
server reads it with no Huffman stage (0x00447100) and answers a bare
`connectResponse` (0x004439fd). These checks hold the server and client to
that, and compile code/qcommon/ritual_handshake.hpp's reply rules.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


PROBE = r'''
#include "code/qcommon/ritual_handshake.hpp"
#include <cstdio>

namespace hs = fntech3::ritual::handshake;

static int failures;
#define CHECK( x ) do { if ( !( x ) ) { std::printf( "failed: %s\n", #x ); ++failures; } } while ( 0 )

int main() {
	const char *retail[] = { "challengeResponse", "123" };
	auto reply = hs::ParseChallengeReply( 2, retail, 5, true );
	CHECK( reply.accepted && reply.challenge == 123 && reply.advertisedProtocol == 0 );
	// A bare reply proves nothing from another address.
	CHECK( !hs::ParseChallengeReply( 2, retail, 5, false ).accepted );
	const char *echoed[] = { "challengeResponse", "123", "5", "1", "FnQL" };
	reply = hs::ParseChallengeReply( 5, echoed, 5, false );
	CHECK( reply.accepted && reply.challenge == 123 && reply.advertisedProtocol == 1 );
	const char *wrong[] = { "challengeResponse", "123", "6" };
	CHECK( !hs::ParseChallengeReply( 3, wrong, 5, true ).accepted );
	CHECK( !hs::ParseChallengeReply( 1, retail, 5, true ).accepted );
	CHECK( !hs::ParseChallengeReply( 2, nullptr, 5, true ).accepted );

	const char *bare[] = { "connectResponse" };
	CHECK( hs::AcceptConnectReply( 1, bare, 77 ) );
	const char *ours[] = { "connectResponse", "77" };
	CHECK( hs::AcceptConnectReply( 2, ours, 77 ) );
	CHECK( !hs::AcceptConnectReply( 2, ours, 78 ) );
	CHECK( !hs::AcceptConnectReply( 0, bare, 77 ) );
	return failures ? 1 : 0;
}
'''


class RitualHandshakeTests(unittest.TestCase):
    def test_the_reply_rules(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-handshake-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.cpp'
            source.write_text(PROBE, encoding='utf-8')
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Werror',
                                    '-I', str(ROOT), str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_the_server_reads_a_plain_connect_and_answers_bare(self):
        packet = function(read('code/server/sv_main.cpp'),
                          'static void SV_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {')
        self.assertIn('if ( !Com_GameIsSTVEFMP() && !Com_GameIsRitual() ) {', packet)
        self.assertLess(packet.index('!Com_GameIsRitual()'), packet.index('Huff_Decompress( msg, header + 8 )'))
        client = read('code/server/sv_client.cpp')
        self.assertIn('if ( ( cl_proto == QL_RETAIL_PROTOCOL_VERSION && !longstr ) || Com_GameIsRitual()\n'
                      '\t\t|| Com_GameIsJK2MP() ) {\n'
                      '\t\tNET_OutOfBandPrint( NS_SERVER, from, "connectResponse" );', client)

    def test_the_client_prints_its_connect_with_its_own_protocol(self):
        main = read('code/client/cl_main.cpp')
        self.assertIn('#include "../qcommon/ritual_handshake.hpp"', main)
        self.assertIn('if ( Com_GameIsSTVEFMP() || Com_GameIsRitual() ) {', main)
        challenge = main.index('fntech3::ritual::handshake::ParseChallengeReply( count,')
        branch = main[main.rindex('if ( Com_GameIsRitual() ) {', 0, challenge):]
        branch = branch[:branch.index('return true;')]
        for line in ('clc.compat = qfalse;', 'clc.handshakeProtocol = ownProtocol;',
                     'const int ownProtocol = Com_GameProfile()->preferredProtocol;'):
            self.assertIn(line, branch)
        # Its own branch comes before the Quake III fallback that sends 68.
        self.assertLess(challenge, main.index('clc.compat = qtrue;\n\t\ts = Cmd_Argv( 3 );'))
        connect = main.index('fntech3::ritual::handshake::AcceptConnectReply( count, tokens.data(), clc.challenge )')
        self.assertLess(main.rindex('} else if ( Com_GameIsRitual() ) {', 0, connect), connect)
        self.assertLess(connect, main.index('} else if ( !clc.compat &&\n\t\t\t( !contract ||'))


if __name__ == '__main__':
    unittest.main()
