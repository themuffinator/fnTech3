/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		snd_dma.cpp
 *
 * desc:		main control for any streaming sound output device
 *
 * $Archive: /MissionPack/code/client/snd_dma.c $
 *
 *****************************************************************************/

extern "C" {
#include "../snd_local.h"
#include "../codecs/snd_codec.h"
#include "../../client.h"
}

#include "../../client_cpp.h"
#include "../shared/AudioSpeakerLayout.h"
#include "../shared/AudioInitPolicy.h"
#include "../shared/AudioVoiceQueue.h"
#include "../shared/AudioMix.h"
#include "../snd_rtcw.h"
#include "../shared/RTCWStreamFiles.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <new>

static void S_Update_( int msec );
static void S_Base_UpdateBackgroundTrack( void );
static void S_Base_StopAllSounds( void );
static void S_Base_StopBackgroundTrack( void );
static void S_memoryLoad( sfx_t *sfx );
static void S_Base_RawSamplesToBuffer( int &rawEnd, portable_samplepair_t *buffer, int samples, int rate, int width, int channels, const byte *data, float volume );

namespace {

static fntech3::audio::rtcw::OneShots rtcwOneShots;
static std::array<vec3_t, MAX_GENTITIES> rtcwEntityPositions;
static_assert(MAX_CHANNELS == fntech3::audio::rtcw::OneShots::Capacity);

static snd_stream_t *s_backgroundStream = nullptr;
static std::array<char, MAX_QPATH> s_backgroundLoop;
//static char		s_backgroundMusic[MAX_QPATH]; //TTimo: unused

static std::array<byte, 0x10000> mutedPaintBuffer; // for muted painting
static std::unique_ptr<byte[]> mutedPaintHeapBuffer;

constexpr int kSoundFullVolume = 80;
constexpr float kSoundAttenuate = 0.0008f;
constexpr int kMasterVolume = 127;
constexpr int kSphereVolume = 90;
constexpr int kMaxSfx = 4096;
constexpr int kLoopHash = 128;

static int S_VolumeToMasterVolume( float volume )
{
	if ( !( volume > 0.0f ) ) {
		return 0;
	}
	if ( volume > 2.0f ) {
		volume = 2.0f;
	}

	return ( std::min )( 255, static_cast<int>( static_cast<float>( kMasterVolume ) * volume + 0.5f ) );
}

static channel_t *GetChannelFreeLink( const channel_t *channel )
{
	return fnql::ReadUnaligned<channel_t *>( channel );
}

static void SetChannelFreeLink( channel_t *channel, channel_t *next )
{
	fnql::WriteUnaligned( channel, next );
}

} // namespace

extern "C" {
byte			*dma_buffer2;
}

// =======================================================================
// Internal sound data & structures
// =======================================================================

extern "C" {
channel_t   s_channels[MAX_CHANNELS];
channel_t   loop_channels[MAX_CHANNELS];
int			numLoopChannels;
}

static bool s_soundStarted;
static bool s_soundMuted;

extern "C" {
dma_t		dma;
}

static int			listener_number;
static vec3_t		listener_origin;
static std::array<vec3_t, 3> listener_axis;

extern "C" {
int			s_soundtime;		// sample PAIRS
int   		s_paintedtime; 		// sample PAIRS
}

// kMaxSfx may be larger than MAX_SOUNDS because
// of custom player sounds
static std::array<sfx_t, kMaxSfx> s_knownSfx;
static int s_numSfx = 0;

static std::array<sfx_t *, kLoopHash> sfxHash;

extern "C" {
cvar_t		*s_testsound;
cvar_t		*s_khz;
cvar_t		*s_show;
}
static cvar_t *s_mixahead;
static cvar_t *s_mixOffset;
static cvar_t *s_mixPreStep;
static cvar_t *s_debugTiming;
#if defined(__linux__) && !defined(USE_SDL)
extern "C" {
cvar_t		*s_device;
}
#endif

static std::array<loopSound_t, MAX_GENTITIES> loopSounds;
static fntech3::audio::rtcw::LoopFrame rtcwLoops;
static void S_UpdateWolfETMasterFade( void );
// Set when an ET fade was told to stop what it faded once it completes.
static bool s_wolfetStopAfterFade = false;
static	channel_t	*freelist = nullptr;

static sfx_t *S_KnownSfxAt( sfxHandle_t handle )
{
	return &s_knownSfx[ handle ];
}

static sfxHandle_t S_HandleForKnownSfx( const sfx_t *sfx )
{
	return static_cast<sfxHandle_t>( sfx - s_knownSfx.data() );
}

extern "C" {
int			s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];
}

// SP reset mode 1 discards effect/raw streams while retaining queued music.
// A separate ring also prevents a cinematic producer from overwriting music.
static int s_rtcwMusicRawEnd;
static std::array<portable_samplepair_t, MAX_RAW_SAMPLES> s_rtcwMusicRawSamples;

extern "C" void S_PaintRTCWMusic( int first, int end, portable_samplepair_t *paint ) {
	const int available = static_cast<int32_t>(static_cast<uint32_t>(s_rtcwMusicRawEnd) - static_cast<uint32_t>(first));
	const int requested = static_cast<int32_t>(static_cast<uint32_t>(end) - static_cast<uint32_t>(first));
	if (!paint || available <= 0 || available > MAX_RAW_SAMPLES || requested <= 0) return;
	const int count = (std::min)(available, requested);
	for (int i = 0; i < count; ++i) {
		const auto &sample = s_rtcwMusicRawSamples[(static_cast<uint32_t>(first) + i) & (MAX_RAW_SAMPLES - 1)];
		paint[i].left = fntech3::audio::AddMixSample(paint[i].left, sample.left);
		paint[i].right = fntech3::audio::AddMixSample(paint[i].right, sample.right);
	}
}

namespace {

struct RTCWMusicOutput {
	bool Ready() const { return s_soundStarted; }
	void Close( bool clearBuffered ) {
		if ( s_backgroundStream ) S_CodecCloseStream( s_backgroundStream );
		s_backgroundStream = nullptr;
		if ( clearBuffered ) s_rtcwMusicRawEnd = 0;
	}
	int Open( const char *name ) {
		s_backgroundStream = S_CodecOpenStream( name );
		if ( !s_backgroundStream ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", name );
			return 0;
		}
		return s_backgroundStream->info.rate;
	}
	std::int64_t Clock() const { return s_soundtime; }
	void CurrentMusic( const char *name ) { Cvar_Set( "s_currentMusic", name ); }
};

struct LegacyVoiceLane {
	fnql_audio_voice::LaneActivity activity;
	int startSample = 0;
	int endSample = 0;
	std::array<short, fnql_audio_voice::kLaneSampleCapacity> samples{};
};

static std::array<LegacyVoiceLane, fnql_audio_voice::kMaxLanes> s_voiceLanes;

static void ResetVoiceLanes() {
	s_voiceLanes.fill( LegacyVoiceLane() );
}

static int MixPreStepSamples() {
	const float preStep = ( s_mixPreStep != nullptr ) ? s_mixPreStep->value : 0.0f;
	return static_cast<int>( preStep * static_cast<float>( dma.speed ) );
}

static void S_Base_AddVoiceSamples( int clientNum, int samples, int rate, const short *data ) {
	if ( !s_soundStarted || s_soundMuted || dma.speed <= 0 || data == nullptr || samples <= 0 || rate <= 0 ) {
		return;
	}

	std::array<fnql_audio_voice::LaneActivity, fnql_audio_voice::kMaxLanes> activity;
	for ( size_t i = 0; i < s_voiceLanes.size(); ++i ) {
		s_voiceLanes[i].activity.queued = s_voiceLanes[i].endSample > s_voiceLanes[i].startSample;
		activity[i] = s_voiceLanes[i].activity;
	}

	const int now = Sys_Milliseconds();
	const int laneIndex = fnql_audio_voice::SelectLane( activity.data(), static_cast<int>( activity.size() ), clientNum, now );
	if ( laneIndex < 0 ) {
		Com_DPrintf( "S_AddVoiceSamples: all remote-voice lanes are busy; dropping client %i packet\n", clientNum );
		return;
	}

	LegacyVoiceLane &lane = s_voiceLanes[static_cast<size_t>( laneIndex )];
	if ( lane.activity.clientNum != clientNum ) {
		lane = LegacyVoiceLane();
	}
	if ( lane.startSample < s_paintedtime ) {
		lane.startSample = ( std::min )( s_paintedtime, lane.endSample );
	}

	if ( lane.endSample <= lane.startSample ) {
		const int voiceStepSamples = static_cast<int>(
			( ( s_voiceStep != nullptr ) ? s_voiceStep->value : 0.0f ) * static_cast<float>( dma.speed ) );
		lane.startSample = ( std::max )( s_paintedtime, s_soundtime ) + MixPreStepSamples() + voiceStepSamples;
		lane.endSample = lane.startSample;
	}

	const int queuedSamples = ( std::max )( 0, lane.endSample - lane.startSample );
	const int room = fnql_audio_voice::kLaneSampleCapacity - ( std::min )( queuedSamples, fnql_audio_voice::kLaneSampleCapacity );
	if ( room <= 0 ) {
		Com_DPrintf( "S_AddVoiceSamples: remote-voice lane %i is full; dropping client %i packet\n", laneIndex, clientNum );
		return;
	}

	std::array<short, fnql_audio_voice::kLaneSampleCapacity> converted{};
	const int convertedSamples = fnql_audio_voice::ResampleMonoPCM16(
		data, samples, rate, converted.data(), room, dma.speed );
	for ( int i = 0; i < convertedSamples; ++i ) {
		lane.samples[static_cast<size_t>( lane.endSample ) & ( fnql_audio_voice::kLaneSampleCapacity - 1 )] = converted[static_cast<size_t>( i )];
		++lane.endSample;
	}

	lane.activity.clientNum = clientNum;
	lane.activity.queued = lane.endSample > lane.startSample;
	lane.activity.lastPacketMs = now;
	if ( convertedSamples < fnql_audio_voice::ResampledFrameCount( samples, rate, dma.speed ) ) {
		Com_DPrintf( "S_AddVoiceSamples: remote-voice lane %i overflowed; truncated client %i packet\n", laneIndex, clientNum );
	}
}

} // namespace

