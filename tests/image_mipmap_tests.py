"""Execute texture upload and mip filters on square, rectangular and 1D images.

Every minified texture must reach one texel on both axes. Exact RGBA outputs
are checked against an independent separable filter, including the weighted
filter's wrapped one-pixel axis. GPU calls are replaced only at the final
OpenGL upload boundary; Vulkan's production upload buffer is read directly.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code/qcommon/game_profile.h"
#undef assert
#define assert(test) do { if (!(test)) { fprintf(stderr,"check failed at line %d: %s\n",__LINE__,#test); exit(1); } } while(0)
typedef unsigned char byte;
typedef int qboolean;
typedef unsigned imgFlags_t;
enum { qfalse, qtrue };
enum { IMGFLAG_MIPMAP=1, IMGFLAG_PICMIP=2, IMGFLAG_NOSCALE=4,
       IMGFLAG_COLORSHIFT=8, IMGFLAG_NOLIGHTSCALE=16, IMGFLAG_NO_COMPRESSION=32,
       IMGFLAG_LIGHTMAP=64, IMGFLAG_RTCW_CHARACTER_MIP=128 };
enum { ERR_DROP, MAX_TEXTURE_SIZE=16384 };
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))
#define Com_Memcpy memcpy
#define Com_Memset memset
typedef struct { int integer; } cvar_t;
static cvar_t simple={1}, picmip={0}, picmip2={0}, roundDown={0}, noMip={0}, colorMip={0};
static cvar_t *r_simpleMipMaps=&simple, *r_picmip=&picmip, *r_picmip2=&picmip2;
static cvar_t *r_roundImagesDown=&roundDown, *r_nomip=&noMip, *r_colorMipLevels=&colorMip;
static struct { int maxTextureSize; } glConfig;
static struct { int mapLoading; } tr;
typedef struct { int width,height,flags,internalFormat,uploadWidth,uploadHeight,srgbDecode; } image_t;
typedef struct { unsigned char *base; size_t size; } allocation_t;
static allocation_t allocations[64];
static int allocationCount;
static void *Allocate(int size) {
    unsigned char *base;
    assert(size>0 && allocationCount<64);
    base=(unsigned char *)malloc((size_t)size+32); assert(base);
    memset(base,0xa5,(size_t)size+32);
    allocations[allocationCount].base=base; allocations[allocationCount++].size=(size_t)size;
    return base+16;
}
static void Release(void *pointer) {
    allocation_t *entry; size_t n;
    assert(allocationCount>0); entry=&allocations[allocationCount-1];
    assert(pointer==entry->base+16);
    for(n=0;n<16;++n) assert(entry->base[n]==0xa5 && entry->base[16+entry->size+n]==0xa5);
    free(entry->base); --allocationCount;
}
static void Error(int code,const char *text) { (void)code; fprintf(stderr,"%s\n",text); exit(1); }
static struct { void *(*Hunk_AllocateTempMemory)(int); void (*Hunk_FreeTempMemory)(void *);
    void (*Error)(int,const char *); comGameId_t gameId; } ri={Allocate,Release,Error,COM_GAME_QUAKE3};
static void R_LightScaleTexture(byte *p,int w,int h,int gamma) { (void)p;(void)w;(void)h;(void)gamma; }
static void R_ColorShiftLightingBytes(byte *in,byte *out,int hdr) { (void)in;(void)out;(void)hdr; }
static void R_BlendOverTexture(byte *p,int n,int level) { (void)p;(void)n;(void)level; assert(0); }
static int RawImage_GetInternalFormat(const byte *p,int count,int lightmap,int compress,int srgb) {
    (void)p;(void)count;(void)lightmap;(void)compress;(void)srgb; return 777;
}
static void GL_CheckErrors(void) {}
typedef struct { byte *buffer; int buffer_size,mip_levels,base_level_width,base_level_height; } Image_Upload_Data;
static Image_Upload_Data captured;
static int uploadedWidths[32],uploadedHeights[32];
static void LoadTexture(int level,int x,int y,int w,int h,const byte *p,int sub,image_t *image) {
    (void)x;(void)y;(void)image; assert(!sub && level==captured.mip_levels && level<32);
    if(!level) { captured.base_level_width=w; captured.base_level_height=h; }
    assert(p && w>0 && h>0);
    memcpy(captured.buffer+captured.buffer_size,p,(size_t)w*h*4);
    captured.buffer_size+=w*h*4; ++captured.mip_levels;
    uploadedWidths[level]=w; uploadedHeights[level]=h;
}
#include "code/renderercommon/tr_rtcw_image_mip.h"
PRODUCTION

static void Downsample(const byte *source,byte *dest,int w,int h,int weighted) {
    static const int weights[4]={1,2,2,1};
    int x,y,c,a,b;
    for(y=0;y<MAX(1,h/2);++y) for(x=0;x<MAX(1,w/2);++x) for(c=0;c<4;++c) {
        unsigned total=0,divisor=0;
        if(weighted) {
            for(a=0;a<4;++a) for(b=0;b<4;++b) {
                int row=(2*y+a+h-1)%h, col=(2*x+b+w-1)%w;
                unsigned weight=(unsigned)(weights[a]*weights[b]);
                total+=weight*source[(row*w+col)*4+c]; divisor+=weight;
            }
        } else {
            for(a=0;a<(h>1?2:1);++a) for(b=0;b<(w>1?2:1);++b) {
                total+=source[((2*y+a)*w+2*x+b)*4+c]; ++divisor;
            }
        }
        dest[(y*MAX(1,w/2)+x)*4+c]=(byte)(total/divisor);
    }
}
static unsigned cases;
static void Case(int width,int height,int mip,int quality,int level,int limit,int nullData) {
    image_t image={width,height,IMGFLAG_NOLIGHTSCALE,0,0,0,0};
    const size_t inputBytes=(size_t)width*height*4;
    byte *pixels=(byte *)malloc(inputBytes), *expected=(byte *)malloc(inputBytes), *next=(byte *)malloc(inputBytes);
    Image_Upload_Data upload; size_t n; int w=width,h=height,offset=0,levels=0;
    assert(pixels && expected && next);
    for(n=0;n<inputBytes;++n) pixels[n]=nullData==1?0:nullData==2?(byte)(19+(n%4)*61):(byte)((n*73+n/7+19)&255);
    memcpy(expected,pixels,inputBytes);
    if(mip)image.flags|=IMGFLAG_MIPMAP;
    if(level)image.flags|=IMGFLAG_PICMIP;
    simple.integer=quality; picmip.integer=level; roundDown.integer=0;
    glConfig.maxTextureSize=limit; tr.mapLoading=1;
    while(w>limit || h>limit) {
        /* Upload's size-limit resampler picks the middle four samples.
         * Test solid colors here so independent dimension/bounds checks do
         * not duplicate that separate resampling algorithm. */
        assert(nullData); w=MAX(1,w/2); h=MAX(1,h/2);
    }
    if(level) {
        int remaining=level;
        while(remaining-- && (w>1 || h>1)) {
            Downsample(expected,next,w,h,!quality);
            memcpy(expected,next,(size_t)MAX(1,w/2)*MAX(1,h/2)*4);
            w=MAX(1,w/2); h=MAX(1,h/2);
        }
    }
    UPLOAD
    assert(upload.base_level_width==w && upload.base_level_height==h);
    for(;;) {
        const size_t bytes=(size_t)w*h*4;
        if(levels>=upload.mip_levels || offset+(int)bytes>upload.buffer_size ||
            memcmp(upload.buffer+offset,expected,bytes)) {
            fprintf(stderr,"case %u profile %d %dx%d mip %d quality %d picmip %d limit %d null %d: "
                "level %d (%dx%d), got %d levels / %d bytes\n",cases,ri.gameId,width,height,mip,quality,level,limit,nullData,
                levels,w,h,upload.mip_levels,upload.buffer_size);
            exit(1);
        }
        ++levels; offset+=(int)bytes;
        if(!mip || (w==1 && h==1)) break;
        Downsample(expected,next,w,h,!quality);
        w=MAX(1,w/2); h=MAX(1,h/2);
        memcpy(expected,next,(size_t)w*h*4);
    }
    assert(levels==upload.mip_levels && offset==upload.buffer_size);
    RELEASE
    assert(allocationCount==0);
    free(next); free(expected); free(pixels); ++cases;
}
int main(void) {
    static const int dims[][2]={{1,1},{1,2},{2,1},{1,16},{16,1},{2,32},{32,2},{4,8},{8,4},{4,4},{32,32},{64,128}};
    unsigned d; int profile,mip,quality,level;
    Case(1,16,1,1,0,256,0);
    for(profile=COM_GAME_QUAKE3;profile<COM_GAME_COUNT;++profile) {
        ri.gameId=(comGameId_t)profile;
        for(d=0;d<ARRAY_LEN(dims);++d) for(mip=0;mip<2;++mip) for(quality=0;quality<2;++quality)
            for(level=0;level<3;++level) Case(dims[d][0],dims[d][1],mip,quality,level,256,0);
        /* Thin images beyond the device limit must retain a one-pixel axis;
         * null texture initialization must also fill every declared level. */
        for(quality=0;quality<2;++quality) {
            Case(1,64,1,quality,0,8,1); Case(64,1,1,quality,0,8,1);
            Case(4,64,1,quality,1,8,1); Case(64,4,1,quality,1,8,1);
            Case(1,64,1,quality,0,8,2); Case(64,1,1,quality,0,8,2);
            Case(4,64,1,quality,1,8,2); Case(64,4,1,quality,1,8,2);
        }
    }
    printf("%u exact mip-chain / filter / device-limit cases passed\n",cases);
}
'''


class ImageMipmapTests(unittest.TestCase):
    def test_complete_upload_chains(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        for renderer in ('renderervk', 'rendererrtx', 'renderer'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='image-mips-') as folder:
                production = (ROOT / f'code/{renderer}/tr_image.c').read_text()
                functions = '\n'.join(function(production, signature) for signature in (
                    'static void ResampleTexture(', 'static void R_MipMap2(', 'static void R_MipMap(',
                    'static void Upload32(' if renderer == 'renderer' else 'static void generate_image_upload_data('))
                source = HARNESS.replace('PRODUCTION', functions)
                if renderer == 'renderer':
                    source = source.replace('UPLOAD', '''
    memset(&captured,0,sizeof(captured));
    captured.buffer=(byte *)malloc(inputBytes*2); assert(captured.buffer);
    Upload32(pixels,0,0,width,height,&image,qfalse); upload=captured;
''').replace('RELEASE', 'free(captured.buffer);')
                else:
                    source = source.replace('UPLOAD', 'generate_image_upload_data(&image,nullData==1?NULL:pixels,&upload);')
                    source = source.replace('RELEASE', 'Release(upload.buffer);')
                cpp = Path(folder) / 'probe.c'; exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
                cpp.write_text(source)
                sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
                built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c11', '-O1',
                    '-D_CRT_SECURE_NO_WARNINGS', *sanitizer, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                    cwd=ROOT, capture_output=True, text=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
