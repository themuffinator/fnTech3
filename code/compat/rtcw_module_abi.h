/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_RTCW_MODULE_ABI_H
#define FNTECH3_COMPAT_RTCW_MODULE_ABI_H

/* Frozen external numbers. Similar names in Q3, QL, SP and MP do not imply
 * matching ordinals or argument contracts. -1 means absent from that mode.
 */
enum {
	FNTECH3_RTCW_UI_API_VERSION = 4,
	FNTECH3_RTCW_CGAME_API_VERSION = 3,
	FNTECH3_RTCW_GAME_API_VERSION = 8,
#define FNTECH3_RTCW_MODULE_CALL(module, direction, name, sp, mp) \
	FNTECH3_RTCW_SP_##name = sp, FNTECH3_RTCW_MP_##name = mp,
#include "rtcw_module_calls.def"
#undef FNTECH3_RTCW_MODULE_CALL
};

#ifdef __cplusplus
#include "../qcommon/game_profile.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace fntech3::rtcw::module {

enum class Module : unsigned { UI, CGAME, GAME };
enum class Direction : unsigned { IMPORT, EXPORT };
enum class Call : uint16_t {
	Invalid,
#define FNTECH3_RTCW_MODULE_CALL(module, direction, name, sp, mp) name,
#include "rtcw_module_calls.def"
#undef FNTECH3_RTCW_MODULE_CALL
	Count
};

struct Descriptor {
	Module module;
	Direction direction;
	Call call;
	int sp, mp;
	const char *name;
};

inline constexpr Descriptor descriptors[] = {
#define FNTECH3_RTCW_MODULE_CALL(module, direction, name, sp, mp) \
	{ Module::module, Direction::direction, Call::name, sp, mp, #name },
#include "rtcw_module_calls.def"
#undef FNTECH3_RTCW_MODULE_CALL
};

inline constexpr unsigned maxOrdinal = [] {
	unsigned maximum = 0;
	for ( const auto &call : descriptors ) {
		if ( call.sp > (int)maximum ) maximum = call.sp;
		if ( call.mp > (int)maximum ) maximum = call.mp;
	}
	return maximum;
}();
static_assert( maxOrdinal < 2048, "Bound native call tables" );

struct LookupTables {
	// Default initialization gives every gap the explicit Invalid operation.
	std::array<std::array<std::array<Call, maxOrdinal + 1>, 2>, 3> sp{}, mp{};
	bool valid = true;
	constexpr LookupTables() {
		for ( const auto &call : descriptors ) {
			const auto m = (unsigned)call.module, d = (unsigned)call.direction;
			for ( unsigned mode = 0; mode < 2; ++mode ) {
				const int number = mode ? call.mp : call.sp;
				if ( number < 0 ) continue;
				auto &entry = ( mode ? mp : sp )[m][d][number];
				if ( entry != Call::Invalid ) valid = false;
				entry = call.call;
			}
		}
	}
};

inline constexpr LookupTables lookupTables;
static_assert( lookupTables.valid, "Native ordinals must be unique within each module/mode" );
static_assert( sizeof( descriptors ) / sizeof( descriptors[0] ) + 1 == (unsigned)Call::Count );

inline constexpr Call Lookup( comGameId_t profile, Module module,
	Direction direction, intptr_t ordinal ) {
	if ( (unsigned)module >= 3 || (unsigned)direction >= 2
		|| ordinal < 0 || ordinal > maxOrdinal ) return Call::Invalid;
	if ( profile == COM_GAME_RTCW_SP ) {
		return lookupTables.sp[(unsigned)module][(unsigned)direction][ordinal];
	}
	if ( profile == COM_GAME_RTCW_MP ) {
		return lookupTables.mp[(unsigned)module][(unsigned)direction][ordinal];
	}
	return Call::Invalid;
}

inline constexpr const Descriptor *Describe( Call call ) {
	const auto index = (unsigned)call;
	return index && index < (unsigned)Call::Count ? &descriptors[index - 1] : nullptr;
}

inline constexpr int Ordinal( comGameId_t profile, Call call ) {
	const auto *descriptor = Describe( call );
	if ( !descriptor ) return -1;
	return profile == COM_GAME_RTCW_SP ? descriptor->sp
		: profile == COM_GAME_RTCW_MP ? descriptor->mp : -1;
}

} // namespace fntech3::rtcw::module
#endif
#endif
