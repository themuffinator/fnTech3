// Included by AudioSystem.cpp inside its private implementation namespace.
// The soundInterface_t-facing OpenAL backend facade.

class AudioSystem {
public:
	static AudioSystem &Get();

	bool Init( soundInterface_t *si );
	void Shutdown();

	void StartSound( const float *origin, int entnum, int entchannel, sfxHandle_t sfxHandle, float volume );
	void StartRTCWSound( const float *origin, int entity, int channel, sfxHandle_t handle, int flags );
	void StartWolfETSound( const float *origin, int entity, int channel, sfxHandle_t handle, int flags, int volume );
	void FadeWolfETAllSounds( float volume, int time, qboolean stopSounds );
	void UpdateWolfETMasterFade();
	int WolfETSoundLength( sfxHandle_t handle );
	int WolfETCurrentSoundTime();
	void StartRTCWEntityStream( const char *intro, const char *loop, int entity, int channel, int attenuation );
	void StopRTCWEntityStream( int entity );
	void FadeRTCWEntityStream( float volume, int time, int stream );
	int RTCWVoiceAmplitude( int entity );
	int JKAVoiceVolume( int entity ) { return world_.JKAVoiceVolume(entity); }
	void StartLocalSound( sfxHandle_t sfxHandle, int channelNum, float volume );
	void StartBackgroundTrack( const char *intro, const char *loop );
	void StartRTCWBackgroundTrack( const char *intro, const char *loop, int fadeTime );
	void FadeRTCWBackgroundTrack( float volume, int time );
	void FadeRTCWAllSounds( float volume, int time );
	void StopBackgroundTrack();
	void StartStreamSource( snd_stream_t *stream );
	void UpdateBackgroundTrack();
	void RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume );
	void AddVoiceSamples( int clientNum, int samples, int rate, const short *data );
	void StopAllSounds();
	void ClearRTCWSounds( qboolean clearMusic );
	void ClearLoopingSoundsFrame();
	void ClearLoopingSounds( qboolean killall );
	void AddLoopingSound( int entityNum, const float *origin, const float *velocity, sfxHandle_t sfxHandle );
	void AddRTCWLoopingSound( const float *origin, const float *velocity, int range, sfxHandle_t handle, int volume );
	void AddSTVEFAmbientLoopingSound( const float *origin, sfxHandle_t handle, int volume );
	void AddJKAAmbientLoopingSound( const float *origin, sfxHandle_t handle, int volume );
	void StartMOHAASound( const float *origin, int entnum, int entchannel, sfxHandle_t handle,
		float volume, float minDist, float maxDist, float pitch, int mohaaChannel );
	void StopMOHAASound( int entnum, int mohaaChannel );
	void AddMOHAALoopingSound( int slot, const float *origin, const float *velocity, sfxHandle_t handle,
		float volume, float minDist, float maxDist, float pitch );
	const int *STVEFSPMouths();
	void AddWolfETLoopingSound( const float *origin, const float *velocity, int range, sfxHandle_t handle, int volume, int soundTime );
	void AddRealLoopingSound( int entityNum, const float *origin, const float *velocity, sfxHandle_t sfxHandle );
	void StopLoopingSound( int entityNum );
	void Respatialize( int entityNum, const float *origin, float axis[3][3], int inwater );
	void UpdateEntityPosition( int entityNum, const float *origin );
	void Update( int msec );
	void DisableSounds();
	void BeginRegistration();
	sfxHandle_t RegisterSound( const char *sample, qboolean compressed );
	void ClearSoundBuffer();
	void SoundInfo();
	void SoundList();
	void ListDevices() const;
	void ListHrtfs() const;
	void PrintOpenALSoftConfigHints() const;
	bool RecoverDevice( bool force );
	qboolean GetSpatialDebugInfo( spatialAudioDebugInfo_t *info ) const;
	void DumpSpatialDebug() const;

private:
	struct RTCWMusicOutput;
	struct RTCWEntityOutput;

	// American McGee's Alice's voices, handed to the engine in
	// shared/AliceSound.h; they are the world's first 36 one-shot voices.
	class AliceDevice final : public fntech3::audio::alice::sfx::Device {
	public:
		explicit AliceDevice( AudioSystem &system ) : system_( system ) {}
		bool Sample( int sfx, fntech3::audio::alice::sfx::SampleFacts &facts ) const override {
			const SoundSample *sample = system_.GetSample( sfx );
			if ( sample == nullptr ) return false;
			facts.loaded = sample->Loaded() && !sample->DefaultSample();
			facts.frames = sample->Frames();
			facts.rate = sample->Rate();
			facts.durationMs = sample->Rate() > 0 ? static_cast<float>( sample->Frames() ) * 1000.0f / static_cast<float>( sample->Rate() ) : 0.0f;
			facts.name = sample->Name();
			// The buffer keeps the file's own rate, so its frames are the file's.
			facts.sourceFrames = sample->Frames();
			facts.frameBytes = sample->SourceFrameBytes();
			return true;
		}
		bool Playing( int voice ) const override { return system_.world_.AlicePlaying( voice ); }
		bool Prepare( int voice, int sfx ) override {
			SoundSample *sample = system_.GetSample( sfx );
			if ( sfx <= 0 || sample == nullptr || !sample->EnsureLoaded( system_.device_, false ) ) return false;
			return system_.world_.AlicePrepare( voice, sfx, sample );
		}
		void LoopBlock( int voice, int startFrame, int endFrame ) override {
			system_.world_.AliceLoopBlock( voice, startFrame, endFrame );
		}
		void Start( int voice, bool loop, int offsetFrames ) override { system_.world_.AliceStart( voice, loop, offsetFrames ); }
		void Stop( int voice ) override { system_.world_.AliceStop( voice ); }
		void Pause( int voice ) override { system_.world_.AlicePause( voice ); }
		void Resume( int voice ) override { system_.world_.AliceResume( voice ); }
		void Place( int voice, const fntech3::audio::alice::sfx::Placement &placement ) override {
			system_.world_.AlicePlace( voice, placement );
		}
	private:
		AudioSystem &system_;
	};
	bool RefuseAliceStereo( SoundSample &sample );
	AliceDevice aliceDevice_{ *this };
	void ClearRTCWEntityStreams();
	void ServiceRTCWEntityStreams();
	fntech3::audio::rtcw::SampleTime RTCWClock();
	void ClearRTCWMusic();
	bool FlushStream( StreamPlayer &player, bool keepTimeline = false );
	bool QueueStreamChunk( StreamPlayer &player, int samples, int rate, int width, int channels, const byte *data, float volume, int outputChannels, bool logFallback );
	int StreamOutputRate() const;
	void ServiceBackgroundTrack();
	bool UpdateDeviceState( int msec );
	void CloseBackgroundStream();
	SoundSample *GetSample( sfxHandle_t handle );
	qboolean IsSoftMuted() const;

	OpenALDevice device_;
	Q3SoundWorld world_;
	StreamPlayer musicPlayer_;
	StreamPlayer rawPlayer_;
	VoiceChatPlayer voicePlayer_;
	struct EntityStreamLane {
		std::array<StreamPlayer, 2> players;
		std::array<ALuint, 2> sources{};
	};
	std::array<EntityStreamLane, fntech3::audio::rtcw::EntityStreams::Capacity> entityStreamLanes_{};
	fntech3::audio::rtcw::EntityStreamFiles entityStreams_;
	std::deque<SoundSample> samples_;
	std::unordered_map<std::string, sfxHandle_t> sampleLookup_;
	SoundShaderLibrary weaponSoundShaders_;
	snd_stream_t *backgroundStream_ = nullptr;
	std::vector<byte> backgroundPendingPCM_;
	std::string backgroundIntro_;
	std::string backgroundLoop_;
	int backgroundStereoFallbackChannels_ = 0;
	adr::DeviceRecoveryState deviceRecovery_;
	fntech3::audio::rtcw::SampleClock rtcwClock_;
	bool started_ = false;
	bool hardMuted_ = true;
	// Set when an ET fade was told to stop what it faded once it completes.
	bool wolfetStopAfterFade_ = false;
};

int AudioSystem::RTCWVoiceAmplitude( int entity ) {
	using namespace fntech3::audio::rtcw;
	if ( !started_ || hardMuted_ || !Com_GameIsRTCWSP() || entity < 0 || entity >= SpeechClients ) return 0;
	int amplitude = 0;
	for ( int slot = 1; slot < entityStreams_.policy.Limit(); ++slot ) {
		const auto &stream = entityStreams_.policy[slot];
		auto &player = entityStreamLanes_[slot].players[0];
		if ( stream.entity == entity && stream.channel == SpeechChannel && !player.Idle() )
			amplitude = player.RTCWSpeechAmplitude();
	}
	return world_.RTCWVoiceAmplitude(entity, amplitude);
}

AudioSystem &AudioSystem::Get() {
	static AudioSystem audioSystem;
	return audioSystem;
}

