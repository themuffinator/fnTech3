/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_animation_cfg.hpp"
#include <cassert>
#include <map>
#include <stdexcept>
using namespace fntech3::jka::sp;
class Files final : public fntech3::ghoul2::Files {
public:
	std::map<std::string,std::vector<unsigned char>> values;
	int reads=0,failAt=0;bool changed=false,large=false;
	std::size_t Read(const char *name,void *destination,std::size_t capacity) override {
		if(++reads==failAt)throw std::runtime_error("read drop");
		if(large)return AnimationCFGCache::MaxFileBytes+1;
		const auto found=values.find(name);if(found==values.end())return 0;
		const auto size=found->second.size();
		if(destination && changed)return size-1;
		if(destination && capacity>=size)std::memcpy(destination,found->second.data(),size);
		return size;
	}
};
int main() {
	Files files;files.values["animation.cfg"]={'a','b','c','d'};
	AnimationCFGCache cache(files);
	assert(cache.Get("ANIMATION.CFG",nullptr,-1).length==4 && files.reads==2 && cache.Count()==1);
	assert(cache.RetainedBytes()==5);
	files.values["animation.cfg"]={'x'};
	unsigned char guarded[10];std::memset(guarded,0xa5,sizeof(guarded));
	assert(cache.Get("animation.cfg",reinterpret_cast<char *>(guarded+1),8).length==4);
	assert(guarded[0]==0xa5 && guarded[9]==0xa5 && !std::memcmp(guarded+1,"abcd\0\0\0\0",8));
	assert(files.reads==2);
	assert(cache.Get("animation.cfg",reinterpret_cast<char *>(guarded+1),3).length==4);
	assert(!std::memcmp(guarded+1,"ab\0",3) && guarded[4]=='d');
	assert(cache.Get("animation.cfg",reinterpret_cast<char *>(guarded+1),1).length==4 && guarded[1]==0);
	for(int capacity:{0,-1})assert(cache.Get("animation.cfg",reinterpret_cast<char *>(guarded),capacity).error==AnimationCFGError::Capacity);
	assert(cache.Get(nullptr,nullptr,0).error==AnimationCFGError::Name);
	assert(cache.Get("",nullptr,0).length==0 && files.reads==2);
	for(const char *name:{"../animation.cfg","dir/../animation.cfg","/animation.cfg","\\animation.cfg","C:/animation.cfg"})
		assert(cache.Get(name,nullptr,0).error==AnimationCFGError::Name);
	assert(cache.Get(std::string(64,'x').c_str(),nullptr,0).error==AnimationCFGError::Name);
	assert(cache.Get(std::string(63,'x').c_str(),nullptr,0).error==AnimationCFGError::None);
	const int before=files.reads;
	assert(cache.Get("missing.cfg",nullptr,0).length==0 && cache.Count()==1);
	assert(cache.Get("MISSING.CFG",nullptr,0).length==0 && files.reads==before+2);
	files.values["missing.cfg"]={'n'};
	assert(cache.Get("missing.cfg",nullptr,0).length==1 && cache.Count()==2);
	files.values["nul.cfg"]={'x',0,'y'};
	assert(cache.Get("nul.cfg",reinterpret_cast<char *>(guarded+1),3).length==1 && guarded[1]=='x' && guarded[2]==0 && guarded[3]==0);
	files.values["empty.cfg"]={};assert(cache.Get("empty.cfg",nullptr,0).length==0 && cache.Count()==3);
	files.values["nul-first.cfg"]={0,'x'};
	assert(cache.Get("nul-first.cfg",nullptr,0).length==0 && cache.Count()==4);
	cache.Clear();assert(cache.Count()==0 && cache.RetainedBytes()==0);
	assert(cache.Get("animation.cfg",nullptr,0).length==1);
	AnimationCFGCache small(files,1);
	assert(small.Get("animation.cfg",nullptr,0).error==AnimationCFGError::Budget && small.Count()==0);
	files.large=true;assert(small.Get("large.cfg",nullptr,0).error==AnimationCFGError::Large && small.Count()==0);files.large=false;
	cache.Clear();files.changed=true;
	assert(cache.Get("animation.cfg",nullptr,0).error==AnimationCFGError::Changed && cache.Count()==0 && cache.RetainedBytes()==0);files.changed=false;
	files.failAt=files.reads+2;
	try { cache.Get("animation.cfg",nullptr,0);assert(false); }catch(const std::runtime_error &) {}
	assert(cache.Count()==1 && cache.RetainedBytes()==2);cache.Clear();files.failAt=0;
	for(std::size_t index=0;index<AnimationCFGCache::Slots;++index) {
		const auto name=std::to_string(index)+".cfg";files.values[name]={'t'};
		assert(cache.Get(name.c_str(),nullptr,0).length==1);
	}
	assert(cache.Get("another.cfg",nullptr,0).error==AnimationCFGError::Full);
	assert(cache.Get("0.CFG",nullptr,0).length==1);
	cache.Clear();assert(cache.Get("another.cfg",nullptr,0).length==0 && cache.Count()==0);
}
