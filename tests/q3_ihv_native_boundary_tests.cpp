/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifdef _MSC_VER
// The boundary refuses 64-bit hosts with sizeof tests, constant there; the
// engine disables this warning in q_shared.h.
#pragma warning(disable: 4127)
#endif
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_ihv_server_clients.hpp"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <limits>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
#define AT(type, member, offset) static_assert(offsetof(type, member) == offset, #type "." #member)
#define POD(type) static_assert(std::is_standard_layout<type>::value && std::is_trivial<type>::value, #type)
POD(fntech3_q3_ihv_cvar_t); POD(fntech3_q3_ihv_entity_t); POD(fntech3_q3_ihv_trace_t);
POD(fntech3_q3_ihv_usercmd_t); POD(fntech3_q3_ihv_game_import_t); POD(fntech3_q3_ihv_game_export_t);
AT(fntech3_q3_ihv_cvar_t, name, 0); AT(fntech3_q3_ihv_cvar_t, string, 4);
AT(fntech3_q3_ihv_cvar_t, resetString, 8); AT(fntech3_q3_ihv_cvar_t, latchedString, 12);
AT(fntech3_q3_ihv_cvar_t, flags, 16); AT(fntech3_q3_ihv_cvar_t, modified, 20);
AT(fntech3_q3_ihv_cvar_t, value, 24); AT(fntech3_q3_ihv_cvar_t, next, 28);
AT(fntech3_q3_ihv_entity_t, svFlags, 0xb4); AT(fntech3_q3_ihv_entity_t, bmodel, 0xb8);
AT(fntech3_q3_ihv_entity_t, maxs, 0xc8); AT(fntech3_q3_ihv_entity_t, absmax, 0xe4);
AT(fntech3_q3_ihv_entity_t, currentAngles, 0xfc);
AT(fntech3_q3_ihv_trace_t, allsolid, 0); AT(fntech3_q3_ihv_trace_t, startsolid, 4);
AT(fntech3_q3_ihv_trace_t, endpos, 12); AT(fntech3_q3_ihv_trace_t, planeNormal, 24);
AT(fntech3_q3_ihv_trace_t, planeDist, 36); AT(fntech3_q3_ihv_trace_t, planeType, 40);
AT(fntech3_q3_ihv_trace_t, planeSignbits, 41); AT(fntech3_q3_ihv_trace_t, surfaceFlags, 44);
AT(fntech3_q3_ihv_trace_t, contents, 48);

#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(fntech3_q3_ihv_game_import_t) == 140);
static_assert(sizeof(fntech3_q3_ihv_game_export_t) == 80);
#define IMPORT(member, slot) AT(fntech3_q3_ihv_game_import_t, member, slot * 4)
IMPORT(Printf, 0); IMPORT(Error, 1); IMPORT(Milliseconds, 2); IMPORT(TagMalloc, 3);
IMPORT(Free, 4); IMPORT(FreeTags, 5); IMPORT(CvarGet, 6); IMPORT(CvarSet, 7);
IMPORT(Argc, 8); IMPORT(Argv, 9); IMPORT(ReadFile, 10); IMPORT(FreeFile, 11);
IMPORT(WriteFile, 12); IMPORT(FileForWriting, 13); IMPORT(AddCommandText, 14); IMPORT(DebugGraph, 15);
IMPORT(DropClient, 16); IMPORT(SendServerCommand, 17); IMPORT(SetConfigstring, 18); IMPORT(GetConfigstring, 19);
IMPORT(SetBrushModel, 20); IMPORT(Trace, 21); IMPORT(PointContents, 22); IMPORT(InPVS, 23);
IMPORT(InPVSIgnorePortals, 24); IMPORT(AdjustAreaPortalState, 25); IMPORT(AreasConnected, 26);
IMPORT(LinkEntity, 27); IMPORT(UnlinkEntity, 28); IMPORT(ContactEntities, 29);
IMPORT(SnapshotEntityCount, 30); IMPORT(SnapshotEntityNumber, 31); IMPORT(BotAllocateClient, 32);
IMPORT(BotFreeClient, 33); IMPORT(BotSetUserinfo, 34);
#define EXPORT(member, slot) AT(fntech3_q3_ihv_game_export_t, member, slot * 4)
EXPORT(apiVersion, 0); EXPORT(Init, 1); EXPORT(Shutdown, 2); EXPORT(SpawnEntities, 3);
EXPORT(ClientConnect, 4); EXPORT(ClientBegin, 5); EXPORT(ClientUserinfoChanged, 6); EXPORT(ClientDisconnect, 7);
EXPORT(ClientCommand, 8); EXPORT(ClientThink, 9); EXPORT(ClientAfterCommands, 10);
EXPORT(BotBegin, 11); EXPORT(BotThink, 12); EXPORT(reserved, 13); EXPORT(RunFrame, 14); EXPORT(ConsoleCommand, 15);
EXPORT(entities, 16); EXPORT(entityStride, 17); EXPORT(entityCount, 18); EXPORT(entityCapacity, 19);
#endif

