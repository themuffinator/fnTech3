#!/usr/bin/env python3
"""Compare RTCW prediction on installed AAS geometry with real BSP contents.

The actual engine collision sources run in a private fixture library. Both
prediction implementations use that same point-contents service, so this checks
movement over installed geometry, not original-engine BSP collision parity.
Dynamic entities are absent. SP navigation under MP policy is a synthetic MP
corpus, not an installed MP bot claim. Assets stay in memory. Original GPLv3
functions remain only in disposable comparison programs with their notices.
"""
from __future__ import annotations
import argparse
import datetime
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
import probe_rtcw_prediction as prediction
import probe_rtcw_collision as collision
from rtcw_aas_load_tests import loader_source
from rtcw_aas_query_tests import query_source
from rtcw_navigation_tests import navigation_source
from rtcw_prediction_tests import prediction_source, SCENE


COLLISION_EXPORTS = r'''
#ifdef _WIN32
#define PROBE_EXPORT extern "C" __declspec(dllexport)
#else
#define PROBE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
PROBE_EXPORT unsigned ProbeLoad(int sp,const byte *data,unsigned bytes){
    selected=Com_GameManagerFindProfile(sp?"rtcw-sp":"rtcw-mp");
    if(!selected||!data||bytes<144||bytes>128u*1024u*1024u)std::abort();
    CM_ClearMap();arenaUsed=0;std::fill(arena.begin(),arena.end(),0);
    mapData.assign(data,data+bytes);mapName="maps/probe.bsp";int checksum;
    CM_LoadMap(mapName,qfalse,&checksum);return unsigned(checksum);
}
PROBE_EXPORT int ProbeContents(const float *point){return CM_PointContents(point,0);}
PROBE_EXPORT void ProbeClose(){CM_ClearMap();mapData.clear();}
'''

