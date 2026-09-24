/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CLIENT_DATA_HPP
#define FNTECH3_Q3_IHV_CLIENT_DATA_HPP

#include "q3_ihv_client_abi.h"
#include "../qcommon/q3_ihv_message.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace fntech3::q3ihv::client {
enum class ReadResult { Unavailable, Ok, FutureEntity, InvalidOutput };

/* Native history getters compare signed dwords, including across wrap. Use
 * unsigned arithmetic followed by an explicit signed interpretation, without
 * invoking C++ signed-overflow or out-of-range conversion behavior. */
inline std::int64_t SignedSequence(std::uint32_t value) {
	return value < 0x80000000u ? std::int64_t(value) : std::int64_t(value) - 0x100000000ll;
}

template<class Record, std::size_t Count, bool CounterIsNext> class History {
	static_assert(Count && !(Count & (Count - 1)) && Count <= 0x7fffffffu);
	static_assert(std::is_trivially_copyable_v<Record>);
public:
	explicit History(std::uint32_t counter = 0) : counter_(counter) {}
	void Clear(std::uint32_t counter = 0) { records_.fill(Record{}); counter_ = counter; }
	std::uint32_t Counter() const { return counter_; }
	void Append(const Record &value) {
		const auto sequence = CounterIsNext ? counter_++ : ++counter_;
		records_[sequence & (Count - 1)] = value;
	}
	ReadResult Copy(std::uint32_t sequence, void *output, std::size_t bytes) const {
		if (!output || bytes < sizeof(Record)) return ReadResult::InvalidOutput;
		const auto requested = SignedSequence(sequence), current = SignedSequence(counter_);
		if constexpr (CounterIsNext) {
			if (requested >= current) return ReadResult::FutureEntity;
		} else if (requested > current) return ReadResult::Unavailable;
		if (requested <= SignedSequence(counter_ - static_cast<std::uint32_t>(Count))) return ReadResult::Unavailable;
		std::memmove(output, &records_[sequence & (Count - 1)], sizeof(Record));
		return ReadResult::Ok;
	}
private:
	std::array<Record, Count> records_{};
	std::uint32_t counter_;
};
using CommandHistory = History<fntech3_q3_ihv_usercmd_t, FNTECH3_Q3_IHV_COMMAND_HISTORY, false>;
using EntityHistory = History<fntech3_q3_ihv_entityState_t, FNTECH3_Q3_IHV_PARSE_ENTITIES, true>;

/* The engine side of the native cgame data imports. It owns exact external
 * records, not retail client/player/entity structures. The IHV driver must
 * validate the entire packet first, then apply its events in order. This
 * component calls no module, executes no server command, and reads no input.
 * Engine startup must select the coherent IHV profile before binding imports.
 */
class ClientData {
public:
	bool Ready() const { return ready_; }
	void Clear() {
		strings_.Clear(); commands_.Clear(); entities_.Clear();
		entitySequence_ = 0; snapshot_ = {}; ready_ = haveSnapshot_ = false;
	}
	bool BeginGameState(const ConfigStrings &strings) {
		if (!wire::Protocol33(strings.String(0))) return false;
		// Preserve aliased views of the previous gamestate across Clear().
		const ConfigStrings next = strings;
		Clear(); strings_ = next; ready_ = true;
		return true;
	}
	bool SetConfigstring(std::uint32_t index, std::string_view value, bool &changed) {
		if (!ready_ || index >= 1024 || value.size() > 1023
			|| (!index && !wire::Protocol33(value))) return false;
		return strings_.Set(index, value, changed);
	}
	const ConfigStrings &Configstrings() const { return strings_; }
	bool CopyGameState(void *output, std::size_t bytes) const {
		if (!ready_ || !output || bytes < sizeof(fntech3_q3_ihv_gameState_t)) return false;
		std::memmove(output, &strings_.Record(), sizeof(fntech3_q3_ihv_gameState_t));
		return true;
	}
	bool AppendCommand(const fntech3_q3_ihv_usercmd_t &command) {
		if (!ready_) return false;
		commands_.Append(command); return true;
	}
	std::uint32_t CurrentCommandNumber() const { return commands_.Counter(); }
	ReadResult CopyCommand(std::uint32_t sequence, void *output, std::size_t bytes) const {
		return ready_ ? commands_.Copy(sequence, output, bytes) : ReadResult::Unavailable;
	}
	ReadResult CopyEntity(std::uint32_t sequence, void *output, std::size_t bytes) const {
		return ready_ ? entities_.Copy(sequence, output, bytes) : ReadResult::Unavailable;
	}
	std::uint64_t EntitySequence() const { return entitySequence_; }
	const fntech3_q3_ihv_snapshot_t *CurrentSnapshot() const { return haveSnapshot_ ? &snapshot_ : nullptr; }
	bool PublishSnapshot(const wire::Snapshot &snapshot, std::uint32_t ping) {
		if (!ready_ || snapshot.firstEntity != entitySequence_ || snapshot.areaBytes > snapshot.areaMask.size()
			|| snapshot.entities.size() > 1023
			|| snapshot.entities.size() > (std::numeric_limits<std::uint64_t>::max)() - entitySequence_) return false;
		unsigned last = 0;
		for (const auto &entity : snapshot.entities) {
			const auto number = wire::Load(entity.data(), 2);
			if (number <= last || number >= 1024) return false;
			last = number;
		}
		fntech3_q3_ihv_snapshot_t next{};
		const auto put = [&](unsigned offset, std::uint32_t value) { wire::Store(next.bytes + offset, 4, value); };
		put(0, 1); put(4, snapshot.flags); put(8, snapshot.serverTime); put(12, snapshot.commandSequence);
		put(16, snapshot.messageNumber); put(20, snapshot.deltaNumber); put(24, ping);
		std::memcpy(next.bytes + 28, snapshot.areaMask.data(), snapshot.areaBytes);
		std::memcpy(next.bytes + 60, snapshot.player.data(), snapshot.player.size());
		put(336, snapshot.configSequence); put(340, static_cast<std::uint32_t>(snapshot.entities.size()));
		put(344, static_cast<std::uint32_t>(entitySequence_));
		// All checks precede history or snapshot publication; the remaining copies
		// and counter increments cannot fail or allocate.
		for (const auto &entity : snapshot.entities) {
			fntech3_q3_ihv_entityState_t record; std::memcpy(record.bytes, entity.data(), sizeof record.bytes);
			entities_.Append(record);
		}
		entitySequence_ += snapshot.entities.size(); snapshot_ = next; haveSnapshot_ = true;
		return true;
	}
private:
	ConfigStrings strings_;
	CommandHistory commands_;
	EntityHistory entities_;
	fntech3_q3_ihv_snapshot_t snapshot_{};
	std::uint64_t entitySequence_ = 0;
	bool ready_ = false, haveSnapshot_ = false;
};
}
#endif
