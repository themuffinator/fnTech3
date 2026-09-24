/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored from retail American McGee's Alice observations. */
#ifndef FNTECH3_AUDIO_ALICE_MUSIC_H
#define FNTECH3_AUDIO_ALICE_MUSIC_H

/*
 * American McGee's Alice's soundtrack manager, as retail alice.exe runs it.
 *
 * A level names a soundtrack, a `.mus` file listing up to sixteen songs, each
 * bound to a mood by its name. The server's player state carries a current
 * and a fallback mood and a music volume with a fade time; the client game
 * hands them to the engine on its first snapshot and whenever they change,
 * and the engine plays the song whose name is the mood, crossfading between
 * two players and returning to the fallback when a song that does not loop
 * runs out.
 *
 *   MUSIC_LoadSoundtrackFile  0x0044eef0   MUSIC_UpdateMood      0x0044f860
 *   MUSIC_Update              0x0044f5b0   MUSIC_UpdateVolume    0x0044f980
 *   MUSIC_PlaySong            0x00471fc0   MUSIC_StopSong        0x00471f20
 *   MUSIC_UpdateSongVolumes   0x00472210   MUSIC_CheckSongEnds   0x00472400
 *   MUSIC_StopAllSongs        0x0044fa10   MUSIC_FreeAllSongs    0x0044fa40
 *
 * Nothing here reads a file or decodes a sample: the manager decides what
 * plays, where each player is in its fade and how loud each is, and a
 * `Player` supplied by the caller does the streaming. See
 * docs/fntech3/2026-09-23-alice-audio.md.
 */

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace fntech3::audio::alice {

inline constexpr int MaxSongs = 16;
inline constexpr int NumMoods = 16;
inline constexpr int NumPlayers = 2;
/* The parser's per-line argument array, sixteen of sixty-four bytes. */
inline constexpr int MaxLineArguments = 16;
inline constexpr std::size_t ArgumentBytes = 64;
/* The tokenizer's own buffer (com_token, 0x00b9b660). */
inline constexpr std::size_t TokenBytes = 1024;
/* Every song starts, and loops, this many seconds of bytes into its file at
 * the file's own rate: floor(rate * 0.063) compressed bytes (the double at
 * 0x005002c8, used at 0x00472113). */
inline constexpr double StartOffsetSeconds = 0.063;

inline int StartOffsetBytes( int rate ) {
	return rate > 0 ? static_cast<int>( static_cast<double>( rate ) * StartOffsetSeconds ) : 0;
}

/* The CRT's `_ftol` (0x004e6d40), which alice.exe converts every float to an
 * integer with: the control word's rounding set to chop, `fistp qword`, and
 * the low 32 bits of the 64-bit result kept (`eax`). A finite value that fits
 * 64 bits keeps its low word, so 1e10 is 0x540BE400; NaN, an infinity or a
 * value past the 64-bit range stores the integer indefinite,
 * 0x8000000000000000, whose low word is 0. A plain cast would be undefined
 * for all of those. */
inline int RetailFtol( double value ) {
	constexpr double kTwo63 = 9223372036854775808.0;
	if ( !( value >= -kTwo63 && value < kTwo63 ) ) return 0;
	const std::uint32_t low = static_cast<std::uint32_t>( static_cast<std::uint64_t>( static_cast<std::int64_t>( value ) ) );
	return low <= 0x7fffffffu ? static_cast<int>( low ) : static_cast<int>( low - 0x80000000u ) - 0x7fffffff - 1;
}

/* The mood names, sixteen bytes apart at 0x00519100; a song's name picks its
 * mood. */
inline constexpr const char *MoodNames[NumMoods] = {
	"none", "normal", "action", "suspense", "mystery", "success", "failure",
	"surprise", "special", "aux1", "aux2", "aux3", "aux4", "aux5", "aux6", "aux7"
};
inline constexpr int MoodNone = 0;
inline constexpr int MoodNormal = 1;
inline constexpr int MoodAction = 2;

/* Every name compare in the music and sound code ignores ASCII case (the CRT
 * _stricmp at 0x004e8760 and Q_stricmp at 0x0044be40). */
inline int CompareNoCase( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		int ca = static_cast<unsigned char>( *a ), cb = static_cast<unsigned char>( *b );
		if ( ca >= 'A' && ca <= 'Z' ) ca += 'a' - 'A';
		if ( cb >= 'A' && cb <= 'Z' ) cb += 'a' - 'A';
		if ( ca != cb ) return ca < cb ? -1 : 1;
		if ( !ca ) return 0;
	}
}

