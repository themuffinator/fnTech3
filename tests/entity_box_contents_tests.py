"""Sweep box entities as RTCW MP and ET do: as their own contents.

Retail WolfMP.exe and et.exe give the temporary box brush a traced entity's
r.contents for the sweep that tests it and put CONTENTS_BODY back afterwards
(SV_ClipMoveToEntities 0x43d339..0x43d3f0 and 0x449e61..0x449f1c). Quake III,
Quake Live and RTCW SP keep CONTENTS_BODY. The first fixture runs the
production server trace code over each frozen entity layout against the real
box hull and trace code, so the entity each mask selects and the contents each
trace reports are the engine's own. The second follows the box's contents
through every way out of the sweep, for every catalog profile.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

# The IHV header exists only while that profile's work is in the tree; the
# server code extracted below uses it exactly when it exists.
SERVER_HEADERS = r'''
#include "code/server/server.h"
#include "code/server/server_world_bounds.hpp"
#include "code/server/sv_collision_model.hpp"
#include "code/qcommon/cm_model_handles.h"
#if __has_include("code/compat/q3_ihv_collision.hpp")
#include "code/compat/q3_ihv_collision.hpp"
#endif
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
server_t sv{};
static comGameProfile_t profile{};
const comGameProfile_t *Com_GameProfile(){return &profile;}
int Com_GameIsWolfET(){return profile.id==COM_GAME_WOLFET;}
int Com_GameIsJKASP(){return profile.id==COM_GAME_JKA_SP;}
// cm_test.c's area portals ask these; no trace here reaches them.
int Com_GameIsRitual(){return profile.titleId==COM_GAME_TITLE_ALICE||profile.titleId==COM_GAME_TITLE_FAKK2;}
int Com_GameIsMOHAA(){return profile.titleId==COM_GAME_TITLE_MOHAA;}
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
static void Check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
'''

# The real temporary box and trace code over an empty world: one node whose
# sides both reach leaf 0, which holds nothing, with room behind each array for
# the box hull, as the map loaders leave it.
COLLISION_PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include <stdlib.h>
#include <string.h>
clipMap_t cm;
int c_pointcontents;
int c_traces, c_brush_traces, c_patch_traces;
cvar_t *cm_noAreas, *cm_noCurves, *cm_playerCurveClip;
static cvar_t fixtureCvars[4];
static int fixtureCvarCount;
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	int i;
	(void)flags;
	for ( i = 0; i < fixtureCvarCount; ++i ) {
		if ( !strcmp( fixtureCvars[i].name, name ) ) return &fixtureCvars[i];
	}
	if ( fixtureCvarCount == 4 ) abort();
	fixtureCvars[fixtureCvarCount].name = (char *)name;
	fixtureCvars[fixtureCvarCount].string = (char *)value;
	fixtureCvars[fixtureCvarCount].value = (float)atof( value );
	fixtureCvars[fixtureCvarCount].integer = atoi( value );
	return &fixtureCvars[fixtureCvarCount++];
}
void CM_TraceThroughPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc ) {
	(void)tw; (void)pc;
}
qboolean CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc ) {
	(void)tw; (void)pc; return qfalse;
}
#include "code/qcommon/cm_mohaa_terrain.h"
void CM_MOHAA_TraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	(void)tw; (void)tc;
}
qboolean CM_MOHAA_PositionTestInTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc ) {
	(void)tw; (void)tc; return qfalse;
}
/* Q3Test's box brushes are copied into the zone (cm_test.c). */
void *Z_Malloc( int size ) { return calloc( 1, (size_t)size ); }
void Z_Free( void *ptr ) { free( ptr ); }
static cmodel_t box_model;
static cplane_t *box_planes;
static cbrush_t *box_brush;
'''

COLLISION_WORLD = r'''
void Fixture_LoadEmptyWorld( void ) {
	static cplane_t planes[1 + 12];
	static cNode_t nodes[1];
	static cLeaf_t leafs[1];
	static cbrush_t brushes[1];
	static cbrushside_t sides[6];
	static int leafbrushes[1];
	static cmodel_t models[1];
	memset( &cm, 0, sizeof( cm ) );
	memset( planes, 0, sizeof( planes ) );
	memset( brushes, 0, sizeof( brushes ) );
	memset( sides, 0, sizeof( sides ) );
	memset( &box_model, 0, sizeof( box_model ) );
	cm_noAreas = Cvar_Get( "cm_noAreas", "0", 0 );
	cm_noCurves = Cvar_Get( "cm_noCurves", "0", 0 );
	cm_playerCurveClip = Cvar_Get( "cm_playerCurveClip", "1", 0 );
	planes[0].normal[0] = 1;
	planes[0].dist = 65536;
	planes[0].type = PLANE_X;
	SetPlaneSignbits( &planes[0] );
	nodes[0].plane = &planes[0];
	nodes[0].children[0] = nodes[0].children[1] = -1;
	cm.planes = planes; cm.numPlanes = 1;
	cm.nodes = nodes; cm.numNodes = 1;
	cm.leafs = leafs; cm.numLeafs = 1;
	cm.brushes = brushes; cm.brushsides = sides; cm.leafbrushes = leafbrushes;
	cm.cmodels = models; cm.numSubModels = 1;
	CM_InitBoxHull();
}
int Fixture_TempBoxContents( void ) {
	return box_brush->contents;
}
'''

REAL_PRELUDE = SERVER_HEADERS + r'''
#include "code/server/server_game_data.h"
#include "code/compat/game_abi_adapter.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/stvef_game_abi.h"
#include "code/compat/q3test_game_abi.h"
void QDECL Com_Printf(const char *,...){}
void QDECL Com_DPrintf(const char *,...){}
extern "C" void Fixture_LoadEmptyWorld(void);
extern "C" int Fixture_TempBoxContents(void);
static std::vector<int> touching;
int SV_AreaEntities(const vec3_t,const vec3_t,int *out,int maximum){
 Check(static_cast<int>(touching.size())<=maximum,"Contact limit");
 for(std::size_t i=0;i<touching.size();++i)out[i]=touching[i];return static_cast<int>(touching.size());
}
static fntech3::server::ServerGameDataView serverGameData;
'''

REAL_CHECKS = r'''
constexpr int corpse=CONTENTS_CORPSE,trigger=CONTENTS_TRIGGER,solid=CONTENTS_SOLID,body=CONTENTS_BODY;
constexpr int shot=CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE;
static const vec3_t lo{-1,-1,-1},hi{1,1,1};

template<class Entity,class Player> void run(comGameId_t id){
 profile={};for(const auto &p:profiles)if(p.id==id)profile=p;
 Check(profile.id==id,"Catalog profile");
 const bool own=profile.entityBoxContentsPolicy==COM_ENTITY_BOX_CONTENTS_ENTITY;
 Check(own==(id==COM_GAME_RTCW_MP||id==COM_GAME_WOLFET),"Retail box-contents profiles");
 Fixture_LoadEmptyWorld();
 // Five boxes 16 units across, one every 64 units along +x. The fifth carries
 // the capsule flag, which RTCW's and ET's hull follows for every sweep.
 constexpr std::size_t count=6,base=16,stride=sizeof(Entity)+64;
 const int contents[count]={0,corpse,trigger,solid,body,corpse};
 std::vector<unsigned char> memory(base+count*stride+16,0xa5),clients(sizeof(Player));
 for(std::size_t n=0;n<count;++n){
  Entity e{};e.s.number=int(n);e.r.linked=1;e.r.ownerNum=ENTITYNUM_NONE;e.r.contents=contents[n];
  for(int axis=0;axis<3;++axis){e.r.mins[axis]=-8;e.r.maxs[axis]=8;}
  e.r.currentOrigin[0]=64.0f*n;
  if(n==5)e.r.svFlags=id==COM_GAME_QUAKE3||id==COM_GAME_QUAKELIVE?SVF_CAPSULE:FNTECH3_RTCW_SVF_CAPSULE;
  std::memcpy(memory.data()+base+n*stride,&e,sizeof(e));
 }
 fntech3::server::NativeGameDataBinding binding;
 binding.entityBase=memory.data()+base;binding.entitySpanBytes=count*stride;binding.entityCount=count;binding.entityStride=stride;
 binding.clientBase=clients.data();binding.clientSpanBytes=clients.size();binding.clientCount=1;binding.clientStride=sizeof(Player);
 Check(serverGameData.BindNative(id,binding)==fntech3::server::GameDataBindError::None,"Module binding");
 const auto before=memory;
 auto sweep=[&](float from,float to,int mask){
  touching={1,2,3,4,5};vec3_t start{from,0,0},end{to,0,0};trace_t result;std::memset(&result,0xa5,sizeof(result));
  SV_Trace(&result,start,lo,hi,end,ENTITYNUM_NONE,mask,qfalse);
  Check(Fixture_TempBoxContents()==body,"The box is CONTENTS_BODY again after the sweep");
  return result;
 };
 auto hits=[&](const trace_t &t,int number,int reported){
  return t.entityNum==number&&t.contents==reported&&t.fraction<1&&!t.startsolid&&!t.allsolid
   &&std::fabs(t.endpos[0]-(64.0f*number-9.0f))<0.25f;
 };
 auto misses=[&](const trace_t &t){
  return t.entityNum==ENTITYNUM_NONE&&t.fraction==1&&!t.startsolid&&!t.allsolid;
 };
 // RTCW MP and ET select a box entity by its own contents and report them;
 // the other profiles test and report every box as CONTENTS_BODY.
 Check(hits(sweep(0,512,shot),1,own?corpse:body),"A shot reports what it struck");
 const trace_t corpseOnly=sweep(0,512,corpse);
 Check(own?hits(corpseOnly,1,corpse):misses(corpseOnly),"Corpse mask");
 const trace_t triggerOnly=sweep(0,512,trigger);
 Check(own?hits(triggerOnly,2,trigger):misses(triggerOnly),"Trigger mask");
 const trace_t solidOnly=sweep(0,512,solid);
 Check(own?hits(solidOnly,3,solid):misses(solidOnly),"Solid mask");
 Check(hits(sweep(0,512,body),4,body),"Body mask");
 // A capsule's handle is not the box, so RTCW MP and ET leave it CONTENTS_BODY.
 const trace_t capsule=sweep(300,512,shot);
 Check(capsule.entityNum==5&&capsule.contents==body&&capsule.fraction<1
  &&capsule.endpos[0]>304&&capsule.endpos[0]<312,"A capsule reports CONTENTS_BODY");
 Check(misses(sweep(300,512,corpse)),"A capsule is not tested as its own contents");
 // A sweep that starts inside a box.
 const trace_t inside=sweep(64,64,corpse);
 Check(own?inside.allsolid&&inside.startsolid&&inside.fraction==0&&inside.entityNum==1&&inside.contents==corpse
  :misses(inside),"Position test");
 // Everything else still tests the box as CONTENTS_BODY.
 vec3_t start{0,0,0},end{512,0,0};trace_t single;
 SV_ClipToEntity(&single,start,lo,hi,end,1,corpse,qfalse);
 Check(single.fraction==1,"SV_ClipToEntity tests CONTENTS_BODY");
 SV_ClipToEntity(&single,start,lo,hi,end,1,shot,qfalse);
 Check(single.entityNum==1&&single.contents==body&&single.fraction<1,"SV_ClipToEntity reports CONTENTS_BODY");
 touching={1,2,3,4,5};vec3_t point{64,0,0};
 Check(SV_PointContents(point,ENTITYNUM_NONE)==body,"SV_PointContents reads CONTENTS_BODY");
 // As a client game's entity traces do.
 const vec3_t mins{-8,-8,-8},maxs{8,8,8},origin{64,0,0};trace_t client;
 const clipHandle_t box=CM_TempBoxModel(mins,maxs,qfalse);
 CM_TransformedBoxTrace(&client,start,end,lo,hi,box,corpse,origin,vec3_origin,qfalse);
 Check(client.fraction==1,"A client box trace tests CONTENTS_BODY");
 CM_TransformedBoxTrace(&client,start,end,lo,hi,box,shot,origin,vec3_origin,qfalse);
 Check(client.fraction<1&&client.contents==body,"A client box trace reports CONTENTS_BODY");
 Check(memory==before,"Traces changed module records");
 serverGameData.Invalidate();
}
int main(){
 try{
  run<fntech3_q3_sharedEntity_t,fntech3_q3_playerState_t>(COM_GAME_QUAKE3);
  run<fntech3_ql_sharedEntity_t,fntech3_ql_playerState_t>(COM_GAME_QUAKELIVE);
  run<fntech3_rtcw_sp_sharedEntity_t,fntech3_rtcw_sp_playerState_t>(COM_GAME_RTCW_SP);
  run<fntech3_rtcw_mp_sharedEntity_t,fntech3_rtcw_mp_playerState_t>(COM_GAME_RTCW_MP);
  run<fntech3_wolfet_sharedEntity_t,fntech3_wolfet_playerState_t>(COM_GAME_WOLFET);
 }catch(const Drop&){std::fprintf(stderr,"Unexpected engine rejection (profile %d)\n",int(profile.id));return 1;}
 catch(const std::exception &e){std::fprintf(stderr,"%s (profile %d)\n",e.what(),int(profile.id));return 1;}
 std::puts("Entity box contents passed");return 0;
}
'''

# Entities held directly and the collision calls recorded, so the order of the
# box's contents around each sweep can be checked for every catalog profile.
ORDER_PRELUDE = SERVER_HEADERS + r'''
const vec3_t vec3_origin{};
static std::array<sharedEntity_t,4> owned;
sharedEntity_t *SV_GentityNum(int i){if(i<0||i>=4)throw Drop{};return &owned[i];}
qboolean SV_ReadGameEntity(const void *e,sharedEntity_t *out){
 for(const auto &candidate:owned)if(e==&candidate&&out){*out=candidate;return qtrue;}return qfalse;
}
qboolean SV_ReadGameEntityByNum(int i,sharedEntity_t *out){return i>=0&&i<4?SV_ReadGameEntity(&owned[i],out):qfalse;}
struct Event{
 char kind;int value,contents;
 bool operator==(const Event &other)const{return kind==other.kind&&value==other.value&&contents==other.contents;}
};
static std::vector<Event> events;
static int boxContents=CONTENTS_BODY,throwAt=-1,solidAt=-1;
int SV_AreaEntities(const vec3_t,const vec3_t,int *out,int){out[0]=1;out[1]=2;out[2]=3;return 3;}
clipHandle_t CM_InlineModel(int index){return index;}
clipHandle_t CM_TempBoxModel(const vec3_t,const vec3_t,int capsule){return CM_TemporaryModelHandle(capsule?qtrue:qfalse);}
void CM_SetTempBoxModelContents(int contents){boxContents=contents;events.push_back({'S',contents,contents});}
void CM_BoxTrace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,clipHandle_t,int,qboolean){*out={};out->fraction=1;}
void CM_TransformedBoxTrace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,clipHandle_t model,int,
 const vec3_t,const vec3_t,qboolean){
 events.push_back({'T',model,boxContents});
 if(model==throwAt)throw Drop{};
 *out={};out->fraction=1;
 if(model==solidAt){out->allsolid=out->startsolid=qtrue;out->fraction=0;}
}
int CM_PointContents(const vec3_t,clipHandle_t){return 0;}
int CM_TransformedPointContents(const vec3_t,clipHandle_t model,const vec3_t,const vec3_t){
 events.push_back({'P',model,boxContents});return 0;
}
'''

ORDER_CHECKS = r'''
static void Sweep(){
 vec3_t start{0,0,0},end{512,0,0},lo{-1,-1,-1},hi{1,1,1};trace_t result;
 SV_Trace(&result,start,lo,hi,end,0,-1,qfalse);
}
int main(){
 const int contents[4]={0,CONTENTS_CORPSE,CONTENTS_TRIGGER,CONTENTS_SOLID};
 int runs=0;
 try{
  // Forcing the policy runs each profile's own sweep loop with the box's
  // contents set, so every way out of that loop is covered.
  for(const auto &catalogProfile:profiles)for(bool forced:{false,true}){
   profile=catalogProfile;
   const int body=profile.id==COM_GAME_JK2_MP?0x100:CONTENTS_BODY;
   const Event restore{'S',body,body};
   if(forced)profile.entityBoxContentsPolicy=COM_ENTITY_BOX_CONTENTS_ENTITY;
   const bool own=profile.entityBoxContentsPolicy==COM_ENTITY_BOX_CONTENTS_ENTITY;
   owned={};
   for(int n=0;n<4;++n){
    auto &e=owned[n];e.s.number=n;e.r.ownerNum=ENTITYNUM_NONE;e.r.contents=contents[n];
    for(int axis=0;axis<3;++axis){e.r.mins[axis]=-8;e.r.maxs[axis]=8;}
    e.r.currentOrigin[0]=e.s.origin[0]=64.0f*n;
   }
   owned[2].r.svFlags=SVF_CAPSULE;                  // a capsule where the hull follows the flag
   owned[3].r.bmodel=qtrue;owned[3].s.modelindex=1; // an inline model
   const clipHandle_t box=CM_TemporaryModelHandle(qfalse);
   const clipHandle_t flagged=profile.entityHullPolicy==COM_ENTITY_HULL_ENTITY_FLAG?CM_TemporaryModelHandle(qtrue):box;
   std::vector<Event> expected;
   const auto swept=[&](clipHandle_t model,int entityContents){
    const bool set=own&&model==box;
    if(set)expected.push_back({'S',entityContents,entityContents});
    expected.push_back({'T',model,set?entityContents:body});
    if(set)expected.push_back(restore);
   };
   // Only the box's sweep carries the entity's contents, and only while it runs.
   events.clear();boxContents=body;throwAt=solidAt=-1;
   Sweep();swept(box,contents[1]);swept(flagged,contents[2]);swept(1,contents[3]);
   Check(events==expected,"Sweep order and contents");
   Check(boxContents==body,"Box restored after the sweep");
   // An all-solid result returns before the next entity.
   events.clear();expected.clear();solidAt=box;
   Sweep();swept(box,contents[1]);
   Check(events==expected,"All-solid return");
   Check(boxContents==body,"Box restored after an all-solid return");
   // An error thrown out of the box's sweep unwinds through the restore.
   events.clear();expected.clear();solidAt=-1;throwAt=box;bool dropped=false;
   try{Sweep();}catch(Drop&){dropped=true;}
   swept(box,contents[1]);
   Check(dropped&&events==expected,"Unwound sweep");
   Check(boxContents==body,"Box restored when a sweep throws");
   throwAt=-1;
   // Single-entity traces and point contents never change the box.
   events.clear();trace_t single;vec3_t start{0,0,0},end{512,0,0},lo{-1,-1,-1},hi{1,1,1};
   SV_ClipToEntity(&single,start,lo,hi,end,1,-1,qfalse);
   Check((events==std::vector<Event>{{'T',box,body}}),"SV_ClipToEntity keeps the profile's BODY bit");
   events.clear();SV_PointContents(start,ENTITYNUM_NONE);
   Check((events==std::vector<Event>{{'P',box,body},{'P',flagged,body},{'P',1,body}}),
    "SV_PointContents keeps CONTENTS_BODY");
   ++runs;
  }
 }catch(const Drop&){std::fprintf(stderr,"Unexpected engine rejection (profile %d)\n",int(profile.id));return 1;}
 catch(const std::exception &e){std::fprintf(stderr,"%s (profile %d)\n",e.what(),int(profile.id));return 1;}
 if(runs!=2*(COM_GAME_COUNT-1)){std::fprintf(stderr,"Only %d profile runs\n",runs);return 1;}
 std::printf("Box contents followed through %d profile runs\n",runs);return 0;
}
'''


def catalog() -> str:
    source = (ROOT/'code/qcommon/game_profile.cpp').read_text(encoding='utf-8')
    return source[source.index('const comGameProfile_t profiles[]'):source.index('\nstatic_assert(')]


def server_world(*signatures: str) -> str:
    world = (ROOT/'code/server/sv_world.cpp').read_text(encoding='utf-8')
    text = ''
    for signature in signatures:
        if signature == 'struct moveclip_t {':
            start = world.index(signature)
            text += '\n'+world[start:world.index('\n};', start)+3]
        else:
            text += '\n'+function(world, signature)
    return text


def collision_source() -> str:
    load = (ROOT/'code/qcommon/cm_load.c').read_text(encoding='utf-8')
    source = COLLISION_PRELUDE
    for signature in ['static void CM_InitBoxHull( void )', 'cmodel_t *CM_ClipHandleToModel(',
                      'clipHandle_t CM_InlineModel(', 'clipHandle_t CM_TempBoxModel(',
                      'void CM_SetTempBoxModelContents(', 'void CM_ModelBounds(']:
        source += '\n'+function(load, signature)
    return source+COLLISION_WORLD


def real_source() -> str:
    game = (ROOT/'code/server/sv_game.cpp').read_text(encoding='utf-8')
    source = REAL_PRELUDE
    source += game[game.index('struct ServerFlagMapping'):game.index('\nvoid InvalidateServerGameData()')]
    for signature in ['sharedEntity_t *SV_GentityNum(', 'qboolean SV_ReadGameEntity(',
                      'qboolean SV_ReadGameEntityByNum(']:
        source += '\n'+function(game, signature)
    source += '\n'+catalog()
    source += server_world('clipHandle_t SV_ClipHandleForEntity(', 'struct moveclip_t {',
                           'void SV_ClipToEntity(', 'static void SV_ClipMoveToEntities(',
                           'void SV_Trace(', 'int SV_PointContents(')
    return source+REAL_CHECKS


def order_source() -> str:
    source = ORDER_PRELUDE+'\n'+catalog()
    source += server_world('clipHandle_t SV_ClipHandleForEntity(', 'struct moveclip_t {',
                           'void SV_ClipToEntity(', 'static void SV_ClipMoveToEntities(',
                           'void SV_Trace(', 'int SV_PointContents(')
    return source+ORDER_CHECKS


class EntityBoxContentsTests(unittest.TestCase):
    def compilers(self):
        cxx = shutil.which('clang++') or shutil.which('g++')
        cc = shutil.which('clang') or shutil.which('gcc')
        if not cxx or not cc:
            self.skipTest('C and C++ compilers unavailable')
        target = compiler_target_args(cxx)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        return cc, cxx, target

    def build_and_run(self, prefix, cpp_source, extra_cpp=(), c_sources=()):
        cc, cxx, target = self.compilers()
        scratch = ROOT/'.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=prefix, dir=scratch) as folder:
            folder = Path(folder)
            objects = []
            for index, (name, text) in enumerate(c_sources):
                if text is None:
                    path = ROOT/name
                else:
                    path = folder/name
                    path.write_text(text, encoding='utf-8')
                obj = folder/f'c{index}.o'
                built = subprocess.run([cc, *target, '-std=c11', '-O2', '-I', str(ROOT), '-c', str(path),
                                        '-o', str(obj)], cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
                objects.append(str(obj))
            cpp = folder/'probe.cpp'
            exe = folder/'probe.exe'
            cpp.write_text(cpp_source, encoding='utf-8')
            command = [cxx, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), *objects, *extra_cpp,
                       '-o', str(exe)]
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout+built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout+run.stderr)
            return run.stdout

    def test_retail_masks_and_reported_contents_through_real_traces(self):
        output = self.build_and_run(
            'entity-box-contents-', real_source(),
            extra_cpp=[str(ROOT/'code/server/server_game_data.cpp'),
                       '-x', 'c++', str(ROOT/'code/compat/game_abi_adapter.c')],
            c_sources=[('collision.c', collision_source()), ('code/qcommon/cm_trace.c', None),
                       ('code/qcommon/cm_test.c', None), ('code/qcommon/q_math.c', None)])
        self.assertIn('Entity box contents passed', output)

    def test_box_contents_restored_on_every_way_out_for_every_profile(self):
        output = self.build_and_run('entity-box-order-', order_source())
        self.assertIn('Box contents followed through', output)

    def test_only_the_entity_sweep_changes_the_box(self):
        world = (ROOT/'code/server/sv_world.cpp').read_text(encoding='utf-8')
        sweep = function(world, 'static void SV_ClipMoveToEntities(')
        # Set before the sweep, restored by a scope guard rather than a
        # statement a later early exit could skip.
        self.assertLess(sweep.index('CM_SetTempBoxModelContents( entity.r.contents )'),
                        sweep.index('CM_TransformedBoxTrace'))
        self.assertLess(sweep.index('SV_MakeScopeExit'), sweep.index('CM_TransformedBoxTrace'))
        self.assertEqual(sweep.count('CM_SetTempBoxModelContents('), 2)
        self.assertEqual(world.count('CM_SetTempBoxModelContents('), 2)
        for signature in ['void SV_ClipToEntity(', 'int SV_PointContents(', 'clipHandle_t SV_ClipHandleForEntity(']:
            self.assertNotIn('CM_SetTempBoxModelContents', function(world, signature))
        load = (ROOT/'code/qcommon/cm_load.c').read_text(encoding='utf-8')
        # The box is built as CONTENTS_BODY, and CM_TempBoxModel leaves the
        # contents alone, as the originals do.
        self.assertIn('box_brush->contents = CM_BodyContents();', function(load, 'static void CM_InitBoxHull( void )'))
        self.assertNotIn('contents', function(load, 'clipHandle_t CM_TempBoxModel('))


if __name__ == '__main__':
    unittest.main()
