/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_STVEF_AMBIENT_H
#define FNTECH3_AUDIO_STVEF_AMBIENT_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

/*
 * Elite Force single player's ambient loops. Its ambient sets each hold one
 * looping wave, which the cgame submits every frame with the volume the set
 * has faded to; retail keeps at most 32 of them a frame, merges those that
 * play the same sound and mixes each with its own volume
 * (S_AddAmbientLoopingSound stvoy.exe 0x420bd0, S_AddLoopSounds 0x420ca0).
 *
 * The gains are retail's spatialization (S_SpatializeOrigin 0x420510): full
 * volume within 256 units of the listener, then falling by 0.0008 a unit to
 * silence at 1,506, panned by the listener's right axis and the stereo
 * separation, or both channels at three quarters on a mono device.
 */

namespace fntech3::audio::stvef {

inline constexpr float FullVolumeDistance = 256.0f;
inline constexpr float Attenuation = 0.0008f;
inline constexpr float MonoScale = 0.75f;

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
 * listener, the pan from its right axis; separation is the engine's stereo
 * separation, which retail takes from a cvar. */
inline Gains Spatialize( const float *origin, float volume, const float *listener,
	const float axis[3][3], bool mono = false, float separation = 0.5f ) {
	Gains gains;
	if ( !origin || !listener || !( volume > 0 ) ) {
		return gains;
	}
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
	const float beyond = ( std::max )( 0.0f, distance - FullVolumeDistance );
	const float scale = 1.0f - beyond * Attenuation;
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
	const auto channel = [&]( float side ) {
		const int value = static_cast<int>( scale * side * volume );
		return ( std::max )( 0, ( std::min )( 255, value ) );
	};
	gains.right = channel( right );
	gains.left = channel( left );
	return gains;
}

/* The gains of every loop playing one sound, summed as retail sums them. */
inline Gains SpatializeMerged( const AmbientLoopFrame &loops, std::size_t first,
	const float *listener, const float axis[3][3], bool mono = false, float separation = 0.5f ) {
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