static int SelectStreamOutputChannels( const OpenALDevice &device, int channels ) {
	if ( !PCMChannelCountCanBeRepresented( channels ) ) {
		return 0;
	}
	if ( channels > kDefaultStreamChannels && device.SupportsPCMChannels( channels ) ) {
		return channels;
	}
	return kDefaultStreamChannels;
}

static float ReadStreamPCM16Sample( const byte *data, size_t sampleIndex, int width, int channels ) {
	if ( data == nullptr ) {
		return 0.0f;
	}
	if ( width == 2 ) {
		return static_cast<float>( ReadNativePCM16( data + sampleIndex * sizeof( short ) ) );
	}
	if ( channels == 1 ) {
		return static_cast<float>( ConvertUnsignedPCM8ToPCM16( data[sampleIndex] ) );
	}
	return static_cast<float>( ConvertSignedPCM8ToPCM16( data[sampleIndex] ) );
}

static void WriteConvertedStreamFrame( const float *input, int inputChannels, short *output, int outputChannels, float gain ) {
	if ( input == nullptr || output == nullptr || inputChannels <= 0 || outputChannels <= 0 ) {
		return;
	}

	if ( outputChannels == inputChannels ) {
		for ( int channel = 0; channel < outputChannels; ++channel ) {
			output[channel] = ClampPCM16FromFloat( input[channel] * gain );
		}
		return;
	}

	if ( outputChannels == kDefaultStreamChannels ) {
		float left = 0.0f;
		float right = 0.0f;
		DownmixFrameToStereoFloat( input, inputChannels, gain, left, right );
		output[0] = ClampPCM16FromFloat( left );
		output[1] = ClampPCM16FromFloat( right );
		return;
	}

	for ( int channel = 0; channel < outputChannels; ++channel ) {
		output[channel] = 0;
	}
	if ( inputChannels == 1 ) {
		output[0] = ClampPCM16FromFloat( input[0] * gain );
		if ( outputChannels > 1 ) {
			output[1] = output[0];
		}
		return;
	}

	const int copiedChannels = ( std::min )( inputChannels, outputChannels );
	for ( int channel = 0; channel < copiedChannels; ++channel ) {
		output[channel] = ClampPCM16FromFloat( input[channel] * gain );
	}
}

int AudioSystem::StreamOutputRate() const {
	const int mixerRate = device_.Capabilities().mixerFrequency;
	if ( mixerRate >= 8000 && mixerRate <= 192000 ) {
		return mixerRate;
	}

	return ClampInt( CvarIntegerOrDefault( s_alFrequency, kFallbackStreamRate ), 8000, 192000 );
}

static std::vector<short> ConvertResampledFrames( const std::vector<float> &frames, const StreamConversionState &state, int outputChannels ) {
	const size_t count = frames.size() / static_cast<size_t>( state.inputChannels );
	if ( count > static_cast<size_t>( std::numeric_limits<int>::max() ) / outputChannels ||
		!PCMByteCountFitsALsizei( count * static_cast<size_t>( outputChannels ) ) ) {
		return {};
	}
	std::vector<short> pcm( count * static_cast<size_t>( outputChannels ) );
	for ( size_t i = 0; i < count; ++i ) {
		WriteConvertedStreamFrame( frames.data() + i * state.inputChannels, state.inputChannels,
			pcm.data() + i * outputChannels, outputChannels, 1.0f );
	}
	return pcm;
}

bool AudioSystem::FlushStream( StreamPlayer &player, bool keepTimeline ) {
	if ( player.conversion.rate == 0 ) return true;
	if ( keepTimeline && !player.conversion.resampler.HasPendingOutput() ) return true;
	auto next = player.conversion;
	std::vector<float> frames;
	if ( !( keepTimeline ? next.resampler.Drain( frames ) : next.resampler.Finish( frames ) ) ) return false;
	if ( !frames.empty() ) {
		const auto pcm = ConvertResampledFrames( frames, next, next.outputChannels );
		if ( pcm.empty() || !player.QueuePCM16( pcm.data(), static_cast<int>( pcm.size() / next.outputChannels ),
			next.outputChannels, next.outputRate ) ) return false;
	}
	player.conversion = keepTimeline ? std::move( next ) : StreamConversionState{};
	return true;
}

bool AudioSystem::QueueStreamChunk( StreamPlayer &player, int samples, int rate, int width, int channels, const byte *data, float volume, int outputChannels, bool logFallback ) {
	if ( samples <= 0 || rate <= 0 || data == nullptr || ( width != 1 && width != 2 ) ||
		!PCMChannelCountCanBeRepresented( channels ) || !PCMChannelCountCanBeRepresented( outputChannels ) ||
		!fntech3::audio::CubicStreamResampler::ChunkSizeFits( samples, channels ) ) return false;
	size_t sourceBytes = 0;
	if ( !PCMFrameByteCount( samples, channels, width, sourceBytes ) ||
		static_cast<size_t>( samples ) > std::numeric_limits<size_t>::max() / sizeof( float ) / channels ) return false;
	(void)sourceBytes;
	const int outputRate = StreamOutputRate();
	const float gain = ClampFloat( volume, 0.0f, 8.0f );
	const auto &old = player.conversion;
	if ( old.rate && old.outputRate != outputRate ) {
		// A changed device clock cannot share an OpenAL buffer queue with
		// its previous sample rate. Recovery starts a fresh conversion timeline.
		player.Clear();
	} else if ( old.rate && ( old.rate != rate || old.width != width || old.inputChannels != channels ||
		old.requestedChannels != outputChannels ) && !FlushStream( player ) ) return false;
	auto next = player.conversion;
	if ( !next.rate ) {
		if ( !next.resampler.Configure( rate, outputRate, channels ) ) return false;
		next.rate = rate; next.width = width; next.inputChannels = channels;
		next.outputRate = outputRate; next.requestedChannels = outputChannels;
		next.outputChannels = outputChannels;
	}
	std::vector<float> input( static_cast<size_t>( samples ) * channels );
	// Gain belongs to these input frames. Scaling before interpolation keeps
	// fades continuous without flushing/restarting the sample-rate phase on
	// every volume change, which would add rounded-up frames and audible steps.
	for ( size_t i = 0; i < input.size(); ++i ) input[i] = ReadStreamPCM16Sample( data, i, width, channels ) * gain;
	std::vector<float> frames;
	if ( !next.resampler.Push( input.data(), samples, frames ) ) return false;
	if ( frames.empty() ) {
		player.conversion = std::move( next );
		return true;
	}
	auto pcm = ConvertResampledFrames( frames, next, next.outputChannels );
	if ( pcm.empty() ) return false;
	if ( player.QueuePCM16( pcm.data(), static_cast<int>( pcm.size() / next.outputChannels ), next.outputChannels, outputRate ) ) {
		player.conversion = std::move( next );
		return true;
	}
	// A queue failure can mean temporary pool exhaustion or an incompatible
	// format while the preceding segment drains. Preserve its buffered audio;
	// stereo fallback is safe only before any buffers have been accepted.
	if ( next.outputChannels <= kDefaultStreamChannels || player.QueuedBufferCount() > 0 ) return false;
	// An empty AL queue can still have accepted samples in the resampler's
	// lookahead. Restore that history if the stereo upload also fails.
	auto previous = std::move( player.conversion );
	player.Clear();
	pcm = ConvertResampledFrames( frames, next, kDefaultStreamChannels );
	if ( pcm.empty() || !player.QueuePCM16( pcm.data(), static_cast<int>( pcm.size() / kDefaultStreamChannels ),
		kDefaultStreamChannels, outputRate ) ) {
		player.conversion = std::move( previous );
		return false;
	}
	next.outputChannels = kDefaultStreamChannels;
	player.conversion = std::move( next );
	if ( logFallback && backgroundStereoFallbackChannels_ != outputChannels ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: OpenAL rejected %s stream buffers; using stereo downmix fallback\n", PCMChannelLayoutName( outputChannels ) );
		backgroundStereoFallbackChannels_ = outputChannels;
	}
	return true;
}

