"""Execute Academy's distance policy and all three production renderer users."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

POLICY = r'''
#include "code/renderercommon/tr_jka_distance_cull.h"
#include <assert.h>
#include <math.h>
int main(void) {
    float value = 19.0f, invalid;
    uint32_t bits;
    const char *bad[] = {"", "zero", "0", "-1", "0.5", "nan", "inf", "-inf",
        "1e90", "1e-90", "2000x", "2000 3000", "}"};
    unsigned i;
    assert(FN_JKA_DEFAULT_DISTANCE_CULL == 6000.0f);
    assert(FnJKA_ParseDistanceCull(" 1.8e3 \t", &value) && value == 1800.0f);
    assert(FnJKA_ParseDistanceCull("10240", &value) && value == 10240.0f);
    errno = EDOM;
    for (i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        assert(!FnJKA_ParseDistanceCull(bad[i], &value) && value == 10240.0f && errno == EDOM);
    }
    assert(!FnJKA_ParseDistanceCull(NULL, &value));
    assert(!FnJKA_ParseDistanceCull("6000", NULL));
    for (bits = UINT32_C(0x7f800000); bits < UINT32_C(0x7f800010); ++bits) {
        memcpy(&invalid, &bits, sizeof(invalid)); assert(!FnJKA_DistanceCullValid(invalid));
    }
    bits = UINT32_C(0xff800000); memcpy(&invalid, &bits, sizeof(invalid));
    assert(!FnJKA_DistanceCullValid(invalid));
    assert(FnJKA_FarClip(100.0f, 6000.0f, 4.0f) == 100.0f);
    assert(fabsf(FnJKA_FarClip(9000.0f, 1800.0f, 4.0f) - 3117.6914f) < 0.01f);
    assert(FnJKA_FarClip(9000.0f, 1.0f, 4.0f) == 5.0f);
    assert(FnJKA_ParseDistanceCull("1", &value) && value == 1.0f);
    return 0;
}
'''

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#include "code/renderercommon/tr_jka_distance_cull.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
struct Drop {};
struct world_t { float lightGridSize[3]; char *entityString; const char *entityParsePoint; };
static world_t s_worldData;
static float s_jkaDistanceCull;
static qboolean s_jkaDistanceCullReady;
static struct {
    world_t *world; qboolean registered;
    struct { int rdflags; struct { float farClip; } rtcwFog; } refdef;
    struct { float zFar; vec3_t visBounds[2]; struct { vec3_t origin; } or; } viewParms;
} tr;
enum { RDF_NOWORLDMODEL = 1 };
struct lump_t { int fileofs, filelen; };
static byte entityBytes[] = "{}";
static byte *fileBase = entityBytes;
static cvar_t nearValue{}, vertexValue{};
static cvar_t *r_znear = &nearValue, *r_vertexLight = &vertexValue;
static std::vector<std::string> tokens;
static size_t nextToken;
const char *COM_ParseExt(const char **, qboolean) {
    return nextToken < tokens.size() ? tokens[nextToken++].c_str() : "";
}

const char *COM_Parse(const char **cursor) {
    static std::string token;
    token.clear(); const char *p=*cursor;
    if(!p)return "";
    while(*p==' '||*p=='\n')++p;
    if(!*p){*cursor=nullptr;return "";}
    if(*p=='{'||*p=='}'){token+=*p++;}
    else if(*p=='"'){++p;while(*p&&*p!='"')token+=*p++;if(*p)++p;}
    else {while(*p&&*p!=' '&&*p!='\n')token+=*p++;}
    *cursor=p;return token.c_str();
}

static char entityOwner[4096];
static char *Allocate(int length, int) { assert(length < 4096); return entityOwner; }
static void Error(errorParm_t, const char *, ...) { throw Drop{}; }
static void Print(printParm_t, const char *, ...) {}
static struct { int gameId; decltype(&Allocate) Hunk_Alloc = Allocate;
    decltype(&Error) Error = ::Error; decltype(&Print) Printf = Print; } ri;
int Q_stricmp(const char *a, const char *b) {
    while (*a && *b) { int x=*a++, y=*b++; if(x>='A'&&x<='Z')x+='a'-'A';
        if(y>='A'&&y<='Z')y+='a'-'A'; if(x!=y)return x-y; } return *a-*b;
}
int Q_strncmp(const char *a, const char *b, int size) { return std::strncmp(a,b,size); }
void Q_strncpyz(char *out, const char *in, int size) {
    if(!out || size<=0)throw Drop{};
    size_t count=std::strlen(in);if(count>=static_cast<size_t>(size))count=size-1;
    std::memcpy(out,in,count);out[count]=0;
}
static void RE_RemapShader(const char *, const char *, const char *) {}
static void R_AliceSetSunFlare(const float *) {}
static void R_AliceSetSunFlareInPortalSky() {}
static void R_MOHAABeginSunKeys() {}
static bool R_MOHAASunKey(const char *, const char *) { return false; }
static void R_MOHAAEndSunKeys() {}
static void Com_Split(char *, char **, int, char) {}
float Q_atof(const char *) { return 0; }
'''

CHECKS = r'''
int main() {
    lump_t lump{0,3}; float output=17;
    auto load=[&](int game, std::initializer_list<std::string> supplied) {
        ri.gameId=game; tokens=supplied; nextToken=0; R_LoadEntities(&lump);
    };
    load(COM_GAME_JKA_MP,{"{","classname","worldspawn","}"});
    assert(s_jkaDistanceCull==6000 && s_worldData.lightGridSize[2]==128);
    tr.world=&s_worldData; tr.registered=qtrue; s_jkaDistanceCullReady=qfalse;
    assert(!RE_GetJKADistanceCull(&output) && output==17);
    s_jkaDistanceCullReady=qtrue;
    assert(RE_GetJKADistanceCull(&output) && output==6000);
    load(COM_GAME_JKA_MP,{"{","distanceCull","1850","}"});
    assert(RE_GetJKADistanceCull(&output) && output==1850);
    load(COM_GAME_JKA_MP,{"{","DISTANCECULL","7000","distanceCull","6300","}"});
    assert(RE_GetJKADistanceCull(&output) && output==6300);
    load(COM_GAME_JKA_MP,{"{","classname","worldspawn","}","{","distanceCull","10","}"});
    assert(RE_GetJKADistanceCull(&output) && output==6000);
    for(const char *bad:{"0","-2","nan","inf","1e90","2000 trailing","","}"}) {
        s_jkaDistanceCullReady=qfalse;
        bool dropped=false; try { load(COM_GAME_JKA_MP,{"{","distanceCull",bad,"}"}); } catch(Drop) { dropped=true; }
        assert(dropped && !RE_GetJKADistanceCull(&output) && output==6000);
    }
    for(int game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,
            COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP,COM_GAME_JK2_SP,COM_GAME_JK2_MP,COM_GAME_JKA_SP,
            COM_GAME_QUAKE3_IHV,COM_GAME_Q3TEST,COM_GAME_ALICE,COM_GAME_FAKK2,COM_GAME_MOHAA}) {
        load(game,{"{","distanceCull","invalid","}"});
        assert(s_jkaDistanceCull==0 && !RE_GetJKADistanceCull(&output) && output==6000);
    }
    load(COM_GAME_JKA_MP,{"{","distanceCull","1800","}"});
    s_jkaDistanceCullReady=qtrue;
    tr.registered=qfalse; assert(!RE_GetJKADistanceCull(&output) && output==6000);
    tr.registered=qtrue; tr.world=nullptr; assert(!RE_GetJKADistanceCull(&output));
    world_t other{}; tr.world=&other; assert(!RE_GetJKADistanceCull(&output));
    tr.world=&s_worldData; assert(!RE_GetJKADistanceCull(nullptr));
    assert(RE_GetJKADistanceCull(&output) && output==1800);
    nearValue.value=4; VectorClear(tr.viewParms.or.origin);
    VectorSet(tr.viewParms.visBounds[0],-20000,-20000,-20000);
    VectorSet(tr.viewParms.visBounds[1],20000,20000,20000);
    R_SetFarClip(); assert(fabsf(tr.viewParms.zFar-3117.6914f)<0.01f);
    VectorSet(tr.viewParms.visBounds[0],-10,-10,-10);
    VectorSet(tr.viewParms.visBounds[1],10,10,10);
    R_SetFarClip(); assert(fabsf(tr.viewParms.zFar-17.3205f)<0.01f);
    tr.refdef.rdflags=RDF_NOWORLDMODEL; R_SetFarClip(); assert(tr.viewParms.zFar==2048);
    tr.refdef.rdflags=0;
    VectorSet(tr.viewParms.visBounds[0],-20000,-20000,-20000);
    VectorSet(tr.viewParms.visBounds[1],20000,20000,20000);
    for(int game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP,COM_GAME_JK2_SP,COM_GAME_JK2_MP,
            COM_GAME_JKA_SP,COM_GAME_QUAKE3_IHV,COM_GAME_Q3TEST,COM_GAME_ALICE,COM_GAME_FAKK2,COM_GAME_MOHAA}) {
        ri.gameId=game; R_SetFarClip(); assert(fabsf(tr.viewParms.zFar-34641.0156f)<0.02f);
    }
    for(int game:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
        ri.gameId=game; tr.refdef.rtcwFog.farClip=8000; R_SetFarClip(); assert(tr.viewParms.zFar==8000);
        tr.refdef.rtcwFog.farClip=0; R_SetFarClip(); assert(fabsf(tr.viewParms.zFar-34641.0156f)<0.02f);
    }

    ri.gameId=COM_GAME_JKA_MP; tr.world=&s_worldData; tr.registered=qtrue;s_jkaDistanceCullReady=qtrue;
    std::strcpy(entityOwner,"{ \"classname\" \"worldspawn\" }");
    s_worldData.entityString=entityOwner;s_worldData.entityParsePoint=nullptr;
    assert(RE_GetEntityToken(nullptr,-1) && s_worldData.entityParsePoint==entityOwner);
    char buffer[24];std::memset(buffer,0xa5,sizeof(buffer));
    assert(RE_GetEntityToken(buffer+3,16)&&!std::strcmp(buffer+3,"{"));
    assert(buffer[2]==char(0xa5)&&buffer[19]==char(0xa5));
    assert(RE_GetEntityToken(buffer+3,16)&&!std::strcmp(buffer+3,"classname"));
    assert(RE_GetEntityToken(buffer+3,16)&&!std::strcmp(buffer+3,"worldspawn"));
    assert(RE_GetEntityToken(buffer+3,16)&&!std::strcmp(buffer+3,"}"));
    assert(!RE_GetEntityToken(buffer+3,16)&&!buffer[3]&&!s_worldData.entityParsePoint);
    assert(!RE_GetEntityToken(buffer+3,16)&&!s_worldData.entityParsePoint);
    assert(RE_GetEntityToken(reinterpret_cast<char *>(1),-1)); // ignored reset address
    assert(RE_GetEntityToken(buffer+3,1)&&!buffer[3]);
    for(int size:{-2,0}){bool dropped=false;try{RE_GetEntityToken(buffer+3,size);}catch(Drop){dropped=true;}assert(dropped);}
    bool dropped=false;try{RE_GetEntityToken(nullptr,16);}catch(Drop){dropped=true;}assert(dropped);
    s_jkaDistanceCullReady=qfalse;auto cursor=s_worldData.entityParsePoint;dropped=false;
    try{RE_GetEntityToken(nullptr,-1);}catch(Drop){dropped=true;}assert(dropped&&cursor==s_worldData.entityParsePoint);
    s_jkaDistanceCullReady=qtrue;
    // The existing Quake III/QL/etc EOF rewind remains intact.
    for(int game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,
            COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP,COM_GAME_JKA_SP,COM_GAME_JK2_SP,COM_GAME_JK2_MP,
            COM_GAME_QUAKE3_IHV,COM_GAME_Q3TEST,COM_GAME_ALICE,COM_GAME_FAKK2,COM_GAME_MOHAA}){
        ri.gameId=game;s_worldData.entityParsePoint=nullptr;
        assert(!RE_GetEntityToken(buffer+3,16)&&s_worldData.entityParsePoint==entityOwner);
        assert(RE_GetEntityToken(buffer+3,16)&&!std::strcmp(buffer+3,"{"));
    }
    return 0;
}
'''


class JKADistanceCullTests(unittest.TestCase):
    def compile_and_run(self, source, suffix, flags):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C/C++ compiler required')
        parent = ROOT / '.tmp/tests/jka-distance-cull'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temp:
            folder = Path(temp)
            probe, executable = folder / ('probe' + suffix), folder / 'probe.exe'
            probe.write_text(source)
            language = ['-x', 'c', '-std=c11'] if suffix == '.c' else ['-std=c++17', '-fno-operator-names']
            build = subprocess.run([compiler, *compiler_target_args(compiler), *language,
                '-Wall', '-Wextra', '-Werror', *flags, '-I', str(ROOT), str(probe), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_policy_in_c_and_cpp_with_fast_math(self):
        for suffix in ('.c', '.cpp'):
            for flags in ([], ['-O2', '-ffast-math']):
                with self.subTest(suffix=suffix, flags=flags):
                    self.compile_and_run(POLICY, suffix, flags)

    def test_actual_worldspawn_getter_and_far_plane_in_all_renderers(self):
        for renderer in RENDERERS:
            bsp = (ROOT / f'code/{renderer}/tr_bsp.c').read_text()
            main = (ROOT / f'code/{renderer}/tr_main.c').read_text()
            source = '\n'.join((PRELUDE, function(bsp, 'static void R_LoadEntities('),
                function(bsp, 'qboolean RE_GetJKADistanceCull('), function(bsp, 'qboolean RE_GetEntityToken('),
                function(main, 'static void R_SetFarClip('), CHECKS))
            for flags in ([], ['-O2', '-ffast-math']):
                with self.subTest(renderer=renderer, flags=flags):
                    self.compile_and_run(source, '.cpp', flags)


if __name__ == '__main__':
    unittest.main()
