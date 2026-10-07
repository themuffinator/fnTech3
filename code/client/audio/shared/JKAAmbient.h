/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_JKA_AMBIENT_H
#define FNTECH3_AUDIO_JKA_AMBIENT_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

/*
 * Jedi Academy multiplayer's ambient-set loops for a frame. Retail jamp.exe
 * 1.01 takes each with the volume its set has faded to, out of 255, the level
 * every one of its loops is otherwise added at (S_AddAmbientLoopingSound
 * 0x46b270, S_AddLoopingSound 0x46b250), keeps 32 loops a frame, and plays
 * the loops of one sound as one channel whose volume is their sum, clamped at
 * 255 (S_AddLoopSounds 0x46b350). Here a loop of a sound already added this
 * frame is merged into it the same way; both backends then render one voice
 * per sound through the engine's own loop path.
 */

namespace fntech3::audio::jka {

struct AmbientLoop {
	std::array<float, 3> origin{};
	int handle = 0;
	int volume = 0;
};

class AmbientLoops {
public:
	static constexpr std::size_t Capacity = 32;
	static constexpr int MaxVolume = 255;

	void Clear() { count_ = 0; }
	std::size_t Size() const { return count_; }
	const AmbientLoop &operator[]( std::size_t index ) const { return loops_[index]; }

	/* The index of the loop now holding this sound, or -1 for a loop that is
	 * silent, has no origin or finds the frame full. */
	int Add( const float *origin, int handle, int volume ) {
		if ( !origin || volume <= 0 ) return -1;
		for ( std::size_t i = 0; i < count_; ++i ) {
			if ( loops_[i].handle == handle ) {
				loops_[i].volume = ( std::min )( MaxVolume, loops_[i].volume + volume );
				return static_cast<int>( i );
			}
		}
		if ( count_ >= Capacity ) return -1;
		AmbientLoop &loop = loops_[count_];
		std::memcpy( loop.origin.data(), origin, sizeof( loop.origin ) );
		loop.handle = handle;
		loop.volume = ( std::min )( MaxVolume, volume );
		return static_cast<int>( count_++ );
	}

	/* The loop's level as a scale of the engine's full-volume loop. */
	static float Scale( int volume ) {
		return static_cast<float>( std::clamp( volume, 0, MaxVolume ) ) / static_cast<float>( MaxVolume );
	}

private:
	std::array<AmbientLoop, Capacity> loops_{};
	std::size_t count_ = 0;
};

} // namespace fntech3::audio::jka
#endif
