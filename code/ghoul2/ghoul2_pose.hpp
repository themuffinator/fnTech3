/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_GHOUL2_POSE_HPP
#define FNTECH3_GHOUL2_POSE_HPP

/* Independently authored Ghoul2 pose arithmetic. Format facts are pinned to
 * OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9, observation only.
 * Parent-relative deformation, frame-zero defaults, inherited playheads and
 * angle controls reproduce retail jamp.exe 1.01 engine-query observations.
 * A deformation palette carries model-space vertices directly. Bone bolts
 * additionally apply the file's base pose; API placement is a separate step.
 * Nothing here owns a module handle, reads a file, caches a pose or draws. */
#include "../compat/jka_model_format.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace fntech3::ghoul2::pose {
namespace model = fntech3::jka::model;
inline constexpr int MaxBones = 256;
using Matrix = std::array<float, 12>;
inline constexpr Matrix RootBasis{0,-1,0,0,1,0,0,0,0,0,1,0};

inline bool Finite(float value) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	volatile std::uint32_t encoded = bits;
	return (encoded & 0x7f800000u) != 0x7f800000u;
}
inline bool Finite(const Matrix &matrix) {
	for (float value : matrix) if (!Finite(value)) return false;
	return true;
}
inline Matrix Compose(const Matrix &a, const Matrix &b) {
	Matrix out;
	model::ComposeMatrix(a.data(), b.data(), out.data());
	return out;
}
struct Frame {
	int current = 0, next = 0;
	float lerp = 0;
};
struct Animation {
	int bone = -1;
	Frame frame;
	Frame previous{};
	float transition = 1;
	bool blending = false;
};
struct Angle {
	int bone = -1;
	Matrix post;
	int mode = 2;
};
struct Palette {
	std::vector<Matrix> deformation;
	std::vector<Matrix> bones;
};

/* Pose validation is stronger than file inventory: indices alone cannot
 * establish an acyclic hierarchy, finite matrices or a usable inverse. */
inline bool Hierarchy(const void *data, std::size_t bytes,
	const model::Skeleton &skeleton, std::vector<int> &parents,
	std::vector<Matrix> &base, std::vector<Matrix> &inverse)
{
	if (!data || skeleton.numBones <= 0 || skeleton.numBones > MaxBones
		|| skeleton.numFrames <= 0) return false;
	model::Reader reader(data, bytes);
	if (std::size_t(skeleton.numFrames) > bytes / (std::size_t(skeleton.numBones)*model::FrameIndexBytes)) return false;
	if (!reader.Fits(0, model::SkeletonHeaderBytes)
		|| reader.Unsigned(0) != model::SkeletonIdent || reader.Int(4) != model::SkeletonVersion
		|| reader.Int(84) != skeleton.numBones || reader.Int(76) != skeleton.numFrames
		|| reader.Int(80) < 0 || std::size_t(reader.Int(80)) != skeleton.frameOffset
		|| reader.Int(88) < 0 || std::size_t(reader.Int(88)) != skeleton.poseOffset
		|| !reader.Fits(model::SkeletonHeaderBytes, 4, std::size_t(skeleton.numBones))
		|| !reader.Fits(skeleton.frameOffset, model::FrameIndexBytes,
			std::size_t(skeleton.numFrames)*std::size_t(skeleton.numBones))) return false;
	parents.resize(skeleton.numBones); base.resize(skeleton.numBones); inverse.resize(skeleton.numBones);
	std::vector<int> childParents(skeleton.numBones, -1);
	for (int i = 0; i < skeleton.numBones; ++i) {
		const int stated = reader.Int(model::SkeletonHeaderBytes + std::size_t(i)*4);
		if (stated < 0) return false;
		const std::size_t at = model::SkeletonHeaderBytes + std::size_t(stated);
		if (!reader.Fits(at, model::BoneBytes)) return false;
		parents[i] = reader.Int(at + 68);
		if ((i == 0 && parents[i] != -1) || (i != 0 && parents[i] < 0)
			|| parents[i] >= skeleton.numBones || parents[i] == i) return false;
		if (!model::BoneRestMatrix(data, bytes, skeleton, i, base[i].data())
			|| !model::BoneRestMatrixInverse(data, bytes, skeleton, i, inverse[i].data())
			|| !Finite(base[i]) || !Finite(inverse[i])) return false;
		const Matrix unit = Compose(base[i], inverse[i]);
		if (!Finite(unit)) return false;
		for (int n = 0; n < 12; ++n) {
			const float expected = n == 0 || n == 5 || n == 10 ? 1.0f : 0.0f;
			if (std::fabs(unit[n] - expected) > 0.01f) return false;
		}
		const int count = reader.Int(at + 168);
		if (count < 0 || count > skeleton.numBones
			|| !reader.Fits(at + model::BoneBytes, 4, std::size_t(count))) return false;
		for (int child = 0; child < count; ++child) {
			const int index = reader.Int(at + model::BoneBytes + std::size_t(child)*4);
			if (index <= 0 || index >= skeleton.numBones || childParents[index] != -1) return false;
			childParents[index] = i;
		}
	}
	for (int i = 1; i < skeleton.numBones; ++i) {
		if (childParents[i] != parents[i]) return false;
		int parent = i, steps = 0;
		while (parent != 0) {
			if (++steps >= skeleton.numBones) return false;
			parent = parents[parent];
		}
	}
	return true;
}

