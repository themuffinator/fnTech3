/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's soundtrack manager against the behaviour read out of retail
 * alice.exe (code/client/audio/shared/AliceMusic.h names each routine). Every
 * soundtrack here is written by this test; no retail file is used, and no
 * sample is decoded -- a fake player stands in for the streams, and a fake
 * decoder over synthetic frames stands in for the MP3 library. */

#include "../code/client/audio/shared/AliceMusic.h"
#include "../code/client/audio/codecs/snd_mp3_frames.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::audio::alice;

namespace {

struct Messages : ParseMessages {
	std::vector<std::string> warnings;
	void Warning( const std::string &text ) override { warnings.push_back( text ); }
	bool Said( const char *part ) const {
		for ( const auto &w : warnings ) if ( w.find( part ) != std::string::npos ) return true;
		return false;
	}
};

struct Log : MusicLog {
	std::vector<std::string> lines;
	std::vector<std::string> debugger;
	void Print( const std::string &text ) override { lines.push_back( text ); }
	void Developer( const std::string &text ) override { lines.push_back( text ); }
	void Debugger( const std::string &text ) override { debugger.push_back( text ); }
	bool Said( const char *part ) const {
		for ( const auto &l : lines ) if ( l.find( part ) != std::string::npos ) return true;
		return false;
	}
};

struct FakePlayer : Player {
	struct State {
		bool playing = false;
		int song = -1;
		bool loop = false;
		int remaining = -1;
	} slots[NumPlayers];
	struct StopRecord { int player, song; bool rewind; };
	std::vector<StopRecord> stops;
	std::vector<std::string> started;
	bool refuse = false;

