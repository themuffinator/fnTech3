"""Jedi Academy multiplayer's ambient-set playback (client-game import 48).

Retail jamp.exe 1.01's S_UpdateAmbientSet and its callees were measured with
Capstone (docs/fntech3/2026-10-06-jka-ambient-sets.md). The probes here:

- run code/compat/jka_ambient_player.hpp frame by frame against an
  independent model written from those readings, over long generated
  sessions: set changes inside and after fades, parses that renumber sets,
  time jumps, sets without loops or waves, single-value ranges, and the
  engine-wide random draws, comparing every loop and wave exactly;
- pin the fade law, the draws and the arithmetic retail does at x87 double
  precision on chosen values;
- run the per-frame loop list both backends share (shared/JKAAmbient.h) and
  the OpenAL world's voice choice, so a loop keeps its voice across a set
  change and the loops of one sound are one voice;
- pin where the host, the sound interface and both backends serve it.
"""
from pathlib import Path
import os
import random
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
MASK = 0xFFFFFFFF


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def compile_run(test, source, prefix, extra=()):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as temp:
        cpp = Path(temp) / 'probe.cpp'
        exe = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *flags, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                                '-Wno-unused-function', *extra, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        return ran.stdout


# --- An independent model of the retail readings --------------------------

def s32(value):
    value &= MASK
    return value - (1 << 32) if value & 0x80000000 else value


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


class RetailModel:
    """Sets are objects that keep their own volume and fade time, numbered as
    retail numbers them; the two slots keep their own wave times."""

    def __init__(self):
        self.hold = 0x89ABCDEF          # 0x56e760
        self.sets = {}
        self.current = -1               # 0x56d530
        self.old = -1                   # 0x56d534
        self.current_wave = 0           # 0x12a4d0c
        self.old_wave = 0               # 0x12a4d10

    def rand(self):
        self.hold = (self.hold * 0x343FD + 0x269EC3) & MASK
        return self.hold >> 17

    def irand(self, lo, hi):
        product = s32(((hi - lo + 1) & MASK) * self.rand())
        return s32((product >> 15) + lo)

    @staticmethod
    def faded(now, began):
        # fild/fstp float, fdiv by the stored 1000, fstp float, fmul 255.0f, _ftol
        quotient = f32(f32(float(s32(now - began))) / f32(1000.0))
        return int(quotient * 255.0)

    def add(self, identity, definition):
        self.sets[identity] = dict(definition, master=255, fade=0)

    def update(self, identity, now, log):
        chosen = self.sets[identity]
        if identity != self.current:                     # 0x468028
            self.old, self.current = self.current, identity
            if self.old in self.sets:
                self.sets[self.old]['master'] = 255
                self.sets[self.old]['fade'] = now
            chosen['master'] = 0
            chosen['fade'] = now
        current = self.sets[self.current]               # 0x467ec0
        if current['master'] < 255:
            current['master'] = self.faded(now, current['fade'])
        if current['master'] > 255:
            current['master'] = 255
        if self.old != -1:
            old = self.sets[self.old]
            if old['master'] > 0:
                old['master'] = 255 - self.faded(now, old['fade'])
            if old['master'] <= 0:
                old['master'] = 0
                self.old = -1
        self.play(chosen, True, now, log)                # 0x4684c4
        if self.old != -1:
            self.play(self.sets[self.old], False, now, log)

    def play(self, set_, current_slot, now, log):        # 0x4682c0
        master = set_['master']
        if set_['loop']:
            log.append(('loop', set_['loop'], master & 0xFF))
        threshold = s32(self.irand(set_['tmin'], set_['tmax']) * 1000)
        last = self.current_wave if current_slot else self.old_wave
        if s32(now - last) < threshold:
            return
        if current_slot:
            self.current_wave = now
        else:
            self.old_wave = now
        scale = f32(f32(float(master)) / f32(255.0))
        lo = int(f32(float(set_['vmin'] & MASK)) * scale)
        hi = int(f32(float(set_['vmax'] & MASK)) * scale)
        product = s32(((hi - lo + 1) & MASK) * self.rand())
        volume = ((product >> 15) + lo) & 0xFF
        if volume > master:
            volume = master & 0xFF
        count = len(set_['waves']) & 0xFF
        if not count:
            return
        index = (count * self.rand()) >> 15
        log.append(('wave', set_['waves'][index], volume))


