/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_ROFF_HPP
#define FNTECH3_SERVER_JKA_ROFF_HPP

#include "../compat/jka_game_abi.h"
#include "server_raven_roff.hpp"

#include <climits>
#include <cstring>
#include <string>
#include <vector>

namespace fntech3::jka::roff {

/* Server motion measured in retail jamp.exe (SHA256
 * e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b):
 * ApplyROFF VA 0x426a00 uses shared currentOrigin/currentAngles as trajectory
 * bases, rotates displacement by the starting angles when requested, sets
 * TR_LINEAR, dispatches note lines through export 11, and schedules the next
 * frame in milliseconds. VA 0x426eb0 scales displacement by integer 1000/ms.
 * The final update makes both trajectories stationary and clears mIsRoffing;
 * purge VA 0x4268c0 calls ClearLerp VA 0x426db0, which preserves that flag.
 * OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9 provides matching names and
 * structure definitions; no external implementation text is incorporated. */
class MotionHost {
public:
	virtual ~MotionHost() = default;
	virtual fntech3_jka_sharedEntity_t *Entity( int number ) = 0;
	/* Columns are forward, left and up, as engine AnglesToAxis supplies. */
	virtual void Basis( const float angles[3], float columns[3][3] ) = 0;
	virtual void Note( int entity, const char *text ) = 0;
	virtual void Fail( const char *reason ) = 0;
};

class ServerPlayer {
public:
	ServerPlayer( fntech3::roff::Store &paths, MotionHost &host ) : paths_( paths ), host_( host ) {}

	bool Play( int entity, int handle, bool translate, int now )
	{
		auto *record = host_.Entity( entity );
		if ( !record || !paths_.At( handle ) || !Finite( record->s.apos.trBase )
			|| !Finite( record->r.currentOrigin ) || !Finite( record->r.currentAngles ) ) return false;
		if ( !paths_.Play( entity, handle, translate, now, record->s.apos.trBase ) ) return false;
		record->r.mIsRoffing = 1;
		return true;
	}

	bool Stop( int entity, int now )
	{
		if ( !paths_.Stop( entity ) ) return false;
		if ( auto *record = host_.Entity( entity ) ) Freeze( *record, now, false );
		return true;
	}

	void Clear( int now )
	{
		while ( const auto *playing = paths_.PlayingAt( 0 ) ) Stop( playing->entity, now );
		paths_.Clear();
	}

	void Update( int now )
	{
		paths_.Advance( now,
			[this, now]( const fntech3::roff::Playing &playing, const Sample &sample,
				const fntech3::roff::Cached &path ) { Apply( playing, sample, path, now ); },
			[this, now]( const fntech3::roff::Playing &playing, const fntech3::roff::Cached & ) {
				if ( auto *record = host_.Entity( playing.entity ) ) Freeze( *record, now, true );
			} );
	}

private:
	fntech3::roff::Store &paths_;
	MotionHost &host_;
	inline static constexpr int Stationary = 0;
	inline static constexpr int Linear = 2;

	static bool Finite( const float values[3] )
	{
		for ( int axis = 0; axis < 3; ++axis ) {
			std::uint32_t bits;
			std::memcpy( &bits, &values[axis], sizeof( bits ) );
			volatile std::uint32_t encoded = bits;
			if ( ( encoded & 0x7f800000u ) == 0x7f800000u ) return false;
		}
		return true;
	}

	void Freeze( fntech3_jka_sharedEntity_t &record, int now, bool completed )
	{
		if ( !Finite( record.r.currentOrigin ) || !Finite( record.r.currentAngles ) ) {
			host_.Fail( "Jedi Academy ROFF stationary base is out of range" );
			return;
		}
		record.s.pos.trType = record.s.apos.trType = Stationary;
		record.s.pos.trTime = record.s.apos.trTime = now;
		std::memcpy( record.s.pos.trBase, record.r.currentOrigin, sizeof( record.s.pos.trBase ) );
		std::memcpy( record.s.apos.trBase, record.r.currentAngles, sizeof( record.s.apos.trBase ) );
		std::memset( record.s.pos.trDelta, 0, sizeof( record.s.pos.trDelta ) );
		std::memset( record.s.apos.trDelta, 0, sizeof( record.s.apos.trDelta ) );
		if ( completed ) record.r.mIsRoffing = 0;
	}

