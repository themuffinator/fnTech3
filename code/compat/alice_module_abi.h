/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_MODULE_ABI_H
#define FNTECH3_COMPAT_ALICE_MODULE_ABI_H

/* Frozen American McGee's Alice module-table shapes.
 *
 * Alice's modules are not Quake III's. Neither library exports dllEntry or
 * vmMain: base/fgamex86.dll exports GetGameAPI and base/cgamex86.dll exports
 * GetCGameAPI, each handing the engine a flat table of pointer-sized slots.
 * There is no QVM form of either, and no third module: the menus are the
 * engine's own .urc resources.
 *
 * Every number here was measured in the retail binaries rather than taken
 * from any reconstruction. The game's GetGameAPI copies the engine's import
 * table with one `rep movsd` whose count is the import length, and writes its
 * own export slots as immediates directly afterwards; the client game's first
 * export does the same with its own import table. The game's import length is
 * confirmed a second time from the other side of the boundary, where
 * alice.exe zeroes exactly that many slots before filling them. The export lengths are the
 * highest slot any instruction in the library writes, and the game's is
 * corroborated a second way: its import copy lands immediately above its
 * export table, so the gap between the two is the export table's size.
 * See docs/fntech3/2026-09-22-alice-module-abi.md for the addresses.
 *
 * Every slot's signature is frozen too, in the alice_*_sigs.def tables at the
 * end of this header, and each function the engine serves into a slot or
 * calls an export through is held to its row at compile time
 * (docs/fntech3/2026-10-08-alice-module-signatures.md). Nothing in this
 * header loads, serves or calls anything: it is the map, not the boundary.
 * The boundary is code/server/server_alice_game.hpp and
 * code/client/client_alice_cgame.hpp. */

enum {
	/* Export slot 0 of the game table is this constant, written as an
	 * immediate by GetGameAPI before any pointer. */
	FNTECH3_ALICE_GAME_API_VERSION = 4,

	FNTECH3_ALICE_GAME_IMPORT_SLOTS = 111,
	FNTECH3_ALICE_GAME_EXPORT_SLOTS = 26,
	FNTECH3_ALICE_CGAME_IMPORT_SLOTS = 125,
	FNTECH3_ALICE_CGAME_EXPORT_SLOTS = 7,

	/* The size the game library writes into export slot 22 as an immediate.
	 * It is a record size, not a record: nothing about the entity's members
	 * is claimed here. */
	FNTECH3_ALICE_GENTITY_SIZE = 732,

	/* The client record size, which the module reports while running: its
	 * LocateGameData call publishes where its entities and clients live and
	 * how big each record is. Unlike the entity size this one is written
	 * nowhere in the library as an immediate, so running the module is what
	 * produced it. Identical on all thirty-six retail maps, and reported
	 * alongside a 732-byte entity record, which agrees with the immediate
	 * above by an unrelated route. It is a size, not a record. */
	FNTECH3_ALICE_GCLIENT_SIZE = 2112,

	/* The two delta tables' lengths, and how far each reaches into its
	 * record: the last field's offset plus its four bytes. Neither length is
	 * Quake III's 51 and 48.
	 *
	 * Neither reach is the record, either. A delta table describes what goes
	 * on the wire, and both records carry a tail past that; reading a table's
	 * end as its record's end is a mistake this reading made once and the
	 * engine corrected. */
	FNTECH3_ALICE_ENTITY_STATE_FIELDS = 79,
	FNTECH3_ALICE_PLAYER_STATE_FIELDS = 49,
	FNTECH3_ALICE_ENTITY_STATE_DELTA_BYTES = 456,
	FNTECH3_ALICE_PLAYER_STATE_DELTA_BYTES = 1044,

