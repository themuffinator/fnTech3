/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_ALICE_INPUT_HPP
#define FNTECH3_CLIENT_ALICE_INPUT_HPP

/* American McGee's Alice's user command builder, as retail alice.exe builds
 * it, with nothing of the engine around it so each rule can be tested alone.
 *
 * Retail's builder is Quake III's with Ritual's changes, and every one of
 * them is read out of the engine's own routines rather than assumed:
 *
 *   CL_CmdButtons    0x00428750   seven buttons, run, any key; nothing else
 *   CL_KeyMove       0x00428170   every term at 127, truncated as it goes
 *   CL_MouseMove     0x004284c0   a camera accumulator, and strafe feeding up
 *   CL_MouseEvent    0x00428310   a cursor instead of motion in cursor mode
 *   ClampChar        0x00447f70   -128 to 127
 *
 * What the button bits mean to the game is the game's: fgamex86.dll's pmove
 * tests bit 3 as run (0x10066498) and bit 6 as a movement lockout
 * (0x1006732b), and `Player::ClientMove` tests bit 7 as the intermission
 * exit (0x100feac8). The names of bits 2 and 4 are their commands' names.
 *
 * Retail's joystick consumer (0x004283e0) is not here: the retail Win32 layer
 * never posts an axis event, so it reads axes that stay zero. fnTech3 keeps
 * its own joystick movement for this title, which is a capability beyond
 * retail rather than a contract. See docs/fntech3/2026-09-22-alice-wire.md for
 * the command's twenty bytes. */

#include <cmath>
#include <cstdint>
#include <limits>

