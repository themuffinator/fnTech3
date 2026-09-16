extern "C" {
#include "q_shared.h"
}

#include <cstddef>

// Engine storage may grow while every external game/module POD remains fixed.
// Existing Q3/QL field offsets are retained; RTCW data has its own extension.
static_assert( sizeof( usercmd_t ) == 36 );
static_assert( offsetof( usercmd_t, rtcw ) == 28 );
static_assert( offsetof( usercmd_t, weapon ) == 20 );
static_assert( offsetof( usercmd_t, weaponPrimary ) == 21 );
static_assert( offsetof( usercmd_t, fov ) == 22 );
static_assert( offsetof( usercmd_t, forwardmove ) == 23 );
static_assert( offsetof( usercmd_t, rightmove ) == 24 );
static_assert( offsetof( usercmd_t, upmove ) == 25 );

static_assert( sizeof( trajectory_t ) == 40 );
static_assert( offsetof( trajectory_t, gravity ) == 36 );

static_assert( sizeof( entityState_t ) == 320 );
static_assert( offsetof( entityState_t, rtcw ) == 236 );
static_assert( offsetof( entityState_t, health ) == 200 );
static_assert( offsetof( entityState_t, armor ) == 204 );
static_assert( offsetof( entityState_t, location ) == 212 );
static_assert( offsetof( entityState_t, generic1 ) == 224 );
static_assert( offsetof( entityState_t, doubleJumped ) == 232 );

// The reconstructed QL prefix remains at its original offsets. Its retail
// module reservation is independent of these engine-owned RTCW extensions.
static_assert( sizeof( playerState_t ) == 1536 );
static_assert( offsetof( playerState_t, rtcw ) == 0x1f8 );
static_assert( offsetof( playerState_t, weaponPrimary ) == 148 );
static_assert( offsetof( playerState_t, fov ) == 156 );
static_assert( offsetof( playerState_t, stats ) == 192 );
static_assert( offsetof( playerState_t, forwardmove ) == 476 );
static_assert( offsetof( playerState_t, ping ) == 480 );
static_assert( offsetof( playerState_t, armorTier ) == 500 );

int main() {
	return 0;
}
