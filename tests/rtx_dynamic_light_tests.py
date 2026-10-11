"""Execute the path tracer's handling of dynamic lights (code/rendererrtx/rt).

RTCW's negdlightshader and flamedlightshader lights are not lights: raster
projects the $dlight image along world z and each of the material's stages
multiplies what a world surface or brush model drew by one minus it (GL_ZERO
GL_ONE_MINUS_SRC_COLOR) or one plus it (GL_DST_COLOR GL_ONE)
(tr_rtcw_light_backend.h). The tracer counts those stages
(RT_MaterialLightPasses) and applies the projection in its shaders
(rt_lighting.glsl rt_material_light_factor), checked here, line for line,
against the C that raster draws with. A still frame's average restarts when
the models or dynamic lights it traces change while the clock stands still
(RT_HashVertices, RT_HashWords).
"""
from pathlib import Path
import re
import unittest

from mesh_tag_tests import function
import rtx_surface_owner_tests

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def section(text, start, end="\n}\n"):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def harness():
    local = read("code/rendererrtx/tr_local.h")
    backend = read("code/rendererrtx/rt/rt_backend.c")
    image = read("code/rendererrtx/tr_image.c")
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
'''
    for name in ("GLS_SRCBLEND_ZERO", "GLS_SRCBLEND_ONE", "GLS_SRCBLEND_DST_COLOR", "GLS_SRCBLEND_BITS",
                 "GLS_DSTBLEND_ZERO", "GLS_DSTBLEND_ONE", "GLS_DSTBLEND_ONE_MINUS_SRC_COLOR", "GLS_DSTBLEND_BITS",
                 "GLS_DEPTHFUNC_EQUAL", "MAX_SHADER_STAGES"):
        source += re.search(rf"#define {name}\s+\w+", local)[0] + "\n"
    source += re.search(r"#define\s+DLIGHT_SIZE\s+\d+", image)[0] + "\n"
    source += r'''
#define IMGFLAG_CLAMPTOEDGE 1
typedef struct { unsigned stateBits; } shaderStage_t;
typedef struct { int numUnfoggedPasses; shaderStage_t *stages[MAX_SHADER_STAGES]; } shader_t;
typedef struct {
    float xyz[4];
    float normal[4];
    float tangent[4];
    float texCoord[2];
    uint32_t color;
    uint32_t materialIndex;
    float lightmapData[4];
} rtxRtPackedVertex_t;
typedef struct { int unused; } image_t;
static image_t dlightImageStorage;
static byte dlightTexels[DLIGHT_SIZE][DLIGHT_SIZE][4];
static struct { image_t *dlightImage; } tr;
static image_t *R_CreateImage( const char *, const char *, const byte *data, int width, int height, int ) {
    assert( width == DLIGHT_SIZE && height == DLIGHT_SIZE );
    memcpy( dlightTexels, data, sizeof( dlightTexels ) );
    return &dlightImageStorage;
}
#include "code/renderercommon/tr_rtcw_light_math.h"
'''
    source += function(backend, "static qboolean RT_MaterialLightPasses(")
    source += function(backend, "static uint32_t RT_HashWords(")
    source += function(backend, "static uint32_t RT_HashVertices(")
    source += function(image, "static void R_CreateDlightImage( void ) {")
    return source


# The shader's formulas, as C: rt_dlight_texel and one light of
# rt_material_light_factor (their lines are pinned by the tests below)
CHECKS = r'''
static float GlslDlightTexel( int x, int y ) {
    x = x < 0 ? 0 : ( x > 15 ? 15 : x );
    y = y < 0 ? 0 : ( y > 15 ? 15 : y );
    const float dx = 7.5f - (float)x, dy = 7.5f - (float)y;
    const float b = floorf( 4000.0f / ( dx * dx + dy * dy ) );
    return b > 255.0f ? 1.0f : ( b < 75.0f ? 0.0f : b / 255.0f );
}

