/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_SERVER_CLIENTS_HPP
#define FNTECH3_Q3_IHV_SERVER_CLIENTS_HPP

#include "q3_ihv_game_abi.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace fntech3::q3ihv {

/* Native server client records. The original keeps one 0xee18-byte array and
 * derives a slot from the entity's signed-short number. fnTech3 stores only the
 * fields its imports and bot scheduler read: the retail client_t, playerState_t
 * and entityState_t records have different layouts and must never alias native
 * storage. Observed addresses and behaviour: docs/fntech3/Q3_IHV.md (native
 * client lifecycle). */
enum {
	FNTECH3_Q3_IHV_CLIENT_FREE = 0,
	FNTECH3_Q3_IHV_CLIENT_ZOMBIE = 1,
	FNTECH3_Q3_IHV_CLIENT_CONNECTED = 2,
	FNTECH3_Q3_IHV_CLIENT_ACTIVE = 4,
	/* Frames are addressed by the outgoing sequence's low five bits. */
	FNTECH3_Q3_IHV_SNAPSHOT_FRAMES = 32,
	FNTECH3_Q3_IHV_USERINFO_BYTES = 1024,
	FNTECH3_Q3_IHV_USERINFO_LIMIT = 1023,
	FNTECH3_Q3_IHV_CLIENT_NAME_LIMIT = 31
};

/* SV_UserinfoChanged 0x418ab0 reads rate and snaps from the copied userinfo.
 * An absent or empty rate becomes 5000; anything else is the original atoi
 * clamped to 100..15000. An absent or empty snaps becomes 50ms; anything else
 * divides 1000 by that value after limiting it to 1..30. The name keeps the
 * original's 31-character strncpy limit, but fnTech3 always terminates it;
 * strncpy leaves a 31-character name running into the next client field. */
inline int UserinfoRate(const char *value) {
	if (!value || !*value) return 5000;
	const int rate = std::atoi(value);
	if (rate < 100) return 100;
	return rate > 15000 ? 15000 : rate;
}
inline int UserinfoSnapshotMsec(const char *value) {
	if (!value || !*value) return 50;
	int snaps = std::atoi(value);
	if (snaps < 1) snaps = 1;
	else if (snaps > 30) snaps = 30;
	return 1000 / snaps;
}

/* SnapshotEntityNumber 0x41b3b0 adds the index to the frame's first entity and
 * takes the x86 signed remainder by the ring capacity, so a negative sum keeps
 * its sign. Reproduce that without forming a negative array index. */
inline bool SnapshotRingSlot(std::uint64_t first, int index, int capacity, std::size_t &slot) {
	if (capacity < 1 || index < 0) return false;
	const auto sum = first + static_cast<std::uint64_t>(index);
	if (sum < first) return false;
	slot = static_cast<std::size_t>(sum % static_cast<std::uint64_t>(capacity));
	return true;
}

/* SV_WriteSnapshotToClient 0x41addd refuses a delta request that is not
 * positive or is more than 28 messages behind the one being written. */
inline bool SnapshotDeltaStale(int outgoingSequence, int deltaMessage) {
	if (deltaMessage <= 0) return true;
	const auto distance = static_cast<std::int64_t>(outgoingSequence) - deltaMessage;
	return distance >= FNTECH3_Q3_IHV_SNAPSHOT_FRAMES - 3;
}

/* 0x41ae40 refuses one whose entities the shared ring has already overwritten. */
inline bool SnapshotEntitiesStale(std::uint64_t first, std::uint64_t cursor, int capacity) {
	return capacity < 1 || first + static_cast<std::uint64_t>(capacity) <= cursor;
}

/* SV_CreateBaseline 0x4196a0 copies an entity into its baseline slot only when
 * the entity is in use and carries a model, a looping sound or effect flags.
 * A slot without one keeps its zero record, so its entity is sent delta'd
 * against zero exactly as the original sends it. */
inline bool BaselineQualifies(const fntech3_q3_ihv_entity_t &entity) {
	if (!entity.inuse) return false;
	std::uint16_t effects = 0;
	std::memcpy(&effects, entity.state + FNTECH3_Q3_IHV_ENTITY_EFFECTS, sizeof effects);
	return entity.state[FNTECH3_Q3_IHV_ENTITY_MODELINDEX] != 0
		|| entity.state[FNTECH3_Q3_IHV_ENTITY_LOOPSOUND] != 0 || effects != 0;
}

