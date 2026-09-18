/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

// Frozen Elite Force records. Sizes and offsets are the retail module views:
// Holomatch from stvoyHM.exe 1.20 copy sizes and displacements, single player
// from stvoy.exe and the x86 SP game DLL. Holomatch records are 32-bit QVM
// data on every host; single player records hold x86 pointers.

#include "../code/compat/stvef_game_abi.h"
#include "../code/compat/stvef_module_abi.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#define ABI_POD( type ) \
	static_assert( std::is_standard_layout<type>::value && std::is_trivial<type>::value, #type )
#define ABI_SIZE( type, expected ) static_assert( sizeof( type ) == ( expected ), #type " size" )
#define ABI_AT( type, member, expected ) \
	static_assert( offsetof( type, member ) == ( expected ), #type "." #member )

// Shared records.
ABI_SIZE( fntech3_stvef_trajectory_t, 36 );
ABI_AT( fntech3_stvef_trajectory_t, trBase, 12 );
ABI_AT( fntech3_stvef_trajectory_t, trDelta, 24 );
ABI_SIZE( fntech3_stvef_cplane_t, 20 );
ABI_SIZE( fntech3_stvef_trace_t, 56 );
ABI_AT( fntech3_stvef_trace_t, plane, 24 );
ABI_AT( fntech3_stvef_trace_t, surfaceFlags, 44 );
ABI_AT( fntech3_stvef_trace_t, entityNum, 52 );
ABI_SIZE( fntech3_stvef_vmCvar_t, 272 );
ABI_AT( fntech3_stvef_vmCvar_t, string, 16 );
ABI_SIZE( fntech3_stvef_gameState_t, 20100 );
ABI_AT( fntech3_stvef_gameState_t, stringData, 4096 );
ABI_AT( fntech3_stvef_gameState_t, dataCount, 20096 );
ABI_SIZE( fntech3_stvef_polyVert_t, 24 );
ABI_SIZE( fntech3_stvef_orientation_t, 48 );
ABI_SIZE( fntech3_stvef_markFragment_t, 8 );
ABI_SIZE( fntech3_stvef_refdef_t, 112 );
ABI_AT( fntech3_stvef_refdef_t, vieworg, 24 );
ABI_AT( fntech3_stvef_refdef_t, time, 72 );
ABI_AT( fntech3_stvef_refdef_t, rdflags, 76 );
ABI_AT( fntech3_stvef_refdef_t, areamask, 80 );

// Holomatch.
ABI_POD( fntech3_stvef_mp_entityState_t );
ABI_SIZE( fntech3_stvef_mp_entityState_t, 204 );
ABI_AT( fntech3_stvef_mp_entityState_t, pos, 12 );
ABI_AT( fntech3_stvef_mp_entityState_t, apos, 48 );
ABI_AT( fntech3_stvef_mp_entityState_t, origin, 92 );
ABI_AT( fntech3_stvef_mp_entityState_t, groundEntityNum, 148 );
ABI_AT( fntech3_stvef_mp_entityState_t, constantLight, 152 );
ABI_AT( fntech3_stvef_mp_entityState_t, loopSound, 156 );
ABI_AT( fntech3_stvef_mp_entityState_t, frame, 172 );
ABI_AT( fntech3_stvef_mp_entityState_t, solid, 176 );
ABI_AT( fntech3_stvef_mp_entityState_t, torsoAnim, 200 );
ABI_SIZE( fntech3_stvef_mp_entityShared_t, 100 );
ABI_AT( fntech3_stvef_mp_entityShared_t, svFlags, 8 );
ABI_AT( fntech3_stvef_mp_entityShared_t, bmodel, 16 );
ABI_AT( fntech3_stvef_mp_entityShared_t, contents, 44 );
ABI_AT( fntech3_stvef_mp_entityShared_t, currentOrigin, 72 );
ABI_AT( fntech3_stvef_mp_entityShared_t, ownerNum, 96 );
ABI_SIZE( fntech3_stvef_mp_sharedEntity_t, 304 );
ABI_AT( fntech3_stvef_mp_sharedEntity_t, r, 204 );
ABI_POD( fntech3_stvef_mp_playerState_t );
ABI_SIZE( fntech3_stvef_mp_playerState_t, 468 );
ABI_AT( fntech3_stvef_mp_playerState_t, rechargeTime, 48 );
ABI_AT( fntech3_stvef_mp_playerState_t, useTime, 52 );
ABI_AT( fntech3_stvef_mp_playerState_t, introTime, 56 );
ABI_AT( fntech3_stvef_mp_playerState_t, gravity, 60 );
ABI_AT( fntech3_stvef_mp_playerState_t, delta_angles, 68 );
ABI_AT( fntech3_stvef_mp_playerState_t, eventSequence, 108 );
ABI_AT( fntech3_stvef_mp_playerState_t, events, 112 );
ABI_AT( fntech3_stvef_mp_playerState_t, eventParms, 128 );
ABI_AT( fntech3_stvef_mp_playerState_t, externalEventTime, 152 );
ABI_AT( fntech3_stvef_mp_playerState_t, clientNum, 156 );
ABI_AT( fntech3_stvef_mp_playerState_t, viewangles, 168 );
ABI_AT( fntech3_stvef_mp_playerState_t, damageShieldCount, 200 );
ABI_AT( fntech3_stvef_mp_playerState_t, stats, 204 );
ABI_AT( fntech3_stvef_mp_playerState_t, persistant, 268 );
ABI_AT( fntech3_stvef_mp_playerState_t, powerups, 332 );
ABI_AT( fntech3_stvef_mp_playerState_t, ammo, 396 );
ABI_AT( fntech3_stvef_mp_playerState_t, ping, 460 );
ABI_AT( fntech3_stvef_mp_playerState_t, entityEventSequence, 464 );
ABI_POD( fntech3_stvef_mp_usercmd_t );
ABI_SIZE( fntech3_stvef_mp_usercmd_t, 24 );
ABI_AT( fntech3_stvef_mp_usercmd_t, buttons, 4 );
ABI_AT( fntech3_stvef_mp_usercmd_t, weapon, 5 );
ABI_AT( fntech3_stvef_mp_usercmd_t, angles, 8 );
ABI_AT( fntech3_stvef_mp_usercmd_t, forwardmove, 20 );
ABI_AT( fntech3_stvef_mp_usercmd_t, upmove, 22 );
ABI_SIZE( fntech3_stvef_mp_snapshot_t, 52748 );
ABI_AT( fntech3_stvef_mp_snapshot_t, ps, 44 );
ABI_AT( fntech3_stvef_mp_snapshot_t, numEntities, 512 );
ABI_AT( fntech3_stvef_mp_snapshot_t, entities, 516 );
ABI_AT( fntech3_stvef_mp_snapshot_t, numServerCommands, 52740 );
ABI_AT( fntech3_stvef_mp_snapshot_t, serverCommandSequence, 52744 );
ABI_SIZE( fntech3_stvef_mp_refEntityData_t, 28 );
ABI_SIZE( fntech3_stvef_mp_refEntity_t, 160 );
ABI_AT( fntech3_stvef_mp_refEntity_t, axis, 28 );
ABI_AT( fntech3_stvef_mp_refEntity_t, origin, 68 );
ABI_AT( fntech3_stvef_mp_refEntity_t, oldorigin, 84 );
ABI_AT( fntech3_stvef_mp_refEntity_t, customShader, 112 );
ABI_AT( fntech3_stvef_mp_refEntity_t, shaderRGBA, 116 );
ABI_AT( fntech3_stvef_mp_refEntity_t, shaderTime, 128 );
ABI_AT( fntech3_stvef_mp_refEntity_t, data, 132 );
ABI_AT( fntech3_stvef_mp_refEntity_t, data.sprite.vertRGBA, 140 );
ABI_AT( fntech3_stvef_mp_refEntity_t, data.bezier.control2, 148 );
ABI_AT( fntech3_stvef_mp_refEntity_t, data.cylinder.wrap, 152 );
ABI_AT( fntech3_stvef_mp_refEntity_t, data.electricity.taper, 148 );
ABI_SIZE( fntech3_stvef_mp_glconfig_t, 5192 );
ABI_AT( fntech3_stvef_mp_glconfig_t, maxTextureSize, 5120 );
ABI_AT( fntech3_stvef_mp_glconfig_t, textureFilterAnisotropicAvailable, 5160 );
ABI_AT( fntech3_stvef_mp_glconfig_t, vidWidth, 5164 );
ABI_AT( fntech3_stvef_mp_glconfig_t, smpActive, 5188 );

// Single player.
ABI_SIZE( fntech3_stvef_sp_entityState_t, 232 );
ABI_AT( fntech3_stvef_sp_entityState_t, modelindex3, 0xa8 );
ABI_AT( fntech3_stvef_sp_entityState_t, clientNum, 0xac );
ABI_AT( fntech3_stvef_sp_entityState_t, solid, 0xb4 );
ABI_AT( fntech3_stvef_sp_entityState_t, legsAnimTimer, 0xcc );
ABI_AT( fntech3_stvef_sp_entityState_t, torsoAnim, 0xd0 );
ABI_AT( fntech3_stvef_sp_entityState_t, scale, 0xd8 );
ABI_AT( fntech3_stvef_sp_entityState_t, pushVec, 0xdc );
ABI_SIZE( fntech3_stvef_sp_playerState_t, 544 );
ABI_AT( fntech3_stvef_sp_playerState_t, useTime, 0x34 );
ABI_AT( fntech3_stvef_sp_playerState_t, gravity, 0x38 );
ABI_AT( fntech3_stvef_sp_playerState_t, leanofs, 0x3c );
ABI_AT( fntech3_stvef_sp_playerState_t, friction, 0x3e );
ABI_AT( fntech3_stvef_sp_playerState_t, speed, 0x40 );
ABI_AT( fntech3_stvef_sp_playerState_t, legsAnimTimer, 0x58 );
ABI_AT( fntech3_stvef_sp_playerState_t, scale, 0x64 );
ABI_AT( fntech3_stvef_sp_playerState_t, events, 0x74 );
ABI_AT( fntech3_stvef_sp_playerState_t, weapon, 0x94 );
ABI_AT( fntech3_stvef_sp_playerState_t, viewangles, 0x9c );
ABI_AT( fntech3_stvef_sp_playerState_t, stats, 0xbc );
ABI_AT( fntech3_stvef_sp_playerState_t, ammo, 0x17c );
ABI_AT( fntech3_stvef_sp_playerState_t, borgAdaptHits, 0x18c );
ABI_AT( fntech3_stvef_sp_playerState_t, pushVec, 0x20c );
ABI_AT( fntech3_stvef_sp_playerState_t, ping, 0x218 );
ABI_AT( fntech3_stvef_sp_playerState_t, leanStopDebounceTime, 0x21c );
ABI_SIZE( fntech3_stvef_sp_usercmd_t, 28 );
ABI_AT( fntech3_stvef_sp_usercmd_t, weapon, 8 );
ABI_AT( fntech3_stvef_sp_usercmd_t, angles, 0xc );
ABI_AT( fntech3_stvef_sp_usercmd_t, forwardmove, 0x18 );
ABI_AT( fntech3_stvef_sp_usercmd_t, upmove, 0x1a );
ABI_SIZE( fntech3_stvef_sp_refEntity_t, 152 );
ABI_AT( fntech3_stvef_sp_refEntity_t, shaderTime, 0x80 );
ABI_AT( fntech3_stvef_sp_refEntity_t, lightDir, 0x84 );
ABI_AT( fntech3_stvef_sp_refEntity_t, radius, 0x90 );
ABI_AT( fntech3_stvef_sp_refEntity_t, rotation, 0x94 );
ABI_SIZE( fntech3_stvef_sp_glconfig_t, 5196 );
ABI_AT( fntech3_stvef_sp_glconfig_t, maxActiveTextures, 0x1404 );
ABI_AT( fntech3_stvef_sp_glconfig_t, clampToEdgeAvailable, 0x142c );
ABI_AT( fntech3_stvef_sp_glconfig_t, vidWidth, 0x1430 );
ABI_AT( fntech3_stvef_sp_glconfig_t, smpActive, 0x1448 );

// The SP game is an x86 DLL; its pointer-bearing records exist only there.
#if defined( _M_IX86 ) || defined( __i386__ )
ABI_SIZE( fntech3_stvef_sp_cvar_t, 40 );
ABI_AT( fntech3_stvef_sp_cvar_t, flags, 0x10 );
ABI_AT( fntech3_stvef_sp_cvar_t, value, 0x1c );
ABI_AT( fntech3_stvef_sp_cvar_t, integer, 0x20 );
ABI_AT( fntech3_stvef_sp_cvar_t, next, 0x24 );
ABI_AT( fntech3_stvef_sp_gentity_t, client, 0xe8 );
ABI_AT( fntech3_stvef_sp_gentity_t, svFlags, 0xf4 );
ABI_AT( fntech3_stvef_sp_gentity_t, contents, 0x114 );
ABI_AT( fntech3_stvef_sp_gentity_t, currentOrigin, 0x130 );
ABI_AT( fntech3_stvef_sp_gentity_t, owner, 0x148 );
ABI_SIZE( fntech3_stvef_sp_gentity_t, 0x14c );
#endif
ABI_SIZE( fntech3_stvef_sp_snapshot_t, 60004 );
ABI_AT( fntech3_stvef_sp_snapshot_t, cmdNum, 0x2c );
ABI_AT( fntech3_stvef_sp_snapshot_t, ps, 0x30 );
ABI_AT( fntech3_stvef_sp_snapshot_t, numEntities, 0x250 );
ABI_AT( fntech3_stvef_sp_snapshot_t, entities, 0x254 );
ABI_AT( fntech3_stvef_sp_snapshot_t, serverCommandSequence, 0xea60 );

// Module numbering spot checks against the retail dispatchers.
static_assert( FNTECH3_STVEF_MP_G_PRINT == 0 );
static_assert( FNTECH3_STVEF_MP_G_LOCATE_GAME_DATA == 15 );
static_assert( FNTECH3_STVEF_MP_CG_R_REGISTERSHADER3D == 59 );
static_assert( FNTECH3_STVEF_MP_UI_LAN_GETPINGQUEUECOUNT == 50 );

int main() {
	return 0;
}
