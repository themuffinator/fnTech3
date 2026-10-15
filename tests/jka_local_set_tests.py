"""Jedi Academy's sets heard around a point (client-game import 51).

Retail jamp.exe 1.01's AS_AddLocalSet (0x4684f0) and AS_PlayLocalSet
(0x4680d0) were measured with Capstone and run in-process on the synthetic
cases of tests/jka_local_set_cases.h
(docs/fntech3/2026-10-15-compatibility-round.md); the fixture
tests/fixtures/jka_local_set_retail.json holds every loop and subwave they
handed the sound system, the time they answered and the generator they left.
The probes here:

- play every case through code/compat/jka_ambient_player.hpp's PlayLocal and
  compare each loop, subwave, answer and generator state with retail's;
- pin the volume a set's own record holds through the general sets' fades,
  which a set heard around a point plays at, and the generator both share;
- mix two places of one sound through the legacy mixer's production loop
  code, whose sides are summed and held at full level as retail's are.
"""
from pathlib import Path
import json
import re
import unittest

from jka_ambient_playback_tests import compile_run, function, read

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/jka_local_set_retail.json'

PLAY = r'''
#include "code/compat/jka_ambient_player.hpp"
#include "tests/jka_local_set_cases.h"
#include <cstdio>
using namespace fntech3::jka::ambient;
struct Sink {
    const float *origin;
    void Loop(const float *at, int handle, int volume) {
        std::printf("loop %d %d %.9g %.9g %.9g %d\n", handle, volume, at[0], at[1], at[2], at == origin ? 1 : 0);
    }
    void EntityWave(const float *at, int entity, int handle, int volume) {
        std::printf("wave %d %d %d %.9g %.9g %.9g %d\n", handle, entity, volume, at[0], at[1], at[2],
            at == origin ? 1 : 0);
    }
};
int main() {
    for (int index = 0; index < JKA_LOCAL_SET_CASES; ++index) {
        jkaLocalSetCase_t c;
        JKALocalSetCase(index, &c);
        Random random;
        random.state = c.seed;
        PlaySet set;
        set.id = 1;
        set.loop = c.loop;
        set.waveCount = static_cast<std::size_t>(c.waveCount);
        for (int i = 0; i < 8; ++i) set.waves[static_cast<std::size_t>(i)] = c.waves[i];
        set.minimumSeconds = static_cast<std::uint32_t>(c.timeStart);
        set.maximumSeconds = static_cast<std::uint32_t>(c.timeEnd);
        set.minimumVolume = c.volumeStart;
        set.maximumVolume = c.volumeEnd;
        set.masterVolume = c.masterVolume;
        Sink sink{c.origin};
        std::printf("case %d\n", index);
        // The host answers the real time for a name it does not hold, as
        // retail's lookup does (0x468504), and draws nothing. Retail's own
        // reckoning takes the handed time as the last subwave's.
        std::int32_t result = c.realtime, lastWave = c.time;
        if (c.found)
            result = PlayLocal(set, c.radius, c.listener, c.origin, c.entity, c.time, c.realtime, lastWave, random, sink);
        std::printf("result %d %u\n", static_cast<int>(result), random.state);
    }
    return 0;
}
'''

