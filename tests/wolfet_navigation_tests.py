"""Exercise ET navigation contracts and the chat-match record translation."""
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
#include "code/qcommon/q_shared.h"
#include "code/compat/wolfet_game_abi.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

int main() {
 // ET's chat records are smaller than the engine's, so their sizes are frozen.
 static_assert(FNTECH3_WOLFET_MAX_MESSAGE_SIZE == 150, "ET message limit");
 static_assert(FNTECH3_WOLFET_MAX_MATCHVARIABLES == 8, "ET match variables");
 static_assert(sizeof(fntech3_wolfet_bot_matchvariable_t) == 8, "ET match variable");
 static_assert(sizeof(fntech3_wolfet_bot_match_t) == 224, "ET match record");
 static_assert(offsetof(fntech3_wolfet_bot_match_t, string) == 0, "match string first");
 static_assert(offsetof(fntech3_wolfet_bot_match_t, type) == 152, "match type");
 static_assert(offsetof(fntech3_wolfet_bot_match_t, variables) == 160, "match variables");
 static_assert(sizeof(fntech3_wolfet_bot_consolemessage_t) == 172, "ET console message");
 static_assert(sizeof(fntech3_wolfet_bot_goal_t) == 64, "ET goal");
 static_assert(sizeof(fntech3_wolfet_bot_initmove_t) == 72, "ET movement setup");
 static_assert(sizeof(fntech3_wolfet_bot_moveresult_t) == 52, "ET movement result");
 static_assert(sizeof(fntech3_wolfet_bot_entitystate_t) == 112, "ET entity state");
 static_assert(sizeof(fntech3_wolfet_bot_input_t) == 40, "ET bot input");

 // ET renderer records the cgame boundary passes. Its reference entity carries
 // a torso skeleton and per-frame model handles Quake's does not.
 static_assert(sizeof(fntech3_wolfet_polyVert_t) == 24, "ET poly vertex");
 static_assert(sizeof(fntech3_wolfet_orientation_t) == 48, "ET orientation");
 static_assert(sizeof(fntech3_wolfet_markFragment_t) == 8, "ET mark fragment");
 static_assert(sizeof(fntech3_wolfet_refEntity_t) == 236, "ET reference entity");
 static_assert(offsetof(fntech3_wolfet_refEntity_t, origin) == 104, "ET entity origin");
 static_assert(offsetof(fntech3_wolfet_refEntity_t, entityNum) == 232, "ET entity number");
 static_assert(sizeof(fntech3_wolfet_glfog_t) == 60, "ET fog");
 static_assert(sizeof(fntech3_wolfet_refdef_t) == 428, "ET scene definition");
 static_assert(offsetof(fntech3_wolfet_refdef_t, glfog) == 368, "ET scene fog");
 static_assert(sizeof(fntech3_wolfet_glconfig_t) == 7264, "ET renderer configuration");

 // ET's prediction result carries a world trace Quake's prediction never makes.
 static_assert(sizeof(fntech3_wolfet_bsp_surface_t) == 24, "ET trace surface");
 static_assert(sizeof(fntech3_wolfet_cplane_t) == 20, "ET trace plane");
 static_assert(sizeof(fntech3_wolfet_bsp_trace_t) == 84, "ET world trace");
 static_assert(sizeof(fntech3_wolfet_aas_clientmove_t) == 128, "ET prediction result");
 static_assert(offsetof(fntech3_wolfet_aas_clientmove_t, trace) == 24, "ET result trace");
 static_assert(offsetof(fntech3_wolfet_aas_clientmove_t, frames) == 124, "ET result frames");

 // A variable addresses the record's own string and never reaches past its
 // 150 bytes. The record stores a 32-bit address because ET's modules are x86,
 // so the bound check is written in that width rather than the host's.
 fntech3_wolfet_bot_match_t record{};
 const std::uint32_t base = 0x00400000u + offsetof(fntech3_wolfet_bot_match_t, string);
 record.variables[0].ptr = base + 4;
 record.variables[0].length = 3;
 assert(record.variables[0].ptr >= base);
 assert(record.variables[0].ptr + std::uint32_t(record.variables[0].length)
        < base + sizeof(record.string));
 // An absent variable is a null pointer, which is how ET spells "no match".
 assert(record.variables[1].ptr == 0 && record.variables[1].length == 0);
 return 0;
}
'''


def _source(name):
    return (ROOT/name).read_text(encoding='utf-8')


class WolfETNavigationTests(unittest.TestCase):
    def test_record_layouts(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-next'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            source = folder/'records.cpp'
            executable = folder/('records.exe' if os.name == 'nt' else 'records')
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

    def test_blocking_entity_uses_wolfet_rules(self):
        """ET resets on a negative value, keeps the caller's bits, retries movers."""
        text = _source('code/botlib/be_aas_main.cpp')
        begin = text.index('void AAS_WolfETSetBlockingEntity(')
        body = text[begin:text.index('\n}\n', begin)]
        self.assertIn('samePoint && blocking < 0', body)
        self.assertIn('0x80000000', body)
        self.assertIn('AREACONTENTS_MOVER', body)
        self.assertIn('( blocking & ~0x1 )', body)
        # The RTCW body must keep its own sentinel, which differs per mode.
        rtcw = text[text.index('void AAS_SetBlockingEntity('):]
        rtcw = rtcw[:rtcw.index('\n}\n')]
        self.assertIn('Com_GameIsRTCWSP()', rtcw)
        self.assertNotIn('AREACONTENTS_MOVER', rtcw)

        server = _source('code/server/server_wolfet_botlib.hpp')
        self.assertIn('AAS_WolfETSetBlockingEntity( mins.data()', server)
        self.assertNotIn('AAS_SetBlockingEntity( mins.data()', server)

    def test_inert_imports_match_the_release_bodies(self):
        """Two ET route imports do nothing in the shipped engine."""
        server = _source('code/server/server_wolfet_botlib.hpp')
        for call, marker in (('BOTLIB_AAS_RT_SHOWROUTE', '#ifdef DEBUG'),
                             ('BOTLIB_AAS_RECORDTEAMDEATHAREA', 'unconditional return')):
            begin = server.index('case Call::%s:' % call)
            block = server[begin:begin+700]
            self.assertIn('result = 0;', block)
            self.assertIn(marker, server[max(0, begin-400):begin+400])

    def test_area_queries_are_bounded(self):
        text = _source('code/botlib/be_aas_main.cpp')
        for signature, limit in (
                ('bool AAS_WolfETAreaCenter( int area, float *center )', 'aasworld.numareas'),
                ('int AAS_WolfETAreaLadder( int area )', 'aasworld.numareasettings')):
            begin = text.index(signature)
            body = text[begin:text.index('\n}\n', begin)]
            self.assertIn('area < 0', body)
            self.assertIn(limit, body)
        # The waypoint query has no table to read and says so rather than guessing.
        begin = text.index('bool AAS_WolfETAreaWaypoint(')
        self.assertIn('return false;', text[begin:text.index('\n}\n', begin)])

    def test_every_declared_import_is_handled(self):
        """Only the ordinals retail ET refuses itself may remain unhandled."""
        declared = re.findall(
            r'FNTECH3_WOLFET_MODULE_CALL\(GAME, IMPORT, (\w+), \d+\)',
            _source('code/compat/wolfet_module_calls.def'))
        handled = set()
        for name in ('code/server/server_wolfet_game.hpp',
                     'code/server/server_wolfet_botlib.hpp'):
            body = '\n'.join(l for l in _source(name).splitlines()
                             if not l.lstrip().startswith('#'))
            handled |= set(re.findall(r'case Call::(\w+)', body))
            handled |= set(re.findall(r'ET_SHARED\((\w+)\)', body))
        missing = sorted(set(declared) - handled)
        self.assertEqual(missing, sorted(RETAIL_REFUSES + NAVIGATION_RESIDUAL))


# Ordinals retail ET's own engine never implements; see wolfet_server_state_tests.
RETAIL_REFUSES = (
    'BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL', 'BOTLIB_AAS_ENTITY_VISIBLE',
    'BOTLIB_AAS_IN_FIELD_OF_VISION', 'BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX',
    'BOTLIB_AAS_PREDICT_ROUTE', 'BOTLIB_AAS_VISIBLE_CLIENTS',
    'BOTLIB_AI_ADD_AVOID_SPOT', 'BOTLIB_AI_SET_AVOID_GOAL_TIME',
)

# Every declared ET game import is now handled; only the ordinals retail ET
# refuses itself remain.
NAVIGATION_RESIDUAL = ()


if __name__ == '__main__':
    unittest.main()