/* 0x0044ad40: the mood a name spells, or -1. */
inline int MoodIndex( const char *name ) {
	if ( !name ) return -1;
	for ( int i = 0; i < NumMoods; ++i ) {
		if ( !CompareNoCase( name, MoodNames[i] ) ) return i;
	}
	return -1;
}

/* 0x0044ada0: a mood's name, or an empty one out of range. */
inline const char *MoodName( int mood ) {
	return mood >= 0 && mood < NumMoods ? MoodNames[mood] : "";
}

enum SongFlags : unsigned {
	SongLoop = 1,
	SongRestart = 2,
	/* `interrupt` sets this with `restart`, and takes the fade away. */
	SongInterrupt = 4
};

struct Song {
	std::string name;
	std::string file;
	int mood = -1;
	unsigned flags = 0;
	float volume = 1.0f;
	float fadeTime = 1.0f;
};

/* 0x0044b640, the tokenizer Ritual's engine reads soundtracks with. It is not
 * Quake III's COM_ParseExt:
 *
 *   - whitespace is a byte from 0 to 32 read as a signed char, so bytes of
 *     0x80 and above belong to tokens;
 *   - a line break with line crossing off answers an empty token and is left
 *     for the next call;
 *   - `//` runs to the line break; `/` `*` runs to the first `/` whose previous
 *     byte is `*`, counting the opener's own `*`, so `/` `*` `/` is a whole
 *     comment; it crosses lines whatever is asked, and the rest of the line
 *     after it is skipped too;
 *   - a quoted string turns `\"` into `"` and `\n` into a line break, and
 *     keeps at most 1,024 bytes;
 *   - a word of 1,024 bytes or more comes back empty.
 *
 * An empty token ends a line or, where lines are crossed, the text. Retail
 * steps past the terminator of a string that runs off the end of the text;
 * this one stops there. */
class Tokens {
public:
	explicit Tokens( const char *text ) : p_( text ) {}

	std::string Next( bool crossLine ) {
		std::string token;
		if ( !p_ ) return token;
		for ( ;; ) {
			while ( White( *p_ ) ) {
				if ( *p_ == '\n' && !crossLine ) return token;
				if ( *p_ == '\0' ) {
					p_ = nullptr;
					return token;
				}
				++p_;
			}
			if ( p_[0] == '/' && p_[1] == '/' ) {
				while ( *p_ && *p_ != '\n' ) ++p_;
				continue;
			}
			if ( p_[0] == '/' && p_[1] == '*' ) {
				++p_;
				while ( *p_ && !( p_[-1] == '*' && p_[0] == '/' ) ) ++p_;
				while ( *p_ && *p_ != '\n' ) ++p_;
				continue;
			}
			break;
		}
		if ( *p_ == '"' ) {
			++p_;
			for ( ;; ) {
				const char c = *p_;
				if ( c == '\0' ) return token;
				++p_;
				if ( c == '\\' && *p_ == '"' ) {
					if ( token.size() < TokenBytes ) token += '"';
					++p_;
					continue;
				}
				if ( c == '"' ) return token;
				if ( token.size() < TokenBytes ) {
					if ( c == '\\' && *p_ == 'n' ) {
						token += '\n';
						++p_;
					} else {
						token += c;
					}
				}
			}
		}
		do {
			if ( token.size() < TokenBytes ) token += *p_;
			++p_;
		} while ( !White( *p_ ) );
		if ( token.size() == TokenBytes ) token.clear();
		return token;
	}

private:
	static bool White( char c ) {
		const signed char s = static_cast<signed char>( c );
		return s >= 0 && s <= ' ';
	}

