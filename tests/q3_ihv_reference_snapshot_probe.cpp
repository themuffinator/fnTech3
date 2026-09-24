/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 comparison of fnTech3's native IHV snapshot selection against
 * the original SV_BuildClientSnapshot and its visibility walk. The collision
 * model is replaced by synthetic replies that both implementations receive, so
 * this establishes the selection rules, not original BSP behaviour. No original
 * entry point, filesystem, display, input, audio or network service starts, and
 * no original module or asset is incorporated. The host base is 0x400000 so the
 * harness owns the original's address range as zeroed storage. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_ihv_snapshot_select.hpp"
#include "q3_ihv_collision_cases.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }

namespace {
/* Original storage this probe drives directly. */
constexpr unsigned SvEntities=0x425729c,SvEntityStride=0x100;
constexpr unsigned LinkNumClusters=0xac,LinkClusterNums=0xb0,LinkHeadnode=0xf0;
constexpr unsigned LinkArea=0xf4,LinkArea2=0xf8,LinkCounter=0xfc;
constexpr unsigned ClientStride=0xee18,SequenceOffset=0x2e18,FramesOffset=0x458,FrameStride=0x14c;
constexpr unsigned FrameAreaBytes=0,FrameAreaBits=4,FramePlayerState=0x24;
constexpr unsigned FrameCount=0x138,FrameFirst=0x13c;
constexpr unsigned EntityStride=556,StateBytes=FNTECH3_Q3_IHV_ENTITY_STATE_BYTES;
constexpr unsigned PlayerStateBytes=FNTECH3_Q3_IHV_PLAYER_STATE_BYTES;
constexpr int Capacity=256,MaxEntities=16,AreaBits=FNTECH3_Q3_IHV_AREABITS_BYTES;

/* Synthetic collision replies, shared by both implementations. */
int leafArea[8],leafCluster[8],leafCount=1;
unsigned char pvs[AreaBits];
std::vector<int> visibleNodes;
int areaBitsResult=1,originalRepairs,replicaRepairs,engineErrors;

int __cdecl StubPointLeafnum(const float *point) {
	const int leaf=static_cast<int>(point[0]);
	return leaf>=0 && leaf<leafCount?leaf:0;
}
int __cdecl StubLeafArea(int leaf){return leaf>=0 && leaf<leafCount?leafArea[leaf]:-1;}
int __cdecl StubLeafCluster(int leaf){return leaf>=0 && leaf<leafCount?leafCluster[leaf]:-1;}
int __cdecl StubWriteAreaBits(unsigned char *buffer,int area) {
	std::memset(buffer,0,AreaBits);
	if(area>=0 && area<AreaBits*8)buffer[area>>3]=static_cast<unsigned char>(1<<(area&7));
	return areaBitsResult;
}
unsigned char * __cdecl StubClusterPVS(int){return pvs;}
int __cdecl StubAreasConnected(int a,int b){return a>=0 && b>=0 && a==b;}
int __cdecl StubHeadnodeVisible(int node,const unsigned char *bits) {
	Check(bits==pvs,"Headnode visibility vector");
	return std::find(visibleNodes.begin(),visibleNodes.end(),node)!=visibleNodes.end();
}
void __cdecl StubPrintf(const char *){++originalRepairs;}
void __cdecl StubError(int,const char *){++engineErrors;}

void PatchJump(unsigned address,const void *target) {
	auto *code=reinterpret_cast<unsigned char *>(address);
	code[0]=0xe9;
	const unsigned displacement=reinterpret_cast<unsigned>(target)-(address+5);
	std::memcpy(code+1,&displacement,4);
}

unsigned char *LinkRecord(int number) {
	return reinterpret_cast<unsigned char *>(SvEntities+static_cast<unsigned>(number)*SvEntityStride);
}
template<typename T> void Put(void *base,unsigned offset,T value) {
	std::memcpy(static_cast<char *>(base)+offset,&value,sizeof value);
}
template<typename T> T Get(const void *base,unsigned offset) {
	T value; std::memcpy(&value,static_cast<const char *>(base)+offset,sizeof value); return value;
}

/* fnTech3's side of the comparison: the same walk the dedicated server runs,
 * reading the same link records through its host callbacks. */
struct Replica {
	std::vector<int> mark=std::vector<int>(MaxEntities+1,0);
	int counter=0;
	unsigned char *entities=nullptr;
};
int HostPointLeafnum(void *,const float *point){return StubPointLeafnum(point);}
int HostLeafArea(void *,int leaf){return StubLeafArea(leaf);}
int HostLeafCluster(void *,int leaf){return StubLeafCluster(leaf);}
int HostWriteAreaBits(void *,unsigned char *buffer,int area){return StubWriteAreaBits(buffer,area);}
const unsigned char *HostClusterPVS(void *,int cluster){return StubClusterPVS(cluster);}
bool HostAreasConnected(void *,int a,int b){return StubAreasConnected(a,b)!=0;}
bool HostHeadnodeVisible(void *,int node,const unsigned char *bits){return StubHeadnodeVisible(node,bits)!=0;}
fntech3_q3_ihv_entity_t *HostEntityAt(void *context,int number) {
	auto *replica=static_cast<Replica *>(context);
	if(number<0 || number>MaxEntities)return nullptr;
	return reinterpret_cast<fntech3_q3_ihv_entity_t *>(replica->entities+static_cast<unsigned>(number)*EntityStride);
}
bool HostReadLink(void *,int number,fntech3::q3ihv::SnapshotLink &link) {
	if(number<0 || number>MaxEntities)return false;
	const auto *record=LinkRecord(number);
	link.numClusters=Get<int>(record,LinkNumClusters);
	std::memcpy(link.clusternums,record+LinkClusterNums,sizeof link.clusternums);
	link.headnode=Get<int>(record,LinkHeadnode);
	link.areanum=Get<int>(record,LinkArea);
	link.areanum2=Get<int>(record,LinkArea2);
	return true;
}
bool HostAdded(void *context,int number) {
	auto *replica=static_cast<Replica *>(context);
	return number>=0 && number<=MaxEntities && replica->mark[static_cast<std::size_t>(number)]==replica->counter;
}
void HostMarkAdded(void *context,int number) {
	auto *replica=static_cast<Replica *>(context);
	if(number>=0 && number<=MaxEntities)replica->mark[static_cast<std::size_t>(number)]=replica->counter;
}
void HostRepairedNumber(void *,int,int){++replicaRepairs;}

unsigned seed=0x13579bdfu;
unsigned Next(){seed=seed*1664525u+1013904223u;return seed>>8;}
} // namespace

