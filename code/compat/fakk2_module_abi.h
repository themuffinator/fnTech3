/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_FAKK2_MODULE_ABI_H
#define FNTECH3_COMPAT_FAKK2_MODULE_ABI_H

/* Frozen Heavy Metal: F.A.K.K.2 module-table shapes.
 *
 * F.A.K.K.2's two libraries are the parents of Alice's: fakk/gamex86.dll
 * exports GetGameAPI and fakk/cgamex86.dll exports GetCGameAPI, each handing
 * the engine a flat table of pointer-sized slots, and the engine's menus are
 * its own .urc resources. The retail 1.0 libraries (both linked 2000-08-22)
 * are ordinary unprotected PEs, so every number here was measured in them.
 * fakk2.exe is LaserLock-wrapped and its code is encrypted on disk, so the
 * engine side of each table could not be read; the modules' own copies and
 * call sites are the whole of the evidence.
 *
 * How the two titles' tables relate is the useful fact. Every one of the 105
 * game and 124 client-game slots is the reference member Alice's table keeps
 * at the same position -- the game's first hundred in place, its last five one
 * place lower, because Alice's engine left slot 100 null -- and the argument
 * count every called slot's sites push agrees with Alice's own module at the
 * mapped slot, with two exceptions noted below. So F.A.K.K.2's tables are
 * Alice's without Alice's hole and without Alice's appends, and the engine
 * serves both from one table kept in Alice's order.
 *
 * The two exceptions are the tag queries. Both games' Tag_Orientation return
 * a 48-byte orientation through a hidden first argument, but F.A.K.K.2's take
 * the entity's bone tags and bone quaternions as two arrays -- eight pushes at
 * every site of game slot 78 and client-game slot 115 -- where Alice's pass one
 * pointer to five 36-byte controllers, seven pushes. And F.A.K.K.2's client
 * game draws filled boxes through slot 73, R_DrawBox, which Alice's never calls.
 *
 * See docs/fntech3/2026-09-24-fakk2-contracts.md. */

#include "alice_module_abi.h"

enum {
	/* GetGameAPI at 0x10083650 copies `mov ecx, 0x69` dwords of the engine's
	 * table to 0x101969d8, writes 4 into export slot 0 and 0x2c4 into export
	 * slot 22, and fills the other slots exactly as Alice's GetGameAPI does. */
	FNTECH3_FAKK2_GAME_API_VERSION = 4,
	FNTECH3_FAKK2_GAME_IMPORT_SLOTS = 105,
	FNTECH3_FAKK2_GAME_EXPORT_SLOTS = 26,

	/* The client game's first export, 0x30022470, copies `mov ecx, 0x7c`
	 * dwords to 0x301a3a20; GetCGameAPI at 0x300224e0 writes six exports. */
	FNTECH3_FAKK2_CGAME_IMPORT_SLOTS = 124,
	FNTECH3_FAKK2_CGAME_EXPORT_SLOTS = 6,

	/* The entity and client record sizes. The entity size is export slot 22's
	 * immediate, and both are pushed as immediates at all three of the
	 * module's LocateGameData sites (0x1008291c..0x10082933, 0x1008345d..
	 * 0x1008346a, 0x1009c907..0x1009c91a). The client record is Alice's size. */
	FNTECH3_FAKK2_GENTITY_SIZE = 708,
	FNTECH3_FAKK2_GCLIENT_SIZE = 2112,

	/* The records, each cleared or copied as a whole in the client game:
	 * `rep movsd` of 122 dwords for an entity state and 262 for a player
	 * state, and 91 and 111 dwords for the render entity and the view it
	 * hands the renderer (code/compat/fakk2_game_abi.h). */
	FNTECH3_FAKK2_ENTITY_STATE_BYTES = 488,
	FNTECH3_FAKK2_PLAYER_STATE_BYTES = 1048,

	/* The engine's half of the entity record. It is Alice's, in the same
	 * order, starting where F.A.K.K.2's shorter entity state ends: Alice's
	 * engine widened the state by 24 bytes and moved nothing else. */
	FNTECH3_FAKK2_ENTITY_SHARED_OFFSET = 488,
	FNTECH3_FAKK2_R_CLIENT = 488,
	FNTECH3_FAKK2_R_INUSE = 492,
	FNTECH3_FAKK2_R_LINKED = 496,
	FNTECH3_FAKK2_R_LINKCOUNT = 500,
	FNTECH3_FAKK2_R_SVFLAGS = 504,
	FNTECH3_FAKK2_R_BMODEL = 508,
	FNTECH3_FAKK2_R_MINS = 512,
	FNTECH3_FAKK2_R_MAXS = 524,
	FNTECH3_FAKK2_R_CONTENTS = 536,
	FNTECH3_FAKK2_R_ABSMIN = 540,
	FNTECH3_FAKK2_R_ABSMAX = 552,
	FNTECH3_FAKK2_R_RADIUS = 564,
	FNTECH3_FAKK2_R_CENTROID = 568,
	FNTECH3_FAKK2_R_AREANUM = 580,
	FNTECH3_FAKK2_R_CURRENTORIGIN = 584,
	FNTECH3_FAKK2_R_CURRENTANGLES = 596,
	FNTECH3_FAKK2_R_OWNERNUM = 608,
	FNTECH3_FAKK2_R_SOLIDTYPE = 612,

