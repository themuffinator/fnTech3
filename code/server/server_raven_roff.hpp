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
	int nextStepTime = 0;    /* when to take it */
	bool translate = true;   /* whether the move part is applied */
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
			? &cached_[std::size_t( handle - 1 )] : nullptr;
	}

	/* Read a path and remember it by name, returning its handle or 0. A name
	 * cached twice keeps its first handle, because a level asks for the same
	 * path on many entities. */
	int Cache( const char *name );

	/* Start a path on an entity. False when there is no such path; an entity
	 * already walking one is moved onto the new one rather than doubled. */
	bool Play( int entity, int handle, bool translate, int now );

	/* Stop whatever this entity was walking. */
	bool Stop( int entity );

	const Playing *For( int entity ) const
	{
		for ( const Playing &one : playing_ ) if ( one.entity == entity ) return &one;
		return nullptr;
	}

	/* Take every step that is due. `step` is handed each one in turn, with the
	 * entity it belongs to and whether the move part applies; a path that has
	 * run out is dropped. Returns how many steps were taken. */
	template<class Step>
	int Advance( int now, Step step );

private:
	Files *files_ = nullptr;
	std::vector<Cached> cached_;
	std::vector<Playing> playing_;
};

inline int Store::Cache( const char *name )
{
	if ( !name || !name[0] ) return 0;
	for ( int handle = 1; handle <= CachedCount(); ++handle ) {
		if ( cached_[std::size_t( handle - 1 )].name == name ) return handle;
	}
	if ( CachedCount() >= MaxCachedPaths ) return 0;

	const std::size_t size = files_->Read( name, nullptr, 0 );
	if ( size == 0 || size > MaxPathBytes ) return 0;
	Cached entry;
	entry.bytes.assign( size, 0 );
	if ( files_->Read( name, entry.bytes.data(), entry.bytes.size() ) != size ) return 0;
	if ( format::Parse( entry.bytes.data(), entry.bytes.size(), entry.path )
		!= format::Error::None ) {
		return 0;
	}
	entry.name = name;
	cached_.push_back( std::move( entry ) );
	return CachedCount();
}

inline bool Store::Play( int entity, int handle, bool translate, int now )
{
	const Cached *entry = At( handle );
	if ( !entry || entity < 0 ) return false;
	Stop( entity );
	Playing one;
	one.entity = entity;
	one.handle = handle;
	one.step = 0;
	one.translate = translate;
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
	int taken = 0;
	for ( std::size_t at = 0; at < playing_.size(); ) {
		Playing &one = playing_[at];
		const Cached *entry = At( one.handle );
		if ( !entry ) { playing_.erase( playing_.begin() + at ); continue; }
		const int rate = entry->path.frameRate > 0
			? entry->path.frameRate : format::SampleRate;
		const int interval = rate > 0 ? 1000 / rate : 100;

		bool finished = false;
		/* A frame may cover more than one step when the server is behind, and
		 * every step is taken rather than skipped: a path that skipped steps
		 * would land its object somewhere it never passed through. */
		while ( !finished && now >= one.nextStepTime ) {
			format::Sample sample;
			if ( !format::ReadSample( entry->bytes.data(), entry->bytes.size(),
				entry->path, one.step, sample ) ) {
				finished = true;
				break;
			}
			step( one, sample, *entry );
			++taken;
			++one.step;
			one.nextStepTime += interval > 0 ? interval : 1;
		}
		if ( finished ) playing_.erase( playing_.begin() + at );
		else ++at;
	}
	return taken;
}

} // namespace roff
} // namespace fntech3

#endif
