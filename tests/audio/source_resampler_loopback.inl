// SPDX-License-Identifier: GPL-2.0-or-later
// Real-runtime quality witness for the source filter selected by the engine.
static double SourceToneAmplitude( const std::vector<float> &pcm, double frequency ) {
	constexpr double pi = 3.14159265358979323846;
	constexpr size_t begin = 6000, count = 36000;
	double sine = 0.0, cosine = 0.0;
	for ( size_t i = begin; i < begin + count; ++i ) {
		const double phase = 2.0 * pi * frequency * static_cast<double>( i ) / 48000.0;
		sine += pcm[i] * std::sin( phase );
		cosine += pcm[i] * std::cos( phase );
	}
	return 2.0 * std::hypot( sine, cosine ) / static_cast<double>( count );
}

static std::vector<float> RenderSourceTone( const OpenALSymbols &al, ALCdevice *device,
	int rate, double frequency, int resampler ) {
	constexpr double pi = 3.14159265358979323846;
	std::vector<short> pcm( static_cast<size_t>( rate ) );
	for ( size_t i = 0; i < pcm.size(); ++i ) {
		pcm[i] = static_cast<short>( std::lround( 12000.0 * std::sin( 2.0 * pi * frequency * static_cast<double>( i ) / rate ) ) );
	}
	std::vector<float> output( 48000 );
	ALuint source = 0, buffer = 0;
	al.alGenBuffers( 1, &buffer );
	al.alBufferData( buffer, AL_FORMAT_MONO16, pcm.data(), static_cast<ALsizei>( pcm.size() * sizeof( short ) ), rate );
	al.alGenSources( 1, &source );
	al.alSourcei( source, AL_BUFFER, static_cast<ALint>( buffer ) );
	al.alSourcei( source, AL_LOOPING, AL_TRUE );
	al.alSourcef( source, AL_GAIN, 0.5f );
	if ( resampler >= 0 ) {
		al.alSourcei( source, AL_SOURCE_RESAMPLER_SOFT, resampler );
	}
	al.alSourcePlay( source );
	al.alcRenderSamplesSOFT( device, output.data(), 48000 );
	al.alSourceStop( source );
	al.alDeleteSources( 1, &source );
	al.alDeleteBuffers( 1, &buffer );
	return output;
}

static void SourceResamplerLoopbackTest( const DynamicLibrary &library, const OpenALSymbols &al,
	ALCdevice *device, TestRunner &runner ) {
	ALCcontext *context = nullptr;
	if ( al.alcIsRenderFormatSupportedSOFT( device, 48000, ALC_MONO_SOFT, ALC_FLOAT_SOFT ) != ALC_TRUE ) {
		runner.Skip( "source resampler quality", "mono float loopback unavailable" );
		return;
	}
	if ( !CreateLoopbackContext( al, device, ALC_MONO_SOFT, false, 0, context, runner, "source resampler context" ) ) {
		return;
	}
	const auto getIntegerv = LoadExtension<decltype( &::alGetIntegerv )>( library, al, device, "alGetIntegerv" );
	const auto getStringi = LoadExtension<LPALGETSTRINGISOFT>( library, al, device, "alGetStringiSOFT" );
	if ( al.alIsExtensionPresent( "AL_SOFT_source_resampler" ) != AL_TRUE || getIntegerv == nullptr || getStringi == nullptr ) {
		runner.Skip( "source resampler quality", "optional source resampler API unavailable" );
		DestroyLoopbackContext( al, context );
		return;
	}
	ALint count = 0;
	getIntegerv( AL_NUM_RESAMPLERS_SOFT, &count );
	if ( count <= 0 || count > 128 || !CheckAL( al, runner, "source resampler inventory" ) ) {
		runner.Check( false, "source resampler inventory", "invalid runtime count" );
		DestroyLoopbackContext( al, context );
		return;
	}
	std::vector<std::string> names;
	for ( ALint index = 0; index < count; ++index ) {
		const char *name = getStringi( AL_RESAMPLER_NAME_SOFT, index );
		if ( name == nullptr ) {
			runner.Check( false, "source resampler inventory", "missing runtime name" );
			DestroyLoopbackContext( al, context );
			return;
		}
		names.emplace_back( name );
	}
	const int selected = fntech3::audio::SelectSourceResampler( names, "balanced" );
	if ( selected < 0 ) {
		runner.Skip( "source resampler quality", "runtime exposes no recognized sinc filter" );
		DestroyLoopbackContext( al, context );
		return;
	}
	al.alDistanceModel( AL_NONE );
	// Older runtimes may offer only the narrower 12-tap sinc. Measure its
	// supported band; the 24-tap default also has a separate high-band witness.
	const double frequency = names[static_cast<size_t>( selected )].find( "11th" ) != std::string::npos ||
		names[static_cast<size_t>( selected )].find( "12" ) != std::string::npos ? 1000.0 : 2800.0;
	const double reference = SourceToneAmplitude( RenderSourceTone( al, device, 48000, frequency, -1 ), frequency );
	const auto up = RenderSourceTone( al, device, 8000, frequency, selected );
	const double wanted = SourceToneAmplitude( up, frequency );
	const double image = SourceToneAmplitude( up, 8000.0 - frequency );
	const double downReference = SourceToneAmplitude( RenderSourceTone( al, device, 48000, 18000.0, -1 ), 18000.0 );
	const double alias = SourceToneAmplitude( RenderSourceTone( al, device, 96000, 30000.0, selected ), 18000.0 );
	std::printf( "Source filter %s: gain %.9f, image %.2f dB, alias %.2f dB\n",
		names[static_cast<size_t>( selected )].c_str(), wanted / reference,
		20.0 * std::log10( std::max( image / wanted, 1e-15 ) ),
		20.0 * std::log10( std::max( alias / downReference, 1e-15 ) ) );
	runner.Check( reference > 0.01 && std::isfinite( wanted ) && std::fabs( wanted / reference - 1.0 ) < 0.02 &&
		image < wanted * 0.001, "source sinc upsampling", "wanted tone or image rejection outside bounds" );
	runner.Check( downReference > 0.01 && std::isfinite( alias ) && alias < downReference * 0.001,
		"source sinc downsampling", "alias rejection below 60 dB" );
	CheckAL( al, runner, "source resampler render status" );
	DestroyLoopbackContext( al, context );
}
