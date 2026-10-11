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
 * Each frame is decoded from its own header without looking past it, as
 * every Raven engine does (stvoy.exe 0x437320, jk2mp.exe 0x456870, jamp.exe
 * 0x47e2c0), so a last frame decodes when stray bytes follow it; see
 * docs/fntech3/2026-10-08-mp3-last-frame.md.
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

/* Byte size of the frame whose header starts at 'header', padding included,
 * read from that header alone (the ISO/IEC 11172-3 and 13818-3 bitrate and
 * sample-rate tables); 0 when fewer than four bytes are available, the
 * header is not valid, or the frame is free format, whose size no header
 * holds. MPEG 2.5 defines Layer III only. */
inline int Mp3HeaderFrameBytes( const std::uint8_t *header, int available ) noexcept {
	// kbps by [MPEG-1][layer field - 1][bitrate index]; layer field 1 is Layer III.
	static constexpr short kKbps[2][3][15] = {
		{ { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
			{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
			{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 } },
		{ { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
			{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
			{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 } } };
	static constexpr int kHz[3] = { 44100, 48000, 32000 };
	if ( !header || available < 4 || header[0] != 0xff || ( header[1] & 0xe0 ) != 0xe0 ) return 0;
	const int version = ( header[1] >> 3 ) & 3; // 0 MPEG 2.5, 1 reserved, 2 MPEG-2, 3 MPEG-1
	const int layer = ( header[1] >> 1 ) & 3;   // 1 Layer III, 2 Layer II, 3 Layer I
	const int bitrate = header[2] >> 4;
	const int rate = ( header[2] >> 2 ) & 3;
	if ( version == 1 || layer == 0 || ( version == 0 && layer != 1 ) || bitrate == 0
		|| bitrate == 15 || rate == 3 ) return 0;
	const bool mpeg1 = version == 3;
	const int hz = kHz[rate] >> ( mpeg1 ? 0 : version == 2 ? 1 : 2 );
	const int samples = layer == 3 ? 384 : ( layer == 1 && !mpeg1 ? 576 : 1152 );
	const int bytes = samples * kKbps[mpeg1][layer - 1][bitrate] * 125 / hz;
	const bool padded = ( header[2] >> 1 ) & 1;
	if ( layer == 3 ) return ( bytes & ~3 ) + ( padded ? 4 : 0 ); // four-byte slots
	return bytes + ( padded ? 1 : 0 );
}

/* Whether 'next', the 'available' bytes after the frame whose header is
 * 'frame', begins another frame of that stream: a sized header with the same
 * MPEG version, layer and sample rate. */
inline bool Mp3ContinuesStream( const std::uint8_t *frame, const std::uint8_t *next, int available ) noexcept {
	return frame && Mp3HeaderFrameBytes( next, available ) > 0 && !( ( frame[1] ^ next[1] ) & 0xfe )
		&& !( ( frame[2] ^ next[2] ) & 0x0c );
}

class Mp3FrameDecoder {
public:
	static constexpr int kMaxFrameSamples = 1152 * 2;

	void Reset( const std::uint8_t *data, int length ) noexcept {
		data_ = data;
		length_ = data && length > 0 ? length : 0;
		position_ = 0;
		limit_ = 0;
		aligned_ = true;
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
		bool aligned = true;
		for ( int position = start_; position < length_; ) {
			if ( limit && length_ - position < limit ) break;
			const int bytes = DecodeBytes( position, aligned );
			int frameBytes = 0, offset = 0, channels = 0, rate = 0;
			const int samples = decode( codec, data_ + position, bytes,
				nullptr, &frameBytes, &offset, &channels, &rate );
			if ( frameBytes <= 0 || frameBytes > bytes || offset < 0 || offset >= frameBytes ) break;
			aligned = channels > 0;
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

	/* The first byte after a leading ID3v2 tag, where decoding begins. */
	int StartOffset() const noexcept { return start_; }

	/* American McGee's Alice alone: continue at a byte offset into the file
	 * image, tag included, as Miles's sample position does (MSS 6.0b
	 * set_sample_position 0x21115200 resyncs its MP3 decoder there). The
	 * next frame is the first sync at or after `offset`; the first frame's
	 * size limit is kept. The caller resets the decoder library as well,
	 * which then resyncs from any byte. */
	void SeekFileOffset( int offset ) noexcept {
		position_ = offset < start_ ? start_ : ( offset > length_ ? length_ : offset );
		aligned_ = position_ == start_;
		frameSamples_ = frameCursor_ = 0;
	}

	/* The samples per channel in the frames whose sync lies before
	 * `offset`: the audio a seek there passes over. */
	std::int64_t SamplesBefore( void *codec, Mp3DecodeFn decode, int offset ) const noexcept {
		std::int64_t total = 0;
		if ( !decode ) return total;
		bool aligned = true;
		for ( int position = start_; position < length_; ) {
			const int bytes = DecodeBytes( position, aligned );
			int frameBytes = 0, skipped = 0, channels = 0, rate = 0;
			const int samples = decode( codec, data_ + position, bytes,
				nullptr, &frameBytes, &skipped, &channels, &rate );
			if ( frameBytes <= 0 || frameBytes > bytes || skipped < 0 || skipped >= frameBytes ) break;
			aligned = channels > 0;
			if ( position + skipped >= offset ) break;
			position += frameBytes;
			if ( samples > 0 && samples <= kMaxFrameSamples / 2 ) total += samples;
		}
		return total;
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
	/* The bytes from 'position' to hand the decoder. Retail decodes each
	 * frame from its own header and never reads past it, but a decoder may
	 * want the next frame's header before it trusts a frame (minimp3 does,
	 * unless the frame ends its input). So where a frame is due, 'aligned'
	 * (the start, or just after a frame the decoder found), a complete frame
	 * that no frame of its stream follows, such as a last frame before stray
	 * bytes, goes alone. After a seek into a frame the decoder resyncs. */
	int DecodeBytes( int position, bool aligned ) const noexcept {
		const int remaining = length_ - position;
		const std::uint8_t *frame = data_ + position;
		const int bytes = aligned ? Mp3HeaderFrameBytes( frame, remaining ) : 0;
		if ( bytes <= 0 || bytes >= remaining ) return remaining;
		return Mp3ContinuesStream( frame, frame + bytes, remaining - bytes ) ? remaining : bytes;
	}

	bool NextFrame( void *codec, Mp3DecodeFn decode, int channels, int rate ) noexcept {
		frameSamples_ = frameCursor_ = 0;
		while ( position_ < length_ ) {
			if ( limit_ && length_ - position_ < limit_ ) break;
			const int bytes = DecodeBytes( position_, aligned_ );
			int frameBytes = 0, offset = 0, frameChannels = 0, frameRate = 0;
			const int samples = decode( codec, data_ + position_, bytes,
				frame_, &frameBytes, &offset, &frameChannels, &frameRate );
			if ( frameBytes <= 0 || frameBytes > bytes || offset < 0 || offset >= frameBytes ) break;
			aligned_ = frameChannels > 0;
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
	bool aligned_ = true;
	short frame_[kMaxFrameSamples] = {};
	int frameSamples_ = 0;
	int frameCursor_ = 0;
};

} // namespace fntech3::audio

#endif
