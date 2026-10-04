/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor: Allied Assault's music

Two layers of music sound over play, mixed here into the one music stream
the audio backend plays and scales by s_musicvolume -- the only music volume
1.00's audio menu offers (ui/audio options.urc):

  - the soundtrack. A level names a `.mus` file binding songs to moods, and
    the player state's current and fallback moods choose the song, two
    players crossfading. 2015's manager is F.A.K.K.2's: OpenMoHAA's
    reconstruction (client/snd_openal_new.cpp, reference only) has the same
    sixteen moods, the same two song channels, the same `restart`,
    `interrupt`, `loop`, `volume` and `fadetime` song commands and the same
    0.063-second start offset as retail Alice's, so
    code/client/audio/shared/AliceMusic.h, written from alice.exe, decides
    what plays and how loud;
  - the triggered music. A level's script stuffs `tmstart <file>` for one
    pass, `tmstartloop <file>` to repeat and `tmstop`
    (maps/m1l1.scr: `$player stufftext "tmstart sound/music/..."`).

Each track streams its file through the engine's codecs and is brought to
the mix rate on its own. The main menu's theme is triggered music too.

And a movie's sound file, and the sounds a level's script waits on (at the
end of the file).
===========================================================================
*/

extern "C" {
#include "client.h"
#include "audio/codecs/snd_codec.h"
#include "audio/snd_local.h"
}

#include "client_cpp.h"
#include "cl_mohaa.h"
#include "audio/shared/AliceMusic.h"
#include "audio/shared/AudioStreamResampler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

namespace music = fntech3::audio::alice;

constexpr int kMixRate = 44100;
constexpr int kMixChannels = 2;
/* Frames decoded at a time. */
constexpr int kChunkFrames = 4096;

/* ---- one streamed file ------------------------------------------------ */

class Track {
public:
	Track() = default;
	Track( const Track & ) = delete;
	Track &operator=( const Track & ) = delete;
	~Track() { Close(); }

	/* Opens `path`; `start` is where to begin in the file's own frames, or
	 * negative for the song start offset every soundtrack song and loop
	 * begins at. */
	bool Open( const std::string &path, bool loop, std::int64_t start, bool soundtrackSong = false ) {
		Close();
		path_ = path;
		loop_ = loop;
		if ( !Reopen() ) {
			return false;
		}
		loopStart_ = soundtrackSong ? startOffset_ : 0;
		skip_ = start >= 0 ? start : loopStart_;
		return true;
	}

	void Close() {
		if ( stream_ ) {
			S_CodecCloseStream( stream_ );
			stream_ = nullptr;
		}
		pending_.clear();
		pendingAt_ = 0;
		ended_ = true;
		inputEnded_ = true;
		resampler_ = {};
	}

	bool Playing() const { return !ended_ && ( stream_ != nullptr || pendingAt_ < pending_.size() ); }
	const std::string &Path() const { return path_; }
	bool Loops() const { return loop_; }

	/* Where the file is, in its own frames, for a song resumed later. */
	std::int64_t Position() const {
		if ( rate_ <= 0 ) return position_;
		const auto completed = (std::min)( acceptedInputFrames_, mixedOutputFrames_ * rate_ / kMixRate );
		std::int64_t position = position_ - ( acceptedInputFrames_ - completed );
		const auto period = totalFrames_ - loopStart_;
		if ( loop_ && period > 0 && position < loopStart_ ) {
			const auto remainder = ( position - loopStart_ ) % period;
			position = loopStart_ + ( remainder < 0 ? remainder + period : remainder );
		}
		return position;
	}

	/* Milliseconds left, or -1 when the file's length is unknown. */
	int RemainingMs() const {
		if ( rate_ <= 0 || totalFrames_ <= 0 ) {
			return -1;
		}
		const std::int64_t left = totalFrames_ - Position();
		return left > 0 ? static_cast<int>( left * 1000 / rate_ ) : 0;
	}

