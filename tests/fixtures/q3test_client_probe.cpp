/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* A headless Q3Test client for server interoperability probes.
 *
 * Q3Test 1.02 and 1.03 shipped only as dedicated Win32 servers, so no retail
 * client exists to join them. This probe speaks a release's client side of
 * the protocol through fnTech3's own codecs -- the connectionless handshake,
 * the netchan family (code/compat/q3test_netchan.hpp) and the message bodies
 * (code/qcommon/q3test_wire.hpp) -- and runs the same against a retail server
 * and fntech3.ded.exe, so each side's behaviour for one client can be
 * compared. It joins, walks forward, turns, acknowledges what it receives
 * and leaves, then prints a JSON summary. Every send rule follows the retail
 * client as docs/fntech3/Q3TEST.md records it (CL_CheckForResend,
 * CL_WritePacket, CL_Disconnect).
 *
 *   fntech3_q3test_client_probe --protocol 35 --server 127.0.0.1:27960
 *       [--port 27990] [--seconds 8] [--name Probe]
 */
#include "../../code/compat/q3test_netchan.hpp"
#include "../../code/qcommon/q3test_wire.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static const socket_t kBadSocket = INVALID_SOCKET;
static void CloseSocket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static const socket_t kBadSocket = -1;
static void CloseSocket(socket_t s) { close(s); }
#endif

namespace q3wire = fntech3::q3test::wire;
using fntech3::q3test::Netchan;
using fntech3::q3test::NetchanResult;
using fntech3::q3test::NetchanSink;
using fntech3::q3test::NetchanSource;

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
	int protocol = 0;
	std::string server = "127.0.0.1:27960";
	int port = 27990;
	double seconds = 8;
	std::string name = "Q3TestProbe";
};

struct Probe {
	Options options;
	socket_t socket = kBadSocket;
	sockaddr_in server{};
	Clock::time_point start;
	int Now() const { return static_cast<int>( std::chrono::duration_cast<std::chrono::milliseconds>( Clock::now() - start ).count() ); }

	// session
	Netchan chan;
	std::unique_ptr<q3wire::ClientState> state;
	std::vector<std::uint8_t> pending;       // 35/36: the reliable block being built
	std::size_t pendingSize = 0;
	int challenge = 0;
	bool connected = false;
	int qport = 0;

	// what the server said
	std::map<int, std::string> configstrings;
	int gamestates = 0, baselines = 0;
	int serverId = 0;
	int serverCommandSequence = 0;       // 38-40
	int reliableSequence = 0;            // 38-40: our commands
	int reliableAcknowledge = 0;         // 38-40: the last the server ran
	int timeDelta = 0;                   // 40: server time less ours
	std::vector<std::string> reliableCommands{64};
	int csAck = -1;                      // 35/36
	int snapshots = 0, fullSnapshots = 0, invalidSnapshots = 0;
	int lastMessageNum = 0, firstServerTime = 0, lastServerTime = 0;
	bool haveSnapshot = false;
	float firstOrigin[3] = {}, lastOrigin[3] = {};
	bool haveOrigin = false;
	int clientNum = -1;                  // the slot the server gave us
	int maxEntities = 0;
	std::vector<std::string> commands;   // server commands, first 16
	int commandCount = 0;
	std::vector<std::string> prints;     // out-of-band prints
	std::string error;
	bool serverDisconnected = false;

	// usercmds
	std::vector<q3wire::Usercmd> cmds;   // index = number
	int lastSentCmd = 0;
	int lastCmdTime = 0;
	int packetsSent = 0, packetsReceived = 0;
};

bool Legacy(const Probe &p) { return q3wire::IsLegacy(p.options.protocol); }

void SendRaw(Probe &p, const void *data, std::size_t size) {
	sendto(p.socket, static_cast<const char *>(data), static_cast<int>(size), 0,
		reinterpret_cast<const sockaddr *>(&p.server), sizeof(p.server));
}

