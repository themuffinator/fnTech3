"""The Vulkan and RTX renderers filter textures and test flares as the GL renderers do.

Elite Force's synchronized single-player views
(docs/fntech3/2026-10-23-stvef-renderer-parity.md) found two places where both
Vulkan-family renderers left the GL renderers' behaviour:

- the GL renderers give every mipmapped image the anisotropy setting, whatever
  its texture mode (code/renderer/tr_image.c), where Vulkan and RTX had given
  it to GL_*_MIPMAP_LINEAR alone, so Elite Force's own GL_LINEAR_MIPMAP_NEAREST
  lost it;
- the GL renderers take a flare as visible within 0.25 / w of the depth in
  front of it, 0.75 / w in a portal view (tr_flares.c), where Vulkan and RTX
  used 0.2 / w everywhere.

The production vk_find_sampler of each renderer is compiled with stubbed
Vulkan calls and asked for every filter combination; the flare margins are
compared with the GL renderer's own formula. No renderer is started.
"""
from pathlib import Path
import itertools
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


COMMON = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "code/renderercommon/vulkan/vulkan.h"
typedef int qboolean;
enum { qfalse, qtrue };
enum { ERR_FATAL, ERR_DROP };
constexpr int GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_NEAREST_MIPMAP_NEAREST = 0x2700,
    GL_LINEAR_MIPMAP_NEAREST = 0x2701, GL_NEAREST_MIPMAP_LINEAR = 0x2702, GL_LINEAR_MIPMAP_LINEAR = 0x2703;
