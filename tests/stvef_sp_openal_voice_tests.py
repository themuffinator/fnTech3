"""Execute the OpenAL world's rendering of Elite Force single player's voices.

shared/STVEFAmbient.h keeps retail's data: each channel's full-volume and
silent distances (stvoy.exe 0x420510), the volume an ambient set has faded
to, and the stereo separation. The OpenAL world renders that data as it renders
every profile's sounds (AGENTS.md invariant 11): positional, the device's
distance model across each channel's span, traced occlusion, the room's sends,
the tone classes, and the separation as a spread. These probes compile the
real `Q3SoundWorld::ApplySTVEFVoice` and the helpers it calls against a
recording device.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
WORLD = ROOT / 'code/client/audio/openal/AudioSystemWorld.inl'
SHARED = ROOT / 'code/client/audio/openal/AudioSystemShared.inl'
DEVICE = ROOT / 'code/client/audio/openal/AudioSystemOpenAL.inl'


def read(path):
    return path.read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def block(source, head):
    start = source.index(head)
    return source[start:source.index('\n};', start) + 3]


def prelude():
    world = read(WORLD)
    shared = read(SHARED)
    constants = shared[shared.index('constexpr float kQ3SoundFullVolumeDistance'):
                       shared.index('constexpr int kMaxStreamChunksPerService')]
    helpers = '\n'.join(function(shared, signature) for signature in (
        'static int ClampInt(', 'static float ClampFloat(', 'static float SmoothStep(',
        'static float MoveFloatTowards(', 'static float HorizonGainAt(', 'static float DistanceBetweenPoints('))
    filters = '\n'.join([block(shared, 'enum class AudioFilterKind {'), block(shared, 'struct AudioFilterSettings {')] +
                        [function(shared, signature) for signature in (
                            'static AudioFilterSettings NoFilter(', 'static AudioFilterSettings LowPassFilter(',
                            'static AudioFilterSettings HighPassFilter(', 'static AudioFilterSettings BandPassFilter(',
                            'static const char *AudioFilterKindName(')])
    tones = '\n'.join([block(world, 'struct VoiceToneSettings {')] + [function(world, signature) for signature in (
        'static tone::SoundToneClass ToneClassForChannel(', 'static AudioFilterSettings ToneFilterFromBands(',
        'static VoiceToneSettings BuildVoiceToneSettings(', 'static void ResetVoiceOcclusion(',
        'static void SmoothVoiceOcclusion(', 'static bool IsLocalOnlyChannel(')])
    stvef = '\n'.join(function(world, signature) for signature in (
        'static fntech3::audio::stvef::Span STVEFVoiceSpan(', 'static bool STVEFHeardAtListener(',
        'static bool STVEFLoopSilent('))
    return r'''
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
namespace ef = fntech3::audio::stvef;
enum qboolean { qfalse, qtrue };
using vec3_t = float[3];
using ALint = int;
constexpr int CHAN_AUTO = 0, CHAN_LOCAL = 1, CHAN_WEAPON = 2, CHAN_VOICE = 3, CHAN_ITEM = 4, CHAN_BODY = 5,
	CHAN_LOCAL_SOUND = 6, CHAN_ANNOUNCER = 7, ENTITYNUM_NONE = 1023;
constexpr int AL_PITCH = 1, AL_SOURCE_RELATIVE = 2, AL_POSITION = 3, AL_VELOCITY = 4, AL_SOURCE_STATE = 5,
	AL_PLAYING = 6, AL_BUFFER = 7, AL_LOOPING = 8, AL_TRUE = 1, AL_FALSE = 0, AL_STOPPED = 9;
constexpr int kDefaultStreamChannels = 2;
#define VectorSubtract( a, b, c ) ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
static float VectorLength( const float *v ) { return std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] ); }
''' + constants + helpers + filters + r'''
struct cvar_t { int integer = 0; float value = 0.0f; };
static cvar_t *s_alSpatializeStereo = nullptr, *s_pvs = nullptr;
static bool inPVS = true;
static float separation = 0.5f;
static bool S_OriginInPVS( const float *, const float * ) { return inPVS; }
static float S_STVEFSeparation() { return separation; }
static int Sys_Milliseconds() { return 1000; }
struct Vec3 { float v[3]{}; void Set( const float *s ) { if ( s ) std::memcpy( v, s, sizeof( v ) ); } const float *Data() const { return v; } };
struct SoundSample {
	int channels = 1; std::string name = "sound/ambience/probe.wav";
	int Channels() const { return channels; } bool EncodedSoundField() const { return false; }
	const std::string &Name() const { return name; } unsigned Buffer() const { return 1; }
};
struct VoiceRouteCache {};
struct SoundVoice {
	bool active = true, fixedOrigin = true, stvefAmbient = false, doppler = false, looping = false, occlusionInitialized = false;
	// A sound the game's menus started (ui_interface_sound_tests runs it).
	bool interfaceSound = false;
	unsigned source = 1, directFilter = 0, sendFilter = 0;
	SoundSample *sample = nullptr; Vec3 origin, velocity;
	int entnum = 200, entchannel = CHAN_AUTO, stvefVolume = 0, lastOcclusionSmoothTime = 0;
	float volumeScale = 1, occlusion = 0, occlusionTarget = 0;
	VoiceRouteCache routeCache;
	float debugGain = 0, debugPan = 0, debugDryGain = 1, debugWetGain = 0, debugPitch = 1, debugDistance = 0;
	bool debugPositional = false, debugDirect = false;
	const char *debugToneClass = "neutral", *debugDirectTone = "none", *debugSendTone = "none";
	float debugDirectToneLF = 1, debugDirectToneHF = 1, debugSendToneLF = 1, debugSendToneHF = 1;
};
struct EnvironmentState { float baseWet = 0.5f, directLF = 1, directHF = 0.95f, wetLF = 1, wetHF = 1; qboolean underwater = qfalse; };
''' + tones + stvef + r'''
// Every call the world makes on its device, as the last voice left it.
struct Device {
	struct Caps { bool sourceSpatialize = true; } caps;
	bool distanceModel = true;
	int spatialize = -1, directChannels = -1, relative = -1, distanceCalls = 0, rangeCalls = 0, radiusCalls = 0, plays = 0;
	bool worldDistance = true; float reference = 0, maximum = 0, radius = -1;
	float pitch = 0, position[3]{}, velocity[3]{}, gain = -1, directGain = -1;
	int state = AL_PLAYING;
	AudioFilterSettings directTone, sendTone;
	const Caps &Capabilities() const { return caps; }
	bool UsesOpenALDistanceAttenuation() const { return distanceModel; }
	Device &AL() { return *this; }
	void alSourcef( unsigned, int what, float value ) { if ( what == AL_PITCH ) pitch = value; }
	void alSourcei( unsigned, int what, int value ) { if ( what == AL_SOURCE_RELATIVE ) relative = value; }
	void alSource3f( unsigned, int what, float x, float y, float z ) { float *out = what == AL_POSITION ? position : velocity; out[0] = x; out[1] = y; out[2] = z; }
	void alGetSourcei( unsigned, int, int *value ) { *value = state; }
	void alSourcePlay( unsigned ) { ++plays; }
	void SetSourceSpatialize( unsigned, bool on ) { spatialize = on; }
	void SetSourceDirectChannels( unsigned, bool on ) { directChannels = on; }
	void SetSourceRadius( unsigned, float r ) { ++radiusCalls; radius = r; }
	void ConfigureSourceDistance( unsigned, bool world, float, float ) { ++distanceCalls; worldDistance = world; }
	void ConfigureSourceDistanceRange( unsigned, float full, float silent ) { ++rangeCalls; reference = full; maximum = silent; }
	void ApplyVoiceRouting( unsigned, unsigned, unsigned, float g, float d, const AudioFilterSettings &dt, const AudioFilterSettings &st, VoiceRouteCache & ) {
		gain = g; directGain = d; directTone = dt; sendTone = st;
	}
};
static float measuredOcclusion = 0.0f;
static int traces = 0;
struct Q3SoundWorld {
	Device device; Device *device_ = &device;
	std::array<Vec3, 1024> entityOrigins_; int listenerNumber_ = 0; Vec3 listenerOrigin_;
	float listenerAxis_[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	EnvironmentState environment_;
	void UpdateVoiceEnvironment( SoundVoice &voice, const float *origin );
	void ApplySTVEFVoice( SoundVoice &voice, qboolean softMuted );
};
// The traced occlusion, settled at once, so the probe reads what reaches the voice.
void Q3SoundWorld::UpdateVoiceEnvironment( SoundVoice &voice, const float * ) {
	++traces;
	ResetVoiceOcclusion( voice, measuredOcclusion, Sys_Milliseconds() );
}
''' + function(world, 'void Q3SoundWorld::ApplySTVEFVoice(') + r'''
static Q3SoundWorld world;
static SoundSample mono;
static bool near( float a, float b, float tolerance = 1e-4f ) { return std::fabs( a - b ) < tolerance; }
static bool same( const char *a, const char *b ) { return std::strcmp( a, b ) == 0; }
constexpr float pi = 3.14159265358979f;
// A one-shot as StartSound starts it, `distance` ahead of the listener.
static SoundVoice shot( int entity, int channel, float distance, float volume = 1.0f ) {
	SoundVoice voice; voice.sample = &mono; voice.entnum = entity; voice.entchannel = channel; voice.volumeScale = volume;
	const float at[3] = { distance, 0, 0 }; voice.origin.Set( at );
	return voice;
}
// An ambient set's loop as AddSTVEFAmbientLoopingSound submits it.
static SoundVoice ambient( float distance, int volume ) {
	SoundVoice voice; voice.stvefAmbient = true; voice.looping = true; voice.sample = &mono;
	voice.entnum = ENTITYNUM_NONE; voice.stvefVolume = volume;
	const float at[3] = { distance, 0, 0 }; voice.origin.Set( at );
	return voice;
}
static void apply( SoundVoice &voice, bool muted = false ) {
	const bool distanceModel = world.device.distanceModel;
	world.device = Device{}; world.device.distanceModel = distanceModel;
	world.ApplySTVEFVoice( voice, muted ? qtrue : qfalse );
}
'''


CHECKS = {
    'spans': r'''
int main() {
	measuredOcclusion = 0.3f;
	// A weapon 500 units off: placed in the world through the device's own
	// model across the 256-to-1,506-unit span, traced, sent to the room.
	SoundVoice weapon = shot( 200, CHAN_WEAPON, 500 );
	apply( weapon );
	const Device &d = world.device;
	assert( d.spatialize == 1 && d.directChannels == 0 && d.relative == AL_FALSE );
	assert( d.rangeCalls == 1 && d.distanceCalls == 0 && d.reference == 256.0f && near( d.maximum, 1506.0f, 1e-2f ) );
	assert( d.position[0] == 500 && d.gain == 1.0f && d.pitch == 1.0f && weapon.debugPositional && weapon.debugPan == 0.0f );
	assert( traces == 1 && near( weapon.occlusion, 0.3f ) && near( d.directGain, occ::DirectGain( 0.3f ) ) );
	const float wet = ClampFloat( occ::WetGain( 0.5f, 500.0f / 512.0f, 0.3f ), 0.0f, 2.0f );
	assert( wet > 0.0f && near( weapon.debugWetGain, wet ) && d.sendTone.kind != AudioFilterKind::None );
	assert( same( weapon.debugToneClass, "weapon" ) && d.radius == 0.0f && d.radiusCalls == 1 );

	// A voice holds to 1,024 units and is silent by 2,274.
	SoundVoice voice = shot( 200, ef::VoiceChannel, 1500 );
	apply( voice );
	assert( d.reference == 1024.0f && near( d.maximum, 2274.0f, 1e-2f ) && d.gain == 1.0f );
	assert( same( voice.debugToneClass, "voice" ) );

	// The attenuated voice holds to 38.4 and is silent by 288.4; it is a
	// voice to the tone classes, and keeps its own channel number.
	SoundVoice whisper = shot( 200, ef::AttenuatedVoiceChannel, 100 );
	apply( whisper );
	assert( near( d.reference, 38.4f ) && near( d.maximum, 288.4f, 1e-2f ) );
	assert( same( whisper.debugToneClass, "voice" ) && whisper.entchannel == ef::AttenuatedVoiceChannel );

	// Every other channel carries as a sound with none.
	for ( int channel : { CHAN_AUTO, CHAN_ITEM, CHAN_BODY, 15 } ) {
		SoundVoice other = shot( 200, channel, 500 );
		apply( other );
		assert( d.reference == 256.0f && near( d.maximum, 1506.0f, 1e-2f ) );
	}

	// Away from the listener, retail measures a local, announcer or menu
	// channel by where it is.
	for ( int channel : { CHAN_LOCAL, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER, ef::MenuChannel, ef::SecondMenuChannel } ) {
		SoundVoice local = shot( 200, channel, 500 );
		apply( local );
		assert( d.spatialize == 1 && d.rangeCalls == 1 && local.debugWetGain > 0.0f );
	}

	// The sound the player's own entity held back keeps its volume.
	SoundVoice quieter = shot( 200, CHAN_WEAPON, 500, ef::ListenerScale );
	apply( quieter );
	assert( d.gain == 0.75f );

	// Faded out toward the silent end, and nothing traced past it.
	SoundVoice far = shot( 200, CHAN_WEAPON, 1400 );
	apply( far );
	assert( near( d.gain, HorizonGainAt( 1400.0f, 1506.0f ), 1e-3f ) && d.gain > 0.0f && d.gain < 1.0f );
	traces = 0;
	SoundVoice silent = shot( 200, CHAN_WEAPON, 1510 );
	apply( silent );
	assert( d.gain == 0.0f && traces == 0 && silent.occlusion == 0.0f );
	SoundVoice carried = shot( 200, ef::VoiceChannel, 2000 );
	apply( carried );
	assert( d.gain > 0.0f && traces == 1 );
}
''',
    'listener': r'''
int main() {
	measuredOcclusion = 0.9f; traces = 0;
	// The listener's own voice: head-relative, in the room, never occluded.
	SoundVoice own = shot( 0, CHAN_VOICE, 900, ef::ListenerScale );
	apply( own );
	const Device &d = world.device;
	assert( d.spatialize == 0 && d.relative == AL_TRUE && d.distanceCalls == 1 && !d.worldDistance && d.rangeCalls == 0 );
	assert( d.gain == 0.75f && d.position[0] == 0.0f && d.position[2] == -1.0f && traces == 0 && own.occlusion == 0.0f );
	assert( own.debugWetGain > 0.0f && same( own.debugToneClass, "voice" ) && d.radius == 0.0f );
	// A local, announcer or menu channel there is an interface sound: dry
	// and head-relative.
	for ( int channel : { CHAN_LOCAL, CHAN_LOCAL_SOUND, CHAN_ANNOUNCER, ef::MenuChannel, ef::SecondMenuChannel } ) {
		SoundVoice local = shot( 0, channel, 0 );
		apply( local );
		assert( d.spatialize == 0 && d.relative == AL_TRUE && d.gain == 1.0f );
		assert( local.debugWetGain == 0.0f && d.sendTone.kind == AudioFilterKind::None );
		assert( same( local.debugToneClass, channel == CHAN_ANNOUNCER ? "announcer" : "local" ) );
	}
	// An ambient set's sub-wave belongs to the player and plays at its volume.
	SoundVoice wave = shot( 0, 15, 0, 96.0f / 255.0f );
	apply( wave );
	assert( d.spatialize == 0 && near( d.gain, 96.0f / 255.0f ) && wave.debugWetGain > 0.0f && same( wave.debugToneClass, "self" ) );
	assert( traces == 0 );
}
''',
    'ambient': r'''
int main() {
	measuredOcclusion = 0.2f; traces = 0;
	// A loop around a point plays at its set's volume over a one-shot's 127,
	// placed and measured as a sound with no channel.
	SoundVoice loop = ambient( 500, 127 );
	apply( loop );
	const Device &d = world.device;
	assert( d.spatialize == 1 && d.relative == AL_FALSE && d.rangeCalls == 1 && d.reference == 256.0f );
	assert( d.gain == 1.0f && traces == 1 && near( loop.occlusion, 0.2f ) && loop.debugWetGain > 0.0f );
	assert( same( loop.debugToneClass, "world" ) && d.velocity[0] == 0.0f );
	SoundVoice full = ambient( 500, 255 );
	apply( full );
	assert( d.gain == 2.0f );
	// A stopped loop is started again.
	world.device.state = AL_STOPPED;
	world.ApplySTVEFVoice( full, qfalse );
	assert( world.device.plays == 1 );

	// The room tone is played where the listener is: in the room, never
	// occluded, at the share of a centred sound retail's separation leaves.
	traces = 0;
	SoundVoice room = ambient( 0, 255 );
	apply( room );
	assert( d.spatialize == 0 && d.relative == AL_TRUE && d.rangeCalls == 0 && traces == 0 && room.occlusion == 0.0f );
	assert( d.gain == 1.0f && room.debugWetGain > 0.0f && same( room.debugToneClass, "self" ) );
	SoundVoice fading = ambient( 0, 64 );
	apply( fading );
	volatile float expectedGain = 32.0f / 127.0f;
	assert( d.gain == expectedGain );
	separation = 1.0f;
	apply( room );
	assert( d.gain == 2.0f );
	separation = 0.0f;
	apply( room );
	assert( d.gain == 0.0f );
	separation = 0.5f;
}
''',
    'separation': r'''
int main() {
	// The default half is the engine's own spread: a point source.
	SoundVoice voice = shot( 200, CHAN_WEAPON, 500 );
	apply( voice );
	assert( world.device.radius == 0.0f );
	// Above it, the voice is spread so that the share of its direction a
	// first-order panner keeps is the share retail keeps between a side
	// sound's two channels, (1 - s) / s.
	for ( float s : { 0.6f, 0.75f, 0.9f, 1.0f } ) {
		separation = s;
		apply( voice );
		const float r = world.device.radius, distance = 500.0f;
		assert( r > 0.0f );
		// OpenAL Soft's own reading of a radius (alc/alu.cpp).
		const float spread = r > distance ? 2.0f * pi - distance / r * pi : 2.0f * std::asin( r / distance );
		assert( near( spread, ef::SeparationSpread( s ), 1e-3f ) );
		assert( near( ( 1.0f + std::cos( spread * 0.5f ) ) * 0.5f, ( 1.0f - s ) / s, 1e-3f ) );
	}
	// Below it nothing is narrower than a point, and a sound at the listener
	// has no size.
	separation = 0.2f;
	apply( voice );
	assert( world.device.radius == 0.0f );
	separation = 1.0f;
	SoundVoice own = shot( 0, CHAN_VOICE, 0 );
	apply( own );
	assert( world.device.radius == 0.0f && world.device.radiusCalls == 1 );
	separation = 0.5f;
	assert( ef::SeparationSpread( -1.0f ) == 0.0f && near( ef::SeparationSpread( 2.0f ), 2.0f * pi ) );
	assert( ef::SpreadRadius( pi, 0.0f ) == 0.0f && ef::SpreadRadius( 0.0f, 100.0f ) == 0.0f );
	assert( ef::SpreadRadius( pi, INFINITY ) == 0.0f && near( ef::SpreadRadius( pi, 100.0f ), 100.0f ) );
}
''',
    'no_distance_model': r'''
int main() {
	world.device.distanceModel = false;
	// Without a distance model the law's own gain plays, without its pan;
	// the device still places the voice.
	SoundVoice weapon = shot( 200, CHAN_WEAPON, 881 );
	apply( weapon );
	const Device &d = world.device;
	assert( d.spatialize == 1 && d.relative == AL_FALSE && d.rangeCalls == 0 && d.distanceCalls == 1 && !d.worldDistance );
	assert( near( d.gain, 0.5f, 1e-3f ) && weapon.debugPan == 0.0f && d.position[0] == 881.0f );
	SoundVoice voice = shot( 200, ef::VoiceChannel, 1649 );
	apply( voice );
	assert( near( d.gain, 0.5f, 1e-3f ) );
	SoundVoice silent = shot( 200, CHAN_WEAPON, 1506 );
	apply( silent );
	assert( d.gain == 0.0f );
	world.device.distanceModel = true;
	// A stereo sample away from the listener stays on its own channels,
	// head-relative, measured by the law and traced.
	SoundSample stereo; stereo.channels = 2;
	SoundVoice wide = shot( 200, CHAN_AUTO, 881 ); wide.sample = &stereo;
	traces = 0;
	apply( wide );
	assert( d.spatialize == 0 && d.directChannels == 1 && d.relative == AL_TRUE && d.rangeCalls == 0 );
	assert( near( d.gain, 0.5f, 1e-3f ) && traces == 1 && wide.debugDirect && same( wide.debugToneClass, "stereo" ) );
}
''',
    'culled_and_muted': r'''
int main() {
	// s_pvs and the focus mute silence the direct and wet paths alike.
	cvar_t pvs; pvs.integer = 1; s_pvs = &pvs; inPVS = false; traces = 0;
	SoundVoice culled = shot( 200, CHAN_WEAPON, 500 );
	apply( culled );
	assert( world.device.gain == 0.0f && culled.debugWetGain == 0.0f && traces == 0 );
	// The room tone is never culled.
	SoundVoice room = ambient( 0, 255 );
	apply( room );
	assert( world.device.gain == 1.0f );
	s_pvs = nullptr; inPVS = true;
	SoundVoice muted = shot( 200, CHAN_WEAPON, 500 );
	apply( muted, true );
	assert( world.device.gain == 0.0f && muted.debugWetGain == 0.0f );
}
''',
    'silent_loops': r'''
int main() {
	// A loop keeps its source until the law silences it.
	assert( !STVEFLoopSilent( ambient( 1505, 255 ), 1505.0f, 0 ) );
	assert( STVEFLoopSilent( ambient( 1507, 255 ), 1507.0f, 0 ) );
	assert( STVEFLoopSilent( ambient( 500, 0 ), 500.0f, 0 ) );
	assert( !STVEFLoopSilent( ambient( 0, 255 ), 0.0f, 0 ) );
	SoundVoice entity = shot( 200, CHAN_AUTO, 1600 ); entity.looping = true;
	assert( STVEFLoopSilent( entity, 1600.0f, 0 ) );
	entity.entchannel = ef::VoiceChannel;
	assert( !STVEFLoopSilent( entity, 2000.0f, 0 ) );
	entity.entnum = 0;
	assert( !STVEFLoopSilent( entity, 9000.0f, 0 ) );
}
''',
}


class STVEFSPOpenALVoiceTests(unittest.TestCase):
    def compile_run(self, main):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        flags = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-voices-', dir=ROOT / '.tmp') as temporary:
            source = Path(temporary) / 'probe.cpp'
            executable = Path(temporary) / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(prelude() + main, encoding='utf-8')
            built = subprocess.run([compiler, '-std=c++17', '-fno-fast-math', *flags, '-I', str(ROOT), str(source),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_each_channel_takes_its_span_through_the_distance_model(self):
        self.compile_run(CHECKS['spans'])

    def test_listener_and_interface_sounds(self):
        self.compile_run(CHECKS['listener'])

    def test_ambient_loops_and_the_room_tone(self):
        self.compile_run(CHECKS['ambient'])

    def test_separation_spreads_placed_voices(self):
        self.compile_run(CHECKS['separation'])

    def test_a_device_without_a_distance_model_plays_the_law(self):
        self.compile_run(CHECKS['no_distance_model'])

    def test_pvs_and_the_focus_mute(self):
        self.compile_run(CHECKS['culled_and_muted'])

    def test_loops_the_law_silences_keep_no_source(self):
        self.compile_run(CHECKS['silent_loops'])

    def test_every_single_player_voice_takes_this_path(self):
        world = read(WORLD)
        apply = function(world, 'void Q3SoundWorld::ApplyVoice( SoundVoice &voice, qboolean softMuted ) {')
        branch = 'if ( Com_GameIsSTVEFSP() ) {\n\t\tApplySTVEFVoice( voice, softMuted );\n\t\treturn;\n\t}'
        self.assertIn(branch, apply)
        # Before anything of the other profiles' paths is measured.
        self.assertLess(apply.index(branch), apply.index('const float *voiceOrigin ='))
        # Virtual loops go by the same law.
        update = function(world, 'void Q3SoundWorld::Update( qboolean softMuted ) {')
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {', update)
        self.assertIn('if ( STVEFLoopSilent( voice, DistanceBetweenPoints( voiceOrigin, listenerOrigin_.Data() ), '
                      'listenerNumber_ ) ) {', update)
        self.assertLess(update.index('if ( Com_GameIsSTVEFSP() ) {'), update.index('if ( voice.source == 0 ) {'))
        # A voice started far off is kept while its channel still carries.
        start = function(world, 'void Q3SoundWorld::StartSound( int entityNum,')
        self.assertIn('const float horizon = Com_GameIsSTVEFSP() ? fntech3::audio::stvef::ChannelSpan( entchannel ).silent :',
                      start)
        self.assertIn('> horizon * kOneShotCullMargin', start)
        # No Elite Force voice is spatialized as retail's mixer spatializes it.
        body = function(world, 'void Q3SoundWorld::ApplySTVEFVoice( SoundVoice &voice, qboolean softMuted ) {')
        self.assertNotIn('SpatializeMerged', body)
        self.assertIn('device_->ConfigureSourceDistanceRange( voice.source, span.full, span.silent );', body)
        self.assertIn('HorizonGainAt( distance, span.silent )', body)

    def test_the_device_can_size_a_source(self):
        device = read(DEVICE)
        self.assertIn('capabilities_.sourceRadius = HasALExtension( "AL_EXT_SOURCE_RADIUS" );', device)
        setter = function(device, 'void OpenALDevice::SetSourceRadius( ALuint source, float radius ) const {')
        self.assertIn('!capabilities_.sourceRadius', setter)
        self.assertIn('AL_SOURCE_RADIUS', setter)
        # A pooled source never carries one voice's size into another's.
        reset = function(device, 'void OpenALDevice::ResetSource( ALuint source ) const {')
        self.assertIn('SetSourceRadius( source, 0.0f );', reset)
        self.assertIn('#define AL_SOURCE_RADIUS 0x1031', read(ROOT / 'code/client/audio/openal/OpenALCompat.h'))


if __name__ == '__main__':
    unittest.main()
