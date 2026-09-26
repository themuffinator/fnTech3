/* Owned recording runtime for force-feedback boundary tests; no device API. */
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <charconv>

static void *Sys_LoadLibrary( const char * );
static void *Sys_LoadFunction( void *, const char * );
static void Sys_UnloadLibrary( void * );

// Test the actual x86 register/stack convention on both host toolchains.
#if defined(__i386__) && !defined(_WIN32)
#define __thiscall __attribute__((thiscall))
#define __fastcall __attribute__((fastcall))
#endif
#if defined(_M_IX86) || defined(__i386__)
#define CALLBACK __fastcall
#define UNUSED , void *
#else
#define CALLBACK
#define UNUSED
#endif
#ifndef _WIN32
#define _WIN32
#endif
#ifndef _M_IX86
#define _M_IX86
#endif
#include "code/client/stvef_force_feedback_native.hpp"
using namespace fntech3::stvef::feedback;

struct Object { int kind; bool playing = false; Guid guid{}; };
struct Recorded { int kind; Guid guid{}; std::vector<std::uint32_t> args, envelope; };
static std::map<void *, Object> objects;
static std::vector<Recorded> recorded;
static std::vector<std::string> exports;
static std::string expectedPath = "installation/FFC10.dll", missing;
static int loads, unloads, creates, destroys, starts, stops, initializations, springStarts, springStops, changes;
static int mouseInits, joyInits, mouseServices, lastSpring, failInitAt;
static bool loaded, failLoad, mouseSuccess, joySuccess = true, springSuccess = true;
static bool failStart, failStop, failStatus, failSpringStart, failSpringStop, failChange;
static void *instance = reinterpret_cast<void *>(0x111), *window = reinterpret_cast<void *>(0x222);

