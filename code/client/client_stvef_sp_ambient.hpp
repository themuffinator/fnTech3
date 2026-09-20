/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_AMBIENT_HPP
#define FNTECH3_CLIENT_STVEF_SP_AMBIENT_HPP
#include "../compat/stvef_sp_ambient.hpp"
#include "client_stvef_records.hpp"

#include <cstdlib>
#include <string>
#include <vector>

/*
 * Elite Force SP's ambient sound sets (stvoy.exe 0x41c960 to 0x41ddf0). The
 * cgame names the sets a level uses, has the engine read sound/sound.txt,
 * and then each frame hands the engine the set the player is in and the
 * local sets around them; the game asks the same sets for the sounds its
 * movers start, run and stop with.
 *
 * A set's looping wave plays wherever it is submitted, at the volume the set
 * has faded to, and its sub-waves play one at a time with seconds between
 * them. The set the player walks into fades up over a second while the one
 * they left fades down, and both play until the old one is silent.
 */

namespace {

namespace ambient = fntech3::stvef::ambient;

struct STVEFSPAmbientSet {
	ambient::Set file{};
	sfxHandle_t loopedWave = 0;
	sfxHandle_t subWaves[ambient::MaxSubWaves]{};
	int numSubWaves = 0;
	// The volume the set has faded to, and when that fade began. A set that
	// has never faded is at full volume, as retail creates it (0x41caac).
	int masterVolume = 255;
	int fadeTime = 0;
};

// Retail crosses one set into the next over a second (0x4916e0).
constexpr int kSTVEFSPAmbientFadeTime = 1000;

std::vector<STVEFSPAmbientSet> stvefSpAmbientSets;
std::vector<std::string> stvefSpAmbientPrecache;
// The set the player is in and the one they left, and when each last played
// a sub-wave (0x4916d8, 0x4916dc, 0x696108, 0x69610c).
int stvefSpAmbientCurrent = -1;
int stvefSpAmbientOld = -1;
int stvefSpAmbientCurrentWave = 0;
int stvefSpAmbientOldWave = 0;

// Retail's random choice: a number from 0 to count - 1.
int CL_STVEFSPAmbientRandom( int count ) {
	if ( count <= 1 ) return 0;
	return ( ( std::rand() & 0x7fff ) * count ) >> 15;
}

int CL_STVEFSPAmbientFind( const char *name ) {
	if ( !name || !name[0] ) return -1;
	for ( std::size_t i = 0; i < stvefSpAmbientSets.size(); ++i ) {
		// Retail keeps its sets in a map of exactly the names the file gives.
		if ( !std::strcmp( stvefSpAmbientSets[i].file.name, name ) ) return static_cast<int>( i );
	}
	return -1;
}

sfxHandle_t CL_STVEFSPAmbientRegister( const char *path ) {
	const sfxHandle_t handle = S_RegisterSound( path, qfalse );
	if ( handle <= 0 ) {
		Com_Printf( "^3WARNING: Unable to load ambient sound \"%s\"\n", path );
	}
	return handle;
}

/* The sets a level uses, named before the file is read (0x41d6e0). */
void CL_STVEFSPAmbientAddPrecacheEntry( const char *name ) {
	if ( !name || !name[0] ) return;
	for ( const std::string &kept : stvefSpAmbientPrecache ) {
		if ( kept == name ) return;
	}
	stvefSpAmbientPrecache.emplace_back( name );
}

/* Every set the file names is kept, but only the ones a level asked for have
 * their waves loaded, as retail loads them (0x41d3c0, 0x41d7e0). */
void CL_STVEFSPAmbientParseSets() {
	stvefSpAmbientSets.clear();
	stvefSpAmbientCurrent = -1;
	stvefSpAmbientOld = -1;
	char *text = nullptr;
	const int length = FS_ReadFile( "sound/sound.txt", reinterpret_cast<void **>( &text ) );
	if ( length <= 0 || !text ) {
		if ( text ) FS_FreeFile( text );
		stvefSpAmbientPrecache.clear();
		Com_Error( ERR_DROP, "^1ERROR: Couldn't load ambient sound sets from %s", "sound/sound.txt" );
		return;
	}
	const int count = ambient::Parse( text,
		[]( const ambient::Set &parsed ) {
			STVEFSPAmbientSet set;
			set.file = parsed;
			bool wanted = false;
			for ( const std::string &kept : stvefSpAmbientPrecache ) {
				if ( kept == parsed.name ) { wanted = true; break; }
			}
			if ( wanted ) {
				if ( parsed.loopedWave[0] ) {
					set.loopedWave = CL_STVEFSPAmbientRegister( parsed.loopedWave );
				}
				for ( int i = 0; i < parsed.numSubWaves; ++i ) {
					set.subWaves[set.numSubWaves++] = CL_STVEFSPAmbientRegister( parsed.subWaves[i] );
				}
			}
			stvefSpAmbientSets.push_back( set );
		},
		[]( ambient::Warning warning, const char *name ) {
			char text[MAXPRINTMSG];
			ambient::WarningText( warning, name, text, sizeof( text ) );
			Com_Printf( "%s", text );
		} );
	FS_FreeFile( text );
	Com_Printf( "AS_ParseFile: Parsed %d ambient set(s)\n", count );
	stvefSpAmbientPrecache.clear();
}

/* A mover's sound for one of its stages, or none (0x41ddf0). */
sfxHandle_t CL_STVEFSPAmbientBModelSound( const char *name, int stage ) {
	const int index = CL_STVEFSPAmbientFind( name );
	if ( index >= 0 && stage >= 0 && stage < stvefSpAmbientSets[index].numSubWaves ) {
		return stvefSpAmbientSets[index].subWaves[stage];
	}
	return -1;
}

/* A set's looping wave every frame, and one of its sub-waves once the time
 * between waves has passed (0x41dc00, 0x41da60). */
void CL_STVEFSPAmbientPlay( const float *origin, STVEFSPAmbientSet &set, int &lastWave, int entity,
	int volume ) {
	if ( set.loopedWave > 0 ) {
		S_AddSTVEFAmbientLoopingSound( origin, set.loopedWave, volume );
	}
	const int span = set.file.timeEnd + 1 - set.file.timeStart;
	const int delay = ( CL_STVEFSPAmbientRandom( span ) + set.file.timeStart ) * 1000;
	if ( cls.realtime - lastWave < delay ) {
		return;
	}
	lastWave = cls.realtime;
	if ( set.numSubWaves <= 0 ) {
		return;
	}
	const float scale = static_cast<float>( volume ) * ( 1.0f / 255.0f );
	const int low = static_cast<int>( set.file.volumeMin * scale );
	const int high = static_cast<int>( set.file.volumeMax * scale ) + 1;
	int wave = CL_STVEFSPAmbientRandom( high - low ) + low;
	if ( wave > volume ) wave = volume;
	if ( wave <= 0 ) {
		return;
	}
	const sfxHandle_t sfx = set.subWaves[CL_STVEFSPAmbientRandom( set.numSubWaves )];
	if ( sfx <= 0 ) {
		return;
	}
	S_StartSoundVolume( const_cast<float *>( origin ), entity,
		CL_STVEFSPSoundChannel( FNTECH3_STVEF_SP_CHAN_AMBIENT ), sfx,
		static_cast<float>( wave ) / 255.0f );
}

/* The set the player walks into fades up while the one they left fades down
 * (0x41d9f0, 0x41d910). */
void CL_STVEFSPAmbientSetCurrent( int index ) {
	if ( index != stvefSpAmbientCurrent ) {
		stvefSpAmbientOld = stvefSpAmbientCurrent;
		stvefSpAmbientCurrent = index;
		if ( stvefSpAmbientOld >= 0 ) {
			stvefSpAmbientSets[stvefSpAmbientOld].masterVolume = 255;
			stvefSpAmbientSets[stvefSpAmbientOld].fadeTime = cls.realtime;
		}
		if ( stvefSpAmbientCurrent >= 0 ) {
			stvefSpAmbientSets[stvefSpAmbientCurrent].masterVolume = 0;
			stvefSpAmbientSets[stvefSpAmbientCurrent].fadeTime = cls.realtime;
		}
	}
	if ( stvefSpAmbientCurrent < 0 ) {
		return;
	}
	const auto faded = []( int began ) {
		return static_cast<int>( static_cast<float>( cls.realtime - began )
			/ static_cast<float>( kSTVEFSPAmbientFadeTime ) * 255.0f );
	};
	STVEFSPAmbientSet &now = stvefSpAmbientSets[stvefSpAmbientCurrent];
	if ( now.masterVolume < 255 ) {
		now.masterVolume = faded( now.fadeTime );
	}
	if ( now.masterVolume > 255 ) {
		now.masterVolume = 255;
	}
	if ( stvefSpAmbientOld < 0 ) {
		return;
	}
	STVEFSPAmbientSet &before = stvefSpAmbientSets[stvefSpAmbientOld];
	before.masterVolume = before.masterVolume <= 0 ? 0 : 255 - faded( before.fadeTime );
	if ( before.masterVolume <= 0 ) {
		before.masterVolume = 0;
		stvefSpAmbientOld = -1;
	}
}

/* The set the player is in, played where they are (0x41dd20). */
void CL_STVEFSPAmbientUpdateSet( const char *name, const float *origin ) {
	const int index = CL_STVEFSPAmbientFind( name );
	if ( index < 0 || !origin ) {
		return;
	}
	CL_STVEFSPAmbientSetCurrent( index );
	if ( stvefSpAmbientCurrent >= 0 ) {
		STVEFSPAmbientSet &now = stvefSpAmbientSets[stvefSpAmbientCurrent];
		CL_STVEFSPAmbientPlay( origin, now, stvefSpAmbientCurrentWave, 0, now.masterVolume );
	}
	if ( stvefSpAmbientOld >= 0 ) {
		STVEFSPAmbientSet &before = stvefSpAmbientSets[stvefSpAmbientOld];
		CL_STVEFSPAmbientPlay( origin, before, stvefSpAmbientOldWave, 0, before.masterVolume );
	}
}

/* A set around a point, at full volume within half its radius and fading to
 * silence at the radius itself; the answer is when it last played, which the
 * game keeps for each of its sounding entities (0x41dda0, 0x41da60). */
int CL_STVEFSPAmbientAddLocalSet( const char *name, const float *listener, const float *origin,
	int entity, int lastWave ) {
	const int index = CL_STVEFSPAmbientFind( name );
	if ( index < 0 || !listener || !origin ) {
		return cls.realtime;
	}
	STVEFSPAmbientSet &set = stvefSpAmbientSets[index];
	const float radius = static_cast<float>( set.file.radius );
	const float half = radius * 0.5f;
	vec3_t source;
	VectorSubtract( origin, listener, source );
	const float distance = VectorLength( source );
	float scale = 0.0f;
	if ( distance < half ) {
		scale = 1.0f;
	} else if ( half > 0 ) {
		scale = ( radius - distance ) / half;
		if ( scale > 1.0f || scale < 0.0f ) {
			scale = 0.0f;
		}
	}
	const int volume = static_cast<int>( static_cast<float>( set.masterVolume ) * scale );
	int wave = lastWave;
	CL_STVEFSPAmbientPlay( origin, set, wave, entity, volume );
	return wave;
}

} // namespace

/* Retail forgets its sets when the sound system starts, so the sounds they
 * hold are never the ones of a system that has gone (0x41fee1). */
void CL_STVEFSPClearAmbientSets( void ) {
	stvefSpAmbientSets.clear();
	stvefSpAmbientPrecache.clear();
	stvefSpAmbientCurrent = -1;
	stvefSpAmbientOld = -1;
	stvefSpAmbientCurrentWave = 0;
	stvefSpAmbientOldWave = 0;
}

#endif