	/* The records themselves, each measured twice from the engine.
	 *
	 * An entity state is 512 bytes: `MSG_ReadDeltaEntity` clears that many of
	 * its destination for a removed entity and copies that many for an
	 * unchanged one, and the client's parse ring strides by the same --
	 * `shl esi, 9` at `0x0042d9e8`, over a 2,048-entry mask.
	 *
	 * A player state is 1,048: its delta writer and reader both clear and
	 * copy `0x106` dwords, and the client frame that holds one is 1,104
	 * bytes -- 1,048 plus a four-byte area count, thirty-two area bytes and
	 * five trailing integers. */
	FNTECH3_ALICE_ENTITY_STATE_BYTES = 512,
	FNTECH3_ALICE_PLAYER_STATE_BYTES = 1048,
	FNTECH3_ALICE_MAX_PARSE_ENTITIES = 2048,

	/* The engine's own half of the 732-byte entity record, every offset read
	 * out of an instruction in retail alice.exe that touches it. The `S_`
	 * names are inside the networked half and come from the delta field table
	 * below; the `R_` names are past it and come from the engine's linker,
	 * unlinker and snapshot walk.
	 *
	 * The entity state is 512 bytes, so the engine's half is the 220 that
	 * follow -- and every `R_` offset below lands inside them, which is the
	 * check that the two sizes agree. The `S_` offsets at 456 and 476 sit in
	 * the entity state's own tail, past the 456 its delta table reaches: the
	 * state carries members it never sends. See
	 * docs/fntech3/2026-09-22-alice-wire.md and
	 * docs/fntech3/2026-09-22-alice-module-abi.md. */
	FNTECH3_ALICE_ENTITY_SHARED_OFFSET = 512,

	/* Networked, and read by the engine as well. */
	FNTECH3_ALICE_S_NUMBER = 0,
	FNTECH3_ALICE_S_ORIGIN2 = 108,
	FNTECH3_ALICE_S_MODELINDEX = 184,
	/* Quake III's `solid` slot, filled with something else: the linker writes
	 * a collision handle here, or zero, or 0xffffff for a brush model, which
	 * is the value Quake III reserves for the same meaning. */
	FNTECH3_ALICE_S_SOLID = 436,
	FNTECH3_ALICE_S_RENDERFX = 448,

	/* In the entity state's tail rather than the engine's half, but read by
	 * the engine: the snapshot walk compares it against 1,023 and hides an
	 * entity whose owner is hidden. */
	FNTECH3_ALICE_S_OWNERNUM = 456,
	/* Also in that tail: the rotation the linker hands to a routine that
	 * walks a box's eight corners. Nine floats reaching exactly the record's
	 * end, which is what an axis is. Nothing here reads it; it is named so
	 * that no later reading mistakes it for an origin again. */
	FNTECH3_ALICE_S_AXIS = 476,

	/* The engine's own. `R_INUSE` is not `linked`: the linker returns at
	 * once when it is zero and the snapshot walk skips such an entity, but
	 * no engine instruction writes it, so the module owns it. */
	FNTECH3_ALICE_R_INUSE = 516,
	FNTECH3_ALICE_R_LINKED = 520,
	FNTECH3_ALICE_R_SVFLAGS = 528,
	FNTECH3_ALICE_R_BMODEL = 532,
	FNTECH3_ALICE_R_MINS = 536,
	FNTECH3_ALICE_R_MAXS = 548,
	FNTECH3_ALICE_R_CONTENTS = 560,
	FNTECH3_ALICE_R_ABSMIN = 564,
	FNTECH3_ALICE_R_ABSMAX = 576,
	FNTECH3_ALICE_R_CURRENTORIGIN = 608,
	FNTECH3_ALICE_R_CURRENTANGLES = 620,
	/* Quake II's `solid_t`, which this lineage kept: the linker compares it
	 * against 1, 2 and 3. Quake III has no such member. */
	FNTECH3_ALICE_R_SOLIDTYPE = 636,
	/* The engine writes a client's ping here, at `0x0045660b`, through the
	 * same index helper it uses for a player state. It is the first word past
	 * the part the delta table reaches, and the last word of the record: the
	 * two sizes meet on it. */
	FNTECH3_ALICE_PS_PING = 1044,

