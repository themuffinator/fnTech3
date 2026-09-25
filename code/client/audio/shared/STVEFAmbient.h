/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_STVEF_AMBIENT_H
#define FNTECH3_AUDIO_STVEF_AMBIENT_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

/*
 * How Elite Force single player's sounds reach the listener, and the ambient
 * loops its sets submit.
 *
 * Retail measures every sound the same way (S_SpatializeOrigin stvoy.exe
 * 0x420510): the distance from the listener less the distance the sound is
 * heard at full volume, times a falloff, taken from one; the pan follows the
 * listener's right axis blended with the stereo separation, and a mono
 * device hears both channels at three quarters. A sound carries full volume
 * to 256 units and falls silent by 1,506, except on the two voice channels:
 * a voice holds to 1,024 units, and an attenuated voice holds to 38.4 and
 * falls five times as fast, silent by about 290. The legacy mixer plays this
 * law and pan as they are; the OpenAL backend takes the law as each channel's
 * span and the separation as a spread, and renders them with its own distance
 * model and panning.
 *
 * An ambient set's looping wave comes with the volume the set has faded to;
 * retail keeps at most 32 loops a frame and merges those playing the same
 * sound (S_AddAmbientLoopingSound 0x420bd0, S_AddLoopSounds 0x420ca0).
 */

namespace fntech3::audio::stvef {

inline constexpr float FullVolumeDistance = 256.0f;
inline constexpr float Attenuation = 0.0008f;
inline constexpr float MonoScale = 0.75f;
// Retail's own stereo separation cvar, which it leaves at a half.
inline constexpr float Separation = 0.5f;
// A voice is heard at full volume much further away, and an attenuated
// voice hardly at all (0x420559).
inline constexpr float VoiceFullVolumeDistance = 1024.0f;
inline constexpr float AttenuatedVoiceFullVolumeDistance = 38.4f;
inline constexpr float AttenuatedVoiceAttenuation = 0.004f;

/* The engine's numbers for those two channels. Single player's voice keeps
 * Quake III's own number, and its attenuated voice, which the engine has no
 * channel for, takes a number of its own above them (the client's
 * CL_STVEFSPSoundChannel; stvef_sp_sound_distance_tests pins the pair). */
inline constexpr int VoiceChannel = 3;
inline constexpr int AttenuatedVoiceChannel = 12;
/* Single player's two menu channels take numbers above the engine's the same
 * way. */
inline constexpr int MenuChannel = 18;
inline constexpr int SecondMenuChannel = 19;

/* Single player holds back the sounds the listener's own entity makes,
 * on every channel the game numbers below its ambient one
 * (S_StartSound stvoy.exe 0x4208cc); its ambient, local, announcer and
 * menu sounds keep the whole of their volume. Those are the engine's own
 * channels up to its body channel, and single player's attenuated voice,
 * which sits above them. */
inline constexpr float ListenerScale = 0.75f;

inline bool QuieterAtListener( int channel ) {
	return ( channel >= 0 && channel <= 5 ) || channel == AttenuatedVoiceChannel;
}

/* How far a sound on a channel carries at full volume, and how quickly it
 * falls away after that. */
inline void ChannelFalloff( int channel, float &fullVolume, float &attenuation ) {
	if ( channel == VoiceChannel ) {
		fullVolume = VoiceFullVolumeDistance;
		attenuation = Attenuation;
	} else if ( channel == AttenuatedVoiceChannel ) {
		fullVolume = AttenuatedVoiceFullVolumeDistance;
		attenuation = AttenuatedVoiceAttenuation;
	} else {
		fullVolume = FullVolumeDistance;
		attenuation = Attenuation;
	}
}

/*
 * The same law for a backend that measures distance itself: a channel's
 * distances, full volume out to `full` and silent from `silent` on, falling in
 * a straight line between. OpenAL's clamped linear model with these as its
 * reference and max distances is the law exactly.
 */
struct Span {
	float full = FullVolumeDistance;
	float silent = FullVolumeDistance + 1.0f / Attenuation;
};

inline Span ChannelSpan( int channel ) {
	Span span;
	float attenuation = Attenuation;
	ChannelFalloff( channel, span.full, attenuation );
	span.silent = span.full + 1.0f / attenuation;
	return span;
}

/* The law's gain at a distance, without the pan. */
inline float SpanGain( const Span &span, float distance ) {
	if ( !std::isfinite( distance ) || !( distance < span.silent ) ) {
		return 0.0f;
	}
	if ( distance <= span.full ) {
		return 1.0f;
	}
	return 1.0f - ( distance - span.full ) / ( span.silent - span.full );
}

/* The channel whose tone class the engine gives a sound: the attenuated
 * voice is a voice, and the rest are the engine's own channels or have no
 * class. */
inline int ToneChannel( int channel ) {
	return channel == AttenuatedVoiceChannel ? VoiceChannel : channel;
}

inline bool IsMenuChannel( int channel ) {
	return channel == MenuChannel || channel == SecondMenuChannel;
}

/* The room tone of the set the player is in is played at the view origin the
 * cgame hands over (S_UpdateAmbientSet 0x41dd20), which is the listener's own:
 * a loop this near is heard at the listener, not from any direction. */
inline constexpr float AtListenerDistance = 1.0f;

/*
 * The separation as the angle a placed source is spread across. Retail keeps
 * (1 - s) / s of a side sound's difference between its near and far channel
 * (0x4205dd): all of it at the default half, none at 1. A first-order
 * panner keeps (1 + cos(spread / 2)) / 2 of a source's direction (OpenAL
 * Soft's spherical source, core/mixer.cpp), so the two are matched here. At or
 * below the half a point source is left as it is: nothing is narrower.
 */
inline float SeparationSpread( float separation ) {
	constexpr float pi = 3.14159265358979f;
	separation = std::clamp( separation, 0.0f, 1.0f );
	if ( !( separation > Separation ) ) {
		return 0.0f;
	}
	const float kept = ( 1.0f - separation ) / separation;
	return std::clamp( 2.0f * std::acos( std::clamp( 2.0f * kept - 1.0f, -1.0f, 1.0f ) ), 0.0f, 2.0f * pi );
}

/* The radius that spreads a source across `spread` radians when heard from
 * `distance`: OpenAL Soft's AL_EXT_SOURCE_RADIUS subtends 2 asin(r / d), and
 * from inside the radius 2 pi - pi d / r (alc/alu.cpp). */
inline float SpreadRadius( float spread, float distance ) {
	constexpr float pi = 3.14159265358979f;
	if ( !( spread > 0.0f ) || !( distance > 0.0f ) || !std::isfinite( distance ) ) {
		return 0.0f;
	}
	if ( spread <= pi ) {
		return distance * std::sin( spread * 0.5f );
	}
	// A whole sphere is reached only at the source itself; this is within a
	// ten-thousandth of it.
	const float remaining = ( std::max )( 2.0f * pi - spread, pi * 1.0e-4f );
	return distance * pi / remaining;
}

struct AmbientLoop {
	std::array<float, 3> origin{};
	int handle = 0;
	int volume = 0;
};

class AmbientLoopFrame {
public:
	// Retail keeps no more than this many a frame and drops the rest.
	static constexpr std::size_t Capacity = 32;
	void Clear() { count_ = 0; }
	std::size_t Size() const { return count_; }
	bool Accepts() const { return count_ < Capacity; }
	const AmbientLoop &operator[]( std::size_t index ) const { return loops_[index]; }

