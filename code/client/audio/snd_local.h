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
// snd_local.h -- private sound definitions


#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "snd_public.h"
#ifdef __cplusplus
// Every file reaches snd_local.h from inside an extern "C" block, and this
// one is C++ of its own.
extern "C++" {
#include "shared/STVEFLipSync.h"
#include "shared/JKALipSync.h"
#include "../client_jka_sound.hpp"
// American McGee's Alice's voices: shared/AliceSound.h decides, a backend plays.
namespace fntech3::audio::alice::sfx { class Device; }
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define	PAINTBUFFER_SIZE		4096					// this is in samples

#define SND_CHUNK_SIZE			1024					// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		// floats

typedef struct portable_samplepair_t portable_samplepair_t;
typedef struct adpcm_state_t adpcm_state_t;
typedef struct sndBuffer sndBuffer;
typedef struct sfx_t sfx_t;
typedef struct dma_t dma_t;
typedef struct loopSound_t loopSound_t;
typedef struct channel_t channel_t;
typedef struct wavinfo_t wavinfo_t;
typedef struct soundInterface_t soundInterface_t;
struct snd_stream_t;

struct portable_samplepair_t {
	int			left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	int			right;
};

struct adpcm_state_t {
	short	sample;		/* Previous output value */
	char	index;		/* Index into stepsize table */
};

struct sndBuffer {
	short					sndChunk[SND_CHUNK_SIZE];
	sndBuffer				*next;
	int						size;
	adpcm_state_t			adpcm;
};

struct sfx_t {
	sndBuffer		*soundData;
	qboolean		defaultSound;			// couldn't be loaded, so use buzz
	qboolean		inMemory;				// not in Memory
	qboolean		soundCompressed;		// not in Memory
	int				soundCompressionMethod;
	int 			soundLength;
	int				soundChannels;
	float			peak;			// the loudest byte Raven's tag reports, or none
	char 			soundName[MAX_QPATH];
	int				lastTimeUsed;
	sfx_t			*next;
	// The file's own frames and the bytes each takes in it, before the
	// resampling above; American McGee's Alice's loop blocks count in them.
	int				sourceFrames;
	int				sourceFrameBytes;
	qboolean		jkaMpeg;
	float			jkaPeak;
};

struct dma_t {
	unsigned int channels;
	unsigned int samples;				// mono samples in buffer
	int			fullsamples;			// samples with all channels in buffer (samples divided by channels)
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			isfloat;
	int			speed;
	byte		*buffer;
	const char	*driver;
};

extern byte *dma_buffer2;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f //arbitrary

struct loopSound_t {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	int			mergeFrame;
	qboolean	active;
	qboolean	kill;
	qboolean	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
};

struct channel_t {
	int			allocTime;
	int			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		// to allow overriding a specific sound
	int			leftvol;		// 0-255 volume after spatialization
	int			rightvol;		// 0-255 volume after spatialization
	int			master_vol;		// 0-255 volume before spatialization
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			// only use if fixed_origin is set
	qboolean	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t		*thesfx;		// sfx structure
	qboolean	doppler;
	// American McGee's Alice's loop block, in the sample's frames: a loop
	// channel whose end is set plays to that end and then repeats
	// [loopBlockStart, loopBlockEnd). Only Alice's voices set it; an end of 0
	// loops the whole sample.
	int			loopBlockStart;
	int			loopBlockEnd;
};


#define WAV_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

struct wavinfo_t {
	int			format;
	int			rate;
	int			width;
	int			channels;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
};

// Interface between Q3 sound "api" and the sound backend
struct soundInterface_t {
	void (*Shutdown)(void);
	void (*StartSound)( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx, float volume );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum, float volume );
	// Whether the next local sound is one the game's menus started, which a
	// backend with tone classes plays as interface audio on any channel.
	void (*MarkInterfaceSound)( qboolean interfaceSound );
	void (*StartRTCWSound)( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags );
	void (*StartWolfETSound)( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags, int volume );
	void (*StartWolfETLocalSound)( sfxHandle_t handle, int channel, int volume );
	void (*StartRTCWLocalSound)( sfxHandle_t handle, int channel );
	void (*StartRTCWEntityStream)( const char *intro, const char *loop, int entity, int channel, int attenuation );
	void (*StopRTCWEntityStream)( int entity );
	void (*FadeRTCWEntityStream)( float volume, int time, int stream );
	int (*RTCWVoiceAmplitude)( int entity );
	int (*JKAVoiceVolume)( int entity );
	void (*StartBackgroundTrack)( const char *intro, const char *loop );
	void (*StartRTCWBackgroundTrack)( const char *intro, const char *loop, int fadeTime );
	void (*FadeRTCWBackgroundTrack)( float volume, int time );
	void (*FadeRTCWAllSounds)( float volume, int time );
	void (*FadeWolfETAllSounds)( float volume, int time, qboolean stopSounds );
	int (*WolfETSoundLength)( sfxHandle_t handle );
	int (*WolfETCurrentSoundTime)( void );
	void (*StopBackgroundTrack)( void );
	// A music source the caller built rather than a file to open: American
	// McGee's Alice mixes its own two song players into one stream. The
	// backend owns the stream from here and closes it as it closes a track.
	void (*StartStreamSource)( struct snd_stream_t *stream );
	void (*UpdateBackgroundTrack)( void );
	void (*RawSamples)(int samples, int rate, int width, int channels, const byte *data, float volume);
	void (*AddVoiceSamples)( int clientNum, int samples, int rate, const short *data );
	void (*StopAllSounds)( void );
	void (*ClearRTCWSounds)( qboolean clearMusic );
	void (*ClearLoopingSoundsFrame)( void );
	void (*ClearLoopingSounds)( qboolean killall );
	void (*AddLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*AddRTCWLoopingSound)( const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t sfx, int volume );
	void (*AddSTVEFAmbientLoopingSound)( const vec3_t origin, sfxHandle_t sfx, int volume );
	// Jedi Academy's ambient-set loops, each with the volume its set has
	// faded to out of 255 (shared/JKAAmbient.h).
	void (*AddJKAAmbientLoopingSound)( const vec3_t origin, sfxHandle_t sfx, int volume );
	// Medal of Honor's sounds and loops with their alias's volume, distances
	// (full volume inside minDist, silent past maxDist) and pitch.
	void (*StartMOHAASound)( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx,
		float volume, float minDist, float maxDist, float pitch, int mohaaChannel );
	void (*StopMOHAASound)( int entnum, int mohaaChannel );
	// Ends what one entity is playing on one channel (Academy's import 36).
	void (*StopEntityChannelSound)( int entnum, int entchannel );
	void (*AddMOHAALoopingSound)( int slot, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx,
		float volume, float minDist, float maxDist, float pitch );
	// How wide a mouth each entity holds, for Elite Force SP alone.
	const int *(*STVEFSPMouths)( void );
	void (*AddWolfETLoopingSound)( const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t sfx, int volume, int soundTime );
	void (*AddRealLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*StopLoopingSound)(int entityNum );
	void (*Respatialize)( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)( int msec );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const char *sample, qboolean compressed );
	void (*ClearSoundBuffer)( void );
	void (*SoundInfo)( void );
	void (*SoundList)( void );
	qboolean (*GetSpatialDebugInfo)( spatialAudioDebugInfo_t *info );
	void (*DumpSpatialDebug)( void );
	// American McGee's Alice's 32 3-D voices and 4 dialog voices, which the
	// engine in shared/AliceSound.h drives for every profile's backend alike;
	// only the Alice profile ever calls it.
