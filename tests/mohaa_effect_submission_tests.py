"""MOHAA's hidden effect carriers must not hide their submitted particles.

Compile the production scene adapters against a recording renderer. Retail
1.00 sends the gas barrel's sprites and TIKI debris with renderfx 0x90:
RF_DONTDRAW belongs to the carrier, which the cgame has already suppressed.
No retail code or assets are needed for these boundary tests.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class MohaaEffectSubmissionTests(unittest.TestCase):
    def test_submitted_effects_keep_geometry_color_time_and_profile_flags(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        source = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        model = source[source.index('struct Model {'):source.index('typedef void ( QDECL *ProcessInitCommands_t )')]
        scene = source[source.index('int EngineRenderFx('):source.index('/* Slot 90:')]
        host = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/mohaa_cgame_abi.h"
#include "code/mohaa/mohaa_tiki.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace mohaa = fntech3::mohaa;
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
std::vector<refEntity_t> drawn;
void Submit(const refEntity_t *e, qboolean view) { assert(!view); drawn.push_back(*e); }
void Clear() { drawn.clear(); }
struct { decltype(&Submit) AddRefEntityToScene; decltype(&Clear) ClearScene; } re{Submit, Clear};
template<typename T> T *Pointer(std::uint32_t address) {
    return reinterpret_cast<T *>(static_cast<std::uintptr_t>(address));
}
'''
        checks = r'''
void CheckCommon(const mohaa::RefEntity &in, const refEntity_t &out) {
    assert(!memcmp(in.origin, out.origin, sizeof(in.origin)));
    assert(!memcmp(in.oldorigin, out.oldorigin, sizeof(in.oldorigin)));
    assert(!memcmp(in.axis, out.axis, sizeof(in.axis)));
    assert(!memcmp(in.shaderRGBA, out.shader.rgba, 4));
    assert(out.shaderTime.f == in.shaderTime);
    assert(out.mohaa.renderfx == in.renderfx);
    assert(out.radius == in.radius && out.rotation == in.rotation);
    // MOHAA's 0x80 must never become Quake III's RF_LIGHTING_ORIGIN.
    assert(!(out.renderfx & RF_LIGHTING_ORIGIN));
}
int main() {
    // One registered sprite, one TIKI debris model and one plain model.
    Model sprite; sprite.name="fire.spr"; sprite.renderer=31; sprite.sprite=true;
    Model debris; debris.name="debris.tik"; debris.renderer=32;
    debris.tiki=reinterpret_cast<dtiki_t *>(std::uintptr_t(0x1234));
    Model plain; plain.name="fixture.md3"; plain.renderer=33;
    models.push_back(sprite); models.push_back(debris); models.push_back(plain);
    mohaa::RefEntity e{};
    e.origin[0]=12; e.origin[1]=-42; e.origin[2]=320;
    e.oldorigin[0]=76; e.oldorigin[2]=300;
    e.axis[0][1]=1; e.axis[1][0]=-1; e.axis[2][2]=1;
    e.scale=2.25f; e.radius=24; e.rotation=35;
    e.shaderRGBA[0]=200; e.shaderRGBA[1]=100; e.shaderRGBA[2]=50; e.shaderRGBA[3]=96;
    e.shaderTime=4.5f; e.entityNumber=27;
    e.frameInfo[0].index=3; e.frameInfo[0].weight=1; e.frameInfo[0].time=.25f;
    e.actionWeight=.5f; e.surfaces[2]=4; e.shader_data[1]=.75f;
    for (int flags : {0, 0x10, 0x80, 0x90, 0x94, 0x20090}) {
        e.renderfx=flags;
        ClearScene(); e.hModel=1;
        AddRefSpriteToScene(&e);
        assert(drawn.size()==1 && drawn.back().reType==RT_ALICE_SPRITE);
        assert(drawn.back().hModel==31 && drawn.back().mohaa.scale==e.scale);
        CheckCommon(e, drawn.back());
        assert(bool(drawn.back().renderfx & RF_DEPTHHACK)==bool(flags & 4));
        assert(bool(drawn.back().renderfx & RF_MINLIGHT)==bool(flags & (1<<17)));
        // Model debris, shader sprites and every other supported scene type.
        const int types[]={0,2,3,4,5,6,7};
        const refEntityType_t translated[]={RT_MODEL,RT_SPRITE,RT_BEAM,RT_RAIL_CORE,
            RT_RAIL_RINGS,RT_LIGHTNING,RT_PORTALSURFACE};
        for (int i=0;i<7;++i) {
            ClearScene(); e.reType=types[i]; e.hModel=2; e.customShader=71;
            AddRefEntityToScene(&e,1023);
            assert(drawn.size()==1 && drawn.back().reType==translated[i]);
            CheckCommon(e, drawn.back());
            assert(GetRenderEntity(27) && GetRenderEntity(27)->renderfx==flags);
            if (!i) {
                const auto &m=drawn.back().mohaa;
                assert(drawn.back().hModel==32 && m.pose.tiki==debris.tiki);
                assert(m.scale==e.scale && m.pose.frameInfo[0].index==3);
                assert(m.pose.frameInfo[0].time==.25f && m.pose.actionWeight==.5f);
                assert(m.surfaces[2]==4 && m.shaderData[1]==.75f);
            }
        }
        ClearScene(); e.reType=0; e.hModel=3;
        AddRefEntityToScene(&e,1023);
        assert(drawn.size()==1 && drawn.back().hModel==33 && !drawn.back().mohaa.pose.tiki);
    }
    // Invalid handles, null input and unsupported/invalid types remain safe.
    ClearScene(); AddRefEntityToScene(nullptr,1023); AddRefSpriteToScene(nullptr);
    e.reType=0;
    for (int handle : {-1,0,4,2000000000}) {
        e.hModel=handle; AddRefEntityToScene(&e,1023); AddRefSpriteToScene(&e);
    }
    e.hModel=1; models[1].bad=true; AddRefSpriteToScene(&e);
    models[1].bad=false; models[1].renderer=0; AddRefSpriteToScene(&e);
    e.reType=1; AddRefEntityToScene(&e,1023);
    for (int type : {-1,8,2000000000}) {
        e.reType=type; bool dropped=false;
        try { AddRefEntityToScene(&e,1023); } catch (Drop) { dropped=true; }
        assert(dropped);
    }
    assert(drawn.empty());
    ClearScene(); assert(!GetRenderEntity(27));
}
'''
        scratch = ROOT / '.tmp/mohaa-effects/tests'
        scratch.mkdir(parents=True, exist_ok=True)
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        with tempfile.TemporaryDirectory(dir=scratch) as temp:
            folder = Path(temp)
            # Independently restore each old rejection. Both must fail the
            # fixture, proving the sprite and debris paths are exercised.
            for name, body, success in (
                ('production', scene, True),
                ('old-sprite-filter', scene.replace(
                    'void QDECL AddRefSpriteToScene( const mohaa::RefEntity *entity ) {',
                    'void QDECL AddRefSpriteToScene( const mohaa::RefEntity *entity ) {\n'
                    'if (entity && (entity->renderfx & 0x80)) return;'), False),
                ('old-entity-filter', scene.replace(
                    '(void)parentEntityNumber;',
                    '(void)parentEntityNumber;\nif (entity && (entity->renderfx & 0x80)) return;'), False),
            ):
                cpp, exe = folder / f'{name}.cpp', folder / f'{name}.exe'
                cpp.write_text(host + model + body + checks)
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT),
                                        str(cpp), '-o', str(exe)], cwd=ROOT,
                                       capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
                if success:
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                else:
                    self.assertNotEqual(run.returncode, 0, f'{name} was not detected')


if __name__ == '__main__':
    unittest.main()
