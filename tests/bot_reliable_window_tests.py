"""A bot rings its profile's reliable-command window, whatever its slot held.

Each retail engine rings one fixed count of reliable commands for every
client, bots included. SV_AddServerCommand compares the unacknowledged count
with it and indexes its ring with the matching mask, as the bot reader,
SV_BotGetConsoleMessage, does:

    engine               SV_AddServerCommand       window  bot reader
    quake3.exe 1.32      0x4395b0 cmp 0x41/and 0x3f    64  0x432620
    quakelive_steam.exe  0x4e40c0 cmp 0x41/and 0x3f    64  0x4dda50
    WolfSP.exe           0x437770 cmp 0x101/and 0xff  256  0x430480
    WolfMP.exe           0x439e80 cmp 0x101/and 0xff  256  0x432be0
    et.exe, etded.exe    0x447820, 0x42e010 cmp 0x101 256  0x43f050, 0x425420
    stvoyHM.exe          0x434280 cmp 0x41/and 0x3f    64  0x42e070
    jamp.exe             0x45f490 cmp 0x81/and 0x7f   128  0x455db0
    jk2mp.exe            0x44e2e0 cmp 0x81/and 0x7f   128  0x4476e0

The single-player engines have no bot library: stvoy.exe (0x425eb0),
jasp.exe (0x43a200) and jk2sp.exe (0x437700) ring 64 command pointers,
alice.exe (0x455a10) rings 128 and fakk2.exe 1.02 (0x4468c0) 64. Those two
share protocol 1 and its wire, so a network client on it rings the profile's
window rather than the wire's. The IHV test's quake3.exe has one reliable
slot and no ring. Every retail bot allocator (quake3.exe 0x4318c0,
quakelive_steam.exe 0x4dcd30, WolfSP.exe 0x42f850, WolfMP.exe 0x431fc0,
et.exe 0x43ef20, etded.exe 0x4252f0, stvoyHM.exe 0x42d330, jamp.exe 0x455050,
jk2mp.exe 0x4469c0) leaves the slot's counters as its last occupant left them.

fnTech3 took a client's window from its wire profile, which only Netchan_Setup
assigns, so a bot rang whatever its slot last held: Quake III's 64 in a slot no
network client had used, and the last player's otherwise. This compiles the
production SV_ReliableCommandLimit, the profile catalog,
SV_BotAllocateClientSlot, SV_BotAllocateClient, SV_BotFreeClient,
SV_BotGetConsoleMessage, SV_AddServerCommand and SV_SendServerCommand and runs
every profile in the catalog. Each mutation below must make the checks fail.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
import unittest

from compiler_support import compiler_target_args
import rtcw_server_world_tests as world
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]

# server.h's own SV_ReliableCommandLimit is renamed out of the way, so the
# copy compiled after it -- the production text or a mutation of it -- is the
# one every sliced function calls.
PRELUDE = ('#define SV_ReliableCommandLimit SV_ReliableCommandLimit_Header\n'
           + world.PRELUDE[:world.PRELUDE.index('const vec3_t vec3_origin')]
           + '#undef SV_ReliableCommandLimit\n' + r'''
#include "code/qcommon/netchan_safety.hpp"
#include <cstdarg>
#include <cstdio>
#include <string>
serverStatic_t svs{};
static cvar_t dedicated{},fps{};
cvar_t *com_dedicated=&dedicated,*sv_fps=&fps;
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
// Q3Test's commands take its own sessions (server_q3test_net.hpp), which seat
// no bot, since no release shipped a bot library; the shared path stands in
// for its catalog entry here.
int Com_GameIsQ3Test(){return 0;}
void SV_Q3TestSendServerCommand(client_t*,const char*){assert(false);}
void SV_IHVNetServerCommand(client_t*,const char*){assert(false);}
static std::vector<std::pair<int,std::string>> drops;
void SV_DropClient(client_t *client,const char *reason){
 drops.emplace_back(SV_ClientIndex(client),reason);client->state=CS_ZOMBIE;
}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
#ifdef _WIN32
int Q_vsnprintf(char *s,size_t n,const char *format,va_list args){return vsnprintf(s,n,format,args);}
#endif
// Every command here is addressed; the bot-broadcast tests cover broadcasts.
qboolean SV_ClientIsRTCWCastAI(const client_t*){assert(false);return qfalse;}
qboolean SV_ClientIsWolfETBot(const client_t*){assert(false);return qfalse;}
// The module's records are the entity tests' subject; these are plain ones.
static sharedEntity_t entities[8];
sharedEntity_t *SV_GentityNum(int n){assert(n>=0&&n<8);return &entities[n];}
qboolean SV_ReadGameEntity(const void *in,sharedEntity_t *out){std::memcpy(out,in,sizeof *out);return qtrue;}
qboolean SV_WriteGameEntity(void *out,const sharedEntity_t *in){std::memcpy(out,in,sizeof *in);return qtrue;}
''')

CHECKS = r'''
// Each retail engine's window; every profile in the catalog must be pinned.
struct Pin { const char *key; int window; };
static const Pin pins[]={
 {"quake3",64},{"quakelive",64},{"rtcw-sp",256},{"rtcw-mp",256},{"wolfet",256},
 {"stvef-sp",64},{"stvef-mp",64},{"q3-ihv",0},{"jka-sp",64},{"jka-mp",128},
 {"jk2-sp",64},{"jk2-mp",128},{"alice",128},
 // The 1.02 patch's fakk2.exe drops a client at the 65th (0x004468dd).
 {"fakk2",64},
 // MOHAA.exe 1.00 is encrypted on disk too, but it has no bot library and its
 // gamex86.dll exports no bot entry points, so no bot ever takes a client slot.
 {"mohaa",0},
 // Q3Test 1.06 to 1.08 ring 64 commands and drop a client past them
 // (1.06 SV_AddServerCommand 0x0041d3a0, 1.08 0x0041dd10); only those
 // releases' game libraries carry the bot AI (GetBotAIAPI).
 {"q3test",64}};
static int pinned(const char *key){
 for(const Pin &pin:pins)if(!std::strcmp(pin.key,key))return pin.window;
 std::fprintf(stderr,"profile %s has no pinned bot window\n",key);std::exit(2);
}

static std::vector<client_t> clients(4);
static int number=1;  // every command says a number no other command says
static void reset(){
 for(client_t &c:clients)std::memset(&c,0,sizeof c);
 std::memset(entities,0,sizeof entities);drops.clear();
}
static std::vector<int> send(client_t &c,int count){
 std::vector<int> sent;
 for(int i=0;i<count;++i){sent.push_back(number);SV_SendServerCommand(&c,"print \"%d\"",number++);}
 return sent;
}
// What the bot's AI is handed, draining its window as BotAI does.
static std::vector<int> drain(int slot){
 std::vector<int> handed;char text[MAX_STRING_CHARS];
 while(SV_BotGetConsoleMessage(slot,text,sizeof text)){
  int n=0;assert(std::sscanf(text,"print \"%d\"",&n)==1);handed.push_back(n);
 }
 return handed;
}
static std::vector<int> join(std::vector<int> a,const std::vector<int> &b){a.insert(a.end(),b.begin(),b.end());return a;}
static int pending(const client_t &c){return c.reliableSequence-c.reliableAcknowledge;}
static bool dropped(int slot){
 return drops.size()==1&&drops[0].first==slot&&drops[0].second=="Server command overflow";
}
// A bot whose window is empty takes exactly `window` unacknowledged commands:
// the AI is handed them in order, and once the window is full again the next
// one overflows it.
static void fills(int slot,int window){
 client_t &bot=clients[slot];assert(pending(bot)==0);
 const auto first=send(bot,window);assert(drops.empty()&&pending(bot)==window);
 assert(drain(slot)==first&&pending(bot)==0);
 send(bot,window);assert(drops.empty());
 send(bot,1);assert(dropped(slot));
}

// A slot no client has used.
static void freshSlot(int window){
 reset();
 const int slot=SV_BotAllocateClient();
 assert(slot==profile.bots.firstClientSlot);
 client_t &bot=clients[slot];
 assert(bot.state==CS_ACTIVE&&bot.netchan.remoteAddress.type==NA_BOT);
 // Its wire profile is left as the slot held it, so every rule that keys on
 // the wire sees what it saw before.
 assert(bot.netchan.wireProfile==NETCHAN_WIRE_LEGACY_Q3);
 assert(SV_ReliableCommandLimit(&bot)==window);
 fills(slot,window);
}

// Ritual's protocol 1 is one wire for Alice and F.A.K.K.2, whose engines ring
// 128 and 64: a client on it rings the profile's own window.
static int wireWindow(netchanWireProfile_t wire,int window){
 return wire==NETCHAN_WIRE_ALICE?window:Netchan_ReliableCommandLimit(wire);
}

// A slot a network client left, whichever window its wire rang.
static void afterNetworkClient(int window,netchanWireProfile_t wire,netadrtype_t address){
 reset();
 const int slot=profile.bots.firstClientSlot,own=wireWindow(wire,window);
 client_t &c=clients[slot];
 // Counters well past every mask, as a long session leaves them.
 c.state=CS_ACTIVE;c.netchan.remoteAddress.type=address;c.netchan.wireProfile=wire;
 c.reliableSequence=c.reliableAcknowledge=1000;
 assert(SV_ReliableCommandLimit(&c)==own);
 // It leaves three quarters of its own window unacknowledged, then its slot
 // is freed.
 const auto leftovers=send(c,own*3/4);assert(drops.empty());
 c.state=CS_FREE;
 assert(SV_BotAllocateClient()==slot);
 assert(c.netchan.wireProfile==wire&&SV_ReliableCommandLimit(&c)==window);
 if(own==window){
  // One window, as in retail: the bot is handed what was left, and the
  // leftovers count against its window.
  assert(pending(c)==int(leftovers.size()));
  const auto more=send(c,window-int(leftovers.size()));assert(drops.empty());
  assert(drain(slot)==join(leftovers,more));
 }else{
  // They were rung under another window, at other indices: dropped.
  assert(pending(c)==0&&c.reliableSequence==1000+int(leftovers.size()));
 }
 fills(slot,window);
}

// A slot a bot left: as in retail, the next bot is handed what it left.
static void afterBot(int window){
 reset();
 const int slot=SV_BotAllocateClient();client_t &c=clients[slot];
 c.reliableSequence=c.reliableAcknowledge=1000;
 const auto leftovers=send(c,window/2);
 SV_BotFreeClient(slot);assert(c.state==CS_FREE);
 assert(SV_BotAllocateClient()==slot&&pending(c)==window/2);
 const auto more=send(c,window-window/2);assert(drops.empty());
 assert(drain(slot)==join(leftovers,more));
 fills(slot,window);
}

// An engine with no ring seats no bot in a client slot, and touches none.
static void noRing(){
 reset();for(client_t &c:clients)c.reliableSequence=77;
 const auto before=clients;
 assert(SV_BotAllocateClient()==-1);
 for(size_t i=0;i<clients.size();++i)assert(!std::memcmp(&clients[i],&before[i],sizeof(client_t)));
}

// A network client still rings its wire's window, whatever the profile, but
// on Ritual's wire it rings the profile's.
static void networkClients(int window){
 for(unsigned w=0;w<64;++w){
  const netchanWireProfile_t wire=netchanWireProfile_t(w);
  for(netadrtype_t address:{NA_BAD,NA_LOOPBACK,NA_IP}){
   client_t c;std::memset(&c,0,sizeof c);c.netchan.remoteAddress.type=address;c.netchan.wireProfile=wire;
   assert(SV_ReliableCommandLimit(&c)==wireWindow(wire,window));
  }
 }
}

// Quake Live's '%' rule still keys on the slot's wire, exactly as before, so
// a bot's text depends on its slot there; that is recorded as open.
static void quakeLivePercent(){
 if(profile.id!=COM_GAME_QUAKELIVE)return;
 for(netchanWireProfile_t wire:{NETCHAN_WIRE_LEGACY_Q3,NETCHAN_WIRE_QL_RETAIL}){
  reset();client_t &c=clients[profile.bots.firstClientSlot];c.netchan.wireProfile=wire;
  const int slot=SV_BotAllocateClient();
  SV_SendServerCommand(&clients[slot],"chat \"100%% sure\"");
  char text[MAX_STRING_CHARS];assert(SV_BotGetConsoleMessage(slot,text,sizeof text));
  assert(std::string(text)==(wire==NETCHAN_WIRE_QL_RETAIL?"chat \"100. sure\"":"chat \"100% sure\""));
 }
}

int main(){
#ifdef _MSC_VER
 // A failed check aborts without Windows fault reporting, whose snapshot of
 // the process keeps its executable mapped after the process has exited.
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 svs.clients=clients.data();sv.maxclients=int(clients.size());svs.time=4321;fps.integer=20;
 for(const comGameProfile_t &entry:profiles){
  profile=entry;
  const int window=pinned(profile.key);
  assert(profile.bots.reliableCommands==window);
  networkClients(window);
  if(window==0){noRing();continue;}
  // The ring's mask needs a power of two, within the engine's storage.
  assert((window&(window-1))==0&&window<=MAX_GAME_RELIABLE_COMMANDS);
  freshSlot(window);
  for(unsigned w=0;w<64;++w){
   const netchanWireProfile_t wire=netchanWireProfile_t(w);
   if(Netchan_ReliableCommandLimit(wire)<=0)continue;
   afterNetworkClient(window,wire,w&1?NA_LOOPBACK:NA_IP);
  }
  afterBot(window);
  quakeLivePercent();
  profile.bots.reliableCommands=0;noRing();
 }
 svs.clients=nullptr;sv.maxclients=0;
}
'''

# Each mutation replaces text in one production piece and must fail.
MUTATIONS = {
    "a bot rings its slot's wire, as before": (
        'limit', 'if ( client->netchan.remoteAddress.type == NA_BOT\n'
                 '\t\t|| client->netchan.wireProfile == NETCHAN_WIRE_ALICE ) {\n'
                 '\t\treturn Com_GameProfile()->bots.reliableCommands;\n\t}\n\t', ''),
    "every client rings its profile's window": (
        'limit', 'client->netchan.remoteAddress.type == NA_BOT', 'true'),
    "a Ritual client rings Alice's window under F.A.K.K.2": (
        'limit', '\n\t\t|| client->netchan.wireProfile == NETCHAN_WIRE_ALICE', ''),
    'a bot keeps leftovers rung under another window': (
        'static int SV_BotAllocateClientSlot(',
        'if ( SV_ReliableCommandLimit( cl ) != previousWindow ) {', 'if ( false ) {'),
    'a bot is never handed what the last bot left': (
        'static int SV_BotAllocateClientSlot(',
        'if ( SV_ReliableCommandLimit( cl ) != previousWindow ) {', 'if ( true ) {'),
    'an engine with no ring seats bots': (
        'static int SV_BotAllocateClientSlot(', 'if ( policy.reliableCommands <= 0 ) {', 'if ( false ) {'),
    "the bot reader indexes by the slot's wire": (
        'int SV_BotGetConsoleMessage(', 'SV_ReliableCommandLimit( cl )',
        'Netchan_ReliableCommandLimit( cl->netchan.wireProfile )'),
    "a bot is given its profile's own wire": (
        'static int SV_BotAllocateClientSlot(', 'cl->netchan.remoteAddress.type = NA_BOT;\n',
        'cl->netchan.remoteAddress.type = NA_BOT;\n'
        '\tcl->netchan.wireProfile = Netchan_SelectWireProfile( Com_GameProfile()->preferredProtocol, 1 );\n'),
    "RTCW MP rings Quake III's window": (
        'catalog', '{ "0", "128", "1024", 1024, 0, 2, 1, 16384, 0, 256 }',
        '{ "0", "128", "1024", 1024, 0, 2, 1, 16384, 0, 64 }'),
}

BOT_FUNCTIONS = ('static int SV_BotAllocateClientSlot(', 'int SV_BotAllocateClient(',
                 'void SV_BotFreeClient(', 'int SV_BotGetConsoleMessage(')
MAIN_FUNCTIONS = ('static const char *SV_ExpandNewlines(', 'void SV_AddServerCommand(',
                  'void QDECL SV_SendServerCommand(')


def pieces():
    header = (ROOT / 'code/server/server.h').read_text()
    catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
    bot = (ROOT / 'code/server/sv_bot.cpp').read_text()
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    result = {'limit': function(header, 'static inline int SV_ReliableCommandLimit('),
              'catalog': catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]}
    result.update((signature, function(main, signature)) for signature in MAIN_FUNCTIONS)
    result.update((signature, function(bot, signature)) for signature in BOT_FUNCTIONS)
    return result


def source(parts):
    return PRELUDE + '\n'.join(parts.values()) + '\n' + CHECKS


def remove_folder(folder):
    # Windows can hold an executable for a moment after its process exits (an
    # antivirus scan of a new file, fault reporting), and deleting it then
    # fails with "Access is denied"; give it a few seconds before failing.
    for _ in range(50):
        try:
            shutil.rmtree(folder)
            return
        except PermissionError:
            time.sleep(0.1)
    shutil.rmtree(folder)


class BotReliableWindowTests(unittest.TestCase):
    def run_probe(self, folder, name, text, objects):
        cpp = folder / f'{name}.cpp'
        exe = folder / f'{name}.exe'
        cpp.write_text(text, encoding='utf-8')
        built = subprocess.run([self.compiler, *self.flags, str(cpp), *objects, '-o', str(exe)],
                               cwd=ROOT, capture_output=True, text=True, timeout=180)
        self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        return subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)

    def test_each_profile_rings_its_retail_bot_window_in_every_slot(self):
        self.compiler = shutil.which('clang++') or shutil.which('g++')
        if not self.compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(self.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        self.flags = [*target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT)]
        production = pieces()
        folder = Path(tempfile.mkdtemp(prefix='bot-window-'))
        self.addCleanup(remove_folder, folder)
        objects = []
        for name, path, language in (('view', 'code/server/server_game_data.cpp', []),
                                     ('adapter', 'code/compat/game_abi_adapter.c', ['-x', 'c++'])):
            built = subprocess.run([self.compiler, *self.flags, '-c', *language, str(ROOT / path),
                                    '-o', str(folder / f'{name}.o')],
                                   cwd=ROOT, capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            objects.append(str(folder / f'{name}.o'))
        ran = self.run_probe(folder, 'production', source(production), objects)
        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        for index, (description, (piece, old, new)) in enumerate(MUTATIONS.items()):
            with self.subTest(mutation=description):
                self.assertEqual(production[piece].count(old), 1, old)
                mutated = dict(production, **{piece: production[piece].replace(old, new)})
                ran = self.run_probe(folder, f'mutation{index}', source(mutated), objects)
                self.assertNotEqual(ran.returncode, 0, description)


if __name__ == '__main__':
    unittest.main()
