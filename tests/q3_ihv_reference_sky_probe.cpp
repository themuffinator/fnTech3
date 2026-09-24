// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
// Optional Windows x86 binary-observation probe. No reference code is included.
// Link at 0x400000 with /DYNAMICBASE:NO and /MD. The zero-filled observation
// section reserves the reference address range before Windows creates a heap.
// It reads the hash-pinned reference without running its entry point or imports,
// supplies observed sky-iterator POD inputs, stubs drawing, and emits numeric facts.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "../code/renderercommon/tr_q3_ihv_sky.h"

static_assert(sizeof(void *) == 4, "Build the IHV reference probe as Windows x86");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()

static void Check(bool good, const char *message) { if (!good) throw std::runtime_error(message); }

static bool Pinned(const std::vector<char> &bytes) {
    HCRYPTPROV provider{}; HCRYPTHASH hash{};
    Check(CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT), "CryptAcquireContext");
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0); throw std::runtime_error("CryptCreateHash");
    }
    BYTE digest[32]{}; DWORD size = sizeof(digest);
    const bool good = CryptHashData(hash, reinterpret_cast<const BYTE *>(bytes.data()), static_cast<DWORD>(bytes.size()), 0) &&
        CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0);
    CryptDestroyHash(hash); CryptReleaseContext(provider, 0);
    Check(good && size == 32, "SHA-256");
    static const BYTE expected[32] = { 0xe9,0xfc,0xaa,0x0a,0x56,0x82,0xe7,0xf0,0x7d,0x54,0x0a,0xac,0xee,0x08,0xcd,0xb7,
        0x7f,0x3e,0xa4,0x01,0x90,0x71,0x9f,0x5c,0xec,0xf3,0x11,0x09,0xa5,0x13,0x5d,0x37 };
    return !std::memcmp(digest, expected, 32);
}

template<class T> static T &At(unsigned address) { return *reinterpret_cast<T *>(address); }

static void ReadImageRange(const std::vector<char> &bytes, unsigned address, unsigned length) {
    const unsigned first = reinterpret_cast<unsigned>(observation);
    Check(address >= first && address-first <= sizeof(observation)-length, "Observation range overlaps harness");
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(bytes.data());
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(bytes.data()+dos->e_lfanew);
    const auto *section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const unsigned start = 0x400000+section[i].VirtualAddress;
        const unsigned end = start+section[i].SizeOfRawData;
        const unsigned from = address > start ? address : start;
        const unsigned to = address+length < end ? address+length : end;
        if (from >= to) continue;
        const size_t offset = section[i].PointerToRawData+(from-start);
        Check(offset <= bytes.size() && to-from <= bytes.size()-offset, "Source section range");
        std::memcpy(reinterpret_cast<void *>(from), bytes.data()+offset, to-from);
    }
}

struct StageObservation {int stage;float rgb[3];};
static std::vector<StageObservation> observedStages;
static void __stdcall ObserveColor(float,float,float,float) {}
static void __cdecl ObserveStage(void *,int stage) {
    StageObservation result{stage,{0,0,0}};
    std::memcpy(result.rgb, reinterpret_cast<void *>(0x46bbb0),12);
    observedStages.push_back(result);
}
static void Jump(unsigned address,void *function) {
    At<unsigned char>(address)=0xe9;
    At<unsigned>(address+1)=reinterpret_cast<unsigned>(function)-address-5;
}
int main(int argc,char **argv) {
    if(argc!=2)return 77;
    try {
        std::ifstream input(argv[1],std::ios::binary);Check(bool(input),"Open reference");
        const std::vector<char> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        Check(Pinned(bytes),"Unexpected original executable");
        Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x400000,"Image base");
        ReadImageRange(bytes,0x410000,0x10000);
        ReadImageRange(bytes,0x42a000,0x1000);
        ReadImageRange(bytes,0x440000,0x10000);
        ReadImageRange(bytes,0x450000,0x10000);
        At<unsigned char>(0x42aac0)=0xc3; // Observe renderer-state calls without OpenGL.
        Jump(0x4442d0,reinterpret_cast<void *>(&ObserveStage)); // Observe each cloud draw's color.
        At<void *>(0x43d6540)=reinterpret_cast<void *>(&ObserveColor);
        for(auto range:{std::array<unsigned,2>{0x410000,0x10000},{0x42a000,0x1000},{0x440000,0x10000}}) {
            DWORD old{};Check(VirtualProtect(reinterpret_cast<void *>(range[0]),range[1],PAGE_EXECUTE_READ,&old),"Code protection");
            Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(range[0]),range[1]),"Flush code");
        }
        std::vector<unsigned> shader(0x28a4/4+1),stages(8*0x27c/4),batch(0x601c/4+1);
        shader[0x4c/4]=1;
        *reinterpret_cast<float *>(&shader[0x58/4])=512;

        shader[0x284c/4]=reinterpret_cast<unsigned>(stages.data());

        batch[0]=reinterpret_cast<unsigned>(shader.data());
        std::array<unsigned,12> fastSky{};
        At<void *>(0x4f0356c)=fastSky.data();
        auto iterate=reinterpret_cast<void (__cdecl *)(void *)>(0x443510);

        std::cout<<std::setprecision(9)<<"{\"function\":\"0x443510\",\"reference_sha256\":\"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37\",\"draw_calls_stubbed\":true,\"cases\":[\n";
        bool firstCase=true;
        for(int active:{0,1,2,3,8})for(int lightning:{0,1}){
            std::fill(stages.begin(),stages.end(),0);
            for(int i=0;i<active;++i)stages[i*0x27c/4]=1;
            shader[0x5c/4]=lightning;
            shader[0x60/4]=0;At<unsigned>(0x41e0648)=0;
            uint32_t seed=0;float lastFlash=0;
            std::cout<<(firstCase?"":",\n")<<"{\"stages\":"<<active<<",\"lightning\":"<<lightning<<",\"frames\":[\n";
            firstCase=false;
            for(int time=0;time<10000;time+=137){
                At<int>(0x43d6aac)=time;observedStages.clear();iterate(batch.data());
                Check(observedStages.size()==unsigned(active),"Active cloud stage count");
                for(int stage=0;stage<8;++stage){
                    const int expected=Q3IHV_SkyStageColor(&seed,&lastFlash,time,lightning,stage);
                    if(stage==active)break;
                    for(float color:observedStages[stage].rgb)
                        Check(color==expected/255.0f,"Cloud color differs from independent implementation");
                }
                Check(seed==At<unsigned>(0x41e0648),"Random advancement differs");
                Check(std::fabs(lastFlash-*reinterpret_cast<float *>(&shader[0x60/4]))<0.000001f,"Flash timestamp differs");
                std::cout<<(time?",\n":"")<<"{\"time\":"<<time<<",\"seed\":"<<At<unsigned>(0x41e0648)
                         <<",\"last_flash\":"<<*reinterpret_cast<float *>(&shader[0x60/4])<<",\"stage_rgb\":[";
                for(size_t i=0;i<observedStages.size();++i){if(i)std::cout<<',';
                    const auto &v=observedStages[i];std::cout<<'['<<v.rgb[0]<<','<<v.rgb[1]<<','<<v.rgb[2]<<']';}
                std::cout<<"]}";
            }
            std::cout<<"\n]}";
        }
        std::cout<<"\n]}\n";
        return 0;
    } catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
