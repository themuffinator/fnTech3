/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_ghoul2.hpp"
#include "jka_sp_model_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
struct Drop { const char *text; };
static void Error(const char *text) { throw Drop{text}; }
template<class F> static void Reject(F action,const char *text) {
	bool rejected=false;
	try { action(); } catch(const Drop &drop) { CHECK(std::strstr(drop.text,text));rejected=true; }
	CHECK(rejected);
}
static void Metadata() {
	namespace sp=fntech3::jka::sp;
	jka_sp_test::Files files;
	files.files["models/test/animation.gla"]=jka_sp_test::Skeleton();
	files.files["models/test/model.glm"]=jka_sp_test::Mesh();
	files.files["models/test/second.glm"]=jka_sp_test::Mesh();
	files.files["malformed.glm"]={1,2,3};
	files.files["unmatched.glm"]=jka_sp_test::Mesh("missing-animation");
	files.files["mismatch.glm"]=jka_sp_test::Mesh();jka_sp_test::Word(files.files["mismatch.glm"],140,2);
	sp::ModelPrecache models(files);sp::Ghoul2Array manager(Error);
	auto *object=manager.Interface();const auto *table=object->vtable;
	fntech3_jka_sp_ghoul2_handle_t handle{};
	Reject([&] { manager.Init(models,nullptr,"model",0,0,0,0,0); },"null handle");
	CHECK(manager.Init(models,&handle,nullptr,0,0,0,0,0)==-1 && !handle.handle);
	if constexpr(sizeof(uintptr_t)!=sizeof(uint32_t)) {
		Reject([&] { manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0); },"32-bit native boundary");
		CHECK(!handle.handle && !files.reads);return;
	}
	for(const char *name:{"missing.glm","malformed.glm","unmatched.glm","mismatch.glm","models/test/animation.gla"})
		CHECK(manager.Init(models,&handle,name,0,0,0,0,0)==-1 && !handle.handle);
	Reject([&] { manager.Init(models,&handle,"models/test.md3",0,0,0,0,0); },"supported GLM/GLA");
	Reject([&] { manager.Init(models,&handle,"*unserved.gla",0,0,0,0,0); },"supported GLM/GLA");
	CHECK(manager.Init(models,&handle,"MODELS/TEST/MODEL.GLM",99,11,22,77,3)==0);
	CHECK(table->IsValid(object,handle.handle));
	const int first=handle.handle;
	auto *header=table->Get(object,first);
	CHECK(header->end_address-header->begin_address==204);
	CHECK(header->capacity_address-header->begin_address==204*sp::Ghoul2Array::MaxModels);
	auto *records=reinterpret_cast<fntech3_jka_sp_ghoul2_info_t *>(uintptr_t(header->begin_address));
	CHECK(records[0].modelindex==0 && records[0].customSkin==11 && records[0].customShader==22 && records[0].lodBias==3);
	CHECK(records[0].flags==0 && records[0].modelBoltLink==-1 && records[0].newOrigin==-1);
	CHECK(records[0].skelFrameNum==-1 && records[0].meshFrameNum==-1 && records[0].animFrameDefault==0);
	CHECK(!std::strcmp(records[0].fileName,"MODELS/TEST/MODEL.GLM"));
	CHECK(!records[0].valid && !records[0].currentModel_address && !records[0].animModel_address && !records[0].animHeader_address);
	CHECK(!records[0].surfaces.begin_address && !records[0].bolts.begin_address && !records[0].bones.capacity_address);
	const auto *entry=models.Resolve(records[0].model);CHECK(entry && models.Resolve(entry->animationHandle));
	/* Actual virtual slot4 supports ordinary module scalar writes. Growth is
	 * engine-only and stable, so previously returned element addresses survive. */
	decltype(table->Get) mutableGet{};
	std::memcpy(&mutableGet,reinterpret_cast<const unsigned char *>(table)+4*sizeof(void *),sizeof(mutableGet));
	CHECK(mutableGet(object,first)==header);records[0].animModelIndexOffset=19;
	CHECK(manager.Init(models,&handle,"models/test/second.glm",123,33,44,55,6)==1);
	CHECK(table->GetConst(object,first)->begin_address==header->begin_address && records[0].animModelIndexOffset==19);
	CHECK(records[1].modelindex==1 && records[1].customSkin==33 && records[1].customShader==44 && records[1].lodBias==6);
	const auto before=records[1];
	CHECK(manager.Init(models,&handle,"missing.glm",0,0,0,0,0)==-1 && !std::memcmp(&before,&records[1],sizeof(before)));
	records[0].modelindex=-1;
	CHECK(manager.Init(models,&handle,"models/test/model.glm",0,8,9,0,10)==0);
	CHECK(records[0].animModelIndexOffset==0 && records[0].customSkin==8 && records[1].customSkin==33);
	records[0].modelindex=-1;
	CHECK(manager.Init(models,&handle,records[0].fileName,0,8,9,0,10)==0);
	CHECK(!std::strcmp(records[0].fileName,"models/test/model.glm") && records[0].customSkin==8);
	for(std::size_t i=2;i<sp::Ghoul2Array::MaxModels;++i)
		CHECK(manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0)==int(i));
	CHECK(header->end_address==header->capacity_address);
	Reject([&] { manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0); },"model capacity exhausted");
	/* Reject all carrier modifications without following their addresses.
	 * Global cleanup always uses canonical ownership, even after tampering. */
	const auto original=*header;
	for(int field=0;field<4;++field) {
		reinterpret_cast<uint32_t *>(header)[field]=4;
		Reject([&] { table->GetConst(object,first); },"vector header was modified");
		Reject([&] { manager.Init(models,&handle,"missing.glm",0,0,0,0,0); },"vector header was modified");
		*header=original;
	}
	records[0].bones.begin_address=4;
	Reject([&] { table->Delete(object,first); },"nested vector resize");
	records[0].bones.begin_address=0;records[0].currentModel_address=4;
	Reject([&] { table->Get(object,first); },"private model/pose pointers");
	records[0].currentModel_address=0;manager.Clean(&handle);CHECK(!handle.handle && !table->IsValid(object,first));
	fntech3_jka_sp_ghoul2_handle_t stale{first};
	Reject([&] { manager.Init(models,&stale,"models/test/model.glm",0,0,0,0,0); },"invalid or stale");
	CHECK(manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0)==0 && handle.handle!=first);
	header=table->Get(object,handle.handle);
	records=reinterpret_cast<fntech3_jka_sp_ghoul2_info_t *>(uintptr_t(header->begin_address));
	records[0].surfaces.begin_address=4;records[0].bones.capacity_address=8;
	header->begin_address=4;header->capacity_address=8;
	manager.Close();manager.Close();CHECK(!table->IsValid(object,handle.handle));
	/* DLL_PROCESS_DETACH may still release acquired handles, repeatedly.
	 * No guest headers are read or passed to any foreign allocator. */
	table->Delete(object,handle.handle);table->Delete(object,handle.handle);
	Reject([&] { table->Delete(object,first); },"invalid or stale");
	Reject([&] { table->Delete(object,513); },"invalid or stale");
	Reject([&] { table->New(object); },"closed for module teardown");
	Reject([&] { table->GetConst(object,handle.handle); },"closed for module teardown");
	Reject([&] { manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0); },"closed for module teardown");
	manager.Reset();CHECK(!table->IsValid(object,handle.handle));
	handle.handle=0;CHECK(manager.Init(models,&handle,"models/test/model.glm",0,0,0,0,0)==0);
	manager.Clean(&handle);models.Clear();
}
int main() {
	static_assert(std::is_standard_layout_v<fntech3_jka_sp_ghoul2_info_t>);
	static_assert(offsetof(fntech3_jka_sp_ghoul2_array_vtable_t,GetConst)==3*sizeof(void *));
	static_assert(offsetof(fntech3_jka_sp_ghoul2_array_vtable_t,Get)==4*sizeof(void *));
	fntech3::jka::sp::EmptyGhoul2Array manager(Error);
	auto *object=manager.Interface(); const auto *table=object->vtable;
	CHECK(table && !table->IsValid(object,0) && !table->IsValid(object,-1));
	CHECK(!table->IsValid(object,512));
	Reject([&] { table->GetConst(object,512); },"invalid or stale");
	table->Delete(object,0);
	const int first=table->New(object); CHECK(first==512 && table->IsValid(object,first));
	const auto *empty=table->GetConst(object,first);
	CHECK(empty && empty->begin_address==0 && empty->end_address==0 && empty->capacity_address==0);
	Reject([&] { table->Get(object,first); },"mutable Get (virtual slot 4)");
	/* Reproduce retail resize's raw +16 call independently of member names.
	 * Declaration-order tests alone would miss MSVC's overloaded-slot order. */
	decltype(table->Get) retailMutable{};
	std::memcpy(&retailMutable,reinterpret_cast<const unsigned char *>(table)+4*sizeof(void *),sizeof(retailMutable));
	Reject([&] { retailMutable(object,first); },"mutable Get (virtual slot 4)");
	CHECK(table->GetConst(object,first)==empty && table->IsValid(object,first));
	table->Delete(object,first); CHECK(!table->IsValid(object,first));
	Reject([&] { table->Delete(object,first); },"invalid or stale");
	const int recycled=table->New(object); CHECK(recycled==1024 && table->IsValid(object,recycled));
	CHECK(!table->IsValid(object,first));
	fntech3_jka_sp_ghoul2_handle_t handle{recycled}; manager.Clean(&handle); CHECK(handle.handle==0);
	manager.Clean(&handle); Reject([&] { manager.Clean(nullptr); },"null handle");
	manager.Reset(); std::array<int32_t,512> handles{};
	for(int i=0;i<512;++i) { handles[i]=table->New(object); CHECK(handles[i]==512+i); }
	Reject([&] { table->New(object); },"capacity exhausted");
	for(int i=0;i<512;++i) CHECK(table->IsValid(object,handles[i]));
	for(int i=0;i<512;++i) table->Delete(object,handles[i]);
	for(int i=0;i<512;++i) CHECK(!table->IsValid(object,handles[i]));
	for(int i=0;i<512;++i) CHECK(table->New(object)==1535-i);
	manager.Reset(); CHECK(manager.Interface()==object && object->vtable==table);
	CHECK(!table->IsValid(object,1535));
	const int corrupt=table->New(object);
	auto *vector=const_cast<fntech3_jka_sp_ghoul2_vector_t *>(table->GetConst(object,corrupt));
	vector->begin_address=4;
	Reject([&] { table->Delete(object,corrupt); },"vector header was modified");
	CHECK(table->IsValid(object,corrupt));
	vector->begin_address=0; table->Delete(object,corrupt);
	Metadata();std::puts("Jedi Academy SP Ghoul2 owned metadata and handle ABI tests passed");
}
