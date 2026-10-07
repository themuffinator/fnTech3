"""Academy's map-change export reaches its client game despite a shared number.

The engine numbers its semantic game and client-game exports from 0x10000 in
two separate lists, so Enemy Territory's game snapshot callback and Jedi
Academy's client-game map change are the same number. A guard on the number
alone refused Academy's map change as an ET snapshot callback whenever a
retail server changed map under fnTech3's client. Production VM_Call is
compiled against the RTCW export fixture's synthetic module.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_module_export_tests import PRELUDE

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#define ERROR(expression) do { \
    const int before = errors; \
    if (!setjmp(errorJump)) { expression; assert(!"expected VM error"); } \
    assert(errors == before + 1); \
} while (0)
static void Reset(comGameId_t id, vmIndex_t index) {
    memset(&profile, 0, sizeof(profile)); profile.id = id; profile.key = "test";
    memset(&module, 0, sizeof(module)); module.name = "test"; module.index = index;
    module.entryPoint = Native; module.syscallCount = 73;
    nativeReturn = -1234567; nativeCalls = structuredCalls = compiledCalls = interpretedCalls = 0;
    observedCall = -1;
}
int main(void) {
    int id;
#ifndef USE_DEDICATED
    _Static_assert((int)GAME_SNAPSHOT_CALLBACK == (int)CG_RAVEN_MAP_CHANGE,
        "the case this test exists for: two semantic exports share a number");
    /* Academy's client game takes the map change as its export 27, with no
     * arguments (retail cgamex86.dll 0x3004277d). */
    Reset(COM_GAME_JKA_MP, VM_CGAME);
    assert(VM_Call(&module, 0, CG_RAVEN_MAP_CHANGE) == nativeReturn);
    assert(nativeCalls == 1 && observedCall == FNTECH3_JKA_CG_MAP_CHANGE && observedCall == 27);
    assert(module.callLevel == 0 && errors == 0);
    /* Every other profile's client game refuses it before its module runs. */
    for (id = 0; id < COM_GAME_COUNT; ++id) {
        if (id == COM_GAME_JKA_MP) continue;
        Reset((comGameId_t)id, VM_CGAME);
        ERROR(VM_Call(&module, 0, CG_RAVEN_MAP_CHANGE));
        assert(nativeCalls == 0 && module.callLevel == 0);
    }
#endif
    /* The game's snapshot callback is still ET's alone. */
    for (id = 0; id < COM_GAME_COUNT; ++id) {
        Reset((comGameId_t)id, VM_GAME);
        if (id == COM_GAME_WOLFET) {
            assert(VM_Call(&module, 2, GAME_SNAPSHOT_CALLBACK, 5, 1) == nativeReturn);
            assert(nativeCalls == 1 && observedCall == FNTECH3_WOLFET_GAME_SNAPSHOT_CALLBACK);
            assert(observedArgs[0] == 5 && observedArgs[1] == 1);
        } else {
            ERROR(VM_Call(&module, 2, GAME_SNAPSHOT_CALLBACK, 5, 1));
            assert(nativeCalls == 0);
        }
        assert(module.callLevel == 0);
    }
    return 0;
}
'''


class JKAMapChangeExportTests(unittest.TestCase):
    def test_shared_semantic_number_reaches_only_its_own_module(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        text = (ROOT / 'code/qcommon/vm.c').read_text(encoding='utf-8')
        start = text.index('#ifndef USE_DEDICATED\nstatic int VM_RTCWMenu(')
        end = text.index('\n}\n', text.index('intptr_t QDECL VM_Call(', start)) + 3

        def function(source, signature):
            position = source.index(signature)
            return source[position:source.index('\n}', position) + 2]
        ui = (ROOT / 'code/client/cl_ui.cpp').read_text(encoding='utf-8')
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text(encoding='utf-8')
        popup = '\n#ifndef USE_DEDICATED\n' + function(shared, 'int Q_stricmp(')
        popup += '\n' + function(ui, 'void CL_RTCWPopup(') + '\n#endif\n'
        parent = ROOT / '.tmp/tests/jka-map-change-export'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary)
            fixture = folder / 'probe.c'
            fixture.write_text(PRELUDE + text[start:end] + popup + CHECKS, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            for dedicated in (False, True):
                with self.subTest(dedicated=dedicated):
                    # One executable per build: Windows can hold the last one run.
                    name = 'probe-dedicated' if dedicated else 'probe'
                    executable = folder / (name + '.exe' if os.name == 'nt' else name)
                    flags = ['-DUSE_DEDICATED'] if dedicated else []
                    build = subprocess.run([compiler, *target, *flags, '-std=c11', '-I', str(ROOT),
                                            '-I', str(ROOT / 'code/qcommon'), str(fixture), '-o', str(executable)],
                                           capture_output=True, text=True, timeout=60, cwd=ROOT)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
