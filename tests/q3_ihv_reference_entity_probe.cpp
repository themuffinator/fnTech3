/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Windows x86 observations against the user-supplied, hash-pinned IHV executable.
 * No original entry point, graphics, input, audio or network startup. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_render_abi.h"
#include "../code/renderercommon/tr_q3_ihv_beam.h"
#include <cstdint>
#include <cstdarg>
#include <cmath>
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
	for (auto range : {std::array<unsigned,2>{0x42b000,0x1000},{0x45b000,0x1000},{0x466000,0x1000},{0x442000,0x1000},
        {0x42c000,0x1000},{0x43d000,0x1000},{0x43e000,0x1000},{0x443000,0x1000},{0x413000,0x1000},
        {0x414000,0x1000},{0x437000,0x1000},{0x42a000,0x1000},
        {0x43c000,0x1000},{0x46b000,0x1000},{0x448000,0x1000}}) {
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

static unsigned &Global(unsigned address){return *reinterpret_cast<unsigned*>(address);}
static fntech3_q3_ihv_render_entity_t entity;
static unsigned transformed,drawn,errors,kind,total,visible,seed=0x4136289;
static float transformedOrigin[3],transformedAxis[3][3];
static std::uint64_t digest=14695981039346656037ull;
static void Hash(const void *input,std::size_t bytes){const auto*p=static_cast<const unsigned char*>(input);for(std::size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
static unsigned Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void __cdecl Transform(const float *origin,const float (*axis)[3]){++transformed;std::memcpy(transformedOrigin,origin,12);std::memcpy(transformedAxis,axis,36);}
static void Draw(unsigned expected,fntech3_q3_ihv_render_entity_t *input){Check(input==&entity,"Native entity pointer");++drawn;kind=expected;}
static void __cdecl Bad(fntech3_q3_ihv_render_entity_t *input){Draw(0,input);}
static void __cdecl Brush(fntech3_q3_ihv_render_entity_t *input){Draw(1,input);}
static void __cdecl Mesh(fntech3_q3_ihv_render_entity_t *input){Draw(2,input);}
static void __cdecl Sprite(fntech3_q3_ihv_render_entity_t *input){Draw(3,input);}
static void __cdecl Error(int level,const char *text){Check(level==1&&text,"Native model error");++errors;}
static unsigned lit,batched,flushed,surfaces,matrixLoads,enabledCull;
static std::vector<std::array<double,2>> depthRanges;
static void __cdecl Noop(){}
static void __cdecl Light(){++lit;}
static void __cdecl Begin(unsigned shader,unsigned,unsigned){Check(shader==0x4498d4c,"Backend shader");++batched;}
static void __cdecl Flush(){++flushed;}
static void __cdecl Surface(const void*){++surfaces;}
static void WINAPI Matrix(const float *matrix){Check(matrix==reinterpret_cast<float*>(matrixLoads?0x4f0026c:0x4f00448),"Backend matrix pointer");++matrixLoads;}
static void WINAPI Depth(double nearValue,double farValue){depthRanges.push_back({nearValue,farValue});}
static void WINAPI Enable(unsigned value){Check(value==0xb44,"Cull enable");++enabledCull;}
static std::array<float,3> injectedLight;
static unsigned lightTraces;
static void __cdecl Trace(unsigned,unsigned char *trace){++lightTraces;Check(!std::memcmp(trace,entity.origin,12),"Lighting trace origin");std::memcpy(trace+52,injectedLight.data(),12);}
static std::vector<std::array<float,3>> beamVertices;
static unsigned beamBinds,beamStates,beamColors,beamBegins,beamEnds;
static void __cdecl Bind(unsigned image){Check(image==9,"Beam white image");++beamBinds;}
static void __cdecl State(unsigned bits){Check(bits==0x22,"Beam GL state");++beamStates;}
static void WINAPI Color(float red,float green,float blue){Check(red==1&&green==0&&blue==0,"Beam color");++beamColors;}
static void WINAPI Primitive(unsigned primitive){Check(primitive==5,"Beam strip primitive");++beamBegins;}
static void WINAPI Vertex(const float *vertex){beamVertices.push_back({vertex[0],vertex[1],vertex[2]});}
static void WINAPI End(){++beamEnds;}
static unsigned queued,registrationWarnings,inlineQueries;
static void __cdecl Queue(unsigned surface,unsigned index,unsigned shader,unsigned fog,unsigned enabled,unsigned flags){Check(surface==0x466568&&index==3&&shader==0x4498d4c&&!fog&&enabled==1&&!flags,"Native beam queue contract");++queued;}
static int __cdecl InlineIndex(const char*){++inlineQueries;return 0;}
static void __cdecl Warning(int level,const char *text,...){Check(!level&&text,"Model warning");++registrationWarnings;}
static void Hook(unsigned address,const void *target){DWORD old{},ignored{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Writable hook");auto*p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned delta=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&delta,4);Check(VirtualProtect(p,5,PAGE_EXECUTE_READ,&ignored)&&FlushInstructionCache(GetCurrentProcess(),p,5),"Executable hook");}
int main(int argc,char **argv){
 if(argc!=2)return 77;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try{
  LoadReference(argv[1]);
  Hook(0x42c520,&Queue);Global(0x4eced50)=0x4498d4c;Global(0x4f00164)=3;
  reinterpret_cast<void(__cdecl*)()>(0x4425e0)();Check(queued==1,"Beam queue once");
  Hook(0x44813e,&InlineIndex);Global(0x43d6ae4)=reinterpret_cast<unsigned>(&Warning);
  const auto registerModel=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x43c810);
  Check(registerModel("*beam")==0x43d63e0&&registerModel("*beam")==0x43d63e0&&!inlineQueries&&!registrationWarnings,"Exact special beam name");
  for(const char *name:{"*BEAM","*Beam","*beam ","*beamx"})Check(!registerModel(name),"Nonmatching special name");
  Check(inlineQueries==4&&registrationWarnings==4,"Other star names use inline parsing");
  Check(!registerModel(nullptr)&&!registerModel("")&&registrationWarnings==6,"Empty model names");
  Hook(0x42b260,&Transform);Hook(0x4426d0,&Bad);Hook(0x441920,&Brush);Hook(0x441cf0,&Mesh);Hook(0x4425e0,&Sprite);Global(0x43d6ae0)=reinterpret_cast<unsigned>(&Error);
  std::array<unsigned char,28> cvar{};float enabled=1;std::memcpy(cvar.data()+24,&enabled,4);Global(0x4f03640)=reinterpret_cast<unsigned>(cvar.data());
  Global(0x43d6ab8)=1;Global(0x43d6abc)=reinterpret_cast<unsigned>(&entity);
  float current[3][3],primary[3][3];for(int a=0;a<3;++a)for(int b=0;b<3;++b){current[a][b]=float(int(Random()%256)-128)/8;primary[a][b]=float(int(Random()%256)-128)/8;}
  std::memcpy(reinterpret_cast<void*>(0x4f00178),current,36);std::memcpy(reinterpret_cast<void*>(0x2dce84c),primary,36);
  const auto submit=reinterpret_cast<void(__cdecl*)()>(0x42bac0);
  for(unsigned portal:{0u,1u})for(unsigned high:{0u,0x80000000u,0xffff0000u})for(unsigned flags=0;flags<4096;++flags)for(unsigned modelType=0;modelType<5;++modelType){
   Global(0x4f002b8)=portal;entity={};entity.flags=flags|high;for(float &x:entity.origin)x=float(int(Random()%8192)-4096);for(auto &row:entity.axis)for(float &x:row)x=float(int(Random()%1024)-512)/32;
   std::array<unsigned char,112> model{};std::memcpy(model.data()+64,&modelType,4);entity.model=modelType?reinterpret_cast<unsigned>(model.data()):0;
   const auto original=entity;transformed=drawn=errors=kind=0;submit();++total;
   const bool skip=!Q3IHV_EntityVisible(entity.flags,portal);
   if(skip){Check(!transformed&&!drawn&&!errors&&!std::memcmp(&entity,&original,sizeof entity),"Skipped entity unchanged");}
   else{
    ++visible;Check(transformed==1,"Entity transform callback");Check(!std::memcmp(transformedOrigin,original.origin,12),"Entity origin unchanged");
    const auto *axes=(flags&0x400)?(portal?primary:current):original.axis;
    Check(!std::memcmp(transformedAxis,axes,36)&&!std::memcmp(entity.axis,axes,36),"Camera-facing axis source");
    Check(modelType==4?errors==1&&!drawn:!errors&&drawn==1&&kind==modelType,"Native model dispatch");
    auto expected=original;std::memcpy(expected.axis,axes,36);Check(!std::memcmp(&entity,&expected,sizeof entity),"Only axes changed");
    Hash(transformedOrigin,12);Hash(transformedAxis,36);Hash(&kind,4);Hash(&errors,4);
   }
  }
  enabled=0;std::memcpy(cvar.data()+24,&enabled,4);entity.flags=0;transformed=drawn=errors=0;submit();Check(!transformed&&!drawn&&!errors,"Disabled entities");

  const auto frontendDigest=digest;digest=14695981039346656037ull;
  unsigned previous{};_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC);
  const auto lerp=reinterpret_cast<void(__cdecl*)(float,float*)>(0x4423a0);Global(0x4f00160)=reinterpret_cast<unsigned>(&entity);unsigned lerpCases=0;
  for(unsigned sample=0;sample<4096;++sample){
   for(float &x:entity.origin)x=float(int(Random()%32768)-16384)*.25f;
   for(float &x:entity.oldorigin)x=float(int(Random()%32768)-16384)*.25f;
   for(auto &row:entity.axis)for(float &x:row)x=float(int(Random()%513)-256)/64;
   std::memcpy(reinterpret_cast<void*>(0x4f003f4),entity.axis,36);
   for(unsigned flags:{0u,1u,0x40u,0x41u,0x80u,0x400u,0xffffffffu,0x80000040u})for(float fraction:{-2.0f,-.125f,0.0f,.25f,.5f,1.0f,2.0f}){
    entity.flags=flags;float actual[3]={71,71,71};lerp(fraction,actual);
    float modern[3];Check(Q3IHV_EntityOriginLerp(flags,entity.origin,entity.oldorigin,entity.axis,fraction,modern),"Modern origin interpolation");
    for(int axis=0;axis<3;++axis){Check(!std::memcmp(&actual[axis],&modern[axis],4),"Origin frame-lerp component");Hash(&actual[axis],4);}
    ++lerpCases;
   }
  }
  const auto lerpDigest=digest;digest=14695981039346656037ull;
  Hook(0x43ddf0,&Trace);std::array<unsigned,48> world{};Global(0x43d6d20)=reinterpret_cast<unsigned>(world.data());Global(0x43d6ac0)=0;
  const auto setupLighting=reinterpret_cast<void(__cdecl*)()>(0x43dba0);unsigned lightingCases=0;
  std::vector<unsigned> lightingFlags{0u,0xffffffffu,0xaabbccddu,0x80000001u};for(unsigned bit=0;bit<32;++bit)lightingFlags.push_back(1u<<bit);
  for(unsigned sample=0;sample<256;++sample){
   for(float &value:injectedLight)value=float(int(Random()%9217)-1024)/16;
   for(unsigned noWorld:{0u,1u})for(unsigned flags:lightingFlags){
    Global(0x43d6ab0)=noWorld;entity.flags=flags;lightTraces=0;setupLighting();Check(lightTraces==(noWorld?0u:1u),"Lighting world trace selection");
    std::array<float,3> expected=noWorld?std::array<float,3>{255,255,255}:injectedLight;
    if(flags)for(float &value:expected)value=static_cast<float>(double(value)+32);
    float maximum=expected[0];for(float value:expected)if(value>maximum)maximum=value;
    if(maximum>255)for(float &value:expected){value=static_cast<float>(double(value)/maximum);value=static_cast<float>(double(value)*255);}
    Check(!std::memcmp(reinterpret_cast<void*>(0x4f00430),expected.data(),12)&&!std::memcmp(reinterpret_cast<void*>(0x4f0043c),expected.data(),12),"Native light color normalization");
    Hash(reinterpret_cast<void*>(0x4f00430),24);++lightingCases;
   }
  }
  const auto lightingDigest=digest;digest=14695981039346656037ull;
  Hook(0x42c660,&Noop);Hook(0x42c330,&Noop);Hook(0x42b930,&Noop);Hook(0x43dba0,&Light);Hook(0x43ed60,&Begin);Hook(0x43eec0,&Flush);Hook(0x443010,&Surface);
  Global(0x43d6594)=reinterpret_cast<unsigned>(&Matrix);Global(0x43d6584)=reinterpret_cast<unsigned>(&Depth);Global(0x43d6508)=reinterpret_cast<unsigned>(&Enable);
  unsigned fog=0;world[46]=reinterpret_cast<unsigned>(&fog);Global(0x4498d4c+0x7c)=2;
  std::array<unsigned,2> drawSurface{0,0};const auto backend=reinterpret_cast<void(__cdecl*)(void*,int)>(0x42c700);unsigned backendCases=0;
  for(unsigned high:{0u,0x80000000u,0xffff0000u})for(unsigned flags=0;flags<4096;++flags){
   entity.flags=flags|high;entity.model=0;transformed=lit=batched=flushed=surfaces=matrixLoads=enabledCull=0;depthRanges.clear();backend(drawSurface.data(),1);
   Check(transformed==((flags&0x80)?0u:1u)&&lit==transformed,"Backend transform/lighting suppression");
   Check(batched==1&&flushed==1&&surfaces==1&&matrixLoads==2&&enabledCull==1,"Backend callback counts");
   Check(depthRanges.size()==((flags&0x10)?2u:0u),"Depth-hack callback count");
   if(flags&0x10)Check(depthRanges[0][0]==0&&depthRanges[0][1]==.3&&depthRanges[1][0]==0&&depthRanges[1][1]==1,"Depth-hack range and restoration");
   Hash(&transformed,4);Hash(&lit,4);for(const auto &range:depthRanges)Hash(range.data(),16);++backendCases;
  }
  const auto backendDigest=digest;digest=14695981039346656037ull;
  Hook(0x437c90,&Bind);Hook(0x42aac0,&State);Global(0x43d6530)=reinterpret_cast<unsigned>(&Color);Global(0x43d698c)=reinterpret_cast<unsigned>(&Primitive);Global(0x43d671c)=reinterpret_cast<unsigned>(&Vertex);Global(0x43d6560)=reinterpret_cast<unsigned>(&End);
  std::array<unsigned,21> white{};white[20]=9;Global(0x4498d44)=reinterpret_cast<unsigned>(white.data());Global(0x4f00160)=reinterpret_cast<unsigned>(&entity);
  const auto beam=reinterpret_cast<void(__cdecl*)()>(0x442450);unsigned beamCases=0,beamFloats=0,beamDifferent=0;double beamMaximum=0;
  for(unsigned sample=0;sample<4096;++sample){
   for(float &value:entity.origin)value=float(int(Random()%32768)-16384)/4;
   for(float &value:entity.oldorigin)value=float(int(Random()%32768)-16384)/4;
   if(sample%32==0)std::memcpy(entity.oldorigin,entity.origin,12);
   entity.flags=Random();for(auto &color:entity.rgba)color=static_cast<unsigned char>(Random());
   beamVertices.clear();beamBinds=beamStates=beamColors=beamBegins=beamEnds=0;beam();++beamCases;
   float modernBeam[12][3];const bool beamResult=!!Q3IHV_BeamVertices(entity.origin,entity.oldorigin,modernBeam);
   if(sample%32==0){Check(!beamResult,"Modern zero-length beam");Check(beamVertices.empty()&&!beamBinds&&!beamStates&&!beamColors&&!beamBegins&&!beamEnds,"Zero-length beam");continue;}
   Check(beamResult,"Modern nonzero beam");
   Check(beamVertices.size()==14&&beamBinds==1&&beamStates==1&&beamColors==1&&beamBegins==1&&beamEnds==1,"Beam GL callback counts");
   Check(!std::memcmp(beamVertices[0].data(),beamVertices[12].data(),24),"Beam strip closure");
   double direction[3];for(int axis=0;axis<3;++axis)direction[axis]=double(entity.oldorigin[axis])-entity.origin[axis];
   for(unsigned point=0;point<6;++point){
    const auto &start=beamVertices[point*2],&finish=beamVertices[point*2+1];double radius=0,dot=0;
    for(int axis=0;axis<3;++axis){radius+=double(start[axis])*start[axis];dot+=direction[axis]*start[axis];const float expected=static_cast<float>(double(start[axis])+direction[axis]);Check(!std::memcmp(&finish[axis],&expected,4),"Beam extrusion");}
    Check(std::fabs(radius-16)<.0001&&std::fabs(dot)<.02,"Beam radius and perpendicular ring");
   }
   for(unsigned point=0;point<14;++point)for(unsigned axis=0;axis<3;++axis){const float a=beamVertices[point][axis],b=modernBeam[point%12][axis];if(std::memcmp(&a,&b,4))++beamDifferent;const double difference=std::fabs(double(a)-b);if(difference>beamMaximum)beamMaximum=difference;}
   Hash(beamVertices.data(),beamVertices.size()*12);beamFloats+=static_cast<unsigned>(beamVertices.size()*3);
  }
  Check(!beamDifferent && beamMaximum==0,"Every beam component matches bit for bit");
  std::printf("{\"beam_queue_calls\":1,\"beam_registration_cases\":8,\"beam_different_float_bits\":0,\"cases\":%u,\"visible\":%u,\"fnv1a64\":\"%016llx\",\"origin_lerp_cases\":%u,\"origin_lerp_digest\":\"%016llx\",\"lighting_cases\":%u,\"lighting_digest\":\"%016llx\",\"backend_cases\":%u,\"backend_digest\":\"%016llx\",\"beam_cases\":%u,\"beam_float_components\":%u,\"beam_digest\":\"%016llx\"}\n",total,visible,static_cast<unsigned long long>(frontendDigest),lerpCases,static_cast<unsigned long long>(lerpDigest),lightingCases,static_cast<unsigned long long>(lightingDigest),backendCases,static_cast<unsigned long long>(backendDigest),beamCases,beamFloats,static_cast<unsigned long long>(digest));return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