	/* Adds up to `frames` frames of this track at `gain` into `out`. */
	void Mix( float gain, float *out, int frames ) {
		if ( out == nullptr || frames <= 0 ) return;
		if ( !std::isfinite( gain ) ) gain = 0.0f;
		int done = 0;
		while ( done < frames && Playing() ) {
			if ( pendingAt_ >= pending_.size() && !Refill() ) {
				break;
			}
			const std::size_t available = ( pending_.size() - pendingAt_ ) / kMixChannels;
			const int take = static_cast<int>( std::min<std::size_t>( available, static_cast<std::size_t>( frames - done ) ) );
			const float *from = pending_.data() + pendingAt_;
			float *to = out + static_cast<std::size_t>( done ) * kMixChannels;
			for ( int i = 0; i < take * kMixChannels; i++ ) {
				to[i] += from[i] * gain;
			}
			pendingAt_ += static_cast<std::size_t>( take ) * kMixChannels;
			mixedOutputFrames_ += take;
			done += take;
			if ( inputEnded_ && pendingAt_ >= pending_.size() ) ended_ = true;
		}
	}

private:
	bool Reopen( bool keepTimeline = false ) {
		if ( stream_ ) {
			S_CodecCloseStream( stream_ );
		}
		stream_ = S_CodecOpenStream( path_.c_str() );
		if ( !stream_ ) {
			return false;
		}
		const auto &info = stream_->info;
		if ( info.rate <= 0 || info.rate > 192000 || info.channels <= 0 || info.channels > 32 ||
			( info.width != 1 && info.width != 2 ) || ( keepTimeline &&
			( info.rate != rate_ || info.channels != channels_ || info.width != width_ ) ) ) {
			S_CodecCloseStream( stream_ );
			stream_ = nullptr;
			return false;
		}
		rate_ = stream_->info.rate;
		channels_ = stream_->info.channels;
		width_ = stream_->info.width;
		totalFrames_ = stream_->info.samples;
		startOffset_ = static_cast<std::int64_t>( static_cast<double>( rate_ ) * music::StartOffsetSeconds );
		position_ = 0;
		cycleHasInput_ = false;
		inputEnded_ = false;
		ended_ = false;
		if ( !keepTimeline ) {
			acceptedInputFrames_ = mixedOutputFrames_ = 0;
			ended_ = !resampler_.Configure( rate_, kMixRate, kMixChannels );
			pending_.clear();
			pendingAt_ = 0;
		}
		return !ended_;
	}

	/* Decodes a chunk and brings it to the mix rate; false once the file is
	 * done and nothing is left. */
	bool Refill() {
		pending_.clear();
		pendingAt_ = 0;
		while ( pending_.empty() && stream_ && !ended_ ) {
			raw_.resize( static_cast<std::size_t>( kChunkFrames ) * static_cast<std::size_t>( channels_ * width_ ) );
			const int bytes = S_CodecReadStream( stream_, static_cast<int>( raw_.size() ), raw_.data() );
			const int frames = bytes > 0 ? bytes / ( channels_ * width_ ) : 0;
			if ( frames <= 0 ) {
				// Keep the filter and fractional phase across a real loop. A
				// file yielding no unskipped PCM cannot make a playable loop.
				if ( loop_ && cycleHasInput_ && Reopen( true ) ) {
					skip_ = loopStart_;
					continue;
				}
				inputEnded_ = true;
				if ( !resampler_.Finish( pending_ ) ) pending_.clear();
				if ( stream_ ) S_CodecCloseStream( stream_ );
				stream_ = nullptr;
				ended_ = pending_.empty();
				return !pending_.empty();
			}
			position_ += frames;
			samples_.clear();
			for ( int f = 0; f < frames; f++ ) {
				if ( skip_ > 0 ) {
					skip_--;
					continue;
				}
				for ( int c = 0; c < kMixChannels; c++ ) {
					const int from = std::min( c, channels_ - 1 );
					const std::size_t at = static_cast<std::size_t>( f * channels_ + from );
					float v;
					if ( width_ == 2 ) {
						std::int16_t s;
						std::memcpy( &s, raw_.data() + at * 2, sizeof( s ) );
						v = static_cast<float>( s );
					} else {
						v = ( static_cast<float>( raw_[at] ) - 128.0f ) * 256.0f;
					}
					samples_.push_back( v );
				}
			}
			if ( !samples_.empty() ) {
				const auto count = samples_.size() / kMixChannels;
				if ( !resampler_.Push( samples_.data(), static_cast<int>( count ), pending_ ) ) {
					Close();
					return false;
				}
				acceptedInputFrames_ += static_cast<std::int64_t>( count );
				cycleHasInput_ = true;
			}
		}
		return !pending_.empty();
	}

