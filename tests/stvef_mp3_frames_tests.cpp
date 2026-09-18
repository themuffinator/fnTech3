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

using fntech3::audio::Mp3FrameDecoder;
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

int ReadAll( Mp3FrameDecoder &walker, Fake &fake, int channels, int rate, int output,
	std::vector<short> &pcm, int chunkBytes ) {
	int total = 0;
	for ( ;; ) {
		if ( pcm.size() < static_cast<std::size_t>( total / 2 + chunkBytes / 2 ) )
			pcm.resize( total / 2 + chunkBytes / 2 );
		const int got = walker.Read( &fake, &Fake::Decode, channels, rate, output,
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

} // namespace

int main() {
	TestUnpaddedSizes();
	TestRetailFrameRule();
	TestMonoMixdownAndStreams();
	TestConsistencyAndTags();
	TestHostileDecoder();
	return 0;
}
