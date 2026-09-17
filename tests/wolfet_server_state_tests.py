"""Exercise ET temporary bans, the binary message channel and action bits."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "code/server/server.h"
#include "code/server/server_wolfet_client.hpp"
#include "code/compat/wolfet_bot_actions.hpp"
#include <cassert>
#include <cstdarg>

serverStatic_t svs;
static cvar_t maxclients;
cvar_t *sv_maxclients = &maxclients;
static client_t clients[FNTECH3_WOLFET_MAX_CLIENTS];
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
qboolean NET_CompareAdr(const netadr_t *a, const netadr_t *b) {
 return (a->type == b->type && a->port == b->port
  && !std::memcmp(a->ipv._4, b->ipv._4, sizeof(a->ipv._4))) ? qtrue : qfalse;
}
template<class F> static void rejected(F f) {
 bool threw = false; try { f(); } catch (Drop &) { threw = true; } assert(threw);
}
static netadr_t Address(unsigned char last, unsigned short port = 27960) {
 netadr_t address{}; address.type = NA_IP; address.port = port;
 address.ipv._4[0] = 10; address.ipv._4[1] = 0; address.ipv._4[2] = 0; address.ipv._4[3] = last;
 return address;
}

int main() {
 using namespace fntech3::wolfet::server;
 svs.clients = clients;
 maxclients.integer = FNTECH3_WOLFET_MAX_CLIENTS;

 // Temporary bans: duration, expiry and address matching.
 Reset(); svs.time = 1000;
 const netadr_t first = Address(1), second = Address(2), otherPort = Address(1, 27961);
 assert(!TempBanIsBanned(first));
 TempBanAddress(first, 5);
 assert(TempBanIsBanned(first));
 assert(!TempBanIsBanned(second) && !TempBanIsBanned(otherPort));
 svs.time = 1000 + 5 * 1000;          // Expiry is inclusive of the end time.
 assert(!TempBanIsBanned(first));
 svs.time = 1000;

 // Nonpositive and unrepresentable durations record no ban at all.
 Reset();
 for (int invalid : {0, -1, INT_MIN, 0x7fffffff, 0x7fffffff / 1000 + 1}) {
  TempBanAddress(second, invalid);
  assert(!TempBanIsBanned(second));
 }

 // Every slot fills, then the oldest unexpired entry is replaced, not dropped.
 Reset();
 for (int index = 0; index < MaxTempBans; ++index)
  TempBanAddress(Address(static_cast<unsigned char>(index)), 10 + index);
 for (int index = 0; index < MaxTempBans; ++index)
  assert(TempBanIsBanned(Address(static_cast<unsigned char>(index))));
 const netadr_t extra = Address(200);
 TempBanAddress(extra, 30);
 assert(TempBanIsBanned(extra));
 // The evicted entry is the shortest remaining one, and keeps full precision.
 assert(!TempBanIsBanned(Address(0)));
 assert(TempBanIsBanned(Address(1)));
 svs.time = 1000 + 30 * 1000 - 1;
 assert(TempBanIsBanned(extra));
 svs.time = 1000 + 30 * 1000;
 assert(!TempBanIsBanned(extra));
 svs.time = 1000;

 // Binary messages: queued, overflowed, replaced and cleared.
 Reset();
 assert(BinaryMessageStatus(0) == MessageEmpty);
 const char payload[] = "abcd";
 SendBinaryMessage(0, payload, 4);
 assert(BinaryMessageStatus(0) == MessageWaiting);
 assert(Storage().messages[0].length == 4);
 assert(!std::memcmp(Storage().messages[0].data.data(), payload, 4));
 assert(BinaryMessageStatus(1) == MessageEmpty);
 Storage().messages[0].overflowed = true;
 assert(BinaryMessageStatus(0) == MessageWaitingOverflow);
 SendBinaryMessage(0, payload, 1);   // A new message clears the overflow report.
 assert(BinaryMessageStatus(0) == MessageWaiting && Storage().messages[0].length == 1);
 SendBinaryMessage(0, nullptr, 0);
 assert(BinaryMessageStatus(0) == MessageEmpty);
 ClearBinaryMessage(0);
 assert(BinaryMessageStatus(0) == MessageEmpty);

 // A full-size payload is accepted; one byte more is refused and left empty.
 static char full[MaxBinaryMessage];
 std::memset(full, 0x5a, sizeof(full));
 SendBinaryMessage(0, full, MaxBinaryMessage);
 assert(BinaryMessageStatus(0) == MessageWaiting);
 rejected([&] { SendBinaryMessage(0, full, MaxBinaryMessage + 1); });
 assert(BinaryMessageStatus(0) == MessageEmpty);
 rejected([&] { SendBinaryMessage(0, full, -1); });
 rejected([&] { SendBinaryMessage(0, nullptr, 4); });

 // Client indices outside the running server never touch storage.
 for (int invalid : {-1, int(FNTECH3_WOLFET_MAX_CLIENTS), INT_MAX, INT_MIN}) {
  rejected([&] { SendBinaryMessage(invalid, payload, 1); });
  assert(BinaryMessageStatus(invalid) == qfalse);
  ClearBinaryMessage(invalid);
 }
 maxclients.integer = 4;
 rejected([&] { SendBinaryMessage(4, payload, 1); });
 assert(BinaryMessageStatus(4) == qfalse);
 maxclients.integer = FNTECH3_WOLFET_MAX_CLIENTS;
 svs.clients = nullptr;
 rejected([&] { SendBinaryMessage(0, payload, 1); });
 svs.clients = clients;

 // ET action bits: fifteen external flags including prone, unknown bits kept.
 using namespace fntech3::wolfet::bot;
 fntech3_wolfet_bot_input_t input{};
 input.thinktime = 0.5f; input.speed = 320.0f; input.weapon = 3;
 input.dir[0] = 1.0f; input.viewangles[1] = 90.0f;
 input.actionflags = static_cast<int>(0x4000u | 0x1u | 0x10000u);
 bot_input_t converted{};
 assert(ActionInput(input, converted));
 assert(converted.actionflags & ACTION_WOLFET_PRONE);
 assert(converted.actionflags & ACTION_ATTACK);
 assert(converted.thinktime == 0.5f && converted.speed == 320.0f && converted.weapon == 3);
 const auto round = ActionOutput(converted, static_cast<unsigned>(input.actionflags));
 assert((static_cast<unsigned>(round.actionflags) & KnownActionBits)
  == (static_cast<unsigned>(input.actionflags) & KnownActionBits));
 assert(static_cast<unsigned>(round.actionflags) & 0x10000u);   // Unknown bit survives.
 assert(round.weapon == 3 && round.dir[0] == 1.0f && round.viewangles[1] == 90.0f);

 // Every external bit maps to a distinct engine bit.
 unsigned seen = 0;
 for (unsigned bit = 0; bit < ActionBitCount; ++bit) {
  assert(!(seen & ActionBits[bit]));
  seen |= ActionBits[bit];
 }

 // Non-finite scalars and vectors are refused before the botlib sees them.
 for (unsigned pattern : {0x7f800000u, 0x7fc00000u, 0xff800000u}) {
  fntech3_wolfet_bot_input_t bad{};
  std::memcpy(&bad.thinktime, &pattern, sizeof(pattern));
  assert(!ActionInput(bad, converted));
  bad = {}; std::memcpy(&bad.speed, &pattern, sizeof(pattern));
  assert(!ActionInput(bad, converted));
  bad = {}; std::memcpy(&bad.dir[2], &pattern, sizeof(pattern));
  assert(!ActionInput(bad, converted));
  bad = {}; std::memcpy(&bad.viewangles[0], &pattern, sizeof(pattern));
  assert(!ActionInput(bad, converted));
 }
 return 0;
}
'''


def _module_call_names(direction):
    text = (ROOT/'code/compat/wolfet_module_calls.def').read_text(encoding='utf-8')
    return [m.group(1) for m in re.finditer(
        r'FNTECH3_WOLFET_MODULE_CALL\(GAME, %s, (\w+), \d+\)' % direction, text)]


def _handled_calls():
    handled = set()
    for name in ('server_wolfet_game.hpp', 'server_wolfet_botlib.hpp'):
        text = (ROOT/'code/server'/name).read_text(encoding='utf-8')
        # Skip preprocessor lines so the ET_SHARED definition is not read as a call.
        body = '\n'.join(l for l in text.splitlines() if not l.lstrip().startswith('#'))
        handled |= set(re.findall(r'case Call::(\w+)', body))
        handled |= set(re.findall(r'ET_SHARED\((\w+)\)', body))
    return handled


class WolfETServerStateTests(unittest.TestCase):
    def test_temp_bans_binary_messages_and_action_bits(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'state.cpp'
            executable = folder/('state.exe' if os.name == 'nt' else 'state')
            source.write_text(PROBE, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT,
                capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_stock_module_imports_are_all_handled(self):
        """Every import the retail ET game module can issue must have a handler."""
        handled = _handled_calls()
        missing = sorted(set(STOCK_MODULE_IMPORTS) - handled)
        self.assertEqual(missing, [], 'unhandled retail ET game imports: %s' % missing)

    def test_declared_imports_are_unique_and_known(self):
        names = _module_call_names('IMPORT')
        self.assertEqual(len(names), len(set(names)))
        unknown = sorted(_handled_calls() - set(names) - set(_module_call_names('EXPORT')))
        self.assertEqual(unknown, [], 'handlers for undeclared ET imports: %s' % unknown)

    def test_original_engine_refusals_are_preserved(self):
        """Ordinals the retail ET engine never implemented must stay refused."""
        handled = _handled_calls()
        wrong = sorted(set(ORIGINAL_ENGINE_REFUSES) & handled)
        self.assertEqual(wrong, [],
            'handlers for ordinals retail ET refuses: %s' % wrong)

    def test_drop_client_carries_the_ban_length(self):
        """ET's three-argument import must not reach the two-argument shared case."""
        text = (ROOT/'code/server/server_wolfet_game.hpp').read_text(encoding='utf-8')
        self.assertNotIn('ET_SHARED(G_DROP_CLIENT)', text)
        self.assertIn('case Call::G_DROP_CLIENT:', text)
        self.assertIn('TempBanAddress', text)

    def test_binary_message_is_written_after_end_of_message(self):
        text = (ROOT/'code/server/sv_net_chan.cpp').read_text(encoding='utf-8')
        marker = text.index('void SV_Netchan_Transmit(')
        body = text[marker:text.index('\n}', marker)]
        self.assertLess(body.index('svc_EOF'), body.index('SV_WolfETWriteBinaryMessage'))