	/* Retail clamps sv_maxclients at this. */
	FNTECH3_ALICE_MAX_CLIENTS = 128,

	/* The configstring count the profile already carries, read out of the
	 * engine's own bounds checks. It is repeated here because the game state
	 * the client game is handed is sized by it: 1,328 offsets, 16,000 bytes
	 * of string data and a count come to the 21,316 bytes the engine copies,
	 * which is the second place this number has had to hold. */
	FNTECH3_ALICE_MAX_CONFIGSTRINGS = 1328,
	FNTECH3_ALICE_GAMESTATE_CHARS = 16000,

	/* What the engine does with each bit of FNTECH3_ALICE_R_SVFLAGS, read out
	 * of the snapshot walk at `0x004571e0` and the ping loop at `0x00456550`.
	 * Alice's bit positions are not Quake III's, so a record handed over
	 * untranslated would read as a different set of flags entirely.
	 *
	 * Bits 5 to 13 and 16 to 31 are never tested by any engine instruction
	 * this reading found, so nothing is claimed about them. */
	FNTECH3_ALICE_SVF_NOCLIENT = 0x0001,
	FNTECH3_ALICE_SVF_BOT = 0x0002,
	FNTECH3_ALICE_SVF_BROADCAST = 0x0004,
	FNTECH3_ALICE_SVF_PORTAL = 0x0008,
	/* Skips the walk's requirement that an entity carry a model index or a
	 * looping sound before it is worth sending. */
	FNTECH3_ALICE_SVF_ALWAYS_RELEVANT = 0x0010,
	/* A second bit reaching the same unconditional-send arm as BROADCAST. */
	FNTECH3_ALICE_SVF_BROADCAST_ALT = 0x4000,
	/* Set by the engine itself on every entity it walks, not by the game. */
	FNTECH3_ALICE_SVF_ENGINE_VISITED = 0x8000,

	/* The two export slots the library gives a fixed value. Slots 1 to 10
	 * and 14 to 20 are seventeen function pointers, also immediates; slots
	 * 11 to 13 are never written by any instruction in the library; and
	 * slots 21, 23, 24 and 25 are written from registers at run time, so
	 * what they hold is not established and none of them is named. */
	FNTECH3_ALICE_GAME_EXPORT_API_VERSION = 0,
	FNTECH3_ALICE_GAME_EXPORT_GENTITY_SIZE = 22,

	/* The client game's first export receives the engine's import table and
	 * is the only entry point the engine can reach before the rest are
	 * usable. Unlike the game's, this table carries no version slot. */
	FNTECH3_ALICE_CGAME_EXPORT_INIT = 0,

	/* The import the client game's initializer calls before it does anything
	 * else, straight after clearing its own state. The engine fills it with a
	 * function whose whole body returns the address of the block the game
	 * table build zeroes on startup, so it is a pointer to shared engine
	 * state rather than a call that does work. */
	FNTECH3_ALICE_CGAME_IMPORT_FIRST_CALLED = 124,

	/* The engine's own builder, at 0x00424400, clears 125 slots and then
	 * fills 122 of them. These three it never writes, and slot 0 being one
	 * of them means this table begins with a hole rather than a member. */
	FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_A = 0,
	FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_B = 88,
	FNTECH3_ALICE_CGAME_IMPORT_UNFILLED_C = 116,

	/* The engine block both modules ask for: the game through import slot 107
	 * and the client game through its own slot 124, each served by a routine
	 * whose whole body returns the address 0xc300a0. The size is measured from
	 * the routine that builds the game import table, which opens by clearing
	 * fifteen dwords there before it fills a single slot. */
	FNTECH3_ALICE_SHARED_BLOCK_BYTES = 60,

