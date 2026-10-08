/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Actual API 4 adapter against a synthetic module and instrumented host
 * services. Real DLL/map evidence is recorded separately; these tests cover
 * ownership, ABI dispatch, malformed records, and partial initialization. */
#include "../code/server/server.h"
#include "../code/server/server_game_data.h"
#include "../code/compat/q3_ihv_game_abi.h"
#include <cstdarg>
#include <csetjmp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool yes,const char *message){if(!yes)throw std::runtime_error(message);}
#define CHECK(x) Check((x),#x)
static std::jmp_buf errorJump;
static errorParm_t errorLevel;
static char errorText[2048];
static bool errorArmed;
void QDECL Com_Error(errorParm_t level,const char *format,...){
 va_list args;va_start(args,format);vsnprintf(errorText,sizeof errorText,format,args);va_end(args);
 errorLevel=level;if(!errorArmed){std::fprintf(stderr,"unexpected engine error: %s\n",errorText);std::abort();}
 std::longjmp(errorJump,1);
}
void QDECL Com_Printf(const char *,...){}
void QDECL Com_DPrintf(const char *,...){}
int Sys_Milliseconds(){return 1234;}
int Q_stricmp(const char *a,const char *b){
 while(*a&&*b){const int x=std::tolower(static_cast<unsigned char>(*a++)),y=std::tolower(static_cast<unsigned char>(*b++));if(x!=y)return x-y;}
 return static_cast<unsigned char>(*a)-static_cast<unsigned char>(*b);
}
void Q_strncpyz(char *out,const char *in,int count){CHECK(count>0);std::snprintf(out,count,"%s",in);}
// Two rotating buffers, like the engine's own parser: the adapter must copy
// each value out before asking for the next one.
const char *Info_ValueForKey(const char *text,const char *key){
 static char storage[2][MAX_INFO_VALUE];static int which;
 char *out=storage[which^=1];out[0]=0;
 if(!text||!key||!*key)return out;
 if(*text=='\\')++text;
 while(*text){
  char name[MAX_INFO_VALUE],value[MAX_INFO_VALUE];size_t length=0;
  while(*text&&*text!='\\'&&length+1<sizeof name)name[length++]=*text++;
  name[length]=0;if(*text=='\\')++text;
  length=0;while(*text&&*text!='\\'&&length+1<sizeof value)value[length++]=*text++;
  value[length]=0;if(*text=='\\')++text;
  if(!Q_stricmp(name,key)){std::memcpy(out,value,std::strlen(value)+1);return out;}
 }
 return out;
}
#if defined(_WIN32) && !defined(__MINGW32__)
int Q_vsnprintf(char *out,size_t count,const char *format,va_list args){return vsnprintf(out,count,format,args);}
#endif
const vec3_t vec3_origin{};
server_t sv{};
vm_t *gvm=nullptr;
fntech3::server::ServerGameDataView serverGameData;
static void InvalidateServerGameData(){serverGameData.Invalidate();sv.gentities=nullptr;sv.gentitySize=sv.num_entities=0;}
static int cvarGets,cvarSets,fsReads,fsFrees,fsWrites,vmFrees,links,unlinks,dropIndex,commandIndex;
static cvar_t padPackets{};
cvar_t *sv_padPackets=&padPackets;
static std::string commandText,writeText;
struct Variable {
 cvar_t record{};std::string name,value,reset;
 void Publish(){record.name=name.data();record.string=value.data();record.resetString=reset.data();record.value=static_cast<float>(std::atof(value.c_str()));}
};
static std::map<std::string,Variable> variables;
static std::string Key(const char *name){std::string key=name;for(auto &c:key)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return key;}
cvar_t *Cvar_Get(const char *name,const char *value,int flags){
 ++cvarGets;auto &v=variables[Key(name)];if(v.name.empty()){v.name=name;v.value=v.reset=value;++v.record.modificationCount;}
 v.record.flags|=flags;v.Publish();return &v.record;
}
unsigned Cvar_Flags(const char *name){const auto i=variables.find(Key(name));return i==variables.end()?CVAR_NONEXISTENT:static_cast<unsigned>(i->second.record.flags);}
cvar_t *Cvar_Set2(const char *name,const char *value,qboolean force){
 CHECK(force);++cvarSets;auto *v=Cvar_Get(name,value,0);auto &owner=variables[Key(name)];
 if(owner.value!=value){owner.value=value;++v->modificationCount;}v->latchedString=nullptr;owner.Publish();return v;
}
int Cvar_VariableIntegerValue(const char *name){return std::atoi(variables[Key(name)].value.c_str());}
const char *Cvar_VariableString(const char *name){return variables[Key(name)].value.c_str();}
int Com_GameClientLimit(){return 256;}
int Cmd_Argc(){return 2;}
const char *Cmd_Argv(int n){return n==1?"argument":"command";}
void Cbuf_AddText(const char *text){commandText=text;}
int FS_ReadFile(const char *name,void **out){
 ++fsReads;if(!std::strcmp(name,"missing")){if(out)*out=nullptr;return -1;}
 if(out){*out=std::malloc(4);std::memcpy(*out,"abc",4);}return 3;
}
void FS_FreeFile(void *p){++fsFrees;std::free(p);}
void FS_WriteFile(const char *,const void *data,int size){++fsWrites;writeText.assign(static_cast<const char *>(data),size);}
qboolean FS_NativeWritePath(const char *name,char *out,size_t size){
 if(!name||!std::strcmp(name,"bad"))return qfalse;std::snprintf(out,size,"home/baseq3/%s",name);return qtrue;
}
static void SV_GameDropClient(int index,const char *){dropIndex=index;}
static void SV_GameSendServerCommand(int index,const char *text){commandIndex=index;commandText=text;}
static std::map<int,std::string> strings;
void SV_SetConfigstring(int index,const char *text){
 CHECK(index>=0&&index<1024);strings[index]=text;sv.configstrings[index]=strings[index].data();
 ++sv.configstringSequence;
}
static void SV_SetBrushModel(sharedEntity_t *,const char *name){CHECK(!std::strcmp(name,"*1"));}
void SV_LinkEntity(sharedEntity_t *){++links;}
void SV_UnlinkEntity(sharedEntity_t *){++unlinks;}
void SV_Trace(trace_t *out,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int pass,int mask,qboolean capsule){
 CHECK(pass==2&&mask==15&&!capsule);*out={};out->fraction=0.25f;out->endpos[2]=3;out->plane.normal[1]=1;
 out->plane.dist=7;out->plane.type=4;out->plane.signbits=2;out->startsolid=qtrue;out->surfaceFlags=19;out->contents=15;out->entityNum=2;
}
int SV_PointContents(const vec3_t,int pass){CHECK(pass==1);return 123;}
qboolean SV_inPVS(const vec3_t,const vec3_t){return qtrue;}
static bool SV_inPVSIgnorePortals(const vec3_t,const vec3_t){return false;}
static void SV_AdjustAreaPortalState(sharedEntity_t *,bool open){CHECK(open);}
qboolean CM_AreasConnected(int a,int b){return a>=0&&b>=0&&a==b?qtrue:qfalse;}
// Synthetic collision replies for the native snapshot builder: one leaf per
// view point, a programmable cluster bit vector and a visible-node set.
static std::map<int,std::array<int,2>> leafForPoint; // x -> {area, cluster}
static int leafArea[8],leafCluster[8],leafCount;
static unsigned char snapshotPvs[MAX_MAP_AREA_BYTES];
static std::vector<int> visibleHeadnodes;
static int areaBitsWritten,areaBitsArea;
int CM_PointLeafnum(const vec3_t point){
 const int key=static_cast<int>(point[0]);
 for(int i=0;i<leafCount;++i)if(i==key)return i;
 return 0;
}
int CM_LeafArea(int leaf){return leaf>=0&&leaf<leafCount?leafArea[leaf]:-1;}
int CM_LeafCluster(int leaf){return leaf>=0&&leaf<leafCount?leafCluster[leaf]:-1;}
int CM_WriteAreaBits(byte *buffer,int area){
 areaBitsArea=area;std::memset(buffer,0,MAX_MAP_AREA_BYTES);
 if(area>=0)buffer[area>>3]=static_cast<byte>(1<<(area&7));
 return areaBitsWritten;
}
byte *CM_ClusterPVS(int){return snapshotPvs;}
int CM_ClusterBytes(void){return static_cast<int>(sizeof snapshotPvs);}
qboolean CM_HeadnodeVisible(int node,const byte *bits){
 CHECK(bits==snapshotPvs);
 return std::find(visibleHeadnodes.begin(),visibleHeadnodes.end(),node)!=visibleHeadnodes.end()?qtrue:qfalse;
}
svEntity_t *SV_SvEntityForGentity(sharedEntity_t *gEnt){
 short number=0;std::memcpy(&number,gEnt,sizeof number);
 CHECK(number>=0&&number<MAX_GENTITIES);return &sv.svEntities[number];
}
static std::vector<int> contacts;
int SV_AreaEntities(const vec3_t,const vec3_t,int *out,int capacity){CHECK(capacity==1024);std::copy(contacts.begin(),contacts.end(),out);return static_cast<int>(contacts.size());}
clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t *,qboolean capsule){CHECK(!capsule);return 5;}
qboolean SV_ReadGameEntity(const void *pointer,sharedEntity_t *out){
 fntech3::server::GameEntityRef ref;
 if(!serverGameData.EntityRefForPointer(pointer,ref))return qfalse;
 *out={};return serverGameData.ReadEntityState(ref,out->s)&&serverGameData.GetCurrentOrigin(ref,out->r.currentOrigin)
  &&serverGameData.GetCurrentAngles(ref,out->r.currentAngles)?qtrue:qfalse;
}
static vec3_t contactOrigin,contactAngles;
static bool contactSolid=true;
void CM_TransformedBoxTrace(trace_t *out,const vec3_t start,const vec3_t end,const vec3_t,const vec3_t,int model,int mask,const vec3_t origin,const vec3_t angles,qboolean capsule){
 CHECK(model==5&&mask==-1&&!capsule&&!std::memcmp(start,vec3_origin,12)&&!std::memcmp(end,vec3_origin,12));
 std::memcpy(contactOrigin,origin,12);std::memcpy(contactAngles,angles,12);*out={};out->startsolid=contactSolid?qtrue:qfalse;
}
static fntech3_q3_ihv_game_import_t *imports;
static fntech3_q3_ihv_game_export_t exports;
static vm_t fakeVm{};
static int initializations,spawns,shutdowns,frames,failAt,frameTime;
static int userinfoChanges,botBegins,botThinks,botThinkMsec;
static fntech3_q3_ihv_entity_t *userinfoEntity,*botEntity;
static char *userinfoText;
static unsigned char *clientArray;
static void Init(){
 ++initializations;
 exports.entities=static_cast<fntech3_q3_ihv_entity_t *>(imports->TagMalloc(16*556,765));
 exports.entityStride=556;exports.entityCount=3;exports.entityCapacity=16;
 clientArray=static_cast<unsigned char *>(imports->TagMalloc(2*1464,765));imports->CvarGet("maxclients","2",32);
 if(failAt==1)imports->Error(1,"failed Init");
}
static void Shutdown(){++shutdowns;if(failAt==3)imports->Error(1,"failed Shutdown");imports->FreeTags(766);imports->FreeTags(765);}
static void Spawn(const char *map,const char *text,int time){
 CHECK(!std::strcmp(map,"test")&&!std::strcmp(text,"{ worldspawn }")&&time==1000);++spawns;
 imports->FreeTags(766);imports->TagMalloc(64,766);
 if(failAt==2)imports->Error(1,"failed Spawn");
 ++exports.entityCount;
 auto *entity=reinterpret_cast<fntech3_q3_ihv_entity_t *>(reinterpret_cast<char *>(exports.entities)+556*(exports.entityCount-1));
 imports->LinkEntity(entity);
}
static void Frame(int time){frameTime=time;++frames;}
static void UserinfoChanged(fntech3_q3_ihv_entity_t *entity,char *text){
 ++userinfoChanges;userinfoEntity=entity;userinfoText=text;
}
static void BotBegin(fntech3_q3_ihv_entity_t *entity){++botBegins;botEntity=entity;}
static void BotThink(fntech3_q3_ihv_entity_t *entity,int msec){++botThinks;botEntity=entity;botThinkMsec=msec;}
static int Console(){CHECK(imports->Argc()==2&&!std::strcmp(imports->Argv(1),"argument"));return 91;}
static fntech3_q3_ihv_game_export_t *Getter(fntech3_q3_ihv_game_import_t *table){
 CHECK(table);imports=table;for(unsigned i=0;i<35;++i){void *function;std::memcpy(&function,reinterpret_cast<char *>(table)+i*sizeof(void*),sizeof function);CHECK(function);}
 if(failAt==4){imports->TagMalloc(1,765);imports->Error(1,"failed GetGameAPI");}
 return &exports;
}
vm_t *VM_CreateTable(vmIndex_t index,const char *name,vmMainFunc_t dispatcher,void **symbol){
 CHECK(index==VM_GAME&&!std::strcmp(name,"GetGameAPI"));fakeVm.entryPoint=dispatcher;*symbol=reinterpret_cast<void *>(Getter);return &fakeVm;
}
void VM_Free(vm_t *vm){CHECK(vm==&fakeVm);++vmFrees;}
#include "../code/server/server_q3_ihv_game.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4611)
#endif
// Match Com_Error's C unwind boundary; only trivial callback locals cross it.
template<typename F>static void Reject(F call,const char *text){
 errorArmed=true;
 if(setjmp(errorJump)){errorArmed=false;CHECK(errorLevel==ERR_DROP&&std::strstr(errorText,text));return;}
 call();errorArmed=false;CHECK(false);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
static void Reset(){
 SV_IHVClearGame();variables.clear();strings.clear();exports={};exports.apiVersion=4;
 exports.Init=Init;exports.Shutdown=Shutdown;exports.SpawnEntities=Spawn;exports.RunFrame=Frame;exports.ConsoleCommand=Console;
 exports.ClientUserinfoChanged=UserinfoChanged;exports.BotBegin=BotBegin;exports.BotThink=BotThink;
 failAt=initializations=spawns=shutdowns=frames=vmFrees=links=unlinks=0;gvm=nullptr;
 userinfoChanges=botBegins=botThinks=botThinkMsec=frameTime=0;userinfoEntity=botEntity=nullptr;userinfoText=nullptr;
 sv.maxclients=2;sv.time=1000;sv.state=SS_GAME;sv.entityParsePoint="{ worldspawn }";Cvar_Get("mapname","test",0);
}
static void Stop(){SV_IHVGameEntry(GAME_SHUTDOWN);VM_Free(gvm);gvm=nullptr;CHECK(!ihvGame&&!ihvGameInitialized&&!serverGameData.IsBound()&&!ihvGameMemory.Count()&&!ihvGameCvars);}
static void Lifecycle(){
 Reset();CHECK(SV_IHVLoadGame()==&fakeVm&&gvm==&fakeVm);CHECK(!initializations);
 CHECK(fakeVm.entryPoint(GAME_INIT,0,0,0)==0&&initializations==1&&spawns==1&&links==1);
 auto *base=exports.entities;CHECK(serverGameData.EntityCount()==4&&sv.num_entities==4&&ihvGameMemory.Count()==3);
 CHECK(fakeVm.entryPoint(GAME_INIT,0,0,1)==0&&initializations==1&&spawns==2&&exports.entities==base);
 CHECK(serverGameData.EntityCount()==5&&sv.num_entities==5&&ihvGameMemory.Count()==3);
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1400)==0&&frames==1&&frameTime==1400);
 CHECK(fakeVm.entryPoint(GAME_CONSOLE_COMMAND)==91);
 CHECK(std::strstr(reinterpret_cast<const char *>(fakeVm.entryPoint(GAME_CLIENT_CONNECT,0,0,0)),"protocol-33"));
 Reject([]{SV_IHVGameEntry(GAME_CLIENT_THINK,0);},"not yet integrated");
 Stop();CHECK(shutdowns==1&&vmFrees==1);
 SV_IHVGameEntry(GAME_SHUTDOWN);CHECK(shutdowns==1);
 for(int failure:{1,2,3,4}){
  Reset();failAt=failure;
  if(failure==4)Reject([]{SV_IHVLoadGame();},"failed GetGameAPI");
  else{SV_IHVLoadGame();if(failure==1||failure==2)Reject([]{SV_IHVGameEntry(GAME_INIT);},"failed");else SV_IHVGameEntry(GAME_INIT);}
  CHECK(gvm==&fakeVm&&ihvGameMemory.Count()>0);
  if(failure==3)Reject([]{SV_IHVGameEntry(GAME_SHUTDOWN);},"failed Shutdown");
  Stop();CHECK(shutdowns==(failure==1||failure==4?0:1)&&vmFrees==1);
 }
 for(int invalid:{0,1,2,3,4,5}){
  Reset();switch(invalid){case 0:exports.apiVersion=5;break;case 1:exports.Init=nullptr;break;case 2:exports.Shutdown=nullptr;break;case 3:exports.SpawnEntities=nullptr;break;case 4:exports.RunFrame=nullptr;break;case 5:exports.ConsoleCommand=nullptr;break;}
  Reject([]{SV_IHVLoadGame();},"API 4");CHECK(!gvm&&!ihvGame&&vmFrees==1&&!initializations);
 }
 Reset();SV_IHVLoadGame();Cvar_Set2("maxclients","1",qtrue);
 Reject([]{SV_IHVGameEntry(GAME_INIT);},"exceeds native maxclients");Stop();CHECK(shutdowns==1);
 Reset();SV_IHVLoadGame();SV_IHVGameEntry(GAME_INIT);sv.maxclients=3;
 Reject([]{SV_IHVGameEntry(GAME_INIT);},"exceeds native maxclients");CHECK(initializations==1&&spawns==1);Stop();
}
static void Cvars(){
 Reset();using namespace fntech3::q3ihv;
 for(int bits=0;bits<512;++bits)CHECK(CvarFlagsToNative(CvarFlagsToEngine(bits)|CVAR_PRIVATE|CVAR_TEMP,bits)==bits);
 auto *v=SV_IHVCvarGet("example","7",1|2|4|8|16|32|64|128);CHECK(v->flags==255&&v->value==7&&v->modified==1);
 auto *address=v;v->modified=0;SV_IHVSyncCvars();CHECK(!v->modified);
 Cvar_Set2("example","9",qtrue);SV_IHVSyncCvars();CHECK(v==address&&v->value==9&&v->modified==1);
 v->modified=0;SV_IHVCvarSet("example","11");CHECK(v->modified==1&&v->value==11);
 SV_IHVCvarGet("second","3",0);CHECK(SV_IHVCvarGet("EXAMPLE","7",0)==address);
 CHECK(std::string(reinterpret_cast<const char *>(static_cast<uintptr_t>(v->string)))=="11");
 Cvar_Get("secret","hidden",CVAR_PRIVATE);const int before=cvarGets;
 Reject([]{SV_IHVCvarGet("secret","bad",8);},"private cvar");CHECK(cvarGets==before&&Cvar_Flags("secret")==CVAR_PRIVATE);
 Reject([]{SV_IHVCvarSet("secret","bad");},"Invalid IHV cvar");CHECK(cvarGets==before);
 Reject([]{SV_IHVCvarSet("","bad");},"Invalid IHV cvar");
 variables["example"].record.flags|=CVAR_PRIVATE;Reject([]{SV_IHVSyncCvars();},"private cvar");
 variables["example"].record.flags&=~CVAR_PRIVATE;
 // Unset/reused engine index: retain native address and restore registration.
 variables["example"].record.name=nullptr;SV_IHVSyncCvars();CHECK(SV_IHVCvarGet("example","7",0)==address);
 SV_IHVClearGame();
}
static void Imports(){
 Reset();SV_IHVLoadGame();SV_IHVGameEntry(GAME_INIT);
 CHECK(imports->Milliseconds()==1234);imports->Printf("%s %d","text",1);imports->DebugGraph(1,1);
 void *file=reinterpret_cast<void *>(1);CHECK(imports->ReadFile("missing",&file)==-1&&!file);
 CHECK(imports->ReadFile("present",nullptr)==3);const int releases=fsFrees;
 CHECK(imports->ReadFile("present",&file)==3&&fsFrees==releases+1&&!std::memcmp(file,"abc",4));imports->FreeFile(file);
 Reject([&]{imports->FreeFile(file);},"unowned allocation");
 Reject([&]{imports->ReadFile(nullptr,&file);},"Invalid IHV file read");CHECK(!file);
 imports->WriteFile("save/test",nullptr,0);CHECK(writeText.empty());imports->WriteFile("save/test","abc",3);CHECK(writeText=="abc");
 const int writes=fsWrites;Reject([]{imports->WriteFile("../bad","x",1);},"Invalid IHV file write");
 Reject([]{imports->WriteFile("save/test","x",-1);},"Invalid IHV file write");Reject([]{imports->WriteFile("save/test",nullptr,1);},"Invalid IHV file write");CHECK(fsWrites==writes);
 CHECK(!std::strcmp(imports->FileForWriting("test.nav"),"home/baseq3/test.nav"));Reject([]{imports->FileForWriting("bad");},"write path rejected");
 imports->AddCommandText("echo text");CHECK(commandText=="echo text");
 auto *client=ihvGameEntities.At(1);auto *other=ihvGameEntities.At(2);
 imports->DropClient(client,"reason");CHECK(dropIndex==0);imports->SendServerCommand(other,"test %i",42);CHECK(commandIndex==1&&commandText=="test 42");
 imports->SendServerCommand(nullptr,"broadcast");CHECK(commandIndex==-1);
 Reject([]{imports->DropClient(exports.entities,"reason");},"non-client entity");
 imports->SetConfigstring(100,"test");CHECK(!std::strcmp(imports->GetConfigstring(100),"test")&&!imports->GetConfigstring(-1)[0]&&!imports->GetConfigstring(1024)[0]);
 imports->SetBrushModel(client,"*1");imports->UnlinkEntity(client);CHECK(unlinks==1);
 const float point[3]={1,2,3};auto trace=imports->Trace(point,nullptr,nullptr,point,2,15);
 CHECK(trace.fraction==0.25f&&trace.endpos[2]==3&&trace.startsolid==1&&trace.planeNormal[1]==1&&trace.planeDist==7&&trace.planeType==4&&trace.planeSignbits==2&&trace.surfaceFlags==19&&trace.contents==15&&trace.entityNum==2);
 CHECK(imports->PointContents(point,1)==123&&imports->InPVS(point,point)&&!imports->InPVSIgnorePortals(point,point)&&imports->AreasConnected(2,2));imports->AdjustAreaPortalState(client,7);
 Reject([&]{imports->InPVS(nullptr,point);},"PVS points");
 const float invalidPoint[3]={(std::numeric_limits<float>::quiet_NaN)(),0,0};
 Reject([&]{imports->InPVSIgnorePortals(point,invalidPoint);},"PVS points");
 contacts={1,2};other->bmodel=1;other->svFlags=0x80;other->currentOrigin[0]=99;other->currentAngles[1]=180;
 fntech3_q3_ihv_entity_t *list[3]={nullptr,nullptr,reinterpret_cast<fntech3_q3_ihv_entity_t *>(1)};
 CHECK(imports->ContactEntities(point,point,list,1)==1&&list[0]==client&&!list[1]);
 CHECK(imports->ContactEntities(point,point,list,2)==2&&list[1]==other&&contactOrigin[0]==99&&contactAngles[1]==180);
 CHECK(list[2]==reinterpret_cast<fntech3_q3_ihv_entity_t *>(1));other->svFlags=0;
 CHECK(imports->ContactEntities(point,point,list,2)==2&&contactOrigin[0]==0&&contactAngles[1]==0);
 contactSolid=false;CHECK(imports->ContactEntities(point,point,list,2)==1);contactSolid=true;
 CHECK(imports->ContactEntities(point,point,nullptr,0)==0);Reject([&]{imports->ContactEntities(point,point,nullptr,1);},"contact output");
 Reject([&]{imports->ContactEntities(invalidPoint,point,list,1);},"contact bounds");
 Reject([&]{imports->ContactEntities(point,vec3_origin,list,1);},"contact bounds");
 Reject([]{imports->LinkEntity(reinterpret_cast<fntech3_q3_ihv_entity_t *>(reinterpret_cast<char *>(exports.entities)+1));},"invalid entity");
 for(int invalid:{-1,0,17}){const int count=exports.entityCount;exports.entityCount=invalid;Reject([]{SV_IHVBindEntities();},"invalid entity storage");exports.entityCount=count;}
 exports.entityCapacity=8;SV_IHVBindEntities();CHECK(ihvBoundEntityCapacity==8);
 Stop();
}
/* Native client lifecycle, bot scheduling and snapshot history. These records
 * are separate from the retail client array and player projection. */
