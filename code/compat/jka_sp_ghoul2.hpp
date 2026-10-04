/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_GHOUL2_HPP
#define FNTECH3_COMPAT_JKA_SP_GHOUL2_HPP
#include "jka_sp_ghoul2_abi.h"
#include "jka_sp_model_precache.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

namespace fntech3::jka::sp {
/* Engine-owned SP metadata, independently expressed from retail observations.
 * The DLL may read records and write their scalar fields. It must never resize
 * or destroy these old STL carriers: SP cgame exports, including resize7..11,
 * are refused in VM_Call, and the game loader never resolves retail vmMain.
 * Canonical ownership is separate from every module-writable vector header.
 * No DLL/private-CRT allocation, MP instance, or host STL object is exposed.
 * Posing, collision and rendering remain separately named import refusals. */
class Ghoul2Array {
public:
	using Error = void (*)(const char *);
	/* Bounded engine policy; the 204-byte record size is the retail contract. */
	static constexpr std::size_t MaxModels = 32;
	explicit Ghoul2Array(Error error) : error_(error) { Reset(); }
	Ghoul2Array(const Ghoul2Array &) = delete;
	Ghoul2Array &operator=(const Ghoul2Array &) = delete;
	fntech3_jka_sp_ghoul2_array_t *Interface() { return &object_.external; }
	/* Internal consumers use canonical count only after checking guest headers.
	 * A null handle is the retail empty instance; foreign nonzero IDs refuse. */
	std::size_t ModelCount(int32_t handle) { return handle ? Checked(handle).count : 0; }
	void Reset() noexcept {
		closed_=false;retired_.fill(0);
		for (int i=0; i<FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS; ++i) {
			slots_[i] = {};
			slots_[i].id = FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS + i;
			free_[i] = i;
		}
		freeCount_ = FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS;
	}
	void Close() noexcept {
		if (closed_) return;
		closed_=true;freeCount_=0;
		/* Retail gentity destruction0x200717e0 releases a nonzero Ghoul2
		 * handle through virtual Delete during DLL_PROCESS_DETACH, after the
		 * host's GAME_SHUTDOWN. Release canonical buffers now, even if module
		 * headers were modified, but retain exact acquired IDs for those late
		 * releases. These tombstones carry no model pointer or allocation. */
		for (std::size_t i=0;i<slots_.size();++i) {
			auto &slot=slots_[i];if (slot.allocated) retired_[i]=slot.id;
			slot.models.reset();slot.count=0;slot.vector={};slot.allocated=false;
		}
	}
	void Clean(fntech3_jka_sp_ghoul2_handle_t *handle) {
		if (!handle) Fail("Jedi Academy single-player Ghoul2 cleanup has a null handle");
		Delete(handle->handle); handle->handle = 0;
	}
	int32_t Init(ModelPrecache &models,fntech3_jka_sp_ghoul2_handle_t *handle,
		const char *name,int32_t /*modelIndex*/,int32_t customSkin,
		int32_t customShader,int32_t /*modelFlags*/,int32_t lodBias) {
		if (!handle) Fail("Jedi Academy single-player G2API_InitGhoul2Model (46) has a null handle");
		if (closed_) Fail("Jedi Academy single-player Ghoul2 array is closed for module teardown");
		/* Only zero denotes a new instance. Never silently replace a foreign,
		 * stale or tampered nonzero handle, even when the filename is missing. */
		Slot *slot=handle->handle ? &Checked(handle->handle) : nullptr;
		if (!name || !name[0]) return -1;
		if constexpr (sizeof(uintptr_t)!=sizeof(uint32_t))
			Fail("Jedi Academy single-player Ghoul2 model metadata requires a 32-bit native boundary");
		std::size_t index=0;
		if (slot) {
			while (index<slot->count && slot->models->records[index].modelindex!=-1) ++index;
			if (index==MaxModels) Fail("Jedi Academy single-player Ghoul2 model capacity exhausted");
		} else if (!freeCount_) Fail("Jedi Academy single-player Ghoul2 handle capacity exhausted");
		/* Registration owns all file bytes before filesystem callbacks. No
		 * automatic owning local crosses an ERR_DROP/longjmp callback here. */
		const PrecacheResult registered=models.Register(name);
		if (registered.error==PrecacheError::Unsupported || registered.error==PrecacheError::BuiltinSkeleton)
			Fail("Jedi Academy single-player G2API_InitGhoul2Model (46) requires supported GLM/GLA metadata");
		const ModelPrecache::Entry *mesh=models.Resolve(registered.handle);
		const ModelPrecache::Entry *skeleton=mesh ? models.Resolve(mesh->animationHandle) : nullptr;
		if (!mesh || mesh->mesh.version!=model::MeshVersion || !skeleton
			|| skeleton->skeleton.version!=model::SkeletonVersion
			|| mesh->mesh.numBones!=skeleton->skeleton.numBones) return -1;
		/* The filename may alias a removed record that this call replaces. */
		char fileName[64]{};std::memcpy(fileName,name,std::strlen(name)+1);
		if (!slot) { handle->handle=New();slot=&Checked(handle->handle); }
		if (!slot->models) slot->models=std::make_unique<Models>();
		auto &record=slot->models->records[index];record={};
		record.modelindex=int32_t(index);record.model=registered.handle;
		record.customSkin=customSkin;record.customShader=customShader;record.lodBias=lodBias;
		record.modelBoltLink=-1;record.newOrigin=-1;record.skelFrameNum=-1;record.meshFrameNum=-1;
		std::memcpy(record.fileName,fileName,sizeof(fileName));
		/* Retail TestModelPointers0x4d08d0 sets mValid only after its private
		 * model_t pointers and GLA header exist. This dedicated metadata subset
		 * truthfully registers validated files, but leaves that private validity,
		 * all model/pose pointers and their sizes zero. Raw files are not model_t.
		 * Retail Init clears the three lists without reserving bone storage. */
		if (index==slot->count) ++slot->count;
		Publish(*slot);
		return int32_t(index);
	}
private:
	struct Models { std::array<fntech3_jka_sp_ghoul2_info_t,MaxModels> records{}; };
	struct Slot {
		int32_t id=0;bool allocated=false;fntech3_jka_sp_ghoul2_vector_t vector{};
		std::unique_ptr<Models> models;std::size_t count=0;
	};
	struct Object { fntech3_jka_sp_ghoul2_array_t external; Ghoul2Array *owner; };
	std::array<Slot,FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS> slots_{};
	std::array<int32_t,FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS> free_{};
	std::array<int32_t,FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS> retired_{};
	int freeCount_ = 0;
	bool closed_=false;
	Error error_;
	[[noreturn]] void Fail(const char *text) const { error_(text); std::abort(); }
	bool Valid(int32_t handle) const {
		/* Retail only compares the generation ID. Restrict this developer
		 * subset further to handles actually acquired through New, so a
		 * guessed future ID cannot allocate or recycle an unowned slot. */
		if (handle <= 0) return false;
		const auto &slot = slots_[handle & 511];
		return slot.allocated && slot.id == handle;
	}
	fntech3_jka_sp_ghoul2_vector_t Header(const Slot &slot) const {
		if (!slot.models) return {};
		const uintptr_t begin=reinterpret_cast<uintptr_t>(slot.models->records.data());
		const uint64_t capacity=uint64_t(begin)+sizeof(slot.models->records);
		if (capacity>UINT32_MAX)
			Fail("Jedi Academy single-player Ghoul2 metadata address exceeds the 32-bit native boundary");
		return {0,uint32_t(begin),uint32_t(begin+slot.count*FNTECH3_JKA_SP_GHOUL2_INFO_BYTES),uint32_t(capacity)};
	}
	void Publish(Slot &slot) { slot.vector=Header(slot); }
	static bool Empty(const fntech3_jka_sp_ghoul2_vector_t &vector) {
		return !vector.allocator_reserved && !vector.begin_address && !vector.end_address && !vector.capacity_address;
	}
	Slot &Checked(int32_t handle) {
		if (closed_) Fail("Jedi Academy single-player Ghoul2 array is closed for module teardown");
		if (!Valid(handle)) Fail("Jedi Academy single-player Ghoul2 has an invalid or stale handle");
		auto &slot = slots_[handle & 511];
		const auto expected=Header(slot);
		if (std::memcmp(&slot.vector,&expected,sizeof(expected)))
			Fail("Jedi Academy single-player Ghoul2 model vector header was modified outside engine ownership");
		/* Inspect our allocation, never addresses carried by guest headers. */
		for (std::size_t i=0;i<slot.count;++i) {
			const auto &record=slot.models->records[i];
			if (!Empty(record.surfaces) || !Empty(record.bolts) || !Empty(record.bones))
				Fail("Jedi Academy single-player Ghoul2 nested vector resize is not served yet");
			if (record.valid || record.currentModel_address || record.animModel_address || record.animHeader_address
				|| record.transformedVertsArray_address || record.boneCache_address
				|| record.currentModelSize || record.currentAnimModelSize)
				Fail("Jedi Academy single-player Ghoul2 private model/pose pointers are not served yet");
		}
		return slot;
	}
	int32_t New() {
		if (closed_) Fail("Jedi Academy single-player Ghoul2 array is closed for module teardown");
		if (!freeCount_) Fail("Jedi Academy single-player Ghoul2 handle capacity exhausted");
		const int index = free_[0];
		for (int i=1; i<freeCount_; ++i) free_[i-1] = free_[i];
		--freeCount_; slots_[index].allocated = true; return slots_[index].id;
	}
	void Delete(int32_t handle) {
		if (!handle) return;
		if (closed_) {
			if (handle>0 && retired_[handle & 511]==handle) return;
			Fail("Jedi Academy single-player Ghoul2 teardown has an invalid or stale handle");
		}
		auto &slot = Checked(handle); slot.allocated = false;
		slot.models.reset();slot.count=0;slot.vector={};
		/* Retire a slot at signed overflow; never revive a stale old handle. */
		if (slot.id > (std::numeric_limits<int32_t>::max)() - 512) return;
		slot.id += 512;
		for (int i=freeCount_; i>0; --i) free_[i] = free_[i-1];
		free_[0] = handle & 511; ++freeCount_;
	}
	static Ghoul2Array &Owner(const void *self) {
		return *static_cast<const Object *>(self)->owner;
	}
	static int32_t FNTECH3_JKA_SP_THISCALL NewThunk(void *self) { return Owner(self).New(); }
	static void FNTECH3_JKA_SP_THISCALL DeleteThunk(void *self,int32_t handle) { Owner(self).Delete(handle); }
	static bool FNTECH3_JKA_SP_THISCALL ValidThunk(const void *self,int32_t handle) { return Owner(self).Valid(handle); }
	static fntech3_jka_sp_ghoul2_vector_t *FNTECH3_JKA_SP_THISCALL GetThunk(void *self,int32_t handle) {
		auto &owner = Owner(self); auto &slot=owner.Checked(handle);
		if (!slot.count) owner.Fail("Jedi Academy single-player Ghoul2 mutable Get (virtual slot 4) requires initialized model metadata");
		return &slot.vector;
	}
	static const fntech3_jka_sp_ghoul2_vector_t *FNTECH3_JKA_SP_THISCALL GetConstThunk(const void *self,int32_t handle) {
		return &Owner(self).Checked(handle).vector;
	}
	inline static const fntech3_jka_sp_ghoul2_array_vtable_t table_ = {
		NewThunk, DeleteThunk, ValidThunk, GetConstThunk, GetThunk
	};
	Object object_{{&table_},this};
};
/* Retained source name for the existing empty-handle contract tests. */
using EmptyGhoul2Array = Ghoul2Array;
}
#endif
