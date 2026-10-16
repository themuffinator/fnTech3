"""Serve Jedi Academy client-game import 321, CG_G2_SETSURFACEONOFF.

Retail jamp.exe 1.01 routes import 321 through 0x413d7e to 0x4deb70, which
works on model 0 of the instance: a null identity, no models or an empty
place 0 answer 0; otherwise 0x4ee700 finds the surface by name (_stricmp),
replaces only its OFF (2) and NODESCENDANTS (0x100) bits in the model's
override list, shared with the skin's visibility, and answers 1. The fixture
compiles the production Ghoul2 and render headers (optionally mutated) into
the client-game render fixture and draws through Fn's renderer hooks; every
mutation must fail.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_cgame_render_tests import fixture as render_fixture

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'code/client/client_jka_cgame_ghoul2.hpp'
RENDER = 'code/client/client_jka_cgame_render.hpp'
CASE = 'case Call::CG_G2_SETSURFACEONOFF: {'
NAMED = 'static int CL_JKACgameGhoul2SurfaceNamed('
FLAGS = 'static void CL_JKACgameGhoul2SurfaceFlags('


def inline_header(text):
    # Inline the header so a mutated copy compiles in its place; its own
    # relative includes then resolve from the repository root.
    text = text.replace('#include "../', '#include "code/')
    return text.replace('#include "client_', '#include "code/client/client_')


def block(text, opening, closing):
    start = text.index(opening)
    return start, text.index(closing, start) + len(closing)


def within(opening, closing, old, new):
    def mutate(text):
        start, end = block(text, opening, closing)
        return text[:start] + text[start:end].replace(old, new, 1) + text[end:]
    return mutate


def in_case(old, new):
    return within(CASE, '\n    }\n', old, new)


def in_named(old, new):
    return within(NAMED, '\n}\n', old, new)


def in_flags(old, new):
    return within(FLAGS, '\n}\n', old, new)


MUTATIONS = {
    'no native module gate': in_case('        CL_JKACgameGhoul2Native();\n', ''),
    'null identity refused': in_case(
        '        if (!identity) return true;',
        '        if (!identity) Com_Error(ERR_DROP, "mutant");'),
    'store made before the null check': in_case(
        '        if (!identity) return true;',
        '        CL_JKACgameGhoul2Store();\n        if (!identity) return true;'),
    'last model instead of model 0': in_case(
        'CL_JKACgameGhoul2Resolve(identity)->At(0);',
        'CL_JKACgameGhoul2Resolve(identity)->At(CL_JKACgameGhoul2Resolve(identity)->Count() - 1);'),
    'empty place 0 refused': in_case(
        '        if (!model) return true;',
        '        if (!model) Com_Error(ERR_DROP, "mutant");'),
    'null name read as no surface': in_case(
        '        if (!args[2]) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 surface switch requires a surface name");',
        '        if (!args[2]) return true;'),
    'long name refused': in_case(
        '        if (name.back()) return true;',
        '        if (name.back()) Com_Error(ERR_DROP, "mutant");'),
    'absent surface answers 1': in_case(
        '        if (surface < 0) return true;',
        '        if (surface < 0) { result = qtrue; return true; }'),
    'success answers 0': in_case(
        '        result = qtrue;\n        return true;', '        return true;'),
    'not served': lambda text: text[:block(text, CASE, '\n    }\n')[0]] + text[block(text, CASE, '\n    }\n')[1]:],
    'names matched exactly': in_named('NameMatch::AsciiFolded', 'NameMatch::Exact'),
    'children not stepped over': in_named(
        '        record += format::SurfHierarchyBytes + std::size_t(children) * 4;',
        '        record += format::SurfHierarchyBytes;'),
    'walk from the end of the header': in_named(
        'auto record = static_cast<std::size_t>(reader.Int(156));',
        'auto record = format::MeshHeaderBytes;'),
    'default flags read from the shader': in_named(
        'flags = reader.Unsigned(record + 64);', 'flags = reader.Unsigned(record + 68);'),
    'unterminated name not refused': in_named(
        '        if (!std::memchr(stored, 0, format::NameBytes))\n            Com_Error',
        '        if (false)\n            Com_Error'),
    'only OFF switched': in_flags(
        '(current & ~0x102u) | (requested & 0x102u)', '(current & ~0x2u) | (requested & 0x2u)'),
    'other bits dropped': in_flags('(current & ~0x102u) | (requested & 0x102u)', '(requested & 0x102u)'),
    'listed override ignored': in_flags(
        '            if (listed != found->second.overrides.end()) current = listed->second;\n', ''),
    'default kept as an override': in_flags(
        '    if (selected == defaults) binding.overrides.erase(surface);\n    else binding.overrides[surface] = selected;',
        '    binding.overrides[surface] = selected;'),
    'empty binding kept': in_flags(
        '    if (!binding.renderSkin && binding.overrides.empty()) jkaCgameSkins->bindings.erase(key);\n', ''),
    'state made by an unchanged switch': in_flags('    if (selected == current) return;\n', ''),
}

RENDER_MUTATIONS = {
    'render skin 0 read as stale': lambda text: text.replace(
        'if (binding->renderSkin && !jkaCgameSkins->definitions.count(binding->renderSkin))',
        'if (!jkaCgameSkins->definitions.count(binding->renderSkin))', 1),
}

MAIN = r'''
static intptr_t Switch(std::uint32_t identity,const char *name,std::uint32_t flags){
 intptr_t args[]{321,static_cast<intptr_t>(identity),reinterpret_cast<intptr_t>(name),static_cast<intptr_t>(flags)};
 return Invoke(args);
}
static std::uint32_t Load(std::uint32_t identity,const char *name,intptr_t expected){
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>(name),0,0,0,0,0};
 assert(Invoke(init)==expected&&identity);return identity;
}
static const std::map<int,std::uint32_t> *Overrides(std::uint32_t identity,int index){
 if(!jkaCgameSkins)return nullptr;const auto found=jkaCgameSkins->bindings.find({identity,index});
 return found==jkaCgameSkins->bindings.end()?nullptr:&found->second.overrides;
}
using Visible=std::vector<int>;
static fntech3_jka_mp_refEntity_t entity;static fntech3_jka_mp_refdef_t scene3d;
static Visible Drawn(std::uint32_t identity){
 entity.ghoul2=identity;Queue(entity);Render(scene3d);ClearScene();
 return scene::state.models.at({identity,0}).visible;
}
/* The geometry fixture's two surfaces, root (index 0) and its child (1),
 * with the given default flags. */
static Bytes Tree(std::int32_t root,std::int32_t child){
 auto file=GeometryFixture();fntech3::jka::model::Reader reader(file.data(),file.size());
 PutInt(file,164+static_cast<std::size_t>(reader.Int(164))+64,root);
 PutInt(file,164+static_cast<std::size_t>(reader.Int(168))+64,child);
 return file;
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 // A null identity answers 0 (0x4deb76), before any model loads too, and
 // makes no store; an unknown one is refused, and makes none either.
 assert(Switch(0,"root",2)==0&&!jkaCgameGhoul2Store&&!jkaCgameSkins);
 Reject([&]{Switch(0x1234,"root",2);});assert(!jkaCgameGhoul2Store);
 if(sizeof(void *)!=4){std::cout<<"x86-only cgame Ghoul2 refused\n";return 0;}
 auto files=Installed();assets=files.files;
 assets["models/players/weighted/pose.gla"]=PoseSkeleton();
 assets["models/players/weighted/model.glm"]=Tree(0,2);
 assets["models/players/bolted/model.glm"]=Tree(1,2);
 const auto tree=Load(0,"models/players/weighted/model.glm",0);
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Switch(tree,"root",2);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Switch(tree,"root",2);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 // Unknown, partial, empty and over-long names answer 0 and record nothing.
 for(const char *name:{"absent","roo","rootx","child ",""})assert(Switch(tree,name,2)==0);
 std::array<char,MAX_QPATH+8> longName;longName.fill('r');longName.back()=0;assert(Switch(tree,longName.data(),2)==0);
 std::array<char,MAX_QPATH> edge;edge.fill('r');edge.back()=0;assert(Switch(tree,edge.data(),2)==0);
 // A surface already as asked answers 1 and leaves no state behind.
 assert(Switch(tree,"child",2)==1&&Switch(tree,"root",0)==1&&!jkaCgameSkins);
 {intptr_t args[]{321,static_cast<intptr_t>(tree),0,2};Reject([&]{Invoke(args);});}assert(!jkaCgameSkins);
 // Names fold case. The child is off by default (2): 321 turns it on, which
 // a skin's material cannot, in a binding with no render skin.
 assert(Switch(tree,"CHILD",0)==1&&*Overrides(tree,0)==(std::map<int,std::uint32_t>{{1,0u}}));
 assert(jkaCgameSkins->bindings.at({tree,0}).renderSkin==0);
 // Back to the default drops the entry, and the last one drops the binding.
 assert(Switch(tree,"Child",0x80)==1&&Overrides(tree,0)&&Switch(tree,"child",2)==1&&!Overrides(tree,0));
 // Only OFF and NODESCENDANTS change: a default's other bit (1) is kept.
 const auto bolted=Load(0,"models/players/bolted/model.glm",0);
 assert(Switch(bolted,"Root",0xffffffffu)==1&&Overrides(bolted,0)->at(0)==0x103);
 assert(Switch(bolted,"ROOT",2)==1&&Overrides(bolted,0)->at(0)==3);
 assert(Switch(bolted,"root",2)==1&&Overrides(bolted,0)->at(0)==3);
 assert(Switch(bolted,"root",0x100)==1&&Overrides(bolted,0)->at(0)==0x101);
 assert(Switch(bolted,"root",0x7ffffefd)==1&&!Overrides(bolted,0));
 // The renderer draws what the switches say: NODESCENDANTS hides the
 // surface and every child, OFF the surface alone.
 for(int axis=0;axis<3;++axis)entity.axis[axis][axis]=1;entity.renderfx=0x82;
 scene3d.width=640;scene3d.height=480;scene3d.fov_x=90;scene3d.fov_y=75;scene3d.time=99999;scene3d.rdflags=0x10;
 jkaCgameBoneClocks.BeginFrame(100,false);
 assert(Drawn(tree)==Visible{0});
 assert(Switch(tree,"child",0)==1&&Drawn(tree)==Visible({0,1}));
 assert(Switch(tree,"root",0x100)==1&&Drawn(tree)==Visible{});
 assert(Switch(tree,"root",2)==1&&Drawn(tree)==Visible{1});
 assert(Switch(tree,"root",0)==1&&Drawn(tree)==Visible({0,1}));
 // A render skin starts the list again (retail 0x4ee7e0 clears it); a skin
 // change without one keeps it.
 const std::string allText="root,material/root\nchild,material/child\n";
 assets["models/players/weighted/all.skin"]=Bytes(allText.begin(),allText.end());
 intptr_t skinArgs[]{55,reinterpret_cast<intptr_t>("models/players/weighted/all.skin")};const auto skin=Invoke(skinArgs);
 assert(skin>0);intptr_t setSkin[]{298,static_cast<intptr_t>(tree),0,skin,skin};
 assert(Invoke(setSkin)==1&&Overrides(tree,0)->empty()&&Drawn(tree)==Visible{0});
 assert(Switch(tree,"child",0)==1&&Overrides(tree,0)->at(1)==0&&jkaCgameSkins->bindings.at({tree,0}).renderSkin==skin);
 setSkin[4]=0;assert(Invoke(setSkin)==1&&Drawn(tree)==Visible({0,1}));
 // Model 0 alone: a surface only model 1 carries answers 0, and nothing is
 // bound to model 1.
 const auto pair=Load(0,"models/players/test/model.glm",0);assert(Load(pair,"models/players/weighted/model.glm",1)==pair);
 assert(Switch(pair,"child",0)==0&&!Overrides(pair,0)&&!Overrides(pair,1));
 assert(Switch(pair,"root",2)==1&&Overrides(pair,0)->at(0)==2&&!Overrides(pair,1));
 // An empty place 0 fails retail's setup and answers 0 before the name is read.
 auto *first=const_cast<fntech3::ghoul2::Model *>(CL_JKACgameGhoul2Resolve(pair)->At(0));first->present=false;
 assert(Switch(pair,"root",0)==0);{intptr_t args[]{321,static_cast<intptr_t>(pair),0,0};assert(Invoke(args)==0);}
 first->present=true;assert(Overrides(pair,0)->at(0)==2);
 // A duplicate carries the list; a clean drops it with the instance.
 std::uint32_t copy=0;intptr_t duplicate[]{309,static_cast<intptr_t>(pair),reinterpret_cast<intptr_t>(&copy)};
 assert(Invoke(duplicate)==0&&copy&&Overrides(copy,0)->at(0)==2);
 std::uint32_t gone=copy;intptr_t clean[]{301,reinterpret_cast<intptr_t>(&gone)};
 assert(!Invoke(clean)&&!gone&&!Overrides(copy,0));Reject([&]{Switch(copy,"root",2);});
 // An identity another store owns is refused.
 fntech3::ghoul2::Store foreign(files);void *foreignHandle=nullptr;fntech3::ghoul2::LoadError error;
 assert(foreign.AddModel(&foreignHandle,"models/players/test/model.glm",error)==0);
 Reject([&]{Switch(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(foreignHandle)),"root",2);});
 // The lookup refuses a changed header and an unterminated record name.
 const auto &kept=*CL_JKACgameGhoul2Resolve(tree)->At(0);std::uint32_t defaults=0;
 fntech3::ghoul2::Model model;model.name="models/players/weighted/model.glm";
 Reject([&]{CL_JKACgameGhoul2SurfaceNamed(model,"root",defaults);});
 auto bytes=*kept.meshBytes;model.meshBytes=&bytes;model.mesh=kept.mesh;
 assert(CL_JKACgameGhoul2SurfaceNamed(model,"CHILD",defaults)==1&&defaults==2);
 model.mesh.numSurfaces=1;Reject([&]{CL_JKACgameGhoul2SurfaceNamed(model,"root",defaults);});model.mesh.numSurfaces=2;
 fntech3::jka::model::Reader reader(bytes.data(),bytes.size());const auto rootName=164+static_cast<std::size_t>(reader.Int(164));
 std::fill(bytes.begin()+std::ptrdiff_t(rootName),bytes.begin()+std::ptrdiff_t(rootName+64),'r');
 Reject([&]{CL_JKACgameGhoul2SurfaceNamed(model,"child",defaults);});
 // Clearing the client game drops the store and the lists; a null identity
 // still answers 0 and makes neither.
 CL_JKACgameClear();assert(!jkaCgameGhoul2Store&&!jkaCgameSkins);
 assert(Switch(0,"root",2)==0&&!jkaCgameGhoul2Store&&!jkaCgameSkins);Reject([&]{Switch(tree,"root",2);});
 std::cout<<"Academy cgame import 321 passed\n";
 return 0;
}
'''


def fixture(header_text=None, render_text=None):
    source = render_fixture()
    for path, text in ((HEADER, header_text), (RENDER, render_text)):
        include = f'#include "{path}"'
        assert source.count(include) == 1, path
        body = (ROOT / path).read_text() if text is None else text
        source = source.replace(include, inline_header(body), 1)
    source = source.replace('int main(){', 'int RenderMain(){', 1)
    at = source.rfind('}')
    assert source[at:].strip() == '}'
    source = source[:at] + '\nreturn 0;\n' + source[at:]
    return '#include <iostream>\n#ifdef _MSC_VER\n#include <stdlib.h>\n#endif\n' + source + MAIN


def build_and_run(compiler, folder, name, source):
    path = folder / f'{name}.cpp'
    path.write_text(source)
    executable = folder / (f'{name}.exe' if os.name == 'nt' else name)
    # The render fixture's own flags (jka_cgame_render_tests.py).
    built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-O2', '-Werror',
        '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(path), '-o', str(executable)], cwd=ROOT,
        capture_output=True, text=True, timeout=240)
    if built.returncode:
        return 'build', built.stdout + built.stderr
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    ran = subprocess.run([str(executable)], cwd=folder, capture_output=True, text=True,
        timeout=60, **options)
    return ('ok' if ran.returncode == 0 else 'run'), ran.stdout + ran.stderr


class JKACgameSurfaceOnOffTests(unittest.TestCase):
    def test_retail_dispatch_is_recorded_with_the_code(self):
        header = (ROOT / HEADER).read_text()
        start, end = block(header, CASE, '\n    }\n')
        case = header[start:end]
        for fact in ('0x413d7e', '0x4deb70', '0x4deb76', '0x496650', '0x4e0b10', '+0x9c'):
            self.assertIn(fact, case)
        start, end = block(header, NAMED, '\n}\n')
        named = header[start - 400:end]
        for fact in ('0x4ee5b0', '+0x9c', '0x539336', '+0x40'):
            self.assertIn(fact, named)
        start, end = block(header, FLAGS, '\n}\n')
        flags = header[start - 600:end]
        for fact in ('0x4ee700', '0x4ee7e0', '0x4ee610', '0x100'):
            self.assertIn(fact, flags)
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text()
        self.assertIn('FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, CG_G2_SETSURFACEONOFF, 321)', calls)

    def test_production_switch_and_failing_mutations(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        header = (ROOT / HEADER).read_text()
        render = (ROOT / RENDER).read_text()
        mutated = {}
        for name, mutate in MUTATIONS.items():
            text = mutate(header)
            self.assertNotEqual(text, header, name)
            mutated[name] = (text, None)
        for name, mutate in RENDER_MUTATIONS.items():
            text = mutate(render)
            self.assertNotEqual(text, render, name)
            mutated[name] = (None, text)
        parent = ROOT / '.tmp/tests/jka-cgame-surface-on-off'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary)
            jobs = {'production': fixture()}
            jobs.update({f'mutant{n}': fixture(*texts) for n, texts in enumerate(mutated.values())})
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