	std::string path_;
	snd_stream_t *stream_ = nullptr;
	bool loop_ = false, ended_ = true;
	bool inputEnded_ = true, cycleHasInput_ = false;
	int rate_ = 0, channels_ = 0, width_ = 0;
	std::int64_t totalFrames_ = 0, position_ = 0, skip_ = 0, startOffset_ = 0;
	std::int64_t loopStart_ = 0, acceptedInputFrames_ = 0, mixedOutputFrames_ = 0;
	fntech3::audio::CubicStreamResampler resampler_;
	std::vector<std::uint8_t> raw_;
	std::vector<float> samples_;
	std::vector<float> pending_;
	std::size_t pendingAt_ = 0;
};

/* ---- the soundtrack's two players ---------------------------------------- */

/* A song stopped without `restart` keeps its place for the next time its mood
 * comes round (MUSIC_StopChannel). */
struct SongPlayer final : music::Player {
	Track tracks[music::NumPlayers];
	std::map<int, std::int64_t> places;

	bool Start( int player, const music::Song &song, int songIndex, bool loop ) override {
		const auto at = places.find( songIndex );
		const std::int64_t start = at != places.end() ? at->second : -1;
		if ( !tracks[player].Open( song.file, loop, start, true ) ) {
			Com_DPrintf( "Could not start music file '%s'!\n", song.file.c_str() );
			return false;
		}
		return true;
	}
	void Stop( int player, int songIndex, bool rewind ) override {
		if ( rewind ) places.erase( songIndex );
		else places[songIndex] = tracks[player].Position();
		tracks[player].Close();
	}
	bool Playing( int player ) const override { return tracks[player].Playing(); }
	int Remaining( int player ) const override { return tracks[player].RemainingMs(); }
};

struct Log final : music::MusicLog {
	void Print( const std::string &text ) override { Com_Printf( "%s", text.c_str() ); }
	void Developer( const std::string &text ) override { Com_DPrintf( "%s", text.c_str() ); }
};

struct Messages final : music::ParseMessages {
	void Warning( const std::string &text ) override { Com_Printf( "%s", text.c_str() ); }
};

music::Soundtrack soundtrack;
SongPlayer songs;
Log musicLog;
std::string soundtrackName;
bool soundtrackActive = false;
bool soundtrackLoaded = false;

Track triggered;

/* ---- the stream the backend plays --------------------------------------- */

bool mixAttached = false;
std::vector<float> mixScratch;
bool AnythingPlays();

int MixRead( snd_stream_t *, int bytes, void *buffer ) {
	const int frames = bytes / static_cast<int>( sizeof( std::int16_t ) * kMixChannels );
	if ( frames <= 0 || buffer == nullptr ) {
		return 0;
	}
	if ( !AnythingPlays() ) return 0;
	mixScratch.assign( static_cast<std::size_t>( frames ) * kMixChannels, 0.0f );
	for ( int i = 0; i < music::NumPlayers; i++ ) {
		if ( songs.tracks[i].Playing() ) {
			songs.tracks[i].Mix( soundtrack.SlotAt( i ).gain, mixScratch.data(), frames );
		}
	}
	triggered.Mix( 1.0f, mixScratch.data(), frames );
	auto *out = static_cast<byte *>( buffer );
	for ( std::size_t i = 0; i < mixScratch.size(); i++ ) {
		const float v = mixScratch[i];
		const auto sample = !std::isfinite( v ) ? std::int16_t{0} :
			static_cast<std::int16_t>( v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : static_cast<int>( v ) );
		std::memcpy( out + i * sizeof( sample ), &sample, sizeof( sample ) );
	}
	return frames * static_cast<int>( sizeof( std::int16_t ) ) * kMixChannels;
}

void MixClose( snd_stream_t * ) {
	mixAttached = false;
}

