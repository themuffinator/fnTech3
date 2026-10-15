/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_EFFECTS_HPP
#define FNTECH3_CLIENT_JKA_CGAME_EFFECTS_HPP
#include "../compat/jka_effects.hpp"
#include "../compat/jka_client_abi.h"
#include "../compat/jka_effect_play.hpp"
#include "../compat/jka_render_adapter.hpp"
#include "client_jka_sound.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>

/* Cgame import 263 registers an owned declaration and actual Fn media handles.
 * Context/pool lifecycle and its frame clock are served. Shader-only no-motion
 * particles, cylinders, lines and electricity enter Fn sprites/polygons; lights
 * enter Fn's dynamic lights; emitters move, trace and emit nested effects, and
 * FxRunners play them, inside the owned pool. Particles move, rotate and run
 * their size, colour and alpha curves; delayed spawns wait in an owned
 * schedule created before each normal pass. Tails draw as Fn polygons from
 * their motion; traced particles bounce, play impact effects and die on
 * impact, and death effects play as particles end. Sound primitives start
 * their sound at the spawn origin through Fn's spatial audio (S_StartJKASound,
 * as the cgame's own S_StartSound import does). CameraShake primitives hand
 * their record to the client game's export 31, whose own view shake Fn then
 * renders. Effects played on an entity (import 268) play at the caller's own
 * axis; bolted ones refuse by name. Remaining playback controls and
 * primitive families remain named refusals. Registration owns all buffers and
 * partial records across engine ERR_DROP/longjmp recovery. */