#include "snd_rtcw_streams.hpp"
#include "snd_rtcw_voice.hpp"

extern "C" void S_PaintVoiceSamples( int starttime, int endtime, portable_samplepair_t *paintSamples ) {
	if ( paintSamples == nullptr || endtime <= starttime ) {
		return;
	}

	const float voiceVolume = ( s_voiceVolume != nullptr ) ? s_voiceVolume->value : 1.0f;
	const int voiceScale = static_cast<int>( ( std::max )( 0.0f, ( std::min )( voiceVolume, 2.0f ) ) * 256.0f );
	for ( LegacyVoiceLane &lane : s_voiceLanes ) {
		if ( lane.startSample < starttime ) {
			lane.startSample = ( std::min )( starttime, lane.endSample );
		}
		const int voiceStart = ( std::max )( starttime, lane.startSample );
		const int voiceEnd = ( std::min )( endtime, lane.endSample );
		for ( int sampleTime = voiceStart; sampleTime < voiceEnd; ++sampleTime ) {
			const int sample = lane.samples[static_cast<size_t>( sampleTime ) & ( fnql_audio_voice::kLaneSampleCapacity - 1 )];
			portable_samplepair_t &output = paintSamples[sampleTime - starttime];
			output.left += sample * voiceScale;
			output.right += sample * voiceScale;
		}
		lane.startSample = ( std::max )( lane.startSample, voiceEnd );
		lane.activity.queued = lane.endSample > lane.startSample;
	}
}


// ====================================================================
// User-setable variables
// ====================================================================


static void S_Base_SoundInfo( void ) {
	Com_Printf( "----- Sound Info -----\n" );
	if ( !s_soundStarted ) {
		Com_Printf( "sound system not started\n" );
	} else {
		const bool focusMuted =
			( !gw_active && !gw_minimized && s_muteWhenUnfocused != nullptr && s_muteWhenUnfocused->integer ) ||
			( gw_minimized && s_muteWhenMinimized != nullptr && s_muteWhenMinimized->integer );
		Com_Printf( "Backend: legacy\n" );
		Com_Printf( "Master volume: %.2f\n", s_volume != nullptr ? s_volume->value : 1.0f );
		Com_Printf( "Mute state: %s%s\n",
			s_soundMuted ? "registration-muted (normal during startup/registration)" :
				( focusMuted ? "focus-muted" : "audible" ),
			( s_volume != nullptr && s_volume->value <= 0.0f ) ? ", master volume is zero" : "" );
		Com_Printf("%5d channels\n", dma.channels);
		Com_Printf("%5d samples\n", dma.samples);
		Com_Printf("%5d samplebits (%s)\n", dma.samplebits, dma.isfloat ? "float" : "int");
		Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
		Com_Printf("%5d speed\n", dma.speed);
		Com_Printf("%p dma buffer\n", dma.buffer);
		if ( dma.driver ) {
			Com_Printf( "Using %s subsystem\n", dma.driver );
		}
		if ( s_backgroundStream ) {
			Com_Printf("Background file: %s\n", Com_GameIsRTCWSP() ? S_RTCWMusic().Name().c_str() : s_backgroundLoop.data() );
		} else {
			Com_Printf("No background file.\n" );
		}
		int activeVoiceLanes = 0;
		for ( const LegacyVoiceLane &lane : s_voiceLanes ) {
			if ( lane.endSample > lane.startSample ) {
				++activeVoiceLanes;
			}
		}
		Com_Printf( "Remote voice lanes: %i allocated, %i active\n",
			fnql_audio_voice::kMaxLanes, activeVoiceLanes );
		if ( Com_GameIsRTCW() ) {
			int active = 0, buffered = 0;
			for ( int i = 1; i < rtcwStreams.policy.Limit(); ++i ) {
				active += rtcwStreams.policy[i].active;
				const int queued = static_cast<int32_t>( static_cast<uint32_t>(rtcwStreamRaw[i].end) - static_cast<uint32_t>(s_soundtime) );
				buffered += rtcwStreamRaw[i].buffered && queued > 0 && queued <= MAX_RAW_SAMPLES;
			}
			Com_Printf( "RTCW entity streams: %d active, %d buffered\n", active, buffered );
		}

	}
	Com_Printf("----------------------\n" );
}


/*
=================
S_Base_SoundList
=================
*/
static void S_Base_SoundList( void ) {
	int		i;
	const sfx_t *sfx;
	int		size, total;
	const char *type[4] = { "16bit", "adpcm", "daub4", "mulaw" };
	const char *mem[2] = { "paged out", "resident " };

	total = 0;
	for (sfx=s_knownSfx.data(), i=0 ; i<s_numSfx ; i++, sfx++) {
		size = sfx->soundLength;
		total += size;
		Com_Printf("%6i[%s] : %s[%s]\n", size,
				type[sfx->soundCompressionMethod],
				sfx->soundName, mem[sfx->inMemory] );
	}
	Com_Printf ("Total resident: %i\n", total);
	S_DisplayFreeMemory();
}


static void S_ChannelFree( channel_t *v ) {
	v->thesfx = nullptr;
	if ( Com_GameIsRTCW() ) {
		rtcwOneShots.Release(static_cast<int>(v - s_channels));
		return;
	}
	SetChannelFreeLink( v, freelist );
	freelist = v;
}


static channel_t* S_ChannelMalloc( int allocTime ) {
	channel_t *v;
	if (freelist == nullptr) {
		return nullptr;
	}
	v = freelist;
	freelist = GetChannelFreeLink( freelist );
	v->allocTime = allocTime;
	return v;
}


static void S_ChannelSetup( void ) {
	channel_t *p, *q;
	rtcwOneShots.Reset();

	// clear all the sounds
	std::fill_n( s_channels, MAX_CHANNELS, channel_t{} );

	p = s_channels;
	q = p + MAX_CHANNELS;
	while (--q > p) {
		SetChannelFreeLink( q, q - 1 );
	}

	SetChannelFreeLink( q, nullptr );
	freelist = p + MAX_CHANNELS - 1;
	Com_DPrintf("Channel memory manager started\n");
}



// =======================================================================
// Load a sound
// =======================================================================

/*
================
return a hash value for the sfx name
================
*/
static unsigned int S_HashSFXName(const char *name) {
	unsigned int hash;
	char	letter;
	int		i;

	hash = 0;
	i = 0;
	while (name[i] != '\0') {
		letter = static_cast<char>( std::tolower( static_cast<unsigned char>( name[i] ) ) );
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash += static_cast<int>( letter ) * ( i + 119 );
		i++;
	}
	hash &= ( kLoopHash - 1 );
	return hash;
}