VALUES = r'''
#include "code/compat/jka_ambient_player.hpp"
#include <cassert>
#include <cstdio>
using namespace fntech3::jka::ambient;
struct Quiet {
    int loops = 0, waves = 0, lastVolume = -1;
    void Loop(const float *, int, int volume) { ++loops; lastVolume = volume; }
    void Wave(const float *, int, int) { ++waves; }
    void EntityWave(const float *, int, int, int) { ++waves; }
};
int main() {
    // The distance and the law at their edges, as retail's 53-bit x87 has them.
    const float at[3] = {0, 0, 0}, x125[3] = {125, 0, 0}, x3[3] = {3, 4, 12};
    assert(LocalDistance(at, x125) == 125.0f && LocalDistance(at, x3) == 13.0f);
    assert(LocalVolume(0.0f, 250, 255) == 255 && LocalVolume(125.0f, 250, 255) == 255);
    assert(LocalVolume(187.5f, 250, 255) == 127 && LocalVolume(250.0f, 250, 255) == 0);
    assert(LocalVolume(0.0f, 0, 255) == 0 && LocalVolume(5.0f, 0, 255) == 0);
    assert(LocalVolume(0.0f, -100, 255) == 0 && LocalVolume(200.0f, 300, 128) == 85);
    assert(LocalVolume(0.0f, 250, 300) == 44 && LocalVolume(0.0f, 250, -1) == 255);   // the low byte

    // A set's record holds 255 until the general fades write it.
    Player player;
    Quiet sink;
    const float origin[3] = {1, 2, 3};
    PlaySet a, b, c;
    a.id = 1; b.id = 2; c.id = 3;
    assert(player.MasterVolume(1) == 255 && player.MasterVolume(99) == 255);
    player.Update(a, origin, 1000, sink);
    assert(player.MasterVolume(1) == 0);
    player.Update(a, origin, 1500, sink);
    assert(player.MasterVolume(1) == 127);
    player.Update(b, origin, 2000, sink);            // b fades in, a out from 255
    assert(player.MasterVolume(2) == 0 && player.MasterVolume(1) == 255);
    player.Update(b, origin, 2600, sink);
    assert(player.MasterVolume(1) == 102 && player.MasterVolume(2) == 153);
    player.Update(c, origin, 2700, sink);            // a is let go mid-fade and keeps its level
    assert(player.MasterVolume(1) == 102 && player.MasterVolume(2) == 255 && player.MasterVolume(3) == 0);
    player.Update(c, origin, 3700, sink);            // b reaches nothing and is let go
    assert(player.MasterVolume(2) == 0 && player.MasterVolume(3) == 255 && !player.HasOld());
    player.Update(a, origin, 3800, sink);            // a comes back: in from 0 again
    assert(player.MasterVolume(1) == 0 && player.MasterVolume(3) == 255 && player.HasOld());

    // A set around a point plays at the level its record holds.
    PlaySet local = b;
    local.loop = 9;
    local.masterVolume = player.MasterVolume(2);
    Random &shared = player.Generator();
    const std::uint32_t before = shared.state;
    std::int32_t last = 0;
    assert(PlayLocal(local, 250, origin, origin, 17, 0, 0, last, shared, sink) == 0);
    assert(sink.lastVolume == 0 && shared.state != before);   // one draw, from the player's own generator
    local.masterVolume = player.MasterVolume(3);
    sink.lastVolume = -1;
    PlayLocal(local, 250, origin, origin, 17, 0, 0, last, shared, sink);
    assert(sink.lastVolume == 255);
    player.Reset();
    assert(player.MasterVolume(1) == 255 && player.MasterVolume(2) == 255 && player.Generator().state == 0x89abcdefu);

    // The multiplayer client game hands over a game clock behind the real
    // one. Retail's reckoning takes it as the last subwave's, so a subwave
    // comes on every call; the engine's own last time paces them at the
    // set's interval. The answer is retail's either way.
    PlaySet paced;
    paced.id = 7; paced.waves[0] = 5; paced.waveCount = 1;
    paced.minimumSeconds = paced.maximumSeconds = 10;
    Random random;
    Quiet waves;
    std::int32_t engine = 0;
    int answers = 0;
    for (std::int32_t now = 20000; now <= 80000; now += 100) {
        std::int32_t retailLast = 5000;
        Random copy = random;
        Quiet retail;
        const std::int32_t answer = PlayLocal(paced, 250, origin, origin, 17, 5000, now, engine, random, waves);
        assert(PlayLocal(paced, 250, origin, origin, 17, 5000, now, retailLast, copy, retail) == answer);
        assert(retail.waves == 1 && retailLast == now);
        answers += answer == now ? 1 : 0;
    }
    assert(waves.waves == 7 && answers == 601 && engine == 80000);   // 20, 30, ..., 80 s
    // A game clock ahead of the real one: retail's reckoning never plays one.
    engine = 0;
    Quiet ahead;
    for (std::int32_t now = 20000; now <= 40000; now += 100) {
        std::int32_t retailLast = 900000;
        Random copy = random;
        Quiet retail;
        assert(PlayLocal(paced, 250, origin, origin, 17, 900000, now, retailLast, copy, retail) == 900000);
        assert(retail.waves == 0);
        assert(PlayLocal(paced, 250, origin, origin, 17, 900000, now, engine, random, ahead) == 900000);
    }
    assert(ahead.waves == 3);                                         // 20, 30 and 40 s
    std::puts("ok");
    return 0;
}
'''