class NativeClients {
public:
	/* The original keeps an unreduced running counter in the frame so a delta
	 * request can tell whether the ring still holds that frame's entities. */
	struct SnapshotFrame { int count = 0; std::uint64_t first = 0; };
	/* The rest of the original's frame record: the player state copied from the
	 * game's own client storage and the area bits for the viewer's area. The
	 * getters never read these; the protocol-33 writer will. */
	struct SnapshotPayload {
		fntech3_q3_ihv_playerState_t playerState{};
		unsigned char areabits[FNTECH3_Q3_IHV_AREABITS_BYTES]{};
		int areabytes = 0;
	};

	/* Native clients are entities 1..count, so the array cannot cover every
	 * entity slot and the world entity is never a client. */
	bool Reset(int count, int entityCapacity) {
		Clear();
		if (count < 1 || entityCapacity < 1 || count >= entityCapacity) return false;
		return Grow(slots_, static_cast<std::size_t>(count));
	}
	void Clear() { slots_.clear(); ring_.clear(); cursor_ = 0; }
	int Count() const { return static_cast<int>(slots_.size()); }
	bool Valid(int slot) const { return slot >= 0 && slot < Count(); }

	/* The original derives the slot from the entity's own number word. Callers
	 * pass the number the engine's bounded entity array already agreed with. */
	bool SlotForNumber(int number, int &slot) const {
		if (number < 1 || number > Count()) return false;
		slot = number - 1;
		return true;
	}

	int State(int slot) const { return Valid(slot) ? At(slot).state : -1; }
	int LastPacketTime(int slot) const { return Valid(slot) ? At(slot).lastPacketTime : 0; }
	int Rate(int slot) const { return Valid(slot) ? At(slot).rate : 0; }
	int SnapshotMsec(int slot) const { return Valid(slot) ? At(slot).snapshotMsec : 0; }
	/* SV_SendClientMessages 0x41b735 builds the next snapshot only once the
	 * server time reaches this, and 0x41b690 pushes it forward afterwards. */
	int NextSnapshotTime(int slot) const { return Valid(slot) ? At(slot).nextSnapshotTime : 0; }
	/* Header state the snapshot writer reads: the message the client asked to
	 * delta against, the sequences it has acknowledged and its snapshot flags.
	 * A connected session sets these; nothing else in fnTech3 does yet. */
	int DeltaMessage(int slot) const { return Valid(slot) ? At(slot).deltaMessage : 0; }
	int ConfigSequence(int slot) const { return Valid(slot) ? At(slot).configSequence : 0; }
	int CommandSequence(int slot) const { return Valid(slot) ? At(slot).commandSequence : 0; }
	int SnapshotFlags(int slot) const { return Valid(slot) ? At(slot).snapshotFlags : 0; }
	bool SetDeltaMessage(int slot, int message) {
		if (!Valid(slot)) return false;
		At(slot).deltaMessage = message;
		return true;
	}
	bool SetSequences(int slot, int configSequence, int commandSequence, int flags) {
		if (!Valid(slot) || flags < 0 || flags > 255) return false;
		Client &client = At(slot);
		client.configSequence = configSequence;
		client.commandSequence = commandSequence;
		client.snapshotFlags = flags;
		return true;
	}
	bool SetNextSnapshotTime(int slot, int time) {
		if (!Valid(slot)) return false;
		At(slot).nextSnapshotTime = time;
		return true;
	}
	const char *Userinfo(int slot) const { return Valid(slot) ? At(slot).userinfo : ""; }
	/* The original hands the module its own stored copy, which the callback may
	 * rewrite in place before the engine rereads it. */
	char *MutableUserinfo(int slot) { return Valid(slot) ? At(slot).userinfo : nullptr; }
	const char *Name(int slot) const { return Valid(slot) ? At(slot).name : ""; }
	bool MessageCleared(int slot) const { return Valid(slot) && At(slot).messageCleared; }
	/* The scheduler at 0x417800 tests the stored entity pointer, which a
	 * released bot keeps, rather than the client's state. */
	bool HasEntity(int slot) const { return Valid(slot) && At(slot).entityBound; }