bool AudioSystem::Init( soundInterface_t *si ) {
	if ( si == nullptr ) {
		return false;
	}
	*si = {};

	s_alReverb = Cvar_Get( "s_alReverb", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_alReverb, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alReverb, "Enables OpenAL environmental reverb sends when the active device supports EFX. Requires snd_restart to fully apply." );
	s_alOcclusion = Cvar_Get( "s_alOcclusion", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alOcclusion, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alOcclusion, "Enables smoothed world-geometry occlusion checks for the OpenAL backend." );
	s_alReverbGain = Cvar_Get( "s_alReverbGain", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alReverbGain, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_alReverbGain, "Scales wet reverb send level for the OpenAL backend." );
	s_alOcclusionStrength = Cvar_Get( "s_alOcclusionStrength", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alOcclusionStrength, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_alOcclusionStrength, "Scales how strongly world occlusion attenuates and muffles OpenAL sounds before smoothing." );
	s_alDopplerFactor = Cvar_Get( "s_alDopplerFactor", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alDopplerFactor, "0", "10", CV_FLOAT );
	Cvar_SetDescription( s_alDopplerFactor, "Scales the OpenAL doppler effect from source and listener motion. Applies while s_doppler is enabled." );
	s_alDopplerSpeed = Cvar_Get( "s_alDopplerSpeed", "6000", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alDopplerSpeed, "1000", "20000", CV_FLOAT );
	Cvar_SetDescription( s_alDopplerSpeed, "Speed of sound in world units per second for OpenAL doppler.\n"
		"Lower values exaggerate pitch shifts; about 13500 matches real-world acoustics at Quake III scale." );
	s_alAirAbsorption = Cvar_Get( "s_alAirAbsorption", "2.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alAirAbsorption, "0", "10", CV_FLOAT );
	Cvar_SetDescription( s_alAirAbsorption, "Scales distance-based high-frequency air absorption for positional sounds when EFX is available.\n"
		"0 disables it, 1 is physically neutral, higher values darken distant sounds more strongly." );
	s_alDebugOverlay = Cvar_Get( "s_alDebugOverlay", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alDebugOverlay, "0", "2", CV_INTEGER );
	Cvar_SetDescription( s_alDebugOverlay, "Draws OpenAL spatial audio debug overlay.\n"
		"0 disables the overlay.\n"
		"1 shows summary environment and selected voice state.\n"
		"2 adds environment, sample, gain, and tone-filter details for the selected voice." );
	s_alDebugVoice = Cvar_Get( "s_alDebugVoice", "-1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( s_alDebugVoice, "Selects which entity the OpenAL spatial audio debug tools inspect.\n"
		"Use -1 to automatically pick the nearest active voice." );
	s_alSourceClassDebug = Cvar_Get( "s_alSourceClassDebug", "0", CVAR_DEVELOPER );
	Cvar_CheckRange( s_alSourceClassDebug, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alSourceClassDebug, "Adds per-source-class aggregate routing and gain summaries to s_alDebugDump." );
	s_alSoundStartDebug = Cvar_Get( "s_alSoundStartDebug", "0", CVAR_DEVELOPER );
	Cvar_CheckRange( s_alSoundStartDebug, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alSoundStartDebug, "Logs Wolfenstein sound starts, rejected requests and OpenAL playback state." );
	s_alAudioZones = Cvar_Get( "s_alAudioZones", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alAudioZones, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alAudioZones, "Enables optional maps/<map>.azb OpenAL audio-zone sidecars when present." );
	s_alAutoRecover = Cvar_Get( "s_alAutoRecover", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_alAutoRecover, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_alAutoRecover, "Automatically tries to reopen the OpenAL device after the runtime reports that it disconnected." );

	if ( !device_.Init() ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: OpenAL device initialization did not complete\n" );
		return false;
	}
	if ( s_alSpatializeStereo != nullptr && s_alSpatializeStereo->integer && !device_.Capabilities().sourceSpatialize ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: s_alSpatializeStereo requested, but AL_SOFT_source_spatialize is unavailable; stereo world samples will stay direct\n" );
	}

	world_.Reset( &device_ );
	musicPlayer_.Init( &device_, device_.MusicSource() );
	rawPlayer_.Init( &device_, device_.RawSource() );
	voicePlayer_.Init( &device_ );
	entityStreams_.SetProfile( Com_GameIsRTCWSP() != 0 );

	samples_.clear();
	sampleLookup_.clear();
	weaponSoundShaders_.Clear();
	started_ = true;
	hardMuted_ = true;
	deviceRecovery_ = {};
	rtcwClock_ = {};
	if ( Com_GameIsRTCWSP() ) RTCWClock();

	si->Shutdown = []() { AudioSystem::Get().Shutdown(); };
	si->StartSound = []( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfxHandle, float volume ) { AudioSystem::Get().StartSound( origin, entnum, entchannel, sfxHandle, volume ); };
	si->StartRTCWSound = []( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags ) { AudioSystem::Get().StartRTCWSound(origin, entity, channel, handle, flags); };
	si->StartWolfETSound = []( const vec3_t origin, int entity, int channel, sfxHandle_t handle, int flags, int volume ) { AudioSystem::Get().StartWolfETSound(origin, entity, channel, handle, flags, volume); };
	si->StartWolfETLocalSound = []( sfxHandle_t handle, int channel, int volume ) {
		auto &audio = AudioSystem::Get();
		audio.StartWolfETSound(nullptr, audio.world_.ListenerNumber(), channel, handle, 0, volume);
	};
	si->AddWolfETLoopingSound = []( const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t handle, int volume, int soundTime ) { AudioSystem::Get().AddWolfETLoopingSound( origin, velocity, range, handle, volume, soundTime ); };
	si->FadeWolfETAllSounds = []( float volume, int time, qboolean stopSounds ) { AudioSystem::Get().FadeWolfETAllSounds( volume, time, stopSounds ); };
	si->WolfETSoundLength = []( sfxHandle_t handle ) { return AudioSystem::Get().WolfETSoundLength( handle ); };
	si->WolfETCurrentSoundTime = []() { return AudioSystem::Get().WolfETCurrentSoundTime(); };
	si->StartRTCWLocalSound = []( sfxHandle_t handle, int channel ) {
		auto &audio = AudioSystem::Get();
		audio.StartRTCWSound(nullptr, audio.world_.ListenerNumber(), channel, handle, 0);
	};
	si->StartRTCWEntityStream = []( const char *intro, const char *loop, int entity, int channel, int attenuation ) { AudioSystem::Get().StartRTCWEntityStream( intro, loop, entity, channel, attenuation ); };
	si->StopRTCWEntityStream = []( int entity ) { AudioSystem::Get().StopRTCWEntityStream( entity ); };
	si->FadeRTCWEntityStream = []( float volume, int time, int stream ) { AudioSystem::Get().FadeRTCWEntityStream( volume, time, stream ); };
	si->RTCWVoiceAmplitude = []( int entity ) { return AudioSystem::Get().RTCWVoiceAmplitude( entity ); };
	si->JKAVoiceVolume = []( int entity ) { return AudioSystem::Get().JKAVoiceVolume( entity ); };
	si->StartLocalSound = []( sfxHandle_t sfxHandle, int channelNum, float volume ) { AudioSystem::Get().StartLocalSound( sfxHandle, channelNum, volume ); };
	si->MarkInterfaceSound = []( qboolean interfaceSound ) { AudioSystem::Get().world_.MarkNextInterfaceSound( interfaceSound != qfalse ); };
	si->StartBackgroundTrack = []( const char *intro, const char *loop ) { AudioSystem::Get().StartBackgroundTrack( intro, loop ); };
	si->StartRTCWBackgroundTrack = []( const char *intro, const char *loop, int time ) { AudioSystem::Get().StartRTCWBackgroundTrack( intro, loop, time ); };
	si->FadeRTCWBackgroundTrack = []( float volume, int time ) { AudioSystem::Get().FadeRTCWBackgroundTrack( volume, time ); };
	si->FadeRTCWAllSounds = []( float volume, int time ) { AudioSystem::Get().FadeRTCWAllSounds( volume, time ); };
	si->StopBackgroundTrack = []() { AudioSystem::Get().StopBackgroundTrack(); };
	si->StartStreamSource = []( snd_stream_t *stream ) { AudioSystem::Get().StartStreamSource( stream ); };
	si->UpdateBackgroundTrack = []() { AudioSystem::Get().UpdateBackgroundTrack(); };
	si->RawSamples = []( int samples, int rate, int width, int channels, const byte *data, float volume ) { AudioSystem::Get().RawSamples( samples, rate, width, channels, data, volume ); };
	si->AddVoiceSamples = []( int clientNum, int samples, int rate, const short *data ) { AudioSystem::Get().AddVoiceSamples( clientNum, samples, rate, data ); };
	si->StopAllSounds = []() { AudioSystem::Get().StopAllSounds(); };
	si->ClearRTCWSounds = []( qboolean clearMusic ) { AudioSystem::Get().ClearRTCWSounds( clearMusic ); };
	si->ClearLoopingSoundsFrame = []() { AudioSystem::Get().ClearLoopingSoundsFrame(); };
	si->ClearLoopingSounds = []( qboolean killall ) { AudioSystem::Get().ClearLoopingSounds( killall ); };
	si->AddLoopingSound = []( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) { AudioSystem::Get().AddLoopingSound( entityNum, origin, velocity, sfxHandle ); };
	si->AddRTCWLoopingSound = []( const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t handle, int volume ) { AudioSystem::Get().AddRTCWLoopingSound( origin, velocity, range, handle, volume ); };
	si->AddSTVEFAmbientLoopingSound = []( const vec3_t origin, sfxHandle_t handle, int volume ) { AudioSystem::Get().AddSTVEFAmbientLoopingSound( origin, handle, volume ); };
	si->AddJKAAmbientLoopingSound = []( const vec3_t origin, sfxHandle_t handle, int volume ) { AudioSystem::Get().AddJKAAmbientLoopingSound( origin, handle, volume ); };
	si->StartMOHAASound = []( const vec3_t origin, int entnum, int entchannel, sfxHandle_t handle,
		float volume, float minDist, float maxDist, float pitch, int mohaaChannel ) {
		AudioSystem::Get().StartMOHAASound( origin, entnum, entchannel, handle, volume, minDist, maxDist, pitch,
			mohaaChannel );
	};
	si->StopMOHAASound = []( int entnum, int mohaaChannel ) { AudioSystem::Get().StopMOHAASound( entnum, mohaaChannel ); };
	si->AddMOHAALoopingSound = []( int slot, const vec3_t origin, const vec3_t velocity, sfxHandle_t handle,
		float volume, float minDist, float maxDist, float pitch ) {
		AudioSystem::Get().AddMOHAALoopingSound( slot, origin, velocity, handle, volume, minDist, maxDist, pitch );
	};
	si->STVEFSPMouths = []() { return AudioSystem::Get().STVEFSPMouths(); };
	si->AddRealLoopingSound = []( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) { AudioSystem::Get().AddRealLoopingSound( entityNum, origin, velocity, sfxHandle ); };
	si->StopLoopingSound = []( int entityNum ) { AudioSystem::Get().StopLoopingSound( entityNum ); };
	si->Respatialize = []( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) { AudioSystem::Get().Respatialize( entityNum, origin, axis, inwater ); };
	si->UpdateEntityPosition = []( int entityNum, const vec3_t origin ) { AudioSystem::Get().UpdateEntityPosition( entityNum, origin ); };
	si->Update = []( int msec ) { AudioSystem::Get().Update( msec ); };
	si->DisableSounds = []() { AudioSystem::Get().DisableSounds(); };
	si->BeginRegistration = []() { AudioSystem::Get().BeginRegistration(); };
	si->RegisterSound = []( const char *sample, qboolean compressed ) { return AudioSystem::Get().RegisterSound( sample, compressed ); };
	si->ClearSoundBuffer = []() { AudioSystem::Get().ClearSoundBuffer(); };
	si->SoundInfo = []() { AudioSystem::Get().SoundInfo(); };
	si->SoundList = []() { AudioSystem::Get().SoundList(); };
	si->GetSpatialDebugInfo = []( spatialAudioDebugInfo_t *info ) { return AudioSystem::Get().GetSpatialDebugInfo( info ); };
	si->DumpSpatialDebug = []() { AudioSystem::Get().DumpSpatialDebug(); };
	si->AliceDevice = &aliceDevice_;

	return true;
}

#include "AudioSystemRTCWStreams.inl"

void AudioSystem::Shutdown() {
	if ( !started_ ) {
		return;
	}

	StopAllSounds();
	CloseBackgroundStream();
	for ( SoundSample &sample : samples_ ) {
		sample.Unload( device_ );
	}
	samples_.clear();
	sampleLookup_.clear();
	weaponSoundShaders_.Clear();
	voicePlayer_.Shutdown();
	musicPlayer_.Shutdown();
	rawPlayer_.Shutdown();
	device_.Shutdown();
	started_ = false;
	hardMuted_ = true;
	deviceRecovery_ = {};
	cls.soundRegistered = qfalse;
}

qboolean AudioSystem::IsSoftMuted() const {
	return ( ( !gw_active && !gw_minimized && s_muteWhenUnfocused != nullptr && s_muteWhenUnfocused->integer ) ||
		( gw_minimized && s_muteWhenMinimized != nullptr && s_muteWhenMinimized->integer ) ) ? qtrue : qfalse;
}

bool AudioSystem::UpdateDeviceState( int msec ) {
	if ( !started_ || !device_.Ready() ) {
		return false;
	}

	const adr::DevicePollDecision poll = adr::AdvanceDevicePoll( deviceRecovery_, msec, device_.DeviceConnected(), kOpenALDeviceStatePollMs );
	if ( !poll.shouldPoll ) {
		return poll.connected;
	}

	const bool refreshSucceeded = device_.RefreshDeviceConnection();
	const adr::DevicePollResult result = adr::FinishDevicePoll(
		deviceRecovery_,
		refreshSucceeded,
		device_.DeviceConnected(),
		s_alAutoRecover != nullptr && s_alAutoRecover->integer != 0,
		kOpenALDeviceRecoveryRetryMs );
	if ( result.printReconnected ) {
		Com_Printf( "OpenAL playback device reports connected again\n" );
	}
	if ( result.printDisconnectedWarning ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: OpenAL playback device disconnected; use s_alRecoverDevice or snd_restart if audio does not return\n" );
	}
	if ( result.shouldAttemptRecovery ) {
		const bool recovered = device_.RecoverDevice( false );
		adr::FinishRecoveryAttempt( deviceRecovery_, recovered );
		if ( recovered ) {
			return true;
		}
	}
	return result.connected;
}

SoundSample *AudioSystem::GetSample( sfxHandle_t handle ) {
	if ( handle < 0 || handle >= static_cast<sfxHandle_t>( samples_.size() ) ) {
		return nullptr;
	}
	return &samples_[static_cast<size_t>( handle )];
}

void AudioSystem::BeginRegistration() {
	hardMuted_ = false;

	if ( !samples_.empty() ) {
		return;
	}

	weaponSoundShaders_.EnsureLoaded();
	if ( Com_GameIsRitual() || Com_GameIsMOHAA() ) {
		// American McGee's Alice's handle 0 is ***DEFAULT***, registered at
		// S_Init and never loaded (alice.exe 0x0044c893); Medal of Honor's
		// engine reserves the same name (OpenMoHAA snd_dma_new.cpp S_Init).
		const std::string placeholder = "***DEFAULT***";
		samples_.emplace_back( placeholder, weaponSoundShaders_.Find( placeholder ) );
		return;
	}
	// Elite Force reserves its silent sound/null.wav as handle 0 (stvoyHM.exe 0x42a296).
	const std::string feedbackSample = Com_GameIsRTCW() || Com_GameIsWolfET() ? fntech3::audio::rtcw::DefaultSoundName
		: NormalizeSoundName( Com_GameIsSTVEF() ? "sound/null.wav" : "sound/feedback/hit.wav" );
	samples_.emplace_back( feedbackSample, weaponSoundShaders_.Find( feedbackSample ) );
	sampleLookup_.emplace( feedbackSample, 0 );
	samples_[0].EnsureLoaded( device_, true );
}

sfxHandle_t AudioSystem::RegisterSound( const char *sample, qboolean /*compressed*/ ) {
	if ( !started_ || sample == nullptr || sample[0] == '\0' ) {
		return 0;
	}

	if ( std::strlen( sample ) >= MAX_QPATH ) {
		Com_Printf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}
	if ( sample[0] == '*' ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Tried to load player sound directly: %s\n", sample );
		return 0;
	}

	if ( samples_.empty() ) {
		BeginRegistration();
	}

	const std::string normalized = NormalizeSoundName( sample );
	const auto existing = sampleLookup_.find( normalized );
	if ( existing != sampleLookup_.end() ) {
		SoundSample &found = samples_[static_cast<size_t>( existing->second )];
		if ( found.EnsureLoaded( device_, existing->second == 0 ) && !RefuseAliceStereo( found ) ) {
			return existing->second;
		}
		return 0;
	}

	if ( samples_.size() >= kMaxRegisteredSamples ) {
		Com_Error( ERR_FATAL, "S_FindName: out of sfx_t" );
	}
	const sfxHandle_t handle = static_cast<sfxHandle_t>( samples_.size() );
	weaponSoundShaders_.EnsureLoaded();
	samples_.emplace_back( normalized, weaponSoundShaders_.Find( normalized ) );
	sampleLookup_.emplace( normalized, handle );
	if ( !samples_.back().EnsureLoaded( device_, false ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: couldn't load sound: %s\n", sample );
		return 0;
	}
	if ( RefuseAliceStereo( samples_.back() ) ) {
		return 0;
	}

	return handle;
}

// American McGee's Alice refuses a stereo WAV outright, and tries it again at
// the next registration (alice.exe 0x0044e560).
bool AudioSystem::RefuseAliceStereo( SoundSample &sample ) {
	if ( !Com_GameIsRitual() || sample.Channels() != 2 || sample.Name().find( ".mp3" ) != std::string::npos ) {
		return false;
	}
	Com_Printf( "%s is a stereo wav file\n", sample.Name().c_str() );
	sample.Unload( device_ );
	return true;
}

void AudioSystem::StartSound( const float *origin, int entnum, int entchannel, sfxHandle_t sfxHandle, float volume ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}
	if ( origin == nullptr && ( entnum < 0 || entnum >= MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entnum );
	}

	SoundSample *sample = GetSample( sfxHandle );
	if ( sample == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}
	if ( !sample->EnsureLoaded( device_, sfxHandle == 0 ) ) {
		return;
	}

	world_.StartSound( entnum, entchannel, sfxHandle, sample, origin, volume );
}

void AudioSystem::StartWolfETSound( const float *origin, int entity, int channel,
	sfxHandle_t handle, int flags, int volume ) {
	if ( !started_ || hardMuted_ ) return;
	if ( !origin && (entity < 0 || entity >= MAX_GENTITIES) )
		Com_Error(ERR_DROP, "S_StartWolfETSound: bad entitynum %i", entity);
	auto *sample = GetSample(handle);
	if ( !sample ) {
		Com_Printf(S_COLOR_YELLOW "S_StartWolfETSound: handle %i out of range\n", handle);
		return;
	}
	entityStreams_.policy.StopChannel( entity, channel );
	if ( sample->EnsureLoaded(device_, handle == 0) )
		world_.StartWolfETSound(entity, channel, handle, sample, origin, flags, volume);
}

void AudioSystem::StartRTCWSound( const float *origin, int entity, int channel, sfxHandle_t handle, int flags ) {
	const bool trace = s_alSoundStartDebug && s_alSoundStartDebug->integer;
	if ( !started_ || hardMuted_ ) {
		if ( trace ) Com_Printf("sound-start: ent=%d channel=%d handle=%d rejected=backend-muted\n", entity, channel, handle);
		return;
	}
	// SP emits null effects that must not occupy a channel. Null weapon
	// requests remain meaningful: their cutoff flags stop a dead actor's fire.
	if ( Com_GameIsRTCWSP() && handle == 0 && channel != fntech3::audio::rtcw::OneShots::Weapon ) return;
	if ( !origin && (entity < 0 || entity >= MAX_GENTITIES) )
		Com_Error(ERR_DROP, "S_StartRTCWSound: bad entitynum %i", entity);
	auto *sample = GetSample(handle);
	if ( !sample ) {
		Com_Printf(S_COLOR_YELLOW "S_StartRTCWSound: handle %i out of range\n", handle);
		return;
	}
	entityStreams_.policy.StopChannel( entity, channel );
	if ( sample->EnsureLoaded(device_, handle == 0) )
		world_.StartRTCWSound(entity, channel, handle, sample, origin, flags);
	else if ( trace ) Com_Printf("sound-start: ent=%d channel=%d handle=%d sample=%s rejected=sample-load\n",
		entity, channel, handle, sample->Name().c_str());
}

void AudioSystem::StartLocalSound( sfxHandle_t sfxHandle, int channelNum, float volume ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}
	if ( GetSample( sfxHandle ) == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}
	StartSound( nullptr, world_.ListenerNumber(), channelNum, sfxHandle, volume );
}