COMPARE = r'''
#define aasworld (*aas_currentWorld)
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <dlfcn.h>
#endif
static int (*worldContents)(const float*);
static uint64_t contentsQueries,liquidQueries;
static int actualContents(vec3_t point){
 ++contentsQueries;int mask=worldContents(point);
 if(mask&(CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA))++liquidQueries;
 return mask;
}
static std::vector<byte> input(){
 byte header[4];assert(fread(header,1,4,stdin)==4);
 unsigned bytes=fntech3::rtcw::routecache::U32(header);
 assert(bytes>=124&&bytes<=128u*1024u*1024u);std::vector<byte> data(bytes);
 assert(fread(data.data(),1,bytes,stdin)==bytes);return data;
}
int main(int argc,char **argv){
 assert(argc==2);profile=&profiles[MODE_SP?2:3];
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);
 auto library=LoadLibraryA(argv[1]);assert(library);
 auto symbol=[&](const char *name){return GetProcAddress(library,name);};
#else
 auto library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!library){fprintf(stderr,"%s\n",dlerror());return 2;}
 auto symbol=[&](const char *name){return dlsym(library,name);};
#endif
 auto load=reinterpret_cast<unsigned(*)(int,const byte*,unsigned)>(symbol("ProbeLoad"));
 worldContents=reinterpret_cast<int(*)(const float*)>(symbol("ProbeContents"));
 auto close=reinterpret_cast<void(*)()>(symbol("ProbeClose"));assert(load&&worldContents&&close);
 const auto aas=input(),bsp=input();const unsigned checksum=load(MODE_SP,bsp.data(),unsigned(bsp.size()));
 initialize();botimport.PointContents=actualContents;
 byte header[124];memcpy(header,aas.data(),124);for(unsigned i=8;i<124;++i)header[i]^=byte((i-8)*119);
 assert(checksum==fntech3::rtcw::routecache::U32(header+8));
 LibVarSet("sv_mapChecksum",std::to_string(checksum).c_str());
 files["maps/probe.aas"]=aas;AAS_InitSettings();
 uint64_t queries=0,differences=0,decisions=0,excluded=0,rawDifferences=0,falseResults=0;
 double maxPositionError=0,maxVelocityError=0,maxFractionError=0;
 std::array<uint32_t,20> firstOurs{},firstTheirs{};int firstArgs[8]{};
 std::array<uint32_t,20> decisionOurs{},decisionTheirs{};int decisionArgs[8]{};
 unsigned random=0x3190401u;auto next=[&](){random=random*1664525u+1013904223u;return random;};
 for(int world=0;world<2;++world){
  assert(AAS_SetCurrentWorld(world)&&AAS_LoadAASFile("maps/probe.aas")==0);
  // This isolated movement fixture needs geometry, not routing caches. The
  // full map/routing initialization sequence has separate runtime evidence.
  aasworld.initialized=1;reference::selectWorld(aas_currentWorld);reference::AAS_InitSettings();
  for(int i=0;i<512;++i){
   int area=1+next()%(aasworld.numareas-1);vec3_t origin;VectorCopy(aasworld.areas[area].center,origin);
   for(int axis=0;axis<3;++axis)origin[axis]+=float(int(next()%17)-8)*.125f;
   vec3_t velocity{float(int(next()%600)-300),float(int(next()%600)-300),float(int(next()%600)-300)};
   vec3_t command{float(int(next()%640)-320),float(int(next()%640)-320),i%3==0?-400.0f:i%3==1?0.0f:400.0f};
   static const int masks[]={0,1,2,4,8,16,32,64,128,256,512,1024,-1};
   static const int lengths[]={0,1,2,8,16,32};static const float times[]={.05f,.1f,.2f};
   int flags=masks[i%13],frames=lengths[i%6],commands=i%5,presence=i%7?PRESENCE_NORMAL:PRESENCE_CROUCH;
   float dt=times[i%3];aas_clientmove_t own;reference::aas_clientmove_t ref;
   int a=AAS_PredictClientMovement(&own,-1,origin,presence,i%2,velocity,command,commands,frames,dt,flags,area,0);
   reference::indeterminatePlane=false;
   int b=reference::AAS_PredictClientMovement(&ref,-1,origin,presence,i%2,velocity,command,commands,frames,dt,flags,area,0);
   fntech3_rtcw_aas_clientmove_t out{};assert(fntech3::rtcw::movement::Result(own,out));
   ++queries;falseResults+=!a;
   bool same=a==b&&!memcmp(&out,&ref,sizeof(out));rawDifferences+=!same;
   if(ORIGINAL_REFERENCE&&reference::indeterminatePlane){++excluded;continue;}
   const bool changedDecision=a!=b||out.stopevent!=ref.stopevent||out.frames!=ref.frames||out.presencetype!=ref.presencetype
     ||out.endcontents!=ref.endcontents||out.trace.startsolid!=ref.trace.startsolid||out.trace.ent!=ref.trace.ent
     ||out.trace.area!=ref.trace.area||out.trace.lastarea!=ref.trace.lastarea||out.trace.planenum!=ref.trace.planenum;
   if(changedDecision){if(!decisions){memcpy(decisionOurs.data(),&out,80);memcpy(decisionTheirs.data(),&ref,80);
     int args[]={world,i,area,flags,frames,commands,a,b};memcpy(decisionArgs,args,sizeof(args));}++decisions;}
   for(int axis=0;axis<3;++axis){
    maxPositionError=std::max(maxPositionError,std::fabs(double(out.endpos[axis])-ref.endpos[axis]));
    maxPositionError=std::max(maxPositionError,std::fabs(double(out.trace.endpos[axis])-ref.trace.endpos[axis]));
    maxVelocityError=std::max(maxVelocityError,std::fabs(double(out.velocity[axis])-ref.velocity[axis]));}
   maxFractionError=std::max(maxFractionError,std::fabs(double(out.trace.fraction)-ref.trace.fraction));
   if(!same){if(!differences){memcpy(firstOurs.data(),&out,80);memcpy(firstTheirs.data(),&ref,80);
     int args[]={world,i,area,flags,frames,commands,a,b};memcpy(firstArgs,args,sizeof(args));}++differences;}
  }
 }
 shutdown();close();
#ifdef _WIN32
 assert(FreeLibrary(library));
#else
 assert(!dlclose(library));
#endif
 printf("{\"bspChecksum\":%u,\"queries\":%llu,\"differences\":%llu,\"decisionDifferences\":%llu,\"excludedIndeterminatePlane\":%llu,\"rawDifferences\":%llu,\"falseResults\":%llu,\"contentsQueries\":%llu,\"liquidQueries\":%llu,\"maxPositionError\":%.9g,\"maxVelocityError\":%.9g,\"maxFractionError\":%.9g,\"firstArgs\":[",
  checksum,(unsigned long long)queries,(unsigned long long)differences,(unsigned long long)decisions,(unsigned long long)excluded,
  (unsigned long long)rawDifferences,(unsigned long long)falseResults,(unsigned long long)contentsQueries,
  (unsigned long long)liquidQueries,maxPositionError,maxVelocityError,maxFractionError);
 for(int i=0;i<8;++i)printf("%s%d",i?",":"",firstArgs[i]);
 printf("],\"firstEngineWords\":[");for(int i=0;i<20;++i)printf("%s%u",i?",":"",firstOurs[i]);
 printf("],\"firstReferenceWords\":[");for(int i=0;i<20;++i)printf("%s%u",i?",":"",firstTheirs[i]);
 printf("],\"firstDecisionArgs\":[");for(int i=0;i<8;++i)printf("%s%d",i?",":"",decisionArgs[i]);
 printf("],\"firstDecisionEngineWords\":[");for(int i=0;i<20;++i)printf("%s%u",i?",":"",decisionOurs[i]);
 printf("],\"firstDecisionReferenceWords\":[");for(int i=0;i<20;++i)printf("%s%u",i?",":"",decisionTheirs[i]);printf("]}\n");
}
'''


