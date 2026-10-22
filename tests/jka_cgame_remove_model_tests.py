"""Serve Jedi Academy client-game import 311, CG_G2_REMOVEGHOUL2MODEL.

Retail jamp.exe 1.01 routes import 311 through 0x413c69 to the game's own
removal core 0x4decc0: an index at or past the instance's places, or a place
already empty, answers 0; otherwise the place is emptied, empty places at the
end are dropped, and an instance left with none is freed and the module's
slot zeroed. The client's init (import 297, 0x4de5c0) then fills the first
empty place. fnTech3's store does the same, and the removed model's
visibility binding goes with it. The fixture compiles the production header
(optionally mutated) beside the cgame Ghoul2 fixture. See
docs/fntech3/2026-10-22-jka-g2-surfaces.md.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shutil
import tempfile
import unittest

from jka_cgame_clear_skin_gore_tests import build_and_run, inline_header
from jka_cgame_ghoul2_tests import fixture as ghoul2_fixture

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'code/client/client_jka_cgame_ghoul2.hpp'
CASE = 'case Call::CG_G2_REMOVEGHOUL2MODEL: {'


def remove_case(text):
    start = text.index(CASE)
    return start, text.index('\n    }\n', start) + len('\n    }\n')


def in_case(text, old, new):
    start, end = remove_case(text)
    return text[:start] + text[start:end].replace(old, new, 1) + text[end:]


MUTATIONS = {
    'not served': lambda text: text[:remove_case(text)[0]] + text[remove_case(text)[1]:],
    'no native module gate': lambda text: in_case(text, '        CL_JKACgameGhoul2Native();\n', ''),
    'empty slot answered': lambda text: in_case(text,
        '        if (!identity) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 removal was handed an empty model slot");\n',
        ''),
    'negative index answered': lambda text: in_case(text,
        '        if (modelIndex < 0)\n', '        if (modelIndex < -1)\n'),
    'nothing removed answers 1': lambda text: in_case(text,
        'result = removal == Removal::NoModel ? qfalse : qtrue;', 'result = qtrue;'),
    'binding kept': lambda text: in_case(text,
        '            jkaCgameSkins->bindings.erase(std::make_pair(identity, modelIndex));', '            ;'),
    'bindings kept after deletion': lambda text: in_case(text,
        'if (removal == Removal::Deleted) CL_JKACgameSkinCleanModel(identity);',
        'if (removal == Removal::Deleted) {}'),
    'init appends': lambda text: text.replace(
        'false,fntech3::ghoul2::NameMatch::AsciiFolded,true);',
        'false,fntech3::ghoul2::NameMatch::AsciiFolded,false);', 1),
}

MAIN = r'''
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 if(sizeof(void *)!=4){std::cout<<"x86-only cgame Ghoul2 refused\n";return 0;}
 auto files=Installed();assets=files.files;
 // The slot sits unaligned between guard bytes, as module memory may.
 std::array<byte,11> slot;slot.fill(0xa5);std::memset(slot.data()+3,0,4);
 const auto read=[&]{std::uint32_t value;std::memcpy(&value,slot.data()+3,4);return value;};
 intptr_t remove[]{311,reinterpret_cast<intptr_t>(slot.data()+3),0};
 // Retail reads the instance through the slot: an empty one is refused, and
 // refusing it makes no store.
 Reject([&]{Invoke(remove);});assert(!jkaCgameGhoul2Store);
 intptr_t init[]{297,reinterpret_cast<intptr_t>(slot.data()+3),
  reinterpret_cast<intptr_t>("models/players/test/model.glm"),0,5,0,0,0};
 assert(Invoke(init)==0);init[4]=6;assert(Invoke(init)==1);init[4]=7;assert(Invoke(init)==2);
 const std::uint32_t first=read();
 const auto *instance=CL_JKACgameGhoul2Resolve(first);
 assert(instance&&instance->Count()==3);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Invoke(remove);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Invoke(remove);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 // A negative index reads before retail's records; one past the places and
 // an empty place answer 0 and change nothing.
 remove[2]=-1;Reject([&]{Invoke(remove);});assert(instance->Count()==3);
 remove[2]=3;assert(Invoke(remove)==0&&instance->Count()==3);
 auto &bindings=CL_JKACgameSkinState().bindings;
 bindings[{first,0}].overrides[0]=2;bindings[{first,1}].overrides[0]=2;
 // Removing model 1 empties its place and drops its visibility binding.
 remove[2]=1;assert(Invoke(remove)==1);
 assert(read()==first&&instance->Count()==3&&!instance->At(1)&&instance->At(0)&&instance->At(2));
 assert(!bindings.count({first,1})&&bindings.count({first,0}));
 assert(Invoke(remove)==0);
 intptr_t has[]{310,reinterpret_cast<intptr_t>(slot.data()+3),1};assert(Invoke(has)==0);
 // The init fills the empty place, with its own skin.
 init[4]=8;assert(Invoke(init)==1&&instance->At(1)&&instance->At(1)->customSkin==8);
 assert(instance->Count()==3&&Invoke(has)==1);
 // Empty places at the end are dropped.
 remove[2]=1;assert(Invoke(remove)==1);remove[2]=2;assert(Invoke(remove)==1);
 assert(instance->Count()==1&&read()==first);
 // The last model frees the instance, zeroes the slot and forgets the
 // instance's bindings; the identity is stale from then on.
 remove[2]=0;assert(Invoke(remove)==1);
 assert(read()==0&&slot[2]==0xa5&&slot[7]==0xa5);
 assert(!bindings.count({first,0}));
 Reject([&]{CL_JKACgameGhoul2Resolve(first);});
 std::memcpy(slot.data()+3,&first,4);Reject([&]{Invoke(remove);});std::memset(slot.data()+3,0,4);
 // An identity another store owns is refused.
 fntech3::ghoul2::Store foreign(files);void *foreignHandle=nullptr;fntech3::ghoul2::LoadError error;
 assert(foreign.AddModel(&foreignHandle,"models/players/test/model.glm",error)==0);
 const auto foreignIdentity=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(foreignHandle));
 std::memcpy(slot.data()+3,&foreignIdentity,4);remove[2]=0;Reject([&]{Invoke(remove);});
 assert(read()==foreignIdentity);std::memset(slot.data()+3,0,4);
 // A new init makes a new instance in the zeroed slot.
 init[4]=0;assert(Invoke(init)==0&&read()&&read()!=first);
 CL_JKACgameClear();assert(!jkaCgameGhoul2Store);
 std::cout<<"Academy cgame import 311 passed\n";
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


class JKACgameRemoveModelTests(unittest.TestCase):
    def test_retail_dispatch_is_recorded_with_the_case(self):
        header = (ROOT / HEADER).read_text()
        start, end = remove_case(header)
        case = header[start:end]
        for fact in ('311:0x413c69', '0x4decc0', 'RemoveModel'):
            self.assertIn(fact, case)
        self.assertIn('0x4de5c0', header)
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text()
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_G2_REMOVEGHOUL2MODEL, 311)', calls)

    def test_production_removal_and_failing_mutations(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        header = (ROOT / HEADER).read_text()
        mutated = {}
        for name, mutate in MUTATIONS.items():
            text = mutate(header)
            self.assertNotEqual(text, header, name)
            mutated[name] = text
        parent = ROOT / '.tmp/tests/jka-cgame-remove-model'
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
