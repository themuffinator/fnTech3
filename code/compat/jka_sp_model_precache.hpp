/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_MODEL_PRECACHE_HPP
#define FNTECH3_COMPAT_JKA_SP_MODEL_PRECACHE_HPP
#include "../ghoul2/ghoul2_server.hpp"
#include <array>

/* Scalar SP model handles, separate from both native Ghoul2 vectors and the
 * MP store's instance pointers. Retail import45 is a tail jump at0x4ce900 to
 * RE_RegisterModel0x470c00. The actual registry0x470800 rejects names>=64,
 * caches successes/failures case-insensitively, and reserves handles1..1023
 * (0x470700 bounds1024, with slot0 the bad model). This registry retains only
 * validated GLM/GLA files supplied through the engine filesystem, plus its
 * independently generated default identity metadata. No renderer
 * or STL address crosses the module boundary. Consumers of a returned handle
 * remain unserved until they use this exact registry rather than cast it.
 *
 * File ownership is established before Files::Read can report ERR_DROP.
 * No automatic owning local survives a filesystem callback, so the server's
 * ordinary partial-initialization cleanup can release the retained bytes. */
namespace fntech3::jka::sp {
/* Retail jasp0x4f3660's generated engine skeleton is one bone and one frame
 * at identity. These are independently expressed metadata, not its294-byte
 * serialized blob. Real poses remain an unserved Ghoul2 boundary. */
struct DefaultSkeletonMetadata {
	const char *boneName="ModView internal default";
	int parent=-1,children=0,frameIndex=0;
	float rest[12]{1,0,0,0,0,1,0,0,0,0,1,0};
	float inverseRest[12]{1,0,0,0,0,1,0,0,0,0,1,0};
	float quaternion[4]{1,0,0,0},translation[3]{0,0,0};
};
inline constexpr DefaultSkeletonMetadata DefaultSkeletonIdentity{};
enum class PrecacheError { None, Name, Missing, Malformed, Full, Budget, Unsupported, BuiltinSkeleton };
struct PrecacheResult { int handle; PrecacheError error; };
class ModelPrecache final {
public:
	static constexpr std::size_t Slots = 1024;
	/* Engine policy, not a retail ABI constant. Bound retained files as well
	 * as handle count, so many distinct maximum-sized files cannot exhaust x86. */
	static constexpr std::size_t MaxRetainedBytes=256u*1024u*1024u;
	explicit ModelPrecache(ghoul2::Files &files,std::size_t byteBudget=MaxRetainedBytes)
		: files_(files),byteBudget_(byteBudget) {}
	void Clear() noexcept { for (auto &entry:entries_) entry.reset(); next_=1; retainedBytes_=0; }
	std::size_t Count() const noexcept { return next_-1; }
	struct Entry {
		std::string name;
		std::vector<unsigned char> bytes;
		model::Mesh mesh{};
		model::Skeleton skeleton{};
		int animationHandle=0;
		std::array<char,64> animationName{},internalName{};
		const DefaultSkeletonMetadata *generatedDefault=nullptr;
		PrecacheError error=PrecacheError::Missing;
	};
	const Entry *Resolve(int handle) const noexcept {
		return handle>0 && std::size_t(handle)<next_ && entries_[std::size_t(handle)]
			&& entries_[std::size_t(handle)]->error==PrecacheError::None
			? entries_[std::size_t(handle)].get() : nullptr;
	}
	/* Retail0x4d0150 returns GLM+72;0x4d0170 returns GLA+8. Wrong kinds,
	 * failed registrations and out-of-range handles return the empty name.
	 * Copies keep the native char* API from exposing the validated file bytes. */
	char *AnimationFileName(int handle) noexcept {
		const Entry *entry=Resolve(handle);
		return entry && entry->mesh.version==model::MeshVersion
			? entries_[std::size_t(handle)]->animationName.data() : emptyName_;
	}
	char *AnimationInternalName(int handle) noexcept {
		const Entry *entry=Resolve(handle);
		return entry && entry->skeleton.version==model::SkeletonVersion
			? entries_[std::size_t(handle)]->internalName.data() : emptyName_;
	}
	PrecacheResult Register(const char *name) {
		char folded[64]{};
		if (!name || !name[0]) return {0,PrecacheError::Name};
		std::size_t length=0;
		for (;length<sizeof(folded) && name[length];++length)
			folded[length]=name[length]>='A' && name[length]<='Z' ? char(name[length]+32) : name[length];
		if (length==sizeof(folded)) return {0,PrecacheError::Name};
		if (folded[0]=='/' || folded[0]=='\\' || std::strchr(folded,':') || std::strstr(folded,".."))
			return {0,PrecacheError::Name};
		for (std::size_t index=1;index<next_;++index) {
			if (entries_[index]->name==folded)
				return {entries_[index]->error==PrecacheError::None ? int(index) : 0,entries_[index]->error};
		}
		const bool builtin=!std::strcmp(folded,"*default.gla");
		if (folded[0]=='*' && !builtin) return {0,PrecacheError::BuiltinSkeleton};
		if (folded[0]=='#' || (length>=4 && !std::strcmp(folded+length-4,".md3")))
			return {0,PrecacheError::Unsupported};
		if (next_==Slots) return {0,PrecacheError::Full};
		const std::size_t index=next_;
		entries_[index]=std::make_unique<Entry>();
		++next_;
		Entry *entry=entries_[index].get();entry->name=folded;
		if (builtin) {
			/* The retail first-load interception0x47020e..223 compares the
			 * original thirteen bytes case-sensitively. Its existing cache
			 * above remains case-insensitive, including failed first requests. */
			if (std::strcmp(name,"*default.gla")) return {0,entry->error};
			entry->generatedDefault=&DefaultSkeletonIdentity;
			entry->skeleton.version=model::SkeletonVersion;
			entry->skeleton.scale=1.0f;entry->skeleton.numFrames=1;
			entry->skeleton.numBones=1;entry->skeleton.maxPoseIndex=0;
			std::memcpy(entry->internalName.data(),"*default",9);
			entry->error=PrecacheError::None;return {int(index),PrecacheError::None};
		}
		const std::size_t size=files_.Read(entry->name.c_str(),nullptr,0);
		if (!size || size>ghoul2::MaxModelBytes) return {0,entry->error};
		if (retainedBytes_>byteBudget_ || size>byteBudget_-retainedBytes_) {
			entry->error=PrecacheError::Budget;return {0,entry->error};
		}
		retainedBytes_+=size;
		entry->bytes.resize(size);
		if (files_.Read(entry->name.c_str(),entry->bytes.data(),size)!=size) {
			return Fail(*entry);
		}
		entry->error=PrecacheError::Malformed;
		const model::Reader reader(entry->bytes.data(),entry->bytes.size());
		if (size<4) return Fail(*entry);
		if (reader.Unsigned(0)==model::SkeletonIdent) {
			if (model::ParseSkeleton(entry->bytes.data(),size,entry->skeleton)!=model::Error::None)
				return Fail(*entry);
			std::memcpy(entry->internalName.data(),entry->bytes.data()+8,64);
		} else if (reader.Unsigned(0)==model::MeshIdent) {
			if (model::ParseMesh(entry->bytes.data(),size,0,entry->mesh)!=model::Error::None)
				return Fail(*entry);
			std::memcpy(entry->animationName.data(),entry->bytes.data()+entry->mesh.animNameOffset,64);
			char animation[64]{};
			std::size_t at=0;
			while (at<64 && entry->bytes[entry->mesh.animNameOffset+at]) {
				animation[at]=char(entry->bytes[entry->mesh.animNameOffset+at]);++at;
			}
			if (!at || at>59) return Fail(*entry);
			std::memcpy(animation+at,".gla",5);
			const PrecacheResult registered=Register(animation);
			if (!registered.handle) {
				entry->error=registered.error;
				return Fail(*entry);
			}
			entry->animationHandle=registered.handle;
			const Entry *skeleton=Resolve(registered.handle);
			if (!skeleton || entry->mesh.numBones!=skeleton->skeleton.numBones
				|| skeleton->skeleton.version!=model::SkeletonVersion
				|| model::ParseMesh(entry->bytes.data(),size,skeleton->skeleton.numBones,entry->mesh)!=model::Error::None)
				return Fail(*entry);
		} else if (reader.Unsigned(0)==0x33504449u) {
			entry->error=PrecacheError::Unsupported;return Fail(*entry);
		} else return Fail(*entry);
		entry->error=PrecacheError::None;
		return {int(index),PrecacheError::None};
	}
private:
	PrecacheResult Fail(Entry &entry) {
		retainedBytes_-=entry.bytes.size();
		std::vector<unsigned char>().swap(entry.bytes);
		return {0,entry.error};
	}
	ghoul2::Files &files_;
	std::array<std::unique_ptr<Entry>,Slots> entries_{};
	std::size_t next_=1;
	std::size_t byteBudget_,retainedBytes_=0;
	char emptyName_[1]{};
};
}
#endif
