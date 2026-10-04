// Run the engine's actual stream player against an OpenAL Soft loopback device.
// SPDX-License-Identifier: GPL-2.0-or-later
// The device adapter supplies only source configuration and buffer allocation;
// queueing, reclamation, deferred starts and speech tracking are production code.
class StreamBufferPool {
public:
	explicit StreamBufferPool( const OpenALSymbols &al ) : al_( al ) {}
	~StreamBufferPool() { for ( ALuint buffer : all_ ) al_.alDeleteBuffers( 1, &buffer ); }
	ALuint Acquire() {
		if ( !free_.empty() ) { const auto buffer = free_.back(); free_.pop_back(); return buffer; }
		ALuint buffer = 0;
		al_.alGenBuffers( 1, &buffer );
		if ( buffer ) all_.push_back( buffer );
		return buffer;
	}
	void Release( ALuint buffer ) { free_.push_back( buffer ); }
	size_t Used() const { return all_.size() - free_.size(); }
	size_t Total() const { return all_.size(); }
private:
	const OpenALSymbols &al_;
	std::vector<ALuint> all_, free_;
};

class OpenALDevice {
public:
	explicit OpenALDevice( const OpenALSymbols &al ) : al_( al ), pool_( al ) {}
	const OpenALSymbols &AL() const { return al_; }
	StreamBufferPool &BufferPool() { return pool_; }
	bool SupportsPCMChannels( int channels ) const { return channels == 1 || channels == 2; }
	ALenum PCM16FormatForChannels( int channels ) const { return channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16; }
	void SetSourceSpatialize( ALuint, bool ) const {}
	void SetSourceDirectChannels( ALuint, bool ) const {}
	void ConfigureSourceDistance( ALuint source, bool ) const { al_.alSourcef( source, AL_ROLLOFF_FACTOR, 0 ); }
	int TotalVoiceCount() const { return 0; }
	int FreeVoiceCount() const { return 0; }
	ALuint AcquireVoiceSource() const { return 0; }
	void ReleaseVoiceSource( ALuint ) const {}
private:
	const OpenALSymbols &al_;
	StreamBufferPool pool_;
};

bool PCMByteCountFitsALsizei( size_t count ) { return count <= static_cast<size_t>( ( std::numeric_limits<ALsizei>::max )() ) / sizeof( short ); }
bool Com_GameIsRTCWSP() { return false; }
float ClampFloat( float value, float minimum, float maximum ) { return std::clamp( value, minimum, maximum ); }
void Com_DPrintf( const char *, ... ) {}
int Sys_Milliseconds() { return 0; }
struct TestVoiceStep { float value; };
TestVoiceStep *s_voiceStep = nullptr;

#include "../../code/client/audio/openal/AudioSystemStreams.inl"

void StreamPlayerLoopbackTest( const OpenALSymbols &al, ALCdevice *device, TestRunner &runner ) {
	OpenALDevice adapter( al );
	ALuint source = 0;
	al.alGenSources( 1, &source );
	StreamPlayer player;
	player.Init( &adapter, source );
	player.deferStart = true;
	const auto pcm = MakeMonoTone( 997.0f );
	constexpr int frames = 2048;
	bool retained = true, audible = true, recycled = true;
	for ( int iteration = 0; iteration < 100; ++iteration ) {
		player.Clear();
		retained &= player.QueuePCM16( pcm.data(), frames, 1, kSampleRate, false );
		retained &= player.QueuePCM16( pcm.data() + frames, frames, 1, kSampleRate, false );
		retained &= !player.Idle() && player.QueuedBufferCount() == 2;
		retained &= std::fabs( player.QueuedSeconds() - 2.0 * frames / kSampleRate ) < 1e-9;
		player.Update( 1.0f );
		std::vector<float> output( 2 * ( frames * 2 + 1024 ) );
		al.alcRenderSamplesSOFT( device, output.data(), static_cast<ALsizei>( output.size() / 2 ) );
		audible &= Rms( output, -1 ) > 0.1;
		recycled &= player.Idle() && adapter.BufferPool().Used() == 0;
		// Natural exhaustion leaves AL_STOPPED. Start a new deferred segment
		// without Clear and verify its two buffers survive until Update.
		retained &= player.QueuePCM16( pcm.data(), frames, 1, kSampleRate, false );
		retained &= player.QueuePCM16( pcm.data() + frames, frames, 1, kSampleRate, false );
		retained &= !player.Idle() && player.QueuedBufferCount() == 2;
		player.Update( 1.0f );
		al.alcRenderSamplesSOFT( device, output.data(), static_cast<ALsizei>( output.size() / 2 ) );
		audible &= Rms( output, -1 ) > 0.1;
		recycled &= player.Idle() && adapter.BufferPool().Used() == 0;
	}
	player.Shutdown();
	VoiceChatPlayer voice;
	voice.Init( &adapter );
	voice.Queue( 0, frames, kSampleRate, pcm.data() );
	voice.Update( 1.0f );
	voice.Clear();
	voice.Shutdown();
	retained &= voice.LaneCount() == 0 && voice.ActiveLaneCount() == 0;
	retained &= player.RTCWSpeechAmplitude() == 0;
	al.alDeleteSources( 1, &source );
	runner.Check( retained, "engine stream delayed start and underrun", "unplayed buffers were reclaimed" );
	runner.Check( audible, "engine stream PCM after restart", "rendered stream was silent or attenuated unexpectedly" );
	runner.Check( recycled && adapter.BufferPool().Total() == 2, "engine stream 200-cycle buffer reuse", "stream buffers leaked or grew across cycles" );
	runner.Check( al.alGetError() == AL_NO_ERROR, "engine stream OpenAL errors", "stream lifecycle left an OpenAL error" );
}
