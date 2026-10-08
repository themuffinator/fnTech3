/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "q3_ihv_client_cases.hpp"
#include "../code/compat/q3_ihv_server_session.hpp"
#include "../code/qcommon/q3_ihv_message.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace sv=fntech3::q3ihv::server;
namespace wire=fntech3::q3ihv::wire;
namespace cases=ihv_client_cases;

/* The original's write rules: bits from the low end, data at the byte count
 * with the bit position left alone, four bytes of headroom for a bit write and
 * a clearing overflow for a copy. */
static void Writes() {
	sv::OriginalMessage message(16);
	message.WriteBits(5,3);
	CHECK(message.Size()==1&&message.Data()[0]==5);
	message.WriteData("ab",2);
	CHECK(message.Size()==3&&message.Data()[1]=='a'&&message.Data()[2]=='b');
	// The next bit write continues the stale position, inside the last byte copied.
	message.WriteBits(1,1);
	CHECK(message.Size()==3&&message.Data()[2]==('b'|8));
	message.Clear();
	message.WriteByte(2);message.WriteShort(0x1234);message.WriteString("x");
	CHECK(message.Size()==5&&message.Data()[0]==2&&message.Data()[1]==0x34&&message.Data()[2]==0x12
		&&message.Data()[3]=='x'&&!message.Data()[4]&&!message.Overflowed());
	// A bit write needs four free bytes before it starts.
	for(int i=0;i<8;++i)message.WriteByte(i);
	CHECK(message.Size()==13&&!message.Overflowed());
	message.WriteByte(9);
	CHECK(message.Size()==13&&message.Overflowed());
	message.Clear();message.WriteData("0123456789",10);message.WriteData("abcdefg",7);
	CHECK(message.Overflowed()&&message.Size()==7&&!std::memcmp(message.Data(),"abcdefg",7));
	// A bit stream appended through bit writes keeps its exact bits.
	std::array<std::uint8_t,4> bits{0xa5,0x5a,0x03,0};
	sv::OriginalMessage appended(16);appended.WriteBits(1,2);appended.AppendBits(bits.data(),18);
	wire::Reader reader(appended.Data(),appended.Size());std::uint32_t value=0;
	CHECK(reader.Read(2,value)&&value==1&&reader.Read(16,value)&&value==0x5aa5&&reader.Read(2,value)&&value==3);
	std::printf("IHV server message writes: ok\n");
}

/* Every reader returns -1 and consumes nothing when its value is incomplete. */
static void Reads() {
	const std::uint8_t data[]={0xfe,0xff,0x34,0x12,'h','i',0,'a',0xff,'b'};
	sv::OriginalReader input(data,sizeof data);
	CHECK(input.ReadChar()==-2&&input.ReadByte()==0xff&&input.ReadShort()==0x1234);
	CHECK(input.ReadString()=="hi"&&input.ReadString()=="a"&&input.ReadString()=="b");
	CHECK(input.ReadByte()==-1&&input.ReadLong()==-1);
	const std::uint8_t three[]={1,2,3};
	sv::OriginalReader shortLong(three,sizeof three);
	CHECK(shortLong.ReadLong()==-1&&shortLong.ReadShort()==0x201&&shortLong.ReadShort()==-1&&shortLong.ReadByte()==3);
	std::vector<std::uint8_t> longText(1200,'z');longText.push_back(0);
	sv::OriginalReader text(longText.data(),longText.size());
	CHECK(text.ReadString().size()==1023&&text.ReadString().size()==1200-1023);
	// A command the client's writer produced reads back exactly.
	wire::Command from{},to{};
	for(unsigned i=0;i<14;++i)to[i]=static_cast<std::uint8_t>(i*37+5);
	to[3]=from[3];to[13]=from[13];
	std::array<std::uint8_t,64> buffer{};wire::Writer out(buffer.data(),buffer.size());
	CHECK(wire::WriteCommand(out,from,to));
	sv::OriginalReader commands(buffer.data(),out.Bytes());wire::Command read{};
	commands.ReadCommand(from,read);CHECK(read==to);
	// An absent mask reads as every field missing.
	sv::OriginalReader empty(buffer.data(),0);wire::Command missing{};
	empty.ReadCommand(from,missing);
	CHECK(missing[0]==0xff&&missing[4]==0xff&&missing[5]==0xff&&missing[10]==0xff&&missing[1]==0xff&&missing[2]==0xff);
	std::printf("IHV server message reads: ok\n");
}