	void Apply( const fntech3::roff::Playing &playing, const Sample &sample,
		const fntech3::roff::Cached &path, int now )
	{
		auto *record = host_.Entity( playing.entity );
		if ( !record ) { paths_.Stop( playing.entity ); return; }
		const std::int64_t next = std::int64_t( now ) + FrameMilliseconds( path.path );
		if ( next > INT_MAX || !Finite( record->r.currentOrigin ) || !Finite( record->r.currentAngles ) ) {
			host_.Fail( "Jedi Academy ROFF trajectory time or base is out of range" );
			return;
		}
		float moved[3];
		std::memcpy( moved, sample.move, sizeof( moved ) );
		if ( playing.translate ) {
			float columns[3][3];
			host_.Basis( playing.startAngles, columns );
			for ( int axis = 0; axis < 3; ++axis ) {
				moved[axis] = columns[0][axis] * sample.move[0]
					+ columns[1][axis] * sample.move[1] + columns[2][axis] * sample.move[2];
			}
		}
		float linear[3], angular[3];
		for ( int axis = 0; axis < 3; ++axis ) {
			linear[axis] = moved[axis] * VelocityScale( path.path );
			/* The retail cache repairs one exporter wrap (VA 0x4264b0), rather
			 * than reducing arbitrary angles modulo 360. */
			float turn = sample.turn[axis];
			if ( turn > 180.0f ) turn -= 360.0f;
			else if ( turn < -180.0f ) turn += 360.0f;
			angular[axis] = turn * VelocityScale( path.path );
		}
		if ( !Finite( linear ) || !Finite( angular ) ) {
			host_.Fail( "Jedi Academy ROFF velocity is out of range" );
			return;
		}
		/* Validate and retain the note lines before writing motion. Notes may
		 * reenter the ROFF imports and remove the entity or clear every path. */
		std::vector<std::string> notes;
		for ( int index = 0; index < sample.notes && sample.firstNote >= 0; ++index ) {
			const char *text = fntech3::jka::roff::Note( path.bytes.data(), path.bytes.size(),
				path.path, sample.firstNote + index );
			if ( !text ) { host_.Fail( "Jedi Academy ROFF note is outside its track" ); return; }
			while ( *text ) {
				while ( *text && static_cast<unsigned char>( *text ) < 32 ) ++text;
				const char *begin = text;
				while ( *text && static_cast<unsigned char>( *text ) >= 32 ) ++text;
				const std::size_t length = std::size_t( text - begin );
				if ( length >= 1024 ) { host_.Fail( "Jedi Academy ROFF note line is too long" ); return; }
				if ( length ) notes.emplace_back( begin, length );
			}
		}
		record->s.pos.trType = record->s.apos.trType = Linear;
		record->s.pos.trTime = record->s.apos.trTime = now;
		std::memcpy( record->s.pos.trBase, record->r.currentOrigin, sizeof( record->s.pos.trBase ) );
		std::memcpy( record->s.apos.trBase, record->r.currentAngles, sizeof( record->s.apos.trBase ) );
		std::memcpy( record->s.pos.trDelta, linear, sizeof( linear ) );
		std::memcpy( record->s.apos.trDelta, angular, sizeof( angular ) );
		record->next_roff_time = static_cast<int>( next );
		for ( const std::string &note : notes ) host_.Note( playing.entity, note.c_str() );
	}
};

} // namespace fntech3::jka::roff
#endif