LEGACY = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include "code/client/audio/shared/JKAAmbient.h"
constexpr int MAX_CHANNELS = 4, kMasterVolume = 127, CHAN_AUTO = 0;
struct sfx_t { int lastTimeUsed = 0; };
struct channel_t { int master_vol = 0, leftvol = 0, rightvol = 0; sfx_t *thesfx = nullptr; };
static sfx_t samples[16];
static sfx_t *S_KnownSfxAt(int handle) { return samples + handle; }
static int s_soundtime = 77, numLoopChannels;
static channel_t loop_channels[MAX_CHANNELS];
static fntech3::audio::jka::AmbientLoops jkaAmbientLoops;
// Stands in for the engine's spatializer: a place's x picks its sides.
static void S_SpatializeOrigin(const float *origin, int master, int *left, int *right, int channel) {
    assert(channel == CHAN_AUTO);
    *left = origin[0] < 0 ? master : origin[0] > 0 ? 0 : master / 2;
    *right = origin[0] > 0 ? master : origin[0] < 0 ? 0 : master / 2;
}
LEGACY_FUNCTION
int main() {
    const float centre[3] = {0, 0, 0}, west[3] = {-10, 0, 0}, east[3] = {10, 0, 0};
    // One place: as before, the merged volume spatialized once.
    jkaAmbientLoops.Add(centre, 3, 200);
    jkaAmbientLoops.Add(centre, 3, 55);
    S_AddJKAAmbientLoopSounds();
    assert(numLoopChannels == 1 && loop_channels[0].leftvol == 63 && loop_channels[0].rightvol == 63);
    assert(loop_channels[0].master_vol == kMasterVolume && loop_channels[0].thesfx == samples + 3);
    assert(samples[3].lastTimeUsed == 77);
    // Two places of one sound are one channel, each side summed.
    jkaAmbientLoops.Clear(); numLoopChannels = 0;
    jkaAmbientLoops.Add(west, 4, 102);
    jkaAmbientLoops.Add(east, 5, 255);
    jkaAmbientLoops.Add(east, 4, 51);
    S_AddJKAAmbientLoopSounds();
    assert(numLoopChannels == 2);
    assert(loop_channels[0].thesfx == samples + 4 && loop_channels[0].leftvol == 50 && loop_channels[0].rightvol == 25);
    assert(loop_channels[1].thesfx == samples + 5 && loop_channels[1].leftvol == 0 && loop_channels[1].rightvol == 127);
    // Summed sides are held at full level, as retail holds them at 255.
    jkaAmbientLoops.Clear(); numLoopChannels = 0;
    jkaAmbientLoops.Add(west, 6, 255);
    jkaAmbientLoops.Add(centre, 6, 255);
    S_AddJKAAmbientLoopSounds();
    assert(numLoopChannels == 1 && loop_channels[0].leftvol == 127 && loop_channels[0].rightvol == 63);
    // Silent sides make no channel.
    jkaAmbientLoops.Clear(); numLoopChannels = 0;
    jkaAmbientLoops.Add(centre, 7, 1);
    S_AddJKAAmbientLoopSounds();
    assert(numLoopChannels == 0);
    std::puts("ok");
    return 0;
}
'''


def run_cases(test):
    output = compile_run(test, PLAY, 'jka-local-set-')
    cases, current = [], None
    for line in output.splitlines():
        words = line.split()
        if words[0] == 'case':
            current = {'case': int(words[1]), 'loops': [], 'waves': []}
            cases.append(current)
        elif words[0] == 'loop':
            current['loops'].append([int(words[1]), int(words[2]), *map(float, words[3:6]), int(words[6])])
        elif words[0] == 'wave':
            current['waves'].append([int(words[1]), int(words[2]), int(words[3]), *map(float, words[4:7]),
                                     int(words[7])])
        elif words[0] == 'result':
            current['result'] = int(words[1])
            current['seed'] = int(words[2])
    return cases


class JKALocalSetTests(unittest.TestCase):
    def test_every_case_plays_as_retail_played_it(self):
        fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
        retail = fixture['cases']
        header = (ROOT / 'tests/jka_local_set_cases.h').read_text(encoding='utf-8')
        hand = int(re.search(r'#define JKA_LOCAL_SET_HAND_CASES (\d+)', header).group(1))
        generated = int(re.search(r'JKA_LOCAL_SET_HAND_CASES \+ (\d+)', header).group(1))
        self.assertEqual([case['case'] for case in retail], list(range(hand + generated)))
        actual = run_cases(self)
        self.assertEqual(len(actual), len(retail))
        for got, want in zip(actual, retail):
            with self.subTest(case=want['case']):
                self.assertEqual(got['result'], want['result'])
                self.assertEqual(got['seed'], want['seed'])
                self.assertEqual(len(got['loops']), want['loopCalls'])
                self.assertEqual(len(got['waves']), want['waveCalls'])
                self.assertEqual(got['loops'], [[*row[:2], *map(float, row[2:5]), row[5]] for row in want['loops']])
                self.assertEqual(got['waves'], [[*row[:3], *map(float, row[3:6]), row[6]] for row in want['waves']])
        # The cases reach every rule the comparison is about.
        volumes = {row[1] for case in retail for row in case['loops']}
        self.assertTrue({0, 255} <= volumes and any(0 < volume < 255 for volume in volumes))
        self.assertTrue(any(case['waveCalls'] for case in retail))
        self.assertTrue(any(not case['waveCalls'] and case['loopCalls'] for case in retail))
        self.assertTrue(any(row[0] == 0 for case in retail for row in case['waves']))   # a null subwave
        self.assertTrue(all(row[-1] == 1 for case in retail for row in case['loops'] + case['waves']))

    def test_fixture_provenance(self):
        provenance = json.loads(FIXTURE.read_text(encoding='utf-8'))['provenance']
        self.assertEqual(provenance['retail'], 'jamp.exe 1.01')
        self.assertEqual(provenance['entries']['addLocalSet'], '0x4684f0')
        self.assertEqual(provenance['entries']['playLocalSet'], '0x4680d0')
        self.assertEqual(provenance['x87ControlWord'], '027f')
        self.assertRegex(provenance['sha256'], r'^[0-9a-f]{64}$')
        # Synthetic input only: no retail data is in the fixture.
        self.assertIn('tests/jka_local_set_cases.h', provenance['input'])

    def test_record_volumes_and_the_shared_generator(self):
        self.assertIn('ok', compile_run(self, VALUES, 'jka-local-values-'))

    def test_legacy_mixer_sums_each_place(self):
        dma = read('code/client/audio/legacy/snd_dma.cpp')
        mix = function(dma, 'static void S_AddJKAAmbientLoopSounds( void ) {')
        self.assertIn('ok', compile_run(self, LEGACY.replace('LEGACY_FUNCTION', mix), 'jka-local-legacy-'))


if __name__ == '__main__':
    unittest.main()
