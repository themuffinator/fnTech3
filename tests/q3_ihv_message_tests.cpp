/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q3_ihv_message.hpp"
#include "q3_ihv_wire_cases.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace fntech3::q3ihv::wire;
using Bytes = std::vector<std::uint8_t>;
static void String(Writer &out,const char *text) { do { CHECK(out.Write(static_cast<std::uint8_t>(*text),8)); } while(*text++); }
static Bytes Game(const char *info="\\protocol\\33\\mapname\\synthetic",unsigned configIndex=0) {
	Bytes data(MaxMessageBytes); Writer out(data.data(),data.size());
	out.Write(2,8); out.Write(3,8); out.Write(configIndex,16); String(out,info);
	out.Write(4,8); Entity empty{},baseline{}; baseline[0]=1; baseline[149]=2; CHECK(WriteEntity(out,empty,&baseline,true));
	out.Write(0,8); data.resize(out.Bytes()); return data;
}
static Entity Baseline() { Entity value{}; value[0]=1; value[149]=2; return value; }
static Bytes FramePacket(unsigned number,unsigned delta,unsigned areas=1,int mode=0,std::size_t *bits=nullptr) {
	Bytes data(MaxMessageBytes); Writer out(data.data(),data.size());
	out.Write(7,8); out.Write(100+number*50,32); out.Write(number,32); out.Write(delta,32);
	out.Write(2,32); out.Write(number*2,32); out.Write(0,8); out.Write(areas,8);
	for(unsigned i=0;i<areas;++i) out.Write(0xa5,8);
	Player empty{},player{}; player[100]=99; CHECK(WritePlayer(out,empty,player));
	Entity entity=Baseline();
	if(mode==0) CHECK(WriteEntity(out,entity,&entity,true));
	if(mode==1) {
		CHECK(WriteEntity(out,entity,nullptr,false));
		const Entity blank{}; entity={}; entity[0]=2; CHECK(WriteEntity(out,blank,&entity,true));
	}
	if(mode==2) { CHECK(WriteEntity(out,entity,&entity,true)); CHECK(WriteEntity(out,entity,&entity,true)); }
	if(mode==4) {
		const Entity blank{};
		for(unsigned i=1;i<=897;++i) { entity={}; Store(entity.data(),2,i); CHECK(WriteEntity(out,blank,&entity,true)); }
	}
	if(mode==5) { out.Write(47,10); out.Write(0,1); out.Write(0,1); } // Unchanged absent baseline: original reader returns number zero.
	out.Write(0,10); if(bits) *bits=out.Position(); data.resize(out.Bytes()); return data;
}
static MessageResult Feed(MessageDecoder &decoder,const Bytes &data,DecodedMessage &out) { return decoder.Decode(data.data(),data.size(),out); }
static void PutRecord(Bytes &out,const Bytes &message) {
	const auto at=out.size(); out.resize(at+4); Store(out.data()+at,4,static_cast<std::uint32_t>(message.size()));
	out.insert(out.end(),message.begin(),message.end());
}
static void End(Bytes &out) { out.insert(out.end(),4,255); }

