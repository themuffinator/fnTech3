/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_RENDER_HPP
#define FNTECH3_CLIENT_JKA_CGAME_RENDER_HPP
#include "client_jka_mdr_identity.hpp"

/* An owned paired-GLA client model uses the same Fn MDR material, lighting
 * and shadow paths as the UI bridge. Skin visibility comes from the actual
 * cgame registration/binding. Surface flag meanings and descendant traversal
 * are separately pinned OpenJK format/contract observations (see record);
 * no renderer implementation text is incorporated. LOD0 is retained for this
 * bounded first world-model slice. Attachments, embedded skeletons, model
 * scaling and active animation transitions remain explicit boundaries. */
namespace {
static const fntech3::ghoul2::Model &CL_JKACgameRenderModel(std::uint32_t identity) {
    const auto *instance=CL_JKACgameGhoul2Resolve(identity);
    if (!instance || instance->ServerOnly() || instance->Count()!=1)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer requires one owned client model; attachments remain open");
    const auto *model=instance->At(0);
    if (!model || !model->meshBytes || !model->skeletonBytes || model->boltInfo)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer requires an unbolted paired-GLA model (mesh=%s, skeleton=%s, meshBytes=%d, skeletonBytes=%d, boltInfo=%d)",
            model?model->name.c_str():"<absent>",model?model->skeletonName.c_str():"<absent>",
            model&&model->meshBytes,model&&model->skeletonBytes,model?model->boltInfo:0);
    if (model->customSkin && (!jkaCgameSkins || !jkaCgameSkins->definitions.count(model->customSkin)))
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 custom skin has no owned Fn declaration");
    return *model;
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
    CL_JKACgameRenderModel(source.ghoul2);
    auto &state=fntech3::jka::cgame::scene::state;
    if (state.pending.size()>=fntech3::jka::cgame::scene::MaxModels)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 scene exceeds its entity budget");
    state.pending.push_back(source);
}

