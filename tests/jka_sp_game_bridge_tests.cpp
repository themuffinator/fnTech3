/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Execute the real isolated developer bridge against synthetic API-8 tables.
 * No proprietary module or assets are required by this regression suite. */
#include "../code/server/server.h"
#include "../code/compat/jka_sp_game_api.h"
#include "../code/compat/jka_sp_cgame_abi.h"
#include "../code/compat/jka_sp_ghoul2_abi.h"
#include "../code/server/server_game_data.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include "jka_sp_model_fixture.hpp"
#include <type_traits>
#include <vector>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
extern "C" int Test_CatchMessageDrop(void (*callback)(void *),void *context);
extern "C" void Test_RaiseExpectedMessageDrop(void);
static char errorText[4096];
static bool armed;
void QDECL Com_Error(errorParm_t, const char *format, ...) {
	va_list args; va_start(args,format); std::vsnprintf(errorText,sizeof(errorText),format,args); va_end(args);
	if (!armed) { std::fprintf(stderr,"unexpected: %s\n",errorText); std::abort(); }
	Test_RaiseExpectedMessageDrop(); std::abort();
}
void QDECL Com_Printf(const char *, ...) {}
#ifndef Q_vsnprintf
int Q_vsnprintf(char *buffer, size_t size, const char *format, va_list args) { return std::vsnprintf(buffer,size,format,args); }
#endif
int Sys_Milliseconds() { return 5678; }
int Com_Milliseconds() { return 9876; }
static int commandArgc=2;
int Cmd_Argc() { return commandArgc; }
const char *Cmd_Argv(int index) { return index==1 ? "a" : index==2 ? "two words" : "test"; }
void Cbuf_AddText(const char *) {}
server_t sv{};
vm_t *gvm = nullptr;
qboolean com_errorEntered = qfalse;
fntech3::server::ServerGameDataView serverGameData;
alignas(4) static std::array<unsigned char, 1252 * 1024> arena;
static int links, unlinks, runFrames, zoneFrees;
void *Z_Malloc(int bytes) { return std::calloc(1,bytes ? bytes : 1); }
void Z_Free(void *buffer) { ++zoneFrees; std::free(buffer); }
int CM_TotalMapContents() { return 0x100|32; }
qboolean CM_AreasConnected(int first,int second) { return first==second ? qtrue : qfalse; }
void SV_LinkEntity(sharedEntity_t *entity) {
	++links; fntech3::server::GameEntityRef ref; CHECK(serverGameData.EntityRefForPointer(entity,ref));
	CHECK(serverGameData.SetLinked(ref,1));
}
void SV_UnlinkEntity(sharedEntity_t *entity) {
	++unlinks; fntech3::server::GameEntityRef ref; CHECK(serverGameData.EntityRefForPointer(entity,ref));
	CHECK(serverGameData.SetLinked(ref,0));
}
void SV_SetBrushModel(sharedEntity_t *,const char *name) { CHECK(!std::strcmp(name,"*1")); }
qboolean SV_inPVS(const float *,const float *) { return qtrue; }
static bool SV_inPVSIgnorePortals(const float *,const float *) { return true; }
static void SV_AdjustAreaPortalState(sharedEntity_t *,bool open) { CHECK(open); }
static bool SV_EntityContact(const float *,const float *,const sharedEntity_t *,bool capsule) { CHECK(!capsule);return true; }
static std::vector<int> areaEntities{1}, clippedEntities;
static int areaCountOverride=-1,worldCalls,areaCalls;
static float areaMins[3],areaMaxs[3],entitySweepEnd[3];
static trace_t worldTrace{};
static std::array<trace_t,1024> entityTraces;
static int tempBoxContents=CONTENTS_BODY,contentsChanges;
static bool clipDrop;
void CM_SetTempBoxModelContents(int contents) { tempBoxContents=contents;++contentsChanges; }
int SV_AreaEntities(const float *mins,const float *maxs,int *list,int maximum) {
	++areaCalls;CHECK(maximum>0);
	std::memcpy(areaMins,mins,sizeof(areaMins));std::memcpy(areaMaxs,maxs,sizeof(areaMaxs));
	const int count=(std::min)(maximum,int(areaEntities.size()));
	for(int i=0;i<count;++i)list[i]=areaEntities[std::size_t(i)];
	return areaCountOverride!=-1 ? areaCountOverride : count;
}
int SV_PointContents(const float *,int passEntity) { CHECK(passEntity==ENTITYNUM_NONE);return 32; }
static float pointMins[3],pointMaxs[3],pointOrigin[3],pointAngles[3];
static int pointModels,pointPrimitives;static bool pointDrop;
int CM_PointContents(const float *,clipHandle_t model){CHECK(!model);return 32;}
clipHandle_t CM_InlineModel(int model){++pointModels;CHECK(model==2);return 17;}
clipHandle_t CM_TempBoxModel(const float *mins,const float *maxs,qboolean capsule){
 ++pointModels;CHECK(!capsule);
 std::memcpy(pointMins,mins,sizeof(pointMins));std::memcpy(pointMaxs,maxs,sizeof(pointMaxs));return 511;
}
int CM_TransformedPointContents(const float *point,clipHandle_t model,const float *origin,const float *angles){
 ++pointPrimitives;std::memcpy(pointOrigin,origin,sizeof(pointOrigin));std::memcpy(pointAngles,angles,sizeof(pointAngles));
 if(pointDrop)Com_Error(ERR_DROP,"partial point primitive");
 if(model==17)return 8;
 CHECK(model==511);
 for(int n=0;n<3;++n)if(point[n]-origin[n]<=pointMins[n]||point[n]-origin[n]>=pointMaxs[n])return 0;
 return tempBoxContents;
}

