/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_STVEF_LIPSYNC_H
#define FNTECH3_AUDIO_STVEF_LIPSYNC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

/*
 * How wide a mouth Elite Force single player opens while a voice plays.
 *
 * The game asks the engine for one number an entity, which it reads through
 * the S_Override slot of its import table (stvoy.exe 0x42562b). Retail fills
 * that array while it mixes (0x421480): it clears every entity, then for each
 * channel playing a voice or an attenuated voice takes ten samples of the
 * sound, one every hundred from wherever the channel has reached, and squares
 * the top byte of each (0x421520). The mean of those squares is compared with
 * s_threshold1 through s_threshold4, each times the loudest byte the sound
 * itself reaches, and the answer is a closed mouth or one of four widths. A
 * voice starting resets its entity to a closed mouth (0x4208ec).
 *
 * Retail keeps two arrays of an entity each, and they say different things.
 * The one the game holds is cleared at the start of every mix and written
 * only for the channels playing a voice then, so an entity saying nothing
 * reads neither a width nor a closed mouth but no answer at all. The other is
 * the engine's own memory of what each entity last measured; it survives the
 * mix, and answers for a channel whose turn to be measured has not come round
 * again, since no channel is measured more often than every 800 samples of
 * mixed sound (0x42153e, 0x4214fc).
 *
 * The samples are read signed and shifted arithmetically, so each is a byte
 * of -128 through 127 and its square at most 16,384; the mean stays a whole
 * number. The loudest byte is what Raven's own "#MAXVOL" tag reports for an
 * MP3; a sound without one counts as 128 (0x408df2).
 */

namespace fntech3::audio::stvef::lipsync {

// Retail's answer for a mouth that is closed, and the four widths above it.
inline constexpr std::int32_t Closed = -1;
inline constexpr std::int32_t NarrowestOpen = 5;
inline constexpr std::int32_t WidestOpen = 8;

// What an entity with nothing to say reads, and what retail answers for a
// channel that has nothing left to measure (0x421617).
inline constexpr std::int32_t Unmeasured = 0;

// How many samples of the sound are looked at, and how far apart they are.
inline constexpr int Samples = 10;
inline constexpr int SampleStride = 100;
// No channel is measured again until this many samples have been mixed.
inline constexpr int RefreshSamples = 800;
// The loudest byte a sound without a tag of its own counts as.
inline constexpr float DefaultPeak = 128.0f;

// The four thresholds, in the order retail compares them, and its defaults.
struct Thresholds {
	float closed = 0.5f;
	float narrow = 4.0f;
	float wide = 7.0f;
	float widest = 8.0f;
};

/* How loud the sound is where the channel has reached: the mean of the
 * squares of the top byte of ten samples, one every hundred, all of them
 * within the sound. Retail keeps that mean a whole number. */
template <typename SampleAt>
inline float Energy( SampleAt sampleAt, int offset, int length ) {
	std::int32_t total = 0;
	int counted = 0;
	for ( int i = 0; i < Samples; ++i ) {
		const int at = offset + i * SampleStride;
		if ( at > length ) {
			break;
		}
		const std::int32_t sample = static_cast<std::int32_t>( sampleAt( at ) ) >> 8;
		total += sample * sample;
		++counted;
	}
	if ( counted == 0 ) {
		return 0.0f;
	}
	return static_cast<float>( total / counted );
}

/* Whether any of the ten samples fell inside the sound. A channel that has
 * played past its end measures nothing. */
inline bool Measurable( int offset, int length ) {
	return offset <= length;
}

/* How wide the mouth that loudness opens, against the loudest byte the sound
 * reaches. A sound quieter than the first threshold closes it. */
inline std::int32_t Level( float energy, float peak, const Thresholds &thresholds = {} ) {
	if ( !( peak > 0 ) ) {
		peak = DefaultPeak;
	}
	if ( thresholds.closed * peak > energy ) {
		return Closed;
	}
	if ( thresholds.narrow * peak > energy ) {
		return 5;
	}
	if ( thresholds.wide * peak > energy ) {
		return 6;
	}
	return ( thresholds.widest * peak <= energy ) ? 8 : 7;
}

/* One sound's answer: how wide a mouth the channel's own position opens, or
 * none where it has played past the end of the sound (0x421520). */
template <typename SampleAt>
inline std::int32_t Measure( SampleAt sampleAt, int offset, int length, float peak,
	const Thresholds &thresholds = {} ) {
	if ( !Measurable( offset, length ) ) {
		return Unmeasured;
	}
	return Level( Energy( sampleAt, offset, length ), peak, thresholds );
}

/* The top byte of each of a sound's samples, which is all a measurement
 * looks at. The legacy mixer reads the sound it keeps for itself; the OpenAL
 * backend hands its samples to the device and keeps none, so it keeps these
 * instead, at a byte a sample rather than the two a sample costs. */
class TopBytes {
public:
	void Clear() {
		std::vector<std::int8_t>().swap( bytes_ );
	}