void AudioSystem::CloseBackgroundStream() {
	backgroundPendingPCM_.clear();
	if ( backgroundStream_ != nullptr ) {
		S_CodecCloseStream( backgroundStream_ );
		backgroundStream_ = nullptr;
	}
	backgroundStereoFallbackChannels_ = 0;
}

struct AudioSystem::RTCWMusicOutput {
	AudioSystem &system;
	bool Ready() const { return system.started_; }
	void Close( bool clearBuffered ) {
		system.CloseBackgroundStream();
		if ( clearBuffered ) system.musicPlayer_.Clear();
	}
	int Open( const char *name ) {
		system.backgroundStream_ = S_CodecOpenStream( name );
		if ( !system.backgroundStream_ ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", name );
			return 0;
		}
		return system.backgroundStream_->info.rate;
	}
	fntech3::audio::rtcw::SampleTime Clock() { return system.RTCWClock(); }
	void CurrentMusic( const char *name ) { Cvar_Set( "s_currentMusic", name ); }
};

fntech3::audio::rtcw::SampleTime AudioSystem::RTCWClock() {
	const bool deviceClock = device_.RefreshTimingDiagnostics();
	const auto micros = deviceClock ? device_.Capabilities().deviceClockValue / 1000 : Sys_Microseconds();
	return rtcwClock_.Advance( micros, deviceClock, StreamOutputRate() );
}

