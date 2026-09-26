"""Execute OpenGL method selection against original SP/Holomatch observations."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def profile_count():
    """The profiles from COM_GAME_QUAKE3 up to COM_GAME_COUNT, as the harness walks them."""
    header = (ROOT / 'code/qcommon/game_profile.h').read_text(encoding='utf-8')
    body = header[header.index('COM_GAME_AUTO = 0,'):header.index('COM_GAME_COUNT')]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    return len(re.findall(r'\bCOM_GAME_\w+', body)) - 1


HARNESS = r'''
#include <stdio.h>
#include <string.h>
#include "code/qcommon/game_profile.h"
typedef int qboolean;
enum { qfalse, qtrue, PRINT_ALL = 0 };
typedef enum { TC_NONE, TC_S3TC, TC_S3TC_ARB } textureCompression_t;
typedef struct { int integer; float value; } cvar_t;
static cvar_t enabled, preferred;
static cvar_t *r_ext_compressed_textures = &enabled;
static cvar_t *r_ext_preferred_tc_method;
static struct { textureCompression_t textureCompression; } glConfig;
static void Quiet(int level, const char *text, ...) { (void)level; (void)text; }
static struct { comGameId_t gameId; void (*Printf)(int, const char *, ...); } ri;
static int extensionMask;
static qboolean R_HaveExtension(const char *name) {
    if (!strcmp(name, "GL_S3_s3tc")) return (extensionMask & 1) != 0;
    if (!strcmp(name, "GL_ARB_texture_compression")) return (extensionMask & 2) != 0;
    if (!strcmp(name, "GL_EXT_texture_compression_s3tc")) return (extensionMask & 4) != 0;
    return qfalse;
}
HEADER
static void Initialize(void) {
    glConfig.textureCompression = TC_NONE;
    SELECTION
}
int main(void) {
    int expectedNative, rows = 0, cases = 0;
    ri.Printf = Quiet;
    while (scanf("%d%d%f%d", &extensionMask, &preferred.integer,
                 &enabled.value, &expectedNative) == 4) {
        int profile;
        enabled.integer = (int)enabled.value;
        for (profile = COM_GAME_QUAKE3; profile < COM_GAME_COUNT; ++profile) {
            const int elite = profile == COM_GAME_STVEF_SP || profile == COM_GAME_STVEF_MP;
            const int previous = !enabled.integer ? TC_NONE :
                (extensionMask & 6) == 6 ? TC_S3TC_ARB :
                (extensionMask & 1) ? TC_S3TC : TC_NONE;
            const int expected = elite ? expectedNative : previous;
            ri.gameId = (comGameId_t)profile;
            r_ext_preferred_tc_method = elite ? &preferred : NULL;
            /* Initialization must discard the previous renderer's capability. */
            glConfig.textureCompression = TC_S3TC_ARB;
            Initialize();
            if (glConfig.textureCompression != expected) {
                fprintf(stderr, "profile=%d extensions=%d preferred=%d enabled=%g: %d != %d\n",
                    profile, extensionMask, preferred.integer, enabled.value,
                    glConfig.textureCompression, expected);
                return 1;
            }
            ++cases;
        }
        ++rows;
    }
    if (rows != 224) return 2;
    printf("%d compression method selections passed\n", cases);
    return 0;
}
'''


class STVEFCompressionPolicyTests(unittest.TestCase):
    def test_native_methods_and_other_profile_controls(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        fixture = json.loads((ROOT/'tests/fixtures/stvef_compression_policy_retail.json').read_text())
        self.assertEqual(fixture['cases']['sp'], fixture['cases']['mp'])
        inputs = '\n'.join(' '.join(str(row[key]) for key in
            ('extensions', 'preferred', 'enabled', 'method')) for row in fixture['cases']['sp'])+'\n'
        sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-compression-') as folder:
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(renderer=renderer):
                    source = (ROOT/f'code/{renderer}/tr_init.c').read_text()
                    start = source.index('// GL_EXT_texture_compression_s3tc')
                    selection = source[start:source.index('// GL_EXT_texture_env_add', start)]
                    header = ('#include "code/renderercommon/tr_stvef_compression.h"'
                              if 'renderercommon/tr_stvef_compression.h"' in source else '')
                    path = Path(folder)/f'{renderer}.c'
                    path.write_text(HARNESS.replace('HEADER', header).replace('SELECTION', selection))
                    exe = path.with_suffix('.exe' if os.name == 'nt' else '')
                    built = subprocess.run([compiler, *compiler_target_args(compiler), *sanitizer,
                        '-std=c11', '-O1', '-I', str(ROOT), str(path), '-o', str(exe)],
                        cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    ran = subprocess.run([str(exe)], input=inputs, cwd=ROOT, capture_output=True,
                                         text=True, timeout=20, **options)
                    self.assertEqual(ran.returncode, 0, ran.stdout+ran.stderr)
                    # 224 per profile: 1568 when the harness first walked seven.
                    self.assertIn(f'{224 * profile_count()} compression method selections passed',
                                  ran.stdout)


if __name__ == '__main__':
    unittest.main()
