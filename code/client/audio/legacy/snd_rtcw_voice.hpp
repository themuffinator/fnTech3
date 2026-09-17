/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
// Included after the DMA backend's RTCW stream state.

static bool S_Base_RTCWSpeechWindow( const sfx_t *sample, std::int64_t frame, int &amplitude ) {
	using namespace fntech3::audio::rtcw;
	if ( !sample || !sample->soundData || frame < 0 ||
		frame >= std::int64_t{sample->soundLength} - SpeechWindow ||
		(sample->soundChannels != 1 && sample->soundChannels != 2) ) return false;
	const int method = sample->soundCompressionMethod;
	if ( method < 0 || method > 3 || (method && sample->soundChannels != 1) ) return false;
	const int capacity = method == 1 ? SND_CHUNK_SIZE * 4 : method ? SND_CHUNK_SIZE * 2 : SND_CHUNK_SIZE;
	const auto scalar = frame * sample->soundChannels;
	auto *chunk = sample->soundData;
	for ( auto skip = scalar / capacity; skip > 0 && chunk; --skip ) chunk = chunk->next;
	int offset = static_cast<int>(scalar % capacity);
	std::array<short, SND_CHUNK_SIZE * 4> decoded;
	std::array<short, SpeechWindow> window;
	const sndBuffer *decodedChunk = nullptr;
	for ( int i = 0; i < SpeechWindow; ++i ) {
		if ( offset >= capacity ) { chunk = chunk ? chunk->next : nullptr; offset -= capacity; }
		if ( !chunk ) return false;
		if ( method == 2 && (chunk->size <= offset || chunk->size > capacity) ) return false;
		if ( (method == 1 || method == 2) && decodedChunk != chunk ) {
			if ( method == 1 ) S_AdpcmGetSamples(chunk, decoded.data());
			else decodeWavelet(chunk, decoded.data());
			decodedChunk = chunk;
		}
		window[i] = method == 0 ? chunk->sndChunk[offset] : method == 3 ?
			mulawToShort[reinterpret_cast<const unsigned char *>(chunk->sndChunk)[offset]] : decoded[offset];
		offset += sample->soundChannels;
	}
	amplitude = SpeechPCM(window.data(), SpeechWindow);
	return true;
}

static int S_Base_RTCWVoiceAmplitude( int entity ) {
	using namespace fntech3::audio::rtcw;
	if ( !Com_GameIsRTCWSP() || !s_soundStarted || s_soundMuted || entity < 0 ||
		entity >= SpeechClients || dma.speed <= 0 || dma.speed > 192000 ) return 0;
	const int ahead = SpeechLookahead(dma.speed);
	const auto now = static_cast<std::uint32_t>(s_soundtime);
	const auto future = now + ahead;
	int amplitude = 0;
	for ( int slot = 1; slot < rtcwStreams.policy.Limit(); ++slot ) {
		const auto &stream = rtcwStreams.policy[slot];
		const auto &raw = rtcwStreamRaw[slot];
		const int queued = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw.end) - now);
		if ( stream.entity != entity || stream.channel != SpeechChannel || !raw.buffered ||
			queued <= 0 || queued > MAX_RAW_SAMPLES ) continue;
		amplitude = 0;
		const int count = (std::min)(SpeechWindow, queued - ahead);
		for ( int i = 0; i < count; ++i )
			amplitude = (std::max)(amplitude, SpeechStreamSample(raw.samples[(future + i) & (MAX_RAW_SAMPLES - 1)].left));
	}
	for ( const auto &voice : s_channels ) {
		if ( voice.entnum != entity || voice.entchannel != SpeechChannel || !voice.thesfx ||
			voice.startSample == START_SAMPLE_IMMEDIATE || (voice.leftvol <= 0 && voice.rightvol <= 0) ) continue;
		const int elapsed = static_cast<std::int32_t>(now - static_cast<std::uint32_t>(voice.startSample));
		if ( elapsed >= 0 ) S_Base_RTCWSpeechWindow(voice.thesfx, std::int64_t{elapsed} + ahead, amplitude);
	}
	return amplitude;
}
