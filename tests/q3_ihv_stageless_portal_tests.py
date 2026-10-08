"""The IHV Test's stageless scripts and portal views follow its own renderer.

quake3.exe 4.01 reads a shader script with no stage, neither sky nor fog, as
no script at all and draws the image of the same name (ParseShader 0x43a3c0,
R_FindShader 0x439fd0); Q3 1.32 gives it the default shader. Its portal pass
(0x42c160) has no range test, and its portal camera (0x42bde0) always rolls,
by frame or by a bob plus skinNum. tr_q3_ihv_stageless.h and
tr_q3_ihv_portal.h hold those rules for that profile alone; each renderer's
shader parser and portal code call them.
"""
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from model_shader_tests import common

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
OTHERS = ('COM_GAME_QUAKE3', 'COM_GAME_QUAKELIVE', 'COM_GAME_Q3TEST', 'COM_GAME_RTCW_SP',
          'COM_GAME_RTCW_MP', 'COM_GAME_WOLFET', 'COM_GAME_STVEF_MP', 'COM_GAME_JKA_MP')


def float_bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def reference_roll(time, frame, skin):
    """0x42c0b6..0x42c12c: fild time, then fmul 0.001f and fimul frame, or
    fmul 0.003, fsin, fmul 4.0f and fiadd skinNum; one float store."""
    if frame:
        thousandth = struct.unpack('<f', struct.pack('<f', 0.001))[0]
        return float_bits(time * thousandth * frame)
    return float_bits(math.sin(time * 0.003) * 4.0 + skin)


ROLLS = ((0, 0, 0), (1000, 25, 0), (123456, 25, 7), (2147483, -3, 0), (16, 1, 99),
         (500, 0, 3), (-2000, 0, -5), (987654, 0, 0), (1, 0, 360))


def checks():
    rolls = '\n'.join(
        f' {{ volatile float got = Q3IHV_PortalRoll({t}, {f}, {s}); float value = got; std::uint32_t bits;'
        f' std::memcpy(&bits, &value, 4); assert(bits == 0x{reference_roll(t, f, s):08x}u); }}'
        for t, f, s in ROLLS)
    others = '\n'.join(
        f' ri.gameId = {name}; s_q3ihvStagelessImage = qtrue; assert(!R_Q3IHVStagelessImage());'
        f' assert(!s_q3ihvStagelessImage); assert(Q3IHV_PortalRanged({name}));'
        for name in OTHERS)
    return r'''
#include <cstdint>
#include "code/renderercommon/tr_q3_ihv_stageless.h"
#include "code/renderercommon/tr_q3_ihv_portal.h"
int main() {
 // The IHV Test looks a stageless script's name up as an image.
 ri.gameId = COM_GAME_QUAKE3_IHV; s_q3ihvStagelessImage = qfalse;
 assert(R_Q3IHVStagelessImage() && s_q3ihvStagelessImage);
 // Its portals have no range.
 assert(!Q3IHV_PortalRanged(COM_GAME_QUAKE3_IHV));
 // Every other profile keeps Q3 1.32's default shader and portal range.
''' + others + r'''
 // The camera's roll matches the original's arithmetic, bit for bit.
''' + rolls + r'''
 return 0;
}
'''


def between(text, start, end):
    first = text.index(start)
    return text[first:text.index(end, first)]


class Q3IHVStagelessPortalTests(unittest.TestCase):
    def test_rules_compile_and_hold_for_this_profile_only(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-portal-', dir=ROOT / '.tmp') as temp:
            cpp = Path(temp) / 'rules.cpp'
            exe = Path(temp) / ('rules' + ('.exe' if os.name == 'nt' else ''))
            cpp.write_text(common('renderer') + '#include <cassert>\n#include <cstring>\n' + checks())
            build = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_each_shader_parser_reads_a_stageless_script_as_an_image(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                self.assertEqual(source.count('#include "../renderercommon/tr_q3_ihv_stageless.h"'), 1)
                # Set only where a stageless script is ignored, reset for every shader.
                ignored = between(source, '// ignore shaders that don\'t have any stages', 'return qfalse;')
                self.assertEqual(source.count('R_Q3IHVStagelessImage();'), 1)
                self.assertIn('R_Q3IHVStagelessImage();', ignored)
                self.assertEqual(source.count('s_q3ihvStagelessImage = qfalse;'), 1)
                reset = between(source, 'R_WolfETResetImplicit();', '\n\n')
                self.assertIn('s_q3ihvStagelessImage = qfalse;', reset)
                # A failed parse defaults the shader unless the image is to be found.
                lookup = between(source, 'if ( !ParseShader( &shaderText ) ) {', 'R_FindShaderImage(')
                self.assertLess(lookup.index('if ( !s_q3ihvStagelessImage )'),
                                lookup.index('shader.defaultShader = qtrue;'))
                self.assertIn('else if ( !s_wolfetImplicitMap[0] )', lookup)

    def test_each_portal_path_uses_the_original_range_and_roll(self):
        shared = (ROOT / 'code/renderercommon/tr_portal_plan.h').read_text()
        rtx = (ROOT / 'code/rendererrtx/tr_main.c').read_text()
        self.assertIn('#include "tr_q3_ihv_portal.h"', shared)
        self.assertIn('#include "../renderercommon/tr_q3_ihv_portal.h"', rtx)
        for renderer in ('renderer', 'renderervk'):
            self.assertIn('#include "../renderercommon/tr_portal_plan.h"',
                          (ROOT / 'code' / renderer / 'tr_main.c').read_text())
        for name, source, entity in (('shared', shared, 'entity->e'), ('rtx', rtx, 'e->e')):
            with self.subTest(path=name):
                self.assertEqual(source.count('Q3IHV_PortalRanged( ri.gameId ) &&'), 1)
                ranged = source.index('Q3IHV_PortalRanged( ri.gameId ) &&')
                self.assertIn('portalRange', source[ranged:ranged + 120])
                roll = source.index(f'if ( {entity}.q3IHV.active )')
                self.assertLess(roll, source.index(f'{entity}.oldframe', roll))
                self.assertIn(f'Q3IHV_PortalRoll( tr.refdef.time, {entity}.frame, {entity}.skinNum )',
                              source[roll:roll + 200])


if __name__ == '__main__':
    unittest.main()