namespace fntech3::alice::input {

/* The button byte at command offset 5. */
enum : std::uint8_t {
	AttackRight = 1,	// +attackright, in_buttons[0]
	AttackLeft = 2,		// +attackleft, in_buttons[1]
	Sneak = 4,			// +sneak, in_buttons[2]
	Run = 8,			// +speed against cl_run, and +button3
	Holster = 16,		// +holster, in_buttons[4]
	Use = 32,			// +use, in_buttons[5]
	Talk = 64,			// +button6 only; the game locks movement on it
	AnyKey = 128,		// any key down while no catcher is up
};

/* CL_CmdButtons walks in_buttons[0..6] and stops: its loop runs from the
 * first button's `wasPressed` at 0x00dcee14 while below 0x00dceebc, which is
 * seven of the 24-byte records. `+button7` and above are not registered. */
inline constexpr int kCommandButtons = 7;

/* CL_KeyMove multiplies every key fraction by the float at 0x004ffa20,
 * which is 127. There is no walking speed: +speed only sets the run bit. */
inline constexpr double kMoveSpeed = 127.0;

/* ClampChar at 0x00447f70: below -128 is -128, above 127 is 127. Quake III
 * 1.32's own ClampChar bounds the same way; fnTech3's ClampCharMove stops at
 * -127 and is not what this title stores. */
constexpr int ClampChar( int value ) noexcept {
	return value < -128 ? -128 : value > 127 ? 127 : value;
}

/* MSVC's _ftol (0x004e6d40), which truncates toward zero. A value that is not
 * finite or does not fit keeps the caller's current value rather than reaching
 * an undefined conversion: retail would have produced 0x80000000 there, which
 * ClampChar then turns into -128. */
inline int Truncate( double value, int fallback ) noexcept {
	if ( !std::isfinite( value ) ) {
		return fallback;
	}
	if ( value >= static_cast<double>( ( std::numeric_limits<int>::max )() ) ) {
		return ( std::numeric_limits<int>::max )();
	}
	if ( value <= static_cast<double>( ( std::numeric_limits<int>::min )() ) ) {
		return ( std::numeric_limits<int>::min )();
	}
	return static_cast<int>( value );
}

/* One term of a move: the running integer plus a float, truncated. Retail
 * truncates after every term rather than once at the end, and the order is
 * observable: right 0.5 then left 0.5 is trunc(63.5) = 63, then
 * trunc(63 - 63.5) = 0. */
inline int Step( int current, double delta ) noexcept {
	return Truncate( static_cast<double>( current ) + delta, current );
}

/*
CommandButtons

CL_CmdButtons at 0x00428750. `pressed` holds bit i for in_buttons[i] held or
pressed this frame, i below seven. Bit 3 is then ORed in when in_speed's
active flag equals `cl_run->integer == 0` (0x00428783..0x0042879d), which is
speed XOR run; bit 7 when anykeydown is non-zero and keyCatchers is zero
(0x004287a1..0x004287b3). Quake III's talk and walking bits have no Alice
equivalent and are never produced.
*/
constexpr std::uint8_t CommandButtons( std::uint8_t pressed, bool speedActive,
	bool run, bool anyKeyDown, bool gameplayCaptured ) noexcept {
	std::uint8_t buttons = static_cast<std::uint8_t>( pressed & 0x7f );
	if ( speedActive == !run ) {
		buttons = static_cast<std::uint8_t>( buttons | Run );
	}
	if ( anyKeyDown && !gameplayCaptured ) {
		buttons = static_cast<std::uint8_t>( buttons | AnyKey );
	}
	return buttons;
}

/* The three move bytes, in the command's order: forward at +14, right at +15
 * and up at +16 (the stores at 0x00428287, 0x00428290 and 0x0042829b). */
struct Move {
	int forward = 0;
	int right = 0;
	int up = 0;
};

/* The key fractions CL_KeyMove reads, each CL_KeyState's result in [0, 1].
 * +right and +left are read here only while +strafe is held; otherwise they
 * turn the view in CL_AdjustAngles and must not be read twice. */
struct KeyFractions {
	bool strafe = false;
	float right = 0.0f;
	float left = 0.0f;
	float moveRight = 0.0f;
	float moveLeft = 0.0f;
	float up = 0.0f;
	float down = 0.0f;
	float forward = 0.0f;
	float back = 0.0f;
};

/*
KeyMove

CL_KeyMove at 0x00428170, term for term: side from +right and -left under
+strafe, then +moveright and -moveleft; up from +moveup and -movedown;
forward from +forward and -back. Each term is 127 times its fraction and is
truncated as it is added; each result is clamped once, at the store.
*/
inline Move KeyMove( const KeyFractions &held ) noexcept {
	int side = 0;
	if ( held.strafe ) {
		side = Step( side, kMoveSpeed * held.right );
		side = Step( side, -kMoveSpeed * held.left );
	}
	side = Step( side, kMoveSpeed * held.moveRight );
	side = Step( side, -kMoveSpeed * held.moveLeft );

	int up = Step( 0, kMoveSpeed * held.up );
	up = Step( up, -kMoveSpeed * held.down );

	int forward = Step( 0, kMoveSpeed * held.forward );
	forward = Step( forward, -kMoveSpeed * held.back );

	Move move;
	move.forward = ClampChar( forward );
	move.right = ClampChar( side );
	move.up = ClampChar( up );
	return move;
}

/*
SampleCount

The first step of CL_MouseMove. With m_filter set the two slots are summed as
integers and halved (0x004284d4..0x0042850a, the 0.5 at 0x004ff9e8);
otherwise the current slot is taken. The sum saturates here where retail's
would wrap, which only an event flood could reach.
*/
inline float SampleCount( const int counts[2], int index, bool filter ) noexcept {
	if ( filter ) {
		const long long sum = static_cast<long long>( counts[0] ) + counts[1];
		const long long bounded = sum > ( std::numeric_limits<int>::max )()
			? ( std::numeric_limits<int>::max )()
			: sum < ( std::numeric_limits<int>::min )()
				? ( std::numeric_limits<int>::min )() : sum;
		return static_cast<float>( static_cast<double>( bounded ) * 0.5 );
	}
	return static_cast<float>( counts[index & 1] );
}

/* What CL_MouseMove reads besides the counts: the frame length, the
 * sensitivity and acceleration cvars, the five m_ scales, +cameralook,
 * +strafe, +mlook and freelook. The defaults are retail's registrations. */
struct MouseParams {
	float mx = 0.0f;
	float my = 0.0f;
	unsigned frameMsec = 1;
	float sensitivity = 5.0f;
	float acceleration = 0.0f;
	float pitch = 0.022f;
	float yaw = 0.022f;
	float forward = 0.25f;
	float side = 0.25f;
	float up = 0.0f;
	bool cameraLook = false;
	bool strafe = false;
	bool mlook = false;
	bool freelook = true;
};

struct MouseResult {
	/* For cl_showmouserate, which prints both when the rate is not zero. */
	float rate = 0.0f;
	float accel = 0.0f;
	/* False when both scaled counts were zero: nothing below changed. */
	bool moved = false;
	/* Added to the camera accumulators at 0x00532a88 and 0x00532a8c. */
	float cameraPitch = 0.0f;
	float cameraYaw = 0.0f;
	/* Added to cl.viewangles. */
	float pitch = 0.0f;
	float yaw = 0.0f;
	Move move;
};

/*
MouseMove

CL_MouseMove at 0x004284c0 from its third step on:

  rate  = sqrt(mx^2 + my^2) / frame_msec
  accel = rate * cl_mouseAccel + sensitivity; mx and my are scaled by it.
          There is no client-game sensitivity factor: Alice's client game has
          no SetUserCmdValue import.
  Both zero: return.
  Always: camera[1] -= mx * m_yaw and camera[0] += my * m_pitch
          (0x0042860c..0x0042863f). The client game reads these through its
          import 47 and turns its third-person camera by them.
  +cameralook held: return, so the view does not turn (0x00428645).
  +strafe: right += mx * m_side, forward -= my * m_forward and
          up -= my * m_up, each truncated and clamped; else yaw -= mx * m_yaw.
  (+mlook or freelook) and not +strafe: pitch += my * m_pitch; otherwise
          forward -= my * m_forward (0x00428720).

With +strafe held the last line applies mouse Y to forward a second time.
That is retail's, and it is kept.
*/
inline MouseResult MouseMove( const MouseParams &p, Move current ) noexcept {
	MouseResult result;
	result.move = current;

	const double mx = p.mx;
	const double my = p.my;
	const unsigned frame = p.frameMsec ? p.frameMsec : 1u;
	result.rate = static_cast<float>( std::sqrt( mx * mx + my * my ) / frame );
	result.accel = static_cast<float>(
		static_cast<double>( result.rate ) * p.acceleration + p.sensitivity );

	const float sx = static_cast<float>( result.accel * mx );
	const float sy = static_cast<float>( result.accel * my );
	if ( sx == 0.0f && sy == 0.0f ) {
		return result;
	}
	result.moved = true;

	result.cameraYaw = -sx * p.yaw;
	result.cameraPitch = sy * p.pitch;
	if ( p.cameraLook ) {
		return result;
	}

	if ( p.strafe ) {
		result.move.right = ClampChar(
			Step( result.move.right, static_cast<double>( sx ) * p.side ) );
		result.move.forward = ClampChar(
			Step( result.move.forward, -static_cast<double>( sy ) * p.forward ) );
		result.move.up = ClampChar(
			Step( result.move.up, -static_cast<double>( sy ) * p.up ) );
	} else {
		result.yaw = -sx * p.yaw;
	}

	if ( ( p.mlook || p.freelook ) && !p.strafe ) {
		result.pitch = sy * p.pitch;
	} else {
		result.move.forward = ClampChar(
			Step( result.move.forward, -static_cast<double>( sy ) * p.forward ) );
	}
	return result;
}

/*
CursorStep

CL_MouseEvent in cursor mode (0x00428343): the position moves by the event's
delta and is then held to [0, bound], the bound being the video width or
height. The addition saturates here where retail's would wrap.
*/
constexpr int CursorStep( int position, int delta, int bound ) noexcept {
	const long long sum = static_cast<long long>( position ) + delta;
	int moved = sum > ( std::numeric_limits<int>::max )()
		? ( std::numeric_limits<int>::max )()
		: sum < ( std::numeric_limits<int>::min )()
			? ( std::numeric_limits<int>::min )() : static_cast<int>( sum );
	if ( moved < 0 ) {
		moved = 0;
	}
	if ( moved > bound ) {
		moved = bound;
	}
	return moved;
}

/* The bit CL_KeyEvent sets in the cursor-mode button mask for MOUSE1 to
 * MOUSE5: bit (key - MOUSE1), set on a press and cleared on a release
 * (0x00429cf6..0x00429d2b). `index` is the button's distance from MOUSE1. */
constexpr int MouseButtonMask( int mask, int index, bool down ) noexcept {
	if ( index < 0 || index > 4 ) {
		return mask;
	}
	return down ? ( mask | ( 1 << index ) ) : ( mask & ~( 1 << index ) );
}

/* Which binding a key press runs (0x00429fbe..0x0042a028). A key pressed
 * while the alt modifier was on runs its alt binding if one was ever set, even
 * an empty one; failing that the ctrl binding on the same terms; failing that
 * its own. The modifier flag is per key, recorded when it went down. */
enum class BindingTable { Normal, Alt, Ctrl };

constexpr BindingTable PressBinding( bool altPressed, bool hasAltBinding,
	bool ctrlPressed, bool hasCtrlBinding ) noexcept {
	if ( hasAltBinding && altPressed ) {
		return BindingTable::Alt;
	}
	if ( hasCtrlBinding && ctrlPressed ) {
		return BindingTable::Ctrl;
	}
	return BindingTable::Normal;
}

} // namespace fntech3::alice::input

#endif
