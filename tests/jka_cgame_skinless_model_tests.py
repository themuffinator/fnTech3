"""Serve Jedi Academy client-game import 312, CG_G2_SKINLESSMODEL.

Retail jamp.exe 1.01 routes import 312 through 0x413c84: 0x4116e0 reads the
instance through the module's identity and indexes its model records with no
bound, and 0x4e0740 answers true unless the model sets up and one record of
its surface hierarchy, walked in file order, names a shader. The cgame asks
once, for place 0 of a Ghoul2 general entity it has just made (0x30032957),
and on true registers that model's folder's model_default.skin. The fixture
compiles the production header (optionally mutated) beside the cgame Ghoul2
fixture; every mutation must fail.
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
CASE = 'case Call::CG_G2_SKINLESSMODEL: {'
HELPER = 'static bool CL_JKACgameGhoul2Skinless(const fntech3::ghoul2::Model *model) {'


def inline_header(text):
    # Inline the header so a mutated copy compiles in its place; its own
    # relative includes then resolve from the repository root.
    text = text.replace('#include "../', '#include "code/')
    return text.replace('#include "client_', '#include "code/client/client_')


def block(text, opening, closing):
    start = text.index(opening)
    return start, text.index(closing, start) + len(closing)


def skinless_case(text):
    return block(text, CASE, '\n    }\n')


def skinless_helper(text):
    return block(text, HELPER, '\n}\n')


def in_case(old, new):
    def mutate(text):
        start, end = skinless_case(text)
        return text[:start] + text[start:end].replace(old, new, 1) + text[end:]
    return mutate


def in_helper(old, new):
    def mutate(text):
        start, end = skinless_helper(text)
        return text[:start] + text[start:end].replace(old, new, 1) + text[end:]
    return mutate


MUTATIONS = {
    'no null refusal': in_case(
        '        if (!identity) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 skinless query requires a model identity");\n',
        ''),
    'no native module gate': in_case('        CL_JKACgameGhoul2Native();\n', ''),
    'no lower bound': in_case('args[2] < 0 || ', ''),
    'no upper bound': in_case(' || args[2] >= instance->Count()', ''),
    'upper bound off by one': in_case('args[2] >= instance->Count()', 'args[2] > instance->Count()'),
    'inverted answer': in_case('? qtrue : qfalse', '? qfalse : qtrue'),
    'empty place answers false': in_helper('    if (!model) return true;', '    if (!model) return false;'),
    'missing mesh answers true': in_helper(
        '    if (!model->meshBytes)\n        Com_Error',
        '    if (!model->meshBytes) return true;\n    if (false)\n        Com_Error'),
    'surface name read as shader': in_helper('bytes[record + 68]', 'bytes[record]'),
    'first surface only': in_helper(
        '        record += format::SurfHierarchyBytes + std::size_t(children) * 4;',
        '        break;'),
    'children not stepped over': in_helper(
        '        record += format::SurfHierarchyBytes + std::size_t(children) * 4;',
        '        record += format::SurfHierarchyBytes;'),
    'walk from the offset table end of header': in_helper(
        'auto record = static_cast<std::size_t>(reader.Int(156));',
        'auto record = format::MeshHeaderBytes;'),
    'header count not checked': in_helper(' || reader.Int(152) != model->mesh.numSurfaces', ''),
    'child count not bounded': in_helper(
        'children < 0 || !reader.Fits(record + format::SurfHierarchyBytes, 4, std::size_t(children))',
        'children < 0'),
    'not served': lambda text: text[:skinless_case(text)[0]] + text[skinless_case(text)[1]:],
}

MAIN = r'''
/* Three surfaces: a root with two children, each naming the given shader. */
static Bytes SkinlessTree(const std::array<const char *,3> &shaders){
 namespace format=fntech3::jka::model;
 Bytes file;
 AppendInt(file,std::int32_t(format::MeshIdent));AppendInt(file,format::MeshVersion);
 AppendName(file,"models/players/tree/model.glm");AppendName(file,"models/players/_humanoid/_humanoid");
 AppendInt(file,0);AppendInt(file,3);AppendInt(file,1);
 const std::size_t lodField=file.size();AppendInt(file,0);
 AppendInt(file,3);AppendInt(file,std::int32_t(format::MeshHeaderBytes+12));
 const std::size_t endField=file.size();AppendInt(file,0);
 const std::size_t offsets=file.size();for(int s=0;s<3;++s)AppendInt(file,0);
 for(int s=0;s<3;++s){
  PutInt(file,offsets+std::size_t(s)*4,std::int32_t(file.size()-format::MeshHeaderBytes));
  AppendName(file,std::string("s")+char('0'+s));AppendInt(file,0);AppendName(file,shaders[std::size_t(s)]);
  AppendInt(file,0);AppendInt(file,s?0:-1);AppendInt(file,s?0:2);
  if(!s){AppendInt(file,1);AppendInt(file,2);}
 }
 PutInt(file,lodField,std::int32_t(file.size()));
 const std::size_t lod=file.size();AppendInt(file,0);
 const std::size_t table=file.size();for(int s=0;s<3;++s)AppendInt(file,0);
 for(int s=0;s<3;++s){
  PutInt(file,table+std::size_t(s)*4,std::int32_t(file.size()-table));
  const std::size_t at=file.size();AppendSurface(file,3,1,1);PutInt(file,at+4,s);
  PutInt(file,at+8,-std::int32_t(at));
 }
 PutInt(file,lod,std::int32_t(file.size()-lod));PutInt(file,endField,std::int32_t(file.size()));
 return file;
}
static std::uint32_t Load(const char *name){
 std::uint32_t identity=0;
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>(name),0,0,0,0,0};
 assert(Invoke(init)==0&&identity);return identity;
}
static intptr_t Ask(std::uint32_t identity,intptr_t index){
 intptr_t ask[]{312,static_cast<intptr_t>(identity),index};return Invoke(ask);
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 // Before any model loads there is no store: neither a null nor an unknown
 // identity is served, and asking never creates the store.
 Reject([&]{Ask(0,0);});assert(!jkaCgameGhoul2Store);
 Reject([&]{Ask(0x1234,0);});assert(!jkaCgameGhoul2Store);
 if(sizeof(void *)!=4){std::cout<<"x86-only cgame Ghoul2 refused\n";return 0;}
 auto files=Installed();assets=files.files;
 // The fixture mesh's one surface names models/players/test/skin; a copy
 // with that field cleared is skinless, as 65 of Academy's 127 meshes are.
 Bytes bare=assets["models/players/test/model.glm"];
 const std::size_t shader=fntech3::jka::model::MeshHeaderBytes+4+68;
 assert(bare[shader]=='m');std::fill(bare.begin()+std::ptrdiff_t(shader),bare.begin()+std::ptrdiff_t(shader+64),0);
 assets["models/players/bare/model.glm"]=bare;
 assets["models/players/tree/model.glm"]=SkinlessTree({"","","models/players/tree/skin"});
 assets["models/players/treebare/model.glm"]=SkinlessTree({"","",""});
 assets["models/players/treeroot/model.glm"]=SkinlessTree({"models/players/tree/root","",""});
 const auto named=Load("models/players/test/model.glm");
 const auto skinless=Load("models/players/bare/model.glm");
 const auto tree=Load("models/players/tree/model.glm");
 const auto treeBare=Load("models/players/treebare/model.glm");
 const auto treeRoot=Load("models/players/treeroot/model.glm");
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Ask(named,0);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Ask(named,0);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 // A shader name anywhere in the hierarchy, past a parent's child list
 // included, makes the model skinned; asking twice changes nothing.
 for(int pass=0;pass<2;++pass){
  assert(Ask(named,0)==0&&Ask(skinless,0)==1);
  assert(Ask(tree,0)==0&&Ask(treeBare,0)==1&&Ask(treeRoot,0)==0);
 }
 // Places: an appended model answers for itself; one past the end, or
 // below zero, is refused (retail reads past its record array there).
 std::uint32_t mixed=named;
 intptr_t append[]{297,reinterpret_cast<intptr_t>(&mixed),reinterpret_cast<intptr_t>("models/players/bare/model.glm"),0,0,0,0,0};
 assert(Invoke(append)==1&&mixed==named);
 assert(Ask(named,0)==0&&Ask(named,1)==1);
 for(intptr_t index:{intptr_t(2),intptr_t(-1),intptr_t(INT_MIN),intptr_t(INT_MAX)})Reject([&]{Ask(named,index);});
 // An empty place (a copy past the end leaves one) fails retail's model
 // setup and answers true; the copied model past it answers for itself.
 intptr_t copy[]{308,static_cast<intptr_t>(tree),0,static_cast<intptr_t>(skinless),2};
 assert(Invoke(copy)==0);
 auto *grown=CL_JKACgameGhoul2Resolve(skinless);
 assert(grown->Count()==3&&!grown->At(1)&&grown->At(2));
 assert(Ask(skinless,1)==1&&Ask(skinless,2)==0&&Ask(skinless,0)==1);Reject([&]{Ask(skinless,3);});
 // The query reads; it never loads, binds a skin or moves a model.
 const int readsBefore=opens;assert(Ask(named,1)==1&&opens==readsBefore);
 assert(CL_JKACgameGhoul2Resolve(named)->Count()==2&&CL_JKACgameGhoul2Resolve(named)->At(0)->customSkin==0);
 // A foreign identity, a stale one and a null one are refused.
 fntech3::ghoul2::Store foreign(files);void *foreignHandle=nullptr;fntech3::ghoul2::LoadError error;
 assert(foreign.AddModel(&foreignHandle,"models/players/test/model.glm",error)==0);
 Reject([&]{Ask(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(foreignHandle)),0);});
 std::uint32_t gone=treeBare;intptr_t clean[]{301,reinterpret_cast<intptr_t>(&gone)};
 assert(!Invoke(clean)&&!gone);Reject([&]{Ask(treeBare,0);});Reject([&]{Ask(0,0);});
 // The test itself, on bounded records: no kept mesh, a header that no
 // longer matches the parsed count, and a child count past the file.
 const auto &kept=*CL_JKACgameGhoul2Resolve(tree)->At(0);
 fntech3::ghoul2::Model model;model.name="models/players/tree/model.glm";
 Reject([&]{CL_JKACgameGhoul2Skinless(&model);});
 assert(CL_JKACgameGhoul2Skinless(nullptr));
 model.meshBytes=kept.meshBytes;model.mesh=kept.mesh;assert(!CL_JKACgameGhoul2Skinless(&model));
 std::vector<unsigned char> treeBareBytes(assets["models/players/treebare/model.glm"].begin(),
  assets["models/players/treebare/model.glm"].end());
 model.meshBytes=&treeBareBytes;assert(CL_JKACgameGhoul2Skinless(&model));
 model.mesh.numSurfaces=1;Reject([&]{CL_JKACgameGhoul2Skinless(&model);});model.mesh.numSurfaces=3;
 auto wide=treeBareBytes;
 const std::size_t rootChildren=fntech3::jka::model::MeshHeaderBytes+12+140;
 wide[rootChildren]=0xff;wide[rootChildren+1]=0xff;wide[rootChildren+2]=0;wide[rootChildren+3]=0;
 model.meshBytes=&wide;Reject([&]{CL_JKACgameGhoul2Skinless(&model);});
 wide[rootChildren+3]=0x80;Reject([&]{CL_JKACgameGhoul2Skinless(&model);});
 // The last record's child list must fit too, though no record follows it.
 auto tail=treeBareBytes;const std::size_t lastChildren=fntech3::jka::model::MeshHeaderBytes+12+152+144+140;
 assert(tail[lastChildren]==0);tail[lastChildren+2]=0x01;
 model.meshBytes=&tail;Reject([&]{CL_JKACgameGhoul2Skinless(&model);});
 // Clearing the client game drops the store; asking does not remake it.
 CL_JKACgameClear();assert(!jkaCgameGhoul2Store);Reject([&]{Ask(named,0);});assert(!jkaCgameGhoul2Store);
 std::cout<<"Academy cgame import 312 passed\n";
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


class JKACgameSkinlessModelTests(unittest.TestCase):
    def test_retail_dispatch_is_recorded_with_the_code(self):
        header = (ROOT / HEADER).read_text()
        start, end = skinless_case(header)
        case = header[start:end]
        for fact in ('0x413c84', '0x4116e0', '0x4116e9', '0xc8', '0x30032957'):
            self.assertIn(fact, case)
        start, end = skinless_helper(header)
        helper = header[start - 600:end]
        for fact in ('0x4e0740', '0x4e0b10', '+0x44', 'mModelindex -1', '0x49ae00'):
            self.assertIn(fact, helper)
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text()
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_G2_SKINLESSMODEL, 312)', calls)

    def test_production_query_and_failing_mutations(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        header = (ROOT / HEADER).read_text()
        mutated = {}
        for name, mutate in MUTATIONS.items():
            text = mutate(header)
            self.assertNotEqual(text, header, name)
            mutated[name] = text
        parent = ROOT / '.tmp/tests/jka-cgame-skinless-model'
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
