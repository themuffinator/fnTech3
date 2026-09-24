/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_MESSAGE_HPP
#define FNTECH3_Q3_IHV_MESSAGE_HPP

#include "q3_ihv_wire.hpp"
#include "../compat/q3_ihv_configstrings.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fntech3::q3ihv::wire {
inline constexpr std::size_t MaxMessageBytes = 16384;
inline constexpr std::size_t MaxConfigBytes = 16000;

struct GameState {
	std::array<std::string,1024> configstrings{};
	std::array<Entity,1024> baselines{};
	std::size_t configBytes = 1;
	client::ConfigStrings nativeConfigstrings;
};
struct Snapshot {
	std::uint32_t serverTime = 0, messageNumber = 0, deltaNumber = 0;
	std::uint32_t configSequence = 0, commandSequence = 0;
	std::uint8_t flags = 0, areaBytes = 0;
	std::array<std::uint8_t,32> areaMask{};
	Player player{};
	std::vector<Entity> entities;
	std::uint64_t firstEntity = 0;
};
struct ConfigstringUpdate {
	std::uint16_t index = 0;
	std::string value;
	bool changed = false;
};
struct MessageEvent {
	enum class Type { Nop, GameState, Configstring, ServerCommand, Snapshot };
	Type type;
	std::size_t index = 0; // Index in configstrings or commands, otherwise zero.
};
struct DecodedMessage {
	std::shared_ptr<const GameState> gamestate;
	std::shared_ptr<const Snapshot> snapshot;
	std::vector<std::string> commands;
	std::vector<ConfigstringUpdate> configstrings;
	// Native callbacks and regenerated messages must retain wire order.
	std::vector<MessageEvent> events;
};
enum class MessageResult { Ok, Malformed, MissingGamestate, MissingDelta, UnsupportedDownload };

inline bool ReadString(Reader &input,std::string &text) {
	Reader next=input; std::string value;
	for(unsigned i=0;i<1024;++i) {
		std::uint32_t c; if(!next.Read(8,c) || c==255) return input.Fail();
		if(!c) { input=next; text=std::move(value); return true; }
		value.push_back(static_cast<char>(c));
	}
	return input.Fail();
}
inline bool Protocol33(std::string_view info) {
	std::size_t position=!info.empty() && info[0]=='\\'?1:0; bool found=false;
	while(position<info.size()) {
		const auto keyEnd=info.find('\\',position); if(keyEnd==std::string::npos || keyEnd==position) return false;
		const auto end=info.find('\\',keyEnd+1);
		const auto valueEnd=end==std::string::npos?info.size():end;
		if(info.compare(position,keyEnd-position,"protocol")==0) {
			if(found || info.compare(keyEnd+1,valueEnd-keyEnd-1,"33")!=0) return false;
			found=true;
		}
		position=valueEnd+1;
	}
	return found;
}

/* A transactional message decoder, shared by the eventual live and demo
 * drivers. Failed packets publish neither a partial gamestate nor a frame.
 * No server command is executed here and no download is written to disk.
 * Original server packets carry one snapshot (0x41aebb) at most; a second
 * snapshot in one packet is rejected before committing state. */
