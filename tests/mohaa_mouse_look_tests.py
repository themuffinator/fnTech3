"""Medal of Honor's mouse look, which a scale of nought had switched off.

2015's client game has no SetUserCmdValue import, so nothing set the client's
cgameSensitivity and CL_MouseMove multiplied every mouse move by nought. The
engine now asks the client game for the scale on every move, as 2015's
CL_MouseMove does (OpenMoHAA cl_input.cpp): export 9, SensitivityScale, which
1.00's cgamex86.dll answers with the fov_y / 75 its view set (0x30047619,
0x30048909); 1 while a menu holds the mouse, with no client game, or for a
negative answer. A move also sets the "any key" bit, 14, which 1.00's
gamex86.dll tests when it decides a player is idle (0x3111769b). These tests
compile the engine's own CL_MouseMove, CL_MOHAAMouseMove and
CL_MOHAACGameSensitivityScale against a small harness and turn the view with
them.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/client/input_compat.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

struct kbutton_t { qboolean active; };
static kbutton_t in_strafe, in_mlook;
static cvar_t accelStyle, filter, accel, sensitivity, showRate, accelOffset, side, yawScale,
	pitchScale, forwardScale, freelook;
static cvar_t *cl_mouseAccelStyle = &accelStyle, *m_filter = &filter, *cl_mouseAccel = &accel,
	*cl_sensitivity = &sensitivity, *cl_showMouseRate = &showRate,
	*cl_mouseAccelOffset = &accelOffset, *m_side = &side, *m_yaw = &yawScale,
	*m_pitch = &pitchScale, *m_forward = &forwardScale, *cl_freelook = &freelook;
static unsigned frame_msec = 16;
static struct {
	int mouseDx[2], mouseDy[2], mouseIndex;
	vec3_t viewangles;
	float cgameSensitivity;
} cl;
static fnql::input::RetailViewAngleFilter retailMouseFilter;
static void CL_UpdateMouseAccelDebugLog( bool ) {}
static int retailMoves;
static bool retailMoved;
static bool CL_RetailMouseMove( usercmd_t * ) { ++retailMoves; return retailMoved; }
signed char ClampCharMove( int value ) {
	return static_cast<signed char>( value < -127 ? -127 : value > 127 ? 127 : value );
}
void QDECL Com_Printf( const char *, ... ) {}

static int catcher;
static int Key_GetCatcher( void ) { return catcher; }
typedef float ( QDECL *Float_t )();
enum { CE_SensitivityScale = SLOT };
static float answer;
static bool exported;
static int asked;
static float QDECL SensitivityScale() { ++asked; return answer; }
template<typename T> T Export( int slot ) {
	assert( slot == CE_SensitivityScale );
	return exported ? &SensitivityScale : nullptr;
}
'''

HARNESS = r'''
static usercmd_t Move( int dx, int dy ) {
	cl.mouseDx[cl.mouseIndex] += dx;
	cl.mouseDy[cl.mouseIndex] += dy;
	usercmd_t cmd{};
	CL_MOHAAMouseMove( &cmd );
	return cmd;
}

static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

int main() {
	sensitivity.value = 5.0f;
	yawScale.value = pitchScale.value = 0.022f;
	side.value = forwardScale.value = 0.25f;
	freelook.integer = 1;
	exported = true;
	answer = 0.6f;

	// The client game's scale turns the view: 100 counts at sensitivity 5
	// and m_yaw 0.022 are 11 degrees, times 0.6; 50 up are 3.3 of pitch.
	usercmd_t cmd = Move( 100, 50 );
	assert( Near( cl.viewangles[YAW], -6.6f ) && Near( cl.viewangles[PITCH], 3.3f ) );
	assert( Near( cl.cgameSensitivity, 0.6f ) && asked == 1 );
	// The move counts as a key for the game's idle check.
	assert( cmd.buttons == ( 1 << 14 ) );

	// No motion turns nothing and leaves the bit clear; the scale is still
	// asked, as on every move.
	cmd = Move( 0, 0 );
	assert( cmd.buttons == 0 && Near( cl.viewangles[YAW], -6.6f ) && asked == 2 );

	// While a menu holds the mouse the scale is 1.
	catcher = KEYCATCH_UI;
	VectorClear( cl.viewangles );
	Move( 100, 0 );
	assert( Near( cl.viewangles[YAW], -11.0f ) && asked == 2 );
	catcher = 0;

	// A negative answer, one that is not a number, and no client game are 1.
	for ( const float bad : { -1.0f, std::numeric_limits<float>::quiet_NaN() } ) {
		answer = bad;
		VectorClear( cl.viewangles );
		Move( 100, 0 );
		assert( Near( cl.viewangles[YAW], -11.0f ) );
	}
	exported = false;
	VectorClear( cl.viewangles );
	Move( 100, 0 );
	assert( Near( cl.viewangles[YAW], -11.0f ) );
	exported = true;

	// Nought, 1.00's answer before its view's first frame, turns nothing,
	// though the move still counts as a key.
	answer = 0.0f;
	VectorClear( cl.viewangles );
	cmd = Move( 100, 0 );
	assert( cl.viewangles[YAW] == 0.0f && cmd.buttons == ( 1 << 14 ) );

	// The Quake Live transform (cl_mouseAccelStyle 2) reports its own move.
	accelStyle.integer = 2;
	retailMoved = true;
	cmd = Move( 0, 0 );
	assert( retailMoves == 1 && cmd.buttons == ( 1 << 14 ) );
	retailMoved = false;
	cmd = Move( 0, 0 );
	assert( retailMoves == 2 && cmd.buttons == 0 );
	std::puts( "ok" );
	return 0;
}
'''


class MOHAAMouseLookTests(unittest.TestCase):
    def test_mouse_turns_the_view_by_the_client_games_scale(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        client = read('code/client/cl_input.cpp')
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        slot = re.search(r'FNTECH3_MOHAA_CGAME_EXPORT\(\s*(\d+),\s*SensitivityScale,',
                         read('code/compat/mohaa_cgame_exports.def'))
        self.assertIsNotNone(slot)
        self.assertEqual(slot.group(1), '9')
        source = PRELUDE.replace('SLOT', slot.group(1))
        source += function(cgame, 'float CL_MOHAACGameSensitivityScale( void ) {')
        for signature in ('static int CL_SafeMoveValue(', 'static void CL_AddViewAngleDelta(',
                          'static bool CL_MouseMove( usercmd_t *cmd )',
                          'static void CL_MOHAAMouseMove( usercmd_t *cmd ) {'):
            source += function(client, signature)
        source += HARNESS
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'mouse.cpp'
            exe = Path(directory) / 'mouse.exe'
            cpp.write_text(source, encoding='utf-8')
            target = compiler_target_args(compiler)
            abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
            built = subprocess.run([compiler, *target, *abi, '-std=c++17', '-I', str(ROOT), str(cpp),
                                    '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=20,
                                 **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_command_builder_moves_the_mouse_through_the_mohaa_path(self):
        client = read('code/client/cl_input.cpp')
        create = function(client, 'static usercmd_t CL_CreateCmd( void ) {')
        start = create.index('} else if ( Com_GameIsMOHAA() ) {')
        branch = create[start:create.index('} else {', start)]
        order = [branch.index(call) for call in ('CL_MOHAACmdButtons( &cmd );', 'CL_MOHAAKeyMove( &cmd );',
                                                 'CL_MOHAAMouseMove( &cmd );')]
        self.assertEqual(order, sorted(order))
        self.assertNotIn('CL_MouseMove( &cmd );', branch)
        # Every other profile keeps the scale its client game hands over.
        self.assertIn('CL_MouseMove( &cmd );', create[create.index('} else {', start):])


if __name__ == '__main__':
    unittest.main()
