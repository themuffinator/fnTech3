"""The Wolfenstein consoles' art: the charset, background and logo layer that
retail WolfMP.exe registers, the logo layers of WolfSP.exe and et.exe, and the
logo drawn where retail draws it."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include "code/client/console_logo.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using fntech3::client::ConsoleLogoPlacement;
using fntech3::client::ConsoleLogoRect;

static void Expect(float frac, float x, float width, float scale, float ex, float ey, float ew, float eh) {
    ConsoleLogoRect logo;
    if (!ConsoleLogoPlacement(frac, x, width, scale, logo)) {
        std::fprintf(stderr, "no logo at %f\n", frac); std::abort();
    }
    const float got[] = {logo.x, logo.y, logo.width, logo.height}, want[] = {ex, ey, ew, eh};
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(got[i] - want[i]) > 0.001f) {
            std::fprintf(stderr, "component %d: got %f expected %f\n", i, got[i], want[i]); std::abort();
        }
    }
}
static void Hidden(float frac, float x, float width, float scale) {
    ConsoleLogoRect logo;
    if (ConsoleLogoPlacement(frac, x, width, scale, logo)) {
        std::fprintf(stderr, "logo at %f\n", frac); std::abort();
    }
}
int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    // Retail's SCR_DrawPic( 192, 70, 256, 128 ) at 640x480 and at 1280x960,
    // half open and full screen.
    Expect(0.5f, 0, 640, 1, 192, 70, 256, 128);
    Expect(1.0f, 0, 1280, 2, 384, 140, 512, 256);
    // 1920x1080: a console across the window, or across its 4:3 middle with
    // con_screenExtents 1, puts the logo in the same place without stretching.
    Expect(0.5f, 0, 1920, 2.25f, 672, 157.5f, 576, 288);
    Expect(0.5f, 240, 1440, 2.25f, 672, 157.5f, 576, 288);
    // 1280x1024 is taller than 4:3.
    Expect(0.5f, 0, 1280, 2, 384, 140, 512, 256);
    // Retail skips the logo below half open, and for an unordered fraction
    // (fcomp 0.5 at WolfMP.exe 0x407657 and WolfSP.exe 0x407387, comiss and
    // jb at et.exe 0x4072c7).
    Hidden(0.49999f, 0, 640, 1);
    Hidden(0.0f, 0, 640, 1);
    Hidden(nan, 0, 640, 1);
    // No renderer size yet.
    Hidden(0.5f, 0, 640, 0);
    Hidden(0.5f, 0, 0, 1);
    Hidden(0.5f, 0, 640, nan);
    return 0;
}
'''


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function_body(source, signature):
    body = source[source.index(signature):]
    return body[:body.index('\n}\n')]


class RTCWMPConsoleArtTests(unittest.TestCase):
    def test_logo_placement_matches_retail_on_a_4_3_screen(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='console-logo-', dir=scratch) as directory:
            path = Path(directory)
            fixture = path / 'logo.cpp'
            fixture.write_text(HARNESS, encoding='utf-8')
            executable = path / ('logo.exe' if os.name == 'nt' else 'logo')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                str(fixture), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_only_rtcw_mp_registers_the_retail_mp_console_art(self):
        # WolfMP.exe CL_InitRenderer (0x40d910) registers gfx/2d/hudchars,
        # white, console-16bit and console2-16bit. WolfSP.exe's (0x40cdc0)
        # registers gfx/2d/bigchars, white, console and console2.
        body = function_body(read('code/client/cl_main.cpp'), 'static void CL_InitRenderer( void ) {')
        self.assertIn('const bool rtcwMP = Com_GameIsRTCWMP() != 0;', body)
        self.assertIn('re.RegisterShader( wolfet ? "gfx/2d/consolechars" : rtcwMP ? "gfx/2d/hudchars" : '
                      '"gfx/2d/bigchars" );', body)
        self.assertIn('cls.consoleShader = re.RegisterShader( wolfet || rtcwMP ? "console-16bit" :\n'
                      '\t\tCom_GameProfile()->id == COM_GAME_QUAKELIVE ? "fnql/console" : "console" );', body)

    def test_each_wolfenstein_engine_registers_its_retail_logo_layer(self):
        # Each CL_InitRenderer registers its logo last and keeps the handle:
        # WolfSP.exe (0x40cdc0) console2 at 0xf00f88, WolfMP.exe (0x40d910)
        # console2-16bit at 0xfec718, and et.exe (0x40e050) console2-16bit at
        # 0x156c1f0. Every other profile registers none.
        body = function_body(read('code/client/cl_main.cpp'), 'static void CL_InitRenderer( void ) {')
        self.assertIn('const bool wolfet = Com_GameIsWolfET() != 0;', body)
        self.assertIn('const bool rtcwMP = Com_GameIsRTCWMP() != 0;', body)
        self.assertIn('cls.consoleLogoShader = wolfet || rtcwMP ? re.RegisterShader( "console2-16bit" )\n'
                      '\t\t: Com_GameIsRTCWSP() ? re.RegisterShader( "console2" ) : 0;', body)
        self.assertEqual(body.count('consoleLogoShader'), 1)

    def test_logo_is_drawn_over_the_textured_background_only(self):
        console = read('code/client/cl_console.cpp')
        solid = function_body(console, 'static void Con_DrawSolidConsole( float frac ) {')
        textured = solid[solid.index('re.DrawStretchPic( xf, 0, wf, yf, 0, 0, 1, 1, cls.consoleShader );'):]
        self.assertTrue(textured.split('\n')[1].strip() == 'Con_DrawBackgroundLogo( frac, xf, wf, alphaScale );')
        self.assertEqual(solid.count('Con_DrawBackgroundLogo'), 1)
        logo = function_body(console, 'static void Con_DrawBackgroundLogo(')
        self.assertIn('!cls.consoleLogoShader ||', logo)
        self.assertIn('ConsoleLogoPlacement( frac, x, width, cls.scale, logo )', logo)


if __name__ == '__main__':
    unittest.main()
