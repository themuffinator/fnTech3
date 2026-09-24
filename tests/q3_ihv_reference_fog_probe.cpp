// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
// Optional Windows x86 binary-observation probe. No reference code is included.
// Link at 0x400000 with /DYNAMICBASE:NO and /MD. The zero-filled observation
// section reserves the reference address range before Windows creates a heap.
// It reads the hash-pinned reference without running its entry point or imports,
// supplies only the fog function's observed POD inputs, and emits numeric facts.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
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
#include "../code/renderercommon/tr_q3_ihv_fog.h"

static_assert(sizeof(void *) == 4, "Build the IHV reference probe as Windows x86");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()

struct Wave { int function; float base, amplitude, phase, frequency; };
struct Fog {
    int brush;
    float bounds[2][3];
    unsigned color;
    float scale;
    float rgb[3];
    int gradient;
    float depth;
    Wave wave;
    float normal[3], distance;
};
static_assert(sizeof(Fog) == 92 && offsetof(Fog, wave) == 56 && offsetof(Fog, normal) == 76);

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

int main(int argc, char **argv) {
    if (argc != 2) { std::fprintf(stderr, "Supply the original IHV quake3.exe (read-only).\n"); return 77; }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        Check(bool(input), "Open reference");
        const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        Check(Pinned(bytes), "Reference SHA-256 differs; refusing to call fixed addresses");
        Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr)) == 0x400000, "Harness image base must be 0x400000");
        ReadImageRange(bytes, 0x440000, 0x10000); // fog functions and original float-to-int helper
        ReadImageRange(bytes, 0x450000, 0x10000); // reference numeric constants
        DWORD previous{};
        Check(VirtualProtect(reinterpret_cast<void *>(0x440000), 0x10000, PAGE_EXECUTE_READ, &previous), "Protect observed code");
        Check(FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(0x440000), 0x10000), "Flush observed code");
        auto calculate = reinterpret_cast<void (__cdecl *)()>(0x4448f0);
        // R_Init table formulas observed at 0x42a685. Only fog code is executed
        // here; supplying these tables avoids graphics and platform startup.
        for (int i=0;i<1024;++i) {
            At<float>(0x4f00488+4*i) = float(std::sin(i*double(360.0f/1023.0f)*3.14159265358979323846/180.0));
            At<float>(0x4f01488+4*i) = i<512 ? 1.0f : -1.0f;
            At<float>(0x4f02488+4*i) = i<256 ? i/256.0f : i<512 ? 1.0f-At<float>(0x4f02488+4*(i-256)) : -At<float>(0x4f02488+4*(i-512));
        }
        const std::array<float,3> identity[3] = {{{1,0,0}},{{0,1,0}},{{0,0,1}}};
        std::memcpy(reinterpret_cast<void *>(0x4f003f4), identity, sizeof(identity));
        At<std::array<float,3>>(0x4f003e8) = {0,0,0}; // entity origin
        std::array<float,4> vertices[6] = {{{0,0,64,0}},{{25,0,64,0}},{{25,0,128,0}},
                                         {{25,0,160,0}},{{0,0,100,0}},{{100,0,64,0}}};
        float result[6]{};
        At<int>(0x4382f34) = 6;
        At<void *>(0x4382f38) = vertices;
        At<void *>(0x4382f48) = result;
        At<int>(0x43d6aac) = 0;
        std::cout << std::setprecision(9) << "{\"function\":\"0x4448f0\",\"reference_sha256\":\"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37\",\"cases\":[\n";
        bool first = true;
        auto observe = [&](int gradient, float eyeZ, int wave, int time, int rotated) {
            Fog fog{};
            fog.bounds[0][2] = 0; fog.bounds[1][2] = 128;
            fog.gradient = gradient; fog.depth = 100;
            fog.scale = gradient ? 2.0f / (100*100) : 1.0f / 100;
            fog.normal[2] = gradient > 0 ? -1.0f : gradient < 0 ? 1.0f : 0;
            fog.distance = gradient > 0 ? -128.0f : 0;
            fog.wave = {wave,100,50,0.125f,0.5f};
            std::memcpy(reinterpret_cast<void *>(0x4f003f4), identity, sizeof(identity));
            At<std::array<float,3>>(0x4f003e8) = {0,0,0};
            if (rotated) {
                const float rotatedAxis[3][3] = {{0,0,1},{0,1,0},{-1,0,0}};
                std::memcpy(reinterpret_cast<void *>(0x4f003f4), rotatedAxis, sizeof(rotatedAxis));
                At<std::array<float,3>>(0x4f003e8) = {10,20,30};
            }
            At<int>(0x43d6aac) = time;
            At<void *>(0x4382f30) = &fog;
            At<std::array<float,3>>(0x4f00418) = {0,0,eyeZ};
            calculate();
            const q3IHVFogParms_t parameters{1,gradient,wave,100,50,0.125f,0.5f};
            const float depth = Q3IHV_FogDepth(&parameters, 100, time);
            double plane[4];
            Q3IHV_FogPlane(gradient, depth, 0, 128, reinterpret_cast<float *>(0x4f003e8),
                          reinterpret_cast<const float (*)[3]>(0x4f003f4), plane);
            for (int i=0;i<6;++i) {
                const float actual = Q3IHV_FogFactor(plane, reinterpret_cast<float *>(0x4f00418), vertices[i].data());
                if (std::fabs(actual-result[i]) > 0.00001f) {
                    std::fprintf(stderr,"gradient=%d eye=%g wave=%d time=%d rotated=%d vertex=%d expected=%g actual=%g\n",gradient,eyeZ,wave,time,rotated,i,result[i],actual);
                    throw std::runtime_error("Independent implementation differs from reference");
                }
            }
            std::cout << (first ? "" : ",\n") << "{\"gradient\":" << gradient << ",\"eyeZ\":" << eyeZ
                      << ",\"wave\":" << wave << ",\"time\":" << time << ",\"rotated\":" << rotated << ",\"factors\":[";
            for (int i=0;i<6;++i) std::cout << (i ? "," : "") << result[i];
            std::cout << "]}"; first = false;
        };
        for (int gradient : {1,-1,0}) for (float eyeZ : {-32.0f,0.0f,64.0f,128.0f,150.0f}) observe(gradient,eyeZ,0,0,0);
        for (int gradient : {1,-1}) for (int wave : {1,2,3}) for (int time : {-1234,0,499,1500,2311}) observe(gradient,64,wave,time,0);
        for (int gradient : {1,-1}) for (float eyeZ : {-32.0f,64.0f,150.0f}) observe(gradient,eyeZ,0,0,1);
        std::cout << "\n]}\n";
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s (Win32 %lu)\n", error.what(), GetLastError()); return 1;
    }
}