#ifdef __cplusplus
	fntech3::audio::alice::sfx::Device *AliceDevice;
#else
	void *AliceDevice;
#endif
};


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	int		numLoopChannels;

extern	int		s_soundtime;
float S_Base_RTCWMasterGain( void );
// Medal of Honor's `fadesound` gain over the whole mix, 1 for every other
// profile and while no fade runs.
float S_MOHAAFadeGain( void );
void S_PaintRTCWMusic( int first, int end, portable_samplepair_t *paint );
void S_PaintRTCWEntityStreams( int first, int end, portable_samplepair_t *paint );
extern	int		s_paintedtime;
extern	int		s_rawend;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
qboolean S_StartStreamSource( struct snd_stream_t *stream );
extern cvar_t *s_voiceVolume;
extern cvar_t *s_voiceStep;
extern cvar_t *s_doppler;
extern cvar_t *s_pvs;
// Elite Force registers this one, and American McGee's Alice, which spreads
// only its two-dimensional dialog voices by it; the other games leave it null
// and are spread evenly, which is Elite Force's default for it.
extern cvar_t *s_separation;
static ID_INLINE float S_STVEFSeparation( void ) {
	if ( s_separation == NULL ) return 0.5f;
	if ( s_separation->value < 0.0f ) return 0.0f;
	if ( s_separation->value > 1.0f ) return 1.0f;
	return s_separation->value;
}
// Elite Force SP and Academy MP register these four in the order their
// retail engines compare them to decide how wide a mouth a voice opens.
extern cvar_t *s_threshold1;
extern cvar_t *s_threshold2;
extern cvar_t *s_threshold3;
extern cvar_t *s_threshold4;
#ifdef __cplusplus
extern "C++" inline bool S_JKAIsVoiceChannel( int channel ) {
	namespace sound=fntech3::jka::sound;
	return channel==sound::EngineChannel(sound::ChanVoice)||
		channel==sound::EngineChannel(sound::ChanVoiceAtten)||channel==sound::EngineChannel(sound::ChanVoiceGlobal);
}
extern "C++" inline fntech3::audio::jka::lipsync::Thresholds S_JKAThresholds( void ) {
	// Snapshot only: sample loading owns temporary PCM buffers, so validation
	// here must not Com_Error/longjmp across their destructors.
	fntech3::audio::jka::lipsync::Thresholds t;
	if (s_threshold1) t.closed=s_threshold1->value;
	if (s_threshold2) t.narrow=s_threshold2->value;
	if (s_threshold3) t.wide=s_threshold3->value;
	if (s_threshold4) t.widest=s_threshold4->value;
	return t;
}
extern "C++" inline fntech3::audio::stvef::lipsync::Thresholds S_STVEFThresholds( void ) {
	fntech3::audio::stvef::lipsync::Thresholds thresholds;
	if ( s_threshold1 != NULL ) thresholds.closed = s_threshold1->value;
	if ( s_threshold2 != NULL ) thresholds.narrow = s_threshold2->value;
	if ( s_threshold3 != NULL ) thresholds.wide = s_threshold3->value;
	if ( s_threshold4 != NULL ) thresholds.widest = s_threshold4->value;
	return thresholds;
}
#endif
extern cvar_t *s_muteWhenUnfocused;
extern cvar_t *s_muteWhenMinimized;
extern cvar_t *s_khz;
extern cvar_t *s_backend;
extern cvar_t *s_backendActive;
extern cvar_t *s_alDevice;
extern cvar_t *s_alHrtf;
extern cvar_t *s_alHrtfId;
extern cvar_t *s_alOutputMode;
extern cvar_t *s_alDistanceModel;
extern cvar_t *s_alFrequency;
extern cvar_t *s_alSourceResampler;
extern cvar_t *s_alRefresh;
extern cvar_t *s_alMonoSources;
extern cvar_t *s_alStereoSources;
extern cvar_t *s_alOutputLimiter;
extern cvar_t *s_alSpatializeStereo;

