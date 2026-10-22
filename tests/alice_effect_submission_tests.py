"""Retail Alice effects through the production adapters and sprite sorter.

All input records and geometry are synthetic; no retail data is required.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


class AliceEffectSubmissionTests(unittest.TestCase):
    def run_fixture(self, source, *, fast_math=False):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        scratch = ROOT / '.tmp/alice-effects/tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temp:
            cpp, exe = Path(temp) / 'fixture.cpp', Path(temp) / 'fixture.exe'
            cpp.write_text(source)
            optimize = ['-O2', '-ffast-math'] if fast_math else []
            build = subprocess.run([compiler, *target, *optimize, '-std=c++17', '-fno-operator-names', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], cwd=ROOT,
                                   capture_output=True, text=True, timeout=90)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_hidden_carrier_effects_keep_their_render_data(self):
        text = (ROOT / 'code/client/client_alice_cgame.hpp').read_text()
        scene = text[text.index('int CL_AliceRenderFx('):text.index('/* Slot 63. */')]
        self.run_fixture(r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include "code/compat/alice_render_abi.h"
#include <array>
#include <cassert>
#include <cstring>
#include <vector>
struct Drop {};
void QDECL Com_Error(errorParm_t, const char *, ...) { throw Drop{}; }
void QDECL Com_DPrintf(const char *, ...) {}
std::array<fntech3_alice_refEntity_t, FNTECH3_ALICE_MAX_RENDER_ENTITIES> aliceSceneEntities;
int aliceNumSceneEntities;
std::vector<refEntity_t> drawn;
void Submit(const refEntity_t *e, qboolean view) { assert(!view); drawn.push_back(*e); }
struct { decltype(&Submit) AddRefEntityToScene; } re{Submit};
''' + scene + r'''
int main() {
    fntech3_alice_refEntity_t e{};
    e.hModel=19; e.customShader=42; e.entityNumber=7;
    e.origin[0]=10; e.origin[1]=-20; e.origin[2]=30;
    e.axis[0][1]=1; e.axis[1][0]=-1; e.axis[2][2]=1;
    e.shaderRGBA[0]=180; e.shaderRGBA[1]=90; e.shaderRGBA[2]=60; e.shaderRGBA[3]=37;
    e.shaderTime=4.5f; e.frame=7; e.anim=3; e.scale=.05f;
    e.shader_data[0]=123; e.surfaces[1]=4;
    for (int flags : {0, 0x10, 0x80, 0x90, 0x94, 0x20090}) {
        e.renderfx=flags;
        for (int sprite=0; sprite<2; ++sprite) {
            drawn.clear(); aliceNumSceneEntities=0;
            if (sprite) CL_AliceAddRefSpriteToScene(&e);
            else CL_AliceAddRefEntityToScene(&e);
            assert(drawn.size()==1);
            const auto &out=drawn.back();
            assert(out.hModel==19 && out.alice.renderfx==flags);
            assert(out.reType==(sprite ? RT_ALICE_SPRITE : RT_MODEL));
            assert(out.customShader==(sprite ? 0 : 42));
            assert(!memcmp(e.origin, out.origin, sizeof(e.origin)));
            assert(!memcmp(e.axis, out.axis, sizeof(e.axis)));
            assert(!memcmp(e.shaderRGBA, out.shader.rgba, 4));
            assert(out.shaderTime.f==4.5f && out.frame==7 && out.alice.scale==.05f);
            assert(!(out.renderfx & RF_LIGHTING_ORIGIN)); // 0x80 is not Q3's flag
            assert(bool(out.renderfx & RF_DEPTHHACK)==bool(flags & 4));
            if (!sprite) {
                assert(out.alice.anim==3 && out.alice.shaderData[0]==123 && out.alice.surfaces[1]==4);
                assert(aliceNumSceneEntities==1 && aliceSceneEntities[0].entityNumber==7);
            }
        }
    }
    e.scale=0; drawn.clear(); CL_AliceAddRefSpriteToScene(&e);
    assert(drawn.size()==1 && drawn[0].alice.scale==0);
    e.scale=-.25f; CL_AliceAddRefSpriteToScene(&e);
    assert(drawn.back().alice.scale==-.25f);
    e.reType=FNTECH3_ALICE_RT_SPRITE; e.scale=0;
    CL_AliceAddRefEntityToScene(&e);
    assert(drawn.back().reType==RT_ALICE_SPRITE && drawn.back().alice.scale==0);
    drawn.clear(); CL_AliceAddRefSpriteToScene(nullptr); CL_AliceAddRefEntityToScene(nullptr);
    assert(drawn.empty());
    for (int type : {-1,1,(int)FNTECH3_ALICE_RT_MAX}) {
        e.reType=type; bool dropped=false;
        try { CL_AliceAddRefEntityToScene(&e); } catch (Drop) { dropped=true; }
        assert(dropped && drawn.empty());
    }
    e.reType=0; aliceNumSceneEntities=FNTECH3_ALICE_MAX_RENDER_ENTITIES;
    CL_AliceAddRefEntityToScene(&e); assert(drawn.empty());
    // A full model registry must not discard independent particle submissions.
    CL_AliceAddRefSpriteToScene(&e); assert(drawn.size()==1);
}
''')

    def test_smoke_layers_sort_by_distance_without_changing_dispatch(self):
        source = r'''
#include "code/renderer/tr_local.h"
#include <cassert>
#include <initializer_list>
trGlobals_t tr;
refimport_t ri;
#include "code/renderercommon/tr_alice_sprite_sort.h"
static surfaceType_t surface=SF_ENTITY, world=SF_FACE;
static unsigned Key(unsigned entity, unsigned shader=3) {
    return (shader<<QSORT_SHADERNUM_SHIFT) | (entity<<QSORT_REFENTITYNUM_SHIFT);
}
static void SetFloatBits(float &out, uint32_t bits) {
    // Model bytes arriving from a module; fast-math may discard NaN literals.
    volatile uint32_t raw=bits;
    uint32_t value=raw;
    memcpy(&out,&value,sizeof(out));
}
int main() {
    trRefEntity_t entities[6]{};
    tr.refdef.entities=entities; tr.refdef.num_entities=6;
    for (auto &e : entities) e.e.reType=RT_ALICE_SPRITE;
    entities[0].e.origin[0]=10; entities[1].e.origin[0]=30; entities[2].e.origin[0]=20;
    entities[3].e.origin[0]=100; entities[4].e.origin[0]=200;
    SetFloatBits(entities[5].e.origin[0],0x7fc00000u);
    drawSurf_t input[]={{Key(0),&surface,0}, {Key(1),&surface,DSF_SHADOW_CASTER_ONLY},
        {Key(2),&surface,0}, {Key(3,4),&surface,0}, {Key(4,4),&world,0}, {Key(5,4),&surface,0}};
    for (int profile=0; profile<COM_GAME_COUNT; ++profile) {
        if (profile==COM_GAME_ALICE) continue;
        ri.gameId=(comGameId_t)profile;
        drawSurf_t copy[6]; memcpy(copy,input,sizeof(input));
        R_SortAliceSprites(copy,6); assert(!memcmp(copy,input,sizeof(input)));
    }
    ri.gameId=COM_GAME_ALICE; R_SortAliceSprites(input,6);
    assert(input[0].sort==Key(1) && input[0].flags==DSF_SHADOW_CASTER_ONLY);
    assert(input[1].sort==Key(2) && input[2].sort==Key(0));
    assert(input[3].sort==Key(3,4) && input[4].surface==&world && input[5].sort==Key(5,4));
    // Changing the viewpoint reverses which end of the plume is farthest.
    tr.viewParms.or.origin[0]=40; R_SortAliceSprites(input,3);
    assert(input[0].sort==Key(0) && input[1].sort==Key(2) && input[2].sort==Key(1));
    entities[0].e.origin[0]=entities[1].e.origin[0]=entities[2].e.origin[0]=10;
    R_SortAliceSprites(input,3);
    assert(input[0].sort==Key(0) && input[1].sort==Key(1) && input[2].sort==Key(2));
    drawSurf_t invalid[]={{Key(REFENTITYNUM_WORLD),&surface,0}, {Key(0),nullptr,0}};
    R_SortAliceSprites(invalid,2); R_SortAliceSprites(nullptr,0);
    assert(invalid[0].sort==Key(REFENTITYNUM_WORLD) && invalid[1].surface==nullptr);
    drawSurf_t nan[]={{Key(5),&surface,0}, {Key(0),&surface,0}};
    R_SortAliceSprites(nan,2); assert(nan[0].sort==Key(0));
    SetFloatBits(entities[5].e.origin[0],0x7f800000u);
    R_SortAliceSprites(nan,2); assert(nan[0].sort==Key(0));
    SetFloatBits(tr.viewParms.or.origin[0],0x7fc00000u);
    R_SortAliceSprites(input,3);
    assert(input[0].sort==Key(0) && input[1].sort==Key(1) && input[2].sort==Key(2));
}
'''
        self.run_fixture(source)
        self.run_fixture(source, fast_math=True)

    def test_sprite_geometry_preserves_scale_and_motion(self):
        sprite = function((ROOT / 'code/renderer/tr_alice.c').read_text(),
                          'void RB_SurfaceAliceSprite(')
        for renderer in ('renderervk', 'rendererrtx'):
            self.assertEqual(sprite, function((ROOT / 'code' / renderer / 'tr_alice.c').read_text(),
                                             'void RB_SurfaceAliceSprite('))
        self.run_fixture(r'''
#include "code/renderer/tr_local.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
backEndState_t backEnd;
refimport_t ri;
model_t model;
vec3_t drawnOrigin, drawnLeft, drawnUp;
color4ub_t drawnColor;
model_t *R_GetModelByHandle(qhandle_t) { return &model; }
vec_t VectorNormalize(vec3_t v) {
    float length=std::sqrt(DotProduct(v,v));
    if (length) for (int i=0;i<3;++i) v[i]/=length;
    return length;
}
void RB_AddQuadStamp(const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color) {
    VectorCopy(origin,drawnOrigin); VectorCopy(left,drawnLeft); VectorCopy(up,drawnUp);
    drawnColor=color;
}
''' + sprite + r'''
int main() {
    trRefEntity_t entity{};
    shader_t shader{};
    aliceSprite_t sprite{};
    model.type=MOD_ALICE_SPRITE; model.aliceSprite=&sprite;
    sprite.shader=&shader; sprite.originX=32; sprite.originY=32;
    shader.aliceSpriteScale=1;
    shader.aliceSpriteGen=ALICE_SPRITEGEN_PARALLEL;
    backEnd.currentEntity=&entity;
    backEnd.viewParms.or.axis[1][1]=1; backEnd.viewParms.or.axis[2][2]=1;
    entity.e.shader.rgba[3]=37;
    for (int game : {COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA}) {
        ri.gameId=(comGameId_t)game;
        for (float scale : {.05f, 0.0f, -.25f}) {
            entity.e.alice.scale=entity.e.mohaa.scale=scale;
            for (float z : {100.0f, 106.0f}) {
                VectorSet(entity.e.origin,10,20,z);
                RB_SurfaceAliceSprite();
                assert(!memcmp(drawnOrigin,entity.e.origin,sizeof(vec3_t)));
                assert(drawnLeft[1]==32*scale && drawnUp[2]==32*scale);
                assert(drawnLeft[0]==0 && drawnUp[0]==0 && drawnColor.rgba[3]==37);
            }
        }
    }
}
''')

    def test_all_renderers_share_sprite_sort_and_preserve_zero_scale(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            main = (ROOT / 'code' / renderer / 'tr_main.c').read_text()
            self.assertIn('#include "../renderercommon/tr_alice_sprite_sort.h"', main)
            self.assertIn('R_RadixSort( drawSurfs, numDrawSurfs );\n'
                          '\tR_SortAliceSprites( drawSurfs, numDrawSurfs );', main)
            sprite = function((ROOT / 'code' / renderer / 'tr_alice.c').read_text(),
                              'void RB_SurfaceAliceSprite(')
            self.assertIn('scale *= ent->e.alice.scale;', sprite)
            self.assertNotIn('ent->e.alice.scale >', sprite)


if __name__ == '__main__':
    unittest.main()
