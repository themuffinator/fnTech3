/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* American McGee's Alice's user command builder, against the rules read out
 * of retail alice.exe in code/client/alice_input.hpp. Nothing here reads an
 * input device or loads a retail module: each rule is exercised on its own,
 * so a change that quietly reshapes a command fails here. */

#include "../code/client/alice_input.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int failures;

void Expect( bool condition, const char *message ) {
	if ( !condition ) {
		std::fprintf( stderr, "FAIL: %s\n", message );
		++failures;
	}
}

using namespace fntech3::alice::input;

void CheckButtons() {
	/* The run bit is +speed XOR cl_run, and nothing else sets it but +button3.
	 * Walking and running are the same 127 on the move bytes; this bit is the
	 * only difference the game sees. */
	Expect( CommandButtons( 0, false, false, false, false ) == 0,
		"cl_run 0 and no +speed walks: no run bit" );
	Expect( CommandButtons( 0, true, false, false, false ) == Run,
		"+speed with cl_run 0 runs" );
	Expect( CommandButtons( 0, false, true, false, false ) == Run,
		"cl_run 1 without +speed runs" );
	Expect( CommandButtons( 0, true, true, false, false ) == 0,
		"+speed with cl_run 1 walks" );
	Expect( CommandButtons( 1u << 3, true, true, false, false ) == Run,
		"+button3 sets the run bit whatever speed says" );

	/* Each of the seven buttons lands on its own bit, and nothing above them
	 * leaks in: bit 7 belongs to the any-key test alone. Walking, so the run
	 * bit stays out of the way. */
	for ( int i = 0; i < kCommandButtons; ++i ) {
		const std::uint8_t pressed = static_cast<std::uint8_t>( 1u << i );
		const std::uint8_t buttons = CommandButtons( pressed, true, true, false, false );
		Expect( buttons == pressed, "a button lands on its own bit and no other" );
	}
	Expect( CommandButtons( 0x80, true, true, false, false ) == 0,
		"a pressed bit above the seven buttons is not carried" );
	Expect( AttackRight == 1 && AttackLeft == 2 && Sneak == 4 && Run == 8
		&& Holster == 16 && Use == 32 && Talk == 64 && AnyKey == 128,
		"the bit names are retail's command wiring" );

	/* No Quake III walking or talk bit appears, whatever the catchers do. */
	for ( int speed = 0; speed < 2; ++speed ) {
		for ( int run = 0; run < 2; ++run ) {
			for ( int captured = 0; captured < 2; ++captured ) {
				const std::uint8_t buttons = CommandButtons( 0, speed != 0,
					run != 0, false, captured != 0 );
				Expect( ( buttons & ( Holster | AttackLeft ) ) == 0,
					"no walking bit lands on holster and no talk bit on attackleft" );
			}
		}
	}

	/* The any-key bit only while nothing captures gameplay input. */
	Expect( CommandButtons( 0, true, true, true, false ) == AnyKey,
		"a key down with no catcher is the any-key bit" );
	Expect( CommandButtons( 0, true, true, true, true ) == 0,
		"a catcher suppresses it" );
	Expect( CommandButtons( 0, true, true, false, false ) == 0,
		"no key down, no bit" );
}

void CheckClampAndTruncate() {
	Expect( ClampChar( 200 ) == 127 && ClampChar( -200 ) == -128,
		"ClampChar holds -128 to 127" );
	Expect( ClampChar( 127 ) == 127 && ClampChar( -128 ) == -128 && ClampChar( 0 ) == 0,
		"ClampChar keeps its bounds and what is inside them" );
	Expect( Truncate( 63.9, 0 ) == 63 && Truncate( -63.9, 0 ) == -63,
		"truncation is toward zero" );
	Expect( Truncate( std::numeric_limits<double>::quiet_NaN(), 5 ) == 5,
		"a NaN keeps the current value" );
	Expect( Truncate( std::numeric_limits<double>::infinity(), 5 ) == 5,
		"an infinity keeps the current value" );
	Expect( Truncate( 1e30, 0 ) == ( std::numeric_limits<int>::max )(),
		"a huge value saturates rather than overflowing" );
}