static std::vector<std::uint8_t> MoveBytes(int serverId,int delta,int ack,int last,int count) {
	std::vector<std::uint8_t> buffer(1024);wire::Writer out(buffer.data(),buffer.size());
	out.Write(static_cast<std::uint32_t>(serverId),32);out.Write(1234,32);out.Write(static_cast<std::uint32_t>(delta),32);
	out.Write(static_cast<std::uint32_t>(ack),32);out.Write(static_cast<std::uint32_t>(last),32);
	out.Write(static_cast<std::uint32_t>(count),8);
	wire::Command from{};
	for(int i=0;i<count&&i<32;++i){wire::Command to{};to[0]=static_cast<std::uint8_t>(10+i);to[4]=static_cast<std::uint8_t>(i);CHECK(wire::WriteCommand(out,from,to));from=to;}
	buffer.resize(out.Bytes());return buffer;
}

static void Moves() {
	auto bytes=MoveBytes(77,5,9,40,3);
	sv::OriginalReader input(bytes.data(),bytes.size());sv::Move move;
	CHECK(sv::ReadMove(input,move)==sv::MoveStatus::Ok);
	CHECK(move.serverId==77&&move.time==1234&&move.deltaMessage==5&&move.configAck==9&&move.lastCommand==40&&move.count==3);
	CHECK(move.commands[0][0]==10&&move.commands[2][0]==12&&move.commands[2][4]==2&&input.ReadByte()==-1);
	bytes=MoveBytes(77,-1,-1,40,1);
	sv::OriginalReader low(bytes.data(),bytes.size());
	CHECK(sv::ReadMove(low,move)==sv::MoveStatus::CountLow&&move.deltaMessage==-1&&move.configAck==-1);
	// The commands after a refused count are left unread, as the original leaves them.
	CHECK(low.ReadByte()!=-1);
	bytes=MoveBytes(77,6,8,40,33);
	sv::OriginalReader high(bytes.data(),bytes.size());
	CHECK(sv::ReadMove(high,move)==sv::MoveStatus::CountHigh&&move.deltaMessage==6&&move.configAck==8);
	// Missing longs read as -1 and leave their bytes for the next reader.
	const std::uint8_t cut[]={1,0,0,0,1};
	sv::OriginalReader truncated(cut,sizeof cut);
	CHECK(sv::ReadMove(truncated,move)==sv::MoveStatus::CountLow&&move.serverId==1&&move.time==-1
		&&move.lastCommand==-1&&move.count==1);

	// Three commands ending at 40 are 39, 40 and the provisional 41.
	auto plan=sv::PlanMove(38,40,3);
	CHECK(!plan.lost&&!plan.replays&&plan.begin==0&&plan.end==2);
	plan=sv::PlanMove(39,40,3);
	CHECK(!plan.lost&&!plan.replays&&plan.begin==1&&plan.end==2);
	// A gap of three is reported and filled three times with the first command.
	plan=sv::PlanMove(35,40,3);
	CHECK(plan.lost==3&&plan.replays==3&&plan.begin==0&&plan.end==2);
	// A long gap is filled at most five times.
	plan=sv::PlanMove(0,100,2);
	CHECK(plan.lost==99&&plan.replays==5&&plan.begin==0&&plan.end==1);
	// Commands already run, or a client counting from behind, run nothing.
	plan=sv::PlanMove(40,40,3);CHECK(!plan.lost&&plan.begin==plan.end);
	plan=sv::PlanMove(500,40,3);CHECK(!plan.lost&&plan.begin==plan.end);
	plan=sv::PlanMove(0,0x7fffffff,32);CHECK(plan.replays==5&&plan.begin==0&&plan.end==31);
	plan=sv::PlanMove(0x7fffffff,-0x7fffffff-1,32);CHECK(!plan.lost&&plan.begin==plan.end);
	std::printf("IHV server moves: ok\n");
}