snd_codec_t mixCodec = { "mohaa-music", nullptr, nullptr, MixRead, MixClose, nullptr };
snd_stream_t mixStream;

bool AnythingPlays() {
	return songs.tracks[0].Playing() || songs.tracks[1].Playing() || triggered.Playing();
}

void AttachMix() {
	if ( mixAttached ) {
		return;
	}
	mixStream = snd_stream_t{};
	mixStream.codec = &mixCodec;
	mixStream.info.rate = kMixRate;
	mixStream.info.width = 2;
	mixStream.info.channels = kMixChannels;
	mixAttached = true;
	if ( !S_StartStreamSource( &mixStream ) ) {
		mixAttached = false;
	}
}

/* ---- the soundtrack file ------------------------------------------------- */

bool LoadSoundtrack( const std::string &name ) {
	std::string path = name;
	if ( !std::strrchr( path.c_str(), '.' ) ) {
		path += ".mus";
	}
	void *buffer = nullptr;
	const int length = FS_ReadFile( path.c_str(), &buffer );
	if ( length <= 0 || !buffer ) {
		if ( buffer ) FS_FreeFile( buffer );
		Com_DPrintf( "MUSIC: couldn't load %s\n", path.c_str() );
		return false;
	}
	const std::string text( static_cast<const char *>( buffer ), static_cast<std::size_t>( length ) );
	FS_FreeFile( buffer );
	Messages messages;
	Com_DPrintf( "SOUNDTRACK: Loading %s\n", path.c_str() );
	return soundtrack.Load( text.c_str(), path.c_str(), songs, musicLog, messages );
}

/* ---- the console commands ------------------------------------------------- */

void TriggeredStart( bool loop ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( loop ? "tmstartloop <sound file>\n" : "tmstart <sound file>\n" );
		return;
	}
	if ( !triggered.Open( Cmd_Argv( 1 ), loop, 0 ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", Cmd_Argv( 1 ) );
		return;
	}
	AttachMix();
}

void TmStart_f() { TriggeredStart( false ); }
void TmStartLoop_f() { TriggeredStart( true ); }
void TmStop_f() { triggered.Close(); }

void LoadSoundtrack_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "loadsoundtrack <sound track file>\n" );
		return;
	}
	soundtrackName = Cmd_Argv( 1 );
	soundtrackLoaded = LoadSoundtrack( soundtrackName );
	soundtrackActive = true;
}

void PlaySong_f() {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "playsong <song alias>\n" );
		return;
	}
	soundtrack.PlaySong( Cmd_Argv( 1 ), cls.realtime, songs, 1.0f, musicLog );
	soundtrackActive = true;
	AttachMix();
}

const struct {
	const char *name;
	xcommand_t function;
} kCommands[] = {
	{ "tmstart", TmStart_f }, { "tmstartloop", TmStartLoop_f }, { "tmstop", TmStop_f },
	{ "loadsoundtrack", LoadSoundtrack_f }, { "playsong", PlaySong_f },
};

bool commandsAdded = false;

} // namespace

void CL_MOHAAMusicInit( void ) {
	if ( commandsAdded ) {
		return;
	}
	for ( const auto &command : kCommands ) {
		Cmd_AddCommand( command.name, command.function );
	}
	commandsAdded = true;
}

void CL_MOHAAMusicShutdown( void ) {
	if ( commandsAdded ) {
		for ( const auto &command : kCommands ) {
			Cmd_RemoveCommand( command.name );
		}
		commandsAdded = false;
	}
	CL_MOHAAMusicFreeAll();
	triggered.Close();
}

/* MUSIC_NewSoundtrack: "none" or nothing stops the songs; another name loads
 * its file once. */
void CL_MOHAAMusicNewSoundtrack( const char *name ) {
	const std::string next = name ? name : "";
	if ( !Q_stricmp( next.c_str(), soundtrackName.c_str() ) ) {
		return;
	}
	soundtrackName = next;
	if ( next.empty() || !Q_stricmp( next.c_str(), "none" ) ) {
		soundtrackActive = false;
		soundtrack.StopAll( songs, musicLog );
		return;
	}
	soundtrackActive = true;
	soundtrackLoaded = LoadSoundtrack( next );
}