DEFINITIONS = [
    dict(loop=11, waves=[21, 22, 23], tmin=1, tmax=3, vmin=100, vmax=200),
    dict(loop=0, waves=[31], tmin=0, tmax=0, vmin=0, vmax=255),
    dict(loop=12, waves=[], tmin=10, tmax=25, vmin=255, vmax=255),
    dict(loop=13, waves=[41, 42], tmin=2, tmax=2, vmin=255, vmax=255),
    dict(loop=14, waves=[51, 52, 53, 54, 55, 56, 57, 58], tmin=0, tmax=4, vmin=17, vmax=30),
]


def session(seed, frames):
    """Frames of (definition, set number, time): a set at a time, renumbered
    when a parse comes, with small steps, long pauses and changes inside and
    after the fades."""
    rng = random.Random(seed)
    numbers = list(range(len(DEFINITIONS)))
    next_number = len(DEFINITIONS)
    now = rng.randrange(0, 40000)
    chosen = rng.randrange(len(DEFINITIONS))
    result = []
    for _ in range(frames):
        roll = rng.random()
        if roll < 0.01:
            # a new map: every set is numbered anew
            numbers = list(range(next_number, next_number + len(DEFINITIONS)))
            next_number += len(DEFINITIONS)
        if roll < 0.04:
            chosen = rng.randrange(len(DEFINITIONS))
        step = rng.choice((1, 5, 16, 16, 16, 33, 50, 100, 400, 999, 1000, 1001))
        if rng.random() < 0.01:
            step = rng.randrange(5000, 90000)
        now += step
        result.append((chosen, numbers[chosen], now))
    return result


def model_log(frames):
    model = RetailModel()
    log = []
    for definition, number, now in frames:
        if number not in model.sets:
            model.add(number, DEFINITIONS[definition])
        log.append(('frame', number, now))
        model.update(number, now, log)
    return log


PLAYER_HARNESS = r'''
#include "code/compat/jka_ambient_player.hpp"
#include <cstdio>
using namespace fntech3::jka::ambient;
struct Definition { int loop; int waves[8]; int count; unsigned tmin, tmax; int vmin, vmax; };
static const Definition definitions[] = { DEFINITIONS };
struct Frame { int definition; unsigned number; int now; };
static const Frame frames[] = { FRAMES };
struct Sink {
    void Loop(const float *origin, int handle, int volume) {
        if (origin[0] != 1.0f || origin[2] != 3.0f) std::printf("bad origin\n");
        std::printf("loop %d %d\n", handle, volume);
    }
    void Wave(const float *, int handle, int volume) { std::printf("wave %d %d\n", handle, volume); }
};
int main() {
    Player player;
    Sink sink;
    const float origin[3] = { 1.0f, 2.0f, 3.0f };
    for (const Frame &frame : frames) {
        const Definition &d = definitions[frame.definition];
        PlaySet set;
        set.id = frame.number;
        set.loop = d.loop;
        set.waveCount = static_cast<std::size_t>(d.count);
        for (int i = 0; i < d.count; ++i) set.waves[static_cast<std::size_t>(i)] = d.waves[i];
        set.minimumSeconds = d.tmin; set.maximumSeconds = d.tmax;
        set.minimumVolume = d.vmin; set.maximumVolume = d.vmax;
        std::printf("frame %u %d\n", frame.number, frame.now);
        player.Update(set, origin, frame.now, sink);
    }
    return 0;
}
'''


def player_source(frames):
    definitions = ',\n'.join(
        '{%d, {%s}, %d, %du, %du, %d, %d}' % (d['loop'], ','.join(map(str, d['waves'] + [0] * (8 - len(d['waves'])))),
                                             len(d['waves']), d['tmin'], d['tmax'], d['vmin'], d['vmax'])
        for d in DEFINITIONS)
    rows = ',\n'.join('{%d, %du, %d}' % frame for frame in frames)
    return PLAYER_HARNESS.replace('DEFINITIONS', definitions).replace('FRAMES', rows)


