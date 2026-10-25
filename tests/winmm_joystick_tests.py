"""Drive the Windows client's WinMM joystick producer against a simulated device.

The production joystick functions are cut out of code/win32/win_input.cpp and
compiled with fake joyGetNumDevs/joyGetPosEx/joyGetDevCaps, so the checks need
no controller and touch no real input device. They pin what the native Windows
backend does now that it compiles its joystick support: nothing at all until
in_joystick is set, a quiet start with no device, Quake III's key mapping for
buttons, stick directions and the POV hat, Quake Live's opt-in analog profile,
and recovery when the device is unplugged.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(text: str, signature: str) -> str:
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def block(text: str, start_marker: str, end_marker: str) -> str:
    start = text.index(start_marker)
    end = text.index(end_marker, start) + len(end_marker)
    return text[start:end] + '\n'


PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "code/client/input_compat.hpp"
#include "code/client/keycodes.h"

typedef enum { qfalse = 0, qtrue } qboolean;
'''

STUBS = r'''
struct cvar_t { int integer; float value; };
static cvar_t joystickCvar, profileCvar, invertedCvar, ballScaleCvar, debugCvar,
	horizViewSensCvar, vertViewSensCvar, horizViewDeadCvar, vertViewDeadCvar,
	horizMoveDeadCvar, vertMoveDeadCvar, thresholdCvar;
cvar_t *in_joystick = &joystickCvar;
cvar_t *in_joystickProfile = &profileCvar;
cvar_t *in_joystickInverted = &invertedCvar;
cvar_t *in_joyBallScale = &ballScaleCvar;
cvar_t *in_debugJoystick = &debugCvar;
cvar_t *in_joyHorizViewSensitivity = &horizViewSensCvar;
cvar_t *in_joyVertViewSensitivity = &vertViewSensCvar;
cvar_t *in_joyHorizViewDeadzone = &horizViewDeadCvar;
cvar_t *in_joyVertViewDeadzone = &vertViewDeadCvar;
cvar_t *in_joyHorizMoveDeadzone = &horizMoveDeadCvar;
cvar_t *in_joyVertMoveDeadzone = &vertMoveDeadCvar;
cvar_t *joy_threshold = &thresholdCvar;

static void Com_DPrintf( const char *, ... ) {}
static void Com_Printf( const char *, ... ) {}
static void Com_Memset( void *dest, int value, size_t count ) { memset( dest, value, count ); }
static std::string uiJoyAvail = "unset";
static void Cvar_Set( const char *name, const char *value ) {
	assert( strcmp( name, "ui_joyavail" ) == 0 );
	uiJoyAvail = value;
}
static float Cvar_VariableValue( const char *name ) {
	assert( strcmp( name, "cl_viewAccel" ) == 0 );
	return 1.0f;
}

struct { int sysMsgTime; } g_wv;
struct QueuedEvent { int type, value, value2; };
static std::vector<QueuedEvent> events;
static void Sys_QueEvent( int, sysEventType_t type, int value, int value2, int, void * ) {
	events.push_back( { type, value, value2 } );
}
static int inputResets;
static void WIN_QueueInputReset( qboolean rebuildModifiers ) {
	assert( rebuildModifiers );
	++inputResets;
}

// The simulated device. An id with no controller behind it answers an
// error; Windows 11 answered JOYERR_PARMS (0xa5) for every slot of a machine
// with none (docs/fntech3/2026-10-24-windows-joystick.md).
static UINT fakeDriverSlots = 16;
static int fakeDeviceId = -1;
static JOYCAPSA fakeCaps;
static JOYINFOEX fakeState;
static int fakeFailures;
static MMRESULT fakeFailure = JOYERR_PARMS;
static int numDevsCalls, posCalls, capsCalls;

static UINT Fake_joyGetNumDevs( void ) { ++numDevsCalls; return fakeDriverSlots; }
static MMRESULT Fake_joyGetPosEx( UINT id, LPJOYINFOEX info ) {
	++posCalls;
	assert( info->dwSize == sizeof( JOYINFOEX ) );
	assert( info->dwFlags == JOY_RETURNCENTERED || info->dwFlags == JOY_RETURNALL );
	if ( static_cast<int>( id ) != fakeDeviceId ) {
		return JOYERR_PARMS;
	}
	if ( fakeFailures > 0 ) {
		--fakeFailures;
		return fakeFailure;
	}
	const DWORD size = info->dwSize, flags = info->dwFlags;
	*info = fakeState;
	info->dwSize = size;
	info->dwFlags = flags;
	return JOYERR_NOERROR;
}
static MMRESULT Fake_joyGetDevCaps( UINT_PTR id, LPJOYCAPSA caps, UINT size ) {
	++capsCalls;
	assert( size == sizeof( JOYCAPSA ) );
	if ( static_cast<int>( id ) != fakeDeviceId ) {
		return MMSYSERR_NODRIVER;
	}
	*caps = fakeCaps;
	return JOYERR_NOERROR;
}
#undef joyGetDevCaps
#define joyGetNumDevs Fake_joyGetNumDevs
#define joyGetPosEx Fake_joyGetPosEx
#define joyGetDevCaps Fake_joyGetDevCaps
'''

CHECKS = r'''
static constexpr DWORD kCentre = 32768;

static void ResetCounters() { events.clear(); numDevsCalls = posCalls = capsCalls = 0; inputResets = 0; }

static void CentreDevice() {
	memset( &fakeState, 0, sizeof( fakeState ) );
	fakeState.dwXpos = fakeState.dwYpos = fakeState.dwZpos = kCentre;
	fakeState.dwRpos = fakeState.dwUpos = fakeState.dwVpos = kCentre;
	fakeState.dwPOV = JOY_POVCENTERED;
}

// An Xbox-style pad as WinMM reports it: sticks on X/Y and R/U, both
// triggers sharing Z, a four-way hat and ten buttons.
static void PlugInPad( int id ) {
	memset( &fakeCaps, 0, sizeof( fakeCaps ) );
	strcpy( fakeCaps.szPname, "Simulated pad" );
	fakeCaps.wNumButtons = 10;
	fakeCaps.wMaxButtons = 32;
	fakeCaps.wNumAxes = 5;
	fakeCaps.wMaxAxes = 6;
	fakeCaps.wCaps = JOYCAPS_HASZ | JOYCAPS_HASR | JOYCAPS_HASU | JOYCAPS_HASPOV | JOYCAPS_POV4DIR;
	fakeDeviceId = id;
	CentreDevice();
}

static bool Has( int type, int value, int value2 ) {
	for ( const QueuedEvent &event : events ) {
		if ( event.type == type && event.value == value && event.value2 == value2 ) {
			return true;
		}
	}
	return false;
}

static void Poll() { events.clear(); IN_JoyMove(); }

int main() {
	thresholdCvar.value = 0.15f;
	ballScaleCvar.value = 0.02f;
	horizViewSensCvar.value = 20.0f;
	vertViewSensCvar.value = 15.0f;
	horizViewDeadCvar.value = vertViewDeadCvar.value = 0.15f;
	horizMoveDeadCvar.value = 0.5f;
	vertMoveDeadCvar.value = 0.15f;

	// in_joystick 0, the default: no WinMM call at all, at start or per frame.
	PlugInPad( 0 );
	ResetCounters();
	IN_StartupJoystick();
	assert( !joy.avail && uiJoyAvail == "0" );
	assert( numDevsCalls == 0 && posCalls == 0 && capsCalls == 0 );
	fakeState.dwButtons = 1;
	Poll();
	assert( posCalls == 0 && events.empty() );

	// in_joystick 1 with no driver, then with sixteen empty slots: a quiet start.
	joystickCvar.integer = 1;
	fakeDeviceId = -1;
	fakeDriverSlots = 0;
	ResetCounters();
	IN_StartupJoystick();
	assert( !joy.avail && numDevsCalls == 1 && posCalls == 0 && capsCalls == 0 );
	fakeDriverSlots = 16;
	ResetCounters();
	IN_StartupJoystick();
	assert( !joy.avail && uiJoyAvail == "0" );
	assert( numDevsCalls == 1 && posCalls == 16 && capsCalls == 0 );
	Poll();
	assert( posCalls == 16 && events.empty() && inputResets == 0 );

	// A pad in the third slot is found and opened.
	PlugInPad( 2 );
	ResetCounters();
	IN_StartupJoystick();
	assert( joy.avail && joy.id == 2 && uiJoyAvail == "1" );
	assert( posCalls == 3 && capsCalls == 1 );

	// At rest it queues nothing, and an unchanged state is never repeated.
	Poll();
	assert( events.empty() );

	// Buttons are K_JOY1 onwards, pressed and released once each; a bit
	// beyond the device's own button count is ignored.
	fakeState.dwButtons = ( 1u << 0 ) | ( 1u << 9 ) | ( 1u << 12 );
	Poll();
	assert( events.size() == 2 );
	assert( Has( SE_KEY, K_JOY1, qtrue ) && Has( SE_KEY, K_JOY10, qtrue ) );
	Poll();
	assert( events.empty() );
	fakeState.dwButtons = 0;
	Poll();
	assert( events.size() == 2 );
	assert( Has( SE_KEY, K_JOY1, qfalse ) && Has( SE_KEY, K_JOY10, qfalse ) );

	// Quake III's direction keys: the left stick is the arrow keys, the
	// trigger axis K_JOY16/17, the right stick's R axis K_JOY18/19.
	fakeState.dwXpos = 65535;
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_RIGHTARROW, qtrue ) );
	fakeState.dwXpos = 0;
	Poll();
	assert( events.size() == 2 && Has( SE_KEY, K_RIGHTARROW, qfalse ) && Has( SE_KEY, K_LEFTARROW, qtrue ) );
	fakeState.dwXpos = kCentre;
	fakeState.dwYpos = 0;
	Poll();
	assert( events.size() == 2 && Has( SE_KEY, K_LEFTARROW, qfalse ) && Has( SE_KEY, K_UPARROW, qtrue ) );
	fakeState.dwYpos = kCentre;
	fakeState.dwZpos = 0;
	fakeState.dwRpos = 65535;
	Poll();
	assert( events.size() == 3 && Has( SE_KEY, K_UPARROW, qfalse ) );
	assert( Has( SE_KEY, K_JOY16, qtrue ) && Has( SE_KEY, K_JOY19, qtrue ) );
	fakeState.dwZpos = 65535;
	fakeState.dwRpos = kCentre;
	Poll();
	assert( events.size() == 3 && Has( SE_KEY, K_JOY16, qfalse ) && Has( SE_KEY, K_JOY17, qtrue ) );
	assert( Has( SE_KEY, K_JOY19, qfalse ) );
	fakeState.dwZpos = kCentre;
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_JOY17, qfalse ) );

	// The U axis alone is not the trackball lane, which needs U and V.
	fakeState.dwUpos = 65535;
	Poll();
	assert( events.empty() );
	fakeState.dwUpos = kCentre;

	// joy_threshold is the dead zone; a broken value disables directions.
	thresholdCvar.value = 0.5f;
	fakeState.dwXpos = kCentre + 13107;	// 0.4 of full deflection
	Poll();
	assert( events.empty() );
	fakeState.dwXpos = kCentre + 22938;	// 0.7
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_RIGHTARROW, qtrue ) );
	fakeState.dwXpos = kCentre;
	Poll();
	thresholdCvar.value = std::numeric_limits<float>::quiet_NaN();
	fakeState.dwXpos = 65535;
	Poll();
	assert( events.empty() );
	fakeState.dwXpos = kCentre;
	thresholdCvar.value = 0.15f;

	// The hat's four directions are K_JOY24..27; a diagonal is none of them.
	fakeState.dwPOV = JOY_POVFORWARD;
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_JOY24, qtrue ) );
	fakeState.dwPOV = JOY_POVRIGHT;
	Poll();
	assert( events.size() == 2 && Has( SE_KEY, K_JOY24, qfalse ) && Has( SE_KEY, K_JOY26, qtrue ) );
	fakeState.dwPOV = 4500;
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_JOY26, qfalse ) );
	fakeState.dwPOV = JOY_POVCENTERED;
	Poll();
	assert( events.empty() );

	// A device without a Z axis never reports one, whatever dwZpos holds.
	fakeCaps.wCaps &= ~JOYCAPS_HASZ;
	joy.jc.wCaps &= ~JOYCAPS_HASZ;
	fakeState.dwZpos = 0;
	Poll();
	assert( events.empty() );
	fakeState.dwZpos = kCentre;
	joy.jc.wCaps |= JOYCAPS_HASZ;

	// An input reset drops the producer's snapshot, so a button still held
	// afterwards is pressed again on the next poll.
	fakeState.dwButtons = 1;
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_JOY1, qtrue ) );
	IN_ClearWinMMJoystickState( qfalse );
	assert( joy.avail );
	Poll();
	assert( events.size() == 1 && Has( SE_KEY, K_JOY1, qtrue ) );

	// Two failed reads are forgiven; the third in a row closes the device
	// and queues one input reset, which releases what it held.
	fakeFailures = 2;
	ResetCounters();
	Poll();
	Poll();
	assert( joy.avail && inputResets == 0 && events.empty() );
	Poll();
	assert( joy.avail && events.empty() );
	fakeFailures = 3;
	Poll();
	Poll();
	assert( joy.avail );
	Poll();
	assert( !joy.avail && inputResets == 1 && uiJoyAvail == "0" && events.empty() );
	const int readsWhenClosed = posCalls;
	Poll();
	assert( posCalls == readsWhenClosed && events.empty() );

	// Unplugging closes it at once.
	ResetCounters();
	IN_StartupJoystick();
	assert( joy.avail );
	fakeState.dwButtons = 0;
	Poll();
	fakeFailures = 1;
	fakeFailure = JOYERR_UNPLUGGED;
	Poll();
	assert( !joy.avail && inputResets == 1 && events.empty() );
	fakeFailure = JOYERR_PARMS;

	// Quake Live's opt-in profile: the left stick is analog movement on the
	// side and forward axes, the R/U axes turn the view.
	profileCvar.integer = 1;
	ResetCounters();
	IN_StartupJoystick();
	assert( joy.avail );
	Poll();
	assert( events.empty() );
	fakeState.dwXpos = 65535;
	fakeState.dwYpos = 0;
	Poll();
	const int side = fnql::input::RetailJoystickMoveAxis(
		fnql::input::NormaliseJoystickAxis( 65535 ), horizMoveDeadCvar.value, ballScaleCvar.value );
	const int forward = fnql::input::RetailJoystickMoveAxis(
		fnql::input::NormaliseJoystickAxis( 0 ), vertMoveDeadCvar.value, ballScaleCvar.value );
	assert( side > 0 && forward < 0 );
	assert( Has( SE_JOYSTICK_AXIS, AXIS_SIDE, side ) && Has( SE_JOYSTICK_AXIS, AXIS_FORWARD, forward ) );
	assert( !Has( SE_KEY, K_RIGHTARROW, qtrue ) && !Has( SE_KEY, K_UPARROW, qtrue ) );
	fakeState.dwXpos = fakeState.dwYpos = kCentre;
	fakeState.dwRpos = 65535;
	Poll();
	assert( Has( SE_JOYSTICK_AXIS, AXIS_SIDE, 0 ) && Has( SE_JOYSTICK_AXIS, AXIS_FORWARD, 0 ) );
	bool turned = false;
	for ( const QueuedEvent &event : events ) {
		turned |= event.type == SE_MOUSE && event.value > 0 && event.value2 == 0;
	}
	assert( turned );
	profileCvar.integer = 0;

	puts( "winmm joystick checks passed" );
	return 0;
}
'''


def build_source() -> str:
    common = (ROOT / 'code/qcommon/qcommon.h').read_text(encoding='utf-8')
    source = (ROOT / 'code/win32/win_input.cpp').read_text(encoding='utf-8')
    parts = [
        PRELUDE,
        block(common, 'typedef enum {\n\tAXIS_SIDE,', '} joystickAxis_t;'),
        block(common, 'typedef enum {\n  // bk001129', '} sysEventType_t;'),
        STUBS,
        block(source, '#define\tJOY_MAX_AXES', 'static\tjoystickInfo_t\tjoy;'),
    ]
    for signature in (
        'static void IN_ClearWinMMJoystickState( qboolean disable )\n{',
        'void IN_StartupJoystick (void) {',
        'float JoyToF( int value ) {',
        'int JoyToI( int value ) {',
        'static void IN_QueueRetailJoystickAxis( int axis, int value ) {',
    ):
        parts.append(function(source, signature))
    parts.append(block(source, 'int\tjoyDirectionKeys[16] = {', '};'))
    parts.append(block(source, 'enum winMMAxis_t {', '};'))
    parts.append(function(
        source,
        'static qboolean IN_WinMMAxisPosition( winMMAxis_t axis, DWORD *position )\n{'))
    parts.append(function(source, 'void IN_JoyMove( void ) {'))
    parts.append(CHECKS)
    return ''.join(parts)


class WinMMJoystickProducerTests(unittest.TestCase):
    def test_simulated_device_through_the_production_producer(self) -> None:
        if os.name != 'nt':
            self.skipTest('WinMM headers are Windows-only')
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='winmm-joystick-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(build_source(), encoding='utf-8')
            build = subprocess.run(
                [compiler, *compiler_target_args(compiler), '-std=c++17', '-O1',
                 '-Wall', '-Wno-unused-function', '-Werror',
                 '-I', str(ROOT), str(cpp), '-o', str(exe)],
                cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(exe)], cwd=folder, capture_output=True, text=True, timeout=30,
                creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('winmm joystick checks passed', run.stdout)


if __name__ == '__main__':
    unittest.main()