	bool Start( int player, const Song &song, int songIndex, bool loop ) override {
		if ( refuse ) return false;
		slots[player] = State{ true, songIndex, loop, -1 };
		started.push_back( song.file );
		return true;
	}
	void Stop( int player, int songIndex, bool rewind ) override {
		stops.push_back( { player, songIndex, rewind } );
		slots[player].playing = false;
	}
	bool Playing( int player ) const override { return slots[player].playing; }
	int Remaining( int player ) const override { return slots[player].remaining; }
};

constexpr int kSuspense = 3;

bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

/* What Miles holds: a volume truncated to 127 steps. */
float Q( float volume ) {
	return static_cast<float>( static_cast<int>( static_cast<double>( volume ) * 127.0 ) ) / 127.0f;
}

std::vector<std::string> AllTokens( const char *text, bool crossLine ) {
	Tokens tokens( text );
	std::vector<std::string> out;
	for ( std::string t = tokens.Next( crossLine ); !t.empty(); t = tokens.Next( crossLine ) ) out.push_back( t );
	return out;
}

void CheckTokenizer() {
	/* A line break with lines not crossed answers an empty token and stays
	 * for the next call. */
	Tokens lines( "one two\nthree" );
	CHECK( lines.Next( false ) == "one" && lines.Next( false ) == "two" );
	CHECK( lines.Next( false ).empty() );
	CHECK( lines.Next( false ).empty() );
	CHECK( lines.Next( true ) == "three" && lines.Next( true ).empty() );

	/* `/` `*` `/` is a whole comment: the opener's own star closes it. */
	CHECK( AllTokens( "a /*/ b\nc", true ) == std::vector<std::string>( { "a", "c" } ) );
	/* The rest of the line after a block comment is skipped. */
	CHECK( AllTokens( "x /* note */ gone gone\ny", true ) == std::vector<std::string>( { "x", "y" } ) );
	/* A block comment crosses lines even when lines are not crossed. */
	{
		Tokens t( "a /* one\ntwo */ skipped\nb" );
		CHECK( t.Next( false ) == "a" );
		CHECK( t.Next( false ).empty() );
		CHECK( t.Next( true ) == "b" );
	}
	/* Quoted strings turn \" into a quote and \n into a line break. */
	{
		Tokens t( "\"say \\\"hi\\\"\\nnow\" after" );
		CHECK( t.Next( true ) == "say \"hi\"\nnow" );
		CHECK( t.Next( true ) == "after" );
	}
	/* An empty quoted string reads as the end. */
	CHECK( AllTokens( "a \"\" b", true ) == std::vector<std::string>( { "a" } ) );
	/* A word of 1,024 bytes comes back empty; one byte shorter survives. */
	{
		const std::string word( TokenBytes, 'w' );
		const std::string shorter( TokenBytes - 1, 'w' );
		CHECK( Tokens( ( word + " next" ).c_str() ).Next( true ).empty() );
		CHECK( Tokens( ( shorter + " next" ).c_str() ).Next( true ) == shorter );
	}
	/* A quoted string keeps 1,024 bytes and drops the rest. */
	{
		const std::string text = "\"" + std::string( TokenBytes + 10, 'q' ) + "\"";
		CHECK( Tokens( text.c_str() ).Next( true ).size() == TokenBytes );
	}
	/* Bytes from 0x80 up are read as signed, so they are not whitespace. */
	{
		const char text[] = { 'a', '\xa0', 'b', ' ', 'c', '\0' };
		CHECK( AllTokens( text, true ) == std::vector<std::string>( { std::string( "a\xa0" "b" ), "c" } ) );
	}
}

void CheckParsing() {
	Messages m;
	std::vector<Song> songs;
	const char *text =
		"// a soundtrack of the test's own\n"
		"path sound/test/dir\n"
		"normal first.mp3\n"
		"action.mp3\n"
		"/* a block\n comment */ suspense \"quoted name.mp3\"\n"
		"\n"
		"!normal loop\n"
		"!normal fadetime 5\n"
		"!normal volume .6\n"
		"!action interrupt\n"
		"!suspense restart\n"
		"!missing loop\n"
		"!normal sparkle\n";
	CHECK( ParseSoundtrack( text, "sound/test/one.mus", songs, m ) );
	/* Retail skips the rest of the line after a block comment, so the
	 * suspense song on it is never defined. */
	CHECK( songs.size() == 2 );
	/* `path` always gains a slash. */
	CHECK( songs[0].name == "normal" && songs[0].file == "sound/test/dir/first.mp3" );
	CHECK( songs[0].mood == MoodNormal && songs[0].flags == SongLoop );
	CHECK( Near( songs[0].volume, 0.6f ) && Near( songs[0].fadeTime, 5.0f ) );
	/* A bare file is named by the file less its extension. */
	CHECK( songs[1].name == "action" && songs[1].file == "sound/test/dir/action.mp3" && songs[1].mood == MoodAction );
	/* `interrupt` takes the fade away and sets two flags. */
	CHECK( songs[1].flags == ( SongRestart | SongInterrupt ) && songs[1].fadeTime == 0.0f );
	CHECK( m.Said( "song suspense not found, command skipped in sound/test/one.mus." ) );
	CHECK( m.Said( "song missing not found" ) );
	CHECK( m.Said( "unknown command sparkle for song normal" ) );

	/* A quoted file is kept whole. */
	songs.clear();
	CHECK( ParseSoundtrack( "suspense \"quoted name.mp3\"\n", "x", songs, m ) && songs[0].file == "quoted name.mp3" );

	/* A path that already ends in a slash gains another, as retail's does. */
	songs.clear();
	CHECK( ParseSoundtrack( "path a/\nnormal b.mp3\n", "x", songs, m ) && songs[0].file == "a//b.mp3" );

	/* A line's missing arguments are what an earlier line left there: a
	 * bare `path` takes the second token of the line before, and a volume
	 * with no value takes the third. */
	songs.clear();
	CHECK( ParseSoundtrack( "normal left.mp3 stale\npath\naction b.mp3\n!action volume\n", "x", songs, m ) );
	CHECK( songs[1].file == "left.mp3/b.mp3" );
	CHECK( songs[0].file == "left.mp3" );

	songs.clear();
	CHECK( ParseSoundtrack( "normal a.mp3 .25\n!normal volume\n", "x", songs, m ) && Near( songs[0].volume, 0.25f ) );

	/* A name that is not a mood still loads, bound to none. */
	songs.clear();
	CHECK( ParseSoundtrack( "calm c.mp3\n", "x", songs, m ) && songs[0].mood == -1 );

	/* Sixteen songs at most. */
	std::string many;
	for ( int i = 0; i < MaxSongs + 2; ++i ) many += "song" + std::to_string( i ) + " f.mp3\n";
	Messages m2;
	CHECK( ParseSoundtrack( many.c_str(), "big.mus", songs, m2 ) && songs.size() == MaxSongs );
	CHECK( m2.Said( "Too many songs in big.mus, skipping song16" ) );

	/* No songs is a failure, and an argument of sixty-four characters or
	 * more is cut to sixty-three. */
	Messages m3;
	CHECK( !ParseSoundtrack( "path only\n", "empty.mus", songs, m3 ) && songs.empty() );
	CHECK( m3.Said( "could not load empty.mus, no songs defined" ) );
	Messages m4;
	const std::string longName( 70, 'n' );
	CHECK( ParseSoundtrack( ( longName + " f.mp3\n" ).c_str(), "long.mus", songs, m4 ) );
	CHECK( m4.Said( "argument too long, truncating in long.mus" ) && songs[0].name.size() == 31 );

	/* A bare file of four bytes or fewer names nothing. */
	songs.clear();
	CHECK( ParseSoundtrack( "a.mp\n", "x", songs, m ) && songs[0].name.empty() );

	CHECK( MoodIndex( "SUSPENSE" ) == 3 && MoodIndex( "aux7" ) == 15 && MoodIndex( "loud" ) == -1 );
	CHECK( std::string( MoodName( 16 ) ).empty() && std::string( MoodName( -1 ) ).empty() );
}

const char *kTrack =
	"path m\n"
	"normal calm.mp3\n"
	"action fight.mp3\n"
	"suspense creep.mp3\n"
	"!normal loop\n"
	"!normal fadetime 5\n"
	"!normal volume .5\n"
	"!action fadetime 2\n"
	"!suspense loop\n"
	"!suspense interrupt\n";

void CheckMoodsAndCrossfades() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	/* With no mood yet the manager starts on normal. */
	CHECK( music.DesiredMood() == MoodNormal );

