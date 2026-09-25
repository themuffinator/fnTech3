/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* American McGee's Alice's sound, served to its client game and its engine:
 *
 *   - the sound effects (client-game imports 30 to 41), decided by the engine
 *     in shared/AliceSound.h and played by whichever backend is running
 *     through the Device it hands over;
 *   - the music (imports 42 to 44), decided by the soundtrack manager in
 *     shared/AliceMusic.h and streamed here from song images read once per
 *     soundtrack, as retail reads them;
 *   - the MP3 track retail plays beside each cinematic, and its playmp3 and
 *     stopmp3 commands.
 *
 * Retail plays two songs and the cinematic track through three Miles handles;
 * here they are mixed into the one music stream the backend already knows how
 * to play, at the volumes Miles would have been handed, so the backend adds
 * neither s_musicVolume nor s_volume to it. See
 * docs/fntech3/2026-09-23-alice-audio.md. */

extern "C" {
#include "../client.h"
#include "codecs/snd_codec.h"
#include "snd_local.h"
#include "snd_public.h"
#include "../../qcommon/cm_public.h"
}
#include "../../qcommon/alice_tiki.h"
#include "shared/AliceMusic.h"
#include "shared/AliceSound.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace fntech3::audio::alice;
namespace sfx = fntech3::audio::alice::sfx;

/* Every song the installation ships is 44.1 kHz MPEG layer III, so the mix
 * runs at that rate and converts anything else to it. */
constexpr int kMixRate = 44100;
constexpr int kMixChannels = 2;
/* The soundtrack name field is 128 bytes (0x0069ffdc); retail copies into it
 * unbounded. */
constexpr std::size_t kSoundtrackNameBytes = 127;

bool SoundStarted() {
	return S_AliceBackendStarted();
}

/* ---- file images ------------------------------------------------------ */

using Image = std::vector<std::uint8_t>;

bool ReadImage( const char *name, Image &image ) {
	image.clear();
	union {
		void *v;
		std::uint8_t *b;
	} buffer;
	buffer.v = nullptr;
	const int length = FS_ReadFile( name, &buffer.v );
	if ( length <= 0 || !buffer.v ) {
		if ( buffer.v ) FS_FreeFile( buffer.v );
		return false;
	}
	image.assign( buffer.b, buffer.b + length );
	FS_FreeFile( buffer.v );
	return true;
}

/* ---- one decoded track ------------------------------------------------ */

/* An MP3 image being decoded, and the linear resampler that brings it to the
 * mix rate. `position` counts the file's frames from its start, so a song
 * started 2,778 bytes in has already passed the frames before them. */
struct Track {
#ifdef USE_CODEC_MP3
	snd_mp3_memory_t *decoder = nullptr;
#endif
	snd_info_t info{};
	int loopOffset = 0;
	bool loop = false;
	bool ended = false;
	std::int64_t position = 0;
	double phase = 0.0;
	float previous[2] = { 0.0f, 0.0f };
	float next[2] = { 0.0f, 0.0f };
	bool primed = false;
	std::vector<std::int16_t> raw;
	std::size_t rawAt = 0;
	/* The gain the last mix ended on, or negative before the first. */
	float applied = -1.0f;

	Track() = default;
	Track( const Track & ) = delete;
	Track &operator=( const Track & ) = delete;
	Track( Track &&other ) noexcept { *this = std::move( other ); }
	Track &operator=( Track &&other ) noexcept {
		if ( this == &other ) return *this;
		Close();
#ifdef USE_CODEC_MP3
		decoder = other.decoder;
		other.decoder = nullptr;
#endif
		info = other.info;
		loopOffset = other.loopOffset;
		loop = other.loop;
		ended = other.ended;
		position = other.position;
		phase = other.phase;
		previous[0] = other.previous[0];
		previous[1] = other.previous[1];
		next[0] = other.next[0];
		next[1] = other.next[1];
		primed = other.primed;
		raw = std::move( other.raw );
		rawAt = other.rawAt;
		applied = other.applied;
		other.Reset();
		return *this;
	}
	~Track() { Close(); }

	bool Open() const {
#ifdef USE_CODEC_MP3
		return decoder != nullptr;
#else
		return false;
#endif
	}

	void Close() {
#ifdef USE_CODEC_MP3
		if ( decoder ) S_MP3_CloseMemory( decoder );
		decoder = nullptr;
#endif
		Reset();
	}