static void Bots(){
 using namespace fntech3::q3ihv;
 Reset();SV_IHVLoadGame();SV_IHVGameEntry(GAME_INIT);
 CHECK(ihvGameClients==2&&ihvGameClientRecords.Count()==2);
 auto *first=imports->BotAllocateClient();
 CHECK(first==ihvGameEntities.At(1)&&EntityNumber(*first)==1);
 CHECK(ihvGameClientRecords.State(0)==FNTECH3_Q3_IHV_CLIENT_CONNECTED);
 CHECK(ihvGameClientRecords.LastPacketTime(0)==sv.time&&ihvGameClientRecords.MessageCleared(0));
 CHECK(ihvGameClientRecords.HasEntity(0)&&!ihvGameClientRecords.HasEntity(1));
 auto *second=imports->BotAllocateClient();
 CHECK(second==ihvGameEntities.At(2)&&EntityNumber(*second)==2&&ihvGameClientRecords.State(1)==FNTECH3_Q3_IHV_CLIENT_CONNECTED);
 CHECK(!imports->BotAllocateClient()); // Every native slot is occupied.
 // The module receives the engine's own copy, which the engine then rereads.
 imports->BotSetUserinfo(first,"\\name\\Bot One\\rate\\25000\\snaps\\20");
 CHECK(userinfoChanges==1&&userinfoEntity==first&&userinfoText==ihvGameClientRecords.MutableUserinfo(0));
 CHECK(!std::strcmp(ihvGameClientRecords.Userinfo(0),"\\name\\Bot One\\rate\\25000\\snaps\\20"));
 CHECK(!std::strcmp(ihvGameClientRecords.Name(0),"Bot One"));
 CHECK(ihvGameClientRecords.Rate(0)==15000&&ihvGameClientRecords.SnapshotMsec(0)==50);
 imports->BotSetUserinfo(second,"\\rate\\50\\snaps\\0");
 CHECK(ihvGameClientRecords.Rate(1)==100&&ihvGameClientRecords.SnapshotMsec(1)==1000&&!ihvGameClientRecords.Name(1)[0]);
 imports->BotSetUserinfo(second,"\\name\\x");
 CHECK(ihvGameClientRecords.Rate(1)==5000&&ihvGameClientRecords.SnapshotMsec(1)==50);
 std::string oversized="\\name\\";oversized.append(1200,'a');
 imports->BotSetUserinfo(second,oversized.c_str());
 CHECK(std::strlen(ihvGameClientRecords.Userinfo(1))==1023&&std::strlen(ihvGameClientRecords.Name(1))==31);
 // Bot scheduling runs before the module's own frame, with that frame's step.
 first->svFlags=FNTECH3_Q3_IHV_SVF_BOT;
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1400)==0&&frames==1&&botBegins==1&&!botThinks&&botEntity==first);
 CHECK(ihvGameClientRecords.State(0)==FNTECH3_Q3_IHV_CLIENT_ACTIVE&&ihvGameClientRecords.LastPacketTime(0)==sv.time);
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1500)==0&&botBegins==1&&botThinks==1&&botThinkMsec==100&&botEntity==first);
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1550)==0&&botThinks==2&&botThinkMsec==50);
 // Releasing a bot keeps its slot and entity association; the name clears.
 imports->BotFreeClient(first);
 CHECK(ihvGameClientRecords.State(0)==FNTECH3_Q3_IHV_CLIENT_ZOMBIE&&!ihvGameClientRecords.Name(0)[0]);
 CHECK(std::strlen(ihvGameClientRecords.Userinfo(0))&&ihvGameClientRecords.HasEntity(0));
 CHECK(!imports->BotAllocateClient()); // A zombie is not a free slot.
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1600)==0&&botBegins==2&&botThinks==2);
 first->svFlags=0;
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1700)==0&&botBegins==2&&botThinks==2);
 // No native snapshot has been built, so every index is outside the frame.
 CHECK(imports->SnapshotEntityCount(first)==0);
 for(int index:{0,-1,1})Reject([&]{imports->SnapshotEntityNumber(first,index);},"outside the client");
 CHECK(ihvGameClientRecords.BindSnapshotRing(64)&&ihvGameClientRecords.SnapshotCapacity()==64);
 for(int i=0;i<64;++i){
  fntech3_q3_ihv_entityState_t state{};const short number=static_cast<short>(i*37-80);
  std::memcpy(state.bytes,&number,sizeof number);CHECK(ihvGameClientRecords.SetSnapshotState(i,state));
 }
 for(int sequence=0;sequence<96;++sequence){
  const int total=1+sequence%16,base=(sequence*3)%64;
  CHECK(ihvGameClientRecords.SetFrame(0,sequence,total,base)&&ihvGameClientRecords.SetOutgoingSequence(0,sequence));
  CHECK(imports->SnapshotEntityCount(first)==total);
  for(int i=0;i<total;++i)CHECK(imports->SnapshotEntityNumber(first,i)==((base+i)%64)*37-80);
  Reject([&]{imports->SnapshotEntityNumber(first,total);},"outside the client");
 }
 // A module that contradicts its own published number is rejected.
 CHECK(SetEntityNumber(*first,2));
 for(auto call:{+[]{imports->SnapshotEntityCount(ihvGameEntities.At(1));},
  +[]{imports->BotFreeClient(ihvGameEntities.At(1));},
  +[]{imports->BotSetUserinfo(ihvGameEntities.At(1),"");}})Reject(call,"contradicts slot");
 CHECK(SetEntityNumber(*first,1));
 for(auto call:{+[]{imports->SnapshotEntityCount(exports.entities);},
  +[]{imports->BotFreeClient(exports.entities);},
  +[]{imports->BotSetUserinfo(exports.entities,"");}})Reject(call,"non-client entity");
 exports.ClientUserinfoChanged=nullptr;
 Reject([&]{imports->BotSetUserinfo(first,"");},"ClientUserinfoChanged");
 exports.ClientUserinfoChanged=UserinfoChanged;
 // A missing bot export fails explicitly instead of dispatching a null call.
 first->svFlags=FNTECH3_Q3_IHV_SVF_BOT;exports.BotThink=nullptr;
 Reject([]{SV_IHVGameEntry(GAME_RUN_FRAME,1800);},"BotThink");
 exports.BotThink=BotThink;first->svFlags=0;
 second->svFlags=FNTECH3_Q3_IHV_SVF_BOT;exports.BotBegin=nullptr;
 Reject([]{SV_IHVGameEntry(GAME_RUN_FRAME,1900);},"BotBegin");
 exports.BotBegin=BotBegin;second->svFlags=0;
 Stop();CHECK(!ihvGameClientRecords.Count()&&!ihvGameClientRecords.SnapshotCapacity());
}
/* Native snapshot building: entity selection, the shared ring and the frame
 * history the two getters read. No packet is written or sent. */