bool SinkSend(void *context, const unsigned char *data, std::size_t size) {
	SendRaw(*static_cast<Probe *>(context), data, size);
	return true;
}

void SendOutOfBand(Probe &p, const std::string &text) {
	std::string packet("\xff\xff\xff\xff", 4);
	packet += text;
	SendRaw(p, packet.data(), packet.size());
}

std::string Info(std::map<std::string, std::string> keys) {
	std::string out;
	for (const auto &kv : keys) out += "\\" + kv.first + "\\" + kv.second;
	return out;
}

std::string InfoValue(const std::string &info, const std::string &key) {
	std::size_t at = 0;
	while (at < info.size()) {
		if (info[at] == '\\') ++at;
		const std::size_t keyEnd = info.find('\\', at);
		if (keyEnd == std::string::npos) break;
		const std::string k = info.substr(at, keyEnd - at);
		const std::size_t valueEnd = info.find('\\', keyEnd + 1);
		const std::string v = info.substr(keyEnd + 1, valueEnd == std::string::npos ? std::string::npos : valueEnd - keyEnd - 1);
		if (k == key) return v;
		if (valueEnd == std::string::npos) break;
		at = valueEnd;
	}
	return {};
}

/* CL_CheckForResend's connect text: 35 carries the protocol, qport and
 * challenge as words, 36 puts them in the userinfo as protocol, port and
 * challenge, 38-40 as protocol, qport and challenge. */
void SendConnect(Probe &p) {
	std::map<std::string, std::string> info{{"name", p.options.name}, {"rate", "25000"}, {"snaps", "20"},
		{"model", "visor"}, {"color", "4"}, {"handicap", "100"}};
	const int protocol = p.options.protocol;
	if (protocol == 35) {
		SendOutOfBand(p, "connect 35 " + std::to_string(p.qport) + " " + std::to_string(p.challenge) + " \"" + Info(info) + "\"");
		return;
	}
	info["protocol"] = std::to_string(protocol);
	info[protocol == 36 ? "port" : "qport"] = std::to_string(p.qport);
	info["challenge"] = std::to_string(p.challenge);
	SendOutOfBand(p, "connect \"" + Info(info) + "\"");
}

/* The release's usercmd record (CL_FinishMove): msec or serverTime, buttons,
 * weapon, three angles and the moves. The probe runs forward and turns. */
q3wire::Usercmd MakeCmd(Probe &p, int number, int time) {
	q3wire::Usercmd out{};
	const int protocol = p.options.protocol;
	int msec = time - p.lastCmdTime;
	if (msec < 0) msec = 0;
	if (msec > 250) msec = 100;
	p.lastCmdTime = time;
	const int yaw = (number * 64) & 0xffff;  // a slow turn
	const auto forward = static_cast<std::uint8_t>(127);
	if (protocol == 40) {
		q3wire::Store(out.data(), 4, static_cast<std::uint32_t>(time + p.timeDelta));
		out[5] = 2;  // machinegun
		q3wire::Store(out.data() + 12, 4, static_cast<std::uint32_t>(yaw));
		out[20] = forward;
	} else if (protocol == 35) {
		out[0] = static_cast<std::uint8_t>(msec);
		out[2] = 2;
		q3wire::Store(out.data() + 6, 2, static_cast<std::uint32_t>(yaw));
		out[10] = forward;
	} else {
		out[0] = static_cast<std::uint8_t>(msec);
		out[2] = 2;
		q3wire::Store(out.data() + 8, 4, static_cast<std::uint32_t>(yaw));
		out[16] = forward;
	}
	return out;
}

void Transmit(Probe &p, const std::uint8_t *data, std::size_t size) {
	if (Legacy(p) && p.pendingSize && !p.chan.ReliableInFlight() && !p.chan.PendingReliable()) {
		p.chan.QueueReliable(p.pending.data(), p.pendingSize);
		p.pendingSize = 0;
	}
	NetchanSink sink{&p, SinkSend};
	p.chan.Transmit(sink, data, size);
	++p.packetsSent;
}

