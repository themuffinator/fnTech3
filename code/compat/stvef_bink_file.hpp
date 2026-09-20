/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_BINK_FILE_HPP
#define FNTECH3_STVEF_BINK_FILE_HPP

#include <cstddef>
#include <cstdint>

namespace fntech3::stvef::bink {

constexpr std::size_t MaxFileBytes = 256u * 1024u * 1024u;
constexpr std::uint32_t MaxDimension = 2048;

struct FileInfo {
	std::uint32_t width, height, frames, rate, rateDivisor, tracks;
	std::uint32_t soundTrack;
};

inline std::uint32_t Word( const std::uint8_t *p ) {
	return std::uint32_t( p[0] ) | ( std::uint32_t( p[1] ) << 8 ) |
		( std::uint32_t( p[2] ) << 16 ) | ( std::uint32_t( p[3] ) << 24 );
}

/* The BIKi envelope observed in all 42 installed SP movies. Validate the
 * complete seek table and audio packet lengths before lending an in-memory
 * file to the user's native decoder, whose API takes no buffer length. This
 * validates framing, not the proprietary compressed video/audio bitstreams. */
inline bool Inspect( const std::uint8_t *data, std::size_t size, bool german, FileInfo &out ) {
	if ( !data || size < 44 || size > MaxFileBytes || Word( data ) != 0x694b4942u ||
		std::uint64_t( Word( data + 4 ) ) + 8 != size ) return false;
	FileInfo info{};
	info.frames = Word( data + 8 );
	info.width = Word( data + 20 );
	info.height = Word( data + 24 );
	info.rate = Word( data + 28 );
	info.rateDivisor = Word( data + 32 );
	info.tracks = Word( data + 40 );
	if ( !info.frames || info.frames > 1000000 || Word( data + 16 ) != info.frames ||
		!Word( data + 12 ) || Word( data + 12 ) > size ||
		!info.width || !info.height || info.width > MaxDimension || info.height > MaxDimension ||
		!info.rate || !info.rateDivisor || info.rate > 1000000 || info.rateDivisor > 1000000 ||
		info.rate < info.rateDivisor ||
		std::uint64_t( info.rate ) > std::uint64_t( info.rateDivisor ) * 240 || info.tracks > 16 ) return false;
	// Bink 1 honors the file's presentation scaling flags at open (installed
	// binkw32.dll 0x1000807d). Most retail _lo movies contain half-height
	// frames and request doubled height; intro_lo instead stores 256 by 192.
	const auto scale = ( Word( data + 36 ) >> 28 ) & 7u;
	if ( scale == 3 || scale == 4 || scale == 5 ) info.width *= 2;
	if ( scale == 1 || scale == 2 || scale == 4 || scale == 5 ) info.height *= 2;
	if ( scale > 5 || info.width > MaxDimension || info.height > MaxDimension ) return false;
	const std::size_t index = 44 + std::size_t( info.tracks ) * 12;
	const std::size_t firstFrame = index + ( std::size_t( info.frames ) + 1 ) * 4;
	if ( firstFrame > size ) return false;
	for ( std::uint32_t track = 0; track < info.tracks; ++track ) {
		const auto maxDecoded = Word( data + 44 + track * 4 );
		const auto format = Word( data + 44 + info.tracks * 4 + track * 4 );
		if ( !maxDecoded || maxDecoded > 16u * 1024u * 1024u || !( format & 0xffffu ) ) return false;
	}
	std::size_t offset = Word( data + index ) & ~1u;
	if ( offset < firstFrame || offset >= size ) return false;
	for ( std::uint32_t frame = 0; frame < info.frames; ++frame ) {
		const std::size_t next = Word( data + index + ( std::size_t( frame ) + 1 ) * 4 ) & ~1u;
		if ( next <= offset || next > size || next - offset > Word( data + 12 ) ) return false;
		std::size_t packet = offset;
		for ( std::uint32_t track = 0; track < info.tracks; ++track ) {
			if ( next - packet < 4 ) return false;
			const std::size_t bytes = Word( data + packet );
			packet += 4;
			if ( bytes > next - packet || ( bytes && bytes < 4 ) ) return false;
			if ( bytes && Word( data + packet ) > Word( data + 44 + track * 4 ) ) return false;
			packet += bytes;
		}
		if ( packet == next ) return false; // A video frame must remain.
		offset = next;
	}
	if ( offset != size ) return false;
	// Retail chooses track 1 for DEUTSCH and retries track 0 when absent
	// (stvoy.exe 0x403e08, 0x4309f6). Bink's setter takes the stored track ID.
	if ( info.tracks ) {
		const std::uint32_t track = german && info.tracks > 1 ? 1 : 0;
		info.soundTrack = Word( data + 44 + info.tracks * 8 + track * 4 );
	}
	out = info;
	return true;
}

} // namespace fntech3::stvef::bink
#endif
