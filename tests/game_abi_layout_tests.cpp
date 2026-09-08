/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

#include "../code/compat/q3_game_abi.h"
#include "../code/compat/ql_game_abi.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#define ABI_ASSERT_POD( type ) \
	static_assert( std::is_standard_layout<type>::value, #type " must be standard-layout" ); \
	static_assert( std::is_trivial<type>::value, #type " must be trivial" )

#define ABI_ASSERT_SIZE( type, expected ) \
	static_assert( sizeof( type ) == ( expected ), #type " size changed" )

#define ABI_ASSERT_ALIGN( type, expected ) \
	static_assert( alignof( type ) == ( expected ), #type " alignment changed" )

#define ABI_ASSERT_OFFSET( type, member, expected ) \
	static_assert( offsetof( type, member ) == ( expected ), \
		#type "." #member " offset changed" )

static_assert( CHAR_BIT == 8, "game module ABIs require eight-bit bytes" );
static_assert( sizeof( std::int8_t ) == 1, "game module ABIs require int8_t" );
static_assert( sizeof( std::uint8_t ) == 1, "game module ABIs require uint8_t" );
static_assert( sizeof( std::int32_t ) == 4, "game module ABIs require int32_t" );
static_assert( sizeof( float ) == 4, "game module ABIs require 32-bit float" );
static_assert( std::numeric_limits<float>::is_iec559,
	"game module ABIs require IEC 559 floating point" );

static_assert( FNTECH3_Q3_MAX_CVAR_VALUE_STRING == 256 );
static_assert( FNTECH3_Q3_MAX_STATS == 16 );
static_assert( FNTECH3_Q3_MAX_PERSISTANT == 16 );
static_assert( FNTECH3_Q3_MAX_POWERUPS == 16 );
static_assert( FNTECH3_Q3_MAX_WEAPONS == 16 );
static_assert( FNTECH3_Q3_MAX_PS_EVENTS == 2 );
static_assert( FNTECH3_Q3_PS_PMOVEFRAMECOUNTBITS == 6 );
static_assert( FNTECH3_Q3_MAX_CLIENTS == 64 );
static_assert( FNTECH3_Q3_GENTITYNUM_BITS == 10 );
static_assert( FNTECH3_Q3_MAX_GENTITIES == 1024 );
static_assert( FNTECH3_Q3_ENTITYNUM_NONE == 1023 );
static_assert( FNTECH3_Q3_ENTITYNUM_WORLD == 1022 );
static_assert( FNTECH3_Q3_ENTITYNUM_MAX_NORMAL == 1022 );
static_assert( FNTECH3_Q3_SOLID_BMODEL == 0x00ffffff );
static_assert( FNTECH3_Q3_MOVE_RUN == 120 );
static_assert( FNTECH3_Q3_MAX_MAP_AREA_BYTES == 32 );
static_assert( FNTECH3_Q3_MAX_ENTITIES_IN_SNAPSHOT == 256 );

static_assert( FNTECH3_Q3_BUTTON_ATTACK == 1 );
static_assert( FNTECH3_Q3_BUTTON_TALK == 2 );
static_assert( FNTECH3_Q3_BUTTON_USE_HOLDABLE == 4 );
static_assert( FNTECH3_Q3_BUTTON_GESTURE == 8 );
static_assert( FNTECH3_Q3_BUTTON_WALKING == 16 );
static_assert( FNTECH3_Q3_BUTTON_AFFIRMATIVE == 32 );
static_assert( FNTECH3_Q3_BUTTON_NEGATIVE == 64 );
static_assert( FNTECH3_Q3_BUTTON_GETFLAG == 128 );
static_assert( FNTECH3_Q3_BUTTON_GUARDBASE == 256 );
static_assert( FNTECH3_Q3_BUTTON_PATROL == 512 );
static_assert( FNTECH3_Q3_BUTTON_FOLLOWME == 1024 );
static_assert( FNTECH3_Q3_BUTTON_ANY == 2048 );

static_assert( FNTECH3_Q3_TR_STATIONARY == 0 );
static_assert( FNTECH3_Q3_TR_INTERPOLATE == 1 );
static_assert( FNTECH3_Q3_TR_LINEAR == 2 );
static_assert( FNTECH3_Q3_TR_LINEAR_STOP == 3 );
static_assert( FNTECH3_Q3_TR_SINE == 4 );
static_assert( FNTECH3_Q3_TR_GRAVITY == 5 );

static_assert( FNTECH3_Q3_CVAR_ARCHIVE == UINT32_C( 0x00000001 ) );
static_assert( FNTECH3_Q3_CVAR_USERINFO == UINT32_C( 0x00000002 ) );
static_assert( FNTECH3_Q3_CVAR_SERVERINFO == UINT32_C( 0x00000004 ) );
static_assert( FNTECH3_Q3_CVAR_SYSTEMINFO == UINT32_C( 0x00000008 ) );
static_assert( FNTECH3_Q3_CVAR_INIT == UINT32_C( 0x00000010 ) );
static_assert( FNTECH3_Q3_CVAR_LATCH == UINT32_C( 0x00000020 ) );
static_assert( FNTECH3_Q3_CVAR_ROM == UINT32_C( 0x00000040 ) );
static_assert( FNTECH3_Q3_CVAR_USER_CREATED == UINT32_C( 0x00000080 ) );
static_assert( FNTECH3_Q3_CVAR_TEMP == UINT32_C( 0x00000100 ) );
static_assert( FNTECH3_Q3_CVAR_CHEAT == UINT32_C( 0x00000200 ) );
static_assert( FNTECH3_Q3_CVAR_NORESTART == UINT32_C( 0x00000400 ) );
static_assert( FNTECH3_Q3_CVAR_SERVER_CREATED == UINT32_C( 0x00000800 ) );
static_assert( FNTECH3_Q3_CVAR_VM_CREATED == UINT32_C( 0x00001000 ) );
static_assert( FNTECH3_Q3_CVAR_PROTECTED == UINT32_C( 0x00002000 ) );
static_assert( FNTECH3_Q3_CVAR_NODEFAULT == UINT32_C( 0x00004000 ) );
static_assert( FNTECH3_Q3_CVAR_PRIVATE == UINT32_C( 0x00008000 ) );
static_assert( FNTECH3_Q3_CVAR_DEVELOPER == UINT32_C( 0x00010000 ) );
static_assert( FNTECH3_Q3_CVAR_NOTABCOMPLETE == UINT32_C( 0x00020000 ) );
static_assert( FNTECH3_Q3_CVAR_ARCHIVE_ND == UINT32_C( 0x00004001 ) );
static_assert( FNTECH3_Q3_CVAR_MODIFIED == UINT32_C( 0x40000000 ) );
static_assert( FNTECH3_Q3_CVAR_NONEXISTENT == UINT32_C( 0x80000000 ) );

ABI_ASSERT_POD( fntech3_q3_vmCvar_t );
ABI_ASSERT_SIZE( fntech3_q3_vmCvar_t, 272 );
ABI_ASSERT_ALIGN( fntech3_q3_vmCvar_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_vmCvar_t, handle, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_vmCvar_t, modificationCount, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_vmCvar_t, value, 8 );
ABI_ASSERT_OFFSET( fntech3_q3_vmCvar_t, integer, 12 );
ABI_ASSERT_OFFSET( fntech3_q3_vmCvar_t, string, 16 );

ABI_ASSERT_POD( fntech3_q3_trajectory_t );
ABI_ASSERT_SIZE( fntech3_q3_trajectory_t, 36 );
ABI_ASSERT_ALIGN( fntech3_q3_trajectory_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_trajectory_t, trType, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_trajectory_t, trTime, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_trajectory_t, trDuration, 8 );
ABI_ASSERT_OFFSET( fntech3_q3_trajectory_t, trBase, 12 );
ABI_ASSERT_OFFSET( fntech3_q3_trajectory_t, trDelta, 24 );

ABI_ASSERT_POD( fntech3_q3_gameState_t );
ABI_ASSERT_SIZE( fntech3_q3_gameState_t, 20100 );
ABI_ASSERT_ALIGN( fntech3_q3_gameState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_gameState_t, stringOffsets, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_gameState_t, stringData, 4096 );
ABI_ASSERT_OFFSET( fntech3_q3_gameState_t, dataCount, 20096 );

ABI_ASSERT_POD( fntech3_ql_gameState_t );
ABI_ASSERT_SIZE( fntech3_ql_gameState_t, 20100 );
ABI_ASSERT_ALIGN( fntech3_ql_gameState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_gameState_t, stringOffsets, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_gameState_t, stringData, 4096 );
ABI_ASSERT_OFFSET( fntech3_ql_gameState_t, dataCount, 20096 );

ABI_ASSERT_POD( fntech3_q3_usercmd_t );
ABI_ASSERT_SIZE( fntech3_q3_usercmd_t, 24 );
ABI_ASSERT_ALIGN( fntech3_q3_usercmd_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, serverTime, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, angles, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, buttons, 16 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, weapon, 20 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, forwardmove, 21 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, rightmove, 22 );
ABI_ASSERT_OFFSET( fntech3_q3_usercmd_t, upmove, 23 );

ABI_ASSERT_POD( fntech3_q3_entityState_t );
ABI_ASSERT_SIZE( fntech3_q3_entityState_t, 208 );
ABI_ASSERT_ALIGN( fntech3_q3_entityState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, number, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, eType, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, eFlags, 8 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, pos, 12 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, apos, 48 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, time, 84 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, time2, 88 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, origin, 92 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, origin2, 104 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, angles, 116 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, angles2, 128 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, otherEntityNum, 140 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, otherEntityNum2, 144 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, groundEntityNum, 148 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, constantLight, 152 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, loopSound, 156 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, modelindex, 160 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, modelindex2, 164 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, clientNum, 168 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, frame, 172 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, solid, 176 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, event, 180 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, eventParm, 184 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, powerups, 188 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, weapon, 192 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, legsAnim, 196 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, torsoAnim, 200 );
ABI_ASSERT_OFFSET( fntech3_q3_entityState_t, generic1, 204 );

ABI_ASSERT_POD( fntech3_q3_playerState_t );
ABI_ASSERT_SIZE( fntech3_q3_playerState_t, 468 );
ABI_ASSERT_ALIGN( fntech3_q3_playerState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, commandTime, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, pm_type, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, bobCycle, 8 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, pm_flags, 12 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, pm_time, 16 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, origin, 20 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, velocity, 32 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, weaponTime, 44 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, gravity, 48 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, speed, 52 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, delta_angles, 56 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, groundEntityNum, 68 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, legsTimer, 72 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, legsAnim, 76 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, torsoTimer, 80 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, torsoAnim, 84 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, movementDir, 88 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, grapplePoint, 92 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, eFlags, 104 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, eventSequence, 108 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, events, 112 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, eventParms, 120 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, externalEvent, 128 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, externalEventParm, 132 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, externalEventTime, 136 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, clientNum, 140 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, weapon, 144 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, weaponstate, 148 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, viewangles, 152 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, viewheight, 164 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, damageEvent, 168 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, damageYaw, 172 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, damagePitch, 176 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, damageCount, 180 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, stats, 184 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, persistant, 248 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, powerups, 312 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, ammo, 376 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, generic1, 440 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, loopSound, 444 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, jumppad_ent, 448 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, ping, 452 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, pmove_framecount, 456 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, jumppad_frame, 460 );
ABI_ASSERT_OFFSET( fntech3_q3_playerState_t, entityEventSequence, 464 );

ABI_ASSERT_POD( fntech3_q3_entityShared_t );
ABI_ASSERT_SIZE( fntech3_q3_entityShared_t, 308 );
ABI_ASSERT_ALIGN( fntech3_q3_entityShared_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, s, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, linked, 208 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, linkcount, 212 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, svFlags, 216 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, singleClient, 220 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, bmodel, 224 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, mins, 228 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, maxs, 240 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, contents, 252 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, absmin, 256 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, absmax, 268 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, currentOrigin, 280 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, currentAngles, 292 );
ABI_ASSERT_OFFSET( fntech3_q3_entityShared_t, ownerNum, 304 );

ABI_ASSERT_POD( fntech3_q3_sharedEntity_t );
ABI_ASSERT_SIZE( fntech3_q3_sharedEntity_t, 516 );
ABI_ASSERT_ALIGN( fntech3_q3_sharedEntity_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_sharedEntity_t, s, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_sharedEntity_t, r, 208 );
static_assert( offsetof( fntech3_q3_sharedEntity_t, r )
	+ offsetof( fntech3_q3_entityShared_t, s )
	+ offsetof( fntech3_q3_entityState_t, origin2 ) == 312,
	"fntech3_q3_sharedEntity_t.r.s.origin2 offset changed" );

ABI_ASSERT_POD( fntech3_q3_snapshot_t );
ABI_ASSERT_SIZE( fntech3_q3_snapshot_t, 0xd20c );
ABI_ASSERT_ALIGN( fntech3_q3_snapshot_t, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, snapFlags, 0 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, ping, 4 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, serverTime, 8 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, areamask, 12 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, ps, 44 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, numEntities, 512 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, entities, 516 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, numServerCommands, 53764 );
ABI_ASSERT_OFFSET( fntech3_q3_snapshot_t, serverCommandSequence, 53768 );

static_assert( FNTECH3_QL_MAX_CVAR_VALUE_STRING == 256 );
static_assert( FNTECH3_QL_MAX_STATS == 16 );
static_assert( FNTECH3_QL_MAX_PERSISTANT == 16 );
static_assert( FNTECH3_QL_MAX_POWERUPS == 16 );
static_assert( FNTECH3_QL_MAX_WEAPONS == 16 );
static_assert( FNTECH3_QL_MAX_PS_EVENTS == 2 );
static_assert( FNTECH3_QL_PS_PMOVEFRAMECOUNTBITS == 6 );
static_assert( FNTECH3_QL_MAX_CLIENTS == 64 );
static_assert( FNTECH3_QL_GENTITYNUM_BITS == 10 );
static_assert( FNTECH3_QL_MAX_GENTITIES == 1024 );
static_assert( FNTECH3_QL_ENTITYNUM_NONE == 1023 );
static_assert( FNTECH3_QL_ENTITYNUM_WORLD == 1022 );
static_assert( FNTECH3_QL_ENTITYNUM_MAX_NORMAL == 1022 );
static_assert( FNTECH3_QL_SOLID_BMODEL == 0x00ffffff );
static_assert( FNTECH3_QL_MOVE_RUN == 120 );
static_assert( FNTECH3_QL_PLAYERSTATE_KNOWN_BYTES == 0x1f8 );
static_assert( FNTECH3_QL_RETAIL_SNAPSHOT_PLAYERSTATE_BYTES == 0x250 );

static_assert( FNTECH3_QL_BUTTON_ATTACK == 1 );
static_assert( FNTECH3_QL_BUTTON_TALK == 2 );
static_assert( FNTECH3_QL_BUTTON_USE_HOLDABLE == 4 );
static_assert( FNTECH3_QL_BUTTON_GESTURE == 8 );
static_assert( FNTECH3_QL_BUTTON_WALKING == 16 );
static_assert( FNTECH3_QL_BUTTON_AFFIRMATIVE == 32 );
static_assert( FNTECH3_QL_BUTTON_NEGATIVE == 64 );
static_assert( FNTECH3_QL_BUTTON_GETFLAG == 128 );
static_assert( FNTECH3_QL_BUTTON_GUARDBASE == 256 );
static_assert( FNTECH3_QL_BUTTON_PATROL == 512 );
static_assert( FNTECH3_QL_BUTTON_FOLLOWME == 1024 );
static_assert( FNTECH3_QL_BUTTON_ANY == 2048 );

static_assert( FNTECH3_QL_TR_STATIONARY == 0 );
static_assert( FNTECH3_QL_TR_INTERPOLATE == 1 );
static_assert( FNTECH3_QL_TR_LINEAR == 2 );
static_assert( FNTECH3_QL_TR_LINEAR_STOP == 3 );
static_assert( FNTECH3_QL_TR_SINE == 4 );
static_assert( FNTECH3_QL_TR_GRAVITY == 5 );
static_assert( FNTECH3_QL_TR_ACCEL == 6 );

static_assert( FNTECH3_QL_CVAR_ARCHIVE == UINT32_C( 0x00000001 ) );
static_assert( FNTECH3_QL_CVAR_USERINFO == UINT32_C( 0x00000002 ) );
static_assert( FNTECH3_QL_CVAR_SERVERINFO == UINT32_C( 0x00000004 ) );
static_assert( FNTECH3_QL_CVAR_SYSTEMINFO == UINT32_C( 0x00000008 ) );
static_assert( FNTECH3_QL_CVAR_INIT == UINT32_C( 0x00000010 ) );
static_assert( FNTECH3_QL_CVAR_LATCH == UINT32_C( 0x00000020 ) );
static_assert( FNTECH3_QL_CVAR_ROM == UINT32_C( 0x00000040 ) );
static_assert( FNTECH3_QL_CVAR_USER_CREATED == UINT32_C( 0x00000080 ) );
static_assert( FNTECH3_QL_CVAR_TEMP == UINT32_C( 0x00000100 ) );
static_assert( FNTECH3_QL_CVAR_CHEAT == UINT32_C( 0x00000200 ) );
static_assert( FNTECH3_QL_CVAR_NORESTART == UINT32_C( 0x00000400 ) );
static_assert( FNTECH3_QL_CVAR_PROTECTED == UINT32_C( 0x00000800 ) );
static_assert( FNTECH3_QL_CVAR_VM_CREATED == UINT32_C( 0x00001000 ) );
static_assert( FNTECH3_QL_CVAR_BOUNDED_DISCRETE == UINT32_C( 0x00002000 ) );
static_assert( FNTECH3_QL_CVAR_PMOVE_SETTINGS == UINT32_C( 0x00004000 ) );
static_assert( FNTECH3_QL_CVAR_WEAPON_RELOAD == UINT32_C( 0x00008000 ) );
static_assert( FNTECH3_QL_CVAR_PLAYER_APPEARANCE == UINT32_C( 0x00010000 ) );
static_assert( FNTECH3_QL_CVAR_ARMOR_TIERED == UINT32_C( 0x00020000 ) );
static_assert( FNTECH3_QL_CVAR_CLOUD == UINT32_C( 0x00080000 ) );
static_assert( FNTECH3_QL_CVAR_GAMERULE == UINT32_C( 0x00100000 ) );
static_assert( FNTECH3_QL_CVAR_MODIFIED == UINT32_C( 0x40000000 ) );
static_assert( FNTECH3_QL_CVAR_NONEXISTENT == UINT32_C( 0x80000000 ) );

/* These equal numeric bits deliberately have different public meanings. */
static_assert( FNTECH3_Q3_CVAR_SERVER_CREATED == FNTECH3_QL_CVAR_PROTECTED );
static_assert( FNTECH3_Q3_CVAR_PROTECTED == FNTECH3_QL_CVAR_BOUNDED_DISCRETE );
static_assert( FNTECH3_Q3_CVAR_PRIVATE == FNTECH3_QL_CVAR_WEAPON_RELOAD );

ABI_ASSERT_POD( fntech3_ql_vmCvar_t );
ABI_ASSERT_SIZE( fntech3_ql_vmCvar_t, 276 );
ABI_ASSERT_ALIGN( fntech3_ql_vmCvar_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, handle, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, modificationCount, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, value, 8 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, integer, 12 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, flags, 16 );
ABI_ASSERT_OFFSET( fntech3_ql_vmCvar_t, string, 20 );

ABI_ASSERT_POD( fntech3_ql_trajectory_t );
ABI_ASSERT_SIZE( fntech3_ql_trajectory_t, 40 );
ABI_ASSERT_ALIGN( fntech3_ql_trajectory_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, trType, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, trTime, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, trDuration, 8 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, trBase, 12 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, trDelta, 24 );
ABI_ASSERT_OFFSET( fntech3_ql_trajectory_t, gravity, 36 );

ABI_ASSERT_POD( fntech3_ql_usercmd_t );
ABI_ASSERT_SIZE( fntech3_ql_usercmd_t, 28 );
ABI_ASSERT_ALIGN( fntech3_ql_usercmd_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, serverTime, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, angles, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, buttons, 16 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, weapon, 20 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, weaponPrimary, 21 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, fov, 22 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, forwardmove, 23 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, rightmove, 24 );
ABI_ASSERT_OFFSET( fntech3_ql_usercmd_t, upmove, 25 );

ABI_ASSERT_POD( fntech3_ql_entityState_t );
ABI_ASSERT_SIZE( fntech3_ql_entityState_t, 236 );
ABI_ASSERT_ALIGN( fntech3_ql_entityState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, number, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, eType, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, eFlags, 8 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, pos, 12 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, apos, 52 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, time, 92 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, time2, 96 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, origin, 100 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, origin2, 112 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, angles, 124 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, angles2, 136 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, otherEntityNum, 148 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, otherEntityNum2, 152 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, groundEntityNum, 156 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, constantLight, 160 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, loopSound, 164 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, modelindex, 168 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, modelindex2, 172 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, clientNum, 176 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, frame, 180 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, solid, 184 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, event, 188 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, eventParm, 192 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, powerups, 196 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, health, 200 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, armor, 204 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, weapon, 208 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, location, 212 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, legsAnim, 216 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, torsoAnim, 220 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, generic1, 224 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, jumpTime, 228 );
ABI_ASSERT_OFFSET( fntech3_ql_entityState_t, doubleJumped, 232 );

ABI_ASSERT_POD( fntech3_ql_playerState_t );
ABI_ASSERT_SIZE( fntech3_ql_playerState_t, 504 );
ABI_ASSERT_ALIGN( fntech3_ql_playerState_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, commandTime, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, pm_type, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, bobCycle, 8 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, pm_flags, 12 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, pm_time, 16 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, origin, 20 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, velocity, 32 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, weaponTime, 44 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, gravity, 48 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, speed, 52 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, delta_angles, 56 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, groundEntityNum, 68 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, legsTimer, 72 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, legsAnim, 76 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, torsoTimer, 80 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, torsoAnim, 84 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, movementDir, 88 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, grapplePoint, 92 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, eFlags, 104 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, eventSequence, 108 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, events, 112 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, eventParms, 120 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, externalEvent, 128 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, externalEventParm, 132 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, clientNum, 136 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, location, 140 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, weapon, 144 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, weaponPrimary, 148 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, weaponstate, 152 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, fov, 156 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, viewangles, 160 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, viewheight, 172 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, damageEvent, 176 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, damageYaw, 180 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, damagePitch, 184 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, damageCount, 188 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, stats, 192 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, persistant, 256 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, powerups, 320 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, ammo, 384 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, generic1, 448 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, loopSound, 452 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, jumppad_ent, 456 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, jumpTime, 460 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, doubleJumped, 464 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, crouchTime, 468 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, crouchSlideTime, 472 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, forwardmove, 476 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, rightmove, 477 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, upmove, 478 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, commandMirrorPad, 479 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, ping, 480 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, pmove_framecount, 484 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, jumppad_frame, 488 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, entityEventSequence, 492 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, externalEventTime, 496 );
ABI_ASSERT_OFFSET( fntech3_ql_playerState_t, armorTier, 500 );

ABI_ASSERT_POD( fntech3_ql_entityShared_t );
ABI_ASSERT_SIZE( fntech3_ql_entityShared_t, 336 );
ABI_ASSERT_ALIGN( fntech3_ql_entityShared_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, s, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, linked, 236 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, linkcount, 240 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, svFlags, 244 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, singleClient, 248 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, bmodel, 252 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, mins, 256 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, maxs, 268 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, contents, 280 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, absmin, 284 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, absmax, 296 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, currentOrigin, 308 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, currentAngles, 320 );
ABI_ASSERT_OFFSET( fntech3_ql_entityShared_t, ownerNum, 332 );

ABI_ASSERT_POD( fntech3_ql_sharedEntity_t );
ABI_ASSERT_SIZE( fntech3_ql_sharedEntity_t, 572 );
ABI_ASSERT_ALIGN( fntech3_ql_sharedEntity_t, 4 );
ABI_ASSERT_OFFSET( fntech3_ql_sharedEntity_t, s, 0 );
ABI_ASSERT_OFFSET( fntech3_ql_sharedEntity_t, r, 236 );
static_assert( offsetof( fntech3_ql_sharedEntity_t, r )
	+ offsetof( fntech3_ql_entityShared_t, s )
	+ offsetof( fntech3_ql_entityState_t, origin2 ) == 348,
	"fntech3_ql_sharedEntity_t.r.s.origin2 offset changed" );

#undef ABI_ASSERT_OFFSET
#undef ABI_ASSERT_ALIGN
#undef ABI_ASSERT_SIZE
#undef ABI_ASSERT_POD

int main() {
	return 0;
}