extern cvar_t *s_testsound;

qboolean S_LoadSound( sfx_t *sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown( void );

void S_PaintChannels(int endtime);
void S_PaintVoiceSamples( int starttime, int endtime, portable_samplepair_t *paintbuffer );
qboolean S_OriginInPVS( const vec3_t listener, const vec3_t origin );

// spatializes a channel
void S_Spatialize(channel_t *ch);

// adpcm functions
int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

void S_FreeOldestSound( void );

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int	   sfxScratchIndex;

qboolean S_Base_Init( soundInterface_t *si );
qboolean S_OpenAL_Init( soundInterface_t *si );
void S_OpenAL_ListDevices( void );
void S_OpenAL_ListHrtfs( void );
void S_OpenAL_ConfigHints( void );
qboolean S_OpenAL_RecoverDevice( qboolean force );

#ifdef __cplusplus
extern "C++" {
// The running backend's Alice voices, or null while no backend runs.
fntech3::audio::alice::sfx::Device *S_AliceDevice( void );
// Whether a backend runs: retail's "sound started" (0x00b84eac).
bool S_AliceBackendStarted( void );
// The entity the last Alice respatialize named the listener (0x0069ffb4).
int S_AliceListenerNumber( void );
// The room Alice's client game last named (import 40): true while it names
// one -- a known environment at a level above 0 -- and s_reverb lets it
// replace the engine's own room.
bool S_AliceNamedRoom( int *type, float *level );
// The same for Medal of Honor's client game (import 65), whose rooms are the
// same 26 EAX environments.
bool S_MOHAANamedRoom( int *type, float *level );
}
#endif

#ifdef __cplusplus
}
#endif