	/* The record the trace import fills. Measured from the copy that leaves
	 * the engine rather than from the clear that precedes it: two routines
	 * each move fifteen dwords into the caller's buffer, while the thirty
	 * dwords cleared first are working space. A size read from the clear
	 * would have been twice the truth. */
	FNTECH3_ALICE_TRACE_BYTES = 60,

	/* The export slot the engine treats as a deferred error message. After
	 * every export call, alice.exe reads this slot and, when it is not null,
	 * hands it to its own error routine and clears it. So the module reports
	 * a failure by leaving a message here rather than by returning one, and
	 * an engine that never reads it swallows every such failure silently. */
	FNTECH3_ALICE_GAME_EXPORT_DEFERRED_ERROR = 25
};

/* Every game import slot by name. The names are the reference table's; the
 * slot numbers are Alice's own, measured in the engine's table build. */
enum {
#define FNTECH3_ALICE_GAME_IMPORT( slot, name, evidence ) \
	FNTECH3_ALICE_IMPORT_##name = slot,
#include "alice_game_imports.def"
#undef FNTECH3_ALICE_GAME_IMPORT
	FNTECH3_ALICE_IMPORT_SENTINEL = FNTECH3_ALICE_GAME_IMPORT_SLOTS
};

/* Every game export slot by name, in the order the module writes them. */
enum {
#define FNTECH3_ALICE_GAME_EXPORT( slot, name, kind, evidence ) \
	FNTECH3_ALICE_EXPORT_##name = slot,
#include "alice_game_exports.def"
#undef FNTECH3_ALICE_GAME_EXPORT
	FNTECH3_ALICE_EXPORT_SENTINEL = FNTECH3_ALICE_GAME_EXPORT_SLOTS
};

/* Every client-game import slot by name. The names are the reference table's,
 * which Alice kept in order; the slot numbers are measured in the engine's
 * own table build at 0x00424400. */
enum {
#define FNTECH3_ALICE_CGAME_IMPORT( slot, name, evidence, sites, args, basis ) \
	FNTECH3_ALICE_CG_##name = slot,
#include "alice_cgame_imports.def"
#undef FNTECH3_ALICE_CGAME_IMPORT
	FNTECH3_ALICE_CG_SENTINEL = FNTECH3_ALICE_CGAME_IMPORT_SLOTS
};