	/* The first song starts at its own volume times the music volume, with
	 * no crossfade. `Playing` goes to the debugger alone. */
	music.Update( 1000, player, log, 1.0f, false );
	CHECK( player.started.size() == 1 && player.started[0] == "m/calm.mp3" );
	CHECK( music.SlotAt( 0 ).active && music.SlotAt( 0 ).looping );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.5f ) ) );
	CHECK( log.debugger.size() == 1 && log.debugger[0] == "Playing normal.\n" && !log.Said( "Playing normal" ) );
	music.Update( 1100, player, log, 1.0f, false );
	CHECK( player.started.size() == 1 );

	/* Action crossfades over the new song's two whole seconds: the old
	 * player fades out, the other fades in. */
	music.UpdateMood( MoodAction, MoodNormal, log );
	music.Update( 2000, player, log, 1.0f, false );
	CHECK( player.started.size() == 2 && player.started[1] == "m/fight.mp3" );
	CHECK( music.SlotAt( 0 ).fade == Soundtrack::Fade::Out && music.SlotAt( 1 ).fade == Soundtrack::Fade::In );
	music.Update( 3000, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 1 ).gain, Q( 0.5f ) ) );   /* half of one */
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.25f ) ) );  /* half of 0.5 */
	music.Update( 4001, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 1 ).gain, 1.0f ) && music.SlotAt( 1 ).fade == Soundtrack::Fade::None );
	/* The faded-out song kept its place: normal has no `restart`. */
	CHECK( !player.slots[0].playing && !player.stops.empty() );
	CHECK( player.stops.back().player == 0 && player.stops.back().song == 0 && !player.stops.back().rewind );

	/* Action does not loop: within the fallback's five seconds of its end
	 * the manager hands back to the fallback mood. */
	player.slots[1].remaining = 6000;
	music.Update( 5000, player, log, 1.0f, false );
	CHECK( music.DesiredMood() == MoodAction );
	player.slots[1].remaining = 5000;
	music.Update( 5100, player, log, 1.0f, false );
	/* The same frame switches: the check runs before the mood is served. */
	CHECK( player.started.size() == 3 && player.started[2] == "m/calm.mp3" );
	CHECK( music.SlotAt( 0 ).fade == Soundtrack::Fade::In && music.SlotAt( 1 ).fade == Soundtrack::Fade::Out );
}