	/* Inside the entity state, where the engine reads it too. */
	FNTECH3_FAKK2_S_NUMBER = 0,
	FNTECH3_FAKK2_S_ORIGIN = 96,
	FNTECH3_FAKK2_S_ORIGIN2 = 108,
	FNTECH3_FAKK2_S_LOOPSOUND = 148,
	FNTECH3_FAKK2_S_LOOPSOUNDMINDIST = 156,
	FNTECH3_FAKK2_S_PARENT = 160,
	FNTECH3_FAKK2_S_MODELINDEX = 184,
	FNTECH3_FAKK2_S_SOLID = 416,
	FNTECH3_FAKK2_S_RENDERFX = 428,
	FNTECH3_FAKK2_S_BINDPARENT = 432,
	FNTECH3_FAKK2_PS_PING = 1044,

	FNTECH3_FAKK2_MAX_CLIENTS = 128,

	/* The configstring layout the client game's dispatch at 0x30022057..
	 * 0x3002211e walks, and the game module's item and ammo reads (`add
	 * ebx, 0x380`, `push 0x3c0`): the SDK header's CS_ blocks. Alice's engine
	 * grew the model block to 512 and moved everything after it. */
	FNTECH3_FAKK2_MAX_CONFIGSTRINGS = 1200,
	FNTECH3_FAKK2_GAMESTATE_CHARS = 16000,
	FNTECH3_FAKK2_CS_MODELS = 32,
	FNTECH3_FAKK2_MAX_MODELS = 384,
	FNTECH3_FAKK2_CS_SOUNDS = 416,
	FNTECH3_FAKK2_MAX_SOUNDS = 256,
	FNTECH3_FAKK2_CS_IMAGES = 672,
	FNTECH3_FAKK2_MAX_IMAGES = 64,
	FNTECH3_FAKK2_CS_LIGHTSTYLES = 736,
	FNTECH3_FAKK2_MAX_LIGHTSTYLES = 32,
	FNTECH3_FAKK2_CS_PLAYERS = 768,
	FNTECH3_FAKK2_CS_ITEMS = 896,
	FNTECH3_FAKK2_MAX_ITEMS = 64,
	FNTECH3_FAKK2_CS_AMMO = 960,

	/* The slots whose records differ from Alice's: the tag queries, which
	 * take the bone arrays, and the command lists, which are F.A.K.K.2's
	 * smaller record (code/compat/fakk2_game_abi.h). */
	FNTECH3_FAKK2_IMPORT_InitCommands = 57,
	FNTECH3_FAKK2_IMPORT_Frame_Commands = 68,
	FNTECH3_FAKK2_IMPORT_Tag_Orientation = 78,
	FNTECH3_FAKK2_IMPORT_Tag_OrientationEx = 79,
	FNTECH3_FAKK2_CG_CM_MarkFragments = 29,
	FNTECH3_FAKK2_CG_R_DrawBox = 73,
	FNTECH3_FAKK2_CG_GetGameState = 74,
	FNTECH3_FAKK2_CG_GetSnapshot = 75,
	FNTECH3_FAKK2_CG_InitCommands = 91,
	FNTECH3_FAKK2_CG_Frame_Commands = 104,
	FNTECH3_FAKK2_CG_Tag_Orientation = 115,
	FNTECH3_FAKK2_CG_Tag_OrientationEx = 116,
	FNTECH3_FAKK2_CG_Tag_NamedOrientation = 117,

	/* Both games carry five bone controllers an entity. */
	FNTECH3_FAKK2_BONE_CONTROLLERS = 5,
	FNTECH3_FAKK2_MODEL_SURFACES = 32
};

