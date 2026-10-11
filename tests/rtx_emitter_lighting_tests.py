"""The path tracer's default lighting: the level's own lights, traced.

rtx_rt_lighting 0 lights a scene with the lights the level defines - the
light entities its map keeps, its q3map_surfacelight shaders, its sky and
q3map_sun - and the surfaces that give off light, with traced bounces in
place of the compilers' ambient and _minlight floors and nothing baked;
rtx_rt_lighting 3 leaves the light entities out, as Q2RTX lights Quake II
from its sky and emitters alone. A surface gives off what its shader draws with
the lighting term black (a glow stage over a lightmapped base, an additive
flame); a shader drawn fullbright is lit by the scene all the same, and only
its bright texels glow, as Q2RTX synthesizes emissive masks from a texture's
bright pixels. The sky lights with its own images, or evenly where its shader
asks for light (q3map_surfacelight, q3map_skyLight) up to a ceiling. Light
is traced in linear units, the colours the shaders and the sky show decoded
from the display's gamma, and the view is tone mapped as Q2RTX maps its own:
a curve fitted to the histogram of its brightness (Eilertsen, Mantiuk and
Unger's noise-aware tone mapping), blended with Reinhard's curve behind an
exposure metered on the 70th to 90th percentiles of that brightness, then
encoded for the display.
"""
from pathlib import Path
import math
import re
import unittest

from rtx_light_entity_tests import compile_run_c
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


