/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_SKA_FORMAT_HPP
#define FNTECH3_COMPAT_ALICE_SKA_FORMAT_HPP

/* Bounded reader for Alice's `SKAN` skeletal animations.
 *
 * A `.ska` holds one animation's frames for one skeleton's bones. Unlike this
 * title's other model formats it declares no file length and carries no
 * offsets, so nothing inside it can be located by following a directory: the
 * layout is fixed-stride and the file's own size is what proves it.
 *
 * Two generations ship together. Version 3 compressed the pose that version 2
 * stores as floats, which is the whole difference between them:
 *
 *   version 2:  112-byte header, then each frame is 40 bytes followed by one
 *               28-byte pose per bone
 *   version 3:  108-byte header, then each frame is 40 bytes followed by one
 *               16-byte pose per bone
 *
 * A single triple reproduces every installed file's size exactly -- 104 files
 * at version 2 and 1,298 at version 3 -- which is what admits the layout. The
 * pose's own contents are described in the record but not decoded here.
 *
 * No published declaration covers this format; Ritual's F.A.K.K.2 v1.02 SDK
 * predates it. Every field below was derived from the 1,402 installed files.
 * See docs/fntech3/2026-09-22-alice-models.md. */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace model {

/* Ritual's identifier spells "SKAN" in file order. */
inline constexpr std::uint32_t SkaIdent =
	( 'N' << 24 ) | ( 'A' << 16 ) | ( 'K' << 8 ) | 'S';
inline constexpr int SkaVersionLow = 2;
inline constexpr int SkaVersionHigh = 3;

inline constexpr std::size_t SkaNameBytes = 64;
inline constexpr std::size_t SkaHeaderBytesV2 = 112;
inline constexpr std::size_t SkaHeaderBytesV3 = 108;
inline constexpr std::size_t SkaFrameHeaderBytes = 40;
inline constexpr std::size_t SkaPoseBytesV2 = 28;   /* float quat[4], float offset[3] */
inline constexpr std::size_t SkaPoseBytesV3 = 16;   /* short quat[4], then four more shorts */

/* Bounds a retail file never reaches: the longest installed animation is 320
 * frames and the largest skeleton has 131 bones. */
inline constexpr int SkaMaxFrames = 4096;
inline constexpr int SkaMaxBones = 1024;

enum class SkaError {
	None, Size, Ident, Version, Counts, Extent, Name, Timing
};

inline const char *SkaErrorName( SkaError error ) {
	switch ( error ) {
	case SkaError::None: return "none";
	case SkaError::Size: return "file too small";
	case SkaError::Ident: return "wrong identifier";
	case SkaError::Version: return "wrong version";
	case SkaError::Counts: return "count out of range";
	case SkaError::Extent: return "file is not the size its counts require";
	case SkaError::Name: return "unterminated name";
	case SkaError::Timing: return "total time is not the frame count times the frame time";
	}
	return "unknown";
}

struct Ska {
	int version;
	std::size_t name;          /* the 64-byte name field */
	int frames;
	int bones;
	float totalTime;
	float frameTime;
	std::size_t headerBytes;
	std::size_t poseBytes;
	std::size_t frameStride;   /* the frame header plus every bone's pose */
	std::size_t frameData;     /* where the first frame's header begins */
};

/* Where a frame's header and a bone's pose inside it begin. ParseSka has
 * already proved the file is exactly large enough for every one of them. */
inline std::size_t SkaFrameAt( const Ska &ska, int frame ) {
	return ska.frameData + ska.frameStride * std::size_t( frame );
}

inline std::size_t SkaPoseAt( const Ska &ska, int frame, int bone ) {
	return SkaFrameAt( ska, frame ) + SkaFrameHeaderBytes
		+ ska.poseBytes * std::size_t( bone );
}

/*
==================
ParseSka

Read a `SKAN` 2 or 3 animation's shape. Because the format carries no offsets,
the check that matters is that the file is exactly the size its own counts
require: one byte more or fewer and it is not this layout.
==================
*/
inline SkaError ParseSka( const void *data, std::size_t bytes, Ska &out ) {
	out = Ska{};
	if ( !data || bytes < SkaHeaderBytesV3 ) return SkaError::Size;
	const std::uint8_t *at = static_cast<const std::uint8_t *>( data );

	std::uint32_t ident = 0;
	std::memcpy( &ident, at, sizeof( ident ) );
	if ( ident != SkaIdent ) return SkaError::Ident;
	std::memcpy( &out.version, at + 4, sizeof( out.version ) );
	if ( out.version < SkaVersionLow || out.version > SkaVersionHigh ) {
		return SkaError::Version;
	}
	if ( !std::memchr( at + 8, 0, SkaNameBytes ) ) return SkaError::Name;
	out.name = 8;

	/* Version 3 inserted one integer before the counts, which is why the two
	 * generations read the same four fields four bytes apart. */
	const std::size_t counts = out.version == 2 ? 72 : 76;
	std::memcpy( &out.frames, at + counts, sizeof( out.frames ) );
	std::memcpy( &out.bones, at + counts + 4, sizeof( out.bones ) );
	std::memcpy( &out.totalTime, at + counts + 8, sizeof( out.totalTime ) );
	std::memcpy( &out.frameTime, at + counts + 12, sizeof( out.frameTime ) );

	if ( out.frames <= 0 || out.frames > SkaMaxFrames ) return SkaError::Counts;
	if ( out.bones <= 0 || out.bones > SkaMaxBones ) return SkaError::Counts;

	out.headerBytes = out.version == 2 ? SkaHeaderBytesV2 : SkaHeaderBytesV3;
	out.poseBytes = out.version == 2 ? SkaPoseBytesV2 : SkaPoseBytesV3;
	if ( bytes < out.headerBytes ) return SkaError::Size;

	/* Compute the required size without letting the products wrap. */
	const std::size_t poses = out.poseBytes * std::size_t( out.bones );
	if ( poses / out.poseBytes != std::size_t( out.bones ) ) return SkaError::Counts;
	out.frameStride = SkaFrameHeaderBytes + poses;
	const std::size_t body = out.frameStride * std::size_t( out.frames );
	if ( body / out.frameStride != std::size_t( out.frames ) ) return SkaError::Counts;
	if ( body > bytes - out.headerBytes ) return SkaError::Extent;
	/* The file carries no length of its own, so it has to be exactly the size
	 * its counts require. A larger one is a different layout, not a padded
	 * one. */
	if ( out.headerBytes + body != bytes ) return SkaError::Extent;
	out.frameData = out.headerBytes;

	/* Every installed animation's total time is its frame count times its
	 * frame time. That identity is what fixes these four fields, so a file
	 * that breaks it is not read as one of these. */
	if ( !( out.frameTime > 0.0f ) || !( out.totalTime > 0.0f ) ) return SkaError::Timing;
	const float expected = float( out.frames ) * out.frameTime;
	const float slack = expected * 1e-4f;
	if ( out.totalTime < expected - slack || out.totalTime > expected + slack ) {
		return SkaError::Timing;
	}
	return SkaError::None;
}

} // namespace model
} // namespace alice
} // namespace fntech3

#endif