void AudioSystem::StartRTCWBackgroundTrack( const char *intro, const char *loop, int fadeTime ) {
	RTCWMusicOutput output{ *this };
	S_RTCWMusic().Start( output, intro, loop, fadeTime );
	if ( fadeTime >= 0 && started_ ) {
		const float volume = s_musicVolume != nullptr ? s_musicVolume->value : 1.0f;
		musicPlayer_.Update( IsSoftMuted() ? 0.0f : volume *
			ClampFloat( S_RTCWMusic().Gain( RTCWClock() ), 0.0f, 16.0f ) *
			fntech3::audio::rtcw::PlaybackGain( S_RTCWMasterFade().current ), false );
	}
}

void AudioSystem::FadeRTCWBackgroundTrack( float volume, int time ) {
	S_RTCWMusic().Fade( volume, time, RTCWClock() );
}

void AudioSystem::FadeRTCWAllSounds( float volume, int time ) {
	S_RTCWMasterFade().Set( volume, time, RTCWClock(), StreamOutputRate() );
}

// ET's fade can be told to stop what it faded once the fade completes, and a
// second stopping fade begun before the first finished stops everything at once.
void AudioSystem::FadeWolfETAllSounds( float volume, int time, qboolean stopSounds ) {
	auto &fade = S_WolfETMasterFade();
	if ( wolfetStopAfterFade_ && RTCWClock() < fade.end ) StopAllSounds();
	fade.Set( volume, time, RTCWClock(), StreamOutputRate() );
	wolfetStopAfterFade_ = stopSounds ? true : false;
}

void AudioSystem::UpdateWolfETMasterFade() {
	auto &fade = S_WolfETMasterFade();
	if ( !wolfetStopAfterFade_ || RTCWClock() < fade.end ) return;
	fade.Value( RTCWClock() );
	wolfetStopAfterFade_ = false;
	StopAllSounds();
}

int AudioSystem::WolfETSoundLength( sfxHandle_t handle ) {
	const SoundSample *sample = GetSample( handle );
	if ( !sample || sample->Rate() <= 0 ) return -1;
	return static_cast<int>( static_cast<std::int64_t>( sample->Frames() ) * 1000 / sample->Rate() );
}

int AudioSystem::WolfETCurrentSoundTime() {
	// ET reports its mixer clock one second ahead, which is what its modules
	// compare a submitted loop start time against.
	const int rate = StreamOutputRate();
	return static_cast<int>( RTCWClock() ) + rate;
}

void AudioSystem::ClearRTCWMusic() {
	RTCWMusicOutput output{ *this };
	S_RTCWMusic().Clear( output );
}

void AudioSystem::StartBackgroundTrack( const char *intro, const char *loop ) {
	if ( Com_GameIsRTCWSP() ) {
		StartRTCWBackgroundTrack( intro, loop, 0 );
		return;
	}
	if ( !started_ ) {
		return;
	}

	backgroundIntro_ = SafeString( intro );
	backgroundLoop_ = ( loop != nullptr && loop[0] != '\0' ) ? SafeString( loop ) : backgroundIntro_;

	if ( backgroundIntro_.empty() ) {
		StopBackgroundTrack();
		return;
	}

	CloseBackgroundStream();
	backgroundStream_ = S_CodecOpenStream( backgroundIntro_.c_str() );
	musicPlayer_.Clear();
	if ( backgroundStream_ == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", backgroundIntro_.c_str() );
		backgroundIntro_.clear();
		backgroundLoop_.clear();
	}
}

void AudioSystem::StartStreamSource( snd_stream_t *stream ) {
	if ( !started_ ) {
		if ( stream ) S_CodecCloseStream( stream );
		return;
	}
	CloseBackgroundStream();
	musicPlayer_.Clear();
	backgroundIntro_.clear();
	backgroundLoop_.clear();
	backgroundStream_ = stream;
}

void AudioSystem::StopBackgroundTrack() {
	if ( Com_GameIsRTCWSP() ) {
		S_RTCWMusic().Stop();
		return;
	}
	CloseBackgroundStream();
	backgroundIntro_.clear();
	backgroundLoop_.clear();
	musicPlayer_.Clear();
}

void AudioSystem::UpdateBackgroundTrack() {
	if ( !started_ ) {
		return;
	}
	const float musicVolume = ( s_musicVolume != nullptr ) ? s_musicVolume->value : 1.0f;
	// American McGee's Alice's stream goes on being read at volume 0: its
	// songs keep playing silently, run out and hand over as retail's do.
	if ( musicVolume > 0.0f || Com_GameIsRTCWSP() || Com_GameIsRitual() ) {
		ServiceBackgroundTrack();
	}
	if ( Com_GameIsRTCWSP() ) {
		// OpenAL applies the fade to the playing source, including buffered PCM.
		// This avoids quantizing a short fade into whole decoded buffer chunks.
		musicPlayer_.Update( IsSoftMuted() ? 0.0f : musicVolume *
			ClampFloat( S_RTCWMusic().Gain( RTCWClock() ), 0.0f, 16.0f ) *
			fntech3::audio::rtcw::PlaybackGain( S_RTCWMasterFade().current ) );
	}
}

