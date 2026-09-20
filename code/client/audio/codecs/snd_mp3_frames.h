/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SND_MP3_FRAMES_H
#define FNTECH3_SND_MP3_FRAMES_H

#include <cstdint>
#include <cstring>
#include <limits>

/* Frame walker for Elite Force's MP3 files, independent of the decoder
 * library. Retail stvoy.exe (MP3 size/unpack loops at 0x43bf70/0x43c0a0)
 * decodes from the first frame sync while the remaining bytes are at least
 * the first frame's unpadded size, so a shorter final frame is never decoded
 * and a leading Xing frame is decoded as audio. Sound effects are unpacked
 * as mono (stereo sources are mixed down); streamed music keeps its channels.
 * This reproduces every retail "#UNCOMP" size (8,071 effects and 3 tracks);
 * see docs/fntech3/2026-09-18-stvef-contracts.md.
 *
 * A trailing 128-byte ID3v1 tag and a leading ID3v2 tag are excluded before
 * any sync search. Rate and channel count must stay constant; a frame that
 * changes them ends the stream. */
namespace fntech3::audio {

struct Mp3StreamInfo {
	bool valid = false;
	int rate = 0;
	int channels = 0; // source channels in the MP3 frames
	int frames = 0;   // sample frames (one sample per channel)
};

/* Decode or, with pcm == nullptr, only measure the next frame. Returns the
 * samples per channel it produced; frameBytes == 0 means no further frame.
 * frameOffset counts skipped bytes before the frame's sync word. */
using Mp3DecodeFn = int ( * )( void *self, const std::uint8_t *data, int bytes,
	short *pcm, int *frameBytes, int *frameOffset, int *channels, int *rate );

/* The loudest byte Raven's own tag reports for this file, or none where it
 * carries none. Retail takes the number after "#MAXVOL" in the comment field
 * of a trailing ID3v1 tag, and counts a file without one as 128
 * (stvoy.exe 0x408c70, 0x408df2). */
inline float Mp3RavenPeak( const std::uint8_t *data, int length ) noexcept {
	constexpr int kTagBytes = 128;
	constexpr int kCommentOffset = 0x61;
	constexpr int kCommentBytes = 30;
	constexpr int kKeyword = 7; // "#MAXVOL"
	if ( !data || length < kTagBytes ) return 0.0f;
	const char *tag = reinterpret_cast<const char *>( data ) + length - kTagBytes;
	if ( std::memcmp( tag, "TAG", 3 ) != 0 ) return 0.0f;
	const char *comment = tag + kCommentOffset;
	if ( std::memcmp( comment, "#MAXVOL", kKeyword ) != 0 ) return 0.0f;
	int at = kKeyword;
	while ( at < kCommentBytes && ( comment[at] == ' ' || comment[at] == '\t' ) ) ++at;
	int peak = 0;
	bool digits = false;
	for ( ; at < kCommentBytes && comment[at] >= '0' && comment[at] <= '9'; ++at ) {
		peak = peak * 10 + ( comment[at] - '0' );
		digits = true;
	}
	return digits ? static_cast<float>( peak ) : 0.0f;
}

/* Byte size of the frame at 'header' without its padding slot, or 0. */
inline int Mp3UnpaddedFrameBytes( const std::uint8_t *header, int frameBytes ) noexcept {
	if ( !header || frameBytes <= 0 ) return 0;
	const bool padded = ( header[2] >> 1 ) & 1;
	const bool layer1 = ( ( header[1] >> 1 ) & 3 ) == 3;
	const int pad = padded ? ( layer1 ? 4 : 1 ) : 0;
	return frameBytes > pad ? frameBytes - pad : 0;
}

class Mp3FrameDecoder {
public:
	static constexpr int kMaxFrameSamples = 1152 * 2;

	void Reset( const std::uint8_t *data, int length ) noexcept {
		data_ = data;
		length_ = data && length > 0 ? length : 0;
		position_ = 0;
		limit_ = 0;
		frameSamples_ = frameCursor_ = 0;
		if ( length_ >= 128 && !std::memcmp( data_ + length_ - 128, "TAG", 3 ) ) {
			length_ -= 128;
		}
		// ID3v2: "ID3", version, flags, then a 28-bit synchsafe size.
		if ( length_ >= 10 && !std::memcmp( data_, "ID3", 3 )
			&& !( ( data_[6] | data_[7] | data_[8] | data_[9] ) & 0x80 ) ) {
			const std::int64_t size = 10 + ( ( std::int64_t( data_[6] ) << 21 )
				| ( data_[7] << 14 ) | ( data_[8] << 7 ) | data_[9] )
				+ ( ( data_[5] & 0x10 ) ? 10 : 0 );
			position_ = size < length_ ? static_cast<int>( size ) : length_;
		}
		start_ = position_;
	}

