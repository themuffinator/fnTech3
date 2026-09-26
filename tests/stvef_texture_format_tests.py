"""Execute image format selection against the retail SP/Holomatch observations."""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from image_mipmap_tests import function

ROOT = Path(__file__).resolve().parents[1]


def profile_count():
    """The profiles from COM_GAME_QUAKE3 up to COM_GAME_COUNT, as the harness walks them."""
    header = (ROOT / 'code/qcommon/game_profile.h').read_text(encoding='utf-8')
    body = header[header.index('COM_GAME_AUTO = 0,'):header.index('COM_GAME_COUNT')]
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    return len(re.findall(r'\bCOM_GAME_\w+', body)) - 1


HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code/qcommon/game_profile.h"
typedef unsigned char byte;
typedef int qboolean, GLint;
enum { qfalse, qtrue };
enum { TC_NONE, TC_S3TC, TC_S3TC_ARB };
enum { GL_RGB=0x1907, GL_RGBA=0x1908, GL_RGB5=0x8050, GL_RGB8=0x8051,
       GL_RGBA4=0x8056, GL_RGBA8=0x8058, GL_RGB4_S3TC=0x83a1,
       GL_COMPRESSED_RGB_S3TC_DXT1_EXT=0x83f0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT=0x83f3,
       GL_SRGB8=0x8c41, GL_SRGB8_ALPHA8=0x8c43 };
static struct { int textureCompression; } glConfig;
static struct { comGameId_t gameId; } ri;
typedef struct { int integer; } cvar_t;
static cvar_t bits;
static cvar_t *r_texturebits=&bits;
static cvar_t lightmapBits, lightmapCompression;
static cvar_t *r_texturebitslm=&lightmapBits, *r_ext_compress_lightmaps=&lightmapCompression;
HAS_ALPHA
FORMAT
static int Normalize(int format) {
    /* Native uses component counts for its unsized RGB/RGBA requests. */
    return format==3 ? GL_RGB : format==4 ? GL_RGBA : format;
}
int main(void) {
    int compression,textureBits,alpha,native,rows=0,count=0;
    while(scanf("%d%d%d%d",&compression,&textureBits,&alpha,&native)==4) {
        int profile,allow,variant,lightmap,srgb;
        bits.integer=textureBits; glConfig.textureCompression=compression;
        for(profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile)
        for(allow=0;allow<2;++allow) for(variant=0;variant<4;++variant)
        for(lightmap=0;lightmap<2;++lightmap) for(srgb=0;srgb<SRGB_MODES;++srgb) {
            byte pixels[16],before[16];const byte *scan=pixels;
            int actual,expected;
            const int elite=profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP;
            ri.gameId=(comGameId_t)profile;
            memset(pixels,255,sizeof(pixels));
            if(alpha) {
                if(variant==3) scan=NULL;
                else pixels[variant?15:3]=(byte)(variant==2?254:0);
            }
            memcpy(before,pixels,sizeof(pixels));
            expected=Normalize(native);
            if(!allow || (!elite && alpha)) {
                if(alpha) expected=textureBits==16?GL_RGBA4:textureBits==32?GL_RGBA8:GL_RGBA;
                else expected=textureBits==16?GL_RGB5:textureBits==32?GL_RGB8:GL_RGB;
            }
            if(srgb) expected=alpha?GL_SRGB8_ALPHA8:GL_SRGB8;
            if(lightmap) expected=elite&&textureBits==16?GL_RGB5:elite&&textureBits==32?GL_RGB8:GL_RGB;
            actual=CALL;
            if(actual!=expected || memcmp(pixels,before,sizeof(pixels))) {
                fprintf(stderr,"profile=%d tc=%d bits=%d alpha=%d allow=%d variant=%d lm=%d srgb=%d: %x != %x\n",
                    profile,compression,textureBits,alpha,allow,variant,lightmap,srgb,actual,expected);
                return 1;
            }
            ++count;
        }
        ++rows;
    }
    if(rows!=36) return 2;
    printf("%d image format selections passed\n",count);
    return 0;
}
'''


class STVEFTextureFormatTests(unittest.TestCase):
    def test_retail_formats_and_other_profile_controls(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        fixture = json.loads((ROOT/'tests/fixtures/stvef_texture_format_retail.json').read_text())
        self.assertEqual(fixture['cases']['sp'], fixture['cases']['mp'])
        inputs = '\n'.join(' '.join(str(row[key]) for key in
            ('compression', 'textureBits', 'alpha', 'format')) for row in fixture['cases']['sp'])+'\n'
        sanitizer = [] if os.name=='nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-formats-') as folder:
            for renderer in ('renderer', 'renderervk', 'rendererrtx'):
                with self.subTest(renderer=renderer):
                    source = (ROOT/f'code/{renderer}/tr_image.c').read_text()
                    srgb = renderer=='renderer'
                    harness = (HARNESS.replace('HAS_ALPHA', function(source, 'static qboolean RawImage_HasAlpha('))
                        .replace('FORMAT', function(source, 'static GLint RawImage_GetInternalFormat('))
                        .replace('SRGB_MODES', '2' if srgb else '1')
                        .replace('CALL', 'RawImage_GetInternalFormat(scan,4,lightmap,allow'+(',srgb)' if srgb else ')')))
                    path = Path(folder)/f'{renderer}.c';path.write_text(harness)
                    exe = path.with_suffix('.exe' if os.name=='nt' else '')
                    built = subprocess.run([compiler, *compiler_target_args(compiler), *sanitizer,
                        '-std=c11', '-O1', '-I', str(ROOT), str(path), '-o', str(exe)],
                        cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
                    ran = subprocess.run([str(exe)], input=inputs, cwd=ROOT, capture_output=True,
                                         text=True, timeout=20, **options)
                    self.assertEqual(ran.returncode, 0, ran.stdout+ran.stderr)
                    # 1152 or 576 per profile: 8064 and 4032 when the harness first walked seven.
                    self.assertIn(f'{(1152 if srgb else 576) * profile_count()} image format selections passed',
                                  ran.stdout)


if __name__=='__main__':
    unittest.main()