/*
==================
S_FindName

Will allocate a new sfx if it isn't found
==================
*/
static sfx_t *S_FindName( const char *name ) {
	int		i;
	int		hash;

	sfx_t	*sfx;

	if ( !name ) {
		Com_Error( ERR_FATAL, "Sound name is NULL" );
	}

	if ( !name[0] ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Sound name is empty\n" );
		return nullptr;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Sound name is too long: %s\n", name );
		return nullptr;
	}

	if ( name[0] == '*' ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Tried to load player sound directly: %s\n", name );
		return nullptr;
	}

	hash = S_HashSFXName( name );

	sfx = sfxHash[hash];
	// see if already loaded
	while (sfx) {
		if (!Q_stricmp(sfx->soundName, name) ) {
			return sfx;
		}
		sfx = sfx->next;
	}

	// find a free sfx
	for ( i=0 ; i < s_numSfx ; i++) {
		if (!s_knownSfx[i].soundName[0]) {
			break;
		}
	}

	if (i == s_numSfx) {
		if (s_numSfx >= kMaxSfx) {
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		}
		s_numSfx++;
	}
	
	sfx = S_KnownSfxAt( i );
	*sfx = {};
	Q_strncpyz( sfx->soundName, name, sizeof( sfx->soundName ) );

	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}


/*
===================
S_DisableSounds

Disables sounds until the next S_BeginRegistration.
This is called when the hunk is cleared and the sounds
are no longer valid.
===================
*/
static void S_Base_DisableSounds( void ) {
	S_Base_StopAllSounds();
	s_soundMuted = true;
}


/*
==================
S_RegisterSound

Creates a default buzz sound if the file can't be loaded
==================
*/
static sfxHandle_t S_Base_RegisterSound( const char *name, qboolean compressed ) {
	sfx_t	*sfx;

	compressed = qfalse;
	if (!s_soundStarted) {
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}

	sfx = S_FindName( name );
	if ( !sfx ) {
		return 0;
	}

	if ( sfx->soundData ) {
		if ( sfx->defaultSound ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
			return 0;
		}
		return S_HandleForKnownSfx( sfx );
	}

	sfx->inMemory = qfalse;
	sfx->soundCompressed = compressed;

	S_memoryLoad( sfx );

	if ( sfx->defaultSound ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
		return 0;
	}

	return S_HandleForKnownSfx( sfx );
}


/*
=====================
S_BeginRegistration
=====================
*/
static void S_Base_LoadRTCWDefaultSound( sfx_t *sample ) {
	const auto pcm = fntech3::audio::rtcw::DefaultSoundPCM(Com_GameIsRTCWSP() != 0,
		Cvar_VariableIntegerValue("s_defaultsound") != 0);
	sample->soundData = SND_malloc();
	*sample->soundData = {};
	std::copy(pcm.begin(), pcm.end(), sample->soundData->sndChunk);
	sample->soundLength = static_cast<int>(pcm.size());
	sample->soundChannels = 1;
	sample->soundCompressionMethod = 0;
	sample->defaultSound = qtrue;
	sample->inMemory = qtrue;
}

static void S_Base_BeginRegistration( void ) {
	s_soundMuted = false;		// we can play again

	if ( s_numSfx )
		return;

	SND_setup();

	s_knownSfx.fill( sfx_t{} );
	sfxHash.fill( nullptr );

	if ( Com_GameIsRTCW() ) {
		// Reserve handle zero directly: the public name lookup rejects '*'
		// player aliases and must never try to load this internal placeholder.
		auto *sample = S_KnownSfxAt(s_numSfx++);
		Q_strncpyz(sample->soundName, fntech3::audio::rtcw::DefaultSoundName, sizeof(sample->soundName));
		S_Base_LoadRTCWDefaultSound(sample);
	} else S_Base_RegisterSound( "sound/feedback/hit.wav", qfalse );
}


