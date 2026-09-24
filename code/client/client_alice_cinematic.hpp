/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_ALICE_CINEMATIC_HPP
#define FNTECH3_CLIENT_ALICE_CINEMATIC_HPP

/* American McGee's Alice's cinematics, as its engine plays one. All retail
 * alice.exe:
 *
 *   cinematic <name> [flags]  0x004276c0  ignored while one plays (cls.state
 *                                         9); otherwise DPrintf
 *                                         "SCR_PlayCinematic( %s )" and
 *                                         CIN_Play( argv 1, argv 2 )
 *   CIN_Play( name, flags )   0x004272e0  also called directly by the engine
 *                                         UI's intro (_intro 0x0040fd69)
 *   CIN_Finish                0x004257a0  the end: from EOF (0x004258ff),
 *                                         from CIN_Stop, and from a bad header
 *   CIN_Stop                  0x00425770
 *
 * Nothing here is a wire or module contract. cl_cin.cpp binds the steps to
 * the engine; they are a template over that binding so that their order and
 * their outcomes can be tested without one (tests/alice_cinematic_tests.py).
 * The header includes nothing of the engine's. */

#include <cstddef>
#include <string>

namespace fntech3::alice::cinematic {

/* The part of retail's 0x229aac-byte cinematic record (kept at 0x00dceee8)
 * that outlives the movie: whether its end resumes the engine UI's intro
 * (+0xb984c) and at which state (+0xb9850). */
struct Record {
	bool resumesIntro = false;
	int introState = 0;
};

/* What the flag argument asks for (CIN_Play 0x004273a2..0x00427415). */
struct PlayFlags {
	bool holdAtEnd = false;
	bool resumesIntro = false;
	int introState = 0;
};

/* The state the intro resumes at when nothing else names one: its main menu
 * (`push 0x64` at 0x0042585e). */
inline constexpr int IntroMainMenuState = 100;

/* The movie's path is formatted into 128 bytes (0x00427330). */
inline constexpr std::size_t PathBytes = 128;

/* Retail's Q_stricmp (0x0044be40) on ASCII. */
inline bool EqualNoCase( const char *a, const char *b ) {
	if ( !a || !b ) {
		return false;
	}
	for ( ;; ++a, ++b ) {
		char x = *a;
		char y = *b;
		if ( x >= 'A' && x <= 'Z' ) x = static_cast<char>( x - 'A' + 'a' );
		if ( y >= 'A' && y <= 'Z' ) y = static_cast<char>( y - 'A' + 'a' );
		if ( x != y ) {
			return false;
		}
		if ( !x ) {
			return true;
		}
	}
}

/* '1' first holds the last frame, as do the names demoend.roq and end.roq,
 * whatever their case (neither ships with Alice). '2' first resumes the intro
 * at the next character less '0', read as a signed char and not checked to be
 * a digit: the intro's logos pass "25" and "26" for states 5 and 6, and "2"
 * alone gives -48. Nothing else means anything, and nothing loops a movie:
 * retail has no loop flag at all. */
inline PlayFlags ParsePlayFlags( const char *name, const char *flags ) {
	PlayFlags parsed;
	parsed.holdAtEnd = ( flags && flags[0] == '1' )
		|| EqualNoCase( name, "demoend.roq" ) || EqualNoCase( name, "end.roq" );
	if ( flags && flags[0] == '2' ) {
		parsed.resumesIntro = true;
		parsed.introState = static_cast<signed char>( flags[1] ) - '0';
	}
	return parsed;
}

/* "video/<name>", cut to the 128-byte buffer it is formatted into. */
inline std::string VideoPath( const char *name ) {
	std::string path = "video/";
	path += name ? name : "";
	if ( path.size() > PathBytes - 1 ) {
		path.resize( PathBytes - 1 );
	}
	return path;
}

/* The movie's sound, an MP3 beside it: its path up to the FIRST '.' -- retail's
 * extension strip (0x0044af70) copies until a '.' or the end -- then ".mp3"
 * (0x0042742a). */
inline std::string SoundPath( const std::string &videoPath ) {
	return videoPath.substr( 0, videoPath.find( '.' ) ) + ".mp3";
}

/* The end's use of `nextmap` and of the record (0x00425808..0x0042586f):
 *
 *   - a nextmap that is set runs as " map <nextmap>" appended to the command
 *     buffer, and is cleared;
 *   - one that begins with ';' ends it there: `map` prints its usage and the
 *     rest runs as commands (the game's ending sets
 *     ";forcemenu main;forcemenu credits");
 *   - otherwise the intro resumes, at the state the flags named, else at 100,
 *     and "_intro" is appended after any map. */
struct FinishAction {
	std::string mapCommand;
	bool continuesIntro = false;
	int introState = 0;
};

inline FinishAction Finish( const char *nextmap, const Record &record ) {
	FinishAction action;
	if ( nextmap && nextmap[0] ) {
		action.mapCommand = " map ";
		action.mapCommand += nextmap;
		action.mapCommand += "\n";
		if ( nextmap[0] == ';' ) {
			return action;
		}
	}
	action.continuesIntro = true;
	action.introState = record.resumesIntro ? record.introState : IntroMainMenuState;
	return action;
}

/*
====================
Play

CIN_Play in retail's order. The host binds each step:

  bool Playing()                   cls.state is CA_CINEMATIC (0x004272eb)
  void StartHunkUsers()            0x004272f8
  void StopAllSounds()             S_StopAllSounds(1), 0x00427303
  int  VideoLength( path )         FS_FOpenFileRead, 0x00427360
  void MissingVideo( name )        DPrintf "play(%s), ROQSize<=0"
  void ShutdownServer()            SV_Shutdown(""), 0x004273df
  bool PreloadSound( path )        0x0042745d; failing prints
  void SoundNotPreloaded( path )     "ERROR: Could not preload mp3 file %s."
  bool Open( path, holdAtEnd )     the header and first chunk, then
                                   cls.state = CA_CINEMATIC (0x0042750d); it
                                   has no loop argument to give
  void BadHeader( name )           0x0042758d
  void Opened( name )              Com_Printf "trFMV::play(), playing %s"
  void ClearSoundBuffer()          0x00427549
  void DecodeFirstFrame()          0x0042754e..0x00427565
  bool PlaySound()                 0x00427569; failing prints
  void SoundNotPlayed()              "ERROR: Could not play preloaded MP3."
====================
*/
template <typename Host>
void Play( Host &host, Record &record, const char *name, const char *flags ) {
	// A cinematic already playing is kept; the new one is not started.
	if ( host.Playing() ) {
		return;
	}
	host.StartHunkUsers();
	host.StopAllSounds();
	// A new, zeroed record (0x00427308..0x00427329).
	record = Record();

	const std::string path = VideoPath( name );
	if ( host.VideoLength( path.c_str() ) <= 0 ) {
		// Nothing else: no server shutdown, no nextmap, no intro step.
		host.MissingVideo( name );
		return;
	}
	const PlayFlags parsed = ParsePlayFlags( name, flags );
	// Playing any cinematic ends a local game; the game's ending relies on it.
	host.ShutdownServer();
	record.resumesIntro = parsed.resumesIntro;
	record.introState = parsed.introState;

	const std::string sound = SoundPath( path );
	if ( !host.PreloadSound( sound.c_str() ) ) {
		host.SoundNotPreloaded( sound.c_str() );
	}
	if ( !host.Open( path.c_str(), parsed.holdAtEnd ) ) {
		/* A bad RoQ header calls the end (0x0042759a), which returns at once
		 * because the record's status is still 0 (0x004257b8): nothing
		 * continues, neither nextmap nor the intro. */
		host.BadHeader( name );
		record = Record();
		return;
	}
	host.Opened( name );
	host.ClearSoundBuffer();
	host.DecodeFirstFrame();
	if ( !host.PlaySound() ) {
		host.SoundNotPlayed();
	}
}

/*
====================
End

CIN_Finish after the movie is closed and the client is back at
CA_DISCONNECTED. The host binds:

  std::string NextMap()            Cvar_VariableString( "nextmap" )
  void Append( text )              EXEC_APPEND / Cbuf_AddText
  void ClearNextMap()              Cvar_Set( "nextmap", "" )
  bool MapLoading()                a map load has already begun
  void SetIntroState( state )      UI_SetIntroState, 0x0040fc50

Retail's `map` and `loadgame` stop a cinematic before they spawn a server
(0x00451118, 0x00451b09, 0x00451c53), so the -1 the load then writes
(UI_BeginLoad 0x0040ed20) replaces the state set here. This engine's do the
same, through CL_AliceStopCinematic (server_alice_save.hpp). Retail never
ends a cinematic while it loads -- its movies run only in state 9 -- so
nothing else takes that branch.
====================
*/
template <typename Host>
void End( Host &host, Record &record ) {
	const std::string nextmap = host.NextMap();
	const FinishAction action = Finish( nextmap.c_str(), record );
	record = Record();
	if ( !action.mapCommand.empty() ) {
		host.Append( action.mapCommand.c_str() );
		host.ClearNextMap();
	}
	if ( !action.continuesIntro ) {
		return;
	}
	if ( !host.MapLoading() ) {
		host.SetIntroState( action.introState );
	}
	host.Append( "_intro\n" );
}

/* CIN_Stop (0x00425770): only while a cinematic plays. A stopped movie does
 * not resume the intro where its flags said; its end sends the intro to the
 * main menu (state 100). Returns whether the movie is to be stopped. */
inline bool Stop( bool playing, Record &record ) {
	if ( !playing ) {
		return false;
	}
	record.resumesIntro = false;
	return true;
}

} // namespace fntech3::alice::cinematic

#endif
