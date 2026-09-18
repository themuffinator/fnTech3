"""ET's world-covering fog volume: loading it, and retuning it with a fade."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
FOG = ROOT/'code/renderercommon/tr_wolfet_global_fog.h'
SCENE = ROOT/'code/renderercommon/tr_wolfet_global_fog_scene.h'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-global-fog.md'
PROBE = ROOT/'scripts/probe_wolfet_global_fog.py'

FIXTURE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_wolfet_global_fog.h"
#include <assert.h>

static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.0002f); }

static const float GREY[3] = { 0.5f, 0.5f, 0.5f };

static fnWolfETGlobalFog_t Authored(void) {
	fnWolfETGlobalFog_t state;
	const float blue[3] = { 0, 0, 1 };
	FnWolfET_GlobalFogClear(&state);
	FnWolfET_GlobalFogAuthored(&state, 1, blue, 2000);
	return state;
}

int main(void) {
	fnWolfETGlobalFog_t state;
	float current[4] = { 0, 0, 1, 2000 };
	float out[4];

	/* A map with no world-covering volume has no global fog to retune. */
	FnWolfET_GlobalFogClear(&state);
	assert(!FnWolfET_GlobalFogSet(&state, current, 0, 0, 0, GREY, 900, out));
	assert(!FnWolfET_GlobalFogFade(&state, 0, out));

	/* With one, an immediate request is what the fog shows at once. */
	state = Authored();
	assert(FnWolfET_GlobalFogSet(&state, current, 0, 0, 1000, GREY, 900, out));
	Near(out[0], 0.5f); Near(out[1], 0.5f); Near(out[2], 0.5f); Near(out[3], 900);
	assert(!FnWolfET_GlobalFogFade(&state, 1000, out));

	/* A depth for opaque never goes below one: the texture scale divides by it. */
	assert(FnWolfET_GlobalFogSet(&state, current, 0, 0, 1000, GREY, 0, out));
	Near(out[3], 1);
	assert(FnWolfET_GlobalFogSet(&state, current, 0, 0, 1000, GREY, -5, out));
	Near(out[3], 1);

	/* A restore ignores the colour it is given and returns what the map had. */
	assert(FnWolfET_GlobalFogSet(&state, current, 1, 0, 1000, GREY, 900, out));
	Near(out[0], 0); Near(out[1], 0); Near(out[2], 1); Near(out[3], 2000);

	/* A duration fades instead. This frame shows what the fog already had, so
	 * nothing jumps; the fade then runs to the target and stops. */
	state = Authored();
	assert(FnWolfET_GlobalFogSet(&state, current, 0, 1000, 5000, GREY, 1000, out));
	Near(out[0], 0); Near(out[2], 1); Near(out[3], 2000);
	assert(FnWolfET_GlobalFogFade(&state, 5000, out));
	Near(out[0], 0); Near(out[3], 2000);
	assert(FnWolfET_GlobalFogFade(&state, 5500, out));
	Near(out[0], 0.25f); Near(out[1], 0.25f); Near(out[2], 0.75f); Near(out[3], 1500);
	assert(FnWolfET_GlobalFogFade(&state, 6000, out));
	Near(out[0], 0.5f); Near(out[2], 0.5f); Near(out[3], 1000);
	/* The last step lands on the target exactly and the fade is over. */
	assert(FnWolfET_GlobalFogFade(&state, 6001, out));
	Near(out[0], 0.5f); Near(out[3], 1000);
	assert(!FnWolfET_GlobalFogFade(&state, 6002, out));

	/* A time that has already run past the end lands on the target too. */
	state = Authored();
	assert(FnWolfET_GlobalFogSet(&state, current, 0, 1000, 5000, GREY, 1000, out));
	assert(FnWolfET_GlobalFogFade(&state, 90000, out));
	Near(out[0], 0.5f); Near(out[3], 1000);
	assert(!FnWolfET_GlobalFogFade(&state, 90001, out));

	/* A fade back to what the map authored starts from wherever the fog is. */
	state = Authored();
	{
		const float half[4] = { 0.5f, 0.5f, 0.5f, 1000 };
		assert(FnWolfET_GlobalFogSet(&state, half, 1, 400, 200, GREY, 0, out));
		Near(out[0], 0.5f); Near(out[3], 1000);
		assert(FnWolfET_GlobalFogFade(&state, 400, out));
		Near(out[0], 0.25f); Near(out[2], 0.75f); Near(out[3], 1500);
		assert(FnWolfET_GlobalFogFade(&state, 600, out));
		Near(out[0], 0); Near(out[2], 1); Near(out[3], 2000);
	}

	/* Nonsense is refused rather than fogged with. */
	state = Authored();
	assert(!FnWolfET_GlobalFogSet(&state, current, 0, 0, 0, NULL, 900, out));
	{
		const float bad[3] = { (float)(1.0 / 0.0), 0, 0 };
		assert(!FnWolfET_GlobalFogSet(&state, current, 0, 0, 0, bad, 900, out));
		assert(!FnWolfET_GlobalFogSet(&state, current, 0, 0, 0, GREY,
			(float)(1.0 / 0.0), out));
	}
	{
		float infinite[4] = { (float)(1.0 / 0.0), 0, 0, 1 };
		assert(!FnWolfET_GlobalFogSet(&state, infinite, 0, 0, 0, GREY, 900, out));
	}
	/* A duration that would run the end time past the counter never finishes. */
	assert(!FnWolfET_GlobalFogSet(&state, current, 0, 1000, INT_MAX - 10, GREY, 900, out));
	/* A refused request leaves no fade behind. */
	assert(!FnWolfET_GlobalFogFade(&state, 0, out));
	return 0;
}
'''


def source(name):
    return (ROOT/name).read_text(encoding='utf-8')


class WolfETGlobalFogTests(unittest.TestCase):
    def test_a_request_becomes_what_the_fog_shows(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet-global-fog-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            folder = Path(directory)
            path = folder/'fog.cpp'
            executable = folder/('fog.exe' if os.name == 'nt' else 'fog')
            path.write_text(FIXTURE, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            built = subprocess.run([compiler, *target, '-std=c++17',
                '-I', str(ROOT), str(path), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_a_fog_with_no_brush_covers_the_world_instead_of_refusing_the_map(self):
        for name in RENDERERS:
            bsp = source('code/%s/tr_bsp.c' % name)
            loader = bsp[bsp.index('static void R_LoadFogs('):]
            loader = loader[:loader.index('\n}\n')]
            self.assertIn('out->originalBrushNumber == -1', loader, name)
            self.assertIn('MIN_WORLD_COORD', loader, name)
            self.assertIn('FnWolfET_GlobalFogAuthored', loader, name)
            # The brush path keeps its own refusal for a number that is out of
            # range rather than simply absent.
            self.assertIn('fog brushNumber out of range', loader, name)

    def test_every_renderer_carries_the_state_and_moves_the_fade_on(self):
        for name in RENDERERS:
            local = source('code/%s/tr_local.h' % name)
            self.assertIn('fnWolfETGlobalFog_t\twolfetGlobalFog;', local, name)
            self.assertIn('void RE_SetWolfETGlobalFog(', local, name)
            scene = source('code/%s/tr_scene.c' % name)
            self.assertIn('R_WolfETAdvanceGlobalFog();', scene, name)
            init = source('code/%s/tr_init.c' % name)
            self.assertIn('re.SetWolfETGlobalFog = RE_SetWolfETGlobalFog;', init, name)

    def test_the_request_belongs_to_its_own_profile(self):
        scene = SCENE.read_text(encoding='utf-8')
        self.assertIn('ri.gameId != COM_GAME_WOLFET', scene)
        # The volume's own fields are written; a shader can be shared.
        self.assertNotIn('->shader->fogParms', scene)
        arithmetic = FOG.read_text(encoding='utf-8')
        self.assertNotIn('COM_GAME_', arithmetic)
        self.assertNotIn('tr.', arithmetic)

    def test_the_import_is_answered(self):
        compat = source('code/client/client_wolfet_render_compat.hpp')
        self.assertIn('case Call::CG_R_SETGLOBALFOG:', compat)
        self.assertIn('re.SetWolfETGlobalFog(', compat)

    def test_the_probe_reports_what_the_maps_carry(self):
        probe = PROBE.read_text(encoding='utf-8')
        for claim in ('brushlessVolumes', 'installationUnchanged', 'visibleSide'):
            self.assertIn(claim, probe)

    def test_the_record_states_what_is_and_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('No image has been produced', record)
        self.assertIn('every fog volume Enemy Territory ships', record)


if __name__ == '__main__':
    unittest.main()
