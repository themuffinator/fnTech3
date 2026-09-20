"""Elite Force SP clip rectangles: pixel bounds and queued drawing order."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include <cassert>
#include <climits>
#include <cfloat>
#include <cstring>
#include <stdexcept>
#include "code/renderercommon/tr_stvef_scissor.h"

constexpr int qtrue = 1, ERR_DROP = 1, RC_STVEF_SCISSOR = 123, DEPTH_RANGE_COUNT = 9;
static struct { int registered; } tr;
static struct { int vidWidth = 640, vidHeight = 480; } glConfig;
static struct { int numIndexes; } tess;
static struct { int projection2D, scissor2DSet; fnScissorRect_t scissor2D; } backEnd;
static struct { int depth_range; } vkCommand;
static struct { decltype(vkCommand) *cmd = &vkCommand; } vk;
static int flushes, setups, allocations;
static fnScissorRect_t flushedRect, glRect;
static stvefScissorCommand_t queued;
static bool full;
static void Error( int, const char * ) { throw std::runtime_error("invalid"); }
static struct { void (*Error)(int, const char *) = ::Error; } ri;
static void *R_GetCommandBuffer( size_t n ) {
    assert(n == sizeof(queued)); ++allocations;
    return full ? nullptr : &queued;
}
static void RB_EndSurface() { ++flushes; flushedRect = backEnd.scissor2D; tess.numIndexes = 0; }
static void RB_SetGL2D() { ++setups; backEnd.projection2D = 1; backEnd.scissor2DSet = 0; }
static void qglScissor( int x, int y, int w, int h ) { glRect = {x,y,w,h}; }
#include "code/renderercommon/tr_stvef_scissor_cmd.h"
#include "code/renderercommon/tr_stvef_scissor_backend.h"

static void Rect( float x, float y, float w, float h, int left, int top, int width, int height ) {
    fnScissorRect_t r;
    assert(FN_STVEFScissorRect(x,y,w,h,640,480,&r));
    assert(r.x == left && r.y == top && r.width == width && r.height == height);
}
int main() {
    Rect(12,23,40,50,12,23,40,50);
    // Round after the lower-left conversion; truncating y first is wrong.
    Rect(12.9f,23.4f,40.8f,50.2f,12,24,40,50);
    Rect(-1,0,0,0,0,0,640,480);
    Rect(10,-20,40,50,10,0,40,30);
    Rect(620,460,80,90,620,460,20,20);
    Rect(700,490,30,40,640,480,0,0);
    Rect(0,0,0,0,0,0,0,0);
    Rect(0,0,FLT_MAX,480,0,0,640,480);
    Rect(FLT_MAX,0,1,1,640,0,0,1);
    fnScissorRect_t sentinel = {11,22,33,44}, r = sentinel;
    assert(!FN_STVEFScissorRect(0,0,-1,2,640,480,&r));
    assert(!FN_STVEFScissorRect(0,0,1,-2,640,480,&r));
    assert(!FN_STVEFScissorRect(NAN,0,1,2,640,480,&r));
    assert(!FN_STVEFScissorRect(0,INFINITY,1,2,640,480,&r));
    assert(!FN_STVEFScissorRect(0,0,1,2,-1,480,&r));
    assert(!std::memcmp(&r,&sentinel,sizeof(r)));
    assert(!FN_STVEFScissorRect(0,0,1,2,640,480,nullptr));

    // Prior vertices are submitted with their own state, including when the
    // next clip command crosses from a 3D view into a 2D projection.
    RE_SetSTVEFScissor(10,20,30,40);
    assert(allocations == 0);
    tr.registered = 1;
    RE_SetSTVEFScissor(10,20,30,40);
    assert(queued.commandId == RC_STVEF_SCISSOR);
    backEnd.scissor2D = sentinel;
    tess.numIndexes = 6;
    assert(RB_SetSTVEFScissor(&queued) == &queued + 1);
    assert(flushes == 1 && setups == 1 && backEnd.scissor2DSet);
    assert(!std::memcmp(&flushedRect,&sentinel,sizeof(sentinel)));
    assert(backEnd.scissor2D.x == 10 && backEnd.scissor2D.y == 20);
#ifdef USE_VULKAN
    assert(vk.cmd->depth_range == DEPTH_RANGE_COUNT);
#else
    assert(glRect.x == 10 && glRect.y == 420 && glRect.width == 30 && glRect.height == 40);
#endif
    RE_SetSTVEFScissor(-1,0,0,0);
    RB_SetSTVEFScissor(&queued);
    assert(flushes == 1 && setups == 1);
    assert(backEnd.scissor2D.width == 640 && backEnd.scissor2D.height == 480);
    full = true;
    RE_SetSTVEFScissor(1,2,3,4);
    assert(queued.rect.width == 640); // A full queue publishes no partial state.
    int before = allocations;
    try { RE_SetSTVEFScissor(NAN,0,0,0); assert(false); } catch(const std::runtime_error&) {}
    assert(allocations == before);
}
'''


class STVEFScissorTests(unittest.TestCase):
    def test_pixel_bounds_and_command_order(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-scissor-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.cpp'
            source.write_text(PROBE, encoding='utf-8')
            for backend in ('gl', 'vk'):
                with self.subTest(backend=backend):
                    binary = Path(folder) / (backend + ('.exe' if os.name == 'nt' else ''))
                    defines = ['-DUSE_VULKAN'] if backend == 'vk' else []
                    built = subprocess.run([compiler, *compiler_target_args(compiler), *defines,
                                            '-std=c++17', '-I', str(ROOT), str(source), '-o', str(binary)],
                                           capture_output=True, text=True, cwd=ROOT, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    ran = subprocess.run([str(binary)], capture_output=True, text=True,
                                         cwd=ROOT, timeout=30, **options)
                    self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_every_renderer_carries_the_command(self):
        def read(path):
            return (ROOT / path).read_text(encoding='utf-8')
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                prefix = f'code/{renderer}/'
                self.assertIn('re.SetSTVEFScissor = RE_SetSTVEFScissor;', read(prefix + 'tr_init.c'))
                self.assertIn('tr_stvef_scissor_cmd.h', read(prefix + 'tr_cmds.c'))
                backend = read(prefix + 'tr_backend.c')
                self.assertIn('tr_stvef_scissor_backend.h', backend)
                self.assertIn('data = RB_SetSTVEFScissor( data );', backend)
                self.assertIn('void RB_SetGL2D( void ) {\n\tbackEnd.projection2D = qtrue;\n'
                              '\tbackEnd.scissor2DSet = qfalse;', backend)
                if renderer != 'renderer':
                    self.assertIn('if ( backEnd.projection2D && backEnd.scissor2DSet )', read(prefix + 'vk.c'))
        self.assertIn('re.SetSTVEFScissor( VMF(1), VMF(2), VMF(3), VMF(4) );',
                      read('code/client/client_stvef_sp_cgame.hpp'))


if __name__ == '__main__':
    unittest.main()
