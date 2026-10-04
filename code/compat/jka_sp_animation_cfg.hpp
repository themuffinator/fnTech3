/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_ANIMATION_CFG_HPP
#define FNTECH3_COMPAT_JKA_SP_ANIMATION_CFG_HPP
#include "../ghoul2/ghoul2_server.hpp"
#include <array>

namespace fntech3::jka::sp {
/* Retail jasp0x467350 serves import122: case-insensitive path caching,
 * successful reads only, optional bounded/NUL-padded text copy, and strlen
 * return independent of destination capacity. A first embedded NUL ends the
 * visible text. This independently written cache retains engine-owned data;
 * it neither parses game animations nor returns renderer/old-STL objects.
 * Owning storage precedes filesystem callbacks so ERR_DROP cleanup is safe. */
enum class AnimationCFGError { None, Name, Capacity, Large, Budget, Full, Changed };
struct AnimationCFGResult { int length; AnimationCFGError error; };
class AnimationCFGCache final {
public:
	/* Engine resource policies, not retail ABI constants. The installed 134 model
	 * animation configuration files total 203780 bytes; the largest is 51637 bytes. */
	static constexpr std::size_t Slots=256,MaxFileBytes=1024u*1024u;
	static constexpr std::size_t MaxRetainedBytes=16u*1024u*1024u;
	explicit AnimationCFGCache(ghoul2::Files &files,std::size_t budget=MaxRetainedBytes)
		: files_(files),budget_(budget) {}
	void Clear() noexcept { for(auto &entry:entries_)entry.reset();count_=retained_=0; }
	std::size_t Count() const noexcept { return count_; }
	std::size_t RetainedBytes() const noexcept { return retained_; }
	AnimationCFGResult Get(const char *name,char *destination,int capacity) {
		if(destination && capacity<1)return {0,AnimationCFGError::Capacity};
		char folded[64]{};
		if(!name)return {0,AnimationCFGError::Name};
		std::size_t length=0;
		for(;length<sizeof(folded) && name[length];++length)
			folded[length]=name[length]>='A' && name[length]<='Z' ? char(name[length]+32) : name[length];
		if(length==sizeof(folded) || folded[0]=='/' || folded[0]=='\\' || std::strchr(folded,':'))
			return {0,AnimationCFGError::Name};
		for(std::size_t start=0;start<length;) {
			std::size_t end=start;
			while(end<length && folded[end]!='/' && folded[end]!='\\')++end;
			if(end-start==2 && folded[start]=='.' && folded[start+1]=='.')return {0,AnimationCFGError::Name};
			start=end+1;
		}
		if(!length)return {0,AnimationCFGError::None};
		for(std::size_t at=0;at<count_;++at)
			if(entries_[at]->loaded && entries_[at]->name==folded)return Copy(*entries_[at],destination,capacity);
		if(count_==Slots)return {0,AnimationCFGError::Full};
		entries_[count_]=std::make_unique<Entry>();
		Entry *entry=entries_[count_++].get();entry->name=folded;
		const std::size_t size=files_.Read(entry->name.c_str(),nullptr,0);
		if(!size)return Discard(AnimationCFGError::None);
		if(size>MaxFileBytes)return Discard(AnimationCFGError::Large);
		if(retained_>budget_ || size+1>budget_-retained_)return Discard(AnimationCFGError::Budget);
		entry->bytes.resize(size+1);retained_+=size+1;
		if(files_.Read(entry->name.c_str(),entry->bytes.data(),size)!=size)return Discard(AnimationCFGError::Changed);
		entry->bytes[size]=0;
		entry->length=0;
		while(std::size_t(entry->length)<size && entry->bytes[std::size_t(entry->length)])++entry->length;
		entry->loaded=true;
		return Copy(*entry,destination,capacity);
	}
private:
	struct Entry { std::string name;std::vector<unsigned char> bytes;int length=0;bool loaded=false; };
	AnimationCFGResult Copy(const Entry &entry,char *destination,int capacity) const noexcept {
		if(destination) {
			const std::size_t room=std::size_t(capacity-1);
			const std::size_t copied=(std::min)(room,std::size_t(entry.length));
			std::memcpy(destination,entry.bytes.data(),copied);
			std::memset(destination+copied,0,std::size_t(capacity)-copied);
		}
		return {entry.length,AnimationCFGError::None};
	}
	AnimationCFGResult Discard(AnimationCFGError error) noexcept {
		retained_-=entries_[count_-1]->bytes.size();entries_[--count_].reset();return {0,error};
	}
	ghoul2::Files &files_;
	std::array<std::unique_ptr<Entry>,Slots> entries_{};
	std::size_t count_=0,retained_=0,budget_;
};
}
#endif