void Messages() {
	MessageDecoder decoder; DecodedMessage output; output.commands={"unchanged"};
	const auto game=Game(); const auto first=FramePacket(1,0xffffffffu);
	CHECK(Feed(decoder,first,output)==MessageResult::MissingGamestate && output.commands[0]=="unchanged");
	CHECK(Feed(decoder,game,output)==MessageResult::Ok && output.gamestate && decoder.CurrentGameState());
	CHECK(output.gamestate->baselines[1]==Baseline()); const auto current=decoder.CurrentGameState();
	CHECK(Feed(decoder,first,output)==MessageResult::Ok && output.snapshot);
	CHECK(output.snapshot->messageNumber==1 && output.snapshot->serverTime==150 && output.snapshot->configSequence==2);
	CHECK(output.snapshot->commandSequence==2 && output.snapshot->areaMask[0]==0xa5 && output.snapshot->player[100]==99);
	CHECK(output.snapshot->entities.size()==1 && output.snapshot->entities[0]==Baseline());
	CHECK(Feed(decoder,FramePacket(2,1,1,1),output)==MessageResult::Ok && output.snapshot->entities.size()==1);
	CHECK(output.snapshot->entities[0][0]==2);
	CHECK(Feed(decoder,FramePacket(3,2,1,3),output)==MessageResult::Ok && output.snapshot->entities[0][0]==2);
	const auto previous=output.snapshot;
	CHECK(Feed(decoder,FramePacket(4,99),output)==MessageResult::MissingDelta && output.snapshot==previous && !decoder.Frame(4));
	CHECK(Feed(decoder,FramePacket(4,3,33),output)==MessageResult::Malformed && output.snapshot==previous && !decoder.Frame(4));
	CHECK(Feed(decoder,FramePacket(4,3,1,2),output)==MessageResult::Malformed && !decoder.Frame(4));
	CHECK(Feed(decoder,FramePacket(4,3,1,3),output)==MessageResult::Ok);
	CHECK(decoder.CurrentGameState()==current);
	for(const auto &bad : {Game("\\protocol\\46"),Game("\\protocol\\33\\protocol\\33"),Game("\\hostname\\protocol33"),Game("\\protocol\\33",1024)}) {
		CHECK(Feed(decoder,bad,output)==MessageResult::Malformed && decoder.CurrentGameState()==current && decoder.Frame(4));
	}
	Bytes command{5,'e','c','h','o',' ','t','e','s','t',0};
	CHECK(Feed(decoder,command,output)==MessageResult::Ok && output.commands==std::vector<std::string>{"echo test"});
	const auto commands=output.commands; command.push_back(0);
	CHECK(Feed(decoder,command,output)==MessageResult::Malformed && output.commands==commands);
	CHECK(Feed(decoder,Bytes{6},output)==MessageResult::UnsupportedDownload && output.commands==commands);
	CHECK(Feed(decoder,Bytes{12},output)==MessageResult::Malformed); // Quake II serverdata
	CHECK(Feed(decoder,Bytes{3,0,0,'x',0},output)==MessageResult::Malformed); // Serverinfo cannot change profile.
	CHECK(decoder.Decode(nullptr,1,output)==MessageResult::Malformed);
	CHECK(Feed(decoder,Bytes{},output)==MessageResult::Malformed);
	CHECK(Feed(decoder,Bytes(MaxMessageBytes+1,1),output)==MessageResult::Malformed);
	// Limits account for all appended configstrings, even replaced indices.
	Bytes oversized(MaxMessageBytes); Writer oversizedWriter(oversized.data(),oversized.size()); oversizedWriter.Write(2,8);
	const std::string value(1000,'x');
	for(unsigned i=0;i<16;++i) { oversizedWriter.Write(3,8); oversizedWriter.Write(0,16); String(oversizedWriter,value.c_str()); }
	oversizedWriter.Write(0,8); oversized.resize(oversizedWriter.Bytes());
	CHECK(Feed(decoder,oversized,output)==MessageResult::Malformed && decoder.CurrentGameState()==current);
	// Rollback when a second snapshot follows a valid one in the same packet.
	std::size_t firstBits=0,secondBits=0;
	const auto firstFrame=FramePacket(5,0xffffffffu,1,0,&firstBits),secondFrame=FramePacket(6,0xffffffffu,1,0,&secondBits);
	Bytes twice(MaxMessageBytes); Writer joined(twice.data(),twice.size());
	for(auto part : {std::make_pair(&firstFrame,firstBits),std::make_pair(&secondFrame,secondBits)}) {
		Reader source(part.first->data(),part.first->size());
		for(std::size_t i=0;i<part.second;++i) { std::uint32_t bit; CHECK(source.Read(1,bit) && joined.Write(bit,1)); }
	}
	twice.resize(joined.Bytes());
	CHECK(Feed(decoder,twice,output)==MessageResult::Malformed && !decoder.Frame(5) && !decoder.Frame(6));
	CHECK(Feed(decoder,FramePacket(33,0xffffffffu),output)==MessageResult::Ok && !decoder.Frame(1));
	CHECK(Feed(decoder,FramePacket(34,1),output)==MessageResult::MissingDelta);
	CHECK(Feed(decoder,Game(),output)==MessageResult::Ok && !decoder.Frame(33));
	CHECK(Feed(decoder,FramePacket(1,0xffffffffu,1,5),output)==MessageResult::Ok && output.snapshot->entities.empty());
	CHECK(Feed(decoder,FramePacket(1,0xffffffffu,1,4),output)==MessageResult::Ok && output.snapshot->entities.size()==897);
	CHECK(Feed(decoder,FramePacket(2,1),output)==MessageResult::MissingDelta); // Original parse-entity window is 896.
	decoder.Clear(); CHECK(!decoder.CurrentGameState() && !decoder.Frame(1));
	// Every truncated byte prefix leaves the previous game and frame intact.
	CHECK(Feed(decoder,game,output)==MessageResult::Ok);
	const auto retained=decoder.CurrentGameState();
	for(const auto &message : {game,first}) for(std::size_t size=0;size<message.size();++size) {
		DecodedMessage untouched; untouched.commands={"retained"};
		CHECK(decoder.Decode(message.data(),size,untouched)!=MessageResult::Ok);
		CHECK(untouched.commands[0]=="retained" && decoder.CurrentGameState()==retained && !decoder.Frame(1));
	}
}