void CheckKeyMove() {
	KeyFractions keys;
	keys.forward = 1.0f;
	Move move = KeyMove( keys );
	Expect( move.forward == 127 && move.right == 0 && move.up == 0,
		"a full frame of +forward is 127: no walking speed" );

	keys = {};
	keys.back = 1.0f;
	keys.moveLeft = 1.0f;
	keys.down = 1.0f;
	move = KeyMove( keys );
	Expect( move.forward == -127 && move.right == -127 && move.up == -127,
		"the negative terms are 127 as well" );

	/* The truncation order is observable. */
	keys = {};
	keys.strafe = true;
	keys.right = 0.5f;
	keys.left = 0.5f;
	move = KeyMove( keys );
	Expect( move.right == 0, "trunc(63.5) = 63, then trunc(63 - 63.5) = 0" );

	keys = {};
	keys.moveRight = 0.5f;
	keys.moveLeft = 0.25f;
	move = KeyMove( keys );
	Expect( move.right == 31, "trunc(63.5) = 63, then trunc(63 - 31.75) = 31" );

	/* +right and +left move sideways only under +strafe. */
	keys = {};
	keys.right = 1.0f;
	move = KeyMove( keys );
	Expect( move.right == 0, "+right does not strafe without +strafe" );
	keys.strafe = true;
	move = KeyMove( keys );
	Expect( move.right == 127, "+right strafes under +strafe" );

	/* Everything sideways at once clamps at the store, not before. */
	keys = {};
	keys.strafe = true;
	keys.right = 1.0f;
	keys.moveRight = 1.0f;
	move = KeyMove( keys );
	Expect( move.right == 127, "254 sideways clamps to 127 at the store" );
}

MouseParams Mouse( float mx, float my ) {
	MouseParams p;
	p.mx = mx;
	p.my = my;
	p.frameMsec = 10;
	p.sensitivity = 5.0f;
	p.acceleration = 0.0f;
	return p;
}

bool Near( float a, float b ) {
	return std::fabs( a - b ) <= 1e-4f * ( 1.0f + std::fabs( b ) );
}

