/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_ICARUS_VALUES_HPP
#define FNTECH3_COMPAT_JKA_ICARUS_VALUES_HPP

/* The value layer over Jedi Academy's ICARUS container: it turns a block's
 * members into the values that block was given.
 *
 * A member's payload is not always its value. Some members introduce the ones
 * after them -- three floats, a type and a name, a pair of bounds -- and this
 * groups them. The rules are exactly those recorded in
 * docs/fntech3/2026-09-21-jka-icarus.md, which consume every one of the 26,512
 * blocks in a legitimate installation without refusing one.
 *
 * They are rules read off one installation's scripts, not a specification, so
 * this refuses anything they do not cover rather than guessing at it. A member
 * number that corpus never used, a group that runs off the end of its block, an
 * operator anywhere but last: each is a refusal with its own name.
 *
 * Still nothing is interpreted. A block keeps its number, a value keeps its
 * bytes, and what either means is left to whatever runs them. */

#include "jka_icarus_format.hpp"
#include "jka_icarus_ids.h"

namespace fntech3 {
namespace jka {
namespace icarus {

/* The member numbers that carry or introduce a value, under Raven's own names.
 * Which shape each one introduces was established from the scripts; the names
 * come from the table beside them and agree with it. */
inline constexpr std::int32_t MemberText = Number( Token::TK_STRING );
inline constexpr std::int32_t MemberNumber = Number( Token::TK_FLOAT );
inline constexpr std::int32_t MemberName = Number( Token::TK_IDENTIFIER );
inline constexpr std::int32_t MemberVector = Number( Token::TK_VECTOR );
inline constexpr std::int32_t MemberTyped = Number( BlockId::ID_GET );
inline constexpr std::int32_t MemberRange = Number( BlockId::ID_RANDOM );
inline constexpr std::int32_t MemberTagged = Number( BlockId::ID_TAG );

/* The four that compare, and end their block when they do: greater than, less
 * than, equals and not, in that order. */
inline constexpr std::int32_t MemberCompareFirst = Number( Token::TK_GREATER_THAN );
inline constexpr std::int32_t MemberCompareLast = Number( Token::TK_NOT );

/* A type number names a kind of value with the same number that kind's members
 * carry, which is why these three and no others are admitted. */
inline bool IsTypeNumber( std::int32_t type ) {
	return type == MemberText || type == MemberNumber || type == MemberVector;
}

enum class ValueKind {
	Text,      /* member 4: terminated text */
	Name,      /* member 7: terminated text from a closed vocabulary */
	Number,    /* member 6: one float */
	Vector,    /* member 14: three floats */
	Typed,     /* member 36: a type number, then a name of that type */
	Range,     /* member 37: two floats; every installed one is ordered */
	Tagged,    /* member 49: a name, given plainly or by type, then a float */
};

struct Value {
	ValueKind kind = ValueKind::Number;
	/* Text and Name; the name inside a Typed; the name inside a Tagged.
	 * Null for a Number, a Vector or a Range. Points into the file. */
	const char *text = nullptr;
	/* Number uses [0]; Vector all three; Range the first two, low then high;
	 * Tagged the float after its name, in [0]. */
	float numbers[3] = { 0.0f, 0.0f, 0.0f };
	/* Typed: the member number naming the type. Tagged: the same, when its
	 * name came through a type, or 0 when the name was given plainly. */
	std::int32_t type = 0;
};

/* One block, with its values and, if it has one, its comparison. A statement's
 * values are `values` entries from `firstValue`, contiguous and nothing else:
 * a value that another introduces is folded into the one that introduced it. */
struct Statement {
	std::int32_t block = 0;
	std::uint8_t flags = 0;
	int firstValue = 0;
	int values = 0;
	/* 0, or the member number of the comparison this block ends with. */
	std::int32_t compare = 0;
	/* The value on the far side of that comparison, or -1. It is the last of
	 * this statement's values when there is one. */
	int compareValue = -1;
};

enum class ValueError {
	None,
	Container,   /* the container itself is malformed; ask it why */
	Member,      /* a member number no recorded rule covers */
	Group,       /* an introduced run that is not the shape it must be */
	Text,        /* a member that must be text and is not */
	Number,      /* a member that must be one float and is not */
	Type,        /* a type number that names no kind of value */
	Compare,     /* a comparison that is not the last thing in its block */
	Capacity,    /* the caller's arrays are too small for this script */
};

inline const char *ValueErrorName( ValueError error ) {
	switch ( error ) {
	case ValueError::None: return "none";
	case ValueError::Container: return "malformed container";
	case ValueError::Member: return "unrecognized member number";
	case ValueError::Group: return "introduced members are the wrong shape";
	case ValueError::Text: return "member is not terminated text";
	case ValueError::Number: return "member is not one float";
	case ValueError::Type: return "type number names no kind of value";
	case ValueError::Compare: return "comparison is not last in its block";
	case ValueError::Capacity: return "script does not fit the caller's arrays";
	}
	return "unknown";
}

/* Where a read puts what it finds. The caller owns both arrays; this never
 * allocates and never writes past either capacity. */
struct ScriptValues {
	Statement *statements = nullptr;
	int statementCapacity = 0;
	int statementCount = 0;
	Value *values = nullptr;
	int valueCapacity = 0;
	int valueCount = 0;
	/* Why the container refused, when one did. */
	Error container = Error::None;
};

/* Read one script's blocks into statements and values.
 *
 * Every member of every block is accounted for: this refuses rather than skip
 * one it has no rule for, so a script that reads is a script wholly read, as
 * far as the rules reach. */
inline ValueError ReadValues( const void *data, std::size_t bytes, ScriptValues &out ) {
	out.statementCount = 0;
	out.valueCount = 0;
	out.container = Error::None;

	std::uint32_t versionBits = 0;
	std::size_t at = 0;
	out.container = ReadHeader( data, bytes, versionBits, at );
	if ( out.container != Error::None ) return ValueError::Container;

	/* State for the block in hand, so the helpers below stay small. */
	Block block;
	int taken = 0;
	ValueError failure = ValueError::None;

	/* Take the next member of this block, or say why it cannot be taken. */
	const auto next = [&]( Member &member ) -> bool {
		if ( taken >= block.members ) { failure = ValueError::Group; return false; }
		out.container = ReadMember( data, bytes, at, member );
		if ( out.container != Error::None ) { failure = ValueError::Container; return false; }
		++taken;
		return true;
	};
	const auto number = [&]( const Member &member, float &value ) -> bool {
		if ( member.id != MemberNumber ) { failure = ValueError::Group; return false; }
		if ( member.bytes != sizeof( float ) ) { failure = ValueError::Number; return false; }
		std::memcpy( &value, static_cast<const std::uint8_t *>( data ) + member.offset,
			sizeof( value ) );
		return true;
	};
	const auto text = [&]( const Member &member, const char *&value ) -> bool {
		if ( member.id != MemberText ) { failure = ValueError::Group; return false; }
		if ( !TerminatedText( data, member, value ) ) { failure = ValueError::Text; return false; }
		return true;
	};
	/* A type number and the name it types: member 36's whole group, used both
	 * on its own and as the name inside a tag. */
	const auto typedName = [&]( Value &value ) -> bool {
		Member typeMember, nameMember;
		if ( !next( typeMember ) ) return false;
		float tag = 0.0f;
		if ( !number( typeMember, tag ) ) return false;
		const std::int32_t whole = static_cast<std::int32_t>( tag );
		if ( float( whole ) != tag || !IsTypeNumber( whole ) ) {
			failure = ValueError::Type;
			return false;
		}
		if ( !next( nameMember ) || !text( nameMember, value.text ) ) return false;
		value.type = whole;
		return true;
	};

	while ( at < bytes ) {
		out.container = ReadBlock( data, bytes, at, block );
		if ( out.container != Error::None ) return ValueError::Container;

		if ( out.statementCount >= out.statementCapacity ) return ValueError::Capacity;
		Statement &statement = out.statements[out.statementCount];
		statement = Statement{};
		statement.block = block.id;
		statement.flags = block.flags;
		statement.firstValue = out.valueCount;

		taken = 0;
		std::int32_t pendingCompare = 0;
		while ( taken < block.members ) {
			Member member;
			if ( !next( member ) ) return failure;

			if ( member.id >= MemberCompareFirst && member.id <= MemberCompareLast ) {
				/* A comparison takes one value and closes its block, so a
				 * second one, before the first has its value, is wrong. */
				if ( pendingCompare ) return ValueError::Compare;
				pendingCompare = member.id;
				continue;
			}

			if ( out.valueCount >= out.valueCapacity ) return ValueError::Capacity;
			Value &value = out.values[out.valueCount];
			value = Value{};

			switch ( member.id ) {
			case MemberText:
				value.kind = ValueKind::Text;
				if ( !text( member, value.text ) ) return failure;
				break;
			case MemberName:
				value.kind = ValueKind::Name;
				if ( !TerminatedText( data, member, value.text ) ) return ValueError::Text;
				break;
			case MemberNumber:
				value.kind = ValueKind::Number;
				if ( !number( member, value.numbers[0] ) ) return failure;
				break;
			case MemberVector: {
				value.kind = ValueKind::Vector;
				for ( int part = 0; part < 3; ++part ) {
					Member part_;
					if ( !next( part_ ) || !number( part_, value.numbers[part] ) ) return failure;
				}
				break;
			}
			case MemberTyped:
				value.kind = ValueKind::Typed;
				if ( !typedName( value ) ) return failure;
				break;
			case MemberRange: {
				value.kind = ValueKind::Range;
				for ( int part = 0; part < 2; ++part ) {
					Member part_;
					if ( !next( part_ ) || !number( part_, value.numbers[part] ) ) return failure;
				}
				break;
			}
			case MemberTagged: {
				/* A name, given plainly or through a type, then a float. The
				 * name never carries another tag, so this nests once at most
				 * and the value stays one entry. */
				value.kind = ValueKind::Tagged;
				Member carried;
				if ( !next( carried ) ) return failure;
				if ( carried.id == MemberText ) {
					if ( !text( carried, value.text ) ) return failure;
				} else if ( carried.id == MemberTyped ) {
					if ( !typedName( value ) ) return failure;
				} else {
					return ValueError::Group;
				}
				Member trailing;
				if ( !next( trailing ) || !number( trailing, value.numbers[0] ) ) return failure;
				break;
			}
			default:
				return ValueError::Member;
			}

			++out.valueCount;
			++statement.values;

			if ( pendingCompare ) {
				/* The value just read is the one compared against, and the
				 * block ends with it. */
				statement.compare = pendingCompare;
				statement.compareValue = out.valueCount - 1;
				pendingCompare = 0;
				if ( taken != block.members ) return ValueError::Compare;
			}
		}
		if ( pendingCompare ) return ValueError::Compare;
		++out.statementCount;
	}
	return at == bytes ? ValueError::None : ValueError::Container;
}

} // namespace icarus
} // namespace jka
} // namespace fntech3

#endif