/* The client game's seven exports, in the order GetCGameAPI writes them. */
enum {
#define FNTECH3_ALICE_CGAME_EXPORT( slot, name, args, evidence ) \
	FNTECH3_ALICE_CGE_##name = slot,
#include "alice_cgame_exports.def"
#undef FNTECH3_ALICE_CGAME_EXPORT
	FNTECH3_ALICE_CGE_SENTINEL = FNTECH3_ALICE_CGAME_EXPORT_SLOTS
};

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fntech3::alice::module {

/* How a slot's name was established. This is part of the claim: an ORDER
 * slot is named from its neighbours alone and carries less weight than one
 * its own body identifies. ADDED and ABSENT name nothing. */
enum class Evidence : unsigned char {
	LITERAL, BEHAVIOUR, ARITY, ORDER, ADDED, UNNAMED, ABSENT,
	/* Client-game only: the same engine address a named game slot holds,
	 * and a slot copied out of the engine's renderer export block. */
	SHARED, RENDERER
};

struct ImportSlot {
	int slot;
	const char *name;
	Evidence evidence;
};

inline constexpr ImportSlot kGameImports[] = {
#define FNTECH3_ALICE_GAME_IMPORT( slot, name, evidence ) \
	{ slot, #name, Evidence::evidence },
#include "alice_game_imports.def"
#undef FNTECH3_ALICE_GAME_IMPORT
};

static_assert( sizeof( kGameImports ) / sizeof( kGameImports[0] )
	== FNTECH3_ALICE_GAME_IMPORT_SLOTS,
	"every measured import slot needs one row" );

/* How the module's own call sites fix a slot's argument count, or fail to.
 * See alice_game_import_calls.def for what each basis means. */
enum class CallBasis : unsigned char {
	AGREED, PUSHES, VARIADIC, UNSETTLED, UNCALLED
};

/* What Alice's module does with a slot: how often it calls it, and how many
 * arguments those calls establish. `args` is -1 where the sites establish
 * none, which is every VARIADIC, UNSETTLED and UNCALLED row. */
struct ImportCalls {
	int slot;
	int sites;
	int args;
	CallBasis basis;
};

inline constexpr ImportCalls kGameImportCalls[] = {
#define FNTECH3_ALICE_GAME_IMPORT_CALL( slot, sites, args, basis ) \
	{ slot, sites, args, CallBasis::basis },
#include "alice_game_import_calls.def"
#undef FNTECH3_ALICE_GAME_IMPORT_CALL
};

static_assert( sizeof( kGameImportCalls ) / sizeof( kGameImportCalls[0] )
	== FNTECH3_ALICE_GAME_IMPORT_SLOTS,
	"every import slot needs one call-site row" );

/* An export slot is either a function the engine may call or a field the
 * module fills in. Handing a VALUE slot to a call site is how an adapter
 * jumps to an entity count. */
enum class SlotKind : unsigned char { FUNCTION, VALUE };

/* How an export slot was identified. ABSENT means no instruction in the
 * module writes it, so the engine must never call it. */
enum class ExportEvidence : unsigned char {
	IMMEDIATE, RUNTIME, ABSENT, ORDER, ADDED,
	/* Client-game only: the export's own body says what it is. */
	BEHAVIOUR
};

struct ExportSlot {
	int slot;
	const char *name;
	SlotKind kind;
	ExportEvidence evidence;
};

inline constexpr ExportSlot kGameExports[] = {
#define FNTECH3_ALICE_GAME_EXPORT( slot, name, kind, evidence ) \
	{ slot, #name, SlotKind::kind, ExportEvidence::evidence },
#include "alice_game_exports.def"
#undef FNTECH3_ALICE_GAME_EXPORT
};

static_assert( sizeof( kGameExports ) / sizeof( kGameExports[0] )
	== FNTECH3_ALICE_GAME_EXPORT_SLOTS,
	"every measured export slot needs one row" );


/* The client game's import map, with what its own call sites establish. */
struct CGameImportSlot {
	int slot;
	const char *name;
	Evidence evidence;
	int sites;
	int args;
	CallBasis basis;
};

inline constexpr CGameImportSlot kCGameImports[] = {
#define FNTECH3_ALICE_CGAME_IMPORT( slot, name, evidence, sites, args, basis ) \
	{ slot, #name, Evidence::evidence, sites, args, CallBasis::basis },
#include "alice_cgame_imports.def"
#undef FNTECH3_ALICE_CGAME_IMPORT
};

static_assert( sizeof( kCGameImports ) / sizeof( kCGameImports[0] )
	== FNTECH3_ALICE_CGAME_IMPORT_SLOTS,
	"every measured client-game import slot needs one row" );

struct CGameExportSlot {
	int slot;
	const char *name;
	int args;
	ExportEvidence evidence;
};

inline constexpr CGameExportSlot kCGameExports[] = {
#define FNTECH3_ALICE_CGAME_EXPORT( slot, name, args, evidence ) \
	{ slot, #name, args, ExportEvidence::evidence },
#include "alice_cgame_exports.def"
#undef FNTECH3_ALICE_CGAME_EXPORT
};

static_assert( sizeof( kCGameExports ) / sizeof( kCGameExports[0] )
	== FNTECH3_ALICE_CGAME_EXPORT_SLOTS,
	"every client-game export slot needs one row" );

/* Raw slot maps, untyped as the tables are in memory. What may go into each
 * slot is the frozen signature's business (Fits, below), so a wrong prototype
 * is a compile error where it is served rather than a silent ABI break. */
struct GameImportTable {
	std::intptr_t slot[FNTECH3_ALICE_GAME_IMPORT_SLOTS];
};

struct GameExportTable {
	std::intptr_t slot[FNTECH3_ALICE_GAME_EXPORT_SLOTS];
};

struct CGameImportTable {
	std::intptr_t slot[FNTECH3_ALICE_CGAME_IMPORT_SLOTS];
};

struct CGameExportTable {
	std::intptr_t slot[FNTECH3_ALICE_CGAME_EXPORT_SLOTS];
};

/* Retail Alice is a Win32 x86 build, so every slot is four bytes there. A
 * host with wider pointers would change these sizes, which is why the module
 * boundary is x86-only and the loader refuses anything else. */
constexpr std::size_t kRetailSlotBytes = 4;

constexpr std::size_t GameImportBytes() {
	return FNTECH3_ALICE_GAME_IMPORT_SLOTS * kRetailSlotBytes;
}
constexpr std::size_t GameExportBytes() {
	return FNTECH3_ALICE_GAME_EXPORT_SLOTS * kRetailSlotBytes;
}
constexpr std::size_t CGameImportBytes() {
	return FNTECH3_ALICE_CGAME_IMPORT_SLOTS * kRetailSlotBytes;
}
constexpr std::size_t CGameExportBytes() {
	return FNTECH3_ALICE_CGAME_EXPORT_SLOTS * kRetailSlotBytes;
}

/* One row of a delta field table, as the retail engine stores it: a name, a
 * byte offset into the record, and a bit count where zero means a float, a
 * positive count means that many unsigned bits and a negative count means
 * that many signed bits.
 *
 * These are declarations, not a codec. Nothing in fnTech3 encodes or decodes
 * an Alice game message; the tables are frozen here because they are what
 * such a codec would have to agree with, and because they bound the two
 * records: 79 entity fields ending at 452, so the entity state is at least
 * 456 bytes, and 49 player fields ending at 1040, so the player state is at
 * least 1,044. Neither number is Quake III's. */
struct WireField {
	const char *name;
	int offset;
	int bits;
};

inline constexpr WireField kEntityStateFields[] = {
#define FNTECH3_ALICE_ENTITY_FIELD( name, offset, bits ) { name, offset, bits },
#include "alice_wire_fields.def"
#undef FNTECH3_ALICE_ENTITY_FIELD
};

inline constexpr WireField kPlayerStateFields[] = {
#define FNTECH3_ALICE_PLAYER_FIELD( name, offset, bits ) { name, offset, bits },
#include "alice_wire_fields.def"
#undef FNTECH3_ALICE_PLAYER_FIELD
};

static_assert( sizeof( kEntityStateFields ) / sizeof( kEntityStateFields[0] )
	== FNTECH3_ALICE_ENTITY_STATE_FIELDS,
	"the entity delta table is the length the retail array is" );
static_assert( sizeof( kPlayerStateFields ) / sizeof( kPlayerStateFields[0] )
	== FNTECH3_ALICE_PLAYER_STATE_FIELDS,
	"the player delta table is the length the retail array is" );

/* The engine names a module by its entry point rather than by its filename,
 * because Alice keeps Quake III's cgamex86.dll name for a library that does
 * not speak Quake III's ABI at all. */
enum class Entry : unsigned { Game, ClientGame };

constexpr const char *EntryPointName( Entry entry ) {
	return entry == Entry::Game ? "GetGameAPI" : "GetCGameAPI";
}

constexpr const char *LibraryName( Entry entry ) {
	return entry == Entry::Game ? "fgamex86.dll" : "cgamex86.dll";
}

/* ---- the frozen signatures ------------------------------------------------
 *
 * Every slot's shape, from alice_*_sigs.def. A function served into a slot, or
 * a type the engine calls an export through, is checked against its row at
 * compile time by ABI class -- void, a 32-bit integer, a 16- or 8-bit one, a
 * float, a pointer -- with its parameter count and whether it is variadic.
 * The classes are compared rather than the C++ types because this engine
 * hands records over as their own types where a row says `void *`; what the
 * two sides of a cdecl call must agree on is each argument's class. */
enum class Abi : unsigned char { Void, I32, U16, U8, F32, Ptr, Other };

template<typename T> constexpr Abi AbiOf() {
	if constexpr ( std::is_void_v<T> ) return Abi::Void;
	else if constexpr ( std::is_pointer_v<T> ) return Abi::Ptr;
	else if constexpr ( std::is_same_v<std::remove_cv_t<T>, float> ) return Abi::F32;
	else if constexpr ( std::is_integral_v<T> || std::is_enum_v<T> ) {
		if constexpr ( sizeof( T ) == 1 ) return Abi::U8;
		else if constexpr ( sizeof( T ) == 2 ) return Abi::U16;
		else return Abi::I32;
	} else return Abi::Other;
}

template<typename F> struct Shape {
	static constexpr bool function = false;
};
template<typename R, typename... A> struct Shape<R ( * )( A... )> {
	static constexpr bool function = true;
	static constexpr bool variadic = false;
	static constexpr std::size_t params = sizeof...( A );
	static constexpr Abi abi[] = { AbiOf<R>(), AbiOf<A>()... };
};
template<typename R, typename... A> struct Shape<R ( * )( A..., ... )> {
	static constexpr bool function = true;
	static constexpr bool variadic = true;
	static constexpr std::size_t params = sizeof...( A );
	static constexpr Abi abi[] = { AbiOf<R>(), AbiOf<A>()... };
};

template<typename F, typename G> constexpr bool SameShape() {
	using A = Shape<F>;
	using B = Shape<G>;
	if constexpr ( !A::function || !B::function ) {
		return false;
	} else if constexpr ( A::variadic != B::variadic || A::params != B::params ) {
		return false;
	} else {
		for ( std::size_t i = 0; i <= A::params; ++i ) {
			if ( A::abi[i] != B::abi[i] ) return false;
		}
		return true;
	}
}

/* A row's kind: a function, a value the slot holds, or a slot retail leaves
 * empty. */
enum class SigKind : unsigned char { FUNCTION, VALUE, ABSENT };

#define FNTECH3_ALICE_SIG_FUNCTION( type, params ) type ( * ) params
#define FNTECH3_ALICE_SIG_VALUE( type, params ) type
#define FNTECH3_ALICE_SIG_ABSENT( type, params ) void

/* `Type` is the function-pointer type for a FUNCTION row and the held value's
 * type for a VALUE row. */
template<int Slot> struct GameImportSig;
template<int Slot> struct GameExportSig;
template<int Slot> struct CGameImportSig;
template<int Slot> struct CGameExportSig;

#define FNTECH3_ALICE_SIG_SPECIALIZE( table, slot, kind, type, params ) \
	template<> struct table<slot> { \
		static constexpr SigKind sigKind = SigKind::kind; \
		using Type = FNTECH3_ALICE_SIG_##kind( type, params ); \
	};
#define FNTECH3_ALICE_GAME_IMPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_SPECIALIZE( GameImportSig, slot, kind, type, params )
#include "alice_game_import_sigs.def"
#undef FNTECH3_ALICE_GAME_IMPORT_SIG
#define FNTECH3_ALICE_GAME_EXPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_SPECIALIZE( GameExportSig, slot, kind, type, params )
#include "alice_game_export_sigs.def"
#undef FNTECH3_ALICE_GAME_EXPORT_SIG
#define FNTECH3_ALICE_CGAME_IMPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_SPECIALIZE( CGameImportSig, slot, kind, type, params )
#include "alice_cgame_import_sigs.def"
#undef FNTECH3_ALICE_CGAME_IMPORT_SIG
#define FNTECH3_ALICE_CGAME_EXPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_SPECIALIZE( CGameExportSig, slot, kind, type, params )
#include "alice_cgame_export_sigs.def"
#undef FNTECH3_ALICE_CGAME_EXPORT_SIG
#undef FNTECH3_ALICE_SIG_SPECIALIZE

/* What a function served into a slot, or a type an export is called through,
 * must satisfy. */
template<typename Sig, typename F> constexpr bool Fits() {
	return Sig::sigKind == SigKind::FUNCTION && SameShape<F, typename Sig::Type>();
}

/* A value slot's own type, by class. */
template<typename Sig, typename T> constexpr bool HoldsValue() {
	return Sig::sigKind == SigKind::VALUE && AbiOf<T>() == AbiOf<typename Sig::Type>();
}

/* The rows themselves, for the tests that hold them to the other tables. */
struct SigRow {
	int slot;
	const char *name;
	SigKind kind;
	const char *type;
	const char *params;
	const char *evidence;
	/* For a FUNCTION row: its parameter count, -1 otherwise, and whether its
	 * last parameter is `...`. */
	int paramCount;
	bool variadic;
};

template<typename Sig> constexpr int SigParams() {
	if constexpr ( Sig::sigKind == SigKind::FUNCTION ) return static_cast<int>( Shape<typename Sig::Type>::params );
	else return -1;
}
template<typename Sig> constexpr bool SigVariadic() {
	if constexpr ( Sig::sigKind == SigKind::FUNCTION ) return Shape<typename Sig::Type>::variadic;
	else return false;
}

#define FNTECH3_ALICE_SIG_ROW( table, slot, name, kind, type, params, evidence ) \
	{ slot, #name, SigKind::kind, #type, #params, #evidence, SigParams<table<slot>>(), SigVariadic<table<slot>>() },
inline constexpr SigRow kGameImportSigs[] = {
#define FNTECH3_ALICE_GAME_IMPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_ROW( GameImportSig, slot, name, kind, type, params, evidence )
#include "alice_game_import_sigs.def"
#undef FNTECH3_ALICE_GAME_IMPORT_SIG
};
inline constexpr SigRow kGameExportSigs[] = {
#define FNTECH3_ALICE_GAME_EXPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_ROW( GameExportSig, slot, name, kind, type, params, evidence )
#include "alice_game_export_sigs.def"
#undef FNTECH3_ALICE_GAME_EXPORT_SIG
};
inline constexpr SigRow kCGameImportSigs[] = {
#define FNTECH3_ALICE_CGAME_IMPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_ROW( CGameImportSig, slot, name, kind, type, params, evidence )
#include "alice_cgame_import_sigs.def"
#undef FNTECH3_ALICE_CGAME_IMPORT_SIG
};
inline constexpr SigRow kCGameExportSigs[] = {
#define FNTECH3_ALICE_CGAME_EXPORT_SIG( slot, name, kind, type, params, evidence ) \
	FNTECH3_ALICE_SIG_ROW( CGameExportSig, slot, name, kind, type, params, evidence )
#include "alice_cgame_export_sigs.def"
#undef FNTECH3_ALICE_CGAME_EXPORT_SIG
};
#undef FNTECH3_ALICE_SIG_ROW

static_assert( sizeof( kGameImportSigs ) / sizeof( kGameImportSigs[0] ) == FNTECH3_ALICE_GAME_IMPORT_SLOTS,
	"every game import slot has a signature" );
static_assert( sizeof( kGameExportSigs ) / sizeof( kGameExportSigs[0] ) == FNTECH3_ALICE_GAME_EXPORT_SLOTS,
	"every game export slot has a signature" );
static_assert( sizeof( kCGameImportSigs ) / sizeof( kCGameImportSigs[0] ) == FNTECH3_ALICE_CGAME_IMPORT_SLOTS,
	"every client-game import slot has a signature" );
static_assert( sizeof( kCGameExportSigs ) / sizeof( kCGameExportSigs[0] ) == FNTECH3_ALICE_CGAME_EXPORT_SLOTS,
	"every client-game export slot has a signature" );

} // namespace fntech3::alice::module
#endif

#endif
