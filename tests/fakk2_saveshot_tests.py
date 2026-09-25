"""Heavy Metal: F.A.K.K.2's savegame picture, as retail fakk2.exe 1.02 makes it.

Retail's `screenshot <path> <width> <height>` (0x004603a4 -> 0x0045ffa0) reads
the frame back and samples it four across and three down, as American
McGee's Alice's does, but keeps each channel's own average and stores it blue,
green, red: a colour picture. Alice's keeps the brightest channel times 2.55,
which F.A.K.K.2's saves showed as a near-white grey. The reductions are
compiled from code/compat/alice_saveshot.h and compared, byte for byte, with
the rules written out here; the renderers are checked to choose F.A.K.K.2's
by the game.
"""
from pathlib import Path
import os
import re
import shutil
import struct
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


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def frame(width, height):
    """RGB bytes, rows as read (the bottom one first), every pixel distinct."""
    return [((x * 29 + y * 7) % 256, (x * 5 + y * 41 + 60) % 256, (x * 13 + y * 3 + 120) % 256)
            for y in range(height) for x in range(width)]


def sums(pixels, width, height, out_width, out_height, ox, oy):
    xs = f32(width / (4 * out_width))
    ys = f32(height / (3 * out_height))
    total = [0, 0, 0]
    for r in range(3):
        sy = min(int((oy * 3 + r) * ys), height - 1)
        for c in range(4):
            sx = min(int((ox * 4 + c) * xs), width - 1)
            for channel in range(3):
                total[channel] += pixels[sy * width + sx][channel]
    return total


def fakk2_expected(pixels, width, height, out_width, out_height):
    out = []
    for oy in range(out_height):
        for ox in range(out_width):
            red, green, blue = sums(pixels, width, height, out_width, out_height, ox, oy)
            out += [blue // 12, green // 12, red // 12]
    return out


def alice_expected(pixels, width, height, out_width, out_height):
    out = []
    for oy in range(out_height):
        for ox in range(out_width):
            red, green, blue = sums(pixels, width, height, out_width, out_height, ox, oy)
            if red > blue and red > green:
                value = red
            elif blue > red and blue > green:
                value = blue
            else:
                value = green
            scaled = (value // 12) * f32(2.55)
            out += [255 if scaled > 255.0 else int(scaled)] * 3
    return out


SHAPES = ((8, 6, 2, 2), (10, 7, 3, 2), (37, 23, 5, 4))


class Fakk2SaveshotTests(unittest.TestCase):
    def test_the_reductions_match_their_rules(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        target = compiler_target_args(compiler)
        lines = ['#include <stdio.h>', '#include <stddef.h>', '#include "code/compat/alice_saveshot.h"',
                 'static void dump( const unsigned char *bytes, int count ) {',
                 '\tint i;',
                 '\tfor ( i = 0; i < count; i++ ) printf( "%d ", bytes[i] );',
                 '\tprintf( "\\n" );',
                 '}',
                 'int main( void ) {',
                 '\tstatic unsigned char out[5 * 4 * 3];']
        for index, (width, height, out_width, out_height) in enumerate(SHAPES):
            values = ', '.join(str(byte) for pixel in frame(width, height) for byte in pixel)
            lines += [f'\t{{ static const unsigned char source{index}[] = {{ {values} }};',
                      f'\tFNTECH3_Fakk2Saveshot( source{index}, {width}, {height}, {width * 3}, out, '
                      f'{out_width}, {out_height} );',
                      f'\tdump( out, {out_width * out_height * 3} );',
                      f'\tFNTECH3_AliceSaveshot( source{index}, {width}, {height}, {width * 3}, out, '
                      f'{out_width}, {out_height} );',
                      f'\tdump( out, {out_width * out_height * 3} ); }}']
        lines += ['\treturn 0;', '}']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='fakk2-saveshot-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.c'
            source.write_text('\n'.join(lines) + '\n', encoding='utf-8')
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            built = subprocess.run([compiler, *target, '-std=c99', '-Wall', '-Werror', '-I', str(ROOT),
                                    str(source), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        rows = [[int(value) for value in line.split()] for line in ran.stdout.splitlines()]
        self.assertEqual(len(rows), 2 * len(SHAPES))
        for index, (width, height, out_width, out_height) in enumerate(SHAPES):
            pixels = frame(width, height)
            with self.subTest(shape=(width, height, out_width, out_height)):
                fakk2 = rows[2 * index]
                self.assertEqual(fakk2, fakk2_expected(pixels, width, height, out_width, out_height))
                self.assertEqual(rows[2 * index + 1], alice_expected(pixels, width, height, out_width, out_height))
                # The picture keeps its colours: its blue and red bytes differ.
                self.assertNotEqual(fakk2[0::3], fakk2[2::3])

    def test_the_renderers_choose_fakk2s_by_the_game(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            take = function(read(f'code/{renderer}/tr_init.c'), 'void RB_TakeSTVEFThumbnail( void ) {')
            with self.subTest(renderer=renderer):
                # The branch may name other titles that share the reduction.
                branch = re.search(r'if \( aliceHeight > 0 && [^\n]*ri\.gameId == COM_GAME_FAKK2[^\n]*\{', take)
                self.assertIsNotNone(branch)
                fakk2 = branch.start()
                call = take.index('FNTECH3_Fakk2Saveshot( source, gls.captureWidth, gls.captureHeight,', fakk2)
                self.assertLess(call, take.index('R_GammaCorrect( rgba, side * aliceHeight * 3 );'))
                self.assertLess(fakk2, take.index('} else if ( aliceHeight > 0 ) {'))
                self.assertLess(take.index('} else if ( aliceHeight > 0 ) {'),
                                take.index('FNTECH3_AliceSaveshot( source, gls.captureWidth, gls.captureHeight,'))


if __name__ == '__main__':
    unittest.main()
