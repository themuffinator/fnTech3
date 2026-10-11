"""A sound a game's menus start is interface audio, whatever channel it names.

Every retail menu module hands its sounds to the engine through a local-sound
import with a channel number. Most name a local channel, which Fn's channel
rules already play as the interface's, but Jedi Academy's `play` script
command names the automatic channel and Elite Force Holomatch's menus several
sounds on 8, a number past the announcer's, and both took the listener's own
diegetic class with a send into the room (2026-10-08 UI sound policy record).
Retail pins the channel and the level (AGENTS.md invariant 11); the
presentation is Fn's, and its tone policy calls menu audio non-diegetic.

These probes compile the real tone policy, `Q3SoundWorld::ApplyVoice` and
`ApplySTVEFVoice` (through the Wolfenstein and Elite Force voice fixtures'
recording worlds), `Q3SoundWorld::PlayOneShot` and the sound front end's
entries, and check that each profile's menu host, and no client game, starts
its sounds through those entries.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function
import stvef_sp_openal_voice_tests as stvef_voices
import wolfenstein_openal_voice_tests as wolfenstein_voices

ROOT = Path(__file__).resolve().parents[1]
WORLD = ROOT / 'code/client/audio/openal/AudioSystemWorld.inl'
BACKEND = ROOT / 'code/client/audio/openal/AudioSystemBackend.inl'
LEGACY = ROOT / 'code/client/audio/legacy/snd_dma.cpp'
MAIN = ROOT / 'code/client/audio/snd_main.cpp'
LOCAL = ROOT / 'code/client/audio/snd_local.h'


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def case(source, label, end='return'):
    """A switch case's statements, from its label to the end of the case."""
    start = source.index(label)
    return source[start:source.index(end, start + len(label))]


POLICY = r'''
#include "code/client/audio/shared/AudioTonePolicy.h"
#include <cassert>
#include <cstdio>
namespace tone = fnql_audio_tone;
using C = tone::SoundToneClass;
static C classify( const tone::ToneClassInputs &inputs ) { return tone::ClassifyToneClass( inputs ); }
int main() {
	// Every channel class, sample prefix, and placement a voice can bring.
	const C channelClasses[] = { C::Neutral, C::Weapon, C::Voice, C::Item, C::Body, C::Announcer };
	const char *names[] = { nullptr, "sound/weapons/rocket/rocklf1a.wav", "sound/feedback/hit.wav",
		"sound/player/footsteps/step1.wav", "sound/items/pickup.wav", "sound/interface/button1.wav" };
	int cases = 0;
	for ( C channelClass : channelClasses ) for ( const char *name : names ) for ( int bits = 0; bits < 16; ++bits ) {
		tone::ToneClassInputs inputs;
		inputs.channelClass = channelClass; inputs.sampleName = name;
		inputs.listenerAttached = bits & 1; inputs.localOnly = bits & 2;
		inputs.positionalSource = bits & 4; inputs.underwater = bits & 8;
		inputs.interfaceSound = true;
		// A menu's sound is the interface's; an announcer keeps its own class.
		const C expected = channelClass == C::Announcer ? C::Announcer : C::Local;
		assert( classify( inputs ) == expected && tone::ToneClassIsNonDiegetic( expected ) );
		// Authored stereo and surround keep the direct path they were mixed for.
		inputs.stereoSample = true;
		assert( classify( inputs ) == C::Stereo );
		inputs.multichannelSample = true;
		assert( classify( inputs ) == C::Multichannel );
		++cases;
	}
	assert( cases == 6 * 6 * 16 );
	// Without the mark nothing changes: the listener's own sound is still in
	// the level (the client game's sound on its automatic channel), a local
	// channel the interface's, and a channel's class wins over a local-only
	// placement, as an American McGee's Alice local sound on a weapon channel has.
	tone::ToneClassInputs own; own.listenerAttached = true;
	assert( classify( own ) == C::Self );
	tone::ToneClassInputs local = own; local.localOnly = true;
	assert( classify( local ) == C::Local );
	tone::ToneClassInputs weapon = local; weapon.channelClass = C::Weapon;
	assert( classify( weapon ) == C::Weapon );
	tone::ToneClassInputs announcer = local; announcer.channelClass = C::Announcer;
	assert( classify( announcer ) == C::Announcer );
	tone::ToneClassInputs feedback; feedback.sampleName = "sound/feedback/hit.wav"; feedback.listenerAttached = true;
	assert( classify( feedback ) == C::Local );
	tone::ToneClassInputs world; world.positionalSource = true;
	assert( classify( world ) == C::World );
	// The water and strong occlusion shape the listener's own sound, never a menu's.
	tone::ToneEnvironmentBands water; water.underwater = true; water.directHF = 0.5f; water.wetHF = 0.5f;
	for ( C menu : { C::Local, C::Announcer } ) {
		const tone::ToneBands bands = tone::ShapeToneBands( menu, water, 0.5f, 0.5f, 0.95f );
		assert( !bands.bandPass && bands.directHF == 1.0f );
	}
	assert( tone::ShapeToneBands( C::Self, water, 0.5f, 0.5f, 0.95f ).bandPass );
	std::puts( "policy passed" );
}
'''

