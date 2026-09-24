/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_IHV_SESSION_HPP
#define FNTECH3_CLIENT_IHV_SESSION_HPP

#include "../compat/q3_ihv_client_data.hpp"
#include <memory>

namespace fntech3::q3ihv::client {
/* The native module dispatcher implements every callback. These events do not
 * initialize modules or execute commands by themselves. Import reads during a
 * callback use the supplied ClientData, whose address stays stable. */
class ClientEvents {
public:
	virtual ~ClientEvents() = default;
	virtual void GameState(const ClientData &) = 0;
	virtual void Configstring(const ClientData &, std::uint32_t index) = 0;
	virtual void ServerCommand(const ClientData &, std::string_view text) = 0;
	virtual void Snapshot(const ClientData &, const fntech3_q3_ihv_snapshot_t &) = 0;
};
struct PacketResult {
	enum Code { Delivered, Rejected, StateMismatch, Reentrant, Aborted } code;
	wire::MessageResult message = wire::MessageResult::Ok;
};

/* A packet is decoded and every native publication checked on a private copy
 * before any module callback. Successful packets are then applied in order, so
 * an earlier snapshot cannot observe a later configstring update. Reentrant
 * packets are rejected for the driver to queue. Callback exceptions invalidate
 * the session and propagate. Engine error adapters must unwind Feed before
 * invoking a non-C++ abort: longjmp must never cross these C++ scopes.
 * This remains separate from the retail cl/gameState/snapshot objects. */
class ClientSession {
public:
	ClientSession() = default;
	ClientSession(const ClientSession &) = delete;
	ClientSession &operator=(const ClientSession &) = delete;
	const ClientData &Data() const { return data_; }
	bool AppendCommand(const fntech3_q3_ihv_usercmd_t &command) { return data_.AppendCommand(command); }
	void Clear() { decoder_.Clear(); data_.Clear(); ++generation_; }
	PacketResult Feed(const void *input, std::size_t bytes, std::uint32_t ping, ClientEvents &events) {
		if (processing_) return {PacketResult::Reentrant};
		struct Processing {
			bool &flag;
			explicit Processing(bool &value) : flag(value) { flag = true; }
			~Processing() { flag = false; }
		} processing(processing_);
		wire::MessageDecoder next = decoder_; wire::DecodedMessage message;
		const auto status = next.Decode(input, bytes, message);
		if (status != wire::MessageResult::Ok) return {PacketResult::Rejected, status};
		const auto generation = generation_;
		auto pending = std::make_unique<ClientData>(data_);
		for (const auto &event : message.events) {
			bool notify;
			if (!Apply(*pending, message, event, ping, notify)) return {PacketResult::StateMismatch};
		}
		struct Delivery {
			ClientSession &session;
			bool complete = false;
			~Delivery() { if (!complete) session.Clear(); }
		} delivery{*this};
		for (const auto &event : message.events) {
			bool notify;
			if (!Apply(data_, message, event, ping, notify)) return {PacketResult::StateMismatch};
			if (!notify) continue;
			switch (event.type) {
			case wire::MessageEvent::Type::GameState: events.GameState(data_); break;
			case wire::MessageEvent::Type::Configstring: events.Configstring(data_, message.configstrings[event.index].index); break;
			case wire::MessageEvent::Type::ServerCommand: events.ServerCommand(data_, message.commands[event.index]); break;
			case wire::MessageEvent::Type::Snapshot: events.Snapshot(data_, *data_.CurrentSnapshot()); break;
			case wire::MessageEvent::Type::Nop: break;
			}
			if (generation != generation_) { delivery.complete = true; return {PacketResult::Aborted}; }
		}
		decoder_ = std::move(next);
		delivery.complete = true;
		return {PacketResult::Delivered};
	}
private:
	static bool Apply(ClientData &data, const wire::DecodedMessage &message,
		const wire::MessageEvent &event, std::uint32_t ping, bool &notify) {
		notify = true;
		switch (event.type) {
		case wire::MessageEvent::Type::Nop: notify = false; return true;
		case wire::MessageEvent::Type::GameState:
			return message.gamestate && data.BeginGameState(message.gamestate->nativeConfigstrings);
		case wire::MessageEvent::Type::Configstring: {
			if (event.index >= message.configstrings.size()) return false;
			const auto &update = message.configstrings[event.index];
			return data.SetConfigstring(update.index, update.value, notify) && notify == update.changed;
		}
		case wire::MessageEvent::Type::ServerCommand: return event.index < message.commands.size();
		case wire::MessageEvent::Type::Snapshot: return message.snapshot && data.PublishSnapshot(*message.snapshot, ping);
		}
		return false;
	}
	ClientData data_;
	wire::MessageDecoder decoder_;
	std::uint64_t generation_ = 0;
	bool processing_ = false;
};
}
#endif