static void S_memoryLoad( sfx_t *sfx ) {
	if ( Com_GameIsRTCW() && !Q_stricmp(sfx->soundName, fntech3::audio::rtcw::DefaultSoundName) ) {
		S_Base_LoadRTCWDefaultSound(sfx);
		return;
	}

	// load the sound file
	if ( !S_LoadSound ( sfx ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: couldn't load sound: %s\n", sfx->soundName );
		sfx->defaultSound = qtrue;
	}

	sfx->inMemory = qtrue;
}

//=============================================================================

/*
=================
S_SpatializeOrigin

Used for spatializing s_channels
=================
*/
static void S_SpatializeOrigin( const vec3_t origin, int master_vol, int *left_vol, int *right_vol )
{
	vec_t	dot;
	vec_t	dist;
	vec_t	lscale, rscale, scale;
	vec3_t	source_vec;
	vec3_t	vec;

	const float dist_mult = kSoundAttenuate;
	
	// calculate stereo separation and distance attenuation
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= kSoundFullVolume;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	dist *= dist_mult;		// different attenuation levels
	
	VectorRotate( source_vec, listener_axis.data(), vec );

	dot = -vec[1];

	if (dma.channels == 1)
	{ // no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5 * (1.0 - dot);
		if ( rscale < 0.0 ) {
			rscale = 0.0;
		}
		if ( lscale < 0.0 ) {
			lscale = 0.0;
		}
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (master_vol * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (master_vol * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}


// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_Base_StartSound

Validates the parms and ques the sound up
if origin is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
static void S_SpatializeRTCWOneShot( channel_t &voice ) {
	if ( voice.entnum == listener_number ) {
		voice.leftvol = voice.rightvol = voice.master_vol;
		return;
	}
	const auto gains = fntech3::audio::rtcw::SpatializeOrigin(
		voice.fixed_origin ? voice.origin : rtcwEntityPositions[voice.entnum],
		1250.0f, voice.master_vol, listener_origin, listener_axis.data(), dma.channels == 1);
	voice.leftvol = gains.left; voice.rightvol = gains.right;
}

static void S_Base_StartRTCWSound( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags ) {
	if ( !s_soundStarted || s_soundMuted ) return;
	if ( !origin && (entity < 0 || entity >= MAX_GENTITIES) )
		Com_Error(ERR_DROP, "S_StartRTCWSound: bad entitynum %i", entity);
	if ( handle < 0 || handle >= s_numSfx ) {
		Com_Printf(S_COLOR_YELLOW "S_StartRTCWSound: handle %i out of range\n", handle);
		return;
	}
	auto *sample = S_KnownSfxAt(handle);
	rtcwStreams.policy.StopChannel( entity, channel );
	if ( !sample->inMemory ) S_memoryLoad(sample);
	const int now = Sys_Milliseconds();
	sample->lastTimeUsed = now;
	const int slot = rtcwOneShots.Start(Com_GameIsRTCWSP(), 128, listener_number, now,
		{entity, channel, handle, flags}, [](int index) { s_channels[index].thesfx = nullptr; });
	if ( slot < 0 ) return;
	auto &voice = s_channels[slot];
	voice = {};
	voice.allocTime = rtcwOneShots[slot].allocated;
	voice.entnum = entity;
	voice.entchannel = channel;
	voice.thesfx = sample;
	voice.master_vol = kMasterVolume;
	voice.fixed_origin = origin ? qtrue : qfalse;
	if ( origin ) VectorCopy(origin, voice.origin);
	S_SpatializeRTCWOneShot(voice);
	voice.startSample = START_SAMPLE_IMMEDIATE;
}

static void S_Base_StartRTCWLocalSound( sfxHandle_t handle, int channel ) {
	S_Base_StartRTCWSound(nullptr, listener_number, channel, handle, 0);
}

// ET submits a volume with every one-shot. Its own modules pass 127 unless a
// caller chose otherwise, so the value is used as given rather than replaced
// by the Wolfenstein default.
static void S_Base_StartWolfETSound( const vec3_t origin, int entity, int channel,
	sfxHandle_t handle, int flags, int volume ) {
	if ( !s_soundStarted || s_soundMuted ) return;
	if ( !origin && (entity < 0 || entity >= MAX_GENTITIES) )
		Com_Error(ERR_DROP, "S_StartWolfETSound: bad entitynum %i", entity);
	if ( handle < 0 || handle >= s_numSfx ) {
		Com_Printf(S_COLOR_YELLOW "S_StartWolfETSound: handle %i out of range\n", handle);
		return;
	}
	if ( volume <= 0 ) return;
	auto *sample = S_KnownSfxAt(handle);
	rtcwStreams.policy.StopChannel( entity, channel );
	if ( !sample->inMemory ) S_memoryLoad(sample);
	const int now = Sys_Milliseconds();
	sample->lastTimeUsed = now;
	const int slot = rtcwOneShots.Start(false, 128, listener_number, now,
		{entity, channel, handle, flags}, [](int index) { s_channels[index].thesfx = nullptr; });
	if ( slot < 0 ) return;
	auto &voice = s_channels[slot];
	voice = {};
	voice.allocTime = rtcwOneShots[slot].allocated;
	voice.entnum = entity;
	voice.entchannel = channel;
	voice.thesfx = sample;
	voice.master_vol = volume;
	voice.fixed_origin = origin ? qtrue : qfalse;
	if ( origin ) VectorCopy(origin, voice.origin);
	S_SpatializeRTCWOneShot(voice);
	voice.startSample = START_SAMPLE_IMMEDIATE;
}

static void S_Base_StartWolfETLocalSound( sfxHandle_t handle, int channel, int volume ) {
	S_Base_StartWolfETSound(nullptr, listener_number, channel, handle, 0, volume);
}

static void S_Base_StartSound( const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle, float volume ) {
	channel_t	*ch;
	sfx_t		*sfx;
	int i, oldest, chosen, startTime;
	int	inplay, allowed;
	const int masterVolume = S_VolumeToMasterVolume( volume );

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum >= MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = S_KnownSfxAt( sfxHandle );

	if ( sfx->inMemory == qfalse ) {
		S_memoryLoad(sfx);
	}

	if ( masterVolume <= 0 ) {
		return;
	}

	if ( s_show->integer == 1 ) {
		Com_Printf( "%i : %s\n", s_paintedtime, sfx->soundName );
	}

	startTime = s_soundtime; // Com_Milliseconds();

	// borrowed from cnq3
	// a UNIQUE entity starting the same sound twice in a frame is either a bug,
	// a timedemo, or a shitmap (eg q3ctf4) giving multiple items on spawn.
	// even if you can create a case where it IS "valid", it's still pointless
	// because you implicitly can't DISTINGUISH between the sounds:
	// all that happens is the sound plays at double volume, which is just annoying

	if ( entityNum != ENTITYNUM_WORLD ) {
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->entnum != entityNum )
				continue;
			if ( ch->allocTime != startTime )
				continue;
			if ( ch->thesfx != sfx )
				continue;
			sfx->lastTimeUsed = startTime;
			//Com_Printf( S_COLOR_YELLOW "double sound start: %d %s\n", entityNum, sfx->soundName);
			return;
		}
	}

//	Com_Printf("playing %s\n", sfx->soundName);
	// pick a channel to play on

	// try to limit sound duplication
	if ( entityNum == listener_number )
		allowed = 16;
	else
		allowed = 8;

	ch = s_channels;
	inplay = 0;
	for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( ch->entnum == entityNum && ch->thesfx == sfx ) {
			if ( startTime - ch->allocTime < 20 ) {
				Com_DPrintf(S_COLOR_YELLOW "S_StartSound: Double start (%d ms < 20 ms) for %s\n", startTime - ch->allocTime, sfx->soundName);
				return;
			}
			inplay++;
		}
	}

	// too much duplicated sounds, ignore
	if ( inplay > allowed ) {
		Com_DPrintf(S_COLOR_YELLOW "S_StartSound: %s hit the concurrent channels limit (%d)\n", sfx->soundName, allowed);
		return;
	}

	sfx->lastTimeUsed = startTime;

	ch = S_ChannelMalloc( startTime ); // entityNum, entchannel);
	if (!ch) {
		ch = s_channels;

		oldest = sfx->lastTimeUsed;
		chosen = -1;
		for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
			if (ch->entnum != listener_number && ch->entnum == entityNum && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER) {
				oldest = ch->allocTime;
				chosen = i;
			}
		}
		if (chosen == -1) {
			ch = s_channels;
			for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
				if (ch->entnum != listener_number && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER) {
					oldest = ch->allocTime;
					chosen = i;
				}
			}
			if (chosen == -1) {
				ch = s_channels;
				if (ch->entnum == listener_number) {
					for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
						if ( ch->allocTime - oldest < 0 ) {
							oldest = ch->allocTime;
							chosen = i;
						}
					}
				}
				if (chosen == -1) {
					Com_DPrintf(S_COLOR_YELLOW "S_StartSound: No more channels free for %s\n", sfx->soundName);
					return;
				}
			}
		}
		ch = &s_channels[chosen];
		ch->allocTime = sfx->lastTimeUsed;
		Com_DPrintf(S_COLOR_YELLOW "S_StartSound: No more channels free for %s, dropping earliest sound: %s\n", sfx->soundName, ch->thesfx->soundName);
	}

	if ( origin ) {
		VectorCopy( origin, ch->origin );
		ch->fixed_origin = qtrue;
	} else {
		ch->fixed_origin = qfalse;
	}

	ch->master_vol = masterVolume;
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;		// these will get calced at next spatialize
	ch->rightvol = ch->master_vol;		// unless the game isn't running
	ch->doppler = qfalse;
}


/*
==================
S_StartLocalSound
==================
*/
static void S_Base_StartLocalSound( sfxHandle_t sfxHandle, int channelNum, float volume ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}

	S_Base_StartSound( nullptr, listener_number, channelNum, sfxHandle, volume );
}


/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
static void S_Base_ClearSoundBuffer( void ) {
	int		clear;
		
	if (!s_soundStarted)
		return;

	// stop looping sounds
	loopSounds.fill( loopSound_t{} );
	rtcwLoops.Clear();
	std::fill_n( loop_channels, MAX_CHANNELS, channel_t{} );
	numLoopChannels = 0;

	S_ChannelSetup();

	s_rawend = 0;
	s_rtcwMusicRawEnd = 0;
	ResetVoiceLanes();
	S_Base_ClearRTCWEntityStreams();

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SNDDMA_BeginPainting();
	
	if ( dma.buffer )
		std::fill_n( dma.buffer, dma.samples * dma.samplebits / 8, static_cast<byte>( clear ) );

	SNDDMA_Submit();
}


/*
==================
S_StopAllSounds
==================
*/
static void S_Base_StopAllSounds( void ) {
	if ( !s_soundStarted ) {
		return;
	}

	// stop the background music
	if ( Com_GameIsRTCWSP() ) {
		RTCWMusicOutput output;
		S_RTCWMusic().Clear( output );
	} else S_Base_StopBackgroundTrack();

	S_Base_ClearSoundBuffer();
}

static void S_Base_ClearRTCWSounds( qboolean clearMusic ) {
	if (!s_soundStarted) return;
	if (clearMusic) {
		S_Base_StopAllSounds();
		return;
	}
	loopSounds.fill(loopSound_t{});
	rtcwLoops.Clear();
	std::fill_n(loop_channels, MAX_CHANNELS, channel_t{});
	numLoopChannels = 0;
	S_ChannelSetup();
	s_rawend = 0;
	ResetVoiceLanes();
	S_Base_ClearRTCWEntityStreams();
	S_Base_UpdateBackgroundTrack();
}


/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/

static void S_Base_StopLoopingSound(int entityNum) {
	loopSounds[entityNum].active = qfalse;
//	loopSounds[entityNum].sfx = 0;
	loopSounds[entityNum].kill = qfalse;
}


/*
==================
S_ClearLoopingSoundsFrame
==================
*/
static void S_Base_ClearLoopingSoundsFrame( void ) {
	rtcwLoops.Clear();
	for ( loopSound_t &loop : loopSounds ) {
		loop.active = qfalse;
	}
	numLoopChannels = 0;
}


/*
==================
S_ClearLoopingSounds
==================
*/
static void S_Base_ClearLoopingSounds( qboolean killall ) {
	int i;
	rtcwLoops.Clear();
	for ( i = 0 ; i < MAX_GENTITIES ; i++) {
		if (killall || loopSounds[i].kill == qtrue || (loopSounds[i].sfx && loopSounds[i].sfx->soundLength == 0)) {
			S_Base_StopLoopingSound(i);
		}
	}
	numLoopChannels = 0;
}


