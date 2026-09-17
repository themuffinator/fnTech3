/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_SPEECH_H
#define FNTECH3_AUDIO_RTCW_SPEECH_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace fntech3::audio::rtcw {

constexpr int SpeechChannel = 3;
constexpr int SpeechClients = 128;
constexpr int SpeechWindow = 100;

// SP anticipates facial interpolation by a quarter second. Use the backend's
// actual sample rate, including rates which the original s_khz did not expose.
inline int SpeechLookahead( int rate ) { return rate > 0 && rate <= 192000 ? rate / 4 : 0; }

// Preserve the original signed accumulation and arithmetic-shift rounding;
// summing absolute samples would open mouths during cancellation/silence.
inline int SpeechContribution( short sample ) {

	const int value = sample;
	if ( value >= -5000 && value <= 5000 ) return 0;
	const int scaled = value * 255;
	return scaled >= 0 ? scaled / 256 : -((-scaled + 255) / 256);
}

inline int SpeechClamp( std::int64_t value ) {
	return value < 25 ? 0 : static_cast<int>((std::min)(value, std::int64_t{255}));
}

inline int SpeechAverage( int sum, int count ) {
	if ( count <= 0 || count > SpeechWindow ) return 0;
	const std::int64_t magnitude = sum < 0 ? -std::int64_t{sum} : sum;
	return SpeechClamp(magnitude / (2 * count));
}

inline int SpeechPCM( const short *samples, int count ) {
	if ( !samples || count <= 0 || count > SpeechWindow ) return 0;
	int sum = 0;
	for ( int i = 0; i < count; ++i ) sum += SpeechContribution(samples[i]);
	return SpeechAverage(sum, count);
}

// The legacy raw ring stores PCM16 multiplied by 256. Streamed speech uses
// a peak over its left channel, independently of spatial and master gains.
inline int SpeechStreamSample( int scaledPCM ) {
	const std::int64_t magnitude = scaledPCM < 0 ? -std::int64_t{scaledPCM} : scaledPCM;
	return SpeechClamp(magnitude / 8000);
}

class SpeechEnvelope {
public:
	void Clear() { std::vector<std::uint8_t>().swap(values_); }
	void Build( const short *pcm, int frames, int channels ) {
		Clear();
		if ( !pcm || frames <= SpeechWindow || channels < 1 || channels > 8 ) return;
		values_.resize(static_cast<std::size_t>(frames - SpeechWindow));
		int sum = 0;
		const auto contribution = [&](std::size_t frame) { return SpeechContribution(pcm[frame * channels]); };
		for ( int i = 0; i < SpeechWindow; ++i ) sum += contribution(i);
		for ( std::size_t i = 0; i < values_.size(); ++i ) {
			values_[i] = static_cast<std::uint8_t>(SpeechAverage(sum, SpeechWindow));
			sum += contribution(i + SpeechWindow) - contribution(i);
		}
	}
	bool At( std::int64_t frame, int &amplitude ) const {
		// Original SP requires a complete window strictly before the sample end.
		if ( frame < 0 || static_cast<std::uint64_t>(frame) >= values_.size() ) return false;
		amplitude = values_[static_cast<std::size_t>(frame)];
		return true;
	}
private:
	std::vector<std::uint8_t> values_;
};

// One entry per successfully queued OpenAL buffer. AL_SAMPLE_OFFSET is relative
// to the whole queue, including processed buffers until they are unqueued.
class SpeechQueue {
public:
	void Clear() { chunks_.clear(); }
	void Pop() { if ( !chunks_.empty() ) chunks_.pop_front(); }
	void Append( const short *pcm, int frames, int channels ) {
		if ( !pcm || frames <= 0 || channels < 1 || channels > 8 ) return;
		auto &chunk = chunks_.emplace_back(static_cast<std::size_t>(frames));
		for ( int i = 0; i < frames; ++i )
			chunk[i] = static_cast<std::uint8_t>(SpeechStreamSample(int(pcm[static_cast<std::size_t>(i) * channels]) * 256));
	}
	int At( std::int64_t frame ) const {
		if ( frame < 0 ) return 0;
		int peak = 0, remaining = SpeechWindow;
		for ( const auto &chunk : chunks_ ) {
			if ( static_cast<std::uint64_t>(frame) >= chunk.size() ) { frame -= chunk.size(); continue; }
			for ( std::size_t i = static_cast<std::size_t>(frame); i < chunk.size() && remaining > 0; ++i, --remaining )
				peak = (std::max)(peak, int(chunk[i]));
			if ( !remaining ) break;
			frame = 0;
		}
		return peak;
	}
private:
	std::deque<std::vector<std::uint8_t>> chunks_;
};

} // namespace fntech3::audio::rtcw
#endif
