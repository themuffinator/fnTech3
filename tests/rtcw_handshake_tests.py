"""RTCW's handshake token layouts, parsed and produced by the shared helper."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include "code/qcommon/rtcw_handshake.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>
using namespace fntech3::rtcw::handshake;

static ChallengeReply Parse(bool mp, std::initializer_list<const char *> tokens, int nonce,
    bool expected) {
    std::vector<const char *> argv(tokens);
    return ParseChallengeReply(mp, static_cast<int>(argv.size()), argv.data(), nonce, mp ? 60 : 49,
        expected);
}
static bool Connect(std::initializer_list<const char *> tokens, int challenge, int protocol) {
    std::vector<const char *> argv(tokens);
    return AcceptConnectReply(static_cast<int>(argv.size()), argv.data(), challenge, protocol);
}
static std::string Format(int onlyVisible, bool sent, const char *marker) {
    char buffer[128];
    const int length = FormatMultiplayerChallengeReply(buffer, sizeof(buffer), 1234, onlyVisible,
        sent, 777, 60, marker);
    assert(length > 0 && static_cast<std::size_t>(length) < sizeof(buffer));
    return buffer;
}

int main() {
    // Original MP: the challenge alone, taken only from the address asked.
    auto reply = Parse(true, {"challengeResponse", "1234"}, 777, true);
    assert(reply.accepted && reply.challenge == 1234 && reply.onlyVisibleClients == 0);
    assert(reply.protocol == 60 && !reply.advertisedProtocol && !reply.echoedNonce);
    assert(!Parse(true, {"challengeResponse", "1234"}, 777, false).accepted);
    // Original MP with sv_onlyVisibleClients set: the second token is that value,
    // not a nonce echo, so it neither validates nor fails the nonce.
    reply = Parse(true, {"challengeResponse", "1234", "1"}, 777, true);
    assert(reply.accepted && reply.onlyVisibleClients == 1 && !reply.echoedNonce && reply.protocol == 60);
    assert(!Parse(true, {"challengeResponse", "1234", "1"}, 777, false).accepted);
    // iortcw MP: onlyVisibleClients, nonce echo, protocol; a matching echo proves
    // the reply even from a handed-off address, a wrong one never does.
    reply = Parse(true, {"challengeResponse", "1234", "0", "777", "60"}, 777, false);
    assert(reply.accepted && reply.echoedNonce && reply.advertisedProtocol && reply.protocol == 60);
    assert(!Parse(true, {"challengeResponse", "1234", "0", "778", "60"}, 777, true).accepted);
    reply = Parse(true, {"challengeResponse", "1234", "1", "777", "57", "FnQL"}, 777, true);
    assert(reply.accepted && reply.onlyVisibleClients == 1 && reply.protocol == 57);
    // Quake III's layout read as MP would take the nonce for onlyVisibleClients
    // and the protocol for the nonce: it fails, as it must.
    assert(!Parse(true, {"challengeResponse", "1234", "777", "60"}, 777, true).accepted);
    // SP: original bare, iortcw's ioquake3 layout; SP has no onlyVisibleClients.
    reply = Parse(false, {"challengeResponse", "99"}, 5, true);
    assert(reply.accepted && reply.challenge == 99 && reply.protocol == 49 && !reply.echoedNonce);
    reply = Parse(false, {"challengeResponse", "99", "5", "49"}, 5, false);
    assert(reply.accepted && reply.echoedNonce && reply.onlyVisibleClients == 0 && reply.protocol == 49);
    assert(!Parse(false, {"challengeResponse", "99", "6", "49"}, 5, true).accepted);
    assert(!Parse(false, {"challengeResponse", "99"}, 5, false).accepted);
    // Missing challenge token or argv.
    assert(!Parse(true, {"challengeResponse"}, 777, true).accepted);
    assert(!ParseChallengeReply(true, 2, nullptr, 777, 60, true).accepted);

    // connectResponse: bare from original servers, echoed challenge otherwise.
    assert(Connect({"connectResponse"}, 1234, 60));
    assert(Connect({"connectResponse", "1234"}, 1234, 60));
    assert(!Connect({"connectResponse", "1235"}, 1234, 60));
    assert(Connect({"connectResponse", "1234", "60"}, 1234, 60));
    assert(!Connect({"connectResponse", "1234", "68"}, 1234, 60));
    assert(!AcceptConnectReply(1, nullptr, 1234, 60));

    // Server MP replies follow the asking client's layout.
    assert(Format(0, false, nullptr) == "challengeResponse 1234");
    assert(Format(2, false, "FnQL") == "challengeResponse 1234 2");
    assert(Format(0, true, nullptr) == "challengeResponse 1234 0 777 60");
    assert(Format(1, true, "FnQL") == "challengeResponse 1234 1 777 60 FnQL");
    assert(Format(0, true, "") == "challengeResponse 1234 0 777 60");
    // A reply produced for a nonce-sending client parses back as MP.
    const std::string produced = Format(1, true, "FnQL");
    const char *tokens[] = {"challengeResponse", "1234", "1", "777", "60", "FnQL"};
    reply = ParseChallengeReply(true, 6, tokens, 777, 60, false);
    assert(produced.find("1234 1 777 60") != std::string::npos && reply.accepted);
    char tiny[8];
    assert(FormatMultiplayerChallengeReply(tiny, sizeof(tiny), 1234, 0, false, 0, 60, nullptr) > 7);
    assert(std::strlen(tiny) == 7);
}
'''


CULLING = r'''
#include "code/qcommon/q_shared.h"
#ifdef _MSC_SSE2
#include <intrin.h>
#endif
#include <array>
#include <cassert>
#include <vector>
enum { COM_GAME_QUAKE3, COM_GAME_RTCW_MP };
static int game = COM_GAME_RTCW_MP;
static struct { int maxClients; } profile = { 64 };
static const decltype(profile) *Com_GameProfile() { return &profile; }
static int Com_GameIsRTCWMP() { return game == COM_GAME_RTCW_MP; }
static struct { int rtcwOnlyVisibleClients; } clc;
static struct { vec3_t cgameRTCWLerpOrigin;
    struct { struct { int viewheight; vec3_t viewangles; struct { float leanf; } rtcw; } ps; } snap; } cl;
static int clearAt, traced;
static std::vector<int> masks;
static void CM_BoxTrace(trace_t *tr, const vec3_t, const vec3_t, const vec3_t mins, const vec3_t maxs,
    clipHandle_t model, int mask, int capsule) {
    assert(!mins && !maxs && model == 0 && capsule == 0);
    masks.push_back(mask);
    tr->fraction = traced++ == clearAt ? 1.0f : 0.25f;
}
'''

CULLING_CHECKS = r'''
static void Apply(entityState_t &state, int serverTime, int clear) {
    clearAt = clear; traced = 0; masks.clear();
    CL_RTCWApplyOnlyVisibleClients(&state, serverTime);
}
int main() {
    entityState_t player{};
    player.number = 3; player.pos.trBase[0] = 128;
    // Off unless the server asked for it, and only for RTCW MP players.
    Apply(player, 1000, 7); assert(traced == 0 && player.eFlags == 0);
    clc.rtcwOnlyVisibleClients = 1;
    game = COM_GAME_QUAKE3; Apply(player, 1000, 7); assert(traced == 0);
    game = COM_GAME_RTCW_MP;
    entityState_t other = player; other.number = 64; Apply(other, 1000, 7); assert(traced == 0);
    other.number = -1; Apply(other, 1000, 7); assert(traced == 0);
    // Never seen: the time starts at zero, so a hidden player is hidden at once.
    Apply(player, 1000, 7);
    assert(traced == 7 && (player.eFlags & 0x80));
    for (int mask : masks) assert(mask == CONTENTS_SOLID);
    // The first clear line wins and clears the flag, even one the server set.
    for (int line = 0; line < 7; ++line) {
        player.eFlags = 0x80 | 0x4;
        Apply(player, 2000 + line, line);
        assert(traced == line + 1 && player.eFlags == 0x4);
    }
    // Unseen for 600 ms keeps drawing; beyond that hides.
    player.eFlags = 0;
    Apply(player, 2006 + 600, 7); assert(player.eFlags == 0);
    Apply(player, 2006 + 601, 7); assert(player.eFlags == 0x80);
    // Times are kept per player.
    entityState_t second = player; second.number = 4; second.eFlags = 0;
    Apply(second, 2607, 7); assert(second.eFlags == 0x80);
}
'''


class RTCWHandshakeTests(unittest.TestCase):
    def test_only_visible_clients_policy(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        parse = (ROOT / 'code/client/cl_parse.cpp').read_text(encoding='utf-8')
        math = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
        def function(text, signature):
            start = text.index(signature)
            return text[start:text.index('\n}', start) + 2] + '\n'
        start = parse.index('static constexpr int RTCW_EF_NODRAW')
        source = CULLING + function(math, 'float Q_rsqrt( float number )\n{')
        source += function(math, 'void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {')
        source += parse[start:parse.index('static void CL_DeltaEntity(', start)] + CULLING_CHECKS
        # The parser applies it to every kept entity, after the removal check.
        delta = function(parse, 'static void CL_DeltaEntity(')
        self.assertLess(delta.index('entity was delta removed'),
                        delta.index('CL_RTCWApplyOnlyVisibleClients( state, frame->serverTime );'))
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-culling-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'culling.cpp'
            binary = Path(folder) / ('culling.exe' if os.name == 'nt' else 'culling')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp),
                                    '-o', str(binary)], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_client_and_server_use_rtcw_layouts(self):
        client = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        server = (ROOT / 'code/server/sv_client.cpp').read_text(encoding='utf-8')
        # Q3's authorizer is Q3's alone; RTCW names its own game in getchallenge.
        self.assertIn('if ( Com_GameIsQuake3() && com_protocol->integer <= OLD_PROTOCOL_VERSION &&', client)
        self.assertIn('gameName = fntech3::rtcw::handshake::MultiplayerGameName;', client)
        self.assertIn('gameName = fntech3::rtcw::handshake::SinglePlayerGameName;', client)
        # Both replies route RTCW through the helper before any Q3 token parsing.
        response = client[client.index('if ( !Q_stricmp(c, "challengeResponse" ) ) {'):]
        self.assertLess(response.index('fntech3::rtcw::handshake::ParseChallengeReply('),
                        response.index('c = Cmd_Argv( 2 );'))
        connect = client[client.index('if ( !Q_stricmp(c, "connectResponse") ) {'):]
        self.assertLess(connect.index('fntech3::rtcw::handshake::AcceptConnectReply('),
                        connect.index('Bad connectResponse received.'))
        challenge = server[server.index('void SV_GetChallenge('):]
        self.assertLess(challenge.index('if ( Com_GameIsRTCWMP() ) {'),
                        challenge.index('// legacy client query, don\'t send unneeded information'))
        self.assertIn('FormatMultiplayerChallengeReply(', challenge[:challenge.index('SV_IsBanned')])
    def test_original_and_iortcw_layouts_for_both_modes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-handshake-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'handshake.cpp'
            binary = Path(folder) / ('handshake.exe' if os.name == 'nt' else 'handshake')
            source.write_text(CHECKS, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall',
                                    '-Wextra', '-Werror', '-Wno-format-truncation', '-I', str(ROOT),
                                    str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
