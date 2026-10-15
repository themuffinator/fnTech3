"""Academy's zoom imports: turned pictures (214, 215) and the ranged fog (216).

The geometry and fog policies run in C and C++; the production cgame cases run
against a stand-in HUD, and each renderer's production fog functions run with
the ranged fog set and cleared."""
from pathlib import Path
import math
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

FOG_POLICY = r'''
#include "code/renderercommon/tr_jka_ranged_fog.h"
#include <assert.h>
#include <math.h>
#include <string.h>
static int near_(float a, float b) { return fabsf(a - b) < 0.001f; }
int main(void) {
    float start = 7.0f, end = 9.0f, scale = 3.0f, nan_, inf_;
    unsigned bits = 0x7fc00000u;
    memcpy(&nan_, &bits, sizeof(nan_)); bits = 0x7f800000u; memcpy(&inf_, &bits, sizeof(inf_));
    /* Zero is the volume's own fog: nothing changes. */
    assert(!FnJKA_RangedFogSpan(0.0f, 1500.0f, 6000.0f, &start, &end) && start == 7.0f && end == 9.0f);
    assert(!FnJKA_RangedFogScale(0.0f, 1500.0f, 6000.0f, &scale, &start) && scale == 3.0f);
    /* Ramp from depthForOpaque already at least range long (retail keeps it). */
    assert(FnJKA_RangedFogSpan(640.0f, 1500.0f, 6000.0f, &start, &end) && start == 1500.0f && end == 6000.0f);
    /* A shorter ramp is lengthened to range: binoculars at fov 80 send 5120. */
    assert(FnJKA_RangedFogSpan(5120.0f, 1500.0f, 6000.0f, &start, &end) && start == 880.0f && end == 6000.0f);
    /* ... but never starts nearer than 16 units. */
    assert(FnJKA_RangedFogSpan(5990.0f, 1500.0f, 6000.0f, &start, &end) && start == 16.0f && end == 6000.0f);
    /* Negative: cg_linearFogOverride's start distance, whatever the depth. */
    assert(FnJKA_RangedFogSpan(-300.0f, 1500.0f, 6000.0f, &start, &end) && start == 300.0f && end == 6000.0f);
    /* A ramp that would end before it starts gets one unit. */
    assert(FnJKA_RangedFogSpan(-7000.0f, 1500.0f, 6000.0f, &start, &end) && start == 7000.0f && end == 7001.0f);
    /* A volume opaque beyond the limit still gets a range-long ramp. */
    assert(FnJKA_RangedFogSpan(640.0f, 9000.0f, 6000.0f, &start, &end) && start == 5360.0f && end == 6000.0f);
    /* The pass reaches full fog (s * 8 == 1) at the ramp's end. */
    assert(FnJKA_RangedFogScale(5120.0f, 1500.0f, 6000.0f, &scale, &start) && start == 880.0f);
    assert(near_((6000.0f - start) * scale * 8.0f, 1.0f) && near_((3440.0f - start) * scale * 8.0f, 0.5f));
    /* Non-finite input and missing outputs leave everything alone. */
    start = 7.0f; scale = 3.0f;
    assert(!FnJKA_RangedFogSpan(nan_, 1500.0f, 6000.0f, &start, &end));
    assert(!FnJKA_RangedFogSpan(inf_, 1500.0f, 6000.0f, &start, &end));
    assert(!FnJKA_RangedFogSpan(-inf_, 1500.0f, 6000.0f, &start, &end));
    assert(!FnJKA_RangedFogSpan(640.0f, nan_, 6000.0f, &start, &end));
    assert(!FnJKA_RangedFogSpan(640.0f, 1500.0f, inf_, &start, &end));
    assert(!FnJKA_RangedFogSpan(640.0f, 1500.0f, 6000.0f, NULL, &end));
    assert(!FnJKA_RangedFogScale(640.0f, 1500.0f, 6000.0f, NULL, &start));
    assert(start == 7.0f && scale == 3.0f);
    /* Too far out for one more unit to register: the volume's own fog. */
    assert(!FnJKA_RangedFogScale(-3.0e38f, 1500.0f, 6000.0f, &scale, &start) && scale == 3.0f);
    return 0;
}
'''

