/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifdef _MSC_VER
// The metadata refuses 64-bit hosts with a sizeof test, constant there; the
// engine disables this warning in q_shared.h.
#pragma warning(disable: 4127)
#endif
#include "q3_ihv_model_tag_cases.hpp"
#include <climits>
#include <stdexcept>
#include <string>

static void Check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
using fntech3::q3ihv::ModelTags;
namespace tc=ihv_tag_cases;
static void Rejections() {
	const auto good=tc::Model();ModelTags model;
	for(std::size_t size=0;size<good.size();++size) {
		Check(model.Load(good.data(),good.size()),"Prior good model");
		Check(!model.Load(good.data(),size)&&!model.Frames()&&!model.Find(0,"tag_0"),"Truncated model clears old state");
	}
	for(auto mutation:{std::array<unsigned,2>{0,0},{4,14},{76,0},{76,0xffffffffu},{80,0xffffffffu},
		{92,0},{92,0xffffffffu},{96,108},{96,0xffffffffu},{104,0},{104,0xffffffffu}}) {
		auto data=good;tc::Word(data,mutation[0],mutation[1]);Check(!model.Load(data.data(),data.size()),"Invalid model header");
	}
	Check(!model.Load(nullptr,good.size()),"Null input");
	auto data=good;std::memset(data.data()+332,'x',64);Check(!model.Load(data.data(),data.size()),"Unterminated tag name");
	for(unsigned member=0;member<12;++member)for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u,0x7fa00000u}) {
		data=good;tc::Word(data,332+64+member*4,bad);Check(!model.Load(data.data(),data.size()),"Nonfinite tag pose");
	}
	for(unsigned member=0;member<10;++member)for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u}) {
		data=good;tc::Word(data,108+member*4,bad);Check(!model.Load(data.data(),data.size()),"Nonfinite frame metadata");
	}
	data=tc::Model(2,0);Check(model.Load(data.data(),data.size())&&model.Frames()==2&&!model.TagsPerFrame()&&!model.Find(0,"tag_0"),"Model without tags");
	std::vector<std::uint8_t> unaligned(good.size()+1);std::memcpy(unaligned.data()+1,good.data(),good.size());
	Check(model.Load(unaligned.data()+1,good.size()),"Unaligned little endian input");
	fntech3_q3_ihv_tag_t output;std::memset(&output,0xa5,sizeof output);const auto before=output;
	for(auto name:{"Tag_0","missing","","tag_00"})Check(!model.Lerp(0,1,0.5f,name,output)&&!std::memcmp(&output,&before,sizeof output),"Missing tag preserves output");
	Check(!model.Lerp(-1,0,0.5f,"tag_0",output)&&!model.Lerp(0,-1,0.5f,"tag_0",output),"Negative frames");
	for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u}){float fraction;std::memcpy(&fraction,&bad,4);Check(!model.Lerp(0,1,fraction,"tag_0",output),"Nonfinite fraction");}
	Check(!model.Lerp(0,3,(std::numeric_limits<float>::max)(),"tag_0",output),"Extrapolation overflow");
	Check(!std::memcmp(&output,&before,sizeof output),"All failures preserve complete output");
	model.Clear();Check(!model.Frames()&&!model.TagsPerFrame()&&!model.Lerp(0,0,0,"tag_0",output),"Clear");
}
static void NativeRecords() {
	const auto data=tc::Model();ModelTags tags;Check(tags.Load(data.data(),data.size()),"Frame fixture");
	for(unsigned frame=0;frame<4;++frame) {
		const auto &value=tags.FrameData()[frame];
		for(unsigned i=0;i<3;++i)Check(value.mins[i]==-float(frame+i+1)&&value.maxs[i]==float(frame+i+2)&&value.localOrigin[i]==float(frame+i)*0.25f,"Frame pose fields");
		Check(value.radius==float(frame+4)&&!std::memcmp(value.name,"generated frame",15),"Frame radius/name");
	}
	fntech3::q3ihv::NativeModelMetadata model;
	if constexpr(sizeof(void*)!=4){Check(!model.Load("model.md3",data.data(),data.size())&&!model.Record(),"Native pointers require x86");return;}
	Check(model.Load("model.md3",data.data(),data.size()),"Native registration");
	const auto *record=model.Record();Check(record&&record->isMD3==1&&record->frameCount==4&&record->tagCount==3&&!std::strcmp(record->name,"model.md3"),"Native descriptor fields");
	Check(record->frames==reinterpret_cast<std::uintptr_t>(model.Metadata().FrameData())&&record->tags==reinterpret_cast<std::uintptr_t>(model.Metadata().TagData()),"Native pointers refer to owned arrays");
	const auto *frames=reinterpret_cast<const fntech3_q3_ihv_model_frame_t*>(static_cast<std::uintptr_t>(record->frames));
	Check(frames[0].mins[2]==-3&&frames[3].maxs[2]==7,"Direct native frame reads");
	fntech3_q3_ihv_tag_t pose{};Check(model.Metadata().Lerp(0,2,0.5f,"tag_0",pose)&&model.Record()==record,"Stable registration address");
	Check(model.Load(record->name,data.data(),data.size())&&!std::strcmp(model.Record()->name,"model.md3"),"Aliased registration name");
	for(const auto &name:{std::string(),std::string(64,'x'),std::string("a\0b",3)})Check(!model.Load(name,data.data(),data.size())&&!model.Record()&&!model.Metadata().Frames(),"Invalid native registration clears previous record");
	Check(model.Load(std::string(63,'x'),data.data(),data.size())&&model.Record()->name[63]==0,"Maximum native model name");
	Check(!model.Load("model.md3",data.data(),107)&&!model.Record(),"Failed native metadata clears registration");
	Check(model.Load("model.md3",data.data(),data.size()),"Native reload");model.Clear();Check(!model.Record()&&!model.Metadata().FrameData()&&!model.Metadata().TagData(),"Native shutdown");
}
int main() {
	try {
		Rejections();NativeRecords();const auto data=tc::Model();ModelTags model;Check(model.Load(data.data(),data.size()),"Load generated model");
		Check(model.Frames()==4&&model.TagsPerFrame()==3,"Metadata counts");
		unsigned cases=0;
		for(int start:{0,1,2,3,4,1024,INT_MAX})for(int end:{0,1,2,3,4,1024,INT_MAX})for(unsigned tag=0;tag<3;++tag)for(float fraction:{-0.5f,0.0f,0.125f,0.5f,1.0f,1.5f}) {
			const auto name="tag_"+std::to_string(tag);fntech3_q3_ihv_tag_t output{};std::memset(output.name,0xa5,64);
			Check(model.Lerp(start,end,fraction,name,output),"Valid interpolation");
			for(char c:output.name)Check(static_cast<unsigned char>(c)==0xa5,"Name bytes preserved");
			const auto *a=model.Find(start,name),*b=model.Find(end,name);
			for(unsigned i=0;i<3;++i) {
				const double expected=(1.0-double(fraction))*a->origin[i]+double(fraction)*b->origin[i];
				Check(std::abs(double(output.origin[i])-expected)<1e-6,"Interpolated origin");
				const auto &axis=output.axis[i];const double length=double(axis[0])*axis[0]+double(axis[1])*axis[1]+double(axis[2])*axis[2];
				Check(tag==2?length==0:std::abs(length-1.0)<1e-6,"Independent normalized axes");
			}++cases;
		}
		std::printf("IHV model tags: %u interpolation cases plus malformed inputs, bounds and lifecycle passed\n",cases);return 0;
	}catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
