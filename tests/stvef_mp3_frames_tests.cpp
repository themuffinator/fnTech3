/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 *
 * Frame-walk rules of Elite Force's MP3 unpacking, driven by a scripted fake
 * decoder so no MPEG data or retail file is needed. The real decoder is
 * exercised against every retail file by scripts/probe_stvef_mp3.py. */
#include "../code/client/audio/codecs/snd_mp3_frames.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

using fntech3::audio::Mp3ContinuesStream;
using fntech3::audio::Mp3FrameDecoder;
using fntech3::audio::Mp3HeaderFrameBytes;
using fntech3::audio::Mp3UnpaddedFrameBytes;

namespace {

// A stream of synthetic frames: each starts with a 4-byte MPEG-like header
// whose third byte carries the padding bit; the fake decoder reads sizes from
// a table keyed by frame index.
struct Frame { int bytes; bool padded; int channels; int rate; int samples; short value; };

struct Fake {
	const std::uint8_t *base = nullptr;
	std::vector<Frame> frames;
	std::vector<int> offsets;
	int calls = 0;

	static int Decode( void *self, const std::uint8_t *data, int bytes, short *pcm,
		int *frameBytes, int *frameOffset, int *channels, int *rate ) {
		auto *fake = static_cast<Fake *>( self );
		++fake->calls;
		*frameBytes = *frameOffset = *channels = *rate = 0;
		const int position = static_cast<int>( data - fake->base );
		for ( std::size_t i = 0; i < fake->frames.size(); ++i ) {
			if ( fake->offsets[i] != position ) continue;
			const Frame &frame = fake->frames[i];
			if ( frame.bytes > bytes ) return 0; // incomplete frame: no sync
			*frameBytes = frame.bytes;
			*channels = frame.channels;
			*rate = frame.rate;
			if ( pcm ) {
				for ( int s = 0; s < frame.samples * frame.channels; ++s ) {
					pcm[s] = static_cast<short>( frame.value + ( frame.channels == 2 ? ( s & 1 ) * 100 : 0 ) );
				}
			}
			return frame.samples;
		}
		return 0;
	}
};

std::vector<std::uint8_t> Build( Fake &fake, const std::vector<Frame> &frames,
	std::size_t prefix = 0, bool id3v1 = false ) {
	std::vector<std::uint8_t> data( prefix, 0 );
	fake.frames = frames;
	fake.offsets.clear();
	for ( const Frame &frame : frames ) {
		fake.offsets.push_back( static_cast<int>( data.size() ) );
		std::vector<std::uint8_t> bytes( frame.bytes, 0x11 );
		bytes[0] = 0xff; bytes[1] = 0xfb; bytes[2] = frame.padded ? 0x02 : 0x00; bytes[3] = 0xc4;
		data.insert( data.end(), bytes.begin(), bytes.end() );
	}
	if ( id3v1 ) {
		std::vector<std::uint8_t> tag( 128, 0 );
		std::memcpy( tag.data(), "TAG", 3 );
		std::memcpy( tag.data() + 97, "#UNCOMP 1", 9 );
		data.insert( data.end(), tag.begin(), tag.end() );
	}
	return data;
}

template<class Decoder>
int ReadAll( Mp3FrameDecoder &walker, Decoder &fake, int channels, int rate, int output,
	std::vector<short> &pcm, int chunkBytes ) {
	int total = 0;
	for ( ;; ) {
		if ( pcm.size() < static_cast<std::size_t>( total / 2 + chunkBytes / 2 ) )
			pcm.resize( total / 2 + chunkBytes / 2 );
		const int got = walker.Read( &fake, &Decoder::Decode, channels, rate, output,
			pcm.data() + total / 2, chunkBytes );
		assert( got >= 0 && got % ( output * 2 ) == 0 && got <= chunkBytes );
		if ( !got ) break;
		total += got;
	}
	return total;
}

void TestUnpaddedSizes() {
	const std::uint8_t unpadded[4] = { 0xff, 0xfb, 0x00, 0xc4 };
	const std::uint8_t padded[4] = { 0xff, 0xfb, 0x02, 0xc4 };
	const std::uint8_t layer1[4] = { 0xff, 0xff, 0x02, 0xc4 };
	assert( Mp3UnpaddedFrameBytes( unpadded, 417 ) == 417 );
	assert( Mp3UnpaddedFrameBytes( padded, 418 ) == 417 );
	assert( Mp3UnpaddedFrameBytes( layer1, 420 ) == 416 );
	assert( Mp3UnpaddedFrameBytes( padded, 1 ) == 0 );
	assert( Mp3UnpaddedFrameBytes( nullptr, 417 ) == 0 );
	assert( Mp3UnpaddedFrameBytes( unpadded, 0 ) == 0 );
}

// Retail keeps decoding while the remaining bytes reach the FIRST frame's
// unpadded size: a padded first frame does not stop a following unpadded
// frame of the same base size, but a shorter final frame is never decoded.
void TestRetailFrameRule() {
	Fake fake;
	auto walker = std::make_unique<Mp3FrameDecoder>();
	const auto data = Build( fake, {
		{ 105, true, 1, 44100, 1152, 7 },  // Xing-style first frame, decoded as audio
		{ 104, false, 1, 44100, 1152, 8 },
		{ 105, true, 1, 44100, 1152, 9 },
		{ 103, false, 1, 44100, 1152, 10 } // shorter than the first unpadded 104
	}, 0, true );
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	fake.base = data.data();
	const auto info = walker->Scan( &fake, &Fake::Decode );
	assert( info.valid && info.channels == 1 && info.rate == 44100 && info.frames == 3 * 1152 );
	std::vector<short> pcm;
	for ( int chunk : { 2, 998, 2304, 4096, 1 << 20 } ) {
		walker->Reset( data.data(), static_cast<int>( data.size() ) );
		const int bytes = ReadAll( *walker, fake, 1, 44100, 1, pcm, chunk );
		if ( chunk == 1 << 20 ) assert( bytes == 3 * 1152 * 2 );
		else assert( bytes == 3 * 1152 * 2 || chunk < 2 );
		assert( pcm[0] == 7 && pcm[1152] == 8 && pcm[2 * 1152] == 9 );
	}
	// An exactly equal final frame is decoded.
	const auto equal = Build( fake, {
		{ 105, true, 1, 44100, 1152, 1 }, { 104, false, 1, 44100, 1152, 2 } } );
	walker->Reset( equal.data(), static_cast<int>( equal.size() ) );
	fake.base = equal.data();
	assert( walker->Scan( &fake, &Fake::Decode ).frames == 2 * 1152 );
}

// Effects unpack stereo sources as mono; streams keep both channels.
void TestMonoMixdownAndStreams() {
	Fake fake;
	auto walker = std::make_unique<Mp3FrameDecoder>();
	const auto data = Build( fake, {
		{ 418, false, 2, 44100, 1152, 100 }, { 418, false, 2, 44100, 1152, 300 } } );
	fake.base = data.data();
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	const auto info = walker->Scan( &fake, &Fake::Decode );
	assert( info.valid && info.channels == 2 && info.frames == 2304 );
	std::vector<short> pcm;
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	assert( ReadAll( *walker, fake, 2, 44100, 1, pcm, 1000 ) == 2304 * 2 );
	assert( pcm[0] == 150 && pcm[1151] == 150 && pcm[1152] == 350 );
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	assert( ReadAll( *walker, fake, 2, 44100, 2, pcm, 1002 ) == 2304 * 4 );
	assert( pcm[0] == 100 && pcm[1] == 200 && pcm[2 * 1152] == 300 && pcm[2 * 1152 + 1] == 400 );
	// A mono source cannot be widened, and invalid requests produce nothing.
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	assert( !walker->Read( &fake, &Fake::Decode, 1, 44100, 2, pcm.data(), 64 ) );
	assert( !walker->Read( &fake, &Fake::Decode, 2, 44100, 1, nullptr, 64 ) );
	assert( !walker->Read( &fake, nullptr, 2, 44100, 1, pcm.data(), 64 ) );
	assert( !walker->Read( &fake, &Fake::Decode, 3, 44100, 1, pcm.data(), 64 ) );
	assert( !walker->Read( &fake, &Fake::Decode, 2, 44100, 1, pcm.data(), 1 ) );
}

// Parameter changes end the stream; junk frames with no samples are skipped.
void TestConsistencyAndTags() {
	Fake fake;
	auto walker = std::make_unique<Mp3FrameDecoder>();
	const auto data = Build( fake, {
		{ 104, false, 1, 44100, 0, 0 },     // header-only frame: no samples
		{ 104, false, 1, 44100, 1152, 4 },
		{ 104, false, 2, 44100, 1152, 5 },  // channel change ends the stream
		{ 104, false, 1, 44100, 1152, 6 } }, 10 );
	// A leading ID3v2 tag of 0 payload bytes precedes the first frame.
	std::vector<std::uint8_t> tagged = data;
	const std::uint8_t id3[10] = { 'I', 'D', '3', 3, 0, 0, 0, 0, 0, 0 };
	std::memcpy( tagged.data(), id3, sizeof( id3 ) );
	fake.base = tagged.data();
	walker->Reset( tagged.data(), static_cast<int>( tagged.size() ) );
	const auto info = walker->Scan( &fake, &Fake::Decode );
	assert( info.valid && info.frames == 1152 );
	std::vector<short> pcm;
	walker->Reset( tagged.data(), static_cast<int>( tagged.size() ) );
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 1152 * 2 && pcm[0] == 4 );