	/* BotAllocateClient 0x417640 takes the first free slot. A zombie is not
	 * free, so a released bot keeps its slot until the host reclaims it. */
	int FirstFreeSlot() const {
		for (int slot = 0; slot < Count(); ++slot)
			if (At(slot).state == FNTECH3_Q3_IHV_CLIENT_FREE) return slot;
		return -1;
	}
	bool Connect(int slot, int time) {
		if (!Valid(slot) || At(slot).state != FNTECH3_Q3_IHV_CLIENT_FREE) return false;
		Client &client = At(slot);
		client.state = FNTECH3_Q3_IHV_CLIENT_CONNECTED;
		client.lastPacketTime = time;
		client.messageCleared = true;
		client.entityBound = true;
		return true;
	}
	/* BotFreeClient 0x4176e0 only marks the slot and clears the visible name;
	 * it calls no game export and retains the entity association. */
	bool Release(int slot) {
		if (!Valid(slot) || At(slot).state == FNTECH3_Q3_IHV_CLIENT_FREE) return false;
		Client &client = At(slot);
		client.state = FNTECH3_Q3_IHV_CLIENT_ZOMBIE;
		client.name[0] = 0;
		return true;
	}
	/* BotSetUserinfo 0x417720 copies at most 1023 bytes before the module
	 * callback runs, then SV_UserinfoChanged 0x418ab0 rereads that copy. */
	bool CopyUserinfo(int slot, const char *text) {
		if (!Valid(slot) || !text) return false;
		Client &client = At(slot);
		const std::size_t length = Bounded(text, FNTECH3_Q3_IHV_USERINFO_LIMIT);
		std::memcpy(client.userinfo, text, length);
		client.userinfo[length] = 0;
		return true;
	}
	bool ApplyUserinfo(int slot, const char *name, const char *rate, const char *snaps) {
		if (!Valid(slot)) return false;
		Client &client = At(slot);
		const std::size_t length = name ? Bounded(name, FNTECH3_Q3_IHV_CLIENT_NAME_LIMIT) : 0;
		if (length) std::memcpy(client.name, name, length);
		client.name[length] = 0;
		client.rate = UserinfoRate(rate);
		client.snapshotMsec = UserinfoSnapshotMsec(snaps);
		return true;
	}

	/* Per-client bot step 0x417780 clears the outgoing message, refreshes the
	 * last-packet time and promotes a client that is not active yet. */
	enum class BotStep { None, Begin, Think };
	BotStep StepBot(int slot, int time) {
		if (!Valid(slot)) return BotStep::None;
		Client &client = At(slot);
		client.messageCleared = true;
		client.lastPacketTime = time;
		if (client.state == FNTECH3_Q3_IHV_CLIENT_ACTIVE) return BotStep::Think;
		client.state = FNTECH3_Q3_IHV_CLIENT_ACTIVE;
		return BotStep::Begin;
	}

	/* The original allocates one shared ring of maxclients * 2048 native entity
	 * states at 0x419737 and stores each selected entity at the running
	 * nextSnapshotEntities counter reduced by that capacity (0x41b30a), while
	 * the frame keeps the unreduced counter. Binding stays with the snapshot
	 * builder, so an empty ring means no native snapshot has been built. */
	bool BindSnapshotRing(int capacity) {
		ring_.clear();
		cursor_ = 0;
		return capacity >= 1 && Grow(ring_, static_cast<std::size_t>(capacity));
	}
	int SnapshotCapacity() const { return static_cast<int>(ring_.size()); }
	/* Each state is stored at the running counter reduced by the capacity. The
	 * counter itself keeps growing, as the original's does, so a delta request
	 * can be refused once the ring has overwritten its entities. fnTech3 counts
	 * in 64 bits, so it cannot reach the wrap the original's int would. */
	std::uint64_t SnapshotCursor() const { return cursor_; }
	bool PushSnapshotState(const fntech3_q3_ihv_entityState_t &state) {
		if (!SnapshotCapacity()) return false;
		ring_[static_cast<std::size_t>(cursor_ % static_cast<std::uint64_t>(SnapshotCapacity()))] = state;
		++cursor_;
		return true;
	}
	bool SnapshotEntitiesAvailable(std::uint64_t first) const {
		return !SnapshotEntitiesStale(first, cursor_, SnapshotCapacity());
	}
	bool SnapshotState(std::uint64_t first, int index, fntech3_q3_ihv_entityState_t &state) const {
		std::size_t slot = 0;
		if (!SnapshotRingSlot(first, index, SnapshotCapacity(), slot)) return false;
		state = ring_[slot];
		return true;
	}
	bool SetSnapshotState(int slot, const fntech3_q3_ihv_entityState_t &state) {
		if (slot < 0 || slot >= SnapshotCapacity()) return false;
		ring_[static_cast<std::size_t>(slot)] = state;
		return true;
	}
	bool SnapshotNumber(std::uint64_t first, int index, int &number) const {
		std::size_t slot = 0;
		if (!SnapshotRingSlot(first, index, SnapshotCapacity(), slot)) return false;
		std::int16_t stored = 0;
		std::memcpy(&stored, ring_[slot].bytes, sizeof stored);
		number = stored;
		return true;
	}

