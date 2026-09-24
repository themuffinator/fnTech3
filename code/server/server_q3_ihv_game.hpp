/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_Q3_IHV_GAME_HPP
#define FNTECH3_SERVER_Q3_IHV_GAME_HPP
#include "../compat/q3_ihv_native_boundary.hpp"
#include "../compat/q3_ihv_cvars.hpp"
#include "../compat/q3_ihv_server_clients.hpp"
#include "../compat/q3_ihv_snapshot_select.hpp"
#include "../qcommon/q3_ihv_message.hpp"
#include "../qcommon/filesystem_write_bounds.h"
#include "server_world_bounds.hpp"
#include <algorithm>
#include <cstdarg>
#include <new>

/* Native API 4 owns its game allocation for the server lifetime, and level
 * allocations between SpawnEntities calls. See Q3_IHV.md and the optional
 * original-module probes. No engine POD is exposed as an IHV record. */
namespace {
fntech3_q3_ihv_game_export_t *ihvGame=nullptr;
fntech3_q3_ihv_game_import_t ihvGameImports{};
fntech3::q3ihv::TaggedMemory ihvGameMemory(128u*1024u*1024u);
fntech3::q3ihv::EntityArray ihvGameEntities;
fntech3::q3ihv::NativeClients ihvGameClientRecords;
/* The gamestate the snapshot writer reads: the systeminfo string it checks for
 * protocol 33, and the entity baselines a newly visible entity is sent from. */
fntech3::q3ihv::wire::GameState ihvGameState;
bool ihvGameInitialized=false;
int ihvGameClients=0;
int ihvBoundEntityCapacity=0;
/* Frame length for the bot scheduler: the original advances its own time then
 * passes that step to 0x4177e0, so derive it from consecutive dispatches. */
int ihvGameFrameTime=0;
bool ihvGameFrameTimeKnown=false;

struct IhvGameCvar {
    fntech3::q3ihv::CvarMirror mirror;
    char name[256]{}, reset[1024]{};
    cvar_t *source=nullptr;
    int flags=0, serial=-1;
    IhvGameCvar *next=nullptr;
};
IhvGameCvar *ihvGameCvars=nullptr;

void SV_IHVSyncCvar(IhvGameCvar *node) {
    if(Cvar_Flags(node->name)&CVAR_PRIVATE)
        Com_Error(ERR_DROP,"IHV game requested private cvar %s",node->name);
    if(!node->source || !node->source->name || Q_stricmp(node->name,node->source->name))
        node->source=Cvar_Get(node->name,node->reset,fntech3::q3ihv::CvarFlagsToEngine(node->flags));
    const cvar_t *v=node->source;
    const int modified=node->serial==v->modificationCount?node->mirror.Record()->modified:1;
    if(!node->mirror.Assign(v->name,v->string,v->resetString,v->latchedString,
        fntech3::q3ihv::CvarFlagsToNative(v->flags,node->flags),modified,v->value,
        node->next?static_cast<uint32_t>(reinterpret_cast<uintptr_t>(node->next->mirror.Record())):0))
        Com_Error(ERR_DROP,"IHV cvar record cannot represent %s",node->name);
    node->serial=v->modificationCount;
}
void SV_IHVSyncCvars() { for(auto *node=ihvGameCvars;node;node=node->next)SV_IHVSyncCvar(node); }
void SV_IHVClearGame() {
    ihvGame=nullptr;ihvGameInitialized=false;ihvGameClients=0;ihvBoundEntityCapacity=0;
    ihvGameFrameTime=0;ihvGameFrameTimeKnown=false;ihvGameClientRecords.Clear();
    ihvGameState={};
    ihvGameEntities.Clear();InvalidateServerGameData();ihvGameMemory.Clear();
    while(ihvGameCvars){auto *node=ihvGameCvars;ihvGameCvars=node->next;delete node;}
}
bool SV_IHVValidateClient(void *,const void *record,int index,size_t bytes) {
    return ihvGameMemory.OwnsRecord(record,index,ihvGameClients,FNTECH3_Q3_IHV_GAME_CLIENT_BYTES,bytes);
}
void SV_IHVBindEntities() {
    if(!ihvGame || !ihvGame->entities || !ihvGameClients)
        Com_Error(ERR_DROP,"IHV game has no entity allocation");
    const size_t span=ihvGameMemory.AllocationSize(ihvGame->entities);
    if(!ihvGameEntities.Bind(ihvGame->entities,span,ihvGame->entityStride,
        ihvGame->entityCount,ihvGame->entityCapacity))
        Com_Error(ERR_DROP,"IHV game published invalid entity storage");
    if(serverGameData.IsBound() && sv.gentities==ihvGame->entities && sv.gentitySize==ihvGame->entityStride
        && serverGameData.EntityCount()==ihvGame->entityCount && ihvBoundEntityCapacity==ihvGame->entityCapacity)return;
    fntech3::server::NativeGameDataBinding binding;
    binding.entityBase=ihvGame->entities;binding.entitySpanBytes=span;
    binding.entityCount=ihvGame->entityCount;binding.entityCapacity=ihvGame->entityCapacity;
    binding.entityStride=ihvGame->entityStride;binding.clientCount=ihvGameClients;
    binding.validateClientRecord=SV_IHVValidateClient;
    const auto error=serverGameData.BindNative(COM_GAME_QUAKE3_IHV,binding);
    if(error!=fntech3::server::GameDataBindError::None)
        Com_Error(ERR_DROP,"Invalid IHV game data: %s",fntech3::server::GameDataBindErrorName(error));
    sv.gentities=ihvGame->entities;sv.gentitySize=ihvGame->entityStride;sv.num_entities=ihvGame->entityCount;
    ihvBoundEntityCapacity=ihvGame->entityCapacity;
}
sharedEntity_t *SV_IHVEntity(fntech3_q3_ihv_entity_t *entity) {
    SV_IHVBindEntities();
    if(ihvGameEntities.Index(entity,true)<0)Com_Error(ERR_DROP,"IHV import received an invalid entity");
    return reinterpret_cast<sharedEntity_t *>(entity);
}
void SV_IHVPrintf(const char *fmt,...) {
    char text[MAXPRINTMSG];va_list args;va_start(args,fmt);Q_vsnprintf(text,sizeof text,fmt,args);va_end(args);
    Com_Printf("%s",text);
}
void SV_IHVError(int level,const char *fmt,...) {
    char text[MAXPRINTMSG];va_list args;va_start(args,fmt);Q_vsnprintf(text,sizeof text,fmt,args);va_end(args);
    Com_Error(level==1?ERR_DROP:ERR_FATAL,"%s",text);
}
int SV_IHVMilliseconds(){return Sys_Milliseconds();}
void *SV_IHVAllocate(int bytes,int tag) {
    void *result=ihvGameMemory.Allocate(bytes,tag);
    if(!result)Com_Error(ERR_DROP,"IHV game allocation rejected: %i bytes, tag %i",bytes,tag);
    return result;
}
void SV_IHVFree(void *pointer) {
    if(!ihvGameMemory.Free(pointer))Com_Error(ERR_DROP,"IHV game freed an unowned allocation");
}
void SV_IHVFreeTags(int tag){ihvGameMemory.FreeTags(tag);}
fntech3_q3_ihv_cvar_t *SV_IHVCvarGet(const char *name,const char *value,int flags) {
    if(!name || !value || !*name || strlen(name)>=256 || strlen(value)>=1024)
        Com_Error(ERR_DROP,"Invalid IHV cvar registration");
    auto *node=ihvGameCvars;
    while(node && Q_stricmp(name,node->name))node=node->next;
    if(Cvar_Flags(name)&CVAR_PRIVATE)Com_Error(ERR_DROP,"IHV game requested private cvar %s",name);
    cvar_t *source=Cvar_Get(name,value,fntech3::q3ihv::CvarFlagsToEngine(flags));
    if(!node) {
        node=new(std::nothrow) IhvGameCvar;
        if(!node)Com_Error(ERR_DROP,"IHV cvar allocation failed");
        Q_strncpyz(node->name,name,sizeof node->name);Q_strncpyz(node->reset,value,sizeof node->reset);
        node->next=ihvGameCvars;ihvGameCvars=node;
    }
    node->flags|=flags;node->source=source;SV_IHVSyncCvar(node);return node->mirror.Record();
}
void SV_IHVCvarSet(const char *name,const char *value) {
    if(!name || !value || !*name || strlen(name)>=256 || strlen(value)>=1024 || (Cvar_Flags(name)&CVAR_PRIVATE))
        Com_Error(ERR_DROP,"Invalid IHV cvar assignment");
    Cvar_Set2(name,value,qtrue);SV_IHVSyncCvars();
}
int SV_IHVArgc(){return Cmd_Argc();}
char *SV_IHVArgv(int n){return const_cast<char *>(Cmd_Argv(n));}
int SV_IHVReadFile(const char *name,void **out) {
    if(out)*out=nullptr;
    if(!name || !*name)Com_Error(ERR_DROP,"Invalid IHV file read");
    void *input=nullptr;
    const int length=FS_ReadFile(name,out?&input:nullptr);
    if(!out || length<0)return length;
    void *copy=length<INT_MAX?ihvGameMemory.Allocate(length+1,0):nullptr;
    if(copy && length)memcpy(copy,input,length);
    if(input)FS_FreeFile(input);
    if(!copy)Com_Error(ERR_DROP,"IHV file allocation rejected: %s",name);
    *out=copy;return length;
}
void SV_IHVWriteFile(const char *name,const void *data,int length) {
    if(!FS_WriteQpathIsValid(name) || length<0 || (length && !data))
        Com_Error(ERR_DROP,"Invalid IHV file write");
    FS_WriteFile(name,data?data:"",length);
}
char *SV_IHVFileForWriting(const char *name) {
    static char path[MAX_OSPATH*3];
    if(!FS_NativeWritePath(name,path,sizeof path))Com_Error(ERR_DROP,"IHV write path rejected: %s",name?name:"<null>");
    return path;
}
void SV_IHVAddCommandText(const char *text){Cbuf_AddText(text);}
void SV_IHVDebugGraph(float,int) { /* No graph display on a dedicated server. */ }
int SV_IHVClientIndex(fntech3_q3_ihv_entity_t *entity) {
    SV_IHVBindEntities();const int index=ihvGameEntities.ClientIndex(entity,ihvGameClients);
    if(!SV_IsClientIndex(index))Com_Error(ERR_DROP,"IHV import received a non-client entity");
    return index;
}
void SV_IHVDropClient(fntech3_q3_ihv_entity_t *entity,const char *reason){SV_GameDropClient(SV_IHVClientIndex(entity),reason);}
void SV_IHVSendServerCommand(fntech3_q3_ihv_entity_t *entity,const char *fmt,...) {
    char text[MAX_STRING_CHARS];va_list args;va_start(args,fmt);Q_vsnprintf(text,sizeof text,fmt,args);va_end(args);
    SV_GameSendServerCommand(entity?SV_IHVClientIndex(entity):-1,text);
}
void SV_IHVSetConfigstring(int index,const char *value){SV_SetConfigstring(index,value);}
char *SV_IHVGetConfigstring(int index) {
    static char empty[]="";
    return index>=0 && index<FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS && sv.configstrings[index]?sv.configstrings[index]:empty;
}
void SV_IHVSetBrushModel(fntech3_q3_ihv_entity_t *entity,const char *name){SV_SetBrushModel(SV_IHVEntity(entity),name);}
fntech3_q3_ihv_trace_t SV_IHVTrace(const float *start,const float *mins,const float *maxs,const float *end,int pass,int mask) {
    SV_IHVBindEntities();trace_t trace{};SV_Trace(&trace,start,mins,maxs,end,pass,mask,qfalse);
    fntech3_q3_ihv_trace_t result{};
    result.allsolid=trace.allsolid;result.startsolid=trace.startsolid;result.fraction=trace.fraction;
    VectorCopy(trace.endpos,result.endpos);VectorCopy(trace.plane.normal,result.planeNormal);
    result.planeDist=trace.plane.dist;result.planeType=trace.plane.type;result.planeSignbits=trace.plane.signbits;
    result.surfaceFlags=trace.surfaceFlags;result.contents=trace.contents;result.entityNum=trace.entityNum;return result;
}
int SV_IHVPointContents(const float *point,int pass){SV_IHVBindEntities();return SV_PointContents(point,pass);}
int SV_IHVInPVS(const float *a,const float *b){
    if(!SV_FiniteWorldVector(a) || !SV_FiniteWorldVector(b))Com_Error(ERR_DROP,"Invalid IHV PVS points");
    return SV_inPVS(a,b);
}
int SV_IHVInPVSIgnorePortals(const float *a,const float *b){
    if(!SV_FiniteWorldVector(a) || !SV_FiniteWorldVector(b))Com_Error(ERR_DROP,"Invalid IHV PVS points");
    return SV_inPVSIgnorePortals(a,b);
}
void SV_IHVAdjustAreaPortal(fntech3_q3_ihv_entity_t *entity,int open){SV_AdjustAreaPortalState(SV_IHVEntity(entity),open!=0);}
int SV_IHVAreasConnected(int a,int b){return CM_AreasConnected(a,b);}
void SV_IHVLink(fntech3_q3_ihv_entity_t *entity){SV_LinkEntity(SV_IHVEntity(entity));}
void SV_IHVUnlink(fntech3_q3_ihv_entity_t *entity){SV_UnlinkEntity(SV_IHVEntity(entity));}
int SV_IHVContactEntities(const float *mins,const float *maxs,fntech3_q3_ihv_entity_t **list,int capacity) {
    SV_IHVBindEntities();
    if(capacity<0 || (capacity && !list))Com_Error(ERR_DROP,"IHV contact output is invalid");
    if(!capacity)return 0;
    if(!SV_ValidWorldBounds(mins,maxs))Com_Error(ERR_DROP,"Invalid IHV contact bounds");
    int numbers[MAX_GENTITIES];const int found=SV_AreaEntities(mins,maxs,numbers,MAX_GENTITIES);
    int count=0;
    for(int i=0;i<found && count<capacity;++i) {
        auto *entity=ihvGameEntities.At(numbers[i],true);
        if(!entity)Com_Error(ERR_DROP,"IHV contact entity outside allocation");
        if(entity->bmodel) {
            sharedEntity_t view;
            auto *handle=reinterpret_cast<sharedEntity_t *>(entity);
            if(!SV_ReadGameEntity(handle,&view))Com_Error(ERR_DROP,"IHV contact entity unavailable");
            const bool current=(entity->svFlags&FNTECH3_Q3_IHV_SVF_CURRENT_ORIGIN)!=0;
            trace_t trace{};
            CM_TransformedBoxTrace(&trace,vec3_origin,vec3_origin,mins,maxs,SV_ClipHandleForEntity(handle,qfalse),-1,
                current?view.r.currentOrigin:view.s.origin,current?view.r.currentAngles:view.s.angles,qfalse);
            if(!trace.startsolid)continue;
        }
        list[count++]=entity;
    }
    return count;
}
/* Native bot and snapshot imports. The original derives the slot from the
 * entity's own signed-short number (0x4176e0, 0x417720, 0x41b370, 0x41b3b0)
 * and prints a warning when that word disagrees with the entity's position.
 * Validate the pointer against the bounded entity array first and reject a
 * module that publishes a number its own storage contradicts. These records
 * are separate from the retail client array and player projection. */
int SV_IHVClientSlot(fntech3_q3_ihv_entity_t *entity) {
    const int slot=SV_IHVClientIndex(entity);
    const int number=fntech3::q3ihv::EntityNumber(*entity);
    if(number!=slot+1)Com_Error(ERR_DROP,"IHV client entity number %i contradicts slot %i",number,slot);
    int record=-1;
    if(!ihvGameClientRecords.SlotForNumber(number,record))
        Com_Error(ERR_DROP,"IHV client %i has no native record",slot);
    return record;
}
fntech3::q3ihv::NativeClients::SnapshotFrame SV_IHVClientFrame(fntech3_q3_ihv_entity_t *entity) {
    fntech3::q3ihv::NativeClients::SnapshotFrame frame{};
    if(!ihvGameClientRecords.CurrentFrame(SV_IHVClientSlot(entity),frame))
        Com_Error(ERR_DROP,"IHV client has no native snapshot history");
    return frame;
}
int SV_IHVSnapshotCount(fntech3_q3_ihv_entity_t *entity){return SV_IHVClientFrame(entity).count;}
int SV_IHVSnapshotNumber(fntech3_q3_ihv_entity_t *entity,int index) {
    const auto frame=SV_IHVClientFrame(entity);
    int number=0;
    if(index<0 || index>=frame.count || !ihvGameClientRecords.SnapshotNumber(frame.first,index,number))
        Com_Error(ERR_DROP,"IHV snapshot entity %i is outside the client's %i entities",index,frame.count);
    return number;
}
fntech3_q3_ihv_entity_t *SV_IHVBotAllocate() {
    SV_IHVBindEntities();
    const int slot=ihvGameClientRecords.FirstFreeSlot();
    if(slot<0)return nullptr; // The original reports a full client array this way.
    auto *entity=ihvGameEntities.At(slot+1);
    if(!entity)Com_Error(ERR_DROP,"IHV client %i has no spawned entity",slot);
    if(!fntech3::q3ihv::SetEntityNumber(*entity,slot+1) || !ihvGameClientRecords.Connect(slot,sv.time))
        Com_Error(ERR_DROP,"IHV bot client %i cannot be published",slot);
    return entity;
}
void SV_IHVBotFree(fntech3_q3_ihv_entity_t *entity) {
    // Only the slot and visible name change; entity cleanup belongs to the game.
    if(!ihvGameClientRecords.Release(SV_IHVClientSlot(entity)))
        Com_Error(ERR_DROP,"IHV bot release found no connected client");
}
void SV_IHVBotUserinfo(fntech3_q3_ihv_entity_t *entity,const char *userinfo) {
    const int slot=SV_IHVClientSlot(entity);
    if(!ihvGame->ClientUserinfoChanged)Com_Error(ERR_DROP,"IHV game has no ClientUserinfoChanged export");
    if(!ihvGameClientRecords.CopyUserinfo(slot,userinfo))Com_Error(ERR_DROP,"Invalid IHV bot userinfo");
    char *stored=ihvGameClientRecords.MutableUserinfo(slot);
    ihvGame->ClientUserinfoChanged(entity,stored);
    // Info_ValueForKey rotates two buffers, so read the module's copy in turn.
    char name[MAX_INFO_VALUE],rate[MAX_INFO_VALUE],snaps[MAX_INFO_VALUE];
    Q_strncpyz(name,Info_ValueForKey(stored,"name"),sizeof name);
    Q_strncpyz(rate,Info_ValueForKey(stored,"rate"),sizeof rate);
    Q_strncpyz(snaps,Info_ValueForKey(stored,"snaps"),sizeof snaps);
    if(!ihvGameClientRecords.ApplyUserinfo(slot,name,rate,snaps))
        Com_Error(ERR_DROP,"IHV client %i cannot record its userinfo",slot);
}
/* Native snapshot building. SV_BuildClientSnapshot 0x41b210 selects what a
 * client can see from its own view point, sorts those entity numbers and copies
 * each 164-byte native state into the shared ring, leaving the count and ring
 * position in the client's frame. Nothing here writes or sends a packet: the
 * protocol-33 message writer is separate and still gated. The walk itself lives
 * in q3_ihv_snapshot_select.hpp so the original-function probe drives the same
 * code; these callbacks supply the engine's collision and link records. */
int SV_IHVPointLeafnum(void *,const float *point){return CM_PointLeafnum(point);}
int SV_IHVLeafArea(void *,int leaf){return CM_LeafArea(leaf);}
int SV_IHVLeafCluster(void *,int leaf){return CM_LeafCluster(leaf);}
int SV_IHVWriteAreaBits(void *,unsigned char *buffer,int area){return CM_WriteAreaBits(buffer,area);}
const unsigned char *SV_IHVClusterPVS(void *,int cluster){return CM_ClusterPVS(cluster);}
bool SV_IHVAreasConnectedFor(void *,int a,int b){return CM_AreasConnected(a,b)!=qfalse;}
bool SV_IHVHeadnodeVisible(void *,int node,const unsigned char *bits){
    return CM_HeadnodeVisible(node,bits)!=qfalse;
}
fntech3_q3_ihv_entity_t *SV_IHVSnapshotEntityAt(void *,int number){return ihvGameEntities.At(number);}
bool SV_IHVReadSnapshotLink(void *,int number,fntech3::q3ihv::SnapshotLink &link) {
    if(number<0 || number>=MAX_GENTITIES)return false;
    const svEntity_t &record=sv.svEntities[number];
    link.numClusters=record.numClusters;link.headnode=record.lastCluster;
    link.areanum=record.areanum;link.areanum2=record.areanum2;
    if(record.numClusters>0) {
        const int clusters=record.numClusters>MAX_ENT_CLUSTERS?MAX_ENT_CLUSTERS:record.numClusters;
        for(int i=0;i<clusters;++i)link.clusternums[i]=record.clusternums[i];
    }
    return true;
}
bool SV_IHVSnapshotAdded(void *,int number) {
    return number>=0 && number<MAX_GENTITIES && sv.svEntities[number].snapshotCounter==sv.snapshotCounter;
}
void SV_IHVMarkSnapshotAdded(void *,int number) {
    if(number>=0 && number<MAX_GENTITIES)sv.svEntities[number].snapshotCounter=sv.snapshotCounter;
}
void SV_IHVRepairedNumber(void *,int number,int published) {
    Com_DPrintf("IHV entity %i published number %i\n",number,published);
}
fntech3::q3ihv::SnapshotHost SV_IHVSnapshotHost() {
    fntech3::q3ihv::SnapshotHost host;
    host.pvsBytes=CM_ClusterBytes();host.entityCount=ihvGame?ihvGame->entityCount:0;
    host.PointLeafnum=SV_IHVPointLeafnum;host.LeafArea=SV_IHVLeafArea;host.LeafCluster=SV_IHVLeafCluster;
    host.WriteAreaBits=SV_IHVWriteAreaBits;host.ClusterPVS=SV_IHVClusterPVS;
    host.AreasConnected=SV_IHVAreasConnectedFor;host.HeadnodeVisible=SV_IHVHeadnodeVisible;
    host.EntityAt=SV_IHVSnapshotEntityAt;host.ReadLink=SV_IHVReadSnapshotLink;
    host.Added=SV_IHVSnapshotAdded;host.MarkAdded=SV_IHVMarkSnapshotAdded;
    host.RepairedNumber=SV_IHVRepairedNumber;
    return host;
}
void SV_IHVBuildClientSnapshot(int slot) {
    if(sv.state==SS_DEAD)return; // No map is loaded, so nothing can be seen.
    SV_IHVBindEntities();
    auto *entity=ihvGameEntities.At(slot+1);
    if(!entity)Com_Error(ERR_DROP,"IHV client %i has no spawned entity",slot);
    // The viewer's own entity counts as already added before the walk starts.
    ++sv.snapshotCounter;
    SV_IHVMarkSnapshotAdded(nullptr,fntech3::q3ihv::EntityNumber(*entity));
    const int sequence=ihvGameClientRecords.OutgoingSequence(slot);
    fntech3::q3ihv::NativeClients::SnapshotPayload payload;
    // The player state is the game's own client record, not a retail projection.
    if(!serverGameData.ReadPlayerState(slot,payload.playerState))
        Com_Error(ERR_DROP,"IHV client %i has no published player state",slot);
    float origin[3];
    memcpy(origin,payload.playerState.bytes+FNTECH3_Q3_IHV_PLAYER_ORIGIN,sizeof origin);
    int viewheight=0;
    memcpy(&viewheight,payload.playerState.bytes+FNTECH3_Q3_IHV_PLAYER_VIEWHEIGHT,sizeof viewheight);
    origin[2]+=static_cast<float>(viewheight);
    if(!SV_FiniteWorldVector(origin))Com_Error(ERR_DROP,"Invalid IHV snapshot viewpoint");
    fntech3::q3ihv::SnapshotSelection selection;
    if(!selection.FromPoint(SV_IHVSnapshotHost(),origin,payload.areabits,payload.areabytes))
        Com_Error(ERR_DROP,"IHV client %i cannot select its snapshot entities",slot);
    selection.Sort();
    const int first=ihvGameClientRecords.SnapshotCursor();
    for(int i=0;i<selection.Count();++i) {
        auto *selected=ihvGameEntities.At(selection.Number(i));
        if(!selected)Com_Error(ERR_DROP,"IHV snapshot selected entity %i outside the game array",selection.Number(i));
        fntech3_q3_ihv_entityState_t state;
        memcpy(state.bytes,selected->state,sizeof state.bytes);
        if(!ihvGameClientRecords.PushSnapshotState(state))
            Com_Error(ERR_DROP,"IHV snapshot ring is unavailable");
    }
    if(!ihvGameClientRecords.SetFramePayload(slot,sequence,payload.playerState,payload.areabits,payload.areabytes)
        || !ihvGameClientRecords.SetFrame(slot,sequence,selection.Count(),first))
        Com_Error(ERR_DROP,"IHV client %i cannot record its snapshot",slot);
}
/* Protocol-33 snapshot writing. SV_WriteSnapshotToClient 0x41add0 chooses a
 * delta base from the client's retained frames, then emits one svc_snapshot
 * whose header, area bits, player delta and entity deltas the shared codec in
 * q3_ihv_message.hpp already encodes. Nothing here transmits the message: the
 * netchan and the connected client session remain gated. */
void SV_IHVCreateBaselines() {
    SV_IHVBindEntities();
    for(auto &baseline:ihvGameState.baselines)baseline={};
    ihvGameState.configstrings[0]=SV_IHVGetConfigstring(0);
    for(int number=1;number<ihvGame->entityCount;++number) {
        if(number>=static_cast<int>(ihvGameState.baselines.size()))
            Com_Error(ERR_DROP,"IHV baseline %i is outside the protocol's entity range",number);
        auto *entity=ihvGameEntities.At(number);
        if(!entity)Com_Error(ERR_DROP,"IHV baseline reached entity %i outside the game array",number);
        if(!fntech3::q3ihv::BaselineQualifies(*entity))continue;
        if(!fntech3::q3ihv::SetEntityNumber(*entity,number))
            Com_Error(ERR_DROP,"IHV entity %i cannot hold its own number",number);
        memcpy(ihvGameState.baselines[static_cast<std::size_t>(number)].data(),
            entity->state,FNTECH3_Q3_IHV_ENTITY_STATE_BYTES);
    }
}
bool SV_IHVSnapshotForFrame(int slot,int sequence,fntech3::q3ihv::wire::Snapshot &out) {
    fntech3::q3ihv::NativeClients::SnapshotFrame frame{};
    fntech3::q3ihv::NativeClients::SnapshotPayload payload;
    if(!ihvGameClientRecords.FrameAt(slot,sequence,frame)
        || !ihvGameClientRecords.FramePayload(slot,sequence,payload))return false;
    out.messageNumber=static_cast<std::uint32_t>(sequence);
    out.areaBytes=static_cast<std::uint8_t>(payload.areabytes);
    memcpy(out.areaMask.data(),payload.areabits,sizeof payload.areabits);
    memcpy(out.player.data(),payload.playerState.bytes,out.player.size());
    out.firstEntity=frame.first;
    out.entities.clear();
    for(int i=0;i<frame.count;++i) {
        fntech3_q3_ihv_entityState_t state;
        if(!ihvGameClientRecords.SnapshotState(frame.first,i,state))return false;
        fntech3::q3ihv::wire::Entity entity;
        memcpy(entity.data(),state.bytes,entity.size());
        out.entities.push_back(entity);
    }
    return true;
}
void SV_IHVWriteSnapshotToClient(int slot,fntech3::q3ihv::wire::MessageWriter &writer) {
    const int sequence=ihvGameClientRecords.OutgoingSequence(slot);
    const int requested=ihvGameClientRecords.DeltaMessage(slot);
    fntech3::q3ihv::wire::Snapshot previous;
    const fntech3::q3ihv::wire::Snapshot *base=nullptr;
    int deltaSequence=-1;
    if(fntech3::q3ihv::SnapshotDeltaStale(sequence,requested)) {
        if(requested>0)
            Com_Printf("%s: Delta request from out of date packet.\n",ihvGameClientRecords.Name(slot));
    } else {
        fntech3::q3ihv::NativeClients::SnapshotFrame old{};
        if(!ihvGameClientRecords.FrameAt(slot,requested,old)
            || !ihvGameClientRecords.SnapshotEntitiesAvailable(old.first))
            Com_Printf("%s: Delta request from out of date entities.\n",ihvGameClientRecords.Name(slot));
        else if(!SV_IHVSnapshotForFrame(slot,requested,previous))
            Com_Error(ERR_DROP,"IHV client %i cannot reread snapshot %i",slot,requested);
        else { base=&previous; deltaSequence=requested; }
    }
    // 0x41ae62 pads the message with nops for rate debugging before the
    // snapshot itself, using the same developer cvar the other profiles do.
    for(int pad=0;pad<sv_padPackets->integer;++pad)
        if(!writer.AddNop())Com_Error(ERR_DROP,"IHV snapshot padding does not fit");
    fntech3::q3ihv::wire::Snapshot frame;
    if(!SV_IHVSnapshotForFrame(slot,sequence,frame))
        Com_Error(ERR_DROP,"IHV client %i has no snapshot to write",slot);
    frame.serverTime=static_cast<std::uint32_t>(sv.time);
    frame.deltaNumber=static_cast<std::uint32_t>(deltaSequence);
    frame.configSequence=static_cast<std::uint32_t>(sv.configstringSequence);
    frame.commandSequence=static_cast<std::uint32_t>(ihvGameClientRecords.CommandSequence(slot));
    frame.flags=static_cast<std::uint8_t>(ihvGameClientRecords.SnapshotFlags(slot));
    std::size_t written=0;
    if(!writer.AddSnapshot(ihvGameState,base,frame,FNTECH3_Q3_IHV_SNAPSHOT_MESSAGE_LIMIT,&written))
        Com_Error(ERR_DROP,"IHV client %i cannot encode its snapshot",slot);
    // A truncated list is what the client received, so later deltas must match.
    if(written<frame.entities.size()
        && !ihvGameClientRecords.SetFrame(slot,sequence,static_cast<int>(written),frame.firstEntity))
        Com_Error(ERR_DROP,"IHV client %i cannot shorten its snapshot",slot);
}
/* SV_SendClientMessages 0x41b6a0 visits every client once per server frame and
 * builds a snapshot for an active one whose interval has elapsed. Sending is
 * skipped for bots (0x41b544) and remains unimplemented for everyone else. */
void SV_IHVBuildClientSnapshots() {
    if(!ihvGame || !ihvGameInitialized || !ihvGameClientRecords.Count())return;
    for(int slot=0;slot<ihvGameClientRecords.Count();++slot) {
        if(ihvGameClientRecords.State(slot)!=fntech3::q3ihv::FNTECH3_Q3_IHV_CLIENT_ACTIVE)continue;
        if(sv.time<ihvGameClientRecords.NextSnapshotTime(slot))continue;
        SV_IHVBuildClientSnapshot(slot);
        ihvGameClientRecords.SetNextSnapshotTime(slot,sv.time+ihvGameClientRecords.SnapshotMsec(slot));
    }
}
/* Bot scheduler 0x4177e0 runs before the game's RunFrame in the same simulated
 * step, visiting every associated client entity that carries the native bot
 * flag. Per-client step 0x417780 promotes a client that is not active yet. */
void SV_IHVBotFrame(int msec) {
    if(!ihvGameClientRecords.Count())return;
    SV_IHVBindEntities();
    for(int slot=0;slot<ihvGameClientRecords.Count();++slot) {
        auto *entity=ihvGameEntities.At(slot+1);
        if(!entity || !ihvGameClientRecords.HasEntity(slot))continue;
        if(!(entity->svFlags&FNTECH3_Q3_IHV_SVF_BOT))continue;
        const auto step=ihvGameClientRecords.StepBot(slot,sv.time);
        if(step==fntech3::q3ihv::NativeClients::BotStep::Begin) {
            if(!ihvGame->BotBegin)Com_Error(ERR_DROP,"IHV game has no BotBegin export");
            ihvGame->BotBegin(entity);
        } else if(step==fntech3::q3ihv::NativeClients::BotStep::Think) {
            if(!ihvGame->BotThink)Com_Error(ERR_DROP,"IHV game has no BotThink export");
            ihvGame->BotThink(entity,msec);
        }
    }
}

intptr_t QDECL SV_IHVGameEntry(int command,...) {
    if(command==GAME_SHUTDOWN) {
        if(ihvGameInitialized) {
            ihvGameInitialized=false; // Error cleanup must not re-enter Shutdown.
            ihvGame->Shutdown();
        }
        Com_DPrintf("IHV shutdown: %zu allocations, %zu bytes before host cleanup\n",
            ihvGameMemory.Count(),ihvGameMemory.Bytes());
        SV_IHVClearGame();return 0;
    }
    if(!ihvGame)Com_Error(ERR_DROP,"IHV game called without exports");
    SV_IHVSyncCvars();
    intptr_t result=0;
    switch(command) {
    case GAME_INIT:
        if(!ihvGameInitialized) {
            ihvGame->Init();ihvGameInitialized=true;
            ihvGameClients=Cvar_VariableIntegerValue("maxclients");
        }
        if(ihvGameClients<1 || ihvGameClients>Com_GameClientLimit() || sv.maxclients>ihvGameClients)
            Com_Error(ERR_DROP,"IHV sv_maxclients (%i) exceeds native maxclients (%i)",sv.maxclients,ihvGameClients);
        SV_IHVBindEntities();
        // Native clients outlive a level spawn, as the original's array does.
        // The shared snapshot ring is maxclients * 2048 states (0x419737).
        if(!ihvGameClientRecords.Count()
            && (!ihvGameClientRecords.Reset(ihvGameClients,ihvGame->entityCapacity)
                || !ihvGameClientRecords.BindSnapshotRing(ihvGameClients*FNTECH3_Q3_IHV_SNAPSHOT_RING_PER_CLIENT)))
            Com_Error(ERR_DROP,"IHV native client records cannot cover %i clients",ihvGameClients);
        ihvGameFrameTime=sv.time;ihvGameFrameTimeKnown=true;
        ihvGame->SpawnEntities(Cvar_VariableString("mapname"),sv.entityParsePoint?sv.entityParsePoint:"",sv.time);
        Com_DPrintf("IHV spawn: %s at %i, entities=%i/%i stride=%i base=%p, %zu allocations, %zu bytes\n",
            Cvar_VariableString("mapname"),sv.time,ihvGame->entityCount,ihvGame->entityCapacity,
            ihvGame->entityStride,ihvGame->entities,ihvGameMemory.Count(),ihvGameMemory.Bytes());
        break;
    case GAME_RUN_FRAME: {
        va_list args;va_start(args,command);const int time=va_arg(args,int);va_end(args);
        /* 0x41ac3d advances the server time, runs the bot scheduler with that
         * step and only then calls RunFrame. The engine dispatches one command
         * per step, so the step is the distance from the previous dispatch. */
        const int msec=ihvGameFrameTimeKnown && time>ihvGameFrameTime?time-ihvGameFrameTime:0;
        ihvGameFrameTime=time;ihvGameFrameTimeKnown=true;
        SV_IHVBotFrame(msec);
        ihvGame->RunFrame(time);break;
    }
    case GAME_CONSOLE_COMMAND:result=ihvGame->ConsoleCommand();break;
    case GAME_CLIENT_CONNECT:
        return reinterpret_cast<intptr_t>("IHV protocol-33 client integration is not yet available");
    default:Com_Error(ERR_DROP,"IHV game export %i is not yet integrated",command);break;
    }
    SV_IHVBindEntities();return result;
}
vm_t *SV_IHVLoadGame() {
    void *symbol=nullptr;vm_t *vm=VM_CreateTable(VM_GAME,"GetGameAPI",SV_IHVGameEntry,&symbol);
    if(!vm)return nullptr;
    // GetGameAPI itself can call an error import. Publish VM ownership before
    // entering it so the normal server error cleanup can unload the library.
    gvm=vm;
    ihvGameImports={SV_IHVPrintf,SV_IHVError,SV_IHVMilliseconds,SV_IHVAllocate,SV_IHVFree,SV_IHVFreeTags,
        SV_IHVCvarGet,SV_IHVCvarSet,SV_IHVArgc,SV_IHVArgv,SV_IHVReadFile,SV_IHVFree,SV_IHVWriteFile,
        SV_IHVFileForWriting,SV_IHVAddCommandText,SV_IHVDebugGraph,SV_IHVDropClient,SV_IHVSendServerCommand,
        SV_IHVSetConfigstring,SV_IHVGetConfigstring,SV_IHVSetBrushModel,SV_IHVTrace,SV_IHVPointContents,
        SV_IHVInPVS,SV_IHVInPVSIgnorePortals,SV_IHVAdjustAreaPortal,SV_IHVAreasConnected,SV_IHVLink,SV_IHVUnlink,
        SV_IHVContactEntities,SV_IHVSnapshotCount,SV_IHVSnapshotNumber,SV_IHVBotAllocate,SV_IHVBotFree,SV_IHVBotUserinfo};
    auto getter=reinterpret_cast<fntech3_q3_ihv_GetGameAPI_t>(symbol);
    auto *game=getter(&ihvGameImports);
    if(!game || game->apiVersion!=FNTECH3_Q3_IHV_GAME_API || !game->Init || !game->Shutdown
        || !game->SpawnEntities || !game->RunFrame || !game->ConsoleCommand) {
        SV_IHVClearGame();VM_Free(vm);gvm=nullptr;
        Com_Error(ERR_DROP,"Invalid IHV native game API 4 table");return nullptr;
    }
    ihvGame=game;return vm;
}
}

/* The dedicated server's per-frame native snapshot pass, in the original's
 * position: after the simulation steps and before client messages. */
void SV_IHVServerFrame( void ) { SV_IHVBuildClientSnapshots(); }
void SV_IHVServerBaselines( void ) { SV_IHVCreateBaselines(); }

/* One client's protocol-33 snapshot message, returning its byte count or -1
 * when the slot cannot be written. A connected session will transmit this;
 * GAME_CLIENT_CONNECT still refuses one, so nothing in the engine calls it
 * yet. The adapter tests and the reference probe do. */
int SV_IHVServerSnapshotMessage( int slot, void *buffer, int capacity ) {
    static fntech3::q3ihv::wire::MessageWriter writer;
    if ( !ihvGameClientRecords.Valid( slot ) || !buffer || capacity < 0 ) return -1;
    writer.Clear();
    SV_IHVWriteSnapshotToClient( slot, writer );
    const int bytes = static_cast<int>( writer.Bytes() );
    if ( capacity < bytes ) return -1;
    memcpy( buffer, writer.Data(), static_cast<size_t>( bytes ) );
    return bytes;
}
#endif