ADAPTER = r'''
#include "code/compat/jka_draw_adapter.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <initializer_list>
using namespace fntech3::jka::draw;
static bool near_(double a, double b) { return std::fabs(a - b) < 0.002; }
// Retail's backend: translate, glRotatef(degrees, 0, 0, 1), then the local
// corners (jamp.exe 0x48b7c0 and 0x48b8b0), all in the 640x480 screen.
static void expected(Pivot pivot, float x, float y, float w, float h, float degrees, double out[4][2]) {
    const double corner[4][2] = {{-w, 0}, {0, 0}, {0, h}, {-w, h}};
    const double centre[4][2] = {{-0.5 * w, -0.5 * h}, {0.5 * w, -0.5 * h}, {0.5 * w, 0.5 * h}, {-0.5 * w, 0.5 * h}};
    const double ox = pivot == Pivot::Corner ? double(x) + w : x, a = degrees * 3.14159265358979323846 / 180.0;
    for (int i = 0; i < 4; ++i) {
        const double *l = pivot == Pivot::Corner ? corner[i] : centre[i];
        out[i][0] = ox + l[0] * std::cos(a) - l[1] * std::sin(a);
        out[i][1] = y + l[0] * std::sin(a) + l[1] * std::cos(a);
    }
}
int main() {
    refDrawQuad_t quad{}; Rect from;
    // Unturned, a corner-pivot picture covers its own rectangle.
    assert(RotatedQuad(Pivot::Corner, 10, 20, 30, 40, 0, 0, 1, 1, 0, 5, quad, from) == Status::Ok);
    assert(quad.xy[0][0] == 10 && quad.xy[0][1] == 20 && quad.xy[1][0] == 40 && quad.xy[1][1] == 20);
    assert(quad.xy[2][0] == 40 && quad.xy[2][1] == 60 && quad.xy[3][0] == 10 && quad.xy[3][1] == 60);
    assert(from.x == 10 && from.y == 20 && from.w == 30 && from.h == 40 && quad.shader == 5);
    assert(quad.st[1][0] == 1 && quad.st[1][1] == 0 && quad.st[3][0] == 0 && quad.st[3][1] == 1 && !quad.colorMask);
    // The rocket lock's eight wedges turn about (cx, cy - sz).
    for (int i = 0; i < 8; ++i) {
        double want[4][2]; expected(Pivot::Corner, 300, 220, 20, 20, i * 45.0f, want);
        assert(RotatedQuad(Pivot::Corner, 300, 220, 20, 20, 0, 0, 1, 1, i * 45.0f, 7, quad, from) == Status::Ok);
        for (int c = 0; c < 4; ++c) assert(near_(quad.xy[c][0], want[c][0]) && near_(quad.xy[c][1], want[c][1]));
        assert(near_(quad.xy[1][0], 320) && near_(quad.xy[1][1], 220)); // the pivot itself
    }
    // A quarter turn clockwise in the y-down screen: top-left goes top-right.
    assert(RotatedQuad(Pivot::Centre, 320, 240, 640, 480, 0, 0, 1, 1, 90, 9, quad, from) == Status::Ok);
    assert(near_(quad.xy[0][0], 560) && near_(quad.xy[0][1], -80) && near_(quad.xy[2][0], 80) && near_(quad.xy[2][1], 560));
    assert(from.x == 0 && from.y == 0 && from.w == 640 && from.h == 480);
    // Disruptor ticks: 12x24 at radius 190, centred where the module asks.
    for (float fi = 18.5f; fi <= 76.5f; fi += 3) {
        const float cx = 320 + std::sin((fi + 90.0f) / 57.296f) * 190, cy = 240 + std::cos((fi + 90.0f) / 57.296f) * 190;
        double want[4][2]; expected(Pivot::Centre, cx, cy, 12, 24, 90 - fi, want);
        assert(RotatedQuad(Pivot::Centre, cx, cy, 12, 24, 0, 0, 1, 1, 90 - fi, 3, quad, from) == Status::Ok);
        for (int c = 0; c < 4; ++c) assert(near_(quad.xy[c][0], want[c][0]) && near_(quad.xy[c][1], want[c][1]));
        assert(near_(from.x + 6, cx) && near_(from.y + 12, cy));
    }
    // No area draws nothing; a non-finite word is refused.
    quad.shader = 77;
    assert(RotatedQuad(Pivot::Corner, 1, 2, 0, 4, 0, 0, 1, 1, 30, 5, quad, from) == Status::Empty && quad.shader == 77);
    assert(RotatedQuad(Pivot::Centre, 1, 2, 3, 0, 0, 0, 1, 1, 30, 5, quad, from) == Status::Empty && quad.shader == 77);
    const float bad[] = {std::nanf(""), INFINITY, -INFINITY};
    for (float value : bad) for (int index = 0; index < 9; ++index) {
        float words[9] = {1, 2, 3, 4, 0, 0, 1, 1, 30}; words[index] = value;
        for (Pivot pivot : {Pivot::Corner, Pivot::Centre})
            assert(RotatedQuad(pivot, words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7],
                words[8], 5, quad, from) == Status::Invalid && quad.shader == 77);
    }
    assert(RotatedQuad(Pivot::Corner, 3.0e38f, 0, 3.0e38f, 1, 0, 0, 1, 1, 0, 5, quad, from) == Status::Invalid);
    // Placement: the turned corners take the unturned rectangle's scale and
    // offset, so a stretched insert turns in 640x480 and is stretched after,
    // as retail's screen was, while a uniform one simply turns.
    assert(RotatedQuad(Pivot::Centre, 320, 240, 640, 480, 0, 0, 1, 1, -103, 9, quad, from) == Status::Ok);
    refDrawQuad_t placed = quad;
    assert(PlaceQuad(placed, from, Rect{0, 0, 1680, 720}));
    for (int c = 0; c < 4; ++c)
        assert(near_(placed.xy[c][0], quad.xy[c][0] * 2.625) && near_(placed.xy[c][1], quad.xy[c][1] * 1.5));
    placed = quad;
    assert(PlaceQuad(placed, from, Rect{360, 0, 960, 720}));
    for (int c = 0; c < 4; ++c)
        assert(near_(placed.xy[c][0], 360 + quad.xy[c][0] * 1.5) && near_(placed.xy[c][1], quad.xy[c][1] * 1.5));
    const double side = std::hypot(placed.xy[1][0] - placed.xy[0][0], placed.xy[1][1] - placed.xy[0][1]);
    assert(near_(side, 960)); // uniform: still a 960-wide picture, turned
    placed = quad;
    assert(!PlaceQuad(placed, Rect{0, 0, 0, 480}, Rect{0, 0, 1680, 720}));
    assert(!PlaceQuad(placed, from, Rect{0, 0, INFINITY, 720}));
    assert(!PlaceQuad(placed, from, Rect{0, 0, 3.0e38f, 3.0e38f}) || std::isfinite(placed.xy[0][0]));
    return 0;
}
'''