void CheckInterruptAndRestore() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	music.UpdateMood( kSuspense, MoodNormal, log );
	music.Update( 0, player, log, 1.0f, false );
	CHECK( player.started.back() == "m/creep.mp3" );

	/* An interrupting song stops the old one at once. */
	music.UpdateMood( MoodNormal, MoodNormal, log );
	music.Update( 100, player, log, 1.0f, false );
	music.UpdateMood( kSuspense, MoodNormal, log );
	const std::size_t stopsBefore = player.stops.size();
	music.Update( 200, player, log, 1.0f, false );
	/* Normal, on the second player, keeps its place; the suspense song still
	 * fading out on the first player is stopped to make room, and rewinds
	 * because it is marked `restart`. */
	CHECK( player.stops.size() == stopsBefore + 2 );
	CHECK( player.stops[stopsBefore].player == 1 && player.stops[stopsBefore].song == 0 && !player.stops[stopsBefore].rewind );
	CHECK( player.stops[stopsBefore + 1].player == 0 && player.stops[stopsBefore + 1].song == 2 && player.stops[stopsBefore + 1].rewind );
	CHECK( music.SlotAt( 0 ).fade == Soundtrack::Fade::None && Near( music.SlotAt( 0 ).gain, 1.0f ) );

	/* Returning from action to the pair before it skips an interrupting
	 * song for the fallback. */
	Soundtrack restore;
	FakePlayer p2;
	CHECK( restore.Load( kTrack, "t.mus", p2, log, m ) );
	restore.UpdateMood( kSuspense, MoodNormal, log );
	restore.UpdateMood( MoodAction, MoodNormal, log );
	restore.UpdateMood( kSuspense, MoodNormal, log );
	CHECK( restore.DesiredMood() == MoodNormal );
	CHECK( log.Said( "restoring music from action state" ) );

	/* None stops everything. */
	music.UpdateMood( MoodNone, MoodNone, log );
	music.Update( 300, player, log, 1.0f, false );
	CHECK( !player.slots[0].playing && !player.slots[1].playing && music.CurrentSong() == -1 );
}

void CheckMasterFade() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	music.Update( 0, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.5f ) ) );

	/* Down to 0.2 over two seconds: retail fades down from one, not from
	 * where the fade began. */
	music.UpdateVolume( 0.2f, 2.0f, 1000 );
	music.Update( 2000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.6f ) );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.3f ) ) );
	music.Update( 3500, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.2f ) );
	/* Repeating the request changes nothing. */
	music.UpdateVolume( 0.2f, 2.0f, 4000 );
	music.Update( 4000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.2f ) );

	/* Back up to one over four seconds, from where it is. */
	music.UpdateVolume( 1.0f, 4.0f, 5000 );
	music.Update( 7000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.6f ) );
	music.Update( 99000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 1.0f ) );

	/* A fade down that begins below one jumps to about one first: from 0.5
	 * to 0.2 over two seconds, 0.85 after one, and 0.2 only at 5,333 ms. */
	music.UpdateVolume( 0.5f, 0.0f, 100000 );
	music.Update( 100000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.5f ) );
	music.UpdateVolume( 0.2f, 2.0f, 200000 );
	music.Update( 201000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.85f ) );
	music.Update( 205300, player, log, 1.0f, false );
	CHECK( music.MasterVolume() > 0.2f );
	music.Update( 205334, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.2f ) );

	/* A zero fade is immediate. */
	music.UpdateVolume( 0.4f, 0.0f, 300000 );
	music.Update( 300000, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.4f ) && Near( music.SlotAt( 0 ).gain, Q( 0.2f ) ) );

	/* A song that starts with no crossfade ignores the master volume until
	 * it or the music volume next changes. */
	music.StopAll( player, log );
	music.Update( 300100, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.5f ) ) );
	music.Update( 300200, player, log, 1.0f, true );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.2f ) ) );
	/* The music volume is part of every volume Miles is handed. */
	music.Update( 300300, player, log, 0.5f, true );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.1f ) ) );
}

