/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Windows x86 observation host for a user-supplied IHV game DLL.
 * No original code, DLL or asset is incorporated. Imports outside the observed
 * lifecycle fail explicitly. This does not stand in for server integration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include <array>
#include <csetjmp>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(sizeof(void *) == 4, "The supplied native IHV game requires Windows x86");
static_assert(sizeof(fntech3_q3_ihv_game_import_t) == 140);
static_assert(sizeof(fntech3_q3_ihv_game_export_t) == 80);
static_assert(offsetof(fntech3_q3_ihv_game_export_t, entities) == 64);
static_assert(offsetof(fntech3_q3_ihv_game_export_t, entityStride) == 68);

namespace {
namespace fs = std::filesystem;
using namespace fntech3::q3ihv;
TaggedMemory memory(32 * 1024 * 1024);
struct Variable {
	CvarMirror mirror;
	std::string name, value, reset;
	int flags = 0;
	void Publish(int modified) {
		if (!mirror.Assign(name.c_str(), value.c_str(), reset.c_str(), nullptr, flags,
			modified, static_cast<float>(std::atof(value.c_str())))) throw std::runtime_error("Cvar mirror overflow");
	}
};
std::map<std::string, std::unique_ptr<Variable>> variables;
std::array<int, 35> calls{};
std::vector<std::pair<int, int>> allocations;
std::vector<int> freedTags;
std::vector<std::string> files;
fs::path assetRoot;
bool missingFiles = false;
int failAllocation = -1, allocationAttempt = 0;
std::jmp_buf moduleError;
char errorText[1024]{};

void Check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
[[noreturn]] void Fail(const char *message) {
	std::snprintf(errorText, sizeof(errorText), "%s", message); std::longjmp(moduleError, 1);
}
// Deliberately match the engine's C error boundary. No object requiring
// destruction lives between this jump target and a failing import.
#pragma warning(push)
#pragma warning(disable: 4611)
bool CallModule(void (__cdecl *callback)()) {
	errorText[0] = 0;
	if (setjmp(moduleError)) return false;
	callback(); return true;
}
#pragma warning(pop)
void __cdecl Printf(const char *fmt, ...) {
	++calls[0]; va_list args; va_start(args, fmt); std::vfprintf(stderr, fmt, args); va_end(args);
}
void __cdecl Error(int, const char *fmt, ...) {
	++calls[1]; va_list args; va_start(args, fmt); std::vsnprintf(errorText, sizeof(errorText), fmt, args); va_end(args);
	std::longjmp(moduleError, 1);
}
int __cdecl Milliseconds() { ++calls[2]; return 0; }
void *__cdecl Allocate(int bytes, int tag) {
	++calls[3]; allocations.emplace_back(bytes, tag);
	if (allocationAttempt++ == failAllocation) Fail("injected allocation failure");
	void *data = memory.Allocate(bytes, tag); if (!data) Fail("tagged allocation rejected"); return data;
}
void __cdecl Free(void *data) { ++calls[4]; if (!memory.Free(data)) Fail("unowned tagged free"); }
void __cdecl FreeTags(int tag) { ++calls[5]; freedTags.push_back(tag); memory.FreeTags(tag); }
std::string Key(const char *name) {
	Check(name && std::strlen(name) < CvarMirror::NameSize, "Invalid cvar name");
	std::string key(name); for (char &c : key) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return key;
}
fntech3_q3_ihv_cvar_t *__cdecl CvarGet(const char *name, const char *value, int flags) {
	++calls[6]; auto &entry = variables[Key(name)];
	if (!entry) {
		entry = std::make_unique<Variable>(); entry->name = name; entry->value = entry->reset = value; entry->flags = flags;
		entry->Publish(1);
	} else {
		entry->flags |= flags;
		if (entry->reset.empty()) entry->reset = value;
		if (flags & 0x80) entry->value = value;
		entry->Publish(entry->mirror.Record()->modified);
	}
	return entry->mirror.Record();
}
void __cdecl CvarSet(const char *name, const char *value) {
	++calls[7]; auto &entry = variables[Key(name)];
	if (!entry) {
		entry = std::make_unique<Variable>(); entry->name = name; entry->value = entry->reset = value; entry->Publish(1);
	} else {
		const bool changed = entry->value != value; entry->value = value;
		entry->Publish(changed ? 1 : entry->mirror.Record()->modified);
	}
}
int __cdecl ReadFile(const char *name, void **out) {
	++calls[10]; if (out) *out = nullptr; files.emplace_back(name);
	const fs::path relative(name);
	Check(!relative.is_absolute() && !relative.has_root_name() && !relative.has_root_directory(), "Absolute asset path");
	Check(std::string(name).find(':') == std::string::npos, "Asset stream path");
	for (const auto &part : relative) Check(part != "..", "Asset traversal");
	if (missingFiles) return -1;
	const auto path = fs::weakly_canonical(assetRoot / relative);
	const auto within = path.lexically_relative(assetRoot);
	Check(!within.empty() && *within.begin() != "..", "Asset escapes root");
	std::ifstream input(path, std::ios::binary | std::ios::ate); if (!input) return -1;
	const auto length = input.tellg(); Check(length >= 0 && length < 16 * 1024 * 1024, "Asset size");
	if (!out) return static_cast<int>(length);
	void *data = memory.Allocate(static_cast<int>(length) + 1, 0); Check(data != nullptr, "Read allocation");
	input.seekg(0); input.read(static_cast<char *>(data), length);
	if (!input) { memory.Free(data); throw std::runtime_error("Short asset read"); }
	*out = data; return static_cast<int>(length);
}
void __cdecl FreeFile(void *data) { ++calls[11]; if (!memory.Free(data)) Fail("unowned file free"); }

#define UNEXPECTED(result, name, slot, args) result __cdecl name args { ++calls[slot]; Fail("Unexpected import: " #name); }
UNEXPECTED(int, Argc, 8, ())
UNEXPECTED(char *, Argv, 9, (int))
UNEXPECTED(void, WriteFile, 12, (const char *, const void *, int))
UNEXPECTED(char *, FileForWriting, 13, (const char *))
UNEXPECTED(void, AddCommandText, 14, (const char *))
UNEXPECTED(void, DebugGraph, 15, (float, int))
UNEXPECTED(void, DropClient, 16, (fntech3_q3_ihv_entity_t *, const char *))
UNEXPECTED(void, SendServerCommand, 17, (fntech3_q3_ihv_entity_t *, const char *, ...))
UNEXPECTED(void, SetConfigstring, 18, (int, const char *))
UNEXPECTED(char *, GetConfigstring, 19, (int))
UNEXPECTED(void, SetBrushModel, 20, (fntech3_q3_ihv_entity_t *, const char *))
UNEXPECTED(fntech3_q3_ihv_trace_t, Trace, 21, (const float *, const float *, const float *, const float *, int, int))
UNEXPECTED(int, PointContents, 22, (const float *, int))
UNEXPECTED(int, InPVS, 23, (const float *, const float *))
UNEXPECTED(int, InPVSIgnorePortals, 24, (const float *, const float *))
UNEXPECTED(void, AdjustAreaPortalState, 25, (fntech3_q3_ihv_entity_t *, int))
UNEXPECTED(int, AreasConnected, 26, (int, int))
UNEXPECTED(void, LinkEntity, 27, (fntech3_q3_ihv_entity_t *))
UNEXPECTED(void, UnlinkEntity, 28, (fntech3_q3_ihv_entity_t *))
UNEXPECTED(int, ContactEntities, 29, (const float *, const float *, fntech3_q3_ihv_entity_t **, int))
UNEXPECTED(int, SnapshotEntityCount, 30, (fntech3_q3_ihv_entity_t *))
UNEXPECTED(int, SnapshotEntityNumber, 31, (fntech3_q3_ihv_entity_t *, int))
UNEXPECTED(fntech3_q3_ihv_entity_t *, BotAllocateClient, 32, ())
UNEXPECTED(void, BotFreeClient, 33, (fntech3_q3_ihv_entity_t *))
UNEXPECTED(void, BotSetUserinfo, 34, (fntech3_q3_ihv_entity_t *, const char *))
#undef UNEXPECTED

void Verify(const char *path) {
	std::ifstream input(path, std::ios::binary); Check(bool(input), "Open reference DLL");
	std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	HCRYPTPROV provider{}; HCRYPTHASH hash{};
	Check(CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT), "Crypto context");
	if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) { CryptReleaseContext(provider, 0); Check(false, "Hash create"); }
	BYTE digest[32]{}; DWORD count = sizeof(digest);
	const bool ok = CryptHashData(hash, reinterpret_cast<const BYTE *>(bytes.data()), static_cast<DWORD>(bytes.size()), 0)
		&& CryptGetHashParam(hash, HP_HASHVAL, digest, &count, 0);
	CryptDestroyHash(hash); CryptReleaseContext(provider, 0); Check(ok && count == 32, "Hash compute");
	char value[65]{}; for (int i = 0; i < 32; ++i) std::snprintf(value + i * 2, 3, "%02x", digest[i]);
	Check(!std::strcmp(value, "9855d375751272d0b22e9078e827cf7ff3c2e42867f525b7fd84b93967c0eeb6"), "Unexpected DLL hash");
}
void Quote(const std::string &text) {
	std::cout << '"';
	for (const unsigned char c : text) {
		if (c == '"' || c == '\\') std::cout << '\\' << c;
		else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); std::cout << escaped; }
		else std::cout << c;
	}
	std::cout << '"';
}
fntech3_q3_ihv_game_import_t DefaultImports() {
	return {Printf, Error, Milliseconds, Allocate, Free, FreeTags, CvarGet, CvarSet, Argc, Argv,
		ReadFile, FreeFile, WriteFile, FileForWriting, AddCommandText, DebugGraph, DropClient, SendServerCommand, SetConfigstring,
		GetConfigstring, SetBrushModel, Trace, PointContents, InPVS, InPVSIgnorePortals, AdjustAreaPortalState, AreasConnected,
		LinkEntity, UnlinkEntity, ContactEntities, SnapshotEntityCount, SnapshotEntityNumber, BotAllocateClient, BotFreeClient, BotSetUserinfo};
}
} // namespace