using namespace fntech3::q3ihv;
static const char *Text(std::uint32_t pointer) { return reinterpret_cast<const char *>(static_cast<std::uintptr_t>(pointer)); }

void Cvars() {
	CvarMirror mirror;
	if constexpr (sizeof(void *) != 4) {
		CHECK(!mirror.Assign("name", "value", "reset", nullptr, 0, 1, 0));
		const fntech3_q3_ihv_cvar_t empty{}; CHECK(!std::memcmp(mirror.Record(), &empty, sizeof(empty))); return;
	}
	CHECK(mirror.Assign("g_speed", "320", "300", "400", 0x28, 1, 320, 0x12345678));
	auto *record = mirror.Record(); const auto before = *record;
	CHECK(!std::strcmp(Text(record->name), "g_speed") && !std::strcmp(Text(record->string), "320"));
	CHECK(!std::strcmp(Text(record->resetString), "300") && !std::strcmp(Text(record->latchedString), "400"));
	CHECK(record->flags == 0x28 && record->modified == 1 && record->value == 320 && record->next == 0x12345678);
	const std::array<char, CvarMirror::ValueSize> unterminated = [] { std::array<char, CvarMirror::ValueSize> a{}; a.fill('x'); return a; }();
	for (const char *invalid : {static_cast<const char *>(nullptr), unterminated.data()}) {
		CHECK(!mirror.Assign("name", invalid, "reset", nullptr, 0, 0, 0));
		CHECK(!mirror.Assign("name", "value", invalid, nullptr, 0, 0, 0));
		CHECK(!mirror.Assign(invalid, "value", "reset", nullptr, 0, 0, 0));
	}
	CHECK(!mirror.Assign("name", "value", "reset", unterminated.data(), 0, 0, 0));
	CHECK(!mirror.Assign("", "value", "reset", nullptr, 0, 0, 0));
	CHECK(!std::memcmp(record, &before, sizeof(before)));
	CHECK(!std::strcmp(Text(record->string), "320"));
	record->modified = 0;
	// Publish from the old mirror itself; cross-field aliases must be safe.
	CHECK(mirror.Assign(Text(record->string), Text(record->resetString), Text(record->name), nullptr, 8, record->modified, 300));
	CHECK(mirror.Record() == record && record->name == before.name && record->string == before.string);
	CHECK(!std::strcmp(Text(record->name), "320") && !std::strcmp(Text(record->string), "300"));
	CHECK(!std::strcmp(Text(record->resetString), "g_speed") && !record->latchedString && !record->modified);
	char maxName[CvarMirror::NameSize], maxValue[CvarMirror::ValueSize];
	std::memset(maxName, 'n', sizeof(maxName)); maxName[sizeof(maxName)-1] = 0;
	std::memset(maxValue, 'v', sizeof(maxValue)); maxValue[sizeof(maxValue)-1] = 0;
	CHECK(mirror.Assign(maxName, maxValue, maxValue, maxValue, 0, 1, 0));
}