	// Rate changes also end the stream.
	const auto rates = Build( fake, {
		{ 104, false, 1, 44100, 1152, 1 }, { 104, false, 1, 22050, 1152, 2 } } );
	fake.base = rates.data();
	walker->Reset( rates.data(), static_cast<int>( rates.size() ) );
	assert( walker->Scan( &fake, &Fake::Decode ).frames == 1152 );

	// Empty, tag-only and frame-less inputs are invalid.
	walker->Reset( nullptr, 100 );
	assert( !walker->Scan( &fake, &Fake::Decode ).valid );
	std::vector<std::uint8_t> onlyTag( 128, 0 );
	std::memcpy( onlyTag.data(), "TAG", 3 );
	walker->Reset( onlyTag.data(), static_cast<int>( onlyTag.size() ) );
	fake.base = onlyTag.data();
	fake.frames.clear(); fake.offsets.clear();
	assert( !walker->Scan( &fake, &Fake::Decode ).valid );
	assert( !walker->Scan( &fake, nullptr ).valid );
	// An oversized ID3v2 size clamps to the end rather than reading beyond it.
	std::vector<std::uint8_t> huge( 32, 0 );
	const std::uint8_t big[10] = { 'I', 'D', '3', 4, 0, 0x10, 0x7f, 0x7f, 0x7f, 0x7f };
	std::memcpy( huge.data(), big, sizeof( big ) );
	walker->Reset( huge.data(), static_cast<int>( huge.size() ) );
	fake.base = huge.data();
	assert( !walker->Scan( &fake, &Fake::Decode ).valid );
}