	const char *p_;
};

/* What the parser says about a file it reads; retail prints each with
 * Com_Printf, uncoloured. */
struct ParseMessages {
	virtual ~ParseMessages() = default;
	virtual void Warning( const std::string &text ) = 0;
};

/* 0x0044eef0 after the file is read: `path <dir>`, then songs as `<name>
 * <file>` or a bare `<file>` named by the file less its four-character
 * extension, then `!<song> <command>` lines setting `volume`, `fadetime`,
 * `loop`, `restart` or `interrupt`. Returns false when no song is defined.
 *
 * Retail copies each line's tokens into one array of sixteen that lives for
 * the whole file, so an argument a line leaves out is whatever an earlier
 * line put in that place: a bare `path` takes the previous line's second
 * token. That array starts out as uninitialised stack; here it starts empty.
 * Retail also writes a seventeenth token past the array and copies names and
 * paths past their fields; these are bounded instead, the extra tokens
 * ignored, a name kept to 31 bytes and a path to 63. */
inline bool ParseSoundtrack( const char *text, const char *fileName, std::vector<Song> &songs,
	ParseMessages &messages ) {
	songs.clear();
	std::string path;
	Tokens tokens( text ? text : "" );
	const std::string file = fileName ? fileName : "";
	std::array<std::string, MaxLineArguments> args;
	for ( std::string token = tokens.Next( true ); !token.empty(); token = tokens.Next( true ) ) {
		int count = 0;
		do {
			if ( token.size() >= ArgumentBytes ) {
				messages.Warning( "MUSIC_LoadSoundtrackFile: argument too long, truncating in " + file + "\n" );
				token.resize( ArgumentBytes - 1 );
			}
			if ( count < MaxLineArguments ) args[static_cast<std::size_t>( count )] = token;
			++count;
			token = tokens.Next( false );
		} while ( !token.empty() );

		if ( !CompareNoCase( args[0].c_str(), "path" ) ) {
			/* Retail tests the last character against '/' and '\\' with the
			 * wrong conjunction, so a slash is appended every time. */
			path = args[1] + "/";
			continue;
		}
		if ( args[0][0] == '!' ) {
			const std::string name = args[0].substr( 1 );
			Song *song = nullptr;
			for ( Song &s : songs ) {
				if ( !CompareNoCase( s.name.c_str(), name.c_str() ) ) { song = &s; break; }
			}
			if ( !song ) {
				messages.Warning( "MUSIC_LoadSoundtrackFile: song " + name + " not found, command skipped in " + file + ".\n" );
				continue;
			}
			const std::string &command = args[1];
			if ( !CompareNoCase( command.c_str(), "volume" ) ) {
				song->volume = static_cast<float>( std::atof( args[2].c_str() ) );
			} else if ( !CompareNoCase( command.c_str(), "fadetime" ) ) {
				song->fadeTime = static_cast<float>( std::atof( args[2].c_str() ) );
			} else if ( !CompareNoCase( command.c_str(), "loop" ) ) {
				song->flags |= SongLoop;
			} else if ( !CompareNoCase( command.c_str(), "restart" ) ) {
				song->flags |= SongRestart;
			} else if ( !CompareNoCase( command.c_str(), "interrupt" ) ) {
				song->fadeTime = 0.0f;
				song->flags |= SongRestart | SongInterrupt;
			} else {
				messages.Warning( "MUSIC_LoadSoundtrackFile: unknown command " + command + " for song " + name + " in " + file + ".\n" );
			}
			continue;
		}

		Song song;
		if ( count > 1 ) {
			song.name = args[0];
			song.file = path + args[1];
		} else {
			song.file = path + args[0];
			/* A bare file shorter than four bytes makes retail copy a negative
			 * count; here it names nothing. */
			song.name = args[0].size() > 4 ? args[0].substr( 0, args[0].size() - 4 ) : std::string();
		}
		if ( static_cast<int>( songs.size() ) >= MaxSongs ) {
			messages.Warning( "MUSIC_LoadSoundtrackFile: Too many songs in " + file + ", skipping " + song.name + ".\n" );
			continue;
		}
		/* The record holds a 32-byte name and a 64-byte path. */
		if ( song.name.size() > 31 ) song.name.resize( 31 );
		if ( song.file.size() > 63 ) song.file.resize( 63 );
		song.mood = MoodIndex( song.name.c_str() );
		songs.push_back( song );
	}
	if ( songs.empty() ) {
		messages.Warning( "MUSIC_LoadSoundtrackFile: could not load " + file + ", no songs defined.\n" );
		return false;
	}
	return true;
}

