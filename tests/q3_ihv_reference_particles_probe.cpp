/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Windows x86 observations against the user-supplied, hash-pinned IHV executable.
 * No original entry point, graphics, input, audio or network startup. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/renderercommon/tr_q3_ihv_particles.h"
#include <cstdint>
#include <cstdarg>
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }
static void LoadReference(const char *path) {
	std::ifstream input(path,std::ios::binary); Check(bool(input),"Open original executable");
	std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
	HCRYPTPROV provider{}; HCRYPTHASH hash{};
	Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
	if (!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) { CryptReleaseContext(provider,0); Check(false,"Hash create"); }
	BYTE digest[32]{}; DWORD count=32;
	const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE *>(data.data()),static_cast<DWORD>(data.size()),0)
		&& CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0);
	CryptDestroyHash(hash); CryptReleaseContext(provider,0); Check(ok && count==32,"SHA-256");
	char text[65]{}; for(int i=0;i<32;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
	Check(!std::strcmp(text,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37"),"Unexpected original executable");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x3f0000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x442000,0x1000},{0x45b000,0x1000},{0x439000,0x1000},{0x448000,0x1000},{0x46a000,0x1000},{0x42a000,0x1000},{0x43c000,0x1000},{0x46b000,0x1000}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
			const unsigned start=0x400000+sections[i].VirtualAddress,end=start+sections[i].SizeOfRawData;
			const unsigned from=address>start?address:start,to=address+length<end?address+length:end;
			if(from>=to) continue;
			const std::size_t offset=sections[i].PointerToRawData+from-start;
			Check(offset<=data.size() && to-from<=data.size()-offset,"Section bounds");
			std::memcpy(reinterpret_cast<void *>(from),data.data()+offset,to-from);
		}
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
}

#include <cmath>
#include <algorithm>
struct Particle { float xyz[3]; unsigned rgba; };
static std::array<std::array<float,4>,1024> vertices;
static std::array<std::array<float,2>,1024> uv;
static std::array<unsigned,1024> colors;
static std::vector<Particle> particles;
static std::array<float,3> origin,forward,right,up;
static unsigned flip,batches,processed,components,different,cases,seed=0x186297ab;
static double maxDifference;
static fnQ3IHVParticleView_t viewContract;
static unsigned imageCalls, imageBytes, blendCalls, depthCalls, enableCalls, particleShaders;
static std::uint64_t digest=14695981039346656037ull;
static unsigned &Global(unsigned address){return *reinterpret_cast<unsigned*>(address);}
static unsigned Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void Hash(const void *input,std::size_t bytes){const auto *p=static_cast<const unsigned char*>(input);for(std::size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
static void __cdecl Begin(unsigned shader,unsigned fog,int flags){
 Check(shader==7&&fog==9&&flags==0,"Particle material callback");Global(0x4382f34)=Global(0x4382f2c)=0;++batches;
}
static void __cdecl Flush(){
 const unsigned count=Global(0x4382f34);Check(count&&count<=1023&&count%3==0&&Global(0x4382f2c)==count,"Particle batch bounds");
 for(unsigned p=0;p<count/3;++p){
  const auto &point=particles.at(processed++);
  float expectedVertices[3][3];Check(Q3IHV_ParticleTriangle(&viewContract,point.xyz,expectedVertices),"Particle triangle");
  for(unsigned v=0;v<3;++v){const auto index=p*3+v;Check(reinterpret_cast<const unsigned*>(0x437cf2c)[index]==index,"Particle index order");
   Check(colors[index]==point.rgba,"Particle RGBA");Check(uv[index][0]==(v==1?1.125f:0)&&uv[index][1]==(v==2?1.125f:0),"Particle UV");
   for(unsigned axis=0;axis<3;++axis){
    const float expected=expectedVertices[v][axis];
    const float actual=vertices[index][axis];if(std::memcmp(&actual,&expected,4)){++different;maxDifference=(std::max)(maxDifference,std::abs(double(actual)-expected));}
    ++components;Hash(&actual,4);
   }
   Hash(uv[index].data(),8);Hash(&colors[index],4);
  }
 }
}
static void Hook(unsigned address,const void *target){DWORD old{},ignored{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Writable hook");auto *p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned delta=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&delta,4);Check(VirtualProtect(p,5,PAGE_EXECUTE_READ,&ignored)&&FlushInstructionCache(GetCurrentProcess(),p,5),"Executable hook");}
static char * __cdecl Format(const char *format,...){static char text[128];va_list args;va_start(args,format);std::vsnprintf(text,sizeof text,format,args);va_end(args);return text;}
static unsigned __cdecl CreateImage(const char *name,const unsigned char *pixels,int width,int height,int mipmap){
 ++imageCalls;
 if(!std::strcmp(name,"*particle")){
  Check(width==16&&height==16&&!mipmap,"Particle image metadata");
  unsigned char expected[1024];Q3IHV_ParticleImage(expected);
  Check(!std::memcmp(pixels,expected,sizeof expected),"Particle image bytes");imageBytes=sizeof expected;
 }
 return 7;
}
static void __cdecl FinishShader(const unsigned char *shader){
 if(std::strcmp(reinterpret_cast<const char*>(shader),"<particle>"))return;
 auto word=[&](unsigned offset){unsigned result;std::memcpy(&result,shader+offset,4);return result;};
 Check(word(0x7c)==6&&word(0x8c)==1&&word(0x90)==7&&word(0x2f8)==0x62&&word(0x2dc)==3&&word(0x2f4)==3,"Particle shader contract");++particleShaders;
}
static void __stdcall Enable(unsigned capability){Check(capability==0xbe2,"Enable blending only");++enableCalls;}
static void __stdcall Blend(unsigned source,unsigned destination){Check(source==1&&destination==0x303,"Particle blend factors");++blendCalls;}
static void __stdcall DepthMask(unsigned char mask){Check(!mask,"Particle depth writes disabled");++depthCalls;}
int main(int argc,char **argv){
 if(argc!=2)return 77;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try{
  LoadReference(argv[1]);Hook(0x43ed60,&Begin);Hook(0x43eec0,&Flush);unsigned previous{};_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC);
  Hook(0x4381e0,&CreateImage);Hook(0x414fe0,&Format);reinterpret_cast<void(__cdecl*)()>(0x439650)();Check(imageBytes==1024,"Particle image callback");
  Hook(0x43beb0,&FinishShader);reinterpret_cast<void(__cdecl*)()>(0x43c570)();Check(particleShaders==1,"Particle shader created");
  Global(0x43d6508)=reinterpret_cast<unsigned>(&Enable);Global(0x43d67b0)=reinterpret_cast<unsigned>(&Blend);Global(0x43d69f4)=reinterpret_cast<unsigned>(&DepthMask);
  Global(0x4f0037c)=0x100;reinterpret_cast<void(__cdecl*)(unsigned)>(0x42aac0)(0x62);
  Check(enableCalls==1&&blendCalls==1&&depthCalls==1,"Particle GL state callbacks");
  std::array<unsigned char,196> world{};const unsigned fog=9;std::memcpy(world.data()+184,&fog,4);Global(0x43d6d20)=reinterpret_cast<unsigned>(world.data());Global(0x4eced50)=7;
  Global(0x4382f38)=reinterpret_cast<unsigned>(vertices.data());Global(0x4382f40)=reinterpret_cast<unsigned>(uv.data());Global(0x4382f4c)=reinterpret_cast<unsigned>(colors.data());
  const auto draw=reinterpret_cast<void(__cdecl*)(int,const Particle*)>(0x4428a0);
  for(unsigned view=0;view<8;++view){
   for(unsigned axis=0;axis<3;++axis){origin[axis]=float(int(Random()%4096)-2048)*.25f;forward[axis]=float(int(Random()%513)-256)/256;right[axis]=float(int(Random()%513)-256)/256;up[axis]=float(int(Random()%513)-256)/256;}
   if(!view){origin={0,0,0};forward={1,0,0};right={0,1,0};up={0,0,1};}
   std::memcpy(reinterpret_cast<void*>(0x4f0016c),origin.data(),12);std::memcpy(reinterpret_cast<void*>(0x4f00178),forward.data(),12);std::memcpy(reinterpret_cast<void*>(0x4f00184),right.data(),12);std::memcpy(reinterpret_cast<void*>(0x4f00190),up.data(),12);
   for(flip=0;flip<2;++flip){Global(0x4f002bc)=flip;float axes[3][3];std::memcpy(axes[0],forward.data(),12);std::memcpy(axes[1],right.data(),12);std::memcpy(axes[2],up.data(),12);Check(Q3IHV_ParticleView(origin.data(),axes,flip,&viewContract),"Particle view");
    for(unsigned count:{0u,1u,2u,340u,341u,342u,682u,2048u,4096u}){
     particles.resize(count);for(auto &point:particles){for(float &x:point.xyz)x=float(int(Random()%16384)-8192)*.25f;point.rgba=Random();}
     if(!view&&count>=4){particles[0].xyz[0]=19.999f;particles[1].xyz[0]=20;particles[2].xyz[0]=20.001f;particles[3].xyz[0]=-20;}
     processed=0;draw(static_cast<int>(count),particles.data());Check(processed==count,"Complete particle batches");++cases;
    }
   }
  }
  Check(!different,"Particle vertex mismatch");
  std::printf("{\"particle_shaders\":%u,\"image_calls\":%u,\"image_bytes\":%u,\"blend_src\":1,\"blend_dst\":771,\"depth_write\":false,\"cases\":%u,\"batches\":%u,\"float_components\":%u,\"different_float_bits\":%u,\"maximum_absolute_difference\":%.17g,\"fnv1a64\":\"%016llx\"}\n",particleShaders,imageCalls,imageBytes,cases,batches,components,different,maxDifference,static_cast<unsigned long long>(digest));return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