void CheckMouse() {
	/* accel = sensitivity + rate * cl_mouseAccel, rate = |d| / frame_msec. */
	MouseParams p = Mouse( 30.0f, 40.0f );
	p.acceleration = 0.5f;
	MouseResult r = MouseMove( p, Move{} );
	Expect( Near( r.rate, 5.0f ), "the rate is the distance over the frame" );
	Expect( Near( r.accel, 7.5f ), "accel is sensitivity plus rate times cl_mouseAccel" );
	Expect( r.moved, "a moving mouse moves" );
	Expect( Near( r.yaw, -30.0f * 7.5f * 0.022f ), "yaw turns by -mx * accel * m_yaw" );
	Expect( Near( r.pitch, 40.0f * 7.5f * 0.022f ), "freelook pitches by my * accel * m_pitch" );
	Expect( Near( r.cameraYaw, r.yaw ) && Near( r.cameraPitch, r.pitch ),
		"the camera accumulates what the view turns" );
	Expect( r.move.forward == 0 && r.move.right == 0 && r.move.up == 0,
		"freelook moves nothing" );

	/* No motion, no camera, nothing: the early return. */
	r = MouseMove( Mouse( 0.0f, 0.0f ), Move{} );
	Expect( !r.moved && r.rate == 0.0f && r.cameraYaw == 0.0f && r.yaw == 0.0f,
		"no motion changes nothing" );

	/* A zero sensitivity scales motion to nothing, which is also the early
	 * return, even though the rate itself is not zero. */
	p = Mouse( 10.0f, 0.0f );
	p.sensitivity = 0.0f;
	r = MouseMove( p, Move{} );
	Expect( !r.moved && r.rate != 0.0f, "motion scaled to zero is no motion" );

	/* +cameralook: the camera still accumulates, the view does not turn. */
	p = Mouse( 10.0f, -20.0f );
	p.cameraLook = true;
	r = MouseMove( p, Move{ 5, 6, 7 } );
	Expect( r.moved && Near( r.cameraYaw, -10.0f * 5.0f * 0.022f )
		&& Near( r.cameraPitch, -20.0f * 5.0f * 0.022f ),
		"+cameralook still feeds the camera" );
	Expect( r.yaw == 0.0f && r.pitch == 0.0f, "+cameralook turns no view" );
	Expect( r.move.forward == 5 && r.move.right == 6 && r.move.up == 7,
		"+cameralook moves nothing" );

	/* +strafe: X to right, Y to forward twice and to up. */
	p = Mouse( 4.0f, 2.0f );
	p.strafe = true;
	p.up = 1.0f;
	r = MouseMove( p, Move{} );
	Expect( r.yaw == 0.0f && r.pitch == 0.0f, "+strafe turns no view" );
	Expect( r.move.right == 5, "right += mx * accel * m_side, 4*5*0.25 = 5" );
	Expect( r.move.forward == -4,
		"forward -= my * accel * m_forward twice: trunc(-2.5) = -2, trunc(-2 - 2.5) = -4" );
	Expect( r.move.up == -10, "up -= my * accel * m_up, 2*5*1 = 10" );
	Expect( Near( r.cameraYaw, -4.0f * 5.0f * 0.022f ),
		"the camera accumulates under +strafe too" );

	/* m_up defaults to zero, so up stays put unless a player sets it. */
	p = Mouse( 0.0f, 2.0f );
	p.strafe = true;
	r = MouseMove( p, Move{ 0, 0, 9 } );
	Expect( r.move.up == 9, "m_up 0 leaves up alone" );

	/* Neither freelook nor +mlook: Y moves forward once. */
	p = Mouse( 0.0f, 4.0f );
	p.freelook = false;
	r = MouseMove( p, Move{} );
	Expect( r.pitch == 0.0f && r.move.forward == -5, "without freelook Y moves forward" );
	p.mlook = true;
	r = MouseMove( p, Move{} );
	Expect( Near( r.pitch, 4.0f * 5.0f * 0.022f ) && r.move.forward == 0,
		"+mlook pitches without freelook" );

	/* The move bytes clamp at -128, not -127. */
	p = Mouse( 0.0f, 200.0f );
	p.freelook = false;
	r = MouseMove( p, Move{ -100, 0, 0 } );
	Expect( r.move.forward == -128, "a mouse move clamps at -128" );

	/* m_filter averages the two slots; otherwise the current one is read. */
	const int counts[2] = { 3, 6 };
	Expect( SampleCount( counts, 1, true ) == 4.5f, "the filter averages both slots" );
	Expect( SampleCount( counts, 1, false ) == 6.0f, "without it the current slot" );
	const int huge[2] = { ( std::numeric_limits<int>::max )(), 10 };
	Expect( std::isfinite( SampleCount( huge, 0, true ) ) && SampleCount( huge, 0, true ) > 0.0f,
		"the filter's sum saturates instead of wrapping" );
}

void CheckCursorAndMask() {
	Expect( CursorStep( 10, -20, 640 ) == 0, "the cursor stops at zero" );
	Expect( CursorStep( 630, 20, 640 ) == 640, "and at the video bound" );
	Expect( CursorStep( 100, 5, 640 ) == 105, "and moves between them" );
	Expect( CursorStep( ( std::numeric_limits<int>::max )(), 5, 640 ) == 640,
		"a saturated sum still clamps" );

	int mask = 0;
	mask = MouseButtonMask( mask, 0, true );
	mask = MouseButtonMask( mask, 4, true );
	Expect( mask == 0x11, "MOUSE1 is bit 0 and MOUSE5 bit 4" );
	mask = MouseButtonMask( mask, 0, false );
	Expect( mask == 0x10, "a release clears its bit" );
	Expect( MouseButtonMask( mask, 5, true ) == mask, "MOUSE6 and above are not in the mask" );
}

void CheckBindingChoice() {
	Expect( PressBinding( true, true, true, true ) == BindingTable::Alt,
		"alt wins over ctrl" );
	Expect( PressBinding( true, false, true, true ) == BindingTable::Ctrl,
		"with no alt binding, ctrl" );
	Expect( PressBinding( false, true, false, true ) == BindingTable::Normal,
		"a key pressed with no modifier runs its own binding" );
	Expect( PressBinding( true, false, false, true ) == BindingTable::Normal,
		"a binding for a modifier that was not held is not run" );
}

} // namespace

int main() {
	CheckButtons();
	CheckClampAndTruncate();
	CheckKeyMove();
	CheckMouse();
	CheckCursorAndMask();
	CheckBindingChoice();

	if ( failures ) {
		std::fprintf( stderr, "%d Alice input checks failed\n", failures );
		return 1;
	}
	std::printf( "Alice input tests passed\n" );
	return 0;
}