static void PublishClient(int slot){
 auto *entity=ihvGameEntities.At(slot+1);
 entity->client=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(clientArray+slot*1464));
 entity->inuse=1;entity->linked=1;
}
static void SetPlayerView(int slot,float x,int viewheight){
 auto *record=clientArray+slot*1464;
 const float origin[3]={x,0,0};
 std::memcpy(record+FNTECH3_Q3_IHV_PLAYER_ORIGIN,origin,sizeof origin);
 std::memcpy(record+FNTECH3_Q3_IHV_PLAYER_VIEWHEIGHT,&viewheight,sizeof viewheight);
}
static void SetClusters(int number,int area,std::initializer_list<int> clusters){
 auto &record=sv.svEntities[number];record={};
 record.areanum=area;record.areanum2=-1;record.numClusters=0;
 for(int cluster:clusters)record.clusternums[record.numClusters++]=cluster;
}
static void VisibleCluster(int cluster){snapshotPvs[cluster>>3]|=static_cast<byte>(1<<(cluster&7));}
static std::vector<int> Snapshot(fntech3_q3_ihv_entity_t *viewer){
 std::vector<int> numbers;
 for(int i=0;i<imports->SnapshotEntityCount(viewer);++i)numbers.push_back(imports->SnapshotEntityNumber(viewer,i));
 return numbers;
}
static void Snapshots(){
 using namespace fntech3::q3ihv;
 const std::vector<int> all{2,3,4,5,6,7};
 Reset();SV_IHVLoadGame();SV_IHVGameEntry(GAME_INIT);
 CHECK(ihvGameClientRecords.SnapshotCapacity()==2*FNTECH3_Q3_IHV_SNAPSHOT_RING_PER_CLIENT);
 exports.entityCount=8;SV_IHVBindEntities();
 leafCount=4;areaBitsWritten=3;
 for(int leaf=0;leaf<leafCount;++leaf){leafArea[leaf]=leaf;leafCluster[leaf]=leaf;}
 std::memset(snapshotPvs,0,sizeof snapshotPvs);visibleHeadnodes.clear();
 for(int number=0;number<8;++number)SetClusters(number,0,{0});
 VisibleCluster(0);
 auto *viewer=imports->BotAllocateClient();
 CHECK(viewer==ihvGameEntities.At(1));
 PublishClient(0);SetPlayerView(0,0,26);
 imports->BotSetUserinfo(viewer,"\\snaps\\20");
 CHECK(ihvGameClientRecords.SnapshotMsec(0)==50);
 for(int number=2;number<8;++number){
  auto *entity=ihvGameEntities.At(number);
  CHECK(SetEntityNumber(*entity,number));entity->inuse=1;entity->linked=1;
 }
 viewer->svFlags=FNTECH3_Q3_IHV_SVF_BOT;
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1400)==0&&botBegins==1);
 CHECK(ihvGameClientRecords.State(0)==FNTECH3_Q3_IHV_CLIENT_ACTIVE);
 SV_IHVServerFrame();
 // The viewer's own entity is marked before the walk, so it is never selected.
 CHECK(Snapshot(viewer)==all);
 NativeClients::SnapshotPayload payload;
 CHECK(ihvGameClientRecords.FramePayload(0,ihvGameClientRecords.OutgoingSequence(0),payload));
 CHECK(payload.areabytes==3&&areaBitsArea==0&&payload.areabits[0]==1);
 float storedOrigin[3];
 std::memcpy(storedOrigin,payload.playerState.bytes+FNTECH3_Q3_IHV_PLAYER_ORIGIN,sizeof storedOrigin);
 CHECK(!storedOrigin[0]&&!storedOrigin[1]&&!storedOrigin[2]);
 // A bot returns before its next snapshot time is advanced (0x41b544), so
 // every server frame rebuilds its snapshot.
 CHECK(!ihvGameClientRecords.NextSnapshotTime(0));
 auto *removed=ihvGameEntities.At(3);removed->inuse=0;
 SV_IHVServerFrame();
 CHECK((Snapshot(viewer)==std::vector<int>{2,4,5,6,7}));
 // A network client's slot is left to its session's own pass.
 removed->inuse=1;CHECK(ihvGameClientRecords.SetRemote(0,true));
 sv.time+=1000;SV_IHVServerFrame();
 CHECK((Snapshot(viewer)==std::vector<int>{2,4,5,6,7}));
 CHECK(ihvGameClientRecords.SetRemote(0,false));
 // Unlinked, no-client, a disconnected area and an invisible cluster each drop
 // an entity. Broadcast ignores visibility but not the first two rules.
 for(int rule=0;rule<4;++rule){
  auto *entity=ihvGameEntities.At(4);
  entity->linked=rule!=0;entity->svFlags=rule==1?FNTECH3_Q3_IHV_SVF_NOCLIENT:0;
  SetClusters(4,rule==2?2:0,{rule==3?3:0});
  sv.time+=1000;SV_IHVServerFrame();
  CHECK((Snapshot(viewer)==std::vector<int>{2,3,5,6,7}));
  entity->svFlags|=FNTECH3_Q3_IHV_SVF_BROADCAST;
  sv.time+=1000;SV_IHVServerFrame();
  CHECK(Snapshot(viewer)==(rule<2?std::vector<int>{2,3,5,6,7}:all));
  entity->linked=1;entity->svFlags=0;SetClusters(4,0,{0});
 }
 // A second area reaches the viewer even when the first one does not.
 sv.svEntities[5].areanum=2;sv.svEntities[5].areanum2=0;
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(Snapshot(viewer)==all);
 sv.svEntities[5].areanum2=2;
 sv.time+=1000;SV_IHVServerFrame();
 CHECK((Snapshot(viewer)==std::vector<int>{2,3,4,6,7}));
 SetClusters(5,0,{0});
 // An overflowed cluster list is tested from the node its bounds straddled.
 sv.svEntities[6].numClusters=-1;sv.svEntities[6].lastCluster=42;
 sv.time+=1000;SV_IHVServerFrame();
 CHECK((Snapshot(viewer)==std::vector<int>{2,3,4,5,7}));
 visibleHeadnodes.push_back(42);
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(Snapshot(viewer)==all);
 SetClusters(6,0,{0});
 // A portal expands the view from its own second origin, once per snapshot.
 SetClusters(7,1,{1});
 sv.time+=1000;SV_IHVServerFrame();
 CHECK((Snapshot(viewer)==std::vector<int>{2,3,4,5,6}));
 auto *portal=ihvGameEntities.At(2);
 portal->svFlags=FNTECH3_Q3_IHV_SVF_PORTAL;
 const float elsewhere[3]={1,0,0};
 std::memcpy(portal->state+FNTECH3_Q3_IHV_ENTITY_ORIGIN2,elsewhere,sizeof elsewhere);
 VisibleCluster(1);
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(Snapshot(viewer)==all&&areaBitsArea==1);
 portal->svFlags=0;SetClusters(7,0,{0});
 std::memset(snapshotPvs,0,sizeof snapshotPvs);VisibleCluster(0);
 // The ring wraps without losing the frame the getters read.
 const int capacity=ihvGameClientRecords.SnapshotCapacity();
 for(int build=0;build<capacity;build+=static_cast<int>(all.size())){
  sv.time+=1000;SV_IHVServerFrame();
  CHECK(Snapshot(viewer)==all);
 }
 // The counter keeps growing past the ring so a stale delta can be detected.
 CHECK(ihvGameClientRecords.SnapshotCursor()>static_cast<std::uint64_t>(capacity));
 NativeClients::SnapshotFrame current{};
 CHECK(ihvGameClientRecords.CurrentFrame(0,current)
  &&ihvGameClientRecords.SnapshotEntitiesAvailable(current.first));
 // A published number that contradicts the entity's position is repaired.
 CHECK(SetEntityNumber(*ihvGameEntities.At(5),9));
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(EntityNumber(*ihvGameEntities.At(5))==5&&Snapshot(viewer)==all);
 // A connected client that is not active yet builds nothing.
 auto *second=imports->BotAllocateClient();
 CHECK(second==ihvGameEntities.At(2)&&ihvGameClientRecords.State(1)==FNTECH3_Q3_IHV_CLIENT_CONNECTED);
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(!ihvGameClientRecords.NextSnapshotTime(1)&&!imports->SnapshotEntityCount(second));
 Reject([&]{imports->SnapshotEntityNumber(second,0);},"outside the client");
 Stop();
}
/* The protocol-33 snapshot message: baselines, the delta base a client asks
 * for, the entity limit and the shorter list a truncated message records. */
