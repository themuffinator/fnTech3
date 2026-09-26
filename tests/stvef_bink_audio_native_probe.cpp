/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Optional Windows x86 probe: runtime DLL, movie file, en/de, PCM output,
 * and optionally file, pause or device. The default observes the installed
 * DirectSound backend through a silent COM buffer. Device mode forwards to
 * the real device with a -30 dB ceiling and reads only this process's meter.
 * No game data, proprietary SDK headers or runtime are distributed. The Bink
 * sound callback prefix is independently observed in the installed DLL;
 * this probe supports one movie at a time. See the movie-audio evidence record.
 * Build with the repository's Windows x86 C++17 toolchain and link
 * dsound.lib, user32.lib and ole32.lib. */
#define NOMINMAX
#include <windows.h>
#include <dsound.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

static_assert(sizeof(void *)==4,"The installed Elite Force runtime uses the Windows x86 ABI");

static std::vector<unsigned char> pcm;
static unsigned pcmFrequency,pcmBits,pcmChannels,unlocks,plays,stops;
static std::vector<LONG> volumes;
static IDirectSound *hardwareDevice=nullptr;
static unsigned hardwareErrors,hardwareCalls;
static DWORD maximumCursor;
static IAudioMeterInformation *ownMeter=nullptr;
static float maximumOwnPeak=0;
static unsigned ownMeterSamples=0;
static void ObserveOwnMeter() {
    if(!ownMeter) {
        IMMDeviceEnumerator *enumerator=nullptr;IMMDevice *endpoint=nullptr;
        IAudioSessionManager2 *manager=nullptr;IAudioSessionEnumerator *sessions=nullptr;
        if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void **>(&enumerator)))&&
           SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint))&&
           SUCCEEDED(endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void **>(&manager)))&&
           SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
            int count=0;sessions->GetCount(&count);
            for(int i=0;i<count&&!ownMeter;++i) {
                IAudioSessionControl *session=nullptr;IAudioSessionControl2 *details=nullptr;
                if(SUCCEEDED(sessions->GetSession(i,&session))&&SUCCEEDED(session->QueryInterface(__uuidof(IAudioSessionControl2),reinterpret_cast<void **>(&details)))) {
                    DWORD process=0;
                    if(SUCCEEDED(details->GetProcessId(&process))&&process==GetCurrentProcessId())
                        session->QueryInterface(__uuidof(IAudioMeterInformation),reinterpret_cast<void **>(&ownMeter));
                }
                if(details)details->Release();if(session)session->Release();
            }
        }
        if(sessions)sessions->Release();if(manager)manager->Release();if(endpoint)endpoint->Release();if(enumerator)enumerator->Release();
    }
    float peak=0;
    if(ownMeter&&SUCCEEDED(ownMeter->GetPeakValue(&peak))){maximumOwnPeak=std::max(maximumOwnPeak,peak);++ownMeterSamples;}
}
static HRESULT HardwareResult(HRESULT result){++hardwareCalls;if(FAILED(result))++hardwareErrors;return result;}