void CL_MOHAAMusicUpdateMood( int current, int fallback ) {
	soundtrack.UpdateMood( current, fallback, musicLog );
	soundtrackActive = true;
}

void CL_MOHAAMusicUpdateVolume( float volume, float fadeTime ) {
	soundtrack.UpdateVolume( volume, fadeTime, cls.realtime );
}

/* MUSIC_FreeAllSongs, as a level ends: every song stops, the moods go and the
 * next level's soundtrack loads again from its name. */
void CL_MOHAAMusicFreeAll( void ) {
	soundtrack.FreeAll( songs, musicLog );
	songs.places.clear();
	soundtrackName.clear();
	soundtrackLoaded = false;
}

/* The triggered music a save keeps (OpenMoHAA SV_SaveGame's tm_* fields):
 * its file, whether it loops and where it is in its own frames. False, with
 * an empty name, when none plays. */
bool CL_MOHAAMusicTriggeredState( char *file, int fileSize, bool *loops, int *offset ) {
	if ( file && fileSize > 0 ) {
		file[0] = '\0';
	}
	if ( !triggered.Playing() || !file || fileSize <= 0 || !loops || !offset ) {
		return false;
	}
	Q_strncpyz( file, triggered.Path().c_str(), fileSize );
	*loops = triggered.Loops();
	*offset = static_cast<int>( std::min<std::int64_t>( triggered.Position(), 0x7fffffff ) );
	return true;
}

/* A loaded save's triggered music, from where it was (S_ServerLoaded); no
 * name stops whatever plays. */
void CL_MOHAAMusicRestoreTriggered( const char *file, bool loops, int offset ) {
	triggered.Close();
	if ( !file || !file[0] ) {
		return;
	}
	if ( triggered.Open( file, loops, offset > 0 ? offset : 0 ) ) {
		Com_DPrintf( "Triggered music %s%s from frame %i\n", file, loops ? " (looping)" : "", offset );
		AttachMix();
	} else {
		Com_DPrintf( "Could not restart triggered music '%s'\n", file );
	}
}

/* The main menu's theme, from its start and repeating, as the menu comes up
 * before a game (OpenMoHAA cl_main.cpp CL_Frame and snd_openal_new.cpp
 * S_TriggeredMusic_PlayIntroMusic, reference only; 1.00 ships the file). It
 * is the triggered music, which a level's load ends with the menus. */
void CL_MOHAAMusicMainTheme( void ) {
	static const char kTheme[] = "sound/music/mus_MainTheme.mp3";
	if ( triggered.Open( kTheme, true, 0 ) ) {
		AttachMix();
	} else {
		Com_DPrintf( "Could not start triggered music '%s'\n", kTheme );
	}
}

void CL_MOHAAMusicStopTriggered( void ) {
	triggered.Close();
}

/* Music_Update, once a frame: a song that ended hands over, the mood's song
 * starts, the fades move; and the mix goes back to the backend whenever it
 * let go of it while something plays. */
void CL_MOHAAMusicUpdate( void ) {
	if ( soundtrackActive ) {
		soundtrack.CheckSongEnds( songs );
		soundtrack.Serve( cls.realtime, songs, musicLog, soundtrackLoaded && !soundtrackName.empty(), 1.0f, false );
	}
	if ( AnythingPlays() ) {
		AttachMix();
	}
}

/* ---- a movie's sound --------------------------------------------------------

A movie's sound is a file of its own beside it, the movie's name with `.mp3`,
or `.wav` where there is none: each of 1.00's three movies has one, and the
RoQ that carries sound of its own as well, EAlogo's, plays only the file
(OpenMoHAA cl_cin.cpp and snd_openal_new.cpp S_SetupMovieAudio, reference
only, which decode the RoQ's sound and drop it; `cl_movieaudio 0` drops the
file too). It streams from the movie's first frame into the raw-sample
channel at the effects volume, as every profile's movie sound plays, kept a
fifth of a second ahead of the movie's clock. */

extern int CL_ScaledMilliseconds( void );

namespace {

constexpr int kMovieLeadMs = 200;

struct MovieSound {
	snd_stream_t *stream = nullptr;
	int start = 0;
	std::int64_t fed = 0;
	std::vector<std::uint8_t> buffer;
	/* 8-bit files widened: the raw channel reads 8-bit stereo as signed. */
	std::vector<std::int16_t> wide;
};
MovieSound movieSound;

} // namespace

