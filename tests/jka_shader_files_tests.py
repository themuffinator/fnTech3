"""Execute each renderer's shader collection with isolated filesystem records."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function
from rtcw_shader_text_tests import PRELUDE, tokenizer


ROOT = Path(__file__).resolve().parents[1]

COLLECTION = r'''
static std::map<std::string,std::vector<std::string>> lists;
static std::vector<std::string> listCalls;
static std::vector<char *> hunks;
static bool extended;
static int liveLists;
static char **List(const char *directory,const char *extension,int *count) {
    const std::string key=std::string(directory)+extension;
    listCalls.push_back(key);
    const auto &names=lists[key];*count=int(names.size());
    if(names.empty())return nullptr;
    auto out=new char *[names.size()+1];++liveLists;
    for(size_t i=0;i<names.size();++i) {
        out[i]=new char[names[i].size()+1];
        std::strcpy(out[i],names[i].c_str());
    }
    out[names.size()]=nullptr;return out;
}
static void FreeList(char **names) {
    assert(names && liveLists>0);
    for(size_t i=0;names[i];++i)delete[] names[i];
    delete[] names;--liveLists;
}
static char *Hunk(int size,int) {
    assert(size>0);auto p=new char[size]();hunks.push_back(p);return p;
}
static void *Allocate(int size) { assert(size>0);++allocations;return new char[size]; }
static char *Q_stradd(char *out,const char *in) { std::strcpy(out,in);return out+std::strlen(in); }
static bool GL_ProgramAvailable() { return extended; }
static unsigned int generateHashValue(const char *name,unsigned int count) {
    unsigned int value=0;for(;*name;++name)value=value*33+static_cast<unsigned char>(*name);
    return value%count;
}
enum { MAX_SHADER_FILES=3,MAX_SHADERTEXT_HASH=16 };
static char *s_shaderText;
static const char *s_extensionOffset;
static const char **shaderTextHashTable[MAX_SHADERTEXT_HASH];
static struct {
    decltype(&Read) FS_ReadFile=Read;decltype(&::Free) FS_FreeFile=::Free;
    decltype(&Print) Printf=Print;decltype(&Error) Error=::Error;
    comGameId_t gameId=COM_GAME_QUAKE3;
    decltype(&List) FS_ListFiles=List;decltype(&FreeList) FS_FreeFileList=FreeList;
    decltype(&Hunk) Hunk_Alloc=Hunk;decltype(&Allocate) Malloc=Allocate;
    decltype(&::Free) Free=::Free;
}ri;
'''

CHECKS = r'''
static const char *Find(const char *name) {
    const auto hash=generateHashValue(name,MAX_SHADERTEXT_HASH);
    for(auto p=shaderTextHashTable[hash];*p;++p) {
        const char *text=*p;
        if(!std::strcmp(COM_ParseExt(&text,qtrue),name)) {
            const char *value=*p;
            while(*value && static_cast<unsigned char>(*value)<=32)++value;
            return value;
        }
    }
    return nullptr;
}
static void Clear() {
    assert(allocations==0 && liveLists==0);
    for(auto p:hunks)delete[] p;
    hunks.clear();files.clear();lists.clear();listCalls.clear();
    std::memset(shaderTextHashTable,0,sizeof(shaderTextHashTable));
    s_shaderText=nullptr;s_extensionOffset=nullptr;
}
int main() {
    for(auto profile:{COM_GAME_JKA_SP,COM_GAME_JKA_MP,COM_GAME_QUAKE3,
        COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_JK2_MP}) {
        for(bool programs:{false,true}) {
            Clear();ri.gameId=profile;extended=programs;
            lists["shaders.shader"]={"shared.shader","broken.shader","only.shader","clipped.shader"};
            lists["scripts.shader"]={"shared.shader","fnql.shader"};
            lists["scripts.shaderx"]={"extended.shaderx"};
            files["shaders/shared.shader"]="duplicate { { map authored } }";
            files["shaders/broken.shader"]="broken { { map no-close }";
            files["shaders/only.shader"]="profile-only { { map retail } }";
            // No clipped.shader entry: reading beyond the cap throws.
            files["scripts/shared.shader"]="duplicate { { map supplement } }";
            files["scripts/fnql.shader"]="engine-owned { { map fn } }";
            files["scripts/extended.shaderx"]="extended-only { { map fnx } }";
            ScanAndLoadShaderFiles();
            assert(allocations==0 && liveLists==0);
            const bool jka=profile==COM_GAME_JKA_SP || profile==COM_GAME_JKA_MP;
            assert(bool(Find("profile-only"))==jka && !Find("broken"));
            assert(Find("engine-owned") && bool(Find("extended-only"))==programs);
            const auto duplicate=Find("duplicate");
            assert(duplicate);
            const char *body=duplicate;
            for(int i=0;i<4;++i)COM_ParseExt(&body,qtrue);
            assert(!std::strcmp(COM_ParseExt(&body,qtrue),"supplement"));
            assert(duplicate<s_extensionOffset);
            if(programs)assert(Find("extended-only")>=s_extensionOffset);
            assert((std::find(listCalls.begin(),listCalls.end(),"shaders.shader")!=listCalls.end())==jka);
        }
    }
    // Profile-only installations still load, without engine supplements.
    Clear();ri.gameId=COM_GAME_JKA_MP;extended=false;
    lists["shaders.shader"]={"only.shader"};
    files["shaders/only.shader"]="profile-only { }";
    ScanAndLoadShaderFiles();assert(Find("profile-only"));
    Clear();ScanAndLoadShaderFiles();assert(!s_shaderText && hunks.empty());
    Clear();return 0;
}
'''


class JkaShaderFilesTests(unittest.TestCase):
    def test_profile_files_supplements_bounds_and_cleanup(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        # Retain the existing loader harness's real tokenizer and read/free
        # tracking, then supply the complete renderer collection imports.
        base = PRELUDE[:PRELUDE.index("static struct {")] + "\n#include <algorithm>\n" + COLLECTION
        base += tokenizer()
        for header in ("tr_rtcw_shader_text.h", "tr_q3_ihv_shader_text.h", "tr_q3test_shader_text.h"):
            base += '\n#include "code/renderercommon/' + header + '"\n'
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="jka-shader-files-", dir=scratch) as folder:
            for backend in ("renderer", "renderervk", "rendererrtx"):
                with self.subTest(backend=backend):
                    source = (ROOT / "code" / backend / "tr_shader.c").read_text(encoding="utf-8")
                    fixture = Path(folder) / "collection.cpp"
                    # One executable per build: Windows can hold the last one run.
                    name = "collection-" + backend
                    executable = Path(folder) / (name + ".exe" if os.name == "nt" else name)
                    body = function(source, "static int loadShaderBuffers(")
                    body += function(source, "static void ScanAndLoadShaderFiles(")
                    checks = CHECKS
                    if backend != "renderer":
                        # Vulkan and RTX always admit engine shaderx files.
                        checks = checks.replace("extended=programs;", "extended=programs;programs=true;")
                    fixture.write_text(base + body + checks, encoding="utf-8")
                    build = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17",
                        "-O2", "-I", str(ROOT), str(fixture), "-o", str(executable)], cwd=ROOT,
                        text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