void Containers() {
	Bytes file; PutRecord(file,Game()); PutRecord(file,FramePacket(1,0xffffffffu)); End(file);
	DemoReader reader(file.data(),file.size()); DemoRecord record; MessageDecoder decoder; DecodedMessage message;
	CHECK(reader.Next(record)==DemoRecordResult::Message && decoder.Decode(record.data,record.size,message)==MessageResult::Ok);
	CHECK(reader.Next(record)==DemoRecordResult::Message && decoder.Decode(record.data,record.size,message)==MessageResult::Ok);
	CHECK(reader.Next(record)==DemoRecordResult::End && reader.Position()==file.size());
	CHECK(reader.Next(record)==DemoRecordResult::End);
	for(std::size_t size=0;size<file.size();++size) {
		DemoReader cut(file.data(),size); DemoRecordResult result;
		do { result=cut.Next(record); } while(result==DemoRecordResult::Message);
		CHECK(result==DemoRecordResult::Truncated);
	}
	file.push_back(0); DemoReader trailing(file.data(),file.size());
	CHECK(trailing.Next(record)==DemoRecordResult::Message && trailing.Next(record)==DemoRecordResult::Message);
	CHECK(trailing.Next(record)==DemoRecordResult::Malformed);
	for(auto length : {0u,0xfffffffeu,16385u,0x7fffffffu}) {
		std::array<std::uint8_t,4> header{}; Store(header.data(),4,length); DemoReader invalid(header.data(),header.size());
		record={reinterpret_cast<const std::uint8_t *>(1),123};
		CHECK(invalid.Next(record)==DemoRecordResult::Malformed && !invalid.Position() && record.size==123);
	}
}