struct Buffer:IDirectSoundBuffer {
    std::vector<unsigned char> data;
    WAVEFORMATEX format{};
    DWORD base=0,started=0;
    bool playing=false;
    IDirectSoundBuffer *hardware=nullptr;
    explicit Buffer(const DSBUFFERDESC *desc,IDirectSoundBuffer *real=nullptr):data(desc->dwBufferBytes),hardware(real) { if(desc->lpwfxFormat)format=*desc->lpwfxFormat; }
    HRESULT __stdcall QueryInterface(REFIID,void **out) override { *out=nullptr;return E_NOINTERFACE; }
    ULONG __stdcall AddRef() override { return 1; }
    ULONG __stdcall Release() override { if(hardware)hardware->Release();delete this;return 0; }
    HRESULT __stdcall GetCaps(DSBCAPS *caps) override { caps->dwBufferBytes=(DWORD)data.size();return DS_OK; }
    HRESULT __stdcall GetCurrentPosition(DWORD *play,DWORD *write) override {
        if(hardware){const HRESULT result=HardwareResult(hardware->GetCurrentPosition(play,write));if(play)maximumCursor=std::max(maximumCursor,*play);return result;}
        DWORD position=base;
        if(playing)position+=(DWORD)((std::uint64_t(GetTickCount()-started)*format.nAvgBytesPerSec)/1000);
        if(!data.empty())position%=DWORD(data.size());
        position-=position%std::max<WORD>(1,format.nBlockAlign);
        if(play)*play=position;if(write)*write=position;return DS_OK;
    }
    HRESULT __stdcall GetFormat(WAVEFORMATEX *out,DWORD bytes,DWORD *written) override { if(out&&bytes>=sizeof(format))*out=format;if(written)*written=sizeof(format);return DS_OK; }
    HRESULT __stdcall GetVolume(LONG *v) override {*v=0;return DS_OK;}
    HRESULT __stdcall GetPan(LONG *v) override {*v=0;return DS_OK;}
    HRESULT __stdcall GetFrequency(DWORD *v) override {*v=format.nSamplesPerSec;return DS_OK;}
    HRESULT __stdcall GetStatus(DWORD *v) override {*v=playing?DSBSTATUS_PLAYING:0;return DS_OK;}
    HRESULT __stdcall Initialize(IDirectSound *,const DSBUFFERDESC *) override {return DS_OK;}
    HRESULT __stdcall Lock(DWORD offset,DWORD bytes,void **p1,DWORD *n1,void **p2,DWORD *n2,DWORD flags) override {
        if(hardware)return HardwareResult(hardware->Lock(offset,bytes,p1,n1,p2,n2,flags));
        if(flags&DSBLOCK_ENTIREBUFFER){offset=0;bytes=(DWORD)data.size();}
        if(offset>=data.size()||bytes>data.size())return DSERR_INVALIDPARAM;
        *n1=std::min<DWORD>(bytes,DWORD(data.size())-offset);*p1=data.data()+offset;
        *n2=bytes-*n1;*p2=*n2?data.data():nullptr;return DS_OK;
    }
    HRESULT __stdcall Play(DWORD a,DWORD b,DWORD c) override {if(!playing){started=GetTickCount();playing=true;}++plays;return hardware?HardwareResult(hardware->Play(a,b,c)):DS_OK;}
    HRESULT __stdcall SetCurrentPosition(DWORD value) override {base=value;started=GetTickCount();return hardware?HardwareResult(hardware->SetCurrentPosition(value)):DS_OK;}
    HRESULT __stdcall SetFormat(const WAVEFORMATEX *value) override {format=*value;return hardware?HardwareResult(hardware->SetFormat(value)):DS_OK;}
    HRESULT __stdcall SetVolume(LONG value) override {if(volumes.empty()||volumes.back()!=value)volumes.push_back(value);return hardware?HardwareResult(hardware->SetVolume(std::min<LONG>(value,-3000))):DS_OK;}
    HRESULT __stdcall SetPan(LONG value) override {return hardware?HardwareResult(hardware->SetPan(value)):DS_OK;}
    HRESULT __stdcall SetFrequency(DWORD value) override {format.nSamplesPerSec=value;return hardware?HardwareResult(hardware->SetFrequency(value)):DS_OK;}
    HRESULT __stdcall Stop() override {GetCurrentPosition(&base,nullptr);playing=false;++stops;return hardware?HardwareResult(hardware->Stop()):DS_OK;}
    HRESULT __stdcall Unlock(void *a,DWORD b,void *c,DWORD d) override {return hardware?HardwareResult(hardware->Unlock(a,b,c,d)):DS_OK;}
    HRESULT __stdcall Restore() override {return hardware?HardwareResult(hardware->Restore()):DS_OK;}
};
struct Device:IDirectSound {
    HRESULT __stdcall QueryInterface(REFIID,void **out) override {*out=nullptr;return E_NOINTERFACE;}
    ULONG __stdcall AddRef() override {return 1;}
    ULONG __stdcall Release() override {return 1;}
    HRESULT __stdcall CreateSoundBuffer(const DSBUFFERDESC *desc,IDirectSoundBuffer **out,IUnknown *outer) override {IDirectSoundBuffer *real=nullptr;if(hardwareDevice){const HRESULT result=HardwareResult(hardwareDevice->CreateSoundBuffer(desc,&real,outer));if(FAILED(result))return result;}*out=new Buffer(desc,real);return DS_OK;}
    HRESULT __stdcall GetCaps(DSCAPS *caps) override {if(hardwareDevice)return HardwareResult(hardwareDevice->GetCaps(caps));const DWORD size=caps->dwSize;memset(caps,0,size);caps->dwSize=size;return DS_OK;}
    HRESULT __stdcall DuplicateSoundBuffer(IDirectSoundBuffer *,IDirectSoundBuffer **) override {return E_NOTIMPL;}
    HRESULT __stdcall SetCooperativeLevel(HWND,DWORD) override {return DS_OK;}
    HRESULT __stdcall Compact() override {return DS_OK;}
    HRESULT __stdcall GetSpeakerConfig(DWORD *out) override {*out=DSSPEAKER_STEREO;return DS_OK;}
    HRESULT __stdcall SetSpeakerConfig(DWORD) override {return DS_OK;}
    HRESULT __stdcall Initialize(const GUID *) override {return DS_OK;}
} device;