/* What streams a song. `Start` opens or resumes a song on a player and says
 * whether it could; `Stop` ends a player and, unless `rewind`, keeps the
 * song's place for the next time it is started; `Playing` is true while a
 * player still has audio; `Remaining` is how many milliseconds of the song
 * are left, or a negative number when that is unknown. */
struct Player {
	virtual ~Player() = default;
	virtual bool Start( int player, const Song &song, int songIndex, bool loop ) = 0;
	virtual void Stop( int player, int songIndex, bool rewind ) = 0;
	virtual bool Playing( int player ) const = 0;
	virtual int Remaining( int player ) const = 0;
};

/* Retail's three printers: Com_Printf, Com_DPrintf, and the developer-only
 * one at 0x00438a90 that writes to the debugger and never to the console. */
struct MusicLog {
	virtual ~MusicLog() = default;
	virtual void Print( const std::string &text ) = 0;
	virtual void Developer( const std::string &text ) = 0;
	virtual void Debugger( const std::string &text ) { (void)text; }
};

class Soundtrack {
public:
	enum class Fade { None = 0, In = 1, Out = 2 };

	struct Slot {
		int song = -1;
		/* Armed for the song-end check (+0x14). */
		bool active = false;
		bool looping = false;
		Fade fade = Fade::None;
		int fadeStart = 0;
		/* Whole seconds (+0x10): the song's fade time through `_ftol`
		 * (0x0047205c, 0x0047219a), which RetailFtol reproduces. */
		int fadeSeconds = 0;
		/* What retail hands Miles, over 127: the song's volume times the
		 * music volume cvar times the master volume, or part of it while a
		 * fade runs. */
		float gain = 0.0f;
	};

	const std::vector<Song> &Songs() const { return songs_; }
	const Slot &SlotAt( int i ) const { return slots_[i]; }
	int CurrentSong() const { return currentSong_; }
	int DesiredMood() const { return desired_; }
	int FallbackMood() const { return fallbackMood_; }
	float MasterVolume() const { return master_; }

	int FindSong( const char *name ) const {
		for ( std::size_t i = 0; i < songs_.size(); ++i ) {
			if ( !CompareNoCase( songs_[i].name.c_str(), name ) ) return static_cast<int>( i );
		}
		return -1;
	}

	/* 0x0044fa70: whether the current song still sounds on a player. */
	bool SongPlaying( const Player &player ) const { return CurrentSlot( player ) >= 0; }

	/* 0x0044eef0 once the file is read: the old songs stop and go before the
	 * new file is parsed (0x0044ee60), so a file that defines none leaves no
	 * soundtrack; with no mood yet the manager starts on `normal`. */
	bool Load( const char *text, const char *fileName, Player &player, MusicLog &log,
		ParseMessages &messages ) {
		StopAll( player, log );
		return Replace( text, fileName, log, messages );
	}

	/* The second half of Load, for a caller that stops the songs itself and
	 * has something to do between that and the table going (0x0044ee60). */
	bool Replace( const char *text, const char *fileName, MusicLog &log, ParseMessages &messages ) {
		songs_.clear();
		std::vector<Song> songs;
		if ( !ParseSoundtrack( text, fileName, songs, messages ) ) return false;
		songs_ = std::move( songs );
		currentSong_ = -1;
		if ( desired_ == MoodNone ) UpdateMood( MoodNormal, MoodNormal, log );
		return true;
	}