	/* Opens `image`, which must outlive the track, and starts at the byte
	 * `startOffset` returns for the file's rate. */
	template<class Offset>
	bool Begin( const Image &image, bool looping, Offset startOffset ) {
		Close();
#ifdef USE_CODEC_MP3
		if ( image.empty() ) return false;
		decoder = S_MP3_OpenMemory( reinterpret_cast<const byte *>( image.data() ),
			static_cast<int>( image.size() ), &info );
		if ( !decoder || info.rate <= 0 || info.rate > 192000 || info.width != 2
			|| info.channels < 1 || info.channels > 2 ) {
			Close();
			return false;
		}
		loop = looping;
		loopOffset = startOffset( info.rate, S_MP3_MemoryStartOffset( decoder ) );
		position = S_MP3_SeekFileOffset( decoder, loopOffset );
		return true;
#else
		(void)image;
		(void)looping;
		(void)startOffset;
		return false;
#endif
	}

	bool Playing() const { return Open() && !ended; }

	/* Milliseconds of the file left, or -1 where that is not measured. */
	int Remaining() const {
		if ( !Open() || loop || info.samples <= 0 || info.rate <= 0 ) return -1;
		const std::int64_t left = info.samples - position;
		return left <= 0 ? 0 : static_cast<int>( left * 1000 / info.rate );
	}

	/* The next source frame; a looping track seeks back to its loop offset
	 * in the image it holds, with no file read. */
	bool Pull( float out[2] ) {
#ifdef USE_CODEC_MP3
		for ( int attempt = 0; attempt < 2 && Open(); ++attempt ) {
			const std::size_t channels = static_cast<std::size_t>( info.channels );
			if ( rawAt + channels > raw.size() ) {
				raw.resize( 8192 - 8192 % channels );
				const int bytes = S_MP3_ReadMemory( decoder, static_cast<int>( raw.size() * sizeof( std::int16_t ) ), raw.data() );
				rawAt = 0;
				const std::size_t got = bytes > 0 ? static_cast<std::size_t>( bytes ) / sizeof( std::int16_t ) : 0;
				raw.resize( got - got % channels );
				if ( raw.empty() ) {
					if ( !loop ) break;
					position = S_MP3_SeekFileOffset( decoder, loopOffset );
					continue;
				}
			}
			out[0] = static_cast<float>( raw[rawAt] );
			out[1] = static_cast<float>( raw[rawAt + ( channels == 2 ? 1 : 0 )] );
			rawAt += channels;
			++position;
			return true;
		}
#else
		(void)out;
#endif
		ended = true;
		return false;
	}

	/* Adds `frames` mix frames times a gain moving from where the last call
	 * left it to `gain`, so a fade is heard as a ramp and not as steps. */
	void Mix( float gain, float *mix, int frames ) {
		if ( !Playing() || frames <= 0 ) return;
		const float from = applied < 0.0f ? gain : applied;
		applied = gain;
		const double step = static_cast<double>( info.rate ) / kMixRate;
		if ( !primed ) {
			if ( !Pull( previous ) ) return;
			if ( !Pull( next ) ) {
				next[0] = previous[0];
				next[1] = previous[1];
			}
			primed = true;
			phase = 0.0;
		}
		for ( int i = 0; i < frames; ++i ) {
			const float g = from + ( gain - from ) * static_cast<float>( i + 1 ) / static_cast<float>( frames );
			const float t = static_cast<float>( phase );
			for ( int c = 0; c < kMixChannels; ++c ) {
				mix[2 * i + c] += g * ( previous[c] + ( next[c] - previous[c] ) * t );
			}
			phase += step;
			while ( phase >= 1.0 ) {
				phase -= 1.0;
				previous[0] = next[0];
				previous[1] = next[1];
				if ( !Pull( next ) ) return;
			}
		}
	}

private:
	void Reset() {
		info = snd_info_t{};
		loopOffset = 0;
		loop = false;
		ended = false;
		position = 0;
		phase = 0.0;
		previous[0] = previous[1] = next[0] = next[1] = 0.0f;
		primed = false;
		raw.clear();
		rawAt = 0;
		applied = -1.0f;
	}
};

/* ---- the soundtrack's two players ------------------------------------- */

std::vector<Image> songImages;

class SongPlayer final : public Player {
public:
	/* 0x00471fc0 from its set-up on: a parked song resumes where it was;
	 * otherwise it starts floor(rate * 0.063) bytes into its image, which is
	 * also where it loops back to (0x00472147, 0x00472170). A song whose
	 * image did not load cannot be set up. */
	bool Start( int player, const Song &, int songIndex, bool loop ) override {
		slots_[player].Close();
		auto parked = parked_.find( songIndex );
		if ( parked != parked_.end() ) {
			slots_[player] = std::move( parked->second );
			parked_.erase( parked );
			slots_[player].loop = loop;
			slots_[player].applied = -1.0f;
			return true;
		}
		if ( songIndex < 0 || songIndex >= static_cast<int>( songImages.size() ) ) return false;
		return slots_[player].Begin( songImages[static_cast<std::size_t>( songIndex )], loop,
			[]( int rate, int ) { return StartOffsetBytes( rate ); } );
	}