void Mutations() {
	MessageDecoder base; DecodedMessage initial;
	CHECK(Feed(base,Game(),initial)==MessageResult::Ok);
	CHECK(Feed(base,FramePacket(1,0xffffffffu),initial)==MessageResult::Ok);
	const auto game=base.CurrentGameState(); const auto frame=base.Frame(1);
	const auto check=[&](const Bytes &bytes) {
		MessageDecoder trial=base; DecodedMessage output; output.commands={"retained"};
		const auto result=Feed(trial,bytes,output);
		if(result!=MessageResult::Ok) {
			CHECK(trial.CurrentGameState()==game && trial.Frame(1)==frame && !trial.Frame(2));
			CHECK(output.commands==std::vector<std::string>{"retained"} && !output.gamestate && !output.snapshot);
		} else {
			CHECK(trial.CurrentGameState() && trial.CurrentGameState()->configBytes<=MaxConfigBytes);
			if(output.snapshot) {
				CHECK(output.snapshot->areaBytes<=32 && output.snapshot->entities.size()<=1023);
				unsigned last=0;
				for(const auto &entity : output.snapshot->entities) { const auto number=Load(entity.data(),2); CHECK(number>last && number<1024); last=number; }
			}
		}
	};
	for(const auto &original : {Game(),FramePacket(2,1,1,1),Bytes{5,'t','e','s','t',0}}) {
		for(std::size_t bit=0;bit<original.size()*8;++bit) {
			auto changed=original; changed[bit/8]^=static_cast<std::uint8_t>(1u<<(bit&7)); check(changed);
		}
	}
	std::uint32_t seed=0x33617271u;
	for(unsigned i=0;i<4096;++i) {
		Bytes bytes(ihv_cases::Next(seed)>>23);
		for(auto &byte : bytes) byte=static_cast<std::uint8_t>(ihv_cases::Next(seed)>>24);
		check(bytes);
	}
}