def smoothstep(edge0, edge1, x):
    t = min(max((x - edge0) / (edge1 - edge0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


PRELUDE = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
static float Com_Clamp( float lo, float hi, float v ) { return v < lo ? lo : ( v > hi ? hi : v ); }
#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define GLS_SRCBLEND_BITS						0x0000000f
#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_BITS						0x000000f0
#define GLS_BLEND_BITS							( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS )
typedef int qboolean;
enum { qfalse, qtrue };
#define Com_Memcpy memcpy
#include <string.h>
'''

CHECKS = r'''
static int Near( float a, float b ) { return fabsf( a - b ) < 1e-5f; }
// the stages of a shader, folded with the lighting term at l (1 or 0)
static void Fold( const unsigned *blends, const float ( *colours )[4], int count, float out[4] )
{
	int i;
	out[0] = out[1] = out[2] = out[3] = 0.0f;
	for ( i = 0; i < count; i++ ) {
		RT_BlendAverage( blends[i], colours[i], out );
	}
}
int main( void )
{
	float out[4];
	// a lightmap, then its texture filtered through it: nothing at zero light
	{
		const unsigned blends[2] = { 0, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO };
		const float dark[2][4] = { { 0, 0, 0, 1 }, { 0.6f, 0.5f, 0.4f, 1 } };
		const float lit[2][4] = { { 1, 1, 1, 1 }, { 0.6f, 0.5f, 0.4f, 1 } };
		Fold( blends, dark, 2, out );
		assert( Near( out[0], 0.0f ) && Near( out[1], 0.0f ) && Near( out[2], 0.0f ) );
		Fold( blends, lit, 2, out );
		assert( Near( out[0], 0.6f ) && Near( out[1], 0.5f ) && Near( out[2], 0.4f ) );
	}
	// a glow stage added over a lit base keeps its colour at zero light
	{
		const unsigned blends[3] = { 0, GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE };
		const float dark[3][4] = { { 0, 0, 0, 1 }, { 0.6f, 0.5f, 0.4f, 1 }, { 0.3f, 0.2f, 0.1f, 1 } };
		Fold( blends, dark, 3, out );
		assert( Near( out[0], 0.3f ) && Near( out[1], 0.2f ) && Near( out[2], 0.1f ) );
	}
	// an additive flame over nothing is its own colour
	{
		const unsigned blends[1] = { GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE };
		const float flame[1][4] = { { 0.9f, 0.5f, 0.1f, 1 } };
		Fold( blends, flame, 1, out );
		assert( Near( out[0], 0.9f ) && Near( out[1], 0.5f ) && Near( out[2], 0.1f ) );
	}
	// an alpha blend mixes by the source's alpha
	{
		const unsigned blends[2] = { 0, GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA };
		const float stages[2][4] = { { 0.2f, 0.2f, 0.2f, 1 }, { 1.0f, 0.0f, 0.0f, 0.25f } };
		Fold( blends, stages, 2, out );
		assert( Near( out[0], 0.4f ) && Near( out[1], 0.15f ) && Near( out[2], 0.15f ) );
	}
	// a destination that filters by the source's colour
	{
		const unsigned blends[2] = { 0, GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR };
		const float stages[2][4] = { { 0.8f, 0.8f, 0.8f, 1 }, { 0.5f, 0.25f, 1.0f, 1 } };
		Fold( blends, stages, 2, out );
		assert( Near( out[0], 0.4f ) && Near( out[1], 0.2f ) && Near( out[2], 0.8f ) );
	}
	// the bright-texel curve: nothing below 0.55, everything above 0.85
	assert( Near( RT_BrightTexel( 0.5f, 0.2f, 0.1f ), 0.0f ) );
	assert( Near( RT_BrightTexel( 0.9f, 0.2f, 0.1f ), 1.0f ) );
	assert( Near( RT_BrightTexel( 0.1f, 0.7f, 0.1f ), 0.5f ) );
	puts( "emitters ok" );
	return 0;
}
'''


class RtxEmitterLightingTests(unittest.TestCase):
    def test_the_stage_fold_splits_emission_as_the_gpu_does(self):
        material = read('code/rendererrtx/rt/rt_material.c')
        source = PRELUDE
        for signature in ('static void RT_BlendFactor(', 'static void RT_BlendAverage(',
                          'static float RT_BrightTexel('):
            source += function(material, signature) + '\n'
        compile_run_c(self, source + CHECKS)

    def test_the_bright_texel_curve_is_the_same_on_both_sides(self):
        common = read('code/rendererrtx/shaders/rt_common.glsl')
        glsl = function(common, 'float rt_bright_texel(vec3 c)')
        self.assertIn('return smoothstep(0.55, 0.85, max(c.r, max(c.g, c.b)));', glsl)
        c = function(read('code/rendererrtx/rt/rt_material.c'), 'static float RT_BrightTexel(')
        self.assertIn('( m - 0.55f ) / ( 0.85f - 0.55f )', c)
        self.assertIn('return x * x * ( 3.0f - 2.0f * x );', c)
        # a stage drawn at tr.identityLight never glows; one drawn at full
        # intensity glows where its texture is bright
        self.assertEqual(smoothstep(0.55, 0.85, 0.5), 0.0)
        self.assertEqual(smoothstep(0.55, 0.85, 1.0), 1.0)

    def test_a_fullbright_shader_is_lit_and_only_its_bright_texels_glow(self):
        glsl = read('code/rendererrtx/shaders/rt_material.glsl')
        evaluate = function(glsl, 'RtSurface rt_evaluate_material(')
        branch = evaluate[evaluate.index('if ((m.flags & RT_MF_FULLBRIGHT_LIT) != 0u) {'):]
        for line in ('float glow = rt_bright_texel(c);', 'surf.emission = c * glow;',
                     'surf.reflectance = c * (1.0 - glow);', 'surf.lit = true;'):
            self.assertIn(line, branch)
        material = read('code/rendererrtx/rt/rt_material.c')
        lit = function(material, 'static qboolean RT_ShaderFullbrightLit(')
        # the default lighting only, never a q3map surface light, sky or portal
        self.assertIn('!RT_LightingEmits( RT_LightingMode() )', lit)
        self.assertIn('( shader->surfaceLightValid && shader->surfaceLight > 0.0f )', lit)
        self.assertIn('RT_ShaderIsSky( shader )', lit)
        self.assertIn('shader->sort == SS_PORTAL', lit)
        # what it emits as a light is its bright texels' average
        glow = function(material, 'static void RT_FullbrightGlow(')
        self.assertIn('RT_ImageBright( s->images[0], scale, part );', glow)
        gpu = read('code/rendererrtx/rt/rt_gpu.h')
        common = read('code/rendererrtx/shaders/rt_common.glsl')
        self.assertEqual(re.search(r'#define RT_MF_FULLBRIGHT_LIT\s+\(\s*1u << (\d+)\s*\)', gpu).group(1),
                         re.search(r'const uint RT_MF_FULLBRIGHT_LIT = 1u << (\d+);', common).group(1))

    def test_a_placeholder_never_glows(self):
        material = read('code/rendererrtx/rt/rt_material.c')
        emission = function(material, 'static qboolean RT_ShaderEmission(')
        self.assertIn('shader == tr.defaultShader || shader->defaultShader', emission)
        # a generated image (the checkerboard) shows no light of its own
        average = function(material, 'static void RT_ImageAverage(')
        self.assertIn("image->imgName[0] != '*' && image->imgName[0] != '$'", average)
        self.assertIn('rgba[0] = rgba[1] = rgba[2] = 0.0f;', average)
        # the image loader's pixels are freed by the image code, not the tracer
        self.assertNotIn('ri.Free(', material)
        self.assertIn('qboolean R_ImageSums(', read('code/rendererrtx/tr_image.c'))

    def test_emitters_are_sampled_and_not_counted_again_by_a_bounce(self):
        lights = read('code/rendererrtx/rt/rt_lights.c')
        add = function(lights, 'static void RT_AddEmissiveSurfaces(')
        self.assertIn('RT_ShaderEmission( shader, emissions[shader->index] )', add)
        # radiance / pi: the diffuse light a white surface returns
        self.assertIn('VectorScale( emissions[shader->index], boost / M_PI, radiance );', add)
        self.assertIn('RT_AddTriangleLight( a->xyz, b->xyz, c->xyz, n, radiance, value, twoSided, 0u );', add)
        material = read('code/rendererrtx/rt/rt_material.c')
        flag = material[material.index('} else if ( RT_LightingEmits( RT_LightingMode() ) ) {'):]
        flag = flag[:flag.index('// Quake III materials carry no reflectance model')]
        self.assertIn('out->flags |= RT_MF_SURFACE_LIGHT;', flag)
        path = read('code/rendererrtx/shaders/rt_path.glsl')
        self.assertIn('vec3 emission = (material.flags & RT_MF_SURFACE_LIGHT) != 0u ? vec3(0.0) : surf.emission;', path)
        init = read('code/rendererrtx/tr_init.c')
        self.assertIn('rtx_rt_emissive_light = ri.Cvar_Get( "rtx_rt_emissive_light", "2.0", CVAR_ARCHIVE_ND );', init)
        # the compilers' stand-in for a fixture's bounced light is not added
        surface = function(lights, 'static void RT_AddSurfaceLights(')
        self.assertIn('if ( !RT_LightingEmits( s_rtLights.mode ) ) {', surface)

    def test_the_sky_lights_with_its_own_colour_or_as_its_shaders_ask(self):
        lights = read('code/rendererrtx/rt/rt_lights.c')
        sky = function(lights, 'static void RT_FindSkyLightColor(')
        self.assertIn('#define RT_SKY_LIGHT_CEILING\t0.75f', lights)
        self.assertIn('VectorScale( wanted, MIN( luminance, RT_SKY_LIGHT_CEILING ) / luminance, out );', sky)
        # a surface light's form factor, and two thirds of a sky light's value
        self.assertIn('shader->surfaceLight * RT_LIGHT_AREA_SCALE * RT_LIGHT_FORM_FACTOR_SCALE *', sky)
        self.assertIn('shader->rtSkyLightValue * s_rtLights.scale * ( 2.0f / 3.0f )', sky)
        glsl = read('code/rendererrtx/shaders/rt_lighting.glsl')
        sample = function(glsl, 'vec3 rt_sample_sky_light(')
        self.assertIn('light = u_frame.skyLight.w > 0.5 ? u_frame.skyLight.rgb * u_frame.params0.w :', sample)
        self.assertIn('rt_sky_radiance(L) * u_frame.skyLight.rgb;', sample)
        self.assertNotIn('rt_sun_visibility(', sample)
        direct = function(read('code/rendererrtx/shaders/rt_path.glsl'), 'ShadeResult shade_direct(')
        self.assertEqual(direct.count('rt_sun_visibility('), 1)
        self.assertIn('[[dont_unroll]] for (uint sky = 0u; sky < 2u; sky++)', direct)
        # q3map's sky surface lights give way to the sky's own light
        surface = function(lights, 'static void RT_AddSurfaceLights(')
        self.assertIn('if ( s_rtLights.mode == RT_LIGHTING_EMITTERS && RT_ShaderIsSky( shader ) ) {', surface)
        path = read('code/rendererrtx/shaders/rt_path.glsl')
        # a diffuse bounce that reaches the sky adds nothing: the light
        # samples reach it
        self.assertIn('radiance += throughput * ((seen || !skyIsLight) ? rt_sky_radiance(dir) : vec3(0.0));', path)

    def test_the_view_is_tone_mapped_as_q2rtx_maps_it(self):
        tonemap = read('code/rendererrtx/shaders/rt_tonemap.comp')
        curve = function(tonemap, 'void curve_pass()')
        # metered on the 70th to 90th percentiles, adapting twice as fast
        # toward a brighter view
        self.assertIn('float inside = bin ? max(0.0, min(cdf, 0.9) - max(cdf - p, 0.7)) : 0.0;', curve)
        self.assertIn('meteredLog = mix(meteredLog, previous, exp(-dt * speed * (previous < meteredLog ? 2.0 : 1.0)));',
                      curve)
        # Eilertsen et al.'s slopes, their threshold iterated, blurred and
        # integrated from -range
        self.assertIn('threshold = inverseSum > 0.0 ? (kept - bins) / inverseSum : 0.0;', curve)
        self.assertIn('float slope = passed * (1.0 + (inverseSum > 0.0 ? inverse * (bins - kept) / inverseSum : 0.0));',
                      curve)
        self.assertIn('float w = exp(-float(dx * dx) / (2.0 * 12.0 * 12.0));', curve)
        self.assertIn('float curve = (bin_prefix(blurred) - blurred) * RT_TONE_BIN_STOPS - range +', curve)
        # a view past the range the exposure adapts over is as much darker or
        # brighter in the curve as in the exposure
        self.assertIn('(viewLog - clamp(viewLog, log2(RT_TONE_MIN_LUMINANCE), log2(RT_TONE_MAX_LUMINANCE)));', curve)
        # the light is mapped, not the surface it lights: textures keep their
        # detail (the detail layer of Eilertsen et al.)
        glsl_base = function(read('code/rendererrtx/shaders/rt_tonemap.glsl'), 'float tone_base(ivec2 tp, uint final)')
        self.assertIn('denoised_irradiance(tp, final) * RT_TONE_GREY +', glsl_base)
        self.assertNotIn('u_reflectance', glsl_base)
        self.assertIn('float lum = tone_base(tp, pc.extra.z & 1u);', function(tonemap, 'void histogram_pass()'))
        self.assertIn('u_tone.histogram[tid] = 0u;', curve)
        histogram = function(tonemap, 'void histogram_pass()')
        self.assertIn('float weight = clamp(1.0 - length(d) * 1.5, 0.01, 1.0) * clamp(imageLoad(u_emission, tp).a, 0.0, 1.0);',
                      histogram)
        glsl = read('code/rendererrtx/shaders/rt_tonemap.glsl')
        apply = function(glsl, 'vec3 tone_map(vec3 c, float base, float bias, float curveShare)')
        self.assertIn('float lum = max(base, exp2(RT_TONE_LOG_MIN));', apply)
        self.assertIn('return clamp(mix(exposed, fitted, curveShare), 0.0, 1.0);', apply)
        for constant in ('const float RT_TONE_LOG_MIN = -24.0;', 'const float RT_TONE_LOG_MAX = 8.0;',
                         'const float RT_TONE_NOISE_STOPS = -12.0;', 'const float RT_TONE_EXPOSURE_BIAS = -1.0;',
                         'const float RT_TONE_KNEE_START = 0.6;', 'const float RT_TONE_WHITE_POINT = 10.0;'):
            self.assertIn(constant, glsl)
        # the composite maps each traced pixel through it, then fogs it
        composite = read('code/rendererrtx/shaders/rt_composite.comp')
        main = function(composite, 'void main()')
        self.assertIn('tone_encode(tone_map(max(color, vec3(0.0)), tone_base(tp, final), pc.exposure.y, pc.exposure.z));',
                      main)
        # a fixed exposure's filmic curve is encoded for the display as well
        self.assertIn('color = tone_encode(tone_knee(color));', main)
        self.assertLess(main.index('tone_map('), main.index('color = mix(pc.fog.rgb, color, clamp(emissionFog.a, 0.0, 1.0));'))
        knee = function(composite, 'vec3 tone_knee(vec3 c)')
        self.assertIn('(c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14)', knee)
        # its passes run between the denoiser and the composite
        backend = read('code/rendererrtx/rt/rt_backend.c')
        self.assertIn('push.exposure[0] = (float)RT_EXPOSURE_AUTO;', backend)
        dispatch = backend[backend.index('RT_TimingStamp( cmd, slot, 3, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );'):]
        self.assertLess(dispatch.index('RT_ToneMap( cmd, frame, &push );'),
                        dispatch.index('RT_Dispatch( cmd, s_rt.compositePipeline, &push, s_rt.outputWidth, s_rt.outputHeight );'))
        tone = function(backend, 'static void RT_ToneMap( VkCommandBuffer cmd, rtFrame_t *frame, const rtDenoisePush_t *push )')
        self.assertIn('qvkCmdFillBuffer( cmd, s_rt.tone.buffer, 0, VK_WHOLE_SIZE, 0 );', tone)
        self.assertLess(tone.index('RT_TONE_PASS_HISTOGRAM'), tone.index('RT_TONE_PASS_CURVE'))
        # its buffer is the denoise set's binding 14, laid out alike
        gpu = read('code/rendererrtx/rt/rt_gpu.h')
        common = read('code/rendererrtx/shaders/rt_denoise_common.glsl')
        enum = backend[backend.index('typedef enum {\n\tRT_DN_NOISY_DIFFUSE,'):backend.index('} rtDenoiseBinding_t;')]
        names = re.findall(r'\t(RT_DN_\w+)', enum)
        self.assertEqual(names.index('RT_DN_TONE'), 14)
        self.assertIn('layout(set = 0, binding = 14, std430) buffer RtTone {', common)
        block = common[common.index('buffer RtTone {'):common.index('} u_tone;')]
        self.assertEqual(re.findall(r'(\w+(?:\[RT_TONE_BINS\])?);', block),
                         ['histogram[RT_TONE_BINS]', 'curve[RT_TONE_BINS]', 'adaptedLog', 'frames', 'pad0', 'pad1'])
        struct = gpu[gpu.index('typedef struct {\n\tuint32_t\thistogram[RT_TONE_BINS];'):gpu.index('} rtToneBuffer_t;')]
        self.assertEqual(re.findall(r'\t(\w+)(?:\[|;)', struct), ['histogram', 'curve', 'adaptedLog', 'frames', 'pad'])
        self.assertIn('#define RT_TONE_BINS\t\t\t128', gpu)
        self.assertIn('const uint RT_TONE_BINS = 128u;', common)
        init = read('code/rendererrtx/tr_init.c')
        for cvar, value in (('rtx_rt_exposure_bias', '0'), ('rtx_rt_exposure_speed', '1'),
                            ('rtx_rt_tonemap_range', '7'), ('rtx_rt_tonemap_curve', '0.5')):
            self.assertIn(f'{cvar} = ri.Cvar_Get( "{cvar}", "{value}", CVAR_ARCHIVE_ND );', init)

    def test_light_is_traced_linear_and_encoded_once(self):
        gpu = read('code/rendererrtx/rt/rt_gpu.h')
        common = read('code/rendererrtx/shaders/rt_common.glsl')
        self.assertEqual(re.search(r'#define RT_FRAME_LINEAR_LIGHT\s+\(\s*1u << (\d+)\s*\)', gpu).group(1),
                         re.search(r'const uint RT_FRAME_LINEAR_LIGHT = 1u << (\d+);', common).group(1))
        self.assertIn('return pow(max(c, vec3(0.0)), vec3(2.2));', function(common, 'vec3 rt_linear(vec3 c)'))
        # the linear camera decodes, unless the textures are decoded already
        backend = read('code/rendererrtx/rt/rt_backend.c')
        self.assertIn('( ( RT_LightingCamera( s_rtLights.mode ) == RT_CAMERA_LINEAR && !vk_scene_linear_enabled() ) ?\n'
                      '\t\t\tRT_FRAME_LINEAR_LIGHT : 0u ) |', backend)
        # what a surface reflects and gives off, after the fold of its stages
        material = function(read('code/rendererrtx/shaders/rt_material.glsl'), 'RtSurface rt_evaluate_material(')
        decode = material[material.index('if ((u_frame.flags.x & RT_FRAME_LINEAR_LIGHT) != 0u) {'):]
        self.assertIn('surf.reflectance = rt_linear(surf.reflectance);', decode)
        self.assertIn('surf.emission = rt_linear(surf.emission);', decode)
        self.assertLess(material.index('float glow = rt_bright_texel(c);'),
                        material.index('if ((u_frame.flags.x & RT_FRAME_LINEAR_LIGHT) != 0u) {'))
        # the sky's images, as the sky lights and as a reflection sees it
        sky = read('code/rendererrtx/shaders/rt_sky.glsl')
        self.assertIn('return (u_frame.flags.x & RT_FRAME_LINEAR_LIGHT) != 0u ? rt_linear(c) : c;',
                      function(sky, 'vec3 rt_sky_color(vec3 c)'))
        radiance = function(sky, 'vec3 rt_sky_radiance(vec3 d)')
        self.assertEqual(radiance.count('rt_sky_color(textureLod('), 2)
        # an emitter's light is what it shows, decoded as the trace decodes it
        # where the camera takes linear light
        lights = read('code/rendererrtx/rt/rt_lights.c')
        self.assertIn("c[i] = c[i] > 0.0f ? powf( c[i], 2.2f ) : 0.0f;", function(lights, 'static void RT_LinearColor('))
        add = function(lights, 'static void RT_AddEmissiveSurfaces(')
        self.assertLess(add.index('RT_ShaderEmission( shader, emissions[shader->index] )'),
                        add.index('RT_LinearColor( emissions[shader->index] );'))
        self.assertIn('if ( RT_LightingCamera( s_rtLights.mode ) == RT_CAMERA_LINEAR ) {\n'
                      '\t\t\t\t\tRT_LinearColor( emissions[shader->index] );', add)
        # encoded once, by the composite
        glsl = read('code/rendererrtx/shaders/rt_tonemap.glsl')
        self.assertIn('return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));', function(glsl, 'vec3 tone_encode(vec3 c)'))
        self.assertNotIn('tone_decode', glsl + read('code/rendererrtx/shaders/rt_composite.comp') +
                         read('code/rendererrtx/shaders/rt_tonemap.comp'))

    def test_the_level_is_lit_by_its_own_lights(self):
        # the default traces the light entities the map keeps (Quake III's
        # and RTCW's do; q3map2 left none in Quake Live's and Enemy
        # Territory's) with the emitters, sky and sun, and leaves the
        # compilers' floors to traced bounces; 3 is the emitters alone
        material = read('code/rendererrtx/rt/rt_material.c')
        for define in ('#define RT_LIGHTING_LEVEL\t\t0', '#define RT_LIGHTING_LIGHTMAPS\t1',
                       '#define RT_LIGHTING_COMPILERS\t2', '#define RT_LIGHTING_EMITTERS\t3'):
            self.assertIn(define, material)
        emits = function(material, 'static qboolean RT_LightingEmits( int mode )')
        self.assertIn('return ( mode == RT_LIGHTING_LEVEL || mode == RT_LIGHTING_EMITTERS ) ? qtrue : qfalse;', emits)
        self.assertIn('return rtx_rt_lighting ? (int)Com_Clamp( 0, 3, rtx_rt_lighting->integer ) : RT_LIGHTING_LEVEL;',
                      function(material, 'static int RT_LightingMode( void )'))
        lights = read('code/rendererrtx/rt/rt_lights.c')
        build = function(lights, 'static qboolean RT_BuildLights( void )')
        emitting = build[build.index('if ( RT_LightingEmits( s_rtLights.mode ) ) {'):build.index('\n\t} else {')]
        level = emitting[emitting.index('if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {'):]
        level = level[:level.index('}')]
        for line in ('RT_AddEntityLights();', 'VectorClear( s_rtLights.ambient );', 'VectorClear( s_rtLights.minLight );',
                     'VectorClear( s_rtLights.minVertexLight );', 'VectorClear( s_rtLights.minGridLight );'):
            self.assertIn(line, level)
        for call in ('RT_AddSurfaceLights();', 'RT_AddEmissiveSurfaces();', 'RT_FindSun();',
                     'RT_FindSkyLightColor( s_rtLights.skyColor );'):
            self.assertLess(emitting.index('RT_AddEntityLights();'), emitting.index(call))
        # the level's sky is its compiler's (q3map's sky surface lights,
        # q3map2's dome), the emitters' an even one in its light colour
        sky = emitting[emitting.index('RT_FindSun();'):]
        self.assertIn('if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {\n\t\t\tRT_FindSkyLight();\n\t\t} else {', sky)
        uniforms = function(read('code/rendererrtx/rt/rt_backend.c'), 'static void RT_WriteFrameUniforms(')
        self.assertIn('if ( s_rtLights.mode == RT_LIGHTING_EMITTERS ) {', uniforms[uniforms.index('Com_Memcpy( u->skyImages'):])
        # the level's lights are shown in their units, the emitters' alone
        # through the linear camera (test_the_traced_light_is_shown_as_it_is)
        backend = read('code/rendererrtx/rt/rt_backend.c')
        self.assertNotIn('const qboolean emitters = RT_LightingEmits( s_rtLights.mode );', backend)
        self.assertIn('const int camera = RT_LightingCamera( s_rtLights.mode );', backend)
        init = read('code/rendererrtx/tr_init.c')
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_lighting, "0", "3", CV_INTEGER );', init)
        self.assertIn('rtx_rt_lighting = ri.Cvar_Get( "rtx_rt_lighting", "0", CVAR_ARCHIVE_ND );', init)

    def test_the_traced_light_is_shown_as_it_is(self):
        # nothing baked takes part: the level's own lights are traced and the
        # light a surface receives, in its compiler's units, multiplies its
        # colour with no exposure; the linear camera is the other choice, and
        # the emitters alone always take it
        material = read('code/rendererrtx/rt/rt_material.c')
        for define in ('#define RT_CAMERA_UNITS\t\t0', '#define RT_CAMERA_LINEAR\t1'):
            self.assertIn(define, material)
        camera = function(material, 'static int RT_LightingCamera( int mode )')
        self.assertLess(camera.index('if ( mode == RT_LIGHTING_EMITTERS ) {\n\t\treturn RT_CAMERA_LINEAR;'),
                        camera.index('if ( mode != RT_LIGHTING_LEVEL ) {\n\t\treturn RT_CAMERA_UNITS;'))
        self.assertIn('return ( rtx_rt_camera && rtx_rt_camera->integer ) ? RT_CAMERA_LINEAR : RT_CAMERA_UNITS;', camera)
        init = read('code/rendererrtx/tr_init.c')
        self.assertIn('rtx_rt_camera = ri.Cvar_Get( "rtx_rt_camera", "0", CVAR_ARCHIVE_ND );', init)
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_camera, "0", "1", CV_INTEGER );', init)
        # the composite: the engine's lightmap ceiling unless the camera is
        # linear, which alone exposes and tone maps
        backend = read('code/rendererrtx/rt/rt_backend.c')
        setup = backend[backend.index('const int camera = RT_LightingCamera( s_rtLights.mode );'):]
        setup = setup[:setup.index('push.view[0] = (uint32_t)view.offset.x;')]
        self.assertIn('push.extra[3] = ( !sceneLinear && camera != RT_CAMERA_LINEAR ) ? RT_COMPOSITE_SHOULDER : 0u;', setup)
        self.assertIn('if ( camera == RT_CAMERA_LINEAR ) {', setup)
        self.assertLess(setup.index('if ( camera == RT_CAMERA_LINEAR ) {'), setup.index('tonemap = qtrue;'))
        # a change of camera rebuilds the lights (a glowing shader's light is
        # in its camera's units) and starts the history over
        self.assertIn('( rtx_rt_camera && rtx_rt_camera->modified ) ) {', backend)
        self.assertIn('rtx_rt_emissive_light, rtx_rt_checkerboard, rtx_rt_camera',
                      function(backend, 'static int RT_TraceSettingsCount( void )'))
        # the lightmaps, vertex light and model light are read only by the
        # lightmap comparison (rtx_rt_lighting 1); the trace never fetches a
        # lightmap texel otherwise, and stores nothing of them
        self.assertIn('const qboolean bakedLighting = s_rtLights.mode == RT_LIGHTING_LIGHTMAPS ? qtrue : qfalse;', backend)
        evaluate = function(read('code/rendererrtx/shaders/rt_material.glsl'), 'RtSurface rt_evaluate_material(')
        self.assertIn('vec4 texel = (!lightmap || bakedLighting) ? rt_stage_texel(s, h, time, lodBias) : vec4(1.0);', evaluate)
        self.assertEqual(evaluate.count('rt_stage_texel('), 1)
        self.assertIn('if (bakedLighting) {\n\t\t\t\tsurf.baked = (texel * rt_stage_generator(s, h, time, -1.0)).rgb;', evaluate)
        self.assertIn('if (bakedLighting && (s.state.z & RT_SF_VERTEX_IS_LIGHT) != 0u) {', evaluate)
        self.assertIn('if (bakedLighting && ((s.state.y >> 8u) & 0xFFu) == CGEN_LIGHTING_DIFFUSE && h.entityLight.w > 0.5) {',
                      evaluate)
        path = read('code/rendererrtx/shaders/rt_path.glsl')
        self.assertIn('return bakedLighting && ((material.flags & (RT_MF_LIGHTMAPPED | RT_MF_VERTEX_LIT)) != 0u || '
                      'surf.bakedEntity != 0u);', function(path, 'bool baked_light('))
        self.assertIn('imageStore(u_outReflectance, pixel, vec4(reflectance, 1.0));', path)
        self.assertNotIn('baked_light(material, surf, true)', path)

    def test_a_dynamic_light_casts_a_soft_shadow(self):
        # its shadow ray aims at a point on a sphere a twentieth of its
        # reach; its brightness stays raster's, from the centre
        lighting = read('code/rendererrtx/shaders/rt_lighting.glsl')
        evaluate = function(lighting, 'bool rt_light_eval(')
        dlight = evaluate[evaluate.index('if (type == RT_LIGHT_DLIGHT) {'):]
        dlight = dlight[:dlight.index('return true;')]
        self.assertIn('s.E = light.color.rgb * (falloff * cosR);', dlight)
        self.assertIn('float body = clamp(0.05 * r, 2.0, 12.0);', dlight)
        self.assertIn('vec3 q = light.position.xyz + body * rand_sphere(seed) - P;', dlight)
        self.assertIn('if (qd > body && dot(N, q) > 0.0) {', dlight)
        self.assertLess(dlight.index('s.E ='), dlight.index('float body'))
        sphere = function(read('code/rendererrtx/shaders/rt_common.glsl'), 'vec3 rand_sphere(inout uint state)')
        self.assertIn('float z = 2.0 * rand01(state) - 1.0;', sphere)

    def test_the_light_sampler_has_one_call_site(self):
        # inlined at each call site, a second copy of the light sampler made
        # the compute backend's trace too large for the 32-bit NVIDIA driver
        # to compile ("LLVM ERROR: out of memory" creating the pipeline)
        path = read('code/rendererrtx/shaders/rt_path.glsl')
        direct = function(path, 'ShadeResult shade_direct(')
        self.assertEqual(direct.count('rt_sample_lights('), 1)
        self.assertIn('rt_sample_lights(P, N, Ng, candidates, exclude, baked, seed, L, found, choice, r.shadowReactive) * share', direct)
        baked = direct[direct.index('if (baked) {'):direct.index('} else if (oneSample) {')]
        self.assertIn('r.irradiance = surf.baked;', baked)
        self.assertIn('pick = (u_frame.flags.w > 0u && (exclude & RT_LIGHT_DYNAMIC) == 0u) ? 1u : 0u;', baked)
        # a lightmapped surface takes no sun or sky sample of its own
        self.assertLess(direct.index('if (baked) {\n\t\treturn r;\n\t}'),
                        direct.index('rt_sample_sun(P, N, Ng, seed, Lsky)'))
        self.assertIn('float weight = (reflections && !baked) ? rt_light_weight(choice, N, Ng, V, L, roughness * roughness) : 1.0;',
                      direct)

    def test_the_light_can_be_seen_alone(self):
        # rtx_rt_debug_visualizer 10: the light on plain grey surfaces,
        # through the same camera
        composite = read('code/rendererrtx/shaders/rt_composite.comp')
        self.assertIn('const uint RT_VIEW_LIGHTING = 10u;', composite)
        self.assertIn('const float RT_VIEW_CLAY = 0.10;', composite)
        main = function(composite, 'void main()')
        view = main[main.index('} else if (view == RT_VIEW_LIGHTING) {'):main.index('} else if (view == RT_VIEW_MOTION) {')]
        self.assertIn('vec3 light = E * RT_VIEW_CLAY + S + Em;', view)
        self.assertIn('tone_encode(tone_map(max(light, vec3(0.0)), tone_base(tp, final), pc.exposure.y, pc.exposure.z));',
                      view)
        self.assertIn('color = mix(pc.fog.rgb, color, clamp(emissionFog.a, 0.0, 1.0));', view)
        self.assertIn('Com_Clamp( 0, 13, rtx_rt_debug_visualizer->integer )', read('code/rendererrtx/rt/rt_backend.c'))
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_debug_visualizer, "0", "13", CV_INTEGER );',
                      read('code/rendererrtx/tr_init.c'))

    def test_quake_live_advertisements_are_traced_in_place(self):
        # the renderer draws a retail QL advertisement's brush model as world
        # surfaces (R_UpdateAdvertisements); a traced view covering it with
        # the wall behind showed no advertisement at all
        backend = read('code/rendererrtx/rt/rt_backend.c')
        world = function(backend, 'static qboolean RT_ExtractWorld(')
        self.assertIn('for ( i = 0; i < tr.world->numAdvertisements; i++ ) {', world)
        self.assertIn('const bmodel_t *bmodel = tr.world->advertisements[i].bmodel;', world)
        self.assertIn('RT_ExtractWorldSurface( geometry, &bmodel->firstSurface[s], filterFlags )', world)
        self.assertLess(world.index('RT_ExtractWorldSurface( geometry, &worldModel->firstSurface[i], filterFlags )'),
                        world.index('tr.world->numAdvertisements'))
        self.assertLess(world.index('tr.world->numAdvertisements'), world.index('RT_RemapMaterials( geometry, 0 );'))
        # their sheen, the lightmap added environment-mapped at 0.15, is a
        # fake reflection that traced reflections replace, not a second
        # lighting term
        material = read('code/rendererrtx/rt/rt_material.c')
        role = function(material, 'static uint32_t RT_StageRole(')
        self.assertLess(role.index('return RT_ROLE_ENVIRONMENT;'), role.index('return RT_ROLE_LIGHTMAP;'))
        self.assertIn('if (role == RT_ROLE_ENVIRONMENT && !bakedLighting) {',
                      read('code/rendererrtx/shaders/rt_material.glsl'))

    def test_a_smoothed_face_is_traced_with_its_own_normals(self):
        # tr_bsp.c smooths a face whose points share no plane (Enemy
        # Territory's terrain) into four floats a point; read three apart,
        # goldrush's ground traced flat grey, lit as if it stood on edge
        bsp = read('code/rendererrtx/tr_bsp.c')
        generate = function(bsp, 'static void GenerateNormals( srfSurfaceFace_t *face )')
        self.assertIn('face->normals = ri.Hunk_Alloc( face->numPoints * sizeof( tess.normal[0] ), h_low );', generate)
        self.assertIn('n1 = face->normals + i*4;', generate)
        self.assertIn('vec4_t\t\tnormal[SHADER_MAX_VERTEXES] QALIGN(16);', read('code/rendererrtx/tr_local.h'))
        vk = read('code/rendererrtx/vk.c')
        # the definition, past the forward declaration
        face = vk[vk.rindex('static qboolean vk_rt_append_surface_face_geometry('):]
        face = face[:face.index('static qboolean vk_rt_append_surface_triangles_geometry(')]
        self.assertIn('VectorCopy( &surface->normals[i * 4], normal );', face)
        self.assertNotIn('normals[i * 3]', face)

    def test_the_fitted_curve_keeps_a_dark_room_and_its_bright_window(self):
        # the curve pass and the composite's mapping, run on the histogram of
        # a room lit through its window: most of the view a dark room, a
        # tenth the sunlit snow outside, 8 stops brighter
        bins, log_min, log_max = 128, -24.0, 8.0
        delta = (log_max - log_min) / bins

        def bin_of(lum):
            return min(max((math.log2(lum) - log_min) / delta, 0.0), bins - 1.0)

        def fit(counts, stops=7.0):
            h = [c + 1.0 for c in counts]
            p = [x / sum(h) for x in h]
            logs = [log_min + k * delta for k in range(bins)]
            cdf, band, band_log = 0.0, 0.0, 0.0
            for k in range(bins):
                cdf += p[k]
                inside = max(0.0, min(cdf, 0.9) - max(cdf - p[k], 0.7))
                band += inside
                band_log += inside * logs[k]
            metered = min(max(band_log / band, math.log2(0.0002)), 0.0)
            q = [p[k] if logs[k] >= -12.0 else 0.0 for k in range(bins)]
            inverse = [1.0 / x if x > 0.0 else 0.0 for x in q]
            span = stops / delta
            threshold = 1e-16
            for _ in range(16):
                passed = [1.0 if x >= threshold else 0.0 for x in q]
                kept = sum(passed)
                inverse_sum = sum(i * s for i, s in zip(inverse, passed))
                threshold = (kept - span) / inverse_sum if inverse_sum > 0.0 else 0.0
            passed = [1.0 if x >= threshold else 0.0 for x in q]
            kept = sum(passed)
            inverse_sum = sum(i * s for i, s in zip(inverse, passed))
            slope = [s * (1.0 + i * (span - kept) / inverse_sum) for s, i in zip(passed, inverse)]
            weights = [math.exp(-dx * dx / (2.0 * 12.0 * 12.0)) for dx in range(-13, 14)]
            blurred = [sum(w * slope[min(max(k + dx, 0), bins - 1)] for w, dx in zip(weights, range(-13, 14))) /
                       sum(weights) for k in range(bins)]
            curve, rise = [], 0.0
            for k in range(bins):
                curve.append(rise * delta - stops)
                rise += blurred[k]
            noise = (-12.0 - log_min) / delta
            at_noise = curve[int(noise)]
            for k in range(int(noise)):
                own = at_noise + (k - noise) * delta
                curve[k] = own + (curve[k] - own) * (smoothstep(0.5 * noise, noise, k) * 0.5 + 0.5)
            return curve, metered

        def display(lum, curve, metered, share=0.5):
            x = bin_of(lum)
            left = int(x)
            right = min(left + 1, bins - 1)
            fitted = lum * 2.0 ** (curve[left] + (curve[right] - curve[left]) * (x - left) - 1.0) / lum
            s, white = 0.6, 10.0
            if fitted >= s:
                w = (s * (s - 2.0) + white) / (white - 1.0)
                fitted = (w * fitted - s * s) / (fitted + w - 2.0 * s)
            scaled = 2.0 ** -3.0 * lum / 2.0 ** metered
            exposed = scaled * (1.0 + scaled / (white * white)) / (1.0 + scaled)
            return min(max(exposed + (fitted - exposed) * share, 0.0), 1.0) ** (1.0 / 2.2)

        room, window = 0.0014, 7.5      # linear light, 12.4 stops apart
        counts = [0.0] * bins
        for lum, weight in ((room * 0.5, 30000.0), (room, 40000.0), (room * 2.0, 20000.0), (window, 10000.0)):
            x = bin_of(lum)
            counts[int(x)] += weight * (1.0 - (x - int(x)))
            counts[int(x) + 1] += weight * (x - int(x))
        curve, metered = fit(counts)
        # monotonic, and within the display's range
        self.assertTrue(all(b >= a - 1e-9 for a, b in zip(curve, curve[1:])))
        self.assertTrue(all(-7.0 - 1e-9 <= c <= 0.05 for c in curve[48:]))
        # the room is seen, with its own contrast; the window is brighter
        self.assertGreater(display(room, curve, metered), 0.2)
        self.assertGreater(display(room * 2.0, curve, metered) / display(room * 0.5, curve, metered), 1.4)
        self.assertGreater(display(window, curve, metered), display(room * 2.0, curve, metered) + 0.2)
        # the fitted curve alone keeps the window short of white (the metered
        # exposure's half clips it, as Q2RTX's does)
        self.assertLess(display(window, curve, metered, 1.0), 0.99)
        self.assertGreater(display(window, curve, metered, 1.0), display(room * 2.0, curve, metered, 1.0) + 0.2)
        # and the stops between them, which nothing occupies, are compressed
        gap = ((curve[int(bin_of(window * 0.5))] - curve[int(bin_of(room * 4.0))]) /
               (math.log2(window * 0.5) - math.log2(room * 4.0)))
        self.assertLess(gap, 0.5)
        # the metered band is the lit room, not the blown window: its 70th to
        # 90th percentiles are the room's brightest pixels
        self.assertLess(abs(metered - math.log2(room * 2.0)), 1.0)

if __name__ == '__main__':
    unittest.main()
