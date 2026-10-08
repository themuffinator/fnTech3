/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "q3_ihv_client_cases.hpp"
#include "../code/client/client_ihv_session.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <iterator>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace fntech3::q3ihv::client;
namespace cases=ihv_client_cases;
namespace wire=fntech3::q3ihv::wire;

static void Histories() {
	cases::Digest digest;unsigned count=0;
	for(std::uint32_t current:{0u,1u,31u,32u,63u,64u,65u,1023u,1024u,1025u,0x7fffffffu,0x80000000u,0x800003ffu,0xfffffc00u,0xfffffffeu,0xffffffffu}) {
		CommandHistory commands(current-64);EntityHistory entities(current-1024);
		for(unsigned i=0;i<64;++i) {
			fntech3_q3_ihv_usercmd_t record;const auto slot=(current-63+i)&63;
			for(unsigned b=0;b<14;++b)record.bytes[b]=static_cast<std::uint8_t>((slot*14+b)*73+17);
			commands.Append(record);
		}
		for(unsigned i=0;i<1024;++i) {
			fntech3_q3_ihv_entityState_t record;const auto slot=(current-1024+i)&1023;
			for(unsigned b=0;b<164;++b)record.bytes[b]=static_cast<std::uint8_t>((slot*164+b)*113+61);
			entities.Append(record);
		}
		CHECK(commands.Counter()==current&&entities.Counter()==current);
		std::vector<std::uint32_t> queries;
		for(int delta=-1025;delta<=1;++delta)queries.push_back(current+static_cast<std::uint32_t>(delta));
		for(std::uint32_t n:{0u,1u,63u,64u,1023u,1024u,0x7fffffffu,0x80000000u,0xffffffffu})queries.push_back(n);
		for(auto query:queries)for(unsigned kind=0;kind<2;++kind) {
			std::array<std::uint8_t,180> output;output.fill(0xa5);
			const auto status=kind?entities.Copy(query,output.data()+8,164):commands.Copy(query,output.data()+8,14);
			digest.Word(current);digest.Word(query);digest.Word(kind);digest.Word(static_cast<unsigned>(status));
			digest.Bytes(output.data(),output.size());++count;
		}
	}
	CHECK(count==33152&&digest.value==0xb009f6ef775c6672ull);
	CommandHistory commands;EntityHistory entities;std::array<std::uint8_t,180> output;output.fill(0xa5);const auto before=output;
	CHECK(commands.Copy(0,nullptr,14)==ReadResult::InvalidOutput&&commands.Copy(0,output.data(),13)==ReadResult::InvalidOutput);
	CHECK(entities.Copy(0,nullptr,164)==ReadResult::InvalidOutput&&entities.Copy(0,output.data(),163)==ReadResult::InvalidOutput&&output==before);
	CHECK(commands.Copy(1,output.data(),14)==ReadResult::Unavailable&&entities.Copy(0,output.data(),164)==ReadResult::FutureEntity&&output==before);
	std::printf("IHV native histories: %u original cases, %016llx\n",count,static_cast<unsigned long long>(digest.value));
}
static std::shared_ptr<const wire::GameState> Start(wire::MessageDecoder &decoder,ClientData &data) {
	wire::MessageWriter out;CHECK(out.AddGameState(cases::Game()));wire::DecodedMessage decoded;
	CHECK(decoder.Decode(out.Data(),out.Bytes(),decoded)==wire::MessageResult::Ok);
	CHECK(data.BeginGameState(decoded.gamestate->nativeConfigstrings));return decoded.gamestate;
}
static void Publication() {
	auto data=std::make_unique<ClientData>();wire::MessageDecoder decoder;
	std::array<std::uint8_t,20116> output;output.fill(0xa5);const auto untouched=output;
	CHECK(!data->Ready()&&!data->CurrentSnapshot()&&!data->CopyGameState(output.data()+8,20100));
	fntech3_q3_ihv_usercmd_t command{};CHECK(!data->AppendCommand(command));
	const auto game=Start(decoder,*data);CHECK(data->Ready()&&data->CopyGameState(output.data()+8,20100));
	CHECK(!std::memcmp(output.data()+8,&game->nativeConfigstrings.Record(),20100));
	CHECK(!std::memcmp(output.data(),untouched.data(),8)&&!std::memcmp(output.data()+20108,untouched.data()+20108,8));
	for(unsigned i=0;i<14;++i)command.bytes[i]=static_cast<std::uint8_t>(i*17);
	CHECK(data->AppendCommand(command)&&data->CurrentCommandNumber()==1);
	fntech3_q3_ihv_usercmd_t read{};CHECK(data->CopyCommand(1,&read,sizeof read)==ReadResult::Ok&&!std::memcmp(&read,&command,sizeof read));
	wire::Snapshot source=cases::Frame(1,nullptr);wire::MessageWriter writer;CHECK(writer.AddSnapshot(*game,nullptr,source));wire::DecodedMessage decoded;
	CHECK(decoder.Decode(writer.Data(),writer.Bytes(),decoded)==wire::MessageResult::Ok);
	CHECK(data->PublishSnapshot(*decoded.snapshot,37)&&data->EntitySequence()==1);
	const auto before=*data->CurrentSnapshot();const auto previous=decoded.snapshot->entities[0];
	for(unsigned mode=0;mode<6;++mode) {
		auto invalid=*decoded.snapshot;invalid.firstEntity=data->EntitySequence();
		if(mode==0)++invalid.firstEntity;
		if(mode==1)invalid.areaBytes=33;
		if(mode==2)invalid.entities[0][0]=0;
		if(mode==3)wire::Store(invalid.entities[0].data(),2,1024);
		if(mode==4)invalid.entities.push_back(invalid.entities[0]);
		if(mode==5)invalid.entities.resize(1024);
		CHECK(!data->PublishSnapshot(invalid,999)&&data->EntitySequence()==1);
		CHECK(!std::memcmp(&before,data->CurrentSnapshot(),sizeof before));
		wire::Entity entity{};CHECK(data->CopyEntity(0,entity.data(),entity.size())==ReadResult::Ok&&entity==previous);
	}
	bool changed=true;CHECK(!data->SetConfigstring(1024,"x",changed)&&changed);
	CHECK(!data->SetConfigstring(0,"\\protocol\\46",changed)&&changed);
	CHECK(data->SetConfigstring(7,"native update",changed)&&changed&&data->CurrentSnapshot());
	CHECK(data->CurrentCommandNumber()==1&&data->EntitySequence()==1);
	const auto *alias=&data->Configstrings();CHECK(data->BeginGameState(*alias));
	CHECK(!data->CurrentSnapshot()&&data->EntitySequence()==0&&data->CurrentCommandNumber()==0);
	CHECK(data->Configstrings().String(7)=="native update");
	ConfigStrings wrong;CHECK(wrong.Append(0,"\\protocol\\46"));CHECK(!data->BeginGameState(wrong)&&data->Ready());
	data->Clear();CHECK(!data->Ready()&&!data->CurrentSnapshot()&&!data->CopyGameState(output.data(),output.size()));
}
static void Snapshots() {
	ClientData data;wire::MessageDecoder decoder;const auto game=Start(decoder,data);
	cases::Digest digest;std::shared_ptr<const wire::Snapshot> previous;unsigned total=0;
	for(unsigned index=0;index<cases::Frames;++index) {
		auto frame=cases::Frame(index,previous.get());wire::MessageWriter writer;
		const auto base=(frame.deltaNumber&&!(frame.deltaNumber&0x80000000u))?previous.get():nullptr;
		CHECK(writer.AddSnapshot(*game,base,frame));wire::DecodedMessage decoded;
		CHECK(decoder.Decode(writer.Data(),writer.Bytes(),decoded)==wire::MessageResult::Ok);
		CHECK(data.PublishSnapshot(*decoded.snapshot,cases::Ping(index)));
		digest.Word(index);digest.Bytes(data.CurrentSnapshot(),sizeof(*data.CurrentSnapshot()));
		for(std::size_t n=0;n<decoded.snapshot->entities.size();++n) {
			wire::Entity entity{};
			CHECK(data.CopyEntity(static_cast<std::uint32_t>(decoded.snapshot->firstEntity+n),entity.data(),entity.size())==ReadResult::Ok);
			CHECK(entity==decoded.snapshot->entities[n]);digest.Bytes(entity.data(),entity.size());++total;
		}
		previous=decoded.snapshot;
	}
	std::printf("IHV native snapshots: %u frames, %u entities, %016llx\n",cases::Frames,total,static_cast<unsigned long long>(digest.value));
	CHECK(total==4583&&digest.value==0x5179d02fca70b215ull);
}
struct Events final : ClientEvents {
	std::vector<std::string> seen;
	ClientSession *session=nullptr;
	int mode=0;
	void GameState(const ClientData &data) override { CHECK(data.Ready()); seen.push_back("game"); }
	void Configstring(const ClientData &data,std::uint32_t index) override { seen.emplace_back("config:"+std::string(data.Configstrings().String(index))); }
	void ServerCommand(const ClientData &data,std::string_view text) override {
		seen.emplace_back("command:"+std::string(text)+":"+std::string(data.Configstrings().String(9)));
		if(mode==1)throw std::runtime_error("module error");
		if(mode==2)session->Clear();
		if(mode==3) {
			const std::uint8_t nop=1;CHECK(session->Feed(&nop,1,0,*this).code==PacketResult::Reentrant);
			fntech3_q3_ihv_usercmd_t command{};CHECK(session->AppendCommand(command));
		}
	}
	void Snapshot(const ClientData &data,const fntech3_q3_ihv_snapshot_t &snapshot) override {
		CHECK(&snapshot==data.CurrentSnapshot());
		seen.emplace_back("snapshot:"+std::string(data.Configstrings().String(9)));
		const auto first=wire::Load(snapshot.bytes+344,4),count=wire::Load(snapshot.bytes+340,4);
		for(std::uint32_t i=0;i<count;++i) {
			wire::Entity entity{};CHECK(data.CopyEntity(first+i,entity.data(),entity.size())==ReadResult::Ok);
		}
	}
};
static void Session() {
	auto session=std::make_unique<ClientSession>();Events events;events.session=session.get();
	wire::MessageWriter writer;auto game=cases::Game();game.configstrings[9]="initial";
	CHECK(writer.AddGameState(game));
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered);
	CHECK(events.seen==std::vector<std::string>{"game"});events.seen.clear();
	writer.Clear();const auto snapshot=cases::Frame(1,nullptr);
	CHECK(writer.AddConfigstring(9,"before")&&writer.AddServerCommand("one")&&writer.AddNop());
	CHECK(writer.AddSnapshot(game,nullptr,snapshot)&&writer.AddConfigstring(9,"after")&&writer.AddServerCommand("two"));
	CHECK(session->Feed(writer.Data(),writer.Bytes(),41,events).code==PacketResult::Delivered);
	CHECK(events.seen==std::vector<std::string>({"config:before","command:one:before","snapshot:before","config:after","command:two:after"}));
	CHECK(wire::Load(session->Data().CurrentSnapshot()->bytes+24,4)==41);
	const auto retained=*session->Data().CurrentSnapshot();const auto sequence=session->Data().EntitySequence();events.seen.clear();
	writer.Clear();CHECK(writer.AddConfigstring(9,"changed")&&writer.AddServerCommand("must not run"));
	std::vector<std::uint8_t> malformed(writer.Data(),writer.Data()+writer.Bytes());malformed.push_back(0);
	const auto bad=session->Feed(malformed.data(),malformed.size(),0,events);
	CHECK(bad.code==PacketResult::Rejected&&bad.message==wire::MessageResult::Malformed&&events.seen.empty());
	CHECK(session->Data().Configstrings().String(9)=="after"&&session->Data().EntitySequence()==sequence);
	CHECK(!std::memcmp(&retained,session->Data().CurrentSnapshot(),sizeof retained));
	writer.Clear();CHECK(writer.AddConfigstring(9,"after")&&writer.AddNop());
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered&&events.seen.empty());
	writer.Clear();CHECK(writer.AddServerCommand("reentrant"));events.mode=3;
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered&&session->Data().CurrentCommandNumber()==1);
	events.mode=1;bool threw=false;
	try { session->Feed(writer.Data(),writer.Bytes(),0,events); }catch(const std::runtime_error &) { threw=true; }
	CHECK(threw&&!session->Data().Ready()&&!session->Data().CurrentSnapshot());
	events.mode=0;writer.Clear();CHECK(writer.AddGameState(game));
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered);
	writer.Clear();CHECK(writer.AddServerCommand("disconnect")&&writer.AddConfigstring(9,"must not publish"));
	events.mode=2;events.seen.clear();CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Aborted);
	CHECK(!session->Data().Ready()&&events.seen==std::vector<std::string>{"command:disconnect:initial"});
	events.mode=0;CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Rejected);
	writer.Clear();CHECK(writer.AddGameState(game));
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered);
	fntech3_q3_ihv_usercmd_t command{};CHECK(session->AppendCommand(command));events.seen.clear();
	CHECK(session->Feed(nullptr,1,0,events).code==PacketResult::Rejected);
	CHECK(session->Feed(writer.Data(),0,0,events).code==PacketResult::Rejected&&events.seen.empty());
	CHECK(writer.AddConfigstring(9,"before")&&writer.AddSnapshot(game,nullptr,snapshot)&&writer.AddConfigstring(9,"after"));
	malformed.assign(writer.Data(),writer.Data()+writer.Bytes());malformed.push_back(0);
	CHECK(session->Feed(malformed.data(),malformed.size(),0,events).code==PacketResult::Rejected&&events.seen.empty());
	CHECK(session->Data().CurrentCommandNumber()==1&&!session->Data().CurrentSnapshot()&&session->Data().Configstrings().String(9)=="initial");
	CHECK(session->Feed(writer.Data(),writer.Bytes(),0,events).code==PacketResult::Delivered);
	CHECK(events.seen==std::vector<std::string>({"game","config:before","snapshot:before","config:after"}));
	CHECK(!session->Data().CurrentCommandNumber()&&session->Data().EntitySequence()==1);
}
static void Supplied(const char *path,bool valid) {
	std::ifstream file(path,std::ios::binary);CHECK(bool(file));
	const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
	wire::DemoReader reader(bytes.data(),bytes.size());wire::DemoRecord record;
	ClientSession session;Events events;events.session=&session;
	unsigned messages=0,frames=0,entities=0;cases::Digest digest;
	wire::DemoRecordResult status;
	while((status=reader.Next(record))==wire::DemoRecordResult::Message) {
		events.seen.clear();const auto result=session.Feed(record.data,record.size,0,events);
		if(!valid) {
			CHECK(result.code==PacketResult::Rejected&&result.message==wire::MessageResult::MissingGamestate);
			CHECK(events.seen.empty()&&!session.Data().Ready()&&!messages);std::puts("Supplied incomplete demo rejected without callbacks");return;
		}
		CHECK(result.code==PacketResult::Delivered);++messages;
		for(const auto &event:events.seen)if(event.rfind("snapshot:",0)==0) {
			const auto *snapshot=session.Data().CurrentSnapshot();CHECK(snapshot);++frames;
			digest.Bytes(snapshot,sizeof(*snapshot));
			const auto first=wire::Load(snapshot->bytes+344,4),count=wire::Load(snapshot->bytes+340,4);
			for(std::uint32_t n=0;n<count;++n) {
				wire::Entity entity{};CHECK(session.Data().CopyEntity(first+n,entity.data(),entity.size())==ReadResult::Ok);
				digest.Bytes(entity.data(),entity.size());++entities;
			}
		}
	}
	CHECK(valid&&status==wire::DemoRecordResult::End&&messages==57&&frames==56&&entities==680);
	// Complete native records and history bytes matched the original parser.
	CHECK(digest.value==0x4582fb032c471ba6ull);
	std::printf("Supplied native client data replay: %u messages, %u snapshots, %u entities, %016llx\n",messages,frames,entities,static_cast<unsigned long long>(digest.value));
}
/* The live client's additions: each frame rewrites the newest command and
 * appends a provisional one (CL_SendCmd 0x4033d9..0x403440), and a frame's ping
 * is computed from the decoded frame (CL_ParseFrame 0x406ab2). */