/* A client command: 35/36 append it to the reliable block, 38-40 ring it. */
void QueueCommand(Probe &p, const std::string &text) {
	if (Legacy(p)) {
		q3wire::Writer out(p.pending.data() + p.pendingSize, p.pending.size() - p.pendingSize);
		if (q3wire::WriteClientCommand(out, p.options.protocol, 0, text)) p.pendingSize += out.Size();
		return;
	}
	++p.reliableSequence;
	p.reliableCommands[static_cast<std::size_t>(p.reliableSequence & 63)] = text;
}

/* CL_WritePacket: 38-40 repeat their unacknowledged commands (all of them;
 * the probe sends few), then a move of every new command; 35/36 carry a
 * provisional last command. */
void WritePacket(Probe &p, bool withMove) {
	const int protocol = p.options.protocol;
	std::vector<std::uint8_t> buffer(q3wire::MaxMessageBytes);
	q3wire::Writer out(buffer.data(), Legacy(p) ? 1024 : buffer.size());
	if (!Legacy(p)) {
		for (int sequence = std::max(p.reliableAcknowledge + 1, p.reliableSequence - 63); sequence <= p.reliableSequence; ++sequence)
			q3wire::WriteClientCommand(out, protocol, sequence, p.reliableCommands[static_cast<std::size_t>(sequence & 63)]);
	}
	if (withMove) {
		const int number = static_cast<int>(p.cmds.size());
		p.cmds.push_back(MakeCmd(p, number, p.Now()));
		q3wire::Move move;
		move.serverCommandSequence = p.serverCommandSequence;
		move.serverId = p.serverId;
		move.clientTime = p.Now();
		move.deltaMessage = p.haveSnapshot ? p.lastMessageNum : -1;
		move.csAck = p.csAck;
		move.lastCmd = number;
		int first = std::max(p.lastSentCmd + 1, number - 15);
		for (int n = first; n <= number; ++n) move.cmds.push_back(p.cmds[static_cast<std::size_t>(n)]);
		if (Legacy(p)) {
			q3wire::Usercmd partial = move.cmds.back();
			if (protocol != 40) partial[0] = 0;
			move.cmds.push_back(partial);
		}
		q3wire::WriteMove(out, protocol, move);
		p.lastSentCmd = number;
	}
	Transmit(p, buffer.data(), out.Size());
}

