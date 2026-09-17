/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
// Included by snd_dma.cpp after its mixer state declarations.

static fntech3::audio::rtcw::EntityStreamFiles rtcwStreams;
struct RTCWEntityRaw {
	int end = 0;
	bool buffered = false;
	std::array<portable_samplepair_t, MAX_RAW_SAMPLES> samples;
};
static std::array<RTCWEntityRaw, fntech3::audio::rtcw::EntityStreams::Capacity> rtcwStreamRaw;

struct RTCWEntityOutput {
	bool Ready() const { return s_soundStarted && !s_soundMuted; }
	bool Prepare( int ) const { return Ready(); }
	void Clear( int index ) { rtcwStreamRaw[index].end = 0; rtcwStreamRaw[index].buffered = false; }
	void Finish( int ) {}
	int FramesNeeded( int index, int rate ) const {
		const auto &raw = rtcwStreamRaw[index];
		int queued = raw.buffered ? static_cast<int32_t>( static_cast<uint32_t>(raw.end) - static_cast<uint32_t>(s_soundtime) ) : 0;
		if ( queued < 0 || queued > MAX_RAW_SAMPLES ) queued = 0;
		return dma.speed > 0 ? static_cast<int>( static_cast<std::int64_t>(MAX_RAW_SAMPLES - queued) * rate / dma.speed ) : 0;
	}
	bool Submit( int index, int frames, int rate, int width, int channels, const byte *data ) {
		auto &raw = rtcwStreamRaw[index];
		if ( !raw.buffered ) raw.end = s_soundtime;
		S_Base_RawSamplesToBuffer( raw.end, raw.samples.data(), frames, rate, width, channels, data, 1.0f );
		raw.buffered = true;
		return true;
	}
};

static void S_Base_StartRTCWEntityStream( const char *intro, const char *loop, int entity, int channel, int attenuation ) {
	if ( cls.state != CA_ACTIVE ) return;
	RTCWEntityOutput output;
	if ( output.Ready() && !rtcwStreams.Start( output, intro, loop, entity, channel, attenuation ) )
		Com_DPrintf( "RTCW: no free entity streaming tracks\n" );
}

static void S_Base_StopRTCWEntityStream( int entity ) {
	RTCWEntityOutput output;
	rtcwStreams.StopEntity( output, entity );
}

static void S_Base_FadeRTCWEntityStream( float volume, int time, int stream ) {
	rtcwStreams.policy.Fade( stream, volume, time, s_soundtime );
}

static void S_Base_ClearRTCWEntityStreams() {
	RTCWEntityOutput output;
	rtcwStreams.Clear( output );
}

static void S_Base_UpdateRTCWEntityStreams() {
	if ( !Com_GameIsRTCW() ) return;
	RTCWEntityOutput output;
	rtcwStreams.Service( output, s_volume->value );
}

extern "C" void S_PaintRTCWEntityStreams( int first, int end, portable_samplepair_t *paint ) {
	if ( !Com_GameIsRTCW() || !paint ) return;
	const int requested = static_cast<int32_t>( static_cast<uint32_t>(end) - static_cast<uint32_t>(first) );
	if ( requested <= 0 || requested > PAINTBUFFER_SIZE ) return;
	for ( int slot = 1; slot < rtcwStreams.policy.Limit(); ++slot ) {
		const auto &raw = rtcwStreamRaw[slot];
		const int available = static_cast<int32_t>( static_cast<uint32_t>(raw.end) - static_cast<uint32_t>(first) );
		if ( !raw.buffered || available <= 0 || available > MAX_RAW_SAMPLES ) continue;
		const auto &stream = rtcwStreams.policy[slot];
		const float *origin = stream.entity >= 0 && stream.entity < 1024 && stream.attenuation ? rtcwEntityPositions[stream.entity] : nullptr;
		const auto gains = fntech3::audio::rtcw::EntityStreamGains( origin, listener_origin, listener_axis.data(),
			s_volume->value, Com_GameIsRTCWSP() ? rtcwStreams.policy.Gain( slot, s_soundtime ) : 1.0f,
			1.0f, dma.channels == 1 ); // The paint pass applies SP master fade once.
		for ( int i = 0, count = (std::min)( available, requested ); i < count; ++i ) {
			const auto &sample = raw.samples[(static_cast<uint32_t>(first) + i) & (MAX_RAW_SAMPLES - 1)];
			paint[i].left = fntech3::audio::MixPCM16( paint[i].left, sample.left, gains.left );
			paint[i].right = fntech3::audio::MixPCM16( paint[i].right, sample.right, gains.right );
		}
	}
}