void CM_BoxTrace(trace_t *trace,const float *,const float *,const float *mins,const float *maxs,clipHandle_t model,int,qboolean capsule) {
	++worldCalls;CHECK(mins&&maxs&&!model&&!capsule);*trace=worldTrace;
}
void SV_ClipToEntity(trace_t *trace,const float *,const float *,const float *,const float *end,int entityNum,int mask,qboolean capsule) {
	CHECK(entityNum>=0&&entityNum<1024&&!capsule);clippedEntities.push_back(entityNum);
	std::memcpy(entitySweepEnd,end,sizeof(entitySweepEnd));
	if(clipDrop)Com_Error(ERR_DROP,"partial entity collision");
	const auto *entity=reinterpret_cast<const fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252*entityNum);
	if(!entity->bmodel && !(tempBoxContents & mask)) { *trace={};trace->fraction=1;std::memcpy(trace->endpos,end,sizeof(trace->endpos));return; }
	*trace=entityTraces[std::size_t(entityNum)];if(trace->fraction<1)trace->entityNum=entity->s.number;
}
struct Variable {
	cvar_t engine{}; std::string name, value, reset;
	void Publish() {
		engine.name = name.data(); engine.string = value.data(); engine.resetString = reset.data();
		engine.integer = std::atoi(value.c_str()); engine.value = std::strtof(value.c_str(),nullptr);
	}
};
static std::map<std::string, Variable> vars;
static int cvarGetCalls;
static std::map<int,cvar_t *> vmVars;
static std::string configstring, userinfo, sentCommand, dropReason;
void SV_SetConfigstring(int index,const char *text) { CHECK(index==20);configstring=text; }
void SV_GetConfigstring(int index,char *buffer,int size) { CHECK(index==20);std::snprintf(buffer,size,"%s",configstring.c_str()); }
static void SV_GetServerinfo(char *buffer,int size) { std::snprintf(buffer,size,"server"); }
void SV_GetUserinfo(int client,char *buffer,int size) { CHECK(client==0);std::snprintf(buffer,size,"%s",userinfo.c_str()); }
void SV_SetUserinfo(int client,const char *text) { CHECK(client==0);userinfo=text; }
static void SV_GameDropClient(int client,const char *reason) { CHECK(client==0);dropReason=reason; }
static void SV_GameSendServerCommand(int client,const char *text) { CHECK(client==-1);sentCommand=text; }
static int fileCloses, bufferFrees, modelDropReads, cfgDropReads, lastFileMode;
static bool modelReadDrop, cfgReadDrop, cgameReadDrop;
static std::map<std::string,jka_sp_test::Bytes> modelFiles;
int FS_FOpenFileByMode(const char *path, fileHandle_t *file, fsMode_t mode) {
	lastFileMode=mode;
	if(file)*file=std::strcmp(path,"missing") ? 9 : 0;
	return std::strcmp(path,"missing") ? 4 : -1;
}
void FS_FCloseFile(fileHandle_t file) { CHECK(file==9); ++fileCloses; }
int FS_Read(void *buffer,int length,fileHandle_t file) {
	CHECK(file==9 && length>=0 && length<=4);
	if(cgameReadDrop) Com_Error(ERR_DROP,"partial cgame filesystem initialization");
	if(length)std::memcpy(buffer,"data",length); return length;
}
int FS_Write(const void *buffer,int length,fileHandle_t file) { CHECK((!length||buffer)&&length>=0&&length<=4&&file==9); return length; }
int FS_ReadFile(const char *name,void **buffer) {
	if(modelReadDrop && !std::strcmp(name,"model-drop.gla") && ++modelDropReads==2)
		Com_Error(ERR_DROP,"partial model filesystem initialization");
	if(cfgReadDrop && !std::strcmp(name,"cfg-drop.cfg") && ++cfgDropReads==2)
		Com_Error(ERR_DROP,"partial animation CFG filesystem initialization");
	if(!std::strcmp(name,"missing")) { if(buffer)*buffer=nullptr; return -1; }
	const auto model=modelFiles.find(name);
	if(model!=modelFiles.end()) {
		if(buffer) { *buffer=std::malloc(model->second.size());std::memcpy(*buffer,model->second.data(),model->second.size()); }
		return int(model->second.size());
	}
	if(buffer) { *buffer=std::malloc(5); std::memcpy(*buffer,"data",5); } return 4;
}
void FS_FreeFile(void *buffer) { ++bufferFrees; std::free(buffer); }
int FS_GetFileList(const char *,const char *,char *list,int size) { CHECK(size>=5);std::memcpy(list,"test",5);return 1; }
static std::string Key(const char *name) { std::string key=name; for (char &c:key) if(c>='A'&&c<='Z')c+=32; return key; }
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
	++cvarGetCalls;
	auto &v=vars[Key(name)]; if(v.name.empty()) { v.name=Key(name); v.value=v.reset=value; }
	// Match Cvar_Get's latch side effect, even when value is empty and flags zero.
	if(v.engine.latchedString) {
		v.value=v.engine.latchedString;v.engine.latchedString=nullptr;
		++v.engine.modificationCount;v.engine.modified=qtrue;
	}
	v.engine.flags|=flags; v.Publish(); return &v.engine;
}
const cvar_t *Cvar_FindExisting(const char *name) {
	const auto found=vars.find(Key(name));return found==vars.end() ? nullptr : &found->second.engine;
}
void Cvar_Set(const char *name, const char *value) {
	auto &v=vars[Key(name)]; v.value=value; v.engine.latchedString=nullptr;
	++v.engine.modificationCount; v.engine.modified=qtrue; v.Publish();
}
void Cvar_Update(vmCvar_t *external,int) {
	const auto found=vmVars.find(external->handle);
	if (found==vmVars.end()) Com_Error(ERR_DROP,"cvar handle out of range");
	const auto &internal=*found->second;
	if (external->modificationCount==internal.modificationCount) return;
	external->modificationCount=internal.modificationCount; external->value=internal.value;
	external->integer=internal.integer; external->flags=internal.flags;
	std::snprintf(external->string,sizeof(external->string),"%s",internal.string);
}
void Cvar_Register(vmCvar_t *external,const char *name,const char *value,int flags,int privateFlag) {
	cvar_t *internal=Cvar_Get(name,value,flags); if (!external) return;
	int handle=0;
	for (const auto &entry:vmVars) { if (entry.second==internal) { handle=entry.first; break; } ++handle; }
	vmVars[handle]=internal; external->handle=handle; external->modificationCount=-1;
	Cvar_Update(external,privateFlag);
}
void Q_strncpyz(char *buffer,const char *source,int bytes) { CHECK(bytes>0);std::snprintf(buffer,bytes,"%s",source); }
int Cvar_VariableIntegerValue(const char *name) { return Cvar_Get(name,"",0)->integer; }
void Cvar_VariableStringBuffer(const char *name, char *buffer, int size) {
	CHECK(size>0); std::snprintf(buffer,size,"%s",Cvar_Get(name,"",0)->string);
}
static vm_t syntheticVM{};
static int frees, shutdowns, initializations, failure, attached, getterCalls;
static fntech3_jka_sp_game_export_t exports{};
static fntech3_jka_sp_game_import_t *received;
static intptr_t (QDECL *cgameSyscall)(intptr_t,...);
static fntech3_jka_sp_ghoul2_array_t *detachArray;
static int detachDeletes;
static void Init(const char *mapname,const char *target,int32_t checksum,const char *entities,
	int32_t time,int32_t seed,int32_t global,int32_t saved,int32_t transition) {
	++initializations;
	CHECK(!std::strcmp(mapname,"academy1")); CHECK(!std::strcmp(target,"spawn")); CHECK(checksum==123);
	CHECK(!std::strcmp(entities,"{ worldspawn }")); CHECK(time==321&&seed==654&&global==9876);
	CHECK(saved==0&&transition==0); CHECK(attached);
	if(failure==5) reinterpret_cast<void (*)(...)>(received->slots[FNTECH3_JKA_SP_IMPORT_RE_RegisterSkin])();
	if(failure==6) cgameSyscall(1001);
	if(failure==10) {
		modelReadDrop=true;
		reinterpret_cast<int32_t (*)(const char *)>(received->slots[45])("model-drop.gla");
	}
	if(failure==11) {
		cfgReadDrop=true;
		reinterpret_cast<fntech3_jka_sp_RE_GetAnimationCFG_t>(received->slots[122])("cfg-drop.cfg",nullptr,0);
	}
	if(failure==14 || failure==15 || failure==16 || failure==17) {
		fntech3_jka_sp_ghoul2_handle_t handle{};
		if(failure==15)modelReadDrop=true;
		const auto init=reinterpret_cast<fntech3_jka_sp_G2API_InitGhoul2Model_t>(received->slots[46]);
		CHECK(init(&handle,failure==15?"models/drop.glm":"models/test/model.glm",5,6,7,8,9)==0);
		if(failure==16 || failure==17) {
			using Getter=fntech3_jka_sp_ghoul2_array_t *(*)();
			detachArray=reinterpret_cast<Getter>(received->slots[88])();
			/* Real gentity+372 handle members, read by retail destruction at
			 *0x200717e3. Aliases make late release idempotence observable. */
			for(int i:{1,2})reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252*i)->ghoul2_handle=handle.handle;
			reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252)->contents=1;
			entityTraces[1].fraction=0.5f;
			auto *vector=detachArray->vtable->Get(detachArray,handle.handle);
			auto *record=reinterpret_cast<fntech3_jka_sp_ghoul2_info_t *>(uintptr_t(vector->begin_address));
			record->animModelIndexOffset=3;record->customShader=42;record->flags=0xffff;
			if(failure==17) { record->surfaces.begin_address=4;record->bones.capacity_address=8;vector->begin_address=4; }
			fntech3_jka_sp_trace_t trace;const float zero[3]{},end[3]{0,0,6};
			using Trace=void (*)(fntech3_jka_sp_trace_t *,const float *,const float *,const float *,const float *,int32_t,int32_t,int32_t,int32_t);
			reinterpret_cast<Trace>(received->slots[30])(&trace,zero,nullptr,nullptr,end,-1,0x1381,1,10);
		}
		reinterpret_cast<void (*)(...)>(received->slots[51])();
	}
	if(failure==8) {
		int32_t file; reinterpret_cast<fntech3_jka_sp_FS_FOpenFile_t>(received->slots[FNTECH3_JKA_SP_IMPORT_FS_FOpenFile])("test",&file,0);
		void *buffer; reinterpret_cast<fntech3_jka_sp_FS_ReadFile_t>(received->slots[FNTECH3_JKA_SP_IMPORT_FS_ReadFile])("test",&buffer);
		Com_Error(ERR_DROP,"partial file initialization");
	}
}
static void Frame(int32_t time) { CHECK(time==777);++runFrames;exports.num_entities=5; }
static void Shutdown() { ++shutdowns; if(failure==7) Com_Error(ERR_DROP,"shutdown error"); }
static fntech3_jka_sp_game_export_t *Getter(fntech3_jka_sp_game_import_t *imports) {
	++getterCalls; received=imports; CHECK(gvm==&syntheticVM);
	if(failure==1) reinterpret_cast<void (*)(int,const char *,...)>(imports->slots[FNTECH3_JKA_SP_IMPORT_Error])(1,"getter error");
	if(failure==2)return nullptr;
	return &exports;
}
static void Entry(intptr_t (QDECL *callback)(intptr_t,...)) {
	CHECK(gvm==&syntheticVM); cgameSyscall=callback; ++attached;
	if(failure==9) cgameSyscall(1,"entry error");
	if(failure==12 || failure==13) {
		fntech3_jka_sp_vmCvar_t cvar{};
		const char *name=failure==12 ? "cg_preinit_good" : "cg_preinit_drop";
		CHECK(cgameSyscall(3,&cvar,name,"4",0)==0 && cvar.integer==4);
		CHECK(cgameSyscall(5,name,"6")==0 && cgameSyscall(4,&cvar)==0 && cvar.integer==6);
		char args[8]{}; CHECK(cgameSyscall(8,args,sizeof(args))==0 && !std::strcmp(args,"a "));
		int32_t file=0; CHECK(cgameSyscall(9,"test",&file,0)==4 && file==9);
		cgameReadDrop=failure==13;
		char bytes[4]{}; CHECK(cgameSyscall(10,static_cast<void *>(bytes),4,file)==0 && !std::memcmp(bytes,"data",4));
		CHECK(cgameSyscall(11,static_cast<const void *>(bytes),4,file)==0 && cgameSyscall(12,file)==0);
	}
}
vm_t *VM_CreateTable(vmIndex_t index,const char *symbol,vmMainFunc_t dispatcher,void **address) {
	CHECK(index==VM_GAME&&!std::strcmp(symbol,"GetGameAPI"));
	syntheticVM.entryPoint=dispatcher; syntheticVM.dllHandle=reinterpret_cast<void *>(uintptr_t(1));
	*address=reinterpret_cast<void *>(Getter); return &syntheticVM;
}
void VM_Free(vm_t *vm) {
	CHECK(vm==&syntheticVM);++frees;
	/* FreeLibrary invokes retail global gentity destructors even when Init
	 * failed and the host deliberately skipped the game Shutdown export. */
	if(detachArray) for(int i:{1,2}) {
		auto *entity=reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252*i);
		if(entity->ghoul2_handle) {
			detachArray->vtable->Delete(detachArray,entity->ghoul2_handle);
			entity->ghoul2_handle=0;++detachDeletes;
		}
	}
}
/* The cgame half's vmMain, which the snapshot builder asks for the camera. */
static intptr_t QDECL CgameMain(int,...) { return 0; }
void *Sys_LoadFunction(void *library,const char *name) {
	CHECK(library==syntheticVM.dllHandle);
	if(!std::strcmp(name,"vmMain")) return reinterpret_cast<void *>(CgameMain);
	CHECK(!std::strcmp(name,"dllEntry"));
	return failure==4 ? nullptr : reinterpret_cast<void *>(Entry);
}
#include "../code/server/server_jka_sp_game.hpp"