static void Replies() {
	CHECK(sv::InfoReply("noname","tim_dm1",8,2)=="info\n33 \"noname\" \"tim_dm1\" 8 2\n");
	std::vector<sv::StatusPlayer> players{{5,40,"one"},{-2,999,"two"}};
	CHECK(sv::StatusReply("","\\a\\b",players)=="status\n\n\\a\\b\n5 40 \"one\"\n-2 999 \"two\"\n");
	CHECK(sv::StatusReply("x","",{})=="status\nx\n\n");
	std::vector<sv::StatusPlayer> many(200,{1,2,std::string(20,'n')});
	const auto reply=sv::StatusReply("","",many);
	CHECK(reply.size()-std::strlen("status\n\n")<1400&&reply.size()>1300);

	auto next=sv::ScheduleSnapshot(5000,true,0,300,5000,50);CHECK(next.time==4999&&!next.rateDelayed);
	next=sv::ScheduleSnapshot(5000,false,3900,300,5000,50);CHECK(next.time==6000&&!next.rateDelayed);
	next=sv::ScheduleSnapshot(5000,false,4900,200,5000,50);CHECK(next.time==5050&&!next.rateDelayed);
	next=sv::ScheduleSnapshot(5000,false,4900,1000,5000,50);CHECK(next.time==5000+209&&next.rateDelayed);
	next=sv::ScheduleSnapshot(5000,false,4900,1000,0,50);CHECK(next.rateDelayed);

	int sent[32]={},acked[32]={};
	CHECK(sv::Ping(sent,acked,32)==0);
	for(int i=0;i<32;++i){sent[i]=1000+i;acked[i]=0;}
	CHECK(sv::Ping(sent,acked,32)==999);
	acked[3]=1103;acked[4]=1054;CHECK(sv::Ping(sent,acked,32)==75);
	acked[5]=9000;CHECK(sv::Ping(sent,acked,32)==999);
	std::printf("IHV server replies, schedule and ping: ok\n");
}

/* A gamestate written with the original's primitives, after a partial byte or
 * not, is the one the client's decoder reads; the aligned one equals the
 * shared writer's bytes. */
static void Gamestate() {
	const auto game=cases::Game();
	sv::OriginalMessage message(wire::MaxMessageBytes);
	message.WriteByte(2);
	for(unsigned i=0;i<game.configstrings.size();++i){
		if(game.configstrings[i].empty())continue;
		message.WriteByte(3);message.WriteShort(static_cast<int>(i));message.WriteString(game.configstrings[i].c_str());
	}
	const wire::Entity empty{};
	for(const auto &baseline:game.baselines){
		if(!baseline[2]&&!baseline[0x95]&&!baseline[0x94]&&!baseline[4]&&!baseline[5])continue;
		std::array<std::uint8_t,512> bits{};wire::Writer out(bits.data(),bits.size());
		CHECK(wire::WriteEntity(out,empty,&baseline,true));
		message.WriteByte(4);message.AppendBits(bits.data(),out.Position());
	}
	message.WriteByte(0);
	wire::MessageWriter shared;CHECK(shared.AddGameState(game));
	CHECK(message.Size()==shared.Bytes()&&!std::memcmp(message.Data(),shared.Data(),shared.Bytes()));
	wire::MessageDecoder decoder;wire::DecodedMessage decoded;
	CHECK(decoder.Decode(message.Data(),message.Size(),decoded)==wire::MessageResult::Ok&&decoded.gamestate);
	std::printf("IHV server gamestate: %zu bytes, ok\n",message.Size());
}

int main() {
	Writes();Reads();Moves();Replies();Gamestate();
	return 0;
}
