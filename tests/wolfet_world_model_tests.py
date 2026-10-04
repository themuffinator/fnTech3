"""Exercise ET's exclusion of world-model entities in production queries.

The original ET source and hash-pinned executable observations in the box
contents record exclude clip handle zero in the multi-entity sweep and point
queries. The actual world query and single-entity trace remain available.
"""
import unittest

import entity_box_contents_tests as box
from rtcw_server_world_tests import function


def source():
    prelude = box.ORDER_PRELUDE
    old = function(prelude, 'void CM_TransformedBoxTrace(')
    prelude = prelude.replace(old, r'''
void CM_TransformedBoxTrace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,
 clipHandle_t model,int,const vec3_t,const vec3_t,qboolean){
 events.push_back({'T',model,boxContents});
 *out={};out->fraction=model==0?0.25f:0.75f;
}
''')
    prelude = prelude.replace('int CM_PointContents(const vec3_t,clipHandle_t){return 0;}',
                             'int CM_PointContents(const vec3_t,clipHandle_t){return CONTENTS_SOLID;}')
    prelude = prelude.replace("events.push_back({'P',model,boxContents});return 0;",
                             "events.push_back({'P',model,boxContents});return model==0?CONTENTS_LAVA:CONTENTS_WATER;")
    prelude = prelude.replace('return index;', 'if(index<0)throw Drop{};return index;')
    return prelude + '\n' + box.catalog() + box.server_world(
        'clipHandle_t SV_ClipHandleForEntity(', 'struct moveclip_t {',
        'void SV_ClipToEntity(', 'static void SV_ClipMoveToEntities(',
        'void SV_Trace(', 'int SV_PointContents(') + r'''
int main(){
 try{
  for(const auto &entry:profiles){
   profile=entry;owned={};
   for(int n=0;n<4;++n){
    owned[n].s.number=n;owned[n].r.ownerNum=ENTITYNUM_NONE;owned[n].r.contents=CONTENTS_SOLID;
    owned[n].r.mins[0]=owned[n].r.mins[1]=owned[n].r.mins[2]=-8;
    owned[n].r.maxs[0]=owned[n].r.maxs[1]=owned[n].r.maxs[2]=8;
   }
   owned[1].r.bmodel=qtrue;owned[1].s.modelindex=0;
   owned[2].r.bmodel=qtrue;owned[2].s.modelindex=1;
   const auto original=owned;
   const bool et=profile.id==COM_GAME_WOLFET;
   vec3_t start{},end{512,0,0},lo{-1,-1,-1},hi{1,1,1};
   moveclip_t clip{};clip.start=start;VectorCopy(end,clip.end);clip.mins=lo;clip.maxs=hi;
   clip.passEntityNum=0;clip.contentmask=CONTENTS_SOLID;
   clip.trace.fraction=0.5f;clip.trace.entityNum=ENTITYNUM_WORLD;
   events.clear();SV_ClipMoveToEntities(clip);
   int worldSweeps=0,otherSweeps=0;
   for(const auto &e:events)if(e.kind=='T'){if(e.value==0)++worldSweeps;else ++otherSweeps;}
   Check(worldSweeps==(et?0:1)&&otherSweeps==2,"Only ET excludes world-model entity sweeps");
   Check(clip.trace.fraction==(et?0.5f:0.25f),"ET retains the real world's nearest collision");
   Check(clip.trace.entityNum==(et?ENTITYNUM_WORLD:1),"World collision keeps its identity in ET");
   events.clear();const int contents=SV_PointContents(start,0);
   int worldPoints=0,otherPoints=0;
   for(const auto &e:events)if(e.kind=='P'){if(e.value==0)++worldPoints;else ++otherPoints;}
   Check(worldPoints==(et?0:1)&&otherPoints==2,"Only ET excludes world-model entity point queries");
   Check(contents==(CONTENTS_SOLID|CONTENTS_WATER|(et?0:CONTENTS_LAVA)),"World contents are sampled once in ET");
   events.clear();trace_t single;
   SV_ClipToEntity(&single,start,lo,hi,end,1,CONTENTS_SOLID,qfalse);
   Check(events.size()==1&&events[0].kind=='T'&&events[0].value==0,"Explicit single-entity world trace retained");
   Check(single.fraction==0.25f,"Explicit single-entity result retained");
   Check(std::memcmp(owned.data(),original.data(),sizeof(owned))==0,"Queries preserve module records");
   if(et){
    owned[1].s.modelindex=-1;bool rejected=false;
    try{SV_PointContents(start,0);}catch(Drop&){rejected=true;}
    Check(rejected,"Invalid model is rejected rather than treated as the world");
   }
  }
 }catch(const std::exception &e){std::fprintf(stderr,"%s (profile %d)\n",e.what(),int(profile.id));return 1;}
 catch(Drop&){std::fprintf(stderr,"Unexpected rejection\n");return 1;}
 std::puts("World-model exclusions passed for every profile");return 0;
}
'''


class WolfETWorldModelTests(unittest.TestCase):
    def test_production_queries_preserve_real_world_and_other_profiles(self):
        runner = box.EntityBoxContentsTests()
        self.assertIn('World-model exclusions passed', runner.build_and_run('et-world-model-', source()))


if __name__ == '__main__':
    unittest.main()