void EntityPointers() {
	alignas(fntech3_q3_ihv_entity_t) std::array<unsigned char, 556 * 12> data{};
	EntityArray entities;
	CHECK(entities.Index(data.data()) == -1 && !entities.At(0) && !entities.SetCount(1));
	CHECK(entities.Bind(data.data(), data.size(), 556, 9, 12));
	for (int i = 0; i < 12; ++i) {
		auto *p = data.data() + i * 556;
		CHECK(entities.Index(p) == (i < 9 ? i : -1)); CHECK(entities.Index(p, true) == i);
		CHECK(entities.At(i, true) == reinterpret_cast<void *>(p));
		CHECK(entities.ClientIndex(p, 8) == (i >= 1 && i <= 8 ? i - 1 : -1));
		for (int byte = 1; byte < 556; ++byte) CHECK(entities.Index(p + byte, true) == -1);
	}
	CHECK(entities.Index(data.data() + data.size(), true) == -1);
	CHECK(entities.Index(reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(data.data()) - 1)) == -1);
	CHECK(!entities.At(-1) && !entities.At(12, true) && !entities.At(INT_MAX));
	CHECK(entities.ClientIndex(entities.At(1), 9) == -1 && entities.ClientIndex(entities.At(1), -1) == -1);
	CHECK(!entities.SetCount(13) && !entities.SetCount(0)); CHECK(entities.At(8) && !entities.At(9));
	CHECK(entities.SetCount(12) && entities.At(11));
	for (int number : {-32768, -1, 0, 1, 1023, 32767}) {
		const auto wire = static_cast<std::int16_t>(number); std::memcpy(entities.At(0)->state, &wire, 2);
		CHECK(EntityNumber(*entities.At(0)) == number);
		CHECK(SetEntityNumber(*entities.At(1), number) && EntityNumber(*entities.At(1)) == number);
	}
	// Only the number word changes; the rest of the native state is the game's.
	std::memset(entities.At(2)->state, 0x5a, sizeof entities.At(2)->state);
	CHECK(SetEntityNumber(*entities.At(2), 7) && EntityNumber(*entities.At(2)) == 7);
	for (std::size_t byte = 2; byte < sizeof entities.At(2)->state; ++byte)
		CHECK(entities.At(2)->state[byte] == 0x5a);
	for (int number : {-32769, 32768, INT_MIN, INT_MAX}) CHECK(!SetEntityNumber(*entities.At(3), number));
	for (int stride : {0, -1, 267, 269, INT_MAX}) CHECK(!entities.Bind(data.data(), data.size(), stride, 1, 1));
	CHECK(!entities.Bind(data.data(), data.size(), 556, 13, 12));
	CHECK(!entities.Bind(data.data(), data.size(), 556, 1, INT_MAX));
	CHECK(!entities.Bind(data.data(), data.size()-1, 556, 9, 12));
	CHECK(!entities.Bind(nullptr, data.size(), 556, 9, 12));
	CHECK(!entities.Bind(data.data()+1, data.size()-1, 556, 1, 1));
	const auto top = (std::numeric_limits<std::uintptr_t>::max)() & ~std::uintptr_t(3);
	CHECK(!entities.Bind(reinterpret_cast<void *>(top), 556, 556, 1, 1));
	CHECK(!entities.At(0) && entities.Index(data.data()) == -1);
	CHECK(entities.Bind(data.data(), data.size(), 556, 9, 12)); entities.Clear(); CHECK(!entities.At(0));
}

void Allocations() {
	TaggedMemory memory(4096); CHECK(!memory.Allocate(-1, 1) && !memory.Allocate(INT_MAX, 1));
	void *a = memory.Allocate(800, 765), *b = memory.Allocate(1200, 766), *c = memory.Allocate(8, 0xffff);
	CHECK(a && b && c && memory.Count() == 3 && memory.Bytes() == 2008);
	for (int i = 0; i < 800; ++i) CHECK(static_cast<unsigned char *>(a)[i] == 0);
	std::memset(a, 0xa5, 800); CHECK(memory.AllocationSize(a) == 800);
	CHECK(!memory.Free(static_cast<unsigned char *>(a)+1) && !memory.Free(nullptr));
	int unrelated = 0; CHECK(!memory.Free(&unrelated));
	memory.FreeTags(0xffff); CHECK(memory.Count() == 3); // Sign extension precedes comparison.
	memory.FreeTags(-1); CHECK(memory.Count() == 2);
	memory.FreeTags(766); CHECK(memory.Bytes() == 800 && memory.Count() == 1);
	CHECK(memory.Free(a) && !memory.Free(a) && !memory.Bytes() && !memory.Count());
	CHECK(!memory.AllocationSize(a));
	// Zero-byte requests still consume a bounded ownership/storage budget.
	int zeroes = 0; while (memory.Allocate(0, 765)) CHECK(++zeroes < 4096);
	CHECK(zeroes > 0 && !memory.Bytes()); memory.FreeTags(765); CHECK(!memory.Count());
	for (int cycle = 0; cycle < 20; ++cycle) {
		CHECK(memory.Allocate(3000, 765)); CHECK(!memory.Allocate(1200, 766));
		memory.Clear(); CHECK(!memory.Count() && !memory.Bytes());
	}
	TaggedMemory empty(0); CHECK(!empty.Allocate(0, 765) && !empty.Allocate(1, 765));
}

/* Native server client records: slot mapping, bot lifecycle, userinfo limits
 * and the snapshot history the two getters read. */
