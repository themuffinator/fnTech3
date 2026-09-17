/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Independently authored from RTCW's public sound contracts. */
#ifndef FNTECH3_AUDIO_RTCW_ENTITY_STREAMS_H
#define FNTECH3_AUDIO_RTCW_ENTITY_STREAMS_H

#include "RTCWMusic.h"
#include "RTCWLoops.h"
#include <array>

namespace fntech3::audio::rtcw {

struct EntityStream {
	std::string name, loop;
	int entity = -1, channel = 0, attenuation = 0, rate = 0;
	bool active = false, stop = false;
	StreamFade fade;
};

// Gains are quantized once per serviced stream and apply to buffered samples.
inline LoopGains EntityStreamGains( const float *origin, const float *listener,
	const float axis[3][3], float volume, float fade, float master, bool mono = false ) {
	volume = (std::min)( PlaybackGain( volume ), 2.0f );
	float left = volume, right = volume;
	if ( origin ) {
		const auto spatial = SpatializeOrigin( origin, 1250.0f, static_cast<int>( volume * 255.0f ), listener, axis, mono );
		left = (std::min)( 1.0f, static_cast<float>( spatial.left / 255.0 ) );
		right = (std::min)( 1.0f, static_cast<float>( spatial.right / 255.0 ) );
	}
	const float scale = PlaybackGain( fade ) * PlaybackGain( master );
	return { static_cast<int>( 256.0f * ( left * scale ) ), static_cast<int>( 256.0f * ( right * scale ) ) };
}

// Slot zero belongs to background music. The original SP pool has eleven
// entity slots; MP has twenty-three (the obsolete classic Mac port had eleven).
// Backend IO owns files and queued PCM. Closing a file ordinarily drains PCM;
// an explicit entity stop or a clear discards it immediately.
class EntityStreams {
public:
	static constexpr int Capacity = 24;
	explicit EntityStreams( bool singlePlayer = true ) : limit_( singlePlayer ? 12 : 24 ) {}
	int Limit() const { return limit_; }
	const EntityStream &operator[]( int index ) const { return streams_[index]; }
	bool Valid( int index ) const { return index > 0 && index < limit_; }

	template<class IO>
	int Start( IO &io, const char *intro, const char *loop, int entity, int channel, int attenuation ) {
		if ( !io.Ready() || entity >= 1024 ) return 0;
		// Snapshot both names before replacing a slot, including aliased Loop().
		std::string requested = MusicPath( intro );
		const std::string repeat = MusicPath( loop, 59 );
		if ( requested.empty() ) requested = MusicPath( loop );
		int index = Matching( entity, channel );
		if ( !index ) {
			for ( int i = 1; i < limit_; ++i ) if ( !streams_[i].active ) { index = i; break; }
		}
		if ( !index ) return 0;
		auto &stream = streams_[index];
		stream.loop = repeat;
		stream.name = requested.substr( 0, 59 );
		const auto slash = stream.name.find_last_of( '/' );
		const auto dot = stream.name.find_last_of( '.' );
		if ( dot == std::string::npos || ( slash != std::string::npos && dot < slash ) ) stream.name += ".wav";
		io.Close( index, false );
		stream.active = false;
		if ( requested.empty() ) return index;
		const int rate = io.Open( index, stream.name.c_str() );
		if ( rate <= 0 || rate > 192000 ) { io.Close( index, false ); return index; }
		stream.entity = entity;
		stream.channel = channel;
		stream.attenuation = attenuation;
		stream.rate = rate;
		stream.stop = false;
		stream.fade = {};
		stream.active = true;
		return index;
	}

	void StopChannel( int entity, int channel ) {
		const int index = Matching( entity, channel );
		if ( index ) streams_[index].stop = true;
	}

	template<class IO>
	void StopEntity( IO &io, int entity ) {
		if ( entity < 0 ) return;
		for ( int i = 1; i < limit_; ++i ) {
			auto &stream = streams_[i];
			if ( stream.active && stream.entity == entity ) {
				stream.stop = true;
				io.Discard( i );
			}
		}
	}

	void Fade( int index, float volume, int milliseconds, SampleTime now ) {
		if ( Valid( index ) && streams_[index].active && !streams_[index].stop )
			streams_[index].fade.Set( volume, milliseconds, now, streams_[index].rate );
	}
	float Gain( int index, SampleTime now ) {
		if ( !Valid( index ) ) return 0;
		auto &stream = streams_[index];
		return stream.fade.Value( now, stream.stop );
	}

	template<class IO>
	bool BeginService( IO &io, int index, float volume ) {
		if ( !Valid( index ) || !io.Ready() ) return false;
		auto &stream = streams_[index];
		if ( stream.active && stream.stop ) {
			io.Close( index, false );
			stream.active = stream.stop = false;
			return false;
		}
		return stream.active && volume > 0;
	}

	void FailedRead( int index ) { if ( Valid( index ) ) streams_[index].stop = true; }

	template<class IO>
	void EndOfTrack( IO &io, int index ) {
		if ( !Valid( index ) || !streams_[index].active ) return;
		auto &stream = streams_[index];
		if ( stream.loop.empty() ) { stream.stop = true; return; }
		// MP treats the loop name as a repeat flag and rewinds the intro.
		// SP entity loops stay on their entity lane. The released original SP
		// source accidentally starts music here; do not let speech replace music.
		const std::string name = limit_ == 12 ? stream.loop : stream.name;
		io.Close( index, false );
		stream.rate = io.Open( index, name.c_str() );
		stream.active = stream.rate > 0 && stream.rate <= 192000;
		if ( !stream.active ) io.Close( index, false );
	}

	template<class IO>
	void Clear( IO &io ) {
		for ( int i = 1; i < limit_; ++i ) {
			io.Close( i, true );
			streams_[i] = {};
		}
	}

private:
	int Matching( int entity, int channel ) const {
		if ( entity < 0 || channel == 0 ) return 0;
		for ( int i = 1; i < limit_; ++i ) {
			const auto &stream = streams_[i];
			if ( stream.active && stream.entity == entity && stream.channel == channel ) return i;
		}
		return 0;
	}
	int limit_;
	std::array<EntityStream, Capacity> streams_{};
};

} // namespace fntech3::audio::rtcw
#endif