def parse_log(text):
    log = []
    for line in text.splitlines():
        kind, first, second = line.split()
        log.append((kind, int(first), int(second)))
    return log


VALUES = r'''
#include "code/compat/jka_ambient_player.hpp"
#include <cassert>
#include <cstdio>
using namespace fntech3::jka::ambient;
int main() {
    // The generator starts where retail's does and draws as it does.
    Random random;
    assert(random.state == 0x89abcdefu);
    assert(random.Next() == ((0x89abcdefu * 0x343fdu + 0x269ec3u) >> 17));
    // A range of one value is that value; the arithmetic wraps as retail's 32 bits do.
    for (int i = 0; i < 1000; ++i) { const int v = random.Range(7, 7); assert(v == 7); }
    // The fade: whole volumes of 255 over 1,000 ms, truncated.
    struct { int elapsed, volume; } const fades[] = {
        {0, 0}, {1, 0}, {3, 0}, {4, 1}, {500, 127}, {999, 254}, {1000, 255}, {2000, 510}, {-4, -1}, {-1, 0}};
    for (const auto &fade : fades) assert(FadedVolume(100000 + fade.elapsed, 100000) == fade.volume);
    // The x87 path: whole numbers stored as floats, their quotient stored as a float.
    assert(StoredQuotient(255, 255) == 1.0f && StoredQuotient(128, 255) == static_cast<float>(128.0 / 255.0));
    assert(StoredQuotient(16777217, 1000) == static_cast<float>(16777216.0 / 1000.0));
    // Past 2^24 ms the stored operand rounds, and that reaches the volume.
    assert(StoredQuotient(16777219, 1000) == 16777.220703125f && FadedVolume(16777219, 0) == 4278191);
    // A fresh player has no set; a reset forgets everything.
    Player player;
    assert(!player.HasCurrent() && !player.HasOld() && player.CurrentWaveTime() == 0);
    struct Sink {
        int loops = 0, waves = 0, silent = 0;
        void Loop(const float *, int, int) { ++loops; }
        void Wave(const float *, int, int volume) { ++(volume > 0 ? waves : silent); }
    } sink;
    const float origin[3] = {};
    PlaySet set; set.id = 4; set.loop = 9; set.waves[0] = 5; set.waveCount = 1; set.minimumSeconds = set.maximumSeconds = 0;
    player.Update(set, origin, 30000, sink);
    assert(player.HasCurrent() && player.Current().masterVolume == 0 && sink.loops == 1);
    // With no time between waves, a wave comes every frame; at master volume 0
    // its volume is 0: drawn, not heard.
    assert(sink.waves == 0 && sink.silent == 1);
    player.Update(set, origin, 30001, sink);
    assert(sink.waves == 0 && sink.silent == 2);
    player.Update(set, origin, 31000, sink);
    assert(player.Current().masterVolume == 255 && sink.waves == 1 && player.CurrentWaveTime() == 31000);
    player.Reset();
    assert(!player.HasCurrent() && player.Generator().state == 0x89abcdefu);
    // A drawn volume is taken as a byte, then held at the set's own; only a
    // set louder than the catalog allows reaches either.
    struct Last {
        int volume = -1;
        void Loop(const float *, int, int) {}
        void Wave(const float *, int, int drawn) { volume = drawn; }
    } last;
    Player loud;
    PlaySet big; big.id = 1; big.waves[0] = 5; big.waveCount = 1; big.minimumSeconds = big.maximumSeconds = 0;
    big.minimumVolume = big.maximumVolume = 300;
    loud.Update(big, origin, 1000, last);
    assert(last.volume == 0);
    loud.Update(big, origin, 1196, last);    // the set at 49: 300 scaled is 57, held at 49
    assert(loud.Current().masterVolume == 49 && last.volume == 49);
    loud.Update(big, origin, 2000, last);    // the set at 255: 300 as a byte is 44
    assert(loud.Current().masterVolume == 255 && last.volume == 44);
    std::puts("ok");
    return 0;
}
'''

