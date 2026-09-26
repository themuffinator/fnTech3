"""SP raw-image brightness, retail geometry and stable queued image contents.

Retail's RE_StretchRaw (stvoy.exe 0x44fb50) uploads the image and draws it
immediately, so one scratch image was always enough and the brightness the SP
UI passes reaches glColor3f unchanged. This engine queues 2D geometry instead,
so a shared image would show every queued preview the last upload's pixels.
The checks replay the recorded native cases against the shared raw-image path
and pin the per-frame image pool that keeps queued previews independent.
"""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from spawn_view_angle_source_tests import function_body

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/stvef_sp_raw_image_retail.json'
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
CASES = json.loads(FIXTURE.read_text())['cases']


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def literal(value):
    """An exact C float literal for a recorded native value."""
    return repr(float(value)) + 'f'


def shade(light):
    """The byte the engine's canonical quad carries for a native glColor3f."""
    return int(min(max(float(light), 0.0), 1.0) * 255.0 + 0.5)


def case_table():
    rows = []
    for case in CASES:
        x, y, w, h = case['rectangle']
        cols, height = case['size']
        corners = case['vertices']
        xy = ', '.join('{%s, %s}' % (literal(c['xy'][0]), literal(c['xy'][1])) for c in corners)
        st = ', '.join('{%s, %s}' % (literal(c['st'][0]), literal(c['st'][1])) for c in corners)
        rows.append('\t{ %d, %d, %d, %d, %d, %d, %d, %d, %s, {%s}, {%s} },'
                    % (x, y, w, h, cols, height, case['resized'], shade(case['light']),
                       literal(case['light']), xy, st))
    return '\n'.join(rows)


