/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original IHV area visibility and entity-color observations. Uses
 * generated leaf/PVS/color inputs and replaced point-leaf/cluster-PVS services.
 * No original engine entry point, display, input or network session. Numeric
 * host base: 0x3f0000. This does not validate BSP traversal or rendered output. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_game_abi.h"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }
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
	char text[65]{}; for(int i=0;i<32;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
	Check(!std::strcmp(text,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37"),"Unexpected original executable");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x3f0000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x440000,0x2000},{0x444000,0x1000},{0x45b000,0x1000}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
			const unsigned start=0x400000+sections[i].VirtualAddress,end=start+sections[i].SizeOfRawData;
			const unsigned from=address>start?address:start,to=address+length<end?address+length:end;
			if(from>=to) continue;
			const std::size_t offset=sections[i].PointerToRawData+from-start;
			Check(offset<=data.size() && to-from<=data.size()-offset,"Section bounds");
			std::memcpy(reinterpret_cast<void *>(from),data.data()+offset,to-from);
		}
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
}

#include "../code/compat/q3_ihv_scene.hpp"
namespace render = fntech3::q3ihv::render;
static std::array<std::array<unsigned char, 68>, 256> leaves;
static std::array<unsigned char, 32> pvs;
static void *__cdecl PointLeaf(const float *) { return leaves[0].data(); }
static const void *__cdecl ClusterPVS(int cluster) { Check(cluster == 0, "View cluster"); return pvs.data(); }
static void Hook(unsigned address, const void *target) {
	DWORD old{}, ignored{}; Check(VirtualProtect(reinterpret_cast<void *>(address), 5, PAGE_EXECUTE_READWRITE, &old), "Hook writable");
	auto *p = reinterpret_cast<unsigned char *>(address); p[0] = 0xe9;
	const unsigned delta = reinterpret_cast<unsigned>(target) - address - 5; std::memcpy(p + 1, &delta, 4);
	Check(VirtualProtect(p, 5, PAGE_EXECUTE_READ, &ignored) && FlushInstructionCache(GetCurrentProcess(), p, 5), "Hook executable");
}
static void Word(void *out, unsigned value) { std::memcpy(out, &value, 4); }
static void Global(unsigned address, unsigned value) { Word(reinterpret_cast<void *>(address), value); }
static unsigned Bits(const void *p) { unsigned value; std::memcpy(&value, p, 4); return value; }
static unsigned Random(unsigned &state) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
int main(int argc, char **argv) {
	if (argc != 2) return 77;
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	try {
		LoadReference(argv[1]); Hook(0x440070, &PointLeaf); Hook(0x4400e0, &ClusterPVS);
		std::array<unsigned char, 32> zeroCvar{};
		for (auto address : {0x4f0366cu, 0x4f0354cu, 0x4f03584u}) Global(address, reinterpret_cast<unsigned>(zeroCvar.data()));
		std::array<unsigned char, 196> world{}; Word(world.data() + 152, 256); Word(world.data() + 160, reinterpret_cast<unsigned>(leaves.data())); Word(world.data() + 192, 256);
		Global(0x43d6d20, reinterpret_cast<unsigned>(world.data()));
		Global(0x43d6ab0, 0);
		const auto mark = reinterpret_cast<void(__cdecl *)()>(0x441a90);
		unsigned seed = 0xa159df21, areaChecks = 0, colorChecks = 0;
		std::uint64_t digest = 14695981039346656037ull;
		auto hash = [&](unsigned char value) { digest ^= value; digest *= 1099511628211ull; };
		for (unsigned n = 0; n < 1024; ++n) {
			render::View view;
			for (unsigned i = 0; i < 32; ++i) {
				view.visibleAreas[i] = n <= 2 ? static_cast<unsigned char>(n == 1 ? 0 : 255) : n < 259 ? 0 : static_cast<unsigned char>(Random(seed));
				pvs[i] = n % 3 == 0 ? 255 : static_cast<unsigned char>(Random(seed));
			}
			if (n >= 3 && n < 259) view.visibleAreas[(n - 3) / 8] = static_cast<unsigned char>(1u << ((n - 3) & 7));
			for (unsigned area = 0; area < 256; ++area) {
				leaves[area].fill(0); Word(leaves[area].data() + 52, area); Word(leaves[area].data() + 56, area);
			}
			Global(0x4f03488, 0xfffffffe); // Force a fresh visibility pass.
			Global(0x43d6ab4, n == 0 ? 0 : reinterpret_cast<unsigned>(view.visibleAreas.data()));
			mark();
			const auto frame = Bits(reinterpret_cast<const void *>(0x43d6b90)); const auto blocked = view.BlockedAreas();
			for (unsigned area = 0; area < 256; ++area) {
				const unsigned bit = 1u << (area & 7);
				const bool visible = (pvs[area / 8] & bit) && !(blocked[area / 8] & bit);
				Check((Bits(leaves[area].data() + 4) == frame) == visible, "Original area visibility differs"); hash(visible ? 1 : 0); ++areaChecks;
			}
		}
		fntech3_q3_ihv_render_entity_t entity{};
		Global(0x4f00160, reinterpret_cast<unsigned>(&entity));
		const auto color = reinterpret_cast<void(__cdecl *)(void *)>(0x444e40);
		const auto inverseColor = reinterpret_cast<void(__cdecl *)(void *)>(0x444e70);
		for (unsigned n = 0; n < 256; ++n) for (unsigned count : {0u, 1u, 7u, 64u}) for (bool inverse : {false, true}) {
			for (unsigned i = 0; i < 4; ++i) entity.rgba[i] = static_cast<unsigned char>(n + i * 47);
			std::array<unsigned char, 260> output; output.fill(0xa5); Global(0x4382f34, count);
			(inverse ? inverseColor : color)(output.data());
			for (unsigned i = 0; i < count * 4; ++i) {
				const auto expected = inverse ? static_cast<unsigned char>(255 - entity.rgba[i & 3]) : entity.rgba[i & 3];
				Check(output[i] == expected, "Original entity RGBA differs"); hash(output[i]); ++colorChecks;
			}
			for (unsigned i = count * 4; i < output.size(); ++i) Check(output[i] == 0xa5, "Color output bounds");
		}
		Global(0x4f00160, 0); Global(0x4382f34, 1); unsigned unchanged = 0x12345678; color(&unchanged); inverseColor(&unchanged); Check(unchanged == 0x12345678, "Absent entity leaves output unchanged");
		std::printf("{\"area_cases\":1024,\"area_checks\":%u,\"color_components\":%u,\"fnv1a64\":\"%016llx\"}\n", areaChecks, colorChecks, static_cast<unsigned long long>(digest));
		return 0;
	} catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}