#ifdef __cplusplus
namespace fntech3::fakk2::module {

/* The Alice-ordered slot a F.A.K.K.2 game slot carries. Slots 0 to 99 are
 * the same member at the same position; 100 to 104 are Alice's 101 to 105,
 * past the slot Alice's engine left null. */
constexpr int AliceGameSlot( int fakk2Slot ) {
	return fakk2Slot < 0 || fakk2Slot >= FNTECH3_FAKK2_GAME_IMPORT_SLOTS ? -1
		: fakk2Slot < 100 ? fakk2Slot : fakk2Slot + 1;
}

/* And the client game's, which is the same position throughout: Alice's only
 * change is the slot it appended at 124. */
constexpr int AliceCgameSlot( int fakk2Slot ) {
	return fakk2Slot < 0 || fakk2Slot >= FNTECH3_FAKK2_CGAME_IMPORT_SLOTS ? -1 : fakk2Slot;
}

static_assert( AliceGameSlot( 0 ) == FNTECH3_ALICE_IMPORT_Printf, "the tables start together" );
static_assert( AliceGameSlot( 99 ) == FNTECH3_ALICE_IMPORT_CalcCRC, "CalcCRC is 99 in both" );
static_assert( AliceGameSlot( 100 ) == FNTECH3_ALICE_IMPORT_DebugLines,
	"F.A.K.K.2 has no hole where Alice's engine leaves slot 100 null" );
static_assert( AliceGameSlot( 102 ) == FNTECH3_ALICE_IMPORT_LocateGameData,
	"LocateGameData is 102 here: five pushes at all three sites" );
static_assert( AliceGameSlot( 104 ) == FNTECH3_ALICE_IMPORT_SetSkyPortal, "the last slot" );
static_assert( AliceGameSlot( FNTECH3_FAKK2_GAME_IMPORT_SLOTS ) == -1, "nothing past 104" );
static_assert( AliceGameSlot( FNTECH3_FAKK2_IMPORT_Tag_Orientation )
	== FNTECH3_ALICE_IMPORT_Tag_Orientation, "the tag query is where Alice's is" );
static_assert( AliceCgameSlot( FNTECH3_FAKK2_CG_R_DrawBox ) == FNTECH3_ALICE_CG_R_DrawBox,
	"R_DrawBox is the same member" );
static_assert( AliceCgameSlot( FNTECH3_FAKK2_CG_Tag_Orientation ) == FNTECH3_ALICE_CG_Tag_Orientation,
	"so is the client game's tag query" );
static_assert( FNTECH3_FAKK2_CGAME_IMPORT_SLOTS + 1 == FNTECH3_ALICE_CGAME_IMPORT_SLOTS,
	"Alice appended one client-game import" );
static_assert( FNTECH3_FAKK2_GAME_IMPORT_SLOTS + 6 == FNTECH3_ALICE_GAME_IMPORT_SLOTS,
	"Alice's game table is this one, a null and five appends" );
static_assert( FNTECH3_FAKK2_ENTITY_SHARED_OFFSET + 24 == FNTECH3_ALICE_ENTITY_SHARED_OFFSET,
	"Alice's entity state is 24 bytes longer" );
static_assert( FNTECH3_FAKK2_R_INUSE + 24 == FNTECH3_ALICE_R_INUSE
	&& FNTECH3_FAKK2_R_LINKED + 24 == FNTECH3_ALICE_R_LINKED
	&& FNTECH3_FAKK2_R_SVFLAGS + 24 == FNTECH3_ALICE_R_SVFLAGS
	&& FNTECH3_FAKK2_R_BMODEL + 24 == FNTECH3_ALICE_R_BMODEL
	&& FNTECH3_FAKK2_R_MINS + 24 == FNTECH3_ALICE_R_MINS
	&& FNTECH3_FAKK2_R_MAXS + 24 == FNTECH3_ALICE_R_MAXS
	&& FNTECH3_FAKK2_R_CONTENTS + 24 == FNTECH3_ALICE_R_CONTENTS
	&& FNTECH3_FAKK2_R_ABSMIN + 24 == FNTECH3_ALICE_R_ABSMIN
	&& FNTECH3_FAKK2_R_ABSMAX + 24 == FNTECH3_ALICE_R_ABSMAX
	&& FNTECH3_FAKK2_R_CURRENTORIGIN + 24 == FNTECH3_ALICE_R_CURRENTORIGIN
	&& FNTECH3_FAKK2_R_CURRENTANGLES + 24 == FNTECH3_ALICE_R_CURRENTANGLES
	&& FNTECH3_FAKK2_R_SOLIDTYPE + 24 == FNTECH3_ALICE_R_SOLIDTYPE,
	"the engine's half is Alice's, 24 bytes earlier" );
static_assert( FNTECH3_FAKK2_GENTITY_SIZE + 24 == FNTECH3_ALICE_GENTITY_SIZE,
	"and so is the whole record" );

} // namespace fntech3::fakk2::module
#endif

#endif
