"""Exercise each renderer's shared skin parser, ownership and RTCW metadata."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
struct shader_t { std::string name; int index; } defaultShader{"default", 0};
enum { LIGHTMAP_NONE = -1, MAX_SKINS = 1024 };
'''

CALLBACKS = r'''
static struct { int numSkins; skin_t *skins[MAX_SKINS]; shader_t *defaultShader; } tr;
static std::vector<std::unique_ptr<unsigned char[]>> allocations;
static std::map<std::string, std::string> files;
static std::map<void *, std::unique_ptr<char[]>> buffers;
static std::map<std::string, std::unique_ptr<shader_t>> shaders;
static int reads, frees, warnings, lookups;
struct Allocation {
    void *pointer;
    template<class T> operator T*() const { return static_cast<T *>(pointer); }
};
static Allocation Allocate(int bytes, ha_pref) {
    assert(bytes > 0); allocations.emplace_back(new unsigned char[bytes]{});
    return {allocations.back().get()};
}
static int ReadFile(const char *name, void **output) {
    ++reads; *output = nullptr; auto found = files.find(name); if (found == files.end()) return -1;
    auto result = std::make_unique<char[]>(found->second.size() + 1);
    std::memcpy(result.get(), found->second.c_str(), found->second.size() + 1);
    *output = result.get(); buffers.emplace(*output, std::move(result)); return (int)found->second.size();
}
static void FreeFile(void *buffer) { assert(buffers.erase(buffer) == 1); ++frees; }
static void QDECL Print(int level, const char *, ...) { if (level == PRINT_WARNING) ++warnings; }
static struct {
    decltype(&Allocate) Hunk_Alloc;
    decltype(&ReadFile) FS_ReadFile;
    decltype(&FreeFile) FS_FreeFile;
    decltype(&Print) Printf;
    comGameId_t gameId;
} ri{Allocate, ReadFile, FreeFile, Print, COM_GAME_QUAKE3};
void Q_strncpyz(char *out, const char *in, int bytes) { assert(bytes > 0); std::strncpy(out, in, bytes - 1); out[bytes - 1] = 0; }
char *Q_strlwr(char *text) { for (char *p = text; *p; ++p) *p = (char)std::tolower((unsigned char)*p); return text; }
int Q_stricmpn(const char *a, const char *b, int n) {
    for (; n > 0; --n, ++a, ++b) {
        const int d = std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
        if (d || !*a) return d;
    }
    return 0;
}
int Q_stricmp(const char *a, const char *b) {
    while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { ++a; ++b; }
    return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
int QDECL Com_sprintf(char *out, int bytes, const char *format, ...) {
    va_list args; va_start(args, format); int n = std::vsnprintf(out, bytes, format, args); va_end(args); return n;
}
static shader_t *R_FindShader(const char *name, int lightmap, qboolean mip) {
    assert(lightmap == LIGHTMAP_NONE && mip); ++lookups;
    auto found = shaders.find(name); if (found != shaders.end()) return found->second.get();
    auto shader = std::make_unique<shader_t>(); shader->name = name; shader->index = (int)shaders.size() + 1;
    auto result = shader.get(); shaders.emplace(name, std::move(shader)); return result;
}
'''


def fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    start = local.index('#define MAX_SKIN_MODELS')
    records = local[start:local.index('} skin_t;', start) + len('} skin_t;')]
    source = PRELUDE + re.search(r'#define MAX_SKIN_SURFACES[^\n]+', local)[0] + '\n' + records + CALLBACKS
    image = (ROOT / 'code' / renderer / 'tr_image.c').read_text()
    source += function(image, 'static char *CommaParse(')
    source += '#include "code/renderercommon/tr_skin.h"\n'
    source += function(image, 'void\tR_InitSkins(')
    source += function(image, 'skin_t\t*R_GetSkinByHandle(')
    return source + r'''
static void Reset(comGameId_t game) {
    assert(buffers.empty()); tr = {}; allocations.clear(); shaders.clear(); files.clear();
    reads = frees = warnings = lookups = 0; tr.defaultShader = &defaultShader; ri.gameId = game; R_InitSkins();
    assert(tr.numSkins == 1 && tr.skins[0]->registered);
}
'''


CHECKS = r'''
int main() {
    for (auto game : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        Reset(game); const bool rtcw = game == COM_GAME_RTCW_SP || game == COM_GAME_RTCW_MP;
        assert(!RE_RegisterSkin(nullptr) && !RE_RegisterSkin("") && !RE_RegisterSkin(std::string(64,'x').c_str()));
        for (int count = 1; count < 5; ++count) {
            auto name = std::make_unique<char[]>(count + 1); std::memset(name.get(), 'a' + count, count); name[count] = 0;
            const auto handle = RE_RegisterSkin(name.get()); assert(handle && tr.skins[handle]->registered);
            assert(tr.skins[handle]->numSurfaces == 1 && !tr.skins[handle]->numModels && tr.skins[handle]->surfaces[0].shader->name == name.get());
            assert(RE_RegisterSkin(name.get()) == handle && reads == 0);
        }
        const int beforeMissing = tr.numSkins;
        assert(!RE_RegisterSkin("missing.skin"));
        assert(tr.numSkins == beforeMissing + (rtcw ? 0 : 1));
        assert(!RE_RegisterSkin("missing.skin") && reads == (rtcw ? 2 : 1));
        files["missing.skin"] = "body,texture/body\n";
        assert((RE_RegisterSkin("missing.skin") != 0) == rtcw);
        files["body.skin"] = "// intro\ntag_head,\n/* block */\n\"Head\",\"texture/head\"\n"
            "md3_part,models/body.mds\nmd3_hat,models/hat.md3\nplayerscale,1.25\nTORSO,texture/torso\n";
        const auto handle = RE_RegisterSkin("body.skin"); assert(handle && tr.skins[handle]->registered);
        const auto *skin = tr.skins[handle]; assert(skin->numSurfaces == (rtcw ? 2 : 5) && skin->numModels == (rtcw ? 2 : 0));
        assert(!std::strcmp(skin->surfaces[0].name, "head") && skin->surfaces[0].shader->name == "texture/head");
        assert(!std::strcmp(skin->surfaces[skin->numSurfaces-1].name, "torso"));
        const int oldReads = reads, oldLookups = lookups;
        assert(RE_RegisterSkin("BODY.SKIN") == handle && reads == oldReads && lookups == oldLookups);
        char output[MAX_QPATH+2], saved[sizeof(output)]; std::memset(output, 0x55, sizeof(output)); std::memcpy(saved, output, sizeof(output));
        assert(RE_GetSkinModel(handle, "md3_part", output + 1) == rtcw);
        if (rtcw) assert(!std::strcmp(output+1,"models/body.mds")); else assert(!std::memcmp(output,saved,sizeof(output)));
        assert(output[0] == 0x55 && output[MAX_QPATH+1] == 0x55);
        assert(RE_GetSkinModel(handle, "PlayerScale", output+1));
        assert(!std::strcmp(output+1, rtcw ? "1.25 1.25 1.25" : "0.00 0.00 0.00"));
        if (rtcw) {
            std::strcpy(output+1,"MD3_HAT"); assert(RE_GetSkinModel(handle, output+1, output+1));
            assert(!std::strcmp(output+1,"models/hat.md3"));
        }
        std::memcpy(saved,output,sizeof(output));
        for (int bad : {INT_MIN,-1,0,tr.numSkins,INT_MAX}) assert(!RE_GetSkinModel(bad,"playerscale",output+1));
        assert(!RE_GetSkinModel(handle,"absent",output+1) && !RE_GetSkinModel(handle,nullptr,output+1) && !RE_GetSkinModel(handle,"playerscale",nullptr));
        assert(!std::memcmp(output,saved,sizeof(output)));
        files["empty.skin"] = "// no surfaces\n"; assert(!RE_RegisterSkin("empty.skin"));
        assert(R_GetSkinByHandle(tr.numSkins-1) == tr.skins[0]);
        files["models/loper/upper.skin"] = "md3_part,models/upper.mdc\n";
        const auto loper = RE_RegisterSkin("models/loper/upper.skin"); assert(loper);
        assert(tr.skins[loper]->numSurfaces == (rtcw ? 0 : 1));
        assert(RE_RegisterSkin("models/loper/upper.skin") == loper);
        files["bad.scale.skin"] = "head,texture/head\nplayerscale,1e300\n";
        auto badScale = RE_RegisterSkin("bad.scale.skin"); assert((badScale == 0) == rtcw);
        if (rtcw) { assert(!tr.skins[tr.numSkins-1]->registered); assert(R_GetSkinByHandle(tr.numSkins-1) == tr.skins[0]); }
        assert(buffers.empty());
    }
    // Enemy Territory reads every skin name as a file, recognises a model or
    // tag entry only by its prefix, and keeps no player scale.
    Reset(COM_GAME_WOLFET);
    assert(!RE_RegisterSkin("plain") && reads == 1 && tr.numSkins == 1);
    assert(!RE_RegisterSkin("missing.skin") && !RE_RegisterSkin("missing.skin") && reads == 3 && tr.numSkins == 1);
    files["body.skin"] = "// intro\ntag_head,\n/* block */\n\"Head\",\"texture/head\"\n"
        "md3_part,models/body.mds\nmd3_hat,models/hat.md3\nplayerscale,1.25\nTORSO,texture/torso\n"
        "body_tag_x,texture/x\nhat_md3_x,texture/y\n";
    {
        const auto handle = RE_RegisterSkin("body.skin"); assert(handle && tr.skins[handle]->registered);
        const auto *skin = tr.skins[handle];
        assert(skin->numSurfaces == 5 && skin->numModels == 2);
        assert(!std::strcmp(skin->surfaces[1].name, "playerscale") && skin->surfaces[1].shader->name == "1.25");
        assert(!std::strcmp(skin->surfaces[3].name, "body_tag_x") && !std::strcmp(skin->surfaces[4].name, "hat_md3_x"));
        char output[MAX_QPATH];
        assert(RE_GetSkinModel(handle, "MD3_HAT", output) && !std::strcmp(output, "models/hat.md3"));
        std::strcpy(output, "untouched");
        assert(!RE_GetSkinModel(handle, "playerscale", output) && !std::strcmp(output, "untouched"));
    }
    files["models/loper/upper.skin"] = "md3_part,models/upper.mdc\n";
    assert(!RE_RegisterSkin("models/loper/upper.skin"));
    assert(buffers.empty());
    Reset(COM_GAME_RTCW_SP);
    std::string content;
    for (int i = 0; i < 300; ++i) content += "md3_"+std::to_string(i)+",models/"+std::to_string(i)+".mds\n";
    for (int i = 0; i < 300; ++i) content += "surface"+std::to_string(i)+",textures/"+std::to_string(i)+"\n";
    files["limits.skin"] = content;
    const auto handle = RE_RegisterSkin("limits.skin"); assert(handle && warnings == 2);
    assert(tr.skins[handle]->numModels == MAX_SKIN_MODELS && tr.skins[handle]->numSurfaces == MAX_SKIN_SURFACES);
    char output[MAX_QPATH]; assert(RE_GetSkinModel(handle,"md3_255",output) && !std::strcmp(output,"models/255.mds"));
    assert(!RE_GetSkinModel(handle,"md3_256",output));
    files["duplicates.skin"]="md3_part,first.md3\nmd3_part,second.md3\nbody,body\n";
    const auto dup = RE_RegisterSkin("duplicates.skin"); assert(RE_GetSkinModel(dup,"md3_part",output) && !std::strcmp(output,"first.md3"));
    files["long.skin"]="md3_part,"+std::string(100,'x')+"\nbody,body\n";
    const auto longName = RE_RegisterSkin("long.skin"); assert(RE_GetSkinModel(longName,"md3_part",output) && std::strlen(output)==63);
    const auto cached = tr.skins[handle];
    tr.numSkins = MAX_SKINS;
    for (int i = longName + 1; i < MAX_SKINS; ++i) tr.skins[i] = tr.skins[0];
    const int oldReads = reads;
    assert(!RE_RegisterSkin("full.skin") && reads == oldReads);
    assert(RE_RegisterSkin("limits.skin") == handle && tr.skins[handle] == cached);
    assert(buffers.empty());
}
'''


class SkinModelTests(unittest.TestCase):
    def test_profile_skin_registration_metadata_and_bounds(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer), tempfile.TemporaryDirectory(prefix='skin-model-', dir=ROOT / '.tmp') as temp:
                folder = Path(temp)
                source, executable = folder / 'probe.cpp', folder / ('probe.exe' if os.name == 'nt' else 'probe')
                source.write_text(fixture(renderer) + CHECKS, encoding='utf-8')
                build = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(executable)],
                    cwd=ROOT, capture_output=True, text=True, timeout=90)
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
