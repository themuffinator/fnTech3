"""Exercise actual 2D command producers and batching in all three renderers."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/rtcw_draw_adapter.hpp"
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
#include <memory>
struct shader_t { int index; } shaderA{17}, shaderB{19};
'''

HARNESS = r'''
static struct { qboolean registered; } tr;
static struct {
    unsigned before;
    int numVertexes, numIndexes, surfType;
    shader_t *shader;
    float xyz[8][4];
    vec2_t texCoords[2][8];
    color4ub_t vertexColors[8];
    int indexes[12];
    unsigned after;
} tess;
static struct { qboolean projection2D; int entity2D; int *currentEntity; color4ub_t color2D; } backEnd;
static int allocations, allocationSize, lookups, flushes, begins, setups, vboFlushes, vboUnbinds, blooms, uiNotes, frameNotes;
static bool failAllocation, failOverflow;
alignas(drawQuadCommand_t) static unsigned char commandBytes[sizeof(drawQuadCommand_t)];
static stretchPicCommand_t *R_GetCommandBuffer(int bytes) {
    ++allocations; allocationSize = bytes; assert(bytes <= static_cast<int>(sizeof(commandBytes)));
    if (failAllocation) return nullptr;
    std::memset(commandBytes, 0xe5, sizeof(commandBytes)); return reinterpret_cast<stretchPicCommand_t *>(commandBytes);
}
static shader_t *R_GetShaderByHandle(int handle) {
    ++lookups; assert(handle == 17 || handle == 19); return handle == 17 ? &shaderA : &shaderB;
}
static void QDECL Error(errorParm_t, const char *, ...) { throw 7; }
static struct { decltype(&::Error) Error; } ri{Error};
static void RB_EndSurface() { ++flushes; tess.numVertexes = tess.numIndexes = 0; }
static void RB_BeginSurface(shader_t *shader, int fog) { ++begins; assert(fog == 0); tess.shader = shader; }
static void RB_SetGL2D() { ++setups; backEnd.projection2D = qtrue; }
static void Overflow(int vertices, int indexes) {
    assert(vertices == 4 && indexes == 6); if (failOverflow) throw 8;
    if (tess.numVertexes + vertices > 8 || tess.numIndexes + indexes > 12) RB_EndSurface();
}
#define RB_CHECKOVERFLOW Overflow
enum { SF_TRIANGLES = 42, GLX_DYNAMIC_CATEGORY_MASK_UI = 8, RTX_FRAME_PASS_UI_2D = 12 };
static void VBO_Flush() { ++vboFlushes; }
static void VBO_UnBind() { ++vboUnbinds; }
static void R_BloomScreen() { ++blooms; }
static void vk_bloom() { ++blooms; }
static struct { int integer; } bloomValue{1}, *r_bloom = &bloomValue;
static void GLX_CompatMarkDynamicCategory(int mask) { assert(mask == 8); ++uiNotes; }
static void RB_FrameGraph_NotePass(int pass) { assert(pass == 12); ++frameNotes; }
#include "code/renderercommon/tr_draw_quad_cmd.h"
'''

CHECKS = r'''
#include "code/renderercommon/tr_draw_quad_backend.h"
static void Reset() {
    std::memset(&tess, 0, sizeof(tess)); tess.before = 0x12345678; tess.after = 0xaabbccdd;
    backEnd = {}; backEnd.color2D.u32 = 0x98765432;
    tr.registered = qtrue; failAllocation = failOverflow = false;
    allocations = lookups = flushes = begins = setups = vboFlushes = vboUnbinds = blooms = uiNotes = frameNotes = 0;
}
static void CheckQuad(const refDrawQuad_t &quad, int base) {
    assert(tess.before == 0x12345678 && tess.after == 0xaabbccdd);
    const int pattern[] = {3, 0, 2, 2, 0, 1};
    for (int i = 0; i < 6; ++i) assert(tess.indexes[base / 4 * 6 + i] == base + pattern[i]);
    for (int i = 0; i < 4; ++i) {
        assert(tess.xyz[base + i][0] == quad.xy[i][0] && tess.xyz[base + i][1] == quad.xy[i][1]);
        assert(tess.xyz[base + i][2] == 0);
        assert(!std::memcmp(tess.texCoords[0][base + i], quad.st[i], sizeof(vec2_t)));
        assert(tess.vertexColors[base + i].u32 == ((quad.colorMask & (1u << i)) ? quad.colors[i].u32 : backEnd.color2D.u32));
    }
}
template<class F> static void Reject(F action) {
    const int allocated = allocations, looked = lookups;
    bool caught = false; try { action(); } catch (int) { caught = true; }
    assert(caught && allocations == allocated && lookups == looked);
}
int main() {
    using namespace fntech3::rtcw::render;
    refDrawQuad_t gradient;
    const vec4_t color = {0, 0.25f, 0.5f, 1};
    assert(GradientQuad(10, 20, -30, 40, -0.25f, 0.5f, 1.5f, -0.75f, 17, color, INT_MIN, gradient));
    const auto savedGradient = gradient;
    Reset(); RE_DrawQuad(&gradient);
    assert(allocations == 1 && allocationSize == sizeof(drawQuadCommand_t) && lookups == 1);
    auto command = *reinterpret_cast<const drawQuadCommand_t *>(commandBytes);
    assert(command.base.commandId == RC_DRAW_QUAD && command.base.shader == &shaderA);
    assert(!std::memcmp(&command.quad, &savedGradient, sizeof(gradient)));
    gradient = {};  // The producer must own its command's full data.
    backEnd.color2D.u32 = 0x12345678;  // Current color is sampled at execution.
    assert(RB_DrawQuad(&command) == &command + 1);
    assert(tess.numVertexes == 4 && tess.numIndexes == 6); CheckQuad(savedGradient, 0);
    assert(begins == 1 && setups == 1 && blooms == 1 && vboUnbinds == 1 && vboFlushes == 1);
    assert(backEnd.currentEntity == &backEnd.entity2D && tess.shader == &shaderA);
    assert(uiNotes == EXPECT_UI_NOTES && frameNotes == EXPECT_FRAME_NOTES);
    RB_DrawQuad(&command); assert(tess.numVertexes == 8 && tess.numIndexes == 12); CheckQuad(savedGradient, 4);
    assert(flushes == 0 && begins == 1 && setups == 1);
    RB_DrawQuad(&command); assert(flushes == 1 && tess.numVertexes == 4); CheckQuad(savedGradient, 0);
    command.base.shader = &shaderB;
    RB_DrawQuad(&command); assert(flushes == 2 && begins == 2 && tess.shader == &shaderB); CheckQuad(savedGradient, 0);
    failOverflow = true;
    const auto beforeOverflow = tess;
    Reject([&] { RB_DrawQuad(&command); });
    assert(!std::memcmp(&tess, &beforeOverflow, sizeof(tess))); failOverflow = false;
    // Every explicit/current-color combination is supported by the canonical command.
    for (unsigned mask = 0; mask < 16; ++mask) {
        Reset(); command.quad = savedGradient; command.quad.colorMask = mask;
        for (int i = 0; i < 4; ++i) command.quad.colors[i].u32 = i + 100;
        RB_DrawQuad(&command); CheckQuad(command.quad, 0);
    }
    Reset(); RE_DrawQuad(nullptr); assert(!allocations);
    tr.registered = qfalse; RE_DrawQuad(&savedGradient); assert(!allocations);
    tr.registered = qtrue; failAllocation = true; RE_DrawQuad(&savedGradient);
    assert(allocations == 1 && !lookups); failAllocation = false;
    auto bad = savedGradient; bad.colorMask = 16; Reject([&] { RE_DrawQuad(&bad); });
    for (int field = 0; field < 16; ++field) {
        bad = savedGradient;
        float &value = field < 8 ? bad.xy[field / 2][field % 2] : bad.st[(field - 8) / 2][field % 2];
        value = field % 2 ? NAN : INFINITY;
        Reject([&] { RE_DrawQuad(&bad); });
    }
    // Legacy pictures retain their original command allocation and batch geometry.
    Reset(); RE_StretchPic(1, 2, 3, 4, 0.1f, 0.2f, 0.8f, 0.9f, 17);
    assert(allocationSize == sizeof(stretchPicCommand_t) && sizeof(stretchPicCommand_t) < sizeof(drawQuadCommand_t));
    const auto legacy = *reinterpret_cast<const stretchPicCommand_t *>(commandBytes);
    assert(legacy.commandId == RC_STRETCH_PIC && RB_StretchPic(&legacy) == &legacy + 1);
    CheckQuad(Rectangle(1, 2, 3, 4, 0.1f, 0.2f, 0.8f, 0.9f, 17), 0);
    // Adapter failures publish no partial geometry or colors.
    refDrawQuad_t output = savedGradient;
    for (float value : {NAN, INFINITY, -INFINITY, std::numeric_limits<float>::max()}) {
        assert(!RotatedQuad(10, 20, 80, 20, 0, 0, 1, 1, 17, value, output));
        assert(!std::memcmp(&output, &savedGradient, sizeof(output)));
    }
    const vec4_t clamped = {-1, 0.5f, 2, 1};
    assert(GradientQuad(0, 0, 100, 20, 0, 0, 1, 1, 17, clamped, INT_MAX, output));
    const byte expected[] = {0, 127, 255, 255}; assert(!std::memcmp(output.colors[2].rgba, expected, 4));
    assert(GradientQuad(0, 0, 100, 20, 0, 0, 1, 1, 17, nullptr, 0, output));
    assert(output.colors[2].u32 == UINT32_MAX && output.colors[3].u32 == UINT32_MAX);
    for (float turns : {0.0f, 0.125f, 0.25f, -0.125f, 1.25f}) {
        assert(RotatedQuad(10, 20, 80, 20, 0, 0, 1, 1, 17, turns, output));
        assert(output.colorMask == 0);
        for (int i = 0; i < 4; ++i) {
            const float dx = output.xy[i][0] - 50, dy = output.xy[i][1] - 30;
            assert(std::fabs(dx * dx + dy * dy - 1700) < 0.001f);
        }
        Reset(); RE_DrawQuad(&output);
        command = *reinterpret_cast<const drawQuadCommand_t *>(commandBytes);
        RB_DrawQuad(&command); CheckQuad(output, 0);
    }
}
'''


class RendererDrawQuadTests(unittest.TestCase):
    def test_commands_geometry_colors_batching_and_legacy_pictures(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='render-quads-', dir=ROOT / '.tmp') as temp:
                folder = ROOT / 'code' / renderer
                local = (folder / 'tr_local.h').read_text(encoding='utf-8')
                declarations = []
                for name in ('stretchPicCommand_t', 'drawQuadCommand_t'):
                    match = re.search(r'typedef struct\s*\{[^{}]*\}\s*' + name + ';', local)
                    self.assertIsNotNone(match)
                    declarations.append(match[0])
                declarations.append(re.search(r'typedef enum\s*\{[^{}]*\}\s*renderCommand_t;', local)[0])
                cmds = (folder / 'tr_cmds.c').read_text(encoding='utf-8')
                backend = (folder / 'tr_backend.c').read_text(encoding='utf-8')
                surface = (folder / 'tr_surface.c').read_text(encoding='utf-8')
                self.assertIn('#include "../renderercommon/tr_draw_quad_cmd.h"', cmds)
                self.assertIn('#include "../renderercommon/tr_draw_quad_backend.h"', backend)
                self.assertIn('case RC_DRAW_QUAD:\n\t\t\tdata = RB_DrawQuad( data );', backend)
                self.assertIn('re.DrawQuad = RE_DrawQuad;', (folder / 'tr_init.c').read_text(encoding='utf-8'))
                config = ('#define USE_VBO 1\n#define USE_FBO 1\n#define EXPECT_UI_NOTES 1\n#define EXPECT_FRAME_NOTES 0\n'
                          if renderer == 'renderer' else
                          '#define USE_VBO 1\n#define USE_VULKAN 1\n#define EXPECT_UI_NOTES 0\n#define EXPECT_FRAME_NOTES ' + ('1' if renderer == 'rendererrtx' else '0') + '\n')
                text = config + PRELUDE + '\n'.join(declarations) + HARNESS
                text += function(cmds, 'void RE_StretchPic(')
                text += function(surface, 'void RB_AddQuadStamp2(')
                text += function(backend, 'static const void *RB_StretchPic(') + CHECKS
                probe = Path(temp) / 'probe.cpp'
                executable = Path(temp) / ('probe.exe' if os.name == 'nt' else 'probe')
                probe.write_text(text, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(probe),
                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=15, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