void Parse(Probe &p, const std::uint8_t *data, std::size_t size) {
	q3wire::ServerMessage message;
	std::string error;
	if (!q3wire::ParseServerMessage(p.options.protocol, data, size, *p.state, message, &error)) {
		if (p.error.empty()) p.error = "parse: " + error;
		return;
	}
	for (const q3wire::ServerOp &op : message.ops) {
		if (const auto *game = std::get_if<q3wire::Gamestate>(&op)) {
			++p.gamestates;
			p.configstrings.clear();
			p.baselines = 0;
			for (const q3wire::GamestateEntry &entry : game->entries) {
				if (entry.command == 3) p.configstrings[entry.index] = entry.text;
				else ++p.baselines;
			}
			if (!Legacy(p)) p.serverCommandSequence = game->sequence;
			p.csAck = p.options.protocol == 36 ? game->sequence : -1;
			p.serverId = std::atoi(InfoValue(p.configstrings[1], "sv_serverid").c_str());
			p.haveSnapshot = false;
		} else if (const auto *update = std::get_if<q3wire::ConfigstringUpdate>(&op)) {
			p.configstrings[update->index] = update->text;
			if (update->index == 1) p.serverId = std::atoi(InfoValue(update->text, "sv_serverid").c_str());
		} else if (const auto *command = std::get_if<q3wire::ServerCommand>(&op)) {
			if (!Legacy(p)) {
				if (!command->fresh) continue;
				p.serverCommandSequence = command->sequence;
			}
			++p.commandCount;
			if (p.commands.size() < 16) p.commands.push_back(command->text);
			if (command->text.rfind("disconnect", 0) == 0) p.serverDisconnected = true;
			// 38-40: a configstring arrives as a command (CL_GetServerCommand)
			if (!Legacy(p) && command->text.rfind("cs ", 0) == 0) {
				const int index = std::atoi(command->text.c_str() + 3);
				const std::size_t quote = command->text.find('"');
				const std::size_t end = command->text.rfind('"');
				if (quote != std::string::npos && end > quote) {
					p.configstrings[index] = command->text.substr(quote + 1, end - quote - 1);
					if (index == 1) p.serverId = std::atoi(InfoValue(p.configstrings[1], "sv_serverid").c_str());
				}
			}
		} else if (const auto *snap = std::get_if<q3wire::Snapshot>(&op)) {
			if (!snap->Valid()) {
				++p.invalidSnapshots;
				continue;
			}
			++p.snapshots;
			if (snap->DeltaNum() < 1) ++p.fullSnapshots;
			if (!p.haveSnapshot && !p.snapshots) p.firstServerTime = snap->serverTime;
			if (!p.haveSnapshot) p.timeDelta = snap->serverTime - p.Now();
			if (!Legacy(p) && snap->reliableAcknowledge <= p.reliableSequence) p.reliableAcknowledge = snap->reliableAcknowledge;
			p.haveSnapshot = true;
			p.lastMessageNum = snap->messageNum;
			p.lastServerTime = snap->serverTime;
			if (Legacy(p)) p.csAck = snap->csSequence;
			p.maxEntities = std::max(p.maxEntities, static_cast<int>(snap->entities.size()));
			// the origin's place per release (code/compat/q3test_state_adapter.hpp)
			const std::size_t origin = p.options.protocol == 35 ? 0x08 : p.options.protocol == 40 ? 20 : 16;
			float o[3];
			std::memcpy(o, snap->player.data() + origin, sizeof(o));
			if (!p.haveOrigin) std::memcpy(p.firstOrigin, o, sizeof(o));
			std::memcpy(p.lastOrigin, o, sizeof(o));
			p.haveOrigin = true;
			// and the player's own number, from the same layouts
			const std::size_t number = p.options.protocol == 35 ? 0x44 : p.options.protocol == 36 ? 96
				: p.options.protocol == 40 ? 136 : 128;
			std::int32_t own;
			std::memcpy(&own, snap->player.data() + number, sizeof(own));
			p.clientNum = own;
		}
	}
}

void HandleOutOfBand(Probe &p, const std::uint8_t *data, std::size_t size) {
	std::string text(reinterpret_cast<const char *>(data) + 4, size - 4);
	const std::size_t newline = text.find('\n');
	const std::string line = text.substr(0, newline);
	if (line.rfind("challengeResponse", 0) == 0) {
		if (p.connected || p.challenge) return;
		p.challenge = std::atoi(line.c_str() + 17);
		SendConnect(p);
	} else if (line.rfind("connectResponse", 0) == 0) {
		if (p.connected) return;
		p.connected = true;
		p.chan.Setup(Legacy(p), NetchanSource::Client, p.qport);
		p.state = std::make_unique<q3wire::ClientState>(p.options.protocol);
	} else if (line.rfind("print", 0) == 0) {
		p.prints.push_back(newline == std::string::npos ? std::string() : text.substr(newline + 1));
	} else if (line.rfind("disconnect", 0) == 0) {
		// retail never honours it (net_transport.md 5.9); noted only
		p.prints.push_back("<oob disconnect>");
	}
}