static void Reject(void (*action)(), const char *message) {
	/* The shared C helper owns setjmp; this callback/context and failing
	 * bridge frames contain no C++ objects requiring destruction. Owned
	 * handles/buffers are verified by the explicit error-cleanup checks. */
	struct Context { void (*action)(); } context{action};
	armed=true;
	const int rejected=Test_CatchMessageDrop([](void *raw) { static_cast<Context *>(raw)->action(); },&context);
	armed=false; CHECK(rejected && std::strstr(errorText,message));
}
static void Load() { SV_JKASPLoadGame(); }
static void Initialize() { SV_JKASPGameEntry(GAME_INIT,321,654); }
static void Stop() { SV_JKASPGameEntry(GAME_SHUTDOWN); }
static int failingSlot;
static void Unsupported() { reinterpret_cast<void (*)(...)>(received->slots[failingSlot])(); }
static void BadOpen() { SV_JKASPOpenFile("test",nullptr,4); }
static void BadRead() { SV_JKASPRead(nullptr,-1,9); }
static void ForeignRead() { char buffer[4]; SV_JKASPRead(buffer,4,10); }
static void ForeignClose() { SV_JKASPCloseFile(10); }
static void ForeignFree() { SV_JKASPFreeFile(reinterpret_cast<void *>(uintptr_t(4))); }
static void ForeignAllocation() { SV_JKASPFree(reinterpret_cast<void *>(uintptr_t(4))); }
static void ForeignEntity() { SV_JKASPLink(arena.data()+1); }
static void BadAllocation() { SV_JKASPMalloc(-1,8,1); }
static void UnsupportedPrecache() { SV_JKASPPrecacheModel("models/test.md3"); }
static void BuiltinPrecache() { SV_JKASPPrecacheModel("*unserved.gla"); }
static void BadAnimationCFG() { char buffer;SV_JKASPAnimationCFG("test.cfg",&buffer,0); }
static void BadCgameUpdate() { fntech3_jka_sp_vmCvar_t cvar{};cvar.handle=0x7fffffff;cgameSyscall(4,&cvar); }
static void NullCgameUpdate() { cgameSyscall(4,static_cast<fntech3_jka_sp_vmCvar_t *>(nullptr)); }
static void BadCgameArgv() { cgameSyscall(7,0,static_cast<char *>(nullptr),1); }
static void BadCgameArgs() { cgameSyscall(8,static_cast<char *>(nullptr),1); }
static void BadCgameArgsSize() { char buffer;cgameSyscall(8,&buffer,0); }
static void NullCgameFile() { cgameSyscall(9,"test",static_cast<int32_t *>(nullptr),0); }
static void BadCgameMode() { int32_t file=0;cgameSyscall(9,"test",&file,4); }
static void BadCgameRead() { cgameSyscall(10,static_cast<void *>(nullptr),1,9); }
static void NegativeCgameRead() { char buffer;cgameSyscall(10,static_cast<void *>(&buffer),-1,9); }
static void BadCgameWrite() { cgameSyscall(11,static_cast<const void *>(nullptr),1,9); }
static void ForeignCgameRead() { char buffer[4];cgameSyscall(10,static_cast<void *>(buffer),4,10); }
static void ForeignCgameClose() { cgameSyscall(12,10); }
static void StaleCgameClose() { cgameSyscall(12,9); }
static int traceSelector=1,tracePass=-1,traceMask=1;
static void GhoulTrace() { fntech3_jka_sp_trace_t trace;const float zero[3]{},end[3]{0,0,6};SV_JKASPTrace(&trace,zero,nullptr,nullptr,end,tracePass,traceMask,traceSelector,10); }
static const float candidateStart[3]{},candidateEnd[3]{40,0,0},candidateMins[3]{-2,-3,-4},candidateMaxs[3]{2,3,4};
static struct { uint32_t before;fntech3_jka_sp_trace_t trace;uint32_t after; } candidateResult{0x12345678,{},0x87654321};
static void CandidateTrace() { SV_JKASPTrace(&candidateResult.trace,candidateStart,candidateMins,candidateMaxs,candidateEnd,tracePass,traceMask,traceSelector,0); }
static void Reset() {
	SV_JKASPClearGame(); serverGameData.Invalidate(); com_errorEntered=qfalse;
	CHECK(jkaSpCgameMain==nullptr);
	vmVars.clear();
	gvm=nullptr; exports={}; exports.apiversion=8;
	exports.Init=Init; exports.Shutdown=Shutdown; exports.gentitySize=1252;
	exports.gentities=arena.data();exports.num_entities=4;exports.RunFrame=Frame;
	arena.fill(0); auto *entity=reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252);
	entity->s.number=1;entity->inuse=1;
	failure=0; attached=initializations=shutdowns=frees=getterCalls=0;
	detachArray=nullptr;detachDeletes=0;
	areaEntities={1};clippedEntities.clear();areaCountOverride=-1;worldCalls=areaCalls=0;
	tempBoxContents=CONTENTS_BODY;contentsChanges=0;clipDrop=false;
	worldTrace={};worldTrace.fraction=0.5f;worldTrace.endpos[2]=3;worldTrace.plane.normal[2]=1;
	worldTrace.plane.dist=3;worldTrace.contents=1;
	for(auto &trace:entityTraces) { trace={};trace.fraction=1; }
	traceSelector=1;tracePass=-1;traceMask=1;
	fileCloses=bufferFrees=links=unlinks=runFrames=zoneFrees=0;
	modelDropReads=0;modelReadDrop=false;
	cfgDropReads=0;cfgReadDrop=false;
	cgameReadDrop=false;lastFileMode=-1;
	commandArgc=2;
	modelFiles.clear();modelFiles["models/test/model.glm"]=jka_sp_test::Mesh();
	modelFiles["models/test/animation.gla"]=jka_sp_test::Skeleton();
	modelFiles["models/drop.glm"]=jka_sp_test::Mesh("model-drop");
	Cvar_Get("mapname","academy1",0); Cvar_Get("spawntarget","spawn",0); Cvar_Get("sv_mapChecksum","123",0);
	sv.entityParsePoint=const_cast<char *>("{ worldspawn }");
}
static fntech3_jka_sp_gentity_prefix_t &Entity(int index) {
	return *reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(arena.data()+1252*index);
}
static void TraceCandidateTests() {
	Reset();Load();
	for(int i=0;i<8;++i) { Entity(i).s.number=i;Entity(i).contents=1; }
	fntech3_jka_sp_ghoul2_handle_t model{};
	CHECK(SV_JKASPInitGhoul2Model(&model,"models/test/model.glm",0,0,0,0,0)==0);
	Entity(1).ghoul2_handle=model.handle;
	worldTrace={};worldTrace.fraction=0.25f;worldTrace.endpos[0]=10;worldTrace.contents=1;
	worldTrace.plane.normal[0]=-1;worldTrace.plane.dist=-10;
	traceSelector=2;traceMask=0x1381;
	areaEntities.clear();CandidateTrace();
	CHECK(candidateResult.trace.fraction==0.25f&&candidateResult.trace.entityNum==ENTITYNUM_WORLD);
	CHECK(candidateResult.before==0x12345678&&candidateResult.after==0x87654321);
	for(const auto &collision:candidateResult.trace.collision)CHECK(collision.distance==100000&&collision.entityNum==-1);
	/* A world block returns before even resolving pass/owner/Ghoul2 fields. */
	worldTrace.fraction=0;tracePass=3;Entity(3).owner_address=4;
	const int previousAreas=areaCalls;CandidateTrace();CHECK(areaCalls==previousAreas&&candidateResult.trace.fraction==0);
	worldTrace.fraction=0.25f;tracePass=-1;Entity(3).owner_address=0;
	areaEntities={1};clippedEntities.clear();
	Entity(1).contents=2;CandidateTrace();CHECK(clippedEntities.empty());Entity(1).contents=1;
	CandidateTrace();CHECK(candidateResult.trace.fraction==0.25f&&candidateResult.trace.entityNum==ENTITYNUM_WORLD);
	CHECK(entitySweepEnd[0]==10&&entitySweepEnd[1]==0&&entitySweepEnd[2]==0);
	CHECK(areaMins[0]==-3&&areaMins[1]==-4&&areaMins[2]==-5);
	CHECK(areaMaxs[0]==13&&areaMaxs[1]==4&&areaMaxs[2]==5);
	CHECK(contentsChanges>0 && tempBoxContents==CONTENTS_BODY && !jkaSpTempBoxContentsActive);
	entityTraces[1].fraction=0.5f;entityTraces[1].endpos[0]=5;
	traceSelector=0;CandidateTrace();CHECK(candidateResult.trace.fraction==0.125f&&candidateResult.trace.entityNum==1&&candidateResult.trace.endpos[0]==5);
	for(int selector:{1,2}) { traceSelector=selector;Reject(CandidateTrace,"Ghoul2 trace geometry is not served yet");CHECK(std::strstr(errorText,"candidate=1")); }
	/* A nearer ordinary entity excludes a later nonwinning model box. A
	 * later startsolid model still needs geometry even without a closer hit.
	 * The cast is load-bearing: GCC's i686 x87 C++ keeps a bare 0.025f at long
	 * double precision (FLT_EVAL_METHOD 2), not the float 0.1f*0.25f stored. */
	areaEntities={2,1};entityTraces[2].fraction=0.1f;entityTraces[2].endpos[0]=1;
	CandidateTrace();CHECK(candidateResult.trace.entityNum==2&&candidateResult.trace.fraction==float(0.025f));
	entityTraces[1].startsolid=qtrue;Reject(CandidateTrace,"Ghoul2 trace geometry");entityTraces[1].startsolid=qfalse;
	areaEntities={1};
	Entity(1).contents=1|0x40000;CandidateTrace();CHECK(candidateResult.trace.entityNum==1&&candidateResult.trace.fraction==0.125f);Entity(1).contents=1;
	/* Null and acquired-empty handles truthfully have no model geometry. */
	Entity(1).ghoul2_handle=0;CandidateTrace();CHECK(candidateResult.trace.entityNum==1);
	auto *array=SV_JKASPTheGhoul2InfoArray();
	const int32_t empty=array->vtable->New(array);
	Entity(1).ghoul2_handle=empty;CandidateTrace();CHECK(candidateResult.trace.entityNum==1);
	Entity(1).ghoul2_handle=INT32_MAX;Reject(CandidateTrace,"invalid or stale handle");Entity(1).ghoul2_handle=model.handle;
	/* Native SP owner rules compare the owner's entity number and both raw
	 * owner identities. Excluded entities must not inspect a foreign G2 ID. */
	Entity(1).ghoul2_handle=INT32_MAX;tracePass=1;clippedEntities.clear();CandidateTrace();CHECK(clippedEntities.empty());
	tracePass=3;Entity(1).owner_address=uint32_t(reinterpret_cast<uintptr_t>(&Entity(4)));Entity(4).s.number=3;
	clippedEntities.clear();CandidateTrace();CHECK(clippedEntities.empty());Entity(4).s.number=4;
	Entity(1).owner_address=0;Entity(3).owner_address=uint32_t(reinterpret_cast<uintptr_t>(&Entity(1)));
	clippedEntities.clear();CandidateTrace();CHECK(clippedEntities.empty());
	Entity(3).owner_address=uint32_t(reinterpret_cast<uintptr_t>(&Entity(4)));Entity(1).owner_address=Entity(3).owner_address;
	clippedEntities.clear();CandidateTrace();CHECK(clippedEntities.empty());
	Entity(1).owner_address=4;Reject(CandidateTrace,"owner is outside the entity allocation");
	Entity(1).owner_address=0;Entity(3).owner_address=4;Reject(CandidateTrace,"owner is outside the entity allocation");
	Entity(3).owner_address=0;tracePass=-1;Entity(1).ghoul2_handle=model.handle;
	areaCountOverride=1025;Reject(CandidateTrace,"broadphase count is out of bounds");areaCountOverride=-1;
	areaEntities={1024};Reject(CandidateTrace,"invalid entity slot");areaEntities={1};
	Entity(1).currentOrigin[0]=std::numeric_limits<float>::infinity();Reject(CandidateTrace,"entity geometry");Entity(1).currentOrigin[0]=0;
	worldTrace.fraction=std::numeric_limits<float>::quiet_NaN();Reject(CandidateTrace,"collision result");worldTrace.fraction=0.25f;
	/* Merge ordinary startsolid/allsolid without pretending model success. */
	Entity(1).ghoul2_handle=0;areaEntities={2,1};entityTraces[1].startsolid=qtrue;
	CandidateTrace();CHECK(candidateResult.trace.startsolid&&candidateResult.trace.entityNum==1&&candidateResult.trace.fraction==float(0.025f));
	entityTraces[1].allsolid=qtrue;CandidateTrace();CHECK(candidateResult.trace.allsolid&&candidateResult.trace.entityNum==1);
	areaEntities={1};clipDrop=true;Reject(CandidateTrace,"partial entity collision");
	CHECK(jkaSpTempBoxContentsActive&&tempBoxContents==0x100);
	Stop();
	CHECK(!jkaSpTempBoxContentsActive&&tempBoxContents==CONTENTS_BODY);
}