// A decoder that claims more bytes than remain must not advance past the end.
void TestHostileDecoder() {
	struct Liar {
		static int Decode( void *, const std::uint8_t *, int bytes, short *, int *frameBytes,
			int *frameOffset, int *channels, int *rate ) {
			*frameBytes = bytes + 1; *frameOffset = 0; *channels = 1; *rate = 44100;
			return 1152;
		}
		static int BadOffset( void *, const std::uint8_t *, int bytes, short *, int *frameBytes,
			int *frameOffset, int *channels, int *rate ) {
			*frameBytes = bytes; *frameOffset = bytes; *channels = 1; *rate = 44100;
			return 1152;
		}
		static int TooMany( void *, const std::uint8_t *, int bytes, short *, int *frameBytes,
			int *frameOffset, int *channels, int *rate ) {
			*frameBytes = bytes; *frameOffset = 0; *channels = 1; *rate = 44100;
			return 5000;
		}
	};
	std::vector<std::uint8_t> data( 400, 0 );
	data[0] = 0xff; data[1] = 0xfb;
	auto walker = std::make_unique<Mp3FrameDecoder>();
	for ( auto decode : { &Liar::Decode, &Liar::BadOffset, &Liar::TooMany } ) {
		walker->Reset( data.data(), static_cast<int>( data.size() ) );
		assert( !walker->Scan( nullptr, decode ).valid );
		short pcm[4096];
		walker->Reset( data.data(), static_cast<int>( data.size() ) );
		assert( !walker->Read( nullptr, decode, 1, 44100, 1, pcm, sizeof( pcm ) ) );
	}
}