inline Matrix Rotation(const float angles[3]) {
	const double radians = 3.14159265358979323846 / 180.0;
	const float cp = float(std::cos(double(angles[0])*radians));
	const float sp = float(std::sin(double(angles[0])*radians));
	const float cy = float(std::cos(double(angles[1])*radians));
	const float sy = float(std::sin(double(angles[1])*radians));
	const float cr = float(std::cos(double(angles[2])*radians));
	const float sr = float(std::sin(double(angles[2])*radians));
	return {cp*cy, sr*sp*cy-cr*sy, cr*sp*cy+sr*sy, 0,
		cp*sy, sr*sp*sy+cr*cy, cr*sp*sy-sr*cy, 0, -sp, sr*cp, cr*cp, 0};
}

/* Raven orientations: +X,+Z,+Y,-X,-Z,-Y = 1..6. POSTMULT's orientation
 * remap and conjugation are needed even for zero angles on a turned bone. */
inline bool PostAngle(const Matrix &base, const Matrix &inverse,
	const float angles[3], int up, int left, int forward, Matrix &out)
{
	if (!angles || !Finite(base) || !Finite(inverse)) return false;
	for (int i = 0; i < 3; ++i) if (!Finite(angles[i])) return false;
	if (up < 1 || up > 6 || left < 1 || left > 6 || forward < 1 || forward > 6
		|| (up-1)%3 == (left-1)%3 || (up-1)%3 == (forward-1)%3
		|| (left-1)%3 == (forward-1)%3) return false;
	const float yaw[] = {angles[2], angles[1], angles[0], angles[2]+180, angles[1]+180, angles[0]};
	const float pitch[] = {angles[2]+180, angles[1], angles[0]+180, angles[2], angles[1], angles[0]};
	const float roll[] = {angles[2], angles[1]+180, angles[0]+180, angles[2], angles[1], angles[0]};
	const float remapped[] = {pitch[left-1], yaw[up-1], roll[forward-1]};
	for (float value : remapped) if (!Finite(value)) return false;
	const Matrix staged = Compose(base, Compose(Rotation(remapped), inverse));
	if (!Finite(staged)) return false;
	out = staged;
	return true;
}

inline bool AngleMatrix(const Matrix &base, const Matrix &inverse,
	const float angles[3], int mode, int up, int left, int forward, Matrix &out)
{
	if (mode == 1 || mode == 2) return PostAngle(base, inverse, angles, up, left, forward, out);
	if (mode != 4 || !angles) return false;
	for (int i = 0; i < 3; ++i) if (!Finite(angles[i])) return false;
	if (up < 1 || up > 6 || left < 1 || left > 6 || forward < 1 || forward > 6
		|| (up-1)%3 == (left-1)%3 || (up-1)%3 == (forward-1)%3
		|| (left-1)%3 == (forward-1)%3) return false;
	float remapped[3]{angles[0] + (left == 3 ? 180.0f : 0.0f), angles[1], angles[2]};
	for (float value : remapped) if (!Finite(value)) return false;
	Matrix permutation{};
	const int orientations[]{forward, left, up};
	const int rows[]{0,2,1};
	for (int column = 0; column < 3; ++column) {
		const int orientation = orientations[column];
		permutation[rows[(orientation-1)%3]*4+column] = orientation > 3 ? -1.0f : 1.0f;
	}
	const Matrix staged = Compose(Rotation(remapped), permutation);
	if (!Finite(staged)) return false;
	out = staged;
	return true;
}