static void LiveAdditions() {
	auto data=std::make_unique<ClientData>();wire::MessageDecoder decoder;Start(decoder,*data);
	fntech3_q3_ihv_usercmd_t finished{},provisional{},second{},out{};finished.bytes[0]=10;second.bytes[0]=20;
	CHECK(data->ReplaceNewestCommand(finished)&&data->AppendCommand(provisional)&&data->CurrentCommandNumber()==1);
	CHECK(data->CopyCommand(0,&out,14)==ReadResult::Ok&&out.bytes[0]==10);
	CHECK(data->CopyCommand(1,&out,14)==ReadResult::Ok&&out.bytes[0]==0);
	CHECK(data->ReplaceNewestCommand(second)&&data->AppendCommand(provisional)&&data->CurrentCommandNumber()==2);
	CHECK(data->CopyCommand(1,&out,14)==ReadResult::Ok&&out.bytes[0]==20&&data->CopyCommand(0,&out,14)==ReadResult::Ok&&out.bytes[0]==10);
	ClientData empty;CHECK(!empty.ReplaceNewestCommand(finished));
	struct Events final:ClientEvents {
		std::uint32_t ping=0;unsigned frames=0;
		void GameState(const ClientData&)override{}
		void Configstring(const ClientData&,std::uint32_t)override{}
		void ServerCommand(const ClientData&,std::string_view)override{}
		void Snapshot(const ClientData&,const fntech3_q3_ihv_snapshot_t &s)override{ping=wire::Load(s.bytes+24,4);++frames;}
	} events;
	auto session=std::make_unique<ClientSession>();const auto game=cases::Game();
	wire::MessageWriter start;CHECK(start.AddGameState(game));
	CHECK(session->Feed(start.Data(),start.Bytes(),0,events).code==PacketResult::Delivered);
	unsigned asked=0;const auto frame=cases::Frame(1,nullptr);
	wire::MessageWriter snapshot;CHECK(snapshot.AddSnapshot(game,nullptr,frame));
	CHECK(session->FeedWith(snapshot.Data(),snapshot.Bytes(),[&](const wire::Snapshot &s){++asked;return s.commandSequence-0xfedc0000u+500;},events).code==PacketResult::Delivered);
	CHECK(asked==1&&events.frames==1&&events.ping==501);
	wire::MessageWriter command;CHECK(command.AddServerCommand("x"));
	CHECK(session->FeedWith(command.Data(),command.Bytes(),[&](const wire::Snapshot &){++asked;return 7u;},events).code==PacketResult::Delivered&&asked==1);
}

int main(int argc,char **argv){Histories();Publication();Snapshots();Session();LiveAdditions();std::puts("IHV native client records, packet staging and ordered callbacks passed");
	if(argc>1)Supplied(argv[1],true);if(argc>2)Supplied(argv[2],false);
}