	/* A song without `restart` keeps its place, as retail keeps the
	 * millisecond it stopped at (0x00471f20); here the decoder itself waits. */
	void Stop( int player, int songIndex, bool rewind ) override {
		Track &slot = slots_[player];
		if ( !slot.Open() ) return;
		if ( rewind || slot.ended ) {
			slot.Close();
			return;
		}
		parked_[songIndex] = std::move( slot );
	}

	bool Playing( int player ) const override { return slots_[player].Playing(); }
	int Remaining( int player ) const override { return slots_[player].Remaining(); }

	void Mix( const Soundtrack &music, float *mix, int frames ) {
		for ( int p = 0; p < NumPlayers; ++p ) {
			/* A silent player still moves on, as a sample at volume zero
			 * does. */
			slots_[p].Mix( music.SlotAt( p ).gain, mix, frames );
		}
	}

	/* The sound system going away (0x0046e260): the two players let go of
	 * their songs without keeping their places, since retail's shutdown
	 * never calls MUSIC_StopSong. The songs stopped earlier keep theirs:
	 * retail keeps a song's place in the song table (+0x70), which only the
	 * loader (0x0044f4c5) and MUSIC_StopSong (0x00471f79, 0x00471f97) write,
	 * and here a parked decoder is that place. A parked decoder reads its
	 * song's image and nothing of the backend, so it outlives one. */
	void CloseLive() {
		for ( Track &slot : slots_ ) slot.Close();
	}

	/* The soundtrack's songs going away (0x0044ee60): every decoder goes,
	 * the parked ones too, since their images are about to be freed. */
	void Clear() {
		CloseLive();
		parked_.clear();
	}

private:
	std::array<Track, NumPlayers> slots_;
	std::map<int, Track> parked_;
};

struct ConsoleLog final : MusicLog {
	void Print( const std::string &text ) override { Com_Printf( "%s", text.c_str() ); }
	void Developer( const std::string &text ) override { Com_DPrintf( "%s", text.c_str() ); }
	/* 0x00438a90 writes to the debugger alone; the system console is the
	 * nearest place fnTech3 has that is not the game's own. */
	void Debugger( const std::string &text ) override {
		if ( com_developer && com_developer->integer ) Sys_Print( text.c_str() );
	}
};

struct ConsoleMessages final : ParseMessages {
	void Warning( const std::string &text ) override { Com_Printf( "%s", text.c_str() ); }
};

Soundtrack music;
SongPlayer player;
ConsoleLog musicLog;
/* 0x0069ffdc, 0x006a005c and 0xb5a3a4. */
std::string soundtrackName;
bool musicActive = false;
bool songsLoaded = false;

/* ---- the cinematic track (one Miles handle at 0x00aa8ecc) ------------- */

Image mp3Image;
bool mp3Held = false;
Track mp3Track;

/* ---- the mix the backend plays ---------------------------------------- */

/* The one music stream the backend reads. The backend closes it whenever it
 * drops its track, and the next update hands it back. */
bool mixAttached = false;
std::vector<float> mixScratch;

int MixRead( snd_stream_t *, int bytes, void *buffer ) {
	const int frames = bytes / static_cast<int>( sizeof( std::int16_t ) * kMixChannels );
	if ( frames <= 0 ) return 0;
	mixScratch.assign( static_cast<std::size_t>( frames ) * kMixChannels, 0.0f );
	player.Mix( music, mixScratch.data(), frames );
	/* No volume is ever set on the cinematic handle (0x0046ee75), so it
	 * plays at Miles's full scale. */
	mp3Track.Mix( 1.0f, mixScratch.data(), frames );
	auto *out = static_cast<std::int16_t *>( buffer );
	for ( std::size_t i = 0; i < mixScratch.size(); ++i ) {
		const float v = mixScratch[i];
		out[i] = static_cast<std::int16_t>( v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : static_cast<int>( v ) );
	}
	return frames * static_cast<int>( sizeof( std::int16_t ) ) * kMixChannels;
}

void MixClose( snd_stream_t * ) {
	mixAttached = false;
}

snd_codec_t mixCodec = { "alice-music", nullptr, nullptr, MixRead, MixClose, nullptr };
snd_stream_t mixStream;

