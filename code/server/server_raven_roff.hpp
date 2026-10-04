/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RAVEN_ROFF_HPP
#define FNTECH3_SERVER_RAVEN_ROFF_HPP

/* The recorded motion paths a Raven game asks the server to hold and play.
 *
 * A `.rof` file is a list of steps: how far an object moves and turns during
 * each one, and which notes fire when it is reached. The game caches a file by
 * name, then asks for it to be played on an entity, and once a frame asks the
 * engine to move everything that is playing.
 *
 * This holds the cache and the list of what is playing. Files are read through
 * the same bounded reader that swept both installations, so a path that would
 * not read there does not cache here either, and a file that does not cache is
 * reported as not cached rather than remembered as empty.
 *
 * See docs/fntech3/2026-09-21-jka-roff.md. */

#include "../compat/jka_roff_format.hpp"

#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fntech3 {
namespace roff {

namespace format = fntech3::jka::roff;

/* This engine's own bounds, not a claim about any game's. */
inline constexpr std::size_t MaxPathBytes = 4u * 1024u * 1024u;
inline constexpr int MaxCachedPaths = 256;

/* What the engine must be able to do to read a path. A test supplies its own
 * files; the server reads the installation's. */
class Files {
public:
	virtual ~Files() = default;
	virtual std::size_t Read( const char *path, void *destination,
		std::size_t capacity ) = 0;
};

struct Cached {
	std::string name;
	format::Path path;
	std::vector<unsigned char> bytes;
};

/* One entity walking one path. */
struct Playing {
	int entity = -1;
	int handle = 0;          /* one-based, into the cache */
	int step = 0;            /* the next step to take */
	std::int64_t nextStepTime = 0; /* when to take it, without signed overflow */
	bool translate = true;   /* rotate motion into the entity's starting axes */
	float startAngles[3] = {};
	std::uint64_t generation = 0;
};

class Store {
public:
	explicit Store( Files &files ) : files_( &files ) {}

	void Clear()
	{
		cached_.clear();
		playing_.clear();
	}

	int CachedCount() const { return int( cached_.size() ); }
	int PlayingCount() const { return int( playing_.size() ); }

	/* Handles are one-based so that zero stays "no path". */
	const Cached *At( int handle ) const
	{
		return handle >= 1 && handle <= CachedCount()
			? cached_[std::size_t( handle - 1 )].get() : nullptr;
	}

	/* Read a path and remember it by name, returning its handle or 0. A name
	 * cached twice keeps its first handle, because a level asks for the same
	 * path on many entities. */
	int Cache( const char *name );

	/* Start a path on an entity. False when there is no such path; an entity
	 * already walking one is moved onto the new one rather than doubled. */
	bool Play( int entity, int handle, bool translate, int now,
		const float startAngles[3] = nullptr );

	/* Stop whatever this entity was walking. */
	bool Stop( int entity );

	const Playing *For( int entity ) const
	{
		for ( const Playing &one : playing_ ) if ( one.entity == entity ) return &one;
		return nullptr;
	}
	const Playing *PlayingAt( int index ) const
	{
		return index >= 0 && index < PlayingCount() ? &playing_[std::size_t( index )] : nullptr;
	}

	/* Retail takes one due sample per update, then schedules the next from the
	 * current server time. It does not catch up by taking all overdue samples.
	 * After the final sample's interval, finish freezes its entity's trajectory.
	 * Callbacks may stop or replace playback, clear the cache or start another
	 * entity; snapshot generations and retained paths protect that reentrancy. */
	template<class Step>
	int Advance( int now, Step step );
	template<class Step, class Finish>
	int Advance( int now, Step step, Finish finish );

private:
	Files *files_ = nullptr;
	std::vector<std::shared_ptr<Cached>> cached_;
	std::vector<Playing> playing_;
	std::uint64_t generation_ = 0;
};

inline int Store::Cache( const char *name )
{
	if ( !name || !name[0] ) return 0;
	for ( int handle = 1; handle <= CachedCount(); ++handle ) {
		if ( cached_[std::size_t( handle - 1 )]->name == name ) return handle;
	}
	if ( CachedCount() >= MaxCachedPaths ) return 0;

	std::string readName( name );
	std::size_t size = files_->Read( readName.c_str(), nullptr, 0 );
	/* Retail tries the literal path, then scripts/<name without extension>.rof
	 * (jamp.exe VA 0x4265a8-0x4265fb). Keep the caller's name as the cache key. */
	if ( size == 0 ) {
		const auto extension = readName.find( '.' );
		if ( extension != std::string::npos ) readName.resize( extension );
		readName = "scripts/" + readName + ".rof";
		size = files_->Read( readName.c_str(), nullptr, 0 );
	}
	if ( size == 0 || size > MaxPathBytes ) return 0;
	Cached entry;
	entry.bytes.assign( size, 0 );
	if ( files_->Read( readName.c_str(), entry.bytes.data(), entry.bytes.size() ) != size ) return 0;
	if ( format::Parse( entry.bytes.data(), entry.bytes.size(), entry.path )
		!= format::Error::None ) {
		return 0;
	}
	entry.name = name;
	cached_.push_back( std::make_shared<Cached>( std::move( entry ) ) );
	return CachedCount();
}

inline bool Store::Play( int entity, int handle, bool translate, int now,
	const float startAngles[3] )
{
	const Cached *entry = At( handle );
	if ( !entry || entity < 0 ) return false;
	Stop( entity );
	Playing one;
	one.entity = entity;
	one.handle = handle;
	one.step = 0;
	one.translate = translate;
	if ( startAngles ) std::memcpy( one.startAngles, startAngles, sizeof( one.startAngles ) );
	one.generation = ++generation_;
	/* Version 2 carries its own rate; version 1 runs at Raven's base rate. */
	one.nextStepTime = now;
	playing_.push_back( one );
	return true;
}

inline bool Store::Stop( int entity )
{
	for ( auto it = playing_.begin(); it != playing_.end(); ++it ) {
		if ( it->entity == entity ) { playing_.erase( it ); return true; }
	}
	return false;
}

template<class Step>
int Store::Advance( int now, Step step )
{
	return Advance( now, step, []( const Playing &, const Cached & ) {} );
}

template<class Step, class Finish>
int Store::Advance( int now, Step step, Finish finish )
{
	int taken = 0;
	const std::vector<Playing> pending = playing_;
	for ( const Playing &previous : pending ) {
		auto current = playing_.begin();
		for ( ; current != playing_.end(); ++current ) {
			if ( current->entity == previous.entity
				&& current->generation == previous.generation ) break;
		}
		if ( current == playing_.end() || now < current->nextStepTime ) continue;
		const Playing one = *current;
		if ( !At( one.handle ) ) { playing_.erase( current ); continue; }
		const auto entry = cached_[std::size_t( one.handle - 1 )];
		format::Sample sample;
		if ( !format::ReadSample( entry->bytes.data(), entry->bytes.size(),
			entry->path, one.step, sample ) ) {
			playing_.erase( current );
			finish( one, *entry );
			continue;
		}
		++current->step;
		current->nextStepTime = std::int64_t( now ) + format::FrameMilliseconds( entry->path );
		step( one, sample, *entry );
		++taken;
	}
	return taken;
}

} // namespace roff
} // namespace fntech3

#endif