void ClientRecords() {
	NativeClients clients;
	CHECK(!clients.Count() && !clients.Valid(0) && clients.State(0) == -1 && clients.FirstFreeSlot() == -1);
	CHECK(!clients.Reset(0, 16) && !clients.Reset(-1, 16) && !clients.Reset(16, 16) && !clients.Reset(17, 16));
	CHECK(!clients.Reset(1, 0) && clients.Reset(8, 16) && clients.Count() == 8);
	int slot = -1;
	for (int number : {-1, 0, 9, INT_MAX, INT_MIN}) CHECK(!clients.SlotForNumber(number, slot));
	for (int number = 1; number <= 8; ++number) CHECK(clients.SlotForNumber(number, slot) && slot == number - 1);
	// Allocation takes the lowest free slot; a released slot stays a zombie.
	for (int i = 0; i < 8; ++i) {
		CHECK(clients.FirstFreeSlot() == i && !clients.HasEntity(i));
		CHECK(clients.Connect(i, 1000 + i) && !clients.Connect(i, 0));
		CHECK(clients.State(i) == FNTECH3_Q3_IHV_CLIENT_CONNECTED && clients.LastPacketTime(i) == 1000 + i);
		CHECK(clients.MessageCleared(i) && clients.HasEntity(i));
	}
	CHECK(clients.FirstFreeSlot() == -1 && !clients.Connect(8, 0) && !clients.Connect(-1, 0));
	CHECK(clients.CopyUserinfo(3, "kept-userinfo") && clients.ApplyUserinfo(3, "keep", "9000", "10"));
	CHECK(clients.Release(3) && !clients.Name(3)[0] && clients.State(3) == FNTECH3_Q3_IHV_CLIENT_ZOMBIE);
	CHECK(clients.HasEntity(3) && clients.Rate(3) == 9000 && clients.SnapshotMsec(3) == 100);
	CHECK(clients.FirstFreeSlot() == -1 && !clients.Release(8) && !clients.Release(-1));
	// The scheduler promotes a client once, then keeps asking it to think.
	CHECK(clients.StepBot(0, 5) == NativeClients::BotStep::Begin && clients.State(0) == FNTECH3_Q3_IHV_CLIENT_ACTIVE);
	CHECK(clients.LastPacketTime(0) == 5);
	CHECK(clients.StepBot(0, 6) == NativeClients::BotStep::Think && clients.LastPacketTime(0) == 6);
	CHECK(clients.StepBot(3, 7) == NativeClients::BotStep::Begin); // A zombie is not active.
	CHECK(clients.StepBot(8, 7) == NativeClients::BotStep::None && clients.StepBot(-1, 7) == NativeClients::BotStep::None);
	// Userinfo copies stop at 1023 bytes and the name at 31 characters.
	const std::string userinfo(1200, 'x');
	CHECK(clients.CopyUserinfo(1, userinfo.c_str()) && std::strlen(clients.Userinfo(1)) == 1023);
	CHECK(clients.ApplyUserinfo(1, userinfo.c_str(), nullptr, nullptr) && std::strlen(clients.Name(1)) == 31);
	CHECK(clients.Rate(1) == 5000 && clients.SnapshotMsec(1) == 50);
	CHECK(!clients.CopyUserinfo(1, nullptr) && !clients.CopyUserinfo(8, "") && !clients.ApplyUserinfo(8, "", "", ""));
	CHECK(clients.MutableUserinfo(1) == clients.Userinfo(1) && !clients.MutableUserinfo(8));
	CHECK(clients.CopyUserinfo(1, "") && !clients.Userinfo(1)[0]);
	CHECK(clients.ApplyUserinfo(1, "", "", "") && !clients.Name(1)[0]);
	for (auto pair : {std::array<const char *, 2>{nullptr, nullptr}, {"", nullptr}, {"0", "100"}, {"-5", "100"},
		{"99", "100"}, {"100", "100"}, {"15000", "15000"}, {"15001", "15000"}, {"1e9", "100"}, {"abc", "100"}})
		CHECK(UserinfoRate(pair[0]) == (pair[1] ? std::atoi(pair[1]) : 5000));
	for (auto pair : {std::array<const char *, 2>{nullptr, nullptr}, {"", nullptr}, {"0", "1000"}, {"-3", "1000"},
		{"1", "1000"}, {"20", "50"}, {"30", "33"}, {"31", "33"}, {"abc", "1000"}})
		CHECK(UserinfoSnapshotMsec(pair[0]) == (pair[1] ? std::atoi(pair[1]) : 50));
	// The history is addressed by the outgoing sequence's low five bits.
	NativeClients::SnapshotFrame frame;
	CHECK(clients.CurrentFrame(0, frame) && !frame.count && !frame.first && !clients.CurrentFrame(8, frame));
	CHECK(!clients.SetFrame(0, 0, -1, 0) && !clients.SetFrame(8, 0, 0, 0) && !clients.SetOutgoingSequence(8, 0));
	for (int sequence : {0, 31, 32, 63, -1, -32, INT_MAX, INT_MIN}) {
		CHECK(NativeClients::FrameIndex(sequence) == static_cast<int>(static_cast<unsigned>(sequence) & 31u));
		CHECK(clients.SetFrame(0, sequence, sequence & 7, sequence) && clients.SetOutgoingSequence(0, sequence));
		CHECK(clients.CurrentFrame(0, frame) && frame.count == (sequence & 7) && frame.first == sequence);
	}
	// A later sequence reads the frame its low five bits select, not its own.
	CHECK(clients.SetFrame(0, 1, 5, 77) && clients.SetOutgoingSequence(0, 33));
	CHECK(clients.CurrentFrame(0, frame) && frame.count == 5 && frame.first == 77);
	// The ring reproduces the original's signed remainder without wrapping low.
	int number = 0;
	std::size_t ringSlot = 0;
	CHECK(!clients.SnapshotCapacity() && !clients.SnapshotNumber(0, 0, number));
	CHECK(!clients.BindSnapshotRing(0) && !clients.BindSnapshotRing(-1) && clients.BindSnapshotRing(64));
	for (int i = 0; i < 64; ++i) {
		fntech3_q3_ihv_entityState_t state{};
		const auto word = static_cast<std::int16_t>(i * 37 - 80);
		std::memcpy(state.bytes, &word, sizeof word);
		CHECK(clients.SetSnapshotState(i, state));
	}
	CHECK(!clients.SetSnapshotState(-1, {}) && !clients.SetSnapshotState(64, {}));
	for (int first : {0, 1, 63, 64, 1000000}) for (int i = 0; i < 5; ++i) {
		CHECK(clients.SnapshotNumber(first, i, number) && number == ((first + i) % 64) * 37 - 80);
	}
	CHECK(!SnapshotRingSlot(0, 0, 0, ringSlot) && !SnapshotRingSlot(0, -1, 64, ringSlot));
	CHECK(SnapshotRingSlot(INT_MAX, 0, 64, ringSlot) && ringSlot == INT_MAX % 64);
	// The counter is unreduced, so it keeps selecting slots far past a wrap.
	const std::uint64_t far = (std::uint64_t(1) << 40) + 5;
	CHECK(SnapshotRingSlot(far, 3, 64, ringSlot) && ringSlot == (far + 3) % 64);
	CHECK(!SnapshotRingSlot((std::numeric_limits<std::uint64_t>::max)(), 1, 64, ringSlot));
	CHECK(!clients.SnapshotNumber(0, -1, number));
	// A frame whose entities the ring has overwritten cannot be delta'd against.
	CHECK(SnapshotEntitiesStale(0, 64, 64) && SnapshotEntitiesStale(3, 100, 64));
	CHECK(!SnapshotEntitiesStale(1, 64, 64) && !SnapshotEntitiesStale(40, 100, 64));
	CHECK(SnapshotEntitiesStale(0, 0, 0));
	// A request that is not positive, or more than twenty-eight messages old.
	CHECK(SnapshotDeltaStale(40, 0) && SnapshotDeltaStale(40, -1) && SnapshotDeltaStale(40, 11));
	CHECK(!SnapshotDeltaStale(40, 12) && !SnapshotDeltaStale(40, 40) && !SnapshotDeltaStale(0, 1));
	// The ring keeps the states it was given until the counter passes them.
	CHECK(clients.BindSnapshotRing(4) && !clients.SnapshotCursor());
	for (int i = 0; i < 6; ++i) {
		fntech3_q3_ihv_entityState_t state{};
		const auto word = static_cast<std::int16_t>(100 + i);
		std::memcpy(state.bytes, &word, sizeof word);
		CHECK(clients.PushSnapshotState(state));
	}
	// The original's rule is conservative at the boundary: a frame starting
	// exactly one capacity behind the counter is already refused.
	CHECK(clients.SnapshotCursor() == 6 && !clients.SnapshotEntitiesAvailable(1));
	CHECK(!clients.SnapshotEntitiesAvailable(2) && clients.SnapshotEntitiesAvailable(3));
	CHECK(clients.SnapshotNumber(4, 0, number) && number == 104);
	clients.Clear();
	CHECK(!clients.Count() && !clients.SnapshotCapacity() && !clients.CurrentFrame(0, frame));
}

int main() { Cvars(); EntityPointers(); Allocations(); ClientRecords(); std::puts("IHV native boundary checks passed"); }