static Bytes Written(const MessageWriter &writer) { return {writer.Data(),writer.Data()+writer.Bytes()}; }
void Writing() {
	MessageDecoder source; DecodedMessage decoded;
	CHECK(Feed(source,Game(),decoded)==MessageResult::Ok); const auto game=decoded.gamestate;
	CHECK(Feed(source,FramePacket(1,0xffffffffu),decoded)==MessageResult::Ok); const auto first=decoded.snapshot;
	CHECK(Feed(source,FramePacket(2,1,1,1),decoded)==MessageResult::Ok); const auto second=decoded.snapshot;
	MessageWriter out; CHECK(out.AddGameState(*game)); CHECK(Written(out)==Game());
	const auto gameBytes=Written(out); const auto gameBits=out.Bits();
	CHECK(!out.AddGameState(*game) && Written(out)==gameBytes && out.Bits()==gameBits);
	CHECK(!out.AddServerCommand(std::string(1024,'x')) && Written(out)==gameBytes);
	CHECK(!out.AddServerCommand(std::string("x\0y",3)) && Written(out)==gameBytes);
	CHECK(!out.AddServerCommand(std::string(1,static_cast<char>(255))) && Written(out)==gameBytes);
	CHECK(out.AddSnapshot(*game,nullptr,*first));
	CHECK(out.AddServerCommand("after snapshot") && out.AddNop());
	MessageDecoder receiver; DecodedMessage received;
	CHECK(receiver.Decode(out.Data(),out.Bytes(),received)==MessageResult::Ok && received.gamestate && received.snapshot);
	CHECK(received.snapshot->player==first->player && received.snapshot->entities==first->entities);
	CHECK(received.commands==std::vector<std::string>{"after snapshot"});
	const auto combined=Written(out); CHECK(!out.AddSnapshot(*game,first.get(),*second) && Written(out)==combined);
	out.Clear(); CHECK(!out.Bytes() && out.AddSnapshot(*game,first.get(),*second));
	CHECK(receiver.Decode(out.Data(),out.Bytes(),received)==MessageResult::Ok && received.snapshot->entities==second->entities);
	CHECK(received.snapshot->configSequence==second->configSequence && received.snapshot->commandSequence==second->commandSequence);
	CHECK(received.snapshot->areaMask==second->areaMask && received.snapshot->flags==second->flags);
	const auto secondBytes=Written(out);
	out.Clear(); CHECK(!out.AddSnapshot(*game,nullptr,*second) && !out.Bytes());
	CHECK(!out.AddSnapshot(*game,second.get(),*second) && !out.Bytes());
	CHECK(!out.AddSnapshot(*game,first.get(),*first) && !out.Bytes());
	auto invalid=*second; invalid.areaBytes=33;
	CHECK(!out.AddSnapshot(*game,first.get(),invalid) && !out.Bytes());
	invalid=*second; invalid.entities.push_back(invalid.entities[0]);
	CHECK(!out.AddSnapshot(*game,first.get(),invalid) && !out.Bytes());
	invalid=*second; Store(invalid.entities[0].data(),2,1024);
	CHECK(!out.AddSnapshot(*game,first.get(),invalid) && !out.Bytes());
	invalid=*first; invalid.entities[0][0]=0;
	CHECK(!out.AddSnapshot(*game,nullptr,invalid) && !out.Bytes());
	auto badGame=std::make_shared<GameState>(*game); badGame->baselines[2][0]=3;
	CHECK(!out.AddGameState(*badGame) && !out.Bytes());
	CHECK(!out.AddSnapshot(*badGame,first.get(),*second) && !out.Bytes());
	badGame->baselines[2]={}; badGame->configstrings[0]="\\protocol\\46";
	CHECK(!out.AddGameState(*badGame) && !out.Bytes());
	CHECK(!out.AddSnapshot(*badGame,nullptr,*first) && !out.Bytes());
	*badGame=*game;
	for(unsigned i=1;i<17;++i) badGame->configstrings[i]=std::string(1000,'x');
	CHECK(!out.AddGameState(*badGame) && !out.Bytes());
	invalid=*first; invalid.entities.clear();
	for(unsigned i=1;i<=1023;++i) { Entity entity; entity.fill(1); Store(entity.data(),2,i); invalid.entities.push_back(entity); }
	CHECK(!out.AddSnapshot(*game,nullptr,invalid) && !out.Bytes()); // Maximum-count states can exceed one message.
	CHECK(out.AddSnapshot(*game,first.get(),*second) && Written(out)==secondBytes);
	for(std::size_t capacity=0;capacity<secondBytes.size()+1;++capacity) {
		MessageWriter small(capacity);
		CHECK(small.AddNop()==bool(capacity)); const auto before=Written(small); const auto bits=small.Bits();
		CHECK(!small.AddSnapshot(*game,first.get(),*second));
		CHECK(Written(small)==before && small.Bits()==bits);
		CHECK(small.AddNop()==(capacity>=2)); // A capacity failure does not poison earlier output.
	}
	MessageWriter invalidCapacity(MaxMessageBytes+1); CHECK(!invalidCapacity.AddNop() && !invalidCapacity.Bytes());
	MessageWriter command; CHECK(command.AddServerCommand(std::string(1023,'x')));
	CHECK(receiver.Decode(command.Data(),command.Bytes(),received)==MessageResult::Ok && received.commands[0].size()==1023);
	command.Clear(); CHECK(command.AddServerCommand("") && command.Bytes()==2);

	const auto message=Game(); const auto required=message.size()+8;
	for(std::size_t capacity=0;capacity<=required;++capacity) {
		Bytes bytes(required,0xa5),before=bytes; DemoWriter writer(bytes.data(),capacity);
		CHECK(writer.Append(message.data(),message.size())==(capacity>=message.size()+4));
		if(capacity<message.size()+4) { CHECK(!writer.Position() && bytes==before); continue; }
		before=bytes; const auto position=writer.Position();
		CHECK(writer.Finish()==(capacity==required));
		if(capacity<required) { CHECK(writer.Position()==position && bytes==before); continue; }
		CHECK(writer.Position()==required && writer.Finish()); before=bytes;
		CHECK(!writer.Append(message.data(),message.size()) && bytes==before);
		DemoReader reader(bytes.data(),bytes.size()); DemoRecord record;
		CHECK(reader.Next(record)==DemoRecordResult::Message && record.size==message.size());
		CHECK(!std::memcmp(record.data,message.data(),record.size) && reader.Next(record)==DemoRecordResult::End);
	}
	Bytes overlap(12,0xa5); overlap[0]=1; overlap[1]=2; overlap[2]=3;
	DemoWriter writer(overlap.data(),overlap.size()); CHECK(writer.Append(overlap.data(),3) && writer.Finish());
	CHECK(Load(overlap.data(),4)==3 && overlap[4]==1 && overlap[5]==2 && overlap[6]==3 && writer.Position()==11);
	Bytes empty(8,0xa5); const auto before=empty; DemoWriter bad(empty.data(),empty.size());
	CHECK(!bad.Append(nullptr,1) && !bad.Append(message.data(),0) && !bad.Append(message.data(),MaxMessageBytes+1));
	CHECK(empty==before && !bad.Position() && bad.Finish());
	DemoWriter null(nullptr,8); CHECK(!null.Append(message.data(),1) && !null.Finish() && !null.Position());
}

