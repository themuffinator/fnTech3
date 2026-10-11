/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q3test_install.hpp"
#include "../code/qcommon/game_manager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace q3t=fntech3::q3test::install;
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;

static void Put(Bytes &data,std::size_t at,unsigned count,std::uint32_t value) {
	for(unsigned i=0;i<count;++i) data[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
/* Synthetic PE metadata only; these fixtures are never loaded or executed.
 * One section maps RVA 0x1000 at file offset 0x200, and the export
 * directory lists the given names, each resolving to a distinct function. */
static Bytes Module(std::uint32_t stamp,std::initializer_list<const char *> names) {
	Bytes data(0x600); Put(data,0,2,0x5a4d); Put(data,60,4,0x80);
	Put(data,0x80,4,0x4550); Put(data,0x84,2,0x14c); Put(data,0x86,2,1); Put(data,0x88,4,stamp);
	Put(data,0x94,2,224); Put(data,0x96,2,0x2000); Put(data,0x98,2,0x10b);
	Put(data,0x98+60,4,0x200); Put(data,0x98+92,4,1);
	Put(data,0x98+96,4,0x1100); Put(data,0x98+100,4,0x100);
	Put(data,0x178+12,4,0x1000); Put(data,0x178+16,4,0x400); Put(data,0x178+20,4,0x200);
	const unsigned count=static_cast<unsigned>(names.size());
	// Export directory at RVA 0x1100 (file 0x300): functions, names, ordinals.
	Put(data,0x300+20,4,count); Put(data,0x300+24,4,count);
	Put(data,0x300+28,4,0x1140); Put(data,0x300+32,4,0x1160); Put(data,0x300+36,4,0x1180);
	unsigned i=0,text=0x1200;
	for(const char *name : names) {
		Put(data,0x340+i*4,4,0x1010+i*4);        // function RVA inside the section
		Put(data,0x360+i*4,4,text);              // name RVA
		Put(data,0x380+i*2,2,i);                 // ordinal index
		std::memcpy(data.data()+0x200+(text-0x1000),name,std::strlen(name)+1);
		text+=static_cast<unsigned>(std::strlen(name))+1; ++i;
	}
	return data;
}
struct Member { const char *name; std::uint32_t size,crc; };
/* A central directory and end record only; the recognizer never reads the
 * members themselves. */
static Bytes Pak(std::initializer_list<Member> members) {
	Bytes directory;
	for(const auto &member : members) {
		Bytes record(46+std::strlen(member.name));
		Put(record,0,4,0x02014b50u); Put(record,16,4,member.crc); Put(record,20,4,member.size);
		Put(record,24,4,member.size); Put(record,28,2,static_cast<unsigned>(std::strlen(member.name)));
		std::memcpy(record.data()+46,member.name,std::strlen(member.name));
		directory.insert(directory.end(),record.begin(),record.end());
	}
	Bytes data(64,0); const auto offset=static_cast<std::uint32_t>(data.size());
	data.insert(data.end(),directory.begin(),directory.end());
	Bytes end(22); Put(end,0,4,0x06054b50u);
	Put(end,8,2,static_cast<unsigned>(members.size())); Put(end,10,2,static_cast<unsigned>(members.size()));
	Put(end,12,4,static_cast<std::uint32_t>(directory.size())); Put(end,16,4,offset);
	data.insert(data.end(),end.begin(),end.end());
	return data;
}
static void Write(const fs::path &path,const Bytes &data) {
	fs::create_directories(path.parent_path());
	std::ofstream file(path,std::ios::binary); CHECK(file);
	file.write(reinterpret_cast<const char *>(data.data()),static_cast<std::streamsize>(data.size())); CHECK(file);
}
static fs::path Install(const fs::path &root,const q3testRelease_t &release) {
	fs::remove_all(root);
	Write(root/"qagamex86.dll",release.botAI ? Module(release.gameStamp,{"GetBotAIAPI","GetGameAPI"})
		: Module(release.gameStamp,{"GetGameAPI"}));
	if(release.cgameEntry==Q3TEST_CGAME_TABLE) Write(root/"cgamex86.dll",Module(release.cgameStamp,{"GetCGameAPI"}));
	if(release.cgameEntry==Q3TEST_CGAME_SYSCALLS) Write(root/"cgamex86.dll",Module(release.cgameStamp,{"dllEntry","vmMain"}));
	if(release.uiStamp) Write(root/"uix86.dll",Module(release.uiStamp,{"GetUIAPI"}));
	const bool capitals=release.bspVersion==44; // The installers wrote Q3TEST1 up to 1.05.
	Write(root/"demoq3"/"pak0.pk3",Pak({{"default.cfg",q3t::defaultConfigSize,q3t::defaultConfigCrc},
		{capitals ? "maps/Q3TEST1.bsp" : "maps/q3test1.bsp",release.mapSize,release.mapCrc},
		{"scripts/arenas.txt",747,0x12345678u}}));
	return root;
}
static void Metadata() {
	const auto module=Module(0x37a67322u,{"GetBotAIAPI","GetGameAPI"});
	CHECK(q3t::Exports(module,"GetGameAPI") && q3t::Exports(module,"GetBotAIAPI") && !q3t::Exports(module,"vmMain"));
	CHECK(q3t::Stamp(module)==0x37a67322u);
	CHECK(q3t::EqualsFolded("MAPS/Q3Test1.BSP","maps/q3test1.bsp") && !q3t::EqualsFolded("maps/q3test1.bsp2","maps/q3test1.bsp"));
}
static void Releases(const fs::path &scratch) {
	unsigned seen=0;
	for(const auto &release : q3t::releases) {
		CHECK(Com_Q3TestReleaseById(release.id)==&release);
		CHECK(release.protocol>=35 && release.protocol<=40 && (release.bspVersion==44 || release.bspVersion==45));
		const auto root=Install(scratch/release.version,release);
		CHECK(q3t::Recognize(root)==release.id);
		CHECK(Com_Q3TestDetectRelease(root.u8string().c_str())==release.id);
		CHECK(Com_ProbeGameProfileMarkers(root.u8string().c_str())==COM_GAME_MARKER_Q3TEST);
		seen|=1u<<release.id;
		// Every piece of evidence is necessary.
		fs::remove(root/"demoq3"/"pak0.pk3"); CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		Install(root,release); fs::remove(root/"qagamex86.dll"); CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		// A game library from no known build, or the bot entry point missing or added.
		Install(root,release); Write(root/"qagamex86.dll",Module(release.gameStamp+1,{"GetGameAPI"}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		Install(root,release); Write(root/"qagamex86.dll",release.botAI ? Module(release.gameStamp,{"GetGameAPI"})
			: Module(release.gameStamp,{"GetBotAIAPI","GetGameAPI"}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		// Another release's pak under this release's modules.
		Install(root,release);
		const q3testRelease_t &other=release.id==Q3TEST_RELEASE_1_08 ? q3t::releases[0] : q3t::releases[5];
		if(other.mapCrc!=release.mapCrc) {
			Write(root/"demoq3"/"pak0.pk3",Pak({{"default.cfg",q3t::defaultConfigSize,q3t::defaultConfigCrc},
				{"maps/q3test1.bsp",other.mapSize,other.mapCrc}}));
			CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		}
		// A modified default.cfg, a duplicated map entry and a missing map.
		Write(root/"demoq3"/"pak0.pk3",Pak({{"default.cfg",q3t::defaultConfigSize,q3t::defaultConfigCrc^1u},
			{"maps/q3test1.bsp",release.mapSize,release.mapCrc}}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		Write(root/"demoq3"/"pak0.pk3",Pak({{"default.cfg",q3t::defaultConfigSize,q3t::defaultConfigCrc},
			{"maps/q3test1.bsp",release.mapSize,release.mapCrc},{"MAPS/Q3TEST1.BSP",release.mapSize,release.mapCrc}}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		Write(root/"demoq3"/"pak0.pk3",Pak({{"default.cfg",q3t::defaultConfigSize,q3t::defaultConfigCrc}}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		// Client modules from the wrong build, or of the wrong kind, make the installation mixed.
		Install(root,release);
		Write(root/"cgamex86.dll",Module(0x12345678u,{"GetCGameAPI"})); CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
		if(release.cgameStamp) {
			Write(root/"cgamex86.dll",release.cgameEntry==Q3TEST_CGAME_TABLE ? Module(release.cgameStamp,{"dllEntry","vmMain"})
				: Module(release.cgameStamp,{"GetCGameAPI"}));
			CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
			// A dedicated-server layout without the client modules is still this release.
			fs::remove(root/"cgamex86.dll"); fs::remove(root/"uix86.dll"); CHECK(q3t::Recognize(root)==release.id);
		}
		Install(root,release);
		Write(root/"uix86.dll",Module(release.uiStamp ? release.uiStamp+1 : 0x378fba4du,{"GetUIAPI"}));
		CHECK(q3t::Recognize(root)==Q3TEST_RELEASE_NONE);
	}
	CHECK(seen==((1u<<Q3TEST_RELEASE_COUNT)-2u));
	CHECK(q3t::Recognize(scratch/"missing")==Q3TEST_RELEASE_NONE);
	CHECK(Com_Q3TestDetectRelease(nullptr)==Q3TEST_RELEASE_NONE && Com_Q3TestDetectRelease("")==Q3TEST_RELEASE_NONE);
}
static void Profile() {
	const auto *profile=Com_GameManagerFindProfile("q3test"); CHECK(profile && profile->id==COM_GAME_Q3TEST);
	CHECK(profile==Com_GameManagerFindProfile("Q3T") && profile==Com_GameManagerFindProfile("quake3-test"));
	CHECK(profile->titleId==COM_GAME_TITLE_QUAKE3 && !profile->basePakName && !std::strcmp(profile->baseGame,"demoq3"));
	CHECK(!profile->usesSteam && !profile->supportsWorkshop && !profile->steamAppId);
	CHECK(profile->pureProof==COM_PURE_PROOF_UNAVAILABLE);
	CHECK(Com_GameProfileAllowsModuleApi(profile,COM_MODULE_API_Q3TEST_NATIVE));
	for(unsigned api : {COM_MODULE_API_QVM,COM_MODULE_API_LEGACY_NATIVE,COM_MODULE_API_STRUCTURED_NATIVE,
		COM_MODULE_API_QUAKE3_IHV_NATIVE,COM_MODULE_API_STVEF_SP_NATIVE}) CHECK(!Com_GameProfileAllowsModuleApi(profile,api));
	for(int id=COM_GAME_AUTO+1;id<COM_GAME_COUNT;++id) {
		const auto *other=Com_GameProfileById(static_cast<comGameId_t>(id));
		if(other!=profile) CHECK(!Com_GameProfileAllowsModuleApi(other,COM_MODULE_API_Q3TEST_NATIVE));
		// No other profile admits a Q3Test-only protocol.
		for(int protocol : {35,36,38,39}) CHECK(other==profile || !Com_GameProfileAllowsProtocol(other,protocol));
	}
	for(int protocol : {35,36,38,39,40}) CHECK(Com_GameProfileAllowsProtocol(profile,protocol));
	for(int protocol : {33,37,41,43,46,66,68,71}) CHECK(!Com_GameProfileAllowsProtocol(profile,protocol));
	// Protocol 40 still belongs to the three single players as well.
	CHECK(Com_GameProfileAllowsProtocol(Com_GameProfileById(COM_GAME_STVEF_SP),40));
	CHECK(!Com_GameProfileAllowsProtocol(Com_GameProfileById(COM_GAME_QUAKE3),40));
	CHECK(Com_GameManagerCanActivate(profile)); // Available in every build.
	CHECK(!Com_Q3TestRelease() && !Com_Q3TestActivateRelease(Q3TEST_RELEASE_1_08)); // Only under the active profile.
	CHECK(Com_GameManagerInstallationMatches(profile,COM_GAME_MARKER_Q3TEST));
	CHECK(!Com_GameManagerInstallationMatches(profile,COM_GAME_MARKER_Q3TEST|COM_GAME_MARKER_QUAKE3));
	CHECK(!Com_GameManagerInstallationMatches(Com_GameProfileById(COM_GAME_QUAKE3),COM_GAME_MARKER_Q3TEST));
	comGameMarkerInputs_t inputs{"auto",COM_GAME_MARKER_Q3TEST|COM_GAME_MARKER_QUAKE3,0,0};
	CHECK(Com_DetectGameProfileMarkers(&inputs).status==COM_GAME_DETECTION_AMBIGUOUS);
	char output[64]="retained"; int root=0;
	const char *leaves[3]={"qagamex86.dll","cgamex86.dll","uix86.dll"}; unsigned index=0;
	for(const char *module : {"qagame","cgame","ui"}) {
		root=0; CHECK(Com_GameManagerNativeModuleCandidate(profile,module,"x86",".dll",0,output,sizeof(output),&root));
		CHECK(root==1 && !std::strcmp(output,leaves[index++]));
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86_64",".dll",0,output,sizeof(output),&root));
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86",".so",0,output,sizeof(output),&root));
		CHECK(!Com_GameManagerNativeModuleCandidate(profile,module,"x86",".dll",1,output,sizeof(output),&root));
	}
	for(int version : {44,45}) CHECK(Com_GameManagerBspFormat(profile,version)==COM_GAME_BSP_Q3TEST);
	for(int version : {43,46,47}) CHECK(Com_GameManagerBspFormat(profile,version)==COM_GAME_BSP_UNSUPPORTED);
	for(int id=COM_GAME_AUTO+1;id<COM_GAME_COUNT;++id) {
		const auto *other=Com_GameProfileById(static_cast<comGameId_t>(id));
		if(other==profile) continue;
		for(int version : {44,45}) CHECK(Com_GameManagerBspFormat(other,version)!=COM_GAME_BSP_Q3TEST);
	}
	CHECK(Com_ActivateGameProfile(COM_GAME_Q3TEST));
	CHECK(Com_Q3TestActivateRelease(Q3TEST_RELEASE_1_08));
	CHECK(!Com_Q3TestActivateRelease(Q3TEST_RELEASE_1_06));
}
int main(int argc,char **argv) {
	const auto scratch=fs::current_path()/".tmp"/"q3test-identity-tests";
	fs::create_directories(scratch);
	Metadata(); Releases(scratch); Profile();
	fs::remove_all(scratch);
	for(int i=1;i<argc;++i) {
		const auto release=q3t::Recognize(fs::u8path(argv[i]));
		CHECK(release!=Q3TEST_RELEASE_NONE);
		std::printf("%s: Q3Test %s recognized without loading a module\n",argv[i],Com_Q3TestReleaseById(release)->version);
	}
	std::puts("Q3Test release identity, profile separation and malformed-installation tests passed");
}
