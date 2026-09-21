"""Exercise retail lightmap format controls and actual Vulkan upload selection."""
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
from stvef_texture_format_tests import HARNESS as FORMAT_HARNESS

ROOT = Path(__file__).resolve().parents[1]
PREAMBLE = FORMAT_HARNESS.split('HAS_ALPHA')[0] + r'''
#define compression lightmapCompression
'''
GL_MAIN = r'''
enum { IMGFLAG_NO_COMPRESSION=16, IMGFLAG_NOLIGHTSCALE=32, IMGFLAG_LIGHTMAP=64,
       IMGFLAG_NOSCALE=128, IMGFLAG_COLORSPACE_LINEAR=2048, IMGFLAG_CLAMPTOBORDER=8,
       IMGFLAG_CLAMPTOEDGE=4 };
FLAGS
FLAGS_HELPER
int main(void) {
    int tc,tb,lb,compress,native,rows=0,cases=0;
    while(scanf("%d%d%d%d%d",&tc,&tb,&lb,&compress,&native)==5) {
        int profile,allow,variant,srgb;
        glConfig.textureCompression=tc; bits.integer=tb;
        lightmapBits.integer=lb; compression.integer=compress;
        for(profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile)
        for(allow=0;allow<2;++allow) for(variant=0;variant<3;++variant)
        for(srgb=0;srgb<SRGB_MODES;++srgb) {
            const int elite=profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP;
            const int selectedBits=lb>0?lb:tb;
            int expected,actual,mergedFlags,separateFlags;
            byte pixels[16]; const byte *scan=variant==2?NULL:pixels;
            ri.gameId=(comGameId_t)profile;
            r_texturebitslm=elite?&lightmapBits:NULL;
            r_ext_compress_lightmaps=elite?&compression:NULL;
            memset(pixels,variant?0:255,sizeof(pixels));
            expected=native==3?GL_RGB:native;
            if(!allow) expected=selectedBits==16?GL_RGB5:selectedBits==32?GL_RGB8:GL_RGB;
            if(!elite) expected=GL_RGB;
            actual=CALL;
            if(actual!=expected) {
                fprintf(stderr,"profile=%d tc=%d bits=%d lmBits=%d compress=%d allow=%d data=%d srgb=%d: %x != %x\n",
                    profile,tc,tb,lb,compress,allow,variant,srgb,actual,expected); return 1;
            }
            /* Compile the flags expressions used by both BSP upload paths. */
            mergedFlags=MERGED_FLAGS; separateFlags=SEPARATE_FLAGS;
            if(!!(mergedFlags&IMGFLAG_NO_COMPRESSION)!=(elite&&compress?0:1) ||
               !!(separateFlags&IMGFLAG_NO_COMPRESSION)!=(elite&&compress?0:1) ||
               !(mergedFlags&IMGFLAG_LIGHTMAP) || !(separateFlags&IMGFLAG_NOSCALE)) return 2;
            ++cases;
        }
        ++rows;
    }
    if(rows!=324) return 3;
    printf("%d lightmap format/flag selections passed\n",cases); return 0;
}
'''
VK_MAIN = r'''
enum { IMGFLAG_LIGHTMAP=64, VK_FORMAT_R8G8B8A8_SRGB=43,
       VK_FORMAT_R8G8B8A8_UNORM=37, VK_FORMAT_B4G4R4A4_UNORM_PACK16=3,
       VK_FORMAT_A1R5G5B5_UNORM_PACK16=8 };
typedef struct { int flags,srgbDecode,internalFormat,uploadWidth,uploadHeight; } image_t;
typedef struct { byte *buffer; int base_level_width,base_level_height,mip_levels,buffer_size; } Image_Upload_Data;
static byte *allocated;
static void Release(void *p) { if(p!=allocated) abort(); free(p); allocated=NULL; }
static void generate_image_upload_data(image_t *image,byte *pic,Image_Upload_Data *out) {
    (void)image;
    out->buffer=allocated=(byte *)malloc(16); if(!allocated) abort();
    if(pic) memcpy(allocated,pic,16); else memset(allocated,0,16);
    out->base_level_width=2; out->base_level_height=2; out->mip_levels=1; out->buffer_size=16;
}
static void vk_create_image(image_t *image,int w,int h,int mips) {
    if(!image->internalFormat || w!=2 || h!=2 || mips!=1) abort();
}
static void vk_upload_image_data(image_t *image,int x,int y,int w,int h,int mips,byte *data,int size,int update) {
    if(image->uploadWidth!=2 || image->uploadHeight!=2 || x || y || w!=2 || h!=2 || mips!=1 || data!=allocated || size!=16 || update) abort();
}
UPLOAD
int main(void) {
    const int values[]={-1,0,8,16,24,32};
    int b,l,profile,lightmap,variant,srgb,cases=0;
    ri.Hunk_FreeTempMemory=Release;
    for(b=0;b<6;++b) for(l=0;l<6;++l)
    for(profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile)
    for(lightmap=0;lightmap<2;++lightmap) for(variant=0;variant<3;++variant)
    for(srgb=0;srgb<2;++srgb) {
        const int elite=profile==COM_GAME_STVEF_SP || profile==COM_GAME_STVEF_MP;
        image_t image={0}; byte pixels[16]; int expected,effective;
        ri.gameId=(comGameId_t)profile;
        bits.integer=values[b];lightmapBits.integer=values[l];
        r_texturebitslm=elite?&lightmapBits:NULL;
        image.flags=lightmap?IMGFLAG_LIGHTMAP:0;image.srgbDecode=srgb;
        memset(pixels,variant?0:255,sizeof(pixels));
        effective=values[l]>0?values[l]:values[b];
        if(lightmap) expected=elite&&effective==16?VK_FORMAT_A1R5G5B5_UNORM_PACK16:VK_FORMAT_R8G8B8A8_UNORM;
        else expected=values[b]>16||values[b]==0?VK_FORMAT_R8G8B8A8_UNORM:
             variant?VK_FORMAT_B4G4R4A4_UNORM_PACK16:VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        if(srgb) expected=VK_FORMAT_R8G8B8A8_SRGB;
        upload_vk_image(&image,variant==2?NULL:pixels);
        if(image.internalFormat!=expected || allocated) {
            fprintf(stderr,"profile=%d lightmap=%d bits=%d lmBits=%d data=%d srgb=%d: %d != %d\n",
                profile,lightmap,values[b],values[l],variant,srgb,image.internalFormat,expected);return 1;
        }
        ++cases;
    }
    printf("%d Vulkan lightmap/upload selections passed\n",cases);return 0;
}
'''