LOOPS = r'''
#include "code/client/audio/shared/JKAAmbient.h"
#include <cassert>
#include <cstdio>
using fntech3::audio::jka::AmbientLoops;
int main() {
    AmbientLoops loops;
    const float a[3] = {1, 2, 3}, b[3] = {4, 5, 6};
    assert(loops.Add(a, 7, 200) == 0 && loops.Add(b, 8, 10) == 1);
    // The loops of one sound merge: their volumes sum, clamped at 255, at the first one's place.
    assert(loops.Add(b, 7, 100) == 0 && loops[0].volume == 255 && loops[0].origin[0] == 1 && loops.Size() == 2);
    // Silent loops and loops without a place are not added.
    assert(loops.Add(a, 9, 0) == -1 && loops.Add(a, 9, -3) == -1 && loops.Add(nullptr, 9, 10) == -1 && loops.Size() == 2);
    for (int handle = 100; loops.Size() < AmbientLoops::Capacity; ++handle) assert(loops.Add(a, handle, 1) >= 0);
    assert(loops.Add(a, 999, 255) == -1);                  // retail keeps 32 a frame
    assert(loops.Add(a, 7, 1) == 0 && loops[0].volume == 255); // merging needs no room
    loops.Clear();
    assert(loops.Size() == 0 && loops.Add(a, 7, 300) == 0 && loops[0].volume == 255);
    // Both sides are stored floats, including under MinGW x87.
    volatile float fifth = 0.2f, scaled = AmbientLoops::Scale(51);
    assert(AmbientLoops::Scale(255) == 1.0f && AmbientLoops::Scale(0) == 0.0f && scaled == fifth);
    assert(AmbientLoops::Scale(-5) == 0.0f && AmbientLoops::Scale(999) == 1.0f);
    std::puts("ok");
    return 0;
}
'''