class MessageDecoder {
public:
	MessageResult Decode(const void *data,std::size_t bytes,DecodedMessage &output) {
		if(!data || !bytes || bytes>MaxMessageBytes) return MessageResult::Malformed;
		Reader input(data,bytes); MessageDecoder pending=*this; DecodedMessage result;
		std::shared_ptr<GameState> changedGame;
		while(input.Remaining()>=8) {
			std::uint32_t command; if(!input.Read(8,command)) return MessageResult::Malformed;
			switch(command) {
			case 1: result.events.push_back({MessageEvent::Type::Nop}); break;
			case 2: {
				if(result.gamestate || result.snapshot) return MessageResult::Malformed;
				auto game=std::make_shared<GameState>();
				if(!ReadGameState(input,*game)) return MessageResult::Malformed;
				pending.game_=game; pending.frames_={}; pending.entitySequence_=0; result.gamestate=std::move(game);
				changedGame.reset(); result.events.push_back({MessageEvent::Type::GameState});
				break;
			}
			case 3: { // svc_configstring: short index and string, no sequence prefix
				if(!pending.game_) return MessageResult::MissingGamestate;
				std::uint32_t index; std::string value;
				if(!input.Read(16,index) || index>=1024 || !ReadString(input,value)
					|| (!index && !Protocol33(value))) return MessageResult::Malformed;
				bool changed=pending.game_->configstrings[index]!=value;
				if(changed) {
					if(!changedGame) changedGame=std::make_shared<GameState>(*pending.game_);
					if(!changedGame->nativeConfigstrings.Set(index,value,changed)) return MessageResult::Malformed;
					changedGame->configstrings[index]=value;
					changedGame->configBytes=changedGame->nativeConfigstrings.BytesUsed();
					pending.game_=changedGame;
				}
				result.events.push_back({MessageEvent::Type::Configstring,result.configstrings.size()});
				result.configstrings.push_back({static_cast<std::uint16_t>(index),std::move(value),changed});
				break;
			}
			case 5: { // svc_serverCommand: string only, no retail sequence prefix
				std::string text; if(!ReadString(input,text)) return MessageResult::Malformed;
				result.events.push_back({MessageEvent::Type::ServerCommand,result.commands.size()});
				result.commands.push_back(std::move(text)); break;
			}
			case 6: return MessageResult::UnsupportedDownload;
			case 7: {
				if(!pending.game_) return MessageResult::MissingGamestate;
				if(result.snapshot) return MessageResult::Malformed;
				auto snapshot=std::make_shared<Snapshot>();
				const auto status=pending.ReadSnapshot(input,*snapshot);
				if(status!=MessageResult::Ok) return status;
				pending.frames_[snapshot->messageNumber&31]=snapshot;
				pending.entitySequence_+=snapshot->entities.size(); result.snapshot=std::move(snapshot);
				result.events.push_back({MessageEvent::Type::Snapshot});
				break;
			}
			default: return MessageResult::Malformed;
			}
		}
		// Original bit writers zero the unused bits of their last byte. There is
		// no svc_EOF byte; the byte extent ends the outer command stream.
		std::uint32_t padding; if(!input.Read(static_cast<unsigned>(input.Remaining()),padding) || padding) return MessageResult::Malformed;
		*this=std::move(pending); output=std::move(result); return MessageResult::Ok;
	}
	void Clear() { game_.reset(); frames_={}; entitySequence_=0; }
	const std::shared_ptr<const GameState> &CurrentGameState() const { return game_; }
	std::shared_ptr<const Snapshot> Frame(std::uint32_t number) const {
		const auto &frame=frames_[number&31]; return frame && frame->messageNumber==number?frame:nullptr;
	}
private:
	static bool ReadGameState(Reader &input,GameState &game) {
		for(;;) {
			std::uint32_t command; if(!input.Read(8,command)) return false;
			if(!command) return Protocol33(game.configstrings[0]);
			if(command==3) {
				std::uint32_t index; std::string text;
				if(!input.Read(16,index) || index>=game.configstrings.size() || !ReadString(input,text)) return false;
				if(!game.nativeConfigstrings.Append(index,text)) return false;
				game.configBytes=game.nativeConfigstrings.BytesUsed(); game.configstrings[index]=std::move(text);
			} else if(command==4) {
				std::uint32_t number; if(!input.Read(10,number) || !number || number>=1024) return false;
				const Entity empty{}; if(!ReadEntityDelta(input,empty,game.baselines[number],number)) return false;
			} else return false;
		}
	}
	MessageResult ReadSnapshot(Reader &input,Snapshot &frame) const {
		std::uint32_t flags,areaBytes;
		if(!input.Read(32,frame.serverTime) || !input.Read(32,frame.messageNumber) || !input.Read(32,frame.deltaNumber)
			|| !input.Read(32,frame.configSequence) || !input.Read(32,frame.commandSequence)
			|| !input.Read(8,flags) || !input.Read(8,areaBytes) || areaBytes>frame.areaMask.size()) return MessageResult::Malformed;
		frame.flags=static_cast<std::uint8_t>(flags); frame.areaBytes=static_cast<std::uint8_t>(areaBytes);
		for(unsigned i=0;i<areaBytes;++i) {
			std::uint32_t value; if(!input.Read(8,value)) return MessageResult::Malformed;
			frame.areaMask[i]=static_cast<std::uint8_t>(value);
		}
		std::shared_ptr<const Snapshot> previous;
		// The original tests the signed delta number for > 0.
		if(frame.deltaNumber && !(frame.deltaNumber&0x80000000u)) {
			previous=Frame(frame.deltaNumber);
			if(!previous || entitySequence_-previous->firstEntity>896) return MessageResult::MissingDelta;
		}
		const Player empty{};
		if(!ReadPlayer(input,previous?previous->player:empty,frame.player)) return MessageResult::Malformed;
		frame.firstEntity=entitySequence_;
		std::size_t oldIndex=0; unsigned lastNumber=0;
		for(;;) {
			std::uint32_t number; if(!input.Read(10,number)) return MessageResult::Malformed;
			if(number && number<=lastNumber) return MessageResult::Malformed;
			lastNumber=number;
			while(previous && oldIndex<previous->entities.size()
				&& (!number || Load(previous->entities[oldIndex].data(),2)<number)) frame.entities.push_back(previous->entities[oldIndex++]);
			if(!number) break;
			const Entity *base=&game_->baselines[number];
			if(previous && oldIndex<previous->entities.size() && Load(previous->entities[oldIndex].data(),2)==number) base=&previous->entities[oldIndex++];
			Entity entity{}; if(!ReadEntityDelta(input,*base,entity,number)) return MessageResult::Malformed;
			if(Load(entity.data(),2)) frame.entities.push_back(entity);
			if(frame.entities.size()>1023) return MessageResult::Malformed;
		}
		return frame.entities.size()<=1023?MessageResult::Ok:MessageResult::Malformed;
	}
	std::shared_ptr<const GameState> game_;
	std::array<std::shared_ptr<const Snapshot>,32> frames_{};
	std::uint64_t entitySequence_=0;
};

