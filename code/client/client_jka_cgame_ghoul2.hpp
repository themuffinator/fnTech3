/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_GHOUL2_HPP
#define FNTECH3_CLIENT_JKA_CGAME_GHOUL2_HPP
#include "../ghoul2/ghoul2_server.hpp"
#include "../compat/jka_skin.hpp"
#include "../compat/jka_client_abi.h"
#include "client_jka_cgame_clock.hpp"
#include "client_jka_cgame_render_state.hpp"
#include <array>

/* MP cgame owns a separate opaque model store. Primary retail dispatch:
 * Init 297:0x413977, Clean 301:0x413ae4, Have 293:0x41385e,
 * Has 310:0x413c52 and GLAName 306:0x413bc6. Has takes the address of
 * a four-byte identity slot; Have and GLAName take the identity itself.
 * This bounded metadata path does not submit an unimplemented cgame pose. */
namespace {

class JKACgameGhoul2Files final : public fntech3::ghoul2::Files {
public:
    std::size_t Read(const char *path, void *destination, std::size_t capacity) override {
        if (!path || !*path || std::strlen(path) >= MAX_QPATH || *path == '/' || *path == '\\') return 0;
        const char *part = path;
        for (const char *at = path; ; ++at) {
            if (*at == ':' || (*at && static_cast<unsigned char>(*at) < 32)) return 0;
            if (!*at || *at == '/' || *at == '\\') {
                const auto count = at - part;
                if (!count || (count == 1 && part[0] == '.') ||
                    (count == 2 && part[0] == '.' && part[1] == '.')) return 0;
                if (!*at) break;
                part = at + 1;
            }
        }
        fileHandle_t handle = 0;
        const int length = FS_FOpenFileRead(path, &handle, qfalse);
        if (length <= 0 || !handle) {
            if (handle) FS_FCloseFile(handle);
            return 0;
        }
        const auto bytes = std::size_t(length);
        int read = length;
        if (destination && capacity >= bytes && bytes <= fntech3::ghoul2::MaxModelBytes)
            read = FS_Read(destination, length, handle);
        FS_FCloseFile(handle);
        return read == length ? bytes : 0;
    }
};

JKACgameGhoul2Files jkaCgameGhoul2Files;
std::unique_ptr<fntech3::ghoul2::Store> jkaCgameGhoul2Store;

fntech3::jka::cgame::clock::State jkaCgameBoneClocks;
// Primary296 sets this selector before the shared core; only a successful
// no-rotation result clears it. A false bolt result leaves it pending.
bool jkaCgameNoRotationPending = false;
inline void CL_JKACgameGhoul2BeginFrame(int now, bool localServer) {
    jkaCgameBoneClocks.BeginFrame(now, localServer);
}
static void CL_JKACgameSkinClear();
static void CL_JKACgameGhoul2Clear() {
    fntech3::jka::cgame::scene::state.Clear();
    jkaCgameBoneClocks.Reset();jkaCgameNoRotationPending=false;CL_JKACgameSkinClear();jkaCgameGhoul2Store.reset();
}

static fntech3::ghoul2::Store &CL_JKACgameGhoul2Store() {
    if (!jkaCgameGhoul2Store)
        // The same retail cores as the server: 0x4df310, 0x4df460, 0x4df820.
        // docs/fntech3/2026-10-07-jka-bone-names.md pins their name matching.
        jkaCgameGhoul2Store = std::make_unique<fntech3::ghoul2::Store>(jkaCgameGhoul2Files,true,true,true,
            false,fntech3::ghoul2::NameMatch::AsciiFolded);
    return *jkaCgameGhoul2Store;
}

static void CL_JKACgameGhoul2Native() {
    if (sizeof(void *) != sizeof(std::uint32_t) || !cgvm || !cgvm->entryPoint || cgvm->dllExports)
        Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 requires its x86 legacy native module");
}

static fntech3::ghoul2::Instance *CL_JKACgameGhoul2Resolve(std::uint32_t identity) {
    if (!identity) return nullptr;
    auto *instance = jkaCgameGhoul2Store ?
        jkaCgameGhoul2Store->Resolve(reinterpret_cast<void *>(std::uintptr_t(identity))) : nullptr;
    if (!instance) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 received a stale or foreign model identity");
    return instance;
}

static std::array<char, MAX_QPATH> CL_JKACgameGhoul2Text(intptr_t address) {
    std::array<char, MAX_QPATH> text{};
    if (!address) return text;
    for (std::size_t n = 0; n < text.size(); ++n) {
        if (std::uintptr_t(address) > (std::numeric_limits<std::uintptr_t>::max)() - n)
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame Ghoul2 text address");
        text[n] = *static_cast<const char *>(CL_JKACgameSpan(std::uintptr_t(address) + n, 1));
        if (!text[n]) return text;
    }
    Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 text exceeds MAX_QPATH");
    return text;
}

#include "client_jka_cgame_skins.hpp"

static bool CL_JKACgameGhoul2Call(intptr_t *args, fntech3::jka::module::Call call, intptr_t &result) {
    using fntech3::jka::module::Call;
    switch (call) {
    case Call::CG_G2_INITGHOUL2MODEL: {
        CL_JKACgameGhoul2Native();
        for (int argument : {3, 5, 6, 7}) if (args[argument])
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 nonzero model index, shader, flags or LOD selector is not served yet "
                "(modelIndex=%ld, skin=%ld, shader=%ld, flags=0x%lx, lod=%ld)",
                static_cast<long>(args[3]), static_cast<long>(args[4]), static_cast<long>(args[5]),
                static_cast<unsigned long>(args[6]), static_cast<long>(args[7]));
        if (args[4] < 0)
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 received a negative skin handle");
        void *slot = CL_JKACgameSpan(args[1], sizeof(std::uint32_t));
        std::uint32_t identity;
        std::memcpy(&identity, slot, sizeof(identity));
        CL_JKACgameGhoul2Resolve(identity);
        result = -1;
        if (!args[2]) return true;
        const auto filename = CL_JKACgameGhoul2Text(args[2]);
        if (!filename[0]) return true;
        auto &store = CL_JKACgameGhoul2Store();
        if (!identity && store.InstanceCount() >= 1024) return true;
        void *handle = reinterpret_cast<void *>(std::uintptr_t(identity));
        fntech3::ghoul2::LoadError error;
        result = store.AddModel(&handle, filename.data(), error);
        if (result >= 0) {
            // Retail 0x4de8ce..0x4de8d5 retains args[4] at model +0x38.
            // This is the client's actual renderer handle, never the server's
            // separate skin registry. The cgame renderer consumes it per
            // model through Fn's skeletal material path.
            if (!store.SetSkin(handle, static_cast<int>(result), static_cast<int>(args[4])))
                Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 cannot bind the loaded model's skin");
            identity = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(handle));
            std::memcpy(slot, &identity, sizeof(identity));
        }
        return true;
    }
    case Call::CG_R_REGISTERSKIN:
        CL_JKACgameGhoul2Native();result=CL_JKACgameRegisterSkin(args[1]);return true;
    case Call::CG_G2_SETSKIN:
        CL_JKACgameGhoul2Native();
        result=CL_JKACgameSetSkin(static_cast<std::uint32_t>(args[1]),static_cast<int>(args[2]),
            static_cast<int>(args[3]),static_cast<int>(args[4]))?qtrue:qfalse;
        return true;
    case Call::CG_G2_ANGLEOVERRIDE: {
        CL_JKACgameGhoul2Native();
        const auto identity=static_cast<std::uint32_t>(args[1]);
        const auto *instance=CL_JKACgameGhoul2Resolve(identity);
        result=qfalse;
        if(!instance||!instance->At(static_cast<int>(args[2])))return true;
        const auto bone=CL_JKACgameGhoul2Text(args[3]);
        std::array<float,3> angles;
        std::memcpy(angles.data(),CL_JKACgameSpan(args[4],sizeof(angles)),sizeof(angles));
        // Retail 302:0x413a9c ->0x4df460 ->0x4e3240 transfers eleven
        // words. The core never reads the mapped model-list word (args[9]);
        // no borrowed list is traversed or retained. Actual Fn pose state
        // owns the copied vector, axes, mode and clock/blend selectors.
        result=CL_JKACgameGhoul2Store().SetBoneAngles(reinterpret_cast<void *>(std::uintptr_t(identity)),
            static_cast<int>(args[2]),bone.data(),angles.data(),static_cast<int>(args[5]),
            static_cast<int>(args[6]),static_cast<int>(args[7]),static_cast<int>(args[8]),
            static_cast<int>(args[10]),static_cast<int>(args[11]))?qtrue:qfalse;
        return true;
    }
    case Call::CG_G2_PLAYANIM: {
        CL_JKACgameGhoul2Native();
        const auto identity=static_cast<std::uint32_t>(args[1]);
        const auto *instance=CL_JKACgameGhoul2Resolve(identity);
        result=qfalse;
        if(!instance||!instance->At(static_cast<int>(args[2])))return true;
        const auto bone=CL_JKACgameGhoul2Text(args[3]);
        // Retail 303:0x413afc ->0x4df310 preserves words 7 and 9 as
        // float encodings. The actual owned Store supplies timing and pose
        // state; cgame geometry still stops at its renderer boundary.
        const std::uint32_t speedBits=static_cast<std::uint32_t>(args[7]);
        const std::uint32_t seedBits=static_cast<std::uint32_t>(args[9]);
        float speed,seed;
        std::memcpy(&speed,&speedBits,sizeof(speed));
        std::memcpy(&seed,&seedBits,sizeof(seed));
        if(!fntech3::ghoul2::FiniteScalar(speed)||!fntech3::ghoul2::FiniteScalar(seed))return true;
        // 0x4df326..0x4df36f bounds the integer frames and finite seed.
        const int start=args[4]>=0&&args[4]<100000?static_cast<int>(args[4]):0;
        const int end=args[5]>0&&args[5]<100000?static_cast<int>(args[5]):1;
        if(seed!=-1.0f&&(seed<0.0f||seed>100000.0f))seed=0.0f;
        auto &store=CL_JKACgameGhoul2Store();
        int flags=static_cast<int>(args[6]),blendTime=static_cast<int>(args[10]);
        // The owned store normalizes inactive prior clips and retains the
        // measured frozen sample for active forward transitions. The pose
        // boundary still refuses controls outside that measured subset.
        result=store.SetBoneAnim(reinterpret_cast<void *>(std::uintptr_t(identity)),
            static_cast<int>(args[2]),bone.data(),start,end,
            flags,speed,static_cast<int>(args[8]),seed,blendTime)?qtrue:qfalse;
        return true;
    }
    case Call::CG_G2_SETBOLTON: {
        CL_JKACgameGhoul2Native();
        const auto identity = static_cast<std::uint32_t>(args[1]);
        // Retail 0x4df940 stores the word as the model's link (+0x3c). The
        // owned Store poses and draws a linked model from its parent's bolt.
        if (CL_JKACgameGhoul2Resolve(identity))
            CL_JKACgameGhoul2Store().SetBoltInfo(reinterpret_cast<void *>(std::uintptr_t(identity)),
                static_cast<int>(args[2]), static_cast<int>(args[3]));
        // Retail0x413d56 returns zero for this void setter, including no model.
        result = 0;
        return true;
    }
    case Call::CG_G2_ATTACHINSTANCETOENTNUM:
    case Call::CG_G2_CLEARATTACHEDINSTANCE:
    case Call::CG_G2_CLEANENTATTACHMENTS:
        CL_JKACgameGhoul2Native();
        // Retail 339..341 all dispatch to0x4142b3: return zero without
        // reading another word or maintaining a listen-server pose cache.
        // Fn keeps each module's poses owned. Models linked within one
        // instance are posed by its own Store, never a borrowed server pose.
        result=0;
        return true;
    case Call::CG_G2_GETBONEFRAME: {
        CL_JKACgameGhoul2Native();
        if (cgvm->index != VM_CGAME) Com_Error(ERR_DROP, "Academy bone-frame query requires its native cgame context");
        const auto identity = static_cast<std::uint32_t>(args[1]);
        const auto *instance = CL_JKACgameGhoul2Resolve(identity);
        if (!instance) Com_Error(ERR_DROP, "Academy bone-frame query requires an owned model instance");
        const auto *model = instance->At(static_cast<int>(args[6]));
        result = qfalse;
        if (!model) return true;
        if (model->skeleton.numFrames <= 0)
            Com_Error(ERR_DROP, "Academy embedded-skeleton bone-frame queries are not served yet");
        const auto bone = CL_JKACgameGhoul2Text(args[2]);
        if (fntech3::ghoul2::detail::BoneNamed(*model, bone.data()) < 0) return true;
        void *destination = CL_JKACgameSpan(args[4], sizeof(float));
        const auto *playing = CL_JKACgameGhoul2Store().BoneAnimation(
            reinterpret_cast<void *>(std::uintptr_t(identity)), static_cast<int>(args[6]), bone.data());
        float frame = 0.0f;
        // Primary0x4e368d uses0x18; known inactive bones publish0 then
        // return false, while an unknown name never reaches publication.
        if (playing && (playing->flags & 0x18)) {
            if (!jkaCgameBoneClocks.FrameReady() || jkaCgameBoneClocks.LocalServer())
                Com_Error(ERR_DROP, "Academy animated bone-frame queries require the measured remote-client clock");
            if (!fntech3::ghoul2::FiniteScalar(playing->speed) || playing->speed < 0.0f
                || playing->startFrame < 0 || playing->endFrame <= playing->startFrame
                || playing->endFrame > model->skeleton.numFrames || (playing->flags & ~0xd8))
                Com_Error(ERR_DROP, "Academy bone-frame query is outside the served forward animation boundary "
                    "(flags=0x%lx, range=%d..%d, frames=%d, speed=%g, clock=%d, start=%d)",
                    static_cast<unsigned long>(playing->flags), playing->startFrame, playing->endFrame,
                    model->skeleton.numFrames, double(playing->speed), jkaCgameBoneClocks.Time(), playing->startTime);
            const auto elapsed = std::int64_t(jkaCgameBoneClocks.Time()) - playing->startTime;
            if (elapsed < INT32_MIN || elapsed > INT32_MAX)
                Com_Error(ERR_DROP, "Academy bone-frame clock subtraction wrap is not served yet");
            // Complete timing0x497290..0x4976c1 reads loop/freeze,
            // never blend duration or the prior clip. Flag0x80 can answer
            // this metadata query while actual pose blending stays refused.
            const double progress = playing->startFrame
                + std::max(0.0, double(elapsed)) / fntech3::ghoul2::FrameMilliseconds * playing->speed;
            if (progress > (std::numeric_limits<float>::max)())
                Com_Error(ERR_DROP, "Academy bone-frame progression exceeds float32");
            if (!(playing->flags & 0x10) && !(playing->flags & 0x40)
                && progress > playing->endFrame - 1)
                Com_Error(ERR_DROP, "Academy animation completion without loop/freeze is not served yet");
            // Wrapper0x4df400 selects the same clocks as bolt queries.
            // It replaces caller time word3; complete core/index ranges
            // never consume caller model-list word5.
            const auto head = fntech3::ghoul2::PlayheadAt(*playing, model->skeleton.numFrames,
                jkaCgameBoneClocks.Time());
            frame = static_cast<float>(head.currentFrame) + head.lerp;
            if (!fntech3::ghoul2::FiniteScalar(frame))
                Com_Error(ERR_DROP, "Invalid Academy bone-frame result");
            result = qtrue;
        }
        std::memcpy(destination, &frame, sizeof(frame));
        return true;
    }
    case Call::CG_G2_GETTIME:
        CL_JKACgameGhoul2Native();
        if (cgvm->index != VM_CGAME) Com_Error(ERR_DROP, "Academy bolt clocks require their native cgame context");
        result = jkaCgameBoneClocks.Time();
        return true;
    case Call::CG_G2_SETTIME:
        CL_JKACgameGhoul2Native();
        if (cgvm->index != VM_CGAME) Com_Error(ERR_DROP, "Academy bolt clocks require their native cgame context");
        if (args[1] < INT32_MIN || args[1] > INT32_MAX || args[2] < 0 || args[2] > 1)
            Com_Error(ERR_DROP, "Invalid Academy cgame Ghoul2 clock request");
        jkaCgameBoneClocks.Set(static_cast<int>(args[2]), static_cast<std::int32_t>(args[1]));
        result = 0;
        return true;
    case Call::CG_G2_GETBOLT_NOREC_NOROT:
    case Call::CG_G2_GETBOLT: {
        CL_JKACgameGhoul2Native();
        if (cgvm->index != VM_CGAME) Com_Error(ERR_DROP, "Academy bolt records require their native cgame context");
        if (call == Call::CG_G2_GETBOLT_NOREC_NOROT) jkaCgameNoRotationPending = true;
        using namespace fntech3::ghoul2;
        void *destination = CL_JKACgameSpan(args[4], sizeof(fntech3_jka_mp_mdxaBone_t));
        std::array<float,3> angles, position;
        std::memcpy(angles.data(), CL_JKACgameSpan(args[5], sizeof(angles)), sizeof(angles));
        std::memcpy(position.data(), CL_JKACgameSpan(args[6], sizeof(position)), sizeof(position));
        for (int axis = 0; axis < 3; ++axis)
            if (!pose::Finite(angles[axis]) || !pose::Finite(position[axis]))
                Com_Error(ERR_DROP, "Invalid Academy cgame bolt placement vector");
        const auto identity = static_cast<std::uint32_t>(args[1]);
        const auto *instance = CL_JKACgameGhoul2Resolve(identity);
        if (!instance) Com_Error(ERR_DROP, "Academy cgame bolt query requires an owned model instance");
        const auto *model = instance->At(static_cast<int>(args[2]));
        pose::Matrix world;
        if (!model || args[3] < 0 || static_cast<std::size_t>(args[3]) >= model->bolts.size()) {
            // Retail0x4dfc1d multiplies its world matrix by the fixed MP
            // fallback before returning false. This is real failure output.
            if (!pose::FailureBolt(angles.data(), position.data(), world.data()))
                Com_Error(ERR_DROP, "Invalid Academy failed-bolt placement");
            std::memcpy(destination, world.data(), sizeof(world));
            result = qfalse;
            return true;
        }
        if (!jkaCgameBoneClocks.FrameReady() || jkaCgameBoneClocks.LocalServer())
            Com_Error(ERR_DROP, "Academy cgame bolt queries require the measured remote-client frame clock; local-server clock sharing is not served yet");
        std::array<float,3> scale;
        std::memcpy(scale.data(), CL_JKACgameSpan(args[9], sizeof(scale)), sizeof(scale));
        for (float component : scale)
            if (!pose::Finite(component)) Com_Error(ERR_DROP, "Invalid Academy cgame bolt scale");
        pose::Matrix raw;
        // The complete primary core0x4dfa00..0x4dfc3a reads its clocks,
        // not native time word7, and never consumes model-list word8. It
        // builds the instance's whole skeleton first (0x49a050), so a linked
        // model answers from its parent's bolt. A model retail's sort never
        // reaches would answer from whatever bone cache it last had; that
        // stale state is not reproduced and refuses by name.
        const auto posing = CL_JKACgameGhoul2Store().LinkedBolt(reinterpret_cast<void *>(std::uintptr_t(identity)),
            static_cast<int>(args[2]), static_cast<int>(args[3]), jkaCgameBoneClocks.Time(), raw.data());
        if (posing == fntech3::ghoul2::Posing::Unserved)
            Com_Error(ERR_DROP, "Academy cgame bolt pose is outside its served hierarchy/animation boundary");
        if (posing != fntech3::ghoul2::Posing::Served)
            Com_Error(ERR_DROP, "Academy cgame bolt query on model %d is %s (link=%d)", static_cast<int>(args[2]),
                fntech3::ghoul2::PosingName(posing), model->boltInfo);
        if (!pose::PlaceBolt(raw.data(), angles.data(), position.data(), scale.data(), jkaCgameNoRotationPending, world.data()))
            Com_Error(ERR_DROP, "Invalid Academy cgame bolt result");
        // Primary0x4dfc07 clears its no-rotation selector on success only.
        jkaCgameNoRotationPending = false;
        std::memcpy(destination, world.data(), sizeof(world));
        result = qtrue;
        return true;
    }
    case Call::CG_G2_ADDBOLT: {
        CL_JKACgameGhoul2Native();
        const auto identity = static_cast<std::uint32_t>(args[1]);
        const auto *instance = CL_JKACgameGhoul2Resolve(identity);
        result = -1;
        if (!instance || !instance->At(static_cast<int>(args[2]))) return true;
        const auto name = CL_JKACgameGhoul2Text(args[3]);
        // Retail0x413cf5 maps only the name; the model identity is direct.
        // Store records a real bone/surface lookup; a model linked to it is
        // posed and drawn from that bolt.
        result = CL_JKACgameGhoul2Store().AddBolt(reinterpret_cast<void *>(std::uintptr_t(identity)),
            static_cast<int>(args[2]), name.data());
        return true;
    }
    case Call::CG_G2_COPYSPECIFICGHOUL2MODEL: {
        CL_JKACgameGhoul2Native();
        result = 0;
        const auto from = static_cast<std::uint32_t>(args[1]);
        const auto to = static_cast<std::uint32_t>(args[3]);
        const auto *source = CL_JKACgameGhoul2Resolve(from);
        if (!source) return true;
        const auto *destination = CL_JKACgameGhoul2Resolve(to);
        if (!destination || !source->At(static_cast<int>(args[2]))) return true;
        const int index = static_cast<int>(args[4]);
        if (index > destination->Count() && index < fntech3::ghoul2::MaxModelsPerInstance)
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 sparse model-copy destinations are not served yet");
        // Retail 0x413c13 forwards opaque identities and scalar indices,
        // rather than pointer slots, and returns zero for this void import.
        if (CL_JKACgameGhoul2Store().CopyModel(reinterpret_cast<void *>(std::uintptr_t(from)),
            static_cast<int>(args[2]), reinterpret_cast<void *>(std::uintptr_t(to)), index))
            CL_JKACgameSkinCopyModel(from, static_cast<int>(args[2]), to, index);
        return true;
    }
    case Call::CG_G2_DUPLICATEGHOUL2INSTANCE: {
        CL_JKACgameGhoul2Native();
        void *slot = CL_JKACgameSpan(args[2], sizeof(std::uint32_t));
        std::uint32_t destination;
        std::memcpy(&destination, slot, sizeof(destination));
        result = 0;
        // Retail 0x4e0590..0x4e05c8 preserves a populated destination and
        // never reads the source on that branch. It does not replace a model.
        if (destination) {
            CL_JKACgameGhoul2Resolve(destination);
            return true;
        }
        const auto source = static_cast<std::uint32_t>(args[1]);
        if (!CL_JKACgameGhoul2Resolve(source)) return true;
        auto &store = CL_JKACgameGhoul2Store();
        if (store.InstanceCount() >= 1024)
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 duplication exceeds its owned model budget");
        void *copy = nullptr;
        if (!store.Duplicate(reinterpret_cast<void *>(std::uintptr_t(source)), &copy))
            Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 cannot duplicate its owned model");
        destination = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(copy));
        CL_JKACgameSkinDuplicateModel(source, destination);
        // Publish only after both the actual model records and per-model
        // visibility bindings belong to the new identity. File bytes remain
        // immutable data owned by this same Store, separate from UI/server.
        std::memcpy(slot, &destination, sizeof(destination));
        return true;
    }
    case Call::CG_G2_CLEANMODELS: {
        CL_JKACgameGhoul2Native();
        void *slot = CL_JKACgameSpan(args[1], sizeof(std::uint32_t));
        std::uint32_t identity;
        std::memcpy(&identity, slot, sizeof(identity));
        CL_JKACgameGhoul2Resolve(identity);
        if (identity) {
            void *handle = reinterpret_cast<void *>(std::uintptr_t(identity));
            if (!CL_JKACgameGhoul2Store().Clean(&handle))
                Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 cleanup failed");
        }
        CL_JKACgameSkinCleanModel(identity);
        identity = 0;
        std::memcpy(slot, &identity, sizeof(identity));
        result = 0;
        return true;
    }
    case Call::CG_G2_HAVEWEGHOULMODELS:
    case Call::CG_G2_HASGHOUL2MODELONINDEX: {
        CL_JKACgameGhoul2Native();
        std::uint32_t identity = static_cast<std::uint32_t>(args[1]);
        if (call == Call::CG_G2_HASGHOUL2MODELONINDEX)
            std::memcpy(&identity, CL_JKACgameSpan(args[1], sizeof(identity)), sizeof(identity));
        const auto *instance = CL_JKACgameGhoul2Resolve(identity);
        result = instance && (call == Call::CG_G2_HAVEWEGHOULMODELS ? instance->HoldsAny() :
            instance->At(static_cast<int>(args[2])) != nullptr) ? qtrue : qfalse;
        return true;
    }
    case Call::CG_G2_GETGLANAME: {
        CL_JKACgameGhoul2Native();
        const auto *instance = CL_JKACgameGhoul2Resolve(static_cast<std::uint32_t>(args[1]));
        const auto *model = instance ? instance->At(static_cast<int>(args[2])) : nullptr;
        result = 0;
        if (!model) return true; // Retail's null GLA name leaves the caller buffer unchanged.
        std::string name = model->skeletonName;
        if (!name.empty() && name[0] != '*' && name.size() >= 4) name.resize(name.size() - 4);
        if (name.size() >= MAX_QPATH) Com_Error(ERR_DROP, "Jedi Academy cgame Ghoul2 GLA name exceeds MAX_QPATH");
        void *output = CL_JKACgameSpan(args[3], name.size() + 1);
        std::memcpy(output, name.c_str(), name.size() + 1);
        return true;
    }
    default: return false;
    }
}
} // namespace
#endif