class STVEFLightmapFormatTests(unittest.TestCase):
    def compile_run(self, harness, inputs, expected):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        with tempfile.TemporaryDirectory(prefix='stvef-lightmaps-') as folder:
            path = Path(folder)/'probe.c';path.write_text(harness)
            exe = path.with_suffix('.exe' if os.name == 'nt' else '')
            built = subprocess.run([compiler, *compiler_target_args(compiler), *sanitizer,
                '-std=c11', '-O1', '-I', str(ROOT), str(path), '-o', str(exe)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], input=inputs, cwd=ROOT, capture_output=True,
                                 text=True, timeout=20, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout+ran.stderr)
            self.assertIn(expected, ran.stdout)

    def test_native_lightmaps_and_bsp_compression_flags(self):
        fixture = json.loads((ROOT/'tests/fixtures/stvef_lightmap_format_retail.json').read_text())
        self.assertEqual(fixture['cases']['sp'], fixture['cases']['mp'])
        inputs = '\n'.join(' '.join(str(row[key]) for key in
            ('compression', 'textureBits', 'lightmapBits', 'compressLightmaps', 'format'))
            for row in fixture['cases']['sp'])+'\n'
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                image = (ROOT/f'code/{renderer}/tr_image.c').read_text()
                bsp = (ROOT/f'code/{renderer}/tr_bsp.c').read_text()
                srgb = renderer == 'renderer'
                flags = re.search(r'static const int lightmapFlags\s*=[^;]+;', bsp)[0]
                helper = (function(bsp, 'static int R_LightmapImageFlags(')
                          if 'static int R_LightmapImageFlags(' in bsp else '')
                main = GL_MAIN.replace('FLAGS_HELPER', helper).replace('FLAGS\n', flags+'\n')
                for name, function_name in [('MERGED_FLAGS', 'R_LoadMergedLightmaps'), ('SEPARATE_FLAGS', 'R_LoadLightmaps')]:
                    load = function(bsp, 'static void '+function_name+'(')
                    expression = re.search(r'(R_LightmapImageFlags\(\)|lightmapFlags)\s*\|\s*IMGFLAG_CLAMPTO\w+', load)[0]
                    main = main.replace(name, expression)
                main = (main.replace('SRGB_MODES', '2' if srgb else '1')
                        .replace('CALL', 'RawImage_GetInternalFormat(scan,4,1,allow'+(',srgb)' if srgb else ')')))
                harness = (PREAMBLE+function(image, 'static qboolean RawImage_HasAlpha(')+
                           function(image, 'static GLint RawImage_GetInternalFormat(')+main)
                self.compile_run(harness, inputs, f'{27216 if srgb else 13608} lightmap format/flag selections passed')

    def test_vulkan_lightmap_precision_and_other_image_controls(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                source = (ROOT/f'code/{renderer}/tr_image.c').read_text()
                prefix = PREAMBLE.replace('comGameId_t gameId;', 'comGameId_t gameId; void (*Hunk_FreeTempMemory)(void *);')
                harness = (prefix+function(source, 'static qboolean RawImage_HasAlpha(')+
                           VK_MAIN.replace('UPLOAD', function(source, 'static void upload_vk_image(')))
                self.compile_run(harness, '', '3024 Vulkan lightmap/upload selections passed')


if __name__ == '__main__':
    unittest.main()