void AttachMix() {
	if ( mixAttached || !SoundStarted() ) return;
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

bool AnythingSounds() {
	return player.Playing( 0 ) || player.Playing( 1 ) || mp3Track.Playing();
}

/* ---- the soundtrack ----------------------------------------------------- */

/* 0x0044fad0 with S_RegisterSong (0x0044cc30): every song's whole file is
 * read now. A song whose file is missing stays in the table and is tried
 * again the next time; it is reported each time. */
void RegisterSongs() {
	const std::vector<Song> &songs = music.Songs();
	songImages.resize( songs.size() );
	for ( std::size_t i = 0; i < songs.size(); ++i ) {
		if ( !songImages[i].empty() ) continue;
		if ( !ReadImage( songs[i].file.c_str(), songImages[i] ) ) {
			Com_Printf( "Couldn't load song: %s\n", songs[i].file.c_str() );
		}
	}
	songsLoaded = true;
}

/* 0x0044ee60: the old songs' images go. Retail registers each song again to
 * free it, which reads a missing one once more and reports it again. */
void ClearSongs( const std::vector<Song> &old ) {
	player.Clear();
	for ( std::size_t i = 0; i < old.size(); ++i ) {
		if ( i < songImages.size() && !songImages[i].empty() ) continue;
		Image discard;
		if ( !ReadImage( old[i].file.c_str(), discard ) ) {
			Com_Printf( "Couldn't load song: %s\n", old[i].file.c_str() );
		}
	}
	songImages.clear();
}

/* 0x0044eef0. A file that cannot be read changes nothing. */
bool LoadSoundtrackFile( const std::string &name ) {
	char file[MAX_QPATH];
	if ( std::strchr( name.c_str(), '.' ) ) {
		Q_strncpyz( file, name.c_str(), sizeof( file ) );
	} else {
		Com_sprintf( file, sizeof( file ), "%s.mus", name.c_str() );
	}
	union {
		char *c;
		void *v;
	} buffer;
	buffer.v = nullptr;
	const int length = FS_ReadFile( file, &buffer.v );
	if ( length < 0 || !buffer.v ) {
		Com_DPrintf( "Couldn't load %s\n", file );
		return false;
	}
	Com_DPrintf( "SOUNDTRACK: Loading %s\n", file );
	const std::vector<Song> old = music.Songs();
	music.StopAll( player, musicLog );
	ClearSongs( old );
	ConsoleMessages messages;
	const bool loaded = music.Replace( buffer.c, file, musicLog, messages );
	FS_FreeFile( buffer.v );
	return loaded;
}

/* Retail notices the music volume cvar through its modified flag
 * (0x00472210); this watches its modification count instead, so no other
 * reader of the flag is disturbed. */
bool MusicVolumeChanged() {
	static int seen = -1;
	if ( !s_musicVolume || s_musicVolume->modificationCount == seen ) return false;
	seen = s_musicVolume->modificationCount;
	return true;
}

float MusicVolume() {
	return s_musicVolume ? s_musicVolume->value : 1.0f;
}

void NewSoundtrack( const char *name ) {
	if ( !SoundStarted() ) return;
	std::string wanted = name ? name : "";
	if ( wanted.size() > kSoundtrackNameBytes ) wanted.resize( kSoundtrackNameBytes );
	if ( !Q_stricmp( wanted.c_str(), soundtrackName.c_str() ) ) return;
	soundtrackName = wanted;
	if ( Q_stricmp( soundtrackName.c_str(), "none" ) && !soundtrackName.empty() ) {
		musicActive = true;
		LoadSoundtrackFile( soundtrackName );
		/* Even when the file could not be read (0x0044f7a0). */
		RegisterSongs();
	} else {
		musicActive = false;
		if ( music.SongPlaying( player ) ) music.StopAll( player, musicLog );
	}
}

void PlaySong_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "playsong <song alias>\n" );
		return;
	}
	music.PlaySong( Cmd_Argv( 1 ), cls.realtime, player, MusicVolume(), musicLog );
	AttachMix();
}

/* 0x00471e60: the file's songs, without naming a soundtrack or making music
 * active. */
void LoadSoundtrack_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "loadsoundtrack <sound track file>\n" );
		return;
	}
	LoadSoundtrackFile( Cmd_Argv( 1 ) );
	RegisterSongs();
}

/* ---- the cinematic track ------------------------------------------------ */

void ReleaseMP3() {
	mp3Track.Close();
	mp3Image.clear();
	mp3Held = false;
}

/* The track plays from its first frame, not from the songs' offset. */
bool BeginMP3( bool loop ) {
	if ( !mp3Track.Begin( mp3Image, loop, []( int, int start ) { return start; } ) ) {
		Com_Printf( "Could not setup mp3 file!\n" );
		return false;
	}
	AttachMix();
	return true;
}