	/* 0x0044fa40: every song stops and the moods go to none. The song table
	 * stays, and so does whatever `playsong` could still find in it; the
	 * caller forgets the soundtrack's name. */
	void FreeAll( Player &player, MusicLog &log ) {
		StopAll( player, log );
		UpdateMood( MoodNone, MoodNone, log );
	}

	/* The sound system going away under the players. Retail's own shutdown
	 * (0x0046e260) releases its sample handles without saving where they
	 * were and forgets only the current song; everything else stays for the
	 * next start. */
	void DetachPlayers() {
		currentSong_ = -1;
	}

	/* The flag at 0x006a0064, raised from outside the music: S_Update raises
	 * it every frame while a `fadesound` fade is on (0x0047112c) and when
	 * s_volume changes (0x0047113f), before it runs the music's own update
	 * (0x00471255). That update then gives every player that is sounding and
	 * not fading the master volume (0x0047221f to 0x00472289), so a song
	 * started without a crossfade stops ignoring it. The flag waits while the
	 * music is inactive, as retail's update returns early then. */
	void Reapply() { reapply_ = true; }

	/* 0x0044f860. Returning from the action mood to the pair that preceded
	 * it skips a song marked `interrupt` in favour of the fallback. Moods are
	 * not range-checked. */
	void UpdateMood( int current, int fallback, MusicLog &log ) {
		if ( current == current_ && fallback == fallback_ ) return;
		const bool restoring = current == previousCurrent_ && fallback == previousFallback_
			&& current_ == MoodAction;
		previousCurrent_ = current_;
		previousFallback_ = fallback_;
		current_ = current;
		desired_ = current;
		fallback_ = fallback;
		fallbackMood_ = fallback;
		log.Developer( std::string( "MUSIC: changing from [ " ) + MoodName( previousCurrent_ ) + " | "
			+ MoodName( previousFallback_ ) + " ] to [ " + MoodName( current_ ) + " | "
			+ MoodName( fallback_ ) + " ]\n" );
		if ( !restoring ) return;
		const int song = FindSong( MoodName( current_ ) );
		if ( song >= 0 && ( songs_[song].flags & SongInterrupt ) ) {
			log.Developer( std::string( "MUSIC: restoring music from action state, skipping [ " )
				+ MoodName( current_ ) + " ] for [ " + MoodName( fallback_ ) + " ]\n" );
			desired_ = fallbackMood_;
		}
	}

	/* 0x0044f980. The compares are exact; an equal volume leaves the
	 * direction as it was. */
	void UpdateVolume( float volume, float fadeTime, int now ) {
		if ( volume == target_ && fadeTime == fadeSeconds_ ) return;
		start_ = master_;
		target_ = volume;
		fadeStartTime_ = now;
		fadeSeconds_ = fadeTime;
		if ( volume > master_ ) direction_ = 0;
		else if ( volume < master_ ) direction_ = 1;
	}

	/* 0x00472400: a song that does not loop hands back to the fallback mood
	 * once it is within the fallback song's fade of its end, or once it has
	 * stopped sounding. With no fallback song the fade is read from the
	 * global before the song table, which nothing writes: zero. */
	void CheckSongEnds( Player &player ) {
		for ( int i = 0; i < NumPlayers; ++i ) {
			Slot &slot = slots_[i];
			if ( !slot.active || slot.looping ) continue;
			const int fallback = FindSong( MoodName( fallbackMood_ ) );
			const float fade = fallback >= 0 ? songs_[fallback].fadeTime : 0.0f;
			const int remaining = player.Remaining( i );
			const bool ending = remaining >= 0 && static_cast<float>( remaining ) <= fade * 1000.0f;
			if ( ending || !player.Playing( i ) ) {
				slot.active = false;
				desired_ = fallbackMood_;
			}
		}
	}