def host_fixture():
    host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text(encoding='utf-8')
    helper = function(host, 'static void CL_JKACgameRotatedPic(')
    start = host.index('    case Call::CG_R_DRAWROTATEPIC:\n')
    cases = host[start:host.index('    case Call::CG_OPENUIMENU: {', start)]
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/compat/jka_module_abi.h"
#include "code/compat/jka_render_adapter.hpp"
#include "code/compat/jka_draw_adapter.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
static bool hidden;
static bool CL_CaptureHidesHud() { return hidden; }
static std::vector<int> order;
static void CL_HudFlushQueue() { order.push_back(1); }
// The 21:9 screen's 4:3 frame (960x720 at 1680x720); the stand-in rules
// stretch whatever the module draws over that frame.
static void CL_HudFromVirtual(float *x, float *y, float *w, float *h) { *x *= 1.5f; *y *= 1.5f; *w *= 1.5f; *h *= 1.5f; }
static bool refuse;
static qhandle_t adjustedShader;
static qboolean CL_HudAdjustPic(float *x, float *, float *w, float *, qhandle_t shader) {
    adjustedShader = shader; *x *= 1.75f; *w *= 1.75f; return refuse ? qfalse : qtrue;
}
static std::vector<refDrawQuad_t> quads;
static std::vector<float> ranges;
static void DrawQuad(const refDrawQuad_t *quad) { order.push_back(2); quads.push_back(*quad); }
static void SetRangedFog(float range) { ranges.push_back(range); }
static struct { void (*DrawQuad)(const refDrawQuad_t *); void (*SetJKARangedFog)(float); } re{DrawQuad, SetRangedFog};
''' + helper + r'''
static bool dispatch(fntech3::jka::module::Call call, intptr_t *args) {
    using namespace fntech3::jka;
    using module::Call;
    switch (call) {
''' + cases + r'''
    default: assert(false);
    }
    return false;
}
static intptr_t word(float value) { std::uint32_t bits; std::memcpy(&bits, &value, 4); return static_cast<intptr_t>(bits); }
template<class F> static void rejected(F callback) { bool dropped = false; try { callback(); } catch (Drop) { dropped = true; } assert(dropped); }
static bool near_(double a, double b) { return std::fabs(a - b) < 0.01; }
int main() {
    using fntech3::jka::module::Call;
    // The disruptor's insert: full screen, turned -103 degrees about its centre.
    intptr_t insert[]{215, word(320), word(240), word(640), word(480), word(0), word(0), word(1), word(1), word(-103), 41};
    assert(dispatch(Call::CG_R_DRAWROTATEPIC2, insert) && quads.size() == 1 && adjustedShader == 41);
    assert(order.size() == 2 && order[0] == 1 && order[1] == 2); // held text is drawn first
    const double a = -103 * 3.14159265358979323846 / 180;
    const double local[4][2]{{-320, -240}, {320, -240}, {320, 240}, {-320, 240}};
    for (int c = 0; c < 4; ++c) {
        const double vx = 320 + local[c][0] * std::cos(a) - local[c][1] * std::sin(a);
        const double vy = 240 + local[c][0] * std::sin(a) + local[c][1] * std::cos(a);
        assert(near_(quads[0].xy[c][0], vx * 2.625) && near_(quads[0].xy[c][1], vy * 1.5));
    }
    assert(quads[0].shader == 41 && quads[0].st[2][0] == 1 && quads[0].st[2][1] == 1);
    // A rocket lock wedge turns about its top-right corner.
    intptr_t wedge[]{214, word(300), word(220), word(20), word(20), word(0), word(0), word(1), word(1), word(90), 42};
    assert(dispatch(Call::CG_R_DRAWROTATEPIC, wedge) && quads.size() == 2);
    assert(near_(quads[1].xy[1][0], 320 * 2.625) && near_(quads[1].xy[1][1], 220 * 1.5));
    assert(near_(quads[1].xy[0][0], 320 * 2.625) && near_(quads[1].xy[0][1], 200 * 1.5)); // top-left turned up
    // Nothing to draw: no area, a capture that hides the HUD, a refused rule.
    wedge[3] = word(0); assert(dispatch(Call::CG_R_DRAWROTATEPIC, wedge)); wedge[3] = word(20);
    hidden = true; assert(dispatch(Call::CG_R_DRAWROTATEPIC, wedge)); hidden = false;
    refuse = true; assert(dispatch(Call::CG_R_DRAWROTATEPIC2, insert)); refuse = false;
    assert(quads.size() == 2);
    for (int index = 1; index <= 9; ++index) {
        intptr_t bad[11]; std::memcpy(bad, insert, sizeof(bad)); bad[index] = static_cast<intptr_t>(0x7fc00000u);
        rejected([&] { dispatch(Call::CG_R_DRAWROTATEPIC2, bad); });
        bad[0] = 214; rejected([&] { dispatch(Call::CG_R_DRAWROTATEPIC, bad); });
    }
    assert(quads.size() == 2);
    // 216 hands the word to Fn's renderer; fov_x * 64, then 0, then an override.
    for (float range : {5120.0f, 0.0f, -300.0f}) {
        intptr_t fog[]{216, word(range)};
        assert(dispatch(Call::CG_R_SETRANGEFOG, fog) && ranges.back() == range);
    }
    for (std::uint32_t bits : {0x7fc00000u, 0x7f800000u, 0xff800000u}) {
        intptr_t fog[]{216, static_cast<intptr_t>(bits)};
        rejected([&] { dispatch(Call::CG_R_SETRANGEFOG, fog); });
    }
    assert(ranges.size() == 3);
    re.SetJKARangedFog = nullptr; { intptr_t fog[]{216, word(640)}; rejected([&] { dispatch(Call::CG_R_SETRANGEFOG, fog); }); }
    re.DrawQuad = nullptr; rejected([&] { dispatch(Call::CG_R_DRAWROTATEPIC, wedge); });
    return 0;
}
'''


SHADE_PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cmath>
#include <cstring>
struct fogParms_t { vec3_t color; float depthForOpaque; };
struct fog_t { int originalBrushNumber; vec4_t color; float tcScale; fogParms_t parms; qboolean hasSurface; float surface[4]; };
struct fogProgramParms_t { float eyeT; qboolean eyeOutside; vec4_t fogDistanceVector; vec4_t fogDepthVector; const float *fogColor; };
struct world_t { fog_t *fogs; int numfogs; float jkaRangedFog; };
struct orientationr_t { vec3_t origin; vec3_t axis[3]; vec3_t viewOrigin; float modelMatrix[16]; };
static world_t s_worldData;
static struct { world_t *world; } tr;
static struct { int gameId; } ri;
static struct { orientationr_t or; struct { orientationr_t or; } viewParms; } backEnd;
static struct { int fogNum; } tess;
static float cull = 6000.0f;
static bool cullReady = true;
static qboolean RE_GetJKADistanceCull(float *distance) { if (!cullReady) return qfalse; *distance = cull; return qtrue; }
#include "code/renderercommon/tr_jka_ranged_fog_shade.h"
'''

