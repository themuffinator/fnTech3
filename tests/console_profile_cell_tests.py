"""The console's character cell belongs to the profile.

Quake Live draws its console on retail's 12x24 cell at half size, scaled from
a 768-pixel reference height. Quake III and the titles built on it (RTCW,
Enemy Territory, Elite Force) draw theirs with the 8x16 small character, which
FnQuake3 scales with the 640x480 virtual screen. Before this, every profile
used Quake Live's cell, so a Quake III or RTCW console at 1280x720 had 6x11
characters where FnQuake3 draws 12x24.

The test compiles the console's own cell helper and the sizing lines of
Con_CheckResize, with the small character's size taken from q_shared.h.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    console = (ROOT / 'code/client/cl_console.cpp').read_text(encoding='utf-8')
    shared = (ROOT / 'code/qcommon/q_shared.h').read_text(encoding='utf-8')
    constants = '\n'.join(line for line in console.splitlines()
                          if line.startswith('constexpr float RETAIL_CONSOLE_'))
    small = '\n'.join(re.search(rf'#define\s+{name}\s+\d+', shared)[0]
                      for name in ('SMALLCHAR_WIDTH', 'SMALLCHAR_HEIGHT'))
    helper = console[console.index('struct ConsoleCell {'):]
    helper = helper[:helper.index('\n}\n', helper.index('static ConsoleCell Con_ProfileCell( void )')) + 3]
    start = console.index('\tconst ConsoleCell cell = Con_ProfileCell();')
    end = console.index('console_char_height = RoundToInt( cell.height * charScale );', start)
    sizing = console[start:console.index('\n', end)]
    return r'''
#include <cassert>
#include <initializer_list>
#include "code/client/canvas_geometry.hpp"
''' + constants + '\n' + small + r'''
static bool quakeLive;
static int Com_GameIsQuakeLive( void ) { return quakeLive; }
static struct { float con_factor, scale; struct { int vidWidth, vidHeight; } glconfig; } cls;
static int console_char_width, console_char_height;
static float con_ttfCellWidth;
''' + function(console, 'static int RoundToInt( float value )') + helper + r'''
static void Size( bool ql, float scale, bool uniformScale, int width, int height, float factor = 1.0f ) {
	float charScale;
	quakeLive = ql;
	cls.con_factor = factor;
	cls.glconfig.vidWidth = width;
	cls.glconfig.vidHeight = height;
	// The client's own 640x480 canvas fit supplies cls.scale.
	cls.scale = fnql::client::CalculateCanvasGeometry( width, height ).scale;
''' + sizing + r'''
}
static void Expect( int width, int height ) {
	assert( console_char_width == width && console_char_height == height );
}
int main() {
	assert( SMALLCHAR_WIDTH == 8 && SMALLCHAR_HEIGHT == 16 );
	// Quake Live keeps retail's half-size 12x24 cell on a 768-row reference.
	Size( true, 1.0f, true, 1366, 768 ); Expect( 6, 12 );
	Size( true, 1.0f, true, 1280, 720 ); Expect( 6, 11 );
	Size( true, 1.0f, false, 1920, 1080 ); Expect( 6, 12 );
	Size( true, 2.0f, true, 1366, 768 ); Expect( 12, 24 );
	assert( con_ttfCellWidth == 12.0f );
	// Every other profile uses Quake III's 8x16 small character, scaled with
	// the 640x480 virtual screen as FnQuake3 does.
	Size( false, 1.0f, true, 640, 480 ); Expect( 8, 16 );
	Size( false, 1.0f, true, 1280, 720 ); Expect( 12, 24 );
	Size( false, 1.0f, true, 1920, 1080 ); Expect( 18, 36 );
	Size( false, 1.0f, true, 1024, 1280 ); Expect( 13, 26 );
	Size( false, 1.0f, false, 1920, 1080 ); Expect( 8, 16 );
	Size( false, 2.0f, false, 1920, 1080 ); Expect( 16, 32 );
	Size( false, 0.5f, true, 640, 480 ); Expect( 4, 8 );
	// A capture factor scales both cells alike.
	Size( false, 1.0f, false, 640, 480, 2.0f ); Expect( 16, 32 );
	Size( true, 1.0f, false, 640, 480, 2.0f ); Expect( 12, 24 );
	return 0;
}
'''


class ConsoleProfileCellTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='console-cell-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_each_profile_sizes_its_console_on_its_own_cell(self):
        self.compile_and_run(fixture())


if __name__ == '__main__':
    unittest.main()