/* 0x0046ede0. */
bool PlayMP3File( const char *name ) {
	if ( mp3Held ) ReleaseMP3();
	if ( !SoundStarted() || !ReadImage( name, mp3Image ) ) return false;
	mp3Held = true;
	if ( !BeginMP3( true ) ) return false;
	Com_Printf( "Playing mp3 - %s\n", name );
	return true;
}

void PlayMP3_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "playmp3 <mp3 file>\n" );
		return;
	}
	if ( !PlayMP3File( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "Could not start mp3 - %s\n", Cmd_Argv( 1 ) );
	}
}

/* 0x0046f080: the track ends; its image stays. */
void StopMP3_f( void ) {
	if ( mp3Held ) mp3Track.Close();
}

/* ---- sound effects ------------------------------------------------------ */

sfx::SoundInfoTable soundInfos;
sfx::Engine engine;
sfx::Fade fade;
float ambient = 1.0f;
int reverbType = 0;
float reverbLevel = 0.0f;
cvar_t *s_reverb;
cvar_t *s_show_sounds;
int volumeSeen = -1;

struct EngineLog final : sfx::Log, sfx::InfoLog {
	void Print( const std::string &text ) override { Com_Printf( "%s", text.c_str() ); }
	void Developer( const std::string &text ) override { Com_DPrintf( "%s", text.c_str() ); }
};

/* CM_BoxTrace (0x00437130) is the trace retail makes, and it leaves the
 * trace's entity number zero; the test against the listener is kept, so for
 * a listener that is entity 0 it never finds anything in the way. */
struct CollisionWorld final : sfx::World {
	bool Blocked( const float *from, const float *to, int listener ) override {
		if ( CM_NumInlineModels() <= 0 ) return false;
		trace_t trace;
		CM_BoxTrace( &trace, from, to, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
		return trace.fraction < 1.0f && trace.entityNum != listener;
	}
};

sfx::Frame MakeFrame() {
	sfx::Frame frame;
	frame.serverTime = cl.serverTime;
	frame.realtime = cls.realtime;
	frame.volume = s_volume ? s_volume->value : 1.0f;
	frame.master = fade.Master( frame.volume );
	frame.ambient = ambient;
	frame.separation = s_separation ? s_separation->value : 0.3f;
	frame.developer = com_developer && com_developer->integer;
	frame.showSounds = s_show_sounds && s_show_sounds->integer > 0;
	return frame;
}

sfx::Device *RequireDevice( const char *what ) {
	if ( !Com_GameIsRitual() ) Com_Error( ERR_DROP, "%s requested for another game profile", what );
	return S_AliceDevice();
}

/* 0x0044da50. */
void LoadSoundInfos() {
	EngineLog log;
	soundInfos.Begin();
	for ( int i = 0; i < 10; ++i ) {
		char name[MAX_QPATH];
		Com_sprintf( name, sizeof( name ), "global/sound%d.txt", i );
		union {
			char *c;
			void *v;
		} buffer;
		buffer.v = nullptr;
		if ( FS_ReadFile( name, &buffer.v ) < 0 || !buffer.v ) continue;
		Com_Printf( "Loading %s\n", name );
		soundInfos.Parse( buffer.c, log );
		FS_FreeFile( buffer.v );
	}
	soundInfos.End();
	engine.SetInfos( &soundInfos );
}

/* play (0x0044cfb0): each name gains `.wav` when it has no dot, then plays
 * as import 31 plays it, alias first, on the menu channel. */
void Play_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: play <sound filename> [sound filename] [sound filename] ...\n" );
		return;
	}
	for ( int i = 1; i < Cmd_Argc(); ++i ) {
		char name[MAX_QPATH];
		Q_strncpyz( name, Cmd_Argv( i ), sizeof( name ) );
		if ( !std::strchr( name, '.' ) ) Q_strcat( name, sizeof( name ), ".wav" );
		S_AliceStartLocalSound( name );
	}
}

/* sounddump (0x004716f0). */
void SoundDump_f( void ) {
	sfx::Device *device = S_AliceDevice();
	if ( !device ) return;
	for ( const std::string &line : engine.Dump( *device ) ) Com_Printf( "%s", line.c_str() );
}

} // namespace

/* ======================================================================= */
/* Sound effects: client-game imports 30 to 41                              */
/* ======================================================================= */