SHADE_CHECKS = r'''
// The fog pass's fraction at one view depth: R_FogFactor on s (t inside).
static float Fraction(float depth) {
    backEnd.or.modelMatrix[2] = backEnd.or.modelMatrix[6] = backEnd.or.modelMatrix[10] = 0;
    backEnd.or.modelMatrix[10] = -1; // view forward is -z in the model matrix
    const fogProgramParms_t *parm = RB_CalcFogProgramParms();
    float s = depth * parm->fogDistanceVector[2] + parm->fogDistanceVector[3];
    s -= 1.0f / 512;
    if (s < 0) return 0;
    s *= 8;
    return s > 1 ? 1 : s;
}
int main() {
    fog_t fogs[3]{};
    fogs[1].originalBrushNumber = 4; fogs[1].parms.depthForOpaque = 200; fogs[1].tcScale = 1.0f / (200 * 8);
    fogs[2].originalBrushNumber = -1; fogs[2].parms.depthForOpaque = 1500; fogs[2].tcScale = 1.0f / (1500 * 8);
    s_worldData.fogs = fogs; s_worldData.numfogs = 3; tr.world = &s_worldData; ri.gameId = COM_GAME_JKA_MP;
    tess.fogNum = 2;
    // No range: the volume's own linear fog, opaque at depthForOpaque.
    assert(std::fabs(Fraction(750) - 0.5f) < 0.001f && Fraction(1600) == 1);
    RE_SetJKARangedFog(5120);
    assert(s_worldData.jkaRangedFog == 5120);
    // Binoculars at fov 80: 880 to 6000.
    assert(Fraction(800) == 0 && std::fabs(Fraction(3440) - 0.5f) < 0.001f && Fraction(5990) < 1 && Fraction(6001) == 1);
    RE_SetJKARangedFog(-300);
    assert(Fraction(290) == 0 && std::fabs(Fraction(3150) - 0.5f) < 0.001f);
    // A volume with a brush of its own keeps its fog.
    tess.fogNum = 1; assert(std::fabs(Fraction(100) - 0.5f) < 0.001f); tess.fogNum = 2;
    // Without a completed world limit, or outside Academy, nothing changes.
    cullReady = false; assert(std::fabs(Fraction(750) - 0.5f) < 0.001f); cullReady = true;
    ri.gameId = COM_GAME_JKA_SP; assert(std::fabs(Fraction(750) - 0.5f) < 0.001f);
    RE_SetJKARangedFog(640); assert(s_worldData.jkaRangedFog == -300); ri.gameId = COM_GAME_JKA_MP;
    // Non-finite words and a world not loaded leave the value.
    RE_SetJKARangedFog(NAN); RE_SetJKARangedFog(INFINITY); assert(s_worldData.jkaRangedFog == -300);
    tr.world = nullptr; RE_SetJKARangedFog(640); tr.world = &s_worldData; assert(s_worldData.jkaRangedFog == -300);
    world_t other{}; tr.world = &other; RE_SetJKARangedFog(640); assert(other.jkaRangedFog == 0); tr.world = &s_worldData;
    RE_SetJKARangedFog(0);
    assert(std::fabs(Fraction(750) - 0.5f) < 0.001f);
    return 0;
}
'''