/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
static void S_Base_AddRTCWLoopingSound( const vec3_t origin, const vec3_t velocity,
	int range, sfxHandle_t handle, int volume ) {
	if ( !s_soundStarted || s_soundMuted || cls.state != CA_ACTIVE || !rtcwLoops.Accepts( volume ) ) return;
	if ( handle < 0 || handle >= s_numSfx )
		Com_Error( ERR_DROP, "RTCW looping sound handle %d is invalid", handle );
	sfx_t *sample = S_KnownSfxAt( handle );
	if ( !sample->inMemory ) S_memoryLoad( sample );
	if ( !sample->soundLength ) Com_Error( ERR_DROP, "RTCW looping sound has no samples" );
	rtcwLoops.Append( origin, velocity, range, handle, volume,
		Com_GameIsRTCWSP() ? S_RTCWMasterFade().current : 1.0f );
}

// ET also submits the time each loop is considered to have started, so every
// listener hears it at the same point in its cycle instead of wherever the
// local mixer clock happens to be.
static void S_Base_AddWolfETLoopingSound( const vec3_t origin, const vec3_t velocity,
	int range, sfxHandle_t handle, int volume, int soundTime ) {
	if ( !s_soundStarted || s_soundMuted || cls.state != CA_ACTIVE || !rtcwLoops.Accepts( volume ) ) return;
	if ( handle < 0 || handle >= s_numSfx )
		Com_Error( ERR_DROP, "ET looping sound handle %d is invalid", handle );
	sfx_t *sample = S_KnownSfxAt( handle );
	if ( !sample->inMemory ) S_memoryLoad( sample );
	if ( !sample->soundLength ) Com_Error( ERR_DROP, "ET looping sound has no samples" );
	rtcwLoops.Append( origin, velocity, range, handle, volume,
		S_WolfETMasterFade().current, soundTime );
}

static void S_Base_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = S_KnownSfxAt( sfxHandle );

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "%s has length 0", sfx->soundName );
	}

	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].active = qtrue;
	loopSounds[entityNum].kill = qtrue;
	loopSounds[entityNum].doppler = qfalse;
	loopSounds[entityNum].oldDopplerScale = 1.0;
	loopSounds[entityNum].dopplerScale = 1.0;
	loopSounds[entityNum].sfx = sfx;

	if (s_doppler->integer && VectorLengthSquared(velocity)>0.0) {
		vec3_t	out;
		float	lena, lenb;

		loopSounds[entityNum].doppler = qtrue;
		lena = DistanceSquared(loopSounds[listener_number].origin, loopSounds[entityNum].origin);
		VectorAdd(loopSounds[entityNum].origin, loopSounds[entityNum].velocity, out);
		lenb = DistanceSquared(loopSounds[listener_number].origin, out);
		if ((loopSounds[entityNum].framenum+1) != cls.framecount) {
			loopSounds[entityNum].oldDopplerScale = 1.0;
		} else {
			loopSounds[entityNum].oldDopplerScale = loopSounds[entityNum].dopplerScale;
		}
		loopSounds[entityNum].dopplerScale = lenb/(lena*100);
		if (loopSounds[entityNum].dopplerScale<=1.0) {
			loopSounds[entityNum].doppler = qfalse;			// don't bother doing the math
		} else if (loopSounds[entityNum].dopplerScale>MAX_DOPPLER_SCALE) {
			loopSounds[entityNum].dopplerScale = MAX_DOPPLER_SCALE;
		}
	}

	loopSounds[entityNum].framenum = cls.framecount;
}


/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
static void S_Base_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = S_KnownSfxAt( sfxHandle );

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "%s has length 0", sfx->soundName );
	}
	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].sfx = sfx;
	loopSounds[entityNum].active = qtrue;
	loopSounds[entityNum].kill = qfalse;
	loopSounds[entityNum].doppler = qfalse;
}


/*
==================
S_AddLoopSounds

Spatialize all of the looping sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
==================
*/
static void S_AddRTCWLoopSounds( void ) {
	numLoopChannels = 0;
	for ( std::size_t i = 0; i < rtcwLoops.Size() && numLoopChannels < MAX_CHANNELS; ++i ) {
		const auto &loop = rtcwLoops[i];
		const auto gains = fntech3::audio::rtcw::SpatializeLoop( loop,
			listener_origin, listener_axis.data(), dma.channels == 1 );
		sfx_t *sample = S_KnownSfxAt( loop.handle );
		sample->lastTimeUsed = s_soundtime;
		if ( !gains.left && !gains.right ) continue;
		channel_t &channel = loop_channels[numLoopChannels++];
		channel = channel_t{};
		channel.master_vol = kMasterVolume;
		channel.leftvol = gains.left;
		channel.rightvol = gains.right;
		channel.thesfx = sample;
		// Zero for RTCW, which submits no start time and keeps the mixer's own
		// phase; ET's submitted millisecond time becomes a sample offset.
		channel.startSample = loop.startTime ?
			static_cast<int>( static_cast<std::int64_t>( loop.startTime ) * dma.speed / 1000 ) : 0;
	}
}

static void S_AddLoopSounds( void ) {
	if ( Com_GameIsRTCW() ) {
		S_AddRTCWLoopSounds();
		return;
	}
	int			i, j, startTime;
	int			left_total, right_total, left, right;
	channel_t	*ch;
	loopSound_t	*loop, *loop2;
	static int	loopFrame;


	numLoopChannels = 0;

	startTime = s_soundtime; // Com_Milliseconds();

	loopFrame++;
	for ( i = 0 ; i < MAX_GENTITIES ; i++) {
		loop = &loopSounds[i];
		if ( !loop->active || loop->mergeFrame == loopFrame ) {
			continue;	// already merged into an earlier sound
		}

		if (loop->kill) {
			S_SpatializeOrigin( loop->origin, kMasterVolume, &left_total, &right_total);	// 3d
		} else {
			S_SpatializeOrigin( loop->origin, kSphereVolume,  &left_total, &right_total);	// sphere
		}

		loop->sfx->lastTimeUsed = startTime;

		for (j=(i+1); j< MAX_GENTITIES ; j++) {
			loop2 = &loopSounds[j];
			if ( !loop2->active || loop2->doppler || loop2->sfx != loop->sfx) {
				continue;
			}
			loop2->mergeFrame = loopFrame;

			if (loop2->kill) {
				S_SpatializeOrigin( loop2->origin, kMasterVolume, &left, &right);		// 3d
			} else {
				S_SpatializeOrigin( loop2->origin, kSphereVolume,  &left, &right);		// sphere
			}

			loop2->sfx->lastTimeUsed = startTime;
			left_total += left;
			right_total += right;
		}
		if (left_total == 0 && right_total == 0) {
			continue;		// not audible
		}

		// allocate a channel
		ch = &loop_channels[numLoopChannels];
		
		if (left_total > 255) {
			left_total = 255;
		}
		if (right_total > 255) {
			right_total = 255;
		}
		
		ch->master_vol = kMasterVolume;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->thesfx = loop->sfx;
		ch->doppler = loop->doppler;
		ch->dopplerScale = loop->dopplerScale;
		ch->oldDopplerScale = loop->oldDopplerScale;
		numLoopChannels++;
		if ( numLoopChannels >= MAX_CHANNELS ) {
			return;
		}
	}
}

//=============================================================================

extern "C" portable_samplepair_t *S_GetRawSamplePointer( void ) 
{
	return s_rawsamples;
}


