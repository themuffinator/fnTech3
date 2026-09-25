/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_LOOPS_H
#define FNTECH3_AUDIO_RTCW_LOOPS_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "RTCWMix.h"

namespace fntech3::audio::rtcw {

// Original RTCW submits a bounded list, not one looping voice per entity.
// The entity argument is unused. These engine-owned records retain no VM data.
struct Loop {
	std::array<float, 3> origin{}, velocity{};
	int handle = 0;
	float range = 1250.0f;
	int volume = 0;
	bool loudUnderwater = false; // Observed metadata; original mixer never reads it.
	// Enemy Territory alone submits the time a loop is considered to have
	// started, so every listener hears it at the same point in its cycle. RTCW
	// submits no such time and leaves this zero, keeping its phase on the
	// mixer's own clock.
	int startTime = 0;
};

class LoopFrame {
public:
	static constexpr std::size_t Capacity = 128;
	void Clear() { count_ = 0; }
	std::size_t Size() const { return count_; }
	bool Accepts( int volume ) const { return volume != 0 && count_ < Capacity; }
	const Loop &operator[]( std::size_t index ) const { return loops_[index]; }

	int Append( const float *origin, const float *velocity, int range, int handle, int volume,
		float master = 1.0f, int startTime = 0 ) {
		if ( !Accepts( volume ) || range < 0 || !origin || !velocity ) return -1;
		Loop next;
		std::memcpy( next.origin.data(), origin, sizeof( next.origin ) );
		std::memcpy( next.velocity.data(), velocity, sizeof( next.velocity ) );
		next.handle = handle;
		next.range = range ? static_cast<float>( range ) : 1250.0f;
		next.volume = LoopVolume( volume, master );
		next.loudUnderwater = ( static_cast<unsigned>( volume ) & 256u ) != 0;
		next.startTime = startTime;
		loops_[count_] = next;
		return static_cast<int>( count_++ );
	}

private:
	std::array<Loop, Capacity> loops_{};
	std::size_t count_ = 0;
};

struct LoopGains { int left = 0, right = 0; };

// New or virtualized OpenAL voices join a common sample cycle. Reduce before
// multiplying so long device clocks cannot overflow the offset calculation.
inline int LoopSampleOffset( std::int64_t microseconds, int rate, int frames ) {
	if ( microseconds < 0 || rate <= 0 || rate > 192000 || frames <= 0 ) return 0;
	const std::int64_t seconds = ( microseconds / 1000000 ) % frames;
	const std::int64_t part = ( microseconds % 1000000 ) * rate / 1000000;
	return static_cast<int>( ( seconds * rate + part ) % frames );
}

// Range is the linear attenuation span after a full-volume radius of 6.4%.
// Quantize spatial volume before applying the submitted 0..255 / 256 factor,
// as in the original mixer. Consumers require IEEE arithmetic (AUDIO_FP_CFLAGS).
inline LoopGains SpatializeOrigin( const float *origin, float range, int volume,
	const float *listener, const float axis[3][3], bool mono = false ) {
	if ( range <= 0 || volume <= 0 ) return {};
	std::array<float, 3> direction;
	float squared = 0;
	for ( std::size_t i = 0; i < direction.size(); ++i ) {
		direction[i] = origin[i] - listener[i];
		squared += direction[i] * direction[i];
	}
	const float distance = std::sqrt( squared );
	// Huge finite coordinates are necessarily outside every ABI-representable
	// range; avoid normalizing infinity or casting a NaN to an integer.
	if ( !std::isfinite( distance ) ) return {};
	const float beyond = (std::max)( 0.0f, distance - range * 0.064f );
	const float fraction = beyond / range;
	// The reference evaluates the subtraction and pan product in double,
	// then stores their product in a float before multiplying by volume.
	const double gain = 1.0 - static_cast<double>(fraction);
	if ( !( gain > 0 ) ) return {};
	float side = 0;
	if ( distance > 0 ) {
		const float reciprocal = 1.0f / distance;
		for ( std::size_t i = 0; i < direction.size(); ++i )
			side += ( direction[i] * reciprocal ) * axis[1][i];
	}
	if ( !std::isfinite( side ) ) return {};
	side = std::clamp( side, -1.0f, 1.0f );
	const auto channel = [&]( float pan ) {
		const float scale = static_cast<float>(gain * pan);
		return static_cast<int>(volume * scale);
	};
	return { channel( mono ? 1.0f : 0.5f * ( 1.0f + side ) ),
		channel( mono ? 1.0f : 0.5f * ( 1.0f - side ) ) };
}

inline LoopGains SpatializeLoop( const Loop &loop, const float *listener,
	const float axis[3][3], bool mono = false ) {
	if ( loop.volume <= 0 ) return {};
	const auto gains = SpatializeOrigin(loop.origin.data(), loop.range, 90, listener, axis, mono);
	const auto channel = [&](int spatial) {
		return (std::min)(255, static_cast<int>(static_cast<float>(loop.volume * spatial) / 256.0f));
	};
	return {channel(gains.left), channel(gains.right)};
}

// Retail's range for every one-shot (SOUND_RANGE_DEFAULT), which a loop
// submitted with range 0 takes too.
constexpr float OneShotRange = 1250.0f;

// The span of the law above, for a renderer whose own distance model carries
// a sound across it: full volume out to 6.4% of the range, silent the range
// beyond that. Nothing for a range the law would not play.
struct LawDistances { float full = 0.0f, silent = 0.0f; };
inline LawDistances Distances( float range ) {
	if ( !( range > 0.0f ) || !std::isfinite( range ) ) return {};
	const float full = range * 0.064f;
	return { full, full + range };
}

// A loop's level where the law is at full volume, on the one-shots' scale of
// 127: its submitted volume over 256 of the loops' 90, capped as retail caps
// each channel. It is SpatializeLoop's figure for a loop at the listener.
inline int LoopLevel( const Loop &loop ) {
	if ( loop.volume <= 0 ) return 0;
	return (std::min)( 255, static_cast<int>( static_cast<float>( loop.volume * 90 ) / 256.0f ) );
}

} // namespace fntech3::audio::rtcw
#endif