inline bool WriteString(Writer &output,const std::string &text) {
	if(text.size()>=1024 || text.find('\0')!=std::string::npos
		|| text.find(static_cast<char>(255))!=std::string::npos) return output.Fail();
	if(!output.Reserve((text.size()+1)*8)) return false;
	for(unsigned char c : text) output.Write(c,8);
	return output.Write(0,8);
}

/* Protocol-33 server message construction. Each command is staged before
 * appending, so a bad state or capacity failure leaves earlier commands usable.
 * Snapshot history selection belongs to the session driver; this boundary
 * checks that the supplied delta frame agrees with the on-wire delta number.
 * The entity merge/order follows the observed original server writer. */
class MessageWriter {
public:
	explicit MessageWriter(std::size_t capacity=MaxMessageBytes)
		: capacity_(capacity<=MaxMessageBytes?capacity:0),valid_(capacity<=MaxMessageBytes),output_(data_.data(),capacity_) {}
	MessageWriter(const MessageWriter &)=delete;
	MessageWriter &operator=(const MessageWriter &)=delete;
	bool AddNop() { const std::uint8_t command=1; return Append(&command,8); }
	bool AddServerCommand(const std::string &text) {
		std::array<std::uint8_t,1025> data{}; Writer out(data.data(),data.size());
		out.Write(5,8);
		return WriteString(out,text) && Append(data.data(),out.Position());
	}
	bool AddConfigstring(std::uint32_t index,const std::string &text) {
		if(index>=1024 || (!index && !Protocol33(text))) return false;
		std::array<std::uint8_t,1027> data{}; Writer out(data.data(),data.size());
		out.Write(3,8); out.Write(index,16);
		return WriteString(out,text) && Append(data.data(),out.Position());
	}
	bool AddGameState(const GameState &game) {
		if(gameWritten_ || snapshotWritten_ || !Protocol33(game.configstrings[0])) return false;
		std::array<std::uint8_t,MaxMessageBytes> data{}; Writer out(data.data(),data.size());
		out.Write(2,8); std::size_t configBytes=1;
		for(unsigned i=0;i<game.configstrings.size();++i) {
			const auto &text=game.configstrings[i]; if(text.empty()) continue;
			if(text.size()>=MaxConfigBytes-configBytes) return false;
			configBytes+=text.size()+1;
			if(!out.Write(3,8) || !out.Write(i,16) || !WriteString(out,text)) return false;
		}
		const Entity empty{};
		for(unsigned i=0;i<game.baselines.size();++i) {
			const auto number=Load(game.baselines[i].data(),2); if(!number) continue;
			if(!i || number!=i || !out.Write(4,8) || !WriteEntity(out,empty,&game.baselines[i],true)) return false;
		}
		if(!out.Write(0,8) || !Append(data.data(),out.Position())) return false;
		gameWritten_=true; return true;
	}
	/* SV_EmitPacketEntities 0x41ace1 stops adding entities once the message has
	 * passed its byte limit, and the server records the shorter count so a later
	 * delta against this frame matches what was sent. A zero limit sends all. */
	bool AddSnapshot(const GameState &game,const Snapshot *previous,const Snapshot &frame,
		std::size_t entityByteLimit=0,std::size_t *entitiesWritten=nullptr) {
		if(entitiesWritten) *entitiesWritten=frame.entities.size();
		if(snapshotWritten_ || !Protocol33(game.configstrings[0]) || frame.areaBytes>frame.areaMask.size()
			|| !ValidEntities(frame.entities) || (previous && !ValidEntities(previous->entities))) return false;
		const bool delta=frame.deltaNumber && !(frame.deltaNumber&0x80000000u);
		if(delta!=bool(previous) || (previous && previous->messageNumber!=frame.deltaNumber)) return false;
		std::array<std::uint8_t,MaxMessageBytes> data{}; Writer out(data.data(),data.size());
		out.Write(7,8); out.Write(frame.serverTime,32); out.Write(frame.messageNumber,32); out.Write(frame.deltaNumber,32);
		out.Write(frame.configSequence,32); out.Write(frame.commandSequence,32); out.Write(frame.flags,8); out.Write(frame.areaBytes,8);
		for(unsigned i=0;i<frame.areaBytes;++i) out.Write(frame.areaMask[i],8);
		const Player empty{}; if(!WritePlayer(out,previous?previous->player:empty,frame.player)) return false;
		std::size_t oldIndex=0,newIndex=0;
		while(newIndex<frame.entities.size() || (previous && oldIndex<previous->entities.size())) {
			if(entityByteLimit && Bytes()+out.Bytes()>entityByteLimit) break;
			const Entity *oldEntity=previous && oldIndex<previous->entities.size()?&previous->entities[oldIndex]:nullptr;
			const Entity *newEntity=newIndex<frame.entities.size()?&frame.entities[newIndex]:nullptr;
			const auto oldNumber=oldEntity?Load(oldEntity->data(),2):1024;
			const auto newNumber=newEntity?Load(newEntity->data(),2):1024;
			if(oldNumber==newNumber) {
				if(!WriteEntity(out,*oldEntity,newEntity,false)) return false;
				++oldIndex; ++newIndex;
			} else if(oldNumber<newNumber) {
				if(!WriteEntity(out,*oldEntity,nullptr,false)) return false;
				++oldIndex;
			} else {
				const auto &base=game.baselines[newNumber]; const auto baseNumber=Load(base.data(),2);
				if((baseNumber && baseNumber!=newNumber) || !WriteEntity(out,base,newEntity,true)) return false;
				++newIndex;
			}
		}
		if(!out.Write(0,10) || !Append(data.data(),out.Position())) return false;
		if(entitiesWritten) *entitiesWritten=newIndex;
		snapshotWritten_=true; return true;
	}
	void Clear() { output_=Writer(data_.data(),capacity_); gameWritten_=snapshotWritten_=false; }
	const std::uint8_t *Data() const { return data_.data(); }
	std::size_t Bytes() const { return output_.Bytes(); }
	std::size_t Bits() const { return output_.Position(); }
private:
	static bool ValidEntities(const std::vector<Entity> &entities) {
		if(entities.size()>1023) return false;
		unsigned last=0;
		for(const auto &entity : entities) {
			const auto number=Load(entity.data(),2); if(number<=last || number>=1024) return false;
			last=number;
		}
		return true;
	}
	bool Append(const void *data,std::size_t bits) {
		if(!valid_) return false;
		Writer next=output_; if(!next.Reserve(bits)) return false;
		Reader source(data,bits/8+!!(bits&7));
		while(bits) {
			const auto count=static_cast<unsigned>(bits>32?32:bits); std::uint32_t value;
			if(!source.Read(count,value) || !next.Write(value,count)) return false;
			bits-=count;
		}
		output_=next; return true;
	}
	std::array<std::uint8_t,MaxMessageBytes> data_{};
	std::size_t capacity_;
	bool valid_,gameWritten_=false,snapshotWritten_=false;
	Writer output_;
};