/* S_Update's request (0x006a0064): while a `fadesound` fade is on, or when
 * s_volume changes, the players not fading take the master volume at the
 * next update, the song started without a crossfade included. */
void CheckReapply() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	music.UpdateVolume( 0.4f, 0.0f, 0 );
	music.Update( 0, player, log, 1.0f, false );
	CHECK( Near( music.MasterVolume(), 0.4f ) );
	/* The master reached 0.4 and was applied before the song started; the
	 * song, started with no crossfade, ignores it. */
	music.StopAll( player, log );
	music.Update( 100, player, log, 1.0f, false );
	CHECK( player.slots[0].playing && Near( music.SlotAt( 0 ).gain, Q( 0.5f ) ) );
	music.Update( 200, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.5f ) ) );
	/* Asked, the next update gives it the master volume, and only once. */
	music.Reapply();
	music.Update( 300, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 0 ).gain, Q( 0.2f ) ) );

	/* A player in a fade is left to its fade. */
	music.UpdateMood( MoodAction, MoodNormal, log );
	music.Update( 1000, player, log, 1.0f, false );
	CHECK( music.SlotAt( 0 ).fade == Soundtrack::Fade::Out && music.SlotAt( 1 ).fade == Soundtrack::Fade::In );
	music.Reapply();
	music.Update( 1000, player, log, 1.0f, false );
	CHECK( Near( music.SlotAt( 1 ).gain, 0.0f ) && Near( music.SlotAt( 0 ).gain, Q( 0.2f ) ) );

	/* The request is served by the next update, not at once, and two are
	 * one. */
	Soundtrack idle;
	FakePlayer idlePlayer;
	CHECK( idle.Load( kTrack, "t.mus", idlePlayer, log, m ) );
	idle.UpdateVolume( 0.4f, 0.0f, 0 );
	idle.Update( 0, idlePlayer, log, 1.0f, false );
	idle.StopAll( idlePlayer, log );
	idle.Update( 100, idlePlayer, log, 1.0f, false );
	idle.Reapply();
	idle.Reapply();
	CHECK( Near( idle.SlotAt( 0 ).gain, Q( 0.5f ) ) );
	idle.Update( 200, idlePlayer, log, 1.0f, false );
	CHECK( Near( idle.SlotAt( 0 ).gain, Q( 0.2f ) ) );
}

/* `_ftol` (0x004e6d40): chopped to 64 bits, the low 32 kept, and 0 for what
 * 64 bits cannot hold. */
