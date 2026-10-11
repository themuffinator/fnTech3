"""Execute the OpenAL world's rendering of RTCW's and Enemy Territory's voices.

The shared Wolfenstein headers keep retail's bookkeeping and its law for how
far a sound carries; the OpenAL world renders them as it renders every
profile's sounds (AGENTS.md invariant 11): positional, the device's distance
model across the law's span, traced occlusion, the room's sends and the tone
classes. These probes compile the real `Q3SoundWorld::ApplyVoice` and the
helpers it calls against a recording device.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]
WORLD = ROOT / 'code/client/audio/openal/AudioSystemWorld.inl'
SHARED = ROOT / 'code/client/audio/openal/AudioSystemShared.inl'


def block(source, head):
    start = source.index(head)
    return source[start:source.index('\n};', start) + 3]


def prelude():
    world = WORLD.read_text(encoding='utf-8')
    shared = SHARED.read_text(encoding='utf-8')
    constants = shared[shared.index('constexpr float kQ3SoundFullVolumeDistance'):
                       shared.index('constexpr int kMaxStreamChunksPerService')]
    helpers = '\n'.join(function(shared, signature) for signature in (
        'static int ClampInt(', 'static float ClampFloat(', 'static float SmoothStep(',
        'static float MoveFloatTowards(', 'static float AudibilityHorizonDistance(',
        'static float HorizonGainAt(', 'static float HorizonGain(', 'static float DistanceBetweenPoints('))
    filters = '\n'.join([block(shared, 'enum class AudioFilterKind {'), block(shared, 'struct AudioFilterSettings {')] +
                        [function(shared, signature) for signature in (
                            'static AudioFilterSettings NoFilter(', 'static AudioFilterSettings LowPassFilter(',
                            'static AudioFilterSettings HighPassFilter(', 'static AudioFilterSettings BandPassFilter(',
                            'static const char *AudioFilterKindName(')])
    tones = '\n'.join([block(world, 'struct VoiceToneSettings {')] + [function(world, signature) for signature in (
        'static tone::SoundToneClass ToneClassForChannel(', 'static AudioFilterSettings ToneFilterFromBands(',
        'static VoiceToneSettings BuildVoiceToneSettings(', 'static void ResetVoiceOcclusion(',
        'static void SmoothVoiceOcclusion(')] + [block(world, 'struct SpatialParams {'),
        function(world, 'static bool IsLocalOnlyChannel(')])
    return r'''
#include "code/client/audio/shared/RTCWLoops.h"
#include "code/client/audio/shared/RTCWMix.h"
#include "code/client/audio/shared/STVEFAmbient.h"
#include "code/client/audio/shared/AudioOcclusion.h"
#include "code/client/audio/shared/AudioTonePolicy.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
namespace occ = fnql_audio_occlusion;
namespace tone = fnql_audio_tone;
enum qboolean { qfalse, qtrue };
using vec3_t = float[3];
using ALint = int;
constexpr int CHAN_AUTO = 0, CHAN_LOCAL = 1, CHAN_WEAPON = 2, CHAN_VOICE = 3, CHAN_ITEM = 4, CHAN_BODY = 5,
	CHAN_LOCAL_SOUND = 6, CHAN_ANNOUNCER = 7, ENTITYNUM_NONE = 1023;
constexpr int AL_PITCH = 1, AL_SOURCE_RELATIVE = 2, AL_POSITION = 3, AL_VELOCITY = 4, AL_SOURCE_STATE = 5,
	AL_PLAYING = 6, AL_BUFFER = 7, AL_LOOPING = 8, AL_TRUE = 1, AL_FALSE = 0;
constexpr int kDefaultStreamChannels = 2;
#define VectorSubtract( a, b, c ) ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
static float VectorLength( const float *v ) { return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ); }
''' + constants + helpers + filters + r'''
struct cvar_t { int integer = 0; float value = 0.0f; };
static cvar_t *s_alSpatializeStereo = nullptr, *s_pvs = nullptr, *s_volume = nullptr;
static bool inPVS = true, singlePlayer = false, eliteForce = false;
static bool S_OriginInPVS( const float *, const float * ) { return inPVS; }
static bool Com_GameIsRTCWSP() { return singlePlayer; }
static bool Com_GameIsSTVEFSP() { return eliteForce; }
static float S_STVEFSeparation() { return 0.5f; }
static struct { float current = 1.0f; } masterFade;
static auto &S_RTCWMasterFade() { return masterFade; }
static int Sys_Milliseconds() { return 1000; }
static int generic = 0;
struct Vec3 { float v[3]{}; void Set( const float *s ) { if ( s ) std::memcpy( v, s, sizeof( v ) ); } const float *Data() const { return v; } };
struct SoundShaderSettings { bool enabled = false; float gainScale = 1, referenceScale = 1, rangeScale = 1, wetScale = 1, pitchScale = 1; };
struct SoundSample {
	int channels = 1; std::string name = "sound/world/probe.wav"; SoundShaderSettings shader;
	int Channels() const { return channels; } bool EncodedSoundField() const { return false; }
	const SoundShaderSettings &Shader() const { return shader; } const std::string &Name() const { return name; }
	unsigned Buffer() const { return 1; }
};
struct VoiceRouteCache {};
struct SoundVoice {
	bool active = true, alice = false, fixedOrigin = true, rtcwLoop = false, rtcwOneShot = false, stvefAmbient = false;
	bool doppler = false, looping = false, liquidBoundary = false, occlusionInitialized = false;
	// A sound the game's menus started (ui_interface_sound_tests runs it).
	bool interfaceSound = false;
	unsigned source = 1, directFilter = 0, sendFilter = 0;
	SoundSample *sample = nullptr; Vec3 origin, velocity;
	int entnum = 200, entchannel = CHAN_AUTO, stvefVolume = 0, lastOcclusionSmoothTime = 0, nextEnvironmentUpdateTime = 0;
	fntech3::audio::rtcw::Loop rtcwLoopData;
	// Medal of Honor's alias distances and pitch; none set for every other profile.
	float mohaaMinDist = 0, mohaaMaxDist = 0, mohaaPitch = 1;
	float volumeScale = 1, occlusion = 0, occlusionTarget = 0;
	VoiceRouteCache routeCache;
	float debugGain = 0, debugPan = 0, debugDryGain = 1, debugWetGain = 0, debugPitch = 1, debugDistance = 0;
	bool debugPositional = false, debugDirect = false;
	const char *debugToneClass = "neutral", *debugDirectTone = "none", *debugSendTone = "none";
	float debugDirectToneLF = 1, debugDirectToneHF = 1, debugSendToneLF = 1, debugSendToneHF = 1;
};
struct EnvironmentState { float baseWet = 0.5f, directLF = 1, directHF = 0.95f, wetLF = 1, wetHF = 1, occlusionMultiplier = 1; qboolean underwater = qfalse; };
''' + tones + r'''
static SpatialParams ComputeSpatialParams( const float *, const float *, const float ( * )[3], int, float, float ) { ++generic; return { 0.42f, 0.1f }; }
// Every call the world makes on its device, as the last voice left it.
struct Device {
	struct Caps { bool sourceSpatialize = true; } caps;
	bool distanceModel = true;
	int spatialize = -1, directChannels = -1, relative = -1, distanceCalls = 0, rangeCalls = 0;
	bool worldDistance = false; float referenceScale = 0, rangeScale = 0, reference = 0, maximum = 0;
	float pitch = 0, position[3]{}, velocity[3]{}, gain = -1, directGain = -1;
	AudioFilterSettings directTone, sendTone;
	const Caps &Capabilities() const { return caps; }
	bool UsesOpenALDistanceAttenuation() const { return distanceModel; }
	Device &AL() { return *this; }
	void alSourcef( unsigned, int what, float value ) { if ( what == AL_PITCH ) pitch = value; }
	void alSourcei( unsigned, int what, int value ) { if ( what == AL_SOURCE_RELATIVE ) relative = value; }
	void alSource3f( unsigned, int what, float x, float y, float z ) { float *out = what == AL_POSITION ? position : velocity; out[0] = x; out[1] = y; out[2] = z; }
	void alGetSourcei( unsigned, int, int *value ) { *value = AL_PLAYING; }
	void alSourcePlay( unsigned ) {}
	void SetSourceSpatialize( unsigned, bool on ) { spatialize = on; }
	void SetSourceDirectChannels( unsigned, bool on ) { directChannels = on; }
	void ConfigureSourceDistance( unsigned, bool world, float reference, float range ) { ++distanceCalls; worldDistance = world; referenceScale = reference; rangeScale = range; }
	void ConfigureSourceDistanceRange( unsigned, float full, float silent ) { ++rangeCalls; reference = full; maximum = silent; }
	void ApplyVoiceRouting( unsigned, unsigned, unsigned, float g, float d, const AudioFilterSettings &dt, const AudioFilterSettings &st, VoiceRouteCache & ) {
		gain = g; directGain = d; directTone = dt; sendTone = st;
	}
};
static float measuredOcclusion = 0.0f;
static int traces = 0, stvefHandOffs = 0;
struct Q3SoundWorld {
	Device device; Device *device_ = &device;
	std::array<Vec3, 1024> entityOrigins_; int listenerNumber_ = 0; Vec3 listenerOrigin_;
	float listenerAxis_[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	EnvironmentState environment_;
	void ApplyAliceVoice( SoundVoice &, qboolean ) { assert( false ); }
	// Where Elite Force single player's voices may leave for a path of their own.
	void ApplySTVEFVoice( SoundVoice &, qboolean ) { ++stvefHandOffs; }
	void UpdateVoiceEnvironment( SoundVoice &voice, const float *origin );
	void ApplyVoice( SoundVoice &voice, qboolean softMuted );
};
// The traced occlusion, settled at once, so the probe reads what reaches the voice.
void Q3SoundWorld::UpdateVoiceEnvironment( SoundVoice &voice, const float * ) {
	++traces;
	ResetVoiceOcclusion( voice, measuredOcclusion, Sys_Milliseconds() );
}
''' + function(world, 'void Q3SoundWorld::ApplyVoice(') + r'''
static Q3SoundWorld world;
static SoundSample mono;
static bool near( float a, float b ) { return std::fabs( a - b ) < 1e-5f; }
static bool same( const char *a, const char *b ) { return std::strcmp( a, b ) == 0; }
// A one-shot started as StartRTCWSound or StartWolfETSound starts it.
static SoundVoice shot( int entity, int channel, float x, float y, float z, float volume = 1.0f ) {
	SoundVoice voice; voice.rtcwOneShot = true; voice.sample = &mono; voice.entnum = entity; voice.entchannel = channel;
	const float at[3] = { x, y, z }; voice.origin.Set( at ); voice.volumeScale = volume;
	return voice;
}
// A loop submitted as AddRTCWLoopingSound or AddWolfETLoopingSound submits it.
static SoundVoice loop( float x, float y, float z, int range, int volume, float master = 1.0f ) {
	fntech3::audio::rtcw::LoopFrame frame;
	const float at[3] = { x, y, z }, velocity[3] = { 300, 0, 0 };
	const int slot = frame.Append( at, velocity, range, 1, volume, master );
	assert( slot == 0 );
	SoundVoice voice; voice.rtcwLoop = true; voice.looping = true; voice.sample = &mono; voice.entnum = ENTITYNUM_NONE;
	voice.rtcwLoopData = frame[0]; voice.origin.Set( at ); voice.velocity.Set( velocity );
	return voice;
}
static void apply( SoundVoice &voice, bool muted = false ) {
	const bool distanceModel = world.device.distanceModel;
	world.device = Device{}; world.device.distanceModel = distanceModel;
	world.ApplyVoice( voice, muted ? qtrue : qfalse );
}
'''


CHECKS = {
    'one_shots': r'''
int main() {
	for ( bool sp : { false, true } ) {
		singlePlayer = sp; traces = 0; measuredOcclusion = 0.3f;
		// A weapon heard 500 units off: placed in the world through the device's
		// own model across retail's 80-to-1,330-unit span, traced, sent to the room.
		SoundVoice voice = shot( 200, CHAN_WEAPON, 500, 0, 0 );
		apply( voice );
		const Device &d = world.device;
		assert( d.spatialize == 1 && d.directChannels == 0 && d.relative == AL_FALSE );
		assert( d.rangeCalls == 1 && d.distanceCalls == 0 && d.reference == 80.0f && d.maximum == 1330.0f );
		assert( d.position[0] == 500 && d.position[1] == 0 && d.velocity[0] == 0 );
		assert( d.gain == 1.0f && voice.debugPan == 0.0f && voice.debugPositional );
		assert( traces == 1 && near( voice.occlusion, 0.3f ) && near( d.directGain, occ::DirectGain( 0.3f ) ) );
		const float wet = ClampFloat( occ::WetGain( 0.5f, 500.0f / 512.0f, 0.3f ), 0.0f, 2.0f );
		assert( wet > 0.0f && near( voice.debugWetGain, wet ) && d.sendTone.kind != AudioFilterKind::None );
		assert( same( voice.debugToneClass, "weapon" ) && !generic );
		// The channels RTCW and ET share with Quake III choose the tone class.
		const struct { int channel; const char *name; } classes[] = {
			{ CHAN_VOICE, "voice" }, { CHAN_ITEM, "item" }, { CHAN_BODY, "body" }, { CHAN_AUTO, "world" } };
		for ( const auto &expected : classes ) {
			SoundVoice classed = shot( 200, expected.channel, 500, 0, 0 );
			apply( classed );
			assert( same( classed.debugToneClass, expected.name ) && world.device.spatialize == 1 );
		}
		// Retail attenuates a local channel away from the listener by where it is.
		for ( int channel : { CHAN_LOCAL, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER } ) {
			SoundVoice local = shot( 200, channel, 500, 0, 0 );
			apply( local );
			assert( world.device.spatialize == 1 && world.device.rangeCalls == 1 && local.debugWetGain > 0.0f );
		}
		// Faded out toward the law's silent end, and nothing traced past it.
		SoundVoice far = shot( 200, CHAN_WEAPON, 1200, 0, 0 );
		apply( far );
		assert( near( world.device.gain, HorizonGainAt( 1200.0f, 1330.0f ) ) && world.device.gain > 0.0f && world.device.gain < 1.0f );
		traces = 0;
		SoundVoice silent = shot( 200, CHAN_WEAPON, 1330, 0, 0 );
		apply( silent );
		assert( world.device.gain == 0.0f && traces == 0 && silent.occlusion == 0.0f );
		// Enemy Territory's submitted volume, where 127 is unchanged.
		SoundVoice et = shot( 200, CHAN_WEAPON, 500, 0, 0, 64.0f / 127.0f );
		apply( et );
		volatile float expectedGain = 64.0f / 127.0f;
		assert( world.device.gain == expectedGain );
		// s_pvs and the focus mute silence the direct and wet paths alike.
		cvar_t pvs; pvs.integer = 1; s_pvs = &pvs; inPVS = false;
		SoundVoice culled = shot( 200, CHAN_WEAPON, 500, 0, 0 );
		apply( culled );
		assert( world.device.gain == 0.0f && culled.debugWetGain == 0.0f );
		s_pvs = nullptr; inPVS = true;
		SoundVoice muted = shot( 200, CHAN_WEAPON, 500, 0, 0 );
		apply( muted, true );
		assert( world.device.gain == 0.0f && muted.debugWetGain == 0.0f );
	}
}
''',
    'listener': r'''
int main() {
	measuredOcclusion = 0.9f; traces = 0;
	// The listener's own voice: head-relative at full volume, in the room, never occluded.
	SoundVoice own = shot( 0, CHAN_VOICE, 900, 0, 0 );
	apply( own );
	const Device &d = world.device;
	assert( d.spatialize == 0 && d.relative == AL_TRUE && d.distanceCalls == 1 && !d.worldDistance && d.rangeCalls == 0 );
	assert( d.gain == 1.0f && d.position[0] == 0.0f && traces == 0 && own.occlusion == 0.0f );
	assert( own.debugWetGain > 0.0f && same( own.debugToneClass, "voice" ) );
	// A local channel at the listener is an interface sound: dry and head-relative.
	for ( int channel : { CHAN_LOCAL, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER } ) {
		SoundVoice local = shot( 0, channel, 0, 0, 0 );
		apply( local );
		assert( world.device.spatialize == 0 && world.device.relative == AL_TRUE && world.device.gain == 1.0f );
		assert( local.debugWetGain == 0.0f && world.device.sendTone.kind == AudioFilterKind::None );
		assert( same( local.debugToneClass, channel == CHAN_ANNOUNCER ? "announcer" : "local" ) );
	}
	assert( traces == 0 );
}
''',
    'loops': r'''
int main() {
	for ( bool sp : { false, true } ) {
		singlePlayer = sp; measuredOcclusion = 0.2f; traces = 0;
		// A submitted range sets the span, and the loop plays at its retail level:
		// 255 over 256 of the loops' 90, against a one-shot's 127.
		SoundVoice voice = loop( 100, 0, 0, 321, 255 );
		apply( voice );
		const Device &d = world.device;
		assert( d.spatialize == 1 && d.relative == AL_FALSE && d.rangeCalls == 1 );
		volatile float expectedReference = 321.0f * 0.064f, expectedMaximum = expectedReference + 321.0f;
		volatile float expectedGain = 89.0f / 127.0f;
		assert( d.reference == expectedReference && d.maximum == expectedMaximum );
		assert( fntech3::audio::rtcw::LoopLevel( voice.rtcwLoopData ) == 89 && d.gain == expectedGain );
		assert( traces == 1 && near( voice.occlusion, 0.2f ) && voice.debugWetGain > 0.0f );
		assert( same( voice.debugToneClass, "world" ) && voice.debugPan == 0.0f );
		// Retail disables loop Doppler, whatever the submitted velocity.
		assert( d.velocity[0] == 0.0f && !voice.doppler );
		// Range 0 is retail's 1,250.
		SoundVoice fallback = loop( 100, 0, 0, 0, 255 );
		apply( fallback );
		assert( world.device.reference == 80.0f && world.device.maximum == 1330.0f );
		// A master fade above one amplifies the level, capped as retail caps a channel.
		SoundVoice loud = loop( 100, 0, 0, 0, 255, 4.0f );
		apply( loud );
		assert( fntech3::audio::rtcw::LoopLevel( loud.rtcwLoopData ) == 255 );
		volatile float expectedLevel = sp ? 255.0f / 127.0f : 2.0f;
		assert( world.device.gain == expectedLevel );
	}
}
''',
    'no_distance_model': r'''
int main() {
	world.device.distanceModel = false;
	// Without a distance model the law's own gain plays, without its pan; the
	// device still places the voice.
	SoundVoice voice = shot( 200, CHAN_WEAPON, 0, 100, 0 );
	apply( voice );
	const Device &d = world.device;
	assert( d.spatialize == 1 && d.relative == AL_FALSE && d.rangeCalls == 0 && d.distanceCalls == 1 && d.worldDistance );
	volatile float expectedGain = 124.0f / 127.0f;
	assert( d.gain == expectedGain && voice.debugPan == 0.0f && d.position[1] == 100.0f );
	SoundVoice looped = loop( 0, 100, 0, 0, 255 );
	apply( looped );
	volatile float expectedLoopGain = 87.0f / 127.0f;
	assert( world.device.gain == expectedLoopGain );
	SoundVoice silent = shot( 200, CHAN_WEAPON, 1400, 0, 0 );
	apply( silent );
	assert( world.device.gain == 0.0f );
	world.device.distanceModel = true;
}
''',
    'single_player_fade': r'''
int main() {
	singlePlayer = true;
	cvar_t volume; volume.value = 0.5f; s_volume = &volume; masterFade.current = 0.5f;
	// SP's quantized effects stage: int( int( 0.5 * 256 ) * 0.5 ) = 64 of 256.
	SoundVoice voice = shot( 200, CHAN_WEAPON, 500, 0, 0 );
	apply( voice );
	assert( world.device.gain == 0.25f && world.device.spatialize == 1 );
	singlePlayer = false;
	apply( voice );
	assert( world.device.gain == 1.0f );
	s_volume = nullptr; masterFade.current = 1.0f;
}
''',
    'other_profiles': r'''
int main() {
	// A Quake III world sound keeps its own distances and fade.
	SoundVoice q3; q3.sample = &mono; const float at[3] = { 500, 0, 0 }; q3.origin.Set( at ); q3.entchannel = CHAN_WEAPON;
	apply( q3 );
	assert( world.device.spatialize == 1 && world.device.rangeCalls == 0 && world.device.distanceCalls == 1 );
	assert( world.device.worldDistance && world.device.referenceScale == 1.0f && world.device.gain == 1.0f && generic == 1 );
	assert( world.device.rangeScale == 1.0f && world.device.pitch == 1.0f );
	// A Medal of Honor voice carries its alias's distances and pitch into the
	// device's reference and range scales: full volume inside its min distance,
	// the device's rolloff out to its max.
	SoundVoice moh; moh.sample = &mono; moh.origin.Set( at ); moh.entchannel = CHAN_WEAPON;
	moh.mohaaMinDist = 2.0f * kOpenALReferenceDistance; moh.mohaaMaxDist = 3.0f * kOpenALMaxDistance; moh.mohaaPitch = 1.25f;
	apply( moh );
	assert( world.device.spatialize == 1 && world.device.distanceCalls == 1 && world.device.rangeCalls == 0 && world.device.worldDistance );
	assert( near( world.device.referenceScale, 2.0f ) && near( world.device.rangeScale, 3.0f ) && world.device.pitch == 1.25f );
	// A max distance no farther than its min keeps the engine's range, and the
	// pitch stays within the voices' clamp.
	moh.mohaaMaxDist = moh.mohaaMinDist; moh.mohaaPitch = 4.0f;
	apply( moh );
	assert( near( world.device.referenceScale, 2.0f ) && world.device.rangeScale == 1.0f && world.device.pitch == 2.0f );
	assert( generic == 3 );
	// Elite Force single player's ambient loops are not Wolfenstein's: they keep
	// retail's head-relative pan here, or leave for a path of their own.
	eliteForce = true;
	SoundVoice ambient; ambient.stvefAmbient = true; ambient.looping = true; ambient.sample = &mono; ambient.entnum = ENTITYNUM_NONE;
	ambient.stvefVolume = 127; ambient.origin.Set( at );
	apply( ambient );
	if ( stvefHandOffs ) {
		assert( stvefHandOffs == 1 && world.device.gain == -1.0f );
	} else {
		assert( world.device.spatialize == 0 && world.device.relative == AL_TRUE && world.device.rangeCalls == 0 );
		assert( ambient.debugWetGain == 0.0f && same( ambient.debugToneClass, "neutral" ) );
	}
	eliteForce = false;
}
''',
}


class WolfensteinOpenALVoiceTests(unittest.TestCase):
    def compile_run(self, main):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        flags = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='wolf-voices-', dir=ROOT / '.tmp') as temporary:
            source = Path(temporary) / 'probe.cpp'
            executable = Path(temporary) / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(prelude() + main, encoding='utf-8')
            built = subprocess.run([compiler, '-std=c++17', '-fno-fast-math', *flags, '-I', str(ROOT), str(source),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_one_shots_are_placed_traced_and_sent_to_the_room(self):
        self.compile_run(CHECKS['one_shots'])

    def test_listener_and_interface_sounds(self):
        self.compile_run(CHECKS['listener'])

    def test_loops_take_their_range_and_retail_level(self):
        self.compile_run(CHECKS['loops'])

    def test_a_device_without_a_distance_model_plays_the_law(self):
        self.compile_run(CHECKS['no_distance_model'])

    def test_single_player_effects_fade_still_applies(self):
        self.compile_run(CHECKS['single_player_fade'])

    def test_other_profiles_keep_their_paths(self):
        self.compile_run(CHECKS['other_profiles'])

    def test_no_wolfenstein_voice_is_panned_as_retail_pans_it(self):
        world = WORLD.read_text(encoding='utf-8')
        apply = function(world, 'void Q3SoundWorld::ApplyVoice(')
        self.assertIn('const bool wolfenstein = voice.rtcwLoop || voice.rtcwOneShot;', apply)
        self.assertNotIn('rtcwSpatial', world)
        self.assertIn('device_->ConfigureSourceDistanceRange( voice.source, wolfensteinSpan.full, wolfensteinSpan.silent );', apply)
        # The legacy mixer stays retail's exact gain and pan.
        self.assertIn("Retail's\nexact gain and pan remain the legacy mixer's (s_backend legacy).", world)


if __name__ == '__main__':
    unittest.main()