PROBE = r'''
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef isfinite
using std::isfinite;
#endif

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

typedef unsigned char byte;
typedef int qboolean;
enum { qfalse = 0, qtrue = 1 };
typedef int qhandle_t;
typedef int imgFlags_t;
typedef float vec2_t[2];
typedef union { byte rgba[4]; unsigned int u32; } color4ub_t;
typedef struct {
	vec2_t xy[4], st[4];
	color4ub_t colors[4];
	unsigned int colorMask;
	qhandle_t shader;
} refDrawQuad_t;

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))
#define Com_Memset memset

enum { IMGFLAG_CLAMPTOEDGE = 1, IMGFLAG_RGB = 2, IMGFLAG_NOSCALE = 4, IMGFLAG_NO_COMPRESSION = 8 };
enum { RAW_FLAGS = IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE | IMGFLAG_NO_COMPRESSION };
enum { GL_TEXTURE_2D = 0x0DE1, GL_RGBA = 0x1908, GL_UNSIGNED_BYTE = 0x1401, GL_LINEAR = 0x2601,
	GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_MIN_FILTER = 0x2801,
	GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803 };
enum { COM_GAME_STVEF_SP = 7, COM_GAME_STVEF_HM = 8 };
enum { ERR_DROP = 1 };
enum { POOL = 32, MAX_PIXELS = 64 * 64 * 4 };

struct image_t {
	char name[64];
	int width, height, uploadWidth, uploadHeight, internalFormat;
	int binds;
	byte pixels[MAX_PIXELS];
};
struct shader_t { int index; image_t *image; };

static image_t images[POOL + 4];
static shader_t shaders[POOL + 4];
static refDrawQuad_t quads[128];
static int imagesCreated, shadersCreated, quadCount, errors;
static int fullUploads, subUploads, paramSets;
static image_t *bound;
static int gl_clamp_mode = 0x812F;

static struct {
	qboolean registered;
	int frameCount;
	image_t *stvefRawImages[POOL];
	shader_t *stvefRawShaders[POOL];
	int stvefRawFrame, stvefRawUsed;
} tr;

static struct { int maxTextureSize; } glConfig;

static void Error( int level, const char *format, ... ) { (void)level; (void)format; ++errors; }

static struct {
	int gameId;
	void (*Error)( int level, const char *format, ... );
} ri = { COM_GAME_STVEF_SP, Error };

static const char *va( const char *format, ... ) {
	static char buffers[4][64];
	static int index;
	char *out = buffers[index++ & 3];
	va_list arguments;
	va_start( arguments, format );
	std::vsnprintf( out, sizeof( buffers[0] ), format, arguments );
	va_end( arguments );
	return out;
}

static void Store( image_t *image, const byte *data, int cols, int rows ) {
	CHECK( cols > 0 && rows > 0 && cols * rows * 4 <= MAX_PIXELS && data != NULL );
	std::memcpy( image->pixels, data, (size_t)cols * (size_t)rows * 4 );
}

static image_t *R_CreateImage( const char *name, const char *name2, byte *pic,
	int width, int height, imgFlags_t flags ) {
	CHECK( name2 == NULL && flags == RAW_FLAGS );
	CHECK( imagesCreated < (int)ARRAY_LEN( images ) );
	image_t *image = &images[imagesCreated++];
	std::snprintf( image->name, sizeof( image->name ), "%s", name );
	image->width = image->uploadWidth = width;
	image->height = image->uploadHeight = height;
	image->internalFormat = GL_RGBA;
	++fullUploads;
	Store( image, pic, width, height );
	return image;
}

static void GL_Bind( image_t *image ) { bound = image; image->binds++; }

static void qglTexImage2D( int target, int level, int internalFormat, int width, int height,
	int border, int format, int type, const void *data ) {
	CHECK( target == GL_TEXTURE_2D && level == 0 && border == 0 );
	CHECK( internalFormat == GL_RGBA && format == GL_RGBA && type == GL_UNSIGNED_BYTE );
	CHECK( bound != NULL );
	++fullUploads;
	Store( bound, (const byte *)data, width, height );
}

static void qglTexSubImage2D( int target, int level, int x, int y, int width, int height,
	int format, int type, const void *data ) {
	CHECK( target == GL_TEXTURE_2D && level == 0 && x == 0 && y == 0 );
	CHECK( format == GL_RGBA && type == GL_UNSIGNED_BYTE && bound != NULL );
	++subUploads;
	Store( bound, (const byte *)data, width, height );
}

static void qglTexParameteri( int target, int name, int value ) {
	CHECK( target == GL_TEXTURE_2D && bound != NULL );
	(void)name; (void)value;
	++paramSets;
}

static void vk_create_image( image_t *image, int width, int height, int mipLevels ) {
	CHECK( mipLevels == 1 );
	image->binds++;
	image->width = image->uploadWidth = width;
	image->height = image->uploadHeight = height;
}

static void vk_upload_image_data( image_t *image, int x, int y, int width, int height,
	int mipLevels, byte *pixels, int size, qboolean update ) {
	CHECK( x == 0 && y == 0 && mipLevels == 1 && size == width * height * 4 );
	if ( update ) ++subUploads; else ++fullUploads;
	Store( image, pixels, width, height );
}

shader_t *R_CreateSTVEFRawShader( const char *name, image_t *image ) {
	CHECK( shadersCreated < (int)ARRAY_LEN( shaders ) );
	CHECK( name != NULL && image != NULL );
	shader_t *shader = &shaders[shadersCreated];
	shader->index = 100 + shadersCreated++;
	shader->image = image;
	return shader;
}

void RE_DrawQuad( const refDrawQuad_t *quad ) {
	CHECK( quadCount < (int)ARRAY_LEN( quads ) );
	quads[quadCount++] = *quad;
}

#include "code/renderercommon/tr_stvef_raw.h"

static image_t *DrawnImage( const refDrawQuad_t *quad ) {
	for ( int i = 0; i < shadersCreated; ++i ) {
		if ( shaders[i].index == quad->shader ) return shaders[i].image;
	}
	CHECK( 0 );
	return NULL;
}

static void CheckColors( const refDrawQuad_t *quad, int value ) {
	CHECK( quad->colorMask == 15 );
	for ( int corner = 0; corner < 4; ++corner ) {
		CHECK( quad->colors[corner].rgba[0] == value );
		CHECK( quad->colors[corner].rgba[1] == value );
		CHECK( quad->colors[corner].rgba[2] == value );
		CHECK( quad->colors[corner].rgba[3] == 255 );
	}
}

struct NativeCase {
	int x, y, w, h, cols, rows, resized, shade;
	float light;
	float xy[4][2], st[4][2];
};

static const NativeCase cases[] = {
'''