# Ordinals declared by ET's game headers that its own engine leaves out of
# SV_GameSystemCalls, so retail ET drops with "Bad game system trap" when a
# module issues one. fnTech3 reproduces that refusal instead of inventing
# behaviour. Source: Enemy-Territory server/sv_game.c at
# 40342a9e3690cb5b627a433d4d5cbf30e3c57698.
ORIGINAL_ENGINE_REFUSES = (
    'BOTLIB_AAS_ENTITY_VISIBLE', 'BOTLIB_AAS_IN_FIELD_OF_VISION',
    'BOTLIB_AAS_VISIBLE_CLIENTS', 'BOTLIB_AI_SET_AVOID_GOAL_TIME',
    'BOTLIB_AI_ADD_AVOID_SPOT', 'BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL',
    'BOTLIB_AAS_PREDICT_ROUTE', 'BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX',
)

# Imports reachable from the retail Enemy Territory game module, derived from its
# published sources (game/*.c trap wrappers at 40342a9e3690cb5b627a433d4d5cbf30e3c57698).
STOCK_MODULE_IMPORTS = (
    'BOTLIB_AAS_SETAASBLOCKINGENTITY', 'BOTLIB_AI_ALLOC_CHAT_STATE',
    'BOTLIB_AI_ALLOC_GOAL_STATE', 'BOTLIB_AI_ALLOC_MOVE_STATE',
    'BOTLIB_AI_ALLOC_WEAPON_STATE', 'BOTLIB_AI_CHARACTERISTIC_BFLOAT',
    'BOTLIB_AI_CHARACTERISTIC_STRING', 'BOTLIB_AI_FREE_CHAT_STATE',
    'BOTLIB_AI_FREE_GOAL_STATE', 'BOTLIB_AI_FREE_WEAPON_STATE',
    'BOTLIB_AI_LOAD_CHARACTER', 'BOTLIB_AI_LOAD_CHAT_FILE',
    'BOTLIB_AI_LOAD_ITEM_WEIGHTS', 'BOTLIB_AI_LOAD_WEAPON_WEIGHTS',
    'BOTLIB_AI_SET_CHAT_GENDER', 'BOTLIB_EA_RESET_INPUT',
    'BOTLIB_PC_FREE_SOURCE', 'BOTLIB_PC_LOAD_SOURCE', 'BOTLIB_PC_READ_TOKEN',
    'BOTLIB_PC_SOURCE_FILE_AND_LINE', 'G_ADJUST_AREA_PORTAL_STATE', 'G_ARGC',
    'G_ARGV', 'G_BOT_ALLOCATE_CLIENT', 'G_CVAR_LATCHEDVARIABLESTRINGBUFFER',
    'G_CVAR_REGISTER', 'G_CVAR_SET', 'G_CVAR_UPDATE',
    'G_CVAR_VARIABLE_INTEGER_VALUE', 'G_CVAR_VARIABLE_STRING_BUFFER',
    'G_DEBUG_POLYGON_CREATE', 'G_DROP_CLIENT', 'G_ENTITIES_IN_BOX',
    'G_ENTITY_CONTACT', 'G_ENTITY_CONTACTCAPSULE', 'G_ERROR', 'G_FS_FCLOSE_FILE',
    'G_FS_FOPEN_FILE', 'G_FS_GETFILELIST', 'G_FS_READ', 'G_FS_RENAME',
    'G_FS_WRITE', 'G_GETTAG', 'G_GET_CONFIGSTRING', 'G_GET_ENTITY_TOKEN',
    'G_GET_SERVERINFO', 'G_GET_USERCMD', 'G_GET_USERINFO', 'G_IN_PVS',
    'G_LINKENTITY', 'G_LOCATE_GAME_DATA', 'G_MILLISECONDS', 'G_POINT_CONTENTS',
    'G_PRINT', 'G_REAL_TIME', 'G_REGISTERTAG', 'G_SEND_CONSOLE_COMMAND',
    'G_SEND_SERVER_COMMAND', 'G_SET_BRUSH_MODEL', 'G_SET_CONFIGSTRING',
    'G_SET_USERINFO', 'G_SNAPVECTOR', 'G_TRACE', 'G_TRACECAPSULE',
    'G_UNLINKENTITY', 'PB_STAT_REPORT',
)


if __name__ == '__main__':
    unittest.main()
