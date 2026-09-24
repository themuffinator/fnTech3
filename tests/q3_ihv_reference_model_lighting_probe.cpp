/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Windows x86 observations against the user-supplied, hash-pinned IHV executable.
 * No original entry point, graphics, input, audio or network startup. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "code/compat/q3_ihv_render_abi.h"
#include "code/renderercommon/tr_q3_ihv_beam.h"
#include "code/renderercommon/tr_q3_ihv_model.h"
#include "code/renderercommon/tr_q3_ihv_model_light.h"
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
#include <filesystem>
#include <algorithm>
#include <memory>
#include <limits>
#include "tests/q3_ihv_model_light_bsp_fixture.hpp"
#include "tests/q3_ihv_model_light_cases.hpp"

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
        {0x43c000,0x1000},{0x46b000,0x1000},{0x448000,0x1000},{0x445000,0x1000},{0x44a000,0x1000},{0x452000,0x1000},{0x46c000,0x1000},{0x440000,0x1000},{0x441000,0x1000},{0x446000,0x1000},{0x444000,0x1000}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		DWORD writable{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_READWRITE,&writable),"Reference reload protection");
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

struct CullFrame {float bounds[2][3],localOrigin[3],radius;char name[16];};
struct CullModel {unsigned char header[108];CullFrame frames[2];};
static CullModel cullModel;
static unsigned sphereCalls,boxCalls,sphereReplies[2],boxReply;
static float observedBounds[2][3];
static int __cdecl SphereCull(const float *origin,float radius){Check(sphereCalls<2,"Sphere call bound");const auto &f=cullModel.frames[sphereCalls?entity.oldframe:entity.frame];Check(!std::memcmp(origin,f.localOrigin,12)&&radius==f.radius,"Sphere frame and radius");return static_cast<int>(sphereReplies[sphereCalls++]);}
static int __cdecl BoxCull(const float (*bounds)[3]){++boxCalls;std::memcpy(observedBounds,bounds,24);return static_cast<int>(boxReply);}


__declspec(naked) static void BuildTrigTables(){__asm sub esp,10h
 __asm push edi
 __asm mov eax,42a685h
 __asm jmp eax
}
__declspec(naked) static void EndTrigTables(){__asm pop edi
 __asm add esp,10h
 __asm ret
}