WORLD = r'''
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
typedef int sfxHandle_t;
typedef int qboolean;
enum { qfalse, qtrue };
#define MAX_GENTITIES 4
#define ENTITYNUM_NONE 1023
namespace fntech3::audio::stvef { struct AmbientLoopFrame { static constexpr std::size_t Capacity = 32; }; }
#include "code/client/audio/shared/JKAAmbient.h"
struct SoundSample {};
struct SoundVoice {
    bool active = false; int handle = 0; bool refreshedThisFrame = false;
    float volumeScale = 1.0f; int entnum = 0; bool doppler = true; int starts = 0;
};
struct Q3SoundWorld {
    std::array<SoundVoice, MAX_GENTITIES + 32> loopVoices_;
    fntech3::audio::jka::AmbientLoops jkaAmbientLoops_;
    void AddLoopingSound(int slot, const float *, const float *, sfxHandle_t handle, SoundSample *, qboolean) {
        SoundVoice &voice = loopVoices_[static_cast<std::size_t>(slot)];
        if (!voice.active || voice.handle != handle) {
            const int starts = voice.starts;
            voice = SoundVoice{}; voice.active = true; voice.handle = handle; voice.starts = starts + 1;
        }
        voice.refreshedThisFrame = true;
    }
    void AddJKAAmbientLoopingSound(const float *origin, sfxHandle_t handle, int volume, SoundSample *sample);
    void Frame() {
        jkaAmbientLoops_.Clear();
        for (SoundVoice &voice : loopVoices_) if (voice.active) voice.refreshedThisFrame = false;
    }
    void Kill() {
        for (SoundVoice &voice : loopVoices_) if (voice.active && !voice.refreshedThisFrame) { voice.active = false; voice.handle = 0; }
    }
    const SoundVoice *Voice(int handle) const {
        for (std::size_t i = MAX_GENTITIES; i < loopVoices_.size(); ++i)
            if (loopVoices_[i].active && loopVoices_[i].handle == handle) return &loopVoices_[i];
        return nullptr;
    }
};
WORLD_FUNCTION
int main() {
    Q3SoundWorld world;
    SoundSample sample;
    const float at[3] = {1, 2, 3};
    world.Frame(); world.AddJKAAmbientLoopingSound(at, 11, 255, &sample); world.Kill();
    assert(world.Voice(11) && world.Voice(11)->starts == 1 && world.Voice(11)->volumeScale == 1.0f);
    assert(world.Voice(11)->entnum == ENTITYNUM_NONE && !world.Voice(11)->doppler);
    assert(&world.loopVoices_[MAX_GENTITIES] == world.Voice(11)); // past the entities' voices
    // A set change: the new current set's loop is added first. The old set's
    // loop keeps its voice, so it plays on from where it was.
    world.Frame(); world.AddJKAAmbientLoopingSound(at, 12, 0, &sample);
    world.AddJKAAmbientLoopingSound(at, 12, 1, &sample); world.AddJKAAmbientLoopingSound(at, 11, 255, &sample); world.Kill();
    assert(world.Voice(11)->starts == 1 && world.Voice(12)->starts == 1 && world.Voice(12) != world.Voice(11));
    for (int volume : {127, 254}) {
        world.Frame(); world.AddJKAAmbientLoopingSound(at, 12, volume, &sample);
        world.AddJKAAmbientLoopingSound(at, 11, 255 - volume, &sample); world.Kill();
        assert(world.Voice(11)->starts == 1 && world.Voice(12)->starts == 1);
    }
    // The old set is let go; the current one plays on in its voice.
    world.Frame(); world.AddJKAAmbientLoopingSound(at, 12, 255, &sample); world.Kill();
    assert(!world.Voice(11) && world.Voice(12)->starts == 1 && world.Voice(12)->volumeScale == 1.0f);
    // Two loops of one sound in a frame are one voice at their summed volume.
    world.Frame(); world.AddJKAAmbientLoopingSound(at, 12, 51, &sample); world.AddJKAAmbientLoopingSound(at, 12, 51, &sample); world.Kill();
    volatile float twoFifths = 0.4f;
    assert(world.Voice(12)->starts == 1 && world.Voice(12)->volumeScale == twoFifths);
    // A silent loop keeps no voice.
    world.Frame(); world.AddJKAAmbientLoopingSound(at, 12, 0, &sample); world.Kill();
    assert(!world.Voice(12));
    std::puts("ok");
    return 0;
}
'''


