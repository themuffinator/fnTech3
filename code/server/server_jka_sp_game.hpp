/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_SP_GAME_HPP
#define FNTECH3_SERVER_JKA_SP_GAME_HPP
#include "../compat/jka_sp_game_api.h"
#include "../compat/jka_sp_state_adapter.hpp"
#include "../compat/jka_sp_ghoul2.hpp"
#include "../compat/jka_sp_cgame_abi.h"
#include "../compat/jka_sp_model_precache.hpp"
#include "../compat/jka_sp_animation_cfg.hpp"
#include "server_game_data.h"
#include "server_world_bounds.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <map>
#include <set>
#include <string>

/* Isolated API-8 developer host. Every unavailable import has its own
 * __cdecl error thunk. Ghoul2 poses, saved games and SP cgame stay explicit
 * blockers; no table slot falls through to an unrelated Q3 service. */
namespace {
fntech3_jka_sp_game_import_t jkaSpImports{};
fntech3_jka_sp_game_export_t *jkaSpGame = nullptr;
bool jkaSpInitialized = false;
bool jkaSpTempBoxContentsActive = false;
std::map<std::string, fntech3_jka_sp_cvar_prefix_t> jkaSpCvars;
std::set<int32_t> jkaSpFiles;
std::set<void *> jkaSpFileBuffers;
struct JKASPAllocation { int32_t bytes; uint8_t tag; };
std::map<void *, JKASPAllocation> jkaSpAllocations;
void SV_JKASPGhoul2Error(const char *text) { Com_Error(ERR_DROP, "%s", text); }
fntech3::jka::sp::Ghoul2Array jkaSpGhoul2(SV_JKASPGhoul2Error);
class JKASPModelFiles final : public fntech3::ghoul2::Files {
public:
	std::size_t Read(const char *path,void *destination,std::size_t capacity) override;
};
JKASPModelFiles jkaSpModelFiles;
fntech3::jka::sp::ModelPrecache jkaSpModels(jkaSpModelFiles);
fntech3::jka::sp::AnimationCFGCache jkaSpAnimationCFGs(jkaSpModelFiles);
fntech3_jka_sp_ghoul2_array_t *SV_JKASPTheGhoul2InfoArray() { return jkaSpGhoul2.Interface(); }
void SV_JKASPCleanGhoul2(fntech3_jka_sp_ghoul2_handle_t *handle) { jkaSpGhoul2.Clean(handle); }

void SV_JKASPClearGame() {
	jkaSpGame = nullptr;
	jkaSpInitialized = false;
	/* A collision error can longjmp out of the temporary SP BODY lease. */
	if (jkaSpTempBoxContentsActive) {
		jkaSpTempBoxContentsActive=false;CM_SetTempBoxModelContents(CONTENTS_BODY);
	}
	/* Relinquish ownership before invoking host cleanup: a cleanup error
	 * must not attempt to close/free the same resource again. */
	while (!jkaSpFiles.empty()) {
		const int file = *jkaSpFiles.begin(); jkaSpFiles.erase(jkaSpFiles.begin());
		FS_FCloseFile(file);
	}
	while (!jkaSpFileBuffers.empty()) {
		void *buffer = *jkaSpFileBuffers.begin(); jkaSpFileBuffers.erase(jkaSpFileBuffers.begin());
		FS_FreeFile(buffer);
	}
	while (!jkaSpAllocations.empty()) {
		void *buffer = jkaSpAllocations.begin()->first; jkaSpAllocations.erase(jkaSpAllocations.begin());
		Z_Free(buffer);
	}
	jkaSpCvars.clear();
	jkaSpGhoul2.Close();
	jkaSpModels.Clear();
	jkaSpAnimationCFGs.Clear();
	jkaSpImports = {};
}
template<int Slot> void QDECL SV_JKASPUnavailable(...) {
	Com_Error(ERR_DROP, "Jedi Academy single-player import %s (%i) is not served yet",
		fntech3::jka::sp::ImportName(Slot), Slot);
}
void QDECL SV_JKASPPrintf(const char *format, ...) {
	char text[4096]; va_list args; va_start(args, format);
	Q_vsnprintf(text, sizeof(text), format, args); va_end(args);
	Com_Printf("%s", text);
}
void QDECL SV_JKASPError(int, const char *format, ...) {
	char text[4096]; va_list args; va_start(args, format);
	Q_vsnprintf(text, sizeof(text), format, args); va_end(args);
	Com_Error(ERR_DROP, "Jedi Academy single-player game: %s", text);
}
/* SAVEGAME is retail's bit 256, where Q3 uses TEMP. Record it externally;
 * saved-game persistence remains a separate unserved contract. */
int SV_JKASPCvarFlags(int flags) { return flags & (1|2|4|8|16|32|64|128|512|1024); }
void SV_JKASPSyncCvars() {
	for (auto &entry : jkaSpCvars) {
		cvar_t *internal = Cvar_Get(entry.first.c_str(), "", 0);
		auto &external = entry.second;
		external.name = internal->name; external.string = internal->string;
		external.resetString = internal->resetString; external.latchedString = internal->latchedString;
		external.modified = internal->modified; external.modificationCount = internal->modificationCount;
		external.value = internal->value; external.integer = internal->integer;
	}
}
fntech3_jka_sp_cvar_prefix_t *SV_JKASPCvar(const char *name, const char *value, int flags) {
	if (!name || !value) Com_Error(ERR_DROP, "Jedi Academy single-player cvar has null input");
	Cvar_Get(name, value, SV_JKASPCvarFlags(flags));
	/* Use the canonical engine name to give case variants the same address. */
	cvar_t *internal = Cvar_Get(name, value, 0);
	auto &external = jkaSpCvars[internal->name];
	external.flags |= flags & 2047;
	SV_JKASPSyncCvars(); return &external;
}
void SV_JKASPCvarSet(const char *name, const char *value) {
	if (!name || !value) Com_Error(ERR_DROP, "Jedi Academy single-player cvar set has null input");
	Cvar_Set(name, value); SV_JKASPSyncCvars();
}
char *SV_JKASPArgv(int index) { return const_cast<char *>(Cmd_Argv(index)); }
void SV_JKASPSetConfigstring(int32_t index, const char *text) {
	if (!text) Com_Error(ERR_DROP, "Jedi Academy single-player set a null configstring");
	SV_SetConfigstring(index, text);
}
void SV_JKASPGetConfigstring(int32_t index, char *buffer, int32_t bytes) {
	if (!buffer || bytes < 1) Com_Error(ERR_DROP, "Invalid Jedi Academy single-player configstring buffer");
	SV_GetConfigstring(index, buffer, bytes);
}
void SV_JKASPGetServerinfo(char *buffer, int32_t bytes) {
	if (!buffer || bytes < 1) Com_Error(ERR_DROP, "Invalid Jedi Academy single-player serverinfo buffer");
	SV_GetServerinfo(buffer, bytes);
}
void SV_JKASPGetUserinfo(int32_t client, char *buffer, int32_t bytes) {
	if (!buffer || bytes < 1) Com_Error(ERR_DROP, "Invalid Jedi Academy single-player userinfo buffer");
	SV_GetUserinfo(client, buffer, bytes);
}
void SV_JKASPSetUserinfo(int32_t client, const char *text) {
	if (!text) Com_Error(ERR_DROP, "Jedi Academy single-player set null userinfo");
	SV_SetUserinfo(client, text);
}
void SV_JKASPDropClient(int32_t client, const char *reason) {
	if (!reason) Com_Error(ERR_DROP, "Jedi Academy single-player dropped a client without a reason");
	SV_GameDropClient(client, reason);
}
void QDECL SV_JKASPSendServerCommand(int32_t client, const char *format, ...) {
	if (!format) Com_Error(ERR_DROP, "Jedi Academy single-player sent a null server command");
	char text[4096]; va_list args; va_start(args, format);
	Q_vsnprintf(text, sizeof(text), format, args); va_end(args);
	SV_GameSendServerCommand(client, text);
}
int32_t SV_JKASPOpenFile(const char *path, int32_t *file, int32_t mode) {
	if (!path || mode < FS_READ || mode > FS_APPEND_SYNC)
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player file open argument");
	if (file) *file = 0;
	const int result = FS_FOpenFileByMode(path, file, static_cast<fsMode_t>(mode));
	if (file && *file > 0) jkaSpFiles.insert(*file);
	return result;
}
void SV_JKASPCheckFile(int32_t file, const void *buffer, int32_t length) {
	if (length < 0 || (length && !buffer) || !jkaSpFiles.count(file))
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player file buffer or handle");
}
int32_t SV_JKASPRead(void *buffer, int32_t length, int32_t file) {
	SV_JKASPCheckFile(file, buffer, length); return FS_Read(buffer, length, file);
}
int32_t SV_JKASPWrite(const void *buffer, int32_t length, int32_t file) {
	SV_JKASPCheckFile(file, buffer, length); return FS_Write(buffer, length, file);
}
void SV_JKASPCloseFile(int32_t file) {
	if (!jkaSpFiles.erase(file)) Com_Error(ERR_DROP, "Jedi Academy single-player closed an unowned file");
	FS_FCloseFile(file);
}
int32_t SV_JKASPReadFile(const char *name, void **buffer) {
	if (!name) Com_Error(ERR_DROP, "Jedi Academy single-player read a null file name");
	if (buffer) *buffer = nullptr;
	const int result = FS_ReadFile(name, buffer);
	if (buffer && *buffer) jkaSpFileBuffers.insert(*buffer);
	return result;
}
void SV_JKASPFreeFile(void *buffer) {
	if (!buffer) return;
	if (!jkaSpFileBuffers.erase(buffer)) Com_Error(ERR_DROP, "Jedi Academy single-player freed an unowned file buffer");
	FS_FreeFile(buffer);
}
int32_t SV_JKASPFileList(const char *path, const char *extension, char *list, int32_t size) {
	if (!path || !extension || !list || size < 1)
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player file list argument");
	return FS_GetFileList(path, extension, list, size);
}
void *SV_JKASPMalloc(int32_t bytes, int32_t tag, int32_t zero) {
	if (bytes < 0 || tag < 0 || tag > 255 || (zero != 0 && zero != 1))
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player allocation");
	void *buffer = Z_Malloc(bytes);
	jkaSpAllocations.emplace(buffer, JKASPAllocation{bytes, static_cast<uint8_t>(tag)});
	return buffer;
}
int32_t SV_JKASPFree(void *buffer) {
	if (!buffer) return 0;
	const auto found = jkaSpAllocations.find(buffer);
	if (found == jkaSpAllocations.end()) Com_Error(ERR_DROP, "Jedi Academy single-player freed an unowned allocation");
	const int32_t bytes = found->second.bytes;
	/* Retail Z_Free returns the requested byte count (0x442a9a), and its
	 * STATIC tag 10 is retained. The host releases it when the DLL ends. */
	if (found->second.tag != 10) { jkaSpAllocations.erase(found); Z_Free(buffer); }
	return bytes;
}
std::size_t JKASPModelFiles::Read(const char *path,void *destination,std::size_t capacity) {
	void *bytes=nullptr;
	const int length=SV_JKASPReadFile(path,&bytes);
	if (length<=0 || !bytes) { if (bytes) SV_JKASPFreeFile(bytes);return 0; }
	const std::size_t size=std::size_t(length);
	if (destination && capacity>=size) std::memcpy(destination,bytes,size);
	SV_JKASPFreeFile(bytes);
	return size;
}
int32_t SV_JKASPPrecacheModel(const char *name) {
	const auto result=jkaSpModels.Register(name);
	if (result.error==fntech3::jka::sp::PrecacheError::Unsupported)
		Com_Error(ERR_DROP,"Jedi Academy single-player MD3/BSP model precache is not served yet: %s",name);
	if (result.error==fntech3::jka::sp::PrecacheError::BuiltinSkeleton)
		Com_Error(ERR_DROP,"Jedi Academy single-player built-in Ghoul2 skeleton precache is not served yet: %s",name);
	return result.handle;
}
int32_t SV_JKASPInitGhoul2Model(fntech3_jka_sp_ghoul2_handle_t *handle,const char *name,
	int32_t modelIndex,int32_t customSkin,int32_t customShader,int32_t modelFlags,int32_t lodBias) {
	return jkaSpGhoul2.Init(jkaSpModels,handle,name,modelIndex,customSkin,customShader,modelFlags,lodBias);
}
char *SV_JKASPAnimFileName(int32_t handle) { return jkaSpModels.AnimationFileName(handle); }
char *SV_JKASPAnimInternalName(int32_t handle) { return jkaSpModels.AnimationInternalName(handle); }
int32_t SV_JKASPAnimationCFG(const char *name,char *destination,int32_t capacity) {
	const auto result=jkaSpAnimationCFGs.Get(name,destination,capacity);
	if(result.error!=fntech3::jka::sp::AnimationCFGError::None)
		Com_Error(ERR_DROP,"Jedi Academy single-player RE_GetAnimationCFG (122) refused invalid or unbounded text request (%i)",int(result.error));
	return result.length;
}
int32_t SV_JKASPIsFromZone(const void *buffer, int32_t tag) {
	const auto found = jkaSpAllocations.find(const_cast<void *>(buffer));
	/* Despite its source qboolean declaration, retail 0x442ad2 returns
	 * allocation bytes, not one, after comparing the low tag byte. */
	return found != jkaSpAllocations.end() && found->second.tag == uint8_t(tag)
		? found->second.bytes : 0;
}
void SV_JKASPBindEntities() {
	if (!jkaSpGame || !jkaSpGame->gentities)
		Com_Error(ERR_DROP, "Jedi Academy single-player game has not published its entity allocation");
	if (const char *reason = fntech3::jka::sp::Validate(jkaSpGame))
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player game data: %s", reason);
	const int count = jkaSpGame->num_entities;
	if (serverGameData.IsBound() && sv.gentities == jkaSpGame->gentities
		&& serverGameData.EntityCount() == (std::max)(1, count)) { sv.num_entities = count; return; }
	fntech3::server::NativeGameDataBinding binding;
	binding.entityBase = jkaSpGame->gentities;
	binding.entityCount = (std::max)(1, count);
	binding.entityCapacity = FNTECH3_JKA_SP_MAX_GENTITIES;
	binding.entityStride = FNTECH3_JKA_SP_GENTITY_BYTES;
	binding.entitySpanBytes = binding.entityStride * FNTECH3_JKA_SP_MAX_GENTITIES;
	/* No player record is read: the layout's player size and ping are absent. */
	binding.clientCount = 1;
	const auto error = serverGameData.BindNative(COM_GAME_JKA_SP, binding);
	if (error != fntech3::server::GameDataBindError::None)
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player entity allocation: %s",
			fntech3::server::GameDataBindErrorName(error));
	sv.gentities = jkaSpGame->gentities; sv.gentitySize = jkaSpGame->gentitySize;
	sv.num_entities = count;
}
sharedEntity_t *SV_JKASPWorldEntity(const void *address, bool geometry) {
	SV_JKASPBindEntities();
	fntech3::server::GameEntityRef reference;
	if (!serverGameData.EntityRefForPointer(address, reference))
		Com_Error(ERR_DROP, "Jedi Academy single-player world argument is not an entity allocation slot");
	if (geometry) {
		const auto *record = static_cast<const fntech3_jka_sp_gentity_prefix_t *>(address);
		if (!SV_ValidWorldBounds(record->mins, record->maxs)
			|| !SV_FiniteWorldVector(record->currentOrigin) || !SV_FiniteWorldVector(record->currentAngles))
			Com_Error(ERR_DROP, "Invalid Jedi Academy single-player entity geometry");
	}
	return reinterpret_cast<sharedEntity_t *>(const_cast<void *>(address));
}
const float *SV_JKASPVector(const float *value, bool optional = false) {
	if (!value && optional) return nullptr;
	if (!SV_FiniteWorldVector(value)) Com_Error(ERR_DROP, "Invalid Jedi Academy single-player world vector");
	return value;
}
void SV_JKASPSetBrushModel(void *entity, const char *name) {
	SV_SetBrushModel(SV_JKASPWorldEntity(entity, false), name);
}
void SV_JKASPLink(void *entity) {
	auto *internal = SV_JKASPWorldEntity(entity, true);
	// Retail's linker returns immediately for an entity which is not in use.
	auto *external = static_cast<fntech3_jka_sp_gentity_prefix_t *>(entity);
	if (!external->inuse) return;
	SV_LinkEntity(internal);
	// Retail0x43cfe1 uses SP SOLID1|BODY0x100, not Q3 BODY0x02000000.
	// Keep the native bit contract at this edge; shared profile linking
	// still owns bounds, spatial references and linked-state publication.
	if (!external->bmodel) {
		external->s.solid = 0;
		if (external->contents & 0x101) {
			const int x = static_cast<int>(std::clamp(external->maxs[0], 1.0f, 255.0f));
			const int zDown = static_cast<int>(std::clamp(-external->mins[2], 1.0f, 255.0f));
			const int zUp = static_cast<int>(std::clamp(external->maxs[2] + 32.0f, 1.0f, 255.0f));
			external->s.solid = (zUp << 16) | (zDown << 8) | x;
		}
	}
}
void SV_JKASPUnlink(void *entity) { SV_UnlinkEntity(SV_JKASPWorldEntity(entity, false)); }
const fntech3_jka_sp_gentity_prefix_t *SV_JKASPTraceEntity(int32_t index) {
	fntech3::server::GameEntityRef reference;
	if (!serverGameData.EntityRefAt(index,reference))
		Com_Error(ERR_DROP,"Jedi Academy single-player trace has an invalid entity slot %i",index);
	return static_cast<const fntech3_jka_sp_gentity_prefix_t *>(serverGameData.EntityPointer(reference));
}
const fntech3_jka_sp_gentity_prefix_t *SV_JKASPTraceOwner(uint32_t address) {
	if (!address) return nullptr;
	fntech3::server::GameEntityRef reference;
	if (!serverGameData.EntityRefForPointer(reinterpret_cast<const void *>(uintptr_t(address)),reference))
		Com_Error(ERR_DROP,"Jedi Academy single-player trace owner is outside the entity allocation");
	return static_cast<const fntech3_jka_sp_gentity_prefix_t *>(serverGameData.EntityPointer(reference));
}
void SV_JKASPTraceResult(const trace_t &trace) {
	if (!std::isfinite(trace.fraction) || trace.fraction<0 || trace.fraction>1
		|| !SV_FiniteWorldVector(trace.endpos))
		Com_Error(ERR_DROP,"Invalid Jedi Academy single-player collision result");
}
/* Retail SP0x43d9a0 sweeps entities only as far as the world endpoint and
 * rescales their fraction afterward. Its owner filters0x43d614..640 include
 * the pass entity's owner as well as siblings; the shared Q3 loop differs.
 * Keep this contained in SP and use the shared collision primitives. */
