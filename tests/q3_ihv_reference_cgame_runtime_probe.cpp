/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional hash-pinned x86 native cgame Init/frame/draw observation. Real MD3
 * tag metadata; synthetic collision, resource and sound services. Rendering
 * calls are captured only. No engine, display, input or network session.
 * Normal DLL loading initializes the module CRT; no original code is patched. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

static_assert(sizeof(void*)==4,"Original cgame requires x86");
static void Check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
static std::vector<char> Verified(const char *path,const char *expected){
 std::ifstream input(path,std::ios::binary);Check(bool(input),"Open supplied cgame");
 std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
 HCRYPTPROV provider{};HCRYPTHASH hash{};
 Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){CryptReleaseContext(provider,0);Check(false,"Hash create");}
 BYTE digest[32]{};DWORD length=32;
 const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()),0)&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0);
 CryptDestroyHash(hash);CryptReleaseContext(provider,0);Check(ok&&length==32,"Hash result");
 char text[65]{};for(unsigned i=0;i<32;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);
 Check(!std::strcmp(text,expected),"Unexpected input hash");return data;
}
struct Library {HMODULE value{};~Library(){if(value)FreeLibrary(value);}};

#include "q3_ihv_client_cases.hpp"
#include "../code/client/client_ihv_session.hpp"
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_ihv_model_tags.hpp"
#include "../code/compat/q3_ihv_scene.hpp"
#include <cstdlib>
namespace client=fntech3::q3ihv::client;
namespace wire=fntech3::q3ihv::wire;
namespace cases=ihv_client_cases;
static const client::ClientData *nativeData;
static unsigned char *moduleBase;
static void __cdecl GetState(void *out){Check(out==moduleBase+0xa1104&&nativeData&&nativeData->CopyGameState(out,20100),"Native gamestate import");}
#include <filesystem>
#include <map>
#include <cstdarg>
namespace fs=std::filesystem;
static std::array<unsigned,62> calls{};
static fs::path assetRoot;
static int clientState=7;
static unsigned snapshotCalls,frameCalls,readyFrames;
static unsigned frameTime;
static bool runFrames;
static unsigned failImport;
struct InjectedFailure {};
static unsigned scenes,sceneEntities,sceneLights,sceneParticles,scenePolys;
static ihv_client_cases::Digest sceneDigest;
static std::array<ihv_client_cases::Digest,29> viewFields;
static std::array<ihv_client_cases::Digest,25> entityFields;
static void DigestMemory(const void *pointer,std::size_t bytes){
 Check(!bytes||pointer,"Scene array pointer");const auto *data=static_cast<const unsigned char*>(pointer);
 if(bytes){MEMORY_BASIC_INFORMATION region{};Check(VirtualQuery(pointer,&region,sizeof region)==sizeof region&&region.State==MEM_COMMIT&&!(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)),"Scene readable memory");const auto offset=reinterpret_cast<std::uintptr_t>(pointer)-reinterpret_cast<std::uintptr_t>(region.BaseAddress);Check(offset<=region.RegionSize&&bytes<=region.RegionSize-offset,"Scene array extent");}
 sceneDigest.Bytes(data,bytes);
}
static fntech3::q3ihv::TaggedMemory fileMemory(16*1024*1024);
static std::vector<std::string> fileNames;
static std::map<std::string,int> resources;
static std::map<std::string,std::unique_ptr<fntech3::q3ihv::NativeModelMetadata>> tagModels;
static fntech3_q3_ihv_tag_t tagResult{};
static unsigned tagMatches;
namespace render=fntech3::q3ihv::render;
static unsigned ownedScenes,ownedEntities,ownedParticles,ownedPolys;
static ihv_client_cases::Digest ownedDigest;
struct SceneMemory final:render::Memory {
 bool Read(std::uint32_t address,void *output,std::size_t bytes)const override{
  // Check every page-range spanned by the copy; no OS or input capture.
  const auto *data=reinterpret_cast<const unsigned char*>(address);std::size_t done=0;
  while(done<bytes){MEMORY_BASIC_INFORMATION region{};const auto *at=data+done;
   if(VirtualQuery(at,&region,sizeof region)!=sizeof region||region.State!=MEM_COMMIT||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
   const auto access=region.Protect&0xff;if(access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
   const auto offset=reinterpret_cast<std::uintptr_t>(at)-reinterpret_cast<std::uintptr_t>(region.BaseAddress);
   if(offset>=region.RegionSize)return false;const auto length=(std::min)(bytes-done,static_cast<std::size_t>(region.RegionSize-offset));std::memcpy(static_cast<unsigned char*>(output)+done,at,length);done+=length;
  }return true;
 }
};
struct SceneResources final:render::Resources {
 bool Resolve(render::ResourceKind kind,std::uint32_t token,std::int32_t &handle)const override{
  const auto prefix=kind==render::ResourceKind::Model?"40:":kind==render::ResourceKind::Skin?"41:":"42:";
  for(const auto &entry:resources)if(static_cast<std::uint32_t>(entry.second)==token&&entry.first.compare(0,3,prefix)==0){handle=entry.second;return true;}return false;
 }
};
static void ObserveOwnedScene(const void *data){
 render::Scene scene;const auto result=render::Capture(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data)),SceneMemory{},SceneResources{},scene);
 Check(result==render::Result::Ok,"Owned native scene capture");
 // Hash published values field-by-field: no native pointers or C++ padding.
 auto &hash=ownedDigest;const auto &v=scene.view;
 hash.Word(v.reserved);for(auto value:{v.x,v.y,v.width,v.height,v.time})hash.Word(static_cast<unsigned>(value));hash.Word(v.flags);
 hash.Bytes(reinterpret_cast<const unsigned char*>(&v.fovX),4);hash.Bytes(reinterpret_cast<const unsigned char*>(&v.fovY),4);
 hash.Bytes(reinterpret_cast<const unsigned char*>(v.origin),sizeof v.origin);hash.Bytes(reinterpret_cast<const unsigned char*>(v.axis),sizeof v.axis);
 hash.Bytes(reinterpret_cast<const unsigned char*>(v.overlayGeometry),sizeof v.overlayGeometry);hash.Bytes(reinterpret_cast<const unsigned char*>(v.overlayColor),sizeof v.overlayColor);
 hash.Word(static_cast<unsigned>(v.overlayShader));hash.Bytes(v.BlockedAreas().data(),32);
 hash.Word(static_cast<unsigned>(scene.entities.size()));
 for(const auto &e:scene.entities){for(auto value:{e.model,e.skin,e.shader,e.frame,e.oldframe,e.skinNum})hash.Word(static_cast<unsigned>(value));
  hash.Bytes(reinterpret_cast<const unsigned char*>(e.axis),sizeof e.axis);hash.Bytes(reinterpret_cast<const unsigned char*>(e.origin),sizeof e.origin);hash.Bytes(reinterpret_cast<const unsigned char*>(e.oldorigin),sizeof e.oldorigin);
  hash.Bytes(reinterpret_cast<const unsigned char*>(&e.backlerp),4);hash.Word(e.flags);hash.Word(e.nonNormalizedAxes?1:0);hash.Bytes(e.rgba.data(),4);
 }
 hash.Word(static_cast<unsigned>(scene.lights.size()));for(const auto &l:scene.lights){hash.Bytes(reinterpret_cast<const unsigned char*>(l.origin),sizeof l.origin);hash.Bytes(reinterpret_cast<const unsigned char*>(l.color),sizeof l.color);hash.Bytes(reinterpret_cast<const unsigned char*>(&l.radius),4);}
 hash.Word(static_cast<unsigned>(scene.particles.size()));for(const auto &p:scene.particles){hash.Bytes(reinterpret_cast<const unsigned char*>(p.origin),sizeof p.origin);hash.Bytes(p.rgba.data(),4);}
 hash.Word(static_cast<unsigned>(scene.polygons.size()));for(const auto &p:scene.polygons){hash.Word(static_cast<unsigned>(p.shader));hash.Word(static_cast<unsigned>(p.vertices.size()));for(const auto &vertex:p.vertices){hash.Bytes(reinterpret_cast<const unsigned char*>(vertex.xyz),sizeof vertex.xyz);hash.Bytes(reinterpret_cast<const unsigned char*>(vertex.st),sizeof vertex.st);hash.Bytes(vertex.rgba.data(),4);}}
 ++ownedScenes;ownedEntities+=static_cast<unsigned>(scene.entities.size());ownedParticles+=static_cast<unsigned>(scene.particles.size());ownedPolys+=static_cast<unsigned>(scene.polygons.size());
}
static LONG CALLBACK Fault(EXCEPTION_POINTERS *fault){
 if(fault->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){
  const auto *c=fault->ContextRecord;std::fprintf(stderr,"Native fault eip=%08lx address=%08lx eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx snapshots=%u frames=%u\n",c->Eip,static_cast<unsigned long>(fault->ExceptionRecord->ExceptionInformation[1]),c->Eax,c->Ebx,c->Ecx,c->Edx,c->Esi,c->Edi,snapshotCalls,frameCalls);std::fflush(stderr);
 }return EXCEPTION_CONTINUE_SEARCH;
}
struct Variable {fntech3::q3ihv::CvarMirror mirror;std::string name,value,reset;int flags=0;};
static std::map<std::string,std::unique_ptr<Variable>> variables;
static void __cdecl Print(const char *format,...){++calls[1];va_list args;va_start(args,format);std::vfprintf(stderr,format,args);va_end(args);}
static void __cdecl Error(int,const char *format,...){++calls[2];char text[1024];va_list args;va_start(args,format);std::vsnprintf(text,sizeof text,format,args);va_end(args);throw std::runtime_error(text);}
static fntech3_q3_ihv_cvar_t *__cdecl Cvar(const char *name,const char *value,int flags){
 ++calls[4];Check(name&&value&&std::strlen(name)<256&&std::strlen(value)<1024,"Cvar extent");
 auto &entry=variables[name];if(!entry){entry=std::make_unique<Variable>();entry->name=name;entry->value=entry->reset=value;}entry->flags|=flags;
 Check(entry->mirror.Assign(entry->name.c_str(),entry->value.c_str(),entry->reset.c_str(),nullptr,entry->flags,1,std::strtof(entry->value.c_str(),nullptr)),"Cvar mirror");return entry->mirror.Record();
}
static int __cdecl ReadAsset(const char *name,void **output){
 ++calls[9];Check(name&&output,"Read arguments");*output=nullptr;fileNames.emplace_back(name);
 fs::path relative(name);Check(!relative.is_absolute()&&!relative.has_root_name()&&!relative.has_root_directory()&&std::string(name).find(':')==std::string::npos,"Absolute file path");
 for(const auto &part:relative)Check(part!="..","Parent traversal");
 const auto path=fs::weakly_canonical(assetRoot/relative),inside=path.lexically_relative(assetRoot);
 Check(!inside.empty()&&*inside.begin()!="..","File escapes asset root");
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return -1;
 const auto size=file.tellg();Check(size>=0&&size<4*1024*1024,"File extent");
 auto *data=fileMemory.Allocate(static_cast<int>(size)+1,0);Check(data!=nullptr,"File allocation");
 file.seekg(0);file.read(static_cast<char*>(data),size);Check(bool(file),"Short read");*output=data;return static_cast<int>(size);
}
static void __cdecl FreeFile(void *data){++calls[10];Check(fileMemory.Free(data),"File ownership");}
static void __cdecl Map(const char *name,int clientLoad,unsigned *checksum){++calls[18];Check(name&&clientLoad==1&&checksum,"Map arguments");*checksum=0x12345678;}
static int __cdecl MapLoaded(const char *name){++calls[19];Check(name&&*name,"Loaded map name");return 1;}
static int __cdecl Inline(int index){++calls[20];Check(index>0,"Inline index");return index;}
static const void *__cdecl CollisionModel(const char *name){
 ++calls[21];Check(name&&*name,"Collision model name");const std::string key(name);
 const auto known=tagModels.find(key);if(known!=tagModels.end())return known->second->Record();
 if(key.size()<4||key.substr(key.size()-4)!=".md3")return nullptr;
 void *data{};const int length=ReadAsset(name,&data);if(length<0)return nullptr;
 auto model=std::make_unique<fntech3::q3ihv::NativeModelMetadata>();const bool ok=model->Load(name,data,static_cast<std::size_t>(length));FreeFile(data);Check(ok,"MD3 metadata rejected");
 const auto *result=model->Record();tagModels.emplace(key,std::move(model));return result;
}
static int __cdecl ModelCount(){++calls[22];return 1;}
static int __cdecl BoxLeafs(const float *mins,const float *maxs,int *list,int capacity,int *top){++calls[16];Check(mins&&maxs&&list&&capacity>=0&&top,"Box leaf arguments");*top=-1;return 0;}
static void *__cdecl LerpTag(void *handle,int start,int end,float fraction,const char *name){
 ++calls[17];Check(name!=nullptr,"Tag name");const fntech3::q3ihv::ModelTags *model=nullptr;
 if(failImport==17)throw InjectedFailure{};
 for(const auto &entry:tagModels)if(entry.second->Record()==handle)model=&entry.second->Metadata();
 Check(model!=nullptr,"Unowned tag model");if(!model->Lerp(start,end,fraction,name,tagResult))return nullptr;++tagMatches;return &tagResult;
}
static int __cdecl TempBox(const float *mins,const float *maxs){++calls[15];Check(mins&&maxs,"Temp box bounds");return 0x10000;}
static int __cdecl PointContents(const float *point,int){++calls[23];Check(point!=nullptr,"Point contents position");return 0;}
static int __cdecl TransformedContents(const float *point,int,const float *origin,const float *angles){++calls[24];Check(point&&origin&&angles,"Transformed contents arguments");return 0;}
static fntech3_q3_ihv_trace_t __cdecl BoxTrace(const float *start,const float *end,const float*,const float*,int,int){++calls[25];Check(start&&end,"Box trace arguments");fntech3_q3_ihv_trace_t result{};result.fraction=1;std::memcpy(result.endpos,end,12);return result;}
static fntech3_q3_ihv_trace_t __cdecl TransformedTrace(const float *start,const float *end,const float *mins,const float *maxs,int model,int mask,const float *origin,const float *angles){++calls[26];Check(origin&&angles,"Transformed trace arguments");return BoxTrace(start,end,mins,maxs,model,mask);}
static unsigned __cdecl Milliseconds(){++calls[3];return frameTime;}
static void __cdecl StartSound(const float*,int,int,void*){++calls[27];}
static void __cdecl LocalSound(const char *name){++calls[28];Check(name!=nullptr,"Local sound name");}
static void __cdecl ClearLoops(){++calls[29];}
static void __cdecl AddLoop(const float *origin,const float *velocity,void*){++calls[30];Check(origin&&velocity,"Loop arguments");}
static void __cdecl Respatialize(int,const float *origin,const float *axis,void(__cdecl *position)(int,float*)){++calls[31];Check(origin&&axis&&position,"Respatialize arguments");}
static void __cdecl SoundBegin(){++calls[32];}
static void __cdecl SoundEnd(){++calls[35];}
static void __cdecl Music(const char *name){++calls[36];Check(name!=nullptr,"Music name");}
static void __cdecl World(const char *name){++calls[39];Check(name&&*name,"World name");}
static void __cdecl RenderScene(const void *data){
 ++calls[43];Check(data!=nullptr,"Scene pointer");const auto *words=static_cast<const unsigned*>(data);
 if(failImport==43)throw InjectedFailure{};
 ObserveOwnedScene(data);
 Check(words[3]>0&&words[3]<=640&&words[4]>0&&words[4]<=480,"Scene viewport");
 Check(words[30]<=1023&&words[32]<=32&&words[34]<=4096&&words[36]<=256,"Scene counts");
 DigestMemory(data,116); // Fixed view data; exclude native array addresses.
 for(unsigned i=0;i<29;++i)viewFields[i].Word(words[i]);
 if(words[29])DigestMemory(reinterpret_cast<const void*>(words[29]),32);
 for(unsigned i=0;i<4;++i){sceneDigest.Word(words[30+i*2]);if(i<3)DigestMemory(reinterpret_cast<const void*>(words[31+i*2]),words[30+i*2]*(i==0?100:i==1?28:16));}
 const auto *polys=reinterpret_cast<const unsigned*>(words[37]);
 const auto *entities=reinterpret_cast<const unsigned*>(words[31]);for(unsigned e=0;e<words[30];++e)for(unsigned i=0;i<25;++i)entityFields[i].Word(entities[e*25+i]);
 for(unsigned i=0;i<words[36];++i){Check(polys[i*4+1]<=1024,"Polygon vertex count");sceneDigest.Word(polys[i*4]);sceneDigest.Word(polys[i*4+1]);DigestMemory(reinterpret_cast<const void*>(polys[i*4+2]),polys[i*4+1]*24);}
 ++scenes;sceneEntities+=words[30];sceneLights+=words[32];sceneParticles+=words[34];scenePolys+=words[36];
}
static void __cdecl SetColor(const float*){++calls[44];}
static void __cdecl Stretch(float,float,float,float,float,float,float,float,void*){++calls[45];}
static void __cdecl Pic(float,float,float,float,void*){++calls[53];}
static void __cdecl Text(int,int,const char *text,float){++calls[54];Check(text!=nullptr,"Draw string");}
static int __cdecl TextWidth(const char *text){++calls[49];Check(text!=nullptr,"Text width");int length=0;while(*text){if(static_cast<unsigned char>(text[0])==27&&text[1]>='0'&&text[1]<='7')text+=2;else{++length;++text;}}return length*16;}
static void __cdecl Adjust(float*,float*,float*,float*){++calls[51];}
static void __cdecl Fill(float,float,float,float,const float *color){++calls[52];Check(color!=nullptr,"Fill color");}
static void __cdecl ColoredText(int,int,const char *text,const float *color){++calls[55];Check(text&&color,"Colored text");}
template<unsigned Slot> static int __cdecl Register(const char *name){
 ++calls[Slot];if constexpr(Slot==40){if(!name||!*name)return 0;}
 if(!name)throw std::runtime_error("Null resource name at import "+std::to_string(Slot));Check(std::strlen(name)<1024,"Resource name extent");
 const auto key=std::to_string(Slot)+":"+name;const auto found=resources.find(key);if(found!=resources.end())return found->second;
 const auto handle=static_cast<int>(resources.size())+1;resources.emplace(key,handle);return handle;
}
static void __cdecl Screen(){++calls[48];Check(clientState==2,"Loading state");}
static void __cdecl State(void *out){++calls[56];GetState(out);}
static int __cdecl Entity(int number,void *out){++calls[58];Check(nativeData!=nullptr,"Entity context");const auto result=nativeData->CopyEntity(static_cast<unsigned>(number),out,164);Check(result!=client::ReadResult::FutureEntity&&result!=client::ReadResult::InvalidOutput,"Entity history request");return result==client::ReadResult::Ok;}
static unsigned __cdecl CurrentCommand(){++calls[59];Check(nativeData!=nullptr,"Command context");return nativeData->CurrentCommandNumber();}
static int __cdecl Command(int number,void *out){++calls[60];Check(nativeData!=nullptr,"Command context");return nativeData->CopyCommand(static_cast<unsigned>(number),out,14)==client::ReadResult::Ok;}
template<std::size_t Slot> static void __cdecl Unhandled(){throw std::runtime_error("Unsupported import "+std::to_string(Slot));}
template<std::size_t... Slots> static std::array<void*,62> Imports(std::index_sequence<Slots...>){return {reinterpret_cast<void*>(&Unhandled<Slots>)...};}
struct RuntimeEvents final:client::ClientEvents {
 void **exports{};std::array<void*,62> imports=Imports(std::make_index_sequence<62>{});
 void GameState(const client::ClientData &data)override{nativeData=&data;reinterpret_cast<void(__cdecl*)(void*)>(exports[0])(imports.data());}
 void Configstring(const client::ClientData&,std::uint32_t index)override{reinterpret_cast<void(__cdecl*)(int)>(exports[2])(index);}
 void ServerCommand(const client::ClientData&,std::string_view)override{Check(false,"No server command execution");}
 void Snapshot(const client::ClientData &data,const fntech3_q3_ihv_snapshot_t &snapshot)override{
  Check(nativeData==&data,"Stable client data");reinterpret_cast<void(__cdecl*)(const void*)>(exports[1])(&snapshot);++snapshotCalls;
  if(runFrames){frameTime=(snapshotCalls-1)*50;const bool ready=!!reinterpret_cast<int(__cdecl*)(int,int)>(exports[5])(static_cast<int>(frameTime),0);readyFrames+=ready;++frameCalls;if(ready)reinterpret_cast<void(__cdecl*)(float)>(exports[6])(0.0f);}
 }
};
int main(int argc,char **argv){
 if(argc<3||argc>5)return 77;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,&Fault);
 try{
  Verified(argv[1],"175f94756f994b06235690fdd2d9fe798503f7a797f2e070277a672847593614");assetRoot=fs::canonical(argv[2]);
  runFrames=argc==5;if(runFrames){if(!std::strcmp(argv[4],"fail-tag"))failImport=17;else if(!std::strcmp(argv[4],"fail-scene"))failImport=43;else Check(!std::strcmp(argv[4],"frames"),"Frame mode");}
  Library library;library.value=LoadLibraryExA(argv[1],nullptr,0);Check(library.value!=nullptr,"Initialize cgame DLL runtime");moduleBase=reinterpret_cast<unsigned char*>(library.value);
  const auto getter=reinterpret_cast<void**(__cdecl*)()>(GetProcAddress(library.value,"GetCGameAPI"));Check(getter!=nullptr,"Cgame getter");
  RuntimeEvents events;events.exports=getter();auto &imports=events.imports;imports[0]=imports[57]=nullptr;
  imports[1]=reinterpret_cast<void*>(&Print);imports[2]=reinterpret_cast<void*>(&Error);imports[4]=reinterpret_cast<void*>(&Cvar);
  imports[3]=reinterpret_cast<void*>(&Milliseconds);imports[27]=reinterpret_cast<void*>(&StartSound);imports[28]=reinterpret_cast<void*>(&LocalSound);imports[29]=reinterpret_cast<void*>(&ClearLoops);imports[30]=reinterpret_cast<void*>(&AddLoop);
  imports[19]=reinterpret_cast<void*>(&MapLoaded);imports[31]=reinterpret_cast<void*>(&Respatialize);
  imports[9]=reinterpret_cast<void*>(&ReadAsset);imports[10]=reinterpret_cast<void*>(&FreeFile);
  imports[18]=reinterpret_cast<void*>(&Map);imports[20]=reinterpret_cast<void*>(&Inline);imports[21]=reinterpret_cast<void*>(&CollisionModel);imports[22]=reinterpret_cast<void*>(&ModelCount);
  imports[16]=reinterpret_cast<void*>(&BoxLeafs);imports[17]=reinterpret_cast<void*>(&LerpTag);
  imports[15]=reinterpret_cast<void*>(&TempBox);imports[23]=reinterpret_cast<void*>(&PointContents);imports[24]=reinterpret_cast<void*>(&TransformedContents);imports[25]=reinterpret_cast<void*>(&BoxTrace);imports[26]=reinterpret_cast<void*>(&TransformedTrace);
  imports[32]=reinterpret_cast<void*>(&SoundBegin);imports[33]=reinterpret_cast<void*>(&Register<33>);imports[34]=reinterpret_cast<void*>(&Register<34>);imports[35]=reinterpret_cast<void*>(&SoundEnd);imports[36]=reinterpret_cast<void*>(&Music);
  imports[37]=reinterpret_cast<void*>(640);imports[38]=reinterpret_cast<void*>(480);imports[39]=reinterpret_cast<void*>(&World);
  imports[40]=reinterpret_cast<void*>(&Register<40>);imports[41]=reinterpret_cast<void*>(&Register<41>);imports[42]=reinterpret_cast<void*>(&Register<42>);imports[48]=reinterpret_cast<void*>(&Screen);imports[56]=reinterpret_cast<void*>(&State);imports[61]=&clientState;
  imports[43]=reinterpret_cast<void*>(&RenderScene);imports[44]=reinterpret_cast<void*>(&SetColor);imports[45]=reinterpret_cast<void*>(&Stretch);imports[53]=reinterpret_cast<void*>(&Pic);imports[54]=reinterpret_cast<void*>(&Text);
  imports[49]=reinterpret_cast<void*>(&TextWidth);imports[51]=reinterpret_cast<void*>(&Adjust);imports[52]=reinterpret_cast<void*>(&Fill);
  imports[55]=reinterpret_cast<void*>(&ColoredText);
  imports[58]=reinterpret_cast<void*>(&Entity);imports[59]=reinterpret_cast<void*>(&CurrentCommand);imports[60]=reinterpret_cast<void*>(&Command);
  std::vector<std::uint8_t> packet;std::vector<char> supplied;wire::MessageWriter writer;
  if(argc>=4){supplied=Verified(argv[3],"fbdc209d1c7fc1075976c9120c79d5e579ed36f4e38062b01248d6b2d807c57b");wire::DemoReader reader(supplied.data(),supplied.size());wire::DemoRecord record;Check(reader.Next(record)==wire::DemoRecordResult::Message,"Initial demo message");packet.assign(record.data,record.data+record.size);}
  else{auto game=cases::Game();game.configstrings[27]=std::string(255,'0');Check(writer.AddGameState(game),"Synthetic gamestate");packet.assign(writer.Data(),writer.Data()+writer.Bytes());}
  auto session=std::make_unique<client::ClientSession>();const auto result=session->Feed(packet.data(),packet.size(),0,events);Check(result.code==client::PacketResult::Delivered&&session->Data().Ready(),"Init packet delivered");
  Check(calls[4]==59&&calls[18]==1&&calls[32]==1&&calls[35]==1&&calls[36]==1&&calls[39]==1&&calls[56]==1,"Completed Init sequence");
  if(!supplied.empty()){
   wire::DemoReader reader(supplied.data(),supplied.size());wire::DemoRecord record;wire::DemoRecordResult status;
   Check(reader.Next(record)==wire::DemoRecordResult::Message,"Skip initial message");
   try{while((status=reader.Next(record))==wire::DemoRecordResult::Message)Check(session->Feed(record.data,record.size,0,events).code==client::PacketResult::Delivered,"Supplied snapshot delivery");}
   catch(const InjectedFailure&){Check(failImport&&!session->Data().Ready()&&fileMemory.Count()==0,"Native callback failure invalidates client session");nativeData=nullptr;std::printf("{\"injected_import\":%u,\"failure_cleanup\":true}\n",failImport);return 0;}
   Check(!failImport,"Failure injection was reached");
   Check(status==wire::DemoRecordResult::End&&snapshotCalls==56,"Supplied replay complete");
  }
  Check(fileMemory.Count()==0,"Released metadata file buffers");
  if(runFrames)Check(readyFrames==56&&scenes==124&&tagMatches==472,"Complete native frame/draw/tag sequence");
  Check(ownedScenes==scenes&&ownedEntities==sceneEntities&&ownedParticles==sceneParticles&&ownedPolys==scenePolys,"Every native scene reaches the owned boundary");
  std::printf("{\"scope\":\"original cgame Init, unpatched snapshots and optional frame/draw callbacks; real read-only MD3 tags, synthetic collision/resource services, captured scenes; no engine/display/input/audio/network session\",\"snapshots\":%u,\"frame_callbacks\":%u,\"ready_frames\":%u,\"packet_graph_count\":%u,\"files\":%zu,\"tag_models\":%zu,\"tag_matches\":%u,\"resources\":%zu,\"calls\":[",snapshotCalls,frameCalls,readyFrames,*reinterpret_cast<unsigned*>(moduleBase+0x1984a4),fileNames.size(),tagModels.size(),tagMatches,resources.size());
  for(unsigned i=0;i<62;++i)std::printf("%s%u",i?",":"",calls[i]);std::printf("],\"scenes\":%u,\"scene_entities\":%u,\"scene_lights\":%u,\"scene_particles\":%u,\"scene_polys\":%u,\"scene_fnv1a64\":\"%016llx\",\"view_fields\":[",scenes,sceneEntities,sceneLights,sceneParticles,scenePolys,static_cast<unsigned long long>(sceneDigest.value));
  for(unsigned i=0;i<viewFields.size();++i)std::printf("%s\"%016llx\"",i?",":"",static_cast<unsigned long long>(viewFields[i].value));std::printf("],\"entity_fields\":[");for(unsigned i=0;i<entityFields.size();++i)std::printf("%s\"%016llx\"",i?",":"",static_cast<unsigned long long>(entityFields[i].value));
  std::printf("],\"owned_scene\":{\"scenes\":%u,\"entities\":%u,\"particles\":%u,\"polygons\":%u,\"fnv1a64\":\"%016llx\"}}\n",ownedScenes,ownedEntities,ownedParticles,ownedPolys,static_cast<unsigned long long>(ownedDigest.value));nativeData=nullptr;return 0;
 }catch(const std::exception&e){nativeData=nullptr;std::fprintf(stderr,"%s\n",e.what());return 1;}
}