/*
============
S_RawSamples

Music streaming
============
*/
static void S_Base_RawSamplesToBuffer( int &rawEnd, portable_samplepair_t *buffer, int samples, int rate, int width, int n_channels, const byte *data, float volume ) {
	if (!s_soundStarted || s_soundMuted || !buffer || !data || samples <= 0 ||
		rate <= 0 || rate > 192000 || dma.speed <= 0 ||
		(width != 1 && width != 2) || (n_channels != 1 && n_channels != 2) || !std::isfinite(volume)) return;

	int queued = static_cast<int32_t>(static_cast<uint32_t>(rawEnd) - static_cast<uint32_t>(s_soundtime));
	if (queued < 0 || queued > MAX_RAW_SAMPLES) {
		rawEnd = s_soundtime;
		queued = 0;
	}
	// Music may combine s_musicVolume 2 with a stream fade of 16.
	const int intVolume = static_cast<int>(256 * std::clamp(volume, -32.0f, 32.0f));
	const float scale = static_cast<float>(rate) / dma.speed;
	for (int i = 0; i < MAX_RAW_SAMPLES - queued; ++i) {
		const float position = i * scale;
		if (static_cast<double>(position) >= samples) break;
		const auto src = static_cast<std::size_t>(position) * n_channels;
		const auto sample = [&](int channel) {
			const auto index = src + (n_channels == 2 ? channel : 0);
			if (width == 2) {
				short value;
				std::memcpy(&value, data + index * 2, sizeof(value));
				return static_cast<int>(value) * intVolume;
			}
			// Preserve the inherited raw API's signed stereo / unsigned mono PCM8 contract.
			const int value = n_channels == 2 ? static_cast<signed char>(data[index]) : data[index] - 128;
			return value * intVolume * 256;
		};
		auto &out = buffer[static_cast<uint32_t>(rawEnd) & (MAX_RAW_SAMPLES - 1)];
		out.left = sample(0);
		out.right = sample(1);
		rawEnd = static_cast<int32_t>(static_cast<uint32_t>(rawEnd) + 1u);
	}
}

static void S_Base_RawSamples( int samples, int rate, int width, int n_channels, const byte *data, float volume ) {
	S_Base_RawSamplesToBuffer(s_rawend, s_rawsamples, samples, rate, width, n_channels, data, volume);
}

//=============================================================================

/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
static void S_Base_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}
	if ( Com_GameIsRTCW() ) VectorCopy(origin, rtcwEntityPositions[entityNum]);
	else VectorCopy( origin, loopSounds[entityNum].origin );
}


/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
static void S_Base_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], int inwater ) {
	int			i;
	channel_t	*ch;
	vec3_t		origin;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	listener_number = entityNum;
	VectorCopy(head, listener_origin);
	VectorCopy(axis[0], listener_axis[0]);
	VectorCopy(axis[1], listener_axis[1]);
	VectorCopy(axis[2], listener_axis[2]);

	// update spatialization for dynamic sounds	
	ch = s_channels;
	for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		if ( Com_GameIsRTCW() ) { S_SpatializeRTCWOneShot(*ch); continue; }
		// anything coming from the view entity will always be full volume
		if (ch->entnum == listener_number) {
			ch->leftvol = ch->master_vol;
			ch->rightvol = ch->master_vol;
		} else {
			if (ch->fixed_origin) {
				VectorCopy( ch->origin, origin );
			} else {
				VectorCopy( loopSounds[ ch->entnum ].origin, origin );
			}
			if ( s_pvs != nullptr && s_pvs->integer && !S_OriginInPVS( listener_origin, origin ) ) {
				ch->leftvol = 0;
				ch->rightvol = 0;
				continue;
			}

			S_SpatializeOrigin (origin, ch->master_vol, &ch->leftvol, &ch->rightvol);
		}
	}

	// add loopsounds
	S_AddLoopSounds ();
}


/*
========================
S_ScanChannelStarts

Returns qtrue if any new sounds were started since the last mix
========================
*/
static bool S_ScanChannelStarts( void ) {
	channel_t		*ch;
	int				i;
	bool			newSamples;

	newSamples = false;
	ch = s_channels;

	for ( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// if this channel was just started this frame,
		// set the sample count to it begins mixing
		// into the very first sample
		if ( ch->startSample == START_SAMPLE_IMMEDIATE ) {
			ch->startSample = s_paintedtime;
			newSamples = true;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + (ch->thesfx->soundLength) - s_soundtime <= 0 ) {
			S_ChannelFree( ch );
		}
	}

	return newSamples;
}


/*
============
S_Update

Called once each time through the main loop
============
*/
static void S_Base_Update( int msec ) {
	int			i;
	int			total;
	channel_t	*ch;

	if ( Com_GameIsWolfET() ) S_UpdateWolfETMasterFade();
	if ( !s_soundStarted || s_soundMuted ) {
//		Com_DPrintf ("not started or muted\n");
		return;
	}

	//
	// debugging output
	//
	if ( s_show->integer == 2 ) {
		total = 0;
		ch = s_channels;
		for (i=0 ; i<MAX_CHANNELS; i++, ch++) {
			if (ch->thesfx && (ch->leftvol || ch->rightvol) ) {
				Com_Printf ("%d %d %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName);
				total++;
			}
		}

		Com_Printf ("----(%i)---- painted: %i\n", total, s_paintedtime);
	}

	// mix some sound
	if ( Com_GameIsRTCW() ) {
		for ( auto &voice : s_channels ) if ( voice.thesfx ) S_SpatializeRTCWOneShot(voice);
	}
	S_Update_( msec );
}


static void S_GetSoundtime( void )
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;

	if ( CL_VideoRecording() )
	{
		const float duration = MAX( static_cast<float>( dma.speed ) / cl_aviFrameRate->value, 1.0f );
		const float frameDuration = duration + clc.aviSoundFrameRemainder;
		const int msec = static_cast<int>( frameDuration );

		s_soundtime += msec;
		clc.aviSoundFrameRemainder = frameDuration - msec;

		// use same offset as in game
		const int cursorOffset = ( s_mixOffset != nullptr && s_mixOffset->value > 0.0f )
			? static_cast<int>( s_mixOffset->value * static_cast<float>( dma.speed ) )
			: MixPreStepSamples();
		s_paintedtime = s_soundtime + cursorOffset;

		// render exactly one frame of audio data
		clc.aviFrameEndTime = s_paintedtime + static_cast<int>( duration + clc.aviSoundFrameRemainder );
		return;
	}

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();
	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (s_paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = dma.fullsamples;
			S_Base_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	s_soundtime = buffers * dma.fullsamples + samplepos/dma.channels;

	if ( dma.submission_chunk < 256 ) {
		const int cursorOffset = ( s_mixOffset != nullptr && s_mixOffset->value > 0.0f )
			? static_cast<int>( s_mixOffset->value * static_cast<float>( dma.speed ) )
			: MixPreStepSamples();
		s_paintedtime = s_soundtime + cursorOffset;
	} else {
		s_paintedtime = s_soundtime + dma.submission_chunk;
	}
}


static void S_Update_( int msec ) {
	unsigned		endtime;
	std::array<int, 2> mixAhead;
	int				thisTime, sane;
	static int		ot = -1;
	static int		lastTime = 0;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	thisTime = Sys_Milliseconds();

	const int previousPaintedEnd = s_paintedtime;
	// Updates s_soundtime
	S_GetSoundtime();

	if ( s_soundtime == ot ) {
		return;
	}

	ot = s_soundtime;

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

	sane = thisTime - lastTime;
	if ( sane < msec ) {
		sane = msec;
	}

	mixAhead[0] = s_mixahead->value * static_cast<float>( dma.speed );
	mixAhead[1] = sane * 0.0015f * static_cast<float>( dma.speed );

	if ( mixAhead[0] < mixAhead[1] ) {
		mixAhead[0] = mixAhead[1];
	}

	// mix ahead of current position
	endtime = s_paintedtime + mixAhead[0];

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);

	// never mix more than the complete buffer
	if ( endtime - s_paintedtime > static_cast<unsigned>( dma.fullsamples ) ) {
		endtime = s_paintedtime + dma.fullsamples;
	}

	// add raw data from streamed samples
	S_Base_UpdateBackgroundTrack();
	S_Base_UpdateRTCWEntityStreams();

	const bool traceTiming = s_debugTiming != nullptr && s_debugTiming->integer;
	const int paintStarted = traceTiming ? Sys_Milliseconds() : 0;
	const int paintStartFrame = s_paintedtime;
	SNDDMA_BeginPainting();

	S_PaintChannels( endtime );

	SNDDMA_Submit();

	if ( traceTiming ) {
		Com_Printf( "snd_timing ms=%d elapsed=%d work_ms=%d paint_ms=%d cursor=%d previous_end=%d start=%d end=%u raw_end=%d rate=%d\n",
			thisTime, sane, paintStarted - thisTime, Sys_Milliseconds() - paintStarted,
			s_soundtime, previousPaintedEnd, paintStartFrame, endtime, s_rawend, dma.speed );
	}
	lastTime = thisTime;
}


/*
===============================================================================

background music functions

===============================================================================
*/

/*
======================
S_StopBackgroundTrack
======================
*/
static void S_Base_StopBackgroundTrack( void ) {
	if ( Com_GameIsRTCWSP() ) {
		S_RTCWMusic().Stop();
		return;
	}
	if(!s_backgroundStream)
		return;
	S_CodecCloseStream(s_backgroundStream);
	s_backgroundStream = nullptr;
	s_rawend = 0;
}