#define MAX_VK_SAMPLERS 32
#define MIN(x,y) ((x)<(y)?(x):(y))
struct cvar_t { int integer; };
static cvar_t anisotropic{ 1 }, maxAnisotropy{ 16 };
static cvar_t *r_ext_texture_filter_anisotropic = &anisotropic, *r_ext_max_anisotropy = &maxAnisotropy;
typedef struct {
    VkSamplerAddressMode address_mode;
    int gl_mag_filter;
    int gl_min_filter;
    qboolean max_lod_1_0;
    qboolean noAnisotropy;
} Vk_Sampler_Def;
static void Fail( int, const char *, ... ) { assert( !"ri.Error" ); }
static struct { void ( *Error )( int, const char *, ... ); } ri = { Fail };
static float Com_Clamp( float lo, float hi, float v ) { return v < lo ? lo : v > hi ? hi : v; }
static const char *va( const char *, ... ) { return ""; }
#define VK_CHECK( call ) call
#define SET_OBJECT_NAME( object, name, type )
static VkSamplerCreateInfo created;
static int creations;
static VkResult qvkCreateSampler( VkDevice, const VkSamplerCreateInfo *info, const VkAllocationCallbacks *, VkSampler *out ) {
    created = *info;
    ++creations;
    *out = ( VkSampler )( uintptr_t )creations;
    return VK_SUCCESS;
}
'''

VULKAN = COMMON + r'''
static struct {
    VkDevice device;
    float maxLod, maxAnisotropy;
    qboolean samplerAnisotropy;
    struct { int count; Vk_Sampler_Def def[MAX_VK_SAMPLERS]; VkSampler handle[MAX_VK_SAMPLERS]; int filter_min, filter_max; } samplers;
} vk;
static void Reset() { std::memset( &vk.samplers, 0, sizeof( vk.samplers ) ); }
PRODUCTION
MAIN
'''

RTX = COMMON + r'''
static struct { VkDevice device; float maxLod, maxAnisotropy; qboolean samplerAnisotropy; } vk;
static struct { int num_samplers; Vk_Sampler_Def sampler_defs[MAX_VK_SAMPLERS]; VkSampler samplers[MAX_VK_SAMPLERS]; } vk_world;
static void Reset() { std::memset( &vk_world, 0, sizeof( vk_world ) ); }
PRODUCTION
MAIN
'''

MAIN = r'''
int main() {
    vk.maxLod = 12.0f;
    vk.maxAnisotropy = 16.0f;
    vk.samplerAnisotropy = qtrue;
    const int mins[] = { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
        GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR };
    int checked = 0;
    for ( int setting : { 0, 1 } ) {
        anisotropic.integer = setting;
        for ( int min : mins ) for ( int mag : { GL_NEAREST, GL_LINEAR } )
        for ( int fixedLod : { 0, 1 } ) for ( int opted : { 0, 1 } ) {
            Reset();
            Vk_Sampler_Def def{};
            def.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            def.gl_min_filter = min; def.gl_mag_filter = mag;
            def.max_lod_1_0 = fixedLod; def.noAnisotropy = opted;
            vk_find_sampler( &def );
            // tr_image.c: every mipmapped image takes the anisotropy setting.
            const bool mipmapped = min != GL_NEAREST && min != GL_LINEAR;
            const bool expected = setting && mipmapped && mag == GL_LINEAR && !opted;
            assert( ( created.anisotropyEnable == VK_TRUE ) == expected );
            if ( expected ) assert( created.maxAnisotropy == 16.0f );
            // The filters themselves are unchanged.
            assert( created.mipmapMode == ( min == GL_NEAREST_MIPMAP_LINEAR || min == GL_LINEAR_MIPMAP_LINEAR
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST ) );
            assert( created.maxLod == ( fixedLod ? 1.0f : mipmapped ? VK_LOD_CLAMP_NONE : 0.25f ) );
            ++checked;
        }
    }
    assert( checked == 2 * 6 * 2 * 2 * 2 );
    std::printf( "%d sampler definitions passed\n", checked );
}
'''


class VulkanGLFilterParityTests(unittest.TestCase):
    def run_sampler(self, path, harness, prefix):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        production = function(read(path), 'static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {')
        source = harness.replace('PRODUCTION', production).replace('MAIN', MAIN)
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertIn('96 sampler definitions passed', ran.stdout)

    def test_vulkan_samplers_take_gl_anisotropy(self):
        self.run_sampler('code/renderervk/vk.c', VULKAN, 'vk-sampler-')

    def test_rtx_samplers_take_gl_anisotropy(self):
        self.run_sampler('code/rendererrtx/vk.c', RTX, 'rtx-sampler-')

    def test_gl_still_gives_every_mipmapped_image_anisotropy(self):
        image = read('code/renderer/tr_image.c')
        block = image[image.index('\tif ( image->flags & IMGFLAG_MIPMAP )\n\t{\n\t\tif ( textureFilterAnisotropic ) {'):]
        self.assertIn('GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLint) maxAnisotropy', block[:300])
        self.assertIn('qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );', block[:400])


class VulkanGLFlareParityTests(unittest.TestCase):
    def test_flare_margins_match_the_gl_renderer(self):
        gl = read('code/renderer/tr_flares.c')
        self.assertIn('f->drawZ = (clip[2] + clip[3] - 1.5 ) / ( 2 * clip[3] );', gl)
        self.assertIn('f->drawZ = (clip[2] + clip[3] - 0.5 ) / ( 2 * clip[3] );', gl)
        # GL's window depth is (z / w + 1) / 2, so its margins are 0.75 / w in a
        # portal view and 0.25 / w otherwise; Vulkan's depth is z / w itself.
        for z, w in itertools.product((-90.0, -3.5, 0.0, 12.25, 640.0), (1.0, 8.0, 333.0, 4096.0)):
            depth = (z + w) / (2 * w)
            self.assertAlmostEqual(depth - (z + w - 1.5) / (2 * w), 0.75 / w)
            self.assertAlmostEqual(depth - (z + w - 0.5) / (2 * w), 0.25 / w)
        for path in ('code/renderervk/vk_flares.c', 'code/rendererrtx/vk_flares.c'):
            with self.subTest(renderer=path):
                add = function(read(path), 'void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {')
                self.assertIn('bias = R_ViewPassIsPortal( &backEnd.viewParms ) ? 0.75f : 0.25f;', add)
                self.assertIn('#ifdef USE_REVERSED_DEPTH\n\tf->drawZ = (clip[2] + bias) / clip[3];\n#else\n'
                              '\tf->drawZ = (clip[2] - bias) / clip[3];\n#endif', add)
                self.assertNotIn('0.20', add)
        self.assertEqual(read('code/renderervk/vk_flares.c'), read('code/rendererrtx/vk_flares.c'))


if __name__ == '__main__':
    unittest.main()