// Frame sizes read from real MPEG headers: bitrate, sample rate, version,
// layer and padding; invalid, free-format and short input size nothing.
void TestHeaderFrameSizes() {
	struct Case { std::uint8_t header[4]; int bytes; };
	const Case cases[] = {
		{ { 0xff, 0xfb, 0x50, 0xc4 }, 208 },  // MPEG-1 Layer III, 64 kbps, 44.1 kHz
		{ { 0xff, 0xfb, 0x52, 0xc4 }, 209 },  // the same, padded
		{ { 0xff, 0xfb, 0x90, 0xc4 }, 417 },  // 128 kbps
		{ { 0xff, 0xfa, 0x92, 0xc4 }, 418 },  // padded, with a CRC
		{ { 0xff, 0xfb, 0xe0, 0xc4 }, 1044 }, // 320 kbps
		{ { 0xff, 0xfb, 0x94, 0xc4 }, 384 },  // 128 kbps at 48 kHz
		{ { 0xff, 0xfb, 0x98, 0xc4 }, 576 },  // 128 kbps at 32 kHz
		{ { 0xff, 0xf3, 0x80, 0xc4 }, 208 },  // MPEG-2 Layer III: 576 samples at 22.05 kHz
		{ { 0xff, 0xe3, 0x10, 0xc4 }, 52 },   // MPEG 2.5 Layer III, 8 kbps at 11.025 kHz
		{ { 0xff, 0xff, 0x12, 0xc4 }, 36 },   // Layer I, padded: four-byte slots
		{ { 0xff, 0xfd, 0xa4, 0xc4 }, 576 },  // Layer II, 192 kbps at 48 kHz
		{ { 0xff, 0xfb, 0x00, 0xc4 }, 0 },    // free format
		{ { 0xff, 0xfb, 0x02, 0xc4 }, 0 },    // free format, padded
		{ { 0xff, 0xfb, 0xf0, 0xc4 }, 0 },    // bitrate index 15
		{ { 0xff, 0xfb, 0x5c, 0xc4 }, 0 },    // reserved sample rate
		{ { 0xff, 0xeb, 0x50, 0xc4 }, 0 },    // reserved version
		{ { 0xff, 0xf9, 0x50, 0xc4 }, 0 },    // reserved layer
		{ { 0xff, 0xe5, 0x10, 0xc4 }, 0 },    // MPEG 2.5 Layer II does not exist
		{ { 0xfe, 0xfb, 0x50, 0xc4 }, 0 },    // no sync
		{ { 0xff, 0x1b, 0x50, 0xc4 }, 0 },
	};
	for ( const Case &c : cases ) assert( Mp3HeaderFrameBytes( c.header, 4 ) == c.bytes );
	assert( Mp3HeaderFrameBytes( cases[0].header, 3 ) == 0 );
	assert( Mp3HeaderFrameBytes( nullptr, 4 ) == 0 );

	// The next frame continues a stream with the same version, layer and
	// sample rate, whatever its bitrate, padding or CRC.
	const std::uint8_t frame[4] = { 0xff, 0xfb, 0x50, 0xc4 };
	const std::uint8_t next[4] = { 0xff, 0xfa, 0x62, 0xc4 };
	const std::uint8_t rate[4] = { 0xff, 0xfb, 0x54, 0xc4 };
	const std::uint8_t layer[4] = { 0xff, 0xfd, 0x50, 0xc4 };
	const std::uint8_t version[4] = { 0xff, 0xf3, 0x50, 0xc4 };
	const std::uint8_t freeFormat[4] = { 0xff, 0xfb, 0x00, 0xc4 };
	const std::uint8_t stray[4] = {};
	assert( Mp3ContinuesStream( frame, next, 4 ) );
	assert( !Mp3ContinuesStream( frame, next, 3 ) );
	for ( const std::uint8_t *other : { rate, layer, version, freeFormat, stray } ) {
		assert( !Mp3ContinuesStream( frame, other, 4 ) );
	}
}