/* `root` is the matrix the root bone is composed onto: RootBasis for a model
 * of its own, or the parent's bolt for a model attached to another. Retail
 * jamp.exe 1.01 stores either in the bone cache (0x498240) and its bone
 * transform composes root x local for bone 0 (0x497c02..0x497c10). */
inline bool Evaluate(const void *data, std::size_t bytes, const model::Skeleton &skeleton,
	const std::vector<Animation> &animations, const std::vector<Angle> &angles, Palette &out,
	const Matrix &root = RootBasis)
{
	if (!Finite(root)) return false;
	std::vector<int> parents;
	std::vector<Matrix> base, inverse;
	if (!Hierarchy(data, bytes, skeleton, parents, base, inverse)) return false;
	const auto validFrame = [&skeleton](const Frame &frame) {
		return frame.current >= 0 && frame.current < skeleton.numFrames
			&& frame.next >= 0 && frame.next < skeleton.numFrames && Finite(frame.lerp)
			&& frame.lerp >= 0 && frame.lerp <= 1;
	};
	std::vector<const Animation *> controls(skeleton.numBones, nullptr);
	for (const Animation &control : animations) {
		if (control.bone < 0 || control.bone >= skeleton.numBones || controls[control.bone]
			|| !validFrame(control.frame) || (control.blending && (!validFrame(control.previous)
				|| !Finite(control.transition) || control.transition < 0 || control.transition > 1))) return false;
		controls[control.bone] = &control;
	}
	std::vector<const Angle *> posts(skeleton.numBones, nullptr);
	for (const Angle &control : angles) {
		if (control.bone < 0 || control.bone >= skeleton.numBones || posts[control.bone]
			|| !Finite(control.post) || (control.mode != 1 && control.mode != 2 && control.mode != 4)) return false;
		posts[control.bone] = &control;
	}
	Palette staged;
	staged.deformation.resize(skeleton.numBones); staged.bones.resize(skeleton.numBones);
	std::vector<Animation> active(skeleton.numBones);
	std::vector<bool> done(skeleton.numBones, false);
	for (int pass = 0, remaining = skeleton.numBones; remaining && pass < skeleton.numBones; ++pass) {
		for (int bone = 0; bone < skeleton.numBones; ++bone) {
			const int parent = parents[bone];
			if (done[bone] || (parent >= 0 && !done[parent])) continue;
			active[bone] = controls[bone] ? *controls[bone] : parent >= 0 ? active[parent] : Animation{};
			const Frame &frame = active[bone].frame;
			model::CompressedPose current, next;
			Matrix local, later;
			if (!model::BonePose(data, bytes, skeleton, frame.current, bone, current)
				|| !model::BonePose(data, bytes, skeleton, frame.next, bone, next)) return false;
			model::PoseMatrix(current, local.data()); model::PoseMatrix(next, later.data());
			for (int i = 0; i < 12; ++i) local[i] = local[i]*(1-frame.lerp)+later[i]*frame.lerp;
			if (active[bone].blending) {
				const Frame &prior = active[bone].previous;
				model::CompressedPose oldCurrent, oldNext;
				Matrix earlier, following;
				if (!model::BonePose(data, bytes, skeleton, prior.current, bone, oldCurrent)
					|| !model::BonePose(data, bytes, skeleton, prior.next, bone, oldNext)) return false;
				model::PoseMatrix(oldCurrent, earlier.data()); model::PoseMatrix(oldNext, following.data());
				const float progress = active[bone].transition;
				for (int i = 0; i < 12; ++i) {
					const float old = earlier[i]*(1-prior.lerp)+following[i]*prior.lerp;
					local[i] = old*(1-progress)+local[i]*progress;
				}
			}
			if (!Finite(local)) return false;
			const Matrix &basis = parent < 0 ? root : staged.deformation[parent];
			staged.deformation[bone] = Compose(basis, local);
			if (posts[bone]) {
				const Angle &angle = *posts[bone];
				if (angle.mode == 1) staged.deformation[bone] = Compose(basis, angle.post);
				if (angle.mode == 2) staged.deformation[bone] = Compose(staged.deformation[bone], angle.post);
				if (angle.mode == 4) {
					const Matrix raw = Compose(staged.deformation[bone], base[bone]);
					const double x = raw[0], y = raw[1], z = raw[2];
					const float scale = float(std::sqrt(x*x+y*y+z*z));
					if (!Finite(scale) || !(scale > 1e-12f)) return false;
					Matrix replaced = angle.post;
					for (int row = 0; row < 3; ++row) {
						for (int column = 0; column < 3; ++column) replaced[row*4+column] *= scale;
						replaced[row*4+3] = raw[row*4+3];
					}
					staged.deformation[bone] = Compose(replaced, inverse[bone]);
				}
			}
			staged.bones[bone] = Compose(staged.deformation[bone], base[bone]);
			if (!Finite(staged.deformation[bone]) || !Finite(staged.bones[bone])) return false;
			done[bone] = true; --remaining;
		}
	}
	for (bool complete : done) if (!complete) return false;
	out = std::move(staged);
	return true;
}

