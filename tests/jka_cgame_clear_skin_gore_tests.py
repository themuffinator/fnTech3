"""Serve Jedi Academy client-game import 315, CG_G2_CLEARSKINGORE.

Retail jamp.exe 1.01 routes import 315 through 0x413cd5 to 0x4e07f0, which
reads the instance through the module's identity, deletes each model's gore
set and answers 0. Here no gore is added, so the clear validates the identity
and leaves models and skins as they were; jka_cgame_skin_gore_tests.py clears
real gore. The fixture compiles the production header (optionally mutated)
beside the cgame Ghoul2 fixture.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_cgame_ghoul2_tests import fixture as ghoul2_fixture

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'code/client/client_jka_cgame_ghoul2.hpp'
CASE = 'case Call::CG_G2_CLEARSKINGORE: {'


def inline_header(text):
    # Inline the header so a mutated copy compiles in its place; its own
    # relative includes then resolve from the repository root.
    text = text.replace('#include "../', '#include "code/')
    return text.replace('#include "client_', '#include "code/client/client_')


def clear_case(text):
    start = text.index(CASE)
    return start, text.index('\n    }\n', start) + len('\n    }\n')


ANCHOR = '        CL_JKACgameGhoul2Resolve(identity);\n        if (jkaCgameGore) jkaCgameGore->Clear(identity);\n        result = 0;'


def at_anchor(new):
    return lambda text: text.replace(ANCHOR, new, 1)


MUTATIONS = {
    'no null refusal': lambda text: text.replace(
        '        if (!identity) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 cannot clear skin gore without a model identity");\n',
        '', 1),
    'no identity validation': at_anchor('        if (jkaCgameGore) jkaCgameGore->Clear(identity);\n        result = 0;'),
    'no native module gate': lambda text: text[:clear_case(text)[0]] + text[clear_case(text)[0]:].replace(
        '        CL_JKACgameGhoul2Native();\n', '', 1),
    'nonzero answer': at_anchor(ANCHOR.replace('result = 0;', 'result = 1;')),
    'store made before validation': at_anchor('        CL_JKACgameGhoul2Store();\n' + ANCHOR),
    'clear removes the models': at_anchor(ANCHOR.replace('        result = 0;',
        '        { void *handle = reinterpret_cast<void *>(std::uintptr_t(identity));'
        ' CL_JKACgameGhoul2Store().Clean(&handle); }\n        result = 0;')),
    'not served': lambda text: text[:clear_case(text)[0]] + text[clear_case(text)[1]:],
}

MAIN = r'''
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 intptr_t clear[]{315,0};
 // Before any model loads there is no store: neither a null nor an unknown
 // identity is served, and validating one never creates the store.
 Reject([&]{Invoke(clear);});assert(!jkaCgameGhoul2Store);
 clear[1]=0x1234;Reject([&]{Invoke(clear);});assert(!jkaCgameGhoul2Store);
 if(sizeof(void *)!=4){std::cout<<"x86-only cgame Ghoul2 refused\n";return 0;}
 auto files=Installed();assets=files.files;
 std::uint32_t identity=0;
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),
  reinterpret_cast<intptr_t>("models/players/test/model.glm"),0,5,0,0,0};
 assert(Invoke(init)==0&&identity);init[4]=6;assert(Invoke(init)==1);
 auto *instance=CL_JKACgameGhoul2Resolve(identity);
 clear[1]=identity;
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(clear);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Invoke(clear);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 // The clear answers 0 and leaves the instance, its models and their skins.
 for(int pass=0;pass<2;++pass){
  assert(Invoke(clear)==0);
  assert(CL_JKACgameGhoul2Resolve(identity)==instance&&instance->Count()==2);
  assert(instance->At(0)->customSkin==5&&instance->At(1)->customSkin==6);
 }
 intptr_t have[]{293,static_cast<intptr_t>(identity)};assert(Invoke(have)==1);
 assert(CL_JKACgameGhoul2Store().InstanceCount()==1);
 clear[1]=0;Reject([&]{Invoke(clear);});
 // An identity another store owns is refused.
 fntech3::ghoul2::Store foreign(files);void *foreignHandle=nullptr;fntech3::ghoul2::LoadError error;
 assert(foreign.AddModel(&foreignHandle,"models/players/test/model.glm",error)==0);
 clear[1]=static_cast<intptr_t>(reinterpret_cast<std::uintptr_t>(foreignHandle));Reject([&]{Invoke(clear);});
 assert(instance->Count()==2);
 // No gore was added, so both models count none (import 313). Gore itself
 // is added, counted and cleared in jka_cgame_skin_gore_tests.py.
 for(intptr_t place:{0,1}){intptr_t count[]{313,static_cast<intptr_t>(identity),place};assert(Invoke(count)==0);}
 // A cleaned identity is stale, and so is any after the owner is cleared.
 const auto stale=identity;intptr_t clean[]{301,reinterpret_cast<intptr_t>(&identity)};
 assert(!Invoke(clean)&&!identity);clear[1]=stale;Reject([&]{Invoke(clear);});
 assert(Invoke(init)==0&&identity);clear[1]=identity;assert(Invoke(clear)==0);
 CL_JKACgameClear();assert(!jkaCgameGhoul2Store);Reject([&]{Invoke(clear);});assert(!jkaCgameGhoul2Store);
 std::cout<<"Academy cgame import 315 passed\n";
 return 0;
}
'''


def fixture(header_text=None):
    header = (ROOT / HEADER).read_text() if header_text is None else header_text
    source = ghoul2_fixture()
    include = '#include "code/client/client_jka_cgame_ghoul2.hpp"'
    assert source.count(include) == 1
    source = source.replace(include, inline_header(header), 1)
    source = source.replace('int main(){', 'int LegacyCgameGhoul2Main(){', 1)
    at = source.rfind('}')
    assert source[at:].strip() == '}'
    source = source[:at] + '\nreturn 0;\n' + source[at:]
    return '#include <iostream>\n#ifdef _MSC_VER\n#include <stdlib.h>\n#endif\n' + source + MAIN


def build_and_run(compiler, folder, name, source):
    path = folder / f'{name}.cpp'
    path.write_text(source)
    executable = folder / (f'{name}.exe' if os.name == 'nt' else name)
    built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-Wall', '-Wextra',
        # The inlined header's unused helpers would be header-silent in place.
        '-Werror', '-Wno-unused-function', '-I', str(ROOT), str(path), '-o', str(executable)], cwd=ROOT,
        capture_output=True, text=True, timeout=180)
    if built.returncode:
        return 'build', built.stdout + built.stderr
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    ran = subprocess.run([str(executable)], cwd=folder, capture_output=True, text=True,
        timeout=60, **options)
    return ('ok' if ran.returncode == 0 else 'run'), ran.stdout + ran.stderr


class JKACgameClearSkinGoreTests(unittest.TestCase):
    def test_retail_dispatch_is_recorded_with_the_case(self):
        header = (ROOT / HEADER).read_text()
        start, end = clear_case(header)
        case = header[start:end]
        for fact in ('0x413cd5', '0x4e07f0', '0x4e07fb', '+0x4c', '0x4e9310'):
            self.assertIn(fact, case)
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text()
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_G2_CLEARSKINGORE, 315)', calls)

    def test_production_clear_and_failing_mutations(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        header = (ROOT / HEADER).read_text()
        mutated = {}
        for name, mutate in MUTATIONS.items():
            text = mutate(header)
            self.assertNotEqual(text, header, name)
            mutated[name] = text
        parent = ROOT / '.tmp/tests/jka-cgame-clear-skin-gore'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary)
            jobs = {'production': fixture()}
            jobs.update({f'mutant{n}': fixture(text) for n, text in enumerate(mutated.values())})
            with ThreadPoolExecutor(max_workers=4) as pool:
                results = dict(zip(jobs, pool.map(
                    lambda item: build_and_run(compiler, folder, item[0], item[1]), jobs.items())))
            status, output = results.pop('production')
            self.assertEqual(status, 'ok', output)
            if 'x86-only' in output:
                return
            for (name, _), (status, output) in zip(mutated.items(), results.values()):
                with self.subTest(mutation=name):
                    self.assertEqual(status, 'run', output)


if __name__ == '__main__':
    unittest.main()
