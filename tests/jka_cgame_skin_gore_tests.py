"""Serve Jedi Academy client-game imports 313 (CG_G2_GETNUMGOREMARKS) and 314
(CG_G2_ADDSKINGORE), and let 315 clear what 314 adds.

Retail jamp.exe 1.01 serves 314 at 0x413cb6 through 0x4e08f0: it poses the
model, carries the hit into model space and, for each LOD from max(bias,
r_lodbias) to min(LODs, 3), projects a splotch onto every surface that is not
switched off (0x4ea460), filing one mark per surface per call in the model's
gore set. 313 (0x413c9d, 0x4e07a0) answers that set's size; the renderer
drops marks whose delete time has come when it draws their surface. The
fixture compiles the production Ghoul2, gore and render headers (optionally
mutated) into the client-game render fixture; every mutation must fail.
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
GORE = 'code/client/client_jka_cgame_gore.hpp'
RENDER = 'code/client/client_jka_cgame_render.hpp'
COUNT = 'case Call::CG_G2_GETNUMGOREMARKS: {'
ADD = 'case Call::CG_G2_ADDSKINGORE: {'
CLEAR = 'case Call::CG_G2_CLEARSKINGORE: {'


def inline_header(text):
    text = text.replace('#include "../', '#include "code/')
    return text.replace('#include "client_', '#include "code/client/client_')


def block(text, opening, closing):
    start = text.index(opening)
    return start, text.index(closing, start) + len(closing)


def within(opening, closing, old, new):
    def mutate(text):
        start, end = block(text, opening, closing)
        assert old in text[start:end], old
        return text[:start] + text[start:end].replace(old, new, 1) + text[end:]
    return mutate


def anywhere(old, new):
    def mutate(text):
        assert old in text, old
        return text.replace(old, new, 1)
    return mutate


HEADER_MUTATIONS = {
    'count without native gate': within(COUNT, '\n    }\n', '        CL_JKACgameGhoul2Native();\n', ''),
    'count without place bound': within(COUNT, '\n    }\n', ' || args[2] >= instance->Count()', ''),
    'count of model 0 always': within(COUNT, '\n    }\n', 'static_cast<int>(args[2]))', '0)'),
    'add without native gate': within(ADD, '\n    }\n', '        CL_JKACgameGhoul2Native();\n', ''),
    'short shot traced': within(ADD, '\n    }\n', ' < 0.1f) return true;', ' < 0.0f) return true;'),
    'identity before the shot test': within(ADD, '\n    }\n',
        '        if (!args[2]) Com_Error', '        CL_JKACgameGhoul2Resolve(static_cast<std::uint32_t>(args[1]));\n'
        '        if (!args[2]) Com_Error'),
    'no finite check': within(ADD, '\n    }\n', '            if (!fntech3::ghoul2::pose::Finite(value))', '            if (false)'),
    'no frame clock check': within(ADD, '\n    }\n', '        if (!jkaCgameBoneClocks.FrameReady())\n', '        if (false)\n'),
    'lod bias ignored': within(ADD, '\n    }\n', 'Cvar_VariableIntegerValue("r_lodbias")', '0'),
    'lod bias not capped at 2': within(ADD, '\n    }\n', '        first = (std::min)(first, 2);\n', ''),
    'lod cap of 3 dropped': within(ADD, '\n    }\n', '(std::min)(base->mesh.numLODs, 3)', 'base->mesh.numLODs'),
    'every model by default': within(ADD, '\n    }\n', '                if (!everyModel) break;\n', ''),
    'scale ignored': within(ADD, '\n    }\n', 'vertex.position[std::size_t(axis)] *= scale[axis];',
        'vertex.position[std::size_t(axis)] *= 1.0f;'),
    'zero scale kept': within(ADD, '\n    }\n', 'request.scale[axis] ? request.scale[axis] : 1.0f', 'request.scale[axis]'),
    'overrides ignored': within(ADD, '\n    }\n', '                    if (binding) {', '                    if (false) {'),
    'hit not carried into model space': within(ADD, '\n    }\n', 'gore::Through(inverse, request.hitLocation, true)',
        'gore::Vector{request.hitLocation[0], request.hitLocation[1], request.hitLocation[2]}'),
    'clear keeps the gore': within(CLEAR, '\n    }\n', '        if (jkaCgameGore) jkaCgameGore->Clear(identity);\n', ''),
    'duplicate shares no gore': anywhere('        if (jkaCgameGore) jkaCgameGore->Duplicate(source, destination);\n', ''),
    'copy carries no gore': anywhere('            if (jkaCgameGore) jkaCgameGore->CopyModel(from, static_cast<int>(args[2]), to, index);\n', ''),
    'clean keeps the gore': anywhere('            if (jkaCgameGore) jkaCgameGore->Clear(identity);\n', ''),
    'gore survives a client-game clear': anywhere('static void CL_JKACgameGoreClear() { jkaCgameGore.reset(); }',
        'static void CL_JKACgameGoreClear() {}'),
}

GORE_MUTATIONS = {
    'no record cap': anywhere('        while (records.size() > MaxRecords) {', '        while (false) {'),
    'cap evicts one record': anywhere(
        '            while (!records.empty() && (records.begin()->first & ~(TagGroup - 1)) == group)\n'
        '                records.erase(records.begin());\n', ''),
    'tags not regrouped per call': anywhere('        currentTag = currentUpper;\n', ''),
    'set deleted with references left': anywhere('        if (set.references && set.references - 1) { --set.references; return; }\n', ''),
    'duplicate without a reference': anywhere('            if (Set *set = FindSet(at->second)) ++set->references;\n', ''),
    'basis threshold': anywhere('if (Dot(first, first) < 0.1f)', 'if (Dot(first, first) < 0.0f)'),
    'theta ignored': anywhere('std::cos(double(trace.theta))', 'std::cos(0.0)'),
    's and t swapped': anywhere('(halfC / trace.tSize) + second[i] * (0.5f * s / trace.tSize)',
        '(halfC / trace.sSize) + second[i] * (0.5f * s / trace.sSize)'),
    'margin on the far edge': anywhere('if (sValue < 1.0f) inside |= 2;', 'if (sValue < 0.9f) inside |= 2;'),
    'triangle needs every corner': anywhere('if (15 & off[triangle[0]] & off[triangle[1]] & off[triangle[2]]) continue;',
        'if (15 & (off[triangle[0]] | off[triangle[1]] | off[triangle[2]])) continue;'),
    'faces culled inverted': anywhere('Dot(trace.direction, normal) > 0.0f ? trace.frontFaces : trace.backFaces',
        'Dot(trace.direction, normal) > 0.0f ? trace.backFaces : trace.frontFaces'),
    'a mark per LOD': anywhere('    if (found == state.pending.end()) {', '    if (true) {'),
    'delete time without lifetime check': anywhere(
        'mark.deleteTime = trace.lifeTime ? trace.now + trace.lifeTime : 0;', 'mark.deleteTime = trace.now + trace.lifeTime;'),
    'growth never ends': anywhere('mark.growEnd = trace.growDuration == -1 ? -1 : trace.now + trace.growDuration;',
        'mark.growEnd = -1;'),
    'off surface traced': anywhere('    if (!flags) Polys(state, record, surface, trace);',
        '    Polys(state, record, surface, trace);'),
    'descendants always walked': anywhere('    if (flags & 0x100) return;\n', ''),
    'expiry ignores the delete time': anywhere(
        '            if (at->second.deleteTime && now >= at->second.deleteTime) {', '            if (false) {'),
    'expired record erased': anywhere('                record->second = Record{};\n',
        '                state.records.erase(record);\n'),
    'missing record kept': anywhere(
        '            if (record == state.records.end()) { at = set->marks.erase(at); continue; }',
        '            if (record == state.records.end()) { ++at; continue; }'),
}

RENDER_MUTATIONS = {
    'personal models expire': anywhere('if (jkaCgameGore && !(source.renderfx & RF_THIRD_PERSON))', 'if (jkaCgameGore)'),
    'off surfaces expire': anywhere('        if (!flags) state.drawn.push_back(surfaceIndex);',
        '        state.drawn.push_back(surfaceIndex);'),
}

MAIN = r'''
namespace gore=fntech3::jka::cgame::gore;
using Request=gore::Request;
/* Three quads per LOD, one weight each: root (children child and far), child
 * beside it, far well away. Flags per surface; quads in the y/z plane. */
static Bytes GoreMesh(int lods,std::array<std::int32_t,3> flags,float childOffset=1.5f){
 namespace format=fntech3::jka::model;
 Bytes file;
 AppendInt(file,std::int32_t(format::MeshIdent));AppendInt(file,format::MeshVersion);
 AppendName(file,"models/players/gore/model.glm");AppendName(file,"models/players/_humanoid/_humanoid");
 AppendInt(file,0);AppendInt(file,3);AppendInt(file,lods);
 const std::size_t lodField=file.size();AppendInt(file,0);
 AppendInt(file,3);AppendInt(file,std::int32_t(format::MeshHeaderBytes+12));
 const std::size_t endField=file.size();AppendInt(file,0);
 const std::size_t offsets=file.size();for(int s=0;s<3;++s)AppendInt(file,0);
 const char *names[]{"root","child","far"};
 for(int s=0;s<3;++s){
  PutInt(file,offsets+std::size_t(s)*4,std::int32_t(file.size()-format::MeshHeaderBytes));
  AppendName(file,names[s]);AppendInt(file,flags[std::size_t(s)]);AppendName(file,"");
  AppendInt(file,0);AppendInt(file,s?0:-1);AppendInt(file,s?0:2);
  if(!s){AppendInt(file,1);AppendInt(file,2);}
 }
 PutInt(file,lodField,std::int32_t(file.size()));
 const float corner[4][2]{{-1,-1},{1,-1},{1,1},{-1,1}};
 const float offset[3]{0,childOffset,40};
 for(int level=0;level<lods;++level){
  const std::size_t lod=file.size();AppendInt(file,0);
  const std::size_t table=file.size();for(int s=0;s<3;++s)AppendInt(file,0);
  for(int s=0;s<3;++s){
   PutInt(file,table+std::size_t(s)*4,std::int32_t(file.size()-table));
   const std::size_t at=file.size();AppendSurface(file,4,2,1);PutInt(file,at+4,s);
   PutInt(file,at+8,-std::int32_t(at));
   const std::size_t verts=at+std::size_t(model::Reader(file.data(),file.size()).Int(at+16));
   for(int v=0;v<4;++v){
    const std::size_t p=verts+std::size_t(v)*32+12;
    // Each LOD shrinks a little, so LODs differ.
    const float shrink=1.0f-0.1f*float(level);
    PutFloat(file,p,0);PutFloat(file,p+4,offset[s]+corner[v][0]*shrink);PutFloat(file,p+8,corner[v][1]*shrink);
   }
  }
  PutInt(file,lod,std::int32_t(file.size()-lod));
 }
 PutInt(file,endField,std::int32_t(file.size()));
 return file;
}
static std::uint32_t Load(std::uint32_t identity,const char *name,intptr_t expected){
 intptr_t init[]{297,reinterpret_cast<intptr_t>(&identity),reinterpret_cast<intptr_t>(name),0,0,0,0,0};
 assert(Invoke(init)==expected&&identity);return identity;
}
static intptr_t Count(std::uint32_t identity,intptr_t index){intptr_t args[]{313,static_cast<intptr_t>(identity),index};return Invoke(args);}
static intptr_t Add(std::uint32_t identity,const Request &request){
 Request copy=request;intptr_t args[]{314,static_cast<intptr_t>(identity),reinterpret_cast<intptr_t>(&copy)};return Invoke(args);
}
static void Clear(std::uint32_t identity){intptr_t args[]{315,static_cast<intptr_t>(identity)};assert(Invoke(args)==0);}
static fntech3::ghoul2::geometry::DeformedGeometry Posed(std::uint32_t identity,int index,int lod,int now,float scale=1){
 const auto &instance=*CL_JKACgameGhoul2Resolve(identity);const auto &m=*instance.At(index);
 pose::Palette palette;assert(CL_JKACgameGhoul2Store().LinkedPalette(instance,index,now,palette)==fntech3::ghoul2::Posing::Served);
 fntech3::ghoul2::geometry::Geometry g;fntech3::ghoul2::geometry::DeformedGeometry out;
 assert(fntech3::ghoul2::geometry::Load(m.meshBytes->data(),m.meshBytes->size(),m.skeleton.numBones,lod,g)==fntech3::ghoul2::geometry::Error::None);
 assert(fntech3::ghoul2::geometry::Deform(g,palette.deformation,out)==fntech3::ghoul2::geometry::Error::None);
 for(auto &s:out.surfaces)for(auto &v:s.vertices)for(auto &c:v.position)c*=scale;
 return out;
}
/* The splotch rules, independently: per-vertex (s,t) in a square of half
 * width size around the hit, a triangle kept unless every corner is past the
 * same edge, faces culled only when one side is excluded. */
struct Expected{std::vector<int> vertices;std::vector<std::array<double,2>> st;std::vector<std::array<int,3>> triangles;};
static Expected Oracle(const fntech3::ghoul2::geometry::DeformedSurface &surface,const std::array<double,3> &start,
 const std::array<double,3> &direction,double sSize,double tSize,double theta,bool front,bool back){
 auto cross=[](std::array<double,3> a,std::array<double,3> b){return std::array<double,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
 auto dot=[](std::array<double,3> a,std::array<double,3> b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 auto unit=[&](std::array<double,3> a){double l=std::sqrt(dot(a,a));return std::array<double,3>{a[0]/l,a[1]/l,a[2]/l};};
 auto b1=cross(direction,{0,0,1});if(dot(b1,b1)<.1)b1=cross(direction,{0,1,0});
 auto b2=unit(cross(direction,b1));b1=unit(b1);
 std::array<double,3> taxis,saxis;
 for(int i=0;i<3;++i){taxis[i]=(b1[i]*std::cos(theta)+b2[i]*std::sin(theta))*.5/tSize;saxis[i]=(-b1[i]*std::sin(theta)+b2[i]*std::cos(theta))*.5/sSize;}
 std::vector<unsigned> off;std::vector<std::array<double,2>> st;
 for(const auto &v:surface.vertices){
  std::array<double,3> d{v.position[0]-start[0],v.position[1]-start[1],v.position[2]-start[2]};
  double s=dot(d,saxis)+.5,t=dot(d,taxis)+.5;unsigned in=0;
  if(s>0)in|=1;if(s<1)in|=2;if(t>0)in|=4;if(t<1)in|=8;off.push_back(~in);st.push_back({s,t});
 }
 Expected e;std::vector<int> remap(surface.vertices.size(),-1);
 for(const auto &tri:surface.triangles){
  if(15&off[tri[0]]&off[tri[1]]&off[tri[2]])continue;
  if(!front||!back){
   const auto &a=surface.vertices[tri[0]].position,&b=surface.vertices[tri[1]].position,&c=surface.vertices[tri[2]].position;
   auto n=cross({double(b[0])-a[0],double(b[1])-a[1],double(b[2])-a[2]},{double(c[0])-a[0],double(c[1])-a[1],double(c[2])-a[2]});
   if(!(dot(direction,n)>0?front:back))continue;
  }
  std::array<int,3> m;
  for(int k=0;k<3;++k){auto &slot=remap[tri[std::size_t(k)]];
   if(slot<0){slot=int(e.vertices.size());e.vertices.push_back(int(tri[std::size_t(k)]));e.st.push_back(st[tri[std::size_t(k)]]);}
   m[std::size_t(k)]=slot;}
  e.triangles.push_back(m);
 }
 return e;
}
static void Same(const gore::Lod &lod,const Expected &e){
 assert(lod.filled&&lod.vertices==e.vertices&&lod.triangles==e.triangles&&lod.texCoords.size()==e.st.size());
 for(std::size_t i=0;i<e.st.size();++i)for(int k=0;k<2;++k)assert(std::fabs(lod.texCoords[i][std::size_t(k)]-e.st[i][std::size_t(k)])<2e-4);
}
static std::array<double,3> Center(const fntech3::ghoul2::geometry::DeformedSurface &s){
 std::array<double,3> c{};for(const auto &v:s.vertices)for(int i=0;i<3;++i)c[std::size_t(i)]+=v.position[std::size_t(i)]/4.0;return c;
}
static std::array<double,3> Normal(const fntech3::ghoul2::geometry::DeformedSurface &s){
 const auto &a=s.vertices[0].position,&b=s.vertices[1].position,&c=s.vertices[2].position;
 std::array<double,3> e1{double(b[0])-a[0],double(b[1])-a[1],double(b[2])-a[2]},e2{double(c[0])-a[0],double(c[1])-a[1],double(c[2])-a[2]};
 std::array<double,3> n{e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
 const double l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);return {n[0]/l,n[1]/l,n[2]/l};
}
/* The splotch is centred on the hit; the shot runs against the normal. */
static Request Shot(const std::array<double,3> &hit,const std::array<double,3> &along){
 Request r{};
 for(int i=0;i<3;++i){r.hitLocation[i]=float(hit[std::size_t(i)]);r.rayDirection[i]=float(-along[std::size_t(i)]);}
 r.sSize=1.2f;r.tSize=0.8f;r.theta=0.7f;r.growDuration=-1;r.startFraction=1;r.frontFaces=r.backFaces=1;
 r.lifeTime=0;r.shader=77;r.scale[0]=r.scale[1]=r.scale[2]=1;return r;
}
static gore::Set *SetOf(std::uint32_t identity,int index){
 if(!jkaCgameGore)return nullptr;const int word=jkaCgameGore->Word(identity,index);return word?jkaCgameGore->FindSet(word):nullptr;
}
static fntech3_jka_mp_refEntity_t entity;static fntech3_jka_mp_refdef_t view3d;
static void Draw(std::uint32_t identity,int renderfx=0x80){
 entity.ghoul2=identity;entity.renderfx=renderfx;Queue(entity);Render(view3d);ClearScene();
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 vm_t vm{};vm.index=VM_CGAME;vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);cgvm=&vm;
 // Before any model loads: counting or adding with a null or unknown
 // identity is refused and makes no store and no gore.
 Request idle{};idle.rayDirection[0]=1;idle.sSize=idle.tSize=1;
 Reject([&]{Count(0,0);});Reject([&]{Count(0x1234,0);});Reject([&]{Add(0,idle);});Reject([&]{Add(0x1234,idle);});
 assert(!jkaCgameGhoul2Store&&!jkaCgameGore);
 // A shot shorter than 0.1 adds nothing and reads no identity (0x4e08fb).
 Request still=idle;still.rayDirection[0]=0.05f;assert(Add(0,still)==0&&Add(0x1234,still)==0&&!jkaCgameGore);
 if(sizeof(void *)!=4){std::cout<<"x86-only cgame Ghoul2 refused\n";return 0;}
 // The projection itself, on a hand-made surface: shot down z with no
 // turn, so s = 0.5 - y/(2*sSize) and t = 0.5 + x/(2*tSize).
 {gore::State st;gore::Trace tr;tr.direction={0,0,-1};tr.sSize=1;tr.tSize=2;tr.identity=0x55;
  tr.growDuration=-1;tr.startFraction=1;tr.now=10;
  fntech3::ghoul2::geometry::DeformedSurface s;
  auto vert=[&](float x,float y){fntech3::ghoul2::geometry::Vertex v{};v.position={x,y,0};s.vertices.push_back(v);};
  vert(0,-0.9f);vert(0.2f,-0.9f);vert(0,-0.85f);   // inside, near the s=1 edge
  vert(3,0);vert(4,0);vert(0,3);                   // corners past different edges: kept
  vert(5,5);vert(6,5);vert(5,6);                   // all past the t=1 edge: cut
  s.triangles={{0,1,2},{3,4,5},{6,7,8}};
  st.ResetPending();gore::Polys(st,s,7,tr);
  const auto *set=st.FindSet(st.Word(0x55,0));assert(set&&set->marks.size()==1&&set->marks.count(7)==1);
  const auto &lod=st.records.at(set->marks.begin()->second.tag).lods[0];
  assert(lod.filled&&lod.vertices==std::vector<int>({0,1,2,3,4,5}));
  {const std::vector<std::array<int,3>> cut{{0,1,2},{3,4,5}};assert(lod.triangles==cut);}
  const float st6[6][2]{{0.95f,0.5f},{0.95f,0.55f},{0.925f,0.5f},{0.5f,1.25f},{0.5f,1.5f},{-1.0f,0.5f}};
  for(int v=0;v<6;++v)for(int k=0;k<2;++k)assert(std::fabs(lod.texCoords[std::size_t(v)][std::size_t(k)]-st6[v][k])<1e-6f);
  // A second LOD of the same call shares the mark and fills its own slot.
  tr.lod=1;gore::Polys(st,s,7,tr);assert(set->marks.size()==1&&st.records.at(set->marks.begin()->second.tag).lods[1].filled);
  // A surface wholly past one edge files nothing.
  fntech3::ghoul2::geometry::DeformedSurface past;past.vertices={s.vertices[6],s.vertices[7],s.vertices[8]};past.triangles={{0,1,2}};
  gore::Polys(st,past,8,tr);assert(!set->marks.count(8));}
 auto files=Installed();assets=files.files;
 assets["models/players/_humanoid/_humanoid.gla"]=PoseSkeleton();
 assets["models/players/gore/model.glm"]=GoreMesh(4,{0,0,0});
 assets["models/players/gore2/model.glm"]=GoreMesh(2,{0,0,0});
 assets["models/players/goreoff/model.glm"]=GoreMesh(2,{0,2,0});
 for(int axis=0;axis<3;++axis)entity.axis[axis][axis]=1;
 view3d.width=640;view3d.height=480;view3d.fov_x=90;view3d.fov_y=75;view3d.time=99999;view3d.rdflags=0x10;
 const auto body=Load(0,"models/players/gore/model.glm",0);
 Request plain=idle;
 Reject([&]{Add(body,plain);});                         // no published frame clock yet
 jkaCgameBoneClocks.BeginFrame(1000,false);const int now=jkaCgameBoneClocks.Time();
 vm.dllExports=reinterpret_cast<decltype(vm.dllExports)>(1);Reject([&]{Add(body,plain);});Reject([&]{Count(body,0);});vm.dllExports=nullptr;
 vm.entryPoint=nullptr;Reject([&]{Add(body,plain);});Reject([&]{Count(body,0);});vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm.index=VM_UI;Reject([&]{Add(body,plain);});vm.index=VM_CGAME;
 {intptr_t args[]{314,static_cast<intptr_t>(body),0};Reject([&]{Invoke(args);});}   // null gore record
 for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
  Request r=plain;r.theta=bad;Reject([&]{Add(body,r);});r=plain;r.hitLocation[2]=bad;Reject([&]{Add(body,r);});}
 assert(Count(body,0)==0);
 for(intptr_t place:{intptr_t(-1),intptr_t(1),intptr_t(INT_MAX)})Reject([&]{Count(body,place);});
 // One shot at the root's centre, along its normal: LODs 0, 1 and 2 of
 // four (the cap is 3); root and child are touched, far is not. One mark per
 // touched surface, shared by the LODs.
 auto lod0=Posed(body,0,0,now);
 const auto hit=Center(lod0.surfaces[0]);const auto normal=Normal(lod0.surfaces[0]);
 Request shot=Shot(hit,normal);shot.lifeTime=500;shot.fadeOutTime=200;shot.fadeRGB=1;
 assert(Add(body,shot)==0&&Count(body,0)==2&&cvarGets==1);
 auto *set=SetOf(body,0);assert(set&&set->references==1&&set->marks.count(0)==1&&set->marks.count(1)==1&&!set->marks.count(2));
 const auto &mark=set->marks.find(0)->second;
 assert(mark.shader==77&&mark.deleteTime==now+500&&mark.fadeTime==200&&mark.fadeRGB&&mark.growStart==now&&mark.growEnd==-1);
 assert(mark.growOffset==1.0f&&mark.growFactor==0.0f);
 for(const auto &entry:set->marks){
  const auto &record=jkaCgameGore->records.at(entry.second.tag);
  for(int lod=0;lod<4;++lod){
   if(lod==3){assert(!record.lods[3].filled);continue;}
   const auto posed=Posed(body,0,lod,now);
   const auto e=Oracle(posed.surfaces[std::size_t(entry.first)],hit,{-normal[0],-normal[1],-normal[2]},1.2,0.8,0.7,true,true);
   assert(!e.vertices.empty());Same(record.lods[std::size_t(lod)],e);
  }
 }
 // Growth: a duration sets the end and the rate from the start fraction.
 {Clear(body);Request grow=shot;grow.growDuration=100;grow.startFraction=0.5f;assert(Add(body,grow)==0);
  const auto &m=SetOf(body,0)->marks.begin()->second;assert(m.growEnd==now+100&&m.growStart==now&&m.growOffset==0.5f&&m.growFactor==0.5f/100.0f);
  Clear(body);assert(Add(body,shot)==0&&Count(body,0)==2);set=SetOf(body,0);}
 // The tags of one call share their upper bits; the next call's do not.
 const int firstTag=set->marks.find(0)->second.tag;
 assert(Add(body,shot)==0&&Count(body,0)==4);
 int secondTag=0;for(const auto &entry:set->marks)if(entry.second.tag!=firstTag&&entry.first==0)secondTag=entry.second.tag;
 assert((firstTag&~0xff)!=(secondTag&~0xff));
 // r_lodbias moves the first LOD (capped at 2); a 2-LOD model stops at 2.
 Clear(body);assert(Count(body,0)==0&&!SetOf(body,0));
 lodBias=1;assert(Add(body,shot)==0&&Count(body,0)==2);
 {const auto &r=jkaCgameGore->records.at(SetOf(body,0)->marks.begin()->second.tag);assert(!r.lods[0].filled&&r.lods[1].filled&&r.lods[2].filled&&!r.lods[3].filled);}
 Clear(body);lodBias=9;assert(Add(body,shot)==0);
 {const auto &r=jkaCgameGore->records.at(SetOf(body,0)->marks.begin()->second.tag);assert(!r.lods[1].filled&&r.lods[2].filled&&!r.lods[3].filled);}
 Clear(body);lodBias=0;
 const auto small=Load(0,"models/players/gore2/model.glm",0);
 assert(Add(small,shot)==0&&Count(small,0)==2);
 {const auto &r=jkaCgameGore->records.at(SetOf(small,0)->marks.begin()->second.tag);assert(r.lods[0].filled&&r.lods[1].filled&&!r.lods[2].filled);}
 // A surface switched off by file flags, by import 321 or under
 // NODESCENDANTS takes no gore.
 const auto off=Load(0,"models/players/goreoff/model.glm",0);
 assert(Add(off,shot)==0&&Count(off,0)==1&&SetOf(off,0)->marks.count(0)==1);
 Clear(off);
 {intptr_t sw[]{321,static_cast<intptr_t>(small),reinterpret_cast<intptr_t>("child"),2};assert(Invoke(sw)==1);}
 Clear(small);assert(Add(small,shot)==0&&Count(small,0)==1&&!SetOf(small,0)->marks.count(1));
 {intptr_t sw[]{321,static_cast<intptr_t>(small),reinterpret_cast<intptr_t>("root"),0x100};assert(Invoke(sw)==1);}
 Clear(small);assert(Add(small,shot)==0&&Count(small,0)==0);
 {intptr_t sw[]{321,static_cast<intptr_t>(body),reinterpret_cast<intptr_t>("root"),0x100};assert(Invoke(sw)==1);}
 Clear(body);assert(Add(body,shot)==0&&Count(body,0)==0);
 {intptr_t sw[]{321,static_cast<intptr_t>(body),reinterpret_cast<intptr_t>("root"),0};assert(Invoke(sw)==1);}
 // A shot straight down z takes the second basis (0,1,0); every mark matches.
 Clear(body);Request down=shot;down.rayDirection[0]=down.rayDirection[1]=0;down.rayDirection[2]=-1;
 assert(Add(body,down)==0);
 {int touched=0;for(int s=0;s<3;++s){const auto e=Oracle(lod0.surfaces[std::size_t(s)],hit,{0,0,-1},1.2,0.8,0.7,true,true);
   if(e.vertices.empty()){assert(!SetOf(body,0)||!SetOf(body,0)->marks.count(s));continue;}
   ++touched;Same(jkaCgameGore->records.at(SetOf(body,0)->marks.find(s)->second.tag).lods[0],e);}
  assert(Count(body,0)==touched);}
 // Faces: with only back faces asked for, the quads facing the shot are cut.
 Clear(body);Request front=shot;front.frontFaces=0;assert(Add(body,front)==0);
 const int frontCount=int(Count(body,0));
 Clear(body);Request back=shot;back.backFaces=0;assert(Add(body,back)==0);
 // The shot runs against the quads' normals: they are back faces to it.
 assert(frontCount==2&&Count(body,0)==0);
 // Scale: the posed vertices grow before the splotch is laid; zero is one.
 Clear(body);Request big=shot;big.scale[0]=big.scale[1]=big.scale[2]=4;
 for(int i=0;i<3;++i)big.hitLocation[i]=float(hit[std::size_t(i)]*4);
 assert(Add(body,big)==0);
 {const auto posed=Posed(body,0,0,now,4);const auto &r=jkaCgameGore->records.at(SetOf(body,0)->marks.find(0)->second.tag);
  Same(r.lods[0],Oracle(posed.surfaces[0],{hit[0]*4,hit[1]*4,hit[2]*4},{-normal[0],-normal[1],-normal[2]},1.2,0.8,0.7,true,true));}
 Clear(body);Request zero=shot;zero.scale[0]=zero.scale[1]=zero.scale[2]=0;assert(Add(body,zero)==0&&Count(body,0)==2);
 Same(jkaCgameGore->records.at(SetOf(body,0)->marks.find(0)->second.tag).lods[0],
  Oracle(lod0.surfaces[0],hit,{-normal[0],-normal[1],-normal[2]},1.2,0.8,0.7,true,true));
 // The entity's angles and position carry the shot into the model's space.
 Clear(body);Request moved=shot;moved.position[2]=100;moved.angles[1]=90;
 {const auto inverse=gore::InverseWorld(moved.angles,moved.position);(void)inverse;
  // the hit placed where the moved root is: world = R*model + position
  const auto world=pose::Rotation(moved.angles);
  for(int i=0;i<3;++i){moved.hitLocation[i]=float(world[std::size_t(i*4)]*hit[0]+world[std::size_t(i*4+1)]*hit[1]+world[std::size_t(i*4+2)]*hit[2]+moved.position[i]);
   moved.rayDirection[i]=float(-(world[std::size_t(i*4)]*normal[0]+world[std::size_t(i*4+1)]*normal[1]+world[std::size_t(i*4+2)]*normal[2]));}}
 assert(Add(body,moved)==0&&Count(body,0)==2);
 // Model 0 alone by default; cg_g2MarksAllModels lets model 1 take gore.
 Clear(body);
 const auto pair=Load(0,"models/players/gore/model.glm",0);assert(Load(pair,"models/players/gore/model.glm",1)==pair);
 assert(Add(pair,shot)==0&&Count(pair,0)==2&&Count(pair,1)==0);
 marksAllModels.integer=1;assert(Add(pair,shot)==0&&Count(pair,0)==4&&Count(pair,1)==2&&cvarGets==1);
 marksAllModels.integer=0;
 // An instance whose model 0 is gone is refused (retail reads it unchecked).
 auto *first=const_cast<fntech3::ghoul2::Model *>(CL_JKACgameGhoul2Resolve(pair)->At(0));first->present=false;
 Reject([&]{Add(pair,shot);});assert(Count(pair,0)==4);first->present=true;
 // Duplicates share the set with one reference more; a clear drops one.
 std::uint32_t copy=0;intptr_t duplicate[]{309,static_cast<intptr_t>(pair),reinterpret_cast<intptr_t>(&copy)};
 assert(Invoke(duplicate)==0&&copy&&Count(copy,0)==4&&Count(copy,1)==2&&SetOf(pair,0)==SetOf(copy,0)&&SetOf(pair,0)->references==2);
 assert(Add(copy,shot)==0&&Count(pair,0)==6);
 Clear(pair);assert(Count(pair,0)==0&&Count(copy,0)==6&&SetOf(copy,0)->references==1);
 // A single-model copy carries the word with no new reference (0x4dea40).
 const auto other=Load(0,"models/players/gore/model.glm",0);
 intptr_t specific[]{308,static_cast<intptr_t>(copy),0,static_cast<intptr_t>(other),0};
 assert(Invoke(specific)==0&&Count(other,0)==6&&SetOf(other,0)==SetOf(copy,0)&&SetOf(copy,0)->references==1);
 // Clean clears the instance's gore first (0x4de520).
 std::uint32_t gone=copy;intptr_t clean[]{301,reinterpret_cast<intptr_t>(&gone)};
 const auto records=jkaCgameGore->records.size();
 assert(!Invoke(clean)&&!gone&&Count(other,0)==0&&jkaCgameGore->records.size()<records);
 // Drawing expires a mark whose delete time has come, on a drawn surface of
 // a model that is not the viewer's own; its record entry stays, empty.
 Clear(body);Clear(pair);Clear(other);
 shot.lifeTime=50;jkaCgameBoneClocks.BeginFrame(2000,false);const int later=jkaCgameBoneClocks.Time();
 assert(Add(body,shot)==0&&Count(body,0)==2);
 const int tag=SetOf(body,0)->marks.begin()->second.tag;
 Draw(body);assert(Count(body,0)==2);
 jkaCgameBoneClocks.BeginFrame(later+50,false);
 Draw(body,0x82);assert(Count(body,0)==2);                 // a personal model keeps its marks
 Draw(body);assert(Count(body,0)==0&&jkaCgameGore->records.count(tag)&&!jkaCgameGore->records.at(tag).lods[0].filled);
 // A mark on a switched-off surface is not drawn, so it does not expire.
 shot.lifeTime=10;assert(Add(small,shot)==0);  // small's root has NODESCENDANTS: nothing taken
 {intptr_t sw[]{321,static_cast<intptr_t>(small),reinterpret_cast<intptr_t>("root"),0};assert(Invoke(sw)==1);}
 assert(Add(small,shot)==0&&Count(small,0)==1);
 {intptr_t sw[]{321,static_cast<intptr_t>(small),reinterpret_cast<intptr_t>("root"),2};assert(Invoke(sw)==1);}
 jkaCgameBoneClocks.BeginFrame(later+5000,false);Draw(small);assert(Count(small,0)==1);
 {intptr_t sw[]{321,static_cast<intptr_t>(small),reinterpret_cast<intptr_t>("root"),0};assert(Invoke(sw)==1);}
 Draw(small);assert(Count(small,0)==0);
 // No lifetime: never expires.
 shot.lifeTime=0;assert(Add(body,shot)==0);jkaCgameBoneClocks.BeginFrame(later+99999,false);Draw(body);assert(Count(body,0)==2);
 // Clearing the client game drops every set and record.
 CL_JKACgameClear();assert(!jkaCgameGore&&!jkaCgameGhoul2Store);
 Reject([&]{Count(body,0);});
 // Past 500 records the oldest call's go together before the next is made;
 // drawing then drops the marks left without a record.
 jkaCgameBoneClocks.BeginFrame(5000,false);
 const auto filler=Load(0,"models/players/gore/model.glm",0);
 for(int call=0;call<250;++call)assert(Add(filler,shot)==0);
 assert(jkaCgameGore->records.size()==500&&Count(filler,0)==500);
 assert(Add(filler,shot)==0&&jkaCgameGore->records.size()==500&&Count(filler,0)==502);
 Draw(filler);assert(Count(filler,0)==500);
 CL_JKACgameClear();
 std::cout<<"Academy cgame imports 313 and 314 passed\n";
 return 0;
}
'''


def fixture(header_text=None, gore_text=None, render_text=None):
    source = render_fixture()
    for path, text in ((HEADER, header_text), (RENDER, render_text)):
        include = f'#include "{path}"'
        assert source.count(include) == 1, path
        body = (ROOT / path).read_text() if text is None else text
        body = inline_header(body)
        if path == HEADER:
            gore = (ROOT / GORE).read_text() if gore_text is None else gore_text
            include_gore = f'#include "{GORE}"'
            assert body.count(include_gore) == 1
            body = body.replace(include_gore, inline_header(gore), 1)
        source = source.replace(include, body, 1)
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
        timeout=120, **options)
    return ('ok' if ran.returncode == 0 else 'run'), ran.stdout + ran.stderr


class JKACgameSkinGoreTests(unittest.TestCase):
    def test_retail_facts_are_recorded_with_the_code(self):
        header = (ROOT / HEADER).read_text()
        start, end = block(header, COUNT, '\n    }\n')
        for fact in ('0x413c9d', '0x4116e0', '0x4116e9', '0x4e07a0', '+0x4c', '+0x10'):
            self.assertIn(fact, header[start:end])
        start, end = block(header, ADD, '\n    }\n')
        for fact in ('0x413cb6', '0x4e08f0', '0x4e08fb', '0x4e091e', 'r_lodbias', 'cg_g2MarksAllModels', '0x4ec1c6'):
            self.assertIn(fact, header[start:end])
        gore = (ROOT / GORE).read_text()
        for fact in ('0x12a5030', '0x12a5048', '0x4e9070', '0x1f4', '0x4e91a0', '0x4e9310', '0x4ea460',
                     '0x4ec0a0', '0x498740', '0x4966a0', '0x4dea40', '0x4e03b0', '0x4ec580', '0x4ec620'):
            self.assertIn(fact, gore)
        calls = (ROOT / 'code/compat/jka_module_calls.def').read_text()
        for name, number in (('CG_G2_GETNUMGOREMARKS', 313), ('CG_G2_ADDSKINGORE', 314)):
            self.assertIn(f'FNTECH3_JKA_MODULE_CALL(CGAME, IMPORT, {name}, {number})', calls)

    def test_production_gore_and_failing_mutations(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        sources = {HEADER: (ROOT / HEADER).read_text(), GORE: (ROOT / GORE).read_text(),
                   RENDER: (ROOT / RENDER).read_text()}
        mutated = {}
        for group, path in ((HEADER_MUTATIONS, HEADER), (GORE_MUTATIONS, GORE), (RENDER_MUTATIONS, RENDER)):
            for name, mutate in group.items():
                text = mutate(sources[path])
                self.assertNotEqual(text, sources[path], name)
                mutated[name] = {HEADER: 'header_text', GORE: 'gore_text', RENDER: 'render_text'}[path], text
        parent = ROOT / '.tmp/tests/jka-cgame-skin-gore'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            folder = Path(temporary)
            jobs = {'production': fixture()}
            jobs.update({f'mutant{n}': fixture(**{key: text})
                         for n, (key, text) in enumerate(mutated.values())})
            with ThreadPoolExecutor(max_workers=6) as pool:
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