namespace {
struct JKACgameEffect {
    fntech3::jka::effects::Name name;
    fntech3::jka::effects::Definition definition;
    std::vector<char> bytes;
    fileHandle_t file{};
    bool ready{};
    std::vector<fntech3::jka::effectplay::Spec> playback;
    bool playable{};
};
struct JKACgameEffectInstance {
    int definition{}, startTime{};
    fntech3::jka::effectplay::Spawn spawn;
    bool active{};
};
// A delayed spawn: retail's scheduled record (0x40755d) keeps the template,
// due time, origin and axis; CreateEffect runs when it falls due.
struct JKACgameEffectScheduled {
    int due{}, definition{}, depth{};
    std::size_t primitive{};
    fntech3::jka::effectplay::Vector origin{};
    fntech3::jka::effectplay::Axis axis{};
};
struct JKACgameEffectDraw {
    refEntity_t entity{};
    int shader{};
    std::vector<polyVert_t> vertices;
    bool light{};
    float radius{};
    std::array<float,3> color{};
};
// Fn bound on FxRunner/emitter nesting; retail recurses without one.
constexpr int jkaCgameEffectNesting=8;
// Fn bound on waiting delayed spawns; retail allocates each without one.
constexpr std::size_t jkaCgameEffectScheduleBound=8192;
struct JKACgameEffectsState {
    // Retail's init pool has 1,800 records; the engine owns its own shape.
    std::array<JKACgameEffectInstance,1800> instances{};
    intptr_t view{};
    int time{}, previousTime{}, frameTime{};
    float realTime{};
    std::size_t nextFree{};
    bool initialized{}, frozen{};
    std::uint32_t random{0x4a4b4101u};
    std::vector<std::pair<int,fntech3::jka::effectplay::Spawn>> spawnWork;
    std::vector<JKACgameEffectScheduled> scheduled,dueWork;
    std::size_t scheduleDropped{};
    std::vector<std::size_t> slotWork,evictWork;
    std::vector<JKACgameEffectDraw> drawWork;
    // Death effects of the instances retired this pass: effect and origin.
    std::vector<std::pair<int,fntech3::jka::effectplay::Vector>> deathWork;
    cvar_t *debug{}, *countScale{}, *nearCull{}, *rmg{};
    fntech3::jka::effects::Name pendingName;
    std::vector<std::unique_ptr<JKACgameEffect>> effects;
    std::map<std::string,int> lookup;
};
std::unique_ptr<JKACgameEffectsState> jkaCgameEffects;
std::uint64_t jkaCgameEffectsRevision{};
bool jkaCgameEffectsTracing{};
static void CL_JKACgameEffectClose(JKACgameEffect &effect) {
    const fileHandle_t file=effect.file;
    effect.file=0;
    if (file) FS_FCloseFile(file);
}
static void CL_JKACgameEffectsClear() {
    ++jkaCgameEffectsRevision;jkaCgameEffectsTracing=false;
    if (!jkaCgameEffects) return;
    for (const auto &effect:jkaCgameEffects->effects) CL_JKACgameEffectClose(*effect);
    jkaCgameEffects.reset();
}
static JKACgameEffectsState &CL_JKACgameEffectsState() {
    if (!jkaCgameEffects) jkaCgameEffects=std::make_unique<JKACgameEffectsState>();
    return *jkaCgameEffects;
}
static intptr_t CL_JKACgameEffectsView(intptr_t address) {
    if (address) CL_JKACgameSpan(address,sizeof(fntech3_jka_mp_refdef_t));
    return address; // Nullable borrowed view; its contents are not read by init.
}
static void CL_JKACgameEffectsInit(intptr_t view) {
    view=CL_JKACgameEffectsView(view);
    ++jkaCgameEffectsRevision;
    auto &state=CL_JKACgameEffectsState();
    if (!state.initialized) {state.instances={};state.scheduled.clear();}
    state.nextFree=0;
    state.debug=Cvar_Get("fx_debug","0",CVAR_TEMP);
    state.countScale=Cvar_Get("fx_countScale","1",CVAR_ARCHIVE);
    state.nearCull=Cvar_Get("fx_nearCull","16",CVAR_ARCHIVE);
    // Retail's engine registers RMG (0x4386e7) and its particles read it
    // (0x401afc); Fn registers the same name with retail's default.
    state.rmg=Cvar_Get("RMG","0",0);
    state.time=state.previousTime=state.frameTime=0;
    state.realTime=0.0f;
    state.frozen=false;
    state.view=view;
    state.initialized=true;
    Com_Printf("Jedi Academy cgame effects context initialized\n");
}
static void CL_JKACgameEffectsAdjustTime(intptr_t argument) {
    if(argument<INT_MIN || argument>INT_MAX)
        Com_Error(ERR_DROP,"Invalid Academy effects clock word");
    ++jkaCgameEffectsRevision;
    auto &state=CL_JKACgameEffectsState();
    const int time=static_cast<int>(argument);
    // Retail0x4010c0 pauses for a nonpositive input without replacing either
    // clock. Positive input is the new absolute time, not an elapsed step.
    if(time<=0){state.frameTime=0;state.realTime=0.0f;return;}
    const uint32_t delta=static_cast<uint32_t>(time)-static_cast<uint32_t>(state.time);
    state.previousTime=state.time;
    state.time=time;
    static_assert(sizeof(state.frameTime)==sizeof(delta));
    std::memcpy(&state.frameTime,&delta,sizeof(delta)); // Defined x86 word wrap.
    // Retail rounds the integer delta to float before multiplying by the
    // float at0x5660dc (0.001f); preserve that rounding on x87 builds too.
    volatile float milliseconds=static_cast<float>(state.frameTime);
    volatile float secondsPerMillisecond=0.001f;
    state.realTime=milliseconds*secondsPerMillisecond;
}
static void CL_JKACgameEffectsFree() {
    if (!jkaCgameEffects) return;
    ++jkaCgameEffectsRevision;
    auto &state=*jkaCgameEffects;
    for (const auto &effect:state.effects) CL_JKACgameEffectClose(*effect);
    state.effects.clear();state.lookup.clear();state.pendingName={};
    state.instances={};state.nextFree=0;
    state.spawnWork.clear();state.slotWork.clear();state.evictWork.clear();state.drawWork.clear();state.deathWork.clear();
    state.scheduled.clear();state.dueWork.clear();state.scheduleDropped=0;
    // Retail Free(true) keeps the helper view/clocks and initialized flag.
}
static intptr_t CL_JKACgameEffectsRegister(const char *name) {
    using namespace fntech3::jka::effects;
    if (!jkaCgameEffects) jkaCgameEffects=std::make_unique<JKACgameEffectsState>();
    auto &state=*jkaCgameEffects;
    if (!ResolveName(name,state.pendingName))
        Com_Error(ERR_DROP,"Invalid Jedi Academy cgame effect path");
    const auto found=state.lookup.find(state.pendingName.key);
    // Retail publishes the cache identity before recursive resource registration.
    // A dependency cycle refers to the same owned record; only the outer call
    // can return its completed graph to the module.
    if (found!=state.lookup.end()) return found->second;
    if (state.effects.size()>=MaxEffects-1) {
        Com_Printf("Jedi Academy cgame reached the effect registration bound\n");return 0;
    }
    state.effects.push_back(std::make_unique<JKACgameEffect>());
    auto &effect=*state.effects.back();
    effect.name=std::move(state.pendingName);
    const int identity=static_cast<int>(state.effects.size());
    const int length=FS_FOpenFileRead(effect.name.file.c_str(),&effect.file,qfalse);
    if (length<=0 || !effect.file || std::size_t(length)>MaxTextBytes) {
        CL_JKACgameEffectClose(effect);
        Com_Printf("Jedi Academy cgame effect file missing, empty or outside the engine bound: %s\n",effect.name.file.c_str());
        state.effects.pop_back();return 0;
    }
    effect.bytes.resize(std::size_t(length));
    const int read=FS_Read(effect.bytes.data(),length,effect.file);
    CL_JKACgameEffectClose(effect);
    if (read!=length)
        Com_Error(ERR_DROP,"Incomplete Jedi Academy cgame effect read: %s",effect.name.file.c_str());
    Failure failure;
    const bool parsed=Parse(std::string_view(effect.bytes.data(),effect.bytes.size()),effect.definition,failure);
    effect.bytes.clear();
    if (!parsed)
        Com_Error(ERR_DROP,"Jedi Academy cgame effect %s line %u: %s",effect.name.file.c_str(),
            static_cast<unsigned>(failure.line),failure.reason);
    state.lookup.emplace(effect.name.key,identity);
    for (auto &primitive:effect.definition.primitives) {
        for (auto &media:primitive.media) {
            switch (media.kind) {
            case MediaKind::Shader:media.handle=re.RegisterShader(media.name.c_str());break;
            case MediaKind::Model:media.handle=re.RegisterModel(media.name.c_str());break;
            case MediaKind::Sound:media.handle=S_RegisterSound(media.name.c_str(),qfalse);break;
            default:media.handle=static_cast<int>(CL_JKACgameEffectsRegister(media.name.c_str()));break;
            }
        }
    }
    effect.ready=true;
    Com_Printf("Jedi Academy cgame effect registered: %s (%u primitives)\n",effect.name.file.c_str(),
        static_cast<unsigned>(effect.definition.primitives.size()));
    return identity;
}
static void CL_JKACgameEffectNative() {
    if(sizeof(void*)!=4||!cgvm||!cgvm->entryPoint||cgvm->dllExports||cgvm->index!=VM_CGAME)
        Com_Error(ERR_DROP,"Academy effects playback requires its x86 legacy native cgame");
}
static fntech3_jka_trace_t CL_JKACgameEffectTrace(const fntech3::jka::effectplay::Vector &start,
    const fntech3::jka::effectplay::Vector &end)
{
    using namespace fntech3::jka;
    // Retail0x40931f (endpoints) and 0x401130 (emitter physics) publish 104
    // bytes to the registered scratch buffer, then export15 traces against
    // the client's world and entity snapshot.
    const auto *owner=jkaCgameEffects.get();
    const auto revision=jkaCgameEffectsRevision;
    auto *const module=cgvm;
    void *const shared=jkaCgameSharedBuffer;
    void *const destination=CL_JKACgameSpan(reinterpret_cast<intptr_t>(shared),sizeof(fntech3_jka_cgame_trace_t));
    fntech3_jka_cgame_trace_t request{};
    std::memcpy(request.start,start.data(),sizeof(request.start));
    std::memcpy(request.end,end.data(),sizeof(request.end));
    request.skipEntity=-1;request.mask=0x1001; // Academy SOLID|TERRAIN.
    std::memcpy(destination,&request,sizeof(request));
    jkaCgameEffectsTracing=true;
    VM_Call(module,0,CG_RAVEN_TRACE);
    jkaCgameEffectsTracing=false;
    if(cgvm!=module||jkaCgameSharedBuffer!=shared||jkaCgameEffects.get()!=owner||jkaCgameEffectsRevision!=revision)
        Com_Error(ERR_DROP,"Academy effects trace changed its owning context");
    std::memcpy(&request,destination,sizeof(request));
    if(!effectplay::Finite(request.result.fraction)||request.result.fraction<0||request.result.fraction>1)
        Com_Error(ERR_DROP,"Invalid Academy effects trace fraction");
    for(int n=0;n<3;++n){
        const float value=request.result.endpos[n];
        if(!effectplay::Finite(value)||std::fabs(value)>10000000.0f)
            Com_Error(ERR_DROP,"Invalid Academy effects trace endpoint");
    }
    return request.result;
}
// CreateEffect's CameraShake case (0x40b2ca..0x40b389) hands 0x40c560 the
// spawn origin, the drawn intensity, and the truncated radius and life; it
// publishes them to the registered scratch buffer and calls cgame export 31.
// The retail client game owns the rest (0x3004281c, 0x30062c70): no shake
// beyond the radius, a linear falloff to it, its clamp of 16, and a shake of
// its own view origin and angles that decays over the life (0x30062990).
// That view reaches Fn's renderer, listener and screenshots through the
// module's own RE_RenderScene and S_Respatialize imports. cl_viewShake scales
// the intensity handed over; the call itself is kept at every scale, so the
// module's own shake state and random draws follow retail's sequence.
static void CL_JKACgameEffectCameraShake(const fntech3::jka::effectplay::Spawn &spawn) {
    const auto *owner=jkaCgameEffects.get();
    const auto revision=jkaCgameEffectsRevision;
    auto *const module=cgvm;
    void *const shared=jkaCgameSharedBuffer;
    void *const destination=CL_JKACgameSpan(reinterpret_cast<intptr_t>(shared),sizeof(fntech3_jka_cgame_camera_shake_t));
    fntech3_jka_cgame_camera_shake_t record{};
    std::memcpy(record.origin,spawn.origin.data(),sizeof(record.origin));
    record.intensity=spawn.shakeIntensity*CL_ViewShakeScale();record.radius=spawn.shakeRadius;record.time=spawn.life;
    std::memcpy(destination,&record,sizeof(record));
    jkaCgameEffectsTracing=true;
    VM_Call(module,0,CG_RAVEN_CAMERA_SHAKE);
    jkaCgameEffectsTracing=false;
    if(cgvm!=module||jkaCgameSharedBuffer!=shared||jkaCgameEffects.get()!=owner||jkaCgameEffectsRevision!=revision)
        Com_Error(ERR_DROP,"Academy effects camera shake changed its owning context");
}
static fntech3_jka_trace_t CL_JKACgameEffectTraceEndpoint(fntech3::jka::effectplay::Spawn &spawn) {
    const auto result=CL_JKACgameEffectTrace(spawn.origin,spawn.endpoint);
    for(int n=0;n<3;++n)spawn.endpoint[n]=result.endpos[n];
    return result;
}
// Retail's primitive pass (0x40f34c..0x40f371) kills a primitive past its
// end with impactKills cleared, so Die (0x401500) plays its death effect;
// the caller plays the collected deaths.
static void CL_JKACgameEffectRetire(JKACgameEffectsState &state) {
    for(const auto &instance:state.instances)if(instance.active){
        const std::int64_t age=std::int64_t(state.time)-instance.startTime;
        if(age<0||age>INT32_MAX)Com_Error(ERR_DROP,"Academy effects clock reversal or wrap is not served yet");
    }
    for(auto &instance:state.instances)if(instance.active&&std::int64_t(state.time)-instance.startTime>instance.spawn.life){
        const auto &spawn=instance.spawn;
        if(fntech3::jka::effectplay::Moving(spawn.kind)&&(spawn.flags&fntech3::jka::effectplay::flag::DeathFx)&&spawn.death)
            state.deathWork.push_back({spawn.death,spawn.origin});
        instance={};
    }
}
// Compile an effect and every effect it can play before any callback or
// publication, so an unserved descendant refuses the whole graph honestly.
static void CL_JKACgameEffectCompileGraph(JKACgameEffectsState &state,int root) {
    using namespace fntech3::jka;
    std::vector<int> pending{root};std::vector<bool> seen(state.effects.size()+1);
    while(!pending.empty()){
        const int id=pending.back();pending.pop_back();
        if(id<1||std::size_t(id)>state.effects.size()||seen[id])continue;
        seen[id]=true;
        auto &effect=*state.effects[id-1];
        if(!effect.ready)continue;
        if(!effect.playable){
            effect.playback.clear();effect.playback.reserve(effect.definition.primitives.size());
            // Retail's file parser (0x406a51..0x406c0f) reads past any group
            // that is not a primitive, such as "forcefeedback", silently.
            for(const auto &primitive:effect.definition.primitives){
                effectplay::Spec spec;effectplay::Failure failure;
                if(!effectplay::Compile(primitive,spec,failure))
                    Com_Error(ERR_DROP,"Academy effect %s: %s (%s)",effect.name.file.c_str(),failure.reason,failure.field);
                // Retail prints each key or group it does not know and reads on.
                for(const auto &ignored:spec.ignored)
                    Com_Printf("Academy effect %s: unknown key %s ignored, as retail does\n",effect.name.file.c_str(),ignored.c_str());
                // Retail's flag parsers skip a word they do not know without
                // a message (0x40d79a); Fn names it under fx_debug.
                if(state.debug&&state.debug->integer)for(const auto &word:spec.unknownWords)
                    Com_Printf("Academy effect %s: unknown flag word %s read past, as retail does\n",effect.name.file.c_str(),word.c_str());
                effect.playback.push_back(std::move(spec));
            }
            effect.playable=true;
        }
        for(const auto &spec:effect.playback){
            for(int child:spec.children)pending.push_back(child);
            for(int child:spec.impacts)pending.push_back(child);
            for(int child:spec.deaths)pending.push_back(child);
        }
    }
}
static float CL_JKACgameEffectCountScale(const JKACgameEffectsState &state) {
    const float scale=state.countScale?state.countScale->value:1.0f;
    if(!fntech3::jka::effectplay::Finite(scale))Com_Error(ERR_DROP,"Invalid Academy effects count scale");
    return scale;
}
static void CL_JKACgameEffectSpawn(JKACgameEffectsState &state,int id,const fntech3::jka::effectplay::Vector &origin,
    const fntech3::jka::effectplay::Axis &axis,std::uint32_t &random,int depth);
// Retail CreateEffect 0x407c40 for one primitive, late by the given lag. An
// FxRunner (0x40b11e) plays one of its effects at once, at its origin and axis.
static void CL_JKACgameEffectCreate(JKACgameEffectsState &state,int id,std::size_t primitive,
    const fntech3::jka::effectplay::Vector &origin,const fntech3::jka::effectplay::Axis &axis,
    std::uint32_t &random,int depth,int late)
{
    using namespace fntech3::jka;
    const auto &spec=state.effects[id-1]->playback[primitive];
    effectplay::Spawn spawn;
    if(!effectplay::Instantiate(spec,origin,axis,random,spawn,late))Com_Error(ERR_DROP,"Invalid Academy effects spawn result");
    if(spawn.traceEnd){
        const auto trace=CL_JKACgameEffectTraceEndpoint(spawn);
        // traceImpactFx (0x4093d9..0x409493) plays the impact effect where
        // the trace ends, facing its plane, hit or not.
        if(spawn.impact){
            effectplay::Vector normal;std::memcpy(normal.data(),trace.plane.normal,sizeof(normal));
            if(!effectplay::Finite(normal))Com_Error(ERR_DROP,"Invalid Academy effects trace plane");
            CL_JKACgameEffectSpawn(state,spawn.impact,spawn.endpoint,effectplay::NormalAxis(normal),random,depth+1);
        }
    }
    if(spawn.kind==effects::Kind::Sound){
        // CreateEffect's Sound case (0x40b100..0x40b111) hands S_StartSound
        // (0x46ac60) the spawn origin, ENTITYNUM_NONE, CHAN_AUTO and the
        // picked handle, with no volume or radius: the cgame's PlayEffect
        // volume and radius (template +0x23c, +0x240) are never read there.
        // Fn's spatial path supplies distance, occlusion, EFX and HRTF.
        // Handle 0 is retail's silent "sound/null.wav" (and Fn's failed
        // registration), so Fn plays nothing for it.
        if(spawn.sound>0){
            vec3_t at;std::memcpy(at,spawn.origin.data(),sizeof(at));
            S_StartJKASound(at,effectplay::SoundEntity,effectplay::SoundChannel,static_cast<sfxHandle_t>(spawn.sound));
        }
        if(state.debug&&state.debug->integer>=2)
            Com_Printf("Academy FX sound: %d at %d\n",spawn.sound,state.time);
        return;
    }
    if(spawn.kind==effects::Kind::CameraShake){
        if(state.debug&&state.debug->integer>=2)
            Com_Printf("Academy FX camera shake: %g within %d for %d ms at %d\n",
                spawn.shakeIntensity*CL_ViewShakeScale(),spawn.shakeRadius,spawn.life,state.time);
        CL_JKACgameEffectCameraShake(spawn);
        return;
    }
    if(spawn.kind==effects::Kind::Runner){
        // The FxRunner plays at its origin and axis, after any spawn shape.
        if(spawn.child)CL_JKACgameEffectSpawn(state,spawn.child,spawn.origin,spawn.axis,random,depth+1);
        return;
    }
    state.spawnWork.push_back({id,spawn});
}
// Retail PlayEffect 0x407110: each primitive's count, then a delay per
// spawn (0x4073c6). A delay below one creates at once, late by its negation;
// any other waits in the schedule until it falls due (0x40755d).
static void CL_JKACgameEffectSpawn(JKACgameEffectsState &state,int id,const fntech3::jka::effectplay::Vector &origin,
    const fntech3::jka::effectplay::Axis &axis,std::uint32_t &random,int depth)
{
    using namespace fntech3::jka;
    if(id<1||std::size_t(id)>state.effects.size())return;
    if(depth>jkaCgameEffectNesting)Com_Error(ERR_DROP,"Academy effects nesting exceeds the Fn bound");
    const auto &effect=*state.effects[id-1];
    if(!effect.ready)return;
    if(!effect.playable)Com_Error(ERR_DROP,"Academy effects playback reached an uncompiled declaration");
    if(depth>0&&state.debug&&state.debug->integer>=2)
        Com_Printf("Academy FX nested play: %s at %d\n",effect.name.file.c_str(),state.time);
    const float scale=CL_JKACgameEffectCountScale(state);
    for(std::size_t primitive=0;primitive<effect.playback.size();++primitive){
        const auto &spec=effect.playback[primitive];
        const int count=effectplay::Count(spec,scale,random);
        for(int n=0;n<count;++n){
            const int delay=effectplay::Delay(spec,n,count,random);
            if(delay<1){CL_JKACgameEffectCreate(state,id,primitive,origin,axis,random,depth,-delay);continue;}
            if(std::int64_t(state.time)+delay>INT_MAX)Com_Error(ERR_DROP,"Academy effects clock reversal or wrap is not served yet");
            // Beyond the Fn bound a delayed spawn is dropped and counted.
            if(state.scheduled.size()>=jkaCgameEffectScheduleBound){++state.scheduleDropped;continue;}
            state.scheduled.push_back({state.time+delay,id,depth,primitive,origin,axis});
        }
    }
}
static void CL_JKACgameEffectPublish(JKACgameEffectsState &state) {
    state.slotWork.clear();
    for(std::size_t n=0;n<state.instances.size()&&state.slotWork.size()<state.spawnWork.size();++n){
        const auto slot=(state.nextFree+n)%state.instances.size();
        if(!state.instances[slot].active)state.slotWork.push_back(slot);
    }
    // A full pool never refuses: retail's slot search (0x40f2c0) kills the
    // primitive in its first slot to make room. Fn takes the instances
    // nearest their end instead, so a long-lived emitter outlasts the cones
    // a module replays every frame; work beyond the pool keeps its newest.
    if(state.slotWork.size()<state.spawnWork.size()){
        const std::size_t capacity=state.instances.size();
        if(state.spawnWork.size()>capacity)
            state.spawnWork.erase(state.spawnWork.begin(),state.spawnWork.end()-std::ptrdiff_t(capacity));
        std::vector<std::size_t> &order=state.evictWork;order.clear();
        for(std::size_t n=0;n<capacity;++n)if(state.instances[n].active)order.push_back(n);
        std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
            const auto &first=state.instances[a],&second=state.instances[b];
            return std::int64_t(first.startTime)+first.spawn.life<std::int64_t(second.startTime)+second.spawn.life;});
        for(std::size_t n=0;n<order.size()&&state.slotWork.size()<state.spawnWork.size();++n){
            state.instances[order[n]]={};state.slotWork.push_back(order[n]);
        }
        if(state.slotWork.size()!=state.spawnWork.size())Com_Error(ERR_DROP,"Invalid Academy effects pool accounting");
    }
    for(std::size_t n=0;n<state.spawnWork.size();++n){
        auto &instance=state.instances[state.slotWork[n]];instance.definition=state.spawnWork[n].first;
        instance.startTime=state.time;instance.spawn=state.spawnWork[n].second;instance.active=true;
        // FX_AddEmitter 0x4108cf: the walk starts at creation time.
        instance.spawn.previousTime=state.time;
    }
    if(!state.slotWork.empty())state.nextFree=(state.slotWork.back()+1)%state.instances.size();
    state.spawnWork.clear();state.slotWork.clear();
}
// Retire ended instances, then play their death effects (Die 0x401500) at
// their origins, each facing a random normal.
static void CL_JKACgameEffectExpire(JKACgameEffectsState &state) {
    using namespace fntech3::jka;
    state.deathWork.clear();
    CL_JKACgameEffectRetire(state);
    if(state.deathWork.empty())return;
    state.spawnWork.clear();state.slotWork.clear();
    auto random=state.random;
    for(std::size_t n=0;n<state.deathWork.size();++n){
        const auto death=state.deathWork[n];
        CL_JKACgameEffectSpawn(state,death.first,death.second,effectplay::NormalAxis(effectplay::DeathNormal(random)),random,1);
    }
    state.deathWork.clear();
    CL_JKACgameEffectPublish(state);
    state.random=random;
}
// Retail AddScheduledEffects 0x407810 creates each due spawn, late by its
// lag, before the primitive pass. A paused frame keeps them waiting.
static void CL_JKACgameEffectRunScheduled(JKACgameEffectsState &state) {
    if(state.frameTime<=0||state.scheduled.empty())return;
    state.dueWork.clear();
    std::size_t kept=0;
    for(std::size_t n=0;n<state.scheduled.size();++n){
        if(state.scheduled[n].due<=state.time)state.dueWork.push_back(state.scheduled[n]);
        else state.scheduled[kept++]=state.scheduled[n];
    }
    state.scheduled.resize(kept);
    if(state.dueWork.empty())return;
    state.spawnWork.clear();state.slotWork.clear();
    auto random=state.random;
    for(const auto &entry:state.dueWork)
        CL_JKACgameEffectCreate(state,entry.definition,entry.primitive,entry.origin,entry.axis,random,entry.depth,state.time-entry.due);
    state.dueWork.clear();
    CL_JKACgameEffectPublish(state);
    state.random=random;
}
// Import 266 (0x413431 -> 0x406d70) builds the axis from a forward
// direction; import 268 (0x41346b -> 0x401050) hands PlayEffect the caller's
// own axis and bolt word, with no Ghoul2 instance and no fxParm. Neither
// passes the entity number (268's args[5]) on, and the volume and radius
// PlayEffect stores in the template (+0x23c, +0x240) are read by no served
// family. A bolt word other than -1 would bolt the spawns to a model:
// PlayEffect then schedules every spawn (0x4074cb) and AddScheduledEffects
// asks the client game for the entity (export 11) and Ghoul2 for the bolt
// (0x40c5a0); no retail call site passes one, and Fn refuses it by name.
static void CL_JKACgameEffectPlay(const intptr_t *args,bool entity=false) {
    using namespace fntech3::jka;
    CL_JKACgameEffectNative();
    effectplay::Vector origin,forward;
    effectplay::Axis axis;
    if(entity){
        float words[9];std::memcpy(words,CL_JKACgameSpan(args[3],sizeof(words)),sizeof(words));
        if(!effectplay::EntityAxis(words,axis))Com_Error(ERR_DROP,"Invalid Academy entity effect axis");
    }else{
        std::memcpy(forward.data(),CL_JKACgameSpan(args[3],sizeof(forward)),sizeof(forward));
        if(!effectplay::Finite(forward))Com_Error(ERR_DROP,"Invalid Academy effects direction");
    }
    const int id=static_cast<int>(args[1]);
    // The measured core logs a missing/inactive declaration and returns.
    if(!jkaCgameEffects||id<1||id>=int(effects::MaxEffects)||std::size_t(id)>jkaCgameEffects->effects.size()||
        !jkaCgameEffects->effects[id-1]->ready){Com_Printf("Academy effects playback has no owned declaration for ID%d\n",id);return;}
    if(entity&&static_cast<int>(args[4])!=effectplay::EntityUnbolted)
        Com_Error(ERR_DROP,"Academy entity effect bolt word %d is not served yet",static_cast<int>(args[4]));
    auto &state=*jkaCgameEffects;const auto &effect=*state.effects[id-1];
    if(!state.initialized||state.time<=0)Com_Error(ERR_DROP,"Academy effects playback requires its initialized positive frame clock");
    std::memcpy(origin.data(),CL_JKACgameSpan(args[2],sizeof(origin)),sizeof(origin));
    if(!effectplay::Finite(origin)||(!entity&&!effectplay::Basis(forward,axis)))Com_Error(ERR_DROP,"Invalid Academy effects placement");
    CL_JKACgameEffectCompileGraph(state,id);
    // Fn diagnostic beyond retail's counters: name each played declaration.
    if(state.debug&&state.debug->integer>=2)
        Com_Printf("Academy FX %s: %s at %d\n",entity?"entity play":"play",effect.name.file.c_str(),state.time);
    // Primary0x40f48b..0x40f49b creates no particle on a paused frame.
    if(!state.frameTime)return;
    if(state.frameTime<0)Com_Error(ERR_DROP,"Academy effects clock reversal or wrap is not served yet");
    CL_JKACgameEffectExpire(state);
    state.spawnWork.clear();state.slotWork.clear();
    auto random=state.random;
    CL_JKACgameEffectSpawn(state,id,origin,axis,random,0);
    CL_JKACgameEffectPublish(state);
    state.random=random;
}
// Retail's primitive pass (0x40f320) updates each live emitter: CEmitter
// 0x403c30 keeps this frame's start, moves after its first frame (0x401a50:
// expensive physics traces the step; a hit kills it, a solid start stops
// it), then walks its emissions (0x4038f0). Children join this frame's
// owned pool. A paused frame advances nothing.
// The traced step of a moving particle (0x401b4d..0x401bc6): a point probe
// first unless it is expensive, then the step itself, both through export 15
// against the client's world and entities.
static fntech3::jka::effectplay::Contact CL_JKACgameEffectContact(const fntech3::jka::effectplay::Vector &start,
    const fntech3::jka::effectplay::Vector &end)
{
    const auto trace=CL_JKACgameEffectTrace(start,end);
    fntech3::jka::effectplay::Contact contact;
    contact.allSolid=trace.allsolid;contact.startSolid=trace.startsolid;contact.fraction=trace.fraction;
    std::memcpy(contact.endpos.data(),trace.endpos,sizeof(contact.endpos));
    std::memcpy(contact.normal.data(),trace.plane.normal,sizeof(contact.normal));
    contact.surfaceFlags=static_cast<std::uint32_t>(trace.surfaceFlags);
    if(!fntech3::jka::effectplay::Finite(contact.normal))Com_Error(ERR_DROP,"Invalid Academy effects trace plane");
    return contact;
}
static void CL_JKACgameEffectAdvance(JKACgameEffectsState &state) {
    using namespace fntech3::jka;
    // CTail::Update (0x402e30) keeps this frame's starting origin before it
    // moves, even on a paused frame, so a still tail has no direction.
    for(auto &instance:state.instances)
        if(instance.active&&instance.spawn.kind==effects::Kind::Tail)instance.spawn.previous=instance.spawn.origin;
    if(state.frameTime<=0)return;
    state.spawnWork.clear();state.slotWork.clear();
    auto random=state.random;
    // CParticle (0x401730), COrientedParticle (0x402770) and CTail (0x402e30):
    // one created before this frame moves (0x401a50). A traced particle may
    // stick, bounce, play its impact effect, or die on impact without a
    // death effect.
    const bool rmg=state.rmg&&state.rmg->integer;
    for(auto &instance:state.instances){
        if(!instance.active||!effectplay::Moving(instance.spawn.kind)||instance.startTime>=state.time)continue;
        auto &spawn=instance.spawn;effectplay::Vector next;
        if(!effectplay::ParticlePredict(spawn,state.realTime,next))Com_Error(ERR_DROP,"Invalid Academy effects particle motion");
        if(!effectplay::ParticleTraces(spawn,rmg)){spawn.origin=next;continue;}
        if(!(spawn.flags&effectplay::flag::ExpensivePhysics)){
            // Retail asks export 9 for the point's contents (0x401b33). Fn
            // asks export 15 for a zero-length trace there, which starts
            // solid exactly when the point lies in the same mask.
            const auto probe=CL_JKACgameEffectContact(next,next);
            if(!probe.startSolid&&!probe.allSolid){spawn.origin=next;continue;}
        }
        const auto contact=CL_JKACgameEffectContact(spawn.origin,next);
        bool impact=false;
        const auto outcome=effectplay::ParticleCollide(spawn,state.realTime,next,contact,impact);
        if(impact&&spawn.impact)
            CL_JKACgameEffectSpawn(state,spawn.impact,contact.endpos,effectplay::NormalAxis(contact.normal),random,1);
        if(outcome==effectplay::Collision::Killed)instance={};
    }
    std::vector<std::size_t> emitters;
    for(std::size_t n=0;n<state.instances.size();++n)
        if(state.instances[n].active&&state.instances[n].spawn.kind==effects::Kind::Emitter)emitters.push_back(n);
    if(emitters.empty()){
        if(!state.spawnWork.empty())CL_JKACgameEffectPublish(state);
        state.random=random;return;
    }
    for(const auto index:emitters){
        auto &instance=state.instances[index];
        auto &spawn=instance.spawn;
        spawn.base=spawn.origin;spawn.baseVelocity=spawn.velocity;
        if(instance.startTime<state.time){
            effectplay::Vector next;
            if(!effectplay::EmitterNext(spawn,state.realTime,next))Com_Error(ERR_DROP,"Invalid Academy effects emitter motion");
            if(spawn.flags&effectplay::flag::UsePhysics){
                const auto trace=CL_JKACgameEffectTrace(spawn.origin,next);
                if(trace.allsolid||trace.startsolid){
                    spawn.velocity={};spawn.flags&=~effectplay::flag::UsePhysics;
                }else if(trace.fraction<1.0f){
                    instance={};continue; // impactKills: no emission on the dying frame.
                }else spawn.origin=next;
            }else spawn.origin=next;
        }
        if(!(spawn.flags&effectplay::flag::EmitFx)||!spawn.child)continue;
        const auto axis=spawn.axis;const int child=spawn.child;
        if(!effectplay::EmitterWalk(spawn,state.time,random,[&](const effectplay::Vector &position){
            CL_JKACgameEffectSpawn(state,child,position,axis,random,1);return true;}))
            Com_Error(ERR_DROP,"Invalid Academy effects emission position");
    }
    CL_JKACgameEffectPublish(state);
    state.random=random;
}
static void CL_JKACgameEffectVertex(polyVert_t &vertex,const fntech3::jka::effectplay::Spawn &spawn,
    float along,float side,float up,float s,float t,const std::array<unsigned char,4> &rgba)
{
    for(int n=0;n<3;++n){
        vertex.xyz[n]=spawn.origin[n]+spawn.axis[0][n]*along+spawn.axis[1][n]*side+spawn.axis[2][n]*up;
        if(!fntech3::jka::effectplay::Finite(vertex.xyz[n]))Com_Error(ERR_DROP,"Invalid Academy effects polygon vertex");
    }
    vertex.st[0]=s;vertex.st[1]=t;std::memcpy(vertex.modulate.rgba,rgba.data(),4);
}
static void CL_JKACgameEffectSpan(polyVert_t &vertex,const fntech3::jka::effectplay::Vector &point,
    const fntech3::jka::effectplay::Vector &side,float offset,float s,float t,const std::array<unsigned char,4> &rgba)
{
    for(int component=0;component<3;++component){
        vertex.xyz[component]=point[component]+side[component]*offset;
        if(!fntech3::jka::effectplay::Finite(vertex.xyz[component]))Com_Error(ERR_DROP,"Invalid Academy effects line vertex");
    }
    vertex.st[0]=s;vertex.st[1]=t;std::memcpy(vertex.modulate.rgba,rgba.data(),4);
}
static void CL_JKACgameEffectDrawScheduled(bool portal) {
    using namespace fntech3::jka;
    CL_JKACgameEffectNative();
    if(!jkaCgameEffects)return;
    auto &state=*jkaCgameEffects;
    // No portal-play import is served by this subset. Its portal lane is
    // genuinely empty; normal instances stay owned for the normal pass.
    if(portal)return;
    CL_JKACgameEffectExpire(state);
    state.drawWork.clear();
    CL_JKACgameEffectRunScheduled(state);
    bool active=false;for(const auto &instance:state.instances)active|=instance.active;
    if(!active)return;
    CL_JKACgameEffectAdvance(state);
    if(!re.AddRefEntityToScene||!re.AddPolyToScene)Com_Error(ERR_DROP,"Academy Fn effects renderer is unavailable");
    refdef_t view{};
    if(render::ReadView(CL_JKACgameSpan(state.view,sizeof(fntech3_jka_mp_refdef_t)),view)!=render::Status::Ok)
        Com_Error(ERR_DROP,"Invalid Academy effects borrowed view");
    const float nearDistance=state.nearCull?state.nearCull->value:16.0f;
    if(!effectplay::Finite(nearDistance)||nearDistance<0||nearDistance>4096)Com_Error(ERR_DROP,"Invalid Academy effects near-cull setting");
    effectplay::Vector eye{};std::memcpy(eye.data(),view.vieworg,sizeof(view.vieworg));
    auto random=state.random;
    for(auto &instance:state.instances){
        if(!instance.active)continue;
        auto &spawn=instance.spawn;
        const int age=static_cast<int>(std::int64_t(state.time)-instance.startTime);
        // An emitter without a model draws nothing (0x4038f0); a Light is
        // never culled (0x403f90 has no cull test).
        if(spawn.kind==effects::Kind::Emitter)continue;
        double distance=0;for(int n=0;n<3;++n){const double delta=double(spawn.origin[n])-view.vieworg[n];distance+=delta*delta;}
        if(spawn.kind!=effects::Kind::Line&&spawn.kind!=effects::Kind::Light&&distance<double(nearDistance)*nearDistance)continue;
        state.drawWork.emplace_back();auto &draw=state.drawWork.back();draw.shader=spawn.shader;
        // A visible curved primitive updates its curves (and a particle's
        // or oriented particle's rotation), then draws (0x401a1b..0x401a36,
        // 0x402abf..0x402add, 0x403188..0x4031a8, 0x402dc3..0x402dd7).
        effectplay::Look look;
        if(effectplay::Curved(spawn.kind)){
            const bool turns=spawn.kind==effects::Kind::Particle||spawn.kind==effects::Kind::OrientedParticle;
            if((turns&&!effectplay::ParticleRotate(spawn,state.frameTime))||
                !effectplay::ParticleLook(spawn,state.time,instance.startTime,look,&random))
                Com_Error(ERR_DROP,"Invalid Academy effects particle curve");
        }
        if(spawn.kind==effects::Kind::Particle){
            draw.entity.reType=RT_SPRITE;draw.entity.customShader=spawn.shader;draw.entity.radius=look.radius;draw.entity.rotation=spawn.rotation;
            // depthHack sets RF_DEPTHHACK (0x401620); setShaderTime starts
            // the shader's clock at creation, in seconds (0x40f440..0x40f467).
            if(spawn.flags&effectplay::flag::DepthHack)draw.entity.renderfx|=RF_DEPTHHACK;
            if(spawn.flags&effectplay::flag::SetShaderTime){
                volatile float start=static_cast<float>(instance.startTime);draw.entity.shaderTime.f=start*0.001f;
            }
            std::memcpy(draw.entity.origin,spawn.origin.data(),sizeof(vec3_t));
            std::memcpy(draw.entity.shader.rgba,look.rgba.data(),4);
        }else if(spawn.kind==effects::Kind::Light){
            // CLight 0x403f50: origin, size curve as radius, RGB. Fn's dynamic
            // light path supplies per-pixel lighting and shadows.
            draw.light=true;draw.radius=effectplay::CurveRadius(spawn,age);draw.color=spawn.light;
            if(!effectplay::Finite(draw.radius)||draw.radius<0)Com_Error(ERR_DROP,"Invalid Academy effects light radius");
            std::memcpy(draw.entity.origin,spawn.origin.data(),sizeof(vec3_t));
        }else if(spawn.kind==effects::Kind::OrientedParticle){
            draw.vertices.resize(4);
            const float radians=spawn.rotation*0.017453292519943295f;
            const float cosine=std::cos(radians),sine=std::sin(radians);
            constexpr float x[]{-1,1,1,-1},y[]{-1,-1,1,1};
            for(int n=0;n<4;++n)CL_JKACgameEffectVertex(draw.vertices[n],spawn,0,
                look.radius*(x[n]*cosine-y[n]*sine),look.radius*(x[n]*sine+y[n]*cosine),n==1||n==2?1:0,n>=2?1:0,look.rgba);
        }else if(spawn.kind==effects::Kind::Line||spawn.kind==effects::Kind::Tail){
            // A Line spans its endpoints; a Tail (0x402df0, retail's RT_LINE)
            // runs from its origin back along its motion by its length.
            effectplay::Vector side{},end=spawn.endpoint;
            if(spawn.kind==effects::Kind::Tail&&!effectplay::TailEnd(spawn,look.length,end)){state.drawWork.pop_back();continue;}
            const float radius=look.radius;
            if(!effectplay::Finite(radius))Com_Error(ERR_DROP,"Invalid Academy effects line width");
            if(radius==0||!effectplay::LineSide(spawn.origin,end,eye,side)){state.drawWork.pop_back();continue;}
            draw.vertices.resize(4);
            for(int n=0;n<4;++n){
                const auto &point=n<2?spawn.origin:end;
                CL_JKACgameEffectSpan(draw.vertices[n],point,side,(n==0||n==3)?radius:-radius,
                    n==0||n==3?0.0f:1.0f,n<2?0.0f:1.0f,look.rgba);
            }
        }else if(spawn.kind==effects::Kind::Electricity){
            const float radius=look.radius;
            if(!effectplay::Finite(radius))Com_Error(ERR_DROP,"Invalid Academy effects electricity width");
            // One camera-facing strip per bolt and branch; each joint faces the
            // eye across its neighbours, so the quads share their edges.
            const auto strip=[&](effectplay::Bolt &points,int segments,int total,const auto &width){
                if(segments<1)return;
                std::array<effectplay::Vector,effectplay::BoltSegments+1> sides;
                for(int n=0;n<=segments;++n)
                    if(!effectplay::LineSide(points[n>0?n-1:0],points[n<segments?n+1:segments],eye,sides[n]))return;
                for(int n=0;n<segments;++n){
                    const float t0=float(n)/total,t1=float(n+1)/total,first=width(n),last=width(n+1);
                    draw.vertices.resize(draw.vertices.size()+4);auto *quad=&draw.vertices[draw.vertices.size()-4];
                    CL_JKACgameEffectSpan(quad[0],points[n],sides[n],first,0,t0,look.rgba);
                    CL_JKACgameEffectSpan(quad[1],points[n],sides[n],-first,1,t0,look.rgba);
                    CL_JKACgameEffectSpan(quad[2],points[n+1],sides[n+1],-last,1,t1,look.rgba);
                    CL_JKACgameEffectSpan(quad[3],points[n+1],sides[n+1],last,0,t1,look.rgba);
                }
            };
            effectplay::Bolt points;
            if(radius>0){
                strip(points,effectplay::BoltPoints(spawn,age,points),effectplay::BoltSegments,
                    [&](int point){return effectplay::BoltWidth(spawn,radius,point);});
                for(int branch=0;branch<effectplay::BoltBranches;++branch){
                    int from=0;const int segments=effectplay::BranchPoints(spawn,age,branch,points,from);
                    strip(points,segments,effectplay::BranchSegments,
                        [&](int point){return effectplay::BranchWidth(spawn,radius,from,point);});
                }
            }
            if(draw.vertices.empty()){state.drawWork.pop_back();continue;}
        }else if(spawn.kind==effects::Kind::Cylinder){
            constexpr int segments=16;draw.vertices.resize(segments*4);
            for(int n=0;n<segments;++n){
                const float first=n*0.39269908169872414f,last=(n+1)*0.39269908169872414f;
                const float c0=std::cos(first),s0=std::sin(first),c1=std::cos(last),s1=std::sin(last);
                CL_JKACgameEffectVertex(draw.vertices[n*4],spawn,0,spawn.size*c0,spawn.size*s0,float(n)/segments,0,spawn.color);
                CL_JKACgameEffectVertex(draw.vertices[n*4+1],spawn,0,spawn.size*c1,spawn.size*s1,float(n+1)/segments,0,spawn.color);
                CL_JKACgameEffectVertex(draw.vertices[n*4+2],spawn,spawn.length,spawn.size2*c1,spawn.size2*s1,float(n+1)/segments,1,spawn.color);
                CL_JKACgameEffectVertex(draw.vertices[n*4+3],spawn,spawn.length,spawn.size2*c0,spawn.size2*s0,float(n)/segments,1,spawn.color);
            }
        }else Com_Error(ERR_DROP,"Invalid Academy effects owned primitive family");
    }
    state.random=random;
    for(const auto &draw:state.drawWork)if(draw.light&&!re.AddLightToScene)
        Com_Error(ERR_DROP,"Academy Fn effects light service is unavailable");
    // Retail fx_debug (0x40f3b2..0x40f40b) prints its active, drawn and
    // scheduled counts each normal pass.
    if(state.debug&&state.debug->integer){
        int count=0;for(const auto &instance:state.instances)count+=instance.active;
        Com_Printf("Active    FX: %i\nDrawn     FX: %i\nScheduled FX: %i\n",count,int(state.drawWork.size()),int(state.scheduled.size()));
        if(state.scheduleDropped)Com_Printf("Dropped   FX: %i (Fn schedule bound)\n",int(state.scheduleDropped));
    }
    for(const auto &draw:state.drawWork){
        if(draw.light)re.AddLightToScene(draw.entity.origin,draw.radius,draw.color[0],draw.color[1],draw.color[2]);
        else if(draw.vertices.empty())re.AddRefEntityToScene(&draw.entity,qfalse);
        else re.AddPolyToScene(draw.shader,4,draw.vertices.data(),static_cast<int>(draw.vertices.size()/4));
    }
    state.drawWork.clear();
}
static bool CL_JKACgameEffectsCall(intptr_t *args,fntech3::jka::module::Call call,intptr_t &result) {
    using fntech3::jka::module::Call;
    if(jkaCgameEffectsTracing)switch(call){
    case Call::CG_FX_PLAY_EFFECT_ID:case Call::CG_FX_PLAY_ENTITY_EFFECT_ID:case Call::CG_FX_ADD_SCHEDULED_EFFECTS:case Call::CG_FX_INIT_SYSTEM:
    case Call::CG_FX_SET_REFDEF:case Call::CG_FX_FREE_SYSTEM:case Call::CG_FX_ADJUST_TIME:case Call::CG_FX_REGISTER_EFFECT:
        Com_Error(ERR_DROP,"Academy effects trace or camera shake cannot reenter its effect context");
    default:break;
    }
    switch (call) {
    case Call::CG_FX_PLAY_EFFECT_ID:CL_JKACgameEffectPlay(args);result=0;return true;
    case Call::CG_FX_PLAY_ENTITY_EFFECT_ID:CL_JKACgameEffectPlay(args,true);result=0;return true;
    case Call::CG_FX_ADD_SCHEDULED_EFFECTS:CL_JKACgameEffectDrawScheduled(args[1]!=0);result=0;return true;
    case Call::CG_FX_INIT_SYSTEM:CL_JKACgameEffectsInit(args[1]);result=1;return true;
    case Call::CG_FX_SET_REFDEF:
        ++jkaCgameEffectsRevision;
        CL_JKACgameEffectsState().view=CL_JKACgameEffectsView(args[1]);result=0;return true;
    case Call::CG_FX_FREE_SYSTEM:CL_JKACgameEffectsFree();result=1;return true;
    case Call::CG_FX_ADJUST_TIME:CL_JKACgameEffectsAdjustTime(args[1]);result=0;return true;
    default:if (call!=Call::CG_FX_REGISTER_EFFECT) return false;break;
    }
    using namespace fntech3::jka::effects;
    std::array<char,NameBytes> name{};
    if (!args[1]) Com_Error(ERR_DROP,"Null Jedi Academy cgame effect name");
    for (std::size_t n=0;n<name.size();++n) {
        if (std::uintptr_t(args[1])>(std::numeric_limits<std::uintptr_t>::max)()-n)
            Com_Error(ERR_DROP,"Invalid Jedi Academy cgame effect name address");
        name[n]=*static_cast<const char *>(CL_JKACgameSpan(std::uintptr_t(args[1])+n,1));
        if (!name[n]) {result=CL_JKACgameEffectsRegister(name.data());return true;}
    }
    Com_Error(ERR_DROP,"Jedi Academy cgame effect name exceeds 63 bytes");
    return true;
}
} // namespace
#endif