static bool GlslProjection( const float origin[3], float r, const float color[3], const float P[3],
    float st[2], float src[3] ) {
    const float d[3] = { origin[0] - P[0], origin[1] - P[1], origin[2] - P[2] };
    const float height = fabsf( d[2] );
    if ( !( r > 0.0f ) || height > r ) return false;
    const float strength = height < 0.5f * r ? 1.0f : 2.0f * ( r - height ) / r;
    st[0] = 0.5f + d[0] / r;
    st[1] = 0.5f + d[1] / r;
    for ( int i = 0; i < 3; ++i ) {
        const float c = color[i] * strength;
        src[i] = c < 0.0f ? 0.0f : ( c > 1.0f ? 1.0f : c );
    }
    return true;
}

static uint32_t Flip( rtxRtPackedVertex_t *v, int word ) {
    uint32_t bits;
    memcpy( &bits, (byte *)v + word * 4, 4 );
    bits ^= 0x00010000u;
    memcpy( (byte *)v + word * 4, &bits, 4 );
    return bits;
}

int main() {
    /* negdlightshader: one GL_ZERO GL_ONE_MINUS_SRC_COLOR stage darkens */
    shaderStage_t darken = { GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR };
    /* flamedlightshader: two GL_DST_COLOR GL_ONE stages brighten; raster adds
     * GLS_DEPTHFUNC_EQUAL, which is not a blend */
    shaderStage_t brighten = { GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL };
    shaderStage_t additive = { GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE };
    shaderStage_t modulate = { GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO };
    shader_t shader;
    uint32_t up, down;
    memset( &shader, 0, sizeof( shader ) );
    shader.numUnfoggedPasses = 1;
    shader.stages[0] = &darken;
    assert( RT_MaterialLightPasses( &shader, &up, &down ) && up == 0 && down == 1 );
    shader.numUnfoggedPasses = 2;
    shader.stages[0] = &brighten;
    shader.stages[1] = &brighten;
    assert( RT_MaterialLightPasses( &shader, &up, &down ) && up == 2 && down == 0 );
    /* blends the tracer cannot follow are no light at all */
    shader.stages[0] = &additive;
    shader.stages[1] = &modulate;
    assert( !RT_MaterialLightPasses( &shader, &up, &down ) && up == 0 && down == 0 );
    /* a pass count past the stages that exist stops at the first missing one */
    shader.numUnfoggedPasses = 3;
    shader.stages[0] = &darken;
    shader.stages[1] = NULL;
    shader.stages[2] = &darken;
    assert( RT_MaterialLightPasses( &shader, &up, &down ) && down == 1 );
    shader.numUnfoggedPasses = 0;
    assert( !RT_MaterialLightPasses( &shader, &up, &down ) );

    /* the $dlight image, texel for texel, is what the shader computes */
    R_CreateDlightImage();
    assert( tr.dlightImage == &dlightImageStorage );
    int lit = 0;
    for ( int y = 0; y < DLIGHT_SIZE; ++y ) {
        for ( int x = 0; x < DLIGHT_SIZE; ++x ) {
            assert( dlightTexels[y][x][0] == dlightTexels[y][x][1] && dlightTexels[y][x][1] == dlightTexels[y][x][2] );
            volatile float expectedTexel = (float)dlightTexels[y][x][0] / 255.0f;
            volatile float actualTexel = GlslDlightTexel( x, y );
            assert( actualTexel == expectedTexel );
            lit += dlightTexels[y][x][0] != 0;
        }
    }
    /* its edge is dark, so clamping to it lights nothing past the image */
    for ( int i = 0; i < DLIGHT_SIZE; ++i ) {
        assert( !dlightTexels[0][i][0] && !dlightTexels[DLIGHT_SIZE - 1][i][0] );
        assert( !dlightTexels[i][0][0] && !dlightTexels[i][DLIGHT_SIZE - 1][0] );
        assert( GlslDlightTexel( -5, i ) == 0.0f && GlslDlightTexel( i, 40 ) == 0.0f );
    }
    assert( lit > 100 && dlightTexels[7][7][0] == 255 );

    /* the projection RTCW_ProjectLightVertex gives each vertex */
    const float origin[3] = { 10.0f, -20.0f, 30.0f };
    const float color[3] = { 1.0f, 0.5f, 0.25f };
    const float radius = 300.0f;
    int reached = 0;
    for ( int k = 0; k < 400; ++k ) {
        const float P[3] = { origin[0] + (float)( k % 20 - 10 ) * 17.0f, origin[1] + (float)( k / 20 - 10 ) * 13.0f,
            origin[2] + (float)( k % 7 - 3 ) * 110.0f };
        const rtcwLightVertex_t projected = RTCW_ProjectLightVertex( origin, radius, color, P );
        float st[2], src[3];
        if ( !GlslProjection( origin, radius, color, P, st, src ) ) {
            /* beyond the radius above or below: raster draws no light there */
            assert( projected.color[0] == 0 && projected.color[1] == 0 && projected.color[2] == 0 );
            continue;
        }
        ++reached;
        assert( fabsf( st[0] - projected.st[0] ) < 1e-5f && fabsf( st[1] - projected.st[1] ) < 1e-5f );
        for ( int i = 0; i < 3; ++i ) {
            /* raster stores the colour as a byte, rounded or cut */
            assert( fabsf( src[i] * 255.0f - (float)projected.color[i] ) <= 1.0f );
        }
    }
    assert( reached > 200 );

    /* a still frame's hash: every word of a vertex a frozen scene can show
     * changed moves it; the normal and tangent follow the position */
    rtxRtPackedVertex_t vertices[3];
    memset( vertices, 0, sizeof( vertices ) );
    for ( int v = 0; v < 3; ++v ) {
        for ( int word = 0; word < 20; ++word ) {
            const float value = (float)( v * 20 + word ) * 0.25f + 1.0f;
            memcpy( (byte *)&vertices[v] + word * 4, &value, 4 );
        }
    }
    const uint32_t base = RT_HashVertices( 2166136261u, vertices, 3 );
    assert( RT_HashVertices( 2166136261u, vertices, 3 ) == base );
    assert( RT_HashVertices( 2166136261u, vertices, 2 ) != base );
    assert( RT_HashVertices( 12345u, vertices, 3 ) != base );
    // A stationary pose may change its shading normal or tangent alone.
    // Each packed normal/tangent lane therefore restarts accumulation too.
    const int hashed[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
    for ( int v = 0; v < 3; ++v ) {
        for ( int word : hashed ) {
            rtxRtPackedVertex_t copy[3];
            memcpy( copy, vertices, sizeof( copy ) );
            Flip( &copy[v], word );
            assert( RT_HashVertices( 2166136261u, copy, 3 ) != base );
        }
        // The fourth position lane is unused padding.
        for ( int word = 3; word < 4; ++word ) {
            rtxRtPackedVertex_t copy[3];
            memcpy( copy, vertices, sizeof( copy ) );
            Flip( &copy[v], word );
            assert( RT_HashVertices( 2166136261u, copy, 3 ) == base );
        }
    }
    /* two vertices trading places is a different scene */
    {
        rtxRtPackedVertex_t copy[3];
        memcpy( copy, vertices, sizeof( copy ) );
        copy[0] = vertices[1];
        copy[1] = vertices[0];
        assert( RT_HashVertices( 2166136261u, copy, 3 ) != base );
    }
    uint32_t words[5] = { 1, 2, 3, 4, 5 };
    const uint32_t wordsBase = RT_HashWords( 7u, words, 5 );
    for ( int i = 0; i < 5; ++i ) {
        words[i] ^= 0x80000000u;
        assert( RT_HashWords( 7u, words, 5 ) != wordsBase );
        words[i] ^= 0x80000000u;
    }
    assert( RT_HashWords( 7u, words, 5 ) == wordsBase && RT_HashWords( 7u, words, 0 ) == 7u );
    return 0;
}
'''


def entity_light_harness():
    """Raster's per-vertex model light (tr_shade_calc.c RB_CalcDiffuseColor)
    and the tracer's copy of it (rt_backend.c RT_StampEntityLight)."""
    backend = read("code/rendererrtx/rt/rt_backend.c")
    shade = read("code/rendererrtx/tr_shade_calc.c")
    gpu = read("code/rendererrtx/rt/rt_gpu.h")
    source = r"""
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "code/renderercommon/tr_stvef_light.h"
#define myftol( x ) ( (int)( x ) )
#define COM_GAME_QUAKE3_IHV 1000
/* Elite Force rounds its vertex light as retail does (tr_stvef_light.h); the
 * profile checked here is not Elite Force. */
#define COM_GAME_STVEF_SP 1001
#define COM_GAME_STVEF_MP 1002
typedef struct {
    float xyz[4];
    float normal[4];
    float tangent[4];
    float texCoord[2];
    uint32_t color;
    uint32_t materialIndex;
    float lightmapData[4];
} rtxRtPackedVertex_t;
typedef struct {
    struct { struct { int active; } q3IHV; } e;
    vec3_t lightDir;
    vec3_t ambientLight;
    int ambientLightInt;
    vec3_t directedLight;
} trRefEntity_t;
static struct { int gameId; } ri;
static struct { const trRefEntity_t *currentEntity; } backEnd;
static struct { int numVertexes; float normal[64][4]; } tess;
static void R_CelQuantizeModelLighting( const trRefEntity_t *ent, byte color[4] ) { (void)ent; (void)color; }
static void Q3IHV_LightDiffuse( const float normal[3], const float direction[3], const float ambient[3],
    const float directed[3], unsigned char rgba[4] ) {
    (void)normal; (void)direction; (void)ambient; (void)directed; (void)rgba;
    abort();
}
"""
    for name in ("RT_ENTITY_LIGHT_GRID", "RT_ENTITY_LIGHT_ALL"):
        source += re.search(rf"#define {name}\s+0x[0-9A-Fa-f]+u", gpu)[0] + "\n"
    source += function(shade, "static void RB_CalcDiffuseColor_scalar( unsigned char *colors )")
    source += function(backend, "static void RT_StampEntityLight(")
    return source + ENTITY_LIGHT_CHECKS


ENTITY_LIGHT_CHECKS = r"""
static uint32_t seed = 7u;
static float Rand( float lo, float hi ) {
    seed = seed * 1664525u + 1013904223u;
    return lo + ( hi - lo ) * (float)( seed >> 8 ) / 16777216.0f;
}
static void RandomUnit( float out[3] ) {
    float length;
    do {
        out[0] = Rand( -1, 1 ); out[1] = Rand( -1, 1 ); out[2] = Rand( -1, 1 );
        length = sqrtf( out[0] * out[0] + out[1] * out[1] + out[2] * out[2] );
    } while ( length < 0.1f || length > 1.0f );
    out[0] /= length; out[1] /= length; out[2] /= length;
}

int main( void ) {
    trRefEntity_t ent;
    unsigned char colors[64 * 4];
    int trial, i, c;
    memset( &ent, 0, sizeof( ent ) );
    backEnd.currentEntity = &ent;
    for ( trial = 0; trial < 3000; trial++ ) {
        const uint32_t marker = trial & 1 ? RT_ENTITY_LIGHT_ALL : RT_ENTITY_LIGHT_GRID;
        RandomUnit( ent.lightDir );
        for ( c = 0; c < 3; c++ ) {
            /* ambient is clamped at 255 (R_SetupEntityLighting); the directed
               light, dynamic lights folded in, is not */
            ent.ambientLight[c] = trial % 7 == 0 ? 0.0f : Rand( 0, 255 );
            ent.directedLight[c] = trial % 5 == 0 ? Rand( 0, 2000 ) : Rand( 0, 300 );
            ( (byte *)&ent.ambientLightInt )[c] = (byte)myftol( ent.ambientLight[c] );
        }
        ( (byte *)&ent.ambientLightInt )[3] = 255;
        tess.numVertexes = 64;
        for ( i = 0; i < 64; i++ ) {
            RandomUnit( tess.normal[i] );
        }
        RB_CalcDiffuseColor_scalar( colors );
        for ( i = 0; i < 64; i++ ) {
            rtxRtPackedVertex_t v;
            uint32_t word;
            float lane;
            memset( &v, 0, sizeof( v ) );
            memcpy( v.normal, tess.normal[i], sizeof( float ) * 3 );
            RT_StampEntityLight( &v, &ent, ent.lightDir, marker );
            memcpy( &word, &v.xyz[3], sizeof( word ) );
            for ( c = 0; c < 3; c++ ) {
                assert( ( ( word >> ( 8 * c ) ) & 0xFFu ) == colors[i * 4 + c] );
            }
            assert( ( word >> 24 ) == marker );
            /* a normal float, which any copy keeps bit for bit */
            memcpy( &lane, &word, sizeof( lane ) );
            assert( isnormal( lane ) && lane >= 2.0f && lane < 32.0f );
        }
    }
    /* a world vertex's lane (1.0f) carries no light */
    {
        const float one = 1.0f;
        uint32_t word;
        memcpy( &word, &one, sizeof( word ) );
        assert( ( word >> 24 ) != RT_ENTITY_LIGHT_GRID && ( word >> 24 ) != RT_ENTITY_LIGHT_ALL );
    }
    return 0;
}
"""


class RtxDynamicLightTests(unittest.TestCase):
    def test_material_lights_and_the_still_frame_hash_run_as_written(self):
        rtx_surface_owner_tests.compile_run(self, harness() + CHECKS)

    def test_a_models_lightmap_mode_light_is_the_light_raster_draws_it_with(self):
        # In lightmap mode a model takes the light the raster pass gave its
        # vertices, the entity's grid light by each normal, clamped as raster
        # clamps it; the shaders interpolate it as raster's colours are
        rtx_surface_owner_tests.compile_run(self, entity_light_harness())
        geometry = read("code/rendererrtx/shaders/rt_geometry.glsl")
        attribs = section(geometry, "RtHitAttribs rt_hit_attribs(")
        self.assertIn("uint marker = w0 >> 24u;", attribs)
        self.assertIn("if (marker == RT_ENTITY_LIGHT_GRID || marker == RT_ENTITY_LIGHT_ALL) {", attribs)
        self.assertIn("h.entityLight.w = marker == RT_ENTITY_LIGHT_ALL ? 2.0 : 1.0;", attribs)
        material = read("code/rendererrtx/shaders/rt_material.glsl")
        evaluate = section(material, "RtSurface rt_evaluate_material(")
        self.assertIn("if (bakedLighting && ((s.state.y >> 8u) & 0xFFu) == CGEN_LIGHTING_DIFFUSE && h.entityLight.w > 0.5) {",
                      evaluate)
        self.assertIn("surf.baked = h.entityLight.rgb;", evaluate)
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        self.assertIn("return bakedLighting && ((material.flags & (RT_MF_LIGHTMAPPED | RT_MF_VERTEX_LIT)) != 0u || "
                      "surf.bakedEntity != 0u);", section(path, "bool baked_light("))
        direct = section(path, "ShadeResult shade_direct(")
        folded = direct[direct.index("if (surf.bakedEntity == 2u) {"):]
        self.assertIn("exclude |= RT_LIGHT_DYNAMIC;", folded[:folded.index("}")])
        # the host stamps a model the raster pass lit, marking whether its
        # light holds the dynamic lights (all but the per-pixel light pass)
        backend = read("code/rendererrtx/rt/rt_backend.c")
        stamp = section(backend, "static void RT_StampEntityVertices(")
        self.assertIn("const uint32_t marker = R_GetDlightMode() == 2 ? RT_ENTITY_LIGHT_GRID : RT_ENTITY_LIGHT_ALL;", stamp)
        self.assertIn("vk_rt_transform_direction( ent, ent->lightDir, lightDir );", stamp)
        self.assertIn("if ( R_GetDlightMode() == 2 && !( R_UsesRTCWLighting() && refdef->rtcw.snooperView ) ) {",
                      read("code/rendererrtx/tr_light.c"))
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        gpu = read("code/rendererrtx/rt/rt_gpu.h")
        for name in ("RT_ENTITY_LIGHT_GRID", "RT_ENTITY_LIGHT_ALL"):
            self.assertEqual(re.search(rf"#define {name}\s+(0x[0-9A-Fa-f]+)u", gpu).group(1).lower(),
                             re.search(rf"const uint {name} = (0x[0-9A-Fa-f]+)u;", common).group(1).lower())

    def test_the_checks_port_the_shader_lines_they_stand_for(self):
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        texel = section(lighting, "float rt_dlight_texel(")
        self.assertIn("vec2 d = vec2(7.5) - vec2(clamp(t, ivec2(0), ivec2(15)));", texel)
        self.assertIn("float b = floor(4000.0 / dot(d, d));", texel)
        self.assertIn("return b > 255.0 ? 1.0 : (b < 75.0 ? 0.0 : b / 255.0);", texel)
        image = section(lighting, "float rt_dlight_image(")
        # bilinear, as the image is sampled; far points clamp to its dark edge
        self.assertIn("vec2 p = clamp(st, vec2(-1.0), vec2(2.0)) * 16.0 - 0.5;", image)
        factor = section(lighting, "vec3 rt_material_light_factor(")
        for line in ("vec3 d = light.position.xyz - P;",
                     "float height = abs(d.z);",
                     "if (!(r > 0.0) || height > r) {",
                     "float strength = height < 0.5 * r ? 1.0 : 2.0 * (r - height) / r;",
                     "vec3 src = clamp(light.color.rgb * strength, 0.0, 1.0) * rt_dlight_image(vec2(0.5) + d.xy / r);",
                     "factor *= vec3(1.0) + src;",
                     "factor *= vec3(1.0) - src;"):
            self.assertIn(line, factor)
        # the stage counts the tracer records are the ones it multiplies by
        self.assertLess(factor.index("light.extra.x"), factor.index("factor *= vec3(1.0) + src;"))
        self.assertLess(factor.index("light.extra.y"), factor.index("factor *= vec3(1.0) - src;"))
        backend = read("code/rendererrtx/rt/rt_backend.c")
        upload = section(backend, "static qboolean RT_UploadLights( rtFrame_t *frame )")
        self.assertIn("light->extra[0] = (float)MIN( brighten, 8u );", upload)
        self.assertIn("light->extra[1] = (float)MIN( darken, 8u );", upload)
        # raster's per-vertex projection, which the checks run
        projection = read("code/renderercommon/tr_rtcw_light_math.h")
        self.assertIn("result.st[0] = 0.5f + dist[0] * inverse;", projection)
        self.assertIn("strength = height < radius * 0.5f ? 1.0f : 2.0f * (radius - height) * inverse;", projection)
        light_pass = read("code/renderercommon/tr_rtcw_light_backend.h")
        self.assertIn("for ( pass = 0; pass < light->rtcwShader->numUnfoggedPasses; ++pass )", light_pass)


if __name__ == "__main__":
    unittest.main()
