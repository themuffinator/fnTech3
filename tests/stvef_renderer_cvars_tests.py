"""Run renderer compression registration through the real cvar system."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from cvar_range_check_tests import PREAMBLE

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
static struct {
    comGameId_t gameId;
    cvar_t *(*Cvar_Get)(const char *, const char *, int);
} ri;
static cvar_t *r_ext_compressed_textures;
HEADER
static void Register(void) { REGISTRATION }

int main(void) {
    int profile, preset, cases = 0;
    for (profile = COM_GAME_QUAKE3; profile < COM_GAME_COUNT; ++profile) {
        const int elite = profile == COM_GAME_STVEF_SP || profile == COM_GAME_STVEF_MP;
        const char *name = elite ? "r_ext_compress_textures" : "r_ext_compressed_textures";
        const char *other = elite ? "r_ext_compressed_textures" : "r_ext_compress_textures";
        for (preset = -1; preset <= 1; ++preset) {
            cvar_t *cv;
            /* Other profiles retain their existing developer-only default. */
            int initial = elite ? (preset < 0 ? 1 : preset) : 0;
            const char *changed = initial ? "0" : "1";
            Cvar_Init();
            ri.gameId = (comGameId_t)profile; ri.Cvar_Get = Cvar_Get;
            /* A config from a different profile must not override this one. */
            Seta(other, initial ? "0" : "1");
            if (preset >= 0) Seta(name, preset ? "1" : "0");
            Register(); cv = r_ext_compressed_textures;
            CHECK(!strcmp(cv->name, name));
            CHECK(cv->integer == initial && !cv->latchedString);
            CHECK(!strcmp(cv->resetString, elite ? "1" : "0"));
            CHECK(cv->flags == ((elite ? CVAR_ARCHIVE | CVAR_LATCH :
                  CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER) |
                  (preset >= 0 ? CVAR_ARCHIVE : 0)));
            CHECK(cv == Cvar_FindVar(name));
            printed[0] = 0;
            Seta(name, changed);
            if (elite) {
                CHECK(cv->integer == initial && cv->latchedString &&
                      !strcmp(cv->latchedString, changed));
                CHECK(!strstr(printed, "developer mode"));
                Register();
                CHECK(r_ext_compressed_textures == cv && !cv->latchedString &&
                      cv->integer == !initial);
                /* The retail SP UI sets a numeric cvar before vid_restart. */
                Cvar_SetValue(name, (float)initial);
                Register();
                CHECK(cv->integer == initial && !cv->latchedString);
                Cvar_Reset(name); Register();
                CHECK(cv->integer == 1 && !cv->latchedString);
            } else {
                CHECK(cv->integer == initial && !cv->latchedString);
                CHECK(strstr(printed, "developer mode"));
            }
            CHECK(Cvar_VariableIntegerValue(other) == !initial);
            while (cvar_vars) Cvar_Unset(cvar_vars);
            ++cases;
        }
        /* The method preference belongs only to Elite Force. A foreign
         * config may contain it, but other renderers must not adopt it. */
        for (preset = -1; preset <= 2; ++preset) {
            cvar_t *cv;
            Cvar_Init();
            ri.gameId = (comGameId_t)profile; ri.Cvar_Get = Cvar_Get;
            if (preset >= 0) Seta("r_ext_preferred_tc_method", va("%d", preset));
            Register();
            cv = r_ext_preferred_tc_method;
            if (elite) {
                CHECK(cv && cv == Cvar_FindVar("r_ext_preferred_tc_method"));
                CHECK(cv->flags == (CVAR_ARCHIVE | CVAR_LATCH));
                CHECK(!strcmp(cv->resetString, "0"));
                CHECK(cv->integer == (preset < 0 ? 0 : preset));
                Seta(cv->name, "-7");
                CHECK(cv->latchedString && !strcmp(cv->latchedString, "-7"));
                Register();
                CHECK(cv->integer == -7 && !cv->latchedString);
                Cvar_Reset(cv->name); Register();
                CHECK(cv->integer == 0 && !cv->latchedString);
            } else {
                CHECK(!cv);
                cv = Cvar_FindVar("r_ext_preferred_tc_method");
                CHECK(preset < 0 ? !cv : cv && cv->integer == preset &&
                      !(cv->flags & CVAR_LATCH));
            }
            while (cvar_vars) Cvar_Unset(cvar_vars);
            ++cases;
        }
    }
    if (!failures) printf("49 compression config/latch cases passed\n");
    return failures ? 1 : 0;
}
'''


class STVEFRendererCvarTests(unittest.TestCase):
    def test_compression_config_and_restart_contract(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-cvars-') as folder:
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(renderer=renderer):
                    source = (ROOT / f'code/{renderer}/tr_init.c').read_text()
                    registration = re.search(r'r_ext_compressed_textures\s*=\s*[^;]+;', source)
                    self.assertIsNotNone(registration)
                    header = ('#include "code/renderercommon/tr_stvef_cvars.h"'
                              if 'renderercommon/tr_stvef_cvars.h"' in source else '')
                    fixture = Path(folder) / f'{renderer}.c'
                    executable = fixture.with_suffix('.exe' if os.name == 'nt' else '')
                    fixture.write_text(PREAMBLE + HARNESS.replace('HEADER', header)
                                       .replace('REGISTRATION', registration[0]))
                    built = subprocess.run([compiler, *target, '-std=c11', '-O1',
                        '-I', str(ROOT), str(fixture), '-o', str(executable),
                        *([] if os.name == 'nt' else ['-lm'])], cwd=ROOT,
                        capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    ran = subprocess.run([str(executable)], cwd=ROOT, capture_output=True,
                                         text=True, timeout=20, **options)
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                    self.assertIn('49 compression config/latch cases passed', ran.stdout)


if __name__ == '__main__':
    unittest.main()
