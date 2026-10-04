"""Elite Force Holomatch routes to a goal as stvoyHM.exe does.

stvoyHM.exe's AAS_UpdateAreaRoutingCache (0x004992d0) starts every area cache
from the goal area's first row of travel times,
aasworld.areatraveltimes[goal][0] -- the times within the goal from each
link's end to the start of its first reachability -- as RTCW's does, where
Quake III 1.32 starts from zeros. It follows a reversed link into an area
whatever that area's AREA_DISABLED flag. The contents flags it tests
(0x00499290) give a liquid area its liquid's flag alone, and any other area
TFL_AIR with TFL_DONOTENTER, with no team or bridge flags
(docs/fntech3/2026-09-29-stvef-route-seed.md).

The fixtures compile the routing from its source over the goal-link worlds
of tests/aas_goal_links_tests.py and check, under stvef-mp and under every
other profile whose bots route with Quake III's botlib:
- each filler's one link into the goal, or into a portal, costs the start,
  the time inside the area it enters from the link's end to that area's
  first reachability (Holomatch) or nothing (Quake III), and its own time;
- a goal marked AREA_DISABLED is routed to under Holomatch exactly as when it
  is not, and not at all under Quake III;
- every combination of the content bits and the bridge flag either engine
  reads gives each engine's own contents flags.
Each mutation below must make them fail.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

import aas_goal_links_tests as goal_links
import rtcw_route_query_tests as routes
import rtcw_routing_tests as routing
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int);
#endif
static bool holomatch(){return Com_GameIsSTVEFMP()!=0;}
// Every route from another named area to the goal, under each set of travel
// flags. (A route from the goal to itself is always there.)
static std::vector<Route> goalRoutes(const GoalWorld &w){
 std::vector<Route> out;
 for(int flags:FLAGS)for(int a:w.named){if(a==w.goal)continue;int time=-1,reach=0;aasworld.frameroutingupdates=0;
  const int routed=AAS_AreaRouteToGoalArea(a,nullptr,w.goal,flags,&time,&reach);
  out.push_back({routed,routed?time:0,routed?reach:0});}
 return out;
}
// Holomatch enters an area whatever its AREA_DISABLED flag; Quake III 1.32
// reaches no disabled goal. (A route from a cluster portal is read straight
// from the portal cache and answered, with a travel time of 0 when the goal
// cannot be reached.)
static bool reaches(const Route &r){return r.routed&&r.time>0;}
static void disabledGoal(unsigned seed,bool portal){
 initialize();GoalWorld w=goalWorld(seed,4,0,portal);const auto open=goalRoutes(w);shutdown();
 initialize();w=goalWorld(seed,4,0,portal);aasworld.areasettings[w.goal].areaflags|=AREA_DISABLED;
 const auto closed=goalRoutes(w);shutdown();
 int reached=0;for(const auto &r:open)reached+=reaches(r);assert(reached>0);
 if(holomatch()){assert(closed==open);return;}
 for(const auto &r:closed)assert(!reaches(r));
}
// stvoyHM.exe 0x00499290, written out: a liquid's own flag, else air, with
// do-not-enter.
static int holomatchFlags(int contents){
 if(contents&1)return 0x00100000;
 if(contents&4)return 0x00200000;
 if(contents&2)return 0x00400000;
 return 0x00080000|((contents&256)?0x00800000:0);
}
// Quake III 1.32's AAS_GetAreaContentsTravelFlags, written out.
static int quake3Flags(int contents,int areaflags){
 int flags=(contents&1)?TFL_WATER:(contents&4)?TFL_SLIME:(contents&2)?TFL_LAVA:TFL_AIR;
 if(contents&256)flags|=TFL_DONOTENTER;
 if(contents&2048)flags|=TFL_NOTTEAM1;
 if(contents&4096)flags|=TFL_NOTTEAM2;
 if(areaflags&16)flags|=TFL_BRIDGE;
 return flags;
}
static void contentsFlags(){
 static const int bits[]={1,2,4,256,1024,2048,4096};
 initialize();aasworld.numareas=aasworld.numareasettings=2;
 aasworld.areasettings=botlib::ClearedAlloc<aas_areasettings_t>(2);
 for(int mask=0;mask<(1<<7);++mask)for(int areaflags:{0,AREA_BRIDGE,AREA_BRIDGE|AREA_DISABLED}){
  int contents=0;for(int b=0;b<7;++b)if(mask&(1<<b))contents|=bits[b];
  aasworld.areasettings[1].contents=contents;aasworld.areasettings[1].areaflags=areaflags;
  AAS_InitAreaContentsTravelFlags();
  const int expected=Com_GameIsWolfET()?fntech3::rtcw::routing::ContentsFlags(contents):
   holomatch()?holomatchFlags(contents):quake3Flags(contents,areaflags);
  assert(AAS_GetAreaContentsTravelFlags(1)==expected&&AAS_AreaContentsTravelFlags(1)==expected);
 }
 shutdown();
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#ifdef _WIN32
 SetErrorMode(0x0001|0x0002);
#endif
 bool sawHolomatch=false;int checked=0;
 for(int id:quakeRoutingProfiles()){useProfile(id);sawHolomatch|=holomatch();
  for(bool portal:{false,true})for(unsigned seed=1;seed<=3;++seed){
   initialize();GoalWorld w=goalWorld(seed,4,6,portal);fillerRoutes(w);shutdown();
   disabledGoal(seed,portal);
  }
  contentsFlags();++checked;
 }
 assert(sawHolomatch&&checked>1);
 printf("%d profiles\n",checked);
}
'''

# Each mutation replaces text in be_aas_route.cpp and must make the checks fail.
MUTATIONS = {
    'Holomatch starts from zeros': (
        'const bool goalrow = rtcw || wolfet || holomatch;', 'const bool goalrow = rtcw || wolfet;'),
    'every profile starts from the goal row': (
        'const bool goalrow = rtcw || wolfet || holomatch;', 'const bool goalrow = true;'),
    "Elite Force's single player routes as Holomatch": (
        'const bool holomatch = Com_GameIsSTVEFMP() != 0;', 'const bool holomatch = Com_GameIsSTVEF() != 0;'),
    'Holomatch skips a disabled area': (
        'if (!holomatch && (aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED)) continue;',
        'if (aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED) continue;'),
    'no profile skips a disabled area': (
        'if (!holomatch && (aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED)) continue;',
        'if (false) continue;'),
    "Holomatch takes Quake III's contents flags": (
        '\tif (Com_GameIsSTVEFMP())\n\t{\n\t\tif (contents & AREACONTENTS_WATER) return TFL_WATER;',
        '\tif (false)\n\t{\n\t\tif (contents & AREACONTENTS_WATER) return TFL_WATER;'),
    'a Holomatch liquid keeps its do-not-enter flag': (
        '\t\tif (contents & AREACONTENTS_WATER) return TFL_WATER;',
        '\t\tif (contents & AREACONTENTS_WATER) return TFL_WATER | ((contents & AREACONTENTS_DONOTENTER) ? '
        'TFL_DONOTENTER : 0);'),
    'Holomatch tests lava before slime': (
        '\t\tif (contents & AREACONTENTS_SLIME) return TFL_SLIME;\n\t\tif (contents & AREACONTENTS_LAVA) return TFL_LAVA;',
        '\t\tif (contents & AREACONTENTS_LAVA) return TFL_LAVA;\n\t\tif (contents & AREACONTENTS_SLIME) return TFL_SLIME;'),
    'Holomatch loses do-not-enter': (
        'return TFL_AIR | ((contents & AREACONTENTS_DONOTENTER) ? TFL_DONOTENTER : 0);', 'return TFL_AIR;'),
}


def remove_folder(folder):
    # A failed variant's process can hold its executable for a moment after it exits.
    for _ in range(50):
        try:
            shutil.rmtree(folder)
            return
        except PermissionError:
            import time
            time.sleep(0.1)
    shutil.rmtree(folder)


class StvefRouteSeedTests(unittest.TestCase):
    def test_holomatch_routes_from_the_goal_row_into_any_area_by_its_own_flags(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        production = routes.route_source() + routes.GRAPH + goal_links.WORLD + CHECKS
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        folder = Path(tempfile.mkdtemp(prefix='stvef-route-seed-', dir=ROOT / '.tmp'))
        self.addCleanup(remove_folder, folder)

        def build_and_run(name, source):
            cpp, exe = folder / f'{name}.cpp', folder / f'{name}.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-O2', '-ffast-math', '-std=c++17',
                                    '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp),
                                    *routing.RTCWRoutingTests.extra(), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=600)
            if built.returncode:
                return built, None
            return built, subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=300)

        variants = [('production', production)]
        for index, (description, (old, new)) in enumerate(MUTATIONS.items()):
            self.assertEqual(production.count(old), 1, description)
            variants.append((f'mutation{index}', production.replace(old, new)))
        with ThreadPoolExecutor(max_workers=max(1, min(len(variants), os.cpu_count() or 1))) as pool:
            results = list(pool.map(lambda variant: build_and_run(*variant), variants))
        built, ran = results[0]
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        for description, (built, ran) in zip(MUTATIONS, results[1:]):
            with self.subTest(mutation=description):
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                self.assertNotEqual(ran.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