static fntech3::jka::cgame::scene::Runtime &CL_JKACgameRenderGeometry(std::uint32_t identity,
    const fntech3::ghoul2::Model &model)
{
    auto &state=fntech3::jka::cgame::scene::state;
    for (auto at=state.models.begin();at!=state.models.end();) {
        if (!jkaCgameGhoul2Store->Resolve(reinterpret_cast<void *>(std::uintptr_t(at->first))))
            at=state.models.erase(at);
        else ++at;
    }
    auto at=state.models.find(identity);
    if (at!=state.models.end() && at->second.meshBytes!=model.meshBytes) {
        state.models.erase(at); at=state.models.end();
    }
    if (at==state.models.end()) {
        if (state.models.size()>=fntech3::jka::cgame::scene::MaxModels)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 renderer cache is full");
        at=state.models.emplace(identity,fntech3::jka::cgame::scene::Runtime{}).first;
        const auto error=fntech3::ghoul2::geometry::Load(model.meshBytes->data(),model.meshBytes->size(),
            model.skeleton.numBones,0,at->second.geometry);
        if (error!=fntech3::ghoul2::geometry::Error::None)
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::geometry::ErrorName(error));
        at->second.meshBytes=model.meshBytes;
    }
    auto &runtime=at->second;
    const JKACgameSkinBinding *binding=nullptr;
    if (jkaCgameSkins) {
        const auto found=jkaCgameSkins->bindings.find({identity,0});
        if (found!=jkaCgameSkins->bindings.end()) binding=&found->second;
    }
    if (binding) {
        if (!jkaCgameSkins->definitions.count(binding->renderSkin))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 visibility skin is stale");
        for (const auto &entry:binding->overrides)
            if (entry.first<0 || std::size_t(entry.first)>=runtime.geometry.surfaces.size())
                Com_Error(ERR_DROP,"Invalid Academy cgame Ghoul2 visibility surface");
    }
    state.visible.clear(); state.traversal.clear();
    for (auto root=runtime.geometry.roots.rbegin();root!=runtime.geometry.roots.rend();++root)
        state.traversal.push_back(*root);
    while (!state.traversal.empty()) {
        const int index=state.traversal.back(); state.traversal.pop_back();
        const auto &surface=runtime.geometry.surfaces[std::size_t(index)];
        auto flags=surface.metadata.flags;
        if (binding) {
            const auto found=binding->overrides.find(index);
            if (found!=binding->overrides.end()) flags=found->second;
        }
        if (flags&~std::uint32_t(0x103))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 unsupported surface visibility flags");
        if (!flags && !surface.triangles.empty()) state.visible.push_back(index);
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
    if (!jkaCgameBoneClocks.FrameReady() || jkaCgameBoneClocks.LocalServer())
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 rendering requires the owned remote-client frame clock");
    if (!re.RegisterRuntimeMDR || !re.UpdateRuntimeMDRPose)
        Com_Error(ERR_DROP,"Academy cgame Ghoul2 Fn skeletal renderer is unavailable");
    state.prepared.clear(); state.prepared.reserve(state.pending.size());
    const int now=jkaCgameBoneClocks.Time();
    for (const auto &source:state.pending) {
        const auto &model=CL_JKACgameRenderModel(source.ghoul2);
        auto &runtime=CL_JKACgameRenderGeometry(source.ghoul2,model);
        if (runtime.visible.empty()) continue;
        state.pose={};
        if (!CL_JKACgameGhoul2Store().DeformationPalette(model,now,state.pose))
            Com_Error(ERR_DROP,"Academy cgame Ghoul2 animation transition or bone-angle mode is not served by the pose evaluator");
        if (!runtime.handle) {
            state.bytes.clear();
            const auto error=fntech3::ghoul2::mdr::Build(runtime.geometry,runtime.visible,state.pose.deformation,state.bytes);
            if (error!=fntech3::ghoul2::mdr::Error::None)
                Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::mdr::ErrorName(error));
            const auto owner=fntech3::jka::mdr::NextOwner();
            if (!owner) Com_Error(ERR_DROP,"Academy Ghoul2 renderer identity space is exhausted");
            const auto handle=re.RegisterRuntimeMDR(owner,state.bytes.data(),static_cast<int>(state.bytes.size()));
            if (!handle) Com_Error(ERR_DROP,"Academy cgame Ghoul2 Fn skeletal registration failed");
            runtime.owner=owner; runtime.handle=handle;
        } else {
            fntech3::ghoul2::geometry::Vector mins,maxs;
            const auto error=fntech3::ghoul2::mdr::Bounds(runtime.geometry,runtime.visible,state.pose.deformation,mins,maxs);
            if (error!=fntech3::ghoul2::mdr::Error::None)
                Com_Error(ERR_DROP,"Academy cgame Ghoul2 %s",fntech3::ghoul2::mdr::ErrorName(error));
            state.bones.clear(); state.bones.reserve(state.pose.deformation.size()*12);
            for (const auto &bone:state.pose.deformation) state.bones.insert(state.bones.end(),bone.begin(),bone.end());
            if (!re.UpdateRuntimeMDRPose(runtime.handle,runtime.owner,state.bones.data(),
                static_cast<int>(state.pose.deformation.size()),mins.data(),maxs.data()))
                Com_Error(ERR_DROP,"Academy cgame Ghoul2 pose is stale, invalid or conflicts with an unrendered scene");
        }
        auto plain=source; plain.ghoul2=0;
        refEntity_t canonical{};
        if (fntech3::jka::render::Entity(plain,canonical)!=fntech3::jka::render::Status::Ok)
            Com_Error(ERR_DROP,"Invalid Academy cgame Ghoul2 canonical entity");
        canonical.hModel=runtime.handle;
        canonical.customSkin=model.customSkin;
        canonical.frame=canonical.oldframe=0; canonical.backlerp=0;
        state.prepared.push_back(canonical);
    }
    // Publish only after every pending entity has an owned, validated pose.
    for (const auto &entity:state.prepared) CL_AddEnemyHighlightRefEntity(&entity,qfalse);
    state.ClearScene();
}
} // namespace
#endif
