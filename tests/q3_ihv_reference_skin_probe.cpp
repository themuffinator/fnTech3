/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original IHV skin registration comparison. Generated and supplied
 * read-only skin text; substituted file, shader and CRT services. No original
 * engine entry point, display, input, audio or network session. Base 0x3f0000. */
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
	for (auto range : {std::array<unsigned,2>{0x439000,0x1000},{0x448000,0x2000},{0x46a000,0x1000}}) {
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

#include "../code/renderercommon/tr_q3_ihv_skin.h"
#include <filesystem>
#include <map>
#include <memory>
#include <string>
struct Skin { char name[64]; int count; unsigned shaders[32]; };
static_assert(sizeof(Skin) == 196);
static std::map<std::string, std::string> files;
static std::map<void *, std::unique_ptr<char[]>> buffers;
static std::map<std::string, unsigned> shaders;
static unsigned reads, frees, shaderCalls, warnings;
static int __cdecl ReadSkinFile(const char *name, void **out) {
	++reads; *out = nullptr; const auto found = files.find(name); if (found == files.end()) return -1;
	auto data = std::make_unique<char[]>(found->second.size() + 1);
	std::memcpy(data.get(), found->second.c_str(), found->second.size() + 1); *out = data.get(); buffers.emplace(*out, std::move(data));
	return static_cast<int>(found->second.size());
}
static void __cdecl FreeFile(void *data) { Check(buffers.erase(data) == 1, "File ownership"); ++frees; }
static unsigned ShaderToken(const char *name) {
	const auto found = shaders.find(name); if (found != shaders.end()) return found->second;
	const auto token = 100u + static_cast<unsigned>(shaders.size()); shaders.emplace(name, token); return token;
}
static unsigned __cdecl Shader(const char *name, int lightmap) { Check(name && lightmap == -1, "Shader registration arguments"); ++shaderCalls; return ShaderToken(name); }
static char *__cdecl Name(char *out, const char *input, std::size_t count) {
	std::size_t i = 0; for (; i < count && input[i]; ++i) out[i] = input[i]; for (; i < count; ++i) out[i] = 0; return out;
}
static const char *__cdecl Find(const char *text, const char *find) { return std::strstr(text, find); }
static const char *__cdecl Character(const char *text, int find) { return std::strchr(text, find); }
static void __cdecl Print(int, const char *, ...) { ++warnings; }
static void Hook(unsigned address, const void *target) {
	DWORD old{}, ignored{}; Check(VirtualProtect(reinterpret_cast<void *>(address), 5, PAGE_EXECUTE_READWRITE, &old), "Hook writable");
	auto *p = reinterpret_cast<unsigned char *>(address); p[0] = 0xe9;
	const unsigned delta = reinterpret_cast<unsigned>(target) - address - 5; std::memcpy(p + 1, &delta, 4);
	Check(VirtualProtect(p, 5, PAGE_EXECUTE_READ, &ignored) && FlushInstructionCache(GetCurrentProcess(), p, 5), "Hook executable");
}
int main(int argc, char **argv) {
	if (argc != 2 && argc != 3) return 77;
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	try {
		LoadReference(argv[1]); Hook(0x448920, &Name); Hook(0x448b40, &Find); Hook(0x4493c0, &Character); Hook(0x439fd0, &Shader);
		*reinterpret_cast<void **>(0x43d6b04) = reinterpret_cast<void *>(&ReadSkinFile);
		*reinterpret_cast<void **>(0x43d6b08) = reinterpret_cast<void *>(&FreeFile);
		*reinterpret_cast<void **>(0x43d6ae4) = reinterpret_cast<void *>(&Print);
		*reinterpret_cast<unsigned *>(0x4eced4c) = 7;
		const auto load = reinterpret_cast<Skin *(__cdecl *)(const char *)>(0x439930);
		std::vector<std::string> content = {"", "tag_weapon,\nbody,tex/body\ntag_torso,\n", "body,one\nhead,two\n",
			"tag_with_shader,ignored\nbody,one\n", "body,one\ntag_with_shader,two\n", " tag_spaced,one\n",
			"// comment,retained\nbody,second\n", "body,  spaces  \nhead,\"quoted\"\n", "body,\nhead,last",
			"body,\r\n\r\nhead,second\r\n", "body,", "body,no-newline", "body,one\rhead,two\r", "no comma"};
		content.emplace_back("body,first\0head,ignored", 23);
		for (unsigned count = 1; count <= 32; ++count) {
			std::string text; for (unsigned i = 0; i < count; ++i) text += "ignored" + std::to_string(i) + ",shaders/" + std::to_string(i) + (i & 1 ? "\r\n" : "\n"); content.push_back(text);
		}
		content.push_back("body," + std::string(1023, 'x'));
		std::vector<std::string> names;
		for (std::size_t i = 0; i < content.size(); ++i) {
			const auto name = "generated/" + std::to_string(i) + ".skin"; names.push_back(name); files[name] = content[i];
		}
		unsigned supplied = 0;
		if (argc == 3) {
			const auto root = std::filesystem::canonical(argv[2]);
			for (const char *part : {"head", "lower", "upper"}) {
				const auto name = std::string("models/players/sarge/") + part + "_default.skin";
				std::ifstream file(root / name, std::ios::binary); Check(bool(file), "Supplied skin");
				files[name] = {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()}; names.push_back(name); ++supplied;
			}
		}
		names.push_back("generated/missing.skin");
		names.push_back("generated/PLAIN.SKIN");
		names.push_back("generated/plain");
		std::uint64_t digest = 14695981039346656037ull; unsigned registered = 0, parsedShaders = 0;
		auto hash = [&](unsigned value) { for (unsigned i = 0; i < 4; ++i) { digest ^= static_cast<unsigned char>(value >> (i * 8)); digest *= 1099511628211ull; } };
		for (const auto &name : names) {
			const auto beforeShaders = shaderCalls, beforeReads = reads;
			const auto *record = load(name.c_str()); Check(record && !std::strcmp(record->name, name.c_str()), "Registered identity");
			fntech3IhvSkinNames_t parsed{};
			const auto found = files.find(name); const bool skinFile = name.size() >= 5 && name.substr(name.size() - 5) == ".skin";
			if (skinFile && found != files.end()) Check(FnQ3IHV_ParseSkin(found->second.data(), found->second.size(), &parsed), "Bounded parse");
			if (!skinFile) { parsed.count = 1; std::memcpy(parsed.names[0], name.c_str(), name.size() + 1); }
			Check(record->count == (parsed.count ? parsed.count : 1), "Original shader count");
			Check(shaderCalls == beforeShaders + parsed.count && reads == beforeReads + (skinFile ? 1 : 0), "Original service sequence");
			for (int i = 0; i < record->count; ++i) { Check(record->shaders[i] == (parsed.count ? ShaderToken(parsed.names[i]) : 7), "Original shader order/name"); hash(record->shaders[i]); }
			const auto savedReads = reads, savedShaders = shaderCalls;
			Check(load(name.c_str()) == record && reads == savedReads && shaderCalls == savedShaders, "Stable cached registration");
			parsedShaders += static_cast<unsigned>(parsed.count); ++registered;
		}
		files["generated/missing.skin"] = "body,now-present\n";
		const auto readsBefore = reads; Check(load("generated/missing.skin")->shaders[0] == 7 && reads == readsBefore, "Cached missing-file fallback");
		const auto *lower = load("generated/case-name"); const auto *upper = load("generated/CASE-NAME"); Check(lower != upper, "Case-sensitive identity");
		while (*reinterpret_cast<unsigned *>(0x4eced58) < 1024) {
			const auto name = "generated/capacity/" + std::to_string(*reinterpret_cast<unsigned *>(0x4eced58)); Check(load(name.c_str()) != nullptr, "Native registration capacity");
		}
		Check(load("generated/overflow") == nullptr && warnings == 1, "Native skin limit");
		Check(load("generated/case-name") == lower && buffers.empty() && frees == files.size() - 1, "Cache remains available and file buffers released");
		std::printf("{\"registrations_compared\":%u,\"supplied_skins\":%u,\"parsed_shader_names\":%u,\"native_capacity\":1024,\"reads\":%u,\"frees\":%u,\"fnv1a64\":\"%016llx\"}\n", registered, supplied, parsedShaders, reads, frees, static_cast<unsigned long long>(digest));
		return 0;
	} catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}