static void Hook(unsigned address,const void *target){DWORD old{},ignored{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Writable hook");auto*p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned delta=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&delta,4);Check(VirtualProtect(p,5,PAGE_EXECUTE_READ,&ignored)&&FlushInstructionCache(GetCurrentProcess(),p,5),"Executable hook");}
static std::vector<std::unique_ptr<unsigned char[]>> mapAllocations;
static std::array<unsigned char,0x2900> mapShader;
static void *__cdecl MapAllocate(unsigned bytes){Check(bytes>0&&bytes<16*1024*1024,"Map allocation bound");auto p=std::make_unique<unsigned char[]>(bytes);void *result=p.get();mapAllocations.push_back(std::move(p));return result;}
static void *__cdecl MapShader(const char*,int){return mapShader.data();}
static void __cdecl MapError(int,const char*){Check(false,"Native map metadata error");}
static const char *__cdecl MapImageName(const char*,...){return "probe-lightmap";}
static unsigned __cdecl MapImage(const char *name,const unsigned char*,int w,int h,int){Check(!strcmp(name,"probe-lightmap")&&w==128&&h==128,"Native lightmap image arguments");return 1;}

__declspec(naked) static void NativeLightDirection(){__asm sub esp,40h
 __asm mov eax,42b452h
 __asm jmp eax
}
__declspec(naked) static int NativeLightInteger(double){__asm fld qword ptr [esp+4]
 __asm mov eax,448050h
 __asm call eax
 __asm ret
}
static void DiffuseLightProbe(){
 float axis[3][3],direction[3],ambient[3],directed[3],normals[17][4];unsigned char colors[17][4];
 Global(0x4382f34)=17;Global(0x4382f3c)=reinterpret_cast<unsigned>(normals);
 const auto shade=reinterpret_cast<void(__cdecl*)(void*)>(0x444d40);
 unsigned cases=0;seed=0x91874362;digest=14695981039346656037ull;
 for(unsigned sample=0;sample<2048;++sample){
  for(auto &row:axis)for(float &value:row)value=float(int(Random()%8192)-4096)/1024;
  for(int a=0;a<3;++a){ambient[a]=float(int(Random()%16384)-8192)/8;directed[a]=float(int(Random()%16384)-8192)/8;}
  for(auto &row:normals)for(float &value:row)value=float(int(Random()%8192)-4096)/1024;
  memset(normals[0],0,12);
  memcpy(reinterpret_cast<void*>(0x4f003f4),axis,36);NativeLightDirection();Q3IHV_LightDirection(axis,direction);
  Check(!memcmp(direction,reinterpret_cast<void*>(0x4f00424),12),"Native fixed direction transform");
  memcpy(reinterpret_cast<void*>(0x4f00430),ambient,12);memcpy(reinterpret_cast<void*>(0x4f0043c),directed,12);shade(colors);
  for(unsigned v=0;v<17;++v){unsigned char candidate[4];Q3IHV_LightDiffuse(normals[v],direction,ambient,directed,candidate);Check(!memcmp(colors[v],candidate,4),"Native model diffuse color");++cases;}
  Hash(direction,12);Hash(colors,sizeof colors);
 }
 for(double value:{0.0,-0.0,1.0,-1.0,255.999,-255.999,2147483648.0,4294967295.0,4294967296.0,9223372036854775808.0,-9223372036854775808.0,1e30,-1e30,std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})Check(NativeLightInteger(value)==Q3IHV_LightInteger(&value),"Native low-word integer conversion");
 printf("model_diffuse_cases %u directions 2048 fnv1a64 %016llx\n",cases,static_cast<unsigned long long>(digest));
}

static void GeneratedLightTrees(){
 const auto sample=reinterpret_cast<void(__cdecl*)(const void*,void*)>(0x43ddf0);
 const uint64_t result=LightCases::Trees([&](const LightCases::Tree &tree,q3IHVModelLightTrace_t &trace){
  std::array<unsigned,17> nodes[63]{};std::array<unsigned,28> faces[64]{};std::array<unsigned,8> records[64]{};unsigned marks[32][12];
  for(int i=0;i<64;++i){const auto &face=tree.faces[i];faces[i][0]=1;memcpy(faces[i].data()+1,face.plane,16);faces[i][8]=face.width;faces[i][9]=face.height;faces[i][12]=reinterpret_cast<unsigned>(face.pixels);memcpy(faces[i].data()+13,face.origin,12);memcpy(faces[i].data()+22,face.reciprocal,24);records[i][7]=reinterpret_cast<unsigned>(faces[i].data());}
  for(int i=0;i<63;++i){const auto &node=tree.nodes[i];if(i<31){nodes[i][0]=0xffffffffu;nodes[i][10]=reinterpret_cast<unsigned>(node.plane);for(int a=0;a<2;++a)nodes[i][11+a]=reinterpret_cast<unsigned>(nodes[node.children[a]-tree.nodes].data());}
   else{nodes[i][15]=reinterpret_cast<unsigned>(marks[i-31]);nodes[i][16]=12;for(int a=0;a<12;++a)marks[i-31][a]=reinterpret_cast<unsigned>(records[tree.marks[i-31][a]-tree.faces].data());}}
  float native[24]{};memcpy(native,trace.start,12);memcpy(native+3,trace.end,12);memcpy(native+6,trace.origin,12);memcpy(native+9,trace.direction,12);native[12]=trace.distance;memcpy(native+13,trace.color,12);
  sample(nodes,native);Check(Q3IHV_LightTrace(tree.nodes,&trace)!=0,"Generated engine traversal");
  Check(!memcmp(native,trace.start,12)&&!memcmp(native+3,trace.end,12)&&!memcmp(native+12,&trace.distance,4)&&!memcmp(native+13,trace.color,12)&&!memcmp(native+21,trace.hit,12),"Generated native tree sample");
 });
 printf("generated_light_trees 32 cases 32768 fnv1a64 %016llx\n",static_cast<unsigned long long>(result));
}

static void ModelLightMapMetadata(const char *directory){
 const auto parse=reinterpret_cast<void(__cdecl*)(const void*,const void*,void*)>(0x440700);
 reinterpret_cast<void(__cdecl*)()>(0x4149c0)(); // original endian dispatch initialization
 std::array<unsigned,12> cvar{};
 for(unsigned address:{0x4f03664u,0x4f03660u,0x4f03524u,0x4f035b8u,0x4f034ccu,0x4f03518u})Global(address)=reinterpret_cast<unsigned>(cvar.data());
 Global(0x4effd5c)=1;*reinterpret_cast<unsigned*>(mapShader.data()+0x2850)=1;
 Hook(0x440120,&MapAllocate);Hook(0x439fd0,&MapShader);Global(0x43d6ae0)=reinterpret_cast<unsigned>(&MapError);
 Hook(0x414fe0,&MapImageName);Hook(0x4381e0,&MapImage);
 std::vector<std::filesystem::path> files;for(const auto &entry:std::filesystem::directory_iterator(directory))if(entry.path().extension()==".bsp")files.push_back(entry.path());std::sort(files.begin(),files.end());
 unsigned maps=0,faces=0,sampledFaces=0,normalDifferences=0,traceCases=0,traceHits=0;digest=14695981039346656037ull;
 uint64_t metadataDigest=digest,traceDigest=digest;seed=0x23149287;
 for(const auto &file:files){
  std::ifstream input(file,std::ios::binary);std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  auto word=[&](size_t offset){Check(offset<=data.size()&&data.size()-offset>=4,"BSP metadata word bounds");uint32_t result;std::memcpy(&result,data.data()+offset,4);return result;};
  Check(data.size()>=120&&word(0)==0x50534249&&word(4)==43,"Native BSP header");
  // Directory indices come from the existing separately verified BSP43 format.
  auto lump=[&](unsigned index){const unsigned offset=word(8+index*8),length=word(12+index*8);Check(offset<=data.size()&&length<=data.size()-offset,"BSP metadata lump extent");return std::array<unsigned,2>{offset,length};};
  const auto surfaceLump=lump(12),vertices=lump(11),lightmaps=lump(9);
  Check(surfaceLump[1]%156==0&&vertices[1]%44==0&&lightmaps[1]%(128*128*3)==0,"BSP metadata record sizes");
  memset(reinterpret_cast<void*>(0x31e0518),0,0xd0);Global(0x437cef8)=reinterpret_cast<unsigned>(data.data());
  reinterpret_cast<void(__cdecl*)(void*)>(0x440180)(data.data()+8+9*8);
  Check(!memcmp(reinterpret_cast<void*>(Global(0x31e05d4)),data.data()+lightmaps[0],lightmaps[1]),"Native model lightmap copy is raw RGB");
  std::vector<std::array<unsigned,8>> records(surfaceLump[1]/156);
  std::vector<q3IHVModelLightFace_t> candidateFaces(records.size());
  unsigned skippedPatchType=2;
  digest=metadataDigest;
  for(unsigned i=0;i<surfaceLump[1]/156;++i){
   auto &record=records[i];
   const size_t offset=surfaceLump[0]+i*156;if(word(offset+80)){record[7]=reinterpret_cast<unsigned>(&skippedPatchType);continue;}
   const unsigned first=word(offset+72),count=word(offset+76);Check(count>=3&&count<=64&&first<=vertices[1]/44&&count<=vertices[1]/44-first,"Native face vertices");
   parse(data.data()+offset,data.data()+vertices[0],record.data());
   const auto *face=reinterpret_cast<const unsigned char*>(record[7]);Check(face&&*reinterpret_cast<const unsigned*>(face)==1,"Original face allocation");
   const auto *firstVertex=data.data()+vertices[0]+first*44;
   Check(!std::memcmp(face+4,firstVertex+28,12),"Native face normal comes from first vertex");
   if(std::memcmp(firstVertex+28,data.data()+offset+144,12))++normalDifferences;
   const auto lightmap=word(offset+88);Hash(face+4,16);
   if(lightmap!=0xffffffffu){
    Check(lightmap<lightmaps[1]/(128*128*3),"Native lightmap index");
    const unsigned x=word(offset+92),y=word(offset+96),width=word(offset+100),height=word(offset+104);
    Check(x+width<=128&&y+height<=128,"Native lightmap rectangle");
    Check(*reinterpret_cast<const unsigned*>(face+32)==width&&*reinterpret_cast<const unsigned*>(face+36)==height,"Native lightmap dimensions");
    const auto *expectedPixels=reinterpret_cast<const char*>(Global(0x31e05d4))+(lightmap*128*128+y*128+x)*3;
    Check(*reinterpret_cast<const unsigned*>(face+48)==reinterpret_cast<unsigned>(expectedPixels),"Native RGB subrectangle pointer");
    Check(!std::memcmp(face+52,data.data()+offset+108,12),"Native lightmap origin");
    for(int axis=0;axis<2;++axis){
     float vector[3],normalized[3],inverseVector[3];std::memcpy(vector,data.data()+offset+120+axis*12,12);
     volatile double x2=double(vector[0])*vector[0],y2=double(vector[1])*vector[1],z2=double(vector[2])*vector[2];volatile double length=x2+y2;length=length+z2;length=std::sqrt(length);
     Check(length>0,"Native nonzero lightmap axis");volatile double inverse=1.0/length;
     for(int a=0;a<3;++a){normalized[a]=float(double(vector[a])*inverse);inverseVector[a]=float(double(normalized[a])*inverse);}
     Check(!std::memcmp(inverseVector,face+88+axis*12,12),"Native twice-normalized reciprocal texture vector");
     float candidate[3];Check(Q3IHV_LightReciprocal(vector,candidate)&&!memcmp(candidate,inverseVector,12),"Engine reciprocal vector");
    }
    Hash(face+32,16);Hash(face+52,60);++sampledFaces;
   }
   auto &candidate=candidateFaces[i];memcpy(candidate.plane,face+4,16);memcpy(candidate.origin,face+52,12);memcpy(candidate.reciprocal,face+88,24);
   memcpy(&candidate.width,face+32,4);memcpy(&candidate.height,face+36,4);candidate.pixels=reinterpret_cast<const unsigned char*>(*reinterpret_cast<const unsigned*>(face+48));
   ++faces;
  }
  metadataDigest=digest;
  Global(0x31e05c0)=reinterpret_cast<unsigned>(records.data());Global(0x31e05bc)=static_cast<unsigned>(records.size());
  reinterpret_cast<void(__cdecl*)(void*)>(0x441140)(data.data()+8+1*8);
  reinterpret_cast<void(__cdecl*)(void*)>(0x4410c0)(data.data()+8+4*8);
  reinterpret_cast<void(__cdecl*)(void*,void*)>(0x440e30)(data.data()+8+2*8,data.data()+8+3*8);
  const unsigned root=Global(0x31e05b8),count=Global(0x31e05b0),internal=Global(0x31e05b4);
  std::vector<q3IHVModelLightNode_t> candidateNodes(count);
  std::vector<const q3IHVModelLightFace_t*> marks;
  const auto marksLump=lump(4);for(unsigned offset=0;offset<marksLump[1];offset+=4){unsigned index=word(marksLump[0]+offset);Check(index<candidateFaces.size(),"Light marksurface bound");marks.push_back(&candidateFaces[index]);}
  for(unsigned i=0;i<count;++i){
   const auto *node=reinterpret_cast<const unsigned*>(root+i*68);auto &candidate=candidateNodes[i];
   if(i<internal){candidate.numFaces=-1;memcpy(candidate.plane,reinterpret_cast<void*>(node[10]),16);for(int a=0;a<2;++a){unsigned child=(node[11+a]-root)/68;Check(child<count,"Native child index");candidate.children[a]=&candidateNodes[child];}}
   else{candidate.numFaces=static_cast<int>(node[16]);unsigned first=(node[15]-Global(0x31e05c8))/4;Check(first<=marks.size()&&node[16]<=marks.size()-first,"Native leaf marks");candidate.faces=marks.data()+first;}
  }
  const std::vector<unsigned char> rendererBytes(data.begin(),data.end());
  const auto *rendererRoot=LightWorldFixture::Build(rendererBytes);
  digest=traceDigest;
  const auto traceNative=reinterpret_cast<void(__cdecl*)(unsigned,float*)>(0x43ddf0);
  for(unsigned sample=0;sample<8192;++sample){
   const auto *bounds=reinterpret_cast<const float*>(root+8);float origin[3];
   for(int a=0;a<3;++a){const double low=bounds[a]-128.0,high=bounds[a+3]+128.0;origin[a]=float(low+(high-low)*double(Random()%65536)/65535.0);}
   float native[24]{};memcpy(native,origin,12);memcpy(native+3,origin,12);memcpy(native+6,origin,12);native[5]=float(double(origin[2])-2048);native[11]=-1;native[12]=2048;native[13]=native[14]=native[15]=255;
   traceNative(root,native);q3IHVModelLightTrace_t candidate;Q3IHV_LightTraceInit(origin,&candidate);Check(Q3IHV_LightTrace(candidateNodes.data(),&candidate)!=0,"Engine BSP light traversal");
   Check(!memcmp(native,candidate.start,12)&&!memcmp(native+3,candidate.end,12),"Native light traversal segment");
   Check(!memcmp(native+12,&candidate.distance,4)&&!memcmp(native+13,candidate.color,12)&&!memcmp(native+21,candidate.hit,12),"Native map model light sample");
   q3IHVModelLightTrace_t rendered;Q3IHV_LightTraceInit(origin,&rendered);Check(Q3IHV_LightTrace(rendererRoot,&rendered)!=0&&!memcmp(&candidate,&rendered,sizeof candidate),"Actual renderer BSP loader light result");
   Hash(native,sizeof native);++traceCases;if(candidate.sampled)++traceHits;
  }
  traceDigest=digest;mapAllocations.clear();
  ++maps;
 }
 std::printf("map_light_metadata_maps %u faces %u sampled_faces %u first_normal_differences %u fnv1a64 %016llx\n",maps,faces,sampledFaces,normalDifferences,static_cast<unsigned long long>(metadataDigest));
 std::printf("map_light_trace_cases %u hits %u fnv1a64 %016llx\n",traceCases,traceHits,static_cast<unsigned long long>(traceDigest));
}

static void SurfaceLightProbe(){
 std::array<uint32_t,28> surface{};surface[0]=1;surface[3]=0x3f800000;surface[8]=surface[9]=4;
 const float step=1.0f/16;std::memcpy(&surface[22],&step,4);std::memcpy(&surface[26],&step,4);
 std::array<unsigned char,128*4*3> pixels{};for(size_t i=0;i<pixels.size();++i)pixels[i]=static_cast<unsigned char>((i*13+17)%256);
 surface[12]=reinterpret_cast<unsigned>(pixels.data());
 const auto sample=reinterpret_cast<void(__cdecl*)(const void*,void*)>(0x43d990);
 unsigned cases=0,hits=0;digest=14695981039346656037ull;
 for(float x:{-.5f,0.0f,7.999f,15.999f,16.0f,63.99f,64.0f,65.0f})for(float y:{-.5f,0.0f,7.999f,15.999f,16.0f,63.99f,64.0f,65.0f})for(float z:{-1.0f,0.0f,16.0f,2048.0f,4096.0f})for(float limit:{0.0f,32.0f,2048.0f}){
  float trace[24]{};trace[0]=trace[3]=trace[6]=x;trace[1]=trace[4]=trace[7]=y;trace[2]=trace[8]=z;trace[5]=z-2048;trace[11]=-1;trace[12]=limit;trace[13]=trace[14]=trace[15]=255;
  float before[24];std::memcpy(before,trace,sizeof trace);sample(surface.data(),trace);++cases;
  const int tx=static_cast<int>(std::floor(x/16)),ty=static_cast<int>(std::floor(y/16));const bool hit=tx>=0&&tx<4&&ty>=0&&ty<4&&z>=0&&z<=limit;
  if(!hit)Check(!std::memcmp(trace,before,sizeof trace),"Outside model light sample unchanged");
  else{++hits;Check(trace[12]==z&&!std::memcmp(trace+16,surface.data()+1,20)&&trace[21]==x&&trace[22]==y&&trace[23]==0,"Native light sample hit");for(int a=0;a<3;++a)Check(trace[13+a]==pixels[(ty*128+tx)*3+a],"Native light texel and 128 stride");}
  Hash(trace,sizeof trace);
 }
 std::printf("surface_light_cases %u hits %u fnv1a64 %016llx\n",cases,hits,static_cast<unsigned long long>(digest));
}

static unsigned meshChecks,meshCalls;
static uint32_t observedFraction;
static float observedMove[3];
static void __cdecl MeshCapacity(int vertices,int triangles){Check(vertices==3&&triangles==1,"Native capacity args");++meshChecks;}
static void __cdecl MeshMath(const void*,float fraction,const float *move){++meshCalls;std::memcpy(&observedFraction,&fraction,4);std::memcpy(observedMove,move,12);}
static void AnimationControl(){
 std::array<uint32_t,36> surface{};surface[20]=3;surface[21]=1;surface[22]=108;surface[24]=120;surface[27]=0;surface[28]=1;surface[29]=2;
 for(unsigned i=30;i<36;++i)surface[i]=0x3d812345+i;
 std::array<uint32_t,10> cvar{};float uv[5][2]{};
 Global(0x4f035b4)=reinterpret_cast<unsigned>(cvar.data());Global(0x4382f40)=reinterpret_cast<unsigned>(uv);
 Global(0x4f00160)=reinterpret_cast<unsigned>(&entity);Global(0x43b904c)=reinterpret_cast<unsigned>(&MeshMath);Hook(0x442c40,&MeshCapacity);
 const auto submit=reinterpret_cast<void(__cdecl*)(const void*)>(0x442280);
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)entity.axis[i][j]=i==j?1.0f:0;
 std::memcpy(reinterpret_cast<void*>(0x4f003f4),entity.axis,36);
 unsigned cases=0;digest=14695981039346656037ull;seed=0x83173621;
 for(unsigned sample=0;sample<256;++sample){
  for(int a=0;a<3;++a){entity.origin[a]=float(int(Random()%8192)-4096)/4;entity.oldorigin[a]=float(int(Random()%8192)-4096)/4;}
  for(uint32_t bits:{0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00123u,0xffc00123u})for(unsigned oldFrame:{0u,1u})for(unsigned flags:{0u,0x40u})for(float fraction:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
   cvar[6]=bits;entity.frame=1;entity.oldframe=oldFrame;entity.flags=flags;entity.backlerp=fraction;
   Global(0x4382f34)=2;Global(0x4382f2c)=3;meshChecks=meshCalls=0;submit(surface.data());
   const bool enabled=(bits&0x7fffffffu)!=0&&(bits&0x7fffffffu)<=0x7f800000u;
   const float expected=enabled&&oldFrame!=1?fraction:0;float control;std::memcpy(&control,&bits,4);Check(Q3IHV_ModelFraction(1,oldFrame,fraction,&control)==expected,"Engine animation control helper");uint32_t expectedBits;std::memcpy(&expectedBits,&expected,4);
   Check(meshChecks==1&&meshCalls==1&&observedFraction==expectedBits,"Native lerpmodels control");
   float move[3];Check(Q3IHV_EntityOriginLerp(flags,entity.origin,entity.oldorigin,entity.axis,expected,move)&&!std::memcmp(move,observedMove,12),"Control origin move");
   Check(Global(0x4382f34)==5&&Global(0x4382f2c)==6&&!std::memcmp(uv+2,surface.data()+30,24),"Surface UV/counts");
   for(unsigned t=0;t<3;++t)Check(Global(0x437cf2c+(3+t)*4)==2+t,"Surface triangle offset");
   Hash(&observedFraction,4);Hash(observedMove,12);++cases;
  }
 }
 std::printf("animation_control_cases %u fnv1a64 %016llx\n",cases,static_cast<unsigned long long>(digest));
}

// Read-only optional installation exercise. Asset bytes are never emitted.
static void AssetAnimation(const char *directory,const float sines[1024]) {
 std::vector<std::filesystem::path> files;
 for(const auto &entry:std::filesystem::recursive_directory_iterator(directory))
  if(entry.is_regular_file()&&entry.path().extension()==".md3")files.push_back(entry.path());
 std::sort(files.begin(),files.end());Check(!files.empty(),"No MD3 assets");
 const auto interpolate=reinterpret_cast<void(__cdecl*)(const void*,float,const float*)>(0x445020);
 unsigned assetSurfaces=0,batches=0;uint64_t components=0;digest=14695981039346656037ull;
 for(const auto &file:files){
  std::ifstream input(file,std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  auto word=[&](size_t offset){Check(offset<=bytes.size()&&bytes.size()-offset>=4,"MD3 word bounds");uint32_t result;std::memcpy(&result,bytes.data()+offset,4);return result;};
  Check(bytes.size()>=108&&word(0)==0x33504449&&word(4)==15,"MD3 header");
  const unsigned frames=word(76),count=word(84),length=word(104);size_t offset=word(100);
  Check(frames>0&&frames<65536&&count<1024&&length<=bytes.size(),"MD3 header extent");
  for(unsigned surface=0;surface<count;++surface){
   Check(offset<=length&&length-offset>=108,"MD3 surface header");
   const unsigned surfaceFrames=word(offset+72),vertices=word(offset+80),data=word(offset+100),end=word(offset+104);
   Check(surfaceFrames>=frames&&vertices<2000&&end>=108&&end<=length-offset&&data>=108&&data<=end&&uint64_t(vertices)*surfaceFrames*8<=end-data,"MD3 native surface extent");
   std::vector<std::array<float,4>> xyz(vertices+4),normals(vertices+4);
   Global(0x4382f38)=reinterpret_cast<unsigned>(xyz.data());Global(0x4382f3c)=reinterpret_cast<unsigned>(normals.data());Global(0x4382f34)=2;
   for(unsigned frame=0;frame<frames;++frame){
    entity.frame=frame;entity.oldframe=frame?frame-1:0;
    const float move[3]={13.125f,-7.75f,.0625f};
    for(float fraction:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
     std::memset(xyz.data(),0x71,xyz.size()*sizeof(xyz[0]));std::memset(normals.data(),0x71,normals.size()*sizeof(normals[0]));
     interpolate(bytes.data()+offset,fraction,move);++batches;
     for(unsigned vertex=0;vertex<vertices;++vertex){
      float expectedXYZ[3],expectedNormal[3];
      const auto *oldRecord=bytes.data()+offset+data+(size_t(entity.oldframe)*vertices+vertex)*8;
      const auto *newRecord=bytes.data()+offset+data+(size_t(entity.frame)*vertices+vertex)*8;
      Check(Q3IHV_ModelVertex(oldRecord,newRecord,fraction,move,sines,expectedXYZ,expectedNormal)&&!std::memcmp(expectedXYZ,xyz[vertex+2].data(),12)&&!std::memcmp(expectedNormal,normals[vertex+2].data(),12),"Asset animation numeric parity");
      Hash(xyz[vertex+2].data(),12);Hash(normals[vertex+2].data(),12);components+=6;
     }
     for(unsigned v=0;v<vertices+4;++v)for(unsigned a=0;a<4;++a)if(v<2||v>=vertices+2||a==3){
      uint32_t x,n;std::memcpy(&x,&xyz[v][a],4);std::memcpy(&n,&normals[v][a],4);Check(x==0x71717171&&n==x,"Asset output guards");
     }
    }
   }
   ++assetSurfaces;offset+=end;
  }
 }
 std::printf("asset_models %u surfaces %u batches %u components %llu fnv1a64 %016llx\n",static_cast<unsigned>(files.size()),assetSurfaces,batches,static_cast<unsigned long long>(components),static_cast<unsigned long long>(digest));
}

static LONG WINAPI ReportException(EXCEPTION_POINTERS *failure){
 if(failure->ExceptionRecord->ExceptionCode==0xe06d7363)return EXCEPTION_CONTINUE_SEARCH;
 std::fprintf(stderr,"Native probe exception %08lx at %08lx eax=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\n",failure->ExceptionRecord->ExceptionCode,failure->ContextRecord->Eip,failure->ContextRecord->Eax,failure->ContextRecord->Ecx,failure->ContextRecord->Edx,failure->ContextRecord->Esi,failure->ContextRecord->Edi);
 return EXCEPTION_CONTINUE_SEARCH;
}
int main(int argc,char **argv){
 if(argc<2||argc>4)return 77;std::setvbuf(stdout,nullptr,_IONBF,0);AddVectoredExceptionHandler(1,ReportException);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
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
  struct NativeLight {float origin[3],color[3],radius;};
  std::array<NativeLight,8> dynamicLights{};Global(0x43d6ac4)=reinterpret_cast<unsigned>(dynamicLights.data());
  unsigned dynamicCases=0;seed=0x97135264;
  for(unsigned sample=0;sample<1024;++sample){
   for(float &x:entity.origin)x=float(int(Random()%4096)-2048)/8;
   for(float &x:injectedLight)x=float(int(Random()%4096)-1024)/16;
   for(auto &light:dynamicLights){for(int a=0;a<3;++a){light.origin[a]=entity.origin[a]+float(int(Random()%2048)-1024)/8;light.color[a]=float(int(Random()%2048)-1024)/256;}light.radius=float(Random()%4096)/8;}
   // Include exact boundary, zero distance, minimum distance, and outside light.
   if(sample%4==0){std::memcpy(dynamicLights[0].origin,entity.origin,12);dynamicLights[0].radius=32;}
   if(sample%4==1){std::memcpy(dynamicLights[0].origin,entity.origin,12);dynamicLights[0].origin[0]+=32;dynamicLights[0].radius=32;}
   for(unsigned count:{0u,1u,2u,8u})for(unsigned noWorld:{0u,1u})for(unsigned flags:{0u,0x40u,0x80000000u}){
    Global(0x43d6ac0)=count;Global(0x43d6ab0)=noWorld;entity.flags=flags;lightTraces=0;setupLighting();
    Check(lightTraces==(noWorld?0u:1u),"Dynamic light world selection");
    float expected[3];for(int a=0;a<3;++a)expected[a]=noWorld?255:injectedLight[a];
    for(unsigned l=0;l<count;++l){const auto &light=dynamicLights[l];float delta[3];for(int a=0;a<3;++a)delta[a]=float(double(light.origin[a])-entity.origin[a]);
     volatile double xx=double(delta[0])*delta[0],yy=double(delta[1])*delta[1],zz=double(delta[2])*delta[2];volatile double length=xx+yy;length=length+zz;length=std::sqrt(length);
     if(length>light.radius)continue;
     volatile double power=double(light.radius)*light.radius;power=power*16;volatile float storedPower=float(power);
     if(length<16)length=16;volatile double squared=length*length;volatile double factor=double(storedPower)/squared;
     for(int a=0;a<3;++a){volatile double value=factor*light.color[a];value=value+expected[a];expected[a]=float(value);}
    }
    if(flags)for(float &x:expected)x=float(double(x)+32);
    float maximum=expected[0];for(float x:expected)if(x>maximum)maximum=x;
    if(maximum>255)for(float &x:expected){x=float(double(x)/maximum);x=float(double(x)*255);}
    Check(!std::memcmp(expected,reinterpret_cast<void*>(0x4f00430),12)&&!std::memcmp(expected,reinterpret_cast<void*>(0x4f0043c),12),"Native dynamic model light");
    float candidate[3];for(int a=0;a<3;++a)candidate[a]=noWorld?255:injectedLight[a];for(unsigned l=0;l<count;++l)Q3IHV_LightAddDynamic(entity.origin,dynamicLights[l].origin,dynamicLights[l].color,dynamicLights[l].radius,candidate);Q3IHV_LightFinish(flags,candidate);Check(!memcmp(candidate,expected,12),"Engine dynamic model light");
    Hash(reinterpret_cast<void*>(0x4f00430),24);++dynamicCases;
   }
  }
  std::printf("dynamic_lighting_cases %u fnv1a64 %016llx\n",dynamicCases,static_cast<unsigned long long>(digest));
  Global(0x43d6ac0)=0;

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

  (void)frontendDigest;(void)lerpDigest;(void)lightingDigest;(void)backendDigest;
  Hook(0x42afe0,&SphereCull);Hook(0x42ae20,&BoxCull);
  const auto cull=reinterpret_cast<int(__cdecl*)(const void*,const fntech3_q3_ihv_render_entity_t*)>(0x4420b0);
  unsigned cullCases=0,cullBoxes=0;seed=0x13942785;
  *reinterpret_cast<unsigned*>(cullModel.header+92)=108;
  Global(0x4f00160)=reinterpret_cast<unsigned>(&entity);digest=14695981039346656037ull;
  for(unsigned sample=0;sample<512;++sample){
   for(auto &frame:cullModel.frames){for(int a=0;a<3;++a){frame.bounds[0][a]=float(int(Random()%8192)-4096)*.25f;frame.bounds[1][a]=frame.bounds[0][a]+float(Random()%1024)*.25f;frame.localOrigin[a]=float(int(Random()%512)-256);}frame.radius=float(Random()%2048)/4;}
   for(int a=0;a<3;++a){entity.origin[a]=float(int(Random()%8192)-4096)/4;entity.oldorigin[a]=float(int(Random()%8192)-4096)/4;}
   entity.frame=1;
   for(int oldFrame:{0,1})for(unsigned nonNormalized:{0u,1u})for(unsigned flags:{0u,0x40u,0x840u})for(unsigned first:{0u,1u,2u})for(unsigned second:{0u,1u,2u})for(unsigned box:{0u,1u,2u}){
    entity.oldframe=oldFrame;entity.nonNormalizedAxes=nonNormalized;entity.flags=flags;sphereReplies[0]=first;sphereReplies[1]=second;boxReply=box;sphereCalls=boxCalls=0;
    unsigned expectedSphere=nonNormalized?0u:oldFrame==1?1u:2u;bool useBox=nonNormalized||first==1||(oldFrame!=1&&first!=second);
    unsigned expectedResult=useBox?box:first;const unsigned actual=static_cast<unsigned>(cull(&cullModel,&entity));
    Check(actual==expectedResult&&sphereCalls==expectedSphere&&boxCalls==(useBox?1u:0u),"Cull branch selection");
    Hash(&actual,4);Hash(&sphereCalls,4);Hash(&boxCalls,4);++cullCases;
    if(!useBox)continue;++cullBoxes;
    float expected[2][3];for(int a=0;a<3;++a){const auto&selectedFrame=cullModel.frames[1];const auto&old=cullModel.frames[oldFrame];expected[0][a]=old.bounds[0][a]<selectedFrame.bounds[0][a]?old.bounds[0][a]:selectedFrame.bounds[0][a];expected[1][a]=old.bounds[1][a]>selectedFrame.bounds[1][a]?old.bounds[1][a]:selectedFrame.bounds[1][a];const float delta=flags&0x40?static_cast<float>(double(entity.origin[a])-entity.oldorigin[a]):0;const int side=delta>0?1:0;expected[side][a]=static_cast<float>(double(expected[side][a])+delta);}
    Check(!std::memcmp(expected,observedBounds,24),"Origin-expanded merged frame bounds");float candidateBounds[2][3];Check(Q3IHV_ModelBounds(flags,cullModel.frames[oldFrame].bounds,cullModel.frames[1].bounds,entity.origin,entity.oldorigin,candidateBounds)&&!std::memcmp(candidateBounds,observedBounds,24),"Candidate bounds");Hash(observedBounds,24);
   }
  }
  std::printf("culling_cases %u box_cases %u fnv1a64 %016llx\n",cullCases,cullBoxes,static_cast<unsigned long long>(digest));


  Hook(0x42a718,&EndTrigTables);BuildTrigTables();
  const auto *sines=reinterpret_cast<const float*>(0x4f00488);digest=14695981039346656037ull;Hash(sines,4096);
  std::printf("native_sine_table %016llx\n",static_cast<unsigned long long>(digest));

  unsigned sineDifferent=0;for(unsigned i=0;i<1024;++i){
   volatile double radians=double(i)*0.3519061505794525f;radians=radians*3.141592653589793;radians=radians*(1.0/180.0);
   const float independent=static_cast<float>(std::sin(radians));if(std::memcmp(&independent,sines+i,4))++sineDifferent;
  }
  Check(!sineDifferent,"Independent sine table matches original FSIN table");
  std::printf("sine_independent_cases 1024 different_float_bits %u\n",sineDifferent);
  float candidateSines[1024];Q3IHV_ModelSines(candidateSines);Check(!std::memcmp(candidateSines,sines,4096),"Candidate sine table");
  const auto interpolate=reinterpret_cast<void(__cdecl*)(const void*,float,const float*)>(0x445020);
  float nativeXYZ[20][4],nativeNormals[20][4];
  Global(0x4382f38)=reinterpret_cast<unsigned>(nativeXYZ);Global(0x4382f3c)=reinterpret_cast<unsigned>(nativeNormals);Global(0x4382f34)=2;
  entity.frame=1;entity.oldframe=0;unsigned modelCases=0,modelComponents=0,modernDifferent=0;digest=14695981039346656037ull;seed=0x13462587;
  for(unsigned sample=0;sample<512;++sample)for(unsigned count:{0u,1u,3u,17u}){
   std::array<unsigned char,108+2*17*8> surface{};*reinterpret_cast<unsigned*>(surface.data()+80)=count;*reinterpret_cast<unsigned*>(surface.data()+100)=108;
   auto *packed=reinterpret_cast<short*>(surface.data()+108);for(unsigned k=0;k<count*8;++k)packed[k]=static_cast<short>(Random());
   float move[3];for(float &v:move)v=float(int(Random()%32768)-16384)/8;
   for(float fraction:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
    std::memset(nativeXYZ,0x71,sizeof nativeXYZ);std::memset(nativeNormals,0x71,sizeof nativeNormals);interpolate(surface.data(),fraction,move);++modelCases;
    const float front=static_cast<float>(1.0-double(fraction)),scaledFront=static_cast<float>((1.0-double(fraction))/64),scaledBack=static_cast<float>(double(fraction)/64);
    for(unsigned vertex=0;vertex<count;++vertex){
     const auto *before=packed+vertex*4,*after=packed+(count+vertex)*4;
     for(unsigned a=0;a<3;++a){const float expected=fraction==0?static_cast<float>(double(after[a])*scaledFront):static_cast<float>(double(after[a])*scaledFront+double(before[a])*scaledBack+move[a]);Check(!std::memcmp(&expected,&nativeXYZ[vertex+2][a],4),"Native combined XYZ interpolation");
      if(fraction!=0){volatile float first=float(after[a])*scaledFront,second=float(before[a])*scaledBack;volatile float rounded=first+second;const float modern=static_cast<float>(double(rounded)+move[a]);if(std::memcmp(&expected,&modern,4))++modernDifferent;}
     }
     double decoded[2][3];for(unsigned frame=0;frame<2;++frame){unsigned packedNormal=static_cast<unsigned short>((frame?after:before)[3]);unsigned lat=(packedNormal>>8)*4,lng=(packedNormal&255)*4;decoded[frame][0]=double(sines[(lat+256)&1023])*sines[lng];decoded[frame][1]=static_cast<float>(double(sines[lat])*sines[lng]);decoded[frame][2]=sines[(lng+256)&1023];}
     float expectedNormal[3];for(unsigned a=0;a<3;++a)expectedNormal[a]=fraction==0?static_cast<float>(decoded[1][a]):static_cast<float>(decoded[1][a]*front+decoded[0][a]*fraction);
     if(fraction!=0)Q3IHV_BeamNormalize(expectedNormal);
     Check(!std::memcmp(expectedNormal,nativeNormals[vertex+2],12),"Native interpolated normal arithmetic");float candidateXYZ[3],candidateNormal[3];Check(Q3IHV_ModelVertex(before,after,fraction,move,candidateSines,candidateXYZ,candidateNormal)&&!std::memcmp(candidateXYZ,nativeXYZ[vertex+2],12)&&!std::memcmp(candidateNormal,nativeNormals[vertex+2],12),"Candidate animated vertex");Hash(nativeXYZ[vertex+2],12);Hash(nativeNormals[vertex+2],12);modelComponents+=6;
    }
    for(unsigned vertex=0;vertex<20;++vertex)for(unsigned a=0;a<4;++a)if(vertex<2||vertex>=2+count||a==3){Check(*reinterpret_cast<unsigned*>(&nativeXYZ[vertex][a])==0x71717171&&*reinterpret_cast<unsigned*>(&nativeNormals[vertex][a])==0x71717171,"Vertex stride and extent guards");}
   }
  }
  std::printf("md3_cases %u components %u modern_rounded_xyz_differences %u fnv1a64 %016llx\n",modelCases,modelComponents,modernDifferent,static_cast<unsigned long long>(digest));

  AnimationControl();
  SurfaceLightProbe();
  if(argc>=3)AssetAnimation(argv[2],candidateSines);
  LoadReference(argv[1]);DiffuseLightProbe();GeneratedLightTrees();
  LightCases::Dynamic();LightCases::Surface();LightCases::Malformed();
  if(argc==4)ModelLightMapMetadata(argv[3]);
  return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