void CheckRetailFtol() {
	CHECK( RetailFtol( 0.0 ) == 0 && RetailFtol( 3.9 ) == 3 && RetailFtol( -3.9 ) == -3 && RetailFtol( -0.5 ) == 0 );
	CHECK( RetailFtol( 2147483647.0 ) == 2147483647 );
	/* Past 32 bits the low word stays. */
	CHECK( RetailFtol( 1e10 ) == 0x540BE400 );
	CHECK( RetailFtol( -1e10 ) == -1410065408 );
	CHECK( RetailFtol( 2147483648.0 ) == INT_MIN && RetailFtol( 4294967295.0 ) == -1 && RetailFtol( 4294967301.0 ) == 5 );
	/* The integer indefinite's low word is 0. */
	CHECK( RetailFtol( std::numeric_limits<double>::quiet_NaN() ) == 0 );
	CHECK( RetailFtol( std::numeric_limits<double>::infinity() ) == 0 );
	CHECK( RetailFtol( -std::numeric_limits<double>::infinity() ) == 0 );
	CHECK( RetailFtol( 9223372036854775808.0 ) == 0 && RetailFtol( 1e30 ) == 0 && RetailFtol( -1e30 ) == 0 );
	/* The last values 64 bits hold. */
	CHECK( RetailFtol( -9223372036854775808.0 ) == 0 );
	CHECK( RetailFtol( 9223372036854774784.0 ) == -1024 );

	/* A song's fade time reaches the crossfade through it. */
	struct Case { const char *fade; int seconds; };
	const Case cases[] = {
		{ "2.9", 2 }, { "-2.9", -2 }, { "1e10", 0x540BE400 }, { "-1e10", -1410065408 },
		{ "1e39", 0 }, { "1e30", 0 }, { "0", 0 }
	};
	for ( const Case &c : cases ) {
		const std::string text = std::string( "normal calm.mp3\naction fight.mp3\n!action fadetime " ) + c.fade + "\n";
		Soundtrack music;
		FakePlayer player;
		Log log;
		Messages m;
		CHECK( music.Load( text.c_str(), "f.mus", player, log, m ) );
		music.Update( 0, player, log, 1.0f, false );
		music.UpdateMood( MoodAction, MoodNormal, log );
		music.PlaySong( "action", 100, player, 1.0f, log );
		CHECK( music.SlotAt( 0 ).fadeSeconds == c.seconds && music.SlotAt( 1 ).fadeSeconds == c.seconds );
	}
}

/* Retail's `V` includes the music volume cvar: with it at zero a fade-out
 * ends at once and a fade-in waits until the cvar rises again. */
void CheckSilentMusicVolume() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	music.Update( 0, player, log, 0.0f, false );
	CHECK( player.slots[0].playing && Near( music.SlotAt( 0 ).gain, 0.0f ) );
	music.UpdateMood( MoodAction, MoodNormal, log );
	music.Update( 100, player, log, 0.0f, false );
	/* The crossfade began and the fade-out ended in the same frame, the old
	 * song keeping its place. */
	CHECK( !player.slots[0].playing && player.stops.back().player == 0 && !player.stops.back().rewind );
	CHECK( player.slots[1].playing && music.SlotAt( 1 ).fade == Soundtrack::Fade::In );
	music.Update( 60000, player, log, 0.0f, false );
	CHECK( music.SlotAt( 1 ).fade == Soundtrack::Fade::In && Near( music.SlotAt( 1 ).gain, 0.0f ) );
	music.Update( 60100, player, log, 1.0f, true );
	CHECK( music.SlotAt( 1 ).fade == Soundtrack::Fade::None && Near( music.SlotAt( 1 ).gain, 1.0f ) );
}

void CheckFailures() {
	Soundtrack music;
	FakePlayer player;
	Log log;
	Messages m;
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	/* A file with no songs leaves no soundtrack behind. */
	CHECK( !music.Load( "path x\n", "none.mus", player, log, m ) && music.Songs().empty() );
	/* A player that cannot set a song up leaves nothing marked playing, and
	 * says so to the developer. */
	CHECK( music.Load( kTrack, "t.mus", player, log, m ) );
	player.refuse = true;
	music.Update( 0, player, log, 1.0f, false );
	CHECK( !music.SlotAt( 0 ).active && !music.SlotAt( 1 ).active );
	CHECK( log.Said( "Could not setup mp3 file!" ) );
	/* Freeing drops the moods and keeps the table, which playsong can
	 * still find songs in. */
	player.refuse = false;
	music.FreeAll( player, log );
	CHECK( music.DesiredMood() == MoodNone && music.Songs().size() == 3 );
	CHECK( music.PlaySong( "action", 1000, player, 1.0f, log ) && player.slots[0].playing );
	/* A song name that is not in the table is quietly ignored. */
	CHECK( music.PlaySong( "nothing", 1000, player, 1.0f, log ) );
	/* Retail's own shutdown forgets only the current song. */
	music.DetachPlayers();
	CHECK( music.CurrentSong() == -1 && music.Songs().size() == 3 );
}

