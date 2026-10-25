"""Drive the SDL3 client's joystick and gamepad producer against a fake SDL3.

The production joystick functions and key tables are cut out of
code/sdl/sdl_input.cpp and compiled against a small stand-in for SDL3's
joystick and gamepad calls, so the checks need no controller, no SDL3 headers
and touch no input device. They pin what SDL3 builds do now that every
platform compiles joystick support: no SDL3 joystick or gamepad subsystem and
an empty device list until in_joystick is set, one subsystem reference however
often the devices are listed again, and the keys a recognised gamepad queues:
Quake Live keeps FnQL's named PAD0 keys, and every other game gets the JOY
numbers and arrow keys its modules know, which are checked against each
game's key adapter.
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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "code/client/input_compat.hpp"
#include "code/client/keycodes.h"
#include "code/compat/q3_key_adapter.h"
#include "code/compat/stvef_key_adapter.h"
#include "code/compat/jka_key_adapter.h"
#include "code/compat/wolfet_key_adapter.h"

typedef enum { qfalse = 0, qtrue } qboolean;
#define ARRAY_LEN( x ) ( sizeof( x ) / sizeof( *( x ) ) )
'''

FAKE_SDL = r'''
// SDL3 3.4.0's values for everything the producer reads.
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef Uint32 SDL_JoystickID;
struct SDL_Joystick { int unused; };
struct SDL_Gamepad { int unused; };
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_GAMEPAD 0x00002000u
#define SDL_HAT_CENTERED 0x00
#define SDL_HAT_UP 0x01
#define SDL_HAT_RIGHT 0x02
#define SDL_HAT_DOWN 0x04
#define SDL_HAT_LEFT 0x08
#define SDL_HAT_RIGHTUP ( SDL_HAT_RIGHT | SDL_HAT_UP )
#define SDL_HAT_RIGHTDOWN ( SDL_HAT_RIGHT | SDL_HAT_DOWN )
#define SDL_HAT_LEFTUP ( SDL_HAT_LEFT | SDL_HAT_UP )
#define SDL_HAT_LEFTDOWN ( SDL_HAT_LEFT | SDL_HAT_DOWN )
typedef enum SDL_GamepadButton {
	SDL_GAMEPAD_BUTTON_INVALID = -1,
	SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST,
	SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE,
	SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK,
	SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
	SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP,
	SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
	SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_MISC1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
	SDL_GAMEPAD_BUTTON_TOUCHPAD, SDL_GAMEPAD_BUTTON_MISC2,
	SDL_GAMEPAD_BUTTON_MISC3, SDL_GAMEPAD_BUTTON_MISC4,
	SDL_GAMEPAD_BUTTON_MISC5, SDL_GAMEPAD_BUTTON_MISC6,
	SDL_GAMEPAD_BUTTON_COUNT
} SDL_GamepadButton;
typedef enum SDL_GamepadAxis {
	SDL_GAMEPAD_AXIS_INVALID = -1,
	SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX,
	SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
	SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_GAMEPAD_AXIS_COUNT
} SDL_GamepadAxis;

struct FakeDevice { SDL_JoystickID id; const char *name; bool gamepad; };
static std::vector<FakeDevice> devices;
static int initCalls, quitCalls, enumerations, joystickRefs, gamepadRefs;
static SDL_JoystickID openedStick;
static SDL_Joystick theStick;
static SDL_Gamepad thePad;
static bool padButtons[SDL_GAMEPAD_BUTTON_COUNT];
static Sint16 padAxes[SDL_GAMEPAD_AXIS_COUNT];
static bool rawButtons[32];
static Sint16 rawAxes[8];
static int rawButtonCount, rawAxisCount;

static bool SDL_InitSubSystem( Uint32 flags ) {
	++initCalls;
	if ( flags & SDL_INIT_JOYSTICK ) ++joystickRefs;
	if ( flags & SDL_INIT_GAMEPAD ) ++gamepadRefs;
	return true;
}
static void SDL_QuitSubSystem( Uint32 flags ) {
	++quitCalls;
	if ( flags & SDL_INIT_JOYSTICK ) --joystickRefs;
	if ( flags & SDL_INIT_GAMEPAD ) --gamepadRefs;
	assert( joystickRefs >= 0 && gamepadRefs >= 0 );
}
static const char *SDL_GetError( void ) { return "fake"; }
static void SDL_free( void *memory ) { free( memory ); }
static SDL_JoystickID *SDL_GetJoysticks( int *count ) {
	assert( joystickRefs > 0 );
	++enumerations;
	*count = static_cast<int>( devices.size() );
	SDL_JoystickID *ids = static_cast<SDL_JoystickID *>(
		malloc( ( devices.size() + 1 ) * sizeof( SDL_JoystickID ) ) );
	for ( size_t i = 0; i < devices.size(); ++i ) ids[i] = devices[i].id;
	ids[devices.size()] = 0;
	return ids;
}
static const FakeDevice *Device( SDL_JoystickID id ) {
	for ( const FakeDevice &device : devices ) if ( device.id == id ) return &device;
	return nullptr;
}
static const char *SDL_GetJoystickNameForID( SDL_JoystickID id ) {
	const FakeDevice *device = Device( id );
	return device ? device->name : nullptr;
}
static SDL_Joystick *SDL_OpenJoystick( SDL_JoystickID id ) {
	assert( joystickRefs > 0 && Device( id ) );
	openedStick = id;
	return &theStick;
}
static bool SDL_IsGamepad( SDL_JoystickID id ) {
	assert( gamepadRefs > 0 );
	return Device( id )->gamepad;
}
static SDL_Gamepad *SDL_OpenGamepad( SDL_JoystickID ) { return &thePad; }
static void SDL_CloseGamepad( SDL_Gamepad *gamepad ) { assert( gamepad == &thePad ); }
static void SDL_CloseJoystick( SDL_Joystick *stick ) { assert( stick == &theStick ); openedStick = 0; }
static int SDL_GetNumJoystickAxes( SDL_Joystick * ) { return rawAxisCount; }
static int SDL_GetNumJoystickHats( SDL_Joystick * ) { return 0; }
static int SDL_GetNumJoystickButtons( SDL_Joystick * ) { return rawButtonCount; }
static int SDL_GetNumJoystickBalls( SDL_Joystick * ) { return 0; }
static void SDL_UpdateGamepads( void ) {}
static void SDL_UpdateJoysticks( void ) {}
static bool SDL_GetGamepadButton( SDL_Gamepad *gamepad, SDL_GamepadButton button ) {
	assert( gamepad == &thePad && button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT );
	return padButtons[button];
}
static Sint16 SDL_GetGamepadAxis( SDL_Gamepad *gamepad, SDL_GamepadAxis axis ) {
	assert( gamepad == &thePad && axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT );
	return padAxes[axis];
}
static bool SDL_GetJoystickBall( SDL_Joystick *, int, int *dx, int *dy ) { *dx = *dy = 0; return true; }
static bool SDL_GetJoystickButton( SDL_Joystick *, int button ) { return rawButtons[button]; }
static Uint8 SDL_GetJoystickHat( SDL_Joystick *, int ) { return SDL_HAT_CENTERED; }
static Sint16 SDL_GetJoystickAxis( SDL_Joystick *, int axis ) { return rawAxes[axis]; }

namespace fnql {
namespace sdl {
template <typename T>
struct SdlFreeDeleter {
	void operator()( T *memory ) const noexcept { if ( memory ) SDL_free( memory ); }
};
template <typename T>
using ScopedSdlMemory = std::unique_ptr<T, SdlFreeDeleter<T>>;
}
}
'''

ENGINE = r'''
#define CVAR_ARCHIVE 0x0001
#define CVAR_LATCH 0x0020
#define CVAR_ROM 0x0040
struct cvar_t { std::string string; int integer; float value; };
static std::map<std::string, cvar_t> cvars;
static cvar_t *Cvar_Get( const char *name, const char *value, int ) {
	auto it = cvars.find( name );
	if ( it == cvars.end() ) {
		it = cvars.emplace( name, cvar_t{ value, atoi( value ),
			static_cast<float>( atof( value ) ) } ).first;
	}
	return &it->second;
}
static void Cvar_Set( const char *name, const char *value ) {
	cvar_t *cv = Cvar_Get( name, value, 0 );
	*cv = cvar_t{ value, atoi( value ), static_cast<float>( atof( value ) ) };
}
static void Cvar_SetDescription( cvar_t *, const char * ) {}
static void Q_strcat( char *dest, int size, const char *src ) {
	const size_t used = strlen( dest );
	assert( used + strlen( src ) < static_cast<size_t>( size ) );
	memcpy( dest + used, src, strlen( src ) + 1 );
}
static int Q_stricmp( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		const int ca = tolower( static_cast<unsigned char>( *a ) );
		const int cb = tolower( static_cast<unsigned char>( *b ) );
		if ( ca != cb || !ca ) return ca - cb;
	}
}
static void Com_DPrintf( const char *, ... ) {}
static int Sys_Milliseconds( void ) { return 100; }
static std::map<int, std::string> bindings;
static const char *Key_GetBinding( int key ) {
	auto it = bindings.find( key );
	return it == bindings.end() ? "" : it->second.c_str();
}
static qboolean quakeLiveProfile;
static qboolean CL_ModulesTakeGamepadKeys( void ) { return quakeLiveProfile; }
struct QueuedEvent { int type, value, value2; };
static std::vector<QueuedEvent> events;
static void Com_QueueEvent( int, sysEventType_t type, int value, int value2, int, void * ) {
	events.push_back( { type, value, value2 } );
}
static int in_eventTime = 0;
'''

CHECKS = r'''
// What the tables must be, written out independently of the production ones.
static const int kExpectedJoyButtons[21] = {
	K_JOY1, K_JOY2, K_JOY3, K_JOY4,		// A, B, X, Y
	K_JOY7, K_JOY11, K_JOY8,			// back, guide, start
	K_JOY9, K_JOY10, K_JOY5, K_JOY6,	// stick clicks, shoulders
	K_JOY24, K_JOY25, K_JOY27, K_JOY26,	// d-pad up, down, left, right
	K_JOY12, K_JOY28, K_JOY29, K_JOY30, K_JOY31, K_JOY13
};

static void Poll( void ) {
	events.clear();
	IN_JoyMove();
}

static void ExpectKeys( std::initializer_list<std::pair<int, int>> expected ) {
	assert( events.size() == expected.size() );
	size_t i = 0;
	for ( const auto &key : expected ) {
		assert( events[i].type == SE_KEY );
		assert( events[i].value == key.first && events[i].value2 == key.second );
		++i;
	}
}

static void Clear( void ) {
	memset( padButtons, 0, sizeof( padButtons ) );
	memset( padAxes, 0, sizeof( padAxes ) );
	Poll();
	events.clear();
}

static void SetCvar( const char *name, const char *value ) { Cvar_Set( name, value ); }

static void OpenDevices( qboolean quakeLive ) {
	quakeLiveProfile = quakeLive;
	SetCvar( "in_joystick", "1" );
	IN_InitJoystick();
}

static void CheckEveryGameDeliversTheJoystickTable( void ) {
	std::vector<int> keys( std::begin( kExpectedJoyButtons ), std::end( kExpectedJoyButtons ) );
	for ( int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i ) {
		if ( kJoystickGamepadKeys.axisNegative[i] ) keys.push_back( kJoystickGamepadKeys.axisNegative[i] );
		keys.push_back( kJoystickGamepadKeys.axisPositive[i] );
	}
	assert( keys.size() == 31 );
	for ( size_t i = 0; i < keys.size(); ++i ) {
		for ( size_t j = i + 1; j < keys.size(); ++j ) assert( keys[i] != keys[j] );
		// Quake III, Elite Force, RTCW, ET and Jedi Academy all have a number
		// for every key the table queues.
		assert( FnTech3_Q3KeyEventFromInternal( keys[i] ) >= 0 );
		assert( FnTech3_STVEFKeyEventFromInternal( keys[i] ) >= 0 );
		assert( FnTech3_RTCWKeyEventFromInternal( keys[i] ) >= 0 );
		assert( FnTech3_WolfETKeyEventFromInternal( keys[i] ) >= 0 );
		assert( FNTech3_JKAKeyEventFromInternal( keys[i] ) >= 0 );
	}
	for ( int i = 0; i < kSupportedGamepadButtonCount; ++i ) {
		assert( kJoystickGamepadKeys.buttons[i] == kExpectedJoyButtons[i] );
		// None of them has a number for a named gamepad key.
		const int named = kNamedGamepadKeys.buttons[i];
		assert( FnTech3_Q3KeyEventFromInternal( named ) == -1 );
		assert( FnTech3_STVEFKeyEventFromInternal( named ) == -1 );
		assert( FnTech3_RTCWKeyEventFromInternal( named ) == -1 );
		assert( FnTech3_WolfETKeyEventFromInternal( named ) == -1 );
		assert( FNTech3_JKAKeyEventFromInternal( named ) == -1 );
	}
	// The south face button reaches q3_ui as Quake III's joystick button 1,
	// which its menus take as Enter.
	assert( FnTech3_Q3KeyEventFromInternal( kJoystickGamepadKeys.buttons[0] ) == 185 );
}

int main() {
	static cvar_t threshold{ "0.15", 0, 0.15f };
	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH );
	in_joystickThreshold = &threshold;
	j_pitch = Cvar_Get( "j_pitch", "0.022", 0 );
	j_yaw = Cvar_Get( "j_yaw", "-0.022", 0 );
	j_forward = Cvar_Get( "j_forward", "-0.25", 0 );
	j_side = Cvar_Get( "j_side", "0.25", 0 );
	j_up = Cvar_Get( "j_up", "0", 0 );
	j_pitch_axis = Cvar_Get( "j_pitch_axis", "3", 0 );
	j_yaw_axis = Cvar_Get( "j_yaw_axis", "2", 0 );
	j_forward_axis = Cvar_Get( "j_forward_axis", "1", 0 );
	j_side_axis = Cvar_Get( "j_side_axis", "0", 0 );
	j_up_axis = Cvar_Get( "j_up_axis", "4", 0 );

	// Off: no SDL3 joystick or gamepad subsystem, no listing, and a list left
	// by an earlier enumeration is emptied.
	devices = { { 7, "Arcade stick", false }, { 9, "Pad", true } };
	Cvar_Set( "in_availableJoysticks", "Old pad\n" );
	IN_InitJoystick();
	assert( initCalls == 0 && enumerations == 0 && openedStick == 0 );
	assert( cvars["in_availableJoysticks"].string.empty() );
	assert( !s_joystickSubsystemAcquired && !s_gamepadSubsystemAcquired );
	Poll();
	assert( events.empty() );
	IN_ShutdownJoystick();
	assert( quitCalls == 0 );

	// On with no device: both subsystems once, an empty list, nothing open.
	devices.clear();
	OpenDevices( qfalse );
	assert( initCalls == 2 && joystickRefs == 1 && gamepadRefs == 1 );
	assert( cvars["in_availableJoysticks"].string.empty() && openedStick == 0 );
	// A hot-plug re-enumeration takes no further reference.
	devices = { { 7, "Arcade stick", false }, { 9, "Pad", true } };
	SetCvar( "in_joystickNo", "1" );
	IN_InitJoystick();
	assert( initCalls == 2 && enumerations == 2 );
	assert( cvars["in_availableJoysticks"].string == "Arcade stick\nPad\n" );
	assert( openedStick == 9 && gamepad == &thePad );
	assert( s_gamepadKeys == &kJoystickGamepadKeys );
	IN_ShutdownJoystick();
	assert( quitCalls == 2 && joystickRefs == 0 && gamepadRefs == 0 && !gamepad );

	CheckEveryGameDeliversTheJoystickTable();

	// Every game but Quake Live: each button queues its JOY key once per change.
	OpenDevices( qfalse );
	Clear();
	for ( int i = 0; i < kSupportedGamepadButtonCount; ++i ) {
		padButtons[i] = true;
		Poll();
		ExpectKeys( { { kExpectedJoyButtons[i], qtrue } } );
		Poll();
		assert( events.empty() );
		padButtons[i] = false;
		Poll();
		ExpectKeys( { { kExpectedJoyButtons[i], qfalse } } );
	}
	// SDL3's buttons past the touchpad have no key.
	for ( int i = SDL_GAMEPAD_BUTTON_MISC2; i < SDL_GAMEPAD_BUTTON_COUNT; ++i ) padButtons[i] = true;
	Poll();
	assert( events.empty() );
	Clear();

	// The left stick is the arrow keys, inside the threshold nothing.
	padAxes[SDL_GAMEPAD_AXIS_LEFTX] = 3000;
	Poll();
	assert( events.empty() );
	padAxes[SDL_GAMEPAD_AXIS_LEFTX] = -32768;
	padAxes[SDL_GAMEPAD_AXIS_LEFTY] = -20000;
	Poll();
	ExpectKeys( { { K_LEFTARROW, qtrue }, { K_UPARROW, qtrue } } );
	padAxes[SDL_GAMEPAD_AXIS_LEFTX] = 0;
	padAxes[SDL_GAMEPAD_AXIS_LEFTY] = 32767;
	Poll();
	ExpectKeys( { { K_LEFTARROW, qfalse }, { K_UPARROW, qfalse }, { K_DOWNARROW, qtrue } } );
	Clear();
	padAxes[SDL_GAMEPAD_AXIS_LEFTX] = 32767;
	Poll();
	ExpectKeys( { { K_RIGHTARROW, qtrue } } );
	Clear();

	// The right stick is JOY18-JOY21, and a crossing releases before it presses.
	padAxes[SDL_GAMEPAD_AXIS_RIGHTX] = -30000;
	padAxes[SDL_GAMEPAD_AXIS_RIGHTY] = 30000;
	Poll();
	ExpectKeys( { { K_JOY18, qtrue }, { K_JOY21, qtrue } } );
	padAxes[SDL_GAMEPAD_AXIS_RIGHTX] = 30000;
	padAxes[SDL_GAMEPAD_AXIS_RIGHTY] = -30000;
	Poll();
	ExpectKeys( { { K_JOY18, qfalse }, { K_JOY21, qfalse }, { K_JOY19, qtrue }, { K_JOY20, qtrue } } );
	Clear();

	// The triggers are JOY16 and JOY17.
	padAxes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 32767;
	Poll();
	ExpectKeys( { { K_JOY16, qtrue } } );
	padAxes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767;
	Poll();
	ExpectKeys( { { K_JOY17, qtrue } } );
	Clear();

	// An input reset drops the producer's snapshot, so a held button is
	// pressed again on the next poll.
	padButtons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
	Poll();
	ExpectKeys( { { K_JOY1, qtrue } } );
	stick_state = {};
	Poll();
	ExpectKeys( { { K_JOY1, qtrue } } );
	Clear();
	IN_ShutdownJoystick();

	// Analog: an arrow key bound to +forward drives the forward axis instead.
	SetCvar( "in_joystickUseAnalog", "1" );
	bindings[K_UPARROW] = "+forward";
	OpenDevices( qfalse );
	Clear();
	padAxes[SDL_GAMEPAD_AXIS_LEFTY] = -32767;
	padAxes[SDL_GAMEPAD_AXIS_RIGHTX] = 30000;
	Poll();
	assert( events.size() == 2 );
	assert( events[0].type == SE_KEY && events[0].value == K_JOY19 && events[0].value2 == qtrue );
	assert( events[1].type == SE_JOYSTICK_AXIS && events[1].value == j_forward_axis->integer );
	assert( events[1].value2 == fnql::input::ApplyJoystickDeadzone( -32767, 0.15f ) );
	Clear();
	IN_ShutdownJoystick();
	SetCvar( "in_joystickUseAnalog", "0" );
	bindings.clear();

	// Quake Live keeps FnQL's named keys.
	OpenDevices( qtrue );
	assert( s_gamepadKeys == &kNamedGamepadKeys );
	Clear();
	for ( int i = 0; i < kSupportedGamepadButtonCount; ++i ) {
		const int expected = i >= SDL_GAMEPAD_BUTTON_MISC1
			? K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1 : K_PAD0_SOUTH + i;
		padButtons[i] = true;
		Poll();
		ExpectKeys( { { expected, qtrue } } );
		padButtons[i] = false;
		Poll();
		ExpectKeys( { { expected, qfalse } } );
	}
	padAxes[SDL_GAMEPAD_AXIS_LEFTX] = -32768;
	padAxes[SDL_GAMEPAD_AXIS_RIGHTY] = 32767;
	padAxes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767;
	Poll();
	ExpectKeys( { { K_PAD0_LEFTSTICK_LEFT, qtrue }, { K_PAD0_RIGHTSTICK_DOWN, qtrue },
		{ K_PAD0_RIGHTTRIGGER, qtrue } } );
	Clear();
	IN_ShutdownJoystick();

	// A device SDL3 does not recognise as a gamepad keeps the raw mapping in
	// every game: buttons are JOY1 onwards and the first two axes are arrows.
	devices = { { 7, "Arcade stick", false } };
	SetCvar( "in_joystickNo", "0" );
	rawButtonCount = 2;
	rawAxisCount = 2;
	OpenDevices( qfalse );
	assert( openedStick == 7 && !gamepad );
	rawButtons[1] = true;
	rawAxes[0] = -32000;
	Poll();
	ExpectKeys( { { K_JOY2, qtrue }, { K_LEFTARROW, qtrue } } );
	IN_ShutdownJoystick();
	assert( joystickRefs == 0 && gamepadRefs == 0 );

	puts( "sdl joystick checks passed" );
	return 0;
}
'''


def build_source() -> str:
    common = (ROOT / 'code/qcommon/qcommon.h').read_text(encoding='utf-8')
    source = (ROOT / 'code/sdl/sdl_input.cpp').read_text(encoding='utf-8')
    parts = [
        PRELUDE,
        block(common, 'typedef enum {\n\tAXIS_SIDE,', '} joystickAxis_t;'),
        block(common, 'typedef enum {\n  // bk001129', '} sysEventType_t;'),
        FAKE_SDL,
        ENGINE,
        block(source, 'static SDL_Gamepad *gamepad;', 'static SDL_JoystickID stickInstance;'),
        block(source, 'static cvar_t *in_joystick;', 'static cvar_t *j_up_axis;'),
        block(source, '// We translate axes movement into keypresses',
              'static joystickState_t stick_state;'),
    ]
    for signature in (
        'static void IN_CloseJoystickHandles( void )\n{',
        'static qboolean IN_AcquireJoystickSubsystem(',
        'static void IN_InitJoystick( void )\n{',
        'static void IN_ShutdownJoystick( void )\n{',
        'static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)\n{',
        'static void IN_GamepadMove( void )\n{',
        'static void IN_QueueHatKeys( Uint8 hat, int hatIndex, qboolean down )\n{',
        'static Uint8 IN_GetPackedHat( unsigned int hats, int index )\n{',
        'static void IN_JoyMove( void )\n{',
    ):
        parts.append(function(source, signature))
    parts.append(CHECKS)
    return ''.join(parts)


def compiler() -> str | None:
    return shutil.which('clang++') or shutil.which('g++')


class SDLJoystickProducerTests(unittest.TestCase):
    def test_fake_devices_through_the_production_producer(self) -> None:
        cxx = compiler()
        if not cxx:
            self.skipTest('No C++ compiler')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='sdl-joystick-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp = folder / 'probe.cpp'
            exe = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(build_source(), encoding='utf-8')
            build = subprocess.run(
                [cxx, *compiler_target_args(cxx), '-std=c++17', '-O1',
                 '-Wall', '-Wno-unused-function', '-Wno-sign-compare', '-Werror',
                 '-I', str(ROOT), str(cpp), '-o', str(exe)],
                cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(exe)], cwd=folder, capture_output=True, text=True, timeout=30,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('sdl joystick checks passed', run.stdout)

    def test_recognised_gamepads_follow_the_profile_key_vocabulary(self) -> None:
        source = (ROOT / 'code/sdl/sdl_input.cpp').read_text(encoding='utf-8')
        keys = (ROOT / 'code/client/cl_keys.cpp').read_text(encoding='utf-8')
        initialize = function(source, 'static void IN_InitJoystick( void )\n{')
        self.assertLess(initialize.index('gamepad = SDL_OpenGamepad(stickInstance);'),
                        initialize.index('s_gamepadKeys = CL_ModulesTakeGamepadKeys()'))
        predicate = function(keys, 'qboolean CL_ModulesTakeGamepadKeys( void ) {')
        self.assertIn('return Com_GameIsQuakeLive() ? qtrue : qfalse;', predicate)
        self.assertIn('qboolean CL_ModulesTakeGamepadKeys( void );',
                      (ROOT / 'code/client/client.h').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
