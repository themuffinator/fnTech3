/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_ICARUS_IDS_H
#define FNTECH3_COMPAT_JKA_ICARUS_IDS_H

/* Raven's own names for the numbers a compiled ICARUS script carries, from the
 * frozen table in jka_icarus_ids.def: tokens, block ids and types, drawn from
 * four enumerations that run on from one another into one space of 0 to 70.
 * Names
 * only: nothing here runs a block or decides what one does, and the readers
 * beside this file work from the shapes the scripts themselves fix rather than
 * from these names.
 *
 * Both were arrived at separately. The numbers were derived from a retail
 * installation's 1,011 scripts first and read from Raven's enumerations after,
 * and they agree on every one; see docs/fntech3/2026-09-21-jka-icarus.md. */

#include <cstdint>

namespace fntech3 {
namespace jka {
namespace icarus {

enum class Token : std::int32_t {
#define FNTECH3_JKA_ICARUS_TOKEN( name, number ) name = number,
#define FNTECH3_JKA_ICARUS_BLOCK( name, number )
#define FNTECH3_JKA_ICARUS_TYPE( name, number )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
};

enum class BlockId : std::int32_t {
#define FNTECH3_JKA_ICARUS_TOKEN( name, number )
#define FNTECH3_JKA_ICARUS_BLOCK( name, number ) name = number,
#define FNTECH3_JKA_ICARUS_TYPE( name, number )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
};

enum class Type : std::int32_t {
#define FNTECH3_JKA_ICARUS_TOKEN( name, number )
#define FNTECH3_JKA_ICARUS_BLOCK( name, number )
#define FNTECH3_JKA_ICARUS_TYPE( name, number ) name = number,
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
};

constexpr std::int32_t Number( Token token ) { return static_cast<std::int32_t>( token ); }
constexpr std::int32_t Number( BlockId block ) { return static_cast<std::int32_t>( block ); }
constexpr std::int32_t Number( Type type ) { return static_cast<std::int32_t>( type ); }

/* The name of a number, or null when this table does not have it. A mod's
 * script may use one it never held. */
inline const char *TokenName( std::int32_t number ) {
#define FNTECH3_JKA_ICARUS_TOKEN( name, value ) if ( number == ( value ) ) return #name;
#define FNTECH3_JKA_ICARUS_BLOCK( name, value )
#define FNTECH3_JKA_ICARUS_TYPE( name, value )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	return nullptr;
}

inline const char *BlockName( std::int32_t number ) {
#define FNTECH3_JKA_ICARUS_TOKEN( name, value )
#define FNTECH3_JKA_ICARUS_BLOCK( name, value ) if ( number == ( value ) ) return #name;
#define FNTECH3_JKA_ICARUS_TYPE( name, value )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	return nullptr;
}

inline const char *TypeName( std::int32_t number ) {
#define FNTECH3_JKA_ICARUS_TOKEN( name, value )
#define FNTECH3_JKA_ICARUS_BLOCK( name, value )
#define FNTECH3_JKA_ICARUS_TYPE( name, value ) if ( number == ( value ) ) return #name;
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	return nullptr;
}

/* How many of each the table holds, so a member that goes missing is a build
 * failure rather than a quieter table. */
constexpr int TokenCount = 0
#define FNTECH3_JKA_ICARUS_TOKEN( name, number ) + 1
#define FNTECH3_JKA_ICARUS_BLOCK( name, number )
#define FNTECH3_JKA_ICARUS_TYPE( name, number )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	;

constexpr int BlockCount = 0
#define FNTECH3_JKA_ICARUS_TOKEN( name, number )
#define FNTECH3_JKA_ICARUS_BLOCK( name, number ) + 1
#define FNTECH3_JKA_ICARUS_TYPE( name, number )
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	;

constexpr int TypeCount = 0
#define FNTECH3_JKA_ICARUS_TOKEN( name, number )
#define FNTECH3_JKA_ICARUS_BLOCK( name, number )
#define FNTECH3_JKA_ICARUS_TYPE( name, number ) + 1
#include "jka_icarus_ids.def"
#undef FNTECH3_JKA_ICARUS_TOKEN
#undef FNTECH3_JKA_ICARUS_BLOCK
#undef FNTECH3_JKA_ICARUS_TYPE
	;

/* They run on from one another without a gap, which is the whole reason a
 * member number, a type tag and a block number share one space. */
static_assert( TokenCount == 19, "every token Raven names is in the table" );
static_assert( BlockCount == 32, "every block id Raven names is in the table" );
static_assert( TypeCount == 20, "every type Raven names is in the table" );
static_assert( Number( Token::TK_BLOCK_START ) == 8, "user tokens follow the base ones" );
static_assert( Number( BlockId::ID_AFFECT ) == TokenCount, "block ids follow the tokens" );
static_assert( Number( Type::TYPE_WAIT_COMPLETE ) == TokenCount + BlockCount,
	"types follow the block ids" );

} // namespace icarus
} // namespace jka
} // namespace fntech3

#endif