void AudioSystem::ServiceBackgroundTrack() {
	RTCWMusicOutput rtcwOutput{ *this };
	if ( Com_GameIsRTCWSP() && !S_RTCWMusic().BeginService( rtcwOutput,
		s_musicVolume != nullptr ? s_musicVolume->value : 1.0f ) ) return;
	if ( backgroundStream_ == nullptr ) {
		return;
	}

	// Cap decode work per frame so starting a track does not burst several
	// large codec reads into one frame; the queue still fills within a couple
	// of frames, well inside the buffered playback margin.
	int emptyRestarts = 0;
	int chunksQueuedThisCall = 0;
	while ( musicPlayer_.QueuedBufferCount() < kQueuedStreamChunks && chunksQueuedThisCall < kMaxStreamChunksPerService ) {
		std::array<byte, 32768> raw;
		const int readBytes = fntech3::audio::WholeFrameReadBytes( static_cast<int>( raw.size() ),
			backgroundStream_->info.width, backgroundStream_->info.channels );
		if ( !readBytes ) { StopBackgroundTrack(); break; }
		int bytesRead;
		if ( backgroundPendingPCM_.empty() ) {
			bytesRead = S_CodecReadStream( backgroundStream_, readBytes, raw.data() );
		} else {
			bytesRead = static_cast<int>( backgroundPendingPCM_.size() );
			std::memcpy( raw.data(), backgroundPendingPCM_.data(), backgroundPendingPCM_.size() );
		}
		if ( bytesRead <= 0 ) {
			// Finish the interpolation tail exactly once before changing files.
			// A full buffer pool can delay the flush; retry without closing the
			// decoder or dropping audio that is still queued for playback.
			if ( !FlushStream( musicPlayer_ ) ) break;
			if ( Com_GameIsRTCWSP() ) {
				S_RTCWMusic().EndOfTrack( rtcwOutput );
				break;
			}
			if ( !backgroundLoop_.empty() ) {
				if ( emptyRestarts > 0 ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: music loop %s produced no audio\n", backgroundLoop_.c_str() );
					CloseBackgroundStream();
					backgroundIntro_.clear();
					backgroundLoop_.clear();
					break;
				}
				CloseBackgroundStream();
				backgroundStream_ = S_CodecOpenStream( backgroundLoop_.c_str() );
				if ( backgroundStream_ == nullptr ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", backgroundLoop_.c_str() );
					backgroundIntro_.clear();
					backgroundLoop_.clear();
					break;
				}
				++emptyRestarts;
				continue;
			}
			// Natural EOF closes the file but lets the source drain. Explicit
			// StopBackgroundTrack still cancels the queued audio immediately.
			CloseBackgroundStream();
			backgroundIntro_.clear();
			backgroundLoop_.clear();
			break;
		}

		if ( backgroundStream_->info.rate <= 0 ||
			( backgroundStream_->info.width != 1 && backgroundStream_->info.width != 2 ) ||
			( !PCMChannelCountCanBeRepresented( backgroundStream_->info.channels ) &&
				!fntech3::audio::NeedsSpeakerDownmix( backgroundStream_->info.speakerMask, backgroundStream_->info.channels ) ) ) {
			StopBackgroundTrack();
			break;
		}

		size_t frameBytesSize = 0;
		if ( !PCMFrameByteCount( 1, backgroundStream_->info.channels, backgroundStream_->info.width, frameBytesSize ) ||
			frameBytesSize > static_cast<size_t>( std::numeric_limits<int>::max() ) ) {
			StopBackgroundTrack();
			break;
		}
		const int frameBytes = static_cast<int>( frameBytesSize );
		const int inputSamples = bytesRead / frameBytes;
		if ( inputSamples <= 0 ) {
			break;
		}
		const auto &info = backgroundStream_->info;
		const bool route = fntech3::audio::NeedsSpeakerDownmix( info.speakerMask, info.channels );
		const auto routed = route ? fntech3::audio::DownmixSpeakers( raw.data(), inputSamples,
			info.width, info.channels, info.speakerMask ) : std::vector<short>{};
		if ( route && routed.empty() ) { StopBackgroundTrack(); break; }
		const int channels = route ? 2 : info.channels;
		const int outputChannels = SelectStreamOutputChannels( device_, channels );
		if ( !QueueStreamChunk( musicPlayer_, inputSamples, info.rate, route ? 2 : info.width,
			channels, route ? reinterpret_cast<const byte *>( routed.data() ) : raw.data(), 1.0f, outputChannels, true ) ) {
			// The decoder has already advanced. Keep this bounded chunk until
			// the pool/source accepts it, instead of skipping ahead on retry.
			if ( backgroundPendingPCM_.empty() ) backgroundPendingPCM_.assign( raw.data(), raw.data() + bytesRead );
			break;
		}
		backgroundPendingPCM_.clear();
		emptyRestarts = 0;
		++chunksQueuedThisCall;
	}
}

void AudioSystem::RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}

	const int outputChannels = SelectStreamOutputChannels( device_, channels );
	QueueStreamChunk( rawPlayer_, samples, rate, width, channels, data, volume, outputChannels, false );
}

void AudioSystem::AddVoiceSamples( int clientNum, int samples, int rate, const short *data ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}
	voicePlayer_.Queue( clientNum, samples, rate, data );
}

void AudioSystem::StopAllSounds() {
	if ( !started_ ) {
		return;
	}

	world_.StopAllSounds();
	ClearRTCWEntityStreams();
	rawPlayer_.Clear();
	voicePlayer_.Clear();
	if ( Com_GameIsRTCWSP() ) ClearRTCWMusic();
	else StopBackgroundTrack();
}

void AudioSystem::ClearRTCWSounds( qboolean clearMusic ) {
	if ( !started_ ) return;
	world_.StopAllSounds();
	ClearRTCWEntityStreams();
	rawPlayer_.Clear();
	voicePlayer_.Clear();
	if ( clearMusic ) ClearRTCWMusic();
	else UpdateBackgroundTrack();
}

void AudioSystem::ClearLoopingSounds( qboolean killall ) {
	world_.ClearLoopingSounds( killall );
}

void AudioSystem::ClearLoopingSoundsFrame() {
	world_.ClearLoopingSoundsFrame();
}

qboolean AudioSystem::GetSpatialDebugInfo( spatialAudioDebugInfo_t *info ) const {
	int preferredEntity;
	int overlayMode;

	if ( !started_ || info == nullptr || s_alDebugOverlay == nullptr ) {
		return qfalse;
	}

	overlayMode = s_alDebugOverlay->integer;
	if ( overlayMode <= 0 ) {
		return qfalse;
	}

	preferredEntity = ( s_alDebugVoice != nullptr ) ? s_alDebugVoice->integer : -1;
	return world_.GetSpatialDebugInfo( info, device_, preferredEntity, overlayMode );
}

void AudioSystem::DumpSpatialDebug() const {
	const int preferredEntity = ( s_alDebugVoice != nullptr ) ? s_alDebugVoice->integer : -1;

	if ( !started_ ) {
		Com_Printf( "OpenAL spatial audio debug unavailable: backend not started\n" );
		return;
	}

	world_.DumpSpatialDebug( device_, preferredEntity );
}

void AudioSystem::AddWolfETLoopingSound( const float *origin, const float *velocity,
	int range, sfxHandle_t handle, int volume, int soundTime ) {
	if ( !started_ || hardMuted_ || cls.state != CA_ACTIVE || !world_.AcceptsRTCWLoop( volume ) ) return;
	SoundSample *sample = GetSample( handle );
	if ( !sample ) Com_Error( ERR_DROP, "ET looping sound handle %d is invalid", handle );
	if ( !sample->EnsureLoaded( device_, handle == 0 ) ) return;
	world_.AddWolfETLoopingSound( origin, velocity, range, handle, volume, sample,
		S_WolfETMasterFade().current, soundTime );
}

/*
The mouths Elite Force SP's game reads. The device does the mixing, so the
measurement is taken when the game asks for it rather than as a mix ends;
retail's own wait of 800 mixed samples still holds every channel between
measurements, counted here against the rate the device runs at.
*/
const int *AudioSystem::STVEFSPMouths() {
	const int rate = StreamOutputRate();
	const std::int64_t mixed = std::int64_t{ Sys_Milliseconds() } * rate / 1000;
	world_.STVEFSPMeasureMouths( static_cast<int>( mixed ) );
	return world_.STVEFSPMouths();
}

void AudioSystem::StartMOHAASound( const float *origin, int entnum, int entchannel, sfxHandle_t handle,
	float volume, float minDist, float maxDist, float pitch, int mohaaChannel ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}
	if ( origin == nullptr && ( entnum < 0 || entnum >= MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartMOHAASound: bad entitynum %i", entnum );
	}
	SoundSample *sample = GetSample( handle );
	if ( sample == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "S_StartMOHAASound: handle %i out of range\n", handle );
		return;
	}
	if ( !sample->EnsureLoaded( device_, handle == 0 ) ) {
		return;
	}
	world_.StartMOHAASound( entnum, entchannel, handle, sample, origin, volume, minDist, maxDist, pitch, mohaaChannel );
}

void AudioSystem::StopMOHAASound( int entnum, int mohaaChannel ) {
	if ( started_ ) {
		world_.StopMOHAASound( entnum, mohaaChannel );
	}
}