	/* 0x0044f5b0 after its song-end check and reload, then 0x00472210.
	 * `ready` is retail's pair of tests before a song starts: the songs have
	 * been registered and the soundtrack has a name. `musicVolume` is the
	 * music volume cvar's value and `musicVolumeChanged` its modified flag. */
	void Serve( int now, Player &player, MusicLog &log, bool ready, float musicVolume,
		bool musicVolumeChanged ) {
		const int playingMood = currentSong_ >= 0 && currentSong_ < static_cast<int>( songs_.size() )
			? songs_[currentSong_].mood : -1;
		if ( desired_ != MoodNone ) {
			if ( desired_ != playingMood ) {
				const char *name = MoodName( desired_ );
				if ( FindSong( name ) >= 0 && ready ) {
					log.Debugger( std::string( "Playing " ) + name + ".\n" );
					PlaySong( name, now, player, musicVolume, log );
				}
			}
		} else if ( SongPlaying( player ) ) {
			StopAll( player, log );
		}

		if ( master_ != target_ ) {
			if ( fadeSeconds_ > 0.0f ) {
				const float elapsed = static_cast<float>( now - fadeStartTime_ );
				if ( direction_ == 0 ) {
					master_ = start_ + elapsed * ( target_ - start_ ) / ( fadeSeconds_ * 1000.0f );
					if ( master_ > target_ ) master_ = target_;
				} else if ( direction_ == 1 ) {
					/* Retail fades down from one, not from where the fade
					 * began (the 1.0 at 0x004ff408, 0x0044f74f). */
					master_ = 1.0f - elapsed * ( start_ - target_ ) / ( fadeSeconds_ * 1000.0f );
					if ( master_ < target_ ) master_ = target_;
				}
			} else {
				master_ = target_;
			}
			reapply_ = true;
		}
		UpdateSlotGains( now, player, musicVolume, musicVolumeChanged );
	}

	/* One whole frame of 0x0044f5b0 for a caller with nothing to reload. */
	void Update( int now, Player &player, MusicLog &log, float musicVolume, bool musicVolumeChanged ) {
		CheckSongEnds( player );
		Serve( now, player, log, true, musicVolume, musicVolumeChanged );
	}

	/* 0x00471fc0; false when the player could not set the song up. */
	bool PlaySong( const char *name, int now, Player &player, float musicVolume, MusicLog &log ) {
		const int song = FindSong( name );
		if ( song < 0 ) return true;
		if ( song == currentSong_ && SongPlaying( player ) ) return true;
		const int old = CurrentSlot( player );
		currentSong_ = song;
		const Song &s = songs_[song];
		bool crossfade = false;
		if ( old >= 0 ) {
			if ( s.flags & SongInterrupt ) {
				StopSlot( old, player );
			} else {
				/* The new song's fade governs both halves of the crossfade. */
				slots_[old].fade = Fade::Out;
				slots_[old].fadeSeconds = RetailFtol( s.fadeTime );
				slots_[old].fadeStart = now;
				crossfade = true;
			}
		}
		const int use = crossfade && old == 0 ? 1 : 0;
		StopSlot( use, player );
		if ( !player.Start( use, s, song, ( s.flags & SongLoop ) != 0 ) ) {
			log.Developer( "Could not setup mp3 file!\n" );
			return false;
		}
		Slot &slot = slots_[use];
		slot.song = song;
		slot.active = true;
		slot.looping = ( s.flags & SongLoop ) != 0;
		if ( crossfade ) {
			slot.fade = Fade::In;
			slot.fadeSeconds = RetailFtol( s.fadeTime );
			slot.fadeStart = now;
			slot.gain = 0.0f;
		} else {
			/* A song that starts without a crossfade starts at its own
			 * volume and the music volume (0x004721c7 to 0x004721e9), and
			 * takes the master volume only at the next reapplication: the
			 * master or the music volume changing, or S_Update asking
			 * (Reapply). */
			slot.fade = Fade::None;
			slot.gain = Quantize( musicVolume * s.volume );
		}
		return true;
	}

	/* 0x0044fa10. */
	void StopAll( Player &player, MusicLog &log ) {
		log.Developer( "MUSIC_StopAllSongs()\n" );
		for ( int i = 0; i < NumPlayers; ++i ) StopSlot( i, player );
		currentSong_ = -1;
	}

private:
	/* Miles takes a volume of 0 to 127, made with `_ftol` (0x004e6d40); a
	 * larger one is taken to be clamped by Miles, which alice.exe never
	 * does itself. */
	static float Quantize( float volume ) {
		const double scaled = static_cast<double>( volume ) * 127.0;
		if ( !( scaled > 0.0 ) ) return 0.0f;
		if ( scaled >= 127.0 ) return 1.0f;
		return static_cast<float>( static_cast<int>( scaled ) ) / 127.0f;
	}