void CL_MOHAAMovieSoundStop( void ) {
	if ( movieSound.stream ) {
		S_CodecCloseStream( movieSound.stream );
		movieSound.stream = nullptr;
	}
	movieSound.fed = 0;
}

/* `movie` as the `cinematic` command names it: under video/ unless it has a
 * folder of its own. */
void CL_MOHAAMovieSoundStart( const char *movie ) {
	CL_MOHAAMovieSoundStop();
	if ( !movie || !movie[0] || !Cvar_VariableIntegerValue( "cl_movieaudio" ) ) {
		return;
	}
	char base[MAX_QPATH];
	if ( std::strchr( movie, '/' ) || std::strchr( movie, '\\' ) ) {
		Q_strncpyz( base, movie, sizeof( base ) );
	} else {
		Com_sprintf( base, sizeof( base ), "video/%s", movie );
	}
	char *dot = std::strrchr( base, '.' );
	char *slash = std::strrchr( base, '/' );
	if ( dot && ( !slash || dot > slash ) ) {
		*dot = '\0';
	}
	for ( const char *extension : { ".mp3", ".wav" } ) {
		char path[MAX_QPATH];
		Com_sprintf( path, sizeof( path ), "%s%s", base, extension );
		snd_stream_t *stream = S_CodecOpenStream( path );
		if ( !stream ) {
			continue;
		}
		const snd_info_t &info = stream->info;
		if ( info.rate <= 0 || ( info.channels != 1 && info.channels != 2 ) || ( info.width != 1 && info.width != 2 ) ) {
			S_CodecCloseStream( stream );
			continue;
		}
		movieSound.stream = stream;
		movieSound.start = CL_ScaledMilliseconds();
		movieSound.fed = 0;
		Com_DPrintf( "Movie Audio setup: %s\n", path );
		CL_MOHAAMovieSoundUpdate();
		return;
	}
	Com_DPrintf( "Can't find any matching audio file for movie: %s\n", movie );
}

void CL_MOHAAMovieSoundUpdate( void ) {
	if ( !movieSound.stream ) {
		return;
	}
	const snd_info_t &info = movieSound.stream->info;
	const int frameBytes = info.channels * info.width;
	const int elapsed = CL_ScaledMilliseconds() - movieSound.start;
	const std::int64_t wanted = static_cast<std::int64_t>( std::max( elapsed, 0 ) + kMovieLeadMs ) * info.rate / 1000;
	while ( movieSound.stream && movieSound.fed < wanted ) {
		const int frames = static_cast<int>( std::min<std::int64_t>( wanted - movieSound.fed, kChunkFrames ) );
		movieSound.buffer.resize( static_cast<std::size_t>( frames ) * static_cast<std::size_t>( frameBytes ) );
		const int bytes = S_CodecReadStream( movieSound.stream, frames * frameBytes, movieSound.buffer.data() );
		const int got = bytes > 0 ? bytes / frameBytes : 0;
		if ( got <= 0 ) {
			CL_MOHAAMovieSoundStop();
			return;
		}
		if ( info.width == 1 ) {
			const std::size_t count = static_cast<std::size_t>( got ) * static_cast<std::size_t>( info.channels );
			movieSound.wide.resize( count );
			for ( std::size_t i = 0; i < count; i++ ) {
				movieSound.wide[i] = static_cast<std::int16_t>( ( static_cast<int>( movieSound.buffer[i] ) - 128 ) * 256 );
			}
			S_RawSamples( got, info.rate, 2, info.channels, reinterpret_cast<const byte *>( movieSound.wide.data() ),
				s_volume->value );
		} else {
			S_RawSamples( got, info.rate, 2, info.channels, movieSound.buffer.data(), s_volume->value );
		}
		movieSound.fed += got;
	}
}

