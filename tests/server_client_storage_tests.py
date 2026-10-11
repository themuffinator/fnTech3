"""Exercise real client allocation/resizing with a bounded process stack."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include "code/server/server.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#undef assert
#define assert(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
server_t sv{};
serverStatic_t svs{};
static cvar_t maximum{};
cvar_t *sv_maxclients=&maximum;
static comGameProfile_t profile{};
const comGameProfile_t *Com_GameProfile() { return &profile; }
struct Drop {};
void QDECL Com_Error(errorParm_t,const char *,...) { throw Drop{}; }
static std::unordered_set<void *> zone,temporary;
void *Z_TagMalloc(int bytes,memtag_t tag) {
    assert(bytes>0 && tag==TAG_CLIENTS);
    void *p=std::malloc(bytes); assert(p); std::memset(p,0xa5,bytes); zone.insert(p); return p;
}
void Z_Free(void *p) { assert(zone.erase(p)==1); std::free(p); }
void *Hunk_AllocateTempMemory(int bytes) {
    assert(bytes>0); void *p=std::malloc(bytes); assert(p);
    std::memset(p,0xa5,bytes); temporary.insert(p); return p;
}
void Hunk_FreeTempMemory(void *p) { assert(temporary.erase(p)==1); std::free(p); }
cvar_t *Cvar_Get(const char *name,const char *value,int flags) {
    assert(!std::strcmp(name,"sv_maxclients") && value==profile.server.clients && flags==0);
    return sv_maxclients;
}
void Cvar_SetIntegerValue(const char *name,int value) {
    assert(!std::strcmp(name,"sv_maxclients")); maximum.integer=value; maximum.modified=qtrue;
}
FUNCTIONS
static std::vector<byte> Bytes(const client_t &client) {
    auto *first=reinterpret_cast<const byte *>(&client);
    return std::vector<byte>(first,first+sizeof(client));
}
static void Resize(int requested,int expected) {
    maximum.integer=requested; maximum.modified=qtrue;
    SV_ChangeMaxClients(); assert(sv.maxclients==expected && maximum.integer==expected && !maximum.modified);
    assert(zone.size()==1 && temporary.empty());
    assert(svs.numSnapshotEntities==PACKET_BACKUP*MAX_GENTITIES);
}
int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#else
    const rlimit limit{256*1024,256*1024}; assert(!setrlimit(RLIMIT_STACK,&limit));
#endif
    profile.server.clients="4";
    std::printf("client_t: %zu bytes; stack limit: 256 KiB\n",sizeof(client_t)); std::fflush(stdout);
    const std::vector<byte> zero(sizeof(client_t),0);
    SV_AllocClients(4);
    for(const client_t &client:SV_Clients()) assert(Bytes(client)==zero);
    auto &first=SV_ClientForIndex(0); first.state=CS_ACTIVE;
    std::strcpy(first.name,"connected"); first.gentity=&maximum;
    first.reliableSequence=43; first.reliableCommands[MAX_GAME_RELIABLE_COMMANDS-1][MAX_STRING_CHARS-2]='z';
    first.netchan.remoteAddress.type=NA_BOT; first.downloadBlocks[MAX_DOWNLOAD_WINDOW-1]=reinterpret_cast<byte *>(&profile);
    first.csUpdated[MAX_GAME_CONFIGSTRINGS-1]=qtrue; first.demoRecordServerId=917;
    auto &third=SV_ClientForIndex(2); third.state=CS_PRIMED;
    std::strcpy(third.userinfo,"saved user info"); third.lastUsercmd.serverTime=123456;
    SV_ClientForIndex(1).state=CS_ZOMBIE; std::strcpy(SV_ClientForIndex(1).name,"discard zombie");
    std::strcpy(SV_ClientForIndex(3).name,"discard free slot");
    const auto expectedFirst=Bytes(first),expectedThird=Bytes(third);
    Resize(6,6);
    assert(Bytes(SV_ClientForIndex(0))==expectedFirst && Bytes(SV_ClientForIndex(2))==expectedThird);
    for(int i: {1,3,4,5}) assert(Bytes(SV_ClientForIndex(i))==zero);
    Resize(1,3); // A connected high slot bounds a requested shrink.
    assert(Bytes(SV_ClientForIndex(0))==expectedFirst && Bytes(SV_ClientForIndex(2))==expectedThird);
    auto *unchanged=svs.clients; Resize(3,3); assert(svs.clients==unchanged);
    SV_ClientForIndex(2).state=CS_ZOMBIE;
    Resize(-1,1); assert(Bytes(SV_ClientForIndex(0))==expectedFirst);
    unchanged=svs.clients;
    try { SV_AllocClients(-1); assert(false); } catch(Drop &) {}
    try { SV_AllocClients((std::numeric_limits<int>::max)()); assert(false); } catch(Drop &) {}
    assert(svs.clients==unchanged && zone.size()==1 && temporary.empty());
    SV_ZFree(svs.clients); sv.maxclients=0;
    assert(zone.empty() && temporary.empty());
    std::puts("zeroing, sparse growth, bounded shrink, unchanged allocation and overflow rejection passed");
}
'''


class ServerClientStorageTests(unittest.TestCase):
    def test_allocation_and_resize_with_small_stack(self):
        msvc = os.name == 'nt' and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == 'x86'
        compiler = shutil.which('cl') if msvc else (shutil.which('clang++') or shutil.which('g++'))
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = (ROOT / 'code/server/sv_init.cpp').read_text()
        functions = []
        for signature in ('static int SV_BoundMaxClients(', 'static void SV_SetSnapshotParams(',
                          'static void SV_AllocClients(', 'void SV_ChangeMaxClients('):
            start = source.index(signature)
            functions.append(source[start:source.index('\n}', start) + 2])
        with tempfile.TemporaryDirectory(prefix='server-client-storage-') as directory:
            folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            cpp.write_text(HARNESS.replace('FUNCTIONS', '\n'.join(functions)))
            if msvc:
                # The owned Com_Error stub throws across its C declaration;
                # /EHsc would promise that this cannot happen. Production
                # keeps its existing exception-handling flags.
                command = [compiler, '/nologo', '/O2', '/Ob0', '/EHs', '/std:c++17', '/D_CRT_SECURE_NO_WARNINGS',
                    '/I' + str(ROOT), str(cpp), '/Fe:' + str(exe), '/Fo:' + str(folder / 'probe.obj'),
                    '/Fd:' + str(folder / 'probe.pdb'), '/link', '/STACK:262144']
            else:
                target = compiler_target_args(compiler)
                if os.name == 'nt' and not target:
                    target = ['-DFNTECH3_Q3_ONLY']
                sanitizer = [] if os.name == 'nt' else ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
                command = [compiler, *target, '-std=c++17', '-O1', '-fno-inline', '-D_CRT_SECURE_NO_WARNINGS',
                    *sanitizer, '-I', str(ROOT), str(cpp), '-o', str(exe)]
                if os.name == 'nt':
                    command += (['-Wl,--stack,262144'] if Path(compiler).stem.lower() == 'g++'
                                else ['-Wl,/STACK:262144'])
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