// A decoder that, as minimp3's mp3dec_decode_frame does, trusts a frame only
// when its input ends with that frame or the four bytes after the frame are a
// valid header of the same stream (minimp3's hdr_compare, restated here); a
// frame not at the start of its input is never trusted by ending it. It
// resyncs to the first frame it trusts, or skips everything it was given.
struct Confirming {
	const std::uint8_t *base = nullptr;
	std::vector<int> offsets, sizes;
	int silent = -1;         // a frame found but decoded to nothing
	std::vector<int> handed; // bytes handed with each call
	std::vector<int> ends;   // where each call's input ended

	static bool SameStream( const std::uint8_t *h1, const std::uint8_t *h2 ) {
		const bool valid = h2[0] == 0xff && ( ( h2[1] & 0xf0 ) == 0xf0 || ( h2[1] & 0xfe ) == 0xe2 )
			&& ( ( h2[1] >> 1 ) & 3 ) && ( h2[2] >> 4 ) != 15 && ( ( h2[2] >> 2 ) & 3 ) != 3;
		return valid && !( ( h1[1] ^ h2[1] ) & 0xfe ) && !( ( h1[2] ^ h2[2] ) & 0x0c )
			&& ( ( h1[2] >> 4 ) == 0 ) == ( ( h2[2] >> 4 ) == 0 );
	}

	static int Decode( void *self, const std::uint8_t *data, int bytes, short *pcm,
		int *frameBytes, int *frameOffset, int *channels, int *rate ) {
		auto *fake = static_cast<Confirming *>( self );
		const int position = static_cast<int>( data - fake->base );
		fake->handed.push_back( bytes );
		fake->ends.push_back( position + bytes );
		*frameOffset = *channels = *rate = 0;
		*frameBytes = bytes;
		for ( std::size_t i = 0; i < fake->offsets.size(); ++i ) {
			const int skip = fake->offsets[i] - position;
			const int size = fake->sizes[i];
			if ( skip < 0 || skip + size > bytes ) continue;
			const std::uint8_t *frame = data + skip;
			if ( skip + size == bytes ? skip != 0
				: bytes - skip - size < 4 || !SameStream( frame, frame + size ) ) continue;
			*frameOffset = skip;
			*frameBytes = skip + size;
			*channels = 1;
			*rate = 44100;
			if ( fake->silent == static_cast<int>( i ) ) return 0;
			if ( pcm ) {
				for ( int s = 0; s < 1152; ++s ) pcm[s] = static_cast<short>( 20 + i );
			}
			return 1152;
		}
		return 0;
	}
};

// Real MPEG-1 Layer III frames: header bytes and the size they declare.
struct RealFrame { std::uint8_t b2; int bytes; };
constexpr RealFrame k64 = { 0x50, 208 }, k64Padded = { 0x52, 209 }, k80Padded = { 0x62, 262 },
	k96 = { 0x70, 313 }, k32 = { 0x10, 104 };

