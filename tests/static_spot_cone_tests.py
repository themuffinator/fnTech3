"""A static map spot light lights only inside its cone, in every renderer.

Map light sidecars (maps/<map>.lights.json) give a spot a direction and an
outer angle. The renderers lit a spot as a segment light, round the segment
on every side, so F.A.K.K.2 cemetery's 15-degree spot over the churchyard lit
a black-fog passage more than a thousand units off its axis. The cone term
(code/renderercommon/tr_spot_cone.h) is now evaluated by every per-pixel pass
-- GLx's ARB and GLSL programs, the Vulkan and RTX light shaders and the path
tracer -- the front end skips surfaces wholly outside a cone, and a spot is
lit from its origin. Every other light keeps its programs.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding='utf-8')


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def compile_run(test: unittest.TestCase, name: str, source: str) -> str:
    cc = shutil.which('clang')
    if not cc:
        test.skipTest('Clang C compiler unavailable')
    target = compiler_target_args(cc)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as directory:
        path = Path(directory) / f'{name}.c'
        path.write_text(source, encoding='utf-8')
        exe = Path(directory) / f'{name}.exe'
        command = [cc, *target, *abi, '-std=c11',
                   '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'), str(path), '-o', str(exe)]
        if os.name != 'nt':
            command.append('-lm')
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=60, **options)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return run.stdout


HEADER_PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_spot_cone.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float Cone( float degreesOff, float innerAngle, float outerAngle ) {
	float cosInner, cosOuter, scale, bias;
	R_SpotConeCosines( innerAngle, outerAngle, &cosInner, &cosOuter );
	R_SpotConeScaleBias( cosInner, cosOuter, &scale, &bias );
	return R_SpotConeTerm( cosf( degreesOff * ( (float)M_PI / 180.0f ) ), scale, bias );
}

/* How far off the axis a point is, seen from the apex, as the shaders see it. */
static float PointCosine( const vec3_t apex, const vec3_t axis, const vec3_t point ) {
	vec3_t v;
	float length;
	VectorSubtract( point, apex, v );
	length = sqrtf( DotProduct( v, v ) );
	return length > 0.0f ? DotProduct( v, axis ) / length : 1.0f;
}

static unsigned int seed = 12345u;
static float Random( float low, float high ) {
	seed = seed * 1664525u + 1013904223u;
	return low + ( high - low ) * (float)( seed >> 8 ) / (float)( 1u << 24 );
}
'''


CONE_CHECKS = r'''
int main( void ) {
	float cosInner, cosOuter, scale, bias, previous, degrees;
	int trial, sample;

	/* Full on the axis and inside the inner angle, nothing at or past the
	 * outer one, easing in between as the cosine falls. */
	assert( Cone( 0.0f, 0.0f, 15.0f ) > 0.9999f );
	assert( Cone( 5.0f, 10.0f, 15.0f ) == 1.0f );
	assert( Cone( 15.01f, 0.0f, 15.0f ) == 0.0f );
	assert( Cone( 30.0f, 0.0f, 15.0f ) == 0.0f && Cone( 90.0f, 0.0f, 15.0f ) == 0.0f );
	assert( Cone( 180.0f, 0.0f, 15.0f ) == 0.0f );
	previous = 2.0f;
	for ( degrees = 0.0f; degrees <= 15.0f; degrees += 0.25f ) {
		const float cone = Cone( degrees, 0.0f, 15.0f );
		assert( cone >= 0.0f && cone <= 1.0f && cone <= previous );
		previous = cone;
	}
	/* smoothstep on the cosine: half way in cosine is half lit */
	R_SpotConeCosines( 0.0f, 15.0f, &cosInner, &cosOuter );
	R_SpotConeScaleBias( cosInner, cosOuter, &scale, &bias );
	assert( fabsf( R_SpotConeTerm( 0.5f * ( cosInner + cosOuter ), scale, bias ) - 0.5f ) < 1e-3f );
	assert( fabsf( R_SpotConeTerm( cosOuter + 0.25f * ( cosInner - cosOuter ), scale, bias ) - 0.15625f ) < 1e-3f );
	/* an inner angle as wide as the outer one is a hard edge */
	assert( Cone( 14.9f, 15.0f, 15.0f ) == 1.0f && Cone( 15.1f, 15.0f, 15.0f ) == 0.0f );
	/* a cone past 90 degrees still lights its own side only */
	assert( Cone( 120.0f, 0.0f, 150.0f ) > 0.0f && Cone( 160.0f, 0.0f, 150.0f ) == 0.0f );
	/* the parser's clamps: outer 1..179, inner 0..outer, and anything else */
	R_SpotConeCosines( -5.0f, 400.0f, &cosInner, &cosOuter );
	assert( cosInner == 1.0f && fabsf( cosOuter - cosf( 179.0f * ( (float)M_PI / 180.0f ) ) ) < 1e-6f );
	R_SpotConeCosines( 50.0f, 30.0f, &cosInner, &cosOuter );
	assert( cosInner == cosOuter );
	R_SpotConeCosines( NAN, NAN, &cosInner, &cosOuter );
	assert( cosInner == 1.0f && fabsf( cosOuter - cosf( (float)M_PI / 180.0f ) ) < 1e-6f );
	R_SpotConeScaleBias( 0.5f, 0.5f, &scale, &bias );
	assert( isfinite( scale ) && isfinite( bias ) && R_SpotConeTerm( 0.4f, scale, bias ) == 0.0f );
	assert( R_SpotConeTerm( NAN, scale, bias ) == 0.0f );

	/* The cull: a sphere round the apex is kept, as is one on the axis; one
	 * far off to the side or behind a narrow cone is not. */
	{
		const vec3_t apex = { 0, 0, 0 }, axis = { 0, 0, -1 }, start = { 0, 0, 0 }, end = { 0, 0, -500 };
		const vec3_t side = { 1073, 0, -1500 }, onAxis = { 0, 0, -3000 }, behind = { 0, 0, 300 }, near = { 20, 0, 10 };
		const vec3_t sideMins = { 1000, -64, -1600 }, sideMaxs = { 1100, 64, -1400 };
		const vec3_t axisMins = { -8, -8, -900 }, axisMaxs = { 8, 8, -800 };
		R_SpotConeCosines( 0.0f, 15.0f, &cosInner, &cosOuter );
		assert( !R_SpotConeCullSphere( apex, axis, cosOuter, near, 32.0f ) );
		assert( !R_SpotConeCullSphere( apex, axis, cosOuter, onAxis, 1.0f ) );
		assert( R_SpotConeCullSphere( apex, axis, cosOuter, side, 64.0f ) );
		assert( R_SpotConeCullSphere( apex, axis, cosOuter, behind, 64.0f ) );
		assert( R_SpotConeCullBounds( start, end, cosOuter, sideMins, sideMaxs ) );
		assert( !R_SpotConeCullBounds( start, end, cosOuter, axisMins, axisMaxs ) );
		/* a segment of no length has no axis to cull by */
		assert( !R_SpotConeCullBounds( start, start, cosOuter, sideMins, sideMaxs ) );
	}

	/* Conservative: every point of a sphere the cull rejects is outside the
	 * cone, for random cones from 1 to 179 degrees. */
	for ( trial = 0; trial < 20000; trial++ ) {
		vec3_t apex, axis, center, point;
		float radius, outer, length;
		apex[0] = Random( -500, 500 ); apex[1] = Random( -500, 500 ); apex[2] = Random( -500, 500 );
		do {
			axis[0] = Random( -1, 1 ); axis[1] = Random( -1, 1 ); axis[2] = Random( -1, 1 );
			length = sqrtf( DotProduct( axis, axis ) );
		} while ( length < 0.1f );
		VectorScale( axis, 1.0f / length, axis );
		outer = Random( 1, 179 );
		R_SpotConeCosines( 0.0f, outer, &cosInner, &cosOuter );
		center[0] = apex[0] + Random( -2000, 2000 );
		center[1] = apex[1] + Random( -2000, 2000 );
		center[2] = apex[2] + Random( -2000, 2000 );
		radius = Random( 1, 800 );
		if ( !R_SpotConeCullSphere( apex, axis, cosOuter, center, radius ) ) {
			continue;
		}
		for ( sample = 0; sample < 64; sample++ ) {
			vec3_t offset;
			do {
				offset[0] = Random( -1, 1 ); offset[1] = Random( -1, 1 ); offset[2] = Random( -1, 1 );
			} while ( DotProduct( offset, offset ) > 1.0f );
			VectorMA( center, radius, offset, point );
			assert( PointCosine( apex, axis, point ) < cosOuter + 1e-4f );
		}
	}
	printf( "ok\n" );
	return 0;
}
'''


class SpotConeHeaderTests(unittest.TestCase):
    def test_cone_term_is_one_inside_and_zero_outside_and_the_cull_is_conservative(self):
        self.assertEqual(compile_run(self, 'spotcone', HEADER_PRELUDE + CONE_CHECKS).strip(), 'ok')

    def test_cemetery_spot_leaves_the_fog_passage_dark(self):
        # docs/fntech3/2026-10-24-fakk2-playability-round.md: the passage at
        # (3008..3520, -128..384, 256..640) is 1,073 units from this spot's
        # axis, far outside its 15 degrees
        sidecar = json.loads(read('pkg/games/fakk2/fakk/maps/cemetery.lights.json'))
        light = next(entry for entry in sidecar['lights'] if entry.get('name') == 'cemetery-light-0117')
        self.assertEqual(light['type'], 'spot')
        values = dict(
            ox=light['origin'][0], oy=light['origin'][1], oz=light['origin'][2],
            dx=light['direction'][0], dy=light['direction'][1], dz=light['direction'][2],
            radius=light.get('radius', light.get('intensity')), inner=light.get('innerAngle', 0.0),
            outer=light['outerAngle'])
        checks = r'''
int main( void ) {
	vec3_t origin = { %(ox)r, %(oy)r, %(oz)r }, axis = { %(dx)r, %(dy)r, %(dz)r }, end;
	const vec3_t mins = { 3008, -128, 256 }, maxs = { 3520, 384, 640 };
	float cosInner, cosOuter, scale, bias;
	vec3_t point, floor;
	int x, y, z;
	VectorScale( axis, 1.0f / sqrtf( DotProduct( axis, axis ) ), axis );
	VectorMA( origin, %(radius)r, axis, end );
	R_SpotConeCosines( %(inner)r, %(outer)r, &cosInner, &cosOuter );
	R_SpotConeScaleBias( cosInner, cosOuter, &scale, &bias );
	/* the front end skips the passage, and no point of it is lit */
	assert( R_SpotConeCullBounds( origin, end, cosOuter, mins, maxs ) );
	for ( x = 0; x <= 8; x++ ) for ( y = 0; y <= 8; y++ ) for ( z = 0; z <= 8; z++ ) {
		point[0] = mins[0] + ( maxs[0] - mins[0] ) * x / 8.0f;
		point[1] = mins[1] + ( maxs[1] - mins[1] ) * y / 8.0f;
		point[2] = mins[2] + ( maxs[2] - mins[2] ) * z / 8.0f;
		assert( R_SpotConeTerm( PointCosine( origin, axis, point ), scale, bias ) == 0.0f );
	}
	/* what it points at, 1,000 units down its axis, is fully lit */
	VectorMA( origin, 1000.0f, axis, floor );
	assert( R_SpotConeTerm( PointCosine( origin, axis, floor ), scale, bias ) > 0.999f );
	printf( "ok\n" );
	return 0;
}
''' % values
        self.assertEqual(compile_run(self, 'cemetery', HEADER_PRELUDE + checks).strip(), 'ok')


ARB_PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#define USE_PMLIGHT 1
#include "code/qcommon/q_shared.h"
#include <stdio.h>
#include <string.h>
static struct { int numTextureUnits; } glConfig = { 4 };
'''

ARB_MAIN = r'''
int main( void ) {
	static char buffer[65536];
	int i;
	for ( i = DLIGHT_FRAGMENT; i <= DLIGHT_SHADOW_LINEAR_ABS_FRAGMENT_FOG; i++ ) {
		printf( "@@%d\n%s", i, ARB_BuildDlightFP( buffer, i ) );
	}
	printf( "@@names %d %d %d %d %d %d %d %d %d %d\n", DLIGHT_FRAGMENT, DLIGHT_LINEAR_FRAGMENT,
		DLIGHT_LINEAR_FRAGMENT_FOG, DLIGHT_LINEAR_ABS_FRAGMENT, DLIGHT_LINEAR_ABS_FRAGMENT_FOG,
		DLIGHT_SPOT_FRAGMENT, DLIGHT_SPOT_FRAGMENT_FOG, DLIGHT_SPOT_ABS_FRAGMENT,
		DLIGHT_SPOT_ABS_FRAGMENT_FOG, DLIGHT_SHADOW_FRAGMENT );
	return 0;
}
'''

# what a spot program adds to its linear twin, verbatim
ARB_SPOT_CONE = (
    "PARAM spotCone = program.local[11]; \n"
    "TEMP cone; \n"
    "DP3 cone.x, LV, LV; \n"
    "RSQ cone.x, cone.x; \n"
    "MUL cone.x, cone.x, tmp.w; \n"
    "MAD cone.x, cone.x, spotCone.x, spotCone.y; \n"
    "KIL cone.x; \n"
    "MIN cone.x, cone.x, {1.0}; \n"
    "MAD cone.y, cone.x, attenShape.y, attenShape.x; \n"
    "MUL cone.y, cone.y, cone.x; \n"
    "MUL cone.y, cone.y, cone.x; \n")
ARB_SPOT_LV = "DP3 lv.w, LV, LV; \nRSQ lv.w, lv.w; \nMUL lv.xyz, LV, lv.w; \n"
ARB_LINEAR_LV = "RSQ lv.w, tmp.w; \nMUL lv.xyz, dnLV, lv.w; \n"
ARB_SPOT_LIGHT = "MUL light, light, cone.y; \n"


class SpotConeRendererTests(unittest.TestCase):
    def test_arb_spot_programs_are_the_linear_ones_with_the_cone(self):
        arb = read('code/renderer/tr_arb.c')
        local = read('code/renderer/tr_local.h')
        enum = local[local.index('typedef enum {', local.index('} programType;')):]
        enum = enum[:enum.index('} programNum;') + len('} programNum;')] + '\n'
        out = compile_run(self, 'arbspot', ARB_PRELUDE + enum +
                          function(arb, 'static const char *ARB_BuildDlightFP( char *program, int programIndex )') +
                          ARB_MAIN)
        chunks = out.split('@@')[1:]
        names = [int(value) for value in chunks[-1].split()[1:]]
        programs = {int(chunk.split('\n', 1)[0]): chunk.split('\n', 1)[1] for chunk in chunks[:-1]}
        (point, linear, linearFog, linearAbs, linearAbsFog,
         spot, spotFog, spotAbs, spotAbsFog, shadow) = names
        spots = {spot: linear, spotFog: linearFog, spotAbs: linearAbs, spotAbsFog: linearAbsFog}
        self.assertEqual(sorted(spots), list(range(spot, spotAbsFog + 1)))
        self.assertEqual(spotAbsFog + 1, shadow)
        for index, text in programs.items():
            with self.subTest(program=index):
                if index in spots:
                    self.assertEqual(text.count(ARB_SPOT_CONE), 1)
                    self.assertEqual(text.count(ARB_SPOT_LV), 1)
                    self.assertEqual(text.count(ARB_SPOT_LIGHT), 1)
                    # the cone reads tmp.w right after the projection wrote it
                    self.assertLess(text.index('MAD dnLV, lightVector, tmp.x, LV; \n'), text.index(ARB_SPOT_CONE))
                    self.assertLess(text.index('MUL light, lightRGB, tmp.x; \n'), text.index(ARB_SPOT_LIGHT))
                    twin = text.replace(ARB_SPOT_CONE, '').replace(ARB_SPOT_LV, ARB_LINEAR_LV).replace(
                        ARB_SPOT_LIGHT, '')
                    self.assertEqual(twin, programs[spots[index]])
                else:
                    self.assertNotIn('cone', text)
                    self.assertNotIn('local[11]', text)
        self.assertIn('for ( i = DLIGHT_FRAGMENT; i <= DLIGHT_SPOT_ABS_FRAGMENT_FOG; i++ ) {', arb)
        setup = function(arb, 'void ARB_SetupLightParams( const shaderStage_t *pStage )')
        self.assertIn('} else if ( dl->spot ) {\n\t\tfragmentProgram = (tess.shader->cullType == CT_TWO_SIDED) ? '
                      'DLIGHT_SPOT_ABS_FRAGMENT : DLIGHT_SPOT_FRAGMENT;', setup)
        self.assertLess(setup.index('} else if ( dl->spot ) {'), setup.index('} else if ( dl->linear ) {'))
        self.assertIn('R_SpotConeScaleBias( dl->spotCosInner, dl->spotCosOuter, &coneScale, &coneBias );', setup)
        self.assertIn('coneScale / VectorLength( ab ), coneBias, 0.0f, 0.0f );', setup)

    def test_glx_glsl_program_takes_the_cone_only_for_a_spot(self):
        arb = read('code/renderer/tr_arb.c')
        setup = function(arb, 'qboolean GLX_LightingSetupProgram( const shaderStage_t *pStage )')
        self.assertIn('dlightFactors[3] = 0.0f;\n\tif ( dl->spot ) {', setup)
        self.assertIn('R_SpotConeScaleBias( dl->spotCosInner, dl->spotCosOuter,\n\t\t\t'
                      '&dlightFactors[1], &dlightFactors[2] );\n\t\tdlightFactors[3] = 1.0f;', setup)
        glsl = function(read('code/rendererglx/glx_module.cpp'), 'static qboolean GLX_Dlight_FragmentSource(')
        block = glsl[glsl.index('"    vec3 lv = safeNormalize(dnLV);\\n"'):glsl.index('"    float shadowFactor = 1.0;\\n"')]
        self.assertIn('"#if GLX_DLIGHT_LINEAR\\n"\n\t\t"    if (u_DlightFactors.w > 0.5) {\\n"', block)
        self.assertIn('lv = safeNormalize(v_LightVector);', block)
        self.assertIn('float cone = dot(-lv, safeNormalize(u_LightVector.xyz)) * u_DlightFactors.y + '
                      'u_DlightFactors.z;', block)
        self.assertIn('intensity *= (3.0 - 2.0 * cone) * cone * cone;', block)
        self.assertTrue(block.rstrip().endswith('"#endif\\n"'))
        # the spot's light direction reaches its shadow's slope bias too
        self.assertLess(glsl.index('lv = safeNormalize(v_LightVector);'),
                        glsl.index('shadowFactor = dlightShadowFactor(dnLV, v_LightVector, nn, lv);'))
        # the projected overlay and every other bind pass no cone
        self.assertIn('zeroVec, zeroVec, zeroVec, zeroVec, texFactors, zeroVec,', read('code/rendererglx/glx_module.cpp'))

    def test_vulkan_and_rtx_light_shaders_take_the_cone_only_for_a_spot(self):
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                fragment = read(f'code/{renderer}/shaders/light_frag.tmpl')
                ubo = fragment[fragment.index('uniform UBO {'):fragment.index('};')]
                self.assertIn('vec4 dlightFactors;\t\t\t// fragment\n#ifdef USE_LINE\n\tvec4 spotCone;', ubo)
                main = fragment[fragment.index('void main() {'):]
                spot = main[main.index('#ifdef USE_LINE\n\tif (spotCone.w > 0.5) {'):]
                spot = spot[:spot.index('#endif')]
                self.assertIn('nL = normalize(L.xyz);', spot)
                self.assertIn('float cone = dot(-nL, normalize(lightVector.xyz)) * spotCone.x + spotCone.y;', spot)
                self.assertIn('intensFactor *= cone * cone * (3.0 - 2.0 * cone);', spot)
                self.assertLess(main.index('intensFactor = mix(intensFactor, smoothFactor'),
                                main.index('if (spotCone.w > 0.5) {'))
                self.assertLess(main.index('if (spotCone.w > 0.5) {'),
                                main.index('vec3 intens = lightColorRadius.rgb * intensFactor;'))
                header = read(f'code/{renderer}/vk.h')
                uniform = header[header.index('typedef struct vkUniform_s {'):header.index('} vkUniform_t;')]
                self.assertRegex(uniform, r'dlightFactors;[^\n]*\n\tunion \{\n\t\tvec4_t csmModelX;[^\n]*\n'
                                          r'\t\tvec4_t spotCone;')
                shade = function(read(f'code/{renderer}/tr_shade.c'),
                                 'static void VK_SetLightParams( vkUniform_t *uniformData, const dlight_t *dl ) {')
                self.assertIn('\tif ( dl->spot ) {\n\t\tR_SpotConeScaleBias( dl->spotCosInner, dl->spotCosOuter,\n'
                              '\t\t\t&uniformData->spotCone[0], &uniformData->spotCone[1] );', shade)
                self.assertIn('\t} else {\n\t\tVector4Set( uniformData->spotCone, 0.0f, 0.0f, 0.0f, 0.0f );', shade)

    def test_embedded_vulkan_light_spirv_matches_its_source(self):
        # the four light-pass variants compile.bat builds from light_frag.tmpl;
        # rendererrtx's are checked by its own build_shaders.py --check
        glslang = None
        sdk = os.environ.get('VULKAN_SDK')
        for candidate in ([Path(sdk) / 'Bin' / 'glslangValidator.exe', Path(sdk) / 'bin' / 'glslangValidator']
                          if sdk else []) + [shutil.which('glslangValidator')]:
            if candidate and Path(candidate).exists():
                glslang = str(candidate)
                break
        if not glslang:
            self.skipTest('glslangValidator unavailable')
        data = read('code/renderervk/shaders/spirv/shader_data.c')
        variants = {'frag_light': [], 'frag_light_fog': ['-DUSE_FOG'], 'frag_light_line': ['-DUSE_LINE'],
                    'frag_light_line_fog': ['-DUSE_LINE', '-DUSE_FOG']}
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as directory:
            for name, defines in variants.items():
                with self.subTest(variant=name):
                    out = Path(directory) / f'{name}.spv'
                    run = subprocess.run([glslang, '-S', 'frag', '-V', '-o', str(out), 'light_frag.tmpl', *defines],
                                         cwd=ROOT / 'code/renderervk/shaders', capture_output=True, text=True,
                                         timeout=120)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    match = re.search(r'const unsigned char ' + name + r'\[(\d+)\] = \{(.*?)\};', data, re.S)
                    embedded = bytes(int(value, 16) for value in re.findall(r'0x([0-9A-Fa-f]{2})', match[2]))
                    self.assertEqual(int(match[1]), len(embedded))
                    self.assertEqual(embedded, out.read_bytes())

    def test_path_tracer_lights_a_spot_from_its_origin_inside_its_cone(self):
        backend = read('code/rendererrtx/rt/rt_backend.c')
        spot = function(backend, 'static void RT_SpotDlight( const dlight_t *dl, rtGpuLight_t *light )')
        for line in ('VectorCopy( dl->origin, light->position );',
                     'light->position[3] = dl->radius + length;',
                     'VectorCopy( axis, light->dir );',
                     'light->dir[3] = length;',
                     'R_SpotConeScaleBias( dl->spotCosInner, dl->spotCosOuter, &light->extra[0], &light->extra[1] );',
                     'light->extra[2] = dl->radius;'):
            self.assertIn(line, spot)
        upload = function(backend, 'static qboolean RT_UploadLights( rtFrame_t *frame )')
        # every other dynamic light's record is zeroed, so its dir.w is 0
        self.assertLess(upload.index('Com_Memset( light, 0, sizeof( *light ) );'),
                        upload.index('if ( dl->spot ) {\n\t\t\tRT_SpotDlight( dl, light );'))
        self.assertLess(upload.index('light->position[3] = dl->radius;'), upload.index('RT_SpotDlight( dl, light );'))
        lighting = function(read('code/rendererrtx/shaders/rt_lighting.glsl'), 'bool rt_light_eval(')
        dlight = lighting[lighting.index('if (type == RT_LIGHT_DLIGHT) {'):]
        dlight = dlight[:dlight.index('return true;')]
        branch = dlight[dlight.index('if (light.dir.w > 0.0) {'):dlight.index('float falloff = 1.0 - d2 / (r * r);')]
        for line in ('vec3 off = toPoint - light.dir.xyz * clamp(along, 0.0, light.dir.w);',
                     'r = light.extra.z;', 'd2 = dot(off, off);',
                     'cone = along / dist * light.extra.x + light.extra.y;',
                     'cone = cone * cone * (3.0 - 2.0 * cone);'):
            self.assertIn(line, branch)
        # every other light: the same falloff, times exactly one
        self.assertIn('float d2 = dist * dist;\n\t\tfloat cone = 1.0;', dlight)
        self.assertIn('falloff *= cone;\n\t\ts.E = light.color.rgb * (falloff * cosR);', dlight)

    def test_only_static_spots_get_a_cone_and_the_front_end_culls_by_it(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                scene = read(f'code/{renderer}/tr_scene.c')
                dynamic = function(scene, 'static void RE_AddDynamicLightToSceneWithShadowEligibility(')
                linear = function(scene, 'void RE_AddLinearLightToScene(')
                self.assertIn('dl->spot = qfalse;', dynamic)
                self.assertIn('dl->spot = qfalse;', linear)
                static = function(scene, 'static void R_AddStaticMapLightsToScene( const refdef_t *fd, int lightEnd )')
                marked = static[static.index('if ( light->type == MAP_LIGHT_SPOT ) {\n\t\t\tdl->shadowSpotSource'):]
                marked = marked[:marked.index('\n\t\t}\n')]
                self.assertIn('dl->spot = dl->linear;', marked)
                self.assertIn('R_SpotConeCosines( light->innerAngle, light->outerAngle,\n'
                              '\t\t\t\t&dl->spotCosInner, &dl->spotCosOuter );', marked)
                self.assertEqual(scene.count('dl->spot = '), 3)
                # surface-light proxies stay segment lights without a cone
                proxies = function(scene, 'static void R_AddSurfaceLightProxiesToScene( const refdef_t *fd, int lightEnd )')
                self.assertNotIn('spot =', proxies)
                header = read(f'code/{renderer}/tr_local.h')
                record = header[header.index('typedef struct dlight_s {'):header.index('} dlight_t;')]
                self.assertIn('qboolean linear;\n', record)
                self.assertIn('\tqboolean spot;\n\tfloat\tspotCosOuter;', record)
                world = function(read(f'code/{renderer}/tr_world.c'), 'qboolean R_LightCullBounds(')
                branch = world[world.index('if ( dl->linear ) {'):world.index('\n\t}\n')]
                self.assertIn('if ( dl->spot && R_SpotConeCullBounds( dl->transformed, dl->transformed2,\n'
                              '\t\t\tdl->spotCosOuter, mins, maxs ) )\n\t\t\treturn qtrue;', branch)
        # the module lights that bypass both scene functions start zeroed
        for helper in ('tr_rtcw_light_scene.h', 'tr_wolfet_light_scene.h'):
            with self.subTest(helper=helper):
                text = read(f'code/renderercommon/{helper}')
                self.assertLess(text.index('Com_Memset( &light, 0, sizeof( light ) );'),
                                text.index('backEndData->dlights[r_numdlights++] = light;'))


if __name__ == '__main__':
    unittest.main()