using Factory=void *(__stdcall *)(std::uint32_t);
using OpenSound=int (__stdcall *)(void *,unsigned,unsigned,unsigned,unsigned,unsigned);
using LockSound=int (__stdcall *)(void *,void **,unsigned *);
using UnlockSound=int (__stdcall *)(void *,unsigned);
static OpenSound nativeOpen;
static LockSound nativeLock;
static UnlockSound nativeUnlock;
static void *lockedPCM;
static unsigned lockedBytes;
static int __stdcall ObserveLock(void *sound,void **buffer,unsigned *bytes) {
    const int result=nativeLock(sound,buffer,bytes);
    if(result){lockedPCM=*buffer;lockedBytes=*bytes;}
    return result;
}
static int __stdcall ObserveUnlock(void *sound,unsigned bytes) {
    if(bytes>lockedBytes||!lockedPCM)std::abort();
    const auto *begin=static_cast<const unsigned char *>(lockedPCM);
    pcm.insert(pcm.end(),begin,begin+bytes);++unlocks;lockedPCM=nullptr;lockedBytes=0;
    return nativeUnlock(sound,bytes);
}
static int __stdcall ObserveOpen(void *sound,unsigned frequency,unsigned bits,unsigned channels,unsigned flags,unsigned user) {
    const int result=nativeOpen(sound,frequency,bits,channels,flags,user);
    if(result){
        auto **table=static_cast<void **>(sound);
        nativeLock=reinterpret_cast<LockSound>(table[1]);nativeUnlock=reinterpret_cast<UnlockSound>(table[2]);
        table[1]=reinterpret_cast<void *>(ObserveLock);table[2]=reinterpret_cast<void *>(ObserveUnlock);
        pcmFrequency=frequency;pcmBits=bits;pcmChannels=channels;
    }
    return result;
}
static Factory nativeFactory;
using OpenMovie=void *(__stdcall *)(const void *,unsigned);
static OpenMovie nativeMovieOpen;
static const char *fileInput=nullptr;
static void *__stdcall ObserveMovieOpen(const void *data,unsigned flags) {
    return nativeMovieOpen(fileInput?static_cast<const void *>(fileInput):data,
                           fileInput?flags&~0x04000000u:flags);
}
static void *__stdcall ObserveFactory(std::uint32_t) {
    nativeOpen=reinterpret_cast<OpenSound>(nativeFactory(reinterpret_cast<std::uint32_t>(&device)));
    return nativeOpen?reinterpret_cast<void *>(ObserveOpen):nullptr;
}
static void *Sys_LoadLibrary(const char *name){return LoadLibraryA(name);}
static void *Sys_LoadFunction(void *library,const char *name){
    void *fn=reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(library),name));
    if(!fn)return nullptr;
    if(!strcmp(name,"_BinkOpenDirectSound@4")){nativeFactory=reinterpret_cast<Factory>(fn);return reinterpret_cast<void *>(ObserveFactory);}
    if(!strcmp(name,"_BinkOpen@8")){nativeMovieOpen=reinterpret_cast<OpenMovie>(fn);return reinterpret_cast<void *>(ObserveMovieOpen);}
    return fn;
}
static void Sys_UnloadLibrary(void *library){FreeLibrary(static_cast<HMODULE>(library));}
#include "code/client/cinematic_bink.hpp"