	/* 0x00471ec0: the last player holding the current song, if it still
	 * sounds. */
	int CurrentSlot( const Player &player ) const {
		int found = -1;
		for ( int i = 0; i < NumPlayers; ++i ) {
			if ( player.Playing( i ) && slots_[i].song == currentSong_ ) found = i;
		}
		return found;
	}

	/* 0x00471f20: a player that is not sounding is left alone; one that is
	 * keeps its song's place unless the song is marked `restart`. The fade
	 * state is not touched. */
	void StopSlot( int i, Player &player ) {
		Slot &slot = slots_[i];
		if ( !player.Playing( i ) ) return;
		const bool rewind = slot.song >= 0 && slot.song < static_cast<int>( songs_.size() )
			&& ( songs_[slot.song].flags & SongRestart ) != 0;
		player.Stop( i, slot.song, rewind );
		slot.active = false;
	}

	/* 0x00472210. `V` is the song's volume times the music volume cvar, so
	 * with the cvar at zero a fade-out ends at once and a fade-in waits until
	 * the cvar rises again. A fade of no whole seconds completes at once;
	 * retail spends one frame on an undefined volume first. */
	void UpdateSlotGains( int now, Player &player, float musicVolume, bool musicVolumeChanged ) {
		if ( musicVolumeChanged || reapply_ ) {
			reapply_ = false;
			for ( int i = 0; i < NumPlayers; ++i ) {
				Slot &slot = slots_[i];
				if ( player.Playing( i ) && slot.fade == Fade::None && ValidSong( slot.song ) ) {
					slot.gain = Quantize( songs_[slot.song].volume * musicVolume * master_ );
				}
			}
		}
		for ( int i = 0; i < NumPlayers; ++i ) {
			Slot &slot = slots_[i];
			if ( !player.Playing( i ) || !ValidSong( slot.song ) || slot.fade == Fade::None ) continue;
			const double volume = static_cast<double>( songs_[slot.song].volume ) * musicVolume;
			const double length = static_cast<double>( slot.fadeSeconds ) * 1000.0;
			const double fraction = length != 0.0
				? static_cast<double>( static_cast<std::uint32_t>( now - slot.fadeStart ) ) / length
				: std::numeric_limits<double>::infinity();
			if ( slot.fade == Fade::In ) {
				const double level = length != 0.0 ? volume * fraction : volume + 1.0;
				if ( level <= volume ) {
					slot.gain = Quantize( static_cast<float>( master_ * level ) );
				} else {
					slot.gain = Quantize( static_cast<float>( master_ * volume ) );
					slot.fade = Fade::None;
				}
			} else {
				const double level = length != 0.0 ? volume - fraction * volume : 0.0;
				if ( level <= 0.0 ) {
					StopSlot( i, player );
				} else {
					slot.gain = Quantize( static_cast<float>( master_ * level ) );
				}
			}
		}
	}

	bool ValidSong( int song ) const {
		return song >= 0 && song < static_cast<int>( songs_.size() );
	}

	std::vector<Song> songs_;
	Slot slots_[NumPlayers];
	int currentSong_ = -1;
	/* The four mood words retail starts at -1 (0x00519b9c-0x00519ba8). */
	int current_ = -1, fallback_ = -1, previousCurrent_ = -1, previousFallback_ = -1;
	int desired_ = MoodNone;
	int fallbackMood_ = MoodNone;
	/* 0x00519b08..0x00519b10. */
	float master_ = 1.0f, target_ = 1.0f, start_ = 1.0f;
	float fadeSeconds_ = 0.0f;
	int fadeStartTime_ = 0;
	int direction_ = 0;
	/* 0x006a0064. */
	bool reapply_ = false;
};

} // namespace fntech3::audio::alice

#endif