void AudioSystem::AddMOHAALoopingSound( int slot, const float *origin, const float *velocity, sfxHandle_t handle,
	float volume, float minDist, float maxDist, float pitch ) {
	if ( !started_ || hardMuted_ || cls.state != CA_ACTIVE ) {
		return;
	}
	SoundSample *sample = GetSample( handle );
	if ( sample == nullptr || !sample->EnsureLoaded( device_, handle == 0 ) ) {
		return;
	}
	world_.AddMOHAALoopingSound( slot, origin, velocity, handle, sample, volume, minDist, maxDist, pitch );
}

void AudioSystem::AddSTVEFAmbientLoopingSound( const float *origin, sfxHandle_t handle, int volume ) {
	if ( !started_ || hardMuted_ || cls.state != CA_ACTIVE || volume <= 0
		|| !world_.AcceptsSTVEFAmbientLoop() ) return;
	SoundSample *sample = GetSample( handle );
	if ( !sample ) Com_Error( ERR_DROP, "Elite Force ambient looping sound handle %d is invalid", handle );
	if ( !sample->EnsureLoaded( device_, handle == 0 ) ) return;
	world_.AddSTVEFAmbientLoopingSound( origin, handle, volume, sample );
}

void AudioSystem::AddJKAAmbientLoopingSound( const float *origin, sfxHandle_t handle, int volume ) {
	if ( !started_ || hardMuted_ || cls.state != CA_ACTIVE || volume <= 0 ) return;
	SoundSample *sample = GetSample( handle );
	if ( !sample ) Com_Error( ERR_DROP, "Jedi Academy ambient looping sound handle %d is invalid", handle );
	if ( !sample->EnsureLoaded( device_, handle == 0 ) ) return;
	world_.AddJKAAmbientLoopingSound( origin, handle, volume, sample );
}

void AudioSystem::AddRTCWLoopingSound( const float *origin, const float *velocity,
	int range, sfxHandle_t handle, int volume ) {
	if ( !started_ || hardMuted_ || cls.state != CA_ACTIVE || !world_.AcceptsRTCWLoop( volume ) ) return;
	SoundSample *sample = GetSample( handle );
	if ( !sample ) Com_Error( ERR_DROP, "RTCW looping sound handle %d is invalid", handle );
	if ( !sample->EnsureLoaded( device_, handle == 0 ) ) return;
	world_.AddRTCWLoopingSound( origin, velocity, range, handle, volume, sample,
		Com_GameIsRTCWSP() ? S_RTCWMasterFade().current : 1.0f );
}

void AudioSystem::AddLoopingSound( int entityNum, const float *origin, const float *velocity, sfxHandle_t sfxHandle ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}

	SoundSample *sample = GetSample( sfxHandle );
	if ( sample == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}
	if ( !sample->EnsureLoaded( device_, sfxHandle == 0 ) ) {
		return;
	}

	world_.AddLoopingSound( entityNum, origin, velocity, sfxHandle, sample, qfalse );
}

