/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_UI_RENDER_HPP
#define FNTECH3_CLIENT_JKA_UI_RENDER_HPP

#include "../ghoul2/ghoul2_mdr.hpp"
#include "client_jka_mdr_identity.hpp"
#include <unordered_map>

/* Engine-owned UI staging. Evaluate at RenderScene using the retail-selected
 * owned UI clock, rather than at AddRefEntity's earlier call. Generated MDR
 * geometry enters the existing Fn
 * skeletal material, lighting and shadow paths. No module address survives
 * staging; its four-byte identity is resolved again before every evaluation.
 * Default visibility: pinned OpenJK tr_ghoul2.cpp::RenderSurfaces uses flags
 * zero for drawing, and descends through default bolt/off surfaces. Custom
 * skins, attachments and nontrivial modelScale remain explicit boundaries. */
namespace {
struct JKAUIRuntimeModel {
	fntech3::ghoul2::geometry::Geometry geometry;
	std::vector<int> visible;
	qhandle_t handle = 0;
	std::uint64_t owner = 0;
};
std::unordered_map<std::uint32_t, JKAUIRuntimeModel> jkaUIRuntimeModels;
std::vector<fntech3_jka_mp_refEntity_t> jkaUIPendingModels;

static void CL_JKAUIRenderClear() {
	jkaUIPendingModels.clear();
	jkaUIRuntimeModels.clear();
	// The nonce spans module and renderer lifetimes. A recycled renderer handle
	// never authorizes a stale owner to write another model's matrices.
}

static void CL_JKAUIRenderClearScene() { jkaUIPendingModels.clear(); }

// The UI serves neither UI_G2_ADDBOLT nor UI_G2_SETBOLTON, so no UI model can
// carry a link; multi-model UI instances stay a named boundary.
static const fntech3::ghoul2::Model &CL_JKAUIRenderModel(std::uint32_t identity) {
	const auto *instance = CL_JKAUIGhoul2Resolve(identity);
	if (!instance || instance->ServerOnly() || instance->Count() != 1)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 renderer requires one owned client model; attachments remain open");
	const auto *model = instance->At(0);
	if (!model || !model->meshBytes || !model->skeletonBytes
		|| model->boltInfo != fntech3::ghoul2::link::None || model->customSkin)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 renderer requires an unbolted paired-GLA model with default visibility");
	return *model;
}

static void CL_JKAUIRenderQueue(const fntech3_jka_mp_refEntity_t &source) {
	CL_JKAUIGhoul2Native();
	fntech3_jka_mp_refEntity_t plain = source;
	plain.ghoul2 = 0;
	refEntity_t canonical{};
	const auto status = fntech3::jka::render::Entity(plain, canonical);
	if (status != fntech3::jka::render::Status::Ok)
		Com_Error(ERR_DROP, "Jedi Academy UI %s", fntech3::jka::render::StatusName(status));
	if (source.reType != 0 || source.customSkin || source.skinNum || source.uRefEnt.miniCount)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 custom skins, mini chains or non-model entities are not served yet");
	if (!fntech3::jka::render::Finite(source.angles) || !fntech3::jka::render::Finite(source.modelScale))
		Com_Error(ERR_DROP, "Invalid Jedi Academy UI Ghoul2 placement");
	for (float scale : source.modelScale) if (scale != 0.0f && scale != 1.0f)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 nontrivial modelScale is not served yet");
	CL_JKAUIRenderModel(source.ghoul2);
	if (jkaUIPendingModels.size() >= 1024)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 scene exceeds the entity budget");
	jkaUIPendingModels.push_back(source);
}

static JKAUIRuntimeModel &CL_JKAUIRenderGeometry(std::uint32_t identity,
	const fntech3::ghoul2::Model &model)
{
	// Cleaned identities cannot keep growing the UI's owned geometry cache.
	for (auto it = jkaUIRuntimeModels.begin(); it != jkaUIRuntimeModels.end();) {
		if (!CL_JKAUIGhoul2Store().Resolve(reinterpret_cast<void *>(std::uintptr_t(it->first))))
			it = jkaUIRuntimeModels.erase(it);
		else ++it;
	}
	const auto found = jkaUIRuntimeModels.find(identity);
	if (found != jkaUIRuntimeModels.end()) return found->second;
	if (jkaUIRuntimeModels.size() >= 1024)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 renderer cache is full");
	JKAUIRuntimeModel staged;
	const auto error = fntech3::ghoul2::geometry::Load(model.meshBytes->data(), model.meshBytes->size(),
		model.skeleton.numBones, 0, staged.geometry);
	if (error != fntech3::ghoul2::geometry::Error::None)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 %s", fntech3::ghoul2::geometry::ErrorName(error));
	for (const auto &surface : staged.geometry.surfaces) {
		if (surface.metadata.flags & ~std::uint32_t(3))
			Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 unsupported default surface flags");
		if (!surface.metadata.flags && !surface.triangles.empty()) staged.visible.push_back(surface.metadata.index);
	}
	return jkaUIRuntimeModels.emplace(identity, std::move(staged)).first->second;
}

static void CL_JKAUIRenderFlush(int sceneTime) {
	if (jkaUIPendingModels.empty()) return;
	(void)sceneTime; // Retail G2 GetTime ignores its argument and selects a clock.
	const int poseTime = CL_JKAUIGhoul2Time();
	if (!re.RegisterRuntimeMDR || !re.UpdateRuntimeMDRPose)
		Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 skeletal renderer is unavailable");
	std::vector<fntech3_jka_mp_refEntity_t> pending;
	pending.swap(jkaUIPendingModels);
	std::vector<refEntity_t> prepared;
	prepared.reserve(pending.size());
	for (const auto &source : pending) {
		const auto &model = CL_JKAUIRenderModel(source.ghoul2);
		auto &runtime = CL_JKAUIRenderGeometry(source.ghoul2, model);
		if (runtime.visible.empty()) continue;
		fntech3::ghoul2::pose::Palette pose;
		if (!CL_JKAUIGhoul2Store().DeformationPalette(model, poseTime, pose))
			Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 animation or bone-angle mode is not served by the pose evaluator");
		// Only registration serializes geometry. Later poses inspect weighted
		// arithmetic/bounds and update the instance's frame in place.
		if (!runtime.handle) {
			std::vector<unsigned char> bytes;
			const auto error = fntech3::ghoul2::mdr::Build(runtime.geometry, runtime.visible, pose.deformation, bytes);
			if (error != fntech3::ghoul2::mdr::Error::None)
				Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 cannot construct its Fn skeletal model (%d)", static_cast<int>(error));
			const auto owner = fntech3::jka::mdr::NextOwner();
			if (!owner)
				Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 renderer identity space is exhausted");
			const auto handle = re.RegisterRuntimeMDR(owner, bytes.data(), static_cast<int>(bytes.size()));
			if (!handle) Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 Fn skeletal model registration failed");
			runtime.owner = owner;
			runtime.handle = handle;
		} else {
			fntech3::ghoul2::geometry::Vector mins, maxs;
			const auto error = fntech3::ghoul2::mdr::Bounds(runtime.geometry, runtime.visible, pose.deformation, mins, maxs);
			if (error != fntech3::ghoul2::mdr::Error::None)
				Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 cannot bound its Fn skeletal pose (%d)", static_cast<int>(error));
			std::vector<float> bones;
			bones.reserve(pose.deformation.size() * 12);
			for (const auto &matrix : pose.deformation) bones.insert(bones.end(), matrix.begin(), matrix.end());
			if (!re.UpdateRuntimeMDRPose(runtime.handle, runtime.owner, bones.data(),
				static_cast<int>(pose.deformation.size()), mins.data(), maxs.data()))
				Com_Error(ERR_DROP, "Jedi Academy UI Ghoul2 renderer pose is stale, invalid or conflicts with an unrendered scene");
		}
		fntech3_jka_mp_refEntity_t plain = source;
		plain.ghoul2 = 0;
		refEntity_t canonical{};
		if (fntech3::jka::render::Entity(plain, canonical) != fntech3::jka::render::Status::Ok)
			Com_Error(ERR_DROP, "Invalid Jedi Academy UI Ghoul2 canonical entity");
		canonical.hModel = runtime.handle;
		canonical.frame = canonical.oldframe = 0;
		canonical.backlerp = 0;
		prepared.push_back(canonical);
	}
	for (const auto &entity : prepared) re.AddRefEntityToScene(&entity, qfalse);
}
} // namespace
#endif
