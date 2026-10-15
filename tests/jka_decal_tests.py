"""Academy's effects Decal primitive and its world decal pool: CreateEffect's
Decal case and the renderer pool it calls in retail jamp.exe 1.01, served
through Fn's mark clipper and polygon path (docs/fntech3/2026-10-15-jka-decal.md)."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import engine_fixture_args
from jka_effect_play_tests import fixture as playback_fixture

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / 'docs/fntech3/2026-10-15-jka-decal.md'
COMPANION = ROOT / 'docs/fntech3/2026-10-15-jka-decal.json'

KERNEL = r'''
#include "code/compat/jka_decals.hpp"
#include "code/compat/jka_effect_play.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
namespace decals=fntech3::jka::decals;
namespace play=fntech3::jka::effectplay;
namespace fx=fntech3::jka::effects;
static bool Near(float a,float b){return std::fabs(a-b)<1e-5f;}
static bool Near(const decals::Vector &v,float x,float y,float z){return Near(v[0],x)&&Near(v[1],y)&&Near(v[2],z);}
static bool Compile(const std::string &text,play::Spec &spec,play::Failure &failure,int index=0){
 fx::Definition definition;fx::Failure parse;
 if(!fx::Parse(text,definition,parse))return false;
 auto primitive=definition.primitives.at(index);
 int handle=11;for(auto &media:primitive.media)media.handle=handle++;
 return play::Compile(primitive,spec,failure);
}
static std::string Decal(const std::string &body){return "Decal\n{\n"+body+"shaders\n[\ngfx/own/burn\n]\n}\n";}
int main(){
 // Retail's constants.
 static_assert(decals::MaxPolys==500&&decals::MaxVerts==10&&decals::MaxFragments==128&&decals::MaxPoints==384);
 static_assert(decals::FadeTime==1000&&decals::Projection==-20.0f&&play::flag::Ghoul2Decals==0x40000u);
 assert(std::string(decals::CountName)=="r_markcount"&&std::string(decals::CountDefault)=="100");
 assert(decals::Limit(-5)==0&&decals::Limit(0)==0&&decals::Limit(100)==100&&decals::Limit(500)==500&&decals::Limit(100000)==500);
 // The square: the normal, a perpendicular along the world axis it leans
 // on least, turned by the orientation, and the cross product.
 decals::Square square;
 assert(decals::Press({10,20,1},{0,0,1},0,4,square));
 assert(Near(square.axis[0],0,0,1)&&Near(square.axis[1],0,1,0)&&Near(square.axis[2],1,0,0));
 assert(Near(square.corners[0],6,16,1)&&Near(square.corners[1],6,24,1)&&Near(square.corners[2],14,24,1)&&Near(square.corners[3],14,16,1));
 assert(Near(square.projection,0,0,-20)&&square.scale==0.125f);
 assert(decals::Press({0,0,0},{0,0,1},90,4,square));
 assert(Near(square.axis[2],0,1,0)&&Near(square.axis[1],-1,0,0));
 // The projection scales the caller's direction, unnormalized.
 assert(decals::Press({0,0,0},{0,0,2},0,8,square)&&Near(square.projection,0,0,-40)&&Near(square.axis[0],0,0,1));
 assert(square.scale==0.0625f);
 assert(decals::Press({0,0,0},{3,0,0},0,1,square)&&Near(square.axis[0],1,0,0)&&Near(square.axis[1],0,0,1)&&Near(square.axis[2],0,1,0));
 // Fn does not press a direction without length, a radius at or below
 // zero (retail's fatal error) or beyond the Fn bound, or values that are
 // not finite.
 const decals::Square kept=square;
 const std::uint32_t nanBits=0x7fc00001u,infBits=0x7f800000u;float nan,inf;std::memcpy(&nan,&nanBits,4);std::memcpy(&inf,&infBits,4);
 assert(!decals::Press({0,0,0},{0,0,0},0,4,square)&&!decals::Press({0,0,0},{0,0,1},0,0,square));
 assert(!decals::Press({0,0,0},{0,0,1},0,-1,square)&&!decals::Press({0,0,0},{0,0,1},0,20000,square));
 assert(!decals::Press({0,0,0},{0,0,1},inf,4,square)&&!decals::Press({0,0,0},{0,0,1},40000,4,square));
 assert(!decals::Press({nan,0,0},{0,0,1},0,4,square)&&!decals::Press({0,0,0},{0,nan,1},0,4,square));
 assert(!decals::Press({0,0,0},{0,0,1},0,nan,square)&&!decals::Press({2e7f,0,0},{0,0,1},0,4,square));
 assert(Near(square.projection,kept.projection[0],kept.projection[1],kept.projection[2]));
 // Texture coordinates centre the square at one half.
 assert(decals::Press({10,20,1},{0,0,1},0,4,square));
 const std::uint8_t white[4]{255,255,255,255};decals::Vertex vertex;
 assert(decals::Place(square,{10,20,1},{6,16,0},white,vertex)&&vertex.st[0]==0&&vertex.st[1]==0&&vertex.xyz[2]==0);
 assert(decals::Place(square,{10,20,1},{12,20,0},white,vertex)&&vertex.st[0]==0.5f&&vertex.st[1]==0.75f&&vertex.rgba[3]==255);
 assert(!decals::Place(square,{10,20,1},{nan,0,0},white,vertex));
 // Colour bytes truncate; the fade truncates (1 - t/1000) * 255.
 assert(decals::Byte(1)==255&&decals::Byte(.5f)==127&&decals::Byte(0)==0&&decals::Byte(-1)==0&&decals::Byte(2)==255&&decals::Byte(nan)==0);
 assert(decals::FadeAlpha(0)==255&&decals::FadeAlpha(500)==127&&decals::FadeAlpha(999)==0&&decals::FadeAlpha(1000)==0);
 assert(decals::FadeAlpha(-50)==255);
 // The pool. Three records at time 100 fill a count of three.
 static decals::Pool pool;pool.Clear();
 for(int n=0;n<3;++n){auto &decal=pool.Allocate(decals::Normal,100,3);decal.shader=7+n;decal.numVerts=3;}
 assert(pool.Total(decals::Normal)==3&&pool.Head(decals::Normal)==0&&pool.Total(decals::Fade)==0);
 // A fourth in a later scene pushes out the oldest and every record made
 // in the same scene as it; they fade from now.
 {auto &decal=pool.Allocate(decals::Normal,200,3);decal.shader=20;decal.numVerts=3;}
 assert(pool.Total(decals::Normal)==1&&pool.Total(decals::Fade)==3&&pool.Head(decals::Normal)==1);
 // The walk frees the records after the head first, then the head.
 for(int n=0;n<3;++n){const auto &fade=pool.At(decals::Fade,n);assert(fade.time==200&&fade.fadeTime==1200&&fade.shader==(n<2?8+n:7));}
 int drawn=0,alpha=-1;const auto visit=[&](int now){drawn=0;alpha=-1;pool.Visit(now,3,[&](const decals::Decal &decal){
  ++drawn;if(decal.fadeTime)alpha=decal.verts[0].rgba[3];});};
 visit(200);assert(drawn==4&&alpha==255);
 visit(700);assert(drawn==4&&alpha==127);
 visit(1200);assert(drawn==1&&alpha==-1&&pool.Total(decals::Fade)==0);
 // A record made in the current scene is pushed out alone.
 pool.Clear();for(int n=0;n<3;++n)pool.Allocate(decals::Normal,300,2);
 assert(pool.Total(decals::Normal)==2&&pool.Total(decals::Fade)==1);
 // A changed count clears every record; a count of zero draws nothing.
 visit(300);assert(drawn==3);
 drawn=0;pool.Visit(300,2,[&](const decals::Decal &){++drawn;});assert(drawn==0&&pool.Total(decals::Normal)==0);
 pool.Allocate(decals::Normal,400,2);drawn=0;pool.Visit(400,0,[&](const decals::Decal &){++drawn;});assert(drawn==0);
 // A pool over its count is cleared whole before the next record.
 pool.Clear();for(int n=0;n<5;++n)pool.Allocate(decals::Normal,500+n,5);
 assert(pool.Total(decals::Normal)==5);pool.Allocate(decals::Normal,600,3);
 assert(pool.Total(decals::Normal)==1&&pool.Total(decals::Fade)==0);
 // A record of time zero is free: retail's first scene time.
 pool.Clear();pool.Visit(0,3,[](const decals::Decal &){});pool.Allocate(decals::Normal,0,3);
 drawn=0;pool.Visit(0,3,[&](const decals::Decal &){++drawn;});assert(drawn==0&&pool.Total(decals::Normal)==1);
 // The Decal primitive. An independently written wall mark: count, size
 // and rotation ranges, the rgb and alpha starts, a shader list.
 play::Spec spec;play::Failure failure;
 assert(Compile(Decal("count 1 2\nrotation 0 360\nrgb\n{\nstart .5 .25 1\n}\nalpha\n{\nstart .5\n}\nsize\n{\nstart 5.5 7\n}\n"),spec,failure));
 assert(spec.kind==fx::Kind::Decal&&spec.shader==11&&spec.shaders.size()==1&&spec.size.first==5.5f&&spec.size.last==7);
 assert(spec.rotation.last==360&&spec.count.last==2&&spec.color[0].first==.5f&&spec.color[3].first==.5f);
 std::uint32_t random=1;play::Spawn spawn;const play::Axis axis{{{0,0,1},{0,1,0},{-1,0,0}}};
 assert(play::Instantiate(spec,{1,2,3},axis,random,spawn));
 assert(spawn.kind==fx::Kind::Decal&&spawn.size>=5.5f&&spawn.size<=7&&spawn.rotation>=0&&spawn.rotation<=360);
 assert(spawn.rgbStart[0]==.5f&&spawn.rgbStart[1]==.25f&&spawn.rgbStart[2]==1&&spawn.alphaStart==.5f);
 assert((spawn.axis[0]==play::Vector{0,0,1})&&(spawn.origin==play::Vector{1,2,3}));
 // Fields the Decal case never reads are inert: life, velocity, the curve
 // ends, parms and words, size2 and length, unknown flag words, cullrange.
 assert(Compile(Decal("life 1e+004 2e+004\ncullrange 1200\nvelocity 1 2 3\nflags usePhysics someWord\n"
  "size\n{\nstart 3\nend 9\nparm 50\nflags linear wave\n}\nalpha\n{\nstart 1\nend 0\n}\nsize2\n{\nstart 2\n}\nlength\n{\nstart 4\n}\n"),spec,failure));
 assert(spec.size.first==3&&spec.unknownWords.size()==1);
 // A list of shaders is picked per spawn.
 assert(Compile("Decal\n{\nshaders\n[\ngfx/a\ngfx/b\n]\n}\n",spec,failure)&&spec.shaders.size()==2&&spec.size.first==1);
 // Named refusals: ghoul2Decals (cgame export 17 is not served), a size at
 // or below zero (retail's fatal error), media other than shaders, no
 // shader, and an unknown curve word.
 assert(!Compile(Decal("flags ghoul2Decals\n"),spec,failure)&&std::string(failure.reason)=="effects ghoul2Decals is not served yet");
 assert(!Compile(Decal("size\n{\nstart 0 4\n}\n"),spec,failure)&&std::string(failure.reason)=="effects decal size must be positive");
 assert(!Compile(Decal("size\n{\nstart -2\n}\n"),spec,failure));
 assert(!Compile("Decal\n{\nshaders\n[\ngfx/a\n]\nsounds\n[\nsound/a.wav\n]\n}\n",spec,failure));
 assert(!Compile("Decal\n{\nsize\n{\nstart 4\n}\n}\n",spec,failure)&&std::string(failure.reason)=="effects playback requires registered Fn shaders");
 assert(!Compile(Decal("size\n{\nstart 4\nflags sideways\n}\n"),spec,failure)&&std::string(failure.reason)=="unknown effects curve flag word");
}
'''

FAKE_MARKS = r'''
// A floor at z = 0: the square's corners dropped onto it, as one fragment,
// or a test's own fragment list.
static int markCalls,markMode;static std::array<std::array<float,3>,4> markCorners;static std::array<float,3> markProjection;
static int MarkEffectFragments(int count,const vec3_t *points,const vec3_t projection,int maxPoints,vec3_t buffer,int maxFragments,markFragment_t *fragments){
 assert(count==4&&maxPoints==384&&maxFragments==128);++markCalls;
 for(int n=0;n<4;++n)for(int c=0;c<3;++c)markCorners[n][c]=points[n][c];
 for(int c=0;c<3;++c)markProjection[c]=projection[c];
 if(markMode==1)return 0;
 if(markMode==2)return 129;
 if(markMode==3){fragments[0].firstPoint=380;fragments[0].numPoints=10;return 1;}
 const int total=markMode==4?12:4;
 // The point buffer is a run of vectors handed over as its first one.
 for(int n=0;n<total;++n){float *point=buffer+3*n;point[0]=points[n%4][0]+n/4;point[1]=points[n%4][1];point[2]=0;}
 fragments[0].firstPoint=0;fragments[0].numPoints=total;return 1;
}
'''

MAIN = r'''
using Kind=fntech3::jka::effects::Kind;
namespace decals=fntech3::jka::decals;
static std::array<byte,14> originBytes,forwardBytes;
static intptr_t Play(intptr_t id,float x,float y,float z,float fx,float fy,float fz){
 const float origin[]{x,y,z},forward[]{fx,fy,fz};
 std::memcpy(originBytes.data()+1,origin,12);std::memcpy(forwardBytes.data()+1,forward,12);
 intptr_t args[]{266,id,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(forwardBytes.data()+1),-1,-1};
 return InvokePlayback(args);
}
static int Kept(){return jkaCgameDecals?jkaCgameDecals->pool.Total(decals::Normal):0;}
int main(){
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);vm.index=VM_CGAME;cgvm=&vm;
 if(sizeof(void*)!=4)return 0;
 Reset();fntech3_jka_mp_refdef_t view{};view.width=320;view.height=240;view.fov_x=90;view.fov_y=75;
 assert(System(Call::CG_FX_INIT_SYSTEM,reinterpret_cast<intptr_t>(&view))==1);
 fxVariables[2].value=16;fxVariables[4].integer=100;Frame(1000);
 originBytes.fill(0xa5);forwardBytes.fill(0xa5);
 // An independently written wall impact: a sound and a coloured mark.
 assets["effects/own/impact.efx"]="Sound\n{\nsound sound/own/hit.wav\n}\n"
  "Decal\n{\nrgb\n{\nstart .5 .25 1\n}\nalpha\n{\nstart .5\n}\nsize\n{\nstart 4\n}\nshaders\n[\ngfx/own/burn\n]\n}\n";
 assets["effects/own/marked.efx"]="Decal\n{\nflags ghoul2Decals\nsize\n{\nstart 4\n}\nshaders\n[\ngfx/own/burn\n]\n}\n";
 const auto impact=Register("own/impact"),marked=Register("own/marked");assert(impact>0&&marked>0);
 const qhandle_t burn=static_cast<qhandle_t>(100+(std::find(shaders.begin(),shaders.end(),"gfx/own/burn")-shaders.begin())+1);
 // Retail's decals take the last world scene's time; one has rendered.
 CL_JKACgameDecalsDraw(900);assert(jkaCgameDecals->count==&fxVariables[4]&&polygons==0);
 assert(Play(impact,10,20,1,0,0,1)==0&&Active()==0&&effectSounds.size()==1&&markCalls==1&&Kept()==1);
 // The square about the spawn origin, pressed 20 units along -direction.
 assert((markCorners[0]==std::array<float,3>{6,16,1})&&(markCorners[2]==std::array<float,3>{14,24,1}));
 assert((markProjection==std::array<float,3>{0,0,-20}));
 const auto &kept=jkaCgameDecals->pool.At(decals::Normal,0);
 assert(kept.time==900&&kept.shader==burn&&kept.numVerts==4&&kept.fadeTime==0&&kept.color[0]==.5f&&kept.color[3]==.5f);
 // The next world scene draws it through Fn's polygon path.
 vertices.clear();CL_JKACgameDecalsDraw(1000);assert(polygons==1&&vertices.size()==4);
 assert(vertices[0].xyz[0]==6&&vertices[0].xyz[1]==16&&vertices[0].xyz[2]==0&&vertices[0].st[0]==0&&vertices[0].st[1]==0);
 assert(vertices[2].st[0]==1&&vertices[2].st[1]==1);
 for(const auto &vertex:vertices)assert(vertex.modulate.rgba[0]==127&&vertex.modulate.rgba[1]==63&&vertex.modulate.rgba[2]==255&&vertex.modulate.rgba[3]==127);
 // The effects pass draws nothing for it: a Decal makes no instance.
 Draw();assert(polygons==1);
 // A fragment of more than ten points keeps its first ten.
 markMode=4;assert(Play(impact,0,0,1,0,0,1)==0&&Kept()==2&&jkaCgameDecals->pool.At(decals::Normal,1).numVerts==10);
 vertices.clear();polygons=0;CL_JKACgameDecalsDraw(1100);assert(polygons==2&&vertices.size()==14);
 // No fragment keeps nothing; a direction without length presses nothing.
 markMode=1;assert(Play(impact,0,0,1,0,0,1)==0&&Kept()==2);markMode=0;
 {const int calls=markCalls;float zero[9]{};std::array<byte,38> axisBytes{};std::memcpy(axisBytes.data()+1,zero,36);
  intptr_t args[]{268,impact,reinterpret_cast<intptr_t>(originBytes.data()+1),reinterpret_cast<intptr_t>(axisBytes.data()+1),-1,-1,-1,-1};
  assert(InvokePlayback(args)==0&&markCalls==calls&&Kept()==2);}
 // A count of two pushes the oldest record out; it fades over a second.
 fxVariables[4].integer=2;CL_JKACgameDecalsDraw(1200);assert(Kept()==0); // A changed count clears.
 assert(Play(impact,0,0,1,0,0,1)==0&&Play(impact,0,0,1,0,0,1)==0&&Kept()==2);
 CL_JKACgameDecalsDraw(1300);assert(Play(impact,0,0,1,0,0,1)==0&&Kept()==1&&jkaCgameDecals->pool.Total(decals::Fade)==2);
 vertices.clear();polygons=0;CL_JKACgameDecalsDraw(1550);assert(polygons==3&&vertices.size()==12);
 for(std::size_t n=0;n<vertices.size();++n)assert(vertices[n].modulate.rgba[3]==(n<4?127:191));
 polygons=0;CL_JKACgameDecalsDraw(2300);assert(polygons==1&&jkaCgameDecals->pool.Total(decals::Fade)==0);
 // r_markcount 0 keeps nothing and draws nothing.
 fxVariables[4].integer=0;const int calls=markCalls;assert(Play(impact,0,0,1,0,0,1)==0&&markCalls==calls);
 polygons=0;CL_JKACgameDecalsDraw(2400);assert(polygons==0);fxVariables[4].integer=100;
 // The clipper's answers are checked.
 for(int mode:{2,3}){markMode=mode;Reject([&]{Play(impact,0,0,1,0,0,1);});}markMode=0;
 // ghoul2Decals refuses by name before anything plays.
 {const int calls=markCalls;Reject([&]{Play(marked,0,0,1,0,0,1);});assert(markCalls==calls);}
 // Import 201's clear and the client game's shutdown empty the pool.
 assert(Play(impact,0,0,1,0,0,1)==0&&Kept()==1);CL_JKACgameDecalsClear();assert(!jkaCgameDecals);
 CL_JKACgameDecalsDraw(2500);assert(Play(impact,0,0,1,0,0,1)==0&&Kept()==1);
 CL_JKACgameEffectsClear();assert(!jkaCgameDecals&&!jkaCgameEffects);
}
'''


def host_fixture():
    prefix = playback_fixture().split('int main(){')[0]
    old_marks = ('static int MarkEffectFragments(int,const vec3_t *,const vec3_t,int,vec3_t,int,markFragment_t *)'
                 '{assert(false);return 0;}')
    assert prefix.count(old_marks) == 1
    prefix = prefix.replace(old_marks, FAKE_MARKS)
    old_polys = 'assert(shader>=0&&count==4&&instances>0);'
    assert prefix.count(old_polys) == 1
    prefix = prefix.replace(old_polys, 'assert(shader>=0&&count>=3&&count<=10&&instances==1);')
    return '#include <algorithm>\n' + prefix + MAIN


class JKADecalTests(unittest.TestCase):
    def compile_and_run(self, body, name):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp/tests/jka-decal'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder)
            source = directory / f'{name}.cpp'
            source.write_text(body)
            for fast in (False, True):
                with self.subTest(fast_math=fast):
                    executable = directory / (('fast' if fast else 'plain') + ('.exe' if os.name == 'nt' else ''))
                    target = engine_fixture_args(compiler)
                    # As in jka_camera_shake_tests: the effects pool's reset
                    # can sit in the import dispatcher's frame under clang.
                    stack = []
                    if os.name == 'nt' and name == 'host':
                        stack = (['-Wl,--stack,8388608'] if Path(compiler).stem.lower() == 'g++'
                                 else ['-Wl,/stack:8388608'])
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-Werror',
                        '-D_CRT_SECURE_NO_WARNINGS', *(['-ffast-math'] if fast else []), '-I', str(ROOT),
                        str(source), '-o', str(executable), *stack], cwd=ROOT, capture_output=True, text=True,
                        timeout=120)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_decal_kernel_pool_and_compile(self):
        self.compile_and_run(KERNEL, 'kernel')

    def test_effects_decal_presses_into_the_pool_and_draws(self):
        self.compile_and_run(host_fixture(), 'host')

    def test_hooks_and_record(self):
        host = (ROOT / 'code/client/client_jka_cgame_host.hpp').read_text(encoding='utf-8')
        # Import 201 clears the pool; every world scene draws it.
        clear = host[host.index('    case Call::CG_R_CLEARDECALS:'):host.index('    case Call::CG_R_ADDREFENTITYTOSCENE:')]
        self.assertIn('CL_JKACgameDecalsClear();', clear)
        scene = host[host.index('    case Call::CG_R_RENDERSCENE:'):host.index('    case Call::CG_R_ADDLIGHTTOSCENE:')]
        self.assertIn('if (!(value.rdflags & RDF_NOWORLDMODEL)) CL_JKACgameDecalsDraw(value.time);', scene)
        # Import 205 is still not served: no case reaches the pool from it.
        self.assertNotIn('case Call::CG_R_ADDDECALTOSCENE', host)
        # r_markcount is registered by the Academy pool alone.
        users = sorted(path.relative_to(ROOT).as_posix() for path in (ROOT / 'code').rglob('*')
                       if path.suffix in {'.c', '.cpp', '.h', '.hpp'} and 'r_markcount' in path.read_text(
                           encoding='utf-8', errors='replace'))
        self.assertEqual(users, ['code/client/client_jka_cgame_decals.hpp', 'code/compat/jka_decals.hpp'])
        if RECORD.exists():
            data = json.loads(COMPANION.read_text(encoding='utf-8'))
            self.assertEqual(data['retail']['decalCase'], '0x40ac7e')
            self.assertEqual(data['retail']['addDecalToScene'], '0x4ab6c0')
            self.assertIn('2026-10-15-jka-decal.json', RECORD.read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