void ConfigUpdates() {
	MessageDecoder decoder; DecodedMessage output;
	MessageWriter update; CHECK(update.AddConfigstring(9,"new"));
	CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::MissingGamestate);
	// Initial empty and duplicate records retain their original append offsets.
	Bytes initial(MaxMessageBytes); Writer raw(initial.data(),initial.size());
	raw.Write(2,8);
	for(const auto &item:std::vector<std::pair<unsigned,std::string>>{{9,"old"},{0,"\\protocol\\33"},{9,"last"},{8,""}}) {
		raw.Write(3,8);raw.Write(item.first,16);String(raw,item.second.c_str());
	}
	raw.Write(0,8);initial.resize(raw.Bytes());
	CHECK(Feed(decoder,initial,output)==MessageResult::Ok);
	const auto game=decoder.CurrentGameState(); const auto before=game->nativeConfigstrings.Record();
	CHECK(game->configstrings[9]=="last" && Load(before.bytes+8*4,4)!=0);
	CHECK(game->nativeConfigstrings.String(9)=="last" && game->configBytes==game->nativeConfigstrings.BytesUsed());
	update.Clear();CHECK(update.AddConfigstring(9,"last") && update.AddConfigstring(8,""));
	CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Ok && decoder.CurrentGameState()==game);
	CHECK(output.configstrings.size()==2 && !output.configstrings[0].changed && !output.configstrings[1].changed);
	CHECK(!std::memcmp(&before,&decoder.CurrentGameState()->nativeConfigstrings.Record(),sizeof before));
	// Changed values, commands, nops and snapshots retain their relative order.
	update.Clear(); Snapshot snapshot{};snapshot.messageNumber=1;
	CHECK(update.AddServerCommand("before") && update.AddConfigstring(9,"new") && update.AddNop());
	CHECK(update.AddSnapshot(*game,nullptr,snapshot) && update.AddConfigstring(9,"after") && update.AddServerCommand("done"));
	CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Ok && output.snapshot);
	CHECK(output.configstrings.size()==2 && output.configstrings[0].changed && output.configstrings[1].changed);
	CHECK(game->configstrings[9]=="last" && decoder.CurrentGameState()->configstrings[9]=="after");
	using Type=MessageEvent::Type;
	const Type order[]={Type::ServerCommand,Type::Configstring,Type::Nop,Type::Snapshot,Type::Configstring,Type::ServerCommand};
	CHECK(output.events.size()==6);
	for(unsigned i=0;i<6;++i) CHECK(output.events[i].type==order[i]);
	CHECK(output.events[1].index==0 && output.events[4].index==1 && output.events[5].index==1);
	MessageWriter replay;
	for(const auto &event:output.events) {
		switch(event.type) {
		case Type::Nop: CHECK(replay.AddNop()); break;
		case Type::ServerCommand: CHECK(replay.AddServerCommand(output.commands[event.index])); break;
		case Type::Configstring: { const auto &c=output.configstrings[event.index]; CHECK(replay.AddConfigstring(c.index,c.value)); break; }
		case Type::Snapshot: CHECK(replay.AddSnapshot(*game,nullptr,*output.snapshot)); break;
		default: CHECK(false);
		}
	}
	CHECK(replay.Bits()==update.Bits() && Written(replay)==Written(update));
	// Truncated changes and an invalid later opcode cannot publish earlier work.
	update.Clear();CHECK(update.AddConfigstring(9,"changed"));const auto packet=Written(update);
	CHECK(packet==Bytes({3,9,0,'c','h','a','n','g','e','d',0}));
	const auto retained=decoder.CurrentGameState();const auto retainedFrame=decoder.Frame(1);
	for(std::size_t length=1;length<packet.size();++length) {
		CHECK(decoder.Decode(packet.data(),length,output)==MessageResult::Malformed);
		CHECK(decoder.CurrentGameState()==retained && decoder.Frame(1)==retainedFrame && output.configstrings[1].value=="after");
	}
	auto invalid=packet;invalid.push_back(0);CHECK(Feed(decoder,invalid,output)==MessageResult::Malformed && decoder.CurrentGameState()==retained);
	for(const auto &bad:std::vector<Bytes>{{3,0,4,0},{3,255,255,0},{3,0,0,'x',0},{3,9,0,255,0}})
		CHECK(Feed(decoder,bad,output)==MessageResult::Malformed && decoder.CurrentGameState()==retained);
	CHECK(!update.AddConfigstring(1024,"x") && !update.AddConfigstring(0,"\\protocol\\46") && Written(update)==packet);
	CHECK(!update.AddConfigstring(9,std::string(1024,'x')) && !update.AddConfigstring(9,std::string("a\0b",3)) && Written(update)==packet);
	for(std::size_t capacity=0;capacity<packet.size();++capacity) {
		MessageWriter small(capacity);CHECK(!small.AddConfigstring(9,"changed") && !small.Bytes() && small.AddNop()==bool(capacity));
	}
	// A completely full table still permits no-op/deletion, with atomic overflow.
	auto full=std::make_shared<GameState>();full->configstrings[0]="\\protocol\\33";
	std::size_t used=full->configstrings[0].size()+2;unsigned index=1;
	while(used<MaxConfigBytes) {
		const auto count=(std::min)(std::size_t(1023),MaxConfigBytes-used-1);
		full->configstrings[index++]=std::string(count,'x');used+=count+1;
	}
	update.Clear();CHECK(update.AddGameState(*full));CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Ok);
	const auto packed=decoder.CurrentGameState();CHECK(packed->configBytes==16000);
	update.Clear();CHECK(update.AddConfigstring(100,"x"));CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Malformed && decoder.CurrentGameState()==packed);
	update.Clear();CHECK(update.AddConfigstring(1,"") && update.AddConfigstring(100,"recovery"));
	CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Ok && decoder.CurrentGameState()->configstrings[100]=="recovery");
	CHECK(!decoder.Frame(1)); // Only the new gamestate above resets frame history.
	update.Clear();CHECK(update.AddGameState(*game) && update.AddConfigstring(9,"same packet"));
	CHECK(decoder.Decode(update.Data(),update.Bytes(),output)==MessageResult::Ok);
	CHECK(output.gamestate->configstrings[9]=="last" && decoder.CurrentGameState()->configstrings[9]=="same packet");
	CHECK(output.events.size()==2 && output.events[0].type==Type::GameState && output.events[1].type==Type::Configstring);
}