static Object &Find( void *p ) { assert(loaded && objects.count(p)); return objects.at(p); }
static void *Add( void *p, int kind, unsigned bytes ) {
	assert(loaded && !objects.count(p));
	memset(p, 0xc7, bytes); objects.emplace(p, Object{kind});
	if(kind <= 2) ++creates;
	return p;
}
static void Remove( void *p, int kind ) {
	assert(Find(p).kind == kind);
	if(kind == 10 || kind == 11) assert(objects.size() == 1); // Children release before their device.
	if(kind <= 2) ++destroys;
	objects.erase(p);
}
static void *CALLBACK MouseCtor( void *p UNUSED ) { return Add(p, 10, 40); }
static void *CALLBACK JoyCtor( void *p UNUSED ) { return Add(p, 11, 48); }
static void *CALLBACK SpringCtor( void *p UNUSED ) { return Add(p, 12, 188); }
static void *CALLBACK ConstantCtor( void *p UNUSED ) { return Add(p, 1, 132); }
static void *CALLBACK PeriodicCtor( void *p UNUSED, const Guid &guid ) { Add(p, 2, 144); Find(p).guid=guid; return p; }
static void CALLBACK MouseDtor( void *p UNUSED ) { Remove(p, 10); }
static void CALLBACK JoyDtor( void *p UNUSED ) { Remove(p, 11); }
static void CALLBACK SpringDtor( void *p UNUSED ) { Remove(p, 12); }
static void CALLBACK ConstantDtor( void *p UNUSED ) { Remove(p, 1); }
static void CALLBACK PeriodicDtor( void *p UNUSED ) { Remove(p, 2); }
static int CALLBACK MouseInit( void *p UNUSED, void *app, void *wnd, std::uint32_t flags ) {
	assert(Find(p).kind==10 && app==instance && wnd==window && flags==4); ++mouseInits; return mouseSuccess;
}
static int CALLBACK JoyInit( void *p UNUSED, void *app, void *wnd, void *api, void *device ) {
	assert(Find(p).kind==11 && app==instance && wnd==window && !api && !device); ++joyInits; return joySuccess;
}
static int CALLBACK MouseServices( void *p UNUSED, int enabled ) {
	assert(Find(p).kind==10 && !enabled); ++mouseServices; return 1;
}
static int Record( void *p, void *device, std::vector<std::uint32_t> args, const Envelope *envelope ) {
	const auto &object=Find(p); assert(Find(device).kind >= 10 && Find(device).kind <= 11);
	Recorded row{object.kind, object.guid, args, {}};
	if(envelope) { assert(envelope->bytes==20); row.envelope={20,envelope->attackLevel,envelope->attackTime,envelope->fadeLevel,envelope->fadeTime}; }
	recorded.push_back(row);
	return ++initializations != failInitAt;
}
static int CALLBACK ConstantInit( void *p UNUSED, void *device, std::int32_t angle,
	std::uint32_t duration, std::int32_t magnitude, Envelope *envelope ) {
	assert(Find(p).kind==1); return Record(p,device,{std::uint32_t(angle),duration,std::uint32_t(magnitude)},envelope);
}
static int CALLBACK PeriodicInit( void *p UNUSED, void *device, std::uint32_t magnitude,
	std::uint32_t period, std::uint32_t duration, std::int32_t angle, std::int32_t offset,
	std::uint32_t phase, Envelope *envelope ) {
	assert(Find(p).kind==2); return Record(p,device,{magnitude,period,duration,std::uint32_t(angle),std::uint32_t(offset),phase},envelope);
}
static int CALLBACK Start( void *p UNUSED, std::uint32_t iterations, std::uint32_t flags ) {
	assert(Find(p).kind<=2 && iterations==1 && !flags); ++starts;
	if(failStart)return 0; Find(p).playing=true; return 1;
}
static int CALLBACK Stop( void *p UNUSED ) {
	if(Find(p).kind==12) { ++springStops; if(failSpringStop)return 0; }
	else { assert(Find(p).kind<=2); ++stops; if(failStop)return 0; }
	Find(p).playing=false; return 1;
}
static int CALLBACK Status( void *p UNUSED, std::uint32_t *flags ) {
	assert(Find(p).kind<=2 && flags); *flags=failStatus ? 1 : Find(p).playing; return !failStatus;
}
static int CALLBACK SpringInit( void *p UNUSED, void *device, std::int32_t coefficient,
	std::uint32_t saturation, std::uint32_t deadband, std::uint32_t axes, Point center,
	std::int32_t x, std::int32_t y, int position ) {
	assert(Find(p).kind==12 && Find(device).kind==11 && coefficient==2000 && saturation==10000 && deadband==1000);
	assert(axes==3 && center.x==0x7fffffff && center.y==0x7fffffff && x==1 && y==1 && position==1);
	return springSuccess;
}
static int CALLBACK SpringStart( void *p UNUSED, std::uint32_t iterations, std::uint32_t flags ) {
	assert(Find(p).kind==12 && iterations==1 && !flags); ++springStarts;
	if(failSpringStart)return 0; Find(p).playing=true; return 1;
}
static int CALLBACK SpringChange( void *p UNUSED, Point point, std::int32_t coefficient,
	std::uint32_t saturation, std::uint32_t deadband, std::int32_t x, std::int32_t y ) {
	assert(Find(p).kind==12 && !point.x && !point.y && saturation==0x80000000u && deadband==0x80000000u);
	assert(x==std::numeric_limits<std::int32_t>::min() && y==x); ++changes; lastSpring=coefficient; return !failChange;
}
static void *Sys_LoadLibrary( const char *path ) {
	assert(!loaded && objects.empty() && path==expectedPath); ++loads; exports.clear();
	if(failLoad)return nullptr; loaded=true; return reinterpret_cast<void *>(1);
}
static void Sys_UnloadLibrary( void *library ) {
	assert(library==reinterpret_cast<void *>(1) && loaded && objects.empty()); ++unloads; loaded=false;
}
static void *Sys_LoadFunction( void *library, const char *name ) {
	assert(library==reinterpret_cast<void *>(1) && loaded); exports.push_back(name);
	if(missing==name)return nullptr;
#define EXPORT(symbol, function) if(!strcmp(name,symbol))return reinterpret_cast<void *>(function)
	EXPORT("??0CFeelMouse@@QAE@XZ",MouseCtor);
	EXPORT("??1CFeelMouse@@UAE@XZ",MouseDtor);
	EXPORT("?Initialize@CFeelMouse@@QAEHPAX0K@Z",MouseInit);
	EXPORT("?UsesWin32MouseServices@CFeelDevice@@QAEHH@Z",MouseServices);
	EXPORT("??0CFeelDXDevice@@QAE@XZ",JoyCtor);
	EXPORT("??1CFeelDXDevice@@UAE@XZ",JoyDtor);
	EXPORT("?Initialize@CFeelDXDevice@@QAEHPAX0PAUIDirectInputA@@PAUIDirectInputDevice2A@@@Z",JoyInit);
	EXPORT("??0CFeelConstant@@QAE@XZ",ConstantCtor);
	EXPORT("??1CFeelConstant@@UAE@XZ",ConstantDtor);
	EXPORT("?InitializePolar@CFeelConstant@@UAEHPAVCFeelDevice@@JKJPAUFEELIT_ENVELOPE@@@Z",ConstantInit);
	EXPORT("??0CFeelPeriodic@@QAE@ABU_GUID@@@Z",PeriodicCtor);
	EXPORT("??1CFeelPeriodic@@UAE@XZ",PeriodicDtor);
	EXPORT("?InitializePolar@CFeelPeriodic@@UAEHPAVCFeelDevice@@KKKJJKPAUFEELIT_ENVELOPE@@@Z",PeriodicInit);
	EXPORT("?Start@CFeelEffect@@UAEHKK@Z",Start);
	EXPORT("?Stop@CFeelEffect@@UAEHXZ",Stop);
	EXPORT("?GetStatus@CFeelEffect@@QAEHPAK@Z",Status);
	EXPORT("??0CFeelSpring@@QAE@XZ",SpringCtor);
	EXPORT("??1CFeelSpring@@UAE@XZ",SpringDtor);
	EXPORT("?Initialize@CFeelSpring@@UAEHPAVCFeelDevice@@JKKKUtagPOINT@@JJH@Z",SpringInit);
	EXPORT("?Start@CFeelCondition@@UAEHKK@Z",SpringStart);
	EXPORT("?ChangeParameters@CFeelSpring@@QAEHUtagPOINT@@JKKJJ@Z",SpringChange);
#undef EXPORT
	assert(false); return nullptr;
}