int main(int argc,char **argv){
    if(argc!=5&&argc!=6)return 64;
    if(strcmp(argv[3],"en")&&strcmp(argv[3],"de"))return 64;
    if(argc==6&&strcmp(argv[5],"file")&&strcmp(argv[5],"pause")&&strcmp(argv[5],"device"))return 64;
    if(argc==6&&!strcmp(argv[5],"file"))fileInput=argv[2];
    HWND window=nullptr;
    if(argc==6&&!strcmp(argv[5],"device")){
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        window=CreateWindowExA(0,"STATIC","fnTech3 audio probe",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
        if(!window||FAILED(DirectSoundCreate(nullptr,&hardwareDevice,nullptr))||FAILED(hardwareDevice->SetCooperativeLevel(window,DSSCL_NORMAL)))return 5;
    }
    std::ifstream input(argv[2],std::ios::binary);
    if(!input)return 66;
    input.seekg(0,std::ios::end);const auto length=input.tellg();input.seekg(0);
    if(length<0||static_cast<std::uint64_t>(length)>fntech3::stvef::bink::MaxFileBytes)return 65;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if(!input.read(reinterpret_cast<char *>(bytes.data()),length))return 66;
    fntech3::stvef::bink::Movie movie;
    if(!movie.Open(std::move(bytes),argv[1],!strcmp(argv[3],"de"),true)){std::cerr<<movie.Error();return 1;}
    const auto info=movie.Info();const bool sound=movie.Sound();
    movie.Volume(1);
    const DWORD start=GetTickCount();unsigned frames=0;bool paused=false;
    for(;;){
        const DWORD now=GetTickCount();
        const auto result=movie.Step(now);
        if(hardwareDevice)ObserveOwnMeter();
        if(result==fntech3::stvef::bink::Movie::Result::End)break;
        if(result==fntech3::stvef::bink::Movie::Result::Frame)++frames;
        if(argc==6&&!strcmp(argv[5],"pause")&&!paused&&frames==1){
            movie.Volume(0);movie.Pause(true,now);Sleep(250);
            if(movie.Step(GetTickCount())!=fntech3::stvef::bink::Movie::Result::Waiting)return 4;
            movie.Pause(false,GetTickCount());movie.Volume(1);paused=true;
        }
        if(now-start>120000)return 2;
        Sleep(1);
    }
    movie.Volume(0);movie.Volume(.5f);movie.Volume(1);
    movie.Close();
    std::ofstream output(argv[4],std::ios::binary);output.write(reinterpret_cast<const char *>(pcm.data()),pcm.size());
    if(!output)return 73;
    std::uint64_t hash=1469598103934665603ull;int peak=0;double squared=0;unsigned nonzero=0;
    for(auto byte:pcm)hash=(hash^byte)*1099511628211ull;
    if(pcmBits==16)for(size_t i=0;i+1<pcm.size();i+=2){int value=static_cast<std::int16_t>(pcm[i]|(pcm[i+1]<<8));peak=std::max(peak,std::abs(value));squared+=double(value)*value;nonzero+=value!=0;}
    std::cout<<"{\"sound\":"<<sound<<",\"frames\":"<<frames<<",\"expectedFrames\":"<<info.frames<<",\"track\":"<<info.soundTrack
      <<",\"frequency\":"<<pcmFrequency<<",\"bits\":"<<pcmBits<<",\"channels\":"<<pcmChannels
      <<",\"bytes\":"<<pcm.size()<<",\"hash\":\""<<hash<<"\",\"unlocks\":"<<unlocks<<",\"plays\":"<<plays<<",\"stops\":"<<stops
      <<",\"peak\":"<<peak<<",\"nonzero\":"<<nonzero<<",\"rms\":"<<(pcm.empty()?0:sqrt(squared/(pcm.size()/2)))<<",\"volumes\":[";
    for(size_t i=0;i<volumes.size();++i)std::cout<<(i?",":"")<<volumes[i];
    std::cout<<"],\"hardwareCalls\":"<<hardwareCalls<<",\"hardwareErrors\":"<<hardwareErrors<<",\"maximumCursor\":"<<maximumCursor
      <<",\"ownMeterSamples\":"<<ownMeterSamples<<",\"maximumOwnPeak\":"<<maximumOwnPeak<<"}\n";
    if(ownMeter)ownMeter->Release();if(hardwareDevice)hardwareDevice->Release();if(window){DestroyWindow(window);CoUninitialize();}
    return sound&&frames==info.frames&&!hardwareErrors?0:3;
}