	Mp3StreamInfo Scan( void *codec, Mp3DecodeFn decode ) const noexcept {
		Mp3StreamInfo info;
		if ( !decode ) return info;
		std::int64_t total = 0;
		int limit = 0;
		for ( int position = start_; position < length_; ) {
			if ( limit && length_ - position < limit ) break;
			int frameBytes = 0, offset = 0, channels = 0, rate = 0;
			const int samples = decode( codec, data_ + position, length_ - position,
				nullptr, &frameBytes, &offset, &channels, &rate );
			if ( frameBytes <= 0 || frameBytes > length_ - position || offset < 0 || offset >= frameBytes ) break;
			if ( samples > 0 && !limit ) {
				limit = Mp3UnpaddedFrameBytes( data_ + position + offset, frameBytes - offset );
				if ( !limit ) break;
			}
			position += frameBytes;
			if ( samples <= 0 ) continue;
			if ( samples > kMaxFrameSamples / 2 || channels < 1 || channels > 2 || rate <= 0 ) break;
			if ( !info.channels ) {
				info.channels = channels;
				info.rate = rate;
			} else if ( channels != info.channels || rate != info.rate ) {
				break;
			}
			total += samples;
		}
		const std::int64_t maxBytes = ( std::numeric_limits<int>::max )();
		if ( !info.channels || total <= 0 || total * info.channels * 2 > maxBytes ) {
			return Mp3StreamInfo{};
		}
		info.frames = static_cast<int>( total );
		info.valid = true;
		return info;
	}

	/* Copy up to 'bytes' of interleaved 16-bit PCM with 'outputChannels'
	 * (1 mixes a stereo source down); never a partial sample frame. */
	int Read( void *codec, Mp3DecodeFn decode, int sourceChannels, int rate,
		int outputChannels, short *output, int bytes ) noexcept {
		if ( !decode || !output || bytes <= 0 || sourceChannels < 1 || sourceChannels > 2
			|| outputChannels < 1 || outputChannels > sourceChannels ) return 0;
		const int wanted = bytes / ( outputChannels * 2 );
		int written = 0;
		while ( written < wanted ) {
			if ( frameCursor_ >= frameSamples_ && !NextFrame( codec, decode, sourceChannels, rate ) ) break;
			int count = ( frameSamples_ - frameCursor_ ) / sourceChannels;
			if ( count > wanted - written ) count = wanted - written;
			const short *source = frame_ + frameCursor_;
			short *destination = output + static_cast<std::size_t>( written ) * outputChannels;
			if ( outputChannels == sourceChannels ) {
				std::memcpy( destination, source, static_cast<std::size_t>( count ) * sourceChannels * 2 );
			} else {
				for ( int i = 0; i < count; ++i ) {
					destination[i] = static_cast<short>( ( source[2 * i] + source[2 * i + 1] ) / 2 );
				}
			}
			frameCursor_ += count * sourceChannels;
			written += count;
		}
		return written * outputChannels * 2;
	}

private:
	bool NextFrame( void *codec, Mp3DecodeFn decode, int channels, int rate ) noexcept {
		frameSamples_ = frameCursor_ = 0;
		while ( position_ < length_ ) {
			if ( limit_ && length_ - position_ < limit_ ) break;
			int frameBytes = 0, offset = 0, frameChannels = 0, frameRate = 0;
			const int samples = decode( codec, data_ + position_, length_ - position_,
				frame_, &frameBytes, &offset, &frameChannels, &frameRate );
			if ( frameBytes <= 0 || frameBytes > length_ - position_ || offset < 0 || offset >= frameBytes ) break;
			if ( samples > 0 && !limit_ ) {
				limit_ = Mp3UnpaddedFrameBytes( data_ + position_ + offset, frameBytes - offset );
				if ( !limit_ ) break;
			}
			position_ += frameBytes;
			if ( samples <= 0 ) continue;
			if ( samples > kMaxFrameSamples / 2 || frameChannels != channels || frameRate != rate ) break;
			frameSamples_ = samples * channels;
			return true;
		}
		position_ = length_;
		return false;
	}

	const std::uint8_t *data_ = nullptr;
	int length_ = 0;
	int start_ = 0;
	int position_ = 0;
	int limit_ = 0;
	short frame_[kMaxFrameSamples] = {};
	int frameSamples_ = 0;
	int frameCursor_ = 0;
};

} // namespace fntech3::audio

#endif