MAIN = r'''
};

int main( void ) {
	static byte buffer[MAX_PIXELS];
	tr.registered = qtrue;
	glConfig.maxTextureSize = 4096;

	/* Retail drew one image per frame from a single scratch texture, so one
	 * draw per frame replays its upload decisions slot for slot. */
	for ( size_t index = 0; index < ARRAY_LEN( cases ); ++index ) {
		const NativeCase *c = &cases[index];
		const int fullBefore = fullUploads, subBefore = subUploads, paramsBefore = paramSets;
		const int quadsBefore = quadCount, errorsBefore = errors;
		for ( int byteIndex = 0; byteIndex < c->cols * c->rows * 4; ++byteIndex ) {
			buffer[byteIndex] = (byte)( index * 7 + byteIndex );
		}
		tr.frameCount++;
		RE_DrawSTVEFRaw( c->x, c->y, c->w, c->h, c->cols, c->rows, buffer, c->light );
		CHECK( errors == errorsBefore && quadCount == quadsBefore + 1 );
		const refDrawQuad_t *quad = &quads[quadCount - 1];
		for ( int corner = 0; corner < 4; ++corner ) {
			CHECK( quad->xy[corner][0] == c->xy[corner][0] );
			CHECK( quad->xy[corner][1] == c->xy[corner][1] );
			CHECK( quad->st[corner][0] == c->st[corner][0] );
			CHECK( quad->st[corner][1] == c->st[corner][1] );
		}
		CheckColors( quad, c->shade );
		CHECK( tr.stvefRawUsed == 1 && quad->shader == shaders[0].index );
		CHECK( fullUploads - fullBefore == c->resized );
		CHECK( subUploads - subBefore == ( c->resized ? 0 : 1 ) );
#ifdef USE_VULKAN
		CHECK( paramSets == 0 );
		(void)paramsBefore;
#else
		/* Retail reset the four sampler parameters whenever it resized. */
		CHECK( paramSets - paramsBefore == ( c->resized && index != 0 ? 4 : 0 ) );
#endif
		CHECK( std::memcmp( DrawnImage( quad )->pixels, buffer, (size_t)c->cols * c->rows * 4 ) == 0 );
	}
	CHECK( imagesCreated == 1 && shadersCreated == 1 );

	/* Several previews in one frame keep their own pixels: the quads are still
	 * queued when the next upload arrives. */
	{
		const byte fills[3] = { 0x11, 0x22, 0x33 };
		const int quadsBefore = quadCount;
		tr.frameCount++;
		for ( int draw = 0; draw < 3; ++draw ) {
			std::memset( buffer, fills[draw], 16 * 8 * 4 );
			RE_DrawSTVEFRaw( 10 * draw, 20, 30, 40, 16, 8, buffer, 1.0f );
		}
		CHECK( quadCount == quadsBefore + 3 && tr.stvefRawUsed == 3 && errors == 0 );
		for ( int draw = 0; draw < 3; ++draw ) {
			const refDrawQuad_t *quad = &quads[quadsBefore + draw];
			image_t *image = DrawnImage( quad );
			CHECK( image->pixels[0] == fills[draw] );
			CHECK( image->pixels[16 * 8 * 4 - 1] == fills[draw] );
			CheckColors( quad, 255 );
			for ( int other = 0; other < draw; ++other ) {
				CHECK( quad->shader != quads[quadsBefore + other].shader );
				CHECK( image != DrawnImage( &quads[quadsBefore + other] ) );
			}
		}
	}

	/* The pool is reused next frame rather than regrown. */
	{
		image_t *slot = tr.stvefRawImages[0];
		const int created = imagesCreated, registered = shadersCreated;
		tr.frameCount++;
		std::memset( buffer, 0x44, 16 * 8 * 4 );
		RE_DrawSTVEFRaw( 1, 2, 3, 4, 16, 8, buffer, 1.0f );
		CHECK( tr.stvefRawUsed == 1 && tr.stvefRawImages[0] == slot && slot->pixels[0] == 0x44 );
		CHECK( imagesCreated == created && shadersCreated == registered );
	}

	/* A menu that draws past the pool keeps running on the last slot. */
	{
		const int quadsBefore = quadCount, errorsBefore = errors;
		tr.frameCount++;
		for ( int draw = 0; draw < POOL + 2; ++draw ) {
			std::memset( buffer, (byte)draw, 16 * 8 * 4 );
			RE_DrawSTVEFRaw( draw, 0, 8, 8, 16, 8, buffer, 1.0f );
		}
		CHECK( errors == errorsBefore && quadCount == quadsBefore + POOL + 2 );
		CHECK( tr.stvefRawUsed == POOL );
		CHECK( quads[quadsBefore + POOL - 1].shader == quads[quadCount - 1].shader );
		CHECK( tr.stvefRawImages[POOL - 1]->pixels[0] == (byte)( POOL + 1 ) );
	}

	/* Unusable images are refused, and the path belongs to single player. */
	{
		const int quadsBefore = quadCount, errorsBefore = errors;
		tr.frameCount++;
		std::memset( buffer, 0x55, 16 * 8 * 4 );
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 16, 8, NULL, 1.0f );
		CHECK( errors == errorsBefore + 1 && quadCount == quadsBefore );
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 0, 8, buffer, 1.0f );
		CHECK( errors == errorsBefore + 2 && quadCount == quadsBefore );
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 8192, 8, buffer, 1.0f );
		CHECK( errors == errorsBefore + 3 && quadCount == quadsBefore );
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 16, 8, buffer, std::nanf( "" ) );
		CHECK( errors == errorsBefore + 4 && quadCount == quadsBefore );
		ri.gameId = COM_GAME_STVEF_HM;
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 16, 8, buffer, 1.0f );
		ri.gameId = COM_GAME_STVEF_SP;
		tr.registered = qfalse;
		RE_DrawSTVEFRaw( 0, 0, 8, 8, 16, 8, buffer, 1.0f );
		tr.registered = qtrue;
		CHECK( errors == errorsBefore + 4 && quadCount == quadsBefore );
	}
	return 0;
}
'''