static const float contractPoint[]{11,12,13};
static void ContractPoint(){SV_JKASPPointContents(contractPoint,-1);}
static void PointAndLinkTests(){
 Reset();Load();auto &entity=Entity(1);entity.inuse=1;entity.contents=0x100;
 entity.mins[0]=-20;entity.mins[1]=-20;entity.mins[2]=-13;
 entity.maxs[0]=20;entity.maxs[1]=20;entity.maxs[2]=34;
 SV_JKASPLink(&entity);CHECK(entity.s.solid==0x420d14);
 entity.contents=1;SV_JKASPLink(&entity);CHECK(entity.s.solid==0x420d14);
 entity.contents=CONTENTS_BODY;SV_JKASPLink(&entity);CHECK(entity.s.solid==0);
 entity.contents=0x100;entity.maxs[0]=.5f;entity.mins[2]=12;entity.maxs[2]=1000;
 SV_JKASPLink(&entity);CHECK(entity.s.solid==0xff0101);
 entity.bmodel=1;entity.s.solid=0xffffff;SV_JKASPLink(&entity);CHECK(entity.s.solid==0xffffff);
 entity.bmodel=0;entity.inuse=0;const int oldLinks=links;entity.s.solid=17;
 SV_JKASPLink(&entity);CHECK(links==oldLinks&&entity.s.solid==17);entity.inuse=1;
 for(int n=0;n<3;++n){entity.mins[n]=-3-n;entity.maxs[n]=13+n;
  entity.s.origin[n]=contractPoint[n];entity.s.angles[n]=31.0f+n;
  entity.currentOrigin[n]=901.0f+n;entity.currentAngles[n]=131.0f+n;}
 areaEntities={1};pointModels=pointPrimitives=0;pointDrop=false;
 CHECK(SV_JKASPPointContents(contractPoint,-1)==0x120);
 CHECK(pointModels==1&&pointPrimitives==1&&tempBoxContents==CONTENTS_BODY&&!jkaSpTempBoxContentsActive);
 for(int n=0;n<3;++n){CHECK(pointOrigin[n]==entity.s.origin[n]);CHECK(pointAngles[n]==entity.s.angles[n]);}
 CHECK(SV_JKASPPointContents(contractPoint,1)==32&&pointPrimitives==1);
 const float outside[]{100,100,100};CHECK(SV_JKASPPointContents(outside,-1)==32);
 entity.bmodel=1;entity.s.modelindex=2;const int oldChanges=contentsChanges;
 CHECK(SV_JKASPPointContents(contractPoint,-1)==40&&contentsChanges==oldChanges);entity.bmodel=0;
 areaCountOverride=1025;Reject(ContractPoint,"point broadphase count");areaCountOverride=-1;
 areaCountOverride=-2;Reject(ContractPoint,"point broadphase count");areaCountOverride=-1;
 areaEntities={1024};Reject(ContractPoint,"invalid entity slot");
 areaEntities={-1};Reject(ContractPoint,"invalid entity slot");areaEntities={1};
 entity.s.origin[0]=std::numeric_limits<float>::infinity();Reject(ContractPoint,"point entity geometry");
 entity.s.origin[0]=contractPoint[0];entity.s.angles[0]=std::numeric_limits<float>::quiet_NaN();
 Reject(ContractPoint,"point entity geometry");entity.s.angles[0]=31;
 entity.mins[0]=20;Reject(ContractPoint,"point entity geometry");entity.mins[0]=-3;
 CHECK(tempBoxContents==CONTENTS_BODY&&!jkaSpTempBoxContentsActive);
 pointDrop=true;Reject(ContractPoint,"partial point primitive");
 CHECK(jkaSpTempBoxContentsActive&&tempBoxContents==0x100);
 Stop();CHECK(!jkaSpTempBoxContentsActive&&tempBoxContents==CONTENTS_BODY);pointDrop=false;
}