std::vector<std::uint8_t> BuildReal( Confirming &fake, const std::vector<RealFrame> &frames,
	const std::vector<std::uint8_t> &trailer, bool id3v1 ) {
	std::vector<std::uint8_t> data;
	fake.offsets.clear();
	fake.sizes.clear();
	for ( const RealFrame &frame : frames ) {
		fake.offsets.push_back( static_cast<int>( data.size() ) );
		fake.sizes.push_back( frame.bytes );
		std::vector<std::uint8_t> bytes( frame.bytes, 0 );
		bytes[0] = 0xff; bytes[1] = 0xfb; bytes[2] = frame.b2; bytes[3] = 0xc4;
		data.insert( data.end(), bytes.begin(), bytes.end() );
	}
	data.insert( data.end(), trailer.begin(), trailer.end() );
	if ( id3v1 ) {
		std::vector<std::uint8_t> tag( 128, 0 );
		std::memcpy( tag.data(), "TAG", 3 );
		data.insert( data.end(), tag.begin(), tag.end() );
	}
	return data;
}

// Retail decodes every frame from its own header and never looks past it
// (stvoy.exe 0x437320, jk2mp.exe 0x456870, jamp.exe 0x47e2c0), so a last
// frame decodes when stray bytes follow it, as in Jedi Outcast's
// sound/weapons/force/drain.mp3: 93 frames, one byte, then the ID3v1 tag.
// The walker hands such a frame to the decoder alone, on the first attempt.
void TestLastFrameBeforeStrayBytes() {
	const std::vector<RealFrame> frames = { k64, k80Padded, k96, k64Padded, k64Padded };
	const int streamBytes = 208 + 262 + 313 + 209 + 209;
	const int last = streamBytes - 209;
	// What follows the last frame, and whether that frame must go alone.
	struct Trailer { std::vector<std::uint8_t> bytes; bool alone; };
	const Trailer trailers[] = {
		{ { 0x00 }, true },                            // drain.mp3's one stray byte
		{ { 0x00, 0xff, 0xfb }, true },                // fewer bytes than a header
		{ std::vector<std::uint8_t>( 100, 0 ), true }, // fewer than the first frame's size
		{ std::vector<std::uint8_t>( 300, 0 ), true }, // enough that the walk tries them
		{ { 0xff, 0xfb, 0x50, 0xc4 }, false },         // a header of the stream confirms it
		{ {}, false },                                 // the last frame ends the input
	};
	for ( const Trailer &trailer : trailers ) {
		for ( bool id3v1 : { true, false } ) {
			Confirming fake;
			const auto data = BuildReal( fake, frames, trailer.bytes, id3v1 );
			const int length = static_cast<int>( data.size() );
			const int inputEnd = length - ( id3v1 ? 128 : 0 );
			fake.base = data.data();
			auto walker = std::make_unique<Mp3FrameDecoder>();
			walker->Reset( data.data(), length );
			const auto info = walker->Scan( &fake, &Confirming::Decode );
			assert( info.valid && info.channels == 1 && info.rate == 44100 && info.frames == 5 * 1152 );
			// Every call but the last frame's is handed the rest of the input.
			int alone = 0;
			for ( std::size_t call = 0; call < fake.handed.size(); ++call ) {
				if ( fake.ends[call] == inputEnd ) continue;
				assert( fake.handed[call] == 209 && fake.ends[call] == streamBytes );
				++alone;
			}
			assert( alone == ( trailer.alone ? 1 : 0 ) );
			for ( int chunk : { 2, 998, 2304, 4096, 1 << 20 } ) {
				std::vector<short> pcm;
				walker->Reset( data.data(), length );
				assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, chunk ) == 5 * 1152 * 2 );
				for ( int frame = 0; frame < 5; ++frame ) assert( pcm[frame * 1152] == 20 + frame );
			}
			// Alice's seeks count the last frame too.
			assert( walker->SamplesBefore( &fake, &Confirming::Decode, length ) == 5 * 1152 );
			assert( walker->SamplesBefore( &fake, &Confirming::Decode, last ) == 4 * 1152 );
			assert( walker->SamplesBefore( &fake, &Confirming::Decode, last + 1 ) == 5 * 1152 );
		}
	}

	// A frame the stream does continue is still confirmed by its successor:
	// every frame but the last is handed the rest of the input.
	Confirming fake;
	const auto data = BuildReal( fake, frames, { 0x00 }, true );
	fake.base = data.data();
	auto walker = std::make_unique<Mp3FrameDecoder>();
	walker->Reset( data.data(), static_cast<int>( data.size() ) );
	assert( walker->Scan( &fake, &Confirming::Decode ).frames == 5 * 1152 );
	assert( fake.handed.size() == 5 );
	for ( int i = 0; i < 4; ++i ) assert( fake.ends[i] == streamBytes + 1 );
	assert( fake.handed[4] == 209 && fake.ends[4] == streamBytes );

	// The retail size rule still wins: a last frame shorter than the first
	// frame's unpadded size is never decoded, stray byte or not.
	const auto shorter = BuildReal( fake, { k64, k80Padded, k32 }, { 0x00 }, true );
	fake.base = shorter.data();
	walker->Reset( shorter.data(), static_cast<int>( shorter.size() ) );
	assert( walker->Scan( &fake, &Confirming::Decode ).frames == 2 * 1152 );

	// A frame the decoder finds but decodes nothing from (minimp3, when its
	// bit reservoir lacks the frame's earlier bytes) still places the next
	// frame, which goes alone before the stray byte.
	fake.silent = 0;
	const auto silent = BuildReal( fake, { k64, k64Padded }, { 0x00 }, true );
	fake.base = silent.data();
	walker->Reset( silent.data(), static_cast<int>( silent.size() ) );
	assert( walker->Scan( &fake, &Confirming::Decode ).frames == 1152 );
	std::vector<short> pcm;
	walker->Reset( silent.data(), static_cast<int>( silent.size() ) );
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 1152 * 2 && pcm[0] == 21 );
	fake.silent = -1;

	// One frame and a stray byte: a frame is due at the start, as retail's
	// first frame is wherever its first sync is, and again after a seek there.
	const auto single = BuildReal( fake, { k64Padded }, { 0x00 }, true );
	fake.base = single.data();
	walker->Reset( single.data(), static_cast<int>( single.size() ) );
	assert( walker->Scan( &fake, &Confirming::Decode ).frames == 1152 );
	walker->Reset( single.data(), static_cast<int>( single.size() ) );
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 1152 * 2 && pcm[0] == 20 );
	walker->SeekFileOffset( 0 );
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 1152 * 2 && pcm[0] == 20 );
}

