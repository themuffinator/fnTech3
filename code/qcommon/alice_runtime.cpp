/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* Engine state American McGee's Alice's modules and renderer share: the one
 * shared block, the renderer's noise, and the debug line variables the game
 * module fills. See alice_runtime.h. */

#include "alice_runtime.h"
#include "qcommon.h"
#include "../compat/alice_module_abi.h"
#include "../compat/alice_noise.hpp"
#include "../compat/alice_render_abi.h"

#include <cstring>

namespace {

/* ---- the shared block ---------------------------------------------------- */

alignas( 8 ) byte sharedBlock[FNTECH3_ALICE_SHARED_BLOCK_BYTES];

/* ---- debug lines --------------------------------------------------------- */

void *debugLines;
int numDebugLines;

} // namespace

void *Alice_SharedBlock( void ) {
	return sharedBlock;
}

void Alice_SharedBlockClear( void ) {
	std::memset( sharedBlock, 0, sizeof( sharedBlock ) );
}

/* Built on first use, from retail's fixed seed; see compat/alice_noise.hpp. */
float Alice_Noise( float x, float y, float z, float t ) {
	static const fntech3::alice::noise::Table table = fntech3::alice::noise::Build();
	return fntech3::alice::noise::Get4f( table, x, y, z, t );
}

void **Alice_DebugLineArray( void ) {
	return &debugLines;
}

int *Alice_DebugLineCount( void ) {
	return &numDebugLines;
}

/* 0x004766f0: nothing when the module has published no array, a warning past
 * the length the game allocated for, and otherwise one line of width one with
 * a solid stipple. The game clears the count each frame. */
qboolean Alice_AddDebugLine( const vec3_t start, const vec3_t end, float r, float g, float b, float alpha ) {
	if ( !debugLines || !start || !end ) {
		return qfalse;
	}
	if ( numDebugLines < 0 || numDebugLines >= Cvar_VariableIntegerValue( "g_numdebuglines" ) ) {
		Com_Printf( "R_DebugLine: Exceeded MAX_DEBUG_LINES\n" );
		return qfalse;
	}
	fntech3_alice_debugline_t &line = static_cast<fntech3_alice_debugline_t *>( debugLines )[numDebugLines++];
	VectorCopy( start, line.start );
	VectorCopy( end, line.end );
	line.color[0] = r;
	line.color[1] = g;
	line.color[2] = b;
	line.alpha = alpha;
	line.width = 1.0f;
	line.factor = 1;
	line.pattern = 0xffff;
	return qtrue;
}