int main() {
	Reset(); CHECK(SV_JKASPLoadGame()==&syntheticVM); CHECK(getterCalls==1&&attached==1&&frees==0);
	CHECK(jkaSpCgameMain==CgameMain);
	using Cvar = fntech3_jka_sp_cvar_prefix_t *(*)(const char *,const char *,int);
	auto get=reinterpret_cast<Cvar>(received->slots[FNTECH3_JKA_SP_IMPORT_cvar]);
	auto set=reinterpret_cast<void (*)(const char *,const char *)>(received->slots[FNTECH3_JKA_SP_IMPORT_cvar_set]);
	const int previousGets=cvarGetCalls;
	auto *var=get("Value","3.5",256|512); CHECK(var->integer==3&&var->value==3.5f);
	CHECK(cvarGetCalls==previousGets+1);
	CHECK(get("VALUE","0",0)==var); CHECK(var->flags==(256|512));
	CHECK((vars["value"].engine.flags&256)==0); CHECK((vars["value"].engine.flags&512)!=0);
	// Existing engine flags are visible through a zero-flag registration;
	// TEMP (engine bit256) must not manufacture SP SAVEGAME (also bit256).
	auto *existingEngine=Cvar_Get("existing","1",CVAR_ROM|CVAR_LATCH|CVAR_TEMP);
	auto *existingExternal=get("EXISTING","1",0);
	CHECK(existingExternal->flags==(CVAR_ROM|CVAR_LATCH));
	CHECK(get("existing","1",256)==existingExternal);
	CHECK(existingExternal->flags==(CVAR_ROM|CVAR_LATCH|256));
	existingEngine->flags=(existingEngine->flags & ~CVAR_ROM)|CVAR_ARCHIVE;
	const int beforeFlagSync=cvarGetCalls;
	SV_JKASPSyncCvars();
	CHECK(cvarGetCalls==beforeFlagSync);
	CHECK(existingExternal->flags==(CVAR_ARCHIVE|CVAR_LATCH|256));
	set("value","7"); CHECK(var->integer==7&&var->value==7&&var->modificationCount==1);
	vars["value"].engine.latchedString=const_cast<char *>("9");
	var->modified=qfalse;
	const int beforeSync=cvarGetCalls;
	SV_JKASPSyncCvars();CHECK(cvarGetCalls==beforeSync);
	CHECK(var->integer==7&&!var->modified&&var->latchedString&&!std::strcmp(var->latchedString,"9"));
	get("unrelated","1",0);set("unrelated","2");
	CHECK(var->integer==7&&!var->modified&&vars["value"].engine.latchedString);
	set("value","8");CHECK(var->integer==8&&var->modified&&!var->latchedString);
	get("removed","1",0);vars.erase("removed");
	Reject(SV_JKASPSyncCvars,"no longer exists: removed");jkaSpCvars.erase("removed");
	struct VmCvarCanaries { uint32_t before; fntech3_jka_sp_vmCvar_t cvar; uint32_t after; } cgvar{0x12345678,{},0x98765432};
	cgameSyscall(3,&cgvar.cvar,"cg_test","3.5",256|512);
	CHECK(cgvar.cvar.value==3.5f && cgvar.cvar.integer==3 && !std::strcmp(cgvar.cvar.string,"3.5"));
	CHECK(cgvar.before==0x12345678 && cgvar.after==0x98765432 && !(vars["cg_test"].engine.flags&256));
	cgameSyscall(5,"cg_test","9");cgameSyscall(4,&cgvar.cvar);
	CHECK(cgvar.cvar.value==9 && cgvar.cvar.integer==9 && !std::strcmp(cgvar.cvar.string,"9"));
	CHECK(cgvar.before==0x12345678 && cgvar.after==0x98765432);
	cgameSyscall(3,static_cast<fntech3_jka_sp_vmCvar_t *>(nullptr),"cg_null","4",0);
	CHECK(vars["cg_null"].engine.integer==4 && cgameSyscall(2)==5678 && cgameSyscall(6)==2);
	char cgargv[3]{'x','x','z'};cgameSyscall(7,0,cgargv,2);CHECK(cgargv[0]=='t' && cgargv[1]==0 && cgargv[2]=='z');
	Reject(BadCgameUpdate,"handle out of range");Reject(NullCgameUpdate,"null cvar update");Reject(BadCgameArgv,"invalid argv storage");
	const auto *voice=reinterpret_cast<const int32_t *>(received->slots[FNTECH3_JKA_SP_IMPORT_VoiceVolume]);
	for(int i=0;i<1024;++i) CHECK(voice[i]==0);
	int32_t file=123;
	CHECK(SV_JKASPOpenFile("test",&file,0)==4&&file==9);
	char read[5]{}; CHECK(SV_JKASPRead(read,4,file)==4&&!std::strcmp(read,"data"));
	CHECK(SV_JKASPWrite(read,4,file)==4);
	Reject(BadOpen,"file open argument"); Reject(BadRead,"buffer or handle");
	Reject(ForeignRead,"buffer or handle"); Reject(ForeignClose,"unowned file");
	Reject(ForeignFree,"unowned file buffer");
	SV_JKASPCloseFile(file); CHECK(fileCloses==1);
	CHECK(SV_JKASPOpenFile("missing",&file,0)==-1&&file==0&&jkaSpFiles.empty());
	void *buffer=nullptr; CHECK(SV_JKASPReadFile("test",&buffer)==4&&buffer);
	SV_JKASPFreeFile(buffer); CHECK(bufferFrees==1&&jkaSpFileBuffers.empty());
	buffer=reinterpret_cast<void *>(uintptr_t(4)); CHECK(SV_JKASPReadFile("missing",&buffer)==-1&&!buffer);
	char list[5]; CHECK(SV_JKASPFileList("","",list,5)==1&&!std::strcmp(list,"test"));
	char cgargs[9];std::memset(cgargs,'z',sizeof(cgargs));
	CHECK(cgameSyscall(8,cgargs,5)==0 && !std::strcmp(cgargs,"a ") && cgargs[3]==0 && cgargs[4]==0 && cgargs[5]=='z');
	for(int count:{0,1}) { commandArgc=count;CHECK(cgameSyscall(8,cgargs,5)==0 && cgargs[0]==0 && cgargs[5]=='z'); }
	commandArgc=3;char joined[16];CHECK(cgameSyscall(8,joined,sizeof(joined))==0 && !std::strcmp(joined,"a two words "));
	std::memset(cgargs,'z',sizeof(cgargs));CHECK(cgameSyscall(8,cgargs,5)==0 && !std::strcmp(cgargs,"a tw") && cgargs[5]=='z');
	CHECK(cgameSyscall(8,cgargs,1)==0 && cgargs[0]==0 && cgargs[1]==' ');
	commandArgc=2;
	Reject(BadCgameArgs,"invalid args storage");Reject(BadCgameArgsSize,"invalid args storage");
	Reject(NullCgameFile,"null file handle storage");Reject(BadCgameMode,"file open argument");
	for(int mode=0;mode<4;++mode) {
		file=123;CHECK(cgameSyscall(9,"test",&file,mode)==4 && file==9 && lastFileMode==mode);
		char bytes[5]{'x','x','x','x','z'};
		CHECK(cgameSyscall(10,static_cast<void *>(bytes),4,file)==0 && !std::memcmp(bytes,"data",4) && bytes[4]=='z');
		CHECK(cgameSyscall(11,static_cast<const void *>(bytes),4,file)==0);
		CHECK(cgameSyscall(10,static_cast<void *>(nullptr),0,file)==0 && cgameSyscall(11,static_cast<const void *>(nullptr),0,file)==0);
		Reject(BadCgameRead,"buffer or handle");Reject(NegativeCgameRead,"buffer or handle");
		Reject(BadCgameWrite,"buffer or handle");Reject(ForeignCgameRead,"buffer or handle");
		CHECK(cgameSyscall(12,file)==0 && jkaSpFiles.empty());
		Reject(StaleCgameClose,"unowned file");
	}
	CHECK(fileCloses==5);Reject(ForeignCgameClose,"unowned file");
	file=123;CHECK(cgameSyscall(9,"missing",&file,0)==-1 && file==0 && jkaSpFiles.empty());
	SV_JKASPSetConfigstring(20,"config"); char text[20]; SV_JKASPGetConfigstring(20,text,20); CHECK(!std::strcmp(text,"config"));
	SV_JKASPGetServerinfo(text,20); CHECK(!std::strcmp(text,"server"));
	SV_JKASPSetUserinfo(0,"info"); SV_JKASPGetUserinfo(0,text,20); CHECK(!std::strcmp(text,"info"));
	SV_JKASPSendServerCommand(-1,"command %i",7); CHECK(sentCommand=="command 7");
	SV_JKASPDropClient(0,"reason"); CHECK(dropReason=="reason");
	void *allocation=SV_JKASPMalloc(16,8,1);
	for(int i=0;i<16;++i)CHECK(static_cast<unsigned char *>(allocation)[i]==0);
	CHECK(SV_JKASPIsFromZone(allocation,8)==16&&SV_JKASPIsFromZone(allocation,9)==0);
	CHECK(SV_JKASPFree(allocation)==16&&zoneFrees==1); Reject(ForeignAllocation,"unowned allocation");
	Reject(BadAllocation,"allocation");
	const float zero[3]{};void *entity=arena.data()+1252;
	SV_JKASPLink(entity);CHECK(links==1);SV_JKASPUnlink(entity);CHECK(unlinks==1);
	SV_JKASPSetBrushModel(entity,"*1");SV_JKASPAdjustPortal(entity,1);
	CHECK(SV_JKASPContact(zero,zero,entity)==1);
	CHECK(SV_JKASPInPVS(zero,zero)==1&&SV_JKASPInPVSIgnorePortals(zero,zero)==1);
	CHECK(SV_JKASPAreasConnected(1,1)==1&&SV_JKASPAreasConnected(1,2)==0);
	CHECK(SV_JKASPPointContents(zero,-1)==32);
	void *entities[1]{};CHECK(SV_JKASPEntitiesInBox(zero,zero,entities,1)==1&&entities[0]==entity);
	const float traceEnd[3]{0,0,6};
	fntech3_jka_sp_trace_t trace{};SV_JKASPTrace(&trace,zero,nullptr,nullptr,traceEnd,-1,1,0,10);
	CHECK(trace.fraction==0.5f&&trace.endpos[2]==3&&trace.entityNum==ENTITYNUM_WORLD);
	for(const auto &collision:trace.collision)CHECK(collision.distance==100000.0f&&collision.entityNum==-1);
	Reject(ForeignEntity,"allocation slot");
	/* A Ghoul2 selector alone does not imply that any model was queried. */
	SV_JKASPTrace(&trace,zero,nullptr,nullptr,traceEnd,-1,1,2,0);
	CHECK(trace.fraction==0.5f && trace.entityNum==ENTITYNUM_WORLD);
	traceSelector=3;Reject(GhoulTrace,"invalid Ghoul2 selector");traceSelector=1;
	using Ghoul2Getter = fntech3_jka_sp_ghoul2_array_t *(*)();
	auto *ghoul2 = reinterpret_cast<Ghoul2Getter>(received->slots[88])();
	CHECK(ghoul2 && !ghoul2->vtable->IsValid(ghoul2,0));
	fntech3_jka_sp_ghoul2_handle_t ghoul2Handle{ghoul2->vtable->New(ghoul2)};
	CHECK(ghoul2->vtable->IsValid(ghoul2,ghoul2Handle.handle));
	CHECK(ghoul2->vtable->GetConst(ghoul2,ghoul2Handle.handle)->begin_address==0);
	reinterpret_cast<void (*)(fntech3_jka_sp_ghoul2_handle_t *)>(received->slots[87])(&ghoul2Handle);
	CHECK(ghoul2Handle.handle==0);
	static_assert(std::is_same_v<decltype(&SV_JKASPInitGhoul2Model),fntech3_jka_sp_G2API_InitGhoul2Model_t>);
	const auto initModel=reinterpret_cast<fntech3_jka_sp_G2API_InitGhoul2Model_t>(received->slots[46]);
	struct HandleCanaries { uint32_t before;fntech3_jka_sp_ghoul2_handle_t handle;uint32_t after; } initialized{0x12345678,{},0x87654321};
	if constexpr(sizeof(uintptr_t)==sizeof(uint32_t)) {
		CHECK(initModel(&initialized.handle,"models/test/model.glm",12,23,34,45,56)==0);
		CHECK(initialized.before==0x12345678 && initialized.after==0x87654321);
		const auto *vector=ghoul2->vtable->GetConst(ghoul2,initialized.handle.handle);
		const auto *record=reinterpret_cast<const fntech3_jka_sp_ghoul2_info_t *>(uintptr_t(vector->begin_address));
		CHECK(record->customSkin==23 && record->customShader==34 && record->lodBias==56 && record->flags==0 && !record->valid);
		reinterpret_cast<void (*)(fntech3_jka_sp_ghoul2_handle_t *)>(received->slots[87])(&initialized.handle);
		CHECK(!initialized.handle.handle);
		/* Return this isolated import46 fixture to an empty registry before
		 * exercising the existing failed-precache handle ordering below. */
		jkaSpModels.Clear();
	}
	auto precache=reinterpret_cast<int32_t (*)(const char *)>(received->slots[45]);
	CHECK(precache(nullptr)==0 && precache("")==0 && precache("missing")==0);
	const int beforePrecache=bufferFrees;
	CHECK(precache("models/invalid.gla")==0 && jkaSpFileBuffers.empty());
	CHECK(bufferFrees==beforePrecache+2);
	CHECK(precache("MODELS/INVALID.GLA")==0 && bufferFrees==beforePrecache+2);
	const int defaultHandle=precache("*default.gla");CHECK(defaultHandle>0 && bufferFrees==beforePrecache+2);
	CHECK(!std::strcmp(SV_JKASPAnimInternalName(defaultHandle),"*default"));
	Reject(UnsupportedPrecache,"MD3/BSP model precache");Reject(BuiltinPrecache,"built-in Ghoul2 skeleton precache");
	for(int slot:{103,104}) {
		auto name=reinterpret_cast<char *(*)(int32_t)>(received->slots[slot]);
		CHECK(!std::strcmp(name(0),"") && !std::strcmp(name(1),"") && !std::strcmp(name(1024),""));
	}
	auto animationCFG=reinterpret_cast<fntech3_jka_sp_RE_GetAnimationCFG_t>(received->slots[122]);
	const int beforeCFG=bufferFrees;
	CHECK(animationCFG("test.cfg",nullptr,0)==4 && bufferFrees==beforeCFG+2 && jkaSpFileBuffers.empty());
	char cfg[8]{};CHECK(animationCFG("TEST.CFG",cfg,sizeof(cfg))==4 && !std::memcmp(cfg,"data\0\0\0\0",sizeof(cfg)));
	CHECK(bufferFrees==beforeCFG+2 && jkaSpAnimationCFGs.Count()==1);
	CHECK(animationCFG("missing",cfg,sizeof(cfg))==0 && jkaSpAnimationCFGs.Count()==1);
	Reject(BadAnimationCFG,"RE_GetAnimationCFG (122) refused");
	int unavailable=0;
	for(int slot=0;slot<131;++slot) {
		if(slot==0||slot==3||(slot>=4&&slot<=17)||(slot>=21&&slot<=46)||slot==87||slot==88||slot==103||slot==104||slot==122)continue;
		failingSlot=slot; Reject(Unsupported,fntech3::jka::sp::ImportName(slot)); ++unavailable;
	}
	CHECK(unavailable==84); Stop(); CHECK(shutdowns==0 && jkaSpAnimationCFGs.Count()==0);
	for(int mode : {1,2,3,4}) {
		Reset(); failure=mode; if(mode==3)exports.apiversion=10;
		Reject(Load,mode==1?"getter error":mode==2?"no export table":mode==3?"API 8":"no dllEntry");
		if(mode==1) { CHECK(gvm==&syntheticVM); Stop(); CHECK(shutdowns==0); VM_Free(gvm); gvm=nullptr; }
		else CHECK(gvm==nullptr&&frees==1);
		CHECK(initializations==0&&shutdowns==0);
	}
	Reset(); failure=9; Reject(Load,"entry error"); CHECK(gvm==&syntheticVM && attached==1 && initializations==0);
	Stop(); CHECK(shutdowns==0); VM_Free(gvm);gvm=nullptr;
	Reset();failure=12;Load();CHECK(attached==1 && initializations==0 && fileCloses==1 && jkaSpFiles.empty());
	Initialize();Stop();CHECK(initializations==1 && shutdowns==1 && jkaSpCvars.empty());
	Reset();failure=13;Reject(Load,"partial cgame filesystem initialization");
	CHECK(gvm==&syntheticVM && attached==1 && initializations==0 && fileCloses==0 && jkaSpFiles.size()==1);
	Stop();CHECK(shutdowns==0 && fileCloses==1 && jkaSpFiles.empty() && jkaSpCvars.empty());
	Stop();CHECK(fileCloses==1);VM_Free(gvm);gvm=nullptr;
	for(int mode : {5,6,8,10,11}) {
		Reset(); failure=mode; Load(); Reject(Initialize,mode==5?"RE_RegisterSkin":mode==6?"cgame import 1001":mode==10?"partial model filesystem":mode==11?"partial animation CFG filesystem":"partial file initialization");
		if(mode==10) CHECK(modelDropReads==2 && jkaSpModels.Count()==1 && bufferFrees==1);
		if(mode==11) CHECK(cfgDropReads==2 && jkaSpAnimationCFGs.Count()==1 && jkaSpAnimationCFGs.RetainedBytes()==5 && bufferFrees==1);
		CHECK(initializations==1); Stop(); CHECK(shutdowns==0&&jkaSpCvars.empty());
		if(mode==8)CHECK(fileCloses==1&&bufferFrees==1&&jkaSpFiles.empty()&&jkaSpFileBuffers.empty());
		if(mode==10) CHECK(jkaSpModels.Count()==0 && jkaSpFileBuffers.empty());
		if(mode==11) CHECK(jkaSpAnimationCFGs.Count()==0 && jkaSpAnimationCFGs.RetainedBytes()==0 && jkaSpFileBuffers.empty());
	}
	if constexpr(sizeof(uintptr_t)==sizeof(uint32_t)) for(int mode : {14,15}) {
		Reset();failure=mode;Load();Reject(Initialize,mode==14?"G2API_SetBoneAnglesMatrix":"partial model filesystem");
		CHECK(initializations==1 && jkaSpModels.Count()>0 && shutdowns==0);
		Stop();CHECK(shutdowns==0 && jkaSpModels.Count()==0 && jkaSpFileBuffers.empty());
		CHECK(!SV_JKASPTheGhoul2InfoArray()->vtable->IsValid(SV_JKASPTheGhoul2InfoArray(),512));
		Stop();CHECK(shutdowns==0);
	}
	if constexpr(sizeof(uintptr_t)==sizeof(uint32_t)) for(int mode : {16,17}) {
		Reset();failure=mode;Load();Reject(Initialize,mode==16?"Ghoul2 trace geometry is not served yet":"model vector header");
		if(mode==16)CHECK(std::strstr(errorText,"selector=1, useLod=10, passEntity=1023, contentMask=0x00001381, candidate=1"));
		CHECK(initializations==1 && !jkaSpInitialized && jkaSpModels.Count()>0);
		com_errorEntered=qtrue;Stop();CHECK(shutdowns==0 && jkaSpModels.Count()==0 && detachDeletes==0);
		/* Match production shutdown order: API8 host cleanup, then VM_Free's
		 * DLL detach callbacks. No recursive Com_Error may occur. */
		VM_Free(gvm);gvm=nullptr;CHECK(detachDeletes==2 && shutdowns==0);
		Stop();CHECK(detachDeletes==2 && shutdowns==0);
		Reset();Load();Initialize();Stop();CHECK(shutdowns==1);
	}
	Reset(); Load(); Initialize(); CHECK(jkaSpInitialized&&serverGameData.IsBound()&&sv.num_entities==4);
	SV_JKASPGameEntry(GAME_RUN_FRAME,777);CHECK(runFrames==1&&sv.num_entities==5);
	CHECK(SV_JKASPGameEntry(GAME_CLIENT_CONNECT)!=0);
	Stop(); CHECK(shutdowns==1&&!jkaSpGame); Stop(); CHECK(shutdowns==1);
	Reset(); Load(); Initialize(); failure=7;
	Reject(Stop,"shutdown error"); CHECK(!jkaSpInitialized); Stop(); CHECK(shutdowns==1&&!jkaSpGame);
	Reset(); Load(); Initialize();com_errorEntered=qtrue;Stop();CHECK(shutdowns==0&&!jkaSpGame);
	if constexpr(sizeof(uintptr_t)==sizeof(uint32_t)){TraceCandidateTests();PointAndLinkTests();}
	return 0;
}