class STVEFSPRawImageTests(unittest.TestCase):
    def compile_run(self, source, defines=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='stvef-raw-', dir=ROOT / '.tmp') as folder:
            folder = Path(folder)
            cpp = folder / 'probe.cpp'
            exe = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source)
            sanitize = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1',
                                     *sanitize, *defines, '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                    cwd=folder, capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            result = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True,
                                    timeout=60, **options)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_ui_forwards_native_brightness_without_changing_color_state(self):
        ui = read('code/client/client_stvef_sp_ui.hpp')
        source = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
using byte=unsigned char; constexpr bool qtrue=true;
static float brightness, currentColor[4]={.2f,.4f,.6f,.3f};
static int rectangle[4],size[2],calls;
static const byte *pixels;
static void Raw(int x,int y,int w,int h,int cols,int rows,const byte *data,float light) {
 rectangle[0]=x;rectangle[1]=y;rectangle[2]=w;rectangle[3]=h;
 size[0]=cols;size[1]=rows;pixels=data;brightness=light;++calls;
}
static void Legacy(int x,int y,int w,int h,int cols,int rows,const byte *data,int,bool) {
 Raw(x,y,w,h,cols,rows,data,1);
}
static struct {
 decltype(&Legacy) DrawStretchRaw=Legacy;
 decltype(&Raw) DrawSTVEFRaw=Raw;
} re;
'''
        source += function_body(ui, 'CL_STVEFSPUIDrawStretchRaw')
        source += '\nint main(){byte buffer[32*16*4]{};\n'
        for row in CASES:
            x, y, w, h = row['rectangle']
            cols, rows = row['size']
            source += (f'CL_STVEFSPUIDrawStretchRaw({x},{y},{w},{h},{cols},{rows},'
                       f'buffer,{literal(row["light"])});\n')
            source += (f'CHECK(brightness=={literal(row["light"])});'
                       f'CHECK(rectangle[0]=={x}&&rectangle[1]=={y}&&rectangle[2]=={w}&&rectangle[3]=={h});\n')
            source += f'CHECK(size[0]=={cols}&&size[1]=={rows}&&pixels==buffer);\n'
        source += f'CHECK(calls=={len(CASES)}&&currentColor[0]==.2f&&currentColor[3]==.3f);}}\n'
        self.compile_run(source)

    def test_pool_replays_retail_draws_and_keeps_queued_images_independent(self):
        source = PROBE + case_table() + MAIN
        for backend, defines in (('gl', ()), ('vulkan', ('-DUSE_VULKAN',))):
            with self.subTest(backend=backend):
                self.compile_run(source, defines)

    def test_every_renderer_wires_the_raw_image_path(self):
        public = read('code/renderercommon/tr_public.h')
        # 28 when this member was added; 31 once the tables it shares grew
        # the IHV, Alice and Medal of Honor members first.
        self.assertIn('#define\tREF_API_VERSION\t\t31', public)
        self.assertIn('void (*DrawSTVEFRaw)( int x, int y, int w, int h, int cols, int rows,\n'
                      '\t\tconst byte *data, float lightValue );', public)
        shared = read('code/renderercommon/tr_stvef_raw.h')
        # One pool entry per submission in a client frame, reused the next frame.
        self.assertIn('if ( tr.stvefRawFrame != tr.frameCount ) {', shared)
        self.assertIn('slot = tr.stvefRawUsed++;', shared)
        self.assertIn('slot = (int)ARRAY_LEN( tr.stvefRawImages ) - 1;', shared)

        self.assertIn('re.DrawSTVEFRaw( x, y, w, h, cols, rows, data, lightValue );',
                      read('code/client/client_stvef_sp_ui.hpp'))
        self.assertIn('re.DrawSTVEFRaw( static_cast<int>( x ), static_cast<int>( y ),',
                      read('code/client/client_stvef_sp_cgame.hpp'))

        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = read(f'code/{renderer}/tr_local.h')
                self.assertIn('image_t *stvefRawImages[32];', local)
                self.assertIn('shader_t *stvefRawShaders[32];', local)
                self.assertIn('int stvefRawFrame, stvefRawUsed;', local)
                self.assertIn('re.DrawSTVEFRaw = RE_DrawSTVEFRaw;', read(f'code/{renderer}/tr_init.c'))
                self.assertIn('#include "../renderercommon/tr_stvef_raw.h"',
                              read(f'code/{renderer}/tr_backend.c'))

                body = function_body(read(f'code/{renderer}/tr_shader.c'), 'R_CreateSTVEFRawShader')
                self.assertIn('shader.cullType = CT_TWO_SIDED;', body)
                self.assertIn('stages[0].stateBits = GLS_DEPTHTEST_DISABLE;', body)
                self.assertIn('stages[0].bundle[0].tcGen = TCGEN_TEXTURE;', body)
                # rgbGen is a stage field in the GL renderer and a bundle field
                # in the Vulkan ones; the vertex colors carry the brightness.
                bundle = local[:local.index('} textureBundle_t;')]
                bundle = bundle[bundle.rindex('typedef struct {'):]
                owner = 'stages[0].bundle[0]' if 'rgbGen;' in bundle else 'stages[0]'
                self.assertIn(f'{owner}.rgbGen = CGEN_EXACT_VERTEX;', body)


if __name__ == '__main__':
    unittest.main()
