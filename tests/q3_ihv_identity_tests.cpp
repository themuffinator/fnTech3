/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q3_ihv_install.hpp"
#include "../code/qcommon/game_manager.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace ihv=fntech3::q3ihv::install;
using Bytes=std::vector<std::uint8_t>;
static void Put(Bytes &data,std::size_t at,unsigned count,std::uint32_t value) {
	for(unsigned i=0;i<count;++i) data[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
static Bytes Module(const char *name) {
	// Synthetic PE metadata only; this fixture is never loaded or executed.
	Bytes data(0x400); Put(data,0,2,0x5a4d); Put(data,60,4,0x80);
	Put(data,0x80,4,0x4550); Put(data,0x84,2,0x14c); Put(data,0x86,2,1);
	Put(data,0x94,2,224); Put(data,0x96,2,0x2000); Put(data,0x98,2,0x10b);
	Put(data,0x98+60,4,0x200); Put(data,0x98+92,4,1);
	Put(data,0x98+96,4,0x1100); Put(data,0x98+100,4,0x80);
	Put(data,0x178+12,4,0x1000); Put(data,0x178+16,4,0x200); Put(data,0x178+20,4,0x200);
	Put(data,0x300+20,4,1); Put(data,0x300+24,4,1);
	Put(data,0x300+28,4,0x1140); Put(data,0x300+32,4,0x1130); Put(data,0x300+36,4,0x1138);
	Put(data,0x330,4,0x1150); Put(data,0x340,4,0x1000);
	std::memcpy(data.data()+0x350,name,std::strlen(name)+1); return data;
}
static void Metadata() {
	for(const char *symbol : {"GetGameAPI","GetCGameAPI"}) {
		const auto good=Module(symbol); CHECK(ihv::HasX86Export(good.data(),good.size(),symbol));
		CHECK(!ihv::HasX86Export(nullptr,good.size(),symbol));
		CHECK(!ihv::HasX86Export(good.data(),good.size(),nullptr));
		CHECK(!ihv::HasX86Export(good.data(),good.size(),"vmMain"));
		for(std::size_t size=0;size<good.size();++size) CHECK(!ihv::HasX86Export(good.data(),size,symbol));
		struct Mutation { unsigned at,count,value; };
		for(auto mutation : {Mutation{0,2,0}, {60,4,0xffffffffu}, {0x80,4,0}, {0x84,2,0x8664},
			{0x86,2,0}, {0x86,2,97}, {0x94,2,103}, {0x96,2,0}, {0x98,2,0x20b},
			{0x98+92,4,0}, {0x98+96,4,0xffffffffu}, {0x98+100,4,39},
			{0x178+16,4,0xffffffffu}, {0x178+20,4,0xfffffffeu},
			{0x300+20,4,0}, {0x300+24,4,4097}, {0x300+28,4,0xffffffffu},
			{0x300+32,4,0xffffffffu}, {0x300+36,4,0xffffffffu}, {0x330,4,0xffffffffu},
			{0x338,2,1}, {0x340,4,0}, {0x340,4,0x1110}}) {
			auto bad=good; Put(bad,mutation.at,mutation.count,mutation.value);
			CHECK(!ihv::HasX86Export(bad.data(),bad.size(),symbol));
		}
		auto duplicate=good; Put(duplicate,0x300+24,4,2); Put(duplicate,0x334,4,0x1150);
		CHECK(!ihv::HasX86Export(duplicate.data(),duplicate.size(),symbol));
		Put(duplicate,0x33a,2,1); // A malformed duplicate must not be skipped.
		CHECK(!ihv::HasX86Export(duplicate.data(),duplicate.size(),symbol));
		auto overlap=good; Put(overlap,0x86,2,2); std::memcpy(overlap.data()+0x1a0,overlap.data()+0x178,40);
		CHECK(!ihv::HasX86Export(overlap.data(),overlap.size(),symbol));
		auto longer=good; longer[0x350+std::strlen(symbol)]='x';
		CHECK(!ihv::HasX86Export(longer.data(),longer.size(),symbol));
	}
}
static void Profile() {
	const auto *profile=Com_GameManagerFindProfile("q3-ihv"); CHECK(profile && profile->id==COM_GAME_QUAKE3_IHV);
	CHECK(profile->titleId==COM_GAME_TITLE_QUAKE3 && !profile->basePakName && profile->preferredProtocol==33);
	CHECK(profile->maxClients==256 && profile->maxConfigstrings==1024 && profile->playerConfigstringBase==544);
	CHECK(!profile->usesSteam && !profile->supportsWorkshop && !profile->steamAppId);
	CHECK(Com_GameProfileAllowsModuleApi(profile,COM_MODULE_API_QUAKE3_IHV_NATIVE));
	for(unsigned api : {COM_MODULE_API_QVM,COM_MODULE_API_LEGACY_NATIVE,COM_MODULE_API_STRUCTURED_NATIVE,COM_MODULE_API_STVEF_SP_NATIVE})
		CHECK(!Com_GameProfileAllowsModuleApi(profile,api));
	CHECK(!Com_GameManagerCanActivate(profile)); // Integration remains gated in ordinary builds.
	CHECK(!Com_ActivateGameProfile(COM_GAME_QUAKE3_IHV));
	CHECK(!Com_GameProfileAllowsProtocol(Com_GameProfileById(COM_GAME_QUAKE3),33));
	for(int protocol : {24,26,35,40,43,46,66,67,68,71,91}) CHECK(!Com_GameProfileAllowsProtocol(profile,protocol));
	CHECK(Com_GameProfileAllowsProtocol(profile,33));
	CHECK(Com_GameManagerInstallationMatches(profile,COM_GAME_MARKER_QUAKE3_IHV));
	CHECK(!Com_GameManagerInstallationMatches(profile,COM_GAME_MARKER_QUAKE3_IHV|COM_GAME_MARKER_QUAKE3));
	CHECK(!Com_GameManagerInstallationMatches(Com_GameProfileById(COM_GAME_QUAKE3),COM_GAME_MARKER_QUAKE3_IHV|COM_GAME_MARKER_QUAKE3));
	comGameMarkerInputs_t inputs{"auto",COM_GAME_MARKER_QUAKE3_IHV|COM_GAME_MARKER_QUAKE3,0,0};
	CHECK(Com_DetectGameProfileMarkers(&inputs).status==COM_GAME_DETECTION_AMBIGUOUS);
	char output[64]="retained"; int root=0;
	CHECK(Com_GameManagerNativeModuleCandidate(profile,"qagame","x86",".dll",0,output,sizeof(output),&root));
	CHECK(root==1 && !std::strcmp(output,"qagamex86.dll"));
	CHECK(Com_GameManagerNativeModuleCandidate(profile,"cgame","x86",".dll",0,output,sizeof(output),&root));
	CHECK(root==1 && !std::strcmp(output,"cgamex86.dll"));
	for(const char *module : {"qagame","cgame","ui"}) {
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86_64",".dll",0,output,sizeof(output),&root));
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86",".so",0,output,sizeof(output),&root));
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86",".dll",1,output,sizeof(output),&root));
	}
	CHECK(!Com_GameManagerNativeModuleCandidate(profile,"ui","x86",".dll",0,output,sizeof(output),&root));
	CHECK(Com_GameManagerBspFormat(profile,43)==COM_GAME_BSP_QUAKE3_IHV);
	for(int version : {44,45,46,47}) CHECK(Com_GameManagerBspFormat(profile,version)==COM_GAME_BSP_UNSUPPORTED);
}
int main(int argc,char **argv) {
	Metadata(); Profile();
	if(argc>1) {
		CHECK(ihv::Recognizes(std::filesystem::u8path(argv[1])));
		CHECK(Com_ProbeGameProfileMarkers(argv[1])==COM_GAME_MARKER_QUAKE3_IHV);
		std::puts("Supplied IHV installation recognized without loading DLLs");
	}
	std::puts("IHV native metadata, profile separation and malformed-input tests passed");
}
