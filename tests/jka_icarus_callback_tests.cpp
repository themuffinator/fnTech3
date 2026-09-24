/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Hold the ICARUS callback records to the layout the retail module reads them
 * at. They travel in the buffer the module lends the engine, with nothing in
 * the call itself, so a member that moves is read as a different member.
 *
 * Nothing here calls a module. These are layout facts. */

#include "../code/compat/jka_icarus_callbacks.h"
#include "../code/compat/jka_module_abi.h"

#include <cstddef>
#include <cstdio>

#define FIELD( record, member, offset ) \
	static_assert( offsetof( record, member ) == ( offset ), \
		#record "::" #member " sits at a different offset" )
#define SIZE( record, bytes ) \
	static_assert( sizeof( record ) == ( bytes ), #record " is a different size" )

/* Most records name the task first and the entity second. */
SIZE( fntech3_jka_icarus_playsound_t, 4104 );
FIELD( fntech3_jka_icarus_playsound_t, taskID, 0 );
FIELD( fntech3_jka_icarus_playsound_t, entID, 4 );
FIELD( fntech3_jka_icarus_playsound_t, name, 8 );
FIELD( fntech3_jka_icarus_playsound_t, channel, 2056 );

SIZE( fntech3_jka_icarus_set_t, 4104 );
FIELD( fntech3_jka_icarus_set_t, taskID, 0 );
FIELD( fntech3_jka_icarus_set_t, entID, 4 );
FIELD( fntech3_jka_icarus_set_t, type_name, 8 );
FIELD( fntech3_jka_icarus_set_t, data, 2056 );

SIZE( fntech3_jka_icarus_lerp2pos_t, 40 );
FIELD( fntech3_jka_icarus_lerp2pos_t, taskID, 0 );
FIELD( fntech3_jka_icarus_lerp2pos_t, entID, 4 );
FIELD( fntech3_jka_icarus_lerp2pos_t, origin, 8 );
FIELD( fntech3_jka_icarus_lerp2pos_t, angles, 20 );
FIELD( fntech3_jka_icarus_lerp2pos_t, duration, 32 );
FIELD( fntech3_jka_icarus_lerp2pos_t, nullAngles, 36 );

SIZE( fntech3_jka_icarus_lerp2origin_t, 24 );
FIELD( fntech3_jka_icarus_lerp2origin_t, origin, 8 );
FIELD( fntech3_jka_icarus_lerp2origin_t, duration, 20 );

SIZE( fntech3_jka_icarus_lerp2angles_t, 24 );
FIELD( fntech3_jka_icarus_lerp2angles_t, angles, 8 );
FIELD( fntech3_jka_icarus_lerp2angles_t, duration, 20 );

SIZE( fntech3_jka_icarus_gettag_t, 2068 );
FIELD( fntech3_jka_icarus_gettag_t, entID, 0 );
FIELD( fntech3_jka_icarus_gettag_t, name, 4 );
FIELD( fntech3_jka_icarus_gettag_t, lookup, 2052 );
FIELD( fntech3_jka_icarus_gettag_t, info, 2056 );

/* These two put the entity first, which the rest do not. */
SIZE( fntech3_jka_icarus_lerp2start_t, 12 );
FIELD( fntech3_jka_icarus_lerp2start_t, entID, 0 );
FIELD( fntech3_jka_icarus_lerp2start_t, taskID, 4 );
FIELD( fntech3_jka_icarus_lerp2start_t, duration, 8 );

SIZE( fntech3_jka_icarus_lerp2end_t, 12 );
FIELD( fntech3_jka_icarus_lerp2end_t, entID, 0 );
FIELD( fntech3_jka_icarus_lerp2end_t, taskID, 4 );

SIZE( fntech3_jka_icarus_use_t, 2052 );
FIELD( fntech3_jka_icarus_use_t, entID, 0 );
FIELD( fntech3_jka_icarus_use_t, target, 4 );

SIZE( fntech3_jka_icarus_kill_t, 2052 );
FIELD( fntech3_jka_icarus_kill_t, name, 4 );

SIZE( fntech3_jka_icarus_remove_t, 2052 );
FIELD( fntech3_jka_icarus_remove_t, name, 4 );

SIZE( fntech3_jka_icarus_play_t, 4104 );
FIELD( fntech3_jka_icarus_play_t, type, 8 );
FIELD( fntech3_jka_icarus_play_t, name, 2056 );

SIZE( fntech3_jka_icarus_getfloat_t, 2060 );
FIELD( fntech3_jka_icarus_getfloat_t, entID, 0 );
FIELD( fntech3_jka_icarus_getfloat_t, type, 4 );
FIELD( fntech3_jka_icarus_getfloat_t, name, 8 );
FIELD( fntech3_jka_icarus_getfloat_t, value, 2056 );

SIZE( fntech3_jka_icarus_getvector_t, 2068 );
FIELD( fntech3_jka_icarus_getvector_t, value, 2056 );

SIZE( fntech3_jka_icarus_getstring_t, 4104 );
FIELD( fntech3_jka_icarus_getstring_t, value, 2056 );

SIZE( fntech3_jka_icarus_soundindex_t, 2048 );
SIZE( fntech3_jka_icarus_setidforstring_t, 2048 );

/* Nothing may be written into the lent buffer that does not fit this bound. */
static_assert( FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES == 4104,
	"the largest callback record sets the bound" );
static_assert( sizeof( fntech3_jka_icarus_playsound_t )
	<= FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES, "playsound fits the bound" );
static_assert( sizeof( fntech3_jka_icarus_getstring_t )
	<= FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES, "getstring fits the bound" );
static_assert( sizeof( fntech3_jka_icarus_play_t )
	<= FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES, "play fits the bound" );
static_assert( sizeof( fntech3_jka_icarus_set_t )
	<= FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES, "set fits the bound" );

/* The exports these records are written for are contiguous in the module's own
 * table, and the frozen ordinals still say so. */
namespace module = fntech3::jka::module;
using module::Call;
using module::Direction;
using module::Module;

static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_PLAYSOUND ) == 13 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_SET ) == 14 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_LERP2POS ) == 15 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_LERP2ORIGIN ) == 16 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_LERP2ANGLES ) == 17 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_GETTAG ) == 18 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_LERP2START ) == 19 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_LERP2END ) == 20 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_USE ) == 21 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_KILL ) == 22 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_REMOVE ) == 23 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_PLAY ) == 24 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_GETFLOAT ) == 25 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_GETVECTOR ) == 26 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_GETSTRING ) == 27 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_SOUNDINDEX ) == 28 );
static_assert( module::Ordinal( COM_GAME_JKA_MP, Call::GAME_ICARUS_GETSETIDFORSTRING ) == 29 );

int main()
{
	std::printf( "Jedi Academy ICARUS callback layouts: seventeen records, "
		"largest %d bytes\n", FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES );
	return 0;
}