static void LoadReference(const char *path) {
	std::ifstream input(path,std::ios::binary); Check(bool(input),"Open original executable");
	std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
	HCRYPTPROV provider{}; HCRYPTHASH hash{};
	Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
	if (!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) { CryptReleaseContext(provider,0); Check(false,"Hash create"); }
	BYTE digest[32]{}; DWORD count=32;
	const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE *>(data.data()),static_cast<DWORD>(data.size()),0)
		&& CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0);
	CryptDestroyHash(hash); CryptReleaseContext(provider,0); Check(ok && count==32,"SHA-256");
	char text[65]{}; for (int i=0;i<32;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
	Check(!std::strcmp(text,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37"),"Unexpected original executable");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x400000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	/* Code: the entity lookups, the snapshot builder with its walk and sort
	 * comparator, and the shared runtime helpers. Read-only and initialized
	 * data follow, writable because the original's variables live there. */
	for (auto range : {std::array<unsigned,3>{0x417000,0x3000,1},{0x41a000,0x2000,1},
		{0x448000,0x2000,1},{0x45b000,0x3000,0},{0x45e000,0x12000,0}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
			const unsigned start=0x400000+sections[i].VirtualAddress,end=start+sections[i].SizeOfRawData;
			const unsigned from=address>start?address:start,to=address+length<end?address+length:end;
			if (from>=to) continue;
			const std::size_t offset=sections[i].PointerToRawData+from-start;
			Check(offset<=data.size() && to-from<=data.size()-offset,"Section bounds");
			std::memcpy(reinterpret_cast<void *>(from),data.data()+offset,to-from);
		}
		if (!range[2]) continue;
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
	/* The collision model and diagnostics answer from this harness instead. */
	PatchJump(0x40a880,reinterpret_cast<const void *>(&StubPointLeafnum));
	PatchJump(0x408860,reinterpret_cast<const void *>(&StubLeafArea));
	PatchJump(0x408830,reinterpret_cast<const void *>(&StubLeafCluster));
	PatchJump(0x40af50,reinterpret_cast<const void *>(&StubWriteAreaBits));
	PatchJump(0x40acf0,reinterpret_cast<const void *>(&StubClusterPVS));
	PatchJump(0x40aee0,reinterpret_cast<const void *>(&StubAreasConnected));
	PatchJump(0x40aff0,reinterpret_cast<const void *>(&StubHeadnodeVisible));
	PatchJump(0x40cc40,reinterpret_cast<const void *>(&StubPrintf));
	PatchJump(0x40cca0,reinterpret_cast<const void *>(&StubError));
}

int main(int argc,char **argv) {
	if (argc!=2) return 77;
	try {
		LoadReference(argv[1]);
		unsigned previous{}; Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
		std::vector<unsigned char> entities(static_cast<std::size_t>(MaxEntities+1)*EntityStride);
		std::vector<unsigned char> saved(entities.size());
		std::vector<unsigned char> client(ClientStride),player(PlayerStateBytes+64);
		std::vector<unsigned char> ring(static_cast<std::size_t>(Capacity)*StateBytes);
		fntech3_q3_ihv_game_export_t game{};
		game.entities=reinterpret_cast<fntech3_q3_ihv_entity_t *>(entities.data());
		game.entityStride=EntityStride;game.entityCapacity=MaxEntities+1;
		*reinterpret_cast<void **>(0x4254e74)=&game;
		*reinterpret_cast<int *>(0x424de48)=Capacity;
		*reinterpret_cast<void **>(0x424de50)=ring.data();
		const auto builder=reinterpret_cast<void (__cdecl *)(void *,fntech3_q3_ihv_entity_t *)>(0x41b210);

		Replica replica;
		replica.entities=entities.data();
		fntech3::q3ihv::SnapshotHost host;
		host.context=&replica;host.pvsBytes=AreaBits;
		host.PointLeafnum=HostPointLeafnum;host.LeafArea=HostLeafArea;host.LeafCluster=HostLeafCluster;
		host.WriteAreaBits=HostWriteAreaBits;host.ClusterPVS=HostClusterPVS;
		host.AreasConnected=HostAreasConnected;host.HeadnodeVisible=HostHeadnodeVisible;
		host.EntityAt=HostEntityAt;host.ReadLink=HostReadLink;
		host.Added=HostAdded;host.MarkAdded=HostMarkAdded;host.RepairedNumber=HostRepairedNumber;

		unsigned scenarios=0,selections=0,portals=0,repairs=0;
		ihv_collision_cases::Digest digest;
		for (int scenario=0;scenario<2048;++scenario) {
			const int entityCount=3+static_cast<int>(Next()%(MaxEntities-2));
			leafCount=1+static_cast<int>(Next()%4);
			for (int leaf=0;leaf<leafCount;++leaf) {
				leafArea[leaf]=static_cast<int>(Next()%3);
				leafCluster[leaf]=static_cast<int>(Next()%8);
			}
			std::memset(pvs,0,sizeof pvs);
			pvs[0]=static_cast<unsigned char>((Next()&3)?0xff:Next()&0xff);
			visibleNodes.clear();
			for (int node=10;node<13;++node) if (Next()&1) visibleNodes.push_back(node);
			areaBitsResult=1+static_cast<int>(Next()%AreaBits);
			std::memset(entities.data(),0,entities.size());
			std::memset(client.data(),0,client.size());
			std::memset(player.data(),0,player.size());
			for (int number=0;number<=MaxEntities;++number) std::memset(LinkRecord(number),0,SvEntityStride);
			game.entityCount=entityCount;
			// The viewer is entity one, reached through its own client record.
			auto *viewer=reinterpret_cast<fntech3_q3_ihv_entity_t *>(entities.data()+EntityStride);
			Check(fntech3::q3ihv::SetEntityNumber(*viewer,1),"Viewer number");
			viewer->inuse=1;viewer->linked=1;
			viewer->client=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(player.data()));
			const float viewOrigin[3]={static_cast<float>(Next()%static_cast<unsigned>(leafCount)),0,0};
			const int viewheight=static_cast<int>(Next()%32);
			std::memcpy(player.data()+FNTECH3_Q3_IHV_PLAYER_ORIGIN,viewOrigin,sizeof viewOrigin);
			std::memcpy(player.data()+FNTECH3_Q3_IHV_PLAYER_VIEWHEIGHT,&viewheight,sizeof viewheight);
			for (int number=2;number<entityCount;++number) {
				auto *entity=reinterpret_cast<fntech3_q3_ihv_entity_t *>(entities.data()+static_cast<unsigned>(number)*EntityStride);
				// Weighted so most entities survive to the visibility rules while
				// every rejection path still runs across the whole run.
				const unsigned roll=Next();
				Check(fntech3::q3ihv::SetEntityNumber(*entity,(roll&0x7c000)?number:number+1),"Entity number");
				entity->inuse=(roll&7)?1:0;
				entity->linked=(roll&0x38)?1:0;
				entity->svFlags=0;
				if (!(roll&0x1c0)) entity->svFlags|=FNTECH3_Q3_IHV_SVF_NOCLIENT;
				if (!(roll&0xe00)) entity->svFlags|=FNTECH3_Q3_IHV_SVF_BROADCAST;
				if (!(roll&0x7000)) entity->svFlags|=FNTECH3_Q3_IHV_SVF_PORTAL;
				const float origin2[3]={static_cast<float>(Next()%static_cast<unsigned>(leafCount)),0,0};
				std::memcpy(entity->state+FNTECH3_Q3_IHV_ENTITY_ORIGIN2,origin2,sizeof origin2);
				auto *link=LinkRecord(number);
				const int viewArea=StubLeafArea(StubPointLeafnum(viewOrigin));
				Put<int>(link,LinkArea,(Next()&1)?viewArea:static_cast<int>(Next()%4)-1);
				Put<int>(link,LinkArea2,(Next()&3)?static_cast<int>(Next()%4)-1:viewArea);
				const unsigned shape=Next()%5;
				if (shape==0) { Put<int>(link,LinkNumClusters,-1); Put<int>(link,LinkHeadnode,10+static_cast<int>(Next()%4)); }
				else {
					const int clusters=static_cast<int>(shape)-1;
					Put<int>(link,LinkNumClusters,clusters);
					for (int i=0;i<clusters;++i) Put<int>(link,LinkClusterNums+static_cast<unsigned>(i)*4,static_cast<int>(Next()%8));
				}
				if (entity->svFlags&FNTECH3_Q3_IHV_SVF_PORTAL) ++portals;
			}
			std::memcpy(saved.data(),entities.data(),entities.size());
			const int sequence=static_cast<int>(Next()%64);
			Put<int>(client.data(),SequenceOffset,sequence);
			*reinterpret_cast<int *>(0x424de4c)=static_cast<int>(Next()%static_cast<unsigned>(Capacity));
			originalRepairs=replicaRepairs=engineErrors=0;

			builder(client.data(),viewer);
			Check(!engineErrors,"Original engine error");
			const auto *frame=client.data()+FramesOffset+static_cast<unsigned>(sequence&31)*FrameStride;
			const int count=Get<int>(frame,FrameCount),first=Get<int>(frame,FrameFirst);
			Check(count>=0 && count<=Capacity,"Original snapshot count");
			std::vector<int> original;
			for (int i=0;i<count;++i) {
				const int slot=((first+i)%Capacity+Capacity)%Capacity;
				original.push_back(Get<short>(ring.data(),static_cast<unsigned>(slot)*StateBytes));
			}
			const int originalAreaBytes=Get<int>(frame,FrameAreaBytes);
			unsigned char originalAreaBits[AreaBits];
			std::memcpy(originalAreaBits,frame+FrameAreaBits,sizeof originalAreaBits);
			// The player state is the game's own client record, copied whole.
			Check(!std::memcmp(frame+FramePlayerState,player.data(),PlayerStateBytes),"Frame player state");

			std::memcpy(entities.data(),saved.data(),entities.size());
			++replica.counter;
			HostMarkAdded(&replica,1);
			host.entityCount=entityCount;
			float origin[3]={viewOrigin[0],viewOrigin[1],viewOrigin[2]+static_cast<float>(viewheight)};
			fntech3::q3ihv::SnapshotSelection selection;
			unsigned char areabits[AreaBits]{};
			int areabytes=0;
			Check(selection.FromPoint(host,origin,areabits,areabytes),"Replica selection");
			selection.Sort();

			Check(selection.Count()==static_cast<int>(original.size()),"Selected entity count");
			for (int i=0;i<selection.Count();++i) {
				Check(selection.Number(i)==original[static_cast<std::size_t>(i)],"Selected entity number");
				digest.Word(static_cast<std::uint32_t>(selection.Number(i)));
				++selections;
			}
			Check(areabytes==originalAreaBytes,"Frame area bytes");
			Check(!std::memcmp(areabits,originalAreaBits,sizeof areabits),"Frame area bits");
			Check(originalRepairs==replicaRepairs,"Repaired entity numbers");
			repairs+=static_cast<unsigned>(originalRepairs);
			digest.Word(static_cast<std::uint32_t>(selection.Count()));
			digest.Word(static_cast<std::uint32_t>(areabytes));
			for (unsigned char bit : areabits) digest.Byte(bit);
			++scenarios;
		}
		std::printf("{\"scope\":\"original SV_BuildClientSnapshot visibility walk against fntech3's native"
			" snapshot selection, with synthetic collision replies; no original engine startup\","
			"\"scenarios\":%u,\"selections\":%u,\"portals\":%u,\"repaired_numbers\":%u,\"digest\":\"%016llx\"}\n",
			scenarios,selections,portals,repairs,static_cast<unsigned long long>(digest.value));
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr,"%s\n",e.what());
		return 1;
	}
}