static std::vector<std::uint8_t> WriteSnapshot(int slot){
 std::vector<std::uint8_t> message(fntech3::q3ihv::wire::MaxMessageBytes);
 const int bytes=SV_IHVServerSnapshotMessage(slot,message.data(),static_cast<int>(message.size()));
 CHECK(bytes>0);message.resize(static_cast<std::size_t>(bytes));return message;
}
static void SnapshotMessages(){
 using namespace fntech3::q3ihv;
 Reset();SV_IHVLoadGame();SV_IHVGameEntry(GAME_INIT);
 exports.entityCount=8;SV_IHVBindEntities();
 leafCount=1;areaBitsWritten=2;leafArea[0]=0;leafCluster[0]=0;
 std::memset(snapshotPvs,0,sizeof snapshotPvs);visibleHeadnodes.clear();
 for(int number=0;number<8;++number)SetClusters(number,0,{0});
 VisibleCluster(0);
 auto *viewer=imports->BotAllocateClient();
 PublishClient(0);SetPlayerView(0,0,26);
 imports->BotSetUserinfo(viewer,"\\snaps\\20");
 for(int number=2;number<8;++number){
  auto *entity=ihvGameEntities.At(number);
  CHECK(SetEntityNumber(*entity,number));entity->inuse=1;entity->linked=1;
  entity->state[FNTECH3_Q3_IHV_ENTITY_MODELINDEX]=static_cast<std::uint8_t>(number);
 }
 // The systeminfo string must advertise protocol 33 before anything is written.
 imports->SetConfigstring(0,"\\protocol\\33\\mapname\\test");
 // Baselines follow Quake II's rule: in use and carrying a model, a looping
 // sound or effect flags. Entity 3 qualifies for none of them.
 auto *plain=ihvGameEntities.At(3);plain->state[FNTECH3_Q3_IHV_ENTITY_MODELINDEX]=0;
 ihvGameEntities.At(4)->state[FNTECH3_Q3_IHV_ENTITY_LOOPSOUND]=9;
 ihvGameEntities.At(4)->state[FNTECH3_Q3_IHV_ENTITY_MODELINDEX]=0;
 const std::uint16_t effects=7;
 std::memcpy(ihvGameEntities.At(5)->state+FNTECH3_Q3_IHV_ENTITY_EFFECTS,&effects,sizeof effects);
 ihvGameEntities.At(5)->state[FNTECH3_Q3_IHV_ENTITY_MODELINDEX]=0;
 auto *absent=ihvGameEntities.At(6);absent->inuse=0;
 SV_IHVServerBaselines();
 absent->inuse=1;
 auto baselineNumber=[](int number){
  std::int16_t word=0;
  std::memcpy(&word,ihvGameState.baselines[static_cast<std::size_t>(number)].data(),sizeof word);
  return static_cast<int>(word);
 };
 CHECK(!baselineNumber(3)&&!baselineNumber(6));
 for(int number:{2,4,5,7})CHECK(baselineNumber(number)==number);
 CHECK(!std::strcmp(ihvGameState.configstrings[0].c_str(),"\\protocol\\33\\mapname\\test"));
 viewer->svFlags=FNTECH3_Q3_IHV_SVF_BOT;
 CHECK(fakeVm.entryPoint(GAME_RUN_FRAME,1400)==0&&botBegins==1);
 // Message one is a sequence of its own: the original never deltas against
 // sequence zero, because a nonpositive request means "no delta".
 CHECK(ihvGameClientRecords.SetOutgoingSequence(0,1));
 SV_IHVServerFrame();
 CHECK(imports->SnapshotEntityCount(viewer)==6);
 CHECK(!ihvGameClientRecords.DeltaMessage(0));
 const auto full=WriteSnapshot(0);
 CHECK(full.size()>1&&full[0]==7);
 // The header carries the client's own sequences, at fixed width.
 // Every configstring change advances the sequence the header carries.
 const int sequenceBefore=sv.configstringSequence;
 imports->SetConfigstring(200,"changed");
 CHECK(sv.configstringSequence==sequenceBefore+1);
 CHECK(ihvGameClientRecords.SetSequences(0,5,9,0x10));
 const auto second=WriteSnapshot(0);
 CHECK(second!=full&&second.size()==full.size());
 // A delta against the previous frame is shorter than a full message.
 CHECK(ihvGameClientRecords.SetOutgoingSequence(0,2));
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(ihvGameClientRecords.SetDeltaMessage(0,1));
 const auto delta=WriteSnapshot(0);
 CHECK(delta.size()<second.size());
 // A request the ring can no longer satisfy falls back to a full message.
 fntech3_q3_ihv_entityState_t filler{};
 for(int i=0;i<ihvGameClientRecords.SnapshotCapacity();++i)CHECK(ihvGameClientRecords.PushSnapshotState(filler));
 NativeClients::SnapshotFrame stale{};
 CHECK(ihvGameClientRecords.FrameAt(0,1,stale)&&!ihvGameClientRecords.SnapshotEntitiesAvailable(stale.first));
 CHECK(ihvGameClientRecords.SetOutgoingSequence(0,3));
 sv.time+=1000;SV_IHVServerFrame();
 CHECK(ihvGameClientRecords.SetDeltaMessage(0,1));
 const auto refused=WriteSnapshot(0);
 CHECK(refused.size()>delta.size());
 // A request more than twenty-eight messages behind is refused the same way.
 CHECK(SnapshotDeltaStale(40,11)&&!SnapshotDeltaStale(40,12)&&SnapshotDeltaStale(40,0)&&SnapshotDeltaStale(40,-3));
 // A buffer that cannot hold the message reports failure instead of writing.
 std::uint8_t narrow[4];
 // Developer padding prefixes the message with that many nops.
 padPackets.integer=3;
 const auto padded=WriteSnapshot(0);
 padPackets.integer=0;
 const auto unpadded=WriteSnapshot(0);
 CHECK(padded.size()==unpadded.size()+3);
 for(int i=0;i<3;++i)CHECK(padded[static_cast<std::size_t>(i)]==1);
 CHECK(!std::memcmp(padded.data()+3,unpadded.data(),unpadded.size()));
 CHECK(SV_IHVServerSnapshotMessage(0,narrow,sizeof narrow)<0);
 CHECK(SV_IHVServerSnapshotMessage(0,nullptr,16)<0);
 CHECK(SV_IHVServerSnapshotMessage(2,narrow,sizeof narrow)<0);
 Stop();
}
int main(){
 if(sizeof(void*)!=4){std::puts("IHV native game bridge requires an x86 host");return 77;}
 try{Lifecycle();Cvars();Imports();Bots();Snapshots();SnapshotMessages();}catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
 std::puts("IHV game bridge lifecycle, ownership, import ABI, bot clients, snapshots, protocol-33 messages and malformed inputs passed");return 0;
}