	int OutgoingSequence(int slot) const { return Valid(slot) ? At(slot).outgoingSequence : 0; }
	bool SetOutgoingSequence(int slot, int sequence) {
		if (!Valid(slot)) return false;
		At(slot).outgoingSequence = sequence;
		return true;
	}
	/* The getters address the history with the low five bits of the outgoing
	 * sequence, which selects the frame the original most recently built. */
	static int FrameIndex(int sequence) { return static_cast<int>(static_cast<unsigned>(sequence) & 0x1fu); }
	bool FrameAt(int slot, int sequence, SnapshotFrame &frame) const {
		if (!Valid(slot)) return false;
		frame = At(slot).frames[static_cast<std::size_t>(FrameIndex(sequence))];
		return true;
	}
	bool CurrentFrame(int slot, SnapshotFrame &frame) const {
		if (!Valid(slot)) return false;
		const Client &client = At(slot);
		frame = client.frames[static_cast<std::size_t>(FrameIndex(client.outgoingSequence))];
		return true;
	}
	bool SetFrame(int slot, int sequence, int count, std::uint64_t first) {
		if (!Valid(slot) || count < 0) return false;
		At(slot).frames[static_cast<std::size_t>(FrameIndex(sequence))] = {count, first};
		return true;
	}
	bool SetFramePayload(int slot, int sequence, const fntech3_q3_ihv_playerState_t &playerState,
		const unsigned char *areabits, int areabytes) {
		if (!Valid(slot) || !areabits || areabytes < 0
			|| areabytes > FNTECH3_Q3_IHV_AREABITS_BYTES) return false;
		SnapshotPayload &payload = At(slot).payloads[static_cast<std::size_t>(FrameIndex(sequence))];
		payload.playerState = playerState;
		std::memcpy(payload.areabits, areabits, sizeof payload.areabits);
		payload.areabytes = areabytes;
		return true;
	}
	bool FramePayload(int slot, int sequence, SnapshotPayload &payload) const {
		if (!Valid(slot)) return false;
		payload = At(slot).payloads[static_cast<std::size_t>(FrameIndex(sequence))];
		return true;
	}

private:
	/* A rejected size must leave an empty, usable object: the host turns that
	 * into an engine error instead of an exception crossing the module ABI. */
	template<class T> static bool Grow(std::vector<T> &storage, std::size_t count) {
		if (count > storage.max_size()) return false;
		try { storage.resize(count); } catch (...) { storage.clear(); return false; }
		return true;
	}
	static std::size_t Bounded(const char *text, std::size_t limit) {
		std::size_t length = 0;
		while (length < limit && text[length]) ++length;
		return length;
	}
	struct Client {
		int state = FNTECH3_Q3_IHV_CLIENT_FREE;
		int lastPacketTime = 0;
		int outgoingSequence = 0;
		int rate = 0;
		int snapshotMsec = 0;
		int nextSnapshotTime = 0;
		int deltaMessage = 0;
		int configSequence = 0;
		int commandSequence = 0;
		int snapshotFlags = 0;
		bool messageCleared = false;
		bool entityBound = false;
		char userinfo[FNTECH3_Q3_IHV_USERINFO_BYTES]{};
		char name[FNTECH3_Q3_IHV_CLIENT_NAME_LIMIT + 1]{};
		SnapshotFrame frames[FNTECH3_Q3_IHV_SNAPSHOT_FRAMES]{};
		SnapshotPayload payloads[FNTECH3_Q3_IHV_SNAPSHOT_FRAMES]{};
	};
	Client &At(int slot) { return slots_[static_cast<std::size_t>(slot)]; }
	const Client &At(int slot) const { return slots_[static_cast<std::size_t>(slot)]; }
	std::vector<Client> slots_;
	std::vector<fntech3_q3_ihv_entityState_t> ring_;
	std::uint64_t cursor_ = 0;
};

} // namespace fntech3::q3ihv
#endif