class JKAAmbientPlaybackTests(unittest.TestCase):
    def test_player_matches_the_retail_model_over_long_sessions(self):
        for seed in (1, 2, 3, 4):
            with self.subTest(seed=seed):
                frames = session(seed, 2500)
                expected = model_log(frames)
                actual = parse_log(compile_run(self, player_source(frames), 'jka-ambient-player-'))
                self.assertEqual(len(actual), len(expected))
                for index, (got, want) in enumerate(zip(actual, expected)):
                    self.assertEqual(got, want, f'event {index}')
                # The sessions reach every behaviour the comparison is about.
                kinds = {event[0] for event in expected}
                self.assertEqual(kinds, {'frame', 'loop', 'wave'})
                self.assertTrue(any(e[0] == 'loop' and 0 < e[2] < 255 for e in expected))

    def test_values_the_retail_arithmetic_gives(self):
        self.assertIn('ok', compile_run(self, VALUES, 'jka-ambient-values-'))

    def test_the_frame_loop_list_both_backends_share(self):
        self.assertIn('ok', compile_run(self, LOOPS, 'jka-ambient-loops-'))

    def test_openal_voices_follow_the_sound_not_the_set(self):
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        method = function(world, 'void Q3SoundWorld::AddJKAAmbientLoopingSound(')
        source = WORLD.replace('WORLD_FUNCTION', method)
        self.assertIn('ok', compile_run(self, source, 'jka-ambient-world-'))

    def test_where_the_engine_serves_it(self):
        calls = read('code/compat/jka_module_calls.def')
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_S_UPDATEAMBIENTSET, 48)', calls)
        host = read('code/client/client_jka_cgame_ambient.hpp')
        serve = host[host.index('case Call::CG_S_UPDATEAMBIENTSET: {'):host.index('case Call::CG_AS_ADDPRECACHEENTRY:')]
        self.assertNotIn('not served', serve)
        self.assertIn('jkaAmbientPlayer.Update(set, origin, cls.realtime, sink);', serve)
        self.assertIn('set.id = state.firstId + static_cast<std::uint32_t>(index);', serve)
        self.assertLess(serve.index('Find(name.data())'), serve.index('CL_JKACgameSpan(args[2]'))
        sink = host[host.index('struct JKAAmbientSink {'):host.index('};', host.index('struct JKAAmbientSink {'))]
        self.assertIn('S_AddJKAAmbientLoopingSound(origin, handle, volume);', sink)
        self.assertIn('S_StartSoundVolume(at, ENTITYNUM_WORLD, fntech3::jka::sound::EngineChannel(fntech3::jka::sound::ChanAmbient),', sink)
        # The player and the numbering are the engine's, kept across client games.
        clear = function(host, 'static void CL_JKACgameAmbientClear() {')
        self.assertNotIn('jkaAmbientPlayer', clear)
        self.assertNotIn('jkaAmbientNextId', clear)
        reset = function(host, 'void CL_JKAClearAmbientPlayer(void) {')
        self.assertIn('jkaAmbientPlayer.Reset();', reset)
        self.assertIn('jkaAmbientNextId = 0;', reset)
        self.assertIn('state.firstId = jkaAmbientNextId;', host)
        self.assertIn('void CL_JKAClearAmbientPlayer( void );', read('code/qcommon/qcommon.h'))
        main = read('code/client/audio/snd_main.cpp')
        init = function(main, 'extern "C" void S_Init( void )')
        self.assertIn('if ( Com_GameIsJKAMP() ) {', init)
        self.assertIn('CL_JKAClearAmbientPlayer();', init)
        self.assertIn('if( !s->AddJKAAmbientLoopingSound ) return false;', main)
        entry = function(main, 'extern "C" void S_AddJKAAmbientLoopingSound(')
        self.assertIn('if ( !Com_GameIsJKAMP() ) Com_Error(', entry)
        # Legacy: added, mixed and dropped where Elite Force's ambient loops are.
        dma = read('code/client/audio/legacy/snd_dma.cpp')
        self.assertEqual(dma.count('jkaAmbientLoops.Clear();'), dma.count('stvefAmbientLoops.Clear();'))
        self.assertIn('si->AddJKAAmbientLoopingSound = S_Base_AddJKAAmbientLoopingSound;', dma)
        loops = function(dma, 'static void S_AddLoopSounds( void ) {')
        self.assertIn('if ( Com_GameIsJKAMP() ) {\n\t\tS_AddJKAAmbientLoopSounds();', loops.replace('\r\n', '\n'))
        mix = function(dma, 'static void S_AddJKAAmbientLoopSounds( void ) {')
        self.assertIn('loop.volume * kMasterVolume / fntech3::audio::jka::AmbientLoops::MaxVolume', mix)
        # OpenAL: the backend entry and the world's frame list.
        backend = read('code/client/audio/openal/AudioSystemBackend.inl')
        self.assertIn('si->AddJKAAmbientLoopingSound = []', backend)
        self.assertIn('world_.AddJKAAmbientLoopingSound( origin, handle, volume, sample );', backend)
        world = read('code/client/audio/openal/AudioSystemWorld.inl')
        self.assertEqual(world.count('jkaAmbientLoops_.Clear();'), world.count('stvefAmbientLoops_.Clear();'))
        method = function(world, 'void Q3SoundWorld::AddJKAAmbientLoopingSound(')
        self.assertIn('voice.volumeScale = fntech3::audio::jka::AmbientLoops::Scale( loop.volume );', method)
        self.assertNotIn('stvefAmbient', method)   # rendered as the engine's own loops are


if __name__ == '__main__':
    unittest.main()
