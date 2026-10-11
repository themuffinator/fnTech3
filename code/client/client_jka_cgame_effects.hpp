/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_EFFECTS_HPP
#define FNTECH3_CLIENT_JKA_CGAME_EFFECTS_HPP
#include "../compat/jka_effects.hpp"
#include "../compat/jka_client_abi.h"
#include "../compat/jka_effect_play.hpp"
#include "../compat/jka_render_adapter.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>

/* Cgame import 263 registers an owned declaration and actual Fn media handles.
 * Context/pool lifecycle and its frame clock are served. Shader-only single-
 * spawn/no-motion particles, cylinders and lines enter Fn sprites/polygons; remaining
 * playback controls and primitive families remain named refusals. Registration owns
 * all buffers and partial records across engine ERR_DROP/longjmp recovery. */
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
struct JKACgameEffectDraw {
    refEntity_t entity{};
    int shader{};
    std::vector<polyVert_t> vertices;
};
struct JKACgameEffectsState {
    // Retail's init pool has 1,800 records; the engine owns its own shape.
    std::array<JKACgameEffectInstance,1800> instances{};
    intptr_t view{};
    int time{}, previousTime{}, frameTime{};
    float realTime{};
    std::size_t nextFree{};
    bool initialized{}, frozen{};
    std::uint32_t random{0x4a4b4101u};
    std::vector<fntech3::jka::effectplay::Spawn> spawnWork;
    std::vector<std::size_t> slotWork;
    std::vector<JKACgameEffectDraw> drawWork;
    cvar_t *debug{}, *countScale{}, *nearCull{};
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
    if (!state.initialized) state.instances={};
    state.nextFree=0;
    state.debug=Cvar_Get("fx_debug","0",CVAR_TEMP);
    state.countScale=Cvar_Get("fx_countScale","1",CVAR_ARCHIVE);
    state.nearCull=Cvar_Get("fx_nearCull","16",CVAR_ARCHIVE);
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
    state.realTime=milliseconds*0.001f;
}
static void CL_JKACgameEffectsFree() {
    if (!jkaCgameEffects) return;
    ++jkaCgameEffectsRevision;
    auto &state=*jkaCgameEffects;
    for (const auto &effect:state.effects) CL_JKACgameEffectClose(*effect);
    state.effects.clear();state.lookup.clear();state.pendingName={};
    state.instances={};state.nextFree=0;
    state.spawnWork.clear();state.slotWork.clear();state.drawWork.clear();
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
static void CL_JKACgameEffectTraceEndpoint(fntech3::jka::effectplay::Spawn &spawn) {
    using namespace fntech3::jka;
    // Retail0x40931f publishes104bytes to the registered scratch buffer,
    // then export15 traces against the client's world and entity snapshot.
    const auto *owner=jkaCgameEffects.get();
    const auto revision=jkaCgameEffectsRevision;
    auto *const module=cgvm;
    void *const shared=jkaCgameSharedBuffer;
    void *const destination=CL_JKACgameSpan(reinterpret_cast<intptr_t>(shared),sizeof(fntech3_jka_cgame_trace_t));
    fntech3_jka_cgame_trace_t request{};
    std::memcpy(request.start,spawn.origin.data(),sizeof(request.start));
    std::memcpy(request.end,spawn.endpoint.data(),sizeof(request.end));
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
        spawn.endpoint[n]=value;
    }
}
static void CL_JKACgameEffectRetire(JKACgameEffectsState &state) {
    for(const auto &instance:state.instances)if(instance.active){
        const std::int64_t age=std::int64_t(state.time)-instance.startTime;
        if(age<0||age>INT32_MAX)Com_Error(ERR_DROP,"Academy effects clock reversal or wrap is not served yet");
    }
    for(auto &instance:state.instances)if(instance.active&&std::int64_t(state.time)-instance.startTime>instance.spawn.life)
        instance={};
}
static void CL_JKACgameEffectPlay(const intptr_t *args) {
    using namespace fntech3::jka;
    CL_JKACgameEffectNative();
    effectplay::Vector origin,forward;
    std::memcpy(forward.data(),CL_JKACgameSpan(args[3],sizeof(forward)),sizeof(forward));
    if(!effectplay::Finite(forward))Com_Error(ERR_DROP,"Invalid Academy effects direction");
    const int id=static_cast<int>(args[1]);
    // The measured core logs a missing/inactive declaration and returns.
    if(!jkaCgameEffects||id<1||id>=int(effects::MaxEffects)||std::size_t(id)>jkaCgameEffects->effects.size()||
        !jkaCgameEffects->effects[id-1]->ready){Com_Printf("Academy effects playback has no owned declaration for ID%d\n",id);return;}
    auto &state=*jkaCgameEffects;auto &effect=*state.effects[id-1];
    if(!state.initialized||state.time<=0)Com_Error(ERR_DROP,"Academy effects playback requires its initialized positive frame clock");
    std::memcpy(origin.data(),CL_JKACgameSpan(args[2],sizeof(origin)),sizeof(origin));
    effectplay::Axis axis;
    if(!effectplay::Finite(origin)||!effectplay::Basis(forward,axis))Com_Error(ERR_DROP,"Invalid Academy effects placement");
    if(!effect.playable){
        effect.playback.clear();effect.playback.reserve(effect.definition.primitives.size());
        if(!effect.definition.ignoredGroups.empty())Com_Error(ERR_DROP,"Academy effects playback has unserved declaration groups");
        for(const auto &primitive:effect.definition.primitives){
            effectplay::Spec spec;effectplay::Failure failure;
            if(!effectplay::Compile(primitive,spec,failure))
                Com_Error(ERR_DROP,"Academy effect %s: %s (%s)",effect.name.file.c_str(),failure.reason,failure.field);
            effect.playback.push_back(spec);
        }
        effect.playable=true;
    }
    // Primary0x40f48b..0x40f49b creates no particle on a paused frame.
    if(!state.frameTime)return;
    if(state.frameTime<0)Com_Error(ERR_DROP,"Academy effects clock reversal or wrap is not served yet");
    CL_JKACgameEffectRetire(state);
    state.spawnWork.clear();state.slotWork.clear();
    auto random=state.random;
    for(const auto &spec:effect.playback){
        effectplay::Spawn spawn;
        if(!effectplay::Instantiate(spec,origin,axis,random,spawn))Com_Error(ERR_DROP,"Invalid Academy effects spawn result");
        if(spawn.traceEnd)CL_JKACgameEffectTraceEndpoint(spawn);
        state.spawnWork.push_back(spawn);
    }
    for(std::size_t n=0;n<state.instances.size()&&state.slotWork.size()<state.spawnWork.size();++n){
        const auto slot=(state.nextFree+n)%state.instances.size();
        if(!state.instances[slot].active)state.slotWork.push_back(slot);
    }
    if(state.slotWork.size()!=state.spawnWork.size())Com_Error(ERR_DROP,"Academy effects owned lifetime pool is full");
    for(std::size_t n=0;n<state.spawnWork.size();++n){
        auto &instance=state.instances[state.slotWork[n]];instance.definition=id;instance.startTime=state.time;
        instance.spawn=state.spawnWork[n];instance.active=true;
    }
    if(!state.slotWork.empty())state.nextFree=(state.slotWork.back()+1)%state.instances.size();
    state.random=random;state.spawnWork.clear();state.slotWork.clear();
}
static void CL_JKACgameEffectVertex(polyVert_t &vertex,const fntech3::jka::effectplay::Spawn &spawn,
    float along,float side,float up,float s,float t)
{
    for(int n=0;n<3;++n){
        vertex.xyz[n]=spawn.origin[n]+spawn.axis[0][n]*along+spawn.axis[1][n]*side+spawn.axis[2][n]*up;
        if(!fntech3::jka::effectplay::Finite(vertex.xyz[n]))Com_Error(ERR_DROP,"Invalid Academy effects polygon vertex");
    }
    vertex.st[0]=s;vertex.st[1]=t;std::memcpy(vertex.modulate.rgba,spawn.color.data(),4);
}
static void CL_JKACgameEffectDrawScheduled(bool portal) {
    using namespace fntech3::jka;
    CL_JKACgameEffectNative();
    if(!jkaCgameEffects)return;
    auto &state=*jkaCgameEffects;
    // No portal-play import is served by this subset. Its portal lane is
    // genuinely empty; normal instances stay owned for the normal pass.
    if(portal)return;
    CL_JKACgameEffectRetire(state);
    state.drawWork.clear();
    bool active=false;for(const auto &instance:state.instances)active|=instance.active;
    if(!active)return;
    if(!re.AddRefEntityToScene||!re.AddPolyToScene)Com_Error(ERR_DROP,"Academy Fn effects renderer is unavailable");
    refdef_t view{};
    if(render::ReadView(CL_JKACgameSpan(state.view,sizeof(fntech3_jka_mp_refdef_t)),view)!=render::Status::Ok)
        Com_Error(ERR_DROP,"Invalid Academy effects borrowed view");
    const float nearDistance=state.nearCull?state.nearCull->value:16.0f;
    if(!effectplay::Finite(nearDistance)||nearDistance<0||nearDistance>4096)Com_Error(ERR_DROP,"Invalid Academy effects near-cull setting");
    for(const auto &instance:state.instances){
        if(!instance.active)continue;
        const auto &spawn=instance.spawn;
        double distance=0;for(int n=0;n<3;++n){const double delta=double(spawn.origin[n])-view.vieworg[n];distance+=delta*delta;}
        if(spawn.kind!=effects::Kind::Line&&distance<double(nearDistance)*nearDistance)continue;
        state.drawWork.emplace_back();auto &draw=state.drawWork.back();draw.shader=spawn.shader;
        if(spawn.kind==effects::Kind::Particle){
            draw.entity.reType=RT_SPRITE;draw.entity.customShader=spawn.shader;draw.entity.radius=spawn.size;draw.entity.rotation=spawn.rotation;
            // No FX_SET_SHADER_TIME control is served; retain measured0.
            std::memcpy(draw.entity.origin,spawn.origin.data(),sizeof(vec3_t));
            std::memcpy(draw.entity.shader.rgba,spawn.color.data(),4);
        }else if(spawn.kind==effects::Kind::OrientedParticle){
            draw.vertices.resize(4);
            const float radians=spawn.rotation*0.017453292519943295f;
            const float cosine=std::cos(radians),sine=std::sin(radians);
            constexpr float x[]{-1,1,1,-1},y[]{-1,-1,1,1};
            for(int n=0;n<4;++n)CL_JKACgameEffectVertex(draw.vertices[n],spawn,0,
                spawn.size*(x[n]*cosine-y[n]*sine),spawn.size*(x[n]*sine+y[n]*cosine),n==1||n==2?1:0,n>=2?1:0);
        }else if(spawn.kind==effects::Kind::Line){
            effectplay::Vector eye{},side{};
            std::memcpy(eye.data(),view.vieworg,sizeof(view.vieworg));
            const float radius=effectplay::LineRadius(spawn,static_cast<int>(std::int64_t(state.time)-instance.startTime));
            if(!effectplay::Finite(radius))Com_Error(ERR_DROP,"Invalid Academy effects line width");
            if(radius==0||!effectplay::LineSide(spawn.origin,spawn.endpoint,eye,side)){state.drawWork.pop_back();continue;}
            draw.vertices.resize(4);
            for(int n=0;n<4;++n){
                auto &vertex=draw.vertices[n];
                const auto &point=n<2?spawn.origin:spawn.endpoint;
                const float offset=(n==0||n==3)?radius:-radius;
                for(int component=0;component<3;++component){
                    vertex.xyz[component]=point[component]+side[component]*offset;
                    if(!effectplay::Finite(vertex.xyz[component]))Com_Error(ERR_DROP,"Invalid Academy effects line vertex");
                }
                vertex.st[0]=n==0||n==3?0.0f:1.0f;vertex.st[1]=n<2?0.0f:1.0f;
                std::memcpy(vertex.modulate.rgba,spawn.color.data(),4);
            }
        }else if(spawn.kind==effects::Kind::Cylinder){
            constexpr int segments=16;draw.vertices.resize(segments*4);
            for(int n=0;n<segments;++n){
                const float first=n*0.39269908169872414f,last=(n+1)*0.39269908169872414f;
                const float c0=std::cos(first),s0=std::sin(first),c1=std::cos(last),s1=std::sin(last);
                CL_JKACgameEffectVertex(draw.vertices[n*4],spawn,0,spawn.size*c0,spawn.size*s0,float(n)/segments,0);
                CL_JKACgameEffectVertex(draw.vertices[n*4+1],spawn,0,spawn.size*c1,spawn.size*s1,float(n+1)/segments,0);
                CL_JKACgameEffectVertex(draw.vertices[n*4+2],spawn,spawn.length,spawn.size2*c1,spawn.size2*s1,float(n+1)/segments,1);
                CL_JKACgameEffectVertex(draw.vertices[n*4+3],spawn,spawn.length,spawn.size2*c0,spawn.size2*s0,float(n)/segments,1);
            }
        }else Com_Error(ERR_DROP,"Invalid Academy effects owned primitive family");
    }
    for(const auto &draw:state.drawWork){
        if(draw.vertices.empty())re.AddRefEntityToScene(&draw.entity,qfalse);
        else re.AddPolyToScene(draw.shader,4,draw.vertices.data(),static_cast<int>(draw.vertices.size()/4));
    }
    state.drawWork.clear();
}
static bool CL_JKACgameEffectsCall(intptr_t *args,fntech3::jka::module::Call call,intptr_t &result) {
    using fntech3::jka::module::Call;
    if(jkaCgameEffectsTracing)switch(call){
    case Call::CG_FX_PLAY_EFFECT_ID:case Call::CG_FX_ADD_SCHEDULED_EFFECTS:case Call::CG_FX_INIT_SYSTEM:
    case Call::CG_FX_SET_REFDEF:case Call::CG_FX_FREE_SYSTEM:case Call::CG_FX_ADJUST_TIME:case Call::CG_FX_REGISTER_EFFECT:
        Com_Error(ERR_DROP,"Academy effects trace cannot reenter its effect context");
    default:break;
    }
    switch (call) {
    case Call::CG_FX_PLAY_EFFECT_ID:CL_JKACgameEffectPlay(args);result=0;return true;
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
