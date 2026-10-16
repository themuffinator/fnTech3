"""Raven light styles that change, through the renderers' own code.

The client game sets a style's colour every frame (Academy cgame import 220,
Outcast's 216); the renderer keeps the 64 colours and recomposes, before the
next world view, the lightmap texels, vertex colours and grid cells a changed
style lights, re-uploading the pages it touched
(code/renderercommon/tr_raven_light_styles.h). The probe compiles that
production header against a small renderer world, and the wiring checks read
each renderer's and the clients' production text. See
docs/fntech3/2026-10-16-raven-light-styles-animated.md.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args, engine_fixture_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PROBE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/renderercommon/tr_raven_bsp_format.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::printf("%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

typedef enum { SF_BAD, SF_SKIP, SF_FACE, SF_GRID, SF_TRIANGLES } surfaceType_t;
struct srfSurfaceFace_t { surfaceType_t surfaceType; int numPoints; float points[3][8]; };
struct srfTriangles_t { surfaceType_t surfaceType; int numVerts; drawVert_t *verts; };
struct srfGridMesh_t { surfaceType_t surfaceType; int width, height; drawVert_t verts[25]; };
struct msurface_t { surfaceType_t *data; qboolean ravenStyled; };
struct world_t {
    char name[64]; fnRavenBspView_t ravenBsp; msurface_t *surfaces; int numsurfaces;
    byte *lightGridData; struct fnRavenStyleWorld_s *ravenStyles;
};
struct image_t { int id; };
static world_t s_worldData;
static byte *fileBase;
static struct { image_t **lightmaps; int numLightmaps; qboolean mergeLightmaps; int lightmapMod; world_t *world; } tr;
#define LIGHTMAP_SIZE 128
#define LIGHTMAP_BORDER 2
#define LIGHTMAP_LEN (LIGHTMAP_SIZE + LIGHTMAP_BORDER*2)
static int lightmapCountX;

// The renderer's own lightmap processing is out of scope: this one keeps the
// raw composed page it was handed and the upload it led to.
static std::vector<byte> processed;
struct Upload { int image, x, y, width, height; std::vector<byte> page; };
static std::vector<Upload> uploads;
static float R_ProcessLightmap(byte *image, const byte *buf, float) {
    processed.assign(buf, buf + LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3); image[0] = 0x77; return 0;
}
static void R_UploadSubImage(byte *data, int x, int y, int width, int height, image_t *image) {
    CHECK(data[0] == 0x77); uploads.push_back({image->id, x, y, width, height, processed});
}
static void R_ColorShiftLightingBytes(const byte in[4], byte out[4], qboolean hasAlpha) {
    out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; if (hasAlpha) out[3] = in[3];
}
static cvar_t debugStyle;
static std::string cvarName, cvarValue; static int cvarFlags;
static cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    cvarName = name; cvarValue = value; cvarFlags = flags; return &debugStyle;
}
static std::vector<void *> hunk;
static void *Hunk(int size, ha_pref) { CHECK(size > 0); void *p = std::calloc(1, (size_t)size); CHECK(p); hunk.push_back(p); return p; }
static void Error(int, const char *format, ...) {
    char message[256]; va_list args; va_start(args, format); std::vsnprintf(message, sizeof(message), format, args); va_end(args);
    throw std::runtime_error(message);
}
static struct { decltype(&Hunk) Hunk_Alloc; decltype(&Error) Error; decltype(&Cvar_Get) Cvar_Get; } ri{Hunk, Error, Cvar_Get};
#define Com_Memset memset
#define Com_Memcpy memcpy

#include "code/renderercommon/tr_raven_light_styles.h"

// A Raven map in memory: the records FN_RavenBspBuildStyles reads.
//  0: a lightmapped face on page 1, its second channel style 1 from page 0 moved ten texels
//  1: a vertex-lit face, vertex styles 0 and 5
//  2: a vertex-lit triangle soup, a lone style 9
//  3: a vertex-lit 3x3 patch, vertex styles 0 and 5
//  4: a lightmapped face with no later channel
// The grid: row 0 style 0 alone, row 1 styles 0 and 1; cells use rows 1, 0, 1.
static std::vector<byte> map;
static fnRavenBspView_t view;
static void PutF(size_t at, float v) { std::memcpy(&map[at], &v, 4); }
static void PutI(size_t at, int v) { std::memcpy(&map[at], &v, 4); }
static void Build() {
    const int vertices = 18, surfaces = 5, pageBytes = 128 * 128 * 3;
    view = {};
    int at = 0;
    auto region = [&](int lump, int bytes) { view.offsets[lump] = at; view.lengths[lump] = bytes; at += bytes; };
    region(10, vertices * 80); region(11, 12); region(13, surfaces * 148); region(14, 2 * pageBytes);
    region(15, 2 * 30); region(17, 3 * 2);
    map.assign((size_t)at, 0);
    view.bytes = map.data(); view.length = at; view.vertices = vertices; view.surfaces = surfaces;
    view.gridRows = 2; view.gridCells = 3;
    PutI(view.offsets[11], 0); PutI(view.offsets[11] + 4, 1); PutI(view.offsets[11] + 8, 2);
    const float corners[3][2] = {{0.5f, 0.5f}, {4.5f, 0.5f}, {0.5f, 4.5f}};
    for (int v = 0; v < vertices; ++v) {
        size_t base = (size_t)view.offsets[10] + (size_t)v * 80;
        for (int c = 0; c < 16; ++c) map[base + 64 + (size_t)c] = (byte)(10 * v + c + 1);
    }
    for (int v = 0; v < 3; ++v) {
        size_t base = (size_t)view.offsets[10] + (size_t)v * 80;
        PutF(base + 20, corners[v][0] / 128); PutF(base + 24, corners[v][1] / 128);
        PutF(base + 28, (corners[v][0] + 10) / 128); PutF(base + 32, corners[v][1] / 128);
    }
    for (int v = 9; v < 18; ++v) { // the patch's own coordinates: linear
        size_t base = (size_t)view.offsets[10] + (size_t)v * 80;
        PutF(base + 20, (float)((v - 9) % 3) * 0.25f); PutF(base + 24, (float)((v - 9) / 3) * 0.125f);
    }
    for (int s = 0; s < surfaces; ++s) {
        size_t base = (size_t)view.offsets[13] + (size_t)s * 148;
        std::memset(&map[base + 28], 255, 8);
        map[base + 28] = 0; map[base + 32] = 0;
        for (int j = 0; j < 4; ++j) PutI(base + 36 + (size_t)j * 4, -1);
    }
    auto surface = [&](int s, int type, int first, int count, int lightmap) {
        size_t base = (size_t)view.offsets[13] + (size_t)s * 148;
        PutI(base + 8, type); PutI(base + 12, first); PutI(base + 16, count);
        PutI(base + 20, 0); PutI(base + 24, type == 2 ? 0 : 3); PutI(base + 36, lightmap);
        return base;
    };
    size_t s0 = surface(0, 1, 0, 3, 1); map[s0 + 29] = 1; PutI(s0 + 40, 0);
    size_t s1 = surface(1, 1, 3, 3, -3); map[s1 + 33] = 5;
    size_t s2 = surface(2, 3, 6, 3, -3); map[s2 + 32] = 9;
    size_t s3 = surface(3, 2, 9, 9, -3); map[s3 + 33] = 5; PutI(s3 + 140, 3); PutI(s3 + 144, 3);
    surface(4, 1, 0, 3, 0);
    std::memset(&map[(size_t)view.offsets[14]], 0x5a, (size_t)(2 * pageBytes));
    for (int y = 0; y < 128; ++y) for (int x = 10; x < 16; ++x) map[(size_t)view.offsets[14] + (size_t)(y * 128 + x) * 3] = 130;
    for (int row = 0; row < 2; ++row) {
        size_t base = (size_t)view.offsets[15] + (size_t)row * 30;
        for (int c = 0; c < 24; ++c) map[base + (size_t)c] = (byte)(20 + row * 40 + c);
        map[base + 24] = 0; map[base + 25] = row ? 1 : 255; map[base + 26] = map[base + 27] = 255;
    }
    const byte cells[6] = {1, 0, 0, 0, 1, 0};
    std::memcpy(&map[(size_t)view.offsets[17]], cells, 6);
}

static srfSurfaceFace_t lit;
static drawVert_t soupVerts[3];
static srfTriangles_t soup;
static srfGridMesh_t patch;
static surfaceType_t faceType = SF_FACE, otherFace = SF_FACE;
static msurface_t surfaces[5];
static image_t images[2] = {{0}, {1}};
static image_t *lightmapImages[2] = {&images[0], &images[1]};
static byte gridData[24];

static void Load(qboolean merged) {
    for (void *p : hunk) std::free(p);
    hunk.clear(); uploads.clear();
    Build();
    s_worldData = {}; std::snprintf(s_worldData.name, sizeof(s_worldData.name), "maps/generated.bsp");
    s_worldData.ravenBsp = view;
    lit = {}; lit.surfaceType = SF_FACE; lit.numPoints = 3;
    soup = {}; soup.surfaceType = SF_TRIANGLES; soup.numVerts = 3; soup.verts = soupVerts;
    patch = {}; patch.surfaceType = SF_GRID; patch.width = 5; patch.height = 5;
    for (int i = 0; i < 25; ++i) { // a tessellation: each vertex carries where it lies
        patch.verts[i].lightmap[0] = (float)(i % 5) * 0.25f; patch.verts[i].lightmap[1] = (float)(i / 5) * 0.25f;
    }
    surfaces[0].data = &faceType; surfaces[1].data = &lit.surfaceType; surfaces[2].data = &soup.surfaceType;
    surfaces[3].data = &patch.surfaceType; surfaces[4].data = &otherFace;
    for (auto &surface : surfaces) surface.ravenStyled = qfalse;
    s_worldData.surfaces = surfaces; s_worldData.numsurfaces = 5;
    std::memset(gridData, 0xee, sizeof(gridData)); s_worldData.lightGridData = gridData;
    // The projected pages: page 1 composed at white where surface 0 lies.
    fnRavenBspStyles_t records; fnRavenBspError_t error;
    CHECK(FN_RavenBspBuildStyles(&view, [](int size) { return std::calloc(1, (size_t)size); }, &records, &error));
    static std::vector<byte> file; file.assign(map.begin() + view.offsets[14], map.begin() + view.offsets[14] + view.lengths[14]);
    CHECK(FN_RavenBspRecomposePage(&view, &records, 1, ~uint64_t(0), nullptr, file.data()));
    fileBase = file.data();
    tr = {}; tr.lightmaps = lightmapImages; tr.numLightmaps = merged ? 1 : 2; tr.mergeLightmaps = merged;
    tr.lightmapMod = merged ? 4 : MAX_QINT; lightmapCountX = 2;
    lump_t lump{0, view.lengths[14]};
    R_RavenInitLightStyles(&lump);
    tr.world = &s_worldData;
}

static void Texel(const std::vector<byte> &page, int x, int y, int r, int g, int b) {
    const byte *p = &page[(size_t)(y * 128 + x) * 3];
    if (p[0] != r || p[1] != g || p[2] != b) { std::printf("texel %d,%d = %d %d %d, not %d %d %d\n", x, y, p[0], p[1], p[2], r, g, b); std::exit(1); }
}

int main() {
    const byte *vertexes = nullptr;
    unsigned char white[FN_RAVEN_BSP_LIGHT_STYLES][4];
    std::memset(white, 255, sizeof(white));
    debugStyle.integer = -1;
    Load(qfalse);
    CHECK(cvarName == "r_debugStyle" && cvarValue == "-1" && cvarFlags == CVAR_CHEAT);
    CHECK(s_worldData.ravenStyles);
    const fnRavenBspStyles_t &records = s_worldData.ravenStyles->records;
    CHECK(records.numGroups == 1 && records.numSurfaces == 3 && records.numCells == 2);
    // Styled vertex light leaves the static vertex buffer; lightmapped faces stay.
    CHECK(!surfaces[0].ravenStyled && surfaces[1].ravenStyled && surfaces[2].ravenStyled && surfaces[3].ravenStyled && !surfaces[4].ravenStyled);
    // The patch's own lightmap coordinates are back where its vertices lie.
    for (int i = 0; i < 25; ++i) {
        float u = (float)(i % 5) * 0.25f, v = (float)(i / 5) * 0.25f;
        CHECK(std::fabs(patch.verts[i].lightmap[0] - u * 0.5f) < 1e-6f && std::fabs(patch.verts[i].lightmap[1] - v * 0.25f) < 1e-6f);
    }
    // The first world view composes everything once at the colours held: the
    // one styled page, the styled vertices and the styled grid cells.
    R_RavenUpdateLightStyles();
    CHECK(uploads.size() == 1 && uploads[0].image == 1 && uploads[0].x == 0 && uploads[0].y == 0 && uploads[0].width == 128 && uploads[0].height == 128);
    Texel(uploads[0].page, 0, 0, 0x5a + 130, 0x5a + 0x5a, 0x5a + 0x5a);
    Texel(uploads[0].page, 3, 3, 0x5a, 0x5a, 0x5a);
    vertexes = view.bytes + view.offsets[10];
    for (int i = 0; i < 3; ++i) {
        byte expect[4];
        FN_RavenBspComposeVertexColor(vertexes + (3 + i) * 80, view.bytes + view.offsets[13] + 148, nullptr, expect);
        CHECK(!std::memcmp(&lit.points[i][7], expect, 4));
        CHECK(!std::memcmp(soupVerts[i].color.rgba, vertexes + (6 + i) * 80 + 64, 4)); // lone style at white: the map's colour
    }
    for (int i = 0; i < 25; ++i) {
        byte expect[4];
        FN_RavenBspPatchColor(&view, view.bytes + view.offsets[13] + 3 * 148, (float)(i % 5) * 0.25f, (float)(i / 5) * 0.25f, nullptr, expect);
        CHECK(!std::memcmp(patch.verts[i].color.rgba, expect, 4));
    }
    {
        byte expect[8];
        FN_RavenBspComposeGrid(view.bytes + view.offsets[15] + 30, nullptr, expect);
        CHECK(!std::memcmp(gridData, expect, 6) && !std::memcmp(gridData + 16, expect, 6));
        CHECK(gridData[8] == 0xee && gridData[6] == 0xee); // a style-0 cell and the cells' tails are untouched
    }
    // Nothing changed: no work.
    uploads.clear(); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    const byte same[4] = {255, 255, 255, 255}; RE_SetRavenLightStyle(1, same); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    // Style 1 changes: its page alone is recomposed and uploaded; vertex light is untouched.
    const byte magenta[4] = {128, 0, 255, 255};
    const auto litBefore = lit; const auto patchBefore = patch;
    RE_SetRavenLightStyle(1, magenta); R_RavenUpdateLightStyles();
    CHECK(uploads.size() == 1 && uploads[0].image == 1);
    Texel(uploads[0].page, 0, 0, 0x5a + 65, 0x5a, 0x5a + 0x5a);
    CHECK(!std::memcmp(&lit, &litBefore, sizeof(lit)) && !std::memcmp(&patch, &patchBefore, sizeof(patch)));
    {
        unsigned char colors[FN_RAVEN_BSP_LIGHT_STYLES][4]; std::memcpy(colors, white, sizeof(colors)); std::memcpy(colors[1], magenta, 4);
        byte expect[8];
        FN_RavenBspComposeGrid(view.bytes + view.offsets[15] + 30, colors, expect);
        CHECK(!std::memcmp(gridData, expect, 6) && gridData[8] == 0xee);
    }
    // A style no record uses changes nothing.
    uploads.clear(); const byte grey[4] = {9, 9, 9, 255}; RE_SetRavenLightStyle(40, grey); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    // Style 5: the vertex-lit face and patch, and no page.
    RE_SetRavenLightStyle(5, grey); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    CHECK(std::memcmp(&lit, &litBefore, sizeof(lit)) && std::memcmp(&patch, &patchBefore, sizeof(patch)));
    {
        unsigned char colors[FN_RAVEN_BSP_LIGHT_STYLES][4]; std::memcpy(colors, white, sizeof(colors));
        std::memcpy(colors[1], magenta, 4); std::memcpy(colors[5], grey, 4); std::memcpy(colors[40], grey, 4);
        byte expect[4];
        FN_RavenBspComposeVertexColor(vertexes + 3 * 80, view.bytes + view.offsets[13] + 148, colors, expect);
        CHECK(!std::memcmp(&lit.points[0][7], expect, 4));
    }
    // Retail single player's r_debugStyle: every style black but the one named.
    debugStyle.integer = 9; uploads.clear(); R_RavenUpdateLightStyles();
    CHECK(uploads.size() == 1);
    Texel(uploads[0].page, 0, 0, 0x5a, 0x5a, 0x5a);
    CHECK(!std::memcmp(soupVerts[0].color.rgba, vertexes + 6 * 80 + 64, 4));
    {
        const byte *colour = (const byte *)&lit.points[0][7];
        CHECK(colour[0] == 0 && colour[1] == 0 && colour[2] == 0);
    }
    debugStyle.integer = 64; uploads.clear(); R_RavenUpdateLightStyles(); // no style is white
    CHECK(soupVerts[0].color.rgba[0] == 0 && uploads.empty());
    debugStyle.integer = -1;
    // Out-of-range styles stop as retail's renderer stops.
    for (int bad : {-1, 64, 1000}) {
        bool refused = false;
        try { RE_SetRavenLightStyle(bad, grey); } catch (const std::runtime_error &e) { refused = std::strstr(e.what(), "out of range"); }
        CHECK(refused);
    }
    { bool refused = false; try { RE_SetRavenLightStyle(3, nullptr); } catch (const std::runtime_error &) { refused = true; } CHECK(refused); }
    // No world, or a world with no styled records: nothing to do.
    RE_SetRavenLightStyle(1, same); tr.world = nullptr; uploads.clear(); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    world_t plain{}; tr.world = &plain; R_RavenUpdateLightStyles(); CHECK(uploads.empty());

    // Merged lightmaps: page 1 is the second cell of atlas 0.
    Load(qtrue);
    R_RavenUpdateLightStyles();
    CHECK(uploads.size() == 1 && uploads[0].image == 0 && uploads[0].x == LIGHTMAP_LEN && uploads[0].y == 0 && uploads[0].width == LIGHTMAP_LEN);
    // The colours outlive the map, as retail's do: style 1 is white again
    // only because it was set white above.
    Texel(uploads[0].page, 0, 0, 0x5a + 130, 0x5a + 0x5a, 0x5a + 0x5a);
    // A map with no lightmaps (vertex lighting) composes vertex light only.
    tr.lightmaps = nullptr; uploads.clear(); RE_SetRavenLightStyle(1, magenta); R_RavenUpdateLightStyles(); CHECK(uploads.empty());
    std::printf("raven light styles: ok\n");
    for (void *p : hunk) std::free(p);
}
'''


def section(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


class RavenLightStylesTests(unittest.TestCase):
    def test_renderer_recomposes_what_a_changed_style_lights(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='raven-styles-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            probe = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            probe.write_text(PROBE)
            built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                                    '-Wno-unused-function', '-I', str(ROOT), str(probe), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertIn('raven light styles: ok', ran.stdout)

    def test_each_renderer_wires_the_styles(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                code = ROOT / 'code' / renderer
                bsp = (code / 'tr_bsp.c').read_text()
                self.assertLess(bsp.index('#include "../renderercommon/tr_raven_bsp_load.h"'),
                                bsp.index('#include "../renderercommon/tr_raven_light_styles.h"'))
                load = section(bsp, 'void RE_LoadWorldMap( const char *name ) {', '\n}\n')
                grid = load.index('R_LoadRavenLightGrid( &header->lumps[LUMP_LIGHTGRID] );')
                styles = load.index('R_RavenInitLightStyles( &header->lumps[LUMP_LIGHTMAPS] );')
                self.assertLess(grid, styles)
                self.assertLess(styles, load.index('R_BuildWorldVBO( s_worldData.surfaces, s_worldData.numsurfaces );'))
                self.assertIn('re.SetRavenLightStyle = RE_SetRavenLightStyle;', (code / 'tr_init.c').read_text())
                scene = section((code / 'tr_scene.c').read_text(), 'void RE_RenderScene( const refdef_t *fd ) {', '\n}\n')
                self.assertLess(scene.index('R_RenderScene: NULL worldmodel'), scene.index('R_RavenUpdateLightStyles();'))
                self.assertIn('if ( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {\n\t\tR_RavenUpdateLightStyles();',
                              scene.replace('\r\n', '\n'))
                local = (code / 'tr_local.h').read_text()
                self.assertIn('struct fnRavenStyleWorld_s *ravenStyles;', local)
                self.assertIn('qboolean\t\t\travenStyled;', local)
                vbo = (code / ('tr_vbo.c' if renderer == 'renderer' else 'vk_vbo.c')).read_text()
                for kind in ('SF_FACE', 'SF_TRIANGLES', 'SF_GRID'):
                    self.assertIn(f'surfaceType == {kind} && !sf->ravenStyled && isStaticShader( sf->shader )', vbo)
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text()
        self.assertIn('void (*SetRavenLightStyle)( int style, const byte color[4] );', public)

    def test_clients_hand_the_colours_to_the_renderer(self):
        academy = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text()
        case = section(academy, 'case Call::CG_R_SET_LIGHT_STYLE: {', 'case Call::CG_GET_ENTITY_TOKEN:')
        self.assertIn('re.SetRavenLightStyle(static_cast<int>(args[1]),color.data());', case)
        self.assertNotIn('Modulation', academy)
        outcast = (ROOT / 'code/client/client_jk2_cgame.hpp').read_text()
        body = section(outcast, '\tcase 215: {', '\tdefault:')
        self.assertIn('re.SetRavenLightStyle( static_cast<int>( args[1] ), color.data() );', body)
        # Run Outcast's two cases as written, against the shared store.
        source = r'''
#include "code/compat/jka_lightstyle.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
typedef unsigned char byte;
enum { ERR_DROP = 1 };
static void Com_Error(int, const char *message) { throw std::runtime_error(message); }
static struct { int unused; } *cgvm;
static unsigned char vmMemory[64];
static int checked;
static void VM_CheckBounds(const void *, intptr_t address, size_t size) {
    if (address < 0 || static_cast<size_t>(address) + size > sizeof(vmMemory)) throw std::runtime_error("bounds");
    ++checked;
}
#define VM_CHECKBOUNDS VM_CheckBounds
#define VMA(index) static_cast<void *>(vmMemory + args[index])
static int calls, lastStyle; static byte lastColor[4];
static void SetStyle(int style, const byte color[4]) { ++calls; lastStyle = style; std::memcpy(lastColor, color, 4); }
static struct { void (*SetRavenLightStyle)(int, const byte[4]); } re{SetStyle};
static fntech3::jka::lightstyle::Store jk2CgameLightStyles;
static bool Call(intptr_t *args) {
    switch (args[0]) {
''' + body + r'''
    }
    return false;
}
static bool Refused(intptr_t *args) { try { Call(args); } catch (const std::runtime_error &) { return true; } return false; }
int main() {
    intptr_t get[]{215, 63, 8};
    assert(Call(get) && checked == 1 && vmMemory[8] == 255 && vmMemory[11] == 255 && vmMemory[12] == 0);
    intptr_t set[]{216, 63, static_cast<int32_t>(UINT32_C(0x817f1305))};
    assert(Call(set) && calls == 1 && lastStyle == 63 && lastColor[0] == 5 && lastColor[1] == 19 && lastColor[2] == 127 && lastColor[3] == 129);
    assert(Call(get) && vmMemory[8] == 5 && vmMemory[11] == 129);
    for (intptr_t bad : {intptr_t(-1), intptr_t(64)}) {
        intptr_t badSet[]{216, bad, 0}, badGet[]{215, bad, 8};
        assert(Refused(badSet) && Refused(badGet));
    }
    assert(calls == 1);
    intptr_t outside[]{215, 63, 62}; assert(Refused(outside));
    re.SetRavenLightStyle = nullptr; set[1] = 0; assert(Call(set) && calls == 1);
    get[1] = 0; assert(Call(get) && vmMemory[8] == 5);
    std::puts("outcast light styles: ok");
}
'''
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang C++ compiler unavailable')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='raven-styles-jk2-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            probe = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            probe.write_text(source)
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                                    str(probe), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