/*
======================
S_OpenBackgroundStream
======================
*/
static void S_OpenBackgroundStream( const char *filename ) {
	// close the background track, but DON'T reset s_rawend
	// if restarting the same background track
	if( s_backgroundStream )
	{
		S_CodecCloseStream( s_backgroundStream );
		s_backgroundStream = nullptr;
	}

	// Open stream
	s_backgroundStream = S_CodecOpenStream( filename );
	if( !s_backgroundStream ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", filename );
		return;
	}

	if( s_backgroundStream->info.channels != 2 || s_backgroundStream->info.rate != 22050 ) {
		Com_Printf(S_COLOR_YELLOW "WARNING: music file %s is not 22k stereo\n", filename );
	}
}


/*
======================
S_StartBackgroundTrack
======================
*/
static void S_Base_StartRTCWBackgroundTrack( const char *intro, const char *loop, int fadeTime ) {
	RTCWMusicOutput output;
	S_RTCWMusic().Start( output, intro, loop, fadeTime );
}

static void S_Base_FadeRTCWBackgroundTrack( float volume, int time ) {
	S_RTCWMusic().Fade( volume, time, s_soundtime );
}

static void S_Base_FadeRTCWAllSounds( float volume, int time ) {
	S_RTCWMasterFade().Set( volume, time, s_soundtime, dma.speed );
}

// ET's fade can be told to stop what it faded once the fade completes. A second
// stopping fade begun before the first finished stops everything at once, which
// is what ET's own client does rather than letting the two overlap.
static void S_Base_FadeWolfETAllSounds( float volume, int time, qboolean stopSounds ) {
	auto &fade = S_WolfETMasterFade();
	if ( s_wolfetStopAfterFade && s_soundtime < fade.end ) S_Base_StopAllSounds();
	fade.Set( volume, time, s_soundtime, dma.speed );
	s_wolfetStopAfterFade = stopSounds ? true : false;
}

static void S_UpdateWolfETMasterFade( void ) {
	auto &fade = S_WolfETMasterFade();
	if ( !s_wolfetStopAfterFade || s_soundtime < fade.end ) return;
	fade.Value( s_soundtime );
	s_wolfetStopAfterFade = false;
	S_Base_StopAllSounds();
}

extern "C" float S_Base_RTCWMasterGain( void ) {
	if ( Com_GameIsWolfET() )
		return std::clamp( S_WolfETMasterFade().Value( s_soundtime ), 0.0f, 16.0f );
	return Com_GameIsRTCWSP() ?
		std::clamp( S_RTCWMasterFade().Value( s_soundtime ), 0.0f, 16.0f ) : 1.0f;
}

static int S_Base_WolfETSoundLength( sfxHandle_t handle ) {
	if ( handle < 0 || handle >= s_numSfx ) return -1;
	const sfx_t *sample = S_KnownSfxAt( handle );
	if ( !sample || dma.speed <= 0 ) return -1;
	return static_cast<int>( static_cast<std::int64_t>( sample->soundLength ) * 1000 / dma.speed );
}

static int S_Base_WolfETCurrentSoundTime( void ) {
	// ET reports the mixer's own sample clock one second ahead, which is what
	// its modules compare a submitted loop start time against.
	return s_soundtime + dma.speed;
}

static void S_Base_StartBackgroundTrack( const char *intro, const char *loop ){
	if ( Com_GameIsRTCWSP() ) {
		S_Base_StartRTCWBackgroundTrack( intro, loop, 0 );
		return;
	}
	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}
	Com_DPrintf( "S_StartBackgroundTrack( %s, %s )\n", intro, loop );

	if(!*intro)
	{
		S_Base_StopBackgroundTrack();
		return;
	}

	Q_strncpyz( s_backgroundLoop.data(), loop, s_backgroundLoop.size() );

	S_OpenBackgroundStream( intro );
}


