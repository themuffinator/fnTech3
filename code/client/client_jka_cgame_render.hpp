/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_RENDER_HPP
#define FNTECH3_CLIENT_JKA_CGAME_RENDER_HPP
#include "client_jka_mdr_identity.hpp"

/* Each owned paired-GLA client model uses the same Fn MDR material, lighting
 * and shadow paths as the UI bridge. Skin visibility comes from the actual
 * cgame registration/binding. Surface flag meanings and descendant traversal
 * are separately pinned OpenJK format/contract observations (see record);
 * no renderer implementation text is incorporated. LOD0 is retained for this
 * bounded world-model slice.
 *
 * An instance's models are posed and drawn as retail jamp.exe 1.01's renderer
 * does (0x499bf0): in its model sort (0x498be0), a model of its own from the
 * root basis, a linked one -- a weapon in the right hand -- from its parent's
 * bolt (0x499850) with its own skeleton composed onto that. A model the sort
 * never reaches is not drawn. Each model becomes its own Fn skeletal entity
 * with the instance's placement, so lighting, shadows and materials apply to
 * it as to the body. Embedded skeletons, model scaling and retail's
 * frame-to-frame bone smoothing remain outside this path. */
namespace {
static const fntech3::ghoul2::Instance &CL_JKACgameRenderInstance(std::uint32_t identity) {
    const auto *instance=CL_JKACgameGhoul2Resolve(identity);
    if (!instance || instance->ServerOnly())
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer requires an owned client instance (owned=%d, serverOnly=%d)",
            instance!=nullptr,instance&&instance->ServerOnly());
    auto &order=fntech3::jka::cgame::scene::state.order;
    fntech3::ghoul2::PoseOrder(*instance,order);
    for (int index:order) {
        const auto *model=instance->At(index);
        if (!model->meshBytes || !model->skeletonBytes)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer requires paired-GLA models (model %d: mesh=%s, skeleton=%s, meshBytes=%d, skeletonBytes=%d)",
                index,model->name.c_str(),model->skeletonName.c_str(),model->meshBytes!=nullptr,model->skeletonBytes!=nullptr);
        if (model->customSkin && (!jkaCgameSkins || !jkaCgameSkins->definitions.count(model->customSkin)))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 custom skin has no owned Fn declaration (model %d)",index);
    }
    return *instance;
}

static void CL_JKACgameRenderQueue(const fntech3_jka_mp_refEntity_t &source) {
    CL_JKACgameGhoul2Native();
    if (cgvm->index!=VM_CGAME || !source.ghoul2)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 scene requires its owned native cgame model");
    auto plain=source; plain.ghoul2=0;
    refEntity_t canonical{};
    const auto status=fntech3::jka::render::Entity(plain,canonical);
    if (status!=fntech3::jka::render::Status::Ok)
        Com_Error(ERR_DROP,"Academy cgame %s",fntech3::jka::render::StatusName(status));
    if (source.reType!=0 || source.customSkin || source.skinNum || source.uRefEnt.miniCount)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 entity skin overrides, mini chains or non-model entities are not served yet");
    if (!fntech3::jka::render::Finite(source.angles) || !fntech3::jka::render::Finite(source.modelScale))
        Com_Error(ERR_DROP,"Invalid Academy cgame Ghoul2 placement");
    for (float scale:source.modelScale) if (scale!=0 && scale!=1)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 nontrivial modelScale is not served yet");
    CL_JKACgameRenderInstance(source.ghoul2);
    auto &state=fntech3::jka::cgame::scene::state;
    if (state.pending.size()>=fntech3::jka::cgame::scene::MaxModels)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 scene exceeds its entity budget");
    state.pending.push_back(source);
}

static fntech3::jka::cgame::scene::Runtime &CL_JKACgameRenderGeometry(std::uint32_t identity, int index,
    const fntech3::ghoul2::Model &model)
{
    auto &state=fntech3::jka::cgame::scene::state;
    for (auto at=state.models.begin();at!=state.models.end();) {
        const auto *owner=jkaCgameGhoul2Store->Resolve(reinterpret_cast<void *>(std::uintptr_t(at->first.first)));
        if (!owner || !owner->At(at->first.second))
            at=state.models.erase(at);
        else ++at;
    }
    const fntech3::jka::cgame::scene::Key key{identity,index};
    auto at=state.models.find(key);
    if (at!=state.models.end() && at->second.meshBytes!=model.meshBytes) {
        state.models.erase(at); at=state.models.end();
    }
    if (at==state.models.end()) {
        if (state.models.size()>=fntech3::jka::cgame::scene::MaxModels)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer cache is full");
        at=state.models.emplace(key,fntech3::jka::cgame::scene::Runtime{}).first;
        const auto error=fntech3::ghoul2::geometry::Load(model.meshBytes->data(),model.meshBytes->size(),
            model.skeleton.numBones,0,at->second.geometry);
        if (error!=fntech3::ghoul2::geometry::Error::None)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::geometry::ErrorName(error));
        at->second.meshBytes=model.meshBytes;
    }
    auto &runtime=at->second;
    const JKACgameSkinBinding *binding=nullptr;
    if (jkaCgameSkins) {
        const auto found=jkaCgameSkins->bindings.find(key);
        if (found!=jkaCgameSkins->bindings.end()) binding=&found->second;
    }
    if (binding) {
        // Render skin 0 is a binding holding only import 321's overrides.
        if (binding->renderSkin && !jkaCgameSkins->definitions.count(binding->renderSkin))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 visibility skin is stale");
        for (const auto &entry:binding->overrides)
            if (entry.first<0 || std::size_t(entry.first)>=runtime.geometry.surfaces.size())
                Com_Error(ERR_DROP,"Invalid Academy cgame Ghoul2 visibility surface");
    }
    state.visible.clear(); state.traversal.clear();
    for (auto root=runtime.geometry.roots.rbegin();root!=runtime.geometry.roots.rend();++root)
        state.traversal.push_back(*root);
    while (!state.traversal.empty()) {
        const int surfaceIndex=state.traversal.back(); state.traversal.pop_back();
        const auto &surface=runtime.geometry.surfaces[std::size_t(surfaceIndex)];
        auto flags=surface.metadata.flags;
        if (binding) {
            const auto found=binding->overrides.find(surfaceIndex);
            if (found!=binding->overrides.end()) flags=found->second;
        }
        if (flags&~std::uint32_t(0x103))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 unsupported surface visibility flags");
        if (!flags && !surface.triangles.empty()) state.visible.push_back(surfaceIndex);
        if (!(flags&0x100))
            for (auto child=surface.metadata.children.rbegin();child!=surface.metadata.children.rend();++child)
                state.traversal.push_back(*child);
    }
    if (runtime.visible!=state.visible) {
        runtime.visible=state.visible;
        runtime.handle=0; runtime.owner=0; // New topology requires a new owned registration.
    }
    return runtime;
}

