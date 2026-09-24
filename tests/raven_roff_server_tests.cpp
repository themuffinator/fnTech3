/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* The server-side store for Raven's recorded motion paths: what it caches,
 * what it refuses, and which steps come due when. Nothing here moves an
 * entity, because how this engine should move one along a path is not
 * measured yet and the store does not pretend otherwise. */

#include "../code/server/server_raven_roff.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using fntech3::roff::Cached;
using fntech3::roff::Files;
using fntech3::roff::MaxCachedPaths;
using fntech3::roff::Playing;
using fntech3::roff::Store;
namespace format = fntech3::jka::roff;

namespace {

int failures;

void Expect( bool condition, const char *message )
{
	if ( !condition ) {
		std::cerr << "FAIL: " << message << "\n";
		++failures;
	}
}

class TestFiles final : public Files {
public:
	std::map<std::string, std::vector<unsigned char>> files;
	int reads = 0;

	std::size_t Read( const char *path, void *destination, std::size_t capacity ) override
	{
		++reads;
		const auto found = files.find( path ? path : "" );
		if ( found == files.end() ) return 0;
		const std::size_t size = found->second.size();
		if ( destination && capacity >= size && size ) {
			std::memcpy( destination, found->second.data(), size );
		}
		return size;
	}
};

void PutWord( std::vector<unsigned char> &out, std::uint32_t value )
{
	for ( int byte = 0; byte < 4; ++byte ) out.push_back( (unsigned char)( value >> ( byte * 8 ) ) );
}

void PutFloat( std::vector<unsigned char> &out, float value )
{
	std::uint32_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	PutWord( out, bits );
}

/* A version 2 path of `steps` steps at `rate` per second, each step moving one
 * unit further along x than the last and turning one degree more. */
std::vector<unsigned char> MakePath( int steps, int rate, int notes = 0 )
{
	std::vector<unsigned char> out;
	PutWord( out, format::Ident );
	PutWord( out, (std::uint32_t)format::Version2 );
	PutWord( out, (std::uint32_t)steps );
	PutWord( out, (std::uint32_t)rate );
	PutWord( out, (std::uint32_t)notes );
	for ( int step = 0; step < steps; ++step ) {
		PutFloat( out, float( step + 1 ) );
		PutFloat( out, 0.0f );
		PutFloat( out, 0.0f );
		PutFloat( out, float( step + 1 ) );
		PutFloat( out, 0.0f );
		PutFloat( out, 0.0f );
		/* The first step carries every note; the rest carry none. */
		PutWord( out, (std::uint32_t)( step == 0 && notes ? 0 : -1 ) );
		PutWord( out, (std::uint32_t)( step == 0 ? notes : 0 ) );
	}
	for ( int note = 0; note < notes; ++note ) {
		const std::string text = "note" + std::to_string( note );
		out.insert( out.end(), text.begin(), text.end() );
		out.push_back( 0 );
	}
	return out;
}

/* A path is cached by name, once, and only if it reads. */
void CheckPathsCacheByName()
{
	TestFiles files;
	files.files["maps/mp/lift.rof"] = MakePath( 4, 20 );
	Store store( files );
	Expect( store.CachedCount() == 0, "nothing is cached to begin with" );

	const int handle = store.Cache( "maps/mp/lift.rof" );
	Expect( handle == 1, "the first path cached is handle one" );
	const Cached *entry = store.At( handle );
	Expect( entry && entry->path.entries == 4 && entry->path.frameRate == 20,
		"the cached path is the one that was read" );

	const int reads = files.reads;
	Expect( store.Cache( "maps/mp/lift.rof" ) == handle && files.reads == reads,
		"the same path twice keeps its handle and is not read again" );
	Expect( store.CachedCount() == 1, "and is held once" );

	Expect( store.At( 0 ) == nullptr && store.At( 2 ) == nullptr && store.At( -1 ) == nullptr,
		"zero is no path, and so is a handle never given out" );
	Expect( store.Cache( nullptr ) == 0 && store.Cache( "" ) == 0,
		"a path needs a name" );
	Expect( store.Cache( "maps/mp/absent.rof" ) == 0,
		"a path that is not there caches as no path" );

	/* A file that is not a path is refused rather than held as an empty one. */
	files.files["maps/mp/broken.rof"] = { 'N', 'O', 'P', 'E', 1, 0, 0, 0 };
	Expect( store.Cache( "maps/mp/broken.rof" ) == 0 && store.CachedCount() == 1,
		"a file that does not read as a path is not cached" );

	store.Clear();
	Expect( store.CachedCount() == 0 && store.At( 1 ) == nullptr,
		"clearing forgets every path" );
}

/* An entity walks one path at a time. */
void CheckAnEntityWalksOnePath()
{
	TestFiles files;
	files.files["a.rof"] = MakePath( 4, 10 );
	files.files["b.rof"] = MakePath( 2, 10 );
	Store store( files );
	const int first = store.Cache( "a.rof" );
	const int second = store.Cache( "b.rof" );

	Expect( !store.Play( 7, 0, true, 0 ), "there is no playing a path that is not there" );
	Expect( !store.Play( 7, 99, true, 0 ), "nor one never cached" );
	Expect( !store.Play( -1, first, true, 0 ), "nor on an entity that is not one" );
	Expect( store.PlayingCount() == 0, "and none of that started anything" );

	Expect( store.Play( 7, first, true, 1000 ), "an entity starts on a path" );
	Expect( store.PlayingCount() == 1 && store.For( 7 )->handle == first,
		"and is walking the one it was given" );

	Expect( store.Play( 7, second, false, 1000 ), "the same entity may be moved to another" );
	Expect( store.PlayingCount() == 1 && store.For( 7 )->handle == second,
		"which replaces the first rather than doubling it" );
	Expect( store.For( 7 )->translate == false, "and keeps how it was started" );

	Expect( store.Play( 8, first, true, 1000 ) && store.PlayingCount() == 2,
		"another entity walks its own" );
	Expect( store.Stop( 7 ) && store.PlayingCount() == 1 && store.For( 7 ) == nullptr,
		"an entity can be stopped" );
	Expect( !store.Stop( 7 ), "and stopping one twice is no" );

	store.Clear();
	Expect( store.PlayingCount() == 0, "clearing stops everything" );
}

/* Steps come due at the path's own rate, every one of them, and the path ends
 * by running out rather than by running off. */
void CheckStepsComeDueAtTheirRate()
{
	TestFiles files;
	files.files["a.rof"] = MakePath( 3, 10 );        /* 100 ms a step */
	Store store( files );
	const int handle = store.Cache( "a.rof" );
	store.Play( 5, handle, true, 1000 );

	std::vector<float> moved;
	const auto record = [&moved]( const Playing &, const format::Sample &sample,
		const Cached & ) { moved.push_back( sample.move[0] ); };

	Expect( store.Advance( 999, record ) == 0 && moved.empty(),
		"nothing is due before the path starts" );
	Expect( store.Advance( 1000, record ) == 1 && moved.size() == 1 && moved[0] == 1.0f,
		"the first step comes due when it starts" );
	Expect( store.Advance( 1050, record ) == 0, "and the next not before its time" );
	Expect( store.Advance( 1100, record ) == 1 && moved.back() == 2.0f,
		"the second comes due a tenth of a second later" );

	/* A frame that covers more than one step takes all of them: a path that
	 * skipped steps would land its object somewhere it never passed. */
	Expect( store.Advance( 5000, record ) == 1 && moved.size() == 3 && moved.back() == 3.0f,
		"a late frame takes every step that was due" );
	Expect( store.PlayingCount() == 0,
		"and the path stops once it has run out" );
	Expect( store.Advance( 9000, record ) == 0 && moved.size() == 3,
		"a path that has run out takes no more steps" );
}

/* Version 1 has no rate of its own and runs at Raven's base rate. */
void CheckVersionOneRunsAtTheBaseRate()
{
	TestFiles files;
	std::vector<unsigned char> path;
	PutWord( path, format::Ident );
	PutWord( path, (std::uint32_t)format::Version1 );
	PutFloat( path, 2.0f );                       /* its count is a float */
	for ( int step = 0; step < 2; ++step ) {
		for ( int axis = 0; axis < 6; ++axis ) PutFloat( path, float( step + 1 ) );
	}
	files.files["old.rof"] = path;

	Store store( files );
	const int handle = store.Cache( "old.rof" );
	Expect( handle == 1 && store.At( handle )->path.version == format::Version1,
		"a version one path caches" );
	Expect( store.At( handle )->path.frameRate == 0, "carrying no rate of its own" );

	store.Play( 1, handle, true, 0 );
	int taken = 0;
	const auto count = [&taken]( const Playing &, const format::Sample &, const Cached & ) {
		++taken;
	};
	store.Advance( 0, count );
	Expect( taken == 1, "its first step comes due at once" );
	store.Advance( 99, count );
	Expect( taken == 1, "and the next not before a tenth of a second" );
	store.Advance( 100, count );
	Expect( taken == 2, "which is Raven's base rate" );
}

/* The notes a step fires are read out of the file's own track. */
void CheckNotesAreReadFromTheTrack()
{
	TestFiles files;
	files.files["noisy.rof"] = MakePath( 2, 10, 3 );
	Store store( files );
	const int handle = store.Cache( "noisy.rof" );
	const Cached *entry = store.At( handle );
	Expect( entry && entry->path.notes == 3, "the track's notes are counted" );

	format::Sample sample;
	Expect( format::ReadSample( entry->bytes.data(), entry->bytes.size(), entry->path,
		0, sample ) && sample.firstNote == 0 && sample.notes == 3,
		"the first step fires every note" );
	Expect( format::ReadSample( entry->bytes.data(), entry->bytes.size(), entry->path,
		1, sample ) && sample.firstNote == -1 && sample.notes == 0,
		"and the second fires none" );
	Expect( !format::ReadSample( entry->bytes.data(), entry->bytes.size(), entry->path,
		2, sample ), "there is no third step" );

	const char *note = format::Note( entry->bytes.data(), entry->bytes.size(), entry->path, 2 );
	Expect( note && std::strcmp( note, "note2" ) == 0, "a note is read by its number" );
	Expect( format::Note( entry->bytes.data(), entry->bytes.size(), entry->path, 3 ) == nullptr,
		"and a number the track does not have is no note" );
}

/* This engine's own ceiling on how many paths it will hold. */
void CheckTheCacheCeiling()
{
	TestFiles files;
	Store store( files );
	for ( int made = 0; made < MaxCachedPaths; ++made ) {
		const std::string name = "p" + std::to_string( made ) + ".rof";
		files.files[name] = MakePath( 1, 10 );
		if ( store.Cache( name.c_str() ) != made + 1 ) {
			Expect( false, "every path up to the ceiling caches" );
			return;
		}
	}
	files.files["one_too_many.rof"] = MakePath( 1, 10 );
	Expect( store.Cache( "one_too_many.rof" ) == 0,
		"and the one past the ceiling is refused, not made room for" );
	Expect( store.CachedCount() == MaxCachedPaths, "leaving the cache as it was" );
}

} // namespace

int main()
{
	CheckPathsCacheByName();
	CheckAnEntityWalksOnePath();
	CheckStepsComeDueAtTheirRate();
	CheckVersionOneRunsAtTheBaseRate();
	CheckNotesAreReadFromTheTrack();
	CheckTheCacheCeiling();

	if ( failures != 0 ) {
		std::cerr << failures << " Raven motion path checks failed\n";
		return 1;
	}
	std::printf( "Raven motion path checks passed\n" );
	return 0;
}
