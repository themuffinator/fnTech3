"""The path tracer's physically based materials (record 2026-11-05-rtx-pbr-materials).

A texture pack's material words (OpenJK rend2's normalMap, specularMap,
rmoMap and the rest, renderercommon/tr_material_words.h) go through each
renderer's production shader parser under every profile: the raster
renderers read past them and draw the stage as the rest of it says, and the
path tracer's parser keeps them for the stage that read them. The material
record, its shading (all six of rend2's packings, the scales, occlusion,
specular occlusion, emission maps and parallax) and the companion names are
checked against their sources. The shader lines are written for this test.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


def section(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i) + len(end)]


# The words in the diffuse stage of a lightmapped wall, then the stage's own
# keywords after them; parsed under every profile by every renderer.
HOST_CHECKS = r'''
#include <math.h>
static int hostFailures;
#define CHECK( cond ) do { if ( !( cond ) ) { fprintf( stderr, "game %d line %d: CHECK( %s ) failed [%s]\n", (int)ri.gameId, __LINE__, #cond, hostWarning ); ++hostFailures; } } while ( 0 )
static qboolean Parse( comGameId_t game, const char *text ) {
	HostInit( game );
	return HostParse( "textures/test/wall", text, text + strlen( text ) );
}
static int Near( float a, float b ) { return fabsf( a - b ) < 1e-5f; }
static int HostChecks( void ) {
	const char *wall =
		"{\n{\nmap textures/test/wall.tga\nnormalHeightMap textures/test/wall_nh.tga\n"
		"rmosMap textures/test/wall_rmos.tga\nspecularScale 0.5 1 0.75 0.6\nparallaxDepth 0.08\n"
		"parallaxBias 0.1\nnormalScale 1 -1\nrgbGen identity\n}\n"
		"{\nmap $lightmap\nblendFunc GL_DST_COLOR GL_ZERO\n}\n}\n";
	const char *gloss =
		"{\n{\nmap textures/test/wall.tga\nspecMap textures/test/wall_s.tga\ngloss 0.25\n}\n}\n";
	const char *constants =
		"{\n{\nmap textures/test/wall.tga\nspecularReflectance 0.5\nroughness 0.3\n}\n}\n";
	const char *exponent =
		"{\n{\nmap textures/test/wall.tga\nspecularExponent 8192\nnormalScale 2\n}\n}\n";
	const char *missing =
		"{\n{\nmap textures/test/wall.tga\nroughness\nblendFunc add\n}\n}\n";
	const char *unknown =
		"{\n{\nmap textures/test/wall.tga\nnormalMapX textures/test/wall_n.tga\n}\n}\n";
	comGameId_t game;
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game = (comGameId_t)( game + 1 ) ) {
		/* the stage reads on past the words: rgbGen, the lightmap stage */
		CHECK( Parse( game, wall ) && !hostWarning[0] );
		CHECK( stages[0].active && stages[1].active );
		CHECK( R_ALICE_GEN( &stages[0] )->rgbGen == CGEN_IDENTITY );
		CHECK( Parse( game, gloss ) && !hostWarning[0] && stages[0].active );
		CHECK( Parse( game, constants ) && !hostWarning[0] && stages[0].active );
		CHECK( Parse( game, exponent ) && !hostWarning[0] && stages[0].active );
		/* a missing value warns and is ignored; the stage reads on */
		CHECK( Parse( game, missing ) && hostWarning[0] && stages[0].active );
		CHECK( ( stages[0].stateBits & GLS_DSTBLEND_BITS ) == GLS_DSTBLEND_ONE );
		/* a word of another name still fails the shader, as before */
		CHECK( !Parse( game, unknown ) && strstr( hostWarning, "normalMapX" ) );
#ifdef RENDERER_RTX
		{
			const rMaterialWords_t *w;
			CHECK( Parse( game, wall ) && stages[0].rtMaterialWords == 1 && stages[1].rtMaterialWords == 0 );
			w = &rtStageWords[0];
			CHECK( w->words && w->scalars && w->normalHeight );
			CHECK( !strcmp( w->normalMap, "textures/test/wall_nh.tga" ) );
			CHECK( !strcmp( w->materialMap, "textures/test/wall_rmos.tga" ) && w->materialType == R_PBR_MAP_RMOS );
			/* metalness, specular, occlusion, one less the gloss */
			CHECK( Near( w->specularScale[0], 0.5f ) && Near( w->specularScale[1], 1.0f ) );
			CHECK( Near( w->specularScale[2], 0.75f ) && Near( w->specularScale[3], 0.4f ) );
			CHECK( w->normalScaleSet && Near( w->normalScale[0], 1.0f ) && Near( w->normalScale[1], -1.0f ) );
			CHECK( w->parallaxDepthSet && Near( w->normalScale[3], 0.08f ) && Near( w->parallaxBias, 0.1f ) );
			/* a specular map takes its colour as it is, and gloss scales it */
			CHECK( Parse( game, gloss ) && rtStageWords[0].materialType == R_PBR_MAP_SPECGLOSS );
			CHECK( Near( rtStageWords[0].specularScale[0], 1.0f ) && Near( rtStageWords[0].specularScale[3], 0.75f ) );
			/* scales alone: reflectance and roughness over rend2's defaults */
			CHECK( Parse( game, constants ) && rtStageWords[0].scalars && !rtStageWords[0].materialMap[0] );
			CHECK( Near( rtStageWords[0].specularScale[1], 0.5f ) && Near( rtStageWords[0].specularScale[3], 0.3f ) );
			/* rend2's gloss for a Phong exponent; one normal scale for both axes */
			CHECK( Parse( game, exponent ) && Near( rtStageWords[0].specularScale[3], 0.0f ) );
			CHECK( Near( rtStageWords[0].normalScale[0], 2.0f ) && Near( rtStageWords[0].normalScale[1], 2.0f ) );
			CHECK( !rtStageWords[0].parallaxDepthSet && Near( rtStageWords[0].normalScale[3], 0.05f ) );
			/* each shader starts again */
			CHECK( Parse( game, "{\n{\nmap textures/test/wall.tga\n}\n}\n" ) && stages[0].rtMaterialWords == 0 );
		}
#endif
	}
	return hostFailures ? 1 : 0;
}
'''


class MaterialWordParseTests(unittest.TestCase):
    def test_every_renderer_reads_past_the_words_under_every_profile(self):
        from alice_shader_parse_tests import RENDERERS, build_host
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtx-pbr-host-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    exe = build_host(Path(temp), f'host-{renderer}', HOST_CHECKS, renderer=renderer)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_parser_words_are_rend2s(self):
        words = read('code/renderercommon/tr_material_words.h')
        for word in ('normalMap', 'normalHeightMap', 'specMap', 'specularMap', 'rmoMap', 'rmosMap', 'moxrMap',
                     'mosrMap', 'ormMap', 'ormsMap', 'specularReflectance', 'specularExponent', 'gloss',
                     'roughness', 'parallaxDepth', 'parallaxBias', 'normalScale', 'specularScale'):
            self.assertIn(f'"{word}"', words, word)
        # rend2's stage defaults (r_baseSpecular 0.04, gloss 0.01, parallax 0.05)
        init = section(words, 'static void R_MaterialWordsInit(', '\n}\n')
        self.assertIn('= 0.04f;', init)
        self.assertIn('words->specularScale[3] = 0.99f;', init)
        self.assertIn('words->normalScale[3] = 0.05f;', init)
        # rend2's gloss from a Phong exponent of 1 to 8192
        self.assertIn('1.0f - logf( value ) / logf( 8192.0f )', words)
        # all three renderers take them in ParseStage, before refusing a word
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            parse = section(read(f'code/{renderer}/tr_shader.c'), 'static qboolean ParseStage(', '\n}\n')
            hook = parse.index('R_MaterialStageWord( token, text, &materialWords, shader.name )')
            self.assertLess(hook, parse.index("WARNING: unknown parameter '%s'"))
            self.assertLess(parse.index('R_MaterialWordsInit( &materialWords );'), hook)


class MaterialRecordTests(unittest.TestCase):
    def test_map_types_agree_everywhere(self):
        words = read('code/renderercommon/tr_material_words.h')
        local = read('code/rendererrtx/tr_local.h')
        common = read('code/rendererrtx/shaders/rt_common.glsl')
        for name in ('SPECGLOSS', 'RMO', 'ORM', 'RMOS', 'MOXR', 'MOSR', 'ORMS'):
            parser = re.search(rf'#define R_PBR_MAP_{name}\s+(\d+)', words)
            host = re.search(rf'#define RT_MATERIAL_MAP_{name}\s+(\d+)', local)
            glsl = re.search(rf'const uint RT_MATERIAL_MAP_{name} = (\d+)u;', common)
            assert parser and host and glsl, name
            self.assertEqual({parser.group(1), host.group(1), glsl.group(1)}, {host.group(1)}, name)
        self.assertIn('typedef char rtPbrMapTypesAgree[', read('code/rendererrtx/tr_shader.c'))
        header = read('code/rendererrtx/rt/rt_gpu.h')
        for name in ('RT_MAPS_TYPE_MASK', 'RT_MAPS_HEIGHT', 'RT_MAPS_EMISSIVE_SHIFT', 'RT_FRAME_PARALLAX'):
            host = re.search(rf'#define {name}\s+(.+)', header)
            glsl = re.search(rf'const uint {name} = ([^;]+);', common)
            assert host and glsl, name
            def value(text):
                return eval(re.sub(r'\b(0x[0-9A-Fa-f]+|\d+)u\b', r'\1', text.split('//')[0]).strip())
            self.assertEqual(value(host.group(1)), value(glsl.group(1)), name)

    def test_finder_order_and_retail_safe_names(self):
        shader = read('code/rendererrtx/tr_shader.c')
        find = section(shader, 'static void R_RtFindMaterialMaps( void )', '\n}\n')
        # a stage's words come first, then the companions in rend2's order,
        # then the names beyond rend2, then the scales alone
        body = find[find.index('shader.rtMaterialStage = -1;'):]
        order = [body.index(s) for s in ('words->normalMap', '"_nh"', '"_n"', 'words->materialType',
                                         'companions[i].suffix', 'words->scalars', '"_emissive"', '"_luma"')]
        self.assertEqual(order, sorted(order))
        table = find[:find.index('};')]
        order = [table.index(s) for s in ('"_specGloss"', '"_rmo"', '"_orm"', '"_rmos"', '"_orms"', '"_moxr"',
                                          '"_mosr"')]
        self.assertEqual(order, sorted(order))
        # names that collide with retail textures are never companions
        for name in ('"_glow"', '"_light"', '"_e"', '"_s"', '"_spec"'):
            self.assertNotIn(name, find)
        # rend2 takes a packed image's channels as they are, and scales a
        # specular map or a map of constants
        self.assertIn('if ( specGloss || map == tr.whiteImage ) {', find)
        self.assertIn('RT_MATERIAL_MAP_ORMS : words->materialType', find)
        self.assertIn('Vector4Set( shader.rtPbrScale, 1.0f, 1.0f, 1.0f, 0.0f );', find)
        # still only a tracing renderer, and only for a lit material
        self.assertIn('!vk_rt_tracing()', find)
        self.assertIn('!R_RtSnapshotLit()', find)

    def test_material_record_carries_the_maps(self):
        header = read('code/rendererrtx/rt/rt_gpu.h')
        self.assertIn('sizeof( rtGpuMaterial_t ) == 128', header)
        translate = section(read('code/rendererrtx/rt/rt_material.c'),
                            'static qboolean RT_TranslateMaterial(', '\n}\n')
        self.assertIn('out->maps[2] = RT_NO_IMAGE << RT_MAPS_EMISSIVE_SHIFT;', translate)
        self.assertIn('out->maps[2] |= RT_MAPS_HEIGHT;', translate)
        self.assertIn('( (uint32_t)emissiveImage << RT_MAPS_EMISSIVE_SHIFT )', translate)
        self.assertIn('Vector4Copy( shader->rtPbrScale, out->pbrScale );', translate)
        self.assertIn('Vector4Copy( shader->rtNormalScale, out->pbrNormal );', translate)
        # an emissive map makes the material a light the sampler holds
        material = read('code/rendererrtx/rt/rt_material.c')
        emission = section(material, 'static qboolean RT_ShaderEmission( const shader_t *shader, vec3_t emission )',
                           '\n}\n')
        self.assertIn('RT_ShaderStageEmission( shader, emission )', emission)
        self.assertIn('RT_ImageAverage( shader->rtEmissiveMap, map );', emission)
        self.assertLess(material.index('static qboolean RT_ShaderEmission('),
                        material.index('static qboolean RT_TranslateMaterial('))


class MaterialShadingTests(unittest.TestCase):
    def setUp(self):
        self.path = read('code/rendererrtx/shaders/rt_path.glsl')

    def test_packings_follow_rend2s_swizzles(self):
        orms = section(self.path, 'vec4 rt_material_orms(', '\n}\n')
        # rend2 tr_image.cpp R_LoadPackedMaterialImage: each packing's
        # occlusion, roughness, metalness and specular; 0.5 where it has no
        # specular channel (rend2's halved scale)
        for line in ('RT_MATERIAL_MAP_RMO) return vec4(t.b, t.r, t.g, 0.5);',
                     'RT_MATERIAL_MAP_RMOS) return vec4(t.b, t.r, t.g, t.a);',
                     'RT_MATERIAL_MAP_MOXR) return vec4(t.g, t.a, t.r, 0.5);',
                     'RT_MATERIAL_MAP_MOSR) return vec4(t.g, t.a, t.r, t.b);',
                     'RT_MATERIAL_MAP_ORM) return vec4(t.r, t.g, t.b, 0.5);'):
            self.assertIn(line, orms)
        self.assertIn('return t;', orms)

    def test_shading_applies_scales_occlusion_and_emission(self):
        shading = section(self.path, 'RtShading rt_shading(', '\n}\n')
        # rend2 lightall.glsl: ORMS *= specularScale.zwxy; F0 from 0.08 * S
        self.assertIn('rt_material_orms(type, texel) * material.pbrScale.zwxy;', shading)
        self.assertIn('sh.F0 = mix(vec3(0.08 * clamp(orms.w, 0.0, 1.0)), min(surf.reflectance, vec3(1.0)), metallic);',
                      shading)
        self.assertIn('sh.diffuse = surf.reflectance * (1.0 - metallic);', shading)
        self.assertIn('sh.occlusion = clamp(orms.x, 0.0, 1.0);', shading)
        self.assertIn('sh.roughness = 1.0 - texel.a * (1.0 - material.pbrScale.w);', shading)
        self.assertIn('* material.pbrScale.xyz;', shading)
        # the normal map's scale, and rend2's decoding
        self.assertIn('vec2 xy = (texels[0].rg * 2.0 - 1.0) * material.pbrNormal.xy;', shading)
        self.assertIn('float z = sqrt(max(1.0 - dot(xy, xy), 0.0));', shading)
        # an emission map adds to what the stages give off
        self.assertIn('surf.emission += linearLight ? rt_linear(glow) : glow;', shading)
        self.assertIn('inout RtSurface surf', shading)
        # a material without maps shades as before
        early = shading[: shading.index('RtStage s = ')]
        for line in ('sh.N = h.normal;', 'sh.diffuse = surf.reflectance;', 'sh.F0 = vec3(material.params.z);',
                     'sh.roughness = material.params.x;', 'sh.occlusion = 1.0;', 'return sh;'):
            self.assertIn(line, early)

    def test_occlusion_takes_the_light_the_rays_bring_back(self):
        main = section(self.path, 'void rt_path_main(', '\n}\n')
        self.assertIn('if (lobe == 0u) indirect = radiance * sh.occlusion;', main)
        self.assertIn('rt_specular_occlusion(NdotV, sh.occlusion, alpha);', main)
        bounce = section(self.path, 'vec3 bounce_radiance(', '\n}\n')
        self.assertIn('throughput *= sh.diffuse * sh.occlusion;', bounce)
        direct = section(self.path, 'ShadeResult shade_direct(', '\n}\n')
        self.assertEqual(direct.count('u_frame.ambient.rgb * sh.occlusion'), 2)
        self.assertNotIn('E * sh.occlusion', direct)
        occlusion = section(self.path, 'float rt_specular_occlusion(', '\n}\n')
        self.assertIn('pow(NdotV + occlusion, exp2(-16.0 * alpha - 1.0)) - 1.0 + occlusion', occlusion)

    def test_parallax_follows_rend2s_search_on_the_primary_view(self):
        parallax = section(self.path, 'void rt_parallax(', '\n}\n')
        for needle in ('(material.maps.z & RT_MAPS_HEIGHT) == 0u', 'RT_FRAME_PARALLAX',
                       'material.pbrNormal.z > 0.0', 'for (int i = 0; i < 15; i++)', 'for (int i = 0; i < 8; i++)',
                       'vec2 ds = -offsetDir.xy * (material.pbrNormal.z / offsetDir.z);',
                       'vec2 offset = ds * (best - bias);', 'h.position += move;',
                       # the depth is one less the height
                       'depth >= 1.0 - textureLod('):
            self.assertIn(needle, parallax)
        # rend2's non-square texture scale
        self.assertIn('size.y <= size.x ? vec3(1.0, size.x / size.y, 1.0) : vec3(size.y / size.x, 1.0, 1.0)',
                      parallax)
        main = section(self.path, 'void rt_path_main(', '\n}\n')
        self.assertLess(main.index('rt_parallax(material, tri, h);'),
                        main.index('RtSurface surf = rt_evaluate_material(material, h, bakedLighting, 0.0);'))
        self.assertNotIn('rt_parallax(', section(self.path, 'vec3 bounce_radiance(', '\n}\n'))
        init = read('code/rendererrtx/tr_init.c')
        self.assertRegex(init, r'rtx_rt_parallax = ri\.Cvar_Get\( "rtx_rt_parallax", "1", CVAR_ARCHIVE_ND \);')
        self.assertIn('RT_FRAME_PARALLAX', read('code/rendererrtx/rt/rt_backend.c'))

    def test_maps_sample_through_their_stage_without_its_frames(self):
        material = read('code/rendererrtx/shaders/rt_material.glsl')
        texel = section(material, 'vec4 rt_stage_texel(', '\n}\n')
        self.assertIn('return rt_image_texel(s, image, h, time, lodBias);', texel)
        shading = section(self.path, 'RtShading rt_shading(', '\n}\n')
        # the three maps through one sampling call site, and the stage's
        # coordinates at one call site in each sampler and tangent frame: the
        # 32-bit driver's compiler must fit the compute trace
        self.assertEqual(shading.count('rt_image_texel('), 1)
        self.assertIn('uint image = k == 0u ? material.maps.x : k == 1u ? material.maps.y : emissive;', shading)
        self.assertNotIn('rt_stage_texel(', shading)
        image = section(material, 'vec4 rt_image_texel(', '\n}\n')
        self.assertEqual(image.count('rt_stage_texcoords('), 1)
        frame = section(self.path, 'bool rt_material_frame(', '\n}\n')
        self.assertEqual(frame.count('rt_stage_texcoords('), 1)


if __name__ == '__main__':
    unittest.main()