/* ---- the sounds a level's script waits on ---------------------------------

A script's `playsound <alias> wait` and `waittill sounddone` wait on a sound
the local client plays. The game marks it with 0x400 on its entity number;
the client, starting it, tells the local server's game which of its channels
took it (the game's SoundCallback, OpenMoHAA S_FLAG_DO_CALLBACK and
callbackServer), and the game then asks each frame whether that channel
still plays it (import 135, S_IsSoundPlaying) until it has ended.

A channel here is a record of the sound: it plays for the file's length at
its pitch, from when it started, in real time as the audio does. Its length
is read from the file through the codecs, not from what the backend plays,
so the level's timing is the same with the audio muted, unfocused or off.
A later sound on the same entity channel takes the channel over -- then it
plays the later sound, as the backend's voice does -- and a stop ends it. */

namespace {

struct WaitedSound {
	int entity = -1;
	int channel = 0;
	std::string name;
	int ends = 0;
	bool active = false;
};

constexpr int kWaitedSounds = 64;
WaitedSound waitedSounds[kWaitedSounds];
int nextWaitedSound;
/* Milliseconds each file lasts, or -1 for one the codecs cannot read. */
std::map<std::string, int> soundLengths;

int SoundFileLength( const char *name ) {
	std::string key( name );
	std::transform( key.begin(), key.end(), key.begin(), []( unsigned char c ) {
		return static_cast<char>( std::tolower( c ) );
	} );
	const auto known = soundLengths.find( key );
	if ( known != soundLengths.end() ) {
		return known->second;
	}
	int length = -1;
	if ( snd_stream_t *stream = S_CodecOpenStream( name ) ) {
		if ( stream->info.rate > 0 && stream->info.samples > 0 ) {
			length = static_cast<int>( static_cast<std::int64_t>( stream->info.samples ) * 1000 / stream->info.rate );
		}
		S_CodecCloseStream( stream );
	}
	soundLengths[key] = length;
	return length;
}

bool Plays( const WaitedSound &sound ) {
	return sound.active && cls.realtime < sound.ends;
}

} // namespace

/* A sound the client game started. The automatic channel, 0, never takes one
 * over; any other ends what its entity's channel played before. */
int CL_MOHAASoundStarted( int entity, int channel, const char *name, float pitch, bool waited ) {
	int id = -1;
	for ( int i = 0; i < kWaitedSounds; i++ ) {
		WaitedSound &sound = waitedSounds[i];
		if ( channel != 0 && sound.active && sound.entity == entity && sound.channel == channel ) {
			sound.active = false;
			if ( id < 0 ) {
				id = i;
			}
		}
	}
	if ( !waited || !name || !name[0] ) {
		return -1;
	}
	if ( id < 0 ) {
		/* The next record in turn that has ended; the next in turn, playing
		 * or not, when every one still plays. */
		id = nextWaitedSound;
		for ( int tries = 0; tries < kWaitedSounds; tries++ ) {
			const int candidate = ( nextWaitedSound + tries ) % kWaitedSounds;
			if ( !Plays( waitedSounds[candidate] ) ) {
				id = candidate;
				break;
			}
		}
		nextWaitedSound = ( id + 1 ) % kWaitedSounds;
	}
	const int length = SoundFileLength( name );
	WaitedSound &sound = waitedSounds[id];
	sound.entity = entity;
	sound.channel = channel;
	sound.name = name;
	sound.active = length > 0;
	sound.ends = cls.realtime + ( length > 0 ? static_cast<int>( length / ( pitch > 0.0f ? pitch : 1.0f ) ) : 0 );
	return id;
}

void CL_MOHAASoundStopped( int entity, int channel ) {
	for ( WaitedSound &sound : waitedSounds ) {
		if ( sound.active && sound.entity == entity && sound.channel == channel ) {
			sound.active = false;
		}
	}
}

bool CL_MOHAASoundPlaying( int id, const char *name ) {
	if ( id < 0 || id >= kWaitedSounds || !name ) {
		return false;
	}
	const WaitedSound &sound = waitedSounds[id];
	return Plays( sound ) && !Q_stricmp( sound.name.c_str(), name );
}

/* A new client game, or none: no sound it started is waited on any more. */
void CL_MOHAASoundsClear( void ) {
	for ( WaitedSound &sound : waitedSounds ) {
		sound = WaitedSound();
	}
	nextWaitedSound = 0;
}
