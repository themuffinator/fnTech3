"""Dedicated servers echo broadcast prints as the selected profile's engine does.

Retail WolfMP.exe and Enemy Territory 2.60b leave the "[lon]" and "[lof]"
localization tokens out of the "broadcast:" console echo. RTCW single player
and Quake III 1.32 print them, and no other supported retail engine contains
either token. Every profile still delivers the command to clients unchanged.
"""
from pathlib import Path
import unittest

import rtcw_botlib_tests as bootstrap

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/server/server.h"
#include "code/qcommon/netchan_safety.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
server_t sv{};
serverStatic_t svs{};
static comGameProfile_t profile{};
// A bot's reliable window is its profile's; these clients are all players.
const comGameProfile_t *Com_GameProfile(){return &profile;}
static cvar_t dedicated{};
cvar_t *com_dedicated=&dedicated;
int Com_GameIsRTCWMP(){return profile.id==COM_GAME_RTCW_MP;}
int Com_GameIsWolfET(){return profile.id==COM_GAME_WOLFET;}
// Q3Test's broadcasts take its own sessions (server_q3test_net.hpp); the
// shared path stands in for its catalog entry here.
int Com_GameIsQ3Test(){return 0;}
void SV_Q3TestSendServerCommand(client_t*,const char*){assert(false);}
// So do the IHV Test's (server_q3_ihv_net.hpp).
int Com_GameIsQuake3IHV(){return 0;}
void SV_IHVNetServerCommand(client_t*,const char*){assert(false);}
static std::vector<std::string> printed;
void QDECL Com_Printf(const char *format,...){
 std::vector<char> text(1<<14);va_list args;va_start(args,format);
 const int length=vsnprintf(text.data(),text.size(),format,args);va_end(args);
 assert(length>=0&&size_t(length)<text.size());printed.emplace_back(text.data(),size_t(length));
}
qboolean SV_ClientIsRTCWCastAI(const client_t *){return qfalse;}
qboolean SV_ClientIsWolfETBot(const client_t *){return qfalse;}
void SV_DropClient(client_t *,const char *){assert(false&&"a reliable window overflowed");}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::strncpy(out,in,size);out[size-1]=0;}
#ifdef _WIN32
int Q_vsnprintf(char *s,size_t n,const char *format,va_list args){return vsnprintf(s,n,format,args);}
#endif
'''

CHECKS = r'''
// Retail's loop, from WolfMP.exe 0x439e00 and et.exe 0x447ce0: a newline
// becomes a backslash and an n, and RTCW MP and ET skip a "[lon]" or "[lof]"
// at the read position. WolfSP.exe 0x437720 and Quake III 1.32 have no skip.
// Retail stops at 1,021 output bytes; the bound here is fnTech3's own.
static std::string retail(const std::string &in,bool strip){
 constexpr size_t bound=MAX_STRING_CHARS*2-3;
 std::string out;
 for(size_t i=0;i<in.size()&&out.size()<bound;){
  if(in[i]=='\n'){out+="\\n";++i;}
  else if(strip&&(!in.compare(i,5,"[lon]")||!in.compare(i,5,"[lof]")))i+=5;
  else out+=in[i++];
 }
 return out;
}
static std::vector<client_t> clients(4);
// Sends one command, then checks the echo and every client's stored copy.
static void send(client_t *target,const std::string &command,const std::string *echo){
 std::vector<int> before;for(const client_t &c:clients)before.push_back(c.reliableSequence);
 printed.clear();SV_SendServerCommand(target,"%s",command.c_str());
 if(echo)assert(printed.size()==1&&printed[0]=="broadcast: "+*echo+"\n");
 else assert(printed.empty());
 for(size_t i=0;i<clients.size();++i){
  client_t &c=clients[i];
  const bool receives=(!target||target==&c)&&c.state>=CS_PRIMED&&(command.size()<=1022||c.longstr);
  assert(c.reliableSequence==before[i]+(receives?1:0));
  if(receives){
   // A reliable slot keeps 1,023 bytes, so a longer command is stored cut.
   const std::string stored=command.substr(0,sizeof c.reliableCommands[0]-1);
   assert(stored==c.reliableCommands[c.reliableSequence&(SV_ReliableCommandLimit(&c)-1)]);
   c.reliableAcknowledge=c.reliableSequence;
  }
 }
}
struct Case{std::string command,kept,stripped;};
int main(){
 svs.clients=clients.data();sv.maxclients=int(clients.size());
 clients[0].state=CS_ACTIVE;clients[1].state=CS_ACTIVE;clients[1].longstr=qtrue;
 clients[2].state=CS_PRIMED;clients[3].state=CS_CONNECTED;
 // Echoes in the profiles that keep the tokens, then in RTCW MP and ET.
 const std::vector<Case> cases={
  // The g_timelimit change both servers logged in the restart probe.
  {"print \"Server:[lof] timelimit [lon]changed to[lof] 10\n\"",
   "print \"Server:[lof] timelimit [lon]changed to[lof] 10\\n\"",
   "print \"Server: timelimit changed to 10\\n\""},
  // WolfMP.exe's own drop notice.
  {"print \"[lof]Player^7 [lon]timed out\n\"",
   "print \"[lof]Player^7 [lon]timed out\\n\"","print \"Player^7 timed out\\n\""},
  {"print \"[lon][lof]x[lof][lon]\n\"","print \"[lon][lof]x[lof][lon]\\n\"","print \"x\\n\""},
  // One pass: taking out the inner token leaves a new one in the echo.
  {"print \"[lo[lon]n]\"","print \"[lo[lon]n]\"","print \"[lon]\""},
  // Case matters, and a broken token stays.
  {"print \"[LON][Lof][lon [lof x[lonx]\"","print \"[LON][Lof][lon [lof x[lonx]\"",
   "print \"[LON][Lof][lon [lof x[lonx]\""},
  {"print \"[lof]\n[lon]\n\"","print \"[lof]\\n[lon]\\n\"","print \"\\n\\n\""},
  {"print \"tail [lof]\"","print \"tail [lof]\"","print \"tail \""},
  {"print \"tail [lon","print \"tail [lon","print \"tail [lon"},
  // The whole command is scanned, as in retail.
  {"print[lon] \"x\"","print[lon] \"x\"","print \"x\""},
 };
 for(const Case &c:cases)assert(retail(c.command,false)==c.kept&&retail(c.command,true)==c.stripped);
 // Past the bound the echo stops short, whatever it skipped.
 std::string longCommand="print \""+std::string(1010,'\n');
 for(int i=0;i<20;++i)longCommand+="[lon]x";
 longCommand+="\"";
 for(bool strip:{false,true}){
  const std::string echo=retail(longCommand,strip);
  assert(echo.size()==MAX_STRING_CHARS*2-3);
  assert(echo.substr(echo.size()-18)==(strip?std::string(18,'x'):"[lon]x[lon]x[lon]x"));
 }
 // Commands made of token pieces, compared with the retail loop.
 static const char *const pieces[]={"[lon]","[lof]","[lo","n]","[l","of]","[","]","\n","x"," ",
  "^7","[LON]","[lof","lon]"};
 std::vector<std::string> generated;
 std::uint32_t random=0x2f6b1c4du;
 for(int i=0;i<300;++i){
  random=random*1664525u+1013904223u;
  // At most 800 bytes of pieces, so no command reaches the 1,022-byte limit.
  std::string command=random%5?"print \"":"cp \"";
  for(std::uint32_t n=(random>>24)%160;n;--n){
   random=random*1664525u+1013904223u;
   command+=pieces[(random>>16)%(sizeof pieces/sizeof *pieces)];
  }
  generated.push_back(command+"\"");
 }
 for(unsigned id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
  profile.id=comGameId_t(id);
  const bool strip=id==COM_GAME_RTCW_MP||id==COM_GAME_WOLFET;
  for(int server:{0,1}){
   dedicated.integer=server;
   for(const Case &c:cases){
    send(nullptr,c.command,server?strip?&c.stripped:&c.kept:nullptr);
    // Other broadcasts and addressed commands are never echoed.
    send(nullptr,"cp"+c.command.substr(5),nullptr);
    send(&clients[0],c.command,nullptr);
   }
   const std::string echo=retail(longCommand,strip);
   send(nullptr,longCommand,server?&echo:nullptr);
   for(const std::string &command:generated){
    const std::string expected=retail(command,strip);
    send(nullptr,command,server&&!command.compare(0,5,"print")?&expected:nullptr);
   }
  }
 }
}
'''


def source():
    main = (ROOT / 'code/server/sv_main.cpp').read_text()
    result = PRELUDE
    for signature in ('static const char *SV_ExpandNewlines(', 'void SV_AddServerCommand(',
                      'void QDECL SV_SendServerCommand('):
        result += '\n' + function(main, signature)
    return result + CHECKS


class ServerBroadcastEchoTests(unittest.TestCase):
    compile_and_run = bootstrap.RTCWBotLibTests.compile_and_run

    def test_rtcw_mp_and_et_echo_broadcasts_without_localization_tokens(self):
        self.compile_and_run(source())


if __name__ == '__main__':
    unittest.main()