trace_t SV_JKASPTraceCandidates(const float *start,const float *mins,const float *maxs,
	const float *end,int32_t passEntity,int32_t mask,int32_t selector,int32_t useLod) {
	trace_t best{};
	CM_BoxTrace(&best,start,end,mins,maxs,0,mask,qfalse);
	SV_JKASPTraceResult(best);
	best.entityNum=best.fraction!=1 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	if (best.fraction==0) return best;
	const float worldFraction=best.fraction;
	float shortEnd[3],boxMins[3],boxMaxs[3];
	std::memcpy(shortEnd,best.endpos,sizeof(shortEnd));
	best.fraction=1;
	for (int axis=0;axis<3;++axis) {
		boxMins[axis]=(end[axis]>start[axis] ? start[axis] : shortEnd[axis])+mins[axis]-1;
		boxMaxs[axis]=(end[axis]>start[axis] ? shortEnd[axis] : start[axis])+maxs[axis]+1;
	}
	if (!SV_ValidWorldBounds(boxMins,boxMaxs))
		Com_Error(ERR_DROP,"Invalid Jedi Academy single-player trace sweep bounds");
	std::array<int,FNTECH3_JKA_SP_MAX_GENTITIES> touches;
	const int count=SV_AreaEntities(boxMins,boxMaxs,touches.data(),int(touches.size()));
	if (count<0 || count>int(touches.size()))
		Com_Error(ERR_DROP,"Jedi Academy single-player trace broadphase count is out of bounds");
	const auto *pass=passEntity!=ENTITYNUM_NONE ? SV_JKASPTraceEntity(passEntity) : nullptr;
	const auto *passOwner=pass ? SV_JKASPTraceOwner(pass->owner_address) : nullptr;
	for (int i=0;i<count && !best.allsolid;++i) {
		const auto *touch=SV_JKASPTraceEntity(touches[i]);
		if (pass) {
			if (touch->s.number==passEntity) continue;
			const auto *owner=SV_JKASPTraceOwner(touch->owner_address);
			if ((owner && owner->s.number==passEntity) || passOwner==touch
				|| (passOwner && owner==passOwner)) continue;
		}
		if (!(touch->contents & mask)) continue;
		if (touch->s.number<0 || touch->s.number>=FNTECH3_JKA_SP_MAX_GENTITIES
			|| !SV_ValidWorldBounds(touch->mins,touch->maxs)
			|| !SV_FiniteWorldVector(touch->currentOrigin) || !SV_FiniteWorldVector(touch->currentAngles))
			Com_Error(ERR_DROP,"Invalid Jedi Academy single-player trace entity geometry");
		trace_t candidate{};
		/* Retail CM_InitBoxHull0x40da6b publishes SP BODY0x100. The shared
		 * hull starts with Q3 BODY0x02000000, which SP mask0x1381 excludes.
		 * Lease the contract value only for this SP box primitive; restore the
		 * shared value before model ownership checks or any named refusal. */
		if (!touch->bmodel) {
			jkaSpTempBoxContentsActive=true;CM_SetTempBoxModelContents(0x100);
		}
		SV_ClipToEntity(&candidate,start,mins,maxs,shortEnd,touches[i],mask,qfalse);
		if (jkaSpTempBoxContentsActive) {
			jkaSpTempBoxContentsActive=false;CM_SetTempBoxModelContents(CONTENTS_BODY);
		}
		SV_JKASPTraceResult(candidate);
		/* CM's retail temporary record starts with entityNum0. The shared
		 * primitive labels every fraction hit; SP labels only the merge cases
		 * below, which determines whether this candidate needs model tracing. */
		candidate.entityNum=0;
		if (candidate.allsolid) {
			if (!best.allsolid) best.entityNum=touch->s.number;
			best.allsolid=qtrue;candidate.entityNum=touch->s.number;
		} else if (candidate.startsolid) {
			if (!best.startsolid) best.entityNum=touch->s.number;
			best.startsolid=qtrue;candidate.entityNum=touch->s.number;
		}
		if (candidate.fraction<best.fraction) {
			candidate.entityNum=touch->s.number;
			const qboolean previousStart=best.startsolid;
			best=candidate;best.startsolid=SV_QBool(SV_AsBool(best.startsolid)||SV_AsBool(previousStart));
		}
		/* Primary0x43d744 tests this candidate's entity number, including a
		 * start/allsolid candidate that did not replace the closest fraction.
		 * It then checks nonempty Ghoul2 and excludes LIGHTSABER0x40000. A
		 * model miss would roll back the box result0x43d924, so a box hit must
		 * never stand in for unimplemented model geometry. Canonical count
		 * also rejects foreign handles and tampered guest vector headers. */
		if (selector && candidate.entityNum==touch->s.number
			&& jkaSpGhoul2.ModelCount(touch->ghoul2_handle) && !(touch->contents & 0x40000))
			Com_Error(ERR_DROP,"Jedi Academy single-player Ghoul2 trace geometry is not served yet "
				"(selector=%i, useLod=%i, passEntity=%i, contentMask=0x%08x, candidate=%i)",
				selector,useLod,passEntity,unsigned(mask),touch->s.number);
	}
	best.fraction*=worldFraction;
	return best;
}
void SV_JKASPTrace(fntech3_jka_sp_trace_t *result, const float *start, const float *mins,
	const float *maxs, const float *end, int32_t passEntity, int32_t mask, int32_t ghoulTrace, int32_t useLod) {
	if (!result) Com_Error(ERR_DROP, "Jedi Academy single-player trace has no result record");
	if (ghoulTrace<0 || ghoulTrace>2)
		Com_Error(ERR_DROP,"Jedi Academy single-player trace has an invalid Ghoul2 selector %i",ghoulTrace);
	SV_JKASPBindEntities();
	const float *lower = SV_JKASPVector(mins, true), *upper = SV_JKASPVector(maxs, true);
	const float zero[3]{};
	if (!SV_ValidWorldBounds(lower ? lower : zero, upper ? upper : zero))
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player trace bounds");
	const trace_t internal=SV_JKASPTraceCandidates(SV_JKASPVector(start),lower ? lower : zero,upper ? upper : zero,
		SV_JKASPVector(end),passEntity < 0 || passEntity >= FNTECH3_JKA_SP_MAX_GENTITIES ? ENTITYNUM_NONE : passEntity,
		mask,ghoulTrace,useLod);
	fntech3_jka_sp_trace_t external{};
	external.allsolid = internal.allsolid; external.startsolid = internal.startsolid;
	external.fraction = internal.fraction;
	std::memcpy(external.endpos, internal.endpos, sizeof(external.endpos));
	std::memcpy(external.plane.normal, internal.plane.normal, sizeof(external.plane.normal));
	external.plane.dist = internal.plane.dist; external.plane.type = internal.plane.type;
	external.plane.signbits = internal.plane.signbits;
	external.surfaceFlags = internal.surfaceFlags; external.contents = internal.contents; external.entityNum = internal.entityNum;
	/* Retail's collision constructors (0x43d9c0) initialize these two words
	 * in each 64-byte Ghoul2 record, including ordinary world-only traces. */
	for (auto &collision : external.collision) { collision.distance = 100000.0f; collision.entityNum = -1; }
	*result = external;
}
int32_t SV_JKASPPointContents(const float *point, int32_t passEntity) {
    SV_JKASPBindEntities();
    SV_JKASPVector(point);
    int contents = CM_PointContents(point, 0);
    std::array<int, FNTECH3_JKA_SP_MAX_GENTITIES> touches;
    const int count = SV_AreaEntities(point, point, touches.data(), static_cast<int>(touches.size()));
    if (count < 0 || count > static_cast<int>(touches.size()))
        Com_Error(ERR_DROP, "Invalid Jedi Academy single-player point broadphase count");
    for (int n = 0; n < count; ++n) {
        const auto *entity = SV_JKASPTraceEntity(touches[n]);
        if (touches[n] == passEntity) continue;
        if (!SV_ValidWorldBounds(entity->mins, entity->maxs) ||
            !SV_FiniteWorldVector(entity->s.origin) || !SV_FiniteWorldVector(entity->s.angles))
            Com_Error(ERR_DROP, "Invalid Jedi Academy single-player point entity geometry");
        clipHandle_t model;
        if (entity->bmodel) model = CM_InlineModel(entity->s.modelindex);
        else {
            // Retail CM_InitBoxHull0x40da6b seeds BODY0x100, and
            // CM_TempBoxModel preserves it. Lease only this SP primitive.
            jkaSpTempBoxContentsActive = true;
            CM_SetTempBoxModelContents(0x100);
            model = CM_TempBoxModel(entity->mins, entity->maxs, qfalse);
        }
        // Retail0x43dc08/0b use state angles/origin. Temporary-box point
        // queries bypass rotation inside the shared collision primitive.
        contents |= CM_TransformedPointContents(point, model, entity->s.origin, entity->s.angles);
        if (jkaSpTempBoxContentsActive) {
            jkaSpTempBoxContentsActive = false;
            CM_SetTempBoxModelContents(CONTENTS_BODY);
        }
    }
    return contents;
}
int32_t SV_JKASPInPVS(const float *first, const float *second) {
	return SV_inPVS(SV_JKASPVector(first), SV_JKASPVector(second));
}
int32_t SV_JKASPInPVSIgnorePortals(const float *first, const float *second) {
	return SV_inPVSIgnorePortals(SV_JKASPVector(first), SV_JKASPVector(second)) ? 1 : 0;
}
void SV_JKASPAdjustPortal(void *entity, int32_t open) {
	SV_AdjustAreaPortalState(SV_JKASPWorldEntity(entity, false), open != 0);
}
int32_t SV_JKASPAreasConnected(int32_t first, int32_t second) { return CM_AreasConnected(first, second); }
int32_t SV_JKASPEntitiesInBox(const float *mins, const float *maxs, void **list, int32_t maximum) {
	if (!list || maximum < 0 || !SV_ValidWorldBounds(mins, maxs))
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player entity box query");
	if (!maximum) return 0;
	SV_JKASPBindEntities(); std::array<int, 1024> numbers{};
	const int count = SV_AreaEntities(mins, maxs, numbers.data(), (std::min)(maximum, 1024));
	for (int index = 0; index < count; ++index) {
		fntech3::server::GameEntityRef reference;
		if (!serverGameData.EntityRefAt(numbers[index], reference))
			Com_Error(ERR_DROP, "Jedi Academy single-player box query returned an invalid entity");
		list[index] = serverGameData.EntityPointer(reference);
	}
	return count;
}
int32_t SV_JKASPContact(const float *mins, const float *maxs, const void *entity) {
	if (!SV_ValidWorldBounds(mins, maxs)) Com_Error(ERR_DROP, "Invalid Jedi Academy single-player contact bounds");
	return SV_EntityContact(mins, maxs, SV_JKASPWorldEntity(entity, true), false) ? 1 : 0;
}
void SV_JKASPStoreVmCvar(fntech3_jka_sp_vmCvar_t *external,const vmCvar_t &internal) {
	external->handle=internal.handle; external->modificationCount=internal.modificationCount;
	external->value=internal.value; external->integer=internal.integer;
	std::memcpy(external->string,internal.string,sizeof(external->string));
}
void SV_JKASPCgameRegister(fntech3_jka_sp_vmCvar_t *external,const char *name,const char *value,int flags) {
	SV_JKASPCvar(name,value,flags);
	vmCvar_t internal{}; internal.handle=-1;
	Cvar_Register(external ? &internal : nullptr,name,value,SV_JKASPCvarFlags(flags),gvm->privateFlag);
	if (external && internal.handle >= 0) SV_JKASPStoreVmCvar(external,internal);
}
void SV_JKASPCgameUpdate(fntech3_jka_sp_vmCvar_t *external) {
	if (!external) Com_Error(ERR_DROP,"Jedi Academy single-player cgame has a null cvar update");
	vmCvar_t internal{};
	internal.handle=external->handle; internal.modificationCount=external->modificationCount;
	internal.value=external->value; internal.integer=external->integer;
	std::memcpy(internal.string,external->string,sizeof(external->string));
	Cvar_Update(&internal,gvm->privateFlag); SV_JKASPStoreVmCvar(external,internal);
}
void SV_JKASPCgameArgs(char *buffer,int bytes) {
	if (!buffer || bytes < 1) Com_Error(ERR_DROP,"Jedi Academy single-player cgame has invalid args storage");
	/* Retail0x419bb0 starts at argv1 and appends a space even after the
	 * last argument. Write directly within the caller's bound instead of
	 * building retail's unchecked intermediate string. */
	std::memset(buffer,0,std::size_t(bytes));
	int used=0;
	for (int index=1,count=Cmd_Argc();index<count && used<bytes-1;++index) {
		const char *text=Cmd_Argv(index);
		while (*text && used<bytes-1) buffer[used++]=*text++;
		if (used<bytes-1) buffer[used++]=' ';
	}
}
/* dllEntry's CG_PreInit and game Init need general cvar/filesystem services
 * even on a dedicated server. Only measured general calls are served; client records, renderer
 * and other SP cgame services retain explicit numbered refusals. */