/* Import 30 (0x0044cdb0, 0x0046fa50). */
extern "C" void S_AliceStartSound( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx,
	float volume, float minDist ) {
	sfx::Device *device = RequireDevice( "Alice sound" );
	if ( !device ) return;
	sfx::SampleFacts facts;
	if ( sfx < 0 || !device->Sample( sfx, facts ) ) {
		Com_Error( ERR_DROP, "S_MILES_StartSound: handle %i out of range", sfx );
	}
	if ( sfx == 0 ) return;
	if ( entchannel < 0 ) Com_Error( ERR_DROP, "S_MILES_PickChannel: entchannel<0" );
	EngineLog log;
	engine.StartSound( *device, log, origin, entnum, entchannel, sfx, volume, minDist, MakeFrame() );
}

/* Import 31 (0x0044ce10): a name, not a handle, resolved through the global
 * alias list, registered, and played at the listener on the menu channel. */
extern "C" void S_AliceStartLocalSound( const char *name ) {
	if ( !name ) return;
	if ( !RequireDevice( "Alice local sound" ) ) return;
	const char *alias = AliceAlias_FindRandom( -1, name );
	const sfxHandle_t handle = S_AliceRegisterSound( alias ? alias : name );
	S_AliceStartSound( nullptr, engine.Listener(), sfx::MenuChannel, handle, -1.0f, -1.0f );
}

/* Import 32 (0x0044cf50, 0x00471340). */
extern "C" void S_AliceStopSound( int entnum, int channel ) {
	sfx::Device *device = RequireDevice( "Alice sound stop" );
	if ( !device ) return;
	EngineLog log;
	engine.StopSound( *device, log, entnum, channel, MakeFrame() );
}

/* Import 33 (0x0044ce80, 0x0046fe30). */
extern "C" void S_AliceClearLoopingSounds( void ) {
	if ( !RequireDevice( "Alice looping sound reset" ) ) return;
	engine.ClearLoopingSounds();
}

/* Import 34 (0x0044cea0, 0x0046fe70). */
extern "C" void S_AliceAddLoopingSound( const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx,
	float volume, float minDist ) {
	sfx::Device *device = RequireDevice( "Alice looping sound" );
	if ( !device ) return;
	sfx::SampleFacts facts;
	if ( sfx < 0 || !device->Sample( sfx, facts ) ) {
		Com_Error( ERR_DROP, "S_AddLoopingSound: handle %i out of range", sfx );
	}
	if ( sfx == 0 ) return;
	EngineLog log;
	engine.AddLoopingSound( *device, log, origin, velocity, sfx, volume, minDist, MakeFrame() );
}

/* Import 35 (0x0044cf00, 0x00470790). The listener goes to the backend with
 * the axis transposed, as retail's column read amounts to. */
extern "C" void S_AliceRespatialize( int entnum, const vec3_t origin, vec3_t axis[3] ) {
	sfx::Device *device = RequireDevice( "Alice respatialize" );
	if ( !device || !origin || !axis ) return;
	vec3_t listenerAxis[3];
	sfx::RetailListenerAxis( axis, listenerAxis );
	S_Respatialize( entnum, origin, listenerAxis, 0 );
	EngineLog log;
	CollisionWorld world;
	engine.Respatialize( *device, world, log, entnum, origin, listenerAxis, MakeFrame() );
}

/* Import 36 (0x0044cae0). */
extern "C" void S_AliceBeginRegistration( void ) {
	if ( !RequireDevice( "Alice sound registration" ) ) return;
	S_BeginRegistration();
}

/* Import 37 (0x0044cb10). Retail's sfx names are case-sensitive and every
 * backend here folds case, so two spellings share one handle. */
extern "C" sfxHandle_t S_AliceRegisterSound( const char *name ) {
	if ( !RequireDevice( "Alice sound registration" ) ) return 0;
	if ( !name || !*name ) return 0;
	if ( std::strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}
	return S_RegisterSound( name, qfalse );
}

/* Import 38 (0x0044ccc0) frees the sounds the level did not register again
 * and recycles their handles; the client game cannot tell, and the backends
 * keep what they loaded. */
extern "C" void S_AliceEndRegistration( void ) {
	RequireDevice( "Alice sound registration" );
}

/* Import 39 (0x0044cd20). */
extern "C" void S_AliceUpdateEntity( int entnum, const vec3_t origin, const vec3_t velocity, qboolean useListener ) {
	if ( !RequireDevice( "Alice sound entity" ) ) return;
	if ( entnum < 0 || entnum > sfx::Entities ) Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entnum );
	engine.UpdateEntity( entnum, origin, velocity, useListener != qfalse, cl.serverTime );
}

/* Import 40 (0x0044cf80): the room the client game names, from its player
 * state. The OpenAL backend plays its own room -- the map's audio zones or
 * the traced room -- and a named one replaces it; see S_AliceNamedRoom. */
