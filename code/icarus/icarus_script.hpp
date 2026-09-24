/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_ICARUS_SCRIPT_HPP
#define FNTECH3_ICARUS_SCRIPT_HPP

/* A loaded ICARUS script, and the cache that holds them.
 *
 * The readers below this own the bytes: a value's text points into the file it
 * came from, so a script keeps its own copy for as long as anything can run it.
 *
 * This is also where the block structure is worked out, because this is the
 * first layer that knows what a block number means. Five of them open a body
 * and one closes it, which the value layer deliberately does not know; here
 * each opener is matched to its close once, at load, so running a script never
 * has to search for the end of anything.
 *
 * See docs/fntech3/2026-09-21-jka-icarus.md. */

#include "../compat/jka_icarus_values.hpp"
#include "icarus_host.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace fntech3 {
namespace icarus {

namespace format = fntech3::jka::icarus;
using format::BlockId;
using format::Statement;
using format::Value;

/* The five blocks that open a body, and the one that closes them. Established
 * over two retail installations: every script in both nests exactly this way. */
inline bool OpensBody( std::int32_t block ) {
	return block == format::Number( BlockId::ID_AFFECT )
		|| block == format::Number( BlockId::ID_LOOP )
		|| block == format::Number( BlockId::ID_IF )
		|| block == format::Number( BlockId::ID_ELSE )
		|| block == format::Number( BlockId::ID_TASK );
}

inline bool ClosesBody( std::int32_t block ) {
	return block == format::Number( BlockId::ID_BLOCK_END );
}

enum class LoadError {
	None,
	Missing,      /* the host has no such script */
	TooLarge,     /* the script does not fit the reader's bound */
	Container,    /* the file is not a readable script */
	Values,       /* the members do not group into values */
	Nesting,      /* a body is not closed, or one closes that never opened */
};

inline const char *LoadErrorName( LoadError error ) {
	switch ( error ) {
	case LoadError::None: return "none";
	case LoadError::Missing: return "no such script";
	case LoadError::TooLarge: return "script too large";
	case LoadError::Container: return "malformed script";
	case LoadError::Values: return "script members do not group into values";
	case LoadError::Nesting: return "script bodies do not nest";
	}
	return "unknown";
}

/* The largest script this will read. A retail installation's longest is 27,083
 * bytes; this leaves room for a mod's without admitting a file that is not a
 * script at all. */
inline constexpr std::size_t MaxScriptBytes = 1u << 20;

class Script {
public:
	const std::string &Name() const { return name_; }
	int StatementCount() const { return int( statements_.size() ); }
	const Statement &At( int index ) const { return statements_[std::size_t( index )]; }
	const Value &ValueAt( int index ) const { return values_[std::size_t( index )]; }

	/* The statement after the body this one opens, or -1 when it opens none.
	 * The close itself is inside that span, so a body runs [index+1, end-1). */
	int BodyEnd( int index ) const { return bodyEnd_[std::size_t( index )]; }

	/* A statement's values, as a span the caller can walk. */
	const Value *Values( const Statement &statement ) const {
		return statement.values ? &values_[std::size_t( statement.firstValue )] : nullptr;
	}

	LoadError Load( const std::string &name, const void *data, std::size_t bytes );

private:
	std::string name_;
	std::vector<unsigned char> bytes_;
	std::vector<Statement> statements_;
	std::vector<Value> values_;
	std::vector<int> bodyEnd_;
};

inline LoadError Script::Load( const std::string &name, const void *data, std::size_t bytes )
{
	name_ = name;
	bytes_.assign( static_cast<const unsigned char *>( data ),
		static_cast<const unsigned char *>( data ) + bytes );

	/* Count first, so the arrays are sized once and the values that point into
	 * `bytes_` are never moved afterwards. */
	format::Script counts;
	if ( format::Parse( bytes_.data(), bytes_.size(), counts ) != format::Error::None ) {
		return LoadError::Container;
	}
	statements_.assign( std::size_t( counts.blocks ), Statement{} );
	values_.assign( std::size_t( counts.members ) + 1, Value{} );

	format::ScriptValues view;
	view.statements = statements_.data();
	view.statementCapacity = int( statements_.size() );
	view.values = values_.data();
	view.valueCapacity = int( values_.size() );
	if ( format::ReadValues( bytes_.data(), bytes_.size(), view ) != format::ValueError::None ) {
		return LoadError::Values;
	}
	statements_.resize( std::size_t( view.statementCount ) );
	values_.resize( std::size_t( view.valueCount ) );

	/* Match each opener to its close, once. A body that never closes, or a
	 * close with nothing open, is a script that cannot be run. */
	bodyEnd_.assign( statements_.size(), -1 );
	std::vector<int> open;
	for ( int index = 0; index < int( statements_.size() ); ++index ) {
		const std::int32_t block = statements_[std::size_t( index )].block;
		if ( OpensBody( block ) ) {
			open.push_back( index );
		} else if ( ClosesBody( block ) ) {
			if ( open.empty() ) return LoadError::Nesting;
			bodyEnd_[std::size_t( open.back() )] = index + 1;
			open.pop_back();
		}
	}
	return open.empty() ? LoadError::None : LoadError::Nesting;
}

/* Scripts are loaded once and kept for the level: a script that runs on every
 * door in a map is read from the filesystem one time. */
class ScriptCache {
public:
	void Clear() { scripts_.clear(); }
	std::size_t Count() const { return scripts_.size(); }

	/* Return the script, loading it if this is the first ask. `error` says why
	 * a null came back. A script that failed to load is not retried: the
	 * failure is remembered, so a broken script cannot be reloaded every
	 * frame by a loop that keeps asking for it. */
	const Script *Acquire( Host &host, const std::string &name, LoadError &error );

private:
	struct Entry {
		Script script;
		LoadError error = LoadError::None;
	};
	std::unordered_map<std::string, Entry> scripts_;
};

inline const Script *ScriptCache::Acquire( Host &host, const std::string &name, LoadError &error )
{
	const auto found = scripts_.find( name );
	if ( found != scripts_.end() ) {
		error = found->second.error;
		return error == LoadError::None ? &found->second.script : nullptr;
	}

	std::vector<unsigned char> buffer;
	const std::size_t size = host.ReadScript( name.c_str(), nullptr, 0 );
	Entry entry;
	if ( size == 0 ) {
		entry.error = LoadError::Missing;
	} else if ( size > MaxScriptBytes ) {
		entry.error = LoadError::TooLarge;
	} else {
		buffer.resize( size );
		const std::size_t read = host.ReadScript( name.c_str(), buffer.data(), buffer.size() );
		if ( read == 0 || read > buffer.size() ) entry.error = LoadError::Missing;
		else entry.error = entry.script.Load( name, buffer.data(), read );
	}
	const auto placed = scripts_.emplace( name, std::move( entry ) );
	error = placed.first->second.error;
	return error == LoadError::None ? &placed.first->second.script : nullptr;
}

} // namespace icarus
} // namespace fntech3

#endif