	void Build( const short *pcm, int frames, int channels ) {
		Clear();
		if ( !pcm || frames <= 0 || channels < 1 || channels > 8 ) {
			return;
		}
		bytes_.resize( static_cast<std::size_t>( frames ) );
		for ( int i = 0; i < frames; ++i ) {
			const int sample = pcm[static_cast<std::size_t>( i ) * channels];
			bytes_[static_cast<std::size_t>( i )] = static_cast<std::int8_t>( sample >> 8 );
		}
	}

	bool Empty() const {
		return bytes_.empty();
	}

	int Frames() const {
		return static_cast<int>( bytes_.size() );
	}

	// The sample as the measurement would have read it.
	short At( int frame ) const {
		if ( frame < 0 || static_cast<std::size_t>( frame ) >= bytes_.size() ) {
			return 0;
		}
		return static_cast<short>( bytes_[static_cast<std::size_t>( frame )] << 8 );
	}

private:
	std::vector<std::int8_t> bytes_;
};

/* The mouths the game reads and the ones the engine remembers. Every entity
 * is answered, so the arrays are as long as retail's own (0x421493). */
class Mouths {
public:
	static constexpr std::size_t Entities = 1024;

	// Every mix starts the game's own array with nothing said (0x421493).
	void Clear() {
		levels_.fill( Unmeasured );
	}

	// Starting the sound buffer again forgets everything (0x420a97).
	void Reset() {
		Clear();
		remembered_.fill( Unmeasured );
		next_ = 0;
	}

	// A voice starting closes that entity's mouth (0x4208ec).
	void Close( int entity ) {
		Write( entity, Closed );
	}

	void Write( int entity, std::int32_t level ) {
		if ( Holds( entity ) ) {
			levels_[Index( entity )] = level;
		}
	}

	std::int32_t Mouth( int entity ) const {
		return Holds( entity ) ? levels_[Index( entity )] : Unmeasured;
	}

	/* Whether this channel is measured again, or answers with what its
	 * entity last measured. Retail measures a channel that has only just
	 * started wherever the mix has reached, and otherwise leaves every
	 * channel alone until the wait has run out (0x42153e). */
	bool Measures( int startSample, int position, int mixedSamples ) const {
		return startSample == position || next_ < mixedSamples;
	}

	std::int32_t Remembered( int entity ) const {
		return Holds( entity ) ? remembered_[Index( entity )] : Unmeasured;
	}

	// Only a fresh measurement is remembered (0x421651).
	void Remember( int entity, std::int32_t level ) {
		if ( Holds( entity ) ) {
			remembered_[Index( entity )] = level;
		}
	}

	/* The wait begins again once a mix has measured what it needed to
	 * (0x4214fc). */
	void Mixed( int mixedSamples ) {
		if ( next_ < mixedSamples ) {
			next_ = mixedSamples + RefreshSamples;
		}
	}

	const std::int32_t *Levels() const {
		return levels_.data();
	}

private:
	static bool Holds( int entity ) {
		return entity >= 0 && entity < static_cast<int>( Entities );
	}

	static std::size_t Index( int entity ) {
		return static_cast<std::size_t>( entity );
	}

	std::array<std::int32_t, Entities> levels_{};
	std::array<std::int32_t, Entities> remembered_{};
	int next_ = 0;
};

/* One channel's answer, remembered as retail remembers it: a channel that has
 * only just started, or one the wait has run out on, is measured afresh and
 * kept; any other answers with what its entity last measured. A channel with
 * nothing left to measure answers with none, and is not kept (0x421520). */
template <typename SampleAt>
inline std::int32_t Answer( Mouths &mouths, SampleAt sampleAt, int entity,
	int startSample, int position, int mixedSamples, int length, float peak,
	const Thresholds &thresholds = {} ) {
	if ( !mouths.Measures( startSample, position, mixedSamples ) ) {
		return mouths.Remembered( entity );
	}
	const std::int32_t level =
		Measure( sampleAt, position - startSample, length, peak, thresholds );
	if ( level != Unmeasured ) {
		mouths.Remember( entity, level );
	}
	return level;
}

} // namespace fntech3::audio::stvef::lipsync
#endif
