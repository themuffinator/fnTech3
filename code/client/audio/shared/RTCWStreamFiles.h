/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_RTCW_STREAM_FILES_H
#define FNTECH3_AUDIO_RTCW_STREAM_FILES_H

// Include after snd_codec.h. Both backends use this bounded decoder service;
// their output objects supply readiness, queue capacity and PCM submission.
#include "RTCWEntityStreams.h"
#include <algorithm>

namespace fntech3::audio::rtcw {

class EntityStreamFiles {
public:
	explicit EntityStreamFiles( bool singlePlayer = true ) : policy( singlePlayer ) {}
	EntityStreamFiles( const EntityStreamFiles & ) = delete;
	EntityStreamFiles &operator=( const EntityStreamFiles & ) = delete;
	EntityStreams policy;

	template<class Output>
	int Start( Output &output, const char *intro, const char *loop, int entity, int channel, int attenuation ) {
		IO<Output> io{ *this, output };
		return policy.Start( io, intro, loop, entity, channel, attenuation );
	}
	template<class Output>
	void StopEntity( Output &output, int entity ) {
		IO<Output> io{ *this, output };
		policy.StopEntity( io, entity );
	}
	template<class Output>
	void Clear( Output &output ) {
		IO<Output> io{ *this, output };
		policy.Clear( io );
	}
	void SetProfile( bool singlePlayer ) { policy = EntityStreams( singlePlayer ); }

	template<class Output>
	void Service( Output &output, float volume ) {
		IO<Output> io{ *this, output };
		for ( int index = 1; index < policy.Limit(); ++index ) {
			if ( !policy.BeginService( io, index, volume ) ) continue;
			// Bound both reads and loop restarts per frame, including empty files.
			for ( int chunk = 0; chunk < 2 && streams_[index]; ++chunk ) {
				const auto info = streams_[index]->info;
				const int frameBytes = info.width * info.channels; // Open validated it.
				const int frames = (std::min)( output.FramesNeeded( index, info.rate ), 30000 / frameBytes );
				if ( frames <= 0 ) break;
				std::array<unsigned char, 30000> raw;
				const int count = S_CodecReadStream( streams_[index], frames * frameBytes, raw.data() );
				if ( count < 0 || count > frames * frameBytes || count % frameBytes ) {
					policy.FailedRead( index );
					break;
				}
				if ( !count ) {
					output.Finish( index );
					policy.EndOfTrack( io, index );
					break;
				}
				if ( !output.Submit( index, count / frameBytes, info.rate, info.width, info.channels, raw.data() ) ) {
					policy.FailedRead( index );
					break;
				}
			}
		}
	}

private:
	template<class Output> struct IO {
		EntityStreamFiles &owner;
		Output &output;
		bool Ready() const { return output.Ready(); }
		void Discard( int index ) { output.Clear( index ); }
		void Close( int index, bool discard ) {
			if ( owner.streams_[index] ) S_CodecCloseStream( owner.streams_[index] );
			owner.streams_[index] = nullptr;
			if ( discard ) Discard( index );
		}
		int Open( int index, const char *name ) {
			if ( !output.Prepare( index ) ) return 0;
			auto *stream = S_CodecOpenStream( name );
			if ( !stream ) return 0;
			const auto &info = stream->info;
			if ( info.rate <= 0 || info.rate > 192000 ||
				( info.width != 1 && info.width != 2 ) || ( info.channels != 1 && info.channels != 2 ) ) {
				S_CodecCloseStream( stream );
				return 0;
			}
			owner.streams_[index] = stream;
			return info.rate;
		}
	};
	std::array<snd_stream_t *, EntityStreams::Capacity> streams_{};
};

} // namespace fntech3::audio::rtcw
#endif
