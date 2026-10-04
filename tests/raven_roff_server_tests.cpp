/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* The path store and Academy's measured server trajectory/callback boundary. */

#include "../code/server/server_raven_roff.hpp"
#include "../code/server/server_jka_roff.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

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

/* A version 2 path of `steps` steps at `rate` milliseconds each, moving one
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

/* Steps come due at the path's own interval, one per update, and the path ends
 * by running out rather than by running off. */
void CheckStepsComeDueAtTheirRate()
{
	TestFiles files;
	files.files["a.rof"] = MakePath( 4, 100 );      /* 100 ms a step */
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

	/* Retail reschedules from now after one sample, even when updates lag. */
	Expect( store.Advance( 5000, record ) == 1 && moved.size() == 3 && moved.back() == 3.0f,
		"a late frame takes one step, not every overdue step" );
	Expect( store.For( 5 ) && store.For( 5 )->nextStepTime == 5100,
		"the next step is scheduled from the current server time" );
	Expect( store.Advance( 5000, record ) == 0 && store.Advance( 5099, record ) == 0,
		"the same frame and the interval before the next frame do not double step" );
	Expect( store.Advance( 5100, record ) == 1 && moved.size() == 4,
		"the last step starts at its new due time" );
	Expect( store.PlayingCount() == 1 && store.Advance( 5199, record ) == 0,
		"the final trajectory keeps playing through its interval" );
	Expect( store.Advance( 5200, record ) == 0 && store.PlayingCount() == 0,
		"and the path ends after the final interval" );
	Expect( store.Advance( 9000, record ) == 0 && moved.size() == 4,
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

class TestMotionHost final : public format::MotionHost {
public:
	std::map<int, fntech3_jka_sharedEntity_t> entities;
	std::vector<std::string> notes;
	std::function<void( int, const char * )> callback;
	fntech3_jka_sharedEntity_t *Entity( int number ) override {
		const auto found = entities.find( number );
		return found == entities.end() ? nullptr : &found->second;
	}
	void Basis( const float angles[3], float columns[3][3] ) override {
		/* These fixtures use yaw only; the production host uses AnglesToAxis. */
		const double radians = angles[1] * 3.14159265358979323846 / 180.0;
		columns[0][0] = float( std::cos( radians ) ); columns[0][1] = float( std::sin( radians ) ); columns[0][2] = 0;
		columns[1][0] = -columns[0][1]; columns[1][1] = columns[0][0]; columns[1][2] = 0;
		columns[2][0] = columns[2][1] = 0; columns[2][2] = 1;
	}
	void Note( int entity, const char *text ) override {
		notes.emplace_back( text );
		if ( callback ) callback( entity, text );
	}
	void Fail( const char *reason ) override { throw std::runtime_error( reason ); }
};

bool Near( float a, float b ) { return std::fabs( a - b ) < 0.001f; }

void CheckServerTrajectoryMotion()
{
	TestFiles files; files.files["scripts/lift.rof"] = MakePath( 3, 50 );
	Store paths( files ); TestMotionHost host; auto &entity = host.entities[7];
	entity.r.currentOrigin[0] = 10; entity.r.currentOrigin[1] = 20; entity.r.currentOrigin[2] = 30;
	entity.r.currentAngles[1] = entity.s.apos.trBase[1] = 90;
	entity.s.pos.trDuration = 1234; entity.s.apos.trDuration = 5678;
	format::ServerPlayer player( paths, host );
	const int handle = paths.Cache( "lift.rof" );
	Expect( handle == 1 && paths.At( handle )->path.frameRate == 50,
		"the retail scripts fallback resolves a missing literal path" );
	Expect( format::FrameMilliseconds( paths.At( handle )->path ) == 50
		&& format::VelocityScale( paths.At( handle )->path ) == 20,
		"the header's 50 is milliseconds, so the velocity scale is twenty" );
	Expect( !player.Play( 8, handle, true, 1000 ) && !player.Play( 7, 99, true, 1000 ),
		"unknown entity and path handles start no motion" );
	Expect( player.Play( 7, handle, true, 1000 ) && entity.r.mIsRoffing == 1,
		"play marks the registered entity as ROFF driven" );
	player.Update( 1000 );
	Expect( entity.s.pos.trType == 2 && entity.s.apos.trType == 2
		&& entity.s.pos.trTime == 1000 && entity.next_roff_time == 1050,
		"the first update writes linear trajectories and the next millisecond time" );
	Expect( Near( entity.s.pos.trDelta[0], 0 ) && Near( entity.s.pos.trDelta[1], 20 )
		&& entity.s.apos.trDelta[0] == 20 && entity.r.currentOrigin[1] == 20,
		"translated offsets become velocities without moving the current origin outright" );
	Expect( entity.s.pos.trBase[1] == 20 && entity.s.apos.trBase[1] == 90
		&& entity.s.pos.trDuration == 1234 && entity.s.apos.trDuration == 5678,
		"trajectories use current bases and preserve durations as retail does" );
	player.Update( 1049 ); Expect( entity.next_roff_time == 1050, "an early update changes nothing" );
	for ( int step = 1; step <= 3; ++step ) {
		/* The game evaluates and links the engine's trajectories between samples. */
		for ( int axis = 0; axis < 3; ++axis ) {
			entity.r.currentOrigin[axis] = entity.s.pos.trBase[axis] + entity.s.pos.trDelta[axis] * .05f;
			entity.r.currentAngles[axis] = entity.s.apos.trBase[axis] + entity.s.apos.trDelta[axis] * .05f;
		}
		player.Update( 1000 + step * 50 );
	}
	Expect( Near( entity.r.currentOrigin[1], 26 ) && entity.s.pos.trType == 0
		&& entity.s.apos.trType == 0 && entity.r.mIsRoffing == 0 && paths.PlayingCount() == 0,
		"completion freezes both trajectories after every sample finishes" );
	Expect( entity.s.pos.trDelta[1] == 0 && Near( entity.s.pos.trBase[1], 26 ),
		"the completed path stops at its evaluated current origin" );
	Expect( player.Play( 7, handle, false, 2000 ), "an untransformed path starts" );
	player.Update( 2000 );
	Expect( entity.s.pos.trDelta[0] == 20 && entity.s.pos.trDelta[1] == 0,
		"translation false means world offsets, not disabled movement" );
	Expect( player.Stop( 7, 2020 ) && !player.Stop( 7, 2020 ) && entity.s.pos.trType == 0
		&& entity.s.pos.trDelta[0] == 0 && entity.r.mIsRoffing == 1,
		"purge freezes motion and preserves the retail shared flag" );
	player.Play( 7, handle, false, 3000 ); player.Update( 3000 ); player.Clear( 3010 );
	Expect( !paths.CachedCount() && !paths.PlayingCount() && entity.s.pos.trType == 0,
		"clean freezes active trajectories and releases paths" );
}

void CheckNotesAndReentrantImports()
{
	TestFiles files; files.files["notes.rof"] = MakePath( 2, 50, 3 );
	Store paths( files ); TestMotionHost host; host.entities[7] = {}; host.entities[8] = {};
	format::ServerPlayer player( paths, host );
	const int handle = paths.Cache( "notes.rof" ); player.Play( 7, handle, false, 100 ); player.Play( 8, handle, false, 100 );
	host.callback = [&]( int entity, const char *text ) {
		if ( entity == 7 && std::strcmp( text, "note0" ) == 0 ) {
			player.Clear( 100 );
			const int replacement = paths.Cache( "notes.rof" );
			player.Play( 7, replacement, false, 100 );
		}
	};
	player.Update( 100 );
	Expect( host.notes == std::vector<std::string>{ "note0", "note1", "note2" },
		"notes survive callback-driven cache clearing, and cleared pending entities do not update" );
	Expect( paths.PlayingCount() == 1 && paths.For( 7 )->step == 0,
		"a replacement playback is not mistaken for the snapshot's previous generation" );
	host.callback = {};
	player.Update( 100 );
	Expect( paths.For( 7 )->step == 1 && host.notes.size() == 6,
		"the replacement begins on the next update and fires its own notes" );
	// The retail callback splits control characters while retaining normal spaces.
	player.Clear( 200 );
	files.files["lines.rof"] = MakePath( 1, 50, 1 );
	auto &bytes = files.files["lines.rof"];
	bytes.resize( format::Version2HeaderBytes + format::Version2EntryBytes );
	const char lines[] = "\n\t sound one \r\nsecond\n";
	bytes.insert( bytes.end(), lines, lines + sizeof( lines ) );
	player.Play( 7, paths.Cache( "lines.rof" ), false, 200 ); player.Update( 200 );
	Expect( host.notes[6] == " sound one " && host.notes[7] == "second",
		"each nonempty note line reaches the module without stripping normal spaces" );
}

void CheckMalformedServerMotion()
{
	TestFiles files; files.files["a.rof"] = MakePath( 1, 50 );
	Store paths( files ); TestMotionHost host; host.entities[7] = {};
	format::ServerPlayer player( paths, host );
	const int handle = paths.Cache( "a.rof" );
	player.Play( 7, handle, false, INT_MAX - 49 );
	const auto before = host.entities[7]; bool refused = false;
	try { player.Update( INT_MAX - 49 ); } catch ( const std::runtime_error & ) { refused = true; }
	Expect( refused && !std::memcmp( &before, &host.entities[7], sizeof( before ) ),
		"overflowing next-frame time is refused before publishing a trajectory" );
	player.Clear( 0 );
	files.files["long.rof"] = MakePath( 1, 50, 1 );
	auto &bytes = files.files["long.rof"];
	bytes.resize( format::Version2HeaderBytes + format::Version2EntryBytes );
	bytes.insert( bytes.end(), 1024, 'x' ); bytes.push_back( 0 );
	player.Play( 7, paths.Cache( "long.rof" ), false, 0 );
	const auto longBefore = host.entities[7]; refused = false;
	try { player.Update( 0 ); } catch ( const std::runtime_error & ) { refused = true; }
	Expect( refused && host.notes.empty() && !std::memcmp( &longBefore, &host.entities[7], sizeof( longBefore ) ),
		"oversized callback lines fail before motion or a partial callback is published" );
	player.Clear( 0 );
	files.files["huge.rof"] = MakePath( 1, 50 );
	const float huge = ( std::numeric_limits<float>::max )();
	std::memcpy( files.files["huge.rof"].data() + format::Version2HeaderBytes, &huge, sizeof( huge ) );
	player.Play( 7, paths.Cache( "huge.rof" ), false, 0 );
	const auto hugeBefore = host.entities[7]; refused = false;
	try { player.Update( 0 ); } catch ( const std::runtime_error & ) { refused = true; }
	Expect( refused && !std::memcmp( &hugeBefore, &host.entities[7], sizeof( hugeBefore ) ),
		"finite offsets whose scaled velocity overflows publish no trajectory" );
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
	CheckServerTrajectoryMotion();
	CheckNotesAndReentrantImports();
	CheckMalformedServerMotion();

	if ( failures != 0 ) {
		std::cerr << failures << " Raven motion path checks failed\n";
		return 1;
	}
	std::printf( "Raven motion path checks passed\n" );
	return 0;
}