void Supplied(const char *path,bool valid) {
	std::ifstream file(path,std::ios::binary); CHECK(bool(file));
	const Bytes bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
	DemoReader reader(bytes.data(),bytes.size()); MessageDecoder decoder,regenerated; DecodedMessage message; DemoRecord record;
	unsigned messages=0,frames=0,entities=0,identical=0; ihv_cases::Digest digest; std::uint32_t first=0,last=0;
	Bytes regeneratedFile(bytes.size()*2); DemoWriter demo(regeneratedFile.data(),regeneratedFile.size());
	DemoRecordResult result;
	while((result=reader.Next(record))==DemoRecordResult::Message) {
		const auto status=decoder.Decode(record.data,record.size,message);
		if(!valid) { CHECK(status==MessageResult::MissingGamestate && !messages && !decoder.CurrentGameState()); std::puts("Incomplete supplied configstring-only demo rejected: missing gamestate"); return; }
		if(status!=MessageResult::Ok) std::fprintf(stderr,"Message %u: status %d\n",messages,static_cast<int>(status));
		CHECK(status==MessageResult::Ok); ++messages;
		MessageWriter encoded;
		for(const auto &event:message.events) {
			switch(event.type) {
			case MessageEvent::Type::Nop: CHECK(encoded.AddNop()); break;
			case MessageEvent::Type::GameState: CHECK(encoded.AddGameState(*message.gamestate)); break;
			case MessageEvent::Type::ServerCommand: CHECK(encoded.AddServerCommand(message.commands[event.index])); break;
			case MessageEvent::Type::Configstring: {
				const auto &c=message.configstrings[event.index]; CHECK(encoded.AddConfigstring(c.index,c.value)); break;
			}
			case MessageEvent::Type::Snapshot: {
				const auto previous=regenerated.Frame(message.snapshot->deltaNumber);
				CHECK(encoded.AddSnapshot(*decoder.CurrentGameState(),previous.get(),*message.snapshot)); break;
			}
			}
		}
		DecodedMessage roundtrip; CHECK(regenerated.Decode(encoded.Data(),encoded.Bytes(),roundtrip)==MessageResult::Ok);
		CHECK(regenerated.CurrentGameState()->configstrings==decoder.CurrentGameState()->configstrings);
		CHECK(regenerated.CurrentGameState()->baselines==decoder.CurrentGameState()->baselines && roundtrip.commands==message.commands);
		CHECK(demo.Append(encoded.Data(),encoded.Bytes()));
		if(encoded.Bytes()==record.size && !std::memcmp(encoded.Data(),record.data,record.size)) ++identical;
		else std::printf("Regenerated message %u: original %zu bytes, encoded %zu bytes (decoded states match)\n",messages-1,record.size,encoded.Bytes());
		if(message.snapshot) {
			const auto &s=*message.snapshot; if(!frames) first=s.serverTime; last=s.serverTime; ++frames;
			CHECK(roundtrip.snapshot && roundtrip.snapshot->player==s.player && roundtrip.snapshot->entities==s.entities);
			CHECK(roundtrip.snapshot->messageNumber==s.messageNumber && roundtrip.snapshot->serverTime==s.serverTime);
			CHECK(roundtrip.snapshot->deltaNumber==s.deltaNumber && roundtrip.snapshot->configSequence==s.configSequence);
			CHECK(roundtrip.snapshot->commandSequence==s.commandSequence && roundtrip.snapshot->flags==s.flags);
			CHECK(roundtrip.snapshot->areaBytes==s.areaBytes && roundtrip.snapshot->areaMask==s.areaMask);
			entities+=static_cast<unsigned>(s.entities.size()); digest.Word(s.messageNumber); digest.Word(s.serverTime);
			digest.Bytes(s.player.data(),s.player.size()); for(const auto &entity:s.entities) digest.Bytes(entity.data(),entity.size());
		}
	}
	CHECK(valid && result==DemoRecordResult::End && messages==57 && frames==56 && decoder.CurrentGameState());
	CHECK(demo.Finish());
	CHECK(entities==680 && first==1398225 && last==1400975 && digest.hash==0x21768873b0a37c0bull);
	// The first two full frames contain six unchanged absent-baseline records.
	// They decode to empty entities and disappear in canonical serialization.
	CHECK(identical==55 && demo.Position()==8610);
	std::printf("Supplied IHV demo: %u messages, %u snapshots, %u entity states, time %u..%u, state digest %016llx\n",
		messages,frames,entities,first,last,static_cast<unsigned long long>(digest.hash));
	std::printf("Demo regeneration: %u/%u byte-identical messages, %zu bytes\n",identical,messages,demo.Position());
}
int main(int argc,char **argv) {
	Messages(); Containers(); Mutations(); Writing(); ConfigUpdates(); std::puts("IHV message/container read, write, configstrings, malformed and rollback tests passed");
	if(argc>1) Supplied(argv[1],true); if(argc>2) Supplied(argv[2],false); return 0;
}