intptr_t QDECL SV_JKASPCgameSyscall(intptr_t call, ...) {
	va_list args; va_start(args,call);
	if (call==FNTECH3_JKA_SP_CG_PRINT || call==FNTECH3_JKA_SP_CG_ERROR) {
		const char *text=va_arg(args,const char *); va_end(args);
		if (!text) Com_Error(ERR_DROP,"Jedi Academy single-player cgame has null diagnostic text");
		if (call==FNTECH3_JKA_SP_CG_ERROR) Com_Error(ERR_DROP,"Jedi Academy single-player cgame: %s",text);
		Com_Printf("%s",text); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_MILLISECONDS) { va_end(args); return Sys_Milliseconds(); }
	if (call==FNTECH3_JKA_SP_CG_CVAR_REGISTER) {
		auto *external=va_arg(args,fntech3_jka_sp_vmCvar_t *);
		const char *name=va_arg(args,const char *); const char *value=va_arg(args,const char *);
		const int flags=va_arg(args,int); va_end(args);
		SV_JKASPCgameRegister(external,name,value,flags); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_CVAR_UPDATE) {
		auto *external=va_arg(args,fntech3_jka_sp_vmCvar_t *); va_end(args);
		SV_JKASPCgameUpdate(external); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_CVAR_SET) {
		const char *name=va_arg(args,const char *); const char *value=va_arg(args,const char *);
		va_end(args); SV_JKASPCvarSet(name,value); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_ARGC) { va_end(args); return Cmd_Argc(); }
	if (call==FNTECH3_JKA_SP_CG_ARGV) {
		const int index=va_arg(args,int); char *buffer=va_arg(args,char *); const int bytes=va_arg(args,int);
		va_end(args);
		if (!buffer || bytes < 1) Com_Error(ERR_DROP,"Jedi Academy single-player cgame has invalid argv storage");
		Q_strncpyz(buffer,Cmd_Argv(index),bytes); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_ARGS) {
		char *buffer=va_arg(args,char *); const int bytes=va_arg(args,int); va_end(args);
		SV_JKASPCgameArgs(buffer,bytes); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_FS_FOPENFILE) {
		const char *path=va_arg(args,const char *); auto *file=va_arg(args,int32_t *);
		const int mode=va_arg(args,int); va_end(args);
		if (!file) Com_Error(ERR_DROP,"Jedi Academy single-player cgame has null file handle storage");
		return SV_JKASPOpenFile(path,file,mode);
	}
	if (call==FNTECH3_JKA_SP_CG_FS_READ) {
		void *buffer=va_arg(args,void *); const int bytes=va_arg(args,int), file=va_arg(args,int); va_end(args);
		SV_JKASPRead(buffer,bytes,file); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_FS_WRITE) {
		const void *buffer=va_arg(args,const void *); const int bytes=va_arg(args,int), file=va_arg(args,int); va_end(args);
		SV_JKASPWrite(buffer,bytes,file); return 0;
	}
	if (call==FNTECH3_JKA_SP_CG_FS_FCLOSEFILE) {
		const int file=va_arg(args,int); va_end(args); SV_JKASPCloseFile(file); return 0;
	}
	va_end(args);
	Com_Error(ERR_DROP, "Jedi Academy single-player cgame import %i is not served yet", int(call));
	return 0;
}
void SV_JKASPFillImports() {
	jkaSpImports = {};
#define FNTECH3_JKA_SP_IMPORT(name, slot) \
	jkaSpImports.slots[slot] = reinterpret_cast<uintptr_t>(SV_JKASPUnavailable<slot>);
#include "../compat/jka_sp_imports.def"
#undef FNTECH3_JKA_SP_IMPORT
#define JKA_SP_SERVE(name, callback) \
	jkaSpImports.slots[FNTECH3_JKA_SP_IMPORT_##name] = reinterpret_cast<uintptr_t>(callback)
	JKA_SP_SERVE(Printf, SV_JKASPPrintf);
	JKA_SP_SERVE(Error, SV_JKASPError);
	JKA_SP_SERVE(Milliseconds, Sys_Milliseconds);
	JKA_SP_SERVE(cvar, SV_JKASPCvar);
	JKA_SP_SERVE(cvar_set, SV_JKASPCvarSet);
	JKA_SP_SERVE(Cvar_VariableIntegerValue, Cvar_VariableIntegerValue);
	JKA_SP_SERVE(Cvar_VariableStringBuffer, Cvar_VariableStringBuffer);
	JKA_SP_SERVE(argc, Cmd_Argc);
	JKA_SP_SERVE(argv, SV_JKASPArgv);
	JKA_SP_SERVE(FS_FOpenFile, SV_JKASPOpenFile);
	JKA_SP_SERVE(FS_Read, SV_JKASPRead);
	JKA_SP_SERVE(FS_Write, SV_JKASPWrite);
	JKA_SP_SERVE(FS_FCloseFile, SV_JKASPCloseFile);
	JKA_SP_SERVE(FS_ReadFile, SV_JKASPReadFile);
	JKA_SP_SERVE(FS_FreeFile, SV_JKASPFreeFile);
	JKA_SP_SERVE(FS_GetFileList, SV_JKASPFileList);
	JKA_SP_SERVE(SendConsoleCommand, Cbuf_AddText);
	JKA_SP_SERVE(DropClient, SV_JKASPDropClient);
	JKA_SP_SERVE(SendServerCommand, SV_JKASPSendServerCommand);
	JKA_SP_SERVE(SetConfigstring, SV_JKASPSetConfigstring);
	JKA_SP_SERVE(GetConfigstring, SV_JKASPGetConfigstring);
	JKA_SP_SERVE(GetUserinfo, SV_JKASPGetUserinfo);
	JKA_SP_SERVE(SetUserinfo, SV_JKASPSetUserinfo);
	JKA_SP_SERVE(GetServerinfo, SV_JKASPGetServerinfo);
	JKA_SP_SERVE(Malloc, SV_JKASPMalloc);
	JKA_SP_SERVE(Free, SV_JKASPFree);
	JKA_SP_SERVE(bIsFromZone, SV_JKASPIsFromZone);
	JKA_SP_SERVE(totalMapContents, CM_TotalMapContents);
	JKA_SP_SERVE(SetBrushModel, SV_JKASPSetBrushModel);
	JKA_SP_SERVE(trace, SV_JKASPTrace);
	JKA_SP_SERVE(pointcontents, SV_JKASPPointContents);
	JKA_SP_SERVE(inPVS, SV_JKASPInPVS);
	JKA_SP_SERVE(inPVSIgnorePortals, SV_JKASPInPVSIgnorePortals);
	JKA_SP_SERVE(AdjustAreaPortalState, SV_JKASPAdjustPortal);
	JKA_SP_SERVE(AreasConnected, SV_JKASPAreasConnected);
	JKA_SP_SERVE(linkentity, SV_JKASPLink);
	JKA_SP_SERVE(unlinkentity, SV_JKASPUnlink);
	JKA_SP_SERVE(EntitiesInBox, SV_JKASPEntitiesInBox);
	JKA_SP_SERVE(EntityContact, SV_JKASPContact);
	JKA_SP_SERVE(G2API_PrecacheGhoul2Model, SV_JKASPPrecacheModel);
	JKA_SP_SERVE(G2API_InitGhoul2Model, SV_JKASPInitGhoul2Model);
	JKA_SP_SERVE(G2API_GetAnimFileNameIndex, SV_JKASPAnimFileName);
	JKA_SP_SERVE(G2API_GetAnimFileInternalNameIndex, SV_JKASPAnimInternalName);
	JKA_SP_SERVE(RE_GetAnimationCFG, SV_JKASPAnimationCFG);
	JKA_SP_SERVE(G2API_CleanGhoul2Models, SV_JKASPCleanGhoul2);
	JKA_SP_SERVE(TheGhoul2InfoArray, SV_JKASPTheGhoul2InfoArray);
#undef JKA_SP_SERVE
	/* VoiceVolume is a data slot. Keep a real zeroed array, not a function
 * address the game would read as speech amplitudes. Dedicated has no audio. */
	static int32_t voiceVolume[FNTECH3_JKA_SP_MAX_GENTITIES]{};
	jkaSpImports.slots[FNTECH3_JKA_SP_IMPORT_VoiceVolume] = reinterpret_cast<uintptr_t>(voiceVolume);
}
intptr_t QDECL SV_JKASPGameEntry(int command, ...) {
	if (command == GAME_SHUTDOWN) {
		/* Never run module Shutdown after a failed GetGameAPI/Init, and mark
		 * it stopped before entering Shutdown so error cleanup cannot re-enter. */
		const bool initialized = jkaSpInitialized; jkaSpInitialized = false;
		if (initialized && jkaSpGame && !com_errorEntered) jkaSpGame->Shutdown();
		SV_JKASPClearGame(); return 0;
	}
	if (!jkaSpGame) Com_Error(ERR_DROP, "Jedi Academy single-player game called without exports");
	SV_JKASPSyncCvars();
	if (command == GAME_INIT) {
		int32_t levelTime, seed;
		va_list args; va_start(args, command);
		levelTime = va_arg(args, int32_t); seed = va_arg(args, int32_t); va_end(args);
		char mapname[MAX_QPATH], target[MAX_QPATH];
		Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));
		Cvar_VariableStringBuffer("spawntarget", target, sizeof(target));
		jkaSpGame->Init(mapname, target, Cvar_VariableIntegerValue("sv_mapChecksum"),
			sv.entityParsePoint ? sv.entityParsePoint : "", levelTime, seed, Com_Milliseconds(),
			FNTECH3_JKA_SP_SAVED_NO, 0);
		if (const char *reason = fntech3::jka::sp::Validate(jkaSpGame))
			Com_Error(ERR_DROP, "Invalid Jedi Academy single-player game after Init: %s", reason);
		jkaSpInitialized = true;
		SV_JKASPBindEntities();
		Com_Printf("Jedi Academy single-player API-8 Init returned with bound entities\n");
		return 0;
	}
	if (command == BOTAI_START_FRAME) return 0;
	if (command == GAME_RUN_FRAME) {
		if (!jkaSpInitialized || !jkaSpGame->RunFrame) Com_Error(ERR_DROP, "Jedi Academy single-player game has no running frame export");
		va_list args; va_start(args, command); const int32_t time = va_arg(args, int32_t); va_end(args);
		jkaSpGame->RunFrame(time); SV_JKASPBindEntities(); return 0;
	}
	if (command == GAME_CONSOLE_COMMAND) return jkaSpGame->ConsoleCommand ? jkaSpGame->ConsoleCommand() : 0;
	if (command == GAME_CLIENT_CONNECT)
		return reinterpret_cast<intptr_t>("Jedi Academy single-player client and snapshot adapters are not served yet");
	Com_Error(ERR_DROP, "Jedi Academy single-player game export %i is not served yet", command);
	return 0;
}
vm_t *SV_JKASPLoadGame() {
	SV_JKASPClearGame();
	/* Previous DLL detach has completed before a new load. Close preserves
	 * only old handle release tombstones; Reset starts this module lifetime. */
	jkaSpGhoul2.Reset();
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable(VM_GAME, "GetGameAPI", SV_JKASPGameEntry, &symbol);
	if (!vm) return nullptr;
	/* Publish ownership before GetGameAPI can call Error. Normal server
 * cleanup can unload the DLL even if no export table was returned. */
	gvm = vm;
	SV_JKASPFillImports();
	auto getter = reinterpret_cast<fntech3_jka_sp_GetGameAPI_t>(symbol);
	auto *game = getter ? getter(&jkaSpImports) : nullptr;
	if (const char *reason = fntech3::jka::sp::Validate(game)) {
		SV_JKASPClearGame(); VM_Free(vm); gvm = nullptr;
		Com_Error(ERR_DROP, "Invalid Jedi Academy single-player game: %s", reason); return nullptr;
	}
	jkaSpGame = game;
	void *entry = Sys_LoadFunction(vm->dllHandle, "dllEntry");
	if (!entry) {
		SV_JKASPClearGame(); VM_Free(vm); gvm = nullptr;
		Com_Error(ERR_DROP, "Jedi Academy single-player module has no dllEntry"); return nullptr;
	}
	reinterpret_cast<dllEntry_t>(entry)(SV_JKASPCgameSyscall);
	return vm;
}
}
#endif