APPLY_VOICE = r'''
static SoundVoice menu( int entity, int channel, float volume, bool wolfenstein ) {
	SoundVoice voice; voice.sample = &mono; voice.entnum = entity; voice.entchannel = channel;
	voice.fixedOrigin = false; voice.volumeScale = volume; voice.rtcwOneShot = wolfenstein; voice.interfaceSound = true;
	return voice;
}
int main() {
	world.listenerNumber_ = 0; measuredOcclusion = 0.9f; traces = 0;
	const float far[3] = { 900, 0, 0 }; world.entityOrigins_[0].Set( far );
	// The listener's own sound without the mark, as the client game starts it on
	// the automatic channel: in the level, with a send into the room.
	for ( bool wolfenstein : { false, true } ) {
		SoundVoice own = menu( 0, CHAN_AUTO, 1.0f, wolfenstein ); own.interfaceSound = false;
		apply( own );
		assert( same( own.debugToneClass, "self" ) && own.debugWetGain > 0.0f );
		assert( world.device.sendTone.kind != AudioFilterKind::None );
		// Holomatch's channel 8 is no local channel either.
		SoundVoice eight = menu( 0, 8, 1.0f, wolfenstein ); eight.interfaceSound = false;
		apply( eight );
		assert( same( eight.debugToneClass, "self" ) && eight.debugWetGain > 0.0f );
	}
	// The menus' sounds on every engine channel, Holomatch's 8, and the numbers
	// Academy's own channels take above the engine's: head-relative at their
	// level, dry, untraced, the interface's (the announcer its own).
	const int channels[] = { CHAN_AUTO, CHAN_LOCAL, CHAN_WEAPON, CHAN_VOICE, CHAN_ITEM, CHAN_BODY,
		CHAN_LOCAL_SOUND, CHAN_ANNOUNCER, 8, CHAN_ANNOUNCER + 1 + 7, CHAN_ANNOUNCER + 1 + 11, -1 };
	for ( bool wolfenstein : { false, true } ) for ( int channel : channels ) for ( float volume : { 1.0f, 0.75f } ) {
		SoundVoice voice = menu( 0, channel, volume, wolfenstein );
		apply( voice );
		const Device &d = world.device;
		assert( d.spatialize == 0 && d.relative == AL_TRUE && !voice.debugPositional && d.position[0] == 0.0f );
		assert( d.gain == volume && voice.debugPan == 0.0f && voice.occlusion == 0.0f );
		assert( voice.debugWetGain == 0.0f && d.sendTone.kind == AudioFilterKind::None );
		assert( same( voice.debugToneClass, channel == CHAN_ANNOUNCER ? "announcer" : "local" ) );
		assert( d.directTone.kind != AudioFilterKind::BandPass );
	}
	assert( traces == 0 );
	// A session that moves the listener to another entity while a menu's sound
	// plays leaves it at the listener, dry.
	world.listenerNumber_ = 5;
	SoundVoice stayed = menu( 0, CHAN_AUTO, 1.0f, false );
	apply( stayed );
	assert( world.device.spatialize == 0 && world.device.relative == AL_TRUE && world.device.gain == 1.0f );
	assert( stayed.debugWetGain == 0.0f && same( stayed.debugToneClass, "local" ) && traces == 0 );
	world.listenerNumber_ = 0;
	// Under water the listener's own voice is shaped by the medium; a menu's never.
	world.environment_.underwater = qtrue;
	SoundVoice body = menu( 0, CHAN_VOICE, 1.0f, false ); body.interfaceSound = false;
	apply( body );
	assert( same( body.debugToneClass, "voice" ) && world.device.directTone.kind == AudioFilterKind::BandPass );
	SoundVoice click = menu( 0, CHAN_VOICE, 1.0f, false );
	apply( click );
	assert( same( click.debugToneClass, "local" ) && world.device.directTone.kind == AudioFilterKind::HighPass );
	world.environment_.underwater = qfalse;
	// Authored stereo keeps its direct path, still unsent.
	SoundSample wide; wide.channels = 2;
	SoundVoice stereo = menu( 0, CHAN_AUTO, 1.0f, false ); stereo.sample = &wide;
	apply( stereo );
	assert( same( stereo.debugToneClass, "stereo" ) && world.device.directChannels == 1 );
	assert( stereo.debugWetGain == 0.0f && world.device.sendTone.kind == AudioFilterKind::None );
	// The focus mute silences a menu's sound as it does any other.
	SoundVoice muted = menu( 0, CHAN_AUTO, 1.0f, false );
	apply( muted, true );
	assert( world.device.gain == 0.0f );
	std::puts( "apply passed" );
}
'''