	int Append( const float *origin, int handle, int volume ) {
		if ( !Accepts() || !origin ) return -1;
		AmbientLoop next;
		std::memcpy( next.origin.data(), origin, sizeof( next.origin ) );
		next.handle = handle;
		next.volume = std::clamp( volume, 0, 255 );
		loops_[count_] = next;
		return static_cast<int>( count_++ );
	}

private:
	std::array<AmbientLoop, Capacity> loops_{};
	std::size_t count_ = 0;
};

struct Gains { int left = 0, right = 0; };

/* One sound's gains at the listener. The distance is measured from the
 * listener and the pan from its right axis; the channel decides how far the
 * sound carries, and separation is the stereo separation retail leaves at a
 * half. */
inline Gains Spatialize( const float *origin, float volume, const float *listener,
	const float axis[3][3], bool mono = false, float separation = Separation,
	int channel = -1 ) {
	Gains gains;
	if ( !origin || !listener || !( volume > 0 ) ) {
		return gains;
	}
	float fullVolume = FullVolumeDistance, attenuation = Attenuation;
	ChannelFalloff( channel, fullVolume, attenuation );
	std::array<float, 3> source;
	float squared = 0;
	for ( std::size_t i = 0; i < source.size(); ++i ) {
		source[i] = origin[i] - listener[i];
		squared += source[i] * source[i];
	}
	const float distance = std::sqrt( squared );
	if ( !std::isfinite( distance ) ) {
		return gains;
	}
	const float beyond = ( std::max )( 0.0f, distance - fullVolume );
	const float scale = 1.0f - beyond * attenuation;
	if ( !( scale > 0 ) ) {
		return gains;
	}
	float dot = 0;
	if ( distance > 0 && axis ) {
		const float reciprocal = 1.0f / distance;
		for ( std::size_t i = 0; i < source.size(); ++i ) {
			dot += ( source[i] * reciprocal ) * axis[1][i];
		}
		dot = -dot;
	}
	if ( !std::isfinite( dot ) ) {
		return gains;
	}
	dot = std::clamp( dot, -1.0f, 1.0f );
	separation = std::clamp( separation, 0.0f, 1.0f );
	float right = MonoScale, left = MonoScale;
	if ( !mono ) {
		right = ( std::max )( 0.0f, dot * ( 1.0f - separation ) + separation );
		left = ( std::max )( 0.0f, separation - dot * ( 1.0f - separation ) );
	}
	const auto gainFor = [&]( float side ) {
		const int value = static_cast<int>( scale * side * volume );
		return ( std::max )( 0, ( std::min )( 255, value ) );
	};
	gains.right = gainFor( right );
	gains.left = gainFor( left );
	return gains;
}

/* The gains of every loop playing one sound, summed as retail sums them. */
inline Gains SpatializeMerged( const AmbientLoopFrame &loops, std::size_t first,
	const float *listener, const float axis[3][3], bool mono = false, float separation = Separation ) {
	Gains total;
	if ( first >= loops.Size() ) {
		return total;
	}
	const int handle = loops[first].handle;
	for ( std::size_t i = first; i < loops.Size(); ++i ) {
		if ( loops[i].handle != handle ) {
			continue;
		}
		const Gains gains = Spatialize( loops[i].origin.data(), static_cast<float>( loops[i].volume ),
			listener, axis, mono, separation );
		total.left += gains.left;
		total.right += gains.right;
	}
	total.left = ( std::min )( 255, total.left );
	total.right = ( std::min )( 255, total.right );
	return total;
}

} // namespace fntech3::audio::stvef
#endif
