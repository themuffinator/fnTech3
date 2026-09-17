/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
// Included after AudioSystem and its generic PCM conversion helpers.

struct AudioSystem::RTCWEntityOutput {
	AudioSystem &system;
	bool Ready() const { return system.started_ && !system.hardMuted_; }
	bool Prepare( int index ) {
		auto &lane = system.entityStreamLanes_[index];
		if ( lane.sources[0] && lane.sources[1] ) return true;
		// Allocate lazily and preserve a small gameplay source reserve. A pair
		// lets left/right gain change for already-queued PCM without HRTF/pan
		// approximations or prematurely clipping amplified speech to PCM16.
		if ( system.device_.FreeVoiceCount() < 4 ) return false;
		for ( int channel = 0; channel < 2; ++channel ) {
			lane.sources[channel] = system.device_.AcquireVoiceSource();
			if ( !lane.sources[channel] ) { Clear( index ); return false; }
			lane.players[channel].Init( &system.device_, lane.sources[channel] );
			lane.players[channel].deferStart = true;
			lane.players[channel].trackRTCWSpeech = channel == 0 && Com_GameIsRTCWSP();
			system.device_.AL().alSourcef( lane.sources[channel], AL_MAX_GAIN, 512.0f );
		}
		return true;
	}
	void Clear( int index ) {
		auto &lane = system.entityStreamLanes_[index];
		for ( int channel = 0; channel < 2; ++channel ) {
			lane.players[channel].Shutdown();
			if ( lane.sources[channel] ) system.device_.ReleaseVoiceSource( lane.sources[channel] );
			lane.sources[channel] = 0;
		}
	}
	int FramesNeeded( int index, int rate ) const {
		const auto &lane = system.entityStreamLanes_[index];
		if ( !lane.sources[0] || !lane.sources[1] ) return 0;
		// Facial animation reads 250 ms ahead. Eight 50 ms chunks provide that
		// window plus refill headroom, within the existing global buffer budget.
		const int target = Com_GameIsRTCWSP() && system.entityStreams_.policy[index].channel ==
			fntech3::audio::rtcw::SpeechChannel ? 8 : 2;
		if ( lane.players[0].QueuedBufferCount() >= target || lane.players[1].QueuedBufferCount() >= target ) return 0;
		const auto &pool = system.device_.BufferPool();
		if ( pool.FreeCount() + kMaxStreamBuffers - pool.TotalCount() < 2 ) return 0;
		// Roughly 50 ms per chunk, bounded at both source-rate extremes.
		return std::clamp( rate / 20, 128, 9600 );
	}
	void Finish( int index ) {
		for ( auto &player : system.entityStreamLanes_[index].players ) {
			if ( !system.FlushStream( player ) ) system.entityStreams_.policy.FailedRead( index );
		}
	}
	bool Submit( int index, int frames, int rate, int width, int channels, const byte *data ) {
		auto &lane = system.entityStreamLanes_[index];
		std::vector<byte> pcm( static_cast<size_t>( frames ) * 4 );
		for ( int side = 0; side < 2; ++side ) {
			std::fill( pcm.begin(), pcm.end(), 0 );
			for ( int frame = 0; frame < frames; ++frame ) {
				const size_t sample = static_cast<size_t>(frame) * channels + ( channels == 2 ? side : 0 );
				// Codecs return native-endian PCM16; the generic stream converter
				// consumes little-endian bytes. Encode explicitly for big-endian hosts.
				short value;
				if ( width == 2 ) std::memcpy( &value, data + sample * 2, 2 );
				else value = static_cast<short>( ( channels == 1 ? int(data[sample]) - 128 : int(static_cast<signed char>(data[sample])) ) * 256 );
				const auto encoded = static_cast<std::uint16_t>( value );
				const size_t offset = ( static_cast<size_t>(frame) * 2 + side ) * 2;
				pcm[offset] = static_cast<byte>( encoded );
				pcm[offset + 1] = static_cast<byte>( encoded >> 8 );
			}
			if ( !system.QueueStreamChunk( lane.players[side], frames, rate, 2, 2,
				pcm.data(), 1.0f, 2, false ) ) {
				Clear( index ); // Never leave half a stereo pair playing after failure.
				return false;
			}
		}
		return true;
	}
};

void AudioSystem::StartRTCWEntityStream( const char *intro, const char *loop, int entity, int channel, int attenuation ) {
	if ( cls.state != CA_ACTIVE ) return;
	RTCWEntityOutput output{ *this };
	if ( output.Ready() && !entityStreams_.Start( output, intro, loop, entity, channel, attenuation ) )
		Com_DPrintf( "RTCW: no free entity streaming tracks\n" );
}

void AudioSystem::StopRTCWEntityStream( int entity ) {
	RTCWEntityOutput output{ *this };
	entityStreams_.StopEntity( output, entity );
}

void AudioSystem::FadeRTCWEntityStream( float volume, int time, int stream ) {
	entityStreams_.policy.Fade( stream, volume, time, RTCWClock() );
}

void AudioSystem::ClearRTCWEntityStreams() {
	RTCWEntityOutput output{ *this };
	entityStreams_.Clear( output );
}

void AudioSystem::ServiceRTCWEntityStreams() {
	if ( !Com_GameIsRTCW() ) return;
	RTCWEntityOutput output{ *this };
	const float volume = s_volume ? std::clamp( s_volume->value, 0.0f, 2.0f ) : 1.0f;
	const bool sp = Com_GameIsRTCWSP() != 0;
	const auto now = RTCWClock();
	const float master = sp ? fntech3::audio::rtcw::PlaybackGain( S_RTCWMasterFade().current ) : 1.0f;
	const auto update = [&]( int slot, bool play ) {
		auto &lane = entityStreamLanes_[slot];
		const auto gains = world_.EntityStreamGains( entityStreams_.policy[slot], volume,
			sp ? entityStreams_.policy.Gain( slot, now ) : 1.0f, master );
		// MP's OpenAL listener already carries s_volume. Compensate after the
		// original spatial quantization; SP's listener is fixed at unity.
		const float scale = IsSoftMuted() || volume <= 0 ? 0.0f : 1.0f / ( 256.0f * ( sp ? 1.0f : volume ) );
		lane.players[0].Update( gains.left * scale, play );
		lane.players[1].Update( gains.right * scale, play );
	};
	for ( int slot = 1; slot < entityStreams_.policy.Limit(); ++slot ) update( slot, false );
	entityStreams_.Service( output, volume );
	for ( int slot = 1; slot < entityStreams_.policy.Limit(); ++slot ) {
		update( slot, true );
		auto &lane = entityStreamLanes_[slot];
		if ( !entityStreams_.policy[slot].active && lane.players[0].Idle() && lane.players[1].Idle() ) output.Clear( slot );
	}
}