enum class DemoRecordResult { Message, End, Truncated, Malformed };
struct DemoRecord { const std::uint8_t *data=nullptr; std::size_t size=0; };
/* IHV .dm3: signed little-endian length + bytes, terminated by length -1.
 * There is no per-record message sequence word. Missing terminators and
 * trailing data are reported distinctly from a completed recording. */
class DemoReader {
public:
	DemoReader(const void *data,std::size_t size) : data_(static_cast<const std::uint8_t *>(data)),size_(size) {}
	DemoRecordResult Next(DemoRecord &output) {
		if(ended_) return DemoRecordResult::End;
		if(!data_ || size_-position_<4) return DemoRecordResult::Truncated;
		const auto length=Load(data_+position_,4);
		if(length==0xffffffffu) {
			if(size_-position_!=4) return DemoRecordResult::Malformed;
			position_+=4; ended_=true; return DemoRecordResult::End;
		}
		if(!length || length>MaxMessageBytes) return DemoRecordResult::Malformed;
		if(length>size_-position_-4) return DemoRecordResult::Truncated;
		output={data_+position_+4,length}; position_+=4+length; return DemoRecordResult::Message;
	}
	std::size_t Position() const { return position_; }
private:
	const std::uint8_t *data_;
	std::size_t size_,position_=0;
	bool ended_=false;
};

/* Caller-owned, bounded .dm3 output. An unsuccessful append/finish does not
 * change any bytes or the cursor. Message bytes may overlap the destination. */
class DemoWriter {
public:
	DemoWriter(void *data,std::size_t size) : data_(static_cast<std::uint8_t *>(data)),size_(size) {}
	bool Append(const void *data,std::size_t bytes) {
		if(!data_ || !data || ended_ || !bytes || bytes>MaxMessageBytes
			|| size_-position_<4 || bytes>size_-position_-4) return false;
		std::memmove(data_+position_+4,data,bytes);
		Store(data_+position_,4,static_cast<std::uint32_t>(bytes)); position_+=4+bytes; return true;
	}
	bool Finish() {
		if(ended_) return true;
		if(!data_ || size_-position_<4) return false;
		Store(data_+position_,4,0xffffffffu); position_+=4; ended_=true; return true;
	}
	std::size_t Position() const { return position_; }
private:
	std::uint8_t *data_;
	std::size_t size_,position_=0;
	bool ended_=false;
};
} // namespace fntech3::q3ihv::wire
#endif
