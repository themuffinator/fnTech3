extern "C" {
#include "q_shared.h"
}

#include <cstddef>

// Engine storage may grow while every external game/module POD remains fixed.
// Existing Q3/QL/RTCW field offsets are retained; ET, Elite Force, Raven's
// two Jedi titles and Alice each have their own extension appended after
// them. A size here moving is expected when a profile is added; an offset
// moving is not, which is what the rest of this file guards.
// Alice's movement bytes now cross as forwardmove, rightmove and upmove, so its
// extension keeps only the byte at +6 of its command and the record is 44.
static_assert( sizeof( usercmd_t ) == 44 );
static_assert( offsetof( usercmd_t, alice ) == 42 );
static_assert( offsetof( usercmd_t, raven ) == 39 );
static_assert( offsetof( usercmd_t, wolfet ) == 36 );
static_assert( offsetof( usercmd_t, rtcw ) == 28 );
static_assert( offsetof( usercmd_t, weapon ) == 20 );
static_assert( offsetof( usercmd_t, weaponPrimary ) == 21 );
static_assert( offsetof( usercmd_t, fov ) == 22 );
static_assert( offsetof( usercmd_t, forwardmove ) == 23 );
static_assert( offsetof( usercmd_t, rightmove ) == 24 );
static_assert( offsetof( usercmd_t, upmove ) == 25 );

static_assert( sizeof( trajectory_t ) == 40 );
static_assert( offsetof( trajectory_t, gravity ) == 36 );

static_assert( sizeof( entityState_t ) == 936 );
static_assert( offsetof( entityState_t, alice ) == 732 );
static_assert( offsetof( entityState_t, raven ) == 352 );
static_assert( offsetof( entityState_t, wolfet ) == 320 );
static_assert( offsetof( entityState_t, stvef ) == 324 );
static_assert( offsetof( entityState_t, rtcw ) == 236 );
static_assert( offsetof( entityState_t, health ) == 200 );
static_assert( offsetof( entityState_t, armor ) == 204 );
static_assert( offsetof( entityState_t, location ) == 212 );
static_assert( offsetof( entityState_t, generic1 ) == 224 );
static_assert( offsetof( entityState_t, doubleJumped ) == 232 );

// The reconstructed QL prefix remains at its original offsets. Its retail
// module reservation is independent of these engine-owned RTCW extensions.
static_assert( sizeof( playerState_t ) == 5156 );
static_assert( offsetof( playerState_t, alice ) == 4280 );
static_assert( offsetof( playerState_t, raven ) == 1732 );
static_assert( offsetof( playerState_t, wolfet ) == 1536 );
static_assert( offsetof( playerState_t, stvef ) == 1544 );
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