APPLY_STVEF_VOICE = r'''
int main() {
	world.listenerNumber_ = 0; measuredOcclusion = 0.9f; traces = 0;
	// Single player's own sound at the listener: in the level, with the room.
	SoundVoice own = shot( 0, CHAN_AUTO, 0 );
	apply( own );
	assert( same( own.debugToneClass, "self" ) && own.debugWetGain > 0.0f );
	// Its menus' sounds, on the channels its menus use and on any other.
	for ( int channel : { CHAN_AUTO, CHAN_LOCAL, CHAN_WEAPON, CHAN_LOCAL_SOUND, ef::AttenuatedVoiceChannel,
		ef::MenuChannel, ef::SecondMenuChannel } ) {
		SoundVoice voice = shot( 0, channel, 0 ); voice.interfaceSound = true;
		apply( voice );
		assert( world.device.spatialize == 0 && world.device.relative == AL_TRUE && world.device.gain == 1.0f );
		assert( voice.debugWetGain == 0.0f && world.device.sendTone.kind == AudioFilterKind::None );
		assert( same( voice.debugToneClass, "local" ) && voice.entchannel == channel );
	}
	// Started on an entity the listener has since left, it is still heard at
	// the listener: not placed, not measured by distance, not traced.
	SoundVoice moved = shot( 7, CHAN_AUTO, 2000 ); moved.interfaceSound = true;
	apply( moved );
	assert( world.device.spatialize == 0 && world.device.relative == AL_TRUE && world.device.gain == 1.0f );
	assert( !moved.debugPositional && moved.debugWetGain == 0.0f && traces == 0 );
	// A menu channel at the listener stays the interface's without the mark.
	SoundVoice menu = shot( 0, ef::MenuChannel, 0 );
	apply( menu );
	assert( same( menu.debugToneClass, "local" ) && menu.debugWetGain == 0.0f );
	std::puts( "stvef passed" );
}
'''