extern "C" void S_AliceSetReverb( int type, float level ) {
	if ( !RequireDevice( "Alice reverb" ) ) return;
	if ( type != reverbType || level != reverbLevel ) {
		Com_DPrintf( "Alice room: environment %i at level %.2f\n", type, static_cast<double>( level ) );
	}
	reverbType = type;
	reverbLevel = level;
}

/* Import 41 (0x0044cda0): S_Update puts it back every frame. */
extern "C" void S_AliceSetGlobalAmbientVolumeLevel( float volume ) {
	if ( !RequireDevice( "Alice ambient level" ) ) return;
	ambient = volume;
}

/* The server command `fadesound <ms>` (0x004241f6) and the engine's own
 * cancel with 0 (0x0042aa6f, 0x0042ab90, 0x0042d89f). */
extern "C" void S_AliceFadeSound( float ms ) {
	if ( !Com_GameIsRitual() ) Com_Error( ERR_DROP, "Alice fade requested for another game profile" );
	fade.Start( ms, cls.realtime );
}

/* ======================================================================= */
/* Music: client-game imports 42 to 44                                     */
/* ======================================================================= */

/* Import 42 (0x0044f7a0). */
extern "C" void S_AliceMusicNewSoundtrack( const char *name ) {
	if ( !Com_GameIsRitual() ) return;
	NewSoundtrack( name );
}

/* Import 43 (0x0044f860). */
extern "C" void S_AliceMusicUpdateMood( int current, int fallback ) {
	if ( !Com_GameIsRitual() ) return;
	music.UpdateMood( current, fallback, musicLog );
}

/* Import 44 (0x0044f980). */
extern "C" void S_AliceMusicUpdateVolume( float volume, float fadeTime ) {
	if ( !Com_GameIsRitual() ) return;
	music.UpdateVolume( volume, fadeTime, cls.realtime );
}

/* 0x0044fa40, which retail reaches through S_StopAllSounds(1): every song
 * stops, the moods go to none and the soundtrack loses its name, so the
 * next level's name reloads it from the top. The table stays. */
extern "C" void S_AliceMusicFreeAll( void ) {
	if ( !Com_GameIsRitual() || !SoundStarted() ) return;
	music.FreeAll( player, musicLog );
	soundtrackName.clear();
	songsLoaded = false;
}

/* After vid_restart and snd_restart outside a level retail names
 * "music/intro" (0x0042b5d8, 0x0042b62b), a file no pak has; the load fails
 * and whatever table was there stays. */
extern "C" void S_AliceMusicSoundtrackAfterRestart( void ) {
	if ( !Com_GameIsRitual() || cls.state == CA_ACTIVE ) return;
	NewSoundtrack( "music/intro" );
}

/* Once a frame (0x0044f5b0): the song-end check comes before the reload. */
extern "C" void S_AliceMusicUpdate( void ) {
	if ( !Com_GameIsRitual() || !SoundStarted() ) return;
	if ( musicActive ) {
		music.CheckSongEnds( player );
		if ( !songsLoaded && !soundtrackName.empty() ) {
			LoadSoundtrackFile( soundtrackName );
			RegisterSongs();
		}
		music.Serve( cls.realtime, player, musicLog, songsLoaded && !soundtrackName.empty(),
			MusicVolume(), MusicVolumeChanged() );
	}
	if ( AnythingSounds() ) AttachMix();
}

/* ======================================================================= */
/* The cinematic track                                                      */
/* ======================================================================= */

/* 0x0046eef0, from CL_PlayCinematic (0x0042745d). */
extern "C" qboolean S_AliceMP3Preload( const char *name ) {
	if ( !Com_GameIsRitual() ) return qfalse;
	if ( mp3Held ) ReleaseMP3();
	if ( !SoundStarted() || !name || !ReadImage( name, mp3Image ) ) return qfalse;
	mp3Held = true;
	return qtrue;
}

/* 0x0046ef90, from CL_PlayCinematic once the first frame is decoded
 * (0x00427569), which asks for a single pass. */
extern "C" qboolean S_AliceMP3PlayPreloaded( qboolean loop ) {
	if ( !Com_GameIsRitual() || !mp3Held || !SoundStarted() ) return qfalse;
	if ( !BeginMP3( loop != qfalse ) ) return qfalse;
	Com_Printf( "Playing mp3\n" );
	return qtrue;
}

/* 0x004725b0, from the cinematic's shutdown (0x004257f4). */
extern "C" void S_AliceMP3Free( void ) {
	if ( !Com_GameIsRitual() ) return;
	if ( mp3Held ) ReleaseMP3();
}

/* ======================================================================= */
/* The sound system's own life                                              */
/* ======================================================================= */

