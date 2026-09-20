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


#ifdef __cplusplus
extern "C" {
#endif

void S_Init( void );
void S_Shutdown( void );

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
void S_StartSoundVolume( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx, float volume );
void S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void S_StartLocalSoundVolume( sfxHandle_t sfx, int channelNum, float volume );
void S_StartRTCWSound( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags );
// Enemy Territory carries a per-sound volume its own modules set, a start
// time on looping sounds, and a flag deciding whether a fade also stops the
// sounds it faded. None of those exist in the other Wolfenstein contract.
void S_StartWolfETSound( const vec3_t origin, int entity, int channel, sfxHandle_t handle,
	int flags, int volume );
void S_StartWolfETLocalSound( sfxHandle_t handle, int channel, int volume );
void S_AddWolfETLoopingSound( const vec3_t origin, const vec3_t velocity, int range,
	sfxHandle_t handle, int volume, int soundTime );
void S_ClearWolfETLoopingSounds( void );
void S_ClearWolfETSounds( qboolean killMusic );
void S_FadeWolfETAllSounds( float volume, int time, qboolean stopSounds );
void S_FadeWolfETStreamingSound( float volume, int time, int stream );
void S_StartWolfETBackgroundTrack( const char *intro, const char *loop, int fadeTime );
int S_StartWolfETEntityStream( const char *intro, const char *loop, int entity,
	int channel, int attenuation );
void S_StopWolfETEntityStream( int entity );
int S_WolfETVoiceAmplitude( int entity );
int S_WolfETSoundLength( sfxHandle_t handle );
int S_STVEFSoundLength( sfxHandle_t handle );
int S_WolfETCurrentSoundTime( void );
void S_StartRTCWLocalSound( sfxHandle_t handle, int channel );
void S_StartRTCWEntityStream( const char *intro, const char *loop, int entity, int channel, int attenuation );
void S_StopRTCWEntityStream( int entity );
int S_RTCWVoiceAmplitude( intptr_t entity );

void S_StartBackgroundTrack( const char *intro, const char *loop );
void S_StartRTCWBackgroundTrack( const char *intro, const char *loop, int fadeTime );
void S_FadeRTCWStreamingSound( float volume, int time, int stream );
void S_FadeRTCWAllSounds( float volume, int time );
void S_StopBackgroundTrack( void );
void S_UpdateBackgroundTrack( void );

// cinematics and other generic stream producers will send raw samples
// 1.0 volume will be direct output of source samples
void S_RawSamples (int samples, int rate, int width, int channels,
					const byte *data, float volume);

// Queues one decoded remote-speaker packet without sharing the cinematic/raw
// stream. The explicit rate keeps the fallback mixer correct when s_khz does
// not match Steam's requested decode rate.
void S_AddVoiceSamples( int clientNum, int samples, int rate, const short *data );

// stop all sounds and the background track
void S_StopAllSounds( void );

// all continuous looping sounds must be added before calling S_Update
void S_ClearLoopingSoundsFrame( void );
void S_ClearRTCWLoopingSounds( int mode );
void S_ClearLoopingSounds( qboolean killall );
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_AddRTCWLoopingSound( const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t sfx, int volume );
// Elite Force single player's ambient loops carry the volume their set has
// faded to, and no entity: at most 32 a frame, merged by sound.
void S_AddSTVEFAmbientLoopingSound( const vec3_t origin, sfxHandle_t sfx, int volume );
void S_StopLoopingSound(int entityNum );

// recompute the relative volumes for all running sounds
// relative to the given entityNum / orientation
void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );

// let the sound system know where an entity currently is
void S_UpdateEntityPosition( int entityNum, const vec3_t origin );

void S_Update( int msec );

void S_DisableSounds( void );

void S_BeginRegistration( void );

// RegisterSound will always return a valid sample, even if it
// has to create a placeholder.  This prevents continuous filesystem
// checks for missing files
sfxHandle_t	S_RegisterSound( const char *sample, qboolean compressed );

void S_DisplayFreeMemory(void);

void S_ClearSoundBuffer( void );

void SNDDMA_Activate( void );

#ifndef S_SPATIAL_AUDIO_DEBUG_INFO_DEFINED
#define S_SPATIAL_AUDIO_DEBUG_INFO_DEFINED

#define S_SPATIAL_DEBUG_MAX_LINES 10
#define S_SPATIAL_DEBUG_LINE_CHARS 128

struct spatialAudioDebugInfo_t {
	qboolean	active;
	qboolean	hasSelectedVoice;
	int			lineCount;
	char		lines[S_SPATIAL_DEBUG_MAX_LINES][S_SPATIAL_DEBUG_LINE_CHARS];
	float		dryGain;
	float		wetGain;
	float		occlusion;
	float		pan;
	float		pitch;
};
#ifndef __cplusplus
#define spatialAudioDebugInfo_t struct spatialAudioDebugInfo_t
#endif

#endif

qboolean S_GetSpatialAudioDebugInfo( spatialAudioDebugInfo_t *info );

#ifdef __cplusplus
}
#endif