/* ---- the byte offset every song starts and loops at -------------------- */

/* Frames at known offsets with a one-byte marker in their samples. The fake
 * decoder resyncs as minimp3 does: asked at any byte, it finds the first
 * frame that starts there or later. */
struct FakeFrames {
	std::vector<int> starts;
	int bytes = 418;
	const std::uint8_t *base = nullptr;

	static int Decode( void *self, const std::uint8_t *data, int available, short *pcm,
		int *frameBytes, int *frameOffset, int *channels, int *rate ) {
		auto *fake = static_cast<FakeFrames *>( self );
		*frameBytes = *frameOffset = *channels = *rate = 0;
		const int position = static_cast<int>( data - fake->base );
		for ( std::size_t i = 0; i < fake->starts.size(); ++i ) {
			const int start = fake->starts[i];
			if ( start < position ) continue;
			const int skip = start - position;
			if ( skip + fake->bytes > available ) return 0;
			*frameOffset = skip;
			*frameBytes = skip + fake->bytes;
			*channels = 1;
			*rate = 44100;
			if ( pcm ) {
				for ( int s = 0; s < 1152; ++s ) pcm[s] = static_cast<short>( i );
			}
			return 1152;
		}
		return 0;
	}
};

void CheckStartOffset() {
	/* floor(44,100 x 0.063): Miles's granularity for an ASI stream is 1. */
	CHECK( StartOffsetBytes( 44100 ) == 2778 );
	CHECK( StartOffsetBytes( 22050 ) == 1389 );
	CHECK( StartOffsetBytes( 0 ) == 0 );

	/* Twenty 418-byte frames after a 10-byte ID3v2 tag. */
	FakeFrames fake;
	std::vector<std::uint8_t> image( 10 + 20 * 418, 0 );
	image[0] = 'I'; image[1] = 'D'; image[2] = '3';
	for ( int i = 0; i < 20; ++i ) fake.starts.push_back( 10 + i * 418 );
	fake.base = image.data();
	fntech3::audio::Mp3FrameDecoder decoder;
	decoder.Reset( image.data(), static_cast<int>( image.size() ) );
	CHECK( decoder.StartOffset() == 10 );
	/* The offset is into the file, tag included: frames 0 to 6 begin
	 * before byte 2,778, so a start there passes seven frames. */
	CHECK( decoder.SamplesBefore( &fake, &FakeFrames::Decode, 2778 ) == 7 * 1152 );
	decoder.SeekFileOffset( 2778 );
	short out[1152];
	CHECK( decoder.Read( &fake, &FakeFrames::Decode, 1, 44100, 1, out, sizeof( out ) ) == static_cast<int>( sizeof( out ) ) );
	CHECK( out[0] == 7 );
	/* A loop goes back to the same byte, and to the same frame. */
	decoder.Read( &fake, &FakeFrames::Decode, 1, 44100, 1, out, sizeof( out ) );
	CHECK( out[0] == 8 );
	decoder.SeekFileOffset( 2778 );
	decoder.Read( &fake, &FakeFrames::Decode, 1, 44100, 1, out, sizeof( out ) );
	CHECK( out[0] == 7 );
	/* An offset inside the tag starts at the tag's end, and one past the end
	 * yields nothing. */
	decoder.SeekFileOffset( 3 );
	decoder.Read( &fake, &FakeFrames::Decode, 1, 44100, 1, out, sizeof( out ) );
	CHECK( out[0] == 0 );
	decoder.SeekFileOffset( 1 << 30 );
	CHECK( decoder.Read( &fake, &FakeFrames::Decode, 1, 44100, 1, out, sizeof( out ) ) == 0 );
}

} // namespace

int main() {
	CheckTokenizer();
	CheckParsing();
	CheckMoodsAndCrossfades();
	CheckInterruptAndRestore();
	CheckMasterFade();
	CheckReapply();
	CheckRetailFtol();
	CheckSilentMusicVolume();
	CheckFailures();
	CheckStartOffset();
	std::cout << "alice music tests passed\n";
	return 0;
}