namespace detail {
inline bool Place(const float raw[12], const float angles[3], const float position[3],
	const float scale[3], bool noRotation, bool normalizeRows, float out[12])
{
	if (!raw || !angles || !position || !out) return false;
	float suppliedAngles[3], suppliedPosition[3], suppliedScale[3]{};
	std::memcpy(suppliedAngles, angles, sizeof(suppliedAngles));
	std::memcpy(suppliedPosition, position, sizeof(suppliedPosition));
	if (scale) std::memcpy(suppliedScale, scale, sizeof(suppliedScale));
	Matrix bolt;
	std::memcpy(bolt.data(), raw, sizeof(bolt));
	if (!Finite(bolt)) return false;
	for (int i = 0; i < 3; ++i) {
		if (!Finite(suppliedAngles[i]) || !Finite(suppliedPosition[i]) || !Finite(suppliedScale[i])) return false;
		if (suppliedScale[i] != 0) bolt[i*4+3] *= suppliedScale[i];
		if (!normalizeRows) continue;
		const double a = bolt[i*4], b = bolt[i*4+1], c = bolt[i*4+2];
		const double length = std::sqrt(a*a+b*b+c*c);
		if (!(length > 1e-12)) return false;
		for (int n = 0; n < 3; ++n) bolt[i*4+n] = float(bolt[i*4+n]/length);
	}
	Matrix entity = Rotation(suppliedAngles);
	for (int i = 0; i < 3; ++i) entity[i*4+3] = suppliedPosition[i];
	Matrix staged = Compose(entity, bolt);
	if (!noRotation) {
		for (int row = 0; row < 3; ++row) {
			const float x = staged[row*4], y = staged[row*4+1];
			staged[row*4] = -y; staged[row*4+1] = x;
		}
	}
	if (!Finite(staged)) return false;
	std::memcpy(out, staged.data(), sizeof(staged));
	return true;
}
} // namespace detail

/* The MP bolt API scales translation in model space, normalizes the three
 * rotation rows, applies entity placement, then rotates the rotation-only
 * part by -90 degrees unless the NOREC_NOROT import was used. */
inline bool PlaceBolt(const float raw[12], const float angles[3], const float position[3],
	const float scale[3], bool noRotation, float out[12])
{
	return detail::Place(raw, angles, position, scale, noRotation, true, out);
}
/* Jedi Outcast's placement (jk2mp.exe 1.02a 0x4a76d0..0x4a7827): the same
 * translation scale, world matrix (0x4abdb0) and yaw-270 turn of the rotation
 * alone (0x4abcc0, translation restored after), but the kept matrix is used as
 * it is: no row is normalized. Outcast has no NOREC_NOROT import. */
inline bool PlaceOutcastBolt(const float raw[12], const float angles[3],
	const float position[3], const float scale[3], float out[12])
{
	return detail::Place(raw, angles, position, scale, false, false, out);
}
/* Retail0x4dfc1d..0x4dfc3a publishes world*fixed-MP-axes on failure.
 * This helper reports publication validity; the module query still returns
 * false. All publication is staged, including potentially unaligned output. */
inline bool FailureBolt(const float angles[3], const float position[3], float out[12]) {
    if (!angles || !position || !out) return false;
    float copiedAngles[3], copiedPosition[3];
    std::memcpy(copiedAngles, angles, sizeof(copiedAngles));
    std::memcpy(copiedPosition, position, sizeof(copiedPosition));
    for (int axis = 0; axis < 3; ++axis)
        if (!Finite(copiedAngles[axis]) || !Finite(copiedPosition[axis])) return false;
    Matrix entity = Rotation(copiedAngles);
    for (int axis = 0; axis < 3; ++axis) entity[axis*4+3] = copiedPosition[axis];
    const Matrix staged = Compose(entity, RootBasis);
    if (!Finite(staged)) return false;
    std::memcpy(out, staged.data(), sizeof(staged));
    return true;
}
} // namespace fntech3::ghoul2::pose
#endif