void Receive(Probe &p) {
	std::vector<std::uint8_t> buffer(q3wire::MaxMessageBytes + 64);
	for (;;) {
		sockaddr_in from{};
#ifdef _WIN32
		int fromLength = sizeof(from);
#else
		socklen_t fromLength = sizeof(from);
#endif
		const int got = recvfrom(p.socket, reinterpret_cast<char *>(buffer.data()), static_cast<int>(buffer.size()), 0,
			reinterpret_cast<sockaddr *>(&from), &fromLength);
		if (got <= 0) return;
		++p.packetsReceived;
		if (got >= 4 && std::memcmp(buffer.data(), "\xff\xff\xff\xff", 4) == 0) {
			HandleOutOfBand(p, buffer.data(), static_cast<std::size_t>(got));
			continue;
		}
		if (!p.connected || !p.state) continue;
		std::size_t size = static_cast<std::size_t>(got);
		if (p.chan.Process(buffer.data(), size, buffer.size()) != NetchanResult::Message) continue;
		Parse(p, buffer.data(), size);
	}
}

/* CL_Disconnect: 35/36 send the command three times unreliably, 38-40 queue
 * it and write three packets. */
void Disconnect(Probe &p) {
	if (!p.connected) return;
	if (Legacy(p)) {
		for (int i = 0; i < 3; ++i) {
			std::uint8_t buffer[64];
			q3wire::Writer out(buffer, sizeof(buffer));
			q3wire::WriteClientCommand(out, p.options.protocol, 0, "disconnect");
			NetchanSink sink{&p, SinkSend};
			p.chan.Transmit(sink, buffer, out.Size());
		}
		return;
	}
	QueueCommand(p, "disconnect");
	for (int i = 0; i < 3; ++i) WritePacket(p, false);
}

std::string Json(const std::string &text) {
	std::string out = "\"";
	for (unsigned char c : text) {
		if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
		else if (c < 0x20 || c > 0x7e) { char hex[8]; std::snprintf(hex, sizeof(hex), "\\u%04x", c); out += hex; }
		else out += static_cast<char>(c);
	}
	return out + "\"";
}

bool ParseArguments(int argc, char **argv, Options &options) {
	for (int i = 1; i < argc; ++i) {
		const std::string key = argv[i];
		if (i + 1 >= argc) return false;
		const std::string value = argv[++i];
		if (key == "--protocol") options.protocol = std::atoi(value.c_str());
		else if (key == "--server") options.server = value;
		else if (key == "--port") options.port = std::atoi(value.c_str());
		else if (key == "--seconds") options.seconds = std::atof(value.c_str());
		else if (key == "--name") options.name = value;
		else return false;
	}
	return q3wire::IsProtocol(options.protocol) && options.port > 0 && options.port < 65536 && options.seconds > 0;
}

} // namespace

