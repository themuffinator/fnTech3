/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_MODULE_ABI_H
#define FNTECH3_COMPAT_JKA_MODULE_ABI_H

/* Frozen Jedi Academy module numbers. Names shared with Quake III do not imply
 * matching ordinals or argument contracts: Raven kept some of Quake III's
 * numbering and inserted its own calls throughout. Single player is not here;
 * it hands the engine a function table instead of numbering its calls. */
enum {
	FNTECH3_JKA_MP_GAME_API_VERSION = 1,
	FNTECH3_JKA_MP_CGAME_API_VERSION = 2,
	FNTECH3_JKA_MP_UI_API_VERSION = 3,
	FNTECH3_JKA_SP_GAME_API_VERSION = 10,
#define FNTECH3_JKA_MODULE_CALL( module, direction, name, ordinal ) \
	FNTECH3_JKA_##name = ordinal,
#include "jka_module_calls.def"
#undef FNTECH3_JKA_MODULE_CALL
};

#ifdef __cplusplus
#include "../qcommon/game_profile.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace fntech3::jka::module {

enum class Module : unsigned { UI, CGAME, GAME };
enum class Direction : unsigned { IMPORT, EXPORT };
enum class Call : uint16_t {
	Invalid,
#define FNTECH3_JKA_MODULE_CALL( module, direction, name, ordinal ) name,
#include "jka_module_calls.def"
#undef FNTECH3_JKA_MODULE_CALL
	Count
};

struct Descriptor {
	Module module;
	Direction direction;
	Call call;
	int ordinal;
	const char *name;
};

inline constexpr Descriptor descriptors[] = {
#define FNTECH3_JKA_MODULE_CALL( module, direction, name, ordinal ) \
	{ Module::module, Direction::direction, Call::name, ordinal, #name },
#include "jka_module_calls.def"
#undef FNTECH3_JKA_MODULE_CALL
};

inline constexpr unsigned maxOrdinal = [] {
	unsigned maximum = 0;
	for ( const auto &call : descriptors ) {
		if ( call.ordinal > (int)maximum ) maximum = (unsigned)call.ordinal;
	}
	return maximum;
}();
static_assert( maxOrdinal < 1024, "Bound module call tables" );

struct LookupTables {
	// Default initialization gives every gap the explicit Invalid operation.
	std::array<std::array<std::array<Call, maxOrdinal + 1>, 2>, 3> calls{};
	bool valid = true;
	constexpr LookupTables() {
		for ( const auto &call : descriptors ) {
			const auto m = (unsigned)call.module, d = (unsigned)call.direction;
			if ( call.ordinal < 0 ) { valid = false; continue; }
			auto &entry = calls[m][d][call.ordinal];
			if ( entry != Call::Invalid ) valid = false;
			entry = call.call;
		}
	}
};

inline constexpr LookupTables lookupTables;
static_assert( lookupTables.valid, "Jedi Academy ordinals must be unique within each module" );
static_assert( sizeof( descriptors ) / sizeof( descriptors[0] ) + 1 == (unsigned)Call::Count );

/* Multiplayer alone numbers its calls, so a single-player profile resolves
 * nothing here and neither does another title. */
inline constexpr Call Lookup( comGameId_t profile, Module module,
	Direction direction, intptr_t ordinal ) {
	if ( profile != COM_GAME_JKA_MP || (unsigned)module >= 3
		|| (unsigned)direction >= 2 || ordinal < 0
		|| ordinal > (intptr_t)maxOrdinal ) return Call::Invalid;
	return lookupTables.calls[(unsigned)module][(unsigned)direction][ordinal];
}

inline constexpr const Descriptor *Describe( Call call ) {
	const auto index = (unsigned)call;
	return index && index < (unsigned)Call::Count ? &descriptors[index - 1] : nullptr;
}

inline constexpr int Ordinal( comGameId_t profile, Call call ) {
	const auto *descriptor = Describe( call );
	if ( !descriptor || profile != COM_GAME_JKA_MP ) return -1;
	return descriptor->ordinal;
}

} // namespace fntech3::jka::module
#endif
#endif