/*
======================
S_Base_UpdateBackgroundTrack
======================
*/
static void S_Base_UpdateBackgroundTrack( void ) {
	if (!s_soundStarted || s_soundMuted) return;
	int		bufferSamples;
	int		fileSamples;
	std::array<byte, 30000> raw;		// just enough to fit in a mac stack frame
	int		fileBytes;
	int		r;
	RTCWMusicOutput rtcwOutput;
	int &rawEnd = Com_GameIsRTCWSP() ? s_rtcwMusicRawEnd : s_rawend;
	if ( Com_GameIsRTCWSP() && !S_RTCWMusic().BeginService( rtcwOutput, s_musicVolume->value ) ) return;

	if ( !s_backgroundStream ) {
		return;
	}

	// don't bother playing anything if musicvolume is 0
	if ( s_musicVolume->value == 0.0f ) {
		return;
	}

	// see how many samples should be copied into the raw buffer
	if ( static_cast<int32_t>(static_cast<uint32_t>(rawEnd) - static_cast<uint32_t>(s_soundtime)) < 0 ) {
		rawEnd = s_soundtime;
	}

	while ( static_cast<int32_t>(static_cast<uint32_t>(rawEnd) - static_cast<uint32_t>(s_soundtime)) < MAX_RAW_SAMPLES ) {
		bufferSamples = MAX_RAW_SAMPLES - (static_cast<int32_t>(static_cast<uint32_t>(rawEnd) - static_cast<uint32_t>(s_soundtime)));

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * s_backgroundStream->info.rate / dma.speed;

		if ( fileSamples == 0 ) {
			return;
		}

		// our max buffer size
		fileBytes = fileSamples * (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		if ( fileBytes > static_cast<int>( raw.size() ) ) {
			fileBytes = static_cast<int>( raw.size() );
			fileSamples = fileBytes / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		}

		fileBytes = fntech3::audio::WholeFrameReadBytes( fileBytes,
			s_backgroundStream->info.width, s_backgroundStream->info.channels );
		if ( !fileBytes ) return;
		// Read
		r = S_CodecReadStream( s_backgroundStream, fileBytes, raw.data() );
		if( r < fileBytes )
		{
			fileSamples = r / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		}

		if ( r > 0 )
		{
			// add to raw buffer
			const auto &info = s_backgroundStream->info;
			const bool route = fntech3::audio::NeedsSpeakerDownmix( info.speakerMask, info.channels, false );
			const auto routed = route ? fntech3::audio::DownmixSpeakers( raw.data(), fileSamples,
				info.width, info.channels, info.speakerMask ) : std::vector<short>{};
			if ( route && routed.empty() ) { S_Base_StopBackgroundTrack(); return; }
			S_Base_RawSamplesToBuffer( rawEnd, Com_GameIsRTCWSP() ? s_rtcwMusicRawSamples.data() : s_rawsamples, fileSamples, info.rate, route ? 2 : info.width, route ? 2 : info.channels,
				route ? reinterpret_cast<const byte *>( routed.data() ) : raw.data(), s_musicVolume->value *
				( Com_GameIsRTCWSP() ? std::clamp( S_RTCWMusic().Gain( s_soundtime ), 0.0f, 16.0f ) : 1.0f ) );
		}
		else
		{
			// loop
			if ( Com_GameIsRTCWSP() ) {
				S_RTCWMusic().EndOfTrack( rtcwOutput );
				return;
			}
			if ( s_backgroundLoop[0] != '\0' )
			{
				S_OpenBackgroundStream( s_backgroundLoop.data() );
				if ( !s_backgroundStream )
					return;
			}
			else
			{
				S_Base_StopBackgroundTrack();
				return;
			}
		}

	}
}


/*
======================
S_FreeOldestSound
======================
*/
extern "C" void S_FreeOldestSound( void ) {
	int	i, oldest, used;
	sfx_t	*sfx;
	sndBuffer	*buffer, *nbuffer;

	// all sounds may be loaded with (s_soundtime + 1) at this moment
	// so we need to trigger match condition at least once
	oldest = s_soundtime + 2; // Com_Milliseconds();

	used = 0;

	for ( i = 1 ; i < s_numSfx ; i++ ) {
		sfx = S_KnownSfxAt( i );
		if ( sfx->inMemory && sfx->lastTimeUsed - oldest < 0 ) {
			used = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	sfx = S_KnownSfxAt( used );

	Com_DPrintf("S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

	buffer = sfx->soundData;
	while(buffer != nullptr) {
		nbuffer = buffer->next;
		SND_free(buffer);
		buffer = nbuffer;
	}
	sfx->inMemory = qfalse;
	sfx->soundData = nullptr;
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

static void S_Base_Shutdown( void ) {

	if ( !s_soundStarted ) {
		return;
	}

	S_Base_ClearRTCWEntityStreams();
	SNDDMA_Shutdown();

	// release sound buffers only when switching to dedicated 
	// to avoid redundant reallocation at client restart
	if ( com_dedicated->integer )
		SND_shutdown();

	s_soundStarted = false;

	s_numSfx = 0; // clean up sound cache -EC-

	mutedPaintHeapBuffer.reset();
	dma_buffer2 = nullptr;

	Cmd_RemoveCommand( "s_info" );

	cls.soundRegistered = qfalse;
}


/*
================
S_Init
================
*/
extern "C" qboolean S_Base_Init( soundInterface_t *si ) {
	bool r;
	std::size_t mutedBytes = 0;

	if ( !si ) {
		return qfalse;
	}
	*si = {};

	s_khz = Cvar_Get( "s_khz", "22", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_khz, "0", "48", CV_INTEGER );
	Cvar_SetDescription( s_khz, "Specifies the sound sampling rate, (8, 11, 22, 44, 48) in kHz. Default value is 22." );

	switch( s_khz->integer ) {
		case 48:
		case 44:
		case 22:
		case 11:
		case 8:
			// these are legal values
			break;
		default:
			// anything else is illegal
			Com_Printf( "WARNING: cvar 's_khz' must be one of (8, 11, 22, 44, 48), setting to '%s'\n", s_khz->resetString );
			Cvar_ForceReset( "s_khz" );
			break;
	}

	s_debugTiming = Cvar_Get( "s_debugTiming", "0", CVAR_TEMP | CVAR_DEVELOPER );
	Cvar_SetDescription( s_debugTiming, "Log legacy playback cursor, painted ranges and queued raw audio for timing diagnostics." );
	s_mixahead = Cvar_Get( "s_mixAhead", "0.2", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixahead, "0.001", "0.5", CV_FLOAT );
	Cvar_SetDescription( s_mixahead, "Amount of time to pre-mix sound data to avoid potential skips/stuttering in case of unstable framerate. Higher values add more CPU usage." );

	s_mixOffset = Cvar_Get( "s_mixOffset", "0", CVAR_ARCHIVE_ND | CVAR_DEVELOPER );
	Cvar_CheckRange( s_mixOffset, "0", "0.5", CV_FLOAT );
	s_mixPreStep = Cvar_Get( "s_mixPreStep", "0.05", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixPreStep, "0", "0.25", CV_FLOAT );
	Cvar_SetDescription( s_mixPreStep, "Retail QL-compatible amount of audio mixed ahead of the current DMA cursor, in seconds." );

	s_show = Cvar_Get( "s_show", "0", CVAR_CHEAT );
	Cvar_SetDescription( s_show, "Debugging output (used sound files)." );
	s_testsound = Cvar_Get( "s_testsound", "0", CVAR_CHEAT );
	Cvar_SetDescription( s_testsound, "Debugging tool that plays a simple sine wave tone to test the sound system." );
#if defined(__linux__) && !defined(USE_SDL)
	s_device = Cvar_Get( "s_device", "default", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_device, "Set ALSA output device\n"
		" Use \"default\", \"sysdefault\", \"front\", etc.\n"
		" Enter " S_COLOR_CYAN "aplay -L " S_COLOR_WHITE "in your shell to see all options.\n"
		S_COLOR_YELLOW " Please note that only mono/stereo devices are acceptable.\n" );
#endif

	r = SNDDMA_Init();

	if ( r ) {
		if ( !fnql_audio_init::DmaConfigurationIsUsable(
			dma.channels,
			dma.samplebits,
			dma.isfloat != qfalse,
			dma.speed,
			dma.samples,
			dma.fullsamples,
			mutedBytes ) ) {
			Com_Printf( S_COLOR_YELLOW
				"WARNING: legacy audio device returned an unsafe DMA format "
				"(channels %d, bits %d, rate %d, samples %d, frames %d); rejecting it\n",
				dma.channels, dma.samplebits, dma.speed, dma.samples, dma.fullsamples );
			SNDDMA_Shutdown();
			return qfalse;
		}

		s_soundStarted = true;
		s_soundMuted = true;
		for ( auto &origin : rtcwEntityPositions ) VectorClear(origin);
//		s_numSfx = 0;

		sfxHash.fill( nullptr );

		s_soundtime = 0;
		s_paintedtime = 0;

		S_Base_StopAllSounds();
		rtcwStreams.SetProfile( Com_GameIsRTCWSP() != 0 );

		// setup (likely) or allocate (unlikely) buffer for muted painting
		if ( mutedBytes <= mutedPaintBuffer.size() ) {
			mutedPaintHeapBuffer.reset();
			dma_buffer2 = mutedPaintBuffer.data();
		} else {
			mutedPaintHeapBuffer.reset( new ( std::nothrow ) byte[ mutedBytes ] );
			dma_buffer2 = mutedPaintHeapBuffer.get();
			if ( !dma_buffer2 ) {
				SNDDMA_Shutdown();
				s_soundStarted = false;
				return qfalse;
			}
			std::fill_n( dma_buffer2, mutedBytes, byte{} );
		}
	} else {
		return qfalse;
	}

	si->Shutdown = S_Base_Shutdown;
	si->StartSound = S_Base_StartSound;
	si->StartLocalSound = S_Base_StartLocalSound;
	si->StartRTCWSound = S_Base_StartRTCWSound;
	si->StartWolfETSound = S_Base_StartWolfETSound;
	si->StartWolfETLocalSound = S_Base_StartWolfETLocalSound;
	si->AddWolfETLoopingSound = S_Base_AddWolfETLoopingSound;
	si->FadeWolfETAllSounds = S_Base_FadeWolfETAllSounds;
	si->WolfETSoundLength = S_Base_WolfETSoundLength;
	si->WolfETCurrentSoundTime = S_Base_WolfETCurrentSoundTime;
	si->StartRTCWLocalSound = S_Base_StartRTCWLocalSound;
	si->StartRTCWEntityStream = S_Base_StartRTCWEntityStream;
	si->StopRTCWEntityStream = S_Base_StopRTCWEntityStream;
	si->FadeRTCWEntityStream = S_Base_FadeRTCWEntityStream;
	si->RTCWVoiceAmplitude = S_Base_RTCWVoiceAmplitude;
	si->StartBackgroundTrack = S_Base_StartBackgroundTrack;
	si->StartRTCWBackgroundTrack = S_Base_StartRTCWBackgroundTrack;
	si->FadeRTCWBackgroundTrack = S_Base_FadeRTCWBackgroundTrack;
	si->FadeRTCWAllSounds = S_Base_FadeRTCWAllSounds;
	si->StopBackgroundTrack = S_Base_StopBackgroundTrack;
	si->UpdateBackgroundTrack = S_Base_UpdateBackgroundTrack;
	si->RawSamples = S_Base_RawSamples;
	si->AddVoiceSamples = S_Base_AddVoiceSamples;
	si->StopAllSounds = S_Base_StopAllSounds;
	si->ClearRTCWSounds = S_Base_ClearRTCWSounds;
	si->ClearLoopingSoundsFrame = S_Base_ClearLoopingSoundsFrame;
	si->ClearLoopingSounds = S_Base_ClearLoopingSounds;
	si->AddLoopingSound = S_Base_AddLoopingSound;
	si->AddRTCWLoopingSound = S_Base_AddRTCWLoopingSound;
	si->AddRealLoopingSound = S_Base_AddRealLoopingSound;
	si->StopLoopingSound = S_Base_StopLoopingSound;
	si->Respatialize = S_Base_Respatialize;
	si->UpdateEntityPosition = S_Base_UpdateEntityPosition;
	si->Update = S_Base_Update;
	si->DisableSounds = S_Base_DisableSounds;
	si->BeginRegistration = S_Base_BeginRegistration;
	si->RegisterSound = S_Base_RegisterSound;
	si->ClearSoundBuffer = S_Base_ClearSoundBuffer;
	si->SoundInfo = S_Base_SoundInfo;
	si->SoundList = S_Base_SoundList;

	return qtrue;
}