// Alice seeks to a byte inside a frame, where the decoder resyncs (MSS 6.0b
// does too). Bytes there that only look like a header are not handed over as
// a frame, even when nothing continues them; a seek to the start is a start.
void TestSeekIntoFrame() {
	Confirming fake;
	auto data = BuildReal( fake, { k64, k80Padded, k96, k64Padded, k64Padded }, { 0x00 }, true );
	// Inside the third frame (470 to 782), a 262-byte "frame" over the
	// fourth frame's header at 783.
	const std::uint8_t lookalike[4] = { 0xff, 0xfb, 0x62, 0xc4 };
	std::memcpy( data.data() + 600, lookalike, sizeof( lookalike ) );
	const int length = static_cast<int>( data.size() );
	fake.base = data.data();
	auto walker = std::make_unique<Mp3FrameDecoder>();
	walker->Reset( data.data(), length );
	assert( walker->Scan( &fake, &Confirming::Decode ).frames == 5 * 1152 );
	assert( walker->SamplesBefore( &fake, &Confirming::Decode, 600 ) == 3 * 1152 );
	walker->SeekFileOffset( 600 );
	std::vector<short> pcm;
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 2 * 1152 * 2 );
	assert( pcm[0] == 23 && pcm[1152] == 24 );
	walker->SeekFileOffset( 0 );
	assert( ReadAll( *walker, fake, 1, 44100, 1, pcm, 4096 ) == 5 * 1152 * 2 );
	assert( pcm[0] == 20 && pcm[4 * 1152] == 24 );
}

} // namespace

int main() {
	TestUnpaddedSizes();
	TestRetailFrameRule();
	TestMonoMixdownAndStreams();
	TestConsistencyAndTags();
	TestHostileDecoder();
	TestHeaderFrameSizes();
	TestLastFrameBeforeStrayBytes();
	TestSeekIntoFrame();
	return 0;
}