void AudioSystem::AddRealLoopingSound( int entityNum, const float *origin, const float *velocity, sfxHandle_t sfxHandle ) {
	if ( !started_ || hardMuted_ ) {
		return;
	}

	SoundSample *sample = GetSample( sfxHandle );
	if ( sample == nullptr ) {
		Com_Printf( S_COLOR_YELLOW "S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}
	if ( !sample->EnsureLoaded( device_, sfxHandle == 0 ) ) {
		return;
	}

	world_.AddLoopingSound( entityNum, origin, velocity, sfxHandle, sample, qtrue );
}

void AudioSystem::StopLoopingSound( int entityNum ) {
	world_.StopLoopingSound( entityNum );
}

void AudioSystem::Respatialize( int entityNum, const float *origin, float axis[3][3], int /*inwater*/ ) {
	if ( !started_ ) {
		return;
	}

	world_.Respatialize( entityNum, origin, axis );
}

void AudioSystem::UpdateEntityPosition( int entityNum, const float *origin ) {
	world_.UpdateEntityPosition( entityNum, origin );
}

void AudioSystem::Update( int msec ) {
	if ( !started_ ) {
		return;
	}
	if ( Com_GameIsWolfET() ) UpdateWolfETMasterFade();
	if ( !UpdateDeviceState( msec ) ) {
		rtcwClock_.Pause();
		return;
	}

	const qboolean softMuted = IsSoftMuted();
	const float streamGain = Com_GameIsRTCWSP() ?
		fntech3::audio::rtcw::PlaybackGain( S_RTCWMasterFade().Value( RTCWClock() ) ) : 1.0f;
	// SP effects quantize the master fade independently of streamed audio.
	// American McGee's Alice's voices carry s_volume in their own gains, as
	// retail hands it to Miles with each sample, and its music and cinematic
	// track never take it (alice.exe 0x00472580, 0x00472210).
	// Medal of Honor's `fadesound` scales the listener, so every source fades.
	device_.SetMasterGain( Com_GameIsRTCWSP() || Com_GameIsRitual() ? 1.0f :
		( s_volume != nullptr ? s_volume->value : 1.0f ) * S_MOHAAFadeGain() );
	device_.ApplyDopplerState();
	const bool deferredUpdates = device_.BeginDeferredUpdates();
	world_.Update( softMuted );
	ServiceRTCWEntityStreams();
	if ( deferredUpdates ) {
		device_.EndDeferredUpdates();
	}
	// RawSamples has no end-of-stream callback. Drain its small interpolation
	// tail once the submitted buffers have played out. Keep its phase/history
	// for later chunks; explicit clears and format changes finish that timeline.
	if ( rawPlayer_.Idle() ) FlushStream( rawPlayer_, true );
	rawPlayer_.Update( softMuted ? 0.0f : streamGain );
	const float voiceVolume = ( s_voiceVolume != nullptr ) ? ClampFloat( s_voiceVolume->value, 0.0f, 2.0f ) : 1.0f;
	voicePlayer_.Update( softMuted ? 0.0f : voiceVolume * streamGain );
	const float musicVolume = ( s_musicVolume != nullptr ) ? s_musicVolume->value : 1.0f;
	// Alice's stream arrives at the volumes Miles would have been handed.
	if ( !Com_GameIsRTCWSP() ) musicPlayer_.Update( softMuted ? 0.0f : ( Com_GameIsRitual() ? 1.0f : musicVolume ) );
	UpdateBackgroundTrack();
}

void AudioSystem::DisableSounds() {
	world_.StopAllSounds();
	ClearRTCWEntityStreams();
	rawPlayer_.Clear();
	voicePlayer_.Clear();
	if ( Com_GameIsRTCWSP() ) ClearRTCWMusic();
	else StopBackgroundTrack();
	hardMuted_ = true;
}

void AudioSystem::ClearSoundBuffer() {
	world_.ClearSoundBuffer();
	ClearRTCWEntityStreams();
	rawPlayer_.Clear();
	voicePlayer_.Clear();
}

void AudioSystem::SoundInfo() {
	if ( Com_GameIsRTCW() ) {
		int active = 0, buffered = 0;
		for ( int i = 1; i < entityStreams_.policy.Limit(); ++i ) {
			active += entityStreams_.policy[i].active;
			buffered += !entityStreamLanes_[i].players[0].Idle() || !entityStreamLanes_[i].players[1].Idle();
		}
		Com_Printf( "RTCW entity streams: %d active, %d buffered\n", active, buffered );
	}
	device_.RefreshDeviceConnection();
	device_.RefreshTimingDiagnostics();
	const ModernOpenALCapabilities &caps = device_.Capabilities();

	Com_Printf( "----- Sound Info -----\n" );
	Com_Printf( "Using OpenAL backend\n" );
	const bool focusMuted = IsSoftMuted() != qfalse;
	Com_Printf( "Master volume: %.2f\n", s_volume != nullptr ? s_volume->value : 1.0f );
	if ( Com_GameIsMOHAA() ) {
		Com_Printf( "fadesound gain: %.2f\n", S_MOHAAFadeGain() );
	}
	Com_Printf( "Mute state: %s%s\n",
		hardMuted_ ? "registration-muted (normal during startup/registration)" :
			( focusMuted ? "focus-muted" : gw_offscreen ? "offscreen-muted (window's desktop is not on screen)" : "audible" ),
		( s_volume != nullptr && s_volume->value <= 0.0f ) ? ", master volume is zero" : "" );
	Com_Printf( "OpenAL library: %s\n", device_.LibraryName().empty() ? "unknown" : device_.LibraryName().c_str() );
	Com_Printf( "Requested device: %s\n", device_.RequestedDeviceName().empty() ? "default" : device_.RequestedDeviceName().c_str() );
	Com_Printf( "Active device: %s\n", device_.ActiveDeviceName().empty() ? "unknown" : device_.ActiveDeviceName().c_str() );
	if ( device_.UsingDefaultFallback() ) {
		Com_Printf( "Device fallback: using system default device\n" );
	}
	Com_Printf( "Device state: %s%s\n",
		caps.connectedQuery ? ALCConnectedName( caps.connectedState ) : "not-queried",
		( caps.reopenDevice && caps.alcReopenDeviceSOFT != nullptr ) ? ", live reopen supported" : "" );
	Com_Printf( "Context attributes: %s%s\n",
		device_.ContextAttributeMode().empty() ? "not-created" : device_.ContextAttributeMode().c_str(),
		device_.ContextUsedFallback() ? " (fallback)" : "" );
	Com_Printf( "EFX support: %s\n", device_.HasEFX() ? "enabled" : "unavailable" );
	if ( device_.HasEFX() ) {
		Com_Printf( "Auxiliary sends: %d\n", device_.MaxAuxiliarySends() );
		Com_Printf( "Reverb send: %s (%s effect, %s)\n",
			device_.HasReverb() ? "enabled" : "disabled",
			device_.ReverbEffectName(),
			device_.CurrentReverbName() );
		Com_Printf( "Air absorption: %.2f (world units calibrated to %.4f m)\n",
			ClampFloat( ( s_alAirAbsorption != nullptr ) ? s_alAirAbsorption->value : 0.0f, 0.0f, 10.0f ),
			kMetersPerGameUnit );
	}
	Com_Printf( "Doppler: %s (factor %.2f, speed of sound %.0f u/s)\n",
		( s_doppler != nullptr && s_doppler->integer ) ? "enabled" : "disabled",
		ClampFloat( ( s_alDopplerFactor != nullptr ) ? s_alDopplerFactor->value : 1.0f, 0.0f, 10.0f ),
		ClampFloat( ( s_alDopplerSpeed != nullptr ) ? s_alDopplerSpeed->value : 6000.0f, 1000.0f, 20000.0f ) );
	const bool stereoSpatializeRequested = CvarIntegerOrDefault( s_alSpatializeStereo, 0 ) != 0;
	Com_Printf( "OpenAL requested render: HRTF %s (id %s), output %s, distance %s, limiter %s, stereo spatialize %s%s\n",
		CvarStringOrDefault( s_alHrtf, "auto" ),
		CvarStringOrDefault( s_alHrtfId, "default" ),
		CvarStringOrDefault( s_alOutputMode, "auto" ),
		CvarStringOrDefault( s_alDistanceModel, "inverse_clamped" ),
		ALCBooleanName( CvarIntegerOrDefault( s_alOutputLimiter, 1 ) ? ALC_TRUE : ALC_FALSE ),
		stereoSpatializeRequested ? "on" : "off",
		( stereoSpatializeRequested && !caps.sourceSpatialize ) ? " (unsupported)" : "" );
	Com_Printf( "OpenAL active render: HRTF %s, output %s, distance %s, limiter %s\n",
		( caps.hrtfStatus >= 0 ) ? HrtfStatusName( caps.hrtfStatus ) : "not-queried",
		( caps.outputModeValue >= 0 ) ? ALCOutputModeName( caps.outputModeValue ) : "not-queried",
		caps.distanceModelValid ? ALDistanceModelName( caps.distanceModel ) : "not-queried",
		( caps.outputLimiterState >= 0 ) ? ALCBooleanName( caps.outputLimiterState ) : "not-queried" );
	Com_Printf( "OpenAL requested context: frequency %d Hz, refresh %d Hz, mono sources %d, stereo sources %d\n",
		CvarIntegerOrDefault( s_alFrequency, 48000 ),
		CvarIntegerOrDefault( s_alRefresh, 100 ),
		CvarIntegerOrDefault( s_alMonoSources, 64 ),
		CvarIntegerOrDefault( s_alStereoSources, 8 ) );
	Com_Printf( "OpenAL active context: frequency %d Hz, refresh %d Hz, mono sources %d, stereo sources %d\n",
		caps.mixerFrequency, caps.refreshRate, caps.monoSources, caps.stereoSources );
	Com_Printf( "OpenAL PCM layouts: %s\n", device_.PCMChannelSupportDescription() );
	Com_Printf( "OpenAL stream output: %d Hz (band-limited sample-rate conversion)\n", StreamOutputRate() );
	if ( caps.deviceClockValuesValid ) {
		Com_Printf( "OpenAL timing: device clock %.3f s, device latency %.2f ms\n",
			NanosecondsToSeconds( caps.deviceClockValue ),
			NanosecondsToMilliseconds( caps.deviceLatency ) );
	} else if ( caps.deviceClock ) {
		Com_Printf( "OpenAL timing: device clock supported, latency unavailable\n" );
	} else {
		Com_Printf( "OpenAL timing: device clock unavailable\n" );
	}
	device_.PrintCapabilityMatrix();
	Com_Printf( "Occlusion: %s (strength %.2f)\n",
		( s_alOcclusion != nullptr && s_alOcclusion->integer ) ? "enabled" : "disabled",
		( s_alOcclusionStrength != nullptr ) ? s_alOcclusionStrength->value : 1.0f );
	world_.PrintAudioZoneInfo();
	Com_Printf( "%5d voice sources (%d free)\n", device_.TotalVoiceCount(), device_.FreeVoiceCount() );
	Com_Printf( "%5d stream buffers (%d free)\n", device_.BufferPool().TotalCount(), device_.BufferPool().FreeCount() );
	Com_Printf( "Music stream buffers: %d queued\n", musicPlayer_.QueuedBufferCount() );
	Com_Printf( "Raw stream buffers: %d queued\n", rawPlayer_.QueuedBufferCount() );
	Com_Printf( "Remote voice lanes: %d allocated, %d active\n", voicePlayer_.LaneCount(), voicePlayer_.ActiveLaneCount() );
	weaponSoundShaders_.EnsureLoaded();
	Com_Printf( "%5d weapon sound shader rules\n", weaponSoundShaders_.Count() );
	Com_Printf( "%5lu registered samples\n", static_cast<unsigned long>( samples_.size() ) );
	const auto &intro = Com_GameIsRTCWSP() ? S_RTCWMusic().Name() : backgroundIntro_;
	const auto &loop = Com_GameIsRTCWSP() ? S_RTCWMusic().Loop() : backgroundLoop_;
	Com_Printf( "Background track: %s\n", intro.empty() ? "none" : intro.c_str() );
	if ( !loop.empty() && loop != intro ) {
		Com_Printf( "Background loop: %s\n", loop.c_str() );
	}
	Com_Printf( "----------------------\n" );
}

void AudioSystem::SoundList() {
	for ( size_t i = 0; i < samples_.size(); ++i ) {
		const SoundSample &sample = samples_[i];
		if ( sample.Loaded() ) {
			Com_Printf( "%4d : %s [%s %d Hz %d ms%s]\n",
				static_cast<int>( i ),
				sample.Name().c_str(),
				sample.FormatName(),
				sample.Rate(),
				sample.DurationMs(),
				sample.DefaultSample() ? " fallback" : "" );
		} else {
			Com_Printf( "%4d : %s [%s]\n",
				static_cast<int>( i ),
				sample.Name().c_str(),
				sample.Missing() ? "missing" : "unloaded" );
		}
	}
}

void AudioSystem::ListDevices() const {
	PrintOpenALDeviceList( ( started_ && device_.Ready() ) ? device_.ActiveDeviceName().c_str() : nullptr );
}

void AudioSystem::ListHrtfs() const {
	if ( started_ && device_.Ready() ) {
		device_.PrintHrtfList();
		return;
	}

	PrintOpenALHrtfListForRequestedDevice();
}

void AudioSystem::PrintOpenALSoftConfigHints() const {
	if ( started_ && device_.Ready() ) {
		device_.PrintOpenALSoftConfigHints();
		return;
	}

	Com_Printf( "----- OpenAL Soft Configuration Hints -----\n" );
	Com_Printf( "OpenAL backend is not active; start it with s_backend openal and snd_restart for live capability details.\n" );
#if defined(_WIN32)
	Com_Printf( "Config files: %%AppData%%\\alsoft.ini, or an app-local alsoft.ini beside the executable.\n" );
#else
	Com_Printf( "Config files: $XDG_CONFIG_HOME/alsoft.conf, /etc/xdg/alsoft.conf, or an app-local alsoft.conf beside the executable.\n" );
#endif
	Com_Printf( "Useful OpenAL Soft options include stereo-mode, stereo-encoding, hrtf-mode, hrtf-size, resampler, period_size, periods, output-limiter, channels, sample-type, and decoder hq-mode/distance-comp/nfc.\n" );
	Com_Printf( "-------------------------------------------\n" );
}

bool AudioSystem::RecoverDevice( bool force ) {
	if ( !started_ || !device_.Ready() ) {
		Com_Printf( "OpenAL device recovery unavailable for backend '%s'\n",
			( s_backendActive != nullptr ) ? s_backendActive->string : "none" );
		return false;
	}

	const bool recovered = device_.RecoverDevice( force );
	deviceRecovery_.warningPrinted = !device_.DeviceConnected();
	deviceRecovery_.retryMs = recovered ? 0 : kOpenALDeviceRecoveryRetryMs;
	return recovered;
}