#ifndef FNTECH3_IHV_REFERENCE_NO_MAIN
int main(int argc, char **argv) {
	if (argc < 3 || argc > 5) return 77;
	HMODULE module = nullptr;
	try {
		Verify(argv[1]); assetRoot = fs::canonical(argv[2]); Check(fs::is_directory(assetRoot), "Asset root");
		const std::string mode = argc > 3 ? argv[3] : "normal";
		if (mode == "missing-files") missingFiles = true;
		else if (mode == "clamp-clients") { CvarSet("maxclients", "128"); CvarSet("maxentities", "64"); }
		else if (mode == "fail-allocation") { Check(argc == 5, "Specify allocation index"); failAllocation = std::stoi(argv[4]); Check(failAllocation >= 0 && failAllocation < 3, "Allocation index"); }
		else Check(mode == "normal", "Unknown probe mode");
		const auto dll = fs::canonical(argv[1]);
		module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
		Check(module != nullptr, "Load native game DLL");
		auto getter = reinterpret_cast<fntech3_q3_ihv_GetGameAPI_t>(GetProcAddress(module, "GetGameAPI")); Check(getter != nullptr, "GetGameAPI");
		auto imports=DefaultImports();
		auto *game = getter(&imports); Check(game && game->apiVersion == 4 && game->entityStride == 556, "Game contract");
		Check(!game->entities && !game->entityCount && !game->entityCapacity, "Initial export state");
		if (!CallModule(game->Init)) {
			// Like the engine's fatal/drop boundary: discard a partially initialized
			// module and reclaim host-owned memory. Do not call Shutdown on it.
			Check(failAllocation >= 0 && !std::strcmp(errorText, "injected allocation failure"), errorText);
			FreeLibrary(module); module = nullptr; memory.Clear();
			Check(memory.Count() == 0, "Failed-init memory cleanup");
			std::cout << "{\"scope\":\"native init failure only\",\"failed_allocation\":" << failAllocation << ",\"cleanup\":true}\n";
			return 0;
		}
		Check(failAllocation < 0, "Failure injection was not reached");
		const int clients = static_cast<int>(variables.at("maxclients")->mirror.Record()->value);
		const int capacity = static_cast<int>(variables.at("maxentities")->mirror.Record()->value);
		Check(game->entityCount == clients + 1 && game->entityCapacity == capacity, "Entity counts after Init");
		Check(memory.AllocationSize(game->entities) == static_cast<std::size_t>(capacity) * 556, "Entity allocation extent");
		EntityArray entities; Check(entities.Bind(game->entities, memory.AllocationSize(game->entities), game->entityStride,
			game->entityCount, game->entityCapacity), "Native entity binding");
		Check(entities.ClientIndex(entities.At(1), clients) == 0 && entities.ClientIndex(entities.At(0), clients) == -1, "Native client indices");
		Check(allocations == std::vector<std::pair<int, int>>{{capacity * 556, 765}, {clients * 1464, 765}, {clients * 700, 765}}, "Init allocations");
		Check(CallModule(game->Shutdown), errorText);
		Check(memory.Count() == 0 && memory.Bytes() == 0, "Shutdown released every allocation");
		Check(freedTags == std::vector<int>{766, 765}, "Shutdown tag order");
		FreeLibrary(module); module = nullptr;
		std::cout << "{\"scope\":\"native Init/Shutdown only; no gameplay or engine integration\",\"mode\":"; Quote(mode);
		std::cout << ",\"api\":4,\"entity_stride\":556,\"clients\":" << clients << ",\"entities\":" << clients + 1 << ",\"capacity\":" << capacity;
		std::cout << ",\"allocations\":[";
		for (std::size_t i = 0; i < allocations.size(); ++i) { if (i) std::cout << ','; std::cout << "[" << allocations[i].first << ',' << allocations[i].second << ']'; }
		std::cout << "],\"import_calls\":["; for (int i = 0; i < 35; ++i) { if (i) std::cout << ','; std::cout << calls[i]; }
		std::cout << "],\"files\":["; for (std::size_t i = 0; i < files.size(); ++i) { if (i) std::cout << ','; Quote(files[i]); }
		std::cout << "],\"cvars\":["; bool first = true;
		for (const auto &pair : variables) { if (!first) std::cout << ','; first = false; const auto &v = *pair.second;
			std::cout << "{\"name\":"; Quote(v.name); std::cout << ",\"value\":"; Quote(v.value); std::cout << ",\"flags\":" << v.flags << '}'; }
		std::cout << "],\"shutdown_tags\":[766,765],\"remaining_allocations\":0}\n";
		return 0;
	} catch (const std::exception &e) {
		if (module) FreeLibrary(module); memory.Clear(); std::fprintf(stderr, "%s\n", e.what()); return 1;
	}
}
#endif