int main(int argc, char **argv) {
	Probe p;
	if (!ParseArguments(argc, argv, p.options)) {
		std::fprintf(stderr, "usage: %s --protocol 35|36|38|39|40 [--server ip:port] [--port n] [--seconds s] [--name n]\n", argv[0]);
		return 2;
	}
#ifdef _WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 3;
#endif
	const std::size_t colon = p.options.server.find(':');
	p.server.sin_family = AF_INET;
	p.server.sin_port = htons(static_cast<unsigned short>(colon == std::string::npos ? 27960 : std::atoi(p.options.server.c_str() + colon + 1)));
	inet_pton(AF_INET, p.options.server.substr(0, colon).c_str(), &p.server.sin_addr);
	p.socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (p.socket == kBadSocket) return 3;
	sockaddr_in local{};
	local.sin_family = AF_INET;
	local.sin_port = htons(static_cast<unsigned short>(p.options.port));
	inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
	if (bind(p.socket, reinterpret_cast<const sockaddr *>(&local), sizeof(local))) {
		std::fprintf(stderr, "bind to 127.0.0.1:%d failed\n", p.options.port);
		return 3;
	}
#ifdef _WIN32
	u_long nonblocking = 1;
	ioctlsocket(p.socket, FIONBIO, &nonblocking);
#else
	fcntl(p.socket, F_SETFL, fcntl(p.socket, F_GETFL, 0) | O_NONBLOCK);
#endif
	p.start = Clock::now();
	p.qport = p.options.port & 0xffff;
	p.pending.assign(fntech3::q3ihv::FNTECH3_Q3_IHV_NETCHAN_BYTES, 0);
	p.cmds.emplace_back();  // commands are numbered from 1

	const int end = static_cast<int>(p.options.seconds * 1000);
	int nextResend = 0, nextPacket = 0;
	while (p.Now() < end && !p.serverDisconnected) {
		Receive(p);
		const int now = p.Now();
		if (!p.connected) {
			// getchallenge while connecting, connect while challenging, every 3 s
			if (now >= nextResend) {
				if (!p.challenge) SendOutOfBand(p, "getchallenge");
				else SendConnect(p);
				nextResend = now + 3000;
			}
		} else if (now >= nextPacket) {
			// 35/36 send while reliable data waits or a second has passed until a
			// gamestate arrives; 38-40 send a move with the server id they have.
			if (!p.gamestates) {
				WritePacket(p, !Legacy(p));
				nextPacket = now + 1000;
			} else {
				WritePacket(p, true);
				nextPacket = now + 50;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	Disconnect(p);

	const float dx = p.lastOrigin[0] - p.firstOrigin[0], dy = p.lastOrigin[1] - p.firstOrigin[1];
	std::printf("{\n");
	std::printf("  \"protocol\": %d,\n", p.options.protocol);
	std::printf("  \"challenge\": %d,\n", p.challenge);
	std::printf("  \"connected\": %s,\n", p.connected ? "true" : "false");
	std::printf("  \"gamestates\": %d,\n  \"baselines\": %d,\n  \"configstrings\": %d,\n", p.gamestates, p.baselines,
		static_cast<int>(p.configstrings.size()));
	std::printf("  \"serverinfo\": %s,\n", Json(p.configstrings[0]).c_str());
	std::printf("  \"systeminfo\": %s,\n", Json(p.configstrings[1]).c_str());
	std::printf("  \"mapname\": %s,\n", Json(InfoValue(p.configstrings[0], "mapname")).c_str());
	std::printf("  \"snapshots\": %d,\n  \"fullSnapshots\": %d,\n  \"invalidSnapshots\": %d,\n", p.snapshots, p.fullSnapshots,
		p.invalidSnapshots);
	std::printf("  \"serverTime\": [%d, %d],\n", p.firstServerTime, p.lastServerTime);
	std::printf("  \"maxEntities\": %d,\n", p.maxEntities);
	std::printf("  \"origin\": [[%.1f, %.1f, %.1f], [%.1f, %.1f, %.1f]],\n", p.firstOrigin[0], p.firstOrigin[1], p.firstOrigin[2],
		p.lastOrigin[0], p.lastOrigin[1], p.lastOrigin[2]);
	std::printf("  \"moved\": %.1f,\n", std::sqrt(dx * dx + dy * dy));
	std::printf("  \"clientNum\": %d,\n", p.clientNum);
	std::printf("  \"serverCommands\": %d,\n  \"firstCommands\": [", p.commandCount);
	for (std::size_t i = 0; i < p.commands.size(); ++i) std::printf("%s%s", i ? ", " : "", Json(p.commands[i]).c_str());
	std::printf("],\n  \"prints\": [");
	for (std::size_t i = 0; i < p.prints.size(); ++i) std::printf("%s%s", i ? ", " : "", Json(p.prints[i]).c_str());
	std::printf("],\n  \"packetsSent\": %d,\n  \"packetsReceived\": %d,\n", p.packetsSent, p.packetsReceived);
	std::printf("  \"serverDisconnected\": %s,\n", p.serverDisconnected ? "true" : "false");
	std::printf("  \"error\": %s\n}\n", Json(p.error).c_str());
	CloseSocket(p.socket);
#ifdef _WIN32
	WSACleanup();
#endif
	return p.connected && p.snapshots > 0 && p.error.empty() ? 0 : 1;
}