/* The Miles start (0x0046e4a0) and S_Init's Alice parts, once the cvars
 * S_Init owns are registered and before a backend starts. */
extern "C" void S_AliceSoundInit( void ) {
	if ( !Com_GameIsRitual() ) return;
	s_reverb = Cvar_Get( "s_reverb", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( s_reverb, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_reverb, "Lets a room American McGee's Alice's client game names -- a map's reverb trigger -- replace the OpenAL backend's own room from the map's audio zones or the traced room. The room reverb itself follows s_alReverb." );
	s_show_sounds = Cvar_Get( "s_show_sounds", "0", 0 );
	Cvar_SetDescription( s_show_sounds, "Prints every American McGee's Alice sound as it starts and stops." );
	fade = sfx::Fade{};
	ambient = 1.0f;
	reverbType = 0;
	reverbLevel = 0.0f;
	volumeSeen = s_volume ? s_volume->modificationCount : -1;
	engine.Reset( s_volume ? s_volume->value : 1.0f );
	LoadSoundInfos();
}

/* Once a backend runs: retail adds its commands with Miles (0x0046e793) and
 * in S_Init (0x0044c811). */
extern "C" void S_AliceRegisterCommands( void ) {
	if ( !Com_GameIsRitual() ) return;
	Cmd_AddCommand( "play", Play_f );
	Cmd_AddCommand( "sounddump", SoundDump_f );
	Cmd_AddCommand( "playsong", PlaySong_f );
	Cmd_AddCommand( "loadsoundtrack", LoadSoundtrack_f );
	Cmd_AddCommand( "playmp3", PlayMP3_f );
	Cmd_AddCommand( "stopmp3", StopMP3_f );
}

/* The part of S_Update (0x00471090) before the backend mixes: the ambient
 * level, pausing, the fade and s_volume, and reaping the loops. */
extern "C" void S_AliceSoundUpdate( void ) {
	if ( !Com_GameIsRitual() ) return;
	sfx::Device *device = S_AliceDevice();
	if ( !device ) return;
	/* [0x00c301e8] is taken to be the key catchers: its readers are the menu
	 * and key code. */
	ambient = Key_GetCatcher() ? sfx::KeyCatcherAmbient : 1.0f;
	fade.Update( cls.realtime );
	bool volumeModified = false;
	if ( s_volume && s_volume->modificationCount != volumeSeen ) {
		volumeSeen = s_volume->modificationCount;
		volumeModified = true;
	}
	/* While the fade is on, and when s_volume changes, S_Update has the
	 * music give its players the master volume again (0x0047112c,
	 * 0x0047113f); the music update later in this frame, S_AliceMusicUpdate
	 * after the backend's, serves it as retail's does at 0x00471255. */
	if ( fade.active || volumeModified ) music.Reapply();
	EngineLog log;
	engine.Update( *device, log, cl_paused && cl_paused->integer, volumeModified, MakeFrame() );
}

/* S_StopAllSounds and S_DisableSounds: every voice ends and every loop
 * record goes (0x004713f0). */
extern "C" void S_AliceSoundStopAll( void ) {
	if ( !Com_GameIsRitual() ) return;
	sfx::Device *device = S_AliceDevice();
	if ( device ) engine.StopAll( *device );
}

/* S_Shutdown. Retail's own shutdown (0x0046e260) keeps the soundtrack's
 * name, table, moods and saved places and forgets the current song
 * (0x0046e2a5); the song images and the parked decoders here stay with them,
 * the two players and the stream let go, and the next start picks up where
 * the manager is. */
extern "C" void S_AliceSoundShutdown( void ) {
	if ( !Com_GameIsRitual() ) return;
	Cmd_RemoveCommand( "sounddump" );
	Cmd_RemoveCommand( "playsong" );
	Cmd_RemoveCommand( "loadsoundtrack" );
	Cmd_RemoveCommand( "playmp3" );
	Cmd_RemoveCommand( "stopmp3" );
	player.CloseLive();
	music.DetachPlayers();
	ReleaseMP3();
	engine.Reset( s_volume ? s_volume->value : 1.0f );
}

int S_AliceListenerNumber( void ) {
	return engine.Listener();
}

/* The room the client game last named, for the OpenAL backend's environment.
 * A level of 0 names none: the player starts there, and no shipped map has a
 * reverb trigger to raise it. */
bool S_AliceNamedRoom( int *type, float *level ) {
	if ( type ) *type = reverbType;
	if ( level ) *level = reverbLevel;
	return Com_GameIsRitual() && s_reverb && s_reverb->integer != 0 &&
		reverbLevel > 0.0f && fntech3::audio::alice::sfx::NearestReverbPreset( reverbType ) != nullptr;
}