def fixture(base, variant):
    source, provenance = prediction.fixture(base, variant)
    loaded = loader_source().replace('void AAS_InitSettings(){++settingsLoads;}', 'void AAS_InitSettings();')
    loaded = loaded.replace('void QDECL AAS_Error(const char*,...){}', '')
    loaded += query_source().removeprefix(navigation_source())
    loaded += prediction_source().removeprefix(query_source())
    source = source.replace(prediction_source() + SCENE, loaded, 1)
    return source.removesuffix(prediction.COMPARE) + COMPARE, provenance


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_collision(folder, compiler, target, flags, msvc=False):
    source = (ROOT / 'tests/fixtures/rtcw_collision_probe.cpp').read_text().split('int main(', 1)[0] + COLLISION_EXPORTS
    bridge = folder / 'collision-bridge.cpp'; bridge.write_text(source, encoding='utf-8')
    c_compiler = str(Path(compiler).with_name(Path(compiler).name.replace('clang++', 'clang')))
    objects, commands = [], []
    for name in collision.SOURCES[:-1] + [str(bridge)]:
        path = ROOT / name; obj = folder / (path.stem + ('.obj' if msvc else '.o'))
        overrides = [] if path.name == 'game_profile.cpp' else [
            '-DCom_GameProfile=FnFixture_GameProfile', '-DCom_GameBspFormat=FnFixture_GameBspFormat']
        if msvc:
            command = [compiler, '/nologo', '/MT', '/O2', '/EHsc',
                '/std:c++17' if path.suffix == '.cpp' else '/std:c11',
                '/I'+str(ROOT), '/I'+str(ROOT/'code/qcommon'), *flags, *overrides,
                '/c', str(path), '/Fo'+str(obj)]
        else:
            command = [compiler if path.suffix == '.cpp' else c_compiler, *target, *flags,
                '-std=c++17' if path.suffix == '.cpp' else '-std=c11', '-O2',
                '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'), *overrides, '-c', str(path), '-o', str(obj)]
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        commands.append(dict(command=command, exitCode=run.returncode, output=run.stdout+run.stderr))
        if run.returncode:
            return None, commands
        objects.append(str(obj))
    library = folder / ('collision.dll' if os.name == 'nt' else 'collision.so')
    command = ([compiler, '/nologo', '/MT', '/LD', *objects, '/Fe'+str(library),
        '/link', '/IMPLIB:'+str(folder/'collision.lib')] if msvc else
        [compiler, *target, *flags, '-shared', *objects, '-o', str(library)])
    run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
    commands.append(dict(command=command, exitCode=run.returncode, output=run.stdout+run.stderr))
    return (None if run.returncode else library), commands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('sp-source', 'mp-source', 'iortcw-source', 'installation', 'report'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--asset', default='*', help='AAS member glob; default all installed AAS files')
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--float-mode', choices=('strict','fast'), default='strict')
    parser.add_argument('--float-unit', choices=('default','sse2'), default='default')
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args(); compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++ compiler unavailable')
    msvc = Path(compiler).name.lower() in ('cl', 'cl.exe')
    output = args.report.resolve(); output.relative_to(ROOT / '.tmp'); output.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target='+args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    if msvc:
        triple = {'x86':'i686-pc-windows-msvc','x64':'x86_64-pc-windows-msvc'}.get(os.environ.get('VSCMD_ARG_TGT_ARCH'))
        if not triple or (args.target and args.target != triple):
            parser.error('Select the matching MSVC x86/x64 developer environment before this probe')
        if args.sanitize:
            parser.error('This probe uses Clang for sanitizer builds')
    else:
        triple = subprocess.check_output([compiler,*target,'-dumpmachine'],cwd=ROOT,text=True).strip()
    flags = ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    if triple.startswith('x86_64'):
        flags += ['-DFNTECH3_Q3_ONLY']
    if args.float_unit == 'sse2':
        flags += (['/arch:SSE2'] if triple.startswith('i686') else []) if msvc else ['-msse2','-mfpmath=sse']
    arithmetic = (['/fp:fast'] if args.float_mode == 'fast' else ['/fp:precise']) if msvc else (
        ['-ffast-math'] if args.float_mode == 'fast' else ['-fno-fast-math','-ffp-contract=off'])
    env = os.environ.copy()
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    files = collision.SOURCES + collision.HEADERS + ['scripts/probe_rtcw_prediction_maps.py',
        'scripts/probe_rtcw_prediction.py','tests/rtcw_prediction_tests.py','tests/rtcw_aas_load_tests.py',
        'code/botlib/be_aas_move.cpp','code/botlib/be_aas_sample.cpp','code/botlib/be_aas_file.cpp',
        'code/botlib/rtcw_aas_loader.hpp','code/botlib/rtcw_movement.hpp','code/compat/rtcw_prediction.hpp']
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,target=triple,compiler=compiler,
        sanitized=args.sanitize,floatMode=args.float_mode,floatUnit=args.float_unit,assetFilter=args.asset,
        implementationSha256={name:sha(ROOT/name) for name in files},results=[])
    archive_paths = sorted((args.installation.resolve(strict=True)/'Main').glob('*.pk3'))
    # Resolve the installed overlay without extracting or modifying any asset.
    members = {}
    for archive in archive_paths:
        with zipfile.ZipFile(archive) as pak:
            for entry in pak.infolist():
                if entry.filename.lower().endswith(('.aas','.bsp')):
                    members[entry.filename.lower()] = (archive, entry.filename, entry.file_size)
    selected = [name for name in sorted(members) if name.endswith('.aas') and fnmatch.fnmatchcase(name,args.asset)]
    if not selected:
        parser.error('No installed AAS members match the requested filter')

    def read(member):
        archive, name, size = members[member]
        if size > 128*1024*1024:
            parser.error('Installed fixture exceeds size limit: '+member)
        with zipfile.ZipFile(archive) as pak:
            return pak.read(name)

    with tempfile.TemporaryDirectory(prefix='prediction-maps-',dir=output.parent) as temp:
        folder = Path(temp)
        library, commands = build_collision(folder,compiler,target,flags+([] if os.name=='nt' else ['-fPIC','-fvisibility=hidden']),msvc)
        report['collisionBuild'] = commands
        output.write_text(json.dumps(report,indent=2)+'\n')
        if library is None:
            print(commands[-1]['output']); return 1
        report['collisionLibrarySha256'] = sha(library)
        for variant,base in [('originalSP',args.sp_source/'src'),('originalMP',args.mp_source/'src'),
                            ('iortcwSP',args.iortcw_source/'SP/code'),('iortcwMP',args.iortcw_source/'MP/code')]:
            source, provenance = fixture(base,variant)
            cpp=folder/(variant+'.cpp'); exe=folder/(variant+'.exe');cpp.write_text(source,encoding='utf-8')
            row=dict(reference=variant,sources=provenance,fixtureSha256=hashlib.sha256(source.encode()).hexdigest(),
                agreementRequired=variant.startswith('original') and args.float_mode=='strict',assets=[])
            report['results'].append(row)
            sources=[str(cpp),str(ROOT/'code/server/server_game_data.cpp'),str(ROOT/'code/compat/game_abi_adapter.c')]
            if msvc:
                command=[compiler,'/nologo','/MT','/EHsc','/std:c++17','/O2',*flags,*arithmetic,
                    '/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT),f'/DMODE_SP={int(variant.endswith("SP"))}',
                    f'/DORIGINAL_REFERENCE={int(variant.startswith("original"))}',
                    '/TP',*sources,'/Fo'+str(folder)+os.sep,'/Fe'+str(exe)]
            else:
                command=[compiler,*target,*flags,'-std=c++17','-O2',*arithmetic,'-D_CRT_SECURE_NO_WARNINGS',
                    '-I',str(ROOT),f'-DMODE_SP={int(variant.endswith("SP"))}',
                    f'-DORIGINAL_REFERENCE={int(variant.startswith("original"))}',*sources[:2],'-x','c++',sources[2],
                    *([] if os.name=='nt' else ['-ldl']),'-o',str(exe)]
            built=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
            row.update(compileCommand=command,buildExitCode=built.returncode,buildOutput=built.stdout+built.stderr)
            if not built.returncode:
                for member in selected:
                    bsp_member=member.rsplit('_b',1)[0]+'.bsp'
                    if bsp_member not in members:
                        parser.error('Missing corresponding BSP: '+bsp_member)
                    aas,bsp=read(member),read(bsp_member)
                    asset=dict(aasMember=member,aasArchive=str(members[member][0]),aasSha256=hashlib.sha256(aas).hexdigest(),
                        bspMember=bsp_member,bspArchive=str(members[bsp_member][0]),bspSha256=hashlib.sha256(bsp).hexdigest())
                    row['assets'].append(asset)
                    data=struct.pack('<I',len(aas))+aas+struct.pack('<I',len(bsp))+bsp
                    options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
                    run=subprocess.run([str(exe),str(library)],cwd=ROOT,input=data,capture_output=True,timeout=120,env=env,**options)
                    asset.update(exitCode=run.returncode,stderr=run.stderr.decode(errors='replace'))
                    if not run.returncode:
                        asset.update(json.loads(run.stdout))
                    else:
                        asset['stdout']=run.stdout.decode(errors='replace')
                    output.write_text(json.dumps(report,indent=2)+'\n')
            row['executed']=not built.returncode and len(row['assets'])==len(selected) and all(a['exitCode']==0 for a in row['assets'])
            row['agrees']=row['executed'] and all(a['differences']==0 for a in row['assets'])
            output.write_text(json.dumps(report,indent=2)+'\n')
            totals={key:sum(a.get(key,0) for a in row['assets']) for key in
                ('queries','differences','decisionDifferences','excludedIndeterminatePlane','liquidQueries')}
            print(variant,'AGREES' if row['agrees'] else 'DIFFERS' if row['executed'] else 'ERROR',totals,flush=True)
            if built.returncode:
                print(row['buildOutput'],flush=True)
    return 0 if all(row['executed'] and (row['agrees'] or not row['agreementRequired']) for row in report['results']) else 1


if __name__=='__main__':
    raise SystemExit(main())