ONE_SHOT = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
enum qboolean { qfalse, qtrue };
typedef int sfxHandle_t;
typedef unsigned ALuint;
typedef int ALint;
typedef int ALenum;
constexpr int AL_BUFFER = 1, AL_LOOPING = 2, AL_FALSE = 0, AL_SOURCE_STATE = 3;
struct cvar_t { int integer = 0; };
static cvar_t *s_alSoundStartDebug = nullptr;
static int Sys_Milliseconds() { return 1000; }
static void Com_Printf( const char *, ... ) {}
// Academy's lip-sync marks, where the tree carries them; no voice channel here.
static bool Com_GameIsJKAMP() { return false; }
static bool S_JKAIsVoiceChannel( int ) { return false; }
static float ClampFloat( float value, float low, float high ) { return value < low ? low : value > high ? high : value; }
struct Vec3f { float v[3]{}; void Set( const float *s ) { if ( s ) { v[0] = s[0]; v[1] = s[1]; v[2] = s[2]; } } };
enum class VoiceKind { OneShot, Loop };
struct SoundSample { unsigned Buffer() const { return 1; } std::string Name() const { return "sound/interface/button1.wav"; } };
struct SoundVoice {
	VoiceKind kind = VoiceKind::Loop;
	bool active = false, fixedOrigin = false, doppler = false, looping = false, killWhenUnrefreshed = false;
	bool refreshedThisFrame = false, occlusionInitialized = false, rtcwOneShot = false, interfaceSound = false;
	int handle = 0, entnum = 0, entchannel = 0, allocTime = 0, lastOcclusionSmoothTime = 0, nextEnvironmentUpdateTime = 0;
	int mohaaChannel = -1;
	std::uint32_t jkaSpeechStart = 0;
	ALuint source = 1;
	SoundSample *sample = nullptr;
	Vec3f origin, velocity;
	float volumeScale = 1, mohaaMinDist = 0, mohaaMaxDist = 0, mohaaPitch = 1, occlusion = 0, occlusionTarget = 0;
	float debugGain = 0, debugDistance = 0;
};
struct Device {
	Device &AL() { return *this; }
	void alSourceStop( ALuint ) {}
	void alSourcei( ALuint, int, int ) {}
	void alSourcePlay( ALuint ) {}
	ALenum alGetError() { return 0; }
	void alGetSourcei( ALuint, int, ALint *value ) { *value = 0; }
	int FreeVoiceCount() const { return 0; }
};
struct Q3SoundWorld {
	Device device; Device *device_ = &device;
	struct MOHAASpan { float minDist = 0, maxDist = 0, pitch = 1; int channel = -1; } mohaaNext_;
	bool interfaceNext_ = false;
	struct { void Write( int, int ) {} } jkaMouths_;
	int applied = 0; bool appliedInterface = false;
	// The voice is applied once as it starts: the mark must be on it by then.
	void ApplyVoice( SoundVoice &voice, qboolean ) { ++applied; appliedInterface = voice.interfaceSound; }
MARK
	void PlayOneShot( SoundVoice *voice, int entityNum, int entchannel, sfxHandle_t handle,
		SoundSample *sample, const float *origin, float volume, int startTime );
};
PLAY
int main() {
	Q3SoundWorld world; SoundSample sample; SoundVoice a, b, c;
	world.MarkNextInterfaceSound( true );
	world.PlayOneShot( &a, 0, 0, 7, &sample, nullptr, 0.75f, 1000 );
	assert( a.interfaceSound && world.appliedInterface && !world.interfaceNext_ && a.volumeScale == 0.75f );
	// One mark, one one-shot: the next is the level's again.
	world.PlayOneShot( &b, 0, 0, 7, &sample, nullptr, 1.0f, 1000 );
	assert( !b.interfaceSound && !world.appliedInterface );
	// A voice reused for the level's sound keeps no mark of the menu's.
	world.PlayOneShot( &a, 3, 2, 9, &sample, nullptr, 1.0f, 1001 );
	assert( !a.interfaceSound );
	// A mark the front end withdrew (the start was refused) reaches no one.
	world.MarkNextInterfaceSound( true ); world.MarkNextInterfaceSound( false );
	world.PlayOneShot( &c, 0, 0, 7, &sample, nullptr, 1.0f, 1002 );
	assert( !c.interfaceSound && world.applied == 4 );
	std::puts( "one-shot passed" );
}
'''

FRONT_END = r'''
#include <cassert>
#include <cstdio>
#include <initializer_list>
enum qboolean { qfalse, qtrue };
typedef int sfxHandle_t;
typedef float vec3_t[3];
constexpr int ERR_DROP = 1;
struct Refused {};
static void Com_Error( int, const char * ) { throw Refused{}; }
static int profile = 0;  // Quake III's line, RTCW, Enemy Territory, Ritual
static bool Com_GameIsRTCW() { return profile == 1; }
static bool Com_GameIsWolfET() { return profile == 2; }
static bool Com_GameIsRitual() { return profile == 3; }
static bool marked = false, startMarked = false;
static int marks = 0, starts = 0, lastHandle = -1, lastChannel = -1, lastEntity = -1;
static float lastVolume = -1;
static void Record( sfxHandle_t handle, int channel, float volume ) {
	++starts; lastHandle = handle; lastChannel = channel; lastVolume = volume; startMarked = marked;
}
static void Mark( qboolean on ) { marked = on != qfalse; ++marks; }
static void Local( sfxHandle_t handle, int channel, float volume ) { Record( handle, channel, volume ); }
static struct { void ( *MarkInterfaceSound )( qboolean ); void ( *StartLocalSound )( sfxHandle_t, int, float ); } si;
static void S_StartRTCWLocalSound( sfxHandle_t handle, int channel ) { Record( handle, channel, -2.0f ); }
static void S_StartWolfETLocalSound( sfxHandle_t handle, int channel, int volume ) { Record( handle, channel, float( volume ) ); }
static int S_AliceListenerNumber() { return 4; }
static void S_AliceStartSound( const vec3_t, int entity, int channel, sfxHandle_t handle, float volume, float ) {
	lastEntity = entity; Record( handle, channel, volume );
}
ENTRIES
int main() {
	si.MarkInterfaceSound = Mark; si.StartLocalSound = Local;
	// Quake III's line: the menus' sound at the level they pass, marked while it starts.
	S_StartInterfaceSound( 7, 6, 0.75f );
	assert( starts == 1 && lastHandle == 7 && lastChannel == 6 && lastVolume == 0.75f );
	assert( startMarked && !marked && marks == 2 );
	// RTCW's own local sound, which carries no volume.
	profile = 1;
	S_StartInterfaceSound( 8, 0, 1.0f );
	assert( lastHandle == 8 && lastChannel == 0 && lastVolume == -2.0f && startMarked && !marked );
	// Enemy Territory's, at the volume its menus pass.
	profile = 2;
	S_StartWolfETInterfaceSound( 9, 0, 127 );
	assert( lastHandle == 9 && lastVolume == 127.0f && startMarked && !marked );
	// The Ritual engines' menu channel at the listener, whatever the import names.
	profile = 3;
	S_StartInterfaceSound( 10, 6, 1.0f );
	assert( lastHandle == 10 && lastChannel == 9 && lastEntity == 4 && !marked );
	// Enemy Territory's entry refuses every other profile before it marks anything.
	for ( int other : { 0, 1, 3 } ) {
		profile = other;
		const int before = marks, started = starts;
		bool refused = false;
		try { S_StartWolfETInterfaceSound( 1, 0, 127 ); } catch ( Refused ) { refused = true; }
		assert( refused && marks == before && starts == started && !marked );
	}
	// A backend without the mark still plays the sound; no backend plays nothing.
	profile = 0; si.MarkInterfaceSound = nullptr;
	S_StartInterfaceSound( 11, 0, 1.0f );
	assert( lastHandle == 11 && !startMarked );
	si.StartLocalSound = nullptr;
	const int before = starts;
	S_StartInterfaceSound( 12, 0, 1.0f );
	assert( starts == before );
	std::puts( "front end passed" );
}
'''


class UIInterfaceSoundTests(unittest.TestCase):
    def compile_run(self, source, marker):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        flags = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ui-sound-', dir=ROOT / '.tmp') as temporary:
            path = Path(temporary) / 'probe.cpp'
            executable = Path(temporary) / ('probe.exe' if os.name == 'nt' else 'probe')
            path.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, '-std=c++17', '-fno-fast-math', *flags, '-I', str(ROOT), str(path),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn(marker, run.stdout)

    def test_the_policy_makes_a_menus_sound_the_interfaces_on_any_channel(self):
        self.compile_run(POLICY, 'policy passed')

    def test_the_world_plays_a_menus_sound_dry_at_the_listener(self):
        self.compile_run(wolfenstein_voices.prelude() + '#include <cstdio>\n' + APPLY_VOICE, 'apply passed')

    def test_elite_force_single_player_plays_its_menus_sounds_the_same_way(self):
        self.compile_run(stvef_voices.prelude() + '#include <cstdio>\n' + APPLY_STVEF_VOICE, 'stvef passed')

    def test_one_mark_reaches_exactly_the_next_one_shot(self):
        world = WORLD.read_text(encoding='utf-8')
        mark = next(line for line in world.splitlines() if 'void MarkNextInterfaceSound(' in line)
        play = function(world, 'void Q3SoundWorld::PlayOneShot(')
        self.compile_run(ONE_SHOT.replace('MARK', mark).replace('PLAY', play), 'one-shot passed')

    def test_the_front_end_marks_the_profiles_own_local_sound(self):
        main = MAIN.read_text(encoding='utf-8')
        entries = '\n'.join(function(main, signature) for signature in (
            'extern "C" void S_StartLocalSoundVolume(', 'extern "C" void S_StartInterfaceSound(',
            'extern "C" void S_StartWolfETInterfaceSound('))
        self.compile_run(FRONT_END.replace('ENTRIES', entries), 'front end passed')

    def test_every_menu_host_starts_its_sounds_as_the_interfaces(self):
        # Quake III, Quake Live and Elite Force Holomatch share the dispatcher's case.
        ui = read('code/client/cl_ui.cpp')
        self.assertIn('S_StartInterfaceSound( args[1], args[2], 1.0f );',
                      case(ui, '\tcase UI_S_STARTLOCALSOUND:\n'))
        rtcw = read('code/client/client_rtcw_audio_compat.hpp')
        self.assertIn('S_StartInterfaceSound(CL_RTCWAudioInteger(args[1]), CL_RTCWAudioInteger(args[2]), 1.0f);',
                      case(rtcw, 'case Call::UI_S_STARTLOCALSOUND:'))
        wolfet = read('code/client/client_wolfet_audio_compat.hpp')
        self.assertIn('S_StartWolfETInterfaceSound( CL_WolfETAudioInteger( args[1] ),',
                      case(wolfet, 'case Call::UI_S_STARTLOCALSOUND:'))
        academy = read('code/client/client_jka_ui_host.hpp')
        self.assertIn('S_StartInterfaceSound( static_cast<sfxHandle_t>( args[1] ), sound::EngineChannel( channel ),',
                      case(academy, '\tcase Call::UI_S_STARTLOCALSOUND: {', 'return true;'))
        outcast = read('code/client/client_jk2_ui.hpp')
        self.assertIn('S_StartInterfaceSound( static_cast<sfxHandle_t>( args[1] ), static_cast<int>( args[2] ), 1.0f );',
                      case(outcast, '\tcase 32:\n', 'return true;'))
        single = read('code/client/client_stvef_sp_ui.hpp')
        self.assertIn('S_StartInterfaceSound( sfx, CL_STVEFSPSoundChannel( channel ), 1.0f );',
                      function(single, 'void CL_STVEFSPUIStartLocalSound('))
        q3test = read('code/client/cl_q3test_ui.cpp')
        self.assertIn('S_StartInterfaceSound( S_RegisterSound( name, qfalse ), 1, 1.0f );',
                      function(q3test, 'void QDECL Q3UI_StartLocalSound('))

    def test_no_client_game_sound_becomes_the_interfaces(self):
        cgame = read('code/client/cl_cgame.cpp')
        self.assertIn('S_StartLocalSound( args[1], args[2] );', case(cgame, '\tcase CG_S_STARTLOCALSOUND:\n'))
        rtcw = read('code/client/client_rtcw_audio_compat.hpp')
        self.assertIn('S_StartRTCWLocalSound(', case(rtcw, 'case Call::CG_S_STARTLOCALSOUND:'))
        wolfet = read('code/client/client_wolfet_audio_compat.hpp')
        self.assertIn('S_StartWolfETLocalSound(', case(wolfet, 'case Call::CG_S_STARTLOCALSOUND:'))
        academy = read('code/client/client_jka_cgame_host.hpp')
        self.assertIn('S_StartLocalSoundVolume(', case(academy, 'case Call::CG_S_STARTLOCALSOUND: {', 'return true;'))
        single = read('code/client/client_stvef_sp_cgame.hpp')
        self.assertIn('S_StartLocalSound( static_cast<sfxHandle_t>( args[1] ), CL_STVEFSPSoundChannel(',
                      case(single, 'case Call::CG_S_STARTLOCALSOUND:'))
        # Only the menu hosts reach the interface entries.
        callers = {}
        for path in sorted((ROOT / 'code').rglob('*')):
            if path.suffix not in ('.c', '.cpp', '.h', '.hpp', '.inl'):
                continue
            text = path.read_text(encoding='utf-8', errors='replace')
            for entry in ('S_StartInterfaceSound(', 'S_StartWolfETInterfaceSound('):
                if entry in text:
                    callers.setdefault(entry, set()).add(path.relative_to(ROOT).as_posix())
        self.assertEqual(callers['S_StartInterfaceSound('], {
            'code/client/audio/snd_main.cpp', 'code/client/audio/snd_public.h', 'code/client/cl_ui.cpp',
            'code/client/client_rtcw_audio_compat.hpp', 'code/client/client_jka_ui_host.hpp',
            'code/client/client_jk2_ui.hpp', 'code/client/client_stvef_sp_ui.hpp', 'code/client/cl_q3test_ui.cpp'})
        self.assertEqual(callers['S_StartWolfETInterfaceSound('], {
            'code/client/audio/snd_main.cpp', 'code/client/audio/snd_public.h',
            'code/client/client_wolfet_audio_compat.hpp'})

    def test_both_backends_take_the_mark_and_only_one_presents_it(self):
        local = LOCAL.read_text(encoding='utf-8')
        self.assertIn('void (*MarkInterfaceSound)( qboolean interfaceSound );', local)
        main = MAIN.read_text(encoding='utf-8')
        self.assertIn('if( !s->MarkInterfaceSound ) return false;', function(main, 'static bool S_ValidSoundInterface('))
        legacy = LEGACY.read_text(encoding='utf-8')
        self.assertIn('si->MarkInterfaceSound = S_Base_MarkInterfaceSound;', legacy)
        # The legacy mixer, the retail-exact option, has no classes to present it with.
        self.assertRegex(function(legacy, 'static void S_Base_MarkInterfaceSound('),
                         r'static void S_Base_MarkInterfaceSound\( qboolean \) \{\s*\}')
        backend = BACKEND.read_text(encoding='utf-8')
        self.assertIn('si->MarkInterfaceSound = []( qboolean interfaceSound ) { '
                      'AudioSystem::Get().world_.MarkNextInterfaceSound( interfaceSound != qfalse ); };', backend)
        world = WORLD.read_text(encoding='utf-8')
        # A mark never outlives a reset or a stop of every sound, and a voice
        # released for reuse forgets it.
        for signature in ('void Q3SoundWorld::Reset(', 'void Q3SoundWorld::StopAllSounds('):
            self.assertIn('interfaceNext_ = false;', function(world, signature), signature)
        release = world[world.index('\tvoid Release( OpenALDevice &device ) {'):]
        self.assertIn('interfaceSound = false;', release[:release.index('\n\t}\n')])
        self.assertIn('classInputs.interfaceSound = voice.interfaceSound;',
                      function(world, 'static VoiceToneSettings BuildVoiceToneSettings('))


if __name__ == '__main__':
    unittest.main()