static void CL_JKACgameRenderFlush() {
    auto &state=fntech3::jka::cgame::scene::state;
    if (state.pending.empty()) return;
    if (!jkaCgameBoneClocks.FrameReady())
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 rendering requires a published frame clock");
    if (!re.RegisterRuntimeMDR || !re.UpdateRuntimeMDRPose)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 Fn skeletal renderer is unavailable");
    state.prepared.clear(); state.prepared.reserve(state.pending.size());
    const int now=jkaCgameBoneClocks.Time();
    for (const auto &source:state.pending) {
        const auto &instance=CL_JKACgameRenderInstance(source.ghoul2);
        int failed=-1;
        const auto posing=CL_JKACgameGhoul2Store().InstancePalettes(instance,now,state.order,state.palettes,failed);
        if (posing==fntech3::ghoul2::Posing::Unserved)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 model %d animation, bone-angle mode or parent tag is not served by the pose evaluator",failed);
        if (posing!=fntech3::ghoul2::Posing::Served) {
            const auto *model=instance.At(failed);
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 model %d is %s (link=%d)",failed,
                fntech3::ghoul2::PosingName(posing),model?model->boltInfo:0);
        }
        auto plain=source; plain.ghoul2=0;
        refEntity_t canonical{};
        if (fntech3::jka::render::Entity(plain,canonical)!=fntech3::jka::render::Status::Ok)
            Com_Error(ERR_DROP,"Invalid Academy cgame Ghoul2 canonical entity");
        canonical.frame=canonical.oldframe=0; canonical.backlerp=0;
        for (std::size_t at=0;at<state.order.size();++at) {
            const int index=state.order[at];
            const auto &model=*instance.At(index);
            auto &runtime=CL_JKACgameRenderGeometry(source.ghoul2,index,model);
            if (runtime.visible.empty()) continue;
            const auto &pose=state.palettes[at];
            if (!runtime.handle) {
                state.bytes.clear();
                const auto error=fntech3::ghoul2::mdr::Build(runtime.geometry,runtime.visible,pose.deformation,state.bytes);
                if (error!=fntech3::ghoul2::mdr::Error::None)
                    Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::mdr::ErrorName(error));
                const auto owner=fntech3::jka::mdr::NextOwner();
                if (!owner) Com_Error(ERR_DROP,"Academy Ghoul2 renderer identity space is exhausted");
                const auto handle=re.RegisterRuntimeMDR(owner,state.bytes.data(),static_cast<int>(state.bytes.size()));
                if (!handle) Com_Error(ERR_DROP,"Academy cgame Ghoul2 Fn skeletal registration failed");
                runtime.owner=owner; runtime.handle=handle;
            } else {
                fntech3::ghoul2::geometry::Vector mins,maxs;
                const auto error=fntech3::ghoul2::mdr::Bounds(runtime.geometry,runtime.visible,pose.deformation,mins,maxs);
                if (error!=fntech3::ghoul2::mdr::Error::None)
                    Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::mdr::ErrorName(error));
                state.bones.clear(); state.bones.reserve(pose.deformation.size()*12);
                for (const auto &bone:pose.deformation) state.bones.insert(state.bones.end(),bone.begin(),bone.end());
                if (!re.UpdateRuntimeMDRPose(runtime.handle,runtime.owner,state.bones.data(),
                    static_cast<int>(pose.deformation.size()),mins.data(),maxs.data()))
                    Com_Error(ERR_DROP,"Academy cgame Ghoul2 pose is stale, invalid or conflicts with an unrendered scene");
            }
            auto entity=canonical;
            entity.hModel=runtime.handle;
            entity.customSkin=model.customSkin;
            state.prepared.push_back(entity);
        }
    }
    // Publish only after every pending entity has an owned, validated pose.
    for (const auto &entity:state.prepared) CL_AddEnemyHighlightRefEntity(&entity,qfalse);
    state.ClearScene();
}
} // namespace
#endif