class JKAZoomTests(unittest.TestCase):
    def compile_and_run(self, source, suffix, flags, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C/C++ compiler required')
        parent = ROOT / '.tmp/tests/jka-zoom'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temp:
            folder = Path(temp)
            probe, executable = folder / (name + suffix), folder / (name + '.exe')
            probe.write_text(source, encoding='utf-8')
            language = ['-x', 'c', '-std=c11'] if suffix == '.c' else ['-std=c++17', '-fno-operator-names']
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            build = subprocess.run([compiler, *target, *language,
                '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags, '-I', str(ROOT),
                str(probe), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_ranged_fog_policy_in_c_and_cpp(self):
        for suffix in ('.c', '.cpp'):
            for flags in ([], ['-O2']):
                with self.subTest(suffix=suffix, flags=flags):
                    self.compile_and_run(FOG_POLICY, suffix, flags, 'fog-policy')

    def test_turned_picture_geometry(self):
        for flags in ([], ['-O2']):
            with self.subTest(flags=flags):
                self.compile_and_run(ADAPTER, '.cpp', flags, 'adapter')

    def test_production_cgame_cases(self):
        self.compile_and_run(host_fixture(), '.cpp', ['-O2', '-Wno-unused-parameter'], 'host')

    def test_production_fog_functions_in_all_renderers(self):
        for renderer in RENDERERS:
            calc = (ROOT / f'code/{renderer}/tr_shade_calc.c').read_text(encoding='utf-8')
            bsp = (ROOT / f'code/{renderer}/tr_bsp.c').read_text(encoding='utf-8')
            self.assertIn('#include "../renderercommon/tr_jka_ranged_fog_shade.h"', calc)
            self.assertEqual(calc.count('RB_JKARangedFog( fog, &fogScale, &fogStart );'), 2, renderer)
            self.assertIn('re.SetJKARangedFog = RE_SetJKARangedFog;',
                          (ROOT / f'code/{renderer}/tr_init.c').read_text(encoding='utf-8'))
            # The world's load clears it: the field lives in the memset world.
            local = (ROOT / f'code/{renderer}/tr_local.h').read_text(encoding='utf-8')
            world = local[:local.index('} world_t;')]
            self.assertIn('float\t\tjkaRangedFog;', world[world.rindex('typedef struct {'):])
            source = '\n'.join((SHADE_PRELUDE, function(bsp, 'void RE_SetJKARangedFog('),
                function(calc, 'const fogProgramParms_t *RB_CalcFogProgramParms( void )').replace(
                    'Com_Memset', 'std::memset'), SHADE_CHECKS))
            with self.subTest(renderer=renderer):
                self.compile_and_run(source, '.cpp', ['-Wno-unused-parameter', '-Wno-missing-field-initializers'],
                                     'shade-' + renderer)

    def test_refused_imports_are_now_served_by_name(self):
        host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text(encoding='utf-8')
        for name in ('CG_R_DRAWROTATEPIC', 'CG_R_DRAWROTATEPIC2', 'CG_R_SETRANGEFOG'):
            self.assertIn(f'    case Call::{name}:', host)
        shared = (ROOT / 'code/client/client_jka_cgame.hpp').read_text(encoding='utf-8')
        self.assertNotIn('ROTATEPIC', shared)
        self.assertNotIn('RANGEFOG', shared)


if __name__ == '__main__':
    unittest.main()
